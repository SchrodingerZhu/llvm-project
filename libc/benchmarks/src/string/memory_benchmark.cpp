//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Freestanding memory benchmarks using the hosted suite size distributions.
///
//===----------------------------------------------------------------------===//

#include "StringBenchmark.h"
#include "src/stdlib/free.h"

#if defined(LIBC_BENCHMARK_MEMCPY)
#include "src/string/memcpy.h"
#define FUNCTION memcpy
#define DISTRIBUTION_PREFIX Memcpy
#define COPY_OPERATION
#elif defined(LIBC_BENCHMARK_MEMMOVE)
#include "src/string/memmove.h"
#define FUNCTION memmove
#define DISTRIBUTION_PREFIX Memmove
#define COPY_OPERATION
#elif defined(LIBC_BENCHMARK_MEMSET)
#include "src/string/memset.h"
#define FUNCTION memset
#define DISTRIBUTION_PREFIX Memset
#elif defined(LIBC_BENCHMARK_BZERO)
#include "src/strings/bzero.h"
#define FUNCTION bzero
#define DISTRIBUTION_PREFIX Memset
#elif defined(LIBC_BENCHMARK_MEMCMP)
#include "src/string/memcmp.h"
#define FUNCTION memcmp
#define DISTRIBUTION_PREFIX Memcmp
#define COMPARE_OPERATION
#elif defined(LIBC_BENCHMARK_BCMP)
#include "src/strings/bcmp.h"
#define FUNCTION bcmp
#define DISTRIBUTION_PREFIX Memcmp
#define COMPARE_OPERATION
#else
#error "Select a memory benchmark function"
#endif

#define STRINGIFY_IMPL(x) #x
#define STRINGIFY(x) STRINGIFY_IMPL(x)
#define JOIN_IMPL(a, b) a##b
#define JOIN(a, b) JOIN_IMPL(a, b)
// clang-format off
#define DISTRIBUTION_FILE(name) \
  STRINGIFY(benchmarks/distributions/JOIN(DISTRIBUTION_PREFIX, name).csv)
// clang-format on

using namespace LIBC_NAMESPACE::benchmarks;
using namespace LIBC_NAMESPACE::benchmarks::string_benchmarks;

