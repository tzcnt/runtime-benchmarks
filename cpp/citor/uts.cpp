// Port of cpp/libfork/uts.cpp using citor::forkJoinAll.

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

static result uts(citor::ThreadPool& pool, int depth, Node* parent) {
  result r(depth, 1, 0);

  int num_children = uts_numChildren(parent);
  int child_type = uts_childType(parent);

  parent->numChildren = num_children;

  if (num_children > 0) {

    std::vector<Node> cs(num_children);
    std::vector<result> res(num_children);

    for (int i = 0; i < num_children; i++) {
      cs[i].type = child_type;
      cs[i].height = parent->height + 1;
      cs[i].numChildren = -1; // not yet determined

      for (int j = 0; j < computeGranularity; j++) {
        rng_spawn(parent->state.state, cs[i].state.state, i);
      }
    }

    pool.forkJoinAll<citor::HintsDefaults>(num_children, [&](size_t i) {
      res[i] = uts(pool, depth + 1, &cs[i]);
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
    auto result = uts(pool, 0, &root); // warmup
    if (result != result_tree(tree_id)) {
      std::cerr << "uts " << tree_id << " failed" << std::endl;
    }
  }

  auto startTime = std::chrono::high_resolution_clock::now();

  for (size_t i = 0; i < iter_count; ++i) {
    uts_initRoot(&root, type);
    auto result = uts(pool, 0, &root);
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
