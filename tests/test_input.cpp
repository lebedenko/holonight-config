#include "test.h"

#include <holonight/config/input.h>
using namespace HoloNight::Config;
TEST_CASE(input_navigation_is_shared_and_defaults_to_standard) {
  EXPECT_EQ(*decodeInputDocument(DocumentSnapshot{}).value, InputProfile::Standard);
  for (const auto& bytes : {"", "[bar.workspaces]\ncount = 5", "[input]\nnavigation = 'standard'"}) {
    const auto document = parseDocument(bytes);
    EXPECT_TRUE(document);
    EXPECT_EQ(*decodeInputDocument(*document.value).value, InputProfile::Standard);
  }
  const auto vim = parseDocument("[input]\nnavigation = 'vim-navigation'");
  EXPECT_EQ(*decodeInputDocument(*vim.value).value, InputProfile::VimNavigation);
  for (const auto& bytes : {"[input]\nnavigation = 'vim'", "[input]\nnavigation = true"}) {
    const auto document = parseDocument(bytes);
    EXPECT_TRUE(document);
    EXPECT_FALSE(decodeInputDocument(*document.value));
    EXPECT_FALSE(validateDocument(*document.value, inputDocumentSchema()).empty());
  }
  const auto path = resolveInputPath({{"HOME", "/home/test"}, {"HOLONIGHT_APPEARANCE_FILE", "/other/theme.toml"}});
  EXPECT_TRUE(path);
  EXPECT_EQ(path.value->string(), "/home/test/.config/holonight/config.toml");
}
TEST_CASE(input_patch_preserves_other_preferences_and_comments) {
  const auto document = parseDocument("# preferences\n[bar.workspaces]\ncount = 7 # keep\n[unknown]\nfoo = 'bar'\n");
  const auto patched = patchDocument(
      *document.value,
      {{.key = {"input", "navigation"}, .baseline = std::nullopt, .pending = Value{std::string{"vim-navigation"}}}},
      inputDocumentSchema());
  EXPECT_EQ(patched.status, SaveStatus::Success);
  EXPECT_TRUE(patched.snapshot->revision.bytes.find("count = 7 # keep") != std::string::npos);
  EXPECT_TRUE(patched.snapshot->revision.bytes.find("foo = 'bar'") != std::string::npos);
  EXPECT_EQ(*decodeInputDocument(*patched.snapshot).value, InputProfile::VimNavigation);
}
