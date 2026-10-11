#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <filesystem>
#include <vector>

#include <QByteArray>
#include <QColor>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QRegularExpression>
#include <QString>
#include <QStringList>
#include <QSet>
#include <QSettings>
#include <QStandardPaths>
#include <QUuid>

#include "app_settings.hpp"

namespace sed::ui::theme_storage
{

inline constexpr qsizetype kMaximumThemeFileBytes = 16 * 1024;
inline constexpr int kMaximumThemeFiles = 128;

enum class ThemeFormat : std::uint8_t
{
    unknown,
    viewport,
    cog,
};

inline QString themeFormatTag(const ThemeFormat format)
{
    switch (format)
    {
        case ThemeFormat::viewport:
            return QStringLiteral("sed.app-theme.v1");
        case ThemeFormat::cog:
            return QStringLiteral("sed.cog-theme.v1");
        case ThemeFormat::unknown:
            break;
    }
    return {};
}

struct ThemeDocument
{
    QString id{};
    QJsonObject object{};
    ThemeFormat format{ ThemeFormat::unknown };
};

inline QString typedThemeIdentity(const QString& id, const ThemeFormat format)
{
    return themeFormatTag(format) + QLatin1Char(0x1f) + id.toCaseFolded();
}

inline ThemeFormat themeFormatFromObject(const QJsonObject& object)
{
    const QJsonValue formatValue = object.value(QStringLiteral("format"));
    if (!formatValue.isString())
    {
        return ThemeFormat::unknown;
    }
    const QString format = formatValue.toString();
    if (format == themeFormatTag(ThemeFormat::viewport))
    {
        return ThemeFormat::viewport;
    }
    if (format == themeFormatTag(ThemeFormat::cog))
    {
        return ThemeFormat::cog;
    }
    return ThemeFormat::unknown;
}

inline bool themeColorPayloadIsValid(
    const QJsonObject& object,
    const ThemeFormat format)
{
    const QJsonValue colorsValue = object.value(QStringLiteral("colors"));
    if (!colorsValue.isObject()) return false;
    const QJsonObject colors = colorsValue.toObject();
    static constexpr std::array<const char*, 20> viewportColorKeys{{
        "background_clear", "wireframe", "grid", "grid_major",
        "grid_axis_x", "grid_axis_y", "grid_axis_z", "selection_primary",
        "selection_multi", "selection_edge_multi", "selection_active",
        "selection_vertex", "selection_vertex_multi", "cog_pick_primary",
        "cog_pick_active", "uv_canvas_background", "uv_grid_main",
        "uv_grid_sub", "uv_grid_axis_u", "uv_grid_axis_v",
    }};
    static constexpr std::array<const char*, 28> cogColorKeys{{
        "background", "text", "selection", "line_bg", "line_text",
        "current_line", "section", "keyword", "type", "message",
        "attribute", "global", "number", "operator", "string", "label",
        "function", "comment", "code_section", "symbol", "brace_1",
        "brace_2", "brace_3", "exposed_symbol", "flags", "find_match",
        "find_current_match", "identifier_reference",
    }};
    if (format == ThemeFormat::viewport)
    {
        if (colors.size() != static_cast<int>(viewportColorKeys.size())) return false;
        for (const char* key : viewportColorKeys)
        {
            const QJsonValue value = colors.value(QLatin1String(key));
            if (!value.isString() || !QColor(value.toString()).isValid()) return false;
        }
        return true;
    }
    if (format == ThemeFormat::cog)
    {
        if (colors.size() != static_cast<int>(cogColorKeys.size())) return false;
        for (const char* key : cogColorKeys)
        {
            const QJsonValue value = colors.value(QLatin1String(key));
            if (!value.isString() || !QColor(value.toString()).isValid()) return false;
        }
        return true;
    }
    return false;
}

inline QString themeSlugFromLabel(QString label)
{
    label = label.trimmed().toLower();
    label.replace(QRegularExpression(QStringLiteral("[^a-z0-9]+")), QStringLiteral("_"));
    label.remove(QRegularExpression(QStringLiteral("^_+|_+$")));
    if (label.isEmpty())
    {
        label = QStringLiteral("theme");
    }
    return label.left(64);
}

inline bool themeNamesCollide(const QString& left, const QString& right)
{
    const QString normalizedLeft = left.trimmed();
    const QString normalizedRight = right.trimmed();
    return normalizedLeft.compare(normalizedRight, Qt::CaseInsensitive) == 0
        || themeSlugFromLabel(normalizedLeft)
            == themeSlugFromLabel(normalizedRight);
}

inline QString themeFileNameForLabel(
    const QString& label,
    const ThemeFormat format)
{
    const QString suffix = format == ThemeFormat::viewport
        ? QStringLiteral(".app.json")
        : format == ThemeFormat::cog
            ? QStringLiteral(".cog.json")
            : QStringLiteral(".json");
    return themeSlugFromLabel(label) + suffix;
}

inline QString themeFilePathForLabel(
    const std::filesystem::path& directory,
    const QString& label,
    const ThemeFormat format)
{
    return QDir(QString::fromStdWString(directory.wstring())).filePath(
        themeFileNameForLabel(label, format));
}

inline QString themeFilePathForDocument(
    const std::filesystem::path& directory,
    const ThemeDocument& document)
{
    const ThemeFormat format = document.format == ThemeFormat::unknown
        ? themeFormatFromObject(document.object)
        : document.format;
    return themeFilePathForLabel(
        directory,
        document.object.value(QStringLiteral("name")).toString(),
        format);
}

struct ThemeStoreSnapshot
{
    std::filesystem::path directory{};
    QSet<QString> targetIds{};
    QSet<QString> existingTargetIds{};
    QHash<QString, QByteArray> targetBytes{};
};

inline std::filesystem::path perUserDataDirectoryForSettings(
    const QSettings& settings)
{
    const QString settingsFile = settings.fileName();
    if (settingsFile.isEmpty())
    {
        return sed::appThemePerUserDataDirectory();
    }

    const QFileInfo requestedInfo(settingsFile);
    const QFileInfo applicationInfo(QString::fromStdWString(
        sed::appSettingsIniPath().wstring()));
#ifdef SED_APP_MAIN_WINDOW_TEST_ACCESS
    if (sed::appSettingsIniPathIsOverriddenForTest())
    {
        return std::filesystem::path(
            requestedInfo.absolutePath().toStdWString()) / ".sed-theme-user-data";
    }
#endif
    if (requestedInfo.absoluteFilePath() != applicationInfo.absoluteFilePath())
    {
        // Explicit settings files are test or isolated-profile stores. Keep
        // their fallback beside that file instead of reading real user themes.
        return std::filesystem::path(
            requestedInfo.absolutePath().toStdWString()) / ".sed-theme-user-data";
    }
    return sed::appThemePerUserDataDirectory();
}

inline std::vector<std::filesystem::path> directoriesForSettings(
    const QSettings& settings,
    const sed::AppThemeKind kind)
{
    const QString settingsFile = settings.fileName();
    if (settingsFile.isEmpty())
    {
        return sed::appThemeDirectorySearchPaths(
            kind,
            sed::appSettingsIniPath(),
            perUserDataDirectoryForSettings(settings));
    }

    return sed::appThemeDirectorySearchPaths(
        kind,
        std::filesystem::path(settingsFile.toStdWString()),
        perUserDataDirectoryForSettings(settings));
}

inline std::filesystem::path directoryForSettings(
    const QSettings& settings,
    const sed::AppThemeKind kind,
    QString* errorMessage = nullptr)
{
    const QString settingsFile = settings.fileName();
    if (settingsFile.isEmpty())
    {
        return sed::appThemeDirectoryPath(kind, errorMessage);
    }

    return sed::appThemeDirectoryPath(
        kind,
        std::filesystem::path(settingsFile.toStdWString()),
        perUserDataDirectoryForSettings(settings),
        errorMessage);
}

inline void setError(QString* errorMessage, const QString& message)
{
    if (errorMessage != nullptr && errorMessage->isEmpty())
    {
        *errorMessage = message;
    }
}

inline bool isSafeThemeId(const QString& id)
{
    if (id.size() < 8 || id.size() > 80
        || id.left(7).compare(QStringLiteral("custom_"), Qt::CaseInsensitive) != 0)
    {
        return false;
    }

    for (const QChar character : id)
    {
        const ushort code = character.unicode();
        const bool asciiLetter = (code >= 'a' && code <= 'z')
            || (code >= 'A' && code <= 'Z');
        const bool asciiDigit = code >= '0' && code <= '9';
        if (!asciiLetter && !asciiDigit && code != '_' && code != '-')
        {
            return false;
        }
    }
    return true;
}

inline bool snapshotThemeDocuments(
    const std::filesystem::path& directory,
    const std::vector<ThemeDocument>& documents,
    ThemeStoreSnapshot* snapshot,
    QString* errorMessage = nullptr)
{
    if (errorMessage != nullptr) errorMessage->clear();
    if (snapshot == nullptr || directory.empty())
    {
        setError(errorMessage, QStringLiteral("Theme storage snapshot is unavailable."));
        return false;
    }
    *snapshot = {};
    snapshot->directory = directory;
    for (const ThemeDocument& document : documents)
    {
        snapshot->targetIds.insert(QFileInfo(
            themeFilePathForDocument(directory, document)).fileName());
    }
    const QString directoryName = QString::fromStdWString(directory.wstring());
    const QFileInfo directoryInfo(directoryName);
    if (directoryInfo.exists() && !directoryInfo.isDir())
    {
        setError(errorMessage, QStringLiteral("Theme storage path is not a directory."));
        return false;
    }
    if (!directoryInfo.exists()) return true;
    for (const ThemeDocument& document : documents)
    {
        const QString path = themeFilePathForDocument(directory, document);
        const QString name = QFileInfo(path).fileName();
        const QFileInfo info(path);
        if (!info.exists() && !info.isSymLink()) continue;
        if (info.isSymLink() || !info.isFile() || info.size() < 0
            || info.size() > kMaximumThemeFileBytes)
        {
            setError(errorMessage, QStringLiteral("An existing theme file cannot be backed up safely."));
            return false;
        }
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly))
        {
            setError(errorMessage, QStringLiteral("An existing theme file could not be backed up."));
            return false;
        }
        const QByteArray bytes = file.read(kMaximumThemeFileBytes + 1);
        if (file.error() != QFileDevice::NoError || bytes.size() > kMaximumThemeFileBytes)
        {
            setError(errorMessage, QStringLiteral("An existing theme file could not be backed up."));
            return false;
        }
        snapshot->existingTargetIds.insert(name);
        snapshot->targetBytes.insert(name, bytes);
    }
    return true;
}

