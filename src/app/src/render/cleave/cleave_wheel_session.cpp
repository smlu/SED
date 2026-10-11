#include "render/cleave/cleave_wheel_session.hpp"

#include "gl_view.hpp"

#include <QScopedValueRollback>

#include <algorithm>
#include <cmath>
#include <new>
#include <utility>

namespace sed::render
{

bool CleaveWheelSession::identity() const noexcept
{
    return operation_ == CleaveWheelOperation::LineRotate
            || operation_ == CleaveWheelOperation::ShapeRotate
        ? std::remainder(angleDegrees_, 360.0) == 0.0
        : scale_ == 1.0;
}

bool CleaveWheelSession::begin(
    const CleaveWheelOperation operation, const double angleStepDegrees) noexcept
{
    if ( (operation != CleaveWheelOperation::LineRotate
            && operation != CleaveWheelOperation::LineScale
            && operation != CleaveWheelOperation::ShapeRotate
            && operation != CleaveWheelOperation::ShapeScale)
        || !std::isfinite(angleStepDegrees)
        || angleStepDegrees < 0.0 || angleStepDegrees > 360.0 )
    {
        return false;
    }
    reset();
    operation_ = operation;
    angleStepDegrees_ = angleStepDegrees;
    return true;
}

bool CleaveWheelSession::accumulate(
    const double steps, const double minimumScale, const double maximumScale) noexcept
{
    if ( !active() || !std::isfinite(steps) || steps == 0.0 )
    {
        return false;
    }
    const double boundedSteps = std::clamp(steps, -1000.0, 1000.0);
    if ( operation_ == CleaveWheelOperation::LineRotate
        || operation_ == CleaveWheelOperation::ShapeRotate )
    {
        const double angleDelta = angleStepDegrees_ > 0.0
            ? std::round(boundedSteps) * angleStepDegrees_
            : boundedSteps * 5.0;
        const double angle = angleDegrees_ + angleDelta;
        if ( !std::isfinite(angle) || angle == angleDegrees_ )
        {
            return false;
        }
        angleDegrees_ = angle;
        return true;
    }
    if ( !std::isfinite(minimumScale) || !std::isfinite(maximumScale)
        || minimumScale < 0.0 || maximumScale <= 0.0
        || minimumScale > maximumScale )
    {
        return false;
    }

    const double scaleLog = std::log(1.1);
    const double minimumEventSteps = std::log(0.05) / scaleLog;
    const double maximumEventSteps = std::log(1000.0) / scaleLog;
    double scaleSteps = scaleSteps_
        + std::clamp(boundedSteps, minimumEventSteps, maximumEventSteps);
    if ( !std::isfinite(scaleSteps) )
    {
        return false;
    }
    double scale{};
    const double maximumSteps = std::log(maximumScale) / scaleLog;
    if ( scaleSteps > maximumSteps )
    {
        scale = maximumScale;
        scaleSteps = maximumSteps;
    }
    else if ( minimumScale > 0.0 && scaleSteps < std::log(minimumScale) / scaleLog )
    {
        scale = minimumScale;
        scaleSteps = std::log(minimumScale) / scaleLog;
    }
    else
    {
        scale = std::pow(1.1, scaleSteps);
    }
    if ( !std::isfinite(scale) || scale <= 0.0 || scale == scale_ )
    {
        return false;
    }
    scaleSteps_ = scaleSteps;
    scale_ = scale;
    return true;
}

void CleaveWheelSession::reset() noexcept
{
    operation_ = CleaveWheelOperation::None;
    angleStepDegrees_ = 0.0;
    angleDegrees_ = 0.0;
    scale_ = 1.0;
    scaleSteps_ = 0.0;
}

} // namespace sed::render

