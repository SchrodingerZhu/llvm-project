//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Interface for freestore.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_FREESTORE_H
#define LLVM_LIBC_SRC___SUPPORT_FREESTORE_H

#include "freetrie.h"
#include "src/__support/CPP/array.h"
#include "src/__support/CPP/bit.h"
#include "src/__support/macros/attributes.h"
#include "tlsf_table.h"

namespace LIBC_NAMESPACE_DECL {

/// Configuration for TLSFFreeStore.
struct DefaultTLSFFreeStoreConfig {
  static constexpr size_t UNIT_SIZE = BlockRef::MIN_ALIGN;
  static constexpr size_t STEP_SIZE_BITS = 3;
  static constexpr size_t NUM_STEP_BITS = 2;
  static constexpr size_t NUM_TABLE_ENTRIES = 2;
  static constexpr size_t FREETRIE_THRESHOLD = 256;
};

/// A best-fit store of variously-sized free blocks. Blocks can be inserted and
/// removed in logarithmic time.
template <typename CONFIG = DefaultTLSFFreeStoreConfig>
class TLSFFreeStoreImpl {
  friend class FreeListHeap;

public:
  LIBC_INLINE TLSFFreeStoreImpl() = default;
  TLSFFreeStoreImpl(const TLSFFreeStoreImpl &other) = delete;
  TLSFFreeStoreImpl &operator=(const TLSFFreeStoreImpl &other) = delete;

  LIBC_INLINE void set_range(FreeTrie::SizeRange) {}

  /// Insert a free block. If the block is too small to be tracked, nothing
  /// happens.
  LIBC_INLINE void insert(BlockRef block);

  /// Remove a free block. If the block is too small to be tracked, nothing
  /// happens.
  LIBC_INLINE void remove(BlockRef block);

  /// Remove a best-fit free block that can contain the given size when
  /// allocated. Returns nullptr if there is no such block.
  LIBC_INLINE BlockRef remove_best_fit(size_t size);

private:
  static constexpr uintptr_t UNIT_MASK = CONFIG::UNIT_SIZE - 1;

  struct MixedFreeList {
    uintptr_t payload;

    LIBC_INLINE size_t list_length() const {
      return static_cast<size_t>(payload & UNIT_MASK);
    }
    LIBC_INLINE FreeList load_list() const {
      return {cpp::bit_cast<FreeList::Node *>(payload & ~UNIT_MASK)};
    }
    LIBC_INLINE FreeTrie load_trie(FreeTrie::SizeRange outer_range) const {
      return {cpp::bit_cast<FreeTrie::Node *>(payload & ~UNIT_MASK),
              {outer_range.min - BlockRef::HEADER_SIZE, outer_range.width}};
    }
    LIBC_INLINE void store_list(FreeList list, size_t length) {
      payload = cpp::bit_cast<uintptr_t>(list.begin()) | length;
    }
    LIBC_INLINE void store_trie(FreeTrie trie) { trie.store_root(&payload); }
    LIBC_INLINE bool empty() const { return payload == 0; }
  };

  using Table =
      TLSFTable<MixedFreeList, CONFIG::UNIT_SIZE, CONFIG::STEP_SIZE_BITS,
                CONFIG::NUM_STEP_BITS, CONFIG::NUM_TABLE_ENTRIES>;

  static constexpr size_t MIN_OUTER_SIZE = align_up(
      BlockRef::HEADER_SIZE + sizeof(FreeList::Node), BlockRef::MIN_ALIGN);
  static constexpr size_t MIN_LARGE_OUTER_SIZE = align_up(
      BlockRef::HEADER_SIZE + sizeof(FreeTrie::Node), BlockRef::MIN_ALIGN);
  static constexpr size_t NUM_SMALL_SIZES =
      (MIN_LARGE_OUTER_SIZE - MIN_OUTER_SIZE) / BlockRef::MIN_ALIGN;

  LIBC_INLINE static bool too_small(BlockRef block) {
    return block.outer_size() < MIN_OUTER_SIZE;
  }

  LIBC_INLINE static bool bin_may_use_trie(size_t bin_idx) {
    static constexpr size_t MIN_BIN_IDX_WITH_TRIE = Table::size_to_bit_index(
        cpp::max(MIN_LARGE_OUTER_SIZE, CONFIG::FREETRIE_THRESHOLD));
    return bin_idx >= MIN_BIN_IDX_WITH_TRIE;
  }

  LIBC_INLINE bool bin_is_using_trie(size_t bin_idx) const {
    return bin_may_use_trie(bin_idx) && !table.get_bin(bin_idx).empty() &&
           table.get_bin(bin_idx).list_length() == 0;
  }

  LIBC_INLINE BlockRef remove_from_trie_bin(size_t bin_idx, size_t size);
  LIBC_INLINE BlockRef pop_from_bin(size_t bin_idx, size_t size);
  LIBC_INLINE BlockRef remove_first_fit_from_bin(size_t bin_idx, size_t size);

