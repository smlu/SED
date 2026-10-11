#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include <QMainWindow>
#include <QCborMap>
#include <QColor>
#include <QIcon>
#include <QPointer>
#include <QSize>
#include <QStringList>
#include <QTimer>
#include <QVector3D>

#include "sed/assets/resource_locator.hpp"
#include "sed/assets/tpl.hpp"
#include "sed/editor/document.hpp"
#include "sed/level/build.hpp"
#include "sed/level/geometry.hpp"
#include "sed/level/lighting.hpp"
#include "sed/level/arch_lighting.hpp"
#include "sed/level/arch_lighting_clipboard.hpp"
#include "gl_view.hpp"
#include "render/grid/viewport_grid_utils.hpp"
#include "thing/thing_resource_utils.hpp"
#include "thing/thing_snap_utils.hpp"
#include "ui/shortcuts/selection_modifier_utils.hpp"
#include "ui/components/preview_thumbnail_list.hpp"
#include "ui/layers/layer_color_utils.hpp"
#include "ui/recovery/document_recovery_utils.hpp"

class QAction;
class QActionGroup;
class QAbstractSpinBox;
class QCheckBox;
class QCloseEvent;
class QColorDialog;
class QComboBox;
class QDialog;
class QDockWidget;
class QDropEvent;
class QDoubleSpinBox;
class QEvent;
class QFileSystemWatcher;
class QFont;
template <typename T>
class QFutureWatcher;
class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QImage;
class QLineEdit;
class QListWidget;
class QMenu;
class QPoint;
class QProgressBar;
class QPushButton;
class QScrollArea;
class QSpinBox;
class QTabBar;
class QTabWidget;
class QString;
class QSlider;
class QShowEvent;
class QSplitter;
class QToolButton;
class QThreadPool;
class QTreeWidget;
class QTreeWidgetItem;
class QVariantAnimation;
class QVector3D;
class QWidget;
class QRubberBand;

