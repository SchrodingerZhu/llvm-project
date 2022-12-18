//===-- Memory Size Checker for SwissTable ----------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SUPPORT_SWISSTABLE_SAFE_MEM_SIZE_H
#define LLVM_LIBC_SUPPORT_SWISSTABLE_SAFE_MEM_SIZE_H
#include "src/__support/CPP/limits.h"
#include "src/__support/common.h"
#include <stddef.h>
#include <sys/types.h>
namespace __llvm_libc::internal {
// Limit memory size to the max of ssize_t
class SafeMemSize {
private:
  static constexpr size_t MAX_MEM_SIZE =
      static_cast<size_t>(__llvm_libc::cpp::numeric_limits<ssize_t>::max());
  ssize_t value;
  explicit SafeMemSize(ssize_t value) : value(value) {}

public:
  explicit SafeMemSize(size_t value)
      : value(value <= MAX_MEM_SIZE ? static_cast<ssize_t>(value) : -1) {}
  operator size_t() { return static_cast<size_t>(value); }
  bool valid() { return value >= 0; }
  SafeMemSize operator+(const SafeMemSize &other) {
    ssize_t result;
    if (unlikely((value | other.value) < 0))
      result = -1;
    result = value + other.value;
    return SafeMemSize{result};
  }
  SafeMemSize operator*(const SafeMemSize &other) {
    ssize_t result;
    if (unlikely((value | other.value) < 0))
      result = -1;
    if (unlikely(__builtin_mul_overflow(value, other.value, &result)))
      result = -1;
    return SafeMemSize{result};
  }
};
} // namespace __llvm_libc::internal
#endif // LLVM_LIBC_SUPPORT_SWISSTABLE_SAFE_MEM_SIZE_H
