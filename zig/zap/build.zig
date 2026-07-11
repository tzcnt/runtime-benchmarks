const std = @import("std");

pub fn build(b: *std.Build) void {
    // Native target with native CPU features (the zig equivalent of the
    // -O3 -march=native flags used by the C++ suite).
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{ .preferred_optimize_mode = .ReleaseFast });

    const thread_pool = b.createModule(.{
        .root_source_file = b.path("src/thread_pool.zig"),
        .target = target,
        .optimize = optimize,
    });
    const benchutil = b.createModule(.{
        .root_source_file = b.path("src/benchutil.zig"),
        .target = target,
        .optimize = optimize,
    });

    // zap has no channel or I/O primitives, so only the fork-join benchmarks
    // are implemented (no channel / io_socket_st).
    inline for (.{ "fib", "skynet", "nqueens", "matmul" }) |name| {
        const exe = b.addExecutable(.{
            .name = name,
            .root_module = b.createModule(.{
                .root_source_file = b.path("src/" ++ name ++ ".zig"),
                .target = target,
                .optimize = optimize,
                .imports = &.{
                    .{ .name = "thread_pool", .module = thread_pool },
                    .{ .name = "benchutil", .module = benchutil },
                },
            }),
        });
        b.installArtifact(exe);
    }
}
