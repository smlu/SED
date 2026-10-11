#include "main_window/common.hpp"

#include "ui/audio/app_sound_player.hpp"
#include "ui/audio/audio_transition_utils.hpp"
#include "ui/audio/audio_resource_identity_utils.hpp"
#include "ui/audio/sector_sound_utils.hpp"

#include "sed/assets/indy_wv.hpp"

#include <QtConcurrent/QtConcurrentRun>

namespace sed
{

bool MainWindow::currentViewportSoundMuted() const noexcept
{
    const bool previewPipeline = glView_
        && glView_->renderPipeline() == GlView::RenderPipeline::GamePreview;
    return ui::audio::viewportSoundMuted(
        editorViewportSoundMuted_,
        previewViewportSoundMuted_,
        previewPipeline);
}

void MainWindow::setCurrentViewportSoundMuted(const bool muted)
{
    const bool previewPipeline = glView_
        && glView_->renderPipeline() == GlView::RenderPipeline::GamePreview;
    if (previewPipeline)
    {
        previewViewportSoundMuted_ = muted;
    }
    else
    {
        editorViewportSoundMuted_ = muted;
    }

    QSettings settings = settingsStore();
    settings.setValue(
        previewPipeline
            ? QStringLiteral("viewport/preview_mute_sound")
            : QStringLiteral("viewport/editor_mute_sound"),
        muted);

    syncViewportSoundControls();
    updateSectorSoundPlayback();
}

void MainWindow::syncViewportSoundControls()
{
    if (!viewportMuteSoundAction_)
    {
        return;
    }

    const bool previewPipeline = glView_
        && glView_->renderPipeline() == GlView::RenderPipeline::GamePreview;
    const bool muted = currentViewportSoundMuted();
    const QSignalBlocker blocker(viewportMuteSoundAction_);
    viewportMuteSoundAction_->setChecked(muted);
    viewportMuteSoundAction_->setIcon(ui::makeViewportIcon(
        muted ? ui::ViewportIcon::soundMuted : ui::ViewportIcon::sound));
    viewportMuteSoundAction_->setText(
        previewPipeline
            ? QStringLiteral("Mute sector sounds (Preview)")
            : QStringLiteral("Mute sector sounds (Editor)"));
    viewportMuteSoundAction_->setToolTip(
        muted
            ? QStringLiteral("<b>Sector sounds</b><br>Turn on ambient sounds assigned to Sectors in the current view.")
            : QStringLiteral("<b>Sector sounds</b><br>Mute ambient sounds assigned to Sectors in the current view."));
}

void MainWindow::updateSectorSoundPlayback()
{
    using ui::audio::AppSoundPlayer;
    using ui::audio::SoundPlaybackOwner;

    AppSoundPlayer& player = AppSoundPlayer::instance();
    const auto clearTrackedSound = [this]() noexcept
    {
        viewportSoundSector_ = -1;
        viewportSoundEyeValid_ = false;
        viewportSoundName_.clear();
        viewportSoundResourceKey_.clear();
        viewportFailedSoundResourceKey_.clear();
        viewportSoundLocatorGeneration_.reset();
        viewportSoundVolume_ = 0.0;
        viewportSoundPlaybackCurrent_ = false;
        ++viewportSoundLoadGeneration_;
    };
    if (!glView_ || !isVisible() || isMinimized())
    {
        player.stop(SoundPlaybackOwner::viewport);
        clearTrackedSound();
        return;
    }

    // A preview temporarily owns the shared channel. A finished docked one-shot
    // releases priority, and ambient playback resumes on the next timer tick.
    if ( QApplication::activeModalWidget() || player.hasPreviewOwner() )
    {
        if ( !player.hasPreviewOwner() )
        {
            player.stop(SoundPlaybackOwner::viewport);
        }
        viewportSoundPlaybackCurrent_ = false;
        return;
    }

    if (currentViewportSoundMuted())
    {
        player.stop(SoundPlaybackOwner::viewport);
        clearTrackedSound();
        return;
    }

    const QVector3D eye = glView_->cameraEye();
    if (viewportSoundDocumentEpoch_ != recoveryDocumentEpoch_)
    {
        viewportSoundDocumentEpoch_ = recoveryDocumentEpoch_;
        viewportSoundSector_ = -1;
        viewportSoundEyeValid_ = false;
        viewportSoundName_.clear();
        viewportSoundResourceKey_.clear();
        viewportFailedSoundResourceKey_.clear();
        viewportSoundLocatorGeneration_.reset();
        viewportSoundPlaybackCurrent_ = false;
        ++viewportSoundLoadGeneration_;
    }
    if (!viewportSoundEyeValid_
        || (viewportSoundEye_ - eye).lengthSquared() > 1.0e-10f)
    {
        viewportSoundEye_ = eye;
        viewportSoundEyeValid_ = true;
        viewportSoundSector_ =
            glView_->findContainingSector(eye, viewportSoundSector_);
    }
    const auto request = ui::audio::sectorSoundForSector(
        document_.level(), viewportSoundSector_, false);
    if (!request)
    {
        if (viewportSoundPlaybackCurrent_
            || player.isOwnedBy(SoundPlaybackOwner::viewport))
        {
            player.stop(SoundPlaybackOwner::viewport);
        }
        viewportSoundName_.clear();
        viewportSoundResourceKey_.clear();
        viewportFailedSoundResourceKey_.clear();
        viewportSoundLocatorGeneration_.reset();
        viewportSoundVolume_ = 0.0;
        viewportSoundPlaybackCurrent_ = false;
        ++viewportSoundLoadGeneration_;
        return;
    }

    std::shared_ptr<const assets::ResourceLocator> locator =
        resourceLocatorSnapshot_;
    if (!locator)
    {
        locator = makeResourceLocatorSnapshot(resources_);
    }

    // The normal update cadence is the 125 ms ambient timer; other callers
    // represent document, pipeline, or mute changes. Resolve one requested
    // sound's metadata here so live and missing-resource caches can refresh.
    const std::string requestedResourceKey =
        ui::audio::soundResourceRequestKey(*locator, request->soundName);
    const bool targetChanged = ui::audio::ambientSoundTargetChanged(
        viewportSoundName_, viewportSoundLocatorGeneration_.get(), viewportSoundResourceKey_,
        request->soundName, locator.get(), requestedResourceKey);
    if (!targetChanged
        && viewportSoundPlaybackCurrent_
        && player.isOwnedBy(SoundPlaybackOwner::viewport)
        && viewportSoundName_ == request->soundName)
    {
        if (std::abs(viewportSoundVolume_ - request->volume) > 1.0e-6)
        {
            (void)player.setPlaybackVolume(
                SoundPlaybackOwner::viewport, request->volume);
            viewportSoundVolume_ = request->volume;
        }
        return;
    }

    if (targetChanged)
    {
        viewportSoundName_ = request->soundName;
        viewportSoundResourceKey_ = requestedResourceKey;
        viewportSoundLocatorGeneration_ = locator;
        viewportFailedSoundResourceKey_.clear();
        viewportSoundPlaybackCurrent_ = false;
        ++viewportSoundLoadGeneration_;
        player.stop(SoundPlaybackOwner::viewport);
    }
    viewportSoundVolume_ = request->volume;

    // Property edits may change the ambient volume while the resource is
    // still loading. Publish the newest value with that same pending sound so
    // completion cannot briefly start it at a stale volume.
    if (viewportSoundLoadPending_)
    {
        viewportPendingSoundVolume_ = viewportSoundVolume_;
    }
    if (viewportSoundLoadPending_
        || (sectorSoundLoadWatcher_ && sectorSoundLoadWatcher_->isRunning())
        || viewportSoundResourceKey_ == viewportFailedSoundResourceKey_)
    {
        return;
    }

    // Mounted archive I/O and IJIM INDIWV conversion used to execute on the
    // GUI/audio thread exactly when fly navigation entered a new sound Sector.
    // Load immutable bytes in the worker pool and publish only the prepared
    // WAV to Qt audio on completion.
    viewportPendingSoundResourceKey_ = viewportSoundResourceKey_;
    viewportPendingSoundVolume_ = viewportSoundVolume_;
    viewportPendingSoundLoadGeneration_ = viewportSoundLoadGeneration_;
    const std::string soundName = viewportSoundName_;
    viewportSoundLoadPending_ = true;
    sectorSoundLoadWatcher_->setFuture(QtConcurrent::run(
        [locator = std::move(locator), soundName]() -> QByteArray
        {
            std::optional<std::vector<std::byte>> bytes =
                ui::audio::readSoundResource(*locator, soundName);
            if (!bytes)
            {
                return {};
            }
            if (assets::isIndyWvSound(*bytes))
            {
                bytes = assets::convertIndyWvToWav(*bytes);
                if (!bytes)
                {
                    return {};
                }
            }
            if (bytes->size()
                > static_cast<std::size_t>(std::numeric_limits<qsizetype>::max()))
            {
                return {};
            }
            return QByteArray(
                reinterpret_cast<const char*>(bytes->data()),
                static_cast<qsizetype>(bytes->size()));
        }));
}

} // namespace sed
