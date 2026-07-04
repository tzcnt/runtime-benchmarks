// Test performance of the channel / async queue primitive

// Original author: tzcnt
// Unlicense License
// This is free and unencumbered software released into the public domain.

// Anyone is free to copy, modify, publish, use, compile, sell, or
// distribute this software, either in source code form or as a compiled
// binary, for any purpose, commercial or non-commercial, and by any
// means.

// In jurisdictions that recognize copyright laws, the author or authors
// of this software dedicate any and all copyright interest in the
// software to the public domain. We make this dedication for the benefit
// of the public at large and to the detriment of our heirs and
// successors. We intend this dedication to be an overt act of
// relinquishment in perpetuity of all present and future rights to this
// software under copyright law.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
// EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
// IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
// OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
// ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
// OTHER DEALINGS IN THE SOFTWARE.

#include "memusage.hpp"

#include <folly/coro/BlockingWait.h>
#include <folly/coro/Collect.h>
#include <folly/coro/Task.h>
#include <folly/coro/UnboundedQueue.h>
#include <folly/executors/CPUThreadPoolExecutor.h>

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <thread>
#include <vector>

static size_t thread_count = std::thread::hardware_concurrency() / 2;
static size_t producer_count = 4;
static size_t consumer_count = 4;
static const size_t iter_count = 1;

static constexpr size_t element_count = 10000000;

// folly's UnboundedQueue has no close/drain operation, so shutdown is
// signaled by enqueueing one sentinel value per consumer after all of the
// producers have completed.
static constexpr size_t sentinel = std::numeric_limits<size_t>::max();

static size_t expected_sum;

using channel = folly::coro::UnboundedQueue<size_t>;

struct result {
  size_t count;
  size_t sum;
};

folly::coro::Task<void> producer(channel& chan, size_t count, size_t base) {
  for (size_t i = 0; i < count; ++i) {
    chan.enqueue(base + i);
  }
  co_return;
}

folly::coro::Task<result> consumer(channel& chan) {
  size_t count = 0;
  size_t sum = 0;
  while (true) {
    size_t data = co_await chan.dequeue();
    if (data == sentinel) {
      break;
    }
    ++count;
    sum += data;
  }
  co_return result{count, sum};
}

// Run all producers to completion, then wake each consumer with a shutdown
// signal.
folly::coro::Task<void>
produce_all(channel& chan, std::vector<folly::coro::Task<void>> producers) {
  co_await folly::coro::collectAllRange(std::move(producers));
  for (size_t i = 0; i < consumer_count; ++i) {
    chan.enqueue(sentinel);
  }
}

static folly::coro::Task<size_t> do_bench() {
  channel chan;

  size_t per_task = element_count / producer_count;
  size_t rem = element_count % producer_count;
  std::vector<folly::coro::Task<void>> producers;
  producers.reserve(producer_count);
  size_t base = 0;
  for (size_t i = 0; i < producer_count; ++i) {
    size_t count = i < rem ? per_task + 1 : per_task;
    producers.push_back(producer(chan, count, base));
    base += count;
  }
  std::vector<folly::coro::Task<result>> consumers;
  consumers.reserve(consumer_count);
  for (size_t i = 0; i < consumer_count; ++i) {
    consumers.push_back(consumer(chan));
  }

  auto [consResults, produced] = co_await folly::coro::collectAll(
    folly::coro::collectAllRange(std::move(consumers)),
    produce_all(chan, std::move(producers))
  );

  size_t count = 0;
  size_t sum = 0;
  for (size_t i = 0; i < consResults.size(); ++i) {
    count += consResults[i].count;
    sum += consResults[i].sum;
  }
  if (count != element_count) {
    std::printf(
      "FAIL: Expected %zu elements but consumed %zu elements\n",
      static_cast<size_t>(element_count), count
    );
  }

  if (sum != expected_sum) {
    std::printf("FAIL: Expected %zu sum but got %zu sum\n", expected_sum, sum);
  }
  co_return sum;
}

int main(int argc, char* argv[]) {
  if (argc > 1) {
    thread_count = static_cast<size_t>(atoi(argv[1]));
    auto c = thread_count / 2;
    if (c == 0) {
      c = 1;
    }
    producer_count = c;
    consumer_count = c;
  }

  // // Allow configuring producer and consumer counts separately for testing
  // // This isn't used by the bench script
  // if (argc > 2) {
  //   producer_count = static_cast<size_t>(atoi(argv[2]));
  //   consumer_count = static_cast<size_t>(atoi(argv[2]));
  // }
  // if (argc > 3) {
  //   consumer_count = static_cast<size_t>(atoi(argv[3]));
  // }

  expected_sum = 0;
  for (size_t i = 0; i < element_count; ++i) {
    expected_sum += i;
  }

  std::printf("threads: %zu\n", thread_count);
  std::printf("producers: %zu\n", producer_count);
  std::printf("consumers: %zu\n", consumer_count);
  folly::CPUThreadPoolExecutor executor(thread_count);

  {
    auto result = folly::coro::blockingWait(
      co_withExecutor(&executor, do_bench())); // warmup
    std::printf("output: %zu\n", result);
  }

  auto startTime = std::chrono::high_resolution_clock::now();

  for (size_t i = 0; i < iter_count; ++i) {
    [[maybe_unused]] auto result =
      folly::coro::blockingWait(co_withExecutor(&executor, do_bench()));
  }

  auto endTime = std::chrono::high_resolution_clock::now();
  auto totalTimeUs =
    std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime)
      .count();

  size_t elementsPerSec = static_cast<size_t>(
    static_cast<float>(element_count) * 1000000.0f /
    static_cast<float>(totalTimeUs)
  );
  std::printf("runs:\n");
  std::printf("  - iteration_count: %zu\n", iter_count);
  std::printf("    elements: %zu\n", element_count);
  std::printf("    duration: %zu us\n", static_cast<size_t>(totalTimeUs));
  std::printf("    elements/sec: %zu\n", elementsPerSec);
  std::printf("    max_rss: %ld KiB\n", peak_memory_usage());
}
