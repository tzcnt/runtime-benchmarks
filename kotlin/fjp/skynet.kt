// The skynet benchmark as described here:
// https://github.com/atemerev/skynet
//
// Canonical (TooManyCooks) implementation:
// ../../cpp/TooManyCooks/skynet.cpp
//
// Each node forks 10 children until depthMax levels deep (10^depthMax = 100M
// leaf tasks) and sums their results. Here each child is a coroutine started via
// async {}, so this measures Kotlin's coroutine spawn/join throughput under deep
// nested fan-out. The scheduler under test is selected by benchutil.newDispatcher.
@file:JvmName("Main")

package skynet

import benchutil.newDispatcher
import benchutil.peakMemoryUsageKiB
import benchutil.threadCountArg
import kotlinx.coroutines.async
import kotlinx.coroutines.coroutineScope
import kotlinx.coroutines.runBlocking

private const val DEPTH_MAX = 8L
private const val ITER_COUNT = 1

private suspend fun skynetOne(base: Long, depth: Long): Long = coroutineScope {
    if (depth == DEPTH_MAX) {
        return@coroutineScope base
    }
    var offset = 1L
    repeat((DEPTH_MAX - depth - 1).toInt()) { offset *= 10 }

    // Each child computes an independent slot; coroutineScope's join gives the
    // happens-before needed to read them all back below.
    (0L until 10L)
        .map { i -> async { skynetOne(base + offset * i, depth + 1) } }
        .sumOf { it.await() }
}

private suspend fun skynet(expected: Long) {
    val count = skynetOne(0, 0)
    if (count != expected) {
        println("ERROR: wrong result - $count")
    }
}

fun main(args: Array<String>) {
    val threadCount = threadCountArg(args, 0)

    var leaves = 1L
    repeat(DEPTH_MAX.toInt()) { leaves *= 10 }
    val expected = (leaves - 1) * leaves / 2

    println("threads: $threadCount")

    runBlocking(newDispatcher(threadCount)) {
        skynet(expected) // warmup

        println("runs:")
        val start = System.nanoTime()
        repeat(ITER_COUNT) { skynet(expected) }
        val durUs = (System.nanoTime() - start) / 1000
        println("  - iteration_count: $ITER_COUNT")
        println("    duration: $durUs us")
        println("    max_rss: ${peakMemoryUsageKiB()} KiB")
    }
}
