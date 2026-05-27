#include <algorithm>
#include <cstdint>

#include "flat_tlsf/flat_tlsf.h"
#include "gtest/gtest.h"

namespace flat_tlsf {
namespace {

void expect_eq(BitField lhs, BitField rhs) {
  EXPECT_EQ(lhs.storage, rhs.storage);
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
