// Package-shared helpers for the Java stdlib (virtual-thread) benchmark
// binaries, so their setup and reported numbers line up with the C++ (TMC), Go
// (stdlib) and Rust (tokio) suites.
package bench;

import java.io.IOException;
import java.nio.file.Files;
import java.nio.file.Path;

public final class Bench {
  private Bench() {}

  // PeakMemoryUsageKiB returns the peak resident set size of the current
  // process in KiB. It reads VmHWM from /proc/self/status, which is what
  // getrusage()'s ru_maxrss reports on Linux, so the number is directly
  // comparable to cpp/2common/memusage.hpp and the Go suite's benchutil.
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
      // Non-Linux or unreadable: fall through to the failure sentinel, matching
      // memusage.hpp's "return -1" behavior.
    }
    return -1;
  }

  // defaultThreads mirrors the C++/Go/Rust default worker count of
  // hardware_concurrency() / 2. It is only used when the harness does not pass
  // an explicit thread count (i.e. manual single runs).
  public static int defaultThreads() {
    int n = Runtime.getRuntime().availableProcessors() / 2;
    return n < 1 ? 1 : n;
  }

  // configureThreads parses args[idx] as a worker-thread count (falling back to
  // defaultThreads() when absent or unparseable), pins the virtual-thread
  // scheduler's carrier pool to that count, and returns it. This is the analog
  // of the C++ executor thread count and Go's runtime.GOMAXPROCS; the benchmark
  // harness passes the thread count in this position (see build_and_bench_all.py).
  //
  // MUST be called before the first virtual thread is created: the default
  // scheduler reads jdk.virtualThreadScheduler.parallelism lazily, when the
  // first virtual thread is started.
  public static int configureThreads(String[] args, int idx) {
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
    setParallelism(n);
    return n;
  }

  // setParallelism pins the virtual-thread scheduler's carrier-pool parallelism.
  // Like configureThreads, it must run before the first virtual thread is
  // created.
  public static void setParallelism(int n) {
    System.setProperty("jdk.virtualThreadScheduler.parallelism", Integer.toString(n));
  }

  // join waits for a (virtual) thread, translating the never-expected
  // interruption into an unchecked exception so the recursive fork-join bodies
  // stay free of checked-exception plumbing.
  public static void join(Thread t) {
    try {
      t.join();
    } catch (InterruptedException e) {
      Thread.currentThread().interrupt();
      throw new RuntimeException(e);
    }
  }

  public static void joinAll(Thread[] threads) {
    for (Thread t : threads) {
      join(t);
    }
  }
}
