#include "src/__support/builtin_wrappers.h"
#include "src/__support/swisstable.h"
#include "src/stdlib/rand.h"
#include "src/stdlib/srand.h"
#include "src/sys/random/getrandom.h"
#include "test/src/__support/swisstable/test_utils.h"
#include "utils/UnitTest/LibcTest.h"

namespace __llvm_libc::internal::swisstable {

enum TestAction { Insert = 0, Find = 1, Delete = 2 };

using FixedInsertOnlyTable = RawTable<uint64_t, false, false>;
using ResizableInsertOnlyTable = RawTable<uint64_t, false, true>;
using FixedTable = RawTable<uint64_t, true, false>;
using ResizableTable = RawTable<uint64_t, true, true>;

static inline uint64_t sample() {
  size_t length = 64 - safe_clz<uint64_t>(RAND_MAX);
  uint64_t accumulator = 0;
  for (size_t i = 0; i < 64; i += length) {
    accumulator |= static_cast<uint64_t>(rand()) << i;
  }
  return accumulator;
}

static inline uint64_t bad_sample(uint64_t x) { return x << 16; }

static inline TestAction next_move(int max = 3) {
  return static_cast<TestAction>(((rand() % max) + max) % max);
}

static inline bool test_equal(const uint64_t &x, const uint64_t &y) {
  return x == y;
}
static inline size_t test_hash(const uint64_t &x) { return x; }

template <class Table, size_t OpRange>
struct LlvmLibcSwissTableTest : testing::Test {

  void init_seed() {
    unsigned int seed;
    __llvm_libc::getrandom(&seed, sizeof(seed), 0);
    __llvm_libc::srand(seed);
  }

  void bad_hash_example() {
    auto table = Table::with_capacity(16384);
    for (uint64_t i = 0; i < 16384; ++i) {
      auto t = bad_sample(i);
      table.insert(t, test_hash);
    }
    for (uint64_t i = 0; i < 16384; ++i) {
      ASSERT_EQ(*table.find(bad_sample(i), test_hash, test_equal),
                bad_sample(i));
    }
    for (uint64_t i = 16384; i < 32768; ++i) {
      ASSERT_FALSE(static_cast<bool>(table.find(i, test_hash, test_equal)));
      ASSERT_FALSE(
          static_cast<bool>(table.find(bad_sample(i), test_hash, test_equal)));
    }
    table.release();
  }

  void random_walk(size_t attempts, size_t capacity) {
    BrutalForceSet set;
    auto table = Table::with_capacity(capacity);
    for (size_t i = 0; i < attempts; ++i) {
      switch (next_move(OpRange)) {
      case Insert: {
        auto x = sample();
        set.insert(x);
        if (x & 1) {
          if (!table.find(x, test_hash, test_equal)) {
            ASSERT_EQ(*table.insert(x, test_hash), x);
          }
        } else {
          ASSERT_EQ(*table.find_or_insert(x, test_hash, test_equal), x);
        }
        ASSERT_EQ(*table.find(x, test_hash, test_equal), x);

        break;
      }
      case Find: {
        if (!set.size)
          continue;
        auto x = sample();
        ASSERT_EQ(static_cast<bool>(table.find(x, test_hash, test_equal)),
                  set.find(x));
        auto index = x % set.size;
        ASSERT_EQ(*table.find(set.data[index], test_hash, test_equal),
                  set.data[index]);
        break;
      }
      case Delete: {
        if (!set.size)
          continue;
        auto x = sample();
        auto index = x % set.size;
        auto target = set.data[index];
        set.erase(target);
        ASSERT_EQ(*table.find(target, test_hash, test_equal), target);
        table.erase(table.find(target, test_hash, test_equal));
        ASSERT_FALSE(
            static_cast<bool>(table.find(target, test_hash, test_equal)));
        break;
      }
      }
    }
    for (size_t i = 0; i < set.size; ++i) {
      ASSERT_EQ(*table.find(set.data[i], test_hash, test_equal), set.data[i]);
    }
    for (size_t i = 0; i < attempts; ++i) {
      auto x = sample();
      ASSERT_EQ(static_cast<bool>(table.find(x, test_hash, test_equal)),
                set.find(x));
    }
    table.release();
  }
};

using LlvmLibcSwissTableFixedInsertOnlyTableTest =
    LlvmLibcSwissTableTest<FixedInsertOnlyTable, 2>;
using LlvmLibcSwissTableResizableInsertOnlyTableTest =
    LlvmLibcSwissTableTest<ResizableInsertOnlyTable, 2>;
using LlvmLibcSwissTableFixedTableTest = LlvmLibcSwissTableTest<FixedTable, 3>;
using LlvmLibcSwissTableResizableTableTest =
    LlvmLibcSwissTableTest<ResizableTable, 3>;

TEST_F(LlvmLibcSwissTableFixedInsertOnlyTableTest, RandomWalk) {
  init_seed();
  for (size_t i = 0; i < 100; ++i) {
    this->random_walk(10000, 10000);
  }
}
TEST_F(LlvmLibcSwissTableResizableInsertOnlyTableTest, RandomWalk) {
  init_seed();
  for (size_t i = 0; i < 100; ++i) {
    this->random_walk(10000, 0);
  }
}
TEST_F(LlvmLibcSwissTableFixedTableTest, RandomWalk) {
  init_seed();
  for (size_t i = 0; i < 100; ++i) {
    this->random_walk(10000, 10000);
  }
}
TEST_F(LlvmLibcSwissTableResizableTableTest, RandomWalk) {
  init_seed();
  for (size_t i = 0; i < 100; ++i) {
    this->random_walk(10000, 0);
  }
}

TEST_F(LlvmLibcSwissTableFixedInsertOnlyTableTest, BadHash) {
  bad_hash_example();
}
TEST_F(LlvmLibcSwissTableResizableInsertOnlyTableTest, BadHash) {
  bad_hash_example();
}
TEST_F(LlvmLibcSwissTableFixedTableTest, BadHash) { bad_hash_example(); }
TEST_F(LlvmLibcSwissTableResizableTableTest, BadHash) { bad_hash_example(); }

TEST(LlvmLibcSwissTableFixedInsertOnlyTableTest, Pressure) {
  auto table = FixedInsertOnlyTable::with_capacity(512);
  uint64_t i = 0;
  for (; i < 512; ++i) {
    if (!table.insert(i, test_hash)) {
      break;
    }
  }
  ASSERT_LE(i, static_cast<uint64_t>(512));
  table.release();
}

TEST(LlvmLibcSwissTableFixedTableTest, InPlaceRehash) {
  auto table = FixedTable::with_capacity(512);
  uint64_t i = 0;
  for (; i < 512; ++i) {
    if (!table.insert(i, test_hash)) {
      break;
    }
  }

  uint64_t n = i;

  ASSERT_GE(n, static_cast<uint64_t>(128));

  for (i = 0; i < 128; ++i) {
    table.erase(table.find(i, test_hash, test_equal));
  }

  for (i = 1000; i < 1128; ++i) {
    table.find_or_insert(i, test_hash, test_equal);
  }

  for (i = 0; i < 128; ++i) {
    ASSERT_FALSE(static_cast<bool>(table.find(i, test_hash, test_equal)));
  }

  for (i = n; i < 512; ++i) {
    ASSERT_EQ(*table.find(i, test_hash, test_equal), i);
  }

  for (i = 1000; i < 1128; ++i) {
    ASSERT_EQ(*table.find(i, test_hash, test_equal), i);
  }

  table.release();
}
} // namespace __llvm_libc::internal::swisstable
