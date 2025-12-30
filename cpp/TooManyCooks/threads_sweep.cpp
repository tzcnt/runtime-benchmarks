// Get system topology and use it to get the threads sweep.

// Original author: tzcnt
// Unlicense License
// This is free and unencumbered software released into the public domain.

#define TMC_IMPL

#include "tmc/all_headers.hpp"

#include <cstdio>
#include <cstdlib>
#include <vector>

int main(int argc, char* argv[]) {
  auto topo = tmc::topology::query();
  std::vector<size_t> breakpoints;
  size_t max = topo.pu_count();
  if (max > 64) {
    max = 64;
  }
  {
    size_t count = 0;
    for (auto c : topo.cpu_kind_counts) {
      auto n = count + c;
      if (n >= max) {
        breakpoints.push_back(max);
        break;
      }
      breakpoints.push_back(n);
      count += c;
    }
  }
  if (breakpoints.back() != max) {
    breakpoints.push_back(max);
  }
  std::printf("[1");
  size_t idx = 0;
  size_t count = 2;
  while (count < max) {
    while (count > breakpoints[idx]) {
      std::printf(",%zu", breakpoints[idx]);
      ++idx;
    }
    std::printf(",%zu", count);
    count *= 2;
  }
  std::printf(",%zu]\n", max);
}
