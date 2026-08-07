#include "test.h"

#include <holonight/config/appearance.h>

#include <cmath>
#include <limits>

using namespace HoloNight::Config;

TEST_CASE(defaults_match_v1_contract) {
  const Appearance value = defaults();
  EXPECT_EQ(value.version, 1);
  EXPECT_EQ(value.theme.scheme, "holonight-dark");
  EXPECT_EQ(value.theme.accent, "blue");
  EXPECT_EQ(value.typography.ui_family, "Inter");
  EXPECT_EQ(value.typography.ui_size, 12);
  EXPECT_EQ(value.typography.monospace_family, "JetBrains Mono");
  EXPECT_EQ(value.typography.monospace_size, 12);
  EXPECT_EQ(value.typography.title_family, "Audiowide");
  EXPECT_EQ(value.typography.title_size, 10);
  EXPECT_EQ(value.typography.display_family, "Rajdhani");
  EXPECT_EQ(value.typography.display_size, 24);
  EXPECT_EQ(value.icons.theme, "HoloNight");
  EXPECT_EQ(value.icons.fallback, "Papirus");
  EXPECT_EQ(value.icons.cursor, "default");
  EXPECT_EQ(value.layout.scale, 1.0);
  EXPECT_EQ(value.shape.style, ShapeStyle::Inherit);
  EXPECT_EQ(value.shape.scale, 1.0);
  EXPECT_FALSE(value.shape.base_radius.has_value());
  EXPECT_FALSE(value.shape.base_chamfer.has_value());
  EXPECT_TRUE(validate(value).empty());
}

TEST_CASE(validation_accepts_all_numeric_boundaries) {
  Appearance value = defaults();
  value.typography.ui_size = 6;
  value.typography.monospace_size = 48;
  value.layout.scale = 0.5;
  value.shape.scale = 4.0;
  value.shape.base_radius = 0.0;
  value.shape.base_chamfer = 128.0;
  EXPECT_TRUE(validate(value).empty());
}

TEST_CASE(validation_rejects_each_numeric_domain_and_non_finite_values) {
  Appearance value = defaults();
  value.typography.ui_size = 5;
  value.layout.scale = 3.01;
  value.shape.scale = std::nan("");
  value.shape.base_radius = std::numeric_limits<double>::infinity();
  EXPECT_EQ(validate(value).size(), 4U);
}

TEST_CASE(validation_rejects_empty_and_oversized_identifiers) {
  Appearance value = defaults();
  value.theme.scheme.clear();
  value.icons.cursor.assign(257, 'x');
  EXPECT_EQ(validate(value).size(), 2U);
}

TEST_CASE(validation_treats_whitespace_only_identifiers_as_empty) {
  Appearance value = defaults();
  value.theme.scheme = "  \t\n";
  EXPECT_EQ(validate(value).size(), 1U);
}

TEST_CASE(shape_style_names_round_trip) {
  for (ShapeStyle style : {ShapeStyle::Inherit, ShapeStyle::Hybrid,
                           ShapeStyle::Rounded, ShapeStyle::Chamfered}) {
    EXPECT_EQ(shapeStyleFromName(shapeStyleName(style)), style);
  }
  EXPECT_FALSE(shapeStyleFromName("invalid").has_value());
}
