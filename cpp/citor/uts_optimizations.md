# UTS optimizations on citor: what we tried and why it didn't help

This is the citor counterpart to `../TooManyCooks/uts_optimizations.md`. The TMC
write-up documents a sequence of optimizations that drive the per-internal-node
heap allocations to zero and buy a real **−14%**. We ported the same ideas to
citor (`uts_opt1.cpp`, `uts_opt2.cpp`) to see whether they transfer.

**They don't.** On citor the same changes are flat-to-negative. This doc records
the variants, the measurements, and the reason — so nobody re-derives it.

| target     | source         | description                                   |
| ---------- | -------------- | --------------------------------------------- |
| `uts`      | `uts.cpp`      | baseline: `vector<Node>` + `vector<result>`   |
| `uts_opt1` | `uts_opt1.cpp` | pass `Node` by value (drop the node vector)   |
| `uts_opt2` | `uts_opt2.cpp` | stack `std::array` results + batched dispatch |

All three produce the same result (tree size `16526523`), validated against the
constant in `../2common/uts.hpp`.

Measurements: depth-11 geometric tree, 64 worker threads (AMD EPYC 7V73X),
median of 10+ single-iteration runs.

| variant            | median  | vs baseline |
| ------------------ | ------- | ----------- |
| `uts` (baseline)   | 49.3 ms | —           |
| `uts_opt1`         | 50.7 ms | +2.8%       |
| `uts_opt2` (N=32)  | 50.8 ms | +3.0%       |

For reference, the same workload and machine on TMC: 57.9 → 54.5 → 50.5 ms
(−12.8%). citor's *baseline* (49.3 ms) already beats TMC's fully-optimized
variant — there is simply much less allocation overhead left to remove.

## Why the TMC optimizations target something citor doesn't have

The TMC wins come from eliminating heap allocations per internal node. The TMC
baseline does **four** per node: two caller-side vectors (child nodes, child
tasks) plus two *inside* `spawn_many`'s dynamic-count form (a work-item buffer
and a result vector). opt1 removes the two caller vectors; opt2 switches to the
fixed-size `spawn_many<N>` form, whose buffers are stack `std::array`s, removing
the last two.

citor's baseline (`uts.cpp`) does only **two** allocations — `std::vector<Node>
cs` and `std::vector<result> res` — and its dispatch primitive,
`forkJoinAll(n, body)`, is **already allocation-free** for fan-out ≤ 32: it
materializes the task/closure descriptors into a stack buffer
(`kStackTaskBudget = 32`) and only spills to the heap for wider nodes. UTS
fan-out is mean ~5, median 3, P99 ~20, so the overwhelming majority of nodes
already dispatch with zero internal allocations. With tcmalloc, the two caller
vectors that remain are cheap relative to the fork-join rendezvous cost. There is
no allocation-heavy `spawn_many`-style code path for an optimization to reclaim.

## opt1: pass `Node` by value — small regression (+2.8%)

In TMC, taking `Node` by value moves the child into the callee's coroutine frame
*which is allocated anyway*, so the backing `std::vector<Node>` disappears for
free. citor's `uts` is an ordinary synchronous function — there is no per-task
heap frame for the node to ride into. Building each child on the worker stack
inside the `forkJoinAll` body does remove `std::vector<Node>`, but now the full
`Node` (including its SHA1 RNG-state array) is **copied by value** into every
recursive call instead of being filled once in a heap slot. In the synchronous
model that copy is a net add, and it slightly outweighs the saved allocation.

## opt2: stack-array results + batched dispatch — regression (+3.0%), and a trap

opt2 replaces `std::vector<result> res` with a stack `std::array<result, N>`,
which forces children to be spawned in fixed-size batches of `N` (a stack array
can't hold an unbounded fan-out). This is where the port goes wrong.

**Batching serializes wide nodes.** A node with more than `N` children is split
into successive `forkJoinAll` calls, each with its own join barrier. The batches
run *sequentially*. For the rare wide nodes **near the root**, each child gates a
huge subtree, so serializing them throttles whole-tree parallelism. An N-sweep
makes the effect unmistakable:

| N    | median  |
| ---- | ------- |
| 9    | 60.4 ms |
| 16   | 52.1 ms |
| 32   | 50.8 ms |
| 128  | 50.9 ms |

This is the **opposite** of TMC's tuning. In TMC the result array lives in every
internal node's *coroutine frame*, so a large N bloats every (heap-allocated)
frame — the optimum is small (N=9). In citor the array lives on the *worker
stack*, costing nothing per task, so the only penalty that matters is batch
serialization — the optimum is large. We set `N = 32` to match citor's
`kStackTaskBudget`: any fan-out ≤ 32 (well past the P99 ~20) dispatches in a
single batch *and* stays on `forkJoinAll`'s stack path. Even so, opt2 only claws
back to roughly baseline; it never beats it. (Naively reusing TMC's N=9 here
costs +23%.)

## Takeaway

The allocation-elimination strategy only pays off when the runtime's dispatch
primitive is allocation-heavy to begin with. TMC's `spawn_many` (dynamic vs.
fixed-`N`) is; citor's `forkJoinAll` already isn't, so opt1/opt2 have nothing to
reclaim and their mechanics (by-value node copies, batch barriers) make things
marginally worse. The baseline `uts.cpp` is the right citor implementation.

There is also no citor analog to TMC's `uts_opt3` ("write results by reference to
work around the missing coroutine RVO"): citor's body already writes into
`res[i]` directly (`res[i] = uts(...)`), so there is no promise round-trip to
remove.

## Reproducing

From `cpp/citor/`:

```sh
./build_all.sh                      # builds uts, uts_opt1, uts_opt2
```

The N-sweep above was done by editing the `constexpr int N` in `uts_opt2.cpp`,
rebuilding `uts_opt2`, and taking the median of several `./build/uts_opt2 64`
runs.
