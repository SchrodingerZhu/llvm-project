#ifndef FLAT_TLSF_FLAT_TLSF_H_
#define FLAT_TLSF_FLAT_TLSF_H_

#include <algorithm>
#include <array>
#include <bit>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <optional>
#include <utility>

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

constexpr void set_bit(size_t& w, uint32_t index) { w |= size_t{1} << index; }

constexpr void clear_bit(size_t& w, uint32_t index) {
  w &= ~(size_t{1} << index);
}

constexpr bool read_bit(size_t w, uint32_t index) {
  return w & (size_t{1} << index);
}

inline bool is_aligned_to(Byte* ptr, size_t align) {
  return (std::bit_cast<uintptr_t>(ptr) & (align - 1)) == 0;
}

inline Byte* align_down_by(Byte* ptr, size_t align) {
  uintptr_t addr = std::bit_cast<uintptr_t>(ptr);
  return std::bit_cast<Byte*>(addr & ~(align - 1));
}

inline Byte* align_up_by_mask(Byte* ptr, size_t align_mask) {
  uintptr_t addr = std::bit_cast<uintptr_t>(ptr);
  return std::bit_cast<Byte*>((addr + align_mask) & ~align_mask);
}

inline Byte* align_up_by(Byte* ptr, size_t align) {
  return align_up_by_mask(ptr, align - 1);
}

inline Byte* saturating_ptr_add(Byte* ptr, size_t bytes) {
  uintptr_t addr = std::bit_cast<uintptr_t>(ptr);
  uintptr_t result;
  if (__builtin_add_overflow(addr, bytes, &result))
    return std::bit_cast<Byte*>(std::numeric_limits<uintptr_t>::max());
  return std::bit_cast<Byte*>(result);
}

}  // namespace bit_utils

struct alignas(16) BitField {
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
  static constexpr size_t BIN_COUNT = BitField::BITS - 1;

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
    if (size <= exponential_region) return size >> bit_utils::ilog2(CHUNK_UNIT);

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

inline void set_above_free(Byte* ptr) { *ptr |= ABOVE_FREE_FLAG; }

inline void clear_above_free(Byte* ptr) { *ptr ^= ABOVE_FREE_FLAG; }

inline void set_end_flag(Byte* ptr) { *ptr ^= HEAP_END_FLAG; }

inline void clear_end_flag(Byte* ptr) { *ptr ^= HEAP_END_FLAG; }
};  // namespace tag

struct Node {
  Node* next;
  // use next_of_prev to avoid branches on special
  // guardian pointer
  Node** next_of_prev;

  Node** addr_of_next() { return &next; }
  void link_at(Node data) {
    *this = data;
    *data.next_of_prev = this;
    if (data.next) data.next->next_of_prev = addr_of_next();
  }
  void unlink() {
    *next_of_prev = next;
    if (next) next->next_of_prev = next_of_prev;
  }
};

namespace chunk {
inline bool is_chunk_size(Byte* base, Byte* end) {
  return end >= base + CHUNK_UNIT;
}
inline size_t required_chunk_size(size_t size) {
  size_t size_with_tag = size + 1;
  size_t align_offset = (-size_with_tag) & (CHUNK_UNIT - 1);
  return size_with_tag + align_offset;
}
inline Byte* alloc_to_end(Byte* base, size_t size) {
  return base + required_chunk_size(size);
}

inline Node* gap_base_to_node(Byte* base) {
  return reinterpret_cast<Node*>(base + GAP_NODE_OFFSET);
}

inline uint32_t* gap_base_to_bin(Byte* base) {
  return reinterpret_cast<uint32_t*>(base + GAP_BIN_OFFSET);
}

inline size_t* gap_base_to_size(Byte* base) {
  return reinterpret_cast<size_t*>(base + GAP_LOW_SIZE_OFFSET);
}

inline size_t* gap_end_to_size_and_flag(Byte* end) {
  return reinterpret_cast<size_t*>(end - GAP_HIGH_SIZE_OFFSET);
}

inline Byte* gap_node_to_base(Node* node) {
  return reinterpret_cast<Byte*>(node) - GAP_NODE_OFFSET;
}

inline size_t* gap_node_to_size(Node* node) {
  return reinterpret_cast<size_t*>(reinterpret_cast<Byte*>(node) -
                                   GAP_NODE_OFFSET + GAP_LOW_SIZE_OFFSET);
}

inline Byte* end_to_tag(Byte* end) { return end - sizeof(Byte); }

inline Byte* align_up(Byte* ptr) {
  return bit_utils::align_up_by(ptr, CHUNK_UNIT);
}

inline Byte* align_down(Byte* ptr) {
  return bit_utils::align_down_by(ptr, CHUNK_UNIT);
}

template <class T>
inline T read_word(const void* ptr) {
  T buffer;
  std::memcpy(&buffer, ptr, sizeof(T));
  return buffer;
}

template <class T>
inline void write_word(void* ptr, T value) {
  std::memcpy(ptr, &value, sizeof(T));
}

template <class T, class F>
inline void update(void* ptr, F&& f) {
  write_word<T>(ptr, f(read_word<T>(ptr)));
}
}  // namespace chunk

