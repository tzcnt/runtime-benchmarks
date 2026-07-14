// The skynet benchmark as described here:
// https://github.com/atemerev/skynet
//
// Canonical (TooManyCooks) implementation:
// ../../../../cpp/TooManyCooks/skynet.cpp
//
// Each node forks 10 children until DEPTH_MAX levels deep (10^DEPTH_MAX = 100M
// leaf tasks) and sums their results. Here each child is a virtual thread, so
// this measures virtual-thread spawn/join throughput under deep nested fan-out.
package bench;

public final class Skynet {
  private static final int DEPTH_MAX = 8;
  private static final int ITER_COUNT = 1;

  static long skynetOne(long base, long depth) {
    if (depth == DEPTH_MAX) {
      return base;
    }
    long depthOffset = 1;
    for (long i = 0; i < DEPTH_MAX - depth - 1; i++) {
      depthOffset *= 10;
    }
    final long offset = depthOffset; // effectively final for capture below

    // Each child writes its own slot, so the disjoint writes need no
    // synchronization beyond join()'s happens-before edge.
    long[] results = new long[10];
    Thread[] threads = new Thread[10];
    for (int i = 0; i < 10; i++) {
      final int idx = i;
      threads[i] = Thread.startVirtualThread(
        () -> results[idx] = skynetOne(base + offset * idx, depth + 1)
      );
    }
    Bench.joinAll(threads);

    long count = 0;
    for (long r : results) {
      count += r;
    }
    return count;
  }

  static void skynet(long expected) {
    long count = skynetOne(0, 0);
    if (count != expected) {
      System.out.printf("ERROR: wrong result - %d%n", count);
    }
  }

  public static void main(String[] args) {
    int threadCount = Bench.configureThreads(args, 0);

    long leaves = 1;
    for (int i = 0; i < DEPTH_MAX; i++) {
      leaves *= 10;
    }
    long expected = (leaves - 1) * leaves / 2;

    System.out.println("threads: " + threadCount);

    skynet(expected); // warmup

    System.out.println("runs:");
    long start = System.nanoTime();
    for (int i = 0; i < ITER_COUNT; i++) {
      skynet(expected);
    }
    long durUs = (System.nanoTime() - start) / 1000;
    System.out.println("  - iteration_count: " + ITER_COUNT);
    System.out.println("    duration: " + durUs + " us");
    System.out.println("    max_rss: " + Bench.peakMemoryUsageKiB() + " KiB");
  }
}
