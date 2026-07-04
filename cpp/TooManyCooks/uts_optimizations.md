# Optimizing the TooManyCooks UTS benchmark

This describes the optimization steps applied to the TooManyCooks (TMC)
implementation of the Unbalanced Tree Search (UTS) benchmark. Four variants are
kept side-by-side so the progression stays reproducible:

| target     | source         | description                                  |
| ---------- | -------------- | -------------------------------------------- |
| `uts`      | `uts.cpp`      | naive baseline                               |
| `uts_opt1` | `uts_opt1.cpp` | pass `Node` by value + lazy-range dispatch   |
| `uts_opt2` | `uts_opt2.cpp` | bounded fixed-size dispatch                  |
| `uts_opt3` | `uts_opt3.cpp` | write results by reference (no promise RVO)  |

All four produce the same result (tree size `16526523`) and are validated
against the precomputed constant in `../2common/uts.hpp`.

Measurements below: depth-11 geometric tree, 64 worker threads, median of 10+
single-iteration runs. (The opt3 row was measured in a later session on an
AMD EPYC 7V73X; the earlier rows match their original measurements, so absolute
numbers across rows are directionally comparable, not from one identical batch.)

| variant            | median  | vs naive |
| ------------------ | ------- | -------- |
| `uts` (naive)      | 57.6 ms | —        |
| `uts_opt1`         | 54.8 ms | −4.9%    |
| `uts_opt2` (N=9)   | 51.1 ms | −11.3%   |
| `uts_opt3` (N=9)   | 49.8 ms | −14.0%   |

## The workload

The tree is UTS preset T1 (geometric, fixed shape, `b=4`) with the depth bumped
to 11: ~16.5M nodes, of which ~3.3M are **internal** (they dispatch a child
group) and ~13.2M are leaves. Per-node fan-out is geometric — mean ~5, median
~3, P99 ~20 — capped at 100 but never near it in practice.

The tree is generated lazily *during* traversal, and every internal node forks a
variable-width group of child tasks. So the benchmark is dominated by (a) the
coroutine-frame allocation each task requires, and (b) the per-node bookkeeping
to set up and join that group. These optimizations target (b): the goal is to
drive the **per-internal-node heap allocations down to zero**.

## Baseline (`uts.cpp`)

```cpp
static tmc::task<result> uts(int depth, Node* parent) {
  ...
  std::vector<Node> cs(num_children);                 // (1)
  std::vector<tmc::task<result>> tsk(num_children);   // (2)
  for (int i = 0; i < num_children; i++) {
    cs[i] = ...;                  // fill child node + RNG state
    tsk[i] = uts(depth + 1, &cs[i]);
  }
  auto results = co_await tmc::spawn_many(tsk.data(), num_children);  // (3)(4)
  // reduce results...
}
```

Each internal node performs **four** heap allocations:

1. `std::vector<Node>` holding the child nodes (kept alive while the group runs),
2. `std::vector<task>` holding the child tasks,
3. inside `spawn_many` (dynamic form): a work-item buffer (`std::vector`),
4. inside `spawn_many` (dynamic form): a result buffer (`std::vector`).

Across ~3.3M internal nodes that is ~13M allocations per traversal.

## opt1: pass `Node` by value, generate tasks with a lazy range

Two changes, each removing one of the caller-side vectors.

**`Node` by value.** Changing the signature from `uts(int, Node*)` to
`uts(int, Node)` moves ownership of each child node *into the callee coroutine's
frame*. TMC already allocates that frame, so the child `Node` rides along for
free — there is no longer any need for a backing array to keep child nodes alive
while the group executes. This removes allocation (1).

**Lazy task range.** Instead of materializing a `std::vector<task>`, the children
are produced on demand by a range and handed straight to `spawn_many`:

```cpp
auto children = std::ranges::views::iota(0, num_children) |
                std::ranges::views::transform([&](int i) {
                  Node child;
                  child.type = child_type;
                  child.height = parent.height + 1;
                  child.numChildren = -1;
                  for (int j = 0; j < computeGranularity; j++)
                    rng_spawn(parent.state.state, child.state.state, i);
                  return uts(depth + 1, child);   // task constructed on deref
                });

auto results = co_await tmc::spawn_many(children);
```

`spawn_many` pulls each task as it walks the range, so the task is built directly
into the dispatch machinery — no intermediate vector. This removes allocation (2).

`spawn_many` is still given a *dynamic* count, so internally it allocates a
work-item buffer and a result `std::vector` — allocations (3) and (4) remain.

**Net:** 4 → 2 allocations per internal node. **57.6 → 54.8 ms (−4.9%).**

## opt2: bounded fixed-size dispatch

`spawn_many` has a fixed-count form, `spawn_many<N>(begin, end)`, that returns
results in a **stack `std::array<result, N>`** and likewise uses a stack array
for its internal work-item buffer. Switching to it eliminates the last two
(internal) allocations — dispatch becomes fully allocation-free.

