//===--- Read-Write Lock =======================-----------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_THREADS_LINUX_RWLOCK_H
#define LLVM_LIBC_SRC___SUPPORT_THREADS_LINUX_RWLOCK_H
#include "futex_utils.h"
#include "futex_word.h"
#include "include/llvm-libc-types/pid_t.h"
#include "src/__support/CPP/atomic.h"
#include "src/__support/CPP/expected.h"
#include "src/__support/CPP/optional.h"
#include "src/__support/macros/attributes.h"
#include "src/__support/threads/linux/lock.h"
#include "timespec_utils.h"
#include <cstdint>
#include <optional>

namespace LIBC_NAMESPACE {
class Rwlock {
public:
  using Timeout = Futex::Timeout;
  enum class LockResult {
    Success = 0,
    Failed = EBUSY,
    Timeout = ETIMEDOUT,
    Overflow = EAGAIN,
    DeadLock = EDEADLK,
    Invalid = EINVAL,
  };

private:
  struct Queue {
    Lock lock;
    FutexWordType pending_reader_count;
    FutexWordType pending_writer_count;
    Futex reader_futex;
    Futex writer_futex;
    LIBC_INLINE constexpr Queue()
        : pending_reader_count(0), pending_writer_count(0), reader_futex(0),
          writer_futex(0) {}
  };

  cpp::Atomic<int> state;
  cpp::Atomic<pid_t> writer_tid;
  Queue queue;
  const bool is_shared;
  const bool prefer_writer;

  LIBC_INLINE void acquire() { queue.lock.lock(std::nullopt, is_shared); }

  LIBC_INLINE void release_queue() { queue.lock.unlock(is_shared); }
  // first bit indicates pending readers
  LIBC_INLINE_VAR constexpr static int PENDING_READERS_SHIFT = 0;
  LIBC_INLINE_VAR constexpr static int PENDING_READERS_BIT =
      1u << PENDING_READERS_SHIFT;

  // second bit indicates pending writers
  LIBC_INLINE_VAR constexpr static int PENDING_WRITERS_SHIFT = 1;
  LIBC_INLINE_VAR constexpr static int PENDING_WRITERS_BIT =
      1u << PENDING_WRITERS_SHIFT;
  // highest bit indicates writer ownership
  LIBC_INLINE_VAR constexpr static int WRITER_SHIFT = sizeof(int) * 8 - 1;
  LIBC_INLINE_VAR constexpr static int WRITER_BIT = 1u << WRITER_SHIFT;

  // middle bits indicate active reader count
  LIBC_INLINE_VAR constexpr static int READER_COUNT_SHIFT = 2;
  LIBC_INLINE_VAR constexpr static int READER_COUNT_UNIT =
      1u << READER_COUNT_SHIFT;
  // bit mask for checking if any pending readers or writers
  LIBC_INLINE_VAR constexpr static int PENDING_MASK =
      PENDING_READERS_BIT | PENDING_WRITERS_BIT;
  LIBC_INLINE_VAR constexpr static uint_fast32_t SPIN_LIMIT = 100;

