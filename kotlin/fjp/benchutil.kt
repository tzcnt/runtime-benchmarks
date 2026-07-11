// Helpers shared across the Kotlin coroutine benchmark binaries so their setup
// and reported numbers line up with the C++ (TMC), Rust (tokio), Go, C# and Java
// suites.
//
// This is the "fjp" variant: coroutines are dispatched onto a JDK ForkJoinPool
// in traditional (LIFO-local / FIFO-steal) work-stealing mode. See newDispatcher.
// The sibling "kotlin/default" variant instead uses Kotlin's own coroutine
// scheduler, Dispatchers.Default.
package benchutil

import kotlinx.coroutines.CoroutineDispatcher
import kotlinx.coroutines.asCoroutineDispatcher
import java.io.File
import java.util.concurrent.ForkJoinPool

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

// A JDK ForkJoinPool with exactly `threads` parallelism, in traditional
// work-stealing mode: asyncMode=false gives LIFO-local scheduling (a worker runs
// its own most-recently-forked task first) with FIFO stealing. That runs the
// nested fork-join workloads depth-first, which bounds the live task set --
// unlike the FIFO/async mode that Executors.newWorkStealingPool selects, which
// expands the whole frontier breadth-first and balloons memory.
//
// ForkJoinPool worker threads are daemons, so the JVM exits once main returns;
// the dispatcher is intentionally not closed.
fun newDispatcher(threads: Int): CoroutineDispatcher =
    ForkJoinPool(threads, ForkJoinPool.defaultForkJoinWorkerThreadFactory, null, false)
        .asCoroutineDispatcher()
