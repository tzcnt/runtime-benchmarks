// An implementation of the recursive fork fibonacci parallelism test.

// Adapted from https://github.com/tzcnt/tmc-examples/blob/main/examples/fib.cpp
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
#include <folly/executors/CPUThreadPoolExecutor.h>

#include <chrono>
#include <cinttypes>
#include <cstdio>
#include <cstdlib>
#include <thread>

static size_t thread_count = std::thread::hardware_concurrency() / 2;
static const size_t iter_count = 1;

// folly::coro::collectAll starts the child tasks concurrently on the
// current executor; this is folly's idiomatic fork-join construct.
static folly::coro::Task<size_t> fib(size_t n) {
  if (n < 2) {
    co_return n;
  }

  auto [x, y] = co_await folly::coro::collectAll(fib(n - 1), fib(n - 2));
  co_return x + y;
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
  folly::CPUThreadPoolExecutor executor(thread_count);

  auto result =
    folly::coro::blockingWait(co_withExecutor(&executor, fib(30))); // warmup

  auto startTime = std::chrono::high_resolution_clock::now();

  for (size_t i = 0; i < iter_count; ++i) {
    result = folly::coro::blockingWait(co_withExecutor(&executor, fib(n)));
    std::printf("output: %zu\n", result);
  }

  auto endTime = std::chrono::high_resolution_clock::now();
  auto totalTimeUs =
    std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
  std::printf("runs:\n");
  std::printf("  - iteration_count: %" PRIu64 "\n", iter_count);
  std::printf("    duration: %" PRIu64 " us\n", totalTimeUs.count());
  std::printf("    max_rss: %ld KiB\n", peak_memory_usage());
  return 0;
}
