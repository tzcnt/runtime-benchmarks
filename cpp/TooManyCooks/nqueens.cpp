// Adapted from the benchmark provided at:
// https://github.com/ConorWilliams/libfork/blob/ce40fa0f3178a43f5da8016788d6cfdadc85554f/bench/source/nqueens/libfork.cpp

// Original Copyright Notice:
// Copyright © Conor Williams <conorwilliams@outlook.com>

// SPDX-License-Identifier: MPL-2.0

// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#define TMC_IMPL

#include "tmc/ex_cpu.hpp"
#include "tmc/spawn_many.hpp"
#include "tmc/task.hpp"
#include <array>
#include <cinttypes>
#include <cstdio>
#include <cstdlib>
#include <ranges>

static size_t thread_count = std::thread::hardware_concurrency() / 2;
static const size_t iter_count = 1;

inline constexpr int nqueens_work = 14;

inline constexpr std::array<int, 28> answers = {
  0,       1,         0,          0,          2,           10,     4,
  40,      92,        352,        724,        2'680,       14'200, 73'712,
  365'596, 2'279'184, 14'772'512, 95'815'104, 666'090'624,
};

void check_answer(int result) {
  if (result != answers[nqueens_work]) {
    std::printf("error: expected %d, got %d\n", answers[nqueens_work], result);
  }
}

template <size_t N> tmc::task<int> nqueens(int xMax, std::array<char, N> buf) {
  if (N == xMax) {
    co_return 1;
  }

  size_t taskCount = 0;
  auto tasks = std::ranges::views::iota(0UL, N) |
               std::ranges::views::filter([xMax, &buf](int y) {
                 char q = y;
                 for (int x = 0; x < xMax; x++) {
                   char p = buf[x];
                   if (q == p || q == p - (xMax - x) || q == p + (xMax - x)) {
                     return false;
                   }
                 }
                 return true;
               }) |
               std::ranges::views::transform([xMax, &buf, &taskCount](int y) {
                 buf[xMax] = y;
                 ++taskCount;
                 return nqueens(xMax + 1, buf);
               });

  // Spawn up to N tasks (but possibly less, if filter returns false).
  // Using N template parameter allows storage in an array (not vector),
  // avoiding an allocation. Thus we have to track taskCount manually.
  auto values = co_await tmc::spawn_many<N>(tasks.begin(), tasks.end());

  int ret = 0;
  for (size_t i = 0; i < taskCount; ++i) {
    ret += values[i];
  }

  co_return ret;
};

int main(int argc, char* argv[]) {
  if (argc > 1) {
    thread_count = static_cast<size_t>(atoi(argv[1]));
  }
  std::printf("threads: %" PRIu64 "\n", thread_count);
  tmc::cpu_executor()
    .set_thread_count(thread_count)
    .set_thread_pinning_level(tmc::topology::ThreadPinningLevel::CORE)
    .init();

  return tmc::async_main([]() -> tmc::task<int> {
    {
      std::array<char, nqueens_work> buf{};
      auto result = co_await nqueens(0, buf); // warmup
      check_answer(result);
    }

    auto startTime = std::chrono::high_resolution_clock::now();

    for (size_t i = 0; i < iter_count; ++i) {
      std::array<char, nqueens_work> buf{};
      auto result = co_await nqueens(0, buf);
      check_answer(result);
      std::printf("output: %d\n", result);
    }

    auto endTime = std::chrono::high_resolution_clock::now();
    auto totalTimeUs = std::chrono::duration_cast<std::chrono::microseconds>(
      endTime - startTime
    );
    std::printf("runs:\n");
    std::printf("  - iteration_count: %" PRIu64 "\n", iter_count);
    std::printf("    duration: %" PRIu64 " us\n", totalTimeUs.count());
    co_return 0;
  }());
}
