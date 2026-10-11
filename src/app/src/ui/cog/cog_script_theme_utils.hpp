#pragma once

#include <algorithm>
#include <array>
#include <filesystem>
#include <utility>
#include <vector>

#include <QRegularExpression>

#include <QColor>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>
#include <QStringList>
#include <QString>
#include <QVariant>

#include "ui/theme_file_storage.hpp"

/**
 * Color palette used by the COG script editor and its preferences panel.
 *
 * The type intentionally stays in the global namespace for now because older
 * MainWindow preference code already refers to it directly. The implementation
 * lives under ui/cog so editor widgets can use themes without depending on
 * MainWindow internals.
 */
struct CogScriptTheme
{
    QString id{};
    QString label{};
    QColor editorBackground{};
    QColor editorForeground{};
    QColor selectionBackground{};
    QColor lineNumberBackground{};
    QColor lineNumberForeground{};
    QColor currentLineBackground{};
    QColor sectionForeground{};
    QColor keywordForeground{};
    QColor typeForeground{};
    QColor messageForeground{};
    QColor attributeForeground{};
    QColor globalForeground{};
    QColor numberForeground{};
    QColor operatorForeground{};
    QColor stringForeground{};
    QColor labelForeground{};
    QColor functionForeground{};
    QColor commentForeground{};
    QColor codeSectionForeground{};
    QColor symbolForeground{};
    QColor braceLevel1Foreground{};
    QColor braceLevel2Foreground{};
    QColor braceLevel3Foreground{};
    QColor exposedSymbolForeground{};
    QColor flagsForeground{};
    QColor findMatchBackground{};
    QColor findCurrentMatchBackground{};
    QColor identifierReferenceBackground{};

    friend bool operator==(const CogScriptTheme&, const CogScriptTheme&) = default;
};

/** Describes one user-editable COG theme color and its target theme member. */
struct CogScriptThemeColorDef
{
    const char* key{};
    const char* label{};
    QColor CogScriptTheme::*member{};
};

inline const std::array<CogScriptThemeColorDef, 28>& cogScriptThemeColorDefs()
{
    static const std::array<CogScriptThemeColorDef, 28> kColorDefs{{
        {"background", "Editor Background", &CogScriptTheme::editorBackground},
        {"text", "Editor Text", &CogScriptTheme::editorForeground},
        {"selection", "Selection", &CogScriptTheme::selectionBackground},
        {"line_bg", "Line Number Background", &CogScriptTheme::lineNumberBackground},
        {"line_text", "Line Number Text", &CogScriptTheme::lineNumberForeground},
        {"current_line", "Current Line", &CogScriptTheme::currentLineBackground},
        {"section", "Section", &CogScriptTheme::sectionForeground},
        {"keyword", "Keyword", &CogScriptTheme::keywordForeground},
        {"type", "Type", &CogScriptTheme::typeForeground},
        {"message", "Message", &CogScriptTheme::messageForeground},
        {"attribute", "Attribute", &CogScriptTheme::attributeForeground},
        {"global", "Global", &CogScriptTheme::globalForeground},
        {"number", "Number", &CogScriptTheme::numberForeground},
        {"operator", "Operator", &CogScriptTheme::operatorForeground},
        {"string", "String", &CogScriptTheme::stringForeground},
        {"label", "Label", &CogScriptTheme::labelForeground},
        {"function", "Function", &CogScriptTheme::functionForeground},
        {"comment", "Comment", &CogScriptTheme::commentForeground},
        {"code_section", "Code Section", &CogScriptTheme::codeSectionForeground},
        {"symbol", "Symbol", &CogScriptTheme::symbolForeground},
        {"brace_1", "Brace Level 1", &CogScriptTheme::braceLevel1Foreground},
        {"brace_2", "Brace Level 2", &CogScriptTheme::braceLevel2Foreground},
        {"brace_3", "Brace Level 3", &CogScriptTheme::braceLevel3Foreground},
        {"exposed_symbol", "Non-local Symbol", &CogScriptTheme::exposedSymbolForeground},
        {"flags", "Flags Keyword", &CogScriptTheme::flagsForeground},
        {"find_match", "Find Match", &CogScriptTheme::findMatchBackground},
        {"find_current_match", "Current Find Match", &CogScriptTheme::findCurrentMatchBackground},
        {"identifier_reference", "Identifier References", &CogScriptTheme::identifierReferenceBackground},
    }};
    return kColorDefs;
}
inline std::vector<CogScriptTheme> customCogScriptThemes(
    QSettings& settings,
    QString* errorMessage = nullptr);

