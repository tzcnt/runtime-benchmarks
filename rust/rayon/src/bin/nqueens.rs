// Recursive fork-join nqueens.
//
// Canonical (TooManyCooks) implementation:
// ../../../../cpp/TooManyCooks/nqueens.cpp
// which was in turn adapted from libfork's benchmark.
//
// At each column, fork a child task for every valid queen placement. The valid
// placements for the next column are gathered into a small vector, which is then
// consumed by a rayon parallel iterator: each child recurses in parallel and the
// solution counts are reduced with `sum`.

use rayon::prelude::*;
use rayon_bench::{build_pool, default_threads, peak_memory_usage};
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

fn nqueens(x_max: usize, buf: [i8; N]) -> i32 {
    if N == x_max {
        return 1;
    }

    let mut children: Vec<[i8; N]> = Vec::new();
    for y in 0..N {
        let q = y as i32;
        let mut valid = true;
        for x in 0..x_max {
            let p = buf[x] as i32;
            let d = (x_max - x) as i32;
            if q == p || q == p - d || q == p + d {
                valid = false;
                break;
            }
        }
        if valid {
            let mut next = buf;
            next[x_max] = y as i8;
            children.push(next);
        }
    }

    children
        .into_par_iter()
        .map(|next| nqueens(x_max + 1, next))
        .sum()
}

fn main() {
    let args: Vec<String> = std::env::args().collect();
    let thread_count = if args.len() > 1 {
        args[1].parse().expect("thread count")
    } else {
        default_threads()
    };

    let pool = build_pool(thread_count);
    println!("threads: {}", thread_count);

    pool.install(|| {
        {
            let buf = [0i8; N];
            let result = nqueens(0, buf); // warmup
            check_answer(result);
        }

        let start = Instant::now();
        let mut result = 0;
        for _ in 0..ITER_COUNT {
            let buf = [0i8; N];
            result = nqueens(0, buf);
            check_answer(result);
        }
        let dur = start.elapsed();
        println!("output: {}", result);

        println!("runs:");
        println!("  - iteration_count: {}", ITER_COUNT);
        println!("    duration: {} us", dur.as_micros());
        println!("    max_rss: {} KiB", peak_memory_usage());
    });
}
