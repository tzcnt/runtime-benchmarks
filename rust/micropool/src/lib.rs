//! Shared helpers for the micropool benchmark binaries.

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

/// A micropool thread pool with `threads` total workers, all created up front by
/// `build()` - no threads are ever spawned once the benchmark is running.
///
/// micropool is an external-thread-participates pool: the thread that calls
/// `join` (or drives a parallel iterator) does not block but actively helps
/// complete the work. Our benchmarks run the whole workload on the calling
/// thread inside `pool.install(...)`, so that thread is one of the `threads`
/// workers. We therefore build the pool with `threads - 1` background workers;
/// together with the calling thread that yields `threads` workers overall -
/// matching the C++/rayon/chili convention. (`threads == 1` builds zero
/// background workers, so the whole workload runs sequentially on the caller.)
pub fn build_pool(threads: usize) -> micropool::ThreadPool {
    micropool::ThreadPoolBuilder::default()
        .num_threads(threads.saturating_sub(1))
        .build()
}
