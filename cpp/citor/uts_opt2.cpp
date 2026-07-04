// Port of cpp/citor/uts.cpp using citor::forkJoinAll.
// opt2: bounded fixed-size dispatch -- drive per-internal-node heap allocations
// to zero.
//
// Builds on opt1 (Node by value removes the child-node vector). The remaining
// caller-side allocation is the std::vector<result> collecting per-child
// results. Here it is replaced by a stack std::array<result, N>, and children
// are spawned in fixed-size batches of N so the results always fit.
//
// Most nodes have < N children (P99 ~= 20), so they complete in one batch.
// Wider nodes loop over successive batches. Because each batch size is <= N <=
// 32 (citor's kStackTaskBudget), forkJoinAll's internal task/closure buffers
// also stay stack-resident: dispatch becomes fully allocation-free.
//
// Choosing N differs sharply from the TMC port. In TMC the result array lives
// in every internal node's *coroutine frame*, so a large N bloats every (heap-
// allocated) frame and small N wins. In citor the array lives on the *worker
// stack* -- it costs nothing per task -- so the only penalty that matters is
// that a too-small N splits wide nodes into multiple sequential fork/join
// rounds, serializing whole subtrees (catastrophic for the wide nodes near the
// root). N therefore wants to be large: N=32 matches citor's kStackTaskBudget,
// so any fan-out <= 32 (well past the P99 ~= 20) dispatches in a single batch
// and forkJoinAll's internal buffers also stay stack-resident. A sweep confirms
// the basin: N=9 -> 60 ms, N=16 -> 52 ms, N=32 -> 51 ms (64 threads).

// The Unbalanced Tree Search (UTS) tree-generation code (2common/uts/external/*)
// is a collaborative project licensed under the MIT Open Source license. See the
// AUTHORS/LICENSE files in the original UTS distribution for details.

#include "memusage.hpp"
#include "citor/thread_pool.h"
#include "citor/hints.h"

#include <array>
#include <chrono>
#include <cinttypes>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <thread>

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

    constexpr int N = 32;

    // Stack result buffer reused across batches. On a short final batch we only
    // read [0, batch); the unused tail is left default-constructed.
    std::array<result, N> res;

    for (int base = 0; base < num_children; base += N) {
      int batch = num_children - base < N ? num_children - base : N;

      // Each child Node is built on the worker stack inside the body and passed
      // by value; results land in the stack array `res`. parent is read-only
      // here, so capturing it by reference across workers is safe.
      pool.forkJoinAll<citor::HintsDefaults>(batch, [&](size_t k) {
        int i = base + static_cast<int>(k);

        Node child;
        child.type = child_type;
        child.height = parent.height + 1;
        child.numChildren = -1; // not yet determined

        for (int j = 0; j < computeGranularity; j++) {
          rng_spawn(parent.state.state, child.state.state, i);
        }

        res[k] = uts(pool, depth + 1, child);
      });

      for (int k = 0; k < batch; k++) {
        r.maxdepth = max(r.maxdepth, res[k].maxdepth);
        r.size += res[k].size;
        r.leaves += res[k].leaves;
      }
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
