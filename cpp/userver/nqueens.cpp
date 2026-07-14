// Recursive fork-join nqueens.
//
// Canonical (TooManyCooks) implementation: ../TooManyCooks/nqueens.cpp
// which was in turn adapted from libfork's benchmark.
//
// At each column, fork a child task for every valid queen placement. Each
// fork is an engine task (userver's fork-join primitive) joined via
// TaskWithResult::Get().

#include "memusage.hpp"
#include "userver_bench.hpp"

#include <userver/engine/async.hpp>
#include <userver/engine/task/task_with_result.hpp>

#include <array>
#include <chrono>
#include <cinttypes>
#include <cstdio>
#include <cstdlib>
#include <vector>

namespace engine = userver::engine;

inline constexpr int nqueens_work = 14;
static const std::size_t iter_count = 1;

inline constexpr std::array<int, 19> answers = {
  0,       1,         0,          0,          2,           10,     4,
  40,      92,        352,        724,        2'680,       14'200, 73'712,
  365'596, 2'279'184, 14'772'512, 95'815'104, 666'090'624,
};

static void check_answer(int result) {
  if (result != answers[nqueens_work]) {
    std::printf("error: expected %d, got %d\n", answers[nqueens_work], result);
  }
}

using Board = std::array<char, nqueens_work>;

static int nqueens(int xMax, Board buf) {
  if (xMax == nqueens_work) {
    return 1;
  }

  int ret = 0;
  std::vector<engine::TaskWithResult<int>> tasks;
  tasks.reserve(nqueens_work);
  for (int y = 0; y < nqueens_work; ++y) {
    const char q = static_cast<char>(y);
    bool valid = true;
    for (int x = 0; x < xMax; ++x) {
      const char p = buf[x];
      if (q == p || q == p - (xMax - x) || q == p + (xMax - x)) {
        valid = false;
        break;
      }
    }
    if (!valid) {
      continue;
    }
    Board next = buf;
    next[xMax] = q;
    tasks.push_back(engine::AsyncNoTracing(
      [xMax, next] { return nqueens(xMax + 1, next); }
    ));
  }

  for (auto& task : tasks) {
    ret += task.Get();
  }
  return ret;
}

int main(int argc, char* argv[]) {
  std::size_t thread_count = bench::default_thread_count();
  if (argc > 1) {
    thread_count = static_cast<std::size_t>(atoi(argv[1]));
  }
  std::printf("threads: %zu\n", thread_count);

  engine::RunStandalone(thread_count, bench::pools_config(true), [] {
    {
      Board buf{};
      check_answer(nqueens(0, buf)); // warmup
    }

    auto startTime = std::chrono::high_resolution_clock::now();
    int result = 0;
    for (std::size_t i = 0; i < iter_count; ++i) {
      Board buf{};
      result = nqueens(0, buf);
      check_answer(result);
    }
    auto endTime = std::chrono::high_resolution_clock::now();
    auto totalTimeUs = std::chrono::duration_cast<std::chrono::microseconds>(
      endTime - startTime
    );
    std::printf("output: %d\n", result);
    std::printf("runs:\n");
    std::printf("  - iteration_count: %zu\n", iter_count);
    std::printf("    duration: %" PRIu64 " us\n",
                static_cast<std::uint64_t>(totalTimeUs.count()));
    std::printf("    max_rss: %ld KiB\n", peak_memory_usage());
  });
}
