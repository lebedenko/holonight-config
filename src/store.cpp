#include "holonight/config/store.h"

#include "holonight/config/codec.h"

#include <fcntl.h>
#include <unistd.h>

#include <array>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>
#include <string_view>
#include <system_error>

namespace HoloNight::Config {
namespace {

Diagnostic fileDiagnostic(ErrorCode code, std::string message,
                          const std::filesystem::path &path) {
  return Diagnostic{.code = code,
                    .severity = Severity::Error,
                    .message = std::move(message),
                    .path = path,
                    .position = std::nullopt};
}

std::string systemMessage(std::string_view operation, int error_number) {
  return std::string{operation} + ": " +
         std::error_code{error_number, std::generic_category()}.message();
}

class TemporaryFile {
public:
  TemporaryFile() = default;
  ~TemporaryFile() {
    if (descriptor_ >= 0)
      ::close(descriptor_);
    if (!path_.empty()) {
      std::error_code ignored;
      std::filesystem::remove(path_, ignored);
    }
  }
  TemporaryFile(const TemporaryFile &) = delete;
  TemporaryFile &operator=(const TemporaryFile &) = delete;

  [[nodiscard]] bool create(const std::filesystem::path &destination,
                            int *error_number) {
    const std::filesystem::path directory =
        destination.parent_path().empty() ? "." : destination.parent_path();
    std::string pattern =
        (directory / ("." + destination.filename().string() + ".tmp-XXXXXX"))
            .string();
    descriptor_ = ::mkstemp(pattern.data());
    if (descriptor_ < 0) {
      *error_number = errno;
      return false;
    }
    path_ = pattern;
    return true;
  }

  [[nodiscard]] bool writeAll(std::string_view contents,
                              int *error_number) const {
    std::size_t offset = 0;
    while (offset < contents.size()) {
      const ssize_t written = ::write(descriptor_, contents.data() + offset,
                                      contents.size() - offset);
      if (written < 0) {
        if (errno == EINTR)
          continue;
        *error_number = errno;
        return false;
      }
      offset += static_cast<std::size_t>(written);
    }
    return true;
  }

  [[nodiscard]] bool flushAndClose(int *error_number) {
    if (::fsync(descriptor_) != 0) {
      *error_number = errno;
      return false;
    }
    if (::close(descriptor_) != 0) {
      *error_number = errno;
      descriptor_ = -1;
      return false;
    }
    descriptor_ = -1;
    return true;
  }

  [[nodiscard]] const std::filesystem::path &path() const { return path_; }
  void release() { path_.clear(); }

private:
  std::filesystem::path path_;
  int descriptor_{-1};
};

} // namespace

Result<LoadedAppearance> load(const std::filesystem::path &path) {
  std::error_code filesystem_error;
  if (!std::filesystem::exists(path, filesystem_error)) {
    if (filesystem_error) {
      return Result<LoadedAppearance>::failure({fileDiagnostic(
          ErrorCode::IoError,
          "failed to inspect appearance file: " + filesystem_error.message(),
          path)});
    }
    return Result<LoadedAppearance>::success(
        LoadedAppearance{.appearance = defaults(),
                         .origin = LoadOrigin::Default},
        {Diagnostic{
            .code = ErrorCode::Missing,
            .severity = Severity::Info,
            .message = "appearance file is missing; shared defaults are active",
            .path = path,
            .position = std::nullopt}});
  }

  const std::uintmax_t size =
      std::filesystem::file_size(path, filesystem_error);
  if (filesystem_error) {
    return Result<LoadedAppearance>::failure({fileDiagnostic(
        ErrorCode::IoError,
        "failed to inspect appearance file: " + filesystem_error.message(),
        path)});
  }
  if (size > kMaximumDocumentBytes) {
    return Result<LoadedAppearance>::failure({fileDiagnostic(
        ErrorCode::TooLarge, "appearance document exceeds 65536 bytes", path)});
  }

  std::ifstream input{path, std::ios::binary};
  if (!input) {
    return Result<LoadedAppearance>::failure({fileDiagnostic(
        ErrorCode::IoError, "failed to open appearance file", path)});
  }
  std::string document;
  document.reserve(static_cast<std::size_t>(size));
  std::array<char, 8192> buffer{};
  while (input) {
    input.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
    document.append(buffer.data(), static_cast<std::size_t>(input.gcount()));
    if (document.size() > kMaximumDocumentBytes) {
      return Result<LoadedAppearance>::failure(
          {fileDiagnostic(ErrorCode::TooLarge,
                          "appearance document exceeds 65536 bytes", path)});
    }
  }
  if (input.bad()) {
    return Result<LoadedAppearance>::failure({fileDiagnostic(
        ErrorCode::IoError, "failed to read appearance file", path)});
  }

  Result<Appearance> parsed = parse(document, path);
  if (!parsed)
    return Result<LoadedAppearance>::failure(std::move(parsed.diagnostics));
  return Result<LoadedAppearance>::success(
      LoadedAppearance{.appearance = std::move(*parsed.value),
                       .origin = LoadOrigin::File},
      std::move(parsed.diagnostics));
}

Result<LoadedAppearance> load(const Environment &environment) {
  Result<std::filesystem::path> path = resolveAppearancePath(environment);
  if (!path)
    return Result<LoadedAppearance>::failure(std::move(path.diagnostics));
  return load(*path.value);
}

Result<LoadedAppearance> load() { return load(processEnvironment()); }

Result<std::filesystem::path>
writeAtomically(const Appearance &appearance,
                const std::filesystem::path &path) {
  if (path.empty() || path.filename().empty()) {
    return Result<std::filesystem::path>::failure(
        {fileDiagnostic(ErrorCode::AtomicWriteError,
                        "appearance destination path is empty", path)});
  }

  Result<std::string> encoded = serialize(appearance);
  if (!encoded) {
    for (Diagnostic &item : encoded.diagnostics)
      item.path = path;
    return Result<std::filesystem::path>::failure(
        std::move(encoded.diagnostics));
  }

  const std::filesystem::path directory =
      path.parent_path().empty() ? "." : path.parent_path();
  std::error_code filesystem_error;
  std::filesystem::create_directories(directory, filesystem_error);
  if (filesystem_error) {
    return Result<std::filesystem::path>::failure({fileDiagnostic(
        ErrorCode::AtomicWriteError,
        "failed to create appearance directory: " + filesystem_error.message(),
        path)});
  }

  int error_number = 0;
  TemporaryFile temporary;
  if (!temporary.create(path, &error_number)) {
    return Result<std::filesystem::path>::failure({fileDiagnostic(
        ErrorCode::AtomicWriteError,
        systemMessage("failed to create temporary file", error_number), path)});
  }
  if (!temporary.writeAll(*encoded.value, &error_number)) {
    return Result<std::filesystem::path>::failure({fileDiagnostic(
        ErrorCode::AtomicWriteError,
        systemMessage("failed to write temporary file", error_number), path)});
  }
  if (!temporary.flushAndClose(&error_number)) {
    return Result<std::filesystem::path>::failure({fileDiagnostic(
        ErrorCode::AtomicWriteError,
        systemMessage("failed to flush temporary file", error_number), path)});
  }
  if (::rename(temporary.path().c_str(), path.c_str()) != 0) {
    error_number = errno;
    return Result<std::filesystem::path>::failure({fileDiagnostic(
        ErrorCode::AtomicWriteError,
        systemMessage("failed to replace appearance file", error_number),
        path)});
  }
  temporary.release();
  return Result<std::filesystem::path>::success(path);
}

} // namespace HoloNight::Config
