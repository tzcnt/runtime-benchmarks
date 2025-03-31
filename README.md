# runtime_benchmarks
Benchmarks to compare the performance of async runtimes / fork-join frameworks.

Currently only contains C++ frameworks:
- [TooManyCooks](https://github.com/tzcnt/TooManyCooks)
- [libfork](https://github.com/ConorWilliams/libfork)
- [OneAPI TBB](https://www.intel.com/content/www/us/en/developer/tools/oneapi/onetbb.html)
- [coros](https://github.com/mtmucha/coros)
- [concurrencpp](https://github.com/David-Haim/concurrencpp)

And 3 benchmarks:
- recursive fibonacci (forks x2)
- skynet ([original link](https://github.com/atemerev/skynet)) but increased to 100M tasks (forks x10)
- nqueens (forks up to x14)

Current Benchmark Results:

| Runtime | [libfork](https://github.com/ConorWilliams/libfork) | [TooManyCooks](https://github.com/tzcnt/TooManyCooks) | [OneAPI TBB](https://www.intel.com/content/www/us/en/developer/tools/oneapi/onetbb.html) | [coros](https://github.com/mtmucha/coros) | [concurrencpp](https://github.com/David-Haim/concurrencpp) |
| --- | --- | --- | --- | --- | --- |
| Mean Ratio to Best<br>(lower is better) | 1.00x | 1.28x | 3.22x | 10.27x | 229.39x |
| skynet | 38373 us | 47492 us | 140220 us | 644348 us | 11877421 us |
| nqueens | 82844 us | 95309 us | 158274 us | 841336 us | 8189625 us |
| fib(40) | 106851 us | 154768 us | 437778 us | 412569 us | 29894700 us |

Benchmark configuration:
- Processor: EPYC 7742 64-core processor
- Worker Thread Count: 64 (no SMT)
- OS: Debian 12 Server
- Compiler: Clang 19.1.6 Release (-O3 -march=native)
- CPU boost enabled / schedutil governor
- Linked against libtcmalloc_minimal.so.4

Frameworks to come:
- (C++) [Staccato](https://github.com/rkuchumov/staccato)
- (C++) [Taskflow](https://github.com/taskflow/taskflow)
- (C#) .Net thread pool
- (Rust) [tokio](https://github.com/tokio-rs/tokio)
- (Golang) goroutines
- Facebook Folly
- PhotonLibOS https://github.com/alibaba/PhotonLibOS

Benchmarks to come:
- Lots of good inspiration [here](https://github.com/ConorWilliams/libfork/tree/main/bench/source)
