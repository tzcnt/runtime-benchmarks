// Get system topology and use it to get the threads sweep.

// Original author: tzcnt
// Unlicense License
// This is free and unencumbered software released into the public domain.

#define TMC_IMPL

#include "tmc/all_headers.hpp"

#include <cstdio>
#include <cstdlib>
#include <vector>
#include <algorithm>


// Generates thread counts to benchmark based on the target hardware.
// Starts with a doubling progression [1,2,4, ..., topo.core_count()]
// Also inserts values at for each different type of core without SMT, and the max with SMT
// e.g. on a 13600k inserts:
// 6 (number of P-cores)
// 14 (number of P + E-cores)
// 20 (6*2 for P-cores SMT + 8 for E-cores without)

int main(int argc, char* argv[]) {
  // Get the breakpoints (number of physical cores of different kinds, and max with SMT)
  auto topo = tmc::topology::query();
  std::vector<size_t> breakpoints;
  {
    size_t count = 0;
    for (auto c : topo.cpu_kind_counts) {
      auto n = count + c;
      breakpoints.push_back(n);
      count += c;
    }
  }
  auto puCount = topo.pu_count();
  if (breakpoints.back() != puCount) {
    breakpoints.push_back(puCount);
  }
  
  // Calculate the doubling progression
  std::vector<size_t> doubling{};
  for (size_t count = 1; count <= topo.core_count(); count *= 2) {
    doubling.push_back(count);
  }

  // Merge two lists together
  std::vector<size_t> merged{};
  std::set_union(doubling.begin(), doubling.end(), breakpoints.begin(), breakpoints.end(), std::back_inserter(merged));
  
  std::printf("[%zu", merged[0]);
  for (size_t i = 1; i < merged.size(); ++i) {
    if (merged[i] > 64) {
      break;
    }
    std::printf(",%zu", merged[i]);
  }
  std::printf("]\n");
}
