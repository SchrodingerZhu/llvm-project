#include <algorithm>
#include <cstdint>
#include <limits>

#include "flat_tlsf/flat_tlsf.h"
#include "gtest/gtest.h"

namespace flat_tlsf {
namespace {

void expect_eq(BitField lhs, BitField rhs) {
  EXPECT_EQ(lhs.storage, rhs.storage);
}

TEST(BitUtilsTest, PointerAlignment) {
  alignas(64) Byte bytes[128] = {};
  Byte *ptr = bytes + 37;

  EXPECT_TRUE(bit_utils::is_aligned_to(bytes, 64));
  EXPECT_FALSE(bit_utils::is_aligned_to(ptr, 32));
  EXPECT_EQ(bit_utils::align_down_by(ptr, 32), bytes + 32);
  EXPECT_EQ(bit_utils::align_up_by(ptr, 32), bytes + 64);
  EXPECT_EQ(bit_utils::align_up_by_mask(ptr, 31), bytes + 64);
  EXPECT_EQ(bit_utils::align_up_by(bytes + 64, 32), bytes + 64);
}

TEST(BitUtilsTest, SaturatingPtrAdd) {
  Byte *ptr = reinterpret_cast<Byte *>(uintptr_t{100});
  EXPECT_EQ(bit_utils::saturating_ptr_add(ptr, 23),
            reinterpret_cast<Byte *>(uintptr_t{123}));

  Byte *near_end = reinterpret_cast<Byte *>(
      std::numeric_limits<uintptr_t>::max() - uintptr_t{3});
  EXPECT_EQ(bit_utils::saturating_ptr_add(near_end, 4),
            reinterpret_cast<Byte *>(std::numeric_limits<uintptr_t>::max()));
}

TEST(ChunkTest, GapPointerConversions) {
  alignas(CHUNK_UNIT) Byte bytes[CHUNK_UNIT * 2] = {};
  Byte *base = bytes;
  Byte *end = bytes + CHUNK_UNIT;

  EXPECT_EQ(reinterpret_cast<Byte *>(chunk::gap_base_to_node(base)),
            base + GAP_NODE_OFFSET);
  EXPECT_EQ(reinterpret_cast<Byte *>(chunk::gap_base_to_bin(base)),
            base + GAP_BIN_OFFSET);
  EXPECT_EQ(reinterpret_cast<Byte *>(chunk::gap_base_to_size(base)),
            base + GAP_LOW_SIZE_OFFSET);
  EXPECT_EQ(reinterpret_cast<Byte *>(chunk::gap_end_to_size_and_flag(end)),
            end - GAP_HIGH_SIZE_OFFSET);

  Node *node = chunk::gap_base_to_node(base);
  EXPECT_EQ(chunk::gap_node_to_base(node), base);
  EXPECT_EQ(reinterpret_cast<Byte *>(chunk::gap_node_to_size(node)),
            base + GAP_LOW_SIZE_OFFSET);
  EXPECT_EQ(chunk::end_to_tag(end), end - sizeof(Byte));
}

TEST(ChunkTest, AlignsByChunkUnit) {
  alignas(CHUNK_UNIT) Byte bytes[CHUNK_UNIT * 3] = {};
  Byte *ptr = bytes + CHUNK_UNIT + 1;

  EXPECT_EQ(chunk::align_down(ptr), bytes + CHUNK_UNIT);
  EXPECT_EQ(chunk::align_up(ptr), bytes + CHUNK_UNIT * 2);
  EXPECT_EQ(chunk::align_up(bytes + CHUNK_UNIT), bytes + CHUNK_UNIT);
}

TEST(BitFieldTest, TestBitScanAfter) {
  BitField field = BitField::zeros();
  field.storage[0] = 0b10100;
  field.storage[1] = 0b00010;

  EXPECT_EQ(field.bit_scan_after(0), 2);
  EXPECT_EQ(field.bit_scan_after(2), 2);
  EXPECT_EQ(field.bit_scan_after(3), 4);
  EXPECT_EQ(field.bit_scan_after(4), 4);
  EXPECT_EQ(field.bit_scan_after(5), BitField::BITS_PER_ELEMENT + 1);
  EXPECT_EQ(field.bit_scan_after(BitField::BITS_PER_ELEMENT + 1),
            BitField::BITS_PER_ELEMENT + 1);
  EXPECT_EQ(field.bit_scan_after(BitField::BITS_PER_ELEMENT + 2),
            BitField::BITS);
}

TEST(BitFieldTest, SetUnset) {
  for (uint32_t i = 0; i < BitField::BITS; ++i) {
    BitField bf = BitField::zeros();
    bf.set_bit(i);
    bf.clear_bit(i);
    expect_eq(bf, BitField::zeros());
  }
}

TEST(BitFieldTest, SetEqSetAllUnsetRest) {
  for (uint32_t i = 0; i < BitField::BITS; ++i) {
    BitField bf = BitField::zeros();
    for (uint32_t j = 0; j < i; ++j)
      bf.set_bit(j);

    BitField bf2 = BitField::zeros();
    for (uint32_t j = 0; j < BitField::BITS; ++j)
      bf2.set_bit(j);
    for (uint32_t j = i; j < BitField::BITS; ++j)
      bf2.clear_bit(j);

    expect_eq(bf, bf2);
  }
}

TEST(BitFieldTest, BsfZero) {
  BitField bf = BitField::zeros();
  for (uint32_t i = 0; i < BitField::BITS; ++i)
    EXPECT_EQ(bf.bit_scan_after(i), BitField::BITS);
}

TEST(BitFieldTest, Bsf) {
  for (uint32_t i = 0; i < BitField::BITS; ++i) {
    BitField bf = BitField::zeros();
    bf.set_bit(i);
    EXPECT_EQ(bf.bit_scan_after(0), i);
  }
}

TEST(BitFieldTest, BsfFromIndex) {
  for (uint32_t i = 0; i < BitField::BITS; ++i) {
    BitField bf = BitField::zeros();

    for (uint32_t j = i; j < BitField::BITS; ++j)
      bf.set_bit(j);

    for (uint32_t j = 0; j < BitField::BITS; ++j)
      EXPECT_EQ(bf.bit_scan_after(j), std::max(i, j));
  }
}

TEST(BitFieldTest, BsfFromIndex1) {
  BitField bf = BitField::zeros();
  bf.set_bit(0);
  for (uint32_t i = 1; i < BitField::BITS; ++i)
    EXPECT_EQ(bf.bit_scan_after(i), BitField::BITS);
}

TEST(BitFieldTest, BsfFirstLast) {
  BitField bf = BitField::zeros();
  bf.set_bit(0);
  bf.set_bit(BitField::BITS - 1);

  EXPECT_EQ(bf.bit_scan_after(0), 0);
  for (uint32_t i = 1; i < BitField::BITS; ++i)
    EXPECT_EQ(bf.bit_scan_after(i), BitField::BITS - 1);
}

TEST(BitFieldTest, BsfOneBehind) {
  for (uint32_t i = 1; i < BitField::BITS; ++i) {
    BitField bf = BitField::zeros();
    bf.set_bit(i - 1);
    EXPECT_EQ(bf.bit_scan_after(i), BitField::BITS);
  }
}

TEST(BitFieldTest, BsfOneBehindOneForward) {
  for (uint32_t i = 1; i < BitField::BITS - 1; ++i) {
    BitField bf = BitField::zeros();
    bf.set_bit(i - 1);
    bf.set_bit(i + 1);
    EXPECT_EQ(bf.bit_scan_after(i), i + 1);
  }
}

TEST(BitFieldTest, BsfOneBehindOneForwardOneOnPoint) {
  for (uint32_t i = 1; i < BitField::BITS - 1; ++i) {
    BitField bf = BitField::zeros();
    bf.set_bit(i - 1);
    bf.set_bit(i);
    bf.set_bit(i + 1);
    EXPECT_EQ(bf.bit_scan_after(i), i);
  }
}

TEST(BitFieldTest, BsfOneForward) {
  for (uint32_t i = 0; i < BitField::BITS - 1; ++i) {
    BitField bf = BitField::zeros();
    bf.set_bit(i + 1);
    EXPECT_EQ(bf.bit_scan_after(i), i + 1);
  }
}

TEST(BitFieldTest, BsfOnes) {
  for (uint32_t i = 0; i < BitField::BITS; ++i) {
    BitField bf = BitField::zeros();

    for (uint32_t j = i; j < BitField::BITS; ++j)
      bf.set_bit(j);
    EXPECT_EQ(bf.bit_scan_after(0), i);
  }
}

TEST(BitFieldTest, BsfOnesBelow) {
  for (uint32_t i = 0; i < BitField::BITS; ++i) {
    BitField bf = BitField::zeros();
    for (uint32_t j = 0; j < i; ++j)
      bf.set_bit(j);

    EXPECT_EQ(bf.bit_scan_after(i), BitField::BITS);
  }
}

} // namespace
} // namespace flat_tlsf
