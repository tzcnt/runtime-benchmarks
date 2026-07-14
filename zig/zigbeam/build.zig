const std = @import("std");

// The zigbeam sources are cloned into vendor/zigbeam by build_all.sh (with a
// small Zig 0.16 compatibility patch applied). Rather than using zigbeam's own
// build.zig (which builds its full workspace of samples/tests), this declares
// just the module subgraph the benchmarks need, mirroring the module names and
// dependencies zigbeam's build.zig assigns:
//   loom -> { backoff, deque-channel, loom (self) }
//   deque-channel -> { deque, dvyukov-mpmc }
//   deque -> { backoff }
pub fn build(b: *std.Build) void {
    // Native target with native CPU features (the zig equivalent of the
    // -O3 -march=native flags used by the C++ suite).
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{ .preferred_optimize_mode = .ReleaseFast });

    const src = "vendor/zigbeam/src/";

    const backoff = b.createModule(.{
        .root_source_file = b.path(src ++ "libs/backoff/backoff.zig"),
        .target = target,
        .optimize = optimize,
    });
    const dvyukov_mpmc = b.createModule(.{
        .root_source_file = b.path(src ++ "libs/dvyukov-mpmc/dvyukov_mpmc_queue.zig"),
        .target = target,
        .optimize = optimize,
    });
    const sharded_dvyukov_mpmc = b.createModule(.{
        .root_source_file = b.path(src ++ "libs/dvyukov-mpmc/sharded_dvyukov_mpmc_queue.zig"),
        .target = target,
        .optimize = optimize,
        .imports = &.{
            .{ .name = "dvyukov-mpmc", .module = dvyukov_mpmc },
        },
    });
    const deque = b.createModule(.{
        .root_source_file = b.path(src ++ "libs/deque/deque.zig"),
        .target = target,
        .optimize = optimize,
        .imports = &.{
            .{ .name = "backoff", .module = backoff },
        },
    });
    const deque_channel = b.createModule(.{
        .root_source_file = b.path(src ++ "libs/deque/deque_channel.zig"),
        .target = target,
        .optimize = optimize,
        .imports = &.{
            .{ .name = "deque", .module = deque },
            .{ .name = "dvyukov-mpmc", .module = dvyukov_mpmc },
        },
    });
    const loom = b.createModule(.{
        .root_source_file = b.path(src ++ "loom/loom.zig"),
        .target = target,
        .optimize = optimize,
        .imports = &.{
            .{ .name = "backoff", .module = backoff },
            .{ .name = "deque-channel", .module = deque_channel },
        },
    });
    loom.addImport("loom", loom); // loom's iterator files self-import by name

    const benchutil = b.createModule(.{
        .root_source_file = b.path("src/benchutil.zig"),
        .target = target,
        .optimize = optimize,
    });

    // The fork-join benchmarks run on Loom; channel runs on the DVyukov MPMC
    // queue. zigbeam has no socket I/O library, so io_socket_st is not
    // implemented.
    const fork_join_benchmarks = [_][]const u8{ "fib", "skynet", "nqueens", "matmul" };
    inline for (fork_join_benchmarks) |name| {
        const exe = b.addExecutable(.{
            .name = name,
            .root_module = b.createModule(.{
                .root_source_file = b.path("src/" ++ name ++ ".zig"),
                .target = target,
                .optimize = optimize,
                .imports = &.{
                    .{ .name = "loom", .module = loom },
                    .{ .name = "benchutil", .module = benchutil },
                },
            }),
        });
        b.installArtifact(exe);
    }

    const channel_exe = b.addExecutable(.{
        .name = "channel",
        .root_module = b.createModule(.{
            .root_source_file = b.path("src/channel.zig"),
            .target = target,
            .optimize = optimize,
            .imports = &.{
                .{ .name = "sharded-dvyukov-mpmc", .module = sharded_dvyukov_mpmc },
                .{ .name = "backoff", .module = backoff },
                .{ .name = "benchutil", .module = benchutil },
            },
        }),
    });
    b.installArtifact(channel_exe);
}
