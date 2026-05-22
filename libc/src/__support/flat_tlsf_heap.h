//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// \file
// Flat Two-Level Segregated Fit (TLSF) memory allocator.
// This is an allocator inspired by TALC, with constant time allocation
// for real-time systems. Currently the implementation assumes little-endian.
//
//===----------------------------------------------------------------------===//
#ifndef LLVM_LIBC_SRC___SUPPORT_FLAT_TLSF_HEAP_H
#define LLVM_LIBC_SRC___SUPPORT_FLAT_TLSF_HEAP_H

#include "hdr/stdint_proxy.h"
#include "src/__support/CPP/algorithm.h"
#include "src/__support/CPP/array.h"
#include "src/__support/CPP/bit.h"
#include "src/__support/CPP/limits.h"
#include "src/__support/CPP/new.h"
#include "src/__support/CPP/optional.h"
#include "src/__support/CPP/span.h"
#include "src/__support/OSUtil/io.h"
#include "src/__support/block.h"
#include "src/__support/common.h"
#include "src/__support/integer_to_string.h"
#include "src/__support/libc_assert.h"
#include "src/__support/macros/attributes.h"
#include "src/__support/math_extras.h"
#include "src/string/memory_utils/inline_memcpy.h"
#include "src/string/memory_utils/inline_memset.h"
#include <stddef.h>

#if __BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__
#error "flat_tlsf_heap assumes little-endian architecture"
#endif

namespace LIBC_NAMESPACE_DECL {
namespace flat_tlsf {
using RawByte = unsigned char;

template <typename T> LIBC_INLINE constexpr uint32_t ilog2(T value) {
  LIBC_ASSERT(value != 0);
  return static_cast<uint32_t>((cpp::numeric_limits<T>::digits - 1) -
                               cpp::countl_zero(value));
}

// BinConfiguration encapsulates all pointer width, boundary size limits, and
// dynamic linear/exponential size-to-bin strategy parameters for FlatTLSF.
struct BinConfiguration {
  static constexpr size_t PTR_SIZE = sizeof(void *);
  static constexpr size_t CHUNK_UNIT = PTR_SIZE;
  static constexpr size_t MIN_GAP_SIZE = PTR_SIZE * 4;

  static constexpr size_t BITS = 192;
  static constexpr size_t BIN_COUNT = BITS - 1;

  static constexpr size_t SIZE_MASK =
      (PTR_SIZE == 8) ? 0x00FFFFFFFFFFFFFFULL : 0x00FFFFFFUL;

  static constexpr size_t LIN_DIVS = 8;
  static constexpr size_t LIN_EXT_MULTI = 4;
  static constexpr size_t EXPONENTIAL_REGION =
      CHUNK_UNIT * LIN_DIVS * LIN_EXT_MULTI;

  LIBC_INLINE static constexpr uint32_t size_to_bin(size_t size) {
    if (size <= EXPONENTIAL_REGION) {
      return static_cast<uint32_t>(size >> (PTR_SIZE == 8 ? 3 : 2));
    }
    uint32_t size_ilog2 = ilog2(size);
    size_t linear_subdivision_plus_lin_divs = size >> (size_ilog2 - 3);
    uint32_t unshifted_exponential_minus_one =
        size_ilog2 - (PTR_SIZE == 8 ? 8 : 7) + 3;
    uint32_t exponential_plus_offset_minus_lin_divs =
        unshifted_exponential_minus_one << 3;
    return exponential_plus_offset_minus_lin_divs +
           static_cast<uint32_t>(linear_subdivision_plus_lin_divs);
  }

  LIBC_INLINE static constexpr uint32_t size_to_bin_ceil(size_t size) {
    return size_to_bin(size - 1) + 1;
  }
};

LIBC_INLINE_VAR static constexpr size_t PTR_SIZE = BinConfiguration::PTR_SIZE;
LIBC_INLINE_VAR static constexpr size_t CHUNK_UNIT =
    BinConfiguration::CHUNK_UNIT;
LIBC_INLINE_VAR static constexpr size_t MIN_GAP_SIZE =
    BinConfiguration::MIN_GAP_SIZE;
LIBC_INLINE_VAR static constexpr size_t SIZE_MASK = BinConfiguration::SIZE_MASK;
LIBC_INLINE_VAR static constexpr size_t BIN_COUNT = BinConfiguration::BIN_COUNT;

using DefaultBinning = BinConfiguration;

// Pointer-based power-of-two alignment helper functions.
LIBC_INLINE static RawByte *align_up_ptr(RawByte *ptr,
                                         size_t alignment = CHUNK_UNIT) {
  uintptr_t val = cpp::bit_cast<uintptr_t>(ptr);
  return cpp::bit_cast<RawByte *>(align_up(val, alignment));
}

LIBC_INLINE static RawByte *align_down_ptr(RawByte *ptr,
                                           size_t alignment = CHUNK_UNIT) {
  uintptr_t val = cpp::bit_cast<uintptr_t>(ptr);
  return cpp::bit_cast<RawByte *>(align_down(val, alignment));
}

// Tag represents the 1-byte boundary flag byte stored at chunk_end - 1 of an
// allocated chunk, or parsed dynamically at chunk_base - 1 to check the state
// of the block below.
//
// 1. Portability & Strict-Aliasing Safety:
//    We wrap a raw byte value and use bitwise masks to prevent
//    compiler-specific, implementation-defined bitfield ordering that flips on
//    big-endian platforms. This also avoids strict-aliasing violations during
//    pointer type-punning, ensuring standard-compliant and secure execution
//    across multiple target architectures.
//
// 2. The Overlapping Bits Coalesce Down Trick:
//    A Free Real Gap stores its 64-bit high_size anchor at the end, leaving no
//    dedicated physical Tag trailing byte. However, since actual heap sizes are
//    far smaller than 2^56 bytes, the highest byte of high_size (read at
//    chunk_base - 1) is naturally 0x00, which automatically maps to a
//    non-allocated real gap under little-endian ordering with zero overhead.
struct Tag {
  RawByte value;

