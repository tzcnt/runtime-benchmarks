// An implementation of recursive matrix multiplication

// Adapted from
// https://github.com/mtmucha/coros/blob/main/benchmarks/coros_mat.h

// Original author: mtmucha
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <tbb/tbb.h>

#include <array>
#include <chrono>
#include <cinttypes>
#include <cstdio>
#include <memory>

static int thread_count = std::thread::hardware_concurrency() / 2;
static const size_t iter_count = 1;
static constexpr int N = 1024;

void matmul(int* a, int* b, std::atomic<int>* c, int n, int N) {
  if (n <= 32) {
    // Base case: Use simple triple-loop multiplication for small matrices
    for (int i = 0; i < n; i++) {
      for (int j = 0; j < n; j++) {
        for (int k = 0; k < n; k++) {
          c[i * N + j].fetch_add(
            a[i * N + k] * b[k * N + j], std::memory_order::relaxed
          );
        }
      }
    }
  } else {
    // Recursive case: Divide the matrices into 4 submatrices and multiply them
    int k = n / 2;

    tbb::parallel_invoke(
      [&]() { matmul(a, b, c, k, N); },
      [&]() { matmul(a + k, b + k * N, c, k, N); },
      [&]() { matmul(a, b + k, c + k, k, N); },
      [&]() { matmul(a + k, b + k * N + k, c + k, k, N); },
      [&]() { matmul(a + k * N, b, c + k * N, k, N); },
      [&]() { matmul(a + k * N + k, b + k * N, c + k * N, k, N); },
      [&]() { matmul(a + k * N, b + k, c + k * N + k, k, N); },
      [&]() { matmul(a + k * N + k, b + k * N + k, c + k * N + k, k, N); }
    );
  }
}

std::unique_ptr<std::array<std::atomic<int>, N * N>>
run_matmul(tbb::task_arena& arena) {
  auto A = std::make_unique<std::array<int, N * N>>();
  auto B = std::make_unique<std::array<int, N * N>>();
  auto C = std::make_unique<std::array<std::atomic<int>, N * N>>();

  int* a = A->data();
  int* b = B->data();
  std::atomic<int>* c = C->data();

  for (int i = 0; i < N; i++) {
    for (int j = 0; j < N; j++) {
      a[i * N + j] = 1;
      b[i * N + j] = 1;
      c[i * N + j] = 0;
    }
  }

  arena.execute([&] { matmul(a, b, c, N, N); });
  return C;
}

void validate_result(std::unique_ptr<std::array<std::atomic<int>, N * N>> C) {
  std::atomic_thread_fence(std::memory_order_seq_cst);
  std::atomic<int>* c = C->data();
  bool done = false;
  for (int i = 0; i < N && !done; i++) {
    for (int j = 0; j < N && !done; j++) {
      auto res = c[i * N + j].load(std::memory_order_relaxed);
      if (res != N) {
        std::printf("Wrong result at (%d,%d) : %d. Expected %d", i, j, res, N);
        done = true;
      }
    }
  }
}

int main(int argc, char* argv[]) {
  std::printf("threads: %d\n", thread_count);
  tbb::task_arena arena(thread_count);

  // warmup
  auto result = run_matmul(arena);
  validate_result(std::move(result));

  auto startTime = std::chrono::high_resolution_clock::now();

  for (size_t i = 0; i < iter_count; ++i) {
    auto result = run_matmul(arena);
  }

  auto endTime = std::chrono::high_resolution_clock::now();
  auto totalTimeUs =
    std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
  std::printf("runs:\n");
  std::printf("  - iteration_count: %" PRIu64 "\n", iter_count);
  std::printf("    duration: %" PRIu64 " us\n", totalTimeUs.count());
}
