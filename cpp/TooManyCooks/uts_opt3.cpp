// Adapted from the benchmark provided at:
// https://github.com/ConorWilliams/libfork/blob/f91d6ef23c74069b6591128ed7736d6feb73b8e7/bench/source/uts/tmc.cpp
// Original author: tzcnt
// Unlicense License
// This is free and unencumbered software released into the public domain.

// The Unbalanced Tree Search (UTS) tree-generation code
// (2common/uts/external/*) is a collaborative project licensed under the MIT
// Open Source license. See the AUTHORS/LICENSE files in the original UTS
// distribution for details.

// opt3: write results by reference instead of returning them.
//
// Builds on opt2 (bounded fixed-size, allocation-free dispatch). opt2 has each
// child task `co_return` its result, which spawn_many<N> gathers into a stack
// std::array<result, N>. A coroutine has no RVO: `co_return r` move-constructs r
// into the promise, and spawn_many then moves it again out of the promise into
// the result array -- two moves of a `result` per child.
//
// Here `uts` returns `tmc::task<void>` and takes a `result*` output slot. The
// array is created *before* the spawn and each child is handed a reference to
// its own element, writing its result directly into place (`*out = r`). The
// task carries no return value, so spawn_many<N> just joins. This mirrors how
// citor (`res[i] = uts(...)`) and libfork (`lf::fork[&res[i], uts]`) thread
// results into caller-provided storage under the hood.

#include "memusage.hpp"
#include "tmc/all_headers.hpp"

#include <array>
#include <chrono>
#include <cinttypes>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <ranges>
#include <thread>

// uts.hpp pulls in uts.h, which defines function-like `max`/`min` macros;
// include it last so those macros can't clobber the standard or runtime
// headers.
#include "uts.hpp"

static size_t thread_count = std::thread::hardware_concurrency() / 2;
static const size_t iter_count = 1;

static tmc::task<void> uts(int depth, Node parent, result* out) {
  result r(depth, 1, 0);

  int num_children = uts_numChildren(&parent);
  int child_type = uts_childType(&parent);

  parent.numChildren = num_children;

  if (num_children > 0) {
    constexpr int N = 9;

    // Result buffer created up front; each child writes its own slot by
    // reference (see the per-child `&res[i - base]` below), so no result is
    // moved back out through a coroutine promise.
    std::array<result, N> res;

    // Start index of the current batch within [0, num_children). Children are
    // pulled from the lazy range in order, so the child at global index i lands
    // in slot (i - base) of the current batch's array.
    int base = 0;

    auto children = std::ranges::views::iota(0, num_children) |
                    std::ranges::views::transform([&](int i) {
                      Node child;
                      child.type = child_type;
                      child.height = parent.height + 1;
                      child.numChildren = -1; // not yet determined
                      for (int j = 0; j < computeGranularity; j++) {
                        rng_spawn(parent.state.state, child.state.state, i);
                      }
                      // spawn_many<N> dereferences (and thus constructs) every
                      // task in a batch synchronously, before the co_await
                      // suspends and before we advance `base`, so `i - base` is
                      // the correct slot at deref time.
                      return uts(depth + 1, child, &res[i - base]);
                    });

    auto it = children.begin();
    auto end = children.end();

    for (int remaining = num_children; remaining > 0;) {
      int batch = remaining < N ? remaining : N;

      // Void tasks: spawn_many<N> just joins; results are already in `res`.
      co_await tmc::spawn_many<N>(it, end);

      for (int k = 0; k < batch; k++) {
        r.maxdepth = max(r.maxdepth, res[k].maxdepth);
        r.size += res[k].size;
        r.leaves += res[k].leaves;
      }

      it += batch;
      remaining -= batch;
      base += batch;
    }
  } else {
    r.leaves = 1;
  }
  *out = r;
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
      result out;
      co_await uts(0, root, &out); // warmup
      if (out != result_tree(tree_id)) {
        std::cerr << "uts " << tree_id << " failed" << std::endl;
      }
    }

    auto startTime = std::chrono::high_resolution_clock::now();

    for (size_t i = 0; i < iter_count; ++i) {
      uts_initRoot(&root, type);
      result out;
      co_await uts(0, root, &out);
      if (out != result_tree(tree_id)) {
        std::cerr << "uts " << tree_id << " failed" << std::endl;
      }
      std::printf("output: %llu\n", out.size);
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
