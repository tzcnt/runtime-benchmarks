// Recursive fork-join nqueens.
//
// Canonical (TooManyCooks) implementation:
// ../../../../cpp/TooManyCooks/nqueens.cpp
// which was in turn adapted from libfork's benchmark.
//
// At each column, fork a child task for every valid queen placement. The forks
// are expressed as an iterator over the candidate rows: filter the valid
// placements, spawn each child on the pool, and collect the `Task` handles;
// awaiting them in order is the join.

use bevy_tasks::{block_on, TaskPool};
use bevy_tasks_bench::{default_threads, init_pool, peak_memory_usage, BoxFut};
use std::time::Instant;

const N: usize = 14; // board size / nqueens_work
const ITER_COUNT: usize = 1;

// answers[k] = number of solutions to the k-queens problem.
const ANSWERS: [i32; 19] = [
    0, 1, 0, 0, 2, 10, 4, 40, 92, 352, 724, 2680, 14200, 73712, 365596, 2279184, 14772512,
    95815104, 666090624,
];

fn check_answer(result: i32) {
    if result != ANSWERS[N] {
        println!("error: expected {}, got {}", ANSWERS[N], result);
    }
}

fn valid_placement(buf: &[i8; N], x_max: usize, y: usize) -> bool {
    let q = y as i32;
    for x in 0..x_max {
        let p = buf[x] as i32;
        let d = (x_max - x) as i32;
        if q == p || q == p - d || q == p + d {
            return false;
        }
    }
    true
}

fn nqueens(pool: &'static TaskPool, x_max: usize, buf: [i8; N]) -> BoxFut<i32> {
    Box::pin(async move {
        if N == x_max {
            return 1;
        }

        let children: Vec<_> = (0..N)
            .filter(|&y| valid_placement(&buf, x_max, y))
            .map(|y| {
                let mut next = buf;
                next[x_max] = y as i8;
                pool.spawn(nqueens(pool, x_max + 1, next))
            })
            .collect();

        let mut ret = 0;
        for c in children {
            ret += c.await;
        }
        ret
    })
}

fn main() {
    let args: Vec<String> = std::env::args().collect();
    let thread_count = if args.len() > 1 {
        args[1].parse().expect("thread count")
    } else {
        default_threads()
    };

    let pool = init_pool(thread_count);
    println!("threads: {}", thread_count);

    {
        let buf = [0i8; N];
        let result = block_on(pool.spawn(nqueens(pool, 0, buf))); // warmup
        check_answer(result);
    }

    let start = Instant::now();
    let mut result = 0;
    for _ in 0..ITER_COUNT {
        let buf = [0i8; N];
        result = block_on(pool.spawn(nqueens(pool, 0, buf)));
        check_answer(result);
    }
    let dur = start.elapsed();
    println!("output: {}", result);

    println!("runs:");
    println!("  - iteration_count: {}", ITER_COUNT);
    println!("    duration: {} us", dur.as_micros());
    println!("    max_rss: {} KiB", peak_memory_usage());
}
