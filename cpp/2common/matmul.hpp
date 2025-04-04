#pragma once

static inline void matmul_small(int* a, int* b, int* c, int n, int N) {
  for (int i = 0; i < n; i++) {
    for (int k = 0; k < n; k++) {
      for (int j = 0; j < n; j++) {
        c[i * N + j] += a[i * N + k] * b[k * N + j];
      }
    }
  }
}