namespace sed
{

enum class OutlinerKind : int;
namespace ui::viewport
{
enum class PanelDockSide : std::uint8_t;
class CleaveShapePopover;
}
namespace ui
{
class ScrubFloatEdit;
enum class ResourcePickerKind : std::uint8_t;
}
namespace ui::shortcuts
{
enum class FlyNavigationShortcut;
}
namespace ui::resources
{
enum class ResourceAuthoringKind : std::uint8_t;
struct ResourceAuthoringContext;
}
namespace ui::shortcuts
{
enum class SelectionHintMode;
}
namespace ui::checks
{
enum class CheckRuleGroup;
}
namespace ui::detail
{
class GlViewModelThumbnailRenderer;
class PreviewResourceDiskWatcher;
}

class MainWindow final : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(
        QWidget* parent = nullptr,
        bool deferWindowPlacementRestore = false);
    ~MainWindow() override;
    bool runInitialStartupFlow();
    // Called synchronously from QGuiApplication::commitDataRequest during an
    // operating-system session shutdown.  The published recovery is retained
    // even if Qt subsequently delivers a normal close event.
    void prepareForSystemShutdown();
#ifdef SED_APP_MAIN_WINDOW_TEST_ACCESS
    GlView* glViewForTest() const noexcept
    {
        return glView_;
    }
    void syncSelectionModifierBindingsForTest()
    {
        syncSelectionModifierBindings();
    }
    int thingPlacementModeForModifiersForTest(
        const Qt::KeyboardModifiers modifiers) const noexcept
    {
        return static_cast<int>(thingPlacementModeForModifiers(modifiers));
    }
    QString selectionModifierHintForTest(
        const ui::shortcuts::SelectionHintMode mode,
        const Qt::KeyboardModifiers held) const
    {
        return ui::shortcuts::selectionModifierHintText(
            mode,
            held,
            ui::shortcuts::SelectionModifierHintBindingsConfig{
                .addBindings = selectionAddModifierBindings_,
                .removeBindings = selectionRemoveModifierBindings_,
                .shortestPathBindings = selectionShortestPathModifierBindings_,
                .bypassFiltersBindings = selectionBypassSurfaceFiltersModifierBindings_,
            });
    }
    const ui::shortcuts::ModifierBindings& selectionAddModifierBindingsForTest() const noexcept
    {
        return selectionAddModifierBindings_;
    }
    const ui::shortcuts::ModifierBindings& selectionRemoveModifierBindingsForTest() const noexcept
    {
        return selectionRemoveModifierBindings_;
    }
    const ui::shortcuts::ModifierBindings& selectionShortestPathModifierBindingsForTest() const noexcept
    {
        return selectionShortestPathModifierBindings_;
    }
    const ui::shortcuts::ModifierBindings& selectionBypassSurfaceFiltersModifierBindingsForTest() const noexcept
    {
        return selectionBypassSurfaceFiltersModifierBindings_;
    }
    const ui::shortcuts::ModifierBindings& thingPlacementTiltModifierBindingsForTest() const noexcept
    {
        return thingPlacementTiltModifierBindings_;
    }
    const ui::shortcuts::ModifierBindings& thingPlacementFreeModifierBindingsForTest() const noexcept
    {
        return thingPlacementFreeModifierBindings_;
    }
    const ui::shortcuts::ModifierBindings& transformGridSnapInvertModifierBindingsForTest() const noexcept
    {
        return transformGridSnapInvertModifierBindings_;
    }
    const ui::shortcuts::ModifierBindings& transformGridStickInvertModifierBindingsForTest() const noexcept
    {
        return transformGridStickInvertModifierBindings_;
    }
    const ui::shortcuts::ModifierBindings& sectorMoveSnapAlignInvertModifierBindingsForTest() const noexcept
    {
        return sectorMoveSnapAlignInvertModifierBindings_;
    }
    const ui::shortcuts::ModifierBindings& surfaceGeometrySnapInvertModifierBindingsForTest() const noexcept
    {
        return surfaceGeometrySnapInvertModifierBindings_;
    }
    const ui::shortcuts::ModifierBindings& thingMoveSurfaceStickInvertModifierBindingsForTest() const noexcept
    {
        return thingMoveSurfaceStickInvertModifierBindings_;
    }
    const ui::shortcuts::ModifierBindings& thingMoveSurfaceAlignInvertModifierBindingsForTest() const noexcept
    {
        return thingMoveSurfaceAlignInvertModifierBindings_;
    }
    const ui::shortcuts::ModifierBindings& thingMoveSnapInvertModifierBindingsForTest() const noexcept
    {
        return thingMoveSnapInvertModifierBindings_;
    }
    const ui::shortcuts::ModifierBindings& thingMoveSnapRotateInvertModifierBindingsForTest() const noexcept
    {
        return thingMoveSnapRotateInvertModifierBindings_;
    }
    /**
     * Test-only wrapper for exercising the real protected event filter.
     */
    bool eventFilterForTest(QObject* watched, QEvent* event)
    {
        return eventFilter(watched, event);
    }
    void updateSelectionModifierHintForTest(
        const int key,
        const Qt::KeyboardModifiers modifiers,
        const bool pressed,
        const bool viewportFocusContext,
        const bool viewportPointerContext)
    {
        updateSelectionModifierHintForKeyState(
            key,
            modifiers,
            pressed,
            viewportFocusContext,
            viewportPointerContext);
    }
    void scheduleEditorViewRefitAfterLayoutForTest();
    bool editorViewRefitViewportReadyForTest() const noexcept;
    bool pendingEditorViewRefitForTest() const noexcept;
    bool editorViewRefitTimerActiveForTest() const noexcept;
    void setCurrentPathForTitleForTest(const std::filesystem::path& path)
    {
        currentPath_ = path;
        refreshWindowTitle();
    }
    const std::filesystem::path& currentPathForTest() const noexcept
    {
        return currentPath_;
    }
    void setLastEngineExportPathForTest(const std::filesystem::path& path)
    {
        lastEngineExportPath_ = path;
    }
    const std::filesystem::path& lastEngineExportPathForTest() const noexcept
    {
        return lastEngineExportPath_;
    }
    void markCurrentDocumentAsSavedForTest()
    {
        markCurrentDocumentAsSaved();
        markRecoveryWorkspaceAsSaved();
    }
    void openLevelFromFilenameForTest(const QString& filename)
    {
        openLevelFromFilename(filename);
    }
    void openLevelFromFilenameForTest(
        const QString& filename,
        std::function<void()> beforeReplacement)
    {
        openLevelFromFilename(filename, std::move(beforeReplacement));
    }
    bool dispatchEditorShortcutForTest(
        const int key,
        const Qt::KeyboardModifiers modifiers)
    {
        return dispatchEditorShortcut(key, modifiers);
    }
    ProjectType resourceKindForTest() const noexcept
    {
        return resourceKind_;
    }
    void setResourceKindForTest(const ProjectType kind) noexcept
    {
        resourceKind_ = kind;
    }
    const std::vector<std::filesystem::path>& configuredResourceRootsForTest()
        const noexcept
    {
        return resourceRoots_;
    }
    const std::vector<std::filesystem::path>& configuredResourceGobsForTest()
        const noexcept
    {
        return resourceGobs_;
    }
    const std::vector<std::filesystem::path>& locatorResourceRootsForTest()
        const noexcept
    {
        return resources_.roots();
    }
    std::vector<std::filesystem::path> locatorResourceGobsForTest() const
    {
        std::vector<std::filesystem::path> paths{};
        paths.reserve(resources_.gobs().size());
        for (const assets::GobArchive& gob : resources_.gobs())
        {
            paths.push_back(gob.path());
        }
        return paths;
    }
    const assets::ResourceLocator& resourceLocatorForTest() const noexcept
    {
        return resources_;
    }
    bool resourceArchiveReloadWarningShownForTest() const noexcept
    {
        return resourceArchiveReloadWarningShown_;
    }
    void setResourcePathsForTest(
        std::vector<std::filesystem::path> roots,
        std::vector<std::filesystem::path> gobs,
        const std::filesystem::path& sourcePath = {})
    {
        resourceRoots_ = std::move(roots);
        resourceGobs_ = std::move(gobs);
        rebuildResourceLocator(sourcePath);
    }
    void setDirectSedSalvageBeforeRereadForTest(
        std::function<void()> hook)
    {
        directSedSalvageBeforeRereadForTest_ = std::move(hook);
    }
    /**
     * Invoked on the load worker after a native SED payload has been decoded
     * but before its post-read size/mtime revision check.  The seam models a
     * concurrent delete/replace/stat failure without allowing a test to
     * mutate the GUI-owned document from the worker.
     */
    void setDirectSedAfterLoadForTest(std::function<void()> hook)
    {
        directSedAfterLoadForTest_ = std::move(hook);
    }
    void setDirectOpenBeforeViewportRefitForTest(std::function<void()> hook)
    {
        directOpenBeforeViewportRefitForTest_ = std::move(hook);
    }
    void setNativeSaveAfterCaptureForTest(std::function<void()> hook)
    {
        nativeSaveAfterCaptureForTest_ = std::move(hook);
    }
    void setEngineExportAfterCaptureForTest(std::function<void()> hook)
    {
        engineExportAfterCaptureForTest_ = std::move(hook);
    }
    void saveWindowPlacementForTest() const
    {
        saveWindowPlacement();
    }
    void pushUndoForTitleForTest(
        const editor::DocumentDirty dirty = editor::DocumentDirty::LevelGeometry)
    {
        document_.pushUndo("Test unsaved edit", dirty);
        refreshWindowTitle();
    }
    bool applyStructuralHistoryEditForTest(
        std::function<bool(editor::DocumentState&)> mutate)
    {
        const bool applied = document_.applyUndoable(
            "Test structural edit",
            editor::DocumentDirty::LevelGeometry
                | editor::DocumentDirty::Structure,
            std::move(mutate));
        refreshWindowTitle();
        return applied;
    }
    void pushThingSubsetUndoForTitleForTest(
        const std::vector<std::int32_t>& thingIndices)
    {
        document_.pushThingSubsetUndo(
            "Test Thing subset edit", editor::DocumentDirty::Things, thingIndices);
        refreshWindowTitle();
    }
    void pushSectorSubsetUndoForTitleForTest(
        const std::vector<std::int32_t>& sectorIndices)
    {
        document_.pushSectorSubsetUndo(
            "Test Sector subset edit", editor::DocumentDirty::LevelGeometry,
            sectorIndices);
        refreshWindowTitle();
    }
    void setRecoveryDirectoryForTest(const std::filesystem::path& directory);
    void beginRecoveryDocumentForTest(
        const std::filesystem::path& sourcePath,
        bool requiresSave);
    void triggerRecoverySnapshotForTest();
    void triggerForcedRecoverySnapshotForTest();
    void flushRecoverySnapshotForTest(const bool forceFinalCapture = true)
    {
        recoveryAutomaticBaselineEnabled_ = true;
        flushRecoverySnapshot(forceFinalCapture);
    }
    void triggerRecoveryRetryForTest();
    void triggerPeriodicRecoverySnapshotForTest()
    {
        recoveryAutomaticBaselineEnabled_ = true;
        handleRecoveryTimerTimeout();
    }
    void waitForRecoverySnapshotForTest();
    void waitForRecoverySalvageForTest();
    void completeRecoveryAfterNormalSaveForTest(const std::filesystem::path& path)
    {
        recoveryAutomaticBaselineEnabled_ = true;
        completeRecoveryAfterNormalSave(path);
    }
    bool recoveryRetryTimerActiveForTest() const noexcept;
    bool recoveryDebounceTimerActiveForTest() const noexcept
    {
        return recoveryDebounceTimer_ && recoveryDebounceTimer_->isActive();
    }
    bool recoveryAuthoredCaptureDeferredForFlyTest() const noexcept
    {
        return recoveryAuthoredCaptureDeferredForFly_;
    }
    bool currentRecoveryPublicationCompleteForTest() const noexcept
    {
        return recoveryLastPublishedValid_
            && ui::recovery::recoveryPathsEqual(
                recoveryLastPublishedPath_, currentRecoveryPath_)
            && recoveryPendingRetirementPath_.empty();
    }
    std::filesystem::path currentRecoveryPathForTest() const;
    bool applyRecoveryFileForTest(const std::filesystem::path& recoveryPath);
    void captureReplacementAuthorizationFingerprintForTest()
    {
        captureReplacementAuthorizationFingerprint();
    }
    bool replacementAuthorizationStillCurrentForTest() const
    {
        return replacementAuthorizationStillCurrent();
    }
    void pauseRecoveryPublicationForTest(bool paused)
    {
        recoveryPublicationPaused_ = paused;
        if (paused)
        {
            if (recoveryDebounceTimer_) recoveryDebounceTimer_->stop();
            if (recoveryRetryTimer_) recoveryRetryTimer_->stop();
            if (recoveryPublicationGate_)
            {
                recoveryPublicationGate_->generation.fetch_add(
                    1, std::memory_order_acq_rel);
            }
        }
    }
    void setRecoveryApplyBeforeInstallForTest(std::function<bool()> hook)
    {
        recoveryApplyBeforeInstallForTest_ = std::move(hook);
    }
    void setRecoveryArchivePublicationAllowedForTest(
        std::function<bool()> hook)
    {
        recoveryArchivePublicationAllowedForTest_ = std::move(hook);
    }
    void setRecoveryLiveCommitAllowedForTest(std::function<bool()> hook)
    {
        recoveryLiveCommitAllowedForTest_ = std::move(hook);
    }
    void setRecoveryRetainCorruptedCopyForTest(std::function<bool()> hook)
    {
        recoveryRetainCorruptedCopyForTest_ = std::move(hook);
    }
    bool offerStartupRecoveryForTest()
    {
        recoveryAutomaticBaselineEnabled_ = true;
        return offerStartupRecovery();
    }
    bool startupRecoveryEditorStateRestoredForTest() const noexcept
    {
        return startupRecoveryEditorStateRestored_;
    }
    bool authorizeDocumentReplacementForTest(
        const QString& actionDescription,
        const std::filesystem::path& preservedRecoveryPath,
        QString* deferredDiscardIdentity,
        QString* deferredDiscardSessionId = nullptr)
    {
        return authorizeDocumentReplacement(
            actionDescription,
            preservedRecoveryPath,
            deferredDiscardIdentity,
            deferredDiscardSessionId);
    }
    int offerRecoveryForPathForTest(
        const std::filesystem::path& sourcePath,
        std::filesystem::path* selectedRecoveryPath = nullptr,
        QString* selectedRecoveryIdentity = nullptr,
        QString* selectedRecoverySessionId = nullptr)
    {
        return static_cast<int>(offerRecoveryForPath(
            sourcePath,
            false,
            selectedRecoveryPath,
            selectedRecoveryIdentity,
            selectedRecoverySessionId));
    }
    int recoveredRecoveryOfferValueForTest() const noexcept
    {
        return static_cast<int>(RecoveryOfferResult::recovered);
    }
    int noRecoveryOfferValueForTest() const noexcept
    {
        return static_cast<int>(RecoveryOfferResult::none);
    }
    QByteArray captureRecoveryEditorStateForTest() const
    {
        return captureRecoveryEditorState();
    }
    QString recoveryEditorStateCaptureErrorForTest() const
    {
        return recoveryEditorStateCaptureError_;
    }
    std::uint64_t recoveryEditorStateCaptureCountForTest() const noexcept
    {
        return recoveryEditorStateCaptureCountForTest_;
    }
    void resetRecoveryEditorStateCaptureCountForTest() const noexcept
    {
        recoveryEditorStateCaptureCountForTest_ = 0;
    }
    std::vector<QPointF> uvEditorStoredPointsForTest(SurfaceRef ref) const;
    QByteArray captureUvEditorWorkspaceStateForTest() const
    {
        return captureUvEditorWorkspaceState();
    }
    void applyUvEditorMaximumZoomForTest()
    {
        applyUvEditorMaximumZoomForPreferences();
    }
    bool recoveryWorkspaceHasSavedBaselineForTest() const
    {
        return !recoverySavedLevelStateHash_.isEmpty()
            && !recoverySavedCogStateHash_.isEmpty()
            && !recoverySavedResourceEditorStateHash_.isEmpty();
    }
    std::int64_t nativeDocumentTimestampForTest() const noexcept
    {
        return nativeDocumentTimestamp_;
    }
    void setNativeDocumentTimestampForTest(
        const std::int64_t timestamp) noexcept
    {
        nativeDocumentTimestamp_ = timestamp;
    }
    bool restoreRecoveryEditorStateForTest(
        const QByteArray& payload,
        QString* warning = nullptr)
    {
        return restoreRecoveryEditorState(payload, warning);
    }
    bool validateRecoveryEditorStateForTest(
        const QByteArray& payload,
        QString* warning = nullptr)
    {
        return validateRecoveryEditorState(payload, warning);
    }
    void resetSelectionContextStateForTest()
    {
        resetSelectionContextState();
    }
    bool restoreCogEditorRecoveryStateForTest(const QByteArray& payload)
    {
        return restoreCogEditorRecoveryState(payload);
    }
    QByteArray captureCogEditorRecoveryStateForTest() const
    {
        return captureCogEditorRecoveryState();
    }
    bool hasUnsavedChangesForTest() const noexcept
    {
        return hasUnsavedChanges();
    }
    bool hasRecoveryOfferEligibleChangesForTest() const
    {
        return hasRecoveryOfferEligibleChanges();
    }
    bool hasUnsavedAuthoredLevelChangesForTest() const
    {
        return hasUnsavedAuthoredLevelChanges();
    }
    bool hasUnsavedCogScriptChangesForTest() const
    {
        return hasUnsavedCogScriptChanges();
    }
    bool saveCogScriptEditorForTest(int index);
    QString normalizedCogScriptNameForTest(const QString& typedName) const;
    QString cogScriptNameValidationErrorForTest(const QString& typedName) const;
    std::optional<std::int32_t> createPlacedCogFromScriptForTest(
        std::string_view scriptName,
        bool selectNewCog)
    {
        return createPlacedCogFromScript(std::string(scriptName), selectNewCog);
    }
    void syncCogScriptWatchesForTest()
    {
        syncCogScriptWatches();
    }
    void notifyCogScriptFileChangedForTest(const QString& path)
    {
        handleCogScriptFileChanged(path);
    }
    void notifyCogScriptDirectoryChangedForTest(const QString& path)
    {
        handleCogScriptDirectoryChanged(path);
    }
    // Counts the real CogScriptEditorDialog save callback without replacing
    // the production save path.  This lets interaction tests prove that the
    // single Ctrl+S accelerator cannot invoke both COG and level saves.
    void setCogSaveCallbackForTest(std::function<void()> callback)
    {
        cogSaveCallbackForTest_ = std::move(callback);
    }
    void setCogDiscardCallbackForTest(std::function<void()> callback)
    {
        cogDiscardCallbackForTest_ = std::move(callback);
    }
    void createNewCogScriptForTest(const QString& name)
    {
        createNewCogScriptNamed(name);
    }
    void openCogScriptEditorForTest(const std::string_view name)
    {
        openCogScriptEditor(name);
    }
    QStringList cogScriptTabContextActionTextsForTest(int index);
    bool cogScriptTabContextActionEnabledForTest(
        int index,
        const QString& actionText);
    bool triggerCogScriptTabContextActionForTest(
        int index,
        const QString& actionText);
    void createNewProjectForTest(const ProjectType kind)
    {
        createNewProject(kind);
    }
    void evaluateLegacyUvCompatibilityForTest(
        const bool allowPromotion,
        const bool resourceAvailabilityChanged)
    {
        evaluateLegacyUvCompatibility(
            allowPromotion, resourceAvailabilityChanged);
    }
    QString legacyUvCompatibilityStatusForTest() const;
    std::size_t legacyUvPromotionEvaluationCountForTest() const noexcept
    {
        return legacyUvPromotionEvaluationCount_;
    }
    bool legacyUvPromotionEvaluationInFlightForTest() const noexcept
    {
        return legacyUvPromotionEvaluationInFlight_;
    }
    bool legacyUvPromotionIdleForTest() const noexcept
    {
        return !legacyUvPromotionRetryScheduled_
            && !legacyUvPromotionEvaluationInFlight_
            && !legacyUvPromotionRetryPending_;
    }
    editor::DocumentUndoSnapshotStats undoSnapshotStatsForTest() const noexcept
    {
        return document_.undoSnapshotStats();
    }
    void notifyLegacyUvResourcesChangedForTest()
    {
        ++legacyUvResourceGeneration_;
        scheduleLegacyUvPromotionRetry();
    }
    void setLegacyUvPromotionPreparedHookForTest(
        std::function<void()> hook)
    {
        legacyUvPromotionPreparedHookForTest_ = std::move(hook);
    }
    void noteLegacyUvSemanticMutationForTest()
    {
        recoveryAutomaticBaselineEnabled_ = true;
        noteRecoveryStateChanged(true);
    }
    void discardCurrentRecoveryForTest();
    bool removeRecoveryPathSafelyForTest(
        const std::filesystem::path& path,
        const ui::recovery::RecoveryMetadata& expectedMetadata,
        QString* error = nullptr)
    {
        return removeRecoveryPathSafely(path, error, &expectedMetadata);
    }
    bool recoverySnapshotRequiredForTest() const noexcept
    {
        return recoverySnapshotRequired_;
    }
    void discardRecoveryFamilyForTest(
        const QString& identity,
        const QString& sessionId,
        const std::filesystem::path& preservedPath = {})
    {
        discardRecoveryFamily(identity, sessionId, preservedPath);
    }
    void discardCurrentRecoveryFamilyForTest(
        const QString& identity,
        const std::filesystem::path& preservedPath = {})
    {
        discardRecoveryFamily(identity, recoverySessionId_, preservedPath);
    }
    void setDocumentStateForTest(editor::DocumentState state)
    {
        if (thingPlacementBrushStrokeActive_)
        {
            (void)finalizeThingPlacementBrushStroke();
        }
        ++resourceEditorDocumentContextGeneration_;
        for ( CogScriptEditorTabHost& host : cogScriptEditorTabHosts_ )
        {
            if ( host.primary && host.tabs && host.tabs->count() == 0 )
            {
                host.sessionGeneration = resourceEditorDocumentContextGeneration_;
            }
        }
        clearFramePlacementPreview();
        document_.clear(std::move(state));
        resetLayerStatesFromLevel();
        invalidateThingPlacementSnapCaches();
        rebuildProperties();
    }
    void refreshThingChecksForTest(
        const std::vector<std::int32_t>& thingIndices,
        const bool refreshResources)
    {
        refreshThingChecks(thingIndices, refreshResources);
    }
    void rebuildChecksForTest()
    {
        rebuildChecks();
    }
    [[nodiscard]] std::uint64_t thingCheckRowsVisitedForTest() const noexcept
    {
        return thingCheckRowsVisited_;
    }
    [[nodiscard]] std::size_t indexedThingCheckRowCountForTest() const noexcept
    {
        std::size_t count = 0;
        for (const auto& [thingIndex, rows] : consistencyThingCheckRows_)
        {
            (void)thingIndex;
            count += rows.size();
        }
        for (const auto& [thingIndex, rows] : resourceThingCheckRows_)
        {
            (void)thingIndex;
            count += rows.size();
        }
        return count;
    }
    void resetLayerColorsToDefaultsForTest()
    {
        resetLayerColorsToDefaults();
        syncLayerViewState();
    }
    void activateThingEditModeForTest()
    {
        setEditMode(EditMode::thing);
    }
    void activateSectorEditModeForTest()
    {
        setEditMode(EditMode::sector);
    }
    void deactivateEditModeForTest()
    {
        setEditMode(EditMode::none);
    }
    int editModeForTest() const noexcept
    {
        return static_cast<int>(editMode_);
    }
    bool cogEditorFocusActiveForTest() const noexcept
    {
        return cogEditorFocusActive_;
    }
    int sectorEditModeValueForTest() const noexcept
    {
        return static_cast<int>(EditMode::sector);
    }
    int surfaceEditModeValueForTest() const noexcept
    {
        return static_cast<int>(EditMode::surface);
    }
    int thingEditModeValueForTest() const noexcept
    {
        return static_cast<int>(EditMode::thing);
    }
    int frameEditModeValueForTest() const noexcept
    {
        return static_cast<int>(EditMode::frame);
    }
    void activateSurfaceEditModeForTest()
    {
        setEditMode(EditMode::surface);
    }
    void activateSurfaceEdgeSubModeForTest()
    {
        setSurfaceSelectionMode(SurfaceSelectionMode::edge);
    }
    void activateSurfaceFaceSubModeForTest()
    {
        setSurfaceSelectionMode(SurfaceSelectionMode::face);
    }
    void activateSurfaceVertexSubModeForTest()
    {
        setSurfaceSelectionMode(SurfaceSelectionMode::vertex);
    }
    bool surfaceCleaveActionEnabledForTest() const noexcept
    {
        return viewportSurfaceCleaveAction_
            && viewportSurfaceCleaveAction_->isEnabled();
    }
    bool surfaceExtrudeActionEnabledForTest() const noexcept
    {
        return extrudeSurfaceAction_ && extrudeSurfaceAction_->isEnabled();
    }
    int surfaceSelectionModeForTest() const noexcept
    {
        return static_cast<int>(surfaceSelectionMode_);
    }
    int surfaceFaceModeValueForTest() const noexcept
    {
        return static_cast<int>(SurfaceSelectionMode::face);
    }
    void activateViewportMoveToolForTest()
    {
        if (viewportTransformMoveAction_)
        {
            viewportTransformMoveAction_->trigger();
        }
    }
    bool viewportMoveToolActiveForTest() const noexcept
    {
        return viewportTransformMoveAction_
            && viewportTransformMoveAction_->isChecked();
    }
    void activateViewportCleaveToolForTest()
    {
        if (viewportSectorCleaveAction_)
        {
            viewportSectorCleaveAction_->trigger();
        }
    }
    bool viewportCleaveToolActiveForTest() const noexcept
    {
        return viewportSectorCleaveAction_
            && viewportSectorCleaveAction_->isChecked();
    }
    void activateFrameEditModeForTest()
    {
        setEditMode(EditMode::frame);
    }
    void activateThingLightEditModeForTest()
    {
        setLightEditSubMode(LightEditSubMode::thing);
        setEditMode(EditMode::light);
    }
    void activateBakedLightEditModeForTest()
    {
        setLightEditSubMode(LightEditSubMode::baked);
        setEditMode(EditMode::light);
    }
    void activateArchitecturalLightingEditModeForTest();
    bool architecturalLightingEditModeActiveForTest() const noexcept;
    bool refreshArchitecturalLightingOwnerForTest(std::int32_t thingIndex)
    {
        return refreshArchitecturalLightingInteractiveOwner(thingIndex);
    }
    bool stageArchitecturalLightingVertexForTest(
        std::int32_t thingIndex,
        std::int32_t lodIndex,
        std::int32_t meshIndex,
        std::int32_t vertexIndex,
        const MotsArchLightVertex& value,
        level::MotsArchLightingVertexMode mode)
    {
        return stageArchitecturalLightingVertex(
            ArchitecturalLightingVertexSelection{
                thingIndex, lodIndex, meshIndex, vertexIndex },
            value,
            mode);
    }
    bool prepareArchitecturalLightingVertexSelectionForTest(
        const std::vector<std::array<std::int32_t, 4>>& selections)
    {
        std::vector<ArchitecturalLightingVertexSelection> converted;
        converted.reserve(selections.size());
        for (const auto& selection : selections)
        {
            converted.push_back(ArchitecturalLightingVertexSelection{
                selection[0], selection[1], selection[2], selection[3] });
        }
        return prepareArchitecturalLightingInteractiveSelection(converted);
    }
    std::size_t architecturalLightingSelectedVertexCountForTest() const noexcept
    {
        return architecturalLightingInteractive_.selectedVertices.size();
    }
    bool stageArchitecturalLightingSelectedVerticesForTest(
        const MotsArchLightVertex& value,
        level::MotsArchLightingVertexMode mode)
    {
        return stageArchitecturalLightingSelectedVertices(
            value, mode, std::array<bool, 4>{ true, true, true, true });
    }
    bool commitArchitecturalLightingVertexForTest()
    {
        return commitArchitecturalLightingInteractiveEdit();
    }
    void cancelArchitecturalLightingVertexForTest() noexcept
    {
        cancelArchitecturalLightingInteractiveEdit();
    }
    void openArchitecturalLightingVertexEditorForTest(
        std::int32_t thingIndex,
        std::int32_t lodIndex,
        std::int32_t meshIndex,
        std::int32_t vertexIndex);
    void openArchitecturalLightingVertexEditorsForTest(
        const std::vector<std::array<std::int32_t, 4>>& selections);
    QDialog* architecturalLightingVertexDialogForTest() const noexcept;
    bool initializeArchitecturalLightingForTest()
    {
        return initializeArchitecturalLightingForSelection();
    }
    bool resetArchitecturalLightingForTest()
    {
        return resetArchitecturalLightingForSelection();
    }
    bool copyArchitecturalLightingClipboardForTest();
    bool pasteArchitecturalLightingClipboardForTest();
    std::uint64_t uvEditorPanelTransitionSerialForTest() const noexcept
    {
        return uvEditorPanelTransitionSerial_;
    }
    void setProjectTypeForTest(const ProjectType kind)
    {
        document_.level().kind = kind;
        document_.level().header.version = kind == ProjectType::IJIM ? 3 : 1;
        setEditMode(editMode_);
    }
    QDialog* createTemplateEditorDialogForTest(std::vector<ThingTemplate> templates);
    QDialog* createTemplateEditorDialogForTest(
        std::vector<ThingTemplate> templates,
        ProjectType projectType,
        std::string previewMasterCmp,
        const assets::ResourceLocator& previewResources,
        float motsTreeCollisionPreviewMeshRadiusFactor = 0.75f);
    const editor::DocumentState& documentStateForTest() const noexcept
    {
        return document_.current();
    }
    editor::DocumentState& mutableDocumentStateForTest() noexcept
    {
        // Tests intentionally bypass the normal undo/mutation helpers to
        // exercise same-undo recovery captures. Let the recovery scheduler
        // observe that direct mutable seam without hashing the whole Level.
        recoveryTestMutationPending_ = true;
        return document_.current();
    }
    void resetLightBakeSourceTrackingForTest()
    {
        resetLightBakeSourceTracking();
    }
    void pollLightBakeSourceChangesForTest()
    {
        pollLightBakeSourceChanges();
    }
    void startPendingAutomaticLightBakeForTest();
    void requestSelectedLightBakeForTest()
    {
        requestLightBake(false, true);
    }
    bool lightBakeRunningForTest() const noexcept
    {
        return lightBakeRunning_;
    }
    const std::vector<level::LightingInfluenceSphere>&
    activeLightBakeInfluencesForTest() const noexcept
    {
        return lightBakeActiveInfluences_;
    }
    void stageLightBakeTransformPreviewForTest(
        Level baseline,
        std::vector<std::int32_t> targets)
    {
        lightBakeTransformPreviewBaseline_ = std::make_shared<Level>(std::move(baseline));
        lightBakeTransformPreviewTargets_ = std::move(targets);
        lightBakeTransformPreviewUpdatedAmbient_ = true;
    }
    void prepareLightBakeTransformPreviewCommitForTest()
    {
        prepareLightBakeTransformPreviewCommit();
    }
    void finishLightBakeTransformPreviewForTest()
    {
        restoreLightBakeTransformPreview();
    }
    bool lightBakeTransformCommitPreviewPendingForTest() const noexcept
    {
        return lightBakeTransformCommittedPreview_.has_value();
    }
    bool lightBakeDebounceActiveForTest() const noexcept;
    std::size_t pendingAutomaticLightBakeTargetCountForTest() const noexcept
    {
        return lightBakePendingAutoTargets_.size();
    }
    std::size_t pendingAutomaticLightBakeInfluenceCountForTest() const noexcept
    {
        return lightBakePendingAutoInfluences_.size();
    }
    void stageLightBakeRunningForTest(const bool running) noexcept
    {
        lightBakeRunning_ = running;
    }
    void queueManualLightBakeForTest(
        std::vector<std::int32_t> targets,
        std::string undoLabel,
        const bool updateAmbient)
    {
        startLightBake(
            std::move(targets),
            false,
            {},
            {},
            std::move(undoLabel),
            updateAmbient);
    }
    const std::vector<std::int32_t>&
    pendingManualLightBakeTargetsForTest() const noexcept
    {
        return lightBakePendingManualTargets_;
    }
    std::string_view pendingManualLightBakeUndoLabelForTest() const noexcept
    {
        return lightBakePendingManualUndoLabel_;
    }
    bool pendingManualLightBakeUpdateAmbientForTest() const noexcept
    {
        return lightBakePendingManualUpdateAmbient_;
    }
    void clearPendingLightBakeForTest()
    {
        lightBakeRunning_ = false;
        cancelLightBake(true);
    }
    std::size_t undoDepthForTest() const noexcept
    {
        return document_.undoDepth();
    }
    std::size_t redoDepthForTest() const noexcept
    {
        return document_.redoDepth();
    }
    void jumpToUndoDepthForTest(const std::size_t targetUndoDepth)
    {
        jumpToUndoDepth(targetUndoDepth);
    }
    bool goToHistoryDepthForTest(
        const std::size_t targetUndoDepth,
        const bool selectTarget)
    {
        return goToHistoryDepth(targetUndoDepth, selectTarget);
    }
    QTreeWidget* historyWidgetForTest() const noexcept
    {
        return history_;
    }
    void emitHistoryDoubleClickForTest(std::size_t targetUndoDepth);
    bool emitHistoryClickForTest(std::size_t targetUndoDepth);
    bool dispatchHistoryMouseDoubleClickForTest(std::size_t targetUndoDepth);
    QStringList historyContextMenuActionTextsForTest(std::size_t targetUndoDepth);
    QStringList historyContextMenuActionToolTipsForTest(std::size_t targetUndoDepth);
    bool historyContextMenuToolTipsVisibleForTest(std::size_t targetUndoDepth);
    bool triggerHistoryContextActionForTest(
        std::size_t targetUndoDepth,
        bool selectTarget);
    bool navigateToEntityForTest(
        OutlinerKind kind,
        std::int32_t index,
        std::int32_t sectorIndex = -1,
        std::int32_t surfaceIndex = -1,
        bool animate = true,
        bool preserveCogContext = false)
    {
        return navigateToEntity(
            kind,
            index,
            sectorIndex,
            surfaceIndex,
            animate,
            preserveCogContext);
    }
    bool outlinerDoubleClickEntityForTest(
        OutlinerKind kind,
        std::int32_t index,
        std::int32_t sectorIndex = -1,
        std::int32_t surfaceIndex = -1);
    bool undoDocumentForTest()
    {
        return document_.undo();
    }
    bool redoDocumentForTest()
    {
        return document_.redo();
    }
    bool beginSectorDrawForTest(
        int planeAxis,
        bool atView,
        bool freeView,
        const QPoint& widgetPos);
    std::optional<Sector> sectorDrawFrameForTest(
        const TVector& origin, const TVector& axisA,
        const TVector& axisB, const TVector& axisHeight,
        const TVector& dimensions, bool viewOriented) const;
    void setSelectedSectorShapeForTest(std::string shapeName)
    {
        selectedSectorShapeName_ = std::move(shapeName);
    }
    const std::string& selectedSectorShapeForTest() const noexcept
    {
        return selectedSectorShapeName_;
    }
    std::int32_t createSectorFromTaggedShapeForTest(
        Level& level,
        const level::SectorShape& shape,
        const TVector& center,
        double size) const
    {
        return createSectorFromTaggedShape(level, shape, center, size);
    }
    std::int32_t createSectorFromTaggedShapeBoxAxesForTest(
        Level& level,
        const level::SectorShape& shape,
        const TVector& minCorner,
        const TVector& maxCorner,
        int localXAxis,
        int localYAxis,
        int localZAxis) const
    {
        return createSectorFromTaggedShapeBoxAxes(
            level,
            shape,
            minCorner,
            maxCorner,
            localXAxis,
            localYAxis,
            localZAxis);
    }
    bool finishSectorDrawFootprintForTest(
        const QPoint& widgetPos,
        Qt::KeyboardModifiers modifiers = Qt::NoModifier,
        bool dragged = true);
    bool commitSectorDrawPlacementForTest(
        const QPoint& widgetPos,
        Qt::KeyboardModifiers modifiers = Qt::NoModifier);
    std::optional<Sector> sectorDrawPreviewSectorForTest(
        const QPoint& widgetPos,
        Qt::KeyboardModifiers modifiers = Qt::NoModifier) const;
    QVector3D sectorDrawAnchorForTest() const noexcept
    {
        return QVector3D(
            static_cast<float>(sectorDrawAnchor_.x),
            static_cast<float>(sectorDrawAnchor_.y),
            static_cast<float>(sectorDrawAnchor_.z));
    }
    std::pair<TVector, TVector> sectorDrawBoundsForTest(
        const QPoint& widgetPos,
        Qt::KeyboardModifiers modifiers = Qt::NoModifier) const
    {
        const SectorDrawBox box = sectorDrawBoxForMouse(widgetPos, modifiers);
        return { box.min, box.max };
    }
    render::TransformGridFrame sectorDrawGridFrameForTest() const noexcept
    {
        return sectorDrawGridFrame_;
    }
    void setSectorDrawGridFrameForTest(
        const render::TransformGridFrame& frame) noexcept
    {
        sectorDrawGridFrame_ = frame;
    }
    bool sectorDrawBoxValidForTest(
        const QPoint& widgetPos,
        Qt::KeyboardModifiers modifiers = Qt::NoModifier) const
    {
        return sectorDrawBoxForMouse(widgetPos, modifiers).valid;
    }
    void cancelSectorDrawForTest();
    std::size_t layerVisualStateSizeForTest() const noexcept
    {
        return std::min(layerVisible_.size(), std::min(layerLocked_.size(), layerColors_.size()));
    }
    bool layerVisibleForTest(const std::size_t index) const noexcept
    {
        return index < layerVisible_.size() ? layerVisible_[index] : true;
    }
    bool layerLockedForTest(const std::size_t index) const noexcept
    {
        return index < layerLocked_.size() ? layerLocked_[index] : false;
    }
    void setLayerVisualStateForTest(
        std::vector<bool> visible,
        std::vector<bool> locked)
    {
        layerVisible_ = std::move(visible);
        layerLocked_ = std::move(locked);
        normalizeLayerVisualState();
        syncLayerViewState();
        rebuildOutliner();
        rebuildProperties();
        updateEditActions();
    }
    QColor layerColorForTest(const std::size_t index) const
    {
        return index < layerColors_.size() ? layerColors_[index] : QColor{};
    }
    bool undoForTest(std::vector<std::int32_t>* outThingSubsetIndices = nullptr)
    {
        return document_.undo(nullptr, outThingSubsetIndices);
    }
    bool redoForTest(std::vector<std::int32_t>* outThingSubsetIndices = nullptr)
    {
        return document_.redo(nullptr, outThingSubsetIndices);
    }
    void rebuildPropertiesForTest()
    {
        rebuildProperties();
    }
    void appendThingsToOutlinerForTest(const std::size_t firstThingIndex)
    {
        appendThingsToOutliner(firstThingIndex);
    }
    void updateEditActionsForTest()
    {
        updateEditActions();
    }
    std::pair<double, std::optional<QVector3D>> thingPlacementOffsetsForTest(std::string_view name)
    {
        ensureThingPlacementTemplateCache(name);
        return {thingPlacementTemplateGroundOffsetCache_, thingPlacementTemplateSpriteOffsetCache_};
    }
    Thing thingPlacementPoseForTest(const QVector3D& position, const double groundOffset)
    {
        // Inject the already-resolved template ground offset; exercise the
        // same pose builder used by both preview and the placement commit.
        ensureThingPlacementTemplateCache(currentThingTemplateName());
        thingPlacementTemplateGroundOffsetCache_ = groundOffset;
        return buildThingPlacementPreviewThing(position, QVector3D(0.0f, 0.0f, 1.0f), Qt::NoModifier, 0);
    }
    std::optional<Thing> thingPlacementTargetPoseForTest(
        const QPoint& widgetPosition,
        const Qt::KeyboardModifiers modifiers = Qt::NoModifier)
    {
        const Qt::KeyboardModifiers effectiveModifiers =
            thingPlacementEffectiveModifiers(modifiers);
        const std::optional<ThingPlacementTarget> target =
            thingPlacementTargetForViewport(widgetPosition, modifiers);
        if (!target || !target->havePlacementTarget)
        {
            return std::nullopt;
        }
        return buildThingPlacementPreviewThing(
            target->position,
            target->surfaceNormal,
            effectiveModifiers,
            target->sectorFallback,
            &target->documentPosition);
    }
    void updateThingPlacementPreviewForTest(
        const QPoint& widgetPosition,
        const Qt::KeyboardModifiers modifiers = Qt::NoModifier)
    {
        updateThingPlacementPreview(widgetPosition, modifiers);
    }
    void resetThingPlacementTemplateRevisionChecksForTest() noexcept
    {
        thingPlacementTemplateRevisionChecks_ = 0;
    }
    std::uint64_t thingPlacementTemplateRevisionChecksForTest() const noexcept
    {
        return thingPlacementTemplateRevisionChecks_;
    }
    void setThingPlacementTemplateForTest(
        std::string templateName,
        const double groundOffset = 0.0);
    void configureThingPlacementBrushForTest(
        const int count,
        const double size,
        const double spacing = 0.1)
    {
        thingPlacementToolMode_ = ThingPlacementToolMode::brush;
        thingPlacementBrushCount_ = count;
        thingPlacementBrushSize_ = size;
        thingPlacementBrushSpacing_ = spacing;
        thingPlacementBrushScatter_ = 0.0;
        thingPlacementBrushShape_ = ThingPlacementBrushShape::circle;
        thingPlacementBrushRandomRotation_ = false;
        thingPlacementBrushRotatePitch_ = false;
        thingPlacementBrushRotateYaw_ = false;
        thingPlacementBrushRotateRoll_ = false;
        thingPlacementBrushFixedAngle_ = 0.0;
        thingPlacementBrushAllowAir_ = false;
        thingPlacementLayerOverride_ = -1;
        thingPlacementBrushStrokeActive_ = true;
        thingPlacementBrushStrokeHaveLast_ = false;
        thingPlacementBrushStrokePlaced_ = false;
        thingPlacementBrushStrokeUndoPushed_ = false;
        thingPlacementBrushStrokeFirstThing_ = document_.level().things.size();
        setPlacementMode(PlacementMode::thing);
    }
    bool stampThingPlacementBrushForTest(
        const QPoint& widgetPosition,
        const Qt::KeyboardModifiers modifiers = Qt::NoModifier,
        const bool force = true)
    {
        return stampThingPlacementBrushAt(widgetPosition, modifiers, force);
    }
    bool finalizeThingPlacementBrushForTest()
    {
        return finalizeThingPlacementBrushStroke();
    }
    QToolButton* thingMoveStickButtonForTest() const { return viewportThingMoveStickButton_; }
    QToolButton* thingMoveSnapButtonForTest() const { return viewportThingMoveSnapButton_; }
    std::optional<Thing> framePlacementPoseForTest(const QVector3D& position) const
    {
        return buildFramePlacementPose(0, position, QVector3D(0.0f, 0.0f, 1.0f), Qt::NoModifier, true);
    }
    void setThingPlacementActiveForTest(const bool active)
    {
        setPlacementMode(active ? PlacementMode::thing : PlacementMode::none);
    }
    /**
     * Render the same selected-placement template icon path used by the
     * viewport overlay, without constructing the surrounding panel.  The
     * test seam keeps cache invalidation tests independent of list timing.
     */
    QImage thingPlacementTemplateIconForTest(
        const ThingTemplate& tpl,
        const QSize& iconSize);
    std::size_t thingPlacementTemplateIconCacheSizeForTest() const noexcept
    {
        return thingPlacementTemplatePreviewIconCache_.size();
    }
    ui::detail::PreviewResourceDiskWatcher*
    thingPlacementResourceDiskWatcherForTest() const noexcept
    {
        return thingPlacementResourceDiskWatcher_;
    }
    void invalidateThingPlacementResourcePreviewsForTest()
    {
        invalidateThingPlacementResourcePreviews();
    }
    bool thingPlacementActiveForTest() const noexcept
    {
        return placementMode_ == PlacementMode::thing;
    }
    bool viewportSelectToolActiveForTest() const noexcept
    {
        return viewportTransformSelectAction_
            && viewportTransformSelectAction_->isChecked();
    }
    std::int32_t resolvedThingPlacementLayerForTest(const std::int32_t targetSector) const noexcept
    {
        return resolvedThingPlacementLayer(targetSector);
    }
    void setPrimaryResourceOverrideForTest(std::string_view name, assets::ResourceLocator::PrimaryResourceReader reader)
    {
        clearFramePlacementPreview();
        resources_.setPrimaryResourceOverride(name, std::move(reader));
        publishResourceLocatorSnapshot();
        invalidateThingPlacementSnapCaches();
    }
    void addResourceRootForTest(const std::filesystem::path& root)
    {
        clearFramePlacementPreview();
        resources_.addRoot(root);
        publishResourceLocatorSnapshot();
        invalidateThingPlacementSnapCaches();
    }
    std::shared_ptr<const assets::ResourceLocator>
    resourceLocatorSnapshotForTest() const noexcept
    {
        return resourceLocatorSnapshot_;
    }
    std::optional<Thing> buildFramePlacementPoseForTest(
        const std::int32_t ownerThingIndex,
        const QVector3D& placementPosition,
        const QVector3D& surfaceNormal,
        const Qt::KeyboardModifiers modifiers,
        const bool haveSurfaceContact) const
    {
        return buildFramePlacementPose(
            ownerThingIndex,
            placementPosition,
            surfaceNormal,
            modifiers,
            haveSurfaceContact);
    }
#endif

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
    void closeEvent(QCloseEvent* event) override;
    void showEvent(QShowEvent* event) override;

private:
    struct PreviewEditingUiVisibility;

