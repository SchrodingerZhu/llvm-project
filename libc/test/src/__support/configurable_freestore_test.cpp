//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Unittests for ConfigurableFreeStore.
///
//===----------------------------------------------------------------------===//

#include "src/__support/CPP/limits.h"
#include "src/__support/block.h"
#include "src/__support/configurable_freestore.h"
#include "test/UnitTest/Test.h"

using LIBC_NAMESPACE::Block;
using LIBC_NAMESPACE::ConfigurableFreeStoreImpl;
using LIBC_NAMESPACE::FreeStoreConfig;
using LIBC_NAMESPACE::IndexType;
using LIBC_NAMESPACE::SearchPreference;
using LIBC_NAMESPACE::cpp::byte;

constexpr size_t BITS_PER_ENTRY =
    LIBC_NAMESPACE::cpp::numeric_limits<uintptr_t>::digits;
constexpr size_t NUM_TABLE_ENTRIES = 192 / BITS_PER_ENTRY;

// 1. BestFit TLSF store config
using BestFitTLSFStore = ConfigurableFreeStoreImpl<
    FreeStoreConfig<
        32, 3, 2, NUM_TABLE_ENTRIES,
        IndexType::LinearList, SearchPreference::BestFit,
        IndexType::LinearList, SearchPreference::BestFit
    >
>;

// 2. OverSized TLSF store config (standard TLSF)
using OverSizedTLSFStore = ConfigurableFreeStoreImpl<
    FreeStoreConfig<
        32, 3, 2, NUM_TABLE_ENTRIES,
        IndexType::LinearList, SearchPreference::OverSized,
        IndexType::LinearList, SearchPreference::OverSized
    >
>;

// 3. Large Trie store config
using LargeTrieStore = ConfigurableFreeStoreImpl<
    FreeStoreConfig<
        32, 3, 2, NUM_TABLE_ENTRIES,
        IndexType::LinearList, SearchPreference::BestFit,
        IndexType::Trie, SearchPreference::BestFit
    >
>;

// 4. Small Trie store config
using SmallTrieStore = ConfigurableFreeStoreImpl<
    FreeStoreConfig<
        32, 3, 2, NUM_TABLE_ENTRIES,
        IndexType::Trie, SearchPreference::BestFit,
        IndexType::Trie, SearchPreference::BestFit
    >
>;

TEST(LlvmLibcConfigurableFreeStoreTest, BestFitVsOverSizedPreference) {
  alignas(Block::MIN_ALIGN) byte buf1[4096];
  auto result1 = Block::init(buf1);
  ASSERT_TRUE(result1.has_value());
  Block *block = *result1;
  block->mark_free();

  // Split to get block1 (1120 B) and block2 (2500 B).
  auto split_res = block->split(1120);
  ASSERT_TRUE(split_res.has_value());
  Block *block1 = block;
  Block *block2 = *split_res;

  auto split_res2 = block2->split(2500);
  ASSERT_TRUE(split_res2.has_value());

  block1->mark_free();
  block2->mark_free();

  // Configuration A: OverSized preference (standard TLSF)
  {
    OverSizedTLSFStore store;
    store.insert(block1);
    store.insert(block2);

    // OverSized preference finds the first oversized bin where all blocks fit.
    // Index for 1050 is 32 (range [1024, 1279]).
    // Index for block1 (1120 B) is 32 (range [1024, 1279]).
    // Index for block2 (2500 B) is 36 (range [2048, 2559]).
    // Oversized search checks bit index > 32, so it finds index 36 first.
    // It returns block2 (2500 B), which is larger and guaranteed to fit.
    Block *removed = store.find_and_remove_fit(1050);
    EXPECT_EQ(removed, block2);
  }

  // Configuration B: BestFit preference
  {
    BestFitTLSFStore store;
    store.insert(block1);
    store.insert(block2);

    // BestFit preference checks the exact mapped bin (32) first.
    // It scans the list at index 32 and finds block1 (1120 B) which fits 1050 B.
    // So it returns block1, which is a much closer fit than block2.
    Block *removed = store.find_and_remove_fit(1050);
    EXPECT_EQ(removed, block1);
  }
}

