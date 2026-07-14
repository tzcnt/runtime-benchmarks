#!/usr/bin/env bash
# Prints the Kotlin compiler (kotlinc) version string, e.g.
# "info: kotlinc-jvm 2.4.0 (JRE 25.0.4-ea+4-1-Debian)". kotlinc writes its
# version banner to stderr, so redirect it to stdout before capturing.
if command -v kotlinc >/dev/null 2>&1; then
  # kotlinc writes its version banner to stderr; grep the kotlin line so JVM
  # launcher notes ("Picked up JAVA_TOOL_OPTIONS: ...") can't shadow it.
  ver="$(kotlinc -version 2>&1 | grep -i 'kotlin' | head -n1)"
  echo "${ver:-unknown}"
else
  echo "unknown"
fi
