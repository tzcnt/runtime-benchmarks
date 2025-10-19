// Adapted from the benchmark provided at:
// https://github.com/ConorWilliams/libfork/blob/ce40fa0f3178a43f5da8016788d6cfdadc85554f/bench/source/nqueens/libfork.cpp

// Original Copyright Notice:
// Copyright © Conor Williams <conorwilliams@outlook.com>

// SPDX-License-Identifier: MPL-2.0

// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "hpx/async_combinators/when_all.hpp"
#include <hpx/config.hpp>
#include <hpx/future.hpp>
#include <hpx/init.hpp>

#include <array>
#include <cinttypes>
#include <cstdio>
#include <cstdlib>
#include <ranges>
#include <string>
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
hpx::future<int> nqueens(int xMax, std::array<char, N> buf) {
  if (N == xMax) {
    co_return 1;
  }

  auto ys = std::ranges::views::iota(0UL, N) |
            std::ranges::views::filter([xMax, &buf](int y) {
              char q = y;
              for (int x = 0; x < xMax; x++) {
                char p = buf[x];
                if (q == p || q == p - (xMax - x) || q == p + (xMax - x)) {
                  return false;
                }
              }
              return true;
            });

  size_t taskCount = 0;
  std::array<hpx::future<int>, nqueens_work> taskArr;
  for (auto y : ys) {
    buf[xMax] = y;
    taskArr[taskCount] = hpx::async(nqueens<nqueens_work>, xMax + 1, buf);
    ++taskCount;
  }
  auto futures =
    co_await hpx::when_all(taskArr.begin(), taskArr.begin() + taskCount);

  int ret = 0;
  for (auto& f : futures) {
    ret += co_await f;
  }

  co_return ret;
}

int hpx_main(hpx::program_options::variables_map& vm) {
  hpx::threads::set_scheduler_mode(
    hpx::threads::policies::scheduler_mode::enable_stealing |
    hpx::threads::policies::scheduler_mode::enable_stealing_numa |
    hpx::threads::policies::scheduler_mode::assign_work_thread_parent |
    // hpx::threads::policies::scheduler_mode::assign_work_round_robin |
    hpx::threads::policies::scheduler_mode::steal_after_local
  );

  {
    std::array<char, nqueens_work> buf{};
    auto result = nqueens(0, buf); // warmup
    auto r = result.get();
    check_answer(r);
  }

  auto startTime = std::chrono::high_resolution_clock::now();

  for (size_t i = 0; i < iter_count; ++i) {
    std::array<char, nqueens_work> buf{};
    auto result = nqueens(0, buf);
    auto r = result.get();
    check_answer(r);
    std::printf("output: %d\n", r);
  }

  auto endTime = std::chrono::high_resolution_clock::now();
  auto totalTimeUs =
    std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
  std::printf("runs:\n");
  std::printf("  - iteration_count: %" PRIu64 "\n", iter_count);
  std::printf("    duration: %" PRIu64 " us\n", totalTimeUs.count());

  return hpx::local::finalize();
}

int main(int argc, char* argv[]) {
  if (argc > 1) {
    thread_count = static_cast<size_t>(atoi(argv[1]));
  }
  std::printf("threads: %" PRIu64 "\n", thread_count);

  // Force HPX to use the most efficient (?) queue mode
  // in a hacky way since it only allows for command line configuration.
  std::string queue_mode("--hpx:queuing=abp-priority-lifo");
  std::array<char*, 2> fake_argv = {
    argv[0], const_cast<char*>(queue_mode.c_str())
  };

  hpx::local::init_params init_args;
  init_args.cfg = {"hpx.os_threads=" + std::to_string(thread_count)};

  return hpx::local::init(
    hpx_main, fake_argv.size(), fake_argv.data(), init_args
  );
}
