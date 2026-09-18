//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Freestanding string length, comparison, and search benchmarks.
///
//===----------------------------------------------------------------------===//

#include "StringBenchmark.h"

#if defined(LIBC_BENCHMARK_STRLEN)
#include "src/string/strlen.h"
#define FUNCTION strlen
#elif defined(LIBC_BENCHMARK_STRCMP)
#include "src/string/strcmp.h"
#define FUNCTION strcmp
#elif defined(LIBC_BENCHMARK_MEMCHR)
#include "src/string/memchr.h"
#define FUNCTION memchr
#elif defined(LIBC_BENCHMARK_STRCHR)
#include "src/string/strchr.h"
#define FUNCTION strchr
#else
#error "Select a string benchmark function"
#endif

using namespace LIBC_NAMESPACE::benchmarks;
using namespace LIBC_NAMESPACE::benchmarks::string_benchmarks;

namespace {

template <size_t Size, size_t Offset, int Position = -1,
          bool Terminator = false>
uint64_t run(uint32_t index) {
  auto *lhs = reinterpret_cast<char *>(left() + Offset);
  auto *rhs = reinterpret_cast<char *>(right() + Offset);
  [[maybe_unused]] constexpr int needle = Terminator ? 0 : '!';
  const auto call = [=] {
#if defined(LIBC_BENCHMARK_STRLEN)
    return LIBC_NAMESPACE::strlen(lhs);
#elif defined(LIBC_BENCHMARK_STRCMP)
    return LIBC_NAMESPACE::strcmp(lhs, rhs);
#elif defined(LIBC_BENCHMARK_MEMCHR)
    return LIBC_NAMESPACE::memchr(lhs, needle, Size);
#else
    return LIBC_NAMESPACE::strchr(lhs, needle);
#endif
  };
  if (index == 0) {
    reset();
    for (size_t i = 0; i < Size; ++i)
      lhs[i] = rhs[i] = static_cast<char>('a' + i % 23);
    lhs[Size] = rhs[Size] = 0;
    if constexpr (Position >= 0) {
      static_assert(Position < Size);
#ifdef LIBC_BENCHMARK_STRCMP
      ++rhs[Position];
#else
      lhs[Position] = needle;
#endif
    }
    // Validate and warm without including either step in the cycle sample.
    const auto result = call();
#if defined(LIBC_BENCHMARK_STRLEN)
    check(result == Size);
#elif defined(LIBC_BENCHMARK_STRCMP)
    check(Position < 0 ? result == 0 : result < 0);
#else
    char *expected = Position < 0 ? nullptr : lhs + Position;
#ifdef LIBC_BENCHMARK_STRCHR
    if constexpr (Terminator)
      expected = lhs + Size;
#endif
    check(result == expected);
#endif
  }
  return LIBC_NAMESPACE::latency(call);
}

#define STRINGIFY_IMPL(x) #x
#define STRINGIFY(x) STRINGIFY_IMPL(x)
#define REGISTER(Name, ...)                                                    \
  Benchmark Name(__VA_ARGS__, STRINGIFY(FUNCTION), #Name, 1, options())
#define REGISTER_SIZE(N)                                                       \
  REGISTER(Warm_##N##_Offset0, (run<N, 0>));                                   \
  REGISTER(Warm_##N##_Offset1, (run<N, 1>))

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

#if !defined(LIBC_BENCHMARK_STRLEN)
#define REGISTER_POSITION(N)                                                   \
  REGISTER(Warm_##N##_First, (run<N, 0, 0>));                                  \
  REGISTER(Warm_##N##_Middle, (run<N, 0, N / 2>));                             \
  REGISTER(Warm_##N##_Last, (run<N, 1, N - 1>))
REGISTER_POSITION(64);
REGISTER_POSITION(1024);
REGISTER_POSITION(4096);
#endif
#if defined(LIBC_BENCHMARK_STRCHR) || defined(LIBC_BENCHMARK_MEMCHR)
REGISTER(Warm_64_FindNul, (run<64, 0, -1, true>));
REGISTER(Warm_4096_FindNul, (run<4096, 1, -1, true>));
#endif

} // namespace
