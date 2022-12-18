//===-- SwissTable ASIMD Specialization -------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
#ifndef LLVM_LIBC_SUPPORT_SWISSTABLE_ASIMD_H
#define LLVM_LIBC_SUPPORT_SWISSTABLE_ASIMD_H

#include "src/__support/swisstable/common.h"
#include <arm_neon.h>

namespace __llvm_libc::internal::swisstable {

// According to abseil-cpp, ARM's 16-byte ASIMD operations may
// introduce too much latency.
// Reference:
// https://github.com/abseil/abseil-cpp/commit/6481443560a92d0a3a55a31807de0cd712cd4f88
// With ASIMD, some bitmasks are not iteratale. This is because we
// do not want to clear the lower bits in each stride with extra
// `AND` operation.
using BitMask = BitMaskAdaptor<uint64_t, 0x8080808080808080ull, 0x8ull>;
using IteratableBitMask = IteratableBitMaskAdaptor<BitMask>;

struct Group {
  int8x8_t data;

  // Load a group of control words from an arbitary address.
  static Group load(const void *__restrict addr) {
    return {vld1_s8(static_cast<const int8_t *>(addr))};
  }

  // Load a group of control words from an aligned address.
  // Notice that there is no difference of aligned/unaigned
  // loading in ASIMD.
  static Group aligned_load(const void *__restrict addr) {

    return {vld1_s8(static_cast<const int8_t *>(addr))};
  }

  // Store a group of control words to an aligned address.
  void aligned_store(void *addr) const {
    vst1_s8(static_cast<int8_t *>(addr), data);
  }

  // Find out the lanes equal to the given byte and return the bitmask
  // with corresponding bits set.
  IteratableBitMask match_byte(uint8_t byte) const {
    auto duplicated = vdup_n_s8(byte);
    auto cmp = vceq_s8(data, duplicated);
    auto converted = vget_lane_u64(vreinterpret_u64_u8(cmp), 0);
    return {converted & BitMask::MASK};
  }

  // Find out the lanes equal to EMPTY and return the bitmask
  // with corresponding bits set.
  BitMask mask_empty() const {
    auto duplicated = vdup_n_s8(EMPTY);
    auto cmp = vceq_s8(data, duplicated);
    auto converted = vget_lane_u64(vreinterpret_u64_u8(cmp), 0);
    return {converted};
  }

  // Find out the lanes equal to EMPTY or DELETE (highest bit set) and
  // return the bitmask with corresponding bits set.
  BitMask mask_empty_or_deleted() const {
    auto converted = vget_lane_u64(vreinterpret_u64_s8(data), 0);
    return {converted & BitMask::MASK};
  }

  // Find out the lanes reprensting full cells (without highest bit) and
  // return the bitmask with corresponding bits set.
  BitMask mask_full() const { return mask_empty_or_deleted().invert(); }

  // Performs the following transformation on all bytes in the group:
  // - `EMPTY => EMPTY`
  // - `DELETED => EMPTY`
  // - `FULL => DELETED`
  Group convert_special_to_empty_and_full_to_deleted() const {
    auto dup = vdup_n_s8(0x80);
    auto zero = vdup_n_s8(0x00);
    auto special = vcgt_s8(zero, data);
    return vorr_s8(dup, vreinterpret_s8_u8(special));
  }
};

} // namespace __llvm_libc::internal::swisstable
#endif // LLVM_LIBC_SUPPORT_SWISSTABLE_ASIMD_H
