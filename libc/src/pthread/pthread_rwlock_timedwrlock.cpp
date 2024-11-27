//===-- Implementation for Rwlock's timedwrlock function ------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "src/pthread/pthread_rwlock_timedwrlock.h"

#include "hdr/time_macros.h"
#include "src/__support/common.h"
#include "src/__support/libc_assert.h"
#include "src/__support/macros/config.h"
#include "src/__support/macros/optimization.h"
#include "src/__support/threads/linux/rwlock.h"
#include "src/__support/time/timeout.h"

#include <pthread.h>

namespace LIBC_NAMESPACE_DECL {

LLVM_LIBC_FUNCTION(int, pthread_rwlock_timedwrlock,
                   (pthread_rwlock_t *__restrict rwlock,
                    const struct timespec *__restrict abstime)) {
  if (!rwlock)
    return EINVAL;
  RwLock *rw = reinterpret_cast<RwLock *>(rwlock);
  LIBC_ASSERT(abstime && "timedwrlock called with a null timeout");
  auto timeout = Timeout::timepoint(CLOCK_REALTIME, *abstime);
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