/** Returns the built-in COG editor themes in preference display order. */
inline const std::vector<CogScriptTheme>& cogScriptThemes()
{
    static const std::vector<CogScriptTheme> kThemes{
        {
            QStringLiteral("sed_dark"),
            QStringLiteral("SED Dark"),
            QColor(30, 30, 30),
            QColor(240, 240, 240),
            QColor(38, 79, 120, 210),
            QColor(37, 37, 38),
            QColor(133, 133, 133),
            QColor(42, 45, 46),
            QColor(80, 145, 150),
            QColor(216, 160, 223),
            QColor(86, 156, 214),
            QColor(255, 166, 120),
            QColor(78, 201, 176),
            QColor(190, 183, 255),
            QColor(181, 206, 168),
            QColor(180, 180, 180),
            QColor(214, 157, 133),
            QColor(227, 183, 113),
            QColor(220, 220, 170),
            QColor(87, 166, 74),
            QColor(130, 120, 180),
            QColor(114, 220, 254),
            QColor(86, 156, 214),
            QColor(255, 215, 0),
            QColor(197, 134, 192),
            QColor(225, 102, 102),
            QColor(0, 215, 215),
            QColor(234, 92, 0, 88),
            QColor(234, 92, 0, 190),
            QColor(52, 113, 196, 150),
        },
        {
            QStringLiteral("midnight"),
            QStringLiteral("Midnight Blue"),
            QColor(12, 18, 30),
            QColor(232, 236, 243),
            QColor(69, 111, 194, 180),
            QColor(20, 25, 38),
            QColor(200, 208, 222, 140),
            QColor(76, 98, 138, 40),
            QColor(134, 189, 255),
            QColor(245, 195, 110),
            QColor(159, 213, 122),
            QColor(255, 166, 120),
            QColor(116, 213, 214),
            QColor(210, 181, 255),
            QColor(210, 161, 255),
            QColor(200, 208, 222),
            QColor(230, 147, 130),
            QColor(120, 216, 255),
            QColor(199, 220, 255),
            QColor(126, 143, 164),
            QColor(134, 189, 255),
            QColor(232, 236, 243),
            QColor(86, 156, 214),
            QColor(255, 215, 0),
            QColor(197, 134, 192),
            QColor(116, 213, 214),
            QColor(255, 110, 199),
            QColor(234, 112, 24, 96),
            QColor(255, 145, 48, 205),
            QColor(63, 128, 224, 165),
        },
        {
            QStringLiteral("graphite"),
            QStringLiteral("Graphite"),
            QColor(24, 24, 28),
            QColor(230, 230, 235),
            QColor(88, 116, 184, 170),
            QColor(30, 30, 34),
            QColor(190, 194, 204, 140),
            QColor(82, 92, 112, 44),
            QColor(124, 182, 248),
            QColor(236, 189, 98),
            QColor(147, 207, 128),
            QColor(247, 164, 120),
            QColor(108, 205, 212),
            QColor(208, 176, 244),
            QColor(208, 156, 240),
            QColor(190, 194, 204),
            QColor(225, 150, 142),
            QColor(116, 208, 246),
            QColor(210, 220, 236),
            QColor(142, 146, 156),
            QColor(210, 161, 255),
            QColor(230, 230, 235),
            QColor(86, 156, 214),
            QColor(255, 215, 0),
            QColor(197, 134, 192),
            QColor(108, 205, 212),
            QColor(97, 226, 194),
            QColor(234, 92, 0, 92),
            QColor(245, 126, 30, 200),
            QColor(57, 120, 214, 160),
        },
        {
            QStringLiteral("amber"),
            QStringLiteral("Amber Terminal"),
            QColor(25, 18, 10),
            QColor(247, 232, 205),
            QColor(154, 112, 52, 180),
            QColor(36, 26, 16),
            QColor(214, 189, 148, 150),
            QColor(117, 86, 44, 42),
            QColor(248, 199, 129),
            QColor(255, 206, 124),
            QColor(188, 214, 133),
            QColor(255, 179, 121),
            QColor(117, 214, 195),
            QColor(226, 193, 142),
            QColor(222, 183, 129),
            QColor(214, 189, 148),
            QColor(236, 165, 128),
            QColor(255, 222, 156),
            QColor(232, 214, 182),
            QColor(160, 144, 120),
            QColor(255, 207, 92),
            QColor(247, 232, 205),
            QColor(103, 175, 255),
            QColor(255, 207, 92),
            QColor(213, 148, 255),
            QColor(117, 214, 195),
            QColor(127, 219, 255),
            QColor(38, 143, 166, 100),
            QColor(40, 190, 215, 210),
            QColor(45, 125, 224, 170),
        },
    };
    return kThemes;
}

