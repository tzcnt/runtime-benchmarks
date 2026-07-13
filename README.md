# runtime-benchmarks
Benchmarks to compare the performance of async runtimes / executors.

[<img src="https://fleetcode.com/img/bench-splash.png">](https://fleetcode.com/runtime-benchmarks/)

An interactive view of the full results dataset is available at: https://fleetcode.com/runtime-benchmarks/

| Runtime | [citor](https://github.com/Lallapallooza/citor) | [libfork](https://github.com/ConorWilliams/libfork) | [TooManyCooks](https://github.com/tzcnt/TooManyCooks) | [forte](https://github.com/NthTensor/Forte) | [spice](https://github.com/judofyr/spice) | [chili](https://github.com/dragostis/chili) | [rayon](https://github.com/rayon-rs/rayon) | [tbb](https://www.intel.com/content/www/us/en/developer/tools/oneapi/onetbb.html) | [zap](https://github.com/kprotty/zap) | [taskflow](https://github.com/taskflow/taskflow) | [forkjoin](https://docs.oracle.com/en/java/javase/25/docs/api/java.base/java/util/concurrent/ForkJoinPool.html) | [cppcoro](https://github.com/andreasbuhr/cppcoro) | [weave](https://github.com/mratsim/weave) | [coros](https://github.com/mtmucha/coros) | [kotlin_fjp](https://github.com/Kotlin/kotlinx.coroutines) | [dotnet](https://learn.microsoft.com/en-us/dotnet/api/) | [zigbeam](https://github.com/eakova/zigbeam) | [folly](https://github.com/facebook/folly) | [micropool](https://github.com/DouglasDwyer/micropool) | [concurrencpp](https://github.com/David-Haim/concurrencpp) | [go](https://pkg.go.dev/std) | [kotlin_default](https://github.com/Kotlin/kotlinx.coroutines) | [tokio](https://github.com/tokio-rs/tokio) | [beekeeper](https://github.com/jdidion/beekeeper) | [HPX](https://github.com/STEllAR-GROUP/hpx) | [java](https://openjdk.org/jeps/444) | [libcoro](https://github.com/jbaldwin/libcoro) | [bevy_tasks](https://github.com/bevyengine/bevy/tree/main/crates/bevy_tasks) | [smol](https://github.com/smol-rs/smol) | [userver](https://github.com/userver-framework/userver) | [PhotonLibOS](https://github.com/alibaba/PhotonLibOS) |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Mean Ratio to Best<br>(lower is better) | 1.34x | 1.50x | 1.73x | 1.93x | 2.23x | 2.29x | 2.34x | 4.08x | 4.28x | 4.28x | 4.67x | 5.16x | 5.21x | 5.89x | 29.07x | 125.65x | 136.97x | 271.78x | 300.54x | 362.60x | 412.45x | 434.17x | 474.40x | 1064.90x | 1101.99x | 1989.64x | 2450.95x | 3274.77x | 3285.89x | 14258.84x | 14258.98x |
| skynet | 24872 us | 37218 us | 38321 us | 43844 us | 45697 us | 53081 us | 97513 us | 119827 us | 246760 us | 190897 us | 181696 us | 146978 us | 108923 us | 90579 us | 785785 us | 5269237 us | 6840971 us | 5852604 us | 5042222 us | 12631272 us | 22018058 us | 35299698 us | 25104353 us | 38748915 us | 43172214 us | 18313064 us | 114395779 us | 156710693 us | 157069567 us | DNF (10m) | DNF (10m) |
| nqueens | 84723 us | 68663 us | 68653 us | 95720 us | 81701 us | 172293 us | 90880 us | 129107 us | 84922 us | 133394 us | 155829 us | 156126 us | 91562 us | 761551 us | 430441 us | 1888680 us | 778667 us | 7490258 us | 3083814 us | 7972459 us | 6539815 us | 1896422 us | 5459275 us | 7822818 us | 16816915 us | 6398470 us | 32426118 us | 43115620 us | 43279039 us | DNF (10m) | DNF (10m) |
| fib(39) | 48704 us | 62273 us | 83271 us | 33710 us | 32804 us | 24823 us | 61047 us | 210523 us | 93912 us | 159770 us | 108490 us | 279652 us | 102028 us | 192224 us | 1841593 us | 6225001 us | 6416846 us | 17415170 us | 23608170 us | 20480773 us | 16376473 us | 7077968 us | 20027825 us | 64172410 us | 60217787 us | 176841906 us | 117258333 us | 152914172 us | 153599622 us | DNF (10m) | DNF (10m) |
| matmul(2048) | 49425 us | 43160 us | 43605 us | 138720 us | 197770 us | 151984 us | 70915 us | 48811 us | 94238 us | 46352 us | 204728 us | 51466 us | 475114 us | 46147 us | 182643 us | 537368 us | 129717 us | 1775395 us | 148671 us | 59036 us | 411689 us | 201714 us | 81847 us | 109428 us | 57943 us | 214341 us | 359766 us | 439736 us | 441791 us | 48077 us | 72836 us |

| Runtime | [tokio](https://github.com/tokio-rs/tokio) | [TooManyCooks](https://github.com/tzcnt/TooManyCooks) | [cobalt](https://github.com/boostorg/cobalt) | [PhotonLibOS](https://github.com/alibaba/PhotonLibOS) | [smol](https://github.com/smol-rs/smol) | [libcoro](https://github.com/jbaldwin/libcoro) | [go](https://pkg.go.dev/std) | [neco](https://github.com/tidwall/neco) | [cppcoro](https://github.com/andreasbuhr/cppcoro) | [dotnet](https://learn.microsoft.com/en-us/dotnet/api/) | [userver](https://github.com/userver-framework/userver) | [weave](https://github.com/mratsim/weave) | [folly](https://github.com/facebook/folly) | [kotlin_fjp](https://github.com/Kotlin/kotlinx.coroutines) | [kotlin_default](https://github.com/Kotlin/kotlinx.coroutines) | [java](https://openjdk.org/jeps/444) |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Mean Ratio to Best<br>(lower is better) | 1.00x | 1.09x | 1.20x | 1.21x | 1.29x | 1.30x | 1.38x | 1.44x | 1.67x | 1.76x | 2.13x | 2.16x | 2.69x | 3.99x | 4.01x | 4.40x |
| io_socket_st | 292726 us | 318976 us | 351168 us | 352910 us | 376400 us | 379725 us | 402958 us | 420366 us | 489036 us | 515406 us | 622233 us | 632172 us | 788126 us | 1169191 us | 1173373 us | 1287701 us |

| Runtime | [TooManyCooks_mt](https://github.com/tzcnt/TooManyCooks) | [TooManyCooks_st_asio](https://github.com/tzcnt/TooManyCooks) | [libcoro_mt](https://github.com/jbaldwin/libcoro) | [neco](https://github.com/tidwall/neco) | [cobalt_st_asio](https://github.com/boostorg/cobalt) | [go](https://pkg.go.dev/std) | [java](https://openjdk.org/jeps/444) | [folly](https://github.com/facebook/folly) | [zigbeam](https://github.com/eakova/zigbeam) | [tokio_flume](https://github.com/tokio-rs/tokio) | [PhotonLibOS](https://github.com/alibaba/PhotonLibOS) | [beekeeper](https://github.com/jdidion/beekeeper) | [kotlin_default](https://github.com/Kotlin/kotlinx.coroutines) | [dotnet](https://learn.microsoft.com/en-us/dotnet/api/) | [userver](https://github.com/userver-framework/userver) | [weave_threading](https://github.com/mratsim/weave) | [kotlin_fjp](https://github.com/Kotlin/kotlinx.coroutines) | [smol](https://github.com/smol-rs/smol) |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Mean Ratio to Best<br>(lower is better) | 1.00x | 1.02x | 1.21x | 2.11x | 2.33x | 2.79x | 3.50x | 4.03x | 6.48x | 12.39x | 12.99x | 14.35x | 15.30x | 15.47x | 17.05x | 17.10x | 19.52x | 24.36x |
| channel | 376499 us | 384408 us | 455188 us | 794814 us | 875631 us | 1049222 us | 1316790 us | 1516630 us | 2438126 us | 4663599 us | 4891633 us | 5402404 us | 5759240 us | 5824840 us | 6419394 us | 6439726 us | 7349066 us | 9171548 us |



### Peak Memory Usage (Max RSS)

| Runtime | citor | libfork | TooManyCooks | TooManyCooks_st_asio | TooManyCooks_mt | tbb | taskflow | cppcoro | coros | cobalt_st_asio | cobalt | PhotonLibOS | folly | concurrencpp | HPX | libcoro | libcoro_mt | userver | tokio | tokio_flume | go | dotnet | java | forkjoin | kotlin_fjp | kotlin_default | weave | weave_threading | neco | zap | zigbeam | spice | beekeeper | bevy_tasks | chili | forte | micropool | rayon | smol |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| skynet | 21.98 MB | 9.66 MB | 11.0 MB | N/A | N/A | 12.65 MB | 7.93 MB | 134.04 MB | 11.94 MB | N/A | N/A | N/A | 14.89 MB | 11.45 MB | 28.84 GB | 15.4 GB | N/A | N/A | 9.05 GB | N/A | 12.09 GB | 54.02 MB | 25.99 GB | 4.38 GB | 6.11 GB | 25.53 GB | 6.1 MB | N/A | N/A | 212.86 MB | 37.88 MB | 32.21 MB | 3.45 GB | 17.86 GB | 6.08 MB | 6.11 MB | 6.11 MB | 6.08 MB | 18.21 GB |
| nqueens | 61.38 MB | 10.15 MB | 11.26 MB | N/A | N/A | 11.07 MB | 10.94 MB | 134.13 MB | 11.96 MB | N/A | N/A | N/A | 11.73 MB | 10.76 MB | 12.84 GB | 5.21 GB | N/A | N/A | 1.41 GB | N/A | 2.17 GB | 55.47 MB | 25.87 GB | 1.72 GB | 5.32 GB | 12.32 GB | 6.1 MB | N/A | N/A | 55.0 MB | 31.86 MB | 35.34 MB | 561.04 MB | 4.41 GB | 6.12 MB | 6.09 MB | 6.11 MB | 6.11 MB | 4.4 GB |
| fib(39) | 31.37 MB | 13.01 MB | 11.19 MB | N/A | N/A | 12.62 MB | 9.12 MB | 134.03 MB | 9.21 MB | N/A | N/A | N/A | 11.77 MB | 10.6 MB | 18.16 GB | 9.96 GB | N/A | N/A | 2.2 GB | N/A | 4.73 GB | 50.81 MB | 26.32 GB | 1.9 GB | 6.93 GB | 15.57 GB | 67.05 MB | N/A | N/A | 123.05 MB | 31.47 MB | 35.77 MB | 2.2 GB | 8.52 GB | 6.13 MB | 6.12 MB | 6.11 MB | 6.11 MB | 8.39 GB |
| matmul(2048) | 70.96 MB | 58.89 MB | 59.87 MB | N/A | N/A | 58.69 MB | 55.83 MB | 186.38 MB | 58.44 MB | N/A | N/A | 75.82 MB | 60.1 MB | 59.52 MB | 126.34 MB | 59.16 MB | N/A | 68.86 MB | 49.12 MB | N/A | 174.72 MB | 130.5 MB | 577.36 MB | 262.84 MB | 592.06 MB | 591.66 MB | 67.07 MB | N/A | N/A | 66.54 MB | 84.32 MB | 83.9 MB | 48.79 MB | 47.01 MB | 48.68 MB | 48.8 MB | 47.78 MB | 48.36 MB | 48.38 MB |
| io_socket_st | N/A | N/A | 13.02 MB | N/A | N/A | N/A | N/A | 9.35 MB | N/A | N/A | 9.16 MB | 14.6 MB | 15.89 MB | N/A | N/A | 7.72 MB | N/A | 18.18 MB | 6.11 MB | N/A | 9.64 MB | 49.97 MB | 113.96 MB | N/A | 170.32 MB | 172.05 MB | 6.11 MB | N/A | 9.54 MB | N/A | N/A | N/A | N/A | N/A | N/A | N/A | N/A | N/A | 6.11 MB |
| channel | N/A | N/A | N/A | 19.67 MB | 26.7 MB | N/A | N/A | N/A | N/A | 9.0 MB | N/A | 14.97 MB | 145.24 MB | N/A | N/A | N/A | 9.31 MB | 19.19 MB | N/A | 27.26 MB | 39.91 MB | 235.26 MB | 627.52 MB | N/A | 851.84 MB | 852.43 MB | N/A | 6.1 MB | 9.52 MB | N/A | 41.02 MB | N/A | 6.12 MB | N/A | N/A | N/A | N/A | N/A | 6.13 MB |

<details>
<summary>Click to view the machine configuration used in the summary tables</summary>

- Processor: EPYC 7V73X 64-core processor
- Worker Thread Count: 64 (no SMT)
- OS: Debian 14 Server
- Compiler: Clang 21.1.7 Release (-O3 -march=native)
- CPU boost enabled / schedutil governor
- Linked against libtcmalloc_minimal.so.4

</details>

### What's covered?
Recursive fork-join benchmarks:
- recursive fibonacci (forks x2)
- skynet ([original link](https://github.com/atemerev/skynet)) but increased to 100M tasks (forks x10)
- nqueens (forks up to x14)
- matmul (forks x4)

As well as some miscellaneous benchmarks:
- channel - tests the performance of the library's async MPMC queue
- io_socket_st - tests TCP ping-pong between a single-threaded client and single-threaded server

Benchmark problem sizes were chosen to balance between making the total runtime of a full sweep tolerable (especially on weaker hardware with slower runtimes), and being sufficiently large to show meaningful differentiation between faster runtimes.

### How to build and run the benchmarks yourself

Note: if you have issues with a particular runtime, you can simply remove it from line 17 of `build_and_bench_all.py` to skip it.

#### Install Dependencies:
- The build+bench script uses python3. The only Python dependency is libyaml.
- CMake + Clang 18 or newer
- libfork and TooManyCooks depend on the [hwloc](https://www.open-mpi.org/projects/hwloc/) library.
- TBB benchmarks depend on system installed TBB - see the [installation guide here for the newest version](https://www.intel.com/content/www/us/en/docs/oneapi/installation-guide-linux/2024-2/apt.html) or you may be able to find the old version 'libtbb-dev' in your system package manager
- HPX and boost::cobalt requires Boost 1.82 or newer. You may need to build Boost from source, since cobalt is currently not included in distro packages.
- A high performance allocator (tcmalloc, jemalloc, or mimalloc) is also recommended. The build script will dynamically link to any of these if they are available.

On Debian/Ubuntu:
`sudo apt-get install cmake hwloc libhwloc-dev intel-oneapi-tbb-devel libtcmalloc-minimal4`

On MacOS:
`brew install cmake gperftools hwloc libyaml tbb`

#### Get Quick Results (uses threads = #CPUs):

NOTE: If a particular library or benchmark fails to build or run, don't worry - its output will simply be ignored.

`python3 ./build_and_bench_all.py`

Results will appear in `RESULTS.md` and `RESULTS.csv` files.

#### Get Full Results (sweeps threads from 1 to #CPUs):

`python3 ./build_and_bench_all.py full`

Results will also appear in `RESULTS.json` file; this file can be parsed by the interactive benchmarks site. A locally viewable version of this HTML chart will be generated as well.

#### Benchmark a Single Runtime (sweeps threads from 1 to #CPUs):

git-ref can be a SHA, tag, or branch:

```
  ./build_and_bench_all.py <runtime> [git-ref]
  ./build_and_bench_all.py compare <runtime> <new-git-ref> [baseline-git-ref]
```

### Rejected Libraries

Some parallelism libraries were evaluated but ultimately excluded from the suite. Every benchmark here is fundamentally a recursive fork-join workload (a task forks child tasks, which fork their own children, and so on), and is run on a fixed-size thread pool that is created once at startup and sized by the thread-count argument: that same pool must absorb the entire recursion, spawning no new OS threads while the benchmark is running. A library is excluded when it cannot meet this bar - either because it only supports *flat* data parallelism (a single parallel-for or parallel-iterator over an indexed collection, with no way to dispatch further parallel work from inside a running task), or because the only way it can express the nested fork-join is by spawning fresh threads mid-run instead of reusing the fixed startup pool. Either way the result no longer measures the scheduler behavior the suite is designed to compare, so rather than publish distorted partial results, these libraries are excluded:

- [Paralight](https://github.com/gendx/paralight) - a parallel-iterator library over indexed structures. Its pipelines cannot be nested: `with_thread_pool` takes `&mut ThreadPool`, only one pipeline may run on a pool at a time, and nested pipelines panic or deadlock. The recursive `fib` and tree-shaped `skynet` therefore have no faithful representation.
- [ForkUnion](https://github.com/ashvardanian/ForkUnion) - a minimalistic scoped thread-pool that dispatches a flat set of tasks. It deliberately provides no nested parallelism ("no nested parallelism ... they are banned, keeping the design simple"), so a running task cannot fork further parallel work and the recursively-forking `fib` and `skynet` cannot be faithfully expressed.
- [orx-parallel](https://github.com/orxfun/orx-parallel) - a configurable parallel-iterator library. Unlike the two above it *can* express the recursive workloads (nested parallel iterators compose, and `into_par_rec` walks a tree through a shared work queue), but its own runner creates worker threads on demand for every parallel region via `std::thread::scope` rather than reusing a fixed startup pool. On these recursively-forking benchmarks that means spawning threads continuously while the benchmark runs - `matmul(2048)` alone burns ~180s of system time thrashing thread creation for a ~5s result. The library has no fixed-pool scheduler of its own: the only way to pin it to a pool built at startup is to plug in an external one via `with_runner`, and the sole external pool that also supports the required nesting is rayon's own - at which point the entry would measure rayon's work-stealing scheduler rather than orx-parallel's.
- [trash_parallelism](https://crates.io/crates/trash_parallelism) - rejected for a different reason than the above: the parallelism it advertises does not exist. The crate bills itself as a "comprehensive parallelism library" with "work-stealing schedulers and fork-join patterns", "automatic parallelization for CPU-bound tasks", and "file and network I/O with async support". In the published source (v0.1.102), the entire `parallel` module is sequential: `parallel_map` / `parallel_for_each` / `parallel_fold` / `parallel_filter` are plain `into_iter()` loops (the doc comments themselves admit "currently uses sequential processing"), `distribute_work` hardcodes `num_threads = 4 // fixed for now` and then runs every chunk inline on the calling thread anyway, and `parallel_process_files` accepts a `&mut ThreadPool` argument it never uses. There is no network I/O anywhere (the `io` module is file-only), the `spawn_task!` macro shown in its README is not defined in the crate, and the doc examples import crates that don't exist (`trash_utilities`, `trash_analyzer`). It also does not compile on any stable Rust - it unconditionally enables brotli's nightly-only `simd` feature while declaring `rust-version = "1.92"`. The only real concurrency in the crate is a thin delegation to smol's global executor, so a benchmark entry would measure smol's scheduler wrapped in pathological overhead (its fan-out helper allocates a results channel plus a producer task per fork; a trial implementation ran `nqueens` ~15x slower than tokio at 27.8 GB peak RSS), not a distinct runtime.
