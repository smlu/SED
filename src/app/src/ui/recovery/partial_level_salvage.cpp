#include "ui/recovery/partial_level_salvage.hpp"

#include <algorithm>
#include <array>
#include <exception>
#include <optional>
#include <streambuf>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>

#include <QByteArray>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryFile>
#include <QUuid>

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#include <io.h>
#else
#include <cerrno>
#include <cstring>
#include <unistd.h>
#endif

#include "sed/assets/jkl.hpp"
#include "sed/assets/material_dimensions.hpp"
#include "sed/assets/resource_locator.hpp"
#include "sed/assets/sed.hpp"
#include "sed/level/uv.hpp"

namespace sed::ui::recovery
{

namespace detail
{

QString escapeForensicText(
    const QString& source,
    const qsizetype maximumCharacters,
    const ForensicNewlinePolicy newlinePolicy,
    bool* truncated)
{
    if (truncated) *truncated = false;
    if (maximumCharacters <= 0)
    {
        if (truncated) *truncated = !source.isEmpty();
        return {};
    }

    QString escaped{};
    escaped.reserve(std::min(source.size(), maximumCharacters));
    const auto append = [&](const QStringView value) -> bool
    {
        if (value.size() > maximumCharacters - escaped.size())
        {
            if (truncated) *truncated = true;
            return false;
        }
        escaped += value;
        return true;
    };
    const auto appendEscape = [&](const ushort code) -> bool
    {
        const QString value = QStringLiteral("\\u")
            + QString::number(code, 16).rightJustified(
                4, QLatin1Char('0')).toUpper();
        return append(value);
    };

    for (qsizetype index = 0; index < source.size(); ++index)
    {
        const QChar character = source.at(index);
        const ushort code = character.unicode();
        if (character == QLatin1Char('\r'))
        {
            if (newlinePolicy == ForensicNewlinePolicy::replaceWithSpace
                && !append(QStringView(u" ")))
            {
                break;
            }
            continue;
        }
        if (character == QLatin1Char('\n'))
        {
            const QStringView replacement = newlinePolicy
                    == ForensicNewlinePolicy::replaceWithSpace
                ? QStringView(u" ")
                : QStringView(u"\n    ");
            if (!append(replacement)) break;
            continue;
        }

        const bool unsafe = code <= 0x001fu
            || (code >= 0x007fu && code <= 0x009fu)
            || code == 0x2028u || code == 0x2029u
            || character.category() == QChar::Other_Format;
        if (unsafe)
        {
            if (!appendEscape(code)) break;
            continue;
        }

        if (character.isHighSurrogate())
        {
            if (index + 1 < source.size()
                && source.at(index + 1).isLowSurrogate())
            {
                const QStringView pair(source.constData() + index, 2);
                if (!append(pair)) break;
                ++index;
            }
            else if (!appendEscape(code))
            {
                break;
            }
            continue;
        }
        if (character.isLowSurrogate())
        {
            if (!appendEscape(code)) break;
            continue;
        }
        const QStringView value(source.constData() + index, 1);
        if (!append(value)) break;
    }
    return escaped;
}

} // namespace detail

namespace
{

constexpr std::array<std::string_view, 8> kRequiredLevelSections{
    "head", "rsrc", "sect", "thng", "tmpl", "cogs", "lite", "hnod"
};
constexpr std::size_t kMaximumNativeTextBytes = assets::SedMaximumFileBytes;
constexpr qsizetype kMaximumManifestCharacters = 2 * 1024 * 1024;
constexpr qsizetype kMaximumManifestListCharacters = 1024 * 1024;
constexpr qsizetype kMaximumManifestListEntries = 256;
constexpr qsizetype kMaximumManifestFieldCharacters = 4096;

QString boundedManifestField(const QString& source)
{
    bool truncated = false;
    QString value = detail::escapeForensicText(
        source,
        kMaximumManifestFieldCharacters,
        detail::ForensicNewlinePolicy::replaceWithSpace,
        &truncated);
    if (!truncated) return value;

    const QString suffix = QStringLiteral(" ... [truncated]");
    value.truncate(kMaximumManifestFieldCharacters - suffix.size());
    value += suffix;
    return value;
}

QString pathText(const std::filesystem::path& path)
{
    if (path.empty()) return QStringLiteral("(not available)");
    std::error_code error{};
    const std::filesystem::path absolute = std::filesystem::absolute(path, error);
    const std::filesystem::path& display = error ? path : absolute;
    return boundedManifestField(
        QDir::toNativeSeparators(QString::fromStdWString(display.wstring())));
}

QString projectName(const ProjectType project)
{
    switch (project)
    {
        case ProjectType::JKDF2: return QStringLiteral("JKDF2");
        case ProjectType::MOTS: return QStringLiteral("MotS");
        case ProjectType::IJIM: return QStringLiteral("IJIM");
    }
    return QStringLiteral("Unknown (%1)").arg(static_cast<int>(project));
}

QString nativeExtension(const ProjectType project)
{
    switch (project)
    {
        case ProjectType::JKDF2:
        case ProjectType::MOTS:
            return QStringLiteral("jkl");
        case ProjectType::IJIM:
            return QStringLiteral("ndy");
    }
    return {};
}

QString sanitizedStem(QString value)
{
    value = value.trimmed();
    if (value.isEmpty()) value = QStringLiteral("recovered-level");
    for (qsizetype index = 0; index < value.size(); ++index)
    {
        const QChar character = value.at(index);
        if (!character.isLetterOrNumber()
            && character != QLatin1Char('.')
            && character != QLatin1Char('-')
            && character != QLatin1Char('_'))
        {
            value[index] = QLatin1Char('_');
        }
    }
    constexpr qsizetype maximumStemCharacters = 80;
    if (value.size() > maximumStemCharacters)
    {
        value.truncate(maximumStemCharacters);
    }
    return value;
}

QString generatedArtifactId()
{
    return QDateTime::currentDateTimeUtc().toString(QStringLiteral("yyyyMMdd-HHmmsszzz"))
        + QLatin1Char('-')
        + QUuid::createUuid().toString(QUuid::WithoutBraces);
}

struct ArtifactPaths
{
    std::filesystem::path manifest{};
    std::filesystem::path nativeText{};
};

ArtifactPaths chooseArtifactPaths(
    const std::filesystem::path& directory,
    const QString& sourceStem,
    const QString& extension,
    const QString& artifactId)
{
    const QString base = sanitizedStem(sourceStem)
        + QStringLiteral(".partial-level-salvage-")
        + sanitizedStem(artifactId.isEmpty() ? generatedArtifactId() : artifactId);
    for (int collision = 0; collision < 10'000; ++collision)
    {
        const QString suffix = collision == 0
            ? QString{}
            : QStringLiteral("-%1").arg(collision + 1);
        const std::filesystem::path stem((base + suffix).toStdWString());
        ArtifactPaths paths{};
        paths.manifest = directory / stem;
        paths.manifest += L".txt";
        if (!extension.isEmpty())
        {
            paths.nativeText = directory / stem;
            paths.nativeText += std::filesystem::path(
                (QStringLiteral(".") + extension).toStdWString());
        }
        std::error_code manifestError{};
        const bool manifestExists = std::filesystem::exists(paths.manifest, manifestError);
        std::error_code nativeError{};
        const bool nativeExists = !paths.nativeText.empty()
            && std::filesystem::exists(paths.nativeText, nativeError);
        if (!manifestError && !nativeError && !manifestExists && !nativeExists)
        {
            return paths;
        }
    }
    return {};
}

class BoundedFileStreamBuffer final : public std::streambuf
{
public:
    BoundedFileStreamBuffer(QTemporaryFile& file, const std::size_t maximumBytes)
        : file_(file), maximumBytes_(maximumBytes)
    {
    }

    bool failed() const noexcept { return !error_.isEmpty(); }
    const QString& error() const noexcept { return error_; }

protected:
    std::streamsize xsputn(const char* data, const std::streamsize count) override
    {
        if (count <= 0) return count;
        const std::size_t requested = static_cast<std::size_t>(count);
        if (requested > maximumBytes_ - bytesWritten_)
        {
            error_ = QStringLiteral("the native text export exceeds the 256 MiB safety limit");
            return 0;
        }

        std::size_t cursor = 0;
        while (cursor < requested)
        {
            const qint64 amount = file_.write(
                data + cursor, static_cast<qint64>(requested - cursor));
            if (amount <= 0)
            {
                error_ = QStringLiteral("staging write failed: %1")
                    .arg(file_.errorString());
                return static_cast<std::streamsize>(cursor);
            }
            cursor += static_cast<std::size_t>(amount);
            bytesWritten_ += static_cast<std::size_t>(amount);
        }
        return count;
    }

    int_type overflow(const int_type character) override
    {
        if (traits_type::eq_int_type(character, traits_type::eof()))
            return traits_type::not_eof(character);
        const char value = traits_type::to_char_type(character);
        return xsputn(&value, 1) == 1
            ? character
            : traits_type::eof();
    }

    int sync() override
    {
        if (file_.flush()) return 0;
        error_ = QStringLiteral("staging flush failed: %1").arg(file_.errorString());
        return -1;
    }

private:
    QTemporaryFile& file_;
    std::size_t maximumBytes_{};
    std::size_t bytesWritten_{};
    QString error_{};
};

QString stagingTemplateFor(const std::filesystem::path& target)
{
    const QFileInfo targetInfo(QString::fromStdWString(target.wstring()));
    return targetInfo.absolutePath()
        + QStringLiteral("/.")
        + targetInfo.fileName()
        + QStringLiteral(".stage-XXXXXX");
}

bool openStagingFile(
    QTemporaryFile& file,
    const std::filesystem::path& target,
    QString* error)
{
    file.setAutoRemove(true);
    if (file.open()) return true;
    if (error)
    {
        *error = QStringLiteral("Could not securely stage salvage artifact %1: %2")
            .arg(pathText(target), file.errorString());
    }
    return false;
}

bool flushStagingFileToDisk(QTemporaryFile& file, QString* error)
{
    if (!file.flush())
    {
        if (error)
        {
            *error = QStringLiteral("Could not flush staged salvage data: %1")
                .arg(file.errorString());
        }
        return false;
    }
#ifdef Q_OS_WIN
    const int fileDescriptor = file.handle();
    const intptr_t nativeHandle = fileDescriptor < 0
        ? -1
        : _get_osfhandle(fileDescriptor);
    if (nativeHandle == -1
        || !FlushFileBuffers(reinterpret_cast<HANDLE>(nativeHandle)))
    {
        if (error)
        {
            *error = QStringLiteral("Could not durably flush staged salvage data (Windows error %1).")
                .arg(static_cast<qulonglong>(GetLastError()));
        }
        return false;
    }
#else
    if (::fsync(static_cast<int>(file.handle())) != 0)
    {
        if (error)
        {
            *error = QStringLiteral("Could not durably flush staged salvage data: %1")
                .arg(QString::fromLocal8Bit(std::strerror(errno)));
        }
        return false;
    }
#endif
    return true;
}

#ifdef Q_OS_WIN
QString extendedWin32Path(const QString& path)
{
    QString absolute = QDir::toNativeSeparators(
        QFileInfo(path).absoluteFilePath());
    if (absolute.startsWith(QStringLiteral("\\\\?\\")))
    {
        return absolute;
    }
    if (absolute.startsWith(QStringLiteral("\\\\")))
    {
        return QStringLiteral("\\\\?\\UNC\\") + absolute.mid(2);
    }
    return QStringLiteral("\\\\?\\") + absolute;
}
#endif

bool publishStagedFileNoReplace(
    QTemporaryFile& staged,
    const std::filesystem::path& target,
    const std::function<void(const std::filesystem::path&)>& beforePublish,
    QString* error)
{
    if (!flushStagingFileToDisk(staged, error)) return false;
    const QString stagedName = staged.fileName();
    staged.close();

    if (beforePublish)
    {
        try
        {
            beforePublish(target);
        }
        catch (const std::exception& exception)
        {
            if (error)
            {
                *error = QStringLiteral("The salvage publication hook failed safely: %1")
                    .arg(QString::fromUtf8(exception.what()));
            }
            return false;
        }
        catch (...)
        {
            if (error)
            {
                *error = QStringLiteral(
                    "The salvage publication hook failed safely after an unknown error.");
            }
            return false;
        }
    }

    const QString targetName = QString::fromStdWString(target.wstring());
#ifdef Q_OS_WIN
    const QString nativeTargetName = extendedWin32Path(targetName);
    const QString nativeStagedName = extendedWin32Path(stagedName);
    if (CreateHardLinkW(
            reinterpret_cast<LPCWSTR>(nativeTargetName.utf16()),
            reinterpret_cast<LPCWSTR>(nativeStagedName.utf16()),
            nullptr))
    {
        (void)QFile::remove(stagedName);
        return true;
    }

    const DWORD hardLinkError = GetLastError();
    if (hardLinkError == ERROR_FILE_EXISTS
        || hardLinkError == ERROR_ALREADY_EXISTS)
    {
        if (error)
        {
            *error = QStringLiteral(
                "Could not atomically publish %1 without overwriting another file (Windows error %2).")
                .arg(pathText(target))
                .arg(static_cast<qulonglong>(hardLinkError));
        }
        return false;
    }

    const bool hardLinksUnsupported = hardLinkError == ERROR_NOT_SUPPORTED
        || hardLinkError == ERROR_INVALID_FUNCTION
        || hardLinkError == ERROR_INVALID_PARAMETER;
    if (!hardLinksUnsupported
        || !MoveFileExW(
            reinterpret_cast<LPCWSTR>(nativeStagedName.utf16()),
            reinterpret_cast<LPCWSTR>(nativeTargetName.utf16()),
            MOVEFILE_WRITE_THROUGH))
    {
        const DWORD publishError = hardLinksUnsupported
            ? GetLastError()
            : hardLinkError;
        if (error)
        {
            *error = QStringLiteral(
                "Could not atomically publish %1 without overwriting another file (Windows error %2).")
                .arg(pathText(target))
                .arg(static_cast<qulonglong>(publishError));
        }
        return false;
    }
#else
    const QByteArray stagedBytes = QFile::encodeName(stagedName);
    const QByteArray targetBytes = QFile::encodeName(targetName);
    if (::link(stagedBytes.constData(), targetBytes.constData()) != 0)
    {
        if (error)
        {
            *error = QStringLiteral(
                "Could not atomically publish %1 without overwriting another file: %2")
                .arg(pathText(target), QString::fromLocal8Bit(std::strerror(errno)));
        }
        return false;
    }
    (void)QFile::remove(stagedName);
#endif
    return true;
}

bool writeBytesNoReplace(
    const std::filesystem::path& target,
    const QByteArray& bytes,
    const std::function<void(const std::filesystem::path&)>& beforePublish,
    QString* error)
{
    QTemporaryFile staged(stagingTemplateFor(target));
    if (!openStagingFile(staged, target, error)) return false;
    qsizetype cursor = 0;
    while (cursor < bytes.size())
    {
        const qint64 amount = staged.write(
            bytes.constData() + cursor, bytes.size() - cursor);
        if (amount <= 0)
        {
            if (error)
            {
                *error = QStringLiteral("Could not write staged salvage artifact %1: %2")
                    .arg(pathText(target), staged.errorString());
            }
            return false;
        }
        cursor += static_cast<qsizetype>(amount);
    }
    return publishStagedFileNoReplace(staged, target, beforePublish, error);
}

QStringList normalizeMissingSections(const QStringList& sections)
{
    QStringList normalized{};
    for (const std::string_view required : kRequiredLevelSections)
    {
        const QString name = QString::fromLatin1(required.data(),
            static_cast<qsizetype>(required.size()));
        const bool present = std::any_of(
            sections.cbegin(), sections.cend(), [&name](const QString& value)
            {
                return value.trimmed().compare(name, Qt::CaseInsensitive) == 0;
            });
        if (present) normalized.push_back(name);
    }
    return normalized;
}

bool streamLevelToStagingFile(
    const Level& level,
    QTemporaryFile& staged,
    const PartialLevelNativeExporter& exporter,
    const std::shared_ptr<const assets::ResourceLocator>& resources,
    QString* error)
{
    try
    {
        BoundedFileStreamBuffer buffer(staged, kMaximumNativeTextBytes);
        std::ostream stream(&buffer);
        if (exporter)
        {
            exporter(level, stream);
        }
        else
        {
            const UvCoordinateSpace nativeUvCoordinateSpace =
                level.kind == ProjectType::IJIM
                ? UvCoordinateSpace::Normalized
                : UvCoordinateSpace::TexturePixels;
            std::optional<Level> convertedLevel{};
            const Level* levelToWrite = &level;
            if (level.uvCoordinateSpace != nativeUvCoordinateSpace)
            {
                convertedLevel = level;
                std::unordered_map<std::string,
                    std::optional<level::UvTextureDimensions>> dimensions{};
                const level::LevelUvConversionResult converted =
                    level::convertLevelUvCoordinateSpace(
                        *convertedLevel,
                        nativeUvCoordinateSpace,
                        [&resources, &dimensions](const Surface& surface)
                            -> std::optional<level::UvTextureDimensions>
                        {
                            if (const auto found = dimensions.find(surface.material);
                                found != dimensions.end())
                            {
                                return found->second;
                            }
                            const std::optional<level::UvTextureDimensions> resolved =
                                resources
                                ? assets::resolveMaterialTextureDimensions(
                                    *resources, surface.material)
                                : std::nullopt;
                            dimensions.emplace(surface.material, resolved);
                            return resolved;
                        });
                if (!converted)
                {
                    if (error)
                    {
                        QString material{};
                        QString location{};
                        if (converted.failedSurface)
                        {
                            location = QStringLiteral(" at sector %1, surface %2")
                                .arg(converted.failedSurface->sector)
                                .arg(converted.failedSurface->surface);
                            const std::int32_t sector =
                                converted.failedSurface->sector;
                            const std::int32_t surface =
                                converted.failedSurface->surface;
                            if (sector >= 0 && surface >= 0
                                && static_cast<std::size_t>(sector)
                                    < level.sectors.size()
                                && static_cast<std::size_t>(surface)
                                    < level.sectors[static_cast<std::size_t>(sector)]
                                        .surfaces.size())
                            {
                                material = QStringLiteral(" for material '%1'")
                                    .arg(QString::fromStdString(
                                        level.sectors[static_cast<std::size_t>(sector)]
                                            .surfaces[static_cast<std::size_t>(surface)]
                                            .material));
                            }
                        }
                        *error = QStringLiteral(
                            "native text UV conversion failed%1%2: MAT dimensions are unavailable or invalid")
                            .arg(material, location);
                    }
                    return false;
                }
                levelToWrite = &*convertedLevel;
            }

            // Forensic salvage is meant to be read and edited by a person.
            // Do not inherit the runtime export dialog's compact presentation
            // here: the ordinary writer is the stable, fully spaced JKL/NDY
            // representation and makes damaged/defaulted records easier to
            // inspect manually.  Keep this explicit even though
            // JklSaveOptions currently defaults to compact=false, so a future
            // export-default change cannot silently alter salvage artifacts.
            assets::JklSaveOptions salvageOptions{};
            salvageOptions.compact = false;
            assets::saveToJKL(*levelToWrite, stream, salvageOptions);
        }
        stream.flush();
        if (buffer.failed())
        {
            if (error) *error = buffer.error();
            return false;
        }
        if (!stream.good())
        {
            if (error)
                *error = QStringLiteral("the native text writer reported a stream failure");
            return false;
        }
        if (staged.size() <= 0)
        {
            if (error)
                *error = QStringLiteral("the native text writer produced empty output");
            return false;
        }
        return true;
    }
    catch (const std::exception& exception)
    {
        if (error) *error = QString::fromUtf8(exception.what());
        return false;
    }
    catch (...)
    {
        if (error)
            *error = QStringLiteral("native text export failed after an unknown error");
        return false;
    }
}

struct NativeTextStructure
{
    int version{};
    bool hasMotSMarker{};
};

bool validateNativeTextStructure(
    QTemporaryFile& file,
    const ProjectType expectedProject,
    NativeTextStructure* structure,
    QString* error)
{
    if (!file.flush() || !file.seek(0))
    {
        if (error)
            *error = QStringLiteral(
                "could not rewind native text for structural validation");
        return false;
    }

    constexpr qint64 maximumLineBytes = 4 * 1024 * 1024;
    constexpr std::array<std::string_view, 8> requiredSections{
        "COPYRIGHT", "HEADER", "GEORESOURCE", "SECTORS",
        "COGS", "TEMPLATES", "THINGS", "LIGHTS"
    };
    std::array<int, requiredSections.size()> sectionCounts{};
    QByteArray currentSection{};
    int version = 0;
    int versionCount = 0;
    bool hasJkMarker = false;
    bool hasMotSMarker = false;

    while (!file.atEnd())
    {
        QByteArray line = file.readLine(maximumLineBytes + 1);
        if (line.isEmpty() && !file.atEnd())
        {
            if (error)
                *error = QStringLiteral("could not read native text during structural validation");
            return false;
        }
        if (line.size() > maximumLineBytes
            || (!line.endsWith('\n') && !file.atEnd()))
        {
            if (error)
                *error = QStringLiteral("native text contains a line exceeding the 4 MiB safety limit");
            return false;
        }
        if (line.contains('\0'))
        {
            if (error)
                *error = QStringLiteral("native text contains an embedded NUL byte");
            return false;
        }

        const qsizetype comment = line.indexOf('#');
        if (comment >= 0) line.truncate(comment);
        const QByteArray normalized = line.trimmed().toUpper();
        if (normalized.isEmpty()) continue;

        if (normalized == QByteArrayLiteral("END"))
        {
            currentSection.clear();
            continue;
        }
        if (normalized.startsWith(QByteArrayLiteral("SECTION:")))
        {
            currentSection = normalized.mid(sizeof("SECTION:") - 1).trimmed();
            if (currentSection == QByteArrayLiteral("JK")) hasJkMarker = true;
            if (currentSection == QByteArrayLiteral("ARCHLIGHTING"))
                hasMotSMarker = true;
            for (std::size_t index = 0; index < requiredSections.size(); ++index)
            {
                if (currentSection == QByteArray(
                        requiredSections[index].data(),
                        static_cast<qsizetype>(requiredSections[index].size())))
                {
                    ++sectionCounts[index];
                }
            }
            continue;
        }
        if (currentSection == QByteArrayLiteral("HEADER")
            && normalized.startsWith(QByteArrayLiteral("VERSION")))
        {
            const QList<QByteArray> words = normalized.simplified().split(' ');
            bool ok = false;
            const int parsedVersion = words.size() == 2
                ? words[1].toInt(&ok)
                : 0;
            if (!ok)
            {
                if (error)
                    *error = QStringLiteral("native text HEADER has an invalid VERSION record");
                return false;
            }
            version = parsedVersion;
            ++versionCount;
        }
    }

    for (std::size_t index = 0; index < requiredSections.size(); ++index)
    {
        if (sectionCounts[index] != 1)
        {
            if (error)
            {
                *error = QStringLiteral(
                    "native text must contain exactly one SECTION: %1 anchor")
                    .arg(QString::fromLatin1(
                        requiredSections[index].data(),
                        static_cast<qsizetype>(requiredSections[index].size())));
            }
            return false;
        }
    }
    if (versionCount != 1)
    {
        if (error)
            *error = QStringLiteral("native text must contain exactly one HEADER VERSION record");
        return false;
    }

    const bool expectsIjim = expectedProject == ProjectType::IJIM;
    if ((expectsIjim && version != 2 && version != 3)
        || (!expectsIjim && version != 1))
    {
        if (error)
        {
            *error = QStringLiteral(
                "native text HEADER VERSION %1 does not match expected project %2")
                .arg(version)
                .arg(projectName(expectedProject));
        }
        return false;
    }
    if (expectsIjim == hasJkMarker)
    {
        if (error)
        {
            *error = expectsIjim
                ? QStringLiteral("IJIM native text unexpectedly contains a SECTION: JK marker")
                : QStringLiteral("JKDF2/MotS native text is missing its SECTION: JK marker");
        }
        return false;
    }
    if (expectedProject == ProjectType::MOTS && !hasMotSMarker)
    {
        if (error)
            *error = QStringLiteral("MotS native text is missing its SECTION: ARCHLIGHTING marker");
        return false;
    }
    if (expectedProject == ProjectType::JKDF2 && hasMotSMarker)
    {
        if (error)
            *error = QStringLiteral("JKDF2 native text unexpectedly contains a MotS ARCHLIGHTING marker");
        return false;
    }

    if (structure)
    {
        structure->version = version;
        structure->hasMotSMarker = hasMotSMarker;
    }
    return true;
}

bool validateAndPublishNativeText(
    const Level& partialLevel,
    const std::filesystem::path& target,
    const PartialLevelSalvageTestHooks& hooks,
    const std::shared_ptr<const assets::ResourceLocator>& resources,
    QString* error)
{
    QTemporaryFile generated(stagingTemplateFor(target));
    if (!openStagingFile(generated, target, error)
        || !streamLevelToStagingFile(
            partialLevel, generated, hooks.nativeExporter, resources, error))
    {
        return false;
    }

    NativeTextStructure structure{};
    if (!validateNativeTextStructure(
            generated, partialLevel.kind, &structure, error))
    {
        return false;
    }

    Level parsed{};
    try
    {
        assets::JklLoadOptions loadOptions{};
        loadOptions.projectType = partialLevel.kind;
        QString generatedName = generated.fileName();
#ifdef Q_OS_WIN
        generatedName = extendedWin32Path(generatedName);
#endif
        parsed = assets::loadFromJKL(
            std::filesystem::path(generatedName.toStdWString()),
            loadOptions);
    }
    catch (const std::exception& exception)
    {
        if (error)
        {
            *error = QStringLiteral("native text parse-back failed: %1")
                .arg(QString::fromUtf8(exception.what()));
        }
        return false;
    }
    catch (...)
    {
        if (error)
            *error = QStringLiteral("native text parse-back failed after an unknown error");
        return false;
    }
    const bool ambiguousMotSV1 = partialLevel.kind == ProjectType::MOTS
        && parsed.kind == ProjectType::JKDF2
        && structure.version == 1
        && structure.hasMotSMarker;
    if (parsed.kind != partialLevel.kind && !ambiguousMotSV1)
    {
        if (error)
        {
            *error = QStringLiteral("native text parse-back changed the project from %1 to %2")
                .arg(projectName(partialLevel.kind), projectName(parsed.kind));
        }
        return false;
    }
    if (parsed.header.version != structure.version)
    {
        if (error)
        {
            *error = QStringLiteral(
                "native text parse-back changed HEADER VERSION from %1 to %2")
                .arg(structure.version)
                .arg(parsed.header.version);
        }
        return false;
    }
    if (ambiguousMotSV1)
    {
        // Version-1 JKL has no mandatory project tag and the loader identifies
        // MotS only when surviving surfaces carry MotS vertex-light syntax.
        // ARCHLIGHTING plus the checked SED project context is unambiguous for
        // this writer, so retain that context for the semantic re-save check.
        parsed.kind = ProjectType::MOTS;
    }

    QTemporaryFile canonical(stagingTemplateFor(target));
    if (!openStagingFile(canonical, target, error)
        || !streamLevelToStagingFile(parsed, canonical, {}, resources, error))
    {
        if (error && !error->startsWith(QStringLiteral("semantic")))
            *error = QStringLiteral("semantic re-save validation failed: %1").arg(*error);
        return false;
    }
    QString canonicalError{};
    if (!validateNativeTextStructure(
            canonical, partialLevel.kind, nullptr, &canonicalError))
    {
        if (error)
        {
            *error = QStringLiteral(
                "native text failed semantic parse/re-save validation: %1")
                .arg(canonicalError);
        }
        return false;
    }

    return publishStagedFileNoReplace(
        generated, target, hooks.beforePublish, error);
}

void appendIndentedList(QString& text, const QStringList& values)
{
    if (values.isEmpty())
    {
        text += QStringLiteral("  - (none)\n");
        return;
    }
    qsizetype addedCharacters = 0;
    qsizetype emittedEntries = 0;
    bool truncated = false;
    for (const QString& source : values)
    {
        if (emittedEntries >= kMaximumManifestListEntries
            || addedCharacters >= kMaximumManifestListCharacters)
        {
            truncated = true;
            break;
        }
        const qsizetype remaining = kMaximumManifestListCharacters
            - addedCharacters;
        bool valueTruncated = false;
        const QString value = detail::escapeForensicText(
            source,
            remaining,
            detail::ForensicNewlinePolicy::indentContinuation,
            &valueTruncated);
        text += QStringLiteral("  - %1\n").arg(value);
        addedCharacters += value.size();
        ++emittedEntries;
        truncated = truncated || valueTruncated;
        if (truncated) break;
    }
    if (truncated || emittedEntries < values.size())
    {
        text += QStringLiteral("  - ... [additional diagnostic text truncated]\n");
    }
}

QString inventoryText(const Level& level)
{
    std::size_t vertexCount = 0;
    std::size_t surfaceCount = 0;
    for (const Sector& sector : level.sectors)
    {
        vertexCount += sector.vertices.size();
        surfaceCount += sector.surfaces.size();
    }
    return QStringLiteral(
        "  Header version: %1\n"
        "  Master CMP: %2\n"
        "  Colormaps: %3\n"
        "  Materials: %4\n"
        "  Layers: %5\n"
        "  Sectors: %6\n"
        "  Sector vertices: %7\n"
        "  Sector surfaces: %8\n"
        "  Placed Things: %9\n"
        "  Templates: %10\n"
        "  COG instances: %11\n"
        "  Editor lights: %12\n"
        "  Hierarchy nodes: %13\n")
        .arg(level.header.version)
        .arg(boundedManifestField(QString::fromStdString(level.masterCMP)))
        .arg(level.colormaps.size())
        .arg(level.materials.size())
        .arg(level.layers.size())
        .arg(level.sectors.size())
        .arg(vertexCount)
        .arg(surfaceCount)
        .arg(level.things.size())
        .arg(level.templates.size())
        .arg(level.cogs.size())
        .arg(level.lights.size())
        .arg(level.h3doNodes.size());
}

QString buildManifest(
    const Level& level,
    const PartialLevelSalvageContext& context,
    const QStringList& missingSections,
    const std::filesystem::path& nativePath,
    const QString& nativeFailure)
{
    QString text{};
    text += QStringLiteral("SED PARTIAL LEVEL SALVAGE MANIFEST\n");
    text += QStringLiteral("==================================\n\n");
    text += QStringLiteral(
        "STATUS: NON-AUTHORITATIVE LAST-RESORT SALVAGE.\n"
        "This artifact does not replace the retained binary evidence and must not be treated as a complete level.\n\n");
    text += QStringLiteral("Created (UTC): %1\n")
        .arg(QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));
    const bool projectTypeKnown = context.projectTypeSource
        != PartialLevelProjectTypeSource::unknown;
    text += QStringLiteral("Project: %1\n").arg(projectTypeKnown
        ? projectName(level.kind)
        : QStringLiteral("Unknown (checked header and HEAD metadata were lost)"));
    if (projectTypeKnown && level.kind == ProjectType::MOTS)
    {
        text += context.projectTypeSource
                == PartialLevelProjectTypeSource::userSelected
            ? QStringLiteral(
                "Project validation note: MotS and JKDF2 share JKL VERSION 1; this export retains the explicit user-selected MotS context and requires the MotS ARCHLIGHTING marker.\n")
            : QStringLiteral(
                "Project validation note: MotS and JKDF2 share JKL VERSION 1; this export retains the checked SED project context and requires the MotS ARCHLIGHTING marker.\n");
    }
    if (missingSections.contains(QStringLiteral("head")))
    {
        switch (context.projectTypeSource)
        {
            case PartialLevelProjectTypeSource::checkedMetadata:
                text += QStringLiteral(
                    "Project inference warning: HEAD was lost; the project shown above was inferred from surviving checked container/header metadata.\n");
                break;
            case PartialLevelProjectTypeSource::userSelected:
                text += QStringLiteral(
                    "Project selection warning: checked header and HEAD project metadata were unavailable; the project shown above was explicitly selected by the user for this detached recovery.\n");
                break;
            case PartialLevelProjectTypeSource::unknown:
                text += QStringLiteral(
                    "Project type warning: checked header and HEAD project metadata were unavailable. Native JKL/NDY export was intentionally blocked.\n");
                break;
        }
    }
    text += QStringLiteral("Selected damaged input: %1\n")
        .arg(pathText(context.sourceRecoveryPath));
    text += QStringLiteral("Retained forensic input: %1\n")
        .arg(pathText(context.forensicRecoveryPath));
    if (!nativePath.empty())
    {
        text += QStringLiteral("Native text salvage: %1\n")
            .arg(pathText(nativePath));
        text += QStringLiteral("Native export status: generated and parsed back successfully\n");
    }
    else
    {
        text += QStringLiteral("Native text salvage: not written\n");
        text += QStringLiteral("Native export status: %1\n")
            .arg(nativeFailure.isEmpty()
                ? QStringLiteral("not available")
                : boundedManifestField(nativeFailure));
    }

    text += QStringLiteral("\nMissing required Level sections:\n");
    appendIndentedList(text, missingSections);
    text += QStringLiteral("Unidentified missing section units: %1\n")
        .arg(static_cast<qulonglong>(
            context.unidentifiedMissingSectionCount));
    if (context.unidentifiedMissingSectionCount != 0)
    {
        text += QStringLiteral(
            "Unidentified loss warning: checked framing could not name these units. Any one may have contained required Level data, and its bytes cannot be reconstructed or assigned to a semantic domain.\n");
    }
    text += QStringLiteral("\nRecovery warnings:\n");
    appendIndentedList(text, context.recoveryWarnings);
    text += QStringLiteral("\nRecovered data inventory (missing sections are represented by defaults/empty collections):\n");
    text += inventoryText(level);
    text += QStringLiteral(
        "\nLimitations and safe use:\n"
        "  - Only fully decoded semantic domains are represented. Missing section bytes were not copied or guessed; a lost section was defaulted or omitted as a whole.\n"
        "  - An unidentified lost unit cannot be named or mapped back to a domain; treat the decoded Level as potentially missing required data.\n"
        "  - The retained forensic binary remains the only source for bytes in an unrecoverable section.\n"
        "  - References into a lost collection may be dangling or incomplete. Inspect geometry, Things, templates, COGs, and resources before reuse.\n"
        "  - JKL/NDY is an engine interchange format and cannot retain all binary SED precision, editor state, COG buffers, or undo/redo history.\n"
        "  - Manual text edits are speculative. Keep the forensic binary unchanged and save any inspected recovery to a new .sed file.\n"
        "  - This manifest and any JKL/NDY file are diagnostic aids, never proof that the recovered Level is complete or playable.\n");
    if (text.size() > kMaximumManifestCharacters)
    {
        const QString suffix = QStringLiteral(
            "\n... [manifest truncated at the safety limit]\n");
        text.truncate(kMaximumManifestCharacters - suffix.size());
        text += suffix;
    }
    return text;
}

} // namespace

namespace detail
{

std::size_t maximumPartialLevelNativeTextBytesForTesting() noexcept
{
    return kMaximumNativeTextBytes;
}

qsizetype maximumPartialLevelManifestCharactersForTesting() noexcept
{
    return kMaximumManifestCharacters;
}

} // namespace detail

QString PartialLevelSalvageResult::recoveryWarning() const
{
    if (wroteManifest())
    {
        QString message = QStringLiteral(
            "A non-authoritative partial-Level salvage manifest was written to %1.")
            .arg(pathText(manifestPath));
        if (wroteNativeText())
        {
            message += QStringLiteral(" The surviving Level data was also exported as %1 for last-resort manual inspection.")
                .arg(pathText(nativeTextPath));
        }
        else if (!nativeExportFailure.isEmpty())
        {
            message += QStringLiteral(" Native JKL/NDY export was not safe or failed: %1")
                .arg(nativeExportFailure);
        }
        return message;
    }
    if (wroteNativeText())
    {
        return QStringLiteral(
            "A non-authoritative native partial-Level text export was written to %1, but its companion manifest could not be published: %2")
            .arg(pathText(nativeTextPath), error);
    }
    return error;
}

QStringList lostRequiredLevelSections(const QStringList& missingSections)
{
    return normalizeMissingSections(missingSections);
}

static PartialLevelSalvageResult writePartialLevelSalvageImpl(
    const Level& partialLevel,
    const PartialLevelSalvageContext& context,
    const PartialLevelSalvageTestHooks& hooks)
{
    PartialLevelSalvageResult result{};
    const QStringList missingSections = normalizeMissingSections(
        context.missingRequiredSections);
    if (missingSections.isEmpty()
        && context.unidentifiedMissingSectionCount == 0)
    {
        return result;
    }

    const std::filesystem::path anchor = !context.forensicRecoveryPath.empty()
        ? context.forensicRecoveryPath
        : context.sourceRecoveryPath;
    std::filesystem::path directory = anchor.parent_path();
    if (directory.empty())
    {
        std::error_code currentError{};
        directory = std::filesystem::current_path(currentError);
        if (currentError)
        {
            result.error = QStringLiteral("Could not choose a partial-Level salvage directory: %1")
                .arg(QString::fromStdString(currentError.message()));
            return result;
        }
    }
    if (!QDir().mkpath(QString::fromStdWString(directory.wstring())))
    {
        result.error = QStringLiteral("Could not create the partial-Level salvage directory %1.")
            .arg(pathText(directory));
        return result;
    }

    const bool projectTypeKnown = context.projectTypeSource
        != PartialLevelProjectTypeSource::unknown;
    const QString extension = projectTypeKnown
        ? nativeExtension(partialLevel.kind)
        : QString{};
    const QString sourceStem = anchor.empty()
        ? QStringLiteral("recovered-level")
        : QFileInfo(QString::fromStdWString(anchor.wstring())).completeBaseName();
    const ArtifactPaths paths = chooseArtifactPaths(
        directory, sourceStem, extension, context.artifactId);
    if (paths.manifest.empty())
    {
        result.error = QStringLiteral(
            "Could not reserve a collision-safe partial-Level salvage filename in %1.")
            .arg(pathText(directory));
        return result;
    }

    if (!projectTypeKnown)
    {
        result.nativeExportFailure = QStringLiteral(
            "the checked project type is unknown; native JKL/NDY export was intentionally blocked");
    }
    else if (extension.isEmpty())
    {
        result.nativeExportFailure = QStringLiteral(
            "the recovered project type is unsupported (%1)")
            .arg(static_cast<int>(partialLevel.kind));
    }
    else
    {
        QString nativeError{};
        if (!validateAndPublishNativeText(
                partialLevel,
                paths.nativeText,
                hooks,
                context.nativeExportResources,
                &nativeError))
        {
            result.nativeExportFailure = nativeError.isEmpty()
                ? QStringLiteral("native text validation or publication failed")
                : nativeError;
        }
        else
        {
            result.nativeTextPath = paths.nativeText;
        }
    }

    try
    {
        const QString manifest = buildManifest(
            partialLevel,
            context,
            missingSections,
            result.nativeTextPath,
            result.nativeExportFailure);
        QString manifestError{};
        const QByteArray manifestBytes = manifest.toUtf8();
        if (!writeBytesNoReplace(
                paths.manifest, manifestBytes, hooks.beforePublish,
                &manifestError))
        {
            result.error = manifestError;
            return result;
        }
        result.manifestPath = paths.manifest;
    }
    catch (const std::exception& exception)
    {
        // Native text may already have been atomically published. Preserve
        // that path in the result so a later manifest allocation/encoding
        // failure never makes the successfully retained artifact invisible.
        result.error = QStringLiteral(
            "Partial-Level manifest generation failed safely: %1")
            .arg(QString::fromUtf8(exception.what()));
        return result;
    }
    catch (...)
    {
        result.error = QStringLiteral(
            "Partial-Level manifest generation failed safely after an unknown error.");
        return result;
    }
    return result;
}

PartialLevelSalvageResult writePartialLevelSalvage(
    const Level& partialLevel,
    const PartialLevelSalvageContext& context,
    PartialLevelSalvageTestHooks testHooks)
{
    try
    {
        return writePartialLevelSalvageImpl(
            partialLevel, context, testHooks);
    }
    catch (const std::exception& exception)
    {
        PartialLevelSalvageResult result{};
        result.error = QStringLiteral("Partial-Level text salvage failed safely: %1")
            .arg(QString::fromUtf8(exception.what()));
        return result;
    }
    catch (...)
    {
        PartialLevelSalvageResult result{};
        result.error = QStringLiteral(
            "Partial-Level text salvage failed safely after an unknown error.");
        return result;
    }
}

} // namespace sed::ui::recovery
