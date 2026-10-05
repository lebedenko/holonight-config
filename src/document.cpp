#include "holonight/config/document.h"

#include "holonight/config/appearance.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>
#include <sstream>
#include <toml++/toml.hpp>

namespace HoloNight::Config {
namespace {
Diagnostic error(std::string message, const std::filesystem::path& path = {}) {
  return {
      .code = ErrorCode::ValidationError,
      .severity = Severity::Error,
      .message = std::move(message),
      .path = path.empty() ? std::nullopt : std::optional{path},
      .position = std::nullopt,
  };
}

std::size_t offset(std::string_view bytes, toml::source_position position) {
  std::size_t result = 0;
  for (std::size_t line = 1; line < position.line && result < bytes.size(); ++line) {
    const auto newline = bytes.find('\n', result);
    result = newline == std::string_view::npos ? bytes.size() : newline + 1;
  }
  // toml++ columns count Unicode codepoints, not bytes.
  for (std::size_t column = 1; column < position.column && result < bytes.size(); ++column) {
    ++result;
    while (result < bytes.size() && (static_cast<unsigned char>(bytes[result]) & 0xC0U) == 0x80U) {
      ++result;
    }
  }
  return result;
}

std::string encode(const Value& value) {
  return std::visit(
      [](const auto& item) -> std::string {
        using T = std::decay_t<decltype(item)>;
        if constexpr (std::is_same_v<T, AggregateValue>) {
          return item.toml;
        } else {
          std::ostringstream output;
          output << toml::toml_formatter{toml::value<T>{item}};
          return output.str();
        }
      },
      value);
}

Value valueOf(const toml::node& node) {
  if (node.is_boolean()) {
    return *node.value<bool>();
  }
  if (node.is_integer()) {
    return *node.value<std::int64_t>();
  }
  if (node.is_floating_point()) {
    return *node.value<double>();
  }
  if (node.is_string()) {
    return *node.value<std::string>();
  }
  std::ostringstream output;
  // JSON would lose TOML dates/types; use canonical TOML formatting for aggregates.
  output << toml::toml_formatter{node};
  return AggregateValue{output.str()};
}

std::optional<SourceSpan> assignment(std::string_view bytes, SourceSpan value) {
  const auto previous_newline = value.begin == 0 ? std::string_view::npos : bytes.rfind('\n', value.begin - 1);
  const std::size_t line = previous_newline == std::string_view::npos ? 0 : previous_newline + 1;
  char quote = 0;
  bool escape = false;
  std::size_t equal = std::string_view::npos;
  // Only an assignment key may precede the selected value on this line.
  for (std::size_t i = line; i < value.begin; ++i) {
    const char character = bytes[i];
    if (quote != 0) {
      if (escape) {
        escape = false;
      } else if (quote == '"' && character == '\\') {
        escape = true;
      } else if (character == quote) {
        quote = 0;
      }
    } else if (character == '\'' || character == '"') {
      {
        quote = character;
      }
    } else if (character == '=') {
      if (equal != std::string_view::npos) {
        return std::nullopt;
      }
      equal = i;
    } else if (character == '{' || character == '[' || character == ',' || character == '#') {
      {
        return std::nullopt;
      }
    }
  }
  if (equal == std::string_view::npos) {
    return std::nullopt;
  }
  return SourceSpan{.begin = line, .end = value.end};
}

void collect(const toml::table& table, const KeyPath& prefix, DocumentSnapshot& snapshot) {
  for (const auto& [key, node] : table) {
    auto path = prefix;
    path.emplace_back(key.str());
    if (node.is_table() && !node.as_table()->is_inline()) {
      collect(*node.as_table(), path, snapshot);
      continue;
    }
    const auto source = node.source();
    const SourceSpan span{
        .begin = offset(snapshot.revision.bytes, source.begin),
        .end = offset(snapshot.revision.bytes, source.end),
    };
    snapshot.overrides.emplace(
        std::move(path),
        DocumentEntry{.value = valueOf(node), .source = span, .assignment = assignment(snapshot.revision.bytes, span)});
  }
}

bool hasErrors(const std::vector<Diagnostic>& diagnostics) {
  return std::ranges::any_of(diagnostics, [](const Diagnostic& item) { return item.severity == Severity::Error; });
}

std::string keyText(const KeyPath& path) {
  std::string result;
  for (const auto& key : path) {
    if (!result.empty()) {
      result += '.';
    }
    result += encode(Value{key});
  }
  return result;
}

// Retain actual TOML comments inside a removed multiline array. A '#' inside
// any basic/literal (including multiline) string is part of the value instead.
std::string retainedComments(std::string_view value) {
  std::string result;
  char quote = 0;
  bool multiline = false;
  for (std::size_t i = 0; i < value.size();) {
    const char character = value[i];
    if (quote != 0) {
      if (quote == '"' && character == '\\') {
        i += std::min<std::size_t>(2, value.size() - i);
      } else if (character == quote && (!multiline || value.substr(i, 3) == std::string(3, quote))) {
        i += multiline ? 3 : 1;
        quote = 0;
      } else {
        {
          ++i;
        }
      }
    } else if (character == '\'' || character == '"') {
      quote = character;
      multiline = value.substr(i, 3) == std::string(3, quote);
      i += multiline ? 3 : 1;
    } else if (character == '#') {
      const auto end = value.find('\n', i);
      const auto next = end == std::string_view::npos ? value.size() : end + 1;
      result += value.substr(i, next - i);
      i = next;
    } else {
      {
        ++i;
      }
    }
  }
  return result;
}

struct Replacement {
  SourceSpan span;
  std::string text;
};
}  // namespace

std::optional<Value> DocumentSnapshot::value(const KeyPath& key) const {
  const auto found = overrides.find(key);
  return found == overrides.end() ? std::nullopt : std::optional{found->second.value};
}

Result<DocumentSnapshot> parseDocument(std::string_view bytes, const std::filesystem::path& path) {
  if (bytes.size() > kMaximumDocumentBytes) {
    auto diagnostic = error("document exceeds 65536 bytes", path);
    diagnostic.code = ErrorCode::TooLarge;
    return Result<DocumentSnapshot>::failure({std::move(diagnostic)});
  }
  try {
    const auto table = toml::parse(bytes, path.string());
    DocumentSnapshot result{.path = path, .revision = {.exists = true, .bytes = std::string{bytes}}, .overrides = {}};
    collect(table, {}, result);
    return Result<DocumentSnapshot>::success(std::move(result));
  } catch (const toml::parse_error& failure) {
    auto diagnostic = error(std::string{failure.description()}, path);
    diagnostic.code = ErrorCode::SyntaxError;
    diagnostic.position = SourcePosition{.line = failure.source().begin.line, .column = failure.source().begin.column};
    return Result<DocumentSnapshot>::failure({std::move(diagnostic)});
  }
}

Result<DocumentSnapshot> readDocument(const std::filesystem::path& path) {
  std::error_code failure;
  const auto status = std::filesystem::status(path, failure);
  if (status.type() == std::filesystem::file_type::not_found &&
      (!failure || failure == std::errc::no_such_file_or_directory)) {
    // Dangling symlinks are storage errors, not missing configuration files.
    std::error_code link_error;
    if (!std::filesystem::is_symlink(std::filesystem::symlink_status(path, link_error))) {
      DocumentSnapshot missing;
      missing.path = path;
      return Result<DocumentSnapshot>::success(std::move(missing));
    }
  }
  if (failure || !std::filesystem::is_regular_file(status)) {
    auto diagnostic = error("configuration is not a readable regular file", path);
    diagnostic.code = ErrorCode::IoError;
    return Result<DocumentSnapshot>::failure({std::move(diagnostic)});
  }
  std::ifstream input(path, std::ios::binary);
  if (!input) {
    auto diagnostic = error("cannot open configuration file", path);
    diagnostic.code = ErrorCode::IoError;
    return Result<DocumentSnapshot>::failure({std::move(diagnostic)});
  }
  std::string bytes;
  std::array<char, 8192> buffer{};
  while (input && bytes.size() <= kMaximumDocumentBytes) {
    input.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
    bytes.append(buffer.data(), static_cast<std::size_t>(input.gcount()));
  }
  if (input.bad()) {
    auto diagnostic = error("cannot read configuration file", path);
    diagnostic.code = ErrorCode::IoError;
    return Result<DocumentSnapshot>::failure({std::move(diagnostic)});
  }
  return parseDocument(bytes, path);
}

std::vector<Diagnostic> validateDocument(const DocumentSnapshot& snapshot, const DocumentSchema& schema) {
  std::vector<Diagnostic> diagnostics;
  for (const auto& field : schema.fields) {
    auto value = snapshot.value(field.key);
    if (!value) {
      value = field.default_value;
    }
    if (!value) {
      continue;
    }
    if (field.default_value && value->index() != field.default_value->index() &&
        !(std::holds_alternative<double>(*field.default_value) && std::holds_alternative<std::int64_t>(*value))) {
      diagnostics.push_back(error("invalid type for " + keyText(field.key), snapshot.path));
    } else if (field.validate) {
      auto errors = field.validate(*value);
      diagnostics.insert(diagnostics.end(), errors.begin(), errors.end());
    }
  }
  if (schema.validate_domain) {
    auto errors = schema.validate_domain(snapshot);
    diagnostics.insert(diagnostics.end(), errors.begin(), errors.end());
  }
  return diagnostics;
}

namespace {
Result<EditBatch> normalizeEdits(const EditBatch& edits, const std::filesystem::path& path) {
  EditBatch batch = edits;
  for (auto& edit : batch) {
    if (!edit.pending) {
      continue;
    }
    const auto literal = parseDocument("value = " + encode(*edit.pending));
    if (!literal || literal.value->overrides.size() != 1 || !literal.value->value({"value"})) {
      return Result<EditBatch>::failure({error("invalid value literal", path)});
    }
    edit.pending = literal.value->value({"value"});
  }
  return Result<EditBatch>::success(std::move(batch));
}

bool planEdit(const DocumentSnapshot& current, const Edit& edit, std::vector<Replacement>& replacements,
              std::map<KeyPath, std::string>& insertions, SaveResult& result) {
  const auto disk = current.value(edit.key);
  if (disk == edit.pending) {
    return true;
  }
  if (disk != edit.baseline) {
    result.conflicts.push_back({.key = edit.key, .baseline = edit.baseline, .disk = disk, .pending = edit.pending});
    return true;
  }
  const auto entry = current.overrides.find(edit.key);
  if (entry == current.overrides.end()) {
    if (edit.pending) {
      KeyPath parent(edit.key.begin(), edit.key.end() - 1);
      const std::string newline = current.revision.bytes.find("\r\n") == std::string::npos ? "\n" : "\r\n";
      insertions[parent] += keyText({edit.key.back()}) + " = " + encode(*edit.pending) + newline;
    }
    return true;
  }
  const auto& selected = entry->second;
  // AoT spans cover several assignments; never replace them as a scalar literal.
  if (!selected.assignment) {
    result.diagnostics.push_back(error("value has no safely editable assignment: " + keyText(edit.key), current.path));
    return false;
  }
  const auto original_value = std::string_view{current.revision.bytes}.substr(
      selected.source.begin, selected.source.end - selected.source.begin);
  replacements.push_back({
      .span = edit.pending ? selected.source : *selected.assignment,
      .text = edit.pending ? encode(*edit.pending) : retainedComments(original_value),
  });
  return true;
}

bool planEdits(const DocumentSnapshot& current, const EditBatch& batch, std::vector<Replacement>& replacements,
               std::map<KeyPath, std::string>& insertions, SaveResult& result) {
  std::vector<KeyPath> keys;
  for (const auto& edit : batch) {
    if (edit.key.empty() || std::ranges::find(keys, edit.key) != keys.end()) {
      result.diagnostics.push_back(error("empty or duplicate edit key", current.path));
      return false;
    }
    keys.push_back(edit.key);
    if (!planEdit(current, edit, replacements, insertions, result)) {
      return false;
    }
  }
  return true;
}

bool headerMatches(std::string_view statement, const KeyPath& parent) {
  const auto first = statement.find_first_not_of(" \t\r\n");
  if (first == std::string_view::npos || statement[first] != '[' || statement.substr(first, 2) == "[[") {
    return false;
  }
  // Let toml++ decode header paths, including quoted keys containing dots.
  const auto probe = parseDocument(std::string{statement} + "\n\"__hn_probe\" = 0\n");
  if (!probe || probe.value->overrides.size() != 1) {
    return false;
  }
  auto path = probe.value->overrides.begin()->first;
  path.pop_back();
  return path == parent;
}

std::optional<std::size_t> insertionPoint(const DocumentSnapshot& current, const KeyPath& parent) {
  if (parent.empty()) {
    return 0;
  }
  const auto& original = current.revision.bytes;
  for (std::size_t line = 0; line < original.size();) {
    const auto end = original.find('\n', line);
    const auto next = end == std::string::npos ? original.size() : end + 1;
    // Skip apparent headers inside every multiline value source span.
    const bool in_value = std::ranges::any_of(current.overrides, [line](const auto& item) {
      return line > item.second.source.begin && line < item.second.source.end;
    });
    if (!in_value && headerMatches(std::string_view{original}.substr(line, next - line), parent)) {
      return next;
    }
    line = next;
  }
  return std::nullopt;
}

void planInsertions(const DocumentSnapshot& current, std::map<KeyPath, std::string>& insertions,
                    std::vector<Replacement>& replacements) {
  const auto& original = current.revision.bytes;
  const std::string newline = original.find("\r\n") == std::string::npos ? "\n" : "\r\n";
  for (auto& [parent, text] : insertions) {
    const auto point = insertionPoint(current, parent);
    if (point) {
      if (*point != 0 && original[*point - 1] != '\n') {
        text.insert(0, newline);
      }
      replacements.push_back({.span = {.begin = *point, .end = *point}, .text = text});
    } else {
      const std::string separator = !original.empty() && original.back() != '\n' ? newline : "";
      std::string addition = separator;
      addition += '[';
      addition += keyText(parent);
      addition += ']';
      addition += newline;
      addition += text;
      replacements.push_back({.span = {.begin = original.size(), .end = original.size()}, .text = std::move(addition)});
    }
  }
}

void coalesceInsertions(std::vector<Replacement>& replacements) {
  // Keep root assignments before table headers when several patches share EOF.
  for (std::size_t i = 0; i < replacements.size(); ++i) {
    if (replacements[i].span.begin != replacements[i].span.end) {
      continue;
    }
    for (std::size_t j = i + 1; j < replacements.size();) {
      if (replacements[j].span == replacements[i].span) {
        replacements[i].text += replacements[j].text;
        replacements.erase(replacements.begin() + static_cast<std::ptrdiff_t>(j));
      } else {
        ++j;
      }
    }
  }
}

Result<DocumentSnapshot> applyReplacements(const DocumentSnapshot& current, std::vector<Replacement>& replacements,
                                           const EditBatch& batch) {
  coalesceInsertions(replacements);
  std::ranges::sort(replacements, [](const auto& left, const auto& right) {
    if (left.span.begin != right.span.begin) {
      return left.span.begin > right.span.begin;
    }
    return left.span.end > right.span.end;
  });
  auto candidate = current.revision.bytes;
  std::size_t boundary = candidate.size();
  for (const auto& replacement : replacements) {
    if (replacement.span.end > boundary || replacement.span.begin > replacement.span.end) {
      return Result<DocumentSnapshot>::failure({error("overlapping patches", current.path)});
    }
    candidate.replace(replacement.span.begin, replacement.span.end - replacement.span.begin, replacement.text);
    boundary = replacement.span.begin;
  }
  auto parsed = parseDocument(candidate, current.path);
  if (!parsed) {
    return parsed;
  }
  for (const auto& edit : batch) {
    if (parsed.value->value(edit.key) != edit.pending) {
      return Result<DocumentSnapshot>::failure({error("candidate does not match requested value", current.path)});
    }
  }
  return parsed;
}
}  // namespace

SaveResult patchDocument(const DocumentSnapshot& current, const EditBatch& edits, const DocumentSchema& schema) {
  SaveResult result;
  result.diagnostics = validateDocument(current, schema);
  if (hasErrors(result.diagnostics)) {
    result.status = SaveStatus::Invalid;
    return result;
  }
  auto batch = normalizeEdits(edits, current.path);
  if (!batch) {
    result.status = SaveStatus::UnsupportedPatch;
    result.diagnostics = std::move(batch.diagnostics);
    return result;
  }
  std::vector<Replacement> replacements;
  std::map<KeyPath, std::string> insertions;
  if (!planEdits(current, *batch.value, replacements, insertions, result)) {
    result.status = SaveStatus::UnsupportedPatch;
    return result;
  }
  if (!result.conflicts.empty()) {
    result.status = SaveStatus::Conflict;
    return result;
  }
  planInsertions(current, insertions, replacements);
  auto parsed = applyReplacements(current, replacements, *batch.value);
  if (!parsed) {
    result.status = SaveStatus::UnsupportedPatch;
    result.diagnostics = std::move(parsed.diagnostics);
    return result;
  }
  result.diagnostics = validateDocument(*parsed.value, schema);
  if (hasErrors(result.diagnostics)) {
    result.status = SaveStatus::Invalid;
    return result;
  }
  result.status = SaveStatus::Success;
  result.snapshot = std::move(parsed.value);
  return result;
}
}  // namespace HoloNight::Config
