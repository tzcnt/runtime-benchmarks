# userver

[userver](https://github.com/userver-framework/userver) is a **stackful**
coroutine framework. Work is spawned as engine tasks (`engine::AsyncNoTracing`)
onto a `TaskProcessor` whose worker threads run each task on its own pooled
coroutine stack; `TaskWithResult::Get()` joins a task, parking the caller.

## Benchmark status

| Benchmark | Result | Peak RSS |
| --- | --- | --- |
| `matmul` | ✅ pass | ~69 MB |
| `channel` | ✅ pass | ~18 MB |
| `io_socket_st` | ✅ pass | ~18 MB |
| `fib` | ❌ **DNF** | aborts ~2.8 GB |
| `skynet` | ❌ **DNF** | aborts ~40 GB |
| `nqueens` | ❌ **DNF** | aborts ~3 GB |

The three recursive fork-join benchmarks (`fib`, `skynet`, `nqueens`) do not
finish. This is a property of userver's scheduler, not a bug in the benchmarks,
and it is **not** fixable by tuning the coroutine stack size (see below).

## Why the recursive fork-join benchmarks DNF

### 1. FIFO scheduler → breadth-first tree expansion

`fib`/`skynet`/`nqueens` build a tree of tasks: each task forks its children
and then blocks (`Get()`) until they complete. How much memory this needs
depends entirely on the scheduling order.

A **depth-first** runtime (an owner-LIFO work-stealing deque, as in TooManyCooks,
libfork, TBB, etc.) runs the most-recently-forked child first, so a forked child
usually finishes before its siblings are even started. The number of
*simultaneously live* tasks stays bounded — roughly `O(depth × threads)` — and
the tree is consumed almost as fast as it is produced.

userver's task queues are **FIFO** (`kGlobalTaskQueue`, and even the experimental
`kWorkStealingTaskQueue`, are first-in-first-out with no owner-LIFO deque). A
FIFO queue runs the *oldest* ready task first, so every task at depth *d* is
started before any task at depth *d+1*. The tree is traversed
**breadth-first**: the runtime eagerly expands an entire level before
collapsing any of it. The number of simultaneously live (forked-but-not-yet-
joined) tasks grows with the *width* of the tree, which is exponential in the
depth — millions of tasks for `fib(39)`, ~10⁸ for `skynet`.

### 2. Every live task pins a coroutine stack

Because each live task is parked mid-execution (blocked in `Get()`), it is
holding a live coroutine stack. Breadth-first expansion therefore requires
millions of stacks resident at once. This is why a stackful runtime is uniquely
punished here: a stackless runtime parks a live task in a few hundred bytes of
heap, but a stackful runtime parks it in a full coroutine stack.

### 3. Live stacks exhaust `vm.max_map_count`, not RAM

The surprising part: the benchmarks do **not** die from running out of memory.
They die because Linux caps the number of distinct virtual memory areas (VMAs,
i.e. `mmap` regions) a process may have — `vm.max_map_count`, which is
`1048576` on the benchmark machine.

Each pooled coroutine stack is an `mmap`'d region, and userver protects it with
a guard page (an `mprotect` that splits the region), plus per-task metadata
spans — measured at **~5.5 VMAs per coroutine**. So the mapping count, not the
byte count, is the binding constraint:

```
fib(39), 16 KiB stacks, 64 threads:
  aborts at 969,516 mappings  (ceiling: 1,048,576)
  RSS at that point:  ~2.8 GB   (machine has 125 GB)
```

When a new stack's `mmap` pushes past `vm.max_map_count` it returns `ENOMEM`;
the allocator (tcmalloc) reports `tcmalloc: allocation failed 8192`, userver
throws `std::bad_alloc`, and — with no handler in the fork path — `std::terminate`
aborts the process (`SIGABRT`, exit 134). The harness records the run as a DNF.

## Stack size: reduced to 16 KiB, but it cannot fix the DNF

`userver_bench.hpp` sets `coro_stack_size = 16 * 1024` (down from userver's
256 KiB default — a 16× cut). 16 KiB is the *smallest safe* value: `fib`
inline-recurses `fib(n-2)` on the coroutine's own stack (~n/2 ≈ 20 frames at
`fib(39)`), and 4 KiB or 8 KiB stacks overflow that chain and crash even on the
`fib(30)` warmup.

Shrinking the stack lowers the byte cost of each parked task, but the failure is
bound by the **number of mappings**, which is independent of stack size. So the
reduction does not change the DNF verdict — it only makes the recursive
benchmarks fail at ~GB scale instead of ballooning toward TB scale (much safer
for the benchmark machine), and it lets the passing benchmarks
(`matmul`/`channel`/`io_socket_st`) run in a fraction of the RAM.

Making these benchmarks pass would require capping fork concurrency and running
excess forks inline (depth-first). userver provides no such primitive
(`CancellableSemaphore` is a plain counting semaphore with no inline fallback,
and the work-stealing queue is still FIFO), and per repo policy the benchmarks
do not hand-roll one — they use the runtime's idiomatic fork-join and DNF where
the runtime cannot sustain it.

## Contrast

[HPX](../HPX) runs the same recursive benchmarks and **passes** them, despite
also being stackful: its scheduler is parent-first work-stealing
(`assign_work_thread_parent` + `steal_after_local`), which keeps the live task
count bounded and never approaches the VMA limit. PhotonLibOS
([../PhotonLibOS](../PhotonLibOS)) is thread-per-core with unbounded migration
queues and hits the *identical* `vm.max_map_count` wall.
