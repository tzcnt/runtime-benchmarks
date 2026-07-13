//! Shared helpers for the beekeeper benchmark binaries.
//!
//! beekeeper is a worker-pool library: a `Hive` of identical `Worker`s pulls
//! inputs from a task queue and delivers each task's result as an `Outcome`.
//! Two properties of that model shape every benchmark in this crate:
//!
//! * A task may fork subtasks from inside a worker (`Context::submit`), but it
//!   cannot *join* them - a subtask's result goes to the same outcome sink as
//!   its parent, not back to the parent. The recursive fork-join benchmarks
//!   therefore return their partial results (leaf values) through the hive's
//!   outcome channel, where drainer threads (below) sum them.
//! * Every task produces an `Outcome` that must be delivered somewhere.
//!   There is no fire-and-forget mode: an outcome that is not sent to a
//!   channel (or whose send fails) is stored in the hive's internal map, which
//!   would accumulate hundreds of millions of entries in these benchmarks. So
//!   each run creates an outcome channel and drains it concurrently.
//!
//! The hive's work-stealing queues are FIFO (crossbeam-deque `new_fifo`), so
//! recursive workloads expand breadth-first and the task frontier is
//! materialized in memory. That is inherent to the library (there is no LIFO
//! option) and is visible in these benchmarks' max_rss.

use beekeeper::bee::{DefaultQueen, Worker};
use beekeeper::hive::{
    Builder, Hive, OpenBuilder, Outcome, OutcomeReceiver, WorkstealingTaskQueues,
};
use std::thread::JoinHandle;

/// Peak resident set size of the current process, in KiB. Mirrors
/// `cpp/2common/memusage.hpp` so the reported numbers are comparable.
pub fn peak_memory_usage() -> i64 {
    unsafe {
        let mut usage: libc::rusage = std::mem::zeroed();
        if libc::getrusage(libc::RUSAGE_SELF, &mut usage) == 0 {
            // On Linux ru_maxrss is already in KiB; on macOS/BSD it is in bytes.
            #[cfg(target_os = "macos")]
            {
                return (usage.ru_maxrss as i64) / 1024;
            }
            #[cfg(not(target_os = "macos"))]
            {
                return usage.ru_maxrss as i64;
            }
        }
    }
    -1
}

/// Default worker-thread count when none is supplied on the command line.
/// Matches the C++ default of `hardware_concurrency() / 2`.
pub fn default_threads() -> usize {
    let hc = std::thread::available_parallelism()
        .map(|n| n.get())
        .unwrap_or(8);
    (hc / 2).max(1)
}

/// The hive type used by all benchmarks: default-constructed workers on the
/// work-stealing task queues. The alternative channel-based queues funnel all
/// forked subtasks through one global channel, which is strictly slower for
/// these workloads.
pub type WorkstealingHive<W> = Hive<DefaultQueen<W>, WorkstealingTaskQueues<W>>;

/// A hive with `threads` worker threads, analogous to how the other runtimes
/// build a fixed-size pool/runtime per invocation.
pub fn build_hive<W: Worker + Default + Send + Sync>(threads: usize) -> WorkstealingHive<W> {
    OpenBuilder::empty()
        .num_threads(threads)
        .with_worker_default::<W>()
        .with_workstealing_queues()
        .build()
}

/// Number of threads draining the outcome channel. One drainer keeps up with a
/// handful of workers (it only matches an enum and adds); scale up a little at
/// high worker counts so the (unbounded) outcome channel cannot grow without
/// limit when many workers produce outcomes faster than one thread can receive
/// them. These threads are pure overhead of beekeeper's delivery model and are
/// intentionally not counted against the benchmark's worker thread budget.
pub fn drainer_count(threads: usize) -> usize {
    (threads / 8).clamp(1, 4)
}

/// Spawns `count` threads that drain `rx` until every clone of the sender has
/// been dropped. Each queued task holds a clone of the outcome sender, so once
/// the caller drops its own sender, channel disconnect means every task
/// (including all recursively forked subtasks) has finished and delivered its
/// outcome. Returns the per-thread (count, sum) of successful outcome values;
/// any failed/unprocessed outcome aborts the benchmark.
pub fn spawn_outcome_drainers<W>(
    rx: &OutcomeReceiver<W>,
    count: usize,
) -> Vec<JoinHandle<(u64, u64)>>
where
    W: Worker<Output = u64>,
{
    (0..count)
        .map(|_| {
            let rx = rx.clone();
            std::thread::spawn(move || {
                let mut count = 0u64;
                let mut sum = 0u64;
                for outcome in rx {
                    match outcome {
                        Outcome::Success { value, .. }
                        | Outcome::SuccessWithSubtasks { value, .. } => {
                            count += 1;
                            sum += value;
                        }
                        other => {
                            eprintln!("FAIL: unexpected outcome: {:?}", other);
                            std::process::exit(1);
                        }
                    }
                }
                (count, sum)
            })
        })
        .collect()
}

/// Joins the drainer threads and returns the combined (count, sum).
pub fn join_outcome_drainers(handles: Vec<JoinHandle<(u64, u64)>>) -> (u64, u64) {
    handles
        .into_iter()
        .map(|h| h.join().expect("outcome drainer panicked"))
        .fold((0, 0), |(c, s), (hc, hs)| (c + hc, s + hs))
}
