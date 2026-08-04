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

#define SANITIZER_COMMON_NO_REDEFINE_BUILTINS

#include "asan_internal.h"
#include "asan_mapping.h"
#include "asan_thread.h"
#include "asan_allocator.h"
#include "asan_fake_stack.h"
#include "asan_interceptors.h"
#include "asan_interceptors_memintrinsics.h"
#include "asan_interface_internal.h"
#include "asan_report.h"
#include "asan_stack.h"
#include "asan_suppressions.h"
#include "sanitizer_common/sanitizer_common.h"

using namespace __asan;
using namespace __sanitizer;

// Default empty weak stub implementations for firmware/linker hooks
extern "C" {
SANITIZER_WEAK_ATTRIBUTE void __asan_baremetal_setup_mpu() {}
SANITIZER_WEAK_ATTRIBUTE void __asan_baremetal_init_shadow() {}
SANITIZER_WEAK_ATTRIBUTE extern char __StackTop;
SANITIZER_WEAK_ATTRIBUTE extern char __StackBottom;

SANITIZER_INTERFACE_ATTRIBUTE void __asan_alloca_poison(uptr addr, uptr size) {
  __asan_poison_memory_region((void *)addr, size);
}
SANITIZER_INTERFACE_ATTRIBUTE void __asan_allocas_unpoison(uptr top, uptr bottom) {
  if (top > bottom)
    __asan_unpoison_memory_region((void *)bottom, top - bottom);
  else
    __asan_unpoison_memory_region((void *)top, bottom - top);
}
SANITIZER_INTERFACE_ATTRIBUTE void *__asan_stack_malloc_0(uptr size) { return nullptr; }
SANITIZER_INTERFACE_ATTRIBUTE void __asan_stack_free_0(uptr ptr, uptr size) {}
SANITIZER_INTERFACE_ATTRIBUTE void __asan_print_accumulated_stats() {}

SANITIZER_INTERFACE_ATTRIBUTE void *__asan_memcpy(void *dst, const void *src, uptr size) {
  if (AsanInited()) {
    ASAN_READ_RANGE(nullptr, src, size);
    ASAN_WRITE_RANGE(nullptr, dst, size);
  }
  return internal_memcpy(dst, src, size);
}
SANITIZER_INTERFACE_ATTRIBUTE void *__asan_memset(void *s, int c, uptr n) {
  if (AsanInited()) {
    ASAN_WRITE_RANGE(nullptr, s, n);
  }
  return internal_memset(s, c, n);
}
}  // extern "C"

namespace __interception {
memset_type real_memset = (memset_type)&internal_memset;
memcpy_type real_memcpy = (memcpy_type)&internal_memcpy;
memmove_type real_memmove = (memmove_type)&internal_memmove;
}  // namespace __interception

namespace __asan {

static void *baremetal_per_thread = nullptr;

void InitializeShadowMemory() {
  __asan_baremetal_init_shadow();
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

void *AsanDlSymNext(const char *sym) { return nullptr; }

// Allocator stubs when using standalone FreeListHeap
void InitializeAllocator(const AllocatorOptions &options) {}
void ReInitializeAllocator(const AllocatorOptions &options) {}
void GetAllocatorOptions(AllocatorOptions *options) {}
void ApplyAllocatorOptions(const AllocatorOptions &options) {}
void AllocatorOptions::SetFrom(const Flags *f, const CommonFlags *cf) {}
void AllocatorOptions::CopyTo(Flags *f, CommonFlags *cf) {}
void ReplaceSystemMalloc() {}
void InstallAtForkHandler() {}

AsanChunkView FindHeapChunkByAddress(uptr addr) { return AsanChunkView(nullptr); }
bool AsanChunkView::IsValid() const { return false; }
bool AsanChunkView::IsAllocated() const { return false; }
bool AsanChunkView::IsQuarantined() const { return false; }
uptr AsanChunkView::AllocTid() const { return kInvalidTid; }
uptr AsanChunkView::FreeTid() const { return kInvalidTid; }
uptr AsanChunkView::Beg() const { return 0; }
uptr AsanChunkView::End() const { return 0; }
uptr AsanChunkView::UsedSize() const { return 0; }
u32 AsanChunkView::UserRequestedAlignment() const { return 0; }
u32 AsanChunkView::GetAllocStackId() const { return 0; }
u32 AsanChunkView::GetFreeStackId() const { return 0; }
AllocType AsanChunkView::GetAllocType() const { return FROM_MALLOC; }

void AsanThreadLocalMallocStorage::CommitBack() {}
void FlushToDeadThreadStats(AsanStats *stats) {}

// FakeStack fallback stubs
uptr FakeStack::AddrIsInFakeStack(uptr addr, uptr *real_stack,
                                  uptr *begin) { return 0; }
FakeStack *FakeStack::Create(uptr stack_size_log) { return nullptr; }
void FakeStack::Destroy(int tid) {}
void FakeStack::ForEachFakeFrame(void (*cb)(uptr, uptr, void*), void *arg) {}
void FakeStack::HandleNoReturn() {}
void ResetTLSFakeStack() {}

void AsanThread::SetThreadStackAndTls(const InitOptions *options) {
  stack_bottom_ = (uptr)&__StackBottom;
  stack_top_ = (uptr)&__StackTop;
  tls_begin_ = tls_end_ = 0;
  dtls_ = nullptr;
}

AsanThread *CreateMainThread() {
  AsanThread *main_thread = AsanThread::Create(kMainTid, nullptr, true);
  SetCurrentThread(main_thread);
  main_thread->Init(nullptr);
  return main_thread;
}

}  // namespace __asan

#endif  // ASAN_BAREMETAL
