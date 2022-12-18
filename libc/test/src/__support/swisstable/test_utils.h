#ifndef LLVM_LIBC_TEST_SUPPORT_SWISSTABLE_TEST_UTILS_H
#define LLVM_LIBC_TEST_SUPPORT_SWISSTABLE_TEST_UTILS_H
#include <src/string/memmove.h>
#include <stdint.h>
#include <stdlib.h>

namespace __llvm_libc::internal::swisstable {

struct BrutalForceSet {
  size_t capacity;
  size_t size;
  uint64_t *data;

  BrutalForceSet() : capacity(16), size(0) {
    data = static_cast<uint64_t *>(calloc(capacity, sizeof(uint64_t)));
  }

  uint64_t *binary_search(uint64_t target) {
    size_t l = 0, r = size;
    while (l < r) {
      auto mid = (l + r) / 2;
      if (data[mid] == target) {
        return &data[mid];
      }
      if (data[mid] < target) {
        l = mid + 1;
      }
      if (data[mid] > target) {
        r = mid;
      }
    }
    return &data[l];
  }

  void grow() {
    data =
        static_cast<uint64_t *>(realloc(data, sizeof(uint64_t) * capacity * 2));
    capacity *= 2;
  }

  void insert(uint64_t target) {
    if (size == capacity)
      grow();
    uint64_t *position = binary_search(target);
    if (*position != target) {
      __llvm_libc::memmove(position + 1, position,
                           sizeof(uint64_t) * (data + size - position));
      size++;
      *position = target;
    }
  }

  bool find(uint64_t target) { return target == *binary_search(target); }

  void erase(uint64_t target) {
    uint64_t *position = binary_search(target);
    if (*position == target) {
      __llvm_libc::memmove(position, position + 1,
                           sizeof(uint64_t) * (data + size - position - 1));
      size--;
    }
  }
};
} // namespace __llvm_libc::internal::swisstable
#endif // LLVM_LIBC_TEST_SUPPORT_SWISSTABLE_TEST_UTILS_H
