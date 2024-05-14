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
| Mean Ratio to Best<br>(lower is better) | 1.00x | 1.15x | 185.92x |
| fib(35) | 20347 us | 22377 us | 2744762 us |
| fib(40) | 138198 us | 157835 us | 29840393 us |
| fib(45) | 1413898 us | 1670629 us | 324076052 us |
| skynet | 46517 us | 52923 us | 11844876 us |
| nqueens | 84220 us | 98847 us | 7996503 us |

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
