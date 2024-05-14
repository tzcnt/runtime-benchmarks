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

void check_answer(int result) {
  if (result != answers[nqueens_work]) {
    std::printf("error: expected %d, got %d\n", answers[nqueens_work], result);
  }
}

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
nqueens(executor_tag, std::shared_ptr<thread_pool_executor> executor, int xMax, std::array<char, N> buf) {
  if (N == xMax) {
    co_return 1;
  }

  for (int x = 0; x < xMax; x++) {
    char p = buf[x];
    for (int j = x + 1; j < xMax; j++) {
      if (char q = buf[j]; q == p || q == p - (j - x) || q == p + (j - x)) {
        co_return 0;
      }
    }
  }

  auto tasks = std::ranges::views::iota(0UL, N) |
                std::ranges::views::filter([xMax, &buf](int y) {
                  buf[xMax] = y;
                  char q = y;
                  for (int x = 0; x < xMax; x++) {
                    char p = buf[x];
                    if (q == p || q == p - (xMax - x) || q == p + (xMax - x)) {
                      return false;
                    }
                  }
                  return true;
                }) |
                std::ranges::views::transform([xMax, executor, &buf](int y) {
                  return nqueens({}, executor, xMax + 1, buf);
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
    check_answer(result);
  }
  
  std::printf("results:\n");
  auto startTime = std::chrono::high_resolution_clock::now();

  for (size_t i = 0; i < iter_count; ++i) {
    std::array<char, nqueens_work> buf{};
    auto result = nqueens({}, runtime.thread_pool_executor(), 0, buf).get();
    check_answer(result);
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
