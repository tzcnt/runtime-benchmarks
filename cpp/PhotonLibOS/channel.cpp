// Test performance of the channel / async queue primitive.
//
// Canonical (TooManyCooks) implementation: ../TooManyCooks/channel.cpp
//
// N producers each post a slice of kElementCount items; M consumers pull
// until the channel is drained; the total count and sum are validated.
//
// Photon's native MPMC primitive is photon::common::RingChannel, a
// photon-aware wrapper (yield-then-park on empty) around its lock-free MPMC
// ring queue - the same construct Photon's own WorkPool dispatches through.
// Like Go channels it is bounded; we give it the same generous 65536-slot
// buffer as the other bounded-channel implementations so the benchmark
// measures steady-state throughput rather than constant task parking.
// RingChannel has no close/drain notion, so after the producers finish, one
// poison value per consumer is pushed; a consumer drains until it pops one,
// which mirrors draining a closed Go channel.
//
// Producers and consumers are bench::threads, distributed round-robin
// across the work-pool vcpus on creation.

#include "memusage.hpp"
#include "photon_bench.hpp"

#include <photon/common/lockfree_queue.h>

#include <chrono>
#include <cinttypes>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <vector>

static std::size_t thread_count;
static std::size_t producer_count = 4;
static std::size_t consumer_count = 4;
static const std::size_t iter_count = 1;

static constexpr std::size_t element_count = 10000000;
static constexpr std::size_t channel_buffer = std::size_t{1} << 16;

// Values are 0..element_count-1, so this can't collide with real data.
static constexpr std::uint64_t poison = ~std::uint64_t{0};

using Queue =
  photon::common::RingChannel<LockfreeMPMCRingQueue<std::uint64_t, channel_buffer>>;

struct result {
  std::uint64_t count;
  std::uint64_t sum;
};

static std::uint64_t do_bench() {
  auto queue = std::make_unique<Queue>();

  std::vector<bench::thread> producers;
  producers.reserve(producer_count);
  const std::size_t per_task = element_count / producer_count;
  const std::size_t rem = element_count % producer_count;
  std::uint64_t base = 0;
  for (std::size_t i = 0; i < producer_count; ++i) {
    const std::uint64_t count = i < rem ? per_task + 1 : per_task;
    producers.emplace_back([&queue, count, base] {
      for (std::uint64_t j = 0; j < count; ++j) {
        queue->send<PhotonPause>(base + j);
      }
    });
    base += count;
  }

  std::vector<result> results(consumer_count);
  std::vector<bench::thread> consumers;
  consumers.reserve(consumer_count);
  for (std::size_t i = 0; i < consumer_count; ++i) {
    result* slot = &results[i];
    consumers.emplace_back([&queue, slot] {
      result r{0, 0};
      for (;;) {
        const std::uint64_t value = queue->recv();
        if (value == poison) {
          break;
        }
        ++r.count;
        r.sum += value;
      }
      *slot = r;
    });
  }

  // Join the producers, then poison the queue once per consumer so the
  // consumers' recv() loops terminate after draining.
  for (auto& producer : producers) {
    producer.join();
  }
  for (std::size_t i = 0; i < consumer_count; ++i) {
    queue->send<PhotonPause>(poison);
  }

  std::uint64_t count = 0;
  std::uint64_t sum = 0;
  for (std::size_t i = 0; i < consumer_count; ++i) {
    consumers[i].join();
    count += results[i].count;
    sum += results[i].sum;
  }

  const std::uint64_t expected_sum =
    static_cast<std::uint64_t>(element_count) * (element_count - 1) / 2;
  if (count != element_count) {
    std::printf(
      "FAIL: Expected %zu elements but consumed %" PRIu64 " elements\n",
      element_count, count
    );
  }
  if (sum != expected_sum) {
    std::printf(
      "FAIL: Expected %" PRIu64 " sum but got %" PRIu64 " sum\n", expected_sum,
      sum
    );
  }
  return sum;
}

int main(int argc, char* argv[]) {
  thread_count = bench::default_thread_count();
  if (argc > 1) {
    thread_count = static_cast<std::size_t>(atoi(argv[1]));
  }
  std::size_t c = thread_count / 2;
  if (c == 0) {
    c = 1;
  }
  producer_count = c;
  consumer_count = c;

  std::printf("threads: %zu\n", thread_count);
  std::printf("producers: %zu\n", producer_count);
  std::printf("consumers: %zu\n", consumer_count);

  bench::quiet_logs();
  bench::use_pooled_stacks();

  bench::run_in_pool(thread_count, [] {
    {
      auto result = do_bench(); // warmup
      std::printf("output: %" PRIu64 "\n", result);
    }

    auto startTime = std::chrono::high_resolution_clock::now();
    for (std::size_t i = 0; i < iter_count; ++i) {
      (void)do_bench();
    }
    auto endTime = std::chrono::high_resolution_clock::now();
    auto totalTimeUs = std::chrono::duration_cast<std::chrono::microseconds>(
                         endTime - startTime
    )
                         .count();

    const std::size_t elementsPerSec = static_cast<std::size_t>(
      static_cast<double>(element_count) * 1000000.0 /
      static_cast<double>(totalTimeUs)
    );
    std::printf("runs:\n");
    std::printf("  - iteration_count: %zu\n", iter_count);
    std::printf("    elements: %zu\n", element_count);
    std::printf("    duration: %" PRIu64 " us\n",
                static_cast<std::uint64_t>(totalTimeUs));
    std::printf("    elements/sec: %zu\n", elementsPerSec);
    std::printf("    max_rss: %ld KiB\n", peak_memory_usage());
  });
}
