//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Cortex-M benchmark timing utilities.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_BENCHMARKS_TIMING_CORTEX_M_H
#define LLVM_LIBC_BENCHMARKS_TIMING_CORTEX_M_H

#if !defined(__ARM_ARCH_7M__) && !defined(__ARM_ARCH_7EM__) &&                 \
    !defined(__ARM_ARCH_8M_MAIN__) && !defined(__ARM_ARCH_8_1M_MAIN__)
#error "Benchmark timing requires a Cortex-M mainline target"
#endif

#include "hdr/stdint_proxy.h"
#include "src/__support/CPP/bit.h"
#include "src/__support/CPP/type_traits.h"
#include "src/__support/macros/attributes.h"
#include "src/__support/macros/config.h"

namespace LIBC_NAMESPACE_DECL {

// The platform must provide access to an enabled DWT cycle counter. Each
// measurement must finish within one wrap of the 32-bit counter.
LIBC_INLINE uint32_t read_dwt_cycle_counter() {
  constexpr uintptr_t DWT_CYCCNT_ADDRESS = 0xE0001004;
  asm volatile("dsb sy\nisb sy" ::: "memory");
  return *cpp::bit_cast<volatile const uint32_t *>(DWT_CYCCNT_ADDRESS);
}

[[gnu::noinline]] static LIBC_INLINE uint64_t overhead() {
  const uint32_t start = read_dwt_cycle_counter();
  const uint32_t stop = read_dwt_cycle_counter();
  return static_cast<uint32_t>(stop - start);
}

// Measure a callable, including the barriers needed to complete memory
// accesses. Capture inputs in the callable so they are loaded after the start
// timestamp.
template <typename F>
[[gnu::noinline]] static LIBC_INLINE uint64_t latency(F &&f) {
  auto *callable = __builtin_addressof(f);
  const uint32_t start = read_dwt_cycle_counter();
  // Hide the callable's address to prevent folding or hoisting its inputs.
  asm volatile("" : "+r"(callable) : "r"(start) : "memory");
  if constexpr (cpp::is_void_v<decltype(static_cast<F &&>(*callable)())>) {
    static_cast<F &&>(*callable)();
  } else {
    decltype(auto) result = static_cast<F &&>(*callable)();
    // Escape the result through memory to support any return type.
    asm volatile("" : : "r"(__builtin_addressof(result)) : "memory");
  }
  const uint32_t stop = read_dwt_cycle_counter();
  return static_cast<uint32_t>(stop - start);
}

} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_BENCHMARKS_TIMING_CORTEX_M_H