/** Returns the fallback theme used when no valid preference is configured. */
inline const CogScriptTheme& defaultCogScriptTheme()
{
    return cogScriptThemes().front();
}

/** Finds a built-in theme by id, falling back to the default theme. */
inline const CogScriptTheme& cogScriptThemeById(const QString& id)
{
    for (const CogScriptTheme& theme : cogScriptThemes())
    {
        if (theme.id.compare(id, Qt::CaseInsensitive) == 0)
        {
            return theme;
        }
    }
    return defaultCogScriptTheme();
}

inline QJsonObject cogScriptThemeJson(const CogScriptTheme& theme)
{
    QJsonObject colors{};
    for (const CogScriptThemeColorDef& def : cogScriptThemeColorDefs())
    {
        colors.insert(
            QString::fromLatin1(def.key),
            (theme.*(def.member)).name(QColor::HexArgb));
    }
    QJsonObject object{};
    object.insert(QStringLiteral("format"),
        sed::ui::theme_storage::themeFormatTag(
            sed::ui::theme_storage::ThemeFormat::cog));
    object.insert(QStringLiteral("schema"), 1);
    object.insert(QStringLiteral("id"), theme.id);
    object.insert(QStringLiteral("name"), theme.label);
    object.insert(QStringLiteral("colors"), colors);
    return object;
}

inline QString cogScriptThemeLabelFromId(const QString& id)
{
    QString label = id;
    if (label.startsWith(QStringLiteral("custom_"), Qt::CaseInsensitive))
    {
        label.remove(0, 7);
    }
    label.replace(QRegularExpression(QStringLiteral("[_-]+")), QStringLiteral(" "));
    QStringList words = label.split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
    for (QString& word : words)
    {
        if (!word.isEmpty())
        {
            word[0] = word[0].toTitleCase();
        }
    }
    return words.join(QLatin1Char(' '));
}

