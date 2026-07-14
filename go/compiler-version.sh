#!/usr/bin/env bash
# Prints the Go toolchain version string (e.g. "go version go1.26.5 linux/amd64").
if command -v go >/dev/null 2>&1; then
  ver="$(go version 2>/dev/null)"
  echo "${ver:-unknown}"
else
  echo "unknown"
fi
