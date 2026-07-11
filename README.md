# runtime-benchmarks
Benchmarks to compare the performance of async runtimes / executors.

[<img src="https://fleetcode.com/img/bench-splash.png">](https://fleetcode.com/runtime-benchmarks/)

An interactive view of the full results dataset is available at: https://fleetcode.com/runtime-benchmarks/

| Runtime | [citor](https://github.com/Lallapallooza/citor) | [libfork](https://github.com/ConorWilliams/libfork) | [TooManyCooks](https://github.com/tzcnt/TooManyCooks) | [tbb](https://www.intel.com/content/www/us/en/developer/tools/oneapi/onetbb.html) | [taskflow](https://github.com/taskflow/taskflow) | [cppcoro](https://github.com/andreasbuhr/cppcoro) | [zap](https://github.com/kprotty/zap) | [forkjoin](https://docs.oracle.com/en/java/javase/25/docs/api/java.base/java/util/concurrent/ForkJoinPool.html) | [weave](https://github.com/mratsim/weave) | [coros](https://github.com/mtmucha/coros) | [kotlin_fjp](https://github.com/Kotlin/kotlinx.coroutines) | [dotnet](https://learn.microsoft.com/en-us/dotnet/api/) | [zigbeam](https://github.com/eakova/zigbeam) | [folly](https://github.com/facebook/folly) | [concurrencpp](https://github.com/David-Haim/concurrencpp) | [go](https://pkg.go.dev/std) | [tokio](https://github.com/tokio-rs/tokio) | [kotlin_default](https://github.com/Kotlin/kotlinx.coroutines) | [HPX](https://github.com/STEllAR-GROUP/hpx) | [java](https://openjdk.org/jeps/444) | [libcoro](https://github.com/jbaldwin/libcoro) | [userver](https://github.com/userver-framework/userver) | [PhotonLibOS](https://github.com/alibaba/PhotonLibOS) |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Mean Ratio to Best<br>(lower is better) | 1.09x | 1.19x | 1.32x | 3.04x | 3.49x | 3.78x | 3.82x | 4.14x | 4.70x | 4.94x | 19.98x | 94.91x | 105.29x | 185.78x | 261.47x | 331.57x | 375.49x | 399.22x | 804.62x | 1116.35x | 1871.90x | 11295.89x | 11296.03x |
| skynet | 24872 us | 37218 us | 38321 us | 119827 us | 190897 us | 146978 us | 246760 us | 181696 us | 108923 us | 90579 us | 785785 us | 5269237 us | 6840971 us | 5852604 us | 12631272 us | 22018058 us | 25104353 us | 35299698 us | 43172214 us | 18313064 us | 114395779 us | DNF (10m) | DNF (10m) |
| nqueens | 84723 us | 68663 us | 68653 us | 129107 us | 133394 us | 156126 us | 84922 us | 155829 us | 91562 us | 761551 us | 430441 us | 1888680 us | 778667 us | 7490258 us | 7972459 us | 6539815 us | 5459275 us | 1896422 us | 16816915 us | 6398470 us | 32426118 us | DNF (10m) | DNF (10m) |
| fib(39) | 48704 us | 62273 us | 83271 us | 210523 us | 159770 us | 279652 us | 93912 us | 108490 us | 102028 us | 192224 us | 1841593 us | 6225001 us | 6416846 us | 17415170 us | 20480773 us | 16376473 us | 20027825 us | 7077968 us | 60217787 us | 176841906 us | 117258333 us | DNF (10m) | DNF (10m) |
| matmul(2048) | 49425 us | 43160 us | 43605 us | 48811 us | 46352 us | 51466 us | 94238 us | 204728 us | 475114 us | 46147 us | 182643 us | 537368 us | 129717 us | 1775395 us | 59036 us | 411689 us | 81847 us | 201714 us | 57943 us | 214341 us | 359766 us | 48077 us | 72836 us |

| Runtime | [tokio](https://github.com/tokio-rs/tokio) | [TooManyCooks](https://github.com/tzcnt/TooManyCooks) | [cobalt](https://github.com/boostorg/cobalt) | [PhotonLibOS](https://github.com/alibaba/PhotonLibOS) | [libcoro](https://github.com/jbaldwin/libcoro) | [go](https://pkg.go.dev/std) | [neco](https://github.com/tidwall/neco) | [cppcoro](https://github.com/andreasbuhr/cppcoro) | [dotnet](https://learn.microsoft.com/en-us/dotnet/api/) | [userver](https://github.com/userver-framework/userver) | [weave](https://github.com/mratsim/weave) | [folly](https://github.com/facebook/folly) | [kotlin_fjp](https://github.com/Kotlin/kotlinx.coroutines) | [kotlin_default](https://github.com/Kotlin/kotlinx.coroutines) | [java](https://openjdk.org/jeps/444) |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Mean Ratio to Best<br>(lower is better) | 1.00x | 1.09x | 1.20x | 1.21x | 1.30x | 1.38x | 1.44x | 1.67x | 1.76x | 2.13x | 2.16x | 2.69x | 3.99x | 4.01x | 4.40x |
| io_socket_st | 292726 us | 318976 us | 351168 us | 352910 us | 379725 us | 402958 us | 420366 us | 489036 us | 515406 us | 622233 us | 632172 us | 788126 us | 1169191 us | 1173373 us | 1287701 us |

| Runtime | [TooManyCooks_mt](https://github.com/tzcnt/TooManyCooks) | [TooManyCooks_st_asio](https://github.com/tzcnt/TooManyCooks) | [libcoro_mt](https://github.com/jbaldwin/libcoro) | [neco](https://github.com/tidwall/neco) | [cobalt_st_asio](https://github.com/boostorg/cobalt) | [go](https://pkg.go.dev/std) | [java](https://openjdk.org/jeps/444) | [folly](https://github.com/facebook/folly) | [zigbeam](https://github.com/eakova/zigbeam) | [tokio_flume](https://github.com/tokio-rs/tokio) | [PhotonLibOS](https://github.com/alibaba/PhotonLibOS) | [kotlin_default](https://github.com/Kotlin/kotlinx.coroutines) | [dotnet](https://learn.microsoft.com/en-us/dotnet/api/) | [userver](https://github.com/userver-framework/userver) | [weave_threading](https://github.com/mratsim/weave) | [kotlin_fjp](https://github.com/Kotlin/kotlinx.coroutines) |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Mean Ratio to Best<br>(lower is better) | 1.00x | 1.02x | 1.21x | 2.11x | 2.33x | 2.79x | 3.50x | 4.03x | 6.48x | 12.39x | 12.99x | 15.30x | 15.47x | 17.05x | 17.10x | 19.52x |
| channel | 376499 us | 384408 us | 455188 us | 794814 us | 875631 us | 1049222 us | 1316790 us | 1516630 us | 2438126 us | 4663599 us | 4891633 us | 5759240 us | 5824840 us | 6419394 us | 6439726 us | 7349066 us |



### Peak Memory Usage (Max RSS)

| Runtime | citor | libfork | TooManyCooks | TooManyCooks_st_asio | TooManyCooks_mt | tbb | taskflow | cppcoro | coros | cobalt_st_asio | cobalt | PhotonLibOS | folly | concurrencpp | HPX | libcoro | libcoro_mt | userver | tokio | tokio_flume | go | dotnet | java | forkjoin | kotlin_fjp | kotlin_default | weave | weave_threading | neco | zap | zigbeam |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| skynet | 21.98 MB | 9.66 MB | 11.0 MB | N/A | N/A | 12.65 MB | 7.93 MB | 134.04 MB | 11.94 MB | N/A | N/A | N/A | 14.89 MB | 11.45 MB | 28.84 GB | 15.4 GB | N/A | N/A | 9.05 GB | N/A | 12.09 GB | 54.02 MB | 25.99 GB | 4.38 GB | 6.11 GB | 25.53 GB | 6.1 MB | N/A | N/A | 212.86 MB | 37.88 MB |
| nqueens | 61.38 MB | 10.15 MB | 11.26 MB | N/A | N/A | 11.07 MB | 10.94 MB | 134.13 MB | 11.96 MB | N/A | N/A | N/A | 11.73 MB | 10.76 MB | 12.84 GB | 5.21 GB | N/A | N/A | 1.41 GB | N/A | 2.17 GB | 55.47 MB | 25.87 GB | 1.72 GB | 5.32 GB | 12.32 GB | 6.1 MB | N/A | N/A | 55.0 MB | 31.86 MB |
| fib(39) | 31.37 MB | 13.01 MB | 11.19 MB | N/A | N/A | 12.62 MB | 9.12 MB | 134.03 MB | 9.21 MB | N/A | N/A | N/A | 11.77 MB | 10.6 MB | 18.16 GB | 9.96 GB | N/A | N/A | 2.2 GB | N/A | 4.73 GB | 50.81 MB | 26.32 GB | 1.9 GB | 6.93 GB | 15.57 GB | 67.05 MB | N/A | N/A | 123.05 MB | 31.47 MB |
| matmul(2048) | 70.96 MB | 58.89 MB | 59.87 MB | N/A | N/A | 58.69 MB | 55.83 MB | 186.38 MB | 58.44 MB | N/A | N/A | 75.82 MB | 60.1 MB | 59.52 MB | 126.34 MB | 59.16 MB | N/A | 68.86 MB | 49.12 MB | N/A | 174.72 MB | 130.5 MB | 577.36 MB | 262.84 MB | 592.06 MB | 591.66 MB | 67.07 MB | N/A | N/A | 66.54 MB | 84.32 MB |
| io_socket_st | N/A | N/A | 13.02 MB | N/A | N/A | N/A | N/A | 9.35 MB | N/A | N/A | 9.16 MB | 14.6 MB | 15.89 MB | N/A | N/A | 7.72 MB | N/A | 18.18 MB | 6.11 MB | N/A | 9.64 MB | 49.97 MB | 113.96 MB | N/A | 170.32 MB | 172.05 MB | 6.11 MB | N/A | 9.54 MB | N/A | N/A |
| channel | N/A | N/A | N/A | 19.67 MB | 26.7 MB | N/A | N/A | N/A | N/A | 9.0 MB | N/A | 14.97 MB | 145.24 MB | N/A | N/A | N/A | 9.31 MB | 19.19 MB | N/A | 27.26 MB | 39.91 MB | 235.26 MB | 627.52 MB | N/A | 851.84 MB | 852.43 MB | N/A | 6.1 MB | 9.52 MB | N/A | 41.02 MB |

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
