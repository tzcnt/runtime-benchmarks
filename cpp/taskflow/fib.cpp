// An implementation of the recursive fork fibonacci parallelism test.

// Adapted from
// https://github.com/taskflow/taskflow/blob/v3.9.0/examples/fibonacci.cpp
// Original author: taskflow

#include <taskflow/taskflow.hpp>

#include <chrono>
#include <cinttypes>
#include <cstdio>
#include <cstdlib>
#include <thread>

static size_t thread_count = std::thread::hardware_concurrency() / 2;
static const size_t iter_count = 1;

size_t fib(size_t n, tf::Runtime& rt) {
  if (n < 2) {
    return n;
  }
  size_t x, y;
  rt.silent_async([&x, n](tf::Runtime& s) { x = fib(n - 1, s); });
  rt.silent_async([&y, n](tf::Runtime& s) { y = fib(n - 2, s); });

  rt.corun_all();
  return x + y;
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

  tf::Executor executor(thread_count);
  tf::Taskflow taskflow("fibonacci");

  std::printf("threads: %zu\n", thread_count);
  std::printf("results:\n");

  size_t result = 0;

  taskflow.emplace([&result, n](tf::Runtime& rt) { result = fib(n, rt); });
  executor.run(taskflow).wait();

  auto startTime = std::chrono::high_resolution_clock::now();

  for (size_t i = 0; i < iter_count; ++i) {
    result = 0;
    executor.run(taskflow).wait();
    std::printf("  - %" PRIu64 "\n", result);
  }

  auto endTime = std::chrono::high_resolution_clock::now();
  auto totalTimeUs =
    std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
  std::printf("runs:\n");
  std::printf("  - iteration_count: %" PRIu64 "\n", iter_count);
  std::printf("    duration: %" PRIu64 " us\n", totalTimeUs.count());
}
