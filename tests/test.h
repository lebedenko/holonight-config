#pragma once

#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace Test {

using Function = void (*)();

struct Case {
  std::string name;
  Function function;
};

inline std::vector<Case>& cases() {
  static std::vector<Case> registered;
  return registered;
}

struct Register {
  Register(std::string name, Function function) { cases().push_back({std::move(name), function}); }
};

inline void fail(const char* expression, const char* file, int line) {
  throw std::runtime_error{std::string{file} + ":" + std::to_string(line) + ": assertion failed: " + expression};
}

}  // namespace Test

#define TEST_CASE(name)                                \
  static void name();                                  \
  static Test::Register name##_register{#name, &name}; \
  static void name()

#define EXPECT_TRUE(expression)                                     \
  do {                                                              \
    if (!(expression)) Test::fail(#expression, __FILE__, __LINE__); \
  } while (false)

#define EXPECT_FALSE(expression) EXPECT_TRUE(!(expression))
#define EXPECT_EQ(lhs, rhs) EXPECT_TRUE((lhs) == (rhs))
