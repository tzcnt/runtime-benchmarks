// An implementation of the recursive fork fibonacci parallelism test.
//
// Canonical (TooManyCooks) implementation:
// ../../../../cpp/TooManyCooks/fib.cpp
//
// micropool is a fixed-size, low-latency work-stealing pool with a rayon-style
// free `join`: it *potentially* runs the two closures in parallel (the first may
// be stolen by another worker while the caller runs the second) and returns once
// both are done. Forking never creates a thread - every worker exists from the
// moment the pool is built - so this measures the scheduler, not thread startup.
// `join` operates on the pool made current by `pool.install(...)`.

use micropool::join;
use micropool_bench::{build_pool, default_threads, peak_memory_usage};
use std::time::Instant;

const ITER_COUNT: usize = 1;

fn fib(n: u64) -> u64 {
    if n < 2 {
        return n;
    }
    let (x, y) = join(|| fib(n - 1), || fib(n - 2));
    x + y
}

fn main() {
    let args: Vec<String> = std::env::args().collect();
    if args.len() < 2 {
        println!("Usage: fib <n-th fibonacci number requested>");
        return;
    }
    let n: u64 = args[1].parse().expect("n");
    let thread_count = if args.len() > 2 {
        args[2].parse().expect("thread count")
    } else {
        default_threads()
    };

    let pool = build_pool(thread_count);
    println!("threads: {}", thread_count);

    pool.install(|| {
        let _ = fib(30); // warmup

        let start = Instant::now();
        let mut result = 0u64;
        for _ in 0..ITER_COUNT {
            result = fib(n);
        }
        let dur = start.elapsed();
        println!("output: {}", result);

        println!("runs:");
        println!("  - iteration_count: {}", ITER_COUNT);
        println!("    duration: {} us", dur.as_micros());
        println!("    max_rss: {} KiB", peak_memory_usage());
    });
}
