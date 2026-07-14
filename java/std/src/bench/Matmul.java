// Recursive divide-and-conquer matrix multiplication.
//
// Canonical (TooManyCooks) implementation:
// ../../../../cpp/TooManyCooks/matmul.cpp
// base case: ../../../../cpp/2common/matmul.hpp
//
// The matrices are split into quadrants recursively down to a 32x32 base case.
// The 8 sub-multiplications are run as two sequential groups of 4 parallel
// virtual threads so that no output tile is written by two tasks at once.
//
// A, B and C share three flat backing arrays; each Mat value carries base
// offsets (ao/bo/co) and the full-matrix stride (nn) into them. The parallel
// tasks in a group write disjoint sub-tiles of C, so no locking is required.
package bench;

import java.util.Arrays;

public final class Matmul {
  // A submatrix view: base offsets into the shared a/b/c arrays, the submatrix
  // size n, and the full-matrix stride nn.
  private static final class Mat {
    final int[] a, b, c;
    final int ao, bo, co, n, nn;

    Mat(int[] a, int[] b, int[] c, int ao, int bo, int co, int n, int nn) {
      this.a = a;
      this.b = b;
      this.c = c;
      this.ao = ao;
      this.bo = bo;
      this.co = co;
      this.n = n;
      this.nn = nn;
    }
  }

  static void matmulSmall(Mat m) {
    for (int i = 0; i < m.n; i++) {
      for (int k = 0; k < m.n; k++) {
        for (int j = 0; j < m.n; j++) {
          m.c[m.co + i * m.nn + j] += m.a[m.ao + i * m.nn + k] * m.b[m.bo + k * m.nn + j];
        }
      }
    }
  }

  static void matmul(Mat m) {
    if (m.n <= 32) {
      matmulSmall(m);
      return;
    }
    int k = m.n / 2;
    int nn = m.nn;
    int[] a = m.a, b = m.b, c = m.c;
    int ao = m.ao, bo = m.bo, co = m.co;

    // Group 1: the four products that only touch the "upper-left" halves.
    runGroup(new Mat[] {
      new Mat(a, b, c, ao, bo, co, k, nn),
      new Mat(a, b, c, ao, bo + k, co + k, k, nn),
      new Mat(a, b, c, ao + k * nn, bo, co + k * nn, k, nn),
      new Mat(a, b, c, ao + k * nn, bo + k, co + k * nn + k, k, nn),
    });
    // Group 2: the accumulating products into the same tiles.
    runGroup(new Mat[] {
      new Mat(a, b, c, ao + k, bo + k * nn, co, k, nn),
      new Mat(a, b, c, ao + k, bo + k * nn + k, co + k, k, nn),
      new Mat(a, b, c, ao + k * nn + k, bo + k * nn, co + k * nn, k, nn),
      new Mat(a, b, c, ao + k * nn + k, bo + k * nn + k, co + k * nn + k, k, nn),
    });
  }

  static void runGroup(Mat[] group) {
    Thread[] threads = new Thread[group.length];
    for (int i = 0; i < group.length; i++) {
      final Mat m = group[i];
      threads[i] = Thread.startVirtualThread(() -> matmul(m));
    }
    Bench.joinAll(threads);
  }

  static int[] runMatmul(int n) {
    int[] a = new int[n * n];
    int[] b = new int[n * n];
    int[] c = new int[n * n];
    Arrays.fill(a, 1);
    Arrays.fill(b, 1);
    matmul(new Mat(a, b, c, 0, 0, 0, n, n));
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
    int threadCount = Bench.configureThreads(args, 1);
    System.out.println("threads: " + threadCount);

    runMatmul(n); // warmup

    System.out.println("runs:");
    long start = System.nanoTime();
    int[] result = runMatmul(n);
    long durUs = (System.nanoTime() - start) / 1000;
    validate(result, n);

    System.out.println("  - matrix_size: " + n);
    System.out.println("    duration: " + durUs + " us");
    System.out.println("    max_rss: " + Bench.peakMemoryUsageKiB() + " KiB");
  }
}
