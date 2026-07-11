// An implementation of recursive matrix multiplication
//
// Canonical (TooManyCooks) implementation: ../TooManyCooks/matmul.cpp
// base case: ../2common/matmul.hpp
//
// The matrices are split into quadrants recursively down to a 32x32 base
// case. The 8 sub-multiplications are run as two sequential groups of 4
// parallel engine tasks (joined via engine::GetAll) so that no output tile is
// written by two tasks at once.

#include "matmul.hpp"
#include "memusage.hpp"
#include "userver_bench.hpp"

#include <userver/engine/async.hpp>
#include <userver/engine/get_all.hpp>
#include <userver/engine/task/task_with_result.hpp>

#include <atomic>
#include <chrono>
#include <cinttypes>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <vector>

namespace engine = userver::engine;

static void matmul(int* a, int* b, int* c, int n, int N) {
  if (n <= 32) {
    // Base case: Use simple triple-loop multiplication for small matrices
    matmul_small(a, b, c, n, N);
    return;
  }
  // Recursive case: Divide the matrices into 4 submatrices and multiply them
  const int k = n / 2;

  // Split the execution into 2 sections to ensure output locations are not
  // written in parallel
  {
    auto t0 = engine::AsyncNoTracing(matmul, a, b, c, k, N);
    auto t1 = engine::AsyncNoTracing(matmul, a, b + k, c + k, k, N);
    auto t2 = engine::AsyncNoTracing(matmul, a + k * N, b, c + k * N, k, N);
    auto t3 =
      engine::AsyncNoTracing(matmul, a + k * N, b + k, c + k * N + k, k, N);
    engine::GetAll(t0, t1, t2, t3);
  }
  {
    auto t0 = engine::AsyncNoTracing(matmul, a + k, b + k * N, c, k, N);
    auto t1 =
      engine::AsyncNoTracing(matmul, a + k, b + k * N + k, c + k, k, N);
    auto t2 = engine::AsyncNoTracing(
      matmul, a + k * N + k, b + k * N, c + k * N, k, N
    );
    auto t3 = engine::AsyncNoTracing(
      matmul, a + k * N + k, b + k * N + k, c + k * N + k, k, N
    );
    engine::GetAll(t0, t1, t2, t3);
  }
}

static std::vector<int> run_matmul(int N) {
  std::vector<int> A(static_cast<std::size_t>(N) * N, 1);
  std::vector<int> B(static_cast<std::size_t>(N) * N, 1);
  std::vector<int> C(static_cast<std::size_t>(N) * N, 0);

  matmul(A.data(), B.data(), C.data(), N, N);
  return C;
}

static void validate_result(std::vector<int>& C, int N) {
  std::atomic_thread_fence(std::memory_order_seq_cst);
  int* c = C.data();
  for (int i = 0; i < N; i++) {
    for (int j = 0; j < N; j++) {
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

static void run_one(int N) {
  auto startTime = std::chrono::high_resolution_clock::now();
  std::vector<int> result = run_matmul(N);
  auto endTime = std::chrono::high_resolution_clock::now();
  validate_result(result, N);
  auto totalTimeUs =
    std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
  std::printf("  - matrix_size: %d\n", N);
  std::printf("    duration: %" PRIu64 " us\n",
              static_cast<std::uint64_t>(totalTimeUs.count()));
  std::printf("    max_rss: %ld KiB\n", peak_memory_usage());
}

int main(int argc, char* argv[]) {
  if (argc < 2) {
    std::printf("Usage: matmul <matrix size (power of 2)>\n");
    return 0;
  }
  const int n = atoi(argv[1]);
  std::size_t thread_count = bench::default_thread_count();
  if (argc > 2) {
    thread_count = static_cast<std::size_t>(atoi(argv[2]));
  }
  std::printf("threads: %zu\n", thread_count);

  engine::RunStandalone(thread_count, bench::pools_config(true), [n] {
    run_matmul(n); // warmup

    std::printf("runs:\n");
    run_one(n);
  });
}
