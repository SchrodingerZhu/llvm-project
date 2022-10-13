//===-- Implementation of hdestroy ------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "src/search/hdestroy.h"
#include "src/search/hashtable/global.h"
namespace __llvm_libc {
LLVM_LIBC_FUNCTION(void, hdestroy, ()) {
  using namespace search::hashtable;
  global_raw_table.raw.release();
  global_raw_table.seed = 0;
}

} // namespace __llvm_libc
