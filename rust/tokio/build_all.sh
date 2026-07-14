#!/usr/bin/env bash
# Builds every tokio benchmark and stages the executables into ./build so that
# build_and_bench_all.py can find them at build/<benchmark>, matching the
# convention used by the C++ (CMake) runtimes.
#
# The first argument (a CMake preset name on macOS/Windows) is ignored; cargo
# does not use presets. target-cpu=native is configured in .cargo/config.toml.
set -euo pipefail
cd "$(dirname "$0")"

BENCHMARKS=(skynet fib nqueens matmul channel io_socket_st)

# The runner may request a specific tokio version via this env var (used by the
# single-runtime / compare modes). Treat it as a crates.io version requirement.
if [ -n "${RUNTIME_BENCHMARKS_LIBRARY_REF:-}" ]; then
  echo "Pinning tokio to ${RUNTIME_BENCHMARKS_LIBRARY_REF}"
  cargo update -p tokio --precise "${RUNTIME_BENCHMARKS_LIBRARY_REF}"
fi

cargo build --release

mkdir -p build
for b in "${BENCHMARKS[@]}"; do
  cp -f "target/release/${b}" "build/${b}"
done
