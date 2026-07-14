#!/usr/bin/env bash
# Builds every Go stdlib benchmark and stages the executables into ./build so
# that build_and_bench_all.py can find them at build/<benchmark>, matching the
# convention used by the C++ (CMake) and Rust (cargo) runtimes.
#
# The first argument (a CMake preset name on macOS/Windows) is ignored; go build
# does not use presets.
#
# RUNTIME_BENCHMARKS_LIBRARY_REF (used by the single-runtime / compare modes to
# pin a library version) is intentionally ignored here: the "library" under test
# is the Go standard library, whose version is the Go toolchain version. Select a
# different toolchain (e.g. via `go env -w GOTOOLCHAIN=go1.25.0` or a `toolchain`
# line in go.mod) to compare stdlib versions.
set -euo pipefail
cd "$(dirname "$0")"

BENCHMARKS=(skynet fib nqueens matmul channel io_socket_st)

mkdir -p build
for b in "${BENCHMARKS[@]}"; do
  go build -o "build/${b}" "./cmd/${b}"
done
