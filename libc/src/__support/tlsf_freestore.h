//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// This file contains a two-level segregated fit free block store.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_TLSF_FREESTORE_H
#define LLVM_LIBC_SRC___SUPPORT_TLSF_FREESTORE_H

#include "hdr/stdint_proxy.h"
#include "hdr/types/size_t.h"
#include "src/__support/CPP/array.h"
#include "src/__support/CPP/bit.h"
#include "src/__support/CPP/limits.h"
#include "src/__support/block.h"
#include "src/__support/freelist.h"
#include "src/__support/macros/config.h"
#include "src/__support/macros/optimization.h"
#include "configurable_freestore.h"

namespace LIBC_NAMESPACE_DECL {

template <size_t UNIT_SIZE, size_t STEP_SIZE_BITS, size_t NUM_STEP_BITS,
          size_t NUM_TABLE_ENTRIES>
using TLSFFreeStore = ConfigurableFreeStoreImpl<
    FreeStoreConfig<
        UNIT_SIZE,
        STEP_SIZE_BITS,
        NUM_STEP_BITS,
        NUM_TABLE_ENTRIES,
        IndexType::LinearList,
        SearchPreference::OverSized,
        IndexType::LinearList,
        SearchPreference::OverSized
    >
>;

} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_TLSF_FREESTORE_H
