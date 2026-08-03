//===-- asan_baremetal.cpp ----------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file is a part of AddressSanitizer, an address sanity checker.
//
// Baremetal platform-specific runtime routines.
//===----------------------------------------------------------------------===//

#if ASAN_BAREMETAL

#include "asan_internal.h"
#include "asan_mapping.h"
#include "asan_thread.h"
#include "sanitizer_common/sanitizer_common.h"

// Optional weak firmware/linker hooks for embedded device initialization
extern "C" SANITIZER_WEAK_ATTRIBUTE void __asan_baremetal_setup_mpu();
extern "C" SANITIZER_WEAK_ATTRIBUTE void __asan_baremetal_init_shadow();

namespace __asan {

static void *baremetal_per_thread = nullptr;

void InitializeShadowMemory() {
  if (&__asan_baremetal_init_shadow)
    __asan_baremetal_init_shadow();
  if (&__asan_baremetal_setup_mpu)
    __asan_baremetal_setup_mpu();
}

void AsanApplyToGlobals(globals_op_fptr op, const void *needle) {
  // Global metadata can be discovered statically from ELF sections if required.
}

void AsanCheckDynamicRTPrereqs() {}
void AsanCheckIncompatibleRT() {}
void InitializeAsanInterceptors() {}
void InitializePlatformExceptionHandlers() {}

void AsanOnDeadlySignal(int signo, void *siginfo, void *context) {}

bool PlatformUnpoisonStacks() {
  return false;
}

void *AsanTSDGet() { return baremetal_per_thread; }
void AsanTSDSet(void *tsd) { baremetal_per_thread = tsd; }
void AsanTSDInit(void (*destructor)(void *tsd)) {}
void PlatformTSDDtor(void *tsd) {}

void AppendToErrorMessageBuffer(const char *buffer) {}
void *AsanDlSymNext(const char *sym) { return nullptr; }

AsanThread *CreateMainThread() {
  // Baremetal systems typically operate in a single main executive thread
  // or simple RTOS tasks; return nullptr or static main thread abstraction.
  return nullptr;
}

}  // namespace __asan

#endif  // ASAN_BAREMETAL
