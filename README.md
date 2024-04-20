# runtime_benchmarks
Benchmarks to compare the performance of async runtimes / fork-join frameworks.

Currently only contains 2 frameworks:
- [TooManyCooks](https://github.com/tzcnt/TooManyCooks)
- [concurrencpp](https://github.com/David-Haim/concurrencpp)

And 2 benchmarks:
- skynet (as originally described [here](https://github.com/atemerev/skynet))
- recursive/parallel fibonacci

Current Benchmark Results:

| Runtime | [TooManyCooks](https://github.com/tzcnt/TooManyCooks) | [concurrencpp](https://github.com/David-Haim/concurrencpp) |
| --- | --- | --- |
| Mean Ratio to Best | 1.00x | 110.89x |
| fib(30) | 5377 us | 273548 us |
| fib(35) | 25304 us | 2781633 us |
| fib(40) | 162997 us | 30004624 us |
| fib(45) | 1710420 us | 326028382 us |
| skynet (first run) | 4419 us | 139690 us |
| skynet (last run) | 1433 us | 140767 us |

Benchmark configuration:
- Processor: EPYC 7742 64-core processor
- Worker Thread Count: 64 (no SMT)
- OS: Debian 12 Server
- Compiler: Clang 18.1.2 Release (-O3 -march=native)
- CPU boost enabled / schedutil governor
- Linked against libtcmalloc_minimal.so.4

Frameworks to come:
- (C++) Intel TBB
- (C++) [libfork](https://github.com/ConorWilliams/libfork)
- (C++) [Staccato](https://github.com/rkuchumov/staccato)
- (C++) [Taskflow](https://github.com/taskflow/taskflow)
- (C#) .Net thread pool
- (Rust) [tokio](https://github.com/tokio-rs/tokio)
- (Golang) goroutines

Benchmarks to come:
- Lots of good inspiration [here](https://github.com/ConorWilliams/libfork/tree/main/bench/source)