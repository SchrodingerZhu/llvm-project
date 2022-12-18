//===-- SwissTable ----------------------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SUPPORT_SWISSTABLE_H
#define LLVM_LIBC_SUPPORT_SWISSTABLE_H

#include "src/__support/CPP/array.h"
#include "src/__support/builtin_wrappers.h"
#include "src/__support/common.h"
#include "src/__support/swisstable/dispatch.h"
#include "src/__support/swisstable/safe_mem_size.h"
#include "src/string/memcpy.h"
#include "src/string/memory_utils/memcpy_implementations.h"
#include "src/string/memset.h"
#include <stdlib.h>

namespace __llvm_libc::internal::swisstable {
static inline bool is_full(CtrlWord ctrl) { return (ctrl & 0x80) == 0; }
static inline bool special_is_empty(CtrlWord ctrl) {
  return (ctrl & 0x01) != 0;
}
static inline size_t h1(HashWord hash) { return static_cast<size_t>(hash); }
static inline CtrlWord h2(HashWord hash) {
  static constexpr size_t HASH_LENGTH =
      sizeof(size_t) < sizeof(HashWord) ? sizeof(size_t) : sizeof(HashWord);
  // We want the top 7 bits of the h1.
  return (hash >> (HASH_LENGTH * 8 - 7)) & 0x7f;
}

struct ProbeSequence {
  size_t position;
  size_t stride;

