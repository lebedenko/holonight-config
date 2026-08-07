#include "test.h"

#include <holonight/config/codec.h>
#include <holonight/config/test_support.h>

#include <algorithm>
#include <cmath>

using namespace HoloNight::Config;

TEST_CASE(default_document_has_deterministic_golden_output) {
  const auto encoded = serialize(defaults());
  EXPECT_TRUE(encoded);
  EXPECT_EQ(*encoded.value, R"(version = 1

[theme]
scheme = "holonight-dark"
accent = "blue"

[typography]
ui_family = "Inter"
ui_size = 12
monospace_family = "JetBrains Mono"
monospace_size = 12
title_family = "Audiowide"
title_size = 10
display_family = "Rajdhani"
display_size = 24

[icons]
theme = "HoloNight"
fallback = "Papirus"
cursor = "default"

[layout]
scale = 1.0

[shape]
style = "inherit"
scale = 1.0
)");
}

TEST_CASE(complete_document_round_trips_unicode_and_optional_shapes) {
  Appearance value = defaults();
  value.typography.ui_family = "Тест Sans";
  value.shape.style = ShapeStyle::Chamfered;
  value.shape.base_radius = 8.5;
  value.shape.base_chamfer = 12.0;
  const auto parsed =
      parse(TestSupport::validDocument(value), "roundtrip.toml");
  EXPECT_TRUE(parsed);
  EXPECT_EQ(*parsed.value, value);
}

TEST_CASE(parser_trims_string_values) {
  std::string document = TestSupport::validDocument();
  const auto offset = document.find("scheme = \"holonight-dark\"");
  document.replace(offset, std::string{"scheme = \"holonight-dark\""}.size(),
                   "scheme = \"  holonight-dark  \"");
  const auto parsed = parse(document);
  EXPECT_TRUE(parsed);
  EXPECT_EQ(parsed.value->theme.scheme, "holonight-dark");
}

TEST_CASE(parser_rejects_unknown_and_missing_fields) {
  const auto unknown =
      parse(TestSupport::documentWithUnknownField(), "unknown.toml");
  EXPECT_FALSE(unknown);
  EXPECT_EQ(unknown.diagnostics.front().code, ErrorCode::ValidationError);
  EXPECT_TRUE(unknown.diagnostics.front().position.has_value());

  std::string missing = TestSupport::validDocument();
  const auto line = missing.find("accent = \"blue\"\n");
  missing.erase(line, std::string{"accent = \"blue\"\n"}.size());
  EXPECT_FALSE(parse(missing));
}

TEST_CASE(parser_rejects_wrong_types_duplicates_versions_and_malformed_toml) {
  std::string wrong = TestSupport::validDocument();
  const auto size = wrong.find("ui_size = 12");
  wrong.replace(size, std::string{"ui_size = 12"}.size(), "ui_size = \"12\"");
  EXPECT_FALSE(parse(wrong));

  const std::string duplicate = "version = 1\nversion = 1\n";
  EXPECT_EQ(parse(duplicate).diagnostics.front().code, ErrorCode::SyntaxError);

  std::string version = TestSupport::validDocument();
  version.replace(0, std::string{"version = 1"}.size(), "version = 2");
  const auto unsupported = parse(version);
  EXPECT_FALSE(unsupported);
  EXPECT_TRUE(std::any_of(unsupported.diagnostics.begin(),
                          unsupported.diagnostics.end(),
                          [](const Diagnostic &item) {
                            return item.code == ErrorCode::UnsupportedVersion;
                          }));

  EXPECT_EQ(parse("not = [valid").diagnostics.front().code,
            ErrorCode::SyntaxError);
}

TEST_CASE(parser_rejects_oversized_input_before_toml_parsing) {
  std::string oversized(kMaximumDocumentBytes + 1, 'x');
  const auto result = parse(oversized, "large.toml");
  EXPECT_FALSE(result);
  EXPECT_EQ(result.diagnostics.front().code, ErrorCode::TooLarge);
}

TEST_CASE(serializer_rejects_invalid_api_values_without_output) {
  Appearance value = defaults();
  value.layout.scale = std::nan("");
  EXPECT_FALSE(serialize(value));
}

TEST_CASE(serializer_normalizes_api_strings_and_rejects_invalid_utf8) {
  Appearance value = defaults();
  value.theme.scheme = "  holonight-dark  ";
  const auto encoded = serialize(value);
  EXPECT_TRUE(encoded);
  const auto normalized_result = parse(*encoded.value);
  EXPECT_TRUE(normalized_result);
  EXPECT_EQ(normalized_result.value->theme.scheme, "holonight-dark");

  value = defaults();
  value.icons.cursor = std::string{"\xC0\xAF", 2};
  EXPECT_FALSE(serialize(value));
}
