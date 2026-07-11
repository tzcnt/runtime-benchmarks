// An implementation of the recursive fork fibonacci parallelism test.
//
// Canonical (TooManyCooks) implementation:
// ../../../../cpp/TooManyCooks/fib.cpp
//
// Fork a task that runs fib(n-1) in parallel with the current task, then
// continue the other leg (fib(n-2)) serially. Each fork is a virtual thread, so
// this measures the JVM's virtual-thread spawn/join throughput.
package bench;

public final class Fib {
  private static final int ITER_COUNT = 1;

  static long fib(long n) {
    if (n < 2) {
      return n;
    }
    // A virtual thread cannot assign to a captured local, so hand it a
    // single-element holder (the analog of Go's captured `var x uint64`).
    long[] x = new long[1];
    Thread t = Thread.startVirtualThread(() -> x[0] = fib(n - 1));
    long y = fib(n - 2);
    Bench.join(t);
    return x[0] + y;
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
    int threadCount = Bench.configureThreads(args, 1);
    System.out.println("threads: " + threadCount);

    fib(30); // warmup

    long start = System.nanoTime();
    long result = 0;
    for (int i = 0; i < ITER_COUNT; i++) {
      result = fib(n);
    }
    long durUs = (System.nanoTime() - start) / 1000;
    System.out.println("output: " + result);

    System.out.println("runs:");
    System.out.println("  - iteration_count: " + ITER_COUNT);
    System.out.println("    duration: " + durUs + " us");
    System.out.println("    max_rss: " + Bench.peakMemoryUsageKiB() + " KiB");
  }
}
