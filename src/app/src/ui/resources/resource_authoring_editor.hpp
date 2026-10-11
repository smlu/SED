#pragma once

#include "sed/assets/resource_locator.hpp"
#include "sed/level.hpp"

#include <QDialog>
#include <QCborMap>

#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <string>

namespace sed::ui::resources
{

enum class ResourceAuthoringKind : std::uint8_t
{
    sprite,
    soundClass,
    stringTable,
};

struct ResourceAuthoringContext
{
    std::shared_ptr<const assets::ResourceLocator> resources{};
    ProjectType projectType{ ProjectType::JKDF2 };
    std::filesystem::path projectDirectory{};
    std::filesystem::path startingDirectory{};
    std::string masterCmp{};
    std::function<bool(QWidget*, const QString&)> activateExistingSource{};
    std::function<bool(QWidget*, const std::filesystem::path&)> destinationAvailable{};
    std::function<bool()> projectContextStillCurrent{};
    std::function<void(QWidget*, const QString&)> briefNotice{};
    std::function<bool(QWidget*, ResourceAuthoringKind)> createNewDraft{};
    std::function<bool(QWidget*, ResourceAuthoringKind)> openProjectResourceInNewTab{};
    bool transientProjectOpenTarget{};
    bool groupedWorkspaceChrome{};
    std::filesystem::path automaticSaveDirectory{};
};

using ResourceAuthoringChangedCallback = std::function<void(QWidget*)>;

#ifdef SED_APP_MAIN_WINDOW_TEST_ACCESS
struct ResourceAuthoringPaneTestCounters
{
    std::uint64_t captureCalls{};
    std::uint64_t rowVisits{};
};
using ResourceAuthoringPaneAfterReadHook =
    std::function<void(const std::filesystem::path&)>;
#endif

/** Creates one independent loose-file SPR, SND, or UNI authoring dialog. */
QDialog* createResourceAuthoringEditor(
    QWidget* parent,
    ResourceAuthoringKind kind,
    ResourceAuthoringContext context);

/** Creates the same isolated editor as an embeddable, non-modal workspace page. */
QWidget* createResourceAuthoringPane(
    QWidget* parent,
    ResourceAuthoringKind kind,
    ResourceAuthoringContext context,
    ResourceAuthoringChangedCallback changed = {});

/** Captures a bounded raw draft, including intermediate table-cell text. */
QCborMap captureResourceAuthoringPaneState(const QWidget* pane, bool includeHistory = true);
bool validateResourceAuthoringPaneState(const QCborMap& state, QString* warning = nullptr);
bool restoreResourceAuthoringPaneState(QWidget* pane, const QCborMap& state, QString* warning = nullptr);
ResourceAuthoringKind resourceAuthoringPaneKind(const QWidget* pane);
QString resourceAuthoringPaneIdentity(const QWidget* pane);
QString resourceAuthoringPaneTitle(const QWidget* pane);
QString resourceAuthoringPaneLooseFilePath(const QWidget* pane);
bool resourceAuthoringPaneTakeSuccessfulOpenNotification(QWidget* pane);
void resourceAuthoringPaneRebindProjectDirectory(
    QWidget* pane,
    const std::filesystem::path& projectDirectory);
bool resourceAuthoringPaneIsModified(const QWidget* pane);
bool resourceAuthoringPaneSave(QWidget* pane, bool saveAs);
bool resourceAuthoringPaneOpenLooseFile(QWidget* pane);
bool resourceAuthoringPaneOpenLooseFileDirect(QWidget* pane);
bool resourceAuthoringPaneOpenProjectResource(QWidget* pane);
bool resourceAuthoringPaneOpenProjectResourceDirect(QWidget* pane);
bool resourceAuthoringPaneConfirmClose(QWidget* pane, bool* discardedAny = nullptr);
void closeResourceAuthoringPaneAfterReplacement(QWidget* pane);
bool resourceAuthoringPaneOwnsFocus(const QWidget* pane);
bool resourceAuthoringPaneUndoRedo(QWidget* pane, bool redo);
bool resourceAuthoringPaneCanUndoRedo(const QWidget* pane, bool redo);
bool resourceAuthoringPaneDocumentUndoRedo(QWidget* pane, bool redo);
#ifdef SED_APP_MAIN_WINDOW_TEST_ACCESS
ResourceAuthoringPaneTestCounters resourceAuthoringPaneTestCounters(const QWidget* pane);
void resetResourceAuthoringPaneTestCounters(QWidget* pane);
void setResourceAuthoringPaneAfterReadHook(
    QWidget* pane,
    ResourceAuthoringPaneAfterReadHook hook);
bool resourceAuthoringPaneReloadCurrentLooseSourceForTest(QWidget* pane, QString* error);
#endif

} // namespace sed::ui::resources
