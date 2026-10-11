#include "gl_view_common.hpp"

#include "app_settings.hpp"

#include "sed/assets/3do.hpp"

#include "render/cleave/closed_cleave_loop_utils.hpp"
#include "render/gizmos/light_category_gizmo_utils.hpp"
#include "render/mesh/render_lighting_utils.hpp"
#include "render/mesh/thing_render_geometry_utils.hpp"
#include "render/overlays/thing_shape_overlay_utils.hpp"
#include "render/picking/viewport_pick_cycle_utils.hpp"
#include "render/selection/surface_selection_utils.hpp"
#include "ui/components/preview_resource_disk_watcher.hpp"
#include "ui/components/ui_decimal_format.hpp"
#include "ui/shortcuts/shortcut_context_utils.hpp"
#include "ui/shortcuts/selection_modifier_utils.hpp"
#include "ui/viewport/viewport_orbit_utils.hpp"

#include <QDebug>
#include <QFontMetrics>
#include <QLineEdit>
#include <QPointer>
#include <QQuaternion>
#include <QSignalBlocker>
#include <QScopeGuard>
#include <QSettings>
#include <QSurface>

#include <limits>
#include <new>
#include <numeric>

namespace sed
{


namespace
{

GlView::CleaveMeasureUnit normalizedCleaveMeasureUnit(
    const GlView::CleaveMeasureUnit unit,
    const Level* const level) noexcept
{
    using Unit = GlView::CleaveMeasureUnit;
    if ( unit != Unit::Bare && unit != Unit::Meters
        && unit != Unit::JKU && unit != Unit::IJU )
    {
        return Unit::Meters;
    }
    if ( level && ((unit == Unit::JKU && level->kind == ProjectType::IJIM)
        || (unit == Unit::IJU && level->kind != ProjectType::IJIM)) )
    {
        return Unit::Meters;
    }
    return unit;
}

std::size_t addCleaveRecoveryOwnedArrayBytes(
    const std::size_t total,
    const std::size_t count,
    const std::size_t elementBytes) noexcept
{
    const std::size_t limit = std::numeric_limits<std::size_t>::max();
    if (elementBytes != 0u && count > (limit - total) / elementBytes)
    {
        return limit;
    }
    return total + count * elementBytes;
}

template <typename T>
std::size_t addCleaveRecoveryVectorCapacityBytes(
    const std::size_t total,
    const std::vector<T>& values) noexcept
{
    return addCleaveRecoveryOwnedArrayBytes(
        total, values.capacity(), sizeof(T));
}

std::size_t cleaveRecoveryDraftOwnedArrayBytes(
    const GlView::RecoveryState::CleaveDraftSnapshot& snapshot) noexcept
{
    std::size_t bytes = addCleaveRecoveryVectorCapacityBytes(
        0u, snapshot.lines);
    bytes = addCleaveRecoveryVectorCapacityBytes(
        bytes, snapshot.context.targetSectors);
    bytes = addCleaveRecoveryVectorCapacityBytes(
        bytes, snapshot.context.targetSurfaces);
    bytes = addCleaveRecoveryVectorCapacityBytes(
        bytes, snapshot.applyOrder);
    return addCleaveRecoveryVectorCapacityBytes(
        bytes, snapshot.selectedCleaveLines);
}

std::size_t cleaveRecoveryHistoryOwnedArrayBytes(
    const GlView::RecoveryState::CleaveHistory& history) noexcept
{
    std::size_t bytes = addCleaveRecoveryVectorCapacityBytes(
        0u, history.snapshots);
    for (const GlView::RecoveryState::CleaveDraftSnapshot& snapshot : history.snapshots)
    {
        bytes = addCleaveRecoveryOwnedArrayBytes(
            bytes,
            cleaveRecoveryDraftOwnedArrayBytes(snapshot),
            1u);
    }
    return bytes;
}

std::size_t cleaveRecoveryStateOwnedArrayBytes(
    const GlView::RecoveryState& state) noexcept
{
    std::size_t bytes = addCleaveRecoveryVectorCapacityBytes(
        0u, state.cleaveLines);
    bytes = addCleaveRecoveryVectorCapacityBytes(
        bytes, state.cleaveContext.targetSectors);
    bytes = addCleaveRecoveryVectorCapacityBytes(
        bytes, state.cleaveContext.targetSurfaces);
    bytes = addCleaveRecoveryVectorCapacityBytes(
        bytes, state.applyOrder);
    bytes = addCleaveRecoveryVectorCapacityBytes(
        bytes, state.selectedCleaveLines);
    if (state.cleaveHistory)
    {
        bytes = addCleaveRecoveryOwnedArrayBytes(
            bytes,
            cleaveRecoveryHistoryOwnedArrayBytes(*state.cleaveHistory),
            1u);
    }
    if ( state.cleavePeerWorkspace )
    {
        bytes = addCleaveRecoveryOwnedArrayBytes(bytes,
            cleaveRecoveryDraftOwnedArrayBytes(state.cleavePeerWorkspace->draft), 1u);
        if ( state.cleavePeerWorkspace->history )
        {
            bytes = addCleaveRecoveryOwnedArrayBytes(bytes,
                cleaveRecoveryHistoryOwnedArrayBytes(*state.cleavePeerWorkspace->history), 1u);
        }
    }
    return bytes;
}

template <typename Draft>
std::size_t cleaveRecoveryDraftPreparedArrayBytes(const Draft& draft) noexcept
{
    const auto& lines = [&]() -> const auto&
    {
        if constexpr ( requires { draft.cleaveLines; } ) return draft.cleaveLines;
        else return draft.lines;
    }();
    const auto& context = [&]() -> const auto&
    {
        if constexpr ( requires { draft.cleaveContext; } ) return draft.cleaveContext;
        else return draft.context;
    }();
    const auto& selected = draft.selectedCleaveLines;
    std::size_t bytes = addCleaveRecoveryOwnedArrayBytes(
        0u, lines.size(), sizeof(GlView::RecoveryState::CleaveLine));
    bytes = addCleaveRecoveryOwnedArrayBytes(bytes, context.targetSectors.size(), sizeof(std::int32_t));
    bytes = addCleaveRecoveryOwnedArrayBytes(bytes, context.targetSurfaces.size(), sizeof(SurfaceRef));
    bytes = addCleaveRecoveryOwnedArrayBytes(bytes, lines.size(), sizeof(std::int32_t));
    return addCleaveRecoveryOwnedArrayBytes(bytes,
        selected.size(), sizeof(std::int32_t));
}

std::size_t cleaveRecoveryHistoryPreparedArrayBytes(
    const std::optional<GlView::RecoveryState::CleaveHistory>& history) noexcept
{
    if ( !history ) return 0u;
    std::size_t bytes = addCleaveRecoveryOwnedArrayBytes(0u,
        history->snapshots.size(), sizeof(GlView::RecoveryState::CleaveDraftSnapshot));
    for ( const auto& snapshot : history->snapshots )
    {
        bytes = addCleaveRecoveryOwnedArrayBytes(bytes,
            cleaveRecoveryDraftPreparedArrayBytes(snapshot), 1u);
    }
    return bytes;
}

std::size_t cleaveRecoveryStatePreparedArrayBytes(
    const GlView::RecoveryState& state) noexcept
{
    std::size_t bytes = addCleaveRecoveryOwnedArrayBytes(
        cleaveRecoveryDraftPreparedArrayBytes(state),
        cleaveRecoveryHistoryPreparedArrayBytes(state.cleaveHistory), 1u);
    if ( state.cleavePeerWorkspace )
    {
        bytes = addCleaveRecoveryOwnedArrayBytes(bytes,
            cleaveRecoveryDraftPreparedArrayBytes(state.cleavePeerWorkspace->draft), 1u);
        bytes = addCleaveRecoveryOwnedArrayBytes(bytes,
            cleaveRecoveryHistoryPreparedArrayBytes(state.cleavePeerWorkspace->history), 1u);
    }
    return bytes;
}

/**
 * Uploading a sparse ARCH overlay can happen from a picker callback while a
 * different QOpenGLWidget owns the current context. Keep that caller context
 * (and its context-local FBO binding) intact while this view's buffers are
 * updated. If this view is already current, avoid the make/done pair entirely
 * so its active caller FBO is untouched.
 */
class ScopedArchitecturalOpenGlContext final
{
public:
    explicit ScopedArchitecturalOpenGlContext(GlView& view)
        : view_(view),
          previousContext_(QOpenGLContext::currentContext()),
          previousSurface_(previousContext_ ? previousContext_->surface() : nullptr)
    {
        if (previousContext_.data() == view_.context())
        {
            return;
        }

        view_.makeCurrent();
        acquired_ = QOpenGLContext::currentContext() == view_.context();
    }

    ~ScopedArchitecturalOpenGlContext()
    {
        if (!acquired_)
        {
            return;
        }

        view_.doneCurrent();
        if (previousContext_ && previousSurface_)
        {
            previousContext_->makeCurrent(previousSurface_);
        }
    }

    ScopedArchitecturalOpenGlContext(const ScopedArchitecturalOpenGlContext&) = delete;
    ScopedArchitecturalOpenGlContext& operator=(const ScopedArchitecturalOpenGlContext&) = delete;

    [[nodiscard]] bool ready() const noexcept
    {
        return QOpenGLContext::currentContext() == view_.context();
    }

private:
    GlView& view_;
    QPointer<QOpenGLContext> previousContext_{};
    QSurface* previousSurface_{};
    bool acquired_{};
};

/**
 * Leaves a small border around orthographic level fits so long/thin maps do
 * not land exactly on the viewport edge after load or manual fit.
 */
constexpr float kOrthographicLevelFitPaddingScale = 1.15f;

bool selectionEquals(const editor::Selection& a, const editor::Selection& b) noexcept
{
    return a.sectors == b.sectors
        && a.surfaces == b.surfaces
        && a.surfaceVertices == b.surfaceVertices
        && a.things == b.things
        && a.frames == b.frames
        && a.lights == b.lights
        && a.cogs == b.cogs;
}

bool computeLevelDataBounds(const Level* level, QVector3D& minv, QVector3D& maxv) noexcept
{
    if (!level)
    {
        return false;
    }

    bool haveAny = false;
    auto includePoint = [&](const QVector3D& point)
    {
        if (!std::isfinite(point.x()) || !std::isfinite(point.y()) || !std::isfinite(point.z()))
        {
            return;
        }

        if (!haveAny)
        {
            minv = point;
            maxv = point;
            haveAny = true;
            return;
        }

        minv.setX(std::min(minv.x(), point.x()));
        minv.setY(std::min(minv.y(), point.y()));
        minv.setZ(std::min(minv.z(), point.z()));
        maxv.setX(std::max(maxv.x(), point.x()));
        maxv.setY(std::max(maxv.y(), point.y()));
        maxv.setZ(std::max(maxv.z(), point.z()));
    };

    bool havePrimarySurfaceVertices = false;
    for (const Sector& sector : level->sectors)
    {
        std::vector<bool> usedVertices(sector.vertices.size(), false);
        for (const Surface& surface : sector.surfaces)
        {
            if ((surface.surfaceFlags & (SF_SkyHorizon | SF_SkyCeiling)) != 0)
            {
                continue;
            }

            for (const std::int32_t vertexIndex : surface.vertices)
            {
                if (vertexIndex >= 0 && static_cast<std::size_t>(vertexIndex) < usedVertices.size())
                {
                    usedVertices[static_cast<std::size_t>(vertexIndex)] = true;
                }
            }
        }

        for (std::size_t vertexIndex = 0; vertexIndex < usedVertices.size(); ++vertexIndex)
        {
            if (!usedVertices[vertexIndex])
            {
                continue;
            }

            const Vertex& vertex = sector.vertices[vertexIndex];
            includePoint(QVector3D(
                static_cast<float>(vertex.x),
                static_cast<float>(vertex.y),
                static_cast<float>(vertex.z)));
            havePrimarySurfaceVertices = true;
        }
    }
    if (havePrimarySurfaceVertices)
    {
        return haveAny;
    }

    for (const Sector& sector : level->sectors)
    {
        for (const Vertex& vertex : sector.vertices)
        {
            includePoint(QVector3D(
                static_cast<float>(vertex.x),
                static_cast<float>(vertex.y),
                static_cast<float>(vertex.z)));
        }
    }

    for (const Thing& thing : level->things)
    {
        const QVector3D origin(
            static_cast<float>(thing.x),
            static_cast<float>(thing.y),
            static_cast<float>(thing.z));
        if (thing.bbox.IsZero())
        {
            includePoint(origin);
            continue;
        }

        const QVector3D bboxMin(
            static_cast<float>(std::min(thing.bbox.x1, thing.bbox.x2)),
            static_cast<float>(std::min(thing.bbox.y1, thing.bbox.y2)),
            static_cast<float>(std::min(thing.bbox.z1, thing.bbox.z2)));
        const QVector3D bboxMax(
            static_cast<float>(std::max(thing.bbox.x1, thing.bbox.x2)),
            static_cast<float>(std::max(thing.bbox.y1, thing.bbox.y2)),
            static_cast<float>(std::max(thing.bbox.z1, thing.bbox.z2)));

        includePoint(origin + bboxMin);
        includePoint(origin + bboxMax);
    }

    return haveAny;
}

} // namespace

GlView::GlView(QWidget* parent) : QOpenGLWidget(parent)
{
    const QSettings settings = settingsStore();
    viewportNavigationSettings_ = loadViewportNavigationSettings(settings);
    viewportBannerSettings_ = loadViewportBannerSettings(settings);
    setMinimumSize(320, 240);
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
    ui::onUiNumericFormatChanged(this, [this]() { update(); });

    camera_.Reset();
    camera_.SetProjectionType(app::ViewportCamera::ProjectionType::Orthographic);
    camera_.SetOrthoScale(64.0f);
    camera_.SetNearFar(app::ViewportCamera::kOrthoNearPlaneDefault, app::ViewportCamera::kOrthoFarPlaneDefault);

    flyCursor_ = loadEyeCursor();
    flyTimer_ = std::make_unique<QTimer>(this);
    flyTimer_->setInterval(16);
    flyTimer_->setTimerType(Qt::PreciseTimer);
    connect(flyTimer_.get(), &QTimer::timeout, this, &GlView::tickFlyNavigation);

    stagedCleaveTransformHudHideTimer_ = std::make_unique<QTimer>(this);
    stagedCleaveTransformHudHideTimer_->setObjectName("cleave_transform_hud_hide_timer");
    stagedCleaveTransformHudHideTimer_->setSingleShot(true);
    stagedCleaveTransformHudHideTimer_->setInterval(viewportBannerDurationMs(
        viewportBannerSettings_, ViewportBannerKind::cleaveScale, 400));
    stagedCleaveTransformHudHideTimer_->setTimerType(Qt::PreciseTimer);
    connect(stagedCleaveTransformHudHideTimer_.get(), &QTimer::timeout, this, [this]()
    {
        // A real single-shot timeout stops the timer before delivery. An
        // obsolete callback cannot hide a sample with a newer live deadline.
        if ( stagedCleaveTransformHudHideTimer_->isActive() )
        {
            return;
        }
        // Idle expiry owns presentation only; the wheel/pointer session and
        // its exact rollback baseline remain admitted until release/cancel.
        clearStagedCleaveTransformHud();
        update();
    });

    stagedCleaveTransformHudPresentationTimer_ = std::make_unique<QTimer>(this);
    stagedCleaveTransformHudPresentationTimer_->setObjectName(
        "cleave_transform_hud_presentation_timer");
    stagedCleaveTransformHudPresentationTimer_->setSingleShot(true);
    stagedCleaveTransformHudPresentationTimer_->setTimerType(Qt::PreciseTimer);
    connect(stagedCleaveTransformHudPresentationTimer_.get(), &QTimer::timeout,
        this, [this]()
        {
            if ( stagedCleaveTransformHudText_.isEmpty() )
            {
                return;
            }
            const auto now = render::cleave::CleaveHudPresentation::Clock::now();
            if ( !stagedCleaveTransformHudPresentation_.activeAt(now) )
            {
                // The hide timer owns clearing the text; this wake only makes
                // sure a late hide delivery cannot leave a visible last frame.
                update();
                return;
            }
            update();
            scheduleStagedCleaveTransformHudPresentationWake();
        });

    toastTimer_ = std::make_unique<QTimer>(this);
    toastTimer_->setInterval(16);
    toastTimer_->setTimerType(Qt::PreciseTimer);
    connect(toastTimer_.get(), &QTimer::timeout, this, [this]()
        {
            const auto now = std::chrono::steady_clock::now();

            bool anyActive = false;
            bool clearedExpiredToast = false;
            for ( ToastState& t : toastStates_ )
            {
                if ( t.text.isEmpty() )
                {
                    continue;
                }

                if ( now >= t.expires )
                {
                    t.text.clear();
                    clearedExpiredToast = true;
                    continue;
                }

                anyActive = true;
            }

            if ( !anyActive )
            {
                toastTimer_->stop();
                if ( clearedExpiredToast )
                {
                    update();
                }
                return;
            }

            update();
        });

    viewAnimTimer_ = std::make_unique<QTimer>(this);
    viewAnimTimer_->setInterval(16);
    viewAnimTimer_->setTimerType(Qt::PreciseTimer);
    connect(viewAnimTimer_.get(), &QTimer::timeout, this, &GlView::tickViewAnimation);

    resourceDiskWatcher_ = new ui::detail::PreviewResourceDiskWatcher(
        this,
        [this]()
        {
            reloadCachedResources();
            Q_EMIT resourceFilesChanged();
            update();
        });
    resourceDiskWatcher_->setArchiveChangeCallback(
        [this]()
        {
            // ResourceLocator ownership stays outside GlView.  The owner
            // transaction must complete before observers see the archive
            // signal; a false result leaves the watcher event pending for a
            // backoff retry.
            if ( resourceArchiveReloadCallback_ )
            {
                const bool reloaded = resourceArchiveReloadCallback_();
                if ( reloaded )
                {
                    Q_EMIT resourceArchivesChanged();
                }
                return reloaded;
            }

            // A mounted archive needs an owner that can rebuild the
            // ResourceLocator.  Do not claim success or notify observers when
            // that owner has been disconnected; the watcher will retain the
            // event and retry after its bounded backoff.
            return false;
        });
}

GlView::~GlView()
{
    QObject::disconnect(contextCleanupConnection_);
    contextCleanupConnection_ = {};
    if ( context() && context()->isValid() )
    {
        makeCurrent();
        if ( QOpenGLContext::currentContext() == context() )
        {
            releaseOpenGlResources();
            doneCurrent();
        }
        else
        {
            deferOpenGlResourceCleanup();
        }
    }
    else
    {
        deferOpenGlResourceCleanup();
    }

    if ( glResourcesContextLost_ )
    {
        QOpenGLContext* const previousContext = QOpenGLContext::currentContext();
        QSurface* const previousSurface = previousContext
            ? previousContext->surface()
            : nullptr;
        if ( previousContext )
        {
            previousContext->doneCurrent();
        }
        releaseOpenGlResources();
        if ( previousContext && previousContext != context()
            && previousSurface && previousContext->isValid() )
        {
            previousContext->makeCurrent(previousSurface);
        }
    }
}

void GlView::deferOpenGlResourceCleanup() noexcept
{
    // The owning context is already unavailable, so its handles will be
    // reclaimed by context destruction. Never submit those numeric names to
    // whichever unrelated context may currently be active.
    dynamicLightUniformBuffer_ = 0;
    dynamicLightUniformBufferReady_ = false;
    dynamicLightUniformBufferWarningEmitted_ = false;
    glReady_ = false;
    glResourcesContextLost_ = true;
}

void GlView::releaseOpenGlResources() noexcept
{
    releaseDynamicLightUniformBuffer();
    releaseFrameGhostOpenGlResources();

    // TextureInfo and these named textures own GL handles. Prepared sources,
    // parsed resources, and all renderer/editor CPU state remain available for
    // the next context's normal upload path.
    textures_.clear();
    gizmoTextures_.clear();
    lightTexture_.reset();
    thingLightTexture_.reset();
    thingHeadLightTexture_.reset();
    sectorLightTexture_.reset();
    architecturalLightingTexture_.reset();
    archThingGizmoTexture_.reset();
    surfaceWireColorTexture_.reset();
    surfaceWireColorTextureLayerCount_ = -1;
    surfaceWireColorTextureDirty_ = true;
    surfaceWireColorMaxTextureSize_ = 0;

    pickFbo_.reset();
    pickProgram_.reset();
    program_.reset();
    thickLineProgram_.reset();
    gridProgram_.reset();
    lineVao_.reset();
    lineVbo_.reset();
    highlightLineVao_.reset();
    highlightLineVbo_.reset();
    gridQuadVao_.reset();
    gridLineVao_.reset();
    gridLineVbo_.reset();
    triVao_.reset();
    triVbo_.reset();
    translucentTriVao_.reset();
    translucentTriVbo_.reset();
    thingGizmoTriVao_.reset();
    thingGizmoTriVbo_.reset();
    thingGizmoPickTriVao_.reset();
    thingGizmoPickTriVbo_.reset();
    lightGizmoTriVao_.reset();
    lightGizmoTriVbo_.reset();
    spriteTriVao_.reset();
    spriteTriVbo_.reset();
    outlineTriVao_.reset();
    outlineTriVbo_.reset();
    pointVao_.reset();
    pointVbo_.reset();
    highlightPointVao_.reset();
    highlightPointVbo_.reset();
    architecturalLightingPointVao_.reset();
    architecturalLightingPointVbo_.reset();
    transformGizmoLineVao_.reset();
    transformGizmoLineVbo_.reset();

    const auto resetCapacity = [](int& capacity)
    {
        capacity = 0;
    };
    resetCapacity(triVboAllocatedBytes_);
    resetCapacity(lineVboAllocatedBytes_);
    resetCapacity(pointVboAllocatedBytes_);
    resetCapacity(translucentTriVboAllocatedBytes_);
    resetCapacity(spriteTriVboAllocatedBytes_);
    resetCapacity(highlightLineVboAllocatedBytes_);
    resetCapacity(outlineTriVboAllocatedBytes_);
    resetCapacity(highlightPointVboAllocatedBytes_);
    resetCapacity(architecturalLightingPointVboAllocatedBytes_);
    resetCapacity(thingGizmoTriVboAllocatedBytes_);
    resetCapacity(thingGizmoPickTriVboAllocatedBytes_);
    resetCapacity(lightGizmoTriVboAllocatedBytes_);
    resetCapacity(transformGizmoLineVboAllocatedBytes_);
    resetCapacity(gridLineVboAllocatedBytes_);

    pickFboWidth_ = 0;
    pickFboHeight_ = 0;
    thingOverlayMultiDrawArraysFunction_ = nullptr;
    dynamicLightUniformBufferWarningEmitted_ = false;
    thingTypeGizmoCacheValid_ = false;
    architecturalLightingOwnerGizmoCacheValid_ = false;
    gridCacheValid_ = false;
    alphaAdjoinWireSupportsDirty_ = true;
    glReady_ = false;
    glResourcesContextLost_ = false;
}

void GlView::setLevel(const Level* level, const SetLevelMode mode)
{
    setLevel(level, mode, templates_, true);
}

void GlView::setLevel(
    const Level* level,
    const SetLevelMode mode,
    const assets::Templates* templates)
{
    setLevel(level, mode, templates, true);
}

void GlView::setLevel(
    const Level* level,
    const SetLevelMode mode,
    const assets::Templates* templates,
    const bool rebuildRenderCache)
{
    if (level != level_ || !level)
    {
        (void)cancelStagedCleaveWheelSession();
        cancelViewportMouseGesture();
    }
    pendingArchitecturalLightingRecovery_.reset();
    pendingArchitecturalLightingSelectionReady_ = false;
    pendingArchitecturalLightingOwnerSelection_.clear();
    // A reused Level address can contain a restored frame list, and the
    // template publication can change its effective model source. Frame pose
    // and model identities therefore follow every complete Level publish.
    invalidateAllFrameRenderCaches();

    // A staged ARCH edit is tied to the currently published level/model
    // buffers. Restore any sparse GPU patches before replacing or rebuilding
    // that publication, even when the caller reuses the same Level address
    // for an in-place document restore.
    clearArchitecturalLightingPreview(false);
    resetPickCycle();
    // Exact edge refs are transient viewport state and are deliberately not
    // carried across document publication/restoration. The same Level object
    // address can be repopulated in-place, so pointer comparison is not a
    // sufficient stale-reference guard here.
    clearSurfaceEdgeSelection();
    activeSurfaceVertexHintValid_ = false;
    activeSurfaceVertexHint_ = {};
    const Level* prevLevel = level_;
    const std::uint64_t previousCleaveRevision = stagedCleaveAuthoredRevision_;
    const bool keepView = mode == SetLevelMode::keepView;
    if (!keepView)
    {
        (void)finishStagedCleaveWheelSession();
    }
    const QVector3D oldTarget = camera_.orbit.target();
    const float oldYawDeg = camera_.orbit.yawDeg;
    const float oldPitchDeg = camera_.orbit.pitchDeg;
    const float oldDistance = camera_.orbit.distance;
    // The optional template pointer is published in the same transaction as
    // the level.  Startup uses this overload after loading resources and the
    // global template database, preventing a mesh build against the previous
    // document/template set.
    templates_ = templates;
    level_ = level;
    const CleaveMeasureUnit previousCleaveUnit = cleaveMeasureUnit_;
    cleaveMeasureUnit_ = normalizedCleaveMeasureUnit(cleaveMeasureUnit_, level_);
    bool peerUnitChanged = false;
    if ( stagedCleavePeerWorkspace_ )
    {
        const auto previousUnit = stagedCleavePeerWorkspace_->measureUnit;
        stagedCleavePeerWorkspace_->measureUnit = normalizedCleaveMeasureUnit(previousUnit, level_);
        peerUnitChanged = previousUnit != stagedCleavePeerWorkspace_->measureUnit;
    }
    const bool pointerChanged = prevLevel != level_;
    if (pointerChanged || level == nullptr)
    {
        architecturalLightingLastOwnerClick_.reset();
    }
    if (pointerChanged || level_ == nullptr)
    {
        thingCollisionOverlayLineRanges_.clear();
        thingBroadPhaseTreeGuideLineRanges_.clear();
        thingVisualBoundsOverlayLineRanges_.clear();
        thingTemplateCollisionBoundsGuideLineRanges_.clear();
        thingOverlayGeometryPrepared_ =
            showThingCollisionShapes_ || showThingVisualBounds_;
        classicCameraSector_ = -1;
        {
            const QSignalBlocker settledPublication(this);
            resetStagedCleaveWorkspaces();
        }
        previewState_.initialized = false;
        levelMeshCache_.valid = false;
        thingMeshCache_.valid = false;
        placementPreviewSector_.reset();
        placementPreviewSourcePatch_.reset();
        placementPreviewHiddenSurface_.reset();
        placementPreviewPersistent_ = false;
        selectedSectorThrusts_.clear();
        hoveredSectorThrust_ = -1;
        sectorThrustDrag_ = {};
        sectorThrustLabels_.clear();
    }
    else
    {
        // Rebuilding the same document does not end its world-authored draft
        // or local timeline. New/Open explicitly reset the document workspace.
        if (!stagedCleaveWheelSession_.active())
        {
            suspendStagedCleaveInteraction();
        }
        validateRetainedStagedCleaveReferences();
    }
    if (!keepView)
    {
        selectedSector_ = -1;
        selectedSurfaceSector_ = -1;
        selectedSurface_ = -1;
        selectedThing_ = -1;
        selectedLight_ = -1;
        highlightedSelection_.clear();
        selectedSectorThrusts_.clear();
        hoveredSectorThrust_ = -1;
        sectorThrustDrag_ = {};
        sectorThrustLabels_.clear();
    }
    if (rebuildRenderCache)
    {
        rebuildMeshes(pointerChanged);
    }
    else
    {
        // The caller is staging a complete editor-state restore.  Invalidate
        // every derived mesh/range cache now, then let it publish exactly one
        // final notifyAllMeshesChanged() after the viewport state has been
        // applied.  Keeping stale caches here would let selection/overlay
        // code observe geometry from the previous document.  The containment
        // and surface-pick broad phases are derived from the Level object too;
        // reset them even when document_ reuses the same Level address (for
        // example, clear()/restore mutates it in place with the same sector
        // count).  A source pointer or count check alone cannot detect that
        // replacement.
        levelMeshCache_.valid = false;
        thingMeshCache_.valid = false;
        spriteTrianglesDirty_ = true;
        levelPeelCacheValid_ = false;
        thingPeelCacheValid_ = false;
        gridCacheValid_ = false;
        surfacePickIndex_ = sed::level::SurfacePickIndex{};
        sectorContainmentIndex_ = sed::level::SectorContainmentIndex{};
    }

    if (keepView)
    {
        camera_.orbit.yawDeg = oldYawDeg;
        camera_.orbit.pitchDeg = oldPitchDeg;
        camera_.orbit.distance = oldDistance;
        camera_.orbit.pan = oldTarget - camera_.orbit.center;
        camera_.orbit.distance = std::max(camera_.orbit.distance, camera_.orbit.radius * 0.05f);
    }
    else if (rebuildRenderCache)
    {
        fitViewToLevel();
    }

    if (glReady_ && rebuildRenderCache)
    {
        makeCurrent();
        uploadMeshes();
        doneCurrent();
    }

    update();
    if ( previousCleaveRevision != stagedCleaveAuthoredRevision_ )
    {
        Q_EMIT stagedCleaveAuthoredStateChanged(stagedCleaveAuthoredRevision_);
    }
    if ( pointerChanged || !level_ || peerUnitChanged || previousCleaveUnit != cleaveMeasureUnit_ )
    {
        Q_EMIT viewportStateChanged();
    }
}

void GlView::notifyLevelGeometryChanged()
{
    invalidateAllFrameRenderCaches();
    transformSurfaceUvPlan_.reset();
    transformSurfaceUvPreviewSurfaces_.clear();
    suspendStagedCleaveInteraction();
    validateRetainedStagedCleaveReferences();
    if ( !level_ )
    {
        clearSurfaceEdgeSelection();
        return;
    }

    const QVector3D oldTarget = camera_.orbit.target();
    const float oldYawDeg = camera_.orbit.yawDeg;
    const float oldPitchDeg = camera_.orbit.pitchDeg;
    const float oldDistance = camera_.orbit.distance;

    rebuildMeshes(false, MeshRebuildKind::LevelOnly);

    camera_.orbit.yawDeg = oldYawDeg;
    camera_.orbit.pitchDeg = oldPitchDeg;
    camera_.orbit.distance = oldDistance;
    camera_.orbit.pan = oldTarget - camera_.orbit.center;
    camera_.orbit.distance = std::max(camera_.orbit.distance, camera_.orbit.radius * 0.05f);

    if (glReady_)
    {
        makeCurrent();
        uploadMeshes();
        doneCurrent();
    }

    if (placementPreviewThing_)
    {
        (void)tryFastUpdatePlacementPreviewThings(
            std::vector<Thing>{ *placementPreviewThing_ },
            placementPreviewThingInvalid_);
    }
    else if (!placementPreviewThings_.empty())
    {
        (void)tryFastUpdatePlacementPreviewThings(
            placementPreviewThings_,
            placementPreviewThingInvalid_);
    }

    markViewChanged();
    update();
}

void GlView::setSurfaceExtrudePreviewOpacity(const float opacity)
{
    const float clamped = std::clamp(opacity, 0.05f, 1.0f);
    if (std::abs(surfaceExtrudePreviewOpacity_ - clamped) <= 1.0e-6f)
    {
        return;
    }
    surfaceExtrudePreviewOpacity_ = clamped;
    if (placementPreviewSector_ || placementPreviewSourcePatch_)
    {
        notifyLevelGeometryChanged();
    }
    else
    {
        update();
    }
}



void GlView::notifyThingsChanged()
{
    notifyThingsChangedImpl(true);
}

void GlView::notifyThingFramesChanged(
    const std::vector<std::int32_t>& thingIndices)
{
    if ( !level_ || thingIndices.empty() )
    {
        return;
    }

    invalidateFramePoseCaches(thingIndices, false);
    thingPeelCacheValid_ = false;
    thingPeelCache_.hits.clear();
    resetPickCycle();
    update();
}

void GlView::notifyThingAppearanceChanged()
{
    if ( !level_ )
    {
        return;
    }

    // ARCHLIGHTING edits can leave every Thing geometry signature unchanged.
    // Invalidate the Thing cache before the conservative rebuild so an append
    // publication cannot retain stale colors for the unchanged prefix.
    thingMeshCache_.valid = false;
    notifyThingsChangedImpl(false);
}

void GlView::notifyThingsChangedImpl(const bool attemptFastUpdate)
{
    if (!level_)
    {
        return;
    }

    invalidateAllFrameRenderCaches();

    const QVector3D oldTarget = camera_.orbit.target();
    const float oldYawDeg = camera_.orbit.yawDeg;
    const float oldPitchDeg = camera_.orbit.pitchDeg;
    const float oldDistance = camera_.orbit.distance;

    const bool fastUpdated = attemptFastUpdate
        && tryFastUpdateThingTransforms();
    if (!fastUpdated)
    {
        rebuildMeshes(false, MeshRebuildKind::ThingsOnly);
    }
    else
    {
        thingTypeGizmoMetadataValid_ = false;
        thingTypeGizmoCacheValid_ = false;
        thingLightGizmoEntriesCacheValid_ = false;
        architecturalLightingOwnerGizmoMetadataValid_ = false;
        architecturalLightingOwnerGizmoCacheValid_ = false;
        architecturalLightingOwnerGizmoEntries_.clear();
        architecturalLightingOwnerGizmoEntryByThing_.clear();
        architecturalLightingOwnerGizmoVertices_.clear();
        refreshArchitecturalLightingRenderCache();
        applyArchitecturalLightingTrianglePreview();
    }

    camera_.orbit.yawDeg = oldYawDeg;
    camera_.orbit.pitchDeg = oldPitchDeg;
    camera_.orbit.distance = oldDistance;
    camera_.orbit.pan = oldTarget - camera_.orbit.center;
    camera_.orbit.distance = std::max(camera_.orbit.distance, camera_.orbit.radius * 0.05f);

    if (glReady_ && !fastUpdated)
    {
        makeCurrent();
        uploadMeshes();
        doneCurrent();
    }
    else if (glReady_ && fastUpdated)
    {
        makeCurrent();
        uploadArchitecturalLightingTrianglePreview();
        doneCurrent();
    }

    // Pick-id zero keeps placement previews out of the persistent Thing
    // updater. Refresh their recorded ranges explicitly so edits to ambient,
    // extra, or IJIM sector point-light values cannot leave a stationary
    // preview with stale lighting.
    if (placementPreviewThing_)
    {
        (void)tryFastUpdatePlacementPreviewThings(
            std::vector<Thing>{ *placementPreviewThing_ },
            placementPreviewThingInvalid_);
    }
    else if (!placementPreviewThings_.empty())
    {
        (void)tryFastUpdatePlacementPreviewThings(
            placementPreviewThings_,
            placementPreviewThingInvalid_);
    }

    // Runtime lights can appear, disappear, or fall back to an inherited
    // template value without changing the owner selection. Keep the selected
    // emitter kind (and the MainWindow-facing property used by Delete/dialog
    // routing) synchronized after every Thing-field mutation.
    if (lightGizmoCategory_ == render::LightGizmoCategory::Thing && selectedThing_ >= 0)
    {
        if (!thingLightGizmoEntriesCacheValid_)
        {
            thingLightGizmoEntriesCache_ =
                render::thingLightGizmoEntries(*level_, templates_);
            thingLightGizmoEntriesCacheValid_ = true;
        }
        const std::vector<render::LightGizmoEntry>& entries =
            thingLightGizmoEntriesCache_;
        auto selectedEntry = std::find_if(
            entries.begin(), entries.end(), [this](const render::LightGizmoEntry& entry)
            {
                return entry.ownerIndex == selectedThing_
                    && entry.kind == selectedLightGizmoKind_;
            });
        if (selectedEntry == entries.end())
        {
            selectedEntry = std::find_if(
                entries.begin(), entries.end(), [this](const render::LightGizmoEntry& entry)
                {
                    return entry.ownerIndex == selectedThing_;
                });
            if (selectedEntry != entries.end())
            {
                selectedLightGizmoKind_ = selectedEntry->kind;
            }
        }
        setProperty(
            "sed.selectedLightGizmoKind",
            selectedEntry != entries.end()
                ? QVariant(static_cast<int>(selectedLightGizmoKind_))
                : QVariant{});
    }

    markViewChanged();
    update();
}

void GlView::notifyThingTransformsChanged(
    const std::vector<std::int32_t>& thingIndices)
{
    if (!level_ || thingIndices.empty())
    {
        return;
    }

    // Ordinary Thing transforms can translate stored Frame points as part of
    // the same document operation. Keep the model identity and shared mesh
    // topology; only these owners' parsed poses and instance data are stale.
    invalidateFramePoseCaches(thingIndices, false);

    // The affine patch preserves topology. A geoset boundary changes its
    // triangle count, so rebuild only when one of these targeted owners
    // actually enters another tier. Ordinary move/scrub samples remain on
    // the existing fast path.
    if (thingModelLodEnabled_ && level_->kind != ProjectType::IJIM)
    {
        const ThingLodView lodView = thingLodView();
        for (const std::int32_t thingIndex : thingIndices)
        {
            if (thingIndex < 0
                || static_cast<std::size_t>(thingIndex) >= thingRenderState_.size()
                || static_cast<std::size_t>(thingIndex) >= level_->things.size())
            {
                continue;
            }
            const ThingRenderState& state =
                thingRenderState_[static_cast<std::size_t>(thingIndex)];
            if (state.geosetCount > 1u && !state.archThing
                && selectedThingModelLod(
                    state.geosetCount,
                    level_->things[static_cast<std::size_t>(thingIndex)],
                    lodView) != state.requestedLodIndex)
            {
                notifyThingsChangedImpl(false);
                return;
            }
        }
    }

    const QVector3D oldTarget = camera_.orbit.target();
    const float oldYawDeg = camera_.orbit.yawDeg;
    const float oldPitchDeg = camera_.orbit.pitchDeg;
    const float oldDistance = camera_.orbit.distance;

    if (!tryFastUpdateThingTransforms(&thingIndices))
    {
        // Geometry-affecting parameters, stale mappings, or a topology change
        // still require the conservative general path.
        ++thingTransformFallbackRebuildCount_;
        const char* reason = thingTransformFastFallbackReasonName(
            lastThingTransformFastFallbackReason_);
        setProperty("sed.thingTransformFastFallbackReason", reason);
        setProperty(
            "sed.thingTransformFallbackRebuildCount",
            QVariant::fromValue<qulonglong>(thingTransformFallbackRebuildCount_));
        qWarning().nospace()
            << "Thing transform fast update fell back to a full Thing rebuild ("
            << reason << ", count=" << thingTransformFallbackRebuildCount_ << ')';
        // The targeted attempt already proved that these caches cannot be
        // patched. Do not immediately repeat a broad all-Thing fast-path scan
        // before taking the conservative rebuild.
        notifyThingsChangedImpl(false);
        return;
    }
    ++thingTransformFastPublicationCount_;
    refreshArchitecturalLightingRenderCache();
    applyArchitecturalLightingTrianglePreview();

    // The persistent Thing mesh was patched in place. Its cached world bounds
    // now contain the authoritative transform, so mark only those owners for
    // a six-vertex camera-facing type-icon patch on the next paint.
    if (thingTypeGizmoMetadataValid_ && thingTypeGizmoCacheValid_)
    {
        thingTypeGizmoDirtyOwners_.insert(
            thingIndices.begin(), thingIndices.end());
    }
    else
    {
        thingTypeGizmoDirtyOwners_.clear();
    }
    // When the cache is already usable, drawThingGizmos() can patch the six
    // camera-facing vertices belonging to each dirty owner. Invalidating the
    // whole cache here made every paced Item Edit sample rebuild and upload
    // every visible Thing icon, which dominates large levels. A stale/absent
    // cache still takes the conservative full rebuild path.
    if (!thingTypeGizmoMetadataValid_)
    {
        thingTypeGizmoCacheValid_ = false;
    }
    if (thingLightGizmoEntriesCacheValid_)
    {
        const std::unordered_set<std::int32_t> changedOwners(
            thingIndices.begin(), thingIndices.end());
        std::erase_if(
            thingLightGizmoEntriesCache_,
            [&changedOwners](const render::LightGizmoEntry& entry)
            {
                return changedOwners.contains(entry.ownerIndex);
            });
        for (const std::int32_t thingIndex : thingIndices)
        {
            if (thingIndex < 0)
            {
                continue;
            }
            render::appendThingLightGizmoEntries(
                thingLightGizmoEntriesCache_,
                *level_,
                templates_,
                static_cast<std::size_t>(thingIndex));
        }
    }

    camera_.orbit.yawDeg = oldYawDeg;
    camera_.orbit.pitchDeg = oldPitchDeg;
    camera_.orbit.distance = oldDistance;
    camera_.orbit.pan = oldTarget - camera_.orbit.center;
    camera_.orbit.distance = std::max(
        camera_.orbit.distance,
        camera_.orbit.radius * 0.05f);

    // Thing positions affect only the Thing peel cache. Do not mark the camera
    // changed: that dirties camera-facing sprite geometry and grid state.
    thingPeelCacheValid_ = false;
    thingPeelCache_.hits.clear();
    if (glReady_)
    {
        makeCurrent();
        uploadArchitecturalLightingTrianglePreview();
        doneCurrent();
    }
    update();
}

void GlView::notifyThingsAppended(const std::size_t firstThingIndex)
{
    if (!level_
        || firstThingIndex > level_->things.size()
        || firstThingIndex != thingRenderState_.size())
    {
        notifyThingsChanged();
        return;
    }

    trustedThingAppendStart_ = firstThingIndex;
    notifyThingsChanged();
    trustedThingAppendStart_.reset();
}

std::int32_t GlView::findContainingSector(
    const QVector3D& point,
    const std::int32_t preferredSector) const noexcept
{
    return findContainingSector(TVector{point.x(), point.y(), point.z()}, preferredSector);
}

std::int32_t GlView::findContainingSector(
    const TVector& point,
    const std::int32_t preferredSector) const noexcept
{
    if (!level_
        || !std::isfinite(point.x)
        || !std::isfinite(point.y)
        || !std::isfinite(point.z))
    {
        return -1;
    }

    if (sectorContainmentIndex_.matchesLevel(*level_))
    {
        return sectorContainmentIndex_.findContainingSector(
            *level_,
            static_cast<double>(point.x),
            static_cast<double>(point.y),
            static_cast<double>(point.z),
            preferredSector);
    }
    return sed::level::levelFindSectorForXYZ(
        *level_,
        static_cast<double>(point.x),
        static_cast<double>(point.y),
        static_cast<double>(point.z));
}

bool GlView::setThingPropertyTransformPreview(
    const std::int32_t thingIndex,
    const Thing& source,
    const Thing& preview)
{
    // Pointer-owned transform gestures and item-editor scrubs are mutually
    // exclusive. Do not overwrite a transform tool's live shader state.
    if (transformDrag_.active
        || !level_
        || thingIndex < 0
        || static_cast<std::size_t>(thingIndex) >= level_->things.size())
    {
        return false;
    }

    TVector sourcePyr{};
    sourcePyr.pitch = source.pch;
    sourcePyr.yaw = source.yaw;
    sourcePyr.roll = source.rol;
    TVector previewPyr{};
    previewPyr.pitch = preview.pch;
    previewPyr.yaw = preview.yaw;
    previewPyr.roll = preview.rol;

    const TMatrix43 sourceTransform = TMatrix43::Create(
        TVector{ source.x, source.y, source.z }, sourcePyr);
    const TMatrix43 previewTransform = TMatrix43::Create(
        TVector{ preview.x, preview.y, preview.z }, previewPyr);
    bool invertible = false;
    const QMatrix4x4 inverseSource =
        matrix43ToQt(sourceTransform).inverted(&invertible);
    if (!invertible)
    {
        return false;
    }

    return setThingPropertyDeltaPreview(
        std::vector<std::int32_t>{ thingIndex },
        thingIndex,
        matrix43ToQt(previewTransform) * inverseSource,
        source.pch != preview.pch
            || source.yaw != preview.yaw
            || source.rol != preview.rol);
}

bool GlView::setThingPropertyDeltaPreview(
    const std::vector<std::int32_t>& thingIndices,
    const std::int32_t primaryThingIndex,
    const QMatrix4x4& transform,
    const bool rotating)
{
    if (transformDrag_.active || !level_ || thingIndices.empty())
    {
        return false;
    }

    transformPreviewVertexFilter_ = false;
    transformPreviewIdCount_ = 0;
    thingPropertyTransformPreviewThings_.clear();
    thingPropertyTransformPreviewThings_.reserve(thingIndices.size());
    for (const std::int32_t thingIndex : thingIndices)
    {
        if (thingIndex < 0
            || static_cast<std::size_t>(thingIndex) >= level_->things.size())
        {
            continue;
        }
        thingPropertyTransformPreviewThings_.insert(thingIndex);
        if (transformPreviewIdCount_
                >= static_cast<int>(transformPreviewIds_.size())
            || static_cast<std::size_t>(thingIndex) >= thingPickIds_.size())
        {
            continue;
        }
        const int pickId = static_cast<int>(
            thingPickIds_[static_cast<std::size_t>(thingIndex)]);
        if (pickId <= 0)
        {
            continue;
        }
        const std::size_t slot = static_cast<std::size_t>(transformPreviewIdCount_++);
        transformPreviewIds_[slot] = pickId;
        transformPreviewPayloads_[slot] = QVector3D(1.0f, 1.0f, 1.0f);
    }
    if (thingPropertyTransformPreviewThings_.empty())
    {
        clearThingPropertyTransformPreview();
        return false;
    }

    // Scrub/step gestures use the same one-matrix shader path as viewport
    // transforms. The authored Things remain untouched until release, so the
    // live cost is independent of model vertex count and Level size.
    transformPreviewMatrix_ = transform;
    transformPreviewActive_ = transformPreviewIdCount_ > 0;
    thingPropertyTransformPreviewActive_ = true;
    thingPropertyTransformPreviewThing_ =
        thingPropertyTransformPreviewThings_.contains(primaryThingIndex)
        ? primaryThingIndex
        : *std::min_element(
            thingPropertyTransformPreviewThings_.begin(),
            thingPropertyTransformPreviewThings_.end());
    setProperty(
        "sed.thingPropertyTransformPreviewPrimary",
        thingPropertyTransformPreviewThing_);
    thingPropertyTransformPreviewRotating_ = rotating;
    previewThingSectors_.clear();
    update();
    return true;
}

void GlView::clearThingPropertyTransformPreview()
{
    if (!thingPropertyTransformPreviewActive_)
    {
        thingPropertyTransformPreviewThing_ = -1;
        thingPropertyTransformPreviewThings_.clear();
        thingPropertyTransformPreviewRotating_ = false;
        setProperty("sed.thingPropertyTransformPreviewPrimary", QVariant{});
        return;
    }

    thingPropertyTransformPreviewActive_ = false;
    thingPropertyTransformPreviewThing_ = -1;
    thingPropertyTransformPreviewThings_.clear();
    thingPropertyTransformPreviewRotating_ = false;
    setProperty("sed.thingPropertyTransformPreviewPrimary", QVariant{});
    transformPreviewActive_ = false;
    transformPreviewVertexFilter_ = false;
    transformPreviewIdCount_ = 0;
    transformPreviewMatrix_.setToIdentity();
    previewThingSectors_.clear();
    update();
}

void GlView::notifyLightsChanged()
{
    if (!level_)
    {
        return;
    }

    markViewChanged();
    update();
}

void GlView::notifyAllMeshesChanged()
{
    suspendStagedCleaveInteraction();
    validateRetainedStagedCleaveReferences();
    if (!level_)
    {
        return;
    }

    invalidateAllFrameRenderCaches();

    const QVector3D oldTarget = camera_.orbit.target();
    const float oldYawDeg = camera_.orbit.yawDeg;
    const float oldPitchDeg = camera_.orbit.pitchDeg;
    const float oldDistance = camera_.orbit.distance;

    rebuildMeshes(false, MeshRebuildKind::All);

    camera_.orbit.yawDeg = oldYawDeg;
    camera_.orbit.pitchDeg = oldPitchDeg;
    camera_.orbit.distance = oldDistance;
    camera_.orbit.pan = oldTarget - camera_.orbit.center;
    camera_.orbit.distance = std::max(camera_.orbit.distance, camera_.orbit.radius * 0.05f);

    if (glReady_)
    {
        makeCurrent();
        uploadMeshes();
        doneCurrent();
    }

    markViewChanged();
    update();
}

void GlView::setResourceLocator(
    const assets::ResourceLocator* locator,
    const bool uploadRenderCache)
{
    // Resource replacement can invalidate the selected model identity while
    // leaving the document Thing unchanged. Cancel staged ARCH patches before
    // clearTextures() drops the model cache.
    clearArchitecturalLightingPreview(false);
    if (!uploadRenderCache)
    {
        ++resourceLocatorDeferredUploadCount_;
    }
    resources_ = locator;
    if (resourceDiskWatcher_)
    {
        resourceDiskWatcher_->setResourceLocator(locator);
    }
    if (glReady_)
    {
        makeCurrent();
        clearTextures();
        if (uploadRenderCache)
        {
            ++resourceLocatorUploadCount_;
            uploadMeshes();
        }
        doneCurrent();
    }
    else
    {
        clearTextures();
    }
    update();
}

void GlView::setResourceArchiveReloadCallback(std::function<bool()> callback)
{
    resourceArchiveReloadCallback_ = std::move(callback);
}

void GlView::setMountedArchiveWatchingEnabled(const bool enabled)
{
    if ( resourceDiskWatcher_ )
    {
        resourceDiskWatcher_->setMountedArchiveWatchingEnabled(enabled);
    }
}

void GlView::setTemplateDatabase(const assets::Templates* templates)
{
    invalidateAllFrameRenderCaches();
    clearArchitecturalLightingPreview(false);
    const QVector3D oldTarget = camera_.orbit.target();
    const float oldYawDeg = camera_.orbit.yawDeg;
    const float oldPitchDeg = camera_.orbit.pitchDeg;
    const float oldDistance = camera_.orbit.distance;

    templates_ = templates;

    rebuildMeshes();
    camera_.orbit.yawDeg = oldYawDeg;
    camera_.orbit.pitchDeg = oldPitchDeg;
    camera_.orbit.distance = oldDistance;
    camera_.orbit.pan = oldTarget - camera_.orbit.center;
    camera_.orbit.distance = std::max(camera_.orbit.distance, camera_.orbit.radius * 0.05f);

    if (glReady_)
    {
        makeCurrent();
        uploadMeshes();
        doneCurrent();
    }

    update();
}

void GlView::setSelection(
    const std::int32_t sectorIndex,
    const std::int32_t surfaceIndex,
    const std::int32_t thingIndex,
    const std::int32_t lightIndex)
{
    setSelectionState(sectorIndex, surfaceIndex, thingIndex, lightIndex, highlightedSelection_);
}

std::optional<GlView::SurfaceEdgeSelectionRef> GlView::surfaceEdgeAtHit(
    const SurfaceRef ref,
    const TVector& hit) const
{
    if (!level_ || ref.sector < 0 || ref.surface < 0
        || static_cast<std::size_t>(ref.sector) >= level_->sectors.size())
    {
        return std::nullopt;
    }
    const Sector& sector = level_->sectors[static_cast<std::size_t>(ref.sector)];
    if (static_cast<std::size_t>(ref.surface) >= sector.surfaces.size())
    {
        return std::nullopt;
    }
    const Surface& surface = sector.surfaces[static_cast<std::size_t>(ref.surface)];
    double bestDistanceSquared = std::numeric_limits<double>::infinity();
    std::optional<SurfaceEdgeSelectionRef> best{};
    for (std::size_t edge = 0; edge < surface.vertices.size(); ++edge)
    {
        const std::int32_t first = surface.vertices[edge];
        const std::int32_t second = surface.vertices[
            (edge + 1u) % surface.vertices.size()];
        if (first < 0 || second < 0
            || static_cast<std::size_t>(first) >= sector.vertices.size()
            || static_cast<std::size_t>(second) >= sector.vertices.size())
        {
            continue;
        }
        const Vertex& firstVertex = sector.vertices[static_cast<std::size_t>(first)];
        const Vertex& secondVertex = sector.vertices[static_cast<std::size_t>(second)];
        const TVector a{ firstVertex.x, firstVertex.y, firstVertex.z };
        const TVector b{ secondVertex.x, secondVertex.y, secondVertex.z };
        const TVector ab = b - a;
        const double lengthSquared = ab.Dot(ab);
        if (!std::isfinite(lengthSquared)
            || lengthSquared <= sed::math::kTinyEpsilon)
        {
            continue;
        }
        const double t = std::clamp(
            (hit - a).Dot(ab) / lengthSquared, 0.0, 1.0);
        const TVector delta = hit - (a + ab.scaled(t));
        const double distanceSquared = delta.Dot(delta);
        if (std::isfinite(distanceSquared)
            && distanceSquared < bestDistanceSquared)
        {
            bestDistanceSquared = distanceSquared;
            best = SurfaceEdgeSelectionRef{
                ref, static_cast<std::int32_t>(edge), first, second };
        }
    }
    return best;
}

bool GlView::surfaceEdgeSelectionRefValid(
    const SurfaceEdgeSelectionRef& edge) const noexcept
{
    if (!level_ || edge.surface.sector < 0 || edge.surface.surface < 0
        || edge.localEdge < 0
        || static_cast<std::size_t>(edge.surface.sector) >= level_->sectors.size())
    {
        return false;
    }
    const Sector& sector = level_->sectors[
        static_cast<std::size_t>(edge.surface.sector)];
    if (static_cast<std::size_t>(edge.surface.surface) >= sector.surfaces.size())
    {
        return false;
    }
    const Surface& surface = sector.surfaces[
        static_cast<std::size_t>(edge.surface.surface)];
    if (static_cast<std::size_t>(edge.localEdge) >= surface.vertices.size())
    {
        return false;
    }
    return surface.vertices[static_cast<std::size_t>(edge.localEdge)]
            == edge.firstVertex
        && surface.vertices[(static_cast<std::size_t>(edge.localEdge) + 1u)
                % surface.vertices.size()]
            == edge.secondVertex;
}

std::optional<TVector> GlView::surfaceEdgeMidpoint(
    const SurfaceEdgeSelectionRef& edge) const
{
    if (!surfaceEdgeSelectionRefValid(edge))
    {
        return std::nullopt;
    }
    const Sector& sector = level_->sectors[
        static_cast<std::size_t>(edge.surface.sector)];
    if (edge.firstVertex < 0 || edge.secondVertex < 0
        || static_cast<std::size_t>(edge.firstVertex) >= sector.vertices.size()
        || static_cast<std::size_t>(edge.secondVertex) >= sector.vertices.size())
    {
        return std::nullopt;
    }
    const Vertex& first = sector.vertices[static_cast<std::size_t>(edge.firstVertex)];
    const Vertex& second = sector.vertices[static_cast<std::size_t>(edge.secondVertex)];
    return TVector{
        (first.x + second.x) * 0.5,
        (first.y + second.y) * 0.5,
        (first.z + second.z) * 0.5 };
}

void GlView::clearSurfaceEdgeSelection() noexcept
{
    selectedSurfaceEdges_.clear();
    activeSurfaceEdge_.reset();
}

bool GlView::revalidateSurfaceEdgeSelection(
    const editor::Selection* owners,
    const std::optional<SurfaceRef> preferredOwner)
{
    bool changed = false;
    for (auto it = selectedSurfaceEdges_.begin(); it != selectedSurfaceEdges_.end();)
    {
        if (!surfaceEdgeSelectionRefValid(*it)
            || (owners && !owners->surfaces.contains(it->surface)))
        {
            it = selectedSurfaceEdges_.erase(it);
            changed = true;
        }
        else
        {
            ++it;
        }
    }
    if (activeSurfaceEdge_
        && !selectedSurfaceEdges_.contains(*activeSurfaceEdge_))
    {
        activeSurfaceEdge_.reset();
        changed = true;
    }
    const auto before = [](const SurfaceEdgeSelectionRef& lhs,
                           const SurfaceEdgeSelectionRef& rhs)
    {
        if (lhs.surface.sector != rhs.surface.sector)
        {
            return lhs.surface.sector < rhs.surface.sector;
        }
        if (lhs.surface.surface != rhs.surface.surface)
        {
            return lhs.surface.surface < rhs.surface.surface;
        }
        return lhs.localEdge < rhs.localEdge;
    };

    // MainWindow publishes the primary Surface independently from the
    // viewport's exact local-edge set. In particular, additive marquee
    // selection deterministically chooses the lowest owning Surface while
    // the previous active edge can belong to another owner. Keep transform,
    // cleave, normal, and active-color semantics on the same primary owner.
    if (preferredOwner
        && (!activeSurfaceEdge_
            || activeSurfaceEdge_->surface != *preferredOwner))
    {
        const auto preferred = std::min_element(
            selectedSurfaceEdges_.begin(), selectedSurfaceEdges_.end(),
            [&](const SurfaceEdgeSelectionRef& lhs,
                const SurfaceEdgeSelectionRef& rhs)
            {
                const bool lhsPreferred = lhs.surface == *preferredOwner;
                const bool rhsPreferred = rhs.surface == *preferredOwner;
                if (lhsPreferred != rhsPreferred)
                {
                    return lhsPreferred;
                }
                return before(lhs, rhs);
            });
        if (preferred != selectedSurfaceEdges_.end()
            && preferred->surface == *preferredOwner)
        {
            activeSurfaceEdge_ = *preferred;
            changed = true;
        }
    }
    if (!activeSurfaceEdge_ && !selectedSurfaceEdges_.empty())
    {
        activeSurfaceEdge_ = *std::min_element(
            selectedSurfaceEdges_.begin(), selectedSurfaceEdges_.end(), before);
        changed = true;
    }
    if (activeSurfaceEdge_)
    {
        if (const std::optional<TVector> midpoint =
                surfaceEdgeMidpoint(*activeSurfaceEdge_))
        {
            haveLastSurfaceHit_ = true;
            lastSurfaceHitSurface_ = activeSurfaceEdge_->surface;
            lastSurfaceHitPos_ = *midpoint;
        }
    }
    else if (surfaceSubMode_ == SurfaceSubMode::Edge)
    {
        haveLastSurfaceHit_ = false;
        lastSurfaceHitSurface_ = {};
        lastSurfaceHitPos_ = {};
    }
    return changed;
}

bool GlView::surfaceVertexSelectionRefValid(
    const editor::SurfaceVertexRef& vertex) const noexcept
{
    if (!level_ || vertex.surface.sector < 0 || vertex.surface.surface < 0
        || vertex.localVertex < 0
        || static_cast<std::size_t>(vertex.surface.sector)
            >= level_->sectors.size())
    {
        return false;
    }
    const Sector& sector = level_->sectors[
        static_cast<std::size_t>(vertex.surface.sector)];
    if (static_cast<std::size_t>(vertex.surface.surface)
        >= sector.surfaces.size())
    {
        return false;
    }
    const Surface& surface = sector.surfaces[
        static_cast<std::size_t>(vertex.surface.surface)];
    if (static_cast<std::size_t>(vertex.localVertex)
        >= surface.vertices.size())
    {
        return false;
    }
    const std::int32_t sectorVertex = surface.vertices[
        static_cast<std::size_t>(vertex.localVertex)];
    return sectorVertex >= 0
        && static_cast<std::size_t>(sectorVertex) < sector.vertices.size();
}

bool GlView::revalidateActiveSurfaceVertex(
    const editor::Selection& selection,
    const std::optional<SurfaceRef> preferredOwner)
{
    const auto before = [](const editor::SurfaceVertexRef& lhs,
                           const editor::SurfaceVertexRef& rhs)
    {
        if (lhs.surface.sector != rhs.surface.sector)
        {
            return lhs.surface.sector < rhs.surface.sector;
        }
        if (lhs.surface.surface != rhs.surface.surface)
        {
            return lhs.surface.surface < rhs.surface.surface;
        }
        return lhs.localVertex < rhs.localVertex;
    };
    const auto usable = [&](const editor::SurfaceVertexRef& ref)
    {
        return selection.surfaceVertices.contains(ref)
            && surfaceVertexSelectionRefValid(ref);
    };

    std::optional<editor::SurfaceVertexRef> next{};
    if (activeSurfaceVertexHintValid_ && usable(activeSurfaceVertexHint_)
        && (!preferredOwner
            || activeSurfaceVertexHint_.surface == *preferredOwner))
    {
        next = activeSurfaceVertexHint_;
    }

    if (!activeSurfaceVertexHintValid_ || !usable(activeSurfaceVertexHint_)
        || (preferredOwner
            && activeSurfaceVertexHint_.surface != *preferredOwner))
    {
        next.reset();
        for (const editor::SurfaceVertexRef& ref : selection.surfaceVertices)
        {
            if (!usable(ref)
                || (preferredOwner && ref.surface != *preferredOwner))
            {
                continue;
            }
            if (!next || before(ref, *next))
            {
                next = ref;
            }
        }
    }
    if (!next && preferredOwner)
    {
        for (const editor::SurfaceVertexRef& ref : selection.surfaceVertices)
        {
            if (usable(ref) && (!next || before(ref, *next)))
            {
                next = ref;
            }
        }
    }

    const bool changed = activeSurfaceVertexHintValid_ != next.has_value()
        || (next && activeSurfaceVertexHint_ != *next);
    activeSurfaceVertexHintValid_ = next.has_value();
    activeSurfaceVertexHint_ = next.value_or(editor::SurfaceVertexRef{});
    return changed;
}

std::unordered_set<SurfaceRef, editor::SurfaceRefHash, editor::SurfaceRefEq>
GlView::selectedSurfaceEdgeOwners() const
{
    std::unordered_set<SurfaceRef, editor::SurfaceRefHash, editor::SurfaceRefEq>
        owners{};
    owners.reserve(selectedSurfaceEdges_.size());
    for (const SurfaceEdgeSelectionRef& edge : selectedSurfaceEdges_)
    {
        if (surfaceEdgeSelectionRefValid(edge))
        {
            owners.insert(edge.surface);
        }
    }
    return owners;
}

void GlView::applySurfaceEdgePointSelection(
    const std::optional<SurfaceEdgeSelectionRef> edge,
    const Qt::KeyboardModifiers modifiers)
{
    if (surfaceSubMode_ != SurfaceSubMode::Edge)
    {
        return;
    }
    constexpr Qt::KeyboardModifiers selectionMask =
        Qt::ShiftModifier | Qt::ControlModifier
        | Qt::AltModifier | Qt::MetaModifier;
    const Qt::KeyboardModifiers held = modifiers & selectionMask;
    const auto bindingActive = [held](const ui::shortcuts::ModifierBindings& bindings)
    {
        return ui::shortcuts::modifierBindingsActive(held, bindings);
    };
    const bool remove = bindingActive(selectionRemoveModifiers_);
    const bool add = !remove && bindingActive(selectionAddModifiers_);
    if (!add && !remove)
    {
        clearSurfaceEdgeSelection();
    }
    if (edge && surfaceEdgeSelectionRefValid(*edge))
    {
        if (remove)
        {
            // One rendered shared/adjoin edge can have several exact owner
            // refs. Remove that visible world-space segment as one unit so a
            // different owner cannot leave the same orange line behind. The
            // canonical endpoint comparison is deliberately not a general
            // collinearity test: adjacent edges and projection-only overlaps
            // at another depth remain selected.
            const auto edgeSegment = [this](
                const SurfaceEdgeSelectionRef& ref)
                -> std::optional<std::pair<QVector3D, QVector3D>>
            {
                if (!surfaceEdgeSelectionRefValid(ref))
                {
                    return std::nullopt;
                }
                const Sector& owner = level_->sectors[
                    static_cast<std::size_t>(ref.surface.sector)];
                const Vertex& first = owner.vertices[
                    static_cast<std::size_t>(ref.firstVertex)];
                const Vertex& second = owner.vertices[
                    static_cast<std::size_t>(ref.secondVertex)];
                return std::pair{
                    QVector3D(first.x, first.y, first.z),
                    QVector3D(second.x, second.y, second.z) };
            };
            const auto pickedSegment = edgeSegment(*edge);
            bool removedActive = false;
            for (auto it = selectedSurfaceEdges_.begin();
                 it != selectedSurfaceEdges_.end();)
            {
                const auto selectedSegment = edgeSegment(*it);
                const bool sameVisibleSegment = pickedSegment
                    && selectedSegment
                    && render::geometryWireSegmentsCoincide(
                        pickedSegment->first,
                        pickedSegment->second,
                        selectedSegment->first,
                        selectedSegment->second);
                if (!sameVisibleSegment)
                {
                    ++it;
                    continue;
                }
                removedActive = removedActive
                    || (activeSurfaceEdge_
                        && *activeSurfaceEdge_ == *it);
                it = selectedSurfaceEdges_.erase(it);
            }
            if (removedActive)
            {
                activeSurfaceEdge_.reset();
            }
        }
        else
        {
            selectedSurfaceEdges_.insert(*edge);
            activeSurfaceEdge_ = *edge;
        }
    }
    (void)revalidateSurfaceEdgeSelection();
    rebuildSelectionOverlays();
    if (glReady_)
    {
        makeCurrent();
        uploadSelectionOverlays();
        doneCurrent();
    }
    update();
}

void GlView::applySurfaceEdgeBoxSelection(
    const std::vector<SurfaceEdgeSelectionRef>& edges,
    const SelectionBoxMode mode)
{
    if (surfaceSubMode_ != SurfaceSubMode::Edge)
    {
        return;
    }
    if (mode == SelectionBoxMode::Replace)
    {
        clearSurfaceEdgeSelection();
    }
    for (const SurfaceEdgeSelectionRef& edge : edges)
    {
        if (!surfaceEdgeSelectionRefValid(edge))
        {
            continue;
        }
        if (mode == SelectionBoxMode::Remove)
        {
            selectedSurfaceEdges_.erase(edge);
            if (activeSurfaceEdge_ == edge)
            {
                activeSurfaceEdge_.reset();
            }
        }
        else
        {
            selectedSurfaceEdges_.insert(edge);
        }
    }
    (void)revalidateSurfaceEdgeSelection();
    rebuildSelectionOverlays();
    if (glReady_)
    {
        makeCurrent();
        uploadSelectionOverlays();
        doneCurrent();
    }
    update();
}

#ifdef SED_APP_MAIN_WINDOW_TEST_ACCESS
void GlView::setSelectedSurfaceEdgesForTest(
    const std::vector<std::pair<SurfaceRef, std::int32_t>>& edges,
    const std::optional<std::size_t> activeIndex)
{
    clearSurfaceEdgeSelection();
    std::vector<SurfaceEdgeSelectionRef> valid{};
    valid.reserve(edges.size());
    for (const auto& [surfaceRef, localEdge] : edges)
    {
        if (!level_ || surfaceRef.sector < 0 || surfaceRef.surface < 0
            || localEdge < 0
            || static_cast<std::size_t>(surfaceRef.sector) >= level_->sectors.size())
        {
            continue;
        }
        const Sector& sector = level_->sectors[
            static_cast<std::size_t>(surfaceRef.sector)];
        if (static_cast<std::size_t>(surfaceRef.surface) >= sector.surfaces.size())
        {
            continue;
        }
        const Surface& surface = sector.surfaces[
            static_cast<std::size_t>(surfaceRef.surface)];
        if (static_cast<std::size_t>(localEdge) >= surface.vertices.size())
        {
            continue;
        }
        const SurfaceEdgeSelectionRef ref{
            surfaceRef,
            localEdge,
            surface.vertices[static_cast<std::size_t>(localEdge)],
            surface.vertices[(static_cast<std::size_t>(localEdge) + 1u)
                % surface.vertices.size()] };
        selectedSurfaceEdges_.insert(ref);
        valid.push_back(ref);
    }
    if (activeIndex && *activeIndex < valid.size())
    {
        activeSurfaceEdge_ = valid[*activeIndex];
    }
    (void)revalidateSurfaceEdgeSelection();
    rebuildSelectionOverlays();
}

void GlView::applySurfaceEdgePointSelectionForTest(
    const SurfaceRef surfaceRef,
    const std::int32_t localEdge,
    const Qt::KeyboardModifiers modifiers)
{
    std::optional<SurfaceEdgeSelectionRef> edge{};
    if (level_ && surfaceRef.sector >= 0 && surfaceRef.surface >= 0
        && localEdge >= 0
        && static_cast<std::size_t>(surfaceRef.sector) < level_->sectors.size())
    {
        const Sector& sector = level_->sectors[
            static_cast<std::size_t>(surfaceRef.sector)];
        if (static_cast<std::size_t>(surfaceRef.surface) < sector.surfaces.size())
        {
            const Surface& surface = sector.surfaces[
                static_cast<std::size_t>(surfaceRef.surface)];
            if (static_cast<std::size_t>(localEdge) < surface.vertices.size())
            {
                edge = SurfaceEdgeSelectionRef{
                    surfaceRef,
                    localEdge,
                    surface.vertices[static_cast<std::size_t>(localEdge)],
                    surface.vertices[(static_cast<std::size_t>(localEdge) + 1u)
                        % surface.vertices.size()] };
            }
        }
    }
    applySurfaceEdgePointSelection(edge, modifiers);
}

void GlView::applySurfaceEdgeBoxSelectionForTest(
    const std::vector<std::pair<SurfaceRef, std::int32_t>>& edges,
    const SelectionBoxMode mode)
{
    std::vector<SurfaceEdgeSelectionRef> refs{};
    refs.reserve(edges.size());
    for (const auto& [surfaceRef, localEdge] : edges)
    {
        if (!level_ || surfaceRef.sector < 0 || surfaceRef.surface < 0
            || localEdge < 0
            || static_cast<std::size_t>(surfaceRef.sector)
                >= level_->sectors.size())
        {
            continue;
        }
        const Sector& sector = level_->sectors[
            static_cast<std::size_t>(surfaceRef.sector)];
        if (static_cast<std::size_t>(surfaceRef.surface)
            >= sector.surfaces.size())
        {
            continue;
        }
        const Surface& surface = sector.surfaces[
            static_cast<std::size_t>(surfaceRef.surface)];
        if (static_cast<std::size_t>(localEdge) >= surface.vertices.size())
        {
            continue;
        }
        refs.push_back(SurfaceEdgeSelectionRef{
            surfaceRef,
            localEdge,
            surface.vertices[static_cast<std::size_t>(localEdge)],
            surface.vertices[(static_cast<std::size_t>(localEdge) + 1u)
                % surface.vertices.size()] });
    }
    applySurfaceEdgeBoxSelection(refs, mode);
}
#endif

void GlView::setSelectionState(
    const std::int32_t sectorIndex,
    const std::int32_t surfaceIndex,
    const std::int32_t thingIndex,
    const std::int32_t lightIndex,
    const editor::Selection& selection)
{
    if ( pendingArchitecturalLightingRecovery_ && level_
        && renderPipeline_ == RenderPipeline::Editor )
    {
        std::vector<std::int32_t> ownerSelection(
            selection.things.begin(),
            selection.things.end());
        if ( thingIndex >= 0
            && static_cast<std::size_t>(thingIndex) < level_->things.size() )
        {
            ownerSelection.push_back(thingIndex);
        }
        std::sort(ownerSelection.begin(), ownerSelection.end());
        ownerSelection.erase(
            std::unique(ownerSelection.begin(), ownerSelection.end()),
            ownerSelection.end());
        if ( !pendingArchitecturalLightingSelectionReady_ )
        {
            pendingArchitecturalLightingSelectionReady_ = true;
            pendingArchitecturalLightingOwnerSelection_ = std::move(ownerSelection);
        }
        else if ( ownerSelection != pendingArchitecturalLightingOwnerSelection_ )
        {
            pendingArchitecturalLightingRecovery_.reset();
            pendingArchitecturalLightingSelectionReady_ = false;
            pendingArchitecturalLightingOwnerSelection_.clear();
        }
    }

    std::int32_t nextSector = -1;
    std::int32_t nextSurfaceSector = -1;
    std::int32_t nextSurface = -1;
    std::int32_t nextThing = -1;
    std::int32_t nextLight = -1;
    std::int32_t nextFrameValue = -1;

    auto minSelectedFrameThing = [&]() -> std::int32_t
    {
        std::int32_t best = -1;
        for (const sed::level::FrameRef& ref : selection.frames)
        {
            if (ref.thing < 0 || static_cast<std::size_t>(ref.thing) >= level_->things.size())
            {
                continue;
            }
            if (best < 0 || ref.thing < best)
            {
                best = ref.thing;
            }
        }
        return best;
    };

    auto minSelectedFrameValueForThing = [&](const std::int32_t thing) -> std::int32_t
    {
        if (thing < 0 || static_cast<std::size_t>(thing) >= level_->things.size())
        {
            return -1;
        }

        const Thing& selectedThing = level_->things[static_cast<std::size_t>(thing)];
        std::int32_t best = -1;
        for (const sed::level::FrameRef& ref : selection.frames)
        {
            if (ref.thing != thing || ref.value < 0)
            {
                continue;
            }
            if (static_cast<std::size_t>(ref.value) >= selectedThing.values.size())
            {
                continue;
            }

            const TplValue& value = selectedThing.values[static_cast<std::size_t>(ref.value)];
            if (value.type != ValueType::frame || !sed::text::iequalsAscii(value.name, "frame"))
            {
                continue;
            }

            if (best < 0 || ref.value < best)
            {
                best = ref.value;
            }
        }
        return best;
    };

    if (!level_)
    {
        const bool edgeSelectionChanged = !selectedSurfaceEdges_.empty()
            || activeSurfaceEdge_.has_value();
        const bool vertexSelectionChanged = activeSurfaceVertexHintValid_;
        clearSurfaceEdgeSelection();
        activeSurfaceVertexHintValid_ = false;
        activeSurfaceVertexHint_ = {};
        const bool selectionChanged =
            selectedSector_ != -1
            || selectedSurfaceSector_ != -1
            || selectedSurface_ != -1
            || selectedThing_ != -1
            || selectedLight_ != -1;
        const bool highlightChanged = !selectionEquals(highlightedSelection_, selection);
        const bool thrustSelectionChanged = !selectedSectorThrusts_.empty();
        if (!selectionChanged && !highlightChanged && !thrustSelectionChanged
            && !edgeSelectionChanged && !vertexSelectionChanged)
        {
            if ( pendingArchitecturalLightingRecovery_
                && pendingArchitecturalLightingSelectionReady_ )
            {
                (void)tryApplyPendingArchitecturalLightingRecovery();
            }
            return;
        }

        selectedSector_ = -1;
        selectedSurfaceSector_ = -1;
        selectedSurface_ = -1;
        selectedThing_ = -1;
        selectedLight_ = -1;
        selectedSectorThrusts_.clear();
        highlightedSelection_ = selection;
        refreshArchitecturalLightingRenderCache();
        applyArchitecturalLightingTrianglePreview();

        rebuildSelectionOverlays();
        if (glReady_)
        {
            makeCurrent();
            uploadArchitecturalLightingTrianglePreview();
            uploadSelectionOverlays();
            doneCurrent();
        }
        update();
        return;
    }

    if (thingIndex >= 0 && static_cast<std::size_t>(thingIndex) < level_->things.size())
    {
        nextThing = thingIndex;
    }
    else if (lightIndex >= 0 && static_cast<std::size_t>(lightIndex) < level_->lights.size())
    {
        nextLight = lightIndex;
    }
    else if (surfaceIndex >= 0 && sectorIndex >= 0 && static_cast<std::size_t>(sectorIndex) < level_->sectors.size())
    {
        const Sector& sector = level_->sectors[static_cast<std::size_t>(sectorIndex)];
        if (static_cast<std::size_t>(surfaceIndex) < sector.surfaces.size())
        {
            nextSurfaceSector = sectorIndex;
            nextSurface = surfaceIndex;
        }
    }
    else if (sectorIndex >= 0 && static_cast<std::size_t>(sectorIndex) < level_->sectors.size())
    {
        nextSector = sectorIndex;
    }
    else if (frameEditEnabled_ && !selection.frames.empty())
    {
        nextThing = minSelectedFrameThing();
    }

    if (frameEditEnabled_)
    {
        nextFrameValue = minSelectedFrameValueForThing(nextThing);
    }

    std::unordered_set<std::int32_t> nextSectorThrusts{};
    nextSectorThrusts.reserve(selection.sectors.size());
    for (const std::int32_t selectedIndex : selection.sectors)
    {
        if (selectedIndex < 0 || static_cast<std::size_t>(selectedIndex) >= level_->sectors.size())
        {
            continue;
        }
        if (isLayerLocked(level_->sectors[static_cast<std::size_t>(selectedIndex)].layer))
        {
            continue;
        }
        nextSectorThrusts.insert(selectedIndex);
    }

    const bool preservePreviewSubElementSelection =
        renderPipeline_ == RenderPipeline::GamePreview;
    const std::optional<SurfaceRef> preferredEdgeOwner =
        nextSurfaceSector >= 0 && nextSurface >= 0
            ? std::optional<SurfaceRef>{
                SurfaceRef{ nextSurfaceSector, nextSurface } }
            : std::nullopt;
    const bool edgeSelectionChanged = !preservePreviewSubElementSelection
        && ((surfaceSubMode_ == SurfaceSubMode::Edge)
            ? revalidateSurfaceEdgeSelection(
                &selection, preferredEdgeOwner)
            : (!selectedSurfaceEdges_.empty() || activeSurfaceEdge_.has_value()));
    if (!preservePreviewSubElementSelection
        && surfaceSubMode_ != SurfaceSubMode::Edge)
    {
        clearSurfaceEdgeSelection();
    }
    const bool vertexSelectionChanged = !preservePreviewSubElementSelection
        && ((surfaceSubMode_ == SurfaceSubMode::Vertex)
            ? revalidateActiveSurfaceVertex(selection, preferredEdgeOwner)
            : activeSurfaceVertexHintValid_);
    if (!preservePreviewSubElementSelection
        && surfaceSubMode_ != SurfaceSubMode::Vertex)
    {
        activeSurfaceVertexHintValid_ = false;
        activeSurfaceVertexHint_ = {};
    }

    const bool selectionChanged =
        selectedSector_ != nextSector
        || selectedSurfaceSector_ != nextSurfaceSector
        || selectedSurface_ != nextSurface
        || selectedThing_ != nextThing
        || selectedLight_ != nextLight
        || (frameEditEnabled_ && selectedFrameValueIndex_ != nextFrameValue);
    const bool highlightChanged = !selectionEquals(highlightedSelection_, selection);
    const bool thrustSelectionChanged = selectedSectorThrusts_ != nextSectorThrusts;

    if (!selectionChanged && !highlightChanged && !thrustSelectionChanged
        && !edgeSelectionChanged && !vertexSelectionChanged)
    {
        if ( pendingArchitecturalLightingRecovery_
            && pendingArchitecturalLightingSelectionReady_ )
        {
            (void)tryApplyPendingArchitecturalLightingRecovery();
        }
        return;
    }

    if (selectionChanged || highlightChanged)
    {
        // Redundant selection publication is presentation only. An actual
        // owner change settles the old target before replacing it.
        (void)finishStagedCleaveWheelSession();
    }
    selectedSector_ = nextSector;
    selectedSurfaceSector_ = nextSurfaceSector;
    selectedSurface_ = nextSurface;
    selectedThing_ = nextThing;
    selectedLight_ = nextLight;
    selectedSectorThrusts_ = std::move(nextSectorThrusts);
    highlightedSelection_ = selection;
    refreshArchitecturalLightingRenderCache();
    applyArchitecturalLightingTrianglePreview();

    if (lightGizmoCategory_ == render::LightGizmoCategory::Thing && selectedThing_ >= 0)
    {
        const std::vector<render::LightGizmoEntry> entries =
            render::thingLightGizmoEntries(*level_, templates_);
        const bool currentExists = std::any_of(entries.begin(), entries.end(), [this](const render::LightGizmoEntry& entry)
        {
            return entry.ownerIndex == selectedThing_ && entry.kind == selectedLightGizmoKind_;
        });
        if (!currentExists)
        {
            const auto first = std::find_if(entries.begin(), entries.end(), [this](const render::LightGizmoEntry& entry)
            {
                return entry.ownerIndex == selectedThing_;
            });
            if (first != entries.end())
            {
                selectedLightGizmoKind_ = first->kind;
            }
        }
        setProperty(
            "sed.selectedLightGizmoKind",
            std::any_of(entries.begin(), entries.end(), [this](const render::LightGizmoEntry& entry)
            {
                return entry.ownerIndex == selectedThing_
                    && entry.kind == selectedLightGizmoKind_;
            })
                ? QVariant(static_cast<int>(selectedLightGizmoKind_))
                : QVariant{});
    }

    if (frameEditEnabled_)
    {
        selectedFrameValueIndex_ = nextFrameValue;
    }

    rebuildSelectionOverlays();
    if (glReady_)
    {
        ScopedArchitecturalOpenGlContext context(*this);
        if (context.ready())
        {
            uploadArchitecturalLightingTrianglePreview();
            uploadSelectionOverlays();
        }
    }

    update();
}

void GlView::setFrameEditEnabled(const bool enabled)
{
    if (frameEditEnabled_ == enabled)
    {
        return;
    }

    frameEditEnabled_ = enabled;
    resetPickCycle();
    if (!frameEditEnabled_)
    {
        selectedFrameValueIndex_ = -1;
    }

    update();
}

void GlView::clearFrameSelection()
{
    if (selectedFrameValueIndex_ < 0)
    {
        return;
    }

    selectedFrameValueIndex_ = -1;
    update();
}

void GlView::setSelectedFrameValueIndex(const std::int32_t valueIndex)
{
    if (selectedFrameValueIndex_ == valueIndex)
    {
        return;
    }

    selectedFrameValueIndex_ = valueIndex;
    update();
}

void GlView::setHighlightedSelection(const editor::Selection& selection)
{
    const std::int32_t primaryFrameThing = selectedThing_;
    const std::int32_t primaryFrameValue = selectedFrameValueIndex_;
    const bool preservePrimaryFrame = frameEditEnabled_
        && primaryFrameThing >= 0
        && primaryFrameValue >= 0
        && selection.frames.contains(sed::level::FrameRef{
            primaryFrameThing, primaryFrameValue });
    const std::int32_t activeSector = (selectedSurface_ >= 0)
        ? selectedSurfaceSector_
        : selectedSector_;
    setSelectionState(
        activeSector, selectedSurface_, selectedThing_, selectedLight_, selection);

    // Highlight updates can recompute the frame selection's lowest index.
    // Preserve the explicit primary only while its exact owner/value remains
    // selected and valid; changing the selected frame set still updates the
    // primary through setSelectionState as before.
    if (!preservePrimaryFrame
        || !frameEditEnabled_
        || selectedThing_ != primaryFrameThing
        || !level_
        || static_cast<std::size_t>(primaryFrameThing) >= level_->things.size())
    {
        return;
    }

    const Thing& thing = level_->things[static_cast<std::size_t>(primaryFrameThing)];
    if (static_cast<std::size_t>(primaryFrameValue) >= thing.values.size())
    {
        return;
    }
    const TplValue& value = thing.values[static_cast<std::size_t>(primaryFrameValue)];
    if (value.type == ValueType::frame && sed::text::iequalsAscii(value.name, "frame"))
    {
        setSelectedFrameValueIndex(primaryFrameValue);
    }
}

void GlView::setSelectedSector(const std::int32_t sectorIndex)
{
    setSelection(sectorIndex, -1, -1, -1);
}

void GlView::setSelectedSurface(const std::int32_t sectorIndex, const std::int32_t surfaceIndex)
{
    setSelection(sectorIndex, surfaceIndex, -1, -1);
}

void GlView::setSelectedThing(const std::int32_t thingIndex)
{
    setSelection(-1, -1, thingIndex, -1);
}

void GlView::setSelectedLight(const std::int32_t lightIndex)
{
    setSelection(-1, -1, -1, lightIndex);
}

void GlView::fitViewToLevel()
{
    fitViewToLevel(-45.0f, 30.0f);
}

bool GlView::fitSpritePreviewToLevel(const bool preserveView)
{
    cancelViewAnimation();
    if ( !level_ )
    {
        return false;
    }

    // This camera policy belongs only to the isolated resource preview. Build
    // a candidate so an unrepresentable depth interval leaves the view intact.
    app::ViewportCamera fitted = camera_;
    const float dpr = devicePixelRatioF();
    const int pixelW = std::max(1, static_cast<int>(std::lround(static_cast<double>(width()) * dpr)));
    const int pixelH = std::max(1, static_cast<int>(std::lround(static_cast<double>(height()) * dpr)));
    fitted.SetCanvas(pixelW, pixelH);

    QVector3D minv{};
    QVector3D maxv{};
    if ( !computeLevelDataBounds(level_, minv, maxv) )
    {
        return false;
    }
    for ( const float component : { minv.x(), minv.y(), minv.z(), maxv.x(), maxv.y(), maxv.z() } )
    {
        if ( !std::isfinite(component) )
        {
            return false;
        }
    }
    if ( !preserveView )
    {
        fitted.SetProjectionType(app::ViewportCamera::ProjectionType::Orthographic);
        fitted.orbit.yawDeg = 0.0f;
        fitted.orbit.pitchDeg = 0.0f;
        fitted.orbit.rollDeg = 0.0f;
        fitted.orbit.pan = QVector3D{};
        fitted.orbit.center = minv * 0.5f + maxv * 0.5f;
        // Orthographic framing is set by scale. Inflating distance with the
        // card radius needlessly invalidates otherwise finite native poses.
    }
    fitted.UpdateFromOrbit();

    const std::array<QVector3D, 8> corners{
        QVector3D(minv.x(), minv.y(), minv.z()),
        QVector3D(minv.x(), minv.y(), maxv.z()),
        QVector3D(minv.x(), maxv.y(), minv.z()),
        QVector3D(minv.x(), maxv.y(), maxv.z()),
        QVector3D(maxv.x(), minv.y(), minv.z()),
        QVector3D(maxv.x(), minv.y(), maxv.z()),
        QVector3D(maxv.x(), maxv.y(), minv.z()),
        QVector3D(maxv.x(), maxv.y(), maxv.z()),
    };
    const auto basis = app::ViewportCamera::BasisFromOrbit(
        fitted.orbit.yawDeg, fitted.orbit.pitchDeg, fitted.orbit.rollDeg);
    const QVector3D target = fitted.orbit.target();
    const auto localCoordinate = [&target](const QVector3D& point, const QVector3D& axis)
    {
        return (static_cast<double>(point.x()) - target.x()) * axis.x()
            + (static_cast<double>(point.y()) - target.y()) * axis.y()
            + (static_cast<double>(point.z()) - target.z()) * axis.z();
    };
    double maxAbsViewX = 1.0e-6;
    double maxAbsViewY = 1.0e-6;
    double minLocalDepth = std::numeric_limits<double>::max();
    double maxLocalDepth = -std::numeric_limits<double>::max();
    double requiredDistance = 1.0e-4;
    const double aspect = static_cast<double>(pixelW) / pixelH;
    const bool perspective = fitted.projectionType() == app::ViewportCamera::ProjectionType::Perspective;
    const double tanHalfFovX = std::tan(static_cast<double>(fitted.fovXDeg()) * std::acos(-1.0) / 360.0);
    const double tanHalfFovY = tanHalfFovX / aspect;
    for ( const QVector3D& corner : corners )
    {
        const double x = std::abs(localCoordinate(corner, basis.right));
        const double y = std::abs(localCoordinate(corner, basis.up));
        const double depth = localCoordinate(corner, basis.forward);
        maxAbsViewX = std::max(maxAbsViewX, x);
        maxAbsViewY = std::max(maxAbsViewY, y);
        minLocalDepth = std::min(minLocalDepth, depth);
        maxLocalDepth = std::max(maxLocalDepth, depth);
        if ( perspective )
        {
            requiredDistance = std::max(requiredDistance,
                std::max(x / tanHalfFovX, y / tanHalfFovY) * 1.06 - depth);
        }
    }
    const double depthPadding = std::max(1.0e-3, (maxLocalDepth - minLocalDepth) * 0.01);
    if ( perspective )
    {
        requiredDistance = std::max(requiredDistance,
            app::ViewportCamera::kPerspectiveNearPlaneMin + depthPadding - minLocalDepth);
    }
    double distance = std::max(static_cast<double>(fitted.orbit.distance), requiredDistance);
    if ( !fitted.clipEnabled() )
    {
        // Clip-disabled projection still has a fixed GPU depth interval. An
        // orthographic fit can move only along its existing view axis without
        // changing scale, pan or authored XYZ; never switch the user's flag.
        const double nearDepth = perspective ? app::ViewportCamera::kPerspectiveNearPlaneMin : -100000.0;
        const double lower = std::max(requiredDistance, nearDepth + depthPadding - minLocalDepth);
        const double upper = 100000.0 - depthPadding - maxLocalDepth;
        if ( !std::isfinite(lower) || !std::isfinite(upper) || upper < lower )
        {
            return false;
        }
        distance = std::clamp(distance, lower, upper);
    }
    if ( !std::isfinite(distance) || distance > std::numeric_limits<float>::max() )
    {
        return false;
    }
    fitted.orbit.distance = static_cast<float>(distance);
    if ( !perspective )
    {
        const double desiredHalfExtent = std::max(maxAbsViewY, maxAbsViewX / aspect) * 1.06;
        const double scale = (pixelH * 0.5) / (aspect * desiredHalfExtent);
        if ( !std::isfinite(scale) || scale <= 0.0 || scale > std::numeric_limits<float>::max()
            || static_cast<float>(scale) <= 0.0f )
        {
            return false;
        }
        if ( !preserveView || scale < fitted.orthoScale() )
        {
            fitted.SetOrthoScale(static_cast<float>(scale));
        }
    }
    if ( fitted.clipEnabled() )
    {
        const double nearDepth = std::min(static_cast<double>(fitted.nearPlane()),
            distance + minLocalDepth - depthPadding);
        const double farDepth = std::max(static_cast<double>(fitted.farPlane()),
            distance + maxLocalDepth + depthPadding);
        if ( !std::isfinite(nearDepth) || !std::isfinite(farDepth)
            || nearDepth < -std::numeric_limits<float>::max() || farDepth > std::numeric_limits<float>::max() )
        {
            return false;
        }
        fitted.SetNearFar(static_cast<float>(nearDepth), static_cast<float>(farDepth));
    }
    fitted.UpdateFromOrbit();
    camera_ = fitted;
    markViewChanged();
    update();
    return true;
}

void GlView::fitViewToLevel(const float yawDegrees, const float pitchDegrees)
{
    camera_.orbit.yawDeg = yawDegrees;
    camera_.orbit.pitchDeg = pitchDegrees;
    camera_.orbit.pan = QVector3D(0.0f, 0.0f, 0.0f);
    const float dpr = devicePixelRatioF();
    const int pixelW = std::max(1, static_cast<int>(std::lround(static_cast<double>(width()) * dpr)));
    const int pixelH = std::max(1, static_cast<int>(std::lround(static_cast<double>(height()) * dpr)));
    camera_.SetCanvas(pixelW, pixelH);

    QVector3D minv(std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), std::numeric_limits<float>::max());
    QVector3D maxv(-std::numeric_limits<float>::max(), -std::numeric_limits<float>::max(), -std::numeric_limits<float>::max());
    bool haveAny = false;

    auto includeBounds = [&](const MeshPartCache& cache)
    {
        if (!cache.valid || !cache.haveAny)
        {
            return;
        }

        if (!haveAny)
        {
            minv = cache.min;
            maxv = cache.max;
            haveAny = true;
            return;
        }

        minv.setX(std::min(minv.x(), cache.min.x()));
        minv.setY(std::min(minv.y(), cache.min.y()));
        minv.setZ(std::min(minv.z(), cache.min.z()));
        maxv.setX(std::max(maxv.x(), cache.max.x()));
        maxv.setY(std::max(maxv.y(), cache.max.y()));
        maxv.setZ(std::max(maxv.z(), cache.max.z()));
    };

    if (computeLevelDataBounds(level_, minv, maxv))
    {
        haveAny = true;
    }
    else if (levelMeshCache_.valid && levelMeshCache_.haveAny)
    {
        includeBounds(levelMeshCache_);
    }
    else
    {
        includeBounds(thingMeshCache_);
    }

    const QVector3D center = haveAny ? (minv + maxv) * 0.5f : QVector3D(0.0f, 0.0f, 0.0f);
    camera_.orbit.center = center;
    camera_.orbit.distance = 1.0f;
    camera_.UpdateFromOrbit();
    camera_.Update();

    const auto viewPoint = [this](const QVector3D& point) -> QVector3D
    {
        return camera_.viewMatrix().map(point);
    };

    float maxAbsViewX = 0.5f;
    float maxAbsViewY = 0.5f;
    float targetDistance = 1.0f;
    if (haveAny)
    {
        const std::array<QVector3D, 8> corners{
            QVector3D(minv.x(), minv.y(), minv.z()),
            QVector3D(minv.x(), minv.y(), maxv.z()),
            QVector3D(minv.x(), maxv.y(), minv.z()),
            QVector3D(minv.x(), maxv.y(), maxv.z()),
            QVector3D(maxv.x(), minv.y(), minv.z()),
            QVector3D(maxv.x(), minv.y(), maxv.z()),
            QVector3D(maxv.x(), maxv.y(), minv.z()),
            QVector3D(maxv.x(), maxv.y(), maxv.z()),
        };

        if (camera_.projectionType() == app::ViewportCamera::ProjectionType::Perspective)
        {
            const float aspect = (pixelH > 0) ? (static_cast<float>(pixelW) / static_cast<float>(pixelH)) : 1.0f;
            const float vfovDeg = app::ViewportCamera::CalcVFovFromHFov(camera_.fovXDeg(), aspect);
            const float tanHalfHFov = std::tan(deg2Rad(camera_.fovXDeg() * 0.5f));
            const float tanHalfVFov = std::tan(deg2Rad(vfovDeg * 0.5f));

            for (const QVector3D& corner : corners)
            {
                const QVector3D view = viewPoint(corner);
                const float depth = std::max(-view.z(), 1.0e-4f);
                const float needX = std::abs(view.x()) / std::max(tanHalfHFov, 1.0e-6f);
                const float needY = std::abs(view.y()) / std::max(tanHalfVFov, 1.0e-6f);
                const float requiredDepth = std::max(needX, needY) * 1.01f;
                targetDistance = std::max(targetDistance, 1.0f + std::max(0.0f, requiredDepth - depth));
            }
        }
        else
        {
            for (const QVector3D& corner : corners)
            {
                const QVector3D view = viewPoint(corner);
                maxAbsViewX = std::max(maxAbsViewX, std::abs(view.x()));
                maxAbsViewY = std::max(maxAbsViewY, std::abs(view.y()));
            }
        }
    }

    if (camera_.projectionType() == app::ViewportCamera::ProjectionType::Perspective)
    {
        camera_.orbit.distance = std::max(targetDistance, 1.0f);
    }
    else
    {
        const float aspect = static_cast<float>(pixelW) / static_cast<float>(pixelH);
        const float hheight = static_cast<float>(pixelH) * 0.5f;

        const float desiredHalfExtent =
            std::max(0.1f, std::max(maxAbsViewY, maxAbsViewX / std::max(aspect, 1.0e-6f)) * kOrthographicLevelFitPaddingScale);
        const float scale = (desiredHalfExtent > 1e-6f) ? (hheight / (aspect * desiredHalfExtent)) : camera_.orthoScale();
        camera_.SetOrthoScale(std::max(scale, 0.01f));

        const float radius = haveAny ? std::max((maxv - minv).length() * 0.5f, 1.0f) : 1.0f;
        camera_.orbit.distance = std::max(radius * 1.04f, 1.0f);
    }

    markViewChanged();
    update();
}

void GlView::frameWorldBounds(const QVector3D& min, const QVector3D& max, const bool animate, const int durationMs)
{
    if (!std::isfinite(min.x()) || !std::isfinite(min.y()) || !std::isfinite(min.z())
        || !std::isfinite(max.x()) || !std::isfinite(max.y()) || !std::isfinite(max.z()))
    {
        return;
    }

    QVector3D mn(
        std::min(min.x(), max.x()),
        std::min(min.y(), max.y()),
        std::min(min.z(), max.z()));
    QVector3D mx(
        std::max(min.x(), max.x()),
        std::max(min.y(), max.y()),
        std::max(min.z(), max.z()));

    const QVector3D center = (mn + mx) * 0.5f;
    const QVector3D ext = mx - mn;
    float radius = ext.length() * 0.5f;
    if (!std::isfinite(radius) || radius <= 1e-6f)
    {
        radius = 0.1f;
    }

    // Keep current view orientation; only move the orbit target and zoom to fit.
    const QVector3D targetPan = center - camera_.orbit.center;

    const float dpr = devicePixelRatioF();
    const int pixelW = std::max(1, static_cast<int>(std::lround(static_cast<double>(width()) * dpr)));
    const int pixelH = std::max(1, static_cast<int>(std::lround(static_cast<double>(height()) * dpr)));
    camera_.SetCanvas(pixelW, pixelH);
    // Orbit state is the authoritative editor camera state. Rebuild the
    // derived eye/target/view before projecting; Update() alone only consumes
    // existing dirty flags and does not notice direct OrbitState edits.
    camera_.UpdateFromOrbit();

    float targetDistance = camera_.orbit.distance;
    float targetOrthoScale = camera_.orthoScale();

    if (camera_.projectionType() == app::ViewportCamera::ProjectionType::Perspective)
    {
        const float aspect = (pixelH > 0) ? (static_cast<float>(pixelW) / static_cast<float>(pixelH)) : 1.0f;
        const float vfovDeg = app::ViewportCamera::CalcVFovFromHFov(camera_.fovXDeg(), aspect);

        const float tanHalfHFov = std::tan(deg2Rad(camera_.fovXDeg() * 0.5f));
        const float tanHalfVFov = std::tan(deg2Rad(vfovDeg * 0.5f));

        const float distX = radius / std::max(tanHalfHFov, 1e-6f);
        const float distY = radius / std::max(tanHalfVFov, 1e-6f);

        targetDistance = std::max(std::max(distX, distY) * 1.02f, 0.01f);
    }
    else
    {
        const float aspect = (pixelH > 0) ? (static_cast<float>(pixelW) / static_cast<float>(pixelH)) : 1.0f;
        const float safeAspect = std::max(aspect, 1e-6f);
        const float hheight = static_cast<float>(pixelH) * 0.5f;

        bool invOk = false;
        const QMatrix4x4 invView = camera_.viewMatrix().inverted(&invOk);
        QVector3D right(1.0f, 0.0f, 0.0f);
        QVector3D up(0.0f, 1.0f, 0.0f);
        if (invOk)
        {
            const QVector4D r4 = invView.column(0);
            const QVector4D u4 = invView.column(1);
            right = QVector3D(r4.x(), r4.y(), r4.z());
            up = QVector3D(u4.x(), u4.y(), u4.z());
            if (!normalizeInPlace(right))
            {
                right = QVector3D(1.0f, 0.0f, 0.0f);
            }
            if (!normalizeInPlace(up))
            {
                up = QVector3D(0.0f, 1.0f, 0.0f);
            }
        }

        const std::array<QVector3D, 8> corners{
            QVector3D(mn.x(), mn.y(), mn.z()),
            QVector3D(mn.x(), mn.y(), mx.z()),
            QVector3D(mn.x(), mx.y(), mn.z()),
            QVector3D(mn.x(), mx.y(), mx.z()),
            QVector3D(mx.x(), mn.y(), mn.z()),
            QVector3D(mx.x(), mn.y(), mx.z()),
            QVector3D(mx.x(), mx.y(), mn.z()),
            QVector3D(mx.x(), mx.y(), mx.z()),
        };

        float maxAbsViewX = 0.01f;
        float maxAbsViewY = 0.01f;
        for (const QVector3D& corner : corners)
        {
            const QVector3D local = corner - center;
            maxAbsViewX = std::max(maxAbsViewX, std::abs(QVector3D::dotProduct(local, right)));
            maxAbsViewY = std::max(maxAbsViewY, std::abs(QVector3D::dotProduct(local, up)));
        }

        const float desiredHalfExtent =
            std::max(0.01f, std::max(maxAbsViewY, maxAbsViewX / safeAspect) * kOrthographicLevelFitPaddingScale);
        const float scale = (desiredHalfExtent > 1e-6f) ? (hheight / (safeAspect * desiredHalfExtent)) : camera_.orthoScale();
        targetOrthoScale = std::clamp(scale, 0.01f, 1.0e6f);
        targetDistance = std::max(camera_.orbit.distance, std::max(radius * 1.02f, 0.01f));
    }

    if (!std::isfinite(targetDistance) || targetDistance <= 0.0f)
    {
        targetDistance = std::max(camera_.orbit.distance, 0.01f);
    }

    if (!animate || durationMs <= 0)
    {
        cancelViewAnimation();
        camera_.orbit.pan = targetPan;
        camera_.orbit.distance = targetDistance;
        if (camera_.projectionType() == app::ViewportCamera::ProjectionType::Orthographic)
        {
            camera_.SetOrthoScale(targetOrthoScale);
        }
        markViewChanged();
        update();
        return;
    }

    startViewAnimation(camera_.orbit.yawDeg, camera_.orbit.pitchDeg, camera_.orbit.rollDeg, durationMs);
    viewAnim_.targetPan = targetPan;
    viewAnim_.targetDistance = targetDistance;
    viewAnim_.targetOrthoScale = targetOrthoScale;
}

void GlView::markViewChanged(const bool preserveGridOverrides) noexcept
{
    const auto refreshCameraShapePreview = qScopeGuard([this]()
    {
        if ( !isStagedCleaveInteractionActive() || !sectorCleaveToolEnabled_
            || !cleaveShapeOptions_.cameraPlane || !cleaveShapeOptions_.placementArmed
            || !stagedCleaveShapePreviewFrame_ )
        {
            return;
        }
        try
        {
            stagedCleaveShapePreviewFrame_ = stagedCleaveShapePlacementFrameAt(
                lastMousePos_, QGuiApplication::keyboardModifiers());
            stagedCleaveShapePreviewCenter_ = stagedCleaveShapePreviewFrame_
                ? std::optional<QVector3D>(render::cleaveRenderPoint(
                    stagedCleaveShapePreviewFrame_->center))
                : std::nullopt;
        }
        catch (const std::bad_alloc&)
        {
            stagedCleaveShapePreviewFrame_.reset();
            stagedCleaveShapePreviewCenter_.reset();
        }
    });
    ++viewSerial_;
    Q_EMIT viewTransformChanged();

    if (render::spriteGeometryDirtyOnViewChange(
            renderPipeline_ == RenderPipeline::GamePreview,
            spriteHasType0Billboards_))
    {
        spriteTrianglesDirty_ = true;
    }

    levelPeelCacheValid_ = false;
    levelPeelCache_.hits.clear();

    thingPeelCacheValid_ = false;
    thingPeelCache_.hits.clear();

    if (preserveGridOverrides)
    {
        return;
    }

    if (!gridResetOnCameraRotate_)
    {
        gridOverrideCamBasisValid_ = false;
        return;
    }

    const bool haveOverride = gridAlignToView_ || gridCenterOverride_.has_value();
    if (!haveOverride)
    {
        gridOverrideCamBasisValid_ = false;
        return;
    }

    QMatrix4x4 proj;
    QMatrix4x4 view;
    QVector3D eye{};
    QVector3D target{};
    float nearPlane{};
    float farPlane{};
    buildCameraMatrices(proj, view, eye, target, nearPlane, farPlane);

    QVector3D forward = target - eye;
    bool invOk = false;
    const QMatrix4x4 invView = view.inverted(&invOk);
    QVector3D up(0.0f, 0.0f, 1.0f);
    if (invOk)
    {
        const QVector4D u4 = invView.column(1);
        up = QVector3D(u4.x(), u4.y(), u4.z());
    }

    if (!normalizeInPlace(forward) || !normalizeInPlace(up))
    {
        return;
    }

    if (!gridOverrideCamBasisValid_)
    {
        gridOverrideCamBasisValid_ = true;
        gridOverrideCamForward_ = forward;
        gridOverrideCamUp_ = up;
        return;
    }

    constexpr float kCosEps = 0.9999f;
    const float fwdDot = QVector3D::dotProduct(forward, gridOverrideCamForward_);
    const float upDot = QVector3D::dotProduct(up, gridOverrideCamUp_);
    if (fwdDot < kCosEps || upDot < kCosEps)
    {
        gridAlignToView_ = false;
        gridCenterOverride_.reset();
        gridOverrideCamBasisValid_ = false;
        gridCacheValid_ = false;
        // Keep Map Settings/View > Grid's Reset action in sync when camera
        // rotation implicitly clears a temporary grid alignment.
        Q_EMIT viewportStateChanged();
    }
}

void GlView::setThingGizmoWorldSize(const float size)
{
    const float clamped = std::clamp(size, 0.001f, 1.0f);
    if (thingGizmoWorldSize_ == clamped)
    {
        return;
    }

    thingGizmoWorldSize_ = clamped;
    thingTypeGizmoCacheValid_ = false;
    architecturalLightingOwnerGizmoCacheValid_ = false;
    architecturalLightingOwnerGizmoVertices_.clear();
    markViewChanged();
    Q_EMIT viewportStateChanged();
    update();
}

void GlView::setThingGizmoHeightAboveModel(const float height)
{
    constexpr float kMaximumHeight = 100.0f;
    const float clamped = std::isfinite(height)
        ? std::clamp(height, 0.0f, kMaximumHeight)
        : 0.0f;
    if (std::abs(thingGizmoHeightAboveModel_ - clamped) <= 1.0e-6f)
    {
        return;
    }

    thingGizmoHeightAboveModel_ = clamped;
    thingTypeGizmoCacheValid_ = false;
    architecturalLightingOwnerGizmoCacheValid_ = false;
    markViewChanged();
    Q_EMIT viewportStateChanged();
    update();
}

void GlView::setThingGizmoStackSpacing(const float spacing)
{
    constexpr float kMaximumSpacing = 1.0f;
    const float clamped = std::isfinite(spacing)
        ? std::clamp(spacing, 0.0f, kMaximumSpacing)
        : 0.0f;
    if (std::abs(thingGizmoStackSpacing_ - clamped) <= 1.0e-6f)
    {
        return;
    }

    thingGizmoStackSpacing_ = clamped;
    thingTypeGizmoCacheValid_ = false;
    architecturalLightingOwnerGizmoCacheValid_ = false;
    markViewChanged();
    Q_EMIT viewportStateChanged();
    update();
}

void GlView::setThingLightGizmoOffsetAboveModel(const float offset)
{
    constexpr float kMaximumOffset = 100.0f;
    const float clamped = std::isfinite(offset)
        ? std::clamp(offset, 0.0f, kMaximumOffset)
        : 0.0f;
    if (std::abs(thingLightGizmoOffsetAboveModel_ - clamped) <= 1.0e-6f)
    {
        return;
    }

    thingLightGizmoOffsetAboveModel_ = clamped;
    thingTypeGizmoCacheValid_ = false;
    architecturalLightingOwnerGizmoCacheValid_ = false;
    markViewChanged();
    Q_EMIT viewportStateChanged();
    update();
}

void GlView::setArchThingGizmoOffsetAboveModel(const float offset)
{
    constexpr float kMaximumOffset = 100.0f;
    const float clamped = std::isfinite(offset)
        ? std::clamp(offset, 0.0f, kMaximumOffset)
        : 0.0f;
    if (std::abs(archThingGizmoOffsetAboveModel_ - clamped) <= 1.0e-6f)
    {
        return;
    }

    archThingGizmoOffsetAboveModel_ = clamped;
    thingTypeGizmoCacheValid_ = false;
    architecturalLightingOwnerGizmoCacheValid_ = false;
    markViewChanged();
    Q_EMIT viewportStateChanged();
    update();
}

void GlView::setLevelGeometryVertexMarkerSizePx(const float sizePx)
{
    const float clamped = std::isfinite(sizePx)
        ? std::clamp(
            sizePx,
            kLevelGeometryVertexMarkerMinSizePx,
            kLevelGeometryVertexMarkerMaxSizePx)
        : kLevelGeometryVertexMarkerDefaultSizePx;
    if (std::abs(levelGeometryVertexMarkerSizePx_ - clamped) <= 1.0e-5f)
    {
        return;
    }

    levelGeometryVertexMarkerSizePx_ = clamped;
    markViewChanged();
    Q_EMIT viewportStateChanged();
    update();
}

void GlView::setLightGizmoWorldSize(const float size)
{
    const float clamped = std::clamp(size, 0.001f, 1.0f);
    if (lightGizmoWorldSize_ == clamped)
    {
        return;
    }

    lightGizmoWorldSize_ = clamped;
    architecturalLightingOwnerGizmoCacheValid_ = false;
    architecturalLightingOwnerGizmoVertices_.clear();
    markViewChanged();
    Q_EMIT viewportStateChanged();
    update();
}

void GlView::setLightGizmoUseLightColor(const bool enabled)
{
    if (lightGizmoUseLightColor_ == enabled)
    {
        return;
    }

    lightGizmoUseLightColor_ = enabled;
    markViewChanged();
    Q_EMIT viewportStateChanged();
    update();
}

void GlView::setArchitecturalLightingMarkerSizePx(const float sizePx)
{
    const float clamped = std::isfinite(sizePx)
        ? std::clamp(
            sizePx,
            kArchitecturalLightingMarkerMinSizePx,
            kArchitecturalLightingMarkerMaxSizePx)
        : kArchitecturalLightingMarkerDefaultSizePx;
    if (std::abs(architecturalLightingMarkerSizePx_ - clamped) <= 1.0e-5f)
    {
        return;
    }

    architecturalLightingMarkerSizePx_ = clamped;
    markViewChanged();
    Q_EMIT viewportStateChanged();
    update();
}

void GlView::setArchitecturalLightingMarkerUseAuthoredColor(const bool enabled)
{
    if (architecturalLightingMarkerUseAuthoredColor_ == enabled)
    {
        return;
    }

    architecturalLightingMarkerUseAuthoredColor_ = enabled;
    markViewChanged();
    Q_EMIT viewportStateChanged();
    update();
}

void GlView::setArchitecturalLightingMarkerColor(const QColor& color)
{
    QColor c = color;
    if (!c.isValid())
    {
        c = QColor::fromRgb(255, 204, 51);
    }
    c.setAlpha(255);
    if (architecturalLightingMarkerColor_ == c)
    {
        return;
    }

    architecturalLightingMarkerColor_ = c;
    update();
}

void GlView::setLightGizmoCategory(const render::LightGizmoCategory category)
{
    if (lightGizmoCategory_ == category)
    {
        setProperty(
            "sed.selectedLightGizmoKind",
            static_cast<int>(selectedLightGizmoKind_));
        return;
    }

    lightGizmoCategory_ = category;
    thingTypeGizmoCacheValid_ = false;
    architecturalLightingOwnerGizmoCacheValid_ = false;
    architecturalLightingOwnerGizmoVertices_.clear();
    haveLastLightGizmoPick_ = false;
    if (category == render::LightGizmoCategory::Baked)
    {
        selectedLightGizmoKind_ = render::LightGizmoKind::Baked;
    }
    else if (category == render::LightGizmoCategory::Sector)
    {
        selectedLightGizmoKind_ = render::LightGizmoKind::SectorPoint;
    }
    else if (category == render::LightGizmoCategory::Architectural)
    {
        // Architectural owners are a MotS overlay, not a runtime emitter;
        // retain a harmless baked kind for legacy transform-state callers.
        selectedLightGizmoKind_ = render::LightGizmoKind::Baked;
    }
    else if (selectedLightGizmoKind_ != render::LightGizmoKind::ThingRegular
        && selectedLightGizmoKind_ != render::LightGizmoKind::ThingHead)
    {
        selectedLightGizmoKind_ = render::LightGizmoKind::ThingRegular;
    }
    setProperty(
        "sed.selectedLightGizmoKind",
        static_cast<int>(selectedLightGizmoKind_));
    update();
}

void GlView::setLightGizmoEditingActive(const bool enabled)
{
    if (lightGizmoEditingActive_ == enabled)
    {
        return;
    }
    lightGizmoEditingActive_ = enabled;
    thingTypeGizmoCacheValid_ = false;
    architecturalLightingOwnerGizmoCacheValid_ = false;
    architecturalLightingOwnerGizmoVertices_.clear();
    haveLastLightGizmoPick_ = false;
    update();
}

void GlView::setArchitecturalLightingEditingActive(const bool enabled)
{
    if (architecturalLightingEditingActive_ == enabled)
    {
        if (!enabled)
        {
            // Game Preview temporarily suspends ARCH editing. Preserve the
            // editor's authoring channel so it can be restored when the user
            // returns; MainWindow resets it explicitly on a true submode exit.
            clearArchitecturalLightingPreview(false);
            clearArchitecturalLightingSelectedVertices();
        }
        return;
    }

    if (!enabled)
    {
        if (architecturalLightingEditingActive_)
        {
            pendingArchitecturalLightingRecovery_.reset();
            pendingArchitecturalLightingSelectionReady_ = false;
            pendingArchitecturalLightingOwnerSelection_.clear();
        }
        clearArchitecturalLightingPreview(false);
        clearArchitecturalLightingSelectedVertices();
        architecturalLightingLastOwnerClick_.reset();
    }
    // ARCH owner markers have their own cache and never replace ordinary Thing
    // icons. Invalidate both payloads on a mode transition so the active mode
    // can add unbound owners and the ordinary path can resume unchanged.
    thingTypeGizmoMetadataValid_ = false;
    thingTypeGizmoCacheValid_ = false;
    thingTypeGizmoEntryByPickId_.clear();
    thingTypeGizmoDirtyOwners_.clear();
    architecturalLightingOwnerGizmoMetadataValid_ = false;
    architecturalLightingOwnerGizmoCacheValid_ = false;
    architecturalLightingOwnerGizmoEntries_.clear();
    architecturalLightingOwnerGizmoEntryByThing_.clear();
    architecturalLightingOwnerGizmoVertices_.clear();
    // A submode change cancels any marquee transaction. The ARCH release
    // branch owns its own multi-owner signal, so leaving it with a stale
    // rectangle would otherwise leave the overlay visible in the next mode.
    leftDragged_ = false;
    boxSelectActive_ = false;
    boxSelectVisible_ = false;
    boxSelectFromTransformTool_ = false;
    architecturalLightingEditingActive_ = enabled;
    refreshArchitecturalLightingRenderCache();
    applyArchitecturalLightingTrianglePreview();
    rebuildArchitecturalLightingPointVertices();
    rebuildSelectionOverlays();
    if (glReady_)
    {
        ScopedArchitecturalOpenGlContext context(*this);
        if (context.ready())
        {
            uploadArchitecturalLightingTrianglePreview();
            uploadArchitecturalLightingPointVertices();
            uploadSelectionOverlays();
        }
    }
    update();
}

void GlView::setArchitecturalLightingAuthoringMode(
    const level::MotsArchLightingVertexMode mode)
{
    if ((mode != level::MotsArchLightingVertexMode::Rgb
            && mode != level::MotsArchLightingVertexMode::Mono)
        || !architecturalLightingEditingActive_
        || architecturalLightingAuthoringMode_ == mode)
    {
        return;
    }

    architecturalLightingAuthoringMode_ = mode;
    applyArchitecturalLightingTrianglePreview();
    rebuildArchitecturalLightingPointVertices();
    if (glReady_)
    {
        ScopedArchitecturalOpenGlContext context(*this);
        if (context.ready())
        {
            uploadArchitecturalLightingTrianglePreview();
            uploadArchitecturalLightingPointVertices();
        }
    }
    update();
}

void GlView::setArchitecturalLightingTargetMode(
    const ArchitecturalLightingTargetMode mode)
{
    if (architecturalLightingTargetMode_ == mode)
    {
        return;
    }

    architecturalLightingTargetMode_ = mode;
    hoveredArchitecturalLightingVertex_.reset();
    architecturalLightingLastOwnerClick_.reset();
    resetPickCycle();
    leftDragged_ = false;
    boxSelectActive_ = false;
    boxSelectVisible_ = false;
    boxSelectFromTransformTool_ = false;
    setCursor(Qt::ArrowCursor);
    update();
}

void GlView::setArchitecturalLightingSelectedVertices(
    const std::span<const ArchitecturalLightingVertexIdentity> vertices)
{
    // Public/programmatic publication represents an explicit identity set.
    // The input path reapplies the implicit-owner provenance only after this
    // validated set has been accepted.
    architecturalLightingSelectedVerticesImplicitOwnerExpansion_ = false;
    std::vector<ArchitecturalLightingVertexIdentity> next{};
    next.reserve(vertices.size());
    std::unordered_set<ArchitecturalLightingVertexIdentity,
        ArchitecturalLightingVertexIdentityHash> nextSet{};
    nextSet.reserve(vertices.size());
    std::unordered_set<ArchitecturalLightingVertexIdentity,
        ArchitecturalLightingVertexIdentityHash> validIdentities{};
    std::size_t cachedVertexCount = 0;
    for (const ArchitecturalLightingRenderCache& cache
         : architecturalLightingSelectedOwnerCaches_)
    {
        cachedVertexCount += cache.vertices.size();
    }
    validIdentities.reserve(cachedVertexCount);
    for (const ArchitecturalLightingRenderCache& cache
         : architecturalLightingSelectedOwnerCaches_)
    {
        for (const ArchitecturalLightingRenderVertex& vertex : cache.vertices)
        {
            validIdentities.insert(architecturalLightingVertexIdentity(vertex));
        }
    }
    for (const ArchitecturalLightingVertexIdentity& identity : vertices)
    {
        if (identity.thingIndex < 0 || identity.lodIndex < 0
            || identity.meshIndex < 0 || identity.vertexIndex < 0
            || !validIdentities.contains(identity)
            || !nextSet.insert(identity).second)
        {
            continue;
        }
        next.push_back(identity);
    }

    if (next == architecturalLightingSelectedVertices_)
    {
        return;
    }

    architecturalLightingSelectedVertices_ = std::move(next);
    architecturalLightingSelectedVertexSet_ = std::move(nextSet);
    rebuildArchitecturalLightingPointVertices();
    if (glReady_)
    {
        ScopedArchitecturalOpenGlContext context(*this);
        if (context.ready())
        {
            uploadArchitecturalLightingPointVertices();
        }
    }
    update();
    Q_EMIT architecturalLightingVertexSelectionChanged();
}

void GlView::clearArchitecturalLightingSelectedVertices()
{
    architecturalLightingSelectedVerticesImplicitOwnerExpansion_ = false;
    if (architecturalLightingSelectedVertices_.empty())
    {
        architecturalLightingSelectedVertexSet_.clear();
        return;
    }

    architecturalLightingSelectedVertices_.clear();
    architecturalLightingSelectedVertexSet_.clear();
    rebuildArchitecturalLightingPointVertices();
    if (glReady_)
    {
        ScopedArchitecturalOpenGlContext context(*this);
        if (context.ready())
        {
            uploadArchitecturalLightingPointVertices();
        }
    }
    update();
    Q_EMIT architecturalLightingVertexSelectionChanged();
}

void GlView::setArchitecturalLightingPreview(
    const std::span<const ArchitecturalLightingPreviewVertex> vertices)
{
    setArchitecturalLightingPreview(
        vertices, level::MotsArchLightingVertexMode::Rgb);
}

void GlView::setArchitecturalLightingPreview(
    const std::span<const ArchitecturalLightingPreviewVertex> vertices,
    const level::MotsArchLightingVertexMode mode)
{
    architecturalLightingPreview_.assign(vertices.begin(), vertices.end());
    architecturalLightingPreviewByIdentity_.clear();
    architecturalLightingPreviewByIdentity_.reserve(vertices.size());
    for (const ArchitecturalLightingPreviewVertex& preview : vertices)
    {
        architecturalLightingPreviewByIdentity_.insert_or_assign(
            architecturalLightingVertexIdentity(preview.identity),
            preview.lighting);
    }
    architecturalLightingPreviewOwnerRevisions_.clear();
    architecturalLightingPreviewMode_ = mode;
    if (architecturalLightingPreview_.empty())
    {
        architecturalLightingPreviewModelIdentity_ = nullptr;
        architecturalLightingPreviewModelRevision_ = 0;
    }
    else if (!architecturalLightingSelectedOwnerCaches_.empty())
    {
        for (const ArchitecturalLightingPreviewVertex& preview
             : architecturalLightingPreview_)
        {
            const auto cacheIndex =
                architecturalLightingSelectedOwnerCacheByThing_.find(
                    preview.identity.thingIndex);
            if (cacheIndex == architecturalLightingSelectedOwnerCacheByThing_.end()
                || cacheIndex->second >= architecturalLightingSelectedOwnerCaches_.size())
            {
                continue;
            }
            const ArchitecturalLightingRenderCache& cache =
                architecturalLightingSelectedOwnerCaches_[cacheIndex->second];
            architecturalLightingPreviewOwnerRevisions_.insert_or_assign(
                preview.identity.thingIndex,
                ArchitecturalLightingPreviewOwnerRevision{
                    cache.modelIdentity,
                    cache.archLightingRevision });
        }

        if (architecturalLightingRenderCache_)
        {
            architecturalLightingPreviewModelIdentity_ =
                architecturalLightingRenderCache_->modelIdentity;
            architecturalLightingPreviewModelRevision_ =
                architecturalLightingRenderCache_->archLightingRevision;
        }
        else
        {
            architecturalLightingPreviewModelIdentity_ = nullptr;
            architecturalLightingPreviewModelRevision_ = 0;
        }
    }
    else
    {
        architecturalLightingPreviewModelIdentity_ = nullptr;
        architecturalLightingPreviewModelRevision_ = 0;
    }
    applyArchitecturalLightingTrianglePreview();
    rebuildArchitecturalLightingPointVertices();
    if (glReady_)
    {
        ScopedArchitecturalOpenGlContext context(*this);
        if (context.ready())
        {
            uploadArchitecturalLightingTrianglePreview();
            uploadArchitecturalLightingPointVertices();
        }
    }
    update();
}

void GlView::clearArchitecturalLightingPreview(
    const bool restoreAuthoringPresentation) noexcept
{
    const bool hadStagedState = !architecturalLightingPreview_.empty()
        || !architecturalLightingPreviewByIdentity_.empty()
        || !architecturalLightingTrianglePatches_.empty()
        || !architecturalLightingTriangleDirtyIndices_.empty()
        || !architecturalLightingTriangleDirtyIndexSet_.empty()
        || architecturalLightingPreviewModelIdentity_ != nullptr
        || architecturalLightingPreviewModelRevision_ != 0
        || !architecturalLightingPreviewOwnerRevisions_.empty()
        || architecturalLightingPointCount_ > 0
        || !architecturalLightingPointVertices_.empty()
        || !architecturalLightingPointIdentities_.empty()
        || architecturalLightingPreviewMode_
            != level::MotsArchLightingVertexMode::Rgb;
    if (!hadStagedState)
    {
        return;
    }

    // Restore patches even if the preview value vector was already cleared by
    // a publication/invalidation path. The patch list is the authoritative
    // record of modified triangle vertices and must not be gated by the UI
    // staging vector's current size.
    architecturalLightingPreview_.clear();
    architecturalLightingPreviewByIdentity_.clear();
    architecturalLightingPreviewOwnerRevisions_.clear();
    architecturalLightingPreviewMode_ = level::MotsArchLightingVertexMode::Rgb;
    architecturalLightingPreviewModelIdentity_ = nullptr;
    architecturalLightingPreviewModelRevision_ = 0;
    restoreArchitecturalLightingTrianglePreview();
    if (restoreAuthoringPresentation
        && renderPipeline_ == RenderPipeline::Editor
        && architecturalLightingEditingActive_
        && architecturalLightingAuthoringMode_
            == level::MotsArchLightingVertexMode::Mono)
    {
        // Clearing staged channel values must not clear the independent
        // Mono authoring presentation. Resource/model publication paths pass
        // false and rebuild against the replacement before applying again.
        applyArchitecturalLightingTrianglePreview();
    }
    rebuildArchitecturalLightingPointVertices();
    if (glReady_)
    {
        ScopedArchitecturalOpenGlContext context(*this);
        if (context.ready())
        {
            uploadArchitecturalLightingTrianglePreview();
            uploadArchitecturalLightingPointVertices();
        }
    }
    update();
}

void GlView::setAlwaysShowLightGizmoCategory(
    const render::LightGizmoCategory category,
    const bool enabled)
{
    const std::size_t index = static_cast<std::size_t>(category);
    if (index >= alwaysShowLightGizmoCategories_.size()
        || alwaysShowLightGizmoCategories_[index] == enabled)
    {
        return;
    }

    alwaysShowLightGizmoCategories_[index] = enabled;
    if (category == render::LightGizmoCategory::Thing)
    {
        thingTypeGizmoCacheValid_ = false;
        architecturalLightingOwnerGizmoCacheValid_ = false;
        architecturalLightingOwnerGizmoVertices_.clear();
    }
    if (category == render::LightGizmoCategory::Architectural)
    {
        architecturalLightingOwnerGizmoMetadataValid_ = false;
        architecturalLightingOwnerGizmoCacheValid_ = false;
        architecturalLightingOwnerGizmoEntries_.clear();
        architecturalLightingOwnerGizmoEntryByThing_.clear();
        architecturalLightingOwnerGizmoVertices_.clear();
    }
    Q_EMIT viewportStateChanged();
    update();
}

void GlView::setShowWireframe(const bool enabled)
{
    if (showWireframe_ == enabled)
    {
        return;
    }

    showWireframe_ = enabled;
    markViewChanged();
    Q_EMIT viewportStateChanged();
    update();
}

void GlView::setShowVertices(const bool enabled)
{
    if (showVertices_ == enabled)
    {
        return;
    }

    showVertices_ = enabled;
    markViewChanged();
    Q_EMIT viewportStateChanged();
    update();
}

void GlView::setShowSolid(const bool enabled)
{
    if (showSolid_ == enabled)
    {
        return;
    }

    showSolid_ = enabled;
    markViewChanged();
    Q_EMIT viewportStateChanged();
    update();
}

void GlView::setShowTextures(const bool enabled)
{
    if (showTextures_ == enabled)
    {
        return;
    }

    showTextures_ = enabled;
    alphaAdjoinWireSupportsDirty_ = true;
    markViewChanged();
    Q_EMIT viewportStateChanged();
    update();
}

void GlView::setShowFog(const bool enabled)
{
    if (showFog_ == enabled)
    {
        return;
    }

    showFog_ = enabled;
    Q_EMIT viewportStateChanged();
    update();
}

void GlView::setDefaultShowFog(const RenderPipeline pipeline, const bool enabled)
{
    ViewportState& state = (pipeline == RenderPipeline::GamePreview) ? previewState_ : editorState_;
    bool& defaultValue = (pipeline == RenderPipeline::GamePreview) ? previewDefaultShowFog_ : editorDefaultShowFog_;

    defaultValue = enabled;
    // Seed the slot even before its first complete pipeline initialization.
    // ViewportState deliberately has a compatibility-safe `true` field
    // default, but an explicit Editor/Preview preference must remain
    // authoritative if an early or deferred transition initializes that slot
    // before the user first enters the pipeline.
    state.showFog = enabled;
    if (renderPipeline_ == pipeline)
    {
        setShowFog(enabled);
    }
}

void GlView::setDefaultShowThingCollisionShapes(
    const RenderPipeline pipeline,
    const bool enabled)
{
    const bool preview = pipeline == RenderPipeline::GamePreview;
    bool& defaultValue = preview
        ? previewDefaultShowThingCollisionShapes_
        : editorDefaultShowThingCollisionShapes_;
    ViewportState& state = preview ? previewState_ : editorState_;
    defaultValue = enabled;
    state.showThingCollisionShapes = enabled;
    if (renderPipeline_ == pipeline)
    {
        setShowThingCollisionShapes(enabled);
    }
}

void GlView::setDefaultShowThingVisualBounds(
    const RenderPipeline pipeline,
    const bool enabled)
{
    const bool preview = pipeline == RenderPipeline::GamePreview;
    bool& defaultValue = preview
        ? previewDefaultShowThingVisualBounds_
        : editorDefaultShowThingVisualBounds_;
    ViewportState& state = preview ? previewState_ : editorState_;
    defaultValue = enabled;
    state.showThingVisualBounds = enabled;
    if (renderPipeline_ == pipeline)
    {
        setShowThingVisualBounds(enabled);
    }
}

void GlView::setShowThings(const bool enabled)
{
    if (showThings_ == enabled)
    {
        return;
    }

    const QVector3D oldTarget = camera_.orbit.target();
    const float oldYawDeg = camera_.orbit.yawDeg;
    const float oldPitchDeg = camera_.orbit.pitchDeg;
    const float oldDistance = camera_.orbit.distance;

    showThings_ = enabled;
    rebuildMeshes();

    camera_.orbit.yawDeg = oldYawDeg;
    camera_.orbit.pitchDeg = oldPitchDeg;
    camera_.orbit.distance = oldDistance;
    camera_.orbit.pan = oldTarget - camera_.orbit.center;
    camera_.orbit.distance = std::max(camera_.orbit.distance, camera_.orbit.radius * 0.05f);

    if (glReady_)
    {
        makeCurrent();
        uploadMeshes();
        doneCurrent();
    }

    markViewChanged();
    Q_EMIT viewportStateChanged();
    update();
}

void GlView::setShowHiddenThings(const bool enabled)
{
    if (showHiddenThings_ == enabled)
    {
        return;
    }

    const QVector3D oldTarget = camera_.orbit.target();
    const float oldYawDeg = camera_.orbit.yawDeg;
    const float oldPitchDeg = camera_.orbit.pitchDeg;
    const float oldDistance = camera_.orbit.distance;

    showHiddenThings_ = enabled;
    if (renderPipeline_ == RenderPipeline::Editor)
    {
        editorState_.showHiddenThings = enabled;
    }
    else
    {
        previewState_.showHiddenThings = enabled;
    }

    rebuildMeshes();

    camera_.orbit.yawDeg = oldYawDeg;
    camera_.orbit.pitchDeg = oldPitchDeg;
    camera_.orbit.distance = oldDistance;
    camera_.orbit.pan = oldTarget - camera_.orbit.center;
    camera_.orbit.distance = std::max(camera_.orbit.distance, camera_.orbit.radius * 0.05f);

    if (glReady_)
    {
        makeCurrent();
        uploadMeshes();
        doneCurrent();
    }

    markViewChanged();
    Q_EMIT viewportStateChanged();
    update();
}

void GlView::setMotsTreeCollisionPreviewMeshRadiusFactor(const float factor)
{
    constexpr float kMinimumFactor = 0.01f;
    constexpr float kMaximumFactor = 10.0f;
    const float defaultFactor = static_cast<float>(
        render::thing_overlay::kDefaultMotsTreeCollisionPreviewMeshRadiusFactor);
    const float clamped = std::isfinite(factor)
        ? std::clamp(factor, kMinimumFactor, kMaximumFactor)
        : defaultFactor;
    if (std::abs(motsTreeCollisionPreviewMeshRadiusFactor_ - clamped) <= 1.0e-6f)
    {
        return;
    }

    motsTreeCollisionPreviewMeshRadiusFactor_ = clamped;
    if (thingOverlayGeometryPrepared_ && level_)
    {
        const QVector3D oldTarget = camera_.orbit.target();
        const float oldYawDeg = camera_.orbit.yawDeg;
        const float oldPitchDeg = camera_.orbit.pitchDeg;
        const float oldRollDeg = camera_.orbit.rollDeg;
        const float oldDistance = camera_.orbit.distance;

        rebuildMeshes(false, MeshRebuildKind::ThingsOnly);
        camera_.orbit.yawDeg = oldYawDeg;
        camera_.orbit.pitchDeg = oldPitchDeg;
        camera_.orbit.rollDeg = oldRollDeg;
        camera_.orbit.distance = oldDistance;
        camera_.orbit.pan = oldTarget - camera_.orbit.center;
        if (glReady_)
        {
            makeCurrent();
            uploadMeshes();
            doneCurrent();
        }
    }

    markViewChanged();
    Q_EMIT viewportStateChanged();
    update();
}

void GlView::setShowThingCollisionShapes(const bool enabled)
{
    if (showThingCollisionShapes_ == enabled)
    {
        return;
    }

    showThingCollisionShapes_ = enabled;
    if (renderPipeline_ == RenderPipeline::Editor)
    {
        editorState_.showThingCollisionShapes = enabled;
    }
    else
    {
        previewState_.showThingCollisionShapes = enabled;
    }
    if (!showThingCollisionShapes_ && !showThingVisualBounds_)
    {
        thingOverlayGeometryPrepared_ = false;
    }

    if (enabled && !thingOverlayGeometryPrepared_)
    {
        const QVector3D oldTarget = camera_.orbit.target();
        const float oldYawDeg = camera_.orbit.yawDeg;
        const float oldPitchDeg = camera_.orbit.pitchDeg;
        const float oldRollDeg = camera_.orbit.rollDeg;
        const float oldDistance = camera_.orbit.distance;

        thingOverlayGeometryPrepared_ = true;
        rebuildMeshes(false, MeshRebuildKind::ThingsOnly);
        camera_.orbit.yawDeg = oldYawDeg;
        camera_.orbit.pitchDeg = oldPitchDeg;
        camera_.orbit.rollDeg = oldRollDeg;
        camera_.orbit.distance = oldDistance;
        camera_.orbit.pan = oldTarget - camera_.orbit.center;
        if (glReady_)
        {
            makeCurrent();
            uploadMeshes();
            doneCurrent();
        }
    }

    markViewChanged();
    Q_EMIT viewportStateChanged();
    update();
}

void GlView::setShowTemplateCollisionBoundsGuide(bool enabled)
{
    // This draw path exists only for the Template Editor's private GamePreview
    // view. Keep the main level viewport and other preview widgets unaffected.
    enabled = enabled && renderPipeline_ == RenderPipeline::GamePreview;
    if (showTemplateCollisionBoundsGuide_ == enabled)
    {
        return;
    }

    showTemplateCollisionBoundsGuide_ = enabled;
    if (enabled && level_)
    {
        const QVector3D oldTarget = camera_.orbit.target();
        const float oldYawDeg = camera_.orbit.yawDeg;
        const float oldPitchDeg = camera_.orbit.pitchDeg;
        const float oldRollDeg = camera_.orbit.rollDeg;
        const float oldDistance = camera_.orbit.distance;

        rebuildMeshes(false, MeshRebuildKind::ThingsOnly);
        camera_.orbit.yawDeg = oldYawDeg;
        camera_.orbit.pitchDeg = oldPitchDeg;
        camera_.orbit.rollDeg = oldRollDeg;
        camera_.orbit.distance = oldDistance;
        camera_.orbit.pan = oldTarget - camera_.orbit.center;
        if (glReady_)
        {
            makeCurrent();
            uploadMeshes();
            doneCurrent();
        }
    }

    markViewChanged();
    update();
}

void GlView::setShowThingVisualBounds(const bool enabled)
{
    if (showThingVisualBounds_ == enabled)
    {
        return;
    }

    showThingVisualBounds_ = enabled;
    if (renderPipeline_ == RenderPipeline::Editor)
    {
        editorState_.showThingVisualBounds = enabled;
    }
    else
    {
        previewState_.showThingVisualBounds = enabled;
    }
    if (!showThingCollisionShapes_ && !showThingVisualBounds_)
    {
        thingOverlayGeometryPrepared_ = false;
    }

    if (enabled && !thingOverlayGeometryPrepared_)
    {
        const QVector3D oldTarget = camera_.orbit.target();
        const float oldYawDeg = camera_.orbit.yawDeg;
        const float oldPitchDeg = camera_.orbit.pitchDeg;
        const float oldRollDeg = camera_.orbit.rollDeg;
        const float oldDistance = camera_.orbit.distance;

        thingOverlayGeometryPrepared_ = true;
        rebuildMeshes(false, MeshRebuildKind::ThingsOnly);
        camera_.orbit.yawDeg = oldYawDeg;
        camera_.orbit.pitchDeg = oldPitchDeg;
        camera_.orbit.rollDeg = oldRollDeg;
        camera_.orbit.distance = oldDistance;
        camera_.orbit.pan = oldTarget - camera_.orbit.center;
        if (glReady_)
        {
            makeCurrent();
            uploadMeshes();
            doneCurrent();
        }
    }

    markViewChanged();
    Q_EMIT viewportStateChanged();
    update();
}

bool GlView::shouldShowThing(const Thing& thing) const noexcept
{
    return level_
        && render::thingVisibleInViewport(*level_, templates_, thing, showHiddenThings_);
}

void GlView::setShowAxisWidget(const bool enabled)
{
    if (showAxisWidget_ == enabled)
    {
        return;
    }

    showAxisWidget_ = enabled;
    update();
}

void GlView::setShowToastOverlays(const bool enabled)
{
    if (showToastOverlays_ == enabled)
    {
        return;
    }

    showToastOverlays_ = enabled;
    if (!showToastOverlays_)
    {
        for (ToastState& t : toastStates_)
        {
            t.text.clear();
            t.durationMs = 0;
        }
    }
    update();
}

void GlView::setShowGrid(const bool enabled)
{
    if (showGrid_ == enabled)
    {
        return;
    }

    showGrid_ = enabled;
    gridCacheValid_ = false;
    update();
}

void GlView::setGridFollowCamera(const bool enabled)
{
    if (gridFollowCamera_ == enabled)
    {
        return;
    }

    gridFollowCamera_ = enabled;
    gridCacheValid_ = false;
    update();
}

void GlView::setGridRotateWithView(const bool enabled)
{
    if (gridRotateWithView_ == enabled)
    {
        return;
    }

    gridRotateWithView_ = enabled;
    gridCacheValid_ = false;
    update();
}

void GlView::setGridKeepViewAlignedVisible(const bool enabled)
{
    if (gridKeepViewAlignedVisible_ == enabled)
    {
        return;
    }

    gridKeepViewAlignedVisible_ = enabled;
    gridCacheValid_ = false;
    update();
}

void GlView::setGridPlane(const GridPlane plane)
{
    if (gridPlane_ == plane)
    {
        return;
    }

    gridPlane_ = plane;
    gridCacheValid_ = false;
    update();
}

void GlView::alignGridPlaneToView()
{
    // Align the grid plane to the current camera view (screen-space plane in world coordinates).
    // This is a temporary override; optional reset-on-camera-rotate is handled separately.
    [[maybe_unused]] const auto gridStateGuard = qScopeGuard(
        [this]()
        {
            Q_EMIT viewportStateChanged();
        });

    QMatrix4x4 proj;
    QMatrix4x4 view;
    QVector3D eye{};
    QVector3D target{};
    float nearPlane{};
    float farPlane{};
    buildCameraMatrices(proj, view, eye, target, nearPlane, farPlane);

    bool invOk = false;
    const QMatrix4x4 invView = view.inverted(&invOk);
    if (!invOk)
    {
        return;
    }

    QVector4D r4 = invView.column(0);
    QVector4D u4 = invView.column(1);

    QVector3D right(r4.x(), r4.y(), r4.z());
    QVector3D up(u4.x(), u4.y(), u4.z());

    if (!normalizeInPlace(right))
    {
        return;
    }
    // Re-orthogonalize and normalize up.
    up -= right * QVector3D::dotProduct(up, right);
    if (!normalizeInPlace(up))
    {
        return;
    }

    gridAlignToView_ = true;
    gridAlignAxisA_ = right;
    gridAlignAxisB_ = up;

    if (gridResetOnCameraRotate_)
    {
        QVector3D forward = target - eye;
        if (normalizeInPlace(forward))
        {
            gridOverrideCamBasisValid_ = true;
            gridOverrideCamForward_ = forward;
            gridOverrideCamUp_ = up;
        }
    }

    gridCacheValid_ = false;
    update();
}

void GlView::alignViewToGridPlane()
{
    QVector3D axisA{1.0f, 0.0f, 0.0f};
    QVector3D axisB{0.0f, 1.0f, 0.0f};
    if (gridAlignToView_)
    {
        axisA = gridAlignAxisA_;
        axisB = gridAlignAxisB_;
    }
    else if (gridRotateWithView_)
    {
        QMatrix4x4 proj;
        QMatrix4x4 view;
        QVector3D eye{};
        QVector3D target{};
        float nearPlane{};
        float farPlane{};
        buildCameraMatrices(proj, view, eye, target, nearPlane, farPlane);
        bool inverseOk = false;
        const QMatrix4x4 inverseView = view.inverted(&inverseOk);
        if (!inverseOk)
        {
            return;
        }
        const QVector4D right = inverseView.column(0);
        const QVector4D up = inverseView.column(1);
        axisA = QVector3D(right.x(), right.y(), right.z());
        axisB = QVector3D(up.x(), up.y(), up.z());
    }
    else
    {
        switch (gridPlane_)
        {
            case GridPlane::XY:
                break;
            case GridPlane::XZ:
                axisB = QVector3D(0.0f, 0.0f, 1.0f);
                break;
            case GridPlane::YZ:
                axisA = QVector3D(0.0f, 1.0f, 0.0f);
                axisB = QVector3D(0.0f, 0.0f, 1.0f);
                break;
        }
    }

    const render::GridViewAngles angles =
        render::viewAnglesForGridAxes(axisA, axisB);
    if (!angles.valid)
    {
        return;
    }
    startViewAnimation(
        angles.yawDeg, angles.pitchDeg, angles.rollDeg, 180);
    viewAnim_.preserveGridOverrides = true;
    // JED's View to Grid centered the camera on the authored grid origin as
    // well as orienting it perpendicular to the plane. Orbit pan is relative
    // to center, so target the exact world-space origin without changing the
    // grid transform, orbit distance, projection, or orthographic scale.
    viewAnim_.targetPan = gridTransformOrigin() - camera_.orbit.center;
}

void GlView::snapGridToObject()
{
    if (!level_)
    {
        return;
    }

    // Grid alignment/centering is exposed as an enabled/disabled action in
    // both View > Grid and Map Settings.  These operations historically only
    // invalidated the renderer, leaving that action stale until another
    // unrelated viewport change.  Publish one state notification on every
    // completed operation, including the early-return surface/thing paths.
    [[maybe_unused]] const auto gridStateGuard = qScopeGuard(
        [this]()
        {
            Q_EMIT viewportStateChanged();
        });

    auto captureOverrideCameraBasis = [this]()
    {
        if (!gridResetOnCameraRotate_)
        {
            return;
        }

        QMatrix4x4 proj;
        QMatrix4x4 view;
        QVector3D eye{};
        QVector3D target{};
        float nearPlane{};
        float farPlane{};
        buildCameraMatrices(proj, view, eye, target, nearPlane, farPlane);

        QVector3D forward = target - eye;
        bool invOk = false;
        const QMatrix4x4 invView = view.inverted(&invOk);
        QVector3D up(0.0f, 0.0f, 1.0f);
        if (invOk)
        {
            const QVector4D u4 = invView.column(1);
            up = QVector3D(u4.x(), u4.y(), u4.z());
        }

        if (normalizeInPlace(forward) && normalizeInPlace(up))
        {
            gridOverrideCamBasisValid_ = true;
            gridOverrideCamForward_ = forward;
            gridOverrideCamUp_ = up;
        }
    };

    auto fetchSurfaceVertex = [this](const SurfaceRef ref, const std::int32_t localIndex, QVector3D& out) -> bool
    {
        if (ref.sector < 0 || ref.surface < 0 || localIndex < 0 || static_cast<std::size_t>(ref.sector) >= level_->sectors.size())
        {
            return false;
        }

        const Sector& sector = level_->sectors[static_cast<std::size_t>(ref.sector)];
        if (static_cast<std::size_t>(ref.surface) >= sector.surfaces.size())
        {
            return false;
        }

        const Surface& surface = sector.surfaces[static_cast<std::size_t>(ref.surface)];
        if (static_cast<std::size_t>(localIndex) >= surface.vertices.size())
        {
            return false;
        }

        const std::int32_t vi = surface.vertices[static_cast<std::size_t>(localIndex)];
        if (vi < 0 || static_cast<std::size_t>(vi) >= sector.vertices.size())
        {
            return false;
        }

        const Vertex& v = sector.vertices[static_cast<std::size_t>(vi)];
        out = QVector3D(static_cast<float>(v.x), static_cast<float>(v.y), static_cast<float>(v.z));
        return true;
    };

    auto applySurfaceBasis = [this, &captureOverrideCameraBasis](const QVector3D& origin, QVector3D axisA, QVector3D normal) -> bool
    {
        if (!normalizeInPlace(normal))
        {
            return false;
        }

        axisA -= normal * QVector3D::dotProduct(axisA, normal);
        if (!normalizeInPlace(axisA))
        {
            return false;
        }

        QVector3D axisB = QVector3D::crossProduct(normal, axisA);
        if (!normalizeInPlace(axisB))
        {
            return false;
        }

        gridCenterOverride_ = origin;
        gridAlignToView_ = true;
        gridAlignAxisA_ = axisA;
        gridAlignAxisB_ = axisB;
        captureOverrideCameraBasis();
        gridCacheValid_ = false;
        update();
        return true;
    };

    auto setCenterOnly = [this, &captureOverrideCameraBasis](const QVector3D& center)
    {
        gridCenterOverride_ = center;
        captureOverrideCameraBasis();
        gridCacheValid_ = false;
        update();
    };

    // Light edit submodes deliberately reuse the owner pick modes: Thing
    // lights pick as PickMode::Thing and Sector lights as PickMode::Sector.
    // Resolve them before the ordinary owner branches below, otherwise a
    // head light would snap to the Thing origin and a Sector point light to
    // the Sector's first vertex rather than to the visible emitter gizmo.
    if (lightGizmoEditingActive_ || pickMode_ == PickMode::Light)
    {
        if (lightGizmoCategory_ == render::LightGizmoCategory::Baked)
        {
            if (selectedLight_ >= 0
                && static_cast<std::size_t>(selectedLight_) < level_->lights.size())
            {
                const SedLight& light =
                    level_->lights[static_cast<std::size_t>(selectedLight_)];
                setCenterOnly(QVector3D(
                    static_cast<float>(light.position.x),
                    static_cast<float>(light.position.y),
                    static_cast<float>(light.position.z)));
            }
            return;
        }

        if (lightGizmoCategory_ == render::LightGizmoCategory::Thing)
        {
            std::int32_t owner = selectedThing_;
            if (owner < 0 && !highlightedSelection_.things.empty())
            {
                owner = *highlightedSelection_.things.begin();
            }
            const std::vector<render::LightGizmoEntry> entries =
                render::thingLightGizmoEntries(*level_, templates_);
            auto entry = std::find_if(
                entries.begin(), entries.end(), [this, owner](const render::LightGizmoEntry& candidate)
                {
                    return candidate.ownerIndex == owner
                        && candidate.kind == selectedLightGizmoKind_;
                });
            if (entry == entries.end())
            {
                entry = std::find_if(
                    entries.begin(), entries.end(), [owner](const render::LightGizmoEntry& candidate)
                    {
                        return candidate.ownerIndex == owner;
                    });
            }
            if (entry != entries.end())
            {
                setCenterOnly(entry->position);
            }
            return;
        }

        std::int32_t owner = selectedSector_;
        if (owner < 0 && !highlightedSelection_.sectors.empty())
        {
            owner = *highlightedSelection_.sectors.begin();
        }
        const std::vector<render::LightGizmoEntry> entries =
            render::sectorLightGizmoEntries(*level_);
        const auto entry = std::find_if(
            entries.begin(), entries.end(), [owner](const render::LightGizmoEntry& candidate)
            {
                return candidate.ownerIndex == owner;
            });
        if (entry != entries.end())
        {
            setCenterOnly(entry->position);
        }
        return;
    }

    auto activeSurfaceRef = [this]() -> std::optional<SurfaceRef>
    {
        // Selections from other edit modes are intentionally retained for a
        // quick return to that mode. They must not make Snap Grid to Item use
        // a stale face while Sector/Thing/Light mode is active.
        if (pickMode_ != PickMode::Surface)
        {
            return std::nullopt;
        }
        if (selectedSurfaceSector_ >= 0 && selectedSurface_ >= 0)
        {
            return SurfaceRef{ selectedSurfaceSector_, selectedSurface_ };
        }
        if (!highlightedSelection_.surfaces.empty())
        {
            return *highlightedSelection_.surfaces.begin();
        }
        if (!highlightedSelection_.surfaceVertices.empty())
        {
            return highlightedSelection_.surfaceVertices.begin()->surface;
        }
        return std::nullopt;
    }();

    if (activeSurfaceRef.has_value())
    {
        const SurfaceRef ref = *activeSurfaceRef;
        if (ref.sector >= 0 && ref.surface >= 0 && static_cast<std::size_t>(ref.sector) < level_->sectors.size())
        {
            const Sector& sector = level_->sectors[static_cast<std::size_t>(ref.sector)];
            if (static_cast<std::size_t>(ref.surface) < sector.surfaces.size())
            {
                const Surface& surface = sector.surfaces[static_cast<std::size_t>(ref.surface)];
                if (!surface.vertices.empty())
                {
                    QVector3D origin{};
                    QVector3D axisA{};
                    QVector3D normal(
                        static_cast<float>(surface.normal.x),
                        static_cast<float>(surface.normal.y),
                        static_cast<float>(surface.normal.z));

                    if (surfaceSubMode_ == SurfaceSubMode::Vertex)
                    {
                        std::vector<editor::SurfaceVertexRef> verts{};
                        for (const editor::SurfaceVertexRef& vref : highlightedSelection_.surfaceVertices)
                        {
                            if (vref.surface.sector == ref.sector && vref.surface.surface == ref.surface)
                            {
                                verts.push_back(vref);
                            }
                        }
                        std::sort(verts.begin(), verts.end(), [](const editor::SurfaceVertexRef& a, const editor::SurfaceVertexRef& b)
                            {
                                return a.localVertex < b.localVertex;
                            });
                        QVector3D activeVertex{};
                        bool haveActiveVertex = activeSurfaceVertexHintValid_
                            && activeSurfaceVertexHint_.surface == ref
                            && fetchSurfaceVertex(
                                ref,
                                activeSurfaceVertexHint_.localVertex,
                                activeVertex);
                        if (!haveActiveVertex && !verts.empty())
                        {
                            haveActiveVertex = fetchSurfaceVertex(
                                ref, verts.back().localVertex, activeVertex);
                        }
                        if (verts.size() >= 3)
                        {
                            QVector3D p0{};
                            QVector3D p1{};
                            QVector3D p2{};
                            if (fetchSurfaceVertex(ref, verts[0].localVertex, p0)
                                && fetchSurfaceVertex(ref, verts[1].localVertex, p1)
                                && fetchSurfaceVertex(ref, verts[2].localVertex, p2))
                            {
                                origin = haveActiveVertex ? activeVertex : p0;
                                axisA = p1 - p0;
                                normal = QVector3D::crossProduct(axisA, p2 - p1);
                                if (applySurfaceBasis(origin, axisA, normal))
                                {
                                    return;
                                }
                            }
                        }
                        if (haveActiveVertex)
                        {
                            // One/two selected vertices define a position but
                            // not a stable plane. Match legacy JED: center the
                            // grid on the active vertex without realignment.
                            setCenterOnly(activeVertex);
                            return;
                        }
                    }

                    if (surfaceSubMode_ == SurfaceSubMode::Edge && haveLastSurfaceHit_ && lastSurfaceHitSurface_ == ref && surface.vertices.size() >= 2)
                    {
                        double bestDist2 = std::numeric_limits<double>::infinity();
                        QVector3D bestA{};
                        QVector3D bestB{};

                        for (std::size_t i = 0; i < surface.vertices.size(); ++i)
                        {
                            QVector3D a{};
                            QVector3D b{};
                            if (!fetchSurfaceVertex(ref, static_cast<std::int32_t>(i), a)
                                || !fetchSurfaceVertex(ref, static_cast<std::int32_t>((i + 1) % surface.vertices.size()), b))
                            {
                                continue;
                            }

                            const QVector3D ab = b - a;
                            const float len2 = ab.lengthSquared();
                            if (len2 <= sed::math::kTinyEpsilonF)
                            {
                                continue;
                            }

                            const QVector3D hit(
                                static_cast<float>(lastSurfaceHitPos_.x),
                                static_cast<float>(lastSurfaceHitPos_.y),
                                static_cast<float>(lastSurfaceHitPos_.z));
                            const float t = std::clamp(QVector3D::dotProduct(hit - a, ab) / len2, 0.0f, 1.0f);
                            const QVector3D closest = a + ab * t;
                            const QVector3D d = hit - closest;
                            const double dist2 = static_cast<double>(d.lengthSquared());
                            if (dist2 < bestDist2)
                            {
                                bestDist2 = dist2;
                                bestA = a;
                                bestB = b;
                            }
                        }

                        if (bestDist2 < std::numeric_limits<double>::infinity())
                        {
                            origin = bestA;
                            axisA = bestB - bestA;
                            if (applySurfaceBasis(origin, axisA, normal))
                            {
                                return;
                            }
                        }
                    }

                    QVector3D p0{};
                    QVector3D p1{};
                    if (fetchSurfaceVertex(ref, 0, p0) && fetchSurfaceVertex(ref, 1, p1))
                    {
                        origin = p0;
                        axisA = p1 - p0;
                        if (applySurfaceBasis(origin, axisA, normal))
                        {
                            return;
                        }
                    }
                }
            }
        }
    }

    if (pickMode_ == PickMode::Sector
        && selectedSector_ >= 0
        && static_cast<std::size_t>(selectedSector_) < level_->sectors.size())
    {
        const Sector& sector = level_->sectors[static_cast<std::size_t>(selectedSector_)];
        if (!sector.vertices.empty())
        {
            const Vertex& v = sector.vertices.front();
            setCenterOnly(QVector3D(static_cast<float>(v.x), static_cast<float>(v.y), static_cast<float>(v.z)));
            return;
        }
    }

    if (pickMode_ == PickMode::Thing
        && selectedThing_ >= 0 && selectedFrameValueIndex_ >= 0
        && static_cast<std::size_t>(selectedThing_) < level_->things.size())
    {
        const Thing& thing = level_->things[static_cast<std::size_t>(selectedThing_)];
        if (static_cast<std::size_t>(selectedFrameValueIndex_) < thing.values.size())
        {
            const TplValue& value = thing.values[static_cast<std::size_t>(selectedFrameValueIndex_)];
            double x{};
            double y{};
            double z{};
            double pch{};
            double yaw{};
            double rol{};
            if (value.type == ValueType::frame && sed::text::iequalsAscii(value.name, "frame")
                && value.tryGetFrame(x, y, z, pch, yaw, rol))
            {
                setCenterOnly(QVector3D(static_cast<float>(x), static_cast<float>(y), static_cast<float>(z)));
                return;
            }
        }
    }

    if (pickMode_ == PickMode::Thing
        && selectedThing_ >= 0
        && static_cast<std::size_t>(selectedThing_) < level_->things.size())
    {
        const Thing& thing = level_->things[static_cast<std::size_t>(selectedThing_)];
        setCenterOnly(QVector3D(static_cast<float>(thing.x), static_cast<float>(thing.y), static_cast<float>(thing.z)));
        return;
    }

}

void GlView::orientGridToSelection()
{
    if (!level_)
    {
        return;
    }

    [[maybe_unused]] const auto gridStateGuard = qScopeGuard(
        [this]()
        {
            Q_EMIT viewportStateChanged();
        });

    const bool hadGridAlignment = gridAlignToView_;
    const QVector3D oldAxisA = gridAlignAxisA_;
    const QVector3D oldAxisB = gridAlignAxisB_;

    snapGridToObject();
    centerGridOnSelection();

    if (!gridAlignToView_ && hadGridAlignment)
    {
        gridAlignToView_ = true;
        gridAlignAxisA_ = oldAxisA;
        gridAlignAxisB_ = oldAxisB;
        gridCacheValid_ = false;
        update();
    }
}

void GlView::centerGridOnSelection()
{
    if (!level_)
    {
        return;
    }

    const ui::viewport::SelectionOrbitFallback fallback{
        selectedSector_,
        selectedSurfaceSector_,
        selectedSurface_,
        selectedThing_,
        selectedLight_,
        selectedFrameValueIndex_,
    };
    std::optional<QVector3D> center;
    bool resolved = true;
#ifdef SED_APP_MAIN_WINDOW_TEST_ACCESS
    resolved = !property("testOrbitPivotAllocationFailure").toBool();
#endif
    if (resolved)
    {
        resolved = ui::viewport::trySelectionOrbitPivot(*level_, highlightedSelection_, fallback, center);
    }
    if (!resolved || !center)
    {
        return;
    }

    [[maybe_unused]] const auto gridStateGuard = qScopeGuard(
        [this]()
        {
            Q_EMIT viewportStateChanged();
        });

    gridCenterOverride_ = *center;
    if (gridResetOnCameraRotate_)
    {
        QMatrix4x4 projection{};
        QMatrix4x4 view{};
        QVector3D eye{};
        QVector3D target{};
        float nearPlane{};
        float farPlane{};
        buildCameraMatrices(
            projection, view, eye, target, nearPlane, farPlane);

        QVector3D forward = target - eye;
        bool inverseOk = false;
        const QMatrix4x4 inverseView = view.inverted(&inverseOk);
        QVector3D up(0.0f, 0.0f, 1.0f);
        if (inverseOk)
        {
            const QVector4D upColumn = inverseView.column(1);
            up = QVector3D(upColumn.x(), upColumn.y(), upColumn.z());
        }

        if (normalizeInPlace(forward) && normalizeInPlace(up))
        {
            gridOverrideCamBasisValid_ = true;
            gridOverrideCamForward_ = forward;
            gridOverrideCamUp_ = up;
        }
    }
    gridCacheValid_ = false;
    update();
}

void GlView::clearGridCenterOverride()
{
    if (!gridCenterOverride_.has_value())
    {
        return;
    }

    [[maybe_unused]] const auto gridStateGuard = qScopeGuard(
        [this]()
        {
            Q_EMIT viewportStateChanged();
        });

    gridCenterOverride_.reset();
    gridCacheValid_ = false;
    update();
}

void GlView::resetGridOverrides()
{
    const bool changed = gridCenterOverride_.has_value()
        || gridAlignToView_
        || gridOverrideCamBasisValid_
        || gridPlane_ != GridPlane::XY
        || gridAlignAxisA_ != QVector3D(1.0f, 0.0f, 0.0f)
        || gridAlignAxisB_ != QVector3D(0.0f, 1.0f, 0.0f);
    if (!changed)
    {
        return;
    }

    [[maybe_unused]] const auto gridStateGuard = qScopeGuard(
        [this]()
        {
            Q_EMIT viewportStateChanged();
        });

    gridCenterOverride_.reset();
    gridAlignToView_ = false;
    gridPlane_ = GridPlane::XY;
    gridAlignAxisA_ = QVector3D(1.0f, 0.0f, 0.0f);
    gridAlignAxisB_ = QVector3D(0.0f, 1.0f, 0.0f);
    gridOverrideCamBasisValid_ = false;
    gridCacheValid_ = false;
    update();
}

QVector3D GlView::gridTransformOrigin() const noexcept
{
    return gridCenterOverride_.value_or(QVector3D(0.0f, 0.0f, 0.0f));
}

render::GridTransformAngles GlView::gridTransformAngles() const noexcept
{
    if (gridAlignToView_)
    {
        return render::gridEulerAnglesFromAxes(gridAlignAxisA_, gridAlignAxisB_);
    }

    switch (gridPlane_)
    {
        case GridPlane::XY:
            return {};
        case GridPlane::XZ:
            return render::gridEulerAnglesFromAxes(
                QVector3D(1.0f, 0.0f, 0.0f),
                QVector3D(0.0f, 0.0f, 1.0f));
        case GridPlane::YZ:
            return render::gridEulerAnglesFromAxes(
                QVector3D(0.0f, 1.0f, 0.0f),
                QVector3D(0.0f, 0.0f, 1.0f));
    }
    return {};
}

void GlView::captureGridOverrideCameraBasis()
{
    gridOverrideCamBasisValid_ = false;
    if (!gridResetOnCameraRotate_
        || (!gridAlignToView_ && !gridCenterOverride_.has_value()))
    {
        return;
    }

    QMatrix4x4 projection{};
    QMatrix4x4 view{};
    QVector3D eye{};
    QVector3D target{};
    float nearPlane{};
    float farPlane{};
    buildCameraMatrices(
        projection, view, eye, target, nearPlane, farPlane);

    QVector3D forward = target - eye;
    bool inverseOk = false;
    const QMatrix4x4 inverseView = view.inverted(&inverseOk);
    if (!inverseOk || !normalizeInPlace(forward))
    {
        return;
    }

    const QVector4D upColumn = inverseView.column(1);
    QVector3D up(upColumn.x(), upColumn.y(), upColumn.z());
    if (!normalizeInPlace(up))
    {
        return;
    }

    gridOverrideCamBasisValid_ = true;
    gridOverrideCamForward_ = forward;
    gridOverrideCamUp_ = up;
}

void GlView::setGridTransformOrigin(const QVector3D& origin)
{
    if (!std::isfinite(origin.x()) || !std::isfinite(origin.y())
        || !std::isfinite(origin.z()))
    {
        return;
    }

    if (!gridAlignToView_)
    {
        switch (gridPlane_)
        {
            case GridPlane::XY:
                gridAlignAxisA_ = QVector3D(1.0f, 0.0f, 0.0f);
                gridAlignAxisB_ = QVector3D(0.0f, 1.0f, 0.0f);
                break;
            case GridPlane::XZ:
                gridAlignAxisA_ = QVector3D(1.0f, 0.0f, 0.0f);
                gridAlignAxisB_ = QVector3D(0.0f, 0.0f, 1.0f);
                break;
            case GridPlane::YZ:
                gridAlignAxisA_ = QVector3D(0.0f, 1.0f, 0.0f);
                gridAlignAxisB_ = QVector3D(0.0f, 0.0f, 1.0f);
                break;
        }
    }
    gridCenterOverride_ = origin;
    gridAlignToView_ = true;
    captureGridOverrideCameraBasis();
    gridCacheValid_ = false;
    Q_EMIT viewportStateChanged();
    update();
}

void GlView::setGridTransformAngles(const render::GridTransformAngles& angles)
{
    QVector3D axisA{};
    QVector3D axisB{};
    QVector3D normal{};
    if (!render::gridAxesFromEulerAngles(angles, axisA, axisB, normal))
    {
        return;
    }

    gridAlignAxisA_ = axisA;
    gridAlignAxisB_ = axisB;
    gridAlignToView_ = true;
    captureGridOverrideCameraBasis();
    gridCacheValid_ = false;
    Q_EMIT viewportStateChanged();
    update();
}

void GlView::setGridTransformPlane(const GridPlane plane)
{
    QVector3D axisA{1.0f, 0.0f, 0.0f};
    QVector3D axisB{0.0f, 1.0f, 0.0f};
    switch (plane)
    {
        case GridPlane::XY:
            break;
        case GridPlane::XZ:
            axisB = QVector3D(0.0f, 0.0f, 1.0f);
            break;
        case GridPlane::YZ:
            axisA = QVector3D(0.0f, 1.0f, 0.0f);
            axisB = QVector3D(0.0f, 0.0f, 1.0f);
            break;
    }

    gridPlane_ = plane;
    gridAlignAxisA_ = axisA;
    gridAlignAxisB_ = axisB;
    gridAlignToView_ = true;
    captureGridOverrideCameraBasis();
    gridCacheValid_ = false;
    Q_EMIT viewportStateChanged();
    update();
}

void GlView::setGridMajorSpacing(const float spacing)
{
    const float clamped = std::clamp(spacing, 1.0e-6f, 1.0e9f);
    if (gridMajorSpacing_ == clamped)
    {
        return;
    }

    gridMajorSpacing_ = clamped;
    if (gridMinorSpacing_ > gridMajorSpacing_)
    {
        gridMinorSpacing_ = gridMajorSpacing_;
    }
    gridCacheValid_ = false;
    update();
}

void GlView::setGridMinorSpacing(const float spacing)
{
    const float clamped = std::clamp(spacing, 1.0e-6f, 1.0e9f);
    const float next = std::min(clamped, gridMajorSpacing_);
    if (gridMinorSpacing_ == next)
    {
        return;
    }

    gridMinorSpacing_ = next;
    gridCacheValid_ = false;
    update();
}

void GlView::setGridAdaptiveSpacing(const bool enabled)
{
    if (gridAdaptiveSpacing_ == enabled)
    {
        return;
    }

    gridAdaptiveSpacing_ = enabled;
    gridCacheValid_ = false;
    update();
}

void GlView::setGridOpacity(const float opacity)
{
    const float next = std::clamp(std::isfinite(opacity) ? opacity : 0.70f, 0.0f, 1.0f);
    if (std::abs(gridOpacity_ - next) <= sed::math::kTinyEpsilonF)
    {
        return;
    }

    gridOpacity_ = next;
    update();
}

void GlView::setGridColor(const QColor& color)
{
    QColor next = color;
    if (!next.isValid())
    {
        next = QColor::fromRgb(112, 128, 156);
    }
    next.setAlpha(255);
    if (gridColor_ == next)
    {
        return;
    }

    gridColor_ = next;
    update();
}

void GlView::setGridMajorColor(const QColor& color)
{
    QColor next = color;
    if (!next.isValid())
    {
        next = QColor::fromRgb(170, 184, 208);
    }
    next.setAlpha(255);
    if (gridMajorColor_ == next)
    {
        return;
    }

    gridMajorColor_ = next;
    update();
}

void GlView::setGridAxisXColor(const QColor& color)
{
    QColor next = color.isValid() ? color : QColor(210, 75, 70);
    next.setAlpha(255);
    if (gridAxisXColor_ == next)
    {
        return;
    }
    gridAxisXColor_ = next;
    update();
}

void GlView::setGridAxisYColor(const QColor& color)
{
    QColor next = color.isValid() ? color : QColor(95, 190, 95);
    next.setAlpha(255);
    if (gridAxisYColor_ == next)
    {
        return;
    }
    gridAxisYColor_ = next;
    update();
}

void GlView::setGridAxisZColor(const QColor& color)
{
    QColor next = color.isValid() ? color : QColor(90, 140, 215);
    next.setAlpha(255);
    if (gridAxisZColor_ == next)
    {
        return;
    }
    gridAxisZColor_ = next;
    update();
}

void GlView::setGridLineWidthPx(const float widthPx)
{
    const float next = render::clampedGridLineWidthPx(widthPx);
    if (std::abs(gridLineWidthPx_ - next) <= sed::math::kTinyEpsilonF)
    {
        return;
    }

    gridLineWidthPx_ = next;
    update();
}

void GlView::setGridMinorDots(const bool enabled)
{
    if ( gridMinorDots_ == enabled )
    {
        return;
    }

    gridMinorDots_ = enabled;
    gridCacheValid_ = false;
    update();
}

void GlView::setGridMinorDotSizePx(const float sizePx)
{
    const float next = render::clampedGridMinorDotSizePx(sizePx);
    if ( std::abs(gridMinorDotSizePx_ - next) <= sed::math::kTinyEpsilonF )
    {
        return;
    }

    gridMinorDotSizePx_ = next;
    update();
}

void GlView::setGridVisibleThroughWalls(const bool enabled)
{
    if (gridVisibleThroughWalls_ == enabled && (!enabled || !gridAlwaysBehind_))
    {
        return;
    }

    gridVisibleThroughWalls_ = enabled;
    if (enabled)
    {
        gridAlwaysBehind_ = false;
    }
    update();
}

void GlView::setGridAlwaysBehind(const bool enabled)
{
    if (gridAlwaysBehind_ == enabled && (!enabled || !gridVisibleThroughWalls_))
    {
        return;
    }

    gridAlwaysBehind_ = enabled;
    if (enabled)
    {
        gridVisibleThroughWalls_ = false;
    }
    update();
}

void GlView::setGridResetOnCameraRotate(const bool enabled)
{
    if (gridResetOnCameraRotate_ == enabled)
    {
        return;
    }

    gridResetOnCameraRotate_ = enabled;
    if (!gridResetOnCameraRotate_)
    {
        gridOverrideCamBasisValid_ = false;
        return;
    }

    // Capture the current camera basis if an override is already active.
    if ((gridAlignToView_ || gridCenterOverride_.has_value()) && !gridOverrideCamBasisValid_)
    {
        QMatrix4x4 proj;
        QMatrix4x4 view;
        QVector3D eye{};
        QVector3D target{};
        float nearPlane{};
        float farPlane{};
        buildCameraMatrices(proj, view, eye, target, nearPlane, farPlane);

        QVector3D forward = target - eye;
        bool invOk = false;
        const QMatrix4x4 invView = view.inverted(&invOk);
        QVector3D up(0.0f, 0.0f, 1.0f);
        if (invOk)
        {
            const QVector4D u4 = invView.column(1);
            up = QVector3D(u4.x(), u4.y(), u4.z());
        }

        if (normalizeInPlace(forward) && normalizeInPlace(up))
        {
            gridOverrideCamBasisValid_ = true;
            gridOverrideCamForward_ = forward;
            gridOverrideCamUp_ = up;
        }
    }
}

void GlView::setSectorCleaveToolEnabled(const bool enabled)
{
    if (sectorCleaveToolEnabled_ == enabled)
    {
        return;
    }

    if ( !enabled )
    {
        suspendStagedCleaveInteraction();
    }
    else
    {
        selectStagedCleaveWorkspace(CleaveWorkspaceFamily::Sector);
        surfaceCleaveToolEnabled_ = false;
    }
    sectorCleaveToolEnabled_ = enabled;
    sectorCleaveHaveFirstPoint_ = false;
    sectorCleaveFirstPoint_ = {};
    sectorCleaveCurrentPoint_ = {};
    cleaveSnapMarker_.reset();
    if (!enabled)
    {
        cleaveSnapGuidesVisible_ = false;
    }
    else
    {
        updateCleaveSnapGuideVisibility(QGuiApplication::keyboardModifiers());
    }
    editingStagedCleaveLine_ = -1;
    editingStagedCleaveEndpoint_ = -1;
    editingDisjointStagedCleaveEndpoint_ = false;
    editingStagedCleaveEndpointPreservePlacement_ = false;
    stagedCleaveEndpointOriginClickCandidate_ = false;
    update();
    Q_EMIT viewportStateChanged();
}

void GlView::setCleaveStagedMode(const bool enabled)
{
    if (cleaveStagedMode_ == enabled)
    {
        return;
    }

    if ( !enabled )
    {
        suspendStagedCleaveInteraction();
    }
    cleaveStagedMode_ = enabled;
    sectorCleaveHaveFirstPoint_ = false;
    surfaceCleaveHaveFirstPoint_ = false;
    cleaveSnapMarker_.reset();
    editingStagedCleaveLine_ = -1;
    editingStagedCleaveEndpoint_ = -1;
    editingDisjointStagedCleaveEndpoint_ = false;
    editingStagedCleaveEndpointPreservePlacement_ = false;
    stagedCleaveEndpointOriginClickCandidate_ = false;
    update();
    Q_EMIT viewportStateChanged();
}

void GlView::setCleaveChainEnabled(const bool enabled)
{
    if (cleaveChainEnabled_ == enabled)
    {
        return;
    }
    cleaveChainEnabled_ = enabled;
    update();
    Q_EMIT viewportStateChanged();
}

void GlView::setCleaveBounded(const bool enabled)
{
    if ( cleaveBounded_ == enabled )
    {
        return;
    }
    cleaveBounded_ = enabled;
    if ( !stagedCleaveLines_.empty() )
    {
        noteStagedCleaveAuthoredChange();
    }
    update();
    Q_EMIT viewportStateChanged();
}

void GlView::setCleaveMeasurementsVisible(const bool enabled)
{
    // Retain the old API for callers restoring pre-guide state, but make the
    // independent Lengths toggle authoritative from this point onward.
    cleaveMeasurementsVisible_ = true;
    if (!enabled)
    {
        setCleaveLengthsVisible(false);
    }
}

void GlView::setCleaveMeasureUnit(const CleaveMeasureUnit unit)
{
    CleaveMeasureUnit effectiveUnit = unit;
    switch (unit)
    {
        case CleaveMeasureUnit::Bare:
        case CleaveMeasureUnit::Meters:
        case CleaveMeasureUnit::JKU:
        case CleaveMeasureUnit::IJU:
            break;
        case CleaveMeasureUnit::None:
        default:
            effectiveUnit = CleaveMeasureUnit::Meters;
            setCleaveLengthsVisible(false);
            break;
    }
    effectiveUnit = normalizedCleaveMeasureUnit(effectiveUnit, level_);
    if (cleaveMeasureUnit_ == effectiveUnit)
    {
        return;
    }
    cleaveMeasureUnit_ = effectiveUnit;
    update();
    Q_EMIT viewportStateChanged();
}

void GlView::setCleaveAnglesVisible(const bool enabled)
{
    if (cleaveAnglesVisible_ == enabled)
    {
        return;
    }
    cleaveAnglesVisible_ = enabled;
    update();
    Q_EMIT viewportStateChanged();
}

void GlView::setCleaveLengthsVisible(const bool enabled)
{
    if (cleaveLengthsVisible_ == enabled)
    {
        return;
    }
    cleaveLengthsVisible_ = enabled;
    update();
    Q_EMIT viewportStateChanged();
}

void GlView::setCleaveLineNumbersVisible(const bool enabled)
{
    if (cleaveLineNumbersVisible_ == enabled)
    {
        return;
    }
    cleaveLineNumbersVisible_ = enabled;
    update();
    Q_EMIT viewportStateChanged();
}

void GlView::rebuildStagedCleaveRankCache()
{
    stagedCleaveRankByLine_.assign(stagedCleaveLines_.size(), -1);
    if ( stagedCleaveApplyOrder_.size() == stagedCleaveLines_.size() )
    {
        for ( std::size_t rank = 0; rank < stagedCleaveApplyOrder_.size(); ++rank )
        {
            const std::int32_t line = stagedCleaveApplyOrder_[rank];
            if ( line >= 0 && static_cast<std::size_t>(line) < stagedCleaveRankByLine_.size() )
            {
                stagedCleaveRankByLine_[static_cast<std::size_t>(line)] =
                    static_cast<std::int32_t>(rank);
            }
        }
    }
    stagedCleaveRankCacheValid_ = true;
}

QRect GlView::stagedCleaveOrderLabelRect(
    const std::int32_t lineIndex,
    const QMatrix4x4& mvp) const
{
    auto* const mutableThis = const_cast<GlView*>(this);
    if ( !mutableThis->stagedCleaveRankCacheValid_ )
    {
        mutableThis->rebuildStagedCleaveRankCache();
    }
    if ( !cleaveLineNumbersVisible_ || !cleaveStagedMode_
        || lineIndex < 0
        || static_cast<std::size_t>(lineIndex) >= stagedCleaveLines_.size()
        || static_cast<std::size_t>(lineIndex) >= stagedCleaveRankByLine_.size()
        || stagedCleaveRankByLine_[static_cast<std::size_t>(lineIndex)] < 0 )
    {
        return {};
    }

    const StagedCleaveLine& line = stagedCleaveLines_[
        static_cast<std::size_t>(lineIndex)];
    return stagedCleaveOrderLabelRectForSegment(
        line.first,
        line.second,
        stagedCleaveRankByLine_[static_cast<std::size_t>(lineIndex)],
        mvp);
}

QRect GlView::stagedCleaveOrderLabelRectForSegment(
    const TVector& firstPoint,
    const TVector& secondPoint,
    const std::int32_t rank,
    const QMatrix4x4& mvp) const
{
    if ( !cleaveLineNumbersVisible_ || !cleaveStagedMode_ || rank < 0 )
    {
        return {};
    }
    const auto projected = render::frame_path::projectWorldSegment(
        mvp,
        firstPoint,
        secondPoint,
        width(),
        height());
    if ( !projected )
    {
        return {};
    }

    const QPointF first = projected->first;
    const QPointF second = projected->second;
    QPointF direction = second - first;
    const double length = std::hypot(direction.x(), direction.y());
    if ( length <= 1.0e-6 )
    {
        return {};
    }
    direction /= length;
    const QPointF normal(-direction.y(), direction.x());
    const QPointF center = (first + second) * 0.5
        + normal * (cleaveLengthsVisible_ ? 26.0 : 0.0);
    const QString text = render::cleaveLineNumberText(
        static_cast<std::size_t>(rank));
    QRectF rect(QFontMetrics(font()).boundingRect(text));
    rect.adjust(-5.0, -2.0, 5.0, 2.0);
    rect.moveCenter(center);
    return rect.toAlignedRect();
}

bool GlView::beginStagedCleaveOrderEdit(
    const std::int32_t line,
    const QRect& rect)
{
    if ( line < 0 || static_cast<std::size_t>(line) >= stagedCleaveLines_.size()
        || rect.isEmpty() || !cleaveLineNumbersVisible_ || !cleaveStagedMode_ )
    {
        return false;
    }
    (void)finishStagedCleaveWheelSession();
    if ( stagedCleaveOrderEditor_ )
    {
        finishStagedCleaveOrderEdit(true, false);
    }
    if ( !stagedCleaveRankCacheValid_ )
    {
        rebuildStagedCleaveRankCache();
    }
    const std::int32_t rank = stagedCleaveRankByLine_[static_cast<std::size_t>(line)];
    if ( rank < 0 )
    {
        return false;
    }

    constexpr std::size_t kMaximum = std::numeric_limits<std::size_t>::max();
    const std::size_t storageBeforeCopy = stagedCleaveHistoryStorageBytes();
    const std::size_t baselineBytes = stagedCleaveApplyOrder_.size()
            > kMaximum / sizeof(std::int32_t)
        ? kMaximum
        : stagedCleaveApplyOrder_.size() * sizeof(std::int32_t);
    if ( storageBeforeCopy > kStagedCleaveHistoryMaxBytes
        || baselineBytes > kStagedCleaveHistoryMaxBytes - storageBeforeCopy )
    {
        showToast(
            "Multi-cut: local history limit reached; Clear or Apply before editing Apply order",
            3200,
            ToastPlacement::BottomCenter,
            ToastGroup::Error);
        return false;
    }

    std::vector<std::int32_t> editorBaseline{};
    try
    {
        editorBaseline = stagedCleaveApplyOrder_;
    }
    catch (const std::bad_alloc&)
    {
        showToast(
            "Multi-cut: local history could not keep the Apply-order baseline",
            3200,
            ToastPlacement::BottomCenter,
            ToastGroup::Error);
        return false;
    }
    const std::size_t actualBaselineBytes = editorBaseline.capacity()
            > kMaximum / sizeof(std::int32_t)
        ? kMaximum
        : editorBaseline.capacity() * sizeof(std::int32_t);
    if ( actualBaselineBytes > kStagedCleaveHistoryMaxBytes - storageBeforeCopy )
    {
        showToast(
            "Multi-cut: local history limit reached; Clear or Apply before editing Apply order",
            3200,
            ToastPlacement::BottomCenter,
            ToastGroup::Error);
        return false;
    }

    QLineEdit* editor = nullptr;
    try
    {
        editor = new QLineEdit(this);
    }
    catch (const std::bad_alloc&)
    {
        showToast(
            "Multi-cut: the Apply-order editor could not be opened",
            3200,
            ToastPlacement::BottomCenter,
            ToastGroup::Error);
        return false;
    }
    editor->setObjectName(QStringLiteral("cleaveLineOrderEditor"));
    editor->setAlignment(Qt::AlignCenter);
    editor->setGeometry(rect.adjusted(-3, -2, 3, 2));
    editor->setText(QString::number(rank + 1));
    editor->selectAll();
    stagedCleaveOrderEditor_ = editor;
    stagedCleaveOrderEditorLine_ = line;
    stagedCleaveOrderEditorBaseline_ = std::move(editorBaseline);
    editor->installEventFilter(this);
    QObject::connect(
        editor,
        &QLineEdit::editingFinished,
        this,
        [this, guarded = QPointer<QLineEdit>(editor)]()
        {
            if ( guarded && stagedCleaveOrderEditor_ == guarded )
            {
                finishStagedCleaveOrderEdit(true, false);
            }
        });
    // The label click is complete when it opens its child editor. End its
    // pointer ownership before the synchronous focus handoff so FocusOut does
    // not cancel the new editor as an unfinished viewport drag.
    if (viewportMouseGesture_.action == ViewportMouseAction::pick)
    {
        viewportMouseGesture_ = {};
    }
    editor->show();
    editor->setFocus(Qt::MouseFocusReason);
    update();
    return true;
}

void GlView::finishStagedCleaveOrderEdit(
    const bool commit,
    const bool returnFocus)
{
    QLineEdit* const editor = stagedCleaveOrderEditor_.data();
    if ( !editor )
    {
        stagedCleaveOrderEditorLine_ = -1;
        stagedCleaveOrderEditorBaseline_.clear();
        return;
    }

    const std::int32_t editedLine = stagedCleaveOrderEditorLine_;
    bool valid = commit && editedLine >= 0
        && static_cast<std::size_t>(editedLine) < stagedCleaveLines_.size()
        && !stagedCleaveOrderEditorBaseline_.empty();
    QString text = editor->text();
    bool negative = false;
    std::size_t position = 0;
    if ( valid && !text.isEmpty() && (text.front() == QChar('+') || text.front() == QChar('-')) )
    {
        negative = text.front() == QChar('-');
        position = 1;
    }
    if ( valid && position == text.size() )
    {
        valid = false;
    }
    std::uint64_t magnitude = 0;
    const std::uint64_t cap = static_cast<std::uint64_t>(
        std::max<std::size_t>(1u, stagedCleaveLines_.size()));
    for ( ; valid && position < static_cast<std::size_t>(text.size()); ++position )
    {
        const QChar character = text.at(static_cast<qsizetype>(position));
        if ( character < QChar('0') || character > QChar('9') )
        {
            valid = false;
            break;
        }
        const std::uint64_t digit = static_cast<std::uint64_t>(character.unicode() - '0');
        magnitude = magnitude > (cap - std::min(cap, digit)) / 10u
            ? cap
            : std::min(cap, magnitude * 10u + digit);
    }

    bool changed = false;
    if ( valid )
    {
        const std::int32_t oldRank = stagedCleaveRankByLine_.size()
                > static_cast<std::size_t>(editedLine)
            ? stagedCleaveRankByLine_[static_cast<std::size_t>(editedLine)]
            : -1;
        const std::int64_t signedRank = negative
            ? -static_cast<std::int64_t>(magnitude)
            : static_cast<std::int64_t>(magnitude);
        const std::size_t newRank = static_cast<std::size_t>(std::clamp<std::int64_t>(
            signedRank,
            1,
            static_cast<std::int64_t>(stagedCleaveLines_.size())) - 1);
        if ( oldRank >= 0 && static_cast<std::size_t>(oldRank) != newRank )
        {
            stagedCleaveOrderEditorBaseline_.erase(
                std::remove(
                    stagedCleaveOrderEditorBaseline_.begin(),
                    stagedCleaveOrderEditorBaseline_.end(),
                    editedLine),
                stagedCleaveOrderEditorBaseline_.end());
            stagedCleaveOrderEditorBaseline_.insert(
                stagedCleaveOrderEditorBaseline_.begin()
                    + static_cast<std::ptrdiff_t>(newRank),
                editedLine);
            if ( stagedCleaveOrderEditorBaseline_ != stagedCleaveApplyOrder_
                && beginStagedCleaveHistoryGesture() )
            {
                stagedCleaveApplyOrder_ =
                    std::move(stagedCleaveOrderEditorBaseline_);
                stagedCleaveRankCacheValid_ = false;
                changed = finishStagedCleaveHistoryGesture();
            }
        }
    }

    stagedCleaveOrderEditor_.clear();
    stagedCleaveOrderEditorLine_ = -1;
    stagedCleaveOrderEditorBaseline_.clear();
    editor->removeEventFilter(this);
    editor->hide();
    editor->deleteLater();
    if ( returnFocus )
    {
        setFocus(Qt::OtherFocusReason);
    }
    if ( changed )
    {
        Q_EMIT viewportStateChanged();
    }
    update();
}

void GlView::setCleaveAngleStepDegrees(const double stepDegrees)
{
    const double next = std::isfinite(stepDegrees)
        ? std::clamp(stepDegrees, 0.0, 360.0)
        : 0.0;
    if (cleaveAngleStepDegrees_ == next)
    {
        return;
    }
    (void)finishStagedCleaveWheelSession();
    cleaveAngleStepDegrees_ = next;
    update();
    Q_EMIT viewportStateChanged();
}

GlView::StagedCleaveTransformMode GlView::stagedCleaveTransformMode() const noexcept
{
    return stagedCleaveTransformMode_;
}

QString GlView::stagedCleaveTransformHudText() const
{
    return render::cleaveTransformHudDisplayText(stagedCleaveTransformHudText_);
}

void GlView::clearStagedCleaveTransformHud() noexcept
{
    if ( stagedCleaveTransformHudHideTimer_ )
    {
        stagedCleaveTransformHudHideTimer_->stop();
    }
    if ( stagedCleaveTransformHudPresentationTimer_ )
    {
        stagedCleaveTransformHudPresentationTimer_->stop();
    }
    stagedCleaveTransformHudPresentation_.clear();
    stagedCleaveTransformHudText_.clear();
    stagedCleaveTransformHudAnchor_.reset();
    stagedCleaveTransformHudRect_ = {};
}

void GlView::startStagedCleaveTransformHudExpiry()
{
    if ( !stagedCleaveTransformHudText_.isEmpty() )
    {
        const int durationMs = viewportBannerDurationMs(
            viewportBannerSettings_, stagedCleaveTransformHudBannerKind_, 400);
        const auto now = render::cleave::CleaveHudPresentation::Clock::now();
        stagedCleaveTransformHudPresentation_.sample(now, durationMs);
        stagedCleaveTransformHudHideTimer_->start(durationMs);
        scheduleStagedCleaveTransformHudPresentationWake();
    }
}

void GlView::scheduleStagedCleaveTransformHudPresentationWake()
{
    if ( !stagedCleaveTransformHudPresentationTimer_
        || stagedCleaveTransformHudText_.isEmpty() )
    {
        return;
    }
    const int delayMs = stagedCleaveTransformHudPresentation_.nextWakeDelayMs(
        render::cleave::CleaveHudPresentation::Clock::now());
    if ( delayMs <= 0 )
    {
        stagedCleaveTransformHudPresentationTimer_->stop();
        return;
    }
    stagedCleaveTransformHudPresentationTimer_->start(delayMs);
}

void GlView::setStagedCleaveTransformMode(const StagedCleaveTransformMode mode)
{
    if (mode != StagedCleaveTransformMode::Move
        && mode != StagedCleaveTransformMode::Rotate
        && mode != StagedCleaveTransformMode::Scale)
    {
        return;
    }
    const StagedCleaveTransformMode effectiveMode = mode != StagedCleaveTransformMode::Move
            && !canTransformSelectedStagedCleave(mode)
        ? StagedCleaveTransformMode::Move
        : mode;
    if (stagedCleaveTransformMode_ == effectiveMode)
    {
        return;
    }
    suspendStagedCleaveInteraction();
    stagedCleaveTransformMode_ = effectiveMode;
    clearStagedCleaveTransformHud();
    Q_EMIT viewportStateChanged();
    update();
}

void GlView::normalizeStagedCleaveTransformMode() noexcept
{
    if ( stagedCleaveTransformMode_ != StagedCleaveTransformMode::Move
        && !canTransformSelectedStagedCleave(stagedCleaveTransformMode_) )
    {
        stagedCleaveTransformMode_ = StagedCleaveTransformMode::Move;
        clearStagedCleaveTransformHud();
    }
}

bool GlView::canTransformSelectedStagedCleave(
    const StagedCleaveTransformMode mode) const noexcept
{
    if (mode != StagedCleaveTransformMode::Move
        && mode != StagedCleaveTransformMode::Rotate
        && mode != StagedCleaveTransformMode::Scale)
    {
        return false;
    }
    if (selectedStagedCleaveLines_.empty())
    {
        return false;
    }
    const bool validSelection = std::all_of(
        selectedStagedCleaveLines_.begin(),
        selectedStagedCleaveLines_.end(),
        [this](const std::int32_t line)
        {
            return line >= 0
                && static_cast<std::size_t>(line) < stagedCleaveLines_.size();
        });
    if ( !validSelection || mode == StagedCleaveTransformMode::Move )
    {
        return validSelection;
    }

    TVector commonNormal = stagedCleaveLines_[static_cast<std::size_t>(
        selectedStagedCleaveLines_.front())].constructionPlaneNormal;
    if ( !render::cleaveNormalize(commonNormal) )
    {
        return false;
    }
    const TVector planeOrigin = stagedCleaveLines_[static_cast<std::size_t>(
        selectedStagedCleaveLines_.front())].first;
    for ( const std::int32_t lineIndex : selectedStagedCleaveLines_ )
    {
        const StagedCleaveLine& line = stagedCleaveLines_[
            static_cast<std::size_t>(lineIndex)];
        TVector lineNormal = line.constructionPlaneNormal;
        if ( !render::cleaveFinitePoint(line.first)
            || !render::cleaveFinitePoint(line.second)
            || render::cleaveSameHandle(line.first, line.second)
            || !render::cleaveNormalize(lineNormal)
            || std::abs(render::cleaveDot(commonNormal, lineNormal)) < 0.9999 )
        {
            return false;
        }
        const double firstOffset = std::abs(render::cleaveDot(
            line.first - planeOrigin,
            commonNormal));
        const double secondOffset = std::abs(render::cleaveDot(
            line.second - planeOrigin,
            commonNormal));
        const double scale = std::max({
            1.0,
            std::sqrt(std::max(0.0, render::cleaveDot(
                line.first - planeOrigin,
                line.first - planeOrigin))),
            std::sqrt(std::max(0.0, render::cleaveDot(
                line.second - planeOrigin,
                line.second - planeOrigin)))});
        if ( firstOffset > 1.0e-8 * scale
            || secondOffset > 1.0e-8 * scale )
        {
            return false;
        }
    }
    return true;
}

void GlView::setCleaveShapeOptions(
    const RecoveryState::CleaveShapeOptions& options)
{
    using ui::viewport::CleaveShapeKind;
    if (options.kind != CleaveShapeKind::Square
        && options.kind != CleaveShapeKind::Rectangle
        && options.kind != CleaveShapeKind::RegularPolygon
        && options.kind != CleaveShapeKind::Arch
        && options.kind != CleaveShapeKind::Star)
    {
        return;
    }
    RecoveryState::CleaveShapeOptions next = options;
    next.width = std::clamp(
        std::isfinite(options.width) ? options.width : 0.1,
        0.005,
        10000.0);
    next.height = std::clamp(
        std::isfinite(options.height) ? options.height : 0.1,
        0.005,
        10000.0);
    next.segments = std::clamp(options.segments, 3, 64);
    next.points = std::clamp(options.points, 3, 64);
    next.rotationDegrees = std::isfinite(options.rotationDegrees)
        ? std::fmod(options.rotationDegrees, 360.0) : 0.0;
    if ( next.rotationDegrees >= 180.0 ) next.rotationDegrees -= 360.0;
    if ( next.rotationDegrees < -180.0 ) next.rotationDegrees += 360.0;
    if ( next.rotationDegrees == 0.0 ) next.rotationDegrees = 0.0;
    if ( next.kind == CleaveShapeKind::Square )
    {
        next.kind = CleaveShapeKind::Rectangle;
        next.dimensionsLinked = true;
    }
    if ( next.kind == CleaveShapeKind::RegularPolygon )
    {
        next.dimensionsLinked = true;
    }
    if ( next.dimensionsLinked )
    {
        next.height = next.width;
    }
    const bool shapeParametersChanged = cleaveShapeOptions_.kind != next.kind
        || cleaveShapeOptions_.width != next.width
        || cleaveShapeOptions_.height != next.height
        || cleaveShapeOptions_.dimensionsLinked != next.dimensionsLinked
        || cleaveShapeOptions_.segments != next.segments
        || cleaveShapeOptions_.points != next.points;
    const bool placementFrameChanged = shapeParametersChanged
        || cleaveShapeOptions_.cameraPlane != next.cameraPlane
        || cleaveShapeOptions_.rotationDegrees != next.rotationDegrees;
    if (!stagedCleaveWheelShapeOptionsApplying_
        && stagedCleaveWheelSession_.active()
        && (cleaveShapeOptions_.placementArmed != next.placementArmed
            || (stagedCleaveWheelSession_.transformsShape() && placementFrameChanged)))
    {
        (void)finishStagedCleaveWheelSession();
    }
    if ( shapeParametersChanged || !next.placementArmed )
    {
        stagedCleaveShapePlacementPlan_.reset();
    }
    if ( placementFrameChanged || !next.placementArmed )
    {
        stagedCleaveShapePreviewCenter_.reset();
        stagedCleaveShapePreviewFrame_.reset();
        stagedCleaveShapePlacementClickPending_ = false;
    }
    if (cleaveShapeOptions_.kind == next.kind
        && cleaveShapeOptions_.width == next.width
        && cleaveShapeOptions_.height == next.height
        && cleaveShapeOptions_.dimensionsLinked == next.dimensionsLinked
        && cleaveShapeOptions_.segments == next.segments
        && cleaveShapeOptions_.points == next.points
        && cleaveShapeOptions_.cameraPlane == next.cameraPlane
        && cleaveShapeOptions_.rotationDegrees == next.rotationDegrees
        && cleaveShapeOptions_.placementArmed == next.placementArmed)
    {
        return;
    }
    cleaveShapeOptions_ = next;
    Q_EMIT viewportStateChanged();
    update();
}

bool GlView::setStagedCleaveShapePlan(
    const ui::viewport::CleaveShapePlan& plan)
{
    if ( !cleaveShapeOptions_.placementArmed
        || plan.perimeter.size() < 3u
        || plan.perimeter.size() > 128u
        || plan.decompositionStrokes.size() > 128u
        || plan.decompositionStrokes.size() + plan.perimeter.size() > 256u )
    {
        return false;
    }
    const auto finitePoint = [](const ui::viewport::CleaveShapePoint2D& point)
    {
        return std::isfinite(point.x) && std::isfinite(point.y);
    };
    if ( !std::all_of(
            plan.perimeter.begin(), plan.perimeter.end(), finitePoint)
        || !std::all_of(
            plan.decompositionStrokes.begin(),
            plan.decompositionStrokes.end(),
            [&finitePoint](const ui::viewport::CleaveShapeStroke2D& stroke)
            {
                return finitePoint(stroke.first) && finitePoint(stroke.second);
            }) )
    {
        return false;
    }
    stagedCleaveShapePlacementPlan_ = plan;
    stagedCleaveShapePreviewCenter_.reset();
    stagedCleaveShapePreviewFrame_.reset();
    update();
    return true;
}

std::uint64_t GlView::stagedCleaveAuthoredRevision() const noexcept
{
    return stagedCleaveAuthoredRevision_;
}


std::uint64_t GlView::stagedCleaveAuthoredRevision(
    const CleaveWorkspaceFamily family) const noexcept
{
    const std::size_t index = static_cast<std::size_t>(family);
    return index < stagedCleaveFamilyAuthoredRevisions_.size()
        ? stagedCleaveFamilyAuthoredRevisions_[index] : 0u;
}

GlView::RecoveryState::CleaveWorkspace GlView::defaultStagedCleaveWorkspaceOptions(
    const Level* const level)
{
    const QSettings settings = settingsStore();
    RecoveryState::CleaveWorkspace workspace{};
    workspace.stagedMode = settings.value(QStringLiteral("cleave/multi_cut_mode"),
        settings.value(QStringLiteral("cleave/staged_mode"), false)).toBool();
    workspace.chainEnabled = settings.value(QStringLiteral("cleave/chain"), false).toBool();
    workspace.bounded = settings.value(QStringLiteral("cleave/bounded"), true).toBool();
    const bool legacyMeasurements = settings.value(
        QStringLiteral("cleave/show_measurements"), true).toBool();
    workspace.anglesVisible = settings.value(
        QStringLiteral("cleave/show_angles"), legacyMeasurements).toBool();
    workspace.lengthsVisible = settings.value(
        QStringLiteral("cleave/show_lengths"), legacyMeasurements).toBool();
    workspace.lineNumbersVisible = settings.value(
        QStringLiteral("cleave/show_line_numbers"), false).toBool();
    bool unitOk = false;
    const int storedUnit = settings.value(QStringLiteral("cleave/measure_unit"),
        static_cast<int>(CleaveMeasureUnit::Meters)).toInt(&unitOk);
    const bool legacyNone = unitOk && storedUnit == static_cast<int>(CleaveMeasureUnit::None);
    const bool validUnit = unitOk && storedUnit >= static_cast<int>(CleaveMeasureUnit::Bare)
        && storedUnit < static_cast<int>(CleaveMeasureUnit::None);
    workspace.measureUnit = normalizedCleaveMeasureUnit(validUnit
        ? static_cast<CleaveMeasureUnit>(storedUnit) : CleaveMeasureUnit::Meters, level);
    workspace.lengthsVisible = workspace.lengthsVisible && legacyMeasurements && !legacyNone;
    bool stepOk = false;
    const double step = settings.value(
        QStringLiteral("cleave/angle_step_degrees"), 0.0).toDouble(&stepOk);
    workspace.angleStepDegrees = stepOk && std::isfinite(step)
        ? std::clamp(step, 0.0, 360.0) : 0.0;
    return workspace;
}

GlView::RecoveryState::CleaveWorkspace
GlView::currentStagedCleaveWorkspaceOptions() const
{
    RecoveryState::CleaveWorkspace workspace{};
    workspace.bounded = cleaveBounded_;
    workspace.stagedMode = cleaveStagedMode_;
    workspace.chainEnabled = cleaveChainEnabled_;
    workspace.measurementsVisible = cleaveMeasurementsVisible_;
    workspace.measureUnit = cleaveMeasureUnit_;
    workspace.anglesVisible = cleaveAnglesVisible_;
    workspace.lengthsVisible = cleaveLengthsVisible_;
    workspace.lineNumbersVisible = cleaveLineNumbersVisible_;
    workspace.angleStepDegrees = cleaveAngleStepDegrees_;
    workspace.shapeOptions = cleaveShapeOptions_;
    workspace.transformMode = stagedCleaveTransformMode_;
    return workspace;
}

GlView::RecoveryState::CleaveWorkspace GlView::captureStagedCleaveWorkspace(
    const CleaveWorkspaceFamily family) const
{
    if ( family != cleaveWorkspaceFamily_ )
    {
        return stagedCleavePeerWorkspace_
            ? *stagedCleavePeerWorkspace_ : defaultStagedCleaveWorkspaceOptions(level_);
    }
    RecoveryState::CleaveWorkspace workspace = currentStagedCleaveWorkspaceOptions();
    workspace.draft = captureStagedCleaveDraftState();
    workspace.history = captureStagedCleaveHistory();
    return workspace;
}


GlView::RecoveryState::CleaveDraftView GlView::stagedCleaveDraftView(
    const CleaveWorkspaceFamily family) const noexcept
{
    const auto snapshotView = [](const RecoveryState::CleaveDraftSnapshot& draft)
    {
        return RecoveryState::CleaveDraftView{
            draft.lines, &draft.context, draft.chainOrigin, draft.applyOrder,
            draft.selectedCleaveLines, draft.selectedCleaveLine };
    };
    if ( family != cleaveWorkspaceFamily_ )
    {
        return stagedCleavePeerWorkspace_
            ? snapshotView(stagedCleavePeerWorkspace_->draft) : RecoveryState::CleaveDraftView{};
    }
    if ( stagedCleaveHistory_.gestureActive )
    {
        if ( stagedCleaveHistory_.gestureBefore )
        {
            return snapshotView(*stagedCleaveHistory_.gestureBefore);
        }
        if ( stagedCleaveHistory_.cursor < stagedCleaveHistory_.states.size() )
        {
            return snapshotView(stagedCleaveHistory_.states[stagedCleaveHistory_.cursor]);
        }
    }
    return RecoveryState::CleaveDraftView{
        stagedCleaveLines_, &stagedCleaveContext_, stagedCleaveChainOrigin_,
        stagedCleaveApplyOrder_, selectedStagedCleaveLines_, selectedStagedCleaveLine_ };
}

bool GlView::cleaveWorkspaceBounded(const CleaveWorkspaceFamily family) const noexcept
{
    return family == cleaveWorkspaceFamily_ ? cleaveBounded_
        : stagedCleavePeerWorkspace_ ? stagedCleavePeerWorkspace_->bounded.value_or(true) : true;
}

GlView::RecoveryState::CleaveWorkspace GlView::takeCurrentStagedCleaveWorkspace()
{
    RecoveryState::CleaveWorkspace workspace = currentStagedCleaveWorkspaceOptions();
    workspace.draft = RecoveryState::CleaveDraftSnapshot{
        std::move(stagedCleaveLines_), std::move(stagedCleaveContext_),
        stagedCleaveChainOrigin_, std::move(stagedCleaveApplyOrder_),
        std::move(selectedStagedCleaveLines_), selectedStagedCleaveLine_ };
    if ( !stagedCleaveHistory_.states.empty() )
    {
        workspace.history.emplace(RecoveryState::CleaveHistory{
            std::move(stagedCleaveHistory_.states), stagedCleaveHistory_.cursor });
    }
    stagedCleaveHistory_ = {};
    stagedCleaveChainOrigin_.reset();
    selectedStagedCleaveLine_ = -1;
    return workspace;
}

void GlView::installCurrentStagedCleaveWorkspace(
    RecoveryState::CleaveWorkspace&& workspace, const bool historyActive)
{
    cleaveBounded_ = workspace.bounded.value_or(workspace.draft.lines.empty());
    cleaveStagedMode_ = workspace.stagedMode;
    cleaveChainEnabled_ = workspace.chainEnabled;
    cleaveMeasurementsVisible_ = workspace.measurementsVisible;
    cleaveMeasureUnit_ = normalizedCleaveMeasureUnit(workspace.measureUnit, level_);
    cleaveAnglesVisible_ = workspace.anglesVisible;
    cleaveLengthsVisible_ = workspace.lengthsVisible;
    cleaveLineNumbersVisible_ = workspace.lineNumbersVisible;
    cleaveAngleStepDegrees_ = workspace.angleStepDegrees;
    cleaveShapeOptions_ = workspace.shapeOptions;
    stagedCleaveTransformMode_ = workspace.transformMode;
    stagedCleaveShapePlacementPlan_.reset();
    stagedCleaveShapePreviewCenter_.reset();
    stagedCleaveShapePreviewFrame_.reset();
    stagedCleaveTransformHudAnchor_.reset();
    stagedCleaveTransformHudRect_ = {};
    const bool publicationWasSuppressed = suppressStagedCleavePublication_;
    suppressStagedCleavePublication_ = true;
    installStagedCleaveDraftState(std::move(workspace.draft));
    stagedCleaveHistory_ = {};
    if ( workspace.history )
    {
        stagedCleaveHistory_.states = std::move(workspace.history->snapshots);
        stagedCleaveHistory_.cursor = workspace.history->cursor;
    }
    stagedCleaveHistory_.active = historyActive;
    stagedCleaveHistory_.ownedBytes = stagedCleaveHistoryStorageBytes();
    suppressStagedCleavePublication_ = publicationWasSuppressed;
}

void GlView::selectStagedCleaveWorkspace(const CleaveWorkspaceFamily family)
{
    if ( family == cleaveWorkspaceFamily_ )
    {
        return;
    }
    // The authored signal must reach MainWindow before the blocked workspace
    // exchange, so the departing family's dirty state is published.
    (void)finishStagedCleaveWheelSession();
    // Rollback and container exchange are one settled ownership publication.
    // The enclosing tool setter publishes once after its final enabled flags.
    const QSignalBlocker blockSignals(this);
    suspendStagedCleaveInteraction();
    const bool previousHistoryActive = stagedCleaveHistory_.active;
    const bool nextHistoryActive = stagedCleavePeerHistoryActive_;
    RecoveryState::CleaveWorkspace next = stagedCleavePeerWorkspace_
        ? std::move(*stagedCleavePeerWorkspace_) : defaultStagedCleaveWorkspaceOptions(level_);
    stagedCleavePeerWorkspace_ = takeCurrentStagedCleaveWorkspace();
    stagedCleavePeerHistoryActive_ = previousHistoryActive;
    cleaveWorkspaceFamily_ = family;
    installCurrentStagedCleaveWorkspace(std::move(next), nextHistoryActive);
}

std::size_t GlView::stagedCleaveWorkspaceOwnedBytesEstimate(
    const RecoveryState::CleaveWorkspace& workspace) noexcept
{
    constexpr std::size_t kMaximum = std::numeric_limits<std::size_t>::max();
    std::size_t bytes = sizeof(RecoveryState::CleaveWorkspace);
    const auto addBytes = [&bytes](const std::size_t additional)
    {
        bytes = additional > kMaximum - bytes ? kMaximum : bytes + additional;
    };
    addBytes(stagedCleaveDraftStateOwnedBytes(workspace.draft)
        - sizeof(StagedCleaveDraftState));
    if ( workspace.history )
    {
        const auto& history = *workspace.history;
        addBytes(history.snapshots.capacity() > kMaximum / sizeof(StagedCleaveDraftState)
            ? kMaximum : history.snapshots.capacity() * sizeof(StagedCleaveDraftState));
        for ( const auto& snapshot : history.snapshots )
        {
            addBytes(stagedCleaveDraftStateOwnedBytes(snapshot)
                - sizeof(StagedCleaveDraftState));
        }
    }
    return bytes;
}

std::size_t GlView::stagedCleaveWorkspaceOwnedBytesEstimate(
    const CleaveWorkspaceFamily family) const noexcept
{
    if ( family != cleaveWorkspaceFamily_ )
    {
        return stagedCleavePeerWorkspace_
            ? stagedCleaveWorkspaceOwnedBytesEstimate(*stagedCleavePeerWorkspace_) : 0u;
    }
    const std::size_t aggregate = stagedCleaveRecoveryWorkspaceOwnedBytesEstimate();
    const std::size_t peerBytes = stagedCleavePeerWorkspace_
        ? stagedCleaveWorkspaceOwnedBytesEstimate(*stagedCleavePeerWorkspace_) : 0u;
    return aggregate >= peerBytes ? aggregate - peerBytes : 0u;
}

bool GlView::restoreStagedCleaveWorkspace(
    const CleaveWorkspaceFamily family,
    RecoveryState::CleaveWorkspace&& workspace,
    QString* const error)
{
    if ( error )
    {
        error->clear();
    }
    const bool validFamily = family == CleaveWorkspaceFamily::Sector
        || family == CleaveWorkspaceFamily::Surface;
    const bool surfaceMode = family == CleaveWorkspaceFamily::Surface;
    const auto validContextFamily = [surfaceMode](const RecoveryState::CleaveContext& context)
    {
        return !context.valid || context.surfaceMode == surfaceMode;
    };
    const bool validHistory = !workspace.history
        || (!workspace.history->snapshots.empty()
            && workspace.history->snapshots.size() <= kStagedCleaveHistoryMaxSnapshots
            && workspace.history->cursor < workspace.history->snapshots.size()
            && std::all_of(workspace.history->snapshots.begin(), workspace.history->snapshots.end(),
                [&validContextFamily](const auto& snapshot)
                {
                    return validContextFamily(snapshot.context);
                })
            && stagedCleaveDraftStatesEqual(workspace.draft,
                workspace.history->snapshots[workspace.history->cursor]));
    const std::size_t sourceBytes = stagedCleaveWorkspaceOwnedBytesEstimate(workspace);
    const std::size_t liveBytes = stagedCleaveHistoryStorageBytes();
    if ( !validFamily || !validContextFamily(workspace.draft.context) || !validHistory
        || sourceBytes > kStagedCleaveHistoryMaxBytes
        || liveBytes > kStagedCleaveHistoryMaxBytes - sourceBytes )
    {
        if ( error )
        {
            *error = QStringLiteral("The Cleave workspace is invalid or exceeds its aggregate 16 MiB restore budget.");
        }
        return false;
    }
    const bool historyActive = workspace.history.has_value()
        || !workspace.draft.lines.empty() || workspace.draft.context.valid;
    if ( family == cleaveWorkspaceFamily_ )
    {
        (void)cancelStagedCleaveWheelSession();
        const QSignalBlocker blockSignals(this);
        suspendStagedCleaveInteraction();
        installCurrentStagedCleaveWorkspace(std::move(workspace), historyActive);
    }
    else
    {
        stagedCleavePeerWorkspace_ = std::move(workspace);
        stagedCleavePeerHistoryActive_ = historyActive;
    }
    noteStagedCleaveAuthoredChange(family);
    Q_EMIT viewportStateChanged();
    update();
    return true;
}

void GlView::setStagedCleaveTransformShortcutBindings(
    QList<QKeySequence> move,
    QList<QKeySequence> rotate,
    QList<QKeySequence> scale)
{
    stagedCleaveTransformShortcutBindings_ = {
        std::move(move), std::move(rotate), std::move(scale) };
}

std::optional<GlView::StagedCleaveTransformMode>
GlView::stagedCleaveTransformShortcutForKey(
    const int key, const Qt::KeyboardModifiers modifiers) const
{
    for ( std::size_t index = 0; index < stagedCleaveTransformShortcutBindings_.size(); ++index )
    {
        if ( ui::shortcuts::shortcutListMatchesKey(
            stagedCleaveTransformShortcutBindings_[index], key, modifiers, false) )
        {
            return static_cast<StagedCleaveTransformMode>(index);
        }
    }
    return std::nullopt;
}

void GlView::resetStagedCleaveWorkspaces()
{
    (void)cancelStagedCleaveWheelSession();
    const std::uint64_t previousRevision = stagedCleaveAuthoredRevision_;
    {
        const QSignalBlocker blockSignals(this);
        suspendStagedCleaveInteraction();
        cancelAllStagedCleaveWorkspaces();
        RecoveryState::CleaveWorkspace fresh = defaultStagedCleaveWorkspaceOptions(level_);
        stagedCleavePeerWorkspace_ = fresh;
        stagedCleavePeerHistoryActive_ = false;
        cleaveWorkspaceFamily_ = CleaveWorkspaceFamily::Sector;
        installCurrentStagedCleaveWorkspace(std::move(fresh), false);
        sectorCleaveToolEnabled_ = false;
        surfaceCleaveToolEnabled_ = false;
    }
    if ( stagedCleaveAuthoredRevision_ != previousRevision )
    {
        Q_EMIT stagedCleaveAuthoredStateChanged(stagedCleaveAuthoredRevision_);
    }
    Q_EMIT viewportStateChanged();
    update();
}

void GlView::cancelStagedCleaveWorkspace(const CleaveWorkspaceFamily family)
{
    if ( family == cleaveWorkspaceFamily_ )
    {
        cancelStagedCleave();
        return;
    }
    if ( !stagedCleavePeerWorkspace_ )
    {
        return;
    }
    auto& peer = *stagedCleavePeerWorkspace_;
    const bool changed = !peer.draft.lines.empty() || peer.draft.context.valid
        || peer.draft.chainOrigin.has_value() || peer.history.has_value();
    peer.draft = {};
    peer.history.reset();
    stagedCleavePeerHistoryActive_ = false;
    if ( changed )
    {
        noteStagedCleaveAuthoredChange(family);
        Q_EMIT viewportStateChanged();
        update();
    }
}

void GlView::cancelAllStagedCleaveWorkspaces()
{
    const CleaveWorkspaceFamily peerFamily = cleaveWorkspaceFamily_ == CleaveWorkspaceFamily::Sector
        ? CleaveWorkspaceFamily::Surface : CleaveWorkspaceFamily::Sector;
    {
        const QSignalBlocker blockSignals(this);
        cancelStagedCleaveWorkspace(peerFamily);
        cancelStagedCleave();
    }
    Q_EMIT stagedCleaveAuthoredStateChanged(stagedCleaveAuthoredRevision_);
    Q_EMIT viewportStateChanged();
}

GlView::RecoveryState::CleaveDraftSnapshot
GlView::captureStagedCleaveDraftState() const
{
    const StagedCleaveDraftState* settled = nullptr;
    if (stagedCleaveHistory_.gestureActive)
    {
        if (stagedCleaveHistory_.gestureBefore)
        {
            settled = &*stagedCleaveHistory_.gestureBefore;
        }
        else if (stagedCleaveHistory_.active
            && stagedCleaveHistory_.cursor < stagedCleaveHistory_.states.size())
        {
            settled = &stagedCleaveHistory_.states[stagedCleaveHistory_.cursor];
        }
    }

    const std::vector<StagedCleaveLine>& sourceLines = settled
        ? settled->lines
        : stagedCleaveLines_;
    const StagedCleaveContext& sourceContext = settled
        ? settled->context
        : stagedCleaveContext_;
    const std::optional<StagedCleaveChainOrigin>& sourceOrigin = settled
        ? settled->chainOrigin
        : stagedCleaveChainOrigin_;
    const std::vector<std::int32_t>& sourceOrder = settled
        ? settled->applyOrder
        : stagedCleaveApplyOrder_;
    const std::vector<std::int32_t>& sourceSelection = settled
        ? settled->selectedCleaveLines
        : selectedStagedCleaveLines_;
    const std::int32_t sourceActive = settled
        ? settled->selectedCleaveLine
        : selectedStagedCleaveLine_;

    RecoveryState::CleaveDraftSnapshot result{};
    result.lines.reserve(sourceLines.size());
    for (const StagedCleaveLine& source : sourceLines)
    {
        RecoveryState::CleaveLine line{};
        line.first = source.first;
        line.second = source.second;
        line.planeNormal = source.planeNormal;
        line.constructionPlaneNormal = source.constructionPlaneNormal;
        line.angleReferenceAxis = source.angleReferenceAxis;
        line.authoredCamera.orbit = source.authoredCamera.orbit;
        line.authoredCamera.projectionType = source.authoredCamera.projectionType;
        line.authoredCamera.fovXDeg = source.authoredCamera.fovXDeg;
        line.authoredCamera.orthoScale = source.authoredCamera.orthoScale;
        line.authoredCamera.nearPlane = source.authoredCamera.nearPlane;
        line.authoredCamera.farPlane = source.authoredCamera.farPlane;
        line.authoredCamera.clipEnabled = source.authoredCamera.clipEnabled;
        line.authoredCamera.valid = source.authoredCamera.valid;
        line.linkedToPrevious = source.linkedToPrevious;
        line.linkedToNext = source.linkedToNext;
        result.lines.push_back(std::move(line));
    }
    result.context.valid = sourceContext.valid;
    result.context.surfaceMode = sourceContext.surfaceMode;
    result.context.projection = sourceContext.projection;
    result.context.view = sourceContext.view;
    result.context.viewportWidth = sourceContext.viewportWidth;
    result.context.viewportHeight = sourceContext.viewportHeight;
    result.context.sourceSurface = sourceContext.sourceSurface;
    result.context.targetSectors = sourceContext.targetSectors;
    result.context.targetSurfaces = sourceContext.targetSurfaces;
    if (sourceOrigin)
    {
        result.chainOrigin = RecoveryState::CleaveChainOrigin{
            sourceOrigin->lineIndex,
            sourceOrigin->endpoint};
    }
    result.applyOrder = sourceOrder;
    result.selectedCleaveLines = sourceSelection;
    result.selectedCleaveLine = sourceActive;
    return result;
}

std::optional<GlView::RecoveryState::CleaveHistory>
GlView::captureStagedCleaveHistory() const
{
    if (!stagedCleaveHistory_.active
        || stagedCleaveHistory_.states.empty()
        || stagedCleaveHistory_.cursor >= stagedCleaveHistory_.states.size())
    {
        return std::nullopt;
    }
    RecoveryState::CleaveHistory result{};
    result.cursor = stagedCleaveHistory_.cursor;
    result.snapshots.reserve(stagedCleaveHistory_.states.size());
    for (const StagedCleaveDraftState& state : stagedCleaveHistory_.states)
    {
        RecoveryState::CleaveDraftSnapshot snapshot{};
        snapshot.lines.reserve(state.lines.size());
        for (const StagedCleaveLine& source : state.lines)
        {
            RecoveryState::CleaveLine line{};
            line.first = source.first;
            line.second = source.second;
            line.planeNormal = source.planeNormal;
            line.constructionPlaneNormal = source.constructionPlaneNormal;
            line.angleReferenceAxis = source.angleReferenceAxis;
            line.authoredCamera.orbit = source.authoredCamera.orbit;
            line.authoredCamera.projectionType = source.authoredCamera.projectionType;
            line.authoredCamera.fovXDeg = source.authoredCamera.fovXDeg;
            line.authoredCamera.orthoScale = source.authoredCamera.orthoScale;
            line.authoredCamera.nearPlane = source.authoredCamera.nearPlane;
            line.authoredCamera.farPlane = source.authoredCamera.farPlane;
            line.authoredCamera.clipEnabled = source.authoredCamera.clipEnabled;
            line.authoredCamera.valid = source.authoredCamera.valid;
            line.linkedToPrevious = source.linkedToPrevious;
            line.linkedToNext = source.linkedToNext;
            snapshot.lines.push_back(std::move(line));
        }
        snapshot.context.valid = state.context.valid;
        snapshot.context.surfaceMode = state.context.surfaceMode;
        snapshot.context.projection = state.context.projection;
        snapshot.context.view = state.context.view;
        snapshot.context.viewportWidth = state.context.viewportWidth;
        snapshot.context.viewportHeight = state.context.viewportHeight;
        snapshot.context.sourceSurface = state.context.sourceSurface;
        snapshot.context.targetSectors = state.context.targetSectors;
        snapshot.context.targetSurfaces = state.context.targetSurfaces;
        if (state.chainOrigin)
        {
            snapshot.chainOrigin = RecoveryState::CleaveChainOrigin{
                state.chainOrigin->lineIndex,
                state.chainOrigin->endpoint};
        }
        snapshot.applyOrder = state.applyOrder;
        snapshot.selectedCleaveLines = state.selectedCleaveLines;
        snapshot.selectedCleaveLine = state.selectedCleaveLine;
        result.snapshots.push_back(std::move(snapshot));
    }
    return result;
}

void GlView::restoreStagedCleaveDraftState(
    const RecoveryState::CleaveDraftSnapshot& snapshot,
    const bool restoreSelection)
{
    StagedCleaveDraftState state{};
    state.lines.reserve(snapshot.lines.size());
    for (const RecoveryState::CleaveLine& source : snapshot.lines)
    {
        StagedCleaveLine line{};
        line.first = source.first;
        line.second = source.second;
        line.planeNormal = source.planeNormal;
        line.constructionPlaneNormal = source.constructionPlaneNormal;
        line.angleReferenceAxis = source.angleReferenceAxis;
        line.authoredCamera.orbit = source.authoredCamera.orbit;
        line.authoredCamera.projectionType = source.authoredCamera.projectionType;
        line.authoredCamera.fovXDeg = source.authoredCamera.fovXDeg;
        line.authoredCamera.orthoScale = source.authoredCamera.orthoScale;
        line.authoredCamera.nearPlane = source.authoredCamera.nearPlane;
        line.authoredCamera.farPlane = source.authoredCamera.farPlane;
        line.authoredCamera.clipEnabled = source.authoredCamera.clipEnabled;
        line.authoredCamera.valid = source.authoredCamera.valid;
        line.linkedToPrevious = source.linkedToPrevious;
        line.linkedToNext = source.linkedToNext;
        state.lines.push_back(std::move(line));
    }
    state.context.valid = snapshot.context.valid;
    state.context.surfaceMode = snapshot.context.surfaceMode;
    state.context.projection = snapshot.context.projection;
    state.context.view = snapshot.context.view;
    state.context.viewportWidth = snapshot.context.viewportWidth;
    state.context.viewportHeight = snapshot.context.viewportHeight;
    state.context.sourceSurface = snapshot.context.sourceSurface;
    state.context.targetSectors = snapshot.context.targetSectors;
    state.context.targetSurfaces = snapshot.context.targetSurfaces;
    if (snapshot.chainOrigin)
    {
        state.chainOrigin = StagedCleaveChainOrigin{
            snapshot.chainOrigin->lineIndex,
            snapshot.chainOrigin->endpoint};
    }
    state.applyOrder = snapshot.applyOrder;
    if (restoreSelection)
    {
        state.selectedCleaveLines = snapshot.selectedCleaveLines;
        state.selectedCleaveLine = snapshot.selectedCleaveLine;
    }
    else
    {
        state.selectedCleaveLines = selectedStagedCleaveLines_;
        state.selectedCleaveLine = selectedStagedCleaveLine_;
    }
    resetStagedCleaveHistory();
    const bool publicationWasSuppressed = suppressStagedCleavePublication_;
    suppressStagedCleavePublication_ = true;
    installStagedCleaveDraftState(std::move(state));
    if ( !stagedCleaveLines_.empty()
        || stagedCleaveContext_.valid
        || stagedCleaveChainOrigin_ )
    {
        stagedCleaveHistory_.active = true;
        const std::size_t currentBytes =
            stagedCleaveDraftStateCurrentOwnedBytesEstimate();
        const std::size_t storageBeforeCopy = stagedCleaveHistoryStorageBytes();
        const std::size_t stateShellBytes = sizeof(StagedCleaveDraftState);
        if ( currentBytes <= kStagedCleaveHistoryMaxBytes
            && storageBeforeCopy <= kStagedCleaveHistoryMaxBytes
            && stateShellBytes <= kStagedCleaveHistoryMaxBytes - storageBeforeCopy
            && currentBytes <= kStagedCleaveHistoryMaxBytes
                - storageBeforeCopy - stateShellBytes )
        {
            // Current-only recovery starts a fresh timeline from the
            // installed draft. Account for the retained baseline copy before
            // allocating it; oversized legacy drafts remain usable without
            // an Undo snapshot.
            stagedCleaveHistory_.states.reserve(1u);
            const std::size_t storageAfterReserve =
                stagedCleaveHistoryStorageBytes();
            if ( storageAfterReserve <= kStagedCleaveHistoryMaxBytes
                && currentBytes <= kStagedCleaveHistoryMaxBytes
                    - storageAfterReserve )
            {
                stagedCleaveHistory_.states.push_back(stagedCleaveDraftState());
                stagedCleaveHistory_.cursor = 0;
            }
        }
    }
    suppressStagedCleavePublication_ = publicationWasSuppressed;
    noteStagedCleaveAuthoredChange();
    if ( !publicationWasSuppressed )
    {
        Q_EMIT viewportStateChanged();
    }
}

void GlView::restoreStagedCleaveDraftState(
    const RecoveryState::CleaveDraftSnapshot& snapshot,
    const RecoveryState::CleaveHistory& history,
    const bool restoreSelection)
{
#ifdef SED_APP_MAIN_WINDOW_TEST_ACCESS
    const std::size_t liveOwnedArrayBytes = cleaveRecoveryLiveOwnedArraysForTest();
    const std::size_t sourceSnapshotOwnedArrayBytes =
        cleaveRecoveryDraftOwnedArrayBytes(snapshot);
#endif
    RecoveryState::CleaveDraftSnapshot snapshotCopy = snapshot;
    RecoveryState::CleaveHistory historyCopy = history;
#ifdef SED_APP_MAIN_WINDOW_TEST_ACCESS
    if (cleaveRecoveryRestoreOwnedArrayTrackingForTest_)
    {
        std::size_t simultaneousOwnedArrayBytes =
            cleaveRecoveryRestoreSourceOwnedArrayBytesForTest_;
        simultaneousOwnedArrayBytes = addCleaveRecoveryOwnedArrayBytes(
            simultaneousOwnedArrayBytes, liveOwnedArrayBytes, 1u);
        simultaneousOwnedArrayBytes = addCleaveRecoveryOwnedArrayBytes(
            simultaneousOwnedArrayBytes, sourceSnapshotOwnedArrayBytes, 1u);
        simultaneousOwnedArrayBytes = addCleaveRecoveryOwnedArrayBytes(
            simultaneousOwnedArrayBytes,
            cleaveRecoveryDraftOwnedArrayBytes(snapshotCopy),
            1u);
        simultaneousOwnedArrayBytes = addCleaveRecoveryOwnedArrayBytes(
            simultaneousOwnedArrayBytes,
            cleaveRecoveryHistoryOwnedArrayBytes(historyCopy),
            1u);
        cleaveRecoveryRestorePeakOwnedArrayBytesForTest_ = std::max(
            cleaveRecoveryRestorePeakOwnedArrayBytesForTest_,
            simultaneousOwnedArrayBytes);
    }
#endif
    restoreStagedCleaveDraftState(
        std::move(snapshotCopy),
        std::move(historyCopy),
        restoreSelection);
}

void GlView::restoreStagedCleaveDraftState(
    RecoveryState::CleaveDraftSnapshot&& snapshot,
    RecoveryState::CleaveHistory&& history,
    const bool restoreSelection)
{
    const bool publicationWasSuppressed = suppressStagedCleavePublication_;
    std::vector<std::int32_t> retainedSelection{};
    const std::int32_t retainedActive = selectedStagedCleaveLine_;
    if ( !restoreSelection )
    {
        retainedSelection = std::move(selectedStagedCleaveLines_);
    }

    StagedCleaveDraftState current{};
    current.lines = std::move(snapshot.lines);
    current.context = std::move(snapshot.context);
    current.chainOrigin = snapshot.chainOrigin;
    current.applyOrder = std::move(snapshot.applyOrder);
    current.selectedCleaveLines = restoreSelection
        ? std::move(snapshot.selectedCleaveLines)
        : std::move(retainedSelection);
    current.selectedCleaveLine = restoreSelection
        ? snapshot.selectedCleaveLine
        : retainedActive;

    suppressStagedCleavePublication_ = true;
    resetStagedCleaveHistory();
    installStagedCleaveDraftState(std::move(current));

    const bool historyShapeValid = !history.snapshots.empty()
        && history.snapshots.size() <= kStagedCleaveHistoryMaxSnapshots
        && history.cursor < history.snapshots.size();
    if ( historyShapeValid )
    {
        stagedCleaveHistory_.states = std::move(history.snapshots);
        stagedCleaveHistory_.cursor = history.cursor;
        stagedCleaveHistory_.active = true;
        stagedCleaveHistory_.ownedBytes = stagedCleaveHistoryStorageBytes();
        if ( stagedCleaveHistory_.ownedBytes > kStagedCleaveHistoryMaxBytes )
        {
            resetStagedCleaveHistory();
        }
    }
    else if ( !stagedCleaveLines_.empty()
        || stagedCleaveContext_.valid
        || stagedCleaveChainOrigin_ )
    {
        stagedCleaveHistory_.active = true;
        if ( stagedCleaveDraftStateCurrentOwnedBytesEstimate()
            <= kStagedCleaveHistoryMaxBytes )
        {
            stagedCleaveHistory_.states.reserve(1u);
            stagedCleaveHistory_.states.push_back(stagedCleaveDraftState());
            stagedCleaveHistory_.ownedBytes = stagedCleaveHistoryStorageBytes();
        }
    }

    suppressStagedCleavePublication_ = publicationWasSuppressed;
    if ( !publicationWasSuppressed )
    {
        noteStagedCleaveAuthoredChange();
        Q_EMIT viewportStateChanged();
    }
}

void GlView::captureStagedCleaveLineCamera(StagedCleaveLine& line) const noexcept
{
    line.authoredCamera.orbit = camera_.orbit;
    line.authoredCamera.projectionType = camera_.projectionType();
    line.authoredCamera.fovXDeg = camera_.fovXDeg();
    line.authoredCamera.orthoScale = camera_.orthoScale();
    line.authoredCamera.nearPlane = camera_.nearPlane();
    line.authoredCamera.farPlane = camera_.farPlane();
    line.authoredCamera.clipEnabled = camera_.clipEnabled();
    line.authoredCamera.valid = true;
}

bool GlView::restoreStagedCleaveLineCamera(const std::size_t index)
{
    if (index >= stagedCleaveLines_.size())
    {
        return false;
    }

    const StagedCleaveCameraState state = stagedCleaveLines_[index].authoredCamera;
    if (!state.valid)
    {
        return false;
    }

    cancelViewAnimation();
    camera_.orbit = state.orbit;
    camera_.SetProjectionType(state.projectionType);
    camera_.SetFovXDeg(state.fovXDeg);
    camera_.SetOrthoScale(state.orthoScale);
    camera_.SetNearFar(state.nearPlane, state.farPlane);
    camera_.SetClipEnabled(state.clipEnabled);
    camera_.UpdateFromOrbit();
    markViewChanged();
    Q_EMIT viewportStateChanged();
    update();
    return true;
}

bool GlView::stagedCleaveLineSelected(const std::int32_t line) const noexcept
{
    return std::binary_search(
        selectedStagedCleaveLines_.begin(),
        selectedStagedCleaveLines_.end(),
        line);
}

void GlView::setSelectedStagedCleaveLines(
    std::vector<std::int32_t> lines,
    std::int32_t activeLine,
    const bool publishState)
{
    lines.erase(
        std::remove_if(
            lines.begin(),
            lines.end(),
            [this](const std::int32_t line)
            {
                return line < 0
                    || static_cast<std::size_t>(line) >= stagedCleaveLines_.size();
            }),
        lines.end());
    std::sort(lines.begin(), lines.end());
    lines.erase(std::unique(lines.begin(), lines.end()), lines.end());
    if (activeLine < 0
        || !std::binary_search(lines.begin(), lines.end(), activeLine))
    {
        activeLine = lines.empty() ? -1 : lines.back();
    }
    if (selectedStagedCleaveLines_ != lines || selectedStagedCleaveLine_ != activeLine)
    {
        (void)finishStagedCleaveWheelSession();
    }
    selectedStagedCleaveLines_ = std::move(lines);
    selectedStagedCleaveLine_ = activeLine;
    normalizeStagedCleaveTransformMode();
    update();
    if ( publishState )
    {
        Q_EMIT viewportStateChanged();
    }
}

void GlView::applyStagedCleaveClickSelection(
    const std::int32_t line,
    const Qt::KeyboardModifiers modifiers)
{
    if (line < 0 || static_cast<std::size_t>(line) >= stagedCleaveLines_.size())
    {
        setSelectedStagedCleaveLines({});
        return;
    }

    std::vector<std::int32_t> next = selectedStagedCleaveLines_;
    if (modifiers.testFlag(Qt::ControlModifier))
    {
        next.erase(std::remove(next.begin(), next.end(), line), next.end());
        setSelectedStagedCleaveLines(std::move(next));
        return;
    }
    if (modifiers.testFlag(Qt::ShiftModifier))
    {
        next.push_back(line);
    }
    else
    {
        next.assign(1, line);
    }
    setSelectedStagedCleaveLines(std::move(next), line);
}

GlView::StagedCleaveDraftState GlView::stagedCleaveDraftState() const
{
    return StagedCleaveDraftState{
        stagedCleaveLines_,
        stagedCleaveContext_,
        stagedCleaveChainOrigin_,
        stagedCleaveApplyOrder_,
        selectedStagedCleaveLines_,
        selectedStagedCleaveLine_,
    };
}

const std::vector<GlView::StagedCleaveLine>&
GlView::stagedCleaveGestureBaselineLines() const noexcept
{
    if (stagedCleaveHistory_.gestureBefore)
    {
        return stagedCleaveHistory_.gestureBefore->lines;
    }
    if (stagedCleaveHistory_.active
        && stagedCleaveHistory_.cursor < stagedCleaveHistory_.states.size())
    {
        return stagedCleaveHistory_.states[stagedCleaveHistory_.cursor].lines;
    }
    return stagedCleaveLines_;
}

bool GlView::stagedCleaveDraftStatesEqual(
    const StagedCleaveDraftState& left,
    const StagedCleaveDraftState& right) noexcept
{
    const auto samePoint = [](const TVector& a, const TVector& b)
    {
        return a.x == b.x && a.y == b.y && a.z == b.z;
    };
    const auto sameOrbit = [](const app::ViewportCamera::OrbitState& a,
                              const app::ViewportCamera::OrbitState& b)
    {
        return a.center == b.center && a.pan == b.pan && a.radius == b.radius
            && a.distance == b.distance && a.yawDeg == b.yawDeg
            && a.pitchDeg == b.pitchDeg && a.rollDeg == b.rollDeg;
    };
    const auto sameMatrix = [](const QMatrix4x4& a, const QMatrix4x4& b)
    {
        const float* const aData = a.constData();
        const float* const bData = b.constData();
        for ( std::size_t index = 0; index < 16; ++index )
        {
            if ( aData[index] != bData[index] )
            {
                return false;
            }
        }
        return true;
    };
    if ( left.lines.size() != right.lines.size()
        || left.applyOrder != right.applyOrder
        || left.context.valid != right.context.valid
        || left.context.surfaceMode != right.context.surfaceMode
        || !sameMatrix(left.context.projection, right.context.projection)
        || !sameMatrix(left.context.view, right.context.view)
        || left.context.viewportWidth != right.context.viewportWidth
        || left.context.viewportHeight != right.context.viewportHeight
        || left.context.sourceSurface.sector != right.context.sourceSurface.sector
        || left.context.sourceSurface.surface != right.context.sourceSurface.surface
        || left.context.targetSectors != right.context.targetSectors
        || left.context.targetSurfaces.size() != right.context.targetSurfaces.size() )
    {
        return false;
    }
    for ( std::size_t index = 0; index < left.context.targetSurfaces.size(); ++index )
    {
        const SurfaceRef& a = left.context.targetSurfaces[index];
        const SurfaceRef& b = right.context.targetSurfaces[index];
        if ( a.sector != b.sector || a.surface != b.surface )
        {
            return false;
        }
    }
    const bool sameOrigin = left.chainOrigin.has_value() == right.chainOrigin.has_value()
        && (!left.chainOrigin
            || (left.chainOrigin->lineIndex == right.chainOrigin->lineIndex
                && left.chainOrigin->endpoint == right.chainOrigin->endpoint));
    if ( !sameOrigin )
    {
        return false;
    }
    for ( std::size_t index = 0; index < left.lines.size(); ++index )
    {
        const StagedCleaveLine& a = left.lines[index];
        const StagedCleaveLine& b = right.lines[index];
        const StagedCleaveCameraState& ac = a.authoredCamera;
        const StagedCleaveCameraState& bc = b.authoredCamera;
        if ( !samePoint(a.first, b.first) || !samePoint(a.second, b.second)
            || !samePoint(a.planeNormal, b.planeNormal)
            || !samePoint(a.constructionPlaneNormal, b.constructionPlaneNormal)
            || !samePoint(a.angleReferenceAxis, b.angleReferenceAxis)
            || a.linkedToPrevious != b.linkedToPrevious
            || a.linkedToNext != b.linkedToNext
            || ac.valid != bc.valid || ac.projectionType != bc.projectionType
            || !sameOrbit(ac.orbit, bc.orbit)
            || ac.fovXDeg != bc.fovXDeg || ac.orthoScale != bc.orthoScale
            || ac.nearPlane != bc.nearPlane || ac.farPlane != bc.farPlane
            || ac.clipEnabled != bc.clipEnabled )
        {
            return false;
        }
    }
    return true;
}

std::size_t GlView::stagedCleaveDraftStateOwnedBytes(
    const StagedCleaveDraftState& state) noexcept
{
    constexpr std::size_t kMaximum = std::numeric_limits<std::size_t>::max();
    std::size_t bytes = sizeof(StagedCleaveDraftState);
    const auto addCapacity = [&bytes](const std::size_t count, const std::size_t elementSize)
    {
        constexpr std::size_t kLimit = std::numeric_limits<std::size_t>::max();
        if ( count > (kLimit - bytes) / elementSize )
        {
            bytes = kLimit;
            return;
        }
        bytes += count * elementSize;
    };
    addCapacity(state.lines.capacity(), sizeof(StagedCleaveLine));
    addCapacity(state.context.targetSectors.capacity(), sizeof(std::int32_t));
    addCapacity(state.context.targetSurfaces.capacity(), sizeof(SurfaceRef));
    addCapacity(state.applyOrder.capacity(), sizeof(std::int32_t));
    addCapacity(state.selectedCleaveLines.capacity(), sizeof(std::int32_t));
    return std::min(bytes, kMaximum);
}

std::size_t GlView::stagedCleaveDraftStateCurrentOwnedBytesEstimate() const noexcept
{
    constexpr std::size_t kMaximum = std::numeric_limits<std::size_t>::max();
    std::size_t bytes = sizeof(StagedCleaveDraftState);
    const auto addSize = [&bytes](const std::size_t count, const std::size_t elementSize)
    {
        constexpr std::size_t kLimit = std::numeric_limits<std::size_t>::max();
        if ( count > (kLimit - bytes) / elementSize )
        {
            bytes = kLimit;
            return;
        }
        bytes += count * elementSize;
    };
    addSize(stagedCleaveLines_.capacity(), sizeof(StagedCleaveLine));
    addSize(stagedCleaveContext_.targetSectors.capacity(), sizeof(std::int32_t));
    addSize(stagedCleaveContext_.targetSurfaces.capacity(), sizeof(SurfaceRef));
    addSize(stagedCleaveApplyOrder_.capacity(), sizeof(std::int32_t));
    addSize(selectedStagedCleaveLines_.capacity(), sizeof(std::int32_t));
    return std::min(bytes, kMaximum);
}

std::size_t GlView::stagedCleaveHistoryStorageBytes() const noexcept
{
    constexpr std::size_t kMaximum = std::numeric_limits<std::size_t>::max();
    std::size_t bytes = stagedCleaveDraftStateCurrentOwnedBytesEstimate();
    const auto addBytes = [&bytes](const std::size_t addition)
    {
        bytes = addition > std::numeric_limits<std::size_t>::max() - bytes
            ? std::numeric_limits<std::size_t>::max()
            : bytes + addition;
    };
    const auto addArray = [&addBytes](const std::size_t count, const std::size_t elementSize)
    {
        constexpr std::size_t kLimit = std::numeric_limits<std::size_t>::max();
        if ( elementSize != 0u && count > kLimit / elementSize )
        {
            addBytes(kLimit);
            return;
        }
        addBytes(count * elementSize);
    };
    addArray(stagedCleaveHistory_.states.capacity(), sizeof(StagedCleaveDraftState));
    for ( const StagedCleaveDraftState& state : stagedCleaveHistory_.states )
    {
        const std::size_t stateBytes = stagedCleaveDraftStateOwnedBytes(state);
        addBytes(stateBytes >= sizeof(StagedCleaveDraftState)
            ? stateBytes - sizeof(StagedCleaveDraftState)
            : 0);
    }
    if ( stagedCleaveHistory_.gestureBefore )
    {
        addBytes(stagedCleaveDraftStateOwnedBytes(*stagedCleaveHistory_.gestureBefore));
    }
    addArray(stagedCleavePlaneDrag_.movingLines.capacity(), sizeof(std::int32_t));
    addArray(stagedCleavePlaneDrag_.affectedLines.capacity(), sizeof(StagedCleaveLineMove));
    addArray(stagedCleaveTransformDrag_.selectedLines.capacity(), sizeof(std::int32_t));
    addArray(stagedCleaveTransformDrag_.affectedLines.capacity(), sizeof(StagedCleaveLineMove));
    addArray(stagedCleaveTransformDrag_.previewLines.capacity(), sizeof(StagedCleaveLine));
    addArray(stagedCleaveMarqueeSelection_.baselineSelection.capacity(), sizeof(std::int32_t));
    addArray(stagedCleaveRankByLine_.capacity(), sizeof(std::int32_t));
    addArray(stagedCleaveOrderEditorBaseline_.capacity(), sizeof(std::int32_t));
    addArray(stagedCleaveJointCycleCandidates_.capacity(), sizeof(std::int32_t));
    if ( stagedCleavePeerWorkspace_ )
    {
        addBytes(stagedCleaveWorkspaceOwnedBytesEstimate(*stagedCleavePeerWorkspace_));
    }
    return std::min(bytes, kMaximum);
}

std::size_t GlView::stagedCleaveHistoryStorageBytesEstimate() const noexcept
{
    return stagedCleaveHistoryStorageBytes();
}

#ifdef SED_APP_MAIN_WINDOW_TEST_ACCESS
std::size_t GlView::cleaveRecoveryLiveOwnedArraysForTest() const noexcept
{
    std::size_t bytes = addCleaveRecoveryVectorCapacityBytes(
        0u, stagedCleaveLines_);
    bytes = addCleaveRecoveryVectorCapacityBytes(
        bytes, stagedCleaveContext_.targetSectors);
    bytes = addCleaveRecoveryVectorCapacityBytes(
        bytes, stagedCleaveContext_.targetSurfaces);
    bytes = addCleaveRecoveryVectorCapacityBytes(
        bytes, stagedCleaveApplyOrder_);
    bytes = addCleaveRecoveryVectorCapacityBytes(
        bytes, selectedStagedCleaveLines_);
    bytes = addCleaveRecoveryVectorCapacityBytes(
        bytes, stagedCleaveHistory_.states);
    for (const StagedCleaveDraftState& snapshot : stagedCleaveHistory_.states)
    {
        bytes = addCleaveRecoveryOwnedArrayBytes(
            bytes,
            cleaveRecoveryDraftOwnedArrayBytes(snapshot),
            1u);
    }
    if (stagedCleaveHistory_.gestureBefore)
    {
        bytes = addCleaveRecoveryOwnedArrayBytes(
            bytes,
            cleaveRecoveryDraftOwnedArrayBytes(
                *stagedCleaveHistory_.gestureBefore),
            1u);
    }
    bytes = addCleaveRecoveryVectorCapacityBytes(
        bytes, stagedCleavePlaneDrag_.movingLines);
    bytes = addCleaveRecoveryVectorCapacityBytes(
        bytes, stagedCleavePlaneDrag_.affectedLines);
    bytes = addCleaveRecoveryVectorCapacityBytes(
        bytes, stagedCleaveTransformDrag_.selectedLines);
    bytes = addCleaveRecoveryVectorCapacityBytes(
        bytes, stagedCleaveTransformDrag_.affectedLines);
    bytes = addCleaveRecoveryVectorCapacityBytes(
        bytes, stagedCleaveTransformDrag_.previewLines);
    bytes = addCleaveRecoveryVectorCapacityBytes(
        bytes, stagedCleaveMarqueeSelection_.baselineSelection);
    bytes = addCleaveRecoveryVectorCapacityBytes(
        bytes, stagedCleaveRankByLine_);
    bytes = addCleaveRecoveryVectorCapacityBytes(
        bytes, stagedCleaveOrderEditorBaseline_);
    bytes = addCleaveRecoveryVectorCapacityBytes(bytes, stagedCleaveJointCycleCandidates_);
    if ( stagedCleavePeerWorkspace_ )
    {
        bytes = addCleaveRecoveryOwnedArrayBytes(bytes,
            cleaveRecoveryDraftOwnedArrayBytes(stagedCleavePeerWorkspace_->draft), 1u);
        if ( stagedCleavePeerWorkspace_->history )
        {
            bytes = addCleaveRecoveryOwnedArrayBytes(bytes,
                cleaveRecoveryHistoryOwnedArrayBytes(*stagedCleavePeerWorkspace_->history), 1u);
        }
    }
    return bytes;
}
#endif

std::size_t GlView::stagedCleaveRecoveryWorkspaceOwnedBytesEstimate() const noexcept
{
    constexpr std::size_t kMaximum = std::numeric_limits<std::size_t>::max();
    std::size_t bytes = stagedCleaveDraftStateCurrentOwnedBytesEstimate();
    const auto addBytes = [&bytes](const std::size_t addition)
    {
        bytes = addition > std::numeric_limits<std::size_t>::max() - bytes
            ? std::numeric_limits<std::size_t>::max()
            : bytes + addition;
    };
    const auto addArray = [&addBytes](const std::size_t count, const std::size_t elementSize)
    {
        constexpr std::size_t kLimit = std::numeric_limits<std::size_t>::max();
        if ( elementSize != 0u && count > kLimit / elementSize )
        {
            addBytes(kLimit);
            return;
        }
        addBytes(count * elementSize);
    };

    if ( stagedCleaveHistory_.active
        && !stagedCleaveHistory_.states.empty()
        && stagedCleaveHistory_.cursor < stagedCleaveHistory_.states.size() )
    {
        addBytes(sizeof(RecoveryState::CleaveHistory));
        // Capture reserves from the live timeline size. Charge its retained
        // capacity as a conservative upper bound before making that copy.
        addArray(
            stagedCleaveHistory_.states.capacity(),
            sizeof(RecoveryState::CleaveDraftSnapshot));
        for ( const StagedCleaveDraftState& state : stagedCleaveHistory_.states )
        {
            const std::size_t stateBytes = stagedCleaveDraftStateOwnedBytes(state);
            addBytes(stateBytes >= sizeof(StagedCleaveDraftState)
                ? stateBytes - sizeof(StagedCleaveDraftState)
                : 0u);
        }
    }
    if ( stagedCleavePeerWorkspace_ )
    {
        addBytes(stagedCleaveWorkspaceOwnedBytesEstimate(*stagedCleavePeerWorkspace_));
    }
    return std::min(bytes, kMaximum);
}

void GlView::noteStagedCleaveAuthoredChange() noexcept
{
    noteStagedCleaveAuthoredChange(cleaveWorkspaceFamily_);
}

void GlView::noteStagedCleaveAuthoredChange(const CleaveWorkspaceFamily family) noexcept
{
    ++stagedCleaveAuthoredRevision_;
    const std::size_t index = static_cast<std::size_t>(family);
    if ( index < stagedCleaveFamilyAuthoredRevisions_.size() )
    {
        ++stagedCleaveFamilyAuthoredRevisions_[index];
    }
    stagedCleaveRankCacheValid_ = false;
    if ( !suppressStagedCleavePublication_ )
    {
        Q_EMIT stagedCleaveAuthoredStateChanged(stagedCleaveAuthoredRevision_);
    }
}

bool GlView::beginStagedCleaveHistoryGesture()
{
    if ( stagedCleaveHistory_.gestureActive )
    {
        return true;
    }
    const std::size_t beforeBytes = stagedCleaveDraftStateCurrentOwnedBytesEstimate();
    const std::size_t currentBytes = stagedCleaveHistoryStorageBytes();
    const std::size_t reserveBytes = stagedCleaveHistory_.states.capacity()
            < kStagedCleaveHistoryMaxSnapshots
        ? kStagedCleaveHistoryMaxSnapshots * sizeof(StagedCleaveDraftState)
        : 0u;
    if ( beforeBytes > kStagedCleaveHistoryMaxBytes
        || currentBytes > kStagedCleaveHistoryMaxBytes
        || reserveBytes > kStagedCleaveHistoryMaxBytes - currentBytes
        || beforeBytes > kStagedCleaveHistoryMaxBytes - currentBytes - reserveBytes )
    {
        return false;
    }

    // Reserve the bounded timeline only after the aggregate byte preflight.
    // Selection and captured context can change outside history commands, so
    // retain one exact press-time state for Escape and failed releases.
    if ( stagedCleaveHistory_.states.capacity() < kStagedCleaveHistoryMaxSnapshots )
    {
        stagedCleaveHistory_.states.reserve(kStagedCleaveHistoryMaxSnapshots);
    }
    if ( stagedCleaveHistoryStorageBytes() > kStagedCleaveHistoryMaxBytes
        || beforeBytes > kStagedCleaveHistoryMaxBytes
            - stagedCleaveHistoryStorageBytes() )
    {
        return false;
    }

#ifdef SED_APP_MAIN_WINDOW_TEST_ACCESS
    ++stagedCleaveBaselineCaptureCountForTest_;
#endif
    StagedCleaveDraftState before = stagedCleaveDraftState();
    if ( stagedCleaveDraftStateOwnedBytes(before) > beforeBytes )
    {
        return false;
    }
    stagedCleaveHistory_.gestureBefore = std::move(before);
    if ( stagedCleaveHistoryStorageBytes() > kStagedCleaveHistoryMaxBytes )
    {
        stagedCleaveHistory_.gestureBefore.reset();
        return false;
    }
    stagedCleaveHistory_.gestureActive = true;
    return true;
}

bool GlView::finishStagedCleaveHistoryGesture()
{
    if ( !stagedCleaveHistory_.gestureActive )
    {
        return false;
    }
    const std::size_t currentBytesEstimate =
        stagedCleaveDraftStateCurrentOwnedBytesEstimate();
    const std::size_t storageBeforeCurrentCopy = stagedCleaveHistoryStorageBytes();
    if ( currentBytesEstimate > kStagedCleaveHistoryMaxBytes
        || storageBeforeCurrentCopy > kStagedCleaveHistoryMaxBytes
        || currentBytesEstimate
            > kStagedCleaveHistoryMaxBytes - storageBeforeCurrentCopy )
    {
        (void)restoreAndDiscardStagedCleaveHistoryGesture();
        showToast(
            "Multi-cut: local history limit reached; Clear or Apply before adding more geometry",
            3200,
            ToastPlacement::BottomCenter,
            ToastGroup::Error);
        return false;
    }
    StagedCleaveDraftState current{};
    try
    {
        // Keep the press-time rollback alive until every allocation for the
        // committed current snapshot has succeeded. If a bounded allocation
        // still fails, the live edit can be rolled back without reconstructing
        // its only exact baseline.
#ifdef SED_APP_MAIN_WINDOW_TEST_ACCESS
        ++stagedCleaveHistoryCurrentCaptureCountForTest_;
#endif
        current = stagedCleaveDraftState();
    }
    catch (const std::bad_alloc&)
    {
        (void)restoreAndDiscardStagedCleaveHistoryGesture();
        showToast(
            "Multi-cut: local history could not keep this edit; the gesture was reverted",
            3200,
            ToastPlacement::BottomCenter,
            ToastGroup::Error);
        return false;
    }

    std::optional<StagedCleaveDraftState> before =
        std::move(stagedCleaveHistory_.gestureBefore);
    stagedCleaveHistory_.gestureBefore.reset();
    stagedCleaveHistory_.gestureActive = false;
    if ( before && stagedCleaveDraftStatesEqual(*before, current) )
    {
        return false;
    }
    if ( !before && stagedCleaveHistory_.active
        && stagedCleaveHistory_.cursor < stagedCleaveHistory_.states.size()
        && stagedCleaveDraftStatesEqual(
            stagedCleaveHistory_.states[stagedCleaveHistory_.cursor],
            current ) )
    {
        return false;
    }

    stagedCleaveHistory_.active = true;
    if ( stagedCleaveDraftStateOwnedBytes(current) > kStagedCleaveHistoryMaxBytes )
    {
        stagedCleaveHistory_.states.clear();
        stagedCleaveHistory_.cursor = 0;
        stagedCleaveHistory_.ownedBytes = stagedCleaveHistoryStorageBytes();
        return true;
    }

    if ( stagedCleaveHistory_.states.empty() )
    {
        if ( before
            && stagedCleaveDraftStateOwnedBytes(*before) <= kStagedCleaveHistoryMaxBytes )
        {
            stagedCleaveHistory_.states.push_back(std::move(*before));
            before.reset();
            stagedCleaveHistory_.cursor = 0;
        }
        else
        {
            stagedCleaveHistory_.cursor = 0;
        }
    }
    else
    {
        stagedCleaveHistory_.states.resize(stagedCleaveHistory_.cursor + 1);
    }

    if ( stagedCleaveHistory_.states.empty()
        || !stagedCleaveDraftStatesEqual(stagedCleaveHistory_.states.back(), current) )
    {
        if ( stagedCleaveHistory_.states.size()
                >= kStagedCleaveHistoryMaxSnapshots
            && stagedCleaveHistory_.cursor > 0u )
        {
            // A committed authored edit can evict the farthest-past state,
            // but do so before appending so the bounded vector never grows
            // beyond its retained-state allocation.
            stagedCleaveHistory_.states.erase(
                stagedCleaveHistory_.states.begin());
            --stagedCleaveHistory_.cursor;
        }
        stagedCleaveHistory_.states.push_back(std::move(current));
        stagedCleaveHistory_.cursor = stagedCleaveHistory_.states.size() - 1;
    }
    trimStagedCleaveHistory();
    stagedCleaveHistory_.ownedBytes = stagedCleaveHistoryStorageBytes();
    noteStagedCleaveAuthoredChange();
    return true;
}

void GlView::discardStagedCleaveHistoryGesture() noexcept
{
    stagedCleaveHistory_.gestureBefore.reset();
    stagedCleaveHistory_.gestureActive = false;
}

bool GlView::restoreAndDiscardStagedCleaveHistoryGesture()
{
    if ( !stagedCleaveHistory_.gestureActive )
    {
        return false;
    }

    std::optional<StagedCleaveDraftState> gestureBefore =
        std::move(stagedCleaveHistory_.gestureBefore);
    stagedCleaveHistory_.gestureBefore.reset();
    stagedCleaveHistory_.gestureActive = false;
    const StagedCleaveDraftState* const timelineState =
        !gestureBefore
            && stagedCleaveHistory_.active
            && stagedCleaveHistory_.cursor < stagedCleaveHistory_.states.size()
        ? &stagedCleaveHistory_.states[stagedCleaveHistory_.cursor]
        : nullptr;

    if ( gestureBefore )
    {
        installStagedCleaveDraftState(std::move(*gestureBefore));
    }
    else if ( timelineState )
    {
        installStagedCleaveDraftState(*timelineState);
    }
    else
    {
        stagedCleavePlaneDrag_ = {};
        editingStagedCleaveLine_ = -1;
        editingStagedCleaveEndpoint_ = -1;
        editingDisjointStagedCleaveEndpoint_ = false;
        editingStagedCleaveEndpointPreservePlacement_ = false;
        stagedCleaveEndpointOriginClickCandidate_ = false;
        stagedCleaveLineClickCandidate_ = false;
        stagedCleaveDragSelectionApplied_ = false;
        stagedCleaveJointCycleClickCandidate_ = false;
        stagedCleaveJointCycleCandidates_.clear();
        sectorCleaveHaveFirstPoint_ = false;
        surfaceCleaveHaveFirstPoint_ = false;
        leftDragged_ = false;
        cleaveSnapMarker_.reset();
        unsetCursor();
        update();
        Q_EMIT viewportStateChanged();
    }
    stagedCleaveHistory_.ownedBytes = stagedCleaveHistoryStorageBytes();
    return true;
}

void GlView::resetStagedCleaveHistory() noexcept
{
    stagedCleaveWheelSession_.reset();
    stagedCleaveWheelShapeBefore_.reset();
    clearStagedCleaveTransformHud();
    stagedCleaveHistory_ = {};
}

void GlView::installStagedCleaveDraftState(const StagedCleaveDraftState& state)
{
    StagedCleaveDraftState copy = state;
    installStagedCleaveDraftState(std::move(copy));
}

void GlView::installStagedCleaveDraftState(StagedCleaveDraftState&& state)
{
    stagedCleaveWheelSession_.reset();
    stagedCleaveWheelShapeBefore_.reset();
    ++stagedCleaveJointGuidePreviewRevision_;
    stagedCleaveLines_ = std::move(state.lines);
    stagedCleaveContext_ = std::move(state.context);
    stagedCleaveChainOrigin_ = state.chainOrigin;
    stagedCleaveApplyOrder_ = std::move(state.applyOrder);
    if ( stagedCleaveApplyOrder_.size() != stagedCleaveLines_.size() )
    {
        stagedCleaveApplyOrder_.resize(stagedCleaveLines_.size());
        std::iota(stagedCleaveApplyOrder_.begin(), stagedCleaveApplyOrder_.end(), 0);
    }
    selectedStagedCleaveLines_ = std::move(state.selectedCleaveLines);
    selectedStagedCleaveLines_.erase(
        std::remove_if(
            selectedStagedCleaveLines_.begin(),
            selectedStagedCleaveLines_.end(),
            [this](const std::int32_t line)
            {
                return line < 0
                    || static_cast<std::size_t>(line) >= stagedCleaveLines_.size();
            }),
        selectedStagedCleaveLines_.end());
    std::sort(selectedStagedCleaveLines_.begin(), selectedStagedCleaveLines_.end());
    selectedStagedCleaveLines_.erase(
        std::unique(selectedStagedCleaveLines_.begin(), selectedStagedCleaveLines_.end()),
        selectedStagedCleaveLines_.end());
    selectedStagedCleaveLine_ = state.selectedCleaveLine >= 0
            && std::binary_search(
                selectedStagedCleaveLines_.begin(),
                selectedStagedCleaveLines_.end(),
                state.selectedCleaveLine)
        ? state.selectedCleaveLine
        : selectedStagedCleaveLines_.empty()
            ? -1
            : selectedStagedCleaveLines_.back();
    stagedCleavePlaneDrag_ = {};
    editingStagedCleaveLine_ = -1;
    editingStagedCleaveEndpoint_ = -1;
    editingDisjointStagedCleaveEndpoint_ = false;
    editingStagedCleaveEndpointPreservePlacement_ = false;
    stagedCleaveEndpointOriginClickCandidate_ = false;
    stagedCleaveLineClickCandidate_ = false;
    stagedCleaveDragSelectionApplied_ = false;
    stagedCleaveJointCycleClickCandidate_ = false;
    stagedCleaveJointCycleCandidates_.clear();
    sectorCleaveHaveFirstPoint_ = false;
    surfaceCleaveHaveFirstPoint_ = false;
    leftDragged_ = false;
    cleaveSnapMarker_.reset();
    unsetCursor();
    stagedCleaveRankCacheValid_ = false;
    stagedCleaveTransformDrag_ = {};
    clearStagedCleaveTransformHud();
    stagedCleaveMarqueeSelection_ = {};
    normalizeStagedCleaveTransformMode();
    update();
    if ( !suppressStagedCleavePublication_ )
    {
        Q_EMIT viewportStateChanged();
    }
}

void GlView::trimStagedCleaveHistory()
{
    while ( !stagedCleaveHistory_.states.empty()
        && (stagedCleaveHistory_.states.size() > kStagedCleaveHistoryMaxSnapshots
            || stagedCleaveHistoryStorageBytes() > kStagedCleaveHistoryMaxBytes ) )
    {
        if (stagedCleaveHistory_.cursor > 0)
        {
            stagedCleaveHistory_.states.erase(stagedCleaveHistory_.states.begin());
            --stagedCleaveHistory_.cursor;
        }
        else if (stagedCleaveHistory_.states.size() > 1)
        {
            // Keep the current state and its next Redo command when trimming a
            // full Redo branch; discard the farthest-future state first.
            stagedCleaveHistory_.states.pop_back();
        }
        else if (stagedCleaveHistory_.gestureBefore)
        {
            stagedCleaveHistory_.gestureBefore.reset();
            break;
        }
        else
        {
            stagedCleaveHistory_.states.clear();
            stagedCleaveHistory_.cursor = 0;
            break;
        }
    }
    if ( stagedCleaveHistory_.states.empty()
        && stagedCleaveHistoryStorageBytes() > kStagedCleaveHistoryMaxBytes )
    {
        stagedCleaveHistory_.gestureBefore.reset();
    }
    stagedCleaveHistory_.ownedBytes = stagedCleaveHistoryStorageBytes();
}

bool GlView::stagedCleaveHistoryOwnsUndo() const noexcept
{
    return isStagedCleaveInteractionActive()
        && (stagedCleaveHistory_.active || stagedCleaveHistory_.gestureActive);
}

bool GlView::isStagedCleaveInteractionActive() const noexcept
{
    return level_ && renderPipeline_ == RenderPipeline::Editor && cleaveStagedMode_
        && ((!stagedCleaveContext_.valid
                && (sectorCleaveToolEnabled_ || surfaceCleaveToolEnabled_))
            || (stagedCleaveContext_.valid
                && (stagedCleaveContext_.surfaceMode
                    ? surfaceCleaveToolEnabled_ : sectorCleaveToolEnabled_)));
}

bool GlView::hasStagedCleaveDraft() const noexcept
{
    return !stagedCleaveLines_.empty() || stagedCleaveContext_.valid
        || stagedCleaveHistory_.active || stagedCleaveHistory_.gestureActive;
}

bool GlView::canUndoStagedCleaveHistory() const
{
    if ( !stagedCleaveHistoryOwnsUndo() )
    {
        return false;
    }
    if ( stagedCleaveHistory_.gestureActive )
    {
        const StagedCleaveDraftState* before = nullptr;
        if ( stagedCleaveHistory_.gestureBefore )
        {
            before = &*stagedCleaveHistory_.gestureBefore;
        }
        else if ( stagedCleaveHistory_.active
            && stagedCleaveHistory_.cursor < stagedCleaveHistory_.states.size() )
        {
            before = &stagedCleaveHistory_.states[stagedCleaveHistory_.cursor];
        }
        if ( !before )
        {
            // Large drafts can exceed the bounded snapshot budget. Keep local
            // command ownership exclusive, but do not report a destructive
            // undo as available when no pre-gesture memento was retained.
            return false;
        }
        if ( stagedCleaveContext_.valid )
        {
            return stagedCleaveTargetsMatchContext(stagedCleaveContext_);
        }
        return !before->context.valid
            || stagedCleaveTargetsMatchContext(before->context);
    }
    if ( !stagedCleaveHistory_.active || stagedCleaveHistory_.cursor == 0 )
    {
        return false;
    }
    if ( !stagedCleaveHistory_.states.empty()
        && stagedCleaveHistory_.states[stagedCleaveHistory_.cursor].context.valid )
    {
        const StagedCleaveContext& context =
            stagedCleaveHistory_.states[stagedCleaveHistory_.cursor].context;
        return stagedCleaveTargetsMatchContext(context);
    }
    const StagedCleaveDraftState& previous =
        stagedCleaveHistory_.states[stagedCleaveHistory_.cursor - 1];
    return !previous.context.valid
        || stagedCleaveTargetsMatchContext(previous.context);
}

bool GlView::canRedoStagedCleaveHistory() const
{
    if ( !stagedCleaveHistoryOwnsUndo()
        || !stagedCleaveHistory_.active
        || stagedCleaveHistory_.gestureActive
        || stagedCleaveHistory_.cursor + 1 >= stagedCleaveHistory_.states.size() )
    {
        return false;
    }
    const StagedCleaveDraftState& current =
        stagedCleaveHistory_.states[stagedCleaveHistory_.cursor];
    if ( current.context.valid )
    {
        return stagedCleaveTargetsMatchContext(current.context);
    }
    const StagedCleaveDraftState& next =
        stagedCleaveHistory_.states[stagedCleaveHistory_.cursor + 1];
    return !next.context.valid || stagedCleaveTargetsMatchContext(next.context);
}

bool GlView::undoStagedCleaveHistory()
{
    if (stagedCleaveWheelSession_.active())
    {
        return cancelStagedCleaveWheelSession();
    }
    if ( stagedCleaveHistory_.gestureActive )
    {
        if ( !canUndoStagedCleaveHistory() )
        {
            return false;
        }
        const bool restored = restoreAndDiscardStagedCleaveHistoryGesture();
        if ( restored )
        {
            // Any successful local Undo invalidates a paired document Redo
            // priority, even when this gesture rollback restores the same
            // authored geometry.
            noteStagedCleaveAuthoredChange();
        }
        return restored;
    }
    if ( !canUndoStagedCleaveHistory() )
    {
        return false;
    }
    const std::size_t destinationIndex = stagedCleaveHistory_.cursor - 1u;
    const StagedCleaveDraftState& destination =
        stagedCleaveHistory_.states[destinationIndex];
    const std::size_t storageBefore = stagedCleaveHistoryStorageBytes();
    const std::size_t destinationBytes = stagedCleaveDraftStateOwnedBytes(destination);
    if ( destination.applyOrder.size() != destination.lines.size()
        || storageBefore > kStagedCleaveHistoryMaxBytes
        || destinationBytes > kStagedCleaveHistoryMaxBytes - storageBefore )
    {
        return false;
    }
    StagedCleaveDraftState prepared{};
    try
    {
        prepared = destination;
    }
    catch (const std::bad_alloc&)
    {
        return false;
    }
    const std::size_t preparedBytes = stagedCleaveDraftStateOwnedBytes(prepared);
    if ( preparedBytes > kStagedCleaveHistoryMaxBytes - storageBefore )
    {
        return false;
    }
    stagedCleaveHistory_.cursor = destinationIndex;
    installStagedCleaveDraftState(std::move(prepared));
    stagedCleaveHistory_.ownedBytes = stagedCleaveHistoryStorageBytes();
    noteStagedCleaveAuthoredChange();
    return true;
}

bool GlView::redoStagedCleaveHistory()
{
    if ( !canRedoStagedCleaveHistory() )
    {
        return false;
    }
    const std::size_t destinationIndex = stagedCleaveHistory_.cursor + 1u;
    const StagedCleaveDraftState& destination =
        stagedCleaveHistory_.states[destinationIndex];
    const std::size_t storageBefore = stagedCleaveHistoryStorageBytes();
    const std::size_t destinationBytes = stagedCleaveDraftStateOwnedBytes(destination);
    if ( destination.applyOrder.size() != destination.lines.size()
        || storageBefore > kStagedCleaveHistoryMaxBytes
        || destinationBytes > kStagedCleaveHistoryMaxBytes - storageBefore )
    {
        return false;
    }
    StagedCleaveDraftState prepared{};
    try
    {
        prepared = destination;
    }
    catch (const std::bad_alloc&)
    {
        return false;
    }
    const std::size_t preparedBytes = stagedCleaveDraftStateOwnedBytes(prepared);
    if ( preparedBytes > kStagedCleaveHistoryMaxBytes - storageBefore )
    {
        return false;
    }
    stagedCleaveHistory_.cursor = destinationIndex;
    installStagedCleaveDraftState(std::move(prepared));
    stagedCleaveHistory_.ownedBytes = stagedCleaveHistoryStorageBytes();
    noteStagedCleaveAuthoredChange();
    return true;
}

void GlView::requestApplyStagedCleave()
{
    (void)finishStagedCleaveWheelSession();
    if (!isStagedCleaveInteractionActive() || stagedCleaveLines_.empty())
    {
        showToast("Multi-cut: add at least one plane", 1800, ToastPlacement::BottomCenter, ToastGroup::Error);
        return;
    }

    const bool surfaceMode = surfaceCleaveToolEnabled_;
    if (!stagedCleaveTargetsMatchCurrent(surfaceMode))
    {
        showToast(
            surfaceMode
                ? "Multi-cut surface: restore the original source and target selection, or Clear"
                : "Multi-cut: restore the original target selection, or Clear",
            3200,
            ToastPlacement::BottomCenter,
            ToastGroup::Error);
        return;
    }

    QList<TVector> normals{};
    QList<TVector> points{};
    QList<TVector> strokeSecondPoints{};
    normals.reserve(static_cast<int>(stagedCleaveLines_.size()));
    points.reserve(static_cast<int>(stagedCleaveLines_.size()));
    strokeSecondPoints.reserve(static_cast<int>(stagedCleaveLines_.size()));
    if ( stagedCleaveApplyOrder_.size() != stagedCleaveLines_.size() )
    {
        showToast(
            "Multi-cut: the staged Apply order is invalid; restore or reorder the lines",
            3200,
            ToastPlacement::BottomCenter,
            ToastGroup::Error);
        return;
    }
    for ( const std::int32_t lineIndex : stagedCleaveApplyOrder_ )
    {
        if ( lineIndex < 0
            || static_cast<std::size_t>(lineIndex) >= stagedCleaveLines_.size() )
        {
            showToast(
                "Multi-cut: the staged Apply order is invalid; restore or reorder the lines",
                3200,
                ToastPlacement::BottomCenter,
                ToastGroup::Error);
            return;
        }
        const StagedCleaveLine& line = stagedCleaveLines_[
            static_cast<std::size_t>(lineIndex)];
        normals.push_back(line.planeNormal);
        points.push_back(line.first);
        strokeSecondPoints.push_back(line.second);
    }

    const bool closedShape = stagedCleaveLinesFormSingleClosedChain();
    const bool hasClosureMarker = stagedCleaveLines_.front().linkedToPrevious
        || stagedCleaveLines_.back().linkedToNext;
    if (hasClosureMarker && !closedShape)
    {
        showToast(
            "Multi-cut: apply a closed chain separately from independent planes",
            3000,
            ToastPlacement::BottomCenter,
            ToastGroup::Error);
        return;
    }

    TVector interiorPoint{};
    for (const StagedCleaveLine& line : stagedCleaveLines_)
    {
        interiorPoint = interiorPoint + (line.first - stagedCleaveLines_.front().first);
    }
    interiorPoint = stagedCleaveLines_.front().first
        + interiorPoint * (1.0 / static_cast<double>(stagedCleaveLines_.size()));

    if (closedShape)
    {
        const QMatrix4x4& projection = stagedCleaveContext_.projection;
        const QMatrix4x4& cameraView = stagedCleaveContext_.view;
        const int projectionWidth = stagedCleaveContext_.viewportWidth;
        const int projectionHeight = stagedCleaveContext_.viewportHeight;
        const QMatrix4x4 mvp = projection * cameraView;
        std::vector<QPointF> polygon{};
        std::vector<render::ClosedCleaveLoopPlane> loopPlanes{};
        polygon.reserve(stagedCleaveLines_.size());
        loopPlanes.reserve(stagedCleaveLines_.size());
        for (const StagedCleaveLine& line : stagedCleaveLines_)
        {
            const ProjectedPoint projected = projectWorldToWidget(
                mvp,
                line.first,
                projectionWidth,
                projectionHeight);
            if (!projected.valid)
            {
                showToast(
                    "Multi-cut: a closed chain must be a simple convex shape",
                    3000,
                    ToastPlacement::BottomCenter,
                    ToastGroup::Error);
                return;
            }
            polygon.push_back(projected.pos);
            loopPlanes.push_back(render::ClosedCleaveLoopPlane{ line.planeNormal, line.first });
        }

        const render::ClosedCleaveLoopValidation validation =
            render::validateClosedCleaveLoop(polygon, loopPlanes, interiorPoint);
        if (!validation.valid())
        {
            showToast(
                "Multi-cut: a closed chain must be a simple convex shape",
                3000,
                ToastPlacement::BottomCenter,
                ToastGroup::Error);
            return;
        }
    }

    if (sectorCleaveToolEnabled_)
    {
        Q_EMIT sectorsCleaveBatchRequested(
            normals,
            points,
            closedShape,
            interiorPoint,
            strokeSecondPoints,
            cleaveBounded_);
    }
    else if (surfaceCleaveToolEnabled_)
    {
        Q_EMIT surfacesCleaveBatchRequested(
            normals,
            points,
            closedShape,
            interiorPoint,
            strokeSecondPoints,
            cleaveBounded_);
    }
}

void GlView::suspendStagedCleaveInteraction()
{
    (void)finishStagedCleaveWheelSession();
    ++stagedCleaveJointGuidePreviewRevision_;
    if ( stagedCleaveOrderEditor_ )
    {
        finishStagedCleaveOrderEdit(false, true);
    }
    if ( stagedCleaveHistory_.gestureActive )
    {
        (void)restoreAndDiscardStagedCleaveHistoryGesture();
    }
    stagedCleavePlaneDrag_ = {};
    stagedCleaveTransformDrag_ = {};
    clearStagedCleaveTransformHud();
    stagedCleaveMarqueeSelection_ = {};
    stagedCleaveShapePreviewCenter_.reset();
    stagedCleaveShapePreviewFrame_.reset();
    stagedCleaveShapePlacementClickPending_ = false;
    editingStagedCleaveLine_ = -1;
    editingStagedCleaveEndpoint_ = -1;
    editingDisjointStagedCleaveEndpoint_ = false;
    editingStagedCleaveEndpointPreservePlacement_ = false;
    stagedCleaveEndpointOriginClickCandidate_ = false;
    stagedCleaveLineClickCandidate_ = false;
    stagedCleaveDragSelectionApplied_ = false;
    stagedCleaveJointCycleClickCandidate_ = false;
    stagedCleaveJointCycleCandidates_.clear();
    sectorCleaveHaveFirstPoint_ = false;
    surfaceCleaveHaveFirstPoint_ = false;
    cleaveSnapMarker_.reset();
    cleaveSnapGuidesVisible_ = false;
    leftDragged_ = false;
}

void GlView::validateRetainedStagedCleaveReferences()
{
    const auto validSector = [this](const std::int32_t sector)
    {
        return level_ && sector >= 0
            && static_cast<std::size_t>(sector) < level_->sectors.size();
    };
    const auto validSurface = [this, &validSector](const SurfaceRef& surface)
    {
        return validSector(surface.sector) && surface.surface >= 0
            && static_cast<std::size_t>(surface.surface)
                < level_->sectors[static_cast<std::size_t>(surface.sector)].surfaces.size();
    };
    const auto validContext = [&validSector, &validSurface](const auto& context)
    {
        return (!context.surfaceMode || validSurface(context.sourceSurface))
            && std::all_of(context.targetSectors.begin(), context.targetSectors.end(), validSector)
            && std::all_of(context.targetSurfaces.begin(), context.targetSurfaces.end(), validSurface);
    };
    const bool invalidCurrent = !validContext(stagedCleaveContext_)
        || !std::all_of(stagedCleaveHistory_.states.begin(), stagedCleaveHistory_.states.end(),
            [&validContext](const auto& snapshot) { return validContext(snapshot.context); });
    const bool invalidPeer = stagedCleavePeerWorkspace_
        && (!validContext(stagedCleavePeerWorkspace_->draft.context)
            || (stagedCleavePeerWorkspace_->history
                && !std::all_of(stagedCleavePeerWorkspace_->history->snapshots.begin(),
                    stagedCleavePeerWorkspace_->history->snapshots.end(),
                    [&validContext](const auto& snapshot) { return validContext(snapshot.context); })));
    if ( !invalidCurrent && !invalidPeer ) return;
    {
        const QSignalBlocker settledPublication(this);
        if ( invalidCurrent ) cancelStagedCleave();
        if ( invalidPeer )
        {
            cancelStagedCleaveWorkspace(cleaveWorkspaceFamily_ == CleaveWorkspaceFamily::Sector
                ? CleaveWorkspaceFamily::Surface : CleaveWorkspaceFamily::Sector);
        }
    }
    Q_EMIT stagedCleaveAuthoredStateChanged(stagedCleaveAuthoredRevision_);
    Q_EMIT viewportStateChanged();
    showToast("Multi-cut: its source or target was removed; the pending draft was cleared",
        3200, ToastPlacement::BottomCenter, ToastGroup::Error);
}

void GlView::cancelStagedCleave()
{
    ++stagedCleaveJointGuidePreviewRevision_;
    const bool authoredDraftWasPresent = !stagedCleaveLines_.empty()
        || stagedCleaveChainOrigin_.has_value()
        || stagedCleaveContext_.valid;
    const bool hadStagedWorkspace = !stagedCleaveLines_.empty()
        || stagedCleaveChainOrigin_.has_value()
        || stagedCleaveContext_.valid
        || stagedCleaveHistoryOwnsUndo();
    std::vector<StagedCleaveLine>{}.swap(stagedCleaveLines_);
    stagedCleaveContext_ = {};
    stagedCleavePlaneDrag_ = {};
    stagedCleaveChainOrigin_.reset();
    resetStagedCleaveHistory();
    std::vector<std::int32_t>{}.swap(stagedCleaveApplyOrder_);
    std::vector<std::int32_t>{}.swap(selectedStagedCleaveLines_);
    selectedStagedCleaveLine_ = -1;
    editingStagedCleaveLine_ = -1;
    editingStagedCleaveEndpoint_ = -1;
    editingDisjointStagedCleaveEndpoint_ = false;
    editingStagedCleaveEndpointPreservePlacement_ = false;
    stagedCleaveEndpointOriginClickCandidate_ = false;
    stagedCleaveLineClickCandidate_ = false;
    stagedCleaveDragSelectionApplied_ = false;
    stagedCleaveJointCycleClickCandidate_ = false;
    stagedCleaveJointCycleCandidates_.clear();
    stagedCleaveTransformDrag_ = {};
    clearStagedCleaveTransformHud();
    stagedCleaveMarqueeSelection_ = {};
    stagedCleaveRankCacheValid_ = false;
    if ( stagedCleaveOrderEditor_ )
    {
        stagedCleaveOrderEditor_->hide();
        stagedCleaveOrderEditor_->deleteLater();
        stagedCleaveOrderEditor_.clear();
    }
    sectorCleaveHaveFirstPoint_ = false;
    surfaceCleaveHaveFirstPoint_ = false;
    cleaveSnapMarker_.reset();
    update();
    if ( hadStagedWorkspace )
    {
        if ( authoredDraftWasPresent )
        {
            noteStagedCleaveAuthoredChange();
        }
        Q_EMIT viewportStateChanged();
    }
}

bool GlView::cancelActiveStagedCleaveInteraction()
{
    if (stagedCleaveWheelSession_.active())
    {
        return cancelStagedCleaveWheelSession();
    }
    if ( stagedCleaveOrderEditor_ )
    {
        finishStagedCleaveOrderEdit(false, true);
        return true;
    }
    if ( stagedCleaveHistory_.gestureActive )
    {
        return restoreAndDiscardStagedCleaveHistoryGesture();
    }
    if ( cleaveShapeOptions_.placementArmed
        || stagedCleaveShapePlacementPlan_ )
    {
        RecoveryState::CleaveShapeOptions options = cleaveShapeOptions_;
        options.placementArmed = false;
        stagedCleaveShapePlacementPlan_.reset();
        stagedCleaveShapePreviewCenter_.reset();
        stagedCleaveShapePreviewFrame_.reset();
        stagedCleaveShapePlacementClickPending_ = false;
        setCleaveShapeOptions(options);
        return true;
    }
    return false;
}

void GlView::clearStagedCleaveDraft()
{
    (void)finishStagedCleaveWheelSession();
    const bool recordClear = beginStagedCleaveHistoryGesture();
    if ( !recordClear )
    {
        showToast(
            "Multi-cut: draft is too large for local history; Apply it or cancel the tool to discard it",
            3200,
            ToastPlacement::BottomCenter,
            ToastGroup::Error);
        return;
    }
    const bool changed = !stagedCleaveLines_.empty()
        || stagedCleaveChainOrigin_.has_value()
        || stagedCleaveContext_.valid;
    stagedCleaveLines_.clear();
    stagedCleaveApplyOrder_.clear();
    selectedStagedCleaveLines_.clear();
    stagedCleaveContext_ = {};
    stagedCleavePlaneDrag_ = {};
    stagedCleaveChainOrigin_.reset();
    selectedStagedCleaveLine_ = -1;
    editingStagedCleaveLine_ = -1;
    editingStagedCleaveEndpoint_ = -1;
    editingDisjointStagedCleaveEndpoint_ = false;
    editingStagedCleaveEndpointPreservePlacement_ = false;
    stagedCleaveEndpointOriginClickCandidate_ = false;
    stagedCleaveLineClickCandidate_ = false;
    stagedCleaveDragSelectionApplied_ = false;
    stagedCleaveJointCycleClickCandidate_ = false;
    stagedCleaveJointCycleCandidates_.clear();
    sectorCleaveHaveFirstPoint_ = false;
    surfaceCleaveHaveFirstPoint_ = false;
    cleaveSnapMarker_.reset();
    if ( changed )
    {
        if ( recordClear )
        {
            (void)finishStagedCleaveHistoryGesture();
        }
        Q_EMIT viewportStateChanged();
    }
    else
    {
        discardStagedCleaveHistoryGesture();
    }
    update();
}

bool GlView::deleteSelectedStagedCleaveLine()
{
    if ( !isStagedCleaveInteractionActive() || selectedStagedCleaveLines_.empty() )
    {
        return false;
    }
    (void)finishStagedCleaveWheelSession();

    constexpr std::size_t kMaximum = std::numeric_limits<std::size_t>::max();
    const std::size_t oldCount = stagedCleaveLines_.size();
    const std::size_t currentBytes = stagedCleaveDraftStateCurrentOwnedBytesEstimate();
    const std::size_t storageBefore = stagedCleaveHistoryStorageBytes();
    const std::size_t reserveBytes = stagedCleaveHistory_.states.capacity()
            < kStagedCleaveHistoryMaxSnapshots
        ? kStagedCleaveHistoryMaxSnapshots * sizeof(StagedCleaveDraftState)
        : 0u;
    std::size_t candidateBytes = 0u;
    const auto addCandidateBytes = [&candidateBytes](
                                      const std::size_t count,
                                      const std::size_t elementSize)
    {
        constexpr std::size_t kLimit = std::numeric_limits<std::size_t>::max();
        if ( elementSize != 0u && count > kLimit / elementSize )
        {
            candidateBytes = kLimit;
            return;
        }
        const std::size_t bytes = count * elementSize;
        candidateBytes = bytes > kLimit - candidateBytes
            ? kLimit
            : candidateBytes + bytes;
    };
    // The packed removal bitmap is word-rounded by vector<bool>; charge a
    // little extra for its last storage word so the preflight does not depend
    // on the standard library's bit-block width.
    addCandidateBytes(oldCount / 8u + (oldCount % 8u != 0u ? 1u : 0u), 1u);
    addCandidateBytes(sizeof(std::size_t), 1u);
    addCandidateBytes(oldCount, sizeof(std::int32_t));
    // Reserve the full old line/order sizes before remapping. The committed
    // vectors may be smaller, but this upper bound covers both the temporary
    // candidate and the finish snapshot while the rollback remains retained.
    addCandidateBytes(oldCount, sizeof(StagedCleaveLine));
    addCandidateBytes(stagedCleaveApplyOrder_.size(), sizeof(std::int32_t));
    addCandidateBytes(selectedStagedCleaveLines_.size(), sizeof(std::int32_t));
    // finishStagedCleaveHistoryGesture snapshots the unchanged target context
    // too, so charge that copy alongside the remap candidates.
    addCandidateBytes(
        stagedCleaveContext_.targetSectors.capacity(),
        sizeof(std::int32_t));
    addCandidateBytes(
        stagedCleaveContext_.targetSurfaces.capacity(),
        sizeof(SurfaceRef));
    bool exceedsBudget = storageBefore > kStagedCleaveHistoryMaxBytes
        || reserveBytes > kStagedCleaveHistoryMaxBytes - storageBefore;
    if ( !exceedsBudget )
    {
        const std::size_t afterReserve = storageBefore + reserveBytes;
        exceedsBudget = currentBytes > kStagedCleaveHistoryMaxBytes - afterReserve;
        if ( !exceedsBudget )
        {
            const std::size_t afterBaseline = afterReserve + currentBytes;
            exceedsBudget = candidateBytes > kStagedCleaveHistoryMaxBytes - afterBaseline;
        }
    }
    if ( exceedsBudget )
    {
        showToast(
            "Multi-cut: local history limit reached; Clear or Apply before deleting lines",
            3200,
            ToastPlacement::BottomCenter,
            ToastGroup::Error);
        return false;
    }

    bool gestureStarted = false;
    try
    {
        gestureStarted = beginStagedCleaveHistoryGesture();
    }
    catch (const std::bad_alloc&)
    {
        showToast(
            "Multi-cut: local history could not keep the line-removal baseline",
            3200,
            ToastPlacement::BottomCenter,
            ToastGroup::Error);
        return false;
    }
    if ( !gestureStarted )
    {
        showToast(
            "Multi-cut: local history limit reached; Clear or Apply before editing this draft",
            3200,
            ToastPlacement::BottomCenter,
            ToastGroup::Error);
        return false;
    }

    std::vector<bool> removed{};
    std::vector<std::int32_t> oldToNew{};
    std::vector<StagedCleaveLine> nextLines{};
    std::vector<std::int32_t> nextOrder{};
    std::vector<std::int32_t> nextSelection{};
    try
    {
        removed.assign(oldCount, false);
        for ( const std::int32_t line : selectedStagedCleaveLines_ )
        {
            if ( line >= 0 && static_cast<std::size_t>(line) < oldCount )
            {
                removed[static_cast<std::size_t>(line)] = true;
            }
        }
        if ( std::none_of(
                removed.begin(),
                removed.end(),
                [](const bool value) { return value; }) )
        {
            discardStagedCleaveHistoryGesture();
            return false;
        }

        oldToNew.assign(oldCount, -1);
        const std::size_t removedCount = static_cast<std::size_t>(std::count(
            removed.begin(),
            removed.end(),
            true));
        nextLines.reserve(oldCount - removedCount);
        for ( std::size_t oldIndex = 0; oldIndex < oldCount; ++oldIndex )
        {
            if ( removed[oldIndex] )
            {
                continue;
            }
            oldToNew[oldIndex] = static_cast<std::int32_t>(nextLines.size());
            nextLines.push_back(stagedCleaveLines_[oldIndex]);
        }

        for ( std::size_t oldIndex = 0; oldIndex < oldCount; ++oldIndex )
        {
            if ( removed[oldIndex] || oldCount < 2 )
            {
                continue;
            }
            StagedCleaveLine& survivor = nextLines[
                static_cast<std::size_t>(oldToNew[oldIndex])];
            const std::size_t previous = oldIndex == 0 ? oldCount - 1 : oldIndex - 1;
            const std::size_t next = (oldIndex + 1) % oldCount;
            if ( removed[previous] )
            {
                survivor.linkedToPrevious = false;
            }
            if ( removed[next] )
            {
                survivor.linkedToNext = false;
            }
        }

        nextOrder.reserve(stagedCleaveApplyOrder_.size());
        for ( const std::int32_t line : stagedCleaveApplyOrder_ )
        {
            if ( line < 0 || static_cast<std::size_t>(line) >= oldCount
                || removed[static_cast<std::size_t>(line)] )
            {
                continue;
            }
            nextOrder.push_back(oldToNew[static_cast<std::size_t>(line)]);
        }

        nextSelection.reserve(selectedStagedCleaveLines_.size());
        for ( const std::int32_t line : selectedStagedCleaveLines_ )
        {
            if ( line < 0 || static_cast<std::size_t>(line) >= oldCount
                || removed[static_cast<std::size_t>(line)] )
            {
                continue;
            }
            nextSelection.push_back(oldToNew[static_cast<std::size_t>(line)]);
        }
    }
    catch (const std::bad_alloc&)
    {
        discardStagedCleaveHistoryGesture();
        showToast(
            "Multi-cut: local history could not prepare the line-removal transaction",
            3200,
            ToastPlacement::BottomCenter,
            ToastGroup::Error);
        return false;
    }

    std::optional<StagedCleaveChainOrigin> nextOrigin = stagedCleaveChainOrigin_;
    if ( nextOrigin )
    {
        const std::int32_t oldOrigin = nextOrigin->lineIndex;
        if ( oldOrigin < 0 || static_cast<std::size_t>(oldOrigin) >= oldCount
            || removed[static_cast<std::size_t>(oldOrigin)] )
        {
            nextOrigin.reset();
        }
        else
        {
            nextOrigin->lineIndex = oldToNew[static_cast<std::size_t>(oldOrigin)];
        }
    }

    stagedCleaveLines_ = std::move(nextLines);
    stagedCleaveApplyOrder_ = std::move(nextOrder);
    stagedCleaveChainOrigin_ = nextOrigin;
    stagedCleaveRankCacheValid_ = false;
    if ( stagedCleaveLines_.empty() )
    {
        stagedCleaveContext_ = {};
        stagedCleaveChainOrigin_.reset();
    }
    const std::int32_t previousActive = selectedStagedCleaveLine_;
    const std::int32_t nextActive = previousActive >= 0
            && static_cast<std::size_t>(previousActive) < oldToNew.size()
            && oldToNew[static_cast<std::size_t>(previousActive)] >= 0
        ? oldToNew[static_cast<std::size_t>(previousActive)]
        : nextSelection.empty() ? -1 : nextSelection.front();
    setSelectedStagedCleaveLines(std::move(nextSelection), nextActive);
    editingStagedCleaveLine_ = -1;
    editingStagedCleaveEndpoint_ = -1;
    editingDisjointStagedCleaveEndpoint_ = false;
    editingStagedCleaveEndpointPreservePlacement_ = false;
    stagedCleaveEndpointOriginClickCandidate_ = false;
    update();
    (void)finishStagedCleaveHistoryGesture();
    Q_EMIT viewportStateChanged();
    return true;
}

void GlView::setSectorThrustToolEnabled(const bool enabled)
{
    if (sectorThrustToolEnabled_ == enabled)
    {
        return;
    }

    sectorThrustToolEnabled_ = enabled;
    hoveredSectorThrust_ = -1;
    sectorThrustDrag_ = {};
    sectorThrustLabels_.clear();
    if (!enabled)
    {
        unsetCursor();
    }
    update();
}

void GlView::setShowAllSectorThrustGizmos(const bool enabled)
{
    if (showAllSectorThrustGizmos_ == enabled)
    {
        return;
    }

    showAllSectorThrustGizmos_ = enabled;
    hoveredSectorThrust_ = -1;
    update();
}

void GlView::setShowSectorThrustVectors(const bool enabled)
{
    if (showSectorThrustVectors_ == enabled)
    {
        return;
    }

    showSectorThrustVectors_ = enabled;
    if (!sectorThrustToolEnabled_)
    {
        sectorThrustLabels_.clear();
    }
    update();
}

void GlView::setSurfaceCleaveToolEnabled(const bool enabled)
{
    if (surfaceCleaveToolEnabled_ == enabled)
    {
        return;
    }

    if ( !enabled )
    {
        suspendStagedCleaveInteraction();
    }
    else
    {
        selectStagedCleaveWorkspace(CleaveWorkspaceFamily::Surface);
        sectorCleaveToolEnabled_ = false;
    }
    surfaceCleaveToolEnabled_ = enabled;
    surfaceCleaveHaveFirstPoint_ = false;
    surfaceCleaveFirstPoint_ = {};
    surfaceCleaveCurrentPoint_ = {};
    cleaveSnapMarker_.reset();
    if (!enabled)
    {
        cleaveSnapGuidesVisible_ = false;
    }
    else
    {
        updateCleaveSnapGuideVisibility(QGuiApplication::keyboardModifiers());
    }
    editingStagedCleaveLine_ = -1;
    editingStagedCleaveEndpoint_ = -1;
    editingDisjointStagedCleaveEndpoint_ = false;
    editingStagedCleaveEndpointPreservePlacement_ = false;
    stagedCleaveEndpointOriginClickCandidate_ = false;
    update();
    Q_EMIT viewportStateChanged();
}

void GlView::setShowThingGizmos(const bool enabled)
{
    if (showThingGizmos_ == enabled)
    {
        return;
    }

    showThingGizmos_ = enabled;
    thingTypeGizmoMetadataValid_ = false;
    thingTypeGizmoCacheValid_ = false;
    architecturalLightingOwnerGizmoCacheValid_ = false;
    architecturalLightingOwnerGizmoVertices_.clear();
    if (renderPipeline_ == RenderPipeline::Editor)
    {
        editorState_.showThingGizmos = enabled;
    }
    else
    {
        previewState_.showThingGizmos = enabled;
    }
    markViewChanged();
    Q_EMIT viewportStateChanged();
    update();
}

void GlView::setEditorDefaultShowThingGizmos(const bool enabled)
{
    editorState_.showThingGizmos = enabled;

    if (renderPipeline_ == RenderPipeline::Editor)
    {
        showThingGizmos_ = enabled;
        thingTypeGizmoMetadataValid_ = false;
        thingTypeGizmoCacheValid_ = false;
        architecturalLightingOwnerGizmoCacheValid_ = false;
        architecturalLightingOwnerGizmoVertices_.clear();
        markViewChanged();
        Q_EMIT viewportStateChanged();
        update();
        return;
    }

    Q_EMIT viewportStateChanged();
}

void GlView::setShowArchThingGizmos(const bool enabled)
{
    if (showArchThingGizmos_ == enabled)
    {
        return;
    }

    showArchThingGizmos_ = enabled;
    if (renderPipeline_ == RenderPipeline::Editor)
    {
        editorState_.showArchThingGizmos = enabled;
    }
    else
    {
        previewState_.showArchThingGizmos = enabled;
    }
    thingTypeGizmoMetadataValid_ = false;
    thingTypeGizmoCacheValid_ = false;
    architecturalLightingOwnerGizmoCacheValid_ = false;
    architecturalLightingOwnerGizmoVertices_.clear();
    markViewChanged();
    Q_EMIT viewportStateChanged();
    update();
}

void GlView::setEditorDefaultShowArchThingGizmos(const bool enabled)
{
    editorState_.showArchThingGizmos = enabled;
    if (renderPipeline_ == RenderPipeline::Editor)
    {
        showArchThingGizmos_ = enabled;
        thingTypeGizmoMetadataValid_ = false;
        thingTypeGizmoCacheValid_ = false;
        markViewChanged();
        Q_EMIT viewportStateChanged();
        update();
        return;
    }
    Q_EMIT viewportStateChanged();
}

void GlView::setPreviewDefaultShowArchThingGizmos(const bool enabled)
{
    previewDefaultShowArchThingGizmos_ = enabled;
    previewState_.showArchThingGizmos = enabled;
    if (renderPipeline_ == RenderPipeline::GamePreview)
    {
        showArchThingGizmos_ = enabled;
        thingTypeGizmoMetadataValid_ = false;
        thingTypeGizmoCacheValid_ = false;
        Q_EMIT viewportStateChanged();
        markViewChanged();
    }
    update();
}

void GlView::setThingIconCubeTintColor(const QColor& color)
{
    QColor c = color;
    if (!c.isValid())
    {
        c = Qt::white;
    }

    if (thingIconCubeTintColor_ == c)
    {
        return;
    }

    thingIconCubeTintColor_ = c;

    // Thing icon textures depend on this color; regenerate them without a full mesh rebuild.
    if (!textures_.empty())
    {
        if (glReady_)
        {
            makeCurrent();
        }

        for (auto it = textures_.begin(); it != textures_.end();)
        {
            if (std::string_view(it->first).rfind(kThingIconMatPrefix, 0) == 0)
            {
                it = textures_.erase(it);
            }
            else
            {
                ++it;
            }
        }

        if (glReady_)
        {
            doneCurrent();
        }
    }

    markViewChanged();
    update();
}

void GlView::setBackgroundClearColor(const QColor& color)
{
    QColor c = color;
    if (!c.isValid())
    {
        c = QColor::fromRgbF(0.08, 0.09, 0.12, 1.0);
    }
    c.setAlpha(255);

    if (backgroundClearColor_ == c)
    {
        return;
    }

    backgroundClearColor_ = c;
    update();
}

void GlView::setWireframeColor(const QColor& color)
{
    QColor c = color;
    if (!c.isValid())
    {
        c = QColor::fromRgbF(0.86, 0.87, 0.92, 1.0);
    }
    c.setAlpha(255);

    if (wireframeColor_ == c)
    {
        return;
    }

    wireframeColor_ = c;
    update();
}

void GlView::setWireframeLineWidthPx(const float widthPx)
{
    const float clamped = std::clamp(widthPx, 0.5f, 6.0f);
    if (std::abs(wireframeLineWidthPx_ - clamped) <= 0.0001f)
    {
        return;
    }

    wireframeLineWidthPx_ = clamped;
    update();
}

void GlView::setWireframeAntialiasingEnabled(const bool enabled)
{
    if (wireframeAntialiasingEnabled_ == enabled)
    {
        return;
    }

    wireframeAntialiasingEnabled_ = enabled;
    update();
}

void GlView::setFramePathLineWorldSize(const float size)
{
    const float clamped = std::clamp(size, 0.001f, 1.0f);
    if (std::abs(framePathLineWorldSize_ - clamped) <= 0.0001f)
    {
        return;
    }

    framePathLineWorldSize_ = clamped;
    update();
}

void GlView::setFramePathPointWorldSize(const float size)
{
    const float clamped = std::clamp(size, 0.005f, 2.0f);
    if (std::abs(framePathPointWorldSize_ - clamped) <= 0.0001f)
    {
        return;
    }

    framePathPointWorldSize_ = clamped;
    update();
}

void GlView::setSelectionPrimaryColor(const QColor& color)
{
    QColor c = color;
    if (!c.isValid())
    {
        c = QColor::fromRgbF(0.98, 0.80, 0.25, 1.0);
    }
    c.setAlpha(255);

    if (selectionPrimaryColor_ == c)
    {
        return;
    }

    selectionPrimaryColor_ = c;
    update();
}

void GlView::setSelectionMultiColor(const QColor& color)
{
    QColor c = color;
    if (!c.isValid())
    {
        c = QColor::fromRgbF(0.20, 0.72, 0.95, 1.0);
    }
    c.setAlpha(255);

    if (selectionMultiColor_ == c)
    {
        return;
    }

    selectionMultiColor_ = c;
    update();
}

void GlView::setSelectionEdgeMultiColor(const QColor& color)
{
    QColor c = color;
    if (!c.isValid())
    {
        c = QColor::fromRgb(255, 152, 56);
    }
    c.setAlpha(255);

    if (selectionEdgeMultiColor_ == c)
    {
        return;
    }

    selectionEdgeMultiColor_ = c;
    update();
}

void GlView::setSelectionActiveColor(const QColor& color)
{
    QColor c = color;
    if (!c.isValid())
    {
        c = QColor::fromRgbF(0.95, 0.20, 0.20, 1.0);
    }
    c.setAlpha(255);

    if (selectionActiveColor_ == c)
    {
        return;
    }

    selectionActiveColor_ = c;
    update();
}

void GlView::setSelectionVertexColor(const QColor& color)
{
    QColor c = color;
    if (!c.isValid())
    {
        c = QColor::fromRgbF(0.98, 0.80, 0.25, 1.0);
    }
    c.setAlpha(255);

    if (selectionVertexColor_ == c)
    {
        return;
    }

    selectionVertexColor_ = c;
    update();
}

void GlView::setSelectionVertexMultiColor(const QColor& color)
{
    QColor c = color;
    if (!c.isValid())
    {
        c = QColor::fromRgbF(0.20, 0.72, 0.95, 1.0);
    }
    c.setAlpha(255);

    if (selectionVertexMultiColor_ == c)
    {
        return;
    }

    selectionVertexMultiColor_ = c;
    update();
}

void GlView::setCogEntityPickPrimaryColor(const QColor& color)
{
    QColor c = color;
    if (!c.isValid())
    {
        c = QColor::fromRgbF(0.20, 0.86, 0.96, 1.0);
    }
    c.setAlpha(255);

    if (cogEntityPickPrimaryColor_ == c)
    {
        return;
    }

    cogEntityPickPrimaryColor_ = c;
    update();
}

void GlView::setCogEntityPickActiveColor(const QColor& color)
{
    QColor c = color;
    if (!c.isValid())
    {
        c = QColor::fromRgbF(0.98, 0.66, 0.20, 1.0);
    }
    c.setAlpha(255);

    if (cogEntityPickActiveColor_ == c)
    {
        return;
    }

    cogEntityPickActiveColor_ = c;
    update();
}

void GlView::setCogEntityPickHighlightEnabled(const bool enabled)
{
    if (cogEntityPickHighlightEnabled_ == enabled)
    {
        return;
    }

    cogEntityPickHighlightEnabled_ = enabled;
    update();
}

void GlView::setActiveSurfaceVertexHint(
    const std::int32_t sectorIndex,
    const std::int32_t surfaceIndex,
    const std::int32_t localVertexIndex)
{
    if (sectorIndex < 0 || surfaceIndex < 0 || localVertexIndex < 0)
    {
        clearActiveSurfaceVertexHint();
        return;
    }

    const editor::SurfaceVertexRef next{ SurfaceRef{ sectorIndex, surfaceIndex }, localVertexIndex };
    if (activeSurfaceVertexHintValid_ && activeSurfaceVertexHint_ == next)
    {
        return;
    }

    activeSurfaceVertexHintValid_ = true;
    activeSurfaceVertexHint_ = next;

    rebuildSelectionOverlays();
    if (glReady_)
    {
        makeCurrent();
        uploadSelectionOverlays();
        doneCurrent();
    }

    update();
}

void GlView::clearActiveSurfaceVertexHint()
{
    if (!activeSurfaceVertexHintValid_)
    {
        return;
    }

    activeSurfaceVertexHintValid_ = false;
    activeSurfaceVertexHint_ = {};

    rebuildSelectionOverlays();
    if (glReady_)
    {
        makeCurrent();
        uploadSelectionOverlays();
        doneCurrent();
    }

    update();
}

void GlView::setThingCubeHalfSize(const float halfSize)
{
    const float clamped = std::clamp(halfSize, 0.001f, 0.05f);
    if (std::abs(thingCubeHalfSize_ - clamped) <= 1.0e-6f)
    {
        return;
    }

    thingCubeHalfSize_ = clamped;

    const QVector3D oldTarget = camera_.orbit.target();
    const float oldYawDeg = camera_.orbit.yawDeg;
    const float oldPitchDeg = camera_.orbit.pitchDeg;
    const float oldDistance = camera_.orbit.distance;

    rebuildMeshes();

    camera_.orbit.yawDeg = oldYawDeg;
    camera_.orbit.pitchDeg = oldPitchDeg;
    camera_.orbit.distance = oldDistance;
    camera_.orbit.pan = oldTarget - camera_.orbit.center;
    camera_.orbit.distance = std::max(camera_.orbit.distance, camera_.orbit.radius * 0.05f);

    if (glReady_)
    {
        makeCurrent();
        uploadMeshes();
        doneCurrent();
    }
    update();
}

void GlView::setShowEditorCubes(const bool enabled)
{
    if (showEditorCubes_ == enabled)
    {
        return;
    }

    showEditorCubes_ = enabled;
    if (renderPipeline_ == RenderPipeline::Editor)
    {
        const QVector3D oldTarget = camera_.orbit.target();
        const float oldYawDeg = camera_.orbit.yawDeg;
        const float oldPitchDeg = camera_.orbit.pitchDeg;
        const float oldDistance = camera_.orbit.distance;

        rebuildMeshes();

        camera_.orbit.yawDeg = oldYawDeg;
        camera_.orbit.pitchDeg = oldPitchDeg;
        camera_.orbit.distance = oldDistance;
        camera_.orbit.pan = oldTarget - camera_.orbit.center;
        camera_.orbit.distance = std::max(camera_.orbit.distance, camera_.orbit.radius * 0.05f);

        if (glReady_)
        {
            makeCurrent();
            uploadMeshes();
            doneCurrent();
        }
    }

    markViewChanged();
    Q_EMIT viewportStateChanged();
    update();
}

void GlView::setShowPreviewCubes(const bool enabled)
{
    if (showPreviewCubes_ == enabled)
    {
        return;
    }

    showPreviewCubes_ = enabled;
    if (renderPipeline_ == RenderPipeline::GamePreview)
    {
        const QVector3D oldTarget = camera_.orbit.target();
        const float oldYawDeg = camera_.orbit.yawDeg;
        const float oldPitchDeg = camera_.orbit.pitchDeg;
        const float oldDistance = camera_.orbit.distance;

        rebuildMeshes();

        camera_.orbit.yawDeg = oldYawDeg;
        camera_.orbit.pitchDeg = oldPitchDeg;
        camera_.orbit.distance = oldDistance;
        camera_.orbit.pan = oldTarget - camera_.orbit.center;
        camera_.orbit.distance = std::max(camera_.orbit.distance, camera_.orbit.radius * 0.05f);

        if (glReady_)
        {
            makeCurrent();
            uploadMeshes();
            doneCurrent();
        }
    }

    markViewChanged();
    Q_EMIT viewportStateChanged();
    update();
}

void GlView::setPreviewDefaultShowThingGizmos(const bool enabled)
{
    previewDefaultShowThingGizmos_ = enabled;
    previewState_.showThingGizmos = enabled;

    if (renderPipeline_ == RenderPipeline::GamePreview)
    {
        showThingGizmos_ = enabled;
        thingTypeGizmoMetadataValid_ = false;
        thingTypeGizmoCacheValid_ = false;
        Q_EMIT viewportStateChanged();
    }

    update();
}

void GlView::setThingTypeColors(const std::unordered_map<std::string, QColor>& colors)
{
    if (thingTypeColors_ == colors)
    {
        return;
    }

    thingTypeColors_ = colors;

    // Gizmo textures are cached per type; clear them so they regenerate with new colors.
    // Non-visual thing icon textures also depend on these colors.
    if (glReady_)
    {
        makeCurrent();
    }

    gizmoTextures_.clear();

    for (auto it = textures_.begin(); it != textures_.end();)
    {
        if (std::string_view(it->first).rfind(kThingIconMatPrefix, 0) == 0)
        {
            it = textures_.erase(it);
        }
        else
        {
            ++it;
        }
    }

    if (glReady_)
    {
        doneCurrent();
    }

    markViewChanged();
    Q_EMIT viewportStateChanged();
    update();
}

void GlView::setLayerColors(const std::vector<QColor>& colors)
{
    std::vector<QColor> normalized = colors;
    for (QColor& c : normalized)
    {
        if (c.isValid())
        {
            c.setAlpha(255);
        }
    }

    if (layerColors_ == normalized)
    {
        return;
    }

    layerColors_ = std::move(normalized);
    surfaceWireColorTextureDirty_ = true;
    update();
}

void GlView::setLayerWireframeColoringEnabled(const bool enabled)
{
    if (layerWireframeColoringEnabled_ == enabled)
    {
        return;
    }

    layerWireframeColoringEnabled_ = enabled;
    update();
}

GlView::ThingLodView GlView::thingLodView() const noexcept
{
    return ThingLodView{
        camera_.orbit.target(),
        app::ViewportCamera::BasisFromOrbit(
            camera_.orbit.yawDeg,
            camera_.orbit.pitchDeg,
            camera_.orbit.rollDeg).forward,
        static_cast<double>(camera_.orbit.distance),
    };
}

std::size_t GlView::selectedThingModelLod(
    const std::size_t geosetCount,
    const Thing& thing,
    const ThingLodView& view) const noexcept
{
    if (!thingModelLodEnabled_ || !level_
        || level_->kind == ProjectType::IJIM || geosetCount <= 1u
        || !std::isfinite(thing.x)
        || !std::isfinite(thing.y)
        || !std::isfinite(thing.z))
    {
        return 0u;
    }

    // Sith's transformedPos.y is depth along the camera forward axis.
    // The editor's orbit rig places its eye distance units behind target.
    // MotS gameplay zoomScale is not represented by this viewport camera;
    // its effective inverse zoom is one here. Orthographic projection uses
    // the same forward depth; an owner behind the eye remains at HQ.
    const double forwardDepth =
        (thing.x - static_cast<double>(view.target.x()))
            * static_cast<double>(view.forward.x())
        + (thing.y - static_cast<double>(view.target.y()))
            * static_cast<double>(view.forward.y())
        + (thing.z - static_cast<double>(view.target.z()))
            * static_cast<double>(view.forward.z())
        + view.eyeDistance;
    return assets::selectModel3doLodIndex(
        geosetCount,
        level_->header.lodDistances,
        forwardDepth);
}

bool GlView::refreshThingModelLodForView()
{
    const bool havePreview =
        placementPreviewRenderStateValid_
        && !placementPreviewLodStates_.empty()
        && placementPreviewLodStates_.size()
            == placementPreviewRenderedThings_.size();
    if (!thingModelLodEnabled_ || !level_
        || level_->kind == ProjectType::IJIM
        || !thingMeshCache_.valid
        || ((!showThings_ || multiLodThingIndices_.empty())
            && !havePreview))
    {
        return false;
    }

    const std::array<double, 4>& distances = level_->header.lodDistances;
    if (thingLodCheckedViewSerial_ == viewSerial_
        && thingLodHeaderChecked_
        && thingLodCheckedHeaderDistances_ == distances)
    {
        return false;
    }

    const ThingLodView lodView = thingLodView();
    bool changed = false;
    if (showThings_)
    {
        for (const std::int32_t thingIndex : multiLodThingIndices_)
        {
            if (thingIndex < 0
                || static_cast<std::size_t>(thingIndex)
                    >= thingRenderState_.size()
                || static_cast<std::size_t>(thingIndex)
                    >= level_->things.size())
            {
                continue;
            }
            const ThingRenderState& state =
                thingRenderState_[static_cast<std::size_t>(thingIndex)];
            if (selectedThingModelLod(
                    state.geosetCount,
                    level_->things[static_cast<std::size_t>(thingIndex)],
                    lodView) != state.requestedLodIndex)
            {
                changed = true;
                break;
            }
        }
    }

    if (!changed && havePreview)
    {
        for (std::size_t i = 0;
             i < placementPreviewLodStates_.size(); ++i)
        {
            const PlacementPreviewLodState& state =
                placementPreviewLodStates_[i];
            if (state.geosetCount > 1u && !state.archThing
                && selectedThingModelLod(
                    state.geosetCount,
                    placementPreviewRenderedThings_[i],
                    lodView) != state.requestedLodIndex)
            {
                changed = true;
                break;
            }
        }
    }

    if (changed)
    {
        const QVector3D oldTarget = camera_.orbit.target();
        const float oldYawDeg = camera_.orbit.yawDeg;
        const float oldPitchDeg = camera_.orbit.pitchDeg;
        const float oldRollDeg = camera_.orbit.rollDeg;
        const float oldDistance = camera_.orbit.distance;
        rebuildMeshes(false, MeshRebuildKind::ThingsOnly);
        camera_.orbit.yawDeg = oldYawDeg;
        camera_.orbit.pitchDeg = oldPitchDeg;
        camera_.orbit.rollDeg = oldRollDeg;
        camera_.orbit.distance = oldDistance;
        camera_.orbit.pan = oldTarget - camera_.orbit.center;
        camera_.orbit.distance =
            std::max(camera_.orbit.distance, camera_.orbit.radius * 0.05f);
        // Called from paintGL with this widget's context current. Re-upload
        // before deriving pass booleans or starting any draw pass.
        if (glReady_)
        {
            uploadMeshes();
        }
    }

    thingLodCheckedViewSerial_ = viewSerial_;
    thingLodCheckedHeaderDistances_ = distances;
    thingLodHeaderChecked_ = true;
    return changed;
}

void GlView::setThingModelLodEnabled(const bool enabled)
{
    if (thingModelLodEnabled_ == enabled)
    {
        return;
    }

    thingModelLodEnabled_ = enabled;
    ViewportState& activeState = renderPipeline_ == RenderPipeline::Editor
        ? editorState_ : previewState_;
    activeState.thingModelLodEnabled = enabled;
    thingLodHeaderChecked_ = false;
    thingLodCheckedViewSerial_ = std::numeric_limits<std::uint64_t>::max();

    if (level_ && level_->kind != ProjectType::IJIM)
    {
        const QVector3D oldTarget = camera_.orbit.target();
        const float oldYawDeg = camera_.orbit.yawDeg;
        const float oldPitchDeg = camera_.orbit.pitchDeg;
        const float oldRollDeg = camera_.orbit.rollDeg;
        const float oldDistance = camera_.orbit.distance;
        rebuildMeshes(false, MeshRebuildKind::ThingsOnly);
        camera_.orbit.yawDeg = oldYawDeg;
        camera_.orbit.pitchDeg = oldPitchDeg;
        camera_.orbit.rollDeg = oldRollDeg;
        camera_.orbit.distance = oldDistance;
        camera_.orbit.pan = oldTarget - camera_.orbit.center;
        camera_.orbit.distance =
            std::max(camera_.orbit.distance, camera_.orbit.radius * 0.05f);
        if (glReady_)
        {
            makeCurrent();
            uploadMeshes();
            doneCurrent();
        }
    }
    // Keep the HQ source bytes and parsed lower variants across a temporary
    // toggle-off. Re-reading only the lower tier on the next toggle could mix
    // a cached old HQ model with a newer file revision.
    markViewChanged();
    Q_EMIT viewportStateChanged();
    update();
}

void GlView::setDefaultThingModelLodEnabled(
    const RenderPipeline pipeline,
    const bool enabled)
{
    if (pipeline == RenderPipeline::Editor)
    {
        editorDefaultThingModelLodEnabled_ = enabled;
        editorState_.thingModelLodEnabled = enabled;
    }
    else
    {
        previewDefaultThingModelLodEnabled_ = enabled;
        previewState_.thingModelLodEnabled = enabled;
    }
    if (renderPipeline_ == pipeline)
    {
        setThingModelLodEnabled(enabled);
    }
}

void GlView::setThingViewMode(const ThingViewMode mode)
{
    const QVector3D oldTarget = camera_.orbit.target();
    const float oldYawDeg = camera_.orbit.yawDeg;
    const float oldPitchDeg = camera_.orbit.pitchDeg;
    const float oldDistance = camera_.orbit.distance;

    thingViewMode_ = mode;
    rebuildMeshes();
    camera_.orbit.yawDeg = oldYawDeg;
    camera_.orbit.pitchDeg = oldPitchDeg;
    camera_.orbit.distance = oldDistance;
    camera_.orbit.pan = oldTarget - camera_.orbit.center;
    camera_.orbit.distance = std::max(camera_.orbit.distance, camera_.orbit.radius * 0.05f);

    if (glReady_)
    {
        makeCurrent();
        uploadMeshes();
        doneCurrent();
    }

    markViewChanged();
    Q_EMIT viewportStateChanged();
    update();
}

void GlView::setLightingMode(const LightingMode mode)
{
    if (lightingMode_ == mode)
    {
        return;
    }

    const QVector3D oldTarget = camera_.orbit.target();
    const float oldYawDeg = camera_.orbit.yawDeg;
    const float oldPitchDeg = camera_.orbit.pitchDeg;
    const float oldDistance = camera_.orbit.distance;

    lightingMode_ = mode;
    rebuildMeshes();

    camera_.orbit.yawDeg = oldYawDeg;
    camera_.orbit.pitchDeg = oldPitchDeg;
    camera_.orbit.distance = oldDistance;
    camera_.orbit.pan = oldTarget - camera_.orbit.center;
    camera_.orbit.distance = std::max(camera_.orbit.distance, camera_.orbit.radius * 0.05f);

    if (glReady_)
    {
        makeCurrent();
        uploadMeshes();
        doneCurrent();
    }
    markViewChanged();
    Q_EMIT viewportStateChanged();
    update();
}

void GlView::setDynamicLightShading(const DynamicLightShading shading)
{
    if (dynamicLightShading_ == shading)
    {
        return;
    }

    dynamicLightShading_ = shading;
    update();
}

void GlView::setThingLightRenderingEnabled(const bool enabled)
{
    if ( thingLightRenderingEnabled_ == enabled )
    {
        return;
    }

    thingLightRenderingEnabled_ = enabled;
    if ( level_ )
    {
        // A runtime Thing-light toggle changes only rendered Thing colors and
        // the bounded scene-light collection. Keep level geometry, placements,
        // cameras, and spatial indexes intact.
        notifyThingAppearanceChanged();
        Q_EMIT viewportStateChanged();
        return;
    }

    markViewChanged();
    Q_EMIT viewportStateChanged();
    update();
}

void GlView::setSectorPointLightRenderingEnabled(const bool enabled)
{
    if ( sectorPointLightRenderingEnabled_ == enabled )
    {
        return;
    }

    sectorPointLightRenderingEnabled_ = enabled;
    Q_EMIT viewportStateChanged();
    update();
}

bool GlView::isLayerVisible(const std::int32_t layer) const noexcept
{
    if (hiddenLayers_.find(layer) == hiddenLayers_.end())
    {
        return true;
    }

    // Game preview keeps rendering all layers by default. An explicit preference can make it
    // respect the editor's hidden-layer state.
    if (renderPipeline_ == RenderPipeline::GamePreview && !previewRespectHiddenLayers_)
    {
        return true;
    }

    return false;
}

bool GlView::isLayerLocked(const std::int32_t layer) const noexcept
{
    return lockedLayers_.contains(layer);
}

void GlView::setLayerVisibilityState(
    std::unordered_set<std::int32_t> hiddenLayers,
    const bool previewRespectHiddenLayers,
    const LayerMeshPublication publication,
    LayerMeshProgressPulse progressPulse)
{
    hiddenLayers.erase(-1);

    const bool hiddenChanged = hiddenLayers_ != hiddenLayers;
    const bool previewPolicyChanged =
        previewRespectHiddenLayers_ != previewRespectHiddenLayers;
    if (!hiddenChanged && !previewPolicyChanged)
    {
        return;
    }

    hiddenLayers_ = std::move(hiddenLayers);
    previewRespectHiddenLayers_ = previewRespectHiddenLayers;

    // Rebuild the small ARCH owner cache before publishing the new layer
    // state. Its layer guard prunes only hidden owners, preserving valid
    // selections belonging to unrelated visible layers while the existing
    // selection-change signal cancels any invalid modeless editor.
    if (hiddenChanged)
    {
        refreshArchitecturalLightingRenderCache();
    }

    const bool previewPipeline =
        renderPipeline_ == RenderPipeline::GamePreview;
    const bool hiddenLayersAffectCurrentPipeline =
        !previewPipeline || previewRespectHiddenLayers_;
    const bool meshVisibilityChanged =
        (hiddenChanged && hiddenLayersAffectCurrentPipeline)
        || (previewPolicyChanged && previewPipeline);
    if (!meshVisibilityChanged)
    {
        return;
    }

    if (publication == LayerMeshPublication::defer)
    {
        // Open/recovery publish a new Level immediately after this state.
        // Retain no false promise that the old cache matches the staged
        // visibility; the caller performs one authoritative setLevel/notify
        // rebuild before the next event pump.
        levelMeshCache_.valid = false;
        thingMeshCache_.valid = false;
        spriteTrianglesDirty_ = true;
        levelPeelCacheValid_ = false;
        thingPeelCacheValid_ = false;
        surfacePickIndex_ = sed::level::SurfacePickIndex{};
        sectorContainmentIndex_ = sed::level::SectorContainmentIndex{};
        return;
    }

    const QVector3D oldTarget = camera_.orbit.target();
    const float oldYawDeg = camera_.orbit.yawDeg;
    const float oldPitchDeg = camera_.orbit.pitchDeg;
    const float oldDistance = camera_.orbit.distance;

    // Hidden/unhidden owners can retain their stable pick IDs. Avoiding a
    // global pick-map reset removes a second O(document) pass while the full
    // visibility-filtered geometry cache is rebuilt.
    rebuildMeshes(false, MeshRebuildKind::All, progressPulse);

    camera_.orbit.yawDeg = oldYawDeg;
    camera_.orbit.pitchDeg = oldPitchDeg;
    camera_.orbit.distance = oldDistance;
    camera_.orbit.pan = oldTarget - camera_.orbit.center;
    camera_.orbit.distance = std::max(camera_.orbit.distance, camera_.orbit.radius * 0.05f);

    if (progressPulse)
    {
        progressPulse();
    }
    if (glReady_)
    {
        makeCurrent();
        uploadMeshes(progressPulse);
        doneCurrent();
    }
    if (progressPulse)
    {
        progressPulse();
    }

    markViewChanged();
    Q_EMIT viewportStateChanged();
    update();
}

void GlView::setHiddenLayers(
    std::unordered_set<std::int32_t> hiddenLayers)
{
    setLayerVisibilityState(
        std::move(hiddenLayers),
        previewRespectHiddenLayers_);
}

void GlView::setLockedLayers(std::unordered_set<std::int32_t> lockedLayers)
{
    lockedLayers.erase(-1);
    if (lockedLayers_ == lockedLayers)
    {
        return;
    }

    lockedLayers_ = std::move(lockedLayers);
    // The ARCH owner cache prunes only locked owners, preserving valid
    // selections belonging to unrelated visible layers. Its selection-change
    // signal invalidates any modeless editor targeting a locked owner.
    refreshArchitecturalLightingRenderCache();
    // A cache refresh can remove the last selected vertex without emitting a
    // selection signal. Rebuild the point payload explicitly so an owner
    // marker/VBO cannot survive after its layer becomes locked, while points
    // belonging to unrelated visible owners remain available.
    rebuildArchitecturalLightingPointVertices();
    if (glReady_)
    {
        ScopedArchitecturalOpenGlContext context(*this);
        if (context.ready())
        {
            uploadArchitecturalLightingPointVertices();
        }
    }
    if (level_)
    {
        selectedSectorThrusts_.clear();
        selectedSectorThrusts_.reserve(highlightedSelection_.sectors.size());
        for (const std::int32_t sectorIndex : highlightedSelection_.sectors)
        {
            if (sectorIndex < 0 || static_cast<std::size_t>(sectorIndex) >= level_->sectors.size()
                || isLayerLocked(level_->sectors[static_cast<std::size_t>(sectorIndex)].layer))
            {
                continue;
            }
            selectedSectorThrusts_.insert(sectorIndex);
        }
    }
    if (sectorThrustDrag_.active)
    {
        sectorThrustDrag_ = {};
        unsetCursor();
    }
    update();
}

void GlView::setPreviewRespectHiddenLayers(const bool enabled)
{
    setLayerVisibilityState(hiddenLayers_, enabled);
}

void GlView::setGeometryTransformSnapEnabled(const bool enabled)
{
    geometryTransformSnapEnabled_ = enabled;
}

void GlView::setGeometryTransformRotateToNearbyEnabled(const bool enabled)
{
    geometryTransformRotateToNearbyEnabled_ = enabled;
}

void GlView::setPreserveConnectedSurfaceUvsEnabled(const bool enabled)
{
    preserveConnectedSurfaceUvsEnabled_ = enabled;
    if ( !enabled )
    {
        restoreTransformSurfaceUvPreview();
    }
}

void GlView::setGeometryTransformSnapInvertModifierBinding(
    const Qt::KeyboardModifiers toggle)
{
    setGeometryTransformSnapInvertModifierBinding(
        ui::shortcuts::ModifierBindings{toggle});
}

void GlView::setGeometryTransformSnapInvertModifierBinding(
    const ui::shortcuts::ModifierBindings& toggles)
{
    geometryTransformSnapInvertModifiers_ =
        ui::shortcuts::normalizedModifierBindings(toggles);
}

void GlView::setTransformGridSnapEnabled(const bool enabled)
{
    transformGridSnapEnabled_ = enabled;
}

void GlView::setTransformGridSnapInvertModifierBinding(const Qt::KeyboardModifiers toggle)
{
    setTransformGridSnapInvertModifierBinding(
        ui::shortcuts::ModifierBindings{toggle});
}

void GlView::setTransformGridSnapInvertModifierBinding(
    const ui::shortcuts::ModifierBindings& toggles)
{
    transformGridSnapInvertModifiers_ =
        ui::shortcuts::normalizedModifierBindings(toggles);
}

void GlView::setTransformGridStickInvertModifierBinding(const Qt::KeyboardModifiers toggle)
{
    setTransformGridStickInvertModifierBinding(
        ui::shortcuts::ModifierBindings{toggle});
}

void GlView::setTransformGridStickInvertModifierBinding(
    const ui::shortcuts::ModifierBindings& toggles)
{
    transformGridStickInvertModifiers_ =
        ui::shortcuts::normalizedModifierBindings(toggles);
}

void GlView::setTransformGridPlaneStickEnabled(const bool enabled)
{
    transformGridPlaneStickEnabled_ = enabled;
    if (!enabled)
    {
        transformDrag_.gridPlaneStick = {};
    }
}

void GlView::setSectorMoveSnapAlignEnabled(const bool enabled)
{
    sectorMoveSnapAlignEnabled_ = enabled;
}

void GlView::setSectorMoveSnapAlignInvertModifierBinding(const Qt::KeyboardModifiers toggle)
{
    setSectorMoveSnapAlignInvertModifierBinding(
        ui::shortcuts::ModifierBindings{toggle});
}

void GlView::setSectorMoveSnapAlignInvertModifierBinding(
    const ui::shortcuts::ModifierBindings& toggles)
{
    sectorMoveSnapAlignInvertModifiers_ =
        ui::shortcuts::normalizedModifierBindings(toggles);
}

void GlView::setThingMoveSurfaceStickEnabled(const bool enabled)
{
    thingMoveSurfaceStickEnabled_ = enabled;
}

void GlView::setThingMoveSurfaceAlignEnabled(const bool enabled)
{
    thingMoveSurfaceAlignEnabled_ = enabled;
}

void GlView::setThingMoveSurfaceStickInvertModifierBinding(const Qt::KeyboardModifiers toggle)
{
    setThingMoveSurfaceStickInvertModifierBinding(
        ui::shortcuts::ModifierBindings{toggle});
}

void GlView::setThingMoveSurfaceStickInvertModifierBinding(
    const ui::shortcuts::ModifierBindings& toggles)
{
    thingMoveSurfaceStickInvertModifiers_ =
        ui::shortcuts::normalizedModifierBindings(toggles);
}

void GlView::setThingMoveSurfaceAlignInvertModifierBinding(const Qt::KeyboardModifiers toggle)
{
    setThingMoveSurfaceAlignInvertModifierBinding(
        ui::shortcuts::ModifierBindings{toggle});
}

void GlView::setThingMoveSurfaceAlignInvertModifierBinding(
    const ui::shortcuts::ModifierBindings& toggles)
{
    thingMoveSurfaceAlignInvertModifiers_ =
        ui::shortcuts::normalizedModifierBindings(toggles);
}

void GlView::setThingMoveSnapToThingEnabled(const bool enabled)
{
    thingMoveSnapToThingEnabled_ = enabled;
}

void GlView::setThingMoveSnapInvertModifierBinding(const Qt::KeyboardModifiers toggle)
{
    setThingMoveSnapInvertModifierBinding(
        ui::shortcuts::ModifierBindings{toggle});
}

void GlView::setThingMoveSnapInvertModifierBinding(
    const ui::shortcuts::ModifierBindings& toggles)
{
    thingMoveSnapInvertModifiers_ =
        ui::shortcuts::normalizedModifierBindings(toggles);
}

void GlView::setThingMoveSnapRotateEnabled(const bool enabled)
{
    thingMoveSnapRotateEnabled_ = enabled;
}

void GlView::setThingMoveSnapRotateInvertModifierBinding(const Qt::KeyboardModifiers toggle)
{
    setThingMoveSnapRotateInvertModifierBinding(
        ui::shortcuts::ModifierBindings{toggle});
}

void GlView::setThingMoveSnapRotateInvertModifierBinding(
    const ui::shortcuts::ModifierBindings& toggles)
{
    thingMoveSnapRotateInvertModifiers_ =
        ui::shortcuts::normalizedModifierBindings(toggles);
}

void GlView::setRenderPipeline(const RenderPipeline pipeline)
{
    if (renderPipeline_ == pipeline)
    {
        return;
    }

    suspendStagedCleaveInteraction();

    // Save current state into the outgoing slot.
    auto saveState = [&](ViewportState& state)
    {
        state.orbit = camera_.orbit;
        state.projectionType = camera_.projectionType();
        state.fovXDeg = camera_.fovXDeg();
        state.orthoScale = camera_.orthoScale();
        state.clipEnabled = camera_.clipEnabled();
        state.showWireframe = showWireframe_;
        state.showVertices = showVertices_;
        state.showSolid = showSolid_;
        state.showTextures = showTextures_;
        state.showFog = showFog_;
        state.showThings = showThings_;
        state.thingModelLodEnabled = thingModelLodEnabled_;
        state.showHiddenThings = showHiddenThings_;
        state.showThingGizmos = showThingGizmos_;
        state.showArchThingGizmos = showArchThingGizmos_;
        state.showThingCollisionShapes = showThingCollisionShapes_;
        state.showThingVisualBounds = showThingVisualBounds_;
        state.thingViewMode = thingViewMode_;
        state.lightingMode = lightingMode_;
        state.initialized = true;
    };

    auto restoreState = [&](const ViewportState& state)
    {
        camera_.orbit = state.orbit;
        camera_.SetProjectionType(state.projectionType);
        camera_.SetFovXDeg(state.fovXDeg);
        camera_.SetOrthoScale(state.orthoScale);
        camera_.SetClipEnabled(state.clipEnabled);
        showWireframe_ = state.showWireframe;
        showVertices_ = state.showVertices;
        showSolid_ = state.showSolid;
        showTextures_ = state.showTextures;
        showFog_ = state.showFog;
        showThings_ = state.showThings;
        thingModelLodEnabled_ = state.thingModelLodEnabled;
        showHiddenThings_ = state.showHiddenThings;
        showThingGizmos_ = state.showThingGizmos;
        showArchThingGizmos_ = state.showArchThingGizmos;
        showThingCollisionShapes_ = state.showThingCollisionShapes;
        showThingVisualBounds_ = state.showThingVisualBounds;
        thingOverlayGeometryPrepared_ =
            showThingCollisionShapes_ || showThingVisualBounds_;
        thingViewMode_ = state.thingViewMode;
        lightingMode_ = state.lightingMode;
    };

    if (renderPipeline_ == RenderPipeline::Editor)
    {
        saveState(editorState_);
    }
    else
    {
        saveState(previewState_);
    }

    renderPipeline_ = pipeline;
    alphaAdjoinWireSupportsDirty_ = true;
    thingTypeGizmoMetadataValid_ = false;
    thingTypeGizmoCacheValid_ = false;
    architecturalLightingLastOwnerClick_.reset();
    if (renderPipeline_ == RenderPipeline::GamePreview)
    {
        // Preview supports point selection only. A pipeline switch can arrive
        // between an Editor mouse press and release, so discard the marquee
        // transaction here as well as guarding its input/dispatch paths.
        boxSelectActive_ = false;
        boxSelectVisible_ = false;
        boxSelectFromTransformTool_ = false;
    }
    resetPickCycle();

    ViewportState& incoming = (pipeline == RenderPipeline::Editor) ? editorState_ : previewState_;

    if (incoming.initialized)
    {
        restoreState(incoming);
    }
    else
    {
        // First-time initialization for this pipeline.
        if (pipeline == RenderPipeline::GamePreview)
        {
            // Default preview state: perspective, textured, shaded things, lit.
            camera_.SetProjectionType(app::ViewportCamera::ProjectionType::Perspective);
            showWireframe_ = false;
            showVertices_ = false;
            showSolid_ = true;
            showTextures_ = true;
            showFog_ = previewDefaultShowFog_;
            showThings_ = true;
            thingModelLodEnabled_ = previewDefaultThingModelLodEnabled_;
            showHiddenThings_ = false;
            showThingGizmos_ = previewDefaultShowThingGizmos_;
            showArchThingGizmos_ = previewDefaultShowArchThingGizmos_;
            showThingCollisionShapes_ = previewDefaultShowThingCollisionShapes_;
            showThingVisualBounds_ = previewDefaultShowThingVisualBounds_;
            thingViewMode_ = ThingViewMode::Shaded;
            lightingMode_ = LightingMode::Gouraud;

            // Position camera at the first player thing; fall back to the first thing; otherwise origin.
            const Thing* spawnThing = nullptr;
            if (level_ && !level_->things.empty())
            {
                for (const Thing& thing : level_->things)
                {
                    const TplValue* typeVal = sed::level::thingFindValue(*level_, templates_, thing, "type");
                    const std::string_view typeName =
                        (typeVal && !typeVal->stringValue.empty()) ? std::string_view(typeVal->stringValue) : std::string_view{};
                    if (typeName == "player")
                    {
                        spawnThing = &thing;
                        break;
                    }
                }

                if (!spawnThing)
                {
                    spawnThing = &level_->things.front();
                }
            }

            const QVector3D spawnPos = spawnThing
                ? QVector3D(static_cast<float>(spawnThing->x), static_cast<float>(spawnThing->y), static_cast<float>(spawnThing->z))
                : QVector3D(0.0f, 0.0f, 0.0f);
            const float spawnYawDeg = spawnThing ? static_cast<float>(spawnThing->yaw) : 0.0f;

            camera_.orbit.center = spawnPos;
            camera_.orbit.pan = QVector3D(0.0f, 0.0f, 0.0f);
            camera_.orbit.yawDeg = spawnYawDeg;
            camera_.orbit.pitchDeg = 10.0f;
            camera_.orbit.distance = 0.2f;
        }
        else
        {
            camera_.SetProjectionType(app::ViewportCamera::ProjectionType::Orthographic);
            showFog_ = editorDefaultShowFog_;
            thingModelLodEnabled_ = editorDefaultThingModelLodEnabled_;
            showThingGizmos_ = editorState_.showThingGizmos;
            showArchThingGizmos_ = editorState_.showArchThingGizmos;
            showThingCollisionShapes_ = editorDefaultShowThingCollisionShapes_;
            showThingVisualBounds_ = editorDefaultShowThingVisualBounds_;
        }
    }

    const QVector3D oldTarget = camera_.orbit.target();
    const float oldYawDeg = camera_.orbit.yawDeg;
    const float oldPitchDeg = camera_.orbit.pitchDeg;
    const float oldDistance = camera_.orbit.distance;

    rebuildMeshes();

    camera_.orbit.yawDeg = oldYawDeg;
    camera_.orbit.pitchDeg = oldPitchDeg;
    camera_.orbit.distance = oldDistance;
    camera_.orbit.pan = oldTarget - camera_.orbit.center;
    camera_.orbit.distance = std::max(camera_.orbit.distance, camera_.orbit.radius * 0.05f);

    if (glReady_)
    {
        makeCurrent();
        uploadMeshes();
        doneCurrent();
    }
    markViewChanged();
    Q_EMIT viewportStateChanged();
    update();
}

GlView::RecoveryState GlView::recoveryState() const
{
    RecoveryState result{};
    result.editor = editorState_;
    result.preview = previewState_;
    result.pipeline = renderPipeline_;

    ViewportState active{};
    active.orbit = camera_.orbit;
    active.projectionType = camera_.projectionType();
    active.fovXDeg = camera_.fovXDeg();
    active.orthoScale = camera_.orthoScale();
    active.clipEnabled = camera_.clipEnabled();
    active.showWireframe = showWireframe_;
    active.showVertices = showVertices_;
    active.showSolid = showSolid_;
    active.showTextures = showTextures_;
    active.showFog = showFog_;
    active.showThings = showThings_;
    active.thingModelLodEnabled = thingModelLodEnabled_;
    active.showHiddenThings = showHiddenThings_;
    active.showThingGizmos = showThingGizmos_;
    active.showArchThingGizmos = showArchThingGizmos_;
    active.showThingCollisionShapes = showThingCollisionShapes_;
    active.showThingVisualBounds = showThingVisualBounds_;
    active.thingViewMode = thingViewMode_;
    active.lightingMode = lightingMode_;
    active.initialized = true;
    if (renderPipeline_ == RenderPipeline::Editor)
    {
        result.editor = active;
    }
    else
    {
        result.preview = active;
    }

    result.nearPlane = camera_.nearPlane();
    result.farPlane = camera_.farPlane();
    result.showGrid = showGrid_;
    result.gridFollowCamera = gridFollowCamera_;
    result.gridRotateWithView = gridRotateWithView_;
    result.gridKeepViewAlignedVisible = gridKeepViewAlignedVisible_;
    result.gridPlane = gridPlane_;
    result.gridMajorSpacing = gridMajorSpacing_;
    result.gridMinorSpacing = gridMinorSpacing_;
    result.gridAdaptiveSpacing = gridAdaptiveSpacing_;
    result.gridOpacity = gridOpacity_;
    result.gridColor = gridColor_;
    result.gridMajorColor = gridMajorColor_;
    result.gridAxisXColor = gridAxisXColor_;
    result.gridAxisYColor = gridAxisYColor_;
    result.gridAxisZColor = gridAxisZColor_;
    result.gridLineWidthPx = gridLineWidthPx_;
    result.gridMinorDots = gridMinorDots_;
    result.gridMinorDotSizePx = gridMinorDotSizePx_;
    result.gridVisibleThroughWalls = gridVisibleThroughWalls_;
    result.gridAlwaysBehind = gridAlwaysBehind_;
    result.gridResetOnCameraRotate = gridResetOnCameraRotate_;
    result.gridAlignToView = gridAlignToView_;
    result.gridCenterOverride = gridCenterOverride_;
    result.gridAlignAxisA = gridAlignAxisA_;
    result.gridAlignAxisB = gridAlignAxisB_;
    result.gridOverrideCamBasisValid = gridOverrideCamBasisValid_;
    result.gridOverrideCamForward = gridOverrideCamForward_;
    result.gridOverrideCamUp = gridOverrideCamUp_;
    result.dynamicLightShading = dynamicLightShading_;
    result.thingLightRenderingEnabled = thingLightRenderingEnabled_;
    result.sectorPointLightRenderingEnabled = sectorPointLightRenderingEnabled_;
    result.showEditorCubes = showEditorCubes_;
    result.showPreviewCubes = showPreviewCubes_;
    result.previewRespectHiddenLayers = previewRespectHiddenLayers_;
    result.xraySelectionCycling = xraySelectionCyclingEnabled_;
    result.orbitAroundSelection = orbitAroundSelectionEnabled_;
    result.showSectorThrustVectors = showSectorThrustVectors_;
    result.alwaysShowLightGizmoCategories = alwaysShowLightGizmoCategories_;
    result.transformTool = transformTool_;
    result.transformOrientation = transformOrientation_;
    result.transformGridSnap = transformGridSnapEnabled_;
    result.transformGridPlaneStick = transformGridPlaneStickEnabled_;
    result.cleaveStagedMode = cleaveStagedMode_;
    result.cleaveChainEnabled = cleaveChainEnabled_;
    result.cleaveBounded = cleaveBounded_;
    result.cleaveMeasurementsVisible = cleaveMeasurementsVisible_;
    result.cleaveMeasureUnit = cleaveMeasureUnit_;
    result.cleaveAnglesVisible = cleaveAnglesVisible_;
    result.cleaveLengthsVisible = cleaveLengthsVisible_;
    result.cleaveLineNumbersVisible = cleaveLineNumbersVisible_;
    result.cleaveAngleStepDegrees = cleaveAngleStepDegrees_;
    result.sectorCleaveToolEnabled = sectorCleaveToolEnabled_;
    result.surfaceCleaveToolEnabled = surfaceCleaveToolEnabled_;
    RecoveryState::CleaveDraftSnapshot stagedDraft =
        captureStagedCleaveDraftState();
    result.cleaveLines = std::move(stagedDraft.lines);
    result.cleaveContext = std::move(stagedDraft.context);
    result.cleaveChainOrigin = stagedDraft.chainOrigin;
    result.applyOrder = std::move(stagedDraft.applyOrder);
    result.selectedCleaveLines = std::move(stagedDraft.selectedCleaveLines);
    result.selectedCleaveLine = stagedDraft.selectedCleaveLine;
    result.cleaveHistory = captureStagedCleaveHistory();
    result.cleaveShapeOptions = cleaveShapeOptions_;
    result.cleaveTransformMode = stagedCleaveTransformMode_;
    result.cleaveWorkspaceFamily = cleaveWorkspaceFamily_;
    result.cleavePeerWorkspace = stagedCleavePeerWorkspace_;
    result.architecturalLighting = pendingArchitecturalLightingRecovery_
        ? pendingArchitecturalLightingRecovery_
        : std::optional<RecoveryState::ArchitecturalLightingState>{
            RecoveryState::ArchitecturalLightingState{
                architecturalLightingAuthoringMode_,
                architecturalLightingTargetMode_,
                architecturalLightingSelectedVertices_,
                architecturalLightingSelectedVerticesImplicitOwnerExpansion_}};
    return result;
}

bool GlView::canRestoreRecoveryState(
    const RecoveryState& state,
    QString* const error) const
{
    if (error)
    {
        error->clear();
    }
    const std::size_t sourceBytes = cleaveRecoveryStateOwnedArrayBytes(state);
    const std::size_t liveBytes = stagedCleaveHistoryStorageBytes();
    const std::size_t preparedBytes = cleaveRecoveryStatePreparedArrayBytes(state);
    const std::size_t lineOrderAllowance = addCleaveRecoveryOwnedArrayBytes(
        addCleaveRecoveryOwnedArrayBytes(
            0u, state.cleaveLines.size(), sizeof(std::int32_t)),
        1u, sizeof(std::int32_t));
    const std::size_t budget = kStagedCleaveHistoryMaxBytes;
    if (sourceBytes > budget || liveBytes > budget - sourceBytes
        || preparedBytes > budget - sourceBytes - liveBytes
        || lineOrderAllowance > budget - sourceBytes - liveBytes - preparedBytes)
    {
        if (error)
        {
            *error = QStringLiteral(
                "The recovered Cleave workspace exceeds its 16 MiB restore budget; the level was still recovered.");
        }
        return false;
    }
    return true;
}

bool GlView::restoreRecoveryState(
    const RecoveryState& state,
    const bool rebuildRenderCache,
    QString* const error)
{
    if (error)
    {
        error->clear();
    }
    if ( !canRestoreRecoveryState(state, error) )
    {
#ifdef SED_APP_MAIN_WINDOW_TEST_ACCESS
        if ( cleaveRecoveryRestoreOwnedArrayTrackingForTest_ )
        {
            cleaveRecoveryRestoreSourceOwnedArrayBytesForTest_ = cleaveRecoveryStateOwnedArrayBytes(state);
            cleaveRecoveryRestorePeakOwnedArrayBytesForTest_ = std::max(
                cleaveRecoveryRestorePeakOwnedArrayBytesForTest_,
                addCleaveRecoveryOwnedArrayBytes(cleaveRecoveryRestoreSourceOwnedArrayBytesForTest_,
                    cleaveRecoveryLiveOwnedArraysForTest(), 1u));
        }
#endif
        return false;
    }
    std::optional<RecoveryState::CleaveWorkspace> preparedPeer = state.cleavePeerWorkspace;
    std::optional<RecoveryState::CleaveDraftSnapshot> preparedCleaveDraft{};
    std::optional<RecoveryState::CleaveHistory> preparedCleaveHistory{};
    const bool hasRecoveredHistory = state.cleaveHistory
        && !state.cleaveHistory->snapshots.empty()
        && state.cleaveHistory->snapshots.size() <= kStagedCleaveHistoryMaxSnapshots
        && state.cleaveHistory->cursor < state.cleaveHistory->snapshots.size();
    {
        const std::size_t sourceBytes = cleaveRecoveryStateOwnedArrayBytes(state);
        const std::size_t liveBytes = stagedCleaveHistoryStorageBytes();
        const std::size_t budget = kStagedCleaveHistoryMaxBytes;
        if (!canRestoreRecoveryState(state, error))
        {
#ifdef SED_APP_MAIN_WINDOW_TEST_ACCESS
            if (cleaveRecoveryRestoreOwnedArrayTrackingForTest_)
            {
                cleaveRecoveryRestoreSourceOwnedArrayBytesForTest_ = sourceBytes;
                cleaveRecoveryRestorePeakOwnedArrayBytesForTest_ = std::max(
                    cleaveRecoveryRestorePeakOwnedArrayBytesForTest_,
                    addCleaveRecoveryOwnedArrayBytes(
                        sourceBytes,
                        cleaveRecoveryLiveOwnedArraysForTest(),
                        1u));
            }
#endif
            return false;
        }

        RecoveryState::CleaveDraftSnapshot current{};
        current.lines = state.cleaveLines;
        current.context = state.cleaveContext;
        current.chainOrigin = state.cleaveChainOrigin
                && state.cleaveChainOrigin->lineIndex >= 0
                && static_cast<std::size_t>(state.cleaveChainOrigin->lineIndex)
                    < state.cleaveLines.size()
                && (state.cleaveChainOrigin->endpoint == 0
                    || state.cleaveChainOrigin->endpoint == 1)
            ? state.cleaveChainOrigin
            : std::nullopt;
        current.applyOrder = state.applyOrder;
        bool validApplyOrder = current.applyOrder.size() == current.lines.size();
        if (validApplyOrder)
        {
            std::vector<bool> seen(current.lines.size(), false);
            for (const std::int32_t line : current.applyOrder)
            {
                if (line < 0
                    || static_cast<std::size_t>(line) >= seen.size()
                    || seen[static_cast<std::size_t>(line)])
                {
                    validApplyOrder = false;
                    break;
                }
                seen[static_cast<std::size_t>(line)] = true;
            }
        }
        if (!validApplyOrder)
        {
            current.applyOrder.resize(current.lines.size());
            std::iota(current.applyOrder.begin(), current.applyOrder.end(), 0);
        }
        current.selectedCleaveLines = state.selectedCleaveLines;
        current.selectedCleaveLines.erase(
            std::remove_if(
                current.selectedCleaveLines.begin(),
                current.selectedCleaveLines.end(),
                [&current](const std::int32_t line)
                {
                    return line < 0
                        || static_cast<std::size_t>(line) >= current.lines.size();
                }),
            current.selectedCleaveLines.end());
        std::sort(
            current.selectedCleaveLines.begin(), current.selectedCleaveLines.end());
        current.selectedCleaveLines.erase(
            std::unique(
                current.selectedCleaveLines.begin(), current.selectedCleaveLines.end()),
            current.selectedCleaveLines.end());
        if (current.selectedCleaveLines.empty()
            && state.selectedCleaveLine >= 0
            && static_cast<std::size_t>(state.selectedCleaveLine)
                < current.lines.size())
        {
            current.selectedCleaveLines.push_back(state.selectedCleaveLine);
        }
        current.selectedCleaveLine = state.selectedCleaveLine >= 0
                && std::binary_search(
                    current.selectedCleaveLines.begin(),
                    current.selectedCleaveLines.end(),
                    state.selectedCleaveLine)
            ? state.selectedCleaveLine
            : current.selectedCleaveLines.empty()
                ? -1
                : current.selectedCleaveLines.back();

        std::optional<RecoveryState::CleaveHistory> history{};
        if ( hasRecoveredHistory ) history = *state.cleaveHistory;
        const std::size_t candidateBytes =
            addCleaveRecoveryOwnedArrayBytes(
                cleaveRecoveryDraftOwnedArrayBytes(current),
                history ? cleaveRecoveryHistoryOwnedArrayBytes(*history) : 0u,
                1u);
        std::size_t allCandidateBytes = candidateBytes;
        if ( preparedPeer )
        {
            allCandidateBytes = addCleaveRecoveryOwnedArrayBytes(allCandidateBytes,
                cleaveRecoveryDraftOwnedArrayBytes(preparedPeer->draft), 1u);
            if ( preparedPeer->history )
            {
                allCandidateBytes = addCleaveRecoveryOwnedArrayBytes(allCandidateBytes,
                    cleaveRecoveryHistoryOwnedArrayBytes(*preparedPeer->history), 1u);
            }
        }
        if (sourceBytes > budget
            || liveBytes > budget - sourceBytes
            || allCandidateBytes > budget - sourceBytes - liveBytes)
        {
#ifdef SED_APP_MAIN_WINDOW_TEST_ACCESS
            if (cleaveRecoveryRestoreOwnedArrayTrackingForTest_)
            {
                cleaveRecoveryRestoreSourceOwnedArrayBytesForTest_ = sourceBytes;
                const std::size_t sourceAndLive =
                    addCleaveRecoveryOwnedArrayBytes(
                        sourceBytes,
                        cleaveRecoveryLiveOwnedArraysForTest(),
                        1u);
                cleaveRecoveryRestorePeakOwnedArrayBytesForTest_ = std::max(
                    cleaveRecoveryRestorePeakOwnedArrayBytesForTest_,
                    addCleaveRecoveryOwnedArrayBytes(
                        sourceAndLive, allCandidateBytes, 1u));
            }
#endif
            if (error)
            {
                *error = QStringLiteral(
                    "The recovered Cleave workspace exceeds its 16 MiB restore budget; the level was still recovered.");
            }
            return false;
        }

#ifdef SED_APP_MAIN_WINDOW_TEST_ACCESS
        if (cleaveRecoveryRestoreOwnedArrayTrackingForTest_)
        {
            cleaveRecoveryRestoreSourceOwnedArrayBytesForTest_ = sourceBytes;
            const std::size_t sourceAndLive =
                addCleaveRecoveryOwnedArrayBytes(
                    sourceBytes,
                    cleaveRecoveryLiveOwnedArraysForTest(),
                    1u);
            cleaveRecoveryRestorePeakOwnedArrayBytesForTest_ = std::max(
                cleaveRecoveryRestorePeakOwnedArrayBytesForTest_, sourceAndLive);
            const std::size_t withCandidates =
                addCleaveRecoveryOwnedArrayBytes(
                    sourceAndLive, allCandidateBytes, 1u);
            cleaveRecoveryRestorePeakOwnedArrayBytesForTest_ = std::max(
                cleaveRecoveryRestorePeakOwnedArrayBytesForTest_, withCandidates);
        }
#endif
        preparedCleaveDraft.emplace(std::move(current));
        preparedCleaveHistory = std::move(history);
    }

#ifdef SED_APP_MAIN_WINDOW_TEST_ACCESS
    if (cleaveRecoveryRestoreOwnedArrayTrackingForTest_)
    {
        cleaveRecoveryRestoreSourceOwnedArrayBytesForTest_ =
            cleaveRecoveryStateOwnedArrayBytes(state);
        cleaveRecoveryRestorePeakOwnedArrayBytesForTest_ = std::max(
            cleaveRecoveryRestorePeakOwnedArrayBytesForTest_,
            cleaveRecoveryRestoreSourceOwnedArrayBytesForTest_);
    }
    const auto recordCleaveRecoveryOwnedArraysForTest = [this](
        const std::size_t bytes)
    {
        if (cleaveRecoveryRestoreOwnedArrayTrackingForTest_)
        {
            cleaveRecoveryRestorePeakOwnedArrayBytesForTest_ = std::max(
                cleaveRecoveryRestorePeakOwnedArrayBytesForTest_, bytes);
        }
    };
    const auto addObservedCleaveRecoveryOwnedArrayBytes = [](
        const std::size_t left,
        const std::size_t right)
    {
        return addCleaveRecoveryOwnedArrayBytes(left, right, 1u);
    };
#endif
    QSignalBlocker settledPublication(this);
    cancelViewAnimation();
    stagedCleavePeerWorkspace_ = std::move(preparedPeer);
    stagedCleavePeerHistoryActive_ = stagedCleavePeerWorkspace_
        && (stagedCleavePeerWorkspace_->history.has_value()
            || !stagedCleavePeerWorkspace_->draft.lines.empty()
            || stagedCleavePeerWorkspace_->draft.context.valid);
    cleaveWorkspaceFamily_ = state.cleaveWorkspaceFamily;
    ++stagedCleaveJointGuidePreviewRevision_;
    resetStagedCleaveHistory();
    stagedCleaveEndpointOriginClickCandidate_ = false;
    editorState_ = state.editor;
    previewState_ = state.preview;
    renderPipeline_ = state.pipeline;

    const ViewportState& active = renderPipeline_ == RenderPipeline::Editor
        ? editorState_
        : previewState_;
    camera_.orbit = active.orbit;
    camera_.SetProjectionType(active.projectionType);
    camera_.SetFovXDeg(active.fovXDeg);
    camera_.SetOrthoScale(active.orthoScale);
    camera_.SetNearFar(state.nearPlane, state.farPlane);
    camera_.SetClipEnabled(active.clipEnabled);
    showWireframe_ = active.showWireframe;
    showVertices_ = active.showVertices;
    showSolid_ = active.showSolid;
    showTextures_ = active.showTextures;
    alphaAdjoinWireSupportsDirty_ = true;
    showFog_ = active.showFog;
    showThings_ = active.showThings;
    thingModelLodEnabled_ = active.thingModelLodEnabled;
    showHiddenThings_ = active.showHiddenThings;
    showThingGizmos_ = active.showThingGizmos;
    showArchThingGizmos_ = active.showArchThingGizmos;
    showThingCollisionShapes_ = active.showThingCollisionShapes;
    showThingVisualBounds_ = active.showThingVisualBounds;
    thingOverlayGeometryPrepared_ =
        showThingCollisionShapes_ || showThingVisualBounds_;
    thingViewMode_ = active.thingViewMode;
    thingTypeGizmoMetadataValid_ = false;
    thingTypeGizmoCacheValid_ = false;
    lightingMode_ = active.lightingMode;

    showGrid_ = state.showGrid;
    gridFollowCamera_ = state.gridFollowCamera;
    gridRotateWithView_ = state.gridRotateWithView;
    gridKeepViewAlignedVisible_ = state.gridKeepViewAlignedVisible;
    gridPlane_ = state.gridPlane;
    gridMajorSpacing_ = std::clamp(
        std::isfinite(state.gridMajorSpacing) ? state.gridMajorSpacing : 1.0f,
        1.0e-6f,
        1.0e9f);
    gridMinorSpacing_ = std::clamp(
        std::isfinite(state.gridMinorSpacing) ? state.gridMinorSpacing : 0.1f,
        1.0e-6f,
        gridMajorSpacing_);
    gridAdaptiveSpacing_ = state.gridAdaptiveSpacing;
    gridOpacity_ = std::clamp(
        std::isfinite(state.gridOpacity) ? state.gridOpacity : 0.70f,
        0.0f,
        1.0f);
    gridColor_ = state.gridColor.isValid()
        ? state.gridColor
        : QColor::fromRgb(112, 128, 156);
    gridColor_.setAlpha(255);
    gridMajorColor_ = state.gridMajorColor.isValid()
        ? state.gridMajorColor
        : QColor::fromRgb(170, 184, 208);
    gridMajorColor_.setAlpha(255);
    gridAxisXColor_ = state.gridAxisXColor.isValid()
        ? state.gridAxisXColor : QColor(210, 75, 70);
    gridAxisYColor_ = state.gridAxisYColor.isValid()
        ? state.gridAxisYColor : QColor(95, 190, 95);
    gridAxisZColor_ = state.gridAxisZColor.isValid()
        ? state.gridAxisZColor : QColor(90, 140, 215);
    gridAxisXColor_.setAlpha(255);
    gridAxisYColor_.setAlpha(255);
    gridAxisZColor_.setAlpha(255);
    gridLineWidthPx_ = render::clampedGridLineWidthPx(state.gridLineWidthPx);
    gridMinorDots_ = state.gridMinorDots;
    gridMinorDotSizePx_ = render::clampedGridMinorDotSizePx(state.gridMinorDotSizePx);
    gridVisibleThroughWalls_ = state.gridVisibleThroughWalls;
    // The two depth overrides are mutually exclusive in the live setters;
    // keep malformed/old recovery payloads from enabling both at once.
    gridAlwaysBehind_ = state.gridAlwaysBehind && !gridVisibleThroughWalls_;
    gridResetOnCameraRotate_ = state.gridResetOnCameraRotate;
    auto finiteVector = [](const QVector3D& value) noexcept
    {
        return std::isfinite(value.x())
            && std::isfinite(value.y())
            && std::isfinite(value.z());
    };
    gridCenterOverride_ = state.gridCenterOverride && finiteVector(*state.gridCenterOverride)
        ? state.gridCenterOverride
        : std::nullopt;
    QVector3D sanitizedAxisA{};
    QVector3D sanitizedAxisB{};
    QVector3D sanitizedNormal{};
    gridAlignToView_ = state.gridAlignToView
        && render::orthonormalizeGridAxes(
            state.gridAlignAxisA,
            state.gridAlignAxisB,
            sanitizedAxisA,
            sanitizedAxisB,
            sanitizedNormal);
    if (gridAlignToView_)
    {
        gridAlignAxisA_ = sanitizedAxisA;
        gridAlignAxisB_ = sanitizedAxisB;
    }
    else
    {
        gridAlignAxisA_ = QVector3D(1.0f, 0.0f, 0.0f);
        gridAlignAxisB_ = QVector3D(0.0f, 1.0f, 0.0f);
    }
    gridOverrideCamBasisValid_ = state.gridOverrideCamBasisValid
        && finiteVector(state.gridOverrideCamForward)
        && finiteVector(state.gridOverrideCamUp);
    if (gridOverrideCamBasisValid_)
    {
        gridOverrideCamForward_ = state.gridOverrideCamForward;
        gridOverrideCamUp_ = state.gridOverrideCamUp;
        gridOverrideCamBasisValid_ = normalizeInPlace(gridOverrideCamForward_);
        if (gridOverrideCamBasisValid_)
        {
            gridOverrideCamUp_ -= gridOverrideCamForward_
                * QVector3D::dotProduct(gridOverrideCamUp_, gridOverrideCamForward_);
            gridOverrideCamBasisValid_ = normalizeInPlace(gridOverrideCamUp_);
        }
    }
    if (!gridOverrideCamBasisValid_)
    {
        gridOverrideCamForward_ = QVector3D(0.0f, 1.0f, 0.0f);
        gridOverrideCamUp_ = QVector3D(0.0f, 0.0f, 1.0f);
    }
    if (!gridAlignToView_ && !gridCenterOverride_.has_value())
    {
        gridOverrideCamBasisValid_ = false;
    }
    dynamicLightShading_ = state.dynamicLightShading;
    thingLightRenderingEnabled_ = state.thingLightRenderingEnabled;
    sectorPointLightRenderingEnabled_ = state.sectorPointLightRenderingEnabled;
    showEditorCubes_ = state.showEditorCubes;
    showPreviewCubes_ = state.showPreviewCubes;
    previewRespectHiddenLayers_ = state.previewRespectHiddenLayers;
    xraySelectionCyclingEnabled_ = state.xraySelectionCycling;
    orbitAroundSelectionEnabled_ = state.orbitAroundSelection;
    showSectorThrustVectors_ = state.showSectorThrustVectors;
    alwaysShowLightGizmoCategories_ = state.alwaysShowLightGizmoCategories;
    transformTool_ = state.transformTool;
    transformOrientation_ = state.transformOrientation;
    transformGridSnapEnabled_ = state.transformGridSnap;
    transformGridPlaneStickEnabled_ = state.transformGridPlaneStick;
    cleaveStagedMode_ = state.cleaveStagedMode;
    cleaveChainEnabled_ = state.cleaveChainEnabled;
    if ( state.cleaveBounded.has_value() )
    {
        cleaveBounded_ = *state.cleaveBounded;
    }
    const bool legacyNoneUnit = state.cleaveMeasureUnit == CleaveMeasureUnit::None;
    const bool invalidMeasureUnit = state.cleaveMeasureUnit != CleaveMeasureUnit::Bare
        && state.cleaveMeasureUnit != CleaveMeasureUnit::Meters
        && state.cleaveMeasureUnit != CleaveMeasureUnit::JKU
        && state.cleaveMeasureUnit != CleaveMeasureUnit::IJU
        && !legacyNoneUnit;
    cleaveMeasurementsVisible_ = true;
    switch (state.cleaveMeasureUnit)
    {
        case CleaveMeasureUnit::Bare:
        case CleaveMeasureUnit::Meters:
        case CleaveMeasureUnit::JKU:
        case CleaveMeasureUnit::IJU:
            cleaveMeasureUnit_ = state.cleaveMeasureUnit;
            break;
        case CleaveMeasureUnit::None:
        default:
            cleaveMeasureUnit_ = CleaveMeasureUnit::Meters;
            break;
    }
    cleaveMeasureUnit_ = normalizedCleaveMeasureUnit(cleaveMeasureUnit_, level_);
    cleaveAnglesVisible_ = state.cleaveAnglesVisible;
    cleaveLengthsVisible_ = state.cleaveLengthsVisible
        && !legacyNoneUnit
        && !invalidMeasureUnit;
    cleaveLineNumbersVisible_ = state.cleaveLineNumbersVisible;
    cleaveAngleStepDegrees_ = state.cleaveAngleStepDegrees;
    sectorCleaveToolEnabled_ = state.sectorCleaveToolEnabled;
    surfaceCleaveToolEnabled_ = state.surfaceCleaveToolEnabled;
    pendingArchitecturalLightingRecovery_ = state.architecturalLighting;
    pendingArchitecturalLightingSelectionReady_ = false;
    pendingArchitecturalLightingOwnerSelection_.clear();
    if ( pendingArchitecturalLightingRecovery_ )
    {
        const auto validVertexMode = [](const level::MotsArchLightingVertexMode mode)
        {
            return mode == level::MotsArchLightingVertexMode::Mono
                || mode == level::MotsArchLightingVertexMode::Rgb;
        };
        const auto validTargetMode = [](const ArchitecturalLightingTargetMode mode)
        {
            return mode == ArchitecturalLightingTargetMode::Owner
                || mode == ArchitecturalLightingTargetMode::Vertices;
        };
        architecturalLightingAuthoringMode_ = validVertexMode(
            pendingArchitecturalLightingRecovery_->authoringMode)
            ? pendingArchitecturalLightingRecovery_->authoringMode
            : level::MotsArchLightingVertexMode::Rgb;
        architecturalLightingTargetMode_ = validTargetMode(
            pendingArchitecturalLightingRecovery_->targetMode)
            ? pendingArchitecturalLightingRecovery_->targetMode
            : ArchitecturalLightingTargetMode::Owner;
        architecturalLightingSelectedVertices_.clear();
        architecturalLightingSelectedVertexSet_.clear();
        architecturalLightingSelectedVerticesImplicitOwnerExpansion_ = false;
    }
    else
    {
        architecturalLightingSelectedVertices_.clear();
        architecturalLightingSelectedVertexSet_.clear();
        architecturalLightingSelectedVerticesImplicitOwnerExpansion_ = false;
    }
    const auto validTransformMode = [](const StagedCleaveTransformMode mode)
    {
        return mode == StagedCleaveTransformMode::Move
            || mode == StagedCleaveTransformMode::Rotate
            || mode == StagedCleaveTransformMode::Scale;
    };
    stagedCleaveTransformMode_ = validTransformMode(state.cleaveTransformMode)
        ? state.cleaveTransformMode
        : StagedCleaveTransformMode::Move;
    setCleaveShapeOptions(state.cleaveShapeOptions);
#ifdef SED_APP_MAIN_WINDOW_TEST_ACCESS
    recordCleaveRecoveryOwnedArraysForTest(addObservedCleaveRecoveryOwnedArrayBytes(
        cleaveRecoveryRestoreSourceOwnedArrayBytesForTest_,
        cleaveRecoveryLiveOwnedArraysForTest()));
#endif
    if (preparedCleaveDraft && preparedCleaveHistory)
    {
        restoreStagedCleaveDraftState(
            std::move(*preparedCleaveDraft),
            std::move(*preparedCleaveHistory));
#ifdef SED_APP_MAIN_WINDOW_TEST_ACCESS
        recordCleaveRecoveryOwnedArraysForTest(addObservedCleaveRecoveryOwnedArrayBytes(
            cleaveRecoveryRestoreSourceOwnedArrayBytesForTest_,
            cleaveRecoveryLiveOwnedArraysForTest()));
#endif
    }
    else
    {
        installStagedCleaveDraftState(std::move(*preparedCleaveDraft));
    }
    if ( !preparedCleaveHistory && !stagedCleaveLines_.empty() )
    {
        // Keep authored draft ownership local even when a baseline snapshot
        // cannot fit. Without this active marker, native Ctrl+Z could reach
        // Level history while the unapplied current-only draft is still live.
        stagedCleaveHistory_.active = true;
        const std::size_t sourceBytes = cleaveRecoveryStateOwnedArrayBytes(state);
        const std::size_t liveBytes = stagedCleaveHistoryStorageBytes();
        const std::size_t baselineBytes =
            stagedCleaveDraftStateCurrentOwnedBytesEstimate();
        const std::size_t reservedSnapshotBytes =
            addCleaveRecoveryOwnedArrayBytes(
                0u,
                kStagedCleaveHistoryMaxSnapshots,
                sizeof(StagedCleaveDraftState));
        const std::size_t budget = kStagedCleaveHistoryMaxBytes;
        if (sourceBytes <= budget
            && liveBytes <= budget - sourceBytes
            && baselineBytes <= budget - sourceBytes - liveBytes
            && reservedSnapshotBytes
                <= budget - sourceBytes - liveBytes - baselineBytes)
        {
            try
            {
                std::vector<StagedCleaveDraftState> preparedStates{};
                preparedStates.reserve(kStagedCleaveHistoryMaxSnapshots);
                const std::size_t actualReservedSnapshotBytes =
                    addCleaveRecoveryOwnedArrayBytes(
                        0u,
                        preparedStates.capacity(),
                        sizeof(StagedCleaveDraftState));
                if (sourceBytes <= budget
                    && liveBytes <= budget - sourceBytes
                    && baselineBytes <= budget - sourceBytes - liveBytes
                    && actualReservedSnapshotBytes
                        <= budget - sourceBytes - liveBytes - baselineBytes)
                {
                    StagedCleaveDraftState baseline = stagedCleaveDraftState();
                    const std::size_t actualBaselineBytes =
                        stagedCleaveDraftStateOwnedBytes(baseline);
                    const std::size_t sourceLiveAndReserve =
                        addCleaveRecoveryOwnedArrayBytes(
                            addCleaveRecoveryOwnedArrayBytes(
                                sourceBytes, liveBytes, 1u),
                            actualReservedSnapshotBytes,
                            1u);
                    if (actualBaselineBytes <= budget - sourceLiveAndReserve)
                    {
                        preparedStates.push_back(std::move(baseline));
                        stagedCleaveHistory_.states = std::move(preparedStates);
                        stagedCleaveHistory_.cursor = 0u;
                        stagedCleaveHistory_.active = true;
                        stagedCleaveHistory_.ownedBytes =
                            stagedCleaveHistoryStorageBytes();
                    }
                }
            }
            catch (const std::bad_alloc&)
            {
                // Current-only EDIT data remains usable even if its optional
                // initial Undo baseline cannot be allocated inside the same
                // bounded restore workset.
            }
        }
    }
#ifdef SED_APP_MAIN_WINDOW_TEST_ACCESS
    recordCleaveRecoveryOwnedArraysForTest(addObservedCleaveRecoveryOwnedArrayBytes(
        cleaveRecoveryRestoreSourceOwnedArrayBytesForTest_,
        cleaveRecoveryLiveOwnedArraysForTest()));
#endif

    normalizeStagedCleaveTransformMode();
    const app::ViewportCamera::OrbitState recoveredOrbit = camera_.orbit;
    if (rebuildRenderCache)
    {
        rebuildMeshes();
    }
    camera_.orbit = recoveredOrbit;
    gridCacheValid_ = false;
    if (glReady_ && rebuildRenderCache)
    {
        makeCurrent();
        uploadMeshes();
        doneCurrent();
    }
    const auto peerFamily = cleaveWorkspaceFamily_ == CleaveWorkspaceFamily::Sector
        ? CleaveWorkspaceFamily::Surface : CleaveWorkspaceFamily::Sector;
    noteStagedCleaveAuthoredChange(peerFamily);
    markViewChanged();
    settledPublication.unblock();
    Q_EMIT stagedCleaveAuthoredStateChanged(stagedCleaveAuthoredRevision_);
    Q_EMIT viewportStateChanged();
    update();
    return true;
}

void GlView::resetPreviewViewportState()
{
    previewState_ = {};
    previewState_.initialized = false;
}

void GlView::resetEditorTopDownView()
{
    cancelViewAnimation();

    // Canonical editor reset: orthographic top-down fit to current level bounds.
    camera_.SetProjectionType(app::ViewportCamera::ProjectionType::Orthographic);
    camera_.orbit.yawDeg   = 0.0f;
    // Fit from the exact top-down plane. A near-top-down pitch leaks tall Z
    // spans into the 2D map footprint and can make some levels load as a tiny
    // dot even though their XY footprint is modest.
    camera_.orbit.pitchDeg = 90.0f;
    camera_.orbit.rollDeg  = 0.0f;
    camera_.orbit.pan = QVector3D(0.0f, 0.0f, 0.0f);

    QVector3D minv(std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), std::numeric_limits<float>::max());
    QVector3D maxv(-std::numeric_limits<float>::max(), -std::numeric_limits<float>::max(), -std::numeric_limits<float>::max());
    bool haveAny = false;

    auto includeBounds = [&](const MeshPartCache& cache)
    {
        if (!cache.valid || !cache.haveAny)
        {
            return;
        }

        if (!haveAny)
        {
            minv = cache.min;
            maxv = cache.max;
            haveAny = true;
            return;
        }

        minv.setX(std::min(minv.x(), cache.min.x()));
        minv.setY(std::min(minv.y(), cache.min.y()));
        minv.setZ(std::min(minv.z(), cache.min.z()));

        maxv.setX(std::max(maxv.x(), cache.max.x()));
        maxv.setY(std::max(maxv.y(), cache.max.y()));
        maxv.setZ(std::max(maxv.z(), cache.max.z()));
    };

    if (computeLevelDataBounds(level_, minv, maxv))
    {
        haveAny = true;
    }
    else if (levelMeshCache_.valid && levelMeshCache_.haveAny)
    {
        includeBounds(levelMeshCache_);
    }
    else
    {
        includeBounds(thingMeshCache_);
    }

    const float dpr = devicePixelRatioF();
    const int pixelW = std::max(1, static_cast<int>(std::lround(static_cast<double>(width()) * dpr)));
    const int pixelH = std::max(1, static_cast<int>(std::lround(static_cast<double>(height()) * dpr)));
    camera_.SetCanvas(pixelW, pixelH);
    camera_.UpdateFromOrbit();
    camera_.Update();

    if (haveAny)
    {
        frameWorldBounds(minv, maxv, false, 0);
        return;
    }

    camera_.orbit.center = QVector3D(0.0f, 0.0f, 0.0f);
    camera_.orbit.distance = std::max(std::max(camera_.orbit.radius, 1.0f) * 1.5f, 1.0f);
    camera_.UpdateFromOrbit();
    camera_.Update();

    markViewChanged();
    update();
}

void GlView::resetFlySpeedMultiplier() noexcept
{
    flyNav_.speedMultiplier = 1.0f;
}

void GlView::setTransformTool(const TransformTool tool)
{
    if (transformTool_ == tool)
    {
        return;
    }

    restoreTransformSurfaceUvPreview();
    transformTool_ = tool;
    resetPickCycle();

    // Switching tools cancels any in-progress drag/preview.
    transformDrag_ = {};
    transformPreviewActive_ = false;
    thingPropertyTransformPreviewActive_ = false;
    thingPropertyTransformPreviewThing_ = -1;
    thingPropertyTransformPreviewThings_.clear();
    thingPropertyTransformPreviewRotating_ = false;
    setProperty("sed.thingPropertyTransformPreviewPrimary", QVariant{});
    transformPreviewVertexFilter_ = false;
    transformPreviewIdCount_ = 0;
    transformPreviewMatrix_.setToIdentity();
    previewThingSectors_.clear();

    Q_EMIT transformPreviewChanged(false, transformTool_, QVector3D(), QVector3D(), 0.0, QVector3D(), 1.0);

    update();
}

void GlView::setTransformInteractionEnabled(const bool enabled)
{
    if (transformInteractionEnabled_ == enabled)
    {
        return;
    }

    transformInteractionEnabled_ = enabled;
    if (!enabled
        && (transformDrag_.active
            || transformPreviewActive_
            || thingPropertyTransformPreviewActive_))
    {
        restoreTransformSurfaceUvPreview();
        // Immediately discard any live preview if the selection's layer
        // becomes hidden or locked. MainWindow also transfers the public tool
        // owner to Select, while this gate remains the final interaction guard.
        transformDrag_ = {};
        transformPreviewActive_ = false;
        thingPropertyTransformPreviewActive_ = false;
        thingPropertyTransformPreviewThing_ = -1;
        thingPropertyTransformPreviewThings_.clear();
        thingPropertyTransformPreviewRotating_ = false;
        setProperty("sed.thingPropertyTransformPreviewPrimary", QVariant{});
        transformPreviewVertexFilter_ = false;
        transformPreviewIdCount_ = 0;
        transformPreviewMatrix_.setToIdentity();
        previewThingSectors_.clear();
        Q_EMIT transformPreviewChanged(
            false, transformTool_, QVector3D(), QVector3D(), 0.0, QVector3D(), 1.0);
    }
    update();
}

bool GlView::transformPreviewMovesThingOwner() const noexcept
{
    if (!transformDrag_.active)
    {
        return false;
    }
    if (transformDrag_.target == TransformTargetKind::Thing)
    {
        return true;
    }
    if (transformDrag_.target != TransformTargetKind::ThingLight
        || transformDrag_.lightKind != render::LightGizmoKind::ThingRegular)
    {
        return false;
    }
    return transformDrag_.handle == TransformHandle::MoveX
        || transformDrag_.handle == TransformHandle::MoveY
        || transformDrag_.handle == TransformHandle::MoveZ
        || transformDrag_.handle == TransformHandle::MoveFree;
}

#ifdef SED_APP_MAIN_WINDOW_TEST_ACCESS
void GlView::previewThingLightTransformForTest(
    const render::LightGizmoKind kind,
    const QVector3D& translation)
{
    transformDrag_ = {};
    transformDrag_.active = true;
    transformDrag_.target = TransformTargetKind::ThingLight;
    transformDrag_.handle = TransformHandle::MoveFree;
    transformDrag_.lightKind = kind;
    transformDrag_.translation = translation;
    Q_EMIT transformPreviewChanged(
        true,
        TransformTool::Move,
        translation,
        QVector3D{},
        0.0,
        QVector3D{},
        1.0);
}

void GlView::clearTransformPreviewForTest()
{
    restoreTransformSurfaceUvPreview();
    transformDrag_ = {};
    Q_EMIT transformPreviewChanged(
        false,
        TransformTool::Move,
        QVector3D{},
        QVector3D{},
        0.0,
        QVector3D{},
        1.0);
}
#endif

void GlView::setTransformOrientation(const TransformOrientation orientation)
{
    if (transformOrientation_ == orientation)
    {
        return;
    }

    restoreTransformSurfaceUvPreview();
    transformOrientation_ = orientation;

    // Switching orientation cancels any in-progress drag/preview.
    transformDrag_ = {};
    transformPreviewActive_ = false;
    thingPropertyTransformPreviewActive_ = false;
    thingPropertyTransformPreviewThing_ = -1;
    thingPropertyTransformPreviewThings_.clear();
    thingPropertyTransformPreviewRotating_ = false;
    setProperty("sed.thingPropertyTransformPreviewPrimary", QVariant{});
    transformPreviewVertexFilter_ = false;
    transformPreviewIdCount_ = 0;
    transformPreviewMatrix_.setToIdentity();

    Q_EMIT transformPreviewChanged(false, transformTool_, QVector3D(), QVector3D(), 0.0, QVector3D(), 1.0);

    update();
}

void GlView::setViewportPickingEnabled(const bool enabled)
{
    if (viewportPickingEnabled_ == enabled)
    {
        return;
    }

    cancelViewportMouseGesture();
    viewportPickingEnabled_ = enabled;

    if (!viewportPickingEnabled_)
    {
        restoreTransformSurfaceUvPreview();
        // Cancel any in-progress interactive operations and clear any viewport-only highlights.
        axisWidgetPressed_ = false;
        axisWidgetDragging_ = false;
        previewInteractionDragging_ = false;
        boxSelectActive_ = false;
        boxSelectVisible_ = false;
        transformDrag_ = {};
        cogEntityPickHighlightEnabled_ = false;
        cleaveSnapMarker_.reset();
        cleaveSnapGuidesVisible_ = false;

        setSelectionState(-1, -1, -1, -1, editor::Selection{});
    }
    else
    {
        updateCleaveSnapGuideVisibility(QGuiApplication::keyboardModifiers());
    }
}

const ViewportNavigationSettings& GlView::effectiveViewportNavigationSettings() const noexcept
{
    // Resource previews have their own interaction model and retain its
    // existing controls independently of the world viewport preferences.
    static const ViewportNavigationSettings defaults{};
    return !viewportPickingEnabled_ && previewInteractionEnabled_
        ? defaults : viewportNavigationSettings_;
}

QPointF GlView::viewportCameraMouseDelta(const QPoint& delta) const noexcept
{
    const auto& navigation = effectiveViewportNavigationSettings();
    const qreal horizontal = static_cast<qreal>(delta.x());
    const qreal vertical = static_cast<qreal>(delta.y());
    return QPointF(navigation.invertHorizontal ? -horizontal : horizontal,
        navigation.invertVertical ? -vertical : vertical);
}

void GlView::setViewportNavigationSettings(const ViewportNavigationSettings& settings)
{
    const ViewportNavigationSettings next = validateViewportNavigationSettings(settings)
        ? settings : ViewportNavigationSettings{};
    if (viewportNavigationSettings_ == next)
    {
        return;
    }
    cancelViewportMouseGesture();
    viewportNavigationSettings_ = next;
}

bool GlView::setViewportBannerSettings(const ViewportBannerSettings& settings)
{
    if ( !validateViewportBannerSettings(settings) )
    {
        return false;
    }
    if ( viewportBannerSettings_ == settings )
    {
        return true;
    }

    viewportBannerSettings_ = settings;
    if ( stagedCleaveTransformHudHideTimer_
        && !stagedCleaveTransformHudHideTimer_->isActive() )
    {
        stagedCleaveTransformHudHideTimer_->setInterval(viewportBannerDurationMs(
            viewportBannerSettings_, stagedCleaveTransformHudBannerKind_, 400));
    }
    update();
    return true;
}

Qt::MouseButton GlView::viewportMouseSemanticButton() const noexcept
{
    if (viewportMouseGesture_.action == ViewportMouseAction::pick)
    {
        return Qt::LeftButton;
    }
    if (viewportMouseGesture_.action == ViewportMouseAction::flyLook)
    {
        return Qt::RightButton;
    }
    return viewportMouseActionIsCameraDrag(viewportMouseGesture_.action)
        ? Qt::MiddleButton : Qt::NoButton;
}

Qt::KeyboardModifiers GlView::viewportMouseSemanticModifiers(
    const Qt::KeyboardModifiers modifiers) const noexcept
{
    if (viewportMouseGesture_.action == ViewportMouseAction::pick)
    {
        return modifiers & ~viewportMouseGesture_.activationModifiers;
    }
    if (!viewportMouseActionIsCameraDrag(viewportMouseGesture_.action))
    {
        return modifiers;
    }

    const ViewportMouseAction action = viewportMouseActionForPress(
        effectiveViewportNavigationSettings(), viewportMouseGesture_.button, modifiers);
    switch (action)
    {
        case ViewportMouseAction::pan:
            return Qt::ShiftModifier;
        case ViewportMouseAction::dolly:
            return Qt::ControlModifier;
        case ViewportMouseAction::fastDolly:
            return Qt::ControlModifier | Qt::ShiftModifier;
        default:
            return Qt::NoModifier;
    }
}

void GlView::cancelViewportMouseGesture()
{
    // Wheel edits settle at focus/capture boundaries. Pointer previews use
    // their existing rollback path and must never synthesize a committing release.
    (void)finishStagedCleaveWheelSession();
    const bool hadPointerGesture = viewportMouseGesture_.button != Qt::NoButton;
    const bool hadPickGesture = viewportMouseGesture_.action == ViewportMouseAction::pick;
    const bool cancelPlacement = hadPickGesture && placementGestureEnabled_;
    viewportMouseGesture_ = {};
    stopFlyNavigation();
    selectionOrbitDragRejected_ = false;
    selectionOrbitDragPivot_.reset();
    if (!hadPointerGesture)
    {
        return;
    }

    axisWidgetPressed_ = false;
    axisWidgetDragging_ = false;
    previewInteractionDragging_ = false;
    boxSelectActive_ = false;
    boxSelectVisible_ = false;
    boxSelectFromTransformTool_ = false;
    leftDragged_ = false;
    suppressNextArchitecturalLightingVertexRelease_ = false;
    sectorThrustDrag_ = {};
    if (hadPickGesture)
    {
        suspendStagedCleaveInteraction();
        if (transformDrag_.active || transformPreviewActive_)
        {
            restoreTransformSurfaceUvPreview();
            transformDrag_ = {};
            transformPreviewActive_ = false;
            transformPreviewVertexFilter_ = false;
            transformPreviewIdCount_ = 0;
            transformPreviewMatrix_.setToIdentity();
            previewThingSectors_.clear();
            Q_EMIT transformPreviewChanged(
                false, transformTool_, QVector3D(), QVector3D(), 0.0, QVector3D(), 1.0);
        }
    }
    if (cancelPlacement)
    {
        Q_EMIT placementGestureCanceled();
    }
    if (hadPickGesture)
    {
        unsetCursor();
    }
    update();
}

void GlView::setPlacementGestureEnabled(const bool enabled) noexcept
{
    if (placementGestureEnabled_ == enabled)
    {
        return;
    }

    placementGestureEnabled_ = enabled;
    if (!placementGestureEnabled_)
    {
        boxSelectActive_ = false;
        boxSelectVisible_ = false;
        leftDragged_ = false;
    }
}

void GlView::setHoverPickEnabled(const bool enabled)
{
    if (hoverPickEnabled_ == enabled)
    {
        return;
    }

    hoverPickEnabled_ = enabled;
    hoverPickSector_ = -1;
    hoverPickSurface_ = -1;
    hoverPickThing_ = -1;
    hoverPickLight_ = -1;

    if (!hoverPickEnabled_)
    {
        Q_EMIT hoverPicked(-1, -1, -1, -1);
    }
}

void GlView::setPreviewInteractionEnabled(const bool enabled)
{
    if (previewInteractionEnabled_ == enabled)
    {
        return;
    }

    cancelViewportMouseGesture();
    previewInteractionEnabled_ = enabled;
    if (!previewInteractionEnabled_)
    {
        previewInteractionDragging_ = false;
    }
}

void GlView::setSelectionModifierBindings(
    Qt::KeyboardModifiers addModifiers,
    Qt::KeyboardModifiers removeModifiers,
    Qt::KeyboardModifiers surfaceBypassFiltersModifiers)
{
    setSelectionModifierBindings(
        ui::shortcuts::ModifierBindings{addModifiers},
        ui::shortcuts::ModifierBindings{removeModifiers},
        ui::shortcuts::ModifierBindings{surfaceBypassFiltersModifiers});
}

void GlView::setSelectionModifierBindings(
    const ui::shortcuts::ModifierBindings& addModifiers,
    const ui::shortcuts::ModifierBindings& removeModifiers,
    const ui::shortcuts::ModifierBindings& surfaceBypassFiltersModifiers)
{
    const ui::shortcuts::ModifierBindings normalizedAdd =
        ui::shortcuts::normalizedModifierBindings(addModifiers);
    const ui::shortcuts::ModifierBindings normalizedRemove =
        ui::shortcuts::normalizedModifierBindings(removeModifiers);
    const ui::shortcuts::ModifierBindings normalizedBypass =
        ui::shortcuts::normalizedModifierBindings(surfaceBypassFiltersModifiers);

    if (selectionAddModifiers_ == normalizedAdd
        && selectionRemoveModifiers_ == normalizedRemove
        && selectionSurfaceBypassFiltersModifiers_ == normalizedBypass)
    {
        return;
    }

    selectionAddModifiers_ = normalizedAdd;
    selectionRemoveModifiers_ = normalizedRemove;
    selectionSurfaceBypassFiltersModifiers_ = normalizedBypass;
}

void GlView::setSurfaceClickPreferSurfaceHit(const bool enabled) noexcept
{
    surfaceClickPreferSurfaceHit_ = enabled;
}

void GlView::setSurfaceClickSelectionEnabled(const bool enabled) noexcept
{
    surfaceClickSelectionEnabled_ = enabled;
}

void GlView::setXraySelectionCyclingEnabled(const bool enabled) noexcept
{
    if (xraySelectionCyclingEnabled_ == enabled)
    {
        return;
    }

    xraySelectionCyclingEnabled_ = enabled;
    resetPickCycle();
}

void GlView::setOrbitAroundSelectionEnabled(const bool enabled) noexcept
{
    // Disabling selection orbit must immediately return an in-flight MMB drag
    // to the legacy camera-target orbit path. Keep this idempotent so a
    // repeated settings/action synchronization cannot leave a stale pivot.
    if (!enabled)
    {
        selectionOrbitDragPivot_.reset();
    }

    if (orbitAroundSelectionEnabled_ == enabled)
    {
        return;
    }

    orbitAroundSelectionEnabled_ = enabled;
    Q_EMIT viewportStateChanged();
}

bool GlView::continuePickCycle(
    const QPoint& widgetPosition,
    const PickMode mode,
    const Qt::KeyboardModifiers selectionModifiers) noexcept
{
    const auto now = std::chrono::steady_clock::now();
    const render::picking::ViewportPickCycleContext previous{
        pickCycleState_.widgetPosition.x(),
        pickCycleState_.widgetPosition.y(),
        static_cast<std::int32_t>(pickCycleState_.pickMode),
        static_cast<std::int32_t>(pickCycleState_.surfaceSubMode),
        static_cast<std::int32_t>(pickCycleState_.transformTool),
        static_cast<std::int32_t>(pickCycleState_.selectionModifiers.toInt()),
        pickCycleState_.viewSerial,
        pickCycleState_.timestamp,
        pickCycleState_.valid };
    const render::picking::ViewportPickCycleContext current{
        widgetPosition.x(),
        widgetPosition.y(),
        static_cast<std::int32_t>(mode),
        static_cast<std::int32_t>(surfaceSubMode_),
        static_cast<std::int32_t>(transformTool_),
        static_cast<std::int32_t>(selectionModifiers.toInt()),
        viewSerial_,
        now,
        true };

    const bool shouldContinue =
        render::picking::shouldContinueViewportPickCycle(previous, current);
    pickCycleState_.widgetPosition = widgetPosition;
    pickCycleState_.pickMode = mode;
    pickCycleState_.surfaceSubMode = surfaceSubMode_;
    pickCycleState_.transformTool = transformTool_;
    pickCycleState_.selectionModifiers = selectionModifiers;
    pickCycleState_.viewSerial = viewSerial_;
    pickCycleState_.timestamp = now;
    pickCycleState_.valid = true;
    return shouldContinue;
}

void GlView::resetPickCycle() noexcept
{
    pickCycleState_ = {};
}

void GlView::setPickMode(const PickMode mode)
{
    if (pickMode_ == mode)
    {
        return;
    }

    const PickMode previousMode = pickMode_;
    pickMode_ = mode;
    if (previousMode == PickMode::ArchitecturalLighting
        && mode != PickMode::ArchitecturalLighting)
    {
        pendingArchitecturalLightingRecovery_.reset();
        pendingArchitecturalLightingSelectionReady_ = false;
        pendingArchitecturalLightingOwnerSelection_.clear();
        clearArchitecturalLightingSelectedVertices();
        hoveredArchitecturalLightingVertex_.reset();
        architecturalLightingLastOwnerClick_.reset();
    }
    resetPickCycle();
    leftDragged_ = false;
    boxSelectActive_ = false;
    boxSelectVisible_ = false;
    boxSelectFromTransformTool_ = false;
    if (renderPipeline_ != RenderPipeline::GamePreview
        && pickMode_ != PickMode::Surface)
    {
        clearSurfaceEdgeSelection();
    }
}

void GlView::setPickPreference(const PickPreference preference)
{
    pickPreference_ = preference;
    resetPickCycle();
    switch (preference)
    {
        case PickPreference::PreferSurface:
        {
            pickMode_ = PickMode::Surface;
            break;
        }
        case PickPreference::PreferThing:
        {
            pickMode_ = PickMode::Thing;
            break;
        }
        case PickPreference::PreferLight:
        {
            pickMode_ = PickMode::Light;
            break;
        }
        default:
        {
            pickMode_ = PickMode::Auto;
            break;
        }
    }
    if (renderPipeline_ != RenderPipeline::GamePreview
        && pickMode_ != PickMode::Surface)
    {
        clearSurfaceEdgeSelection();
    }
}

GlView::SurfaceSubMode GlView::surfaceSubMode() const noexcept
{
    return surfaceSubMode_;
}

std::span<const GlView::ArchitecturalLightingRenderVertex>
GlView::architecturalLightingVerticesForThing(
    const std::int32_t thingIndex) const noexcept
{
    static const std::vector<ArchitecturalLightingRenderVertex> empty{};
    const ArchitecturalLightingRenderCache* cache = nullptr;
    if (const auto cacheIndex =
            architecturalLightingSelectedOwnerCacheByThing_.find(thingIndex);
        cacheIndex != architecturalLightingSelectedOwnerCacheByThing_.end()
        && cacheIndex->second < architecturalLightingSelectedOwnerCaches_.size())
    {
        cache = &architecturalLightingSelectedOwnerCaches_[cacheIndex->second];
    }
    if (renderPipeline_ != RenderPipeline::Editor
        || !architecturalLightingEditingActive_
        || !cache
        || !cache->model
        || cache->modelIdentity == nullptr
        || cache->modelKey.empty()
        || !level_
        || thingIndex < 0
        || static_cast<std::size_t>(thingIndex) >= level_->things.size()
        || thingGeometrySignature(level_->things[static_cast<std::size_t>(thingIndex)])
            != cache->thingGeometryRevision
        || level_->things[static_cast<std::size_t>(thingIndex)].archLightingIndex
            != cache->archLightingIndex)
    {
        return std::span<const ArchitecturalLightingRenderVertex>(empty);
    }

    const auto modelIt = models3do_.find(cache->modelKey);
    if (modelIt == models3do_.end() || modelIt->second.get() != cache->modelIdentity)
    {
        // A resource publication can replace/clear the model map without
        // changing the document Thing. Do not expose a span into that stale
        // model; the next renderer rebuild will republish it.
        return std::span<const ArchitecturalLightingRenderVertex>(empty);
    }

    // ARCH table edits are published through notifyThingAppearanceChanged(),
    // which rebuilds this one-owner cache and replaces its revision. Keep the
    // accessor free of a full model/table hash: it is called from hover and
    // scrub paths and must remain a bounded read of the already published
    // owner span. Binding, Thing geometry, and resource-map identity checks
    // still reject the common stale-publication cases synchronously.
    return std::span<const ArchitecturalLightingRenderVertex>(cache->vertices);
}

void GlView::setSurfaceSubMode(const SurfaceSubMode mode)
{
    if (surfaceSubMode_ == mode)
    {
        return;
    }
    if (surfaceCleaveToolEnabled_)
    {
        (void)finishStagedCleaveWheelSession();
    }

    clearSurfaceEdgeSelection();
    surfaceSubMode_ = mode;
    if (surfaceSubMode_ == SurfaceSubMode::Vertex)
    {
        const std::optional<SurfaceRef> preferredOwner =
            selectedSurfaceSector_ >= 0 && selectedSurface_ >= 0
            ? std::optional<SurfaceRef>{ SurfaceRef{
                selectedSurfaceSector_, selectedSurface_ } }
            : std::nullopt;
        (void)revalidateActiveSurfaceVertex(
            highlightedSelection_, preferredOwner);
    }
    else
    {
        activeSurfaceVertexHintValid_ = false;
        activeSurfaceVertexHint_ = {};
    }
    resetPickCycle();
    if (surfaceSubMode_ != SurfaceSubMode::Face)
    {
        cleaveSnapMarker_.reset();
        cleaveSnapGuidesVisible_ = false;
    }
    else if (surfaceCleaveToolEnabled_)
    {
        updateCleaveSnapGuideVisibility(QGuiApplication::keyboardModifiers());
    }

    rebuildSelectionOverlays();
    if (glReady_)
    {
        makeCurrent();
        uploadSelectionOverlays();
        doneCurrent();
    }

    Q_EMIT viewportStateChanged();
    update();
}

void GlView::setStrictSurfaceSelectionScope(
    const bool enabled,
    const std::unordered_set<SurfaceRef, editor::SurfaceRefHash,
        editor::SurfaceRefEq>& surfaces)
{
    if (strictSurfaceSelectionEnabled_ == enabled
        && strictSurfaceSelectionScope_ == surfaces)
    {
        return;
    }

    strictSurfaceSelectionEnabled_ = enabled;
    strictSurfaceSelectionScope_ = surfaces;
    resetPickCycle();
    Q_EMIT viewportStateChanged();
}

void GlView::setShowSelectedSurfaceNormals(const bool enabled)
{
    if (showSelectedSurfaceNormals_ == enabled)
    {
        return;
    }

    showSelectedSurfaceNormals_ = enabled;
    rebuildSelectionOverlays();
    if (glReady_)
    {
        makeCurrent();
        uploadSelectionOverlays();
        doneCurrent();
    }
    Q_EMIT viewportStateChanged();
    update();
}

void GlView::setSurfaceNormalLengthScale(const float scale)
{
    const float next = render::clampedSurfaceNormalLengthScale(scale);
    if (std::abs(surfaceNormalLengthScale_ - next) <= sed::math::kTinyEpsilonF)
    {
        return;
    }

    surfaceNormalLengthScale_ = next;
    rebuildSelectionOverlays();
    if (glReady_)
    {
        makeCurrent();
        uploadSelectionOverlays();
        doneCurrent();
    }
    Q_EMIT viewportStateChanged();
    update();
}

void GlView::setSurfaceNormalArrowheadScale(const float scale)
{
    const float next = render::clampedSurfaceNormalArrowheadScale(scale);
    if (std::abs(surfaceNormalArrowheadScale_ - next) <= sed::math::kTinyEpsilonF)
    {
        return;
    }

    surfaceNormalArrowheadScale_ = next;
    rebuildSelectionOverlays();
    if (glReady_)
    {
        makeCurrent();
        uploadSelectionOverlays();
        doneCurrent();
    }
    Q_EMIT viewportStateChanged();
    update();
}

void GlView::setShowSurfaceNormalArrowhead(const bool enabled)
{
    if (showSurfaceNormalArrowhead_ == enabled)
    {
        return;
    }

    showSurfaceNormalArrowhead_ = enabled;
    rebuildSelectionOverlays();
    if (glReady_)
    {
        makeCurrent();
        uploadSelectionOverlays();
        doneCurrent();
    }
    Q_EMIT viewportStateChanged();
    update();
}

app::ViewportCamera::ProjectionType GlView::cameraProjectionType() const noexcept
{
    return camera_.projectionType();
}

void GlView::setCameraProjectionType(const app::ViewportCamera::ProjectionType type)
{
    if (camera_.projectionType() == type)
    {
        return;
    }

    const auto prevType = camera_.projectionType();

    const float dpr = devicePixelRatioF();
    const int pixelW = std::max(1, static_cast<int>(std::lround(static_cast<double>(width()) * dpr)));
    const int pixelH = std::max(1, static_cast<int>(std::lround(static_cast<double>(height()) * dpr)));
    camera_.SetCanvas(pixelW, pixelH);
    camera_.UpdateFromOrbit();

    const float aspect = camera_.aspect();
    const float hheight = static_cast<float>(pixelH) * 0.5f;
    const float vfovDeg = app::ViewportCamera::CalcVFovFromHFov(camera_.fovXDeg(), aspect);
    const float tanHalfVFov = std::tan(deg2Rad(vfovDeg * 0.5f));

    if (tanHalfVFov > 1e-6f && hheight > 1e-6f && aspect > 1e-6f)
    {
        if (prevType == app::ViewportCamera::ProjectionType::Perspective
            && type == app::ViewportCamera::ProjectionType::Orthographic)
        {
            const float halfHeight = camera_.orbit.distance * tanHalfVFov;
            if (halfHeight > 1e-6f)
            {
                const float scale = hheight / (aspect * halfHeight);
                camera_.SetOrthoScale(std::clamp(scale, 0.01f, 1.0e6f));
            }
        }
        else if (prevType == app::ViewportCamera::ProjectionType::Orthographic
            && type == app::ViewportCamera::ProjectionType::Perspective)
        {
            const float orthoHalfHeight = (hheight / std::max(camera_.orthoScale(), 1e-6f)) / aspect;
            const float dist = orthoHalfHeight / tanHalfVFov;
            camera_.orbit.distance = std::max(dist, camera_.orbit.radius * 0.05f);
        }
    }

    camera_.SetProjectionType(type);
    markViewChanged();
    update();

    showToast(
        (type == app::ViewportCamera::ProjectionType::Perspective) ? "Projection: Perspective" : "Projection: Orthographic",
        2500,
        ToastPlacement::TopLeft,
        ToastGroup::Projection);
}

float GlView::cameraOrthoScale() const noexcept
{
    return camera_.orthoScale();
}

void GlView::setCameraOrthoScale(const float scale)
{
    if (!std::isfinite(scale) || scale <= 0.0f)
    {
        return;
    }

    cancelViewAnimation();
    camera_.SetOrthoScale(std::clamp(scale, 0.01f, 1.0e6f));
    markViewChanged();
    update();
}

float GlView::cameraOrbitDistance() const noexcept
{
    return camera_.orbit.distance;
}

void GlView::setCameraOrbitDistance(const float distance)
{
    if (!std::isfinite(distance) || distance <= 0.0f)
    {
        return;
    }

    cancelViewAnimation();
    camera_.orbit.distance = std::max(distance, std::max(camera_.orbit.radius * 0.05f, 0.01f));
    markViewChanged();
    update();
}

void GlView::zoomCameraBySteps(const float steps)
{
    if ( !std::isfinite(steps) || std::abs(steps) < 0.001f || flyNav_.active
        || viewportMouseGesture_.action == ViewportMouseAction::pick
        || stagedCleaveHistory_.gestureActive || stagedCleaveWheelSession_.active() )
    {
        return;
    }

    cancelViewAnimation();
    if ( camera_.projectionType() == app::ViewportCamera::ProjectionType::Perspective )
    {
        camera_.orbit.distance *= std::pow(0.9f, steps);
        camera_.orbit.distance = std::max(camera_.orbit.distance, camera_.orbit.radius * 0.05f);
    }
    else
    {
        const float scale = camera_.orthoScale() * std::pow(1.0f / 0.9f, steps);
        camera_.SetOrthoScale(std::clamp(scale, 0.01f, 1.0e6f));
    }
    markViewChanged();
    update();
}

void GlView::showToast(const QString& text, const int durationMs, const ToastPlacement placement, const ToastGroup group)
{
    if (!showToastOverlays_)
    {
        return;
    }

    const auto now = std::chrono::steady_clock::now();

    const std::size_t idx = static_cast<std::size_t>(group);
    if (idx >= toastStates_.size())
    {
        return;
    }

    ToastState& t = toastStates_[idx];
    const bool wasVisible = !t.text.isEmpty() && now < t.expires;
    const int configuredDurationMs = viewportBannerDurationMs(
        viewportBannerSettings_, viewportBannerKindForToastGroup(group), durationMs);

    t.text = text;
    t.durationMs = std::max(1, configuredDurationMs);
    t.placement = placement;
    if (!wasVisible)
    {
        t.shownAt = now;
    }
    t.expires = now + std::chrono::milliseconds(t.durationMs);

    update();

    if (toastTimer_)
    {
        toastTimer_->start();
    }
}

void GlView::startFlyNavigation(const Qt::KeyboardModifiers modifiers)
{
    if (flyNav_.active)
    {
        return;
    }

    cancelViewAnimation();

    flyNav_.active = true;
    flyNav_.forward = false;
    flyNav_.backward = false;
    flyNav_.left = false;
    flyNav_.right = false;
    flyNav_.down = false;
    flyNav_.up = false;
    flyNav_.fastKeyHeld = false;
    flyNav_.slowKeyHeld = false;
    refreshFlyNavigationSpeedModifiers(modifiers | QGuiApplication::keyboardModifiers());
    flyNav_.lastTick = std::chrono::steady_clock::now();

    // Fly is deliberately a conventional, world-upright yaw/pitch camera.
    // F2 may leave the orbit representation banked or on the alternate side
    // of a pole, so derive Fly's yaw/pitch from the actual visible forward
    // vector instead of reusing those scalar angles. The orbit target already
    // is the F2/grid origin; preserving it also preserves the eye and avoids a
    // navigation jump while roll is removed.
    const app::ViewportCamera::OrbitBasis beforeFlyBasis =
        app::ViewportCamera::BasisFromOrbit(
            camera_.orbit.yawDeg,
            camera_.orbit.pitchDeg,
            camera_.orbit.rollDeg);
    const app::ViewportCamera::OrbitAngles upright =
        app::ViewportCamera::UprightFlyOrbit(
            camera_.orbit.yawDeg,
            camera_.orbit.pitchDeg,
            camera_.orbit.rollDeg);
    if (upright.valid)
    {
        camera_.orbit.yawDeg = upright.yawDeg;
        camera_.orbit.pitchDeg = upright.pitchDeg;
        camera_.orbit.rollDeg = 0.0f;
        const app::ViewportCamera::OrbitBasis uprightBasis =
            app::ViewportCamera::BasisFromOrbit(
                upright.yawDeg, upright.pitchDeg, 0.0f);
        const bool presentationChanged =
            QVector3D::dotProduct(beforeFlyBasis.right, uprightBasis.right)
                    < 0.99999f
            || QVector3D::dotProduct(beforeFlyBasis.up, uprightBasis.up)
                    < 0.99999f;
        if (presentationChanged)
        {
            markViewChanged();
            update();
        }
    }

    Q_EMIT flyNavigationActiveChanged(true);

    prevCursor_ = cursor();
    havePrevCursor_ = true;
    setCursor(flyCursor_);

    if (flyTimer_)
    {
        flyTimer_->start();
    }
}

void GlView::stopFlyNavigation()
{
    if (!flyNav_.active)
    {
        return;
    }

    flyNav_.active = false;
    flyNav_.forward = false;
    flyNav_.backward = false;
    flyNav_.left = false;
    flyNav_.right = false;
    flyNav_.down = false;
    flyNav_.up = false;
    flyNav_.fast = false;
    flyNav_.slow = false;
    flyNav_.fastKeyHeld = false;
    flyNav_.slowKeyHeld = false;
    flySpeedHeldModifiers_ = Qt::NoModifier;

    Q_EMIT flyNavigationActiveChanged(false);

    if (flyTimer_)
    {
        flyTimer_->stop();
    }

    if (havePrevCursor_)
    {
        setCursor(prevCursor_);
        havePrevCursor_ = false;
    }
    else
    {
        unsetCursor();
    }
}

void GlView::tickFlyNavigation()
{
    if (!flyNav_.active || !level_)
    {
        return;
    }

    const auto now = std::chrono::steady_clock::now();
    const float elapsed = std::chrono::duration_cast<std::chrono::duration<float>>(
        now - flyNav_.lastTick).count();
    flyNav_.lastTick = now;

    if (elapsed <= 0.0f || elapsed > 0.25f)
    {
        return;
    }

    // A busy UI frame must not be converted into one large camera leap on
    // the next 16 ms timer tick. Capping catch-up displacement makes fly
    // navigation slow for the missed frame instead of visibly snapping; the
    // following ticks immediately resume normal speed.
    const float dt = std::min(elapsed, 1.0f / 30.0f);

    QVector3D move(0.0f, 0.0f, 0.0f);

    const QVector3D dir = app::ViewportCamera::SphericalDirection(
        camera_.orbit.yawDeg, camera_.orbit.pitchDeg).normalized();
    const QVector3D forward = -dir;
    const QVector3D worldUp(0.0f, 0.0f, 1.0f);
    QVector3D right = QVector3D::crossProduct(forward, worldUp);
    if (right.lengthSquared() < sed::math::kTinyEpsilonF)
    {
        const float yawRad = deg2Rad(camera_.orbit.yawDeg);
        right = QVector3D(
            static_cast<float>(std::cos(yawRad)),
            static_cast<float>(std::sin(yawRad)),
            0.0f);
    }
    if (right.lengthSquared() > sed::math::kTinyEpsilonF)
    {
        right.normalize();
    }

    if (flyNav_.forward)
    {
        move += forward;
    }
    if (flyNav_.backward)
    {
        move -= forward;
    }
    if (flyNav_.left)
    {
        move -= right;
    }
    if (flyNav_.right)
    {
        move += right;
    }
    if (flyNav_.down)
    {
        move -= worldUp;
    }
    if (flyNav_.up)
    {
        move += worldUp;
    }

    if (move.lengthSquared() < 1e-10f)
    {
        return;
    }
    move.normalize();

    const float baseSpeed = std::max(0.1f, camera_.orbit.radius * 0.05f);
    const float speed = baseSpeed * flyNav_.speedMultiplier
        * (flyNav_.fast ? 2.0f : 1.0f) * (flyNav_.slow ? 0.5f : 1.0f);

    camera_.orbit.pan += move * (speed * dt);
    markViewChanged();
    update();
}

void GlView::flyLook(const QPointF& mouseDelta)
{
    const QVector3D oldTarget = camera_.orbit.target();
    const QVector3D oldDir = app::ViewportCamera::SphericalDirection(
        camera_.orbit.yawDeg, camera_.orbit.pitchDeg);
    const QVector3D eye = oldTarget + oldDir * camera_.orbit.distance;

    camera_.orbit.yawDeg -= static_cast<float>(mouseDelta.x()) * 0.25f;
    camera_.orbit.pitchDeg = app::ViewportCamera::ClampPitch(
        camera_.orbit.pitchDeg
        + static_cast<float>(mouseDelta.y()) * 0.25f);
    camera_.orbit.rollDeg = 0.0f;

    const QVector3D newDir = app::ViewportCamera::SphericalDirection(
        camera_.orbit.yawDeg, camera_.orbit.pitchDeg);
    const QVector3D newTarget = eye - newDir * camera_.orbit.distance;
    camera_.orbit.pan = newTarget - camera_.orbit.center;

    markViewChanged();
    update();
}

float GlView::cameraFovXDeg() const noexcept
{
    return camera_.fovXDeg();
}

void GlView::setCameraFovXDeg(const float fovXDeg)
{
    camera_.SetFovXDeg(fovXDeg);
    markViewChanged();
    update();
}

float GlView::cameraNearPlane() const noexcept
{
    return camera_.nearPlane();
}

float GlView::cameraFarPlane() const noexcept
{
    return camera_.farPlane();
}

void GlView::setCameraNearFar(const float nearPlane, const float farPlane)
{
    camera_.SetNearFar(nearPlane, farPlane);
    markViewChanged();
    update();
}

void GlView::tightenOrthoClipToWorldBounds(const QVector3D& min, const QVector3D& max, const float paddingScale)
{
    if (camera_.projectionType() != app::ViewportCamera::ProjectionType::Orthographic)
    {
        return;
    }

    QVector3D mn(
        std::min(min.x(), max.x()),
        std::min(min.y(), max.y()),
        std::min(min.z(), max.z()));
    QVector3D mx(
        std::max(min.x(), max.x()),
        std::max(min.y(), max.y()),
        std::max(min.z(), max.z()));

    if (!std::isfinite(mn.x()) || !std::isfinite(mn.y()) || !std::isfinite(mn.z())
        || !std::isfinite(mx.x()) || !std::isfinite(mx.y()) || !std::isfinite(mx.z()))
    {
        return;
    }

    QVector3D extents = mx - mn;
    if (extents.lengthSquared() <= sed::math::kTinyEpsilonF)
    {
        mn -= QVector3D(0.5f, 0.5f, 0.5f);
        mx += QVector3D(0.5f, 0.5f, 0.5f);
        extents = mx - mn;
    }

    const QVector3D center = (mn + mx) * 0.5f;
    const float radius = std::max(0.05f, extents.length() * 0.5f * std::max(paddingScale, 1.0f));

    const float dpr = devicePixelRatioF();
    const int pixelW = std::max(1, static_cast<int>(std::lround(static_cast<double>(width()) * dpr)));
    const int pixelH = std::max(1, static_cast<int>(std::lround(static_cast<double>(height()) * dpr)));
    camera_.SetCanvas(pixelW, pixelH);
    camera_.UpdateFromOrbit();
    camera_.Update();

    const float centerViewZ = camera_.viewMatrix().map(center).z();
    const float clipDepth = std::max(
        1.0f,
        std::max(radius * 6.0f, std::abs(centerViewZ) + radius * 4.0f));

    camera_.SetClipEnabled(true);
    camera_.SetNearFar(-clipDepth, clipDepth);
    markViewChanged();
    update();
}

bool GlView::cameraClipEnabled() const noexcept
{
    return camera_.clipEnabled();
}

void GlView::setCameraClipEnabled(const bool enabled)
{
    camera_.SetClipEnabled(enabled);
    markViewChanged();
    update();
}

QVector3D GlView::cameraEye() const noexcept
{
    return camera_.orbit.target()
        + app::ViewportCamera::SphericalDirection(
            camera_.orbit.yawDeg, camera_.orbit.pitchDeg)
            * camera_.orbit.distance;
}

QVector3D GlView::cameraTarget() const noexcept
{
    // Orbit state is authoritative between repaint ticks. Returning the
    // matrix cache here made camera navigation appear stale while a view
    // animation was running (and in hidden test views which never repaint).
    return camera_.orbit.target();
}

QVector3D GlView::cameraForward() const noexcept
{
    return app::ViewportCamera::BasisFromOrbit(
        camera_.orbit.yawDeg,
        camera_.orbit.pitchDeg,
        camera_.orbit.rollDeg).forward;
}

QVector3D GlView::cameraRight() const noexcept
{
    return app::ViewportCamera::BasisFromOrbit(
        camera_.orbit.yawDeg,
        camera_.orbit.pitchDeg,
        camera_.orbit.rollDeg).right;
}

QVector3D GlView::cameraUp() const noexcept
{
    return app::ViewportCamera::BasisFromOrbit(
        camera_.orbit.yawDeg,
        camera_.orbit.pitchDeg,
        camera_.orbit.rollDeg).up;
}

std::optional<QPointF> GlView::projectWorldToViewport(const QVector3D& world)
{
    QMatrix4x4 projection{};
    QMatrix4x4 view{};
    QVector3D eye{};
    QVector3D target{};
    float nearPlane{};
    float farPlane{};
    buildCameraMatrices(projection, view, eye, target, nearPlane, farPlane);

    const QMatrix4x4 clipMatrix = projection * view;
    const QVector4D clip = clipMatrix * QVector4D(world, 1.0f);
    if (!std::isfinite(clip.x()) || !std::isfinite(clip.y()) || !std::isfinite(clip.z()) || !std::isfinite(clip.w()))
    {
        return std::nullopt;
    }
    if (std::abs(clip.w()) <= 1.0e-6f
        || (camera_.projectionType() == app::ViewportCamera::ProjectionType::Perspective
            && clip.w() <= 0.0f))
    {
        return std::nullopt;
    }

    const QVector3D ndc = clip.toVector3DAffine();
    if (!std::isfinite(ndc.x()) || !std::isfinite(ndc.y()) || !std::isfinite(ndc.z()))
    {
        return std::nullopt;
    }
    if (ndc.z() < -1.0001f || ndc.z() > 1.0001f)
    {
        return std::nullopt;
    }

    return QPointF(
        (static_cast<qreal>(ndc.x()) * 0.5 + 0.5) * static_cast<qreal>(width()),
        (1.0 - (static_cast<qreal>(ndc.y()) * 0.5 + 0.5)) * static_cast<qreal>(height()));
}

std::optional<QVector3D> GlView::viewportPointOnPlane(
    const QPoint& widgetPos,
    const QVector3D& planePoint,
    const QVector3D& planeNormal)
{
    if (width() <= 0 || height() <= 0)
    {
        return std::nullopt;
    }

    QVector3D normal = planeNormal;
    if (!normalizeInPlace(normal))
    {
        return std::nullopt;
    }

    QMatrix4x4 proj;
    QMatrix4x4 view;
    QVector3D eye{};
    QVector3D target{};
    float nearPlane{};
    float farPlane{};
    buildCameraMatrices(proj, view, eye, target, nearPlane, farPlane);

    bool invOk = false;
    const QMatrix4x4 invMvp = (proj * view).inverted(&invOk);
    if (!invOk)
    {
        return std::nullopt;
    }

    const bool isOrtho = camera_.projectionType() == app::ViewportCamera::ProjectionType::Orthographic;
    return intersectWidgetRayWithPlane(widgetPos, invMvp, eye, isOrtho, width(), height(), planePoint, normal);
}

std::optional<TVector> GlView::viewportPointOnPlane(
    const QPoint& widgetPos, const TVector& planePoint, const TVector& planeNormal)
{
    QMatrix4x4 projection;
    QMatrix4x4 view;
    QVector3D eye{};
    QVector3D target{};
    float nearPlane{};
    float farPlane{};
    buildCameraMatrices(projection, view, eye, target, nearPlane, farPlane);
    const auto ray = render::viewportRayForWidget(
        projection, view, eye, widgetPos, width(), height(),
        camera_.projectionType() == app::ViewportCamera::ProjectionType::Orthographic);
    return ray ? render::cleaveIntersectRayPlane(*ray, planePoint, planeNormal) : std::nullopt;
}

double GlView::worldUnitsPerPixelAtWorld(const TVector& world)
{
    if (width() <= 0 || height() <= 0) return 0.0;
    QMatrix4x4 projection;
    QMatrix4x4 view;
    QVector3D eye{};
    QVector3D target{};
    float nearPlane{};
    float farPlane{};
    buildCameraMatrices(projection, view, eye, target, nearPlane, farPlane);
    const bool orthographic = camera_.projectionType() == app::ViewportCamera::ProjectionType::Orthographic;
    const TVector normal = render::cleaveDocumentPoint(cameraForward());
    const QPointF center(width() * 0.5, height() * 0.5);
    const auto firstRay = render::viewportRayForWidget(projection, view, eye, center, width(), height(), orthographic);
    const auto secondRay = render::viewportRayForWidget(projection, view, eye, center + QPointF(1.0, 0.0), width(), height(), orthographic);
    if (!firstRay || !secondRay) return 0.0;
    const auto first = render::cleaveIntersectRayPlane(*firstRay, world, normal);
    const auto second = render::cleaveIntersectRayPlane(*secondRay, world, normal);
    return first && second ? (*second - *first).length() : 0.0;
}

float GlView::worldUnitsPerPixelAtWorld(const QVector3D& world)
{
    if (width() <= 0 || height() <= 0)
    {
        return 0.0f;
    }

    QMatrix4x4 proj;
    QMatrix4x4 view;
    QVector3D eye{};
    QVector3D target{};
    float nearPlane{};
    float farPlane{};
    buildCameraMatrices(proj, view, eye, target, nearPlane, farPlane);

    const QMatrix4x4 mvp = proj * view;
    bool invOk = false;
    const QMatrix4x4 invMvp = mvp.inverted(&invOk);
    if (!invOk)
    {
        return 0.0f;
    }

    return worldUnitsPerPixelAt(world, mvp, invMvp, width(), height());
}

void GlView::setPlacementPreviewLines(std::vector<QVector3D> lines, const bool cursorGuide, const bool invalid)
{
    std::vector<TVector> worldLines;
    worldLines.reserve(lines.size());
    for ( const auto& point : lines )
    {
        worldLines.push_back({point.x(), point.y(), point.z()});
    }
    setPlacementPreviewLines(std::move(worldLines), cursorGuide, invalid);
}

void GlView::setPlacementPreviewLines(std::vector<TVector> lines, const bool cursorGuide, const bool invalid)
{
    placementPreviewLines_ = std::move(lines);
    placementPreviewCursorGuide_ = cursorGuide;
    placementPreviewLinesInvalid_ = invalid && !placementPreviewLines_.empty();
    update();
}

void GlView::clearPlacementPreviewLines()
{
    if (placementPreviewLines_.empty() && !placementPreviewCursorGuide_)
    {
        return;
    }

    placementPreviewLines_.clear();
    placementPreviewCursorGuide_ = false;
    placementPreviewLinesInvalid_ = false;
    update();
}

void GlView::setPlacementPreviewSector(
    std::optional<Sector> sector,
    const bool invalid,
    std::optional<SurfaceRef> hiddenSurface,
    std::optional<Sector> sourcePatch)
{
    placementPreviewSector_ = std::move(sector);
    placementPreviewSourcePatch_ = std::move(sourcePatch);
    const bool hasPreviewGeometry = placementPreviewSector_.has_value() || placementPreviewSourcePatch_.has_value();
    placementPreviewHiddenSurface_ = hasPreviewGeometry ? hiddenSurface : std::nullopt;
    placementPreviewSectorInvalid_ = placementPreviewSector_.has_value() && invalid;
    if (!level_)
    {
        update();
        return;
    }

    const QVector3D oldTarget = camera_.orbit.target();
    const float oldYawDeg = camera_.orbit.yawDeg;
    const float oldPitchDeg = camera_.orbit.pitchDeg;
    const float oldDistance = camera_.orbit.distance;

    rebuildMeshes(false, MeshRebuildKind::LevelOnly);

    camera_.orbit.yawDeg = oldYawDeg;
    camera_.orbit.pitchDeg = oldPitchDeg;
    camera_.orbit.distance = oldDistance;
    camera_.orbit.pan = oldTarget - camera_.orbit.center;
    camera_.orbit.distance = std::max(camera_.orbit.distance, camera_.orbit.radius * 0.05f);

    if (glReady_)
    {
        makeCurrent();
        uploadMeshes();
        doneCurrent();
    }

    update();
}

void GlView::clearPlacementPreviewSector()
{
    if (!placementPreviewSector_ && !placementPreviewSourcePatch_ && !placementPreviewHiddenSurface_)
    {
        return;
    }
    setPlacementPreviewSector(std::nullopt);
}

void GlView::setPlacementPreviewThing(std::optional<Thing> thing, const bool invalid)
{
    const bool hadMultiPreview = !placementPreviewThings_.empty();
    const bool changed =
        hadMultiPreview
        || placementPreviewThing_.has_value() != thing.has_value()
        || placementPreviewThingInvalid_ != (thing.has_value() && invalid)
        || (placementPreviewThing_ && thing
            && (placementPreviewThing_->name != thing->name
                || placementPreviewThing_->templateName != thing->templateName
                || thingGeometrySignature(*placementPreviewThing_)
                    != thingGeometrySignature(*thing)
                || thingRuntimeLightSignature(*placementPreviewThing_)
                    != thingRuntimeLightSignature(*thing)
                || placementPreviewThing_->sector != thing->sector
                || placementPreviewThing_->x != thing->x
                || placementPreviewThing_->y != thing->y
                || placementPreviewThing_->z != thing->z
                || placementPreviewThing_->pch != thing->pch
                || placementPreviewThing_->yaw != thing->yaw
                || placementPreviewThing_->rol != thing->rol));
    const bool fastUpdated = changed
        && !hadMultiPreview
        && placementPreviewThing_.has_value()
        && thing.has_value()
        && tryFastUpdatePlacementPreviewThings(
            std::vector<Thing>{ *thing },
            invalid);

    placementPreviewThings_.clear();
    placementPreviewThing_ = std::move(thing);
    placementPreviewThingInvalid_ = placementPreviewThing_.has_value() && invalid;
    if (fastUpdated)
    {
        update();
        return;
    }
    if (!changed || !level_)
    {
        update();
        return;
    }

    const QVector3D oldTarget = camera_.orbit.target();
    const float oldYawDeg = camera_.orbit.yawDeg;
    const float oldPitchDeg = camera_.orbit.pitchDeg;
    const float oldDistance = camera_.orbit.distance;

    rebuildMeshes(false, MeshRebuildKind::ThingsOnly);

    camera_.orbit.yawDeg = oldYawDeg;
    camera_.orbit.pitchDeg = oldPitchDeg;
    camera_.orbit.distance = oldDistance;
    camera_.orbit.pan = oldTarget - camera_.orbit.center;
    camera_.orbit.distance = std::max(camera_.orbit.distance, camera_.orbit.radius * 0.05f);

    if (glReady_)
    {
        makeCurrent();
        uploadMeshes();
        doneCurrent();
    }

    update();
}

void GlView::setPlacementPreviewThings(std::vector<Thing> things, const bool invalid)
{
    auto sameThing = [](const Thing& lhs, const Thing& rhs) noexcept -> bool
    {
        return lhs.name == rhs.name
            && lhs.templateName == rhs.templateName
            && thingGeometrySignature(lhs) == thingGeometrySignature(rhs)
            && thingRuntimeLightSignature(lhs) == thingRuntimeLightSignature(rhs)
            && lhs.sector == rhs.sector
            && lhs.x == rhs.x
            && lhs.y == rhs.y
            && lhs.z == rhs.z
            && lhs.pch == rhs.pch
            && lhs.yaw == rhs.yaw
            && lhs.rol == rhs.rol;
    };

    bool same = !placementPreviewThing_.has_value()
        && placementPreviewThingInvalid_ == (invalid && !things.empty())
        && placementPreviewThings_.size() == things.size();
    if (same)
    {
        for (std::size_t i = 0; i < things.size(); ++i)
        {
            if (!sameThing(placementPreviewThings_[i], things[i]))
            {
                same = false;
                break;
            }
        }
    }

    const bool fastUpdated = !same
        && !placementPreviewThing_.has_value()
        && !placementPreviewThings_.empty()
        && tryFastUpdatePlacementPreviewThings(things, invalid);

    placementPreviewThing_.reset();
    placementPreviewThings_ = std::move(things);
    placementPreviewThingInvalid_ = !placementPreviewThings_.empty() && invalid;
    if (fastUpdated)
    {
        update();
        return;
    }
    if (same || !level_)
    {
        update();
        return;
    }

    const QVector3D oldTarget = camera_.orbit.target();
    const float oldYawDeg = camera_.orbit.yawDeg;
    const float oldPitchDeg = camera_.orbit.pitchDeg;
    const float oldDistance = camera_.orbit.distance;

    rebuildMeshes(false, MeshRebuildKind::ThingsOnly);

    camera_.orbit.yawDeg = oldYawDeg;
    camera_.orbit.pitchDeg = oldPitchDeg;
    camera_.orbit.distance = oldDistance;
    camera_.orbit.pan = oldTarget - camera_.orbit.center;
    camera_.orbit.distance = std::max(camera_.orbit.distance, camera_.orbit.radius * 0.05f);

    if (glReady_)
    {
        makeCurrent();
        uploadMeshes();
        doneCurrent();
    }

    update();
}

void GlView::clearPlacementPreviewThing()
{
    if (!placementPreviewThing_ && placementPreviewThings_.empty())
    {
        return;
    }
    setPlacementPreviewThing(std::nullopt);
}

void GlView::orbitRotate(const float yawDeltaDeg, const float pitchDeltaDeg)
{
    cancelViewAnimation();
    const app::ViewportCamera::OrbitAngles rotated =
        app::ViewportCamera::RotatedOrbit(
            camera_.orbit.yawDeg,
            camera_.orbit.pitchDeg,
            camera_.orbit.rollDeg,
            yawDeltaDeg,
            pitchDeltaDeg);
    if (!rotated.valid)
    {
        return;
    }
    camera_.orbit.yawDeg = rotated.yawDeg;
    camera_.orbit.pitchDeg = rotated.pitchDeg;
    camera_.orbit.rollDeg = rotated.rollDeg;
    markViewChanged();
    update();
}

void GlView::setOrbitViewImmediate(const float yawDeg, const float pitchDeg, const float rollDeg)
{
    cancelViewAnimation();
    camera_.orbit.yawDeg = yawDeg;
    camera_.orbit.pitchDeg = app::ViewportCamera::NormalizePitch(pitchDeg);
    camera_.orbit.rollDeg = rollDeg;
    markViewChanged();
    update();
}

void GlView::orbitPanPixels(const QPoint& deltaPixels)
{
    orbitPanPixels(QPointF(deltaPixels));
}

void GlView::orbitPanPixels(const QPointF& deltaPixels)
{
    cancelViewAnimation();
    float panScaleX = 0.0025f * camera_.orbit.distance;
    float panScaleY = panScaleX;

    if (camera_.projectionType() == app::ViewportCamera::ProjectionType::Orthographic && width() > 0 && height() > 0)
    {
        const float dpr = devicePixelRatioF();
        const int pixelW = std::max(1, static_cast<int>(std::lround(static_cast<double>(width()) * dpr)));
        const int pixelH = std::max(1, static_cast<int>(std::lround(static_cast<double>(height()) * dpr)));
        camera_.SetCanvas(pixelW, pixelH);
        camera_.Update();

        const float worldW = camera_.orthoRight() - camera_.orthoLeft();
        const float worldH = camera_.orthoTop() - camera_.orthoBottom();
        panScaleX = worldW / static_cast<float>(width());
        panScaleY = worldH / static_cast<float>(height());
    }

    const app::ViewportCamera::OrbitBasis basis =
        app::ViewportCamera::BasisFromOrbit(
            camera_.orbit.yawDeg,
            camera_.orbit.pitchDeg,
            camera_.orbit.rollDeg);

    const QVector3D panDelta =
        (-static_cast<float>(deltaPixels.x()) * basis.right) * panScaleX
        + (static_cast<float>(deltaPixels.y()) * basis.up) * panScaleY;
    camera_.orbit.pan += panDelta;

    markViewChanged();
    update();
}

void GlView::orbitViewOpposite()
{
    const float currentPitchDeg = camera_.orbit.pitchDeg;

    // When looking nearly straight up/down, flipping to the opposite view should not also spin the yaw.
    // This matches the expected "top <-> bottom" behavior.
    constexpr float kNearPoleCos = 0.0348994967f; // cos(88 degrees)
    const bool nearPole = std::abs(std::cos(deg2Rad(currentPitchDeg)))
        <= kNearPoleCos;

    const float targetYawDeg = nearPole ? camera_.orbit.yawDeg : (camera_.orbit.yawDeg + 180.0f);
    const float targetPitchDeg = app::ViewportCamera::NormalizePitch(-currentPitchDeg);
    const float targetRollDeg = camera_.orbit.rollDeg;
    startViewAnimation(targetYawDeg, targetPitchDeg, targetRollDeg, 180);
}

void GlView::orbitSetView(const float yawDeg, const float pitchDeg)
{
    startViewAnimation(yawDeg, pitchDeg, 0.0f, 180);
}

void GlView::orbitRoll(const float rollDeltaDeg)
{
    cancelViewAnimation();
    camera_.orbit.rollDeg += rollDeltaDeg;
    markViewChanged();
    update();
}

void GlView::orbitRotateLeft()
{
    constexpr float kStepDeg = 15.0f;
    orbitRotate(-kStepDeg, 0.0f);
}

void GlView::orbitRotateRight()
{
    constexpr float kStepDeg = 15.0f;
    orbitRotate(kStepDeg, 0.0f);
}

void GlView::orbitRotateUp()
{
    constexpr float kStepDeg = 15.0f;
    orbitRotate(0.0f, kStepDeg);
}

void GlView::orbitRotateDown()
{
    constexpr float kStepDeg = 15.0f;
    orbitRotate(0.0f, -kStepDeg);
}

void GlView::orbitPanLeft()
{
    constexpr int kPanPx = 48;
    orbitPanPixels(QPoint(kPanPx, 0));
}

void GlView::orbitPanRight()
{
    constexpr int kPanPx = 48;
    orbitPanPixels(QPoint(-kPanPx, 0));
}

void GlView::orbitPanUp()
{
    constexpr int kPanPx = 48;
    orbitPanPixels(QPoint(0, kPanPx));
}

void GlView::orbitPanDown()
{
    constexpr int kPanPx = 48;
    orbitPanPixels(QPoint(0, -kPanPx));
}

void GlView::orbitRollLeft()
{
    constexpr float kStepDeg = 15.0f;
    orbitRoll(kStepDeg);
}

void GlView::orbitRollRight()
{
    constexpr float kStepDeg = 15.0f;
    orbitRoll(-kStepDeg);
}

void GlView::orbitRollReset()
{
    cancelViewAnimation();
    camera_.orbit.rollDeg = 0.0f;
    markViewChanged();
    update();
}

void GlView::orbitViewFront()
{
    // Front view: look along +Y (world forward).
    orbitSetView(0.0f, 0.0f);
}

void GlView::orbitViewBack()
{
    // Back view: look along -Y.
    orbitSetView(180.0f, 0.0f);
}

void GlView::orbitViewRight()
{
    orbitSetView(90.0f, 0.0f);
}

void GlView::orbitViewLeft()
{
    orbitSetView(-90.0f, 0.0f);
}

void GlView::orbitViewTop()
{
    // Top view: +X right, +Y up.
    orbitSetView(0.0f, 90.0f);
}

void GlView::orbitViewBottom()
{
    // Bottom view: +X right, +Y up.
    orbitSetView(0.0f, -90.0f);
}

float GlView::lerpAngleDeg(const float aDeg, const float bDeg, const float t) noexcept
{
    auto wrap180 = [](float deg) noexcept -> float
    {
        deg = std::fmod(deg + 180.0f, 360.0f);
        if (deg < 0.0f)
        {
            deg += 360.0f;
        }
        return deg - 180.0f;
    };

    const float delta = wrap180(bDeg - aDeg);
    return aDeg + delta * t;
}

void GlView::startViewAnimation(const float yawDeg, const float pitchDeg, const float rollDeg, const int durationMs)
{
    viewAnim_.active = true;
    viewAnim_.startYawDeg = camera_.orbit.yawDeg;
    viewAnim_.startPitchDeg = camera_.orbit.pitchDeg;
    viewAnim_.startRollDeg = camera_.orbit.rollDeg;
    viewAnim_.targetYawDeg = yawDeg;
    viewAnim_.targetPitchDeg = app::ViewportCamera::NormalizePitch(pitchDeg);
    viewAnim_.targetRollDeg = rollDeg;
    viewAnim_.startPan = camera_.orbit.pan;
    viewAnim_.targetPan = camera_.orbit.pan;
    viewAnim_.startDistance = camera_.orbit.distance;
    viewAnim_.targetDistance = camera_.orbit.distance;
    viewAnim_.startOrthoScale = camera_.orthoScale();
    viewAnim_.targetOrthoScale = camera_.orthoScale();
    viewAnim_.durationMs = std::max(durationMs, 1);
    viewAnim_.preserveGridOverrides = false;
    viewAnim_.startedAt = std::chrono::steady_clock::now();

    if (viewAnimTimer_)
    {
        viewAnimTimer_->start();
    }

    update();
}

void GlView::cancelViewAnimation() noexcept
{
    if (!viewAnim_.active)
    {
        return;
    }

    viewAnim_.active = false;
    if (viewAnimTimer_)
    {
        viewAnimTimer_->stop();
    }
}

void GlView::tickViewAnimation()
{
    if (!viewAnim_.active)
    {
        if (viewAnimTimer_)
        {
            viewAnimTimer_->stop();
        }
        return;
    }

    const auto now = std::chrono::steady_clock::now();
    const auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(now - viewAnim_.startedAt).count();
    const float rawT = std::clamp(static_cast<float>(elapsedMs) / static_cast<float>(viewAnim_.durationMs), 0.0f, 1.0f);
    const float t = rawT * rawT * (3.0f - 2.0f * rawT); // smoothstep

    camera_.orbit.yawDeg = lerpAngleDeg(viewAnim_.startYawDeg, viewAnim_.targetYawDeg, t);
    camera_.orbit.pitchDeg = app::ViewportCamera::NormalizePitch(
        lerpAngleDeg(viewAnim_.startPitchDeg, viewAnim_.targetPitchDeg, t));
    camera_.orbit.rollDeg = viewAnim_.startRollDeg + (viewAnim_.targetRollDeg - viewAnim_.startRollDeg) * t;
    camera_.orbit.pan = viewAnim_.startPan + (viewAnim_.targetPan - viewAnim_.startPan) * t;
    camera_.orbit.distance = viewAnim_.startDistance + (viewAnim_.targetDistance - viewAnim_.startDistance) * t;

    if (camera_.projectionType() == app::ViewportCamera::ProjectionType::Orthographic)
    {
        const float orthoScale = viewAnim_.startOrthoScale + (viewAnim_.targetOrthoScale - viewAnim_.startOrthoScale) * t;
        camera_.SetOrthoScale(std::clamp(orthoScale, 0.01f, 1.0e6f));
    }

    markViewChanged(viewAnim_.preserveGridOverrides);
    update();

    if (rawT >= 1.0f)
    {
        if (viewAnim_.preserveGridOverrides)
        {
            // Manual rotation after F2 should still clear temporary grid
            // overrides when that preference is enabled. Anchor the reset
            // policy to the completed grid-aligned camera, not the pre-F2
            // camera or an intermediate animation sample.
            gridOverrideCamBasisValid_ = false;
            captureGridOverrideCameraBasis();
        }
        cancelViewAnimation();
    }
}

bool GlView::flyNavigationActive() const noexcept
{
    return flyNav_.active;
}

void GlView::cancelFlyNavigation()
{
    if (viewportMouseGesture_.action == ViewportMouseAction::flyLook)
    {
        cancelViewportMouseGesture();
    }
    else
    {
        stopFlyNavigation();
    }
}

void GlView::setFlyNavigationFast(const bool fast) noexcept
{
    if (!flyNav_.active)
    {
        return;
    }

    flyNav_.fastKeyHeld = fast;
    refreshFlyNavigationSpeedModifiers(flySpeedHeldModifiers_);
}

void GlView::setFlyNavigationSlow(const bool slow) noexcept
{
    if (flyNav_.active)
    {
        flyNav_.slowKeyHeld = slow;
        refreshFlyNavigationSpeedModifiers(flySpeedHeldModifiers_);
    }
}

void GlView::adjustFlyNavigationSpeedBySteps(const float steps)
{
    if ( !flyNav_.active || !std::isfinite(steps) || std::abs(steps) < 0.001f )
    {
        return;
    }

    flyNav_.speedMultiplier = std::clamp(
        flyNav_.speedMultiplier * std::pow(1.1f, steps), 0.01f, 100.0f);
    showToast(
        QString("Fly speed: x%1").arg(ui::formatUiNumber(flyNav_.speedMultiplier, 'f', 2)),
        900, ToastPlacement::Center, ToastGroup::FlySpeed);
}

void GlView::setFlyNavigationSpeedModifierBindings(
    const QList<QKeySequence>& fast, const QList<QKeySequence>& slow) noexcept
{
    const auto cacheAlternatives = [](const QList<QKeySequence>& shortcuts,
        std::array<Qt::KeyboardModifiers, 15>& alternatives, std::size_t& count)
    {
        alternatives.fill(Qt::NoModifier);
        count = 0;
        for (const QKeySequence& sequence : shortcuts)
        {
            const auto modifiers = ui::shortcuts::decodeModifierOnlyShortcut(sequence);
            if (!modifiers || *modifiers == Qt::NoModifier)
            {
                continue;
            }
            const auto end = alternatives.begin() + count;
            if (std::find(alternatives.begin(), end, *modifiers) == end
                && count < alternatives.size())
            {
                alternatives[count++] = *modifiers;
            }
        }
    };
    cacheAlternatives(fast, flyFastModifierAlternatives_, flyFastModifierAlternativeCount_);
    cacheAlternatives(slow, flySlowModifierAlternatives_, flySlowModifierAlternativeCount_);
    if (flyNav_.active)
    {
        refreshFlyNavigationSpeedModifiers(QGuiApplication::keyboardModifiers());
    }
}

void GlView::refreshFlyNavigationSpeedModifiers(const Qt::KeyboardModifiers held) noexcept
{
    if (!flyNav_.active)
    {
        return;
    }
    flySpeedHeldModifiers_ = held & ui::shortcuts::editorModifierMask();
    const auto matches = [this](const std::array<Qt::KeyboardModifiers, 15>& alternatives,
        const std::size_t count)
    {
        return std::any_of(alternatives.begin(), alternatives.begin() + count,
            [this](const Qt::KeyboardModifiers required)
            {
                return (flySpeedHeldModifiers_ & required) == required;
            });
    };
    flyNav_.fast = flyNav_.fastKeyHeld
        || matches(flyFastModifierAlternatives_, flyFastModifierAlternativeCount_);
    flyNav_.slow = flyNav_.slowKeyHeld
        || matches(flySlowModifierAlternatives_, flySlowModifierAlternativeCount_);
}

void GlView::setFlyNavigationDirection(const FlyDirection dir, const bool pressed) noexcept
{
    if (!flyNav_.active)
    {
        return;
    }

    switch (dir)
    {
        case FlyDirection::Forward:
            flyNav_.forward = pressed;
            break;
        case FlyDirection::Backward:
            flyNav_.backward = pressed;
            break;
        case FlyDirection::Left:
            flyNav_.left = pressed;
            break;
        case FlyDirection::Right:
            flyNav_.right = pressed;
            break;
        case FlyDirection::Down:
            flyNav_.down = pressed;
            break;
        case FlyDirection::Up:
            flyNav_.up = pressed;
            break;
    }
}

} // namespace sed
