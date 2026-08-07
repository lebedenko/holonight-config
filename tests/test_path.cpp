#include "test.h"

#include <holonight/config/path.h>

using namespace HoloNight::Config;

TEST_CASE(explicit_path_override_has_highest_precedence) {
  Environment environment{{"HOLONIGHT_APPEARANCE_FILE", "relative.toml"},
                          {"XDG_CONFIG_HOME", "/xdg"},
                          {"HOME", "/home"}};
  const auto result = resolveAppearancePath(environment);
  EXPECT_TRUE(result);
  EXPECT_EQ(*result.value, std::filesystem::path{"relative.toml"});
}

TEST_CASE(xdg_path_precedes_home_fallback) {
  const auto result = resolveAppearancePath(
      Environment{{"XDG_CONFIG_HOME", "/xdg"}, {"HOME", "/home"}});
  EXPECT_TRUE(result);
  EXPECT_EQ(*result.value,
            std::filesystem::path{"/xdg/holonight/appearance.toml"});
}

TEST_CASE(home_fallback_and_empty_values_are_deterministic) {
  const auto result =
      resolveAppearancePath(Environment{{"HOLONIGHT_APPEARANCE_FILE", ""},
                                        {"XDG_CONFIG_HOME", ""},
                                        {"HOME", "/home/test"}});
  EXPECT_TRUE(result);
  EXPECT_EQ(*result.value, std::filesystem::path{
                               "/home/test/.config/holonight/appearance.toml"});
}

TEST_CASE(unavailable_path_returns_structured_error) {
  const auto result = resolveAppearancePath(Environment{});
  EXPECT_FALSE(result);
  EXPECT_EQ(result.diagnostics.size(), 1U);
  EXPECT_EQ(result.diagnostics.front().code, ErrorCode::PathUnavailable);
}
