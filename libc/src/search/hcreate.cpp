//===-- Implementation of hcreate -------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "src/search/hcreate.h"
#include "src/search/hashtable/global.h"
#include "src/search/hashtable/utils.h"
#include <errno.h>

namespace __llvm_libc {
LLVM_LIBC_FUNCTION(int, hcreate, (size_t nel)) {
  using namespace search::hashtable;
  global_raw_table.raw = RawTable::with_capacity(nel);
  global_raw_table.seed = DEFAULT_SEED ^ reinterpret_cast<uintptr_t>(&nel);
  if (!global_raw_table.raw.is_valid()) {
    errno = ENOMEM;
    return 0;
  }
  return 1;
}

} // namespace __llvm_libc
