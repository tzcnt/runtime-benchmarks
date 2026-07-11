#!/usr/bin/env bash
# Builds every C# stdlib benchmark and stages the executables into ./build so
# that build_and_bench_all.py can find them at build/<benchmark>, matching the
# convention used by the C++ (CMake), Rust (cargo) and Go runtimes.
#
# The first argument (a CMake preset name on macOS/Windows) is ignored; dotnet
# does not use presets. Each benchmark is published self-contained for the
# current runtime so build/<benchmark> is a standalone executable that runs even
# though the dotnet SDK is not on PATH (mirroring how the harness invokes the
# native binaries directly). The self-contained apps share one copy of the
# runtime DLLs in ./build.
#
# RUNTIME_BENCHMARKS_LIBRARY_REF (used by the single-runtime / compare modes to
# pin a library version) is intentionally ignored here: the "library" under test
# is the .NET base class library, whose version is the .NET SDK/runtime version.
# Select a different toolchain (e.g. via a global.json "sdk" version, or a
# different DOTNET_ROOT) to compare stdlib versions.
set -euo pipefail
cd "$(dirname "$0")"

# Locate the dotnet SDK. Honor an explicit DOTNET_ROOT, else fall back to the
# per-user install location.
export DOTNET_ROOT="${DOTNET_ROOT:-$HOME/.dotnet}"
export PATH="$DOTNET_ROOT:$PATH"
export DOTNET_CLI_TELEMETRY_OPTOUT=1
export DOTNET_NOLOGO=1

if ! command -v dotnet >/dev/null 2>&1; then
  echo "error: dotnet SDK not found. Set DOTNET_ROOT or add dotnet to PATH." >&2
  exit 1
fi

BENCHMARKS=(skynet fib nqueens matmul channel io_socket_st)
OUT="$(pwd)/build"

rm -rf "$OUT"
mkdir -p "$OUT"
for b in "${BENCHMARKS[@]}"; do
  dotnet publish "${b}/${b}.csproj" \
    -c Release \
    --self-contained true \
    -p:UseCurrentRuntime=true \
    -o "$OUT"
done
