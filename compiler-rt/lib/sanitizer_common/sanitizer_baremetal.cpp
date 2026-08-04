//===-- sanitizer_baremetal.cpp -------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file is shared between varied sanitizers' runtime libraries.
//
// Baremetal specific platform implementation.
//===----------------------------------------------------------------------===//

#include "sanitizer_platform.h"
#if SANITIZER_BAREMETAL

#include "sanitizer_common.h"
#include "sanitizer_allocator_internal.h"
#include "sanitizer_file.h"
#include "sanitizer_flags.h"
#include "sanitizer_internal_defs.h"
#include "sanitizer_libc.h"
#include "sanitizer_mutex.h"
#include "sanitizer_procmaps.h"
#include "sanitizer_stackdepot.h"
#include "sanitizer_stacktrace.h"
#include "sanitizer_symbolizer.h"

// Default empty weak stub implementations for firmware synchronization and halting
extern "C" {
SANITIZER_WEAK_ATTRIBUTE void __asan_baremetal_mutex_lock() {}
SANITIZER_WEAK_ATTRIBUTE void __asan_baremetal_mutex_unlock() {}
SANITIZER_WEAK_ATTRIBUTE void __asan_baremetal_futex_wait(void *addr, unsigned val) {
  while (__sanitizer::atomic_load_relaxed((__sanitizer::atomic_uint32_t *)addr) == val) {}
}
SANITIZER_WEAK_ATTRIBUTE void __asan_baremetal_futex_wake(void *addr, unsigned count) {}
SANITIZER_WEAK_ATTRIBUTE void __asan_baremetal_write(const void *buffer, __sanitizer::uptr count) {
  (void)buffer;
  (void)count;
}
SANITIZER_WEAK_ATTRIBUTE void __asan_baremetal_halt() {
  while (true) {}
}
SANITIZER_WEAK_ATTRIBUTE extern char __StackTop;
SANITIZER_WEAK_ATTRIBUTE extern char __StackBottom;
}

