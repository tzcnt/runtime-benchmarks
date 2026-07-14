// An implementation of the recursive fork fibonacci parallelism test.
//
// Canonical (TooManyCooks) implementation:
// ../../../../cpp/TooManyCooks/fib.cpp
//
// forte is a heartbeat-scheduled fork-join scheduler: its `Worker::join` is
// almost identical to rayon's `join`, but forking is nearly free and work is
// only actually shared with another worker on a periodic heartbeat (the same
// design as chili/spice). Each recursive call threads the `&Worker` down so the
// join tree stays anchored to one pool.

use forte::Worker;
use forte_bench::{default_threads, peak_memory_usage, size_pool, POOL};
use std::time::Instant;

const ITER_COUNT: usize = 1;

fn fib(n: u64, w: &Worker) -> u64 {
    if n < 2 {
        return n;
    }
    let (x, y) = w.join(|w| fib(n - 1, w), |w| fib(n - 2, w));
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

    size_pool(thread_count);
    println!("threads: {}", thread_count);

    // The whole workload runs inside `with_worker` so the calling thread joins
    // the pool as a worker (analogous to rayon's `pool.install`).
    POOL.with_worker(|w| {
        let _ = fib(30, w); // warmup

        let start = Instant::now();
        let mut result = 0u64;
        for _ in 0..ITER_COUNT {
            result = fib(n, w);
        }
        let dur = start.elapsed();
        println!("output: {}", result);

        println!("runs:");
        println!("  - iteration_count: {}", ITER_COUNT);
        println!("    duration: {} us", dur.as_micros());
        println!("    max_rss: {} KiB", peak_memory_usage());
    });
}
