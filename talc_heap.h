//===-- Interface for talc_heap ---------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_TALC_HEAP_H
#define LLVM_LIBC_SRC___SUPPORT_TALC_HEAP_H

#define TALC_NOINLINE __attribute__((noinline))

#include <stddef.h>
#include <stdint.h>

#include "src/__support/CPP/array.h"
#include "src/__support/CPP/bit.h"
#include "src/__support/CPP/optional.h"
#include "src/__support/CPP/span.h"
#include "src/__support/block.h"
#include "src/__support/libc_assert.h"
#include "src/__support/macros/config.h"
#include "src/string/memory_utils/inline_memcpy.h"
#include "src/string/memory_utils/inline_memset.h"

namespace LIBC_NAMESPACE_DECL {

using cpp::array;
using cpp::optional;
using cpp::span;

// CHUNK_UNIT is the minimum size and alignment that Talc will use for chunks.
constexpr size_t CHUNK_UNIT = 8;

struct Tag {
  uint8_t value;

  static constexpr uint8_t ALLOCATED_FLAG = 1 << 0;
  static constexpr uint8_t ABOVE_FREE_FLAG = 1 << 1;
  static constexpr uint8_t HEAP_BASE_FLAG = 1 << 2;
  static constexpr uint8_t HEAP_END_FLAG = 1 << 3;
  static constexpr uint8_t HOLE_FLAG = 1 << 4;

  LIBC_INLINE constexpr Tag(uint8_t val = 0) : value(val) {}

  LIBC_INLINE constexpr bool is_above_free() const {
    return (value & ABOVE_FREE_FLAG) != 0;
  }

  LIBC_INLINE constexpr bool is_allocated() const {
    return (value & ALLOCATED_FLAG) != 0;
  }

  LIBC_INLINE constexpr bool is_heap_base() const {
    return (value & HEAP_BASE_FLAG) != 0;
  }

  LIBC_INLINE constexpr bool is_heap_end() const {
    return (value & HEAP_END_FLAG) != 0;
  }

  LIBC_INLINE constexpr bool is_hole() const {
    return (value & HOLE_FLAG) != 0;
  }

  LIBC_INLINE static void set_above_free(Tag* ptr) {
    LIBC_ASSERT((ptr->value & ABOVE_FREE_FLAG) == 0);
    ptr->value |= ABOVE_FREE_FLAG;
  }

  LIBC_INLINE static void clear_above_free(Tag* ptr) {
    LIBC_ASSERT((ptr->value & ABOVE_FREE_FLAG) != 0);
    ptr->value ^= ABOVE_FREE_FLAG;
  }

  LIBC_INLINE static void set_end_flag(Tag* ptr) {
    LIBC_ASSERT((ptr->value & HEAP_END_FLAG) == 0);
    ptr->value ^= HEAP_END_FLAG;
  }

  LIBC_INLINE static void clear_end_flag(Tag* ptr) {
    LIBC_ASSERT((ptr->value & HEAP_END_FLAG) != 0);
    ptr->value ^= HEAP_END_FLAG;
  }
};

struct Header {
  size_t size;
};

static_assert(sizeof(Tag) == 1, "Tag must be exactly 1 byte");
static_assert(sizeof(Header) == 8, "Header must be exactly 8 bytes");

struct Node {
  Node* next;
  Node** next_of_prev;

  LIBC_INLINE static Node** addr_of_next(Node* ptr) { return &(ptr->next); }

  LIBC_INLINE static void link_at(Node* ptr, Node data) {
    LIBC_ASSERT(ptr != nullptr);
    LIBC_ASSERT(data.next_of_prev != nullptr);

    *data.next_of_prev = ptr;

    if (data.next != nullptr) {
      data.next->next_of_prev = addr_of_next(ptr);
    }

    *ptr = data;
  }

