#!/usr/bin/env bash
# Builds every Kotlin coroutine benchmark into a single runtime-bundled jar and
# stages a small launcher script per benchmark into ./build, so that
# build_and_bench_all.py can find them at build/<benchmark>, matching the
# convention used by the C++ (CMake), Rust (cargo) and Go runtimes.
#
# This is the "fjp" variant: coroutines run on a JDK ForkJoinPool in traditional
# (LIFO-local) work-stealing mode. The scheduler choice lives entirely in
# benchutil.kt; the benchmark sources are shared verbatim with kotlin/default.
#
# The first argument (a CMake preset name on macOS/Windows) is ignored; kotlinc
# does not use presets.
#
# RUNTIME_BENCHMARKS_LIBRARY_REF (used by the single-runtime / compare modes to
# pin a library version) is intentionally ignored here: the "library" under test
# is Kotlin's coroutines (kotlinx.coroutines), whose version is the one shipped
# with the installed Kotlin toolchain. Select a different toolchain (e.g. via
# `sdk use kotlin <version>`) to compare coroutine versions.
set -euo pipefail
cd "$(dirname "$0")"

# kotlinx-coroutines-core is shipped in the Kotlin toolchain's lib directory.
KOTLINC_BIN="$(command -v kotlinc)"
if [[ -z "$KOTLINC_BIN" ]]; then
  echo "kotlinc not found on PATH" >&2
  exit 1
fi
KOTLIN_LIB="$(dirname "$(dirname "$(readlink -f "$KOTLINC_BIN")")")/lib"
CORO_JAR="$KOTLIN_LIB/kotlinx-coroutines-core-jvm.jar"
if [[ ! -f "$CORO_JAR" ]]; then
  echo "kotlinx-coroutines-core-jvm.jar not found at $CORO_JAR" >&2
  exit 1
fi

BENCHMARKS=(skynet fib nqueens matmul channel io_socket_st)

rm -rf build
mkdir -p build

# Compile all benchmarks + the shared benchutil into a single jar with the Kotlin
# runtime bundled in. Each benchmark's package exposes a `Main` entry point (see
# @file:JvmName("Main")), e.g. skynet.Main.
kotlinc benchutil.kt "${BENCHMARKS[@]/%/.kt}" \
  -include-runtime -cp "$CORO_JAR" -d build/bench.jar

cp "$CORO_JAR" build/kotlinx-coroutines-core-jvm.jar

# One launcher per benchmark. Each resolves its own directory so it can be
# invoked by absolute path from the harness regardless of the working directory.
for b in "${BENCHMARKS[@]}"; do
  cat > "build/${b}" <<EOF
#!/usr/bin/env bash
DIR="\$(cd "\$(dirname "\${BASH_SOURCE[0]}")" && pwd)"
exec java -cp "\$DIR/bench.jar:\$DIR/kotlinx-coroutines-core-jvm.jar" ${b}.Main "\$@"
EOF
  chmod +x "build/${b}"
done
