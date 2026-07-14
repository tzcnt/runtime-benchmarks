// An implementation of the recursive fork fibonacci parallelism test.
//
// Canonical (TooManyCooks) implementation:
// ../../../../cpp/TooManyCooks/fib.cpp
//
// Spawn a hot task that runs in parallel with the current task (fib(n-1)), then
// continue the other leg (fib(n-2)) serially. Each fork is a `pool.spawn`;
// awaiting the returned `Task` is the join (dropping it would cancel the task).

use bevy_tasks::{block_on, TaskPool};
use bevy_tasks_bench::{default_threads, init_pool, peak_memory_usage, BoxFut};
use std::time::Instant;

const ITER_COUNT: usize = 1;

fn fib(pool: &'static TaskPool, n: u64) -> BoxFut<u64> {
    Box::pin(async move {
        if n < 2 {
            return n;
        }
        let x_hot = pool.spawn(fib(pool, n - 1));
        let y = fib(pool, n - 2).await;
        let x = x_hot.await;
        x + y
    })
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

    let pool = init_pool(thread_count);
    println!("threads: {}", thread_count);

    let _ = block_on(pool.spawn(fib(pool, 30))); // warmup

    let start = Instant::now();
    let mut result = 0u64;
    for _ in 0..ITER_COUNT {
        result = block_on(pool.spawn(fib(pool, n)));
    }
    let dur = start.elapsed();
    println!("output: {}", result);

    println!("runs:");
    println!("  - iteration_count: {}", ITER_COUNT);
    println!("    duration: {} us", dur.as_micros());
    println!("    max_rss: {} KiB", peak_memory_usage());
}
