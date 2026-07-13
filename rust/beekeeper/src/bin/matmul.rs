// Recursive divide-and-conquer matrix multiplication.
//
// Canonical (TooManyCooks) implementation:
// ../../../../cpp/TooManyCooks/matmul.cpp
// base case: ../../../../cpp/2common/matmul.hpp
//
// The matrices are split into quadrants recursively down to a 32x32 base case.
// The 8 sub-multiplications run as two sequential groups of 4 parallel tasks
// so that no output tile is written by two tasks at once.
//
// beekeeper has no in-task join, so the group barrier is expressed as
// continuation-passing dataflow: each recursive step allocates a `Node` with a
// countdown of 4 and holds group 2 aside. Whichever task finishes its subtree
// last (countdown hits 0) submits group 2 via `Context::submit`; when group 2
// finishes, completion propagates to the parent `Node`. A leaf task completes
// its node after running the 32x32 base case. The atomic countdown
// (AcqRel/Release) also provides the happens-before edge that lets group 2
// safely accumulate into the C tiles group 1 wrote.
//
// Rust's borrow checker cannot see that the parallel tasks write to disjoint
// regions of C, so - exactly like the C++ version's `int*` arithmetic - we use
// raw pointers. `Mat` bundles them into a `Send` value that can cross the task
// submission boundary. This is sound because: the tasks only read A/B and
// write disjoint sub-tiles of C, and the owning `Vec`s outlive the run (the
// drainer join at the end of run_matmul proves every task finished).

use beekeeper::bee::{Context, Worker, WorkerResult};
use beekeeper::hive::outcome_channel;
use beekeeper_bench::{
    build_hive, default_threads, join_outcome_drainers, peak_memory_usage, spawn_outcome_drainers,
    WorkstealingHive,
};
use std::sync::atomic::{AtomicUsize, Ordering};
use std::sync::{Arc, Mutex};
use std::time::Instant;

#[derive(Clone, Copy, Debug)]
struct Mat {
    a: *const i32,
    b: *const i32,
    c: *mut i32,
    n: usize,
    nn: usize, // stride (the full matrix dimension N)
}
// SAFETY: each task operates on disjoint sub-tiles of C and read-only A/B.
unsafe impl Send for Mat {}

// Fork-join continuation state for one recursive step. `remaining` counts the
// outstanding tasks of the current group; `group2` holds the second group of
// sub-multiplications until the first group completes.
#[derive(Debug)]
struct Node {
    remaining: AtomicUsize,
    group2: Mutex<Option<[Mat; 4]>>,
    parent: Option<Arc<Node>>,
}

// One task: multiply quadrant `m`, then signal `node` when the whole subtree
// rooted at `m` is done.
#[derive(Debug)]
struct Step {
    m: Mat,
    node: Arc<Node>,
}

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

// Compute the two groups of four sub-multiplications. Identical quadrant
// layout to the rayon/C++ versions.
fn split_quadrants(m: Mat) -> ([Mat; 4], [Mat; 4]) {
    let k = m.n / 2;
    let nn = m.nn;
    let (a, b, c) = (m.a, m.b, m.c);
    unsafe {
        // Group 1: the four products that only touch the "upper-left" halves.
        let group1 = [
            Mat { a, b, c, n: k, nn },
            Mat {
                a,
                b: b.add(k),
                c: c.add(k),
                n: k,
                nn,
            },
            Mat {
                a: a.add(k * nn),
                b,
                c: c.add(k * nn),
                n: k,
                nn,
            },
            Mat {
                a: a.add(k * nn),
                b: b.add(k),
                c: c.add(k * nn + k),
                n: k,
                nn,
            },
        ];
        // Group 2: the accumulating products into the same tiles.
        let group2 = [
            Mat {
                a: a.add(k),
                b: b.add(k * nn),
                c,
                n: k,
                nn,
            },
            Mat {
                a: a.add(k),
                b: b.add(k * nn + k),
                c: c.add(k),
                n: k,
                nn,
            },
            Mat {
                a: a.add(k * nn + k),
                b: b.add(k * nn),
                c: c.add(k * nn),
                n: k,
                nn,
            },
            Mat {
                a: a.add(k * nn + k),
                b: b.add(k * nn + k),
                c: c.add(k * nn + k),
                n: k,
                nn,
            },
        ];
        (group1, group2)
    }
}

// Signal that one task of `node`'s current group finished. The last finisher
// either releases the held-back group 2, or (if group 2 already ran)
// completes the node and recurses into the parent. Runs on whichever worker
// finished last, which has a `ctx` to submit the next group with.
fn complete(node: &Arc<Node>, ctx: &Context<Step>) {
    if node.remaining.fetch_sub(1, Ordering::AcqRel) != 1 {
        return;
    }
    let group2 = node.group2.lock().unwrap().take();
    if let Some(group2) = group2 {
        node.remaining.store(4, Ordering::Release);
        for m in group2 {
            ctx.submit(Step {
                m,
                node: Arc::clone(node),
            })
            .expect("submit matmul group 2");
        }
    } else if let Some(parent) = &node.parent {
        complete(parent, ctx);
    }
    // No parent: this is the root node; run_matmul's drainer join observes
    // completion via channel disconnect.
}

#[derive(Debug, Default)]
struct MatMulWorker;

impl Worker for MatMulWorker {
    type Input = Step;
    // The value is always 0; a u64 output lets matmul reuse the shared
    // outcome-drainer helper from lib.rs.
    type Output = u64;
    type Error = ();

    fn apply(&mut self, step: Step, ctx: &Context<Step>) -> WorkerResult<Self> {
        if step.m.n <= 32 {
            unsafe { matmul_small(step.m) };
            complete(&step.node, ctx);
        } else {
            let (group1, group2) = split_quadrants(step.m);
            let node = Arc::new(Node {
                remaining: AtomicUsize::new(4),
                group2: Mutex::new(Some(group2)),
                parent: Some(step.node),
            });
            for m in group1 {
                ctx.submit(Step {
                    m,
                    node: Arc::clone(&node),
                })
                .expect("submit matmul group 1");
            }
        }
        Ok(0)
    }
}

fn run_matmul(hive: &WorkstealingHive<MatMulWorker>, n: usize) -> Vec<i32> {
    let a = vec![1i32; n * n];
    let b = vec![1i32; n * n];
    let mut c = vec![0i32; n * n];

    let root_mat = Mat {
        a: a.as_ptr(),
        b: b.as_ptr(),
        c: c.as_mut_ptr(),
        n,
        nn: n,
    };
    let root_node = Arc::new(Node {
        remaining: AtomicUsize::new(1),
        group2: Mutex::new(None),
        parent: None,
    });

    let (tx, rx) = outcome_channel::<MatMulWorker>();
    // ~300k tiny outcomes per run; a single drainer keeps up easily.
    let handles = spawn_outcome_drainers(&rx, 1);
    hive.apply_send(
        Step {
            m: root_mat,
            node: root_node,
        },
        &tx,
    );
    drop(tx);
    drop(rx);
    // Disconnect implies every task ran and its C writes happen-before this.
    join_outcome_drainers(handles);
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

    let hive = build_hive::<MatMulWorker>(thread_count);
    println!("threads: {}", thread_count);

    let _ = run_matmul(&hive, n); // warmup

    println!("runs:");
    let start = Instant::now();
    let result = run_matmul(&hive, n);
    let dur = start.elapsed();
    validate(&result, n);

    println!("  - matrix_size: {}", n);
    println!("    duration: {} us", dur.as_micros());
    println!("    max_rss: {} KiB", peak_memory_usage());
}
