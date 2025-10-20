# runtime-benchmarks
Benchmarks to compare the performance of async runtimes / executors.

[<img src="https://fleetcode.com/runtime-benchmarks/splash.png?">](https://fleetcode.com/runtime-benchmarks/)

An interactive view of the full results dataset is available at: https://fleetcode.com/runtime-benchmarks/

Results summary table of a single configuration:

| Runtime | [libfork](https://github.com/ConorWilliams/libfork) | [TooManyCooks](https://github.com/tzcnt/TooManyCooks) | [tbb](https://www.intel.com/content/www/us/en/developer/tools/oneapi/onetbb.html) | [taskflow](https://github.com/taskflow/taskflow) | [cppcoro](https://github.com/andreasbuhr/cppcoro) | [coros](https://github.com/mtmucha/coros) | [concurrencpp](https://github.com/David-Haim/concurrencpp) | [HPX](https://github.com/STEllAR-GROUP/hpx) | [libcoro](https://github.com/jbaldwin/libcoro) |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Mean Ratio to Best<br>(lower is better) | 1.00x | 1.21x | 2.77x | 3.40x | 4.03x | 4.26x | 171.21x | 903.93x | 2163.58x |
| skynet | 39050 us | 48412 us | 137643 us | 195920 us | 283737 us | 105094 us | 12080295 us | 49761721 us | 161232827 us |
| nqueens | 79549 us | 83999 us | 162379 us | 258937 us | 187504 us | 834379 us | 8235763 us | 34641489 us | 42377788 us |
| fib(39) | 68338 us | 102193 us | 272868 us | 257003 us | 333695 us | 179554 us | 18472130 us | 130125704 us | 272088332 us |
| matmul(2048) | 40772 us | 42863 us | 62607 us | 63254 us | 65595 us | 49801 us | 68045 us | 73184 us | 456989 us |

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