inline bool cogScriptThemeFromJson(
    const QJsonObject& object,
    CogScriptTheme& theme)
{
    const QJsonValue format = object.value(QStringLiteral("format"));
    const QJsonValue schema = object.value(QStringLiteral("schema"));
    const QJsonValue id = object.value(QStringLiteral("id"));
    const QJsonValue label = object.value(QStringLiteral("name"));
    const QJsonValue colors = object.value(QStringLiteral("colors"));
    if (!format.isString()
        || format.toString() != sed::ui::theme_storage::themeFormatTag(
            sed::ui::theme_storage::ThemeFormat::cog)
        || !schema.isDouble() || schema.toInt(-1) != 1
        || !id.isString() || !label.isString() || !colors.isObject())
    {
        return false;
    }

    theme.id = id.toString();
    theme.label = label.toString().trimmed();
    if (!sed::ui::theme_storage::isSafeThemeId(theme.id)
        || theme.label.isEmpty()
        || (theme.label.startsWith(QLatin1Char('#'))
            && QColor(theme.label).isValid())
        || theme.label.size() > 128)
    {
        return false;
    }

    const QJsonObject colorObject = colors.toObject();
    for (const CogScriptThemeColorDef& def : cogScriptThemeColorDefs())
    {
        const QJsonValue value = colorObject.value(QString::fromLatin1(def.key));
        if (!value.isString())
        {
            return false;
        }
        const QColor color(value.toString());
        if (!color.isValid())
        {
            return false;
        }
        theme.*(def.member) = color;
    }
    return true;
}

