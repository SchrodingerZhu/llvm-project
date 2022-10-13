//===-- Implementation Header of Hashtable Search  -------------------  ---===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
#ifndef LLVM_LIBC_SRC_SEARCH_HASHTABLE_SEARCH_IMPL_H
#define LLVM_LIBC_SRC_SEARCH_HASHTABLE_SEARCH_IMPL_H

#include "src/search/hashtable/utils.h"
#include <errno.h>
namespace __llvm_libc::search::hashtable {

static inline int search_impl(ENTRY item, ACTION action, ENTRY **retval,
                              SeededTable *table) {
  using namespace search;
  Hash hasher{table->seed};
  switch (action) {
  case ENTER: {
    auto bucket = table->raw.find_or_insert(item, hasher, equal);
    if (!bucket) {
      *retval = nullptr;
      errno = ENOMEM;
      return 0;
    }
    *retval = bucket.ptr - 1;
    return 1;
  }
  case FIND: {
    auto bucket = table->raw.find(item, hasher, equal);
    if (!bucket) {
      *retval = nullptr;
      errno = ESRCH;
      return 0;
    }
    *retval = bucket.ptr - 1;
    return 1;
  }
  }

  return 0;
}
} // namespace __llvm_libc::search::hashtable
#endif // LLVM_LIBC_SRC_SEARCH_HASHTABLE_SEARCH_IMPL_H
