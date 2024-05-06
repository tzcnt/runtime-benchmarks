// Adapted from the benchmark provided at:
// https://github.com/ConorWilliams/libfork/blob/ce40fa0f3178a43f5da8016788d6cfdadc85554f/bench/source/nqueens/libfork.cpp

// Original Copyright Notice:
// Copyright © Conor Williams <conorwilliams@outlook.com>

// SPDX-License-Identifier: MPL-2.0

// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include <libfork.hpp>
#include <cstdio>
#include <cinttypes>
#include <array>

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

constexpr auto nqueens = []<std::size_t N>(auto nqueens, int j, std::array<char, N> const &a)
                             LF_STATIC_CALL -> lf::task<int> {
  if (N == j) {
    co_return 1;
  }

  std::array<std::array<char, N>, N> buf;
  std::array<int, N> parts;

  for (int i = 0; i < N; i++) {
    for (int k = 0; k < j; k++) {
      buf[i][k] = a[k];
    }

    buf[i][j] = i;

    if (queens_ok(j + 1, buf[i].data())) {
      co_await lf::fork[&parts[i], nqueens](j + 1, buf[i]);
    } else {
      parts[i] = 0;
    }
  }

  co_await lf::join;

  int ret = 0;
  for (auto p : parts) {
    ret += p;
  }

  co_return ret;
};


int main(int argc, char* argv[]) {
  std::printf("threads: %" PRIu64 "\n", thread_count);
  lf::lazy_pool pool(thread_count);
  {
    std::array<char, nqueens_work> buf{};
    auto result = lf::sync_wait(pool, nqueens, 0, buf); // warmup
  }
  
  std::printf("results:\n");
  auto startTime = std::chrono::high_resolution_clock::now();

  for (size_t i = 0; i < iter_count; ++i) {

    std::array<char, nqueens_work> buf{};
    auto result = lf::sync_wait(pool, nqueens, 0, buf);
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