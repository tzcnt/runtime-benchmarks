# runtime-benchmarks
Benchmarks to compare the performance of async runtimes / executors.

[<img src="https://fleetcode.com/runtime-benchmarks/splash-1.png">](https://fleetcode.com/runtime-benchmarks/)

An interactive view of the full results dataset is available at: https://fleetcode.com/runtime-benchmarks/

Results summary table of a single configuration:

| Runtime | [libfork](https://github.com/ConorWilliams/libfork) | [TooManyCooks](https://github.com/tzcnt/TooManyCooks) | [tbb](https://www.intel.com/content/www/us/en/developer/tools/oneapi/onetbb.html) | [taskflow](https://github.com/taskflow/taskflow) | [cppcoro](https://github.com/andreasbuhr/cppcoro) | [coros](https://github.com/mtmucha/coros) | [HPX](https://github.com/STEllAR-GROUP/hpx) | [concurrencpp](https://github.com/David-Haim/concurrencpp) | [libcoro](https://github.com/jbaldwin/libcoro) |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Mean Ratio to Best<br>(lower is better) | 1.00x | 1.11x | 2.79x | 2.95x | 3.00x | 4.41x | 164.68x | 172.02x | 2247.44x |
| skynet(8) | 39509 us | 46285 us | 141389 us | 205437 us | 171084 us | 104557 us | 15275347 us | 12211548 us | 155806778 us |
| fib(39) | 67773 us | 82517 us | 269588 us | 200510 us | 264781 us | 172050 us | 14422928 us | 18555453 us | 304651430 us |
| nqueens(14) | 78595 us | 83610 us | 163150 us | 166061 us | 173162 us | 883629 us | 4522909 us | 8142602 us | 42437681 us |
| matmul(2048) | 41751 us | 41608 us | 64036 us | 63297 us | 64771 us | 50476 us | 72353 us | 67167 us | 459776 us |

<details>
<summary>Click to view the machine configuration used in the summary table</summary>

- Processor: EPYC 7742 64-core processor
- Worker Thread Count: 64 (no SMT)
- OS: Debian 13 Server
- Compiler: Clang 21.1.7 Release (-O3 -march=native)
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

`apt-get install cmake hwloc libhwloc-dev intel-oneapi-tbb-devel libtcmalloc-minimal4`

#### Get Quick Results (uses threads = #CPUs):

`python3 ./build_and_bench_all.py`

Results will appear in `RESULTS.md` and `RESULTS.csv` files.

#### Get Full Results (sweeps threads from 1 to #CPUs):

`python3 ./build_and_bench_all.py full`

Results will also appear in `RESULTS.json` file; this file can be parsed by the interactive benchmarks site. A locally viewable version of this HTML chart will be generated as well.

### Future Plans

Frameworks to come:
- (C#) .Net thread pool
- (Rust) [tokio](https://github.com/tokio-rs/tokio)
- (Golang) goroutines
- Facebook Folly
- PhotonLibOS https://github.com/alibaba/PhotonLibOS

Benchmarks to come:
- Lots of good inspiration [here](https://github.com/ConorWilliams/libfork/tree/main/bench/source)
