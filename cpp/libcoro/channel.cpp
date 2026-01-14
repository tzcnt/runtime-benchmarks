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

#include "coro/coro.hpp" // IWYU pragma: keep

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <thread>

static size_t thread_count = std::thread::hardware_concurrency() / 2;
static size_t producer_count = 4;
static size_t consumer_count = 4;
static const size_t iter_count = 1;

static constexpr size_t element_count = 10000000;

static size_t expected_sum;

struct result {
  size_t count;
  size_t sum;
};

coro::task<result> producer(
  coro::queue<size_t>& chan, size_t count, size_t base,
  std::atomic<size_t>& countDown, std::unique_ptr<coro::thread_pool>& tp
) {
  co_await tp->schedule();
  for (size_t i = 0; i < count; ++i) {
    [[maybe_unused]] auto result = co_await chan.push(base + i);
    assert(result == coro::queue_produce_result::produced);
  }

  // Since we can't fork and await them separately in the main task, one of the
  // producers has to handle shutting down the channel.
  if (countDown.fetch_sub(1) == 1) {
    co_await chan.shutdown_drain(tp);
  }
  co_return {};
}

coro::task<result>
consumer(coro::queue<size_t>& chan, std::unique_ptr<coro::thread_pool>& tp) {
  co_await tp->schedule();
  size_t count = 0;
  size_t sum = 0;
  while (auto data = co_await chan.pop()) {
    ++count;
    sum += data.value();
  }
  co_return result{count, sum};
}

static coro::task<size_t> do_bench(std::unique_ptr<coro::thread_pool>& tp) {
  auto chan = coro::queue<size_t>();
  size_t per_task = element_count / producer_count;
  size_t rem = element_count % producer_count;

  // I can't figure out how to fork the producers separately using libcoro.
  // So I make the return type the same as the consumers and initiate them all
  // at once.
  std::vector<coro::task<result>> tasks(producer_count + consumer_count);
  size_t base = 0;
  std::atomic<size_t> countDown = producer_count;
  for (size_t i = 0; i < producer_count; ++i) {
    size_t count = i < rem ? per_task + 1 : per_task;
    tasks[i] = producer(chan, count, base, countDown, tp);
    base += count;
  }
  for (size_t i = producer_count; i < producer_count + consumer_count; ++i) {
    tasks[i] = consumer(chan, tp);
  }
  auto results = co_await coro::when_all(std::move(tasks));

  size_t count = 0;
  size_t sum = 0;
  for (size_t i = 0; i < results.size(); ++i) {
    count += results[i].return_value().count;
    sum += results[i].return_value().sum;
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
  }
  if (argc > 2) {
    producer_count = static_cast<size_t>(atoi(argv[2]));
    consumer_count = static_cast<size_t>(atoi(argv[2]));
  }
  if (argc > 3) {
    consumer_count = static_cast<size_t>(atoi(argv[3]));
  }

  expected_sum = 0;
  for (size_t i = 0; i < element_count; ++i) {
    expected_sum += i;
  }

  std::printf("threads: %zu\n", thread_count);
  std::printf("producers: %zu\n", producer_count);
  std::printf("consumers: %zu\n", consumer_count);
  std::unique_ptr<coro::thread_pool> tp = coro::thread_pool::make_unique(
    coro::thread_pool::options{
      .thread_count = static_cast<uint32_t>(thread_count)
    }
  );

  return coro::sync_wait(
    [](std::unique_ptr<coro::thread_pool>& tp) -> coro::task<int> {
      co_await tp->schedule();
      {
        auto result = co_await do_bench(tp); // warmup
        std::printf("output: %zu\n", result);
      }

      auto startTime = std::chrono::high_resolution_clock::now();

      for (size_t i = 0; i < iter_count; ++i) {
        [[maybe_unused]] auto result = co_await do_bench(tp);
      }

      auto endTime = std::chrono::high_resolution_clock::now();
      auto totalTimeUs = std::chrono::duration_cast<std::chrono::microseconds>(
                           endTime - startTime
      )
                           .count();

      size_t elementsPerSec = static_cast<size_t>(
        static_cast<float>(element_count) * 1000000.0f /
        static_cast<float>(totalTimeUs)
      );
      std::printf("runs:\n");
      std::printf("  - iteration_count: %zu\n", iter_count);
      std::printf("    elements: %zu\n", element_count);
      std::printf("    duration: %zu us\n", totalTimeUs);
      std::printf("    elements/sec: %zu\n", elementsPerSec);
      co_return 0;
    }(tp)
  );
}
