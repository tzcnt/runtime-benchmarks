//! Shared helpers for the forte benchmark binaries.

use forte::ThreadPool;

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

/// The single, process-wide forte thread pool. forte's `ThreadPool` methods
/// require `&'static self`, so the pool must be a `static`; `ThreadPool::new()`
/// is a const constructor for exactly this purpose. Each benchmark sizes it once
/// via `size_pool` and then runs its whole workload inside `POOL.with_worker`.
pub static POOL: ThreadPool = ThreadPool::new();

/// Size the global pool so that `threads` workers participate in total, and
/// return that total.
///
/// forte spawns `resize_to(n)` *managed* background workers. The thread that
/// calls `POOL.with_worker` (our main thread) additionally joins the pool as a
/// worker and actively runs the root of the join tree - so it is a full worker,
/// not a blocked coordinator. `threads - 1` managed workers plus the calling
/// thread therefore give `threads` active workers overall, matching the
/// rayon/chili convention where `threads` is the total worker count.
///
/// `threads == 1` resizes to 0 managed workers, so the whole benchmark runs
/// serially on the calling thread (no heartbeat sharing ever occurs).
pub fn size_pool(threads: usize) -> usize {
    // forte spawns its workers as std threads without an explicit stack size, so
    // they default to 2 MiB. Its `join` runs stolen jobs inline on a *blocked*
    // worker's stack, which at low worker counts drives the (already deep) nqueens
    // recursion past 2 MiB and aborts with a stack overflow - even though chili,
    // with the same recursion, fits in 2 MiB. Growth is bounded (8 MiB already
    // suffices for N=14), so give the workers a generous 64 MiB stack via
    // RUST_MIN_STACK, which std reads once when the first worker spawns just below.
    // Only reserved pages that are actually touched become resident, so this does
    // not inflate max_rss. An explicit RUST_MIN_STACK from the environment wins.
    // This mirrors the per-runtime stack tuning other runtimes here need.
    if std::env::var_os("RUST_MIN_STACK").is_none() {
        std::env::set_var("RUST_MIN_STACK", "67108864"); // 64 MiB
    }
    let managed = threads.max(1) - 1;
    POOL.resize_to(managed);
    managed + 1
}
