// The skynet benchmark as described here:
// https://github.com/atemerev/skynet
//
// Canonical (TooManyCooks) implementation:
// ../../../cpp/TooManyCooks/skynet.cpp
//
// Each node forks 10 children until depth_max levels deep (10^depth_max = 100M
// leaf tasks) and sums their results. Spice only offers a binary `join`, so the
// 10-way fan-out is expressed as a balanced binary reduction over the child
// index range [0,10) - the same shape rayon's `into_par_iter().sum()` produces
// internally. Because Spice is stack-based (heartbeat-scheduled), no per-task
// heap allocation happens, unlike the frame-allocating zap port.
const std = @import("std");
const spice = @import("spice");
const bu = @import("benchutil");

const depth_max = 8;
const iter_count = 1;

var pool: spice.ThreadPool = undefined;

const One = struct { base: u64, depth: u32 };
// Sum children [lo,hi) of a node whose child 0 has value `base` and whose
// children are spaced `offset` apart, all at `depth`.
const Range = struct { base: u64, offset: u64, depth: u32, lo: u32, hi: u32 };

fn skynetOne(t: *spice.Task, a: One) u64 {
    if (a.depth == depth_max) return a.base;
    var offset: u64 = 1;
    var k: u32 = 0;
    while (k < depth_max - a.depth - 1) : (k += 1) offset *= 10;
    return sumRange(t, .{ .base = a.base, .offset = offset, .depth = a.depth, .lo = 0, .hi = 10 });
}

fn sumRange(t: *spice.Task, r: Range) u64 {
    if (r.hi - r.lo == 1) {
        return skynetOne(t, .{ .base = r.base + r.offset * r.lo, .depth = r.depth + 1 });
    }
    const mid = r.lo + (r.hi - r.lo) / 2;
    const left = Range{ .base = r.base, .offset = r.offset, .depth = r.depth, .lo = r.lo, .hi = mid };
    const right = Range{ .base = r.base, .offset = r.offset, .depth = r.depth, .lo = mid, .hi = r.hi };

    var fut = spice.Future(Range, u64).init();
    fut.fork(t, sumRange, right);
    const l = t.call(u64, sumRange, left);
    if (fut.join(t)) |rr| {
        return l + rr;
    } else {
        return l + t.call(u64, sumRange, right);
    }
}

fn skynet(expected: u64) void {
    const count = pool.call(u64, skynetOne, One{ .base = 0, .depth = 0 });
    if (count != expected) {
        bu.print("ERROR: wrong result - {d}\n", .{count});
    }
}

pub fn main(init: std.process.Init) void {
    const argv = bu.Argv(1).init(init.minimal.args);
    const thread_count = argv.int(usize, 0) orelse bu.defaultThreads();
    bu.print("threads: {d}\n", .{thread_count});

    pool = spice.ThreadPool.init(bu.allocator, init.io);
    pool.start(.{ .background_worker_count = thread_count - 1 });

    var leaves: u64 = 1;
    var i: u32 = 0;
    while (i < depth_max) : (i += 1) leaves *= 10;
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
