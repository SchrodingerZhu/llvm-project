#include "src/__support/swisstable/dispatch.h"
#include "utils/UnitTest/LibcTest.h"
namespace __llvm_libc::internal::swisstable {

using ShortBitMask = BitMaskAdaptor<uint16_t, 0xffff, 1>;
using LargeBitMask = BitMaskAdaptor<uint64_t, 0x80'80'80'80'80'80'80'80, 8>;

TEST(LlvmLibcSwissTableBitMask, Invert) {
  {
    auto x = ShortBitMask{0b10101010'10101010};
    ASSERT_EQ(x.invert().word, static_cast<uint16_t>(0b010101010'1010101));
  }

  {
    auto x = LargeBitMask{0x80808080'00000000};
    ASSERT_EQ(x.invert().word, static_cast<uint64_t>(0x00000000'80808080));
  }
}

TEST(LlvmLibcSwissTableBitMask, SingleBitStrideLeadingZeros) {
  ASSERT_EQ(ShortBitMask{0x8808}.leading_zeros(), size_t{0});
  ASSERT_EQ(ShortBitMask{0x0808}.leading_zeros(), size_t{4});
  ASSERT_EQ(ShortBitMask{0x0408}.leading_zeros(), size_t{5});
  ASSERT_EQ(ShortBitMask{0x0208}.leading_zeros(), size_t{6});
  ASSERT_EQ(ShortBitMask{0x0108}.leading_zeros(), size_t{7});
  ASSERT_EQ(ShortBitMask{0x0008}.leading_zeros(), size_t{12});

  uint16_t data = 0xffff;
  for (size_t i = 0; i < 16; ++i) {
    ASSERT_EQ(ShortBitMask{data}.leading_zeros(), i);
    data >>= 1;
  }
}

TEST(LlvmLibcSwissTableBitMask, MultiBitStrideLeadingZeros) {
  ASSERT_EQ(LargeBitMask{0x80808080'80808080}.leading_zeros(), size_t{0});
  ASSERT_EQ(LargeBitMask{0x00808080'80808080}.leading_zeros(), size_t{1});
  ASSERT_EQ(LargeBitMask{0x00000080'80808080}.leading_zeros(), size_t{3});

  ASSERT_EQ(LargeBitMask{0x80808080'80808080}.leading_zeros(), size_t{0});
  ASSERT_EQ(LargeBitMask{0x01808080'80808080}.leading_zeros(), size_t{0});
  ASSERT_EQ(LargeBitMask{0x10000080'80808080}.leading_zeros(), size_t{0});
  ASSERT_EQ(LargeBitMask{0x0F808080'80808080}.leading_zeros(), size_t{0});

  uint64_t data = 0xffff'ffff'ffff'ffff;
  for (size_t i = 0; i < 8; ++i) {
    for (size_t j = 0; j < 8; ++j) {
      ASSERT_EQ(LargeBitMask{data}.leading_zeros(), i);
      data >>= 1;
    }
  }
}

TEST(LlvmLibcSwissTableBitMask, SingleBitStrideTrailingZeros) {
  ASSERT_EQ(ShortBitMask{0x8808}.trailing_zeros(), size_t{3});
  ASSERT_EQ(ShortBitMask{0x0804}.trailing_zeros(), size_t{2});
  ASSERT_EQ(ShortBitMask{0x0802}.trailing_zeros(), size_t{1});
  ASSERT_EQ(ShortBitMask{0x0801}.trailing_zeros(), size_t{0});
  ASSERT_EQ(ShortBitMask{0x0800}.trailing_zeros(), size_t{11});
  ASSERT_EQ(ShortBitMask{0x1000}.trailing_zeros(), size_t{12});

  uint16_t data = 0xffff;
  for (size_t i = 0; i < 16; ++i) {
    ASSERT_EQ(ShortBitMask{data}.trailing_zeros(), i);
    data <<= 1;
  }
}

TEST(LlvmLibcSwissTableBitMask, MultiBitStrideTrailingZeros) {
  ASSERT_EQ(LargeBitMask{0x80808080'80808080}.trailing_zeros(), size_t{0});
  ASSERT_EQ(LargeBitMask{0x80808080'80808000}.trailing_zeros(), size_t{1});
  ASSERT_EQ(LargeBitMask{0x80808080'80804000}.trailing_zeros(), size_t{1});

  ASSERT_EQ(LargeBitMask{0x80808080'80800000}.trailing_zeros(), size_t{2});
  ASSERT_EQ(LargeBitMask{0x80808080'80000000}.trailing_zeros(), size_t{3});
  ASSERT_EQ(LargeBitMask{0x80808080'00000000}.trailing_zeros(), size_t{4});
  ASSERT_EQ(LargeBitMask{0x80808000'00000000}.trailing_zeros(), size_t{5});

  uint64_t data = 0xffff'ffff'ffff'ffff;
  for (size_t i = 0; i < 8; ++i) {
    for (size_t j = 0; j < 8; ++j) {
      ASSERT_EQ(LargeBitMask{data}.trailing_zeros(), i);
      data <<= 1;
    }
  }
}

TEST(LlvmLibcSwissTableBitMask, SingleBitStrideLowestSetBit) {
  uint16_t data = 0xffff;
  for (size_t i = 0; i < 16; ++i) {
    if (ShortBitMask{data}.any_bit_set()) {
      ASSERT_EQ(ShortBitMask{data}.lowest_set_bit_nonzero(), i);
      data <<= 1;
    }
  }
}

TEST(LlvmLibcSwissTableBitMask, MultiBitStrideLowestSetBit) {
  uint64_t data = 0xffff'ffff'ffff'ffff;
  for (size_t i = 0; i < 8; ++i) {
    for (size_t j = 0; j < 8; ++j) {
      if (LargeBitMask{data}.any_bit_set()) {
        ASSERT_EQ(LargeBitMask{data}.lowest_set_bit_nonzero(), i);
        data <<= 1;
      }
    }
  }
}

TEST(LlvmLibcSwissTableBitMask, SingleBitStrideIteration) {
  using Iter = IteratableBitMaskAdaptor<ShortBitMask>;
  uint16_t data = 0xffff;
  for (size_t i = 0; i < 16; ++i) {
    Iter iter = {data};
    size_t j = i;
    for (auto x : iter) {
      ASSERT_EQ(x, j);
      j++;
    }
    ASSERT_EQ(j, size_t{16});
    data <<= 1;
  }
}

TEST(LlvmLibcSwissTableBitMask, MultiBitStrideIteration) {
  using Iter = IteratableBitMaskAdaptor<LargeBitMask>;
  uint64_t data = Iter::MASK;
  for (size_t i = 0; i < 8; ++i) {
    Iter iter = {data};
    size_t j = i;
    for (auto x : iter) {
      ASSERT_EQ(x, j);
      j++;
    }
    ASSERT_EQ(j, size_t{8});
    data <<= Iter::STRIDE;
  }
}
} // namespace __llvm_libc::internal::swisstable
