//! Shared helpers for the tokio benchmark binaries.

use std::future::Future;
use std::pin::Pin;

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

/// A multi-threaded tokio runtime pinned to `threads` worker threads. Used by
/// the CPU-bound fork-join benchmarks and the channel benchmark. The IO/time
/// drivers are intentionally left disabled since none of those workloads need
/// them (this mirrors TMC's pure-compute `cpu_executor`).
pub fn multi_thread_runtime(threads: usize) -> tokio::runtime::Runtime {
    tokio::runtime::Builder::new_multi_thread()
        .worker_threads(threads)
        .build()
        .unwrap()
}
