#if SWISSTABLE_TEST_USE_GENERIC_GROUP
#include "src/__support/swisstable/generic.h"
#define SWISSTABLE_TEST_SUITE(X) TEST(LlvmLibcSwissTableGroupGeneric, X)
#else
#include "src/__support/swisstable/dispatch.h"
#define SWISSTABLE_TEST_SUITE(X) TEST(LlvmLibcSwissTableGroup, X)
#endif
#include "src/string/memcmp.h"
#include "utils/UnitTest/LibcTest.h"

namespace __llvm_libc::internal::swisstable {

struct ByteArray {
  alignas(Group) uint8_t data[sizeof(Group) + 1]{};
};

SWISSTABLE_TEST_SUITE(LoadStore) {
  ByteArray array{};
  ByteArray compare{};
  for (size_t i = 0; i < sizeof(array.data); ++i) {
    array.data[i] = 0xff;
    auto group = Group::aligned_load(array.data);
    group.aligned_store(compare.data);
    EXPECT_EQ(__llvm_libc::memcmp(compare.data, array.data, sizeof(Group)), 0);
    group = Group::load(&array.data[1]);
    group.aligned_store(compare.data);
    EXPECT_EQ(__llvm_libc::memcmp(compare.data, &array.data[1], sizeof(Group)),
              0);
    array.data[i] = 0;
  }
}

SWISSTABLE_TEST_SUITE(Match) {
  // Any pair of targets have bit differences not only at the lowest bit.
  // No False positive.
  uint8_t targets[4] = {0x00, 0x11, 0xFF, 0x0F};
  size_t count[4] = {0, 0, 0, 0};
  size_t appearance[4][sizeof(Group)];
  ByteArray array{};

  uintptr_t random = reinterpret_cast<uintptr_t>(&array) ^
                     reinterpret_cast<uintptr_t>(aligned_alloc);

  for (size_t i = 0; i < sizeof(Group); ++i) {
    size_t choice = random % 4;
    random /= 4;
    array.data[i] = targets[choice];
    appearance[choice][count[choice]++] = i;
  }

  for (size_t t = 0; t < sizeof(targets); ++t) {
    auto bitmask = Group::aligned_load(array.data).match_byte(targets[t]);
    for (size_t i = 0; i < count[t]; ++i) {
      size_t iterated = 0;
      for (size_t position : bitmask) {
        ASSERT_EQ(appearance[t][iterated], position);
        iterated++;
      }
      ASSERT_EQ(count[t], iterated);
    }
  }
}

SWISSTABLE_TEST_SUITE(MaskEmpty) {
  uint8_t values[4] = {0x00, 0x0F, DELETED, EMPTY};

  for (size_t i = 0; i < sizeof(Group); ++i) {
    ByteArray array{};

    intptr_t random = reinterpret_cast<uintptr_t>(&array) ^
                      reinterpret_cast<uintptr_t>(aligned_alloc);
    ASSERT_FALSE(Group::aligned_load(array.data).mask_empty().any_bit_set());

    array.data[i] = EMPTY;
    for (size_t j = 0; j < sizeof(Group); ++j) {
      if (i == j)
        continue;
      size_t sample_space = 3 + (j > i);
      size_t choice = random % sample_space;
      random /= sizeof(values);
      array.data[j] = values[choice];
    }

    auto mask = Group::aligned_load(array.data).mask_empty();
    ASSERT_TRUE(mask.any_bit_set());
    ASSERT_EQ(mask.lowest_set_bit_nonzero(), i);
  }
}

SWISSTABLE_TEST_SUITE(MaskEmptyOrDeleted) {
  uint8_t values[4] = {0x00, 0x0F, DELETED, EMPTY};
  for (size_t t = 2; t <= 3; ++t) {
    for (size_t i = 0; i < sizeof(Group); ++i) {
      ByteArray array{};

      intptr_t random = reinterpret_cast<uintptr_t>(&array) ^
                        reinterpret_cast<uintptr_t>(aligned_alloc);
      ASSERT_FALSE(Group::aligned_load(array.data)
                       .mask_empty_or_deleted()
                       .any_bit_set());

      array.data[i] = values[t];
      for (size_t j = 0; j < sizeof(Group); ++j) {
        if (i == j)
          continue;
        size_t sample_space = 2 + 2 * (j > i);
        size_t choice = random % sample_space;
        random /= sizeof(values);
        array.data[j] = values[choice];
      }

      auto mask = Group::aligned_load(array.data).mask_empty_or_deleted();
      ASSERT_TRUE(mask.any_bit_set());
      ASSERT_EQ(mask.lowest_set_bit_nonzero(), i);
    }
  }
}
} // namespace __llvm_libc::internal::swisstable
