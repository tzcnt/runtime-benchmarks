// Port of cpp/libfork/nqueens.cpp using citor::forkJoinAll.

#include "memusage.hpp"
#include "citor/thread_pool.h"
#include "citor/hints.h"

#include <array>
#include <chrono>
#include <cinttypes>
#include <cstdio>
#include <cstdlib>
#include <thread>

static size_t thread_count = std::thread::hardware_concurrency() / 2;
static const size_t iter_count = 1;

inline constexpr int nqueens_work = 14;

inline constexpr std::array<int, 28> answers = {
  0,       1,         0,          0,          2,           10,     4,
  40,      92,        352,        724,        2'680,       14'200, 73'712,
  365'596, 2'279'184, 14'772'512, 95'815'104, 666'090'624,
};

static void check_answer(int result) {
  if (result != answers[nqueens_work]) {
    std::printf("error: expected %d, got %d\n", answers[nqueens_work], result);
  }
}

template <size_t N>
int nqueens(citor::ThreadPool& pool, int xMax, std::array<char, N> buf) {
  if (xMax == static_cast<int>(N)) {
    return 1;
  }

  std::array<char, N> ys{};
  size_t taskCount = 0;
  for (int y = 0; y < static_cast<int>(N); ++y) {
    char q = static_cast<char>(y);
    bool legal = true;
    for (int x = 0; x < xMax; ++x) {
      char p = buf[x];
      if (q == p || q == p - (xMax - x) || q == p + (xMax - x)) {
        legal = false;
        break;
      }
    }
    if (legal) {
      ys[taskCount++] = static_cast<char>(y);
    }
  }

  if (taskCount == 0) {
    return 0;
  }

  std::array<int, N> values{};
  pool.forkJoinAll<citor::HintsDefaults>(taskCount, [&](size_t i) {
    auto childBuf = buf;
    childBuf[xMax] = ys[i];
    values[i] = nqueens<N>(pool, xMax + 1, childBuf);
  });

  int ret = 0;
  for (size_t i = 0; i < taskCount; ++i) {
    ret += values[i];
  }
  return ret;
}

int main(int argc, char* argv[]) {
  if (argc > 1) {
    thread_count = static_cast<size_t>(atoi(argv[1]));
  }
  std::printf("threads: %zu\n", thread_count);
  citor::ThreadPool pool(thread_count);
  {
    std::array<char, nqueens_work> buf{};
    auto result = nqueens<nqueens_work>(pool, 0, buf); // warmup
    check_answer(result);
  }

  auto startTime = std::chrono::high_resolution_clock::now();

  for (size_t i = 0; i < iter_count; ++i) {
    std::array<char, nqueens_work> buf{};
    auto result = nqueens<nqueens_work>(pool, 0, buf);
    check_answer(result);
    std::printf("output: %d\n", result);
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
