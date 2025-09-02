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

#include <taskflow/taskflow.hpp>

#include <chrono>
#include <cinttypes>
#include <cstdio>
#include <cstdlib>

static size_t thread_count = std::thread::hardware_concurrency() / 2;
static const size_t iter_count = 1;

template <size_t DepthMax>
size_t skynet_one(tf::Runtime& rt, size_t BaseNum, size_t Depth) {
  if (Depth == DepthMax) {
    return BaseNum;
  }
  size_t depthOffset = 1;
  for (size_t i = 0; i < DepthMax - Depth - 1; ++i) {
    depthOffset *= 10;
  }

  std::array<size_t, 10> results;

  for (size_t i = 0; i < 10; ++i) {
    rt.silent_async([=, &results, idx = i](tf::Runtime& s) {
      results[idx] =
        skynet_one<DepthMax>(s, BaseNum + depthOffset * idx, Depth + 1);
    });
  }
  rt.corun_all();

  size_t count = 0;
  for (size_t idx = 0; idx < 10; ++idx) {
    count += results[idx];
  }
  return count;
}
template <size_t DepthMax> void skynet(tf::Executor& executor) {
  tf::Taskflow taskflow;
  size_t count;
  taskflow.emplace([&](tf::Runtime& rt) {
    count = skynet_one<DepthMax>(rt, 0, 0);
  });
  executor.run(taskflow).wait();
  if (count != 4999999950000000) {
    std::printf("ERROR: wrong result - %" PRIu64 "\n", count);
  }
}

template <size_t Depth = 6> void loop_skynet(tf::Executor& executor) {
  std::printf("runs:\n");
  auto startTime = std::chrono::high_resolution_clock::now();
  for (size_t j = 0; j < iter_count; ++j) {
    skynet<Depth>(executor);
  }
  auto endTime = std::chrono::high_resolution_clock::now();
  auto totalTimeUs =
    std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
  std::printf("  - iteration_count: %" PRIu64 "\n", iter_count);
  std::printf("    duration: %" PRIu64 " us\n", totalTimeUs.count());
}

int main(int argc, char* argv[]) {
  if (argc > 1) {
    thread_count = static_cast<size_t>(atoi(argv[1]));
  }
  std::printf("threads: %zu\n", thread_count);
  tf::Executor executor(thread_count);
  skynet<8>(executor);
  loop_skynet<8>(executor);
}
