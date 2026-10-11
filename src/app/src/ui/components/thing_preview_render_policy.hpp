#pragma once

#include <algorithm>
#include <cstdint>

#include <QMetaType>
#include <QString>
#include <QVariant>

namespace sed::ui::detail
{

enum class ThingAssetPreviewThingMode : std::uint8_t
{
    shaded,
    wireframes,
    vertices,
};

struct ThingAssetPreviewRenderPolicy
{
    ThingAssetPreviewThingMode thingMode{ ThingAssetPreviewThingMode::shaded };
    bool textures{ true };
};

inline ThingAssetPreviewRenderPolicy thingAssetPreviewRenderPolicy(
    const int storedMode) noexcept
{
    switch (storedMode)
    {
        case -2:
            return { ThingAssetPreviewThingMode::vertices, false };
        case -1:
            return { ThingAssetPreviewThingMode::wireframes, false };
        case 0:
            return { ThingAssetPreviewThingMode::shaded, false };
        case 1:
        default:
            return { ThingAssetPreviewThingMode::shaded, true };
    }
}

struct TemplateThumbnailModeSetting
{
    int mode{}; // 0 = shaded, 1 = wireframe
    bool migrate{};
};

/** Normalizes the persisted thumbnail setting without confusing integer Wireframe with legacy bool true. */
inline TemplateThumbnailModeSetting normalizeTemplateThumbnailModeSetting(
    const QVariant& storedValue) noexcept
{
    if (storedValue.metaType().id() == QMetaType::Bool)
    {
        return { 0, true };
    }
    if (storedValue.metaType().id() == QMetaType::QString)
    {
        const QString lower = storedValue.toString().trimmed().toLower();
        if (lower == QStringLiteral("wireframe"))
        {
            return { 1, true };
        }
        if (lower == QStringLiteral("true")
            || lower == QStringLiteral("false")
            || lower == QStringLiteral("0")
            || lower == QStringLiteral("1")
            || lower == QStringLiteral("shaded")
            || lower == QStringLiteral("none"))
        {
            return { 0, lower != QStringLiteral("shaded") };
        }
        return { 0, true };
    }

    const int raw = storedValue.toInt();
    const int mode = raw == 1 ? 1 : 0;
    return { mode, mode != raw };
}

} // namespace sed::ui::detail
