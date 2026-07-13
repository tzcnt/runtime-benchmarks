#!/usr/bin/env bash
# Builds every Spice benchmark and stages the executables into ./build so that
# build_and_bench_all.py can find them at build/<benchmark>, matching the
# convention used by the other runtimes.
#
# The first argument (a CMake preset name on macOS/Windows) is ignored; zig
# build does not use presets.
#
# The Spice sources are cloned into vendor/spice at a pinned commit that
# explicitly targets Zig 0.16, so no compatibility patch is needed (unlike
# zigbeam). A bugfix patch (fix-stale-job-tail.patch) is applied on top: it
# fixes a job-queue corruption (stale Task.job_tail after inline calls and
# executing-state joins) that crashed matmul at the benchmarked n=2048. The
# fix is prepared as an upstream PR; drop the patch once a ref containing it
# is pinned. RUNTIME_BENCHMARKS_LIBRARY_REF overrides the pinned ref for the
# single-runtime / compare modes, but note the patch was written against the
# pinned commit and may not apply to arbitrary refs.
set -euo pipefail
cd "$(dirname "$0")"

SPICE_REPO="https://github.com/judofyr/spice"
# Pinned to the commit titled "Support 0.16.0-dev.2915+065c6e794", which builds
# against this suite's Zig 0.16 toolchain.
SPICE_REF="${RUNTIME_BENCHMARKS_LIBRARY_REF:-39186ac9da2b851f558a33926836e605e238694d}"

if [ ! -d vendor/spice/.git ]; then
  rm -rf vendor/spice
  mkdir -p vendor
  git clone "$SPICE_REPO" vendor/spice
fi

# Check out the requested ref, dropping any previously applied patch first so
# the tree is pristine before re-applying.
git -C vendor/spice reset --hard >/dev/null
git -C vendor/spice checkout --detach "$SPICE_REF" >/dev/null 2>&1 || {
  git -C vendor/spice fetch origin
  git -C vendor/spice checkout --detach "$SPICE_REF" >/dev/null
}
git -C vendor/spice apply "$(pwd)/fix-stale-job-tail.patch"

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
