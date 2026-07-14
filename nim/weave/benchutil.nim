# Helpers shared across the Nim / Weave benchmark binaries so their setup and
# reported numbers line up with the C++ (TMC), Rust (tokio), and Go suites.

import std/[cpuinfo, os, strutils]
import std/posix

proc peakMemoryUsageKiB*(): int64 =
  ## Peak resident set size of the current process in KiB, mirroring
  ## cpp/2common/memusage.hpp so the reported numbers are directly comparable
  ## across languages.
  var ru: Rusage
  if getrusage(RUSAGE_SELF, ru.addr) != 0:
    return -1
  # On Linux ru_maxrss is already in KiB; on macOS/BSD it is in bytes.
  when defined(macosx):
    int64(ru.ru_maxrss) div 1024
  else:
    int64(ru.ru_maxrss)

proc defaultThreads*(): int =
  ## Mirrors the C++/Rust default worker count of hardware_concurrency() / 2.
  result = countProcessors() div 2
  if result < 1:
    result = 1

proc threadCountArg*(idx: int): int =
  ## Parses paramStr(idx) as a worker-thread count, falling back to
  ## defaultThreads() when it is absent or unparseable. The benchmark harness
  ## passes the thread count in this position (see build_and_bench_all.py).
  result = defaultThreads()
  if paramCount() >= idx:
    try:
      let v = parseInt(paramStr(idx))
      if v > 0:
        result = v
    except ValueError:
      discard

proc setWeaveThreads*(n: int) =
  ## Weave sizes its worker pool from WEAVE_NUM_THREADS when init(Weave) runs,
  ## so this must be called before init.
  putEnv("WEAVE_NUM_THREADS", $n)
