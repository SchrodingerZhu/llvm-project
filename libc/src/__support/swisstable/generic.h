//===-- SwissTable Generic Fallback -----------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
#ifndef LLVM_LIBC_SUPPORT_SWISSTABLE_GENERIC_H
#define LLVM_LIBC_SUPPORT_SWISSTABLE_GENERIC_H
#include "src/__support/endian.h"
#include "src/__support/swisstable/common.h"
#include "src/string/memory_utils/memcpy_implementations.h"

namespace __llvm_libc::internal::swisstable {

// Helper function to spread a byte across the whole word.
// Accumutively, the procedure looks like:
//    byte                  = 0x00000000000000ff
//    byte | (byte << 8)    = 0x000000000000ffff
//    byte | (byte << 16)   = 0x00000000ffffffff
//    byte | (byte << 32)   = 0xffffffffffffffff
constexpr static inline uintptr_t repeat(uintptr_t byte) {
  size_t shift_amount = 8;
  while (shift_amount < sizeof(uintptr_t) * 8) {
    byte |= byte << shift_amount;
    shift_amount <<= 1;
  }
  return byte;
}

using BitMask = BitMaskAdaptor<uintptr_t, repeat(0x80), 0x8ull>;
using IteratableBitMask = IteratableBitMaskAdaptor<BitMask>;

struct Group {
  uintptr_t data;

  // Load a group of control words from an arbitary address.
  static Group load(const void *__restrict addr) {
    uintptr_t data;
    inline_memcpy(reinterpret_cast<char *>(&data),
                  static_cast<const char *>(addr), sizeof(data));
    return {data};
  }

  // Load a group of control words from an aligned address.
  // Notice that there is no difference of aligned/unaigned
  // loading in ASIMD.
  static Group aligned_load(const void *__restrict addr) {
    uintptr_t data = *static_cast<const uintptr_t *>(addr);
    return {data};
  }

  // Store a group of control words to an aligned address.
  void aligned_store(void *addr) const {
    *static_cast<uintptr_t *>(addr) = data;
  }

  // Find out the lanes equal to the given byte and return the bitmask
  // with corresponding bits set.
  IteratableBitMask match_byte(uint8_t byte) const {
    // Given byte = 0x10, suppose the data is:
    //
    //       data = [ 0x10 | 0x10 | 0x00 | 0xF1 | ... ]
    //
    // First, we compare the byte using XOR operation:
    //
    //        [ 0x10 | 0x10 | 0x10 | 0x10 | ... ]   (0)
    //      ^ [ 0x10 | 0x10 | 0x00 | 0xF1 | ... ]   (1)
    //      = [ 0x00 | 0x00 | 0x10 | 0xE1 | ... ]   (2)
    //
    // Notice that the equal positions will now be 0x00, so if we substract 0x01
    // respective to every byte, it will need to carry the substraction to upper
    // bits (assume no carry from the hidden parts)
    //        [ 0x00 | 0x00 | 0x10 | 0xE1 | ... ]   (2)
    //      - [ 0x01 | 0x01 | 0x01 | 0x01 | ... ]   (3)
    //      = [ 0xFE | 0xFF | 0x0F | 0xE0 | ... ]   (4)
    //
    // But there may be some bytes whose highest bit is already set after the
    // xor operation. To rule out these positions, we AND them with the NOT
    // of the XOR result:
    //
    //        [ 0xFF | 0xFF | 0xEF | 0x1E | ... ]   (5, NOT (2))
    //      & [ 0xFE | 0xFF | 0x0F | 0xE0 | ... ]   (4)
    //      = [ 0xFE | 0xFF | 0x0F | 0x10 | ... ]   (6)
    //
    // To make the bitmask iteratable, only one bit can be set in each stride.
    // So we AND each byte with 0x80 and keep only the highest bit:
    //
    //        [ 0xFE | 0xFF | 0x0F | 0x10 | ... ]   (6)
    //      & [ 0x80 | 0x80 | 0x80 | 0x80 | ... ]   (7)
    //      = [ 0x80 | 0x80 | 0x00 | 0x00 | ... ]   (8)
    //
    // However, there are possitbilites for false positives. For example, if the
    // data is [ 0x10 | 0x11 | 0x10 | 0xF1 | ... ]. This only happens when there
    // is a key only differs from the searched by the lowest bit. The claims
    // are:
    //
    //  - This never happens for `EMPTY` and `DELETED`, only full entries.
    //  - The check for key equality will catch these.
    //  - This only happens if there is at least 1 true match.
    //  - The chance of this happening is very low (< 1% chance per byte).
    auto cmp = data ^ repeat(byte);
    auto result = __llvm_libc::Endian::to_little_endian((cmp - repeat(0x01)) &
                                                        ~cmp & repeat(0x80));
    return {result};
  }

  // Find out the lanes equal to EMPTY and return the bitmask
  // with corresponding bits set.
  BitMask mask_empty() const {
    // If the high bit is set, then the byte must be either:
    // 1111_1111 (EMPTY) or 1000_0000 (DELETED).
    // So we can just check if the top two bits are 1 by ANDing them.
    return {__llvm_libc::Endian::to_little_endian(data & (data << 1) &
                                                  repeat(0x80))};
  }

  // Find out the lanes equal to EMPTY or DELETE (highest bit set) and
  // return the bitmask with corresponding bits set.
  BitMask mask_empty_or_deleted() const {
    return {__llvm_libc::Endian::to_little_endian(data) & repeat(0x80)};
  }

  // Find out the lanes reprensting full cells (without highest bit) and
  // return the bitmask with corresponding bits set.
  BitMask mask_full() const { return mask_empty_or_deleted().invert(); }

  // Performs the following transformation on all bytes in the group:
  // - `EMPTY => EMPTY`
  // - `DELETED => EMPTY`
  // - `FULL => DELETED`
  Group convert_special_to_empty_and_full_to_deleted() const {
    // Set the highest bit only for positions whose highest bit is not set
    // before.
    //
    //   data = [ 00000000 | 11111111 | 10000000 | ... ]
    //  ~data = [ 11111111 | 00000000 | 00000000 | ... ]
    //   full = [ 10000000 | 00000000 | 00000000 | ... ]

    auto full = (~data) & repeat(0x80);

    // Inverse the bit and convert `01111111` to `1000000` by
    // add `1` in that bit. The carry will not propogate outside
    // that byte:
    //      ~full = [ 01111111 | 11111111 | 11111111 | ... ]
    //  full >> 1 = [ 00000001 | 00000000 | 00000000 | ... ]
    //     result = [ 10000000 | 11111111 | 11111111 | ... ]
    return {~full + (full >> 1)};
  }
};

} // namespace __llvm_libc::internal::swisstable
#endif // LLVM_LIBC_SUPPORT_SWISSTABLE_GENERIC_H
