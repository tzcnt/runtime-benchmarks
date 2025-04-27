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

#include <chrono>
#include <cinttypes>
#include <cstdio>
#include <cstdlib>
#include <thread>

using namespace tmc;
static size_t thread_count = std::thread::hardware_concurrency() / 2;
static const size_t iter_count = 1;

static constexpr size_t element_count = 10000000;

static size_t expected_sum;

using token = tmc::chan_tok<size_t>;

tmc::task<void> producer(token chan, size_t count, size_t base) {
  for (size_t i = 0; i < count; ++i) {
    bool ok = co_await chan.push(base + i);
    assert(ok);
  }
}

struct result {
  size_t count;
  size_t sum;
};

tmc::task<result> consumer(token chan) {
  size_t count = 0;
  size_t sum = 0;
  auto data = co_await chan.pull();
  while (data.has_value()) {
    ++count;
    sum += data.value();
    data = co_await chan.pull();
  }
  co_return result{count, sum};
}

static task<size_t> do_bench() {
  // Half the workers are producers, and half are consumers
  size_t prodCount = 8;
  size_t consCount = 4;
  thread_count = 2;
  auto chan = tmc::make_channel<size_t>();
  size_t per_task = element_count / prodCount;
  size_t rem = element_count % prodCount;
  std::vector<tmc::task<void>> prod(prodCount);
  size_t base = 0;
  for (size_t i = 0; i < prodCount; ++i) {
    size_t count = i < rem ? per_task + 1 : per_task;
    prod[i] = producer(chan, count, base);
    base += count;
  }
  std::vector<tmc::task<result>> cons(consCount);
  for (size_t i = 0; i < consCount; ++i) {
    cons[i] = consumer(chan);
  }
  auto c = tmc::spawn_many(cons.data(), cons.size()).fork();
  co_await tmc::spawn_many(prod.data(), prod.size());

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
  }

  expected_sum = 0;
  for (size_t i = 0; i < element_count; ++i) {
    expected_sum += i;
  }

  std::printf("threads: %" PRIu64 "\n", thread_count);
  tmc::cpu_executor().set_thread_count(thread_count).init();

  return tmc::async_main([]() -> tmc::task<int> {
    {
      auto result = co_await do_bench(); // warmup
      std::printf("output: %zu\n", result);
    }

    auto startTime = std::chrono::high_resolution_clock::now();

    for (size_t i = 0; i < iter_count; ++i) {
      auto result = co_await do_bench();
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
    co_return 0;
  }());
}
