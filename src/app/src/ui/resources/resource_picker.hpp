#pragma once

#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#ifdef SED_APP_MAIN_WINDOW_TEST_ACCESS
#include <QImage>
#endif

class QWidget;

namespace sed
{
enum class ProjectType : std::int32_t;
struct ThingTemplate;
}

namespace sed::assets
{
class ResourceLocator;
class Templates;
}

namespace sed::ui
{

enum class ResourcePickerKind : std::uint8_t
{
    cog,
    material,
    model3do,
    key,
    wav,
    ai,
    template_,
    message,
    cmp,
    snd,
    pup,
    spr,
    par,
    uni,
};

struct ResourcePickerOptions
{
    ResourcePickerKind kind{ ResourcePickerKind::material };
    std::string initialName{};
    std::optional<std::filesystem::path> preferredDiskRoot{};
    std::optional<std::filesystem::path> projectDirectory{};
    std::string title{};
    std::string previewCmp{};
    ProjectType previewProjectType;
    const sed::assets::Templates* templateDatabaseFallback{};
    const std::vector<ThingTemplate>* templateOverrides{};
    bool allowBaseTemplates{ true };
};

/** Returns the concise window title shared by resource picker entry points. */
std::string resourcePickerTitle(ResourcePickerKind kind);

std::optional<std::string> pickResource(
    QWidget* parent,
    const sed::assets::ResourceLocator& resources,
    const ResourcePickerOptions& options);

std::optional<std::string> pickResource(
    QWidget* parent,
    std::shared_ptr<const sed::assets::ResourceLocator> resources,
    const ResourcePickerOptions& options);

#ifdef SED_APP_MAIN_WINDOW_TEST_ACCESS
struct ResourcePickerNestedArchiveReloadTestResult
{
    bool ready{};
    int archiveSignalCount{};
    int archiveReloadAttemptCount{};
    int templatePreviewActiveJobs{};
    int previewWorkerActiveThreads{};
    bool archiveRetryPending{};
    bool previewSettledBefore{};
    bool previewSettledAfter{};
    std::vector<std::string> visibleRowsBefore{};
    std::vector<std::string> visibleRowsAfter{};
    std::optional<std::string> oldPayloadBefore{};
    std::optional<std::string> oldPayloadAfter{};
    std::optional<std::string> newPayloadAfter{};
    std::optional<std::string> previewCmpPayloadBefore{};
    std::optional<std::string> previewCmpPayloadAfter{};
    std::optional<std::string> previewMaterialPayloadBefore{};
    std::optional<std::string> previewMaterialPayloadAfter{};
    bool previewInteractiveBefore{};
    bool previewInteractiveAfter{};
    bool previewTexturesBefore{};
    bool previewTexturesAfter{};
    bool previewSolidBefore{};
    bool previewSolidAfter{};
    int previewRenderModeBefore{ -99 };
    int previewRenderModeAfter{ -99 };
    std::string previewSelectedNameBefore{};
    std::string previewSelectedNameAfter{};
    QImage previewBefore{};
    QImage previewAfter{};
};

struct ResourcePickerStorageMetadataTestStep
{
    int sourceKind{};
    int sourceIndex{ -1 };
    std::string resourceName{}; // Empty clears the current selection.
    std::function<void()> beforeSelect{};
    bool runPreviewUpdate{ true };
};

struct ResourcePickerStorageMetadataTestSnapshot
{
    bool sourceFound{};
    bool itemFound{};
    bool labelFound{};
    bool itemBold{};
    bool labelWordWrap{};
    int labelMaximumWidth{};
    std::string text{};
};

std::vector<ResourcePickerStorageMetadataTestSnapshot>
resourcePickerStorageMetadataTraceForTest(
    std::shared_ptr<const sed::assets::ResourceLocator> resources,
    const ResourcePickerOptions& options,
    const std::vector<ResourcePickerStorageMetadataTestStep>& steps);

bool resourcePickerArchiveReloadRetryForTest(
    std::shared_ptr<const sed::assets::ResourceLocator> resources,
    const ResourcePickerOptions& options,
    const std::function<void()>& corruptArchive,
    const std::function<void()>& repairArchive,
    const std::string& oldEntry,
    const std::string& newEntry,
    int& callbackAttempts,
    bool& oldEntryVisibleAfterFailure,
    bool& oldEntryVisibleAfterRetry,
    bool& newEntryVisibleAfterRetry,
    std::string& oldEntryPayloadAfterFailure,
    std::string& oldEntryPayloadAfterRetry,
    std::string& newEntryPayloadAfterRetry,
    bool& reloadFailureObserved);

ResourcePickerNestedArchiveReloadTestResult
resourcePickerNestedArchiveReloadForTest(
    std::shared_ptr<const sed::assets::ResourceLocator> resources,
    const ResourcePickerOptions& options,
    const std::function<void()>& replaceArchive,
    const std::string& oldEntry,
    const std::string& newEntry);
#endif

} // namespace sed::ui
