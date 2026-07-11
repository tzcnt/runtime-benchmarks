#pragma once

// Shared engine-bootstrap defaults for the PhotonLibOS benchmarks.
//
// Photon is a stackful coroutine ("photon thread") framework with a
// thread-per-core scheduler: each vcpu (an OS thread) runs its own photon
// threads and there is no work stealing between vcpus. Multi-core execution
// uses a photon::WorkPool: work_pool_init stands up N worker vcpus, and every
// bench::thread is created on the spawning vcpu and immediately migrated to a
// pool vcpu (round-robin), so bench::thread + join() gives fork-join semantics
// comparable to the other runtimes' spawn/join primitives (joins are safe
// across vcpus).
//
// bench::thread mirrors photon_std::thread (Photon's std::thread-compatible
// API) but creates its photon thread with a small stack (see kForkStackSize)
// instead of photon_std::thread's fixed 8MiB DEFAULT_STACK_SIZE, and migrates
// through a WorkPool we own directly (photon_std keeps its WorkPool in a
// file-static, so the stack size and pool are not reachable through that API).
//
// The migration queues are unbounded and nothing biases execution
// depth-first, so unbounded recursive fork-join expands the task tree
// breadth-first, parking every started parent on a coroutine stack until
// its children complete. Photon provides no primitive to bound the live
// task count, so the recursive benchmarks may exhaust memory and DNF - the
// same policy applied to userver's FIFO schedulers. Shrinking the per-thread
// stack (below) lets far more parents be parked before that happens.

#include <photon/common/alog.h>
#include <photon/photon.h>
#include <photon/thread/stack-allocator.h>
#include <photon/thread/thread.h>
#include <photon/thread/thread11.h>
#include <photon/thread/workerpool.h>

#include <cstddef>
#include <cstdint>
#include <exception>
#include <thread>
#include <utility>

namespace bench {

inline std::size_t default_thread_count() {
  const std::size_t n = std::thread::hardware_concurrency() / 2;
  return n ? n : 1;
}

// Per-fork photon-thread stack size. Photon's default is 8MiB, which makes the
// recursive fork-join benchmarks exhaust memory once thousands of parents are
// parked. Photon enforces a hard floor of 16KiB: thread_create rounds the
// requested size up to align_up(max(16KiB, ...), PAGE_SIZE), and logs a WARN
// per created thread when the request is below that. So 16KiB is the smallest
// value that actually shrinks the allocation without flooding stderr; the
// benchmark payloads (shallow fib/skynet/nqueens recursion, flat channel and
// divide-and-conquer matmul workers) fit within it.
inline constexpr std::uint64_t kForkStackSize = 16 * 1024;

// The WorkPool created by run_in_pool; bench::thread migrates newly created
// photon threads onto its worker vcpus. Non-null only for the duration of
// run_in_pool.
inline photon::WorkPool* g_work_pool = nullptr;

// A photon_std::thread work-alike with a configurable (small) stack size.
// Each instance owns exactly one join-enabled photon thread; construction
// spawns it and migrates it round-robin onto a pool vcpu, and join() waits
// for it to finish. Move-only, like std::thread / photon_std::thread.
class thread {
public:
  thread() = default;

  ~thread() {
    if (joinable()) {
      std::terminate();
    }
  }

  thread(const thread&) = delete;
  thread& operator=(const thread&) = delete;

  thread(thread&& other) noexcept : m_th(other.m_th) { other.m_th = nullptr; }

  thread& operator=(thread&& other) noexcept {
    if (joinable()) {
      std::terminate();
    }
    m_th = other.m_th;
    other.m_th = nullptr;
    return *this;
  }

  template <typename Function, typename... Args>
  explicit thread(Function&& f, Args&&... args) {
    m_th = photon::thread_create11(
      kForkStackSize, std::forward<Function>(f), std::forward<Args>(args)...
    );
    photon::thread_enable_join(m_th, true);
    if (g_work_pool) {
      g_work_pool->thread_migrate(m_th);
    }
  }

  bool joinable() const { return m_th != nullptr; }

  void join() {
    photon::thread_join(reinterpret_cast<photon::join_handle*>(m_th));
    m_th = nullptr;
  }

private:
  photon::thread* m_th = nullptr;
};

// Photon's logger writes to stdout by default, which would corrupt the YAML
// results that build_and_bench_all.py parses from the benchmark's stdout.
// Route diagnostics to stderr and keep only warnings and above. Must be
// called before photon::init / work_pool_init, which log at DEBUG level.
inline void quiet_logs() {
  set_log_output(log_output_stderr);
  set_log_output_level(ALOG_WARN);
}

// Fork-heavy benchmarks create and destroy photon threads constantly; the
// pooled stack allocator recycles stacks instead of mmap/munmap-ing a fresh
// region per thread. Must be called before the work pool is created so
// every worker vcpu picks it up. The trim threshold of -1 keeps recycled
// stacks pooled for the whole run (mirrors Photon's own workpool-perf
// example).
inline void use_pooled_stacks() {
  photon::use_pooled_stack_allocator();
  photon::pooled_stack_trim_threshold(-1UL);
}

// Stand up the photon environment (the main thread becomes a vcpu plus
// `thread_count` pool vcpus) and run `fn` to completion on a pool vcpu (the
// root bench::thread migrates there on creation). The main vcpu only blocks
// in join, so `thread_count` OS threads execute benchmark work - the analog
// of userver's engine::RunStandalone. The CPU benchmarks need no fd polling,
// so no event engine is requested; cross-vcpu wakeups still work through each
// vcpu's default engine.
template <typename F> void run_in_pool(std::size_t thread_count, F&& fn) {
  photon::init(photon::INIT_EVENT_NONE, photon::INIT_IO_NONE);
  g_work_pool = new photon::WorkPool(
    thread_count, photon::INIT_EVENT_NONE, photon::INIT_IO_NONE, -1
  );
  {
    bench::thread root(std::forward<F>(fn));
    root.join();
  }
  delete g_work_pool;
  g_work_pool = nullptr;
  photon::fini();
}

} // namespace bench
