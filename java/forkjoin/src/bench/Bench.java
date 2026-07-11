// Package-shared helpers for the Java ForkJoinPool benchmark binaries. Kept in
// sync with java/std/src/bench/Bench.java so the two Java runtimes report
// directly comparable numbers; the difference is the fork-join primitive
// (ForkJoinPool work-stealing tasks here vs. one virtual thread per fork there).
package bench;

import java.io.IOException;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.concurrent.ForkJoinPool;

public final class Bench {
  private Bench() {}

  // peakMemoryUsageKiB returns the peak resident set size of the current process
  // in KiB, read from VmHWM in /proc/self/status (what getrusage()'s ru_maxrss
  // reports on Linux), so it is directly comparable to cpp/2common/memusage.hpp
  // and the Go/Java-std suites.
  public static long peakMemoryUsageKiB() {
    try {
      for (String line : Files.readAllLines(Path.of("/proc/self/status"))) {
        if (line.startsWith("VmHWM:")) {
          // "VmHWM:\t   123456 kB" -> the number is already in KiB.
          String[] parts = line.trim().split("\\s+");
          return Long.parseLong(parts[1]);
        }
      }
    } catch (IOException | RuntimeException e) {
      // Non-Linux or unreadable: fall through to the failure sentinel.
    }
    return -1;
  }

  // defaultThreads mirrors the C++/Go/Rust default worker count of
  // hardware_concurrency() / 2. Used only when the harness does not pass an
  // explicit thread count.
  public static int defaultThreads() {
    int n = Runtime.getRuntime().availableProcessors() / 2;
    return n < 1 ? 1 : n;
  }

  // threadCountArg parses args[idx] as the ForkJoinPool parallelism (falling back
  // to defaultThreads() when absent or unparseable) and returns it. The benchmark
  // harness passes the thread count in this position (see build_and_bench_all.py).
  public static int threadCountArg(String[] args, int idx) {
    int n = defaultThreads();
    if (args.length > idx) {
      try {
        int v = Integer.parseInt(args[idx].trim());
        if (v > 0) {
          n = v;
        }
      } catch (NumberFormatException e) {
        // keep default
      }
    }
    return n;
  }

  // newPool builds a ForkJoinPool of the given parallelism with asyncMode=false,
  // so each worker drains its own deque in LIFO order. LIFO is the efficient
  // order for recursive divide-and-conquer: the most recently forked subtask is
  // the most cache-hot and is the one about to be joined, so running it next
  // (instead of the oldest, as asyncMode=true / FIFO would) minimizes both cache
  // misses and the number of live, un-joined tasks. FIFO mode exists for
  // event-style tasks that are submitted but never joined. (false is also the
  // ForkJoinPool default; it is set explicitly here to document the intent.)
  public static ForkJoinPool newPool(int parallelism) {
    return new ForkJoinPool(
      parallelism,
      ForkJoinPool.defaultForkJoinWorkerThreadFactory,
      null,   // no special uncaught-exception handler
      false   // asyncMode=false => LIFO local execution
    );
  }
}
