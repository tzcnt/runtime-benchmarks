# runtime-benchmarks
Benchmarks to compare the performance of async runtimes / executors.

[<img src="https://fleetcode.com/img/bench-splash.png">](https://fleetcode.com/runtime-benchmarks/)

An interactive view of the full results dataset is available at: https://fleetcode.com/runtime-benchmarks/

Results summary table of a single configuration:

| Runtime | [libfork](https://github.com/ConorWilliams/libfork) | [TooManyCooks](https://github.com/tzcnt/TooManyCooks) | [tbb](https://www.intel.com/content/www/us/en/developer/tools/oneapi/onetbb.html) | [taskflow](https://github.com/taskflow/taskflow) | [cppcoro](https://github.com/andreasbuhr/cppcoro) | [coros](https://github.com/mtmucha/coros) | [HPX](https://github.com/STEllAR-GROUP/hpx) | [concurrencpp](https://github.com/David-Haim/concurrencpp) | [libcoro](https://github.com/jbaldwin/libcoro) |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Mean Ratio to Best<br>(lower is better) | 1.00x | 1.11x | 2.82x | 2.98x | 3.53x | 4.45x | 160.93x | 170.80x | 2238.69x |
| skynet | 39639 us | 42512 us | 146884 us | 200196 us | 156739 us | 110734 us | 14654199 us | 12085877 us | 153184034 us |
| nqueens | 78579 us | 83539 us | 161880 us | 183805 us | 186797 us | 883579 us | 4498900 us | 8252158 us | 43830994 us |
| fib(39) | 67668 us | 84565 us | 272178 us | 203514 us | 438185 us | 171781 us | 14550913 us | 18381070 us | 305949459 us |
| matmul(2048) | 41733 us | 43626 us | 62264 us | 62783 us | 54275 us | 50580 us | 72222 us | 68116 us | 465260 us |

| Runtime | [TooManyCooks_st_asio](https://github.com/tzcnt/TooManyCooks) | [TooManyCooks_mt](https://github.com/tzcnt/TooManyCooks) | [libcoro_mt](https://github.com/jbaldwin/libcoro) | [cobalt_st_asio](https://github.com/boostorg/cobalt) |
| --- | --- | --- | --- | --- |
| Mean Ratio to Best<br>(lower is better) | 1.00x | 1.02x | 1.55x | 3.77x |
| channel | 365842 us | 374115 us | 565826 us | 1379967 us |

| Runtime | [TooManyCooks](https://github.com/tzcnt/TooManyCooks) | [cobalt](https://github.com/boostorg/cobalt) | [cppcoro](https://github.com/andreasbuhr/cppcoro) | [libcoro](https://github.com/jbaldwin/libcoro) |
| --- | --- | --- | --- | --- |
| Mean Ratio to Best<br>(lower is better) | 1.00x | 1.12x | 1.45x | 1.48x |
| io_socket_st | 393705 us | 441244 us | 569703 us | 582490 us |

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

As well as some miscellaneous benchmarks:
- channel - tests the performance of the library's async MPMC queue
- io_socket_st - tests TCP ping-pong between a single-threaded client and single-threaded server

Benchmark problem sizes were chosen to balance between making the total runtime of a full sweep tolerable (especially on weaker hardware with slower runtimes), and being sufficiently large to show meaningful differentiation between faster runtimes.

### How to build and run the benchmarks yourself

#### Install Dependencies:
- The build+bench script uses python3. The only Python dependency is libyaml.
- CMake + Clang 18 or newer
- libfork and TooManyCooks depend on the [hwloc](https://www.open-mpi.org/projects/hwloc/) library.
- TBB benchmarks depend on system installed TBB - see the [installation guide here for the newest version](https://www.intel.com/content/www/us/en/docs/oneapi/installation-guide-linux/2024-2/apt.html) or you may be able to find the old version 'libtbb-dev' in your system package manager
- boost::cobalt requires Boost 1.82 or newer. You may need to build Boost from source, since cobalt is currently not included in distro packages.
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

### Future Plans

Frameworks to come:
- (C#) .Net thread pool
- (Rust) [tokio](https://github.com/tokio-rs/tokio)
- (Golang) goroutines
- Facebook Folly
- PhotonLibOS https://github.com/alibaba/PhotonLibOS

Benchmarks to come:
- Some inspiration [here](https://github.com/ConorWilliams/libfork/tree/main/bench/source)
