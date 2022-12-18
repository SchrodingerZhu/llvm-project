
//===-- SwissTable Common Definitions ---------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SUPPORT_SWISSTABLE_COMMON_H
#define LLVM_LIBC_SUPPORT_SWISSTABLE_COMMON_H

#include <src/__support/builtin_wrappers.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

namespace __llvm_libc::internal::swisstable {
// special values of the control byte
using CtrlWord = uint8_t;
using HashWord = uint64_t;
constexpr static inline CtrlWord EMPTY = 0b11111111u;
constexpr static inline CtrlWord DELETED = 0b10000000u;

// Implementations of the bitmask.
// The backend word type may vary depending on different microarchitectures.
// For example, with X86 SSE2, the bitmask is just the 16bit unsigned integer
// corresponding to lanes in a SIMD register.
template <typename T, T WORD_MASK, size_t WORD_STRIDE> struct BitMaskAdaptor {
  // A masked constant whose bits are all set.
  constexpr static inline T MASK = WORD_MASK;
  // A stride in the bitmask may use multiple bits.
  constexpr static inline size_t STRIDE = WORD_STRIDE;

  T word;

  // Invert zeros and ones inside the word.
  BitMaskAdaptor invert() const { return {static_cast<T>(word ^ WORD_MASK)}; }

  // Operator helper to do bit manipulations.
  BitMaskAdaptor operator^(T value) const {
    return {static_cast<T>(this->word ^ value)};
  }

  // Check if any bit is set inside the word.
  bool any_bit_set() const { return word != 0; }

  // Count trailing zeros with respect to stride.
  size_t trailing_zeros() const { return safe_ctz<T>(word) / WORD_STRIDE; }

  // Count trailing zeros with respect to stride. (Assume the bitmask is none
  // zero.)
  size_t lowest_set_bit_nonzero() const {
    return unsafe_ctz<T>(word) / WORD_STRIDE;
  }

  // Count leading zeros with respect to stride.
  size_t leading_zeros() const {
    // move the word to the highest location.
    return safe_clz<T>(word) / WORD_STRIDE;
  }
};

template <class BitMask> struct IteratableBitMaskAdaptor : public BitMask {
  // Use the bitmask as an iterator. Update the state and return current lowest
  // set bit. To make the bitmask iterable, each stride must contain 0 or exact
  // 1 set bit.
  void remove_lowest_bit() {
    // Remove the last set bit inside the word:
    //    word              = 011110100 (original value)
    //    word - 1          = 011110011 (invert all bits up to the last set bit)
    //    word & (word - 1) = 011110000 (value with the last bit cleared)
    this->word = this->word & (this->word - 1);
  }
  using value_type = size_t;
  using iterator = BitMask;
  using const_iterator = BitMask;
  size_t operator*() { return this->lowest_set_bit_nonzero(); }
  IteratableBitMaskAdaptor &operator++() {
    this->remove_lowest_bit();
    return *this;
  }
  IteratableBitMaskAdaptor begin() { return *this; }
  IteratableBitMaskAdaptor end() { return {0}; }
  bool operator==(const IteratableBitMaskAdaptor &other) {
    return this->word == other.word;
  }
  bool operator!=(const IteratableBitMaskAdaptor &other) {
    return this->word != other.word;
  }
};

} // namespace __llvm_libc::internal::swisstable
#endif // LLVM_LIBC_SUPPORT_SWISSTABLE_COMMON_H
