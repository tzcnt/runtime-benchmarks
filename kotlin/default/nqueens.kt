// Recursive fork-join nqueens.
//
// Canonical (TooManyCooks) implementation:
// ../../cpp/TooManyCooks/nqueens.cpp
// which was in turn adapted from libfork's benchmark.
//
// At each column, fork a child coroutine for every valid queen placement. Each
// fork is a coroutine started via async {}. The scheduler under test is selected
// by benchutil.newDispatcher.
@file:JvmName("Main")

package nqueens

import benchutil.newDispatcher
import benchutil.peakMemoryUsageKiB
import benchutil.threadCountArg
import kotlinx.coroutines.Deferred
import kotlinx.coroutines.async
import kotlinx.coroutines.coroutineScope
import kotlinx.coroutines.runBlocking

private const val NQUEENS_WORK = 14 // board size
private const val ITER_COUNT = 1

// answers[k] = number of solutions to the k-queens problem.
private val answers = intArrayOf(
    0, 1, 0, 0, 2, 10, 4, 40, 92, 352, 724, 2680, 14200, 73712, 365596, 2279184,
    14772512, 95815104, 666090624,
)

private fun checkAnswer(result: Int) {
    if (result != answers[NQUEENS_WORK]) {
        println("error: expected ${answers[NQUEENS_WORK]}, got $result")
    }
}

private suspend fun nqueens(xMax: Int, buf: ByteArray): Int = coroutineScope {
    if (NQUEENS_WORK == xMax) {
        return@coroutineScope 1
    }

    // Fork one child per valid placement; each gets its own copy of the board.
    val children = ArrayList<Deferred<Int>>(NQUEENS_WORK)
    for (y in 0 until NQUEENS_WORK) {
        val q = y
        var valid = true
        for (x in 0 until xMax) {
            val p = buf[x].toInt()
            val d = xMax - x
            if (q == p || q == p - d || q == p + d) {
                valid = false
                break
            }
        }
        if (!valid) {
            continue
        }
        val next = buf.copyOf()
        next[xMax] = y.toByte()
        children.add(async { nqueens(xMax + 1, next) })
    }

    children.sumOf { it.await() }
}

fun main(args: Array<String>) {
    val threadCount = threadCountArg(args, 0)
    println("threads: $threadCount")

    runBlocking(newDispatcher(threadCount)) {
        checkAnswer(nqueens(0, ByteArray(NQUEENS_WORK))) // warmup

        val start = System.nanoTime()
        var result = 0
        repeat(ITER_COUNT) {
            result = nqueens(0, ByteArray(NQUEENS_WORK))
            checkAnswer(result)
        }
        val durUs = (System.nanoTime() - start) / 1000
        println("output: $result")

        println("runs:")
        println("  - iteration_count: $ITER_COUNT")
        println("    duration: $durUs us")
        println("    max_rss: ${peakMemoryUsageKiB()} KiB")
    }
}
