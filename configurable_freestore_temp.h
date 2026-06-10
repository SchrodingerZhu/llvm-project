//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// This file contains the definition of the configurable free block store.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_CONFIGURABLE_FREESTORE_H
#define LLVM_LIBC_SRC___SUPPORT_CONFIGURABLE_FREESTORE_H

#include "hdr/stdint_proxy.h"
#include "hdr/types/size_t.h"
#include "src/__support/CPP/array.h"
#include "src/__support/CPP/bit.h"
#include "src/__support/CPP/limits.h"
#include "src/__support/CPP/type_traits/conditional.h"
#include "src/__support/block.h"
#include "src/__support/freelist.h"
#include "src/__support/freetrie.h"
#include "src/__support/macros/config.h"
#include "src/__support/macros/optimization.h"

namespace LIBC_NAMESPACE_DECL {

enum class IndexType {
  LinearList,
  Trie,
};

enum class SearchPreference {
  OverSized,
  BestFit,
};

union BinContainer {
  FreeList list;
  FreeTrie trie;

  LIBC_INLINE constexpr BinContainer() : list(nullptr) {}
};

template <size_t UNIT_SIZE_VAL, size_t STEP_SIZE_BITS_VAL,
          size_t NUM_STEP_BITS_VAL, size_t NUM_TABLE_ENTRIES_VAL,
          IndexType SMALL_INDEX_VAL = IndexType::LinearList,
          SearchPreference SMALL_PREF_VAL = SearchPreference::OverSized,
          IndexType LARGE_INDEX_VAL = IndexType::LinearList,
          SearchPreference LARGE_PREF_VAL = SearchPreference::OverSized>
struct FreeStoreConfig {
  static constexpr size_t UNIT_SIZE = UNIT_SIZE_VAL;
  static constexpr size_t STEP_SIZE_BITS = STEP_SIZE_BITS_VAL;
  static constexpr size_t NUM_STEP_BITS = NUM_STEP_BITS_VAL;
  static constexpr size_t NUM_TABLE_ENTRIES = NUM_TABLE_ENTRIES_VAL;

