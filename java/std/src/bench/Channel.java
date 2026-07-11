// Test performance of an MPMC channel primitive.
//
// Canonical (TooManyCooks) implementation:
// ../../../../cpp/TooManyCooks/channel.cpp
//
// N producers each post a slice of ELEMENT_COUNT items; M consumers pull until
// the channel is drained; the total count and sum are validated.
//
// The Go suite uses a Go channel; the Java stdlib's closest MPMC primitive is a
// java.util.concurrent BlockingQueue, so this benchmark shares one bounded
// ArrayBlockingQueue among all producers and consumers. Like Go channels these
// are always bounded (unlike TMC's / async-channel's unbounded queues), so we
// give it a generous buffer to measure steady-state throughput rather than
// constant parking. A BlockingQueue has no "close", so once the producers
// finish we enqueue one POISON sentinel per consumer to end their loops. Values
// are non-negative, so -1 is an unambiguous sentinel; note that, unlike the Go
// channel of uint64, the stdlib queue boxes every element (Long).
package bench;

import java.util.concurrent.ArrayBlockingQueue;
import java.util.concurrent.BlockingQueue;

public final class Channel {
  private static final int ELEMENT_COUNT = 10_000_000;
  private static final int ITER_COUNT = 1;
  private static final int CHANNEL_BUFFER = 1 << 16;
  private static final long POISON = -1L;

  private static final class Counts {
    long count;
    long sum;
  }

  private record Assignment(long count, long base) {}

  // producerAssignments splits ELEMENT_COUNT into per-producer (count, base) work.
  static Assignment[] producerAssignments(int producerCount) {
    long per = (long) ELEMENT_COUNT / producerCount;
    long rem = (long) ELEMENT_COUNT % producerCount;
    long base = 0;
    Assignment[] out = new Assignment[producerCount];
    for (int i = 0; i < producerCount; i++) {
      long count = i < rem ? per + 1 : per;
      out[i] = new Assignment(count, base);
      base += count;
    }
    return out;
  }

  private static void put(BlockingQueue<Long> q, long v) {
    try {
      q.put(v);
    } catch (InterruptedException e) {
      Thread.currentThread().interrupt();
      throw new RuntimeException(e);
    }
  }

  private static long take(BlockingQueue<Long> q) {
    try {
      return q.take();
    } catch (InterruptedException e) {
      Thread.currentThread().interrupt();
      throw new RuntimeException(e);
    }
  }

  static long doBench(int producerCount, int consumerCount) {
    BlockingQueue<Long> ch = new ArrayBlockingQueue<>(CHANNEL_BUFFER);

    Assignment[] assignments = producerAssignments(producerCount);
    Thread[] producers = new Thread[producerCount];
    for (int i = 0; i < producerCount; i++) {
      final Assignment a = assignments[i];
      producers[i] = Thread.startVirtualThread(() -> {
        for (long j = 0; j < a.count(); j++) {
          put(ch, a.base() + j);
        }
      });
    }

    Counts[] results = new Counts[consumerCount];
    Thread[] consumers = new Thread[consumerCount];
    for (int idx = 0; idx < consumerCount; idx++) {
      final int slot = idx;
      consumers[idx] = Thread.startVirtualThread(() -> {
        Counts local = new Counts();
        while (true) {
          long v = take(ch);
          if (v == POISON) {
            break;
          }
          local.count++;
          local.sum += v;
        }
        results[slot] = local;
      });
    }

    // Once every producer has finished, feed one sentinel per consumer so each
    // consumer's loop terminates after draining what remains (FIFO guarantees
    // the sentinels sit behind every real element).
    Bench.joinAll(producers);
    for (int i = 0; i < consumerCount; i++) {
      put(ch, POISON);
    }
    Bench.joinAll(consumers);

    Counts total = new Counts();
    for (Counts r : results) {
      total.count += r.count;
      total.sum += r.sum;
    }

    long expectedSum = (long) ELEMENT_COUNT * ((long) ELEMENT_COUNT - 1) / 2;
    if (total.count != ELEMENT_COUNT) {
      System.out.printf(
        "FAIL: Expected %d elements but consumed %d elements%n",
        (long) ELEMENT_COUNT, total.count
      );
    }
    if (total.sum != expectedSum) {
      System.out.printf("FAIL: Expected %d sum but got %d sum%n", expectedSum, total.sum);
    }
    return total.sum;
  }

  public static void main(String[] args) {
    int threadCount = Bench.configureThreads(args, 0);

    int per = threadCount / 2;
    if (per < 1) {
      per = 1;
    }
    int producerCount = per;
    int consumerCount = per;

    System.out.println("threads: " + threadCount);
    System.out.println("producers: " + producerCount);
    System.out.println("consumers: " + consumerCount);

    long result = doBench(producerCount, consumerCount); // warmup
    System.out.println("output: " + result);

    long start = System.nanoTime();
    for (int i = 0; i < ITER_COUNT; i++) {
      doBench(producerCount, consumerCount);
    }
    long durUs = (System.nanoTime() - start) / 1000;
    if (durUs < 1) {
      durUs = 1;
    }
    long elementsPerSec = (long) ELEMENT_COUNT * 1_000_000 / durUs;

    System.out.println("runs:");
    System.out.println("  - iteration_count: " + ITER_COUNT);
    System.out.println("    elements: " + ELEMENT_COUNT);
    System.out.println("    duration: " + durUs + " us");
    System.out.println("    elements/sec: " + elementsPerSec);
    System.out.println("    max_rss: " + Bench.peakMemoryUsageKiB() + " KiB");
  }
}