  LIBC_INLINE_VAR static constexpr RawByte ALLOCATED_FLAG =
      1 << 0; // 0x01: Block is currently active
  LIBC_INLINE_VAR static constexpr RawByte ABOVE_FREE_FLAG =
      1 << 1; // 0x02: Block directly above in memory is free
  LIBC_INLINE_VAR static constexpr RawByte HEAP_BASE_FLAG =
      1 << 2; // 0x04: Block is lower static boundary barrier
  LIBC_INLINE_VAR static constexpr RawByte HEAP_END_FLAG =
      1 << 3; // 0x08: Block is upper static boundary barrier
  LIBC_INLINE_VAR static constexpr RawByte HOLE_FLAG =
      1 << 4; // 0x10: Block below is a sub-size mini-gap

  LIBC_INLINE constexpr bool is_allocated() const {
    return (value & ALLOCATED_FLAG) != 0;
  }
  LIBC_INLINE constexpr bool is_above_free() const {
    return (value & ABOVE_FREE_FLAG) != 0;
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

  LIBC_INLINE constexpr void set_allocated(bool val) {
    if (val)
      value |= ALLOCATED_FLAG;
    else
      value &= ~ALLOCATED_FLAG;
  }
  LIBC_INLINE constexpr void set_above_free(bool val) {
    if (val)
      value |= ABOVE_FREE_FLAG;
    else
      value &= ~ABOVE_FREE_FLAG;
  }
  LIBC_INLINE constexpr void set_heap_base(bool val) {
    if (val)
      value |= HEAP_BASE_FLAG;
    else
      value &= ~HEAP_BASE_FLAG;
  }
  LIBC_INLINE constexpr void set_heap_end(bool val) {
    if (val)
      value |= HEAP_END_FLAG;
    else
      value &= ~HEAP_END_FLAG;
  }
  LIBC_INLINE constexpr void set_hole_flag(bool val) {
    if (val)
      value |= HOLE_FLAG;
    else
      value &= ~HOLE_FLAG;
  }
  LIBC_INLINE void store_to(void *ptr) const {
    *static_cast<RawByte *>(ptr) = value;
  }
};

// Node represents the doubly-linked list nodes stored directly inside the
// FreeGap payload space, using the next_of_prev pointer-to-pointer trick for
// branchless unlinking.
struct Node {
  Node *next;
  Node **next_of_prev;

  LIBC_INLINE static Node **addr_of_next(Node *ptr) { return &(ptr->next); }

  LIBC_INLINE static void link_at(Node *ptr, Node data) {
    LIBC_ASSERT(ptr != nullptr);
    LIBC_ASSERT(data.next_of_prev != nullptr);

    *data.next_of_prev = ptr;
    if (data.next != nullptr)
      data.next->next_of_prev = addr_of_next(ptr);
    *ptr = data;
  }

