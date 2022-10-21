//===-- Implementation of hsearch_r -----------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "src/search/hsearch_r.h"
#include "src/search/hashtable/search_impl.h"

namespace __llvm_libc {
LLVM_LIBC_FUNCTION(int, hsearch_r,
                   (ENTRY item, ACTION action, ENTRY **retval,
                    hsearch_data *hdata)) {
  using namespace search::hashtable;
  return search_impl(item, action, retval, get_table(hdata));
}

} // namespace __llvm_libc
