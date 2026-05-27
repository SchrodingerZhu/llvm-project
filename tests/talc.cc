#include <algorithm>
#include <array>
#include <cstdlib>
#include <cstring>

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

TEST(TalcTest, VerifyGapProperties) {
  Talc talc;

  Layout meta_layout = min_first_heap_layout();
  auto* meta_mem = static_cast<Byte*>(std::aligned_alloc(meta_layout.align, meta_layout.size));
  ASSERT_NE(meta_mem, nullptr);
  Byte* meta_heap_end = talc.claim(meta_mem, meta_layout.size);
  ASSERT_NE(meta_heap_end, nullptr);

  auto* gap_mem = new Byte[999];
  Byte* gap_end = talc.claim(gap_mem, 999);
  ASSERT_NE(gap_end, nullptr);

  ASSERT_LE(gap_end, gap_mem + 999);
  ASSERT_GT(gap_end + CHUNK_UNIT, gap_mem + 999);

  Byte* gap_base = chunk::align_up(gap_mem + sizeof(Byte));
  size_t gap_size = gap_end - gap_base;
  ASSERT_LE(gap_size, 999);
  ASSERT_GT(gap_size, 999 - CHUNK_UNIT * 2);

  uint32_t gap_bin = std::min(Binning::size_to_bin(gap_size),
                              static_cast<uint32_t>(Binning::BIN_COUNT - 1));

  Node* gap_node_ptr = talc.get_gap_list_head(gap_bin);
  ASSERT_NE(gap_node_ptr, nullptr);
  ASSERT_EQ(gap_node_ptr, chunk::gap_base_to_node(gap_base));

  Node gap_node = *gap_node_ptr;
  ASSERT_EQ(gap_node.next, nullptr);
  ASSERT_EQ(gap_node.next_of_prev, talc.get_gap_list_ptr(gap_bin));

  ASSERT_EQ(gap_bin, chunk::read_word<uint32_t>(chunk::gap_base_to_bin(gap_base)));
  ASSERT_EQ(gap_size, chunk::read_word<size_t>(chunk::gap_base_to_size(gap_base)));

  ASSERT_EQ(chunk::read_word<size_t>(chunk::gap_base_to_size(gap_base)), gap_size);
  ASSERT_EQ(chunk::gap_end_to_size_and_flag(gap_end),
            reinterpret_cast<size_t*>(gap_end - sizeof(size_t)));
  ASSERT_EQ(chunk::read_word<size_t>(chunk::gap_end_to_size_and_flag(gap_end)), gap_size);

  talc.test_deregister_gap(gap_base, gap_size);

  std::free(meta_mem);
  delete[] gap_mem;
}

TEST(TalcTest, AllocDeallocTest) {
  auto* arena = new Byte[5000];
  Talc talc;
  ASSERT_NE(talc.claim(arena, 5000), nullptr);

  size_t size = 2435;
  size_t align = 8;
  Byte* allocation = talc.allocate(size, align);
  ASSERT_NE(allocation, nullptr);

  std::fill(allocation, allocation + size, static_cast<Byte>(0xCD));

  talc.deallocate(allocation, size, align);

  delete[] arena;
}

TEST(TalcTest, AllocFailTest) {
  size_t arena_size = min_first_heap_size() + 100 + CHUNK_UNIT;
  auto* arena = new Byte[arena_size];
  Talc talc;
  ASSERT_NE(talc.claim(arena, arena_size), nullptr);

  Byte* a1 = talc.allocate(8, 8);
  ASSERT_NE(a1, nullptr);

  size_t large_size = 1234 + CHUNK_UNIT;
  Byte* a2 = talc.allocate(large_size, 8);
  ASSERT_EQ(a2, nullptr);

  delete[] arena;
}

TEST(TalcTest, ClaimHeapThatsTooSmall) {
  alignas(8) Byte tiny_heap[200];
  Talc talc;
  ASSERT_EQ(talc.claim(tiny_heap, 200), nullptr);

  ASSERT_EQ(talc.get_gap_list(), nullptr);
  ASSERT_GE(talc.get_available().bit_scan_after(0), Binning::BIN_COUNT);
}

TEST(TalcTest, ClaimSmallHeapAfterMetadataIsAllocated) {
  Layout meta_layout = min_first_heap_layout();
  auto* big_heap = static_cast<Byte*>(std::aligned_alloc(meta_layout.align, meta_layout.size));
  ASSERT_NE(big_heap, nullptr);

  Talc talc;
  ASSERT_NE(talc.claim(big_heap, meta_layout.size), nullptr);

  ASSERT_NE(talc.get_gap_list(), nullptr);
  ASSERT_GE(talc.get_available().bit_scan_after(0), Binning::BIN_COUNT);

  alignas(8) Byte tiny_heap[300];
  ASSERT_NE(talc.claim(tiny_heap, 300), nullptr);

  std::free(big_heap);
}

TEST(TalcTest, MallocFreeBasic) {
  auto* arena = new Byte[5000];
  Talc talc;
  ASSERT_NE(talc.claim(arena, 5000), nullptr);

  // Test simple malloc/free
  void* p1 = talc.malloc(100);
  ASSERT_NE(p1, nullptr);
  ASSERT_EQ(reinterpret_cast<uintptr_t>(p1) % alignof(std::max_align_t), 0);

  std::memset(p1, 0xAB, 100);
  talc.free(p1);

  // Test aligned_alloc
  void* p2 = talc.aligned_alloc(64, 100);
  ASSERT_NE(p2, nullptr);
  ASSERT_EQ(reinterpret_cast<uintptr_t>(p2) % 64, 0);
  std::memset(p2, 0xBC, 100);
  talc.free(p2);

  // Test free(nullptr) is a safe no-op
  talc.free(nullptr);

  delete[] arena;
}

TEST(TalcTest, ReallocBasic) {
  auto* arena = new Byte[5000];
  Talc talc;
  ASSERT_NE(talc.claim(arena, 5000), nullptr);

  // realloc(nullptr, size) is equivalent to malloc
  void* p1 = talc.realloc(nullptr, 100);
  ASSERT_NE(p1, nullptr);
  std::memset(p1, 0x11, 100);

  // Grow in place (or via move)
  void* p2 = talc.realloc(p1, 200);
  ASSERT_NE(p2, nullptr);
  Byte* p2_bytes = static_cast<Byte*>(p2);
  for (size_t i = 0; i < 100; ++i) {
    ASSERT_EQ(p2_bytes[i], 0x11);
  }

  // Shrink in place
  void* p3 = talc.realloc(p2, 50);
  ASSERT_EQ(p3, p2); // Shrink should always be in-place
  Byte* p3_bytes = static_cast<Byte*>(p3);
  for (size_t i = 0; i < 50; ++i) {
    ASSERT_EQ(p3_bytes[i], 0x11);
  }

  // realloc(ptr, 0) is equivalent to free
  void* p4 = talc.realloc(p3, 0);
  ASSERT_EQ(p4, nullptr);

  delete[] arena;
}

}  // namespace
}  // namespace flat_tlsf
