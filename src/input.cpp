// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Andrii L <lebeden@gmail.com>
#include "holonight/config/input.h"
namespace HoloNight::Config {
Result<InputProfile> decodeInputDocument(const DocumentSnapshot& document) {
  if (!document.revision.exists) {
    return Result<InputProfile>::success(InputProfile::Standard);
  }
  const auto profile = document.value({"input", "navigation"});
  if (!profile || *profile == Value{std::string{"standard"}}) {
    return Result<InputProfile>::success(InputProfile::Standard);
  }
  if (*profile == Value{std::string{"vim-navigation"}}) {
    return Result<InputProfile>::success(InputProfile::VimNavigation);
  }
  return Result<InputProfile>::failure({
      Diagnostic{
          .code = ErrorCode::ValidationError,
          .severity = Severity::Error,
          .message = "input.navigation must be 'standard' or 'vim-navigation'",
          .path = document.path,
          .position = std::nullopt,
      },
  });
}
DocumentSchema inputDocumentSchema() {
  return {
      .fields =
          {
              {
                  .key = {"input", "navigation"},
                  .default_value = Value{std::string{"standard"}},
                  .description = "Navigation style",
                  .reload = ReloadPolicy::Live,
                  .validate =
                      [](const Value& value) {
                        DocumentSnapshot document;
                        document.revision.exists = true;
                        document.overrides[{"input", "navigation"}] = {
                            .value = value,
                            .source = {},
                            .assignment = std::nullopt,
                        };
                        return decodeInputDocument(document).diagnostics;
                      },
              },
          },
      .validate_domain = {},
  };
}
Result<std::filesystem::path> resolveInputPath(const Environment& environment) {
  // Appearance overrides never redirect input preferences.
  auto clean = environment;
  clean.erase("HOLONIGHT_APPEARANCE_FILE");
  auto result = resolveAppearancePath(clean);
  if (result) {
    *result.value = result.value->parent_path() / "config.toml";
  }
  return result;
}
Result<std::filesystem::path> resolveInputPath() { return resolveInputPath(processEnvironment()); }
}  // namespace HoloNight::Config
