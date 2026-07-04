#pragma once

// Shared configuration for the Unbalanced Tree Search (UTS) benchmark.
//
// The tree-generation code itself (uts/external/*) is plain C from the UTS
// reference distribution and is runtime-agnostic. This header adds the small
// amount of C++ glue (the result type and the tree presets) that is likewise
// shared by all runtime implementations. Each runtime only provides its own
// traversal coroutine and main().
//
// Adapted from libfork's UTS benchmark config:
// https://github.com/ConorWilliams/libfork/blob/f91d6ef23c74069b6591128ed7736d6feb73b8e7/bench/source/uts/config.hpp

#include <compare>
#include <cstdlib>
#include <iostream>

#include "uts/external/uts.h"

// The tree to search: a geometric [fixed] tree based on UTS preset T1, but with
// the depth increased by 1 (d=11 instead of the standard T1's d=10) to give a
// longer-running, ~4x-larger tree. Standard T1 was:
//   Tree size = 4130071, tree depth = 10, num leaves = 3305118 (80.03%)
//   export T1="-t 1 -a 3 -d 10 -b 4 -r 19"
inline constexpr int tree_id = 11;

struct result {
  counter_t maxdepth, size, leaves;
  auto operator<=>(const result&) const = default;
};

inline void reset_uts() {
  type = GEO;                // t
  b_0 = 4.0;                 // b
  rootId = 0;                // r
  nonLeafBF = 4;             // m
  nonLeafProb = 15.0 / 64.0; // q
  gen_mx = 6;                // d
  shape_fn = LINEAR;         // a
  shiftDepth = 0.5;          // f
  computeGranularity = 1;    // g
  debug = 0;                 // x
  verbose = 1;               // v
}

// (T1) Geometric [fixed], depth bumped from 10 to 11
inline void setup_tree(int i) {
  if (i != 11) {
    std::cerr << "Invalid tree id" << std::endl;
    std::exit(1);
  }
  reset_uts();
  type = (tree_t)1;
  shape_fn = (geoshape_t)3;
  gen_mx = 11;
  b_0 = 4;
  rootId = 19;
}

inline result result_tree(int i) {
  if (i != 11) {
    std::cerr << "Invalid tree id" << std::endl;
    std::exit(1);
  }
  // Geometric [fixed] tree, b=4, r=19, d=11 (standard T1 with depth +1).
  return {11, 16526523, 13221220};
}
