#pragma once

#include "holonight/config/diagnostic.h"

#include <optional>
#include <utility>
#include <vector>

namespace HoloNight::Config {

template <typename T> struct Result {
  std::optional<T> value;
  std::vector<Diagnostic> diagnostics;

  [[nodiscard]] bool hasValue() const noexcept { return value.has_value(); }
  [[nodiscard]] explicit operator bool() const noexcept { return hasValue(); }

  [[nodiscard]] static Result success(T result,
                                      std::vector<Diagnostic> notes = {}) {
    return Result{.value = std::move(result), .diagnostics = std::move(notes)};
  }

  [[nodiscard]] static Result failure(std::vector<Diagnostic> errors) {
    return Result{.value = std::nullopt, .diagnostics = std::move(errors)};
  }
};

} // namespace HoloNight::Config
