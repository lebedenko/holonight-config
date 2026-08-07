#include "test.h"

#include <fstream>
#include <holonight/config/store.h>
#include <holonight/config/test_support.h>

using namespace HoloNight::Config;

namespace {

std::string readFile(const std::filesystem::path& path) {
  std::ifstream input{path, std::ios::binary};
  return {std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{}};
}

std::size_t temporaryFileCount(const std::filesystem::path& directory) {
  std::size_t count = 0;
  for (const auto& entry : std::filesystem::directory_iterator{directory}) {
    if (entry.path().filename().string().starts_with(".appearance.toml.tmp-")) ++count;
  }
  return count;
}

}  // namespace

TEST_CASE(missing_load_returns_defaults_without_creating_file) {
  TestSupport::TemporaryDirectory directory;
  const auto path = directory.child("holonight/appearance.toml");
  const auto result = load(path);
  EXPECT_TRUE(result);
  EXPECT_EQ(result.value->origin, LoadOrigin::Default);
  EXPECT_EQ(result.value->appearance, defaults());
  EXPECT_EQ(result.diagnostics.front().code, ErrorCode::Missing);
  EXPECT_FALSE(std::filesystem::exists(path));
}

TEST_CASE(atomic_write_creates_parent_and_loads_complete_value) {
  TestSupport::TemporaryDirectory directory;
  const auto path = directory.child("nested/appearance.toml");
  Appearance value = defaults();
  value.theme.accent = "violet";
  value.shape.base_radius = 9.0;
  EXPECT_TRUE(writeAtomically(value, path));
  const auto loaded = load(path);
  EXPECT_TRUE(loaded);
  EXPECT_EQ(loaded.value->origin, LoadOrigin::File);
  EXPECT_EQ(loaded.value->appearance, value);
  EXPECT_EQ(temporaryFileCount(path.parent_path()), 0U);
}

TEST_CASE(atomic_write_replaces_existing_document) {
  TestSupport::TemporaryDirectory directory;
  const auto path = directory.child("appearance.toml");
  EXPECT_TRUE(writeAtomically(defaults(), path));
  Appearance replacement = defaults();
  replacement.theme.scheme = "holonight-light";
  EXPECT_TRUE(writeAtomically(replacement, path));
  EXPECT_EQ(load(path).value->appearance, replacement);
}

TEST_CASE(validation_failure_preserves_existing_destination_and_leaves_no_temporary_file) {
  TestSupport::TemporaryDirectory directory;
  const auto path = directory.child("appearance.toml");
  EXPECT_TRUE(writeAtomically(defaults(), path));
  const std::string before = readFile(path);
  Appearance invalid = defaults();
  invalid.shape.scale = 100.0;
  EXPECT_FALSE(writeAtomically(invalid, path));
  EXPECT_EQ(readFile(path), before);
  EXPECT_EQ(temporaryFileCount(directory.path()), 0U);
}

TEST_CASE(rename_failure_cleans_temporary_file) {
  TestSupport::TemporaryDirectory directory;
  const auto destination = directory.child("appearance.toml");
  std::filesystem::create_directory(destination);
  EXPECT_FALSE(writeAtomically(defaults(), destination));
  EXPECT_EQ(temporaryFileCount(directory.path()), 0U);
  EXPECT_TRUE(std::filesystem::is_directory(destination));
}

TEST_CASE(load_rejects_present_invalid_and_oversized_documents) {
  TestSupport::TemporaryDirectory directory;
  const auto invalid_path = directory.child("invalid.toml");
  std::ofstream{invalid_path} << "invalid = [";
  EXPECT_EQ(load(invalid_path).diagnostics.front().code, ErrorCode::SyntaxError);

  const auto large_path = directory.child("large.toml");
  std::ofstream output{large_path, std::ios::binary};
  output << std::string(kMaximumDocumentBytes + 1, 'x');
  output.close();
  EXPECT_EQ(load(large_path).diagnostics.front().code, ErrorCode::TooLarge);
}

TEST_CASE(environment_load_uses_injected_path_without_mutating_process_environment) {
  TestSupport::TemporaryDirectory directory;
  const Environment environment = TestSupport::environmentFor(directory.path());
  const auto expected = directory.child("holonight/appearance.toml");
  EXPECT_TRUE(writeAtomically(defaults(), expected));
  EXPECT_EQ(load(environment).value->origin, LoadOrigin::File);
}
