// Helpers shared across the zap benchmark binaries so their setup and
// reported numbers line up with the C++ (TMC) / Go / Rust suites.
//
// Zig 0.16 note: stdout, timing and futex access all go through thin Linux
// syscall wrappers because the std.Io-based equivalents require threading an
// `Io` instance through the benchmark hot path.
const std = @import("std");
const linux = std.os.linux;

/// Multi-threaded, ReleaseFast-oriented general purpose allocator; fills the
/// role of the high-performance allocator (tcmalloc/jemalloc) the C++ suite
/// links against.
pub const allocator = std.heap.smp_allocator;

/// Monotonic clock in nanoseconds (std.time.Timer was removed in Zig 0.16).
pub fn nowNs() u64 {
    var ts: linux.timespec = undefined;
    _ = linux.clock_gettime(.MONOTONIC, &ts);
    return @as(u64, @intCast(ts.sec)) * std.time.ns_per_s + @as(u64, @intCast(ts.nsec));
}

/// Peak resident set size of the current process in KiB, mirroring
/// cpp/2common/memusage.hpp so the reported numbers are directly comparable.
pub fn peakMemoryUsageKiB() i64 {
    const ru = std.posix.getrusage(linux.rusage.SELF);
    return @intCast(ru.maxrss); // Linux reports ru_maxrss in KiB
}

/// Mirrors the C++/Rust default worker count of hardware_concurrency() / 2.
pub fn defaultThreads() u32 {
    const n = (std.Thread.getCpuCount() catch 2) / 2;
    return @max(1, @as(u32, @intCast(n)));
}

/// Formatted print to stdout (the harness parses stdout as YAML; std.debug.print
/// writes to stderr and must not be used for results).
pub fn print(comptime fmt: []const u8, args: anytype) void {
    var buf: [512]u8 = undefined;
    const s = std.fmt.bufPrint(&buf, fmt, args) catch return;
    _ = linux.write(1, s.ptr, s.len);
}

/// Collect up to `max` command line arguments (after argv[0]) as slices.
pub fn Argv(comptime max: usize) type {
    return struct {
        buf: [max][:0]const u8 = undefined,
        len: usize = 0,

        pub fn init(args: std.process.Args) @This() {
            var self: @This() = .{};
            var it = std.process.Args.Iterator.init(args);
            _ = it.skip(); // argv[0]
            while (it.next()) |a| {
                if (self.len == max) break;
                self.buf[self.len] = a;
                self.len += 1;
            }
            return self;
        }

        pub fn get(self: *const @This(), idx: usize) ?[:0]const u8 {
            return if (idx < self.len) self.buf[idx] else null;
        }

        /// Parse argv[idx+1] as an integer, or null if absent/unparseable.
        pub fn int(self: *const @This(), comptime T: type, idx: usize) ?T {
            const s = self.get(idx) orelse return null;
            return std.fmt.parseInt(T, s, 10) catch null;
        }
    };
}

/// One-shot completion event: the benchmark's main thread parks on it while
/// the pool runs the task graph. Reusable via reset().
pub const DoneEvent = struct {
    state: std.atomic.Value(u32) = std.atomic.Value(u32).init(0),

    pub fn wait(self: *DoneEvent) void {
        while (self.state.load(.acquire) == 0) {
            _ = linux.futex_4arg(&self.state.raw, .{ .cmd = .WAIT, .private = true }, 0, null);
        }
    }

    pub fn set(self: *DoneEvent) void {
        self.state.store(1, .release);
        _ = linux.futex_3arg(&self.state.raw, .{ .cmd = .WAKE, .private = true }, std.math.maxInt(i32));
    }

    pub fn reset(self: *DoneEvent) void {
        self.state.store(0, .monotonic);
    }
};
