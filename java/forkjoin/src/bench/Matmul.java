// Recursive divide-and-conquer matrix multiplication via ForkJoinPool /
// RecursiveAction.
//
// Canonical (TooManyCooks) implementation:
// ../../../../cpp/TooManyCooks/matmul.cpp
// base case: ../../../../cpp/2common/matmul.hpp
// Virtual-thread sibling: ../../std/src/bench/Matmul.java
//
// The matrices are split into quadrants recursively down to a 32x32 base case.
// The 8 sub-multiplications run as two sequential groups of 4 parallel tasks so
// that no output tile is written by two tasks at once. Each task is a subtile
// view over three shared flat backing arrays (base offsets ao/bo/co, stride nn);
// the four tasks in a group write disjoint sub-tiles of C, so no locking is
// required. invokeAll forks three of the four and computes the last inline.
package bench;

import java.util.Arrays;
import java.util.concurrent.ForkJoinPool;
import java.util.concurrent.RecursiveAction;

public final class Matmul {
  static final class MatmulTask extends RecursiveAction {
    private final int[] a, b, c;
    private final int ao, bo, co, n, nn;

    MatmulTask(int[] a, int[] b, int[] c, int ao, int bo, int co, int n, int nn) {
      this.a = a;
      this.b = b;
      this.c = c;
      this.ao = ao;
      this.bo = bo;
      this.co = co;
      this.n = n;
      this.nn = nn;
    }

    @Override
    protected void compute() {
      if (n <= 32) {
        matmulSmall();
        return;
      }
      int k = n / 2;

      // Group 1: the four products that only touch the "upper-left" halves.
      invokeAll(
        new MatmulTask(a, b, c, ao, bo, co, k, nn),
        new MatmulTask(a, b, c, ao, bo + k, co + k, k, nn),
        new MatmulTask(a, b, c, ao + k * nn, bo, co + k * nn, k, nn),
        new MatmulTask(a, b, c, ao + k * nn, bo + k, co + k * nn + k, k, nn)
      );
      // Group 2: the accumulating products into the same tiles.
      invokeAll(
        new MatmulTask(a, b, c, ao + k, bo + k * nn, co, k, nn),
        new MatmulTask(a, b, c, ao + k, bo + k * nn + k, co + k, k, nn),
        new MatmulTask(a, b, c, ao + k * nn + k, bo + k * nn, co + k * nn, k, nn),
        new MatmulTask(a, b, c, ao + k * nn + k, bo + k * nn + k, co + k * nn + k, k, nn)
      );
    }

    private void matmulSmall() {
      for (int i = 0; i < n; i++) {
        for (int k = 0; k < n; k++) {
          for (int j = 0; j < n; j++) {
            c[co + i * nn + j] += a[ao + i * nn + k] * b[bo + k * nn + j];
          }
        }
      }
    }
  }

  static int[] runMatmul(ForkJoinPool pool, int n) {
    int[] a = new int[n * n];
    int[] b = new int[n * n];
    int[] c = new int[n * n];
    Arrays.fill(a, 1);
    Arrays.fill(b, 1);
    pool.invoke(new MatmulTask(a, b, c, 0, 0, 0, n, n));
    return c;
  }

  static void validate(int[] c, int n) {
    for (int i = 0; i < n; i++) {
      for (int j = 0; j < n; j++) {
        int res = c[i * n + j];
        if (res != n) {
          System.out.printf("Wrong result at (%d,%d) : %d. expected %d%n", i, j, res, n);
          System.exit(1);
        }
      }
    }
  }

  public static void main(String[] args) {
    if (args.length < 1) {
      System.out.println("Usage: matmul <matrix size (power of 2)>");
      return;
    }
    int n;
    try {
      n = Integer.parseInt(args[0].trim());
    } catch (NumberFormatException e) {
      System.out.println("Usage: matmul <matrix size (power of 2)>");
      return;
    }
    int threadCount = Bench.threadCountArg(args, 1);
    ForkJoinPool pool = Bench.newPool(threadCount);
    System.out.println("threads: " + threadCount);

    runMatmul(pool, n); // warmup

    System.out.println("runs:");
    long start = System.nanoTime();
    int[] result = runMatmul(pool, n);
    long durUs = (System.nanoTime() - start) / 1000;
    validate(result, n);

    System.out.println("  - matrix_size: " + n);
    System.out.println("    duration: " + durUs + " us");
    System.out.println("    max_rss: " + Bench.peakMemoryUsageKiB() + " KiB");
  }
}
