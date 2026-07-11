// Recursive fork-join nqueens.
//
// Canonical (TooManyCooks) implementation:
// ../../../cpp/TooManyCooks/nqueens.cpp
// which was in turn adapted from libfork's benchmark.
//
// At each column, fork a child task for every valid queen placement. Uses
// zigbeam's Loom work-stealing pool: the per-column fan-out is expressed as a
// recursive binary split over the valid placements via loom.joinOnPool
// (Rayon-style fork-join with steal-while-wait). The whole computation runs
// as a pool task so the requested thread count does all the work.
const std = @import("std");
const loom = @import("loom");
const bu = @import("benchutil");

const nqueens_work = 14; // board size
const iter_count = 1;

// answers[k] = number of solutions to the k-queens problem.
const answers = [19]u64{
    0,         1,   0,    0,     2,     10,     4,       40,       92,
    352,       724, 2680, 14200, 73712, 365596, 2279184, 14772512, 95815104,
    666090624,
};

var pool: *loom.ThreadPool = undefined;
var done: bu.DoneEvent = .{};

const Board = [nqueens_work]i8;

fn nqueens(x_max: usize, buf: Board) u64 {
    if (x_max == nqueens_work) return 1;

    // Collect the valid placements for this column, then fork over them.
    var ys: Board = undefined;
    var count: usize = 0;
    var y: i8 = 0;
    while (y < nqueens_work) : (y += 1) {
        var valid = true;
        for (buf[0..x_max], 0..) |p, i| {
            const d: i8 = @intCast(x_max - i);
            if (y == p or y == p - d or y == p + d) {
                valid = false;
                break;
            }
        }
        if (valid) {
            ys[count] = y;
            count += 1;
        }
    }
    if (count == 0) return 0;
    return nqRange(x_max, buf, ys, 0, count);
}

/// Sum the subtrees for placements ys[lo..hi) by recursive binary fork-join.
fn nqRange(x_max: usize, buf: Board, ys: Board, lo: usize, hi: usize) u64 {
    if (hi - lo == 1) {
        var next = buf;
        next[x_max] = ys[lo];
        return nqueens(x_max + 1, next);
    }
    const mid = lo + (hi - lo) / 2;
    const l, const r = loom.joinOnPool(
        pool,
        nqRange,
        .{ x_max, buf, ys, lo, mid },
        nqRange,
        .{ x_max, buf, ys, mid, hi },
    );
    return l + r;
}

const RootCtx = struct {
    result: u64 = 0,

    fn run(ctx_opaque: *anyopaque) void {
        const ctx: *RootCtx = @ptrCast(@alignCast(ctx_opaque));
        ctx.result = nqueens(0, @splat(0));
        done.set();
    }
};

fn nqueensOnPool() u64 {
    var ctx = RootCtx{};
    var task = loom.Task{ .execute = RootCtx.run, .context = @ptrCast(&ctx), .scope = null };
    done.reset();
    pool.spawn(&task) catch @panic("spawn failed");
    done.wait();
    return ctx.result;
}

fn checkAnswer(result: u64) void {
    if (result != answers[nqueens_work]) {
        bu.print("error: expected {d}, got {d}\n", .{ answers[nqueens_work], result });
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

    checkAnswer(nqueensOnPool()); // warmup

    const start = bu.nowNs();
    var result: u64 = 0;
    var i: usize = 0;
    while (i < iter_count) : (i += 1) {
        result = nqueensOnPool();
        checkAnswer(result);
    }
    const dur_us = (bu.nowNs() - start) / std.time.ns_per_us;
    bu.print("output: {d}\n", .{result});

    bu.print("runs:\n", .{});
    bu.print("  - iteration_count: {d}\n", .{iter_count});
    bu.print("    duration: {d} us\n", .{dur_us});
    bu.print("    max_rss: {d} KiB\n", .{bu.peakMemoryUsageKiB()});

    pool.deinit();
}
