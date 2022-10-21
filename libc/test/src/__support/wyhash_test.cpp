//===-- Unittests for wyhash ----------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "src/__support/wyhash.h"
#include "src/stdlib/rand.h"
#include "src/string/memset.h"
#include "src/string/strlen.h"
#include "utils/UnitTest/LibcTest.h"

#if defined __has_builtin
#if __has_builtin(__builtin_popcountll)
#define LLVM_LIBC_WYHASH_TEST_HAS_BUILTIN_POPCOUNTLL
#endif
#endif

static inline size_t random_string(char *buf) {
  static constexpr size_t LOWER_LIMIT = 16;
  static constexpr size_t UPPER_LIMIT = 64;
  size_t length =
      LOWER_LIMIT + __llvm_libc::rand() % (UPPER_LIMIT - LOWER_LIMIT);
  // 1 << (8 + 16) ~ 1 << (8 + 64) should be of good randomness
  for (size_t i = 0; i < length; ++i) {
    buf[i] = __llvm_libc::rand();
  }
  return length;
}

static inline size_t hamming_distance(uint64_t x, uint64_t y) {
#if defined LLVM_LIBC_WYHASH_TEST_HAS_BUILTIN_POPCOUNTLL
  return __builtin_popcountll(x ^ y);
#else
  size_t sum = 0;
  uint64_t current = x ^ y;
  while (current != 0) {
    sum += current & 0b1;
    current >>= 1;
  }
  return sum;
#endif
}

// We expect the hash value to be of low bias at each byte
// so that we can select a section of hash value as a second-level hash
TEST(LlvmLibcWyHashTest, LowBias) {
  using namespace __llvm_libc::internal::wyhash;
  static constexpr size_t SLOT_NUM = 1u << 8u;
  static constexpr size_t SCALE = 1000;
  static constexpr uint64_t SEED = 0x0123'4567'89AB'CDEFull;
  // Iterate through each byte
  for (size_t i = 0; i < sizeof(uint64_t) * 8; ++i) {
    size_t counter[SLOT_NUM];
    __llvm_libc::memset(counter, 0, sizeof(counter));
    size_t total = SCALE * SLOT_NUM;
    while (total--) {
      char buf[64];
      size_t length = random_string(buf);
      uint64_t hash = DefaultHash::hash(buf, length, SEED);
      // Get the section corresponds to the byte being examined
      counter[(hash >> (i * 8)) & 0xFF]++;
    }

    // Sum up to get the variance
    size_t sum = 0;
    for (size_t i = 0; i < SLOT_NUM; ++i)
      sum += static_cast<double>((counter[i] - SCALE) * (counter[i] - SCALE));

    // We hope that averaged bias at each slot is less than 10%.
    // Use square to avoid SQRT.
    ASSERT_LE(sum / SLOT_NUM, (SCALE / 10) * (SCALE / 10));
  }
}

// In real world, user may insert similar data into a hash set.
// To make the hash function robust, we hope a single-bit change is
// enough to change the hash value by a large extend.
TEST(LlvmLibcWyHashTest, AvalancheRandomString) {
  using namespace __llvm_libc::internal::wyhash;
  static constexpr size_t TEST_GROUP_NUM = 10000;
  static constexpr size_t TEST_GROUP_SIZE = 100;
  static constexpr uint64_t SEED = 0x89AB'CDEF'0123'4567ull;
  for (size_t i = 0; i < TEST_GROUP_NUM; ++i) {
    char buf[64];
    size_t length = random_string(buf);
    size_t original_hash = DefaultHash::hash(buf, length, SEED);
    for (size_t j = 0; j < TEST_GROUP_SIZE; ++j) {
      // Randomly select a byte
      size_t byte = __llvm_libc::rand() % length;
      // Randomly select a bit
      size_t bit = __llvm_libc::rand() % 8;
      // Alter the bit
      char backup = buf[byte];
      buf[byte] ^= (1u << bit);
      size_t new_hash = DefaultHash::hash(buf, length, SEED);
      // The hamming distance of the hash is larger than 20% of the hash volume.
      ASSERT_GE(hamming_distance(original_hash, new_hash),
                (8 * sizeof(uint64_t)) * 2 / 10);
      // Recover the string
      buf[byte] = backup;
    }
  }
}

TEST(LlvmLibcWyHashTest, AvalancheSmallValue) {
  using namespace __llvm_libc::internal::wyhash;
  static constexpr uint64_t SEED = 0xCDEF'89AB'4567'0123ull;
  for (size_t i = 0; i < 4096; ++i) {
    for (size_t j = i + 1; j < 4096; ++j) {
      size_t hash_i = DefaultHash::hash(&i, sizeof(i), SEED);
      size_t hash_j = DefaultHash::hash(&j, sizeof(j), SEED);
      // The hamming distance of the hash is larger than 20% of the hash volume.
      ASSERT_GE(hamming_distance(hash_i, hash_j),
                (8 * sizeof(uint64_t)) * 2 / 10);
    }
  }
}

// Sanity check
TEST(LlvmLibcWyHashTest, DefaultValues) {
  using namespace __llvm_libc::internal::wyhash;
  // clang-format off
  const char *data[] = {
    "",
    "a",
    "abc",
    "message digest",
    "abcdefghijklmnopqrstuvwxyz",
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789",
    "12345678901234567890123456789012345678901234567890123456789012345678901234567890" 
  };
  // generated with original WyHash implementation (ea3b25e1aef55d90f707c3a292eeb9162e2615d8)
  uint64_t hash[] = {
    0x0409638ee2bde459,
    0xa8412d091b5fe0a9,
    0x32dd92e4b2915153,
    0x8619124089a3a16b,
    0x7a43afb61d7f5f40,
    0xff42329b90e50d58,
    0xc39cab13b115aad3,

  };
  // clang-format on
  for (size_t i = 0; i < 7; ++i) {
    ASSERT_EQ(DefaultHash::hash(data[i], __llvm_libc::strlen(data[i]), i),
              hash[i]);
  }
}
