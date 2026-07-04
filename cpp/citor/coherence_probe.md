# How citor measures core-to-core latency, and what it does with it

citor's `ThreadPool` constructor runs a one-time **coherence probe**: it measures
the cache-line round-trip latency between every pair of CPUs the pool will run
on, clusters the CPUs by that measured latency, and hands the result to the
topology-aware primitives. This is the ~4 ms/worker startup cost noted in
`uts_optimizations.md` — this document explains what that cost buys.

All code references are to the vendored copy under
`build/_deps/citor-src/include/citor/`:
`detail/coherence_probe.h` (measurement + clustering) and `thread_pool.h`
(consumption).

---

## 1. Why measure it at all

On a multi-die CPU (AMD EPYC/Ryzen CCDs, Intel tile/cluster parts, multi-socket
NUMA), not all "cores" are equally close. Two cores sharing an L3 can ping-pong a
cache line in ~30-50 ns; two cores on different dies pay a trip across the
fabric/IO-die at ~150-300 ns — often **5-10×** slower. A work-stealing scheduler
that ignores this will happily steal across the fabric and thrash, and a
parallel scan/reduce that splits work evenly will bottleneck on whichever
cluster is farthest from the data.

citor could read this structure from sysfs (which CPUs share an L3), and it does
keep that as a *prior*. But sysfs tells you the topology, not the **cost ratio**,
and it is frequently wrong or absent in containers, VMs, and CI runners. The
probe measures the real numbers on the real silicon the process is pinned to.

The host this was characterized on (AMD EPYC 7V73X, 64 cores / 128 threads,
8 CCDs of 8 cores each sharing a large V-Cache L3) is exactly the case the probe
targets.

---

## 2. Measuring one pair accurately: `pingPongLatencyNs`

`pingPongLatencyNs(cpuA, cpuB, roundTrips=1024)` (coherence_probe.h:137) measures
the round-trip latency between two specific CPUs. The technique is the standard
core-to-core ping-pong (cf. nviennot/core-to-core-latency, ChipsandCheese), with
several details that matter for accuracy:

**One shared cache line, parity hand-off.** A single `std::atomic<uint64_t>
counter` is shared by both threads. The convention: when the counter is **even**
it is "thread A's turn"; A stores `counter+1` (now odd) and spins until it sees
it go even again. The helper on cpuB does the mirror: when it sees odd, it stores
`counter+1` (now even). Each full cycle is one cache-line transfer in each
direction — exactly one core-to-core round-trip:

```cpp
// measuring thread, pinned to cpuA:
const std::uint64_t observed = counter.load(acquire);
counter.store(observed + 1, release);            // hand off to B (odd)
while ((counter.load(acquire) & 1ULL) == 1ULL)   // wait for B to take it (even)
  cpuRelax();
```

**Both threads are pinned.** The helper pins itself to `cpuB`
(`coherenceProbePin`, coherence_probe.h:108, via `pthread_setaffinity_np`); the
measuring thread pins to `cpuA`. Without hard pinning the kernel could migrate
either thread and you'd measure a random pair, not the intended one.

**No false sharing.** `counter`, the `ready` handshake flag, and the `stop` flag
are each `alignas(kCacheLine)` (coherence_probe.h:139-141), so they sit on
separate cache lines. Otherwise the handshake/stop traffic would contend with the
line being measured and inflate the reading.

**Warmup before timing.** 64 untimed round-trips run first
(coherence_probe.h:177) to settle the shared line into the steady-state coherence
protocol and let the helper's first scheduler dispatch retire, so the timed
window isn't polluted by cold-start effects.

**Mean over many round-trips.** Only after warmup does it take `steady_clock`
timestamps around `roundTrips` (1024) iterations and return
`totalNs / roundTrips` (coherence_probe.h:214). The loop body is a single
atomic RMW that dominates; averaging over 1024 trips lets occasional interrupts
average out. (The matrix-level code calls this the "median round-trip"; per pair
it is a mean, then medians are taken across *pairs* during clustering.)

**Caller affinity is restored.** The caller's CPU mask is captured on entry and
restored on exit (coherence_probe.h:144-147, 204-212), so probing doesn't leave
the constructor thread stranded on some probe CPU.

**Clean teardown.** After timing, `stop` is set and the counter is flipped once
more so the helper advances past its spin, observes `stop`, and exits — no
detached threads, no leak (coherence_probe.h:195-202).

---

## 3. Measuring the whole matrix fast: disjoint-pair round-robin

A naive all-pairs probe is O(N²) pair-measurements, each taking ~1024 round-trips
— for 64 CPUs that's ~2000 sequential probes. citor compresses this to **O(N)
wall time** by observing that disjoint pairs can be measured *simultaneously*:
if (a,b) and (c,d) share no CPU, their ping-pongs don't contend, so run them in
parallel.

