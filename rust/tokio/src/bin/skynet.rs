// The skynet benchmark as described here:
// https://github.com/atemerev/skynet
//
// Canonical (TooManyCooks) implementation:
// ../../../../cpp/TooManyCooks/skynet.cpp
//
// Each node forks 10 children until DEPTH_MAX levels deep (10^DEPTH_MAX = 100M
// leaf tasks) and sums their results. Here each child is a `tokio::spawn`, so
// this measures tokio's task spawn/join throughput under deep nested fan-out.

use std::time::Instant;
use tokio_bench::{multi_thread_runtime, peak_memory_usage, BoxFut};

const DEPTH_MAX: u64 = 8;
const ITER_COUNT: usize = 1;

fn skynet_one(base: u64, depth: u64) -> BoxFut<u64> {
    Box::pin(async move {
        if depth == DEPTH_MAX {
            return base;
        }
        let mut offset = 1u64;
        for _ in 0..(DEPTH_MAX - depth - 1) {
            offset *= 10;
        }

        let mut handles = Vec::with_capacity(10);
        for i in 0..10u64 {
            handles.push(tokio::spawn(skynet_one(base + offset * i, depth + 1)));
        }

        let mut count = 0u64;
        for h in handles {
            count += h.await.unwrap();
        }
        count
    })
}

async fn skynet(expected: u64) {
    let count = skynet_one(0, 0).await;
    if count != expected {
        println!("ERROR: wrong result - {}", count);
    }
}

fn main() {
    let args: Vec<String> = std::env::args().collect();
    let thread_count = if args.len() > 1 {
        args[1].parse().expect("thread count")
    } else {
        tokio_bench::default_threads()
    };

    let leaves = 10u64.pow(DEPTH_MAX as u32);
    let expected = (leaves - 1) * leaves / 2;

    let rt = multi_thread_runtime(thread_count);
    println!("threads: {}", thread_count);

    rt.block_on(async move {
        skynet(expected).await; // warmup

        let start = Instant::now();
        for _ in 0..ITER_COUNT {
            skynet(expected).await;
        }
        let dur = start.elapsed();

        println!("runs:");
        println!("  - iteration_count: {}", ITER_COUNT);
        println!("    duration: {} us", dur.as_micros());
        println!("    max_rss: {} KiB", peak_memory_usage());
    });
}
