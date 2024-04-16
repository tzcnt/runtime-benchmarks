// The skynet benchmark as described here:
// https://github.com/atemerev/skynet

// Adapted from https://github.com/tzcnt/tmc-examples/blob/main/examples/skynet/main.cpp
// Original author: tzcnt
// Unlicense License

#define TMC_IMPL

#include "tmc/ex_cpu.hpp"
#include "tmc/sync.hpp"
#include "tmc/task.hpp"
#include "tmc/spawn_task_many.hpp"
#include "tmc/utils.hpp"

#include <chrono>
#include <cinttypes>
#include <cstdio>

template <size_t DepthMax>
tmc::task<size_t> skynet_one(size_t BaseNum, size_t Depth) {
  if (Depth == DepthMax) {
    co_return BaseNum;
  }
  size_t depthOffset = 1;
  for (size_t i = 0; i < DepthMax - Depth - 1; ++i) {
    depthOffset *= 10;
  }

  /// Simplest way to spawn subtasks
  // std::array<tmc::task<size_t>, 10> children;
  // for (size_t idx = 0; idx < 10; ++idx) {
  //   children[idx] = skynet_one<DepthMax>(BaseNum + depthOffset * idx,
  //   Depth + 1);
  // }
  // std::array<size_t, 10> results = co_await
  // tmc::spawn_many<10>(children.data());

  /// Concise and slightly faster way to run subtasks
  std::array<size_t, 10> results =
    co_await tmc::spawn_many<10>(tmc::iter_adapter(
      0ULL,
      [=](size_t idx) -> tmc::task<size_t> {
        return skynet_one<DepthMax>(BaseNum + depthOffset * idx, Depth + 1);
      }
    ));

  size_t count = 0;
  for (size_t idx = 0; idx < 10; ++idx) {
    count += results[idx];
  }
  co_return count;
}
template <size_t DepthMax> tmc::task<void> skynet() {
  size_t count = co_await skynet_one<DepthMax>(0, 0);
  if (count != 499999500000) {
    std::printf("%" PRIu64 "\n", count);
  }
}

template <size_t Depth = 6> void run_skynet() {
  tmc::ex_cpu executor;
  executor.init();
  auto startTime = std::chrono::high_resolution_clock::now();
  auto future = tmc::post_waitable(executor, skynet<Depth>(), 0);
  future.wait();
  auto endTime = std::chrono::high_resolution_clock::now();

  auto execDur =
    std::chrono::duration_cast<std::chrono::nanoseconds>(endTime - startTime);
  std::printf(
    "executed skynet in %" PRIu64 " ns: %" PRIu64 " thread-ns\n",
    execDur.count(),
    executor.thread_count() * static_cast<size_t>(execDur.count())
  );
}

template <size_t Depth = 6> tmc::task<void> loop_skynet() {
  const size_t iter_count = 1;
  for (size_t j = 0; j < 5; ++j) {
    auto startTime = std::chrono::high_resolution_clock::now();
    for (size_t i = 0; i < iter_count; ++i) {
      co_await skynet<Depth>();
    }
    auto endTime = std::chrono::high_resolution_clock::now();
    auto execDur = endTime - startTime;
    auto totalTimeUs = std::chrono::duration_cast<std::chrono::microseconds>(
      endTime - startTime
    );
    std::printf(
      "%" PRIu64 " skynet iterations in %" PRIu64 " us: %" PRIu64
      " thread-us\n",
      iter_count, totalTimeUs.count(),
      tmc::cpu_executor().thread_count() *
        static_cast<size_t>(totalTimeUs.count())
    );
  }
}

int main() {
  tmc::cpu_executor().set_thread_count(std::thread::hardware_concurrency()/2).init();
  return tmc::async_main([]() -> tmc::task<int> {
    co_await loop_skynet<6>();
    co_return 0;
  }());
}
