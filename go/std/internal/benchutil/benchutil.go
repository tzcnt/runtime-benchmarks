// Package benchutil holds helpers shared across the Go stdlib benchmark
// binaries so their setup and reported numbers line up with the C++ (TMC) and
// Rust (tokio) suites.
package benchutil

import (
	"os"
	"runtime"
	"strconv"
	"syscall"
)

// PeakMemoryUsageKiB returns the peak resident set size of the current process
// in KiB, mirroring cpp/2common/memusage.hpp so the reported numbers are
// directly comparable across languages.
func PeakMemoryUsageKiB() int64 {
	var ru syscall.Rusage
	if err := syscall.Getrusage(syscall.RUSAGE_SELF, &ru); err != nil {
		return -1
	}
	// On Linux ru_maxrss is already in KiB; on macOS/BSD it is in bytes.
	if runtime.GOOS == "darwin" {
		return int64(ru.Maxrss) / 1024
	}
	return int64(ru.Maxrss)
}

// DefaultThreads mirrors the C++/Rust default worker count of
// hardware_concurrency() / 2.
func DefaultThreads() int {
	n := runtime.NumCPU() / 2
	if n < 1 {
		n = 1
	}
	return n
}

// ThreadCountArg parses os.Args[idx] as a worker-thread count, falling back to
// DefaultThreads() when it is absent or unparseable. The value is applied to
// GOMAXPROCS so the Go scheduler is pinned to the requested parallelism,
// matching how the other suites configure their executor thread counts. The
// benchmark harness passes the thread count in this position (see
// build_and_bench_all.py).
func ThreadCountArg(idx int) int {
	n := DefaultThreads()
	if len(os.Args) > idx {
		if v, err := strconv.Atoi(os.Args[idx]); err == nil && v > 0 {
			n = v
		}
	}
	runtime.GOMAXPROCS(n)
	return n
}
