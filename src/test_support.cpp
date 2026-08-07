#include "holonight/config/test_support.h"

#include "holonight/config/codec.h"
#include "holonight/config/path.h"

#include <cstdlib>
#include <stdexcept>
#include <system_error>

namespace HoloNight::Config::TestSupport {

TemporaryDirectory::TemporaryDirectory() {
  std::filesystem::path pattern =
      std::filesystem::temp_directory_path() / "holonight-config-XXXXXX";
  std::string writable = pattern.string();
  writable.push_back('\0');
  char *created = ::mkdtemp(writable.data());
  if (created == nullptr)
    throw std::runtime_error{"failed to create temporary directory"};
  path_ = created;
}

TemporaryDirectory::~TemporaryDirectory() {
  std::error_code ignored;
  std::filesystem::remove_all(path_, ignored);
}

std::filesystem::path
TemporaryDirectory::child(std::string_view relative) const {
  return path_ / std::filesystem::path{relative};
}

std::string validDocument(const Appearance &appearance) {
  Result<std::string> result = serialize(appearance);
  if (!result)
    throw std::logic_error{"test fixture appearance is invalid"};
  return std::move(*result.value);
}

std::string documentWithUnknownField() {
  std::string document = validDocument();
  document += "\n[unexpected]\nvalue = true\n";
  return document;
}

Environment environmentFor(const std::filesystem::path &config_home) {
  return Environment{{"XDG_CONFIG_HOME", config_home.string()}};
}

} // namespace HoloNight::Config::TestSupport
