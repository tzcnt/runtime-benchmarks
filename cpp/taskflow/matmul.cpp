// An implementation of recursive matrix multiplication

// Adapted from
// https://github.com/mtmucha/coros/blob/main/benchmarks/coros_mat.h

// Original author: mtmucha
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt)

#include "matmul.hpp"
#include <taskflow/algorithm/for_each.hpp>
#include <taskflow/taskflow.hpp>

#include <chrono>
#include <cstdio>
#include <cstring>
#include <exception>
#include <vector>

static int thread_count = std::thread::hardware_concurrency() / 2;
static const size_t iter_count = 1;

void matmul(tf::Subflow& sbf, int* a, int* b, int* c, int n, int N) {
  if (n <= 32) {
    // Base case: Use simple triple-loop multiplication for small matrices
    matmul_small(a, b, c, n, N);
  } else {
    // Recursive case: Divide the matrices into 4 submatrices and multiply them
    int k = n / 2;

    // Split the execution into 2 sections to ensure output locations are not
    // written in parallel
    auto A1 = sbf.emplace([=](tf::Subflow& s) { matmul(s, a, b, c, k, N); });
    auto A2 = sbf.emplace([=](tf::Subflow& s) {
      matmul(s, a + k, b + k * N, c, k, N);
    });
    A1.precede(A2);

    auto B1 =
      sbf.emplace([=](tf::Subflow& s) { matmul(s, a, b + k, c + k, k, N); });
    auto B2 = sbf.emplace([=](tf::Subflow& s) {
      matmul(s, a + k, b + k * N + k, c + k, k, N);
    });
    B1.precede(B2);

    auto C1 = sbf.emplace([=](tf::Subflow& s) {
      matmul(s, a + k * N, b, c + k * N, k, N);
    });
    auto C2 = sbf.emplace([=](tf::Subflow& s) {
      matmul(s, a + k * N + k, b + k * N, c + k * N, k, N);
    });
    C1.precede(C2);

    auto D1 = sbf.emplace([=](tf::Subflow& s) {
      matmul(s, a + k * N, b + k, c + k * N + k, k, N);
    });
    auto D2 = sbf.emplace([=](tf::Subflow& s) {
      matmul(s, a + k * N + k, b + k * N + k, c + k * N + k, k, N);
    });
    D1.precede(D2);

    sbf.join();
  }
}

std::vector<int> run_matmul(tf::Executor& executor, int N, bool flat) {
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

  tf::Taskflow taskflow;
  if (flat) {
    taskflow.for_each_index(0, N, 1, [&](int i) {
      for (int j = 0; j < N; ++j) {
        for (int k = 0; k < N; k++) {
          c[i * N + j] += a[i * N + k] * b[k * N + j];
        }
      }
    });
  } else {
    taskflow.emplace([=](tf::Subflow& sbf) { matmul(sbf, a, b, c, N, N); });
  }
  executor.run(taskflow).wait();
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

void run_one(tf::Executor& executor, int N, bool flat) {
  auto startTime = std::chrono::high_resolution_clock::now();
  std::vector<int> result = run_matmul(executor, N, flat);
  auto endTime = std::chrono::high_resolution_clock::now();
  validate_result(result, N);
  auto totalTimeUs =
    std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
  std::printf("  - matrix_size: %d\n", N);
  std::printf("    duration: %zu us\n", totalTimeUs.count());
}

int main(int argc, char* argv[]) {
  if (argc < 2) {
    printf("Usage: matmul <matrix size (power of 2)>\n");
    exit(0);
  }
  int n = atoi(argv[1]);
  bool flat = argc >= 3 && strcmp(argv[2], "flat") == 0;

  std::printf("threads: %d\n", thread_count);
  tf::Executor executor(thread_count);

  run_matmul(executor, n, flat); // warmup

  std::printf("runs:\n");

  run_one(executor, n, flat);
}
