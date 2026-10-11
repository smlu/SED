#include "ui/project/project_startup_utils.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <stdexcept>

#include <QDir>
#include <QSettings>

#include "sed/level/build.hpp"
#include "sed/level/geometry.hpp"
#include "sed/level/uv.hpp"
#include "sed/text.hpp"

namespace sed::ui::project
{
namespace
{

using ExportBufferMember = std::uint32_t LevelExportBufferExtras::*;

struct ExportBufferSetting
{
    const char* name{};
    ExportBufferMember member{};
};

constexpr std::array<ExportBufferSetting, 12> kExportBufferSettings{
    ExportBufferSetting{ "sounds", &LevelExportBufferExtras::sounds },
    ExportBufferSetting{ "materials", &LevelExportBufferExtras::materials },
    ExportBufferSetting{ "ai_classes", &LevelExportBufferExtras::aiClasses },
    ExportBufferSetting{ "models", &LevelExportBufferExtras::models },
    ExportBufferSetting{ "sprites", &LevelExportBufferExtras::sprites },
    ExportBufferSetting{ "keyframes", &LevelExportBufferExtras::keyframes },
    ExportBufferSetting{ "puppets", &LevelExportBufferExtras::puppets },
    ExportBufferSetting{ "sound_classes", &LevelExportBufferExtras::soundClasses },
    ExportBufferSetting{ "cog_scripts", &LevelExportBufferExtras::cogScripts },
    ExportBufferSetting{ "cogs", &LevelExportBufferExtras::cogs },
    ExportBufferSetting{ "templates", &LevelExportBufferExtras::templates },
    ExportBufferSetting{ "things", &LevelExportBufferExtras::things },
};

QString exportBufferSettingsPrefix(const ProjectType kind)
{
    QString project{};
    switch (kind)
    {
        case ProjectType::JKDF2: project = QStringLiteral("jkdf2"); break;
        case ProjectType::MOTS: project = QStringLiteral("mots"); break;
        case ProjectType::IJIM: project = QStringLiteral("ijim"); break;
    }
    return QStringLiteral("projects/%1/new_project_export_buffer_extras/").arg(project);
}

constexpr int kStartupDialogPreferredWidth = 1180;
constexpr int kStartupDialogPreferredHeight = 720;
constexpr int kStartupDialogScreenMargin = 16;

void applyClassicStarterRoomUvTopology(Sector& sector)
{
    // Pascal NewProject deliberately starts each wall at its lower-left
    // vertical edge. RecalcAll derives its frame from the initially zero UVs,
    // so wall height remains the U axis and wall width remains the V axis.
    // Preserve that classic JKDF2/MotS authoring orientation on the restored
    // Pascal-size starter room. IJIM retains its project-specific topology and
    // RecalcNewAll initialization.
    constexpr std::array<std::array<std::int32_t, 4>, 6> kSurfaceVertices{
        std::array<std::int32_t, 4>{ 3, 0, 1, 2 },
        std::array<std::int32_t, 4>{ 4, 5, 6, 7 },
        std::array<std::int32_t, 4>{ 1, 6, 5, 2 },
        std::array<std::int32_t, 4>{ 0, 7, 6, 1 },
        std::array<std::int32_t, 4>{ 3, 4, 7, 0 },
        std::array<std::int32_t, 4>{ 2, 5, 4, 3 },
    };

    if ( sector.vertices.size() != 8u
        || sector.surfaces.size() != kSurfaceVertices.size() )
    {
        return;
    }

    for ( std::size_t surfaceIndex = 0;
        surfaceIndex < kSurfaceVertices.size();
        ++surfaceIndex )
    {
        Surface& surface = sector.surfaces[surfaceIndex];
        surface.vertices.assign(
            kSurfaceVertices[surfaceIndex].begin(),
            kSurfaceVertices[surfaceIndex].end());
        surface.texVertices.assign(surface.vertices.size(), TexVertex{});
    }
}

} // namespace

QRect startupDialogGeometryForAvailableScreen(
    const QRect& availableGeometry) noexcept
{
    const QSize preferredSize(
        kStartupDialogPreferredWidth,
        kStartupDialogPreferredHeight);
    if (!availableGeometry.isValid())
    {
        return QRect(QPoint{}, preferredSize);
    }

    const int horizontalMargin = std::min(
        kStartupDialogScreenMargin,
        std::max(0, (availableGeometry.width() - 1) / 2));
    const int verticalMargin = std::min(
        kStartupDialogScreenMargin,
        std::max(0, (availableGeometry.height() - 1) / 2));
    const QRect safeGeometry = availableGeometry.adjusted(
        horizontalMargin,
        verticalMargin,
        -horizontalMargin,
        -verticalMargin);
    const QSize fittedSize(
        std::min(preferredSize.width(), std::max(1, safeGeometry.width())),
        std::min(preferredSize.height(), std::max(1, safeGeometry.height())));
    return QRect(
        safeGeometry.x() + (safeGeometry.width() - fittedSize.width()) / 2,
        safeGeometry.y() + (safeGeometry.height() - fittedSize.height()) / 2,
        fittedSize.width(),
        fittedSize.height());
}

assets::LevLoadOptions defaultLevImportOptions()
{
    assets::LevLoadOptions options{};
    options.dfToJkFactor = 40.0;
    options.textureMode = assets::LevTextureMode::Keep;
    options.loadObjects = true;
    return options;
}

bool isValidLevImportScale(const double scale) noexcept
{
    return std::isfinite(scale) && scale > 0.0;
}

void applyNewProjectTypeDefaults(Level& level, const ProjectType kind)
{
    level.kind = kind;
    level.header = LevelHeader{};
    level.header.version = (kind == ProjectType::IJIM) ? 3 : 1;
    level.header.gravity = (kind == ProjectType::IJIM) ? 2.0 : 4.0;
    level.header.ceilingSky.height = 15.0;
    level.header.horizonSky.distance = 100.0;
    level.header.horizonSky.pixelsPerRev = 768.0;
    level.header.mipmapDistances = { 1.0, 2.0, 3.0, 4.0 };
    level.header.lodDistances = { 0.3, 0.6, 0.9, 1.2 };
    level.header.perspectiveDistance = 2.0;
    level.header.gouraudDistance = 2.0;
    level.header.fog.enabled = false;
    level.header.fog.color = ColorWhite;
    level.header.fog.fogStart = 0.0;
    level.header.fog.fogEnd = 250.0;
}

LevelExportBufferExtras defaultNewProjectExportBufferExtras(
    const ProjectType kind) noexcept
{
    // These are explicit editable extras now, not hidden writer floors. The
    // values retain the original SED writer's new-project headroom while a
    // user-entered zero is honored exactly on export.
    return LevelExportBufferExtras{
        .sounds = kind == ProjectType::IJIM ? 80u : 0u,
        .materials = kind == ProjectType::IJIM ? 64u : 0u,
        .aiClasses = 64,
        .models = 1024,
        .sprites = 128,
        .keyframes = 512,
        .puppets = 64,
        .soundClasses = 128,
        .cogScripts = 128,
        .cogs = 0,
        .templates = 0,
        .things = 256,
    };
}

LevelExportBufferExtras configuredNewProjectExportBufferExtras(
    const QSettings& settings,
    const ProjectType kind)
{
    LevelExportBufferExtras configured =
        defaultNewProjectExportBufferExtras(kind);
    const QString prefix = exportBufferSettingsPrefix(kind);
    for (const ExportBufferSetting& field : kExportBufferSettings)
    {
        const qulonglong fallback = configured.*(field.member);
        bool ok = false;
        const qulonglong stored = settings.value(
            prefix + QString::fromLatin1(field.name), fallback).toULongLong(&ok);
        configured.*(field.member) = static_cast<std::uint32_t>(
            std::min<qulonglong>(
                ok ? stored : fallback,
                std::numeric_limits<std::uint32_t>::max()));
    }
    return configured;
}

void writeConfiguredNewProjectExportBufferExtras(
    QSettings& settings,
    const ProjectType kind,
    const LevelExportBufferExtras& extras)
{
    const LevelExportBufferExtras defaults =
        defaultNewProjectExportBufferExtras(kind);
    const QString prefix = exportBufferSettingsPrefix(kind);
    for (const ExportBufferSetting& field : kExportBufferSettings)
    {
        const QString key = prefix + QString::fromLatin1(field.name);
        if (extras.*(field.member) == defaults.*(field.member))
        {
            settings.remove(key);
        }
        else
        {
            settings.setValue(key, extras.*(field.member));
        }
    }
}

void seedDefaultNewProjectLevel(
    Level& outLevel,
    const level::UvTextureDimensions materialDimensions)
{
    // Preserve Pascal NewProject's 2x2x1 starter-room bounds and ground plane.
    // The C++ flow keeps its project-aware player resources and resolved
    // palette, then applies the normal resource-derived player insertion
    // offset.
    outLevel.exportBufferExtras =
        defaultNewProjectExportBufferExtras(outLevel.kind);
    outLevel.layers = { "Main" };
    outLevel.masterCMP = "dflt.cmp";
    outLevel.colormaps = { "dflt.cmp" };
    outLevel.materials = { "dflt.mat" };

    // The starter room deliberately recreates Pascal's pixel-space mapping
    // before converting the complete document to normalized UVs below.
    outLevel.uvCoordinateSpace = UvCoordinateSpace::TexturePixels;
    Sector& sector = level::levelCreateCube(
        outLevel,
        -1.0,
        -1.0,
        0.0,
        TVector{ 0.0, 0.0, 1.0 },
        TVector{ 2.0, 0.0, 0.0 });

    for (Vertex& vertex : sector.vertices)
    {
        if (vertex.z > 0.0)
        {
            vertex.z = 1.0;
        }
    }

    if ( outLevel.kind == ProjectType::JKDF2
        || outLevel.kind == ProjectType::MOTS )
    {
        applyClassicStarterRoomUvTopology(sector);
    }

    // Build the deterministic starter mapping first, then publish the Level
    // only after the complete mapping and its scale metadata have converted to
    // the canonical normalized space.
    const double ppu = level::levelPixelPerUnit(outLevel);
    for (Surface& surface : sector.surfaces)
    {
        if ( outLevel.kind == ProjectType::JKDF2
            || outLevel.kind == ProjectType::MOTS )
        {
            // Match Pascal TJedMain.NewProject, which creates zeroed texture
            // vertices and invokes RecalcAll rather than NewRecalcAll.
            level::surfaceRecalcAll(ppu, sector, surface);
        }
        else
        {
            level::surfaceRecalcNewAll(ppu, sector, surface, 0);
        }
    }

    const level::LevelUvConversionResult converted =
        level::convertLevelUvCoordinateSpace(
            outLevel,
            UvCoordinateSpace::Normalized,
            [materialDimensions](const Surface&)
                -> std::optional<level::UvTextureDimensions>
            {
                return materialDimensions;
            });
    if (!converted)
    {
        throw std::runtime_error(
            "Cannot create the starter level because the default material dimensions are invalid.");
    }

    sector.layer = 0;
    sector.colormap = outLevel.masterCMP;
    sector.ambient = ColorWhite;

    Thing& player = outLevel.newThing();
    player.sector = sector.num;
    player.layer = 0;
    player.x = 0.0;
    player.y = 0.0;
    player.z = 0.0;
    player.templateName = (outLevel.kind == ProjectType::IJIM) ? std::string("shirtplayer") : std::string("walkplayer");
    player.name = player.templateName;

    // Match the original SED starter level: SedLight defaults provide a white
    // intensity-10, range-2 baked light at the center of the room.
    SedLight& light = outLevel.newLight();
    light.layer = 0;
    light.position = level::sectorCalcCenter(sector);
}

QString projectTypeLabel(const ProjectType kind)
{
    switch (kind)
    {
        case ProjectType::JKDF2: return QStringLiteral("Jedi Knight: Dark Forces II");
        case ProjectType::MOTS: return QStringLiteral("Mysteries of the Sith");
        case ProjectType::IJIM: return QStringLiteral("Indiana Jones and the Infernal Machine");
        default: return QStringLiteral("Unknown project");
    }
}

bool supportsJedLevelExport(const ProjectType kind) noexcept
{
    return kind == ProjectType::JKDF2 || kind == ProjectType::MOTS;
}

QString nativeRuntimeLevelSuffix(const ProjectType kind)
{
    return (kind == ProjectType::IJIM) ? QStringLiteral(".ndy") : QStringLiteral(".jkl");
}

QString levelSaveDialogFilter(const ProjectType kind)
{
    (void)kind;
    return QStringLiteral("Sith Editor File (*.sed)");
}

QString levelExportDialogFilter(const ProjectType kind)
{
    if (kind == ProjectType::IJIM)
    {
        return QStringLiteral("Infernal Machine Level File (*.ndy)");
    }
    return QStringLiteral(
        "Jedi Knight / Mysteries of the Sith Level Files (*.jkl *.jed);;"
        "Jedi Knight / Mysteries of the Sith Runtime Level File (*.jkl);;"
        "JED Level Editor File (*.jed)");
}

QString levelOpenDialogFilter()
{
    return QStringLiteral(
        "All supported files (*.sed *.jkl *.ndy *.jed *.asc *.lev);;"
        "Sith Editor File (*.sed);;"
        "Engine Level Files (*.jkl *.ndy *.jed);;"
        "Legacy Geometry Files (*.asc *.lev)");
}

QString projectInstallPathMismatchMessage(
    const ProjectType expectedKind,
    const std::filesystem::path& installDir,
    const ui::resources::ProjectInstallPathValidation& validation)
{
    if (validation.match
            != ui::resources::ProjectInstallPathMatch::mismatched
        || !validation.detectedKind)
    {
        return {};
    }

    const QString selectedPath = QDir::toNativeSeparators(
        QString::fromStdWString(installDir.wstring()));
    const QString signature = QString::fromStdWString(
        validation.signaturePath.filename().wstring());
    return QStringLiteral(
        "The selected folder appears to contain %1 resources (%2), not %3 resources.\n\n"
        "Selected folder:\n%4\n\n"
        "Choose the %3 game install or Resource directory. Custom resource folders are allowed when they do not clearly identify another supported game.")
        .arg(
            projectTypeLabel(*validation.detectedKind),
            signature,
            projectTypeLabel(expectedKind),
            selectedPath);
}

bool isSupportedLevelOpenPath(const std::filesystem::path& path)
{
    const std::string ext = text::lowerAscii(path.extension().string());
    return ext == ".sed" || ext == ".jkl" || ext == ".ndy"
        || ext == ".jed" || ext == ".asc" || ext == ".lev";
}

bool isNativeLevelSavePath(const std::filesystem::path& path, const ProjectType kind)
{
    (void)kind;
    const std::string ext = text::lowerAscii(path.extension().string());
    return ext == ".sed";
}

bool isEngineLevelExportPath(const std::filesystem::path& path, const ProjectType kind)
{
    const std::string ext = text::lowerAscii(path.extension().string());
    if (ext == ".ndy")
    {
        return kind == ProjectType::IJIM;
    }
    if (ext == ".jkl")
    {
        return kind == ProjectType::JKDF2 || kind == ProjectType::MOTS;
    }
    if (ext == ".jed")
    {
        return supportsJedLevelExport(kind);
    }
    return false;
}

bool recentLevelListContainsPath(const QStringList& paths, const QString& needle)
{
    return std::any_of(paths.begin(), paths.end(), [&](const QString& path)
        {
            return QString::compare(path, needle, Qt::CaseInsensitive) == 0;
        });
}

QString matchingRecentLevelPath(const QStringList& recentLevels, const QString& needle)
{
    for (const QString& recentPath : recentLevels)
    {
        const QString trimmed = recentPath.trimmed();
        if (QString::compare(trimmed, needle, Qt::CaseInsensitive) == 0)
        {
            return trimmed;
        }
    }
    return {};
}

void removeRecentLevelPath(QStringList& paths, const QString& needle)
{
    for (int i = paths.size() - 1; i >= 0; --i)
    {
        if (QString::compare(paths[i], needle, Qt::CaseInsensitive) == 0)
        {
            paths.removeAt(i);
        }
    }
}

QStringList sanitizedPinnedRecentLevels(const QStringList& recentLevels, const QStringList& pinnedLevels)
{
    QStringList sanitizedPinned;
    for (const QString& pinnedPath : pinnedLevels)
    {
        const QString trimmed = pinnedPath.trimmed();
        const QString canonicalRecentPath = matchingRecentLevelPath(recentLevels, trimmed);
        if (trimmed.isEmpty()
            || canonicalRecentPath.isEmpty()
            || recentLevelListContainsPath(sanitizedPinned, canonicalRecentPath))
        {
            continue;
        }

        sanitizedPinned.push_back(canonicalRecentPath);
    }
    return sanitizedPinned;
}

QStringList recentLevelsWithPinnedFirst(const QStringList& recentLevels, const QStringList& pinnedLevels)
{
    QStringList ordered = sanitizedPinnedRecentLevels(recentLevels, pinnedLevels);
    ordered.reserve(recentLevels.size());

    for (const QString& recentPath : recentLevels)
    {
        const QString trimmed = recentPath.trimmed();
        if (trimmed.isEmpty() || recentLevelListContainsPath(ordered, trimmed))
        {
            continue;
        }
        ordered.push_back(trimmed);
    }

    return ordered;
}

QStringList pinnedRecentLevelsAfterToggle(
    const QStringList& recentLevels,
    const QStringList& pinnedLevels,
    const QString& toggledPath,
    const bool pinned)
{
    QStringList pins = sanitizedPinnedRecentLevels(recentLevels, pinnedLevels);
    removeRecentLevelPath(pins, toggledPath);

    const QString trimmedPath = toggledPath.trimmed();
    const QString canonicalRecentPath = matchingRecentLevelPath(recentLevels, trimmedPath);
    if (pinned && !canonicalRecentPath.isEmpty())
    {
        pins.push_back(canonicalRecentPath);
    }

    return pins;
}

bool installDirExists(const std::optional<std::filesystem::path>& dir) noexcept
{
    if (!dir || dir->empty())
    {
        return false;
    }

    std::error_code ec;
    return std::filesystem::exists(*dir, ec) && !ec;
}

ProjectInstallStatus::ProjectInstallStatus(
    const ProjectType kind,
    std::optional<std::filesystem::path> configured,
    std::optional<std::filesystem::path> detected)
    : kind_(kind)
    , configured_(std::move(configured))
    , detected_(std::move(detected))
    , configuredPathValidation_(configured_
          ? ui::resources::validateProjectInstallPath(kind_, *configured_)
          : ui::resources::ProjectInstallPathValidation{})
    , detectedPathValidation_(detected_
          ? ui::resources::validateProjectInstallPath(kind_, *detected_)
          : ui::resources::ProjectInstallPathValidation{})
    , configuredValid_(installDirExists(configured_)
          && configuredPathValidation_.canUse())
    , detectedValid_(installDirExists(detected_)
          && detectedPathValidation_.canUse())
{
}

QString ProjectInstallStatus::statusObjectName() const
{
    if (configured_ && !configuredValid_)
    {
        return detectedValid_ ? QStringLiteral("startup_status_warn") : QStringLiteral("startup_status_bad");
    }

    if (configured_ && configuredValid_)
    {
        return QStringLiteral("startup_status_ok");
    }

    if (detected_ && detectedValid_)
    {
        return QStringLiteral("startup_status_warn");
    }

    return QStringLiteral("startup_status_bad");
}

QString ProjectInstallStatus::statusText() const
{
    const auto nativePath = [](const std::filesystem::path& path) -> QString
    {
        return QDir::toNativeSeparators(QString::fromStdWString(path.wstring()));
    };

    if (configured_ && installDirExists(configured_)
        && !configuredPathValidation_.canUse())
    {
        QString text = projectInstallPathMismatchMessage(
            kind_, *configured_, configuredPathValidation_);
        if (detectedValid_
            && detected_
            && nativePath(*configured_).compare(
                   nativePath(*detected_), Qt::CaseInsensitive) != 0)
        {
            text += QString("\n\nA matching install was auto-detected: %1")
                .arg(nativePath(*detected_));
        }
        return text;
    }

    if (configured_ && !configuredValid_ && detectedValid_)
    {
        return QString("Configured install directory is missing, but a valid install was auto-detected: %1")
            .arg(nativePath(*detected_));
    }

    if (configured_ && !configuredValid_)
    {
        return QString("Configured install directory is missing: %1")
            .arg(nativePath(*configured_));
    }

    if (configured_ && configuredValid_)
    {
        return nativePath(*configured_);
    }

    if (detected_ && detectedValid_)
    {
        return QString("Auto-detected install directory: %1")
            .arg(nativePath(*detected_));
    }

    return QStringLiteral("No valid install/resource path configured.");
}

} // namespace sed::ui::project
