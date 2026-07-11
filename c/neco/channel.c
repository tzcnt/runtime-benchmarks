// Test performance of an MPMC channel primitive.
//
// Canonical (TooManyCooks) implementation:
// ../../cpp/TooManyCooks/channel.cpp
//
// N producers each post a slice of ELEMENT_COUNT items; M consumers pull
// until the channel is drained; the total count and sum are validated.
//
// neco's native MPMC primitive is neco_chan, shared here by all producer and
// consumer coroutines on the single scheduler thread. Like Go channels (and
// unlike the unbounded queues some other runtimes use), neco channels are
// always bounded; we give it a generous buffer so the benchmark measures
// steady-state throughput rather than constant coroutine parking.

#include "bench_util.h"
#include "neco.h"

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define ELEMENT_COUNT 10000000
#define CHANNEL_BUFFER (1 << 16)

static const int iter_count = 1;

typedef struct {
  uint64_t count;
  uint64_t sum;
} counts;

typedef struct {
  uint64_t count;
  uint64_t base;
} assignment;

static void producer_co(int argc, void *argv[]) {
  (void)argc;
  neco_chan *ch = argv[0];
  assignment a = *(const assignment *)argv[1];
  neco_waitgroup *wg = argv[2];
  for (uint64_t i = 0; i < a.count; i++) {
    uint64_t v = a.base + i;
    neco_chan_send(ch, &v);
  }
  neco_chan_release(ch);
  neco_waitgroup_done(wg);
}

static void consumer_co(int argc, void *argv[]) {
  (void)argc;
  neco_chan *ch = argv[0];
  counts *result = argv[1];
  neco_waitgroup *wg = argv[2];
  uint64_t count = 0;
  uint64_t sum = 0;
  uint64_t v;
  while (neco_chan_recv(ch, &v) == NECO_OK) {
    count++;
    sum += v;
  }
  result->count = count;
  result->sum = sum;
  neco_chan_release(ch);
  neco_waitgroup_done(wg);
}

static int producer_count;
static int consumer_count;

static uint64_t do_bench(void) {
  neco_chan *ch;
  neco_chan_make(&ch, sizeof(uint64_t), CHANNEL_BUFFER);

  // Split ELEMENT_COUNT into per-producer (count, base) work.
  assignment *assignments =
    malloc((size_t)producer_count * sizeof(assignment));
  uint64_t per = ELEMENT_COUNT / (uint64_t)producer_count;
  uint64_t rem = ELEMENT_COUNT % (uint64_t)producer_count;
  uint64_t base = 0;
  for (int i = 0; i < producer_count; i++) {
    uint64_t count = per + ((uint64_t)i < rem ? 1 : 0);
    assignments[i] = (assignment){.count = count, .base = base};
    base += count;
  }

  neco_waitgroup producers;
  neco_waitgroup_init(&producers);
  neco_waitgroup_add(&producers, producer_count);
  for (int i = 0; i < producer_count; i++) {
    neco_chan_retain(ch);
    neco_start(producer_co, 3, ch, &assignments[i], &producers);
  }

  counts *results = calloc((size_t)consumer_count, sizeof(counts));
  neco_waitgroup consumers;
  neco_waitgroup_init(&consumers);
  neco_waitgroup_add(&consumers, consumer_count);
  for (int i = 0; i < consumer_count; i++) {
    neco_chan_retain(ch);
    neco_start(consumer_co, 3, ch, &results[i], &consumers);
  }

  // Once every producer has finished, close the channel so the consumers'
  // recv loops terminate after draining what remains.
  neco_waitgroup_wait(&producers);
  neco_chan_close(ch);
  neco_waitgroup_wait(&consumers);

  counts total = {0, 0};
  for (int i = 0; i < consumer_count; i++) {
    total.count += results[i].count;
    total.sum += results[i].sum;
  }
  free(results);
  free(assignments);
  neco_chan_release(ch);

  uint64_t expected_sum =
    (uint64_t)ELEMENT_COUNT * ((uint64_t)ELEMENT_COUNT - 1) / 2;
  if (total.count != ELEMENT_COUNT) {
    printf(
      "FAIL: Expected %" PRIu64 " elements but consumed %" PRIu64
      " elements\n",
      (uint64_t)ELEMENT_COUNT, total.count
    );
  }
  if (total.sum != expected_sum) {
    printf(
      "FAIL: Expected %" PRIu64 " sum but got %" PRIu64 " sum\n",
      expected_sum, total.sum
    );
  }
  return total.sum;
}

static uint64_t bench_output;
static int64_t bench_dur_us;

static void main_co(int argc, void *argv[]) {
  (void)argc;
  (void)argv;
  bench_output = do_bench(); // warmup

  int64_t start = bench_now_us();
  for (int i = 0; i < iter_count; i++) {
    do_bench();
  }
  bench_dur_us = bench_now_us() - start;
  if (bench_dur_us < 1) {
    bench_dur_us = 1;
  }
}

int main(int argc, char *argv[]) {
  // argv[1] is the harness's worker thread count. The neco scheduler always
  // runs on a single thread; the argument only sizes the producer/consumer
  // coroutine counts, mirroring the other runtimes' structure.
  int threads = 0;
  if (argc > 1) {
    threads = atoi(argv[1]);
  }
  int per = threads / 2;
  if (per < 1) {
    per = 1;
  }
  producer_count = per;
  consumer_count = per;

  printf("threads: 1\n");
  printf("producers: %d\n", producer_count);
  printf("consumers: %d\n", consumer_count);

  neco_start(main_co, 0);

  printf("output: %" PRIu64 "\n", bench_output);

  int64_t elements_per_sec = (int64_t)ELEMENT_COUNT * 1000000 / bench_dur_us;
  printf("runs:\n");
  printf("  - iteration_count: %d\n", iter_count);
  printf("    elements: %d\n", ELEMENT_COUNT);
  printf("    duration: %" PRIi64 " us\n", bench_dur_us);
  printf("    elements/sec: %" PRIi64 "\n", elements_per_sec);
  printf("    max_rss: %ld KiB\n", peak_memory_usage());
}
