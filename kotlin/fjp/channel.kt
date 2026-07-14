// Test performance of an MPMC channel primitive.
//
// Canonical (TooManyCooks) implementation:
// ../../cpp/TooManyCooks/channel.cpp
//
// N producers each post a slice of elementCount items; M consumers pull until
// the channel is drained; the total count and sum are validated.
//
// The Kotlin coroutines library's native MPMC primitive is kotlinx's Channel, so
// this benchmark uses a single Channel<Long> shared by all producers and
// consumers. Like the Go suite (and unlike TMC's unbounded queue), it is given a
// generous fixed buffer so the benchmark measures steady-state throughput rather
// than constant coroutine parking. The scheduler under test is selected by
// benchutil.newDispatcher.
@file:JvmName("Main")

package channel

import benchutil.newDispatcher
import benchutil.peakMemoryUsageKiB
import benchutil.threadCountArg
import kotlinx.coroutines.channels.Channel
import kotlinx.coroutines.coroutineScope
import kotlinx.coroutines.joinAll
import kotlinx.coroutines.async
import kotlinx.coroutines.launch
import kotlinx.coroutines.runBlocking

private const val ELEMENT_COUNT = 10_000_000L
private const val ITER_COUNT = 1
private const val CHANNEL_BUFFER = 1 shl 16

private data class Counts(val count: Long, val sum: Long)
private data class Assignment(val count: Long, val base: Long)

// Split ELEMENT_COUNT into per-producer (count, base) work.
private fun producerAssignments(producerCount: Int): List<Assignment> {
    val per = ELEMENT_COUNT / producerCount
    val rem = ELEMENT_COUNT % producerCount
    var base = 0L
    return (0 until producerCount).map { i ->
        val count = if (i.toLong() < rem) per + 1 else per
        Assignment(count, base).also { base += count }
    }
}

private suspend fun doBench(producerCount: Int, consumerCount: Int): Long = coroutineScope {
    val ch = Channel<Long>(CHANNEL_BUFFER)

    val producers = producerAssignments(producerCount).map { a ->
        launch {
            val end = a.base + a.count
            var v = a.base
            while (v < end) {
                ch.send(v)
                v++
            }
        }
    }

    val consumers = (0 until consumerCount).map {
        async {
            var count = 0L
            var sum = 0L
            for (v in ch) {
                count++
                sum += v
            }
            Counts(count, sum)
        }
    }

    // Once every producer has finished, close the channel so the consumers'
    // for-loops terminate after draining what remains.
    producers.joinAll()
    ch.close()

    var totalCount = 0L
    var totalSum = 0L
    for (c in consumers) {
        val r = c.await()
        totalCount += r.count
        totalSum += r.sum
    }

    val expectedSum = ELEMENT_COUNT * (ELEMENT_COUNT - 1) / 2
    if (totalCount != ELEMENT_COUNT) {
        println("FAIL: Expected $ELEMENT_COUNT elements but consumed $totalCount elements")
    }
    if (totalSum != expectedSum) {
        println("FAIL: Expected $expectedSum sum but got $totalSum sum")
    }
    totalSum
}

fun main(args: Array<String>) {
    val threadCount = threadCountArg(args, 0)

    val per = (threadCount / 2).coerceAtLeast(1)
    val producerCount = per
    val consumerCount = per

    println("threads: $threadCount")
    println("producers: $producerCount")
    println("consumers: $consumerCount")

    runBlocking(newDispatcher(threadCount)) {
        val result = doBench(producerCount, consumerCount) // warmup
        println("output: $result")

        val start = System.nanoTime()
        repeat(ITER_COUNT) { doBench(producerCount, consumerCount) }
        val durUs = ((System.nanoTime() - start) / 1000).coerceAtLeast(1)
        val elementsPerSec = ELEMENT_COUNT * 1_000_000 / durUs

        println("runs:")
        println("  - iteration_count: $ITER_COUNT")
        println("    elements: $ELEMENT_COUNT")
        println("    duration: $durUs us")
        println("    elements/sec: $elementsPerSec")
        println("    max_rss: ${peakMemoryUsageKiB()} KiB")
    }
}
