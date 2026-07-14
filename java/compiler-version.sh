#!/usr/bin/env bash
# Prints the Java compiler (javac) version string, e.g. "javac 25.0.4-ea".
if command -v javac >/dev/null 2>&1; then
  # `javac --version` prints to stdout (JDK 9+); drop stderr so a JVM launcher
  # note like "Picked up JAVA_TOOL_OPTIONS: ..." can't shadow the version line.
  ver="$(javac --version 2>/dev/null | head -n1)"
  echo "${ver:-unknown}"
else
  echo "unknown"
fi
