// The skynet benchmark as described here:
// https://github.com/atemerev/skynet
//
// Canonical (TooManyCooks) implementation:
// ../../../../cpp/TooManyCooks/skynet.cpp
//
// Each node forks 10 children until DEPTH_MAX levels deep (10^DEPTH_MAX = 100M
// leaf tasks) and sums their results. The 10-way fork is an iterator over 0..10
// that spawns each child on the pool and collects the `Task` handles; awaiting
// them in order is the join. This measures bevy_tasks' task spawn/join
// throughput under deep nested fan-out.

use bevy_tasks::{block_on, TaskPool};
use bevy_tasks_bench::{default_threads, init_pool, peak_memory_usage, BoxFut};
use std::time::Instant;

const DEPTH_MAX: u64 = 8;
const ITER_COUNT: usize = 1;

fn skynet_one(pool: &'static TaskPool, base: u64, depth: u64) -> BoxFut<u64> {
    Box::pin(async move {
        if depth == DEPTH_MAX {
            return base;
        }
        let mut offset = 1u64;
        for _ in 0..(DEPTH_MAX - depth - 1) {
            offset *= 10;
        }

        let children: Vec<_> = (0..10u64)
            .map(|i| pool.spawn(skynet_one(pool, base + offset * i, depth + 1)))
            .collect();

        let mut count = 0u64;
        for c in children {
            count += c.await;
        }
        count
    })
}

fn skynet(pool: &'static TaskPool, expected: u64) {
    let count = block_on(pool.spawn(skynet_one(pool, 0, 0)));
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

    let pool = init_pool(thread_count);
    println!("threads: {}", thread_count);

    skynet(pool, expected); // warmup

    let start = Instant::now();
    for _ in 0..ITER_COUNT {
        skynet(pool, expected);
    }
    let dur = start.elapsed();

    println!("runs:");
    println!("  - iteration_count: {}", ITER_COUNT);
    println!("    duration: {} us", dur.as_micros());
    println!("    max_rss: {} KiB", peak_memory_usage());
}
