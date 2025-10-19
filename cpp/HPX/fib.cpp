// An implementation of the recursive fork fibonacci parallelism test.

// Adapted from https://github.com/tzcnt/tmc-examples/blob/main/examples/fib.cpp
// Original author: tzcnt
// Unlicense License
// This is free and unencumbered software released into the public domain.

// Anyone is free to copy, modify, publish, use, compile, sell, or
// distribute this software, either in source code form or as a compiled
// binary, for any purpose, commercial or non-commercial, and by any
// means.

// In jurisdictions that recognize copyright laws, the author or authors
// of this software dedicate any and all copyright interest in the
// software to the public domain. We make this dedication for the benefit
// of the public at large and to the detriment of our heirs and
// successors. We intend this dedication to be an overt act of
// relinquishment in perpetuity of all present and future rights to this
// software under copyright law.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
// EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
// IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
// OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
// ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
// OTHER DEALINGS IN THE SOFTWARE.

#include <hpx/future.hpp>
#include <hpx/init.hpp>

#include <chrono>
#include <cinttypes>
#include <cstdio>
#include <cstdlib>
#include <string>

static size_t thread_count = std::thread::hardware_concurrency() / 2;
static const size_t iter_count = 1;
static size_t fib_n = 0;

hpx::future<size_t> fib(size_t n) {
  if (n < 2)
    co_return n;

  hpx::future<size_t> n1 = hpx::async(fib, n - 1);
  size_t n2 = co_await fib(n - 2);
  co_return co_await n1 + n2;

  // This version is faster, but cheats the benchmark - it doesn't actually
  // fork. There is no difference in runtime scaling from 1 to 64 threads.
  // auto [x, y] = co_await hpx::when_all(fib(n - 1), fib(n - 2));
  // co_return co_await x + co_await y;
}

// // This version causes HUGE memory blowup (using stackful coroutine)
// size_t fib(size_t n) {
//   if (n < 2)
//     return n;

//   hpx::future<size_t> n1 = hpx::async(fib, n - 1);
//   size_t n2 = fib(n - 2);
//   return n1.get() + n2;
// }

int hpx_main(hpx::program_options::variables_map&) {
  hpx::threads::set_scheduler_mode(
    hpx::threads::policies::scheduler_mode::enable_stealing |
    hpx::threads::policies::scheduler_mode::enable_stealing_numa |
    hpx::threads::policies::scheduler_mode::assign_work_thread_parent |
    // hpx::threads::policies::scheduler_mode::assign_work_round_robin |
    hpx::threads::policies::scheduler_mode::steal_after_local
  );

  auto result = fib(30).get(); // warmup

  auto startTime = std::chrono::high_resolution_clock::now();

  for (size_t i = 0; i < iter_count; ++i) {
    result = fib(fib_n).get();
    std::printf("output: %zu\n", result);
  }

  auto endTime = std::chrono::high_resolution_clock::now();
  auto totalTimeUs =
    std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
  std::printf("runs:\n");
  std::printf("  - iteration_count: %" PRIu64 "\n", iter_count);
  std::printf("    duration: %" PRIu64 " us\n", totalTimeUs.count());

  return hpx::local::finalize();
}

int main(int argc, char* argv[]) {
  if (argc > 2) {
    thread_count = static_cast<size_t>(atoi(argv[2]));
  }
  if (argc < 2) {
    printf("Usage: fib <n-th fibonacci number requested> [thread count]\n");
    exit(0);
  }
  fib_n = static_cast<size_t>(atoi(argv[1]));

  // Force HPX to use the most efficient (?) queue mode
  // in a hacky way since it only allows for command line configuration.
  std::string queue_mode("--hpx:queuing=abp-priority-lifo");
  argv[1] = const_cast<char*>(queue_mode.c_str());

  std::printf("threads: %" PRIu64 "\n", thread_count);

  hpx::local::init_params init_args;
  init_args.cfg = {"hpx.os_threads=" + std::to_string(thread_count)};

  return hpx::local::init(hpx_main, argc, argv, init_args);
}
