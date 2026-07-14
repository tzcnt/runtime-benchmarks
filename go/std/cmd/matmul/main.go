// Recursive divide-and-conquer matrix multiplication.
//
// Canonical (TooManyCooks) implementation:
// ../../../../cpp/TooManyCooks/matmul.cpp
// base case: ../../../../cpp/2common/matmul.hpp
//
// The matrices are split into quadrants recursively down to a 32x32 base case.
// The 8 sub-multiplications are run as two sequential groups of 4 parallel
// goroutines (started via sync.WaitGroup.Go) so that no output tile is written
// by two tasks at once.
//
// A, B and C share three flat backing slices; each `mat` value carries base
// offsets (ao/bo/co) and the full-matrix stride (nn) into them. The parallel
// tasks in a group write disjoint sub-tiles of C, so no locking is required.
package main

import (
	"fmt"
	"os"
	"strconv"
	"sync"
	"time"

	"gostdbench/internal/benchutil"
)

type mat struct {
	a, b, c    []int32
	ao, bo, co int // base offsets into a/b/c
	n, nn      int // submatrix size, and stride (the full matrix dimension N)
}

func matmulSmall(m mat) {
	for i := 0; i < m.n; i++ {
		for k := 0; k < m.n; k++ {
			for j := 0; j < m.n; j++ {
				m.c[m.co+i*m.nn+j] += m.a[m.ao+i*m.nn+k] * m.b[m.bo+k*m.nn+j]
			}
		}
	}
}

func matmul(m mat) {
	if m.n <= 32 {
		matmulSmall(m)
		return
	}
	k := m.n / 2
	nn := m.nn
	a, b, c := m.a, m.b, m.c
	ao, bo, co := m.ao, m.bo, m.co

	// Group 1: the four products that only touch the "upper-left" halves.
	runGroup([4]mat{
		{a, b, c, ao, bo, co, k, nn},
		{a, b, c, ao, bo + k, co + k, k, nn},
		{a, b, c, ao + k*nn, bo, co + k*nn, k, nn},
		{a, b, c, ao + k*nn, bo + k, co + k*nn + k, k, nn},
	})
	// Group 2: the accumulating products into the same tiles.
	runGroup([4]mat{
		{a, b, c, ao + k, bo + k*nn, co, k, nn},
		{a, b, c, ao + k, bo + k*nn + k, co + k, k, nn},
		{a, b, c, ao + k*nn + k, bo + k*nn, co + k*nn, k, nn},
		{a, b, c, ao + k*nn + k, bo + k*nn + k, co + k*nn + k, k, nn},
	})
}

func runGroup(group [4]mat) {
	var wg sync.WaitGroup
	for _, m := range group {
		wg.Go(func() {
			matmul(m)
		})
	}
	wg.Wait()
}

func runMatmul(n int) []int32 {
	a := make([]int32, n*n)
	b := make([]int32, n*n)
	c := make([]int32, n*n)
	for i := range a {
		a[i] = 1
		b[i] = 1
	}
	matmul(mat{a: a, b: b, c: c, n: n, nn: n})
	return c
}

func validate(c []int32, n int) {
	for i := 0; i < n; i++ {
		for j := 0; j < n; j++ {
			if res := c[i*n+j]; res != int32(n) {
				fmt.Printf("Wrong result at (%d,%d) : %d. expected %d\n", i, j, res, n)
				os.Exit(1)
			}
		}
	}
}

func main() {
	if len(os.Args) < 2 {
		fmt.Println("Usage: matmul <matrix size (power of 2)>")
		return
	}
	n, err := strconv.Atoi(os.Args[1])
	if err != nil {
		fmt.Println("Usage: matmul <matrix size (power of 2)>")
		return
	}
	threadCount := benchutil.ThreadCountArg(2)
	fmt.Printf("threads: %d\n", threadCount)

	_ = runMatmul(n) // warmup

	fmt.Println("runs:")
	start := time.Now()
	result := runMatmul(n)
	durUs := time.Since(start).Microseconds()
	validate(result, n)

	fmt.Printf("  - matrix_size: %d\n", n)
	fmt.Printf("    duration: %d us\n", durUs)
	fmt.Printf("    max_rss: %d KiB\n", benchutil.PeakMemoryUsageKiB())
}
