#include "main_window/common.hpp"
#include "ui/components/path_display_utils.hpp"
#include "ui/project/project_startup_utils.hpp"
#include "ui/resources/project_resource_settings.hpp"

#include "sed/assets/material_dimensions.hpp"
#include "sed/editor/uv_promotion.hpp"

#include <QFutureWatcher>
#include <QScopeGuard>
#include <QtConcurrent/QtConcurrentRun>

namespace sed
{

#include "main_window/detail/helpers.hpp"

namespace
{

struct LegacyUvPromotionPreparation
{
    Level level{};
    editor::DocumentRecoveryHistory history{};
    editor::DocumentUvConversionResult conversion{};
};

bool selectionsEqual(
    const editor::Selection& lhs,
    const editor::Selection& rhs)
{
    return lhs.sectors == rhs.sectors
        && lhs.surfaces == rhs.surfaces
        && lhs.surfaceVertices == rhs.surfaceVertices
        && lhs.things == rhs.things
        && lhs.frames == rhs.frames
        && lhs.lights == rhs.lights
        && lhs.cogs == rhs.cogs;
}

QString legacyUvFailureDetail(
    const editor::DocumentUvConversionResult& conversion)
{
    QString location{};
    if ( conversion.failedHistoryStack )
    {
        location = QStringLiteral(" in %1 history entry %2")
            .arg(*conversion.failedHistoryStack
                    == editor::DocumentRecoveryHistoryStack::Undo
                ? QStringLiteral("Undo")
                : QStringLiteral("Redo"))
            .arg(static_cast<qulonglong>(
                conversion.failedHistoryIndex + 1));
    }
    if ( conversion.failedSurface )
    {
        location += QStringLiteral(" at Sector %1, Surface %2")
            .arg(conversion.failedSurface->sector)
            .arg(conversion.failedSurface->surface);
    }
    return QStringLiteral(
        "This Level remains in legacy texture-pixel UV space because the "
        "complete Level and history cannot yet be normalized%1: %2")
        .arg(location, QString::fromStdString(conversion.error));
}

} // namespace

#ifdef SED_APP_MAIN_WINDOW_TEST_ACCESS
QString MainWindow::legacyUvCompatibilityStatusForTest() const
{
    return legacyUvStatusLabel_ && !legacyUvStatusLabel_->isHidden()
        ? legacyUvStatusLabel_->text()
        : QString{};
}
#endif

MainWindow::StagedProjectResourceState MainWindow::stageProjectResources(
    const ProjectType kind,
    const std::filesystem::path& sourcePath)
{
    StagedProjectResourceState staged{};
    staged.kind = kind;

    const auto isExistingPath = [](
        const std::optional<std::filesystem::path>& path) noexcept
    {
        if (!path || path->empty()) return false;
        std::error_code error{};
        return std::filesystem::exists(*path, error) && !error;
    };

    std::optional<std::filesystem::path> installDir =
        readProjectInstallDirSetting(kind);
    const ui::resources::ProjectInstallPathValidation configuredValidation =
        installDir
        ? ui::resources::validateProjectInstallPath(kind, *installDir)
        : ui::resources::ProjectInstallPathValidation{};
    if (installDir && isExistingPath(installDir)
        && !configuredValidation.canUse())
    {
        staged.warnings.push_back(
            ui::project::projectInstallPathMismatchMessage(
                kind, *installDir, configuredValidation));
    }
    if (!installDir || !isExistingPath(installDir)
        || !configuredValidation.canUse())
    {
        installDir.reset();
        if (const auto detected = tryGetProjectInstallDir(kind))
        {
            installDir = detected;
            staged.detectedInstallDir = detected;
        }
    }

    // A speculative Open is a pure read. In particular it must not run the
    // legacy migration, whose QSettings writes and process-local guard cannot
    // be rolled back after a parse/prompt/authorization failure.
    const ui::resources::ResourceSettings loaded =
        readResourceSettings(kind, false, false);
    if (installDir)
    {
        std::error_code installError{};
        if (!std::filesystem::exists(*installDir, installError)
            || installError)
        {
            staged.warnings.push_back(
                QString("Configured install directory does not exist; not filtering resources: %1")
                    .arg(ui::nativeDisplayPath(*installDir)));
            staged.roots = loaded.roots;
            staged.gobs = loaded.gobs;
        }
        else
        {
            const ui::resources::ResourceSettings detected =
                ui::resources::findDefaultProjectResources(kind, *installDir);
            const ui::resources::ResourceSettingsMergeResult merge =
                ui::resources::mergeDetectedResourceSettings(loaded, detected);
            for (const std::filesystem::path& root : merge.settings.roots)
            {
                if (ui::resources::pathIsUnderInstallDir(root, *installDir))
                {
                    staged.roots.push_back(root);
                }
                else
                {
                    staged.warnings.push_back(
                        QString("Ignoring resource root outside this project's install directory: %1")
                            .arg(ui::nativeDisplayPath(root)));
                }
            }
            for (const std::filesystem::path& gob : merge.settings.gobs)
            {
                if (ui::resources::pathIsUnderInstallDir(gob, *installDir))
                {
                    staged.gobs.push_back(gob);
                }
                else
                {
                    staged.warnings.push_back(
                        QString("Ignoring archive outside this project's install directory: %1")
                            .arg(ui::nativeDisplayPath(gob)));
                }
            }
            if (kind == ProjectType::IJIM)
            {
                ui::resources::putIjimResourceRootFirst(
                    staged.roots, *installDir);
            }
        }
    }
    else
    {
        staged.roots = loaded.roots;
        staged.gobs = loaded.gobs;
    }

    for (const std::filesystem::path& root : staged.roots)
    {
        staged.locator.addRoot(root);
    }
    for (const std::filesystem::path& gob : staged.gobs)
    {
        try
        {
            staged.locator.mountGob(gob);
        }
        catch (const std::exception& exception)
        {
            staged.warnings.push_back(
                QString("Could not mount resource archive %1: %2")
                    .arg(ui::nativeDisplayPath(gob),
                        QString::fromUtf8(exception.what())));
        }
        catch (...)
        {
            staged.warnings.push_back(
                QString("Could not mount resource archive %1.")
                    .arg(ui::nativeDisplayPath(gob)));
        }
    }
    if (!sourcePath.empty())
    {
        ui::resources::configureResourceLocator(
            staged.locator, sourcePath, kind);
    }

    QString rootsText{};
    for (const std::filesystem::path& root : staged.locator.roots())
    {
        if (!rootsText.isEmpty()) rootsText += "; ";
        rootsText += ui::nativeDisplayPath(root);
    }
    if (!rootsText.isEmpty())
    {
        staged.information.push_back(
            QString("Resource roots: %1").arg(rootsText));
    }
    QString gobsText{};
    for (const assets::GobArchive& gob : staged.locator.gobs())
    {
        if (!gobsText.isEmpty()) gobsText += "; ";
        gobsText += ui::nativeDisplayPath(gob.path());
    }
    if (!gobsText.isEmpty())
    {
        staged.information.push_back(
            QString("Mounted GOBs: %1").arg(gobsText));
    }
    staged.locatorSnapshot = makeResourceLocatorSnapshot(staged.locator);
    return staged;
}

void MainWindow::persistStagedProjectResources(
    const StagedProjectResourceState& staged) const
{
    writeLastProjectKindSetting(staged.kind);
    if (staged.detectedInstallDir)
    {
        writeProjectInstallDirSetting(
            staged.kind, staged.detectedInstallDir);
    }
    writeResourceSettings(staged.kind,
        ui::resources::ResourceSettings{ staged.roots, staged.gobs });
}

void MainWindow::reportStagedProjectResources(
    const StagedProjectResourceState& staged)
{
    for (const QString& warning : staged.warnings)
    {
        logMessage(MessageSeverity::warning, warning);
    }
    for (const QString& information : staged.information)
    {
        logMessage(MessageSeverity::info, information);
    }
}

void MainWindow::resetLegacyUvCompatibilityStatus()
{
    legacyUvPromotionReady_.reset();
    legacyUvBlockedMaterial_.clear();
    legacyUvCompatibilityDetail_.clear();
    legacyUvPromotionRetryScheduled_ = false;
    legacyUvPromotionRetryPending_ = false;
    ++legacyUvPromotionEvaluationGeneration_;
    updateLegacyUvCompatibilityStatusLabel();
}

void MainWindow::updateLegacyUvCompatibilityStatusLabel()
{
    if ( !legacyUvStatusLabel_ )
    {
        return;
    }
    if ( document_.level().uvCoordinateSpace
        != UvCoordinateSpace::TexturePixels )
    {
        legacyUvStatusLabel_->hide();
        legacyUvStatusLabel_->clear();
        legacyUvStatusLabel_->setToolTip({});
        return;
    }

    legacyUvStatusLabel_->setText(
        legacyUvPromotionReady_.value_or(false)
            ? QStringLiteral("UV: Ready to normalize")
            : QStringLiteral("UV: Legacy pixels"));
    legacyUvStatusLabel_->setToolTip(
        legacyUvCompatibilityDetail_.isEmpty()
            ? QStringLiteral(
                "This Level is using legacy texture-pixel UV coordinates.")
            : legacyUvCompatibilityDetail_);
    legacyUvStatusLabel_->show();
}

void MainWindow::scheduleLegacyUvPromotionRetry()
{
    if ( legacyUvPromotionRetryScheduled_
        || document_.level().uvCoordinateSpace
            != UvCoordinateSpace::TexturePixels )
    {
        return;
    }
    legacyUvPromotionRetryScheduled_ = true;
    QTimer::singleShot(0, this,
        [this]()
        {
            legacyUvPromotionRetryScheduled_ = false;
            evaluateLegacyUvCompatibility(true, true);
        });
}

void MainWindow::evaluateLegacyUvCompatibility(
    const bool allowPromotion,
    const bool resourceAvailabilityChanged)
{
    if ( document_.level().uvCoordinateSpace
        != UvCoordinateSpace::TexturePixels )
    {
        resetLegacyUvCompatibilityStatus();
        return;
    }

    if ( legacyUvPromotionEvaluationInFlight_ )
    {
        legacyUvPromotionRetryPending_ = true;
        return;
    }

    const LegacyUvPromotionPolicy policy = loadLegacyUvPromotionPolicy();
    const std::shared_ptr<const assets::ResourceLocator>
        sourceResourceIdentity = resourceLocatorSnapshot_;
    const std::shared_ptr<const assets::ResourceLocator> sourceResources =
        sourceResourceIdentity
            ? sourceResourceIdentity
            : makeResourceLocatorSnapshot(resources_);

    // File watchers can report the same directory write several times. Once
    // one exact material blocks promotion, first probe only that dependency;
    // do not repeatedly copy and scan the complete native timeline until it
    // becomes resolvable.
    if ( resourceAvailabilityChanged && !legacyUvBlockedMaterial_.empty() )
    {
        Surface blocker{};
        blocker.material = legacyUvBlockedMaterial_;
        if ( !assets::resolveMaterialTextureDimensions(
                *sourceResources, blocker.material) )
        {
            legacyUvPromotionReady_ = false;
            updateLegacyUvCompatibilityStatusLabel();
            return;
        }
    }

    const std::uint64_t sourceDocumentEpoch = recoveryDocumentEpoch_;
    const std::uint64_t sourceUndoSignature = document_.undoSignature();
    const std::uint64_t sourceRedoSignature = document_.redoSignature();
    const std::uint64_t sourceResourceGeneration =
        legacyUvResourceGeneration_;
    const std::uint64_t sourceSemanticMutationSerial =
        recoverySemanticMutationSerial_;
    // Same-Undo property/preview commits can change authored Level bytes
    // without changing the ordinary Undo signature. Use the authoritative
    // saved-Level comparison so a fresh retry cannot bless such an edit as
    // clean merely because it shares the preceding history token.
    const bool sourceWasDirty = hasUnsavedAuthoredLevelChanges();
    const editor::Selection sourceSelection = document_.selection();
    Level sourceLevel{};
    editor::DocumentRecoveryHistory sourceHistory{};
    try
    {
        sourceLevel = document_.level();
        sourceHistory = document_.compactRecoveryHistory();
    }
    catch ( const std::exception& exception )
    {
        legacyUvPromotionReady_ = false;
        legacyUvCompatibilityDetail_ = QStringLiteral(
            "Could not prepare legacy UV normalization safely: %1")
            .arg(QString::fromUtf8(exception.what()));
        updateLegacyUvCompatibilityStatusLabel();
        return;
    }
    catch ( ... )
    {
        legacyUvPromotionReady_ = false;
        legacyUvCompatibilityDetail_ = QStringLiteral(
            "Could not prepare legacy UV normalization safely.");
        updateLegacyUvCompatibilityStatusLabel();
        return;
    }
    ++legacyUvPromotionEvaluationCount_;
    const std::uint64_t evaluationGeneration =
        ++legacyUvPromotionEvaluationGeneration_;
    legacyUvPromotionEvaluationInFlight_ = true;
    using PreparationPtr = std::shared_ptr<LegacyUvPromotionPreparation>;
    auto* watcher = new QFutureWatcher<PreparationPtr>(this);
    connect(watcher, &QFutureWatcher<PreparationPtr>::finished, this,
        [this, watcher, evaluationGeneration, sourceDocumentEpoch,
         sourceUndoSignature, sourceRedoSignature, sourceResourceGeneration,
         sourceSemanticMutationSerial,
         sourceResourceIdentity, sourceResources, sourceSelection,
         sourceWasDirty, allowPromotion,
         resourceAvailabilityChanged, policy]()
        {
            PreparationPtr prepared{};
            try
            {
                prepared = watcher->result();
            }
            catch ( const std::exception& exception )
            {
                legacyUvCompatibilityDetail_ = QStringLiteral(
                    "Legacy UV normalization preparation failed safely: %1")
                    .arg(QString::fromUtf8(exception.what()));
            }
            catch ( ... )
            {
                legacyUvCompatibilityDetail_ = QStringLiteral(
                    "Legacy UV normalization preparation failed safely.");
            }
            watcher->deleteLater();
#ifdef SED_APP_MAIN_WINDOW_TEST_ACCESS
            if ( legacyUvPromotionPreparedHookForTest_ )
            {
                std::function<void()> hook = std::move(
                    legacyUvPromotionPreparedHookForTest_);
                legacyUvPromotionPreparedHookForTest_ = {};
                hook();
            }
#endif
            const auto finishEvaluation = qScopeGuard(
                [this]()
                {
                    legacyUvPromotionEvaluationInFlight_ = false;
                    if ( legacyUvPromotionRetryPending_ )
                    {
                        legacyUvPromotionRetryPending_ = false;
                        scheduleLegacyUvPromotionRetry();
                    }
                });
            const bool sourceStillCurrent =
                evaluationGeneration == legacyUvPromotionEvaluationGeneration_
                && recoveryDocumentEpoch_ == sourceDocumentEpoch
                && document_.undoSignature() == sourceUndoSignature
                && document_.redoSignature() == sourceRedoSignature
                && recoverySemanticMutationSerial_
                    == sourceSemanticMutationSerial
                && legacyUvResourceGeneration_ == sourceResourceGeneration
                && resourceLocatorSnapshot_ == sourceResourceIdentity
                && loadLegacyUvPromotionPolicy() == policy
                && document_.level().uvCoordinateSpace
                    == UvCoordinateSpace::TexturePixels
                && selectionsEqual(document_.selection(), sourceSelection);
            if ( !prepared )
            {
                legacyUvPromotionReady_ = false;
                updateLegacyUvCompatibilityStatusLabel();
                return;
            }
            if ( !sourceStillCurrent )
            {
                if ( legacyUvPromotionRetryPending_
                    || document_.level().uvCoordinateSpace
                        == UvCoordinateSpace::TexturePixels )
                {
                    scheduleLegacyUvPromotionRetry();
                }
                return;
            }
            if ( !prepared->conversion )
            {
                legacyUvPromotionReady_ = false;
                legacyUvBlockedMaterial_ =
                    prepared->conversion.failedMaterial;
                legacyUvCompatibilityDetail_ = legacyUvFailureDetail(
                    prepared->conversion);
                updateLegacyUvCompatibilityStatusLabel();
                return;
            }

            const bool newlyReady =
                !legacyUvPromotionReady_.value_or(false);
            legacyUvPromotionReady_ = true;
            legacyUvBlockedMaterial_.clear();
            legacyUvCompatibilityDetail_ = QStringLiteral(
                "Every required material size is available. The complete "
                "Level and its undo/redo history can be normalized without "
                "changing the visible texture mapping.");
            updateLegacyUvCompatibilityStatusLabel();
            if ( !allowPromotion
                || policy == LegacyUvPromotionPolicy::keepPixelSpace )
            {
                return;
            }

            bool normalize = policy
                == LegacyUvPromotionPolicy::normalizeAutomatically;
            if ( policy == LegacyUvPromotionPolicy::askBeforeNormalizing )
            {
                if ( !resourceAvailabilityChanged || !newlyReady )
                {
                    return;
                }
                QMessageBox prompt(this);
                prompt.setObjectName(QStringLiteral(
                    "legacy_uv_ready_promotion_prompt"));
                prompt.setIcon(QMessageBox::Information);
                prompt.setWindowTitle(QStringLiteral(
                    "Normalize legacy UVs?"));
                prompt.setText(QStringLiteral(
                    "All material sizes needed by this Level are now "
                    "available."));
                prompt.setInformativeText(QStringLiteral(
                    "Normalize the complete Level and its undo/redo history "
                    "now? The visible texture mapping will not change."));
                QPushButton* normalizeButton = prompt.addButton(
                    QStringLiteral("Normalize"), QMessageBox::AcceptRole);
                normalizeButton->setObjectName(QStringLiteral(
                    "legacy_uv_ready_normalize"));
                QPushButton* keepButton = prompt.addButton(
                    QStringLiteral("Keep Pixel Space"),
                    QMessageBox::ActionRole);
                keepButton->setObjectName(QStringLiteral(
                    "legacy_uv_ready_keep_pixels"));
                QPushButton* cancelButton = prompt.addButton(
                    QMessageBox::Cancel);
                cancelButton->setObjectName(QStringLiteral(
                    "legacy_uv_ready_cancel"));
                prompt.setDefaultButton(normalizeButton);
                prompt.setEscapeButton(cancelButton);
                (void)prompt.exec();
                normalize = prompt.clickedButton() == normalizeButton;
            }
            if ( !normalize )
            {
                return;
            }

            std::optional<editor::Document> stagedDocument;
            try
            {
                stagedDocument.emplace();
                stagedDocument->restoreRecoveryHistory(
                    editor::DocumentState{std::move(prepared->level), sourceSelection},
                    std::move(prepared->history));
            }
            catch (const std::exception& exception)
            {
                legacyUvPromotionReady_ = false;
                legacyUvCompatibilityDetail_ = QStringLiteral(
                    "Could not prepare legacy UV normalization safely: %1")
                    .arg(QString::fromUtf8(exception.what()));
                updateLegacyUvCompatibilityStatusLabel();
                return;
            }

            // The Ask dialog runs a nested event loop. Recheck every source
            // generation and the exact selection before atomic publication.
            if ( recoveryDocumentEpoch_ != sourceDocumentEpoch
                || document_.undoSignature() != sourceUndoSignature
                || document_.redoSignature() != sourceRedoSignature
                || recoverySemanticMutationSerial_
                    != sourceSemanticMutationSerial
                || legacyUvResourceGeneration_ != sourceResourceGeneration
                || resourceLocatorSnapshot_ != sourceResourceIdentity
                || loadLegacyUvPromotionPolicy() != policy
                || document_.level().uvCoordinateSpace
                    != UvCoordinateSpace::TexturePixels
                || !selectionsEqual(
                    document_.selection(), sourceSelection) )
            {
                legacyUvPromotionReady_ = false;
                scheduleLegacyUvPromotionRetry();
                return;
            }

            document_.swap(*stagedDocument);
            if ( sourceWasDirty )
            {
                recoveryRequiresSave_ = true;
            }
            else
            {
                savedUndoSignature_ = document_.undoSignature();
            }
            legacyUvPromotionReady_.reset();
            legacyUvCompatibilityDetail_.clear();
            updateLegacyUvCompatibilityStatusLabel();
            if ( glView_ )
            {
                glView_->notifyLevelGeometryChanged();
                glView_->setHighlightedSelection(document_.selection());
            }
            syncUvEditorPanel();
            rebuildProperties();
            updateEditActions();
            if ( sourceWasDirty )
            {
                noteRecoveryStateChanged(true);
                scheduleRecoverySnapshot(true);
            }
            logMessage(MessageSeverity::info, QStringLiteral(
                "Normalized the complete legacy UV Level and undo/redo "
                "history after the required material sizes became "
                "available."));
        });
    watcher->setFuture(QtConcurrent::run(
        [sourceLevel = std::move(sourceLevel),
         sourceHistory = std::move(sourceHistory),
         sourceResources]() mutable -> PreparationPtr
        {
            try
            {
                auto prepared =
                    std::make_shared<LegacyUvPromotionPreparation>();
                prepared->level = std::move(sourceLevel);
                prepared->history = std::move(sourceHistory);
                auto dimensionsCache = std::make_shared<std::unordered_map<
                    std::string,
                    std::optional<level::UvTextureDimensions>>>();
                const level::UvTextureDimensionsResolver dimensions =
                    [dimensionsCache, sourceResources](const Surface& surface)
                        -> std::optional<level::UvTextureDimensions>
                    {
                        const std::string key = text::lowerAscii(
                            text::trimView(surface.material));
                        if ( const auto found = dimensionsCache->find(key);
                            found != dimensionsCache->end() )
                        {
                            return found->second;
                        }
                        std::optional<level::UvTextureDimensions> value =
                            assets::resolveMaterialTextureDimensions(
                                *sourceResources, surface.material);
                        dimensionsCache->emplace(key, value);
                        return value;
                    };
                prepared->conversion =
                    editor::convertDocumentUvCoordinateSpace(
                        prepared->level,
                        prepared->history,
                        UvCoordinateSpace::Normalized,
                        dimensions);
                return prepared;
            }
            catch ( ... )
            {
                return {};
            }
        }));
}

void MainWindow::loadResourceSettings(
    const ProjectType kind,
    const bool persistSettings)
{
    // Explicit active-project loads retain one-time migration. Speculative
    // Open/recovery callers use stageProjectResources() directly.
    if (persistSettings) maybeMigrateLegacyResourceSettings();
    StagedProjectResourceState staged = stageProjectResources(kind, {});
    if (persistSettings) persistStagedProjectResources(staged);
    resourceKind_ = staged.kind;
    resourceRoots_ = staged.roots;
    resourceGobs_ = staged.gobs;
    reportStagedProjectResources(staged);
}

void MainWindow::saveResourceSettings()
{
    writeResourceSettings(resourceKind_, ui::resources::ResourceSettings{ resourceRoots_, resourceGobs_ });
}

void MainWindow::rebuildResourceLocator(const std::filesystem::path& levelPath)
{
    clearFramePlacementPreview();
    invalidateThingPlacementSnapCaches();
    resources_.clearRoots();
    resources_.clearGobs();

    for (const std::filesystem::path& root : resourceRoots_)
    {
        try
        {
            resources_.addRoot(root);
        }
        catch (...)
        {
        }
    }

    for (const std::filesystem::path& gob : resourceGobs_)
    {
        try
        {
            resources_.mountGob(gob);
        }
        catch (...)
        {
        }
    }

    if (!levelPath.empty())
    {
        ui::resources::configureResourceLocator(resources_, levelPath, resourceKind_);
    }

    publishResourceLocatorSnapshot();
}

void MainWindow::publishResourceLocatorSnapshot()
{
    publishResourceLocatorSnapshot(makeResourceLocatorSnapshot(resources_));
}

std::shared_ptr<const assets::ResourceLocator>
MainWindow::makeResourceLocatorSnapshot(
    const assets::ResourceLocator& resources)
{
    return std::make_shared<const assets::ResourceLocator>(resources);
}

void MainWindow::publishResourceLocatorSnapshot(
    std::shared_ptr<const assets::ResourceLocator> snapshot)
{
    // Detach the member from the prior generation. A renderer currently inside
    // a non-input event pump retains itself (and its locator snapshot) through
    // a local shared owner until that render call returns.
    thingPlacementModelThumbnailRenderer_.reset();
    // Advance the shared preview epoch so a later allocation that reuses an
    // old snapshot address cannot make pointer-scoped cache entries visible.
    ui::detail::clearSharedPreviewAssetCaches();
    resourceLocatorSnapshot_ = std::move(snapshot);
}

MainWindow::StagedGlobalTemplateState MainWindow::stageGlobalTemplates(
    const Level& level) const
{
    StagedGlobalTemplateState staged{};
    for (const ThingTemplate& tpl : level.templates)
    {
        staged.templates.addOrUpdate(tpl, true);
    }

    const auto findDataRoot = []() -> std::optional<std::filesystem::path>
    {
        const std::filesystem::path exeDir = std::filesystem::path(QCoreApplication::applicationDirPath().toStdWString());

        std::filesystem::path cur = exeDir;
        for (int depth = 0; depth < 10; ++depth)
        {
            std::error_code ec;
            const std::filesystem::path dataDir = cur / "data";
            if (std::filesystem::exists(dataDir, ec) && !ec)
            {
                return dataDir;
            }

            const std::filesystem::path parent = cur.parent_path();
            if (parent == cur)
            {
                break;
            }
            cur = parent;
        }

        {
            std::error_code ec;
            const std::filesystem::path dataDir = std::filesystem::current_path() / "data";
            if (std::filesystem::exists(dataDir, ec) && !ec)
            {
                return dataDir;
            }
        }

        return std::nullopt;
    };

    const auto dataRootOpt = findDataRoot();
    if (!dataRootOpt)
    {
        staged.warnings.push_back(
            "Template database not found (missing 'data' directory); using level templates only.");
        return staged;
    }

    const std::filesystem::path dataRoot = *dataRootOpt;
    std::filesystem::path tplPath{};

    switch (level.kind)
    {
    case ProjectType::JKDF2:
        tplPath = dataRoot / "master.tpl";
        break;
    case ProjectType::MOTS:
        tplPath = dataRoot / "mots.tpl";
        break;
    case ProjectType::IJIM:
        tplPath = dataRoot / "ijim.tpl";
        break;
    default:
        staged.errors.push_back(
            "Invalid project type; cannot load global templates.");
        return staged;
    }

    std::error_code ec;
    if (!std::filesystem::exists(tplPath, ec) || ec)
    {
        staged.warnings.push_back(
            QString("Template database missing: %1; using level templates only.")
                .arg(ui::nativeDisplayPath(tplPath)));
        return staged;
    }

    try
    {
        const assets::Templates projectTemplates = assets::loadTemplates(tplPath);
        staged.templates.clear();

        for (std::size_t i = 0; i < projectTemplates.size(); ++i)
        {
            if (const ThingTemplate* tpl = projectTemplates.get(i))
            {
                staged.templates.addOrUpdate(*tpl, true);
            }
        }

        for (const ThingTemplate& tpl : level.templates)
        {
            staged.templates.addOrUpdate(tpl, true);
        }

        staged.information.push_back(
            QString("Loaded templates: %1 (%2 templates)")
                .arg(ui::nativeDisplayPath(tplPath))
                .arg(static_cast<qulonglong>(staged.templates.size())));
    }
    catch (const std::exception& e)
    {
        staged.warnings.push_back(
            QString("Failed to load templates: %1; using level templates only.")
                .arg(QString::fromUtf8(e.what())));

        staged.templates.clear();
        for (const ThingTemplate& tpl : level.templates)
        {
            staged.templates.addOrUpdate(tpl, true);
        }
    }
    return staged;
}

void MainWindow::publishGlobalTemplates(
    StagedGlobalTemplateState&& staged,
    const bool refreshViewport)
{
    clearFramePlacementPreview();
    invalidateThingPlacementSnapCaches();
    templates_ = std::move(staged.templates);
    if (refreshViewport)
    {
        glView_->setTemplateDatabase(
            templates_.size() > 0 ? &templates_ : nullptr);
    }
}

void MainWindow::refreshPublishedGlobalTemplates()
{
    // This also advances the asynchronous thumbnail generation and publishes
    // its immutable ResourceLocator snapshot for the new template list.
    refreshThingPlacementTemplateList();
    syncThingPlacementPanel();
}

void MainWindow::reportStagedGlobalTemplates(
    const StagedGlobalTemplateState& staged)
{
    for (const QString& error : staged.errors)
    {
        logMessage(MessageSeverity::error, error);
    }
    for (const QString& warning : staged.warnings)
    {
        logMessage(MessageSeverity::warning, warning);
    }
    for (const QString& information : staged.information)
    {
        logMessage(MessageSeverity::info, information);
    }
}

void MainWindow::loadGlobalTemplates(const bool refreshViewport)
{
    StagedGlobalTemplateState staged =
        stageGlobalTemplates(document_.level());
    const QStringList information = staged.information;
    const QStringList warnings = staged.warnings;
    const QStringList errors = staged.errors;
    publishGlobalTemplates(std::move(staged), refreshViewport);
    refreshPublishedGlobalTemplates();
    StagedGlobalTemplateState diagnostics{};
    diagnostics.information = information;
    diagnostics.warnings = warnings;
    diagnostics.errors = errors;
    reportStagedGlobalTemplates(diagnostics);
}

} // namespace sed
