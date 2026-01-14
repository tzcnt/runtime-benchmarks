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

#define TMC_IMPL

#include "tmc/all_headers.hpp"
#include "tmc/asio/ex_asio.hpp"

#include <chrono>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <thread>

static size_t thread_count = std::thread::hardware_concurrency() / 2;
static size_t producer_count = 4;
static size_t consumer_count = 4;
static const size_t iter_count = 1;

static constexpr size_t element_count = 10000000;

static size_t expected_sum;

using token = tmc::chan_tok<size_t>;

tmc::task<void> producer(token chan, size_t count, size_t base) {
  for (size_t i = 0; i < count; ++i) {
    bool ok = co_await chan.push(base + i);
    assert(ok);
  }
  co_return;
}

struct result {
  size_t count;
  size_t sum;
};

tmc::task<result> consumer(token chan) {
  size_t count = 0;
  size_t sum = 0;
  while (auto data = co_await chan.pull()) {
    ++count;
    sum += data.value();
  }
  co_return result{count, sum};
}

static tmc::task<size_t> do_bench() {
  auto chan = tmc::make_channel<size_t>();

  // Yeah it's a benchmark, let's spin a bit.
  // The other libraries don't let you configure this. Too bad ;)
  chan.set_consumer_spins(10);

  size_t per_task = element_count / producer_count;
  size_t rem = element_count % producer_count;
  std::vector<tmc::task<void>> producers(producer_count);
  size_t base = 0;
  for (size_t i = 0; i < producer_count; ++i) {
    size_t count = i < rem ? per_task + 1 : per_task;
    producers[i] = producer(chan, count, base);
    base += count;
  }
  std::vector<tmc::task<result>> consumers(consumer_count);
  for (size_t i = 0; i < consumer_count; ++i) {
    consumers[i] = consumer(chan);
  }
  auto c = tmc::spawn_many(consumers).fork();
  co_await tmc::spawn_many(producers);

  co_await chan.drain();
  auto consResults = co_await std::move(c);

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
  if (argc > 2) {
    producer_count = static_cast<size_t>(atoi(argv[2]));
    consumer_count = static_cast<size_t>(atoi(argv[2]));
  }
  if (argc > 3) {
    consumer_count = static_cast<size_t>(atoi(argv[3]));
  }

  // Allow switching between asio executor and ex_cpu so that we can compare
  // just the channel performance against boost::cobalt which also uses Asio.
  bool use_asio = false;
  if (argc > 4) {
    if (std::string(argv[4]) == "asio") {
      use_asio = true;
    }
  }

  expected_sum = 0;
  for (size_t i = 0; i < element_count; ++i) {
    expected_sum += i;
  }

  std::printf("threads: %zu\n", thread_count);
  std::printf("producers: %zu\n", producer_count);
  std::printf("consumers: %zu\n", consumer_count);
  tmc::cpu_executor().set_thread_count(thread_count).init();
  tmc::asio_executor().init();

  tmc::ex_any* exec = use_asio ? tmc::asio_executor().type_erased()
                               : tmc::cpu_executor().type_erased();

  {
    auto result = tmc::post_waitable(exec, do_bench()).get(); // warmup
    std::printf("output: %zu\n", result);
  }

  auto startTime = std::chrono::high_resolution_clock::now();

  for (size_t i = 0; i < iter_count; ++i) {
    auto result = tmc::post_waitable(exec, do_bench()).get();
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
  std::printf("    duration: %zu us\n", totalTimeUs);
  std::printf("    elements/sec: %zu\n", elementsPerSec);
}
