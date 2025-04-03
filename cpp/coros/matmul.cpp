// An implementation of recursive matrix multiplication

// Adapted from
// https://github.com/mtmucha/coros/blob/main/benchmarks/coros_mat.h

// Original author: mtmucha
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt)

#include "start_tasks.h"
#include "thread_pool.h"
#include "wait_tasks.h"

#include <chrono>
#include <cstdio>
#include <exception>
#include <vector>

static int thread_count = std::thread::hardware_concurrency() / 2;

coros::Task<void> matmul(int* a, int* b, int* c, int n, int N) {
  if (n <= 32) {
    // Base case: Use simple triple-loop multiplication for small matrices
    for (int i = 0; i < n; i++) {
      for (int k = 0; k < n; k++) {
        for (int j = 0; j < n; j++) {
          c[i * N + j] += a[i * N + k] * b[k * N + j];
        }
      }
    }
  } else {
    // Recursive case: Divide the matrices into 4 submatrices and multiply them
    int k = n / 2;

    // Split the execution into 2 sections to ensure output locations are not
    // written in parallel
    co_await coros::wait_tasks(
      matmul(a, b, c, k, N), matmul(a, b + k, c + k, k, N),
      matmul(a + k * N, b, c + k * N, k, N),
      matmul(a + k * N, b + k, c + k * N + k, k, N)
    );

    co_await coros::wait_tasks(
      matmul(a + k, b + k * N, c, k, N),
      matmul(a + k, b + k * N + k, c + k, k, N),
      matmul(a + k * N + k, b + k * N, c + k * N, k, N),
      matmul(a + k * N + k, b + k * N + k, c + k * N + k, k, N)
    );
  }
}

std::vector<int> run_matmul(coros::ThreadPool& executor, int N) {
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

  coros::Task<void> t = matmul(a, b, c, N, N);
  coros::start_sync(executor, t);
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

void run_one(coros::ThreadPool& executor, int N) {
  auto startTime = std::chrono::high_resolution_clock::now();
  std::vector<int> result = run_matmul(executor, N);
  auto endTime = std::chrono::high_resolution_clock::now();
  validate_result(result, N);
  auto totalTimeUs =
    std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
  std::printf("  - matrix_size: %d\n", N);
  std::printf("    duration: %zu us\n", totalTimeUs.count());
}

int main(int argc, char* argv[]) {
  std::printf("threads: %d\n", thread_count);
  coros::ThreadPool executor{thread_count};

  run_matmul(executor, 1024); // warmup

  std::printf("runs:\n");

  for (int i = 5; i < 13; ++i) {
    int N = 1 << i;
    run_one(executor, N);
  }
}
