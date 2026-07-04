# UTS optimizations on libfork: what we tried and why it didn't help

This is the libfork counterpart to `../TooManyCooks/uts_optimizations.md`. The
TMC write-up documents a sequence of optimizations that drive the
per-internal-node heap allocations to zero and buy a real **−14%**. We ported the
same ideas to libfork (`uts_opt1.cpp`, `uts_opt2.cpp`) to see whether they
transfer.

**They don't.** On libfork the same changes land within run-to-run noise. This
doc records the variants, the measurements, and the reason — so nobody
re-derives it.

| target     | source         | description                                       |
| ---------- | -------------- | ------------------------------------------------- |
| `uts`      | `uts.cpp`      | baseline: one `vector<pair>` (Node + result slot) |
| `uts_opt1` | `uts_opt1.cpp` | pass `Node` by value (vector shrinks to results)  |
| `uts_opt2` | `uts_opt2.cpp` | stack `std::array` results + batched fork/join     |

All three produce the same result (tree size `16526523`), validated against the
constant in `../2common/uts.hpp`.

Measurements: depth-11 geometric tree, 64 worker threads (AMD EPYC 7V73X),
median of 10+ single-iteration runs. libfork's three variants sit within ±2%
of each other across repeats, so treat the deltas as noise.

| variant            | median  | vs baseline |
| ------------------ | ------- | ----------- |
| `uts` (baseline)   | 54.1 ms | —           |
| `uts_opt1`         | 54.3 ms | +0.4%       |
| `uts_opt2` (N=9)   | 54.7 ms | +1.1%       |

## Why there's nothing to reclaim

The TMC wins come from eliminating heap allocations per internal node — the TMC
baseline does **four** (two caller vectors plus two inside the dynamic
`spawn_many`).

libfork's baseline (`uts.cpp`) does only **one** allocation per internal node: a
single `std::vector<pair>`, where `pair { result res; Node child; }` holds, for
each child, both its `Node` and the slot the child writes its result into via
`lf::fork[&cs[i].res, uts]`. There is no separate task vector (fork/join encodes
the children directly) and no `spawn_many`-style internal buffer. libfork's real
per-node cost is the **coroutine-frame allocation for each spawned task**, which
is intrinsic to the model and untouched by any of these changes.

## opt1: pass `Node` by value — noise (+0.4%)

Taking `Node` by value moves each child into the callee coroutine's frame (which
libfork allocates anyway), so the backing storage no longer needs to carry
`Node`s and the vector shrinks from `std::vector<pair>` to
`std::vector<result>`. But that is still **one** allocation — the same count as
the baseline, just a smaller one. Unlike TMC (where opt1 deletes two whole
vectors) and citor (where it deletes the node vector), libfork's baseline already
fused node + result into a single allocation, so there is no allocation to
remove here — only bytes to shave. The result is unmeasurable.

## opt2: stack-array results + batched fork/join — noise (+1.1%)

opt2 replaces the `std::vector<result>` with a stack `std::array<result, N>`
living in the coroutine frame, forking children in fixed-size batches of `N`
(`lf::fork` for all but the last in a batch, `lf::call` for the last, then
`lf::join`). This *does* remove the last heap allocation — but it adds a
join barrier per batch for wide nodes, and the saved allocation is dwarfed by the
unavoidable per-task frame allocations. The two roughly cancel.

We swept `N` and small N is best (same direction as TMC, opposite of citor):

| N    | median  |
| ---- | ------- |
| 9    | 52.9 ms |
| 16   | 54.2 ms |
| 32   | 53.5 ms |
| 64   | 53.8 ms |
| 128  | 54.6 ms |

This matches TMC's reasoning: in libfork the `std::array<result, N>` rides in
*every* internal node's coroutine frame, so a larger N inflates every frame
regardless of actual fan-out — small N wins. We keep `N = 9`. But note even the
best N is within noise of the baseline; the sweep mostly confirms there's no
real win to tune toward.

## Takeaway

The allocation-elimination strategy only pays off when the runtime's dispatch
primitive is allocation-heavy to begin with. TMC's `spawn_many` (dynamic vs.
fixed-`N`) is; libfork's fork/join already does a single small bookkeeping
allocation and is otherwise dominated by per-task coroutine frames, which these
changes don't touch. The baseline `uts.cpp` is the right libfork implementation.

There is also no libfork analog to TMC's `uts_opt3` ("write results by reference
to work around the missing coroutine RVO"): libfork already writes results into a
caller-provided slot (`lf::fork[&res[i], uts]`), so there is no promise
round-trip to remove.

## Reproducing

From `cpp/libfork/`:

```sh
./build_all.sh                      # builds uts, uts_opt1, uts_opt2
```

The N-sweep above was done by editing the `constexpr int N` in `uts_opt2.cpp`,
rebuilding `uts_opt2`, and taking the median of several `./build/uts_opt2 64`
runs.
