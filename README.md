# runtime_benchmarks
Benchmarks to compare the performance of async runtimes / fork-join frameworks.

Currently only contains C++ frameworks:
- [TooManyCooks](https://github.com/tzcnt/TooManyCooks)
- [libfork](https://github.com/ConorWilliams/libfork)
- [OneAPI TBB](https://www.intel.com/content/www/us/en/developer/tools/oneapi/onetbb.html)
- [coros](https://github.com/mtmucha/coros)
- [concurrencpp](https://github.com/David-Haim/concurrencpp)

And 4 benchmarks:
- recursive fibonacci (forks x2)
- skynet ([original link](https://github.com/atemerev/skynet)) but increased to 100M tasks (forks x10)
- nqueens (forks up to x14)
- matmul (forks x4)

Current Benchmark Results:

| Runtime | [TooManyCooks](https://github.com/tzcnt/TooManyCooks) | [libfork](https://github.com/ConorWilliams/libfork) | [tbb](https://www.intel.com/content/www/us/en/developer/tools/oneapi/onetbb.html) | [coros](https://github.com/mtmucha/coros) | [concurrencpp](https://github.com/David-Haim/concurrencpp) | [taskflow](https://github.com/taskflow/taskflow) |
| --- | --- | --- | --- | --- | --- | --- |
| Mean Ratio to Best<br>(lower is better) | 1.22x | 2.15x | 2.77x | 8.04x | 171.91x | 281.91x |
| skynet | 49619 us | 38518 us | 140487 us | 639277 us | 11889739 us | 20008863 us |
| nqueens | 95342 us | 82941 us | 160199 us | 859354 us | 8206345 us | 7229609 us |
| fib(40) | 156765 us | 107774 us | 433678 us | 433178 us | 30012566 us | 55921776 us |
| matmul(2048) | 45018 us | 252345 us | 66608 us | 53247 us | 69233 us | 96501 us |

Benchmark configuration:
- Processor: EPYC 7742 64-core processor
- Worker Thread Count: 64 (no SMT)
- OS: Debian 12 Server
- Compiler: Clang 19.1.6 Release (-O3 -march=native)
- CPU boost enabled / schedutil governor
- Linked against libtcmalloc_minimal.so.4

Frameworks to come:
- (C#) .Net thread pool
- (Rust) [tokio](https://github.com/tokio-rs/tokio)
- (Golang) goroutines
- Facebook Folly
- PhotonLibOS https://github.com/alibaba/PhotonLibOS

Benchmarks to come:
- Lots of good inspiration [here](https://github.com/ConorWilliams/libfork/tree/main/bench/source)
