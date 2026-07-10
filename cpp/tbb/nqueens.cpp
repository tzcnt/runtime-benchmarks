// Adapted from the benchmark provided at:
// https://github.com/ConorWilliams/libfork/blob/ce40fa0f3178a43f5da8016788d6cfdadc85554f/bench/source/nqueens/libfork.cpp

// Original Copyright Notice:
// Copyright © Conor Williams <conorwilliams@outlook.com>

// SPDX-License-Identifier: MPL-2.0

// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "memusage.hpp"
#include <tbb/tbb.h>

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

template <size_t N> void nqueens(int xMax, std::array<char, N> buf, int& out) {
  if (N == xMax) {
    out = 1;
    return;
  }

  // Materialize the legal child positions up front so the last one can be run
  // inline via run_and_wait rather than spawned.
  std::array<char, N> ys;
  size_t taskCount = 0;
  for (int y = 0; y < static_cast<int>(N); ++y) {
    char q = static_cast<char>(y);
    bool legal = true;
    for (int x = 0; x < xMax; ++x) {
      char p = buf[x];
      if (q == p || q == p - (xMax - x) || q == p + (xMax - x)) {
        legal = false;
        break;
      }
    }
    if (legal) {
      ys[taskCount++] = static_cast<char>(y);
    }
  }

  if (taskCount == 0) {
    out = 0;
    return;
  }

  std::array<int, N> results;
  tbb::task_group tg;
  // Spawn all children except the last; run the last inline via run_and_wait.
  for (size_t i = 0; i + 1 < taskCount; ++i) {
    buf[xMax] = ys[i];
    tg.run([xMax, buf, i, &results]() { nqueens(xMax + 1, buf, results[i]); });
  }
  {
    size_t last = taskCount - 1;
    buf[xMax] = ys[last];
    tg.run_and_wait([xMax, buf, last, &results]() {
      nqueens(xMax + 1, buf, results[last]);
    });
  }

  int ret = 0;
  for (size_t i = 0; i < taskCount; ++i) {
    ret += results[i];
  }

  out = ret;
};

int main(int argc, char* argv[]) {
  if (argc > 1) {
    thread_count = static_cast<size_t>(atoi(argv[1]));
  }
  std::printf("threads: %zu\n", thread_count);
  tbb::task_arena arena(thread_count);

  {
    std::array<char, nqueens_work> buf{};
    int result;
    arena.execute([&]() { nqueens(0, buf, result); });
    check_answer(result);
  }

  auto startTime = std::chrono::high_resolution_clock::now();

  for (size_t i = 0; i < iter_count; ++i) {
    std::array<char, nqueens_work> buf{};
    int result;
    arena.execute([&]() { nqueens(0, buf, result); });
    check_answer(result);
    std::printf("output: %d\n", result);
  }

  auto endTime = std::chrono::high_resolution_clock::now();
  auto totalTimeUs =
    std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
  std::printf("runs:\n");
  std::printf("  - iteration_count: %" PRIu64 "\n", iter_count);
  std::printf("    duration: %" PRIu64 " us\n", totalTimeUs.count());
  std::printf("    max_rss: %ld KiB\n", peak_memory_usage());
}
