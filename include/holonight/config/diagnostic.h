#pragma once

#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>

namespace HoloNight::Config {

enum class Severity { Info, Error };

enum class ErrorCode {
  Missing,
  PathUnavailable,
  IoError,
  TooLarge,
  SyntaxError,
  UnsupportedVersion,
  ValidationError,
  AtomicWriteError,
};

struct SourcePosition {
  std::size_t line{};
  std::size_t column{};

  bool operator==(const SourcePosition &) const = default;
};

struct Diagnostic {
  ErrorCode code{ErrorCode::ValidationError};
  Severity severity{Severity::Error};
  std::string message;
  std::optional<std::filesystem::path> path;
  std::optional<SourcePosition> position;

  bool operator==(const Diagnostic &) const = default;
};

} // namespace HoloNight::Config
