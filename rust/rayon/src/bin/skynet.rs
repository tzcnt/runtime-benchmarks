// The skynet benchmark as described here:
// https://github.com/atemerev/skynet
//
// Canonical (TooManyCooks) implementation:
// ../../../../cpp/TooManyCooks/skynet.cpp
//
// Each node forks 10 children until DEPTH_MAX levels deep (10^DEPTH_MAX = 100M
// leaf tasks) and sums their results. In rayon the 10-way fork-and-sum is a
// parallel iterator over 0..10: rayon adaptively splits the range across idle
// workers (via `join` internally) and reduces the per-child results with `sum`.

use rayon::prelude::*;
use rayon_bench::{build_pool, default_threads, peak_memory_usage};
use std::time::Instant;

const DEPTH_MAX: u64 = 8;
const ITER_COUNT: usize = 1;

fn skynet_one(base: u64, depth: u64) -> u64 {
    if depth == DEPTH_MAX {
        return base;
    }
    let mut offset = 1u64;
    for _ in 0..(DEPTH_MAX - depth - 1) {
        offset *= 10;
    }

    (0..10u64)
        .into_par_iter()
        .map(|i| skynet_one(base + offset * i, depth + 1))
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
