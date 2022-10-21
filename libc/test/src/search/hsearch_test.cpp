//===-- Unittests for hsearch ---------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "src/search/hcreate.h"
#include "src/search/hcreate_r.h"
#include "src/search/hdestroy.h"
#include "src/search/hsearch.h"
#include "test/ErrnoSetterMatcher.h"
#include "utils/UnitTest/LibcTest.h"
#include <asm-generic/errno-base.h>

TEST(LlvmLibcHsearchTest, CreateTooLarge) {
  using __llvm_libc::testing::ErrnoSetterMatcher::Fails;
  struct hsearch_data hdata;
  ASSERT_THAT(__llvm_libc::hcreate(-1), Fails(ENOMEM, 0));
  ASSERT_THAT(__llvm_libc::hcreate_r(-1, &hdata), Fails(ENOMEM, 0));
}

TEST(LlvmLibcHSearchTest, CreateInvalid) {
  using __llvm_libc::testing::ErrnoSetterMatcher::Fails;
  ASSERT_THAT(__llvm_libc::hcreate_r(16, nullptr), Fails(EINVAL, 0));
}

TEST(LlvmLibcHSearchTest, CreateValid) {
  struct hsearch_data hdata;
  ASSERT_GT(__llvm_libc::hcreate_r(16, &hdata), 0);
  hdestroy_r(&hdata);

  ASSERT_GT(__llvm_libc::hcreate(16), 0);
  hdestroy();
}

char search_data[] = "1234567890abcdefghijklmnopqrstuvwxyz"
                     "1234567890abcdefghijklmnopqrstuvwxyz"
                     "1234567890abcdefghijklmnopqrstuvwxyz"
                     "1234567890abcdefghijklmnopqrstuvwxyz"
                     "1234567890abcdefghijklmnopqrstuvwxyz";
char search_data2[] =
    "@@@@@@@@@@@@@@!!!!!!!!!!!!!!!!!###########$$$$$$$$$$^^^^^^&&&&&&&&";

TEST(LlvmLibcHSearchTest, InsertTooMany) {
  using __llvm_libc::testing::ErrnoSetterMatcher::Fails;
  ASSERT_GT(__llvm_libc::hcreate(16), 0);
  for (size_t i = 0; i < 16 * 7; ++i) {
    ASSERT_EQ(__llvm_libc::hsearch({&search_data[i], nullptr}, ENTER)->key,
              &search_data[i]);
  }
  ASSERT_THAT(
      static_cast<void *>(__llvm_libc::hsearch({search_data2, nullptr}, ENTER)),
      Fails(ENOMEM, static_cast<void *>(nullptr)));
  hdestroy();
}

TEST(LlvmLibcHSearchTest, NotFound) {
  using __llvm_libc::testing::ErrnoSetterMatcher::Fails;
  ASSERT_GT(__llvm_libc::hcreate(16), 0);
  ASSERT_THAT(
      static_cast<void *>(__llvm_libc::hsearch({search_data2, nullptr}, FIND)),
      Fails(ESRCH, static_cast<void *>(nullptr)));
  for (size_t i = 0; i < 16 * 7; ++i) {
    ASSERT_EQ(__llvm_libc::hsearch({&search_data[i], nullptr}, ENTER)->key,
              &search_data[i]);
  }
  ASSERT_THAT(
      static_cast<void *>(__llvm_libc::hsearch({search_data2, nullptr}, FIND)),
      Fails(ESRCH, static_cast<void *>(nullptr)));
  hdestroy();
}

TEST(LlvmLibcHSearchTest, Found) {
  using __llvm_libc::testing::ErrnoSetterMatcher::Fails;
  ASSERT_GT(__llvm_libc::hcreate(16), 0);
  for (size_t i = 0; i < 16 * 7; ++i) {
    ASSERT_EQ(__llvm_libc::hsearch(
                  {&search_data[i], reinterpret_cast<void *>(i)}, ENTER)
                  ->key,
              &search_data[i]);
  }
  for (size_t i = 0; i < 16 * 7; ++i) {
    ASSERT_EQ(__llvm_libc::hsearch({&search_data[i], nullptr}, FIND)->data,
              reinterpret_cast<void *>(i));
  }
  hdestroy();
}

TEST(LlvmLibcHSearchTest, OnlyInsertWhenNotFound) {
  using __llvm_libc::testing::ErrnoSetterMatcher::Fails;
  ASSERT_GT(__llvm_libc::hcreate(16), 0);
  for (size_t i = 0; i < 16 * 5; ++i) {
    ASSERT_EQ(__llvm_libc::hsearch(
                  {&search_data[i], reinterpret_cast<void *>(i)}, ENTER)
                  ->key,
              &search_data[i]);
  }
  for (size_t i = 0; i < 16 * 7; ++i) {
    ASSERT_EQ(__llvm_libc::hsearch(
                  {&search_data[i], reinterpret_cast<void *>(1000 + i)}, ENTER)
                  ->key,
              &search_data[i]);
  }
  for (size_t i = 0; i < 16 * 5; ++i) {
    ASSERT_EQ(__llvm_libc::hsearch({&search_data[i], nullptr}, FIND)->data,
              reinterpret_cast<void *>(i));
  }
  for (size_t i = 16 * 5; i < 16 * 7; ++i) {
    ASSERT_EQ(__llvm_libc::hsearch({&search_data[i], nullptr}, FIND)->data,
              reinterpret_cast<void *>(1000 + i));
  }
  hdestroy();
}
