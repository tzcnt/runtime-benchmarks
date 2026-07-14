// Shared helpers for the C benchmark binaries, mirroring
// cpp/2common/memusage.hpp so the reported numbers line up across languages.
#ifndef BENCH_UTIL_H
#define BENCH_UTIL_H

#include <stdint.h>
#include <sys/param.h>
#include <sys/resource.h>
#include <time.h>

// Returns peak memory usage in KiB
static inline long peak_memory_usage(void) {
  struct rusage usage;
  if (getrusage(RUSAGE_SELF, &usage) == 0) {
    // BSD based systems return maxrss in bytes,
    // on Linux it is in kilobytes
#if defined(BSD) || defined(__FreeBSD__) || defined(__APPLE__)
    return usage.ru_maxrss / 1024;
#else
    return usage.ru_maxrss;
#endif
  }
  return -1;
}

// Monotonic clock reading in microseconds.
static inline int64_t bench_now_us(void) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (int64_t)ts.tv_sec * 1000000 + ts.tv_nsec / 1000;
}

#endif // BENCH_UTIL_H