class Talc {
  BitField available = {};
  Node** gap_list = nullptr;
  void* heap_base = nullptr;
  size_t heap_size = 0;

 public:
  // Add an area to be managed by the heap
  Byte* claim(Byte* base, size_t size) {
    // Check if `base + size` overflows. If so, that's okay, just claim up to
    // the top. Currently we never claim the last CHUNK_UNIT of memory. Talc
    // could be changed to be able to use them (i.e. support the end wrapping to
    // NULL) however
    // 1. Dealing with this correctly throughout the allocator is very tricky.
    // 2. It's not easy to verify that this code works as intended.
    // 3. I doubt anyone really cares much about those last few bytes of the
    // address space.
    //     It's common practice to put a guard page or something similar there
    //     anyway. The main exception I'm aware of is WebAssembly, which has no
    //     qualms with you using the entire linear address space.
    Byte* heap_end =
        chunk::align_down(bit_utils::saturating_ptr_add(base, size));
    Byte* heap_base;
    Byte* gap_base;

    // Gap lists haven't been initialized
    if (gap_list == nullptr) {
      base = std::max(std::bit_cast<Byte*>(uintptr_t{1}), base);
      heap_base = bit_utils::align_up_by(base, alignof(Node*));
      size_t gap_list_size = sizeof(Node*) * Binning::BIN_COUNT;
      gap_base = chunk::align_up(heap_base + gap_list_size + sizeof(Byte));

      // if calculating gap_base overflowed OR the gap_base is higher than
      // heap_end there isn't enough memory to allocate the metadata and cap it
      // off with a tag
      if (gap_base < heap_base || heap_end < gap_base) return nullptr;

      Byte tag = tag::ALLOCATED_FLAG;
      if (gap_base < heap_end) tag |= tag::ABOVE_FREE_FLAG;
      chunk::write_word(chunk::end_to_tag(gap_base), tag);
      gap_list = reinterpret_cast<Node**>(heap_base);
      for (size_t i = 0; i < Binning::BIN_COUNT; ++i) gap_list[i] = nullptr;
    } else {
      // Note that adding the header size and aligning up automatically dodges
      // the possibility of claiming null, if `memory` started at null.
      gap_base = chunk::align_up(base + sizeof(Byte));

      // if calculating gap_base overflowed OR there isn't a CHUNK_UNIT between
      // gap_base and heap_end, then there isn't enough memory to claim
      if (gap_base + CHUNK_UNIT < base || heap_end < gap_base + CHUNK_UNIT)
        return nullptr;

      heap_base = chunk::end_to_tag(gap_base);
      chunk::write_word(heap_base, tag::ALLOCATED_FLAG | tag::ABOVE_FREE_FLAG |
                                       tag::HEAP_BASE_FLAG);
    }
    if (gap_base < heap_end) {
      register_gap(gap_base, heap_end);
    }

    return heap_end;
  }

 private:
  void register_gap(Byte* base, Byte* end) {
    assert(chunk::is_chunk_size(base, end));

    size_t size = end - base;
    uint32_t bin = std::min(Binning::size_to_bin(size),
                            static_cast<uint32_t>(Binning::BIN_COUNT - 1));
    Node** bin_ptr = &gap_list[bin];

    if (*bin_ptr == nullptr) {
      assert(!available.read_bit(bin));
      available.set_bit(bin);
    }

    chunk::gap_base_to_node(base)->link_at(Node{*bin_ptr, bin_ptr});
    chunk::write_word(chunk::gap_base_to_bin(base), bin);
    chunk::write_word(chunk::gap_base_to_size(base), size);
    chunk::write_word(chunk::gap_end_to_size_and_flag(end), size);

    assert(*bin_ptr != nullptr);
  }