inline bool restoreThemeStoreSnapshot(
    const ThemeStoreSnapshot& snapshot,
    QString* errorMessage = nullptr)
{
    if (errorMessage != nullptr) errorMessage->clear();
    if (snapshot.directory.empty())
    {
        setError(errorMessage, QStringLiteral("Theme storage rollback has no target directory."));
        return false;
    }
    const QString directoryName = QString::fromStdWString(snapshot.directory.wstring());
    bool restored = QDir().mkpath(directoryName);
    for (const QString& fileName : snapshot.targetIds)
    {
        const QString path = QDir(directoryName).filePath(fileName);
        const QFileInfo current(path);
        if (snapshot.existingTargetIds.contains(fileName))
        {
            if (current.isSymLink()) { restored = false; continue; }
            const QByteArray bytes = snapshot.targetBytes.value(fileName);
            QSaveFile restore(path);
            restore.setDirectWriteFallback(false);
            const bool thisRestored = restore.open(QIODevice::WriteOnly)
                && restore.write(bytes) == bytes.size() && restore.commit();
            if (restore.isOpen()) restore.cancelWriting();
            restored = thisRestored && restored;
        }
        else if (current.exists() || current.isSymLink())
        {
            const bool thisRestored = !current.isSymLink() && current.isFile()
                && (QFile::remove(path) || !QFileInfo::exists(path));
            restored = thisRestored && restored;
        }
    }
    if (!restored)
    {
        setError(errorMessage, QStringLiteral("Theme storage rollback was incomplete."));
    }
    return restored;
}

