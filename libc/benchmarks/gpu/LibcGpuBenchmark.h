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
/// GPU benchmark interface and math helpers.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_BENCHMARKS_LIBC_GPU_BENCHMARK_H
#define LLVM_LIBC_BENCHMARKS_LIBC_GPU_BENCHMARK_H

#include "benchmarks/Benchmark.h"
#include "benchmarks/gpu/Random.h"

#include "benchmarks/gpu/timing/timing.h"

#include "hdr/stdint_proxy.h"
#include "src/__support/CPP/algorithm.h"
#include "src/__support/CPP/array.h"
#include "src/__support/CPP/string_view.h"
#include "src/__support/CPP/type_traits.h"
#include "src/__support/FPUtil/FPBits.h"
#include "src/__support/macros/config.h"

namespace LIBC_NAMESPACE_DECL {

namespace benchmarks {

class Benchmark {
  const BenchmarkTarget target;
  const cpp::string_view suite_name;
  const cpp::string_view test_name;
  const uint32_t num_threads;

public:
  Benchmark(uint64_t (*f)(), const char *suite, const char *test,
            uint32_t threads)
      : target(BenchmarkTarget(f)), suite_name(suite), test_name(test),
        num_threads(threads) {
    add_benchmark(this);
  }

  Benchmark(uint64_t (*f)(uint32_t), char const *suite_name,
            char const *test_name, uint32_t num_threads)
      : target(BenchmarkTarget(f)), suite_name(suite_name),
        test_name(test_name), num_threads(num_threads) {
    add_benchmark(this);
  }

  static void run_benchmarks();
  const cpp::string_view get_suite_name() const { return suite_name; }
  const cpp::string_view get_test_name() const { return test_name; }

protected:
  static void add_benchmark(Benchmark *benchmark);

private:
  BenchmarkResult run() {
    BenchmarkOptions options;
    return benchmark(options, target);
  }
};

template <typename T> class MathPerf {
  static LIBC_INLINE uint64_t make_seed(uint64_t base_seed, uint64_t salt) {
    const uint64_t tid = gpu::get_thread_id();
    return base_seed ^ (salt << 32) ^ (tid * 0x9E3779B97F4A7C15ULL);
  }

public:
  // Returns cycles-per-call (lower is better)
  template <size_t N = 1, typename Dist>
  static uint64_t run_throughput(T (*f)(T), const Dist &dist,
                                 uint32_t call_index) {
    cpp::array<T, N> inputs;

    uint64_t base_seed = static_cast<uint64_t>(call_index);
    uint64_t salt = static_cast<uint64_t>(N);
    RandomGenerator rng(make_seed(base_seed, salt));

    for (size_t i = 0; i < N; ++i)
      inputs[i] = dist(rng);

    uint64_t total_time = LIBC_NAMESPACE::throughput(f, inputs);

    return total_time / N;
  }

  // Returns cycles-per-call (lower is better)
  template <size_t N = 1, typename Dist1, typename Dist2>
  static uint64_t run_throughput(T (*f)(T, T), const Dist1 &dist1,
                                 const Dist2 &dist2, uint32_t call_index) {
    cpp::array<T, N> inputs1;
    cpp::array<T, N> inputs2;

    uint64_t base_seed = static_cast<uint64_t>(call_index);
    uint64_t salt = static_cast<uint64_t>(N);
    RandomGenerator rng(make_seed(base_seed, salt));

    for (size_t i = 0; i < N; ++i) {
      inputs1[i] = dist1(rng);
      inputs2[i] = dist2(rng);
    }

    uint64_t total_time = LIBC_NAMESPACE::throughput(f, inputs1, inputs2);

    return total_time / N;
  }
};

} // namespace benchmarks
} // namespace LIBC_NAMESPACE_DECL

// Passing -1 indicates the benchmark should be run with as many threads as
// allocated by the user in the benchmark's CMake.
#define BENCHMARK(SuiteName, TestName, Func)                                   \
  LIBC_NAMESPACE::benchmarks::Benchmark SuiteName##_##TestName##_Instance(     \
      Func, #SuiteName, #TestName, -1)

#define BENCHMARK_N_THREADS(SuiteName, TestName, Func, NumThreads)             \
  LIBC_NAMESPACE::benchmarks::Benchmark SuiteName##_##TestName##_Instance(     \
      Func, #SuiteName, #TestName, NumThreads)

#define SINGLE_THREADED_BENCHMARK(SuiteName, TestName, Func)                   \
  BENCHMARK_N_THREADS(SuiteName, TestName, Func, 1)

#define SINGLE_WAVE_BENCHMARK(SuiteName, TestName, Func)                       \
  BENCHMARK_N_THREADS(SuiteName, TestName, Func,                               \
                      LIBC_NAMESPACE::gpu::get_lane_size())

#endif // LLVM_LIBC_BENCHMARKS_LIBC_GPU_BENCHMARK_H
