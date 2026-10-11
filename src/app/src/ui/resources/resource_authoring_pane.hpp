#pragma once

#include "ui/resources/resource_authoring_editor.hpp"

#include "gl_view.hpp"
#include "sed/assets/resource_locator.hpp"
#include "sed/assets/snd.hpp"
#include "sed/assets/spr.hpp"
#include "sed/assets/string_table.hpp"
#include "sed/assets/tpl.hpp"
#include "sed/level.hpp"
#include "ui/components/live_color_popup.hpp"
#include "ui/components/scrub_float_edit.hpp"

#include <QAbstractItemView>
#include <QAction>
#include <QByteArray>
#include <QCheckBox>
#include <QCloseEvent>
#include <QColor>
#include <QComboBox>
#include <QDialog>
#include <QElapsedTimer>
#include <QEvent>
#include <QHideEvent>
#include <QIcon>
#include <QCborArray>
#include <QCborMap>
#include <QLabel>
#include <QMap>
#include <QLineEdit>
#include <QModelIndex>
#include <QPersistentModelIndex>
#include <QPointer>
#include <QPushButton>
#include <QSet>
#include <QSlider>
#include <QShowEvent>
#include <QSpinBox>
#include <QStringList>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTimer>
#include <QTreeWidget>
#include <QToolButton>
#include <QWidget>
#include <QVBoxLayout>

