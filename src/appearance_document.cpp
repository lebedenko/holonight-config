#include "holonight/config/appearance_document.h"

#include "holonight/config/codec.h"

#include <algorithm>
#include <sstream>
#include <toml++/toml.hpp>

namespace HoloNight::Config {
namespace {
Diagnostic diagnostic(std::string message, const DocumentSnapshot& snapshot, Severity severity = Severity::Error,
                      ErrorCode code = ErrorCode::ValidationError) {
  return {
      .code = code,
      .severity = severity,
      .message = std::move(message),
      .path = snapshot.path,
      .position = std::nullopt,
  };
}

bool invalidType(const toml::node* known, const toml::node& node, bool optional_shape) {
  if (optional_shape || (known != nullptr && known->is_floating_point())) {
    return !node.is_number();
  }
  return known != nullptr && node.type() != known->type();
}

void overlay(toml::table& effective, const toml::table& incoming, const std::string& prefix,
             std::vector<Diagnostic>& notes, const DocumentSnapshot& snapshot) {
  for (const auto& [key, node] : incoming) {
    if (prefix.empty() && key.str() == "version") {
      continue;
    }
    auto* known = effective.get(key.str());
    const std::string name = prefix + std::string{key.str()};
    // These optional shape values are absent from the default template.
    const bool optional_shape = prefix == "shape." && (key.str() == "base_radius" || key.str() == "base_chamfer");
    if ((known == nullptr) && !optional_shape) {
      notes.push_back(diagnostic("unknown field " + name, snapshot, Severity::Warning));
      continue;
    }
    if ((known != nullptr) && known->is_table()) {
      if (node.is_table()) {
        overlay(*known->as_table(), *node.as_table(), name + ".", notes, snapshot);
      } else {
        notes.push_back(diagnostic("invalid table " + name, snapshot));
      }
    } else {
      if (invalidType(known, node, optional_shape)) {
        notes.push_back(diagnostic("invalid type for " + name, snapshot));
      } else {
        effective.insert_or_assign(key.str(), node);
      }
    }
  }
}
}  // namespace

Result<AppearanceDocument> decodeAppearanceDocument(const DocumentSnapshot& snapshot) {
  if (!snapshot.revision.exists) {
    return Result<AppearanceDocument>::success(
        {.snapshot = snapshot, .document_version = kEditingDocumentVersion, .appearance = defaults()});
  }
  const auto version = snapshot.value({"version"});
  if (!version || !std::holds_alternative<std::int64_t>(*version)) {
    return Result<AppearanceDocument>::failure({diagnostic("missing or invalid version", snapshot)});
  }
  const auto document_version = std::get<std::int64_t>(*version);
  if (document_version == kDocumentVersion) {
    auto parsed = parse(snapshot.revision.bytes, snapshot.path);
    if (!parsed) {
      return Result<AppearanceDocument>::failure(std::move(parsed.diagnostics));
    }
    return Result<AppearanceDocument>::success(
        {.snapshot = snapshot, .document_version = document_version, .appearance = *parsed.value},
        std::move(parsed.diagnostics));
  }
  if (document_version != kEditingDocumentVersion) {
    return Result<AppearanceDocument>::failure({
        diagnostic("unsupported appearance document version", snapshot, Severity::Error, ErrorCode::UnsupportedVersion),
    });
  }
  // This in-memory decode projection reuses the unchanged, explicit v1 validator.
  // It is never used to write an existing document.
  auto effective = toml::parse(*serialize(defaults()).value);
  const auto incoming = toml::parse(snapshot.revision.bytes);
  std::vector<Diagnostic> notes;
  overlay(effective, incoming, "", notes, snapshot);
  if (std::ranges::any_of(notes, [](const auto& note) { return note.severity == Severity::Error; })) {
    return Result<AppearanceDocument>::failure(std::move(notes));
  }
  std::ostringstream projected;
  projected << effective;
  auto decoded = parse(projected.str(), snapshot.path);
  if (!decoded) {
    return Result<AppearanceDocument>::failure(std::move(decoded.diagnostics));
  }
  return Result<AppearanceDocument>::success(
      {.snapshot = snapshot, .document_version = document_version, .appearance = *decoded.value}, std::move(notes));
}

Result<AppearanceDocument> readAppearanceDocument(const std::filesystem::path& path) {
  auto snapshot = readDocument(path);
  if (!snapshot) {
    return Result<AppearanceDocument>::failure(std::move(snapshot.diagnostics));
  }
  return decodeAppearanceDocument(*snapshot.value);
}

DocumentSchema appearanceDocumentSchema() {
  DocumentSchema schema;
  const auto encoded = serialize(defaults());
  const auto snapshot = parseDocument(*encoded.value);
  for (const auto& [key, entry] : snapshot.value->overrides) {
    if (key == KeyPath{"version"}) {
      continue;
    }
    schema.fields.push_back({
        .key = key,
        .default_value = entry.value,
        .description = "Shared appearance preference",
        .reload = ReloadPolicy::Live,
        .validate = {},
    });
  }
  schema.fields.push_back({
      .key = {"shape", "base_radius"},
      .default_value = std::nullopt,
      .description = "Optional base radius, 0–128",
      .reload = ReloadPolicy::Live,
      .validate = {},
  });
  schema.fields.push_back({
      .key = {"shape", "base_chamfer"},
      .default_value = std::nullopt,
      .description = "Optional base chamfer, 0–128",
      .reload = ReloadPolicy::Live,
      .validate = {},
  });
  // The domain decoder owns known-type, range and enum validation and unknown warnings.
  schema.validate_domain = [](const DocumentSnapshot& document) {
    return decodeAppearanceDocument(document).diagnostics;
  };
  return schema;
}

SaveResult saveAppearanceDocument(const std::filesystem::path& path, const EditBatch& edits) {
  // Version is a reserved metadata edit. Caller edits cannot override it.
  auto current = readAppearanceDocument(path);
  if (!current) {
    SaveResult result;
    result.status = SaveStatus::Invalid;
    result.diagnostics = std::move(current.diagnostics);
    return result;
  }
  EditBatch batch = edits;
  if (std::ranges::any_of(batch, [](const Edit& edit) { return edit.key == KeyPath{"version"}; })) {
    SaveResult result;
    result.status = SaveStatus::Invalid;
    result.diagnostics.push_back(diagnostic("version is reserved document metadata", current.value->snapshot));
    return result;
  }
  batch.push_back({
      .key = {"version"},
      .baseline = current.value->snapshot.value({"version"}),
      .pending = Value{kEditingDocumentVersion},
  });
  return saveDocument(path, batch, appearanceDocumentSchema());
}
}  // namespace HoloNight::Config
