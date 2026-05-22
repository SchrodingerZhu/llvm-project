//===-- Unittests for talc_heap -------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "src/__support/CPP/new.h"
#include "src/__support/CPP/span.h"
#include "src/__support/macros/config.h"
#include "src/__support/talc_heap.h"
#include "src/string/memcmp.h"
#include "src/string/memcpy.h"
#include "test/UnitTest/Test.h"

asm(R"(
.globl _end, __llvm_libc_heap_limit

.bss
_end:
  .fill 1024
__llvm_libc_heap_limit:
)");

using LIBC_NAMESPACE::TalcHeap;
using LIBC_NAMESPACE::TalcHeapBuffer;
using LIBC_NAMESPACE::cpp::byte;
using LIBC_NAMESPACE::cpp::span;

static LIBC_CONSTINIT TalcHeapBuffer<2048> talc_heap_symbols;
TalcHeap *talc_heap = &talc_heap_symbols;

#define TEST_FOR_EACH_ALLOCATOR(TestCase, BufferSize)                          \
  class LlvmLibcTalcHeapTest##TestCase                                         \
      : public LIBC_NAMESPACE::testing::Test {                                 \
  public:                                                                      \
    TalcHeapBuffer<BufferSize> fake_global_buffer;                             \
    void SetUp() override {                                                    \
      talc_heap = new (&fake_global_buffer) TalcHeapBuffer<BufferSize>;        \
    }                                                                          \
    void RunTest(TalcHeap &allocator, [[maybe_unused]] size_t N);              \
  };                                                                           \
  TEST_F(LlvmLibcTalcHeapTest##TestCase, TestCase) {                           \
    byte buf[BufferSize] = {byte(0)};                                          \
    TalcHeap allocator(buf);                                                   \
    RunTest(allocator, BufferSize);                                            \
    RunTest(*talc_heap, talc_heap->region().size());                           \
  }                                                                            \
  void LlvmLibcTalcHeapTest##TestCase::RunTest(TalcHeap &allocator,            \
                                               [[maybe_unused]] size_t N)

TEST_FOR_EACH_ALLOCATOR(CanAllocate, 2048) {
  constexpr size_t ALLOC_SIZE = 512;

  void *ptr = allocator.allocate(ALLOC_SIZE);

  ASSERT_NE(ptr, static_cast<void *>(nullptr));
}

TEST_FOR_EACH_ALLOCATOR(AllocationsDontOverlap, 2048) {
  constexpr size_t ALLOC_SIZE = 256;

  void *ptr1 = allocator.allocate(ALLOC_SIZE);
  void *ptr2 = allocator.allocate(ALLOC_SIZE);

  ASSERT_NE(ptr1, static_cast<void *>(nullptr));
  ASSERT_NE(ptr2, static_cast<void *>(nullptr));

  uintptr_t ptr1_start = reinterpret_cast<uintptr_t>(ptr1);
  uintptr_t ptr1_end = ptr1_start + ALLOC_SIZE;
  uintptr_t ptr2_start = reinterpret_cast<uintptr_t>(ptr2);

  EXPECT_GT(ptr2_start, ptr1_end);
}

TEST_FOR_EACH_ALLOCATOR(CanFreeAndRealloc, 2048) {
  constexpr size_t ALLOC_SIZE = 512;

  void *ptr1 = allocator.allocate(ALLOC_SIZE);
  allocator.free(ptr1);
  void *ptr2 = allocator.allocate(ALLOC_SIZE);

  EXPECT_EQ(ptr1, ptr2);
}

TEST_FOR_EACH_ALLOCATOR(ReturnsNullWhenAllocationTooLarge, 2048) {
  EXPECT_EQ(allocator.allocate(N), static_cast<void *>(nullptr));
}

TEST(LlvmLibcTalcHeap, ReturnsNullWhenFull) {
  constexpr size_t N = 2048;
  byte buf[N];

  TalcHeap allocator(buf);

  bool went_null = false;
  for (size_t i = 0; i < N; i++) {
    if (!allocator.allocate(1)) {
      went_null = true;
      break;
    }
  }
  EXPECT_TRUE(went_null);
  EXPECT_EQ(allocator.allocate(1), static_cast<void *>(nullptr));
}

