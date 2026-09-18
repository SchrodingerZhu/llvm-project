//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Latency benchmark for isalpha.
///
//===----------------------------------------------------------------------===//

#include "benchmarks/Benchmark.h"
#include "benchmarks/timing/timing.h"

#include "src/ctype/isalpha.h"

uint64_t BM_IsAlpha() {
  char x = 'c';
  return LIBC_NAMESPACE::latency(LIBC_NAMESPACE::isalpha, x);
}
BENCHMARK(LlvmLibcIsAlphaBenchmark, IsAlpha, BM_IsAlpha);