`roundRobinPairs(n)` (coherence_probe.h:226) generates a schedule where every
pair (i,j) appears exactly once — the edge set of the complete graph K_N — using
the **circle method** for round-robin tournament scheduling: fix one element,
rotate the rest by one each round. For `N` participants this yields `N-1` rounds
(odd `N` gets a "bye" slot), each round a perfect matching of `N/2` disjoint
pairs.

`probeLatencyMatrix` (coherence_probe.h:267) then runs each round by spawning one
`std::thread` per pair and joining them before the next round
(coherence_probe.h:292-316). Total wall time ≈ `(N-1) × single-pair-probe-time +
thread-spawn overhead` — which is exactly the **linear ~4 ms/worker** scaling
measured at construction. The output is a symmetric `LatencyMatrix`
(coherence_probe.h:58): `matrix[i][j]` = latency between `cpus[i]` and `cpus[j]`,
diagonal defined as 0.

> This is why the probe dominates pool construction (≈270 ms at 64 threads) yet
> never touches the steady-state benchmark number: it runs once, before any work.

---

## 4. From a latency matrix to coherence clusters

Raw latencies are noisy and host-specific; the consumers want a **partition** of
CPUs into "close" groups plus a single cost ratio. `clusterByLatency`
(coherence_probe.h:442) derives both, parameter-free:

1. **Log-space histogram + Otsu's threshold.** All off-diagonal latencies are
   histogrammed in **log space** (64 bins) and `otsuThresholdLog`
   (coherence_probe.h:351) finds the threshold that maximizes between-class
   variance — the classic image-binarization method, here separating "fast/same
   cluster" from "slow/cross cluster" edges. Log space makes it scale-invariant
   (a 40 ns vs 240 ns split looks the same as 80 vs 480).

2. **Trust gate.** Otsu always returns *a* threshold even on unimodal data, so
   the result is only used if the histogram is genuinely bimodal: a bimodality
   score (between-class / total variance) `≥ 0.55` (the textbook 5/9 cutoff) and
   `≥ 10` off-diagonal pairs (≥ 5 CPUs), because tiny samples produce spurious
   splits from jitter (coherence_probe.h:481-486).

3. **Connected components.** If the gate passes, build a graph with an edge for
   every pair whose `log(latency) ≤ threshold` and take connected components via
   union-find (coherence_probe.h:487-524). Each component is one coherence
   cluster (≈ one CCD / L3 domain).

4. **Fallback to the sysfs prior.** If the histogram is unimodal (e.g. a
   single-CCX consumer chip, where every pair is equally fast), the measured
   split is untrustworthy, so it falls back to the per-L3 CPU grouping from
   `detail::detectTopology()` (coherence_probe.h:525-554). Measurement refines
   the topology; it never blindly overrides a sane prior.

5. **Cluster distance matrix + ratio scalar.** It computes the median pairwise
   latency within and between clusters (`clusterDistanceNs`,
   coherence_probe.h:560-584) and a convenience scalar
   `maxCrossOverIntraRatio = max cross-cluster median / max intra-cluster median`
   (coherence_probe.h:609-625) — the single number "how much more expensive is a
   far access than a near one," = 1.0 on a single-cluster host.

