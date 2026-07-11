// Recursive fork-join nqueens.
//
// Canonical (TooManyCooks) implementation:
// ../../../cpp/TooManyCooks/nqueens.cpp
// which was in turn adapted from libfork's benchmark.
//
// At each column, fork a child task for every valid queen placement. zap's
// ThreadPool is schedule-only, so each node is a heap-allocated continuation
// frame whose children are submitted as one Batch; solution counts cascade up
// through atomic pending counts.
const std = @import("std");
const ThreadPool = @import("thread_pool");
const bu = @import("benchutil");

const nqueens_work = 14; // board size
const iter_count = 1;

// answers[k] = number of solutions to the k-queens problem.
const answers = [19]u64{
    0,         1,   0,    0,     2,     10,     4,       40,       92,
    352,       724, 2680, 14200, 73712, 365596, 2279184, 14772512, 95815104,
    666090624,
};

var pool: ThreadPool = undefined;
var done: bu.DoneEvent = .{};

const NqFrame = struct {
    task: ThreadPool.Task = .{ .callback = run },
    buf: [nqueens_work]i8,
    x: usize,
    parent: ?*NqFrame,
    result: std.atomic.Value(u64) = std.atomic.Value(u64).init(0),
    pending: std.atomic.Value(u32) = std.atomic.Value(u32).init(0),

    fn run(task: *ThreadPool.Task) void {
        const frame: *NqFrame = @fieldParentPtr("task", task);
        const x_max = frame.x;
        if (x_max == nqueens_work) {
            _ = frame.result.fetchAdd(1, .monotonic);
            return finish(frame);
        }

        var batch = ThreadPool.Batch{};
        var forks: u32 = 0;
        var y: i8 = 0;
        while (y < nqueens_work) : (y += 1) {
            var valid = true;
            for (frame.buf[0..x_max], 0..) |p, i| {
                const d: i8 = @intCast(x_max - i);
                if (y == p or y == p - d or y == p + d) {
                    valid = false;
                    break;
                }
            }
            if (!valid) continue;
            const child = bu.allocator.create(NqFrame) catch @panic("oom");
            child.* = .{ .buf = frame.buf, .x = x_max + 1, .parent = frame };
            child.buf[x_max] = y;
            batch.push(ThreadPool.Batch.from(&child.task));
            forks += 1;
        }
        if (forks == 0) return finish(frame); // dead end: contributes 0
        frame.pending.store(forks, .release);
        pool.schedule(batch);
    }

    fn finish(frame_arg: *NqFrame) void {
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

fn nqueens() u64 {
    var root = NqFrame{ .buf = @splat(0), .x = 0, .parent = null };
    done.reset();
    pool.schedule(ThreadPool.Batch.from(&root.task));
    done.wait();
    return root.result.load(.monotonic);
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
    pool = ThreadPool.init(.{ .max_threads = thread_count });

    checkAnswer(nqueens()); // warmup

    const start = bu.nowNs();
    var result: u64 = 0;
    var i: usize = 0;
    while (i < iter_count) : (i += 1) {
        result = nqueens();
        checkAnswer(result);
    }
    const dur_us = (bu.nowNs() - start) / std.time.ns_per_us;
    bu.print("output: {d}\n", .{result});

    bu.print("runs:\n", .{});
    bu.print("  - iteration_count: {d}\n", .{iter_count});
    bu.print("    duration: {d} us\n", .{dur_us});
    bu.print("    max_rss: {d} KiB\n", .{bu.peakMemoryUsageKiB()});

    pool.shutdown();
    pool.deinit();
}
