// Test performance of an async MPMC channel primitive.
//
// Canonical (TooManyCooks) implementation:
// ../../../../cpp/TooManyCooks/channel.cpp
//
// N producers each post a slice of `element_count` items; M consumers pull
// until the channel is drained; the total count and sum are validated.
//
// smol ships a native async MPMC channel: `smol::channel` (the async-channel
// crate), so no third-party channel is needed.

use smol::{future, Executor};
use smol_bench::{default_threads, init_executor, peak_memory_usage};
use std::time::Instant;

const ELEMENT_COUNT: u64 = 10_000_000;
const ITER_COUNT: usize = 1;

struct Counts {
    count: u64,
    sum: u64,
}

// Split ELEMENT_COUNT into per-producer (count, base) work assignments.
fn producer_assignments(producer_count: usize) -> Vec<(u64, u64)> {
    let per = ELEMENT_COUNT / producer_count as u64;
    let rem = ELEMENT_COUNT % producer_count as u64;
    let mut base = 0u64;
    let mut out = Vec::with_capacity(producer_count);
    for i in 0..producer_count as u64 {
        let count = if i < rem { per + 1 } else { per };
        out.push((count, base));
        base += count;
    }
    out
}

async fn do_bench(
    ex: &'static Executor<'static>,
    producer_count: usize,
    consumer_count: usize,
) -> u64 {
    let (tx, rx) = smol::channel::unbounded::<u64>();

    let mut producers = Vec::with_capacity(producer_count);
    for (count, base) in producer_assignments(producer_count) {
        let tx = tx.clone();
        producers.push(ex.spawn(async move {
            for i in 0..count {
                tx.send(base + i).await.unwrap();
            }
        }));
    }
    drop(tx); // once all producer clones finish, consumers see the channel close

    let mut consumers = Vec::with_capacity(consumer_count);
    for _ in 0..consumer_count {
        let rx = rx.clone();
        consumers.push(ex.spawn(async move {
            let mut count = 0u64;
            let mut sum = 0u64;
            while let Ok(v) = rx.recv().await {
                count += 1;
                sum += v;
            }
            Counts { count, sum }
        }));
    }
    drop(rx);

    for p in producers {
        p.await;
    }
    let mut totals = Counts { count: 0, sum: 0 };
    for c in consumers {
        let r = c.await;
        totals.count += r.count;
        totals.sum += r.sum;
    }

    let expected_sum = ELEMENT_COUNT * (ELEMENT_COUNT - 1) / 2;
    if totals.count != ELEMENT_COUNT {
        println!(
            "FAIL: Expected {} elements but consumed {} elements",
            ELEMENT_COUNT, totals.count
        );
    }
    if totals.sum != expected_sum {
        println!(
            "FAIL: Expected {} sum but got {} sum",
            expected_sum, totals.sum
        );
    }
    totals.sum
}

fn main() {
    let args: Vec<String> = std::env::args().collect();
    let thread_count = if args.len() > 1 {
        args[1].parse().expect("thread count")
    } else {
        default_threads()
    };

    let per = (thread_count / 2).max(1);
    let producer_count = per;
    let consumer_count = per;

    let ex = init_executor(thread_count);
    println!("threads: {}", thread_count);
    println!("producers: {}", producer_count);
    println!("consumers: {}", consumer_count);

    let result = future::block_on(ex.spawn(do_bench(ex, producer_count, consumer_count))); // warmup
    println!("output: {}", result);

    let start = Instant::now();
    for _ in 0..ITER_COUNT {
        let _ = future::block_on(ex.spawn(do_bench(ex, producer_count, consumer_count)));
    }
    let dur_us = start.elapsed().as_micros();

    let elements_per_sec = (ELEMENT_COUNT as u128) * 1_000_000 / dur_us.max(1);
    println!("runs:");
    println!("  - iteration_count: {}", ITER_COUNT);
    println!("    elements: {}", ELEMENT_COUNT);
    println!("    duration: {} us", dur_us);
    println!("    elements/sec: {}", elements_per_sec);
    println!("    max_rss: {} KiB", peak_memory_usage());
}
