// Recursive divide-and-conquer matrix multiplication.
//
// Canonical (TooManyCooks) implementation:
// ../../cpp/TooManyCooks/matmul.cpp
// base case: ../../cpp/2common/matmul.hpp
//
// The matrices are split into quadrants recursively down to a 32x32 base case.
// The 8 sub-multiplications are run as two sequential groups of 4 parallel
// coroutines (started via launch {}) so that no output tile is written by two
// tasks at once.
//
// A, B and C share three flat IntArrays; each Mat value carries base offsets
// (ao/bo/co) and the full-matrix stride (nn) into them. The parallel tasks in a
// group write disjoint sub-tiles of C, so no locking is required. The scheduler
// under test is selected by benchutil.newDispatcher.
@file:JvmName("Main")

package matmul

import benchutil.newDispatcher
import benchutil.peakMemoryUsageKiB
import benchutil.threadCountArg
import kotlinx.coroutines.coroutineScope
import kotlinx.coroutines.launch
import kotlinx.coroutines.runBlocking
import kotlin.system.exitProcess

private class Mat(
    val a: IntArray, val b: IntArray, val c: IntArray,
    val ao: Int, val bo: Int, val co: Int, // base offsets into a/b/c
    val n: Int, val nn: Int,                // submatrix size, and stride (full N)
)

private fun matmulSmall(m: Mat) {
    for (i in 0 until m.n) {
        for (k in 0 until m.n) {
            for (j in 0 until m.n) {
                m.c[m.co + i * m.nn + j] += m.a[m.ao + i * m.nn + k] * m.b[m.bo + k * m.nn + j]
            }
        }
    }
}

private suspend fun matmul(m: Mat) {
    if (m.n <= 32) {
        matmulSmall(m)
        return
    }
    val k = m.n / 2
    val nn = m.nn
    val a = m.a; val b = m.b; val c = m.c
    val ao = m.ao; val bo = m.bo; val co = m.co

    // Group 1: the four products that only touch the "upper-left" halves.
    runGroup(
        Mat(a, b, c, ao, bo, co, k, nn),
        Mat(a, b, c, ao, bo + k, co + k, k, nn),
        Mat(a, b, c, ao + k * nn, bo, co + k * nn, k, nn),
        Mat(a, b, c, ao + k * nn, bo + k, co + k * nn + k, k, nn),
    )
    // Group 2: the accumulating products into the same tiles.
    runGroup(
        Mat(a, b, c, ao + k, bo + k * nn, co, k, nn),
        Mat(a, b, c, ao + k, bo + k * nn + k, co + k, k, nn),
        Mat(a, b, c, ao + k * nn + k, bo + k * nn, co + k * nn, k, nn),
        Mat(a, b, c, ao + k * nn + k, bo + k * nn + k, co + k * nn + k, k, nn),
    )
}

private suspend fun runGroup(vararg group: Mat) = coroutineScope {
    for (m in group) {
        launch { matmul(m) }
    }
}

private suspend fun runMatmul(n: Int): IntArray = coroutineScope {
    val a = IntArray(n * n) { 1 }
    val b = IntArray(n * n) { 1 }
    val c = IntArray(n * n)
    matmul(Mat(a, b, c, 0, 0, 0, n, n))
    c
}

private fun validate(c: IntArray, n: Int) {
    for (i in 0 until n) {
        for (j in 0 until n) {
            val res = c[i * n + j]
            if (res != n) {
                println("Wrong result at ($i,$j) : $res. expected $n")
                exitProcess(1)
            }
        }
    }
}

fun main(args: Array<String>) {
    if (args.isEmpty()) {
        println("Usage: matmul <matrix size (power of 2)>")
        return
    }
    val n = args[0].toIntOrNull()
    if (n == null) {
        println("Usage: matmul <matrix size (power of 2)>")
        return
    }
    val threadCount = threadCountArg(args, 1)
    println("threads: $threadCount")

    runBlocking(newDispatcher(threadCount)) {
        runMatmul(n) // warmup

        println("runs:")
        val start = System.nanoTime()
        val result = runMatmul(n)
        val durUs = (System.nanoTime() - start) / 1000
        validate(result, n)

        println("  - matrix_size: $n")
        println("    duration: $durUs us")
        println("    max_rss: ${peakMemoryUsageKiB()} KiB")
    }
}
