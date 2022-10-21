//===-- Implementation of hdestroy_r ----------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "src/search/hdestroy_r.h"
#include "src/search/hashtable/utils.h"
#include <search.h>
namespace __llvm_libc {
LLVM_LIBC_FUNCTION(void, hdestroy_r, (hsearch_data * hdata)) {
  using namespace search::hashtable;
  SeededTable *table = get_table(hdata);
  table->raw.release();
  table->seed = 0;
  free(table);
}

} // namespace __llvm_libc
