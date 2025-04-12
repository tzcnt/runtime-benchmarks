// Adapted from the benchmark provided at:
// https://github.com/ConorWilliams/libfork/blob/c53c13102e30e8a68d5a9200ff90ad8d4b239520/bench/source/fib/libfork.cpp

// Original Copyright Notice:
// Copyright © Conor Williams <conorwilliams@outlook.com>

// SPDX-License-Identifier: MPL-2.0

// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include <chrono>
#include <cinttypes>
#include <cstdio>
#include <photon/common/alog.h>
#include <photon/photon.h>
#include <photon/thread/stack-allocator.h>
#include <photon/thread/thread11.h>
#include <photon/thread/workerpool.h>
#include <thread>

static size_t thread_count = std::thread::hardware_concurrency() / 2;
static const size_t iter_count = 1;

static photon::WorkPool* pool;

struct fib_data {
  size_t n;
  size_t sum;
};

void* fib(void* d) {
  fib_data* data = reinterpret_cast<fib_data*>(d);
  size_t n = data->n;
  if (n < 2) {
    data->sum = n;
    return nullptr;
  }

  photon::semaphore sem(0);
  fib_data a, b;
  a.n = n - 1;
  b.n = n - 2;

  pool->async_call(new auto([&] {
    fib(&a);
    sem.signal(1);
  }));
  // photon::thread_yield();
  pool->async_call(new auto([&] {
    fib(&b);
    sem.signal(1);
  }));
  // photon::thread_yield();
  sem.wait(2);

  data->sum = a.sum + b.sum;
  return nullptr;
}

int main(int argc, char* argv[]) {
  if (argc > 2) {
    thread_count = static_cast<size_t>(atoi(argv[2]));
  }
  if (argc < 2) {
    //   printf("Usage: fib <n-th fibonacci number requested>\n");
    //   exit(0);
    // }
    // size_t n = static_cast<size_t>(atoi(argv[1]));
  }
  size_t n = 10;

  std::printf("threads: %" PRIu64 "\n", thread_count);

  set_log_output_level(ALOG_WARN);

  photon::use_pooled_stack_allocator();

  photon::init(photon::INIT_EVENT_DEFAULT, photon::INIT_IO_NONE);
  DEFER(photon::fini());

  pool = new photon::WorkPool(
    thread_count, photon::INIT_EVENT_DEFAULT, photon::INIT_IO_NONE, -1
  );
  DEFER(delete pool);

  fib_data data{n, 0};
  photon::threads_create_join(1, fib, &data, 16UL * 1024);

  std::printf("results:\n");
  auto startTime = std::chrono::high_resolution_clock::now();

  for (size_t i = 0; i < iter_count; ++i) {
    fib_data data{n, 0};
    photon::threads_create_join(1, fib, &data);
    auto result = data.sum;
    std::printf("  - %" PRIu64 "\n", result);
  }

  auto endTime = std::chrono::high_resolution_clock::now();
  auto totalTimeUs =
    std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
  std::printf("runs:\n");
  std::printf("  - iteration_count: %" PRIu64 "\n", iter_count);
  std::printf("    duration: %" PRIu64 " us\n", totalTimeUs.count());

  return 0;
}
