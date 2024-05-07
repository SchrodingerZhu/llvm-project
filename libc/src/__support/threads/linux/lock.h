//===--- Simple Timable Lock for Internal Usage -----------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_THREADS_LINUX_LOCK_H
#define LLVM_LIBC_SRC___SUPPORT_THREADS_LINUX_LOCK_H
#include "futex_word.h"
#include "src/__support/CPP/atomic.h"
#include "src/__support/CPP/optional.h"
#include "src/__support/macros/attributes.h"
#include "src/__support/threads/linux/futex_utils.h"
#include "src/__support/threads/linux/timespec_utils.h"
#include "src/__support/threads/sleep.h"

namespace LIBC_NAMESPACE {
// Lock is a simple timable lock for internal usage.
// This is separated from Mutex because this one does not need to consider
// rubustness and reentrancy. Also, this one has spin optimization for shorter
// critical sections.
class Lock {
  Futex futex_word;
  LIBC_INLINE_VAR static constexpr FutexWordType UNLOCKED = 0;
  LIBC_INLINE_VAR static constexpr FutexWordType LOCKED = 1;
  LIBC_INLINE_VAR static constexpr FutexWordType CONTENTED = 2;
  LIBC_INLINE_VAR static constexpr uint_fast32_t SPIN_COUNT = 100;

  LIBC_INLINE FutexWordType spin() {
    FutexWordType result;
    uint_fast32_t cnt = SPIN_COUNT;
    for (;;) {
      result = futex_word.load(cpp::MemoryOrder::RELAXED);
      if (result != LOCKED || cnt == 0)
        return result;
      sleep_briefly();
      cnt--;
    };
  }

  [[gnu::cold]]
  LIBC_INLINE bool lock_contented(cpp::optional<Futex::Timeout> timeout,
                                  bool is_shared) {
    FutexWordType state = spin();
    if (state == UNLOCKED && futex_word.compare_exchange_strong(
                                 state, LOCKED, cpp::MemoryOrder::ACQUIRE,
                                 cpp::MemoryOrder::RELAXED))
      return true;

    // do time correction before going into contention loop
    if (timeout && timeout->is_realtime) {
      timeout->abs_time =
          convert_clock(timeout->abs_time, CLOCK_REALTIME, CLOCK_MONOTONIC);
      timeout->is_realtime = false;
    }

    for (;;) {
      if (state != CONTENTED &&
          futex_word.exchange(CONTENTED, cpp::MemoryOrder::ACQUIRE) == UNLOCKED)
        return true;
      if (-ETIMEDOUT == futex_word.wait(CONTENTED, timeout, is_shared))
        return false;
      state = spin();
    }
  }

  [[gnu::cold]]
  LIBC_INLINE void wake(bool is_shared) {
    futex_word.notify_one(is_shared);
  }

public:
  LIBC_INLINE constexpr Lock() : futex_word(UNLOCKED) {}
  LIBC_INLINE bool try_lock() {
    FutexWordType expected = UNLOCKED;
    return futex_word.compare_exchange_strong(
        expected, LOCKED, cpp::MemoryOrder::ACQUIRE, cpp::MemoryOrder::RELAXED);
  }
  LIBC_INLINE bool lock(cpp::optional<Futex::Timeout> timeout = cpp::nullopt,
                        bool is_shared = false) {
    return try_lock() || lock_contented(timeout, is_shared);
  }
  LIBC_INLINE void unlock(bool is_shared = false) {
    // if there is someone waiting, wake them up
    if (futex_word.exchange(UNLOCKED, cpp::MemoryOrder::RELEASE) == CONTENTED)
      wake(is_shared);
  }
};
} // namespace LIBC_NAMESPACE

#endif // LLVM_LIBC_SRC___SUPPORT_THREADS_LINUX_LOCK_H
