// Port of cpp/citor/uts.cpp using citor::forkJoinAll.
// opt1: pass Node by value so each child Node is built on the worker stack
// inside the fork-join body instead of in a heap-allocated backing vector.
//
// The citor baseline performs two caller-side heap allocations per internal
// node: a std::vector<Node> holding the child nodes (kept alive while the group
// runs) and a std::vector<result> collecting the per-child results. citor is
// synchronous (no coroutine frame), so a child Node need only live for the
// duration of its recursive uts() call -- it can be constructed locally inside
// the body lambda and passed by value, removing the std::vector<Node>.
//
// The std::vector<result> remains here (eliminated in opt2). forkJoinAll is
// still given a runtime count; its internal task/closure buffers are stack-
// resident for fan-out <= 32 (citor's kStackTaskBudget) and only spill to the
// heap for wider nodes.

// The Unbalanced Tree Search (UTS) tree-generation code (2common/uts/external/*)
// is a collaborative project licensed under the MIT Open Source license. See the
// AUTHORS/LICENSE files in the original UTS distribution for details.

#include "memusage.hpp"
#include "citor/thread_pool.h"
#include "citor/hints.h"

#include <chrono>
#include <cinttypes>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <thread>
#include <vector>

// uts.hpp pulls in uts.h, which defines function-like `max`/`min` macros;
// include it last so those macros can't clobber the standard or runtime headers.
#include "uts.hpp"

static size_t thread_count = std::thread::hardware_concurrency() / 2;
static const size_t iter_count = 1;

static result uts(citor::ThreadPool& pool, int depth, Node parent) {
  result r(depth, 1, 0);

  int num_children = uts_numChildren(&parent);
  int child_type = uts_childType(&parent);

  parent.numChildren = num_children;

  if (num_children > 0) {

    std::vector<result> res(num_children);

    // Each child Node is constructed on the worker stack inside the body and
    // passed by value into the recursive uts() call; no backing vector needed.
    // parent is read-only during the parallel section (rng_spawn only reads its
    // state), so capturing it by reference across workers is safe.
    pool.forkJoinAll<citor::HintsDefaults>(num_children, [&](size_t i) {
      Node child;
      child.type = child_type;
      child.height = parent.height + 1;
      child.numChildren = -1; // not yet determined

      for (int j = 0; j < computeGranularity; j++) {
        rng_spawn(parent.state.state, child.state.state, i);
      }

      res[i] = uts(pool, depth + 1, child);
    });

    for (auto&& elem : res) {
      r.maxdepth = max(r.maxdepth, elem.maxdepth);
      r.size += elem.size;
      r.leaves += elem.leaves;
    }
  } else {
    r.leaves = 1;
  }
  return r;
}

int main(int argc, char* argv[]) {
  if (argc > 1) {
    thread_count = static_cast<size_t>(atoi(argv[1]));
  }
  std::printf("threads: %zu\n", thread_count);
  // citor's default PerCpu affinity caps workers at the physical-core
  // count. When the sweep requests every logical CPU, opt into
  // SMT-sibling placement so all hardware threads are used.
  const citor::Affinity affinity =
      (thread_count == std::thread::hardware_concurrency())
          ? citor::Affinity::PerCpuSmtPair
          : citor::Affinity::PerCpu;
  citor::ThreadPool pool(thread_count, affinity);

  setup_tree(tree_id);

  Node root;

  {
    uts_initRoot(&root, type);
    auto result = uts(pool, 0, root); // warmup
    if (result != result_tree(tree_id)) {
      std::cerr << "uts " << tree_id << " failed" << std::endl;
    }
  }

  auto startTime = std::chrono::high_resolution_clock::now();

  for (size_t i = 0; i < iter_count; ++i) {
    uts_initRoot(&root, type);
    auto result = uts(pool, 0, root);
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