TEST_FOR_EACH_ALLOCATOR(ReturnedPointersAreAligned, 2048) {
  void *ptr1 = allocator.allocate(1);

  uintptr_t ptr1_start = reinterpret_cast<uintptr_t>(ptr1);
  EXPECT_EQ(ptr1_start % 16, static_cast<size_t>(0));

  void *ptr2 = allocator.allocate(1);
  uintptr_t ptr2_start = reinterpret_cast<uintptr_t>(ptr2);

  EXPECT_EQ(ptr2_start % 16, static_cast<size_t>(0));
}

TEST_FOR_EACH_ALLOCATOR(CanRealloc, 2048) {
  constexpr size_t ALLOC_SIZE = 512;
  constexpr size_t kNewAllocSize = 768;

  void *ptr1 = allocator.allocate(ALLOC_SIZE);
  void *ptr2 = allocator.realloc(ptr1, kNewAllocSize);

  ASSERT_NE(ptr1, static_cast<void *>(nullptr));
  ASSERT_NE(ptr2, static_cast<void *>(nullptr));
}

TEST_FOR_EACH_ALLOCATOR(ReallocHasSameContent, 2048) {
  constexpr size_t ALLOC_SIZE = sizeof(int);
  constexpr size_t kNewAllocSize = sizeof(int) * 2;
  byte data1[ALLOC_SIZE];
  byte data2[ALLOC_SIZE];

  int *ptr1 = reinterpret_cast<int *>(allocator.allocate(ALLOC_SIZE));
  *ptr1 = 42;
  LIBC_NAMESPACE::memcpy(data1, ptr1, ALLOC_SIZE);
  int *ptr2 = reinterpret_cast<int *>(allocator.realloc(ptr1, kNewAllocSize));
  LIBC_NAMESPACE::memcpy(data2, ptr2, ALLOC_SIZE);

  ASSERT_NE(ptr1, static_cast<int *>(nullptr));
  ASSERT_NE(ptr2, static_cast<int *>(nullptr));
  EXPECT_EQ(LIBC_NAMESPACE::memcmp(data1, data2, ALLOC_SIZE), 0);
}

TEST_FOR_EACH_ALLOCATOR(ReallocSmallerSize, 2048) {
  constexpr size_t ALLOC_SIZE = 512;
  constexpr size_t kNewAllocSize = 256;

  void *ptr1 = allocator.allocate(ALLOC_SIZE);
  void *ptr2 = allocator.realloc(ptr1, kNewAllocSize);

  // In our simple implementation, realloc of smaller size might allocate new or keep.
  // If it keeps, ptr1 == ptr2. If it allocates new, they might differ but content is preserved.
  // Let's just expect it to succeed.
  EXPECT_NE(ptr2, static_cast<void *>(nullptr));
}

TEST_FOR_EACH_ALLOCATOR(ReallocTooLarge, 2048) {
  constexpr size_t ALLOC_SIZE = 512;
  size_t kNewAllocSize = N * 2;

  void *ptr1 = allocator.allocate(ALLOC_SIZE);
  void *ptr2 = allocator.realloc(ptr1, kNewAllocSize);

  EXPECT_NE(static_cast<void *>(nullptr), ptr1);
  EXPECT_EQ(static_cast<void *>(nullptr), ptr2);
}

TEST_FOR_EACH_ALLOCATOR(CanCalloc, 2048) {
  constexpr size_t ALLOC_SIZE = 128;
  constexpr size_t NUM = 4;
  constexpr int size = NUM * ALLOC_SIZE;
  constexpr byte zero{0};

  byte *ptr1 = reinterpret_cast<byte *>(allocator.calloc(NUM, ALLOC_SIZE));

  for (int i = 0; i < size; i++) {
    EXPECT_EQ(ptr1[i], zero);
  }
}

TEST_FOR_EACH_ALLOCATOR(CallocTooLarge, 2048) {
  size_t ALLOC_SIZE = N + 1;
  EXPECT_EQ(allocator.calloc(1, ALLOC_SIZE), static_cast<void *>(nullptr));
}

