#include "test_common.hpp"
#include "resource_release_test_fixtures.hpp"
#include "main_window/templates/template_editor_test_seam.hpp"
#include "ui/components/preview_resource_disk_watcher.hpp"

#include <QByteArray>
#include <QTemporaryDir>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <utility>
#include <vector>

namespace sed::app_tests
{

namespace
{

QByteArray makeTemplateArchiveGob(
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

    appendU32(0x14u);
    appendU32(0x0cu);
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

bool writeTemplateArchive(
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

void checkTemplateArchiveRows(
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

} // namespace

void runTemplateArchiveRefreshTests()
{
    QTemporaryDir directory{};
    check(
        directory.isValid(),
        "Template Editor nested archive fixture creates a temporary directory");
    if ( !directory.isValid() )
    {
        return;
    }

    const std::filesystem::path archivePath =
        std::filesystem::path(directory.path().toStdWString())
        / "template-nested-refresh.gob";
    const std::string modelName = "nested_archive_refresh.3do";
    const std::string materialName = "nested_archive_refresh.mat";
    const std::string cmpName = "nested_archive_refresh.cmp";

    const QByteArray material = fixtures::makeIndexedMatFixture(6u);
    const QByteArray redPalette = fixtures::makeClassicCmpFixture(
        6u,
        QColor(255, 0, 0));
    const QByteArray greenPalette = fixtures::makeClassicCmpFixture(
        6u,
        QColor(0, 255, 0));
    const QByteArray oldModel = fixtures::makeDiskChangeModelFixture(
        QByteArray::fromStdString(materialName),
        QByteArrayLiteral("nested archive old payload"),
        2.0);
    QByteArray newModel = oldModel;
    newModel.replace(
        QByteArrayLiteral("nested archive old payload"),
        QByteArrayLiteral("nested archive new payload"));
    check(
        oldModel != newModel,
        "Template Editor nested archive fixture gives the model an exact replacement payload");
    check(
        oldModel.size() == newModel.size(),
        "Template Editor nested archive fixture keeps model payload lengths stable");

    const auto makeArchive = [&](const QByteArray& palette, const QByteArray& model)
    {
        return makeTemplateArchiveGob({
            { "3do/" + modelName, model },
            { materialName, material },
            { cmpName, palette } });
    };
    const QByteArray oldArchive = makeArchive(redPalette, oldModel);
    const QByteArray replacementArchive = makeArchive(greenPalette, newModel);
    QByteArray brokenArchive = oldArchive;
    brokenArchive[0] = 'X';
    check(
        oldArchive.size() == replacementArchive.size()
            && oldArchive.size() == brokenArchive.size(),
        "Template Editor nested archive fixture keeps archive replacement and failure sizes stable");

    const auto stamp = std::filesystem::file_time_type::clock::now();
    check(
        writeTemplateArchive(archivePath, oldArchive, stamp),
        "Template Editor nested archive fixture writes its initial archive");

    assets::ResourceLocator resources{};
    bool mounted = false;
    try
    {
        resources.mountGob(archivePath);
        mounted = true;
    }
    catch (const std::exception&)
    {
    }
    check(
        mounted,
        "Template Editor nested archive fixture mounts its initial archive");
    if ( !mounted )
    {
        return;
    }

    ThingTemplate templateValue{};
    templateValue.name = "nested_archive_refresh_template";
    templateValue.parent = "none";
    TplValue modelValue{};
    modelValue.name = "model3d";
    modelValue.type = ValueType::str;
    modelValue.stringValue = modelName;
    templateValue.values.push_back(modelValue);

    bool brokenWriteSucceeded = false;
    bool replacementWriteSucceeded = false;
    const TemplateArchiveRefreshTestResult result =
        templateEditorNestedArchiveRefreshForTest(
            { templateValue },
            resources,
            cmpName,
            modelName,
            archivePath,
            [&]()
            {
                brokenWriteSucceeded = writeTemplateArchive(
                    archivePath,
                    brokenArchive,
                    stamp);
            },
            [&]()
            {
                replacementWriteSucceeded = writeTemplateArchive(
                    archivePath,
                    replacementArchive,
                    stamp);
            });

    if ( result.ready && !result.initialDrainCompleted )
    {
        std::cerr << "Template Editor nested archive initial icon diagnostics: pending="
                  << result.initialPendingRowCount
                  << " activeJobs=" << result.initialActiveJobCount
                  << " poolActive="
                  << result.initialPreviewWorkerPoolActiveCount
                  << " cache=" << result.iconCacheSizeBefore
                  << " rows=" << result.rowsBefore.size() << '\n';
    }
    if ( result.ready && !result.finalDrainCompleted )
    {
        std::cerr << "Template Editor nested archive final icon diagnostics: pending="
                  << result.finalPendingRowCount
                  << " activeJobs=" << result.finalActiveJobCount
                  << " poolActive="
                  << result.finalPreviewWorkerPoolActiveCount
                  << " cache=" << result.iconCacheSizeAfter
                  << " rows=" << result.rowsAfter.size() << '\n';
    }

    check(
        result.ready && result.outerWatcherReady && result.nestedWatcherReady,
        "Template Editor nested archive fixture creates both owner and nested watchers");
    check(
        brokenWriteSucceeded && replacementWriteSucceeded,
        "Template Editor nested archive fixture writes the broken and repaired archives");
    check(
        result.outerArchiveWasWatched && result.nestedArchiveWasWatched,
        "Template Editor nested archive fixture resolves the mounted archive path in both watchers");
    check(
        result.outerMountedArchiveWatchingDisabled
            && result.nestedMountedArchiveWatchingDisabled,
        "Template Editor nested archive fixture disables only operating-system archive events before driving the nested seam");

    checkTemplateArchiveRows(
        result.rowsBefore,
        { templateValue.name },
        "Template Editor nested archive fixture initially exposes the exact owner list row");
    checkTemplateArchiveRows(
        result.rowsAfter,
        { templateValue.name },
        "Template Editor nested archive refresh retains the exact owner list row");
    check(
        result.payloadBefore == std::optional<std::string>(oldModel.toStdString()),
        "Template Editor nested archive fixture reads the exact initial model payload");
    check(
        result.payloadAfterFailure
            == std::optional<std::string>(oldModel.toStdString()),
        "Template Editor nested archive failure retains the old model snapshot");
    check(
        result.payloadAfterRepair
            == std::optional<std::string>(newModel.toStdString()),
        "Template Editor nested archive retry publishes the exact repaired model payload");
    check(
        result.sourcePayloadChanged
            && result.oldPayloadRetainedAfterFailure
            && result.oldPayloadAbsentAfterRepair
            && result.replacementPayloadPublished,
        "Template Editor nested archive retry distinguishes exact old and replacement payloads");

    check(
        result.initialDrainCompleted
            && result.initialIconReady
            && result.initialIconHasRedForeground,
        "Template Editor owner list warms a ready red model icon before nested archive notification");
    check(
        result.finalDrainCompleted
            && result.finalIconReady
            && result.finalIconHasGreenForeground,
        "Template Editor owner list publishes a ready green model icon after nested archive retry");
    check(
        result.listIconChanged,
        "Template Editor nested archive retry refreshes the owner list icon pixels");
    check(
        result.initialPendingRowCount == 0 && result.finalPendingRowCount == 0,
        "Template Editor nested archive retry drains all owner list icon work");
    check(
        result.initialActiveJobCount == 0
            && result.finalActiveJobCount == 0
            && result.initialPreviewWorkerPoolActiveCount == 0
            && result.finalPreviewWorkerPoolActiveCount == 0,
        "Template Editor nested archive retry leaves the preview worker pool idle at both observations");
    check(
        result.iconCacheSizeBefore == 1u && result.iconCacheSizeAfter == 1u,
        "Template Editor nested archive retry replaces the single owner icon cache entry");
    check(
        result.ownerInvalidationCountAfter
            == result.ownerInvalidationCountBefore + 1,
        "Template Editor nested archive success invalidates owner preview resources exactly once");

    check(
        result.nestedRetryRetainedAfterFailure
            && result.nestedRetryClearedAfterRepair
            && result.nestedReloadAttemptCount == 2,
        "Template Editor nested archive watcher retries the retained event without a second notification");
    check(
        result.nestedArchiveSignalCount == 1
            && result.ownerWasAliveForArchiveSignal,
        "Template Editor nested archive success emits one context-safe signal while its owner is alive");
    check(
        !result.outerRetryPendingAfterNestedEvent,
        "Template Editor nested archive test leaves the outer watcher without an archive retry");
}

} // namespace sed::app_tests
