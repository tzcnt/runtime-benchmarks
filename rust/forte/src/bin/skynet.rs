// The skynet benchmark as described here:
// https://github.com/atemerev/skynet
//
// Canonical (TooManyCooks) implementation:
// ../../../../cpp/TooManyCooks/skynet.cpp
//
// Each node forks 10 children until DEPTH_MAX levels deep (10^DEPTH_MAX = 100M
// leaf tasks) and sums their results. forte only offers a binary `join`, so the
// 10-way fan-out is expressed as a balanced binary reduction over the child
// index range [0,10) - the same shape rayon's `into_par_iter().sum()` produces
// internally, and identical to the chili benchmark.

use forte::Worker;
use forte_bench::{default_threads, peak_memory_usage, size_pool, POOL};
use std::time::Instant;

const DEPTH_MAX: u64 = 8;
const ITER_COUNT: usize = 1;

fn skynet_one(base: u64, depth: u64, w: &Worker) -> u64 {
    if depth == DEPTH_MAX {
        return base;
    }
    let offset = 10u64.pow((DEPTH_MAX - depth - 1) as u32);
    sum_range(base, offset, depth, 0, 10, w)
}

// Sum children [lo,hi) of a node whose child 0 has value `base` and whose
// children are spaced `offset` apart, all at `depth`.
fn sum_range(base: u64, offset: u64, depth: u64, lo: u64, hi: u64, w: &Worker) -> u64 {
    if hi - lo == 1 {
        return skynet_one(base + offset * lo, depth + 1, w);
    }
    let mid = lo + (hi - lo) / 2;
    let (l, r) = w.join(
        |w| sum_range(base, offset, depth, lo, mid, w),
        |w| sum_range(base, offset, depth, mid, hi, w),
    );
    l + r
}

fn skynet(expected: u64, w: &Worker) {
    let count = skynet_one(0, 0, w);
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

    size_pool(thread_count);
    println!("threads: {}", thread_count);

    POOL.with_worker(|w| {
        skynet(expected, w); // warmup

        let start = Instant::now();
        for _ in 0..ITER_COUNT {
            skynet(expected, w);
        }
        let dur = start.elapsed();

        println!("runs:");
        println!("  - iteration_count: {}", ITER_COUNT);
        println!("    duration: {} us", dur.as_micros());
        println!("    max_rss: {} KiB", peak_memory_usage());
    });
}