    enum class EditMode : std::uint8_t
    {
        none,
        sector,
        surface,
        thing,
        light,
        frame
    };

    enum class MessageSeverity : std::uint8_t
    {
        verbose,
        trace,
        debug,
        info,
        warning,
        error,
        fatal
    };

    enum class MessageLogCategory : std::uint8_t
    {
        application,
        resources
    };

    struct StagedProjectResourceState
    {
        ProjectType kind{ProjectType::JKDF2};
        std::vector<std::filesystem::path> roots{};
        std::vector<std::filesystem::path> gobs{};
        assets::ResourceLocator locator{};
        std::shared_ptr<const assets::ResourceLocator> locatorSnapshot{};
        std::optional<std::filesystem::path> detectedInstallDir{};
        QStringList information{};
        QStringList warnings{};
    };

    struct StagedGlobalTemplateState
    {
        assets::Templates templates{};
        QStringList information{};
        QStringList warnings{};
        QStringList errors{};
    };

    enum class SurfaceSelectionMode : std::uint8_t
    {
        face,
        edge,
        vertex
    };

    enum class LightEditSubMode : std::uint8_t
    {
        baked,
        thing,
        sector,
        architectural,
    };

    struct ArchitecturalLightingVertexSelection
    {
        std::int32_t thingIndex{-1};
        std::int32_t lodIndex{-1};
        std::int32_t meshIndex{-1};
        std::int32_t vertexIndex{-1};

        constexpr bool valid() const noexcept
        {
            return thingIndex >= 0 && lodIndex >= 0 && meshIndex >= 0
                && vertexIndex >= 0;
        }
        constexpr bool operator==(
            const ArchitecturalLightingVertexSelection&) const noexcept = default;
    };

    struct ArchitecturalLightingInteractiveState
    {
        struct SelectedVertex
        {
            ArchitecturalLightingVertexSelection selection{};
            std::int32_t sourceRecordIndex{-1};
            // Capture the resolved model revision and source-mesh topology for
            // this owner at selection time. A vertex popup may span Things
            // that use different 3DOs; commit revalidates each snapshot
            // independently instead of borrowing the first owner's metadata.
            std::string modelIdentity{};
            level::MotsArchLightingTopology topology{};
            MotsArchLightVertex baseline{};
            MotsArchLightVertex preview{};
            std::array<bool, 4> dirty{};
        };

        std::int32_t ownerThingIndex{-1};
        std::int32_t sourceRecordIndex{-1};
        std::uint64_t documentEpoch{};
        std::uint64_t undoSignature{};
        std::uint64_t resourceIdentity{};
        std::string modelIdentity{};
        level::MotsArchLightingTopology topology{};
        std::optional<MotsArchLightObject> baseline{};
        std::optional<MotsArchLightVertex> baselineVertex{};
        std::optional<MotsArchLightVertex> previewVertex{};
        std::optional<ArchitecturalLightingVertexSelection> vertex{};
        std::vector<SelectedVertex> selectedVertices{};
        level::MotsArchLightingVertexMode previewMode{
            level::MotsArchLightingVertexMode::Rgb};
        bool active{};
        bool previewDirty{};

        void clear() noexcept
        {
            ownerThingIndex = -1;
            sourceRecordIndex = -1;
            documentEpoch = 0;
            undoSignature = 0;
            resourceIdentity = 0;
            modelIdentity.clear();
            topology = {};
            baseline.reset();
            baselineVertex.reset();
            previewVertex.reset();
            vertex.reset();
            selectedVertices.clear();
            previewMode = level::MotsArchLightingVertexMode::Rgb;
            active = false;
            previewDirty = false;
        }
    };

    enum class PlacementMode : std::uint8_t
    {
        none,
        cube,
        thing,
        light,
        thingLight,
        sectorLight,
    };

    enum class ThingLightPlacementKind : std::uint8_t
    {
        regular,
        headlight,
    };

    enum class SectorDrawStage : std::uint8_t
    {
        none,
        footprint,
        height,
    };

    enum class SectorDrawPlaneAxis : std::uint8_t
    {
        x,
        y,
        z,
    };

    struct SectorDrawBox
    {
        TVector min{};
        TVector max{};
        TVector origin{};
        TVector axisA{1.0f, 0.0f, 0.0f};
        TVector axisB{0.0f, 1.0f, 0.0f};
        TVector axisHeight{0.0f, 0.0f, 1.0f};
        bool viewOriented{};
        bool valid{};
    };

    enum class UvEditorDockPosition : std::uint8_t
    {
        bottom,
        top,
        left,
        right,
    };

    enum class ThingPlacementDockPosition : std::uint8_t
    {
        right,
        left,
        top,
        bottom,
    };

    enum class ThingPlacementDefaultMode : std::uint8_t
    {
        surface,
        surfaceTilt,
        free,
    };

    enum class ThingPlacementToolMode : std::uint8_t
    {
        single,
        brush,
    };

    enum class ThingPlacementBrushShape : std::uint8_t
    {
        circle,
        square,
        rectangle,
    };

    struct ThingPlacementTarget
    {
        QVector3D position{};
        TVector documentPosition{};
        QVector3D surfaceNormal{};
        bool haveSurfaceHit{};
        bool havePlacementTarget{};
        std::int32_t sectorFallback{-1};
    };
    struct FramePlacementOptions
    {
        bool surfaceStick{true};
        bool surfaceAlign{};
        bool snapToThing{};
        bool rotateToSnap{true};

        bool operator==(const FramePlacementOptions&) const noexcept = default;
    };
    struct FramePlacementOwnerProxy
    {
        std::int32_t thingIndex{-1};
        std::uint64_t documentSignature{};
        std::uint64_t resourceIdentity{};
        Thing thing{};
        TVector lastFrameOrientation{};
    };
    struct FramePlacementPreview
    {
        std::int32_t ownerThingIndex{-1};
        QPoint widgetPosition{};
        Qt::KeyboardModifiers modifiers{Qt::NoModifier};
        std::uint64_t documentSignature{};
        std::uint64_t resourceIdentity{};
        FramePlacementOptions options{};
        QVector3D position{};
        TVector documentPosition{};
        QVector3D surfaceNormal{};
        std::int32_t surfaceSector{-1};
        std::int32_t surfaceIndex{-1};
        bool haveSurfaceContact{};
        Thing pose{};
    };
    using ThingPlacementConnector = sed::thing::SnapConnector;

    struct ThingPlacementCachedConnector
    {
        ThingPlacementConnector connector{};
        TVector worldPosition{};
        TVector worldTangent{};
        bool haveWorldTangent{};
    };

    struct ThingPlacementSnapAnchorCacheEntry
    {
        std::int32_t thingIndex{-1};
        TMatrix43 transform{};
        TMatrix43 inverseTransform{};
        TVector origin{};
        TVector pyr{};
        std::optional<ThingBox> box{};
        std::array<double, 3> boxMin{};
        std::array<double, 3> boxMax{};
        std::array<double, 3> boxCenter{};
        double boxRadius{};
        std::vector<ThingPlacementCachedConnector> connectors{};
        bool finite{};
    };

