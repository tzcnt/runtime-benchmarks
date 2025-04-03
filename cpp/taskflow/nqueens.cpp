// Adapted from the benchmark provided at:
// https://github.com/ConorWilliams/libfork/blob/ce40fa0f3178a43f5da8016788d6cfdadc85554f/bench/source/nqueens/libfork.cpp

// Original Copyright Notice:
// Copyright © Conor Williams <conorwilliams@outlook.com>

// SPDX-License-Identifier: MPL-2.0

// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "taskflow/core/flow_builder.hpp"
#include <taskflow/taskflow.hpp>

#include <array>
#include <cinttypes>
#include <cstdio>
#include <functional>
#include <ranges>

static int thread_count = std::thread::hardware_concurrency() / 2;
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
void nqueens(tf::Subflow& sbf, int xMax, std::array<char, N> buf, int& out) {
  if (N == xMax) {
    out = 1;
    return;
  }

  int taskCount = 0;
  std::array<int, nqueens_work> results;
  auto tasks =
    std::ranges::views::iota(0UL, N) |
    std::ranges::views::filter([xMax, &buf, &taskCount](int y) {
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
    std::ranges::views::transform([xMax, &buf, &taskCount, &results](int y) {
      size_t idx = taskCount;
      ++taskCount;
      return [xMax, buf, idx, &results](tf::Subflow& s) {
        nqueens(s, xMax + 1, buf, results[idx]);
      };
    });

  for (auto&& t : tasks) {
    sbf.emplace(t);
  }
  sbf.join();

  int ret = 0;
  for (size_t i = 0; i < taskCount; ++i) {
    ret += results[i];
  }

  out = ret;
};

int main(int argc, char* argv[]) {
  std::printf("threads: %d\n", thread_count);
  tf::Executor executor(thread_count);
  tf::Taskflow taskflow;

  {
    std::array<char, nqueens_work> buf{};
    int result;
    taskflow.emplace([&](tf::Subflow& sbf) { nqueens(sbf, 0, buf, result); });
    executor.run(taskflow).wait();
    check_answer(result);
  }

  auto startTime = std::chrono::high_resolution_clock::now();

  for (size_t i = 0; i < iter_count; ++i) {
    std::array<char, nqueens_work> buf{};
    int result;
    taskflow.emplace([&](tf::Subflow& sbf) { nqueens(sbf, 0, buf, result); });
    executor.run(taskflow).wait();
    check_answer(result);
    std::printf("  - %d\n", result);
  }

  auto endTime = std::chrono::high_resolution_clock::now();
  auto totalTimeUs =
    std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
  std::printf("runs:\n");
  std::printf("  - iteration_count: %" PRIu64 "\n", iter_count);
  std::printf("    duration: %" PRIu64 " us\n", totalTimeUs.count());
}