  LIBC_INLINE void unlink() {
    LIBC_ASSERT(next_of_prev != nullptr);

    *next_of_prev = next;

    if (next != nullptr) {
      next->next_of_prev = next_of_prev;
    }
  }

  struct Iterator {
    Node* current;

    LIBC_INLINE Iterator(Node* start) : current(start) {}

    LIBC_INLINE Node* operator*() const { return current; }
    LIBC_INLINE Iterator& operator++() {
      current = current->next;
      return *this;
    }
    LIBC_INLINE bool operator!=(const Iterator& other) const {
      return current != other.current;
    }
  };

  LIBC_INLINE static Iterator begin(Node* start) { return Iterator(start); }
  LIBC_INLINE static Iterator end() { return Iterator(nullptr); }
};

// Gap offsets
constexpr size_t GAP_NODE_OFFSET = 0;
constexpr size_t GAP_BIN_OFFSET = sizeof(void*) * 2;
constexpr size_t GAP_LOW_SIZE_OFFSET = sizeof(void*) * 3;
constexpr size_t GAP_HIGH_SIZE_OFFSET = sizeof(void*);

constexpr size_t END_FLAG = 1ULL << 0;

LIBC_INLINE Node* gap_base_to_node(cpp::byte* base) {
  return reinterpret_cast<Node*>(base + GAP_NODE_OFFSET);
}
LIBC_INLINE uint32_t* gap_base_to_bin(cpp::byte* base) {
  return reinterpret_cast<uint32_t*>(base + GAP_BIN_OFFSET);
}
LIBC_INLINE size_t* gap_base_to_size(cpp::byte* base) {
  return reinterpret_cast<size_t*>(base + GAP_LOW_SIZE_OFFSET);
}
LIBC_INLINE size_t* gap_end_to_size_and_flag(cpp::byte* end) {
  return reinterpret_cast<size_t*>(end - GAP_HIGH_SIZE_OFFSET);
}
LIBC_INLINE cpp::byte* gap_node_to_base(Node* node) {
  return reinterpret_cast<cpp::byte*>(node) - GAP_NODE_OFFSET;
}
LIBC_INLINE size_t* gap_node_to_size(Node* node) {
  return reinterpret_cast<size_t*>(reinterpret_cast<cpp::byte*>(node) -
                                   GAP_NODE_OFFSET + GAP_LOW_SIZE_OFFSET);
}
LIBC_INLINE Tag* end_to_tag(cpp::byte* end) {
  return reinterpret_cast<Tag*>(end - sizeof(Tag));
}

LIBC_INLINE cpp::byte* align_up(cpp::byte* ptr, size_t alignment = CHUNK_UNIT) {
  return reinterpret_cast<cpp::byte*>(
      align_up(reinterpret_cast<uintptr_t>(ptr), alignment));
}
LIBC_INLINE cpp::byte* align_down(cpp::byte* ptr,
                                  size_t alignment = CHUNK_UNIT) {
  return reinterpret_cast<cpp::byte*>(
      align_down(reinterpret_cast<uintptr_t>(ptr), alignment));
}

LIBC_INLINE bool is_chunk_size(cpp::byte* base, cpp::byte* end) {
  return reinterpret_cast<uintptr_t>(end) - reinterpret_cast<uintptr_t>(base) >=
         CHUNK_UNIT;
}

LIBC_INLINE constexpr size_t required_chunk_size(size_t size,
                                                 size_t alignment) {
  size_t max_padding = (alignment > CHUNK_UNIT) ? (alignment - CHUNK_UNIT) : 0;
  return align_up(size + sizeof(Header) + sizeof(Tag) + max_padding,
                  CHUNK_UNIT);
}

template <typename T>
LIBC_INLINE constexpr uint32_t ilog2(T value) {
  LIBC_ASSERT(value != 0);
  return cpp::numeric_limits<T>::digits - 1 - cpp::countl_zero(value);
}

// Default binning strategy
template <size_t LIN_DIVS, uint32_t LIN_EXT_MULTI>
LIBC_INLINE constexpr uint32_t
linear_extent_then_linearly_divided_exponential_binning(size_t size) {
  static_assert((LIN_DIVS & (LIN_DIVS - 1)) == 0,
                "LIN_DIVS must be power of 2");
  static_assert((LIN_EXT_MULTI & (LIN_EXT_MULTI - 1)) == 0,
                "LIN_EXT_MULTI must be power of 2");

  size_t exponential_region = CHUNK_UNIT * LIN_DIVS * LIN_EXT_MULTI;

  if (size <= exponential_region) {
    return static_cast<uint32_t>(size >> ilog2(CHUNK_UNIT));
  }

  uint32_t size_ilog2 = ilog2(size);
  size_t linear_subdivision_plus_lin_divs =
      size >> (size_ilog2 - ilog2(LIN_DIVS));
  uint32_t unshifted_exponential_minus_one =
      size_ilog2 - ilog2(exponential_region) + LIN_EXT_MULTI - 1;
  uint32_t exponential_plus_offset_minus_lin_divs =
      unshifted_exponential_minus_one << ilog2(LIN_DIVS);

  return exponential_plus_offset_minus_lin_divs +
         static_cast<uint32_t>(linear_subdivision_plus_lin_divs);
}

struct DefaultBinning {
  static constexpr uint32_t BITS = 192;
  static constexpr uint32_t BIN_COUNT = BITS - 1;