inline void removeUnlistedThemeDocuments(
    const std::filesystem::path& directory,
    const std::vector<ThemeDocument>& documents,
    const ThemeFormat requestedFormat = ThemeFormat::unknown)
{
    QSet<QString> keepFiles{};
    QSet<ThemeFormat> formats{};
    for (const ThemeDocument& document : documents)
    {
        const ThemeFormat format = document.format == ThemeFormat::unknown
            ? themeFormatFromObject(document.object) : document.format;
        formats.insert(format);
        keepFiles.insert(QFileInfo(
            themeFilePathForDocument(directory, document)).fileName().toCaseFolded());
    }
    if (requestedFormat != ThemeFormat::unknown)
    {
        formats.clear();
        formats.insert(requestedFormat);
    }
    const QString directoryName = QString::fromStdWString(directory.wstring());
    const QDir themeDirectory(directoryName);
    const QFileInfoList entries = themeDirectory.entryInfoList(
        QDir::Files | QDir::NoDotAndDotDot, QDir::Name | QDir::IgnoreCase);
    for (const QFileInfo& entry : entries)
    {
        if (entry.suffix().compare(QStringLiteral("json"), Qt::CaseInsensitive) != 0
            || entry.isSymLink() || entry.size() < 0
            || entry.size() > kMaximumThemeFileBytes)
            continue;
        QFile file(entry.filePath());
        if (!file.open(QIODevice::ReadOnly)) continue;
        const QJsonDocument json = QJsonDocument::fromJson(file.readAll());
        if (!json.isObject()) continue;
        const QJsonObject object = json.object();
        const ThemeFormat fileFormat = themeFormatFromObject(object);
        const QString id = object.value(QStringLiteral("id")).toString();
        const QString name = object.value(QStringLiteral("name")).toString().trimmed();
        if (fileFormat == ThemeFormat::unknown || !formats.contains(fileFormat)
            || object.value(QStringLiteral("schema")).toInt(-1) != 1
            || !isSafeThemeId(id) || name.isEmpty() || name.size() > 128
            || !themeColorPayloadIsValid(object, fileFormat)
            || entry.fileName().compare(themeFileNameForLabel(name, fileFormat),
                Qt::CaseInsensitive) != 0)
            continue;
        if (!keepFiles.contains(entry.fileName().toCaseFolded()))
        {
            (void)QFile::remove(entry.filePath());
        }
    }
}

