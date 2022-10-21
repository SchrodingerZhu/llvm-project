//===-- Utilities of hashtable --------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC_SEARCH_HASHTABLE_UTILS_H
#define LLVM_LIBC_SRC_SEARCH_HASHTABLE_UTILS_H

#ifndef LLVM_LIBC_SEARCH_ENABLE_HASHTABLE_RESIZE
#define LLVM_LIBC_SEARCH_ENABLE_HASHTABLE_RESIZE 0
#endif

#ifndef LLVM_LIBC_SEARCH_ENABLE_HASHTABLE_DELETION
#define LLVM_LIBC_SEARCH_ENABLE_HASHTABLE_DELETION 0
#endif

#include "src/__support/swisstable.h"
#include "src/__support/wyhash.h"
#include "src/string/strcmp.h"
#include "src/string/strlen.h"
#include <search.h>

namespace __llvm_libc::search::hashtable {

static inline bool equal(const ENTRY &a, const ENTRY &b) {
  return strcmp(a.key, b.key) == 0;
}

using RawTable =
    internal::swisstable::RawTable<ENTRY, LLVM_LIBC_SEARCH_ENABLE_HASHTABLE_RESIZE,
                              LLVM_LIBC_SEARCH_ENABLE_HASHTABLE_DELETION>;

struct SeededTable {
  RawTable raw;
  uint64_t seed;
};

using Bucket = internal::swisstable::Bucket<ENTRY>;

struct TableHeader {
  SeededTable *table;
};

static inline SeededTable *get_table(hsearch_data *hdata) {
  return reinterpret_cast<TableHeader *>(hdata)->table;
}

static inline void set_table(hsearch_data *hdata, SeededTable *table) {
  reinterpret_cast<TableHeader *>(hdata)->table = table;
}

struct Hash {
  uint64_t seed;
  uint64_t operator()(const ENTRY &entry) const {
    size_t length = __llvm_libc::strlen(entry.key);
    return internal::wyhash::DefaultHash::hash(entry.key, length, seed);
  }
};

// For now, we use only for seed to initialize the table.
// The number comes from Kunth's PRNG.
static constexpr inline uint64_t DEFAULT_SEED = 6364136223846793005;

} // namespace __llvm_libc::search::hashtable
#endif // LLVM_LIBC_SRC_SEARCH_HASHTABLE_UTILS_H
