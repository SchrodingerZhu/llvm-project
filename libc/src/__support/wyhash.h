//===-- A 64-bit Hash Function ----------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "src/__support/UInt128.h"
#include "src/__support/common.h"

// WyHash comes from https://github.com/wangyi-fudan/wyhash/ (which has been
// released into public domain). It is also the default hash function
// of Go, Nim and Zig.
//
// According to SMHasher's results, it is one of the fastest hash functions
// without primary quality issues. This hash function can be used with
// swisstable as it has the Avalanche effect to encode the data into full
// 64-bits; thus the table can effectively utilize two level hash technique
// to effectively locate candidate entries without too many false positives.
namespace __llvm_libc::internal::wyhash {
// default secret parameters from WyHash
// commit hash: ea3b25e1aef55d90f707c3a292eeb9162e2615d8
static constexpr inline uint64_t DEFAULT_SECRET[4] = {
    0xa0761d6478bd642full, 0xe7037ed1a0b428dbull, 0x8ebc6af09c88c6e3ull,
    0x589965cc75374cc3ull};
template <bool EntropyProtection> class WyHash {
  static void multiply(uint64_t &a, uint64_t &b) {
    UInt128 data{a};
    data *= b;
    if constexpr (EntropyProtection) {
      a ^= static_cast<uint64_t>(data);
      b ^= static_cast<uint64_t>(data >> 64);
    } else {
      a = static_cast<uint64_t>(data);
      b = static_cast<uint64_t>(data >> 64);
    }
  }
  static uint64_t mix(uint64_t a, uint64_t b) {
    multiply(a, b);
    return a ^ b;
  }
  template <size_t N> static uint64_t read(const uint8_t *p) {
    uint64_t value = 0;
    for (size_t i = 0; i < N; ++i) {
      value |= (static_cast<uint64_t>(p[i]) << (i * 8));
    }
    return value;
  }

  static uint64_t read3(const uint8_t *p, size_t k) {
    auto a = static_cast<uint64_t>(p[0]) << 16;
    auto b = static_cast<uint64_t>(p[k / 2]) << 8;
    auto c = static_cast<uint64_t>(p[k - 1]);
    return a | b | c;
  }

  static uint64_t wyhash(const void *key, size_t length, uint64_t seed,
                         const uint64_t *secret) {
    const uint8_t *p = static_cast<const uint8_t *>(key);
    seed ^= mix(seed ^ secret[0], secret[1]);
    uint64_t a = 0, b = 0;
    if (likely(length <= 16)) {
      if (likely(length >= 4)) {
        a = (read<4>(p) << 32) | read<4>(p + ((length >> 3) << 2));
        b = (read<4>(p + length - 4) << 32) |
            read<4>(p + length - 4 - ((length >> 3) << 2));
      } else if (likely(length > 0)) {
        a = read3(p, length);
        b = 0;
      }
    } else {
      size_t i = length;
      if (likely(i > 48)) {
        uint64_t s1 = seed, s2 = seed;
        do {
          seed = mix(read<8>(p) ^ secret[1], read<8>(p + 8) ^ seed);
          s1 = mix(read<8>(p + 16) ^ secret[2], read<8>(p + 24) ^ s1);
          s2 = mix(read<8>(p + 32) ^ secret[3], read<8>(p + 40) ^ s2);
          p += 48;
          i -= 48;
        } while (likely(i > 48));
        seed ^= s1 ^ s2;
      }
      while (unlikely(i > 16)) {
        seed = mix(read<8>(p) ^ secret[1], read<8>(p + 8) ^ seed);
        i -= 16;
        p += 16;
      }
      a = read<8>(p + i - 16);
      b = read<8>(p + i - 8);
    }
    a ^= secret[1];
    b ^= seed;
    multiply(a, b);
    return mix(a ^ secret[0] ^ length, b ^ secret[1]);
  }

public:
  static uint64_t hash(const void *key, size_t length, uint64_t seed) {
    return wyhash(key, length, seed, DEFAULT_SECRET);
  }
};

// Follow the practice in Golang to disable low-entropy protection by default.
using DefaultHash = WyHash<false>;

} // namespace __llvm_libc::internal::wyhash
