#pragma once

#include "sed/assets/resource_locator.hpp"
#include "sed/level.hpp"

#include <cstddef>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <vector>

#include <QImage>

namespace sed::app_tests
{

struct TemplateArchiveRefreshTestResult
{
    bool ready{};
    bool outerWatcherReady{};
    bool nestedWatcherReady{};
    bool outerArchiveWasWatched{};
    bool nestedArchiveWasWatched{};
    bool outerMountedArchiveWatchingDisabled{};
    bool nestedMountedArchiveWatchingDisabled{};
    bool nestedRetryRetainedAfterFailure{};
    bool nestedRetryClearedAfterRepair{};
    bool outerRetryPendingAfterNestedEvent{};
    bool ownerWasAliveForArchiveSignal{};
    bool initialIconReady{};
    bool finalIconReady{};
    bool initialDrainCompleted{};
    bool finalDrainCompleted{};
    bool initialIconHasRedForeground{};
    bool finalIconHasGreenForeground{};
    bool listIconChanged{};
    bool sourcePayloadChanged{};
    bool oldPayloadRetainedAfterFailure{};
    bool oldPayloadAbsentAfterRepair{};
    bool replacementPayloadPublished{};
    int nestedArchiveSignalCount{};
    int nestedReloadAttemptCount{};
    int ownerInvalidationCountBefore{};
    int ownerInvalidationCountAfter{};
    int initialPendingRowCount{};
    int finalPendingRowCount{};
    int initialActiveJobCount{};
    int finalActiveJobCount{};
    int initialPreviewWorkerPoolActiveCount{};
    int finalPreviewWorkerPoolActiveCount{};
    std::size_t iconCacheSizeBefore{};
    std::size_t iconCacheSizeAfter{};
    std::vector<std::string> rowsBefore{};
    std::vector<std::string> rowsAfter{};
    std::optional<std::string> payloadBefore{};
    std::optional<std::string> payloadAfterFailure{};
    std::optional<std::string> payloadAfterRepair{};
    QImage iconBefore{};
    QImage iconAfter{};
};

} // namespace sed::app_tests

namespace sed
{

#ifdef SED_APP_MAIN_WINDOW_TEST_ACCESS
app_tests::TemplateArchiveRefreshTestResult
templateEditorNestedArchiveRefreshForTest(
    std::vector<ThingTemplate> templates,
    const assets::ResourceLocator& resources,
    std::string previewMasterCmp,
    const std::string& modelName,
    const std::filesystem::path& archivePath,
    const std::function<void()>& writeBrokenArchive,
    const std::function<void()>& writeReplacementArchive);
#endif

} // namespace sed