inline bool removeThemeDocumentByIdFromDirectories(
    const std::vector<std::filesystem::path>& directories,
    const QString& requestedId,
    const ThemeFormat requestedFormat,
    QString* errorMessage = nullptr)
{
    if (errorMessage != nullptr)
    {
        errorMessage->clear();
    }
    if (!isSafeThemeId(requestedId) || requestedFormat == ThemeFormat::unknown)
    {
        setError(errorMessage, QStringLiteral("Theme deletion has an invalid id or format."));
        return false;
    }

    QSet<QString> visitedFiles{};
    for (const std::filesystem::path& directory : directories)
    {
        if (directory.empty())
        {
            continue;
        }
        const QString directoryName = QString::fromStdWString(directory.wstring());
        const QFileInfo directoryInfo(directoryName);
        if (!directoryInfo.exists() || !directoryInfo.isDir())
        {
            continue;
        }
        const QDir themeDirectory(directoryName);
        const QFileInfoList entries = themeDirectory.entryInfoList(
            QDir::Files | QDir::NoDotAndDotDot, QDir::Name | QDir::IgnoreCase);
        for (const QFileInfo& entry : entries)
        {
            if (entry.suffix().compare(QStringLiteral("json"), Qt::CaseInsensitive) != 0
                || entry.isSymLink() || !entry.isFile() || entry.size() < 0
                || entry.size() > kMaximumThemeFileBytes)
            {
                continue;
            }
            const QString identity = entry.absoluteFilePath().toCaseFolded();
            if (visitedFiles.contains(identity))
            {
                continue;
            }
            visitedFiles.insert(identity);

            QFile file(entry.filePath());
            if (!file.open(QIODevice::ReadOnly))
            {
                continue;
            }
            const QByteArray bytes = file.read(kMaximumThemeFileBytes + 1);
            const bool readFailed = file.error() != QFileDevice::NoError
                || bytes.size() > kMaximumThemeFileBytes;
            file.close();
            if (readFailed)
            {
                continue;
            }
            QJsonParseError parseError{};
            const QJsonDocument json = QJsonDocument::fromJson(bytes, &parseError);
            if (parseError.error != QJsonParseError::NoError || !json.isObject())
            {
                continue;
            }
            const QJsonObject object = json.object();
            const QString storedId = object.value(QStringLiteral("id")).toString();
            if (themeFormatFromObject(object) != requestedFormat
                || storedId.compare(requestedId, Qt::CaseInsensitive) != 0)
            {
                continue;
            }

            const QString name = object.value(QStringLiteral("name")).toString().trimmed();
            const bool validTarget = object.value(QStringLiteral("schema")).toInt(-1) == 1
                && isSafeThemeId(storedId)
                && !name.isEmpty() && name.size() <= 128
                && themeColorPayloadIsValid(object, requestedFormat)
                && entry.fileName().compare(
                    themeFileNameForLabel(name, requestedFormat),
                    Qt::CaseInsensitive) == 0;
            if (!validTarget)
            {
                continue;
            }
            if (!QFile::remove(entry.filePath()))
            {
                setError(errorMessage, QStringLiteral(
                    "A matching theme file could not be removed."));
                return false;
            }
        }
    }
    return true;
}