  LIBC_INLINE static constexpr uint32_t size_to_bin(size_t size) {
    return linear_extent_then_linearly_divided_exponential_binning<8, 4>(size);
  }

  LIBC_INLINE static constexpr uint32_t size_to_bin_ceil(size_t size) {
    return size_to_bin(size - 1) + 1;
  }
};

// BitField implementation for std::array<uint64_t, 3>
class BitField192 {
 public:
  static constexpr uint32_t BITS = 192;
  array<uint64_t, 3> data;

  LIBC_INLINE constexpr BitField192() : data{0, 0, 0} {}

  LIBC_INLINE void set_bit(uint32_t b) {
    LIBC_ASSERT(b < BITS);
    data[b / 64] |= (1ULL << (b % 64));
  }

  LIBC_INLINE void clear_bit(uint32_t b) {
    LIBC_ASSERT(b < BITS);
    data[b / 64] &= ~(1ULL << (b % 64));
  }

  LIBC_INLINE bool read_bit(uint32_t b) const {
    LIBC_ASSERT(b < BITS);
    return (data[b / 64] & (1ULL << (b % 64))) != 0;
  }

  LIBC_INLINE uint32_t bit_scan_after(uint32_t b) const {
    LIBC_ASSERT(b < BITS);
    uint32_t idx = b / 64;
    uint32_t bit_idx = b % 64;

    uint64_t masked_val = data[idx] & ~((1ULL << bit_idx) - 1);
    if (masked_val != 0) {
      return idx * 64 + static_cast<uint32_t>(cpp::countr_zero(masked_val));
    }

    for (uint32_t i = idx + 1; i < 3; ++i) {
      if (data[i] != 0) {
        return i * 64 + static_cast<uint32_t>(cpp::countr_zero(data[i]));
      }
    }

    return BITS;
  }
};

extern "C" cpp::byte _end;
extern "C" cpp::byte __llvm_libc_heap_limit;

class TalcHeap {
 public:
  constexpr TalcHeap()
      : begin(&_end),
        end(&__llvm_libc_heap_limit),
        is_initialized(false),
        avails{},
        gap_lists{} {}

  constexpr TalcHeap(span<cpp::byte> region)
      : begin(region.begin()),
        end(region.end()),
        is_initialized(false),
        avails{},
        gap_lists{} {}

  static constexpr size_t MIN_ALIGN = cpp::max(size_t{8}, alignof(max_align_t));

