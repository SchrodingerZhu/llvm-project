//===--- Portable Timeout Implementation ------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "src/__support/time/timeout.h"
#include "src/__support/time/clock_gettime.h"
namespace LIBC_NAMESPACE_DECL {
static timespec add_duration(timespec timepoint, timespec duration) {
  using namespace time_units;

  long long tv_sec = static_cast<long long>(timepoint.tv_sec) +
                     static_cast<long long>(duration.tv_sec);
  long long tv_nsec = static_cast<long long>(timepoint.tv_nsec) +
                      static_cast<long long>(duration.tv_nsec);

  if (tv_nsec >= 1_s_ns) {
    tv_sec++;
    tv_nsec -= 1_s_ns;
  }

  timespec output;
  output.tv_sec = static_cast<decltype(output.tv_sec)>(tv_sec);
  output.tv_nsec = static_cast<decltype(output.tv_nsec)>(tv_nsec);
  return output;
}

timespec Timeout::duration_to_timepoint(clockid_t base, timespec duration) {
  using namespace time_units;
  timespec current_time;
  internal::clock_gettime(base, &current_time);
  return add_duration(current_time, duration);
}

timespec Timeout::timepoint_to_duration(clockid_t base, timespec timepoint) {
  using namespace time_units;
  timespec current_time;
  internal::clock_gettime(base, &current_time);

  long long tv_sec = static_cast<long long>(timepoint.tv_sec) -
                     static_cast<long long>(current_time.tv_sec);
  long long tv_nsec = static_cast<long long>(timepoint.tv_nsec) -
                      static_cast<long long>(current_time.tv_nsec);
  if (tv_nsec < 0) {
    tv_sec--;
    tv_nsec += 1_s_ns;
  }

  if (tv_sec < 0) {
    tv_sec = 0;
    tv_nsec = 0;
  }

  timespec output;
  output.tv_sec = static_cast<decltype(output.tv_sec)>(tv_sec);
  output.tv_nsec = static_cast<decltype(output.tv_nsec)>(tv_nsec);
  return output;
}

timespec Timeout::convert_clock(timespec input, clockid_t from, clockid_t to) {
  using namespace time_units;
  timespec from_time;
  timespec to_time;
  timespec output;
  internal::clock_gettime(from, &from_time);
  internal::clock_gettime(to, &to_time);
  output.tv_sec = input.tv_sec - from_time.tv_sec + to_time.tv_sec;
  output.tv_nsec = input.tv_nsec - from_time.tv_nsec + to_time.tv_nsec;

  if (output.tv_nsec > 1_s_ns) {
    output.tv_sec++;
    output.tv_nsec -= 1_s_ns;
  } else if (output.tv_nsec < 0) {
    output.tv_sec--;
    output.tv_nsec += 1_s_ns;
  }
  return output;
}
} // namespace LIBC_NAMESPACE_DECL
