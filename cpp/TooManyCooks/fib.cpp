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

#define TMC_IMPL

#include "memusage.hpp"
#include "tmc/all_headers.hpp"

#include <chrono>
#include <cinttypes>
#include <cstdio>
#include <cstdlib>

static size_t thread_count = std::thread::hardware_concurrency() / 2;
static const size_t iter_count = 1;

// This is the approach used by most libraries. Spawn a hot task that runs in
// parallel with the current task, then continue the other leg serially.
// It has better performance than the tuple variant (by using separate
// synchronization variables) but uses more memory for the same reason.
static tmc::task<size_t> fib(size_t n) {
  if (n < 2)
    co_return n;

  auto x_hot = spawn(fib(n - 1)).fork();
  auto y = co_await fib(n - 2);
  auto x = co_await std::move(x_hot);
  co_return x + y;

  // // TooManyCooks offers a cleaner approach that uses less memory.
  // // However it does run slightly slower (due to the shared synchronization
  // // variable) for this specific example.
  // // Fork the tasks in parallel and retrieve the results in a tuple.
  //   auto [x, y] = co_await spawn_tuple(fib(n - 1), fib(n - 2));
  //   co_return x + y;
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
  tmc::cpu_executor()
    .set_thread_count(thread_count)
    .set_thread_pinning_level(tmc::topology::thread_pinning_level::CORE)
    .init();

  return tmc::async_main([](size_t N) -> tmc::task<int> {
    auto result = co_await fib(30); // warmup

    auto startTime = std::chrono::high_resolution_clock::now();

    for (size_t i = 0; i < iter_count; ++i) {
      result = co_await fib(N);
      std::printf("output: %zu\n", result);
    }

    auto endTime = std::chrono::high_resolution_clock::now();
    auto totalTimeUs = std::chrono::duration_cast<std::chrono::microseconds>(
      endTime - startTime
    );
    std::printf("runs:\n");
    std::printf("  - iteration_count: %" PRIu64 "\n", iter_count);
    std::printf("    duration: %" PRIu64 " us\n", totalTimeUs.count());
    std::printf("    max_rss: %ld KiB\n", peak_memory_usage());
    co_return 0;
  }(n));
}
