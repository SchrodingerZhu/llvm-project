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
/// GPU benchmark runner.
///
//===----------------------------------------------------------------------===//

#include "LibcGpuBenchmark.h"

#include "hdr/stdint_proxy.h"
#include "src/__support/CPP/algorithm.h"
#include "src/__support/CPP/atomic.h"
#include "src/__support/CPP/string.h"
#include "src/__support/FPUtil/FPBits.h"
#include "src/__support/FPUtil/sqrt.h"
#include "src/__support/GPU/utils.h"
#include "src/__support/macros/config.h"
#include "src/stdio/printf.h"

namespace LIBC_NAMESPACE_DECL {
namespace benchmarks {

static void atomic_add_double(cpp::Atomic<uint64_t> &atomic_bits,
                              double value) {
  using FPBits = LIBC_NAMESPACE::fputil::FPBits<double>;

  uint64_t expected_bits = atomic_bits.load(cpp::MemoryOrder::RELAXED);

  while (true) {
    double current_value = FPBits(expected_bits).get_val();
    double next_value = current_value + value;

    uint64_t desired_bits = FPBits(next_value).uintval();
    if (atomic_bits.compare_exchange_strong(expected_bits, desired_bits,
                                            cpp::MemoryOrder::ACQUIRE,
                                            cpp::MemoryOrder::RELAXED))
      break;
  }
}

struct AtomicBenchmarkSums {
  cpp::Atomic<uint32_t> active_threads = 0;
  cpp::Atomic<uint64_t> iterations_sum = 0;
  cpp::Atomic<uint64_t> weighted_cycles_sum_bits = 0;
  cpp::Atomic<uint64_t> weighted_squared_cycles_sum_bits = 0;
  cpp::Atomic<uint64_t> min = UINT64_MAX;
  cpp::Atomic<uint64_t> max = 0;

  void reset() {
    cpp::atomic_thread_fence(cpp::MemoryOrder::RELEASE);
    active_threads.store(0, cpp::MemoryOrder::RELAXED);
    iterations_sum.store(0, cpp::MemoryOrder::RELAXED);
    weighted_cycles_sum_bits.store(0, cpp::MemoryOrder::RELAXED);
    weighted_squared_cycles_sum_bits.store(0, cpp::MemoryOrder::RELAXED);
    min.store(UINT64_MAX, cpp::MemoryOrder::RELAXED);
    max.store(0, cpp::MemoryOrder::RELAXED);
    cpp::atomic_thread_fence(cpp::MemoryOrder::RELEASE);
  }

  void update(const BenchmarkResult &result) {
    cpp::atomic_thread_fence(cpp::MemoryOrder::RELEASE);
    active_threads.fetch_add(1, cpp::MemoryOrder::RELAXED);
    iterations_sum.fetch_add(result.total_iterations,
                             cpp::MemoryOrder::RELAXED);

    const double n_i = static_cast<double>(result.total_iterations);
    const double mean_i = result.cycles;
    const double stddev_i = result.standard_deviation;
    const double variance_i = stddev_i * stddev_i;
    atomic_add_double(weighted_cycles_sum_bits, n_i * mean_i);
    atomic_add_double(weighted_squared_cycles_sum_bits,
                      n_i * (variance_i + mean_i * mean_i));

    // Perform a CAS loop to atomically update the min
    uint64_t orig_min = min.load(cpp::MemoryOrder::RELAXED);
    while (!min.compare_exchange_strong(
        orig_min, cpp::min(orig_min, result.min), cpp::MemoryOrder::ACQUIRE,
        cpp::MemoryOrder::RELAXED))
      ;

    // Perform a CAS loop to atomically update the max
    uint64_t orig_max = max.load(cpp::MemoryOrder::RELAXED);
    while (!max.compare_exchange_strong(
        orig_max, cpp::max(orig_max, result.max), cpp::MemoryOrder::ACQUIRE,
        cpp::MemoryOrder::RELAXED))
      ;

    cpp::atomic_thread_fence(cpp::MemoryOrder::RELEASE);
  }
};

AtomicBenchmarkSums all_results;
constexpr auto GREEN = "\033[32m";
constexpr auto RESET = "\033[0m";

void print_results(Benchmark *b) {
  using FPBits = LIBC_NAMESPACE::fputil::FPBits<double>;

  BenchmarkResult final_result;
  cpp::atomic_thread_fence(cpp::MemoryOrder::RELEASE);

  const uint32_t num_threads =
      all_results.active_threads.load(cpp::MemoryOrder::RELAXED);
  final_result.total_iterations =
      all_results.iterations_sum.load(cpp::MemoryOrder::RELAXED);

  if (final_result.total_iterations > 0) {
    const uint64_t s1_bits =
        all_results.weighted_cycles_sum_bits.load(cpp::MemoryOrder::RELAXED);
    const uint64_t s2_bits = all_results.weighted_squared_cycles_sum_bits.load(
        cpp::MemoryOrder::RELAXED);

    const double S1 = FPBits(s1_bits).get_val();
    const double S2 = FPBits(s2_bits).get_val();
    const double N = static_cast<double>(final_result.total_iterations);

    const double global_mean = S1 / N;
    const double global_mean_of_squares = S2 / N;
    const double global_variance =
        global_mean_of_squares - (global_mean * global_mean);

    final_result.cycles = global_mean;
    final_result.standard_deviation =
        fputil::sqrt<double>(global_variance < 0.0 ? 0.0 : global_variance);
  } else {
    final_result.cycles = 0.0;
    final_result.standard_deviation = 0.0;
  }

  final_result.min = all_results.min.load(cpp::MemoryOrder::RELAXED);
  final_result.max = all_results.max.load(cpp::MemoryOrder::RELAXED);
  cpp::atomic_thread_fence(cpp::MemoryOrder::RELEASE);

  LIBC_NAMESPACE::printf(
      "%-24s |%15.0f |%9.0f |%8llu |%8llu |%15llu |%9u |\n",
      b->get_test_name().data(), final_result.cycles,
      final_result.standard_deviation,
      static_cast<unsigned long long>(final_result.min),
      static_cast<unsigned long long>(final_result.max),
      static_cast<unsigned long long>(final_result.total_iterations),
      static_cast<unsigned>(num_threads));
}

void print_header(const Benchmark *b) {
  LIBC_NAMESPACE::printf("%s", GREEN);
  LIBC_NAMESPACE::printf("Running Suite: %-10s\n",
                         b->get_suite_name().data());
  LIBC_NAMESPACE::printf("%s", RESET);
  cpp::string titles = "Benchmark                |  Cycles (Mean) |   Stddev | "
                       "    Min |     Max |     Iterations |  Threads |\n";
  LIBC_NAMESPACE::printf(titles.data());

  cpp::string separator(titles.size(), '-');
  separator[titles.size() - 1] = '\n';
  LIBC_NAMESPACE::printf(separator.data());
}

void Benchmark::run_benchmarks() {
  uint64_t id = gpu::get_thread_id();

  if (id == 0)
    print_header(benchmarks[0]);

  gpu::sync_threads();

  for (Benchmark *b : benchmarks) {
    if (id == 0)
      all_results.reset();

    gpu::sync_threads();
    if (b->num_threads == static_cast<uint32_t>(-1) || id < b->num_threads) {
      auto current_result = b->run();
      all_results.update(current_result);
    }
    gpu::sync_threads();

    if (id == 0)
      print_results(b);
  }
  gpu::sync_threads();
}

} // namespace benchmarks
} // namespace LIBC_NAMESPACE_DECL
