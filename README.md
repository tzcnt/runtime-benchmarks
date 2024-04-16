# runtime_benchmarks
Benchmarks to compare the performance of async runtimes / fork-join frameworks.

Currently only contains 2 frameworks:
- [TooManyCooks](https://github.com/tzcnt/TooManyCooks)
- [concurrencpp](https://github.com/David-Haim/concurrencpp)

And 2 benchmarks:
- skynet (as originally described [here](https://github.com/atemerev/skynet))
- recursive/parallel fibonacci

Current Benchmark Results (all runtimes in microseconds):

| Benchmark | [TooManyCooks](https://github.com/tzcnt/TooManyCooks) | [concurrencpp](https://github.com/David-Haim/concurrencpp) |
| --- | --- | --- |
| skynet (cold / first iteration) | 5248 | 153187 |
| skynet (hot / last iteration) | 1875 | 133235 |
| fib(30) | 4681 | 271576 |
| fib(35) | 24836 | 2774196 |
| fib(40) | 164296 | 29810551 |
| fib(45) | 1684424 | 324080310 |

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
- (C#) .Net thread pool
- (Rust) [tokio](https://github.com/tokio-rs/tokio)
- (Golang) goroutines
