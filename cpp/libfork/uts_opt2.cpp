// Adapted from the benchmark provided at:
// https://github.com/ConorWilliams/libfork/blob/f91d6ef23c74069b6591128ed7736d6feb73b8e7/bench/source/uts/libfork.cpp
// opt2: bounded fixed-size dispatch -- drive per-internal-node heap allocations
// to zero.
//
// Builds on opt1 (Node by value removes the Node storage from the caller-side
// vector). The remaining allocation is the std::vector<result> that the forked
// children write into. Here it is replaced by a stack std::array<result, N>
// living in the coroutine frame (already allocated), and children are forked in
// fixed-size batches of N so the results always fit.
//
// Most nodes have < N children (P99 ~= 20), so they complete in one batch.
// Wider nodes loop: each batch forks up to N children, joins, then reduces.
//
// N balances two costs: the std::array<result, N> inflates every internal
// node's coroutine frame regardless of its actual fan-out (larger N wastes
// frame space), while a too-small N forces wide nodes into multiple sequential
// fork/join rounds. N=9 was the TMC optimum (see
// ../TooManyCooks/uts_optimizations.md); reused here as a starting point.

// Original Copyright Notice:
// Copyright © Conor Williams <conorwilliams@outlook.com>

// SPDX-License-Identifier: MPL-2.0

// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

// The Unbalanced Tree Search (UTS) tree-generation code (2common/uts/external/*)
// is a collaborative project licensed under the MIT Open Source license. See the
// AUTHORS/LICENSE files in the original UTS distribution for details.

#include <array>
#include <chrono>
#include <cinttypes>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <thread>
#include "memusage.hpp"
#include <libfork.hpp>
// uts.hpp pulls in uts.h, which defines function-like `max`/`min` macros;
// include it after the runtime headers so those macros can't clobber them.
#include "uts.hpp"

static size_t thread_count = std::thread::hardware_concurrency() / 2;
static const size_t iter_count = 1;

inline constexpr auto uts =
  [](auto uts, int depth, Node parent) LF_STATIC_CALL -> lf::task<result> {
  result r(depth, 1, 0);

  int num_children = uts_numChildren(&parent);
  int child_type = uts_childType(&parent);

  parent.numChildren = num_children;

  if (num_children > 0) {

    constexpr int N = 9;

    // Stack result buffer (in the coroutine frame) reused across batches. On a
    // short final batch we only read [0, batch); the tail is default-init.
    std::array<result, N> res;

    for (int base = 0; base < num_children; base += N) {
      int batch = num_children - base < N ? num_children - base : N;

      for (int k = 0; k < batch; k++) {
        int i = base + k;

        Node child;
        child.type = child_type;
        child.height = parent.height + 1;
        child.numChildren = -1; // not yet determined

        for (int j = 0; j < computeGranularity; j++) {
          rng_spawn(parent.state.state, child.state.state, i);
        }

        if (k + 1 == batch) {
          co_await lf::call[&res[k], uts](depth + 1, child);
        } else {
          co_await lf::fork[&res[k], uts](depth + 1, child);
        }
      }

      co_await lf::join;

      for (int k = 0; k < batch; k++) {
        r.maxdepth = max(r.maxdepth, res[k].maxdepth);
        r.size += res[k].size;
        r.leaves += res[k].leaves;
      }
    }
  } else {
    r.leaves = 1;
  }
  co_return r;
};

int main(int argc, char* argv[]) {
  if (argc > 1) {
    thread_count = static_cast<size_t>(atoi(argv[1]));
  }
  std::printf("threads: %" PRIu64 "\n", thread_count);
  lf::lazy_pool pool(thread_count);

  setup_tree(tree_id);

  Node root;

  {
    uts_initRoot(&root, type);
    auto result = lf::sync_wait(pool, uts, 0, root); // warmup
    if (result != result_tree(tree_id)) {
      std::cerr << "uts " << tree_id << " failed" << std::endl;
    }
  }

  auto startTime = std::chrono::high_resolution_clock::now();

  for (size_t i = 0; i < iter_count; ++i) {
    uts_initRoot(&root, type);
    auto result = lf::sync_wait(pool, uts, 0, root);
    if (result != result_tree(tree_id)) {
      std::cerr << "uts " << tree_id << " failed" << std::endl;
    }
    std::printf("output: %llu\n", result.size);
  }

  auto endTime = std::chrono::high_resolution_clock::now();
  auto totalTimeUs =
    std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
  std::printf("runs:\n");
  std::printf("  - iteration_count: %" PRIu64 "\n", iter_count);
  std::printf("    duration: %" PRIu64 " us\n", totalTimeUs.count());
  std::printf("    max_rss: %ld KiB\n", peak_memory_usage());
  return 0;
}
