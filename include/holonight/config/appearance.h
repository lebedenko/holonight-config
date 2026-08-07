#pragma once

#include "holonight/config/diagnostic.h"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace HoloNight::Config {

inline constexpr std::int64_t kDocumentVersion = 1;
inline constexpr std::size_t kMaximumDocumentBytes = 64U * 1024U;

enum class ShapeStyle { Inherit, Hybrid, Rounded, Chamfered };

struct Theme {
  std::string scheme{"holonight-dark"};
  std::string accent{"blue"};
  bool operator==(const Theme &) const = default;
};

struct Typography {
  std::string ui_family{"Inter"};
  std::int64_t ui_size{12};
  std::string monospace_family{"JetBrains Mono"};
  std::int64_t monospace_size{12};
  std::string title_family{"Audiowide"};
  std::int64_t title_size{10};
  std::string display_family{"Rajdhani"};
  std::int64_t display_size{24};
  bool operator==(const Typography &) const = default;
};

struct Icons {
  std::string theme{"HoloNight"};
  std::string fallback{"Papirus"};
  std::string cursor{"default"};
  bool operator==(const Icons &) const = default;
};

struct Layout {
  double scale{1.0};
  bool operator==(const Layout &) const = default;
};

struct Shape {
  ShapeStyle style{ShapeStyle::Inherit};
  double scale{1.0};
  std::optional<double> base_radius;
  std::optional<double> base_chamfer;
  bool operator==(const Shape &) const = default;
};

struct Appearance {
  std::int64_t version{kDocumentVersion};
  Theme theme;
  Typography typography;
  Icons icons;
  Layout layout;
  Shape shape;
  bool operator==(const Appearance &) const = default;
};

[[nodiscard]] Appearance defaults();
[[nodiscard]] Appearance normalized(Appearance appearance);
[[nodiscard]] std::vector<Diagnostic> validate(const Appearance &appearance);
[[nodiscard]] std::string_view shapeStyleName(ShapeStyle style) noexcept;
[[nodiscard]] std::optional<ShapeStyle>
shapeStyleFromName(std::string_view name) noexcept;

} // namespace HoloNight::Config