  LIBC_INLINE void unlink() {
    LIBC_ASSERT(next_of_prev != nullptr);
    *next_of_prev = next;
    if (next != nullptr)
      next->next_of_prev = next_of_prev;
  }
};

/*
 * ============================================================================
 *                                CHUNK LAYOUTS
 * ============================================================================
 *
 * 1. Allocated Chunk Layout (Multiples of 8 bytes):
 * ┌──────────────────────────────────────┐ ▲ base_ptr (8-byte aligned)
 * │ Header: size_t size (8B)             │ │
 * ├──────────────────────────────────────┤ ▼ user_ptr (base_ptr + 8)
 * │                                      │ │
 * │ User Payload (8-Byte aligned)        │ │ Active user storage
 * │                                      │ │
 * ├──────────────────────────────────────┤ │
 * │ Padding (0 to 7 bytes)               │ │
 * ├──────────────────────────────────────┤ ▼ end_ptr - 1
 * │ Tag (1B)                             │ Tag byte (boundary flags)
 * └──────────────────────────────────────┘ ▼ end_ptr (base_ptr + size)
 *
 * 2. Free Real Gap Layout (Size >= 32 Bytes):
 * ┌──────────────────────────────────────┐ ▲ base (8-byte aligned)
 * │ Node *next (8B)                      │ │
 * ├──────────────────────────────────────┤ │ Doubly-linked Node (16B)
 * │ Node **next_of_prev (8B)             │ │
 * ├──────────────────────────────────────┤ ▼ base + 16
 * │ uint32_t bin (4B)                    │ │ Size class cached bin index
 * ├──────────────────────────────────────┤ │
 * │ uint32_t padding (4B)                │ │ Padding/alignment space
 * ├──────────────────────────────────────┤ ▼ base + 24
 * │ size_t low_size (8B)                 │ Low boundary coalesce size anchor
 * ├──────────────────────────────────────┤ ▼ base + 32
 * │                                      │ │
 * │ Unused space (if size > 32B)         │ │
 * │                                      │ │
 * ├──────────────────────────────────────┤ ▼ end - 8
 * │ size_t high_size & end_flag (8B)     │ High boundary coalesce size anchor
 * └──────────────────────────────────────┘ ▼ end
 * NOTE: For an exact 32-byte gap, low_size and high_size overlap at offset 24.
 *
 * 3. Free Barrier Hole Layout (Size < 32 Bytes - e.g., 16-Byte Hole):
 * ┌──────────────────────────────────────┐ ▲ base (8-byte aligned)
 * │ Unused Space (8B)                    │
 * ├──────────────────────────────────────┤ ▼ end - 8
 * │ size (56 bits)                       │ Hole Trailer:
 * │ flags [ALLOCATED | HOLE] (8 bits MSB)│ MSB byte at end-1 acts as Tag
 * └──────────────────────────────────────┘ ▼ end
 * NOTE: The most significant byte of the trailer word sits at end - 1, which
 *       represents the Tag. Since ALLOCATED_FLAG is set, search sweeps skip it,
 *       while the coalescer identifies the HOLE_FLAG to extract size and merge.
 * ============================================================================
 */

/*
 * =============================================================================
 *                 SIZE-TO-BIN CONVERSION & MAPPING INVARIANTS
 * =============================================================================
 * All chunk allocation requests are mathematically segregated into one of our
 * 191 contiguous size bins (0..190) based on the linear transition limit:
 * Threshold = CHUNK_UNIT(8B) * LIN_DIVS(8) * LIN_EXT_MULTI(4) = 256 bytes.
 *
 * 1. Linear Mapping (Size <= 256B):
 *    - Formula: Bin = size >> 3 (1-to-1 chunk size step; no log calculations)
 *    - Example: Request = 32 bytes -> Bin = 32 >> 3 = 4 (Bit 4 of data[0])
 *
 * 2. Exponential-Linear Mapping (Size > 256B) — Discrete "Floating Point":
 *    To divide each power-of-two range into 8 linear subdivisions (mantissas),
 *    we extract the exponent and slice the trailing bits directly:
 *
 *    - Exponent (Base-2 power): size_ilog2 = ilog2(size).
 *    - Mantissa (Linear Subdivision): we grab the leading set bit plus the next
 *      3 bits right below it (yielding a value between 8 and 15):
 *      subdivision = size >> (size_ilog2 - 3)
 *    - Exponent Base Offset: shifts past all lower power-of-two buckets:
 *      offset = (size_ilog2 - 5) << 3
 *    - Final Bin Formula: Bin = offset + subdivision
 *    - For any power-of-two level k = ilog2(size), the subdivisions yield
 *      values in [8..15]. Since the base offset shifts each level by exactly 8
 *      indices ((k + 1 - 5) << 3 = offset_k + 8), the end of level k (offset_k
 *      + 15) is followed immediately by the start of level k+1 (offset_k + 16).
 *
 *    - Example 1: Request = 512 bytes
 *      - Exponent = ilog2(512) = 9
 *      - Mantissa (subdivision) = 512 >> (9 - 3) = 8 (binary 1000)
 *      - Offset = (9 - 5) << 3 = 32
 *      - Bin = 32 + 8 = 40 (Bit 40 of data[0])
 *
 *    - Example 2: Request = 8192 bytes
 *      - Exponent = ilog2(8192) = 13
 *      - Mantissa (subdivision) = 8192 >> (13 - 3) = 8 (binary 1000)
 *      - Offset = (13 - 5) << 3 = 64
 *      - Bin = 64 + 8 = 72 (Bit 8 of data[1])
 * =============================================================================
 */

enum class ChunkType { Allocated, FreeGap, FreeBarrier };

LIBC_INLINE static ChunkType get_tag_type(const void *ptr) {
  RawByte val = *static_cast<const RawByte *>(ptr);
  auto t = cpp::bit_cast<Tag>(val);
  if (t.is_allocated()) {
    if (t.is_hole())
      return ChunkType::FreeBarrier;
    return ChunkType::Allocated;
  }
  return ChunkType::FreeGap;
}

// AllocatedChunk represents a physical active chunk holding active user
// allocations.
class AllocatedChunk {
  RawByte *ptr; // Points to the start of the block (the Header address)

public:
  LIBC_INLINE explicit AllocatedChunk(RawByte *p) : ptr(p) {}

  // Safe static placement-new constructor for active allocated blocks.
  LIBC_INLINE static AllocatedChunk initialize(RawByte *p, size_t size,
                                               Tag tag_val) {
    new (p) size_t(size);
    new (p + size - 1) Tag(tag_val);
    return AllocatedChunk(p);
  }

