// Recursive fork-join nqueens.
//
// Canonical (TooManyCooks) implementation:
// ../../../cpp/TooManyCooks/nqueens.cpp
// which was in turn adapted from libfork's benchmark.
//
// At each column we gather every valid queen placement into a small on-stack
// array of boards, then reduce over them. Spice only offers a binary `join`, so
// the N-way fan-out is a balanced binary reduction over that array (the same
// shape rayon's `into_par_iter().sum()` produces). The board array lives on the
// current frame, which stays alive until the reduction's forked jobs are all
// joined, so the slice pointers handed to forked work remain valid.
const std = @import("std");
const spice = @import("spice");
const bu = @import("benchutil");

const nqueens_work = 14; // board size
const iter_count = 1;

// answers[k] = number of solutions to the k-queens problem.
const answers = [19]u64{
    0,         1,   0,    0,     2,     10,     4,       40,       92,
    352,       724, 2680, 14200, 73712, 365596, 2279184, 14772512, 95815104,
    666090624,
};

var pool: spice.ThreadPool = undefined;

const Board = [nqueens_work]i8;
const Node = struct { buf: Board, x: u32 };
const Boards = struct { slice: []const Board, x: u32 };

fn nqueens(t: *spice.Task, node: Node) u64 {
    const x_max = node.x;
    if (x_max == nqueens_work) return 1;

    // Gather valid placements for column x_max into an on-stack array.
    var boards: [nqueens_work]Board = undefined;
    var count: u32 = 0;
    var y: i8 = 0;
    while (y < nqueens_work) : (y += 1) {
        var valid = true;
        for (node.buf[0..x_max], 0..) |p, i| {
            const d: i8 = @intCast(x_max - i);
            if (y == p or y == p - d or y == p + d) {
                valid = false;
                break;
            }
        }
        if (!valid) continue;
        boards[count] = node.buf;
        boards[count][x_max] = y;
        count += 1;
    }
    if (count == 0) return 0; // dead end
    return sumBoards(t, .{ .slice = boards[0..count], .x = x_max + 1 });
}

fn sumBoards(t: *spice.Task, b: Boards) u64 {
    if (b.slice.len == 1) {
        return nqueens(t, .{ .buf = b.slice[0], .x = b.x });
    }
    const mid = b.slice.len / 2;
    const left = Boards{ .slice = b.slice[0..mid], .x = b.x };
    const right = Boards{ .slice = b.slice[mid..], .x = b.x };

    var fut = spice.Future(Boards, u64).init();
    fut.fork(t, sumBoards, right);
    const l = t.call(u64, sumBoards, left);
    if (fut.join(t)) |r| {
        return l + r;
    } else {
        return l + t.call(u64, sumBoards, right);
    }
}

fn checkAnswer(result: u64) void {
    if (result != answers[nqueens_work]) {
        bu.print("error: expected {d}, got {d}\n", .{ answers[nqueens_work], result });
    }
}

pub fn main(init: std.process.Init) void {
    const argv = bu.Argv(1).init(init.minimal.args);
    const thread_count = argv.int(usize, 0) orelse bu.defaultThreads();
    bu.print("threads: {d}\n", .{thread_count});

    pool = spice.ThreadPool.init(bu.allocator, init.io);
    pool.start(.{ .background_worker_count = thread_count - 1 });

    const empty: Board = @splat(0);
    checkAnswer(pool.call(u64, nqueens, Node{ .buf = empty, .x = 0 })); // warmup

    const start = bu.nowNs();
    var result: u64 = 0;
    var i: usize = 0;
    while (i < iter_count) : (i += 1) {
        result = pool.call(u64, nqueens, Node{ .buf = @splat(0), .x = 0 });
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
