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
#include "tlsf_table.h"

namespace LIBC_NAMESPACE_DECL {

/// Configuration for TLSFFreeStore.
template <size_t UNIT_SIZE_VAL, size_t STEP_SIZE_BITS_VAL,
          size_t NUM_STEP_BITS_VAL, size_t NUM_TABLE_ENTRIES_VAL>
struct TLSFFreeStoreConfig {
  static constexpr size_t UNIT_SIZE = UNIT_SIZE_VAL;
  static constexpr size_t STEP_SIZE_BITS = STEP_SIZE_BITS_VAL;
  static constexpr size_t NUM_STEP_BITS = NUM_STEP_BITS_VAL;
  static constexpr size_t NUM_TABLE_ENTRIES = NUM_TABLE_ENTRIES_VAL;
};

/// A best-fit store of variously-sized free blocks. Blocks can be inserted and
/// removed in logarithmic time.
template <typename CONFIG> class TLSFFreeStoreImpl {
  friend class FreeListHeap;

public:
  using Table = TLSFTable<FreeList, CONFIG::UNIT_SIZE, CONFIG::STEP_SIZE_BITS,
                          CONFIG::NUM_STEP_BITS, CONFIG::NUM_TABLE_ENTRIES>;

  LIBC_INLINE TLSFFreeStoreImpl() = default;
  TLSFFreeStoreImpl(const TLSFFreeStoreImpl &other) = delete;
  TLSFFreeStoreImpl &operator=(const TLSFFreeStoreImpl &other) = delete;

  /// Sets the range of possible block sizes. This can only be called when the
  /// trie is empty.
  LIBC_INLINE void set_range(FreeTrie::SizeRange range) {
    large_trie.set_range(range);
  }

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
  static constexpr size_t MIN_OUTER_SIZE = align_up(
      BlockRef::HEADER_SIZE + sizeof(FreeList::Node), BlockRef::MIN_ALIGN);
  static constexpr size_t MIN_LARGE_OUTER_SIZE = align_up(
      BlockRef::HEADER_SIZE + sizeof(FreeTrie::Node), BlockRef::MIN_ALIGN);
  static constexpr size_t NUM_SMALL_SIZES =
      (MIN_LARGE_OUTER_SIZE - MIN_OUTER_SIZE) / BlockRef::MIN_ALIGN;

  LIBC_INLINE static bool too_small(BlockRef block) {
    return block.outer_size() < MIN_OUTER_SIZE;
  }
  LIBC_INLINE static bool is_small(BlockRef block) {
    return block.outer_size() < MIN_LARGE_OUTER_SIZE;
  }

  LIBC_INLINE FreeList &small_list(BlockRef block);
  LIBC_INLINE FreeList *find_best_small_fit(size_t size);

  cpp::array<FreeList, NUM_SMALL_SIZES> small_lists;
  FreeTrie large_trie;
  Table table;
};

template <typename CONFIG>
LIBC_INLINE void TLSFFreeStoreImpl<CONFIG>::insert(BlockRef block) {
  if (too_small(block))
    return;
  if (is_small(block))
    small_list(block).push(block);
  else
    large_trie.push(block);
}

template <typename CONFIG>
LIBC_INLINE void TLSFFreeStoreImpl<CONFIG>::remove(BlockRef block) {
  if (too_small(block))
    return;
  if (is_small(block)) {
    small_list(block).remove(
        reinterpret_cast<FreeList::Node *>(block.usable_space()));
  } else {
    large_trie.remove(reinterpret_cast<FreeTrie::Node *>(block.usable_space()));
  }
}

template <typename CONFIG>
LIBC_INLINE BlockRef TLSFFreeStoreImpl<CONFIG>::remove_best_fit(size_t size) {
  if (FreeList *list = find_best_small_fit(size)) {
    BlockRef block = list->front();
    list->pop();
    return block;
  }
  if (FreeTrie::Node *best_fit = large_trie.find_best_fit(size)) {
    BlockRef block = best_fit->block();
    large_trie.remove(best_fit);
    return block;
  }
  return BlockRef();
}

template <typename CONFIG>
LIBC_INLINE FreeList &TLSFFreeStoreImpl<CONFIG>::small_list(BlockRef block) {
  LIBC_ASSERT(is_small(block) && "only legal for small blocks");
  return small_lists[(block.outer_size() - MIN_OUTER_SIZE) /
                     BlockRef::MIN_ALIGN];
}

template <typename CONFIG>
LIBC_INLINE FreeList *
TLSFFreeStoreImpl<CONFIG>::find_best_small_fit(size_t size) {
  for (FreeList &list : small_lists)
    if (!list.empty() && list.size() >= size)
      return &list;
  return nullptr;
}

using DefaultFreeStoreConfig =
    TLSFFreeStoreConfig<BlockRef::MIN_ALIGN, 2, 2, 2>;
using FreeStore = TLSFFreeStoreImpl<DefaultFreeStoreConfig>;

} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_FREESTORE_H
