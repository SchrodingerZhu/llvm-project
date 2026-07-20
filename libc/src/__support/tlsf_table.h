//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// This file contains a two-level segregated fit table data structure.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_TLSF_TABLE_H
#define LLVM_LIBC_SRC___SUPPORT_TLSF_TABLE_H

#include "hdr/stdint_proxy.h"
#include "hdr/types/size_t.h"
#include "src/__support/CPP/array.h"
#include "src/__support/CPP/bit.h"
#include "src/__support/CPP/limits.h"
#include "src/__support/macros/config.h"
#include "src/__support/macros/optimization.h"

namespace LIBC_NAMESPACE_DECL {

/// Configuration for TLSFTable.
template <size_t UNIT_SIZE_VAL, size_t STEP_SIZE_BITS_VAL,
          size_t NUM_STEP_BITS_VAL, size_t NUM_TABLE_ENTRIES_VAL>
struct TLSFTableConfig {
  static constexpr size_t UNIT_SIZE = UNIT_SIZE_VAL;
  static constexpr size_t STEP_SIZE_BITS = STEP_SIZE_BITS_VAL;
  static constexpr size_t NUM_STEP_BITS = NUM_STEP_BITS_VAL;
  static constexpr size_t NUM_TABLE_ENTRIES = NUM_TABLE_ENTRIES_VAL;
};

// A two-level segregated fit table for free blocks or bin storage.
//
// The store starts with small lists that grow linearly for small sizes, which
// covers [0, ... UNIT_SIZE * EXP_BASE]. For larger sizes, the bits are managed
// in a 2-D table. One can think of each row containing NUM_STEPS lists. Along
// the row, the size grows by 2 exponentially; along the column, the size
// increases by STEP_SIZE linearly.
//
// Mathematical layout:
//   STEP_SIZE = 1 << STEP_SIZE_BITS
//   NUM_STEPS = 1 << NUM_STEP_BITS
//   EXP_BASE = STEP_SIZE * NUM_STEPS
//   LARGE_SIZE_THRESHOLD = UNIT_SIZE * EXP_BASE
//
// Visual representation with example parameters:
//   UNIT_SIZE = 32, STEP_SIZE = 8, NUM_STEPS = 4
//   EXP_BASE = 32, THRESHOLD = 1024 B (1 KiB)
//
// 1. Small Sizes (Linear Array):
//    Covers [0, ... 1024 B] growing directly by UNIT_SIZE = 32 B
//   +-------+-------+-------+-------+-------+-----------+---------------+
//   | [0 B] | [32B] | [64B] | [96B] |  ...  | [992 B]   | [1024 B (Th)] |
//   +-------+-------+-------+-------+-------+-----------+---------------+
//
// 2. Large Sizes (2-D Table):
//    Rows = FL (Exponential growth), Columns = SL (Linear steps)
//    One can think of each Row containing NUM_STEPS (4) lists.
//
//                       LINEAR INCREASE ALONG COLUMN (SL) --->
//             +---------------+---------------+---------------+---------------+
//             |    Col = 0    |    Col = 1    |    Col = 2    |    Col = 3    |
//             |    (Base)     |   (+25% FL)   |   (+50% FL)   |   (+75% FL)   |
//   +---------+---------------+---------------+---------------+---------------+
// E | Row = 0 |    1024 B     |    1280 B     |    1536 B     |    1792 B     |
// X |(Base 1K)| [1024 - 1279] | [1280 - 1535] | [1536 - 1791] | [1792 - 2047] |
// P +---------+---------------+---------------+---------------+---------------+
// O | Row = 1 |    2048 B     |    2560 B     |    3072 B     |    3584 B     |
// N |(Base 2K)| [2048 - 2559] | [2560 - 3071] | [3072 - 3583] | [3584 - 4095] |
// E +---------+---------------+---------------+---------------+---------------+
// N | Row = 2 |    4096 B     |    5120 B     |    6144 B     |    7168 B     |
// T |(Base 4K)| [4096 - 5119] | [5120 - 6143] | [6144 - 7167] | [7168 - 8191] |
// I +---------+---------------+---------------+---------------+---------------+
// A | Row = 3 |    8192 B     |   10240 B     |   12288 B     |   14336 B     |
// L |(Base 8K)| [8192 - 10239]|[10240 - 12287]|[12288 - 14335]|[14336 - 16383]|
//   +---------+---------------+---------------+---------------+---------------+
//
// Note: For the real implementation, we don't actually store the lists in a
// 2-D structure. Instead, we flatten the entire 2-D layout into a single
// flat 1-D array of size TOTAL_BITS (free_lists), and map sizes directly to
// a continuous 1-D index using size_to_bit_index. The allocation state is
// tracked compactly in the lookup_table bitmask array.
template <typename T, size_t UNIT_SIZE, size_t STEP_SIZE_BITS,
          size_t NUM_STEP_BITS, size_t NUM_TABLE_ENTRIES>
class TLSFTable {
  static_assert(cpp::has_single_bit(UNIT_SIZE),
                "unit size must be a power of two");
  static_assert(NUM_TABLE_ENTRIES > 0,
                "the lookup table must have at least one entry");

public:
  static constexpr size_t STEP_SIZE = size_t(1) << STEP_SIZE_BITS;
  static constexpr size_t NUM_STEPS = size_t(1) << NUM_STEP_BITS;
  static constexpr size_t EXP_BASE = STEP_SIZE * NUM_STEPS;
  static constexpr int UNIT_SIZE_LOG2 = cpp::bit_width(UNIT_SIZE) - 1;
  static constexpr int EXP_BASE_LOG2 = STEP_SIZE_BITS + NUM_STEP_BITS;
  static constexpr int SMALL_SIZE_LOG2 = UNIT_SIZE_LOG2 + EXP_BASE_LOG2;
  static constexpr size_t THRESHOLD = size_t(1) << SMALL_SIZE_LOG2;
  static constexpr size_t BITS_PER_ENTRY =
      cpp::numeric_limits<uintptr_t>::digits;
  static constexpr size_t TOTAL_BITS = NUM_TABLE_ENTRIES * BITS_PER_ENTRY;

