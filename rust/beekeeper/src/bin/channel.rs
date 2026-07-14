// Test performance of the library's async queue primitive.
//
// Canonical (TooManyCooks) implementation:
// ../../../../cpp/TooManyCooks/channel.cpp
//
// N producers each post a slice of `element_count` items; M consumers pull
// until the queue is drained; the total count and sum are validated.
//
// beekeeper is a worker pool, not a channel library, but its core primitive IS
// an MPMC pipeline: inputs flow through the hive's task queues to the worker
// threads, and each result flows through the outcome channel to whoever
// receives it. This benchmark measures that pipeline end-to-end: producer
// threads submit u64 elements to a hive of `EchoWorker`s (the identity
// worker), and consumer threads drain the hive's outcome channel, summing the
// values. Producers and consumers are external OS threads (submission and
// outcome receipt cannot run on hive workers); the hive's worker threads are
// the benchmark's thread count, as with the other runtimes.

use beekeeper::bee::stock::EchoWorker;
use beekeeper::hive::outcome_channel;
use beekeeper_bench::{
    build_hive, default_threads, join_outcome_drainers, peak_memory_usage, spawn_outcome_drainers,
    WorkstealingHive,
};
use std::time::Instant;

const ELEMENT_COUNT: u64 = 10_000_000;
const ITER_COUNT: usize = 1;

type ChannelWorker = EchoWorker<u64>;

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

fn do_bench(
    hive: &WorkstealingHive<ChannelWorker>,
    producer_count: usize,
    consumer_count: usize,
) -> u64 {
    let (tx, rx) = outcome_channel::<ChannelWorker>();
    let consumers = spawn_outcome_drainers(&rx, consumer_count);
    drop(rx);

    let producers: Vec<_> = producer_assignments(producer_count)
        .into_iter()
        .map(|(count, base)| {
            // The hive is shallow-cloneable: all clones share the same queues
            // and worker threads, like cloning a channel sender.
            let hive = hive.clone();
            let tx = tx.clone();
            std::thread::spawn(move || {
                for i in 0..count {
                    hive.apply_send(base + i, &tx);
                }
            })
        })
        .collect();
    drop(tx);

    for p in producers {
        p.join().unwrap();
    }
    // Every in-flight task holds a clone of tx, so the consumers finish when
    // the last element's outcome has been delivered.
    let (count, sum) = join_outcome_drainers(consumers);

    let expected_sum = ELEMENT_COUNT * (ELEMENT_COUNT - 1) / 2;
    if count != ELEMENT_COUNT {
        println!(
            "FAIL: Expected {} elements but consumed {} elements",
            ELEMENT_COUNT, count
        );
    }
    if sum != expected_sum {
        println!("FAIL: Expected {} sum but got {} sum", expected_sum, sum);
    }
    sum
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

    let hive = build_hive::<ChannelWorker>(thread_count);
    println!("threads: {}", thread_count);
    println!("producers: {}", producer_count);
    println!("consumers: {}", consumer_count);

    let result = do_bench(&hive, producer_count, consumer_count); // warmup
    println!("output: {}", result);

    let start = Instant::now();
    for _ in 0..ITER_COUNT {
        let _ = do_bench(&hive, producer_count, consumer_count);
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
