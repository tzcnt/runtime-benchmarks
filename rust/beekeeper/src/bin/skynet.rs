// The skynet benchmark as described here:
// https://github.com/atemerev/skynet
//
// Canonical (TooManyCooks) implementation:
// ../../../../cpp/TooManyCooks/skynet.cpp
//
// Each node forks 10 children until DEPTH_MAX levels deep (10^DEPTH_MAX = 100M
// leaf tasks). beekeeper has no in-task join, so rather than each node summing
// its children's results, leaf tasks return their ordinal and interior tasks
// return 0; the drainer threads sum the hive's outcome stream, which yields
// the same total. The 10-way fork is a range iterator over `Context::submit`.

use beekeeper::bee::{Context, Worker, WorkerResult};
use beekeeper::hive::outcome_channel;
use beekeeper_bench::{
    build_hive, default_threads, drainer_count, join_outcome_drainers, peak_memory_usage,
    spawn_outcome_drainers, WorkstealingHive,
};
use std::time::Instant;

const DEPTH_MAX: u64 = 8;
const ITER_COUNT: usize = 1;

#[derive(Debug, Default)]
struct SkynetWorker;

impl Worker for SkynetWorker {
    // (base, depth) of one skynet node.
    type Input = (u64, u64);
    type Output = u64;
    type Error = ();

    fn apply(
        &mut self,
        (base, depth): (u64, u64),
        ctx: &Context<(u64, u64)>,
    ) -> WorkerResult<Self> {
        if depth == DEPTH_MAX {
            return Ok(base);
        }
        let mut offset = 1u64;
        for _ in 0..(DEPTH_MAX - depth - 1) {
            offset *= 10;
        }

        (0..10u64).for_each(|i| {
            ctx.submit((base + offset * i, depth + 1))
                .expect("submit skynet child")
        });
        Ok(0)
    }
}

fn skynet(hive: &WorkstealingHive<SkynetWorker>, expected: u64, drainers: usize) {
    let (tx, rx) = outcome_channel::<SkynetWorker>();
    let handles = spawn_outcome_drainers(&rx, drainers);
    hive.apply_send((0, 0), &tx);
    drop(tx);
    drop(rx);
    let (_tasks, count) = join_outcome_drainers(handles);
    if count != expected {
        println!("ERROR: wrong result - {}", count);
    }
}

fn main() {
    let args: Vec<String> = std::env::args().collect();
    let thread_count = if args.len() > 1 {
        args[1].parse().expect("thread count")
    } else {
        default_threads()
    };

    let leaves = 10u64.pow(DEPTH_MAX as u32);
    let expected = (leaves - 1) * leaves / 2;

    let hive = build_hive::<SkynetWorker>(thread_count);
    let drainers = drainer_count(thread_count);
    println!("threads: {}", thread_count);

    skynet(&hive, expected, drainers); // warmup

    let start = Instant::now();
    for _ in 0..ITER_COUNT {
        skynet(&hive, expected, drainers);
    }
    let dur = start.elapsed();

    println!("runs:");
    println!("  - iteration_count: {}", ITER_COUNT);
    println!("    duration: {} us", dur.as_micros());
    println!("    max_rss: {} KiB", peak_memory_usage());
}
