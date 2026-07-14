// An implementation of the recursive fork fibonacci parallelism test.
//
// Canonical (TooManyCooks) implementation:
// ../../../cpp/TooManyCooks/fib.cpp
//
// Spice is a heartbeat-scheduled fork-join library: `fork` is nearly free (it
// just pushes the job onto a per-worker stack), and only the periodic heartbeat
// actually shares the oldest queued job with another worker. `join` returns
// null when the job was never picked up, in which case we run it inline via
// `t.call`. This mirrors the TMC "fork one leg, run the other inline" shape.
const std = @import("std");
const spice = @import("spice");
const bu = @import("benchutil");

const iter_count = 1;

var pool: spice.ThreadPool = undefined;

fn fib(t: *spice.Task, n: u64) u64 {
    if (n < 2) return n;

    var fut = spice.Future(u64, u64).init();
    fut.fork(t, fib, n - 1);

    const y = t.call(u64, fib, n - 2);

    if (fut.join(t)) |x| {
        return x + y;
    } else {
        // The forked job was never stolen; compute it ourselves.
        return y + t.call(u64, fib, n - 1);
    }
}

pub fn main(init: std.process.Init) void {
    const argv = bu.Argv(2).init(init.minimal.args);
    const n = argv.int(u64, 0) orelse {
        bu.print("Usage: fib <n-th fibonacci number requested>\n", .{});
        return;
    };
    const thread_count = argv.int(usize, 1) orelse bu.defaultThreads();
    bu.print("threads: {d}\n", .{thread_count});

    // Spice counts the calling thread as one worker, so request thread_count-1
    // background workers to end up with thread_count total.
    pool = spice.ThreadPool.init(bu.allocator, init.io);
    pool.start(.{ .background_worker_count = thread_count - 1 });

    _ = pool.call(u64, fib, 30); // warmup

    const start = bu.nowNs();
    var result: u64 = 0;
    var i: usize = 0;
    while (i < iter_count) : (i += 1) {
        result = pool.call(u64, fib, n);
    }
    const dur_us = (bu.nowNs() - start) / std.time.ns_per_us;
    bu.print("output: {d}\n", .{result});

    bu.print("runs:\n", .{});
    bu.print("  - iteration_count: {d}\n", .{iter_count});
    bu.print("    duration: {d} us\n", .{dur_us});
    bu.print("    max_rss: {d} KiB\n", .{bu.peakMemoryUsageKiB()});

    pool.deinit();
}
