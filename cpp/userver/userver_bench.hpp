#pragma once

// Shared engine-bootstrap defaults for the userver benchmarks.
//
// userver is a stackful coroutine framework: work is spawned as engine tasks
// onto a TaskProcessor whose worker threads run tasks on pooled coroutine
// stacks. engine::RunStandalone stands up a temporary engine instance without
// the full component system.

#include <userver/engine/run_standalone.hpp>

#include <cstddef>
#include <thread>

namespace bench {

inline std::size_t default_thread_count() {
  const std::size_t n = std::thread::hardware_concurrency() / 2;
  return n ? n : 1;
}

// The work-stealing scheduler keeps a per-worker local queue that workers
// drain before visiting the global queue, which keeps recursive fork-join
// expansion mostly depth-first. The default (global FIFO queue) scheduler
// expands the task tree breadth-first instead, which parks an unbounded
// number of coroutine stacks in the recursive benchmarks.
inline userver::engine::TaskProcessorPoolsConfig
pools_config(bool work_stealing) {
  userver::engine::TaskProcessorPoolsConfig config;
  // Cache enough coroutine stacks that steady-state task churn reuses stacks
  // instead of mmap/munmap-ing a new one per task.
  config.initial_coro_pool_size = 1024;
  config.max_coro_pool_size = 65536;
  // Shrink the per-coroutine stack far below userver's 256KiB default (16x).
  // The recursive fork-join benchmarks park one live stack per outstanding
  // parent, so the stack size is the dominant memory cost. 16KiB is the
  // smallest safe value: fib inline-recurses fib(n-2) on the coroutine's own
  // stack (~n/2 deep, ~20 frames at fib(39)), and 4KiB/8KiB stack-overflow on
  // that chain; 16KiB clears it with margin while still letting the benches
  // hold far more live tasks per gigabyte. (The recursive benches still DNF at
  // the harness sizes - the unbounded breadth-first task tree exhausts
  // vm.max_map_count long before memory - but they fail an order of magnitude
  // more gracefully, and matmul/channel/io_socket run in a fraction of the RAM.)
  config.coro_stack_size = 16 * 1024;
  config.is_stack_usage_monitor_enabled = false;
  if (work_stealing) {
    config.queue_type = userver::engine::TaskQueueType::kWorkStealingTaskQueue;
  }
  return config;
}

} // namespace bench
