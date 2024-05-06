// The skynet benchmark as described here:
// https://github.com/atemerev/skynet

// Adapted from https://github.com/tzcnt/tmc-examples/blob/main/examples/skynet/main.cpp
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

#include "concurrencpp/concurrencpp.h"
#include <concurrencpp/runtime/runtime.h>

#include <chrono>
#include <cinttypes>
#include <cstdio>

using namespace concurrencpp;
static size_t thread_count = std::thread::hardware_concurrency()/2;
static const size_t iter_count = 1;

template <size_t DepthMax>
result<size_t> skynet_one(executor_tag, std::shared_ptr<thread_pool_executor> executor, size_t BaseNum, size_t Depth) {
  if (Depth == DepthMax) {
    co_return BaseNum;
  }
  size_t depthOffset = 1;
  for (size_t i = 0; i < DepthMax - Depth - 1; ++i) {
    depthOffset *= 10;
  }

  std::array<result<size_t>, 10> children;
    for (size_t idx = 0; idx < 10; ++idx) {
    children[idx] = skynet_one<DepthMax>({}, executor,BaseNum + depthOffset * idx, Depth + 1);
  }

  auto results = co_await when_all(executor, children.begin(), children.end());

  size_t count = 0;
  for (auto& res : results) {
    count += co_await res;
  }
  co_return count;
}
template <size_t DepthMax> result<void> skynet(executor_tag, std::shared_ptr<thread_pool_executor> executor) {
  size_t count = co_await skynet_one<DepthMax>({}, executor, 0, 0);
  if (count != 4999999950000000) {
    std::printf("ERROR: wrong result - %" PRIu64 "\n", count);
  }
}

template <size_t Depth = 6> result<void> loop_skynet(executor_tag, std::shared_ptr<thread_pool_executor> executor) {
  std::printf("runs:\n");
  for (size_t j = 0; j < iter_count; ++j) {
    auto startTime = std::chrono::high_resolution_clock::now();

    co_await skynet<Depth>({}, executor);
    
    auto endTime = std::chrono::high_resolution_clock::now();
    auto execDur = endTime - startTime;
    auto totalTimeUs = std::chrono::duration_cast<std::chrono::microseconds>(
      endTime - startTime
    );
    std::printf("  - iteration_count: %" PRIu64 "\n",iter_count);
    std::printf("    duration: %" PRIu64 " us\n", totalTimeUs.count());
  }
}

int main() {
  std::printf("threads: %" PRIu64 "\n", thread_count);
  concurrencpp::runtime_options opt;
  opt.max_cpu_threads = thread_count;
  concurrencpp::runtime runtime(opt);
  skynet<8>({}, runtime.thread_pool_executor()).get(); // warmup
  loop_skynet<8>({}, runtime.thread_pool_executor()).get();
}