inline std::vector<ThemeDocument> readThemeDocuments(
    const std::filesystem::path& directory,
    const ThemeFormat expectedFormat,
    QString* errorMessage = nullptr)
{
    if (errorMessage != nullptr) errorMessage->clear();
    const QString directoryName = QString::fromStdWString(directory.wstring());
    const QFileInfo directoryInfo(directoryName);
    if (!directoryInfo.exists()) return {};
    if (!directoryInfo.isDir())
    {
        setError(errorMessage, QStringLiteral("Theme storage path is not a directory."));
        return {};
    }
    const QDir themeDirectory(directoryName);
    const QFileInfoList entries = themeDirectory.entryInfoList(
        QDir::Files | QDir::NoDotAndDotDot, QDir::Name | QDir::IgnoreCase);
    std::vector<ThemeDocument> documents{};
    QSet<QString> seenIds{};
    int jsonFileCount = 0;
    for (const QFileInfo& entry : entries)
    {
        if (entry.suffix().compare(QStringLiteral("json"), Qt::CaseInsensitive) != 0)
            continue;
        if (++jsonFileCount > kMaximumThemeFiles)
        {
            setError(errorMessage, QStringLiteral("Theme storage contains too many files."));
            break;
        }
        if (entry.isSymLink() || entry.size() < 0
            || entry.size() > kMaximumThemeFileBytes)
        {
            setError(errorMessage, QStringLiteral("A theme file is unsafe or exceeds the size limit and was ignored."));
            continue;
        }
        QFile file(entry.filePath());
        if (!file.open(QIODevice::ReadOnly))
        {
            setError(errorMessage, QStringLiteral("A theme file could not be opened."));
            continue;
        }
        const QByteArray bytes = file.read(kMaximumThemeFileBytes + 1);
        if (file.error() != QFileDevice::NoError || bytes.size() > kMaximumThemeFileBytes)
        {
            setError(errorMessage, QStringLiteral("A theme file could not be read within the size limit."));
            continue;
        }
        QJsonParseError parseError{};
        const QJsonDocument json = QJsonDocument::fromJson(bytes, &parseError);
        if (parseError.error != QJsonParseError::NoError || !json.isObject())
        {
            setError(errorMessage, QStringLiteral("A malformed theme JSON file was ignored."));
            continue;
        }
        const QJsonObject object = json.object();
        const ThemeFormat format = themeFormatFromObject(object);
        if (format == ThemeFormat::unknown
            || (expectedFormat != ThemeFormat::unknown && format != expectedFormat))
        {
            // Untagged, future, and other-editor files are left to their
            // owning format and never treated as this editor's themes.
            continue;
        }
        const QJsonValue idValue = object.value(QStringLiteral("id"));
        const QJsonValue nameValue = object.value(QStringLiteral("name"));
        if (object.value(QStringLiteral("schema")).toInt(-1) != 1
            || !idValue.isString() || !nameValue.isString()
            || !themeColorPayloadIsValid(object, format))
        {
            setError(errorMessage, QStringLiteral("A theme definition has invalid schema, id, or name fields."));
            continue;
        }
        const QString id = idValue.toString();
        const QString name = nameValue.toString().trimmed();
        if (!isSafeThemeId(id) || name.isEmpty() || name.size() > 128)
        {
            setError(errorMessage, QStringLiteral("A theme definition has an unsafe id or name."));
            continue;
        }
        if (entry.fileName().compare(themeFileNameForLabel(name, format),
                Qt::CaseInsensitive) != 0)
        {
            setError(errorMessage, QStringLiteral("A theme filename does not match its definition name."));
            continue;
        }
        const QString foldedId = typedThemeIdentity(id, format);
        if (seenIds.contains(foldedId))
        {
            setError(errorMessage, QStringLiteral("Theme storage contains duplicate ids."));
            continue;
        }
        seenIds.insert(foldedId);
        documents.push_back({ id, object, format });
    }
    return documents;
}

