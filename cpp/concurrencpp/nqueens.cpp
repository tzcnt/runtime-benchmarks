// Adapted from the benchmark provided at:
// https://github.com/ConorWilliams/libfork/blob/ce40fa0f3178a43f5da8016788d6cfdadc85554f/bench/source/nqueens/libfork.cpp

// Original Copyright Notice:
// Copyright © Conor Williams <conorwilliams@outlook.com>

// SPDX-License-Identifier: MPL-2.0

// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "concurrencpp/concurrencpp.h"
#include <concurrencpp/runtime/runtime.h>

#include <cstdio>
#include <cinttypes>
#include <array>
#include <ranges>

using namespace concurrencpp;
static size_t thread_count = std::thread::hardware_concurrency()/2;
static const size_t iter_count = 1;

inline constexpr int nqueens_work = 14;

inline constexpr std::array<int, 28> answers = {
    0,   1,     0,      0,      2,       10,        4,          40,         92,          352,
    724, 2'680, 14'200, 73'712, 365'596, 2'279'184, 14'772'512, 95'815'104, 666'090'624,
};

inline auto queens_ok(int n, char *a) -> bool {
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

template<size_t N>
result<int>
nqueens(executor_tag, std::shared_ptr<thread_pool_executor> executor, int j, std::array<char, N> const &a) {
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
               std::ranges::views::transform([j, executor, &buf, &taskCount](int i) {
                 ++taskCount;
                 return nqueens({}, executor, j + 1, buf[i]);
               });

  // Calling when_all(tasks.begin(), tasks.end()) directly seems to trigger some kind of UB
  // Materializing a vector first fixes it
  std::vector<result<int>> taskVec = std::vector<result<int>>(tasks.begin(), tasks.end());

  auto parts = co_await when_all(executor, taskVec.begin(), taskVec.end());


  int ret = 0;
  for (auto& p : parts) {
    ret += co_await p;
  }

  co_return ret;
};


int main(int argc, char* argv[]) {
  std::printf("threads: %" PRIu64 "\n", thread_count);
  concurrencpp::runtime_options opt;
  opt.max_cpu_threads = thread_count;
  concurrencpp::runtime runtime(opt);
  {
    std::array<char, nqueens_work> buf{};
    auto result = nqueens({}, runtime.thread_pool_executor(), 0, buf).get(); // warmup
  }
  
  std::printf("results:\n");
  auto startTime = std::chrono::high_resolution_clock::now();

  for (size_t i = 0; i < iter_count; ++i) {
    std::array<char, nqueens_work> buf{};
    auto result = nqueens({}, runtime.thread_pool_executor(), 0, buf).get(); // warmup
    std::printf("  - %d\n", result);
  }

  auto endTime = std::chrono::high_resolution_clock::now();
  auto totalTimeUs = std::chrono::duration_cast<std::chrono::microseconds>(
    endTime - startTime
  );
  std::printf("runs:\n");
  std::printf("  - iteration_count: %" PRIu64 "\n", iter_count);
  std::printf("    duration: %" PRIu64 " us\n", totalTimeUs.count());
  return 0;
}
