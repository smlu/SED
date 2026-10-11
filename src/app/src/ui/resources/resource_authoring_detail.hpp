#pragma once

#include "sed/assets/snd.hpp"
#include "sed/assets/spr.hpp"
#include "ui/resources/resource_authoring_editor.hpp"

#include <QString>

#include <cstdint>
#include <filesystem>
#include <optional>

namespace sed::ui::resources::detail
{

enum class SoundTreeItemKind : int
{
    mode,
    alternative,
    addAlternative,
    addMode,
    columnHeader,
};

inline constexpr int kSoundTreeItemKindRole = Qt::UserRole + 20;
inline constexpr int kSoundTreeSourceIndexRole = Qt::UserRole + 21;
inline constexpr int kSoundTreeModeBytesRole = Qt::UserRole + 22;
inline constexpr int kSoundTreeModeSlotRole = Qt::UserRole + 23;
inline constexpr int kSoundTreePreviewLoopRole = Qt::UserRole + 24;

assets::SprProfile defaultSpriteProfile(ProjectType projectType) noexcept;
assets::SoundClassProfile soundProfile(ProjectType projectType) noexcept;

QString qPath(const std::filesystem::path& path);
QString floatText(float value);
std::optional<float> parseFiniteFloat(const QString& text);
std::optional<std::uint32_t> parseHexFlags(const QString& text);
QString flagsText(std::uint32_t flags);

} // namespace sed::ui::resources::detail