    void setupMenus();
    void setupEditCoreActions(QMenu* editMenu);
    void setupEditClipboardActions(QMenu* editMenu);
    void setupEditDeleteAction();
    void deleteSelection();
    bool deleteSelectedSurfaceVertices(bool force);
    void setupEditEntityMenus(QMenu* editMenu);
    void setupEditHistoryActions();
    void setupFileMenu(QMenu* fileMenu);
    void setupMapMenu(QMenu* mapMenu, QMenu* editMenu);
    void setupTransformMenu(QMenu* transformMenu);
    void setupTranslateTransformAction(QMenu* transformMenu);
    void setupRotateTransformAction(QMenu* transformMenu);
    void setupScaleTransformAction(QMenu* transformMenu);
    void setupMirrorTransformAction(QMenu* transformMenu);
    void setupFlipSurfaceTransformAction(QMenu* transformMenu);
    void setupViewMenu(QMenu* viewMenu);
    void setupViewportKeyBindings(QMenu* viewMenu);
    void setupToolbars();
    void setupDockWidgets();
    void setupResourceEditorsDock();
    struct ResourceEditorTabHost
    {
        QPointer<QDockWidget> dock{};
        QPointer<QTabWidget> tabs{};
        std::uint64_t sessionGeneration{};
        QString stableId{};
        bool primary{};
        ui::resources::ResourceAuthoringKind kind{};
    };
    ui::resources::ResourceAuthoringContext makeResourceAuthoringContext(
        ui::resources::ResourceAuthoringKind kind,
        QTabWidget* preferredHost = nullptr,
        bool enableProjectOpen = true);
    bool openProjectResourceInNewTab(
        QWidget* source,
        ui::resources::ResourceAuthoringKind kind,
        QTabWidget* preferredHost);
    bool openLooseResourceInNewTab(
        ui::resources::ResourceAuthoringKind kind,
        QTabWidget* preferredHost);
    void openResourceEditor(
        ui::resources::ResourceAuthoringKind kind,
        int openMode = 0,
        QTabWidget* host = nullptr);
    std::vector<QPointer<QWidget>> resourceEditorPages() const;
    QTabWidget* resourceEditorTabsForPage(const QWidget* page) const noexcept;
    QDockWidget* resourceEditorDockForTabs(const QTabWidget* tabs) const noexcept;
    QTabWidget* resourceEditorDropTargetAtPosition(
        QTabWidget* source,
        QWidget* page,
        const QPoint& globalPosition,
        bool* rejectCrossKindTabBar = nullptr) const noexcept;
    QTabWidget* activeResourceEditorTabHost() const noexcept;
    QTabWidget* primaryResourceEditorTabHost(ui::resources::ResourceAuthoringKind kind) const noexcept;
    bool resourceEditorTabHostIsPrimary(const QTabWidget* tabs) const noexcept;
    bool saveResourceEditorGroup(ui::resources::ResourceAuthoringKind kind);
    QCborMap normalizeResourceEditorRecoveryState(const QCborMap& payload) const;
    void registerResourceEditorTabHost(
        QDockWidget* dock,
        QTabWidget* tabs,
        bool primary = false,
        QString stableId = {},
        ui::resources::ResourceAuthoringKind kind = {});
    void unregisterResourceEditorTabHost(QTabWidget* tabs);
    QDockWidget* createResourceEditorDockHost(
        const QString& stableId,
        bool floating,
        ui::resources::ResourceAuthoringKind kind = {},
        bool primary = false);
    void configureResourceEditorTabHost(QDockWidget* dock, QTabWidget* tabs);
    void observeResourceEditorWindowDrops(QDockWidget* dock);
    void mergeResourceEditorDockHosts(QDockWidget* sourceDock, QDockWidget* targetDock,
        std::uint64_t generation);
    bool prepareResourceEditorRecoveryHosts(const QCborMap& payload);
    void transferResourceEditorTab(
        QTabWidget* source,
        QWidget* page,
        const QPoint& globalPosition);
    bool filterResourceEditorTabBar(QObject* watched, QEvent* event);
    void openResourceEditorProjectFile(ui::resources::ResourceAuthoringKind kind);
    void openResourceEditorLooseFile(ui::resources::ResourceAuthoringKind kind);
    void resourceEditorPaneChanged(QWidget* pane);
    void refreshResourceEditorTab(QWidget* pane);
    bool confirmCloseResourceEditors(bool* discardedAny = nullptr);
    void closeResourceEditorsAfterReplacement();
    bool hasUnsavedResourceEditorChanges() const;
    bool triggerActiveResourceEditorSave(bool saveAs = false);
    bool triggerActiveResourceEditorUndoRedo(bool redo);
    bool resourceEditorOwnsFocus() const;
    QCborMap captureResourceEditorRecoveryState() const;
    QByteArray captureAuthoredResourceEditorRecoveryState() const;
    bool resourceEditorAuthoredStateMatchesSavedBaseline() const;
    bool validateResourceEditorRecoveryState(
        const QCborMap& payload,
        QString* warning = nullptr) const;
    bool restoreResourceEditorRecoveryState(
        const QCborMap& payload,
        QStringList* warnings = nullptr);
    void restoreWindowPlacement();
    void saveWindowPlacement() const;
    void initializeRecoverySupport();
    void configureRecoveryTimerFromSettings();
    // `semanticMutation` is reserved for the central editor mutation hook.
    // Ordinary viewport/layout notifications do not advance either serial;
    // the EDIT hash carries editor-only changes while explicit same-undo
    // authored hooks advance the cheap event generation.
    void noteRecoveryStateChanged();
    void noteRecoveryStateChanged(bool semanticMutation);
    void noteCleaveAuthoredStateChanged(std::uint64_t revision);
    void noteRecoveryEditorStateChangedIfNeeded();
    bool hasRecoverableChanges() const;
    bool hasRecoveryOfferEligibleChanges() const;
    void handleRecoveryTimerTimeout();
    void flushRecoverySnapshot(bool forceFinalCapture = false);
    void scheduleRecoverySnapshot(bool force = false);
    void handleRecoveryWriteFinished();
    void switchRecoveryDocument(
        const std::filesystem::path& sourcePath,
        bool newUntitledSession,
        bool removePreviousRecovery,
        bool preserveDocumentLifetime = false);
    void completeRecoveryAfterNormalSave(const std::filesystem::path& savedPath);
    void discardCurrentRecovery(
        const std::filesystem::path& preservedPath = {});
    void discardRecoveryFamily(
        const QString& identity,
        const QString& sessionId,
        const std::filesystem::path& preservedPath = {});
    // Retire only automatic live offers for one editing session. Historical
    // `.sedautosave` restore points remain available from Autosave History.
    void discardRecoveryLiveFamily(
        const QString& identity,
        const QString& sessionId,
        const std::filesystem::path& preservedPath = {});
    bool removeRecoveryPathSafely(
        const std::filesystem::path& path,
        QString* error = nullptr,
        const ui::recovery::RecoveryMetadata* expectedMetadata = nullptr);
    void showAutosaveHistory();
    void shutdownRecoverySupport(bool removePublishedRecovery);
    bool offerStartupRecovery();
    enum class RecoveryOfferResult : std::uint8_t
    {
        none,
        recovered,
        openSaved,
        cancelled,
    };
    RecoveryOfferResult offerRecoveryForPath(
        const std::filesystem::path& sourcePath,
        bool startupOffer,
        std::filesystem::path* selectedRecoveryPath = nullptr,
        QString* selectedRecoveryIdentity = nullptr,
        QString* selectedRecoverySessionId = nullptr);
    bool applyRecoveryFile(const std::filesystem::path& recoveryPath);
    QByteArray captureRecoveryEditorState() const;
    QByteArray recoveryCurrentLevelStateHash() const;
    bool validateRecoveryEditorState(
        const QByteArray& payload,
        QString* warning = nullptr);
    bool restoreRecoveryEditorState(
        const QByteArray& payload,
        QString* warning = nullptr,
        bool deferViewportRebuild = false);
    void restoreWorkspaceHistoryAttachments(
        const ui::recovery::WorkspaceHistoryState& workspaceHistory,
        QStringList* warnings = nullptr);
    bool processRecoveryEditorState(
        const QByteArray& payload,
        QString* warning,
        bool deferViewportRebuild,
        bool validateOnly);
    QByteArray captureUvEditorWorkspaceState() const;
    bool validateUvEditorWorkspaceState(
        const QByteArray& payload,
        const editor::Selection& documentSelection,
        EditMode recoveredEditMode,
        SurfaceSelectionMode recoveredSurfaceMode,
        bool recoveredViewportFacing,
        QString* warning = nullptr) const;
    bool restoreUvEditorWorkspaceState(
        const QByteArray& payload,
        const editor::Selection& documentSelection,
        EditMode recoveredEditMode,
        SurfaceSelectionMode recoveredSurfaceMode,
        bool recoveredViewportFacing,
        QString* warning = nullptr);
    void resetUvEditorWorkspaceState();
    QByteArray captureCogEditorRecoveryState() const;
    struct CogScriptEditorTabHost
    {
        QPointer<QDockWidget> dock{};
        QPointer<QTabWidget> tabs{};
        QString stableId{};
        std::uint64_t sessionGeneration{};
        bool primary{};
    };
    std::vector<QPointer<QWidget>> cogScriptEditorPages() const;
    std::vector<QPointer<QDockWidget>> cogScriptEditorDocks() const;
    QTabWidget* cogScriptEditorTabsForPage(const QWidget* page) const noexcept;
    QDockWidget* cogScriptEditorDockForTabs(const QTabWidget* tabs) const noexcept;
    QTabWidget* cogScriptEditorDropTargetAtPosition(
        QTabWidget* source,
        QWidget* page,
        const QPoint& globalPosition,
        bool* rejectForeignTabBar = nullptr) const noexcept;
    QTabWidget* activeCogScriptEditorTabHost() const noexcept;
    void registerCogScriptEditorTabHost(QDockWidget* dock, QTabWidget* tabs, const QString& id, bool primary);
    void configureCogScriptEditorTabHost(QDockWidget* dock, QTabWidget* tabs);
    QDockWidget* createCogScriptEditorDockHost(const QString& id, bool floating);
    bool prepareCogEditorRecoveryHosts(const QByteArray& payload);
    void observeCogScriptEditorRecoveryHosts();
    void transferCogScriptEditorTab(QTabWidget* source, QWidget* page, const QPoint& globalPosition);
    bool closeCogScriptEditorHost(QTabWidget* tabs);
    void cleanupEmptyCogScriptEditorHosts();
    QByteArray captureAuthoredCogRecoveryState() const;
    void markRecoveryCogBufferAsSaved(const std::filesystem::path& path);
    bool validateCogEditorRecoveryState(
        const QByteArray& payload,
        QString* warning = nullptr) const;
    bool restoreCogEditorRecoveryState(const QByteArray& payload, QStringList* warnings = nullptr);
    bool hasUnsavedCogScriptChanges() const;
    bool hasUnsavedAuthoredLevelChanges() const;
    std::filesystem::path recoveryDirectory() const;
    std::vector<std::filesystem::path> recoverySearchDirectories() const;
    void setupViewportOverlay();
    void positionViewportOverlay();
    void positionViewportMapSettingsPanel();
    void positionViewportThingRenderingPanel();
    void syncViewportContextualOverlays();
    /**
     * Reflow the visible top-center option bars as one collision-aware stack.
     * The stack avoids the full top-left edit/transform/tool bars and the
     * top-right viewport controls, including on narrow viewports.
     */
    void reflowTopCenterOverlays();
    QPoint avoidThingPlacementPanelForOverlay(const QSize& overlaySize, QPoint desired, int margin) const;
    void setupAddSectorOptionsOverlay();
    void positionAddSectorOptionsOverlay();
    void syncAddSectorOptionsOverlay();
    void setupTransformMoveOptionsOverlay();
    void positionTransformMoveOptionsOverlay();
    void syncTransformMoveOptionsOverlay();
    void setupLightBakeControls();
    void positionLightBakeOverlay();
    void syncLightBakeOverlay();
    void setLightBakeAutoEnabled(bool enabled);
    void resetLightBakeSourceTracking();
    void pollLightBakeSourceChanges();
    void updateLightBakeTransformPreview();
    void cancelLightBakeTransformPreviewWorker();
    void prepareLightBakeTransformPreviewCommit();
    void restoreLightBakeTransformPreview();
    void requestLightBake(
        bool automatic,
        bool selectedLightsOnly = false,
        std::vector<level::LightingInfluenceSphere> capturedSelectedInfluences = {});
    void startLightBake(
        std::vector<std::int32_t> targets,
        bool automatic,
        std::vector<level::LightingInfluenceSphere> dirtyInfluences = {},
        std::vector<std::int32_t> permittedTargets = {},
        std::string manualUndoLabel = {},
        std::optional<bool> requestedUpdateAmbient = std::nullopt);
    void updateLightBakeProgress(int percent);
    void finishLightBake(
        Level bakedLevel,
        std::vector<std::int32_t> targets,
        std::uint64_t generation,
        std::uint64_t sourceFingerprint,
        bool canceled,
        bool automatic,
        bool updateSectorAmbient,
        std::string manualUndoLabel,
        std::uint64_t motsArchSourceFingerprint,
        bool motsArchRequested,
        bool motsArchSucceeded,
        std::string motsArchMessage,
        std::vector<std::int32_t> changedArchThings,
        std::shared_ptr<const assets::ResourceLocator> resourceSnapshot);
    void cancelLightBake(bool discardPending = true);
    void showLightBakeProgressDialog();
    std::vector<std::int32_t> collectLightBakeSectorTargets(bool visibleOnly) const;
    void setupSectorJoinOptionsOverlay();
    void positionSectorJoinOptionsOverlay();
    void syncSectorJoinOptionsOverlay();
    void setupSectorThrustOptionsOverlay();
    void positionSectorThrustOptionsOverlay();
    void syncSectorThrustOptionsOverlay();
    void setupCleaveOptionsOverlay();
    void positionCleaveOptionsOverlay();
    void syncCleaveOptionsOverlay();
    void beginSectorJoinPreview();
    void updateSectorJoinPreview();
    void cancelSectorJoinPreview();
    void applySectorJoinPreview();
    void setupThingMoveOptionsOverlay();
    void positionThingMoveOptionsOverlay();
    void syncThingMoveOptionsOverlay();
    void setupSurfaceExtrudeOptionsOverlay();
    void positionSurfaceExtrudeOptionsOverlay();
    void syncSurfaceExtrudeOptionsOverlay();
    void beginSurfaceExtrudePreview();
    void updateSurfaceExtrudePreview();
    void refreshSurfaceExtrudeGridSnapState(bool quantizeValues);
    void cancelSurfaceExtrudePreview();
    void applySurfaceExtrudePreview();
    std::optional<SurfaceRef> selectedSingleSurfaceForExtrude() const;
    void setupThingPlacementPanel();
    void positionThingPlacementPanel();
    void syncThingPlacementPanel();
    void refreshThingPlacementTemplateList();
    void scheduleThingPlacementTemplateIcons(
        int generation,
        int startIndex,
        int delayMs);
    void populateThingPlacementTemplateIcons(int generation, int startIndex);
    void queueThingPlacementTemplateModelPrep(const std::string& modelName);
    bool loadThingPlacementTemplateIconForRow(
        int row,
        bool* renderedAuthoritativeThumbnail = nullptr);
    void invalidateThingPlacementResourcePreviews();
    void applyThingPlacementTemplateFilters();
    void setThingPlacementPanelVisible(bool visible);
    void cancelPlacementForViewportTool(QAction* activatingAction);
    void setViewportTransformActionChecks(QAction* activeAction);
    void deactivateViewportTransformForExclusiveTool();
    void restoreViewportTransformAfterExclusiveTool();
    void syncViewportSelectionInputEnabled();
    [[nodiscard]] bool viewportSelectionToolActive() const noexcept;
    [[nodiscard]] bool viewportEntitySelectionInputActive() const noexcept;
    [[nodiscard]] bool viewportModificationToolActive() const noexcept;
    void deactivateViewportEditFunctions();
    void applyThingPlacementDockPosition(ThingPlacementDockPosition position, bool preserveSize = true);
    /**
     * Converts a thing placement dock enum into the shared panel-dock side.
     */
    ui::viewport::PanelDockSide thingPlacementPanelDockSideForPosition(ThingPlacementDockPosition position) const noexcept;
    /**
     * Converts the current thing placement dock into the shared panel-resize direction.
     */
    ui::viewport::PanelDockSide thingPlacementPanelDockSide() const noexcept;
    /**
     * Converts shared panel-dock geometry results back into the thing placement dock enum.
     */
    ThingPlacementDockPosition thingPlacementDockPositionForPanelDockSide(ui::viewport::PanelDockSide dockSide) const noexcept;
    /**
     * Resolves the placement-panel dock target under a global cursor point.
     */
    std::optional<ThingPlacementDockPosition> thingPlacementDockDropTarget(const QPoint& globalPos) const;
    /**
     * Builds the placement-panel docking preview rectangle in viewport-local coordinates.
     */
    QRect thingPlacementDockPreviewRect(ThingPlacementDockPosition position) const;
    /**
     * Resolves a release-drag fallback dock target when the cursor is not near a viewport edge.
     */
    std::optional<ThingPlacementDockPosition> thingPlacementDockPositionForDragDelta(const QPoint& delta) const noexcept;
    /**
     * Returns true when the cursor is on the active resize edge of the placement panel.
     */
    bool thingPlacementPanelResizeEdgeHit(const QPoint& globalPos) const;
    /**
     * Applies an outer placement-panel resize drag and refreshes affected layout.
     */
    void applyThingPlacementPanelResize(const QPoint& globalPos);
    /**
     * Applies a template-filter resize drag and refreshes affected layout.
     */
    void applyThingPlacementFilterResize(const QPoint& globalPos);
    void deselectThingPlacementTemplate();
    void setupViewportTransformOverlay();
    void positionViewportTransformOverlay();
    void setupViewportToolsOverlay();
    void positionViewportToolsOverlay();
    void updateSurfaceTooltips();
    void syncViewportToolsOverlay();
    void setupViewportEditModeOverlay();
    void positionViewportEditModeOverlay();
    void refreshOutlinerTabCaptions();
    void populateOutlinerSectorChildren(QTreeWidgetItem* sectorItem);
    void populateOutlinerSurfaceVertexChildren(QTreeWidgetItem* surfaceItem);
    void rebuildOutliner();
    void appendThingsToOutliner(std::size_t firstThingIndex);
    void appendLightsToOutliner(std::size_t firstLightIndex);
    /**
     * Returns the selected Surface which owns the explicit viewport primary.
     * Selection containers are unordered, so callers which drive focused UI
     * (Item Edit and the Outliner current row) must not choose begin().
     */
    [[nodiscard]] std::optional<SurfaceRef>
    activeSelectedSurfaceRef() const noexcept;
    void syncOutlinerSelectionOnly();
    void refreshPropertiesEditability();
    void rebuildProperties();
    void rebuildChecks();
    void scheduleRebuildChecks(editor::DocumentDirty dirty);
    void refreshThingChecks(
        const std::vector<std::int32_t>& thingIndices,
        bool refreshResources,
        bool containmentAlreadyResolved = false);
    void showCheckerConfigurationDialog(ui::checks::CheckRuleGroup group);
    void syncCogScriptWatches();
    void refreshCogScriptEditorAppearance();
    void previewCogScriptEditorFont(const QFont& font);
    void setupCogScriptEditorActions();
    void setupCogScriptEditorDock(QDockWidget* dock);
    enum class CogScriptTabCloseScope : std::uint8_t
    {
        others,
        toRight,
        saved,
        all,
    };
    void populateCogScriptTabContextMenu(QMenu* menu, QWidget* contextPage);
    std::vector<QPointer<QWidget>> cogScriptTabsForClose(
        QWidget* contextPage,
        CogScriptTabCloseScope scope) const;
    bool closeCogScriptTabs(
        QWidget* contextPage,
        CogScriptTabCloseScope scope);
    void updateCogScriptDockWidthConstraints();
    void restoreCogScriptDockWidth();
    void restoreStandaloneOutlinerDockWidth();
    int defaultCogScriptDockWidth() const;
    void createNewCogScript();
    // Returns an empty string when the normalized name is available.  This is
    // shared by the live name-entry validator and the final creation path so
    // a resource appearing while the dialog is open cannot bypass the UI
    // check.
    QString newCogScriptNameValidationError(const QString& typedName) const;
    void createNewCogScriptNamed(const QString& typedName);
    std::optional<std::filesystem::path> ensureCogScriptEditablePath(
        std::string_view scriptName,
        bool& sourceWasLoose);
    void openCogScriptEditor(std::string_view scriptName);
    bool confirmCloseCogScriptEditors(bool* discardedAny = nullptr);
    void closeCogScriptEditorsAfterReplacement();
    bool triggerActiveCogScriptEditorSave();
    bool cogScriptEditorOwnsFocus() const;
    bool activeCogScriptEditorCanUndoRedo(bool redo) const;
    bool triggerActiveCogScriptEditorUndoRedo(bool redo);
    void refreshCogScriptEditorToolbars();
    bool saveAllCogScriptEditors();
    bool triggerActiveCogScriptEditorSaveForKeyState(
        int key,
        Qt::KeyboardModifiers modifiers,
        bool pressed,
        bool autoRepeat,
        bool cogEditorFocusContext);
    std::optional<std::filesystem::path> preferredExtractionRoot() const;
    std::optional<std::int32_t> createPlacedCogFromValues(
        const std::string& scriptName,
        const std::vector<CogValue>& values,
        bool selectNewCog);
    std::optional<std::int32_t> createPlacedCogFromScript(const std::string& scriptName, bool selectNewCog);
    std::optional<std::int32_t> pickAndCreatePlacedCog(bool selectNewCog);
    void handleCogScriptFileChanged(const QString& path);
    void handleCogScriptDirectoryChanged(const QString& path);
    void refreshCogScriptFileNow(const QString& path);
    void applyCogScriptFileChangedNow(
        const QString& path,
        std::uint64_t watchGeneration,
        std::uint64_t documentEpoch);
    void updateThingPropertiesPreview(bool applyTransformPreview);
    void openThingLightValueDialog(
        std::int32_t thingIndex,
        std::string_view valueName,
        QWidget* anchor = nullptr);
    void openBakedLightParametersDialog(std::int32_t lightIndex);
    void openSectorLightParametersDialog(std::int32_t sectorIndex);
    void clearArchitecturalLightingInteractiveState() noexcept;
    bool refreshArchitecturalLightingInteractiveOwner(std::int32_t thingIndex);
    bool prepareArchitecturalLightingInteractiveSelection(
        const std::vector<ArchitecturalLightingVertexSelection>& vertices);
    void openArchitecturalLightingVertexEditor(
        const ArchitecturalLightingVertexSelection& vertex);
    void openArchitecturalLightingVertexEditor(
        const std::vector<ArchitecturalLightingVertexSelection>& vertices);
    bool stageArchitecturalLightingVertex(
        const ArchitecturalLightingVertexSelection& vertex,
        const MotsArchLightVertex& value,
        level::MotsArchLightingVertexMode mode);
    bool stageArchitecturalLightingSelectedVertices(
        const MotsArchLightVertex& value,
        level::MotsArchLightingVertexMode mode,
        const std::array<bool, 4>& dirtyChannels);
    bool stageArchitecturalLightingSelectedVertices(
        const std::vector<MotsArchLightVertex>& values,
        level::MotsArchLightingVertexMode mode,
        const std::array<bool, 4>& dirtyChannels);
    bool commitArchitecturalLightingInteractiveEdit();
    void cancelArchitecturalLightingInteractiveEdit() noexcept;
    bool initializeArchitecturalLightingForSelection();
    bool resetArchitecturalLightingForSelection();
    bool copyArchitecturalLightingClipboard();
    bool pasteArchitecturalLightingClipboard();
    void toggleEditMode(EditMode mode);
    void updateEditActions();
    void syncCogScriptEditorActionFocus();
    void updateCogScriptEditorFocusFromWidget(QWidget* focusWidget);
    [[nodiscard]] bool documentEditingAllowed() const noexcept;
    void cancelDocumentMutationPreviewsForGamePreview();
    void captureEditingUiVisibilityForGamePreview();
    void settleEditingUiTransitionsForGamePreview();
    void hideEditingUiForGamePreview();
    void applyEditingUiVisibilitySnapshotForGamePreview(
        const PreviewEditingUiVisibility& snapshot,
        bool restoreWorkspaceDock);
    void restoreEditingUiVisibilityAfterGamePreview();
    void setEditMode(EditMode mode);
    void setSurfaceSelectionMode(SurfaceSelectionMode mode);
    void setLightEditSubMode(LightEditSubMode mode);
    [[nodiscard]] int lightEditOutlinerTab() const noexcept;
    // Keep GlView forward-declared in this header; convert the stored pick-mode
    // value back to GlView::PickMode inside state.cpp where GlView is complete.
    [[nodiscard]] int lightEditPickModeValue() const noexcept;
    void setPlacementMode(PlacementMode mode);
    void setThingLightPlacementKind(ThingLightPlacementKind kind);
    void updateThingLightPlacementTarget(std::int32_t thingIndex);
    [[nodiscard]] bool thingLightPlacementTargetValid(std::int32_t thingIndex) const;
    bool attachThingLightToThing(std::int32_t thingIndex);
    void updateSectorLightPlacementTarget(std::int32_t sectorIndex);
    [[nodiscard]] bool sectorLightPlacementTargetValid(std::int32_t sectorIndex) const;
    bool placeSectorLightInSector(std::int32_t sectorIndex);
    void cancelSectorDrawPlacement();
    void updateSectorDrawPreview(const QPoint& widgetPos, Qt::KeyboardModifiers modifiers);
    void updateSectorDrawModifierPreviewForKeyState(
        int key,
        Qt::KeyboardModifiers eventModifiers,
        bool pressed,
        bool viewportFocusContext,
        bool viewportPointerContext);
    bool beginSectorDrawPlacement(const QVector3D& position, bool haveSurfaceHit, const QPoint& widgetPos, Qt::KeyboardModifiers modifiers);
    bool finishSectorDrawFootprint(const QPoint& widgetPos, Qt::KeyboardModifiers modifiers, bool dragged);
    bool commitSectorDrawPlacement();
    SectorDrawBox sectorDrawBoxForMouse(const QPoint& widgetPos, Qt::KeyboardModifiers modifiers) const;
    std::vector<TVector> buildSectorDrawPreviewLines(const SectorDrawBox& box) const;
    std::vector<TVector> buildSectorDrawCursorGuideLines(const QPoint& widgetPos) const;
    void updateSectorDrawCursorGuide(const QPoint& widgetPos);
    [[nodiscard]] bool sectorDrawGridSnapActive() const noexcept;
    [[nodiscard]] TVector snapSectorDrawPointToGrid(
        const TVector& point,
        const TVector& planePoint,
        const TVector& planeNormal) const noexcept;
    TVector sectorDrawPlaneNormal() const noexcept;
    TVector sectorDrawPlacementPlanePoint() const noexcept;
    std::array<int, 3> sectorDrawWorldAxes() const noexcept;
    [[nodiscard]] level::UvTextureDimensionsResolver
        generatedSectorUvDimensionsResolver() const;
    std::int32_t createSectorFromTaggedShape(
        Level& level,
        const level::SectorShape& shape,
        const TVector& center,
        double size) const;
    std::int32_t createSectorFromTaggedShapeBoxAxes(
        Level& level,
        const level::SectorShape& shape,
        const TVector& minCorner,
        const TVector& maxCorner,
        int localXAxis,
        int localYAxis,
        int localZAxis) const;
    std::int32_t createSectorFromDrawBox(Level& level, const SectorDrawBox& box) const;
    std::string currentThingTemplateName() const;
    QString selectedThingTemplateLabel() const;
    std::int32_t resolvedThingPlacementLayer(std::int32_t targetSector) const noexcept;
    void refreshThingPlacementLayerControl();
    bool thingPlacementPointerInsidePanel() const;
    [[nodiscard]] FramePlacementOptions framePlacementOptions(
        Qt::KeyboardModifiers modifiers) const;
    [[nodiscard]] std::optional<std::int32_t>
    framePlacementOwnerThingIndex() const;
    [[nodiscard]] std::optional<FramePlacementPreview>
    resolveFramePlacementPreview(
        const QPoint& widgetPos,
        Qt::KeyboardModifiers modifiers) const;
    [[nodiscard]] std::optional<FramePlacementPreview>
    framePlacementPreviewForCommit(
        const QPoint& widgetPos,
        Qt::KeyboardModifiers modifiers);
    void updateFramePlacementPreview(
        const QPoint& widgetPos,
        Qt::KeyboardModifiers modifiers);
    void clearFramePlacementPreview();
    const ui::shortcuts::ModifierBindings& thingPlacementTiltModifierBindings() const noexcept
    {
        return thingPlacementTiltModifierBindings_;
    }
    const ui::shortcuts::ModifierBindings& thingPlacementFreeModifierBindings() const noexcept
    {
        return thingPlacementFreeModifierBindings_;
    }
    const ui::shortcuts::ModifierBindings& transformGridSnapInvertModifierBindings() const noexcept
    {
        return transformGridSnapInvertModifierBindings_;
    }
    const ui::shortcuts::ModifierBindings& transformGridStickInvertModifierBindings() const noexcept
    {
        return transformGridStickInvertModifierBindings_;
    }
    const ui::shortcuts::ModifierBindings& sectorMoveSnapAlignInvertModifierBindings() const noexcept
    {
        return sectorMoveSnapAlignInvertModifierBindings_;
    }
    const ui::shortcuts::ModifierBindings& surfaceGeometrySnapInvertModifierBindings() const noexcept
    {
        return surfaceGeometrySnapInvertModifierBindings_;
    }
    const ui::shortcuts::ModifierBindings& thingMoveSurfaceStickInvertModifierBindings() const noexcept
    {
        return thingMoveSurfaceStickInvertModifierBindings_;
    }
    const ui::shortcuts::ModifierBindings& thingMoveSurfaceAlignInvertModifierBindings() const noexcept
    {
        return thingMoveSurfaceAlignInvertModifierBindings_;
    }
    const ui::shortcuts::ModifierBindings& thingMoveSnapInvertModifierBindings() const noexcept
    {
        return thingMoveSnapInvertModifierBindings_;
    }
    const ui::shortcuts::ModifierBindings& thingMoveSnapRotateInvertModifierBindings() const noexcept
    {
        return thingMoveSnapRotateInvertModifierBindings_;
    }
    ThingPlacementDefaultMode thingPlacementModeForModifiers(Qt::KeyboardModifiers modifiers) const noexcept;
    Qt::KeyboardModifiers thingPlacementEffectiveModifiers(Qt::KeyboardModifiers modifiers) const noexcept;
    QString thingPlacementModeLabel(ThingPlacementDefaultMode mode) const;
    void syncThingPlacementOptionControls();
    void updateThingPlacementSelectedPreview();
    void saveThingPlacementOptions() const;
    void updateThingPlacementPreview(const QPoint& widgetPos, Qt::KeyboardModifiers modifiers);
    void updateThingPlacementModifierPreviewForKeyState(
        int key,
        Qt::KeyboardModifiers eventModifiers,
        bool pressed,
        bool viewportFocusContext,
        bool viewportPointerContext);
    void updateTransformMoveModifierPreviewForKeyState(
        int key,
        Qt::KeyboardModifiers eventModifiers,
        bool pressed);
    /**
     * Maps a Qt drag/drop position into GlView coordinates and rejects drops that
     * are still inside the thing-placement controls.
     */
    std::optional<QPoint> thingPlacementViewportPosForDropEvent(QObject* watched, const QDropEvent* dropEvent) const;
    /**
     * Applies a dropped template name to the active thing-placement state.
     */
    bool activateThingTemplatePlacementFromDrop(const QString& templateName);
    /**
     * Handles thing-template drag/drop events routed through the global event filter.
     */
    bool handleThingPlacementTemplateDropEvent(QObject* watched, QEvent* event);
    /**
     * Handles horizontal spin-box scrub events routed through the global event filter.
     */
    std::optional<bool> handleThingPlacementSpinScrubEvent(QObject* watched, QEvent* event);
    /**
     * Polishes and sizes message boxes that enter through the global event filter.
     */
    void polishMessageBoxForEventFilter(QObject* watched, QEvent* event);
    /**
     * Keeps related dock widths synchronized after dock resize events.
     */
    void syncDockWidthsForEventFilterResize(QObject* watched, QEvent* event);
    /**
     * Clears template-list selection when the list background is clicked.
     */
    void handleThingPlacementTemplateListBackgroundPress(QObject* watched, QEvent* event);
    /**
     * Handles thing-placement yaw wheel shortcuts routed through the global event filter.
     */
    std::optional<bool> handleThingPlacementWheelEvent(QObject* watched, QEvent* event);
    /**
     * Handles thing-placement panel resize and filter resize mouse events.
     */
    std::optional<bool> handleThingPlacementPanelResizeEvent(QObject* watched, QEvent* event);
    /**
     * Handles double-click navigation/resource picking from line edits.
     */
    std::optional<bool> handleLineEditDoubleClickEvent(QObject* watched, QEvent* event);
    /**
     * Opens the correct resource picker for a double-clicked COG value editor.
     */
    std::optional<QString> pickCogValueResourceForDoubleClick(ui::ResourcePickerKind kind, const QString& currentValue);
    /**
     * Handles global key shortcuts routed through the event filter.
     */
    std::optional<bool> handleGlobalShortcutKeyEvent(QObject* watched, QEvent* event);
    /**
     * Clears transient selection modifier hints on focus/leave/deactivate events.
     */
    void clearSelectionModifierHintForEventFilterEvent(QEvent* event);
    /**
     * Handles an active thing-placement header drag.
     */
    std::optional<bool> handleActiveThingPlacementHeaderDragEvent(QEvent* event);
    /**
     * Starts or completes thing-placement header drag events from header widgets.
     */
    std::optional<bool> handleThingPlacementHeaderWidgetEvent(QObject* watched, QEvent* event);
    /**
     * Handles an active UV editor header drag.
     */
    std::optional<bool> handleActiveUvEditorPanelHeaderDragEvent(QEvent* event);
    /**
     * Starts or completes UV editor header drag events from the panel header.
     */
    std::optional<bool> handleUvEditorPanelHeaderWidgetEvent(QObject* watched, QEvent* event);
    /**
     * Clears thing-placement preview geometry when the cursor returns to the panel.
     */
    void clearThingPlacementPreviewForPanelHover(QObject* watched, QEvent* event);
    /**
     * Repositions viewport overlays after the viewport itself resizes.
     */
    void updateViewportOverlaysForEventFilterResize(QObject* watched, QEvent* event);
    /**
     * Cancels an active thing-placement panel header drag and clears its preview.
     */
    void cancelThingPlacementHeaderDrag();
    /**
     * Moves the thing-placement dock preview to the edge under the cursor.
     */
    void updateThingPlacementHeaderDragPreview(const QPoint& globalPos);
    /**
     * Commits the thing-placement panel header drag to a dock edge when possible.
     */
    void finishThingPlacementHeaderDrag(const QPoint& releaseGlobal);
    /**
     * Cancels an active UV editor panel header drag and clears its preview.
     */
    void cancelUvEditorPanelHeaderDrag();
    /**
     * Moves the UV editor dock preview to the edge under the cursor.
     */
    void updateUvEditorPanelHeaderDragPreview(const QPoint& globalPos);
    /**
     * Commits the UV editor panel header drag to a dock edge when possible.
     */
    void finishUvEditorPanelHeaderDrag(const QPoint& releaseGlobal);
    std::optional<ThingPlacementTarget> thingPlacementTargetForViewport(
        const QPoint& widgetPos,
        Qt::KeyboardModifiers modifiers) const;
    std::pair<double, double> thingPlacementBrushSampleUv(
        int index,
        int count,
        const QVector3D& center) const;
    std::optional<ThingPlacementTarget> thingPlacementBrushSampleTarget(
        const TVector& samplePoint,
        QVector3D brushNormal,
        Qt::KeyboardModifiers modifiers,
        std::int32_t preferredSector) const;
    bool stampThingPlacementBrushAt(const QPoint& widgetPos, Qt::KeyboardModifiers modifiers, bool force);
    bool finalizeThingPlacementBrushStroke();
    enum class ThingPlacementVisualKind : std::uint8_t
    {
        none,
        sprite,
        model
    };
    Thing buildThingPlacementPreviewThing(
        const QVector3D& surfacePosition,
        const QVector3D& surfaceNormal,
        Qt::KeyboardModifiers modifiers,
        std::int32_t fallbackSector = -1,
        const TVector* documentSurfacePosition = nullptr,
        bool templateCacheValidated = false) const;
    std::optional<Thing> buildFramePlacementPose(
        std::int32_t ownerThingIndex,
        const QVector3D& placementPosition,
        QVector3D surfaceNormal,
        Qt::KeyboardModifiers modifiers,
        bool haveSurfaceContact,
        const TVector* documentPlacementPosition = nullptr) const;
    std::optional<Thing> buildThingPlacementSnapThing(
        const QVector3D& surfacePosition,
        const QVector3D& surfaceNormal,
        Qt::KeyboardModifiers modifiers,
        std::int32_t fallbackSector = -1,
        const TVector* documentSurfacePosition = nullptr,
        bool templateCacheValidated = false) const;
    std::optional<Thing> snapThingPlacementPose(
        Thing baseThing,
        const QVector3D& probePosition,
        const QVector3D& surfaceNormal,
        bool rotateToSnap,
        double yawOffset,
        std::int32_t excludedThingIndex,
        bool requireInsideLevel) const;
    std::vector<QVector3D> buildThingPlacementPreviewLines(
        const QVector3D& surfacePosition,
        std::string_view thingName,
        const QVector3D& surfaceNormal,
        Qt::KeyboardModifiers modifiers,
        bool templateCacheValidated = false) const;
    std::optional<ThingBox> resolvedThingTemplateBox(std::string_view thingName) const;
    std::optional<ThingBox> resolvedPlacedThingBox(const Thing& thing) const;
    std::vector<ThingPlacementConnector> resolvedThingTemplateConnectors(std::string_view thingName) const;
    std::vector<ThingPlacementConnector> resolvedPlacedThingConnectors(const Thing& thing) const;
    void invalidateThingPlacementSnapCaches() noexcept;
    void ensureThingPlacementTemplateCache(std::string_view thingName) const;
    void ensureThingPlacementSnapAnchorCache() const;
    std::optional<TVector> thingPlacementSurfaceAlignedPyr(const QVector3D& surfaceNormal) const;
    std::optional<TVector> thingPlacementSpriteSurfaceAlignedPyr(const QVector3D& surfaceNormal) const;
    std::optional<QVector3D> thingPlacementSurfaceNormal(std::int32_t sectorIndex, std::int32_t surfaceIndex) const;
    std::optional<QVector3D> thingPlacementSpriteOffset(std::string_view thingName) const;
    ThingPlacementVisualKind thingTemplateVisualKind(std::string_view thingName) const;
    double thingPlacementGroundOffsetZ(std::string_view thingName) const;
    bool commitThingPlacementAt(
        const QVector3D& surfacePosition,
        bool haveSurfaceHit,
        const QVector3D& surfaceNormal,
        Qt::KeyboardModifiers modifiers,
        std::int32_t fallbackSector = -1,
        const QPoint* widgetPos = nullptr,
        const TVector* documentSurfacePosition = nullptr);
    bool selectedSectorShapeSupportsSides() const;
    int selectedSectorShapeSides() const noexcept;
    void setSelectedSectorShapeSides(int sides) noexcept;
    QString selectedSectorSidesLabel() const;
    void resetSelectionContextState();
    bool hasSurfaceSelectionContextForTopologyInvalidation() const noexcept;
    bool captureSurfaceSelectionTopologySnapshot(
        std::vector<std::int64_t>& snapshot) const noexcept;
    bool matchesSurfaceSelectionTopologySnapshot(
        std::span<const std::int64_t> snapshot) const noexcept;
    void invalidateSurfaceSelectionContextForTopologyEdit();
    bool hasUnsavedChanges() const noexcept;
    void refreshCleaveAuthoredDirtyCache() const noexcept;
    QByteArray currentCleaveAuthoredStateHash() const noexcept;
    void markCurrentDocumentAsSaved();
    void markRecoveryWorkspaceAsSaved();
    void markRecoveryCogWorkspaceAsSaved();
    void refreshWindowTitle();
    void updateProjectTypeStatusLabel();
    void resetLegacyUvCompatibilityStatus();
    void updateLegacyUvCompatibilityStatusLabel();
    void evaluateLegacyUvCompatibility(
        bool allowPromotion,
        bool resourceAvailabilityChanged);
    void scheduleLegacyUvPromotionRetry();
    bool promptSaveIfUnsaved(
        const QString& actionDescription,
        const std::filesystem::path& preservedRecoveryPath = {},
        QString* deferredDiscardIdentity = nullptr,
        QString* deferredDiscardSessionId = nullptr);
    void captureReplacementAuthorizationFingerprint();
    bool replacementAuthorizationStillCurrent() const;
    void clearReplacementAuthorizationFingerprint() noexcept;
    bool authorizeDocumentReplacement(
        const QString& actionDescription,
        const std::filesystem::path& preservedRecoveryPath,
        QString* deferredDiscardIdentity,
        QString* deferredDiscardSessionId);
    void openLevelFromFilename(
        const QString& filename,
        std::function<void()> beforeReplacement = {});
    void createNewProject(ProjectType kind);
    bool configureProjectInstallDir(ProjectType kind, QWidget* parent = nullptr);
    bool showProjectStartDialog();
    void frameBoundsWithNavigationPrefs(const QVector3D& min, const QVector3D& max, bool animate);
    /**
     * Queues a top-down editor refit for after Qt has finished applying show,
     * resize, and dock layout updates.
     */
    void scheduleEditorViewRefitAfterLayout();
    bool editorViewRefitViewportReady() const noexcept;
    void loadViewportPreferences();
    void applyArchThingGizmoPreferences();
    void connectViewportSignals();
    bool reloadViewportResourceArchives();
    bool currentViewportSoundMuted() const noexcept;
    void setCurrentViewportSoundMuted(bool muted);
    void syncViewportSoundControls();
    void syncThingModelLodActionAvailability();
    void updateSectorSoundPlayback();
    void resetLayerStatesFromLevel(bool rebuildViewportMeshes = true);
    void resetLayerColorsToDefaults();
    std::string buildLayerVisibilityString() const;
    void normalizeLayerVisualState();
    void persistLayerColors() const;
    void syncLayerViewState(bool rebuildViewportMeshes = true);
    using LayerVisualHistoryState = ui::layers::LayerVisualState;
    struct LayerVisualHistoryTransition
    {
        std::uint64_t recordId{};
        std::uint64_t beforeSignature{};
        std::uint64_t afterSignature{};
        LayerVisualHistoryState before{};
        LayerVisualHistoryState after{};
    };
    using LayerVisualHistoryByAfter =
        std::unordered_map<std::uint64_t, LayerVisualHistoryTransition>;
    using LayerVisualHistoryAfterByBefore =
        std::unordered_map<std::uint64_t, std::uint64_t>;
    struct CleaveApplyWorkspaceMemento
    {
        GlView::CleaveWorkspaceFamily family{GlView::CleaveWorkspaceFamily::Sector};
        std::uint64_t recordId{};
        std::uint64_t documentGeneration{};
        std::uint64_t ownedBytes{};
        GlView::RecoveryState::CleaveDraftSnapshot draft{};
        std::optional<GlView::RecoveryState::CleaveHistory> history{};
        std::optional<bool> bounded{};
        bool stagedMode{};
        bool chainEnabled{};
        bool measurementsVisible{true};
        GlView::CleaveMeasureUnit measureUnit{GlView::CleaveMeasureUnit::Meters};
        bool anglesVisible{true};
        bool lengthsVisible{true};
        bool lineNumbersVisible{};
        double angleStepDegrees{};
        GlView::RecoveryState::CleaveShapeOptions shape{};
        GlView::StagedCleaveTransformMode transformMode{
            GlView::StagedCleaveTransformMode::Move};
        bool sectorToolEnabled{};
        bool surfaceToolEnabled{};
        editor::Selection documentSelection{};
        std::int32_t activeSector{-1};
        std::int32_t activeSurfaceSector{-1};
        std::int32_t activeSurface{-1};
        std::int32_t activeThing{-1};
        std::int32_t activeLight{-1};
        std::int32_t activeFrameThing{-1};
        std::int32_t activeFrameValue{-1};
        EditMode editMode{EditMode::none};
        SurfaceSelectionMode surfaceSelectionMode{SurfaceSelectionMode::face};
        LightEditSubMode lightEditSubMode{LightEditSubMode::baked};
    };
    std::optional<CleaveApplyWorkspaceMemento>
    captureCleaveApplyWorkspaceMemento();
    std::optional<CleaveApplyWorkspaceMemento>
    prepareCleaveApplyWorkspaceRestore(std::uint64_t recordId) const;
    void recordCleaveApplyWorkspaceMemento(
        CleaveApplyWorkspaceMemento memento,
        std::uint64_t recordId);
    bool restoreCleaveApplyWorkspaceAfterUndo(
        std::uint64_t recordId,
        CleaveApplyWorkspaceMemento prepared);
    bool clearCleaveApplyWorkspaceAfterRedo(std::uint64_t recordId);
    bool restoreCleaveApplyWorkspaceAtCurrentCursor();
    bool hasPairedCleaveApplyRedo() const noexcept;
    bool cleaveApplyDocumentRedoHasPriority() const noexcept;
    std::optional<GlView::CleaveWorkspaceFamily>
    cleaveApplyWorkspaceFamilyForRecord(std::uint64_t recordId) const noexcept;
    bool canFitCleaveWorkspaceAttachmentBudget(
        std::uint64_t additionalBytes,
        bool addsTransition) const noexcept;
    void pruneCleaveApplyWorkspaceMementos();
    LayerVisualHistoryState captureLayerVisualHistoryState() const;
    std::uint64_t layerVisualHistoryTransitionOwnedBytes(
        const LayerVisualHistoryState& before,
        const LayerVisualHistoryState& after) const noexcept;
    void applyLayerVisualHistoryState(LayerVisualHistoryState state);
    void recordLayerVisualHistoryTransition(
        std::uint64_t beforeSignature,
        LayerVisualHistoryState before);
    void publishPreallocatedLayerVisualHistoryTransition(
        std::uint64_t beforeSignature,
        std::uint64_t afterSignature,
        std::uint64_t recordId,
        LayerVisualHistoryByAfter& stagedByAfter,
        LayerVisualHistoryAfterByBefore& stagedAfterByBefore);
    bool restoreLayerVisualHistoryForUndo(
        std::uint64_t fromSignature,
        std::uint64_t toSignature);
    bool restoreLayerVisualHistoryForRedo(
        std::uint64_t fromSignature,
        std::uint64_t toSignature);
    void refreshLayerVisualHistoryForCurrentSignature();
    bool isLayerLocked(std::int32_t layer) const noexcept;
    bool selectionTouchesLockedLayer(const editor::Selection& selection) const;
    bool selectionTouchesHiddenLayer(const editor::Selection& selection) const;
    bool selectionTouchesNonEditableLayer(const editor::Selection& selection) const;
    void syncViewportSelectionEditabilityIndicator();
    void initializeKeymapBindings();
    const ui::shortcuts::ModifierBindings& selectionAddModifierBindings() const noexcept
    {
        return selectionAddModifierBindings_;
    }
    const ui::shortcuts::ModifierBindings& selectionRemoveModifierBindings() const noexcept
    {
        return selectionRemoveModifierBindings_;
    }
    const ui::shortcuts::ModifierBindings& selectionShortestPathModifierBindings() const noexcept
    {
        return selectionShortestPathModifierBindings_;
    }
    const ui::shortcuts::ModifierBindings& selectionBypassSurfaceFiltersModifierBindings() const noexcept
    {
        return selectionBypassSurfaceFiltersModifierBindings_;
    }
    /**
     * Converts private editor mode state into the public selection-hint mode.
     */
    ui::shortcuts::SelectionHintMode selectionHintModeForEditMode() const noexcept;
    /**
     * Shows or hides the tracked selection-modifier status hint without
     * clearing unrelated status-bar messages.
     */
    void applySelectionModifierHint(const QString& hintText);
    void clearSelectionModifierHintIfShown();
    /**
     * Recomputes the selection-modifier hint for the current viewport/key state.
     */
    void updateSelectionModifierHintForKeyState(
        int key,
        Qt::KeyboardModifiers eventModifiers,
        bool pressed,
        bool viewportFocusContext,
        bool viewportPointerContext);
    void syncSelectionModifierBindings();
    /**
     * Applies fly-navigation key shortcuts to the viewport while preserving the
     * configured QAction shortcut order.
     */
    bool dispatchFlyNavigationShortcut(int key, Qt::KeyboardModifiers modifiers, bool pressed);
    /**
     * Applies viewport roll shortcuts after the event-filter gate accepts the key event.
     */
    bool dispatchViewportRollShortcut(int key, Qt::KeyboardModifiers modifiers, bool inViewport);
    bool dispatchEditorShortcut(int key, Qt::KeyboardModifiers modifiers);
    void rebuildHistoryView(bool force = false);
    void jumpToUndoDepth(
        std::size_t targetUndoDepth,
        bool handleCleaveWorkspaceMementos = true);
    bool goToHistoryDepth(std::size_t targetUndoDepth, bool selectTarget);
    void populateHistoryContextMenu(QMenu* menu, std::size_t targetUndoDepth);
    void logMessage(
        MessageSeverity severity,
        const QString& message,
        MessageLogCategory category = MessageLogCategory::application);
    bool messageSeverityVisible(MessageSeverity severity) const;
    void refreshMessageSeverityVisibility();
    void showMessageSeverityPopup(QWidget* anchor);
    void showPreferencesDialog(QWidget* parent = nullptr);
    void showLevelQueryDialog();
    void findNextLevelQuery();
    void copySelectedMessagesToClipboard();
    void showMessageDetails(const QTreeWidgetItem* item);
    void rebuildAddSectorShapeMenu();
    void saveSelectedSectorAsShape();
    void cycleSelectedSectorShape();
    void showUvEditorDialog();
    void setupUvEditorSurfaceActions(QMenu* surfaceMenu);
    void setupUvEditorPanel();
    void syncUvEditorPanel();
    void applyUvEditorMaximumZoomForPreferences();
    void syncUvEditorViewportFacingOrientation();
    bool cancelActiveUvEditorModalTransform();
    bool dispatchUvEditorShortcut(int key, Qt::KeyboardModifiers modifiers);
    bool forwardUvEditorShortcutIfNeeded(
        int key,
        Qt::KeyboardModifiers modifiers,
        bool autoRepeat,
        bool uvPanelVisible,
        bool uvFocusContext,
        bool uvCanvasPointerContext,
        bool textEntryFocusContext);
    void prepareUvEditorViewportZoomCompensation(const QSize& baselineViewportSize);
    void applyUvEditorViewportZoomCompensation(const QSize& targetViewportSize);
    void restoreUvEditorViewportZoomCompensation();
    void settleUvEditorPanelTransitionForGamePreview();
    void hideUvEditorPanelWithTransition();
    void setUvEditorPanelExpanded(
        bool expanded,
        bool animate = true,
        bool startFromCollapsedPresentation = false);
    void applyUvEditorDockPosition(UvEditorDockPosition position, bool preserveSize = true);
    /**
     * Converts shared panel-dock geometry results into the UV editor dock enum.
     */
    UvEditorDockPosition uvEditorDockPositionForPanelDockSide(ui::viewport::PanelDockSide dockSide) const noexcept;
    /**
     * Converts a UV editor dock enum into the shared panel-dock side.
     */
    ui::viewport::PanelDockSide uvEditorPanelDockSideForPosition(UvEditorDockPosition position) const noexcept;
    /**
     * Resolves the UV editor dock target under a global cursor point.
     */
    std::optional<UvEditorDockPosition> uvEditorDockDropTarget(const QPoint& globalPos) const;
    /**
     * Builds the UV editor docking preview rectangle in splitter-local coordinates.
     */
    QRect uvEditorDockPreviewRect(UvEditorDockPosition position) const;
    /**
     * Resolves a release-drag fallback UV dock target when the cursor is not near a splitter edge.
     */
    std::optional<UvEditorDockPosition> uvEditorDockPositionForDragDelta(const QPoint& delta) const noexcept;
    void showTemplateEditorDialog();
    static StagedProjectResourceState stageProjectResources(
        ProjectType kind,
        const std::filesystem::path& sourcePath);
    void persistStagedProjectResources(
        const StagedProjectResourceState& staged) const;
    void reportStagedProjectResources(
        const StagedProjectResourceState& staged);
    StagedGlobalTemplateState stageGlobalTemplates(const Level& level) const;
    void publishGlobalTemplates(
        StagedGlobalTemplateState&& staged,
        bool refreshViewport);
    void refreshPublishedGlobalTemplates();
    void reportStagedGlobalTemplates(
        const StagedGlobalTemplateState& staged);
    // Populate the active project/template database. Loading transactions use
    // the staged helpers above so parse/prompt failure leaves the old workspace
    // and persistent project settings untouched.
    void loadGlobalTemplates(bool refreshViewport = true);
    void loadResourceSettings(ProjectType kind, bool persistSettings = true);
    void saveResourceSettings();
    void rebuildResourceLocator(const std::filesystem::path& levelPath);
    static std::shared_ptr<const assets::ResourceLocator>
    makeResourceLocatorSnapshot(const assets::ResourceLocator& resources);
    void publishResourceLocatorSnapshot();
    void publishResourceLocatorSnapshot(
        std::shared_ptr<const assets::ResourceLocator> snapshot);
    bool navigateToEntity(
        OutlinerKind kind,
        std::int32_t index,
        std::int32_t sectorIndex,
        std::int32_t surfaceIndex,
        bool animate,
        bool preserveCogContext = false);

