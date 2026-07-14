// Helpers to launch a top-level fork-join task on a capy::thread_pool and block
// the calling thread until it completes.
//
// The fork-join benchmarks (fib / skynet / nqueens / matmul) express every
// recursive task as a capy::io_task<T> (an alias for task<io_result<T>>), which
// is what capy::when_all composes. when_all launches each child on the pool's
// shared work queue, so a multi-threaded thread_pool runs siblings in parallel.
//
// capy::run_async posts the top-level task and returns immediately; the handler
// runs on a pool worker when the task completes. We bridge that back to the
// calling thread with a std::promise, which also keeps the pool reusable across
// the warmup and timed runs (unlike thread_pool::join, which is one-shot).

#pragma once

#include <boost/capy/ex/run_async.hpp>
#include <boost/capy/ex/thread_pool.hpp>
#include <boost/capy/io_result.hpp>
#include <boost/capy/io_task.hpp>

#include <future>
#include <tuple>
#include <utility>

namespace capy = boost::capy;

/// Run a value-returning fork-join task on the pool and return its result.
template <class T>
T run_on_pool(capy::thread_pool::executor_type ex, capy::io_task<T> t) {
  std::promise<T> prom;
  auto fut = prom.get_future();
  capy::run_async(ex, [&prom](capy::io_result<T> r) {
    prom.set_value(std::move(std::get<0>(r.values)));
  })(std::move(t));
  return fut.get();
}

/// Run a void fork-join task on the pool and block until it completes.
inline void run_on_pool(capy::thread_pool::executor_type ex, capy::io_task<> t) {
  std::promise<void> prom;
  auto fut = prom.get_future();
  capy::run_async(ex, [&prom](capy::io_result<>) {
    prom.set_value();
  })(std::move(t));
  fut.get();
}
