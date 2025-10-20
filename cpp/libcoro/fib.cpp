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

#include "coro/coro.hpp"
#include "coro/thread_pool.hpp"

#include <chrono>
#include <cinttypes>
#include <cstdio>
#include <cstdlib>

static size_t thread_count = std::thread::hardware_concurrency() / 2;
static const size_t iter_count = 1;

static coro::task<size_t> fib(coro::thread_pool& tp, size_t n) {
  co_await tp.schedule();

  if (n < 2)
    co_return n;

  auto [x, y] = co_await coro::when_all(fib(tp, n - 1), fib(tp, n - 2));
  co_return x.return_value() + y.return_value();
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

  coro::thread_pool::options opts;
  opts.thread_count = static_cast<uint32_t>(thread_count);
  auto tp = coro::thread_pool::make_shared(opts);

  return coro::sync_wait(
    [](coro::thread_pool& tp, size_t N) -> coro::task<int> {
      co_await tp.schedule();
      auto result = co_await fib(tp, 30); // warmup

      auto startTime = std::chrono::high_resolution_clock::now();

      for (size_t i = 0; i < iter_count; ++i) {
        result = co_await fib(tp, N);
        std::printf("output: %zu\n", result);
      }

      auto endTime = std::chrono::high_resolution_clock::now();
      auto totalTimeUs = std::chrono::duration_cast<std::chrono::microseconds>(
        endTime - startTime
      );
      std::printf("runs:\n");
      std::printf("  - iteration_count: %" PRIu64 "\n", iter_count);
      std::printf("    duration: %" PRIu64 " us\n", totalTimeUs.count());
      co_return 0;
    }(*tp, n)
  );
}