  LIBC_INLINE RawByte *get_start() const { return ptr; }
  LIBC_INLINE size_t get_size() const {
    return *reinterpret_cast<const size_t *>(ptr);
  }
  LIBC_INLINE RawByte *get_end() const { return ptr + get_size(); }
  LIBC_INLINE void *get_user_ptr() const { return ptr + PTR_SIZE; }

  LIBC_INLINE Tag *get_tag() const {
    return reinterpret_cast<Tag *>(ptr + get_size() - 1);
  }

  LIBC_INLINE static AllocatedChunk from_user_ptr(void *user_ptr) {
    return AllocatedChunk(static_cast<RawByte *>(user_ptr) - PTR_SIZE);
  }
};

// FreeGap represents a real free gap (size >= MIN_GAP_SIZE) that is indexed
// inside free store lists.
class FreeGap {
  RawByte *ptr; // Points to the start of the gap (the Node space location)

public:
  LIBC_INLINE explicit FreeGap(RawByte *p) : ptr(p) {}

  // Safe static placement-new constructor for FreeGaps (complying with standard
  // C++ object model).
  LIBC_INLINE static FreeGap initialize(RawByte *p, size_t size, uint32_t bin) {
    new (p) Node();
    new (p + PTR_SIZE * 2) uint32_t(bin);
    new (p + PTR_SIZE * 3) size_t(size);
    if (size > MIN_GAP_SIZE)
      new (p + size - PTR_SIZE) size_t(size);
    return FreeGap(p);
  }

  LIBC_INLINE RawByte *get_start() const { return ptr; }
  LIBC_INLINE size_t get_size() const {
    return (*reinterpret_cast<const size_t *>(ptr + PTR_SIZE * 3) & ~1ULL) &
           SIZE_MASK;
  }
  LIBC_INLINE RawByte *get_end() const { return ptr + get_size(); }

  LIBC_INLINE Node *get_node() const { return reinterpret_cast<Node *>(ptr); }

  // The cached size-class bin index where this gap is currently stored,
  // enabling instant list-head validation during unlinking.
  LIBC_INLINE uint32_t &get_bin() const {
    return *reinterpret_cast<uint32_t *>(ptr + PTR_SIZE * 2);
  }

  // The lower boundary size field of the block, read by high-memory neighbor
  // blocks looking down (forward coalesce sweeps) to read the size of this
  // block.
  LIBC_INLINE size_t &get_low_size() const {
    return *reinterpret_cast<size_t *>(ptr + PTR_SIZE * 3);
  }

  // The upper boundary size word (which also packs the LSB END_FLAG), read by
  // low-memory neighbor blocks looking up (backward coalesce sweeps) to find
  // the gap start.
  LIBC_INLINE size_t &get_high_size() const {
    return *reinterpret_cast<size_t *>(ptr + get_size() - PTR_SIZE);
  }

  LIBC_INLINE bool is_end_flag_set() const {
    return (get_high_size() & 1ULL) != 0;
  }

  LIBC_INLINE void set_end_flag() { get_high_size() |= 1ULL; }

  LIBC_INLINE void clear_end_flag() { get_high_size() &= ~1ULL; }

  LIBC_INLINE static FreeGap from_end(RawByte *end) {
    size_t high_val = *reinterpret_cast<const size_t *>(end - PTR_SIZE);
    size_t size = (high_val & ~static_cast<size_t>(1)) & SIZE_MASK;
    return FreeGap(end - size);
  }
};

// FreeHole represents a mini-gap barrier hole (size < MIN_GAP_SIZE) that cannot
// support node pointers.
class FreeHole {
  RawByte *ptr; // Points to the start of the hole
  size_t size;

public:
  LIBC_INLINE FreeHole(RawByte *p, size_t s) : ptr(p), size(s) {}

  // Safe static placement-new constructor for FreeHoles.
  LIBC_INLINE static FreeHole initialize(RawByte *p, size_t size) {
    size_t trailer_val =
        size | (static_cast<size_t>(Tag::ALLOCATED_FLAG | Tag::HOLE_FLAG)
                << (PTR_SIZE * 8 - 8));
    new (p + size - PTR_SIZE) size_t(trailer_val);
    return FreeHole(p, size);
  }

  LIBC_INLINE RawByte *get_start() const { return ptr; }
  LIBC_INLINE size_t get_size() const { return size; }
  LIBC_INLINE RawByte *get_end() const { return ptr + size; }

  LIBC_INLINE static FreeHole from_end(RawByte *end) {
    size_t trailer = *reinterpret_cast<const size_t *>(end - PTR_SIZE);
    size_t size = trailer & SIZE_MASK;
    return FreeHole(end - size, size);
  }
};

// BitMask192 acts as a 192-bit contiguous availability bitmap for our 1D
// segregated lists. It wraps a 3-word array (3 * 64 bits = 192 bits total)
// where each bit tracks whether the corresponding size bin is occupied (1) or
// empty (0).
//
// CONCRETE CHUNK MAPPINGS:
//
// 1. data[0] (Bits 0-63): Small, precise, linear size classes (e.g., up to 256
// bytes on 64-bit). These provide exact size-matching for common small
// allocations, bypassing all list lookup loops.
//
// 2. data[1] (Bits 64-127): Medium-sized exponentially-segregated classes with
// linear subdivisions. These support efficient, fast-path allocation of
// moderate-sized objects.
//
// 3. data[2] (Bits 128-191): Large exponential-linear segregated classes. These
// handle huge allocations up to multiple gigabytes.
class alignas(16) BitMask192 {
public:
  static constexpr uint32_t BITS = 192;
  cpp::array<uint64_t, 3> data;