  LIBC_INLINE static constexpr size_t size_to_bit_index(size_t size) {
    if (size < THRESHOLD)
      return size >> UNIT_SIZE_LOG2;

    size_t size_ilog2 = static_cast<size_t>(cpp::bit_width(size) - 1);
    size_t exp_offset = (size_ilog2 - SMALL_SIZE_LOG2) << NUM_STEP_BITS;
    size_t step_index =
        (size >> (size_ilog2 - NUM_STEP_BITS)) & (NUM_STEPS - 1);
    size_t index = EXP_BASE + exp_offset + step_index;

    return index < TOTAL_BITS ? index : TOTAL_BITS - 1;
  }

  LIBC_INLINE static constexpr cpp::array<size_t, 2>
  get_bin_range(size_t bit_index) {
    if (bit_index >= TOTAL_BITS)
      return {0, 0};

    if (bit_index == TOTAL_BITS - 1) {
      cpp::array<size_t, 2> prev = get_bin_range(bit_index - 1);
      return {prev[1] + 1, ~size_t(0)};
    }

    if (bit_index < EXP_BASE) {
      size_t min_s = bit_index << UNIT_SIZE_LOG2;
      size_t max_s = ((bit_index + 1) << UNIT_SIZE_LOG2) - 1;
      return {min_s, max_s};
    }

    size_t idx_offset = bit_index - EXP_BASE;
    size_t exp_level = idx_offset >> NUM_STEP_BITS;
    size_t step_in_octave = idx_offset & (NUM_STEPS - 1);

    size_t k = static_cast<size_t>(UNIT_SIZE_LOG2 + EXP_BASE_LOG2) + exp_level;
    size_t step_size = size_t(1) << (k - NUM_STEP_BITS);
    size_t min_s = (size_t(1) << k) + step_in_octave * step_size;
    size_t max_s = min_s + step_size - 1;
    return {min_s, max_s};
  }

  LIBC_INLINE T &get_bin(size_t bit_index) { return bins[bit_index]; }

  LIBC_INLINE const T &get_bin(size_t bit_index) const {
    return bins[bit_index];
  }

  LIBC_INLINE void set_bit(size_t bit_index) {
    size_t entry_index = bit_index / BITS_PER_ENTRY;
    size_t bit_offset = bit_index % BITS_PER_ENTRY;
    lookup_table[entry_index] |= uintptr_t(1) << bit_offset;
  }

  LIBC_INLINE void clear_bit(size_t bit_index) {
    size_t entry_index = bit_index / BITS_PER_ENTRY;
    size_t bit_offset = bit_index % BITS_PER_ENTRY;
    lookup_table[entry_index] &= ~(uintptr_t(1) << bit_offset);
  }

  LIBC_INLINE bool get_bit(size_t bit_index) const {
    size_t entry_index = bit_index / BITS_PER_ENTRY;
    size_t bit_offset = bit_index % BITS_PER_ENTRY;
    return (lookup_table[entry_index] & (uintptr_t(1) << bit_offset)) != 0;
  }

  LIBC_INLINE size_t find_first_bit_set_after(size_t bit_index) const {
    if (bit_index >= TOTAL_BITS - 1)
      return TOTAL_BITS;

    size_t target_index = bit_index + 1;
    size_t start_entry = target_index / BITS_PER_ENTRY;
    size_t bit_offset = target_index % BITS_PER_ENTRY;

    uintptr_t value = lookup_table[start_entry] & (~uintptr_t(0) << bit_offset);
    if (value != 0)
      return start_entry * BITS_PER_ENTRY +
             static_cast<size_t>(cpp::countr_zero(value));

    for (size_t i = start_entry + 1; i < NUM_TABLE_ENTRIES; ++i) {
      value = lookup_table[i];
      if (value != 0)
        return i * BITS_PER_ENTRY +
               static_cast<size_t>(cpp::countr_zero(value));
    }
    return TOTAL_BITS;
  }

private:
  cpp::array<uintptr_t, NUM_TABLE_ENTRIES> lookup_table{};
  cpp::array<T, TOTAL_BITS> bins{};
};

template <typename T, typename CONFIG>
using TLSFTableFromConfig =
    TLSFTable<T, CONFIG::UNIT_SIZE, CONFIG::STEP_SIZE_BITS,
              CONFIG::NUM_STEP_BITS, CONFIG::NUM_TABLE_ENTRIES>;

} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_TLSF_TABLE_H
