// Port of cpp/libfork/fib.cpp using citor::forkJoin.

#include "memusage.hpp"
#include "citor/thread_pool.h"
#include "citor/hints.h"

#include <chrono>
#include <cinttypes>
#include <cstdio>
#include <cstdlib>
#include <thread>

static size_t thread_count = std::thread::hardware_concurrency() / 2;
static const size_t iter_count = 1;

size_t fibonacci(citor::ThreadPool& pool, size_t n) {
  if (n < 2) {
    return n;
  }
  size_t x = 0;
  size_t y = 0;
  pool.forkJoin<citor::HintsDefaults>(
    [&] { x = fibonacci(pool, n - 1); },
    [&] { y = fibonacci(pool, n - 2); }
  );
  return x + y;
}

int main(int argc, char* argv[]) {
  if (argc > 2) {
    thread_count = static_cast<size_t>(atoi(argv[2]));
  }
  if (argc < 2) {
    printf("Usage: fib <n-th fibonacci number requested>\n");
    exit(0);
  }
  size_t n = static_cast<size_t>(atoi(argv[1]));

  std::printf("threads: %zu\n", thread_count);
  // citor's default PerCpu affinity caps workers at the physical-core
  // count. When the sweep requests every logical CPU, opt into
  // SMT-sibling placement so all hardware threads are used.
  const citor::Affinity affinity =
      (thread_count == std::thread::hardware_concurrency())
          ? citor::Affinity::PerCpuSmtPair
          : citor::Affinity::PerCpu;
  citor::ThreadPool pool(thread_count, affinity);

  size_t result = fibonacci(pool, 30); // warmup
  (void)result;

  auto startTime = std::chrono::high_resolution_clock::now();

  for (size_t i = 0; i < iter_count; ++i) {
    result = fibonacci(pool, n);
    std::printf("output: %zu\n", result);
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
