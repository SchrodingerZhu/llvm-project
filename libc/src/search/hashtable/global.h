//===-- Implementation Header of Global Hashtable -------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC_SEARCH_HASHTABLE_GLOBAL_H
#define LLVM_LIBC_SRC_SEARCH_HASHTABLE_GLOBAL_H

#include "src/search/hashtable/utils.h"

namespace __llvm_libc::search::hashtable {
extern SeededTable global_raw_table;
} // namespace __llvm_libc::search::hashtable

#endif // LLVM_LIBC_SRC_SEARCH_HASHTABLE_GLOBAL_H