namespace sed
{
namespace
{

QString wheelHudNumber(const double value)
{
    const QString number = QString::number(value, 'f', 2);
    return number == QStringLiteral("-0.00") ? QStringLiteral("0.00") : number;
}

} // namespace

bool GlView::stagedCleaveWheelTargetsMatchContext() const noexcept
{
    if ( !stagedCleaveContext_.valid )
    {
        return false;
    }
    if ( stagedCleaveContext_.surfaceMode )
    {
        const SurfaceRef primary{selectedSurfaceSector_, selectedSurface_};
        if ( stagedCleaveContext_.sourceSurface != primary )
        {
            return false;
        }
        const auto& targets = stagedCleaveContext_.targetSurfaces;
        if ( highlightedSelection_.surfaces.empty() )
        {
            return selectedSurfaceSector_ >= 0 && selectedSurface_ >= 0
                && targets.size() == 1u && targets.front() == primary;
        }
        return targets.size() == highlightedSelection_.surfaces.size()
            && std::all_of(targets.begin(), targets.end(), [this](const SurfaceRef& target)
            {
                return highlightedSelection_.surfaces.contains(target);
            });
    }
    const auto& targets = stagedCleaveContext_.targetSectors;
    if ( highlightedSelection_.sectors.empty() )
    {
        return selectedSector_ >= 0 && targets.size() == 1u
            && targets.front() == selectedSector_;
    }
    return targets.size() == highlightedSelection_.sectors.size()
        && std::all_of(targets.begin(), targets.end(), [this](const std::int32_t target)
        {
            return highlightedSelection_.sectors.contains(target);
        });
}

bool GlView::applyStagedCleaveWheelTransform(const double steps)
{
    using render::CleaveWheelOperation;
    if ( !isStagedCleaveInteractionActive() || !std::isfinite(steps) || steps == 0.0
        || (stagedCleaveHistory_.gestureActive && !stagedCleaveWheelSession_.transformsLines())
        || stagedCleaveTransformMode_ == StagedCleaveTransformMode::Move )
    {
        return false;
    }
    const CleaveWheelOperation operation = stagedCleaveTransformMode_ == StagedCleaveTransformMode::Rotate
        ? CleaveWheelOperation::LineRotate : CleaveWheelOperation::LineScale;
    if ( stagedCleaveWheelSession_.active() && stagedCleaveWheelSession_.operation() != operation )
    {
        (void)finishStagedCleaveWheelSession();
    }
    if ( !canTransformSelectedStagedCleave(stagedCleaveTransformMode_)
        || !stagedCleaveWheelTargetsMatchContext() )
    {
        (void)cancelStagedCleaveWheelSession();
        showToast("Multi-cut: restore the original target selection before editing", 3000,
            ToastPlacement::BottomCenter, ToastGroup::Error);
        return false;
    }

    render::CleaveWheelSession candidate = stagedCleaveWheelSession_;
    const bool starting = !candidate.active();
    if ( starting && !candidate.begin(operation, cleaveAngleStepDegrees_) )
    {
        return false;
    }
    const bool accumulated = candidate.accumulate(steps);
    if ( !accumulated && starting )
    {
        return false;
    }
    try
    {
        if ( !accumulated )
        {
            // A valid wheel sample is still activity when Angle step rounds
            // its delta away or Scale has reached its finite bound. Reuse the
            // admitted session without another preview or history workset.
            stagedCleaveTransformHudBannerKind_ = operation
                    == CleaveWheelOperation::LineRotate
                ? ViewportBannerKind::cleaveRotate
                : ViewportBannerKind::cleaveScale;
            stagedCleaveTransformHudAnchor_ = stagedCleaveTransformDrag_.pivot;
            stagedCleaveTransformHudText_ = operation == CleaveWheelOperation::LineRotate
                ? QStringLiteral("Rotate %1°").arg(wheelHudNumber(candidate.angleDegrees()))
                : QStringLiteral("Scale %1×").arg(wheelHudNumber(candidate.scale()));
            startStagedCleaveTransformHudExpiry();
            update();
            return true;
        }
        if ( starting )
        {
            if ( !beginStagedCleaveHistoryGesture() )
            {
                showToast("Multi-cut: local history limit reached; Clear or Apply before editing", 3200,
                    ToastPlacement::BottomCenter, ToastGroup::Error);
                return false;
            }
            if ( !prepareStagedCleaveGroupTransform() )
            {
                (void)restoreAndDiscardStagedCleaveHistoryGesture();
                return false;
            }
        }
        stagedCleaveWheelSession_ = candidate;
        stagedCleaveTransformHudBannerKind_ = operation
                == CleaveWheelOperation::LineRotate
            ? ViewportBannerKind::cleaveRotate
            : ViewportBannerKind::cleaveScale;
        if ( candidate.identity() )
        {
            // Avoid trigonometric/normal refresh roundoff when an inverse
            // session or a full turn returns to the exact authored baseline.
            const auto& baseline = stagedCleaveGestureBaselineLines();
            for ( const StagedCleaveLineMove& affected : stagedCleaveTransformDrag_.affectedLines )
            {
                if ( affected.line < 0 || static_cast<std::size_t>(affected.line) >= baseline.size()
                    || static_cast<std::size_t>(affected.line) >= stagedCleaveLines_.size() )
                {
                    (void)cancelStagedCleaveWheelSession();
                    return false;
                }
            }
            for ( const StagedCleaveLineMove& affected : stagedCleaveTransformDrag_.affectedLines )
            {
                const auto index = static_cast<std::size_t>(affected.line);
                stagedCleaveLines_[index] = baseline[index];
            }
            ++stagedCleaveJointGuidePreviewRevision_;
            stagedCleaveTransformDrag_.appliedAngleDegrees = candidate.angleDegrees();
            stagedCleaveTransformDrag_.appliedScale = candidate.scale();
            stagedCleaveTransformHudAnchor_ = stagedCleaveTransformDrag_.pivot;
            stagedCleaveTransformHudText_ = operation == CleaveWheelOperation::LineRotate
                ? QStringLiteral("Rotate %1°").arg(wheelHudNumber(candidate.angleDegrees()))
                : QStringLiteral("Scale %1×").arg(wheelHudNumber(candidate.scale()));
            startStagedCleaveTransformHudExpiry();
        }
        else if ( !updateStagedCleaveGroupTransform(candidate.angleDegrees(), candidate.scale()) )
        {
            (void)cancelStagedCleaveWheelSession();
            return false;
        }
        if ( starting )
        {
            // Expose cancellation through local Undo once, without making
            // each subsequent preview sample a viewport/history publication.
            Q_EMIT viewportStateChanged();
        }
        update();
        return true;
    }
    catch ( const std::bad_alloc& )
    {
        (void)cancelStagedCleaveWheelSession();
        // The baseline may have been admitted before the scalar owner was
        // published. Roll it back even if preparation allocation failed.
        (void)restoreAndDiscardStagedCleaveHistoryGesture();
        stagedCleaveTransformDrag_ = {};
        showToast("Multi-cut: this wheel edit could not be retained; the draft was restored", 3200,
            ToastPlacement::BottomCenter, ToastGroup::Error);
        return false;
    }
}

bool GlView::applyStagedCleaveShapeWheelTransform(
    const double steps, const bool rotate, const QPoint& position,
    const Qt::KeyboardModifiers modifiers)
{
    using render::CleaveWheelOperation;
    if ( !isStagedCleaveInteractionActive() || !cleaveShapeOptions_.placementArmed
        || stagedCleaveHistory_.gestureActive || !std::isfinite(steps) || steps == 0.0 )
    {
        return false;
    }
    const CleaveWheelOperation operation = rotate
        ? CleaveWheelOperation::ShapeRotate : CleaveWheelOperation::ShapeScale;
    if ( stagedCleaveWheelSession_.active() && stagedCleaveWheelSession_.operation() != operation )
    {
        (void)finishStagedCleaveWheelSession();
    }
    render::CleaveWheelSession candidate = stagedCleaveWheelSession_;
    const bool starting = !candidate.active();
    const RecoveryState::CleaveShapeOptions baseline = starting
        ? cleaveShapeOptions_ : stagedCleaveWheelShapeBefore_.value_or(cleaveShapeOptions_);
    if ( starting && !candidate.begin(operation, cleaveAngleStepDegrees_) )
    {
        return false;
    }
    const double minimumScale = std::max(0.005 / baseline.width, 0.005 / baseline.height);
    const double maximumScale = std::min(10000.0 / baseline.width, 10000.0 / baseline.height);
    const bool accumulated = candidate.accumulate(steps, minimumScale, maximumScale);
    if ( !accumulated && starting )
    {
        return false;
    }

    try
    {
        if ( !accumulated )
        {
            // Clamping/quantization can keep the options unchanged while the
            // same Ctrl-held session restores its idle readout.
            stagedCleaveTransformHudBannerKind_ = rotate
                ? ViewportBannerKind::cleaveRotate
                : ViewportBannerKind::cleaveScale;
            stagedCleaveTransformHudAnchor_ = stagedCleaveShapePreviewFrame_
                ? std::optional<TVector>(stagedCleaveShapePreviewFrame_->center) : std::nullopt;
            stagedCleaveTransformHudText_ = rotate
                ? QStringLiteral("Rotate %1°").arg(wheelHudNumber(candidate.angleDegrees()))
                : QStringLiteral("Scale %1×").arg(wheelHudNumber(candidate.scale()));
            startStagedCleaveTransformHudExpiry();
            update();
            return true;
        }
        if ( starting )
        {
            clearStagedCleaveTransformHud();
            stagedCleaveWheelShapeBefore_ = baseline;
        }
        stagedCleaveWheelSession_ = candidate;
        stagedCleaveTransformHudBannerKind_ = rotate
            ? ViewportBannerKind::cleaveRotate
            : ViewportBannerKind::cleaveScale;
        RecoveryState::CleaveShapeOptions options = cleaveShapeOptions_;
        if ( rotate )
        {
            options.rotationDegrees = baseline.rotationDegrees + candidate.angleDegrees();
        }
        else
        {
            options.width = baseline.width * candidate.scale();
            options.height = baseline.height * candidate.scale();
        }
        const QScopedValueRollback<bool> applying(stagedCleaveWheelShapeOptionsApplying_, true);
        setCleaveShapeOptions(options);
        lastMousePos_ = position;
        stagedCleaveShapePreviewFrame_ = stagedCleaveShapePlacementFrameAt(position, modifiers);
        stagedCleaveShapePreviewCenter_ = stagedCleaveShapePreviewFrame_
            ? std::optional<QVector3D>(render::cleaveRenderPoint(stagedCleaveShapePreviewFrame_->center))
            : std::nullopt;
        stagedCleaveTransformHudAnchor_ = stagedCleaveShapePreviewFrame_
            ? std::optional<TVector>(stagedCleaveShapePreviewFrame_->center) : std::nullopt;
        stagedCleaveTransformHudText_ = rotate
            ? QStringLiteral("Rotate %1°").arg(wheelHudNumber(candidate.angleDegrees()))
            : QStringLiteral("Scale %1×").arg(wheelHudNumber(candidate.scale()));
        startStagedCleaveTransformHudExpiry();
        update();
        return true;
    }
    catch ( const std::bad_alloc& )
    {
        (void)cancelStagedCleaveWheelSession();
        showToast("Multi-cut: this shape wheel edit could not be retained; its options were restored", 3200,
            ToastPlacement::BottomCenter, ToastGroup::Error);
        return false;
    }
}

bool GlView::finishStagedCleaveWheelSession()
{
    if ( !stagedCleaveWheelSession_.active() )
    {
        return false;
    }
    const bool lines = stagedCleaveWheelSession_.transformsLines();
    stagedCleaveWheelSession_.reset();
    stagedCleaveWheelShapeBefore_.reset();
    if ( lines )
    {
        // Release sparse preview capacity before the one final history copy,
        // while retaining the admitted exact rollback baseline until it succeeds.
        stagedCleaveTransformDrag_ = {};
        if ( stagedCleaveWheelTargetsMatchContext() )
        {
            (void)finishStagedCleaveHistoryGesture();
        }
        else
        {
            (void)restoreAndDiscardStagedCleaveHistoryGesture();
        }
    }
    // The last HUD sample owns the idle deadline; completion cannot extend it.
    Q_EMIT viewportStateChanged();
    update();
    return true;
}

bool GlView::cancelStagedCleaveWheelSession()
{
    if ( !stagedCleaveWheelSession_.active() )
    {
        return false;
    }
    const bool lines = stagedCleaveWheelSession_.transformsLines();
    const auto shapeBefore = std::move(stagedCleaveWheelShapeBefore_);
    stagedCleaveWheelSession_.reset();
    stagedCleaveWheelShapeBefore_.reset();
    if ( lines )
    {
        stagedCleaveTransformDrag_ = {};
        (void)restoreAndDiscardStagedCleaveHistoryGesture();
    }
    else if ( shapeBefore )
    {
        const QScopedValueRollback<bool> applying(stagedCleaveWheelShapeOptionsApplying_, true);
        setCleaveShapeOptions(*shapeBefore);
    }
    clearStagedCleaveTransformHud();
    Q_EMIT viewportStateChanged();
    update();
    return true;
}

bool GlView::handleStagedCleaveWheelModifierRelease(const int key, const bool autoRepeat)
{
    if ( key != Qt::Key_Control )
    {
        return false;
    }
    const bool ownsRelease = stagedCleaveWheelSession_.active();
    if ( autoRepeat )
    {
        return ownsRelease;
    }
    if ( ownsRelease )
    {
        (void)finishStagedCleaveWheelSession();
    }
    if ( ownsRelease || (!stagedCleaveTransformDrag_.active
            && !stagedCleaveHistory_.gestureActive
            && !stagedCleaveTransformHudText_.isEmpty()) )
    {
        clearStagedCleaveTransformHud();
        update();
    }
    // A completed readout owns presentation only. Its release must still
    // reach native editors and other input owners; a live wheel owns the key.
    return ownsRelease;
}

} // namespace sed
