#pragma once

#include "holonight/config/appearance.h"
#include "holonight/config/path.h"
#include "holonight/config/result.h"

#include <filesystem>

namespace HoloNight::Config {

enum class LoadOrigin { File, Default };

struct LoadedAppearance {
  Appearance appearance;
  LoadOrigin origin{LoadOrigin::File};
  bool operator==(const LoadedAppearance &) const = default;
};

[[nodiscard]] Result<LoadedAppearance> load(const std::filesystem::path &path);
[[nodiscard]] Result<LoadedAppearance> load(const Environment &environment);
[[nodiscard]] Result<LoadedAppearance> load();
[[nodiscard]] Result<std::filesystem::path>
writeAtomically(const Appearance &appearance,
                const std::filesystem::path &path);

} // namespace HoloNight::Config
