// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Andrii L <lebeden@gmail.com>
#pragma once
#include "holonight/config/document.h"
#include "holonight/config/path.h"
namespace HoloNight::Config {
enum class InputProfile : unsigned char { Standard, VimNavigation };
[[nodiscard]] Result<InputProfile> decodeInputDocument(const DocumentSnapshot& document);
[[nodiscard]] Result<std::filesystem::path> resolveInputPath(const Environment& environment);
[[nodiscard]] Result<std::filesystem::path> resolveInputPath();
}  // namespace HoloNight::Config
