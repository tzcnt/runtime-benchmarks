// An implementation of the recursive fork fibonacci parallelism test.

// Adapted from
// https://github.com/taskflow/taskflow/blob/v3.9.0/examples/fibonacci.cpp
// Original author: taskflow

#include <taskflow/taskflow.hpp>

#include <chrono>
#include <cinttypes>
#include <cstdio>
#include <cstdlib>
#include <optional>
#include <thread>

static size_t thread_count = std::thread::hardware_concurrency() / 2;
static const size_t iter_count = 1;
std::optional<tf::Executor> executor;

size_t fib(size_t n) {
  if (n < 2) {
    return n;
  }

  tf::TaskGroup tg = executor->task_group();

  size_t x, y;

  tg.silent_async([n, &x]() { x = fib(n - 1); });
  y = fib(n - 2); // compute one branch synchronously

  tg.corun();
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

  executor.emplace(thread_count);

  std::printf("threads: %zu\n", thread_count);

  size_t result = 0;

  executor->async([&result, n]() { result = fib(n); }).get();

  auto startTime = std::chrono::high_resolution_clock::now();

  for (size_t i = 0; i < iter_count; ++i) {
    result = 0;
    executor->async([&result, n]() { result = fib(n); }).get();
    std::printf("output: %zu\n", result);
  }

  auto endTime = std::chrono::high_resolution_clock::now();
  auto totalTimeUs =
    std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
  std::printf("runs:\n");
  std::printf("  - iteration_count: %" PRIu64 "\n", iter_count);
  std::printf("    duration: %" PRIu64 " us\n", totalTimeUs.count());
}
