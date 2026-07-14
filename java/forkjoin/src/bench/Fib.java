// Recursive fork fibonacci via ForkJoinPool / RecursiveTask.
//
// Canonical (TooManyCooks) implementation:
// ../../../../cpp/TooManyCooks/fib.cpp
// Virtual-thread sibling: ../../std/src/bench/Fib.java
//
// Fork fib(n-1) as a ForkJoinTask, compute fib(n-2) inline, then join. Unlike
// the virtual-thread version (one stackful thread per fork), each fork here is a
// lightweight task object on a work-stealing deque -- the JVM-stdlib analog of
// the C++ work-stealing schedulers (TBB / TooManyCooks / libfork). Like those it
// forks all the way down to the base case (no sequential cutoff), so the
// benchmark still measures fine-grained spawn/join overhead.
package bench;

import java.util.concurrent.ForkJoinPool;
import java.util.concurrent.RecursiveTask;

public final class Fib {
  private static final int ITER_COUNT = 1;

  static final class FibTask extends RecursiveTask<Long> {
    private final long n;

    FibTask(long n) {
      this.n = n;
    }

    @Override
    protected Long compute() {
      if (n < 2) {
        return n;
      }
      FibTask f1 = new FibTask(n - 1);
      f1.fork();
      long y = new FibTask(n - 2).compute();
      return f1.join() + y;
    }
  }

  public static void main(String[] args) {
    if (args.length < 1) {
      System.out.println("Usage: fib <n-th fibonacci number requested>");
      return;
    }
    long n;
    try {
      n = Long.parseLong(args[0].trim());
    } catch (NumberFormatException e) {
      System.out.println("Usage: fib <n-th fibonacci number requested>");
      return;
    }
    int threadCount = Bench.threadCountArg(args, 1);
    ForkJoinPool pool = Bench.newPool(threadCount);
    System.out.println("threads: " + threadCount);

    pool.invoke(new FibTask(30)); // warmup

    long start = System.nanoTime();
    long result = 0;
    for (int i = 0; i < ITER_COUNT; i++) {
      result = pool.invoke(new FibTask(n));
    }
    long durUs = (System.nanoTime() - start) / 1000;
    System.out.println("output: " + result);

    System.out.println("runs:");
    System.out.println("  - iteration_count: " + ITER_COUNT);
    System.out.println("    duration: " + durUs + " us");
    System.out.println("    max_rss: " + Bench.peakMemoryUsageKiB() + " KiB");
  }
}
