// Port of cpp/libfork/matmul.cpp using citor::forkJoin.

#include "matmul.hpp"
#include "citor/hints.h"
#include "citor/thread_pool.h"
#include "memusage.hpp"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <thread>
#include <vector>

static size_t thread_count = std::thread::hardware_concurrency() / 2;

static void
matmul(citor::ThreadPool& pool, int* a, int* b, int* c, int n, int N) {
  if (n <= 32) {
    matmul_small(a, b, c, n, N);
    return;
  }
  int k = n / 2;

  pool.forkJoin<citor::HintsDefaults>(
    [&] { matmul(pool, a, b, c, k, N); },
    [&] { matmul(pool, a, b + k, c + k, k, N); },
    [&] { matmul(pool, a + k * N, b, c + k * N, k, N); },
    [&] { matmul(pool, a + k * N, b + k, c + k * N + k, k, N); }
  );

  pool.forkJoin<citor::HintsDefaults>(
    [&] { matmul(pool, a + k, b + k * N, c, k, N); },
    [&] { matmul(pool, a + k, b + k * N + k, c + k, k, N); },
    [&] { matmul(pool, a + k * N + k, b + k * N, c + k * N, k, N); },
    [&] { matmul(pool, a + k * N + k, b + k * N + k, c + k * N + k, k, N); }
  );
}

static std::vector<int> run_matmul(citor::ThreadPool& pool, int N) {
  std::vector<int> A(N * N, 1);
  std::vector<int> B(N * N, 1);
  std::vector<int> C(N * N, 0);

  int* a = A.data();
  int* b = B.data();
  int* c = C.data();
  matmul(pool, a, b, c, N, N);
  return C;
}

static void validate_result(std::vector<int>& C, int N) {
  std::atomic_thread_fence(std::memory_order_seq_cst);
  int* c = C.data();
  for (int i = 0; i < N; ++i) {
    for (int j = 0; j < N; ++j) {
      auto res = c[i * N + j];
      if (res != N) {
        std::printf(
          "Wrong result at (%d,%d) : %d. expected %d\n", i, j, res, N
        );
        std::fflush(stdout);
        std::terminate();
      }
    }
  }
}

static void run_one(citor::ThreadPool& pool, int N) {
  auto startTime = std::chrono::high_resolution_clock::now();
  std::vector<int> result = run_matmul(pool, N);
  auto endTime = std::chrono::high_resolution_clock::now();
  validate_result(result, N);
  auto totalTimeUs =
    std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
  std::printf("  - matrix_size: %d\n", N);
  std::printf(
    "    duration: %zu us\n", static_cast<size_t>(totalTimeUs.count())
  );
  std::printf("    max_rss: %ld KiB\n", peak_memory_usage());
}

int main(int argc, char* argv[]) {
  if (argc > 2) {
    thread_count = static_cast<size_t>(atoi(argv[2]));
  }
  if (argc < 2) {
    printf("Usage: matmul <matrix size (power of 2)>\n");
    exit(0);
  }
  int n = atoi(argv[1]);
  std::printf("threads: %zu\n", thread_count);
  // citor's default PerCpu affinity caps workers at the physical-core
  // count. When the sweep requests every logical CPU, opt into
  // SMT-sibling placement so all hardware threads are used.
  const citor::Affinity affinity =
    (thread_count == std::thread::hardware_concurrency())
      ? citor::Affinity::PerCpuSmtPair
      : citor::Affinity::PerCpu;
  citor::ThreadPool pool(thread_count, affinity);

  run_matmul(pool, n); // warmup

  std::printf("runs:\n");
  run_one(pool, n);
  return 0;
}
