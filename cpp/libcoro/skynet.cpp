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

#include "coro/coro.hpp"

#include <chrono>
#include <cinttypes>
#include <cstdio>
#include <cstdlib>
#include <ranges>

static size_t thread_count = std::thread::hardware_concurrency() / 2;
static const size_t iter_count = 1;

template <size_t DepthMax>
coro::task<size_t>
skynet_one(coro::thread_pool& tp, size_t BaseNum, size_t Depth) {
  co_await tp.schedule();

  if (Depth == DepthMax) {
    co_return BaseNum;
  }

  size_t depthOffset = 1;
  for (size_t i = 0; i < DepthMax - Depth - 1; ++i) {
    depthOffset *= 10;
  }

  auto results = co_await coro::when_all(
    std::ranges::views::iota(0UL, 10UL) |
    std::ranges::views::transform([=, &tp](size_t idx) {
      return skynet_one<DepthMax>(tp, BaseNum + depthOffset * idx, Depth + 1);
    })
  );

  size_t count = 0;
  for (size_t idx = 0; idx < 10; ++idx) {
    count += results[idx].return_value();
  }
  co_return count;
}
template <size_t DepthMax> coro::task<void> skynet(coro::thread_pool& tp) {
  size_t count = co_await skynet_one<DepthMax>(tp, 0, 0);
  if (count != 4999999950000000) {
    std::printf("ERROR: wrong result - %" PRIu64 "\n", count);
  }
}

template <size_t Depth = 6>
coro::task<void> loop_skynet(coro::thread_pool& tp) {
  std::printf("runs:\n");
  auto startTime = std::chrono::high_resolution_clock::now();
  for (size_t j = 0; j < iter_count; ++j) {
    co_await skynet<Depth>(tp);
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
  std::printf("threads: %" PRIu64 "\n", thread_count);
  coro::thread_pool tp{coro::thread_pool::options{
    .thread_count = static_cast<uint32_t>(thread_count)
  }};

  return coro::sync_wait([](coro::thread_pool& tp) -> coro::task<int> {
    co_await tp.schedule();
    co_await skynet<8>(tp); // warmup
    co_await loop_skynet<8>(tp);
    co_return 0;
  }(tp));
}
