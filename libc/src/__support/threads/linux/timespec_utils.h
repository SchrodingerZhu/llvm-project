//===--- Timespec Wrapper ---------------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
#ifndef LLVM_LIBC_SRC___SUPPORT_THREADS_LINUX_TIMESPEC_UTILS_H
#define LLVM_LIBC_SRC___SUPPORT_THREADS_LINUX_TIMESPEC_UTILS_H

#include "src/__support/common.h"
#include "src/time/linux/clock_gettime_impl.h"
namespace LIBC_NAMESPACE {

LIBC_INLINE timespec convert_clock(timespec input, clockid_t from,
                                   clockid_t to) {
  timespec from_time;
  timespec to_time;
  timespec output;
  internal::clock_gettimeimpl(from, &from_time);
  internal::clock_gettimeimpl(to, &to_time);
  output.tv_sec = input.tv_sec - from_time.tv_sec + to_time.tv_sec;
  output.tv_nsec = input.tv_nsec - from_time.tv_nsec + to_time.tv_nsec;

  if (output.tv_nsec > 1'000'000'000) {
    output.tv_sec++;
    output.tv_nsec -= 1'000'000'000;
  } else if (output.tv_nsec < 0) {
    output.tv_sec--;
    output.tv_nsec += 1'000'000'000;
  }
  return output;
}

} // namespace LIBC_NAMESPACE

#endif // LLVM_LIBC_SRC___SUPPORT_THREADS_LINUX_TIMESPEC_UTILS_H
