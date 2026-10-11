#include "test_common.hpp"
#include "resource_release_test_fixtures.hpp"
#include "main_window/main_window.hpp"
#include "gl_view.hpp"
#include "render/gizmos/light_category_gizmo_utils.hpp"
#include "thing/thing_light_authoring_utils.hpp"
#include "sed/level/thing.hpp"
#include "ui/resources/resource_picker.hpp"
#include "ui/components/preview_resource_disk_watcher.hpp"
#include "ui/components/template_model_preview_icon.hpp"
#include "sed/assets/resource_locator.hpp"
#include "sed/assets/3do.hpp"
#include "sed/assets/mat.hpp"
#include <QTemporaryDir>
#include <QApplication>
#include <QLabel>
#include <QPlainTextEdit>
#include <QTimer>
#include <fstream>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QLineEdit>
#include <QToolButton>
#include "ui/components/scrub_float_edit.hpp"
#include "sed/format_number.hpp"
#include <QPushButton>
#include <bit>
#include <cstdint>
#include <memory>
#include <algorithm>
#include <functional>
#include <utility>
#include <QImage>
#include <QDir>
#include <QFileInfo>
#include <chrono>
#include <cstdio>
#include <sstream>
#include <tuple>

namespace sed
{
bool templateEditorPaletteAvailableForTest(const assets::ResourceLocator&, const std::string&);
std::vector<ThingTemplate> templateEditorTemplatesForTest(QDialog* dialog);
}

namespace sed::ui
{
QString resourcePickerSourceDescriptionForTest(const assets::ResourceLocator&,
    const ResourcePickerOptions&, int, int, const std::string&);
std::pair<std::vector<QImage>, std::vector<QImage>> resourcePickerPaletteChangeForTest(
    std::shared_ptr<const assets::ResourceLocator>, const ResourcePickerOptions&,
    const QString&, const std::function<void()>&, bool&);
std::vector<std::string> resourcePickerSourceNamesForTest(
    const assets::ResourceLocator&, const ResourcePickerOptions&, int, int);
std::shared_ptr<const assets::ResourceLocator> resourcePickerPreviewResourcesForTest(
    std::shared_ptr<const assets::ResourceLocator>, const ResourcePickerOptions&, int, int);
}

