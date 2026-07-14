// The skynet benchmark as described here:
// https://github.com/atemerev/skynet
//
// Canonical (TooManyCooks) implementation: ../TooManyCooks/skynet.cpp
//
// Each node forks 10 children until kDepthMax levels deep (10^kDepthMax =
// 100M leaf tasks) and sums their results. Each child is an engine task
// (userver's fork-join primitive) joined via TaskWithResult::Get(), so this
// measures userver's stackful task spawn/join throughput under deep nested
// fan-out.

#include "memusage.hpp"
#include "userver_bench.hpp"

#include <userver/engine/async.hpp>
#include <userver/engine/task/task_with_result.hpp>

#include <chrono>
#include <cinttypes>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>

namespace engine = userver::engine;

static constexpr std::uint64_t kDepthMax = 8;
static const std::size_t iter_count = 1;

static std::uint64_t skynet_one(std::uint64_t base, std::uint64_t depth) {
  if (depth == kDepthMax) {
    return base;
  }
  std::uint64_t offset = 1;
  for (std::uint64_t i = 0; i < kDepthMax - depth - 1; ++i) {
    offset *= 10;
  }

  std::uint64_t count = 0;
  std::vector<engine::TaskWithResult<std::uint64_t>> children;
  children.reserve(10);
  for (std::uint64_t i = 0; i < 10; ++i) {
    const std::uint64_t childBase = base + offset * i;
    children.push_back(engine::AsyncNoTracing(
      [childBase, depth] { return skynet_one(childBase, depth + 1); }
    ));
  }

  for (auto& child : children) {
    count += child.Get();
  }
  return count;
}

static void skynet(std::uint64_t expected) {
  const std::uint64_t count = skynet_one(0, 0);
  if (count != expected) {
    std::printf("ERROR: wrong result - %" PRIu64 "\n", count);
  }
}

int main(int argc, char* argv[]) {
  std::size_t thread_count = bench::default_thread_count();
  if (argc > 1) {
    thread_count = static_cast<std::size_t>(atoi(argv[1]));
  }

  std::uint64_t leaves = 1;
  for (std::uint64_t i = 0; i < kDepthMax; ++i) {
    leaves *= 10;
  }
  const std::uint64_t expected = (leaves - 1) * leaves / 2;

  std::printf("threads: %zu\n", thread_count);

  engine::RunStandalone(thread_count, bench::pools_config(true), [expected] {
    skynet(expected); // warmup

    std::printf("runs:\n");
    auto startTime = std::chrono::high_resolution_clock::now();
    for (std::size_t i = 0; i < iter_count; ++i) {
      skynet(expected);
    }
    auto endTime = std::chrono::high_resolution_clock::now();
    auto totalTimeUs = std::chrono::duration_cast<std::chrono::microseconds>(
      endTime - startTime
    );
    std::printf("  - iteration_count: %zu\n", iter_count);
    std::printf("    duration: %" PRIu64 " us\n",
                static_cast<std::uint64_t>(totalTimeUs.count()));
    std::printf("    max_rss: %ld KiB\n", peak_memory_usage());
  });
}