namespace __sanitizer {

extern "C" void *malloc(uptr size);
extern "C" void free(void *ptr);

int Atexit(void (*fp)()) { return 0; }
void CheckASLR() {}
void DisableCoreDumperIfNecessary() {}
void DumpProcessMap() {}
bool ErrorIsOOM(int) { return false; }

void FutexWait(atomic_uint32_t *p, unsigned int cmp) {
  __asan_baremetal_futex_wait(p, cmp);
}

void FutexWake(atomic_uint32_t *p, unsigned int count) {
  __asan_baremetal_futex_wake(p, count);
}

char **GetArgv() { return nullptr; }
char **GetEnviron() { return nullptr; }
const char *GetEnv(const char *name) { return nullptr; }

uptr GetMmapGranularity() { return 4096; }
uptr GetPageSize() { return 4096; }
uptr GetThreadSelf() { return 0; }
ThreadID GetTid() { return 0; }

void GetThreadStackAndTls(bool main, uptr *stk_begin, uptr *stk_end,
                          uptr *tls_begin, uptr *tls_end) {
  if (stk_begin && stk_end) {
    if (&__StackBottom && &__StackTop) {
      *stk_begin = (uptr)&__StackBottom;
      *stk_end = (uptr)&__StackTop;
    } else {
      *stk_begin = 0x2006f800;
      *stk_end = 0x20070000;
    }
  }
  if (tls_begin && tls_end) {
    *tls_begin = 0;
    *tls_end = 0;
  }
}

void InitializeCoverage(bool, const char *) {}
void InitializePlatformEarly() {}
void InstallDeadlySignalHandlers(void (*)(int, void *, void *)) {}

void internal__exit(int exitcode) {
  __asan_baremetal_halt();
  while (true) {}
}

uptr internal_getpid() { return 0; }
uptr internal_sched_yield() { return 0; }
void internal_usleep(u64) {}
void *internal_start_thread(void *(*func)(void *), void *arg) { return nullptr; }
void internal_join_thread(void *) {}

bool IsAbsolutePath(const char *) { return false; }
bool IsPathSeparator(char c) { return c == '/'; }
bool IsAccessibleMemoryRange(uptr beg, uptr size) { return true; }
bool MprotectReadOnly(uptr, uptr) { return false; }
u64 MonotonicNanoTime() { return 0; }

void *MmapOrDie(uptr size, const char *mem_type, bool raw_report) {
  void *res = malloc(size);
  if (!res && !raw_report)
    Die();
  return res;
}

void *MmapOrDieOnFatalError(uptr size, const char *mem_type) {
  return malloc(size);
}

void *MmapNoReserveOrDie(uptr size, const char *mem_type) {
  return malloc(size);
}

void *MmapAlignedOrDieOnFatalError(uptr size, uptr alignment,
                                   const char *mem_type) {
  return malloc(size);
}

void UnmapOrDie(void *addr, uptr size, bool raw_report) {
  free(addr);
}

void ReserveShadowMemoryRange(uptr beg, uptr size, const char *name, bool) {}

uptr ReadBinaryName(char *, uptr) { return 0; }
uptr ReadLongProcessName(char *, uptr) { return 0; }

bool FileExists(const char *) { return false; }
bool DirExists(const char *) { return false; }
fd_t OpenFile(const char *, FileAccessMode, error_t *) { return kInvalidFd; }
void CloseFile(int) {}
bool ReadFromFile(int, void *, uptr, uptr *, int *) { return false; }
bool WriteToFile(int, const void *buffer, uptr count, uptr *bytes_written, int *) {
  __asan_baremetal_write(buffer, count);
  if (bytes_written)
    *bytes_written = count;
  return true;
}

void ReportFile::Write(const char *buffer, uptr length) {
  __asan_baremetal_write(buffer, length);
}
void *SetAlternateSignalStack() { return nullptr; }
void UnsetAlternateSignalStack(void *) {}
bool SupportsColoredOutput(int) { return false; }

void ListOfModules::fallbackInit() {}

void SignalContext::InitPcSpBp() {}
void SignalContext::DumpAllRegisters(void *) {}
const char *SignalContext::Describe() const { return "hardware exception"; }
uptr SignalContext::GetAddress() const { return 0; }
bool SignalContext::IsMemoryAccess() const { return false; }
SignalContext::WriteFlag SignalContext::GetWriteFlag() const {
  return SignalContext::Unknown;
}
bool SignalContext::IsTrueFaultingAddress() const { return false; }
bool SignalContext::IsStackOverflow() const { return false; }

void BufferedStackTrace::UnwindSlow(uptr pc, u32 max_depth) {}
void BufferedStackTrace::UnwindSlow(uptr pc, void *ctx, u32 max_depth) {}

// Freestanding allocator and stack depot stubs
static LowLevelAllocator GlobalLowLevelAllocator;
LowLevelAllocator &GetGlobalLowLevelAllocator() {
  return GlobalLowLevelAllocator;
}
void *LowLevelAllocator::Allocate(uptr size) { return malloc(size); }
void SetLowLevelAllocateMinAlignment(uptr alignment) {}
void SetLowLevelAllocateCallback(LowLevelAllocateCallback callback) {}

void *InternalAlloc(uptr size, InternalAllocatorCache *cache, uptr alignment) {
  return malloc(size);
}
void InternalFree(void *p, InternalAllocatorCache *cache) { free(p); }
void *InternalRealloc(void *p, uptr size, InternalAllocatorCache *cache) {
  return nullptr;
}
void *InternalCalloc(uptr count, uptr size, InternalAllocatorCache *cache,
                     uptr alignment) {
  void *p = malloc(count * size);
  if (p)
    internal_memset(p, 0, count * size);
  return p;
}

void InternalAllocatorLock() { __asan_baremetal_mutex_lock(); }
void InternalAllocatorUnlock() { __asan_baremetal_mutex_unlock(); }

u32 StackDepotPut(StackTrace stack) { return 0; }
StackTrace StackDepotGet(u32 id) { return StackTrace(); }
void PrintHintAllocatorCannotReturnNull() {}

}  // namespace __sanitizer

#endif  // SANITIZER_BAREMETAL
