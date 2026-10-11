#pragma once

#include <algorithm>
#include <array>
#include <filesystem>
#include <utility>
#include <vector>

#include <QColor>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>
#include <QString>

#include "ui/theme_file_storage.hpp"

namespace sed::render
{

/** Persisted colors controlled by a named viewport color theme. */
struct ViewportColorPalette
{
    QColor backgroundClear{};
    QColor wireframe{};
    QColor grid{};
    QColor gridMajor{};
    QColor gridAxisX{};
    QColor gridAxisY{};
    QColor gridAxisZ{};
    QColor selectionPrimary{};
    QColor selectionMulti{};
    QColor selectionEdgeMulti{};
    QColor selectionActive{};
    QColor selectionVertex{};
    QColor selectionVertexMulti{};
    QColor cogPickPrimary{};
    QColor cogPickActive{};
    QColor uvCanvasBackground{};
    QColor uvGridMain{};
    QColor uvGridSub{};
    QColor uvGridAxisU{};
    QColor uvGridAxisV{};
};

inline constexpr const char* kViewportColorThemeSetting = "viewport/color_theme";
inline constexpr const char* kViewportColorThemeNightBlue = "night_blue";
inline constexpr const char* kViewportColorThemeOriginalSed = "original_sed";

inline ViewportColorPalette nightBlueViewportColorPalette()
{
    return {
        QColor::fromRgbF(0.08, 0.09, 0.12, 1.0),
        QColor::fromRgbF(0.86, 0.87, 0.92, 1.0),
        QColor::fromRgb(112, 128, 156),
        QColor::fromRgb(170, 184, 208),
        QColor(210, 75, 70),
        QColor(95, 190, 95),
        QColor(90, 140, 215),
        QColor::fromRgbF(0.98, 0.80, 0.25, 1.0),
        QColor::fromRgbF(0.20, 0.72, 0.95, 1.0),
        QColor::fromRgb(255, 152, 56),
        QColor::fromRgbF(0.95, 0.20, 0.20, 1.0),
        QColor::fromRgbF(0.98, 0.80, 0.25, 1.0),
        QColor::fromRgbF(0.20, 0.72, 0.95, 1.0),
        QColor::fromRgbF(0.20, 0.86, 0.96, 1.0),
        QColor::fromRgbF(0.98, 0.66, 0.20, 1.0),
        QColor(15, 20, 29),
        QColor(66, 78, 96),
        QColor(48, 58, 74),
        QColor(160, 78, 78),
        QColor(78, 160, 112),
    };
}

/**
 * Pascal SED's 2D map palette, with explicit approximations for concepts that
 * did not exist in that renderer. Pascal had only X and Y grid axes, so the
 * modern Z axis keeps Night Blue's blue. The modern edge, active, vertex, and
 * COG-pick variants reuse the closest legacy selection colors.
 */
inline ViewportColorPalette originalSedViewportColorPalette()
{
    return {
        QColor(0, 0, 0),
        QColor(255, 255, 255),
        QColor(255, 0, 255),
        QColor(255, 0, 255),
        QColor(255, 255, 0),
        QColor(0, 0, 255),
        QColor(90, 140, 215),
        QColor(255, 0, 0),
        QColor(0, 255, 255),
        QColor(255, 127, 0),
        QColor(255, 0, 0),
        QColor(255, 0, 0),
        QColor(0, 255, 255),
        QColor(255, 0, 0),
        QColor(255, 127, 0),
        QColor(0, 0, 0),
        QColor(255, 0, 255),
        QColor(127, 0, 127),
        QColor(255, 255, 0),
        QColor(0, 0, 255),
    };
}

inline constexpr std::array<std::pair<const char*, QColor ViewportColorPalette::*>, 20>
    kViewportColorPaletteFields{{
        { "background_clear", &ViewportColorPalette::backgroundClear },
        { "wireframe", &ViewportColorPalette::wireframe },
        { "grid", &ViewportColorPalette::grid },
        { "grid_major", &ViewportColorPalette::gridMajor },
        { "grid_axis_x", &ViewportColorPalette::gridAxisX },
        { "grid_axis_y", &ViewportColorPalette::gridAxisY },
        { "grid_axis_z", &ViewportColorPalette::gridAxisZ },
        { "selection_primary", &ViewportColorPalette::selectionPrimary },
        { "selection_multi", &ViewportColorPalette::selectionMulti },
        { "selection_edge_multi", &ViewportColorPalette::selectionEdgeMulti },
        { "selection_active", &ViewportColorPalette::selectionActive },
        { "selection_vertex", &ViewportColorPalette::selectionVertex },
        { "selection_vertex_multi", &ViewportColorPalette::selectionVertexMulti },
        { "cog_pick_primary", &ViewportColorPalette::cogPickPrimary },
        { "cog_pick_active", &ViewportColorPalette::cogPickActive },
        { "uv_canvas_background", &ViewportColorPalette::uvCanvasBackground },
        { "uv_grid_main", &ViewportColorPalette::uvGridMain },
        { "uv_grid_sub", &ViewportColorPalette::uvGridSub },
        { "uv_grid_axis_u", &ViewportColorPalette::uvGridAxisU },
        { "uv_grid_axis_v", &ViewportColorPalette::uvGridAxisV },
    }};

inline bool loadViewportColorPalette(
    const QSettings& settings,
    const QString& prefix,
    ViewportColorPalette& palette)
{
    bool loadedAny = false;
    for (const auto& [key, member] : kViewportColorPaletteFields)
    {
        const QString setting = prefix + QLatin1Char('/') + QLatin1String(key);
        if (!settings.contains(setting))
        {
            continue;
        }
        const QColor parsed(settings.value(setting).toString().trimmed());
        if (parsed.isValid())
        {
            QColor opaque = parsed;
            opaque.setAlpha(255);
            palette.*member = opaque;
            loadedAny = true;
        }
    }
    return loadedAny;
}

inline void saveViewportColorPalette(
    QSettings& settings,
    const QString& prefix,
    const ViewportColorPalette& palette)
{
    for (const auto& [key, member] : kViewportColorPaletteFields)
    {
        settings.setValue(
            prefix + QLatin1Char('/') + QLatin1String(key),
            (palette.*member).name(QColor::HexRgb));
    }
}

inline bool sameViewportColorPalette(
    const ViewportColorPalette& left,
    const ViewportColorPalette& right)
{
    for (const auto& [key, member] : kViewportColorPaletteFields)
    {
        Q_UNUSED(key);
        if ((left.*member).name(QColor::HexRgb)
            != (right.*member).name(QColor::HexRgb))
        {
            return false;
        }
    }
    return true;
}


struct ViewportColorTheme
{
    QString id{};
    QString label{};
    ViewportColorPalette palette{};
};

struct ViewportColorThemeSelection
{
    QString id{};
    ViewportColorPalette palette{};
};

inline std::array<ViewportColorTheme, 2> builtInViewportColorThemes()
{
    return {{
        {
            QString::fromLatin1(kViewportColorThemeNightBlue),
            QStringLiteral("Midnight Blue"),
            nightBlueViewportColorPalette(),
        },
        {
            QString::fromLatin1(kViewportColorThemeOriginalSed),
            QStringLiteral("JED"),
            originalSedViewportColorPalette(),
        },
    }};
}

inline QJsonObject viewportColorThemeJson(const ViewportColorTheme& theme)
{
    QJsonObject colors{};
    for (const auto& [key, member] : kViewportColorPaletteFields)
    {
        colors.insert(QLatin1String(key),
            (theme.palette.*member).name(QColor::HexRgb));
    }
    QJsonObject object{};
    object.insert(QStringLiteral("format"),
        sed::ui::theme_storage::themeFormatTag(
            sed::ui::theme_storage::ThemeFormat::viewport));
    object.insert(QStringLiteral("schema"), 1);
    object.insert(QStringLiteral("id"), theme.id);
    object.insert(QStringLiteral("name"), theme.label);
    object.insert(QStringLiteral("colors"), colors);
    return object;
}

inline bool viewportColorThemeFromJson(
    const QJsonObject& object,
    ViewportColorTheme& theme)
{
    const QJsonValue format = object.value(QStringLiteral("format"));
    const QJsonValue schema = object.value(QStringLiteral("schema"));
    const QJsonValue id = object.value(QStringLiteral("id"));
    const QJsonValue label = object.value(QStringLiteral("name"));
    const QJsonValue colors = object.value(QStringLiteral("colors"));
    if (!format.isString()
        || format.toString() != sed::ui::theme_storage::themeFormatTag(
            sed::ui::theme_storage::ThemeFormat::viewport)
        || !schema.isDouble() || schema.toInt(-1) != 1
        || !id.isString() || !label.isString() || !colors.isObject())
    {
        return false;
    }

    theme.id = id.toString();
    theme.label = label.toString().trimmed();
    if (!sed::ui::theme_storage::isSafeThemeId(theme.id)
        || theme.label.isEmpty() || theme.label.size() > 128)
    {
        return false;
    }

    const QJsonObject colorObject = colors.toObject();
    theme.palette = nightBlueViewportColorPalette();
    for (const auto& [key, member] : kViewportColorPaletteFields)
    {
        const QJsonValue value = colorObject.value(QLatin1String(key));
        if (!value.isString())
        {
            return false;
        }
        const QColor parsed(value.toString());
        if (!parsed.isValid())
        {
            return false;
        }
        QColor opaque = parsed;
        opaque.setAlpha(255);
        theme.palette.*member = opaque;
    }
    return true;
}

inline std::vector<ViewportColorTheme> mergeViewportColorThemeDocuments(
    const std::vector<sed::ui::theme_storage::ThemeDocument>& documents,
    QString* errorMessage = nullptr)
{
    QString fileError = errorMessage != nullptr ? *errorMessage : QString{};
    std::vector<ViewportColorTheme> themes{};
    themes.reserve(documents.size());
    for (const auto& document : documents)
    {
        ViewportColorTheme theme{};
        if (!viewportColorThemeFromJson(document.object, theme)
            || theme.id != document.id)
        {
            if (fileError.isEmpty())
            {
                fileError = QStringLiteral("A viewport theme file has invalid fields.");
            }
            continue;
        }
        themes.push_back(std::move(theme));
    }

    if (errorMessage != nullptr)
    {
        *errorMessage = fileError;
    }
    return themes;
}

inline std::vector<ViewportColorTheme> loadCustomViewportColorThemesInDirectory(
    QSettings& settings,
    const std::filesystem::path& directory,
    QString* errorMessage = nullptr)
{
    Q_UNUSED(settings);
    QString fileError{};
    const auto documents = sed::ui::theme_storage::readThemeDocuments(
        directory, sed::ui::theme_storage::ThemeFormat::viewport, &fileError);
    return mergeViewportColorThemeDocuments(documents, &fileError);
}

inline std::vector<ViewportColorTheme> loadCustomViewportColorThemes(
    QSettings& settings,
    QString* errorMessage = nullptr)
{
    QString fileError{};
    const auto directories = sed::ui::theme_storage::directoriesForSettings(
        settings, sed::AppThemeKind::viewport);
    const auto documents = sed::ui::theme_storage::readThemeDocumentsFromDirectories(
        directories, sed::ui::theme_storage::ThemeFormat::viewport, &fileError);
    return mergeViewportColorThemeDocuments(documents, &fileError);
}

inline bool preflightCustomViewportColorThemes(
    QSettings& settings,
    const std::vector<ViewportColorTheme>& themes,
    QString* errorMessage = nullptr)
{
    if (errorMessage != nullptr)
    {
        errorMessage->clear();
    }
    std::vector<sed::ui::theme_storage::ThemeDocument> documents{};
    documents.reserve(themes.size());
    for (const ViewportColorTheme& theme : themes)
    {
        if (!sed::ui::theme_storage::isSafeThemeId(theme.id)
            || theme.label.trimmed().isEmpty() || theme.label.size() > 128)
        {
            if (errorMessage != nullptr)
            {
                *errorMessage = QStringLiteral("A viewport theme has an invalid id or label.");
            }
            return false;
        }
        for (const auto& [key, member] : kViewportColorPaletteFields)
        {
            Q_UNUSED(key);
            if (!(theme.palette.*member).isValid())
            {
                if (errorMessage != nullptr)
                {
                    *errorMessage = QStringLiteral("A viewport theme contains an invalid color.");
                }
                return false;
            }
        }
        ViewportColorTheme normalized = theme;
        normalized.label = normalized.label.trimmed();
        documents.push_back({ normalized.id, viewportColorThemeJson(normalized),
            sed::ui::theme_storage::ThemeFormat::viewport });
    }
    QString pathError{};
    const std::filesystem::path directory =
        sed::ui::theme_storage::directoryForSettings(
            settings, sed::AppThemeKind::viewport, &pathError);
    if (directory.empty())
    {
        if (errorMessage != nullptr)
        {
            *errorMessage = pathError;
        }
        return false;
    }
    return sed::ui::theme_storage::preflightThemeDocuments(
        directory, documents, errorMessage,
        sed::ui::theme_storage::ThemeFormat::viewport);
}

inline bool saveCustomViewportColorThemesInDirectoryImpl(
    const std::vector<ViewportColorTheme>& themes,
    const std::filesystem::path& directory,
    QString* errorMessage)
{
    if (errorMessage != nullptr)
    {
        errorMessage->clear();
    }
    std::vector<sed::ui::theme_storage::ThemeDocument> documents{};
    documents.reserve(themes.size());
    for (const ViewportColorTheme& theme : themes)
    {
        if (!sed::ui::theme_storage::isSafeThemeId(theme.id)
            || theme.label.trimmed().isEmpty() || theme.label.size() > 128)
        {
            if (errorMessage != nullptr)
            {
                *errorMessage = QStringLiteral("A viewport theme has an invalid id or label.");
            }
            return false;
        }
        for (const auto& [key, member] : kViewportColorPaletteFields)
        {
            Q_UNUSED(key);
            if (!(theme.palette.*member).isValid())
            {
                if (errorMessage != nullptr)
                {
                    *errorMessage = QStringLiteral("A viewport theme contains an invalid color.");
                }
                return false;
            }
        }
        ViewportColorTheme normalized = theme;
        normalized.label = normalized.label.trimmed();
        documents.push_back({ normalized.id, viewportColorThemeJson(normalized),
            sed::ui::theme_storage::ThemeFormat::viewport });
    }
    return sed::ui::theme_storage::saveThemeDocuments(
        directory, documents, errorMessage,
        sed::ui::theme_storage::ThemeFormat::viewport);
}

inline bool saveCustomViewportColorThemesInDirectory(
    QSettings& settings,
    const std::vector<ViewportColorTheme>& themes,
    const std::filesystem::path& directory,
    QString* errorMessage = nullptr)
{
    Q_UNUSED(settings);
    return saveCustomViewportColorThemesInDirectoryImpl(
        themes, directory, errorMessage);
}


inline bool saveCustomViewportColorThemes(
    QSettings& settings,
    const std::vector<ViewportColorTheme>& themes,
    QString* errorMessage)
{
    QString pathError{};
    const std::filesystem::path directory =
        sed::ui::theme_storage::directoryForSettings(
            settings, sed::AppThemeKind::viewport, &pathError);
    if (directory.empty())
    {
        if (errorMessage != nullptr)
        {
            *errorMessage = pathError;
        }
        return false;
    }
    return saveCustomViewportColorThemesInDirectory(
        settings, themes, directory, errorMessage);
}

inline void saveCustomViewportColorThemes(
    QSettings& settings,
    const std::vector<ViewportColorTheme>& themes)
{
    (void)saveCustomViewportColorThemes(settings, themes, nullptr);
}

inline ViewportColorThemeSelection resolveViewportColorTheme(QSettings& settings)
{
    const QString storedId = settings.value(
        QLatin1String(kViewportColorThemeSetting),
        QLatin1String(kViewportColorThemeNightBlue)).toString().trimmed();
    for (const ViewportColorTheme& theme : builtInViewportColorThemes())
    {
        if (theme.id.compare(storedId, Qt::CaseInsensitive) == 0)
        {
            return { theme.id, theme.palette };
        }
    }
    for (const ViewportColorTheme& theme : loadCustomViewportColorThemes(settings))
    {
        if (theme.id.compare(storedId, Qt::CaseInsensitive) == 0)
        {
            return { theme.id, theme.palette };
        }
    }
    return {
        QLatin1String(kViewportColorThemeNightBlue),
        nightBlueViewportColorPalette(),
    };
}

} // namespace sed::render
