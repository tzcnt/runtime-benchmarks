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
#include "tmc/task.hpp"
#include "tmc/spawn_many.hpp"
#include <cstdio>
#include <cinttypes>
#include <array>
#include <ranges>

static size_t thread_count = std::thread::hardware_concurrency()/2;
static const size_t iter_count = 1;

inline constexpr int nqueens_work = 14;

inline constexpr std::array<int, 28> answers = {
    0,   1,     0,      0,      2,       10,        4,          40,         92,          352,
    724, 2'680, 14'200, 73'712, 365'596, 2'279'184, 14'772'512, 95'815'104, 666'090'624,
};

inline auto queens_ok(int n, char const *a) -> bool {
  for (int i = 0; i < n; i++) {
    char p = a[i];
    for (int j = i + 1; j < n; j++) {
      if (char q = a[j]; q == p || q == p - (j - i) || q == p + (j - i)) {
        return false;
      }
    }
  }
  return true;
}

template <size_t N>
tmc::task<int> nqueens(int j, std::array<char, N> const& a) {
  if (N == j) {
    co_return 1;
  }

  std::array<std::array<char, N>, N> buf;
  for (int i = 0; i < N; i++) {
    for (int k = 0; k < j; k++) {
      buf[i][k] = a[k];
    }
    buf[i][j] = i;
  }

  int taskCount = 0;
  auto tasks = std::ranges::views::iota(0UL, N) |
               std::ranges::views::filter([j, &buf](int i) {
                 return queens_ok(j + 1, buf[i].data());
               }) |
               std::ranges::views::transform([j, &buf, &taskCount](int i) {
                 ++taskCount;
                 return nqueens(j + 1, buf[i]);
               });

  // Spawn up to N tasks (but possibly less, if queens_ok fails)
  auto parts = co_await tmc::spawn_many<N>(tasks.begin(), tasks.end());

  int ret = 0;
  for (size_t i = 0; i < taskCount; ++i) {
    ret += parts[i];
  }

  co_return ret;
};


int main(int argc, char* argv[]) {
  std::printf("threads: %" PRIu64 "\n", thread_count);
  tmc::cpu_executor().set_thread_count(thread_count).init();

  return tmc::async_main([]() -> tmc::task<int> {
    {
      std::array<char, nqueens_work> buf{};
      auto result = co_await nqueens(0, buf); // warmup
    }

    auto startTime = std::chrono::high_resolution_clock::now();

    for (size_t i = 0; i < iter_count; ++i) {
      std::array<char, nqueens_work> buf{};
      auto result = co_await nqueens(0, buf);
      std::printf("  - %d\n", result);
    }

    auto endTime = std::chrono::high_resolution_clock::now();
    auto totalTimeUs = std::chrono::duration_cast<std::chrono::microseconds>(
      endTime - startTime
    );
    std::printf("runs:\n");
    std::printf("  - iteration_count: %" PRIu64 "\n",iter_count);
    std::printf("    duration: %" PRIu64 " us\n", totalTimeUs.count());
    co_return 0;
  }());
}