// Recursive divide-and-conquer matrix multiplication.
//
// Canonical (TooManyCooks) implementation:
// ../../../cpp/TooManyCooks/matmul.cpp
// base case: ../../../cpp/2common/matmul.hpp
//
// The matrices are split into quadrants recursively down to a 32x32 base case.
// The 8 sub-multiplications run as two sequential groups of 4 parallel tasks
// so that no output tile is written by two tasks at once. zap's ThreadPool is
// schedule-only, so each node is a heap-allocated continuation frame with a
// phase field: when the first group of 4 children completes, the frame is
// re-scheduled to fork the second group.
const std = @import("std");
const ThreadPool = @import("thread_pool");
const bu = @import("benchutil");

var pool: ThreadPool = undefined;
var done: bu.DoneEvent = .{};

// The A/B/C backing arrays of the current multiplication; frames carry base
// offsets into them. nn is the stride (the full matrix dimension N).
var ga: []i32 = undefined;
var gb: []i32 = undefined;
var gc: []i32 = undefined;
var gnn: usize = undefined;

fn matmulSmall(ao: usize, bo: usize, co: usize, n: usize) void {
    const nn = gnn;
    var i: usize = 0;
    while (i < n) : (i += 1) {
        var k: usize = 0;
        while (k < n) : (k += 1) {
            const a = ga[ao + i * nn + k];
            var j: usize = 0;
            while (j < n) : (j += 1) {
                gc[co + i * nn + j] += a * gb[bo + k * nn + j];
            }
        }
    }
}

const MatFrame = struct {
    task: ThreadPool.Task = .{ .callback = run },
    ao: usize,
    bo: usize,
    co: usize,
    n: usize,
    phase: u8 = 0,
    parent: ?*MatFrame,
    pending: std.atomic.Value(u32) = std.atomic.Value(u32).init(0),

    fn run(task: *ThreadPool.Task) void {
        const frame: *MatFrame = @fieldParentPtr("task", task);
        const n = frame.n;
        if (n <= 32) {
            matmulSmall(frame.ao, frame.bo, frame.co, n);
            return finish(frame);
        }
        const k = n / 2;
        const nn = gnn;
        const ao = frame.ao;
        const bo = frame.bo;
        const co = frame.co;

        // Two sequential groups of 4: group 2 accumulates into the same C
        // tiles as group 1, so it may only start after group 1 completes.
        const group: [4][3]usize = if (frame.phase == 0) .{
            .{ ao, bo, co },
            .{ ao, bo + k, co + k },
            .{ ao + k * nn, bo, co + k * nn },
            .{ ao + k * nn, bo + k, co + k * nn + k },
        } else .{
            .{ ao + k, bo + k * nn, co },
            .{ ao + k, bo + k * nn + k, co + k },
            .{ ao + k * nn + k, bo + k * nn, co + k * nn },
            .{ ao + k * nn + k, bo + k * nn + k, co + k * nn + k },
        };

        var batch = ThreadPool.Batch{};
        for (group) |offsets| {
            const child = bu.allocator.create(MatFrame) catch @panic("oom");
            child.* = .{
                .ao = offsets[0],
                .bo = offsets[1],
                .co = offsets[2],
                .n = k,
                .parent = frame,
            };
            batch.push(ThreadPool.Batch.from(&child.task));
        }
        frame.pending.store(4, .release);
        pool.schedule(batch);
    }

    fn finish(frame_arg: *MatFrame) void {
        var frame = frame_arg;
        while (true) {
            const parent = frame.parent orelse {
                done.set(); // root (lives on the main thread's stack)
                return;
            };
            bu.allocator.destroy(frame);
            if (parent.pending.fetchSub(1, .acq_rel) != 1) return;
            if (parent.phase == 0) {
                // Group 1 done: re-schedule the parent to fork group 2.
                // Tasks are intrusive linked-list nodes; a task that was
                // already scheduled once has a stale .next from its previous
                // queue linkage and must be re-initialized before re-use.
                parent.phase = 1;
                parent.task = .{ .callback = run };
                pool.schedule(ThreadPool.Batch.from(&parent.task));
                return;
            }
            frame = parent;
        }
    }
};

fn runMatmul(n: usize) []i32 {
    const a = bu.allocator.alloc(i32, n * n) catch @panic("oom");
    const b = bu.allocator.alloc(i32, n * n) catch @panic("oom");
    const c = bu.allocator.alloc(i32, n * n) catch @panic("oom");
    @memset(a, 1);
    @memset(b, 1);
    @memset(c, 0);
    ga = a;
    gb = b;
    gc = c;
    gnn = n;

    var root = MatFrame{ .ao = 0, .bo = 0, .co = 0, .n = n, .parent = null };
    done.reset();
    pool.schedule(ThreadPool.Batch.from(&root.task));
    done.wait();

    bu.allocator.free(a);
    bu.allocator.free(b);
    return c;
}

fn validate(c: []const i32, n: usize) void {
    var i: usize = 0;
    while (i < n) : (i += 1) {
        var j: usize = 0;
        while (j < n) : (j += 1) {
            const res = c[i * n + j];
            if (res != @as(i32, @intCast(n))) {
                bu.print("Wrong result at ({d},{d}) : {d}. expected {d}\n", .{ i, j, res, n });
                std.process.exit(1);
            }
        }
    }
}

pub fn main(init: std.process.Init.Minimal) void {
    const argv = bu.Argv(2).init(init.args);
    const n = argv.int(usize, 0) orelse {
        bu.print("Usage: matmul <matrix size (power of 2)>\n", .{});
        return;
    };
    const thread_count = argv.int(u32, 1) orelse bu.defaultThreads();
    bu.print("threads: {d}\n", .{thread_count});
    pool = ThreadPool.init(.{ .max_threads = thread_count });

    bu.allocator.free(runMatmul(n)); // warmup

    bu.print("runs:\n", .{});
    const start = bu.nowNs();
    const result = runMatmul(n);
    const dur_us = (bu.nowNs() - start) / std.time.ns_per_us;
    validate(result, n);
    bu.allocator.free(result);

    bu.print("  - matrix_size: {d}\n", .{n});
    bu.print("    duration: {d} us\n", .{dur_us});
    bu.print("    max_rss: {d} KiB\n", .{bu.peakMemoryUsageKiB()});

    pool.shutdown();
    pool.deinit();
}
