#include "flat_tlsf/flat_tlsf.h"
#include "gtest/gtest.h"

namespace flat_tlsf {
namespace {

TEST(TagTest, ReadsFlags) {
  EXPECT_FALSE(tag::is_above_free(0));
  EXPECT_FALSE(tag::is_allocated(0));
  EXPECT_FALSE(tag::is_heap_base(0));
  EXPECT_FALSE(tag::is_heap_end(0));

  EXPECT_TRUE(tag::is_above_free(tag::ABOVE_FREE_FLAG));
  EXPECT_TRUE(tag::is_allocated(tag::ALLOCATED_FLAG));
  EXPECT_TRUE(tag::is_heap_base(tag::HEAP_BASE_FLAG));
  EXPECT_TRUE(tag::is_heap_end(tag::HEAP_END_FLAG));

  Byte all_flags = tag::ABOVE_FREE_FLAG | tag::ALLOCATED_FLAG |
                   tag::HEAP_BASE_FLAG | tag::HEAP_END_FLAG;
  EXPECT_TRUE(tag::is_above_free(all_flags));
  EXPECT_TRUE(tag::is_allocated(all_flags));
  EXPECT_TRUE(tag::is_heap_base(all_flags));
  EXPECT_TRUE(tag::is_heap_end(all_flags));
}

TEST(TagTest, MutatesFlags) {
  Byte byte = tag::ALLOCATED_FLAG;

  tag::set_above_free(&byte);
  EXPECT_TRUE(tag::is_above_free(byte));
  EXPECT_TRUE(tag::is_allocated(byte));

  tag::clear_above_free(&byte);
  EXPECT_FALSE(tag::is_above_free(byte));
  EXPECT_TRUE(tag::is_allocated(byte));

  tag::set_end_flag(&byte);
  EXPECT_TRUE(tag::is_heap_end(byte));
  EXPECT_TRUE(tag::is_allocated(byte));

  tag::clear_end_flag(&byte);
  EXPECT_FALSE(tag::is_heap_end(byte));
  EXPECT_TRUE(tag::is_allocated(byte));
}

} // namespace
} // namespace flat_tlsf