  void* allocate(size_t size);
  void* aligned_allocate(size_t alignment, size_t size);
  void free(void* ptr);
  void* realloc(void* ptr, size_t size);
  void* calloc(size_t num, size_t size);

  void dump_avails() const;
  size_t get_free_mem() const;

  cpp::span<cpp::byte> region() const { return {begin, end}; }

 private:
  void init();
  void* allocate_impl(size_t alignment, size_t size);
  LIBC_INLINE bool is_valid_ptr(void* ptr) const {
    return reinterpret_cast<cpp::byte*>(ptr) >= begin &&
           reinterpret_cast<cpp::byte*>(ptr) < end;
  }

  struct AllocatedRegion {
    cpp::byte* base;
    cpp::byte* end;
  };

  void register_gap(cpp::byte* base, cpp::byte* gap_end);
  void deregister_gap(cpp::byte* base, size_t size);
  optional<AllocatedRegion> full_search_bin(uint32_t bin,
                                            size_t actual_size_needed,
                                            size_t alignment);

  LIBC_INLINE Node** gap_list_ptr(uint32_t bin) {
    LIBC_ASSERT(bin < DefaultBinning::BIN_COUNT);
    return &(gap_lists[bin]);
  }

  cpp::byte* begin;
  cpp::byte* end;
  bool is_initialized;

  BitField192 avails;
  cpp::array<Node*, DefaultBinning::BIN_COUNT> gap_lists;
};

template <size_t BUFF_SIZE>
class TalcHeapBuffer : public TalcHeap {
 public:
  constexpr TalcHeapBuffer() : TalcHeap{buffer}, buffer{} {}

