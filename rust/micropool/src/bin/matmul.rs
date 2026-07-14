// Recursive divide-and-conquer matrix multiplication.
//
// Canonical (TooManyCooks) implementation:
// ../../../../cpp/TooManyCooks/matmul.cpp
// base case: ../../../../cpp/2common/matmul.hpp
//
// The matrices are split into quadrants recursively down to a 32x32 base case.
// The 8 sub-multiplications are run as two sequential groups of 4 parallel
// tasks (`run_group`) so that no output tile is written by two tasks at once.
// micropool only offers a binary `join`, so each group of 4 is a nested
// `join(join(m0, m1), join(m2, m3))`. Every `join` reuses the fixed pool made
// current by `pool.install(...)`; no threads are spawned while the benchmark
// runs.
//
// Rust's borrow checker cannot see that the parallel tasks write to disjoint
// regions of C, so - exactly like the C++ version's `int*` arithmetic - we use
// raw pointers. `Mat` bundles them into a `Send` value that can cross the join
// boundary. This is sound because: the tasks only read A/B and write disjoint
// sub-tiles of C, and the owning `Vec`s outlive the joins.

use micropool::join;
use micropool_bench::{build_pool, default_threads, peak_memory_usage};
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
                unsafe { *c.add(i * nn + j) += *a.add(i * nn + k) * *b.add(k * nn + j) };
            }
        }
    }
}

// Compute the two groups of four sub-multiplications. Kept as a plain function so
// the raw pointers never live across a join; only the `Send` `Mat` values cross
// the boundary.
fn split_quadrants(m: Mat) -> ([Mat; 4], [Mat; 4]) {
    let k = m.n / 2;
    let nn = m.nn;
    let (a, b, c) = (m.a, m.b, m.c);
    unsafe {
        // Group 1: the four products that first write the C tiles.
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

fn matmul(m: Mat) {
    if m.n <= 32 {
        unsafe { matmul_small(m) };
        return;
    }
    let (group1, group2) = split_quadrants(m);
    run_group(group1);
    run_group(group2);
}

// Run a group of four independent sub-multiplications in parallel via a nested
// binary join. `move` closures capture the (Copy) `Mat` values by value: a
// borrow would require `Mat: Sync` (it holds raw pointers and is only `Send`),
// whereas an owned `Mat` is `Send`, which is all `join` requires.
fn run_group(g: [Mat; 4]) {
    join(
        move || {
            join(move || matmul(g[0]), move || matmul(g[1]));
        },
        move || {
            join(move || matmul(g[2]), move || matmul(g[3]));
        },
    );
}

fn run_matmul(n: usize) -> Vec<i32> {
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
    matmul(root);
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

    let pool = build_pool(thread_count);
    println!("threads: {}", thread_count);

    pool.install(|| {
        let _ = run_matmul(n); // warmup

        println!("runs:");
        let start = Instant::now();
        let result = run_matmul(n);
        let dur = start.elapsed();
        validate(&result, n);

        println!("  - matrix_size: {}", n);
        println!("    duration: {} us", dur.as_micros());
        println!("    max_rss: {} KiB", peak_memory_usage());
    });
}
