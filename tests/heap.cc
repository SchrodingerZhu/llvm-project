#include <algorithm>
#include <array>
#include <cstdlib>

#include "flat_tlsf/flat_tlsf.h"
#include "gtest/gtest.h"

namespace flat_tlsf {
namespace {

struct Layout {
  size_t size;
  size_t align;
};

inline size_t min_first_heap_size() {
  size_t size = chunk::required_chunk_size(Binning::BIN_COUNT * sizeof(Node*));
  size_t max_overhead = CHUNK_UNIT + alignof(size_t) - 1;
  return size + max_overhead;
}

constexpr Layout min_first_heap_layout() {
  size_t size = Binning::BIN_COUNT * sizeof(size_t);
  size_t max_overhead = CHUNK_UNIT;
  return {size + max_overhead, alignof(size_t)};
}

TEST(HeapTest, VerifyGapProperties) {
  Heap heap;

  Layout meta_layout = min_first_heap_layout();
  auto* meta_mem = static_cast<Byte*>(std::aligned_alloc(meta_layout.align, meta_layout.size));
  ASSERT_NE(meta_mem, nullptr);
  Byte* meta_heap_end = heap.claim(meta_mem, meta_layout.size);
  ASSERT_NE(meta_heap_end, nullptr);

  auto* gap_mem = new Byte[999];
  Byte* gap_end = heap.claim(gap_mem, 999);
  ASSERT_NE(gap_end, nullptr);

  ASSERT_LE(gap_end, gap_mem + 999);
  ASSERT_GT(gap_end + CHUNK_UNIT, gap_mem + 999);

  Byte* gap_base = chunk::align_up(gap_mem + sizeof(Byte));
  size_t gap_size = gap_end - gap_base;
  ASSERT_LE(gap_size, 999);
  ASSERT_GT(gap_size, 999 - CHUNK_UNIT * 2);

  uint32_t gap_bin = std::min(Binning::size_to_bin(gap_size),
                              static_cast<uint32_t>(Binning::BIN_COUNT - 1));



  Node* gap_node_ptr = heap.get_gap_list_head(gap_bin);
  ASSERT_NE(gap_node_ptr, nullptr);
  ASSERT_EQ(gap_node_ptr, chunk::gap_base_to_node(gap_base));

  Node gap_node = *gap_node_ptr;
  ASSERT_EQ(gap_node.next, nullptr);
  ASSERT_EQ(gap_node.next_of_prev, heap.get_gap_list_ptr(gap_bin));

  ASSERT_EQ(gap_bin, chunk::read_word<uint32_t>(chunk::gap_base_to_bin(gap_base)));
  ASSERT_EQ(gap_size, chunk::read_word<size_t>(chunk::gap_base_to_size(gap_base)));

  ASSERT_EQ(chunk::read_word<size_t>(chunk::gap_base_to_size(gap_base)), gap_size);
  ASSERT_EQ(chunk::gap_end_to_size_and_flag(gap_end),
            reinterpret_cast<size_t*>(gap_end - sizeof(size_t)));
  ASSERT_EQ(chunk::read_word<size_t>(chunk::gap_end_to_size_and_flag(gap_end)), gap_size);

  heap.test_deregister_gap(gap_base, gap_size);

  std::free(meta_mem);
  delete[] gap_mem;
}

TEST(HeapTest, AllocDeallocTest) {
  auto* arena = new Byte[5000];
  Heap heap;
  ASSERT_NE(heap.claim(arena, 5000), nullptr);

  size_t size = 2435;
  size_t align = 8;
  Byte* allocation = heap.allocate(size, align);
  ASSERT_NE(allocation, nullptr);

  std::fill(allocation, allocation + size, static_cast<Byte>(0xCD));

  heap.deallocate(allocation, size, align);

  delete[] arena;
}

TEST(HeapTest, AllocFailTest) {
  size_t arena_size = min_first_heap_size() + 100 + CHUNK_UNIT;
  auto* arena = new Byte[arena_size];
  Heap heap;
  ASSERT_NE(heap.claim(arena, arena_size), nullptr);

  Byte* a1 = heap.allocate(8, 8);
  ASSERT_NE(a1, nullptr);

  size_t large_size = 1234 + CHUNK_UNIT;
  Byte* a2 = heap.allocate(large_size, 8);
  ASSERT_EQ(a2, nullptr);

  delete[] arena;
}

TEST(HeapTest, ClaimHeapThatsTooSmall) {
  alignas(8) Byte tiny_heap[200];
  Heap heap;
  ASSERT_EQ(heap.claim(tiny_heap, 200), nullptr);

  ASSERT_EQ(heap.get_gap_list(), nullptr);
  ASSERT_GE(heap.get_available().bit_scan_after(0), Binning::BIN_COUNT);
}

TEST(HeapTest, ClaimSmallHeapAfterMetadataIsAllocated) {
  Layout meta_layout = min_first_heap_layout();
  auto* big_heap = static_cast<Byte*>(std::aligned_alloc(meta_layout.align, meta_layout.size));
  ASSERT_NE(big_heap, nullptr);

  Heap heap;
  ASSERT_NE(heap.claim(big_heap, meta_layout.size), nullptr);

  ASSERT_NE(heap.get_gap_list(), nullptr);
  ASSERT_GE(heap.get_available().bit_scan_after(0), Binning::BIN_COUNT);

  alignas(8) Byte tiny_heap[300];
  ASSERT_NE(heap.claim(tiny_heap, 300), nullptr);

  std::free(big_heap);
}

}  // namespace
}  // namespace flat_tlsf
