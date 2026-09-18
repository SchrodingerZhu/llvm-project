//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Buffer storage and validation for freestanding string benchmarks.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_BENCHMARKS_SRC_STRING_STRINGBENCHMARK_H
#define LLVM_LIBC_BENCHMARKS_SRC_STRING_STRINGBENCHMARK_H

#include "benchmarks/Benchmark.h"
#include "benchmarks/timing/timing.h"
#include "src/stdio/printf.h"
#include "src/stdlib/aligned_alloc.h"
#include "src/stdlib/exit.h"

namespace LIBC_NAMESPACE_DECL {
namespace benchmarks {
namespace string_benchmarks {

constexpr size_t MAX_SIZE = 4096;
constexpr size_t ALIGNMENT = 64;
constexpr size_t BUFFER_SIZE = MAX_SIZE + 3 * ALIGNMENT;
constexpr size_t SAMPLE_COUNT = 1024;

inline void check(bool condition) {
  if (!condition) {
    LIBC_NAMESPACE::printf("String benchmark validation failed\n");
    LIBC_NAMESPACE::exit(1);
  }
}

// Allocating through libc lets the board place operands in its normal heap,
// independently of the stack and code placement. Retain two bounded buffers
// for the executable's lifetime, shared by its single-threaded benchmark cases.
inline unsigned char *storage() {
  static unsigned char *buffer = nullptr;
  if (!buffer)
    buffer = static_cast<unsigned char *>(
        LIBC_NAMESPACE::aligned_alloc(ALIGNMENT, 2 * BUFFER_SIZE));
  check(buffer != nullptr);
  return buffer;
}

inline unsigned char *left() { return storage() + ALIGNMENT; }
inline unsigned char *right() { return left() + BUFFER_SIZE; }

inline void reset() {
  for (size_t i = 0; i < 2 * BUFFER_SIZE; ++i)
    storage()[i] = static_cast<unsigned char>(1 + i % 251);
}

inline BenchmarkOptions options() {
  BenchmarkOptions result;
  // Cover the entire parameter sequence in the first sample, even if setup
  // exhausts the time budget on a slower target.
  result.initial_iterations = SAMPLE_COUNT;
  return result;
}

inline uint32_t random(uint32_t &state) {
  state ^= state << 13;
  state ^= state >> 17;
  state ^= state << 5;
  return state;
}

struct Parameters {
  uint16_t size;
  uint16_t dst_offset;
  uint16_t src_offset;
};

} // namespace string_benchmarks
} // namespace benchmarks
} // namespace LIBC_NAMESPACE_DECL

#endif