  static constexpr IndexType SMALL_INDEX = SMALL_INDEX_VAL;
  static constexpr SearchPreference SMALL_PREF = SMALL_PREF_VAL;
  static constexpr IndexType LARGE_INDEX = LARGE_INDEX_VAL;
  static constexpr SearchPreference LARGE_PREF = LARGE_PREF_VAL;
};

template <typename CONFIG> class ConfigurableFreeStoreImpl {
protected:
  static_assert(cpp::has_single_bit(CONFIG::UNIT_SIZE),
                "unit size must be a power of two");
  static_assert(CONFIG::NUM_TABLE_ENTRIES > 0,
                "the lookup table must have at least one entry");

  static constexpr size_t UNIT_SIZE = CONFIG::UNIT_SIZE;
  static constexpr size_t STEP_SIZE_BITS = CONFIG::STEP_SIZE_BITS;
  static constexpr size_t NUM_STEP_BITS = CONFIG::NUM_STEP_BITS;

  static constexpr size_t STEP_SIZE = size_t(1) << STEP_SIZE_BITS;
  static constexpr size_t NUM_STEPS = size_t(1) << CONFIG::NUM_STEP_BITS;
  static constexpr size_t EXP_BASE = STEP_SIZE * NUM_STEPS;
  static constexpr int UNIT_SIZE_LOG2 = cpp::bit_width(UNIT_SIZE) - 1;
  static constexpr int EXP_BASE_LOG2 = STEP_SIZE_BITS + NUM_STEP_BITS;
  static constexpr size_t BITS_PER_ENTRY =
      cpp::numeric_limits<uintptr_t>::digits;
  static constexpr size_t TOTAL_BITS =
      CONFIG::NUM_TABLE_ENTRIES * BITS_PER_ENTRY;


  // The small store always uses FreeList, so the minimum outer size is based on
  // FreeList::Node.
  static constexpr size_t MIN_OUTER_SIZE =
      align_up(sizeof(Block) + sizeof(FreeList::Node), Block::MIN_ALIGN);

  // If LARGE_INDEX is configured to use Trie, verify that the smallest block in
  // the large bins (which starts at EXP_BASE * UNIT_SIZE) has enough space to
  // hold a FreeTrie::Node.
  static constexpr size_t LARGE_START_SIZE = EXP_BASE * UNIT_SIZE;

  static_assert(CONFIG::LARGE_INDEX == IndexType::LinearList ||
                    LARGE_START_SIZE >= sizeof(Block) + sizeof(FreeTrie::Node),
                "large size threshold is too small to accommodate trie nodes");

public:
  LIBC_INLINE ConfigurableFreeStoreImpl() = default;
  LIBC_INLINE
  ConfigurableFreeStoreImpl(const ConfigurableFreeStoreImpl &other) = delete;
  LIBC_INLINE ConfigurableFreeStoreImpl &
  operator=(const ConfigurableFreeStoreImpl &other) = delete;

  LIBC_INLINE void set_range(FreeTrie::SizeRange) {
    // Range can be computed per-bin, but we keep the setter for API
    // compatibility if needed.
  }

  LIBC_INLINE void insert(Block *block);
  LIBC_INLINE void remove(Block *block);
  LIBC_INLINE Block *find_and_remove_fit(size_t size);

protected:
  LIBC_INLINE static bool too_small(Block *block) {
    return block->outer_size() < MIN_OUTER_SIZE;
  }

  LIBC_INLINE static constexpr bool use_trie(size_t index) {
    if constexpr (CONFIG::SMALL_INDEX == IndexType::Trie) {
      if (index < EXP_BASE) {
        return index * UNIT_SIZE >=
               sizeof(FreeTrie::Node) + Block::PREV_FIELD_SIZE;
      }
    }
    if constexpr (CONFIG::LARGE_INDEX == IndexType::Trie) {
      if (index >= EXP_BASE) {
        return true;
      }
    }
    return false;
  }

  LIBC_INLINE static constexpr FreeTrie::SizeRange get_bin_range(size_t index) {
    if (index < EXP_BASE) {
      return {index * UNIT_SIZE, UNIT_SIZE};
    } else {
      size_t large_index = index - EXP_BASE;
      size_t exp_index = large_index >> CONFIG::NUM_STEP_BITS;
      size_t linear_index = large_index & (NUM_STEPS - 1);

      size_t row_base = (EXP_BASE * UNIT_SIZE) << exp_index;
      size_t step_size = row_base >> CONFIG::NUM_STEP_BITS;
      size_t min_size = row_base + linear_index * step_size;

      if (index == TOTAL_BITS - 1) {
        size_t target_width = (sizeof(size_t) == 8) ? (1ULL << 48) : (1ULL << 30);
        if (min_size < target_width) {
          step_size = target_width;
        }
      }
      return {min_size, step_size};
    }
  }

  LIBC_INLINE static constexpr size_t size_to_bit_index(size_t size) {
    if (size <= (EXP_BASE << UNIT_SIZE_LOG2))
      return size >> UNIT_SIZE_LOG2;

    size_t size_ilog2 = static_cast<size_t>(cpp::bit_width(size) - 1);
    size_t exp_offset = (size_ilog2 - UNIT_SIZE_LOG2 - EXP_BASE_LOG2 - 1)
                        << CONFIG::NUM_STEP_BITS;
    size_t step_index = size >> (size_ilog2 - CONFIG::NUM_STEP_BITS);
    size_t index = EXP_BASE + exp_offset + step_index;

    return index < TOTAL_BITS ? index : TOTAL_BITS - 1;
  }

  // Storage: a single flat array of unified BinContainers
  cpp::array<BinContainer, TOTAL_BITS> free_store_bins{};
  cpp::array<uintptr_t, CONFIG::NUM_TABLE_ENTRIES> lookup_table{};

  // Bit Manipulation
  LIBC_INLINE void set_bit(size_t index) {
    size_t entry_index = index / BITS_PER_ENTRY;
    size_t bit_offset = index % BITS_PER_ENTRY;
    lookup_table[entry_index] |= uintptr_t(1) << bit_offset;
  }

  LIBC_INLINE void clear_bit(size_t index) {
    size_t entry_index = index / BITS_PER_ENTRY;
    size_t bit_offset = index % BITS_PER_ENTRY;
    lookup_table[entry_index] &= ~(uintptr_t(1) << bit_offset);
  }

  LIBC_INLINE bool get_bit(size_t index) const {
    size_t entry_index = index / BITS_PER_ENTRY;
    size_t bit_offset = index % BITS_PER_ENTRY;
    return (lookup_table[entry_index] & (uintptr_t(1) << bit_offset)) != 0;
  }

  LIBC_INLINE size_t find_first_bit_set_after(size_t index) const {
    if (index >= TOTAL_BITS - 1)
      return TOTAL_BITS;

    size_t target_index = index + 1;
    size_t start_entry = target_index / BITS_PER_ENTRY;
    size_t bit_offset = target_index % BITS_PER_ENTRY;

    uintptr_t value = lookup_table[start_entry] & (~uintptr_t(0) << bit_offset);
    if (value != 0)
      return start_entry * BITS_PER_ENTRY +
             static_cast<size_t>(cpp::countr_zero(value));

    for (size_t i = start_entry + 1; i < CONFIG::NUM_TABLE_ENTRIES; ++i) {
      value = lookup_table[i];
      if (value != 0)
        return i * BITS_PER_ENTRY +
               static_cast<size_t>(cpp::countr_zero(value));
    }
    return TOTAL_BITS;
  }

  LIBC_INLINE Block *remove_first_fit_in_list(FreeList &list, size_t index,
                                              size_t size) {
    FreeList::Node *begin_node = list.begin();
    if (begin_node == nullptr)
      return nullptr;

    FreeList::Node *cur = begin_node;
    do {
      if (cur->size() >= size) {
        list.remove(cur);
        if (list.empty())
          clear_bit(index);
        return cur->block();
      }
      cur = cur->next_node();
    } while (cur != begin_node);

    return nullptr;
  }

  LIBC_INLINE Block *remove_first_fit_in_bin(size_t index, size_t size) {
    if (use_trie(index)) {
      if (FreeTrie::Node *node = free_store_bins[index].trie.find_best_fit(
              size, get_bin_range(index))) {
        Block *block = node->block();
        free_store_bins[index].trie.remove(node);
        if (free_store_bins[index].trie.empty())
          clear_bit(index);
        return block;
      }
      return nullptr;
    }
    return remove_first_fit_in_list(free_store_bins[index].list, index, size);
  }

  // Exposed for testing
  LIBC_INLINE Block *remove_first_fit_in_list(size_t index, size_t size) {
    return remove_first_fit_in_bin(index, size);
  }
};

template <typename CONFIG>
LIBC_INLINE void ConfigurableFreeStoreImpl<CONFIG>::insert(Block *block) {
  if (too_small(block))
    return;

  size_t index = size_to_bit_index(block->inner_size());
  if (use_trie(index)) {
    free_store_bins[index].trie.push(block, get_bin_range(index));
  } else {
    free_store_bins[index].list.push(block);
  }
  set_bit(index);
}

template <typename CONFIG>
LIBC_INLINE void ConfigurableFreeStoreImpl<CONFIG>::remove(Block *block) {
  if (too_small(block))
    return;

  size_t index = size_to_bit_index(block->inner_size());
  if (use_trie(index)) {
    free_store_bins[index].trie.remove(
        reinterpret_cast<FreeTrie::Node *>(block->usable_space()));
  } else {
    free_store_bins[index].list.remove(
        reinterpret_cast<FreeList::Node *>(block->usable_space()));
  }

  bool is_empty = false;
  if (use_trie(index)) {
    is_empty = free_store_bins[index].trie.empty();
  } else {
    is_empty = free_store_bins[index].list.empty();
  }

  if (is_empty)
    clear_bit(index);
}

template <typename CONFIG>
LIBC_INLINE Block *
ConfigurableFreeStoreImpl<CONFIG>::find_and_remove_fit(size_t size) {
  size_t index = size_to_bit_index(size);
  if (LIBC_UNLIKELY(index >= TOTAL_BITS - 1)) {
    return remove_first_fit_in_bin(TOTAL_BITS - 1, size);
  }

  if constexpr (CONFIG::LARGE_PREF == SearchPreference::BestFit) {
    if (get_bit(index)) {
      if (Block *block = remove_first_fit_in_bin(index, size))
        return block;
    }

    size_t next_bit = find_first_bit_set_after(index);
    if (next_bit < TOTAL_BITS) {
      return remove_first_fit_in_bin(next_bit, size);
    }
  } else {
    size_t next_bit = find_first_bit_set_after(index);
    if (next_bit < TOTAL_BITS) {
      return remove_first_fit_in_bin(next_bit, size);
    }

    if (get_bit(index)) {
      if (Block *block = remove_first_fit_in_bin(index, size))
        return block;
    }
  }

  return nullptr;
}

using TrieFreeStore = ConfigurableFreeStoreImpl<FreeStoreConfig<
    /*UNIT_SIZE=*/Block::MIN_ALIGN,
    /*STEP_SIZE_BITS=*/2,
    /*NUM_STEP_BITS=*/0,
    /*NUM_TABLE_ENTRIES=*/1,
    /*SMALL_INDEX=*/IndexType::Trie,
    /*SMALL_PREF=*/SearchPreference::BestFit,
    /*LARGE_INDEX=*/IndexType::Trie,
    /*LARGE_PREF=*/SearchPreference::BestFit>>;

} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_CONFIGURABLE_FREESTORE_H
