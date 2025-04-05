# runtime_benchmarks
Benchmarks to compare the performance of async runtimes / fork-join frameworks.

Currently only includes C++ frameworks, and 4 benchmarks:
- recursive fibonacci (forks x2)
- skynet ([original link](https://github.com/atemerev/skynet)) but increased to 100M tasks (forks x10)
- nqueens (forks up to x14)
- matmul (forks x4)

Current Benchmark Results:

| Runtime | [libfork](https://github.com/ConorWilliams/libfork) | [TooManyCooks](https://github.com/tzcnt/TooManyCooks) | [tbb](https://www.intel.com/content/www/us/en/developer/tools/oneapi/onetbb.html) | [coros](https://github.com/mtmucha/coros) | [concurrencpp](https://github.com/David-Haim/concurrencpp) | [taskflow](https://github.com/taskflow/taskflow) |
| --- | --- | --- | --- | --- | --- | --- |
| Mean Ratio to Best<br>(lower is better) | 1.00x | 1.23x | 2.83x | 8.09x | 172.85x | 282.47x |
| skynet | 38224 us | 48412 us | 140226 us | 642072 us | 11957154 us | 19889202 us |
| nqueens | 82906 us | 95463 us | 160356 us | 860870 us | 8148187 us | 7239839 us |
| fib(40) | 106895 us | 155419 us | 445900 us | 418247 us | 29787722 us | 55576715 us |
| matmul(2048) | 42275 us | 43795 us | 64743 us | 53487 us | 69684 us | 98070 us |

Benchmark configuration:
- Processor: EPYC 7742 64-core processor
- Worker Thread Count: 64 (no SMT)
- OS: Debian 13 Server
- Compiler: Clang 19.1.6 Release (-O3 -march=native)
- CPU boost enabled / schedutil governor
- Linked against libtcmalloc_minimal.so.4

### Running the Benchmarks
Install Dependencies:
- libfork and TooManyCooks depend on the [hwloc](https://www.open-mpi.org/projects/hwloc/) library.
- TBB benchmarks depend on system installed TBB
- A high performance allocator (tcmalloc, jemalloc, or mimalloc) is also recommended. The build script will dynamically link to any of these if they are available.

`apt-get install libhwloc-dev intel-oneapi-tbb-devel libtcmalloc-minimal4`

Run the Script:

`python3 ./build_and_bench_all.py`

Results will appear in `RESULTS.md` and `RESULTS.csv` files.

### Caveats
taskflow benchmarks are currently disabled due to high memory consumption in recursive subflows ([issue link](https://github.com/taskflow/taskflow/issues/674)). The taskflow results shown in this README were gathered on a server with 128GB of RAM, but on smaller systems they do not complete. These will be re-enabled once I can update the benchmark script to handle an OOM kill and represent the data appropriately. 

Frameworks to come:
- (C#) .Net thread pool
- (Rust) [tokio](https://github.com/tokio-rs/tokio)
- (Golang) goroutines
- Facebook Folly
- PhotonLibOS https://github.com/alibaba/PhotonLibOS

Benchmarks to come:
- Lots of good inspiration [here](https://github.com/ConorWilliams/libfork/tree/main/bench/source)
