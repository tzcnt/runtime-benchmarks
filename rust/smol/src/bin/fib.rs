// An implementation of the recursive fork fibonacci parallelism test.
//
// Canonical (TooManyCooks) implementation:
// ../../../../cpp/TooManyCooks/fib.cpp
//
// Spawn a hot task that runs in parallel with the current task (fib(n-1)), then
// continue the other leg (fib(n-2)) serially. Each fork is an
// `Executor::spawn`; awaiting the returned `Task` is the join.

use smol::{future, Executor};
use smol_bench::{default_threads, init_executor, peak_memory_usage, BoxFut};
use std::time::Instant;

const ITER_COUNT: usize = 1;

fn fib(ex: &'static Executor<'static>, n: u64) -> BoxFut<u64> {
    Box::pin(async move {
        if n < 2 {
            return n;
        }
        let x_hot = ex.spawn(fib(ex, n - 1));
        let y = fib(ex, n - 2).await;
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

    let ex = init_executor(thread_count);
    println!("threads: {}", thread_count);

    let _ = future::block_on(ex.spawn(fib(ex, 30))); // warmup

    let start = Instant::now();
    let mut result = 0u64;
    for _ in 0..ITER_COUNT {
        result = future::block_on(ex.spawn(fib(ex, n)));
    }
    let dur = start.elapsed();
    println!("output: {}", result);

    println!("runs:");
    println!("  - iteration_count: {}", ITER_COUNT);
    println!("    duration: {} us", dur.as_micros());
    println!("    max_rss: {} KiB", peak_memory_usage());
}
