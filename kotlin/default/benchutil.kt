// Helpers shared across the Kotlin coroutine benchmark binaries so their setup
// and reported numbers line up with the C++ (TMC), Rust (tokio), Go, C# and Java
// suites.
//
// This is the "default" variant: coroutines are dispatched onto Kotlin's own
// coroutine scheduler, Dispatchers.Default (kotlinx.coroutines' CoroutineScheduler
// -- a work-stealing scheduler with a per-worker LIFO slot in front of a FIFO
// ring, FIFO stealing, and a lock-free global overflow queue). See newDispatcher.
// The sibling "kotlin/fjp" variant instead uses a JDK ForkJoinPool.
package benchutil

import kotlinx.coroutines.CoroutineDispatcher
import kotlinx.coroutines.Dispatchers
import java.io.File

// Peak resident set size in KiB, read from /proc/self/status VmHWM. This mirrors
// cpp/2common/memusage.hpp and the Go suite's getrusage(RUSAGE_SELF).ru_maxrss
// so the reported numbers are directly comparable across languages.
fun peakMemoryUsageKiB(): Long = try {
    File("/proc/self/status").useLines { lines ->
        lines.firstOrNull { it.startsWith("VmHWM:") }
            ?.let { Regex("\\d+").find(it)?.value?.toLong() }
    } ?: -1L
} catch (e: Exception) {
    -1L
}

// Mirrors the C++/Rust/Go default worker count of hardware_concurrency() / 2.
fun defaultThreads(): Int = (Runtime.getRuntime().availableProcessors() / 2).coerceAtLeast(1)

// Parse args[idx] as a positive worker-thread count, falling back to
// defaultThreads() when absent or unparseable. The benchmark harness passes the
// thread count in this position (see build_and_bench_all.py).
fun threadCountArg(args: Array<String>, idx: Int): Int {
    if (args.size > idx) {
        args[idx].toIntOrNull()?.let { if (it > 0) return it }
    }
    return defaultThreads()
}

// Kotlin's own coroutine scheduler (Dispatchers.Default), pinned to exactly
// `threads` worker threads. Its CoroutineScheduler reads the core/max pool size
// from these system properties exactly once, when it is first initialized, so
// they must be set before any use of Dispatchers.Default. This function is the
// first thing each benchmark's main() touches that references it, so setting them
// here is early enough. Dispatchers.Default's threads are daemons, so the JVM
// exits once main returns; it is a shared singleton and must not be closed.
fun newDispatcher(threads: Int): CoroutineDispatcher {
    System.setProperty("kotlinx.coroutines.scheduler.core.pool.size", threads.toString())
    System.setProperty("kotlinx.coroutines.scheduler.max.pool.size", threads.toString())
    return Dispatchers.Default
}
