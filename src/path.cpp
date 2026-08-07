#include "holonight/config/path.h"

#include <cstdlib>
#include <string_view>

namespace HoloNight::Config {
namespace {

std::string valueFor(const Environment& environment, std::string_view name) {
  const auto found = environment.find(name);
  return found == environment.end() ? std::string{} : found->second;
}

void addProcessValue(Environment& environment, const char* name) {
  if (const char* value = std::getenv(name); value != nullptr) {
    environment.emplace(name, value);
  }
}

}  // namespace

Environment processEnvironment() {
  Environment environment;
  addProcessValue(environment, "HOLONIGHT_APPEARANCE_FILE");
  addProcessValue(environment, "XDG_CONFIG_HOME");
  addProcessValue(environment, "HOME");
  return environment;
}

Result<std::filesystem::path> resolveAppearancePath(const Environment& environment) {
  if (std::string override_path = valueFor(environment, "HOLONIGHT_APPEARANCE_FILE"); !override_path.empty()) {
    return Result<std::filesystem::path>::success(std::filesystem::path{std::move(override_path)});
  }
  if (std::string xdg = valueFor(environment, "XDG_CONFIG_HOME"); !xdg.empty()) {
    return Result<std::filesystem::path>::success(std::filesystem::path{std::move(xdg)} / "holonight" /
                                                  "appearance.toml");
  }
  if (std::string home = valueFor(environment, "HOME"); !home.empty()) {
    return Result<std::filesystem::path>::success(std::filesystem::path{std::move(home)} / ".config" / "holonight" /
                                                  "appearance.toml");
  }
  return Result<std::filesystem::path>::failure(
      {Diagnostic{.code = ErrorCode::PathUnavailable,
                  .severity = Severity::Error,
                  .message = "cannot resolve appearance path: HOLONIGHT_APPEARANCE_FILE, "
                             "XDG_CONFIG_HOME, and HOME "
                             "are empty",
                  .path = std::nullopt,
                  .position = std::nullopt}});
}

Result<std::filesystem::path> resolveAppearancePath() { return resolveAppearancePath(processEnvironment()); }

}  // namespace HoloNight::Config
