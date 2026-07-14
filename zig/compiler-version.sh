#!/usr/bin/env bash
# Prints the Zig toolchain version string (e.g. "zig 0.16.0").
# The zig binary may not be on PATH; fall back to the known local install
# (/usr/local/bin/zig is the install directory, containing the executable).
if command -v zig >/dev/null 2>&1; then
  ZIG=zig
elif [ -x /usr/local/bin/zig/zig ]; then
  ZIG=/usr/local/bin/zig/zig
else
  echo "unknown"
  exit 0
fi
ver="$("$ZIG" version 2>/dev/null)"
if [ -n "$ver" ]; then
  echo "zig $ver"
else
  echo "unknown"
fi
