// Adapted from the example provided at:
// https://github.com/David-Haim/concurrencpp/tree/7a88f5401e15e1b64acae70077e40df1a5a9f6bf?tab=readme-ov-file#parallel-fibonacci-example
// Importantly, modified by removing the "if (curr <= 10)" switch to sync.

// Original Copyright Notice:
// Copyright 2020 David Haim
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions: The above copyright
// notice and this permission notice shall be included in all copies or
// substantial portions of the Software. THE SOFTWARE IS PROVIDED "AS IS",
// WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED
// TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
// NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE
// FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
// TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR
// THE USE OR OTHER DEALINGS IN THE SOFTWARE.

#include "concurrencpp/concurrencpp.h"
#include <cinttypes>
#include <concurrencpp/runtime/runtime.h>
#include <cstdio>
#include <cstdlib>

using namespace concurrencpp;
static size_t thread_count = std::thread::hardware_concurrency() / 2;
static const size_t iter_count = 1;

result<size_t> fibonacci(
  executor_tag, std::shared_ptr<thread_pool_executor> tpe, const size_t curr
) {
  if (curr < 2) {
    co_return curr;
  }

  auto x = fibonacci({}, tpe, curr - 1);
  auto y = fibonacci({}, tpe, curr - 2);

  co_return co_await x + co_await y;
}

int main(int argc, char* argv[]) {
  if (argc > 2) {
    thread_count = static_cast<size_t>(atoi(argv[2]));
  }
  if (argc < 2) {
    printf("Usage: fib <n-th fibonacci number requested>\n");
    exit(0);
  }
  size_t n = static_cast<size_t>(atoi(argv[1]));

  std::printf("threads: %" PRIu64 "\n", thread_count);
  concurrencpp::runtime_options opt;
  opt.max_cpu_threads = thread_count;
  concurrencpp::runtime runtime(opt);

  auto result =
    fibonacci({}, runtime.thread_pool_executor(), 30).get(); // warmup

  auto startTime = std::chrono::high_resolution_clock::now();

  for (size_t i = 0; i < iter_count; ++i) {
    auto result = fibonacci({}, runtime.thread_pool_executor(), n).get();
    std::printf("output: %zu\n", result);
  }

  auto endTime = std::chrono::high_resolution_clock::now();
  auto totalTimeUs =
    std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
  std::printf("runs:\n");
  std::printf("  - iteration_count: %" PRIu64 "\n", iter_count);
  std::printf("    duration: %" PRIu64 " us\n", totalTimeUs.count());
  return 0;
}