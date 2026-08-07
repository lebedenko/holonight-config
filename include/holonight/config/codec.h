#pragma once

#include "holonight/config/appearance.h"
#include "holonight/config/result.h"

#include <filesystem>
#include <string>
#include <string_view>

namespace HoloNight::Config {

[[nodiscard]] Result<Appearance>
parse(std::string_view document, const std::filesystem::path &source_path = {});
[[nodiscard]] Result<std::string> serialize(const Appearance &appearance);

} // namespace HoloNight::Config
