# runtime_benchmarks
Benchmarks to compare the performance of async runtimes / fork-join frameworks.

Currently only contains 3 frameworks:
- [TooManyCooks](https://github.com/tzcnt/TooManyCooks)
- [libfork](https://github.com/ConorWilliams/libfork)
- [concurrencpp](https://github.com/David-Haim/concurrencpp)

And 3 benchmarks:
- recursive fibonacci (forks x2)
- skynet ([original link](https://github.com/atemerev/skynet)) but increased to 100M tasks (forks x10)
- nqueens (forks up to x14)

Current Benchmark Results:

| Runtime | [libfork](https://github.com/ConorWilliams/libfork) | [TooManyCooks](https://github.com/tzcnt/TooManyCooks) | [concurrencpp](https://github.com/David-Haim/concurrencpp) |
| --- | --- | --- | --- |
| Mean Ratio to Best<br>(lower is better) | 1.00x | 1.15x | 190.99x |
| fib(40) | 138141 us | 164062 us | 29761567 us |
| skynet | 45584 us | 50445 us | 11943488 us |
| nqueens | 83572 us | 95431 us | 7981495 us |

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
