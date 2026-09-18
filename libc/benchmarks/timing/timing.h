//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

///
/// \file
/// Architecture dispatch for benchmark timing.
///
//===----------------------------------------------------------------------===//
#ifndef LLVM_LIBC_BENCHMARKS_TIMING_H
#define LLVM_LIBC_BENCHMARKS_TIMING_H

#include "src/__support/macros/properties/architectures.h"

#if defined(LIBC_TARGET_ARCH_IS_AMDGPU)
#include "amdgpu/timing.h"
#elif defined(LIBC_TARGET_ARCH_IS_NVPTX)
#include "nvptx/timing.h"
#elif defined(LIBC_TARGET_ARCH_IS_ARM)
#include "cortex_m/timing.h"
#else
#error "unsupported platform"
#endif

#endif // LLVM_LIBC_BENCHMARKS_TIMING_H
