// An implementation of the recursive fork fibonacci parallelism test.
//
// Canonical (TooManyCooks) implementation: ../TooManyCooks/fib.cpp
//
// userver has true fork-join primitives: engine::AsyncNoTracing forks a task
// onto the current TaskProcessor and TaskWithResult::Get() joins it, parking
// the calling coroutine until the child completes. As in the Go and TMC
// implementations, fib(n-1) is forked and fib(n-2) is continued serially on
// the current coroutine's stack.

#include "memusage.hpp"
#include "userver_bench.hpp"

#include <userver/engine/async.hpp>
#include <userver/engine/task/task_with_result.hpp>

#include <chrono>
#include <cinttypes>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

namespace engine = userver::engine;

static const std::size_t iter_count = 1;

static std::uint64_t fib(std::uint64_t n) {
  if (n < 2) {
    return n;
  }
  auto x = engine::AsyncNoTracing([n] { return fib(n - 1); });
  const std::uint64_t y = fib(n - 2);
  return x.Get() + y;
}

int main(int argc, char* argv[]) {
  if (argc < 2) {
    std::printf("Usage: fib <n-th fibonacci number requested>\n");
    return 0;
  }
  const std::uint64_t n = strtoull(argv[1], nullptr, 10);
  std::size_t thread_count = bench::default_thread_count();
  if (argc > 2) {
    thread_count = static_cast<std::size_t>(atoi(argv[2]));
  }
  std::printf("threads: %zu\n", thread_count);

  engine::RunStandalone(thread_count, bench::pools_config(true), [n] {
    (void)fib(30); // warmup

    auto startTime = std::chrono::high_resolution_clock::now();
    std::uint64_t result = 0;
    for (std::size_t i = 0; i < iter_count; ++i) {
      result = fib(n);
    }
    auto endTime = std::chrono::high_resolution_clock::now();
    auto totalTimeUs = std::chrono::duration_cast<std::chrono::microseconds>(
      endTime - startTime
    );
    std::printf("output: %" PRIu64 "\n", result);
    std::printf("runs:\n");
    std::printf("  - iteration_count: %zu\n", iter_count);
    std::printf("    duration: %" PRIu64 " us\n",
                static_cast<std::uint64_t>(totalTimeUs.count()));
    std::printf("    max_rss: %ld KiB\n", peak_memory_usage());
  });
}
