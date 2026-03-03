// CodSpeed benchmark wrapper for the skynet test
// using the taskflow runtime.

// Adapted from cpp/taskflow/skynet.cpp

#include <taskflow/taskflow.hpp>

#include <array>
#include <benchmark/benchmark.h>
#include <cinttypes>
#include <cstdlib>
#include <optional>
#include <thread>

static size_t thread_count = std::thread::hardware_concurrency();
std::optional<tf::Executor> executor;

template <size_t DepthMax>
size_t skynet_one(size_t BaseNum, size_t Depth) {
  if (Depth == DepthMax) {
    return BaseNum;
  }
  size_t depthOffset = 1;
  for (size_t i = 0; i < DepthMax - Depth - 1; ++i) {
    depthOffset *= 10;
  }

  std::array<size_t, 10> results;

  tf::TaskGroup tg = executor->task_group();

  for (size_t i = 0; i < 9; ++i) {
    tg.silent_async([=, &results, idx = i]() {
      results[idx] =
        skynet_one<DepthMax>(BaseNum + depthOffset * idx, Depth + 1);
    });
  }
  results[9] =
    skynet_one<DepthMax>(BaseNum + depthOffset * 9, Depth + 1);
  tg.corun();

  size_t count = 0;
  for (size_t idx = 0; idx < 10; ++idx) {
    count += results[idx];
  }
  return count;
}

template <size_t DepthMax>
void skynet(tf::Executor& exec) {
  size_t count;
  exec.async([&]() { count = skynet_one<DepthMax>(0, 0); }).get();
  if (count != 4999999950000000) {
    std::fprintf(
      stderr, "ERROR: wrong result - %" PRIu64 "\n", count
    );
  }
}

static void BM_Skynet(benchmark::State& state) {
  if (!executor.has_value()) {
    executor.emplace(thread_count);
  }

  // warmup
  skynet<8>(*executor);

  for (auto _ : state) {
    skynet<8>(*executor);
  }
}
BENCHMARK(BM_Skynet);
BENCHMARK_MAIN();
