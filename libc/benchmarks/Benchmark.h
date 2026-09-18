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
/// Freestanding benchmark interface and registration.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_BENCHMARKS_BENCHMARK_H
#define LLVM_LIBC_BENCHMARKS_BENCHMARK_H

#include "hdr/stdint_proxy.h"
#include "src/__support/CPP/string_view.h"
#include "src/__support/fixedvector.h"
#include "src/__support/macros/attributes.h"
#include "src/__support/macros/config.h"

namespace LIBC_NAMESPACE_DECL {
namespace benchmarks {

struct BenchmarkOptions {
  uint32_t initial_iterations = 1;
  uint32_t min_iterations = 1;
  uint32_t max_iterations = 10000000;
  uint32_t min_samples = 4;
  uint32_t max_samples = 1000;
  int64_t min_duration = 500 * 1000;         // 500 * 1000 nanoseconds = 500 us
  int64_t max_duration = 1000 * 1000 * 1000; // 1e9 nanoseconds = 1 second
  double epsilon = 0.0001;
  double scaling_factor = 1.4;
};

struct BenchmarkResult {
  uint64_t total_iterations = 0;
  double cycles = 0;
  double standard_deviation = 0;
  uint64_t min = UINT64_MAX;
  uint64_t max = 0;
};

struct BenchmarkTarget {
  using IndexedFnPtr = uint64_t (*)(uint32_t);
  using IndexlessFnPtr = uint64_t (*)();

  enum class Kind : uint8_t { Indexed, Indexless } kind;
  union {
    IndexedFnPtr indexed_fn_ptr;
    IndexlessFnPtr indexless_fn_ptr;
  };

  LIBC_INLINE BenchmarkTarget(IndexedFnPtr func)
      : kind(Kind::Indexed), indexed_fn_ptr(func) {}
  LIBC_INLINE BenchmarkTarget(IndexlessFnPtr func)
      : kind(Kind::Indexless), indexless_fn_ptr(func) {}

  LIBC_INLINE uint64_t operator()([[maybe_unused]] uint32_t call_index) const {
    return kind == Kind::Indexed ? indexed_fn_ptr(call_index)
                                 : indexless_fn_ptr();
  }
};

BenchmarkResult benchmark(const BenchmarkOptions &options,
                          const BenchmarkTarget &target);

class Benchmark {
  static FixedVector<Benchmark *, 64> benchmarks;
  const BenchmarkTarget target;
  const cpp::string_view suite_name;
  const cpp::string_view test_name;
  const uint32_t num_threads;
  const BenchmarkOptions options;

public:
  Benchmark(uint64_t (*f)(), const char *suite, const char *test,
            uint32_t threads, BenchmarkOptions options = {})
      : target(BenchmarkTarget(f)), suite_name(suite), test_name(test),
        num_threads(threads), options(options) {
    add_benchmark(this);
  }

  Benchmark(uint64_t (*f)(uint32_t), char const *suite_name,
            char const *test_name, uint32_t num_threads,
            BenchmarkOptions options = {})
      : target(BenchmarkTarget(f)), suite_name(suite_name),
        test_name(test_name), num_threads(num_threads), options(options) {
    add_benchmark(this);
  }

  static void run_benchmarks();
  const cpp::string_view get_suite_name() const { return suite_name; }
  const cpp::string_view get_test_name() const { return test_name; }

protected:
  static void add_benchmark(Benchmark *benchmark);

private:
  BenchmarkResult run() { return benchmark(options, target); }
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

#endif // LLVM_LIBC_BENCHMARKS_BENCHMARK_H
