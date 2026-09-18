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
/// Baremetal benchmark runner.
///
//===----------------------------------------------------------------------===//

#include "benchmarks/Benchmark.h"
#include "src/stdio/printf.h"

namespace LIBC_NAMESPACE_DECL {
namespace benchmarks {

void Benchmark::run_benchmarks() {
  const Benchmark *previous = nullptr;
  for (Benchmark *b : benchmarks) {
    // Baremetal runs on one thread, so only a zero thread count excludes it.
    if (b->num_threads == 0)
      continue;

    if (!previous || previous->suite_name != b->suite_name) {
      LIBC_NAMESPACE::printf("Running Suite: %s\n", b->suite_name.data());
      LIBC_NAMESPACE::printf(
          "Benchmark                |  Cycles (Mean) |   Stddev | "
          "    Min |     Max |     Iterations |\n");
    }
    previous = b;

    const BenchmarkResult result = b->run();
    LIBC_NAMESPACE::printf(
        "%-24s |%15.0f |%9.0f |%8llu |%8llu |%15llu |\n", b->test_name.data(),
        result.cycles, result.standard_deviation,
        static_cast<unsigned long long>(result.min),
        static_cast<unsigned long long>(result.max),
        static_cast<unsigned long long>(result.total_iterations));
  }
}

} // namespace benchmarks
} // namespace LIBC_NAMESPACE_DECL
