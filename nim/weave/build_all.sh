#!/usr/bin/env bash
# Builds every Nim / Weave benchmark and stages the executables into ./build so
# that build_and_bench_all.py can find them at build/<benchmark>, matching the
# convention used by the C++ (CMake), Rust (cargo), and Go runtimes.
#
# The first argument (a CMake preset name on macOS/Windows) is ignored; nim
# does not use presets.
#
# Dependencies (weave, threading, and weave's transitive dep synthesis) are
# declared in weavebench.nimble and pinned to exact git revisions by the
# committed nimble.lock. `nimble setup` installs them and generates
# nimble.paths, which the (committed) config.nims feeds to every plain `nim c`
# invocation in this directory - so builds use exactly the locked revisions,
# regardless of what else is in the global nimble cache.
set -euo pipefail
cd "$(dirname "$0")"

# choosenim installs to ~/.nimble/bin, which is typically not on PATH for the
# non-login shell the benchmark harness runs this script with.
if ! command -v nim >/dev/null 2>&1 && [ -x "$HOME/.nimble/bin/nim" ]; then
  export PATH="$HOME/.nimble/bin:$PATH"
fi

# The library under test is pinned by nimble.lock, so the runner's
# single-runtime / compare ref modes cannot apply here. Fail loudly rather
# than silently benchmarking the locked version under the requested ref's name.
# To change the weave version: edit weavebench.nimble, delete nimble.lock, and
# re-run `nimble lock`.
if [ -n "${RUNTIME_BENCHMARKS_LIBRARY_REF:-}" ]; then
  echo "ERROR: weave is pinned by nimble.lock; RUNTIME_BENCHMARKS_LIBRARY_REF is not supported." >&2
  exit 1
fi

BENCHMARKS=(skynet fib nqueens matmul channel io_socket_st)

# Install the locked dependencies and (re)generate nimble.paths when it is
# missing or the lockfile changed; repeat builds stay offline.
if [ ! -f nimble.paths ] || [ nimble.lock -nt nimble.paths ]; then
  nimble setup -y
fi

# -d:danger (all runtime checks off) + -march=native to match the C++ suite's
# -O3 -march=native release flags. The nimcache lives under build/ so that the
# runner's clean_build wipes it too.
mkdir -p build
for b in "${BENCHMARKS[@]}"; do
  nim c -d:danger --threads:on --passC:-march=native \
    --nimcache:"build/nimcache/${b}" \
    --hints:off \
    -o:"build/${b}" "${b}.nim"
done
