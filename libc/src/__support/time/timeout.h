//===--- Portable Timeout Uitilities ----------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_TIME_TIMEOUT_H
#define LLVM_LIBC_SRC___SUPPORT_TIME_TIMEOUT_H

#include "hdr/types/clockid_t.h"
#include "hdr/types/struct_timespec.h"
#include "src/__support/CPP/optional.h"
#include "src/__support/error_or.h"
#include "src/__support/time/units.h"

namespace LIBC_NAMESPACE_DECL {
enum class TimeoutKind { Duration, Timepoint };
class Timeout {
  TimeoutKind kind;
  clockid_t base;
  timespec ts;
  LIBC_INLINE Timeout(TimeoutKind k, clockid_t b, timespec t)
      : kind(k), base(b), ts(t) {}

public:
  enum class Error { Invalid, BeforeEpoch };
  using Result = cpp::expected<Timeout, Timeout::Error>;
  LIBC_INLINE static constexpr Result duration(timespec t);
  LIBC_INLINE static constexpr Result timepoint(clockid_t base, timespec ts);
  LIBC_INLINE TimeoutKind get_kind() const { return kind; }
  LIBC_INLINE clockid_t get_base() const { return base; }
  LIBC_INLINE const timespec &get_timespec() const { return ts; }
  LIBC_INLINE Timeout to_timepoint(clockid_t base) const;
  LIBC_INLINE Timeout to_duration() const;

private:
  static timespec convert_clock(timespec input, clockid_t from, clockid_t to);
  static timespec timepoint_to_duration(clockid_t base, timespec timepoint);
  static timespec duration_to_timepoint(clockid_t base, timespec duration);
};

LIBC_INLINE constexpr Timeout::Result Timeout::timepoint(clockid_t base,
                                                         timespec ts) {
  using namespace time_units;
  if (ts.tv_nsec < 0 || ts.tv_nsec >= 1_s_ns)
    return cpp::unexpected(Error::Invalid);

  // POSIX allows tv_sec to be negative. We interpret this as an expired
  // timeout.
  if (ts.tv_sec < 0)
    return cpp::unexpected(Error::BeforeEpoch);

  return Timeout{TimeoutKind::Timepoint, base, ts};
}

LIBC_INLINE constexpr Timeout::Result Timeout::duration(timespec t) {
  using namespace time_units;
  if (t.tv_nsec < 0 || t.tv_nsec >= 1_s_ns)
    return cpp::unexpected(Error::Invalid);

  return Timeout{TimeoutKind::Duration, static_cast<clockid_t>(-1), t};
}

LIBC_INLINE Timeout Timeout::to_timepoint(clockid_t base) const {
  if (kind == TimeoutKind::Timepoint) {
    if (base == this->base)
      return *this;
    return Timeout{kind, base, convert_clock(ts, this->base, base)};
  }
  return Timeout{TimeoutKind::Timepoint, base,
                 duration_to_timepoint(this->base, ts)};
}

LIBC_INLINE Timeout Timeout::to_duration() const {
  if (kind == TimeoutKind::Duration)
    return *this;
  return Timeout{TimeoutKind::Duration, static_cast<clockid_t>(-1),
                 timepoint_to_duration(base, ts)};
}
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_TIME_TIMEOUT_H
