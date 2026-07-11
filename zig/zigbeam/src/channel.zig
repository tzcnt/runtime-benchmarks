// Test performance of an MPMC channel primitive.
//
// Canonical (TooManyCooks) implementation:
// ../../../cpp/TooManyCooks/channel.cpp
//
// N producers each post a slice of element_count items; M consumers pull until
// the channel is drained; the total count and sum are validated.
//
// zigbeam's bounded lock-free MPMC primitive is the DVyukov MPMC queue; this
// uses its sharded variant (the library's high-contention configuration).
// Each producer enqueues to a home shard and spills to the next on full; each
// consumer dequeues from a home shard and sweeps the others when it runs dry,
// so a drained channel is detected deterministically (a full sweep finding
// every shard empty after all producers finished). zigbeam's Backoff handles
// full/empty waits. Like the Go version's buffered channel (and unlike the
// unbounded queues some other runtimes use), it is bounded; a generous
// capacity keeps the benchmark measuring steady-state throughput rather than
// constant parking. Producers and consumers run on OS threads, splitting the
// requested thread count evenly between the two roles.
const std = @import("std");
const dvyukov = @import("sharded-dvyukov-mpmc");
const backoff_mod = @import("backoff");
const bu = @import("benchutil");

const element_count: u64 = 10_000_000;
const iter_count = 1;
const num_shards = 16;
const channel_capacity = 1 << 16; // total, split across the shards

const Queue = dvyukov.ShardedDVyukovMPMCQueue(u64, num_shards, channel_capacity / num_shards);
const Backoff = backoff_mod.Backoff;

// Low-latency backoff for a saturated throughput benchmark: never fall into
// the default 1ms power-saving sleeps, which turn momentary full/empty blips
// into millisecond stalls.
const backoff_config = backoff_mod.Config{ .sleep_limit = std.math.maxInt(u32) };

var queue: *Queue = undefined;
// Set (with release ordering) after every producer thread has been joined;
// consumers seeing it may treat an empty queue as fully drained.
var producers_done = std.atomic.Value(bool).init(false);

const Counts = struct {
    count: u64 = 0,
    sum: u64 = 0,
};

fn producer(id: usize, count: u64, base: u64) void {
    var shard = id % num_shards;
    var i: u64 = 0;
    while (i < count) : (i += 1) {
        const v = base + i;
        var backoff = Backoff.init(backoff_config);
        var attempts: usize = 0;
        while (true) {
            queue.enqueueToShard(shard, v) catch {
                // Home shard full: spill to the next; snooze after a full
                // rotation found every shard full.
                shard = (shard + 1) % num_shards;
                attempts += 1;
                if (attempts % num_shards == 0) backoff.snooze();
                continue;
            };
            break;
        }
    }
}

/// Dequeue from the first non-empty shard, starting the sweep at `start`.
fn dequeueSweep(start: usize) ?u64 {
    var k: usize = 0;
    while (k < num_shards) : (k += 1) {
        if (queue.dequeueFromShard((start + k) % num_shards)) |v| return v;
    }
    return null;
}

fn consumer(id: usize, out: *Counts) void {
    const home = id % num_shards;
    var counts = Counts{};
    var backoff = Backoff.init(backoff_config);
    while (true) {
        if (dequeueSweep(home)) |v| {
            counts.count += 1;
            counts.sum += v;
            backoff.reset();
        } else if (producers_done.load(.acquire)) {
            // All items are visible now; one more full sweep distinguishes a
            // momentarily-contended channel from a drained one.
            if (dequeueSweep(home)) |v| {
                counts.count += 1;
                counts.sum += v;
                backoff.reset();
                continue;
            }
            break;
        } else {
            backoff.snooze(); // channel momentarily empty
        }
    }
    out.* = counts;
}

fn doBench(producer_count: usize, consumer_count: usize) u64 {
    queue = bu.allocator.create(Queue) catch @panic("oom");
    queue.* = Queue.init(bu.allocator) catch @panic("queue init failed");
    producers_done.store(false, .monotonic);

    const producers = bu.allocator.alloc(std.Thread, producer_count) catch @panic("oom");
    defer bu.allocator.free(producers);
    const consumers = bu.allocator.alloc(std.Thread, consumer_count) catch @panic("oom");
    defer bu.allocator.free(consumers);
    const results = bu.allocator.alloc(Counts, consumer_count) catch @panic("oom");
    defer bu.allocator.free(results);

    // Split element_count into per-producer (count, base) assignments.
    const per = element_count / producer_count;
    const rem = element_count % producer_count;
    var base: u64 = 0;
    for (producers, 0..) |*t, i| {
        var count = per;
        if (i < rem) count += 1;
        t.* = std.Thread.spawn(.{}, producer, .{ i, count, base }) catch @panic("spawn failed");
        base += count;
    }
    for (consumers, 0..) |*t, i| {
        t.* = std.Thread.spawn(.{}, consumer, .{ i, &results[i] }) catch @panic("spawn failed");
    }

    // Once every producer has finished, mark the queue as complete so the
    // consumers stop after draining what remains.
    for (producers) |t| t.join();
    producers_done.store(true, .release);
    for (consumers) |t| t.join();

    var total = Counts{};
    for (results) |r| {
        total.count += r.count;
        total.sum += r.sum;
    }

    const expected_sum = element_count * (element_count - 1) / 2;
    if (total.count != element_count) {
        bu.print("FAIL: Expected {d} elements but consumed {d} elements\n", .{ element_count, total.count });
    }
    if (total.sum != expected_sum) {
        bu.print("FAIL: Expected {d} sum but got {d} sum\n", .{ expected_sum, total.sum });
    }

    queue.deinit();
    bu.allocator.destroy(queue);
    return total.sum;
}

pub fn main(init: std.process.Init.Minimal) void {
    const argv = bu.Argv(1).init(init.args);
    const thread_count = argv.int(u32, 0) orelse bu.defaultThreads();

    const per_role = @max(1, thread_count / 2);
    const producer_count: usize = per_role;
    const consumer_count: usize = per_role;

    bu.print("threads: {d}\n", .{thread_count});
    bu.print("producers: {d}\n", .{producer_count});
    bu.print("consumers: {d}\n", .{consumer_count});

    const result = doBench(producer_count, consumer_count); // warmup
    bu.print("output: {d}\n", .{result});

    const start = bu.nowNs();
    var i: usize = 0;
    while (i < iter_count) : (i += 1) {
        _ = doBench(producer_count, consumer_count);
    }
    var dur_us = (bu.nowNs() - start) / std.time.ns_per_us;
    if (dur_us < 1) dur_us = 1;
    const elements_per_sec = element_count * 1_000_000 / dur_us;

    bu.print("runs:\n", .{});
    bu.print("  - iteration_count: {d}\n", .{iter_count});
    bu.print("    elements: {d}\n", .{element_count});
    bu.print("    duration: {d} us\n", .{dur_us});
    bu.print("    elements/sec: {d}\n", .{elements_per_sec});
    bu.print("    max_rss: {d} KiB\n", .{bu.peakMemoryUsageKiB()});
}
