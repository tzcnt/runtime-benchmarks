#!/usr/bin/env bash
# Builds every zigbeam benchmark and stages the executables into ./build so
# that build_and_bench_all.py can find them at build/<benchmark>, matching the
# convention used by the other runtimes.
#
# The first argument (a CMake preset name on macOS/Windows) is ignored; zig
# build does not use presets.
#
# The zigbeam sources are fetched into vendor/zigbeam at a pinned commit and a
# small Zig 0.16 compatibility patch (zig-0.16-compat.patch) is applied; see
# that file for the details. RUNTIME_BENCHMARKS_LIBRARY_REF overrides the
# pinned ref for the single-runtime / compare modes, but note the compat patch
# was written against the pinned commit and may not apply to arbitrary refs.
set -euo pipefail
cd "$(dirname "$0")"

ZIGBEAM_REPO="https://github.com/eakova/zigbeam"
ZIGBEAM_REF="${RUNTIME_BENCHMARKS_LIBRARY_REF:-0120a6851a9c7c7ddb1cadaba25545b2a44472cb}"

if [ ! -d vendor/zigbeam/.git ]; then
  rm -rf vendor/zigbeam
  mkdir -p vendor
  git clone "$ZIGBEAM_REPO" vendor/zigbeam
fi

# Check out the requested ref, dropping any previously applied patch first so
# the tree is pristine before re-applying.
git -C vendor/zigbeam reset --hard >/dev/null
git -C vendor/zigbeam checkout --detach "$ZIGBEAM_REF" >/dev/null 2>&1 || {
  git -C vendor/zigbeam fetch origin
  git -C vendor/zigbeam checkout --detach "$ZIGBEAM_REF" >/dev/null
}
git -C vendor/zigbeam apply "$(pwd)/zig-0.16-compat.patch"

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
