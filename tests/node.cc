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
    Node* curr = x;
    ASSERT_NE(curr, nullptr);
    EXPECT_EQ(curr, x);
    curr = curr->next;
    ASSERT_NE(curr, nullptr);
    EXPECT_EQ(curr, z);
    curr = curr->next;
    ASSERT_NE(curr, nullptr);
    EXPECT_EQ(curr, y);
    curr = curr->next;
    EXPECT_EQ(curr, nullptr);
  }

  {
    Node* curr = y;
    ASSERT_NE(curr, nullptr);
    EXPECT_EQ(curr, y);
    curr = curr->next;
    EXPECT_EQ(curr, nullptr);
  }

  z->unlink();

  {
    Node* curr = x;
    ASSERT_NE(curr, nullptr);
    EXPECT_EQ(curr, x);
    curr = curr->next;
    ASSERT_NE(curr, nullptr);
    EXPECT_EQ(curr, y);
    curr = curr->next;
    EXPECT_EQ(curr, nullptr);
  }

  z->link_at(Node{y, x->addr_of_next()});

  {
    Node* curr = x;
    ASSERT_NE(curr, nullptr);
    EXPECT_EQ(curr, x);
    curr = curr->next;
    ASSERT_NE(curr, nullptr);
    EXPECT_EQ(curr, z);
    curr = curr->next;
    ASSERT_NE(curr, nullptr);
    EXPECT_EQ(curr, y);
    curr = curr->next;
    EXPECT_EQ(curr, nullptr);
  }

  z->unlink();
  y->unlink();

  {
    Node* curr = x;
    ASSERT_NE(curr, nullptr);
    EXPECT_EQ(curr, x);
    curr = curr->next;
    EXPECT_EQ(curr, nullptr);
  }

  delete x;
  delete y;
  delete z;
}

}  // namespace
}  // namespace flat_tlsf