    GlView* glView_{};
    QMainWindow* workspaceWindow_{};
    QSplitter* viewportUvSplitter_{};
    QWidget* viewportOverlay_{};
    QScrollArea* viewportMapSettingsScrollArea_{};
    QMenu* viewportMapSettingsMenu_{};
    QToolButton* viewportMapSettingsButton_{};
    QMenu* viewportThingRenderingMenu_{};
    QWidget* addSectorOptionsOverlay_{};
    QWidget* transformMoveOptionsOverlay_{};
    QWidget* lightBakeOverlay_{};
    QWidget* sectorJoinOptionsOverlay_{};
    QWidget* sectorThrustOptionsOverlay_{};
    QWidget* cleaveOptionsOverlay_{};
    ui::viewport::CleaveShapePopover* cleaveShapePopover_{};
    QWidget* thingMoveOptionsOverlay_{};
    QWidget* viewportThingMoveSpecificOptionsSeparator_{};
    QWidget* thingPlacementPanel_{};
    QWidget* thingPlacementHeader_{};
    QWidget* thingPlacementCollapsedBar_{};
    QWidget* thingPlacementSelectedPreviewHost_{};
    QWidget* thingPlacementBrushOptions_{};
    QWidget* viewportTransformOverlay_{};
    QWidget* viewportToolsOverlay_{};
    QWidget* viewportEditModeOverlay_{};
    QToolButton* viewportSelectionEditabilityBadge_{};
    QWidget* uvEditorPanelHost_{};
    QWidget* uvEditorPanelHeader_{};
    QWidget* uvEditorPanelBody_{};
    QWidget* uvEditorPanelWidget_{};
    QToolButton* lightBakeRecalculateButton_{};
    QAction* lightBakeRecalculateAllAction_{};
    QAction* lightBakeRecalculateSelectedAction_{};
    QToolButton* lightBakeVisibleOnlyButton_{};
    QToolButton* lightBakeUpdateAmbientButton_{};
    QToolButton* lightBakeAutoButton_{};
    QToolButton* lightBakeStatusButton_{};
    QDialog* lightBakeProgressDialog_{};
    QLabel* lightBakeProgressLabel_{};
    QProgressBar* lightBakeProgressBar_{};
    QPushButton* lightBakeStopButton_{};
    QTimer* lightBakePollTimer_{};
    QTimer* lightBakeDebounceTimer_{};
    QTimer* lightBakeTransformPreviewTimer_{};
    QObject* lightBakeWatcher_{};
    std::function<void()> lightBakeWaitForFinished_{};
    std::shared_ptr<std::atomic_bool> lightBakeCancelFlag_{};
    QObject* lightBakeTransformPreviewWatcher_{};
    std::function<void()> lightBakeTransformPreviewWaitForFinished_{};
    std::shared_ptr<std::atomic_bool> lightBakeTransformPreviewCancelFlag_{};
    std::uint64_t lightBakeSourceGeneration_{};
    std::uint64_t lightBakeJobSerial_{};
    std::uint64_t lightBakeTransformPreviewJobSerial_{};
    bool lightBakeFingerprintInitialized_{};
    std::vector<SedLight> lightBakeObservedLights_{};
    std::vector<std::int32_t> lightBakePendingAutoTargets_{};
    std::vector<std::int32_t> lightBakeActiveTargets_{};
    std::vector<level::LightingInfluenceSphere> lightBakePendingAutoInfluences_{};
    std::vector<std::int32_t> lightBakePendingManualTargets_{};
    std::vector<std::int32_t> lightBakePendingManualPermittedTargets_{};
    std::vector<level::LightingInfluenceSphere> lightBakePendingManualInfluences_{};
    std::string lightBakePendingManualUndoLabel_{};
    bool lightBakePendingManualUpdateAmbient_{};
    std::vector<level::LightingInfluenceSphere> lightBakeActiveInfluences_{};
    std::shared_ptr<const Level> lightBakeTransformPreviewBaseline_{};
    std::vector<std::int32_t> lightBakeTransformPreviewSelection_{};
    std::vector<std::int32_t> lightBakeTransformPreviewTargets_{};
    std::vector<level::LightingInfluenceSphere> lightBakeTransformPreviewInfluences_{};
    bool lightBakeTransformPreviewUpdatedAmbient_{};
    bool lightBakeTransformPreviewArchApplied_{};
    assets::Templates lightBakeTransformPreviewTemplates_{};
    std::shared_ptr<const assets::ResourceLocator>
        lightBakeTransformPreviewResourceSnapshot_{};
    std::optional<Level> lightBakeTransformCommittedPreview_{};
    std::vector<std::int32_t> lightBakeTransformCommittedPreviewTargets_{};
    std::vector<level::LightingInfluenceSphere> lightBakeTransformCommittedPreviewInfluences_{};
    bool lightBakeTransformCommittedPreviewUpdatedAmbient_{};
    bool lightBakeTransformCommittedPreviewArchApplied_{};
    bool lightBakeRunning_{};
    bool lightBakeTransformPreviewWorkerRunning_{};
    bool lightBakeTransformPreviewPending_{};
    bool lightBakePendingAuto_{};
    bool lightBakePendingManual_{};
    bool lightBakePendingManualSelectedOnly_{};
    bool lightBakeRecalculateSelectedDefault_{};
    QToolButton* uvEditorPanelToggleButton_{};
    QToolButton* uvEditorPanelDockButton_{};
    QVariantAnimation* uvEditorPanelAnimation_{};
    QWidget* viewportEditModeSurfacePanel_{};
    QWidget* viewportEditModeThingPanel_{};
    QWidget* viewportEditModeLightPanel_{};
    QToolButton* viewportEditLightArchitecturalButton_{};
    QToolButton* viewportEditLightSectorButton_{};
    QToolButton* viewportEditSectorButton_{};
    QToolButton* viewportEditSurfaceButton_{};
    QToolButton* viewportEditThingButton_{};
    QToolButton* viewportEditLightButton_{};
    QToolButton* viewportCameraButton_{};
    QToolButton* addSectorShapeButton_{};
    QLineEdit* thingPlacementSearchEdit_{};
    QWidget* thingPlacementFilterHost_{};
    QWidget* thingPlacementFilterContent_{};
    QWidget* thingPlacementFilterResizeGrip_{};
    QToolButton* thingPlacementFilterToggleButton_{};
    QListWidget* thingPlacementCategoryList_{};
    QListWidget* thingPlacementTemplateList_{};
    QWidget* thingPlacementSelectedPreviewWidget_{};
    QLabel* thingPlacementSelectedPreviewImage_{};
    QLabel* thingPlacementSelectedPreviewName_{};
    QLabel* thingPlacementSummaryLabel_{};
    QToolButton* thingPlacementDockButton_{};
    QToolButton* thingPlacementHideButton_{};
    QToolButton* thingPlacementShowButton_{};
    QCheckBox* thingPlacementKeepActiveCheck_{};
    QCheckBox* thingPlacementSnapToThingCheck_{};
    QCheckBox* thingPlacementSnapRotateCheck_{};
    QToolButton* thingPlacementModeSurfaceButton_{};
    QToolButton* thingPlacementModeTiltButton_{};
    QToolButton* thingPlacementModeFreeButton_{};
    QComboBox* thingPlacementLayerCombo_{};
    QToolButton* thingPlacementToolSingleButton_{};
    QToolButton* thingPlacementToolBrushButton_{};
    QDoubleSpinBox* thingPlacementBrushSizeSpin_{};
    QSpinBox* thingPlacementBrushCountSpin_{};
    QComboBox* thingPlacementBrushShapeCombo_{};
    QCheckBox* thingPlacementBrushRandomRotateCheck_{};
    QCheckBox* thingPlacementBrushRotatePitchCheck_{};
    QCheckBox* thingPlacementBrushRotateYawCheck_{};
    QCheckBox* thingPlacementBrushRotateRollCheck_{};
    QDoubleSpinBox* thingPlacementBrushFixedAngleSpin_{};
    QDoubleSpinBox* thingPlacementBrushSpacingSpin_{};
    QDoubleSpinBox* thingPlacementBrushScatterSpin_{};
    QCheckBox* thingPlacementBrushAllowAirCheck_{};
    QRubberBand* thingPlacementDockPreviewBand_{};
    QVariantAnimation* thingPlacementPanelAnimation_{};
    QVariantAnimation* thingPlacementCollapsedBarAnimation_{};
    QToolButton* viewportRenderModeButton_{};
    QToolButton* viewportLightingModeButton_{};
    QToolButton* viewportThingModeButton_{};
    QToolButton* viewportTransformOrientationButton_{};
    QToolButton* addSectorGridSnapButton_{};
    QToolButton* viewportTransformGridSnapButton_{};
    QToolButton* viewportTransformGridStickButton_{};
    QToolButton* viewportThingMoveGridSnapButton_{};
    QToolButton* viewportThingMoveGridStickButton_{};
    QMenu* viewportCameraMenu_{};
    QComboBox* viewportCameraProjectionCombo_{};
    QSlider* viewportCameraFovSlider_{};
    QLineEdit* viewportCameraFovEdit_{};
    QDoubleSpinBox* viewportCameraNearSpin_{};
    QDoubleSpinBox* viewportCameraFarSpin_{};
    QCheckBox* viewportCameraClipCheck_{};
    QActionGroup* viewportTransformToolGroup_{};
    QAction* viewportTransformSelectAction_{};
    QAction* viewportTransformMoveAction_{};
    QAction* viewportTransformRotateAction_{};
    QAction* viewportTransformScaleAction_{};
    QAction* rememberedViewportTransformAction_{};
    QAction* viewportTransformGridSnapAction_{};
    QAction* viewportTransformGridStickAction_{};
    QActionGroup* viewportTransformOrientationGroup_{};
    QActionGroup* addSectorPlaneAxisGroup_{};
    QAction* addSectorPlaneXAction_{};
    QAction* addSectorPlaneYAction_{};
    QAction* addSectorPlaneZAction_{};
    QAction* addSectorAtViewAction_{};
    QAction* addSectorFreeViewAction_{};
    QLabel* addSectorSidesLabel_{};
    QSpinBox* addSectorSidesSpin_{};
    QAction* viewportTransformOrientationLocalAction_{};
    QAction* viewportTransformOrientationWorldAction_{};
    QAction* thingToolSelectKeyAction_{};
    QAction* thingToolMoveKeyAction_{};
    QAction* thingToolRotateKeyAction_{};
    QAction* thingToolScaleKeyAction_{};
    QAction* thingToolToggleOrientationKeyAction_{};
    QAction* editModeSectorKeyAction_{};
    QAction* editModeSurfaceKeyAction_{};
    QAction* editModeThingKeyAction_{};
    QAction* editModeLightKeyAction_{};
    QAction* sectorCycleAddModeKeyAction_{};
    QAction* sectorCleaveKeyAction_{};
    QAction* sectorJoinKeyAction_{};
    QAction* sectorMergeKeyAction_{};
    QAction* surfaceMakeAdjoinKeyAction_{};
    QAction* surfaceUnadjoinKeyAction_{};
    QAction* surfaceCycleSubmodeKeyAction_{};
    QAction* surfaceCleaveKeyAction_{};
    QAction* surfaceMergeKeyAction_{};
    QAction* surfaceExtrudeKeyAction_{};
    QAction* surfaceConnectKeyAction_{};
    QAction* surfaceFaceModeKeyAction_{};
    QAction* surfaceEdgeModeKeyAction_{};
    QAction* surfaceVertexModeKeyAction_{};
    QAction* thingToggleFrameModeKeyAction_{};
    QAction* transformGridSnapInvertModifierAction_{};
    QAction* transformGridStickInvertModifierAction_{};
    QAction* sectorMoveSnapAlignInvertModifierAction_{};
    QAction* surfaceGeometrySnapInvertModifierAction_{};
    QAction* thingMoveSurfaceStickInvertModifierAction_{};
    QAction* thingMoveSurfaceAlignInvertModifierAction_{};
    QAction* thingMoveSnapInvertModifierAction_{};
    QAction* thingMoveSnapRotateInvertModifierAction_{};
    QActionGroup* viewportEditToolsGroup_{};
    QAction* viewportSectorCleaveAction_{};
    QAction* viewportSectorConnectAction_{};
    QAction* viewportSectorMergeAction_{};
    QAction* viewportSectorThrustAction_{};
    QAction* viewportSurfaceCleaveAction_{};
    QAction* viewportSurfaceMergeAction_{};
    QAction* viewportFrameAddAction_{};
    QAction* viewportFrameDeleteAction_{};

