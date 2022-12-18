#include "src/__support/swisstable/safe_mem_size.h"
#include "utils/UnitTest/LibcTest.h"

namespace __llvm_libc::internal::swisstable {

static inline constexpr size_t SAFE_MEM_SIZE_TEST_LIMIT =
    static_cast<size_t>(__llvm_libc::cpp::numeric_limits<ssize_t>::max());

TEST(LlvmLibcSwissTableSafeMemSize, Constuction) {
  ASSERT_FALSE(SafeMemSize{static_cast<size_t>(-1)}.valid());
  ASSERT_FALSE(SafeMemSize{static_cast<size_t>(-2)}.valid());
  ASSERT_FALSE(SafeMemSize{static_cast<size_t>(-1024 + 33)}.valid());
  ASSERT_FALSE(SafeMemSize{static_cast<size_t>(-1024 + 66)}.valid());
  ASSERT_FALSE(SafeMemSize{SAFE_MEM_SIZE_TEST_LIMIT + 1}.valid());
  ASSERT_FALSE(SafeMemSize{SAFE_MEM_SIZE_TEST_LIMIT + 13}.valid());

  ASSERT_TRUE(SafeMemSize{static_cast<size_t>(1)}.valid());
  ASSERT_TRUE(SafeMemSize{static_cast<size_t>(1024 + 13)}.valid());
  ASSERT_TRUE(SafeMemSize{static_cast<size_t>(2048 - 13)}.valid());
  ASSERT_TRUE(SafeMemSize{static_cast<size_t>(4096 + 1)}.valid());
  ASSERT_TRUE(SafeMemSize{static_cast<size_t>(8192 - 1)}.valid());
  ASSERT_TRUE(SafeMemSize{static_cast<size_t>(16384 + 15)}.valid());
  ASSERT_TRUE(SafeMemSize{static_cast<size_t>(32768 * 3)}.valid());
  ASSERT_TRUE(SafeMemSize{static_cast<size_t>(65536 * 13)}.valid());
  ASSERT_TRUE(SafeMemSize{SAFE_MEM_SIZE_TEST_LIMIT}.valid());
  ASSERT_TRUE(SafeMemSize{SAFE_MEM_SIZE_TEST_LIMIT - 1}.valid());
  ASSERT_TRUE(SafeMemSize{SAFE_MEM_SIZE_TEST_LIMIT - 13}.valid());
}

TEST(LlvmLibcSwissTableSafeMemSize, Addition) {
  auto max = SafeMemSize{SAFE_MEM_SIZE_TEST_LIMIT};
  auto half = SafeMemSize{SAFE_MEM_SIZE_TEST_LIMIT / 2};
  auto third = SafeMemSize{SAFE_MEM_SIZE_TEST_LIMIT / 3};

  ASSERT_TRUE(half.valid());
  ASSERT_TRUE(third.valid());
  ASSERT_TRUE((half + half).valid());
  ASSERT_TRUE((third + third + third).valid());
  ASSERT_TRUE((half + third).valid());

  ASSERT_FALSE((max + SafeMemSize{static_cast<size_t>(1)}).valid());
  ASSERT_FALSE((third + third + third + third).valid());
  ASSERT_FALSE((half + half + half).valid());
}

TEST(LlvmLibcSwissTableSafeMemSize, Multiplication) {
  auto max = SafeMemSize{SAFE_MEM_SIZE_TEST_LIMIT};
  auto half = SafeMemSize{SAFE_MEM_SIZE_TEST_LIMIT / 2};
  auto third = SafeMemSize{SAFE_MEM_SIZE_TEST_LIMIT / 3};

  ASSERT_TRUE((max * SafeMemSize{static_cast<size_t>(1)}).valid());
  ASSERT_TRUE((max * SafeMemSize{static_cast<size_t>(0)}).valid());

  ASSERT_FALSE((max * SafeMemSize{static_cast<size_t>(2)}).valid());
  ASSERT_FALSE((half * half).valid());
  ASSERT_FALSE((half * SafeMemSize{static_cast<size_t>(3)}).valid());
  ASSERT_FALSE((third * SafeMemSize{static_cast<size_t>(4)}).valid());
}
} // namespace __llvm_libc::internal::swisstable
