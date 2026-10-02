#include "test.h"

#include <string_view>

TEST_CASE(assertions_evaluate_each_operand_once) {
  int true_calls = 0;
  EXPECT_TRUE(++true_calls == 1);
  EXPECT_EQ(true_calls, 1);

  int left_calls = 0;
  int right_calls = 0;
  EXPECT_EQ(++left_calls, ++right_calls);
  EXPECT_EQ(left_calls, 1);
  EXPECT_EQ(right_calls, 1);
}

TEST_CASE(false_assertions_throw_with_expression_and_source) {
  try {
    EXPECT_FALSE(true);
  } catch (const std::runtime_error& error) {
    const std::string_view message{error.what()};
    EXPECT_TRUE(message.find("test_assertions.cpp:") != std::string_view::npos);
    EXPECT_TRUE(message.find("assertion failed: !(true)") != std::string_view::npos);
    return;
  }
  Test::fail("false assertion must throw", __FILE__, __LINE__);
}