  LIBC_INLINE static bool owned_by_writer(int state) { return state < 0; }
  LIBC_INLINE static bool owned_by_reader(int state) {
    return state >= READER_COUNT_UNIT;
  }
  LIBC_INLINE static bool owned_by_anyone(int state) {
    return owned_by_reader(state) || owned_by_writer(state);
  }
  LIBC_INLINE static int add_writer_flag(int state) {
    return state | PENDING_READERS_BIT;
  }
  LIBC_INLINE static bool is_last_reader(int state) {
    return (state >> READER_COUNT_SHIFT) == 1;
  }
  LIBC_INLINE static bool has_pending_writer(int state) {
    return state & PENDING_WRITERS_BIT;
  }
  LIBC_INLINE static bool has_pending(int state) {
    return state & PENDING_MASK;
  }
  LIBC_INLINE LockResult check_timespec(cpp::optional<Timeout> &timeout) {
    if (!timeout)
      return LockResult::Success;
    if (timeout->abs_time.tv_nsec < 0 ||
        timeout->abs_time.tv_nsec >= 1'000'000'000)
      return LockResult::Invalid;
    if (timeout->abs_time.tv_sec < 0)
      return LockResult::Timeout;
    /// TODO: gate this with a feature flag
    if (timeout->is_realtime) {
      timeout->abs_time =
          convert_clock(timeout->abs_time, CLOCK_REALTIME, CLOCK_MONOTONIC);
      timeout->is_realtime = false;
    }
    return LockResult::Success;
  }

  LIBC_INLINE bool is_read_lockable(int state) {
    bool cannot =
        owned_by_writer(state) || (prefer_writer && has_pending_writer(state));
    return !cannot;
  }
  LIBC_INLINE bool is_write_lockable(int state) {
    return !owned_by_anyone(state);
  }

  /// TODO: use internal function to gettid directly once it is available
  LIBC_INLINE static pid_t get_tid() {
    static thread_local pid_t tid = 0;
    if (tid == 0)
      tid = syscall_impl<pid_t>(SYS_gettid);
    return tid;
  }

  template <class F> LIBC_INLINE int spin_until(F &&func) {
    uint_fast32_t cnt = SPIN_LIMIT;
    for (;;) {
      state = this->state.load(cpp::MemoryOrder::RELAXED);
      if (func(state) || cnt == 0)
        return state;
      sleep_briefly();
      cnt--;
    }
  }

  LIBC_INLINE int spin_read() {
    return spin_until([this](int state) {
      return is_read_lockable(state) || has_pending(state);
    });
  }

  [[gnu::cold]]
  LIBC_INLINE LockResult read_contented(cpp::optional<Timeout> timeout) {
    if (writer_tid.load(cpp::MemoryOrder::RELAXED) == get_tid())
      return LockResult::DeadLock;
    LockResult result = check_timespec(timeout);
    if (result != LockResult::Success)
      return result;
    // Do several spin until we find that either the lock is free or the lock is
    // contended
    int old_state = spin_read();
    for (;;) {
      if (is_read_lockable(old_state)) {
        if (__builtin_sadd_overflow(old_state, READER_COUNT_UNIT, &old_state))
          return LockResult::Overflow;
        if (state.compare_exchange_weak(old_state, old_state,
                                        cpp::MemoryOrder::ACQUIRE,
                                        cpp::MemoryOrder::RELAXED))
          return LockResult::Success;
        continue;
      }
      // now, we know that lock is likely to be contended, we should go sleep.
      acquire_queue(timeout);
    }
  }

public:
  LIBC_INLINE constexpr Rwlock(bool is_shared = false,
                               bool prefer_writer = false)
      : state(0), writer_tid(0), queue(), is_shared(is_shared),
        prefer_writer(prefer_writer) {}
  /// Check if any thread is working on the lock.
  LIBC_INLINE bool is_cleared() {
    return state.load(cpp::MemoryOrder::RELAXED) == 0;
  }

  LIBC_INLINE LockResult try_read() {
    int old_state = state.load(cpp::MemoryOrder::RELAXED);
    // if the lock state continues to be read lockable, we know that the whole
    // system is making progress (i.e. it is not a blocking case), so we
    // continue to retry.
    while (is_read_lockable(old_state)) {
      int new_state;
      if (__builtin_sadd_overflow(old_state, READER_COUNT_UNIT, &new_state))
        return LockResult::Overflow;
      if (state.compare_exchange_weak(old_state, new_state,
                                      cpp::MemoryOrder::ACQUIRE,
                                      cpp::MemoryOrder::RELAXED))
        return LockResult::Success;
    }
    return LockResult::Failed;
  }

  LIBC_INLINE LockResult try_write() {
    int old_state = state.load(cpp::MemoryOrder::RELAXED);
    while (is_write_lockable(old_state)) {
      if (state.compare_exchange_weak(old_state, add_writer_flag(old_state),
                                      cpp::MemoryOrder::ACQUIRE,
                                      cpp::MemoryOrder::RELAXED)) {

        pid_t tid = get_tid();
        writer_tid.store(tid, cpp::MemoryOrder::RELAXED);
        return LockResult::Success;
      }
    }
    return LockResult::Failed;
  }

  LIBC_INLINE LockResult read(cpp::optional<Timeout> timeout = cpp::nullopt) {
    LockResult result = try_read();
    if (result == LockResult::Success || result == LockResult::Overflow)
      return result;
    return read_contented(timeout);
  }
};

} // namespace LIBC_NAMESPACE

#endif // LLVM_LIBC_SRC___SUPPORT_THREADS_LINUX_RWLOCK_H
