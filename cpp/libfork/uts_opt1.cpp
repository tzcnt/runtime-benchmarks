// Adapted from the benchmark provided at:
// https://github.com/ConorWilliams/libfork/blob/f91d6ef23c74069b6591128ed7736d6feb73b8e7/bench/source/uts/libfork.cpp
// opt1: pass Node by value so each child Node rides in the callee coroutine's
// frame instead of a heap-allocated backing vector.
//
// The libfork baseline performs one caller-side heap allocation per internal
// node: a std::vector<pair> holding, for each child, both its Node (kept alive
// while the group runs) and the slot the child writes its result into. libfork
// already allocates a coroutine frame per spawned task, so a child Node passed
// by value rides in that frame for free -- the vector no longer needs to carry
// Nodes and shrinks to a std::vector<result>.
//
// One allocation remains (the result vector); opt2 removes it via a stack array.

// Original Copyright Notice:
// Copyright © Conor Williams <conorwilliams@outlook.com>

// SPDX-License-Identifier: MPL-2.0

// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

// The Unbalanced Tree Search (UTS) tree-generation code (2common/uts/external/*)
// is a collaborative project licensed under the MIT Open Source license. See the
// AUTHORS/LICENSE files in the original UTS distribution for details.

#include <chrono>
#include <cinttypes>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <thread>
#include <vector>
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

    // Only the results need to outlive the join now; each child Node is built on
    // the stack in the loop and passed by value into the child coroutine frame.
    std::vector<result> res(num_children);

    for (int i = 0; i < num_children; i++) {

      Node child;
      child.type = child_type;
      child.height = parent.height + 1;
      child.numChildren = -1; // not yet determined

      for (int j = 0; j < computeGranularity; j++) {
        rng_spawn(parent.state.state, child.state.state, i);
      }

      if (i + 1 == num_children) {
        co_await lf::call[&res[i], uts](depth + 1, child);
      } else {
        co_await lf::fork[&res[i], uts](depth + 1, child);
      }
    }

    co_await lf::join;

    for (auto&& elem : res) {
      r.maxdepth = max(r.maxdepth, elem.maxdepth);
      r.size += elem.size;
      r.leaves += elem.leaves;
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
