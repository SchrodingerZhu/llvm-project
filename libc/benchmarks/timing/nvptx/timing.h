//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

///
/// \file
/// NVPTX benchmark timing utilities.
///
//===----------------------------------------------------------------------===//
#ifndef LLVM_LIBC_BENCHMARKS_TIMING_NVPTX
#define LLVM_LIBC_BENCHMARKS_TIMING_NVPTX

#include "hdr/stdint_proxy.h"
#include "src/__support/CPP/algorithm.h"
#include "src/__support/CPP/array.h"
#include "src/__support/CPP/atomic.h"
#include "src/__support/CPP/type_traits.h"
#include "src/__support/GPU/utils.h"
#include "src/__support/macros/attributes.h"
#include "src/__support/macros/config.h"

namespace LIBC_NAMESPACE_DECL {

// Returns the overhead associated with calling the profiling region. This
// allows us to substract the constant-time overhead from the latency to
// obtain a true result. This can vary with system load.
[[gnu::noinline]] static uint64_t overhead() {
  volatile uint32_t x = 1;
  uint32_t y = x;
  uint64_t start = gpu::processor_clock();
  asm("" ::"llr"(start));
  uint32_t result = y;
  asm("or.b32 %[v_reg], %[v_reg], 0;" ::[v_reg] "r"(result));
  uint64_t stop = gpu::processor_clock();
  volatile auto storage = result;
  return stop - start;
}

// Measure a callable, including the barriers needed to complete memory
// accesses. Capture inputs in the callable so they are loaded after the start
// timestamp.
template <typename F>
[[gnu::noinline]] static LIBC_INLINE uint64_t latency(F &&f) {
  auto *callable = __builtin_addressof(f);
  cpp::atomic_thread_fence(cpp::MemoryOrder::ACQ_REL);
  const uint64_t start = gpu::processor_clock();
  // Hide the callable's address to prevent folding or hoisting its inputs.
  asm volatile("" : "+l"(callable) : "l"(start) : "memory");
  if constexpr (cpp::is_void_v<decltype(static_cast<F &&>(*callable)())>) {
    static_cast<F &&>(*callable)();
  } else {
    decltype(auto) result = static_cast<F &&>(*callable)();
    // Escape the result through memory to support any return type.
    asm volatile("" : : "l"(__builtin_addressof(result)) : "memory");
  }
  cpp::atomic_thread_fence(cpp::MemoryOrder::ACQ_REL);
  const uint64_t stop = gpu::processor_clock();
  return stop - start;
}

// Provides the *baseline* for throughput: measures loop and measurement costs
// without calling the f function
template <typename T, size_t N>
static LIBC_INLINE uint64_t
throughput_baseline(const cpp::array<T, N> &inputs) {
  asm("" ::"r"(&inputs));

  cpp::atomic_thread_fence(cpp::MemoryOrder::ACQ_REL);
  uint64_t start = gpu::processor_clock();
  asm("" ::"llr"(start));

  T result{};

#pragma clang loop unroll(disable)
  for (auto input : inputs) {
    asm("" ::"r"(input));
    result = input;
    asm("" ::"r"(result));
  }

  uint64_t stop = gpu::processor_clock();
  asm("" ::"r"(stop));
  cpp::atomic_thread_fence(cpp::MemoryOrder::ACQ_REL);

  volatile auto output = result;

  return stop - start;
}

// Provides throughput benchmarking
template <typename F, typename T, size_t N>
static LIBC_INLINE uint64_t throughput(F f, const cpp::array<T, N> &inputs) {
  uint64_t baseline = UINT64_MAX;
  for (int i = 0; i < 5; ++i)
    baseline = cpp::min(baseline, throughput_baseline<T, N>(inputs));

  asm("" ::"r"(&inputs));

  cpp::atomic_thread_fence(cpp::MemoryOrder::ACQ_REL);
  uint64_t start = gpu::processor_clock();
  asm("" ::"llr"(start));

  T result{};

#pragma clang loop unroll(disable)
  for (auto input : inputs) {
    asm("" ::"r"(input));
    result = f(input);
    asm("" ::"r"(result));
  }

  uint64_t stop = gpu::processor_clock();
  asm("" ::"r"(stop));
  cpp::atomic_thread_fence(cpp::MemoryOrder::ACQ_REL);

  volatile auto output = result;

  const uint64_t measured = stop - start;
  return measured > baseline ? (measured - baseline) : 0;
}

// Provides the *baseline* for throughput with 2 arguments: measures loop and
// measurement costs without calling the f function
template <typename T, size_t N>
static LIBC_INLINE uint64_t throughput_baseline(
    const cpp::array<T, N> &inputs1, const cpp::array<T, N> &inputs2) {
  asm("" ::"r"(&inputs1), "r"(&inputs2));

  cpp::atomic_thread_fence(cpp::MemoryOrder::ACQ_REL);
  uint64_t start = gpu::processor_clock();
  asm("" ::"llr"(start));

  T result{};

#pragma clang loop unroll(disable)
  for (size_t i = 0; i < N; i++) {
    T x = inputs1[i];
    T y = inputs2[i];
    asm("" ::"r"(x), "r"(y));
    result = x;
    asm("" ::"r"(result));
  }

  uint64_t stop = gpu::processor_clock();
  asm("" ::"r"(stop));
  cpp::atomic_thread_fence(cpp::MemoryOrder::ACQ_REL);

  volatile auto output = result;

  return stop - start;
}

// Provides throughput benchmarking for 2 arguments (e.g. atan2())
template <typename F, typename T, size_t N>
static LIBC_INLINE uint64_t throughput(F f, const cpp::array<T, N> &inputs1,
                                       const cpp::array<T, N> &inputs2) {
  uint64_t baseline = UINT64_MAX;
  for (int i = 0; i < 5; ++i)
    baseline = cpp::min(baseline, throughput_baseline<T, N>(inputs1, inputs2));

  asm("" ::"r"(&inputs1), "r"(&inputs2));

  cpp::atomic_thread_fence(cpp::MemoryOrder::ACQ_REL);
  uint64_t start = gpu::processor_clock();
  asm("" ::"llr"(start));

  T result{};

#pragma clang loop unroll(disable)
  for (size_t i = 0; i < N; i++) {
    T x = inputs1[i];
    T y = inputs2[i];
    asm("" ::"r"(x), "r"(y));
    result = f(x, y);
    asm("" ::"r"(result));
  }

  uint64_t stop = gpu::processor_clock();
  asm("" ::"r"(stop));
  cpp::atomic_thread_fence(cpp::MemoryOrder::ACQ_REL);

  volatile auto output = result;

  const uint64_t measured = stop - start;
  return measured > baseline ? (measured - baseline) : 0;
}

} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_BENCHMARKS_TIMING_NVPTX
