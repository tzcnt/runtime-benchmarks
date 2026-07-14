// The skynet benchmark as described here:
// https://github.com/atemerev/skynet
//
// Canonical (TooManyCooks) implementation:
// ../../../cpp/TooManyCooks/skynet.cpp
//
// Each node forks 10 children until depth_max levels deep (10^depth_max =
// 100M leaf tasks) and sums their results. zap's ThreadPool is schedule-only,
// so each node is a heap-allocated continuation frame; the 10 children are
// submitted as a single Batch (zap's batch-scheduling feature) and completions
// cascade up through atomic pending counts.
const std = @import("std");
const ThreadPool = @import("thread_pool");
const bu = @import("benchutil");

const depth_max = 8;
const iter_count = 1;

var pool: ThreadPool = undefined;
var done: bu.DoneEvent = .{};

const SkyFrame = struct {
    task: ThreadPool.Task = .{ .callback = run },
    base: u64,
    depth: u32,
    parent: ?*SkyFrame,
    result: std.atomic.Value(u64) = std.atomic.Value(u64).init(0),
    pending: std.atomic.Value(u32) = std.atomic.Value(u32).init(0),

    fn run(task: *ThreadPool.Task) void {
        const frame: *SkyFrame = @fieldParentPtr("task", task);
        if (frame.depth == depth_max) {
            _ = frame.result.fetchAdd(frame.base, .monotonic);
            return finish(frame);
        }
        var offset: u64 = 1;
        var i: u32 = 0;
        while (i < depth_max - frame.depth - 1) : (i += 1) {
            offset *= 10;
        }

        var batch = ThreadPool.Batch{};
        var j: u64 = 0;
        while (j < 10) : (j += 1) {
            const child = bu.allocator.create(SkyFrame) catch @panic("oom");
            child.* = .{
                .base = frame.base + offset * j,
                .depth = frame.depth + 1,
                .parent = frame,
            };
            batch.push(ThreadPool.Batch.from(&child.task));
        }
        frame.pending.store(10, .release);
        pool.schedule(batch);
    }

    fn finish(frame_arg: *SkyFrame) void {
        var frame = frame_arg;
        while (true) {
            const parent = frame.parent orelse {
                done.set(); // root (lives on the main thread's stack)
                return;
            };
            const res = frame.result.load(.monotonic);
            bu.allocator.destroy(frame);
            _ = parent.result.fetchAdd(res, .monotonic);
            if (parent.pending.fetchSub(1, .acq_rel) != 1) return;
            frame = parent;
        }
    }
};

fn skynet(expected: u64) void {
    var root = SkyFrame{ .base = 0, .depth = 0, .parent = null };
    done.reset();
    pool.schedule(ThreadPool.Batch.from(&root.task));
    done.wait();
    const count = root.result.load(.monotonic);
    if (count != expected) {
        bu.print("ERROR: wrong result - {d}\n", .{count});
    }
}

pub fn main(init: std.process.Init.Minimal) void {
    const argv = bu.Argv(1).init(init.args);
    const thread_count = argv.int(u32, 0) orelse bu.defaultThreads();
    bu.print("threads: {d}\n", .{thread_count});
    pool = ThreadPool.init(.{ .max_threads = thread_count });

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

    pool.shutdown();
    pool.deinit();
}
