# runtime_benchmarks
Benchmarks to compare the performance of async runtimes / fork-join frameworks.

Currently only contains 3 frameworks:
- [TooManyCooks](https://github.com/tzcnt/TooManyCooks)
- [libfork](https://github.com/ConorWilliams/libfork)
- [concurrencpp](https://github.com/David-Haim/concurrencpp)

And 2 benchmarks:
- skynet (as originally described [here](https://github.com/atemerev/skynet))
- recursive/parallel fibonacci

Current Benchmark Results:

| Runtime | [TooManyCooks](https://github.com/tzcnt/TooManyCooks) | [libfork](https://github.com/ConorWilliams/libfork) | [concurrencpp](https://github.com/David-Haim/concurrencpp) |
| --- | --- | --- | --- |
| Mean Ratio to Best<br>(lower is better) | 1.16x | 1.29x | 134.33x |
| fib(35) | 25219 us | 33416 us | 2828353 us |
| fib(40) | 160893 us | 204698 us | 29931094 us |
| fib(45) | 1712947 us | 2177882 us | 326018179 us |
| skynet (first run) | 4299 us | 2372 us | 142357 us |
| skynet (last run) | 1199 us | 1909 us | 147620 us |

Benchmark configuration:
- Processor: EPYC 7742 64-core processor
- Worker Thread Count: 64 (no SMT)
- OS: Debian 12 Server
- Compiler: Clang 18.1.2 Release (-O3 -march=native)
- CPU boost enabled / schedutil governor
- Linked against libtcmalloc_minimal.so.4

Frameworks to come:
- (C++) Intel TBB
- (C++) [Staccato](https://github.com/rkuchumov/staccato)
- (C++) [Taskflow](https://github.com/taskflow/taskflow)
- (C#) .Net thread pool
- (Rust) [tokio](https://github.com/tokio-rs/tokio)
- (Golang) goroutines

Benchmarks to come:
- Lots of good inspiration [here](https://github.com/ConorWilliams/libfork/tree/main/bench/source)