inline std::vector<CogScriptTheme> mergeCogScriptThemeDocuments(
    const std::vector<sed::ui::theme_storage::ThemeDocument>& documents,
    QString* errorMessage = nullptr)
{
    QString fileError = errorMessage != nullptr ? *errorMessage : QString{};
    std::vector<CogScriptTheme> themes{};
    themes.reserve(documents.size());
    for (const auto& document : documents)
    {
        CogScriptTheme theme{};
        if (!cogScriptThemeFromJson(document.object, theme)
            || theme.id != document.id)
        {
            if (fileError.isEmpty())
            {
                fileError = QStringLiteral("A COG theme file has invalid fields.");
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

inline std::vector<CogScriptTheme> loadCustomCogScriptThemesInDirectory(
    QSettings& settings,
    const std::filesystem::path& directory,
    QString* errorMessage = nullptr)
{
    Q_UNUSED(settings);
    QString fileError{};
    const auto documents = sed::ui::theme_storage::readThemeDocuments(
        directory, sed::ui::theme_storage::ThemeFormat::cog, &fileError);
    return mergeCogScriptThemeDocuments(documents, &fileError);
}

inline std::vector<CogScriptTheme> customCogScriptThemes(
    QSettings& settings,
    QString* errorMessage)
{
    QString fileError{};
    const auto directories = sed::ui::theme_storage::directoriesForSettings(
        settings, sed::AppThemeKind::cog);
    const auto documents = sed::ui::theme_storage::readThemeDocumentsFromDirectories(
        directories, sed::ui::theme_storage::ThemeFormat::cog, &fileError);
    return mergeCogScriptThemeDocuments(documents, &fileError);
}

inline CogScriptTheme cogScriptThemeById(QSettings& settings, const QString& id)
{
    for (const CogScriptTheme& theme : customCogScriptThemes(settings, nullptr))
    {
        if (theme.id.compare(id, Qt::CaseInsensitive) == 0)
        {
            return theme;
        }
    }
    return cogScriptThemeById(id);
}

inline bool preflightCustomCogScriptThemes(
    QSettings& settings,
    const std::vector<CogScriptTheme>& themes,
    QString* errorMessage = nullptr)
{
    if (errorMessage != nullptr)
    {
        errorMessage->clear();
    }
    std::vector<sed::ui::theme_storage::ThemeDocument> documents{};
    documents.reserve(themes.size());
    for (const CogScriptTheme& theme : themes)
    {
        if (!sed::ui::theme_storage::isSafeThemeId(theme.id)
            || theme.label.trimmed().isEmpty() || theme.label.size() > 128)
        {
            if (errorMessage != nullptr)
            {
                *errorMessage = QStringLiteral("A COG theme has an invalid id or label.");
            }
            return false;
        }
        for (const CogScriptThemeColorDef& def : cogScriptThemeColorDefs())
        {
            if (!(theme.*(def.member)).isValid())
            {
                if (errorMessage != nullptr)
                {
                    *errorMessage = QStringLiteral("A COG theme contains an invalid color.");
                }
                return false;
            }
        }
        CogScriptTheme normalized = theme;
        normalized.label = normalized.label.trimmed();
        documents.push_back({ normalized.id, cogScriptThemeJson(normalized),
            sed::ui::theme_storage::ThemeFormat::cog });
    }
    QString pathError{};
    const std::filesystem::path directory =
        sed::ui::theme_storage::directoryForSettings(
            settings, sed::AppThemeKind::cog, &pathError);
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
        sed::ui::theme_storage::ThemeFormat::cog);
}

inline bool saveCustomCogScriptThemesInDirectoryImpl(
    const std::vector<CogScriptTheme>& themes,
    const std::filesystem::path& directory,
    QString* errorMessage)
{
    if (errorMessage != nullptr)
    {
        errorMessage->clear();
    }
    std::vector<sed::ui::theme_storage::ThemeDocument> documents{};
    documents.reserve(themes.size());
    for (const CogScriptTheme& theme : themes)
    {
        if (!sed::ui::theme_storage::isSafeThemeId(theme.id)
            || theme.label.trimmed().isEmpty() || theme.label.size() > 128)
        {
            if (errorMessage != nullptr)
            {
                *errorMessage = QStringLiteral("A COG theme has an invalid id or label.");
            }
            return false;
        }
        for (const CogScriptThemeColorDef& def : cogScriptThemeColorDefs())
        {
            if (!(theme.*(def.member)).isValid())
            {
                if (errorMessage != nullptr)
                {
                    *errorMessage = QStringLiteral("A COG theme contains an invalid color.");
                }
                return false;
            }
        }
        CogScriptTheme normalized = theme;
        normalized.label = normalized.label.trimmed();
        documents.push_back({ normalized.id, cogScriptThemeJson(normalized),
            sed::ui::theme_storage::ThemeFormat::cog });
    }
    return sed::ui::theme_storage::saveThemeDocuments(
        directory, documents, errorMessage,
        sed::ui::theme_storage::ThemeFormat::cog);
}

inline bool saveCustomCogScriptThemesInDirectory(
    QSettings& settings,
    const std::vector<CogScriptTheme>& themes,
    const std::filesystem::path& directory,
    QString* errorMessage = nullptr)
{
    Q_UNUSED(settings);
    return saveCustomCogScriptThemesInDirectoryImpl(
        themes, directory, errorMessage);
}


inline bool saveCustomCogScriptThemes(
    QSettings& settings,
    const std::vector<CogScriptTheme>& themes,
    QString* errorMessage)
{
    QString pathError{};
    const std::filesystem::path directory =
        sed::ui::theme_storage::directoryForSettings(
            settings, sed::AppThemeKind::cog, &pathError);
    if (directory.empty())
    {
        if (errorMessage != nullptr)
        {
            *errorMessage = pathError;
        }
        return false;
    }
    return saveCustomCogScriptThemesInDirectory(
        settings, themes, directory, errorMessage);
}

/** Builds the QSettings key used for a per-color override. */
inline QString cogScriptThemeColorSettingKey(const QString& key)
{
    return QStringLiteral("cog_editor/color_%1").arg(key);
}

/** Builds the QSettings key used for a per-color override. */
inline QString cogScriptThemeColorSettingKey(const char* key)
{
    return cogScriptThemeColorSettingKey(QString::fromLatin1(key));
}

/** Resolves the active theme and applies any per-color user overrides. */
inline CogScriptTheme resolveCogScriptTheme(QSettings& settings)
{
    const QString storedId = settings.value(
        "cog_editor/theme", defaultCogScriptTheme().id).toString();
    CogScriptTheme theme = cogScriptThemeById(settings, storedId.trimmed());
    for (const CogScriptThemeColorDef& def : cogScriptThemeColorDefs())
    {
        const QColor overrideColor(settings.value(cogScriptThemeColorSettingKey(def.key)).toString().trimmed());
        if (overrideColor.isValid())
        {
            theme.*(def.member) = overrideColor;
        }
    }
    return theme;
}
