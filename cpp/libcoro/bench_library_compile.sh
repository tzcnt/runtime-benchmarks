#!/bin/bash
set -e

PRESET=${1:-"clang-linux-release"}

echo "=== Library Compilation Benchmark ==="
echo "Using preset: $PRESET"
echo

# Configure if needed
cmake --preset $PRESET -DBENCHMARK_COMPILE_TIME=TRUE .

# Clean without deleting build directory
echo "=== Cleaning build artifacts ==="
cmake --build ./build --target clean
echo

echo "=== Building library ==="
START=$(date +%s.%N)
cmake --build ./build --target libcoro
END=$(date +%s.%N)
LIBRARY_COMPILE_TIME=$(echo "$END - $START" | bc)
echo

echo "=== Building benchmarks (executable) ==="
START=$(date +%s.%N)
# Disable parallelism for consistent benchmarking
cmake --build ./build --parallel 1 --target fib matmul nqueens skynet
END=$(date +%s.%N)
LIBRARY_USER_TIME=$(echo "$END - $START" | bc)
echo

echo "=== Summary ==="
echo "library build time: ${LIBRARY_COMPILE_TIME}s"
echo "benchmarks build time: ${LIBRARY_USER_TIME}s"
