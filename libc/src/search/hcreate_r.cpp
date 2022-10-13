//===-- Implementation of hcreate_r -----------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "src/search/hcreate_r.h"
#include "src/search/hashtable/utils.h"
#include <asm-generic/errno-base.h>
#include <errno.h>
#include <stdlib.h>

namespace __llvm_libc {
LLVM_LIBC_FUNCTION(int, hcreate_r, (size_t nel, hsearch_data *hdata)) {
  using namespace search::hashtable;

  if (hdata == nullptr) {
    errno = EINVAL;
    return 0;
  }

  SeededTable *table = static_cast<SeededTable *>(
      aligned_alloc(alignof(SeededTable), sizeof(SeededTable)));

  if (table == nullptr) {
    errno = ENOMEM;
    return 0;
  }

  table->raw = RawTable::with_capacity(nel);

  if (!table->raw.is_valid()) {
    free(table);
    errno = ENOMEM;
    return 0;
  }

  set_table(hdata, table);
  table->seed = DEFAULT_SEED ^ reinterpret_cast<uintptr_t>(table);
  return 1;
}

} // namespace __llvm_libc
