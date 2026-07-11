// Recursive divide-and-conquer matrix multiplication.
//
// Canonical (TooManyCooks) implementation:
// ../../../cpp/TooManyCooks/matmul.cpp
// base case: ../../../cpp/2common/matmul.hpp
//
// The matrices are split into quadrants recursively down to a 32x32 base case.
// The 8 sub-multiplications run as two sequential groups of 4 parallel tasks
// so that no output tile is written by two tasks at once. Uses zigbeam's Loom
// work-stealing pool: each group of 4 is a small tree of loom.joinOnPool
// calls (Rayon-style fork-join with steal-while-wait).
const std = @import("std");
const loom = @import("loom");
const bu = @import("benchutil");

var pool: *loom.ThreadPool = undefined;
var done: bu.DoneEvent = .{};

// The A/B/C backing arrays of the current multiplication; tasks carry base
// offsets into them. nn is the stride (the full matrix dimension N).
var ga: []i32 = undefined;
var gb: []i32 = undefined;
var gc: []i32 = undefined;
var gnn: usize = undefined;

// A tile is the (ao, bo, co) offset triple of a sub-multiplication.
const Tile = [3]usize;

fn matmulSmall(t: Tile, n: usize) void {
    const nn = gnn;
    var i: usize = 0;
    while (i < n) : (i += 1) {
        var k: usize = 0;
        while (k < n) : (k += 1) {
            const a = ga[t[0] + i * nn + k];
            var j: usize = 0;
            while (j < n) : (j += 1) {
                gc[t[2] + i * nn + j] += a * gb[t[1] + k * nn + j];
            }
        }
    }
}

fn matmul(t: Tile, n: usize) void {
    if (n <= 32) {
        return matmulSmall(t, n);
    }
    const k = n / 2;
    const nn = gnn;
    const ao = t[0];
    const bo = t[1];
    const co = t[2];

    // Two sequential groups of 4: group 2 accumulates into the same C tiles
    // as group 1, so it may only start after group 1 completes.
    runGroup(.{
        .{ ao, bo, co },
        .{ ao, bo + k, co + k },
        .{ ao + k * nn, bo, co + k * nn },
        .{ ao + k * nn, bo + k, co + k * nn + k },
    }, k);
    runGroup(.{
        .{ ao + k, bo + k * nn, co },
        .{ ao + k, bo + k * nn + k, co + k },
        .{ ao + k * nn + k, bo + k * nn, co + k * nn },
        .{ ao + k * nn + k, bo + k * nn + k, co + k * nn + k },
    }, k);
}

fn runGroup(group: [4]Tile, n: usize) void {
    _ = loom.joinOnPool(
        pool,
        matmulPair,
        .{ group[0], group[1], n },
        matmulPair,
        .{ group[2], group[3], n },
    );
}

fn matmulPair(t1: Tile, t2: Tile, n: usize) void {
    _ = loom.joinOnPool(pool, matmul, .{ t1, n }, matmul, .{ t2, n });
}

const RootCtx = struct {
    n: usize,

    fn run(ctx_opaque: *anyopaque) void {
        const ctx: *RootCtx = @ptrCast(@alignCast(ctx_opaque));
        matmul(.{ 0, 0, 0 }, ctx.n);
        done.set();
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

    var ctx = RootCtx{ .n = n };
    var task = loom.Task{ .execute = RootCtx.run, .context = @ptrCast(&ctx), .scope = null };
    done.reset();
    pool.spawn(&task) catch @panic("spawn failed");
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
    // Steal-while-wait joins nest stolen subtrees on the waiting worker's
    // stack, so recursive fork-join workloads need far more than the default
    // thread stack. Virtual memory: only touched pages are committed.
    pool = loom.ThreadPool.init(bu.allocator, .{
        .num_threads = thread_count,
        .stack_size = 1 << 30,
    }) catch @panic("pool init failed");

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
