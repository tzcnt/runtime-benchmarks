# runtime_benchmarks
Benchmarks to compare the performance of async runtimes / fork-join frameworks.

Currently only includes C++ frameworks, and 4 benchmarks:
- recursive fibonacci (forks x2)
- skynet ([original link](https://github.com/atemerev/skynet)) but increased to 100M tasks (forks x10)
- nqueens (forks up to x14)
- matmul (forks x4)

Benchmark problem sizes were chosen to balance between making the total runtime of a full sweep tolerable (especially on weaker hardware with slower runtimes), and being sufficiently large to show meaningful differentiation between faster runtimes.

An interactive view of the full dataset is available at: https://fleetcode.com/runtime-benchmarks/
[<img src="https://fleetcode.com/runtime-benchmarks/splash.png">](https://fleetcode.com/runtime-benchmarks/)

Summary table of a single configuration:

| Runtime | [libfork](https://github.com/ConorWilliams/libfork) | [TooManyCooks](https://github.com/tzcnt/TooManyCooks) | [tbb](https://www.intel.com/content/www/us/en/developer/tools/oneapi/onetbb.html) | [taskflow](https://github.com/taskflow/taskflow) | [coros](https://github.com/mtmucha/coros) | [concurrencpp](https://github.com/David-Haim/concurrencpp) |
| --- | --- | --- | --- | --- | --- | --- |
| Mean Ratio to Best<br>(lower is better) | 1.00x | 1.22x | 2.79x | 4.72x | 4.89x | 172.59x |
| skynet | 38959 us | 49876 us | 143463 us | 297976 us | 169046 us | 12022416 us |
| nqueens | 84538 us | 94694 us | 157681 us | 295778 us | 856788 us | 8248584 us |
| fib(39) | 66197 us | 98391 us | 271773 us | 415146 us | 254883 us | 18706393 us |
| matmul(2048) | 43163 us | 43367 us | 65335 us | 63915 us | 53713 us | 69539 us |

Configuration used in the summary table:
- Processor: EPYC 7742 64-core processor
- Worker Thread Count: 64 (no SMT)
- OS: Debian 13 Server
- Compiler: Clang 19.1.6 Release (-O3 -march=native)
- CPU boost enabled / schedutil governor
- Linked against libtcmalloc_minimal.so.4

### Running the Benchmarks
Install Dependencies:
- libfork and TooManyCooks depend on the [hwloc](https://www.open-mpi.org/projects/hwloc/) library.
- TBB benchmarks depend on system installed TBB - see the [installation guide here for the newest version](https://www.intel.com/content/www/us/en/docs/oneapi/installation-guide-linux/2024-2/apt.html) or you may be able to find the old version 'libtbb-dev' in your system package manager
- A high performance allocator (tcmalloc, jemalloc, or mimalloc) is also recommended. The build script will dynamically link to any of these if they are available.

`apt-get install libhwloc-dev intel-oneapi-tbb-devel libtcmalloc-minimal4`

Quick Results (uses threads = #CPUs):

`python3 ./build_and_bench_all.py`

Results will appear in `RESULTS.md` and `RESULTS.csv` files.

Full Results (sweeps threads from 1 to #CPUs):

`python3 ./build_and_bench_all.py full`

Results will appear in `RESULTS.json` file; this file can be parsed by the interactive benchmarks site.

Frameworks to come:
- (C#) .Net thread pool
- (Rust) [tokio](https://github.com/tokio-rs/tokio)
- (Golang) goroutines
- Facebook Folly
- PhotonLibOS https://github.com/alibaba/PhotonLibOS

Benchmarks to come:
- Lots of good inspiration [here](https://github.com/ConorWilliams/libfork/tree/main/bench/source)
