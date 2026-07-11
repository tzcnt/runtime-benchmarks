// Recursive fork-join nqueens.
//
// Canonical (TooManyCooks) implementation:
// ../../../../cpp/TooManyCooks/nqueens.cpp
// which was in turn adapted from libfork's benchmark.
//
// At each column, fork a child task for every valid queen placement. Each fork
// is a goroutine started via sync.WaitGroup.Go.
package main

import (
	"fmt"
	"sync"
	"time"

	"gostdbench/internal/benchutil"
)

const nqueensWork = 14 // board size
const iterCount = 1

// answers[k] = number of solutions to the k-queens problem.
var answers = [19]int{
	0, 1, 0, 0, 2, 10, 4, 40, 92, 352, 724, 2680, 14200, 73712, 365596, 2279184,
	14772512, 95815104, 666090624,
}

func checkAnswer(result int) {
	if result != answers[nqueensWork] {
		fmt.Printf("error: expected %d, got %d\n", answers[nqueensWork], result)
	}
}

func nqueens(xMax int, buf [nqueensWork]int8) int {
	if nqueensWork == xMax {
		return 1
	}

	// Fork one child per valid placement into its own result slot; taskCount
	// tracks how many of the (up to nqueensWork) slots were actually used.
	var results [nqueensWork]int
	var wg sync.WaitGroup
	taskCount := 0
	for y := 0; y < nqueensWork; y++ {
		q := y
		valid := true
		for x := 0; x < xMax; x++ {
			p := int(buf[x])
			d := xMax - x
			if q == p || q == p-d || q == p+d {
				valid = false
				break
			}
		}
		if !valid {
			continue
		}
		next := buf
		next[xMax] = int8(y)
		idx := taskCount
		wg.Go(func() {
			results[idx] = nqueens(xMax+1, next)
		})
		taskCount++
	}
	wg.Wait()

	ret := 0
	for i := 0; i < taskCount; i++ {
		ret += results[i]
	}
	return ret
}

func main() {
	threadCount := benchutil.ThreadCountArg(1)
	fmt.Printf("threads: %d\n", threadCount)

	{
		var buf [nqueensWork]int8
		checkAnswer(nqueens(0, buf)) // warmup
	}

	start := time.Now()
	result := 0
	for i := 0; i < iterCount; i++ {
		var buf [nqueensWork]int8
		result = nqueens(0, buf)
		checkAnswer(result)
	}
	durUs := time.Since(start).Microseconds()
	fmt.Printf("output: %d\n", result)

	fmt.Println("runs:")
	fmt.Printf("  - iteration_count: %d\n", iterCount)
	fmt.Printf("    duration: %d us\n", durUs)
	fmt.Printf("    max_rss: %d KiB\n", benchutil.PeakMemoryUsageKiB())
}
