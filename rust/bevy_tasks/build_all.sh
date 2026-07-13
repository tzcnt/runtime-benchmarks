#!/usr/bin/env bash
# Builds every bevy_tasks benchmark and stages the executables into ./build so
# that build_and_bench_all.py can find them at build/<benchmark>, matching the
# convention used by the C++ (CMake) runtimes.
#
# bevy_tasks is Bevy's async task pool (built on async-executor). Tasks can
# spawn and await further tasks on the same pool, so it implements the four
# recursive fork-join benchmarks. It has no async socket API and no MPMC
# channel, so io_socket_st and channel are intentionally absent (the runner
# skips any benchmark whose executable is missing from ./build).
#
# The first argument (a CMake preset name on macOS/Windows) is ignored; cargo
# does not use presets. target-cpu=native is configured in .cargo/config.toml.
set -euo pipefail
cd "$(dirname "$0")"

BENCHMARKS=(fib skynet nqueens matmul)

# The runner may request a specific bevy_tasks version via this env var (used by
# the single-runtime / compare modes). Treat it as a crates.io version requirement.
if [ -n "${RUNTIME_BENCHMARKS_LIBRARY_REF:-}" ]; then
  echo "Pinning bevy_tasks to ${RUNTIME_BENCHMARKS_LIBRARY_REF}"
  cargo update -p bevy_tasks --precise "${RUNTIME_BENCHMARKS_LIBRARY_REF}"
fi

cargo build --release

mkdir -p build
for b in "${BENCHMARKS[@]}"; do
  cp -f "target/release/${b}" "build/${b}"
done
