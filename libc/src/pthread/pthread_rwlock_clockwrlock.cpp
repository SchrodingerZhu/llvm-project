//===-- Implementation of the Rwlock's clockwrlock function----------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "src/pthread/pthread_rwlock_clockwrlock.h"

#include "hdr/errno_macros.h"
#include "src/__support/common.h"
#include "src/__support/macros/config.h"
#include "src/__support/threads/linux/rwlock.h"
#include "src/__support/time/timeout.h"

#include <pthread.h>

namespace LIBC_NAMESPACE_DECL {

static_assert(
    sizeof(RwLock) == sizeof(pthread_rwlock_t) &&
        alignof(RwLock) == alignof(pthread_rwlock_t),
    "The public pthread_rwlock_t type must be of the same size and alignment "
    "as the internal rwlock type.");

LLVM_LIBC_FUNCTION(int, pthread_rwlock_clockwrlock,
                   (pthread_rwlock_t * rwlock, clockid_t clockid,
                    const timespec *abstime)) {
  if (!rwlock)
    return EINVAL;
  if (clockid != CLOCK_MONOTONIC && clockid != CLOCK_REALTIME)
    return EINVAL;
  RwLock *rw = reinterpret_cast<RwLock *>(rwlock);
  LIBC_ASSERT(abstime && "clockwrlock called with a null timeout");
  auto timeout = Timeout::timepoint(clockid, *abstime);
  if (LIBC_LIKELY(timeout.has_value()))
    return static_cast<int>(rw->write_lock(timeout.value()));

  switch (timeout.error()) {
  case Timeout::Error::Invalid:
    return EINVAL;
  case Timeout::Error::BeforeEpoch:
    return ETIMEDOUT;
  }
  __builtin_unreachable();
}

} // namespace LIBC_NAMESPACE_DECL
