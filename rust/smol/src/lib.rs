//! Shared helpers for the smol benchmark binaries.

use std::future::Future;
use std::pin::Pin;
use std::sync::OnceLock;

use smol::future;
use smol::Executor;

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

static EXECUTOR: OnceLock<Executor<'static>> = OnceLock::new();

/// A smol `Executor` driven by exactly `threads` worker threads. smol's global
/// executor sizes itself from the SMOL_THREADS env var; building our own pool
/// instead makes the thread count an explicit function argument like the other
/// runtimes. Each worker parks in `ex.run(pending())`, processing tasks
/// forever. `Executor::spawn` requires `'static` futures, so the recursive
/// benchmarks capture a `&'static` executor reference in their tasks; the
/// executor therefore lives in a process-wide static.
///
/// The main thread does not run the executor - it only blocks on the root
/// `Task` handle - so exactly `threads` threads execute tasks, matching the
/// other runtimes. The workers use futures-lite's `block_on` (not async-io's
/// reactor-driving one) since these workloads are pure compute; this mirrors
/// tokio's benchmark runtime, which disables its IO driver.
pub fn init_executor(threads: usize) -> &'static Executor<'static> {
    let ex = EXECUTOR.get_or_init(Executor::new);
    for _ in 0..threads {
        std::thread::spawn(move || {
            future::block_on(ex.run(future::pending::<()>()));
        });
    }
    ex
}
