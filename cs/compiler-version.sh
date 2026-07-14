#!/usr/bin/env bash
# Prints the .NET SDK version (the C# / Roslyn toolchain shipped with it),
# e.g. ".NET SDK 10.0.301".
if command -v dotnet >/dev/null 2>&1; then
  ver="$(dotnet --version 2>/dev/null)"
  if [ -n "$ver" ]; then echo ".NET SDK $ver"; else echo "unknown"; fi
else
  echo "unknown"
fi
