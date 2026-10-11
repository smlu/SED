#include "test_common.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <filesystem>
#include <memory>
#include <optional>
#include <ostream>
#include <span>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include <QDir>
#include <QFile>
#include <QTemporaryDir>

#include "sed/assets/jkl.hpp"
#include "sed/assets/resource_locator.hpp"
#include "sed/assets/sed.hpp"
#include "ui/recovery/partial_level_salvage.hpp"

namespace sed::ui::recovery::detail
{

std::size_t maximumPartialLevelNativeTextBytesForTesting() noexcept;
qsizetype maximumPartialLevelManifestCharactersForTesting() noexcept;

} // namespace sed::ui::recovery::detail

namespace sed::app_tests
{
namespace
{

QByteArray readFile(const std::filesystem::path& path)
{
    QFile file(QString::fromStdWString(path.wstring()));
    if (!file.open(QIODevice::ReadOnly)) return {};
    return file.readAll();
}

void appendLittleU32(QByteArray& bytes, const std::uint32_t value)
{
    bytes.push_back(static_cast<char>(value & 0xffu));
    bytes.push_back(static_cast<char>((value >> 8u) & 0xffu));
    bytes.push_back(static_cast<char>((value >> 16u) & 0xffu));
    bytes.push_back(static_cast<char>((value >> 24u) & 0xffu));
}

QByteArray indexedMatFixture(
    const std::int32_t width,
    const std::int32_t height)
{
    QByteArray bytes("MAT ", 4);
    appendLittleU32(bytes, 0x32u);
    appendLittleU32(bytes, 2u);
    appendLittleU32(bytes, 1u);
    appendLittleU32(bytes, 1u);

    appendLittleU32(bytes, 1u);
    appendLittleU32(bytes, 8u);
    for (std::size_t index = 0; index < 12u; ++index)
    {
        appendLittleU32(bytes, 0u);
    }

    appendLittleU32(bytes, 8u);
    appendLittleU32(bytes, 0u);
    for (std::size_t index = 0; index < 8u; ++index)
    {
        appendLittleU32(bytes, 0u);
    }

    appendLittleU32(bytes, static_cast<std::uint32_t>(width));
    appendLittleU32(bytes, static_cast<std::uint32_t>(height));
    appendLittleU32(bytes, 0u);
    appendLittleU32(bytes, 0u);
    appendLittleU32(bytes, 0u);
    appendLittleU32(bytes, 1u);
    bytes.resize(
        bytes.size()
        + static_cast<qsizetype>(width) * static_cast<qsizetype>(height));
    return bytes;
}

bool writeFile(
    const std::filesystem::path& path,
    const QByteArray& bytes)
{
    QFile file(QString::fromStdWString(path.wstring()));
    return file.open(QIODevice::WriteOnly)
        && file.write(bytes) == bytes.size()
        && file.flush();
}

Level salvageFixture(const ProjectType project)
{
    Level level{};
    level.kind = project;
    // This fixture exercises native-text salvage itself, not resource-backed
    // UV migration. Match each runtime format's wire space explicitly so the
    // classic JKL cases remain exact without inventing a MAT dimension.
    level.uvCoordinateSpace = project == ProjectType::IJIM
        ? UvCoordinateSpace::Normalized
        : UvCoordinateSpace::TexturePixels;
    level.header.version = project == ProjectType::IJIM ? 3 : 1;
    level.masterCMP = project == ProjectType::IJIM ? "" : "dflt.cmp";
    level.layers.push_back("Main");
    level.materials.push_back("salvaged.mat");

    ThingTemplate thingTemplate{};
    thingTemplate.name = "salvaged_template";
    TplValue unknownParameter{};
    unknownParameter.name = "unknown_param";
    unknownParameter.type = ValueType::str;
    unknownParameter.stringValue = "preserved_string";
    unknownParameter.adjType = AdjType::unk;
    thingTemplate.values.push_back(unknownParameter);
    level.templates.push_back(std::move(thingTemplate));

    Sector& sector = level.newSector();
    sector.colormap = project == ProjectType::IJIM ? "" : "dflt.cmp";
    sector.addVertex(0.0, 0.0, 0.0);
    sector.addVertex(1.0, 0.0, 0.0);
    sector.addVertex(0.0, 1.0, 0.0);
    Surface surface{};
    surface.material = "salvaged.mat";
    surface.vertices = { 0, 1, 2 };
    surface.texVertices = {
        TexVertex{ 0.0f, 0.0f, ColorWhite },
        TexVertex{ 1.0f, 0.0f, ColorWhite },
        TexVertex{ 0.0f, 1.0f, ColorWhite },
    };
    sector.addSurface(std::move(surface));

    Thing& thing = level.newThing();
    thing.templateName = "salvaged_template";
    thing.name = "salvaged_instance";
    thing.sector = 0;
    thing.layer = 0;
    return level;
}

ui::recovery::PartialLevelSalvageContext salvageContext(
    const std::filesystem::path& directory,
    const QString& artifactId,
    const QStringList& missing = { QStringLiteral("lite") })
{
    ui::recovery::PartialLevelSalvageContext context{};
    context.sourceRecoveryPath = directory / "selected-damaged.sedrecovery";
    context.forensicRecoveryPath = directory / "selected-damaged.corrupted.sedrecovery";
    context.missingRequiredSections = missing;
    context.recoveryWarnings = {
        QStringLiteral("Native SED V1 lost section LITE."),
        QStringLiteral("Three shards in the section were unreadable."),
    };
    context.artifactId = artifactId;
    return context;
}

} // namespace

void runPartialLevelSalvageTests()
{
    check(ui::recovery::detail::maximumPartialLevelNativeTextBytesForTesting()
            == assets::SedMaximumFileBytes
            && assets::SedMaximumFileBytes == 256ull * 1024ull * 1024ull,
        "partial_level_salvage bounds automatic native-text disk and CPU work to the shared 256 MiB file limit");
    check(ui::recovery::detail::maximumPartialLevelManifestCharactersForTesting()
            == 2 * 1024 * 1024,
        "partial_level_salvage bounds the diagnostic manifest independently of recovered content");

    QTemporaryDir temporaryDirectory{};
    check(temporaryDirectory.isValid(),
        "partial_level_salvage creates an isolated directory");
    if (!temporaryDirectory.isValid()) return;
    const std::filesystem::path root(temporaryDirectory.path().toStdWString());

#ifdef Q_OS_WIN
    {
        const std::filesystem::path longPathDirectory = root
            / std::string(48, 'd');
        check(QDir().mkpath(QString::fromStdWString(
                  longPathDirectory.wstring())),
            "partial_level_salvage creates the deterministic long-path vector directory");
        ui::recovery::PartialLevelSalvageContext longPathContext =
            salvageContext(longPathDirectory, QString(80, QLatin1Char('a')));
        longPathContext.forensicRecoveryPath = longPathDirectory
            / (std::string(80, 'f') + ".sedrecovery");
        const auto longPathResult = ui::recovery::writePartialLevelSalvage(
            salvageFixture(ProjectType::JKDF2), longPathContext);
        if (!longPathResult.wroteManifest()
            || !longPathResult.wroteNativeText())
        {
            std::cerr << "Long-path salvage diagnostic: "
                      << longPathResult.error.toStdString()
                      << " | native: "
                      << longPathResult.nativeExportFailure.toStdString()
                      << "\n";
        }
        check(longPathResult.wroteManifest(),
            "partial_level_salvage atomically publishes a manifest beyond the legacy Win32 MAX_PATH limit");
        check(longPathResult.wroteNativeText(),
            "partial_level_salvage atomically publishes native text beyond the legacy Win32 MAX_PATH limit");
        check(longPathResult.manifestPath.wstring().size() > 260,
            "partial_level_salvage long-path vector exceeds the legacy Win32 MAX_PATH limit");
    }
#endif

    QString controlText = QStringLiteral("first\r\nsecond\t");
    controlText += QChar(0x0000);
    controlText += QChar(0x001b); // ESC; also neutralizes two-byte ESC/CSI forms.
    controlText += QChar(0x009b); // Unicode CSI.
    controlText += QChar(0x061c); // Arabic letter mark.
    controlText += QChar(0x2028); // Unicode line separator.
    controlText += QChar(0x202e); // Right-to-left override.
    controlText += QChar(0x2067); // Right-to-left isolate.
    controlText += QChar(0xfeff); // Zero-width no-break/BOM format control.
    bool escapedAuditTextTruncated = false;
    const QString escapedAuditText = ui::recovery::detail::escapeForensicText(
        controlText,
        256,
        ui::recovery::detail::ForensicNewlinePolicy::replaceWithSpace,
        &escapedAuditTextTruncated);
    check(!escapedAuditTextTruncated
            && escapedAuditText == QStringLiteral(
                "first  second\\u0009\\u0000\\u001B\\u009B\\u061C"
                "\\u2028\\u202E\\u2067\\uFEFF"),
        "partial_level_salvage complete fixed escaped audit string and truncation OUT vector");
    bool cappedEscapeTruncated = false;
    const QString cappedEscape = ui::recovery::detail::escapeForensicText(
        QString(3, QChar(0x001b)),
        12,
        ui::recovery::detail::ForensicNewlinePolicy::replaceWithSpace,
        &cappedEscapeTruncated);
    check(cappedEscapeTruncated
            && cappedEscape == QStringLiteral("\\u001B\\u001B")
            && cappedEscape.size() == 12,
        "partial_level_salvage applies forensic-text caps after visible escaping without emitting partial escapes");

    const QStringList lost = ui::recovery::lostRequiredLevelSections({
        QStringLiteral("edit"),
        QStringLiteral("sect"),
        QStringLiteral("hnod"),
        QStringLiteral("not-a-section"),
    });
    check(lost == QStringList({ QStringLiteral("sect"), QStringLiteral("hnod") }),
        "partial_level_salvage extracts only exact required Level losses");

    struct ProjectCase
    {
        ProjectType project{};
        const char* stem{};
        const char* name{};
        const char* extension{};
    };
    constexpr std::array<ProjectCase, 3> projects{{
        { ProjectType::JKDF2, "jkdf2", "JKDF2", ".jkl" },
        { ProjectType::MOTS, "mots", "MotS", ".jkl" },
        { ProjectType::IJIM, "ijim", "IJIM", ".ndy" },
    }};
    for (const ProjectCase& project : projects)
    {
        const Level fixture = salvageFixture(project.project);
        const std::filesystem::path directory = root / project.name;
        QDir().mkpath(QString::fromStdWString(directory.wstring()));
        const auto result = ui::recovery::writePartialLevelSalvage(
            fixture,
            salvageContext(directory, QString::fromLatin1(project.name)));
        const QByteArray manifest = readFile(result.manifestPath);
        check(result.wroteManifest() && result.wroteNativeText()
                && result.nativeTextPath.extension() == project.extension,
            "partial_level_salvage writes project-native JKL/NDY plus a manifest");
        check(manifest.contains("STATUS: NON-AUTHORITATIVE")
                && manifest.contains(project.name)
                && manifest.contains("Missing required Level sections:\n  - lite")
                && manifest.contains("Retained forensic input:")
                && manifest.contains("Manual text edits are speculative"),
            "partial_level_salvage manifest records provenance, exact loss, and limitations");
        check(readFile(result.nativeTextPath).contains("SECTION:"),
            "partial_level_salvage native export is human-readable engine text");

        // Salvage intentionally uses the readable, non-compact writer even
        // though normal runtime export defaults to Compact in the UI.  Keep
        // this on the real artifact path so a future writer-default change
        // cannot silently turn forensic output into a packed stream.
        std::ostringstream readableExpected{};
        assets::JklSaveOptions readableOptions{};
        readableOptions.compact = false;
        assets::saveToJKL(fixture, readableExpected, readableOptions);
        std::ostringstream compactExpected{};
        assets::JklSaveOptions compactOptions{};
        compactOptions.compact = true;
        assets::saveToJKL(fixture, compactExpected, compactOptions);
        const QByteArray salvagedText = readFile(result.nativeTextPath);
        check(salvagedText == QByteArray::fromStdString(readableExpected.str())
                && readableExpected.str() != compactExpected.str(),
            "partial_level_salvage keeps the full readable JKL/NDY presentation rather than compact export formatting");
    }

    const std::filesystem::path normalizedClassicDirectory =
        root / "normalized-classic-with-resources";
    QDir().mkpath(QString::fromStdWString(
        normalizedClassicDirectory.wstring()));
    check(writeFile(
            normalizedClassicDirectory / "salvaged.mat",
            indexedMatFixture(64, 32)),
        "partial_level_salvage creates an anisotropic MAT dimension fixture");
    auto mutableResources = std::make_shared<assets::ResourceLocator>();
    mutableResources->addRoot(normalizedClassicDirectory);
    const std::shared_ptr<const assets::ResourceLocator> resources =
        std::move(mutableResources);
    Level normalizedClassicFixture = salvageFixture(ProjectType::JKDF2);
    normalizedClassicFixture.uvCoordinateSpace = UvCoordinateSpace::Normalized;
    const std::vector<TexVertex> sourceCoordinates =
        normalizedClassicFixture.sectors.front().surfaces.front().texVertices;
    const float sourceUScale =
        normalizedClassicFixture.sectors.front().surfaces.front().uScale;
    const float sourceVScale =
        normalizedClassicFixture.sectors.front().surfaces.front().vScale;
    ui::recovery::PartialLevelSalvageContext normalizedClassicContext =
        salvageContext(
            normalizedClassicDirectory,
            QStringLiteral("normalized-classic-with-resources"));
    normalizedClassicContext.nativeExportResources = resources;
    const auto normalizedClassicSalvage =
        ui::recovery::writePartialLevelSalvage(
            normalizedClassicFixture,
            normalizedClassicContext);
    Level normalizedClassicParsed{};
    bool normalizedClassicParsedSuccessfully = false;
    if (normalizedClassicSalvage.wroteNativeText())
    {
        try
        {
            assets::JklLoadOptions options{};
            options.projectType = ProjectType::JKDF2;
            normalizedClassicParsed = assets::loadFromJKL(
                normalizedClassicSalvage.nativeTextPath,
                options);
            normalizedClassicParsedSuccessfully = true;
        }
        catch (...)
        {
            normalizedClassicParsedSuccessfully = false;
        }
    }
    const Surface* parsedSurface = normalizedClassicParsedSuccessfully
            && !normalizedClassicParsed.sectors.empty()
            && !normalizedClassicParsed.sectors.front().surfaces.empty()
        ? &normalizedClassicParsed.sectors.front().surfaces.front()
        : nullptr;
    const QByteArray normalizedClassicNativeText =
        readFile(normalizedClassicSalvage.nativeTextPath);
    const QByteArray expectedClassicTextureVertices = QByteArrayLiteral(
        "WORLD TEXTURE VERTICES 3\n"
        "0: 0.00000000 0.00000000\n"
        "1: 64.00000000 0.00000000\n"
        "2: 0.00000000 32.00000000\n");
    check(normalizedClassicSalvage.wroteManifest()
            && normalizedClassicSalvage.wroteNativeText()
            && normalizedClassicNativeText.contains(
                expectedClassicTextureVertices)
            && parsedSurface != nullptr
            && normalizedClassicParsed.uvCoordinateSpace
                == UvCoordinateSpace::TexturePixels
            && parsedSurface->texVertices.size() == 3u
            && close(parsedSurface->texVertices[1].u, 64.0)
            && close(parsedSurface->texVertices[2].v, 32.0),
        "partial_level_salvage exports a normalized SED V2 classic Level through an immutable anisotropic MAT resource snapshot");
    const Surface& unchangedSource =
        normalizedClassicFixture.sectors.front().surfaces.front();
    const bool sourceCoordinatesUnchanged =
        unchangedSource.texVertices.size() == sourceCoordinates.size()
        && std::equal(
            unchangedSource.texVertices.cbegin(),
            unchangedSource.texVertices.cend(),
            sourceCoordinates.cbegin(),
            [](const TexVertex& lhs, const TexVertex& rhs)
            {
                return lhs.u == rhs.u
                    && lhs.v == rhs.v
                    && lhs.color.r == rhs.color.r
                    && lhs.color.g == rhs.color.g
                    && lhs.color.b == rhs.color.b
                    && lhs.color.a == rhs.color.a;
            });
    check(normalizedClassicFixture.uvCoordinateSpace
                == UvCoordinateSpace::Normalized
            && sourceCoordinatesUnchanged
            && unchangedSource.uScale == sourceUScale
            && unchangedSource.vScale == sourceVScale,
        "partial_level_salvage converts a private native-export copy without mutating the recovered normalized Level");

    Level pixelIjimFixture = salvageFixture(ProjectType::IJIM);
    pixelIjimFixture.uvCoordinateSpace = UvCoordinateSpace::TexturePixels;
    ui::recovery::PartialLevelSalvageContext pixelIjimContext = salvageContext(
        normalizedClassicDirectory,
        QStringLiteral("pixel-ijim-with-resources"));
    pixelIjimContext.nativeExportResources = resources;
    const auto pixelIjimSalvage = ui::recovery::writePartialLevelSalvage(
        pixelIjimFixture,
        pixelIjimContext);
    Level pixelIjimParsed{};
    bool pixelIjimParsedSuccessfully = false;
    if (pixelIjimSalvage.wroteNativeText())
    {
        try
        {
            assets::JklLoadOptions options{};
            options.projectType = ProjectType::IJIM;
            pixelIjimParsed = assets::loadFromJKL(
                pixelIjimSalvage.nativeTextPath,
                options);
            pixelIjimParsedSuccessfully = true;
        }
        catch (...)
        {
            pixelIjimParsedSuccessfully = false;
        }
    }
    const Surface* pixelIjimSurface = pixelIjimParsedSuccessfully
            && !pixelIjimParsed.sectors.empty()
            && !pixelIjimParsed.sectors.front().surfaces.empty()
        ? &pixelIjimParsed.sectors.front().surfaces.front()
        : nullptr;
    const QByteArray pixelIjimNativeText =
        readFile(pixelIjimSalvage.nativeTextPath);
    const QByteArray expectedIjimTextureVertices = QByteArrayLiteral(
        "WORLD TEXTURE VERTICES 3\n"
        "0: 0.00000000 0.00000000\n"
        "1: 0.01562500 0.00000000\n"
        "2: 0.00000000 0.03125000\n");
    check(pixelIjimSalvage.wroteManifest()
            && pixelIjimSalvage.wroteNativeText()
            && pixelIjimNativeText.contains(expectedIjimTextureVertices)
            && pixelIjimSurface != nullptr
            && pixelIjimParsed.uvCoordinateSpace
                == UvCoordinateSpace::Normalized
            && pixelIjimSurface->texVertices.size() == 3u
            && close(pixelIjimSurface->texVertices[1].u, 1.0 / 64.0)
            && close(pixelIjimSurface->texVertices[2].v, 1.0 / 32.0)
            && pixelIjimFixture.uvCoordinateSpace
                == UvCoordinateSpace::TexturePixels,
        "partial_level_salvage exports a legacy pixel-space SED V1 IJIM Level to normalized NDY without mutating its compatibility snapshot");

    const std::filesystem::path unresolvedUvDirectory =
        root / "normalized-classic-without-resources";
    QDir().mkpath(
        QString::fromStdWString(unresolvedUvDirectory.wstring()));
    Level unresolvedUvFixture = salvageFixture(ProjectType::JKDF2);
    unresolvedUvFixture.uvCoordinateSpace = UvCoordinateSpace::Normalized;
    const auto unresolvedUvSalvage =
        ui::recovery::writePartialLevelSalvage(
            unresolvedUvFixture,
            salvageContext(
                unresolvedUvDirectory,
                QStringLiteral("normalized-classic-without-resources")));
    const QByteArray unresolvedUvManifest =
        readFile(unresolvedUvSalvage.manifestPath);
    const bool unresolvedNativeArtifactAbsent = QDir(
        QString::fromStdWString(unresolvedUvDirectory.wstring()))
        .entryInfoList({ QStringLiteral("*.jkl") }, QDir::Files)
        .isEmpty();
    check(unresolvedUvSalvage.wroteManifest()
            && !unresolvedUvSalvage.wroteNativeText()
            && unresolvedNativeArtifactAbsent
            && unresolvedUvSalvage.nativeExportFailure.contains(
                QStringLiteral("salvaged.mat"))
            && unresolvedUvSalvage.nativeExportFailure.contains(
                QStringLiteral("MAT dimensions are unavailable"))
            && unresolvedUvManifest.contains(
                "Native text salvage: not written")
            && unresolvedUvManifest.contains("salvaged.mat")
            && unresolvedUvFixture.uvCoordinateSpace
                == UvCoordinateSpace::Normalized
            && close(unresolvedUvFixture.sectors.front().surfaces.front()
                    .texVertices[1].u,
                1.0),
        "partial_level_salvage refuses to guess MAT dimensions when a normalized classic Level cannot be converted exactly and still publishes its manifest");

    for (const ProjectCase& project : projects)
    {
        const std::filesystem::path vectorRoot =
            "src/core/tests/assets/fixtures/sed_vectors";
        const QByteArray corrupted = readFile(vectorRoot
            / (std::string(project.stem) + "_sect_unrecoverable.sed"));
        const QByteArray expectedPartial = readFile(vectorRoot
            / (std::string(project.stem) + "_expected_partial"
                + project.extension));
        std::optional<assets::SedRecoveryLoadResult> recoveredResult{};
        try
        {
            if (!corrupted.isEmpty())
            {
                recoveredResult = assets::recoverFromSedBytes(std::as_bytes(
                    std::span(corrupted.constData(),
                        static_cast<std::size_t>(corrupted.size()))));
            }
        }
        catch (...)
        {
            recoveredResult.reset();
        }
        const assets::SedRecoveryLoadResult recovered = recoveredResult
            ? *recoveredResult : assets::SedRecoveryLoadResult{};
        const bool lostGeometry = recovered.partialRecoveryFromCorruption
            && std::find(recovered.missingRequiredSections.begin(),
                    recovered.missingRequiredSections.end(), "sect")
                != recovered.missingRequiredSections.end();
        const TplValue* unknownParameter = nullptr;
        if (recovered.level.templates.size() == 1u)
        {
            const auto found = std::find_if(
                recovered.level.templates[0].values.begin(),
                recovered.level.templates[0].values.end(),
                [](const TplValue& value)
                {
                    return value.name == "unknown_param";
                });
            if (found != recovered.level.templates[0].values.end())
                unknownParameter = &*found;
        }
        const std::string projectLabel = project.name;
        check(lostGeometry && recovered.level.kind == project.project
                && recovered.level.sectors.empty()
                && recovered.level.templates.size() == 1u
                && recovered.level.things.size() == 1u
                && unknownParameter != nullptr
                && !unknownParameter->stringValue.empty(),
            ("partial_level_salvage " + projectLabel
                + " corruption vector loses over-budget sect while retaining healthy authored domains").c_str());

        const std::filesystem::path directory = root
            / (std::string("binary-corruption-") + project.name);
        QDir().mkpath(QString::fromStdWString(directory.wstring()));
        auto context = salvageContext(directory,
            QString::fromLatin1(project.name), { QStringLiteral("sect") });
        const auto result = ui::recovery::writePartialLevelSalvage(
            recovered.level, context);
        check(result.wroteManifest() && result.wroteNativeText()
                && result.nativeTextPath.extension() == project.extension
                && readFile(result.nativeTextPath) == expectedPartial
                && readFile(result.manifestPath).contains(
                    "Missing required Level sections:\n  - sect"),
            ("partial_level_salvage " + projectLabel
                + " checked-in corruption vector exports surviving Level data to the exact checked-in JKL/NDY text").c_str());
    }

    const std::filesystem::path headDirectory = root / "head-loss";
    QDir().mkpath(QString::fromStdWString(headDirectory.wstring()));
    const auto headLoss = ui::recovery::writePartialLevelSalvage(
        salvageFixture(ProjectType::MOTS),
        salvageContext(headDirectory, QStringLiteral("head"),
            { QStringLiteral("head") }));
    check(readFile(headLoss.manifestPath).contains(
            "HEAD was lost; the project shown above was inferred"),
        "partial_level_salvage calls out project inference after HEAD loss");

    const std::filesystem::path selectedProjectDirectory =
        root / "user-selected-project";
    QDir().mkpath(QString::fromStdWString(selectedProjectDirectory.wstring()));
    auto selectedProjectContext = salvageContext(
        selectedProjectDirectory, QStringLiteral("selected-project"),
        { QStringLiteral("head") });
    selectedProjectContext.projectTypeSource =
        ui::recovery::PartialLevelProjectTypeSource::userSelected;
    const auto selectedProject = ui::recovery::writePartialLevelSalvage(
        salvageFixture(ProjectType::MOTS), selectedProjectContext);
    check(selectedProject.wroteManifest() && selectedProject.wroteNativeText()
            && readFile(selectedProject.manifestPath).contains(
                "explicitly selected by the user"),
        "partial_level_salvage records explicit project selection before native export");

    const std::filesystem::path unknownProjectDirectory =
        root / "unknown-project";
    QDir().mkpath(QString::fromStdWString(unknownProjectDirectory.wstring()));
    auto unknownProjectContext = salvageContext(
        unknownProjectDirectory, QStringLiteral("unknown-project"),
        { QStringLiteral("head") });
    unknownProjectContext.projectTypeSource =
        ui::recovery::PartialLevelProjectTypeSource::unknown;
    const auto unknownProject = ui::recovery::writePartialLevelSalvage(
        salvageFixture(ProjectType::JKDF2), unknownProjectContext);
    const QByteArray unknownProjectManifest = readFile(
        unknownProject.manifestPath);
    check(unknownProject.wroteManifest() && !unknownProject.wroteNativeText()
            && unknownProject.nativeExportFailure.contains(
                QStringLiteral("project type is unknown"))
            && unknownProjectManifest.contains(
                "Unknown (checked header and HEAD metadata were lost)")
            && unknownProjectManifest.contains(
                "native JKL/NDY export was intentionally blocked"),
        "partial_level_salvage blocks native text export when checked project metadata is unknown");

    const std::filesystem::path unidentifiedDirectory =
        root / "unidentified-loss";
    QDir().mkpath(QString::fromStdWString(unidentifiedDirectory.wstring()));
    auto unidentifiedContext = salvageContext(
        unidentifiedDirectory, QStringLiteral("unidentified"), {});
    unidentifiedContext.unidentifiedMissingSectionCount = 2;
    const auto unidentified = ui::recovery::writePartialLevelSalvage(
        salvageFixture(ProjectType::MOTS), unidentifiedContext);
    const QByteArray unidentifiedManifest = readFile(
        unidentified.manifestPath);
    check(unidentified.wroteManifest() && unidentified.wroteNativeText()
            && unidentifiedManifest.contains(
                "Unidentified missing section units: 2")
            && unidentifiedManifest.contains(
                "Any one may have contained required Level data")
            && unidentifiedManifest.contains(
                "cannot be named or mapped back to a domain"),
        "partial_level_salvage emits decoded-domain artifacts and explicit uncertainty for unidentified-only loss");

    const std::filesystem::path ambiguousMotsDirectory = root / "ambiguous-mots";
    QDir().mkpath(QString::fromStdWString(ambiguousMotsDirectory.wstring()));
    Level ambiguousMots{};
    ambiguousMots.kind = ProjectType::MOTS;
    ambiguousMots.header.version = 1;
    const auto ambiguousMotsResult = ui::recovery::writePartialLevelSalvage(
        ambiguousMots,
        salvageContext(ambiguousMotsDirectory, QStringLiteral("ambiguous-mots")));
    check(ambiguousMotsResult.wroteNativeText()
            && readFile(ambiguousMotsResult.nativeTextPath).contains(
                "Section: ARCHLIGHTING")
            && readFile(ambiguousMotsResult.manifestPath).contains(
                "MotS and JKDF2 share JKL VERSION 1"),
        "partial_level_salvage retains checked MotS context when version-1 syntax lacks inferable surfaces");

    const std::filesystem::path failureDirectory = root / "export-failure";
    QDir().mkpath(QString::fromStdWString(failureDirectory.wstring()));
    ui::recovery::PartialLevelSalvageTestHooks failureHooks{};
    failureHooks.nativeExporter =
        [](const Level&, std::ostream&)
        {
            throw std::runtime_error("synthetic native export failure");
        };
    const auto failedExport = ui::recovery::writePartialLevelSalvage(
        salvageFixture(ProjectType::JKDF2),
        salvageContext(failureDirectory, QStringLiteral("failure")),
        failureHooks);
    const QByteArray failedManifest = readFile(failedExport.manifestPath);
    check(failedExport.wroteManifest() && !failedExport.wroteNativeText()
            && failedExport.nativeExportFailure == QStringLiteral("synthetic native export failure"),
        "partial_level_salvage retains a manifest when native export fails");
    check(failedManifest.contains("Native text salvage: not written")
            && failedManifest.contains("synthetic native export failure"),
        "partial_level_salvage explains native export failure in the manifest");

    const std::filesystem::path invalidDirectory = root / "invalid-export";
    QDir().mkpath(QString::fromStdWString(invalidDirectory.wstring()));
    ui::recovery::PartialLevelSalvageTestHooks emptyHooks{};
    emptyHooks.nativeExporter = [](const Level&, std::ostream&) {};
    const auto emptyExport = ui::recovery::writePartialLevelSalvage(
        salvageFixture(ProjectType::JKDF2),
        salvageContext(invalidDirectory, QStringLiteral("empty")),
        emptyHooks);
    check(emptyExport.wroteManifest() && !emptyExport.wroteNativeText()
            && emptyExport.nativeExportFailure == QStringLiteral("the native text writer produced empty output"),
        "partial_level_salvage rejects an empty native export despite permissive JKL parsing");

    ui::recovery::PartialLevelSalvageTestHooks garbageHooks{};
    garbageHooks.nativeExporter = [](const Level&, std::ostream& out)
    {
        out << "this is not a canonical JKL or NDY level\n";
    };
    const auto garbageExport = ui::recovery::writePartialLevelSalvage(
        salvageFixture(ProjectType::MOTS),
        salvageContext(invalidDirectory, QStringLiteral("garbage")),
        garbageHooks);
    check(garbageExport.wroteManifest() && !garbageExport.wroteNativeText()
            && garbageExport.nativeExportFailure == QStringLiteral("native text must contain exactly one SECTION: COPYRIGHT anchor"),
        "partial_level_salvage rejects parse-tolerated garbage without required structural anchors");

    const std::filesystem::path raceDirectory = root / "publication-race";
    QDir().mkpath(QString::fromStdWString(raceDirectory.wstring()));
    std::filesystem::path racedTarget{};
    const QByteArray raceSentinel("concurrent owner must survive\n");
    ui::recovery::PartialLevelSalvageTestHooks raceHooks{};
    raceHooks.beforePublish = [&racedTarget, &raceSentinel](
        const std::filesystem::path& target)
    {
        if (target.extension() != ".jkl" || !racedTarget.empty()) return;
        racedTarget = target;
        QFile competitor(QString::fromStdWString(target.wstring()));
        if (competitor.open(QIODevice::WriteOnly | QIODevice::NewOnly))
        {
            (void)competitor.write(raceSentinel);
            competitor.close();
        }
    };
    const auto raced = ui::recovery::writePartialLevelSalvage(
        salvageFixture(ProjectType::JKDF2),
        salvageContext(raceDirectory, QStringLiteral("race")),
        raceHooks);
    check(raced.wroteManifest() && !raced.wroteNativeText()
            && !racedTarget.empty() && readFile(racedTarget) == raceSentinel,
        "partial_level_salvage atomic no-replace publication preserves a target created at the race seam");
    check(raced.nativeExportFailure.contains(QStringLiteral("without overwriting")),
        "partial_level_salvage reports the no-replace race as native export unavailability");

    const std::filesystem::path collisionDirectory = root / "collisions";
    QDir().mkpath(QString::fromStdWString(collisionDirectory.wstring()));
    const auto collision = salvageContext(
        collisionDirectory, QStringLiteral("same-id"));
    const auto first = ui::recovery::writePartialLevelSalvage(
        salvageFixture(ProjectType::MOTS), collision);
    const QByteArray firstManifest = readFile(first.manifestPath);
    const auto second = ui::recovery::writePartialLevelSalvage(
        salvageFixture(ProjectType::MOTS), collision);
    check(first.wroteManifest() && second.wroteManifest()
            && first.manifestPath != second.manifestPath
            && first.nativeTextPath != second.nativeTextPath,
        "partial_level_salvage resolves deterministic filename collisions");
    check(readFile(first.manifestPath) == firstManifest,
        "partial_level_salvage never overwrites the earlier collision target");

    const std::filesystem::path cleanDirectory = root / "clean";
    QDir().mkpath(QString::fromStdWString(cleanDirectory.wstring()));
    const auto cleanContext = salvageContext(
        cleanDirectory, QStringLiteral("clean"), {});
    const auto clean = ui::recovery::writePartialLevelSalvage(
        salvageFixture(ProjectType::IJIM), cleanContext);
    check(!clean.wroteManifest() && !clean.wroteNativeText()
            && QDir(QString::fromStdWString(cleanDirectory.wstring()))
                .entryList(QDir::Files | QDir::NoDotAndDotDot).isEmpty(),
        "partial_level_salvage creates no artifact for clean, repaired-complete, or optional-only recovery");

    const auto optionalOnly = ui::recovery::writePartialLevelSalvage(
        salvageFixture(ProjectType::JKDF2),
        salvageContext(cleanDirectory, QStringLiteral("optional-only"),
            { QStringLiteral("edit"), QStringLiteral("hist") }));
    check(!optionalOnly.wroteManifest() && !optionalOnly.wroteNativeText()
            && QDir(QString::fromStdWString(cleanDirectory.wstring()))
                .entryList(QDir::Files | QDir::NoDotAndDotDot).isEmpty(),
        "partial_level_salvage ignores loss confined to editor state or history");

    const std::filesystem::path boundedManifestDirectory =
        root / "bounded-manifest";
    QDir().mkpath(QString::fromStdWString(
        boundedManifestDirectory.wstring()));
    auto boundedManifestContext = salvageContext(
        boundedManifestDirectory, QStringLiteral("bounded-manifest"));
    boundedManifestContext.recoveryWarnings.clear();
    for (int index = 0; index < 300; ++index)
    {
        boundedManifestContext.recoveryWarnings.push_back(
            QString(8192, QLatin1Char('w')));
    }
    const auto boundedManifest = ui::recovery::writePartialLevelSalvage(
        salvageFixture(ProjectType::JKDF2), boundedManifestContext);
    const QByteArray boundedManifestBytes = readFile(
        boundedManifest.manifestPath);
    check(boundedManifest.wroteManifest()
            && boundedManifestBytes.size()
                <= ui::recovery::detail::maximumPartialLevelManifestCharactersForTesting()
            && boundedManifestBytes.contains(
                "additional diagnostic text truncated"),
        "partial_level_salvage caps adversarial warning amplification in its last-resort text manifest");

    const std::filesystem::path controlManifestDirectory =
        root / "control-manifest";
    QDir().mkpath(QString::fromStdWString(
        controlManifestDirectory.wstring()));
    auto controlManifestContext = salvageContext(
        controlManifestDirectory, QStringLiteral("control-manifest"));
    controlManifestContext.recoveryWarnings = { controlText };
    const auto controlManifest = ui::recovery::writePartialLevelSalvage(
        salvageFixture(ProjectType::JKDF2), controlManifestContext);
    const QByteArray controlManifestBytes = readFile(
        controlManifest.manifestPath);
    check(controlManifest.wroteManifest()
            && controlManifestBytes.contains("\\u0000")
            && controlManifestBytes.contains("\\u001B")
            && controlManifestBytes.contains("\\u009B")
            && controlManifestBytes.contains("\\u061C")
            && controlManifestBytes.contains("\\u2028")
            && controlManifestBytes.contains("\\u202E")
            && controlManifestBytes.contains("\\u2067")
            && controlManifestBytes.contains("\\uFEFF")
            && !controlManifestBytes.contains(QByteArray(1, '\0'))
            && !controlManifestBytes.contains(QByteArray(1, '\x1b'))
            && !controlManifestBytes.contains(QString(QChar(0x009b)).toUtf8())
            && !controlManifestBytes.contains(QString(QChar(0x061c)).toUtf8())
            && !controlManifestBytes.contains(QString(QChar(0x2028)).toUtf8())
            && !controlManifestBytes.contains(QString(QChar(0x202e)).toUtf8())
            && !controlManifestBytes.contains(QString(QChar(0x2067)).toUtf8())
            && !controlManifestBytes.contains(QString(QChar(0xfeff)).toUtf8()),
        "partial_level_salvage publishes forensic manifests without raw terminal, line, or bidi controls");
}

} // namespace sed::app_tests