namespace sed::app_tests
{
namespace
{

std::string expectedFilesystemStorageText(const std::filesystem::path& path)
{
    std::error_code error{};
    std::filesystem::path normalized = std::filesystem::absolute(path, error);
    if ( error )
    {
        normalized = path;
    }
    else
    {
        normalized = normalized.lexically_normal();
    }
    normalized.make_preferred();
    return QStringLiteral("Storage: %1")
        .arg(QString::fromStdWString(normalized.wstring())).toStdString();
}

void checkResourceRows(
    const std::vector<std::string>& actual,
    const std::vector<std::string>& expected,
    const char* message)
{
    if ( actual == expected )
    {
        return;
    }

    ++failures;
    std::cerr << "FAIL: " << message << " actual=[";
    for ( std::size_t index = 0; index < actual.size(); ++index )
    {
        if ( index != 0u )
        {
            std::cerr << ", ";
        }
        std::cerr << '"' << actual[index] << '"';
    }
    std::cerr << "] expected=[";
    for ( std::size_t index = 0; index < expected.size(); ++index )
    {
        if ( index != 0u )
        {
            std::cerr << ", ";
        }
        std::cerr << '"' << expected[index] << '"';
    }
    std::cerr << "]\n";
}

void savePreviewArtifactsOnFailure(const QImage& before, const QImage& after)
{
    const QByteArray artifactDirectory = qgetenv("SED_TEST_PREVIEW_ARTIFACT_DIR");
    if ( artifactDirectory.isEmpty() )
    {
        return;
    }

    QDir directory(QString::fromLocal8Bit(artifactDirectory));
    if ( !directory.mkpath(QStringLiteral(".")) )
    {
        std::cerr << "nested Resource Picker could not create preview artifact directory: "
                  << artifactDirectory.constData() << '\n';
        return;
    }

    const QString beforePath = directory.filePath(
        QStringLiteral("resource-picker-preview-before.png"));
    const QString afterPath = directory.filePath(
        QStringLiteral("resource-picker-preview-after.png"));
    const bool beforeSaved = before.save(beforePath, "PNG");
    const bool afterSaved = after.save(afterPath, "PNG");
    std::cerr << "nested Resource Picker preview artifacts: before="
              << beforePath.toStdString()
              << " saved=" << beforeSaved
              << " after=" << afterPath.toStdString()
              << " saved=" << afterSaved << '\n';
}

QByteArray makeArchiveRetryGob(
    const std::string& entry,
    const QByteArray& payload)
{
    QByteArray archive("GOB ", 4);
    const auto appendU32 = [&archive](const std::uint32_t value)
    {
        for ( int shift = 0; shift < 32; shift += 8 )
        {
            archive.append(static_cast<char>((value >> shift) & 0xffu));
        }
    };
    appendU32(0x14);
    appendU32(0x0c);
    appendU32(1);
    appendU32(152);
    appendU32(static_cast<std::uint32_t>(payload.size()));
    QByteArray archiveName = QByteArray::fromStdString(entry);
    archiveName.resize(128, '\0');
    archive += archiveName;
    archive += payload;
    return archive;
}

QByteArray makeMultiEntryArchiveGob(
    const std::vector<std::pair<std::string, QByteArray>>& entries)
{
    QByteArray archive("GOB ", 4);
    const auto appendU32 = [&archive](const std::uint32_t value)
    {
        for ( int shift = 0; shift < 32; shift += 8 )
        {
            archive.append(static_cast<char>((value >> shift) & 0xffu));
        }
    };
    appendU32(0x14);
    appendU32(0x0c);
    appendU32(static_cast<std::uint32_t>(entries.size()));

    std::uint32_t dataOffset = static_cast<std::uint32_t>(
        16u + 136u * entries.size());
    for ( const auto& [name, payload] : entries )
    {
        appendU32(dataOffset);
        appendU32(static_cast<std::uint32_t>(payload.size()));
        QByteArray archiveName = QByteArray::fromStdString(name);
        archiveName.resize(128, '\0');
        archive += archiveName;
        dataOffset += static_cast<std::uint32_t>(payload.size());
    }
    for ( const auto& [name, payload] : entries )
    {
        (void)name;
        archive += payload;
    }
    return archive;
}

bool writeArchiveRetryFixture(
    const std::filesystem::path& path,
    const QByteArray& bytes,
    const std::filesystem::file_time_type stamp)
{
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if ( !output )
    {
        return false;
    }
    output.write(bytes.constData(), bytes.size());
    output.close();
    if ( !output )
    {
        return false;
    }
    std::error_code ec{};
    std::filesystem::last_write_time(path, stamp, ec);
    return !ec;
}

void runArchiveReloadRetryTests()
{
    QTemporaryDir directory;
    check(directory.isValid(), "archive retry fixture creates a temporary resource root");
    if ( !directory.isValid() )
    {
        return;
    }

    const auto archivePath = std::filesystem::path(
        directory.path().toStdWString()) / "mounted.gob";
    const std::string oldEntry = "old.3do";
    const std::string newEntry = "new.3do";
    const QByteArray oldArchive = makeArchiveRetryGob(oldEntry, QByteArray("old-payload"));
    const QByteArray newArchive = makeArchiveRetryGob(newEntry, QByteArray("new-payload"));
    QByteArray brokenArchive = oldArchive;
    brokenArchive[0] = 'X';
    check(oldArchive.size() == newArchive.size()
            && oldArchive.size() == brokenArchive.size(),
        "archive retry fixture keeps broken and replacement archives at one byte length");
    const auto stamp = std::filesystem::file_time_type::clock::now();
    bool initialWrite = writeArchiveRetryFixture(archivePath, oldArchive, stamp);
    check(initialWrite, "archive retry fixture writes its initial valid archive");
    if ( !initialWrite )
    {
        return;
    }

    auto resources = std::make_shared<assets::ResourceLocator>();
    bool mounted = false;
    try
    {
        resources->mountGob(archivePath);
        mounted = true;
    }
    catch ( const std::exception& )
    {
    }
    check(mounted, "archive retry fixture mounts its initial archive");
    if ( !mounted )
    {
        return;
    }

    ui::ResourcePickerOptions pickerOptions;
    pickerOptions.kind = ui::ResourcePickerKind::model3do;
    pickerOptions.previewProjectType = ProjectType::IJIM;
    bool pickerCorruptWrite = false;
    bool pickerRepairWrite = false;
    int pickerAttempts = 0;
    bool pickerOldAfterFailure = false;
    bool pickerOldAfterRetry = true;
    bool pickerNewAfterRetry = false;
    std::string pickerOldPayloadAfterFailure{};
    std::string pickerOldPayloadAfterRetry{};
    std::string pickerNewPayloadAfterRetry{};
    bool pickerReloadFailureObserved = false;
    const bool pickerHelperReady = ui::resourcePickerArchiveReloadRetryForTest(
        std::make_shared<const assets::ResourceLocator>(*resources),
        pickerOptions,
        [&]()
        {
            pickerCorruptWrite = writeArchiveRetryFixture(
                archivePath, brokenArchive, stamp);
        },
        [&]()
        {
            pickerRepairWrite = writeArchiveRetryFixture(
                archivePath, newArchive, stamp);
        },
        oldEntry,
        newEntry,
        pickerAttempts,
        pickerOldAfterFailure,
        pickerOldAfterRetry,
        pickerNewAfterRetry,
        pickerOldPayloadAfterFailure,
        pickerOldPayloadAfterRetry,
        pickerNewPayloadAfterRetry,
        pickerReloadFailureObserved);
    check(pickerHelperReady && pickerCorruptWrite && pickerRepairWrite,
        "Resource Picker archive retry fixture wires a real dialog watcher and both archive writes");
    check(pickerHelperReady && pickerAttempts == 2,
        "Resource Picker retries one failed archive reload without a second filesystem event");
    check(pickerHelperReady && pickerOldAfterFailure && !pickerOldAfterRetry
            && pickerNewAfterRetry,
        "Resource Picker retains the old archive index after failure and publishes the repaired index on retry");
    check(pickerOldPayloadAfterFailure == "old-payload"
            && pickerOldPayloadAfterRetry.empty()
            && pickerNewPayloadAfterRetry == "new-payload",
        "Resource Picker archive retry returns exact fixed payloads and removes the old entry");
    check(pickerReloadFailureObserved,
        "Resource Picker archive retry records an actual reload failure before repair");

    check(writeArchiveRetryFixture(archivePath, oldArchive, stamp),
        "Template Editor archive retry fixture restores its initial archive");
    int templateAttempts = 0;
    bool templateOldAfterFailure = false;
    bool templateOldAfterRetry = true;
    bool templateNewAfterRetry = false;
    std::string templateOldPayloadAfterFailure{};
    std::string templateOldPayloadAfterRetry{};
    std::string templateNewPayloadAfterRetry{};
    bool templateReloadFailureObserved = false;
    bool templateCorruptWrite = false;
    bool templateRepairWrite = false;
    const bool templateHelperReady = templateEditorArchiveReloadRetryForTest(
        *resources,
        [&]()
        {
            templateCorruptWrite = writeArchiveRetryFixture(
                archivePath, brokenArchive, stamp);
        },
        [&]()
        {
            templateRepairWrite = writeArchiveRetryFixture(
                archivePath, newArchive, stamp);
        },
        oldEntry,
        newEntry,
        templateAttempts,
        templateOldAfterFailure,
        templateOldAfterRetry,
        templateNewAfterRetry,
        templateOldPayloadAfterFailure,
        templateOldPayloadAfterRetry,
        templateNewPayloadAfterRetry,
        templateReloadFailureObserved);
    check(templateHelperReady && templateCorruptWrite && templateRepairWrite,
        "Template Editor archive retry fixture wires a real dialog watcher and both archive writes");
    check(templateHelperReady && templateAttempts == 2,
        "Template Editor retries one failed archive reload without a second filesystem event");
    check(templateHelperReady && templateOldAfterFailure && !templateOldAfterRetry
            && templateNewAfterRetry,
        "Template Editor retains the old archive index after failure and publishes the repaired index on retry");
    check(templateOldPayloadAfterFailure == "old-payload"
            && templateOldPayloadAfterRetry.empty()
            && templateNewPayloadAfterRetry == "new-payload",
        "Template Editor archive retry returns exact fixed payloads and removes the old entry");
    check(templateReloadFailureObserved,
        "Template Editor archive retry records an actual reload failure before repair");

    check(writeArchiveRetryFixture(archivePath, brokenArchive, stamp),
        "unowned GlView archive fixture writes a corrupt mounted archive");
    GlView unownedView;
    auto* unownedWatcher = unownedView.resourceDiskWatcherForTest();
    int unownedArchiveSignals = 0;
    QObject::connect(
        &unownedView,
        &GlView::resourceArchivesChanged,
        [&unownedArchiveSignals]()
        {
            ++unownedArchiveSignals;
        });
    unownedView.setResourceLocator(resources.get());
    check(unownedWatcher != nullptr,
        "unowned GlView archive fixture exposes its production watcher");
    if ( unownedWatcher )
    {
        unownedWatcher->syncNow();
        unownedWatcher->notifyArchivePathChangedForTest();
        unownedWatcher->flushPendingChangeForTest();
        check(unownedArchiveSignals == 0
                && unownedWatcher->archiveRetryPendingForTest(),
            "GlView without a reload owner retains a mounted archive event and emits no success signal");
        unownedView.setResourceLocator(nullptr);
    }
    check(writeArchiveRetryFixture(archivePath, oldArchive, stamp),
        "unowned GlView archive fixture restores its initial archive");

    check(writeArchiveRetryFixture(archivePath, oldArchive, stamp),
        "GlView archive retry fixture restores its initial archive");
    MainWindow glWindow;
    glWindow.setAttribute(Qt::WA_DontShowOnScreen);
    glWindow.setResourcePathsForTest({}, {archivePath});
    GlView* view = glWindow.glViewForTest();
    auto* watcher = view ? view->resourceDiskWatcherForTest() : nullptr;
    int glViewArchiveSignals = 0;
    if ( view )
    {
        QObject::connect(
            view,
            &GlView::resourceArchivesChanged,
            [&glViewArchiveSignals]()
            {
                ++glViewArchiveSignals;
            });
    }
    check(watcher != nullptr, "GlView archive retry fixture exposes its production watcher");
    if ( watcher )
    {
        watcher->syncNow();
        const bool glCorruptWrite = writeArchiveRetryFixture(
            archivePath, brokenArchive, stamp);
        watcher->notifyArchivePathChangedForTest();
        watcher->flushPendingChangeForTest();
        const int glViewSignalsAfterFailure = glViewArchiveSignals;
        const bool glReloadWarningAfterFailure =
            glWindow.resourceArchiveReloadWarningShownForTest();
        const bool glOldAfterFailure =
            glWindow.resourceLocatorForTest().tryReadText(oldEntry).has_value();
        const auto glOldPayloadAfterFailure =
            glWindow.resourceLocatorForTest().tryReadText(oldEntry);
        const bool glRepairWrite = writeArchiveRetryFixture(
            archivePath, newArchive, stamp);
        watcher->flushPendingChangeForTest();
        const bool glOldAfterRetry =
            glWindow.resourceLocatorForTest().tryReadText(oldEntry).has_value();
        const bool glNewAfterRetry =
            glWindow.resourceLocatorForTest().tryReadText(newEntry).has_value();
        const auto glOldPayloadAfterRetry =
            glWindow.resourceLocatorForTest().tryReadText(oldEntry);
        const auto glNewPayloadAfterRetry =
            glWindow.resourceLocatorForTest().tryReadText(newEntry);
        const bool glReloadWarningAfterRetry =
            glWindow.resourceArchiveReloadWarningShownForTest();
        check(glCorruptWrite && glRepairWrite,
            "GlView archive retry fixture writes broken and repaired archives");
        check(glViewSignalsAfterFailure == 0 && glViewArchiveSignals == 1,
            "GlView's production archive signal fires only after a successful archive reload");
        check(glReloadWarningAfterFailure && !glReloadWarningAfterRetry,
            "MainWindow reports one archive reload warning for a failed episode and clears it after repair");
        check(glOldAfterFailure && !glOldAfterRetry && glNewAfterRetry,
            "MainWindow's connected GlView archive path retains the old index then publishes the repaired index without a second event");
        check(glOldPayloadAfterFailure == std::optional<std::string>("old-payload")
                && !glOldPayloadAfterRetry
                && glNewPayloadAfterRetry == std::optional<std::string>("new-payload"),
            "MainWindow's connected GlView archive path returns exact fixed payloads and removes the old entry");
    }
}

void runNestedResourcePickerArchiveRefreshTests()
{
    QTemporaryDir directory;
    check(directory.isValid(),
        "nested Resource Picker archive fixture creates a temporary resource root");
    if ( !directory.isValid() )
    {
        return;
    }

    const auto archivePath = std::filesystem::path(
        directory.path().toStdWString()) / "nested-preview.gob";
    const std::string oldEntry = "old.3do";
    const std::string newEntry = "new.3do";
    const std::string selectedEntry = "preview.3do";
    const QByteArray material = fixtures::makeIndexedMatFixture(6u);
    const QByteArray oldPalette =
        fixtures::makeClassicCmpFixture(6u, QColor(255, 0, 0));
    const QByteArray newPalette =
        fixtures::makeClassicCmpFixture(6u, QColor(0, 255, 0));
    // Keep the model geometry byte-for-byte equivalent across the archive
    // replacement.  The fixed MAT/CMP palette change is the only visual
    // input, so a changed framebuffer proves cache refresh rather than a
    // camera-fit or model-size difference.
    const QByteArray oldModel = fixtures::makeDiskChangeModelFixture(
        "unused.mat", "nested old model payload", 2.0, sed::FF_DoubleSided);
    const QByteArray newModel = fixtures::makeDiskChangeModelFixture(
        "unused.mat", "nested new model payload", 2.0, sed::FF_DoubleSided);
    const auto hasDoubleSidedFace = [](const QByteArray& bytes)
    {
        try
        {
            std::istringstream stream(
                bytes.toStdString(), std::ios::in | std::ios::binary);
            const assets::Model3do model = assets::loadFrom3DO(stream);
            return model.meshes.size() == 1u
                && model.meshes.front().faces.size() == 1u
                && model.meshes.front().faces.front().faceFlags == sed::FF_DoubleSided;
        }
        catch ( const std::exception& )
        {
            return false;
        }
    };
    check(hasDoubleSidedFace(oldModel) && hasDoubleSidedFace(newModel),
        "nested Resource Picker preview fixtures preserve the exact double-sided face flag");
    const QByteArray oldArchive = makeMultiEntryArchiveGob({
        { "3do/" + oldEntry, oldModel },
        { "3do/" + selectedEntry, oldModel },
        { "mat/unused.mat", material },
        { "misc/cmp/preview.cmp", oldPalette } });
    const QByteArray newArchive = makeMultiEntryArchiveGob({
        { "3do/" + newEntry, newModel },
        { "3do/" + selectedEntry, newModel },
        { "mat/unused.mat", material },
        { "misc/cmp/preview.cmp", newPalette } });
    const auto stamp = std::filesystem::file_time_type::clock::now();
    check(writeArchiveRetryFixture(archivePath, oldArchive, stamp),
        "nested Resource Picker archive fixture writes its initial model archive");
    if ( !std::filesystem::exists(archivePath) )
    {
        return;
    }

    auto resources = std::make_shared<assets::ResourceLocator>();
    bool mounted = false;
    try
    {
        resources->mountGob(archivePath);
        mounted = true;
    }
    catch ( const std::exception& )
    {
    }
    check(mounted,
        "nested Resource Picker archive fixture mounts its initial model archive");
    if ( !mounted )
    {
        return;
    }

    ui::ResourcePickerOptions options;
    options.kind = ui::ResourcePickerKind::model3do;
    options.initialName = selectedEntry;
    // The fixture uses an indexed MAT plus an explicit classic CMP.  IJIM
    // intentionally has no master CMP, so use the classic preview pipeline
    // here to make the palette replacement a rendered input.
    options.previewProjectType = ProjectType::JKDF2;
    options.previewCmp = "preview.cmp";
    const auto result = ui::resourcePickerNestedArchiveReloadForTest(
        std::make_shared<const assets::ResourceLocator>(*resources),
        options,
        [&]()
        {
            check(writeArchiveRetryFixture(archivePath, newArchive, stamp),
                "nested Resource Picker archive fixture writes its replacement model archive");
        },
        oldEntry,
        newEntry);

    if ( result.archiveSignalCount != 1
        || result.archiveRetryPending
        || !result.previewSettledBefore
        || !result.previewSettledAfter
        || result.templatePreviewActiveJobs != 0 )
    {
        std::cerr << "nested Resource Picker archive diagnostics: attempts="
                  << result.archiveReloadAttemptCount
                  << " signals=" << result.archiveSignalCount
                  << " activeJobs=" << result.templatePreviewActiveJobs
                  << " workerThreads=" << result.previewWorkerActiveThreads
                  << " retryPending=" << result.archiveRetryPending
                  << " settledBefore=" << result.previewSettledBefore
                  << " settledAfter=" << result.previewSettledAfter << '\n';
    }

    check(result.ready,
        "nested Resource Picker archive fixture wires the real nested GlView watcher");
    check(result.previewSettledBefore,
        "nested Resource Picker settles its first visible preview before capturing the old frame");
    check(result.previewSettledAfter,
        "nested Resource Picker settles its replacement preview before capturing the new frame");
    checkResourceRows(
        result.visibleRowsBefore,
        std::vector<std::string>{ oldEntry, selectedEntry },
        "nested Resource Picker initially exposes the exact mounted model rows");
    check(result.oldPayloadBefore == std::optional<std::string>(oldModel.toStdString()),
        "nested Resource Picker initially reads the exact old model payload");
    checkResourceRows(
        result.visibleRowsAfter,
        std::vector<std::string>{ newEntry, selectedEntry },
        "nested GlView archive success refreshes the Resource Picker's visible rows");
    check(!result.oldPayloadAfter
            && result.newPayloadAfter == std::optional<std::string>(newModel.toStdString()),
        "nested GlView archive success publishes the exact replacement payload and removes the old entry");

    const auto payloadMatches = [](const std::optional<std::string>& actual,
                                   const QByteArray& expected)
    {
        return actual == std::optional<std::string>(expected.toStdString());
    };
    const bool oldCmpResolved =
        payloadMatches(result.previewCmpPayloadBefore, oldPalette);
    const bool newCmpResolved =
        payloadMatches(result.previewCmpPayloadAfter, newPalette);
    const bool oldMaterialResolved =
        payloadMatches(result.previewMaterialPayloadBefore, material);
    const bool newMaterialResolved =
        payloadMatches(result.previewMaterialPayloadAfter, material);
    check(oldCmpResolved && newCmpResolved
            && oldMaterialResolved && newMaterialResolved,
        "nested Resource Picker resolves the fixed old/new CMP payloads and unchanged MAT payload");

    const auto countDominantPixels = [](
        const QImage& image,
        const bool red,
        const bool central)
    {
        if ( image.isNull() )
        {
            return 0;
        }
        const int left = central ? image.width() / 4 : 0;
        const int right = central ? image.width() - left : image.width();
        const int top = central ? image.height() / 4 : 0;
        const int bottom = central ? image.height() - top : image.height();
        int count = 0;
        for ( int y = top; y < bottom; ++y )
        {
            for ( int x = left; x < right; ++x )
            {
                const QColor pixel = image.pixelColor(x, y);
                const bool dominant = red
                    ? pixel.red() >= 96
                        && pixel.red() >= pixel.green() + 24
                        && pixel.red() >= pixel.blue() + 24
                    : pixel.green() >= 96
                        && pixel.green() >= pixel.red() + 24
                        && pixel.green() >= pixel.blue() + 24;
                if ( dominant )
                {
                    ++count;
                }
            }
        }
        return count;
    };
    check(!result.previewBefore.isNull() && !result.previewAfter.isNull(),
        "nested Resource Picker renders a real model preview before and after archive replacement");
    const int redBefore = countDominantPixels(result.previewBefore, true, true);
    const int greenBefore = countDominantPixels(result.previewBefore, false, true);
    const int redAfter = countDominantPixels(result.previewAfter, true, true);
    const int greenAfter = countDominantPixels(result.previewAfter, false, true);
    const int redBeforeWhole = countDominantPixels(result.previewBefore, true, false);
    const int greenBeforeWhole = countDominantPixels(result.previewBefore, false, false);
    const int redAfterWhole = countDominantPixels(result.previewAfter, true, false);
    const int greenAfterWhole = countDominantPixels(result.previewAfter, false, false);
    if ( result.previewBefore.size() != result.previewAfter.size() )
    {
        ++failures;
        std::cerr << "FAIL: nested Resource Picker preview size changed from "
                  << result.previewBefore.width() << 'x' << result.previewBefore.height()
                  << " to " << result.previewAfter.width() << 'x'
                  << result.previewAfter.height() << '\n';
    }
    check(redBefore >= 8 && greenBefore == 0
            && redAfter == 0 && greenAfter >= 8,
        "nested GlView archive success replaces a fixed red MAT/CMP foreground with green at unchanged geometry");
    if ( redBefore < 8 || greenBefore != 0 || redAfter != 0 || greenAfter < 8
        || !oldCmpResolved || !newCmpResolved
        || !oldMaterialResolved || !newMaterialResolved )
    {
        const auto printImage = [](const char* label, const QImage& image)
        {
            std::cerr << label
                      << " null=" << image.isNull()
                      << " size=" << image.width() << 'x' << image.height()
                      << " format=" << static_cast<int>(image.format())
                      << " dpr=" << image.devicePixelRatio()
                      << '\n';
        };
        std::cerr << "nested Resource Picker foreground diagnostics: redBefore="
                  << redBefore
                  << " greenBefore=" << greenBefore
                  << " redAfter=" << redAfter
                  << " greenAfter=" << greenAfter
                  << " redBeforeWhole=" << redBeforeWhole
                  << " greenBeforeWhole=" << greenBeforeWhole
                  << " redAfterWhole=" << redAfterWhole
                  << " greenAfterWhole=" << greenAfterWhole
                  << " cmpBefore=" << oldCmpResolved
                  << " cmpAfter=" << newCmpResolved
                  << " matBefore=" << oldMaterialResolved
                  << " matAfter=" << newMaterialResolved
                  << " cmpBeforeBytes="
                  << (result.previewCmpPayloadBefore
                          ? result.previewCmpPayloadBefore->size() : 0u)
                  << " cmpAfterBytes="
                  << (result.previewCmpPayloadAfter
                          ? result.previewCmpPayloadAfter->size() : 0u)
                  << " matBeforeBytes="
                  << (result.previewMaterialPayloadBefore
                          ? result.previewMaterialPayloadBefore->size() : 0u)
                  << " matAfterBytes="
                  << (result.previewMaterialPayloadAfter
                          ? result.previewMaterialPayloadAfter->size() : 0u)
                  << " interactiveBefore=" << result.previewInteractiveBefore
                  << " interactiveAfter=" << result.previewInteractiveAfter
                  << " texturesBefore=" << result.previewTexturesBefore
                  << " texturesAfter=" << result.previewTexturesAfter
                  << " solidBefore=" << result.previewSolidBefore
                  << " solidAfter=" << result.previewSolidAfter
                  << " renderModeBefore=" << result.previewRenderModeBefore
                  << " renderModeAfter=" << result.previewRenderModeAfter
                  << " selectedBefore=\"" << result.previewSelectedNameBefore
                  << "\" selectedAfter=\"" << result.previewSelectedNameAfter
                  << "\"\n";
        printImage("nested Resource Picker preview before:", result.previewBefore);
        printImage("nested Resource Picker preview after:", result.previewAfter);
        savePreviewArtifactsOnFailure(result.previewBefore, result.previewAfter);
    }
    check(result.archiveSignalCount == 1,
        "nested GlView emits exactly one archive-success signal after owner reload");
    check(result.previewWorkerActiveThreads == 0
            && result.templatePreviewActiveJobs == 0,
        "nested Resource Picker captures its preview only after all worker jobs retire");
}

void runDelegatedThumbnailArchiveWatchTests()
{
    QTemporaryDir directory;
    check(directory.isValid(),
        "delegated thumbnail archive fixture creates a temporary resource root");
    if ( !directory.isValid() )
    {
        return;
    }

    const auto archivePath = std::filesystem::path(
        directory.path().toStdWString()) / "hidden-thumbnail.gob";
    const auto looseRoot = std::filesystem::path(
        directory.path().toStdWString()) / "loose-preview-root";
    std::error_code looseRootError{};
    std::filesystem::create_directories(looseRoot / "3do", looseRootError);
    check(!looseRootError,
        "delegated thumbnail fixture creates a loose override directory");
    if ( looseRootError )
    {
        return;
    }
    const QByteArray model = fixtures::makeDiskChangeModelFixture(
        "unused.mat", "hidden thumbnail model payload", 2.0);
    const QByteArray archive = makeMultiEntryArchiveGob({
        { "3do/hidden.3do", model } });
    const auto stamp = std::filesystem::file_time_type::clock::now();
    check(writeArchiveRetryFixture(archivePath, archive, stamp),
        "delegated thumbnail archive fixture writes its model archive");
    if ( !std::filesystem::exists(archivePath) )
    {
        return;
    }

    auto resources = std::make_shared<assets::ResourceLocator>();
    bool mounted = false;
    try
    {
        resources->addRoot(looseRoot);
        resources->mountGob(archivePath);
        mounted = true;
    }
    catch ( const std::exception& )
    {
    }
    check(mounted,
        "delegated thumbnail archive fixture mounts its model archive");
    if ( !mounted )
    {
        return;
    }

    const auto renderer = ui::detail::acquireSharedGlViewModelThumbnailRenderer(
        std::make_shared<const assets::ResourceLocator>(*resources),
        ProjectType::IJIM,
        {});
    const QImage image = renderer
        ? renderer->renderModel("hidden.3do", QSize(96, 96), false)
        : QImage{};
    auto* watcher = renderer ? renderer->resourceDiskWatcherForTest() : nullptr;
    check(!image.isNull(),
        "delegated thumbnail archive fixture renders through the hidden GlView");
    check(watcher != nullptr,
        "delegated thumbnail renderer exposes its real GlView watcher to the test");
    if ( !watcher )
    {
        return;
    }
    watcher->syncNow();
    watcher->watchResource("hidden.3do", ".3do");
    watcher->syncNow();
#ifdef _WIN32
    const QString archiveQString = QString::fromStdWString(archivePath.wstring());
#else
    const QString archiveQString = QString::fromStdString(archivePath.string());
#endif
    const auto normalizeWatchedPath = [](const QString& path)
    {
        return QDir::cleanPath(
            QDir::fromNativeSeparators(QFileInfo(path).absoluteFilePath()))
            .toCaseFolded();
    };
    const QString normalizedArchivePath = normalizeWatchedPath(archiveQString);
    const QString normalizedLooseDirectory = normalizeWatchedPath(
        QString::fromStdWString((looseRoot / "3do").wstring()));
    const QStringList watchedFiles = watcher->watchedFiles();
    const QStringList watchedDirectories = watcher->watchedDirectories();
    const auto archiveIsWatched = std::any_of(
        watchedFiles.cbegin(),
        watchedFiles.cend(),
        [&](const QString& path)
        {
            return normalizeWatchedPath(path) == normalizedArchivePath;
        });
    if ( archiveIsWatched )
    {
        ++failures;
        std::cerr << "FAIL: delegated hidden thumbnail GlView does not independently watch mounted archives"
                  << " normalizedArchive=" << normalizedArchivePath.toStdString()
                  << " watchedFiles=[";
        for ( int index = 0; index < watchedFiles.size(); ++index )
        {
            if ( index != 0 )
            {
                std::cerr << ", ";
            }
            std::cerr << '"' << normalizeWatchedPath(watchedFiles.at(index)).toStdString() << '"';
        }
        std::cerr << "]\n";
    }
    const bool looseDirectoryIsWatched = std::any_of(
        watchedDirectories.cbegin(),
        watchedDirectories.cend(),
        [&](const QString& path)
        {
            return normalizeWatchedPath(path) == normalizedLooseDirectory;
        });
    check(looseDirectoryIsWatched,
        "delegated hidden thumbnail retains the loose override directory watch");
}

void runDelegatedPlacementArchiveWatchTests()
{
    QTemporaryDir directory;
    check(directory.isValid(),
        "delegated placement watcher fixture creates a temporary resource root");
    if ( !directory.isValid() )
    {
        return;
    }

    const auto looseRoot = std::filesystem::path(
        directory.path().toStdWString()) / "loose-placement-root";
    const auto archiveRoot = std::filesystem::path(
        directory.path().toStdWString()) / "mounted-placement-root";
    std::error_code directoryError{};
    std::filesystem::create_directories(looseRoot / "3do", directoryError);
    const bool looseDirectoriesCreated = !directoryError;
    directoryError.clear();
    std::filesystem::create_directories(archiveRoot, directoryError);
    const bool archiveDirectoryCreated = !directoryError;
    check(looseDirectoriesCreated && archiveDirectoryCreated,
        "delegated placement watcher fixture creates separate loose and archive roots");
    if ( !looseDirectoriesCreated || !archiveDirectoryCreated )
    {
        return;
    }

    const auto looseModelPath = looseRoot / "3do" / "placement.3do";
    const auto archivePath = archiveRoot / "mounted.gob";
    const QByteArray looseModel = fixtures::makeDiskChangeModelFixture(
        "unused.mat", "placement loose dependency", 2.0);
    const QByteArray archive = makeMultiEntryArchiveGob({
        { "3do/archive-only.3do", QByteArray("archive-only") } });
    check(writeArchiveRetryFixture(
            looseModelPath,
            looseModel,
            std::filesystem::file_time_type::clock::now())
            && writeArchiveRetryFixture(
                archivePath,
                archive,
                std::filesystem::file_time_type::clock::now()),
        "delegated placement watcher fixture writes loose and mounted resources");

    MainWindow window;
    window.setAttribute(Qt::WA_DontShowOnScreen);
    window.setResourcePathsForTest({ looseRoot }, { archivePath });
    auto* watcher = window.thingPlacementResourceDiskWatcherForTest();
    check(watcher != nullptr,
        "placement dependency watcher is available for archive ownership testing");
    if ( !watcher )
    {
        return;
    }

    watcher->clearDependencies();
    watcher->watchResource("placement.3do", ".3do");
    watcher->syncNow();

    const auto normalizeWatchedPath = [](const QString& path)
    {
        return QDir::cleanPath(
            QDir::fromNativeSeparators(QFileInfo(path).absoluteFilePath()))
            .toCaseFolded();
    };
#ifdef _WIN32
    const QString archiveQString = QString::fromStdWString(archivePath.wstring());
    const QString archiveDirectoryQString =
        QString::fromStdWString(archiveRoot.wstring());
    const QString looseModelQString =
        QString::fromStdWString(looseModelPath.wstring());
    const QString looseDirectoryQString =
        QString::fromStdWString((looseRoot / "3do").wstring());
#else
    const QString archiveQString = QString::fromStdString(archivePath.string());
    const QString archiveDirectoryQString =
        QString::fromStdString(archiveRoot.string());
    const QString looseModelQString =
        QString::fromStdString(looseModelPath.string());
    const QString looseDirectoryQString =
        QString::fromStdString((looseRoot / "3do").string());
#endif
    const QString normalizedArchivePath = normalizeWatchedPath(archiveQString);
    const QString normalizedArchiveDirectory =
        normalizeWatchedPath(archiveDirectoryQString);
    const QString normalizedLooseModel = normalizeWatchedPath(looseModelQString);
    const QString normalizedLooseDirectory =
        normalizeWatchedPath(looseDirectoryQString);
    const QStringList watchedFiles = watcher->watchedFiles();
    const QStringList watchedDirectories = watcher->watchedDirectories();
    const bool archiveIsWatched = std::any_of(
        watchedFiles.cbegin(),
        watchedFiles.cend(),
        [&](const QString& path)
        {
            return normalizeWatchedPath(path) == normalizedArchivePath;
        });
    const bool looseDirectoryIsWatched = std::any_of(
        watchedDirectories.cbegin(),
        watchedDirectories.cend(),
        [&](const QString& path)
        {
            return normalizeWatchedPath(path) == normalizedLooseDirectory;
        });
    const bool looseModelIsWatched = std::any_of(
        watchedFiles.cbegin(),
        watchedFiles.cend(),
        [&](const QString& path)
        {
            return normalizeWatchedPath(path) == normalizedLooseModel;
        });
    const bool archiveDirectoryIsWatched = std::any_of(
        watchedDirectories.cbegin(),
        watchedDirectories.cend(),
        [&](const QString& path)
        {
            return normalizeWatchedPath(path) == normalizedArchiveDirectory;
        });
    check(!watcher->mountedArchiveWatchingEnabledForTest(),
        "placement dependency watcher delegates mounted archive ownership to MainWindow");
    check(!archiveIsWatched,
        "placement dependency watcher does not duplicate MainWindow mounted archive watching");
    check(looseDirectoryIsWatched,
        "placement dependency watcher retains the loose override directory watch");
    check(looseModelIsWatched,
        "placement dependency watcher retains the selected loose model file watch");
    check(!archiveDirectoryIsWatched,
        "placement dependency watcher does not retain the mounted archive parent directory");
}

void runThingPlacementTemplateIconCacheTests()
{
    QTemporaryDir directory;
    check(directory.isValid(),
        "Thing placement icon fixture creates a temporary resource root");
    if ( !directory.isValid() )
    {
        return;
    }

    const auto root = std::filesystem::path(directory.path().toStdWString());
    const auto writeBytes = [](const std::filesystem::path& path,
                               const QByteArray& bytes) -> bool
    {
        std::ofstream output(path, std::ios::binary | std::ios::trunc);
        if ( !output )
        {
            return false;
        }
        output.write(bytes.constData(), bytes.size());
        output.close();
        return static_cast<bool>(output);
    };
    const auto setStamp = [](const std::filesystem::path& path,
                             const std::filesystem::file_time_type stamp) -> bool
    {
        std::error_code ec{};
        std::filesystem::last_write_time(path, stamp, ec);
        return !ec;
    };
    const auto imagesDiffer = [](const QImage& left, const QImage& right)
    {
        if ( left.size() != right.size() || left.format() != right.format() )
        {
            return true;
        }
        for ( int y = 0; y < left.height(); ++y )
        {
            for ( int x = 0; x < left.width(); ++x )
            {
                if ( left.pixel(x, y) != right.pixel(x, y) )
                {
                    return true;
                }
            }
        }
        return false;
    };
    const auto containsExactColor = [](const QImage& image, const QColor& color)
    {
        for ( int y = 0; y < image.height(); ++y )
        {
            for ( int x = 0; x < image.width(); ++x )
            {
                if ( image.pixelColor(x, y) == color )
                {
                    return true;
                }
            }
        }
        return false;
    };
    const auto containsRedTint = [](const QImage& image)
    {
        for ( int y = 0; y < image.height(); ++y )
        {
            for ( int x = 0; x < image.width(); ++x )
            {
                const QColor pixel = image.pixelColor(x, y);
                if ( pixel.red() >= 128 && pixel.red() > pixel.green() * 2
                    && pixel.red() > pixel.blue() * 2 )
                {
                    return true;
                }
            }
        }
        return false;
    };

    const QByteArray material = fixtures::makeIndexedMatFixture(6u);
    const QByteArray redPalette =
        fixtures::makeClassicCmpFixture(6u, QColor(255, 0, 0));
    const QByteArray greenPalette =
        fixtures::makeClassicCmpFixture(6u, QColor(0, 255, 0));
    const QByteArray spriteBytes(
        "placement.mat 0 2 1 4 3 3 0 0 0 0\n");
    const QByteArray modelWide = fixtures::makeDiskChangeModelFixture(
        "placement.mat", "same-revision", 2.0);
    const QByteArray modelNarrow = fixtures::makeDiskChangeModelFixture(
        "placement.mat", "same-revision", 3.0);
    const auto cmpPath = root / "placement.cmp";
    const auto matPath = root / "placement.mat";
    const auto spritePath = root / "placement.spr";
    const auto modelPath = root / "placement.3do";
    const bool initialFiles = writeBytes(cmpPath, redPalette)
        && writeBytes(matPath, material)
        && writeBytes(spritePath, spriteBytes)
        && writeBytes(modelPath, modelWide);
    check(initialFiles,
        "Thing placement icon fixture writes MAT/CMP/SPR/3DO resources");
    if ( !initialFiles )
    {
        return;
    }

    const auto cmpStamp = std::filesystem::last_write_time(cmpPath);
    const auto modelStamp = std::filesystem::last_write_time(modelPath);
    check(modelWide.size() == modelNarrow.size(),
        "Thing placement model replacement keeps its byte length");
    if ( modelWide.size() != modelNarrow.size() )
    {
        return;
    }

    MainWindow window;
    window.setAttribute(Qt::WA_DontShowOnScreen);
    GlView* selectedPreviewView = window.findChild<GlView*>(
        QStringLiteral("thingPlacementSelectedPreviewView"));
    auto* selectedPreviewWatcher = selectedPreviewView
        ? selectedPreviewView->resourceDiskWatcherForTest()
        : nullptr;
    check(selectedPreviewWatcher != nullptr,
        "selected placement preview exposes its delegated resource watcher");
    check(selectedPreviewWatcher
            && !selectedPreviewWatcher->mountedArchiveWatchingEnabledForTest(),
        "selected placement preview delegates mounted archive watching to MainWindow");
    window.addResourceRootForTest(root);
    window.mutableDocumentStateForTest().level.masterCMP = "placement.cmp";
    window.invalidateThingPlacementResourcePreviewsForTest();

    TplValue spriteValue{};
    spriteValue.name = "sprite";
    spriteValue.type = ValueType::str;
    spriteValue.stringValue = "placement.spr";
    ThingTemplate spriteTemplate{};
    spriteTemplate.name = "placement_cache_sprite";
    spriteTemplate.parent = "none";
    spriteTemplate.values.push_back(spriteValue);

    const QSize iconSize(96, 96);
    const QImage spriteBefore =
        window.thingPlacementTemplateIconForTest(spriteTemplate, iconSize);
    const bool spriteReplacementWritten = writeBytes(cmpPath, greenPalette)
        && setStamp(cmpPath, cmpStamp);
    const QImage spriteWithoutInvalidation =
        window.thingPlacementTemplateIconForTest(spriteTemplate, iconSize);
    check(!spriteBefore.isNull()
            && containsExactColor(spriteBefore, QColor(255, 0, 0))
            && spriteReplacementWritten
            && !spriteWithoutInvalidation.isNull()
            && !imagesDiffer(spriteBefore, spriteWithoutInvalidation),
        "Selected SPR placement icon remains stable until explicit resource invalidation");

    window.invalidateThingPlacementResourcePreviewsForTest();
    const QImage spriteAfterInvalidation =
        window.thingPlacementTemplateIconForTest(spriteTemplate, iconSize);
    check(spriteReplacementWritten && !spriteAfterInvalidation.isNull()
            && containsExactColor(spriteAfterInvalidation, QColor(0, 255, 0))
            && imagesDiffer(spriteBefore, spriteAfterInvalidation),
        "Selected SPR placement icon reflects same-metadata palette replacement after explicit invalidation");

    const bool restoredRedPalette = writeBytes(cmpPath, redPalette)
        && setStamp(cmpPath, cmpStamp);
    check(restoredRedPalette,
        "Thing placement model fixture restores the independent red palette");
    window.invalidateThingPlacementResourcePreviewsForTest();

    TplValue modelValue{};
    modelValue.name = "model3do";
    modelValue.type = ValueType::str;
    modelValue.stringValue = "placement.3do";
    ThingTemplate modelTemplate{};
    modelTemplate.name = "placement_cache_model";
    modelTemplate.parent = "none";
    modelTemplate.values.push_back(modelValue);

    check(setStamp(modelPath, modelStamp),
        "Thing placement model fixture records its initial timestamp");
    const QImage modelBefore =
        window.thingPlacementTemplateIconForTest(modelTemplate, iconSize);
    const bool modelReplacementWritten = writeBytes(modelPath, modelNarrow)
        && setStamp(modelPath, modelStamp);
    const QImage modelWithoutInvalidation =
        window.thingPlacementTemplateIconForTest(modelTemplate, iconSize);
    check(!modelBefore.isNull() && containsRedTint(modelBefore)
            && modelReplacementWritten
            && !modelWithoutInvalidation.isNull()
            && !imagesDiffer(modelBefore, modelWithoutInvalidation),
        "Selected 3DO placement icon remains stable until explicit resource invalidation");

    window.invalidateThingPlacementResourcePreviewsForTest();
    const QImage modelAfterInvalidation =
        window.thingPlacementTemplateIconForTest(modelTemplate, iconSize);
    check(modelReplacementWritten && !modelAfterInvalidation.isNull()
            && containsRedTint(modelAfterInvalidation)
            && imagesDiffer(modelBefore, modelAfterInvalidation),
        "Selected 3DO placement icon reflects same-metadata geometry replacement after explicit invalidation");

    bool churnPreservesChanges = true;
    QImage previous{};
    for ( int revision = 0; revision < 8; ++revision )
    {
        const QByteArray& palette = (revision % 2) == 0 ? redPalette : greenPalette;
        churnPreservesChanges = writeBytes(cmpPath, palette)
            && setStamp(cmpPath, cmpStamp) && churnPreservesChanges;
        window.invalidateThingPlacementResourcePreviewsForTest();
        const QImage current =
            window.thingPlacementTemplateIconForTest(spriteTemplate, iconSize);
        if ( revision > 0 )
        {
            churnPreservesChanges = imagesDiffer(previous, current)
                && churnPreservesChanges;
        }
        previous = current;
    }
    check(churnPreservesChanges,
        "Thing placement icon cache refreshes every same-metadata revision during bounded churn");

    window.invalidateThingPlacementResourcePreviewsForTest();
    bool capacityIconsReady = true;
    for ( int index = 0; index < 200; ++index )
    {
        ThingTemplate capacityTemplate = spriteTemplate;
        capacityTemplate.name = "placement_capacity_" + std::to_string(index);
        const QImage icon = window.thingPlacementTemplateIconForTest(
            capacityTemplate, iconSize);
        capacityIconsReady = !icon.isNull()
            && containsExactColor(icon, QColor(0, 255, 0))
            && capacityIconsReady;
    }
    check(capacityIconsReady
            && window.thingPlacementTemplateIconCacheSizeForTest() == 192u,
        "Thing placement template icon cache remains bounded for more than 192 distinct keys");

    window.invalidateThingPlacementResourcePreviewsForTest();
    check(window.thingPlacementTemplateIconCacheSizeForTest() == 0u,
        "Thing placement template icon cache is empty after resource invalidation");

    ThingTemplate cacheProbe = spriteTemplate;
    cacheProbe.name = "placement_capacity_probe";
    const QImage firstAfterInvalidation =
        window.thingPlacementTemplateIconForTest(cacheProbe, iconSize);
    check(!firstAfterInvalidation.isNull()
            && containsExactColor(firstAfterInvalidation, QColor(0, 255, 0))
            && window.thingPlacementTemplateIconCacheSizeForTest() == 1u,
        "Thing placement template icon cache stores the first post-invalidation icon");

    const QImage repeatedAfterInvalidation =
        window.thingPlacementTemplateIconForTest(cacheProbe, iconSize);
    check(window.thingPlacementTemplateIconCacheSizeForTest() == 1u
            && !imagesDiffer(firstAfterInvalidation, repeatedAfterInvalidation),
        "Thing placement template icon cache reuses a repeated key without growing");
}

void runResourcePickerStorageMetadataTests()
{
    const auto diagnoseStorageText = [](
        const std::string& actual,
        const std::string& expected)
    {
        if ( actual != expected )
        {
            std::cerr << "Storage text diagnostic: actual=[" << actual
                << "] expected=[" << expected << "]\n";
        }
    };
    QTemporaryDir directory;
    check(directory.isValid(), "Resource picker storage metadata fixture root exists");
    if ( !directory.isValid() )
    {
        return;
    }

    const std::filesystem::path root(directory.path().toStdWString());
    std::filesystem::create_directories(root / "sound");
    std::filesystem::create_directories(root / "mat");
    const auto writeBytes = [](const std::filesystem::path& path, const QByteArray& bytes)
    {
        std::ofstream output(path, std::ios::binary | std::ios::trunc);
        if ( !output )
        {
            return false;
        }
        output.write(bytes.constData(), bytes.size());
        output.close();
        return static_cast<bool>(output);
    };

    const auto looseSound = root / "sound" / "Foo.WAV";
    const auto vanishingSound = root / "sound" / "Vanishing.WAV";
    const auto looseKey = root / "key" / "Tracked.Key";
    std::filesystem::create_directories(root / "key");
    const bool looseFilesWritten = writeBytes(looseSound, QByteArray("loose wav bytes"))
        && writeBytes(looseKey, QByteArray("loose key bytes"))
        && writeBytes(vanishingSound, QByteArray("will disappear after list population"));
    const auto gobPath = root / "selected.gob";
    const auto gooPath = root / "selected.goo";
    const bool archiveFilesWritten = writeBytes(gobPath, makeMultiEntryArchiveGob({
            {"Sound/Foo.WAV", QByteArray("archive wav bytes")},
            {"Sound/ArchiveOnly.WAV", QByteArray("archive only")},
            {"Key/Tracked.Key", QByteArray("keyframe")},
            {"Key/KeyArchiveOnly.Key", QByteArray("archive only keyframe")},
            {"CoG/Example.COG", QByteArray("# synthetic COG\n")},
            {"MiSc/SnD/Example.SND", QByteArray("synthetic sound class")},
            {"3do/PreviewBroken.3DO", QByteArray("not a 3DO model")},
            {"MiSc/Spr/PreviewBroken.SPR", QByteArray("not a sprite")},
            {"Mat/ArchiveOnly.MAT", QByteArray("synthetic MAT")},
            {"AI/Example.AI", QByteArray("synthetic AI")},
            {"misc/msg/Example.MSG", QByteArray("synthetic message")},
            {"CMP/Example.CMP", QByteArray("synthetic CMP")},
            {"misc/pup/Example.PUP", QByteArray("synthetic puppet")},
            {"misc/par/Example.PAR", QByteArray("synthetic particle")},
            {"misc/Example.UNI", QByteArray("synthetic UNI")},
        }))
        && writeBytes(gooPath, makeMultiEntryArchiveGob({
            {"SoUnD/GooOnly.WAV", QByteArray("goo archive only")},
            {"MiSc/Key/GooOnly.Key", QByteArray("goo archive keyframe")},
        }));
    check(looseFilesWritten && archiveFilesWritten,
        "Resource picker fixture writes loose rows and valid case-preserved GOB/GOO archives");
    if ( !looseFilesWritten || !archiveFilesWritten )
    {
        return;
    }

    auto resources = std::make_shared<assets::ResourceLocator>();
    resources->addRoot(root);
    resources->mountGob(gobPath);
    resources->mountGob(gooPath);

    ui::ResourcePickerOptions wavOptions{};
    wavOptions.kind = ui::ResourcePickerKind::wav;
    wavOptions.previewProjectType = ProjectType::JKDF2;
    const auto wavTrace = ui::resourcePickerStorageMetadataTraceForTest(resources, wavOptions, {
        {0, -1, "Foo.WAV", {}},
        {0, -1, "ArchiveOnly.WAV", {}},
        {0, -1, "GooOnly.WAV", {}},
        {0, -1, "Vanishing.WAV", [vanishingSound]()
            {
                std::error_code error{};
                std::filesystem::remove(vanishingSound, error);
            }},
        {0, -1, "", {}},
    });
    const std::string expectedLoose = expectedFilesystemStorageText(looseSound);
    const QString expectedArchiveOnly =
        QStringLiteral("Storage: selected.gob: Sound/ArchiveOnly.WAV");
    const QString expectedGoo = QStringLiteral("Storage: selected.goo: SoUnD/GooOnly.WAV");
    const QString expectedUnavailable = QStringLiteral("Storage: Unavailable");
    const bool traceRowsReady = wavTrace.size() == 5u
        && std::all_of(wavTrace.begin(), wavTrace.end(), [](const auto& snapshot)
            { return snapshot.sourceFound && snapshot.itemFound; });
    check(traceRowsReady,
        "WAV picker fixture selects actual All-source rows through archive and filesystem changes");
    check(wavTrace.size() == 5u
            && std::all_of(wavTrace.begin(), wavTrace.end(), [](const auto& snapshot)
                { return snapshot.labelFound; }),
        "Resource picker exposes selected storage metadata across source changes and clearing");
    if ( wavTrace.size() == 5u )
    {
        diagnoseStorageText(wavTrace[0].text, expectedLoose);
        check(wavTrace[0].text == expectedLoose,
            "All-source picker metadata follows ResourceLocator precedence to the winning loose path");
        check(QString::fromStdString(wavTrace[1].text) == expectedArchiveOnly,
            "All-source archive fallback reports the indexed entry without an absolute archive path");
        check(QString::fromStdString(wavTrace[2].text) == expectedGoo,
            "GOO metadata uses the real archive extension and preserves the entry's original spelling");
        check(QString::fromStdString(wavTrace[3].text) == expectedUnavailable,
            "A selected loose row that disappears reports unavailable instead of stale storage metadata");
        check(wavTrace[4].text.empty(), "Clearing selection removes prior storage metadata");
    }

    auto overrideResources = std::make_shared<assets::ResourceLocator>(*resources);
    int primaryOverrideReads = 0;
    overrideResources->setPrimaryResourceOverride(
        "Foo.WAV",
        [&primaryOverrideReads]() -> std::optional<std::vector<std::byte>>
        {
            ++primaryOverrideReads;
            return std::vector<std::byte>{std::byte{0x7f}};
        });
    const auto overrideTrace = ui::resourcePickerStorageMetadataTraceForTest(
        overrideResources,
        wavOptions,
        {{0, -1, "Foo.WAV", {}, false}});
    check(overrideTrace.size() == 1u && overrideTrace[0].labelFound
            && overrideTrace[0].text == "Storage: Project data"
            && primaryOverrideReads == 0,
        "Primary project-data provenance is shown without invoking a lazy payload reader");

    ui::ResourcePickerOptions keyOptions{};
    keyOptions.kind = ui::ResourcePickerKind::key;
    keyOptions.previewProjectType = ProjectType::JKDF2;
    bool archiveRewrittenAfterMount = false;
    const auto keyTrace = ui::resourcePickerStorageMetadataTraceForTest(resources, keyOptions, {
        {0, -1, "Tracked.Key", {}},
        {1, 0, "Tracked.Key", {}},
        {2, 0, "Tracked.Key", [gobPath, writeBytes, &archiveRewrittenAfterMount]()
            {
                archiveRewrittenAfterMount = writeBytes(gobPath, makeMultiEntryArchiveGob({
                    {"Sound/Foo.WAV", QByteArray("archive wav bytes")},
                    {"Sound/ArchiveOnly.WAV", QByteArray("archive only")},
                    {"Stash/Tracked.Key", QByteArray("keyframe")},
                    {"Key/KeyArchiveOnly.Key", QByteArray("archive only keyframe")},
                    {"CoG/Example.COG", QByteArray("# synthetic COG\n")},
                    {"MiSc/SnD/Example.SND", QByteArray("synthetic sound class")},
                    {"3do/PreviewBroken.3DO", QByteArray("not a 3DO model")},
                    {"MiSc/Spr/PreviewBroken.SPR", QByteArray("not a sprite")},
                    {"Mat/ArchiveOnly.MAT", QByteArray("synthetic MAT")},
                    {"AI/Example.AI", QByteArray("synthetic AI")},
                    {"misc/msg/Example.MSG", QByteArray("synthetic message")},
                    {"CMP/Example.CMP", QByteArray("synthetic CMP")},
                    {"misc/pup/Example.PUP", QByteArray("synthetic puppet")},
                    {"misc/par/Example.PAR", QByteArray("synthetic particle")},
                    {"misc/Example.UNI", QByteArray("synthetic UNI")},
                }));
            }},
        {2, 0, "KeyArchiveOnly.Key", {}},
        {2, 1, "GooOnly.Key", {}},
        {2, 0, "", {}},
        {0, -1, "KeyArchiveOnly.Key", {}},
    });
    check(keyTrace.size() == 7u
            && std::all_of(keyTrace.begin(), keyTrace.end(), [](const auto& snapshot)
                { return snapshot.sourceFound && snapshot.itemFound && snapshot.labelFound; }),
        "File-backed picker exposes storage metadata through All, disk, GOB, GOO, and cleared selection");
    if ( keyTrace.size() == 7u )
    {
        const std::string expectedKeyLoose = expectedFilesystemStorageText(looseKey);
        diagnoseStorageText(keyTrace[0].text, expectedKeyLoose);
        check(keyTrace[0].text == expectedKeyLoose && keyTrace[0].itemBold,
            "All-source key metadata follows the winning loose path and keeps loose-copy emphasis");
        diagnoseStorageText(keyTrace[1].text, expectedKeyLoose);
        check(keyTrace[1].text == expectedKeyLoose && keyTrace[1].itemBold,
            "Explicit disk source metadata shows its full path and stays bold");
        check(keyTrace[2].text == "Storage: selected.gob: Key/Tracked.Key"
                && keyTrace[2].itemBold && archiveRewrittenAfterMount,
            "Explicit GOB selection keeps the mounted index's origin after disk archive names change");
        check(keyTrace[3].text == "Storage: selected.gob: Key/KeyArchiveOnly.Key"
                && !keyTrace[3].itemBold,
            "Archive-only GOB entry is plain and retains the complete indexed path");
        check(keyTrace[4].text == "Storage: selected.goo: MiSc/Key/GooOnly.Key"
                && !keyTrace[4].itemBold,
            "Explicit GOO source reports its actual extension and case-preserved entry");
        check(keyTrace[5].text.empty(), "Changing to no key selection clears the displayed origin");
        check(keyTrace[6].text == "Storage: selected.gob: Key/KeyArchiveOnly.Key",
            "All-source key fallback keeps candidate/root/archive lookup precedence");
    }

    const std::filesystem::path longRoot = root
        / std::filesystem::path(std::string(36, 'a'))
        / std::filesystem::path(std::string(36, 'b'));
    const auto longKeyPath = longRoot / "key" / "LongPath.Key";
    std::filesystem::create_directories(longKeyPath.parent_path());
    const bool longPathWritten = writeBytes(longKeyPath, QByteArray("long path keyframe"));
    check(longPathWritten, "Resource picker writes a long-path metadata fixture");
    if ( longPathWritten )
    {
        ui::ResourcePickerOptions longPathOptions = keyOptions;
        longPathOptions.preferredDiskRoot = longRoot;
        const auto longPathTrace = ui::resourcePickerStorageMetadataTraceForTest(resources, longPathOptions, {
            {0, -1, "LongPath.Key", {}},
        });
        const std::string expectedLongPath = expectedFilesystemStorageText(longKeyPath);
        if ( longPathTrace.size() == 1u )
        {
            diagnoseStorageText(longPathTrace[0].text, expectedLongPath);
        }
        check(longPathTrace.size() == 1u && longPathTrace[0].sourceFound
                && longPathTrace[0].itemFound && longPathTrace[0].labelFound
                && longPathTrace[0].text == expectedLongPath
                && longPathTrace[0].labelWordWrap
                && longPathTrace[0].labelMaximumWidth <= 480,
            "Long full filesystem origins remain literal and wrap within the preview column width");
    }

    for ( const auto [kind, name, expected] : {
            std::tuple{ui::ResourcePickerKind::cog, std::string("Example.COG"),
                std::string("Storage: selected.gob: CoG/Example.COG")},
            std::tuple{ui::ResourcePickerKind::material, std::string("ArchiveOnly.MAT"),
                std::string("Storage: selected.gob: Mat/ArchiveOnly.MAT")},
            std::tuple{ui::ResourcePickerKind::key, std::string("KeyArchiveOnly.Key"),
                std::string("Storage: selected.gob: Key/KeyArchiveOnly.Key")},
            std::tuple{ui::ResourcePickerKind::wav, std::string("ArchiveOnly.WAV"),
                std::string("Storage: selected.gob: Sound/ArchiveOnly.WAV")},
            std::tuple{ui::ResourcePickerKind::ai, std::string("Example.AI"),
                std::string("Storage: selected.gob: AI/Example.AI")},
            std::tuple{ui::ResourcePickerKind::message, std::string("Example.MSG"),
                std::string("Storage: selected.gob: misc/msg/Example.MSG")},
            std::tuple{ui::ResourcePickerKind::cmp, std::string("Example.CMP"),
                std::string("Storage: selected.gob: CMP/Example.CMP")},
            std::tuple{ui::ResourcePickerKind::snd, std::string("Example.SND"),
                std::string("Storage: selected.gob: MiSc/SnD/Example.SND")},
            std::tuple{ui::ResourcePickerKind::pup, std::string("Example.PUP"),
                std::string("Storage: selected.gob: misc/pup/Example.PUP")},
            std::tuple{ui::ResourcePickerKind::model3do, std::string("PreviewBroken.3DO"),
                std::string("Storage: selected.gob: 3do/PreviewBroken.3DO")},
            std::tuple{ui::ResourcePickerKind::spr, std::string("PreviewBroken.SPR"),
                std::string("Storage: selected.gob: MiSc/Spr/PreviewBroken.SPR")},
            std::tuple{ui::ResourcePickerKind::par, std::string("Example.PAR"),
                std::string("Storage: selected.gob: misc/par/Example.PAR")},
            std::tuple{ui::ResourcePickerKind::uni, std::string("Example.UNI"),
                std::string("Storage: selected.gob: misc/Example.UNI")},
        } )
    {
        ui::ResourcePickerOptions options{};
        options.kind = kind;
        options.previewProjectType = ProjectType::JKDF2;
        const bool categoryBackedSource = kind == ui::ResourcePickerKind::material
            || kind == ui::ResourcePickerKind::wav;
        const auto trace = ui::resourcePickerStorageMetadataTraceForTest(resources, options, {
            {categoryBackedSource ? 0 : 2, categoryBackedSource ? -1 : 0, name, {}},
        });
        const bool exactMetadata = trace.size() == 1u && trace[0].sourceFound && trace[0].itemFound
            && trace[0].labelFound && trace[0].labelWordWrap
            && trace[0].labelMaximumWidth <= 480 && trace[0].text == expected;
        if ( !exactMetadata )
        {
            std::fprintf(stderr,
                "picker GOB origin diagnostic: kind=%d name=%s traceSize=%zu source=%d item=%d label=%d wrap=%d maxWidth=%d expected=[%s] actual=[%s]\n",
                static_cast<int>(kind), name.c_str(), trace.size(),
                trace.empty() ? 0 : static_cast<int>(trace[0].sourceFound),
                trace.empty() ? 0 : static_cast<int>(trace[0].itemFound),
                trace.empty() ? 0 : static_cast<int>(trace[0].labelFound),
                trace.empty() ? 0 : static_cast<int>(trace[0].labelWordWrap),
                trace.empty() ? -1 : trace[0].labelMaximumWidth, expected.c_str(),
                trace.empty() ? "<no snapshot>" : trace[0].text.c_str());
        }
        check(exactMetadata,
            "Every file-backed picker kind exposes the exact case-preserved mounted GOB origin in its metadata");
    }

    ui::ResourcePickerOptions sndTextOptions{};
    sndTextOptions.kind = ui::ResourcePickerKind::snd;
    sndTextOptions.initialName = "Example.SND";
    sndTextOptions.previewProjectType = ProjectType::JKDF2;
    QString sndPreviewText{};
    QString sndSeparateStorage{};
    bool sndPickerSeen = false;
    QTimer inspectSndPicker{};
    inspectSndPicker.setInterval(10);
    QObject::connect(&inspectSndPicker, &QTimer::timeout, [&]()
        {
            QDialog* const dialog = QApplication::activeModalWidget()
                ? qobject_cast<QDialog*>(QApplication::activeModalWidget()) : nullptr;
            if ( !dialog || !dialog->windowTitle().contains(QStringLiteral("Sound Class")) )
            {
                return;
            }
            auto* const preview = dialog->findChild<QPlainTextEdit*>();
            if ( !preview || !preview->toPlainText().contains(QStringLiteral("Name: Example.SND")) )
            {
                return;
            }
            sndPickerSeen = true;
            sndPreviewText = preview->toPlainText();
            if ( const QLabel* const storage = dialog->findChild<QLabel*>(
                    QStringLiteral("resourcePickerStorageMetadata")) )
            {
                if ( storage->isVisible() )
                {
                    sndSeparateStorage = storage->text();
                }
            }
            dialog->reject();
            inspectSndPicker.stop();
        });
    QTimer closeSndPickerTimeout{};
    closeSndPickerTimeout.setSingleShot(true);
    QObject::connect(&closeSndPickerTimeout, &QTimer::timeout, []()
        {
            if ( QWidget* const modal = QApplication::activeModalWidget() )
            {
                modal->close();
            }
        });
    inspectSndPicker.start();
    closeSndPickerTimeout.start(5000);
    (void)ui::pickResource(nullptr, resources, sndTextOptions);
    inspectSndPicker.stop();
    closeSndPickerTimeout.stop();
    const QString canonicalSndOrigin = QStringLiteral("Storage: selected.gob: MiSc/SnD/Example.SND");
    check(sndPickerSeen && sndPreviewText.contains(canonicalSndOrigin)
            && !sndPreviewText.contains(QStringLiteral("Disk:"))
            && !sndPreviewText.contains(QStringLiteral("Archive:"))
            && !sndPreviewText.contains(QStringLiteral("Storage: GOB"))
            && sndSeparateStorage.isEmpty(),
        "Sound Class preview contains one selectable canonical Storage origin and no duplicate source line or separate storage box");

    ThingTemplate archivedTemplate{};
    archivedTemplate.name = "picker_archive_template";
    const std::vector<ThingTemplate> archivedTemplateOverrides{archivedTemplate};
    ui::ResourcePickerOptions archivedTemplateOptions{};
    archivedTemplateOptions.kind = ui::ResourcePickerKind::template_;
    archivedTemplateOptions.previewProjectType = ProjectType::JKDF2;
    archivedTemplateOptions.templateOverrides = &archivedTemplateOverrides;
    const auto archivedTemplateTrace = ui::resourcePickerStorageMetadataTraceForTest(resources,
        archivedTemplateOptions, {{0, -1, "picker_archive_template", {}}});
    check(archivedTemplateTrace.size() == 1u && archivedTemplateTrace[0].sourceFound
            && archivedTemplateTrace[0].itemFound && archivedTemplateTrace[0].labelFound
            && archivedTemplateTrace[0].text == "Storage: Template database",
        "Template picker metadata identifies project template data without inventing a file origin");

    const QByteArray material = fixtures::makeIndexedMatFixture(6u);
    const auto materialPath = root / "mat" / "Swatch.MAT";
    const bool materialWritten = writeBytes(materialPath, material);
    check(materialWritten, "Resource picker material preview fixture writes a valid MAT");
    if ( materialWritten )
    {
        ui::ResourcePickerOptions materialOptions{};
        materialOptions.kind = ui::ResourcePickerKind::material;
        materialOptions.previewProjectType = ProjectType::JKDF2;
        const auto materialTrace = ui::resourcePickerStorageMetadataTraceForTest(resources, materialOptions, {
            {0, -1, "Swatch.MAT", {}},
        });
        const std::string expectedMaterialPath = expectedFilesystemStorageText(materialPath);
        if ( materialTrace.size() == 1u )
        {
            diagnoseStorageText(materialTrace[0].text, expectedMaterialPath);
        }
        check(materialTrace.size() == 1u && materialTrace[0].sourceFound
                && materialTrace[0].itemFound && materialTrace[0].labelFound
                && materialTrace[0].text == expectedMaterialPath,
            "Storage metadata remains available beside a rendered MAT preview");
    }

    ThingTemplate templateOverride{};
    templateOverride.name = "picker_storage_template";
    const std::vector<ThingTemplate> templateOverrides{templateOverride};
    ui::ResourcePickerOptions templateOptions{};
    templateOptions.kind = ui::ResourcePickerKind::template_;
    templateOptions.previewProjectType = ProjectType::JKDF2;
    templateOptions.templateOverrides = &templateOverrides;
    const auto templateTrace = ui::resourcePickerStorageMetadataTraceForTest(resources, templateOptions, {
        {0, -1, "picker_storage_template", {}},
    });
    check(templateTrace.size() == 1u && templateTrace[0].sourceFound
            && templateTrace[0].itemFound && templateTrace[0].labelFound
            && templateTrace[0].text == "Storage: Template database",
        "Template picker metadata identifies project data instead of inventing a filesystem or archive source");
}

} // namespace

void runResourcePickerReleaseTests()
{
    runResourcePickerStorageMetadataTests();
    runArchiveReloadRetryTests();
    runNestedResourcePickerArchiveRefreshTests();
    runDelegatedThumbnailArchiveWatchTests();
    runDelegatedPlacementArchiveWatchTests();
    runThingPlacementTemplateIconCacheTests();
    // A source row is a view of that source's bytes, even when another root
    // shadows the same asset in the project's normal search order.
    for (const auto kind : {ui::ResourcePickerKind::model3do, ui::ResourcePickerKind::spr})
    {
        QTemporaryDir first, second;
        check(first.isValid() && second.isValid(), "resource picker source fixture roots exist");
        if (!first.isValid() || !second.isValid()) continue;
        const std::string name = kind == ui::ResourcePickerKind::spr ? "same.spr" : "same.3do";
        const std::string folder = kind == ui::ResourcePickerKind::spr ? "spr" : "3do";
        const QByteArray firstBytes = kind == ui::ResourcePickerKind::spr
            ? QByteArray("fallback.mat 0 2.0 1.0 4 3 3 0.0 0.0 0.0 0.0\n")
            : fixtures::makeDiskChangeModelFixture("fallback.mat", "selected first root", 2.0);
        const QByteArray secondBytes = kind == ui::ResourcePickerKind::spr
            ? QByteArray("fallback.mat 0 4.0 1.0 4 3 3 0.0 0.0 0.0 0.0\n")
            : fixtures::makeDiskChangeModelFixture("fallback.mat", "shadowing second root", 4.0);
        const QByteArray gobBytes = kind == ui::ResourcePickerKind::spr
            ? QByteArray("fallback.mat 0 6.0 1.0 4 3 3 0.0 0.0 0.0 0.0\n")
            : fixtures::makeDiskChangeModelFixture("fallback.mat", "selected archive", 6.0);
        auto resources = std::make_shared<assets::ResourceLocator>();
        const auto firstPath = std::filesystem::path(first.path().toStdWString());
        const auto secondPath = std::filesystem::path(second.path().toStdWString());
        std::filesystem::create_directories(firstPath / folder);
        std::filesystem::create_directories(secondPath / folder);
        std::ofstream(firstPath / folder / name, std::ios::binary).write(firstBytes.data(), firstBytes.size());
        std::ofstream(secondPath / folder / name, std::ios::binary).write(secondBytes.data(), secondBytes.size());
        std::ofstream(firstPath / "fallback.mat", std::ios::binary) << "dependency fallback";
        QByteArray archive("GOB ", 4);
        const auto appendU32 = [&archive](std::uint32_t value)
        { for (int i = 0; i < 4; ++i) archive.append(static_cast<char>((value >> (8*i)) & 0xff)); };
        appendU32(0x14); appendU32(0x0c); appendU32(1); appendU32(152);
        appendU32(static_cast<std::uint32_t>(gobBytes.size()));
        QByteArray archiveName = QByteArray::fromStdString(folder + "/" + name);
        archiveName.resize(128, '\0');
        archive += archiveName;
        archive += gobBytes;
        const auto archivePath = firstPath / "selected.gob";
        std::ofstream(archivePath, std::ios::binary).write(archive.data(), archive.size());
        resources->addRoot(firstPath);
        resources->addRoot(secondPath);
        resources->mountGob(archivePath);
        ui::ResourcePickerOptions options;
        options.kind = kind;
        options.previewProjectType = ProjectType::IJIM;
        // Probe the exact snapshot used by preparation and thumbnail workers.
        for (int sourceKind : {1, 2})
        {
            const auto description = ui::resourcePickerSourceDescriptionForTest(*resources, options, sourceKind, 0, name);
            const QString expectedDescription = sourceKind == 1
                ? QString::fromStdString(expectedFilesystemStorageText(firstPath / folder / name))
                : QStringLiteral("Storage: selected.gob: ") + QString::fromStdString(folder + "/" + name);
            if ( description != expectedDescription )
            {
                std::fprintf(stderr,
                    "picker selected-source diagnostic: kind=%d sourceKind=%d index=0 name=%s expected=[%s] actual=[%s]\n",
                    static_cast<int>(kind), sourceKind, name.c_str(),
                    expectedDescription.toUtf8().constData(), description.toUtf8().constData());
            }
            check(description == expectedDescription,
                "Resource picker text metadata uses one canonical loose path or archive basename plus case-preserved entry");
            const auto scoped = ui::resourcePickerPreviewResourcesForTest(resources, options, sourceKind, 0);
            check(scoped != nullptr, "resource picker fixture selects requested disk/GOB source row");
            if (!scoped) continue;
            const auto expected = sourceKind == 1 ? firstBytes.toStdString() : gobBytes.toStdString();
            check(scoped->tryReadText(name) == std::optional<std::string>(expected),
                "resource picker model/SPR preview snapshot reads selected disk/GOB bytes despite loose shadow");
            check(scoped->tryReadText("fallback.mat") == std::optional<std::string>("dependency fallback"),
                "resource picker source preview retains project dependency fallback");
        }
        const auto beforeRemount = ui::resourcePickerSourceNamesForTest(*resources, options, 2, 0);
        const auto archiveStamp = std::filesystem::last_write_time(archivePath);
        const std::string replacementName = kind == ui::ResourcePickerKind::spr ? "else.spr" : "else.3do";
        archive.replace(QByteArray::fromStdString(folder + "/" + name),
            QByteArray::fromStdString(folder + "/" + replacementName));
        std::ofstream(archivePath, std::ios::binary).write(archive.data(), archive.size());
        std::filesystem::last_write_time(archivePath, archiveStamp);
        resources->reloadGobs();
        const auto afterRemount = ui::resourcePickerSourceNamesForTest(*resources, options, 2, 0);
        check(beforeRemount == std::vector<std::string>{name}
                && afterRemount == std::vector<std::string>{replacementName},
            "resource picker source enumeration refreshes after same-metadata archive remount");
    }

    for (const auto pickerKind : {ui::ResourcePickerKind::material, ui::ResourcePickerKind::spr})
    for (const bool initiallyMissing : {false, true})
    {
        QTemporaryDir directory;
        check(directory.isValid(), "MAT palette replacement fixture root exists");
        if (!directory.isValid()) continue;
        const auto root = std::filesystem::path(directory.path().toStdWString());
        auto resources = std::make_shared<assets::ResourceLocator>();
        resources->addRoot(root);
        auto mat = fixtures::makeIndexedMatFixture(6);
        if (pickerKind == ui::ResourcePickerKind::spr)
        {
            // Two cel headers precede two complete texture-data blocks.
            // Each animation cel needs its own pixels, even when identical.
            const QByteArray cel = mat.mid(76, 40);
            const QByteArray texture = mat.mid(116);
            mat.insert(116, cel);
            mat.append(texture);
            mat[12] = 2;
            mat[16] = 2;
            bool decodedTwoCels = false;
            try
            {
                const auto* begin = reinterpret_cast<const std::byte*>(mat.constData());
                const auto decoded = assets::MatFile::parse({begin, begin + mat.size()});
                decodedTwoCels = decoded.celCount() == 2 && decoded.isAnimated()
                    && decoded.cel(0).width == 2 && decoded.cel(1).width == 2
                    && decoded.cel(0).height == 2 && decoded.cel(1).height == 2;
            }
            catch (const std::exception&) {}
            check(decodedTwoCels, "SPR palette fixture independently decodes both complete animation cels");
        }
        std::ofstream(root / "palette_test.mat", std::ios::binary).write(mat.data(), mat.size());
        const auto first = fixtures::makeClassicCmpFixture(6, QColor(255, 0, 0));
        if (!initiallyMissing)
            std::ofstream(root / "palette_test.cmp", std::ios::binary).write(first.data(), first.size());
        ui::ResourcePickerOptions options;
        options.kind = pickerKind;
        std::ofstream(root / "palette_test.spr", std::ios::binary)
            << "palette_test.mat 0 2 1 4 3 3 0 0 0 0\n";
        options.previewProjectType = ProjectType::JKDF2;
        options.previewCmp = "palette_test.cmp";
        bool watched = false;
        const auto images = ui::resourcePickerPaletteChangeForTest(resources, options,
            pickerKind == ui::ResourcePickerKind::spr
                ? QStringLiteral("palette_test.spr") : QStringLiteral("palette_test.mat"), [&]()
            {
                const auto second = fixtures::makeClassicCmpFixture(6, QColor(0, 255, 0));
                std::ofstream(root / "palette_test.cmp", std::ios::binary).write(second.data(), second.size());
                std::filesystem::last_write_time(root / "palette_test.cmp",
                    std::filesystem::file_time_type::clock::now() + std::chrono::seconds(10));
            }, watched);
        check(watched, "MAT/SPR picker watches selected dependencies and effective or missing palette");
        const std::size_t expectedFrames = pickerKind == ui::ResourcePickerKind::spr ? 2u : 1u;
        bool correctColors = images.first.size() == expectedFrames && images.second.size() == expectedFrames;
        for (std::size_t frame = 0; frame < std::min(images.first.size(), images.second.size()); ++frame)
        {
            const QImage& before = images.first[frame];
            const QImage& after = images.second[frame];
            correctColors = correctColors && !before.isNull() && !after.isNull()
                && after.pixelColor(0, 0) == QColor(0, 255, 0)
                && (initiallyMissing || before.pixelColor(0, 0) == QColor(255, 0, 0));
        }
        const auto describeFrames = [](const std::vector<QImage>& frames)
        {
            QString result;
            for (const auto& frame : frames)
                result += frame.isNull() ? QStringLiteral(" null")
                    : QStringLiteral(" %1x%2:%3").arg(frame.width()).arg(frame.height())
                        .arg(frame.pixelColor(0, 0).name(QColor::HexArgb));
            return result;
        };
        const auto diagnostic = QStringLiteral("MAT/SPR picker same-dialog palette refresh kind=%1 missing=%2 before=[%3] after=[%4]")
            .arg(pickerKind == ui::ResourcePickerKind::spr ? "SPR" : "MAT").arg(initiallyMissing)
            .arg(describeFrames(images.first)).arg(describeFrames(images.second)).toStdString();
        check(correctColors, diagnostic.c_str());
    }

    {
        QTemporaryDir directory;
        check(directory.isValid(), "Template palette validation fixture root exists");
        if (directory.isValid())
        {
            const auto root = std::filesystem::path(directory.path().toStdWString());
            assets::ResourceLocator resources;
            resources.addRoot(root);
            check(!templateEditorPaletteAvailableForTest(resources, "absent.cmp"),
                "Template Editor missing CMP produces no palette pointer");
            std::ofstream(root / "invalid.cmp", std::ios::binary) << "invalid CMP bytes";
            check(!templateEditorPaletteAvailableForTest(resources, "invalid.cmp"),
                "Template Editor malformed CMP produces no disengaged optional palette pointer");
            const auto valid = fixtures::makeClassicCmpFixture(6, QColor(0,255,0));
            std::ofstream(root / "valid.cmp", std::ios::binary).write(valid.data(), valid.size());
            check(templateEditorPaletteAvailableForTest(resources, "valid.cmp"),
                "Template Editor valid CMP still supplies its parsed palette");
        }
    }

    MainWindow window;
    window.setAttribute(Qt::WA_DontShowOnScreen);
    ThingTemplate authored;
    authored.name = "precise_bounds";
    authored.parent = "none";
    authored.bbox.Set(1e-8, -0.0, -1e-11,
        std::nextafter(1.0, 2.0), 123456789.1250001, 9.876543210987654);
    std::unique_ptr<QDialog> dialog(window.createTemplateEditorDialogForTest({authored}));
    auto* buttons = dialog->findChild<QDialogButtonBox*>();
    check(buttons && buttons->button(QDialogButtonBox::Save),
        "Template Editor precise bounds fixture exposes Accept");
    if (buttons && buttons->button(QDialogButtonBox::Save))
        buttons->button(QDialogButtonBox::Save)->click();
    const auto saved = templateEditorTemplatesForTest(dialog.get());
    const auto sameBits = [](double a, double b)
    { return std::bit_cast<std::uint64_t>(a) == std::bit_cast<std::uint64_t>(b); };
    check(dialog->result() == QDialog::Accepted && saved.size() == 1,
        "Template Editor accepts an unchanged valid template");
    if (saved.size() == 1)
    {
        const auto& a = authored.bbox;
        const auto& b = saved.front().bbox;
        check(sameBits(a.x1,b.x1) && sameBits(a.y1,b.y1) && sameBits(a.z1,b.z1)
            && sameBits(a.x2,b.x2) && sameBits(a.y2,b.y2) && sameBits(a.z2,b.z2),
            "Template Editor unchanged Accept preserves exact bounds beyond display precision/range");
    }
    std::unique_ptr<QDialog> editedDialog(window.createTemplateEditorDialogForTest({authored}));
    auto* editedX = editedDialog->findChild<ui::ScrubFloatEdit*>(QStringLiteral("templateEditorBBoxX1"));
    check(editedX != nullptr, "Template Editor bounds fixture exposes the X1 coordinate");
    const auto typeValue = [](ui::ScrubFloatEdit* edit, const double value)
    {
        if (!edit) return;
        edit->lineEdit()->setText(QString::fromStdString(sed::format::authoredDouble(value)));
        QMetaObject::invokeMethod(edit->lineEdit(), "textEdited", Qt::DirectConnection,
            Q_ARG(QString, edit->lineEdit()->text()));
        QMetaObject::invokeMethod(edit->lineEdit(), "editingFinished", Qt::DirectConnection);
    };
    typeValue(editedX, 0.5);
    const auto edited = templateEditorTemplatesForTest(editedDialog.get());
    if (edited.size() == 1)
    {
        const auto& a = authored.bbox;
        const auto& b = edited.front().bbox;
        check(b.x1 == 0.5 && sameBits(a.y1,b.y1) && sameBits(a.z1,b.z1)
            && sameBits(a.x2,b.x2) && sameBits(a.y2,b.y2) && sameBits(a.z2,b.z2),
            "Template Editor changing one bound preserves all other authored coordinates exactly");
    }
    std::unique_ptr<QDialog> preciseEditDialog(window.createTemplateEditorDialogForTest({authored}));
    auto* preciseX = preciseEditDialog->findChild<ui::ScrubFloatEdit*>(QStringLiteral("templateEditorBBoxX1"));
    check(preciseX != nullptr, "Template BBOX uses exact-state numeric editing");
    if (preciseX)
    {
        QToolButton* increase = nullptr;
        for (auto* button : preciseX->findChildren<QToolButton*>())
            if (button->text() == ">") increase = button;
        if (increase) increase->click();
        const auto stepped = templateEditorTemplatesForTest(preciseEditDialog.get());
        check(increase && stepped.size() == 1 && sameBits(stepped[0].bbox.x1, authored.bbox.x1 + 0.1),
            "Template BBOX step starts from exact subdisplay authored base");
        typeValue(preciseX, std::nextafter(1.0, 2.0));
        const auto typed = templateEditorTemplatesForTest(preciseEditDialog.get());
        check(typed.size() == 1 && sameBits(typed[0].bbox.x1, std::nextafter(1.0, 2.0))
                && sameBits(typed[0].bbox.x2, authored.bbox.x2),
            "Template BBOX typed edits retain all digits without touching other axes");
    }

    std::unique_ptr<QDialog> pendingDialog(window.createTemplateEditorDialogForTest({authored}));
    auto* pendingX = pendingDialog->findChild<ui::ScrubFloatEdit*>(QStringLiteral("templateEditorBBoxX1"));
    if (pendingX)
    {
        pendingX->lineEdit()->selectAll();
        pendingX->lineEdit()->insert(QStringLiteral("0.0000000123456789012345"));
        auto* pendingButtons = pendingDialog->findChild<QDialogButtonBox*>();
        if (pendingButtons && pendingButtons->button(QDialogButtonBox::Save))
            pendingButtons->button(QDialogButtonBox::Save)->click();
        const auto accepted = templateEditorTemplatesForTest(pendingDialog.get());
        check(pendingDialog->result() == QDialog::Accepted && accepted.size() == 1
                && sameBits(accepted[0].bbox.x1, 0.0000000123456789012345)
                && sameBits(accepted[0].bbox.y2, authored.bbox.y2),
            "Template Accept commits pending precise text while preserving untouched out-of-range bounds");
    }

    // Independently decode written tokens: the handler's authoring parser
    // must not also define the expected result of these commit regressions.
    const auto writtenLightNumbers = [](const TplValue* value) -> std::optional<std::vector<double>>
    {
        if (!value) return std::nullopt;
        QString text = QString::fromStdString(value->asString());
        for (const auto delimiter : {'(', ')', '/', ',', ':'}) text.replace(QChar(delimiter), QChar(' '));
        std::vector<double> numbers;
        for (const auto& token : text.split(QChar(' '), Qt::SkipEmptyParts))
        {
            bool ok = false;
            const double number = token.toDouble(&ok);
            if (!ok || !std::isfinite(number)) return std::nullopt;
            numbers.push_back(number);
        }
        return numbers;
    };

    // Exercise the real MainWindow signal commit, not only its formatter.
    for (const auto project : {ProjectType::IJIM, ProjectType::JKDF2})
    {
        editor::DocumentState state;
        state.level.kind = project;
        state.level.sectors.emplace_back();
        auto& owner = state.level.newThing();
        owner.sector = 0;
        owner.yaw = 33.0;
        const TVector originalOffset{std::nextafter(1.0, 2.0), -2.123456789012345, 1e-8};
        const double authoredRange = 4.123456789012345;
        const double red = std::nextafter(0.5, 1.0);
        const auto light = project == ProjectType::IJIM
            ? sed::formatTplTuple({red, 0.234567890123456, 0.123456789012345, authoredRange})
            : sed::format::authoredDouble(authoredRange);
        owner.values = {sed::deserializeTplValue("type", "actor"),
            sed::deserializeTplValue("typeflags", "0x4"),
            sed::deserializeTplValue("lightintensity", light),
            sed::deserializeTplValue("light", light),
            sed::deserializeTplValue("lightoffset", sed::formatTplTuple(
                {originalOffset.x, originalOffset.y, originalOffset.z}))};
        // Deliberately noncanonical but valid authored tokens must survive
        // a no-op without producing a formatting-only history record.
        owner.values[2].stringValue += ' ';
        owner.values[3].stringValue += ' ';
        owner.values[4].stringValue += ' ';
        const auto untouchedHead = owner.values[2].asString();
        const auto untouchedRegular = owner.values[3].asString();
        const auto untouchedOffset = owner.values[4].asString();
        state.selection.things.insert(0);
        window.setDocumentStateForTest(std::move(state));
        auto* view = window.glViewForTest();
        const bool noopMove = view && QMetaObject::invokeMethod(view, "thingLightTranslateRequested", Qt::DirectConnection,
            Q_ARG(std::int32_t, static_cast<std::int32_t>(render::LightGizmoKind::ThingHead)),
            Q_ARG(double, 0.0), Q_ARG(double, 0.0), Q_ARG(double, 0.0));
        bool noopScales = view != nullptr;
        for (const auto kind : {render::LightGizmoKind::ThingRegular, render::LightGizmoKind::ThingHead})
            noopScales = (view && QMetaObject::invokeMethod(view, "thingLightRangeScaleRequested", Qt::DirectConnection,
                Q_ARG(std::int32_t, static_cast<std::int32_t>(kind)), Q_ARG(double, 1.0))) && noopScales;
        const auto& unchanged = window.documentStateForTest().level.things[0].values;
        check(noopMove && noopScales && window.undoDepthForTest() == 0
                && unchanged[2].asString() == untouchedHead
                && unchanged[3].asString() == untouchedRegular
                && unchanged[4].asString() == untouchedOffset,
            "Zero Headlight Move and identity light scaling preserve raw tokens without history");
        const TVector delta{1e-8, -2e-8, 3e-8};
        TVector orientation{};
        orientation.yaw = 33.0;
        const TVector localDelta = TMatrix43::Create(TVector::zero, orientation).InvertOrtho().TransformVector(delta);
        const bool moved = view && QMetaObject::invokeMethod(view, "thingLightTranslateRequested", Qt::DirectConnection,
            Q_ARG(std::int32_t, static_cast<std::int32_t>(render::LightGizmoKind::ThingHead)),
            Q_ARG(double, delta.x), Q_ARG(double, delta.y), Q_ARG(double, delta.z));
        const auto& movedLevel = window.documentStateForTest().level;
        const auto* offsetValue = level::thingFindValue(movedLevel, nullptr, movedLevel.things[0], "lightoffset");
        const auto offset = writtenLightNumbers(offsetValue);
        check(moved && offset && offset->size() == 3
                && sameBits((*offset)[0], originalOffset.x + localDelta.x)
                && sameBits((*offset)[1], originalOffset.y + localDelta.y)
                && sameBits((*offset)[2], originalOffset.z + localDelta.z),
            "Headlight Move preserves authored offsets and rotated micron deltas in double precision");
        check(window.undoForTest(), "Exact Headlight Move creates an undo record");
        const double factor = std::nextafter(1.0, 2.0);
        for (const auto kind : {render::LightGizmoKind::ThingRegular, render::LightGizmoKind::ThingHead})
        {
            const bool scaled = view && QMetaObject::invokeMethod(view, "thingLightRangeScaleRequested", Qt::DirectConnection,
                Q_ARG(std::int32_t, static_cast<std::int32_t>(kind)), Q_ARG(double, factor));
            const auto& level = window.documentStateForTest().level;
            const auto* actual = sed::level::thingFindValue(level, nullptr, level.things[0],
                kind == render::LightGizmoKind::ThingHead ? "lightintensity" : "light");
            bool precise = false;
            if (actual && project == ProjectType::IJIM)
            {
                const auto tuple = writtenLightNumbers(actual);
                precise = tuple && tuple->size() == 4 && sameBits((*tuple)[0], red)
                    && sameBits((*tuple)[1], 0.234567890123456)
                    && sameBits((*tuple)[2], 0.123456789012345)
                    && sameBits((*tuple)[3], authoredRange * factor);
            }
            else if (actual)
            {
                const auto scalar = writtenLightNumbers(actual);
                precise = scalar && scalar->size() == 1 && sameBits(scalar->front(), authoredRange * factor);
            }
            check(scaled && precise, "Thing light range scale retains exact RGB and subdisplay scalar/range changes");
            check(window.undoForTest(), "Tiny authored light range change is undoable");
        }
    }

}
}
