//===-- unit tests for linux's timeout utilities --------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "hdr/time_macros.h"
#include "src/__support/CPP/expected.h"
#include "src/__support/time/clock_gettime.h"
#include "src/__support/time/timeout.h"
#include "test/UnitTest/Test.h"

template <class T, class E>
using expected = LIBC_NAMESPACE::cpp::expected<T, E>;
using Timeout = LIBC_NAMESPACE::Timeout;

TEST(LlvmLibcSupportLinuxTimeoutTest, NegativeSecond) {
  timespec ts = {-1, 0};
  expected<Timeout, Timeout::Error> result =
      Timeout::timepoint(CLOCK_MONOTONIC, ts);
  ASSERT_FALSE(result.has_value());
  ASSERT_EQ(result.error(), Timeout::Error::BeforeEpoch);
}
TEST(LlvmLibcSupportLinuxTimeoutTest, OverflowNano) {
  using namespace LIBC_NAMESPACE::time_units;
  timespec ts = {0, 2_s_ns};
  expected<Timeout, Timeout::Error> result =
      Timeout::timepoint(CLOCK_MONOTONIC, ts);
  ASSERT_FALSE(result.has_value());
  ASSERT_EQ(result.error(), Timeout::Error::Invalid);
}
TEST(LlvmLibcSupportLinuxTimeoutTest, UnderflowNano) {
  timespec ts = {0, -1};
  expected<Timeout, Timeout::Error> result =
      Timeout::timepoint(CLOCK_MONOTONIC, ts);
  ASSERT_FALSE(result.has_value());
  ASSERT_EQ(result.error(), Timeout::Error::Invalid);
}
TEST(LlvmLibcSupportLinuxTimeoutTest, NoChangeIfClockIsMonotonic) {
  timespec ts = {10000, 0};
  expected<Timeout, Timeout::Error> result =
      Timeout::timepoint(CLOCK_MONOTONIC, ts);
  ASSERT_TRUE(result.has_value());
  ASSERT_EQ(result->get_base(), CLOCK_MONOTONIC);
  ASSERT_EQ(result->get_timespec().tv_sec, static_cast<time_t>(10000));
  ASSERT_EQ(result->get_timespec().tv_nsec, static_cast<long int>(0));
}
TEST(LlvmLibcSupportLinuxTimeoutTest, ValidAfterConversion) {
  timespec ts;
  LIBC_NAMESPACE::internal::clock_gettime(CLOCK_REALTIME, &ts);
  expected<Timeout, Timeout::Error> result =
      Timeout::timepoint(CLOCK_REALTIME, ts);
  ASSERT_TRUE(result.has_value());
  *result = result->to_timepoint(CLOCK_MONOTONIC);
  ASSERT_EQ(result->get_base(), CLOCK_MONOTONIC);
  ASSERT_TRUE(
      Timeout::timepoint(CLOCK_MONOTONIC, result->get_timespec()).has_value());
}
