//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SUPPORT_H
#define LLVM_LIBC_SUPPORT_H

#include <stddef.h>
#include <stdint.h>
#include <assert.h>
#include <new>
#include <utility>
#include <stdio.h>

#define LIBC_NAMESPACE __llvm_libc_flat_tlsf2
#define LIBC_INLINE inline
#define LIBC_INLINE_VAR inline
#define LIBC_DECL
#define LIBC_NAMESPACE_DECL __llvm_libc_flat_tlsf2
#define LIBC_CONSTINIT

#define LIBC_ASSERT(expr) assert(expr)

namespace LIBC_NAMESPACE {

// Minimal standard-compliant C++17 span replacement
namespace cpp {
  enum class byte : unsigned char {};

  template <typename T>
  class span {
    T* data_;
    size_t size_;
  public:
    constexpr span() : data_(nullptr), size_(0) {}
    constexpr span(T* ptr, size_t count) : data_(ptr), size_(count) {}
    constexpr span(T* first, T* last) : data_(first), size_(last - first) {}
    
    // Static array constructor helper
    template <size_t N>
    constexpr span(T (&arr)[N]) : data_(arr), size_(N) {}

    constexpr T* data() const { return data_; }
    constexpr size_t size() const { return size_; }
    constexpr T* begin() const { return data_; }
    constexpr T* end() const { return data_ + size_; }
    constexpr T& operator[](size_t idx) const { return data_[idx]; }
    
    constexpr span<T> subspan(size_t offset) const {
      return span<T>(data_ + offset, size_ - offset);
    }
    constexpr span<T> subspan(size_t offset, size_t count) const {
      return span<T>(data_ + offset, count);
    }
  };

  // Minimal standard-compliant C++17 optional replacement
  struct nullopt_t {};
  constexpr nullopt_t nullopt{};

  template <typename T>
  class optional {
    union {
      char dummy;
      T value;
    };
    bool has_value_;

  public:
    constexpr optional() : dummy(0), has_value_(false) {}
    constexpr optional(nullopt_t) : dummy(0), has_value_(false) {}
    constexpr optional(const T& val) : value(val), has_value_(true) {}
    constexpr optional(T&& val) : value(std::move(val)), has_value_(true) {}

    ~optional() {
      if (has_value_) {
        value.~T();
      }
    }

    constexpr bool has_value() const { return has_value_; }
    constexpr bool operator!() const { return !has_value_; }
    constexpr explicit operator bool() const { return has_value_; }
    
    constexpr T& operator*() { return value; }
    constexpr const T& operator*() const { return value; }
    constexpr T* operator->() { return &value; }
    constexpr const T* operator->() const { return &value; }
    
    constexpr T value_or(const T& def) const { return has_value_ ? value : def; }
  };

  // Minimal standard-compliant C++17 array replacement
  template <typename T, size_t N>
  struct array {
    T data_[N];
    constexpr T& operator[](size_t idx) { return data_[idx]; }
    constexpr const T& operator[](size_t idx) const { return data_[idx]; }
    constexpr T* begin() { return data_; }
    constexpr const T* begin() const { return data_; }
    constexpr T* end() { return data_ + N; }
    constexpr const T* end() const { return data_ + N; }
    constexpr size_t size() const { return N; }
  };

  // Standard bitcast helper wrapping compiler builtins
  template <typename To, typename From>
  LIBC_INLINE To bit_cast(const From& src) {
    static_assert(sizeof(To) == sizeof(From), "bit_cast sizes must match");
    To dst;
    __builtin_memcpy(&dst, &src, sizeof(To));
    return dst;
  }

  template <typename T>
  constexpr T max(T a, T b) { return a > b ? a : b; }

  template <typename T>
  constexpr T min(T a, T b) { return a < b ? a : b; }

  // High-speed bit_ceil implementation using count leading zeros
  LIBC_INLINE size_t bit_ceil(size_t x) {
    if (x <= 1) return 1;
    int lz = __builtin_clzll(x - 1);
    return 1ULL << (64 - lz);
  }

  template <typename T>
  LIBC_INLINE constexpr int countr_zero(T val) {
    if (val == 0) return sizeof(T) * 8;
    if (sizeof(T) <= 4) {
      return __builtin_ctz(static_cast<uint32_t>(val));
    } else {
      return __builtin_ctzll(static_cast<uint64_t>(val));
    }
  }
} // namespace cpp

// Native high-performance logarithm calculation using hardware intrinsics
LIBC_INLINE constexpr uint32_t ilog2(size_t x) {
  if (x <= 1) return 0;
  return 63 - __builtin_clzll(x);
}

template <typename T>
LIBC_INLINE bool mul_overflow(T a, T b, T& res) {
  return __builtin_mul_overflow(a, b, &res);
}

template <typename T>
LIBC_INLINE bool add_overflow(T a, T b, T& res) {
  return __builtin_add_overflow(a, b, &res);
}

template <typename T>
LIBC_INLINE constexpr T align_down(T value, size_t alignment) {
  return (value / alignment) * alignment;
}

template <typename T>
LIBC_INLINE constexpr T align_up(T value, size_t alignment) {
  return align_down(value + alignment - 1, alignment);
}

// Low-level high-speed memory operations wrapping compiler builtins
LIBC_INLINE void* inline_memcpy(void* dst, const void* src, size_t count) {
  return __builtin_memcpy(dst, src, count);
}
LIBC_INLINE void* inline_memset(void* dst, int val, size_t count) {
  return __builtin_memset(dst, val, count);
}
LIBC_INLINE void* inline_memmove(void* dst, const void* src, size_t count) {
  return __builtin_memmove(dst, src, count);
}

} // namespace LIBC_NAMESPACE

#endif // LLVM_LIBC_SUPPORT_H
