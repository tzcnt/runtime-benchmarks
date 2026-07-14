// Recursive fork-join nqueens.
//
// Canonical (TooManyCooks) implementation:
// ../../../../cpp/TooManyCooks/nqueens.cpp
// which was in turn adapted from libfork's benchmark.
//
// At each column, fork a child task for every valid queen placement: the valid
// placements are a filtered range iterator fed directly into
// `Context::submit`. beekeeper has no in-task join, so instead of summing
// child results in the parent, tasks that reach a full board return 1 (0
// otherwise) and the drainer threads sum the hive's outcome stream to count
// the solutions.

use beekeeper::bee::{Context, Worker, WorkerResult};
use beekeeper::hive::outcome_channel;
use beekeeper_bench::{
    build_hive, default_threads, drainer_count, join_outcome_drainers, peak_memory_usage,
    spawn_outcome_drainers, WorkstealingHive,
};
use std::time::Instant;

const N: usize = 14; // board size / nqueens_work
const ITER_COUNT: usize = 1;

// answers[k] = number of solutions to the k-queens problem.
const ANSWERS: [u64; 19] = [
    0, 1, 0, 0, 2, 10, 4, 40, 92, 352, 724, 2680, 14200, 73712, 365596, 2279184, 14772512,
    95815104, 666090624,
];

fn check_answer(result: u64) {
    if result != ANSWERS[N] {
        println!("error: expected {}, got {}", ANSWERS[N], result);
    }
}

#[derive(Debug, Default)]
struct NqueensWorker;

impl Worker for NqueensWorker {
    // (partial board, number of columns already placed)
    type Input = ([i8; N], usize);
    type Output = u64;
    type Error = ();

    fn apply(
        &mut self,
        (buf, x_max): ([i8; N], usize),
        ctx: &Context<([i8; N], usize)>,
    ) -> WorkerResult<Self> {
        if x_max == N {
            return Ok(1);
        }

        (0..N)
            .filter(|&y| {
                let q = y as i32;
                (0..x_max).all(|x| {
                    let p = buf[x] as i32;
                    let d = (x_max - x) as i32;
                    q != p && q != p - d && q != p + d
                })
            })
            .for_each(|y| {
                let mut next = buf;
                next[x_max] = y as i8;
                ctx.submit((next, x_max + 1)).expect("submit nqueens child");
            });
        Ok(0)
    }
}

fn nqueens(hive: &WorkstealingHive<NqueensWorker>, drainers: usize) -> u64 {
    let (tx, rx) = outcome_channel::<NqueensWorker>();
    let handles = spawn_outcome_drainers(&rx, drainers);
    hive.apply_send(([0i8; N], 0), &tx);
    drop(tx);
    drop(rx);
    let (_tasks, solutions) = join_outcome_drainers(handles);
    solutions
}

fn main() {
    let args: Vec<String> = std::env::args().collect();
    let thread_count = if args.len() > 1 {
        args[1].parse().expect("thread count")
    } else {
        default_threads()
    };

    let hive = build_hive::<NqueensWorker>(thread_count);
    let drainers = drainer_count(thread_count);
    println!("threads: {}", thread_count);

    check_answer(nqueens(&hive, drainers)); // warmup

    let start = Instant::now();
    let mut result = 0;
    for _ in 0..ITER_COUNT {
        result = nqueens(&hive, drainers);
        check_answer(result);
    }
    let dur = start.elapsed();
    println!("output: {}", result);

    println!("runs:");
    println!("  - iteration_count: {}", ITER_COUNT);
    println!("    duration: {} us", dur.as_micros());
    println!("    max_rss: {} KiB", peak_memory_usage());
}
