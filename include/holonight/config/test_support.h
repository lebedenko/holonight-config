#pragma once

#include "holonight/config/appearance.h"
#include "holonight/config/path.h"

#include <filesystem>
#include <string>
#include <string_view>

namespace HoloNight::Config::TestSupport {

class TemporaryDirectory {
public:
  TemporaryDirectory();
  ~TemporaryDirectory();
  TemporaryDirectory(const TemporaryDirectory &) = delete;
  TemporaryDirectory &operator=(const TemporaryDirectory &) = delete;
  TemporaryDirectory(TemporaryDirectory &&) = delete;
  TemporaryDirectory &operator=(TemporaryDirectory &&) = delete;

  [[nodiscard]] const std::filesystem::path &path() const noexcept {
    return path_;
  }
  [[nodiscard]] std::filesystem::path child(std::string_view relative) const;

private:
  std::filesystem::path path_;
};

[[nodiscard]] std::string
validDocument(const Appearance &appearance = defaults());
[[nodiscard]] std::string documentWithUnknownField();
[[nodiscard]] Environment
environmentFor(const std::filesystem::path &config_home);

} // namespace HoloNight::Config::TestSupport