inline std::vector<ThemeDocument> readThemeDocuments(
    const std::filesystem::path& directory,
    QString* errorMessage = nullptr)
{
    return readThemeDocuments(directory, ThemeFormat::unknown, errorMessage);
}

inline std::vector<ThemeDocument> readThemeDocumentsFromDirectories(
    const std::vector<std::filesystem::path>& directories,
    const ThemeFormat expectedFormat,
    QString* errorMessage = nullptr)
{
    if (errorMessage != nullptr)
    {
        errorMessage->clear();
    }
    std::vector<ThemeDocument> merged{};
    QSet<QString> seenIds{};
    QSet<QString> seenFileNames{};
    for (const std::filesystem::path& directory : directories)
    {
        QString directoryError{};
        const std::vector<ThemeDocument> documents =
            readThemeDocuments(directory, expectedFormat, &directoryError);
        if (errorMessage != nullptr && errorMessage->isEmpty()
            && !directoryError.isEmpty())
        {
            *errorMessage = directoryError;
        }
        for (const ThemeDocument& document : documents)
        {
            const QString foldedId = typedThemeIdentity(
                document.id, document.format);
            const QString displayName = document.object.value(
                QStringLiteral("name")).toString();
            const QString foldedFileName = themeFileNameForLabel(
                displayName, document.format).toCaseFolded();
            if (!seenIds.contains(foldedId)
                && !seenFileNames.contains(foldedFileName))
            {
                seenIds.insert(foldedId);
                seenFileNames.insert(foldedFileName);
                merged.push_back(document);
            }
        }
    }
    return merged;
}

inline std::vector<ThemeDocument> readThemeDocumentsFromDirectories(
    const std::vector<std::filesystem::path>& directories,
    QString* errorMessage = nullptr)
{
    return readThemeDocumentsFromDirectories(
        directories, ThemeFormat::unknown, errorMessage);
}

inline bool preflightThemeDocuments(
    const std::filesystem::path& directory,
    const std::vector<ThemeDocument>& documents,
    QString* errorMessage = nullptr,
    const ThemeFormat requestedFormat = ThemeFormat::unknown)
{
    if (errorMessage != nullptr)
    {
        errorMessage->clear();
    }
    if (directory.empty())
    {
        setError(errorMessage, QStringLiteral("Theme storage directory is unavailable."));
        return false;
    }
    if (documents.size() > static_cast<std::size_t>(kMaximumThemeFiles))
    {
        setError(errorMessage, QStringLiteral("Too many custom themes were requested."));
        return false;
    }

    QSet<QString> seenIds{};
    QSet<QString> seenFiles{};
    for (const ThemeDocument& document : documents)
    {
        const ThemeFormat format = document.format == ThemeFormat::unknown
            ? themeFormatFromObject(document.object)
            : document.format;
        const QString name = document.object.value(QStringLiteral("name")).toString().trimmed();
        if (!isSafeThemeId(document.id)
            || document.object.value(QStringLiteral("id")).toString() != document.id
            || format == ThemeFormat::unknown
            || (requestedFormat != ThemeFormat::unknown && format != requestedFormat)
            || name.isEmpty() || name.size() > 128)
        {
            setError(errorMessage, QStringLiteral("A custom theme has an invalid id, format, or name."));
            return false;
        }
        const QString foldedId = typedThemeIdentity(document.id, format);
        if (seenIds.contains(foldedId))
        {
            setError(errorMessage, QStringLiteral("Custom themes contain duplicate ids."));
            return false;
        }
        seenIds.insert(foldedId);
        const QString path = themeFilePathForDocument(directory, document);
        const QString foldedFile = QFileInfo(path).fileName().toCaseFolded();
        if (seenFiles.contains(foldedFile))
        {
            setError(errorMessage, QStringLiteral("Two custom themes use the same visible name."));
            return false;
        }
        seenFiles.insert(foldedFile);
        if (QJsonDocument(document.object).toJson(QJsonDocument::Compact).size()
            > kMaximumThemeFileBytes)
        {
            setError(errorMessage, QStringLiteral("A custom theme exceeds the size limit."));
            return false;
        }

        const QFileInfo existing(path);
        if (!existing.exists() && !existing.isSymLink())
        {
            continue;
        }
        if (existing.isSymLink() || !existing.isFile() || existing.size() < 0
            || existing.size() > kMaximumThemeFileBytes)
        {
            setError(errorMessage, QStringLiteral("An existing theme path cannot be replaced safely."));
            return false;
        }
        QFile existingFile(path);
        if (!existingFile.open(QIODevice::ReadOnly))
        {
            setError(errorMessage, QStringLiteral("An existing theme file could not be inspected safely."));
            return false;
        }
        const QByteArray bytes = existingFile.read(kMaximumThemeFileBytes + 1);
        QJsonParseError parseError{};
        const QJsonDocument json = QJsonDocument::fromJson(bytes, &parseError);
        if (existingFile.error() != QFileDevice::NoError
            || bytes.size() > kMaximumThemeFileBytes
            || parseError.error != QJsonParseError::NoError
            || !json.isObject())
        {
            setError(errorMessage, QStringLiteral("A malformed file occupies the target theme name and was left unchanged."));
            return false;
        }
        const QJsonObject object = json.object();
        if (themeFormatFromObject(object) != format
            || object.value(QStringLiteral("schema")).toInt(-1) != 1
            || object.value(QStringLiteral("id")).toString() != document.id
            || !themeColorPayloadIsValid(object, format))
        {
            setError(errorMessage, QStringLiteral("A theme with this name already exists in another format or with another id."));
            return false;
        }
    }

    const QString directoryName = QString::fromStdWString(directory.wstring());
    if (!QDir().mkpath(directoryName))
    {
        setError(errorMessage, QStringLiteral("Theme storage directory could not be created."));
        return false;
    }
    const QString probePath = QDir(directoryName).filePath(
        QStringLiteral(".preflight-%1.tmp")
            .arg(QUuid::createUuid().toString(QUuid::WithoutBraces)));
    QSaveFile probe(probePath);
    probe.setDirectWriteFallback(false);
    const bool writable = probe.open(QIODevice::WriteOnly)
        && probe.write("SED", 3) == 3
        && probe.commit();
    if (probe.isOpen())
    {
        probe.cancelWriting();
    }
    (void)QFile::remove(probePath);
    if (!writable)
    {
        setError(errorMessage, QStringLiteral("Theme storage cannot commit atomic files."));
        return false;
    }
    return true;
}

