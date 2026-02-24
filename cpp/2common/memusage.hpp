#pragma once
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#include <psapi.h>
#else
#include <sys/resource.h>
#include <sys/param.h>
#endif

/**
 * Returns peak memory usage in KiB
 */
static inline long peak_memory_usage() {
#if defined(_WIN32)
  PROCESS_MEMORY_COUNTERS pmc;
  if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) {
    return pmc.PeakWorkingSetSize / 1024;
  }
#else // Linux/BSD
  rusage usage{};
  if (getrusage(RUSAGE_SELF, &usage) == 0) {
    // BSD based systems return maxrss in bytes,
    // on Linux it is in kilobytes
#if defined(BSD) || defined(__FreeBSD__) || defined(__APPLE__)
    return usage.ru_maxrss / 1024;
#else
    return usage.ru_maxrss;
#endif
  }
#endif
  return -1;
}