#include <array>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace sed::ui::resources::detail
{

class ResourceAuthoringDialog final : public QDialog
{
public:
    ResourceAuthoringDialog(
        QWidget* parent,
        const ResourceAuthoringKind kind,
        ResourceAuthoringContext context,
        const bool embedded = false,
        ResourceAuthoringChangedCallback changed = {});

    ~ResourceAuthoringDialog() override;

    ResourceAuthoringKind kind() const noexcept;

    bool isModified() const noexcept;

    QString identity() const;

    QString displayTitle() const;

    QString looseFilePath() const;

    bool takeSuccessfulOpenNotification() noexcept;

#ifdef SED_APP_MAIN_WINDOW_TEST_ACCESS
    void setAfterReadHookForTest(ResourceAuthoringPaneAfterReadHook hook);
#endif

    void rebindProjectDirectory(const std::filesystem::path& projectDirectory);

    bool save(bool saveAs);

    bool openLooseFileForCaller(const bool skipReplaceConfirmation = false);

    bool openProjectResourceForCaller(const bool bypassWorkspaceCallback = false);

    bool confirmClose(bool* discardedAny);

    void closeAfterReplacement();

    bool ownsFocus() const;

#ifdef SED_APP_MAIN_WINDOW_TEST_ACCESS
    ResourceAuthoringPaneTestCounters testCounters() const noexcept;

    void resetTestCounters() const noexcept;

    bool reloadCurrentLooseSourceForTest(QString* error);
#endif

    QCborMap captureState(bool includeHistory = true) const;

    QCborMap authoredContentState() const;

    bool authoringSnapshotStillCurrent(
        const std::uint64_t revision,
        const QCborMap& snapshot);

    bool restoreState(const QCborMap& state, QString* warning);

    static bool validateState(const QCborMap& state, QString* warning);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

    void closeEvent(QCloseEvent* event) override;

    void showEvent(QShowEvent* event) override;

    void hideEvent(QHideEvent* event) override;

    void accept() override;

    void reject() override;

private:
    struct LiveTableEditor
    {
        QPointer<QAbstractItemView> view{};
        QPointer<QWidget> editor{};
        QPersistentModelIndex index{};
    };

    void trackTableEditor(
        QAbstractItemView* const view,
        QWidget* const editor,
        const QModelIndex& index);

    void pruneDetachedTableEditors();

    QString cellTextWithPendingEditor(
        const QAbstractItemView* const view,
        const QModelIndex& index,
        const QString& fallback) const;

    void settleActiveTableEditors();

    enum class SourceKind : std::uint8_t
    {
        draft,
        loose,
        projectResource,
    };

    void changed();

    void buildCommonChrome();

    QPushButton* makeButton(const QString& label, const char* objectName);

    QToolButton* makeToolbarButton(const QIcon& icon, const QString& tooltip, const char* objectName);

    void buildSpriteEditor();

    void buildSoundEditor();
    void openSoundModePopup();
    QTreeWidgetItem* createSoundModeGroup(const std::string& mode);
    QTreeWidgetItem* createSoundAlternative(QTreeWidgetItem* group, int row);
    void appendSoundEntryFooter(QTreeWidgetItem* group);
    void resizeSoundWavColumn(bool fitColumns = true, int changedColumn = -1);
    void updateSoundHistoryActions();
    void setAllSoundModesExpanded(bool expanded);
    bool handleSoundTreePointerEvent(QEvent* event);

public:
    bool undoRedoSoundEdit(bool redo);
    bool canUndoRedoDocument(bool redo) const;
    bool hasPendingDocumentEdit() const;
    bool undoRedoDocument(bool redo, bool respectTextFocus = false);

private:
    struct SoundRowRecord;
    QCborMap spriteHistoryValue() const;
    QCborMap stringHistoryRow(int row) const;
    QCborMap soundHistoryRow(const SoundRowRecord& row) const;
    void resetDocumentHistory();
    void recordDocumentHistory();
    void recordStringHistoryRow(int row);
    void recordStringHistoryInsert(int row);
    void recordStringHistoryRemove(int row);
    QCborMap captureDocumentHistory(const QCborMap& state) const;
    static bool validateDocumentHistory(const QCborMap& state);
    void restoreDocumentHistory(const QCborMap& state);
    void applyDocumentHistoryCommand(const QCborMap& command, bool redo);
    void appendDocumentHistoryCommand(QCborMap command);

    struct SoundRowRecord
    {
        assets::SoundClassEntry entry{};
        std::array<QString, 5> raw{};
        bool previewLoop{};
    };

    struct SoundHistoryEdit
    {
        int row{};
        std::optional<SoundRowRecord> before{};
        std::optional<SoundRowRecord> after{};
        QString presentationMode{};
    };

    SoundRowRecord soundRowRecord(int row) const;
    void recordSoundHistoryEdit(SoundHistoryEdit edit);
    void applySoundHistoryEdit(const SoundHistoryEdit& edit, bool redo);
    void resetSoundHistory();
    void publishSoundHistoryChange(const QString& validationError = {});
    void removeSoundEntry(int row);
    void updateSoundHoverControls(const QPoint& position);
    void updateSoundPlayingControls();
    void hideSoundHoverControls();
    void setSoundPreviewLoop(int row, bool loop);
    void playSoundAlternative(int row, bool togglePlayback = true);
    void updateSoundPreviewVolume(int row, const QString& raw);
    void stopSoundPreview();

    void buildStringEditor();

    static ui::ScrubFloatEdit* makeFloatControl(QWidget* parent, const char* objectName);



    static void selectIntegerCombo(QComboBox* combo, const int value);

    static QStringList soundModeSuggestions(const ProjectType projectType);

    QStringList soundModeSuggestions() const;

    QColor displayExtraLightColor() const;

    void openExtraLightPopup();

    void updateSpriteProfileRows();

    void updateSpriteAnimationTimer();

    std::optional<float> spriteNumericValue(const QString& key) const;
    bool setSpritePendingNumericEdit(const QString& key, const QString& text);

    bool clearSpritePendingNumericEdit(const QString& key);

    void restorePendingSpriteNumericEditors();

    void commitPendingSpriteNumericEdits(assets::SprFile& output) const;

    void commitPendingSpriteNumericEditsAfterSave();

    int spritePreviewFramesPerSecond() const;

    void setSpritePreviewFramesPerSecond(int fps);

    QCborMap captureSpritePresentation() const;

    static bool validateSpritePresentation(const QCborMap& presentation, QString* error);

    void restoreSpritePresentation(const QCborMap& presentation);

    QCborMap captureEditorPresentation() const;

    static bool validateEditorPresentation(
        const QCborMap& presentation,
        const QCborMap& state,
        QString* error);

    void restoreEditorPresentation(const QCborMap& presentation);

    void cancelDeferredEditorPresentation();

    void applyDeferredPresentationScroll();

    void restorePendingTableEditorCaret(
        QAbstractItemView* view,
        QWidget* editor,
        const QModelIndex& index);

    void applyPendingSpritePresentationAfterRefit();

    void refreshSpriteWidgets();

    void refreshSoundRows();

    void refreshSoundModeGroups();

    void syncSoundRawRowsFromTree();

    std::optional<std::string> selectedSoundMode() const;

    void appendSoundEntry(const std::string& mode);

    void syncSoundModelFromRawRows();

    void syncStringModelFromRawRows();

    void refreshStringRows();

    static void restoreRawRows(QTableWidget* table, const QCborArray& rows);

    void restoreSoundRawRows(const QCborArray& rows);

    void updateSoundEntryFromCell(QTreeWidgetItem* item, int column);

    void updateSoundTreeItem(QTreeWidgetItem* item, int column);

    void openSoundClassFlagsPopup(QTreeWidgetItem* item);

    QTreeWidgetItem* soundAlternativeAt(int sourceIndex) const;

    int sourceIndexForSoundItem(const QTreeWidgetItem* item) const;

    bool commitSoundTableDraftToModel();

    void updateStringEntryFromCell(QTableWidgetItem* item);

    bool commitStringTableDraftToModel();

    void filterStringRows();

    void refreshDocumentWidgets();

    void initializeNewDocument(const bool asDraft = false);

    void createNewDocument();

    bool confirmReplaceDocument(bool* discardedAny = nullptr);

    QString fileFilter() const;

    QString extension() const;

    QString suggestedBaseName() const;

    std::filesystem::path defaultDocumentPath() const;

    std::filesystem::path openStartingPath() const;

    std::optional<std::filesystem::path> choosePath(const bool saveAs);

    void openDocument(const bool skipReplaceConfirmation = false);

    void openProjectResource(const bool bypassWorkspaceCallback = false);

    std::string serializeCurrentDocument(std::vector<SoundHistoryEdit>* soundEditsAfterSave = nullptr) const;

    bool saveDocument(const bool saveAs);

    bool reloadCurrentLooseSource(QString* error = nullptr);

    void pickSpriteMaterial();

    void pickSoundWav();

    void scheduleSpritePreview(const bool refit = false);

    void rebuildSpritePreview();

    void markDirty(bool preserveStatus = false);

    void updateDocumentLabel();

    void notifyChanged();

    void setStatus(const QString& text, const bool error = false);

    void reportFailure(const QString& title, const QString& detail);

    const ResourceAuthoringKind kind_;
    ResourceAuthoringContext context_{};
    assets::SprFile sprite_{};
    QCborMap spritePendingNumericEdits_{};
    QMap<QString, QString> spriteNumericEditBaselines_{};
    QSet<QString> spriteNumericUndoKeys_{};
    assets::SprProfile spriteProfile_{ assets::SprProfile::classic };
    assets::SoundClassFile soundFile_{};
    assets::UniProfile stringProfile_{ assets::UniProfile::classic };
    assets::StringTableFile stringsFile_{};
    std::filesystem::path currentPath_{};
    QByteArray diskBaselineHash_{};
    QString draftName_{};
    QString draftIdentity_{};
    QString projectResourceName_{};
    QString archiveSourceTooltip_{};
    SourceKind sourceKind_{ SourceKind::draft };
    ResourceAuthoringChangedCallback changed_{};
    bool successfulOpenNotification_{};
    bool lastOpenSucceeded_{};
    bool lastProjectOpenSucceeded_{};
#ifdef SED_APP_MAIN_WINDOW_TEST_ACCESS
    mutable ResourceAuthoringPaneTestCounters testCounters_{};
    ResourceAuthoringPaneAfterReadHook afterReadHook_{};
#endif
    bool embedded_{};
    bool dirty_{};
    std::uint64_t authoredRevision_{};
    bool refreshing_{};
    bool settlingTableEditors_{};
    std::vector<LiveTableEditor> liveTableEditors_{};
    QCborMap pendingTableEditorCarets_{};
    std::uint64_t editorPresentationGeneration_{};
    std::uint64_t presentationInteractionGeneration_{};
    std::uint64_t presentationInputGeneration_{};
    struct DeferredPresentationScroll
    {
        QPointer<QAbstractItemView> view{};
        int horizontal{};
        int vertical{};
        std::uint64_t presentationGeneration{};
        std::uint64_t interactionGeneration{};
        bool pending{};
    };
    DeferredPresentationScroll deferredPresentationScroll_{};
    bool applyingEditorPresentation_{};
    bool applyingPresentationScroll_{};
    bool soundManualColumnWidths_{};

    QToolButton* newButton_{};
    QToolButton* openButton_{};
    QToolButton* saveButton_{};
    QLabel* documentLabel_{};
    QLabel* statusLabel_{};
    QWidget* contentHost_{};
    QVBoxLayout* contentLayout_{};

    QLineEdit* spriteMaterial_{};
    QToolButton* materialPickButton_{};
    QComboBox* spriteTypeCombo_{};
    ui::ScrubFloatEdit* spriteWidth_{};
    ui::ScrubFloatEdit* spriteHeight_{};
    QComboBox* spriteGeometryMode_{};
    QComboBox* spriteLightMode_{};
    QComboBox* spriteTextureMode_{};
    QWidget* spriteClassicExtraRow_{};
    QWidget* spriteIjimExtraRow_{};
    ui::ScrubFloatEdit* spriteExtraScalar_{};
    QPushButton* extraLightSwatch_{};
    QPointer<ui::LiveColorPopup> extraLightPopup_{};
    std::optional<std::array<float, 4>> previewExtraLight_{};
    bool extraLightPopupEscaped_{};
    std::array<ui::ScrubFloatEdit*, 3> spriteOffset_{};
    QPushButton* previewFitButton_{};
    QCheckBox* spritePreviewBoundsCheck_{};
    QSlider* spritePreviewFpsSlider_{};
    QLabel* spritePreviewFpsLabel_{};
    QToolButton* spritePreviewPauseButton_{};
    QToolButton* spritePreviewStopButton_{};
    QPushButton* spritePreviewRecenterButton_{};
    bool spriteAnimationPaused_{};
    QCborMap pendingSpritePresentation_{};
    GlView* previewView_{};
    QTimer* previewTimer_{};
    QTimer* spriteAnimationTimer_{};
    struct SpritePreviewMatCel
    {
        QImage image{};
        std::int32_t alphaBpp{};
        QVector3D solidColor{ 1.0f, 1.0f, 1.0f };
        std::optional<ClassicSolidLightRamp> classicSolidLightRamp{};
    };
    std::vector<SpritePreviewMatCel> spritePreviewCels_{};
    std::string spritePreviewCacheKey_{};
    int spritePreviewCelIndex_{};
    QElapsedTimer spriteAnimationClock_{};
    qint64 spriteAnimationLastElapsedMs_{};
    double spriteAnimationFrameRemainder_{};
    bool previewRefitPending_{ true };
    bool previewAttached_{};
    std::shared_ptr<assets::ResourceLocator> previewResources_{};
    Level previewLevel_{};
    assets::Templates previewTemplates_{};

    QPushButton* soundAddModeButton_{};
    int soundColumnResizeSection_{ -1 };
    int soundColumnResizeStartX_{};
    int soundColumnResizeStartWidth_{};
    QLineEdit* soundSearch_{};
    bool soundSearchPublicationPending_{};
    QTreeWidget* soundEntries_{};
    std::vector<std::array<QString, 5>> soundRawRows_{};
    std::vector<bool> soundPreviewLoopRows_{};
    QSet<QString> collapsedSoundModes_{};
    QSet<QString> knownSoundModes_{};
    QSet<QString> soundPresentationModes_{};
    std::vector<QTreeWidgetItem*> soundAlternativeItems_{};
    QToolButton* soundPlayButton_{};
    QToolButton* soundLoopButton_{};
    QToolButton* soundRemoveButton_{};
    QToolButton* soundPlayingStopButton_{};
    QToolButton* soundPlayingLoopButton_{};
    QTimer* soundPlaybackWatchTimer_{};
    QPersistentModelIndex soundHoveredIndex_{};
    std::vector<SoundHistoryEdit> soundHistory_{};
    qsizetype soundHistoryBytes_{};
    std::vector<SoundRowRecord> soundHistoryCurrent_{};
    int soundHistoryCursor_{};
    int soundHistoryCleanCursor_{};
    int soundHistoryAuthoredCursor_{};
    int soundHistoryCleanAuthoredCursor_{};
    bool soundHistoryBaselineDirty_{};
    std::vector<QCborMap> documentHistory_{};
    qsizetype documentHistoryBytes_{};
    int documentHistoryCursor_{};
    int documentHistoryCleanCursor_{};
    bool documentHistoryBaselineDirty_{};
    bool applyingDocumentHistory_{};
    QCborMap spriteHistoryCurrent_{};
    QString spriteHistoryPendingKey_{};
    std::vector<QCborMap> stringHistoryCurrent_{};
    QAction* soundUndoAction_{};
    QAction* soundRedoAction_{};
    std::uint64_t soundPlaybackLease_{};
    int soundPlayingRow_{ -1 };
    QLineEdit* stringSearch_{};
    QTableWidget* stringTable_{};
    QPushButton* stringAddButton_{};
    QPushButton* stringRemoveButton_{};
};

} // namespace sed::ui::resources::detail
