// Adapted from the benchmark provided at:
// https://github.com/ConorWilliams/libfork/blob/c53c13102e30e8a68d5a9200ff90ad8d4b239520/bench/source/fib/libfork.cpp

// Original Copyright Notice:
// Copyright © Conor Williams <conorwilliams@outlook.com>

// SPDX-License-Identifier: MPL-2.0

// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include <cinttypes>
#include <cstdio>
#include <cstdlib>
#include <libfork.hpp>

static size_t thread_count = std::thread::hardware_concurrency() / 2;
static const size_t iter_count = 1;

inline constexpr auto fib = [](auto fib, size_t n) -> lf::task<size_t> {
  if (n < 2) {
    co_return n;
  }

  size_t x, y;

  co_await lf::fork[&x, fib](n - 1);
  co_await lf::call[&y, fib](n - 2);

  co_await lf::join;

  co_return x + y;
};

int main(int argc, char* argv[]) {
  if (argc > 2) {
    thread_count = static_cast<size_t>(atoi(argv[2]));
  }
  if (argc < 2) {
    printf("Usage: fib <n-th fibonacci number requested>\n");
    exit(0);
  }
  size_t n = static_cast<size_t>(atoi(argv[1]));

  std::printf("threads: %" PRIu64 "\n", thread_count);
  lf::lazy_pool pool(thread_count);

  auto result = lf::sync_wait(pool, fib, 30); // warmup

  auto startTime = std::chrono::high_resolution_clock::now();

  for (size_t i = 0; i < iter_count; ++i) {
    auto result = lf::sync_wait(pool, fib, n);
    std::printf("output: %zu\n", result);
  }

  auto endTime = std::chrono::high_resolution_clock::now();
  auto totalTimeUs =
    std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
  std::printf("runs:\n");
  std::printf("  - iteration_count: %" PRIu64 "\n", iter_count);
  std::printf("    duration: %" PRIu64 " us\n", totalTimeUs.count());
  return 0;
}
