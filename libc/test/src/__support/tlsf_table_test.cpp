//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Unittests for TLSFTable.
///
//===----------------------------------------------------------------------===//

#include "src/__support/tlsf_table.h"
#include "test/UnitTest/Test.h"

using LIBC_NAMESPACE::TLSFTable;

TEST(LlvmLibcTLSFTableTest, SizeToBitIndexSmallSizes) {
  // UNIT_SIZE = 32, STEP_SIZE_BITS = 3, NUM_STEP_BITS = 2, NUM_TABLE_ENTRIES = 3
  // STEP_SIZE = 8, NUM_STEPS = 4, EXP_BASE = 32
  // THRESHOLD = 32 * 32 = 1024
  using Table = TLSFTable<int, 32, 3, 2, 3>;

  EXPECT_EQ(Table::size_to_bit_index(0), static_cast<size_t>(0));
  EXPECT_EQ(Table::size_to_bit_index(1), static_cast<size_t>(0));
  EXPECT_EQ(Table::size_to_bit_index(31), static_cast<size_t>(0));

  EXPECT_EQ(Table::size_to_bit_index(32), static_cast<size_t>(1));
  EXPECT_EQ(Table::size_to_bit_index(63), static_cast<size_t>(1));

  EXPECT_EQ(Table::size_to_bit_index(64), static_cast<size_t>(2));

  EXPECT_EQ(Table::size_to_bit_index(992), static_cast<size_t>(31));
  EXPECT_EQ(Table::size_to_bit_index(1023), static_cast<size_t>(31));
}

TEST(LlvmLibcTLSFTableTest, SizeToBitIndexLargeSizes) {
  using Table = TLSFTable<int, 32, 3, 2, 3>;

  // Row 0: Base 1024, Col step = 256
  EXPECT_EQ(Table::size_to_bit_index(1024), static_cast<size_t>(32));
  EXPECT_EQ(Table::size_to_bit_index(1025), static_cast<size_t>(32));
  EXPECT_EQ(Table::size_to_bit_index(1279), static_cast<size_t>(32));

  EXPECT_EQ(Table::size_to_bit_index(1280), static_cast<size_t>(33));
  EXPECT_EQ(Table::size_to_bit_index(1535), static_cast<size_t>(33));

  EXPECT_EQ(Table::size_to_bit_index(1536), static_cast<size_t>(34));
  EXPECT_EQ(Table::size_to_bit_index(1791), static_cast<size_t>(34));

  EXPECT_EQ(Table::size_to_bit_index(1792), static_cast<size_t>(35));
  EXPECT_EQ(Table::size_to_bit_index(2047), static_cast<size_t>(35));

  // Row 1: Base 2048, Col step = 512
  EXPECT_EQ(Table::size_to_bit_index(2048), static_cast<size_t>(36));
  EXPECT_EQ(Table::size_to_bit_index(2559), static_cast<size_t>(36));

  EXPECT_EQ(Table::size_to_bit_index(2560), static_cast<size_t>(37));
  EXPECT_EQ(Table::size_to_bit_index(3071), static_cast<size_t>(37));

  EXPECT_EQ(Table::size_to_bit_index(3072), static_cast<size_t>(38));
  EXPECT_EQ(Table::size_to_bit_index(3583), static_cast<size_t>(38));

  EXPECT_EQ(Table::size_to_bit_index(3584), static_cast<size_t>(39));
  EXPECT_EQ(Table::size_to_bit_index(4095), static_cast<size_t>(39));

  // Row 2: Base 4096, Col step = 1024
  EXPECT_EQ(Table::size_to_bit_index(4096), static_cast<size_t>(40));

  // Clamping check for oversized value
  EXPECT_EQ(Table::size_to_bit_index(~static_cast<size_t>(0)),
            Table::TOTAL_BITS - 1);
}

TEST(LlvmLibcTLSFTableTest, GetBin) {
  TLSFTable<int, 32, 3, 2, 3> table;

  table.get_bin(table.size_to_bit_index(100)) = 42;
  table.get_bin(table.size_to_bit_index(1280)) = 100;
  table.get_bin(table.size_to_bit_index(3584)) = 999;

  EXPECT_EQ(table.get_bin(table.size_to_bit_index(100)), 42);
  EXPECT_EQ(table.get_bin(table.size_to_bit_index(1280)), 100);
  EXPECT_EQ(table.get_bin(table.size_to_bit_index(3584)), 999);

  // Check const get_bin
  const auto &const_table = table;
  EXPECT_EQ(const_table.get_bin(const_table.size_to_bit_index(100)), 42);
  EXPECT_EQ(const_table.get_bin(const_table.size_to_bit_index(1280)), 100);
  EXPECT_EQ(const_table.get_bin(const_table.size_to_bit_index(3584)), 999);
}

TEST(LlvmLibcTLSFTableTest, BitmaskOperations) {
  TLSFTable<int, 32, 3, 2, 3> table;

  // Initially all bits are cleared
  EXPECT_FALSE(table.get_bit(5));
  EXPECT_FALSE(table.get_bit(70));

  table.set_bit(5);
  table.set_bit(70);

  EXPECT_TRUE(table.get_bit(5));
  EXPECT_TRUE(table.get_bit(70));
  EXPECT_FALSE(table.get_bit(6));

  EXPECT_EQ(table.find_first_bit_set_after(0), static_cast<size_t>(5));
  EXPECT_EQ(table.find_first_bit_set_after(4), static_cast<size_t>(5));
  EXPECT_EQ(table.find_first_bit_set_after(5), static_cast<size_t>(70));
  EXPECT_EQ(table.find_first_bit_set_after(69), static_cast<size_t>(70));
  EXPECT_EQ(table.find_first_bit_set_after(70), decltype(table)::TOTAL_BITS);

  table.clear_bit(5);
  EXPECT_FALSE(table.get_bit(5));
  EXPECT_EQ(table.find_first_bit_set_after(0), static_cast<size_t>(70));

  table.clear_bit(70);
  EXPECT_FALSE(table.get_bit(70));
  EXPECT_EQ(table.find_first_bit_set_after(0), decltype(table)::TOTAL_BITS);
}
