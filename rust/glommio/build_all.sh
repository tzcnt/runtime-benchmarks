#!/usr/bin/env bash
# Builds the glommio benchmark(s) and stages the executables into ./build so that
# build_and_bench_all.py can find them at build/<benchmark>, matching the
# convention used by the C++ (CMake) runtimes.
#
# glommio is thread-per-core with no cross-core work-stealing and only SPSC
# channels, so only io_socket_st applies (see Cargo.toml for the rationale).
#
# The first argument (a CMake preset name on macOS/Windows) is ignored; cargo
# does not use presets. target-cpu=native is configured in .cargo/config.toml.
set -euo pipefail
cd "$(dirname "$0")"

BENCHMARKS=(io_socket_st)

# The runner may request a specific glommio version via this env var (used by the
# single-runtime / compare modes). Treat it as a crates.io version requirement.
if [ -n "${RUNTIME_BENCHMARKS_LIBRARY_REF:-}" ]; then
  echo "Pinning glommio to ${RUNTIME_BENCHMARKS_LIBRARY_REF}"
  cargo update -p glommio --precise "${RUNTIME_BENCHMARKS_LIBRARY_REF}"
fi

cargo build --release

mkdir -p build
for b in "${BENCHMARKS[@]}"; do
  cp -f "target/release/${b}" "build/${b}"
done
