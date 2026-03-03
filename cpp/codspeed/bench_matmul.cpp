// CodSpeed benchmark wrapper for the matmul test
// using the taskflow runtime.

// Adapted from cpp/taskflow/matmul.cpp
// Uses 256x256 instead of larger sizes for reasonable CI runtimes
// under simulation mode.

#include "matmul.hpp"
#include <taskflow/algorithm/for_each.hpp>
#include <taskflow/taskflow.hpp>

#include <benchmark/benchmark.h>
#include <cstdlib>
#include <exception>
#include <optional>
#include <thread>
#include <vector>

static size_t thread_count = std::thread::hardware_concurrency();
std::optional<tf::Executor> executor;

void matmul(int* a, int* b, int* c, int n, int N) {
  if (n <= 32) {
    matmul_small(a, b, c, n, N);
  } else {
    int k = n / 2;

    tf::TaskGroup tg = executor->task_group();
    tg.silent_async([=]() { matmul(a, b, c, k, N); });
    tg.silent_async([=]() { matmul(a, b + k, c + k, k, N); });
    tg.silent_async(
      [=]() { matmul(a + k * N, b, c + k * N, k, N); }
    );
    matmul(a + k * N, b + k, c + k * N + k, k, N);
    tg.corun();

    tg.silent_async(
      [=]() { matmul(a + k, b + k * N, c, k, N); }
    );
    tg.silent_async(
      [=]() { matmul(a + k, b + k * N + k, c + k, k, N); }
    );
    tg.silent_async(
      [=]() { matmul(a + k * N + k, b + k * N, c + k * N, k, N); }
    );
    matmul(
      a + k * N + k, b + k * N + k, c + k * N + k, k, N
    );
    tg.corun();
  }
}

std::vector<int> run_matmul(tf::Executor& exec, int N) {
  std::vector<int> A(N * N, 1);
  std::vector<int> B(N * N, 1);
  std::vector<int> C(N * N, 0);

  int* a = A.data();
  int* b = B.data();
  int* c = C.data();

  for (int i = 0; i < N; i++) {
    for (int j = 0; j < N; j++) {
      a[i * N + j] = 1;
      b[i * N + j] = 1;
      c[i * N + j] = 0;
    }
  }

  exec.async([=]() { matmul(a, b, c, N, N); }).get();
  return C;
}

static void BM_Matmul(benchmark::State& state) {
  int N = static_cast<int>(state.range(0));
  if (!executor.has_value()) {
    executor.emplace(thread_count);
  }

  // warmup
  run_matmul(*executor, N);

  for (auto _ : state) {
    auto result = run_matmul(*executor, N);
    benchmark::DoNotOptimize(result.data());
    benchmark::ClobberMemory();
  }
}
BENCHMARK(BM_Matmul)->Arg(256);
BENCHMARK_MAIN();
