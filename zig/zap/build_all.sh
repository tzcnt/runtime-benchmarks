#!/usr/bin/env bash
# Builds every zap benchmark and stages the executables into ./build so that
# build_and_bench_all.py can find them at build/<benchmark>, matching the
# convention used by the other runtimes.
#
# The first argument (a CMake preset name on macOS/Windows) is ignored; zig
# build does not use presets.
#
# RUNTIME_BENCHMARKS_LIBRARY_REF is intentionally ignored here: the library
# under test is a vendored port of kprotty/zap's "blog" branch
# (src/thread_pool.zig), because upstream targets a pre-0.10 Zig and no longer
# compiles. There is no other version to check out.
set -euo pipefail
cd "$(dirname "$0")"

# The zig toolchain may not be on PATH; fall back to the known local install
# (/usr/local/bin/zig is the install directory, containing the executable).
if command -v zig >/dev/null 2>&1; then
  ZIG=zig
else
  ZIG=/usr/local/bin/zig/zig
fi

"$ZIG" build --release=fast

mkdir -p build
cp -f zig-out/bin/* build/
