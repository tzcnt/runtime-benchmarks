//! Shared helpers for the rayon benchmark binaries.

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

/// A rayon thread pool with `threads` worker threads. The fork-join benchmarks
/// run their whole workload inside `pool.install(...)` so that every nested
/// `join`, `scope`, and parallel iterator uses this pool rather than rayon's
/// implicit global pool (whose size we could not otherwise control per run).
/// This mirrors how the tokio benchmarks build a fresh runtime per invocation.
pub fn build_pool(threads: usize) -> rayon::ThreadPool {
    rayon::ThreadPoolBuilder::new()
        .num_threads(threads)
        .build()
        .unwrap()
}
