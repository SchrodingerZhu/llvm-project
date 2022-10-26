//===-- Unittests for Builtin Wrappers ------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "src/__support/CPP/limits.h"
#include "src/__support/builtin_wrappers.h"
#include "utils/UnitTest/LibcTest.h"

namespace __llvm_libc {

template <class T> static int trivial_ctz(T x) {
  size_t i = 0;
  while (i < 8 * sizeof(T)) {
    if (x & 1)
      break;
    i++;
    x >>= 1;
  }
  return static_cast<int>(i);
}

template <class T> static int trivial_clz(T x) {
  size_t i = 0;
  while (x != 0) {
    i++;
    x >>= 1;
  }
  return 8 * sizeof(T) - static_cast<int>(i);
}

template <class T, int (&Trivial)(T), int (&Safe)(T), int (&Unsafe)(T)>
struct LlvmLibcBuiltinWrapperZeroCount : testing::Test {
  using Type = T;
  void check_all(const T min, const T max) {

    for (T i = min; i <= max; i++) {
      if (i != 0) {
        ASSERT_EQ(Trivial(i), Unsafe(i));
      }
      ASSERT_EQ(Trivial(i), Safe(i));
      if (i == max)
        break;
    }
  }
  void per_byte() {
    for (size_t i = 0; i < sizeof(T); ++i) {
      for (T j = 0; j <= 0xFF; ++j) {
        T target = j << (8 * i);
        if (target != 0) {
          ASSERT_EQ(Trivial(target), Unsafe(target));
        }
        ASSERT_EQ(Trivial(target), Safe(target));
      }
    }
  }
};

template <class T>
using LlvmLibcBuiltinWrapperClz =
    LlvmLibcBuiltinWrapperZeroCount<T, trivial_clz<T>, safe_clz<T>,
                                    unsafe_clz<T>>;

template <class T>
using LlvmLibcBuiltinWrapperCtz =
    LlvmLibcBuiltinWrapperZeroCount<T, trivial_ctz<T>, safe_ctz<T>,
                                    unsafe_ctz<T>>;

using LlvmLibcBuiltinWrapperUnsignedShortClz =
    LlvmLibcBuiltinWrapperClz<unsigned short>;

using LlvmLibcBuiltinWrapperUnsignedShortCtz =
    LlvmLibcBuiltinWrapperCtz<unsigned short>;

using LlvmLibcBuiltinWrapperUnsignedIntClz =
    LlvmLibcBuiltinWrapperClz<unsigned int>;

using LlvmLibcBuiltinWrapperUnsignedIntCtz =
    LlvmLibcBuiltinWrapperCtz<unsigned int>;

using LlvmLibcBuiltinWrapperUnsignedLongClz =
    LlvmLibcBuiltinWrapperClz<unsigned long>;

using LlvmLibcBuiltinWrapperUnsignedLongCtz =
    LlvmLibcBuiltinWrapperCtz<unsigned long>;

using LlvmLibcBuiltinWrapperUnsignedLongLongClz =
    LlvmLibcBuiltinWrapperClz<unsigned long long>;

using LlvmLibcBuiltinWrapperUnsignedLongLongCtz =
    LlvmLibcBuiltinWrapperCtz<unsigned long long>;

TEST_F(LlvmLibcBuiltinWrapperUnsignedShortClz, CheckAll) {
  check_all(cpp::numeric_limits<Type>::min(), cpp::numeric_limits<Type>::max());
}

TEST_F(LlvmLibcBuiltinWrapperUnsignedShortCtz, CheckAll) {
  check_all(cpp::numeric_limits<Type>::min(), cpp::numeric_limits<Type>::max());
}

TEST_F(LlvmLibcBuiltinWrapperUnsignedIntClz, CheckPerByte) { per_byte(); }

TEST_F(LlvmLibcBuiltinWrapperUnsignedIntCtz, CheckPerByte) { per_byte(); }

TEST_F(LlvmLibcBuiltinWrapperUnsignedLongClz, CheckPerByte) { per_byte(); }

TEST_F(LlvmLibcBuiltinWrapperUnsignedLongCtz, CheckPerByte) { per_byte(); }

TEST_F(LlvmLibcBuiltinWrapperUnsignedLongLongClz, CheckPerByte) { per_byte(); }

TEST_F(LlvmLibcBuiltinWrapperUnsignedLongLongCtz, CheckPerByte) { per_byte(); }

} // namespace __llvm_libc
