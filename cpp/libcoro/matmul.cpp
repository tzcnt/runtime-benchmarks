// An implementation of recursive matrix multiplication

// Adapted from
// https://github.com/mtmucha/coros/blob/main/benchmarks/coros_mat.h

// Original author: mtmucha
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt)

#include "memusage.hpp"
#include "matmul.hpp"
#include "coro/coro.hpp" // IWYU pragma: keep

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <vector>

static size_t thread_count = std::thread::hardware_concurrency() / 2;

coro::task<void>
matmul(coro::thread_pool& tp, int* a, int* b, int* c, int n, int N) {
  co_await tp.schedule();

  if (n <= 32) {
    // Base case: Use simple triple-loop multiplication for small matrices
    matmul_small(a, b, c, n, N);
  } else {
    // Recursive case: Divide the matrices into 4 submatrices and multiply them
    int k = n / 2;

    // Split the execution into 2 sections to ensure output locations are not
    // written in parallel
    co_await coro::when_all(
      matmul(tp, a, b, c, k, N), matmul(tp, a, b + k, c + k, k, N),
      matmul(tp, a + k * N, b, c + k * N, k, N),
      matmul(tp, a + k * N, b + k, c + k * N + k, k, N)
    );

    co_await coro::when_all(
      matmul(tp, a + k, b + k * N, c, k, N),
      matmul(tp, a + k, b + k * N + k, c + k, k, N),
      matmul(tp, a + k * N + k, b + k * N, c + k * N, k, N),
      matmul(tp, a + k * N + k, b + k * N + k, c + k * N + k, k, N)
    );
  }
}

std::vector<int> run_matmul(coro::thread_pool& tp, int N) {
  std::vector<int> A(N * N, 1);
  std::vector<int> B(N * N, 1);
  std::vector<int> C(N * N, 0);

  int* a = A.data();
  int* b = B.data();
  int* c = C.data();

  coro::sync_wait(matmul(tp, a, b, c, N, N));
  return C;
}

void validate_result(std::vector<int>& C, int N) {
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

void run_one(coro::thread_pool& tp, int N) {
  auto startTime = std::chrono::high_resolution_clock::now();
  std::vector<int> result = run_matmul(tp, N);
  auto endTime = std::chrono::high_resolution_clock::now();
  validate_result(result, N);
  auto totalTimeUs =
    std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
  std::printf("  - matrix_size: %d\n", N);
  std::printf("    duration: %zu us\n", totalTimeUs.count());
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

  coro::thread_pool::options opts;
  opts.thread_count = static_cast<uint32_t>(thread_count);
  auto tp = coro::thread_pool::make_unique(opts);
  run_matmul(*tp, n); // warmup

  std::printf("runs:\n");

  run_one(*tp, n);
}
