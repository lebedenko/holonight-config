#pragma once

#include "holonight/config/result.h"

#include <filesystem>
#include <map>
#include <string>

namespace HoloNight::Config {

using Environment = std::map<std::string, std::string, std::less<>>;

[[nodiscard]] Environment processEnvironment();
[[nodiscard]] Result<std::filesystem::path>
resolveAppearancePath(const Environment &environment);
[[nodiscard]] Result<std::filesystem::path> resolveAppearancePath();

} // namespace HoloNight::Config
