//===-- SwissTable SSE2 Specialization --------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
#ifndef LLVM_LIBC_SUPPORT_SWISSTABLE_SSE2_H
#define LLVM_LIBC_SUPPORT_SWISSTABLE_SSE2_H

#include "src/__support/swisstable/common.h"
#include <emmintrin.h>

namespace __llvm_libc::internal::swisstable {

// With SSE2, every bitmask is iteratable: because
// we use single bit to encode the data.

using BitMask = BitMaskAdaptor<uint16_t, 0xffffu, 0x1u>;
using IteratableBitMask = IteratableBitMaskAdaptor<BitMask>;

struct Group {
  __m128i data;

  // Load a group of control words from an arbitary address.
  static Group load(const void *__restrict addr) {
    return {_mm_loadu_si128(static_cast<const __m128i *>(addr))};
  }

  // Load a group of control words from an aligned address.
  static Group aligned_load(const void *__restrict addr) {
    return {_mm_load_si128(static_cast<const __m128i *>(addr))};
  }

  // Store a group of control words to an aligned address.
  void aligned_store(void *addr) const {
    _mm_store_si128(static_cast<__m128i *>(addr), data);
  }

  // Find out the lanes equal to the given byte and return the bitmask
  // with corresponding bits set.
  IteratableBitMask match_byte(uint8_t byte) const {
    auto cmp = _mm_cmpeq_epi8(data, _mm_set1_epi8(byte));
    auto bitmask = static_cast<uint16_t>(_mm_movemask_epi8(cmp));
    return {bitmask};
  }

  // Find out the lanes equal to EMPTY and return the bitmask
  // with corresponding bits set.
  BitMask mask_empty() const { return match_byte(EMPTY); }

  // Find out the lanes equal to EMPTY or DELETE (highest bit set) and
  // return the bitmask with corresponding bits set.
  BitMask mask_empty_or_deleted() const {
    auto bitmask = static_cast<uint16_t>(_mm_movemask_epi8(data));
    return {bitmask};
  }

  // Find out the lanes reprensting full cells (without highest bit) and
  // return the bitmask with corresponding bits set.
  BitMask mask_full() const { return mask_empty_or_deleted().invert(); }

  // Performs the following transformation on all bytes in the group:
  // - `EMPTY => EMPTY`
  // - `DELETED => EMPTY`
  // - `FULL => DELETED`
  Group convert_special_to_empty_and_full_to_deleted() const {
    // Recall that EMPTY and DELETED are distinguished from others in
    // their highest bit. This makes them negative when considered as
    // signed integer. And for full ones, highest bits are all zeros.
    // So we first identify those lanes smaller than or equal to zero
    // and then convert them by setting the highest bit of them.
    __m128i zero = _mm_setzero_si128();
    __m128i special = _mm_cmpgt_epi8(zero, data);
    __m128i converted = _mm_or_si128(special, _mm_set1_epi8(0x80u));
    return {converted};
  }
};

} // namespace __llvm_libc::internal::swisstable
#endif // LLVM_LIBC_SUPPORT_SWISSTABLE_SSE2_H
