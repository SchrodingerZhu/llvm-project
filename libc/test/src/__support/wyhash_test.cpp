//===-- Unittests for wyhash ----------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "src/__support/wyhash.h"
#include "src/string/strlen.h"
#include "utils/UnitTest/LibcTest.h"

// Examination Data from SMHasher
TEST(LlvmLibcWyHashTest, DefaultValues) {
  using namespace __llvm_libc::cpp::wyhash;
  // clang-format off
  const char *data[] = {
    "",
    "a",
    "abc",
    "message digest",
    "abcdefghijklmnopqrstuvwxyz",
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789",
    "12345678901234567890123456789012345678901234567890123456789012345678901234567890" 
  };
  uint64_t hash[] = {
    0x42bc986dc5eec4d3,
    0x84508dc903c31551,
    0xbc54887cfc9ecb1,
    0x6e2ff3298208a67c,
    0x9a64e42e897195b9,
    0x9199383239c32554,
    0x7c1ccf6bba30f5a5,
  };
  // clang-format on
  for (size_t i = 0; i < 7; ++i) {
    ASSERT_EQ(DefaultHash::hash(data[i], __llvm_libc::strlen(data[i]), i),
              hash[i]);
  }
}