    QToolButton* viewportSectorCleaveButton_{};
    QToolButton* viewportSectorConnectButton_{};
    QToolButton* viewportSectorMergeButton_{};
    QToolButton* viewportSectorThrustButton_{};
    QToolButton* viewportSectorThrustShowAllButton_{};
    QToolButton* viewportCleaveStagedButton_{};
    QToolButton* viewportCleaveAddShapeButton_{};
    QToolButton* viewportCleaveMoveButton_{};
    QToolButton* viewportCleaveRotateButton_{};
    QToolButton* viewportCleaveScaleButton_{};
    QToolButton* viewportCleaveGridSnapButton_{};
    QToolButton* viewportCleaveChainButton_{};
    QToolButton* viewportCleaveBoundedButton_{};
    QToolButton* viewportCleaveShowAnglesButton_{};
    QToolButton* viewportCleaveShowLengthsButton_{};
    QToolButton* viewportCleaveShowNumbersButton_{};
    QComboBox* viewportCleaveUnitsCombo_{};
    ui::ScrubFloatEdit* viewportCleaveAngleStepSpin_{};
    QToolButton* viewportCleaveApplyButton_{};
    QToolButton* viewportCleaveCancelButton_{};
    QToolButton* viewportSurfaceCleaveButton_{};
    QToolButton* viewportSurfaceMergeButton_{};
    QToolButton* viewportFrameAddButton_{};
    QToolButton* viewportFrameDeleteButton_{};
    QToolButton* viewportThingMoveStickButton_{};
    QToolButton* viewportThingMoveSurfaceAlignButton_{};
    QToolButton* viewportThingMoveSnapButton_{};
    QToolButton* viewportThingMoveSnapRotateButton_{};
    QLabel* viewportTransformMoveOptionsLabel_{};
    QLabel* viewportThingMoveOptionsLabel_{};
    QToolButton* viewportSectorMoveSnapAlignButton_{};
    QToolButton* viewportSurfaceGeometrySnapButton_{};
    QToolButton* viewportSurfaceGeometryRotateButton_{};
    QToolButton* viewportPreserveConnectedSurfaceUvsButton_{};
    QToolButton* viewportTransformSurfaceStrictButton_{};
    QToolButton* viewportSectorJoinKeepPiecesButton_{};
    QToolButton* sectorJoinApplyButton_{};
    QToolButton* sectorJoinCancelButton_{};
    QWidget* surfaceExtrudeOptionsOverlay_{};
    ui::ScrubFloatEdit* surfaceExtrudeLengthSpin_{};
    ui::ScrubFloatEdit* surfaceExtrudeExpandSpin_{};
    QToolButton* surfaceExtrudeGridSnapButton_{};
    QToolButton* surfaceExtrudeApplyButton_{};
    QToolButton* surfaceExtrudeCancelButton_{};
    QToolButton* viewportSurfaceExtrudeButton_{};
    QToolButton* viewportSurfaceFlipButton_{};
    QToolButton* viewportSurfaceMakeAdjoinButton_{};
    QToolButton* viewportSurfaceUnadjoinButton_{};
    QToolButton* viewportSurfaceConnectSurfacesButton_{};
    QToolButton* viewportCommonDeleteButton_{};
    QToolButton* viewportCommonAddCubeButton_{};
    QToolButton* viewportCommonAddThingButton_{};
    QToolButton* viewportCommonAddLightButton_{};
    QToolButton* viewportCommonAddThingLightButton_{};
    QToolButton* viewportCommonAddSectorLightButton_{};
    QMenu* addSectorShapeMenu_{};
    QActionGroup* addSectorShapeGroup_{};
    assets::ResourceLocator resources_{};
    bool resourceArchiveReloadWarningShown_{};
    // Thumbnail preparation runs off-thread. Each published resource
    // generation gets an immutable locator copy that remains alive until all
    // workers/renderers using that generation release it.
    std::shared_ptr<const assets::ResourceLocator> resourceLocatorSnapshot_{};
    std::vector<std::filesystem::path> resourceRoots_{};
    std::vector<std::filesystem::path> resourceGobs_{};
    ProjectType resourceKind_{ProjectType::JKDF2};
    assets::Templates templates_{};
    QTreeWidget* outliner_{};
    QTabBar* outlinerTabs_{};
    QToolButton* outlinerShowHiddenLayersButton_{};
    QLabel* outlinerCountLabel_{};
    QWidget* cogOutlinerSearchRow_{};
    QLineEdit* cogOutlinerSearchEdit_{};
    int levelQueryEntityKind_{1};
    struct SavedLevelQueryState
    {
        bool hasState{};
        int field{2};
        int matchMode{};
        bool caseSensitive{};
        std::string value{};
        std::vector<int> criteriaFields{};
        std::vector<int> criteriaMatchModes{};
        std::vector<std::string> criteriaValues{};
        int resultMode{};
        bool selectionOnly{};
    };
    std::array<SavedLevelQueryState, 3> levelQueryStates_{};
    QTreeWidget* properties_{};
    QWidget* thingParametersPanel_{};
    QTreeWidget* thingParameters_{};
    QLineEdit* thingParameterSearch_{};
    QWidget* cogLinksPanel_{};
    QListWidget* cogLinksList_{};
    QDockWidget* outlinerDock_{};
    QDockWidget* cogScriptEditorDock_{};
    QTabWidget* cogScriptEditorTabs_{};
    std::vector<CogScriptEditorTabHost> cogScriptEditorTabHosts_{};
    QPointer<QTabWidget> preferredCogScriptEditorHost_{};
    QPointer<QTabWidget> lastActiveCogScriptEditorHost_{};
    QDockWidget* resourceEditorsDock_{};
    QTabWidget* resourceEditorTabs_{};
    std::vector<ResourceEditorTabHost> resourceEditorTabHosts_{};
    QTabWidget* checksTabs_{};
    QToolButton* consistencyCheckSettingsButton_{};
    QToolButton* resourceCheckSettingsButton_{};
    QTreeWidget* consistencyChecks_{};
    QTreeWidget* resourceChecks_{};
    std::unordered_map<int, std::vector<QTreeWidgetItem*>> consistencyThingCheckRows_{};
    std::unordered_map<int, std::vector<QTreeWidgetItem*>> resourceThingCheckRows_{};
    std::uint64_t thingCheckRowsVisited_{};
    QTreeWidget* messages_{};
    QTreeWidget* history_{};
    QLabel* projectTypeStatusLabel_{};
    QLabel* legacyUvStatusLabel_{};
    QTimer* checksRebuildTimer_{};
    QTimer* editorViewRefitTimer_{};
    QTimer* sectorSoundTimer_{};
    QFutureWatcher<QByteArray>* sectorSoundLoadWatcher_{};
    std::int32_t viewportSoundSector_{-1};
    QVector3D viewportSoundEye_{};
    bool viewportSoundEyeValid_{};
    std::uint64_t viewportSoundDocumentEpoch_{};
    std::string viewportSoundName_{};
    std::string viewportSoundResourceKey_{};
    std::string viewportPendingSoundResourceKey_{};
    std::string viewportFailedSoundResourceKey_{};
    double viewportSoundVolume_{};
    double viewportPendingSoundVolume_{};
    bool viewportSoundPlaybackCurrent_{};
    bool viewportSoundLoadPending_{};
    std::uint64_t viewportSoundLoadGeneration_{};
    std::uint64_t viewportPendingSoundLoadGeneration_{};
    std::shared_ptr<const assets::ResourceLocator> viewportSoundLocatorGeneration_{};
    QTimer* recoveryTimer_{};
    QTimer* recoveryDebounceTimer_{};
    QTimer* recoveryRetryTimer_{};
    bool recoveryCaptureDeferredForFly_{};
    // A dirty document can still have a cleanly published recovery baseline.
    // Keep authored work that actually had a pending debounce separate from
    // editor-only fly changes; testing hasUnsavedChanges() on fly release
    // would otherwise launch a full Level+HIST encode after every camera
    // gesture in a dirty document.
    bool recoveryAuthoredCaptureDeferredForFly_{};
    QThreadPool* recoveryThreadPool_{};
    QFutureWatcher<ui::recovery::WriteResult>* recoveryWriteWatcher_{};
    QSize editorViewRefitLastViewportSize_{};
    bool windowPlacementRestoreDeferred_{};
    bool startupRecoveryEditorStateRestored_{};
    std::filesystem::path currentPath_{};
    // The open document may be normalized in memory while its associated
    // native path still contains a legacy pixel-space document (V1 or V4).
    // Keep this source-format fact independent from the live Level UV-space
    // tag so Save can never replace it with its normalized counterpart
    // (V2 or V3) without an explicit choice.
    bool nativeSourceUsesLegacyUvFormat_{};
    std::optional<bool> legacyUvPromotionReady_{};
    std::string legacyUvBlockedMaterial_{};
    QString legacyUvCompatibilityDetail_{};
    bool legacyUvPromotionRetryScheduled_{};
    bool legacyUvPromotionEvaluationInFlight_{};
    bool legacyUvPromotionRetryPending_{};
    std::uint64_t legacyUvResourceGeneration_{};
    std::uint64_t legacyUvPromotionEvaluationGeneration_{};
    std::size_t legacyUvPromotionEvaluationCount_{};
#ifdef SED_APP_MAIN_WINDOW_TEST_ACCESS
    std::function<void()> legacyUvPromotionPreparedHookForTest_{};
#endif
    // Creation instant of the most recently loaded or atomically published
    // native SED bytes. Every Save creates a new file image and replaces it.
    std::int64_t nativeDocumentTimestamp_{};
    // Native .sed ownership and runtime export ownership are deliberately
    // independent. Ctrl+S updates currentPath_; Export updates this sidecar and
    // EDIT persists it for manual SED, autosave, and crash recovery.
    std::filesystem::path lastEngineExportPath_{};
    // Last accepted JKL/NDY export presentation choices.  A fresh editor uses
    // the compact runtime representation and omits editor-only optional
    // sections.  Accepted choices remain independent from native SED save
    // state and are reused by repeat Export during this session.
    bool engineExportIncludeLights_{false};
    bool engineExportIncludeLayers_{false};
    bool engineExportCompact_{true};
    bool engineExportClampExtraLight_{true};
    bool engineExportOmitDefaultMotsArchLighting_{false};
    bool engineExportWeldNearbyVertices_{false};
    double engineExportVertexWeldDistance_{0.00001};
    std::uint64_t savedUndoSignature_{};
    std::shared_ptr<ui::recovery::PublicationGate> recoveryPublicationGate_{};
    std::filesystem::path recoveryDirectoryOverride_{};
    std::filesystem::path currentRecoveryPath_{};
    std::filesystem::path recoveryPendingRetirementPath_{};
    std::optional<ui::recovery::RecoveryMetadata>
        recoveryPendingRetirementMetadata_{};
    QString recoverySessionId_{};
    QString recoveryIdentity_{};
    // Explicit startup Discard applies to one selected editing session on
    // disk, but all older live sessions for that document must stay quiet for
    // the remainder of this editor run. They remain available from Autosave
    // History and are offered again after a future process start.
    QStringList recoveryOfferSuppressedIdentitiesThisRun_{};
    qint64 recoverySourceModifiedAtMs_{-1};
    QString recoveryInFlightIdentity_{};
    // Completion bookkeeping is bound to the exact writer request. A writer
    // can commit just before Save As/document replacement and deliver its Qt
    // callback afterwards, so the gate generation is paired with the current
    // document epoch and physical destination path.
    std::uint64_t recoveryDocumentEpoch_{};
    // Native Save/Save As rotates recovery publication but retains the same
    // installed Document and its concrete History RecordIDs. Workspace
    // attachments bind to this lifetime generation instead of the recovery
    // publication epoch.
    std::uint64_t documentLifetimeGeneration_{};
    std::uint64_t recoveryInFlightDocumentEpoch_{};
    std::uint64_t recoveryInFlightPublicationGeneration_{};
    std::filesystem::path recoveryInFlightPath_{};
    std::uint64_t recoveryInFlightUndoSignature_{};
    std::uint64_t recoveryInFlightRedoSignature_{};
    std::uint64_t recoveryInFlightEventSerial_{};
    std::uint64_t recoveryInFlightSemanticMutationSerial_{};
    bool recoveryInFlightForced_{};
    bool recoveryInFlightArchivePreviousSnapshot_{};
    std::filesystem::path recoveryInFlightArchiveSourcePath_{};
    QByteArray recoveryInFlightEditorStateHash_{};
    QByteArray recoveryInFlightLevelStateHash_{};
    QString recoveryLastPublishedIdentity_{};
    std::uint64_t recoveryLastPublishedDocumentEpoch_{};
    std::filesystem::path recoveryLastPublishedPath_{};
    std::uint64_t recoveryLastPublishedUndoSignature_{};
    std::uint64_t recoveryLastPublishedRedoSignature_{};
    std::uint64_t recoveryLastPublishedEventSerial_{};
    std::uint64_t recoveryLastPublishedSemanticMutationSerial_{};
    std::uint64_t recoveryEventSerial_{};
    std::uint64_t recoverySemanticMutationSerial_{};
    // The latest history timeline already accounted for by the semantic
    // recovery serial. updateEditActions() is called by both mutations and
    // ordinary UI refreshes; retaining this pair prevents repeated refreshes
    // (and the Undo/Redo refresh that follows an explicit notification) from
    // manufacturing duplicate recovery generations.
    std::uint64_t recoveryLastNotedSemanticUndoSignature_{};
    std::uint64_t recoveryLastNotedSemanticRedoSignature_{};
    bool recoveryLastNotedSemanticHistoryValid_{};
    QByteArray recoveryLastPublishedEditorStateHash_{};
    QByteArray recoveryLastPublishedLevelStateHash_{};
    // Canonical authored-data baselines at the last successful manual
    // Save/Open/New. Editor-only workspace changes are live-checkpointed but
    // deliberately do not make the document dirty or offer-eligible.
    QByteArray recoverySavedLevelStateHash_{};
    QByteArray recoverySavedCleaveAuthoredHash_{};
    mutable QByteArray recoveryCurrentCleaveAuthoredHash_{};
    mutable QString recoveryEditorStateCaptureError_{};
    mutable std::uint64_t recoveryCleaveAuthoredRevision_{};
    mutable bool recoveryCleaveAuthoredCacheValid_{};
    mutable bool recoveryCleaveAuthoredDirty_{};
    bool recoverySavedCleaveAuthoredHashKnown_{};
    // False means authored Level equality could not be established (for
    // example a bounded serialization failure). Replacement/close paths are
    // conservative in that state; generic test windows without recovery
    // enabled remain unaffected.
    bool recoverySavedLevelHashKnown_{};
    // Only modified COG buffer identity/content participates here. Keep the
    // canonical state as well as its digest so saving one external COG can
    // retire only that buffer without blessing unrelated modified tabs.
    QByteArray recoverySavedCogState_{};
    QByteArray recoverySavedCogStateHash_{};
    bool recoveryLastPublishedValid_{};
    // Production publishes an initial clean checkpoint so a hard termination
    // always has a coherent document to recover.  Main-window integration
    // tests construct many generic windows; they opt into this asynchronous
    // baseline only through the recovery-specific test helpers below.
#ifdef SED_APP_MAIN_WINDOW_TEST_ACCESS
    bool recoveryAutomaticBaselineEnabled_{};
    bool recoveryTestMutationPending_{};
    mutable std::uint64_t recoveryEditorStateCaptureCountForTest_{};
    std::function<void()> directSedSalvageBeforeRereadForTest_{};
    std::function<void()> directSedAfterLoadForTest_{};
    // Runs after an ordinary Open has finished its potentially slow UI and
    // recovery finalization, but before the deferred viewport fit is armed.
    // This lets the regression harness pump the event loop at the exact load
    // boundary which used to consume the fit too early.
    std::function<void()> directOpenBeforeViewportRefitForTest_{};
    std::function<void()> cogSaveCallbackForTest_{};
    std::function<void()> cogDiscardCallbackForTest_{};
    // Invoked by the native-save regression harness after the immutable
    // Level/EDIT/HIST snapshot has been captured but before the worker is
    // allowed to publish it.  Production builds never install this hook.
    std::function<void()> nativeSaveAfterCaptureForTest_{};
    // Invoked by the engine-export regression harness after the immutable
    // Level and option snapshot has been captured but before the worker is
    // allowed to publish it.  Production builds never install this hook.
    std::function<void()> engineExportAfterCaptureForTest_{};
    std::function<bool()> recoveryApplyBeforeInstallForTest_{};
    std::function<bool()> recoveryRetainCorruptedCopyForTest_{};
    std::function<bool()> recoveryArchivePublicationAllowedForTest_{};
    std::function<bool()> recoveryLiveCommitAllowedForTest_{};
    std::vector<QPointer<QObject>> recoverySalvageWatchersForTest_{};
#else
    bool recoveryAutomaticBaselineEnabled_{true};
#endif
    // Replacement prompts must save the document even when focus is inside a
    // COG editor.  Ordinary Ctrl+S is claimed by the focus-aware event-filter
    // route and saves only the COG buffer; this guard is raised only around
    // the prompt-triggered native-document QAction.
    bool forceNativeDocumentSave_{};
    // A published checkpoint is required even for a clean document.  This is
    // deliberately separate from recoveryRequiresSave_: a clean baseline must
    // not put a '*' in the window title, but it still protects against a hard
    // process kill between edits.
    bool recoverySnapshotRequired_{};
    bool recoverySnapshotQueued_{};
    bool recoveryForcedSnapshotQueued_{};
    // Resource draft edits participate in the native document's authored
    // dirty state without scanning or re-encoding every open resource table
    // from title, focus, or viewport hot paths.  Resource pane callbacks
    // advance the current generation; a successful native Save records the
    // separate baseline generation while leaving each pane's external-file
    // modified flag untouched.
    std::uint64_t resourceEditorAuthoredGeneration_{};
    mutable std::uint64_t recoverySavedResourceEditorGeneration_{};
    // This generation tracks installed Level/resource-locator contexts. It is
    // intentionally separate from recoveryDocumentEpoch_, which also changes
    // after a successful native Save to rotate recovery publication slots.
    std::uint64_t resourceEditorDocumentContextGeneration_{};
    // A semantic/same-undo mutation was observed and must be represented as
    // user-recoverable metadata even when the ordinary undo dirty marker has
    // not changed yet. It is consumed only after a matching worker publish.
    mutable bool recoveryAuthoredMutationPending_{};
    // Cheap authored-dirty evidence retained until an exact saved-baseline
    // check (or a new document/save) proves that the Level returned clean.
    mutable bool recoveryAuthoredLevelDirty_{};
    bool recoveryInFlightAuthoredMutationPending_{};
    bool recoveryRetryForceCapture_{};
    bool recoveryImmediateSnapshotPending_{};
    bool recoveryArchivePreviousOnNextSnapshot_{};
    std::filesystem::path recoveryArchiveSourcePath_{};
    // A shutdown/save flush drains queued successors synchronously so a
    // generation posted by the writer callback cannot hide behind a zero-time
    // event after the caller has returned.
    bool recoveryFlushDraining_{};
    bool recoveryRequiresSave_{};
    bool recoveryShuttingDown_{};
    bool recoverySystemShutdown_{};
    bool recoveryPublicationPaused_{};
    bool recoveryFailureReported_{};
    // A matching recovery selected before the current document's Save prompt
    // must remain available until the subsequent Open/Recover commits.
    std::filesystem::path recoveryProtectedPathDuringSave_{};
    // Fingerprints captured after replacement authorization.  Loading/recovery
    // pumps non-user Qt events; if authored data changes before the install
    // boundary, the replacement is aborted instead of overwriting it.
    QByteArray replacementAuthorizationLevelHash_{};
    QByteArray replacementAuthorizationCleaveHash_{};
    QByteArray replacementAuthorizationCogHash_{};
    QByteArray replacementAuthorizationResourceHash_{};
    bool replacementAuthorizationFingerprintValid_{};
    QByteArray recoverySavedResourceEditorState_{};
    QByteArray recoverySavedResourceEditorStateHash_{};
    int recoveryRetryAttempt_{};
    std::uint64_t recoveryPendingRetirementDocumentEpoch_{};
    std::uint64_t recoveryPendingRetirementPublicationGeneration_{};
    std::filesystem::path recoveryPendingRetirementCurrentPath_{};
    editor::Document document_{};
    std::vector<bool> layerVisible_{};
    std::vector<bool> layerLocked_{};
    std::vector<QColor> layerColors_{};
    LayerVisualHistoryByAfter layerVisualHistoryByAfterSignature_{};
    LayerVisualHistoryAfterByBefore layerVisualHistoryAfterByBeforeSignature_{};
    std::vector<CleaveApplyWorkspaceMemento> cleaveApplyWorkspaceMementos_{};
    std::optional<std::uint64_t> pendingCleaveApplyRedoRecordId_{};
    GlView::CleaveWorkspaceFamily pendingCleaveApplyRedoFamily_{
        GlView::CleaveWorkspaceFamily::Sector};
    std::uint64_t pendingCleaveApplyRedoAuthoredRevision_{};
    bool outlinerColorByLayer_{true};
    bool outlinerShowHiddenLayerEntities_{};
    bool wireframeColorByLayer_{true};
    bool previewRespectHiddenLayers_{};
    editor::Selection lastSectorSelection_{};
    editor::Selection lastSurfaceSelection_{};
    std::unordered_set<SurfaceRef, editor::SurfaceRefHash, editor::SurfaceRefEq>
        rememberedSurfaceFaceSelection_{};
    std::unordered_set<SurfaceRef, editor::SurfaceRefHash, editor::SurfaceRefEq>
        strictSurfaceSelectionScope_{};
    editor::Selection lastThingSelection_{};
    editor::Selection lastLightSelection_{};
    editor::Selection lastLightThingSelection_{};
    editor::Selection lastLightSectorSelection_{};