  Table table;
};

template <typename CONFIG>
LIBC_INLINE void TLSFFreeStoreImpl<CONFIG>::insert(BlockRef block) {
  if (too_small(block))
    return;
  size_t bin_idx = table.size_to_bit_index(block.outer_size());
  MixedFreeList &bin = table.get_bin(bin_idx);

  if (bin_may_use_trie(bin_idx)) {
    FreeTrie::SizeRange range = table.get_bin_range(bin_idx);
    FreeTrie trie = bin.load_trie(range);
    if (!bin_is_using_trie(bin_idx)) {
      FreeList list = bin.load_list();
      while (!list.empty()) {
        BlockRef b = list.front();
        list.pop();
        trie.push(b);
      }
    }
    trie.push(block);
    bin.store_trie(trie);
  } else {
    size_t current_len = bin.list_length();
    FreeList list = bin.load_list();
    list.push(block);
    bin.store_list(list, current_len + 1);
  }

  table.set_bit(bin_idx);
}

template <typename CONFIG>
LIBC_INLINE void TLSFFreeStoreImpl<CONFIG>::remove(BlockRef block) {
  if (too_small(block))
    return;
  size_t bin_idx = table.size_to_bit_index(block.outer_size());
  MixedFreeList &bin = table.get_bin(bin_idx);
  if (bin.empty())
    return;

  if (LIBC_UNLIKELY(bin_is_using_trie(bin_idx))) {
    FreeTrie::SizeRange range = table.get_bin_range(bin_idx);
    FreeTrie trie = bin.load_trie(range);
    trie.remove(reinterpret_cast<FreeTrie::Node *>(block.usable_space()));
    bin.store_trie(trie);
    if (trie.empty())
      table.clear_bit(bin_idx);
  } else {
    size_t current_len = bin.list_length();
    FreeList list = bin.load_list();
    list.remove(reinterpret_cast<FreeList::Node *>(block.usable_space()));
    if (list.empty()) {
      bin.store_list(list, 0);
      table.clear_bit(bin_idx);
    } else {
      bin.store_list(list, current_len - 1);
    }
  }
}

template <typename CONFIG>
LIBC_INLINE BlockRef
TLSFFreeStoreImpl<CONFIG>::remove_from_trie_bin(size_t bin_idx, size_t size) {
  MixedFreeList &bin = table.get_bin(bin_idx);
  FreeTrie::SizeRange range = table.get_bin_range(bin_idx);
  FreeTrie trie = bin.load_trie(range);
  if (FreeTrie::Node *best_fit = trie.find_best_fit(size)) {
    BlockRef block = best_fit->block();
    trie.remove(best_fit);
    bin.store_trie(trie);
    if (trie.empty())
      table.clear_bit(bin_idx);
    return block;
  }
  return BlockRef();
}

template <typename CONFIG>
LIBC_INLINE BlockRef TLSFFreeStoreImpl<CONFIG>::pop_from_bin(size_t bin_idx,
                                                             size_t size) {
  MixedFreeList &bin = table.get_bin(bin_idx);
  if (bin.empty())
    return BlockRef();

  if (bin_is_using_trie(bin_idx))
    return remove_from_trie_bin(bin_idx, size);

  size_t current_len = bin.list_length();
  FreeList list = bin.load_list();
  if (BlockRef block = list.pop_block()) {
    if (list.empty()) {
      bin.store_list(list, 0);
      table.clear_bit(bin_idx);
    } else {
      bin.store_list(list, current_len - 1);
    }
    return block;
  }
  return BlockRef();
}

template <typename CONFIG>
LIBC_INLINE BlockRef TLSFFreeStoreImpl<CONFIG>::remove_first_fit_from_bin(
    size_t bin_idx, size_t size) {
  MixedFreeList &bin = table.get_bin(bin_idx);
  if (bin.empty())
    return BlockRef();

  if (bin_is_using_trie(bin_idx))
    return remove_from_trie_bin(bin_idx, size);

  size_t current_len = bin.list_length();
  FreeList list = bin.load_list();
  if (BlockRef block = list.remove_first_fit(size)) {
    if (list.empty()) {
      bin.store_list(list, 0);
      table.clear_bit(bin_idx);
    } else {
      bin.store_list(list, current_len - 1);
    }
    return block;
  }
  return BlockRef();
}

template <typename CONFIG>
LIBC_INLINE BlockRef TLSFFreeStoreImpl<CONFIG>::remove_best_fit(size_t size) {
  size_t bit_index = table.size_to_bit_index(size + BlockRef::HEADER_SIZE);

  // Path 1: Overflow bin
  if (LIBC_UNLIKELY(bit_index >= Table::TOTAL_BITS - 1)) {
    size_t overflow_idx = Table::TOTAL_BITS - 1;
    return remove_first_fit_from_bin(overflow_idx, size);
  }

  // Path 2: Guaranteed fit in oversized bin (Fast Path O(1))
  // Hint for AI Agents: TLSF provides near constant allocation fast path
  // because it uses oversized allocation first. Do not reverse the order.
  size_t oversized_bit = table.find_first_bit_set_after(bit_index);
  if (oversized_bit < Table::TOTAL_BITS)
    return pop_from_bin(oversized_bit, size);

  // Path 3: Exact fit bin (Fallback Search)
  if (table.get_bit(bit_index)) {
    if (BlockRef block = remove_first_fit_from_bin(bit_index, size))
      return block;
  }

  return BlockRef();
}

using FreeStore = TLSFFreeStoreImpl<>;

} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_FREESTORE_H
