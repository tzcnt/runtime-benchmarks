// An implementation of the recursive fork fibonacci parallelism test.
//
// Canonical (TooManyCooks) implementation:
// ../../../../cpp/TooManyCooks/fib.cpp
//
// beekeeper has no in-task join: a worker can fork subtasks with
// `Context::submit`, but their results are delivered to the hive's outcome
// channel rather than back to the parent. So instead of each node summing its
// children, every node returns its own leaf contribution (n for n < 2, else 0)
// and the drainer threads sum the outcome stream. The sum over the whole call
// tree of fib's leaves (fib(1) = 1, fib(0) = 0) equals fib(n).

use beekeeper::bee::{Context, Worker, WorkerResult};
use beekeeper::hive::outcome_channel;
use beekeeper_bench::{
    build_hive, default_threads, drainer_count, join_outcome_drainers, peak_memory_usage,
    spawn_outcome_drainers, WorkstealingHive,
};
use std::time::Instant;

const ITER_COUNT: usize = 1;

#[derive(Debug, Default)]
struct FibWorker;

impl Worker for FibWorker {
    type Input = u64;
    type Output = u64;
    type Error = ();

    fn apply(&mut self, n: u64, ctx: &Context<u64>) -> WorkerResult<Self> {
        if n < 2 {
            return Ok(n);
        }
        ctx.submit(n - 1).expect("submit fib(n-1)");
        ctx.submit(n - 2).expect("submit fib(n-2)");
        Ok(0)
    }
}

fn fib(hive: &WorkstealingHive<FibWorker>, n: u64, drainers: usize) -> u64 {
    let (tx, rx) = outcome_channel::<FibWorker>();
    let handles = spawn_outcome_drainers(&rx, drainers);
    hive.apply_send(n, &tx);
    // Every forked subtask carries a clone of tx; dropping ours means the
    // drainers see disconnect exactly when the whole call tree has finished.
    drop(tx);
    drop(rx);
    let (_count, sum) = join_outcome_drainers(handles);
    sum
}

fn fib_sequential(n: u64) -> u64 {
    let (mut a, mut b) = (0u64, 1u64);
    for _ in 0..n {
        (a, b) = (b, a + b);
    }
    a
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

    let hive = build_hive::<FibWorker>(thread_count);
    let drainers = drainer_count(thread_count);
    println!("threads: {}", thread_count);

    let _ = fib(&hive, 30, drainers); // warmup

    let start = Instant::now();
    let mut result = 0u64;
    for _ in 0..ITER_COUNT {
        result = fib(&hive, n, drainers);
    }
    let dur = start.elapsed();
    if result != fib_sequential(n) {
        println!("error: expected {}, got {}", fib_sequential(n), result);
    }
    println!("output: {}", result);

    println!("runs:");
    println!("  - iteration_count: {}", ITER_COUNT);
    println!("    duration: {} us", dur.as_micros());
    println!("    max_rss: {} KiB", peak_memory_usage());
}
