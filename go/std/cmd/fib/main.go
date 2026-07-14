// An implementation of the recursive fork fibonacci parallelism test.
//
// Canonical (TooManyCooks) implementation:
// ../../../../cpp/TooManyCooks/fib.cpp
//
// Fork a task that runs fib(n-1) in parallel with the current task, then
// continue the other leg (fib(n-2)) serially. Each fork is a goroutine started
// via sync.WaitGroup.Go, so this measures Go's goroutine spawn/join throughput.
package main

import (
	"fmt"
	"os"
	"strconv"
	"sync"
	"time"

	"gostdbench/internal/benchutil"
)

const iterCount = 1

func fib(n uint64) uint64 {
	if n < 2 {
		return n
	}
	var x uint64
	var wg sync.WaitGroup
	wg.Go(func() {
		x = fib(n - 1)
	})
	y := fib(n - 2)
	wg.Wait()
	return x + y
}

func main() {
	if len(os.Args) < 2 {
		fmt.Println("Usage: fib <n-th fibonacci number requested>")
		return
	}
	n, err := strconv.ParseUint(os.Args[1], 10, 64)
	if err != nil {
		fmt.Println("Usage: fib <n-th fibonacci number requested>")
		return
	}
	threadCount := benchutil.ThreadCountArg(2)
	fmt.Printf("threads: %d\n", threadCount)

	_ = fib(30) // warmup

	start := time.Now()
	var result uint64
	for i := 0; i < iterCount; i++ {
		result = fib(n)
	}
	durUs := time.Since(start).Microseconds()
	fmt.Printf("output: %d\n", result)

	fmt.Println("runs:")
	fmt.Printf("  - iteration_count: %d\n", iterCount)
	fmt.Printf("    duration: %d us\n", durUs)
	fmt.Printf("    max_rss: %d KiB\n", benchutil.PeakMemoryUsageKiB())
}
