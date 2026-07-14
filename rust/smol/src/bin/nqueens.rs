// Recursive fork-join nqueens.
//
// Canonical (TooManyCooks) implementation:
// ../../../../cpp/TooManyCooks/nqueens.cpp
// which was in turn adapted from libfork's benchmark.
//
// At each column, an iterator over the N rows filters the valid queen
// placements and spawns a child task for each one, collecting the `Task`
// handles (mirroring TMC's `spawn_many` over a task iterator); awaiting them
// in order is the join.

use smol::{future, Executor, Task};
use smol_bench::{default_threads, init_executor, peak_memory_usage, BoxFut};
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

// A queen on row `q` of the next column is valid if it doesn't share a row or
// diagonal with any of the queens already placed in `buf[0..x_max]`.
fn valid_placement(buf: &[i8; N], x_max: usize, q: i32) -> bool {
    (0..x_max).all(|x| {
        let p = buf[x] as i32;
        let d = (x_max - x) as i32;
        q != p && q != p - d && q != p + d
    })
}

fn nqueens(ex: &'static Executor<'static>, x_max: usize, buf: [i8; N]) -> BoxFut<i32> {
    Box::pin(async move {
        if N == x_max {
            return 1;
        }

        let children: Vec<Task<i32>> = (0..N)
            .filter(|&y| valid_placement(&buf, x_max, y as i32))
            .map(|y| {
                let mut next = buf;
                next[x_max] = y as i8;
                ex.spawn(nqueens(ex, x_max + 1, next))
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

    let ex = init_executor(thread_count);
    println!("threads: {}", thread_count);

    {
        let buf = [0i8; N];
        let result = future::block_on(ex.spawn(nqueens(ex, 0, buf))); // warmup
        check_answer(result);
    }

    let start = Instant::now();
    let mut result = 0;
    for _ in 0..ITER_COUNT {
        let buf = [0i8; N];
        result = future::block_on(ex.spawn(nqueens(ex, 0, buf)));
        check_answer(result);
    }
    let dur = start.elapsed();
    println!("output: {}", result);

    println!("runs:");
    println!("  - iteration_count: {}", ITER_COUNT);
    println!("    duration: {} us", dur.as_micros());
    println!("    max_rss: {} KiB", peak_memory_usage());
}
