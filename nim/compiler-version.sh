#!/usr/bin/env bash
# Prints the Nim toolchain version string (e.g. "Nim Compiler Version 2.2.10 [Linux: amd64]").

# choosenim installs to ~/.nimble/bin, which is typically not on PATH for the
# non-login shell the benchmark harness runs this script with.
if ! command -v nim >/dev/null 2>&1 && [ -x "$HOME/.nimble/bin/nim" ]; then
  export PATH="$HOME/.nimble/bin:$PATH"
fi

if command -v nim >/dev/null 2>&1; then
  ver="$(nim --version 2>/dev/null | head -n 1)"
  echo "${ver:-unknown}"
else
  echo "unknown"
fi