inline bool saveThemeDocumentsImpl(
    const std::filesystem::path& directory,
    const std::vector<ThemeDocument>& documents,
    QString* errorMessage,
    const int testFailureAfterCommittedPayloads,
    const bool testFailureBeforeCleanup,
    const bool deferStaleCleanup,
    const ThemeFormat requestedFormat)
{
    if (errorMessage != nullptr)
    {
        errorMessage->clear();
    }
    std::vector<ThemeDocument> normalizedDocuments{};
    normalizedDocuments.reserve(documents.size());
    for (const ThemeDocument& document : documents)
    {
        ThemeDocument normalized = document;
        normalized.format = normalized.format == ThemeFormat::unknown
            ? themeFormatFromObject(normalized.object)
            : normalized.format;
        if (normalized.format == ThemeFormat::unknown)
        {
            setError(errorMessage, QStringLiteral("A custom theme has an unknown file format."));
            return false;
        }
        normalized.object.insert(QStringLiteral("format"),
            themeFormatTag(normalized.format));
        normalizedDocuments.push_back(std::move(normalized));
    }
    if (!preflightThemeDocuments(
            directory, normalizedDocuments, errorMessage, requestedFormat))
    {
        return false;
    }

    struct ExistingFile
    {
        QString path{};
        bool existed{};
        QByteArray bytes{};
    };
    std::vector<ExistingFile> originals{};
    originals.reserve(normalizedDocuments.size());
    for (const ThemeDocument& document : normalizedDocuments)
    {
        ExistingFile original{};
        original.path = themeFilePathForDocument(directory, document);
        const QFileInfo info(original.path);
        if (info.exists() || info.isSymLink())
        {
            if (info.isSymLink() || !info.isFile() || info.size() < 0
                || info.size() > kMaximumThemeFileBytes)
            {
                setError(errorMessage, QStringLiteral("An existing theme file cannot be backed up safely."));
                return false;
            }
            QFile oldFile(original.path);
            if (!oldFile.open(QIODevice::ReadOnly))
            {
                setError(errorMessage, QStringLiteral("An existing theme file could not be backed up."));
                return false;
            }
            original.bytes = oldFile.read(kMaximumThemeFileBytes + 1);
            if (oldFile.error() != QFileDevice::NoError
                || original.bytes.size() > kMaximumThemeFileBytes)
            {
                setError(errorMessage, QStringLiteral("An existing theme file could not be backed up."));
                return false;
            }
            original.existed = true;
        }
        originals.push_back(std::move(original));
    }

    const auto restoreCommittedFiles = [&originals](const std::size_t committedCount)
    {
        bool restored = true;
        for (std::size_t index = 0; index < committedCount; ++index)
        {
            const ExistingFile& original = originals[index];
            if (!original.existed)
            {
                const bool thisRestored = QFile::remove(original.path)
                    || !QFileInfo::exists(original.path);
                restored = thisRestored && restored;
                continue;
            }
            QSaveFile restore(original.path);
            restore.setDirectWriteFallback(false);
            const bool thisRestored = restore.open(QIODevice::WriteOnly)
                && restore.write(original.bytes) == original.bytes.size()
                && restore.commit();
            if (restore.isOpen())
            {
                restore.cancelWriting();
            }
            restored = thisRestored && restored;
        }
        return restored;
    };

    std::size_t committedCount = 0;
    for (std::size_t index = 0; index < normalizedDocuments.size(); ++index)
    {
        const ThemeDocument& document = normalizedDocuments[index];
        QSaveFile file(originals[index].path);
        file.setDirectWriteFallback(false);
        const QByteArray bytes = QJsonDocument(document.object)
                                     .toJson(QJsonDocument::Indented);
        if (!file.open(QIODevice::WriteOnly)
            || bytes.size() > kMaximumThemeFileBytes
            || file.write(bytes) != bytes.size()
            || !file.commit())
        {
            file.cancelWriting();
            const bool restored = restoreCommittedFiles(committedCount);
            setError(errorMessage, restored
                ? QStringLiteral("A custom theme file could not be committed atomically.")
                : QStringLiteral("A custom theme write failed and rollback was incomplete."));
            return false;
        }
        ++committedCount;
        if (testFailureAfterCommittedPayloads >= 0
            && committedCount == static_cast<std::size_t>(
                testFailureAfterCommittedPayloads))
        {
            const bool restored = restoreCommittedFiles(committedCount);
            setError(errorMessage, restored
                ? QStringLiteral("An injected theme write failure was rolled back.")
                : QStringLiteral("An injected theme write failure could not be fully rolled back."));
            return false;
        }
    }

    if (testFailureBeforeCleanup)
    {
        const bool restored = restoreCommittedFiles(committedCount);
        setError(errorMessage, restored
            ? QStringLiteral("An injected pre-cleanup failure was rolled back.")
            : QStringLiteral("An injected pre-cleanup failure could not be fully rolled back."));
        return false;
    }

    if (!deferStaleCleanup)
    {
        removeUnlistedThemeDocuments(
            directory, normalizedDocuments, requestedFormat);
    }
    return true;
}

