//! Shared helpers for the bevy_tasks benchmark binaries.

use std::future::Future;
use std::pin::Pin;
use std::sync::OnceLock;

use bevy_tasks::{TaskPool, TaskPoolBuilder};

/// A heap-allocated, `Send` future. The recursive fork-join benchmarks return
/// this from their recursive helpers because an `async fn` that awaits itself
/// would otherwise be infinitely sized.
pub type BoxFut<T> = Pin<Box<dyn Future<Output = T> + Send>>;

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

static POOL: OnceLock<TaskPool> = OnceLock::new();

/// A bevy_tasks `TaskPool` pinned to `threads` worker threads. `TaskPool::spawn`
/// requires `'static` futures, so the recursive benchmarks capture a `&'static`
/// pool reference in their tasks; the pool therefore lives in a process-wide
/// static (this mirrors how bevy itself exposes `ComputeTaskPool::get()`).
/// Nested fork-join works because a task that awaits a child `Task` suspends,
/// freeing its worker thread to execute other tasks from the pool's executor.
pub fn init_pool(threads: usize) -> &'static TaskPool {
    POOL.get_or_init(|| TaskPoolBuilder::new().num_threads(threads).build())
}
