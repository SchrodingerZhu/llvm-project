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
/// Freestanding benchmark sampling and registration.
///
//===----------------------------------------------------------------------===//

#include "benchmarks/Benchmark.h"
#include "benchmarks/RuntimeEstimator.h"
#include "hdr/time_macros.h"
#include "src/__support/CPP/algorithm.h"
#include "src/__support/FPUtil/NearestIntegerOperations.h"
#include "src/time/clock.h"

namespace LIBC_NAMESPACE_DECL {
namespace benchmarks {

BenchmarkResult benchmark(const BenchmarkOptions &options,
                          const BenchmarkTarget &target) {
  BenchmarkResult result;
  RuntimeEstimationProgression rep;
  uint32_t iterations = options.initial_iterations;

  if (iterations < 1u)
    iterations = 1;

  uint32_t samples = 0;
  uint64_t total_time = 0;
  uint64_t min = UINT64_MAX;
  uint64_t max = 0;

  uint32_t call_index = 0;

  for (int64_t time_budget = options.max_duration; time_budget >= 0;) {
    RefinableRuntimeEstimator sample_estimator;

    const clock_t start = clock();
    while (sample_estimator.get_iterations() < iterations) {
      auto current_result = target(call_index++);
      max = cpp::max(max, current_result);
      min = cpp::min(min, current_result);
      sample_estimator.update(current_result);
    }
    const clock_t end = clock();

    const int64_t duration_ns =
        ((static_cast<int64_t>(end) - start) * 1'000'000'000) / CLOCKS_PER_SEC;
    total_time += duration_ns;
    time_budget -= duration_ns;
    samples++;

    const double change_ratio = rep.compute_improvement(sample_estimator);

    if (samples >= options.max_samples || iterations >= options.max_iterations)
      break;

    const auto total_iterations = rep.get_estimator().get_iterations();

    if (total_time >= options.min_duration && samples >= options.min_samples &&
        total_iterations >= options.min_iterations &&
        change_ratio < options.epsilon)
      break;

    iterations = static_cast<uint32_t>(
        fputil::ceil(iterations * options.scaling_factor));
  }

  const auto &estimator = rep.get_estimator();
  result.total_iterations = estimator.get_iterations();
  result.cycles = estimator.get_mean();
  result.standard_deviation = estimator.get_stddev();
  result.min = min;
  result.max = max;

  return result;
}

} // namespace benchmarks
} // namespace LIBC_NAMESPACE_DECL
