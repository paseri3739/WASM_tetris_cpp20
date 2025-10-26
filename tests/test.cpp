#include <gtest/gtest.h>
import add;

TEST(AddTest, BasicAssersions) { EXPECT_EQ(add(2, 5), 7); }