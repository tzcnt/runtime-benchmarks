// An implementation of recursive matrix multiplication

// Adapted from
// https://github.com/mtmucha/coros/blob/main/benchmarks/coros_mat.h

// Original author: mtmucha
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt)

#include "matmul.hpp"
#include "concurrencpp/concurrencpp.h"
#include <concurrencpp/results/constants.h>
#include <concurrencpp/runtime/runtime.h>

#include <chrono>
#include <concurrencpp/task.h>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <thread>
#include <vector>

using namespace concurrencpp;

static size_t thread_count = std::thread::hardware_concurrency() / 2;

fork_result<void>
matmul(thread_pool_executor* executor, int* a, int* b, int* c, int n, int N) {
  if (n <= 32) {
    // Base case: Use simple triple-loop multiplication for small matrices
    matmul_small(a, b, c, n, N);
  } else {
    // Recursive case: Divide the matrices into 4 submatrices and multiply them
    int k = n / 2;

    // Split the execution into 2 sections to ensure output locations are not
    // written in parallel
    std::array<fork_result<void>, 4> children;
    children[0] = matmul(executor, a, b, c, k, N);
    children[1] = matmul(executor, a, b + k, c + k, k, N);
    children[2] = matmul(executor, a + k * N, b, c + k * N, k, N);
    children[3] = matmul(executor, a + k * N, b + k, c + k * N + k, k, N);
    auto results = co_await fork_join(executor, children);

    children[0] = matmul(executor, a + k, b + k * N, c, k, N);
    children[1] = matmul(executor, a + k, b + k * N + k, c + k, k, N);
    children[2] = matmul(executor, a + k * N + k, b + k * N, c + k * N, k, N);
    children[3] =
      matmul(executor, a + k * N + k, b + k * N + k, c + k * N + k, k, N);
    results = co_await fork_join(executor, children);
  }
}

std::vector<int> run_matmul(thread_pool_executor* executor, int N) {
  std::vector<int> A(N * N, 1);
  std::vector<int> B(N * N, 1);
  std::vector<int> C(N * N, 0);

  int* a = A.data();
  int* b = B.data();
  int* c = C.data();
  matmul(executor, a, b, c, N, N).as_root(executor).run().get();
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

void run_one(thread_pool_executor* executor, int N) {
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
  if (argc > 2) {
    thread_count = static_cast<size_t>(atoi(argv[2]));
  }
  if (argc < 2) {
    printf("Usage: matmul <matrix size (power of 2)>\n");
    exit(0);
  }
  int n = atoi(argv[1]);
  std::printf("threads: %zu\n", thread_count);
  concurrencpp::runtime_options opt;
  opt.max_cpu_threads = thread_count;
  concurrencpp::runtime runtime(opt);

  run_matmul(runtime.thread_pool_executor().get(), n); // warmup

  std::printf("runs:\n");

  run_one(runtime.thread_pool_executor().get(), n);
}
