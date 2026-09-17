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
/// Benchmark runtime estimation.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_BENCHMARKS_RUNTIME_ESTIMATOR_H
#define LLVM_LIBC_BENCHMARKS_RUNTIME_ESTIMATOR_H

#include "hdr/stdint_proxy.h"
#include "src/__support/FPUtil/sqrt.h"
#include "src/__support/macros/config.h"

namespace LIBC_NAMESPACE_DECL {
namespace benchmarks {

class RefinableRuntimeEstimator {
  uint32_t iterations = 0;
  uint64_t sum_of_cycles = 0;
  uint64_t sum_of_squared_cycles = 0;

public:
  void update(uint64_t cycles) noexcept {
    iterations += 1;
    sum_of_cycles += cycles;
    sum_of_squared_cycles += cycles * cycles;
  }

  void update(const RefinableRuntimeEstimator &other) noexcept {
    iterations += other.iterations;
    sum_of_cycles += other.sum_of_cycles;
    sum_of_squared_cycles += other.sum_of_squared_cycles;
  }

  double get_mean() const noexcept {
    if (iterations == 0)
      return 0.0;

    return static_cast<double>(sum_of_cycles) / iterations;
  }

  double get_variance() const noexcept {
    if (iterations == 0)
      return 0.0;

    const double num = static_cast<double>(iterations);
    const double sum_x = static_cast<double>(sum_of_cycles);
    const double sum_x2 = static_cast<double>(sum_of_squared_cycles);

    const double mean_of_squares = sum_x2 / num;
    const double mean = sum_x / num;
    const double mean_squared = mean * mean;
    const double variance = mean_of_squares - mean_squared;

    return variance < 0.0 ? 0.0 : variance;
  }

  double get_stddev() const noexcept {
    return fputil::sqrt<double>(get_variance());
  }

  uint32_t get_iterations() const noexcept { return iterations; }
};

// Tracks the progression of the runtime estimation
class RuntimeEstimationProgression {
  RefinableRuntimeEstimator estimator;
  double current_mean = 0.0;

public:
  const RefinableRuntimeEstimator &get_estimator() const noexcept {
    return estimator;
  }

  double
  compute_improvement(const RefinableRuntimeEstimator &sample_estimator) {
    if (sample_estimator.get_iterations() == 0)
      return 1.0;

    estimator.update(sample_estimator);

    const double new_mean = estimator.get_mean();
    if (current_mean == 0.0 || new_mean == 0.0) {
      current_mean = new_mean;
      return 1.0;
    }

    double ratio = (current_mean / new_mean) - 1.0;
    if (ratio < 0)
      ratio = -ratio;

    current_mean = new_mean;
    return ratio;
  }
};

} // namespace benchmarks
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_BENCHMARKS_RUNTIME_ESTIMATOR_H
