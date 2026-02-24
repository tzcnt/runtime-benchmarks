// Adapted from the benchmark provided at:
// https://github.com/ConorWilliams/libfork/blob/ce40fa0f3178a43f5da8016788d6cfdadc85554f/bench/source/nqueens/libfork.cpp

// Original Copyright Notice:
// Copyright © Conor Williams <conorwilliams@outlook.com>

// SPDX-License-Identifier: MPL-2.0

// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "memusage.hpp"
#include <cppcoro/schedule_on.hpp>
#include <cppcoro/shared_task.hpp>
#include <cppcoro/static_thread_pool.hpp>
#include <cppcoro/sync_wait.hpp>
#include <cppcoro/task.hpp>
#include <cppcoro/when_all.hpp>

#include <array>
#include <cinttypes>
#include <cstdio>
#include <cstdlib>
#include <ranges>
#include <vector>

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

template <size_t N>
cppcoro::task<int>
nqueens(cppcoro::static_thread_pool& tp, int xMax, std::array<char, N> buf) {
  co_await tp.schedule();

  if (N == xMax) {
    co_return 1;
  }

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
               std::ranges::views::transform([xMax, &buf, &tp](int y) {
                 buf[xMax] = y;
                 return nqueens(tp, xMax + 1, buf);
               });

  // when_all won't compile if the range is passed directly
  // materializing a vector is required
  std::vector<cppcoro::task<int>> taskVec(tasks.begin(), tasks.end());

  // Spawn up to N tasks (but possibly less, if filter fails)
  auto values = co_await cppcoro::when_all(std::move(taskVec));

  int ret = 0;
  for (int v : values) {
    ret += v;
  }

  co_return ret;
};

int main(int argc, char* argv[]) {
  if (argc > 1) {
    thread_count = static_cast<size_t>(atoi(argv[1]));
  }
  std::printf("threads: %" PRIu64 "\n", thread_count);
  cppcoro::static_thread_pool tp(thread_count);

  return cppcoro::sync_wait(
    [](cppcoro::static_thread_pool& tp) -> cppcoro::task<int> {
      co_await tp.schedule();
      {
        std::array<char, nqueens_work> buf{};
        auto result = co_await nqueens(tp, 0, buf); // warmup
        check_answer(result);
      }

      auto startTime = std::chrono::high_resolution_clock::now();

      for (size_t i = 0; i < iter_count; ++i) {
        std::array<char, nqueens_work> buf{};
        auto result = co_await nqueens(tp, 0, buf);
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
      std::printf("    max_rss: %ld KiB\n", peak_memory_usage());
      co_return 0;
    }(tp)
  );
}
