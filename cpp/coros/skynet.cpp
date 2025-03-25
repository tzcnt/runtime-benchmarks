// The skynet benchmark as described here:
// https://github.com/atemerev/skynet

// Adapted from
// https://github.com/tzcnt/tmc-examples/blob/main/examples/skynet/main.cpp
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

#include "start_tasks.h"
#include "thread_pool.h"
#include "wait_tasks.h"

#include <chrono>
#include <cinttypes>
#include <cstdio>
#include <ranges>

static int thread_count = std::thread::hardware_concurrency() / 2;
static const size_t iter_count = 1;

template <size_t DepthMax>
coros::Task<size_t> skynet_one(size_t BaseNum, size_t Depth) {
  if (Depth == DepthMax) {
    co_return BaseNum;
  }
  size_t depthOffset = 1;
  for (size_t i = 0; i < DepthMax - Depth - 1; ++i) {
    depthOffset *= 10;
  }

  auto tasks =
    std::ranges::views::iota(0UL, 10UL) |
    std::ranges::views::transform([=](size_t idx) {
      return skynet_one<DepthMax>(BaseNum + depthOffset * idx, Depth + 1);
    });
  std::vector<coros::Task<size_t>> taskVec(tasks.begin(), tasks.end());

  co_await coros::wait_tasks(taskVec);

  size_t count = 0;
  for (size_t idx = 0; idx < 10; ++idx) {
    count += *taskVec[idx];
  }
  co_return count;
}
template <size_t DepthMax> coros::Task<void> skynet() {
  auto t = skynet_one<DepthMax>(0, 0);
  co_await t;
  size_t count = *t;
  if (count != 4999999950000000) {
    std::printf("ERROR: wrong result - %" PRIu64 "\n", count);
  }
}

template <size_t Depth = 6> coros::Task<void> loop_skynet() {
  std::printf("runs:\n");
  for (size_t j = 0; j < iter_count; ++j) {
    auto startTime = std::chrono::high_resolution_clock::now();

    co_await skynet<Depth>();

    auto endTime = std::chrono::high_resolution_clock::now();
    auto execDur = endTime - startTime;
    auto totalTimeUs = std::chrono::duration_cast<std::chrono::microseconds>(
      endTime - startTime
    );
    std::printf("  - iteration_count: %" PRIu64 "\n", iter_count);
    std::printf("    duration: %" PRIu64 " us\n", totalTimeUs.count());
  }
}

int main() {
  std::printf("threads: %d\n", thread_count);
  coros::ThreadPool tp{thread_count};
  coros::start_sync(tp, skynet<8>());
  coros::start_sync(tp, loop_skynet<8>());
}
