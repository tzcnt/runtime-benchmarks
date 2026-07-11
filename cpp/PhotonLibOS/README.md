# PhotonLibOS

[PhotonLibOS](https://github.com/alibaba/PhotonLibOS) is a **stackful** coroutine
("photon thread") framework with a **thread-per-core** scheduler and **no work
stealing**. Each vcpu (OS thread) runs its own photon threads. The benchmarks
fork work with `bench::thread` (see `photon_bench.hpp`): a photon thread is
created on the current vcpu and migrated round-robin onto a pool vcpu, then
joined cross-vcpu — the fork-join analog of the other runtimes' spawn/join.

## Benchmark status

| Benchmark | Result | Notes |
| --- | --- | --- |
| `matmul` | ✅ pass | ~15 MB |
| `channel` | ✅ pass | ~13 MB |
| `io_socket_st` | ✅ pass | ~11 MB |
| `fib` | ❌ **DNF** | runs out of RAM |
| `skynet` | ❌ **DNF** | runs out of mappings |
| `nqueens` | ❌ **DNF** | runs out of RAM |

The three recursive fork-join benchmarks (`fib`, `skynet`, `nqueens`) do not
finish. This is a property of Photon's scheduler, not a bug in the benchmarks,
and it is **not** fixable by tuning the coroutine stack size (see below).

## Why the recursive fork-join benchmarks DNF

### 1. FIFO migration queues → breadth-first tree expansion

`fib`/`skynet`/`nqueens` build a tree of tasks: each task forks its children and
then blocks (`join`) until they complete. How much memory this needs depends
entirely on the scheduling order.

A **depth-first** runtime (an owner-LIFO work-stealing deque, as in TooManyCooks,
libfork, TBB, etc.) runs the most-recently-forked child first, so a forked child
usually finishes before its siblings are even started. The number of
*simultaneously live* tasks stays bounded — roughly `O(depth × threads)`.

Photon has neither an owner-LIFO deque nor work stealing. A forked photon thread
is pushed onto an **unbounded, FIFO** migration queue for a pool vcpu, and
nothing biases execution toward the newest task. The runtime therefore runs the
*oldest* ready task first and traverses the tree **breadth-first**: it eagerly
expands an entire level before collapsing any of it. The number of
simultaneously live (forked-but-not-yet-joined) photon threads grows with the
*width* of the tree, which is exponential in the depth — millions for `fib(39)`,
~10⁸ for `skynet`.

> Photon's `WorkPool::async_call` API is *not* used for forks: its shared task
> ring is bounded (65536 entries) and senders yield-spin when it is full, so
> unbounded fork recursion **livelocks** (flat RSS, no progress) instead of
> failing cleanly. `bench::thread` uses the migration path, which fails fast.

### 2. Every live task pins a coroutine stack

Each live task is parked mid-execution (blocked in `join`), so it holds a live
coroutine stack. Breadth-first expansion requires millions of stacks resident at
once. A stackless runtime parks a live task in a few hundred bytes of heap; a
stackful runtime like Photon parks it in a full coroutine stack — which is why
this workload is uniquely punishing for it.

### 3. Millions of live stacks exhaust RAM *or* `vm.max_map_count`

The tree is so wide that the pool of live stacks exhausts a hard system
resource. *Which* resource it hits first depends on the benchmark's allocation
pattern:

- **`fib` and `nqueens` run out of RAM.** Photon's pooled stack allocator
  (`use_pooled_stacks()`) hands out stacks from large slabs, so the *mapping*
  count stays low and the *byte* count is the binding constraint. `fib` scales
  ~2.45× per `+2` in `n` (`fib(32)` = 16 GB, `fib(34)` = 41 GB, `fib(36)` is
  OOM-killed); `fib(39)` extrapolates to hundreds of GB. `nqueens(14)` blows
  past 72 GB. On a 125 GB machine both exhaust RAM and are killed.

- **`skynet` runs out of memory *mappings*.** Linux caps the number of distinct
  virtual memory areas (VMAs, i.e. `mmap` regions) per process —
  `vm.max_map_count`, which is `1048576` on the benchmark machine. `skynet`'s
  10-way fan-out fragments the per-vcpu stack pools into a huge number of
  mappings, and it stalls at **exactly** the ceiling:

  ```
  skynet, 16 KiB stacks, 64 threads:
    reaches 1,048,576 mappings  (== vm.max_map_count)  at only ~4 GB RSS
  ```

  When the next stack's `mmap` is refused (`ENOMEM`), tcmalloc reports
  `tcmalloc: allocation failed 16384`, Photon dereferences the null stack
  pointer, and the process dies with `SIGSEGV`.

Either way the harness records the run as a DNF.

## Stack size: reduced to 16 KiB, but it cannot fix the DNF

`photon_bench.hpp` creates every fork with a 16 KiB stack (`kForkStackSize`),
down from Photon's **8 MiB** default (`photon_std::thread`'s
`DEFAULT_STACK_SIZE`) — a 512× cut. 16 KiB is Photon's **hard floor**:
`thread_create` rounds any request up to `align_up(max(16 KiB, …), PAGE)` and
logs a warning per thread below it, so a smaller request (e.g. 4 KiB) is
pointless — it rounds back up to 16 KiB and floods stderr.

> Photon's `photon_std::thread` hardcodes the 8 MiB stack and migrates through a
> work pool kept in a file-static, unreachable through its public API. So
> `photon_bench.hpp` defines `bench::thread` (a `photon_std::thread` work-alike)
> that owns its own `photon::WorkPool` and creates forks via
> `thread_create11(kForkStackSize, …)`.

Shrinking the stack lowers the byte cost of each parked task, but it does not
reduce the *number* of live tasks (or their mappings), which is what the
breadth-first tree exhausts. So the reduction does not change the DNF verdict —
it only makes the recursive benchmarks fail at ~GB scale instead of ballooning
toward TB scale (much safer for the benchmark machine, and far less likely to
trigger the system OOM-killer mid-sweep), and it lets the passing benchmarks
(`matmul`/`channel`/`io_socket_st`) run in a fraction of the RAM.

Making these benchmarks pass would require capping fork concurrency and running
excess forks inline (depth-first). Photon provides no such primitive (`throttle`
is a bandwidth rate-limiter, not a concurrency cap), and per repo policy the
benchmarks do not hand-roll one — they use the runtime's idiomatic fork-join and
DNF where the runtime cannot sustain it.

## Contrast

[HPX](../HPX) runs the same recursive benchmarks and **passes** them, despite
also being stackful: its scheduler is parent-first work-stealing, which keeps the
live task count bounded and never approaches either limit. userver
([../userver](../userver)) is likewise stackful with FIFO queues and hits the
same breadth-first wall (in its case the `vm.max_map_count` ceiling, at ~970k
mappings / 2.8 GB for `fib(39)`).
