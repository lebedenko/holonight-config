#pragma once

#include "holonight/config/appearance.h"
#include "holonight/config/document.h"

namespace HoloNight::Config {
inline constexpr std::int64_t kEditingDocumentVersion = 2;
struct AppearanceDocument {
  DocumentSnapshot snapshot;
  std::int64_t document_version{kEditingDocumentVersion};
  // The effective model keeps the v1 resolution/adapter ABI and meaning.
  Appearance appearance;
};
[[nodiscard]] Result<AppearanceDocument> decodeAppearanceDocument(const DocumentSnapshot& snapshot);
[[nodiscard]] Result<AppearanceDocument> readAppearanceDocument(const std::filesystem::path& path);
[[nodiscard]] DocumentSchema appearanceDocumentSchema();
// Adds/updates only version metadata and requested edits; never rewrites other values.
[[nodiscard]] SaveResult saveAppearanceDocument(const std::filesystem::path& path, const EditBatch& edits);
}  // namespace HoloNight::Config
