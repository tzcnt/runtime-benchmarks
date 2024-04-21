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

| Runtime | [libfork](https://github.com/ConorWilliams/libfork) | [TooManyCooks](https://github.com/tzcnt/TooManyCooks) | [concurrencpp](https://github.com/David-Haim/concurrencpp) |
| --- | --- | --- | --- |
| Mean Ratio to Best<br>(lower is better) | 1.00x | 1.63x | 163.49x |
| fib(35) | 19528 us | 22755 us | 2806699 us |
| fib(40) | 139774 us | 159085 us | 30064024 us |
| fib(45) | 1412147 us | 1720917 us | 326289821 us |
| skynet (first run) | 1404 us | 4754 us | 136142 us |
| skynet (last run) | 1046 us | 1302 us | 136593 us |

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
