//===-- Implementation of hsearch -------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "src/search/hsearch.h"
#include "src/search/hashtable/global.h"
#include "src/search/hashtable/search_impl.h"

namespace __llvm_libc {
LLVM_LIBC_FUNCTION(ENTRY *, hsearch, (ENTRY item, ACTION action)) {
  using namespace search::hashtable;
  ENTRY *e;
  search_impl(item, action, &e, &global_raw_table);
  return e;
}

} // namespace __llvm_libc
