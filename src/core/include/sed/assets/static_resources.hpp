#pragma once

#include "sed/level.hpp"

#include <cstdint>
#include <span>
#include <string_view>

namespace sed::assets
{

inline constexpr std::int32_t StaticResIdxMask = 0x8000;

std::span<const std::string_view> staticSounds(ProjectType kind) noexcept;
std::span<const std::string_view> staticMaterials(ProjectType kind) noexcept;
std::span<const std::string_view> staticModels(ProjectType kind) noexcept;
std::span<const std::string_view> staticSprites(ProjectType kind) noexcept;
std::span<const std::string_view> staticKeys(ProjectType kind) noexcept;
std::span<const std::string_view> staticCogScripts(ProjectType kind) noexcept;
std::span<const std::string_view> staticTemplates(ProjectType kind) noexcept;

std::int32_t findStaticMaterialIndex(ProjectType kind, std::string_view material) noexcept;
bool isStaticMaterial(ProjectType kind, std::string_view material) noexcept;
std::string_view staticMaterialName(ProjectType kind, std::int32_t index) noexcept;

} // namespace sed::assets
