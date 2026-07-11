// An implementation of the recursive fork fibonacci parallelism test.
//
// Canonical (TooManyCooks) implementation:
// ../../../cpp/TooManyCooks/fib.cpp
//
// zap's ThreadPool is schedule-only (intrusive tasks, no join primitive), so
// this uses the continuation-passing style the library was designed around:
// each node is a heap-allocated frame that forks fib(n-1) as a child task and
// continues the fib(n-2) leg serially (iteratively, within the same callback),
// mirroring the "fork one leg, run the other inline" shape of the TMC and Go
// versions. Children deliver results into their parent frame; an atomic
// pending count schedules the cascade of completions up the tree.
const std = @import("std");
const ThreadPool = @import("thread_pool");
const bu = @import("benchutil");

const iter_count = 1;

var pool: ThreadPool = undefined;
var done: bu.DoneEvent = .{};

const FibFrame = struct {
    task: ThreadPool.Task = .{ .callback = run },
    n: u64,
    parent: ?*FibFrame,
    result: std.atomic.Value(u64) = std.atomic.Value(u64).init(0),
    pending: std.atomic.Value(u32) = std.atomic.Value(u32).init(0),

    fn run(task: *ThreadPool.Task) void {
        const frame: *FibFrame = @fieldParentPtr("task", task);
        var n = frame.n;
        var forks: u32 = 0;
        var batch = ThreadPool.Batch{};
        // Fork fib(n-1) as a task and iterate into the fib(n-2) leg.
        while (n >= 2) : (n -= 2) {
            const child = bu.allocator.create(FibFrame) catch @panic("oom");
            child.* = .{ .n = n - 1, .parent = frame };
            batch.push(ThreadPool.Batch.from(&child.task));
            forks += 1;
        }
        // The serial leg bottoms out at fib(0) = 0 or fib(1) = 1.
        _ = frame.result.fetchAdd(n, .monotonic);
        if (forks == 0) return finish(frame);
        // Release: the last child to decrement must see this frame's own
        // result contribution above.
        frame.pending.store(forks, .release);
        pool.schedule(batch);
    }

    /// Called when `frame`'s subtree is fully computed: deliver its result to
    /// the parent and cascade any completions up the tree.
    fn finish(frame_arg: *FibFrame) void {
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

fn fib(n: u64) u64 {
    var root = FibFrame{ .n = n, .parent = null };
    done.reset();
    pool.schedule(ThreadPool.Batch.from(&root.task));
    done.wait();
    return root.result.load(.monotonic);
}

pub fn main(init: std.process.Init.Minimal) void {
    const argv = bu.Argv(2).init(init.args);
    const n = argv.int(u64, 0) orelse {
        bu.print("Usage: fib <n-th fibonacci number requested>\n", .{});
        return;
    };
    const thread_count = argv.int(u32, 1) orelse bu.defaultThreads();
    bu.print("threads: {d}\n", .{thread_count});
    pool = ThreadPool.init(.{ .max_threads = thread_count });

    _ = fib(30); // warmup

    const start = bu.nowNs();
    var result: u64 = 0;
    var i: usize = 0;
    while (i < iter_count) : (i += 1) {
        result = fib(n);
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
