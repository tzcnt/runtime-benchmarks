//! Shared helpers for the glommio benchmark binaries.

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
