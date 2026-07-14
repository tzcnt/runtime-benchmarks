// The skynet benchmark as described here:
// https://github.com/atemerev/skynet
//
// Canonical (TooManyCooks) implementation:
// ../../../../cpp/TooManyCooks/skynet.cpp
//
// Each node forks 10 children until DEPTH_MAX levels deep (10^DEPTH_MAX = 100M
// leaf tasks) and sums their results. micropool re-exports paralight's parallel
// iterators driven by micropool's own fixed pool, so the 10-way fan-out is a
// parallel-map over the child index range 0..10 reduced with `sum` - the direct
// analogue of rayon's `into_par_iter().map().sum()`, and much cheaper here than a
// binary tree of `join`s: skynet's 100M tiny uniform nodes are dominated by the
// cost of micropool's spinning `join` (~9 per 10-way fork), so batched iteration
// runs ~10x faster (measured 6.5s vs 67s at 8 threads). The dispatch nests: each mapped
// child recurses into another parallel dispatch on the same pool, and no threads
// are spawned while the benchmark runs (`split_by_threads` reuses the pool made
// current by `pool.install(...)`).

use micropool::iter::*;
use micropool::split_by_threads;
use micropool_bench::{build_pool, default_threads, peak_memory_usage};
use std::time::Instant;

const DEPTH_MAX: u64 = 8;
const ITER_COUNT: usize = 1;

fn skynet_one(base: u64, depth: u64) -> u64 {
    if depth == DEPTH_MAX {
        return base;
    }
    let offset = 10u64.pow((DEPTH_MAX - depth - 1) as u32);
    (0..10usize)
        .into_par_iter()
        .with_thread_pool(split_by_threads())
        .map(move |i| skynet_one(base + offset * i as u64, depth + 1))
        .sum()
}

fn skynet(expected: u64) {
    let count = skynet_one(0, 0);
    if count != expected {
        println!("ERROR: wrong result - {}", count);
    }
}

fn main() {
    let args: Vec<String> = std::env::args().collect();
    let thread_count = if args.len() > 1 {
        args[1].parse().expect("thread count")
    } else {
        default_threads()
    };

    let leaves = 10u64.pow(DEPTH_MAX as u32);
    let expected = (leaves - 1) * leaves / 2;

    let pool = build_pool(thread_count);
    println!("threads: {}", thread_count);

    pool.install(|| {
        skynet(expected); // warmup

        let start = Instant::now();
        for _ in 0..ITER_COUNT {
            skynet(expected);
        }
        let dur = start.elapsed();

        println!("runs:");
        println!("  - iteration_count: {}", ITER_COUNT);
        println!("    duration: {} us", dur.as_micros());
        println!("    max_rss: {} KiB", peak_memory_usage());
    });
}
