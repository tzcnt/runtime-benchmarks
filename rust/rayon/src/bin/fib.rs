// An implementation of the recursive fork fibonacci parallelism test.
//
// Canonical (TooManyCooks) implementation:
// ../../../../cpp/TooManyCooks/fib.cpp
//
// Fork fib(n-1) and fib(n-2) with `rayon::join`: the first closure may be stolen
// and run on another worker while the current worker runs the second, and join
// waits for both legs. This is rayon's direct analogue of spawning a "hot" task
// that runs in parallel with the continuation and then joining it.

use rayon_bench::{build_pool, default_threads, peak_memory_usage};
use std::time::Instant;

const ITER_COUNT: usize = 1;

fn fib(n: u64) -> u64 {
    if n < 2 {
        return n;
    }
    let (x, y) = rayon::join(|| fib(n - 1), || fib(n - 2));
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
