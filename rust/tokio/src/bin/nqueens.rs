// Recursive fork-join nqueens.
//
// Canonical (TooManyCooks) implementation:
// ../../../../cpp/TooManyCooks/nqueens.cpp
// which was in turn adapted from libfork's benchmark.
//
// At each column, fork a child task for every valid queen placement. Each fork
// is a `tokio::spawn`.

use std::time::Instant;
use tokio_bench::{multi_thread_runtime, peak_memory_usage, BoxFut};

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

fn nqueens(x_max: usize, buf: [i8; N]) -> BoxFut<i32> {
    Box::pin(async move {
        if N == x_max {
            return 1;
        }

        let mut handles = Vec::new();
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
                handles.push(tokio::spawn(nqueens(x_max + 1, next)));
            }
        }

        let mut ret = 0;
        for h in handles {
            ret += h.await.unwrap();
        }
        ret
    })
}

fn main() {
    let args: Vec<String> = std::env::args().collect();
    let thread_count = if args.len() > 1 {
        args[1].parse().expect("thread count")
    } else {
        tokio_bench::default_threads()
    };

    let rt = multi_thread_runtime(thread_count);
    println!("threads: {}", thread_count);

    rt.block_on(async move {
        {
            let buf = [0i8; N];
            let result = nqueens(0, buf).await; // warmup
            check_answer(result);
        }

        let start = Instant::now();
        let mut result = 0;
        for _ in 0..ITER_COUNT {
            let buf = [0i8; N];
            result = nqueens(0, buf).await;
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