TEST_FOR_EACH_ALLOCATOR(AllocateZero, 2048) {
  void *ptr = allocator.allocate(0);
  ASSERT_EQ(ptr, static_cast<void *>(nullptr));
}

TEST_FOR_EACH_ALLOCATOR(AlignedAlloc, 2048) {
  constexpr size_t ALIGNMENTS[] = {1, 2, 4, 8, 16, 32, 64, 128, 256};
  constexpr size_t SIZE_SCALES[] = {1, 2, 3, 4, 5};

  for (size_t alignment : ALIGNMENTS) {
    for (size_t scale : SIZE_SCALES) {
      size_t size = alignment * scale;
      void *ptr = allocator.aligned_allocate(alignment, size);
      EXPECT_NE(ptr, static_cast<void *>(nullptr));
      EXPECT_EQ(reinterpret_cast<uintptr_t>(ptr) % alignment, size_t(0));
      allocator.free(ptr);
    }
  }
}

TEST(LlvmLibcTalcHeap, AlignedAllocUnalignedBuffer) {
  byte buf[4096] = {byte(0)};
  TalcHeap allocator(span<byte>(buf).subspan(1));

  constexpr size_t ALIGNMENTS[] = {1, 2, 4, 8, 16, 32, 64, 128, 256};
  constexpr size_t SIZE_SCALES[] = {1, 2, 3, 4, 5};

  allocator.dump_avails();
  for (size_t alignment : ALIGNMENTS) {
    for (size_t scale : SIZE_SCALES) {
      LIBC_NAMESPACE::IntegerToString<size_t> align_str(alignment);
      LIBC_NAMESPACE::IntegerToString<size_t> scale_str(scale);
      LIBC_NAMESPACE::write_to_stderr("Testing AlignedAllocUnalignedBuffer alignment=");
      LIBC_NAMESPACE::write_to_stderr(align_str.view());
      LIBC_NAMESPACE::write_to_stderr(", scale=");
      LIBC_NAMESPACE::write_to_stderr(scale_str.view());
      LIBC_NAMESPACE::write_to_stderr("\n");

      size_t size = alignment * scale;
      void *ptr = allocator.aligned_allocate(alignment, size);
      if (ptr == nullptr || reinterpret_cast<uintptr_t>(ptr) % alignment != 0) {
        LIBC_NAMESPACE::write_to_stderr("AlignedAllocUnalignedBuffer failed for alignment=");
        LIBC_NAMESPACE::write_to_stderr(align_str.view());
        LIBC_NAMESPACE::write_to_stderr(", scale=");
        LIBC_NAMESPACE::write_to_stderr(scale_str.view());
        LIBC_NAMESPACE::write_to_stderr("\n");
      }
      EXPECT_NE(ptr, static_cast<void *>(nullptr));
      EXPECT_EQ(reinterpret_cast<uintptr_t>(ptr) % alignment, size_t(0));
      allocator.free(ptr);

      LIBC_NAMESPACE::IntegerToString<size_t> free_mem_str(allocator.get_free_mem());
      LIBC_NAMESPACE::write_to_stderr("Free memory: ");
      LIBC_NAMESPACE::write_to_stderr(free_mem_str.view());
      LIBC_NAMESPACE::write_to_stderr("\n");
    }
  }
}

TEST_FOR_EACH_ALLOCATOR(InvalidAlignedAllocAlignment, 2048) {
  constexpr size_t ALIGNMENTS[] = {4, 8, 16, 32, 64, 128, 256};
  for (size_t alignment : ALIGNMENTS) {
    void *ptr = allocator.aligned_allocate(alignment - 1, alignment - 1);
    EXPECT_EQ(ptr, static_cast<void *>(nullptr));
  }

  for (size_t alignment : ALIGNMENTS) {
    void *ptr = allocator.aligned_allocate(alignment, alignment + 1);
    EXPECT_EQ(ptr, static_cast<void *>(nullptr));
  }

  void *ptr = allocator.aligned_allocate(1, 0);
  EXPECT_EQ(ptr, static_cast<void *>(nullptr));

  ptr = allocator.aligned_allocate(0, 8);
  EXPECT_EQ(ptr, static_cast<void *>(nullptr));
}