    EditMode editMode_{EditMode::none};
    SurfaceSelectionMode surfaceSelectionMode_{SurfaceSelectionMode::face};
    LightEditSubMode lightEditSubMode_{LightEditSubMode::baked};
    ArchitecturalLightingInteractiveState architecturalLightingInteractive_{};
    QPointer<QColorDialog> architecturalLightingColorDialog_{};
    QPointer<QDialog> architecturalLightingVertexDialog_{};
    // Recovery applies a fully validated Editor workspace before publishing
    // its saved Editor/Game Preview pipeline.  This narrowly bypasses the
    // user-input Preview guard for those internal mode setters only.
    bool recoveryEditorModeRestoreInProgress_{};
    // Recovery publishes serialized viewport categories through GlView's
    // viewportStateChanged signal.  Keep the normal settings synchronizer
    // from replacing those values with machine preferences during that
    // publication; the persisted preferences remain untouched.
    bool recoveryViewportStateRestoreInProgress_{};
    std::array<bool, 4> alwaysShowLightGizmoCategories_{};
    PlacementMode placementMode_{PlacementMode::none};
    ThingLightPlacementKind thingLightPlacementKind_{ThingLightPlacementKind::regular};
    std::int32_t thingLightPlacementHoverThing_{-1};
    std::int32_t sectorLightPlacementHoverSector_{-1};
    std::string selectedSectorShapeName_{"Cube"};
    SectorDrawStage sectorDrawStage_{SectorDrawStage::none};
    SectorDrawPlaneAxis sectorDrawPlaneAxis_{SectorDrawPlaneAxis::z};
    bool sectorDrawAtCameraView_{};
    bool sectorDrawFreeView_{};
    TVector sectorDrawAnchor_{};
    TVector sectorDrawFootprint_{};
    QPoint sectorDrawAnchorWidgetPos_{};
    QPoint sectorDrawHeightWidgetPos_{};
    SectorDrawBox sectorDrawLastBox_{};
    render::TransformGridFrame sectorDrawGridFrame_{};
    TVector sectorDrawFreeAxisA_{1.0f, 0.0f, 0.0f};
    TVector sectorDrawFreeAxisB_{0.0f, 0.0f, -1.0f};
    TVector sectorDrawFreeAxisHeight_{0.0f, 1.0f, 0.0f};
    int addSectorCylinderSides_{16};
    int addSectorConeSides_{16};
    int addSectorUvSphereSegments_{8};
    std::string selectedThingTemplateName_{};
    bool thingPlacementPanelVisible_{true};
    bool thingPlacementPanelCollapsePending_{};
    bool thingPlacementFiltersVisible_{true};
    int thingPlacementFilterContentHeight_{92};
    ThingPlacementDockPosition thingPlacementDockPosition_{ThingPlacementDockPosition::right};
    ThingPlacementDefaultMode thingPlacementDefaultMode_{ThingPlacementDefaultMode::surface};
    ThingPlacementToolMode thingPlacementToolMode_{ThingPlacementToolMode::single};
    ThingPlacementBrushShape thingPlacementBrushShape_{ThingPlacementBrushShape::circle};
    std::int32_t thingPlacementLayerOverride_{-1};
    bool thingPlacementKeepActive_{};
    bool thingPlacementSnapToThing_{};
    bool thingPlacementSnapRotate_{true};
    double thingPlacementYawOffset_{};
    double thingPlacementBrushSize_{0.25};
    int thingPlacementBrushCount_{5};
    double thingPlacementBrushSpacing_{0.55};
    double thingPlacementBrushScatter_{0.0};
    bool thingPlacementBrushRandomRotation_{true};
    bool thingPlacementBrushRotatePitch_{};
    bool thingPlacementBrushRotateYaw_{true};
    bool thingPlacementBrushRotateRoll_{};
    double thingPlacementBrushFixedAngle_{0.0};
    bool thingPlacementBrushAllowAir_{};
    bool thingPlacementBrushStrokeActive_{};
    bool thingPlacementBrushStrokeHaveLast_{};
    bool thingPlacementBrushStrokePlaced_{};
    bool thingPlacementBrushStrokeUndoPushed_{};
    std::size_t thingPlacementBrushStrokeFirstThing_{};
    TVector thingPlacementBrushStrokeLastPoint_{};
    int thingPlacementTemplateIconGeneration_{};
    bool thingPlacementHeaderDragActive_{};
    QPoint thingPlacementHeaderDragStartGlobal_{};
    bool thingPlacementResizeActive_{};
    QPoint thingPlacementResizeStartGlobal_{};
    int thingPlacementResizeStartSize_{};
    bool thingPlacementFilterResizeActive_{};
    QPoint thingPlacementFilterResizeStartGlobal_{};
    int thingPlacementFilterResizeStartHeight_{};
    int thingPlacementPanelSideSize_{};
    int thingPlacementPanelBandSize_{};
    QPointer<QAbstractSpinBox> thingPlacementSpinScrubBox_{};
    bool thingPlacementSpinScrubPending_{};
    bool thingPlacementSpinScrubActive_{};
    int thingPlacementSpinScrubStartX_{};
    double thingPlacementSpinScrubStartValue_{};
    bool thingPlacementTemplateListDirty_{true};
    std::unordered_map<std::string, QIcon> thingPlacementTemplatePreviewIconCache_{};
    mutable std::vector<ThingPlacementSnapAnchorCacheEntry> thingPlacementSnapAnchorCache_{};
    mutable std::uint64_t thingPlacementSnapAnchorCacheUndoSignature_{};
    mutable std::size_t thingPlacementSnapAnchorCacheThingCount_{};
    mutable bool thingPlacementSnapAnchorCacheValid_{};
    mutable std::optional<FramePlacementOwnerProxy>
        framePlacementOwnerProxyCache_{};
    std::optional<FramePlacementPreview> framePlacementPreview_{};
    mutable std::string thingPlacementSnapTemplateCacheName_{};
    mutable std::string thingPlacementSnapTemplateResourceRevision_{};
    mutable std::optional<ThingBox> thingPlacementSnapTemplateBoxCache_{};
    mutable std::vector<ThingPlacementConnector> thingPlacementSnapTemplateConnectorCache_{};
    mutable ThingPlacementVisualKind thingPlacementTemplateVisualKindCache_{
        ThingPlacementVisualKind::none};
    mutable double thingPlacementTemplateGroundOffsetCache_{};
    mutable std::optional<QVector3D> thingPlacementTemplateSpriteOffsetCache_{};
    mutable bool thingPlacementSnapTemplateCacheValid_{};
#ifdef SED_APP_MAIN_WINDOW_TEST_ACCESS
    mutable std::uint64_t thingPlacementTemplateRevisionChecks_{};
#endif
    std::shared_ptr<ui::detail::GlViewModelThumbnailRenderer> thingPlacementModelThumbnailRenderer_{};
    ui::detail::PreviewResourceDiskWatcher* thingPlacementResourceDiskWatcher_{};
    std::unordered_set<std::string> thingPlacementTemplateIconPrepInFlight_{};
    int thingPlacementTemplateIconActiveJobs_{};
    bool thingPlacementTemplateIconLoadInProgress_{};
    QTimer thingPlacementTemplateIconLoadTimer_{};
    int thingPlacementTemplateIconScheduledGeneration_{};
    int thingPlacementTemplateIconScheduledStartIndex_{};
    ui::detail::PreviewThumbnailPublicationPacer
        thingPlacementTemplateIconPublicationPacer_{};
    int prefsLastCategoryRow_{};
    bool uvScaleLinked_{};
    bool previewShowEditModeOverlay_{};
    bool previewShowTransformOverlay_{};
    bool previewShowToolsOverlay_{};
    bool lastViewportWasPreview_{};
    struct PreviewEditingUiVisibility
    {
        bool valid{};
        std::uint64_t documentEpoch{};
        bool uvEditor{};
        QList<int> uvEditorSplitterSizes{};
        bool thingPlacement{};
        bool thingPlacementCollapsed{};
        QRect thingPlacementGeometry{};
        QRect thingPlacementCollapsedGeometry{};
        bool cogScriptEditor{};
        QRect cogScriptEditorGeometry{};
        int cogScriptEditorWidth{};
        std::vector<std::pair<QPointer<QDockWidget>, bool>> detachedCogScriptEditorVisibility{};
    } previewEditingUiVisibility_{};
    // One-shot authored splitter endpoint produced when Preview interrupts a
    // UV transition. QSplitter::sizes() can transiently expose normalized
    // weights rather than this logical allocation until its next layout turn.
    QList<int> uvEditorPreviewSettledLogicalSplitterSizes_{};
    std::uint64_t uvEditorPreviewSettledLogicalDocumentEpoch_{};
    std::uint64_t previewEditingUiTransitionSerial_{};
    bool previewEditingUiRestoreInProgress_{};
    bool previewUvEditorRestoreInProgress_{};
    bool surfaceExtrudePreviewActive_{};
    SurfaceRef surfaceExtrudePreviewSurface_{};
    double surfaceExtrudeLength_{1.0};
    double surfaceExtrudeExpand_{0.0};
    bool sectorJoinPreviewActive_{};
    editor::Selection sectorJoinPreviewSelection_{};
    bool uvEditorPanelExpanded_{true};
    bool uvEditorForceDisplayRebuild_{};
    bool uvEditorViewportFacing_{};
    bool uvEditorSuppressViewTransformSync_{};
    bool uvEditorViewportZoomSaved_{};
    bool uvEditorPanelProgrammaticResize_{};
    bool uvEditorPanelHideTransitionActive_{};
    std::uint64_t uvEditorPanelTransitionSerial_{};
    int uvEditorPanelExpandedHeight_{320};
    UvEditorDockPosition uvEditorDockPosition_{UvEditorDockPosition::bottom};
    QSize uvEditorViewportZoomBaseSize_{};
    QSize uvEditorViewportZoomLastTargetSize_{};
    bool pendingEditorViewRefitAfterResize_{};
    editor::Selection cogLinkSelectionBackup_{};
    bool cogLinkSelectionRestorePending_{};
    bool uvEditorPanelHeaderDragActive_{};
    QPoint uvEditorPanelHeaderDragStartGlobal_{};
    QRubberBand* uvEditorPanelDockPreviewBand_{};
    bool uvEditorPanelDockPreviewVisible_{};
    UvEditorDockPosition uvEditorPanelDockPreviewPosition_{UvEditorDockPosition::bottom};

