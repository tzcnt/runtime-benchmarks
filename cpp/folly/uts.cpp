// Adapted from the benchmark provided at:
// https://github.com/ConorWilliams/libfork/blob/f91d6ef23c74069b6591128ed7736d6feb73b8e7/bench/source/uts/libfork.cpp

// Original Copyright Notice:
// Copyright © Conor Williams <conorwilliams@outlook.com>

// SPDX-License-Identifier: MPL-2.0

// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

// The Unbalanced Tree Search (UTS) tree-generation code (2common/uts/external/*)
// is a collaborative project licensed under the MIT Open Source license. See the
// AUTHORS/LICENSE files in the original UTS distribution for details.

#include "memusage.hpp"

#include <folly/coro/BlockingWait.h>
#include <folly/coro/Collect.h>
#include <folly/coro/Task.h>
#include <folly/executors/CPUThreadPoolExecutor.h>

#include <chrono>
#include <cinttypes>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <thread>
#include <vector>

// uts.hpp pulls in uts.h, which defines function-like `max`/`min` macros;
// include it after the runtime headers so those macros can't clobber them.
#include "uts.hpp"

static size_t thread_count = std::thread::hardware_concurrency() / 2;
static const size_t iter_count = 1;

static folly::coro::Task<result> uts(int depth, Node* parent) {
  result r(depth, 1, 0);

  int num_children = uts_numChildren(parent);
  int child_type = uts_childType(parent);

  parent->numChildren = num_children;

  if (num_children > 0) {

    std::vector<Node> cs(num_children);

    for (int i = 0; i < num_children; i++) {
      cs[i].type = child_type;
      cs[i].height = parent->height + 1;
      cs[i].numChildren = -1; // not yet determined

      for (int j = 0; j < computeGranularity; j++) {
        rng_spawn(parent->state.state, cs[i].state.state, i);
      }
    }

    std::vector<folly::coro::Task<result>> tasks;
    tasks.reserve(num_children);
    for (int i = 0; i < num_children; i++) {
      tasks.push_back(uts(depth + 1, &cs[i]));
    }

    auto res = co_await folly::coro::collectAllRange(std::move(tasks));

    for (auto&& elem : res) {
      r.maxdepth = max(r.maxdepth, elem.maxdepth);
      r.size += elem.size;
      r.leaves += elem.leaves;
    }
  } else {
    r.leaves = 1;
  }
  co_return r;
}

int main(int argc, char* argv[]) {
  if (argc > 1) {
    thread_count = static_cast<size_t>(atoi(argv[1]));
  }
  std::printf("threads: %zu\n", thread_count);
  folly::CPUThreadPoolExecutor executor(thread_count);

  setup_tree(tree_id);

  Node root;

  {
    uts_initRoot(&root, type);
    auto result = folly::coro::blockingWait(
      co_withExecutor(&executor, uts(0, &root))); // warmup
    if (result != result_tree(tree_id)) {
      std::cerr << "uts " << tree_id << " failed" << std::endl;
    }
  }

  auto startTime = std::chrono::high_resolution_clock::now();

  for (size_t i = 0; i < iter_count; ++i) {
    uts_initRoot(&root, type);
    auto result =
      folly::coro::blockingWait(co_withExecutor(&executor, uts(0, &root)));
    if (result != result_tree(tree_id)) {
      std::cerr << "uts " << tree_id << " failed" << std::endl;
    }
    std::printf("output: %llu\n", result.size);
  }

  auto endTime = std::chrono::high_resolution_clock::now();
  auto totalTimeUs =
    std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
  std::printf("runs:\n");
  std::printf("  - iteration_count: %zu\n", iter_count);
  std::printf("    duration: %" PRIu64 " us\n",
              static_cast<uint64_t>(totalTimeUs.count()));
  std::printf("    max_rss: %ld KiB\n", peak_memory_usage());
  return 0;
}
