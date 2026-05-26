//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef FLAT_TLSF_FFI_H
#define FLAT_TLSF_FFI_H

#include <stddef.h>

extern "C" {
  void* flat_tlsf2_create(void* mem, size_t size);
  void flat_tlsf2_destroy(void* heap);
  void* flat_tlsf2_allocate(void* heap, size_t size);
  void* flat_tlsf2_aligned_allocate(void* heap, size_t alignment, size_t size);
  void flat_tlsf2_free(void* heap, void* ptr);
  void* flat_tlsf2_realloc(void* heap, void* ptr, size_t size);
  void* flat_tlsf2_calloc(void* heap, size_t num, size_t size);
  size_t flat_tlsf2_get_free_mem(void* heap);

  // FreeListHeap baseline bindings
  void* freelist2_create(void* mem, size_t size);
  void freelist2_destroy(void* heap);
  void* freelist2_allocate(void* heap, size_t size);
  void* freelist2_aligned_allocate(void* heap, size_t alignment, size_t size);
  void freelist2_free(void* heap, void* ptr);
  void* freelist2_realloc(void* heap, void* ptr, size_t size);
  void* freelist2_calloc(void* heap, size_t num, size_t size);
}

#endif // FLAT_TLSF_FFI_H
