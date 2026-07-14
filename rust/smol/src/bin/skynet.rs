// The skynet benchmark as described here:
// https://github.com/atemerev/skynet
//
// Canonical (TooManyCooks) implementation:
// ../../../../cpp/TooManyCooks/skynet.cpp
//
// Each node forks 10 children until DEPTH_MAX levels deep (10^DEPTH_MAX = 100M
// leaf tasks) and sums their results. The 10-way fork is an iterator over
// 0..10 that spawns each child on the executor and collects the `Task`
// handles (mirroring TMC's `spawn_many` over a task iterator); awaiting them
// in order is the join. This measures smol's task spawn/join throughput under
// deep nested fan-out.

use smol::{future, Executor, Task};
use smol_bench::{default_threads, init_executor, peak_memory_usage, BoxFut};
use std::time::Instant;

const DEPTH_MAX: u64 = 8;
const ITER_COUNT: usize = 1;

fn skynet_one(ex: &'static Executor<'static>, base: u64, depth: u64) -> BoxFut<u64> {
    Box::pin(async move {
        if depth == DEPTH_MAX {
            return base;
        }
        let mut offset = 1u64;
        for _ in 0..(DEPTH_MAX - depth - 1) {
            offset *= 10;
        }

        let children: Vec<Task<u64>> = (0..10u64)
            .map(|i| ex.spawn(skynet_one(ex, base + offset * i, depth + 1)))
            .collect();

        let mut count = 0u64;
        for c in children {
            count += c.await;
        }
        count
    })
}

fn skynet(ex: &'static Executor<'static>, expected: u64) {
    let count = future::block_on(ex.spawn(skynet_one(ex, 0, 0)));
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

    let ex = init_executor(thread_count);
    println!("threads: {}", thread_count);

    skynet(ex, expected); // warmup

    let start = Instant::now();
    for _ in 0..ITER_COUNT {
        skynet(ex, expected);
    }
    let dur = start.elapsed();

    println!("runs:");
    println!("  - iteration_count: {}", ITER_COUNT);
    println!("    duration: {} us", dur.as_micros());
    println!("    max_rss: {} KiB", peak_memory_usage());
}
