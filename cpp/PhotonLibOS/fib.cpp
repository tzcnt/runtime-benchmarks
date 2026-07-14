// An implementation of the recursive fork fibonacci parallelism test.
//
// Canonical (TooManyCooks) implementation: ../TooManyCooks/fib.cpp
//
// A fork spawns fib(n-1) as a bench::thread (created on the current
// vcpu and migrated to a work-pool vcpu) and the caller continues with
// fib(n-2) on its own stack, then joins the child - the same
// fork-continue-join shape as the Go and TMC implementations.

#include "memusage.hpp"
#include "photon_bench.hpp"

#include <chrono>
#include <cinttypes>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

static const std::size_t iter_count = 1;

static std::uint64_t fib(std::uint64_t n) {
  if (n < 2) {
    return n;
  }
  std::uint64_t x;
  bench::thread child([&x, n] { x = fib(n - 1); });
  const std::uint64_t y = fib(n - 2);
  child.join();
  return x + y;
}

int main(int argc, char* argv[]) {
  if (argc < 2) {
    std::printf("Usage: fib <n-th fibonacci number requested>\n");
    return 0;
  }
  const std::uint64_t n = strtoull(argv[1], nullptr, 10);
  std::size_t thread_count = bench::default_thread_count();
  if (argc > 2) {
    thread_count = static_cast<std::size_t>(atoi(argv[2]));
  }
  std::printf("threads: %zu\n", thread_count);

  bench::quiet_logs();
  bench::use_pooled_stacks();

  bench::run_in_pool(thread_count, [n] {
    (void)fib(30); // warmup

    auto startTime = std::chrono::high_resolution_clock::now();
    std::uint64_t result = 0;
    for (std::size_t i = 0; i < iter_count; ++i) {
      result = fib(n);
    }
    auto endTime = std::chrono::high_resolution_clock::now();
    auto totalTimeUs = std::chrono::duration_cast<std::chrono::microseconds>(
      endTime - startTime
    );
    std::printf("output: %" PRIu64 "\n", result);
    std::printf("runs:\n");
    std::printf("  - iteration_count: %zu\n", iter_count);
    std::printf("    duration: %" PRIu64 " us\n",
                static_cast<std::uint64_t>(totalTimeUs.count()));
    std::printf("    max_rss: %ld KiB\n", peak_memory_usage());
  });
}
