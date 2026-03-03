// CodSpeed benchmark wrapper for the recursive fork fibonacci test
// using the taskflow runtime.

// Adapted from cpp/taskflow/fib.cpp

#include <taskflow/taskflow.hpp>

#include <benchmark/benchmark.h>
#include <cstdlib>
#include <optional>
#include <thread>

static size_t thread_count = std::thread::hardware_concurrency();
std::optional<tf::Executor> executor;

size_t fib(size_t n) {
  if (n < 2) {
    return n;
  }

  tf::TaskGroup tg = executor->task_group();

  size_t x, y;

  tg.silent_async([n, &x]() { x = fib(n - 1); });
  y = fib(n - 2);

  tg.corun();
  return x + y;
}

static void BM_Fib(benchmark::State& state) {
  size_t n = static_cast<size_t>(state.range(0));
  if (!executor.has_value()) {
    executor.emplace(thread_count);
  }

  // warmup
  size_t result = 0;
  executor->async([&result, n]() { result = fib(n); }).get();

  for (auto _ : state) {
    result = 0;
    executor->async([&result, n]() { result = fib(n); }).get();
    benchmark::DoNotOptimize(result);
  }
}
BENCHMARK(BM_Fib)->Arg(20);
BENCHMARK_MAIN();
