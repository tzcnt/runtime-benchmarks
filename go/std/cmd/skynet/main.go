// The skynet benchmark as described here:
// https://github.com/atemerev/skynet
//
// Canonical (TooManyCooks) implementation:
// ../../../../cpp/TooManyCooks/skynet.cpp
//
// Each node forks 10 children until depthMax levels deep (10^depthMax = 100M
// leaf tasks) and sums their results. Here each child is a goroutine started via
// sync.WaitGroup.Go, so this measures Go's goroutine spawn/join throughput under
// deep nested fan-out.
package main

import (
	"fmt"
	"sync"
	"time"

	"gostdbench/internal/benchutil"
)

const depthMax = 8
const iterCount = 1

func skynetOne(base, depth uint64) uint64 {
	if depth == depthMax {
		return base
	}
	offset := uint64(1)
	for i := uint64(0); i < depthMax-depth-1; i++ {
		offset *= 10
	}

	// Each child writes its own slot, so the disjoint writes need no
	// synchronization beyond the WaitGroup's happens-before on Wait().
	var results [10]uint64
	var wg sync.WaitGroup
	for i := uint64(0); i < 10; i++ {
		wg.Go(func() {
			results[i] = skynetOne(base+offset*i, depth+1)
		})
	}
	wg.Wait()

	var count uint64
	for _, r := range results {
		count += r
	}
	return count
}

func skynet(expected uint64) {
	count := skynetOne(0, 0)
	if count != expected {
		fmt.Printf("ERROR: wrong result - %d\n", count)
	}
}

func main() {
	threadCount := benchutil.ThreadCountArg(1)

	leaves := uint64(1)
	for i := 0; i < depthMax; i++ {
		leaves *= 10
	}
	expected := (leaves - 1) * leaves / 2

	fmt.Printf("threads: %d\n", threadCount)

	skynet(expected) // warmup

	fmt.Println("runs:")
	start := time.Now()
	for i := 0; i < iterCount; i++ {
		skynet(expected)
	}
	durUs := time.Since(start).Microseconds()
	fmt.Printf("  - iteration_count: %d\n", iterCount)
	fmt.Printf("    duration: %d us\n", durUs)
	fmt.Printf("    max_rss: %d KiB\n", benchutil.PeakMemoryUsageKiB())
}
