// Recursive fork-join nqueens via ForkJoinPool / RecursiveTask.
//
// Canonical (TooManyCooks) implementation:
// ../../../../cpp/TooManyCooks/nqueens.cpp
// Virtual-thread sibling: ../../std/src/bench/Nqueens.java
//
// At each column, fork a child task for every valid queen placement. Each fork
// is a ForkJoinTask on a work-stealing deque.
package bench;

import java.util.concurrent.ForkJoinPool;
import java.util.concurrent.RecursiveTask;

public final class Nqueens {
  private static final int NQUEENS_WORK = 14; // board size
  private static final int ITER_COUNT = 1;

  // answers[k] = number of solutions to the k-queens problem.
  private static final int[] ANSWERS = {
    0, 1, 0, 0, 2, 10, 4, 40, 92, 352, 724, 2680, 14200, 73712, 365596,
    2279184, 14772512, 95815104, 666090624,
  };

  static void checkAnswer(int result) {
    if (result != ANSWERS[NQUEENS_WORK]) {
      System.out.printf("error: expected %d, got %d%n", ANSWERS[NQUEENS_WORK], result);
    }
  }

  static final class NqueensTask extends RecursiveTask<Integer> {
    private final int xMax;
    private final byte[] buf;

    NqueensTask(int xMax, byte[] buf) {
      this.xMax = xMax;
      this.buf = buf;
    }

    @Override
    protected Integer compute() {
      if (NQUEENS_WORK == xMax) {
        return 1;
      }

      // Fork one child per valid placement into its own slot; taskCount tracks
      // how many of the (up to NQUEENS_WORK) slots were actually used.
      NqueensTask[] children = new NqueensTask[NQUEENS_WORK];
      int taskCount = 0;
      for (int y = 0; y < NQUEENS_WORK; y++) {
        int q = y;
        boolean valid = true;
        for (int x = 0; x < xMax; x++) {
          int p = buf[x];
          int d = xMax - x;
          if (q == p || q == p - d || q == p + d) {
            valid = false;
            break;
          }
        }
        if (!valid) {
          continue;
        }
        // Each child needs its own board (the canonical version copies by value).
        byte[] next = buf.clone();
        next[xMax] = (byte) y;
        children[taskCount++] = new NqueensTask(xMax + 1, next);
      }
      if (taskCount == 0) {
        return 0;
      }

      for (int i = 1; i < taskCount; i++) {
        children[i].fork();
      }
      int ret = children[0].compute();
      for (int i = taskCount - 1; i >= 1; i--) {
        ret += children[i].join();
      }
      return ret;
    }
  }

  public static void main(String[] args) {
    int threadCount = Bench.threadCountArg(args, 0);
    ForkJoinPool pool = Bench.newPool(threadCount);
    System.out.println("threads: " + threadCount);

    checkAnswer(pool.invoke(new NqueensTask(0, new byte[NQUEENS_WORK]))); // warmup

    long start = System.nanoTime();
    int result = 0;
    for (int i = 0; i < ITER_COUNT; i++) {
      result = pool.invoke(new NqueensTask(0, new byte[NQUEENS_WORK]));
      checkAnswer(result);
    }
    long durUs = (System.nanoTime() - start) / 1000;
    System.out.println("output: " + result);

    System.out.println("runs:");
    System.out.println("  - iteration_count: " + ITER_COUNT);
    System.out.println("    duration: " + durUs + " us");
    System.out.println("    max_rss: " + Bench.peakMemoryUsageKiB() + " KiB");
  }
}