TEST(LlvmLibcConfigurableFreeStoreTest, LargeTrieConfiguration) {
  LargeTrieStore store;

  alignas(Block::MIN_ALIGN) byte buf[8192];
  auto result = Block::init(buf);
  ASSERT_TRUE(result.has_value());
  Block *block = *result;
  block->mark_free();

  // Split into multiple blocks:
  // block1: inner size ~56 (small list)
  auto split1 = block->split(56);
  ASSERT_TRUE(split1.has_value());
  Block *block1 = block;
  block1->mark_free();

  // block2: inner size ~80 (small list)
  Block *rem1 = *split1;
  auto split2 = rem1->split(80);
  ASSERT_TRUE(split2.has_value());
  Block *block2 = rem1;
  block2->mark_free();

  // block3: inner size ~1200 (large trie)
  Block *rem2 = *split2;
  auto split3 = rem2->split(1200);
  ASSERT_TRUE(split3.has_value());
  Block *block3 = rem2;
  block3->mark_free();

  // block4: inner size ~2000 (large trie)
  Block *rem3 = *split3;
  auto split4 = rem3->split(2000);
  ASSERT_TRUE(split4.has_value());
  Block *block4 = rem3;
  block4->mark_free();

  Block *block5 = *split4;
  block5->mark_free();

  store.insert(block1);
  store.insert(block2);
  store.insert(block3);
  store.insert(block4);
  store.insert(block5);

  // Small list exact fit
  EXPECT_EQ(store.find_and_remove_fit(block1->inner_size()), block1);
  // Small list best fit
  EXPECT_EQ(store.find_and_remove_fit(70), block2);
  // Large trie best fit
  EXPECT_EQ(store.find_and_remove_fit(1100), block3);
  // Large trie another fit
  EXPECT_EQ(store.find_and_remove_fit(1800), block4);
}

TEST(LlvmLibcConfigurableFreeStoreTest, SmallTrieConfiguration) {
  SmallTrieStore store;

  alignas(Block::MIN_ALIGN) byte buf[4096];
  auto result = Block::init(buf);
  ASSERT_TRUE(result.has_value());
  Block *block = *result;
  block->mark_free();

  // We want to test Bin 1 (size [32, 64)) and Bin 2 (size [64, 96)).
  // Required size for Trie (64-bit) is 48.
  // Bin 1 min is 32 < 48 -> List.
  // Bin 2 min is 64 >= 48 -> Trie.

  // Allocate blocks for Bin 1: Block A (40 B payload), Block B (48 B payload)
  auto split_a = block->split(40);
  ASSERT_TRUE(split_a.has_value());
  Block *block_a = block;
  block_a->mark_free();

  Block *rem1 = *split_a;
  auto split_b = rem1->split(48);
  ASSERT_TRUE(split_b.has_value());
  Block *block_b = rem1;
  block_b->mark_free();

  // Allocate blocks for Bin 2: Block C (80 B payload), Block D (72 B payload)
  Block *rem2 = *split_b;
  auto split_c = rem2->split(80);
  ASSERT_TRUE(split_c.has_value());
  Block *block_c = rem2;
  block_c->mark_free();

  Block *rem3 = *split_c;
  auto split_d = rem3->split(72);
  ASSERT_TRUE(split_d.has_value());
  Block *block_d = rem3;
  block_d->mark_free();

  Block *block_rem = *split_d;
  block_rem->mark_free();

  // Test Bin 1 (List - First Fit):
  // Insert B then A.
  store.insert(block_b);
  store.insert(block_a);

  // Request 35. B (48) and A (40) both fit.
  EXPECT_EQ(store.find_and_remove_fit(35), block_b);
  EXPECT_EQ(store.find_and_remove_fit(35), block_a);

  // Test Bin 2 (Trie - Best Fit):
  // Insert C (80) then D (72).
  store.insert(block_c);
  store.insert(block_d);

  // Request 70. C (80) and D (72) both fit.
  EXPECT_EQ(store.find_and_remove_fit(70), block_d);
  EXPECT_EQ(store.find_and_remove_fit(70), block_c);
}