 private:
  cpp::byte buffer[BUFF_SIZE];
};

LIBC_INLINE TALC_NOINLINE void TalcHeap::init() {
  LIBC_ASSERT(!is_initialized && "duplicate initialization");

  // Initialize gap lists
  for (uint32_t i = 0; i < DefaultBinning::BIN_COUNT; ++i) {
    gap_lists[i] = nullptr;
  }

  cpp::byte* heap_base = align_up(begin, CHUNK_UNIT);
  cpp::byte* heap_end = align_down(end, CHUNK_UNIT);

  LIBC_ASSERT(heap_base < heap_end && "Heap is too small");

  // Determine gap_base to leave space for barrier tag
  cpp::byte* gap_base;
  if (reinterpret_cast<uintptr_t>(begin) % CHUNK_UNIT == 0) {
    gap_base = begin + CHUNK_UNIT;
  } else {
    gap_base = align_up(begin, CHUNK_UNIT);
  }

  LIBC_ASSERT(gap_base < heap_end && "Heap is too small for barrier");

  // Write barrier tag
  Tag barrier_tag = Tag(Tag::ALLOCATED_FLAG | Tag::HEAP_BASE_FLAG);
  *end_to_tag(gap_base) = barrier_tag;

  is_initialized = true;

  if (gap_base < heap_end) {
    register_gap(gap_base, heap_end);
    *gap_end_to_size_and_flag(heap_end) |= END_FLAG;
  }
}

LIBC_INLINE void TalcHeap::register_gap(cpp::byte* base, cpp::byte* gap_end) {
  LIBC_ASSERT(is_chunk_size(base, gap_end));

  size_t size = gap_end - base;
  if (size < 32) {
    uint64_t val =
        size |
        (static_cast<uint64_t>(Tag::ALLOCATED_FLAG | Tag::HOLE_FLAG) << 56);
    *reinterpret_cast<uint64_t*>(gap_end - 8) = val;
    return;
  }

  // Real gap!
  Tag* tag_ptr = end_to_tag(base);
  Tag::set_above_free(tag_ptr);

  uint32_t bin = cpp::min(DefaultBinning::size_to_bin(size),
                          DefaultBinning::BIN_COUNT - 1);
  Node** bin_ptr = gap_list_ptr(bin);

  if (*bin_ptr == nullptr) {
    LIBC_ASSERT(!avails.read_bit(bin));
    avails.set_bit(bin);
  }

  Node::link_at(gap_base_to_node(base), Node{*bin_ptr, bin_ptr});
  *gap_base_to_bin(base) = bin;
  *gap_base_to_size(base) = size;
  *gap_end_to_size_and_flag(gap_end) = size;

  LIBC_ASSERT(*bin_ptr != nullptr);
}

LIBC_INLINE void TalcHeap::deregister_gap(cpp::byte* base, size_t size) {
  size_t masked_size = size & 0x00FFFFFFFFFFFFFFULL;
  uint32_t bin = cpp::min(DefaultBinning::size_to_bin(masked_size),
                          DefaultBinning::BIN_COUNT - 1);
  LIBC_ASSERT(*gap_list_ptr(bin) != nullptr);

  Node* node = gap_base_to_node(base);
  LIBC_ASSERT(*node->next_of_prev == node && "List backlink is corrupted!");
  node->unlink();

  uint32_t actual_bin = *gap_base_to_bin(base);
  if (*gap_list_ptr(actual_bin) == nullptr) {
    LIBC_ASSERT(avails.read_bit(actual_bin));
    avails.clear_bit(actual_bin);
  }
}

LIBC_INLINE TALC_NOINLINE optional<TalcHeap::AllocatedRegion>
TalcHeap::full_search_bin(uint32_t bin, size_t actual_size_needed,
                          size_t alignment) {
  Node* current = *gap_list_ptr(bin);
  while (current != nullptr) {
    size_t size = *gap_node_to_size(current);
    size &= ~END_FLAG;
    size &= 0x00FFFFFFFFFFFFFFULL;  // Mask out tag flags if any (should be 0
                                    // for free, but be safe)
    cpp::byte* base = gap_node_to_base(current);
    cpp::byte* gap_end = base + size;

    cpp::byte* aligned_user_ptr = align_up(base + sizeof(Header), alignment);
    cpp::byte* aligned_base = aligned_user_ptr - sizeof(Header);

    if (aligned_base >= base && aligned_base + actual_size_needed <= gap_end) {
      deregister_gap(base, size);
      Tag::clear_above_free(end_to_tag(base));

      if (base != aligned_base) {
        register_gap(base, aligned_base);
      }

      return AllocatedRegion{aligned_base, gap_end};
    }
    current = current->next;
  }
  return cpp::nullopt;
}

LIBC_INLINE TALC_NOINLINE void* TalcHeap::allocate_impl(size_t alignment,
                                                        size_t size) {
  if (size == 0) return nullptr;

  if (!is_initialized) init();

  size_t actual_size_needed =
      align_up(size + sizeof(Header) + sizeof(Tag), CHUNK_UNIT);
  size_t worst_case_size = required_chunk_size(size, alignment);

  cpp::byte* base = nullptr;
  cpp::byte* chunk_end = nullptr;

  uint32_t bin = DefaultBinning::size_to_bin_ceil(worst_case_size);

  if (bin >= DefaultBinning::BIN_COUNT - 1) {
    if (avails.read_bit(DefaultBinning::BIN_COUNT - 1)) {
      if (auto success = full_search_bin(DefaultBinning::BIN_COUNT - 1,
                                         actual_size_needed, alignment)) {
        base = success->base;
        chunk_end = success->end;
      }
    }
  } else {
    uint32_t b = avails.bit_scan_after(bin);

    if (b >= DefaultBinning::BIN_COUNT) {
      if (avails.read_bit(bin - 1)) {
        if (auto success =
                full_search_bin(bin - 1, actual_size_needed, alignment)) {
          base = success->base;
          chunk_end = success->end;
        }
      }
    } else {
      if (alignment <= CHUNK_UNIT) {
        Node* node_ptr = *gap_list_ptr(b);
        LIBC_ASSERT(node_ptr != nullptr);
        size_t gap_size = *gap_node_to_size(node_ptr);
        gap_size &= ~END_FLAG;
        gap_size &= 0x00FFFFFFFFFFFFFFULL;

        LIBC_ASSERT(gap_size >= actual_size_needed);

        base = gap_node_to_base(node_ptr);
        deregister_gap(base, gap_size);
        Tag::clear_above_free(end_to_tag(base));

        chunk_end = base + gap_size;
      } else {
        while (true) {
          if (auto success =
                  full_search_bin(b, actual_size_needed, alignment)) {
            base = success->base;
            chunk_end = success->end;
            break;
          }

          if (b + 1 < DefaultBinning::BIN_COUNT) {
            b = avails.bit_scan_after(b + 1);
            if (b < DefaultBinning::BIN_COUNT) {
              continue;
            }
          }

          if (auto success =
                  full_search_bin(bin - 1, actual_size_needed, alignment)) {
            base = success->base;
            chunk_end = success->end;
          }
          break;
        }
      }
    }
  }

  if (base == nullptr) {
    return nullptr;
  }

  LIBC_ASSERT(align_down(base) == base);

  size_t total_space = chunk_end - base;
  cpp::byte* alloc_end = nullptr;
  Tag tag = Tag::ALLOCATED_FLAG;

  bool end_flag = *gap_end_to_size_and_flag(chunk_end) & END_FLAG;
  size_t remaining = total_space - actual_size_needed;

  if (remaining >= 32) {
    alloc_end = base + actual_size_needed;
    *end_to_tag(alloc_end) = tag;
    register_gap(alloc_end, chunk_end);
    if (end_flag) {
      *gap_end_to_size_and_flag(chunk_end) |= END_FLAG;
    }
  } else {
    alloc_end = chunk_end;
    if (end_flag) {
      tag.value |= Tag::HEAP_END_FLAG;
    }
    *end_to_tag(alloc_end) = tag;
  }

  // Write Header
  reinterpret_cast<Header*>(base)->size = alloc_end - base;

  return base + sizeof(Header);
}

LIBC_INLINE void* TalcHeap::allocate(size_t size) {
  return allocate_impl(MIN_ALIGN, size);
}

LIBC_INLINE void* TalcHeap::aligned_allocate(size_t alignment, size_t size) {
  // The alignment must be an integral power of two.
  if (alignment == 0 || (alignment & (alignment - 1)) != 0) return nullptr;

  // The size parameter must be an integral multiple of alignment.
  if (size % alignment != 0) return nullptr;

  alignment = cpp::max(alignment, MIN_ALIGN);

  return allocate_impl(alignment, size);
}

LIBC_INLINE TALC_NOINLINE void TalcHeap::free(void* ptr) {
  if (ptr == nullptr) return;

  cpp::byte* bytes = static_cast<cpp::byte*>(ptr);
  LIBC_ASSERT(is_valid_ptr(bytes) && "Invalid pointer");

  if (!is_initialized) return;

  cpp::byte* chunk_base = bytes - sizeof(Header);
  Header* header = reinterpret_cast<Header*>(chunk_base);

  size_t chunk_size = header->size;
  cpp::byte* chunk_end = chunk_base + chunk_size;

  Tag tag = *end_to_tag(chunk_end);
  bool is_heap_end = tag.is_heap_end();

  LIBC_ASSERT(tag.is_allocated());
  LIBC_ASSERT(is_chunk_size(chunk_base, chunk_end));

  // Coalesce Down (Looping!)
  while (true) {
    Tag below_tag = *end_to_tag(chunk_base);
    if (!below_tag.is_allocated() || below_tag.is_hole()) {
      size_t below_size;
      if (below_tag.is_hole()) {
        below_size = *reinterpret_cast<size_t*>(chunk_base - 8);
        below_size &= 0x00FFFFFFFFFFFFFFULL;
        chunk_base = chunk_base - below_size;
        continue;
      } else {
        below_size = *gap_end_to_size_and_flag(chunk_base);
        below_size &= 0x00FFFFFFFFFFFFFFULL;
        cpp::byte* below_base = chunk_base - below_size;
        deregister_gap(below_base, below_size);
        Tag::clear_above_free(end_to_tag(below_base));
        chunk_base = chunk_base - below_size;
        break;
      }
    } else {
      break;
    }
  }

  // Coalesce Up
  if (tag.is_above_free()) {
    LIBC_ASSERT(!tag.is_heap_end());

    size_t above_size = *gap_base_to_size(chunk_end);
    above_size &= ~END_FLAG;
    above_size &= 0x00FFFFFFFFFFFFFFULL;

    deregister_gap(chunk_end, above_size);
    Tag::clear_above_free(end_to_tag(chunk_end));
    chunk_end = chunk_end + above_size;

    size_t end_val = *gap_end_to_size_and_flag(chunk_end);
    if (end_val & END_FLAG) {
      is_heap_end = true;
    }
  }

  register_gap(chunk_base, chunk_end);
  if (is_heap_end) {
    *gap_end_to_size_and_flag(chunk_end) |= END_FLAG;
  }
}

LIBC_INLINE TALC_NOINLINE void* TalcHeap::realloc(void* ptr, size_t size) {
  if (size == 0) {
    free(ptr);
    return nullptr;
  }

  if (ptr == nullptr) return allocate(size);

  cpp::byte* bytes = static_cast<cpp::byte*>(ptr);
  if (!is_valid_ptr(bytes)) return nullptr;

  cpp::byte* chunk_base = bytes - sizeof(Header);
  Header* header = reinterpret_cast<Header*>(chunk_base);
  size_t old_chunk_size = header->size;
  size_t old_user_size = old_chunk_size - sizeof(Header) - sizeof(Tag);

  if (old_user_size >= size) return ptr;

  void* new_ptr = allocate(size);
  if (new_ptr == nullptr) return nullptr;

  LIBC_NAMESPACE::inline_memcpy(new_ptr, ptr, old_user_size);
  free(ptr);
  return new_ptr;
}

LIBC_INLINE TALC_NOINLINE void* TalcHeap::calloc(size_t num, size_t size) {
  size_t bytes;
  if (__builtin_mul_overflow(num, size, &bytes)) return nullptr;
  void* ptr = allocate(bytes);
  if (ptr != nullptr) LIBC_NAMESPACE::inline_memset(ptr, 0, bytes);
  return ptr;
}

LIBC_INLINE TALC_NOINLINE void TalcHeap::dump_avails() const {
  LIBC_NAMESPACE::write_to_stderr("Active bins: ");
  for (uint32_t i = 0; i < DefaultBinning::BIN_COUNT; ++i) {
    if (avails.read_bit(i)) {
      LIBC_NAMESPACE::IntegerToString<uint32_t> bin_str(i);
      LIBC_NAMESPACE::write_to_stderr(bin_str.view());
      LIBC_NAMESPACE::write_to_stderr(" ");
    }
  }
  LIBC_NAMESPACE::write_to_stderr("\n");
}

LIBC_INLINE TALC_NOINLINE size_t TalcHeap::get_free_mem() const {
  size_t total_free = 0;
  for (uint32_t i = 0; i < DefaultBinning::BIN_COUNT; ++i) {
    Node* current = gap_lists[i];
    while (current != nullptr) {
      size_t size = *gap_node_to_size(current);
      size &= ~END_FLAG;
      size &= 0x00FFFFFFFFFFFFFFULL;
      total_free += size;
      current = current->next;
    }
  }
  return total_free;
}

}  // namespace LIBC_NAMESPACE_DECL

#endif  // LLVM_LIBC_SRC___SUPPORT_TALC_HEAP_H