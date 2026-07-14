// Test performance of the channel / async queue primitive.
//
// Canonical (TooManyCooks) implementation: ../TooManyCooks/channel.cpp
//
// N producers each post a slice of kElementCount items; M consumers pull
// until the channel is drained; the total count and sum are validated.
//
// userver's native MPMC primitive is concurrent::NonFifoMpmcQueue. Like Go
// channels (and unlike the unbounded queues some other runtimes use), it is
// bounded; we give it a generous buffer so the benchmark measures
// steady-state throughput rather than constant task parking. Consumers' Pop()
// returns false once the queue is empty and every producer handle has been
// destroyed, which mirrors draining a closed Go channel.

#include "memusage.hpp"
#include "userver_bench.hpp"

#include <userver/concurrent/queue.hpp>
#include <userver/engine/async.hpp>
#include <userver/engine/get_all.hpp>
#include <userver/engine/task/task_with_result.hpp>

#include <chrono>
#include <cinttypes>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>

namespace engine = userver::engine;

using Queue = userver::concurrent::NonFifoMpmcQueue<std::uint64_t>;

static std::size_t thread_count;
static std::size_t producer_count = 4;
static std::size_t consumer_count = 4;
static const std::size_t iter_count = 1;

static constexpr std::size_t element_count = 10000000;
static constexpr std::size_t channel_buffer = std::size_t{1} << 16;

struct result {
  std::uint64_t count;
  std::uint64_t sum;
};

static std::uint64_t do_bench() {
  auto queue = Queue::Create(channel_buffer);

  {
    std::vector<engine::TaskWithResult<void>> producers;
    producers.reserve(producer_count);
    const std::size_t per_task = element_count / producer_count;
    const std::size_t rem = element_count % producer_count;
    std::uint64_t base = 0;
    for (std::size_t i = 0; i < producer_count; ++i) {
      const std::uint64_t count = i < rem ? per_task + 1 : per_task;
      producers.push_back(engine::AsyncNoTracing(
        [producer = queue->GetProducer(), count, base]() {
          for (std::uint64_t j = 0; j < count; ++j) {
            const bool ok = producer.Push(base + j);
            if (!ok) {
              std::printf("FAIL: Push failed\n");
              std::abort();
            }
          }
        }
      ));
      base += count;
    }

    std::vector<engine::TaskWithResult<result>> consumers;
    consumers.reserve(consumer_count);
    for (std::size_t i = 0; i < consumer_count; ++i) {
      consumers.push_back(
        engine::AsyncNoTracing([consumer = queue->GetConsumer()]() {
          result r{0, 0};
          std::uint64_t value = 0;
          while (consumer.Pop(value)) {
            ++r.count;
            r.sum += value;
          }
          return r;
        })
      );
    }

    // Join the producers, then destroy their task handles so every producer
    // is gone and the consumers' Pop() loops terminate after draining.
    engine::GetAll(producers);
    producers.clear();

    std::uint64_t count = 0;
    std::uint64_t sum = 0;
    for (auto& consumer : consumers) {
      const result r = consumer.Get();
      count += r.count;
      sum += r.sum;
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
        "FAIL: Expected %" PRIu64 " sum but got %" PRIu64 " sum\n",
        expected_sum, sum
      );
    }
    return sum;
  }
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

  engine::RunStandalone(thread_count, bench::pools_config(false), [] {
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