**Caching.** `cachedCoherenceProbe` (coherence_probe.h:634) memoizes the whole
result in a process-wide map keyed on the sorted CPU set. The matrix depends only
on the hardware, so test suites, bench harnesses, and `PoolGroup` arenas that
build many pools pay the probe once. (A single benchmark process builds one pool,
so this doesn't help the startup cost we measured — but it does mean a long-lived
program that recreates pools isn't re-probing.)

---

## 5. How the result drives dispatch and stealing

Here the important — and slightly non-obvious — distinction: **fork/join work
stealing uses the sysfs CCD prior, while the parallel scan/reduce family uses the
measured probe.**

### 5a. Fork/join stealing — sysfs CCD groups, probabilistic same-CCD bias

The work-stealing victim lists are built from `m_ccdOfSlot`, which comes from
`m_topology.ccdOfCpu` (sysfs), not from the latency matrix. At construction each
slot gets three precomputed victim rings (thread_pool.h:549-576):
`m_sameCcdVictims`, `m_crossCcdVictims`, `m_allVictims`. Precomputing slot
indices removes a modulo and a CCD comparison from the steal hot path.

`trySteal` (thread_pool.h:5194) then, under the default `ClusterLocal` steal
policy, probes **one** random victim per call with a **7/8 same-CCD, 1/8
cross-CCD** probability bias (thread_pool.h:5219-5243):

```cpp
const bool tryCross = (xorshiftNext(rng) & 0x7U) == 0U;   // 1 in 8
// 7/8: steal from a random same-CCD victim; 1/8: reach across the fabric
```

The design notes (thread_pool.h:5202-5213) explain why it's a *probability* bias
rather than an exhaustive same-CCD sweep: idle workers chasing a small root set
would otherwise burn most of their CPU loading empty same-CCD deques before ever
reaching across, while the 1/8 cross-CCD escape valve guarantees a worker on a
starved CCD still finds work. Net effect: stolen tasks stay L3-local most of the
time (cheap re-steal, warm data), with a guaranteed path to remote work.

So fork/join — and therefore the UTS benchmark — gets **CCD-locality from the
sysfs topology**; it does not consume the measured nanosecond matrix directly.
The probe's value to fork/join is indirect: it validates/refines the cluster
structure the rest of the engine trusts.

### 5b. Parallel scan / reduce — the measured matrix earns its keep

`initScanScratch` (thread_pool.h:~795-914) precomputes, once at construction, two
things the scan/reduce hot path reads as O(1) fields:

**Asymmetric chunk partitioning.** When the pool's slots span more than one CCD,
the producer's CCD is given a *larger* share of the iteration space, because
cross-CCD slots pay more to touch producer-resident data. The bias factor is the
measured `maxCrossOverIntraRatio` (falling back to 2.0 if unavailable),
thread_pool.h:819-834:

```cpp
biasFactor   = m_coherenceProbe.maxCrossOverIntraRatio;        // measured ratio
producerWeight = round(biasFactor * slotsOnProducerCcd);
asymmetricNum  = clamp(16 * producerWeight / totalWeight, 9, 15);  // out of 16
```

The producer-CCD slots get `asymmetricNum/16` of the work; the more expensive the
fabric (higher measured ratio), the more work stays local. This is the place the
actual nanoseconds — not just the topology — change the schedule.

**Hierarchical clustered reduce.** When the probe found ≥ 2 contiguous clusters
and the producer is in cluster 0 (thread_pool.h:846-913, gated to avoid spurious
small-sample clusters), the scan switches to a two-level reduction
(thread_pool.h:4247-4280): each cluster's leader reduces its own slots locally,
then leaders exchange **one cluster-total cache line per cluster pair** across the
fabric, instead of every slot pinging every other slot. This bounds inter-cluster
line transit to one transfer per cluster pair — the single biggest lever on a
high-cross-latency machine, and only safe to attempt because the probe confirmed
the clusters are real.

### 5c. Producer auto-pin (first-touch placement)

Right after the probe, the constructor pins itself (the producer = slot 0) to its
CPU *before* returning to the caller (thread_pool.h:709-719). This guarantees the
caller's subsequent workload buffers first-touch on the producer's CCD, so
they're local to the slots that will get the asymmetric "big share." Without it,
the buffers could land on a different CCD and every access would cross the fabric
— the comment cites a 5-10× stall.

---

## 6. Cost, gating, and accuracy safeguards

- **Cost.** ≈ `(N-1)` sequential rounds × ~4 ms ≈ 270 ms at 64 workers. Paid once
  at construction, outside any timed region. Amortized to nothing over a
  long-lived pool; visible in short-lived/one-shot processes (see
  `uts_optimizations.md`).
- **Gating.** Only runs for standalone pools with `effective > 2` participants
  (thread_pool.h:732). Single-/dual-worker pools and `PoolGroup` arenas skip it.
- **Safeguards against measuring noise as structure:** hard pinning; cache-line
  isolation of control flags; warmup; averaging over 1024 trips; log-space
  scale-invariant histogram; the bimodality + minimum-sample-size trust gate; and
  the sysfs-prior fallback. The clustering is also re-checked for contiguity and
  producer-in-cluster-0 before enabling the hierarchical scan path, because a
  false-positive cluster on that path can deadlock the cross-cluster wait
  (thread_pool.h:837-849).

## TL;DR

citor measures real core-to-core latency at startup with a pinned, cache-aligned,
warmed-up atomic ping-pong, runs all pairs in `N-1` parallel round-robin rounds
to keep it O(N) wall time, then Otsu-thresholds a log-latency histogram into
coherence clusters (falling back to sysfs when the signal is weak). Fork/join
stealing rides the sysfs CCD prior with a 7/8 same-CCD probability bias; the
measured matrix specifically powers the parallel scan/reduce family — sizing the
producer-CCD work share by the observed cross/intra latency ratio and switching
to a one-line-per-cluster-pair hierarchical reduce when distinct clusters are
confirmed.
