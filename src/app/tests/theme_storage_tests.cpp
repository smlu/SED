#include "test_common.hpp"

#include "app_settings.hpp"
#include "render/viewport_color_theme.hpp"
#include "ui/cog/cog_script_theme_utils.hpp"
#include "ui/theme_file_storage.hpp"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>
#include <QStandardPaths>
#include <QTemporaryDir>

#include <algorithm>
#include <filesystem>
#include <vector>

namespace sed::app_tests
{
namespace
{

using sed::render::ViewportColorTheme;
using sed::ui::theme_storage::ThemeDocument;
using sed::ui::theme_storage::ThemeFormat;

bool writeFile(const QString& path, const QByteArray& bytes)
{
    if (!QDir().mkpath(QFileInfo(path).absolutePath()))
    {
        return false;
    }
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
    {
        return false;
    }
    return file.write(bytes) == bytes.size();
}

QByteArray readFileBytes(const QString& path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray{};
}

void testThemeDirectorySelection()
{
    QTemporaryDir root{};
    check(root.isValid(), "Theme storage path fixture creates an isolated directory");
    if (!root.isValid())
    {
        return;
    }

    const std::filesystem::path portableSettings =
        std::filesystem::path(root.path().toStdWString()) / "portable" / "sed.ini";
    const std::filesystem::path userData =
        std::filesystem::path(root.path().toStdWString()) / "user";
    const QString blockedData = QString::fromStdWString(
        portableSettings.parent_path().wstring()) + QStringLiteral("/data");
    check(QDir().mkpath(QFileInfo(blockedData).absolutePath())
            && writeFile(blockedData, QByteArrayLiteral("block")),
        "Theme storage fixture blocks portable directory creation with a file");

    QString error{};
    const std::filesystem::path fallback = sed::appThemeDirectoryPath(
        sed::AppThemeKind::viewport,
        portableSettings,
        userData,
        &error);
    check(fallback == userData / "data" / "themes"
            && error.isEmpty(),
        "A non-writable portable theme folder selects the shared per-user theme root");

    const std::filesystem::path fallbackCog = sed::appThemeDirectoryPath(
        sed::AppThemeKind::cog,
        portableSettings,
        userData,
        &error);
    check(fallbackCog == fallback / "cog",
        "COG themes use a cog subfolder in the per-user fallback");

    const std::filesystem::path writableSettings =
        std::filesystem::path(root.path().toStdWString()) / "writable" / "sed.ini";
    const std::filesystem::path unusedUserData =
        std::filesystem::path(root.path().toStdWString()) / "unused-user";
    const std::filesystem::path portableThemeRoot =
        writableSettings.parent_path() / "data" / "themes";
    check(sed::appThemeDirectoryPath(
                sed::AppThemeKind::viewport, writableSettings, unusedUserData, &error)
                == portableThemeRoot
            && sed::appThemeDirectoryPath(
                sed::AppThemeKind::cog, writableSettings, unusedUserData, &error)
                == portableThemeRoot / "cog",
        "Writable portable app and COG theme stores use the plural themes folder");

    const ViewportColorTheme fallbackTheme{
        QStringLiteral("custom_fallback"), QStringLiteral("Fallback"),
        sed::render::nightBlueViewportColorPalette(),
    };
    check(sed::ui::theme_storage::saveThemeDocuments(
              fallback,
              {{fallbackTheme.id, sed::render::viewportColorThemeJson(fallbackTheme),
                  ThemeFormat::viewport}},
              &error, ThemeFormat::viewport),
        "The fallback theme folder can commit a named definition without a manifest");
    QFile::remove(blockedData);
    const std::filesystem::path retainedFallback = sed::appThemeDirectoryPath(
        sed::AppThemeKind::viewport,
        portableSettings,
        userData,
        &error);
    check(retainedFallback == fallback,
        "A writable fallback with a committed definition stays selected after portable storage becomes writable");

    QSettings isolatedSettings(
        QDir(root.path()).filePath(QStringLiteral("isolated.ini")),
        QSettings::IniFormat);
    const auto isolatedDirectories =
        sed::ui::theme_storage::directoriesForSettings(
            isolatedSettings, sed::AppThemeKind::viewport);
    const std::filesystem::path temporaryRoot(root.path().toStdWString());
    const bool everyPathIsIsolated = !isolatedDirectories.empty()
        && std::all_of(isolatedDirectories.begin(), isolatedDirectories.end(),
            [&temporaryRoot](const std::filesystem::path& path)
            {
                const std::filesystem::path normalizedPath =
                    std::filesystem::weakly_canonical(path);
                const std::filesystem::path normalizedRoot =
                    std::filesystem::weakly_canonical(temporaryRoot);
                auto pathPart = normalizedPath.begin();
                auto rootPart = normalizedRoot.begin();
                for (; rootPart != normalizedRoot.end(); ++rootPart, ++pathPart)
                {
                    if (pathPart == normalizedPath.end() || *pathPart != *rootPart)
                    {
                        return false;
                    }
                }
                return true;
            });
    check(everyPathIsIsolated,
        "A temporary QSettings file never reads the real per-user theme directory");
    const auto viewportPaths = sed::appThemeDirectorySearchPaths(
        sed::AppThemeKind::viewport, portableSettings, userData);
    const auto cogPaths = sed::appThemeDirectorySearchPaths(
        sed::AppThemeKind::cog, portableSettings, userData);
    check(!viewportPaths.empty() && viewportPaths.front() == fallback
            && std::find(viewportPaths.begin(), viewportPaths.end(),
                fallback / "app") != viewportPaths.end()
            && !cogPaths.empty() && cogPaths.front() == fallback / "cog"
            && std::find(cogPaths.begin(), cogPaths.end(), fallback)
                != cogPaths.end(),
        "Theme search prioritizes viewport root and COG cog/ while accepting their alternate grouping");
    CogScriptTheme cogAtRoot = defaultCogScriptTheme();
    cogAtRoot.id = QStringLiteral("custom_cog_at_root");
    cogAtRoot.label = QStringLiteral("COG at Root");
    const bool rootCogSaved = sed::ui::theme_storage::saveThemeDocuments(
        fallback,
        {{cogAtRoot.id, cogScriptThemeJson(cogAtRoot), ThemeFormat::cog}},
        &error, ThemeFormat::cog);
    const auto cogLoadedFromAlternateRoot =
        sed::ui::theme_storage::readThemeDocumentsFromDirectories(
            cogPaths, ThemeFormat::cog, &error);
    check(rootCogSaved
            && std::any_of(cogLoadedFromAlternateRoot.begin(),
                cogLoadedFromAlternateRoot.end(),
                [&cogAtRoot](const ThemeDocument& document)
                { return document.id == cogAtRoot.id; }),
        "COG loader accepts a correctly tagged definition at the shared root");
    const auto withoutUserData = sed::appThemeDirectorySearchPaths(
        sed::AppThemeKind::cog, portableSettings, {});
    check(std::find(withoutUserData.begin(), withoutUserData.end(),
                std::filesystem::path("cog")) == withoutUserData.end(),
        "An empty per-user data path never adds a relative COG directory");

    const std::filesystem::path cogOnlyUser =
        std::filesystem::path(root.path().toStdWString()) / "cog-only-user";
    const std::filesystem::path cogOnlyRoot = cogOnlyUser / "data" / "themes";
    CogScriptTheme groupedCog = defaultCogScriptTheme();
    groupedCog.id = QStringLiteral("custom_grouped_cog");
    groupedCog.label = QStringLiteral("Grouped COG");
    check(sed::ui::theme_storage::saveThemeDocuments(
            cogOnlyRoot / "cog",
            {{groupedCog.id, cogScriptThemeJson(groupedCog), ThemeFormat::cog}},
            &error, ThemeFormat::cog)
            && sed::appThemeDirectoryPath(
                sed::AppThemeKind::cog, portableSettings, cogOnlyUser, &error)
                == cogOnlyRoot / "cog",
        "A per-user fallback containing only data/themes/cog remains the COG save target");

    const std::filesystem::path appOnlyUser =
        std::filesystem::path(root.path().toStdWString()) / "app-only-user";
    const std::filesystem::path appOnlyRoot = appOnlyUser / "data" / "themes";
    const ViewportColorTheme groupedApp{
        QStringLiteral("custom_grouped_app"), QStringLiteral("Grouped App"),
        sed::render::nightBlueViewportColorPalette()};
    check(sed::ui::theme_storage::saveThemeDocuments(
            appOnlyRoot / "app",
            {{groupedApp.id, sed::render::viewportColorThemeJson(groupedApp),
                ThemeFormat::viewport}},
            &error, ThemeFormat::viewport)
            && sed::appThemeDirectoryPath(
                sed::AppThemeKind::viewport, portableSettings, appOnlyUser, &error)
                == appOnlyRoot,
        "A per-user fallback containing only data/themes/app remains the viewport save target");
    const auto appOnlyPaths = sed::appThemeDirectorySearchPaths(
        sed::AppThemeKind::viewport, portableSettings, appOnlyUser);
    const auto appLoadedFromSubfolder =
        sed::ui::theme_storage::readThemeDocumentsFromDirectories(
            appOnlyPaths, ThemeFormat::viewport, &error);
    check(std::any_of(appLoadedFromSubfolder.begin(), appLoadedFromSubfolder.end(),
            [&groupedApp](const ThemeDocument& document)
            { return document.id == groupedApp.id; }),
        "Viewport loader accepts a correctly tagged definition in the optional app/ folder");
}

void testThemeWriteRollback()
{
    QTemporaryDir root{};
    check(root.isValid(), "Theme rollback fixture creates an isolated directory");
    if (!root.isValid())
    {
        return;
    }

    const std::filesystem::path directory(root.path().toStdWString());
    QString error{};
    const ThemeDocument original{
        QStringLiteral("custom_keep"),
        sed::render::viewportColorThemeJson({
            QStringLiteral("custom_keep"),
            QStringLiteral("Before"),
            sed::render::nightBlueViewportColorPalette(),
        }),
    };
    check(sed::ui::theme_storage::saveThemeDocuments(
              directory, { original }, &error, ThemeFormat::viewport),
        "Rollback fixture commits an initial named theme definition");
    const QString originalPath = sed::ui::theme_storage::themeFilePathForLabel(
        directory, QStringLiteral("Before"), ThemeFormat::viewport);
    const QByteArray originalBytes = readFileBytes(originalPath);

    const ThemeDocument updated{
        QStringLiteral("custom_keep"),
        sed::render::viewportColorThemeJson({
            QStringLiteral("custom_keep"),
            QStringLiteral("After"),
            sed::render::originalSedViewportColorPalette(),
        }),
    };
    const ThemeDocument added{
        QStringLiteral("custom_new"),
        sed::render::viewportColorThemeJson({
            QStringLiteral("custom_new"),
            QStringLiteral("New"),
            sed::render::nightBlueViewportColorPalette(),
        }),
    };
    check(!sed::ui::theme_storage::saveThemeDocumentsFailingAfterPayloadsForTest(
              directory, { updated, added }, 1, &error)
            && error == QStringLiteral("An injected theme write failure was rolled back."),
        "A failure after one payload commit reports successful rollback");
    const QByteArray payloadFailureBytes = readFileBytes(originalPath);
    const auto afterPayloadFailure =
        sed::ui::theme_storage::readThemeDocuments(directory, &error);
    check(payloadFailureBytes == originalBytes
            && !QFileInfo::exists(sed::ui::theme_storage::themeFilePathForLabel(
                directory, QStringLiteral("New"), ThemeFormat::viewport))
            && afterPayloadFailure.size() == 1
            && afterPayloadFailure.front().id == original.id,
        "Payload rollback restores prior bytes without a generated manifest");

    check(!sed::ui::theme_storage::saveThemeDocumentsFailingBeforeCleanupForTest(
              directory, { updated, added }, &error)
            && error == QStringLiteral("An injected pre-cleanup failure was rolled back."),
        "A pre-cleanup failure reports successful payload rollback");
    const QByteArray preCleanupFailureBytes = readFileBytes(originalPath);
    const auto afterPreCleanupFailure =
        sed::ui::theme_storage::readThemeDocuments(directory, &error);
    check(preCleanupFailureBytes == originalBytes
            && !QFileInfo::exists(sed::ui::theme_storage::themeFilePathForLabel(
                directory, QStringLiteral("New"), ThemeFormat::viewport))
            && afterPreCleanupFailure.size() == 1
            && afterPreCleanupFailure.front().id == original.id,
        "Pre-cleanup rollback restores the previous payload set without a manifest");
}

void testThemeTypedFileDiscovery()
{
    QTemporaryDir root{};
    check(root.isValid(), "Typed theme fixture creates an isolated directory");
    if (!root.isValid()) return;
    const std::filesystem::path directory(root.path().toStdWString());
    const ViewportColorTheme viewport{QStringLiteral("custom_view"),
        QStringLiteral("View"), sed::render::nightBlueViewportColorPalette()};
    CogScriptTheme cog=defaultCogScriptTheme();
    cog.id=QStringLiteral("custom_cog"); cog.label=QStringLiteral("Grouped COG");
    QString error{};
    check(sed::ui::theme_storage::saveThemeDocuments(directory,{
            {viewport.id,sed::render::viewportColorThemeJson(viewport),ThemeFormat::viewport},
            {cog.id,cogScriptThemeJson(cog),ThemeFormat::cog}},&error)
            && error.isEmpty(), "Typed themes coexist in the shared root");
    const QString malformed=QDir(QString::fromStdWString(directory.wstring()))
        .filePath(QStringLiteral("damaged.json"));
    const QByteArray original=QByteArrayLiteral("{not json");
    writeFile(malformed,original);
    check(sed::ui::theme_storage::readThemeDocuments(
            directory,ThemeFormat::viewport).size()==1
            && sed::ui::theme_storage::readThemeDocuments(
                directory,ThemeFormat::cog).size()==1,
        "Each reader discovers only its typed name-based definitions");
    check(sed::ui::theme_storage::saveThemeDocuments(directory,{{viewport.id,
                sed::render::viewportColorThemeJson(viewport),ThemeFormat::viewport}},
                &error,ThemeFormat::viewport)
            && readFileBytes(malformed)==original
            && QFileInfo::exists(sed::ui::theme_storage::themeFilePathForLabel(
                directory,cog.label,ThemeFormat::cog))
            && !QFileInfo::exists(QDir(root.path()).filePath(QStringLiteral(".manifest"))),
        "Typed saves preserve malformed and foreign-format files without creating manifests");
}

void testThemeSearchPathNamePrecedence()
{
    QTemporaryDir root{};
    check(root.isValid(), "Theme search precedence fixture creates an isolated directory");
    if (!root.isValid()) return;
    const std::filesystem::path active =
        std::filesystem::path(root.path().toStdWString()) / "active";
    const std::filesystem::path fallback =
        std::filesystem::path(root.path().toStdWString()) / "fallback";
    const ViewportColorTheme activeTheme{
        QStringLiteral("custom_active"), QStringLiteral("Same Name"),
        sed::render::nightBlueViewportColorPalette()};
    const ViewportColorTheme fallbackTheme{
        QStringLiteral("custom_fallback"), QStringLiteral("Same Name"),
        sed::render::originalSedViewportColorPalette()};
    QString error{};
    const bool activeSaved = sed::ui::theme_storage::saveThemeDocuments(
        active, {{activeTheme.id, sed::render::viewportColorThemeJson(activeTheme),
            ThemeFormat::viewport}}, &error, ThemeFormat::viewport);
    const bool fallbackSaved = sed::ui::theme_storage::saveThemeDocuments(
        fallback, {{fallbackTheme.id, sed::render::viewportColorThemeJson(fallbackTheme),
            ThemeFormat::viewport}}, &error, ThemeFormat::viewport);
    const auto merged = sed::ui::theme_storage::readThemeDocumentsFromDirectories(
        {active, fallback}, ThemeFormat::viewport, &error);
    check(activeSaved && fallbackSaved && merged.size() == 1
            && merged.front().id == activeTheme.id,
        "A same-name fallback theme is ignored in favor of the active definition");
}

void testThemeDeleteAcrossSearchPaths()
{
    QTemporaryDir root{};
    check(root.isValid(), "Theme delete fixture creates an isolated directory");
    if (!root.isValid()) return;
    const std::filesystem::path active =
        std::filesystem::path(root.path().toStdWString()) / "active";
    const std::filesystem::path alternate = active / "app";
    const QString sharedId = QStringLiteral("custom_shared_theme");
    const ViewportColorTheme selected{
        sharedId, QStringLiteral("Shared Theme"),
        sed::render::nightBlueViewportColorPalette()};
    const ViewportColorTheme other{
        QStringLiteral("custom_keep_theme"), QStringLiteral("Keep Theme"),
        sed::render::originalSedViewportColorPalette()};
    CogScriptTheme sameIdDifferentFormat = defaultCogScriptTheme();
    sameIdDifferentFormat.id = sharedId;
    sameIdDifferentFormat.label = QStringLiteral("Shared Theme");
    QString activeError{};
    QString alternateError{};
    QString deletionError{};
    const bool activeSaved = sed::ui::theme_storage::saveThemeDocuments(
        active,
        {{selected.id, sed::render::viewportColorThemeJson(selected), ThemeFormat::viewport},
            {other.id, sed::render::viewportColorThemeJson(other), ThemeFormat::viewport},
            {sameIdDifferentFormat.id, cogScriptThemeJson(sameIdDifferentFormat),
                ThemeFormat::cog}},
        &activeError);
    const bool alternateSaved = sed::ui::theme_storage::saveThemeDocuments(
        alternate,
        {{selected.id, sed::render::viewportColorThemeJson(selected),
            ThemeFormat::viewport}},
        &alternateError, ThemeFormat::viewport);
    const bool deleted = sed::ui::theme_storage::removeThemeDocumentByIdFromDirectories(
        {active, alternate}, sharedId, ThemeFormat::viewport, &deletionError);
    const auto activeViewport = sed::ui::theme_storage::readThemeDocuments(
        active, ThemeFormat::viewport);
    const auto alternateViewport = sed::ui::theme_storage::readThemeDocuments(
        alternate, ThemeFormat::viewport);
    const auto activeCog = sed::ui::theme_storage::readThemeDocuments(
        active, ThemeFormat::cog);
    const bool deletionStateMatches = activeSaved && alternateSaved && deleted
        && activeViewport.size() == 1
        && activeViewport.front().id == other.id
        && alternateViewport.empty()
        && activeCog.size() == 1
        && activeCog.front().id == sharedId;
    if (!deletionStateMatches)
    {
        std::cerr << "Theme deletion diagnostic: activeSaved=" << activeSaved
            << " alternateSaved=" << alternateSaved << " deleted=" << deleted
            << " activeError='" << activeError.toStdString() << "'"
            << " alternateError='" << alternateError.toStdString() << "'"
            << " deletionError='" << deletionError.toStdString() << "'"
            << " activeViewport=" << activeViewport.size()
            << " alternateViewport=" << alternateViewport.size()
            << " activeCog=" << activeCog.size() << '\n';
        const auto printJsonFiles = [](const char* group,
            const std::filesystem::path& directory)
        {
            const QString directoryName = QString::fromStdWString(directory.wstring());
            const QFileInfoList files = QDir(directoryName).entryInfoList(
                QStringList{QStringLiteral("*.json")},
                QDir::Files | QDir::NoDotAndDotDot,
                QDir::Name | QDir::IgnoreCase);
            std::cerr << "  " << group << " files:";
            for (const QFileInfo& info : files)
            {
                QString id{};
                QString format{};
                QFile file(info.filePath());
                if (file.open(QIODevice::ReadOnly))
                {
                    const QJsonDocument json = QJsonDocument::fromJson(file.readAll());
                    if (json.isObject())
                    {
                        const QJsonObject object = json.object();
                        id = object.value(QStringLiteral("id")).toString();
                        format = object.value(QStringLiteral("format")).toString();
                    }
                }
                std::cerr << " " << info.fileName().toStdString()
                    << "[id=" << id.toStdString()
                    << ",format=" << format.toStdString() << "]";
            }
            std::cerr << '\n';
        };
        printJsonFiles("active", active);
        printJsonFiles("alternate", alternate);
    }
    check(deletionStateMatches,
        "Theme deletion removes matching id/format copies across groups and preserves other ids and formats");
}

void testNamedThemeFormats()
{
    QTemporaryDir root{};
    check(root.isValid(), "Named theme format fixture creates an isolated directory");
    if (!root.isValid())
    {
        return;
    }
    const QString label = QStringLiteral("Twilight Workbench");
    check(sed::ui::theme_storage::themeFileNameForLabel(label, ThemeFormat::viewport)
                == QStringLiteral("twilight_workbench.app.json")
            && sed::ui::theme_storage::themeFileNameForLabel(label, ThemeFormat::cog)
                == QStringLiteral("twilight_workbench.cog.json"),
        "Theme filenames use the user name with distinct COG and app suffixes");
    check(sed::ui::theme_storage::themeFormatTag(ThemeFormat::viewport)
                == QStringLiteral("sed.app-theme.v1")
            && sed::ui::theme_storage::themeFormatTag(ThemeFormat::cog)
                == QStringLiteral("sed.cog-theme.v1"),
        "App and COG themes carry their distinct current-format tags");
    check(sed::ui::theme_storage::themeNamesCollide(
                QStringLiteral("Copper Theme"), QStringLiteral("copper-theme"))
            && !sed::ui::theme_storage::themeNamesCollide(
                QStringLiteral("Copper Theme"), QStringLiteral("Brass Theme")),
        "Theme-name collision checks catch case and filename-slug duplicates");
    const QString id = QStringLiteral("custom_twilight_workbench");
    const ViewportColorTheme viewport{id, label, sed::render::nightBlueViewportColorPalette()};
    CogScriptTheme cog = defaultCogScriptTheme();
    cog.id = id;
    cog.label = label;
    const std::filesystem::path directory(root.path().toStdWString());
    QString error{};
    check(sed::ui::theme_storage::saveThemeDocuments(directory, {
                {id, sed::render::viewportColorThemeJson(viewport), ThemeFormat::viewport},
                {id, cogScriptThemeJson(cog), ThemeFormat::cog}}, &error)
            && error.isEmpty(),
        "Same-name COG and viewport themes save together in the shared root");
    check(QFileInfo::exists(sed::ui::theme_storage::themeFilePathForLabel(
                directory, label, ThemeFormat::viewport))
            && QFileInfo::exists(sed::ui::theme_storage::themeFilePathForLabel(
                directory, label, ThemeFormat::cog))
            && !QFileInfo::exists(QDir(root.path()).filePath(QStringLiteral(".manifest"))),
        "Theme definitions use name-based files and do not create a manifest");
    check(sed::ui::theme_storage::readThemeDocuments(directory, ThemeFormat::viewport).size() == 1
            && sed::ui::theme_storage::readThemeDocuments(directory, ThemeFormat::cog).size() == 1
            && sed::ui::theme_storage::readThemeDocuments(directory).size() == 2,
        "Each theme loader discovers only its embedded format while generic discovery retains both typed files");
    const QJsonObject viewportObject = sed::render::viewportColorThemeJson(viewport);
    const QJsonObject cogObject = cogScriptThemeJson(cog);
    QStringList expectedViewportKeys{};
    for (const auto& [key, member] : sed::render::kViewportColorPaletteFields)
    {
        Q_UNUSED(member);
        expectedViewportKeys.push_back(QLatin1String(key));
    }
    expectedViewportKeys.sort();
    QStringList expectedCogKeys{};
    for (const CogScriptThemeColorDef& def : cogScriptThemeColorDefs())
    {
        expectedCogKeys.push_back(QString::fromLatin1(def.key));
    }
    expectedCogKeys.sort();
    check(viewportObject.value(QStringLiteral("name")).toString() == label
            && !viewportObject.contains(QStringLiteral("label"))
            && viewportObject.value(QStringLiteral("colors")).toObject().keys()
                == expectedViewportKeys
            && sed::ui::theme_storage::themeColorPayloadIsValid(
                viewportObject, ThemeFormat::viewport),
        "Viewport JSON uses the top-level name field and all 20 color definitions");
    check(cogObject.value(QStringLiteral("name")).toString() == label
            && !cogObject.contains(QStringLiteral("label"))
            && cogObject.value(QStringLiteral("colors")).toObject().keys()
                == expectedCogKeys
            && cogObject.value(QStringLiteral("colors")).toObject()
                .value(QStringLiteral("label")).isString()
            && sed::ui::theme_storage::themeColorPayloadIsValid(
                cogObject, ThemeFormat::cog),
        "COG JSON keeps its label color nested and serializes all 28 color definitions");
    CogScriptTheme recoveredCog{};
    check(cogScriptThemeFromJson(cogObject, recoveredCog)
            && recoveredCog.label == label,
        "COG JSON round-trips the display name independently from the label color");
    QJsonObject missingCogColor = cogObject;
    QJsonObject missingColors = missingCogColor.value(QStringLiteral("colors")).toObject();
    missingColors.remove(QStringLiteral("label"));
    missingCogColor.insert(QStringLiteral("colors"), missingColors);
    QJsonObject extraViewportColor = viewportObject;
    QJsonObject extraColors = extraViewportColor.value(QStringLiteral("colors")).toObject();
    extraColors.insert(QStringLiteral("unexpected"), QStringLiteral("#ffffff"));
    extraViewportColor.insert(QStringLiteral("colors"), extraColors);
    check(!sed::ui::theme_storage::themeColorPayloadIsValid(
                missingCogColor, ThemeFormat::cog)
            && !sed::ui::theme_storage::themeColorPayloadIsValid(
                extraViewportColor, ThemeFormat::viewport),
        "Theme validation rejects incomplete and unexpected color sets");
    QJsonObject colorAsName = cogObject;
    colorAsName.insert(QStringLiteral("name"), QStringLiteral("#ffe3b771"));
    check(!cogScriptThemeFromJson(colorAsName, recoveredCog),
        "A COG color value cannot masquerade as its top-level theme name");
}

} // namespace

void runThemeStorageTests()
{
    testThemeDirectorySelection();
    testThemeWriteRollback();
    testThemeTypedFileDiscovery();
    testThemeSearchPathNamePrecedence();
    testThemeDeleteAcrossSearchPaths();
    testNamedThemeFormats();
}

} // namespace sed::app_tests
