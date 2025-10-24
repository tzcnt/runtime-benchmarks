// The skynet benchmark as described here:
// https://github.com/atemerev/skynet

// Adapted from
// https://github.com/tzcnt/tmc-examples/blob/main/examples/skynet/main.cpp
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

#include "hpx/async_combinators/when_all.hpp"
#include <hpx/experimental/task_group.hpp>
#include <hpx/future.hpp>
#include <hpx/init.hpp>

#include <array>
#include <chrono>
#include <cinttypes>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

static size_t thread_count = std::thread::hardware_concurrency() / 2;
static const size_t iter_count = 1;

template <size_t DepthMax>
hpx::future<size_t> skynet_one(size_t BaseNum, size_t Depth) {
  if (Depth == DepthMax) {
    co_return BaseNum;
  }
  size_t depthOffset = 1;
  for (size_t i = 0; i < DepthMax - Depth - 1; ++i) {
    depthOffset *= 10;
  }

  std::array<hpx::future<size_t>, 9> futures;
  for (size_t idx = 0; idx < 9; ++idx) {
    // // This is the fastest overall (11 seconds) but has no scaling: 1 or 64
    // // threads all take 11 seconds. I believe this means that it's not
    // // actually being parallelized; using the debugger confirms skynet is
    // // being called directly each time, rather than dispatched, which defeats
    // the purpose of the benchmark.
    // futures[idx] = skynet_one<DepthMax>(BaseNum + depthOffset
    // * idx, Depth + 1);

    // This is slower but is accurate to the intent of the benchmark.
    // However it still causes a huge memory blowup, even when using 1 thread
    // and running with --hpx:queuing=abp-priority-lifo which SHOULD prevent
    // this memory blowup...
    //
    // But the memory blowup is not as high as using the default HPX stackful
    // coroutines approach. It eventually stabilizes at ~24GB mem used, which
    // means this can actually complete on a consumer grade system.
    //
    // Adding hpx::launch::fork increases the speed of the benchmark, but pushes
    // memory usage over 30GB which will cause OOM on several of the test
    // machines. Thus it is not included here.
    futures[idx] =
      hpx::async(skynet_one<DepthMax>, BaseNum + depthOffset * idx, Depth + 1);
  }

  // Fork 9, run 1 synchronously
  size_t count =
    co_await skynet_one<DepthMax>(BaseNum + depthOffset * 9, Depth + 1);

  auto results = co_await hpx::when_all(futures);

  for (auto& f : results) {
    count += co_await f;
  }
  co_return count;
}

// // This stackful coroutine implementation caused OOM despite being the
// // recommended implementation in /hpx-src/tests/performance/local/skynet.cpp
// template <size_t DepthMax> size_t skynet_one(size_t BaseNum, size_t Depth)
// {
//   if (Depth == DepthMax) {
//     return BaseNum;
//   }
//   size_t depthOffset = 1;
//   for (size_t i = 0; i < DepthMax - Depth - 1; ++i) {
//     depthOffset *= 10;
//   }

//   std::array<hpx::future<size_t>, 10> futures;
//   for (size_t idx = 0; idx < 10; ++idx) {
//     futures[idx] =
//       hpx::async(skynet_one<DepthMax>, BaseNum + depthOffset * idx, Depth +
//       1);
//   }
//   hpx::wait_all(futures);

//   size_t count = 0;
//   for (auto& f : futures) {
//     count += f.get();
//   }

//   // // This implementation using task_group also caused OOM.
//   // std::array<size_t, 10> results;
//   // hpx::experimental::task_group tg;
//   // for (size_t idx = 0; idx < 10; ++idx) {
//   //   tg.run(
//   //     [&results, idx, base = BaseNum + depthOffset * idx, Depth]() -> void
//   {
//   //       results[idx] = skynet_one<DepthMax>(base, Depth + 1);
//   //     }
//   //   );
//   // }
//   // tg.wait();
//   // size_t count = 0;
//   // for (size_t i = 0; i < 10; ++i) {
//   //   count += results[i];
//   // }
//   return count;
// }

template <size_t DepthMax> void skynet() {
  size_t count = skynet_one<DepthMax>(0, 0).get();
  if (count != 4999999950000000) {
    std::printf("ERROR: wrong result - %" PRIu64 "\n", count);
  }
}

template <size_t Depth = 6> void loop_skynet() {
  std::printf("runs:\n");
  auto startTime = std::chrono::high_resolution_clock::now();
  for (size_t j = 0; j < iter_count; ++j) {
    skynet<Depth>();
  }
  auto endTime = std::chrono::high_resolution_clock::now();
  auto totalTimeUs =
    std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
  std::printf("  - iteration_count: %" PRIu64 "\n", iter_count);
  std::printf("    duration: %" PRIu64 " us\n", totalTimeUs.count());
}

int hpx_main(hpx::program_options::variables_map&) {
  hpx::threads::set_scheduler_mode(
    hpx::threads::policies::scheduler_mode::enable_stealing |
    hpx::threads::policies::scheduler_mode::enable_stealing_numa |
    hpx::threads::policies::scheduler_mode::assign_work_thread_parent |
    // hpx::threads::policies::scheduler_mode::assign_work_round_robin |
    hpx::threads::policies::scheduler_mode::steal_after_local
  );

  // This benchmark peaks around 27GB usage on
  // the warmup, and then the real run goes to 30.5GB which causes OOM kill on
  // my 32GB RAM systems. So I can only reliably complete this benchmark with
  // the warmup disabled.
  // skynet<8>(); // warmup

  loop_skynet<8>();

  return hpx::local::finalize();
}

int main(int argc, char* argv[]) {
  if (argc > 1) {
    thread_count = static_cast<size_t>(atoi(argv[1]));
  }
  std::printf("threads: %" PRIu64 "\n", thread_count);

  // Force HPX to use the most efficient (?) queue mode
  hpx::local::init_params init_args;
  init_args.cfg = {
    "hpx.os_threads=" + std::to_string(thread_count)
    // ,
    // "--hpx:queuing=abp-priority-lifo"
  };

  return hpx::local::init(hpx_main, argc, argv, init_args);
}
