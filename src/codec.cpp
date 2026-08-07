#include "holonight/config/codec.h"

#include <toml++/toml.hpp>

#include <algorithm>
#include <initializer_list>
#include <iomanip>
#include <locale>
#include <sstream>
#include <string>
#include <unordered_set>

namespace HoloNight::Config {
namespace {

Diagnostic diagnostic(ErrorCode code, std::string message,
                      const std::filesystem::path &path = {},
                      std::optional<SourcePosition> position = std::nullopt) {
  Diagnostic result{.code = code,
                    .severity = Severity::Error,
                    .message = std::move(message),
                    .path = std::nullopt,
                    .position = position};
  if (!path.empty())
    result.path = path;
  return result;
}

void rejectUnknown(const toml::table &table,
                   std::initializer_list<std::string_view> accepted,
                   std::string_view prefix,
                   std::vector<Diagnostic> &diagnostics,
                   const std::filesystem::path &path) {
  for (const auto &[key, node] : table) {
    if (std::find(accepted.begin(), accepted.end(), key.str()) ==
        accepted.end()) {
      const auto source = node.source();
      diagnostics.push_back(diagnostic(
          ErrorCode::ValidationError,
          "unknown field " + std::string{prefix} + std::string{key.str()}, path,
          SourcePosition{source.begin.line, source.begin.column}));
    }
  }
}

const toml::table *requiredTable(const toml::table &root, std::string_view name,
                                 std::vector<Diagnostic> &diagnostics,
                                 const std::filesystem::path &path) {
  const toml::node *node = root.get(name);
  if (node == nullptr || !node->is_table()) {
    diagnostics.push_back(
        diagnostic(ErrorCode::ValidationError,
                   "missing or invalid table " + std::string{name}, path));
    return nullptr;
  }
  return node->as_table();
}

template <typename T>
std::optional<T> requiredValue(const toml::table &table, std::string_view key,
                               std::string_view qualified,
                               std::vector<Diagnostic> &diagnostics,
                               const std::filesystem::path &path) {
  const toml::node *node = table.get(key);
  if (node == nullptr) {
    diagnostics.push_back(diagnostic(ErrorCode::ValidationError,
                                     "missing field " + std::string{qualified},
                                     path));
    return std::nullopt;
  }
  auto value = node->value<T>();
  if (!value.has_value()) {
    const auto source = node->source();
    diagnostics.push_back(
        diagnostic(ErrorCode::ValidationError,
                   "invalid type for " + std::string{qualified}, path,
                   SourcePosition{source.begin.line, source.begin.column}));
  }
  return value;
}

std::optional<double> requiredNumber(const toml::table &table,
                                     std::string_view key,
                                     std::string_view qualified,
                                     std::vector<Diagnostic> &diagnostics,
                                     const std::filesystem::path &path) {
  const toml::node *node = table.get(key);
  if (node == nullptr) {
    diagnostics.push_back(diagnostic(ErrorCode::ValidationError,
                                     "missing field " + std::string{qualified},
                                     path));
    return std::nullopt;
  }
  if (const auto floating = node->value<double>(); floating.has_value())
    return floating;
  if (const auto integer = node->value<std::int64_t>(); integer.has_value())
    return static_cast<double>(*integer);
  const auto source = node->source();
  diagnostics.push_back(diagnostic(
      ErrorCode::ValidationError, "invalid type for " + std::string{qualified},
      path, SourcePosition{source.begin.line, source.begin.column}));
  return std::nullopt;
}

std::string quote(std::string_view value) {
  std::string result{"\""};
  for (const unsigned char character : value) {
    switch (character) {
    case '\\':
      result += "\\\\";
      break;
    case '"':
      result += "\\\"";
      break;
    case '\b':
      result += "\\b";
      break;
    case '\t':
      result += "\\t";
      break;
    case '\n':
      result += "\\n";
      break;
    case '\f':
      result += "\\f";
      break;
    case '\r':
      result += "\\r";
      break;
    default:
      result += static_cast<char>(character);
      break;
    }
  }
  result += '"';
  return result;
}

std::string number(double value) {
  std::ostringstream stream;
  stream.imbue(std::locale::classic());
  stream << std::setprecision(17) << value;
  std::string result = stream.str();
  if (result.find_first_of(".eE") == std::string::npos)
    result += ".0";
  return result;
}

void attachPath(std::vector<Diagnostic> &diagnostics,
                const std::filesystem::path &path) {
  if (path.empty())
    return;
  for (Diagnostic &item : diagnostics)
    item.path = path;
}

} // namespace

Result<Appearance> parse(std::string_view document,
                         const std::filesystem::path &source_path) {
  if (document.size() > kMaximumDocumentBytes) {
    return Result<Appearance>::failure(
        {diagnostic(ErrorCode::TooLarge,
                    "appearance document exceeds 65536 bytes", source_path)});
  }

  toml::table root;
  try {
    root = toml::parse(document, source_path.string());
  } catch (const toml::parse_error &error) {
    const auto source = error.source();
    return Result<Appearance>::failure({diagnostic(
        ErrorCode::SyntaxError, std::string{error.description()}, source_path,
        SourcePosition{source.begin.line, source.begin.column})});
  }

  std::vector<Diagnostic> diagnostics;
  rejectUnknown(root,
                {"version", "theme", "typography", "icons", "layout", "shape"},
                "", diagnostics, source_path);

  Appearance appearance;
  if (auto version = requiredValue<std::int64_t>(root, "version", "version",
                                                 diagnostics, source_path)) {
    appearance.version = *version;
  }

  if (const toml::table *theme =
          requiredTable(root, "theme", diagnostics, source_path)) {
    rejectUnknown(*theme, {"scheme", "accent"}, "theme.", diagnostics,
                  source_path);
    if (auto value = requiredValue<std::string>(
            *theme, "scheme", "theme.scheme", diagnostics, source_path))
      appearance.theme.scheme = std::move(*value);
    if (auto value = requiredValue<std::string>(
            *theme, "accent", "theme.accent", diagnostics, source_path))
      appearance.theme.accent = std::move(*value);
  }

  if (const toml::table *typography =
          requiredTable(root, "typography", diagnostics, source_path)) {
    rejectUnknown(*typography,
                  {"ui_family", "ui_size", "monospace_family", "monospace_size",
                   "title_family", "title_size", "display_family",
                   "display_size"},
                  "typography.", diagnostics, source_path);
    if (auto value = requiredValue<std::string>(*typography, "ui_family",
                                                "typography.ui_family",
                                                diagnostics, source_path))
      appearance.typography.ui_family = std::move(*value);
    if (auto value = requiredValue<std::int64_t>(*typography, "ui_size",
                                                 "typography.ui_size",
                                                 diagnostics, source_path))
      appearance.typography.ui_size = *value;
    if (auto value = requiredValue<std::string>(*typography, "monospace_family",
                                                "typography.monospace_family",
                                                diagnostics, source_path))
      appearance.typography.monospace_family = std::move(*value);
    if (auto value = requiredValue<std::int64_t>(*typography, "monospace_size",
                                                 "typography.monospace_size",
                                                 diagnostics, source_path))
      appearance.typography.monospace_size = *value;
    if (auto value = requiredValue<std::string>(*typography, "title_family",
                                                "typography.title_family",
                                                diagnostics, source_path))
      appearance.typography.title_family = std::move(*value);
    if (auto value = requiredValue<std::int64_t>(*typography, "title_size",
                                                 "typography.title_size",
                                                 diagnostics, source_path))
      appearance.typography.title_size = *value;
    if (auto value = requiredValue<std::string>(*typography, "display_family",
                                                "typography.display_family",
                                                diagnostics, source_path))
      appearance.typography.display_family = std::move(*value);
    if (auto value = requiredValue<std::int64_t>(*typography, "display_size",
                                                 "typography.display_size",
                                                 diagnostics, source_path))
      appearance.typography.display_size = *value;
  }

  if (const toml::table *icons =
          requiredTable(root, "icons", diagnostics, source_path)) {
    rejectUnknown(*icons, {"theme", "fallback", "cursor"}, "icons.",
                  diagnostics, source_path);
    if (auto value = requiredValue<std::string>(*icons, "theme", "icons.theme",
                                                diagnostics, source_path))
      appearance.icons.theme = std::move(*value);
    if (auto value = requiredValue<std::string>(
            *icons, "fallback", "icons.fallback", diagnostics, source_path))
      appearance.icons.fallback = std::move(*value);
    if (auto value = requiredValue<std::string>(
            *icons, "cursor", "icons.cursor", diagnostics, source_path))
      appearance.icons.cursor = std::move(*value);
  }

  if (const toml::table *layout =
          requiredTable(root, "layout", diagnostics, source_path)) {
    rejectUnknown(*layout, {"scale"}, "layout.", diagnostics, source_path);
    if (auto value = requiredNumber(*layout, "scale", "layout.scale",
                                    diagnostics, source_path))
      appearance.layout.scale = *value;
  }

  if (const toml::table *shape =
          requiredTable(root, "shape", diagnostics, source_path)) {
    rejectUnknown(*shape, {"style", "scale", "base_radius", "base_chamfer"},
                  "shape.", diagnostics, source_path);
    if (auto value = requiredValue<std::string>(*shape, "style", "shape.style",
                                                diagnostics, source_path)) {
      if (auto style = shapeStyleFromName(*value); style.has_value()) {
        appearance.shape.style = *style;
      } else {
        diagnostics.push_back(diagnostic(ErrorCode::ValidationError,
                                         "invalid value for shape.style",
                                         source_path));
      }
    }
    if (auto value = requiredNumber(*shape, "scale", "shape.scale", diagnostics,
                                    source_path))
      appearance.shape.scale = *value;
    if (shape->contains("base_radius")) {
      if (auto value =
              requiredNumber(*shape, "base_radius", "shape.base_radius",
                             diagnostics, source_path))
        appearance.shape.base_radius = *value;
    }
    if (shape->contains("base_chamfer")) {
      if (auto value =
              requiredNumber(*shape, "base_chamfer", "shape.base_chamfer",
                             diagnostics, source_path))
        appearance.shape.base_chamfer = *value;
    }
  }

  appearance = normalized(std::move(appearance));
  std::vector<Diagnostic> validation = validate(appearance);
  attachPath(validation, source_path);
  diagnostics.insert(diagnostics.end(),
                     std::make_move_iterator(validation.begin()),
                     std::make_move_iterator(validation.end()));
  if (!diagnostics.empty())
    return Result<Appearance>::failure(std::move(diagnostics));
  return Result<Appearance>::success(std::move(appearance));
}

Result<std::string> serialize(const Appearance &appearance) {
  const Appearance canonical = normalized(appearance);
  std::vector<Diagnostic> diagnostics = validate(canonical);
  if (!diagnostics.empty())
    return Result<std::string>::failure(std::move(diagnostics));

  std::ostringstream output;
  output.imbue(std::locale::classic());
  output << "version = 1\n\n"
         << "[theme]\n"
         << "scheme = " << quote(canonical.theme.scheme) << '\n'
         << "accent = " << quote(canonical.theme.accent) << "\n\n"
         << "[typography]\n"
         << "ui_family = " << quote(canonical.typography.ui_family) << '\n'
         << "ui_size = " << canonical.typography.ui_size << '\n'
         << "monospace_family = "
         << quote(canonical.typography.monospace_family) << '\n'
         << "monospace_size = " << canonical.typography.monospace_size << '\n'
         << "title_family = " << quote(canonical.typography.title_family)
         << '\n'
         << "title_size = " << canonical.typography.title_size << '\n'
         << "display_family = " << quote(canonical.typography.display_family)
         << '\n'
         << "display_size = " << canonical.typography.display_size << "\n\n"
         << "[icons]\n"
         << "theme = " << quote(canonical.icons.theme) << '\n'
         << "fallback = " << quote(canonical.icons.fallback) << '\n'
         << "cursor = " << quote(canonical.icons.cursor) << "\n\n"
         << "[layout]\n"
         << "scale = " << number(canonical.layout.scale) << "\n\n"
         << "[shape]\n"
         << "style = " << quote(shapeStyleName(canonical.shape.style)) << '\n'
         << "scale = " << number(canonical.shape.scale) << '\n';
  if (canonical.shape.base_radius.has_value())
    output << "base_radius = " << number(*canonical.shape.base_radius) << '\n';
  if (canonical.shape.base_chamfer.has_value())
    output << "base_chamfer = " << number(*canonical.shape.base_chamfer)
           << '\n';
  return Result<std::string>::success(output.str());
}

} // namespace HoloNight::Config
