// The skynet benchmark (https://github.com/atemerev/skynet) via ForkJoinPool /
// RecursiveTask.
//
// Canonical (TooManyCooks) implementation:
// ../../../../cpp/TooManyCooks/skynet.cpp
// Virtual-thread sibling: ../../std/src/bench/Skynet.java
//
// Each node forks 10 children until DEPTH_MAX levels deep (10^DEPTH_MAX = 100M
// leaf tasks) and sums their results. Each child is a ForkJoinTask on a
// work-stealing deque.
package bench;

import java.util.concurrent.ForkJoinPool;
import java.util.concurrent.RecursiveTask;

public final class Skynet {
  private static final int DEPTH_MAX = 8;
  private static final int ITER_COUNT = 1;

  static final class SkynetTask extends RecursiveTask<Long> {
    private final long base;
    private final long depth;

    SkynetTask(long base, long depth) {
      this.base = base;
      this.depth = depth;
    }

    @Override
    protected Long compute() {
      if (depth == DEPTH_MAX) {
        return base;
      }
      long offset = 1;
      for (long i = 0; i < DEPTH_MAX - depth - 1; i++) {
        offset *= 10;
      }

      SkynetTask[] children = new SkynetTask[10];
      for (int i = 0; i < 10; i++) {
        children[i] = new SkynetTask(base + offset * i, depth + 1);
      }
      // Fork nine children, run the tenth inline, then join in reverse order:
      // with LIFO deques the most recently forked child is on top, so reverse
      // join pops rather than scans for it.
      for (int i = 1; i < 10; i++) {
        children[i].fork();
      }
      long count = children[0].compute();
      for (int i = 9; i >= 1; i--) {
        count += children[i].join();
      }
      return count;
    }
  }

  static void skynet(ForkJoinPool pool, long expected) {
    long count = pool.invoke(new SkynetTask(0, 0));
    if (count != expected) {
      System.out.printf("ERROR: wrong result - %d%n", count);
    }
  }

  public static void main(String[] args) {
    int threadCount = Bench.threadCountArg(args, 0);
    ForkJoinPool pool = Bench.newPool(threadCount);

    long leaves = 1;
    for (int i = 0; i < DEPTH_MAX; i++) {
      leaves *= 10;
    }
    long expected = (leaves - 1) * leaves / 2;

    System.out.println("threads: " + threadCount);

    skynet(pool, expected); // warmup

    System.out.println("runs:");
    long start = System.nanoTime();
    for (int i = 0; i < ITER_COUNT; i++) {
      skynet(pool, expected);
    }
    long durUs = (System.nanoTime() - start) / 1000;
    System.out.println("  - iteration_count: " + ITER_COUNT);
    System.out.println("    duration: " + durUs + " us");
    System.out.println("    max_rss: " + Bench.peakMemoryUsageKiB() + " KiB");
  }
}
