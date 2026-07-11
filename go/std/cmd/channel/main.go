// Test performance of an MPMC channel primitive.
//
// Canonical (TooManyCooks) implementation:
// ../../../../cpp/TooManyCooks/channel.cpp
//
// N producers each post a slice of elementCount items; M consumers pull until
// the channel is drained; the total count and sum are validated.
//
// The Go stdlib's native MPMC primitive is the channel, so this benchmark uses
// a single `chan uint64` shared by all producers and consumers. Unlike the
// unbounded queues the other runtimes use (TMC's channel, async-channel/flume),
// Go channels are always bounded; we give it a generous buffer so the benchmark
// measures steady-state throughput rather than constant goroutine parking.
package main

import (
	"fmt"
	"sync"
	"time"

	"gostdbench/internal/benchutil"
)

const elementCount = 10_000_000
const iterCount = 1
const channelBuffer = 1 << 16

type counts struct {
	count uint64
	sum   uint64
}

type assignment struct {
	count uint64
	base  uint64
}

// producerAssignments splits elementCount into per-producer (count, base) work.
func producerAssignments(producerCount int) []assignment {
	per := uint64(elementCount) / uint64(producerCount)
	rem := uint64(elementCount) % uint64(producerCount)
	base := uint64(0)
	out := make([]assignment, 0, producerCount)
	for i := 0; i < producerCount; i++ {
		count := per
		if uint64(i) < rem {
			count++
		}
		out = append(out, assignment{count: count, base: base})
		base += count
	}
	return out
}

func doBench(producerCount, consumerCount int) uint64 {
	ch := make(chan uint64, channelBuffer)

	var producers sync.WaitGroup
	for _, a := range producerAssignments(producerCount) {
		producers.Go(func() {
			for i := uint64(0); i < a.count; i++ {
				ch <- a.base + i
			}
		})
	}

	results := make([]counts, consumerCount)
	var consumers sync.WaitGroup
	for idx := 0; idx < consumerCount; idx++ {
		consumers.Go(func() {
			var count, sum uint64
			for v := range ch {
				count++
				sum += v
			}
			results[idx] = counts{count: count, sum: sum}
		})
	}

	// Once every producer has finished, close the channel so the consumers'
	// range loops terminate after draining what remains.
	producers.Wait()
	close(ch)
	consumers.Wait()

	total := counts{}
	for _, r := range results {
		total.count += r.count
		total.sum += r.sum
	}

	expectedSum := uint64(elementCount) * (uint64(elementCount) - 1) / 2
	if total.count != elementCount {
		fmt.Printf(
			"FAIL: Expected %d elements but consumed %d elements\n",
			uint64(elementCount), total.count,
		)
	}
	if total.sum != expectedSum {
		fmt.Printf("FAIL: Expected %d sum but got %d sum\n", expectedSum, total.sum)
	}
	return total.sum
}

func main() {
	threadCount := benchutil.ThreadCountArg(1)

	per := threadCount / 2
	if per < 1 {
		per = 1
	}
	producerCount := per
	consumerCount := per

	fmt.Printf("threads: %d\n", threadCount)
	fmt.Printf("producers: %d\n", producerCount)
	fmt.Printf("consumers: %d\n", consumerCount)

	result := doBench(producerCount, consumerCount) // warmup
	fmt.Printf("output: %d\n", result)

	start := time.Now()
	for i := 0; i < iterCount; i++ {
		doBench(producerCount, consumerCount)
	}
	durUs := time.Since(start).Microseconds()
	if durUs < 1 {
		durUs = 1
	}
	elementsPerSec := int64(elementCount) * 1_000_000 / durUs

	fmt.Println("runs:")
	fmt.Printf("  - iteration_count: %d\n", iterCount)
	fmt.Printf("    elements: %d\n", elementCount)
	fmt.Printf("    duration: %d us\n", durUs)
	fmt.Printf("    elements/sec: %d\n", elementsPerSec)
	fmt.Printf("    max_rss: %d KiB\n", benchutil.PeakMemoryUsageKiB())
}