Because `N` is a compile-time bound, a node with more than `N` children is
handled in successive batches:

```cpp
constexpr int N = 9;

auto children = /* same iota | transform as opt1 */;

auto it  = children.begin();
auto end = children.end();
for (int remaining = num_children; remaining > 0;) {
  int batch = remaining < N ? remaining : N;

  std::array<result, N> res = co_await tmc::spawn_many<N>(it, end);

  for (int k = 0; k < batch; k++)
    /* reduce res[k] into r */;

  it += batch;          // advance manually — see note below
  remaining -= batch;
}
```

Two implementation details that make this correct:

- **`spawn_many` does not advance the caller's iterator by reference.** It stores
  the iterator by value and advances an internal copy. So we advance `it`
  ourselves by the batch size between rounds.
- **A short final batch is safe.** The `(begin, end)` sentinel form stops at
  `end`, so the last round spawns only what remains and leaves the unused array
  tail default-initialized; we only ever read `res[0 .. batch)`.

**Net:** 2 → 0 allocations per internal node. **54.8 → 51.1 ms (−11.3% vs naive).**

### Choosing N

`N` balances two opposing costs:

- The `std::array<result, N>` lives in **every internal node's coroutine frame**,
  so a larger `N` inflates every frame by `N × sizeof(result)` (24 bytes each)
  regardless of the node's actual fan-out — and most fan-outs are small (median
  3). Big arrays are mostly wasted capacity that bloats frames and cache.
- A too-small `N` forces wide nodes into multiple **sequential** fork/join
  rounds, serializing work that could otherwise run in parallel.

A low-variance sweep (`iter_count=4`) located the optimum at **N=9**: a steep
penalty below ~6 (N=4 is ~80% slower), a flat basin across N≈7–10, minimum at 9.
The optimum sits well *below* the P99 fan-out of ~20 — frame size matters more
than avoiding the occasional extra round.

## opt3: write results by reference (work around the missing coroutine RVO)

opt2 still has each child task *return* its `result`: the child does `co_return
r`, and `spawn_many<N>` gathers the N returned values into the stack
`std::array<result, N>`. Coroutines have no RVO — the return value cannot be
constructed in place at the call site — so each child's `result` is moved
**twice**:

1. `co_return r` move-constructs `r` into the child's promise, then
2. `spawn_many` moves it back out of the promise into the result array.

opt3 removes both moves by handing each child a reference to its destination and
having it write the result in place. `uts` returns `tmc::task<void>` and takes a
`result* out`; the array is created *before* the spawn:

```cpp
static tmc::task<void> uts(int depth, Node parent, result* out) {
  result r(depth, 1, 0);
  ...
  if (num_children > 0) {
    constexpr int N = 9;
    std::array<result, N> res;        // created up front
    int base = 0;                     // start index of the current batch

    auto children = /* same iota | transform as opt2, but: */
      ... return uts(depth + 1, child, &res[i - base]);   // child writes its slot

    for (int remaining = num_children; remaining > 0;) {
      int batch = remaining < N ? remaining : N;
      co_await tmc::spawn_many<N>(it, end);   // void tasks: just join
      for (int k = 0; k < batch; k++) /* reduce res[k] into r */;
      it += batch; remaining -= batch; base += batch;
    }
  } else {
    r.leaves = 1;
  }
  *out = r;                           // write our own result by reference too
}
```

This is the model citor (`res[i] = uts(...)`) and libfork (`lf::fork[&res[i],
uts]`) already use natively — they thread a result destination into the child
rather than returning a value, so they had nothing analogous to gain.

Two details that make it correct:

- **`spawn_many<N>` over `task<void>` just joins.** When the task's result type
  is `void`, the awaitable yields nothing (no array, no vector); awaiting it only
  waits for the batch to retire. Results are already in `res` by then.
- **The `i - base` slot mapping is safe.** `spawn_many<N>` dereferences (and thus
  constructs) every task in a batch synchronously, before the `co_await`
  suspends and before we advance `base`. So at deref time `i - base` is the
  child's slot in the current batch's array. The leaf branch
  (`num_children == 0`) has no `co_await`, but the function is still a coroutine
  (the `spawn_many` in the other branch makes it one); it falls through to
  `*out = r`.

**Net:** ~6.6M `result` moves removed across ~3.3M internal nodes (2 per child).
**51.1 → 49.8 ms (−1.7% vs opt2, −14.0% vs naive).** A `result` is only 24
bytes, so the per-child saving is tiny; the gain is small but consistent (the
whole run-to-run distribution shifts down, not just the median).

## Reproducing

From `cpp/TooManyCooks/`:

```sh
./build_all.sh                      # builds uts, uts_opt1, uts_opt2, uts_opt3
./sweep_n.sh                        # N = 8..32 (coarse)
./sweep_n_lowend.sh                 # N = 4..10, iter_count=4 (low variance)
```

Each sweep script edits the `N` constant in `uts_opt2.cpp`, rebuilds only that
target, benchmarks it, and restores the file when done.
