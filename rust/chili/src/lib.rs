//! Shared helpers for the chili benchmark binaries.

use std::num::NonZero;

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

/// A chili thread pool with `threads` total workers. chili counts the calling
/// thread (the one that owns the `Scope`) as one worker and spawns
/// `thread_count - 1` background workers, so passing `threads` here yields
/// `threads` workers overall - matching the C++/rayon convention. The heartbeat
/// interval is left at chili's default (100us). Each benchmark creates one
/// `Scope` from this pool and threads it through the whole recursive workload,
/// analogous to how the rayon benchmarks run inside `pool.install`.
pub fn build_pool(threads: usize) -> chili::ThreadPool {
    let nz = NonZero::new(threads.max(1)).unwrap();
    chili::ThreadPool::with_config(chili::Config {
        thread_count: Some(nz),
        ..Default::default()
    })
}