inline bool saveThemeDocuments(
    const std::filesystem::path& directory,
    const std::vector<ThemeDocument>& documents,
    QString* errorMessage = nullptr,
    const ThemeFormat format = ThemeFormat::unknown)
{
    return saveThemeDocumentsImpl(
        directory, documents, errorMessage, -1, false, false, format);
}

inline bool saveThemeDocumentsDeferredCleanup(
    const std::filesystem::path& directory,
    const std::vector<ThemeDocument>& documents,
    QString* errorMessage = nullptr,
    const ThemeFormat format = ThemeFormat::unknown)
{
    return saveThemeDocumentsImpl(
        directory, documents, errorMessage, -1, false, true, format);
}

#ifdef SED_APP_THEME_STORAGE_TEST_ACCESS
inline bool saveThemeDocumentsFailingAfterPayloadsForTest(
    const std::filesystem::path& directory,
    const std::vector<ThemeDocument>& documents,
    int committedPayloadCount,
    QString* errorMessage = nullptr)
{
    return saveThemeDocumentsImpl(
        directory, documents, errorMessage, committedPayloadCount, false, false,
        ThemeFormat::unknown);
}

inline bool saveThemeDocumentsFailingBeforeCleanupForTest(
    const std::filesystem::path& directory,
    const std::vector<ThemeDocument>& documents,
    QString* errorMessage = nullptr)
{
    return saveThemeDocumentsImpl(
        directory, documents, errorMessage, -1, true, false,
        ThemeFormat::unknown);
}
#endif

} // namespace sed::ui::theme_storage
