//===-- Implementation of hdestroy ------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "src/search/hdestroy.h"
#include "src/__support/HashTable/table.h"
#include "src/search/hsearch/global.h"

namespace LIBC_NAMESPACE {
LLVM_LIBC_FUNCTION(void, hdestroy, (void)) {
  using namespace internal;
  HashTable::deallocate(global_hash_table);
  global_hash_table = nullptr;
}

} // namespace LIBC_NAMESPACE