  void move_next(size_t bucket_mask) {
    stride += sizeof(Group);
    position += stride;
    position &= bucket_mask;
  }
};

static inline size_t next_power_of_two(size_t val) {
  size_t idx = __llvm_libc::unsafe_clz(val - 1);
  return 1ull << ((8ull * sizeof(size_t)) - idx);
}

static inline size_t capacity_to_buckets(size_t cap) {
  if (cap < 8) {
    return (cap < 4) ? 4 : 8;
  }
  return next_power_of_two(cap * 8);
}

static inline size_t bucket_mask_to_capacity(size_t bucket_mask) {
  if (bucket_mask < 8) {
    return bucket_mask;
  } else {
    return (bucket_mask + 1) / 8 * 7;
  }
}

template <typename T, size_t SIZE> struct ConstantStorage {
  alignas(Group) T data[SIZE];
  constexpr ConstantStorage(CtrlWord val) {
    for (size_t i = 0; i < SIZE; ++i) {
      data[i] = val;
    }
  }
};

constexpr static inline ConstantStorage<CtrlWord, sizeof(Group)>
    CONST_EMPTY_GROUP = {EMPTY};

// The heap memory layout for N buckets of size S:
//
//             =========================================
// Fields:     | buckets |  ctrl bytes |     group     |
//             -----------------------------------------
// Size:       |   N*S   |      N      | sizeof(Group) |
//             =========================================
//                       ^
//                       |
//               Store this position in RawTable.
//
// The trailing group part is to make sure we can always load
// a whole group of control bytes.
template <typename T> struct TableLayout {
  // Pick the largest alignment between T and Group.
  static constexpr inline size_t ALIGNMENT = alignof(T) > alignof(Group)
                                                 ? alignof(T)
                                                 : alignof(Group);
  size_t offset;
  size_t size;

  TableLayout(size_t offset, size_t size) : offset(offset), size(size) {}

  // We want to find an aligned boundary, put buckets to its left and
  // put ctrl words to its left. So we just trim the trailing ones from
  // (buckets * sizeof(T) + alignment - 1)
  static TableLayout checked(size_t buckets, bool &valid) {
    valid = true;
    SafeMemSize padded = SafeMemSize{buckets} * SafeMemSize{sizeof(T)} +
                         SafeMemSize{ALIGNMENT - 1};
    valid = valid && padded.valid();
    size_t offset = static_cast<size_t>(padded) & ~(ALIGNMENT - 1);

    SafeMemSize safe_size =
        SafeMemSize{offset} + SafeMemSize{buckets} + SafeMemSize{sizeof(Group)};
    valid = valid && safe_size.valid();
    size_t size = static_cast<size_t>(safe_size);

    return {offset, size};
  }

  static TableLayout unchecked(size_t buckets) {
    size_t offset = (buckets * sizeof(T) + ALIGNMENT - 1) & ~(ALIGNMENT - 1);
    size_t size = offset + buckets + sizeof(Group);
    return {offset, size};
  }
};

// We will not consider zero-sized type
template <class T> struct Bucket {
  static_assert(sizeof(T) > 1, "zero sized type not allowed");

  T *ptr;

  Bucket(T *base, size_t index) : ptr(base - index) {}

  size_t base_index(T *base) const { return base - ptr; }

  T &operator*() { return ptr[-1]; }

  Bucket operator+(size_t offset) { return {ptr - offset}; }

  bool operator==(const Bucket &other) { return ptr == other.ptr; }

  operator bool() { return ptr != nullptr; }
};

// T should be trivial type.
template <class T, bool ENABLE_DELETION, bool ENABLE_RESIZE> class RawTable {
  using Layout = TableLayout<T>;
  // Bucket size is a power of two
  size_t bucket_mask;
  // Pointer to the control words
  CtrlWord *ctrl;
  // Number of items before growth is required
  size_t growth_left;
  // Number of items
  size_t items;

public:
  static RawTable create_invalid() {
    RawTable table{};
    table.bucket_mask = 0;
    table.ctrl = nullptr;
    table.growth_left = 0;
    table.items = 0;
    return table;
  }

  static RawTable uninitialized(size_t buckets) {
    RawTable table = RawTable::create();
    bool valid;
    Layout layout = Layout::checked(buckets, valid);

    if (unlikely(!valid))
      return table;

    CtrlWord *address =
        static_cast<CtrlWord *>(aligned_alloc(Layout::ALIGNMENT, layout.size));
    if (unlikely(address == nullptr))
      return table;

    table.bucket_mask = buckets - 1;
    table.ctrl = address + layout.offset;
    table.growth_left = bucket_mask_to_capacity(table.bucket_mask);
    return table;
  }

  static RawTable create() {
    RawTable table{};
    table.ctrl = const_cast<CtrlWord *>(&CONST_EMPTY_GROUP.data[0]);
    table.bucket_mask = 0;
    table.growth_left = 0;
    table.items = 0;
    return table;
  }

  static RawTable with_capacity(size_t capacity) {
    if (capacity == 0)
      return create();

    // additional check for capacity overflow
    auto safe_cap = SafeMemSize{capacity};
    safe_cap = safe_cap * SafeMemSize{size_t{8}};
    if (!safe_cap.valid()) {
      return create_invalid();
    }

    size_t buckets = capacity_to_buckets(capacity);
    RawTable table = uninitialized(buckets);
    if (likely(table.ctrl != nullptr))
      memset(reinterpret_cast<char *>(table.ctrl), EMPTY,
             table.num_ctrl_words());
    return table;
  }

  void release() {
    if (!is_empty_singleton()) {
      Layout layout = Layout::unchecked(num_buckets());
      free(ctrl - layout.offset);
    }
    ctrl = nullptr;
  }

  bool is_valid() { return ctrl != nullptr; }

private:
  size_t num_buckets() const { return bucket_mask + 1; }
  size_t num_ctrl_words() const { return num_buckets() + sizeof(Group); }

  T *data_end() const { return reinterpret_cast<T *>(ctrl); }
  T *data_begin() const { return reinterpret_cast<T *>(ctrl) - num_buckets(); }

  Bucket<T> bucket_at(size_t index) const { return {data_end(), index}; }
  size_t bucket_index(const Bucket<T> &bucket) const {
    return bucket.base_index(data_end());
  }

  // Sets a control byte, and possibly also the replicated control byte at
  // the end of the array.
  void set_ctrl(size_t index, CtrlWord value) {
    // Replicate the first sizeof(Group) control bytes at the end of
    // the array without using a branch:
    // - If index >= sizeof(Group) then index == index2.
    // - Otherwise index2 == self.bucket_mask + 1 + index.
    //
    // The very last replicated control byte is never actually read because
    // we mask the initial index for unaligned loads, but we write it
    // anyways because it makes the set_ctrl implementation simpler.
    //
    // If there are fewer buckets than sizeof(Group) then this code will
    // replicate the buckets at the end of the trailing group. For example
    // with 2 buckets and a group size of 4, the control bytes will look
    // like this:
    // =============================================
    // |   Real    |            Replicated         |
    // ---------------------------------------------
    // | [A] | [B] | [EMPTY] | [EMPTY] | [A] | [B] |
    // =============================================

    size_t index2 = ((index - sizeof(Group)) & bucket_mask) + sizeof(Group);
    ctrl[index] = value;
    ctrl[index2] = value;
  }

  ProbeSequence probe_sequence(HashWord hash) const {
    return {h1(hash) & bucket_mask, 0};
  }

  size_t proper_insertion_slot(size_t index) const {
    // In tables smaller than the group width, trailing control
    // bytes outside the range of the table are filled with
    // EMPTY entries. These will unfortunately trigger a
    // match, but once masked may point to a full bucket that
    // is already occupied. We detect this situation here and
    // perform a second scan starting at the beginning of the
    // table. This second scan is guaranteed to find an empty
    // slot (due to the load factor) before hitting the trailing
    // control bytes (containing EMPTY).
    if (unlikely(is_full(ctrl[index]))) {
      return Group::aligned_load(ctrl)
          .mask_empty_or_deleted()
          .lowest_set_bit_nonzero();
    }
    return index;
  }

  size_t find_insert_slot(HashWord hash) const {
    ProbeSequence seq = probe_sequence(hash);
    while (true) {
      Group group = Group::load(&ctrl[seq.position]);
      BitMask empty_slot = group.mask_empty_or_deleted();
      if (empty_slot.any_bit_set()) {
        size_t result =
            (seq.position + empty_slot.lowest_set_bit_nonzero()) & bucket_mask;
        return proper_insertion_slot(result);
      }
      seq.move_next(bucket_mask);
    }
  }

  template <typename Hasher, typename Equal>
  Bucket<T> find_or_insert_with_hash(HashWord hash, const T &value, Equal eq,
                                     Hasher hasher) {
    CtrlWord h2_hash = h2(hash);
    ProbeSequence seq = probe_sequence(hash);
    while (true) {
      Group group = Group::load(&ctrl[seq.position]);
      for (size_t bit : group.match_byte(h2_hash)) {
        size_t index = (seq.position + bit) & bucket_mask;
        auto bucket = bucket_at(index);
        if (likely(eq(*bucket, value)))
          return bucket;
      }

      if constexpr (ENABLE_DELETION) {
        BitMask empty_slot = group.mask_empty();
        if (likely(empty_slot.any_bit_set())) {
          size_t index = find_insert_slot(hash);
          return insert_at(index, hash, value, hasher);
        }
      } else {
        BitMask empty_slot = group.mask_empty_or_deleted();
        if (likely(empty_slot.any_bit_set())) {
          size_t index = (seq.position + empty_slot.lowest_set_bit_nonzero()) &
                         bucket_mask;
          index = proper_insertion_slot(index);
          return insert_at(index, hash, value, hasher);
        }
      }

      seq.move_next(bucket_mask);
    }
  }

  template <typename Equal>
  Bucket<T> find_with_hash(HashWord hash, const T &value, Equal eq) const {
    CtrlWord h2_hash = h2(hash);
    ProbeSequence seq = probe_sequence(hash);
    while (true) {
      Group group = Group::load(&ctrl[seq.position]);
      for (size_t bit : group.match_byte(h2_hash)) {
        size_t index = (seq.position + bit) & bucket_mask;
        auto bucket = bucket_at(index);
        if (likely(eq(*bucket, value)))
          return bucket;
      }

      if constexpr (ENABLE_DELETION) {
        if (likely(group.mask_empty().any_bit_set()))
          return {nullptr, 0};
      } else {
        // Only EMPTY will appear; no need to distingush EMPTY and DELETED.
        if (likely(group.mask_empty_or_deleted().any_bit_set()))
          return {nullptr, 0};
      }

      seq.move_next(bucket_mask);
    }
  }

  void set_ctrl_h2(size_t index, HashWord hash) { set_ctrl(index, h2(hash)); }

  CtrlWord replace_ctrl_h2(size_t index, HashWord hash) {
    CtrlWord prev = ctrl[index];
    set_ctrl_h2(index, hash);
    return prev;
  }

  bool is_bucket_full(size_t index) const { return is_full(ctrl[index]); }

  struct Slot {
    size_t index;
    CtrlWord prev_ctrl;
  };

  Slot prepare_insert_slot(HashWord hash) {
    size_t index = find_insert_slot(hash);
    CtrlWord prev_ctrl = ctrl[index];
    set_ctrl_h2(index, hash);
    return {index, prev_ctrl};
  }

  void prepare_rehash_inplace() {
    // convert full to deleted, deleted to empty s.t. we can use
    // deleted as an indicator for rehash
    for (size_t i = 0; i < num_buckets(); i += sizeof(Group)) {
      Group group = Group::aligned_load(&ctrl[i]);
      Group converted = group.convert_special_to_empty_and_full_to_deleted();
      converted.aligned_store(&ctrl[i]);
    }

    // handle the cases when table size is smaller than group size
    if (num_buckets() < sizeof(Group)) {
      memcpy(&ctrl[sizeof(Group)], &ctrl[0], num_buckets());
    } else {
      inline_memcpy(reinterpret_cast<char *>(&ctrl[num_buckets()]),
                    reinterpret_cast<const char *>(&ctrl[0]), sizeof(Group));
    }
  }

  void record_item_insert_at(size_t index, CtrlWord prev_ctrl, HashWord hash) {
    growth_left -= special_is_empty(prev_ctrl) ? 1 : 0;
    set_ctrl_h2(index, hash);
    items++;
  }

  bool is_in_same_group(size_t index, size_t new_index, HashWord hash) const {
    size_t probe_position = probe_sequence(hash).position;
    size_t position[2] = {
        (index - probe_position) & bucket_mask / sizeof(Group),
        (new_index - probe_position) & bucket_mask / sizeof(Group),
    };
    return position[0] == position[1];
  }

  bool is_empty_singleton() const { return ctrl == CONST_EMPTY_GROUP.data; }

  RawTable prepare_resize(size_t capacity) {
    RawTable new_table = RawTable::with_capacity(capacity);
    if (likely(new_table.ctrl != nullptr)) {
      new_table.growth_left -= items;
      new_table.items += items;
    }
    return new_table;
  }

  template <typename Hasher> void rehash_in_place(Hasher hasher) {
    prepare_rehash_inplace();

    for (size_t idx = 0; idx < num_buckets(); ++idx) {
      if (ctrl[idx] != DELETED) {
        continue;
      }

      Bucket<T> bucket = bucket_at(idx);

      while (true) {
        HashWord hash = hasher(*bucket_at(idx));
        size_t new_idx = find_insert_slot(hash);
        // Probing works by scanning through all of the control
        // bytes in groups, which may not be aligned to the group
        // size. If both the new and old position fall within the
        // same unaligned group, then there is no benefit in moving
        // it and we can just continue to the next item.
        if (likely(is_in_same_group(idx, new_idx, hash))) {
          set_ctrl_h2(idx, hash);
          break; // continue outer loop
        }

        Bucket<T> new_bucket = bucket_at(new_idx);
        CtrlWord prev_ctrl = replace_ctrl_h2(new_idx, hash);

        if (prev_ctrl == EMPTY) {
          set_ctrl(idx, EMPTY);
          *new_bucket = *bucket;
          break; // continue outer loop
        }

        T temp;
        temp = *new_bucket;
        *new_bucket = *bucket;
        *bucket = temp;
      }
    }

    growth_left = bucket_mask_to_capacity(bucket_mask) - items;
  }

  template <typename Hasher> bool resize(size_t new_capacity, Hasher hasher) {
    RawTable new_table = prepare_resize(new_capacity);
    if (new_table.ctrl == nullptr)
      return false;

    for (size_t i = 0; i < num_buckets(); ++i) {
      if (!is_bucket_full(i))
        continue;

      Bucket<T> bucket = bucket_at(i);
      HashWord hash = hasher(*bucket);

      // We can use a simpler version of insert() here since:
      // - there are no DELETED entries.
      // - we know there is enough space in the table.
      // - all elements are unique.
      Slot slot = new_table.prepare_insert_slot(hash);
      Bucket<T> new_bucket = new_table.bucket_at(slot.index);

      *new_bucket = *bucket;
    }

    release();
    *this = new_table;

    return true;
  }

  template <typename Hasher>
  bool reserve_rehash(size_t additional, Hasher hasher) {
    SafeMemSize checked_new_items =
        SafeMemSize{additional} + SafeMemSize{items};
    if (!checked_new_items.valid())
      return false;

    size_t new_items = static_cast<size_t>(checked_new_items);
    size_t full_capacity = bucket_mask_to_capacity(bucket_mask);

    if constexpr (ENABLE_DELETION) {
      if (new_items <= full_capacity / 2) {
        rehash_in_place(hasher);
        return true;
      }
    }

    if constexpr (ENABLE_RESIZE) {
      size_t new_capacity =
          new_items > (full_capacity + 1) ? new_items : (full_capacity + 1);
      return resize(new_capacity, hasher);
    }

    return false;
  }

  template <typename Hasher>
  Bucket<T> insert_at(size_t index, HashWord hash, const T &element,
                      Hasher hasher) {
    size_t prev_ctrl = ctrl[index];

    // If we reach full load factor:
    //
    //   - When deletion is allowed and the found slot is deleted, then it is
    //     okay to insert.
    //   - Otherwise, it means it is an empty slot and such insertion will
    //     invalidate the load factor constraints. Thus, we need to rehash
    //     the table. There are several cases:
    //
    //       - If deletion is enabled, we may be able to rehash the table in
    //         place if the total item is less than half of the table.
    //       - If resizing is enabled, we may be able to grow the table.
    //       - If either deletion nor resizing is enabled, we will fail
    //         immediately.
    if (unlikely(growth_left == 0 &&
                 (!ENABLE_DELETION || special_is_empty(prev_ctrl)))) {
      if (!reserve_rehash(1, hasher))
        return {nullptr, 0};
      index = find_insert_slot(hash);
    }

    record_item_insert_at(index, prev_ctrl, hash);

    Bucket<T> bucket = bucket_at(index);
    *bucket = element;

    return bucket;
  }

public:
  template <typename Hasher> Bucket<T> insert(const T &element, Hasher hasher) {
    HashWord hash = hasher(element);
    size_t index = find_insert_slot(hash);
    return insert_at(index, hash, element, hasher);
  }

  template <typename Hasher, typename Equal>
  Bucket<T> find(const T &element, Hasher hasher, Equal equal) const {
    HashWord hash = hasher(element);
    return find_with_hash(hash, element, equal);
  }

  template <typename Hasher, typename Equal>
  Bucket<T> find_or_insert(const T &element, Hasher hasher, Equal equal) {
    HashWord hash = hasher(element);
    return find_or_insert_with_hash(hash, element, equal, hasher);
  }

  void erase(Bucket<T> item) {
    size_t index = bucket_index(item);
    size_t index_before = (index - sizeof(Group)) & bucket_mask;
    BitMask empty_before = Group::load(&ctrl[index_before]).mask_empty();
    BitMask empty_after = Group::load(&ctrl[index]).mask_empty();
    CtrlWord ctrl;
    if (empty_before.leading_zeros() + empty_after.trailing_zeros() >=
        sizeof(Group)) {
      ctrl = DELETED;
    } else {
      growth_left++;
      ctrl = EMPTY;
    };
    set_ctrl(index, ctrl);
    items--;
  }
};

} // namespace __llvm_libc::internal::swisstable
#endif // LLVM_LIBC_SUPPORT_SWISSTABLE_H