  void deregister_gap(Byte* base, size_t size) {
    assert(gap_list[std::min(Binning::size_to_bin(size),
                             static_cast<uint32_t>(Binning::BIN_COUNT - 1))] !=
           nullptr);

    chunk::gap_base_to_node(base)->unlink();

    uint32_t bin = chunk::read_word<uint32_t>(chunk::gap_base_to_bin(base));
    if (gap_list[bin] == nullptr) {
      assert(available.read_bit(bin));
      available.clear_bit(bin);
    }
  }

  std::optional<std::pair<Byte*, Byte*>> full_search_bin(uint32_t bin,
                                                         size_t required_size,
                                                         size_t align_mask) {
    for (Node* node = gap_list[bin]; node != nullptr; node = node->next) {
      size_t size = chunk::read_word<size_t>(chunk::gap_node_to_size(node));

      Byte* base = chunk::gap_node_to_base(node);
      Byte* end = base + size;
      Byte* aligned_base = bit_utils::align_up_by_mask(base, align_mask);
      if (aligned_base + required_size <= end) {
        deregister_gap(base, size);
        if (base != aligned_base)
          register_gap(base, aligned_base);
        else
          tag::clear_above_free(chunk::end_to_tag(base));
        return std::make_pair(aligned_base, end);
      }
    }

    return std::nullopt;
  }

 public:
  Node* get_gap_list_head(uint32_t bin) const { return gap_list[bin]; }
  Node** get_gap_list_ptr(uint32_t bin) const { return &gap_list[bin]; }
  const BitField& get_available() const { return available; }
  Node** get_gap_list() const { return gap_list; }
  void test_deregister_gap(Byte* base, size_t size) {
    deregister_gap(base, size);
  }

  Byte* allocate(size_t required_size, size_t required_align) {
    size_t required_chunk_size = chunk::required_chunk_size(required_size);
    auto search = [&, this]() -> std::pair<Byte*, Byte*> {
      while (true) {
        // This is allowed to return values >= B::BIN_COUNT.
        // This indicates that the last bucket is our only bet,
        // and the allocations therein are not necessarily big enough.
        size_t bin = Binning::size_to_bin_ceil(
            std::max(required_chunk_size, required_align));

        // special case, this is a large allocation, dig around the last bin
        if (bin >= Binning::BIN_COUNT - 1) {
          if (available.read_bit(Binning::BIN_COUNT - 1)) {
            if (auto result =
                    full_search_bin(Binning::BIN_COUNT - 1, required_chunk_size,
                                    required_align - 1))
              return *result;
          }
          return {nullptr, nullptr};
        }

        size_t bit = available.bit_scan_after(bin);
        // Handle the case where it turns out there's no feasible bins
        // available.
        if (bit >= Binning::BIN_COUNT) {
          if (available.read_bit(bin - 1)) {
            if (auto result = full_search_bin(bin - 1, required_chunk_size,
                                              required_align - 1))
              return *result;
          }
          return {nullptr, nullptr};
        }

        if (required_align <= CHUNK_UNIT) {
          Node* node_ptr = gap_list[bit];
          size_t size =
              chunk::read_word<size_t>(chunk::gap_node_to_size(node_ptr));

          assert(size >= required_chunk_size);
          Byte* base = chunk::gap_node_to_base(node_ptr);
          deregister_gap(base, size);
          tag::clear_above_free(chunk::end_to_tag(base));
          return {base, base + size};
        } else {
          // a larger than CHUNK_UNIT alignment is demanded
          // therefore each chunk is manually checked to be sufficient
          // accordingly
          size_t align_mask = required_align - 1;
          while (true) {
            if (auto result =
                    full_search_bin(bit, required_chunk_size, align_mask))
              return *result;
            if (bit + 1 < Binning::BIN_COUNT ||
                BitField::BITS > Binning::BIN_COUNT) {
              bit = available.bit_scan_after(bit + 1);
              if (bit < Binning::BIN_COUNT) continue;
            }
            if (auto res =
                    full_search_bin(bin - 1, required_chunk_size, align_mask))
              return *res;

            return {nullptr, nullptr};
          }
        }
      }
    };
    auto [base, chunk_end] = search();
    if (base == nullptr) return nullptr;
    assert(chunk::align_down(base) == base);

    Byte* end = base + required_chunk_size;
    Byte tag = tag::ALLOCATED_FLAG;
    if (end != chunk_end) {
      register_gap(end, chunk_end);
      tag |= tag::ABOVE_FREE_FLAG;
    }

    chunk::write_word(chunk::end_to_tag(end), tag);
    return base;
  }

