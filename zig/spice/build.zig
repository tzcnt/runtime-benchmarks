const std = @import("std");

// The Spice sources are cloned into vendor/spice at a pinned commit by
// build_all.sh. Rather than going through Spice's own build.zig (which pulls in
// a lazy `parg` dependency used only by its example), this declares just the
// single-file `spice` module the benchmarks need (vendor/spice/src/root.zig).
pub fn build(b: *std.Build) void {
    // Native target with native CPU features (the zig equivalent of the
    // -O3 -march=native flags used by the C++ suite).
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{ .preferred_optimize_mode = .ReleaseFast });

    const spice = b.createModule(.{
        .root_source_file = b.path("vendor/spice/src/root.zig"),
        .target = target,
        .optimize = optimize,
    });
    const benchutil = b.createModule(.{
        .root_source_file = b.path("src/benchutil.zig"),
        .target = target,
        .optimize = optimize,
    });

    // Spice is a fork-join-only library (heartbeat-scheduled join), so only the
    // recursive fork-join benchmarks apply (no channel / io).
    //
    // matmul requires the stale-job_tail bugfix that build_all.sh patches into
    // the vendored Spice checkout (fix-stale-job-tail.patch, prepared as an
    // upstream PR); see the note at the top of src/matmul.zig.
    inline for (.{ "fib", "skynet", "nqueens", "matmul" }) |name| {
        const exe = b.addExecutable(.{
            .name = name,
            .root_module = b.createModule(.{
                .root_source_file = b.path("src/" ++ name ++ ".zig"),
                .target = target,
                .optimize = optimize,
                .imports = &.{
                    .{ .name = "spice", .module = spice },
                    .{ .name = "benchutil", .module = benchutil },
                },
            }),
        });
        b.installArtifact(exe);
    }
}
