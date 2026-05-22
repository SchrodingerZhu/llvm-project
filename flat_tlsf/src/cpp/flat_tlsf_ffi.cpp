//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "flat_tlsf_ffi.h"
#include "flat_tlsf_heap.h"
#include "vendor/freelist_heap.h"
#include <stdlib.h>

using LIBC_NAMESPACE::flat_tlsf::FlatTlsfHeap;
using LIBC_NAMESPACE::flat_tlsf::RawByte;
using LIBC_NAMESPACE::FreeListHeap;

extern "C" {

void* flat_tlsf_create(void* mem, size_t size) {
  if (mem == nullptr || size < LIBC_NAMESPACE::flat_tlsf::MIN_GAP_SIZE)
    return nullptr;
  
  // Allocate opaque shell memory
  void* shell = ::malloc(sizeof(FlatTlsfHeap));
  if (shell == nullptr)
    return nullptr;

  // Placement new the allocator instance inside the shell
  return new (shell) FlatTlsfHeap(
      LIBC_NAMESPACE::cpp::span<RawByte>(reinterpret_cast<RawByte*>(mem), size));
}

void flat_tlsf_destroy(void* heap) {
  if (heap == nullptr)
    return;
  FlatTlsfHeap* h = static_cast<FlatTlsfHeap*>(heap);
  h->~FlatTlsfHeap();
  ::free(h);
}

void* flat_tlsf_allocate(void* heap, size_t size) {
  if (heap == nullptr)
    return nullptr;
  return static_cast<FlatTlsfHeap*>(heap)->allocate(size);
}

void* flat_tlsf_aligned_allocate(void* heap, size_t alignment, size_t size) {
  if (heap == nullptr)
    return nullptr;
  return static_cast<FlatTlsfHeap*>(heap)->aligned_allocate(alignment, size);
}

void flat_tlsf_free(void* heap, void* ptr) {
  if (heap == nullptr || ptr == nullptr)
    return;
  static_cast<FlatTlsfHeap*>(heap)->free(ptr);
}

void* flat_tlsf_realloc(void* heap, void* ptr, size_t size) {
  if (heap == nullptr)
    return nullptr;
  return static_cast<FlatTlsfHeap*>(heap)->realloc(ptr, size);
}

void* flat_tlsf_calloc(void* heap, size_t num, size_t size) {
  if (heap == nullptr)
    return nullptr;
  return static_cast<FlatTlsfHeap*>(heap)->calloc(num, size);
}

size_t flat_tlsf_get_free_mem(void* heap) {
  if (heap == nullptr)
    return 0;
  return static_cast<FlatTlsfHeap*>(heap)->get_free_mem();
}

// FreeListHeap FFI implementations
void* freelist_create(void* mem, size_t size) {
  if (mem == nullptr || size < 32) // min gap size equivalent check
    return nullptr;
  void* shell = ::malloc(sizeof(FreeListHeap));
  if (shell == nullptr)
    return nullptr;
  return new (shell) FreeListHeap(
      LIBC_NAMESPACE::cpp::span<LIBC_NAMESPACE::cpp::byte>(
          reinterpret_cast<LIBC_NAMESPACE::cpp::byte*>(mem), size));
}

void freelist_destroy(void* heap) {
  if (heap == nullptr)
    return;
  FreeListHeap* h = static_cast<FreeListHeap*>(heap);
  h->~FreeListHeap();
  ::free(h);
}

void* freelist_allocate(void* heap, size_t size) {
  if (heap == nullptr)
    return nullptr;
  return static_cast<FreeListHeap*>(heap)->allocate(size);
}

void* freelist_aligned_allocate(void* heap, size_t alignment, size_t size) {
  if (heap == nullptr)
    return nullptr;
  return static_cast<FreeListHeap*>(heap)->aligned_allocate(alignment, size);
}

void freelist_free(void* heap, void* ptr) {
  if (heap == nullptr || ptr == nullptr)
    return;
  static_cast<FreeListHeap*>(heap)->free(ptr);
}

void* freelist_realloc(void* heap, void* ptr, size_t size) {
  if (heap == nullptr)
    return nullptr;
  return static_cast<FreeListHeap*>(heap)->realloc(ptr, size);
}

void* freelist_calloc(void* heap, size_t num, size_t size) {
  if (heap == nullptr)
    return nullptr;
  return static_cast<FreeListHeap*>(heap)->calloc(num, size);
}

} // extern "C"
