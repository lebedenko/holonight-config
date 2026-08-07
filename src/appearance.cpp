#include "holonight/config/appearance.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <string_view>

namespace HoloNight::Config {
namespace {

constexpr std::int64_t kMinimumFontSize = 6;
constexpr std::int64_t kMaximumFontSize = 48;
constexpr double kMinimumLayoutScale = 0.5;
constexpr double kMaximumLayoutScale = 3.0;
constexpr double kMinimumShapeScale = 0.25;
constexpr double kMaximumShapeScale = 4.0;
constexpr double kMaximumShapeExtent = 128.0;

void addError(std::vector<Diagnostic> &diagnostics, std::string message) {
  diagnostics.push_back(Diagnostic{.code = ErrorCode::ValidationError,
                                   .severity = Severity::Error,
                                   .message = std::move(message),
                                   .path = std::nullopt,
                                   .position = std::nullopt});
}

std::string trim(std::string value) {
  const auto is_space = [](unsigned char character) {
    return std::isspace(character) != 0;
  };
  const auto first = std::find_if_not(value.begin(), value.end(), is_space);
  const auto last =
      std::find_if_not(value.rbegin(), value.rend(), is_space).base();
  return first < last ? std::string{first, last} : std::string{};
}

bool isValidUtf8(std::string_view value) {
  std::size_t index = 0;
  while (index < value.size()) {
    const auto first = static_cast<unsigned char>(value[index]);
    if (first <= 0x7FU) {
      ++index;
      continue;
    }
    std::size_t length = 0;
    std::uint32_t code_point = 0;
    if ((first & 0xE0U) == 0xC0U) {
      length = 2;
      code_point = first & 0x1FU;
    } else if ((first & 0xF0U) == 0xE0U) {
      length = 3;
      code_point = first & 0x0FU;
    } else if ((first & 0xF8U) == 0xF0U) {
      length = 4;
      code_point = first & 0x07U;
    } else {
      return false;
    }
    if (index + length > value.size())
      return false;
    for (std::size_t offset = 1; offset < length; ++offset) {
      const auto continuation =
          static_cast<unsigned char>(value[index + offset]);
      if ((continuation & 0xC0U) != 0x80U)
        return false;
      code_point = (code_point << 6U) | (continuation & 0x3FU);
    }
    if ((length == 2 && code_point < 0x80U) ||
        (length == 3 && code_point < 0x800U) ||
        (length == 4 && code_point < 0x10000U) || code_point > 0x10FFFFU ||
        (code_point >= 0xD800U && code_point <= 0xDFFFU)) {
      return false;
    }
    index += length;
  }
  return true;
}

void validateIdentifier(std::vector<Diagnostic> &diagnostics,
                        std::string_view field, const std::string &value,
                        std::size_t maximum_bytes) {
  if (value.empty()) {
    addError(diagnostics, std::string{field} + " must not be empty");
  } else if (!isValidUtf8(value)) {
    addError(diagnostics, std::string{field} + " must contain valid UTF-8");
  } else if (value.size() > maximum_bytes) {
    addError(diagnostics, std::string{field} + " exceeds " +
                              std::to_string(maximum_bytes) + " bytes");
  }
}

void validateFontSize(std::vector<Diagnostic> &diagnostics,
                      std::string_view field, std::int64_t value) {
  if (value < kMinimumFontSize || value > kMaximumFontSize) {
    addError(diagnostics, std::string{field} + " must be in [6, 48]");
  }
}

void validateFiniteRange(std::vector<Diagnostic> &diagnostics,
                         std::string_view field, double value, double minimum,
                         double maximum) {
  if (!std::isfinite(value) || value < minimum || value > maximum) {
    addError(diagnostics,
             std::string{field} + " is outside its accepted range");
  }
}

} // namespace

Appearance defaults() { return {}; }

Appearance normalized(Appearance appearance) {
  appearance.theme.scheme = trim(std::move(appearance.theme.scheme));
  appearance.theme.accent = trim(std::move(appearance.theme.accent));
  appearance.typography.ui_family =
      trim(std::move(appearance.typography.ui_family));
  appearance.typography.monospace_family =
      trim(std::move(appearance.typography.monospace_family));
  appearance.typography.title_family =
      trim(std::move(appearance.typography.title_family));
  appearance.typography.display_family =
      trim(std::move(appearance.typography.display_family));
  appearance.icons.theme = trim(std::move(appearance.icons.theme));
  appearance.icons.fallback = trim(std::move(appearance.icons.fallback));
  appearance.icons.cursor = trim(std::move(appearance.icons.cursor));
  return appearance;
}

std::vector<Diagnostic> validate(const Appearance &appearance) {
  const Appearance canonical = normalized(appearance);
  std::vector<Diagnostic> diagnostics;
  if (canonical.version != kDocumentVersion) {
    diagnostics.push_back(Diagnostic{.code = ErrorCode::UnsupportedVersion,
                                     .severity = Severity::Error,
                                     .message = "version must be exactly 1",
                                     .path = std::nullopt,
                                     .position = std::nullopt});
  }

  validateIdentifier(diagnostics, "theme.scheme", canonical.theme.scheme, 128);
  validateIdentifier(diagnostics, "theme.accent", canonical.theme.accent, 128);
  validateIdentifier(diagnostics, "typography.ui_family",
                     canonical.typography.ui_family, 256);
  validateIdentifier(diagnostics, "typography.monospace_family",
                     canonical.typography.monospace_family, 256);
  validateIdentifier(diagnostics, "typography.title_family",
                     canonical.typography.title_family, 256);
  validateIdentifier(diagnostics, "typography.display_family",
                     canonical.typography.display_family, 256);
  validateFontSize(diagnostics, "typography.ui_size",
                   canonical.typography.ui_size);
  validateFontSize(diagnostics, "typography.monospace_size",
                   canonical.typography.monospace_size);
  validateFontSize(diagnostics, "typography.title_size",
                   canonical.typography.title_size);
  validateFontSize(diagnostics, "typography.display_size",
                   canonical.typography.display_size);
  validateIdentifier(diagnostics, "icons.theme", canonical.icons.theme, 256);
  validateIdentifier(diagnostics, "icons.fallback", canonical.icons.fallback,
                     256);
  validateIdentifier(diagnostics, "icons.cursor", canonical.icons.cursor, 256);
  validateFiniteRange(diagnostics, "layout.scale", canonical.layout.scale,
                      kMinimumLayoutScale, kMaximumLayoutScale);
  validateFiniteRange(diagnostics, "shape.scale", canonical.shape.scale,
                      kMinimumShapeScale, kMaximumShapeScale);
  if (canonical.shape.base_radius.has_value()) {
    validateFiniteRange(diagnostics, "shape.base_radius",
                        *canonical.shape.base_radius, 0.0, kMaximumShapeExtent);
  }
  if (canonical.shape.base_chamfer.has_value()) {
    validateFiniteRange(diagnostics, "shape.base_chamfer",
                        *canonical.shape.base_chamfer, 0.0,
                        kMaximumShapeExtent);
  }
  return diagnostics;
}

std::string_view shapeStyleName(ShapeStyle style) noexcept {
  switch (style) {
  case ShapeStyle::Inherit:
    return "inherit";
  case ShapeStyle::Hybrid:
    return "hybrid";
  case ShapeStyle::Rounded:
    return "rounded";
  case ShapeStyle::Chamfered:
    return "chamfered";
  }
  return "inherit";
}

std::optional<ShapeStyle> shapeStyleFromName(std::string_view name) noexcept {
  if (name == "inherit")
    return ShapeStyle::Inherit;
  if (name == "hybrid")
    return ShapeStyle::Hybrid;
  if (name == "rounded")
    return ShapeStyle::Rounded;
  if (name == "chamfered")
    return ShapeStyle::Chamfered;
  return std::nullopt;
}

} // namespace HoloNight::Config
