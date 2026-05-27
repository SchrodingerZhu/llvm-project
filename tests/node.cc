#include "flat_tlsf/flat_tlsf.h"
#include "gtest/gtest.h"

namespace flat_tlsf {
namespace {

TEST(NodeTest, BasicOperations) {
  auto* x = new Node{nullptr, nullptr};
  auto* y = new Node{nullptr, nullptr};
  auto* z = new Node{nullptr, nullptr};

  y->link_at(Node{nullptr, x->addr_of_next()});
  z->link_at(Node{y, x->addr_of_next()});

  {
    auto iter = x->begin();
    ASSERT_NE(iter, x->end());
    EXPECT_EQ(*iter, x);
    ASSERT_NE(++iter, x->end());
    EXPECT_EQ(*iter, z);
    ASSERT_NE(++iter, x->end());
    EXPECT_EQ(*iter, y);
    EXPECT_EQ(++iter, x->end());
  }

  {
    auto iter = y->begin();
    ASSERT_NE(iter, y->end());
    EXPECT_EQ(*iter, y);
    EXPECT_EQ(++iter, y->end());
  }

  z->unlink();

  {
    auto iter = x->begin();
    ASSERT_NE(iter, x->end());
    EXPECT_EQ(*iter, x);
    ASSERT_NE(++iter, x->end());
    EXPECT_EQ(*iter, y);
    EXPECT_EQ(++iter, x->end());
  }

  z->link_at(Node{y, x->addr_of_next()});

  {
    auto iter = x->begin();
    ASSERT_NE(iter, x->end());
    EXPECT_EQ(*iter, x);
    ASSERT_NE(++iter, x->end());
    EXPECT_EQ(*iter, z);
    ASSERT_NE(++iter, x->end());
    EXPECT_EQ(*iter, y);
    EXPECT_EQ(++iter, x->end());
  }

  z->unlink();
  y->unlink();

  {
    auto iter = x->begin();
    ASSERT_NE(iter, x->end());
    EXPECT_EQ(*iter, x);
    EXPECT_EQ(++iter, x->end());
  }

  delete x;
  delete y;
  delete z;
}

}  // namespace
}  // namespace flat_tlsf
