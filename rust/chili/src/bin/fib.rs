// An implementation of the recursive fork fibonacci parallelism test.
//
// Canonical (TooManyCooks) implementation:
// ../../../../cpp/TooManyCooks/fib.cpp
//
// chili (the Rust port of Spice) is a heartbeat-scheduled fork-join library: its
// `Scope::join` is almost identical to rayon's `join`, but forking is nearly
// free and work is only actually shared with another thread on a periodic
// heartbeat. Each recursive call threads the `&mut Scope` down so the join tree
// stays anchored to one pool.

use chili::Scope;
use chili_bench::{build_pool, default_threads, peak_memory_usage};
use std::time::Instant;

const ITER_COUNT: usize = 1;

fn fib(n: u64, scope: &mut Scope<'_>) -> u64 {
    if n < 2 {
        return n;
    }
    let (x, y) = scope.join(|s| fib(n - 1, s), |s| fib(n - 2, s));
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
    let mut scope = pool.scope();
    println!("threads: {}", thread_count);

    let _ = fib(30, &mut scope); // warmup

    let start = Instant::now();
    let mut result = 0u64;
    for _ in 0..ITER_COUNT {
        result = fib(n, &mut scope);
    }
    let dur = start.elapsed();
    println!("output: {}", result);

    println!("runs:");
    println!("  - iteration_count: {}", ITER_COUNT);
    println!("    duration: {} us", dur.as_micros());
    println!("    max_rss: {} KiB", peak_memory_usage());
}
