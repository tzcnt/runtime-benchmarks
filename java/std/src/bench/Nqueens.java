// Recursive fork-join nqueens.
//
// Canonical (TooManyCooks) implementation:
// ../../../../cpp/TooManyCooks/nqueens.cpp
// which was in turn adapted from libfork's benchmark.
//
// At each column, fork a child task for every valid queen placement. Each fork
// is a virtual thread.
package bench;

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

  static int nqueens(int xMax, byte[] buf) {
    if (NQUEENS_WORK == xMax) {
      return 1;
    }

    // Fork one child per valid placement into its own result slot; taskCount
    // tracks how many of the (up to NQUEENS_WORK) slots were actually used.
    int[] results = new int[NQUEENS_WORK];
    Thread[] threads = new Thread[NQUEENS_WORK];
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
      // Each child needs its own board (Go copies the array by value here).
      byte[] next = buf.clone();
      next[xMax] = (byte) y;
      final int idx = taskCount;
      threads[idx] = Thread.startVirtualThread(
        () -> results[idx] = nqueens(xMax + 1, next)
      );
      taskCount++;
    }
    for (int i = 0; i < taskCount; i++) {
      Bench.join(threads[i]);
    }

    int ret = 0;
    for (int i = 0; i < taskCount; i++) {
      ret += results[i];
    }
    return ret;
  }

  public static void main(String[] args) {
    int threadCount = Bench.configureThreads(args, 0);
    System.out.println("threads: " + threadCount);

    checkAnswer(nqueens(0, new byte[NQUEENS_WORK])); // warmup

    long start = System.nanoTime();
    int result = 0;
    for (int i = 0; i < ITER_COUNT; i++) {
      result = nqueens(0, new byte[NQUEENS_WORK]);
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
