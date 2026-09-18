//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
///
/// \file
/// Freestanding benchmark entrypoint.
///
//===----------------------------------------------------------------------===//

#if __STDC_HOSTED__
#error "BenchmarkMain.cpp requires a freestanding build"
#else

#include "benchmarks/Benchmark.h"

// extern "C" main is UB in hosted environment
extern "C" int main() {
  LIBC_NAMESPACE::benchmarks::Benchmark::run_benchmarks();
  return 0;
}

#endif
