# runtime-benchmarks
Benchmarks to compare the performance of async runtimes / executors.

[<img src="https://fleetcode.com/runtime-benchmarks/splash.png?">](https://fleetcode.com/runtime-benchmarks/)

An interactive view of the full results dataset is available at: https://fleetcode.com/runtime-benchmarks/

Results summary table of a single configuration:

| Runtime | [libfork](https://github.com/ConorWilliams/libfork) | [TooManyCooks](https://github.com/tzcnt/TooManyCooks) | [tbb](https://www.intel.com/content/www/us/en/developer/tools/oneapi/onetbb.html) | [cppcoro](https://github.com/andreasbuhr/cppcoro) | [taskflow](https://github.com/taskflow/taskflow) | [coros](https://github.com/mtmucha/coros) | [concurrencpp](https://github.com/David-Haim/concurrencpp) |
| --- | --- | --- | --- | --- | --- | --- | --- |
| Mean Ratio to Best<br>(lower is better) | 1.00x | 1.21x | 2.78x | 3.69x | 4.91x | 5.26x | 170.29x |
| skynet(8) | 39243 us | 48183 us | 142476 us | 277238 us | 310068 us | 150896 us | 11879067 us |
| nqueens(14) | 85499 us | 97645 us | 158381 us | 216374 us | 323129 us | 1024948 us | 8247812 us |
| fib(39) | 66483 us | 94078 us | 271683 us | 243936 us | 428538 us | 266954 us | 18636205 us |
| matmul(2048) | 41195 us | 42898 us | 64231 us | 61755 us | 62715 us | 49846 us | 68094 us |

<details>
<summary>Click to view the machine configuration used in the summary table</summary>

- Processor: EPYC 7742 64-core processor
- Worker Thread Count: 64 (no SMT)
- OS: Debian 13 Server
- Compiler: Clang 19.1.7 Release (-O3 -march=native)
- CPU boost enabled / schedutil governor
- Linked against libtcmalloc_minimal.so.4

</details>

### What's covered?
Currently only includes C++ frameworks, and several recursive fork-join benchmarks:
- recursive fibonacci (forks x2)
- skynet ([original link](https://github.com/atemerev/skynet)) but increased to 100M tasks (forks x10)
- nqueens (forks up to x14)
- matmul (forks x4)

Benchmark problem sizes were chosen to balance between making the total runtime of a full sweep tolerable (especially on weaker hardware with slower runtimes), and being sufficiently large to show meaningful differentiation between faster runtimes.

### How to build and run the benchmarks yourself

#### Install Dependencies:
- The build+bench script uses python3
- CMake + Clang 18 or newer
- libfork and TooManyCooks depend on the [hwloc](https://www.open-mpi.org/projects/hwloc/) library.
- TBB benchmarks depend on system installed TBB - see the [installation guide here for the newest version](https://www.intel.com/content/www/us/en/docs/oneapi/installation-guide-linux/2024-2/apt.html) or you may be able to find the old version 'libtbb-dev' in your system package manager
- A high performance allocator (tcmalloc, jemalloc, or mimalloc) is also recommended. The build script will dynamically link to any of these if they are available.

`apt-get install cmake libhwloc-dev intel-oneapi-tbb-devel libtcmalloc-minimal4`

#### Get Quick Results (uses threads = #CPUs):

`python3 ./build_and_bench_all.py`

Results will appear in `RESULTS.md` and `RESULTS.csv` files.

#### Get Full Results (sweeps threads from 1 to #CPUs):

`python3 ./build_and_bench_all.py full`

Results will also appear in `RESULTS.json` file; this file can be parsed by the interactive benchmarks site.

### Future Plans

Frameworks to come:
- (C#) .Net thread pool
- (Rust) [tokio](https://github.com/tokio-rs/tokio)
- (Golang) goroutines
- Facebook Folly
- PhotonLibOS https://github.com/alibaba/PhotonLibOS

Benchmarks to come:
- Lots of good inspiration [here](https://github.com/ConorWilliams/libfork/tree/main/bench/source)
