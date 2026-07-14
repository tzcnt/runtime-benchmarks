// An implementation of the recursive fork fibonacci parallelism test.
//
// Canonical (TooManyCooks) implementation:
// ../../../../cpp/TooManyCooks/fib.cpp
//
// Spawn a hot task that runs in parallel with the current task (fib(n-1)), then
// continue the other leg (fib(n-2)) serially. Each fork is a `tokio::spawn`.

use std::time::Instant;
use tokio_bench::{multi_thread_runtime, peak_memory_usage, BoxFut};

const ITER_COUNT: usize = 1;

fn fib(n: u64) -> BoxFut<u64> {
    Box::pin(async move {
        if n < 2 {
            return n;
        }
        let x_hot = tokio::spawn(fib(n - 1));
        let y = fib(n - 2).await;
        let x = x_hot.await.unwrap();
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
        tokio_bench::default_threads()
    };

    let rt = multi_thread_runtime(thread_count);
    println!("threads: {}", thread_count);

    rt.block_on(async move {
        let _ = fib(30).await; // warmup

        let start = Instant::now();
        let mut result = 0u64;
        for _ in 0..ITER_COUNT {
            result = fib(n).await;
        }
        let dur = start.elapsed();
        println!("output: {}", result);

        println!("runs:");
        println!("  - iteration_count: {}", ITER_COUNT);
        println!("    duration: {} us", dur.as_micros());
        println!("    max_rss: {} KiB", peak_memory_usage());
    });
}
