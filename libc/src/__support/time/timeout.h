//===--- Portable Timeout Implementation ------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_TIME_CLOCK_GETTIME_H
#define LLVM_LIBC_SRC___SUPPORT_TIME_CLOCK_GETTIME_H

#include "hdr/types/clockid_t.h"
#include "hdr/types/struct_timespec.h"
#include "src/__support/error_or.h"

namespace LIBC_NAMESPACE_DECL {
enum class TimeoutKind { Duration, Timepoint };
class Timeout {
  TimeoutKind kind;
  clockid_t base;
  timespec ts;
  LIBC_INLINE Timeout(TimeoutKind k, clockid_t b, timespec t)
      : kind(k), base(b), ts(t) {}

public:
  LIBC_INLINE static Timeout duration(clockid_t base, timespec t) {
    return Timeout(TimeoutKind::Duration, base, t);
  }
  LIBC_INLINE static Timeout timepoint(clockid_t base, timespec t) {
    return Timeout(TimeoutKind::Timepoint, base, t);
  }
  LIBC_INLINE TimeoutKind get_kind() const { return kind; }
  LIBC_INLINE clockid_t get_base() const { return base; }
  LIBC_INLINE timespec get_timespec() const { return ts; }
  Timeout to_timepoint() const;
  Timeout to_duration() const;
  Timeout convert_base(clockid_t new_base) const;
};

Timeout Timeout::convert_base(clockid_t new_base) const {
  timespec new_ts = ts;
  if (kind == TimeoutKind::Timepoint) {
    timespec current_time;
    internal::clock_gettime(base, &current_time);
    new_ts.tv_sec += current_time.tv_sec;
    new_ts.tv_nsec += current_time.tv_nsec;
  }
  return Timeout(kind, new_base, new_ts);
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_TIME_CLOCK_GETTIME_H
