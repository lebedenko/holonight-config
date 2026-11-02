#include "test.h"

#include <holonight/config/input.h>
using namespace HoloNight::Config;
TEST_CASE(input_profiles_are_versioned_and_independent) {
  EXPECT_EQ(*decodeInputDocument(DocumentSnapshot{}).value, InputProfile::Standard);
  for (const auto& name : {"standard", "vim-navigation"}) {
    const auto document = parseDocument(std::string{"version = 1\nprofile = '"} + name + "'\n");
    EXPECT_TRUE(document);
    EXPECT_TRUE(decodeInputDocument(*document.value));
  }
  for (const auto& bytes : {"", "version = 2\nprofile = 'standard'", "version = 1\nprofile = 'vim'",
                            "version = true\nprofile = 'standard'"}) {
    const auto document = parseDocument(bytes);
    EXPECT_TRUE(document);
    EXPECT_FALSE(decodeInputDocument(*document.value));
  }
  const auto path = resolveInputPath({{"HOME", "/home/test"}, {"HOLONIGHT_APPEARANCE_FILE", "/other/theme.toml"}});
  EXPECT_TRUE(path);
  EXPECT_EQ(path.value->string(), "/home/test/.config/holonight/input.toml");
}
