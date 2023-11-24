//===-- Unittests for BlockStore ------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "src/__support/bit.h"
#include "test/UnitTest/Test.h"

namespace LIBC_NAMESPACE {

TEST(LlvmLibcBlockBitTest, TODO) {
  // TODO Implement me.
}

TEST(LlvmLibcBlockBitTest, NextPowerOfTwo) {
  ASSERT_EQ(1u, next_power_of_two(0u));
  for (unsigned int i = 0; i < 31; ++i) {
    ASSERT_EQ(1u << (i + 1), next_power_of_two((1u << i) + 1));
    ASSERT_EQ(1u << i, next_power_of_two(1u << i));
  }
}

TEST(LlvmLibcBlockBitTest, IsPowerOfTwo) {
  ASSERT_FALSE(is_power_of_two(0u));
  ASSERT_TRUE(is_power_of_two(1u));
  for (unsigned int i = 1; i < 31; ++i) {
    ASSERT_TRUE(is_power_of_two(1u << i));
    ASSERT_FALSE(is_power_of_two((1u << i) + 1));
  }
}

} // namespace LIBC_NAMESPACE
