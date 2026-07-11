#!/usr/bin/env bash
# Prints the C++ compiler version string used to build the C++ benchmarks.
# Uses clang++, matching the clang-* CMake presets in build_and_bench_all.py.
if command -v clang++ >/dev/null 2>&1; then
  ver="$(clang++ --version 2>/dev/null | head -n1)"
  echo "${ver:-unknown}"
else
  echo "unknown"
fi
