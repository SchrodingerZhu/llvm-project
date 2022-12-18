//===-- SwissTable Platform Dispatch ----------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
#ifndef LLVM_LIBC_SUPPORT_SWISSTABLE_DISPATCH_H
#define LLVM_LIBC_SUPPORT_SWISSTABLE_DISPATCH_H
#include "src/__support/architectures.h"
#if defined(LLVM_LIBC_ARCH_X86_64) && defined(__SSE2__)
#include "src/__support/swisstable/sse2.h"
#elif defined(LLVM_LIBC_ARCH_ANY_ARM) && defined(__ARM_NEON) &&                \
    defined(__ORDER_LITTLE_ENDIAN__)
#include "src/__support/swisstable/asimd.h"
#else
#include "src/__support/swisstable/generic.h"
#endif
#endif // LLVM_LIBC_SUPPORT_SWISSTABLE_DISPATCH_H