    bool haveLastSurfaceClick_{};
    SurfaceRef lastSurfaceClickSurface_{};
    TVector lastSurfaceClickPos_{};
    std::int32_t lastViewportClickSector_{-1};
    std::int32_t lastViewportClickSurface_{-1};
    std::int32_t lastViewportClickThing_{-1};
    std::int32_t lastViewportClickLight_{-1};

    struct TransformPreviewState
    {
        bool active{};
        bool rotating{};
        TVector translation{};
        TVector axis{};
        TVector pivot{};
        double angleDeg{};
        double scaleFactor{1.0};
    };

    TransformPreviewState transformPreview_{};

    QAction* undoAction_{};
    QAction* redoAction_{};
    QAction* deleteAction_{};
    QAction* sectorDeleteMenuAction_{};
    QAction* surfaceDeleteMenuAction_{};
    QAction* thingDeleteMenuAction_{};
    QAction* lightDeleteMenuAction_{};
    QAction* cogDeleteMenuAction_{};
    QAction* findAction_{};
    QAction* findNextAction_{};
    std::array<QAction*, 11> cogEditorFocusActions_{};
    bool levelFindBaseEnabled_{};
    bool levelFindNextBaseEnabled_{};
    bool cogEditorActionsBaseEnabled_{};
    bool cogEditorFocusActive_{};
    QAction* extrudeSurfaceAction_{};
    QAction* makeAdjoinAction_{};
    QAction* unAdjoinAction_{};
    QAction* connectSurfacesAction_{};
    QAction* connectSectorsAction_{};
    QAction* addCubeAction_{};
    QAction* importSectorAction_{};
    QAction* exportSectorAction_{};
    QAction* forceDeleteSurfaceVertexAction_{};
    QAction* addThingAction_{};
    QAction* addLightAction_{};
    QAction* addThingLightAction_{};
    QAction* addSectorLightAction_{};
    QAction* addThingRegularLightAction_{};
    QAction* addThingHeadlightAction_{};
    QMenu* addThingLightMenu_{};
    QAction* addCogAction_{};
    QAction* duplicateCogAction_{};
    QAction* translateAction_{};
    QAction* rotateAction_{};
    QAction* scaleAction_{};
    QAction* mirrorAction_{};
    QAction* flipSurfaceAction_{};
    QAction* uvAutoAlignAction_{};
    QAction* uvEditorAction_{};
    QAction* templateEditorAction_{};
    QAction* uvMoveAction_{};
    QAction* uvRotateAction_{};
    QAction* uvScaleAction_{};
    QAction* uvStitchFromReferenceAction_{};
    QAction* uvStitchConnectedAction_{};
    QAction* uvCopyFromReferenceAction_{};
    QAction* recalcLightingAction_{};
    QAction* autoRecalculateLightingAction_{};
    QAction* thingLightRenderingAction_{};
    QAction* sectorPointLightRenderingAction_{};

    QFileSystemWatcher* cogScriptWatcher_{};
    std::unordered_map<std::string, std::filesystem::path> cogScriptDiskPathByKey_{};
    std::unordered_map<std::wstring, std::filesystem::path> cogScriptDiskPathByIdentity_{};
    std::unordered_map<std::wstring, std::string> cogScriptDiagnosticsByPath_{};
    // Last successfully parsed disk bytes per current watcher scope. This is
    // only a duplicate-notification guard; it is never used to recreate files.
    std::unordered_map<std::wstring, std::string> cogScriptAppliedContentByPath_{};
    std::uint64_t cogScriptWatchGeneration_{};
    std::string cogScriptWatchIdentity_{};
    std::unordered_map<std::wstring, QPointer<QWidget>> openCogScriptEditors_{};
    static constexpr int kMinimumOutlinerDockWidth = 420;
    static constexpr int kDefaultOutlinerDockWidth = 480;
    // The dock contains a source editor and its outline. Keep enough width for
    // the complete bottom Find/zoom chrome so splitter drags cannot compress
    // the fixed search controls into each other.
    static constexpr int kMinimumCogScriptDockWidth = 720;

    int cogScriptDockPreferredWidth_{620};
    int standaloneOutlinerPreferredWidth_{kDefaultOutlinerDockWidth};
    bool cogScriptDockWidthLearned_{false};
    bool adjustingCogDockWidths_{false};
    bool suppressStandaloneOutlinerWidthCapture_{false};
    bool cogEntityPickActive_{};
    std::int32_t cogEntityPickCogIndex_{-1};
    std::int32_t cogEntityPickValueIndex_{-1};
    CogType cogEntityPickType_{CogType::int_};
    PlacementMode cogEntityPickRestorePlacementMode_{PlacementMode::none};
    bool cogEntityNavigatePending_{};
    OutlinerKind cogEntityNavigatePendingKind_{};
    std::int32_t cogEntityNavigatePendingIndex_{-1};
    std::int32_t cogEntityNavigatePendingSector_{-1};
    std::int32_t cogEntityNavigatePendingSurface_{-1};
    bool cogValueFocusPending_{};
    std::int32_t cogValueFocusCogIndex_{-1};
    std::int32_t cogValueFocusValueIndex_{-1};

    QActionGroup* editModeGroup_{};
    QAction* editSectorAction_{};
    QAction* editSurfaceAction_{};
    QAction* editThingAction_{};
    QAction* editLightAction_{};
    QAction* editFrameAction_{};

    QActionGroup* surfaceSelectionGroup_{};
    QAction* surfaceFaceAction_{};
    QAction* surfaceEdgeAction_{};
    QAction* surfaceVertexAction_{};
    QAction* surfaceStrictSelectionAction_{};
    QAction* surfaceNormalsAction_{};

    QActionGroup* lightEditSubModeGroup_{};
    QAction* lightEditBakedAction_{};
    QAction* lightEditThingAction_{};
    QAction* lightEditSectorAction_{};
    QAction* lightEditArchitecturalAction_{};
    QAction* alwaysShowBakedLightGizmosAction_{};
    QAction* alwaysShowThingLightGizmosAction_{};
    QAction* alwaysShowSectorLightGizmosAction_{};
    QAction* alwaysShowArchitecturalLightGizmosAction_{};

    QActionGroup* shadingGroup_{};
    QAction* viewportVerticesAction_{};
    QAction* viewportWireframeAction_{};
    QAction* viewportSolidAction_{};
    QAction* viewportTexturedAction_{};
    QAction* viewportWireframeOverlayAction_{};
    QAction* viewportGamePreviewAction_{};
    QAction* viewportMuteSoundAction_{};

    bool editorViewportSoundMuted_{};
    bool previewViewportSoundMuted_{};

    QActionGroup* lightingGroup_{};
    QAction* viewportFullbrightAction_{};
    QAction* viewportLitAction_{};
    QAction* viewportDiffuseAction_{};
    QAction* viewportGouraudAction_{};

    QActionGroup* thingViewGroup_{};
    QAction* thingDotsAction_{};
    QAction* thingBoxesAction_{};
    QAction* thingWireframesAction_{};
    QAction* thingShadedAction_{};
    QAction* thingRenderAction_{};
    QAction* thingHiddenAction_{};
    QAction* thingCollisionShapesAction_{};
    QAction* thingVisualBoundsAction_{};
    QAction* thingModelLodAction_{};
    QAction* thingGizmosAction_{};
    QAction* thingCubesAction_{};

    QAction* viewportCancelAction_{};
    QAction* viewportFrameLevelAction_{};
    QAction* viewportToggleProjectionAction_{};
    QAction* viewportOrbitLeftAction_{};
    QAction* viewportOrbitRightAction_{};
    QAction* viewportOrbitUpAction_{};
    QAction* viewportOrbitDownAction_{};
    QAction* viewportViewOppositeAction_{};
    QAction* viewportViewFrontAction_{};
    QAction* viewportViewBackAction_{};
    QAction* viewportViewRightAction_{};
    QAction* viewportViewLeftAction_{};
    QAction* viewportViewTopAction_{};
    QAction* viewportViewBottomAction_{};
    QAction* viewportPanLeftAction_{};
    QAction* viewportPanRightAction_{};
    QAction* viewportPanUpAction_{};
    QAction* viewportPanDownAction_{};
    QAction* viewportRollLeftAction_{};
    QAction* viewportRollRightAction_{};
    QAction* viewportRollResetAction_{};
    QAction* viewportGridVisibleThroughWallsAction_{};
    QAction* viewportGridAlwaysBehindAction_{};
    QAction* viewportViewAlignToGridAction_{};
    QAction* viewportGridAlignToViewAction_{};
    QAction* viewportGridOrientToObjectAction_{};
    QAction* viewportGridOrientToSelectionAction_{};
    QAction* viewportGridCenterOnSelectionAction_{};
    QAction* viewportGridClearCenterAction_{};

    QAction* flyFastAction_{};
    QAction* flySlowAction_{};
    QAction* flySpeedUpAction_{};
    QAction* flySpeedDownAction_{};
    std::unordered_map<int, ui::shortcuts::FlyNavigationShortcut> flyHeldKeys_{};
    QAction* flyForwardAction_{};
    QAction* flyBackwardAction_{};
    QAction* flyLeftAction_{};
    QAction* flyRightAction_{};
    QAction* flyDownAction_{};
    QAction* flyUpAction_{};
    ui::shortcuts::ModifierBindings selectionAddModifierBindings_{Qt::ShiftModifier};
    ui::shortcuts::ModifierBindings selectionRemoveModifierBindings_{Qt::ControlModifier};
    ui::shortcuts::ModifierBindings selectionShortestPathModifierBindings_{
        Qt::ControlModifier | Qt::ShiftModifier};
    ui::shortcuts::ModifierBindings selectionBypassSurfaceFiltersModifierBindings_{Qt::AltModifier};
    ui::shortcuts::ModifierBindings thingPlacementTiltModifierBindings_{Qt::ControlModifier};
    ui::shortcuts::ModifierBindings thingPlacementFreeModifierBindings_{Qt::ShiftModifier};
    ui::shortcuts::ModifierBindings transformGridSnapInvertModifierBindings_{Qt::ControlModifier};
    ui::shortcuts::ModifierBindings transformGridStickInvertModifierBindings_{
        Qt::ShiftModifier | Qt::AltModifier};
    ui::shortcuts::ModifierBindings sectorMoveSnapAlignInvertModifierBindings_{Qt::ControlModifier};
    ui::shortcuts::ModifierBindings surfaceGeometrySnapInvertModifierBindings_{Qt::ShiftModifier};
    ui::shortcuts::ModifierBindings thingMoveSurfaceStickInvertModifierBindings_{Qt::ShiftModifier};
    ui::shortcuts::ModifierBindings thingMoveSurfaceAlignInvertModifierBindings_{
        Qt::ControlModifier | Qt::ShiftModifier};
    ui::shortcuts::ModifierBindings thingMoveSnapInvertModifierBindings_{Qt::ControlModifier};
    ui::shortcuts::ModifierBindings thingMoveSnapRotateInvertModifierBindings_{
        Qt::ControlModifier | Qt::AltModifier};
    QAction* selectionAddModeAction_{};
    QAction* selectionRemoveModeAction_{};
    QAction* selectionSurfacePathModeAction_{};
    QAction* selectionSurfaceBypassFiltersModeAction_{};
    QAction* thingPlacementTiltModifierAction_{};
    QAction* thingPlacementFreeModifierAction_{};
};

#ifdef SED_APP_MAIN_WINDOW_TEST_ACCESS
bool templateEditorArchiveReloadRetryForTest(
    const assets::ResourceLocator& resources,
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
#endif

} // namespace sed