namespace {

auto invoke(unsigned char *dst, const unsigned char *src, size_t size) {
#if defined(COPY_OPERATION) || defined(COMPARE_OPERATION)
  return LIBC_NAMESPACE::FUNCTION(dst, src, size);
#elif defined(LIBC_BENCHMARK_MEMSET)
  (void)src;
  return LIBC_NAMESPACE::memset(dst, 0x5a, size);
#else
  (void)src;
  return LIBC_NAMESPACE::bzero(dst, size);
#endif
}

// Validation and warming are outside latency(). For overlapping moves, the
// expected bytes are captured before invoking the implementation under test.
void prepare(unsigned char *dst, unsigned char *src, size_t size,
             int mismatch = -1) {
  reset();
#ifdef COMPARE_OPERATION
  for (size_t i = 0; i < size; ++i)
    dst[i] = src[i];
  if (mismatch >= 0)
    dst[mismatch] ^= 0x80;
  const int result = invoke(dst, src, size);
#ifdef LIBC_BENCHMARK_MEMCMP
  const int expected =
      mismatch < 0 ? 0 : (dst[mismatch] > src[mismatch] ? 1 : -1);
  check((result > 0) - (result < 0) == expected);
#else
  check((result != 0) == (mismatch >= 0));
#endif
#else
  (void)mismatch;
  unsigned char expected[2 * BUFFER_SIZE];
  for (size_t i = 0; i < 2 * BUFFER_SIZE; ++i)
    expected[i] = storage()[i];
  for (size_t i = 0; i < size; ++i) {
#ifdef COPY_OPERATION
    expected[dst - storage() + i] = src[i];
#elif defined(LIBC_BENCHMARK_MEMSET)
    expected[dst - storage() + i] = 0x5a;
#else
    expected[dst - storage() + i] = 0;
#endif
  }
#ifdef COPY_OPERATION
  check(invoke(dst, src, size) == dst);
#elif defined(LIBC_BENCHMARK_MEMSET)
  check(invoke(dst, src, size) == dst);
#else
  invoke(dst, src, size);
#endif
  for (size_t i = 0; i < 2 * BUFFER_SIZE; ++i)
    check(storage()[i] == expected[i]);
#endif
}

template <size_t Size, size_t DstOffset, size_t SrcOffset, bool Overlap = false,
          int Mismatch = -1>
uint64_t fixed(uint32_t index) {
  auto *dst = left() + DstOffset;
  auto *src = (Overlap ? left() : right()) + SrcOffset;
  if (index == 0)
    prepare(dst, src, Size, Mismatch);
  return LIBC_NAMESPACE::latency([=] { return invoke(dst, src, Size); });
}

Parameters parameters[SAMPLE_COUNT];

template <const auto &Weights> uint64_t distribution(uint32_t index) {
  if (index == 0) {
    constexpr size_t count = sizeof(Weights) / sizeof(Weights[0]);
    static_assert(count <= MAX_SIZE + 1);
    const size_t bytes =
        (count * sizeof(double) + ALIGNMENT - 1) & ~(ALIGNMENT - 1);
    auto *cdf =
        static_cast<double *>(LIBC_NAMESPACE::aligned_alloc(ALIGNMENT, bytes));
    check(cdf != nullptr);
    double sum = 0;
    for (size_t i = 0; i < count; ++i) {
      sum += Weights[i];
      cdf[i] = sum;
    }
    check(sum > 0);
    uint32_t state = 0x9e3779b9;
    for (auto &p : parameters) {
      const double draw = (random(state) / 4294967296.0) * sum;
      size_t lo = 0, hi = count - 1;
      while (lo < hi) {
        const size_t mid = lo + (hi - lo) / 2;
        if (cdf[mid] <= draw)
          lo = mid + 1;
        else
          hi = mid;
      }
      p.size = static_cast<uint16_t>(lo);
      const uint32_t range = MAX_SIZE + ALIGNMENT - p.size + 1;
      p.dst_offset = static_cast<uint16_t>(random(state) % range);
      p.src_offset = static_cast<uint16_t>(random(state) % range);
    }
    LIBC_NAMESPACE::free(cdf);
    // Check all generated sizes and alignments before timing any of them.
    for (auto p : parameters) {
#ifdef LIBC_BENCHMARK_MEMMOVE
      prepare(left() + p.dst_offset, left() + p.src_offset, p.size);
#else
      prepare(left() + p.dst_offset, right() + p.src_offset, p.size);
#endif
    }
#ifdef COMPARE_OPERATION
    // Equal data must compare equal at every independently sampled offset.
    for (size_t i = 0; i < 2 * BUFFER_SIZE; ++i)
      storage()[i] = 0x5a;
#endif
    // Warm the same parameter sequence that subsequent samples will reuse.
    for (auto p : parameters) {
#ifdef LIBC_BENCHMARK_MEMMOVE
      invoke(left() + p.dst_offset, left() + p.src_offset, p.size);
#else
      invoke(left() + p.dst_offset, right() + p.src_offset, p.size);
#endif
    }
  }
  const auto p = parameters[index % SAMPLE_COUNT];
  auto *dst = left() + p.dst_offset;
#ifdef LIBC_BENCHMARK_MEMMOVE
  auto *src = left() + p.src_offset;
#else
  auto *src = right() + p.src_offset;
#endif
  return LIBC_NAMESPACE::latency([=] { return invoke(dst, src, p.size); });
}

#define REGISTER(Name, ...)                                                    \
  Benchmark Name(__VA_ARGS__, STRINGIFY(FUNCTION), #Name, 1, options())
#define REGISTER_SIZE(N)                                                       \
  REGISTER(Warm_##N##_Dst0_Src0, (fixed<N, 0, 0>));                            \
  REGISTER(Warm_##N##_Dst3_Src1, (fixed<N, 3, 1>))

REGISTER_SIZE(0);
REGISTER_SIZE(1);
REGISTER_SIZE(7);
REGISTER_SIZE(15);
REGISTER_SIZE(16);
REGISTER_SIZE(17);
REGISTER_SIZE(31);
REGISTER_SIZE(32);
REGISTER_SIZE(33);
REGISTER_SIZE(63);
REGISTER_SIZE(64);
REGISTER_SIZE(65);
REGISTER_SIZE(256);
REGISTER_SIZE(1024);
REGISTER_SIZE(4096);

#ifdef LIBC_BENCHMARK_MEMMOVE
#define REGISTER_OVERLAP(N)                                                    \
  REGISTER(Warm_##N##_ForwardOverlap, (fixed<N, 0, 1, true>));                 \
  REGISTER(Warm_##N##_BackwardOverlap, (fixed<N, 1, 0, true>))
REGISTER_OVERLAP(64);
REGISTER_OVERLAP(1024);
REGISTER_OVERLAP(4096);
#endif
#ifdef COMPARE_OPERATION
#define REGISTER_MISMATCH(N)                                                   \
  REGISTER(Warm_##N##_MismatchFirst, (fixed<N, 0, 0, false, 0>));              \
  REGISTER(Warm_##N##_MismatchLast, (fixed<N, 0, 0, false, N - 1>))
REGISTER_MISMATCH(64);
REGISTER_MISMATCH(1024);
REGISTER_MISMATCH(4096);
#endif

constexpr double GoogleA[] = {
#include DISTRIBUTION_FILE(GoogleA)
};
REGISTER(Warm_GoogleA, distribution<GoogleA>);
constexpr double GoogleB[] = {
#include DISTRIBUTION_FILE(GoogleB)
};
REGISTER(Warm_GoogleB, distribution<GoogleB>);
constexpr double GoogleD[] = {
#include DISTRIBUTION_FILE(GoogleD)
};
REGISTER(Warm_GoogleD, distribution<GoogleD>);
constexpr double GoogleL[] = {
#include DISTRIBUTION_FILE(GoogleL)
};
REGISTER(Warm_GoogleL, distribution<GoogleL>);
constexpr double GoogleM[] = {
#include DISTRIBUTION_FILE(GoogleM)
};
REGISTER(Warm_GoogleM, distribution<GoogleM>);
constexpr double GoogleQ[] = {
#include DISTRIBUTION_FILE(GoogleQ)
};
REGISTER(Warm_GoogleQ, distribution<GoogleQ>);
constexpr double GoogleS[] = {
#include DISTRIBUTION_FILE(GoogleS)
};
REGISTER(Warm_GoogleS, distribution<GoogleS>);
constexpr double GoogleU[] = {
#include DISTRIBUTION_FILE(GoogleU)
};
REGISTER(Warm_GoogleU, distribution<GoogleU>);
constexpr double GoogleW[] = {
#include DISTRIBUTION_FILE(GoogleW)
};
REGISTER(Warm_GoogleW, distribution<GoogleW>);

constexpr double Uniform[] = {
#include "benchmarks/distributions/Uniform384To4096.csv"
};
REGISTER(Warm_Uniform384To4096, distribution<Uniform>);

} // namespace
