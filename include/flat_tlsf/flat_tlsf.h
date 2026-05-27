#ifndef FLAT_TLSF_FLAT_TLSF_H_
#define FLAT_TLSF_FLAT_TLSF_H_

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace flat_tlsf {

using Byte = unsigned char;
constexpr size_t CHUNK_UNIT = 4 * sizeof(size_t);
constexpr size_t GAP_NODE_OFFSET = 0;
constexpr size_t GAP_BIN_OFFSET = sizeof(size_t) * 2;
constexpr size_t GAP_LOW_SIZE_OFFSET = sizeof(size_t) * 3;
constexpr size_t GAP_HIGH_SIZE_OFFSET = sizeof(size_t);

namespace bit_utils {

constexpr size_t ilog2(size_t n) {
  return std::numeric_limits<size_t>::digits - 1 -
         static_cast<size_t>(std::countl_zero(n));
}

constexpr bool is_power_of_2(size_t n) { return n > 0 && (n & (n - 1)) == 0; }

constexpr uint32_t bit_scan_after(size_t w, uint32_t start_index) {
  size_t lower_bits_cleared = (w >> start_index) << start_index;
  return std::countr_zero(lower_bits_cleared);
}

constexpr void set_bit(size_t &w, uint32_t index) { w |= size_t{1} << index; }

constexpr void clear_bit(size_t &w, uint32_t index) {
  w &= ~(size_t{1} << index);
}

constexpr bool read_bit(size_t w, uint32_t index) {
  return w & (size_t{1} << index);
}

inline bool is_aligned_to(Byte *ptr, size_t align) {
  return (std::bit_cast<uintptr_t>(ptr) & (align - 1)) == 0;
}

inline Byte *align_down_by(Byte *ptr, size_t align) {
  uintptr_t addr = std::bit_cast<uintptr_t>(ptr);
  return std::bit_cast<Byte *>(addr & ~(align - 1));
}

inline Byte *align_up_by_mask(Byte *ptr, size_t align_mask) {
  uintptr_t addr = std::bit_cast<uintptr_t>(ptr);
  return std::bit_cast<Byte *>((addr + align_mask) & ~align_mask);
}

inline Byte *align_up_by(Byte *ptr, size_t align) {
  return align_up_by_mask(ptr, align - 1);
}

inline Byte *saturating_ptr_add(Byte *ptr, size_t bytes) {
  uintptr_t addr = std::bit_cast<uintptr_t>(ptr);
  uintptr_t result;
  if (__builtin_add_overflow(addr, bytes, &result))
    return std::bit_cast<Byte *>(std::numeric_limits<uintptr_t>::max());
  return std::bit_cast<Byte *>(result);
}

} // namespace bit_utils

struct BitField {
  static constexpr size_t BITS_PER_ELEMENT = 8 * sizeof(size_t);
  static constexpr size_t NUMBER_OF_ELEMENTS = 3;
  static constexpr size_t BITS = BITS_PER_ELEMENT * NUMBER_OF_ELEMENTS;

  std::array<size_t, NUMBER_OF_ELEMENTS> storage;

  static constexpr BitField zeros() { return {}; }

  constexpr uint32_t bit_scan_after(uint32_t bit) const {
    uint32_t array_index = bit / BITS_PER_ELEMENT;
    uint32_t element_index = bit % BITS_PER_ELEMENT;
    uint32_t bit_index =
        bit_utils::bit_scan_after(storage[array_index], element_index);
    if (bit_index < BITS_PER_ELEMENT)
      return array_index * BITS_PER_ELEMENT + bit_index;
    for (array_index = array_index + 1; array_index < NUMBER_OF_ELEMENTS;
         ++array_index) {
      bit_index = bit_utils::bit_scan_after(storage[array_index], 0);
      if (bit_index < BITS_PER_ELEMENT)
        return array_index * BITS_PER_ELEMENT + bit_index;
    }
    return BITS;
  }

  constexpr void set_bit(uint32_t b) {
    size_t array_index = b / BITS_PER_ELEMENT;
    size_t element_index = b % BITS_PER_ELEMENT;
    bit_utils::set_bit(storage[array_index], element_index);
  }

  constexpr void clear_bit(uint32_t b) {
    size_t array_index = b / BITS_PER_ELEMENT;
    size_t element_index = b % BITS_PER_ELEMENT;
    bit_utils::clear_bit(storage[array_index], element_index);
  }

  constexpr bool read_bit(uint32_t b) const {
    size_t array_index = b / BITS_PER_ELEMENT;
    size_t element_index = b % BITS_PER_ELEMENT;
    return bit_utils::read_bit(storage[array_index], element_index);
  }
};

struct Binning {
  static constexpr size_t BIT_COUNT = BitField::BITS - 1;

  /// A fast binning algorithm with relatively even coverage and configurable
  /// behavior.
  ///
  /// This is the default binning algorithm that `Talc` uses due to having a
  /// good spread of bin intervals, being able to take advantage of many or few
  /// buckets well, and being very fast (only a handful of instructions with one
  /// branch).
  ///
  /// # Behavior by size
  /// - `0..=(CHUNK_UNIT*LIN_DIVS*LIN_EXT_MULTI)` : Bins sizes into
  /// one-bin-per-chunk-size
  /// - `(CHUNK_UNIT*LIN_DIVS*LIN_EXT_MULTI)..`   : Binds sizes by
  /// linearly-subdivided exponential levels.
  ///
  /// # Parameters
  /// - `LIN_DIVS`: the number of linear regions per power of two in the
  /// exponential region.
  ///     - The higher this is, the more buckets are needed but the binning is
  ///     more fine-grained.
  ///     - Must be a power of two.
  ///     - Typically 2 (few bins, subpar granularity), 4, or 8 (lots of bins,
  ///     good granularity).
  ///     - This is the parameter you want to figure out first for a given
  ///     number of bins.
  ///
  /// - `LIN_EXT_MULTI`: the linear region extent multiplier.
  ///     - Scales the extent of the linear region.
  ///     - Must be a power of two.
  ///     - Set this to 1 by default.
  ///     - If there are too many bins being used on excessively-high size
  ///     regions, this is useful
  ///         for spending those bins on more buckets for small sizes instead.
  ///
  /// # Deciding on the parameters
  /// Make use of [`test_utils::find_binning_boundaries`] to get a sense for
  /// the mapping. `LIN_DIVS` has a much larger effect so tinker with that first
  /// while keeping `LIN_EXT_MULTI` low, and then increase `LIN_EXT_MULTI` if
  /// there is useless range at the top, given the number of bins you have.
  ///
  /// Having a range up to around 128MiB~2GiB is enough for most applications.
  /// But keep in mind the largest bucket size you'll ever make use of is the
  /// largest contiguous span of memory.
  ///
  /// The main effects on the allocator will be the heap efficiency and the
  /// performance. Scripts to test these can be found in the repository in
  /// `benches/src/bin/`.
  template <size_t LIN_DIVS, size_t LIN_EXT_MULTI>
  static constexpr size_t
  linear_extend_then_linearly_divided_expotential_binning(size_t size) {
    static_assert(bit_utils::is_power_of_2(LIN_DIVS),
                  "LIN_DIVS must be a power of two");
    static_assert(bit_utils::is_power_of_2(LIN_EXT_MULTI),
                  "LIN_EXT_MULTI must be a power of two");

    size_t exponential_region = CHUNK_UNIT * LIN_DIVS * LIN_EXT_MULTI;

    // If the size is small enough, just divide by the chunk size.
    // This is fast short-circuit that handles the case where Talc
    // might give us a `size` smaller than `super::CHUNK_UNIT`
    // and doesn't waste extra bins due to exponential subdivisions
    // being smaller than `super::CHUNK_UNIT` here.
    if (size <= exponential_region)
      return size >> bit_utils::ilog2(CHUNK_UNIT);

    // Let's say `exponential_region` is 256, the chunk unit is 32, LIN_DIVS is
    // 4
    //
    // Exponential level 0:  256 ;  (512 - 256)/LIN_DIVS = 256/LIN_DIVS = 64
    //  Subdiv 0: 256       ; bin 0 + LIN_DIVS * LIN_EXT_MULTI
    //  Subdiv 1: 256 +  64 ; bin 1 + LIN_DIVS * LIN_EXT_MULTI
    //  Subdiv 2: 256 + 128 ; bin 2 + LIN_DIVS * LIN_EXT_MULTI
    //  Subdiv 3: 256 + 196 ; bin 3 + LIN_DIVS * LIN_EXT_MULTI
    // Exponential level 1:  512 ;  512/LIN_DIVS = 128
    //  Subdiv 0: 512       ; bin 4 + LIN_DIVS * LIN_EXT_MULTI
    //  Subdiv 1: 512 + 128 ; bin 5 + LIN_DIVS * LIN_EXT_MULTI
    //  Subdiv 2: 512 + 256 ; bin 6 + LIN_DIVS * LIN_EXT_MULTI
    //  Subdiv 3: 512 + 384 ; bin 7 + LIN_DIVS * LIN_EXT_MULTI
    // Exponential level 2: 1024 ; 1024/LIN_DIVS = 256
    //  etc...
    //
    // Any size here is essentially broken up as follows:
    //
    // 00000000_1_01_010101010
    //               ^^^^^^^^^ dead bits; all of this is ignored, effectively
    //               rounding down these bits away
    //            ^^ linear division bits; LIN_DIVS.ilog2() bits long after the
    //            first set bit; tells us which linear subdivision we're in
    //          ^ first set bit; dictates size.ilog2(); tells us which
    //          "exponential level" this size is

    size_t size_ilog2 = bit_utils::ilog2(size);

    // Shift out the dead bits. This leaves the linear subdivision plus LIN_DIVS
    // (due to the always-set bit at the top)
    size_t linear_subdivision_plus_lin_divs =
        size >> (size_ilog2 - bit_utils::ilog2(LIN_DIVS));

    // Extract the exponential level above the `exponential_region` limit
    // add LIN_EXT_MULTI here along with the other constants, it will get
    // multiplied by LIN_DIVS next which gives us the exponential bins offset
    // subtract 1 along with the other constants, this is important later
    size_t unshifted_exponential_minus_one =
        size_ilog2 - bit_utils::ilog2(exponential_region) + LIN_EXT_MULTI - 1;

    // Multiply the exponential level by LIN_DIVS to shift it above the linear
    // division bits Multiply the LIN_EXT_MULTI by LIN_DIVS to add the offset
    // due to the linearly-spaced buckets Multiply (-1) to get (-LIN_DIVS)
    size_t exponential_plus_offset_minus_lin_divs =
        unshifted_exponential_minus_one << bit_utils::ilog2(LIN_DIVS);

    // This LIN_DIVS cancel out, yielding the expected exponential-region bin
    return exponential_plus_offset_minus_lin_divs +
           linear_subdivision_plus_lin_divs;
  }

  constexpr static uint32_t size_to_bin(size_t size) {
#if __SIZEOF_POINTER__ == 8
    return static_cast<uint32_t>(
        linear_extend_then_linearly_divided_expotential_binning<8, 4>(size));
#elif __SIZEOF_POINTER__ == 4
    return static_cast<uint32_t>(
        linear_extend_then_linearly_divided_expotential_binning<4, 4>(size));
#else
#error "only 64-bit and 32-bit architectures are currently supported"
#endif
  }

  constexpr static uint32_t size_to_bin_ceil(size_t size) {
    return size_to_bin(size - 1) + 1;
  }
};

namespace tag {
static constexpr Byte ALLOCATED_FLAG = 0b0001;
static constexpr Byte ABOVE_FREE_FLAG = 0b0010;
static constexpr Byte HEAP_BASE_FLAG = 0b0100;
static constexpr Byte HEAP_END_FLAG = 0b1000;

inline bool is_above_free(Byte tag) { return tag & ABOVE_FREE_FLAG; }

inline bool is_allocated(Byte tag) { return tag & ALLOCATED_FLAG; }

inline bool is_heap_base(Byte tag) { return tag & HEAP_BASE_FLAG; }

inline bool is_heap_end(Byte tag) { return tag & HEAP_END_FLAG; }

inline void set_above_free(Byte *ptr) { *ptr |= ABOVE_FREE_FLAG; }

inline void clear_above_free(Byte *ptr) { *ptr ^= ABOVE_FREE_FLAG; }

inline void set_end_flag(Byte *ptr) { *ptr ^= HEAP_END_FLAG; }

inline void clear_end_flag(Byte *ptr) { *ptr ^= HEAP_END_FLAG; }
}; // namespace tag

struct Node {};

namespace chunk {
inline bool is_chunk_size(Byte *base, Byte *end) {
  return end - base >= CHUNK_UNIT;
}
inline size_t required_chunk_size(size_t size) {
  size_t size_with_tag = size + 1;
  size_t align_offset = (-size_with_tag) & (CHUNK_UNIT - 1);
  return size_with_tag + align_offset;
}
inline Byte *alloc_to_end(Byte *base, size_t size) {
  return base + required_chunk_size(size);
}

inline Node *gap_base_to_node(Byte *base) {
  return reinterpret_cast<Node *>(base + GAP_NODE_OFFSET);
}

inline uint32_t *gap_base_to_bin(Byte *base) {
  return reinterpret_cast<uint32_t *>(base + GAP_BIN_OFFSET);
}

inline size_t *gap_base_to_size(Byte *base) {
  return reinterpret_cast<size_t *>(base + GAP_LOW_SIZE_OFFSET);
}

inline size_t *gap_end_to_size_and_flag(Byte *end) {
  return reinterpret_cast<size_t *>(end - GAP_HIGH_SIZE_OFFSET);
}

inline Byte *gap_node_to_base(Node *node) {
  return reinterpret_cast<Byte *>(node) - GAP_NODE_OFFSET;
}

inline size_t *gap_node_to_size(Node *node) {
  return reinterpret_cast<size_t *>(reinterpret_cast<Byte *>(node) -
                                    GAP_NODE_OFFSET + GAP_LOW_SIZE_OFFSET);
}

inline Byte *end_to_tag(Byte *end) { return end - sizeof(Byte); }

inline Byte *align_up(Byte *ptr) {
  return bit_utils::align_up_by(ptr, CHUNK_UNIT);
}

inline Byte *align_down(Byte *ptr) {
  return bit_utils::align_down_by(ptr, CHUNK_UNIT);
}
} // namespace chunk

} // namespace flat_tlsf

#endif // FLAT_TLSF_FLAT_TLSF_H_
