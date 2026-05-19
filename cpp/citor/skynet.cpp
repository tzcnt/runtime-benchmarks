// Port of cpp/libfork/skynet.cpp using citor::forkJoinAll.

#include "memusage.hpp"
#include "citor/thread_pool.h"
#include "citor/hints.h"

#include <array>
#include <chrono>
#include <cinttypes>
#include <cstdio>
#include <cstdlib>
#include <thread>

static size_t thread_count = std::thread::hardware_concurrency() / 2;
static const size_t iter_count = 1;

template <size_t DepthMax>
size_t skynet_one(citor::ThreadPool& pool, size_t BaseNum, size_t Depth) {
  if (Depth == DepthMax) {
    return BaseNum;
  }
  size_t depthOffset = 1;
  for (size_t i = 0; i < DepthMax - Depth - 1; ++i) {
    depthOffset *= 10;
  }

  std::array<size_t, 10> results{};
  pool.forkJoinAll<citor::HintsDefaults>(10, [&](size_t idx) {
    results[idx] =
        skynet_one<DepthMax>(pool, BaseNum + depthOffset * idx, Depth + 1);
  });

  size_t count = 0;
  for (size_t idx = 0; idx < 10; ++idx) {
    count += results[idx];
  }
  return count;
}

template <size_t Depth>
void skynet(citor::ThreadPool& pool) {
  size_t count = skynet_one<Depth>(pool, 0, 0);
  if (count != 4999999950000000ULL) {
    std::printf("ERROR: wrong result - %zu\n", count);
  }
}

int main(int argc, char* argv[]) {
  if (argc > 1) {
    thread_count = static_cast<size_t>(atoi(argv[1]));
  }
  std::printf("threads: %zu\n", thread_count);
  citor::ThreadPool pool(thread_count);

  skynet<8>(pool); // warmup

  std::printf("runs:\n");
  auto startTime = std::chrono::high_resolution_clock::now();
  for (size_t j = 0; j < iter_count; ++j) {
    skynet<8>(pool);
  }
  auto endTime = std::chrono::high_resolution_clock::now();
  auto totalTimeUs =
    std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
  std::printf("  - iteration_count: %zu\n", iter_count);
  std::printf("    duration: %" PRIu64 " us\n",
              static_cast<uint64_t>(totalTimeUs.count()));
  std::printf("    max_rss: %ld KiB\n", peak_memory_usage());
  return 0;
}
