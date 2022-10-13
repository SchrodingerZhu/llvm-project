//===-- Implementation of Global Hashtable --------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "src/search/hashtable/global.h"

namespace __llvm_libc::search::hashtable {
SeededTable global_raw_table;
}
