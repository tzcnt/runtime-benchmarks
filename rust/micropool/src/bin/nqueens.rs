// Recursive fork-join nqueens.
//
// Canonical (TooManyCooks) implementation:
// ../../../../cpp/TooManyCooks/nqueens.cpp
// which was in turn adapted from libfork's benchmark.
//
// At each column we gather every valid queen placement into a small vector of
// boards, then reduce over them. micropool offers both a free `join` and (via
// its re-exported paralight iterators) a range/collection dispatch; unlike
// skynet, nqueens is fastest with the binary-`join` reduction here. Its tree is
// deep with a small, shrinking branching factor, so most nodes fork only a
// handful of children: paralight's `split_by_threads` eagerly splits every
// dispatch into `num_threads + 1` chunks regardless of size, and that per-node
// setup cost dominates (~2x slower than `join` at 8 threads). `join`, by
// contrast, is cheap per call and the node count is modest, so the balanced
// binary reduction over the child slice (the same shape rayon's
// `into_par_iter().sum()` produces) wins. The vector lives on the current frame,
// which outlives the synchronous `join`, so the borrowed sub-slices handed to
// each closure stay valid. Every `join` reuses the fixed pool made current by
// `pool.install(...)`; no threads are spawned while the benchmark runs.

use micropool::join;
use micropool_bench::{build_pool, default_threads, peak_memory_usage};
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

    reduce(&children, x_max + 1)
}

fn reduce(children: &[[i8; N]], x: usize) -> i32 {
    match children.len() {
        0 => 0,
        1 => nqueens(x, children[0]),
        _ => {
            let mid = children.len() / 2;
            let (l, r) = join(
                || reduce(&children[..mid], x),
                || reduce(&children[mid..], x),
            );
            l + r
        }
    }
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
