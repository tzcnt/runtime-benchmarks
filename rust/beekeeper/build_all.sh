#!/usr/bin/env bash
# Builds every beekeeper benchmark and stages the executables into ./build so
# that build_and_bench_all.py can find them at build/<benchmark>, matching the
# convention used by the C++ (CMake) runtimes.
#
# beekeeper is a synchronous worker-pool ("hive") library. Workers can fork
# subtasks from inside a task (Context::submit), so the four recursive
# fork-join benchmarks are implementable, and its task->outcome pipeline is an
# MPMC queue primitive, so channel is too. It has no async socket API, so
# io_socket_st is intentionally absent (the runner skips any benchmark whose
# executable is missing from ./build).
#
# The first argument (a CMake preset name on macOS/Windows) is ignored; cargo
# does not use presets. target-cpu=native is configured in .cargo/config.toml.
set -euo pipefail
cd "$(dirname "$0")"

BENCHMARKS=(fib skynet nqueens matmul channel)

# The runner may request a specific beekeeper version via this env var (used by
# the single-runtime / compare modes). Treat it as a crates.io version
# requirement.
if [ -n "${RUNTIME_BENCHMARKS_LIBRARY_REF:-}" ]; then
  echo "Pinning beekeeper to ${RUNTIME_BENCHMARKS_LIBRARY_REF}"
  cargo update -p beekeeper --precise "${RUNTIME_BENCHMARKS_LIBRARY_REF}"
fi

cargo build --release

mkdir -p build
for b in "${BENCHMARKS[@]}"; do
  cp -f "target/release/${b}" "build/${b}"
done
