// An implementation of the recursive fork fibonacci parallelism test.
//
// Canonical (TooManyCooks) implementation:
// ../../cpp/TooManyCooks/fib.cpp
//
// Fork a coroutine that runs fib(n-1) in parallel with the current task, then
// continue the other leg (fib(n-2)) serially. Each fork is a coroutine started
// via async {}, so this measures Kotlin's coroutine spawn/join throughput. The
// scheduler under test is selected by benchutil.newDispatcher.
@file:JvmName("Main")

package fib

import benchutil.newDispatcher
import benchutil.peakMemoryUsageKiB
import benchutil.threadCountArg
import kotlinx.coroutines.async
import kotlinx.coroutines.coroutineScope
import kotlinx.coroutines.runBlocking

private const val ITER_COUNT = 1

private suspend fun fib(n: Long): Long = coroutineScope {
    if (n < 2) {
        return@coroutineScope n
    }
    val x = async { fib(n - 1) }
    val y = fib(n - 2)
    x.await() + y
}

fun main(args: Array<String>) {
    if (args.isEmpty()) {
        println("Usage: fib <n-th fibonacci number requested>")
        return
    }
    val n = args[0].toLongOrNull()
    if (n == null) {
        println("Usage: fib <n-th fibonacci number requested>")
        return
    }
    val threadCount = threadCountArg(args, 1)
    println("threads: $threadCount")

    runBlocking(newDispatcher(threadCount)) {
        fib(30) // warmup

        val start = System.nanoTime()
        var result = 0L
        repeat(ITER_COUNT) { result = fib(n) }
        val durUs = (System.nanoTime() - start) / 1000
        println("output: $result")

        println("runs:")
        println("  - iteration_count: $ITER_COUNT")
        println("    duration: $durUs us")
        println("    max_rss: ${peakMemoryUsageKiB()} KiB")
    }
}
