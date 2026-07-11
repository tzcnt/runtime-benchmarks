#!/usr/bin/env bash
# Prints the C compiler version string used to build the C benchmarks.
# Uses gcc, matching the default CMake preset in c/neco/build_all.sh.
if command -v gcc >/dev/null 2>&1; then
  ver="$(gcc --version 2>/dev/null | head -n1)"
  echo "${ver:-unknown}"
else
  echo "unknown"
fi
