#!/usr/bin/env bash
# Prints the Rust compiler (rustc) version string, e.g.
# "rustc 1.90.0 (1159e78c4 2025-09-14)".
if command -v rustc >/dev/null 2>&1; then
  ver="$(rustc --version 2>/dev/null)"
  echo "${ver:-unknown}"
else
  echo "unknown"
fi
