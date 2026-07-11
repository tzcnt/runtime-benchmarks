// The skynet benchmark as described here:
// https://github.com/atemerev/skynet
//
// Canonical (TooManyCooks) implementation:
// ../../../cpp/TooManyCooks/skynet.cpp
//
// Each node forks 10 children until depth_max levels deep (10^depth_max =
// 100M leaf tasks) and sums their results. Uses zigbeam's Loom work-stealing
// pool: the 10-way fan-out is expressed as a recursive binary split via
// loom.joinOnPool (Rayon-style fork-join with steal-while-wait), the idiomatic
// divide-and-conquer shape for this library. The whole computation runs as a
// pool task so the requested thread count does all the work while main parks.
const std = @import("std");
const loom = @import("loom");
const bu = @import("benchutil");

const depth_max = 8;
const iter_count = 1;

var pool: *loom.ThreadPool = undefined;
var done: bu.DoneEvent = .{};

fn skynetOne(base: u64, depth: u32) u64 {
    if (depth == depth_max) return base;
    var offset: u64 = 1;
    var i: u32 = 0;
    while (i < depth_max - depth - 1) : (i += 1) {
        offset *= 10;
    }
    return skyRange(base, depth, offset, 0, 10);
}

/// Sum children [lo, hi) of a node by recursive binary fork-join.
fn skyRange(base: u64, depth: u32, offset: u64, lo: u64, hi: u64) u64 {
    if (hi - lo == 1) return skynetOne(base + offset * lo, depth + 1);
    const mid = lo + (hi - lo) / 2;
    const l, const r = loom.joinOnPool(
        pool,
        skyRange,
        .{ base, depth, offset, lo, mid },
        skyRange,
        .{ base, depth, offset, mid, hi },
    );
    return l + r;
}

const RootCtx = struct {
    result: u64 = 0,

    fn run(ctx_opaque: *anyopaque) void {
        const ctx: *RootCtx = @ptrCast(@alignCast(ctx_opaque));
        ctx.result = skynetOne(0, 0);
        done.set();
    }
};

fn skynet(expected: u64) void {
    var ctx = RootCtx{};
    var task = loom.Task{ .execute = RootCtx.run, .context = @ptrCast(&ctx), .scope = null };
    done.reset();
    pool.spawn(&task) catch @panic("spawn failed");
    done.wait();
    if (ctx.result != expected) {
        bu.print("ERROR: wrong result - {d}\n", .{ctx.result});
    }
}

pub fn main(init: std.process.Init.Minimal) void {
    const argv = bu.Argv(1).init(init.args);
    const thread_count = argv.int(u32, 0) orelse bu.defaultThreads();
    bu.print("threads: {d}\n", .{thread_count});
    // Steal-while-wait joins nest stolen subtrees on the waiting worker's
    // stack, so recursive fork-join workloads need far more than the default
    // thread stack. Virtual memory: only touched pages are committed.
    pool = loom.ThreadPool.init(bu.allocator, .{
        .num_threads = thread_count,
        .stack_size = 1 << 30,
    }) catch @panic("pool init failed");

    var leaves: u64 = 1;
    var i: u32 = 0;
    while (i < depth_max) : (i += 1) {
        leaves *= 10;
    }
    const expected = (leaves - 1) * leaves / 2;

    skynet(expected); // warmup

    bu.print("runs:\n", .{});
    const start = bu.nowNs();
    var iter: usize = 0;
    while (iter < iter_count) : (iter += 1) {
        skynet(expected);
    }
    const dur_us = (bu.nowNs() - start) / std.time.ns_per_us;
    bu.print("  - iteration_count: {d}\n", .{iter_count});
    bu.print("    duration: {d} us\n", .{dur_us});
    bu.print("    max_rss: {d} KiB\n", .{bu.peakMemoryUsageKiB()});

    pool.deinit();
}
