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

#include <boost/cobalt.hpp>

#include <chrono>
#include <cstddef>
#include <cstdio>
#include <cstdlib>

namespace cobalt = boost::cobalt;

static size_t producer_count = 4;
static size_t consumer_count = 4;
static const size_t iter_count = 1;

static constexpr size_t element_count = 10000000;

static size_t expected_sum;

using token = cobalt::channel<size_t>;

cobalt::promise<void> producer(token& chan, size_t count, size_t base) {
  for (size_t i = 0; i < count; ++i) {
    co_await chan.write(base + i);
  }
}

struct result {
  size_t count;
  size_t sum;
};

cobalt::promise<result> consumer(token& chan) {
  size_t count = 0;
  size_t sum = 0;
  while (chan.is_open()) {
    auto data = co_await chan.read();
    ++count;
    sum += data;
  }
  co_return result{count, sum};
}

static cobalt::task<size_t> do_bench() {
  cobalt::channel<size_t> chan;
  size_t per_task = element_count / producer_count;
  size_t rem = element_count % producer_count;
  std::vector<cobalt::promise<void>> producers;
  producers.reserve(producer_count);
  size_t base = 0;
  for (size_t i = 0; i < producer_count; ++i) {
    size_t count = i < rem ? per_task + 1 : per_task;
    producers.emplace_back(producer(chan, count, base));
    base += count;
  }
  std::vector<cobalt::promise<result>> consumers;
  consumers.reserve(consumer_count);
  for (size_t i = 0; i < consumer_count; ++i) {
    consumers.emplace_back(consumer(chan));
  }
  co_await cobalt::join(producers);
  chan.close();
  auto consResults = co_await cobalt::join(consumers);

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

cobalt::main co_main(int argc, char* argv[]) {
  if (argc > 1) {
    auto thread_count = static_cast<size_t>(atoi(argv[1]));
    // cobalt doesn't actually support multiple threads but we can still scale
    // the number of producers and consumers on a single thread
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

  std::printf("threads: 1\n");
  std::printf("producers: %zu\n", producer_count);
  std::printf("consumers: %zu\n", consumer_count);

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
}
