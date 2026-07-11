// An implementation of the recursive fork fibonacci parallelism test.
//
// Canonical (TooManyCooks) implementation:
// ../../../cpp/TooManyCooks/fib.cpp
//
// Uses zigbeam's Loom work-stealing pool. Each node forks fib(n-1) to the
// pool via loom.joinOnPool (Rayon-style binary fork-join: the second leg is
// spawned as a stealable task, the first runs inline, and the joiner helps by
// stealing while it waits), matching the "fork one leg, run the other inline"
// shape of the TMC and Go versions. The whole computation runs as a pool task
// so the requested thread count does all the work while main is parked.
const std = @import("std");
const loom = @import("loom");
const bu = @import("benchutil");

const iter_count = 1;

var pool: *loom.ThreadPool = undefined;
var done: bu.DoneEvent = .{};

fn fib(n: u64) u64 {
    if (n < 2) return n;
    // Left leg (fib(n-2)) runs inline; right leg (fib(n-1)) is forked.
    const y, const x = loom.joinOnPool(pool, fib, .{n - 2}, fib, .{n - 1});
    return x + y;
}

const RootCtx = struct {
    n: u64,
    result: u64 = 0,

    fn run(ctx_opaque: *anyopaque) void {
        const ctx: *RootCtx = @ptrCast(@alignCast(ctx_opaque));
        ctx.result = fib(ctx.n);
        done.set();
    }
};

/// Run fib(n) entirely on the pool; main parks until it completes.
fn fibOnPool(n: u64) u64 {
    var ctx = RootCtx{ .n = n };
    var task = loom.Task{ .execute = RootCtx.run, .context = @ptrCast(&ctx), .scope = null };
    done.reset();
    pool.spawn(&task) catch @panic("spawn failed");
    done.wait();
    return ctx.result;
}

pub fn main(init: std.process.Init.Minimal) void {
    const argv = bu.Argv(2).init(init.args);
    const n = argv.int(u64, 0) orelse {
        bu.print("Usage: fib <n-th fibonacci number requested>\n", .{});
        return;
    };
    const thread_count = argv.int(u32, 1) orelse bu.defaultThreads();
    bu.print("threads: {d}\n", .{thread_count});
    // Steal-while-wait joins nest stolen subtrees on the waiting worker's
    // stack, so recursive fork-join workloads need far more than the default
    // thread stack. Virtual memory: only touched pages are committed.
    pool = loom.ThreadPool.init(bu.allocator, .{
        .num_threads = thread_count,
        .stack_size = 1 << 30,
    }) catch @panic("pool init failed");

    _ = fibOnPool(30); // warmup

    const start = bu.nowNs();
    var result: u64 = 0;
    var i: usize = 0;
    while (i < iter_count) : (i += 1) {
        result = fibOnPool(n);
    }
    const dur_us = (bu.nowNs() - start) / std.time.ns_per_us;
    bu.print("output: {d}\n", .{result});

    bu.print("runs:\n", .{});
    bu.print("  - iteration_count: {d}\n", .{iter_count});
    bu.print("    duration: {d} us\n", .{dur_us});
    bu.print("    max_rss: {d} KiB\n", .{bu.peakMemoryUsageKiB()});

    pool.deinit();
}
