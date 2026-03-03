// CodSpeed benchmark wrapper for the nqueens test
// using the taskflow runtime.

// Adapted from cpp/taskflow/nqueens.cpp

#include <taskflow/taskflow.hpp>

#include <array>
#include <benchmark/benchmark.h>
#include <cstdlib>
#include <optional>
#include <ranges>
#include <thread>

static size_t thread_count = std::thread::hardware_concurrency();
std::optional<tf::Executor> executor;

inline constexpr int nqueens_work = 14;

inline constexpr std::array<int, 28> answers = {
  0,       1,         0,          0,          2,           10,     4,
  40,      92,        352,        724,        2'680,       14'200, 73'712,
  365'596, 2'279'184, 14'772'512, 95'815'104, 666'090'624,
};

template <size_t N>
void nqueens(int xMax, std::array<char, N> buf, int& out) {
  if (N == xMax) {
    out = 1;
    return;
  }

  size_t taskCount = 0;
  std::array<int, nqueens_work> results;
  auto tasks =
    std::ranges::views::iota(0UL, N) |
    std::ranges::views::filter(
      [xMax, &buf, &taskCount](int y) {
        char q = y;
        for (int x = 0; x < xMax; x++) {
          char p = buf[x];
          if (q == p || q == p - (xMax - x) ||
              q == p + (xMax - x)) {
            return false;
          }
        }
        return true;
      }
    ) |
    std::ranges::views::transform(
      [xMax, &buf, &taskCount, &results](int y) {
        buf[xMax] = y;
        size_t idx = taskCount;
        ++taskCount;
        return [xMax, buf, idx, &results]() {
          nqueens(xMax + 1, buf, results[idx]);
        };
      }
    );

  tf::TaskGroup tg = executor->task_group();

  for (auto&& t : tasks) {
    tg.silent_async(t);
  }
  tg.corun();

  int ret = 0;
  for (size_t i = 0; i < taskCount; ++i) {
    ret += results[i];
  }

  out = ret;
}

static void BM_NQueens(benchmark::State& state) {
  if (!executor.has_value()) {
    executor.emplace(thread_count);
  }

  // warmup
  {
    std::array<char, nqueens_work> buf{};
    int result;
    executor->async([&]() { nqueens(0, buf, result); }).get();
  }

  for (auto _ : state) {
    std::array<char, nqueens_work> buf{};
    int result;
    executor->async([&]() { nqueens(0, buf, result); }).get();
    benchmark::DoNotOptimize(result);
  }
}
BENCHMARK(BM_NQueens);
BENCHMARK_MAIN();
