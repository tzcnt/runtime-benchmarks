// Recursive divide-and-conquer matrix multiplication.
//
// Canonical (TooManyCooks) implementation:
// ../../../../cpp/TooManyCooks/matmul.cpp
// base case: ../../../../cpp/2common/matmul.hpp
//
// The matrices are split into quadrants recursively down to a 32x32 base case.
// The 8 sub-multiplications are run as two sequential groups of 4 parallel
// `Executor::spawn`s so that no output tile is written by two tasks at once.
//
// Rust's borrow checker cannot see that the parallel tasks write to disjoint
// regions of C, so - exactly like the C++ version's `int*` arithmetic - we use
// raw pointers. `Mat` bundles them into a `Send` value that can cross the
// spawn boundary. This is sound because: the tasks only read A/B and write
// disjoint sub-tiles of C, and the owning `Vec`s outlive the blocking call.

use smol::{future, Executor, Task};
use smol_bench::{default_threads, init_executor, peak_memory_usage, BoxFut};
use std::time::Instant;

#[derive(Clone, Copy)]
struct Mat {
    a: *const i32,
    b: *const i32,
    c: *mut i32,
    n: usize,
    nn: usize, // stride (the full matrix dimension N)
}
// SAFETY: each spawned task operates on disjoint sub-tiles of C and read-only A/B.
unsafe impl Send for Mat {}

unsafe fn matmul_small(m: Mat) {
    let Mat { a, b, c, n, nn } = m;
    for i in 0..n {
        for k in 0..n {
            for j in 0..n {
                *c.add(i * nn + j) += *a.add(i * nn + k) * *b.add(k * nn + j);
            }
        }
    }
}

// Compute the two groups of four sub-multiplications. Kept as a plain (non-async)
// function so the raw pointers never live across an `.await`; only the `Send`
// `Mat` values cross the spawn/await boundary.
fn split_quadrants(m: Mat) -> ([Mat; 4], [Mat; 4]) {
    let k = m.n / 2;
    let nn = m.nn;
    let (a, b, c) = (m.a, m.b, m.c);
    unsafe {
        // Group 1: the four products that only touch the "upper-left" halves.
        let group1 = [
            Mat { a, b, c, n: k, nn },
            Mat { a, b: b.add(k), c: c.add(k), n: k, nn },
            Mat { a: a.add(k * nn), b, c: c.add(k * nn), n: k, nn },
            Mat { a: a.add(k * nn), b: b.add(k), c: c.add(k * nn + k), n: k, nn },
        ];
        // Group 2: the accumulating products into the same tiles.
        let group2 = [
            Mat { a: a.add(k), b: b.add(k * nn), c, n: k, nn },
            Mat { a: a.add(k), b: b.add(k * nn + k), c: c.add(k), n: k, nn },
            Mat { a: a.add(k * nn + k), b: b.add(k * nn), c: c.add(k * nn), n: k, nn },
            Mat { a: a.add(k * nn + k), b: b.add(k * nn + k), c: c.add(k * nn + k), n: k, nn },
        ];
        (group1, group2)
    }
}

fn matmul(ex: &'static Executor<'static>, m: Mat) -> BoxFut<()> {
    Box::pin(async move {
        if m.n <= 32 {
            unsafe { matmul_small(m) };
            return;
        }
        let (group1, group2) = split_quadrants(m);
        run_group(ex, group1).await;
        run_group(ex, group2).await;
    })
}

async fn run_group(ex: &'static Executor<'static>, group: [Mat; 4]) {
    let children: Vec<Task<()>> = group.into_iter().map(|m| ex.spawn(matmul(ex, m))).collect();
    for c in children {
        c.await;
    }
}

fn run_matmul(ex: &'static Executor<'static>, n: usize) -> Vec<i32> {
    let a = vec![1i32; n * n];
    let b = vec![1i32; n * n];
    let mut c = vec![0i32; n * n];

    let root = Mat {
        a: a.as_ptr(),
        b: b.as_ptr(),
        c: c.as_mut_ptr(),
        n,
        nn: n,
    };
    future::block_on(ex.spawn(matmul(ex, root)));
    c
}

fn validate(c: &[i32], n: usize) {
    for i in 0..n {
        for j in 0..n {
            let res = c[i * n + j];
            if res != n as i32 {
                eprintln!("Wrong result at ({},{}) : {}. expected {}", i, j, res, n);
                std::process::exit(1);
            }
        }
    }
}

fn main() {
    let args: Vec<String> = std::env::args().collect();
    if args.len() < 2 {
        println!("Usage: matmul <matrix size (power of 2)>");
        return;
    }
    let n: usize = args[1].parse().expect("matrix size");
    let thread_count = if args.len() > 2 {
        args[2].parse().expect("thread count")
    } else {
        default_threads()
    };

    let ex = init_executor(thread_count);
    println!("threads: {}", thread_count);

    let _ = run_matmul(ex, n); // warmup

    println!("runs:");
    let start = Instant::now();
    let result = run_matmul(ex, n);
    let dur = start.elapsed();
    validate(&result, n);

    println!("  - matrix_size: {}", n);
    println!("    duration: {} us", dur.as_micros());
    println!("    max_rss: {} KiB", peak_memory_usage());
}