  LIBC_INLINE constexpr BitMask192() : data{0, 0, 0} {}

  LIBC_INLINE void set_bit(uint32_t b) {
    LIBC_ASSERT(b < BITS && "Bit index out of bounds");
    data[b / 64] |= (1ULL << (b % 64));
  }

  LIBC_INLINE void clear_bit(uint32_t b) {
    LIBC_ASSERT(b < BITS && "Bit index out of bounds");
    data[b / 64] &= ~(1ULL << (b % 64));
  }

  LIBC_INLINE bool read_bit(uint32_t b) const {
    LIBC_ASSERT(b < BITS && "Bit index out of bounds");
    return (data[b / 64] & (1ULL << (b % 64))) != 0;
  }

  // Returns the index of the first bit set to 1 at or after index 'b'.
  // Returns BITS (192) if no bit is set.
  LIBC_INLINE uint32_t bit_scan_after(uint32_t b) const {
    LIBC_ASSERT(b < BITS && "Scan starting bit index out of bounds");
    uint32_t idx = b / 64;
    uint32_t bit_idx = b % 64;

    uint64_t masked_val = data[idx] & (0xFFFFFFFFFFFFFFFFULL << bit_idx);
    if (masked_val != 0)
      return idx * 64 + static_cast<uint32_t>(cpp::countr_zero(masked_val));

    for (uint32_t i = idx + 1; i < 3; ++i) {
      if (data[i] != 0)
        return i * 64 + static_cast<uint32_t>(cpp::countr_zero(data[i]));
    }
    return BITS;
  }
};

struct Node;

extern "C" RawByte _end;
extern "C" RawByte __llvm_libc_heap_limit;

// FlatTlsfHeap implements a high-performance Flat Two-Level Segregated Fit heap
// allocator, supporting standard dynamic memory allocation/deallocation in
// real-time systems.
class FlatTlsfHeap {
public:
  static constexpr size_t BIN_COUNT = 191;
  static constexpr size_t MIN_ALIGN = cpp::max(size_t{8}, alignof(max_align_t));

  LIBC_INLINE constexpr FlatTlsfHeap()
      : begin(&_end), end(&__llvm_libc_heap_limit), is_initialized(false),
        avails{}, gap_lists{} {}

  LIBC_INLINE constexpr FlatTlsfHeap(cpp::span<RawByte> region)
      : begin(region.begin()), end(region.end()), is_initialized(false),
        avails{}, gap_lists{} {}

  void *allocate(size_t size);
  void *aligned_allocate(size_t alignment, size_t size);
  void free(void *ptr);
  void *realloc(void *ptr, size_t size);
  void *calloc(size_t num, size_t size);

  LIBC_INLINE cpp::span<RawByte> region() const { return {begin, end}; }

  size_t get_free_mem() const;
  void dump_avails() const;

private:
  void init();
  void *allocate_impl(size_t alignment, size_t size);

  LIBC_INLINE bool is_valid_ptr(void *ptr) const {
    return static_cast<RawByte *>(ptr) >= begin &&
           static_cast<RawByte *>(ptr) < end;
  }

  struct AllocatedRegion {
    RawByte *base;
    RawByte *end;
  };

  cpp::optional<AllocatedRegion>
  full_search_bin(uint32_t bin, size_t actual_size_needed, size_t alignment);

  void register_gap(RawByte *base, RawByte *gap_end);
  void deregister_gap(RawByte *base);

  RawByte *begin;
  RawByte *end;
  bool is_initialized;