  void deallocate(Byte* ptr, size_t required_size, size_t required_align) {
    Byte* chunk_base = ptr;
    Byte* chunk_end = chunk::alloc_to_end(chunk_base, required_size);
    Byte tag = chunk::read_word<Byte>(chunk::end_to_tag(chunk_end));

    assert(tag::is_allocated(tag));
    assert(chunk::is_chunk_size(chunk_base, chunk_end));
    // Try to recombine with a gap below, if it's there.
    // This gap is never the end of the heap, so we don't need to worry about
    // the presence of an end flag.
    Byte* below_tag_ptr = chunk::end_to_tag(chunk_base);
    if (!tag::is_allocated(chunk::read_word<Byte>(below_tag_ptr))) {
      size_t below_size =
          chunk::read_word<size_t>(chunk::gap_end_to_size_and_flag(chunk_base));

      Byte* below_base = chunk_base - below_size;
      deregister_gap(below_base, below_size);
      chunk_base = below_base;
    } else {
      tag::set_above_free(below_tag_ptr);
    }

    // Try to recombine with a gap above, if it's there.
    // The end flag is never clobbered by this operation, so we can still read
    // it later.
    if (tag::is_above_free(tag)) {
      assert(!tag::is_heap_end(tag));
      size_t above_size =
          chunk::read_word<size_t>(chunk::gap_base_to_size(chunk_end));
      deregister_gap(chunk_end, above_size);
      chunk_end += above_size;
    }

    register_gap(chunk_base, chunk_end);
  }

  bool try_grow_in_place(Byte* ptr, size_t old_size, size_t new_size) {
    assert(new_size >= old_size);

    Byte* old_end = chunk::alloc_to_end(ptr, old_size);
    Byte* new_end = chunk::alloc_to_end(ptr, new_size);

    if (old_end == new_end) return true;

    Byte old_tag = chunk::read_word<Byte>(chunk::end_to_tag(old_end));
    assert(tag::is_allocated(old_tag));

    if (tag::is_above_free(old_tag)) {
      size_t above_size =
          chunk::read_word<size_t>(chunk::gap_base_to_size(old_end));
      Byte* above_end = old_end + above_size;

      if (new_end <= above_end) {
        deregister_gap(old_end, above_size);

        if (new_end != above_end) {
          register_gap(new_end, above_end);
          chunk::write_word(
              chunk::end_to_tag(new_end),
              static_cast<Byte>(tag::ALLOCATED_FLAG | tag::ABOVE_FREE_FLAG));
        } else {
          chunk::write_word(chunk::end_to_tag(new_end),
                            static_cast<Byte>(tag::ALLOCATED_FLAG));
        }

        return true;
      }
    }

    return false;
  }

  void shrink_in_place(Byte* ptr, size_t old_size, size_t new_size) {
    assert(new_size != 0);
    assert(new_size <= old_size);

    Byte* chunk_end = chunk::alloc_to_end(ptr, old_size);
    Byte* new_end = chunk::alloc_to_end(ptr, new_size);

    if (new_end != chunk_end) {
      Byte old_tag = chunk::read_word<Byte>(chunk::end_to_tag(chunk_end));

      if (tag::is_above_free(old_tag)) {
        size_t above_size =
            chunk::read_word<size_t>(chunk::gap_base_to_size(chunk_end));
        deregister_gap(chunk_end, above_size);
        chunk_end += above_size;
      }

      register_gap(new_end, chunk_end);
      chunk::write_word(
          chunk::end_to_tag(new_end),
          static_cast<Byte>(tag::ALLOCATED_FLAG | tag::ABOVE_FREE_FLAG));
    }
  }

  bool try_reallocate_in_place(Byte* ptr, size_t old_size, size_t new_size) {
    if (new_size > old_size) {
      return try_grow_in_place(ptr, old_size, new_size);
    } else if (new_size < old_size) {
      shrink_in_place(ptr, old_size, new_size);
      return true;
    } else {
      return true;
    }
  }

  Byte* reallocate(Byte* ptr, size_t old_size, size_t old_align,
                   size_t new_size, size_t new_align) {
    if (try_reallocate_in_place(ptr, old_size, new_size)) return ptr;
    Byte* new_ptr = allocate(new_size, new_align);
    if (new_ptr == nullptr) return nullptr;
    std::copy(ptr, ptr + old_size, new_ptr);
    deallocate(ptr, old_size, old_align);
    return new_ptr;
  }

