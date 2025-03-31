//===-- Ecnoded pointer for global function pointers-------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_ENCODED_FUNC_PTR_H
#define LLVM_LIBC_SRC___SUPPORT_ENCODED_FUNC_PTR_H
#include "src/__support/common.h"

namespace LIBC_NAMESPACE_DECL {
namespace internal {
using PtrData = __UINTPTR_TYPE__;
static PtrData GLOBAL_PTR_MASK;
LIBC_INLINE PtrData encode(PtrData ptr) {
  return ptr ^ GLOBAL_PTR_MASK;
}
LIBC_INLINE PtrData decode(PtrData ptr) {
  return ptr ^ GLOBAL_PTR_MASK;
}
LIBC_INLINE PtrData get_global_ptr_mask() {
  return GLOBAL_PTR_MASK;
}
}

template <typename Func>
class EncodedFuncPtr {
  internal::PtrData encoded;

public:
  EncodedFuncPtr(Func func) : encoded {}
};

}

#endif // LLVM_LIBC_SRC___SUPPORT_ENCODED_FUNC_PTR_H