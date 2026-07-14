// Recursive divide-and-conquer matrix multiplication.
//
// Canonical (TooManyCooks) implementation:
// ../../../cpp/TooManyCooks/matmul.cpp
// base case: ../../../cpp/2common/matmul.hpp
//
// The matrices are split into quadrants recursively down to a 32x32 base case.
// The 8 sub-multiplications run as two sequential groups of 4 parallel tasks so
// that no output tile is written by two tasks at once. Each group of 4 is
// expressed with Spice's native multi-fork idiom: fork three children, run the
// fourth inline, then join the three in LIFO order (Spice's job queue is a
// stack, so joins must unwind in reverse of the forks). Group 2 accumulates into
// the same C tiles as group 1, so it only starts after group 1 has fully joined.
//
// NOTE: this benchmark requires the fix-stale-job-tail.patch that build_all.sh
// applies to the vendored Spice checkout (prepared as an upstream PR; the
// patch and its commit message carry the full analysis). Unpatched Spice
// lets a frame's cached `Task.job_tail` dangle at a job that a heartbeat has
// already shifted out of the queue - `Task.call` never writes the callee's
// final job_tail back to the caller, and a join that finds its job in the
// executing state never restores the tail at all. Any fork AFTER such a join
// then pushes onto the removed job and corrupts the queue (crashing later with
// a null execute-state). This benchmark is the only one in the suite that
// forks again after joining in the same frame (two runGroup calls per level,
// plus the inline fallback inside each group), which is why matmul crashed at
// n=2048 while fib/skynet/nqueens (fork-then-return) never hit it. The Rust
// port (chili) is unaffected: its queue is owned by the worker, not threaded
// through frames as a cached tail pointer.
const std = @import("std");
const spice = @import("spice");
const bu = @import("benchutil");

var pool: spice.ThreadPool = undefined;

// A single sub-multiplication: C[..] += A[..] * B[..] over an n×n tile of
// matrices with stride nn. a/b are read-only; c is written (disjoint per task).
const Mat = struct {
    a: [*]const i32,
    b: [*]const i32,
    c: [*]i32,
    n: usize,
    nn: usize,
};

fn matmulSmall(m: Mat) void {
    const nn = m.nn;
    var i: usize = 0;
    while (i < m.n) : (i += 1) {
        var k: usize = 0;
        while (k < m.n) : (k += 1) {
            const av = m.a[i * nn + k];
            var j: usize = 0;
            while (j < m.n) : (j += 1) {
                m.c[i * nn + j] += av * m.b[k * nn + j];
            }
        }
    }
}

// Run four independent sub-multiplications in parallel: fork g[1..3], run g[0]
// inline, then join in LIFO order (reverse of the fork order). The task
// functions return a dummy value because Spice's `Future` requires a result
// type; the real output is the writes into C.
fn runGroup(t: *spice.Task, g: [4]Mat) void {
    var f1 = spice.Future(Mat, u64).init();
    var f2 = spice.Future(Mat, u64).init();
    var f3 = spice.Future(Mat, u64).init();
    f1.fork(t, matmul, g[1]);
    f2.fork(t, matmul, g[2]);
    f3.fork(t, matmul, g[3]);
    _ = t.call(u64, matmul, g[0]);
    if (f3.join(t) == null) _ = t.call(u64, matmul, g[3]);
    if (f2.join(t) == null) _ = t.call(u64, matmul, g[2]);
    if (f1.join(t) == null) _ = t.call(u64, matmul, g[1]);
}

fn matmul(t: *spice.Task, m: Mat) u64 {
    if (m.n <= 32) {
        matmulSmall(m);
        return 0;
    }
    const k = m.n / 2;
    const nn = m.nn;
    const a = m.a;
    const b = m.b;
    const c = m.c;

    // Group 1: the four products that first write the C tiles.
    runGroup(t, .{
        .{ .a = a, .b = b, .c = c, .n = k, .nn = nn },
        .{ .a = a, .b = b + k, .c = c + k, .n = k, .nn = nn },
        .{ .a = a + k * nn, .b = b, .c = c + k * nn, .n = k, .nn = nn },
        .{ .a = a + k * nn, .b = b + k, .c = c + k * nn + k, .n = k, .nn = nn },
    });

    // Group 2: the four products that accumulate into the same tiles. Group 1
    // has fully joined above, so the tiles are written then accumulated in order.
    runGroup(t, .{
        .{ .a = a + k, .b = b + k * nn, .c = c, .n = k, .nn = nn },
        .{ .a = a + k, .b = b + k * nn + k, .c = c + k, .n = k, .nn = nn },
        .{ .a = a + k * nn + k, .b = b + k * nn, .c = c + k * nn, .n = k, .nn = nn },
        .{ .a = a + k * nn + k, .b = b + k * nn + k, .c = c + k * nn + k, .n = k, .nn = nn },
    });

    return 0;
}

fn runMatmul(n: usize) []i32 {
    const a = bu.allocator.alloc(i32, n * n) catch @panic("oom");
    const b = bu.allocator.alloc(i32, n * n) catch @panic("oom");
    const c = bu.allocator.alloc(i32, n * n) catch @panic("oom");
    @memset(a, 1);
    @memset(b, 1);
    @memset(c, 0);

    _ = pool.call(u64, matmul, Mat{ .a = a.ptr, .b = b.ptr, .c = c.ptr, .n = n, .nn = n });

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

pub fn main(init: std.process.Init) void {
    const argv = bu.Argv(2).init(init.minimal.args);
    const n = argv.int(usize, 0) orelse {
        bu.print("Usage: matmul <matrix size (power of 2)>\n", .{});
        return;
    };
    const thread_count = argv.int(usize, 1) orelse bu.defaultThreads();
    bu.print("threads: {d}\n", .{thread_count});

    pool = spice.ThreadPool.init(bu.allocator, init.io);
    pool.start(.{ .background_worker_count = thread_count - 1 });

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

    pool.deinit();
}
