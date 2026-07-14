// Adapted from the benchmark provided at:
// https://github.com/ConorWilliams/libfork/blob/ce40fa0f3178a43f5da8016788d6cfdadc85554f/bench/source/nqueens/libfork.cpp

// Original Copyright Notice:
// Copyright © Conor Williams <conorwilliams@outlook.com>

// SPDX-License-Identifier: MPL-2.0

// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "memusage.hpp"
#include "pool_runner.hpp"

#include <boost/capy/when_all.hpp>

#include <array>
#include <cinttypes>
#include <cstdio>
#include <cstdlib>
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
capy::io_task<int> nqueens(int xMax, std::array<char, N> buf) {
  if (N == static_cast<size_t>(xMax)) {
    co_return capy::io_result<int>{{}, 1};
  }

  // Build the list of valid child placements. Using capy::when_all requires a
  // non-empty range, so a fully-pruned node returns 0 directly.
  std::vector<capy::io_task<int>> tasks;
  for (int y = 0; y < static_cast<int>(N); ++y) {
    char q = static_cast<char>(y);
    bool ok = true;
    for (int x = 0; x < xMax; x++) {
      char p = buf[x];
      if (q == p || q == p - (xMax - x) || q == p + (xMax - x)) {
        ok = false;
        break;
      }
    }
    if (ok) {
      auto next = buf;
      next[xMax] = q;
      tasks.push_back(nqueens<N>(xMax + 1, next));
    }
  }

  if (tasks.empty()) {
    co_return capy::io_result<int>{{}, 0};
  }

  auto [ec, values] = co_await capy::when_all(std::move(tasks));

  int ret = 0;
  for (size_t i = 0; i < values.size(); ++i) {
    ret += values[i];
  }

  co_return capy::io_result<int>{{}, ret};
}

int main(int argc, char* argv[]) {
  if (argc > 1) {
    thread_count = static_cast<size_t>(atoi(argv[1]));
  }
  std::printf("threads: %" PRIu64 "\n", thread_count);
  capy::thread_pool pool(thread_count);
  auto ex = pool.get_executor();

  {
    std::array<char, nqueens_work> buf{};
    auto result = run_on_pool<int>(ex, nqueens(0, buf)); // warmup
    check_answer(result);
  }

  auto startTime = std::chrono::high_resolution_clock::now();

  for (size_t i = 0; i < iter_count; ++i) {
    std::array<char, nqueens_work> buf{};
    auto result = run_on_pool<int>(ex, nqueens(0, buf));
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
  return 0;
}
