/**
 * @file sieve.hpp
 *
 * @section LICENSE
 *
 * The MIT License
 *
 * @copyright Copyright (c) 2022 TileDB, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 * @section DESCRIPTION
 *
 * Demo program sieve of Eratosthenes, sequential implementation
 */

#ifndef TILEDB_SIEVE_HPP
#define TILEDB_SIEVE_HPP

#include <cmath>
#include <future>
#include <iostream>
#include <vector>

template <class bool_t>
auto sieve_to_primes(std::vector<bool_t>& sieve) {
  std::vector<size_t> primes;

  for (size_t i = 2; i < sieve.size(); ++i) {
    if (sieve[i]) {
      primes.push_back(i);
    }
  }
  return primes;
}

template <class bool_t>
auto sieve_to_primes(std::vector<bool_t>& sieve, std::vector<size_t>& base_primes, size_t sqrt_n) {
  std::vector<size_t> primes(base_primes);

  for (size_t i = sqrt_n; i < sieve.size(); ++i) {
    if (sieve[i]) {
      primes.push_back(i);
    }
  }
  return primes;
}

template <class bool_t>
auto sieve_seq(size_t n) {
  std::vector<bool_t> sieve(n, true);

  sieve[0] = sieve[1] = true;

  size_t sqrt_n = static_cast<size_t>(std::ceil(std::sqrt(n)));

  for (size_t i = 2; i < sqrt_n; ++i) {
    if (sieve[i]) {
      for (size_t j = i * i; j < n; j += i) {
        sieve[j] = false;
      }
    }
  }

  return sieve;
}

#endif  // TILEDB_SIEVE_HPP
