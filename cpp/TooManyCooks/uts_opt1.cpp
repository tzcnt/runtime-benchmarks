// Adapted from the benchmark provided at:
// https://github.com/ConorWilliams/libfork/blob/f91d6ef23c74069b6591128ed7736d6feb73b8e7/bench/source/uts/tmc.cpp
// Original author: tzcnt
// Unlicense License
// This is free and unencumbered software released into the public domain.

// The Unbalanced Tree Search (UTS) tree-generation code
// (2common/uts/external/*) is a collaborative project licensed under the MIT
// Open Source license. See the AUTHORS/LICENSE files in the original UTS
// distribution for details.

#include "memusage.hpp"
#include "tmc/all_headers.hpp"

#include <chrono>
#include <cinttypes>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <ranges>
#include <thread>
#include <vector>

// uts.hpp pulls in uts.h, which defines function-like `max`/`min` macros;
// include it last so those macros can't clobber the standard or runtime
// headers.
#include "uts.hpp"

static size_t thread_count = std::thread::hardware_concurrency() / 2;
static const size_t iter_count = 1;

static tmc::task<result> uts(int depth, Node parent) {
  result r(depth, 1, 0);

  int num_children = uts_numChildren(&parent);
  int child_type = uts_childType(&parent);

  parent.numChildren = num_children;

  if (num_children > 0) {

    // Construct and dispatch each child entirely within the range. Because uts
    // takes its Node by value, each child Node lives in the callee coroutine's
    // frame, so no backing std::vector is needed to keep child Nodes alive.
    auto children = std::ranges::views::iota(0, num_children) |
                    std::ranges::views::transform([&](int i) {
                      Node child;
                      child.type = child_type;
                      child.height = parent.height + 1;
                      child.numChildren = -1; // not yet determined
                      for (int j = 0; j < computeGranularity; j++) {
                        rng_spawn(parent.state.state, child.state.state, i);
                      }
                      return uts(depth + 1, child);
                    });

    auto results = co_await tmc::spawn_many(children);

    for (auto&& res : results) {
      r.maxdepth = max(r.maxdepth, res.maxdepth);
      r.size += res.size;
      r.leaves += res.leaves;
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
  std::printf("threads: %" PRIu64 "\n", thread_count);
  tmc::cpu_executor()
    .set_thread_count(thread_count)
    .set_thread_pinning_level(tmc::topology::thread_pinning_level::CORE)
    .set_spins(0)
    .init();

  setup_tree(tree_id);

  return tmc::async_main([]() -> tmc::task<int> {
    Node root;

    {
      uts_initRoot(&root, type);
      auto result = co_await uts(0, root); // warmup
      if (result != result_tree(tree_id)) {
        std::cerr << "uts " << tree_id << " failed" << std::endl;
      }
    }

    auto startTime = std::chrono::high_resolution_clock::now();

    for (size_t i = 0; i < iter_count; ++i) {
      uts_initRoot(&root, type);
      auto result = co_await uts(0, root);
      if (result != result_tree(tree_id)) {
        std::cerr << "uts " << tree_id << " failed" << std::endl;
      }
      std::printf("output: %llu\n", result.size);
    }

    auto endTime = std::chrono::high_resolution_clock::now();
    auto totalTimeUs = std::chrono::duration_cast<std::chrono::microseconds>(
      endTime - startTime
    );
    std::printf("runs:\n");
    std::printf("  - iteration_count: %" PRIu64 "\n", iter_count);
    std::printf("    duration: %" PRIu64 " us\n", totalTimeUs.count());
    std::printf("    max_rss: %ld KiB\n", peak_memory_usage());
    co_return 0;
  }());
}