  /*
   * Small Alignments (A <= 32):
   *
   * +------------------------------------------------------------+
   * |                  Low-Level Chunk (Size C)                  |
   * +-------------------+----------------+-----------------------+
   * | base_ptr          | user_ptr - 8   | user_ptr              |
   * | (32-B aligned)    | (8-B Header)   | (A <= 32 aligned)     |
   * v                   v                v                       v
   * +───────────────────+────────────────+─────────────────┬─────+
   * │ Pad (shift - 8B)  │ Header (C | E) │ Usable Payload  │ Tag │
   * │ (0 to 24 bytes)   │ (8 bytes)      │ (C-shift-1B)    │(1B) │
   * +───────────────────+────────────────+─────────────────┴─────+
   *                                                        ^
   *                                    base_ptr + C - 1 ---+
   *
   * Large Alignments (A >= 64):
   *
   * +------------------------------------------------------------+
   * |                  Low-Level Chunk (Size C)                  |
   * +-------------------+----------------+-----------------------+
   * | base_ptr          | user_ptr - 8   | user_ptr              |
   * | (A-aligned)       | (8-B Header)   | (A >= 64 aligned)     |
   * v                   v                v                       v
   * +───────────────────+────────────────+─────────────────┬─────+
   * │ Pad (shift - 8B)  │ Header (C | E) │ Usable Payload  │ Tag │
   * │ (A - 8 bytes)     │ (8 bytes)      │ (C-shift-1B)    │(1B) │
   * +───────────────────+────────────────+─────────────────┴─────+
   *                                                        ^
   *                                    base_ptr + C - 1 ---+
   */

  static constexpr size_t HEADER_SIZE = sizeof(size_t);

  void* malloc(size_t size) {
    return aligned_alloc(alignof(std::max_align_t), size);
  }

  void* aligned_alloc(size_t align, size_t size) {
    if (size == 0) return nullptr;

    size_t header_align = alignof(size_t);  // 8 bytes
    size_t allocated_align = std::max(align, header_align);

    size_t shift = (HEADER_SIZE + align - 1) & ~(align - 1);
    size_t allocated_size = size + shift;

    Byte* base_ptr = allocate(allocated_size, allocated_align);
    if (base_ptr == nullptr) return nullptr;

    size_t actual_chunk_size = chunk::required_chunk_size(allocated_size);
    Byte* user_ptr = base_ptr + shift;

    size_t shift_exponent = std::countr_zero(shift);

    size_t* header = reinterpret_cast<size_t*>(user_ptr - HEADER_SIZE);
    *header = actual_chunk_size | (shift_exponent & 31);

    return user_ptr;
  }

  void free(void* ptr) {
    if (ptr == nullptr) return;

    Byte* user_ptr = static_cast<Byte*>(ptr);
    size_t* header = reinterpret_cast<size_t*>(user_ptr - HEADER_SIZE);
    size_t header_val = *header;

    size_t shift_exponent = header_val & 31;
    size_t shift = size_t{1} << shift_exponent;
    size_t actual_chunk_size = header_val & ~31;

    Byte* base_ptr = user_ptr - shift;

    deallocate(base_ptr, actual_chunk_size - 1, shift);
  }

  void* realloc(void* ptr, size_t new_size) {
    if (ptr == nullptr) return malloc(new_size);

    if (new_size == 0) {
      free(ptr);
      return nullptr;
    }

    Byte* user_ptr = static_cast<Byte*>(ptr);
    size_t* header = reinterpret_cast<size_t*>(user_ptr - HEADER_SIZE);
    size_t header_val = *header;

    size_t shift_exponent = header_val & 31;
    size_t shift = size_t{1} << shift_exponent;
    size_t old_chunk_size = header_val & ~31;

    Byte* base_ptr = user_ptr - shift;

    size_t new_allocated_size = new_size + shift;
    size_t new_chunk_size = chunk::required_chunk_size(new_allocated_size);

    if (try_reallocate_in_place(base_ptr, old_chunk_size - 1,
                                new_chunk_size - 1)) {
      *header = new_chunk_size | shift_exponent;
      return user_ptr;
    }

    Byte* new_user_ptr = static_cast<Byte*>(aligned_alloc(shift, new_size));
    if (new_user_ptr == nullptr) {
      return nullptr;
    }

    size_t old_user_size = old_chunk_size - shift - 1;
    size_t bytes_to_copy = std::min(old_user_size, new_size);
    std::copy(user_ptr, user_ptr + bytes_to_copy, new_user_ptr);

    free(ptr);
    return new_user_ptr;
  }
};

}  // namespace flat_tlsf

#endif  // FLAT_TLSF_FLAT_TLSF_H_
