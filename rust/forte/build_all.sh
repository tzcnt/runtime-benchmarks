#!/usr/bin/env bash
# Builds every forte benchmark and stages the executables into ./build so that
# build_and_bench_all.py can find them at build/<benchmark>, matching the
# convention used by the C++ (CMake) runtimes.
#
# forte is a heartbeat-scheduled fork-join scheduler (like chili/spice) with a
# rayon-style join/scope, so it implements the four recursive fork-join
# benchmarks. It has no async socket or MPMC channel API, so io_socket_st and
# channel are intentionally absent (the runner skips any benchmark whose
# executable is missing from ./build).
#
# The first argument (a CMake preset name on macOS/Windows) is ignored; cargo
# does not use presets. target-cpu=native is configured in .cargo/config.toml.
set -euo pipefail
cd "$(dirname "$0")"

BENCHMARKS=(fib skynet nqueens matmul)

# The runner may request a specific forte version via this env var (used by the
# single-runtime / compare modes). Treat it as a crates.io version requirement.
# Note: forte's Cargo.toml pins =1.0.0-alpha.4 (the newest release that builds on
# Rust 1.90); overriding to 1.0.0-alpha.5+ also requires Rust >= 1.96.
if [ -n "${RUNTIME_BENCHMARKS_LIBRARY_REF:-}" ]; then
  echo "Pinning forte to ${RUNTIME_BENCHMARKS_LIBRARY_REF}"
  cargo update -p forte --precise "${RUNTIME_BENCHMARKS_LIBRARY_REF}"
fi

cargo build --release

mkdir -p build
for b in "${BENCHMARKS[@]}"; do
  cp -f "target/release/${b}" "build/${b}"
done