  BitMask192 avails;
  cpp::array<Node *, BIN_COUNT> gap_lists;
};

// FlatTlsfHeapBuffer is a utility helper that bundles a static array storage
// buffer inside the Heap object itself, simplifying standalone stack or global
// static allocation.
template <size_t BUFF_SIZE> class FlatTlsfHeapBuffer : public FlatTlsfHeap {
public:
  LIBC_INLINE constexpr FlatTlsfHeapBuffer() : FlatTlsfHeap{buffer}, buffer{} {}

private:
  RawByte buffer[BUFF_SIZE];
};

LIBC_INLINE void FlatTlsfHeap::init() {
  LIBC_ASSERT(!is_initialized && "duplicate initialization");

  // Initialize all segregated bin gap lists to empty heads.
  for (size_t i = 0; i < BIN_COUNT; ++i)
    gap_lists[i] = nullptr;

  RawByte *heap_base = align_up_ptr(begin, CHUNK_UNIT);
  RawByte *heap_end = align_down_ptr(end, CHUNK_UNIT);

  LIBC_ASSERT(heap_base < heap_end && "Heap is too small");

  // Determine gap_base, leaving space for the initial static HEAP_BASE tag
  // byte.
  RawByte *gap_base = (cpp::bit_cast<uintptr_t>(begin) % CHUNK_UNIT == 0)
                          ? begin + CHUNK_UNIT
                          : align_up_ptr(begin, CHUNK_UNIT);

  LIBC_ASSERT(gap_base < heap_end && "Heap is too small for barrier");

  // Write the initial heap base boundary barrier tag.
  Tag barrier_tag =
      Tag{static_cast<RawByte>(Tag::ALLOCATED_FLAG | Tag::HEAP_BASE_FLAG)};
  barrier_tag.store_to(gap_base - 1);

  if (gap_base < heap_end) {
    register_gap(gap_base, heap_end);
    // Write terminal heap limit boundary marker (END_FLAG bit 0) in the
    // high_size trailing anchor.
    FreeGap(gap_base).set_end_flag();
  }

  is_initialized = true;
}

// register_gap registers a raw segment of memory as a free gap:
// 1. If size < MIN_GAP_SIZE, initializes it as a FreeHole (not linked to
// lists).
// 2. Sets ABOVE_FREE_FLAG on the physical block directly below in memory (at
// base - 1).
// 3. Maps size to bin, sets avails bitmap occupancy, placement-news FreeGap
// subfields,
//    and links the list node at the head position branchlessly.
LIBC_INLINE void FlatTlsfHeap::register_gap(RawByte *base, RawByte *gap_end) {
  LIBC_ASSERT(gap_end > base && "Register gap has invalid size bounds");

  size_t size = gap_end - base;
  if (size < MIN_GAP_SIZE) {
    // A mini-gap (Hole) cannot act as a free gap list node, so we must clear
    // the ABOVE_FREE flag on the tag of the chunk directly below us in memory!
    Tag below_tag = cpp::bit_cast<Tag>(*(base - 1));
    below_tag.set_above_free(false);
    below_tag.store_to(base - 1);

    FreeHole::initialize(base, size);
    return;
  }

  // Set the ABOVE_FREE_FLAG on the tag of the chunk directly below us in
  // memory.
  Tag below_tag = cpp::bit_cast<Tag>(*(base - 1));
  below_tag.set_above_free(true);
  below_tag.store_to(base - 1);

  uint32_t bin = cpp::min(DefaultBinning::size_to_bin(size),
                          static_cast<uint32_t>(BIN_COUNT - 1));
  Node **bin_head_ptr = &gap_lists[bin];
  Node *old_head = *bin_head_ptr;

  if (old_head == nullptr)
    avails.set_bit(bin);

  FreeGap::initialize(base, size, bin);
  Node::link_at(reinterpret_cast<Node *>(base), Node{old_head, bin_head_ptr});

  LIBC_ASSERT(*bin_head_ptr != nullptr);
}

// deregister_gap removes a free gap block from its size-class list:
// 1. Unlinks the gap's Node branchlessly from its doubly-linked head lists.
// 2. If the target size list is now empty, clears its availability bit in the
// avails mask.
LIBC_INLINE void FlatTlsfHeap::deregister_gap(RawByte *base) {
  FreeGap gap(base);
  uint32_t actual_bin = gap.get_bin();

  Node **bin_head_ptr = &gap_lists[actual_bin];
  LIBC_ASSERT(*bin_head_ptr != nullptr &&
              "Attempting to deregister from an empty list");

  Node *node = gap.get_node();
  LIBC_ASSERT(*node->next_of_prev == node && "List backlink is corrupted!");

  node->unlink();

  if (*bin_head_ptr == nullptr) {
    LIBC_ASSERT(avails.read_bit(actual_bin) &&
                "Avails bit is already clear for active bin");
    avails.clear_bit(actual_bin);
  }
}

LIBC_INLINE cpp::optional<FlatTlsfHeap::AllocatedRegion>
FlatTlsfHeap::full_search_bin(uint32_t bin, size_t actual_size_needed,
                              size_t alignment) {
  Node *current = gap_lists[bin];
  // Search the bin's list sequentially to find a gap that can satisfy the
  // aligned size request:
  // 1. Calculate the candidate start pointer (aligned_base) ensuring the user
  // payload is properly aligned.
  // 2. If the gap bounds can contain both the alignment padding and the
  // required space, select this gap!
  // 3. Deregister, clear ABOVE_FREE on the tag below, split/register prefix
  // padding, and return.
  while (current != nullptr) {
    FreeGap gap(reinterpret_cast<RawByte *>(current));
    RawByte *base = gap.get_start();
    RawByte *gap_end = gap.get_end();

    RawByte *aligned_user_ptr = align_up_ptr(base + PTR_SIZE, alignment);
    RawByte *aligned_base = aligned_user_ptr - PTR_SIZE;

    if (aligned_base >= base && aligned_base + actual_size_needed <= gap_end) {
      deregister_gap(base);
      if (base != aligned_base) {
        // A prefix split is made; register_gap will keep the ABOVE_FREE tag
        // below us set to true.
        register_gap(base, aligned_base);
      } else {
        // No split: this block becomes allocated, so we must clear the
        // ABOVE_FREE flag on the tag below us.
        Tag below_tag = cpp::bit_cast<Tag>(*(base - 1));
        below_tag.set_above_free(false);
        below_tag.store_to(base - 1);
      }

      return AllocatedRegion{aligned_base, gap_end};
    }
    current = current->next;
  }
  return cpp::nullopt;
}

LIBC_INLINE void *FlatTlsfHeap::allocate_impl(size_t alignment, size_t size) {
  if (size == 0)
    return nullptr;

  if (!is_initialized)
    init();

  // 1. Sizing Math: actual_size_needed rounds up the request (Header + Tag +
  // size).
  //    worst_case_size adds the maximum possible alignment padding to ensure a
  //    fit.
  size_t actual_size_needed = align_up(size + PTR_SIZE + 1, CHUNK_UNIT);
  size_t max_padding = (alignment > CHUNK_UNIT) ? (alignment - CHUNK_UNIT) : 0;
  size_t worst_case_size =
      align_up(size + PTR_SIZE + 1 + max_padding, CHUNK_UNIT);

  RawByte *base = nullptr;
  RawByte *chunk_end = nullptr;

  uint32_t bin = DefaultBinning::size_to_bin_ceil(worst_case_size);

  if (bin >= BIN_COUNT - 1) {
    if (avails.read_bit(BIN_COUNT - 1))
      if (auto success =
              full_search_bin(BIN_COUNT - 1, actual_size_needed, alignment)) {
        base = success->base;
        chunk_end = success->end;
      }
  } else {
    uint32_t b = avails.bit_scan_after(bin);

    if (b >= BIN_COUNT) {
      if (bin > 0 && avails.read_bit(bin - 1)) {
        if (auto success =
                full_search_bin(bin - 1, actual_size_needed, alignment)) {
          base = success->base;
          chunk_end = success->end;
        }
      }
    } else {
      if (alignment <= CHUNK_UNIT) {
        // Fast O(1) Path: alignment is standard, so we can return the first
        // available gap in our best-fit bin instantly without any list
        // traversal loop!
        Node *node_ptr = gap_lists[b];
        LIBC_ASSERT(node_ptr != nullptr);

        FreeGap gap(reinterpret_cast<RawByte *>(node_ptr));
        size_t gap_size = gap.get_size();

        base = gap.get_start();
        deregister_gap(base);

        // Clear the ABOVE_FREE_FLAG on the tag byte of the block directly below
        // us in memory.
        Tag below_tag = cpp::bit_cast<Tag>(*(base - 1));
        below_tag.set_above_free(false);
        below_tag.store_to(base - 1);

        chunk_end = base + gap_size;
      } else {
        // Custom Alignment Path: we must scan higher bins sequentially to find
        // a gap that can satisfy the alignment offset constraints, falling back
        // to bin - 1 if needed.
        while (true) {
          if (auto success =
                  full_search_bin(b, actual_size_needed, alignment)) {
            base = success->base;
            chunk_end = success->end;
            break;
          }

          if (b + 1 < BIN_COUNT) {
            b = avails.bit_scan_after(b + 1);
            if (b < BIN_COUNT)
              continue;
          }

          if (bin > 0 && avails.read_bit(bin - 1))
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

  if (base == nullptr)
    return nullptr;

  LIBC_ASSERT(align_down_ptr(base, CHUNK_UNIT) == base &&
              "Chunk start must be standard chunk-aligned");

  size_t total_space = chunk_end - base;
  RawByte *alloc_end = nullptr;
  Tag tag = Tag{Tag::ALLOCATED_FLAG};

  // Check if heap_end flag is set on high_size trailing anchor
  bool end_flag = (*(chunk_end - PTR_SIZE) & 1) != 0;
  size_t remaining = total_space - actual_size_needed;

  // 2. Remainder Splitting:
  //    - If remaining space >= MIN_GAP_SIZE, split off and register the suffix
  //    back to the free store.
  //    - Otherwise (too small to link), absorb it as internal padding and set
  //    HEAP_END if relevant.
  if (remaining >= MIN_GAP_SIZE) {
    alloc_end = base + actual_size_needed;
    tag.store_to(alloc_end - 1);

    AllocatedChunk::initialize(base, alloc_end - base, tag);

    register_gap(alloc_end, chunk_end);
    if (end_flag)
      *(chunk_end - PTR_SIZE) |= 1;
  } else {
    alloc_end = chunk_end;
    if (end_flag)
      tag.set_heap_end(true);
    tag.store_to(alloc_end - 1);

    AllocatedChunk::initialize(base, alloc_end - base, tag);
  }
  return AllocatedChunk(base).get_user_ptr();
}

LIBC_INLINE void *FlatTlsfHeap::allocate(size_t size) {
  return allocate_impl(MIN_ALIGN, size);
}

LIBC_INLINE void *FlatTlsfHeap::aligned_allocate(size_t alignment,
                                                 size_t size) {
  // The alignment must be an integral power of two.
  if (alignment == 0 || (alignment & (alignment - 1)) != 0)
    return nullptr;

  // The size parameter must be an integral multiple of alignment.
  if (size % alignment != 0)
    return nullptr;

  alignment = cpp::max(alignment, MIN_ALIGN);
  return allocate_impl(alignment, size);
}

LIBC_INLINE void FlatTlsfHeap::free(void *ptr) {
  if (ptr == nullptr)
    return;

  RawByte *bytes = static_cast<RawByte *>(ptr);
  LIBC_ASSERT(is_valid_ptr(bytes) && "Invalid pointer");

  RawByte *chunk_base = bytes - PTR_SIZE;
  AllocatedChunk chunk(chunk_base);
  size_t chunk_size = chunk.get_size();
  RawByte *chunk_end = chunk_base + chunk_size;

  Tag tag = *chunk.get_tag();
  LIBC_ASSERT(tag.is_allocated() && "Double free or corrupted block");

  bool is_heap_end = tag.is_heap_end();
  RawByte *current_base = chunk_base;

  // 1. Coalesce Down Loop: merge adjacent free gaps/holes directly below us.
  while (true) {
    Tag below_tag = cpp::bit_cast<Tag>(*(current_base - 1));
    if (!below_tag.is_allocated() || below_tag.is_hole()) {
      if (below_tag.is_hole()) {
        size_t below_size =
            *reinterpret_cast<size_t *>(current_base - PTR_SIZE) & SIZE_MASK;
        current_base -= below_size;
        continue;
      }

      size_t below_size =
          (*reinterpret_cast<size_t *>(current_base - PTR_SIZE) & ~1ULL) &
          SIZE_MASK;
      RawByte *below_base = current_base - below_size;

      deregister_gap(below_base);

      Tag below_below_tag = cpp::bit_cast<Tag>(*(below_base - 1));
      below_below_tag.set_above_free(false);
      below_below_tag.store_to(below_base - 1);

      current_base = below_base;
      break;
    }
    break;
  }

  // 2. Coalesce Up Check: merge adjacent free gaps directly above us.
  if (tag.is_above_free()) {
    LIBC_ASSERT(!tag.is_heap_end() &&
                "Heap end block cannot have ABOVE_FREE set");

    size_t above_size =
        (*reinterpret_cast<size_t *>(chunk_end + PTR_SIZE * 3) & ~1ULL) &
        SIZE_MASK;
    deregister_gap(chunk_end);

    chunk_end += above_size;
    if (*reinterpret_cast<size_t *>(chunk_end - PTR_SIZE) & 1ULL)
      is_heap_end = true;
  }

  // 3. Register Combined Free Gap:
  register_gap(current_base, chunk_end);
  if (is_heap_end)
    FreeGap(current_base).set_end_flag();
}

LIBC_INLINE void *FlatTlsfHeap::realloc(void *ptr, size_t size) {
  if (size == 0) {
    free(ptr);
    return nullptr;
  }

  if (ptr == nullptr)
    return allocate(size);

  RawByte *bytes = static_cast<RawByte *>(ptr);
  if (!is_valid_ptr(bytes))
    return nullptr;

  RawByte *chunk_base = bytes - PTR_SIZE;
  AllocatedChunk chunk(chunk_base);
  size_t old_chunk_size = chunk.get_size();
  size_t old_user_size = old_chunk_size - PTR_SIZE - 1;

  if (old_user_size >= size)
    return ptr;

  void *new_ptr = allocate(size);
  if (new_ptr == nullptr)
    return nullptr;

  inline_memcpy(new_ptr, ptr, old_user_size);
  free(ptr);
  return new_ptr;
}

LIBC_INLINE void *FlatTlsfHeap::calloc(size_t num, size_t size) {
  size_t bytes;
  if (mul_overflow(num, size, bytes))
    return nullptr;

  void *ptr = allocate(bytes);
  if (ptr != nullptr)
    inline_memset(ptr, 0, bytes);
  return ptr;
}

LIBC_INLINE size_t FlatTlsfHeap::get_free_mem() const {
  size_t total_free = 0;
  for (uint32_t i = 0; i < BIN_COUNT; ++i) {
    Node *current = gap_lists[i];
    while (current != nullptr) {
      FreeGap gap(reinterpret_cast<RawByte *>(current));
      total_free += gap.get_size();
      current = current->next;
    }
  }
  return total_free;
}

LIBC_INLINE void FlatTlsfHeap::dump_avails() const {
  write_to_stderr("Free Gaps:\n");
  for (uint32_t i = 0; i < BIN_COUNT; ++i) {
    Node *current = gap_lists[i];
    while (current != nullptr) {
      FreeGap gap(reinterpret_cast<RawByte *>(current));
      write_to_stderr("  Bin ");
      IntegerToString<uint32_t> bin_str(i);
      write_to_stderr(bin_str.view());
      write_to_stderr(": base=");
      IntegerToString<uintptr_t> base_str(
          reinterpret_cast<uintptr_t>(gap.get_start()));
      write_to_stderr(base_str.view());
      write_to_stderr(", size=");
      IntegerToString<size_t> size_str(gap.get_size());
      write_to_stderr(size_str.view());
      write_to_stderr("\n");
      current = current->next;
    }
  }
}
} // namespace flat_tlsf
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_FLAT_TLSF_HEAP_H
