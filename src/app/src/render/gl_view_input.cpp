#include "gl_view_common.hpp"
#include "ui/components/ui_decimal_format.hpp"

#include <QKeyEvent>
#include <QLineEdit>
#include <QPointer>
#include <QStyleHints>

#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <map>
#include <tuple>
#include <unordered_set>
#include <utility>

#include "render/gizmos/light_category_gizmo_utils.hpp"
#include "render/gizmos/sector_thrust_gizmo_utils.hpp"
#include "render/picking/viewport_pick_cycle_utils.hpp"
#include "render/picking/viewport_ray_utils.hpp"
#include "render/selection/selection_geometry_utils.hpp"
#include "render/selection/surface_selection_utils.hpp"
#include "ui/shortcuts/selection_modifier_utils.hpp"
#include "ui/viewport/viewport_orbit_utils.hpp"

namespace sed
{
namespace
{

struct ProjectedLineSegment
{
    QPointF first{};
    QPointF second{};
    float firstDepth{ 1.0f };
    float secondDepth{ 1.0f };
};

struct ProjectedLinePoint
{
    QPointF screen{};
    float depth{ 1.0f };
    double parameter{};
    double distanceSquared{};
};

TVector rotateCleaveVectorAroundAxis(
    const TVector& value,
    const TVector& normalizedAxis,
    const double angleRadians) noexcept
{
    const double cosine = std::cos(angleRadians);
    const double sine = std::sin(angleRadians);
    return value * cosine
        + render::cleaveCross(normalizedAxis, value) * sine
        + normalizedAxis
            * (render::cleaveDot(normalizedAxis, value) * (1.0 - cosine));
}

QString cleaveTransformHudNumber(const double value)
{
    const QString number = QString::number(std::isfinite(value) ? value : 0.0, 'f', 2);
    return number == QStringLiteral("-0.00") ? QStringLiteral("0.00") : number;
}

double cleaveVectorLength(const TVector& value) noexcept
{
    return std::sqrt(std::max(0.0, render::cleaveDot(value, value)));
}

std::optional<ProjectedLineSegment> projectClippedLineSegment(
    const QVector3D& first,
    const QVector3D& second,
    const QMatrix4x4& mvp,
    const int viewportWidth,
    const int viewportHeight)
{
    if (viewportWidth <= 0 || viewportHeight <= 0)
    {
        return std::nullopt;
    }

    const QVector4D clipFirst = mvp * QVector4D(first, 1.0f);
    const QVector4D clipSecond = mvp * QVector4D(second, 1.0f);
    if (!std::isfinite(clipFirst.x()) || !std::isfinite(clipFirst.y())
        || !std::isfinite(clipFirst.z()) || !std::isfinite(clipFirst.w())
        || !std::isfinite(clipSecond.x()) || !std::isfinite(clipSecond.y())
        || !std::isfinite(clipSecond.z()) || !std::isfinite(clipSecond.w()))
    {
        return std::nullopt;
    }

    // Clip in homogeneous coordinates so a line crossing the near plane or
    // viewport edge remains selectable exactly where the renderer can draw it.
    double enter = 0.0;
    double leave = 1.0;
    const auto clipAgainstPlane = [&] (const double firstDistance,
                                      const double secondDistance) -> bool
    {
        if (!std::isfinite(firstDistance) || !std::isfinite(secondDistance))
        {
            return false;
        }
        if (firstDistance < 0.0 && secondDistance < 0.0)
        {
            return false;
        }
        if ((firstDistance < 0.0) != (secondDistance < 0.0))
        {
            const double parameter = firstDistance
                / (firstDistance - secondDistance);
            if (firstDistance < 0.0)
            {
                enter = std::max(enter, parameter);
            }
            else
            {
                leave = std::min(leave, parameter);
            }
        }
        return enter <= leave;
    };
    constexpr double kPositiveW = std::numeric_limits<float>::epsilon();
    const std::array<std::pair<double, double>, 7> clipPlanes{
        std::pair<double, double>{ clipFirst.x() + clipFirst.w(), clipSecond.x() + clipSecond.w() },
        std::pair<double, double>{ clipFirst.w() - clipFirst.x(), clipSecond.w() - clipSecond.x() },
        std::pair<double, double>{ clipFirst.y() + clipFirst.w(), clipSecond.y() + clipSecond.w() },
        std::pair<double, double>{ clipFirst.w() - clipFirst.y(), clipSecond.w() - clipSecond.y() },
        std::pair<double, double>{ clipFirst.z() + clipFirst.w(), clipSecond.z() + clipSecond.w() },
        std::pair<double, double>{ clipFirst.w() - clipFirst.z(), clipSecond.w() - clipSecond.z() },
        std::pair<double, double>{ clipFirst.w() - kPositiveW, clipSecond.w() - kPositiveW },
    };
    for (const auto& [firstDistance, secondDistance] : clipPlanes)
    {
        if (!clipAgainstPlane(firstDistance, secondDistance))
        {
            return std::nullopt;
        }
    }

    const QVector4D clippedFirst = clipFirst + (clipSecond - clipFirst)
        * static_cast<float>(enter);
    const QVector4D clippedSecond = clipFirst + (clipSecond - clipFirst)
        * static_cast<float>(leave);
    if (clippedFirst.w() <= 0.0f || clippedSecond.w() <= 0.0f)
    {
        return std::nullopt;
    }

    const QVector3D firstNdc = clippedFirst.toVector3D() / clippedFirst.w();
    const QVector3D secondNdc = clippedSecond.toVector3D() / clippedSecond.w();
    if (!std::isfinite(firstNdc.x()) || !std::isfinite(firstNdc.y())
        || !std::isfinite(firstNdc.z()) || !std::isfinite(secondNdc.x())
        || !std::isfinite(secondNdc.y()) || !std::isfinite(secondNdc.z()))
    {
        return std::nullopt;
    }

    return ProjectedLineSegment{
        QPointF(
            (static_cast<double>(firstNdc.x()) * 0.5 + 0.5) * viewportWidth,
            (1.0 - (static_cast<double>(firstNdc.y()) * 0.5 + 0.5)) * viewportHeight),
        QPointF(
            (static_cast<double>(secondNdc.x()) * 0.5 + 0.5) * viewportWidth,
            (1.0 - (static_cast<double>(secondNdc.y()) * 0.5 + 0.5)) * viewportHeight),
        firstNdc.z() * 0.5f + 0.5f,
        secondNdc.z() * 0.5f + 0.5f,
    };
}

std::optional<std::pair<double, double>> projectedLineRectInterval(
    const QPointF& first,
    const QPointF& second,
    const QRectF& rectangle)
{
    if (!std::isfinite(first.x()) || !std::isfinite(first.y())
        || !std::isfinite(second.x()) || !std::isfinite(second.y()))
    {
        return std::nullopt;
    }

    const QRectF box = rectangle.normalized();
    const double dx = second.x() - first.x();
    const double dy = second.y() - first.y();
    double enter = 0.0;
    double leave = 1.0;
    const auto clipBoundary = [&enter, &leave](
        const double direction,
        const double distance) -> bool
    {
        if (std::abs(direction) <= 1.0e-12)
        {
            return distance >= -1.0e-9;
        }
        const double parameter = distance / direction;
        if (direction < 0.0)
        {
            enter = std::max(enter, parameter);
        }
        else
        {
            leave = std::min(leave, parameter);
        }
        return enter <= leave + 1.0e-9;
    };
    if (!clipBoundary(-dx, first.x() - box.left())
        || !clipBoundary(dx, box.right() - first.x())
        || !clipBoundary(-dy, first.y() - box.top())
        || !clipBoundary(dy, box.bottom() - first.y()))
    {
        return std::nullopt;
    }

    enter = std::clamp(enter, 0.0, 1.0);
    leave = std::clamp(leave, 0.0, 1.0);
    if (enter > leave + 1.0e-9)
    {
        return std::nullopt;
    }
    return std::pair<double, double>{ enter, leave };
}

std::optional<ProjectedLinePoint> closestPointOnProjectedSegment(
    const ProjectedLineSegment& segment,
    const QPointF& point)
{
    const QPointF delta = segment.second - segment.first;
    const double lengthSquared = delta.x() * delta.x() + delta.y() * delta.y();
    double parameter = 0.0;
    if (lengthSquared > 1.0e-12)
    {
        const QPointF fromFirst = point - segment.first;
        parameter = std::clamp(
            (fromFirst.x() * delta.x() + fromFirst.y() * delta.y()) / lengthSquared,
            0.0,
            1.0);
    }

    const QPointF closest = segment.first + delta * parameter;
    const QPointF offset = point - closest;
    const double distanceSquared = offset.x() * offset.x() + offset.y() * offset.y();
    const float depth = static_cast<float>(
        segment.firstDepth + (segment.secondDepth - segment.firstDepth) * parameter);
    if (!std::isfinite(closest.x()) || !std::isfinite(closest.y())
        || !std::isfinite(distanceSquared) || !std::isfinite(depth)
        || depth < 0.0f || depth > 1.0f)
    {
        return std::nullopt;
    }
    return ProjectedLinePoint{ closest, depth, parameter, distanceSquared };
}

} // namespace

std::optional<GlView::ArchitecturalLightingRenderVertex>
GlView::architecturalLightingVertexAt(
    const QPoint& widgetPosition,
    const QMatrix4x4& mvp,
    const bool includeOccluded)
{
    if (renderPipeline_ != RenderPipeline::Editor
        || !architecturalLightingEditingActive_
        || architecturalLightingSelectedOwnerCaches_.empty()
        || width() <= 0
        || height() <= 0)
    {
        return std::nullopt;
    }

    struct Candidate
    {
        const ArchitecturalLightingRenderVertex* vertex{};
        QPointF screen{};
        float distanceSquared{ std::numeric_limits<float>::infinity() };
        float depth{ std::numeric_limits<float>::infinity() };
    };

    const QPointF pointer(widgetPosition);
    std::vector<Candidate> candidates{};
    const float pickRadiusPx = architecturalLightingMarkerHitRadiusPx();
    const float pickRadiusSquared = pickRadiusPx * pickRadiusPx;

    for (const ArchitecturalLightingRenderCache& ownerCache
         : architecturalLightingSelectedOwnerCaches_)
    {
        const std::span<const ArchitecturalLightingRenderVertex> vertices =
            architecturalLightingVerticesForThing(ownerCache.thingIndex);
        for (const ArchitecturalLightingRenderVertex& vertex : vertices)
        {
            const QVector4D clip = mvp * QVector4D(vertex.worldPosition, 1.0f);
            if (!std::isfinite(clip.x()) || !std::isfinite(clip.y())
                || !std::isfinite(clip.z()) || !std::isfinite(clip.w())
                || clip.w() <= std::numeric_limits<float>::epsilon())
            {
                continue;
            }

            const QVector3D ndc = clip.toVector3D() / clip.w();
            if (!std::isfinite(ndc.x()) || !std::isfinite(ndc.y())
                || !std::isfinite(ndc.z()) || ndc.z() < -1.0f
                || ndc.z() > 1.0f)
            {
                continue;
            }

            const QPointF screen(
                (static_cast<double>(ndc.x()) * 0.5 + 0.5)
                    * static_cast<double>(width()),
                (1.0 - (static_cast<double>(ndc.y()) * 0.5 + 0.5))
                    * static_cast<double>(height()));
            const double dx = screen.x() - pointer.x();
            const double dy = screen.y() - pointer.y();
            const float distanceSquared = static_cast<float>(dx * dx + dy * dy);
            if (!std::isfinite(distanceSquared)
                || distanceSquared > pickRadiusSquared)
            {
                continue;
            }
            candidates.push_back(Candidate{
                &vertex,
                screen,
                distanceSquared,
                ndc.z() * 0.5f + 0.5f });
        }
    }

    if (candidates.empty())
    {
        return std::nullopt;
    }

    if (!includeOccluded)
    {
        // ARCH markers are rasterized as camera-facing points. The user clicks
        // a pixel within that point footprint, so test visibility at the
        // actual click pixel rather than at the vertex center. On a sloped
        // model or a partly covered marker the two pixels can have different
        // scene depths; the click-pixel test matches the rendered fragment
        // while still rejecting a solid wall at the cursor. Keep the shared
        // 4-ULP depth tolerance; no world- or NDC-wide bias is added here.
        const std::vector<float> sceneDepths = pickSceneDepthsAtPoints(
            mvp,
            std::vector<QPointF>{ pointer },
            true,
            true,
            render::picking::solidLevelOccludersCullBackFaces(
                render::picking::SolidLevelOcclusionPickFamily::Thing,
                false));
        const float sceneDepth =
            sceneDepths.empty() ? 1.0f : sceneDepths.front();
        std::vector<Candidate> visible{};
        visible.reserve(candidates.size());
        for (const Candidate& candidate : candidates)
        {
            if (render::picking::surfaceSelectionCandidatePassesSceneDepth(
                    candidate.depth,
                    sceneDepth,
                    false,
                    false))
            {
                visible.push_back(candidate);
            }
        }
        candidates = std::move(visible);
        if (candidates.empty())
        {
            return std::nullopt;
        }
    }

    const auto best = std::min_element(
        candidates.begin(),
        candidates.end(),
        [](const Candidate& lhs, const Candidate& rhs)
        {
            if (lhs.distanceSquared != rhs.distanceSquared)
            {
                return lhs.distanceSquared < rhs.distanceSquared;
            }
            return lhs.depth < rhs.depth;
        });
    return best == candidates.end() || !best->vertex
        ? std::nullopt
        : std::optional<ArchitecturalLightingRenderVertex>{ *best->vertex };
}

std::vector<GlView::ArchitecturalLightingVertexIdentity>
GlView::architecturalLightingVerticesInScreenRect(
    const QRect& widgetRect,
    const QMatrix4x4& mvp,
    const bool includeOccluded)
{
    std::vector<ArchitecturalLightingVertexIdentity> result{};
    if (renderPipeline_ != RenderPipeline::Editor
        || !architecturalLightingEditingActive_
        || architecturalLightingSelectedOwnerCaches_.empty()
        || width() <= 0
        || height() <= 0)
    {
        return result;
    }

    struct Candidate
    {
        ArchitecturalLightingVertexIdentity identity{};
        QPointF screen{};
        float depth{ std::numeric_limits<float>::infinity() };
    };
    const QRect normalized = widgetRect.normalized();
    std::vector<Candidate> candidates{};
    for (const ArchitecturalLightingRenderCache& ownerCache
         : architecturalLightingSelectedOwnerCaches_)
    {
        const std::span<const ArchitecturalLightingRenderVertex> vertices =
            architecturalLightingVerticesForThing(ownerCache.thingIndex);
        for (const ArchitecturalLightingRenderVertex& vertex : vertices)
        {
            const QVector4D clip = mvp * QVector4D(vertex.worldPosition, 1.0f);
            if (!std::isfinite(clip.x()) || !std::isfinite(clip.y())
                || !std::isfinite(clip.z()) || !std::isfinite(clip.w())
                || clip.w() <= std::numeric_limits<float>::epsilon())
            {
                continue;
            }
            const QVector3D ndc = clip.toVector3D() / clip.w();
            if (!std::isfinite(ndc.x()) || !std::isfinite(ndc.y())
                || !std::isfinite(ndc.z()) || ndc.z() < -1.0f
                || ndc.z() > 1.0f)
            {
                continue;
            }
            const QPointF screen(
                (static_cast<double>(ndc.x()) * 0.5 + 0.5)
                    * static_cast<double>(width()),
                (1.0 - (static_cast<double>(ndc.y()) * 0.5 + 0.5))
                    * static_cast<double>(height()));
            if (!normalized.contains(screen.toPoint()))
            {
                continue;
            }
            candidates.push_back(Candidate{
                architecturalLightingVertexIdentity(vertex),
                screen,
                ndc.z() * 0.5f + 0.5f});
        }
    }
    if (candidates.empty())
    {
        return result;
    }

    if (!includeOccluded)
    {
        std::vector<QPointF> samples{};
        samples.reserve(candidates.size());
        for (const Candidate& candidate : candidates)
        {
            samples.push_back(candidate.screen);
        }
        const std::vector<float> sceneDepths = pickSceneDepthsAtPoints(
            mvp,
            samples,
            true,
            true,
            render::picking::solidLevelOccludersCullBackFaces(
                render::picking::SolidLevelOcclusionPickFamily::Thing,
                false));
        std::vector<Candidate> visible{};
        visible.reserve(candidates.size());
        for (std::size_t index = 0; index < candidates.size(); ++index)
        {
            if (index < sceneDepths.size()
                && render::picking::surfaceSelectionCandidatePassesSceneDepth(
                    candidates[index].depth,
                    sceneDepths[index],
                    false,
                    false))
            {
                visible.push_back(candidates[index]);
            }
        }
        candidates = std::move(visible);
    }

    std::unordered_set<ArchitecturalLightingVertexIdentity,
        ArchitecturalLightingVertexIdentityHash> seen{};
    seen.reserve(candidates.size());
    result.reserve(candidates.size());
    for (const Candidate& candidate : candidates)
    {
        if (seen.insert(candidate.identity).second)
        {
            result.push_back(candidate.identity);
        }
    }
    return result;
}

void GlView::applyArchitecturalLightingVertexSelection(
    const std::vector<ArchitecturalLightingVertexIdentity>& vertices,
    const SelectionBoxMode mode,
    const bool implicitOwnerExpansion)
{
    std::vector<ArchitecturalLightingVertexIdentity> next(
        architecturalLightingSelectedVertices_.begin(),
        architecturalLightingSelectedVertices_.end());
    if (mode == SelectionBoxMode::Replace)
    {
        next.clear();
    }

    if (mode == SelectionBoxMode::Remove)
    {
        const std::unordered_set<ArchitecturalLightingVertexIdentity,
            ArchitecturalLightingVertexIdentityHash> removed(
            vertices.begin(),
            vertices.end());
        next.erase(
            std::remove_if(
                next.begin(),
                next.end(),
                [&removed](const ArchitecturalLightingVertexIdentity& identity)
                {
                    return removed.contains(identity);
                }),
            next.end());
    }
    else
    {
        next.insert(next.end(), vertices.begin(), vertices.end());
    }
    setArchitecturalLightingSelectedVertices(next);
    architecturalLightingSelectedVerticesImplicitOwnerExpansion_ =
        implicitOwnerExpansion && mode != SelectionBoxMode::Remove
        && !architecturalLightingSelectedVertices_.empty();
}

void GlView::applyArchitecturalLightingVertexClick(
    const ArchitecturalLightingVertexIdentity& vertex,
    const Qt::KeyboardModifiers modifiers)
{
    const Qt::KeyboardModifiers held = modifiers
        & (Qt::ShiftModifier | Qt::ControlModifier | Qt::AltModifier
            | Qt::MetaModifier);
    const auto modifierActive = [held](const ui::shortcuts::ModifierBindings& required)
    {
        return ui::shortcuts::modifierBindingsActive(held, required);
    };
    SelectionBoxMode mode = SelectionBoxMode::Replace;
    if (modifierActive(selectionRemoveModifiers_))
    {
        mode = SelectionBoxMode::Remove;
    }
    else if (modifierActive(selectionAddModifiers_))
    {
        mode = SelectionBoxMode::Add;
    }
    if (architecturalLightingSelectedVerticesImplicitOwnerExpansion_
        && mode == SelectionBoxMode::Add)
    {
        // Shift-click starts an explicit vertex subset after an owner/body
        // expansion. Subsequent Shift-clicks add to that exact subset.
        mode = SelectionBoxMode::Replace;
    }
    else if (mode == SelectionBoxMode::Replace
        && architecturalLightingSelectedVertexSet_.contains(vertex))
    {
        if (!architecturalLightingSelectedVerticesImplicitOwnerExpansion_)
        {
            // A plain click on an explicitly selected vertex is also the
            // first release of Qt's double-click sequence. Keep the exact
            // batch intact so the following edit request can open it as a
            // group.
            return;
        }
        // The current set came from an owner/body click. A plain vertex
        // click changes the target from that implicit owner expansion to
        // the exact source vertex under the pointer.
        mode = SelectionBoxMode::Replace;
    }
    applyArchitecturalLightingVertexSelection({ vertex }, mode, false);
}

#ifdef SED_APP_MAIN_WINDOW_TEST_ACCESS
void GlView::beginBakedLightTransformHandleClickForTest(
    const std::int32_t lightIndex,
    const QPoint& pressPosition)
{
    restoreTransformSurfaceUvPreview();
    transformDrag_ = {};
    transformDrag_.active = true;
    transformDrag_.target = TransformTargetKind::Light;
    transformDrag_.handle = TransformHandle::MoveFree;
    transformDrag_.lightOwner = lightIndex;
    transformDrag_.lightKind = render::LightGizmoKind::Baked;
    transformDrag_.startMouse = pressPosition;
}

void GlView::beginSectorScaleTransformForTest(const float scale, const TVector& pivot)
{
    restoreTransformSurfaceUvPreview();
    transformDrag_ = {};
    transformDrag_.active = true;
    transformDrag_.target = TransformTargetKind::Sector;
    transformDrag_.handle = TransformHandle::ScaleUniform;
    transformDrag_.scaleFactor = scale;
    transformDrag_.documentPivot = pivot;
    transformDrag_.pivot = QVector3D(static_cast<float>(pivot.x), static_cast<float>(pivot.y), static_cast<float>(pivot.z));
    transformPreviewMatrix_.setToIdentity();
    transformPreviewMatrix_.scale(scale);
}

void GlView::beginThingMoveTransformForTest(const QVector3D& translation)
{
    restoreTransformSurfaceUvPreview();
    transformDrag_ = {};
    transformDrag_.active = true;
    transformDrag_.target = TransformTargetKind::Thing;
    transformDrag_.handle = TransformHandle::MoveFree;
    transformDrag_.translation = translation;
    transformPreviewMatrix_.setToIdentity();
    transformPreviewMatrix_.translate(translation);
    transformPreviewIdCount_ = 0;
    if (selectedThing_ >= 0
        && static_cast<std::size_t>(selectedThing_) < thingPickIds_.size())
    {
        const int pickId = static_cast<int>(
            thingPickIds_[static_cast<std::size_t>(selectedThing_)]);
        if (pickId > 0)
        {
            transformPreviewIds_[0] = pickId;
            transformPreviewPayloads_[0] = QVector3D(1.0f, 1.0f, 1.0f);
            transformPreviewIdCount_ = 1;
        }
    }
    transformPreviewActive_ = transformPreviewIdCount_ > 0;
    Q_EMIT transformPreviewChanged(
        true,
        TransformTool::Move,
        translation,
        QVector3D{},
        0.0,
        QVector3D{},
        1.0);
}

bool GlView::beginThingMoveDragForTest(
    const std::vector<std::int32_t>& thingIndices,
    const bool surfaceStick,
    const bool thingSnap)
{
    if (!level_ || thingIndices.empty() || width() <= 0 || height() <= 0)
    {
        return false;
    }

    editor::Selection selection{};
    std::int32_t primary = -1;
    for (const std::int32_t thingIndex : thingIndices)
    {
        if (thingIndex < 0
            || static_cast<std::size_t>(thingIndex) >= level_->things.size())
        {
            continue;
        }
        if (primary < 0)
        {
            primary = thingIndex;
        }
        selection.things.insert(thingIndex);
    }
    if (primary < 0 || selection.things.empty())
    {
        return false;
    }

    setTransformTool(TransformTool::Move);
    setSelectionState(-1, -1, primary, -1, selection);
    transformGridSnapEnabled_ = false;
    transformGridPlaneStickEnabled_ = false;
    geometryTransformSnapEnabled_ = false;
    thingMoveSnapToThingEnabled_ = thingSnap;
    thingMoveSnapRotateEnabled_ = false;
    thingMoveSurfaceStickEnabled_ = surfaceStick;
    thingMoveSurfaceAlignEnabled_ = false;
    camera_.SetProjectionType(
        app::ViewportCamera::ProjectionType::Orthographic);

    const Thing& primaryThing = level_->things[static_cast<std::size_t>(primary)];
    const QVector3D target(
        static_cast<float>(primaryThing.x),
        static_cast<float>(primaryThing.y),
        static_cast<float>(primaryThing.z));
    QMatrix4x4 projection{};
    projection.ortho(
        -0.0078125f,
        0.0078125f,
        -0.0078125f,
        0.0078125f,
        0.25f,
        20.0f);
    QMatrix4x4 view{};
    view.translate(-target.x(), -target.y(), -target.z() - 10.0f);

    resetThingMoveHotPathCountersForTest();
    const bool began = beginTransformDrag(
        QPoint(width() / 2, height() / 2),
        projection,
        view,
        target + QVector3D(0.0f, 0.0f, 10.0f),
        target,
        20.0f);
    return began;
}

bool GlView::beginFrameMoveDragForTest(
    const std::int32_t thingIndex,
    const std::int32_t valueIndex,
    const bool surfaceStick,
    const bool thingSnap)
{
    if (!level_ || !frameEditEnabled_ || width() <= 0 || height() <= 0
        || thingIndex < 0
        || static_cast<std::size_t>(thingIndex) >= level_->things.size())
    {
        return false;
    }

    const Thing& owner = level_->things[static_cast<std::size_t>(thingIndex)];
    if (valueIndex < 0 || static_cast<std::size_t>(valueIndex) >= owner.values.size())
    {
        return false;
    }
    const std::optional<Thing> framePose = sed::thing::thingAtFramePose(
        owner, owner.values[static_cast<std::size_t>(valueIndex)]);
    if (!framePose)
    {
        return false;
    }

    editor::Selection selection{};
    selection.things.insert(thingIndex);
    selection.frames.insert(sed::level::FrameRef{ thingIndex, valueIndex });
    setTransformTool(TransformTool::Move);
    setSelectionState(-1, -1, thingIndex, -1, selection);
    setSelectedFrameValueIndex(valueIndex);
    transformGridSnapEnabled_ = false;
    transformGridPlaneStickEnabled_ = false;
    geometryTransformSnapEnabled_ = false;
    thingMoveSnapToThingEnabled_ = thingSnap;
    thingMoveSnapRotateEnabled_ = false;
    thingMoveSurfaceStickEnabled_ = surfaceStick;
    thingMoveSurfaceAlignEnabled_ = false;
    camera_.SetProjectionType(
        app::ViewportCamera::ProjectionType::Orthographic);

    const QVector3D target(
        static_cast<float>(framePose->x),
        static_cast<float>(framePose->y),
        static_cast<float>(framePose->z));
    QMatrix4x4 projection{};
    projection.ortho(
        -0.0078125f,
        0.0078125f,
        -0.0078125f,
        0.0078125f,
        0.25f,
        20.0f);
    QMatrix4x4 view{};
    view.translate(-target.x(), -target.y(), -target.z() - 10.0f);

    resetThingMoveHotPathCountersForTest();
    const bool began = beginTransformDrag(
        QPoint(width() / 2, height() / 2),
        projection,
        view,
        target + QVector3D(0.0f, 0.0f, 10.0f),
        target,
        20.0f);
    return began && transformDrag_.target == TransformTargetKind::Frame;
}

void GlView::updateThingMoveDragSampleForTest(const int pointerSteps)
{
    if (!transformDrag_.active
        || transformDrag_.target != TransformTargetKind::Thing)
    {
        return;
    }

    const TVector target = transformDrag_.documentPivot.value_or(
        TVector{
            transformDrag_.pivot.x(),
            transformDrag_.pivot.y(),
            transformDrag_.pivot.z()});
    QMatrix4x4 projection{};
    projection.ortho(
        -0.0078125f,
        0.0078125f,
        -0.0078125f,
        0.0078125f,
        0.25f,
        20.0f);
    QMatrix4x4 view{};
    view.translate(
        -static_cast<float>(target.x),
        -static_cast<float>(target.y),
        -static_cast<float>(target.z) - 10.0f);
    updateTransformDrag(
        QPoint(width() / 2 + pointerSteps, height() / 2),
        projection,
        view,
        QVector3D(
            static_cast<float>(target.x),
            static_cast<float>(target.y),
            static_cast<float>(target.z) + 10.0f),
        QVector3D(
            static_cast<float>(target.x),
            static_cast<float>(target.y),
            static_cast<float>(target.z)),
        20.0f,
        Qt::NoModifier);
}

void GlView::updateFrameMoveDragSampleForTest(const int pointerSteps)
{
    if (!transformDrag_.active
        || transformDrag_.target != TransformTargetKind::Frame)
    {
        return;
    }

    const TVector target = transformDrag_.documentPivot.value_or(
        TVector{
            transformDrag_.pivot.x(),
            transformDrag_.pivot.y(),
            transformDrag_.pivot.z()});
    QMatrix4x4 projection{};
    projection.ortho(
        -0.0078125f,
        0.0078125f,
        -0.0078125f,
        0.0078125f,
        0.25f,
        20.0f);
    QMatrix4x4 view{};
    view.translate(
        -static_cast<float>(target.x),
        -static_cast<float>(target.y),
        -static_cast<float>(target.z) - 10.0f);
    updateTransformDrag(
        QPoint(width() / 2 + pointerSteps, height() / 2),
        projection,
        view,
        QVector3D(
            static_cast<float>(target.x),
            static_cast<float>(target.y),
            static_cast<float>(target.z) + 10.0f),
        QVector3D(
            static_cast<float>(target.x),
            static_cast<float>(target.y),
            static_cast<float>(target.z)),
        20.0f,
        Qt::NoModifier);
}

void GlView::updateThingMovePointerForTest(const float origin, const bool gridSnap, const bool surfaceStick, const bool thingSnap, const bool surfaceAlign, const int pointerSteps, const bool gridPlaneStick, const bool geometrySnap, const float yawDeg, const float pitchDeg, const bool requestedPerspective, const bool actualCamera)
{
    const bool perspective = actualCamera ? requestedPerspective : property("testPerspectivePointer").toBool();
    beginThingMoveTransformForTest(QVector3D{});
    transformDrag_.pivot = QVector3D(origin, origin, 0.0f);
    transformDrag_.documentStartHit = TVector{origin, origin, 0.0};
    if (level_ && selectedThing_ >= 0 && static_cast<std::size_t>(selectedThing_) < level_->things.size())
    {
        const Thing& thing = level_->things[static_cast<std::size_t>(selectedThing_)];
        transformDrag_.documentPivot = TVector{thing.x, thing.y, thing.z};
    }
    transformDrag_.axis = QVector3D(0.0f, 0.0f, 1.0f);
    transformDrag_.startHit = transformDrag_.pivot;
    transformDrag_.gridSnapReference = TVector{origin, origin, 0.0};
    transformDrag_.gridFrame = render::makeTransformGridFrame(QVector3D{},
        QVector3D(1.0f, 0.0f, 0.0f), QVector3D(0.0f, 1.0f, 0.0f), 1.0e-6);
    transformGridSnapEnabled_ = gridSnap;
    transformGridPlaneStickEnabled_ = gridPlaneStick;
    if (gridPlaneStick && level_ && selectedThing_ >= 0 && static_cast<std::size_t>(selectedThing_) < level_->things.size())
    {
        transformDrag_.gridFrame = render::makeTransformGridFrame(QVector3D(origin, origin, 0.0f),
            QVector3D(0.0f, 1.0f, 0.0f), QVector3D(0.0f, 0.0f, 1.0f), 1.0e-6);
        transformDrag_.gridStickPoints = thing::resolveThingDocumentSupportPoints(
            *level_, templates_, resources_, level_->things[static_cast<std::size_t>(selectedThing_)], thingCubeHalfSize_);
        gridPlaneStickRadius_ = 0.00005f;
    }
    geometryTransformSnapEnabled_ = actualCamera ? geometrySnap : false;
    thingMoveSnapToThingEnabled_ = thingSnap;
    thingMoveSnapRotateEnabled_ = true;
    thingMoveSurfaceAlignEnabled_ = surfaceAlign;
    thingMoveSurfaceStickEnabled_ = surfaceStick;
    camera_.SetProjectionType(perspective ? app::ViewportCamera::ProjectionType::Perspective
                                          : app::ViewportCamera::ProjectionType::Orthographic);
    if (actualCamera)
    {
        setOrbitViewImmediate(yawDeg, pitchDeg, 0.0f);
        QMatrix4x4 projection{};
        QMatrix4x4 view{};
        QVector3D eye{};
        QVector3D target{};
        float nearPlane{};
        float farPlane{};
        buildCameraMatrices(projection, view, eye, target, nearPlane, farPlane);
        const QPoint startPosition(width() / 2, height() / 2);
        const auto startRay = render::viewportRayForWidget(
            projection, view, eye, startPosition, width(), height(), !perspective);
        if (!startRay)
        {
            return;
        }
        {
            const QVector3D selectedPivot = transformDrag_.documentPivot
                ? QVector3D(
                    static_cast<float>(transformDrag_.documentPivot->x),
                    static_cast<float>(transformDrag_.documentPivot->y),
                    static_cast<float>(transformDrag_.documentPivot->z))
                : QVector3D(origin, origin, 0.0f);
            transformDrag_.pivot = selectedPivot;
            QVector3D planeNormal(
                static_cast<float>(-startRay->direction.x),
                static_cast<float>(-startRay->direction.y),
                static_cast<float>(-startRay->direction.z));
            if (planeNormal.lengthSquared() <= 1.0e-8f)
            {
                return;
            }
            {
                planeNormal.normalize();
                transformDrag_.axis = planeNormal;

                const TVector normal{
                    static_cast<double>(planeNormal.x()),
                    static_cast<double>(planeNormal.y()),
                    static_cast<double>(planeNormal.z())};
                const double denominator = startRay->direction.Dot(normal);
                const TVector pivot = transformDrag_.documentPivot.value_or(
                    TVector{ origin, origin, 0.0 });
                if (!std::isfinite(denominator) || std::abs(denominator) < 1.0e-6)
                {
                    return;
                }
                const double distance = (pivot - startRay->origin).Dot(normal)
                    / denominator;
                if (!std::isfinite(distance))
                {
                    return;
                }
                const TVector intersection = startRay->origin
                    + startRay->direction * distance;
                if (!std::isfinite(intersection.x)
                    || !std::isfinite(intersection.y)
                    || !std::isfinite(intersection.z))
                {
                    return;
                }
                transformDrag_.documentStartHit = intersection;
                transformDrag_.startHit = QVector3D(
                    static_cast<float>(intersection.x),
                    static_cast<float>(intersection.y),
                    static_cast<float>(intersection.z));
            }
            transformDrag_.gridSnapReference = TVector{
                selectedPivot.x(), selectedPivot.y(), selectedPivot.z()};
            transformDrag_.gridFrame = render::makeTransformGridFrame(
                selectedPivot,
                QVector3D(1.0f, 0.0f, 0.0f),
                QVector3D(0.0f, 1.0f, 0.0f),
                1.0 / 16.0);
        }
        updateTransformDrag(
            QPoint(width() / 2 + pointerSteps, height() / 2),
            projection,
            view,
            eye,
            target,
            farPlane,
            Qt::NoModifier);
        return;
    }
    QMatrix4x4 projection{};
    // The perspective near half-width is 1/2048 at z=1/2; the plane is
    // eight units from the eye, giving the same exact 1/64 world width.
    if (perspective)
        projection.frustum(-0.00048828125f, 0.00048828125f, -0.00048828125f, 0.00048828125f, 0.5f, 16.0f);
    else
        projection.ortho(-0.0078125f, 0.0078125f, -0.0078125f, 0.0078125f, 0.25f, 20.0f);
    const float eyeZ = perspective ? 8.0f : 10.0f;
    QMatrix4x4 view{};
    view.translate(-origin, -origin, -eyeZ);
    updateTransformDrag(QPoint(width() / 2 + pointerSteps, height() / 2), projection, view,
        QVector3D(origin, origin, eyeZ), QVector3D(origin, origin, 0.0f), 20.0f, Qt::NoModifier);
}

void GlView::updateThingMovePointerCameraForTest(
    const float origin,
    const bool gridSnap,
    const bool geometrySnap,
    const bool surfaceStick,
    const bool surfaceAlign,
    const float yawDeg,
    const float pitchDeg,
    const bool perspective,
    const float desiredScreenRightWorldDistance)
{
    camera_.SetProjectionType(perspective
        ? app::ViewportCamera::ProjectionType::Perspective
        : app::ViewportCamera::ProjectionType::Orthographic);
    setOrbitViewImmediate(yawDeg, pitchDeg, 0.0f);
    QMatrix4x4 projection{};
    QMatrix4x4 view{};
    QVector3D eye{};
    QVector3D target{};
    float nearPlane{};
    float farPlane{};
    buildCameraMatrices(projection, view, eye, target, nearPlane, farPlane);
    QVector3D source(origin, origin, 0.0f);
    if (level_ && selectedThing_ >= 0
        && static_cast<std::size_t>(selectedThing_) < level_->things.size())
    {
        const Thing& thing = level_->things[static_cast<std::size_t>(selectedThing_)];
        source = QVector3D(static_cast<float>(thing.x), static_cast<float>(thing.y), static_cast<float>(thing.z));
    }
    auto projectX = [&](const QVector3D& point) -> float
    {
        const QVector4D clip = projection * view * QVector4D(point, 1.0f);
        return std::abs(clip.w()) > 1.0e-6f
            ? (clip.x() / clip.w() + 1.0f) * static_cast<float>(width()) * 0.5f
            : static_cast<float>(width()) * 0.5f;
    };
    const QVector3D cameraRight(view(0, 0), view(0, 1), view(0, 2));
    const QVector3D targetPoint = source + cameraRight.normalized()
        * desiredScreenRightWorldDistance;
    const int pointerSteps = qRound(projectX(targetPoint) - projectX(source));
    updateThingMovePointerForTest(
        origin, gridSnap, surfaceStick, false, surfaceAlign, pointerSteps,
        false, geometrySnap, yawDeg, pitchDeg, perspective, true);
}

void GlView::beginThingRotateTransformForTest(
    const QVector3D& axis,
    const float angleDeg,
    const QVector3D& pivot)
{
    restoreTransformSurfaceUvPreview();
    transformDrag_ = {};
    transformDrag_.active = true;
    transformDrag_.target = TransformTargetKind::Thing;
    transformDrag_.handle = TransformHandle::RotateZ;
    transformDrag_.axis = axis;
    transformDrag_.angleDeg = angleDeg;
    transformDrag_.pivot = pivot;
    transformPreviewMatrix_.setToIdentity();
    transformPreviewMatrix_.translate(pivot);
    transformPreviewMatrix_.rotate(angleDeg, axis);
    transformPreviewMatrix_.translate(-pivot);
    transformPreviewIdCount_ = 0;
    if (selectedThing_ >= 0
        && static_cast<std::size_t>(selectedThing_) < thingPickIds_.size())
    {
        const int pickId = static_cast<int>(
            thingPickIds_[static_cast<std::size_t>(selectedThing_)]);
        if (pickId > 0)
        {
            transformPreviewIds_[0] = pickId;
            transformPreviewPayloads_[0] = QVector3D(1.0f, 1.0f, 1.0f);
            transformPreviewIdCount_ = 1;
        }
    }
    transformPreviewActive_ = transformPreviewIdCount_ > 0;
    Q_EMIT transformPreviewChanged(
        true,
        TransformTool::Rotate,
        QVector3D{},
        axis,
        static_cast<double>(angleDeg),
        pivot,
        1.0);
}

void GlView::endTransformDragForTest()
{
    endTransformDrag();
}
#endif

namespace
{

double cleavePointSegmentDistanceSquared(
    const QPointF& point,
    const QPointF& first,
    const QPointF& second) noexcept
{
    const QPointF segment = second - first;
    const double lengthSquared = segment.x() * segment.x() + segment.y() * segment.y();
    if (lengthSquared <= 1.0e-12)
    {
        const QPointF delta = point - first;
        return delta.x() * delta.x() + delta.y() * delta.y();
    }

    const QPointF fromFirst = point - first;
    const double t = std::clamp(
        (fromFirst.x() * segment.x() + fromFirst.y() * segment.y()) / lengthSquared,
        0.0,
        1.0);
    const QPointF closest = first + segment * t;
    const QPointF delta = point - closest;
    return delta.x() * delta.x() + delta.y() * delta.y();
}

std::optional<TVector> intersectCleaveWidgetRayWithPlane(
    const QPoint& position, const QMatrix4x4& projection, const QMatrix4x4& view,
    const QVector3D& eye, const bool orthographic, const int width, const int height,
    const TVector& planePoint, const TVector& planeNormal) noexcept
{
    const auto ray = render::viewportRayForWidget(projection, view, eye, position, width, height, orthographic);
    return ray ? render::cleaveIntersectRayPlane(*ray, planePoint, planeNormal) : std::nullopt;
}

} // namespace

render::TransformGridFrame GlView::activeTransformGridFrame(
    const QMatrix4x4& cameraView,
    const QMatrix4x4& mvp,
    const QMatrix4x4& inverseMvp,
    const QVector3D& eye,
    const QVector3D& target,
    const bool orthographic) const noexcept
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
        bool inverseViewOk = false;
        const QMatrix4x4 inverseView = cameraView.inverted(&inverseViewOk);
        if (!inverseViewOk)
        {
            return {};
        }
        const QVector4D right = inverseView.column(0);
        const QVector4D up = inverseView.column(1);
        const render::ViewAlignedGridFrame viewFrame =
            render::makeViewAlignedGridFrame(
                QVector3D(right.x(), right.y(), right.z()),
                QVector3D(up.x(), up.y(), up.z()),
                target - eye);
        axisA = viewFrame.axisA;
        axisB = viewFrame.axisB;
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

    QVector3D cleanA{};
    QVector3D cleanB{};
    QVector3D normal{};
    if (!render::orthonormalizeGridAxes(axisA, axisB, cleanA, cleanB, normal))
    {
        return {};
    }

    const bool floatingGrid = gridCenterOverride_.has_value()
        || gridAlignToView_ || gridRotateWithView_;
    const QVector3D defaultCenter = gridFollowCamera_ ? target : QVector3D{};
    const QVector3D gridCenter = gridCenterOverride_.value_or(defaultCenter);
    const QVector3D planePoint = floatingGrid
        ? gridCenter
        : cleanA * QVector3D::dotProduct(gridCenter, cleanA)
            + cleanB * QVector3D::dotProduct(gridCenter, cleanB);
    const render::ViewAlignedGridDisplayPlane viewAlignedDisplay =
        render::resolveViewAlignedGridDisplayPlane(
            gridCenter,
            eye,
            target - eye,
            camera_.nearPlane(),
            gridRotateWithView_ && gridKeepViewAlignedVisible_);
    // Keep the snap frame itself on the authored origin/plane. Only measure
    // adaptive cell size at the same optional camera-side display proxy used
    // by the renderer; otherwise the visible and snapped minor lattices can
    // select different zoom levels in perspective.
    const QVector3D spacingPlanePoint = gridRotateWithView_
        ? viewAlignedDisplay.planePoint
        : planePoint;

    float unitsPerPixel = 0.0f;
    const QPoint center(width() / 2, height() / 2);
    const std::optional<QVector3D> first = intersectWidgetRayWithPlane(
        center,
        inverseMvp,
        eye,
        orthographic,
        width(),
        height(),
        spacingPlanePoint,
        normal);
    const std::optional<QVector3D> second = intersectWidgetRayWithPlane(
        QPoint(center.x() + 1, center.y()),
        inverseMvp,
        eye,
        orthographic,
        width(),
        height(),
        spacingPlanePoint,
        normal);
    if (first && second)
    {
        unitsPerPixel = (*second - *first).length();
    }
    const float projectionUnitsPerPixel =
        (!std::isfinite(unitsPerPixel) || unitsPerPixel <= 0.0f)
            ? worldUnitsPerPixelAt(
                spacingPlanePoint, mvp, inverseMvp, width(), height())
            : 0.0f;
    unitsPerPixel = render::resolveGridUnitsPerPixel(
        unitsPerPixel,
        projectionUnitsPerPixel,
        orthographic,
        camera_.orthoScale(),
        height());

    const render::EffectiveGridSpacing spacing = render::effectiveGridSpacing(
        gridMajorSpacing_, gridMinorSpacing_, gridAdaptiveSpacing_, unitsPerPixel);
    return render::makeTransformGridFrame(
        floatingGrid ? gridCenter : QVector3D{}, cleanA, cleanB, spacing.minor);
}

double GlView::activeGridSnapSpacing()
{
    const render::TransformGridFrame frame = activeGridSnapFrame();
    return frame.valid && std::isfinite(frame.spacing) && frame.spacing > 0.0
        ? frame.spacing
        : static_cast<double>(gridMinorSpacing_);
}

render::TransformGridFrame GlView::activeGridSnapFrame()
{
    QMatrix4x4 projection{};
    QMatrix4x4 view{};
    QVector3D eye{};
    QVector3D target{};
    float nearPlane{};
    float farPlane{};
    buildCameraMatrices(
        projection, view, eye, target, nearPlane, farPlane);

    const QMatrix4x4 mvp = projection * view;
    bool inverseOk = false;
    const QMatrix4x4 inverseMvp = mvp.inverted(&inverseOk);
    if (!inverseOk)
    {
        return {};
    }

    return activeTransformGridFrame(
        view,
        mvp,
        inverseMvp,
        eye,
        target,
        camera_.projectionType()
            == app::ViewportCamera::ProjectionType::Orthographic);
}

void GlView::initializeStagedCleaveLineFrame(
    StagedCleaveLine& line,
    const TVector& preferredAngleReference) const noexcept
{
    TVector direction = line.second - line.first;
    if (!render::cleaveNormalize(direction) || !render::cleaveNormalize(line.planeNormal))
    {
        line.constructionPlaneNormal = {};
        line.angleReferenceAxis = {};
        return;
    }

    // D and N define a unique editing plane. Its normal C makes
    // cross(C, D) == N, so endpoint motion in that plane rotates the authored
    // cutting plane without consulting the current camera.
    line.constructionPlaneNormal = render::cleaveCross(direction, line.planeNormal);
    if (!render::cleaveNormalize(line.constructionPlaneNormal))
    {
        line.constructionPlaneNormal = {};
        line.angleReferenceAxis = direction;
        return;
    }

    line.angleReferenceAxis = preferredAngleReference
        - line.constructionPlaneNormal * render::cleaveDot(
            preferredAngleReference,
            line.constructionPlaneNormal);
    if (!render::cleaveNormalize(line.angleReferenceAxis))
    {
        line.angleReferenceAxis = direction;
    }
}

void GlView::refreshStagedCleaveLinePlane(StagedCleaveLine& line) const noexcept
{
    TVector direction = line.second - line.first;
    if (!render::cleaveNormalize(direction))
    {
        return;
    }

    TVector constructionNormal = line.constructionPlaneNormal;
    if (!render::cleaveNormalize(constructionNormal))
    {
        initializeStagedCleaveLineFrame(line, line.angleReferenceAxis);
        return;
    }

    TVector nextNormal = render::cleaveCross(constructionNormal, direction);
    if (!render::cleaveNormalize(nextNormal))
    {
        return;
    }
    if (render::cleaveDot(nextNormal, line.planeNormal) < 0.0f)
    {
        nextNormal = -nextNormal;
        constructionNormal = -constructionNormal;
    }
    line.planeNormal = nextNormal;
    line.constructionPlaneNormal = constructionNormal;

    line.angleReferenceAxis = line.angleReferenceAxis - constructionNormal * render::cleaveDot(
        line.angleReferenceAxis,
        constructionNormal);
    if (!render::cleaveNormalize(line.angleReferenceAxis))
    {
        line.angleReferenceAxis = direction;
    }
}

TVector GlView::snapStagedCleaveEndpointToAngleStep(
    const TVector& fixed,
    const TVector& candidate,
    const TVector& constructionPlaneNormal,
    const TVector& fallbackReference) const noexcept
{
    if (!cleaveStagedMode_ || cleaveAngleStepDegrees_ <= 0.0)
    {
        return candidate;
    }

    const bool editing = editingStagedCleaveLine_ >= 0
        && static_cast<std::size_t>(editingStagedCleaveLine_) < stagedCleaveLines_.size();
    const bool movingFirst = editing && editingStagedCleaveEndpoint_ == 0;
    TVector direction = movingFirst
        ? fixed - candidate
        : candidate - fixed;
    TVector reference = fallbackReference;

    if (editing)
    {
        const std::size_t index = static_cast<std::size_t>(editingStagedCleaveLine_);
        const StagedCleaveLine& line = stagedCleaveLines_[index];
        reference = line.angleReferenceAxis;
        // Alt-ripping the first endpoint removes its previous-neighbor
        // topology on commit, so do not let that former segment steer the
        // disjointed endpoint's angle snap during the live drag.
        if (!(editingDisjointStagedCleaveEndpoint_ && movingFirst)
            && line.linkedToPrevious
            && stagedCleaveLines_.size() > 1)
        {
            const std::size_t previous = index == 0
                ? stagedCleaveLines_.size() - 1
                : index - 1;
            reference = stagedCleaveLines_[previous].first - line.first;
        }
    }
    else if ( cleaveChainEnabled_ && !stagedCleaveLines_.empty() )
    {
        reference = stagedCleaveChainOriginIncomingDirection(stagedCleaveLines_);
    }

    const TVector snapped = render::cleaveSnapDirectionToAngleStep(
        direction,
        reference,
        constructionPlaneNormal,
        cleaveAngleStepDegrees_);
    return movingFirst ? fixed - snapped : fixed + snapped;
}

bool GlView::reshapeStagedCleaveEndpoint(
    const std::size_t index,
    const std::int32_t endpoint,
    const TVector& position)
{
    return reshapeStagedCleaveEndpointWithOptionalDisjoint(
        stagedCleaveLines_,
        index,
        endpoint,
        position,
        false);
}

bool GlView::reshapeStagedCleaveEndpointIn(
    std::vector<StagedCleaveLine>& lines,
    const std::size_t index,
    const std::int32_t endpoint,
    const TVector& position) const
{
    if (index >= lines.size() || (endpoint != 0 && endpoint != 1))
    {
        return false;
    }
    StagedCleaveLine& edited = lines[index];
    const TVector fixed = endpoint == 0 ? edited.second : edited.first;
    if (!render::cleaveFinitePoint(position) || render::cleaveSameHandle(position, fixed))
    {
        return false;
    }
    if (endpoint == 0)
    {
        edited.first = position;
    }
    else
    {
        edited.second = position;
    }
    refreshStagedCleaveLinePlane(edited);

    if (endpoint == 0 && edited.linkedToPrevious && lines.size() > 1)
    {
        const std::size_t previous = index == 0
            ? lines.size() - 1
            : index - 1;
        lines[previous].second = edited.first;
        refreshStagedCleaveLinePlane(lines[previous]);
    }
    if (endpoint == 1 && edited.linkedToNext && lines.size() > 1)
    {
        const std::size_t next = (index + 1) % lines.size();
        lines[next].first = edited.second;
        refreshStagedCleaveLinePlane(lines[next]);
    }
    return true;
}

bool GlView::disjointStagedCleaveEndpointIn(
    std::vector<StagedCleaveLine>& lines,
    const std::size_t index,
    const std::int32_t endpoint) const
{
    if (index >= lines.size() || (endpoint != 0 && endpoint != 1))
    {
        return false;
    }

    StagedCleaveLine& edited = lines[index];
    if (endpoint == 0 && lines.size() > 1)
    {
        const std::size_t previous = index == 0 ? lines.size() - 1 : index - 1;
        const bool wasLinked = edited.linkedToPrevious || lines[previous].linkedToNext;
        edited.linkedToPrevious = false;
        lines[previous].linkedToNext = false;
        return wasLinked;
    }
    if (endpoint == 1 && lines.size() > 1)
    {
        const std::size_t next = (index + 1) % lines.size();
        const bool wasLinked = edited.linkedToNext || lines[next].linkedToPrevious;
        edited.linkedToNext = false;
        lines[next].linkedToPrevious = false;
        return wasLinked;
    }
    return false;
}

bool GlView::reshapeStagedCleaveEndpointWithOptionalDisjoint(
    std::vector<StagedCleaveLine>& lines,
    const std::size_t index,
    const std::int32_t endpoint,
    const TVector& position,
    const bool disjointSelected) const
{
    if (index >= lines.size() || (endpoint != 0 && endpoint != 1))
    {
        return false;
    }

    // Endpoint edits can update reciprocal chain bits and any independently
    // authored lines that share the exact same endpoint. Stage that complete
    // operation so an invalid/collapsed collateral segment cannot leave half
    // of a joint mutation behind.
    std::vector<StagedCleaveLine> candidate = lines;

    std::vector<std::pair<std::size_t, std::int32_t>> coincidentEndpoints{};
    if (!disjointSelected)
    {
        const TVector originalPosition = endpoint == 0
            ? candidate[index].first
            : candidate[index].second;
        coincidentEndpoints.reserve(candidate.size());
        for (std::size_t lineIndex = 0; lineIndex < candidate.size(); ++lineIndex)
        {
            if (!(lineIndex == index && endpoint == 0)
                && render::cleaveSameHandle(candidate[lineIndex].first, originalPosition))
            {
                coincidentEndpoints.emplace_back(lineIndex, 0);
            }
            if (!(lineIndex == index && endpoint == 1)
                && render::cleaveSameHandle(candidate[lineIndex].second, originalPosition))
            {
                coincidentEndpoints.emplace_back(lineIndex, 1);
            }
        }
    }
    else
    {
        (void)disjointStagedCleaveEndpointIn(
            candidate,
            index,
            endpoint);
    }

    if (!reshapeStagedCleaveEndpointIn(candidate, index, endpoint, position))
    {
        return false;
    }

    // Snapped endpoints form one manipulation handle even when their lines
    // are not adjacent in the authored chain order. Alt intentionally skips
    // this propagation and therefore rips only the picked endpoint away.
    for (const auto [lineIndex, coincidentEndpoint] : coincidentEndpoints)
    {
        StagedCleaveLine& coincident = candidate[lineIndex];
        if (coincidentEndpoint == 0)
        {
            coincident.first = position;
        }
        else
        {
            coincident.second = position;
        }
        refreshStagedCleaveLinePlane(coincident);
    }

    // A normal endpoint snap recreates chain topology. Spatial coincidence is
    // also used as the shared manipulation handle above, while the explicit
    // bits remain restricted to the authored neighbour used by chain Apply.
    // Alt deliberately suppresses both behaviors.
    if (!disjointSelected && lines.size() > 1)
    {
        StagedCleaveLine& edited = candidate[index];
        if (endpoint == 0)
        {
            const std::size_t previous = index == 0 ? candidate.size() - 1 : index - 1;
            if (render::cleaveSameHandle(edited.first, candidate[previous].second))
            {
                edited.linkedToPrevious = true;
                candidate[previous].linkedToNext = true;
            }
        }
        else
        {
            const std::size_t next = (index + 1) % candidate.size();
            if (render::cleaveSameHandle(edited.second, candidate[next].first))
            {
                edited.linkedToNext = true;
                candidate[next].linkedToPrevious = true;
            }
        }
    }

    for (const StagedCleaveLine& line : candidate)
    {
        if (!render::cleaveFinitePoint(line.first)
            || !render::cleaveFinitePoint(line.second)
            || render::cleaveSameHandle(line.first, line.second)
            || !render::cleaveFinitePoint(line.planeNormal)
            || line.planeNormal.lengthSqr() == 0.0)
        {
            return false;
        }
    }
    lines = std::move(candidate);
    return true;
}

bool GlView::translateStagedCleavePlane(
    const std::size_t index,
    const TVector& translation,
    const StagedCleavePlaneDrag* const drag)
{
    const std::vector<StagedCleaveLine>& baselineLines = drag
        ? stagedCleaveGestureBaselineLines()
        : stagedCleaveLines_;
    if (index >= baselineLines.size() || !render::cleaveFinitePoint(translation))
    {
        return false;
    }

    const TVector originalFirst = baselineLines[index].first;
    const TVector originalSecond = baselineLines[index].second;
    if ( !render::cleaveFinitePoint(originalFirst)
        || !render::cleaveFinitePoint(originalSecond)
        || render::cleaveSameHandle(originalFirst, originalSecond) )
    {
        return false;
    }

    std::vector<StagedCleaveLineMove> fallbackAffectedLines{};
    const std::vector<StagedCleaveLineMove>* affectedLines = nullptr;
    if (drag)
    {
        affectedLines = &drag->affectedLines;
    }
    else
    {
        fallbackAffectedLines.reserve(baselineLines.size());
        for (std::size_t lineIndex = 0; lineIndex < baselineLines.size(); ++lineIndex)
        {
            const StagedCleaveLine& line = baselineLines[lineIndex];
            std::uint8_t mask = lineIndex == index ? 3u : 0u;
            if (render::cleaveSameHandle(line.first, originalFirst)
                || render::cleaveSameHandle(line.first, originalSecond))
            {
                mask |= 1u;
            }
            if (render::cleaveSameHandle(line.second, originalFirst)
                || render::cleaveSameHandle(line.second, originalSecond))
            {
                mask |= 2u;
            }
            if (mask != 0u)
            {
                fallbackAffectedLines.push_back(StagedCleaveLineMove{
                    static_cast<std::int32_t>(lineIndex), mask});
            }
        }
        affectedLines = &fallbackAffectedLines;
    }

    const auto proposedEndpoint = [&translation](
                                      const StagedCleaveLine& line,
                                      const std::uint8_t mask,
                                      const std::uint8_t endpoint)
    {
        const TVector point = endpoint == 0u ? line.first : line.second;
        return (mask & (endpoint == 0u ? 1u : 2u)) != 0u
            ? point + translation
            : point;
    };
    for (const StagedCleaveLineMove& affected : *affectedLines)
    {
        if (affected.line < 0
            || static_cast<std::size_t>(affected.line) >= baselineLines.size())
        {
            return false;
        }
        const StagedCleaveLine& line = baselineLines[
            static_cast<std::size_t>(affected.line)];
        const TVector nextFirst = proposedEndpoint(line, affected.endpointMask, 0u);
        const TVector nextSecond = proposedEndpoint(line, affected.endpointMask, 1u);
        if (!render::cleaveFinitePoint(nextFirst)
            || !render::cleaveFinitePoint(nextSecond)
            || render::cleaveSameHandle(nextFirst, nextSecond))
        {
            if (drag)
            {
                bool restoredGeometry = false;
                for (const StagedCleaveLineMove& restore : *affectedLines)
                {
                    auto& live = stagedCleaveLines_[static_cast<std::size_t>(restore.line)];
                    const auto& baseline = baselineLines[static_cast<std::size_t>(restore.line)];
                    restoredGeometry = restoredGeometry
                        || !render::cleaveSameHandle(live.first, baseline.first)
                        || !render::cleaveSameHandle(live.second, baseline.second);
                    live = baseline;
                }
                if (restoredGeometry) ++stagedCleaveJointGuidePreviewRevision_;
            }
            return false;
        }
    }

    // The selected set and all spatially coincident endpoints are captured
    // once at press. Each sample then updates only those affected authored
    // lines; all candidates passed the full preflight above before publishing.
    bool geometryChanged = false;
    for (const StagedCleaveLineMove& affected : *affectedLines)
    {
        const std::size_t lineIndex = static_cast<std::size_t>(affected.line);
        const StagedCleaveLine& baseline = baselineLines[lineIndex];
        StagedCleaveLine& line = stagedCleaveLines_[lineIndex];
        const TVector first = proposedEndpoint(baseline, affected.endpointMask, 0u);
        const TVector second = proposedEndpoint(baseline, affected.endpointMask, 1u);
        geometryChanged = geometryChanged
            || !render::cleaveSameHandle(line.first, first)
            || !render::cleaveSameHandle(line.second, second);
        line = baseline;
        line.first = first;
        line.second = second;
        if (affected.endpointMask != 3u)
        {
            refreshStagedCleaveLinePlane(line);
        }
    }
    if (geometryChanged) ++stagedCleaveJointGuidePreviewRevision_;
    return true;
}

bool GlView::reconnectStagedCleaveLineLinks(const std::size_t index)
{
    if (index >= stagedCleaveLines_.size() || stagedCleaveLines_.size() < 2)
    {
        return false;
    }

    bool changed = false;
    StagedCleaveLine& moved = stagedCleaveLines_[index];
    const std::size_t previous = index == 0
        ? stagedCleaveLines_.size() - 1
        : index - 1;
    if (render::cleaveSameHandle(moved.first, stagedCleaveLines_[previous].second))
    {
        changed = !moved.linkedToPrevious
            || !stagedCleaveLines_[previous].linkedToNext;
        moved.linkedToPrevious = true;
        stagedCleaveLines_[previous].linkedToNext = true;
    }

    const std::size_t next = (index + 1) % stagedCleaveLines_.size();
    if (render::cleaveSameHandle(moved.second, stagedCleaveLines_[next].first))
    {
        changed = !moved.linkedToNext
            || !stagedCleaveLines_[next].linkedToPrevious
            || changed;
        moved.linkedToNext = true;
        stagedCleaveLines_[next].linkedToPrevious = true;
    }
    return changed;
}

std::vector<std::vector<TVector>> GlView::stagedCleavePlanePolygons(
    const StagedCleaveLine& line) const
{
    std::vector<std::vector<TVector>> polygons{};
    if (!level_)
    {
        return polygons;
    }
    std::vector<std::int32_t> targets = stagedCleaveContext_.targetSectors;
    if (targets.empty() && selectedSector_ >= 0)
    {
        targets.push_back(selectedSector_);
    }
    for (const auto index : targets)
    {
        if (index < 0 || static_cast<std::size_t>(index) >= level_->sectors.size())
        {
            continue;
        }
        auto polygon = render::cleavePlaneSectionPolygon(
            level_->sectors[static_cast<std::size_t>(index)], line.planeNormal,
            line.first, line.second - line.first);
        if (!polygon.empty())
        {
            polygons.push_back(std::move(polygon));
        }
    }
    return polygons;
}

std::vector<render::CleaveSnapCandidate> GlView::cleaveGeometrySnapTargets(
    const QMatrix4x4& mvp,
    const bool surfaceMode) const
{
    struct PointKey
    {
        std::uint64_t x{};
        std::uint64_t y{};
        std::uint64_t z{};
        bool operator==(const PointKey&) const noexcept = default;
    };
    struct MarkerKey
    {
        render::CleaveSnapTargetKind kind{};
        PointKey position{};
        PointKey edgeFirst{};
        PointKey edgeSecond{};
        bool operator==(const MarkerKey&) const noexcept = default;
    };
    struct MarkerKeyHash
    {
        std::size_t operator()(const MarkerKey& key) const noexcept
        {
            std::size_t hash = static_cast<std::size_t>(key.kind);
            const auto mix = [&](const std::uint64_t value)
            {
                hash ^= static_cast<std::size_t>(value)
                    + 0x9e3779b9U + (hash << 6U) + (hash >> 2U);
            };
            mix(key.position.x);
            mix(key.position.y);
            mix(key.position.z);
            mix(key.edgeFirst.x);
            mix(key.edgeFirst.y);
            mix(key.edgeFirst.z);
            mix(key.edgeSecond.x);
            mix(key.edgeSecond.y);
            mix(key.edgeSecond.z);
            return hash;
        }
    };

    std::vector<render::CleaveSnapCandidate> candidates{};
    std::unordered_set<MarkerKey, MarkerKeyHash> markerKeys{};
    if (!level_)
    {
        return candidates;
    }

    const auto pointKey = [](const TVector& point)
    {
        const auto componentBits = [](const double value)
        {
            // Canonicalize signed zero so coincident geometry authored with
            // -0 does not draw a second marker over +0.
            return value == 0.0f
                ? std::uint64_t{0}
                : std::bit_cast<std::uint64_t>(value);
        };
        return PointKey{
            componentBits(point.x),
            componentBits(point.y),
            componentBits(point.z),
        };
    };

    const auto addCandidate = [&](const TVector& position,
                                  const render::CleaveSnapTargetKind kind,
                                  const TVector& edgeFirst = {},
                                  const TVector& edgeSecond = {})
    {
        const ProjectedPoint projected = projectWorldToWidget(
            mvp,
            position,
            width(),
            height());
        if (!projected.valid || projected.clipW <= 1.0e-6f
            || projected.ndcZ < -1.0f || projected.ndcZ > 1.0f)
        {
            return;
        }

        MarkerKey key{kind, pointKey(position), {}, {}};
        if (kind == render::CleaveSnapTargetKind::EdgeMidpoint)
        {
            key.edgeFirst = pointKey(edgeFirst);
            key.edgeSecond = pointKey(edgeSecond);
            if (std::tie(key.edgeSecond.x, key.edgeSecond.y, key.edgeSecond.z)
                < std::tie(key.edgeFirst.x, key.edgeFirst.y, key.edgeFirst.z))
            {
                std::swap(key.edgeFirst, key.edgeSecond);
            }
        }
        if (!markerKeys.insert(key).second)
        {
            return;
        }
        candidates.push_back(render::CleaveSnapCandidate{
            position,
            projected.pos,
            kind,
            edgeFirst,
            edgeSecond,
        });
    };

    const auto addSurfaceCandidates = [&](const Sector& sector, const Surface& surface)
    {
        const std::optional<TVector> center =
            render::cleaveSurfaceCenter(sector, surface);

        for (std::size_t edge = 0; edge < surface.vertices.size(); ++edge)
        {
            const std::int32_t firstIndex = surface.vertices[edge];
            const std::int32_t secondIndex = surface.vertices[(edge + 1) % surface.vertices.size()];
            if (firstIndex < 0 || secondIndex < 0
                || static_cast<std::size_t>(firstIndex) >= sector.vertices.size()
                || static_cast<std::size_t>(secondIndex) >= sector.vertices.size())
            {
                continue;
            }
            const Vertex& firstVertex = sector.vertices[static_cast<std::size_t>(firstIndex)];
            const Vertex& secondVertex = sector.vertices[static_cast<std::size_t>(secondIndex)];
            const TVector first(
                firstVertex.x,
                firstVertex.y,
                firstVertex.z);
            const TVector second(
                secondVertex.x,
                secondVertex.y,
                secondVertex.z);
            addCandidate(
                (first + second) * 0.5f,
                render::CleaveSnapTargetKind::EdgeMidpoint,
                first,
                second);
        }
        if (center)
        {
            addCandidate(
                *center,
                render::CleaveSnapTargetKind::SurfaceCenter);
        }
    };

    if (surfaceMode)
    {
        std::vector<SurfaceRef> targetSurfaces(
            highlightedSelection_.surfaces.begin(),
            highlightedSelection_.surfaces.end());
        if (selectedSurfaceSector_ >= 0 && selectedSurface_ >= 0)
        {
            const SurfaceRef active{selectedSurfaceSector_, selectedSurface_};
            if (std::find(targetSurfaces.begin(), targetSurfaces.end(), active)
                == targetSurfaces.end())
            {
                targetSurfaces.push_back(active);
            }
        }
        std::sort(
            targetSurfaces.begin(),
            targetSurfaces.end(),
            [](const SurfaceRef& lhs, const SurfaceRef& rhs)
            {
                return lhs.sector == rhs.sector
                    ? lhs.surface < rhs.surface
                    : lhs.sector < rhs.sector;
            });
        targetSurfaces.erase(
            std::unique(targetSurfaces.begin(), targetSurfaces.end()),
            targetSurfaces.end());

        for (const SurfaceRef ref : targetSurfaces)
        {
            if (ref.sector < 0 || ref.surface < 0
                || static_cast<std::size_t>(ref.sector) >= level_->sectors.size())
            {
                continue;
            }
            const Sector& sector = level_->sectors[static_cast<std::size_t>(ref.sector)];
            if (static_cast<std::size_t>(ref.surface) >= sector.surfaces.size())
            {
                continue;
            }
            const Surface& surface = sector.surfaces[static_cast<std::size_t>(ref.surface)];
            for (const std::int32_t vertexIndex : surface.vertices)
            {
                if (vertexIndex < 0
                    || static_cast<std::size_t>(vertexIndex) >= sector.vertices.size())
                {
                    continue;
                }
                const Vertex& vertex = sector.vertices[static_cast<std::size_t>(vertexIndex)];
                addCandidate(
                    TVector(
                        vertex.x,
                        vertex.y,
                        vertex.z),
                    render::CleaveSnapTargetKind::Vertex);
            }
            addSurfaceCandidates(sector, surface);
        }
    }
    else
    {
        std::vector<std::int32_t> targetSectors(
            highlightedSelection_.sectors.begin(),
            highlightedSelection_.sectors.end());
        if (selectedSector_ >= 0
            && std::find(targetSectors.begin(), targetSectors.end(), selectedSector_)
                == targetSectors.end())
        {
            targetSectors.push_back(selectedSector_);
        }
        std::sort(targetSectors.begin(), targetSectors.end());
        targetSectors.erase(
            std::unique(targetSectors.begin(), targetSectors.end()),
            targetSectors.end());

        for (const std::int32_t sectorIndex : targetSectors)
        {
            if (sectorIndex < 0
                || static_cast<std::size_t>(sectorIndex) >= level_->sectors.size())
            {
                continue;
            }
            const Sector& sector = level_->sectors[static_cast<std::size_t>(sectorIndex)];
            for (const Vertex& vertex : sector.vertices)
            {
                addCandidate(
                    TVector(
                        vertex.x,
                        vertex.y,
                        vertex.z),
                    render::CleaveSnapTargetKind::Vertex);
            }
            for (const Surface& surface : sector.surfaces)
            {
                addSurfaceCandidates(sector, surface);
            }
        }
    }

    return candidates;
}

std::optional<render::CleaveSnapCandidate> GlView::closestCleaveSnapTarget(
    const QPoint& widgetPosition,
    const QMatrix4x4& mvp,
    const bool surfaceMode,
    const double tolerancePx,
    const std::int32_t excludedStagedLine) const
{
    const QPointF pointer(widgetPosition);
    const std::vector<render::CleaveSnapCandidate> candidates =
        cleaveSnapTargets(mvp, surfaceMode, excludedStagedLine);

    // While reshaping a chain endpoint, prefer its compatible authored
    // neighbor once that endpoint enters the staged-endpoint acquisition
    // radius. A wider radius is useful for tiny handles, but without this
    // topology preference a different endpoint on a short neighboring line
    // can be a few pixels closer and prevent the intended reciprocal join.
    if (editingStagedCleaveLine_ >= 0
        && editingStagedCleaveEndpoint_ >= 0
        && editingStagedCleaveLine_ == excludedStagedLine
        && stagedCleaveLines_.size() > 1)
    {
        const std::size_t editedIndex =
            static_cast<std::size_t>(editingStagedCleaveLine_);
        if (editedIndex < stagedCleaveLines_.size())
        {
            const StagedCleaveLine& edited = stagedCleaveLines_[editedIndex];
            const TVector movingOrigin = editingStagedCleaveEndpoint_ == 0
                ? edited.first
                : edited.second;
            const std::size_t neighborIndex = editingStagedCleaveEndpoint_ == 0
                ? (editedIndex == 0
                    ? stagedCleaveLines_.size() - 1
                    : editedIndex - 1)
                : (editedIndex + 1) % stagedCleaveLines_.size();
            const TVector neighborPosition = editingStagedCleaveEndpoint_ == 0
                ? stagedCleaveLines_[neighborIndex].second
                : stagedCleaveLines_[neighborIndex].first;

            // A currently linked/coincident neighbor at the drag origin is
            // the magnetic-stick case this path must avoid, not a destination.
            if (!render::cleaveSameHandle(neighborPosition, movingOrigin))
            {
                const ProjectedPoint projected = projectWorldToWidget(
                    mvp,
                    neighborPosition,
                    width(),
                    height());
                if (projected.valid && projected.clipW > 1.0e-6f
                    && projected.ndcZ >= -1.0f && projected.ndcZ <= 1.0f)
                {
                    const std::vector<render::CleaveSnapCandidate> preferred{
                        render::CleaveSnapCandidate{
                            neighborPosition,
                            projected.pos,
                            render::CleaveSnapTargetKind::StagedEndpoint,
                            {},
                            {},
                        },
                    };
                    if (const auto snapped =
                            render::selectNearestCleaveSnapCandidate(
                                pointer,
                                preferred,
                                tolerancePx))
                    {
                        return snapped;
                    }
                }
            }
        }
    }

    return render::selectNearestCleaveSnapCandidate(
        pointer,
        candidates,
        tolerancePx);
}

std::vector<render::CleaveSnapCandidate> GlView::cleaveSnapTargets(
    const QMatrix4x4& mvp,
    const bool surfaceMode,
    const std::int32_t excludedStagedLine) const
{
    std::vector<render::CleaveSnapCandidate> candidates{};
    if (!level_)
    {
        return candidates;
    }
    const StagedCleaveLine* excludedLine = excludedStagedLine >= 0
            && static_cast<std::size_t>(excludedStagedLine) < stagedCleaveLines_.size()
        ? &stagedCleaveLines_[static_cast<std::size_t>(excludedStagedLine)]
        : nullptr;
    const auto movingEndpointMask = [this](const std::int32_t lineIndex)
    {
        if (!stagedCleavePlaneDrag_.active)
        {
            return std::uint8_t{};
        }
        const auto& affectedLines = stagedCleavePlaneDrag_.affectedLines;
        const auto found = std::lower_bound(
            affectedLines.begin(),
            affectedLines.end(),
            lineIndex,
            [](const StagedCleaveLineMove& affected, const std::int32_t candidate)
            {
                return affected.line < candidate;
            });
        return found != affectedLines.end() && found->line == lineIndex
            ? found->endpointMask
            : std::uint8_t{};
    };
    const auto addStagedEndpoint = [&](
        const TVector& position,
        const std::int32_t lineIndex,
        const std::uint8_t endpointMask)
    {
        if ((movingEndpointMask(lineIndex) & endpointMask) != 0u)
        {
            return;
        }
        if (excludedLine
            && (render::cleaveSameHandle(position, excludedLine->first)
                || render::cleaveSameHandle(position, excludedLine->second)))
        {
            // A reciprocal linked endpoint is another authored record of the
            // dragged line's own position. Exclude by position as well as line
            // index so Shift cannot immediately snap the line back to itself.
            return;
        }
        const ProjectedPoint projected = projectWorldToWidget(
            mvp,
            position,
            width(),
            height());
        if (!projected.valid || projected.clipW <= 1.0e-6f
            || projected.ndcZ < -1.0f || projected.ndcZ > 1.0f)
        {
            return;
        }
        candidates.push_back(render::CleaveSnapCandidate{
            position,
            projected.pos,
            render::CleaveSnapTargetKind::StagedEndpoint,
            {},
            {},
        });
    };

    // Keep explicit Multi-cut endpoints first so an exactly coincident shared
    // endpoint remains the stable target when geometry candidates overlap it.
    if (cleaveStagedMode_)
    {
        for (std::size_t lineIndex = 0; lineIndex < stagedCleaveLines_.size(); ++lineIndex)
        {
            if (static_cast<std::int32_t>(lineIndex) == excludedStagedLine)
            {
                continue;
            }
            const StagedCleaveLine& line = stagedCleaveLines_[lineIndex];
            if (static_cast<std::int32_t>(lineIndex) != editingStagedCleaveLine_
                || editingStagedCleaveEndpoint_ != 0)
            {
                addStagedEndpoint(
                    line.first,
                    static_cast<std::int32_t>(lineIndex),
                    1u);
            }
            if (static_cast<std::int32_t>(lineIndex) != editingStagedCleaveLine_
                || editingStagedCleaveEndpoint_ != 1)
            {
                addStagedEndpoint(
                    line.second,
                    static_cast<std::int32_t>(lineIndex),
                    2u);
            }
        }
    }

    std::vector<render::CleaveSnapCandidate> geometry =
        cleaveGeometrySnapTargets(mvp, surfaceMode);
    candidates.insert(
        candidates.end(),
        geometry.begin(),
        geometry.end());

    return candidates;
}

void GlView::updateCleaveSnapGuideVisibility(
    const Qt::KeyboardModifiers modifiers)
{
    const bool forcedEndpointAngle = editingStagedCleaveLine_ >= 0
        && editingStagedCleaveEndpoint_ >= 0
        && modifiers.testFlag(Qt::ShiftModifier)
        && modifiers.testFlag(Qt::ControlModifier);
    const bool visible = viewportPickingEnabled_
        && modifiers.testFlag(Qt::ShiftModifier)
        && !forcedEndpointAngle
        && (sectorCleaveToolEnabled_
            || (surfaceCleaveToolEnabled_
                && surfaceSubMode_ == SurfaceSubMode::Face));
    if (cleaveSnapGuidesVisible_ == visible)
    {
        return;
    }

    cleaveSnapGuidesVisible_ = visible;
    update();
}

void GlView::refreshCleaveSnapPreviewForModifiers(
    const Qt::KeyboardModifiers modifiers)
{
    refreshCleaveSnapPreview(lastMousePos_, modifiers);
}

void GlView::refreshCleaveSnapPreview(
    const QPoint& widgetPosition,
    const Qt::KeyboardModifiers modifiers)
{
    updateCleaveSnapGuideVisibility(modifiers);

    const bool sectorStart = sectorCleaveToolEnabled_
        && !sectorCleaveHaveFirstPoint_;
    const bool surfaceFaceStart = surfaceCleaveToolEnabled_
        && surfaceSubMode_ == SurfaceSubMode::Face
        && !surfaceCleaveHaveFirstPoint_;
    const bool canPreview = viewportPickingEnabled_
        && (sectorStart || surfaceFaceStart)
        && !stagedCleavePlaneDrag_.active
        && editingStagedCleaveLine_ < 0
        && modifiers.testFlag(Qt::ShiftModifier)
        && QGuiApplication::mouseButtons() == Qt::NoButton
        // A chained continuation has an exact staged endpoint and therefore
        // no ambiguous construction-point choice to preview.
        && !(cleaveStagedMode_ && cleaveChainEnabled_
            && !stagedCleaveLines_.empty());

    std::optional<render::CleaveSnapCandidate> next{};
    if (canPreview)
    {
        QMatrix4x4 projection{};
        QMatrix4x4 view{};
        QVector3D eye{};
        QVector3D target{};
        float nearPlane{};
        float farPlane{};
        buildCameraMatrices(
            projection,
            view,
            eye,
            target,
            nearPlane,
            farPlane);
        (void)eye;
        (void)target;
        (void)nearPlane;
        (void)farPlane;
        constexpr double kStartSnapTolerancePx = 18.0;
        next = closestCleaveSnapTarget(
            widgetPosition,
            projection * view,
            surfaceFaceStart,
            kStartSnapTolerancePx);
    }

    const bool changed = cleaveSnapMarker_.has_value() != next.has_value()
        || (cleaveSnapMarker_ && next
            && (cleaveSnapMarker_->kind != next->kind
                || (cleaveSnapMarker_->position - next->position).lengthSqr()
                    > 1.0e-12f));
    cleaveSnapMarker_ = std::move(next);
    if (changed)
    {
        update();
    }
}

bool GlView::attachStagedSectorCleaveLineToCameraEdge(const std::size_t index)
{
    if (index >= stagedCleaveLines_.size())
    {
        return false;
    }
    StagedCleaveLine& line = stagedCleaveLines_[index];
    // Only endpoints which are already linked to a neighbor are topology.
    // Merely arming Chain must not leave an otherwise independent first line
    // floating at its screen-drawn construction position.
    if (line.linkedToPrevious || line.linkedToNext
        || !line.authoredCamera.valid)
    {
        return false;
    }
    for ( std::size_t otherIndex = 0; otherIndex < stagedCleaveLines_.size(); ++otherIndex )
    {
        if ( otherIndex == index )
        {
            continue;
        }
        const StagedCleaveLine& other = stagedCleaveLines_[otherIndex];
        if ( render::cleaveSameHandle(line.first, other.first)
            || render::cleaveSameHandle(line.first, other.second)
            || render::cleaveSameHandle(line.second, other.first)
            || render::cleaveSameHandle(line.second, other.second))
        {
            // A chosen branch source is a shared coordinate handle even when
            // it deliberately carries no adjacent-vector topology bits.
            return false;
        }
    }

    const std::vector<std::vector<TVector>> polygons = stagedCleavePlanePolygons(line);
    const TVector cameraTarget = render::cleaveDocumentPoint(line.authoredCamera.orbit.target());
    const TVector cameraPosition = cameraTarget
        + render::cleaveDocumentPoint(app::ViewportCamera::SphericalDirection(
            line.authoredCamera.orbit.yawDeg,
            line.authoredCamera.orbit.pitchDeg))
            * line.authoredCamera.orbit.distance;
    const std::optional<render::CleavePlanePolygonEdge> nearest =
        render::nearestCleavePlanePolygonEdge(polygons, cameraPosition);
    if (!nearest)
    {
        return false;
    }

    TVector first = nearest->first;
    TVector second = nearest->second;
    const TVector oldDirection = line.second - line.first;
    if (render::cleaveDot(second - first, oldDirection) < 0.0f)
    {
        std::swap(first, second);
    }

    TVector normal = line.planeNormal;
    if (!render::cleaveNormalize(normal) || render::cleaveSameHandle(second, first))
    {
        return false;
    }
    const TVector oldPlanePoint = line.first;
    // Polygon intersections are epsilon based. Project the selected edge back
    // onto the exact authoritative plane so re-anchoring cannot drift its
    // signed offset even after repeated edits/restores.
    first = first - normal * render::cleaveDot(normal, first - oldPlanePoint);
    second = second - normal * render::cleaveDot(normal, second - oldPlanePoint);
    const TVector oldReference = line.angleReferenceAxis;
    line.first = first;
    line.second = second;
    line.planeNormal = normal;
    initializeStagedCleaveLineFrame(line, oldReference);
    return line.constructionPlaneNormal.lengthSqr() > 1.0e-12f;
}

void GlView::captureStagedCleaveContext(const bool surfaceMode)
{
    cancelViewAnimation();

    QMatrix4x4 projection{};
    QMatrix4x4 view{};
    QVector3D eye{};
    QVector3D target{};
    float nearPlane{};
    float farPlane{};
    buildCameraMatrices(projection, view, eye, target, nearPlane, farPlane);

    stagedCleaveContext_.valid = true;
    stagedCleaveContext_.surfaceMode = surfaceMode;
    stagedCleaveContext_.projection = projection;
    stagedCleaveContext_.view = view;
    stagedCleaveContext_.viewportWidth = width();
    stagedCleaveContext_.viewportHeight = height();
    stagedCleaveContext_.sourceSurface = surfaceMode
        ? SurfaceRef{ selectedSurfaceSector_, selectedSurface_ }
        : SurfaceRef{ -1, -1 };
    stagedCleaveContext_.targetSectors.assign(
        highlightedSelection_.sectors.begin(),
        highlightedSelection_.sectors.end());
    stagedCleaveContext_.targetSurfaces.assign(
        highlightedSelection_.surfaces.begin(),
        highlightedSelection_.surfaces.end());
    if (!surfaceMode && stagedCleaveContext_.targetSectors.empty() && selectedSector_ >= 0)
    {
        stagedCleaveContext_.targetSectors.push_back(selectedSector_);
    }
    if (surfaceMode && stagedCleaveContext_.targetSurfaces.empty()
        && selectedSurfaceSector_ >= 0 && selectedSurface_ >= 0)
    {
        stagedCleaveContext_.targetSurfaces.push_back(
            SurfaceRef{ selectedSurfaceSector_, selectedSurface_ });
    }
    std::sort(
        stagedCleaveContext_.targetSectors.begin(),
        stagedCleaveContext_.targetSectors.end());
    std::sort(
        stagedCleaveContext_.targetSurfaces.begin(),
        stagedCleaveContext_.targetSurfaces.end(),
        [](const SurfaceRef& lhs, const SurfaceRef& rhs)
        {
            return lhs.sector == rhs.sector
                ? lhs.surface < rhs.surface
                : lhs.sector < rhs.sector;
    });
}

std::optional<GlView::StagedCleaveShapePlacementFrame>
GlView::stagedCleaveShapePlacementFrameAt(
    const QPoint& position,
    const Qt::KeyboardModifiers modifiers)
{
    if ( !cleaveStagedMode_ || !cleaveShapeOptions_.placementArmed
        || !level_ || (!sectorCleaveToolEnabled_ && !surfaceCleaveToolEnabled_) )
    {
        return std::nullopt;
    }
    if ( stagedCleaveContext_.valid
        && !stagedCleaveContextMatchesCurrent(surfaceCleaveToolEnabled_) )
    {
        return std::nullopt;
    }

    if ( !stagedCleaveShapePlacementPlan_ )
    {
        const ui::viewport::CleaveShapeSpec spec{
            .kind = cleaveShapeOptions_.kind,
            .width = cleaveShapeOptions_.width,
            .height = cleaveShapeOptions_.height,
            .dimensionsLinked = cleaveShapeOptions_.dimensionsLinked,
            .segments = cleaveShapeOptions_.segments,
            .points = cleaveShapeOptions_.points,
        };
        stagedCleaveShapePlacementPlan_ =
            ui::viewport::makeCleaveShapePlan(spec);
    }
    if ( !stagedCleaveShapePlacementPlan_
        || stagedCleaveShapePlacementPlan_->perimeter.size() < 3 )
    {
        return std::nullopt;
    }

    QMatrix4x4 projection{};
    QMatrix4x4 cameraView{};
    QVector3D eye{};
    QVector3D target{};
    float nearPlane{};
    float farPlane{};
    buildCameraMatrices(
        projection, cameraView, eye, target, nearPlane, farPlane);
    (void)nearPlane;
    const bool orthographic = camera_.projectionType()
        == app::ViewportCamera::ProjectionType::Orthographic;
    const bool cameraPlane = sectorCleaveToolEnabled_
        && cleaveShapeOptions_.cameraPlane;
    const QMatrix4x4 mvp = projection * cameraView;
    bool inverseOk = false;
    const QMatrix4x4 inverseMvp = mvp.inverted(&inverseOk);
    if ( !inverseOk )
    {
        return std::nullopt;
    }
    const auto ray = render::viewportRayForWidget(
        projection,
        cameraView,
        eye,
        position,
        width(),
        height(),
        orthographic);
    if ( !ray )
    {
        return std::nullopt;
    }
    const double rayMaxDistance = render::viewportRayMaxDistanceForWidget(
        projection,
        cameraView,
        QPointF(position),
        width(),
        height(),
        orthographic).value_or(std::max(0.0, static_cast<double>(farPlane)));
    const std::vector<sed::level::SurfacePickHit> hits =
        cameraPlane ? std::vector<sed::level::SurfacePickHit>{}
        : surfacePickIndex_.pickSurfaces(
            *level_, ray->origin, ray->direction, rayMaxDistance);

    const std::vector<std::int32_t>* const targetSectors =
        stagedCleaveContext_.valid
        ? &stagedCleaveContext_.targetSectors
        : nullptr;
    const auto sectorIsEligible = [this, targetSectors](
                                      const std::int32_t sector) -> bool
    {
        if ( targetSectors && !targetSectors->empty() )
        {
            return std::binary_search(
                targetSectors->begin(), targetSectors->end(), sector);
        }
        if ( !highlightedSelection_.sectors.empty() )
        {
            return highlightedSelection_.sectors.contains(sector);
        }
        return selectedSector_ < 0 || selectedSector_ == sector;
    };

    const sed::level::SurfacePickHit* selectedHit = nullptr;
    const Surface* selectedSurface = nullptr;
    for ( const sed::level::SurfacePickHit& hit : hits )
    {
        if ( surfaceCleaveToolEnabled_ )
        {
            if ( hit.surface.sector != selectedSurfaceSector_
                || hit.surface.surface != selectedSurface_ )
            {
                continue;
            }
        }
        else if ( !sectorIsEligible(hit.surface.sector) )
        {
            continue;
        }

        if ( hit.surface.sector < 0 || hit.surface.surface < 0
            || static_cast<std::size_t>(hit.surface.sector) >= level_->sectors.size() )
        {
            continue;
        }
        const Sector& sector = level_->sectors[
            static_cast<std::size_t>(hit.surface.sector)];
        if ( static_cast<std::size_t>(hit.surface.surface)
            >= sector.surfaces.size() )
        {
            continue;
        }
        selectedHit = &hit;
        selectedSurface = &sector.surfaces[
            static_cast<std::size_t>(hit.surface.surface)];
        break;
    }
    if ( !cameraPlane && (!selectedHit || !selectedSurface) )
    {
        return std::nullopt;
    }

    TVector planeNormal = cameraPlane
        ? render::cleaveDocumentPoint(cameraForward())
        : selectedSurface->normal;
    if ( !render::cleaveNormalize(planeNormal) )
    {
        return std::nullopt;
    }
    TVector center{};
    if ( cameraPlane )
    {
        // The orbit target is the already maintained selection/view proxy.
        // Keep this depth fixed across pointer samples and avoid wall picking
        // or selected-sector bounds traversal for free camera-plane placement.
        const auto hit = render::cleaveIntersectRayPlane(
            *ray, render::cleaveDocumentPoint(target), planeNormal);
        if ( !hit )
        {
            return std::nullopt;
        }
        const double rayDistance = render::cleaveDot(*hit - ray->origin, ray->direction);
        const double eyeDepth = render::cleaveDot(
            *hit - render::cleaveDocumentPoint(eye), planeNormal);
        if ( !std::isfinite(rayDistance) || rayDistance < 0.0
            || rayDistance > rayMaxDistance || !std::isfinite(eyeDepth)
            || eyeDepth <= std::max(0.0, static_cast<double>(nearPlane)) )
        {
            return std::nullopt;
        }
        center = *hit;
    }
    else
    {
        center = selectedHit->position;
    }
    const render::TransformGridFrame gridFrame = activeTransformGridFrame(
        cameraView,
        mvp,
        inverseMvp,
        eye,
        target,
        orthographic);
    if ( modifiers.testFlag(Qt::ShiftModifier) )
    {
        constexpr double kShapeGeometrySnapRadiusPx = 18.0;
        const auto snap = closestCleaveSnapTarget(
            position,
            mvp,
            surfaceCleaveToolEnabled_,
            kShapeGeometrySnapRadiusPx);
        if ( snap )
        {
            center = snap->position;
        }
    }
    else
    {
        const bool useGridSnap = transformGridSnapEnabled_
            != modifiers.testFlag(Qt::ControlModifier);
        if ( useGridSnap && gridFrame.valid )
        {
            center = render::snapPointToGridOnPlane(
                center,
                center,
                planeNormal,
                gridFrame);
        }
    }

    TVector axisA = render::cleaveDocumentPoint(
        cameraPlane ? cameraRight() : gridFrame.axisA);
    axisA = axisA - planeNormal * render::cleaveDot(axisA, planeNormal);
    if ( !render::cleaveNormalize(axisA) )
    {
        axisA = render::cleaveCross(
            render::cleaveDocumentPoint(gridFrame.axisB), planeNormal);
        if ( !render::cleaveNormalize(axisA) )
        {
            return std::nullopt;
        }
    }
    TVector axisB = render::cleaveDocumentPoint(
        cameraPlane ? cameraUp() : gridFrame.axisB);
    axisB = axisB - planeNormal * render::cleaveDot(axisB, planeNormal);
    axisB = axisB - axisA * render::cleaveDot(axisB, axisA);
    if ( !render::cleaveNormalize(axisB) )
    {
        axisB = render::cleaveCross(planeNormal, axisA);
        if ( !render::cleaveNormalize(axisB) )
        {
            return std::nullopt;
        }
        const TVector preferredB = render::cleaveDocumentPoint(gridFrame.axisB);
        if ( render::cleaveDot(axisB, preferredB) < 0.0 )
        {
            axisB = axisB * -1.0;
        }
    }
    if ( !render::cleaveFinitePoint(center)
        || !render::cleaveFinitePoint(planeNormal) )
    {
        return std::nullopt;
    }
    const double radians = cleaveShapeOptions_.rotationDegrees
        * (3.14159265358979323846 / 180.0);
    const TVector originalAxisA = axisA;
    axisA = originalAxisA * std::cos(radians) + axisB * std::sin(radians);
    axisB = axisB * std::cos(radians) - originalAxisA * std::sin(radians);
    return StagedCleaveShapePlacementFrame{
        center,
        planeNormal,
        axisA,
        axisB,
    };
}

bool GlView::appendStagedCleaveShape(
    const StagedCleaveShapePlacementFrame& frame)
{
    (void)finishStagedCleaveWheelSession();
    if ( !stagedCleaveShapePlacementPlan_
        || !cleaveStagedMode_
        || (!sectorCleaveToolEnabled_ && !surfaceCleaveToolEnabled_)
        || (stagedCleaveContext_.valid
            && !stagedCleaveContextMatchesCurrent(surfaceCleaveToolEnabled_)) )
    {
        return false;
    }
    if ( stagedCleaveLinesFormSingleClosedChain() )
    {
        showToast(
            "Multi-cut: the chain is closed; edit it, Apply, or Clear",
            2600,
            ToastPlacement::BottomCenter,
            ToastGroup::General);
        return false;
    }

    const ui::viewport::CleaveShapePlan& plan =
        *stagedCleaveShapePlacementPlan_;
    const std::size_t strokeCount = plan.decompositionStrokes.size()
        + plan.perimeter.size();
    if ( strokeCount == 0u
        || strokeCount > static_cast<std::size_t>(
            std::numeric_limits<std::int32_t>::max())
        || stagedCleaveLines_.size() > static_cast<std::size_t>(
            std::numeric_limits<std::int32_t>::max()) - strokeCount )
    {
        return false;
    }

    std::vector<StagedCleaveLine> addedLines{};
    addedLines.reserve(strokeCount);
    const auto mapPoint = [&frame](
                              const ui::viewport::CleaveShapePoint2D& point)
    {
        return frame.center
            + frame.axisA * point.x
            + frame.axisB * point.y;
    };
    const auto appendStroke = [this, &frame, &mapPoint, &addedLines](
                                  const ui::viewport::CleaveShapeStroke2D& stroke)
    {
        StagedCleaveLine line{};
        line.first = mapPoint(stroke.first);
        line.second = mapPoint(stroke.second);
        line.planeNormal = render::cleaveCross(
            frame.planeNormal,
            line.second - line.first);
        if ( !render::cleaveFinitePoint(line.first)
            || !render::cleaveFinitePoint(line.second)
            || render::cleaveSameHandle(line.first, line.second)
            || !render::cleaveNormalize(line.planeNormal) )
        {
            return false;
        }
        initializeStagedCleaveLineFrame(line, frame.axisA);
        captureStagedCleaveLineCamera(line);
        if ( !render::cleaveFinitePoint(line.constructionPlaneNormal)
            || !render::cleaveFinitePoint(line.angleReferenceAxis)
            || line.constructionPlaneNormal.lengthSqr() == 0.0 )
        {
            return false;
        }
        addedLines.push_back(std::move(line));
        return true;
    };
    for ( const ui::viewport::CleaveShapeStroke2D& stroke
        : plan.decompositionStrokes )
    {
        if ( !appendStroke(stroke) )
        {
            return false;
        }
    }
    for ( std::size_t index = 0; index < plan.perimeter.size(); ++index )
    {
        const std::size_t next = (index + 1u) % plan.perimeter.size();
        if ( !appendStroke(ui::viewport::CleaveShapeStroke2D{
                plan.perimeter[index],
                plan.perimeter[next]}) )
        {
            return false;
        }
    }
    if ( addedLines.size() != strokeCount )
    {
        return false;
    }

    const std::size_t currentBytes = stagedCleaveHistoryStorageBytes();
    const std::size_t beforeBytes =
        stagedCleaveDraftStateCurrentOwnedBytesEstimate();
    const std::size_t resultingCount = stagedCleaveLines_.size() + strokeCount;
    const std::size_t candidateBytes = addedLines.capacity()
        * sizeof(StagedCleaveLine);
    const std::size_t resultingSnapshotBytes = sizeof(StagedCleaveDraftState)
        + resultingCount * sizeof(StagedCleaveLine)
        + resultingCount * sizeof(std::int32_t)
        + resultingCount * sizeof(std::int32_t)
        + stagedCleaveContext_.targetSectors.capacity() * sizeof(std::int32_t)
        + stagedCleaveContext_.targetSurfaces.capacity() * sizeof(SurfaceRef);
    const std::size_t lineReallocationBytes = resultingCount
            > stagedCleaveLines_.capacity()
        ? resultingCount * sizeof(StagedCleaveLine)
        : 0u;
    const std::size_t orderReallocationBytes = resultingCount
            > stagedCleaveApplyOrder_.capacity()
        ? resultingCount * sizeof(std::int32_t)
        : 0u;
    const std::size_t selectionReallocationBytes = resultingCount
            > selectedStagedCleaveLines_.capacity()
        ? resultingCount * sizeof(std::int32_t)
        : 0u;
    const std::size_t descriptorReserveBytes =
        stagedCleaveHistory_.states.capacity() < kStagedCleaveHistoryMaxSnapshots
        ? (kStagedCleaveHistoryMaxSnapshots
            - stagedCleaveHistory_.states.capacity())
            * sizeof(StagedCleaveDraftState)
        : 0u;
    const std::array<std::size_t, 7> additions{
        stagedCleaveHistory_.gestureActive ? 0u : beforeBytes,
        candidateBytes,
        resultingSnapshotBytes,
        lineReallocationBytes,
        orderReallocationBytes,
        selectionReallocationBytes,
        descriptorReserveBytes,
    };
    std::size_t projectedBytes = currentBytes;
    for ( const std::size_t addition : additions )
    {
        if ( addition > kStagedCleaveHistoryMaxBytes - std::min(
                projectedBytes,
                kStagedCleaveHistoryMaxBytes) )
        {
            showToast(
                "Multi-cut: local history limit reached; Clear or Apply before adding this shape",
                3200,
                ToastPlacement::BottomCenter,
                ToastGroup::Error);
            return false;
        }
        projectedBytes += addition;
    }
    if ( projectedBytes > kStagedCleaveHistoryMaxBytes
        || !beginStagedCleaveHistoryGesture() )
    {
        return false;
    }

    if ( stagedCleaveLines_.empty() )
    {
        captureStagedCleaveContext(surfaceCleaveToolEnabled_);
    }
    stagedCleaveLines_.reserve(resultingCount);
    stagedCleaveApplyOrder_.reserve(resultingCount);
    std::vector<std::int32_t> selectedLines{};
    selectedLines.reserve(strokeCount);
    for ( StagedCleaveLine& line : addedLines )
    {
        const std::int32_t lineIndex = static_cast<std::int32_t>(
            stagedCleaveLines_.size());
        stagedCleaveLines_.push_back(std::move(line));
        stagedCleaveApplyOrder_.push_back(lineIndex);
        selectedLines.push_back(lineIndex);
    }
    setSelectedStagedCleaveLines(
        std::move(selectedLines),
        static_cast<std::int32_t>(stagedCleaveLines_.size() - 1u),
        false);
    stagedCleaveChainOrigin_.reset();
    if ( !finishStagedCleaveHistoryGesture() )
    {
        (void)restoreAndDiscardStagedCleaveHistoryGesture();
        return false;
    }
    Q_EMIT viewportStateChanged();
    update();
    return true;
}

bool GlView::stagedCleaveTargetsMatchCurrent(const bool surfaceMode) const
{
    if ( !stagedCleaveContext_.valid || stagedCleaveContext_.surfaceMode != surfaceMode )
    {
        return false;
    }

    return stagedCleaveTargetsMatchContext(stagedCleaveContext_);
}

bool GlView::stagedCleaveTargetsMatchContext(const StagedCleaveContext& context) const
{
    if ( !context.valid )
    {
        return false;
    }

    std::vector<std::int32_t> sectors(
        highlightedSelection_.sectors.begin(),
        highlightedSelection_.sectors.end());
    std::vector<SurfaceRef> surfaces(
        highlightedSelection_.surfaces.begin(),
        highlightedSelection_.surfaces.end());
    if ( !context.surfaceMode && sectors.empty() && selectedSector_ >= 0 )
    {
        sectors.push_back(selectedSector_);
    }
    if ( context.surfaceMode && surfaces.empty()
        && selectedSurfaceSector_ >= 0 && selectedSurface_ >= 0 )
    {
        surfaces.push_back(SurfaceRef{ selectedSurfaceSector_, selectedSurface_ });
    }
    std::sort(sectors.begin(), sectors.end());
    std::sort(surfaces.begin(), surfaces.end(), [](const SurfaceRef& lhs, const SurfaceRef& rhs)
        {
            return lhs.sector == rhs.sector
                ? lhs.surface < rhs.surface
                : lhs.sector < rhs.sector;
        });

    return context.surfaceMode
        ? context.sourceSurface
                == SurfaceRef{ selectedSurfaceSector_, selectedSurface_ }
            && surfaces == context.targetSurfaces
        : sectors == context.targetSectors;
}

bool GlView::stagedCleaveContextMatchesCurrent(const bool surfaceMode)
{
    if (!stagedCleaveContext_.valid
        || stagedCleaveContext_.surfaceMode != surfaceMode
        || !stagedCleaveTargetsMatchCurrent(surfaceMode))
    {
        return false;
    }
    // Multi-cut geometry is world-authoritative for both Sector and Surface
    // workflows. Camera projection/view are retained only as the stable
    // construction reference for closed-loop validation and first-line angle
    // labels; orbiting, zooming, or resizing can never retarget a plane.
    return true;
}

bool GlView::stagedCleaveLinesFormSingleOpenChain() const noexcept
{
    if (stagedCleaveLines_.empty()
        || stagedCleaveLines_.front().linkedToPrevious
        || stagedCleaveLines_.back().linkedToNext)
    {
        return false;
    }
    for (std::size_t index = 0; index + 1 < stagedCleaveLines_.size(); ++index)
    {
        if (!stagedCleaveLines_[index].linkedToNext
            || !stagedCleaveLines_[index + 1].linkedToPrevious)
        {
            return false;
        }
    }
    return true;
}

bool GlView::stagedCleaveLinesFormSingleClosedChain() const noexcept
{
    return stagedCleaveLines_.size() >= 3
        && stagedCleaveLines_.front().linkedToPrevious
        && stagedCleaveLines_.back().linkedToNext
        && std::all_of(
            stagedCleaveLines_.begin(),
            stagedCleaveLines_.end(),
            [](const StagedCleaveLine& line)
            {
                return line.linkedToPrevious && line.linkedToNext;
            });
}

TVector GlView::stagedCleaveChainOriginPoint(
    const std::vector<StagedCleaveLine>& lines) const noexcept
{
    if ( lines.empty() )
    {
        return {};
    }
    if ( stagedCleaveChainOrigin_
        && stagedCleaveChainOrigin_->lineIndex >= 0
        && static_cast<std::size_t>(stagedCleaveChainOrigin_->lineIndex) < lines.size()
        && (stagedCleaveChainOrigin_->endpoint == 0
            || stagedCleaveChainOrigin_->endpoint == 1) )
    {
        const StagedCleaveLine& line = lines[
            static_cast<std::size_t>(stagedCleaveChainOrigin_->lineIndex)];
        return stagedCleaveChainOrigin_->endpoint == 0 ? line.first : line.second;
    }
    return lines.back().second;
}

TVector GlView::stagedCleaveChainOriginIncomingDirection(
    const std::vector<StagedCleaveLine>& lines) const noexcept
{
    if ( lines.empty() )
    {
        return {};
    }
    if ( stagedCleaveChainOrigin_
        && stagedCleaveChainOrigin_->lineIndex >= 0
        && static_cast<std::size_t>(stagedCleaveChainOrigin_->lineIndex) < lines.size()
        && (stagedCleaveChainOrigin_->endpoint == 0
            || stagedCleaveChainOrigin_->endpoint == 1) )
    {
        const StagedCleaveLine& line = lines[
            static_cast<std::size_t>(stagedCleaveChainOrigin_->lineIndex)];
        return stagedCleaveChainOrigin_->endpoint == 0
            ? line.second - line.first
            : line.first - line.second;
    }
    return lines.back().first - lines.back().second;
}

bool GlView::stagedCleaveChainOriginIsCanonicalTailSecond() const noexcept
{
    if ( stagedCleaveLines_.empty() )
    {
        return false;
    }
    if ( !stagedCleaveChainOrigin_ )
    {
        return true;
    }
    return stagedCleaveChainOrigin_->lineIndex
            == static_cast<std::int32_t>(stagedCleaveLines_.size() - 1)
        && stagedCleaveChainOrigin_->endpoint == 1;
}

bool GlView::beginSectorThrustDrag(
    const std::int32_t sectorIndex,
    const QPoint& mousePos,
    const QMatrix4x4& mvp,
    const QMatrix4x4& view,
    const QVector3D& eye,
    const QVector3D& target)
{
    if (!sectorThrustToolEnabled_ || renderPipeline_ != RenderPipeline::Editor
        || pickMode_ != PickMode::Sector || !level_ || level_->kind != ProjectType::IJIM
        || sectorIndex < 0 || static_cast<std::size_t>(sectorIndex) >= level_->sectors.size()
        || !selectedSectorThrusts_.contains(sectorIndex)
        || isLayerLocked(level_->sectors[static_cast<std::size_t>(sectorIndex)].layer))
    {
        return false;
    }

    const Sector& activeSector = level_->sectors[static_cast<std::size_t>(sectorIndex)];
    const TVector centerValue = sed::level::sectorCalcCenter(activeSector);
    const QVector3D center(centerValue.x, centerValue.y, centerValue.z);
    const ProjectedPoint center2d = projectWorldToWidget(mvp, center, width(), height());
    bool invOk = false;
    const QMatrix4x4 invMvp = mvp.inverted(&invOk);
    if (!center2d.valid || !invOk)
    {
        return false;
    }

    SectorThrustDragState drag{};
    drag.active = true;
    drag.activeSector = sectorIndex;
    drag.startMouse = mousePos;
    drag.originScreen = center2d.pos;
    drag.cameraRight = QVector3D(view(0, 0), view(0, 1), view(0, 2));
    drag.cameraUp = QVector3D(view(1, 0), view(1, 1), view(1, 2));
    drag.cameraForward = target - eye;
    if (!normalizeInPlace(drag.cameraRight))
    {
        drag.cameraRight = QVector3D(1.0f, 0.0f, 0.0f);
    }
    if (!normalizeInPlace(drag.cameraUp))
    {
        drag.cameraUp = QVector3D(0.0f, 0.0f, 1.0f);
    }
    if (!normalizeInPlace(drag.cameraForward))
    {
        drag.cameraForward = QVector3D(0.0f, 1.0f, 0.0f);
    }

    drag.baselines.reserve(selectedSectorThrusts_.size());
    drag.previews.reserve(selectedSectorThrusts_.size());
    for (const std::int32_t selectedIndex : selectedSectorThrusts_)
    {
        if (selectedIndex < 0 || static_cast<std::size_t>(selectedIndex) >= level_->sectors.size()
            || isLayerLocked(level_->sectors[static_cast<std::size_t>(selectedIndex)].layer))
        {
            continue;
        }
        const Sector& sector = level_->sectors[static_cast<std::size_t>(selectedIndex)];
        const QVector3D baseline(sector.thrust.x, sector.thrust.y, sector.thrust.z);
        drag.baselines.emplace_back(selectedIndex, baseline);
        drag.previews.emplace_back(selectedIndex, baseline);
    }
    if (drag.baselines.empty())
    {
        return false;
    }

    const QVector3D baseline(activeSector.thrust.x, activeSector.thrust.y, activeSector.thrust.z);
    const QVector3D direction = render::normalizedSectorThrustDirection(baseline);
    const float lengthPx = render::sectorThrustDisplayLengthPx(baseline.length());
    drag.activeDepthPx = QVector3D::dotProduct(direction, drag.cameraForward) * lengthPx;
    drag.baselineHandleRadiusPx = lengthPx;

    float unitsPerPx = worldUnitsPerPixelAt(center, mvp, invMvp, width(), height());
    if (!std::isfinite(unitsPerPx) || unitsPerPx <= 0.0f)
    {
        unitsPerPx = 0.01f;
    }
    if (const auto arrow = render::sectorThrustArrowGeometry(center, baseline, eye, unitsPerPx))
    {
        const ProjectedPoint tip = projectWorldToWidget(mvp, arrow->tip, width(), height());
        if (tip.valid)
        {
            drag.endpointOffset = tip.pos - QPointF(mousePos);
            const QPointF screenDelta = tip.pos - drag.originScreen;
            drag.baselineHandleRadiusPx = std::sqrt(
                static_cast<float>(screenDelta.x() * screenDelta.x()
                    + screenDelta.y() * screenDelta.y())
                + drag.activeDepthPx * drag.activeDepthPx);
        }
    }

    sectorThrustDrag_ = std::move(drag);
    setCursor(Qt::ClosedHandCursor);
    update();
    return true;
}

void GlView::updateSectorThrustDrag(const QPoint& mousePos)
{
    if (!sectorThrustDrag_.active || !level_)
    {
        return;
    }
    if (!sectorThrustDrag_.moved
        && (mousePos - sectorThrustDrag_.startMouse).manhattanLength() <= 2)
    {
        return;
    }
    sectorThrustDrag_.moved = true;

    QVector3D activeBaseline{};
    bool haveActive = false;
    for (const auto& [sectorIndex, baseline] : sectorThrustDrag_.baselines)
    {
        if (sectorIndex == sectorThrustDrag_.activeSector)
        {
            activeBaseline = baseline;
            haveActive = true;
            break;
        }
    }
    if (!haveActive)
    {
        return;
    }

    const QPointF endpoint = QPointF(mousePos) + sectorThrustDrag_.endpointOffset;
    const QPointF screenDelta = endpoint - sectorThrustDrag_.originScreen;
    QVector3D displayVector =
        sectorThrustDrag_.cameraRight * static_cast<float>(screenDelta.x())
        + sectorThrustDrag_.cameraUp * static_cast<float>(-screenDelta.y())
        + sectorThrustDrag_.cameraForward * sectorThrustDrag_.activeDepthPx;
    const float displayLengthPx = displayVector.length();
    const float baselineMagnitude = activeBaseline.length();

    QVector3D nextActive{};
    // Measure the radial delta from the handle's actual projected drag-start
    // radius. This avoids a first-move jump when perspective makes the
    // reconstructed screen/depth vector differ slightly from the theoretical
    // display length. The bounded helper also prevents an off-handle cursor
    // from feeding an arbitrarily large delta into the logarithmic mapping.
    const float magnitude = render::sectorThrustMagnitudeAfterHandleDragPx(
        baselineMagnitude,
        sectorThrustDrag_.baselineHandleRadiusPx,
        displayLengthPx);
    if (magnitude > 0.0f && normalizeInPlace(displayVector))
    {
        nextActive = displayVector * magnitude;
    }
    sectorThrustDrag_.previews.clear();
    sectorThrustDrag_.previews.reserve(sectorThrustDrag_.baselines.size());
    for (const auto& [sectorIndex, baseline] : sectorThrustDrag_.baselines)
    {
        (void)baseline;
        sectorThrustDrag_.previews.emplace_back(sectorIndex, nextActive);
    }
    update();
}

void GlView::endSectorThrustDrag()
{
    if (!sectorThrustDrag_.active)
    {
        return;
    }

    QVector3D nextThrust{};
    bool haveChange = false;
    if (sectorThrustDrag_.moved)
    {
        for (const auto& [sectorIndex, preview] : sectorThrustDrag_.previews)
        {
            if (sectorIndex != sectorThrustDrag_.activeSector)
            {
                continue;
            }
            for (const auto& [baselineIndex, baseline] : sectorThrustDrag_.baselines)
            {
                if (baselineIndex == sectorIndex)
                {
                    nextThrust = preview;
                    haveChange = (preview - baseline).lengthSquared() > 1.0e-18f;
                    break;
                }
            }
            break;
        }
    }

    QList<std::int32_t> indices{};
    indices.reserve(static_cast<qsizetype>(sectorThrustDrag_.baselines.size()));
    for (const auto& [sectorIndex, baseline] : sectorThrustDrag_.baselines)
    {
        (void)baseline;
        indices.push_back(sectorIndex);
    }
    sectorThrustDrag_ = {};
    setCursor(Qt::OpenHandCursor);
    update();

    if (haveChange)
    {
        Q_EMIT sectorThrustVectorsRequested(indices, nextThrust);
    }
}


std::optional<GlView::PlacementPoint> GlView::placementPointAt(
    const QPoint& widgetPos,
    const bool snapToSurfaceHit,
    const bool freePlacement)
{
    if (width() <= 0 || height() <= 0)
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

    const bool isOrtho = camera_.projectionType() == app::ViewportCamera::ProjectionType::Orthographic;
    // Measure from the ray origin used by viewportRayForWidget() to this
    // pixel's actual far-plane endpoint. Scalar camera planes are axial
    // distances: they truncate the full ortho interval and off-axis perspective
    // rays. Keep a conservative scalar fallback if inverse projection fails.
    const double fallbackRayMaxDistance = isOrtho
        ? (camera_.clipEnabled()
            ? std::max(0.0, static_cast<double>(farPlane) - nearPlane)
            : 2.0 * -static_cast<double>(app::ViewportCamera::kOrthoNearPlaneMin))
        : std::max(0.0, static_cast<double>(farPlane));
    const double placementRayMaxDistance = render::viewportRayMaxDistanceForWidget(
        proj, view, QPointF(widgetPos), width(), height(), isOrtho)
        .value_or(fallbackRayMaxDistance);
    const auto ray = render::viewportRayForWidget(proj, view, eye, widgetPos, width(), height(), isOrtho);
    if (!ray)
    {
        return std::nullopt;
    }
    const TVector origin = ray->origin;
    const TVector direction = ray->direction;
    const TVector targetPoint{target.x(), target.y(), target.z()};
    PlacementPoint out{};
    if (isOrtho)
    {
        out.documentPosition = origin + direction * (targetPoint - origin).Dot(direction);
    }
    else if (freePlacement)
    {
        const double targetDistance = (targetPoint - origin).Dot(direction);
        out.documentPosition = std::isfinite(targetDistance) && targetDistance > nearPlane
            ? origin + direction * std::min(targetDistance, placementRayMaxDistance) : targetPoint;
    }
    else
    {
        const float distScale = surfaceClickPreferSurfaceHit_ ? 0.15f : 0.06f;
        const float distMax = surfaceClickPreferSurfaceHit_ ? 16.0f : 6.0f;
        const double distance = std::clamp(camera_.orbit.distance * distScale, 0.25f, distMax);
        out.documentPosition = origin + direction * distance;
    }
    out.position = QVector3D(static_cast<float>(out.documentPosition.x),
        static_cast<float>(out.documentPosition.y), static_cast<float>(out.documentPosition.z));

    if (freePlacement && level_)
    {
        const std::vector<sed::level::SurfacePickHit> hits =
            surfacePickIndex_.pickSurfaces(*level_, origin, direction, placementRayMaxDistance);

        constexpr double kInsideSampleOffset = 0.03;
        const TVector fallback = out.documentPosition;
        auto acceptFreePoint = [&](
            const sed::TVector& anchor,
            const sed::TVector& insideSample,
            const std::int32_t preferredSector = -1) -> bool
        {
            auto contains = [&](const std::int32_t sectorIndex) -> bool
            {
                return sectorIndex >= 0
                    && static_cast<std::size_t>(sectorIndex) < level_->sectors.size()
                    && sed::level::sectorContainsPoint(
                        level_->sectors[static_cast<std::size_t>(sectorIndex)],
                        insideSample.x,
                        insideSample.y,
                        insideSample.z);
            };

            std::int32_t sector = contains(preferredSector)
                ? preferredSector
                : (contains(lastPlacementSector_) ? lastPlacementSector_ : -1);
            if (sector < 0)
            {
                sector = findContainingSector(
                    insideSample,
                    preferredSector);
            }
            if (sector < 0)
            {
                return false;
            }

            lastPlacementSector_ = sector;

            out.documentPosition = anchor;
            out.position = QVector3D(
                static_cast<float>(anchor.x),
                static_cast<float>(anchor.y),
                static_cast<float>(anchor.z));
            out.surface = SurfaceRef{ sector, -1 };
            out.surfaceNormal = QVector3D(0.0f, 0.0f, 1.0f);
            out.haveSurfaceHit = false;
            return true;
        };

        if (acceptFreePoint(fallback, fallback))
        {
            return out;
        }

        for (const sed::level::SurfacePickHit& hit : hits)
        {
            const sed::TVector before = hit.position - direction.Scaled(kInsideSampleOffset);
            const sed::TVector after = hit.position + direction.Scaled(kInsideSampleOffset);
            if (acceptFreePoint(hit.position, before, hit.surface.sector)
                || acceptFreePoint(hit.position, after, hit.surface.sector))
            {
                return out;
            }
        }

        (void)acceptFreePoint(fallback, fallback);
        return out;
    }

    const bool wantsSurfaceHit = surfaceClickPreferSurfaceHit_ || snapToSurfaceHit;
    if (wantsSurfaceHit && level_)
    {
        const std::vector<sed::level::SurfacePickHit> hits =
            surfacePickIndex_.pickSurfaces(*level_, origin, direction, placementRayMaxDistance);

        auto normalForHit = [&](const sed::level::SurfacePickHit& hit) -> std::optional<TVector>
        {
            if (hit.surface.sector < 0
                || static_cast<std::size_t>(hit.surface.sector) >= level_->sectors.size())
            {
                return std::nullopt;
            }

            const Sector& sector = level_->sectors[static_cast<std::size_t>(hit.surface.sector)];
            if (hit.surface.surface < 0
                || static_cast<std::size_t>(hit.surface.surface) >= sector.surfaces.size())
            {
                return std::nullopt;
            }

            const Surface& surface = sector.surfaces[static_cast<std::size_t>(hit.surface.surface)];
            TVector normal = surface.normal;
            if (normal.IsZero())
            {
                normal = sed::level::surfaceCalcNormalNewell(sector, surface);
            }
            if (normal.Normalize() == 0.0)
            {
                return std::nullopt;
            }
            return normal;
        };

        auto isPassThroughPlacementSurface = [&](const sed::level::SurfacePickHit& hit) -> bool
        {
            if (hit.surface.sector < 0
                || static_cast<std::size_t>(hit.surface.sector) >= level_->sectors.size())
            {
                return false;
            }

            const Sector& sector = level_->sectors[static_cast<std::size_t>(hit.surface.sector)];
            if (hit.surface.surface < 0
                || static_cast<std::size_t>(hit.surface.surface) >= sector.surfaces.size())
            {
                return false;
            }

            const Surface& surface = sector.surfaces[static_cast<std::size_t>(hit.surface.surface)];
            if (!surface.adjoin)
            {
                return false;
            }

            const bool noCollision = (surface.surfaceFlags & SF_Collision) == 0;
            const bool notSolidGeo = surface.geo < static_cast<std::int32_t>(TGeoMode::Solid);
            const std::string material = sed::text::lowerAscii(sed::text::trimView(surface.material));
            const bool noMaterial = material.empty() || material == "-" || material == "none";
            return noCollision && (notSolidGeo || noMaterial);
        };

        std::optional<sed::level::SurfacePickHit> chosenHit{};
        TVector chosenNormal{};
        int chosenPreference = std::numeric_limits<int>::max();
        double chosenDistance = std::numeric_limits<double>::infinity();

        auto orthoSurfacePreference = [&](const TVector& normal) -> int
        {
            if (!isOrtho || std::abs(direction.z) < 0.15)
            {
                return 0;
            }

            // In angled ortho views, actors are usually being placed on the sector floor,
            // even when the view ray crosses a wall face first. Prefer the walking plane
            // so placing things does not feel like threading a needle through wall overlap.
            if (direction.z < 0.0 && normal.z > 0.35)
            {
                return 0;
            }
            if (direction.z > 0.0 && normal.z < -0.35)
            {
                return 0;
            }
            return 1;
        };

        for (const sed::level::SurfacePickHit& hit : hits)
        {
            if (isPassThroughPlacementSurface(hit))
            {
                continue;
            }

            const std::optional<TVector> normal = normalForHit(hit);
            if (!normal)
            {
                continue;
            }
            TVector placementNormal = *normal;

            // Placement should only snap to the visible/front side of sector surfaces.
            // A reversed wall normal can otherwise grab the ray from its hidden side and rotate
            // actors into the wall instead of letting the floor receive the placement.
            if (placementNormal.Dot(direction) >= -0.025)
            {
                continue;
            }

            if (!isOrtho)
            {
                chosenHit = hit;
                chosenNormal = placementNormal;
                break;
            }

            const int preference = orthoSurfacePreference(placementNormal);
            if (preference < chosenPreference
                || (preference == chosenPreference && hit.distance < chosenDistance))
            {
                chosenHit = hit;
                chosenNormal = placementNormal;
                chosenPreference = preference;
                chosenDistance = hit.distance;
            }
        }

        if (isOrtho && snapToSurfaceHit && chosenPreference > 0 && std::abs(direction.z) >= 0.15)
        {
            const TVector verticalDirection = (direction.z < 0.0)
                ? TVector{ 0.0, 0.0, -1.0 }
                : TVector{ 0.0, 0.0, 1.0 };

            auto tryWalkingPlaneSample = [&](
                const TVector& sample,
                const std::int32_t expectedSector) -> bool
            {
                std::int32_t sectorIndex = expectedSector;
                if (expectedSector >= 0)
                {
                    if (static_cast<std::size_t>(expectedSector) >= level_->sectors.size()
                        || !sed::level::sectorContainsPoint(
                            level_->sectors[static_cast<std::size_t>(expectedSector)],
                            sample.x,
                            sample.y,
                            sample.z))
                    {
                        return false;
                    }
                }
                else
                {
                    sectorIndex = sed::level::levelFindSectorForXYZ(
                        *level_, sample.x, sample.y, sample.z);
                    if (sectorIndex < 0)
                    {
                        return false;
                    }
                }

                const Sector& sector = level_->sectors[static_cast<std::size_t>(sectorIndex)];
                double sectorMinZ = std::numeric_limits<double>::infinity();
                double sectorMaxZ = -std::numeric_limits<double>::infinity();
                for (const Vertex& vertex : sector.vertices)
                {
                    if (!std::isfinite(vertex.z))
                    {
                        continue;
                    }
                    sectorMinZ = std::min(sectorMinZ, vertex.z);
                    sectorMaxZ = std::max(sectorMaxZ, vertex.z);
                }
                if (!std::isfinite(sectorMinZ) || !std::isfinite(sectorMaxZ))
                {
                    return false;
                }

                const double sectorBoundaryZ = verticalDirection.z < 0.0 ? sectorMinZ : sectorMaxZ;
                const double verticalDistance =
                    (sectorBoundaryZ - sample.z) / verticalDirection.z;
                if (!std::isfinite(verticalDistance))
                {
                    return false;
                }
                const double coordinateScale = std::max({
                    1.0,
                    std::abs(sample.z),
                    std::abs(sectorBoundaryZ)});
                const double verticalEpsilon = std::max(
                    1.0e-6,
                    16.0 * std::numeric_limits<double>::epsilon() * coordinateScale);
                const double verticalRayMaxDistance = std::max(0.0, verticalDistance) + verticalEpsilon;
                const std::vector<sed::level::SurfacePickHit> verticalHits =
                    surfacePickIndex_.pickSurfaces(
                        *level_, sample, verticalDirection, verticalRayMaxDistance);
                for (const sed::level::SurfacePickHit& verticalHit : verticalHits)
                {
                    if (verticalHit.surface.sector != sectorIndex)
                    {
                        continue;
                    }
                    if (isPassThroughPlacementSurface(verticalHit))
                    {
                        continue;
                    }

                    const std::optional<TVector> normal = normalForHit(verticalHit);
                    if (!normal || normal->Dot(verticalDirection) >= -0.025)
                    {
                        continue;
                    }
                    if (direction.z < 0.0 && normal->z <= 0.35)
                    {
                        continue;
                    }
                    if (direction.z > 0.0 && normal->z >= -0.35)
                    {
                        continue;
                    }

                    chosenHit = verticalHit;
                    chosenNormal = *normal;
                    chosenPreference = 0;
                    chosenDistance = verticalHit.distance;
                    return true;
                }

                return false;
            };

            auto tryOrthoWalkingPlaneFromRay = [&]() -> bool
            {
                constexpr double kLegacyInsideSampleOffset = 0.02;
                for (const sed::level::SurfacePickHit& entryHit : hits)
                {
                    for (const double sampleOffset : {
                             kLegacyInsideSampleOffset,
                             -kLegacyInsideSampleOffset })
                    {
                        const TVector sample = entryHit.position + direction.Scaled(sampleOffset);
                        if (tryWalkingPlaneSample(sample, -1))
                        {
                            return true;
                        }
                    }
                }

                // Fixed probes can jump completely across a thin sector when
                // both its entry and exit surfaces are less than two probe
                // offsets apart. Probe the interior of only those short
                // same-sector hit intervals; containment is still verified
                // before launching the vertical floor ray.
                constexpr double kMinimumInteriorInterval = sed::math::kGeometryEpsilon;
                constexpr double kMaximumInteriorInterval =
                    kLegacyInsideSampleOffset * 2.0;
                for (std::size_t i = 0; i + 1u < hits.size(); ++i)
                {
                    const sed::level::SurfacePickHit& entryHit = hits[i];
                    const sed::level::SurfacePickHit& exitHit = hits[i + 1u];
                    const double interval = exitHit.distance - entryHit.distance;
                    if (entryHit.surface.sector < 0
                        || entryHit.surface.sector != exitHit.surface.sector
                        || !std::isfinite(interval)
                        || interval <= kMinimumInteriorInterval
                        || interval > kMaximumInteriorInterval)
                    {
                        continue;
                    }

                    const TVector interiorSample = entryHit.position
                        + direction.Scaled(interval * 0.5);
                    if (tryWalkingPlaneSample(interiorSample, entryHit.surface.sector))
                    {
                        return true;
                    }
                }

                return false;
            };

            (void)tryOrthoWalkingPlaneFromRay();
        }

        if (!chosenHit && isOrtho && snapToSurfaceHit)
        {
            const TVector fallbackOrigin = out.documentPosition;

            auto tryPlacementFallbackRay = [&](const sed::TVector& fallbackDirection) -> bool
            {
                const std::vector<sed::level::SurfacePickHit> fallbackHits =
                    surfacePickIndex_.pickSurfaces(*level_, fallbackOrigin, fallbackDirection, placementRayMaxDistance);
                for (const sed::level::SurfacePickHit& hit : fallbackHits)
                {
                    if (isPassThroughPlacementSurface(hit))
                    {
                        continue;
                    }

                    const std::optional<TVector> normal = normalForHit(hit);
                    if (!normal || normal->Dot(fallbackDirection) >= -0.025)
                    {
                        continue;
                    }

                    chosenHit = hit;
                    chosenNormal = *normal;
                    return true;
                }
                return false;
            };

            (void)(tryPlacementFallbackRay(TVector{ 0.0, 0.0, -1.0 })
                || tryPlacementFallbackRay(TVector{ 0.0, 0.0, 1.0 }));
        }

        if (chosenHit)
        {
            out.documentPosition = chosenHit->position;
            out.position = QVector3D(
                static_cast<float>(chosenHit->position.x),
                static_cast<float>(chosenHit->position.y),
                static_cast<float>(chosenHit->position.z));
            out.surface = chosenHit->surface;
            out.surfaceNormal = QVector3D(
                static_cast<float>(chosenNormal.x),
                static_cast<float>(chosenNormal.y),
                static_cast<float>(chosenNormal.z));
            out.haveSurfaceHit = true;
        }
    }

    if (!std::isfinite(out.position.x()) || !std::isfinite(out.position.y()) || !std::isfinite(out.position.z()))
    {
        return std::nullopt;
    }

    return out;
}

std::optional<QVector3D> GlView::placementPointForViewport(
    const QPoint& widgetPos,
    bool* haveSurfaceHit,
    SurfaceRef* surfaceRef,
    QVector3D* surfaceNormal,
    const bool snapToSurfaceHit,
    const bool freePlacement,
    TVector* documentPosition)
{
    const std::optional<PlacementPoint> point = placementPointAt(widgetPos, snapToSurfaceHit, freePlacement);
    if (!point)
    {
        if (haveSurfaceHit)
        {
            *haveSurfaceHit = false;
        }
        if (surfaceRef)
        {
            *surfaceRef = {};
        }
        if (surfaceNormal)
        {
            *surfaceNormal = {};
        }
        return std::nullopt;
    }

    if (haveSurfaceHit)
    {
        *haveSurfaceHit = point->haveSurfaceHit;
    }
    if (surfaceRef)
    {
        *surfaceRef = point->surface;
    }
    if (surfaceNormal)
    {
        *surfaceNormal = point->surfaceNormal;
    }
    if (documentPosition)
    {
        *documentPosition = point->documentPosition;
    }
    return point->position;
}

std::vector<sed::level::SurfacePickHit> GlView::placementSurfaceHits(
    const TVector& rayOrigin,
    const TVector& rayDirection,
    const double maxDistance) const
{
    if (!level_)
    {
        return {};
    }
    return surfacePickIndex_.pickSurfaces(
        *level_, rayOrigin, rayDirection, maxDistance);
}

void GlView::mousePressEvent(QMouseEvent* event)
{
    event->accept();
    // A new pointer owner settles the wheel command first. Additional buttons
    // cannot replace the gesture that already owns the viewport.
    (void)finishStagedCleaveWheelSession();
    if (viewportMouseGesture_.button != Qt::NoButton)
    {
        return;
    }
    const ViewportMouseAction action = viewportMouseActionForPress(
        effectiveViewportNavigationSettings(), event->button(), event->modifiers());
    if (action == ViewportMouseAction::none)
    {
        return;
    }
    viewportMouseGesture_ = {
        event->button(), action,
        effectiveViewportNavigationSettings().mouse[static_cast<std::size_t>(action)].modifiers,
    };
    const Qt::MouseButton semanticButton = viewportMouseSemanticButton();
    QMouseEvent routedEvent(event->type(), event->position(), event->globalPosition(),
        semanticButton, semanticButton, viewportMouseSemanticModifiers(event->modifiers()),
        event->pointingDevice());
    event = &routedEvent;

    if (event->button() == Qt::MiddleButton)
    {
        // Selection is a drag-scoped revolution pivot, not the camera's look
        // target. Merely pressing MMB therefore cannot recenter or jump the
        // view; the eye and target are rotated together only after movement.
        const ui::viewport::SelectionOrbitFallback fallback{
            selectedSector_,
            selectedSurfaceSector_,
            selectedSurface_,
            selectedThing_,
            selectedLight_,
            selectedFrameValueIndex_,
        };
        const Qt::KeyboardModifiers navigationModifiers =
            event->modifiers() & (Qt::ControlModifier | Qt::ShiftModifier);
        std::optional<QVector3D> nextPivot;
        if (orbitAroundSelectionEnabled_ && level_ && navigationModifiers == Qt::NoModifier)
        {
            bool resolved = true;
#ifdef SED_APP_MAIN_WINDOW_TEST_ACCESS
            resolved = !property("testOrbitPivotAllocationFailure").toBool();
#endif
            if (resolved)
            {
                resolved = ui::viewport::trySelectionOrbitPivot(*level_, highlightedSelection_, fallback, nextPivot);
            }
            if (!resolved)
            {
                // Reject this complete gesture: later MMB move samples must
                // not silently fall back to an unrelated camera-target orbit.
                selectionOrbitDragRejected_ = true;
                event->accept();
                return;
            }
        }
        selectionOrbitDragRejected_ = false;
        selectionOrbitDragPivot_ = nextPivot;
        lastMousePos_ = mouseEventPos(*event);
        event->accept();
        return;
    }
    lastMousePos_ = mouseEventPos(*event);
#ifdef SED_APP_MAIN_WINDOW_TEST_ACCESS
    const bool traceCleavePress = event->type() == QEvent::MouseButtonPress
        && event->button() == Qt::LeftButton
        && property("testCleavePressRouteDiagnostics").toBool();
    if ( traceCleavePress )
    {
        std::fprintf(
            stderr,
            "Cleave press entry: pos=(%d,%d) picking=%d placementGesture=%d sectorTool=%d surfaceTool=%d staged=%d numbers=%d lines=%zu shapeArmed=%d axisVisible=%d\n",
            lastMousePos_.x(),
            lastMousePos_.y(),
            viewportPickingEnabled_ ? 1 : 0,
            placementGestureEnabled_ ? 1 : 0,
            sectorCleaveToolEnabled_ ? 1 : 0,
            surfaceCleaveToolEnabled_ ? 1 : 0,
            cleaveStagedMode_ ? 1 : 0,
            cleaveLineNumbersVisible_ ? 1 : 0,
            stagedCleaveLines_.size(),
            cleaveShapeOptions_.placementArmed ? 1 : 0,
            showAxisWidget_ ? 1 : 0);
    }
#endif
    if (event->button() == Qt::LeftButton)
    {
        // A new press starts a fresh gesture. This also clears a defensive
        // stale double-click marker if a platform drops the trailing release.
        suppressNextArchitecturalLightingVertexRelease_ = false;
        axisWidgetPressed_ = false;
        axisWidgetDragging_ = false;

        const QRect axisRect = axisWidgetRectForViewport(this, width(), height());
        const AxisWidgetLayout axis = computeAxisWidgetLayout(camera_.viewMatrix(), axisRect);
        if (axis.rect.isValid())
        {
            const QPointF d = QPointF(lastMousePos_) - axis.center;
            const float dist2 = static_cast<float>((d.x() * d.x()) + (d.y() * d.y()));
            const float outer2 = (axis.outerRadius + 1.0f) * (axis.outerRadius + 1.0f);
            if (dist2 <= outer2)
            {
                axisWidgetPressed_ = true;
                axisWidgetLastPos_ = lastMousePos_;

                // Check for handle clicks.
                int bestAxis = -1;
                int bestSign = 0;
                float bestD2 = std::numeric_limits<float>::infinity();

                std::array<bool, 3> axisCollapsed{};
                {
                    const float collapseDist = axis.frontRadius * 0.75f;
                    const float collapseDist2 = collapseDist * collapseDist;
                    for (int a = 0; a < 3; ++a)
                    {
                        const AxisWidgetEnd& ePlus = axis.ends[static_cast<std::size_t>(a) * 2 + 0];
                        const AxisWidgetEnd& eMinus = axis.ends[static_cast<std::size_t>(a) * 2 + 1];
                        const QPointF dd = ePlus.pos - eMinus.pos;
                        const float d2 = static_cast<float>((dd.x() * dd.x()) + (dd.y() * dd.y()));
                        axisCollapsed[static_cast<std::size_t>(a)] = d2 <= collapseDist2;
                    }
                }

                for (int a = 0; a < 3; ++a)
                {
                    const AxisWidgetEnd& ePlus = axis.ends[static_cast<std::size_t>(a) * 2 + 0];
                    const AxisWidgetEnd& eMinus = axis.ends[static_cast<std::size_t>(a) * 2 + 1];

                    if (axisCollapsed[static_cast<std::size_t>(a)])
                    {
                        const AxisWidgetEnd& front = ePlus.front ? ePlus : eMinus;
                        const float r = axis.frontRadius;
                        const QPointF ed = QPointF(lastMousePos_) - axis.center;
                        const float ed2 = static_cast<float>((ed.x() * ed.x()) + (ed.y() * ed.y()));
                        if (ed2 <= r * r && ed2 < bestD2)
                        {
                            bestAxis = front.axis;
                            bestSign = front.sign;
                            bestD2 = ed2;
                        }
                        continue;
                    }

                    for (const AxisWidgetEnd& e : { ePlus, eMinus })
                    {
                        const float r = e.front ? axis.frontRadius : axis.backRadius;
                        const QPointF ed = QPointF(lastMousePos_) - e.pos;
                        const float ed2 = static_cast<float>((ed.x() * ed.x()) + (ed.y() * ed.y()));
                        if (ed2 <= r * r && ed2 < bestD2)
                        {
                            bestAxis = e.axis;
                            bestSign = e.sign;
                            bestD2 = ed2;
                        }
                    }
                }

        if (bestAxis >= 0)
        {
#ifdef SED_APP_MAIN_WINDOW_TEST_ACCESS
                    if ( traceCleavePress )
                    {
                        std::fprintf(stderr, "Cleave press route: axis-widget handle axis=%d sign=%d\n",
                            bestAxis, bestSign);
                    }
#endif
                    // Map +/- axis to a canonical orbit view.
                    if (bestAxis == 0)
                    {
                        orbitSetView(bestSign > 0 ? 90.0f : -90.0f, 0.0f);
                    }
                    else if (bestAxis == 1)
                    {
                        orbitSetView(bestSign > 0 ? 180.0f : 0.0f, 0.0f);
                    }
                    else
                    {
                        orbitSetView(0.0f, bestSign > 0 ? 90.0f : -90.0f);
                    }

                    event->accept();
                    return;
                }

                // Otherwise, allow drag-rotate within the widget.
#ifdef SED_APP_MAIN_WINDOW_TEST_ACCESS
                if ( traceCleavePress )
                {
                    std::fprintf(stderr, "Cleave press route: axis-widget drag\n");
                }
#endif
                axisWidgetDragging_ = true;
                cancelViewAnimation();
                event->accept();
                return;
            }
        }

        if (!viewportPickingEnabled_)
        {
#ifdef SED_APP_MAIN_WINDOW_TEST_ACCESS
            if ( traceCleavePress )
            {
                std::fprintf(stderr, "Cleave press route: viewport picking disabled\n");
            }
#endif
            // View-only mode: allow the axis widget and preview-only orbit drag, but ignore selection/picking.
            leftPressPos_ = lastMousePos_;
            leftDragged_ = false;
            boxSelectActive_ = false;
            boxSelectVisible_ = false;
            if (previewInteractionEnabled_)
            {
                previewInteractionDragging_ = true;
                cancelViewAnimation();
                setFocus(Qt::MouseFocusReason);
            }
            event->accept();
            return;
        }

        if (placementGestureEnabled_)
        {
#ifdef SED_APP_MAIN_WINDOW_TEST_ACCESS
            if ( traceCleavePress )
            {
                std::fprintf(stderr, "Cleave press route: generic placement gesture\n");
            }
#endif
            leftPressPos_ = lastMousePos_;
            leftDragged_ = false;
            boxSelectActive_ = false;
            boxSelectVisible_ = false;
            setFocus(Qt::MouseFocusReason);

            if (const std::optional<PlacementPoint> point = placementPointAt(lastMousePos_))
            {
                Q_EMIT placementGestureStarted(point->position, point->haveSurfaceHit, lastMousePos_, event->modifiers());
            }
            else
            {
                // Camera-plane placement modes resolve their own interaction
                // plane in MainWindow and must not lose the complete drag just
                // because the generic surface/grid probe has no hit.
                Q_EMIT placementGestureStarted(
                    cameraTarget(), false, lastMousePos_, event->modifiers());
            }

            event->accept();
            return;
        }

        if (sectorThrustToolEnabled_)
        {
#ifdef SED_APP_MAIN_WINDOW_TEST_ACCESS
            if ( traceCleavePress )
            {
                std::fprintf(stderr, "Cleave press route: Sector thrust tool\n");
            }
#endif
            leftPressPos_ = lastMousePos_;
            leftDragged_ = false;
            boxSelectActive_ = false;
            boxSelectVisible_ = false;
            setFocus(Qt::MouseFocusReason);

            QMatrix4x4 proj;
            QMatrix4x4 view;
            QVector3D eye{};
            QVector3D target{};
            float nearPlane{};
            float farPlane{};
            buildCameraMatrices(proj, view, eye, target, nearPlane, farPlane);
            (void)nearPlane;
            (void)farPlane;
            const QMatrix4x4 mvp = proj * view;
            bool invOk = false;
            const QMatrix4x4 invMvp = mvp.inverted(&invOk);
            const std::optional<std::int32_t> hit = invOk
                ? hitTestSectorThrustArrow(lastMousePos_, mvp, invMvp, eye)
                : std::nullopt;

            const std::int32_t pickedSector = hit.value_or(-1);
            Q_EMIT clickPicked(pickedSector, -1, -1, -1);
            if (hit)
            {
                // The thrust modification tool owns normal entity picking, so
                // explicitly route its handle hit through selection. The
                // direct connection refreshes selectedSectorThrusts_ before
                // beginSectorThrustDrag() below (Ctrl still toggles).
                Q_EMIT selectionPicked(*hit, -1, -1, -1, event->modifiers());
                if (selectedSectorThrusts_.contains(*hit))
                {
                    setSelectionState(*hit, -1, -1, -1, highlightedSelection_);
                    Q_EMIT sectorThrustPrimaryChanged(*hit);
                }
            }

            hoveredSectorThrust_ = pickedSector;
            if (hit && selectedSectorThrusts_.contains(*hit))
            {
                (void)beginSectorThrustDrag(*hit, lastMousePos_, mvp, view, eye, target);
            }
            else
            {
                setCursor(hit ? Qt::ArrowCursor : Qt::OpenHandCursor);
                update();
            }
            event->accept();
            return;
        }

        if (sectorCleaveToolEnabled_ || surfaceCleaveToolEnabled_)
        {
#ifdef SED_APP_MAIN_WINDOW_TEST_ACCESS
            if ( traceCleavePress )
            {
                std::fprintf(
                    stderr,
                    "Cleave press route: cleave branch staged=%d numbers=%d lines=%zu shapeArmed=%d\n",
                    cleaveStagedMode_ ? 1 : 0,
                    cleaveLineNumbersVisible_ ? 1 : 0,
                    stagedCleaveLines_.size(),
                    cleaveShapeOptions_.placementArmed ? 1 : 0);
            }
#endif
            // Tool mode: start a cleave drag; do not start selection/transform drags.
            stagedCleaveEndpointOriginClickCandidate_ = false;
            stagedCleaveLineClickCandidate_ = false;
            stagedCleaveDragSelectionApplied_ = false;
            stagedCleaveJointCycleClickCandidate_ = false;
            stagedCleaveJointCycleCandidates_.clear();
            stagedCleavePressModifiers_ = event->modifiers();
            updateCleaveSnapGuideVisibility(event->modifiers());
            cleaveSnapMarker_.reset();
            leftPressPos_ = lastMousePos_;
            leftDragged_ = false;
            boxSelectActive_ = false;
            boxSelectVisible_ = false;

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
#ifdef SED_APP_MAIN_WINDOW_TEST_ACCESS
                if ( traceCleavePress )
                {
                    std::fprintf(stderr, "Cleave press route: camera inverse failed\n");
                }
#endif
                event->accept();
                return;
            }

            const bool isOrtho = camera_.projectionType() == app::ViewportCamera::ProjectionType::Orthographic;
            const Qt::KeyboardModifiers mods = event->modifiers();

            if ( cleaveStagedMode_ && cleaveShapeOptions_.placementArmed )
            {
                stagedCleaveShapePlacementClickPending_ = true;
                leftPressPos_ = lastMousePos_;
                leftDragged_ = false;
                stagedCleaveShapePreviewFrame_ =
                    stagedCleaveShapePlacementFrameAt(lastMousePos_, mods);
                stagedCleaveShapePreviewCenter_ =
                    stagedCleaveShapePreviewFrame_
                    ? std::optional<QVector3D>(render::cleaveRenderPoint(
                        stagedCleaveShapePreviewFrame_->center))
                    : std::nullopt;
                update();
                event->accept();
                return;
            }

            if ( cleaveStagedMode_ && cleaveLineNumbersVisible_
                && !stagedCleaveLines_.empty() )
            {
                auto* const mutableThis = const_cast<GlView*>(this);
                if ( !mutableThis->stagedCleaveRankCacheValid_ )
                {
                    mutableThis->rebuildStagedCleaveRankCache();
                }
                for ( std::size_t index = 0;
                    index < stagedCleaveLines_.size();
                    ++index )
                {
                    const QRect label = stagedCleaveOrderLabelRect(
                        static_cast<std::int32_t>(index),
                        mvp);
                    const bool containsPress = label.contains(lastMousePos_);
#ifdef SED_APP_MAIN_WINDOW_TEST_ACCESS
                    if ( traceCleavePress )
                    {
                        std::fprintf(
                            stderr,
                            "Cleave rank hit candidate: line=%zu rank=%d rect=(%d,%d %dx%d) contains=%d press=(%d,%d)\n",
                            index,
                            index < stagedCleaveRankByLine_.size()
                                ? stagedCleaveRankByLine_[index]
                                : -1,
                            label.x(),
                            label.y(),
                            label.width(),
                            label.height(),
                            containsPress ? 1 : 0,
                            lastMousePos_.x(),
                            lastMousePos_.y());
                    }
#endif
                    const bool began = containsPress
                        && beginStagedCleaveOrderEdit(
                            static_cast<std::int32_t>(index),
                            label);
#ifdef SED_APP_MAIN_WINDOW_TEST_ACCESS
                    if ( traceCleavePress && containsPress )
                    {
                        std::fprintf(
                            stderr,
                            "Cleave rank editor begin: line=%zu began=%d\n",
                            index,
                            began ? 1 : 0);
                    }
#endif
                    if ( began )
                    {
                        event->accept();
                        return;
                    }
                }
            }

            if (cleaveStagedMode_ && !stagedCleaveLines_.empty())
            {
                const bool surfaceMode = surfaceCleaveToolEnabled_;
                if (!stagedCleaveContextMatchesCurrent(surfaceMode))
                {
                    showToast(
                        surfaceMode
                            ? "Multi-cut: restore the original source face and target selection before editing"
                            : "Multi-cut: restore the original target selection before editing",
                        3200,
                        ToastPlacement::BottomCenter,
                        ToastGroup::Error);
                    event->accept();
                    return;
                }

                constexpr double kEndpointPickRadiusPx = 10.0;
                constexpr double kLinePickRadiusPx = 7.0;
                const auto planeDragWorksetFitsBudget = [this](
                    const std::int32_t lineIndex,
                    const std::int32_t endpoint,
                    const bool includeGestureAdmission)
                {
                    (void)endpoint;
                    if ( lineIndex < 0
                        || static_cast<std::size_t>(lineIndex)
                            >= stagedCleaveLines_.size() )
                    {
                        return false;
                    }
                    const bool movingSelectedGroup = stagedCleaveLineSelected(lineIndex)
                        && selectedStagedCleaveLines_.size() > 1;
                    const std::size_t movingCount = movingSelectedGroup
                        ? selectedStagedCleaveLines_.size()
                        : 1u;
                    using HandleKey = std::tuple<double, double, double>;
                    std::size_t workBytes = 0u;
                    const auto addArray = [&workBytes](
                                              const std::size_t count,
                                              const std::size_t elementSize)
                    {
                        constexpr std::size_t kMaximum =
                            std::numeric_limits<std::size_t>::max();
                        if ( elementSize != 0u
                            && count > (kMaximum - workBytes) / elementSize )
                        {
                            workBytes = kMaximum;
                            return;
                        }
                        workBytes += count * elementSize;
                    };
                    addArray(movingCount, sizeof(std::int32_t));
                    const std::size_t mapNodeCount = movingCount
                            > std::numeric_limits<std::size_t>::max() / 2u
                        ? std::numeric_limits<std::size_t>::max()
                        : movingCount * 2u;
                    addArray(
                        mapNodeCount,
                        sizeof(HandleKey) + sizeof(bool) + 64u);
                    addArray(
                        stagedCleaveLines_.size(),
                        sizeof(StagedCleaveLineMove));

                    const std::size_t storageBytes = stagedCleaveHistoryStorageBytes();
                    if ( storageBytes > kStagedCleaveHistoryMaxBytes )
                    {
                        return false;
                    }
                    std::size_t remaining =
                        kStagedCleaveHistoryMaxBytes - storageBytes;
                    if ( includeGestureAdmission )
                    {
                        const std::size_t stateCapacity =
                            stagedCleaveHistory_.states.capacity();
                        const std::size_t descriptorReserveBytes =
                            stateCapacity < kStagedCleaveHistoryMaxSnapshots
                            ? (kStagedCleaveHistoryMaxSnapshots - stateCapacity)
                                * sizeof(StagedCleaveDraftState)
                            : 0u;
                        const std::size_t beforeBytes =
                            stagedCleaveDraftStateCurrentOwnedBytesEstimate();
                        if ( descriptorReserveBytes > remaining )
                        {
                            return false;
                        }
                        remaining -= descriptorReserveBytes;
                        if ( beforeBytes > remaining )
                        {
                            return false;
                        }
                        remaining -= beforeBytes;
                    }
                    return workBytes <= remaining;
                };
                const QPointF pointer(lastMousePos_);
                const auto preparePlaneDrag = [this,
                                               &proj,
                                               &view,
                                               &eye,
                                               &target,
                                               isOrtho,
                                               &planeDragWorksetFitsBudget](
                                                   const std::int32_t lineIndex,
                                                   const std::int32_t endpoint,
                                                   const bool lineBody)
                {
                    if (lineIndex < 0
                        || static_cast<std::size_t>(lineIndex) >= stagedCleaveLines_.size())
                    {
                        return false;
                    }
                    const std::size_t lineNumber = static_cast<std::size_t>(lineIndex);
                    const StagedCleaveLine& line = stagedCleaveLines_[lineNumber];
                    const bool movingSelectedGroup = stagedCleaveLineSelected(lineIndex)
                        && selectedStagedCleaveLines_.size() > 1;
                    if (endpoint >= 0 && !movingSelectedGroup)
                    {
                        return false;
                    }
                    if ( !planeDragWorksetFitsBudget(
                            lineIndex,
                            endpoint,
                            false) )
                    {
                        showToast(
                            "Multi-cut: local history limit reached; this line group is too large to move",
                            3200,
                            ToastPlacement::BottomCenter,
                            ToastGroup::Error);
                        return false;
                    }

                    std::vector<std::int32_t> movingLines = movingSelectedGroup
                        ? selectedStagedCleaveLines_
                        : std::vector<std::int32_t>{lineIndex};
                    using HandleKey = std::tuple<double, double, double>;
                    const auto keyFor = [](const TVector& point)
                    {
                        return HandleKey{point.x, point.y, point.z};
                    };
                    std::map<HandleKey, bool> movingHandles{};
                    for (const std::int32_t selectedLine : movingLines)
                    {
                        if (selectedLine < 0
                            || static_cast<std::size_t>(selectedLine) >= stagedCleaveLines_.size())
                        {
                            return false;
                        }
                        const StagedCleaveLine& selected = stagedCleaveLines_[
                            static_cast<std::size_t>(selectedLine)];
                        movingHandles.emplace(keyFor(selected.first), true);
                        movingHandles.emplace(keyFor(selected.second), true);
                    }
                    std::vector<StagedCleaveLineMove> affectedLines{};
                    affectedLines.reserve(stagedCleaveLines_.size());
                    for (std::size_t current = 0; current < stagedCleaveLines_.size(); ++current)
                    {
                        const StagedCleaveLine& currentLine = stagedCleaveLines_[current];
                        const bool selected = std::binary_search(
                            movingLines.begin(), movingLines.end(),
                            static_cast<std::int32_t>(current));
                        std::uint8_t endpointMask = selected ? 3u : 0u;
                        if (!selected)
                        {
                            if (movingHandles.contains(keyFor(currentLine.first)))
                            {
                                endpointMask |= 1u;
                            }
                            if (movingHandles.contains(keyFor(currentLine.second)))
                            {
                                endpointMask |= 2u;
                            }
                        }
                        if (endpointMask != 0u)
                        {
                            affectedLines.push_back(StagedCleaveLineMove{
                                static_cast<std::int32_t>(current), endpointMask});
                        }
                    }

                    TVector dragNormal = sectorCleaveToolEnabled_
                        ? render::cleaveDocumentPoint(target)
                            - render::cleaveDocumentPoint(eye)
                        : line.constructionPlaneNormal;
                    if (!normalizeInPlace(dragNormal))
                    {
                        return false;
                    }
                    const TVector anchor = endpoint == 0
                        ? line.first
                        : endpoint == 1
                            ? line.second
                            : (line.first + line.second) * 0.5;
                    const auto initialHit = intersectCleaveWidgetRayWithPlane(
                        lastMousePos_,
                        proj,
                        view,
                        eye,
                        isOrtho,
                        width(),
                        height(),
                        anchor,
                        dragNormal);
                    if (!initialHit)
                    {
                        return false;
                    }
                    stagedCleavePlaneDrag_ = {};
                    stagedCleavePlaneDrag_.active = true;
                    stagedCleavePlaneDrag_.translationReady = true;
                    stagedCleavePlaneDrag_.restoreAuthoredCameraOnClick = lineBody;
                    stagedCleavePlaneDrag_.line = lineIndex;
                    stagedCleavePlaneDrag_.planePoint = anchor;
                    stagedCleavePlaneDrag_.planeNormal = dragNormal;
                    stagedCleavePlaneDrag_.initialHit = *initialHit;
                    stagedCleavePlaneDrag_.movingLines = std::move(movingLines);
                    stagedCleavePlaneDrag_.affectedLines = std::move(affectedLines);
                    stagedCleavePlaneDrag_.grabbedEndpoint = endpoint;
                    if (endpoint >= 0)
                    {
                        stagedCleavePlaneDrag_.preserveRigidLinePlacement = true;
                    }
                    setCursor(Qt::SizeAllCursor);
                    return true;
                };
                const auto prepareTransformDrag = [this, &proj, &view, &eye, isOrtho](
                                                     const std::int32_t lineIndex)
                {
                    if ( !stagedCleaveLineSelected(lineIndex)
                        || !prepareStagedCleaveGroupTransform() )
                    {
                        return false;
                    }
                    const auto initialHit = intersectCleaveWidgetRayWithPlane(
                        lastMousePos_, proj, view, eye, isOrtho, width(), height(),
                        stagedCleaveTransformDrag_.pivot,
                        stagedCleaveTransformDrag_.planeNormal);
                    if ( !initialHit )
                    {
                        stagedCleaveTransformDrag_ = {};
                        return false;
                    }
                    TVector initialRadial = *initialHit - stagedCleaveTransformDrag_.pivot;
                    initialRadial = initialRadial - stagedCleaveTransformDrag_.planeNormal
                        * render::cleaveDot(initialRadial, stagedCleaveTransformDrag_.planeNormal);
                    const double initialRadius = cleaveVectorLength(initialRadial);
                    if ( !std::isfinite(initialRadius) || initialRadius <= 1.0e-8 )
                    {
                        stagedCleaveTransformDrag_ = {};
                        return false;
                    }
                    stagedCleaveTransformDrag_.startPosition = lastMousePos_;
                    stagedCleaveTransformDrag_.currentPosition = lastMousePos_;
                    stagedCleaveTransformDrag_.initialHit = *initialHit;
                    stagedCleaveTransformDrag_.initialRadius = initialRadius;
                    setCursor(Qt::SizeAllCursor);
                    return true;
                };
                std::int32_t bestLine = -1;
                std::int32_t bestEndpoint = -1;
                bool hitAuthoredLineBody = false;
                double bestDistanceSquared = kEndpointPickRadiusPx * kEndpointPickRadiusPx;

                // Endpoints take precedence over the line body so shared or
                // short chained segments remain straightforward to edit.
                for (std::size_t index = 0; index < stagedCleaveLines_.size(); ++index)
                {
                    const StagedCleaveLine& line = stagedCleaveLines_[index];
                    const ProjectedPoint first = projectWorldToWidget(mvp, line.first, width(), height());
                    const ProjectedPoint second = projectWorldToWidget(mvp, line.second, width(), height());
                    if (!first.valid || !second.valid)
                    {
                        continue;
                    }

                    const QPointF firstDelta = pointer - first.pos;
                    const QPointF secondDelta = pointer - second.pos;
                    const double firstDistanceSquared = firstDelta.x() * firstDelta.x()
                        + firstDelta.y() * firstDelta.y();
                    const double secondDistanceSquared = secondDelta.x() * secondDelta.x()
                        + secondDelta.y() * secondDelta.y();
                    if (firstDistanceSquared <= bestDistanceSquared)
                    {
                        bestLine = static_cast<std::int32_t>(index);
                        bestEndpoint = 0;
                        bestDistanceSquared = firstDistanceSquared;
                    }
                    if (secondDistanceSquared < bestDistanceSquared)
                    {
                        bestLine = static_cast<std::int32_t>(index);
                        bestEndpoint = 1;
                        bestDistanceSquared = secondDistanceSquared;
                    }
                }

                if (bestEndpoint < 0)
                {
                    bestDistanceSquared = kLinePickRadiusPx * kLinePickRadiusPx;
                    for (std::size_t index = 0; index < stagedCleaveLines_.size(); ++index)
                    {
                        const StagedCleaveLine& line = stagedCleaveLines_[index];
                        const ProjectedPoint first = projectWorldToWidget(mvp, line.first, width(), height());
                        const ProjectedPoint second = projectWorldToWidget(mvp, line.second, width(), height());
                        if (!first.valid || !second.valid)
                        {
                            continue;
                        }
                        const double lineDistanceSquared = cleavePointSegmentDistanceSquared(
                            pointer,
                            first.pos,
                            second.pos);
                        if (lineDistanceSquared < bestDistanceSquared)
                        {
                            bestLine = static_cast<std::int32_t>(index);
                            hitAuthoredLineBody = true;
                            bestDistanceSquared = lineDistanceSquared;
                        }
                    }
                }

                // Sector Multi-cut planes remain selectable through their
                // actual intersection with the target Sector, not only through
                // the short construction line that originally authored them.
                if (bestLine < 0 && sectorCleaveToolEnabled_)
                {
                    for (std::size_t index = 0; index < stagedCleaveLines_.size(); ++index)
                    {
                        for (const std::vector<TVector>& polygon3d
                             : stagedCleavePlanePolygons(stagedCleaveLines_[index]))
                        {
                            QPolygonF polygon2d{};
                            bool validPolygon = true;
                            for (const TVector& point : polygon3d)
                            {
                                const ProjectedPoint projected = projectWorldToWidget(
                                    mvp,
                                    point,
                                    width(),
                                    height());
                                if (!projected.valid || projected.clipW <= 1.0e-6f)
                                {
                                    validPolygon = false;
                                    break;
                                }
                                if (projected.ndcZ < -1.0f || projected.ndcZ > 1.0f)
                                {
                                    validPolygon = false;
                                    break;
                                }
                                polygon2d.push_back(projected.pos);
                            }
                            if (validPolygon
                                && polygon2d.containsPoint(pointer, Qt::OddEvenFill))
                            {
                                bestLine = static_cast<std::int32_t>(index);
                                break;
                            }
                        }
                        if (bestLine >= 0)
                        {
                            break;
                        }
                    }
                }

                if (bestLine >= 0)
                {
                    const bool transformRouteExpected =
                        mods == Qt::NoModifier
                        && stagedCleaveTransformMode_
                            != StagedCleaveTransformMode::Move
                        && stagedCleaveLineSelected(bestLine)
                        && canTransformSelectedStagedCleave(
                            stagedCleaveTransformMode_);
                    const bool movingSelectedGroupEndpoint = bestEndpoint >= 0
                        && stagedCleaveLineSelected(bestLine)
                        && selectedStagedCleaveLines_.size() > 1;
                    const bool planeMoveWorksetExpected = !transformRouteExpected
                        && (hitAuthoredLineBody || movingSelectedGroupEndpoint);
                    if ( planeMoveWorksetExpected
                        && !planeDragWorksetFitsBudget(
                            bestLine,
                            bestEndpoint,
                            true) )
                    {
                        showToast(
                            "Multi-cut: local history limit reached; this line group is too large to move",
                            3200,
                            ToastPlacement::BottomCenter,
                            ToastGroup::Error);
                        event->accept();
                        return;
                    }
                    if ( !beginStagedCleaveHistoryGesture() )
                    {
                        showToast(
                            "Multi-cut: local history limit reached; Clear or Apply before editing this draft",
                            3200,
                            ToastPlacement::BottomCenter,
                            ToastGroup::Error);
                        event->accept();
                        return;
                    }
                    stagedCleaveLineClickCandidate_ = true;
                    stagedCleaveDragSelectionApplied_ = false;
                    if (bestEndpoint >= 0 && mods == Qt::AltModifier)
                    {
                        stagedCleaveJointCycleActiveBefore_ = selectedStagedCleaveLine_;
                        const StagedCleaveLine& hitLine = stagedCleaveLines_[
                            static_cast<std::size_t>(bestLine)];
                        const TVector joint = bestEndpoint == 0
                            ? hitLine.first
                            : hitLine.second;
                        for (std::size_t candidate = 0;
                             candidate < stagedCleaveLines_.size();
                             ++candidate)
                        {
                            const StagedCleaveLine& line = stagedCleaveLines_[candidate];
                            if (render::cleaveSameHandle(line.first, joint)
                                || render::cleaveSameHandle(line.second, joint))
                            {
                                stagedCleaveJointCycleCandidates_.push_back(
                                    static_cast<std::int32_t>(candidate));
                            }
                        }
                        stagedCleaveJointCycleClickCandidate_ =
                            stagedCleaveJointCycleCandidates_.size() > 1;
                    }
                    selectedStagedCleaveLine_ = bestLine;
                    if ( mods == Qt::NoModifier
                        && stagedCleaveTransformMode_
                            != StagedCleaveTransformMode::Move
                        && prepareTransformDrag(bestLine) )
                    {
                        stagedCleaveLineClickCandidate_ = false;
                        stagedCleaveEndpointOriginClickCandidate_ = false;
                        editingStagedCleaveLine_ = -1;
                        editingStagedCleaveEndpoint_ = -1;
                        editingDisjointStagedCleaveEndpoint_ = false;
                        editingStagedCleaveEndpointPreservePlacement_ = false;
                        update();
                        event->accept();
                        return;
                    }
                    if (bestEndpoint >= 0
                        && stagedCleaveLineSelected(bestLine)
                        && selectedStagedCleaveLines_.size() > 1
                        && preparePlaneDrag(bestLine, bestEndpoint, false))
                    {
                        editingStagedCleaveLine_ = -1;
                        editingStagedCleaveEndpoint_ = -1;
                        stagedCleaveEndpointOriginClickCandidate_ = false;
                        editingDisjointStagedCleaveEndpoint_ = false;
                        editingStagedCleaveEndpointPreservePlacement_ = false;
                        update();
                        event->accept();
                        return;
                    }
                    if (bestEndpoint >= 0)
                    {
                        editingStagedCleaveLine_ = bestLine;
                        editingStagedCleaveEndpoint_ = bestEndpoint;
                        stagedCleaveEndpointOriginClickCandidate_ =
                            cleaveStagedMode_
                            && event->modifiers() == Qt::NoModifier;
                        editingDisjointStagedCleaveEndpoint_ =
                            event->modifiers().testFlag(Qt::AltModifier);
                        editingStagedCleaveEndpointPreservePlacement_ = false;
                        const StagedCleaveLine& line = stagedCleaveLines_[static_cast<std::size_t>(bestLine)];
                        const TVector fixed = bestEndpoint == 0 ? line.second : line.first;
                        const TVector moving = bestEndpoint == 0 ? line.first : line.second;
                        if (sectorCleaveToolEnabled_)
                        {
                            sectorCleaveHaveFirstPoint_ = true;
                            sectorCleaveFirstPoint_ = fixed;
                            sectorCleaveCurrentPoint_ = moving;
                        }
                        else
                        {
                            surfaceCleaveHaveFirstPoint_ = true;
                            surfaceCleaveFirstPoint_ = fixed;
                            surfaceCleaveCurrentPoint_ = moving;
                        }
                        setCursor(Qt::ClosedHandCursor);
                    }
                    else
                    {
                        editingStagedCleaveLine_ = -1;
                        editingStagedCleaveEndpoint_ = -1;
                        stagedCleaveEndpointOriginClickCandidate_ = false;
                        editingDisjointStagedCleaveEndpoint_ = false;
                        editingStagedCleaveEndpointPreservePlacement_ = false;
                        const StagedCleaveLine& line = stagedCleaveLines_[static_cast<std::size_t>(bestLine)];
                        if (hitAuthoredLineBody)
                        {
                            // Camera return is a line-click action, not a drag-plane
                            // intersection side effect. Arm it even when the current
                            // view ray is parallel to the line's construction plane.
                            stagedCleavePlaneDrag_.active = true;
                            stagedCleavePlaneDrag_.restoreAuthoredCameraOnClick = true;
                            stagedCleavePlaneDrag_.line = bestLine;
                        }
                        if (preparePlaneDrag(bestLine, -1, hitAuthoredLineBody))
                        {
                            stagedCleavePlaneDrag_.restoreAuthoredCameraOnClick =
                                hitAuthoredLineBody;
                        }
                    }
                    update();
                    event->accept();
                    return;
                }

                selectedStagedCleaveLine_ = -1;
                editingStagedCleaveLine_ = -1;
                editingStagedCleaveEndpoint_ = -1;
                stagedCleaveEndpointOriginClickCandidate_ = false;
                editingDisjointStagedCleaveEndpoint_ = false;
                editingStagedCleaveEndpointPreservePlacement_ = false;

                if ( cleaveStagedMode_ && mods.testFlag(Qt::AltModifier) )
                {
                    stagedCleaveMarqueeSelection_ = {};
                    stagedCleaveMarqueeSelection_.active = true;
                    stagedCleaveMarqueeSelection_.startPosition = lastMousePos_;
                    stagedCleaveMarqueeSelection_.currentPosition = lastMousePos_;
                    stagedCleaveMarqueeSelection_.mode =
                        mods.testFlag(Qt::ControlModifier)
                        ? SelectionBoxMode::Remove
                        : SelectionBoxMode::Add;
                    stagedCleaveMarqueeSelection_.baselineSelection =
                        selectedStagedCleaveLines_;
                    event->accept();
                    return;
                }
            }

            if (sectorCleaveToolEnabled_)
            {
                // Sector cleave: interpret the drag line in screen space (camera view plane) so it works
                // from arbitrary view angles, not only axis-aligned top/side views.
                TVector workNormal = (render::cleaveDocumentPoint(target) - render::cleaveDocumentPoint(eye));
                if (!normalizeInPlace(workNormal))
                {
                    event->accept();
                    return;
                }

                // View matrix is world->camera; its rows are the camera basis in world space.
                TVector axisA(view(0, 0), view(0, 1), view(0, 2)); // camera right
                TVector axisB(view(1, 0), view(1, 1), view(1, 2)); // camera up
                if (!normalizeInPlace(axisA))
                {
                    axisA = TVector(1.0f, 0.0f, 0.0f);
                }
                if (!normalizeInPlace(axisB))
                {
                    axisB = TVector(0.0f, 1.0f, 0.0f);
                }

                std::optional<TVector> hitOpt{};
                if (level_)
                {
                    // Prefer anchoring the cleave line to the visible level geometry under the cursor.
                    // This makes the tool behave correctly in perspective views (not tied to the orbit target depth).
                    const auto ray = render::viewportRayForWidget(
                        proj, view, eye, lastMousePos_, width(), height(), isOrtho);
                    if (ray)
                    {
                        const TVector ro = ray->origin;
                        const TVector rd = ray->direction;
                        const double rayMaxDistance = render::viewportRayMaxDistanceForWidget(
                            proj, view, QPointF(lastMousePos_), width(), height(), isOrtho)
                            .value_or(std::max(0.0, static_cast<double>(farPlane)));
                        const std::vector<sed::level::SurfacePickHit> hits =
                            surfacePickIndex_.pickSurfaces(*level_, ro, rd, rayMaxDistance);

                        auto sectorIsSelected = [&](const std::int32_t sectorIndex) -> bool
                        {
                            if (!highlightedSelection_.sectors.empty())
                            {
                                return highlightedSelection_.sectors.find(sectorIndex) != highlightedSelection_.sectors.end();
                            }
                            if (selectedSector_ >= 0)
                            {
                                return sectorIndex == selectedSector_;
                            }
                            return true;
                        };

                        for (const sed::level::SurfacePickHit& h : hits)
                        {
                            if (!sectorIsSelected(h.surface.sector))
                            {
                                continue;
                            }

                            hitOpt = TVector(
                                h.position.x,
                                h.position.y,
                                h.position.z);
                            break;
                        }
                    }
                }

                if (!hitOpt)
                {
                    hitOpt = intersectCleaveWidgetRayWithPlane(
                        lastMousePos_,
                        proj, view,
                        eye,
                        isOrtho,
                        width(),
                        height(),
                        render::cleaveDocumentPoint(target),
                        workNormal);
                }

                if (!hitOpt)
                {
                    showToast("Cleave: cannot pick point", 2500, ToastPlacement::BottomCenter, ToastGroup::Error);
                    event->accept();
                    return;
                }

                TVector hit = *hitOpt;

                const bool initialGridSnap = transformGridSnapEnabled_
                    != mods.testFlag(Qt::ControlModifier);
                if (initialGridSnap
                    && !mods.testFlag(Qt::ShiftModifier))
                {
                    const render::TransformGridFrame gridFrame =
                        activeTransformGridFrame(
                            view, mvp, invMvp, eye, target, isOrtho);
                    hit = render::snapPointToGridOnPlane(
                        hit, hit, workNormal, gridFrame);
                }

                if (mods.testFlag(Qt::ShiftModifier))
                {
                    // Initial-point acquisition is intentionally generous: a
                    // construction point should be easy to acquire before the
                    // user has any line feedback to guide the gesture.
                    constexpr double kStartSnapTolerancePx = 18.0;
                    cleaveSnapMarker_ = closestCleaveSnapTarget(
                        lastMousePos_,
                        mvp,
                        false,
                        kStartSnapTolerancePx);
                    if (cleaveSnapMarker_)
                    {
                        hit = cleaveSnapMarker_->position;
                    }
                }

                if ( cleaveStagedMode_ && stagedCleaveLinesFormSingleClosedChain() )
                {
                    cleaveSnapMarker_.reset();
                    showToast(
                        "Multi-cut: the chain is closed; edit it, Apply, or Clear",
                        2600,
                        ToastPlacement::BottomCenter,
                        ToastGroup::General);
                    event->accept();
                    return;
                }
                if ( cleaveStagedMode_ && cleaveChainEnabled_ && !stagedCleaveLines_.empty() )
                {
                    hit = stagedCleaveChainOriginPoint(stagedCleaveLines_);
                    cleaveSnapMarker_.reset();
                }
                if ( cleaveStagedMode_ )
                {
                    if ( !beginStagedCleaveHistoryGesture() )
                    {
                        showToast(
                            "Multi-cut: local history limit reached; Clear or Apply before editing this draft",
                            3200,
                            ToastPlacement::BottomCenter,
                            ToastGroup::Error);
                        event->accept();
                        return;
                    }
                    if ( stagedCleaveLines_.empty() )
                    {
                        captureStagedCleaveContext(false);
                    }
                }
                sectorCleaveHaveFirstPoint_ = true;
                sectorCleaveFirstPoint_ = hit;
                sectorCleaveCurrentPoint_ = hit;
                update();
                showToast(
                    cleaveStagedMode_
                        ? "Multi-cut: drag line; Ctrl = grid, Shift = connect either end, Alt = disconnect both ends"
                        : "Cleave: drag line, release to apply (Ctrl = grid snap, Shift = vertex/edge/center snap)",
                    2200,
                    ToastPlacement::BottomCenter,
                    ToastGroup::General);

                event->accept();
                return;
            }

            if (surfaceCleaveToolEnabled_)
            {
                if (!level_ || selectedSurfaceSector_ < 0 || selectedSurface_ < 0)
                {
                    showToast("Cleave surface: select a surface first", 2500, ToastPlacement::BottomCenter, ToastGroup::Error);
                    event->accept();
                    return;
                }

                if (static_cast<std::size_t>(selectedSurfaceSector_) >= level_->sectors.size())
                {
                    event->accept();
                    return;
                }

                const Sector& sector = level_->sectors[static_cast<std::size_t>(selectedSurfaceSector_)];
                if (static_cast<std::size_t>(selectedSurface_) >= sector.surfaces.size())
                {
                    event->accept();
                    return;
                }

                const Surface& surface = sector.surfaces[static_cast<std::size_t>(selectedSurface_)];
                if (surface.vertices.size() < 3)
                {
                    showToast("Cleave surface: invalid surface", 2500, ToastPlacement::BottomCenter, ToastGroup::Error);
                    event->accept();
                    return;
                }

                const std::int32_t v0i = surface.vertices[0];
                if (v0i < 0 || static_cast<std::size_t>(v0i) >= sector.vertices.size())
                {
                    event->accept();
                    return;
                }
                const Vertex& v0 = sector.vertices[static_cast<std::size_t>(v0i)];
                const TVector planePoint(v0.x, v0.y, v0.z);

                TVector surfNormal(surface.normal.x, surface.normal.y, surface.normal.z);
                if (surfNormal.lengthSqr() < sed::math::kTinyEpsilonF)
                {
                    // Fallback: compute normal from the first three vertices.
                    const std::int32_t v1i = surface.vertices[1];
                    const std::int32_t v2i = surface.vertices[2];
                    if (v1i >= 0 && v2i >= 0
                        && static_cast<std::size_t>(v1i) < sector.vertices.size()
                        && static_cast<std::size_t>(v2i) < sector.vertices.size())
                    {
                        const Vertex& v1 = sector.vertices[static_cast<std::size_t>(v1i)];
                        const Vertex& v2 = sector.vertices[static_cast<std::size_t>(v2i)];
                        const TVector p1(v1.x, v1.y, v1.z);
                        const TVector p2(v2.x, v2.y, v2.z);
                        surfNormal = render::cleaveCross(p1 - planePoint, p2 - planePoint);
                    }
                }

                if (!normalizeInPlace(surfNormal))
                {
                    showToast("Cleave surface: invalid normal", 2500, ToastPlacement::BottomCenter, ToastGroup::Error);
                    event->accept();
                    return;
                }

                const auto hitOpt = intersectCleaveWidgetRayWithPlane(
                    lastMousePos_,
                    proj, view,
                    eye,
                    isOrtho,
                    width(),
                    height(),
                    planePoint,
                    surfNormal);
                if (!hitOpt)
                {
                    showToast("Cleave surface: cannot pick point", 2500, ToastPlacement::BottomCenter, ToastGroup::Error);
                    event->accept();
                    return;
                }

                TVector hit = *hitOpt;

                // Shift geometry/staged-point snapping is authoritative when
                // both modifiers are held. It must never fall through to a
                // grid result merely because no geometry candidate is close.
                const bool vertexSnap = mods.testFlag(Qt::ShiftModifier);
                const bool gridSnap = (transformGridSnapEnabled_
                    != mods.testFlag(Qt::ControlModifier)) && !vertexSnap;

                if (gridSnap || vertexSnap)
                {
                    if (gridSnap)
                    {
                        const render::TransformGridFrame gridFrame =
                            activeTransformGridFrame(
                                view, mvp, invMvp, eye, target, isOrtho);
                        hit = render::snapPointToGridOnPlane(
                            hit, planePoint, surfNormal, gridFrame);
                    }

                    if (vertexSnap)
                    {
                        constexpr double kStartSnapTolerancePx = 18.0;
                        cleaveSnapMarker_ = closestCleaveSnapTarget(
                            lastMousePos_,
                            mvp,
                            true,
                            kStartSnapTolerancePx);
                        if (cleaveSnapMarker_)
                        {
                            hit = cleaveSnapMarker_->position;
                        }
                    }
                }

                if ( cleaveStagedMode_ && stagedCleaveLinesFormSingleClosedChain() )
                {
                    cleaveSnapMarker_.reset();
                    showToast(
                        "Multi-cut: the chain is closed; edit it, Apply, or Clear",
                        2600,
                        ToastPlacement::BottomCenter,
                        ToastGroup::General);
                    event->accept();
                    return;
                }
                if ( cleaveStagedMode_ && cleaveChainEnabled_ && !stagedCleaveLines_.empty() )
                {
                    hit = stagedCleaveChainOriginPoint(stagedCleaveLines_);
                    cleaveSnapMarker_.reset();
                }
                if ( cleaveStagedMode_ && surfaceSubMode_ != SurfaceSubMode::Edge )
                {
                    if ( !beginStagedCleaveHistoryGesture() )
                    {
                        showToast(
                            "Multi-cut: local history limit reached; Clear or Apply before editing this draft",
                            3200,
                            ToastPlacement::BottomCenter,
                            ToastGroup::Error);
                        event->accept();
                        return;
                    }
                }
                if ( cleaveStagedMode_ && stagedCleaveLines_.empty()
                    && surfaceSubMode_ != SurfaceSubMode::Edge )
                {
                    captureStagedCleaveContext(true);
                }
                surfaceCleaveHaveFirstPoint_ = true;
                surfaceCleaveFirstPoint_ = hit;
                surfaceCleaveCurrentPoint_ = hit;
                update();
                showToast(
                    cleaveStagedMode_
                        ? "Multi-cut surface: drag line; Ctrl = grid, Shift = connect either end, Alt = disconnect both ends"
                        : "Cleave surface: drag line, release to apply (Ctrl = grid snap, Shift = vertex/edge/center snap)",
                    2200,
                    ToastPlacement::BottomCenter,
                    ToastGroup::General);

                event->accept();
                return;
            }

            event->accept();
            return;
        }

        const Qt::KeyboardModifiers mods = event->modifiers();
        const bool transformToolAreaSelect =
            transformTool_ != TransformTool::Select
            && mods.testFlag(Qt::AltModifier);
        const bool wantBoxSelect =
            surfaceClickSelectionEnabled_
            && renderPipeline_ != RenderPipeline::GamePreview
            && (transformTool_ == TransformTool::Select || transformToolAreaSelect);
        if (wantBoxSelect)
        {
            // Select always owns marquee input. Move/Rotate/Scale temporarily
            // use the same path while Alt is held, preserving their active
            // state and avoiding an accidental transform-handle drag.
            leftPressPos_ = lastMousePos_;
            leftDragged_ = false;
            boxSelectActive_ = true;
            boxSelectVisible_ = false;
            boxSelectFromTransformTool_ = transformToolAreaSelect;
            boxSelectStart_ = lastMousePos_;
            boxSelectEnd_ = lastMousePos_;
            event->accept();
            return;
        }

        if (transformTool_ != TransformTool::Select && transformInteractionEnabled_)
        {
            QMatrix4x4 proj;
            QMatrix4x4 view;
            QVector3D eye{};
            QVector3D target{};
            float nearPlane{};
            float farPlane{};
            buildCameraMatrices(proj, view, eye, target, nearPlane, farPlane);

            if (beginTransformDrag(lastMousePos_, proj, view, eye, target, farPlane))
            {
                event->accept();
                return;
            }
        }

        leftPressPos_ = lastMousePos_;
        leftDragged_ = false;
        boxSelectActive_ = false;
        boxSelectVisible_ = false;
        boxSelectFromTransformTool_ = false;
    }
    if (event->button() == Qt::RightButton)
    {
        if (!placementPreviewPersistent_)
        {
            clearPlacementPreviewThing();
            clearPlacementPreviewLines();
        }
        startFlyNavigation(event->modifiers());
    }
    event->accept();
}

void GlView::mouseMoveEvent(QMouseEvent* event)
{
    event->accept();
    Qt::MouseButtons semanticButtons = Qt::NoButton;
    Qt::KeyboardModifiers semanticModifiers = event->modifiers();
    if (viewportMouseGesture_.button != Qt::NoButton)
    {
        if (!event->buttons().testFlag(viewportMouseGesture_.button))
        {
            // Recover from a release lost outside the widget, without
            // converting the next hover sample into an edit commit.
            cancelViewportMouseGesture();
            lastMousePos_ = mouseEventPos(*event);
            return;
        }
        if (viewportMouseActionIsCameraDrag(viewportMouseGesture_.action)
            && !viewportMouseActionIsCameraDrag(viewportMouseActionForPress(
                effectiveViewportNavigationSettings(), viewportMouseGesture_.button,
                event->modifiers())))
        {
            lastMousePos_ = mouseEventPos(*event);
            return;
        }
        semanticButtons = viewportMouseSemanticButton();
        semanticModifiers = viewportMouseSemanticModifiers(event->modifiers());
    }
    else if (event->buttons() != Qt::NoButton)
    {
        // Moves from another widget, a disabled chord, or an expired grab do
        // not acquire a gesture implicitly.
        lastMousePos_ = mouseEventPos(*event);
        return;
    }
    if (selectionOrbitDragRejected_ && semanticButtons.testFlag(Qt::MiddleButton))
    {
        event->accept();
        return;
    }
    const QPoint currentPos = mouseEventPos(*event);
    const QPoint delta = currentPos - lastMousePos_;
    lastMousePos_ = currentPos;

    if (delta.isNull())
    {
        return;
    }

    const Qt::MouseButtons buttons = semanticButtons;
    const Qt::KeyboardModifiers mods = semanticModifiers;
    if ( stagedCleaveMarqueeSelection_.active
        && buttons.testFlag(Qt::LeftButton) )
    {
        stagedCleaveMarqueeSelection_.currentPosition = currentPos;
        leftDragged_ = render::cleavePointerMotionIsDrag(
            currentPos - stagedCleaveMarqueeSelection_.startPosition);
        update();
        event->accept();
        return;
    }
    if ( stagedCleaveEndpointOriginClickCandidate_
        && (mods != Qt::NoModifier
            || render::cleavePointerMotionIsDrag(currentPos - leftPressPos_)))
    {
        stagedCleaveEndpointOriginClickCandidate_ = false;
    }

    // RMB fly mode owns the whole pointer sample. Do not let its Shift/Ctrl/Alt
    // state leak into cleave, placement, snapping, or transform previews.
    if (flyNav_.active)
    {
        if (buttons.testFlag(Qt::RightButton))
        {
            flyLook(viewportCameraMouseDelta(delta));
        }
        event->accept();
        return;
    }

    if ( cleaveStagedMode_ && cleaveShapeOptions_.placementArmed
        && (buttons == Qt::NoButton
            || (buttons.testFlag(Qt::LeftButton)
                && stagedCleaveShapePlacementClickPending_)) )
    {
        stagedCleaveShapePreviewFrame_ =
            stagedCleaveShapePlacementFrameAt(currentPos, mods);
        stagedCleaveShapePreviewCenter_ = stagedCleaveShapePreviewFrame_
            ? std::optional<QVector3D>(render::cleaveRenderPoint(
                stagedCleaveShapePreviewFrame_->center))
            : std::nullopt;
        if ( buttons.testFlag(Qt::LeftButton) )
        {
            leftDragged_ = render::cleavePointerMotionIsDrag(
                currentPos - leftPressPos_);
        }
        update();
        event->accept();
        return;
    }

    if (sectorCleaveToolEnabled_ || surfaceCleaveToolEnabled_)
    {
        updateCleaveSnapGuideVisibility(mods);
    }

    if (viewportPickingEnabled_
        && !flyNav_.active
        && !buttons.testFlag(Qt::LeftButton)
        && !buttons.testFlag(Qt::RightButton))
    {
        Q_EMIT placementPreviewMouseMoved(currentPos, mods);
    }

    if (!buttons.testFlag(Qt::LeftButton)
        && !buttons.testFlag(Qt::MiddleButton)
        && !buttons.testFlag(Qt::RightButton)
        && (sectorCleaveToolEnabled_ || surfaceCleaveToolEnabled_))
    {
        refreshCleaveSnapPreview(currentPos, mods);
    }

    if (axisWidgetDragging_ && buttons.testFlag(Qt::LeftButton))
    {
        orbitRotate(
            -static_cast<float>(delta.x()) * 0.25f,
            static_cast<float>(delta.y()) * 0.25f);
        event->accept();
        return;
    }

    if (sectorThrustDrag_.active && buttons.testFlag(Qt::LeftButton))
    {
        updateSectorThrustDrag(currentPos);
        leftDragged_ = sectorThrustDrag_.moved;
        event->accept();
        return;
    }

    if (sectorThrustToolEnabled_
        && !buttons.testFlag(Qt::LeftButton)
        && !buttons.testFlag(Qt::RightButton))
    {
        QMatrix4x4 proj;
        QMatrix4x4 view;
        QVector3D eye{};
        QVector3D target{};
        float nearPlane{};
        float farPlane{};
        buildCameraMatrices(proj, view, eye, target, nearPlane, farPlane);
        (void)target;
        (void)nearPlane;
        (void)farPlane;
        const QMatrix4x4 mvp = proj * view;
        bool invOk = false;
        const QMatrix4x4 invMvp = mvp.inverted(&invOk);
        const std::int32_t nextHover = invOk
            ? hitTestSectorThrustArrow(currentPos, mvp, invMvp, eye).value_or(-1)
            : -1;
        if (nextHover != hoveredSectorThrust_)
        {
            hoveredSectorThrust_ = nextHover;
            setCursor(nextHover >= 0 ? Qt::OpenHandCursor : Qt::ArrowCursor);
            update();
        }
    }

    if (placementGestureEnabled_ && viewportPickingEnabled_ && buttons.testFlag(Qt::LeftButton))
    {
        leftDragged_ = true;
        Q_EMIT placementGestureDragged(currentPos, mods);
        event->accept();
        return;
    }

    if (transformDrag_.active && buttons.testFlag(Qt::LeftButton))
    {
        QMatrix4x4 proj;
        QMatrix4x4 view;
        QVector3D eye{};
        QVector3D target{};
        float nearPlane{};
        float farPlane{};
        buildCameraMatrices(proj, view, eye, target, nearPlane, farPlane);

        updateTransformDrag(currentPos, proj, view, eye, target, farPlane, mods);
        leftDragged_ = true;
        event->accept();
        return;
    }

    if (previewInteractionEnabled_ && previewInteractionDragging_ && buttons.testFlag(Qt::LeftButton))
    {
        orbitRotate(-static_cast<float>(delta.x()) * 0.35f, static_cast<float>(delta.y()) * 0.35f);
        leftDragged_ = true;
        event->accept();
        return;
    }

    if ( stagedCleaveTransformDrag_.active && !stagedCleaveWheelSession_.active()
        && buttons.testFlag(Qt::LeftButton) )
    {
        QMatrix4x4 projection{};
        QMatrix4x4 cameraView{};
        QVector3D eye{};
        QVector3D target{};
        float nearPlane{};
        float farPlane{};
        buildCameraMatrices(projection, cameraView, eye, target, nearPlane, farPlane);
        const bool orthographic = camera_.projectionType()
            == app::ViewportCamera::ProjectionType::Orthographic;
        const std::vector<StagedCleaveLine>& baselineLines =
            stagedCleaveGestureBaselineLines();
        const auto hit = intersectCleaveWidgetRayWithPlane(
            currentPos,
            projection,
            cameraView,
            eye,
            orthographic,
            width(),
            height(),
            stagedCleaveTransformDrag_.pivot,
            stagedCleaveTransformDrag_.planeNormal);
        if ( hit && baselineLines.size() == stagedCleaveLines_.size()
            && stagedCleaveTransformDrag_.previewLines.size()
                == stagedCleaveTransformDrag_.affectedLines.size() )
        {
            double appliedAngle = 0.0;
            double appliedScale = 1.0;
            if ( stagedCleaveTransformDrag_.mode == StagedCleaveTransformMode::Rotate )
            {
                TVector initialRadial = stagedCleaveTransformDrag_.initialHit
                    - stagedCleaveTransformDrag_.pivot;
                TVector currentRadial = *hit - stagedCleaveTransformDrag_.pivot;
                const TVector normal = stagedCleaveTransformDrag_.planeNormal;
                initialRadial = initialRadial
                    - normal * render::cleaveDot(initialRadial, normal);
                currentRadial = currentRadial
                    - normal * render::cleaveDot(currentRadial, normal);
                const double initialLength = cleaveVectorLength(initialRadial);
                const double currentLength = cleaveVectorLength(currentRadial);
                if ( initialLength > 1.0e-8 && currentLength > 1.0e-8 )
                {
                    const double sine = render::cleaveDot(
                        normal,
                        render::cleaveCross(initialRadial, currentRadial));
                    const double cosine = render::cleaveDot(
                        initialRadial,
                        currentRadial);
                    appliedAngle = std::atan2(sine, cosine)
                        * (180.0 / 3.14159265358979323846);
                    if ( cleaveAngleStepDegrees_ > 0.0 )
                    {
                        appliedAngle = std::round(
                            appliedAngle / cleaveAngleStepDegrees_)
                            * cleaveAngleStepDegrees_;
                    }
                }
                else
                {
                    event->accept();
                    return;
                }
            }
            else if ( stagedCleaveTransformDrag_.mode
                == StagedCleaveTransformMode::Scale )
            {
                TVector initialRadial = stagedCleaveTransformDrag_.initialHit
                    - stagedCleaveTransformDrag_.pivot;
                TVector currentRadial = *hit - stagedCleaveTransformDrag_.pivot;
                const TVector normal = stagedCleaveTransformDrag_.planeNormal;
                initialRadial = initialRadial
                    - normal * render::cleaveDot(initialRadial, normal);
                currentRadial = currentRadial
                    - normal * render::cleaveDot(currentRadial, normal);
                const double initialLength = cleaveVectorLength(initialRadial);
                const double currentLength = cleaveVectorLength(currentRadial);
                if ( initialLength > 1.0e-8 && std::isfinite(currentLength) )
                {
                    appliedScale = std::clamp(
                        currentLength / initialLength,
                        0.05,
                        1000.0);
                }
                else
                {
                    event->accept();
                    return;
                }
            }

            if ( updateStagedCleaveGroupTransform(appliedAngle, appliedScale) )
            {
                stagedCleaveTransformDrag_.currentPosition = currentPos;
                leftDragged_ = render::cleavePointerMotionIsDrag(
                    currentPos - stagedCleaveTransformDrag_.startPosition);
                update();
            }
        }
        event->accept();
        return;
    }

    if (stagedCleavePlaneDrag_.active && buttons.testFlag(Qt::LeftButton))
    {
        QMatrix4x4 projection{};
        QMatrix4x4 cameraView{};
        QVector3D eye{};
        QVector3D target{};
        float nearPlane{};
        float farPlane{};
        buildCameraMatrices(projection, cameraView, eye, target, nearPlane, farPlane);
        const QMatrix4x4 mvp = projection * cameraView;
        bool invertible = false;
        const QMatrix4x4 inverseMvp = mvp.inverted(&invertible);
        const bool orthographic = camera_.projectionType()
            == app::ViewportCamera::ProjectionType::Orthographic;
        const std::vector<StagedCleaveLine>& gestureBaseline =
            stagedCleaveGestureBaselineLines();
        if (invertible && stagedCleavePlaneDrag_.translationReady)
        {
            const auto hit = intersectCleaveWidgetRayWithPlane(
                currentPos,
                projection, cameraView,
                eye,
                orthographic,
                width(),
                height(),
                stagedCleavePlaneDrag_.planePoint,
                stagedCleavePlaneDrag_.planeNormal);
            const std::int32_t lineIndex = stagedCleavePlaneDrag_.line;
            if (hit && lineIndex >= 0
                && static_cast<std::size_t>(lineIndex) < gestureBaseline.size())
            {
                const std::size_t index = static_cast<std::size_t>(lineIndex);
                const bool realDrag = render::cleavePointerMotionIsDrag(
                    currentPos - leftPressPos_);
                if (realDrag && !stagedCleaveDragSelectionApplied_)
                {
                    setSelectedStagedCleaveLines(
                        stagedCleavePlaneDrag_.movingLines,
                        lineIndex,
                        false);
                    stagedCleaveDragSelectionApplied_ = true;
                }
                if (realDrag
                    && mods.testFlag(Qt::AltModifier)
                    && !stagedCleavePlaneDrag_.disconnectedForDrag)
                {
                    // Alt is topology-only and intentionally delayed until a
                    // real drag. Keep the immutable history baseline untouched;
                    // detach the selected group from outside handles once and
                    // never carry those outside endpoints in the move set.
                    stagedCleavePlaneDrag_.disconnectedForDrag = true;
                    const std::vector<std::int32_t>& movingLines =
                        stagedCleavePlaneDrag_.movingLines;
                    auto& affectedLines = stagedCleavePlaneDrag_.affectedLines;
                    affectedLines.erase(
                        std::remove_if(
                            affectedLines.begin(),
                            affectedLines.end(),
                            [&movingLines](const StagedCleaveLineMove& affected)
                            {
                                return !std::binary_search(
                                    movingLines.begin(),
                                    movingLines.end(),
                                    affected.line);
                            }),
                        affectedLines.end());
                }
                TVector planeNormal = gestureBaseline[index].planeNormal;
                normalizeInPlace(planeNormal);
                const TVector pointerTranslation = *hit - stagedCleavePlaneDrag_.initialHit;
                const bool shiftHeld = mods.testFlag(Qt::ShiftModifier);
                const bool controlHeld = mods.testFlag(Qt::ControlModifier);
                const bool constrainedMove = shiftHeld && controlHeld;
                const bool geometrySnap = shiftHeld && !controlHeld;
                const bool gridSnap = !shiftHeld
                    && (transformGridSnapEnabled_ != controlHeld);
                TVector translation{};
                stagedCleavePlaneDrag_.preserveRigidLinePlacement = !constrainedMove;

                if (geometrySnap)
                {
                    // Move freely in the pointer interaction plane, then let
                    // either moved endpoint acquire the closest candidate. The
                    // chosen world delta is applied once to both endpoints.
                    translation = pointerTranslation;
                    const TVector movedFirst = gestureBaseline[index].first + translation;
                    const TVector movedSecond = gestureBaseline[index].second + translation;
                    const ProjectedPoint projectedFirst = projectWorldToWidget(
                        mvp, movedFirst, width(), height());
                    const ProjectedPoint projectedSecond = projectWorldToWidget(
                        mvp, movedSecond, width(), height());
                    cleaveSnapMarker_.reset();
                    const bool snapEndpointsValid =
                        stagedCleavePlaneDrag_.grabbedEndpoint == 0
                            ? projectedFirst.valid
                            : stagedCleavePlaneDrag_.grabbedEndpoint == 1
                                ? projectedSecond.valid
                                : projectedFirst.valid && projectedSecond.valid;
                    if (snapEndpointsValid)
                    {
                        constexpr double kLineEndpointSnapTolerancePx = 14.0;
                        const std::vector<render::CleaveSnapCandidate> candidates =
                            cleaveSnapTargets(
                                mvp,
                                surfaceCleaveToolEnabled_,
                                lineIndex);
                        const auto snapped = render::selectNearestCleaveRigidEndpointSnap(
                            projectedFirst.pos,
                            projectedSecond.pos,
                            candidates,
                            kLineEndpointSnapTolerancePx,
                            stagedCleavePlaneDrag_.grabbedEndpoint >= 0
                                ? std::optional<std::int32_t>(
                                    stagedCleavePlaneDrag_.grabbedEndpoint)
                                : std::nullopt);
                        if (snapped)
                        {
                            const TVector movedEndpoint =
                                stagedCleavePlaneDrag_.grabbedEndpoint == 1
                                    ? movedSecond
                                    : stagedCleavePlaneDrag_.grabbedEndpoint == 0
                                        ? movedFirst
                                        : snapped->endpoint == 0
                                            ? movedFirst
                                            : movedSecond;
                            translation = translation + snapped->target.position - movedEndpoint;
                            cleaveSnapMarker_ = snapped->target;
                        }
                    }
                }
                else if (gridSnap)
                {
                    // Snap an authored line anchor, not the pointer hit. The
                    // ray/viewport round trip can offset initialHit by a
                    // fraction of a pixel; snapping the pointer and then
                    // subtracting that offset leaves the line itself visibly
                    // off-grid.
                    translation = pointerTranslation;
                    const TVector movedAnchor = stagedCleavePlaneDrag_.grabbedEndpoint == 1
                        ? gestureBaseline[index].second + translation
                        : gestureBaseline[index].first + translation;
                    const render::TransformGridFrame gridFrame =
                        activeTransformGridFrame(
                            cameraView,
                            mvp,
                            inverseMvp,
                            eye,
                            target,
                            orthographic);
                    const TVector snappedAnchor =
                        render::snapPointToGridOnPlane(
                            movedAnchor,
                            stagedCleavePlaneDrag_.planePoint,
                            stagedCleavePlaneDrag_.planeNormal,
                            gridFrame);
                    translation = translation + snappedAnchor - movedAnchor;
                    cleaveSnapMarker_.reset();
                }
                else if (constrainedMove)
                {
                    // Ctrl+Shift exposes the original plane-normal movement:
                    // tangent pointer motion alone does not change an infinite
                    // cut plane, and Sector handles remain camera-edge bound.
                    translation = planeNormal
                        * render::cleaveDot(pointerTranslation, planeNormal);
                    cleaveSnapMarker_.reset();
                }
                else
                {
                    // Plain body dragging is a free rigid translation in the
                    // pointer interaction plane. This preserves the authored
                    // line angle and length without requiring a modifier.
                    translation = pointerTranslation;
                    cleaveSnapMarker_.reset();
                }
                const bool translated = translateStagedCleavePlane(
                    index,
                    translation,
                    &stagedCleavePlaneDrag_);
                if ( translated && stagedCleavePlaneDrag_.disconnectedForDrag )
                {
                    const std::vector<std::int32_t>& movingLines =
                        stagedCleavePlaneDrag_.movingLines;
                    const auto isMoving = [&movingLines](const std::int32_t candidate)
                    {
                        return std::binary_search(
                            movingLines.begin(),
                            movingLines.end(),
                            candidate);
                    };
                    for ( const std::int32_t movingLine : movingLines )
                    {
                        if ( movingLine < 0
                            || static_cast<std::size_t>(movingLine)
                                >= stagedCleaveLines_.size() )
                        {
                            continue;
                        }
                        StagedCleaveLine& line = stagedCleaveLines_[
                            static_cast<std::size_t>(movingLine)];
                        const std::int32_t previous = movingLine == 0
                            ? static_cast<std::int32_t>(stagedCleaveLines_.size() - 1u)
                            : movingLine - 1;
                        if ( line.linkedToPrevious && !isMoving(previous) )
                        {
                            line.linkedToPrevious = false;
                            stagedCleaveLines_[static_cast<std::size_t>(previous)]
                                .linkedToNext = false;
                        }
                        const std::int32_t next =
                            (movingLine + 1) % static_cast<std::int32_t>(
                                stagedCleaveLines_.size());
                        if ( line.linkedToNext && !isMoving(next) )
                        {
                            line.linkedToNext = false;
                            stagedCleaveLines_[static_cast<std::size_t>(next)]
                                .linkedToPrevious = false;
                        }
                    }
                }
                if ( translated && geometrySnap
                    && !stagedCleavePlaneDrag_.disconnectedForDrag )
                {
                    // Position and topology are one Shift-snap operation. A
                    // rigid line returned to either authored neighbour must
                    // not require a second endpoint click to restore the
                    // reciprocal chain bits.
                    (void)reconnectStagedCleaveLineLinks(index);
                }
                if ( translated && sectorCleaveToolEnabled_
                    && !cleaveBounded_
                    && !stagedCleavePlaneDrag_.preserveRigidLinePlacement )
                {
                    // Keep the finite handle on the moving intersection edge
                    // during the drag, not only after mouse release.
                    (void)attachStagedSectorCleaveLineToCameraEdge(index);
                }
                selectedStagedCleaveLine_ = lineIndex;
                leftDragged_ = translated && realDrag && translation.lengthSqr() > 1.0e-12f;
                update();
            }
        }
        event->accept();
        return;
    }

    if (buttons.testFlag(Qt::LeftButton) && (sectorCleaveToolEnabled_ || surfaceCleaveToolEnabled_))
    {
        if (!sectorCleaveHaveFirstPoint_ && !surfaceCleaveHaveFirstPoint_)
        {
            event->accept();
            return;
        }

        if (cleaveStagedMode_
            && stagedCleaveLineClickCandidate_
            && editingStagedCleaveLine_ >= 0
            && render::cleavePointerMotionIsDrag(currentPos - leftPressPos_)
            && !stagedCleaveDragSelectionApplied_)
        {
            if (!stagedCleaveLineSelected(editingStagedCleaveLine_))
            {
                setSelectedStagedCleaveLines(
                    {editingStagedCleaveLine_},
                    editingStagedCleaveLine_,
                    false);
            }
            else
            {
                setSelectedStagedCleaveLines(
                    selectedStagedCleaveLines_,
                    editingStagedCleaveLine_,
                    false);
            }
            stagedCleaveDragSelectionApplied_ = true;
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
            event->accept();
            return;
        }

        const bool isOrtho = camera_.projectionType() == app::ViewportCamera::ProjectionType::Orthographic;

        if (sectorCleaveToolEnabled_ && sectorCleaveHaveFirstPoint_)
        {
            TVector workNormal = (render::cleaveDocumentPoint(target) - render::cleaveDocumentPoint(eye));
            TVector workPlanePoint = sectorCleaveFirstPoint_;
            if (cleaveStagedMode_
                && editingStagedCleaveLine_ >= 0
                && static_cast<std::size_t>(editingStagedCleaveLine_) < stagedCleaveLines_.size())
            {
                const StagedCleaveLine& edited =
                    stagedCleaveLines_[static_cast<std::size_t>(editingStagedCleaveLine_)];
                workNormal = edited.constructionPlaneNormal;
                workPlanePoint = edited.first;
            }
            if (normalizeInPlace(workNormal))
            {
                // View matrix is world->camera; its rows are the camera basis in world space.
                TVector axisA(view(0, 0), view(0, 1), view(0, 2)); // camera right
                TVector axisB(view(1, 0), view(1, 1), view(1, 2)); // camera up
                if (!normalizeInPlace(axisA))
                {
                    axisA = TVector(1.0f, 0.0f, 0.0f);
                }
                axisA = axisA - workNormal * render::cleaveDot(axisA, workNormal);
                if (!normalizeInPlace(axisA))
                {
                    axisA = render::cleaveCross(
                        std::abs(workNormal.z) < 0.9f
                            ? TVector(0.0f, 0.0f, 1.0f)
                            : TVector(0.0f, 1.0f, 0.0f),
                        workNormal);
                    normalizeInPlace(axisA);
                }
                axisB = render::cleaveCross(workNormal, axisA);
                if (!normalizeInPlace(axisB))
                {
                    axisB = TVector(0.0f, 1.0f, 0.0f);
                }

                if (const auto hitOpt = intersectCleaveWidgetRayWithPlane(
                        currentPos,
                        proj, view,
                        eye,
                        isOrtho,
                        width(),
                        height(),
                        workPlanePoint,
                        workNormal))
                {
                    TVector hit = *hitOpt;

                    const bool editingEndpoint = editingStagedCleaveLine_ >= 0
                        && editingStagedCleaveEndpoint_ >= 0;
                    const render::CleavePointDragConstraint pointConstraint =
                        render::cleavePointDragConstraint(
                            editingEndpoint,
                            mods.testFlag(Qt::ShiftModifier),
                            mods.testFlag(Qt::ControlModifier),
                            transformGridSnapEnabled_,
                            cleaveAngleStepDegrees_ > 0.0);

                    // Angle-step remains the endpoint default when grid snap
                    // is off; the configured Grid Snap toggle and Ctrl
                    // inversion apply when no higher-priority constraint is
                    // active.
                    if (cleaveStagedMode_ && cleaveAngleStepDegrees_ > 0.0
                        && pointConstraint == render::CleavePointDragConstraint::Angle)
                    {
                        TVector angleReference = axisA;
                        if (stagedCleaveContext_.valid)
                        {
                            angleReference = TVector(
                                stagedCleaveContext_.view(0, 0),
                                stagedCleaveContext_.view(0, 1),
                                stagedCleaveContext_.view(0, 2));
                        }
                        hit = snapStagedCleaveEndpointToAngleStep(
                            sectorCleaveFirstPoint_,
                            hit,
                            workNormal,
                            angleReference);
                    }

                    if (pointConstraint == render::CleavePointDragConstraint::Grid)
                    {
                        const render::TransformGridFrame gridFrame =
                            activeTransformGridFrame(
                                view, mvp, invMvp, eye, target, isOrtho);
                        hit = render::snapPointToGridOnPlane(
                            hit, workPlanePoint, workNormal, gridFrame);
                    }

                    cleaveSnapMarker_.reset();
                    if (editingStagedCleaveLine_ >= 0)
                    {
                        // An edited endpoint is an explicit authored position.
                        // Never reattach it to the camera-facing Sector edge
                        // after release, including an unmodified free drag.
                        editingStagedCleaveEndpointPreservePlacement_ = true;
                    }
                    if (pointConstraint == render::CleavePointDragConstraint::Geometry)
                    {
                        constexpr double kEndpointSnapTolerancePx = 18.0;
                        cleaveSnapMarker_ = closestCleaveSnapTarget(
                            currentPos,
                            mvp,
                            false,
                            kEndpointSnapTolerancePx,
                            editingStagedCleaveLine_);
                        if (cleaveSnapMarker_)
                        {
                            hit = cleaveSnapMarker_->position;
                        }
                    }

                    if (editingStagedCleaveLine_ >= 0 && editingStagedCleaveEndpoint_ >= 0
                        && !render::cleaveSameHandle(sectorCleaveCurrentPoint_, hit))
                    {
                        ++stagedCleaveJointGuidePreviewRevision_;
                    }
                    sectorCleaveCurrentPoint_ = hit;
                }
            }
        }

        if (surfaceCleaveToolEnabled_ && surfaceCleaveHaveFirstPoint_)
        {
            cleaveSnapMarker_.reset();
            if (level_ && selectedSurfaceSector_ >= 0 && selectedSurface_ >= 0
                && static_cast<std::size_t>(selectedSurfaceSector_) < level_->sectors.size())
            {
                const Sector& sector = level_->sectors[static_cast<std::size_t>(selectedSurfaceSector_)];
                if (static_cast<std::size_t>(selectedSurface_) < sector.surfaces.size())
                {
                    const Surface& surface = sector.surfaces[static_cast<std::size_t>(selectedSurface_)];
                    if (surface.vertices.size() >= 3)
                    {
                        const std::int32_t v0i = surface.vertices[0];
                        if (v0i >= 0 && static_cast<std::size_t>(v0i) < sector.vertices.size())
                        {
                            const Vertex& v0 = sector.vertices[static_cast<std::size_t>(v0i)];
                            const TVector planePoint(v0.x, v0.y, v0.z);

                            TVector surfNormal(surface.normal.x, surface.normal.y, surface.normal.z);
                            if (surfNormal.lengthSqr() < sed::math::kTinyEpsilonF)
                            {
                                const std::int32_t v1i = surface.vertices[1];
                                const std::int32_t v2i = surface.vertices[2];
                                if (v1i >= 0 && v2i >= 0
                                    && static_cast<std::size_t>(v1i) < sector.vertices.size()
                                    && static_cast<std::size_t>(v2i) < sector.vertices.size())
                                {
                                    const Vertex& v1 = sector.vertices[static_cast<std::size_t>(v1i)];
                                    const Vertex& v2 = sector.vertices[static_cast<std::size_t>(v2i)];
                                    const TVector p1(v1.x, v1.y, v1.z);
                                    const TVector p2(v2.x, v2.y, v2.z);
                                    surfNormal = render::cleaveCross(p1 - planePoint, p2 - planePoint);
                                }
                            }

                            if (normalizeInPlace(surfNormal))
                            {
                                if (const auto hitOpt = intersectCleaveWidgetRayWithPlane(currentPos, proj, view, eye, isOrtho, width(), height(), planePoint, surfNormal))
                                {
                                    TVector hit = *hitOpt;

                                    const bool editingEndpoint = editingStagedCleaveLine_ >= 0
                                        && editingStagedCleaveEndpoint_ >= 0;
                                    const render::CleavePointDragConstraint pointConstraint =
                                        render::cleavePointDragConstraint(
                                            editingEndpoint,
                                            mods.testFlag(Qt::ShiftModifier),
                                            mods.testFlag(Qt::ControlModifier),
                                            transformGridSnapEnabled_,
                                            cleaveAngleStepDegrees_ > 0.0);

                                    // New lines retain the configured default
                                    // angle step. Edited Multi-cut endpoints
                                    // are free unless Ctrl+Shift requests it.
                                    if (cleaveStagedMode_ && cleaveAngleStepDegrees_ > 0.0
                                        && pointConstraint == render::CleavePointDragConstraint::Angle)
                                    {
                                        TVector angleReference(
                                            view(0, 0),
                                            view(0, 1),
                                            view(0, 2));
                                        if (stagedCleaveContext_.valid)
                                        {
                                            angleReference = TVector(
                                                stagedCleaveContext_.view(0, 0),
                                                stagedCleaveContext_.view(0, 1),
                                                stagedCleaveContext_.view(0, 2));
                                        }
                                        hit = snapStagedCleaveEndpointToAngleStep(
                                            surfaceCleaveFirstPoint_,
                                            hit,
                                            surfNormal,
                                            angleReference);
                                    }

                                    const bool vertexSnap = pointConstraint
                                        == render::CleavePointDragConstraint::Geometry;
                                    const bool gridSnap = pointConstraint
                                        == render::CleavePointDragConstraint::Grid;

                                    if (editingStagedCleaveLine_ >= 0)
                                    {
                                        editingStagedCleaveEndpointPreservePlacement_ = true;
                                    }

                                    if (gridSnap || vertexSnap)
                                    {
                                        if (gridSnap)
                                        {
                                            const render::TransformGridFrame gridFrame =
                                                activeTransformGridFrame(
                                                    view,
                                                    mvp,
                                                    invMvp,
                                                    eye,
                                                    target,
                                                    isOrtho);
                                            hit = render::snapPointToGridOnPlane(
                                                hit,
                                                planePoint,
                                                surfNormal,
                                                gridFrame);
                                        }

                                        if (vertexSnap)
                                        {
                                            constexpr double kEndpointSnapTolerancePx = 18.0;
                                            cleaveSnapMarker_ = closestCleaveSnapTarget(
                                                currentPos,
                                                mvp,
                                                true,
                                                kEndpointSnapTolerancePx,
                                                editingStagedCleaveLine_);
                                            if (cleaveSnapMarker_)
                                            {
                                                hit = cleaveSnapMarker_->position;
                                            }
                                        }
                                    }

                                    if (editingStagedCleaveLine_ >= 0 && editingStagedCleaveEndpoint_ >= 0
                                        && !render::cleaveSameHandle(surfaceCleaveCurrentPoint_, hit))
                                    {
                                        ++stagedCleaveJointGuidePreviewRevision_;
                                    }
                                    surfaceCleaveCurrentPoint_ = hit;
                                }
                            }
                        }
                    }
                }
            }
        }

        leftDragged_ = true;
        update();
        event->accept();
        return;
    }

    if (buttons.testFlag(Qt::LeftButton))
    {
        leftDragged_ = true;
        if (boxSelectActive_)
        {
            boxSelectEnd_ = currentPos;
            const QRect r = QRect(boxSelectStart_, boxSelectEnd_).normalized();
            boxSelectVisible_ = (r.width() > 4 || r.height() > 4);
            update();
            event->accept();
            return;
        }

        event->accept();
        return;
    }

    if (buttons.testFlag(Qt::MiddleButton))
    {
        const QPointF cameraDelta = viewportCameraMouseDelta(delta);
        if (mods.testFlag(Qt::ControlModifier))
        {
            const float speed = mods.testFlag(Qt::ShiftModifier) ? 4.0f : 1.0f;
            const float factor = std::exp(static_cast<float>(cameraDelta.y()) * 0.005f * speed);

            if (camera_.projectionType() == app::ViewportCamera::ProjectionType::Perspective)
            {
                const QVector3D camDir =
                    app::ViewportCamera::SphericalDirection(camera_.orbit.yawDeg, camera_.orbit.pitchDeg).normalized();
                const QVector3D forward = -camDir;

                // "Dolly" should move the camera rig forward/back along the look direction (not zoom).
                // We do this by translating the orbit target and eye together (pan), leaving distance unchanged.
                const float baseDist = std::max(camera_.orbit.distance, camera_.orbit.radius * 0.05f);
                const float dolly = baseDist * (1.0f - factor);
                camera_.orbit.pan += forward * dolly;
            }
            else
            {
                const float scale = camera_.orthoScale() / std::max(factor, 1e-6f);
                camera_.SetOrthoScale(std::clamp(scale, 0.01f, 1.0e6f));
            }

            markViewChanged();
            update();
            return;
        }

        if (mods.testFlag(Qt::ShiftModifier))
        {
            if (!viewportPickingEnabled_ && previewInteractionEnabled_)
            {
                event->accept();
                return;
            }
            orbitPanPixels(cameraDelta);
            return;
        }

        const float yawDeltaDeg = -static_cast<float>(cameraDelta.x()) * 0.25f;
        const float pitchDeltaDeg = static_cast<float>(cameraDelta.y()) * 0.25f;
        if (selectionOrbitDragPivot_)
        {
            const std::optional<ui::viewport::OrbitRigUpdate> revolved =
                ui::viewport::orbitRigAroundPivot(
                    camera_.orbit.target(),
                    camera_.orbit.distance,
                    camera_.orbit.yawDeg,
                    camera_.orbit.pitchDeg,
                    camera_.orbit.rollDeg,
                    *selectionOrbitDragPivot_,
                    yawDeltaDeg,
                    pitchDeltaDeg);
            if (revolved)
            {
                camera_.orbit.pan = revolved->target - camera_.orbit.center;
                camera_.orbit.yawDeg = revolved->yawDeg;
                camera_.orbit.pitchDeg = revolved->pitchDeg;
                camera_.orbit.rollDeg = revolved->rollDeg;
                camera_.orbit.distance = revolved->distance;
            }
        }
        else
        {
            orbitRotate(yawDeltaDeg, pitchDeltaDeg);
            event->accept();
            return;
        }
        markViewChanged();
        update();
        event->accept();
        return;
    }

    if (buttons == Qt::NoButton && hoverPickEnabled_ && viewportPickingEnabled_ && level_)
    {
        if (pickMode_ == PickMode::ArchitecturalLighting
            && renderPipeline_ == RenderPipeline::Editor
            && architecturalLightingEditingActive_)
        {
            QMatrix4x4 proj;
            QMatrix4x4 view;
            QVector3D eye{};
            QVector3D target{};
            float nearPlane{};
            float farPlane{};
            buildCameraMatrices(proj, view, eye, target, nearPlane, farPlane);
            (void)eye;
            (void)target;
            (void)nearPlane;
            (void)farPlane;
            const std::optional<ArchitecturalLightingRenderVertex> hit =
                architecturalLightingVertexAt(
                    currentPos,
                    proj * view,
                    xraySelectionCyclingEnabled_);
            const bool changed = hoveredArchitecturalLightingVertex_ != hit;
            hoveredArchitecturalLightingVertex_ = hit;
            setCursor(hit ? Qt::PointingHandCursor : Qt::ArrowCursor);
            if (changed)
            {
                Q_EMIT hoverPicked(
                    -1,
                    -1,
                    hit ? hit->thingIndex : -1,
                    -1);
                update();
            }
            event->accept();
            return;
        }

        std::int32_t hoverSector = -1;
        std::int32_t hoverSurface = -1;
        std::int32_t hoverThing = -1;
        std::int32_t hoverLight = -1;

        PickMode mode = pickMode_;
        if (mode == PickMode::Auto)
        {
            switch (pickPreference_)
            {
                case PickPreference::PreferSurface:
                {
                    mode = PickMode::Surface;
                    break;
                }
                case PickPreference::PreferThing:
                {
                    mode = PickMode::Thing;
                    break;
                }
                case PickPreference::PreferLight:
                {
                    mode = PickMode::Light;
                    break;
                }
                default:
                {
                    break;
                }
            }
        }

        QMatrix4x4 proj;
        QMatrix4x4 view;
        QVector3D eye{};
        QVector3D target{};
        float nearPlane{};
        float farPlane{};
        buildCameraMatrices(proj, view, eye, target, nearPlane, farPlane);
        (void)nearPlane;
        const QMatrix4x4 mvp = proj * view;

        const bool wireframeBaseMode = !showSolid_ && !showVertices_;
        const bool wireVisible = wireframeBaseMode || showWireframe_;
        const QPointF mp(currentPos);

        TVector hoverRayDirection{};
        bool haveHoverRayDirection = false;
        {
            bool invOk = false;
            const QMatrix4x4 inv = mvp.inverted(&invOk);
            if (invOk)
            {
                auto toAffine = [](const QVector4D& v) -> QVector3D
                {
                    if (qFuzzyIsNull(v.w()))
                    {
                        return QVector3D(0.0f, 0.0f, 0.0f);
                    }
                    return QVector3D(v.x() / v.w(), v.y() / v.w(), v.z() / v.w());
                };

                const float ndcX = (2.0f * static_cast<float>(currentPos.x()) / static_cast<float>(width())) - 1.0f;
                const float ndcY = 1.0f - (2.0f * static_cast<float>(currentPos.y()) / static_cast<float>(height()));
                const QVector3D nearWorld = toAffine(inv * QVector4D(ndcX, ndcY, -1.0f, 1.0f));
                const QVector3D farWorld = toAffine(inv * QVector4D(ndcX, ndcY, 1.0f, 1.0f));
                const bool isOrtho = camera_.projectionType() == app::ViewportCamera::ProjectionType::Orthographic;
                const QVector3D origin3 = isOrtho ? nearWorld : eye;
                QVector3D rayDir3 = farWorld - origin3;
                if (rayDir3.lengthSquared() > 1e-10f)
                {
                    rayDir3.normalize();
                    hoverRayDirection = TVector{
                        static_cast<double>(rayDir3.x()),
                        static_cast<double>(rayDir3.y()),
                        static_cast<double>(rayDir3.z()) };
                    haveHoverRayDirection = true;
                }
            }
        }

        if (!haveHoverRayDirection)
        {
            QVector3D fallbackDir = target - eye;
            if (fallbackDir.lengthSquared() > 1e-10f)
            {
                fallbackDir.normalize();
                hoverRayDirection = TVector{
                    static_cast<double>(fallbackDir.x()),
                    static_cast<double>(fallbackDir.y()),
                    static_cast<double>(fallbackDir.z()) };
                haveHoverRayDirection = true;
            }
        }

        auto projectToScreen = [&](const TVector& wp, QPointF& outPos) -> bool
        {
            const QVector4D clip = mvp * QVector4D(
                static_cast<float>(wp.x),
                static_cast<float>(wp.y),
                static_cast<float>(wp.z),
                1.0f);
            if (qFuzzyIsNull(clip.w()) || clip.w() <= 0.0f)
            {
                return false;
            }

            const QVector3D ndc(clip.x() / clip.w(), clip.y() / clip.w(), clip.z() / clip.w());
            const float sx = (ndc.x() * 0.5f + 0.5f) * static_cast<float>(width());
            const float sy = (1.0f - (ndc.y() * 0.5f + 0.5f)) * static_cast<float>(height());
            outPos = QPointF(sx, sy);
            return true;
        };

        auto projectSpriteBounds = [&](const std::size_t thingIndex, QRectF& outRect, float& outDepth) -> bool
        {
            if (thingIndex >= spriteThingBounds_.size())
            {
                return false;
            }
            const ThingBounds& bounds = spriteThingBounds_[thingIndex];
            if (!bounds.valid)
            {
                return false;
            }

            const QVector3D corners[8]{
                { bounds.min.x(), bounds.min.y(), bounds.min.z() },
                { bounds.max.x(), bounds.min.y(), bounds.min.z() },
                { bounds.min.x(), bounds.max.y(), bounds.min.z() },
                { bounds.max.x(), bounds.max.y(), bounds.min.z() },
                { bounds.min.x(), bounds.min.y(), bounds.max.z() },
                { bounds.max.x(), bounds.min.y(), bounds.max.z() },
                { bounds.min.x(), bounds.max.y(), bounds.max.z() },
                { bounds.max.x(), bounds.max.y(), bounds.max.z() },
            };

            float minX = std::numeric_limits<float>::infinity();
            float minY = std::numeric_limits<float>::infinity();
            float maxX = -std::numeric_limits<float>::infinity();
            float maxY = -std::numeric_limits<float>::infinity();
            float minDepth = std::numeric_limits<float>::infinity();
            bool visible = false;
            for (const QVector3D& corner : corners)
            {
                const QVector4D clip = mvp * QVector4D(corner, 1.0f);
                if (!std::isfinite(clip.w()) || clip.w() <= 0.0f)
                {
                    continue;
                }
                const QVector3D ndc = clip.toVector3D() / clip.w();
                if (!std::isfinite(ndc.x()) || !std::isfinite(ndc.y()) || !std::isfinite(ndc.z())
                    || ndc.z() < -1.0f || ndc.z() > 1.0f)
                {
                    continue;
                }
                const float sx = (ndc.x() * 0.5f + 0.5f) * static_cast<float>(width());
                const float sy = (1.0f - (ndc.y() * 0.5f + 0.5f)) * static_cast<float>(height());
                minX = std::min(minX, sx);
                minY = std::min(minY, sy);
                maxX = std::max(maxX, sx);
                maxY = std::max(maxY, sy);
                minDepth = std::min(minDepth, ndc.z() * 0.5f + 0.5f);
                visible = true;
            }
            if (!visible)
            {
                return false;
            }
            outRect = QRectF(QPointF(minX, minY), QPointF(maxX, maxY)).normalized();
            outDepth = minDepth;
            return outRect.isValid();
        };

        auto surfaceBestEdgeDistanceSq = [&](const Sector& sector, const Surface& surface) -> std::optional<float>
        {
            const auto& vxs = surface.vertices;
            if (vxs.size() < 2)
            {
                return std::nullopt;
            }

            float best = std::numeric_limits<float>::infinity();

            for (std::size_t i = 0; i < vxs.size(); ++i)
            {
                const std::int32_t aidx = vxs[i];
                const std::int32_t bidx = vxs[(i + 1) % vxs.size()];
                if (aidx < 0 || bidx < 0)
                {
                    continue;
                }
                if (static_cast<std::size_t>(aidx) >= sector.vertices.size()
                    || static_cast<std::size_t>(bidx) >= sector.vertices.size())
                {
                    continue;
                }

                const Vertex& va = sector.vertices[static_cast<std::size_t>(aidx)];
                const Vertex& vb = sector.vertices[static_cast<std::size_t>(bidx)];

                QPointF sa{};
                QPointF sb{};
                if (!projectToScreen(TVector{ va.x, va.y, va.z }, sa)
                    || !projectToScreen(TVector{ vb.x, vb.y, vb.z }, sb))
                {
                    continue;
                }

                best = std::min(best, distanceSqPointToSegment(mp, sa, sb));
            }

            if (!std::isfinite(best))
            {
                return std::nullopt;
            }

            return best;
        };

        auto clickNearAnyEdge = [&](const Sector& sector, const Surface& surface) -> bool
        {
            constexpr float kWireEdgeTolPx = 3.0f;
            const float wireTol2 = kWireEdgeTolPx * kWireEdgeTolPx;
            const std::optional<float> best = surfaceBestEdgeDistanceSq(sector, surface);
            return best && *best <= wireTol2;
        };

        auto surfaceAllowsFaceHoverPick = [&](const std::int32_t sectorIndex, const std::int32_t surfaceIndex) -> bool
        {
            if (sectorIndex < 0 || surfaceIndex < 0)
            {
                return false;
            }
            if (static_cast<std::size_t>(sectorIndex) >= level_->sectors.size())
            {
                return false;
            }

            const Sector& sector = level_->sectors[static_cast<std::size_t>(sectorIndex)];
            if (static_cast<std::size_t>(surfaceIndex) >= sector.surfaces.size())
            {
                return false;
            }

            const Surface& surface = sector.surfaces[static_cast<std::size_t>(surfaceIndex)];
            const bool backfaceSingleSided =
                mode != PickMode::Sector
                && haveHoverRayDirection
                && ((surface.faceFlags & FF_DoubleSided) == 0u)
                && (surface.normal.Dot(hoverRayDirection) > 1e-9);
            const bool edgeOnly =
                (surface.geo < static_cast<std::int32_t>(TGeoMode::Solid))
                || backfaceSingleSided;

            if (!edgeOnly)
            {
                return true;
            }

            if (!wireVisible)
            {
                return false;
            }

            return clickNearAnyEdge(sector, surface);
        };

        bool picked = false;

        if (glReady_ && pickProgram_ && triVao_ && triVertexCount_ > 0
            && (mode == PickMode::Sector || mode == PickMode::Surface || mode == PickMode::Thing))
        {
            struct ContextGuard
            {
                GlView& view;
                explicit ContextGuard(GlView& v)
                    : view(v)
                {
                    view.makeCurrent();
                }
                ~ContextGuard()
                {
                    view.doneCurrent();
                }
            };

            ContextGuard guard(*this);

            if (mode == PickMode::Sector || mode == PickMode::Surface)
            {
                const int surfaceGeoMin = wireVisible ? 0 : static_cast<int>(TGeoMode::Solid);
                // Hover follows ordinary visible Surface eligibility.
                const bool levelCullBackFaces = render::picking::
                    solidLevelPointCandidatesCullBackFaces(
                        mode != PickMode::Surface);
                constexpr int kMaxPeels = 16;
                const int maxHoverPeels =
                    (surfaceSubMode_ == SurfaceSubMode::Face && !wireVisible)
                    ? 1
                    : kMaxPeels;
                const std::vector<PickHit> hits = pickPeel(
                    mvp,
                    currentPos,
                    TriKind::Level,
                    surfaceGeoMin,
                    levelCullBackFaces,
                    maxHoverPeels);

                std::optional<SurfaceRef> bestEdgeRef{};
                float bestEdgeD2 = std::numeric_limits<float>::infinity();
                constexpr float kSectorWirePickTolPx = 8.0f;
                const float sectorWirePickTol2 = kSectorWirePickTolPx * kSectorWirePickTolPx;

                std::optional<SurfaceRef> firstFaceRef{};
                for (const PickHit& hit : hits)
                {
                    if (hit.id >= pickObjects_.size())
                    {
                        continue;
                    }

                    const PickObject& obj = pickObjects_[hit.id];
                    if (obj.kind != PickKind::Surface)
                    {
                        continue;
                    }

                    const std::int32_t secIndex = obj.sector;
                    const std::int32_t surfIndex = obj.surface;
                    if (secIndex < 0 || surfIndex < 0
                        || static_cast<std::size_t>(secIndex) >= level_->sectors.size()
                        || static_cast<std::size_t>(surfIndex) >= level_->sectors[static_cast<std::size_t>(secIndex)].surfaces.size())
                    {
                        continue;
                    }

                    const Sector& sector = level_->sectors[static_cast<std::size_t>(secIndex)];
                    const Surface& surface = sector.surfaces[static_cast<std::size_t>(surfIndex)];

                    if (wireVisible)
                    {
                        const std::optional<float> d2 = surfaceBestEdgeDistanceSq(sector, surface);
                        if (d2 && *d2 <= sectorWirePickTol2 && *d2 < bestEdgeD2)
                        {
                            bestEdgeD2 = *d2;
                            bestEdgeRef = SurfaceRef{ secIndex, surfIndex };
                        }
                    }

                    if (!surfaceAllowsFaceHoverPick(obj.sector, obj.surface))
                    {
                        continue;
                    }

                    if (!firstFaceRef)
                    {
                        firstFaceRef = SurfaceRef{ secIndex, surfIndex };
                    }
                }

                std::optional<SurfaceRef> chosenRef{};
                if (mode == PickMode::Sector && firstFaceRef)
                {
                    // In Sector mode a wire-edge hover is only a fallback;
                    // the nearest filled face must win in ordinary picking.
                    chosenRef = firstFaceRef;
                }
                else if (bestEdgeRef)
                {
                    chosenRef = bestEdgeRef;
                }
                else if (firstFaceRef)
                {
                    chosenRef = firstFaceRef;
                }

                if (chosenRef)
                {
                    hoverSector = chosenRef->sector;
                    hoverSurface = (mode == PickMode::Surface) ? chosenRef->surface : -1;
                    picked = true;
                }
            }
            else if (mode == PickMode::Thing)
            {
                const std::vector<PickHit> hits = pickPeel(mvp, currentPos, TriKind::Thing, 0, false, 1);
                if (!hits.empty())
                {
                    const PickHit& hit = hits.front();
                    if (hit.id < pickObjects_.size())
                    {
                        const PickObject& obj = pickObjects_[hit.id];
                        if (obj.kind == PickKind::Thing)
                        {
                            hoverThing = obj.index;
                            picked = true;
                        }
                    }
                }
            }
        }

        if (!picked && (mode == PickMode::Sector || mode == PickMode::Surface || mode == PickMode::Thing))
        {
            if (mode == PickMode::Sector || mode == PickMode::Surface)
            {
                bool invOk = false;
                const QMatrix4x4 inv = mvp.inverted(&invOk);
                if (invOk)
                {
                    auto toAffine = [](const QVector4D& v) -> QVector3D
                    {
                        if (qFuzzyIsNull(v.w()))
                        {
                            return QVector3D(0.0f, 0.0f, 0.0f);
                        }
                        return QVector3D(v.x() / v.w(), v.y() / v.w(), v.z() / v.w());
                    };

                    const float ndcX = (2.0f * static_cast<float>(currentPos.x()) / static_cast<float>(width())) - 1.0f;
                    const float ndcY = 1.0f - (2.0f * static_cast<float>(currentPos.y()) / static_cast<float>(height()));
                    const QVector3D nearWorld = toAffine(inv * QVector4D(ndcX, ndcY, -1.0f, 1.0f));
                    const QVector3D farWorld = toAffine(inv * QVector4D(ndcX, ndcY, 1.0f, 1.0f));
                    const bool isOrtho = camera_.projectionType() == app::ViewportCamera::ProjectionType::Orthographic;
                    const QVector3D origin3 = isOrtho ? nearWorld : eye;
                    QVector3D rayDir = farWorld - origin3;
                    if (rayDir.lengthSquared() > 1e-10f)
                    {
                        rayDir.normalize();
                        const TVector origin{ static_cast<double>(origin3.x()), static_cast<double>(origin3.y()), static_cast<double>(origin3.z()) };
                        const TVector direction{ static_cast<double>(rayDir.x()), static_cast<double>(rayDir.y()), static_cast<double>(rayDir.z()) };

                        const double rayMaxDistance = render::viewportRayMaxDistanceForWidget(
                            proj, view, QPointF(currentPos), width(), height(), isOrtho)
                            .value_or(std::max(0.0, static_cast<double>(farPlane)));
                        std::vector<sed::level::SurfacePickHit> hits =
                            surfacePickIndex_.pickSurfaces(*level_, origin, direction, rayMaxDistance);

                        std::optional<SurfaceRef> bestEdgeRef{};
                        float bestEdgeD2 = std::numeric_limits<float>::infinity();
                        constexpr float kSectorWirePickTolPx = 8.0f;
                        const float sectorWirePickTol2 = kSectorWirePickTolPx * kSectorWirePickTolPx;

                        std::optional<SurfaceRef> firstFaceRef{};
                        for (const sed::level::SurfacePickHit& hit : hits)
                        {
                            const std::int32_t secIndex = hit.surface.sector;
                            const std::int32_t surfIndex = hit.surface.surface;
                            if (secIndex < 0 || surfIndex < 0
                                || static_cast<std::size_t>(secIndex) >= level_->sectors.size()
                                || static_cast<std::size_t>(surfIndex) >= level_->sectors[static_cast<std::size_t>(secIndex)].surfaces.size())
                            {
                                continue;
                            }

                            const Sector& sector = level_->sectors[static_cast<std::size_t>(secIndex)];
                            const Surface& surface = sector.surfaces[static_cast<std::size_t>(surfIndex)];
                            if (wireVisible)
                            {
                                const std::optional<float> d2 = surfaceBestEdgeDistanceSq(sector, surface);
                                if (d2 && *d2 <= sectorWirePickTol2 && *d2 < bestEdgeD2)
                                {
                                    bestEdgeD2 = *d2;
                                    bestEdgeRef = SurfaceRef{ secIndex, surfIndex };
                                }
                            }

                            if (!surfaceAllowsFaceHoverPick(hit.surface.sector, hit.surface.surface))
                            {
                                continue;
                            }

                            if (!firstFaceRef)
                            {
                                firstFaceRef = SurfaceRef{ secIndex, surfIndex };
                            }
                        }

                        std::optional<SurfaceRef> chosenRef{};
                        if (mode == PickMode::Sector && firstFaceRef)
                        {
                            chosenRef = firstFaceRef;
                        }
                        else if (bestEdgeRef)
                        {
                            chosenRef = bestEdgeRef;
                        }
                        else if (firstFaceRef)
                        {
                            chosenRef = firstFaceRef;
                        }

                        if (chosenRef)
                        {
                            hoverSector = chosenRef->sector;
                            hoverSurface = (mode == PickMode::Surface) ? chosenRef->surface : -1;
                            picked = true;
                        }
                    }
                }
            }
            else if (mode == PickMode::Thing)
            {
                constexpr float kRadiusPx = 10.0f;
                const float r2 = kRadiusPx * kRadiusPx;
                float bestDepth = std::numeric_limits<float>::infinity();

                for (std::size_t i = 0; i < level_->things.size(); ++i)
                {
                    const Thing& thing = level_->things[i];
                    if (!isLayerVisible(thing.layer))
                    {
                        continue;
                    }
                    if (!shouldShowThing(thing))
                    {
                        continue;
                    }
                    if (!std::isfinite(thing.x) || !std::isfinite(thing.y) || !std::isfinite(thing.z))
                    {
                        continue;
                    }

                    // GPU picking normally covers shaded SPR quads.  Keep a
                    // geometry-aware fallback for drivers/context transitions
                    // where the sprite VAO is unavailable: testing only the
                    // Thing origin made clicks on the visible outer parts of
                    // a sprite miss completely.
                    if (thingViewMode_ == ThingViewMode::Shaded)
                    {
                        QRectF spriteRect{};
                        float spriteDepth = std::numeric_limits<float>::infinity();
                        if (projectSpriteBounds(i, spriteRect, spriteDepth)
                            && spriteRect.adjusted(-4.0, -4.0, 4.0, 4.0).contains(QPointF(currentPos)))
                        {
                            if (spriteDepth < bestDepth)
                            {
                                bestDepth = spriteDepth;
                                hoverThing = static_cast<std::int32_t>(i);
                                picked = true;
                            }
                            continue;
                        }
                    }

                    const QVector4D clip = mvp * QVector4D(
                        static_cast<float>(thing.x),
                        static_cast<float>(thing.y),
                        static_cast<float>(thing.z),
                        1.0f);
                    if (qFuzzyIsNull(clip.w()) || clip.w() <= 0.0f)
                    {
                        continue;
                    }

                    const QVector3D ndc(clip.x() / clip.w(), clip.y() / clip.w(), clip.z() / clip.w());
                    const float sx = (ndc.x() * 0.5f + 0.5f) * static_cast<float>(width());
                    const float sy = (1.0f - (ndc.y() * 0.5f + 0.5f)) * static_cast<float>(height());
                    const float dx = sx - static_cast<float>(currentPos.x());
                    const float dy = sy - static_cast<float>(currentPos.y());
                    const float d2 = dx * dx + dy * dy;
                    if (d2 > r2)
                    {
                        continue;
                    }

                    const float depth = ndc.z() * 0.5f + 0.5f;
                    if (depth < bestDepth)
                    {
                        bestDepth = depth;
                        hoverThing = static_cast<std::int32_t>(i);
                        picked = true;
                    }
                }
            }
        }

        if (hoverSector != hoverPickSector_
            || hoverSurface != hoverPickSurface_
            || hoverThing != hoverPickThing_
            || hoverLight != hoverPickLight_)
        {
            hoverPickSector_ = hoverSector;
            hoverPickSurface_ = hoverSurface;
            hoverPickThing_ = hoverThing;
            hoverPickLight_ = hoverLight;
            Q_EMIT hoverPicked(hoverSector, hoverSurface, hoverThing, hoverLight);
        }
    }
}

void GlView::mouseReleaseEvent(QMouseEvent* event)
{
    event->accept();
    if (viewportMouseGesture_.button == Qt::NoButton
        || event->button() != viewportMouseGesture_.button)
    {
        return;
    }
    const Qt::MouseButton semanticButton = viewportMouseSemanticButton();
    const Qt::KeyboardModifiers semanticModifiers =
        viewportMouseSemanticModifiers(event->modifiers());
    viewportMouseGesture_ = {};
    QMouseEvent routedEvent(event->type(), event->position(), event->globalPosition(),
        semanticButton, Qt::NoButton, semanticModifiers, event->pointingDevice());
    event = &routedEvent;

    if (event->button() == Qt::MiddleButton)
    {
        selectionOrbitDragRejected_ = false;
        selectionOrbitDragPivot_.reset();
        event->accept();
        return;
    }
    if (event->button() == Qt::RightButton)
    {
        stopFlyNavigation();
        if (viewportPickingEnabled_)
        {
            Q_EMIT placementPreviewMouseMoved(
                mouseEventPos(*event), Qt::NoModifier);
        }
        event->accept();
        return;
    }

    if (event->button() != Qt::LeftButton)
    {
        QOpenGLWidget::mouseReleaseEvent(event);
        return;
    }

    if (suppressNextArchitecturalLightingVertexRelease_)
    {
        // Qt sends this release after MouseButtonDblClick. The double-click
        // handler has already published the editor request; re-entering the
        // ARCH click path here would replace a multi-vertex selection and the
        // MainWindow selection-change callback would close the new popup.
        suppressNextArchitecturalLightingVertexRelease_ = false;
        leftDragged_ = false;
        boxSelectActive_ = false;
        boxSelectVisible_ = false;
        boxSelectFromTransformTool_ = false;
        update();
        event->accept();
        return;
    }

    if (axisWidgetPressed_)
    {
        axisWidgetPressed_ = false;
        axisWidgetDragging_ = false;
        event->accept();
        return;
    }

    if (sectorThrustDrag_.active)
    {
        endSectorThrustDrag();
        event->accept();
        return;
    }

    if (transformDrag_.active)
    {
        const TransformTargetKind target = transformDrag_.target;
        const std::int32_t lightOwner = transformDrag_.lightOwner;
        const render::LightGizmoKind lightKind = transformDrag_.lightKind;
        const bool stationaryLightHandleClick =
            lightOwner >= 0
            && (mouseEventPos(*event) - transformDrag_.startMouse).manhattanLength() <= 2
            && (target == TransformTargetKind::Light
                || target == TransformTargetKind::ThingLight
                || target == TransformTargetKind::SectorLight);
        endTransformDrag();
        if (stationaryLightHandleClick)
        {
            // A selected light's Move/Scale handle overlaps its source gizmo.
            // A stationary click used to be consumed as a zero-distance drag,
            // leaving MainWindow without the first click needed to route the
            // following double-click. Record the owning source without changing
            // selection or committing an edit.
            selectedLightGizmoKind_ = lightKind;
            setProperty(
                "sed.selectedLightGizmoKind",
                static_cast<int>(selectedLightGizmoKind_));
            if (target == TransformTargetKind::Light)
            {
                Q_EMIT clickPicked(-1, -1, -1, lightOwner);
            }
            else if (target == TransformTargetKind::ThingLight)
            {
                Q_EMIT clickPicked(-1, -1, lightOwner, -1);
            }
            else
            {
                Q_EMIT clickPicked(lightOwner, -1, -1, -1);
            }
        }
        event->accept();
        return;
    }

    if (!viewportPickingEnabled_)
    {
        previewInteractionDragging_ = false;
        event->accept();
        return;
    }

    const QPoint releasePos = mouseEventPos(*event);
    const QPoint dragDelta = releasePos - leftPressPos_;
    const bool selectStagedCleaveOriginOnClick =
        stagedCleaveEndpointOriginClickCandidate_;
    stagedCleaveEndpointOriginClickCandidate_ = false;
    const bool stagedCleavePointerWasDragged =
        render::cleavePointerMotionIsDrag(dragDelta);
    if ( stagedCleaveShapePlacementClickPending_ )
    {
        stagedCleaveShapePlacementClickPending_ = false;
        leftDragged_ = false;
        if ( !stagedCleavePointerWasDragged
            && cleaveStagedMode_
            && cleaveShapeOptions_.placementArmed )
        {
            stagedCleaveShapePreviewFrame_ =
                stagedCleaveShapePlacementFrameAt(
                    releasePos,
                    event->modifiers());
            stagedCleaveShapePreviewCenter_ =
                stagedCleaveShapePreviewFrame_
                ? std::optional<QVector3D>(render::cleaveRenderPoint(
                    stagedCleaveShapePreviewFrame_->center))
                : std::nullopt;
            if ( stagedCleaveShapePreviewFrame_ )
            {
                (void)appendStagedCleaveShape(
                    *stagedCleaveShapePreviewFrame_);
            }
            else
            {
                showToast(
                    "Add Shape: click a visible target surface",
                    2400,
                    ToastPlacement::BottomCenter,
                    ToastGroup::Error);
            }
        }
        update();
        event->accept();
        return;
    }
    if ( stagedCleaveMarqueeSelection_.active )
    {
        StagedCleaveMarqueeSelection marquee =
            std::move(stagedCleaveMarqueeSelection_);
        stagedCleaveMarqueeSelection_ = {};
        leftDragged_ = false;
        const bool contextStillMatches = stagedCleaveContext_.valid
            && stagedCleaveTargetsMatchContext(stagedCleaveContext_);
        if ( stagedCleavePointerWasDragged && contextStillMatches )
        {
            QMatrix4x4 projection{};
            QMatrix4x4 cameraView{};
            QVector3D eye{};
            QVector3D target{};
            float nearPlane{};
            float farPlane{};
            buildCameraMatrices(
                projection, cameraView, eye, target, nearPlane, farPlane);
            const QMatrix4x4 mvp = projection * cameraView;
            const QRectF area(
                QPointF(marquee.startPosition),
                QPointF(releasePos));
            const QRectF bounds = area.normalized();
            const auto segmentIntersectsBounds = [&bounds](
                const QPointF& first,
                const QPointF& second)
            {
                if ( bounds.contains(first) || bounds.contains(second) )
                {
                    return true;
                }

                const double dx = second.x() - first.x();
                const double dy = second.y() - first.y();
                double enter = 0.0;
                double leave = 1.0;
                const auto clip = [&enter, &leave](
                    const double denominator,
                    const double numerator)
                {
                    if ( denominator == 0.0 )
                    {
                        return numerator >= 0.0;
                    }
                    const double parameter = numerator / denominator;
                    if ( denominator < 0.0 )
                    {
                        if ( parameter > leave )
                        {
                            return false;
                        }
                        enter = std::max(enter, parameter);
                    }
                    else
                    {
                        if ( parameter < enter )
                        {
                            return false;
                        }
                        leave = std::min(leave, parameter);
                    }
                    return true;
                };
                return clip(-dx, first.x() - bounds.left())
                    && clip(dx, bounds.right() - first.x())
                    && clip(-dy, first.y() - bounds.top())
                    && clip(dy, bounds.bottom() - first.y())
                    && enter <= leave;
            };

            std::vector<std::int32_t> next =
                std::move(marquee.baselineSelection);
            for ( std::size_t index = 0;
                index < stagedCleaveLines_.size();
                ++index )
            {
                const StagedCleaveLine& line = stagedCleaveLines_[index];
                const ProjectedPoint first = projectWorldToWidget(
                    mvp, line.first, width(), height());
                const ProjectedPoint second = projectWorldToWidget(
                    mvp, line.second, width(), height());
                if ( !first.valid || !second.valid
                    || !segmentIntersectsBounds(first.pos, second.pos) )
                {
                    continue;
                }
                const std::int32_t lineIndex = static_cast<std::int32_t>(index);
                if ( marquee.mode == SelectionBoxMode::Remove )
                {
                    next.erase(
                        std::remove(next.begin(), next.end(), lineIndex),
                        next.end());
                }
                else
                {
                    next.push_back(lineIndex);
                }
            }
            const std::int32_t activeLine =
                std::find(next.begin(), next.end(), selectedStagedCleaveLine_)
                    != next.end()
                ? selectedStagedCleaveLine_
                : next.empty() ? -1 : next.back();
            setSelectedStagedCleaveLines(std::move(next), activeLine);
        }
        else
        {
            update();
        }
        event->accept();
        return;
    }
    if (stagedCleaveJointCycleClickCandidate_
        && stagedCleavePressModifiers_ == Qt::AltModifier
        && !stagedCleavePointerWasDragged)
    {
        const auto current = std::find(
            stagedCleaveJointCycleCandidates_.begin(),
            stagedCleaveJointCycleCandidates_.end(),
            stagedCleaveJointCycleActiveBefore_);
        const std::int32_t nextLine = current
                == stagedCleaveJointCycleCandidates_.end()
            ? stagedCleaveJointCycleCandidates_.front()
            : stagedCleaveJointCycleCandidates_[
                (static_cast<std::size_t>(current
                    - stagedCleaveJointCycleCandidates_.begin())
                    + 1u)
                % stagedCleaveJointCycleCandidates_.size()];
        setSelectedStagedCleaveLines({nextLine}, nextLine);
        discardStagedCleaveHistoryGesture();
        Q_EMIT viewportStateChanged();
        stagedCleaveJointCycleClickCandidate_ = false;
        stagedCleaveJointCycleCandidates_.clear();
        stagedCleaveLineClickCandidate_ = false;
        stagedCleaveDragSelectionApplied_ = false;
        stagedCleavePlaneDrag_ = {};
        editingStagedCleaveLine_ = -1;
        editingStagedCleaveEndpoint_ = -1;
        editingDisjointStagedCleaveEndpoint_ = false;
        editingStagedCleaveEndpointPreservePlacement_ = false;
        sectorCleaveHaveFirstPoint_ = false;
        surfaceCleaveHaveFirstPoint_ = false;
        leftDragged_ = false;
        unsetCursor();
        update();
        event->accept();
        return;
    }
    if (stagedCleaveLineClickCandidate_
        && stagedCleavePressModifiers_.testFlag(Qt::AltModifier)
        && !stagedCleavePointerWasDragged
        && editingStagedCleaveLine_ >= 0)
    {
        // Alt endpoint clicks select or cycle the shared joint. Topology only
        // changes after the pointer crosses the drag threshold.
        applyStagedCleaveClickSelection(
            editingStagedCleaveLine_, stagedCleavePressModifiers_);
        discardStagedCleaveHistoryGesture();
        Q_EMIT viewportStateChanged();
        stagedCleaveJointCycleClickCandidate_ = false;
        stagedCleaveJointCycleCandidates_.clear();
        stagedCleaveLineClickCandidate_ = false;
        stagedCleaveDragSelectionApplied_ = false;
        editingStagedCleaveLine_ = -1;
        editingStagedCleaveEndpoint_ = -1;
        editingDisjointStagedCleaveEndpoint_ = false;
        editingStagedCleaveEndpointPreservePlacement_ = false;
        sectorCleaveHaveFirstPoint_ = false;
        surfaceCleaveHaveFirstPoint_ = false;
        leftDragged_ = false;
        unsetCursor();
        update();
        event->accept();
        return;
    }
    if (stagedCleaveLineClickCandidate_
        && !stagedCleavePointerWasDragged
        && !selectStagedCleaveOriginOnClick
        && (stagedCleavePressModifiers_.testFlag(Qt::ShiftModifier)
            || stagedCleavePressModifiers_.testFlag(Qt::ControlModifier))
        && editingStagedCleaveLine_ >= 0)
    {
        applyStagedCleaveClickSelection(
            editingStagedCleaveLine_, stagedCleavePressModifiers_);
        discardStagedCleaveHistoryGesture();
        Q_EMIT viewportStateChanged();
        stagedCleaveLineClickCandidate_ = false;
        stagedCleaveDragSelectionApplied_ = false;
        editingStagedCleaveLine_ = -1;
        editingStagedCleaveEndpoint_ = -1;
        editingDisjointStagedCleaveEndpoint_ = false;
        editingStagedCleaveEndpointPreservePlacement_ = false;
        sectorCleaveHaveFirstPoint_ = false;
        surfaceCleaveHaveFirstPoint_ = false;
        leftDragged_ = false;
        unsetCursor();
        update();
        event->accept();
        return;
    }

    if (!(pickMode_ == PickMode::ArchitecturalLighting
        && renderPipeline_ == RenderPipeline::Editor
        && architecturalLightingEditingActive_))
    {
        architecturalLightingLastOwnerClick_.reset();
    }

    if (pickMode_ == PickMode::ArchitecturalLighting
        && renderPipeline_ == RenderPipeline::Editor
        && architecturalLightingEditingActive_
        && level_)
    {
        // A new ARCH release supersedes the owner remembered by the previous
        // click. The owner branch below republishes it only after the common
        // marker/body visibility checks have succeeded.
        architecturalLightingLastOwnerClick_.reset();
        QMatrix4x4 projection;
        QMatrix4x4 cameraView;
        QVector3D eye{};
        QVector3D target{};
        float nearPlane{};
        float farPlane{};
        buildCameraMatrices(
            projection, cameraView, eye, target, nearPlane, farPlane);
        (void)eye;
        (void)target;
        (void)nearPlane;
        (void)farPlane;
        const QMatrix4x4 mvp = projection * cameraView;
        const bool xray = xraySelectionCyclingEnabled_
            || event->modifiers().testFlag(Qt::AltModifier);

        // ARCH release handling can be entered from a Qt event while another
        // QOpenGLWidget (or no widget) owns the current context. Every GPU
        // pick in this branch must use this view's resources, then restore the
        // context that was current when the event arrived.
        struct ArchitecturalPickContextGuard
        {
            GlView& view;
            QPointer<QOpenGLContext> previousContext{};
            GLint previousDrawFramebuffer{};
            GLint previousReadFramebuffer{};
            GLint previousViewport[4]{ 0, 0, 0, 0 };
            bool callerFramebufferCaptured{};
            bool acquired{};

            explicit ArchitecturalPickContextGuard(GlView& v)
                : view(v),
                  previousContext(QOpenGLContext::currentContext())
            {
                if (previousContext.data() == view.context())
                {
                    if (QOpenGLContext* const current =
                            QOpenGLContext::currentContext())
                    {
                        QOpenGLFunctions* const functions = current->functions();
                        functions->glGetIntegerv(
                            GL_DRAW_FRAMEBUFFER_BINDING,
                            &previousDrawFramebuffer);
                        functions->glGetIntegerv(
                            GL_READ_FRAMEBUFFER_BINDING,
                            &previousReadFramebuffer);
                        functions->glGetIntegerv(
                            GL_VIEWPORT,
                            previousViewport);
                        callerFramebufferCaptured = true;
                    }
                    return;
                }

                view.makeCurrent();
                acquired = true;
            }

            ~ArchitecturalPickContextGuard()
            {
                release();
            }

            void release() noexcept
            {
                restoreCallerFramebuffer();
                if (!acquired)
                {
                    return;
                }
                acquired = false;
                view.doneCurrent();
                if (previousContext)
                {
                    if (QSurface* surface = previousContext->surface())
                    {
                        previousContext->makeCurrent(surface);
                    }
                }
            }

            void restoreCallerFramebuffer() noexcept
            {
                if (!callerFramebufferCaptured
                    || QOpenGLContext::currentContext() != view.context())
                {
                    return;
                }

                QOpenGLFunctions* const functions =
                    QOpenGLContext::currentContext()->functions();
                functions->glBindFramebuffer(
                    GL_DRAW_FRAMEBUFFER,
                    static_cast<GLuint>(previousDrawFramebuffer));
                functions->glBindFramebuffer(
                    GL_READ_FRAMEBUFFER,
                    static_cast<GLuint>(previousReadFramebuffer));
                functions->glViewport(
                    previousViewport[0],
                    previousViewport[1],
                    previousViewport[2],
                    previousViewport[3]);
                callerFramebufferCaptured = false;
            }

            bool ready() const noexcept
            {
                return view.context() != nullptr
                    && QOpenGLContext::currentContext() == view.context();
            }
        } pickContext(*this);

        struct OwnerCandidate
        {
            std::int32_t thingIndex{ -1 };
            QPointF screen{};
            float distanceSquared{ std::numeric_limits<float>::infinity() };
            float depth{ std::numeric_limits<float>::infinity() };
        };
        std::vector<OwnerCandidate> owners{};

        const QRect normalizedBox = QRect(leftPressPos_, releasePos).normalized();
        const bool directClick = dragDelta.manhattanLength() <= 4;

        const auto hasResolvableModel = [this](const Thing& candidate)
        {
            if (!level_)
            {
                return false;
            }
            const std::string modelKey = thing::thingModel3doName(
                *level_,
                templates_,
                candidate);
            if (modelKey.empty())
            {
                return false;
            }
            std::string cacheKey = modelKey;
            if (!sed::text::iendsWithAscii(cacheKey, ".3do"))
            {
                cacheKey += ".3do";
            }
            const auto found = models3do_.find(cacheKey);
            return found != models3do_.end()
                && found->second
                && !found->second->meshes.empty();
        };

        const auto archBodyOwner = [this, &hasResolvableModel](
                                        const std::size_t thingIndex) -> bool
        {
            if (!level_ || thingIndex >= level_->things.size())
            {
                return false;
            }
            const Thing& candidate = level_->things[thingIndex];
            const std::int32_t flags = sed::level::thingFlags(
                *level_,
                templates_,
                candidate);
            return isLayerVisible(candidate.layer)
                && !isLayerLocked(candidate.layer)
                && (flags & 0x00000008) != 0
                && (flags & TF_INVISIBLE) == 0
                && std::isfinite(candidate.x)
                && std::isfinite(candidate.y)
                && std::isfinite(candidate.z)
                && hasResolvableModel(candidate);
        };

        const auto archOwner = [this](const std::size_t thingIndex) -> bool
        {
            if (!level_ || thingIndex >= level_->things.size())
            {
                return false;
            }
            const Thing& candidate = level_->things[thingIndex];
            return isLayerVisible(candidate.layer)
                && !isLayerLocked(candidate.layer)
                && (sed::level::thingFlags(*level_, templates_, candidate)
                    & 0x00000008) != 0
                && std::isfinite(candidate.x)
                && std::isfinite(candidate.y)
                && std::isfinite(candidate.z);
        };

        const auto architecturalSelectionMode =
            [this](const Qt::KeyboardModifiers modifiers)
        {
            const Qt::KeyboardModifiers held = modifiers
                & (Qt::ShiftModifier | Qt::ControlModifier
                    | Qt::AltModifier | Qt::MetaModifier);
            const auto modifierActive = [held](
                const ui::shortcuts::ModifierBindings& required)
            {
                return ui::shortcuts::modifierBindingsActive(held, required);
            };
            if (modifierActive(selectionRemoveModifiers_))
            {
                return SelectionBoxMode::Remove;
            }
            if (modifierActive(selectionAddModifiers_))
            {
                return SelectionBoxMode::Add;
            }
            return SelectionBoxMode::Replace;
        };

        const auto ownerVertexIdentities =
            [this](const std::int32_t thingIndex)
        {
            std::vector<ArchitecturalLightingVertexIdentity> identities{};
            const std::span<const ArchitecturalLightingRenderVertex> vertices =
                architecturalLightingVerticesForThing(thingIndex);
            identities.reserve(vertices.size());
            for (const ArchitecturalLightingRenderVertex& vertex : vertices)
            {
                identities.push_back(architecturalLightingVertexIdentity(vertex));
            }
            return identities;
        };

        struct OwnerFootprint
        {
            QPointF center{};
            std::array<QPointF, 4> corners{};
            QRectF bounds{};
            float depth{ std::numeric_limits<float>::infinity() };
        };

        const auto projectOwnerFootprint =
            [this, &mvp](const QVector3D& center)
                -> std::optional<OwnerFootprint>
        {
            QVector3D right = architecturalLightingOwnerGizmoCameraRight_;
            QVector3D up = architecturalLightingOwnerGizmoCameraUp_;
            const float halfSize = architecturalLightingOwnerGizmoHalfSize_;
            if (right.lengthSquared() <= sed::math::kTinyEpsilonF
                || up.lengthSquared() <= sed::math::kTinyEpsilonF
                || !std::isfinite(halfSize)
                || halfSize <= sed::math::kTinyEpsilonF)
            {
                return std::nullopt;
            }
            right.normalize();
            up.normalize();
            right *= halfSize;
            up *= halfSize;

            const std::array<QVector3D, 4> worldCorners{
                center - right - up,
                center + right - up,
                center + right + up,
                center - right + up };
            OwnerFootprint result{};
            result.bounds = QRectF{};
            bool haveBounds = false;
            for (std::size_t index = 0; index < worldCorners.size(); ++index)
            {
                const QVector4D clip =
                    mvp * QVector4D(worldCorners[index], 1.0f);
                if (!std::isfinite(clip.x()) || !std::isfinite(clip.y())
                    || !std::isfinite(clip.z()) || !std::isfinite(clip.w())
                    || clip.w() <= std::numeric_limits<float>::epsilon())
                {
                    return std::nullopt;
                }
                const QVector3D ndc = clip.toVector3D() / clip.w();
                if (!std::isfinite(ndc.x()) || !std::isfinite(ndc.y())
                    || !std::isfinite(ndc.z()) || ndc.z() < -1.0f
                    || ndc.z() > 1.0f)
                {
                    return std::nullopt;
                }
                result.corners[index] = QPointF(
                    (static_cast<double>(ndc.x()) * 0.5 + 0.5)
                        * static_cast<double>(width()),
                    (1.0 - (static_cast<double>(ndc.y()) * 0.5 + 0.5))
                        * static_cast<double>(height()));
                if (!haveBounds)
                {
                    result.bounds = QRectF(
                        result.corners[index],
                        QSizeF(0.0, 0.0));
                    haveBounds = true;
                }
                else
                {
                    result.bounds = result.bounds.united(
                        QRectF(result.corners[index], QSizeF(0.0, 0.0)));
                }
            }

            const QVector4D centerClip = mvp * QVector4D(center, 1.0f);
            if (!std::isfinite(centerClip.x())
                || !std::isfinite(centerClip.y())
                || !std::isfinite(centerClip.z())
                || !std::isfinite(centerClip.w())
                || centerClip.w() <= std::numeric_limits<float>::epsilon())
            {
                return std::nullopt;
            }
            const QVector3D centerNdc =
                centerClip.toVector3D() / centerClip.w();
            if (!std::isfinite(centerNdc.x())
                || !std::isfinite(centerNdc.y())
                || !std::isfinite(centerNdc.z())
                || centerNdc.z() < -1.0f || centerNdc.z() > 1.0f)
            {
                return std::nullopt;
            }
            result.center = QPointF(
                (static_cast<double>(centerNdc.x()) * 0.5 + 0.5)
                    * static_cast<double>(width()),
                (1.0 - (static_cast<double>(centerNdc.y()) * 0.5 + 0.5))
                    * static_cast<double>(height()));
            result.depth = centerNdc.z() * 0.5f + 0.5f;
            return result;
        };

        const auto pointInOwnerFootprint =
            [](const QPointF& point,
               const std::array<QPointF, 4>& corners)
        {
            bool positive = false;
            bool negative = false;
            for (std::size_t index = 0; index < corners.size(); ++index)
            {
                const QPointF& a = corners[index];
                const QPointF& b = corners[(index + 1) % corners.size()];
                const QPointF edge = b - a;
                const QPointF relative = point - a;
                const double cross =
                    edge.x() * relative.y() - edge.y() * relative.x();
                if (cross > 1.0e-6)
                {
                    positive = true;
                }
                else if (cross < -1.0e-6)
                {
                    negative = true;
                }
            }
            return !(positive && negative);
        };

        // A Shaded ARCH owner can be selected from its actual rendered body.
        // Reuse the normal Thing pick pass so a projected bounding box cannot
        // select empty space, and compare the hit against the same scene-depth
        // readback used by ordinary selection.  The marker path below remains
        // the fast exact-anchor path for a click on the visible icon.
        std::optional<OwnerCandidate> bodyOwner{};
        if (directClick
            && showThings_
            && thingViewMode_ == ThingViewMode::Shaded
            && pickContext.ready())
        {
            constexpr int kMaxArchitecturalBodyPeels = 16;
            const std::vector<PickHit> bodyHits = pickPeel(
                mvp,
                releasePos,
                TriKind::Thing,
                0,
                false,
                kMaxArchitecturalBodyPeels,
                false);
            std::optional<float> sceneDepth{};
            if (!xray)
            {
                const std::vector<float> sceneDepths = pickSceneDepthsAtPoints(
                    mvp,
                    std::vector<QPointF>{ QPointF(releasePos) },
                    true,
                    false,
                    render::picking::solidLevelOccludersCullBackFaces(
                        render::picking::SolidLevelOcclusionPickFamily::Thing,
                        false));
                if (!sceneDepths.empty() && std::isfinite(sceneDepths.front()))
                {
                    sceneDepth = sceneDepths.front();
                }
            }
            for (const PickHit& hit : bodyHits)
            {
                if (hit.id >= pickObjects_.size())
                {
                    continue;
                }
                const PickObject& object = pickObjects_[hit.id];
                if (object.kind != PickKind::Thing || object.index < 0
                    || !archBodyOwner(static_cast<std::size_t>(object.index)))
                {
                    continue;
                }

                bool visible = xray;
                if (!visible)
                {
                    visible = sceneDepth.has_value()
                        && render::picking::surfaceSelectionCandidatePassesSceneDepth(
                            hit.depth,
                            *sceneDepth,
                            false,
                            false);
                }
                if (visible)
                {
                    bodyOwner = OwnerCandidate{
                        object.index,
                        QPointF(releasePos),
                        0.0f,
                        hit.depth };
                    break;
                }
            }
        }

        for (std::size_t thingIndex = 0;
             thingIndex < level_->things.size();
             ++thingIndex)
        {
            const Thing& thing = level_->things[thingIndex];
            if (!archOwner(thingIndex))
            {
                continue;
            }

            // Marker picking must use the same dedicated entry that was
            // uploaded by drawArchitecturalLightingOwnerGizmos(). If no
            // marker was published, do not fall back to the hidden Thing
            // origin or a stale selected-model bounds approximation.
            const auto archEntry = architecturalLightingOwnerGizmoEntryByThing_.find(
                static_cast<std::int32_t>(thingIndex));
            if (archEntry == architecturalLightingOwnerGizmoEntryByThing_.end()
                || archEntry->second >= architecturalLightingOwnerGizmoEntries_.size())
            {
                continue;
            }
            // The published entry center is already CPU-transformed to match
            // the previewed ARCH marker uploaded for rendering. Applying the
            // preview matrix here again would move the hit target away from
            // the visible marker during transforms.
            const QVector3D center =
                architecturalLightingOwnerGizmoEntries_[archEntry->second].center;

            const std::optional<OwnerFootprint> footprint =
                projectOwnerFootprint(center);
            if (!footprint)
            {
                continue;
            }
            const double dx = footprint->center.x() - releasePos.x();
            const double dy = footprint->center.y() - releasePos.y();
            const float distanceSquared = static_cast<float>(dx * dx + dy * dy);
            const QRectF clickBounds = footprint->bounds.adjusted(
                -2.0, -2.0, 2.0, 2.0);
            const bool insideFootprint = pointInOwnerFootprint(
                QPointF(releasePos),
                footprint->corners);
            if (directClick && !insideFootprint
                && !clickBounds.contains(QPointF(releasePos)))
            {
                continue;
            }
            if (!directClick
                && !normalizedBox.intersects(footprint->bounds.toAlignedRect()))
            {
                continue;
            }
            owners.push_back(OwnerCandidate{
                static_cast<std::int32_t>(thingIndex),
                footprint->center,
                distanceSquared,
                footprint->depth });
        }

        if (bodyOwner)
        {
            owners.push_back(*bodyOwner);
        }

        if (!xray && !owners.empty() && pickContext.ready())
        {
            std::vector<QPointF> samples{};
            samples.reserve(owners.size());
            for (const OwnerCandidate& owner : owners)
            {
                samples.push_back(owner.screen);
            }
            const std::vector<float> sceneDepths = pickSceneDepthsAtPoints(
                mvp,
                samples,
                true,
                false,
                render::picking::solidLevelOccludersCullBackFaces(
                    render::picking::SolidLevelOcclusionPickFamily::Thing,
                    false));
            std::vector<OwnerCandidate> visible{};
            visible.reserve(owners.size());
            for (std::size_t i = 0; i < owners.size(); ++i)
            {
                if (i < sceneDepths.size()
                    && render::picking::surfaceSelectionCandidatePassesSceneDepth(
                        owners[i].depth,
                        sceneDepths[i],
                        false,
                        false))
                {
                    visible.push_back(owners[i]);
                }
            }
            owners = std::move(visible);
        }

        if (!directClick)
        {
            // Unified ARCH marquee selection carries both entity ownership and
            // exact cached vertices. A box that touches an owner marker/body
            // expands that owner to all authored vertices; a box containing
            // only already-published vertex markers edits the vertex set
            // without clearing the existing owner selection.
            const std::vector<ArchitecturalLightingVertexIdentity> markerVertices =
                pickContext.ready()
                ? architecturalLightingVerticesInScreenRect(
                      normalizedBox,
                      mvp,
                      xray)
                : std::vector<ArchitecturalLightingVertexIdentity>{};
            editor::Selection boxSelection{};
            Qt::KeyboardModifiers boxMods = event->modifiers();
            if (boxSelectFromTransformTool_)
            {
                boxMods &= ~Qt::AltModifier;
            }
            constexpr Qt::KeyboardModifiers kSelectionModMask =
                Qt::ShiftModifier | Qt::ControlModifier
                | Qt::AltModifier | Qt::MetaModifier;
            const Qt::KeyboardModifiers heldMods = boxMods & kSelectionModMask;
            const auto modifierActive = [heldMods](
                const ui::shortcuts::ModifierBindings& required)
            {
                return ui::shortcuts::modifierBindingsActive(heldMods, required);
            };
            SelectionBoxMode boxMode = SelectionBoxMode::Replace;
            if (modifierActive(selectionRemoveModifiers_))
            {
                boxMode = SelectionBoxMode::Remove;
            }
            else if (modifierActive(selectionAddModifiers_))
            {
                boxMode = SelectionBoxMode::Add;
            }

            for (const OwnerCandidate& owner : owners)
            {
                boxSelection.things.insert(owner.thingIndex);
            }
            if (showThings_ && thingViewMode_ == ThingViewMode::Shaded
                && pickContext.ready())
            {
                const std::unordered_set<std::int32_t> bodyOwners =
                    pickThingsInScreenRect(
                        mvp,
                        normalizedBox.left(),
                        normalizedBox.top(),
                        normalizedBox.right(),
                        normalizedBox.bottom(),
                        !xray);
                for (const std::int32_t owner : bodyOwners)
                {
                    if (owner >= 0
                        && archBodyOwner(static_cast<std::size_t>(owner)))
                    {
                        boxSelection.things.insert(owner);
                    }
                }
            }

            // A selected owner's visible vertex markers have priority over
            // its shaded body for a marquee.  The body often surrounds every
            // marker, but expanding it here would make it impossible to
            // retain a partial vertex selection.  New owners, or an owner
            // whose box contains no exact marker, still expand to the full
            // cached model as expected from an entity marquee.
            std::unordered_map<
                std::int32_t,
                std::vector<ArchitecturalLightingVertexIdentity>>
                markerVerticesByOwner{};
            markerVerticesByOwner.reserve(markerVertices.size());
            for (const ArchitecturalLightingVertexIdentity& identity
                 : markerVertices)
            {
                markerVerticesByOwner[identity.thingIndex].push_back(identity);
            }

            const auto ownerAlreadySelected = [this](const std::int32_t owner)
            {
                return highlightedSelection_.things.contains(owner)
                    || selectedThing_ == owner
                    || architecturalLightingSelectedOwnerCacheByThing_.contains(
                        owner);
            };
            std::unordered_set<std::int32_t> preservedOwners{};
            preservedOwners.reserve(markerVerticesByOwner.size());
            for (const auto& [owner, identities] : markerVerticesByOwner)
            {
                (void)identities;
                if (ownerAlreadySelected(owner))
                {
                    preservedOwners.insert(owner);
                }
            }
            if (!markerVertices.empty())
            {
                // Once any exact marker is inside the box, an already
                // selected owner's intersected body is context for the
                // vertex operation, not another request to expand that
                // owner's full model.
                for (const std::int32_t owner : boxSelection.things)
                {
                    if (ownerAlreadySelected(owner))
                    {
                        preservedOwners.insert(owner);
                    }
                }
            }

            std::vector<ArchitecturalLightingVertexIdentity>
                ownerVerticesToApply{};
            for (const std::int32_t owner : boxSelection.things)
            {
                const auto markerIt = markerVerticesByOwner.find(owner);
                if (preservedOwners.contains(owner))
                {
                    if (markerIt != markerVerticesByOwner.end())
                    {
                        ownerVerticesToApply.insert(
                            ownerVerticesToApply.end(),
                            markerIt->second.begin(),
                            markerIt->second.end());
                    }
                    continue;
                }

                const std::vector<ArchitecturalLightingVertexIdentity> cached =
                    ownerVertexIdentities(owner);
                ownerVerticesToApply.insert(
                    ownerVerticesToApply.end(),
                    cached.begin(),
                    cached.end());
            }

            // Exact-marker owners are a vertex-only target for this box. Do
            // not send them through the generic entity operation in any
            // modifier mode: Remove would discard the owner and Replace
            // would rebuild its cache before the precise vertex operation.
            // A Replace box that also targets a different owner preserves the
            // current entity set while adding that new owner.
            editor::Selection genericBoxSelection{};
            std::unordered_set<std::int32_t> unpreservedOwners{};
            for (const std::int32_t owner : boxSelection.things)
            {
                if (preservedOwners.contains(owner))
                {
                    continue;
                }
                unpreservedOwners.insert(owner);
                genericBoxSelection.things.insert(owner);
            }
            if (boxMode == SelectionBoxMode::Replace
                && !unpreservedOwners.empty()
                && !preservedOwners.empty())
            {
                for (const std::int32_t owner : highlightedSelection_.things)
                {
                    genericBoxSelection.things.insert(owner);
                }
                if (selectedThing_ >= 0)
                {
                    genericBoxSelection.things.insert(selectedThing_);
                }
            }

            const bool hadOwnerSelection =
                !genericBoxSelection.things.empty();
            pickContext.release();
            if (hadOwnerSelection)
            {
                Q_EMIT selectionBoxPicked(
                    std::move(genericBoxSelection),
                    boxMode);
            }

            std::vector<ArchitecturalLightingVertexIdentity> selectedVertices =
                std::move(ownerVerticesToApply);
            if (boxMode != SelectionBoxMode::Remove)
            {
                // A newly targeted owner had no published vertex cache when
                // the box was scanned. Its generic selection callback above
                // has now published that cache, so include the full owner in
                // the vertex selection before applying Replace/Add.
                for (const std::int32_t owner : unpreservedOwners)
                {
                    const std::vector<ArchitecturalLightingVertexIdentity>
                        cached = ownerVertexIdentities(owner);
                    selectedVertices.insert(
                        selectedVertices.end(),
                        cached.begin(),
                        cached.end());
                }
            }
            for (const ArchitecturalLightingVertexIdentity& identity
                 : markerVertices)
            {
                if (!boxSelection.things.contains(identity.thingIndex))
                {
                    selectedVertices.push_back(identity);
                }
            }
            if (!selectedVertices.empty() || hadOwnerSelection)
            {
                applyArchitecturalLightingVertexSelection(
                    selectedVertices,
                    boxMode,
                    boxMode != SelectionBoxMode::Remove
                        && !unpreservedOwners.empty());
            }
            leftDragged_ = false;
            boxSelectActive_ = false;
            boxSelectVisible_ = false;
            boxSelectFromTransformTool_ = false;
            update();
            event->accept();
            return;
        }

        // The remembered Owner/Vertices choice is a compatibility setting,
        // not a routing gate. A direct ARCH click always gives an exact
        // visible vertex the first chance, then falls back to the displayed
        // owner/body.
        const std::optional<ArchitecturalLightingRenderVertex> vertex =
            directClick && pickContext.ready()
            ? architecturalLightingVertexAt(releasePos, mvp, xray)
            : std::nullopt;
        pickContext.release();
        if (vertex)
        {
            architecturalLightingLastOwnerClick_.reset();
            hoveredArchitecturalLightingVertex_ = vertex;
            Q_EMIT clickPicked(-1, -1, vertex->thingIndex, -1);
            // Vertex picks already have a dedicated signal and selection
            // publication below. Do not send a generic Thing selection first:
            // that callback can replace a multi-owner selection before the
            // exact vertex modifier operation runs.
            Q_EMIT architecturalLightingVertexPicked(
                vertex->thingIndex,
                vertex->lodIndex,
                vertex->meshIndex,
                vertex->vertexIndex,
                event->modifiers());
            applyArchitecturalLightingVertexClick(
                architecturalLightingVertexIdentity(*vertex),
                event->modifiers());
        }
        else if (!owners.empty())
        {
            const auto bestOwner = std::min_element(
                owners.begin(),
                owners.end(),
                [](const OwnerCandidate& lhs, const OwnerCandidate& rhs)
                {
                    if (lhs.distanceSquared != rhs.distanceSquared)
                    {
                        return lhs.distanceSquared < rhs.distanceSquared;
                    }
                    if (lhs.depth != rhs.depth)
                    {
                        return lhs.depth < rhs.depth;
                    }
                    return lhs.thingIndex < rhs.thingIndex;
                });
            const std::vector<ArchitecturalLightingVertexIdentity>
                ownerVerticesBefore =
                    ownerVertexIdentities(bestOwner->thingIndex);
            const SelectionBoxMode ownerMode =
                architecturalSelectionMode(event->modifiers());
            const bool ownerWasSelected =
                highlightedSelection_.things.contains(bestOwner->thingIndex)
                || selectedThing_ == bestOwner->thingIndex;
            const bool preserveOwnerSelection =
                ownerMode == SelectionBoxMode::Replace && ownerWasSelected;
            const std::int32_t resolvedOwner = bestOwner->thingIndex;
            hoveredArchitecturalLightingVertex_.reset();
            Q_EMIT clickPicked(-1, -1, bestOwner->thingIndex, -1);

            if (!preserveOwnerSelection)
            {
                Q_EMIT selectionPicked(
                    -1,
                    -1,
                    bestOwner->thingIndex,
                    -1,
                    event->modifiers());
            }

            const SelectionBoxMode effectiveOwnerMode =
                preserveOwnerSelection
                ? SelectionBoxMode::Add
                : ownerMode;
            std::vector<ArchitecturalLightingVertexIdentity> ownerVertices =
                effectiveOwnerMode == SelectionBoxMode::Remove
                ? ownerVerticesBefore
                : ownerVertexIdentities(bestOwner->thingIndex);
            if (ownerVertices.empty())
            {
                ownerVertices = ownerVerticesBefore;
            }
            applyArchitecturalLightingVertexSelection(
                ownerVertices,
                effectiveOwnerMode,
                effectiveOwnerMode != SelectionBoxMode::Remove);

            // MainWindow selection/cache callbacks may change the legacy
            // target mode or invalidate the selected-owner table. Publish
            // the double-click handoff only after those callbacks complete,
            // and only when the same bound, visible owner still has a cache.
            architecturalLightingLastOwnerClick_.reset();
            if (level_ && resolvedOwner >= 0
                && static_cast<std::size_t>(resolvedOwner)
                    < level_->things.size())
            {
                const Thing& resolvedThing = level_->things[
                    static_cast<std::size_t>(resolvedOwner)];
                const bool activeOwner =
                    level::isMotsArchThing(
                        *level_, resolvedThing, templates_)
                    && (resolvedThing.archLightingIndex == -1
                        || (resolvedThing.archLightingIndex >= 0
                            && static_cast<std::size_t>(
                                resolvedThing.archLightingIndex)
                                < level_->motsArchLighting.size()));
                const bool selected = highlightedSelection_.things.contains(
                    resolvedOwner)
                    || selectedThing_ == resolvedOwner
                    || architecturalLightingSelectedOwnerCacheByThing_.contains(
                        resolvedOwner);
                if (activeOwner && selected && isLayerVisible(resolvedThing.layer)
                    && !isLayerLocked(resolvedThing.layer)
                    && !architecturalLightingVerticesForThing(resolvedOwner)
                        .empty())
                {
                    architecturalLightingLastOwnerClick_ = resolvedOwner;
                    architecturalLightingLastOwnerClickPosition_ = releasePos;
                }
            }
        }
        else
        {
            architecturalLightingLastOwnerClick_.reset();
            hoveredArchitecturalLightingVertex_.reset();
            Q_EMIT clickPicked(-1, -1, -1, -1);
            // A direct ordinary miss in ARCH mode is a normal Replace
            // selection, so clear the prior owner and exact vertex set. The
            // MainWindow selection callback applies the same configured
            // Replace/Add/Remove modifiers used for owner and vertex hits;
            // Shift/Ctrl misses therefore leave the existing set intact.
            Q_EMIT selectionPicked(
                -1,
                -1,
                -1,
                -1,
                event->modifiers());
        }

        leftDragged_ = false;
        boxSelectActive_ = false;
        boxSelectVisible_ = false;
        boxSelectFromTransformTool_ = false;
        update();
        event->accept();
        return;
    }

    // Project a cached sprite AABB to a screen-space rectangle.  This helper
    // is deliberately owned by the complete release handler so direct clicks
    // and marquee selection use exactly the same fallback, regardless of
    // which nested pick path was taken before it.
    auto projectSpriteBounds = [&](const std::size_t thingIndex,
                                   const QMatrix4x4& mvp,
                                   QRectF& outRect,
                                   float& outDepth) -> bool
    {
        if (thingIndex >= spriteThingBounds_.size())
        {
            return false;
        }
        const ThingBounds& bounds = spriteThingBounds_[thingIndex];
        if (!bounds.valid)
        {
            return false;
        }
        const QVector3D corners[8]{
            { bounds.min.x(), bounds.min.y(), bounds.min.z() },
            { bounds.max.x(), bounds.min.y(), bounds.min.z() },
            { bounds.min.x(), bounds.max.y(), bounds.min.z() },
            { bounds.max.x(), bounds.max.y(), bounds.min.z() },
            { bounds.min.x(), bounds.min.y(), bounds.max.z() },
            { bounds.max.x(), bounds.min.y(), bounds.max.z() },
            { bounds.min.x(), bounds.max.y(), bounds.max.z() },
            { bounds.max.x(), bounds.max.y(), bounds.max.z() },
        };
        float minX = std::numeric_limits<float>::infinity();
        float minY = std::numeric_limits<float>::infinity();
        float maxX = -std::numeric_limits<float>::infinity();
        float maxY = -std::numeric_limits<float>::infinity();
        float minDepth = std::numeric_limits<float>::infinity();
        bool visible = false;
        for (const QVector3D& corner : corners)
        {
            const QVector4D clip = mvp * QVector4D(corner, 1.0f);
            if (!std::isfinite(clip.w()) || clip.w() <= 0.0f)
            {
                continue;
            }
            const QVector3D ndc = clip.toVector3D() / clip.w();
            if (!std::isfinite(ndc.x()) || !std::isfinite(ndc.y()) || !std::isfinite(ndc.z())
                || ndc.z() < -1.0f || ndc.z() > 1.0f)
            {
                continue;
            }
            const float sx = (ndc.x() * 0.5f + 0.5f) * static_cast<float>(width());
            const float sy = (1.0f - (ndc.y() * 0.5f + 0.5f)) * static_cast<float>(height());
            minX = std::min(minX, sx);
            minY = std::min(minY, sy);
            maxX = std::max(maxX, sx);
            maxY = std::max(maxY, sy);
            minDepth = std::min(minDepth, ndc.z() * 0.5f + 0.5f);
            visible = true;
        }
        if (!visible)
        {
            return false;
        }
        outRect = QRectF(QPointF(minX, minY), QPointF(maxX, maxY)).normalized();
        outDepth = minDepth;
        return outRect.isValid();
    };
    if (sectorCleaveToolEnabled_ || surfaceCleaveToolEnabled_)
    {
        updateCleaveSnapGuideVisibility(event->modifiers());
        cleaveSnapMarker_.reset();
    }

    if (placementGestureEnabled_)
    {
        const bool dragged = leftDragged_ || dragDelta.manhattanLength() > 2;
        leftDragged_ = false;
        boxSelectActive_ = false;
        boxSelectVisible_ = false;
        Q_EMIT placementGestureReleased(releasePos, event->modifiers(), dragged);
        event->accept();
        return;
    }

    if ( stagedCleaveTransformDrag_.active && !stagedCleaveWheelSession_.active() )
    {
        const bool dragged = render::cleavePointerMotionIsDrag(
            releasePos - stagedCleaveTransformDrag_.startPosition);
        const bool targetsStillMatch = stagedCleaveContext_.valid
            && stagedCleaveTargetsMatchContext(stagedCleaveContext_);
        stagedCleaveTransformDrag_ = {};
        clearStagedCleaveTransformHud();
        leftDragged_ = false;
        unsetCursor();
        if ( dragged && targetsStillMatch )
        {
            (void)finishStagedCleaveHistoryGesture();
            Q_EMIT viewportStateChanged();
        }
        else if ( dragged )
        {
            (void)restoreAndDiscardStagedCleaveHistoryGesture();
            showToast(
                "Multi-cut: restore the original target selection before editing",
                3000,
                ToastPlacement::BottomCenter,
                ToastGroup::Error);
        }
        else
        {
            // Rotate/Scale preview writes into the live staged lines while the
            // pointer is moving. A release below the drag threshold is still
            // a cancelled preview, so restore the press-time draft before
            // publishing the settled state.
            (void)restoreAndDiscardStagedCleaveHistoryGesture();
        }
        update();
        event->accept();
        return;
    }

    if (stagedCleavePlaneDrag_.active)
    {
        StagedCleavePlaneDrag completedDrag = std::move(stagedCleavePlaneDrag_);
        const bool dragged = render::cleavePointerMotionIsDrag(dragDelta);
        const bool completedLineMove = leftDragged_;
        const bool targetsStillMatch = !cleaveStagedMode_
            || (stagedCleaveContext_.valid
                && stagedCleaveTargetsMatchContext(stagedCleaveContext_));
        stagedCleavePlaneDrag_ = {};
        leftDragged_ = false;
        unsetCursor();
        if ( dragged && !targetsStillMatch )
        {
            (void)restoreAndDiscardStagedCleaveHistoryGesture();
            showToast(
                "Multi-cut: restore the original target selection before editing",
                3000,
                ToastPlacement::BottomCenter,
                ToastGroup::Error);
        }
        else if (dragged && !completedLineMove)
        {
            (void)restoreAndDiscardStagedCleaveHistoryGesture();
        }
        else if (!dragged)
        {
            const bool clickCandidate = stagedCleaveLineClickCandidate_;
            const Qt::KeyboardModifiers clickModifiers = stagedCleavePressModifiers_;
            const bool ctrlEndpointClick = completedDrag.grabbedEndpoint >= 0
                && clickModifiers.testFlag(Qt::ControlModifier);
            const bool restored = restoreAndDiscardStagedCleaveHistoryGesture();
            if (!restored)
            {
                discardStagedCleaveHistoryGesture();
            }
            if (clickCandidate || ctrlEndpointClick)
            {
                applyStagedCleaveClickSelection(
                    completedDrag.line,
                    clickModifiers);
            }
            else if (!restored)
            {
                Q_EMIT viewportStateChanged();
            }
        }
        if ( !dragged && targetsStillMatch
            && completedDrag.restoreAuthoredCameraOnClick
            && completedDrag.line >= 0 )
        {
            (void)restoreStagedCleaveLineCamera(
                static_cast<std::size_t>(completedDrag.line));
        }
        else if ( dragged && targetsStillMatch && completedLineMove
            && sectorCleaveToolEnabled_
            && !cleaveBounded_
            && completedDrag.line >= 0
            && !completedDrag.preserveRigidLinePlacement )
        {
            (void)attachStagedSectorCleaveLineToCameraEdge(
                static_cast<std::size_t>(completedDrag.line));
        }
        if ( dragged && targetsStillMatch && completedLineMove )
        {
            (void)finishStagedCleaveHistoryGesture();
            Q_EMIT viewportStateChanged();
        }
        stagedCleaveLineClickCandidate_ = false;
        stagedCleaveDragSelectionApplied_ = false;
        stagedCleaveJointCycleClickCandidate_ = false;
        stagedCleaveJointCycleCandidates_.clear();
        update();
        event->accept();
        return;
    }

    if ( selectStagedCleaveOriginOnClick
        && cleaveStagedMode_
        && editingStagedCleaveLine_ >= 0
        && (editingStagedCleaveEndpoint_ == 0 || editingStagedCleaveEndpoint_ == 1)
        && event->modifiers() == Qt::NoModifier
        && !render::cleavePointerMotionIsDrag(dragDelta)
        && stagedCleaveContextMatchesCurrent(surfaceCleaveToolEnabled_) )
    {
        const bool originChanged = !stagedCleaveChainOrigin_
            || stagedCleaveChainOrigin_->lineIndex != editingStagedCleaveLine_
            || stagedCleaveChainOrigin_->endpoint != editingStagedCleaveEndpoint_;
        stagedCleaveChainOrigin_ = StagedCleaveChainOrigin{
            editingStagedCleaveLine_, editingStagedCleaveEndpoint_ };
        setSelectedStagedCleaveLines(
            {editingStagedCleaveLine_}, editingStagedCleaveLine_);
        editingStagedCleaveLine_ = -1;
        editingStagedCleaveEndpoint_ = -1;
        editingDisjointStagedCleaveEndpoint_ = false;
        editingStagedCleaveEndpointPreservePlacement_ = false;
        sectorCleaveHaveFirstPoint_ = false;
        surfaceCleaveHaveFirstPoint_ = false;
        leftDragged_ = false;
        cleaveSnapMarker_.reset();
        unsetCursor();
        if ( originChanged )
        {
            (void)finishStagedCleaveHistoryGesture();
            Q_EMIT viewportStateChanged();
        }
        else
        {
            discardStagedCleaveHistoryGesture();
            Q_EMIT viewportStateChanged();
        }
        stagedCleaveLineClickCandidate_ = false;
        stagedCleaveDragSelectionApplied_ = false;
        update();
        event->accept();
        return;
    }

    if (sectorCleaveToolEnabled_)
    {
        if (!sectorCleaveHaveFirstPoint_)
        {
            event->accept();
            return;
        }

        if (cleaveStagedMode_ && !stagedCleaveContextMatchesCurrent(false))
        {
            (void)restoreAndDiscardStagedCleaveHistoryGesture();
            sectorCleaveHaveFirstPoint_ = false;
            editingStagedCleaveLine_ = -1;
            editingStagedCleaveEndpoint_ = -1;
            editingDisjointStagedCleaveEndpoint_ = false;
            editingStagedCleaveEndpointPreservePlacement_ = false;
            unsetCursor();
            if (stagedCleaveLines_.empty())
            {
                stagedCleaveContext_ = {};
            }
            showToast(
                "Multi-cut: the target selection changed during the drag",
                3000,
                ToastPlacement::BottomCenter,
                ToastGroup::Error);
            update();
            event->accept();
            return;
        }

        QMatrix4x4 proj;
        QMatrix4x4 view;
        QVector3D eye{};
        QVector3D target{};
        float nearPlane{};
        float farPlane{};
        buildCameraMatrices(proj, view, eye, target, nearPlane, farPlane);
        (void)proj;
        (void)view;
        (void)nearPlane;
        (void)farPlane;

        const TVector p0 = sectorCleaveFirstPoint_;
        TVector p1 = sectorCleaveCurrentPoint_;
        bool closesChain = false;
        if (cleaveStagedMode_
            && cleaveChainEnabled_
            && editingStagedCleaveLine_ < 0
            && stagedCleaveLines_.size() >= 2
            && stagedCleaveChainOriginIsCanonicalTailSecond()
            && stagedCleaveLinesFormSingleOpenChain())
        {
            const ProjectedPoint firstEndpoint = projectWorldToWidget(
                proj * view,
                stagedCleaveLines_.front().first,
                width(),
                height());
            if (firstEndpoint.valid)
            {
                const QPointF delta = QPointF(releasePos) - firstEndpoint.pos;
                constexpr double kCloseChainRadiusPx = 12.0;
                closesChain = delta.x() * delta.x() + delta.y() * delta.y()
                    <= kCloseChainRadiusPx * kCloseChainRadiusPx;
                if (closesChain)
                {
                    p1 = stagedCleaveLines_.front().first;
                }
            }
        }
        TVector lineDir = p1 - p0;
        sectorCleaveHaveFirstPoint_ = false;
        update();
        if (!render::cleaveFinitePoint(lineDir) || lineDir.lengthSqr() == 0.0)
        {
            (void)restoreAndDiscardStagedCleaveHistoryGesture();
            showToast("Cleave: drag a line", 2500, ToastPlacement::BottomCenter, ToastGroup::Error);
            event->accept();
            return;
        }
        (void)render::cleaveNormalize(lineDir);

        // The sector cleave plane must match the dragged screen-space line.
        // - Perspective: the drag defines a plane through the camera eye (two rays).
        // - Orthographic: rays are parallel, so treat the drag as a line on the view plane at the target
        //   and build a plane containing the view direction and that line.
        TVector viewDir = (render::cleaveDocumentPoint(target) - render::cleaveDocumentPoint(eye));
        if (!normalizeInPlace(viewDir))
        {
            (void)restoreAndDiscardStagedCleaveHistoryGesture();
            showToast("Cleave: invalid camera", 2500, ToastPlacement::BottomCenter, ToastGroup::Error);
            event->accept();
            return;
        }

        const bool isOrtho = camera_.projectionType() == app::ViewportCamera::ProjectionType::Orthographic;

        TVector planeNormal{};
        if (cleaveStagedMode_
            && editingStagedCleaveLine_ >= 0
            && static_cast<std::size_t>(editingStagedCleaveLine_) < stagedCleaveLines_.size())
        {
            TVector constructionNormal = stagedCleaveLines_[
                static_cast<std::size_t>(editingStagedCleaveLine_)].constructionPlaneNormal;
            if (normalizeInPlace(constructionNormal))
            {
                planeNormal = render::cleaveCross(constructionNormal, lineDir);
            }
        }
        else if (!isOrtho)
        {
            TVector r0 = p0 - render::cleaveDocumentPoint(eye);
            TVector r1 = p1 - render::cleaveDocumentPoint(eye);
            if (!normalizeInPlace(r0) || !normalizeInPlace(r1))
            {
                (void)restoreAndDiscardStagedCleaveHistoryGesture();
                showToast("Cleave: invalid ray", 2500, ToastPlacement::BottomCenter, ToastGroup::Error);
                event->accept();
                return;
            }

            planeNormal = render::cleaveCross(r0, r1);
        }
        else
        {
            planeNormal = render::cleaveCross(viewDir, lineDir);
        }

        if (!normalizeInPlace(planeNormal))
        {
            (void)restoreAndDiscardStagedCleaveHistoryGesture();
            showToast("Cleave: invalid cut plane", 2500, ToastPlacement::BottomCenter, ToastGroup::Error);
            event->accept();
            return;
        }

        if (cleaveStagedMode_)
        {
            StagedCleaveLine line{};
            line.first = p0;
            line.second = p1;
            line.planeNormal = planeNormal;
            TVector preferredReference(view(0, 0), view(0, 1), view(0, 2));
            if (stagedCleaveContext_.valid)
            {
                preferredReference = TVector(
                    stagedCleaveContext_.view(0, 0),
                    stagedCleaveContext_.view(0, 1),
                    stagedCleaveContext_.view(0, 2));
            }
            if ( cleaveChainEnabled_
                && editingStagedCleaveLine_ < 0
                && !stagedCleaveLines_.empty() )
            {
                preferredReference = stagedCleaveChainOriginIncomingDirection(
                    stagedCleaveLines_);
            }
            initializeStagedCleaveLineFrame(
                line,
                preferredReference);
            captureStagedCleaveLineCamera(line);
            if (editingStagedCleaveLine_ >= 0
                && static_cast<std::size_t>(editingStagedCleaveLine_) < stagedCleaveLines_.size())
            {
                const std::size_t editedIndex = static_cast<std::size_t>(editingStagedCleaveLine_);
                reshapeStagedCleaveEndpointWithOptionalDisjoint(
                    stagedCleaveLines_,
                    editedIndex,
                    editingStagedCleaveEndpoint_,
                    p1,
                    editingDisjointStagedCleaveEndpoint_);
                // Alt is an endpoint rip: preserve the exact position the
                // user chose instead of immediately replacing it with the
                // independent Sector line's camera-edge attachment.
                if (!editingDisjointStagedCleaveEndpoint_
                    && !editingStagedCleaveEndpointPreservePlacement_
                    && !cleaveBounded_)
                {
                    (void)attachStagedSectorCleaveLineToCameraEdge(editedIndex);
                }
                selectedStagedCleaveLine_ = editingStagedCleaveLine_;
            }
            else
            {
                const bool extendsCanonicalChain = cleaveChainEnabled_
                    && !stagedCleaveLines_.empty()
                    && stagedCleaveChainOriginIsCanonicalTailSecond();
                if ( extendsCanonicalChain )
                {
                    line.linkedToPrevious = true;
                    stagedCleaveLines_.back().linkedToNext = true;
                    if (closesChain)
                    {
                        line.linkedToNext = true;
                        stagedCleaveLines_.front().linkedToPrevious = true;
                    }
                }
                stagedCleaveLines_.push_back(line);
                selectedStagedCleaveLine_ = static_cast<std::int32_t>(
                    stagedCleaveLines_.size() - 1);
                stagedCleaveApplyOrder_.push_back(selectedStagedCleaveLine_);
                setSelectedStagedCleaveLines(
                    {selectedStagedCleaveLine_}, selectedStagedCleaveLine_);
                if ( cleaveChainEnabled_ )
                {
                    stagedCleaveChainOrigin_ = StagedCleaveChainOrigin{
                        selectedStagedCleaveLine_, 1 };
                }
                if ( !cleaveBounded_ )
                {
                    (void)attachStagedSectorCleaveLineToCameraEdge(
                        static_cast<std::size_t>(selectedStagedCleaveLine_));
                }
            }
            editingStagedCleaveLine_ = -1;
            editingStagedCleaveEndpoint_ = -1;
            editingDisjointStagedCleaveEndpoint_ = false;
            editingStagedCleaveEndpointPreservePlacement_ = false;
            unsetCursor();
            (void)finishStagedCleaveHistoryGesture();
            Q_EMIT viewportStateChanged();
            update();
            event->accept();
            return;
        }

        Q_EMIT sectorsCleaveRequested(planeNormal, p0);

        event->accept();
        return;
    }

    if (surfaceCleaveToolEnabled_)
    {
        if (!surfaceCleaveHaveFirstPoint_)
        {
            event->accept();
            return;
        }

        if (cleaveStagedMode_ && !stagedCleaveContextMatchesCurrent(true))
        {
            (void)restoreAndDiscardStagedCleaveHistoryGesture();
            surfaceCleaveHaveFirstPoint_ = false;
            editingStagedCleaveLine_ = -1;
            editingStagedCleaveEndpoint_ = -1;
            editingDisjointStagedCleaveEndpoint_ = false;
            editingStagedCleaveEndpointPreservePlacement_ = false;
            unsetCursor();
            if (stagedCleaveLines_.empty())
            {
                stagedCleaveContext_ = {};
            }
            showToast(
                "Multi-cut surface: the source face or target selection changed during the drag",
                3000,
                ToastPlacement::BottomCenter,
                ToastGroup::Error);
            update();
            event->accept();
            return;
        }

        if (!level_ || selectedSurfaceSector_ < 0 || selectedSurface_ < 0
            || static_cast<std::size_t>(selectedSurfaceSector_) >= level_->sectors.size())
        {
            (void)restoreAndDiscardStagedCleaveHistoryGesture();
            surfaceCleaveHaveFirstPoint_ = false;
            update();
            event->accept();
            return;
        }

        const Sector& sector = level_->sectors[static_cast<std::size_t>(selectedSurfaceSector_)];
        if (static_cast<std::size_t>(selectedSurface_) >= sector.surfaces.size())
        {
            (void)restoreAndDiscardStagedCleaveHistoryGesture();
            surfaceCleaveHaveFirstPoint_ = false;
            update();
            event->accept();
            return;
        }

        const Surface& surface = sector.surfaces[static_cast<std::size_t>(selectedSurface_)];
        if (surface.vertices.size() < 3)
        {
            (void)restoreAndDiscardStagedCleaveHistoryGesture();
            surfaceCleaveHaveFirstPoint_ = false;
            update();
            event->accept();
            return;
        }

        TVector surfNormal(surface.normal.x, surface.normal.y, surface.normal.z);
        if (surfNormal.lengthSqr() < sed::math::kTinyEpsilonF)
        {
            const std::int32_t v0i = surface.vertices[0];
            const std::int32_t v1i = surface.vertices[1];
            const std::int32_t v2i = surface.vertices[2];
            if (v0i >= 0 && v1i >= 0 && v2i >= 0
                && static_cast<std::size_t>(v0i) < sector.vertices.size()
                && static_cast<std::size_t>(v1i) < sector.vertices.size()
                && static_cast<std::size_t>(v2i) < sector.vertices.size())
            {
                const Vertex& v0 = sector.vertices[static_cast<std::size_t>(v0i)];
                const Vertex& v1 = sector.vertices[static_cast<std::size_t>(v1i)];
                const Vertex& v2 = sector.vertices[static_cast<std::size_t>(v2i)];
                const TVector p0v(v0.x, v0.y, v0.z);
                const TVector p1v(v1.x, v1.y, v1.z);
                const TVector p2v(v2.x, v2.y, v2.z);
                surfNormal = render::cleaveCross(p1v - p0v, p2v - p0v);
            }
        }

        if (!normalizeInPlace(surfNormal))
        {
            (void)restoreAndDiscardStagedCleaveHistoryGesture();
            surfaceCleaveHaveFirstPoint_ = false;
            update();
            event->accept();
            return;
        }

        if (surfaceSubMode_ == SurfaceSubMode::Vertex)
        {
            (void)restoreAndDiscardStagedCleaveHistoryGesture();
            surfaceCleaveHaveFirstPoint_ = false;
            update();
            showToast("Cleave: switch to face or edge mode", 2500, ToastPlacement::BottomCenter, ToastGroup::Error);
            event->accept();
            return;
        }

        if (surfaceSubMode_ == SurfaceSubMode::Edge)
        {
            const bool haveEdge =
                haveLastSurfaceHit_
                && lastSurfaceHitSurface_.sector == selectedSurfaceSector_
                && lastSurfaceHitSurface_.surface == selectedSurface_;
            if (!haveEdge)
            {
                (void)restoreAndDiscardStagedCleaveHistoryGesture();
                surfaceCleaveHaveFirstPoint_ = false;
                update();
                showToast("Cleave edge: click an edge first", 2500, ToastPlacement::BottomCenter, ToastGroup::Error);
                event->accept();
                return;
            }
        }

        const TVector p0 = surfaceCleaveFirstPoint_;
        TVector p1 = surfaceCleaveCurrentPoint_;
        bool closesChain = false;
        if (cleaveStagedMode_
            && cleaveChainEnabled_
            && editingStagedCleaveLine_ < 0
            && stagedCleaveLines_.size() >= 2
            && stagedCleaveChainOriginIsCanonicalTailSecond()
            && stagedCleaveLinesFormSingleOpenChain())
        {
            QMatrix4x4 projection{};
            QMatrix4x4 cameraView{};
            QVector3D cameraEye{};
            QVector3D cameraTarget{};
            float nearPlane{};
            float farPlane{};
            buildCameraMatrices(
                projection,
                cameraView,
                cameraEye,
                cameraTarget,
                nearPlane,
                farPlane);
            const ProjectedPoint firstEndpoint = projectWorldToWidget(
                projection * cameraView,
                stagedCleaveLines_.front().first,
                width(),
                height());
            if (firstEndpoint.valid)
            {
                const QPointF delta = QPointF(releasePos) - firstEndpoint.pos;
                constexpr double kCloseChainRadiusPx = 12.0;
                closesChain = delta.x() * delta.x() + delta.y() * delta.y()
                    <= kCloseChainRadiusPx * kCloseChainRadiusPx;
                if (closesChain)
                {
                    p1 = stagedCleaveLines_.front().first;
                }
            }
        }
        TVector lineDir = p1 - p0;
        surfaceCleaveHaveFirstPoint_ = false;
        update();
        if (!render::cleaveFinitePoint(lineDir) || lineDir.lengthSqr() == 0.0)
        {
            (void)restoreAndDiscardStagedCleaveHistoryGesture();
            showToast("Cleave surface: drag a line", 2500, ToastPlacement::BottomCenter, ToastGroup::Error);
            event->accept();
            return;
        }
        (void)render::cleaveNormalize(lineDir);

        TVector planeNormal = render::cleaveCross(surfNormal, lineDir);
        if (!normalizeInPlace(planeNormal))
        {
            (void)restoreAndDiscardStagedCleaveHistoryGesture();
            showToast("Cleave surface: invalid cut plane", 2500, ToastPlacement::BottomCenter, ToastGroup::Error);
            event->accept();
            return;
        }

        if (cleaveStagedMode_ && surfaceSubMode_ != SurfaceSubMode::Edge)
        {
            StagedCleaveLine line{};
            line.first = p0;
            line.second = p1;
            line.planeNormal = planeNormal;
            TVector preferredReference = lineDir;
            if (stagedCleaveContext_.valid)
            {
                preferredReference = TVector(
                    stagedCleaveContext_.view(0, 0),
                    stagedCleaveContext_.view(0, 1),
                    stagedCleaveContext_.view(0, 2));
            }
            if ( cleaveChainEnabled_
                && editingStagedCleaveLine_ < 0
                && !stagedCleaveLines_.empty() )
            {
                preferredReference = stagedCleaveChainOriginIncomingDirection(
                    stagedCleaveLines_);
            }
            initializeStagedCleaveLineFrame(line, preferredReference);
            captureStagedCleaveLineCamera(line);
            if (editingStagedCleaveLine_ >= 0
                && static_cast<std::size_t>(editingStagedCleaveLine_) < stagedCleaveLines_.size())
            {
                const std::size_t editedIndex = static_cast<std::size_t>(editingStagedCleaveLine_);
                reshapeStagedCleaveEndpointWithOptionalDisjoint(
                    stagedCleaveLines_,
                    editedIndex,
                    editingStagedCleaveEndpoint_,
                    p1,
                    editingDisjointStagedCleaveEndpoint_);
                selectedStagedCleaveLine_ = editingStagedCleaveLine_;
            }
            else
            {
                const bool extendsCanonicalChain = cleaveChainEnabled_
                    && !stagedCleaveLines_.empty()
                    && stagedCleaveChainOriginIsCanonicalTailSecond();
                if ( extendsCanonicalChain )
                {
                    line.linkedToPrevious = true;
                    stagedCleaveLines_.back().linkedToNext = true;
                    if (closesChain)
                    {
                        line.linkedToNext = true;
                        stagedCleaveLines_.front().linkedToPrevious = true;
                    }
                }
                stagedCleaveLines_.push_back(line);
                selectedStagedCleaveLine_ = static_cast<std::int32_t>(
                    stagedCleaveLines_.size() - 1);
                stagedCleaveApplyOrder_.push_back(selectedStagedCleaveLine_);
                setSelectedStagedCleaveLines(
                    {selectedStagedCleaveLine_}, selectedStagedCleaveLine_);
                if ( cleaveChainEnabled_ )
                {
                    stagedCleaveChainOrigin_ = StagedCleaveChainOrigin{
                        selectedStagedCleaveLine_, 1 };
                }
            }
            editingStagedCleaveLine_ = -1;
            editingStagedCleaveEndpoint_ = -1;
            editingDisjointStagedCleaveEndpoint_ = false;
            editingStagedCleaveEndpointPreservePlacement_ = false;
            unsetCursor();
            (void)finishStagedCleaveHistoryGesture();
            Q_EMIT viewportStateChanged();
            update();
            event->accept();
            return;
        }

        if (surfaceSubMode_ == SurfaceSubMode::Edge)
        {
            // Edge mode: split the active edge (insert a vertex) instead of cleaving the whole face.
            std::int32_t bestEdge = -1;
            double bestDist2 = std::numeric_limits<double>::infinity();

            const auto tryGetVx = [&](const std::int32_t vi, TVector& out) -> bool
            {
                if (vi < 0 || static_cast<std::size_t>(vi) >= sector.vertices.size())
                {
                    return false;
                }
                const Vertex& v = sector.vertices[static_cast<std::size_t>(vi)];
                out = TVector{ v.x, v.y, v.z };
                return true;
            };

            if (surface.vertices.size() >= 2)
            {
                for (std::size_t i = 0; i < surface.vertices.size(); ++i)
                {
                    const std::int32_t aidx = surface.vertices[i];
                    const std::int32_t bidx = surface.vertices[(i + 1) % surface.vertices.size()];

                    TVector a{};
                    TVector b{};
                    if (!tryGetVx(aidx, a) || !tryGetVx(bidx, b))
                    {
                        continue;
                    }

                    const TVector ab = b - a;
                    const double ab2 = ab.Dot(ab);
                    if (ab2 <= sed::math::kTinyEpsilon)
                    {
                        continue;
                    }

                    const TVector ap = lastSurfaceHitPos_ - a;
                    double t = ap.Dot(ab) / ab2;
                    t = std::clamp(t, 0.0, 1.0);
                    const TVector closest = a + ab * t;
                    const TVector d = lastSurfaceHitPos_ - closest;
                    const double d2 = d.Dot(d);
                    if (d2 < bestDist2)
                    {
                        bestDist2 = d2;
                        bestEdge = static_cast<std::int32_t>(i);
                    }
                }
            }

            if (bestEdge < 0)
            {
                showToast("Cleave edge: cannot determine edge", 2500, ToastPlacement::BottomCenter, ToastGroup::Error);
                event->accept();
                return;
            }

            Q_EMIT surfaceEdgeCleaveRequested(selectedSurfaceSector_, selectedSurface_, bestEdge, planeNormal, p0);
        }
        else
        {
            Q_EMIT surfacesCleaveRequested(planeNormal, p0);
        }

        event->accept();
        return;
    }

    if (boxSelectActive_)
    {
        boxSelectActive_ = false;
        const bool fromTransformTool = boxSelectFromTransformTool_;
        boxSelectFromTransformTool_ = false;
        const QRect rect = QRect(boxSelectStart_, releasePos).normalized();
        const bool doBoxSelect =
            renderPipeline_ != RenderPipeline::GamePreview
            && boxSelectVisible_
            && rect.isValid()
            && (rect.width() > 4 || rect.height() > 4);
        boxSelectVisible_ = false;

        if (doBoxSelect)
        {
            editor::Selection boxSelection{};
            Qt::KeyboardModifiers boxMods = event->modifiers();
            if (fromTransformTool)
            {
                // Alt is the temporary marquee clutch, not a selection mode.
                boxMods &= ~Qt::AltModifier;
            }
            const Qt::KeyboardModifiers kSelectionModMask =
                Qt::ShiftModifier | Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier;
            const Qt::KeyboardModifiers heldMods = boxMods & kSelectionModMask;
            auto selectionModifierActive = [heldMods](
                const ui::shortcuts::ModifierBindings& required) -> bool
            {
                return ui::shortcuts::modifierBindingsActive(heldMods, required);
            };

            SelectionBoxMode boxMode = SelectionBoxMode::Replace;
            if (selectionModifierActive(selectionRemoveModifiers_))
            {
                boxMode = SelectionBoxMode::Remove;
            }
            else if (selectionModifierActive(selectionAddModifiers_))
            {
                boxMode = SelectionBoxMode::Add;
            }

            PickMode mode = pickMode_;
            if (mode == PickMode::Auto)
            {
                switch (pickPreference_)
                {
                    case PickPreference::PreferThing:
                    {
                        mode = PickMode::Thing;
                        break;
                    }
                    case PickPreference::PreferLight:
                    {
                        mode = PickMode::Light;
                        break;
                    }
                    default:
                    {
                        mode = PickMode::Thing;
                        break;
                    }
                }
            }

            if (level_ && width() > 0 && height() > 0)
            {
                QMatrix4x4 proj;
                QMatrix4x4 view;
                QVector3D eye{};
                QVector3D target{};
                float nearPlane{};
                float farPlane{};
                buildCameraMatrices(proj, view, eye, target, nearPlane, farPlane);

                const QMatrix4x4 mvp = proj * view;

                auto toScreen = [&](const QVector3D& p, QPointF& outPos) -> bool
                {
                    return render::projectWorldPointToScreen(p, mvp, width(), height(), outPos);
                };

                const QRectF box(rect);
                const bool orthographicCamera =
                    camera_.projectionType() == app::ViewportCamera::ProjectionType::Orthographic;

                const bool renderThroughPickMode = !showSolid_;
                const bool bypassSelectionFilters =
                    selectionModifierActive(selectionSurfaceBypassFiltersModifiers_);
                // X-Ray Selection applies to marquee selection as well as
                // repeated point clicks. A marquee must collect every
                // eligible projected candidate, not only the depth-visible
                // front-most one. Keep the existing layer/edit-mode filters
                // below; this flag only relaxes scene occlusion.
                const bool xrayAreaSelection = render::picking::marqueeIgnoresSceneDepth(
                    xraySelectionCyclingEnabled_,
                    fromTransformTool
                        ? render::picking::MarqueeSelectionSource::TransformTool
                        : render::picking::MarqueeSelectionSource::Select);
                const bool allowThroughSurfacePick =
                    bypassSelectionFilters || renderThroughPickMode || xrayAreaSelection;
                const bool allowThroughThingPick =
                    bypassSelectionFilters || renderThroughPickMode || xrayAreaSelection;

                std::optional<render::LightGizmoCategory> pickedLightCategory{};
                if (surfaceClickSelectionEnabled_ && !cogEntityPickHighlightEnabled_)
                {
                    if (pickMode_ == PickMode::Light
                        && lightGizmoCategory_ == render::LightGizmoCategory::Baked)
                    {
                        pickedLightCategory = render::LightGizmoCategory::Baked;
                    }
                    else if (pickMode_ == PickMode::Thing
                        && lightGizmoCategory_ == render::LightGizmoCategory::Thing)
                    {
                        pickedLightCategory = render::LightGizmoCategory::Thing;
                    }
                    else if (pickMode_ == PickMode::Sector
                        && lightGizmoCategory_ == render::LightGizmoCategory::Sector)
                    {
                        pickedLightCategory = render::LightGizmoCategory::Sector;
                    }
                }

                if (pickedLightCategory)
                {
                    std::vector<render::LightGizmoEntry> entries{};
                    switch (*pickedLightCategory)
                    {
                        case render::LightGizmoCategory::Baked:
                            entries = render::bakedLightGizmoEntries(*level_);
                            break;
                        case render::LightGizmoCategory::Thing:
                            entries = render::thingLightGizmoEntries(*level_, templates_);
                            break;
                        case render::LightGizmoCategory::Sector:
                            entries = render::sectorLightGizmoEntries(*level_);
                            break;
                        case render::LightGizmoCategory::Architectural:
                            // ARCH owners are selected by the dedicated
                            // architectural branch above this mixed-family
                            // marquee path.
                            break;
                    }
                    bool invViewOk = false;
                    const QMatrix4x4 invViewForLightIcons = view.inverted(&invViewOk);
                    QVector3D lightIconUp(
                        invViewForLightIcons(0, 1),
                        invViewForLightIcons(1, 1),
                        invViewForLightIcons(2, 1));
                    if (!invViewOk)
                    {
                        lightIconUp = QVector3D(0.0f, 0.0f, 1.0f);
                    }
                    const float lightIconHalfSize =
                        std::clamp(lightGizmoWorldSize_, 0.001f, 1.0f) * 0.5f;
                    for (render::LightGizmoEntry& entry : entries)
                    {
                        entry.position = lightGizmoDisplayPositionForEntry(
                            entry,
                            lightIconUp,
                            lightIconHalfSize);
                    }

                    struct CategoryCandidate
                    {
                        std::int32_t ownerIndex{-1};
                        QPointF screen{};
                        float depth{1.0f};
                    };
                    std::vector<CategoryCandidate> candidates{};
                    candidates.reserve(entries.size());
                    std::vector<QPointF> samplePoints{};
                    samplePoints.reserve(entries.size());
                    for (const render::LightGizmoEntry& entry : entries)
                    {
                        if (!isLayerVisible(entry.layer))
                        {
                            continue;
                        }

                        const QVector4D clip = mvp * QVector4D(entry.position, 1.0f);
                        if (!std::isfinite(clip.w()) || clip.w() <= 0.0f)
                        {
                            continue;
                        }
                        const float invW = 1.0f / clip.w();
                        const float ndcX = clip.x() * invW;
                        const float ndcY = clip.y() * invW;
                        const float ndcZ = clip.z() * invW;
                        if (!std::isfinite(ndcX) || !std::isfinite(ndcY) || !std::isfinite(ndcZ)
                            || ndcZ < -1.0f || ndcZ > 1.0f)
                        {
                            continue;
                        }

                        const QPointF screen(
                            (ndcX * 0.5f + 0.5f) * static_cast<float>(width()),
                            (1.0f - (ndcY * 0.5f + 0.5f)) * static_cast<float>(height()));
                        if (!box.contains(screen))
                        {
                            continue;
                        }

                        candidates.push_back(CategoryCandidate{
                            entry.ownerIndex,
                            screen,
                            ndcZ * 0.5f + 0.5f,
                        });
                        samplePoints.push_back(screen);
                    }

                    // X-Ray deliberately bypasses scene-depth filtering. Do
                    // not allocate/fill a dummy depth array in that mode: a
                    // large light-gizmo marquee can otherwise pay for an
                    // allocation and initialization that the loop below
                    // never reads.
                    const std::vector<float> sceneDepths = xrayAreaSelection
                        ? std::vector<float>{}
                        : pickSceneDepthsAtPoints(
                              mvp,
                              samplePoints,
                              showSolid_,
                              showThings_ && thingViewMode_ == ThingViewMode::Shaded,
                              true);
                    for (std::size_t i = 0; i < candidates.size(); ++i)
                    {
                        if (!xrayAreaSelection
                            && (i >= sceneDepths.size()
                                || !render::picking::marqueeCandidatePassesSceneDepth(
                                    candidates[i].depth,
                                    sceneDepths[i],
                                    xrayAreaSelection)))
                        {
                            continue;
                        }
                        switch (*pickedLightCategory)
                        {
                            case render::LightGizmoCategory::Baked:
                                boxSelection.lights.insert(candidates[i].ownerIndex);
                                break;
                            case render::LightGizmoCategory::Thing:
                                boxSelection.things.insert(candidates[i].ownerIndex);
                                break;
                            case render::LightGizmoCategory::Sector:
                                boxSelection.sectors.insert(candidates[i].ownerIndex);
                                break;
                            case render::LightGizmoCategory::Architectural:
                                // There is no runtime light owner set here;
                                // ARCH owner rectangles are emitted below.
                                break;
                        }
                    }
                }
                else if (mode == PickMode::Thing)
                {
                    struct SpriteMarqueeCandidate
                    {
                        std::int32_t index{-1};
                        float depth{1.0f};
                        QPointF sample{};
                    };
                    std::vector<SpriteMarqueeCandidate> spriteMarqueeCandidates{};
                    struct OverlayMarqueeCandidate
                    {
                        std::int32_t index{ -1 };
                        QPointF sample{};
                        float depth{ 1.0f };
                    };
                    std::vector<OverlayMarqueeCandidate> overlayMarqueeCandidates{};

                    if (thingViewMode_ == ThingViewMode::Shaded && !xrayAreaSelection)
                    {
                        const auto pickedThings =
                            pickThingsInScreenRect(
                                mvp,
                                rect.left(),
                                rect.top(),
                                rect.right(),
                                rect.bottom(),
                                showSolid_ && !allowThroughThingPick);
                        boxSelection.things.insert(pickedThings.begin(), pickedThings.end());
                    }

                    auto thingHasTriGeometry = [&](const int pid) -> bool
                    {
                        if (pid <= 0)
                        {
                            return false;
                        }
                        const auto it = thingTriBuildRangesByPickId_.find(pid);
                        return it != thingTriBuildRangesByPickId_.end() && !it->second.empty();
                    };

                    auto thingHitsRectByLines = [&](const std::size_t thingIndex) -> bool
                    {
                        if (thingIndex >= thingPickIds_.size() || thingMeshCache_.lineVertices.empty())
                        {
                            return false;
                        }

                        const int pid = static_cast<int>(thingPickIds_[thingIndex]);
                        if (pid <= 0)
                        {
                            return false;
                        }

                        const auto it = thingLineRangesByPickId_.find(pid);
                        if (it == thingLineRangesByPickId_.end())
                        {
                            return false;
                        }

                        const auto& lines = thingMeshCache_.lineVertices;
                        const int lineCount = static_cast<int>(lines.size());
                        for (const DrawRange& range : it->second)
                        {
                            const int rangeFirst = range.first;
                            const int rangeEnd = range.first + range.count;
                            int first = std::max(0, rangeFirst);
                            int end = std::min(rangeEnd, lineCount);
                            if (first >= end - 1)
                            {
                                continue;
                            }

                            for (int vi = first; vi + 1 < end; vi += 2)
                            {
                                const LineVertex& a = lines[static_cast<std::size_t>(vi)];
                                const LineVertex& b = lines[static_cast<std::size_t>(vi + 1)];
                                QPointF sa{};
                                QPointF sb{};
                                if (!toScreen(QVector3D(a.x, a.y, a.z), sa) || !toScreen(QVector3D(b.x, b.y, b.z), sb))
                                {
                                    continue;
                                }

                                if (render::screenSegmentIntersectsRect(sa, sb, box))
                                {
                                    return true;
                                }
                            }
                        }

                        return false;
                    };

                    auto thingHitsRectByOverlayLines = [&] (
                        const std::size_t thingIndex) -> bool
                    {
                        if ((!showThingCollisionShapes_ && !showThingVisualBounds_)
                            || !thingMeshCache_.valid
                            || thingMeshCache_.overlayLineVertices.empty()
                            || thingIndex >= thingPickIds_.size())
                        {
                            return false;
                        }
                        const std::uint32_t pickIdValue = thingPickIds_[thingIndex];
                        if (pickIdValue == 0u
                            || pickIdValue > static_cast<std::uint32_t>(
                                std::numeric_limits<int>::max()))
                        {
                            return false;
                        }
                        const int pickId = static_cast<int>(pickIdValue);
                        const auto& vertices = thingMeshCache_.overlayLineVertices;
                        bool found = false;
                        const auto appendOwnerRanges = [&] (
                            const std::unordered_map<int, std::vector<DrawRange>>& rangesByPickId)
                        {
                            const auto rangeIt = rangesByPickId.find(pickId);
                            if (rangeIt == rangesByPickId.end())
                            {
                                return;
                            }
                            for (const DrawRange& range : rangeIt->second)
                            {
                                if (range.first < 0 || range.count < 2)
                                {
                                    continue;
                                }
                                const std::int64_t rangeFirst = range.first;
                                const std::int64_t rangeEnd = rangeFirst
                                    + static_cast<std::int64_t>(range.count);
                                const std::size_t first = static_cast<std::size_t>(
                                    std::max<std::int64_t>(rangeFirst, 0));
                                const std::size_t end = static_cast<std::size_t>(
                                    std::min<std::int64_t>(
                                        rangeEnd,
                                        static_cast<std::int64_t>(vertices.size())));
                                if (first >= end || end - first < 2u)
                                {
                                    continue;
                                }
                                for (std::size_t vertex = first;
                                     vertex + 1u < end;
                                     vertex += 2u)
                                {
                                    const LineVertex& a = vertices[vertex];
                                    const LineVertex& b = vertices[vertex + 1u];
                                    const std::optional<ProjectedLineSegment> projected =
                                        projectClippedLineSegment(
                                            QVector3D(a.x, a.y, a.z),
                                            QVector3D(b.x, b.y, b.z),
                                            mvp,
                                            width(),
                                            height());
                                    if (!projected)
                                    {
                                        continue;
                                    }
                                    const std::optional<std::pair<double, double>> interval =
                                        projectedLineRectInterval(
                                            projected->first,
                                            projected->second,
                                            box);
                                    if (!interval)
                                    {
                                        continue;
                                    }

                                    const QPointF screenDelta =
                                        projected->second - projected->first;
                                    const std::array<double, 3> sampleFractions{
                                        interval->first,
                                        (interval->first + interval->second) * 0.5,
                                        interval->second };
                                    const std::size_t sampleCount =
                                        interval->second - interval->first <= 1.0e-9
                                            ? 1u
                                            : sampleFractions.size();
                                    for (std::size_t sampleIndex = 0;
                                         sampleIndex < sampleCount;
                                         ++sampleIndex)
                                    {
                                        const double parameter =
                                            sampleCount == 1u
                                                ? sampleFractions[0]
                                                : sampleFractions[sampleIndex];
                                        const QPointF samplePoint =
                                            projected->first + screenDelta * parameter;
                                        const float depth = static_cast<float>(
                                            projected->firstDepth
                                            + (projected->secondDepth
                                                - projected->firstDepth)
                                                * parameter);
                                        if (!std::isfinite(depth)
                                            || depth < 0.0f || depth > 1.0f)
                                        {
                                            continue;
                                        }
                                        overlayMarqueeCandidates.push_back(
                                            OverlayMarqueeCandidate{
                                                static_cast<std::int32_t>(thingIndex),
                                                samplePoint,
                                                depth });
                                        found = true;
                                    }
                                }
                            }
                        };

                        if (showThingCollisionShapes_)
                        {
                            appendOwnerRanges(thingCollisionOverlayRangesByPickId_);
                            appendOwnerRanges(thingBroadPhaseTreeGuideRangesByPickId_);
                        }
                        if (showThingVisualBounds_)
                        {
                            appendOwnerRanges(thingVisualBoundsOverlayRangesByPickId_);
                        }
                        return found;
                    };

                    struct SpriteBoundsHit
                    {
                        float depth{1.0f};
                        QPointF sample{};
                    };

                    auto thingHitsRectBySpriteBounds = [&](const std::size_t thingIndex)
                        -> std::optional<SpriteBoundsHit>
                    {
                        if (thingIndex >= spriteThingBounds_.size())
                        {
                            return std::nullopt;
                        }
                        const ThingBounds& bounds = spriteThingBounds_[thingIndex];
                        if (!bounds.valid)
                        {
                            return std::nullopt;
                        }

                        const QVector3D corners[8]{
                            { bounds.min.x(), bounds.min.y(), bounds.min.z() },
                            { bounds.max.x(), bounds.min.y(), bounds.min.z() },
                            { bounds.min.x(), bounds.max.y(), bounds.min.z() },
                            { bounds.max.x(), bounds.max.y(), bounds.min.z() },
                            { bounds.min.x(), bounds.min.y(), bounds.max.z() },
                            { bounds.max.x(), bounds.min.y(), bounds.max.z() },
                            { bounds.min.x(), bounds.max.y(), bounds.max.z() },
                            { bounds.max.x(), bounds.max.y(), bounds.max.z() },
                        };
                        float minX = std::numeric_limits<float>::infinity();
                        float minY = std::numeric_limits<float>::infinity();
                        float maxX = -std::numeric_limits<float>::infinity();
                        float maxY = -std::numeric_limits<float>::infinity();
                        float minDepth = std::numeric_limits<float>::infinity();
                        QPointF nearestSample{};
                        bool visible = false;
                        for (const QVector3D& corner : corners)
                        {
                            const QVector4D clip = mvp * QVector4D(corner, 1.0f);
                            if (!std::isfinite(clip.w()) || clip.w() <= 0.0f)
                            {
                                continue;
                            }
                            const QVector3D ndc = clip.toVector3D() / clip.w();
                            if (!std::isfinite(ndc.x()) || !std::isfinite(ndc.y()) || !std::isfinite(ndc.z())
                                || ndc.z() < -1.0f || ndc.z() > 1.0f)
                            {
                                continue;
                            }
                            const float sx = (ndc.x() * 0.5f + 0.5f) * static_cast<float>(width());
                            const float sy = (1.0f - (ndc.y() * 0.5f + 0.5f)) * static_cast<float>(height());
                            minX = std::min(minX, sx);
                            minY = std::min(minY, sy);
                            maxX = std::max(maxX, sx);
                            maxY = std::max(maxY, sy);
                            const float depth = ndc.z() * 0.5f + 0.5f;
                            if (depth < minDepth)
                            {
                                minDepth = depth;
                                nearestSample = QPointF(sx, sy);
                            }
                            visible = true;
                        }
                        if (!visible || !std::isfinite(minDepth))
                        {
                            return std::nullopt;
                        }

                        const QRectF screenRect =
                            QRectF(QPointF(minX, minY), QPointF(maxX, maxY)).normalized();
                        if (!screenRect.intersects(box))
                        {
                            return std::nullopt;
                        }
                        return SpriteBoundsHit{ minDepth, nearestSample };
                    };

                    // X-Ray marquee picking cannot use the depth-tested GPU
                    // color pass: that pass intentionally leaves only the
                    // nearest triangle at each pixel. Project the cached
                    // world bounds instead so a model/sprite behind another
                    // entity remains an eligible candidate. Bounds are the
                    // same conservative representation used by the normal
                    // Thing fallback and retain the existing visibility and
                    // editability filters from the caller. Clip the bounds
                    // to the viewport before returning them. This keeps a
                    // full-viewport marquee cheap even when a model extends
                    // beyond a viewport edge, while boundary marquees can
                    // still request exact triangle refinement.
                    auto thingProjectedBounds = [&](const ThingBounds& bounds)
                        -> std::optional<QRectF>
                    {
                        if (!bounds.valid)
                        {
                            return std::nullopt;
                        }

                        const QVector3D corners[8]{
                            { bounds.min.x(), bounds.min.y(), bounds.min.z() },
                            { bounds.max.x(), bounds.min.y(), bounds.min.z() },
                            { bounds.min.x(), bounds.max.y(), bounds.min.z() },
                            { bounds.max.x(), bounds.max.y(), bounds.min.z() },
                            { bounds.min.x(), bounds.min.y(), bounds.max.z() },
                            { bounds.max.x(), bounds.min.y(), bounds.max.z() },
                            { bounds.min.x(), bounds.max.y(), bounds.max.z() },
                            { bounds.max.x(), bounds.max.y(), bounds.max.z() },
                        };

                        float minX = std::numeric_limits<float>::infinity();
                        float minY = std::numeric_limits<float>::infinity();
                        float maxX = -std::numeric_limits<float>::infinity();
                        float maxY = -std::numeric_limits<float>::infinity();
                        bool projected = false;
                        for (const QVector3D& corner : corners)
                        {
                            QPointF screen{};
                            if (!toScreen(corner, screen))
                            {
                                continue;
                            }
                            minX = std::min(minX, static_cast<float>(screen.x()));
                            minY = std::min(minY, static_cast<float>(screen.y()));
                            maxX = std::max(maxX, static_cast<float>(screen.x()));
                            maxY = std::max(maxY, static_cast<float>(screen.y()));
                            projected = true;
                        }

                        if (!projected)
                        {
                            return std::nullopt;
                        }

                        const QRectF screenRect =
                            QRectF(QPointF(minX, minY), QPointF(maxX, maxY)).normalized();
                        const QRectF viewportRect(
                            0.0,
                            0.0,
                            static_cast<double>(width()),
                            static_cast<double>(height()));
                        if (!screenRect.intersects(viewportRect))
                        {
                            return std::nullopt;
                        }
                        const QRectF visibleRect = screenRect.intersected(viewportRect).normalized();
                        if (!visibleRect.isValid())
                        {
                            return std::nullopt;
                        }
                        return visibleRect;
                    };

                    auto thingBoundsHitRect = [&](const ThingBounds& bounds) -> bool
                    {
                        const std::optional<QRectF> projectedBounds = thingProjectedBounds(bounds);
                        return projectedBounds && projectedBounds->intersects(box);
                    };

                    auto thingTrianglesHitRect = [&](const int pickId) -> bool
                    {
                        if (pickId <= 0)
                        {
                            return false;
                        }

                        const auto rangeIt = thingTriBuildRangesByPickId_.find(pickId);
                        if (rangeIt == thingTriBuildRangesByPickId_.end())
                        {
                            return false;
                        }

                        for (const TriBuildRange& range : rangeIt->second)
                        {
                            if (static_cast<std::size_t>(range.buildIndex)
                                >= thingMeshCache_.triBuilds.size()
                                || range.first < 0
                                || range.count < 3)
                            {
                                continue;
                            }

                            const auto& vertices = thingMeshCache_.triBuilds[
                                static_cast<std::size_t>(range.buildIndex)].vertices;
                            const std::size_t first = static_cast<std::size_t>(range.first);
                            const std::size_t count = static_cast<std::size_t>(range.count);
                            if (first > vertices.size() || count > (vertices.size() - first))
                            {
                                continue;
                            }

                            const std::size_t completeCount = count - (count % 3u);
                            for (std::size_t offset = 0; offset < completeCount; offset += 3u)
                            {
                                const TriVertex& a = vertices[first + offset + 0u];
                                const TriVertex& b = vertices[first + offset + 1u];
                                const TriVertex& c = vertices[first + offset + 2u];
                                QPointF sa{};
                                QPointF sb{};
                                QPointF sc{};
                                if (!toScreen(QVector3D(a.x, a.y, a.z), sa)
                                    || !toScreen(QVector3D(b.x, b.y, b.z), sb)
                                    || !toScreen(QVector3D(c.x, c.y, c.z), sc))
                                {
                                    continue;
                                }

                                if (render::screenPolygonIntersectsRect(
                                        std::vector<QPointF>{ sa, sb, sc },
                                        box))
                                {
                                    return true;
                                }
                            }
                        }

                        return false;
                    };

                    const render::picking::MarqueeRect marqueeBounds{
                        static_cast<double>(box.left()),
                        static_cast<double>(box.top()),
                        static_cast<double>(box.right()),
                        static_cast<double>(box.bottom()),
                    };

                    const bool allowFallbackThingRectPick =
                        allowThroughThingPick
                        || thingViewMode_ != ThingViewMode::Shaded;

                    for (std::size_t i = 0; i < level_->things.size(); ++i)
                    {
                        const Thing& thing = level_->things[i];
                        if (!isLayerVisible(thing.layer))
                        {
                            continue;
                        }
                        if (!shouldShowThing(thing))
                        {
                            continue;
                        }
                        if (!std::isfinite(thing.x) || !std::isfinite(thing.y) || !std::isfinite(thing.z))
                        {
                            continue;
                        }

                        const std::int32_t idx32 = static_cast<std::int32_t>(i);
                        const bool alreadyPicked = boxSelection.things.find(idx32) != boxSelection.things.end();
                        if (alreadyPicked)
                        {
                            continue;
                        }

                        bool picked = false;
                        const int pid = (i < thingPickIds_.size()) ? static_cast<int>(thingPickIds_[i]) : 0;
                        std::optional<SpriteBoundsHit> spriteBoundsHit{};
                        const bool overlayLineHit = thingHitsRectByOverlayLines(i);
                        if (overlayLineHit && (allowThroughThingPick || xrayAreaSelection))
                        {
                            picked = true;
                        }

                        if (!picked
                            && (thingViewMode_ == ThingViewMode::Wireframes
                                || thingViewMode_ == ThingViewMode::Boxes))
                        {
                            picked = thingHitsRectByLines(i);
                        }
                        else if (thingViewMode_ == ThingViewMode::Shaded)
                        {
                            // In shaded mode, sprites have no ordinary Thing
                            // triangle range.  Their cached world bounds are
                            // the authoritative marquee geometry fallback.
                            spriteBoundsHit = thingHitsRectBySpriteBounds(i);
                            if (spriteBoundsHit)
                            {
                                // Defer the depth test for all sprite fallbacks
                                // to one batched scene readback below.  A
                                // per-sprite depth FBO pass made marquee
                                // selection scale poorly in sprite-heavy maps.
                                if (!allowThroughThingPick && showSolid_)
                                {
                                    spriteMarqueeCandidates.push_back(SpriteMarqueeCandidate{
                                        idx32,
                                        spriteBoundsHit->depth,
                                        spriteBoundsHit->sample });
                                }
                                else
                                {
                                    picked = true;
                                }
                            }
                            // Missing-triangle things (box/cube fallbacks)
                            // still use the legacy line/point fallback.
                            if (!picked
                                && !spriteBoundsHit
                                && allowFallbackThingRectPick
                                && (!thingHasTriGeometry(pid) || xrayAreaSelection))
                            {
                                picked = thingHitsRectByLines(i);
                            }
                        }

                        if (xrayAreaSelection && !picked)
                        {
                            // Shaded 3DOs normally have no line representation
                            // in the cache. Their cached model bounds are the
                            // conservative projected pick representation for
                            // the through-selection path.
                            picked = thingHitsRectByLines(i);
                            if (!picked && i < spriteThingBounds_.size())
                            {
                                picked = thingBoundsHitRect(spriteThingBounds_[i]);
                            }
                            if (!picked && i < thingBounds_.size())
                            {
                                const std::optional<QRectF> projectedBounds =
                                    thingProjectedBounds(thingBounds_[i]);
                                if (projectedBounds)
                                {
                                    const render::picking::MarqueeRect projectedBoundsValue{
                                        static_cast<double>(projectedBounds->left()),
                                        static_cast<double>(projectedBounds->top()),
                                        static_cast<double>(projectedBounds->right()),
                                        static_cast<double>(projectedBounds->bottom()),
                                    };
                                    const render::picking::MarqueeBoundsRelation relation =
                                        render::picking::classifyMarqueeBounds(
                                            marqueeBounds,
                                            projectedBoundsValue);
                                    if (relation != render::picking::MarqueeBoundsRelation::Disjoint)
                                    {
                                        picked = true;
                                        if (render::picking::marqueeBoundsNeedTriangleRefinement(
                                                relation,
                                                thingHasTriGeometry(pid)))
                                        {
                                            // A partial AABB overlap can be
                                            // an empty corner of a model.
                                            // Refine only that boundary case;
                                            // a fully contained projected
                                            // bounds rectangle is already an
                                            // exact enough broad selection and
                                            // remains cheap for full-screen
                                            // marquees.
                                            picked = thingTrianglesHitRect(pid);
                                        }
                                    }
                                }
                            }
                        }

                        if (!picked && allowFallbackThingRectPick)
                        {
                            QPointF sp{};
                            if (toScreen(QVector3D(static_cast<float>(thing.x), static_cast<float>(thing.y), static_cast<float>(thing.z)), sp)
                                && box.contains(sp))
                            {
                                picked = true;
                            }
                        }

                        if (picked)
                        {
                            boxSelection.things.insert(idx32);
                        }
                    }

                    if (!overlayMarqueeCandidates.empty())
                    {
                        const bool bypassOverlayDepth =
                            allowThroughThingPick || xrayAreaSelection;
                        std::vector<float> sceneDepths{};
                        if (!bypassOverlayDepth)
                        {
                            // Use the same front-face visibility as the point-pick
                            // path so culled backfaces do not block marquee hits.
                            std::vector<QPointF> samples{};
                            samples.reserve(overlayMarqueeCandidates.size());
                            for (const OverlayMarqueeCandidate& candidate
                                 : overlayMarqueeCandidates)
                            {
                                samples.push_back(candidate.sample);
                            }
                            sceneDepths = pickSceneDepthsAtPoints(
                                mvp,
                                samples,
                                showSolid_,
                                showThings_
                                    && thingViewMode_ == ThingViewMode::Shaded,
                                true);
                        }

                        std::unordered_set<std::int32_t> visibleOverlayOwners{};
                        visibleOverlayOwners.reserve(overlayMarqueeCandidates.size());
                        for (std::size_t candidateIndex = 0;
                             candidateIndex < overlayMarqueeCandidates.size();
                             ++candidateIndex)
                        {
                            const OverlayMarqueeCandidate& candidate =
                                overlayMarqueeCandidates[candidateIndex];
                            if (!bypassOverlayDepth
                                && (candidateIndex >= sceneDepths.size()
                                    || !render::picking::marqueeCandidatePassesSceneDepth(
                                        candidate.depth,
                                        sceneDepths[candidateIndex],
                                        false)))
                            {
                                continue;
                            }
                            visibleOverlayOwners.insert(candidate.index);
                        }
                        boxSelection.things.insert(
                            visibleOverlayOwners.begin(),
                            visibleOverlayOwners.end());
                    }

                    if (!spriteMarqueeCandidates.empty())
                    {
                        std::vector<QPointF> samplePoints{};
                        samplePoints.reserve(spriteMarqueeCandidates.size());
                        for (const SpriteMarqueeCandidate& candidate : spriteMarqueeCandidates)
                        {
                            samplePoints.push_back(candidate.sample);
                        }

                        const std::vector<float> sceneDepths = pickSceneDepthsAtPoints(
                            mvp,
                            samplePoints,
                            true,
                            showThings_ && thingViewMode_ == ThingViewMode::Shaded);
                        for (std::size_t i = 0; i < spriteMarqueeCandidates.size(); ++i)
                        {
                            if (i >= sceneDepths.size()
                                || !render::lightGizmoDepthVisible(
                                    spriteMarqueeCandidates[i].depth,
                                    sceneDepths[i]))
                            {
                                continue;
                            }
                            boxSelection.things.insert(spriteMarqueeCandidates[i].index);
                        }
                    }
                }
                else if (mode == PickMode::Light)
                {
                    for (std::size_t i = 0; i < level_->lights.size(); ++i)
                    {
                        const SedLight& light = level_->lights[i];
                        if (!isLayerVisible(light.layer))
                        {
                            continue;
                        }
                        if (!std::isfinite(light.position.x) || !std::isfinite(light.position.y) || !std::isfinite(light.position.z))
                        {
                            continue;
                        }

                        QPointF sp{};
                        if (!toScreen(QVector3D(
                                          static_cast<float>(light.position.x),
                                          static_cast<float>(light.position.y),
                                          static_cast<float>(light.position.z)),
                                      sp))
                        {
                            continue;
                        }

                        if (rect.contains(sp.toPoint()))
                        {
                            boxSelection.lights.insert(static_cast<std::int32_t>(i));
                        }
                    }
                }
                else if (mode == PickMode::Surface)
                {
                    // Face marquee owns polygon overlap, while Edge/Vertex
                    // marquee owns only the projected sub-elements that
                    // actually touch the rectangle.  Keep the owning Surface
                    // refs as wire/strict-scope context, but publish explicit
                    // vertex refs so Vertex-mode Delete and transforms cannot
                    // be mistaken for whole-face operations.
                    struct VertexMarqueeCandidate
                    {
                        editor::SurfaceVertexRef ref{};
                        QPointF screen{};
                        float depth{1.0f};
                    };
                    std::vector<VertexMarqueeCandidate>
                        vertexMarqueeCandidates{};
                    struct EdgeMarqueeCandidate
                    {
                        SurfaceEdgeSelectionRef ref{};
                        QPointF screen{};
                        float depth{1.0f};
                    };
                    std::vector<EdgeMarqueeCandidate>
                        edgeMarqueeCandidates{};
                    struct FaceMarqueeCandidate
                    {
                        SurfaceRef ref{};
                        QPointF screen{};
                        float depth{1.0f};
                    };
                    std::vector<FaceMarqueeCandidate>
                        faceMarqueeCandidates{};

                    const auto faceMarqueeDepthSample = [&] (
                        const Sector& sector,
                        const Surface& surface)
                        -> std::optional<FaceMarqueeCandidate>
                    {
                        struct ProjectedVertex
                        {
                            QPointF screen{};
                            float depth{1.0f};
                        };
                        std::vector<ProjectedVertex> projected{};
                        projected.reserve(surface.vertices.size());
                        for (const std::int32_t vertexIndex : surface.vertices)
                        {
                            if (vertexIndex < 0
                                || static_cast<std::size_t>(vertexIndex)
                                    >= sector.vertices.size())
                            {
                                continue;
                            }
                            const Vertex& vertex = sector.vertices[
                                static_cast<std::size_t>(vertexIndex)];
                            const QVector3D world(
                                static_cast<float>(vertex.x),
                                static_cast<float>(vertex.y),
                                static_cast<float>(vertex.z));
                            QPointF screen{};
                            const QVector4D clip = mvp * QVector4D(world, 1.0f);
                            if (!toScreen(world, screen)
                                || !std::isfinite(clip.z())
                                || !std::isfinite(clip.w())
                                || std::abs(clip.w())
                                    <= std::numeric_limits<float>::epsilon())
                            {
                                continue;
                            }
                            const float depth =
                                (clip.z() / clip.w()) * 0.5f + 0.5f;
                            if (!std::isfinite(depth)
                                || depth < 0.0f || depth > 1.0f)
                            {
                                continue;
                            }
                            projected.push_back(ProjectedVertex{ screen, depth });
                        }
                        if (projected.size() < 3u)
                        {
                            return std::nullopt;
                        }

                        for (const ProjectedVertex& vertex : projected)
                        {
                            if (box.contains(vertex.screen))
                            {
                                return FaceMarqueeCandidate{
                                    {}, vertex.screen, vertex.depth };
                            }
                        }
                        for (std::size_t i = 0; i < projected.size(); ++i)
                        {
                            const ProjectedVertex& a = projected[i];
                            const ProjectedVertex& b = projected[
                                (i + 1u) % projected.size()];
                            const auto sample =
                                render::screenSegmentRectMidpointSample(
                                    a.screen, b.screen, box);
                            if (!sample)
                            {
                                continue;
                            }
                            return FaceMarqueeCandidate{
                                {},
                                sample->point,
                                std::lerp(
                                    a.depth,
                                    b.depth,
                                    static_cast<float>(sample->parameter)) };
                        }

                        std::vector<QPointF> polygon{};
                        polygon.reserve(projected.size());
                        for (const ProjectedVertex& vertex : projected)
                        {
                            polygon.push_back(vertex.screen);
                        }
                        const std::array<QPointF, 5> interiorSamples{
                            box.center(),
                            box.topLeft(),
                            box.topRight(),
                            box.bottomRight(),
                            box.bottomLeft() };
                        for (const QPointF& point : interiorSamples)
                        {
                            if (!render::screenPointInPolygon(point, polygon))
                            {
                                continue;
                            }
                            for (std::size_t i = 1u;
                                 i + 1u < projected.size(); ++i)
                            {
                                const QPointF& a = projected[0].screen;
                                const QPointF& b = projected[i].screen;
                                const QPointF& c = projected[i + 1u].screen;
                                const double denominator =
                                    render::screenPointOrientation(a, b, c);
                                if (std::abs(denominator) <= 1.0e-9)
                                {
                                    continue;
                                }
                                const double wa =
                                    render::screenPointOrientation(b, c, point)
                                    / denominator;
                                const double wb =
                                    render::screenPointOrientation(c, a, point)
                                    / denominator;
                                const double wc = 1.0 - wa - wb;
                                constexpr double kBarycentricTolerance = 1.0e-6;
                                if (wa < -kBarycentricTolerance
                                    || wb < -kBarycentricTolerance
                                    || wc < -kBarycentricTolerance)
                                {
                                    continue;
                                }
                                const float depth = static_cast<float>(
                                    wa * projected[0].depth
                                    + wb * projected[i].depth
                                    + wc * projected[i + 1u].depth);
                                if (std::isfinite(depth))
                                {
                                    return FaceMarqueeCandidate{
                                        {}, point, depth };
                                }
                            }
                        }
                        return std::nullopt;
                    };

                    const auto appendSurfaceMarqueeSelection = [&] (
                        const SurfaceRef ref) -> bool
                    {
                        if (ref.sector < 0 || ref.surface < 0
                            || static_cast<std::size_t>(ref.sector)
                                >= level_->sectors.size())
                        {
                            return false;
                        }
                        const Sector& sector = level_->sectors[
                            static_cast<std::size_t>(ref.sector)];
                        if (static_cast<std::size_t>(ref.surface)
                            >= sector.surfaces.size())
                        {
                            return false;
                        }
                        const Surface& surface = sector.surfaces[
                            static_cast<std::size_t>(ref.surface)];

                        if (surfaceSubMode_ == SurfaceSubMode::Vertex)
                        {
                            bool selectedAny = false;
                            for (std::size_t local = 0;
                                 local < surface.vertices.size(); ++local)
                            {
                                const std::int32_t vertexIndex =
                                    surface.vertices[local];
                                if (vertexIndex < 0
                                    || static_cast<std::size_t>(vertexIndex)
                                        >= sector.vertices.size())
                                {
                                    continue;
                                }
                                const Vertex& vertex = sector.vertices[
                                    static_cast<std::size_t>(vertexIndex)];
                                const QVector3D world(
                                    static_cast<float>(vertex.x),
                                    static_cast<float>(vertex.y),
                                    static_cast<float>(vertex.z));
                                QPointF screen{};
                                if (!toScreen(world, screen)
                                    || !box.contains(screen))
                                {
                                    continue;
                                }
                                const QVector4D clip = mvp * QVector4D(world, 1.0f);
                                if (!std::isfinite(clip.z())
                                    || !std::isfinite(clip.w())
                                    || std::abs(clip.w())
                                        <= std::numeric_limits<float>::epsilon())
                                {
                                    continue;
                                }
                                const float ndcDepth = clip.z() / clip.w();
                                if (!std::isfinite(ndcDepth)
                                    || ndcDepth < -1.0f || ndcDepth > 1.0f)
                                {
                                    continue;
                                }
                                vertexMarqueeCandidates.push_back(
                                    VertexMarqueeCandidate{
                                        editor::SurfaceVertexRef{
                                            ref,
                                            static_cast<std::int32_t>(local) },
                                        screen,
                                        ndcDepth * 0.5f + 0.5f });
                                selectedAny = true;
                            }
                            return selectedAny;
                        }

                        if (surfaceSubMode_ == SurfaceSubMode::Edge)
                        {
                            bool selectedAny = false;
                            for (std::size_t edge = 0;
                                 edge < surface.vertices.size(); ++edge)
                            {
                                const std::int32_t first = surface.vertices[edge];
                                const std::int32_t second = surface.vertices[
                                    (edge + 1) % surface.vertices.size()];
                                if (first < 0 || second < 0
                                    || static_cast<std::size_t>(first)
                                        >= sector.vertices.size()
                                    || static_cast<std::size_t>(second)
                                        >= sector.vertices.size())
                                {
                                    continue;
                                }
                                const Vertex& a = sector.vertices[
                                    static_cast<std::size_t>(first)];
                                const Vertex& b = sector.vertices[
                                    static_cast<std::size_t>(second)];
                                QPointF screenA{};
                                QPointF screenB{};
                                const QVector3D worldA(
                                    static_cast<float>(a.x),
                                    static_cast<float>(a.y),
                                    static_cast<float>(a.z));
                                const QVector3D worldB(
                                    static_cast<float>(b.x),
                                    static_cast<float>(b.y),
                                    static_cast<float>(b.z));
                                const QVector4D clipA = mvp * QVector4D(worldA, 1.0f);
                                const QVector4D clipB = mvp * QVector4D(worldB, 1.0f);
                                const bool validDepth = std::isfinite(clipA.z())
                                    && std::isfinite(clipA.w())
                                    && std::isfinite(clipB.z())
                                    && std::isfinite(clipB.w())
                                    && std::abs(clipA.w())
                                        > std::numeric_limits<float>::epsilon()
                                    && std::abs(clipB.w())
                                        > std::numeric_limits<float>::epsilon();
                                const auto sample = toScreen(worldA, screenA)
                                    && toScreen(worldB, screenB)
                                    && validDepth
                                    ? render::screenSegmentRectMidpointSample(
                                          screenA, screenB, box)
                                    : std::nullopt;
                                if (sample)
                                {
                                    const float depthA =
                                        (clipA.z() / clipA.w()) * 0.5f + 0.5f;
                                    const float depthB =
                                        (clipB.z() / clipB.w()) * 0.5f + 0.5f;
                                    const float candidateDepth = std::lerp(
                                        depthA,
                                        depthB,
                                        static_cast<float>(sample->parameter));
                                    if (!std::isfinite(candidateDepth)
                                        || candidateDepth < 0.0f
                                        || candidateDepth > 1.0f)
                                    {
                                        continue;
                                    }
                                    boxSelection.surfaces.insert(ref);
                                    edgeMarqueeCandidates.push_back(
                                        EdgeMarqueeCandidate{
                                            SurfaceEdgeSelectionRef{
                                                ref,
                                                static_cast<std::int32_t>(edge),
                                                first,
                                                second },
                                            sample->point,
                                            candidateDepth });
                                    selectedAny = true;
                                }
                            }
                            return selectedAny;
                        }

                        boxSelection.surfaces.insert(ref);
                        return true;
                    };

                    const bool canGpuRectPick =
                        !allowThroughSurfacePick
                        && glReady_
                        && pickProgram_
                        && triVao_
                        && triVertexCount_ > 0;

                    if (canGpuRectPick)
                    {
                        const auto pickedSurfaces = pickSurfacesInScreenRect(
                            mvp,
                            rect.left(),
                            rect.top(),
                            rect.right(),
                            rect.bottom(),
                            static_cast<int>(TGeoMode::Solid),
                            true);

                        for (const SurfaceRef ref : pickedSurfaces)
                        {
                            if (strictSurfaceSelectionEnabled_
                                && (surfaceSubMode_ == SurfaceSubMode::Edge
                                    || surfaceSubMode_ == SurfaceSubMode::Vertex)
                                && !strictSurfaceSelectionScope_.contains(ref))
                            {
                                continue;
                            }
                            (void)appendSurfaceMarqueeSelection(ref);
                        }
                    }
                    else
                    {
                        for (std::size_t secIndex = 0; secIndex < level_->sectors.size(); ++secIndex)
                        {
                            const Sector& sec = level_->sectors[secIndex];
                            if (!isLayerVisible(sec.layer))
                            {
                                continue;
                            }
                            if (sec.surfaces.empty() || sec.vertices.empty())
                            {
                                continue;
                            }

                            for (std::size_t sfIndex = 0; sfIndex < sec.surfaces.size(); ++sfIndex)
                            {
                                const SurfaceRef ref{
                                    static_cast<std::int32_t>(secIndex),
                                    static_cast<std::int32_t>(sfIndex) };
                                if (strictSurfaceSelectionEnabled_
                                    && (surfaceSubMode_ == SurfaceSubMode::Edge
                                        || surfaceSubMode_ == SurfaceSubMode::Vertex)
                                    && !strictSurfaceSelectionScope_.contains(ref))
                                {
                                    continue;
                                }
                                const Surface& surf = sec.surfaces[sfIndex];
                                if (surf.vertices.size() < 3)
                                {
                                    continue;
                                }
                                if (!allowThroughSurfacePick
                                    && showSolid_
                                    && surf.geo < static_cast<std::int32_t>(TGeoMode::Solid))
                                {
                                    continue;
                                }
                                if (!allowThroughSurfacePick
                                    && !render::surfaceFacesSelectionCamera(sec, surf, eye, target, orthographicCamera))
                                {
                                    continue;
                                }

                                if (surfaceSubMode_ == SurfaceSubMode::Face)
                                {
                                    const std::vector<QPointF> poly =
                                        render::projectSurfacePolygonToScreen(
                                            sec, surf, mvp, width(), height());
                                    if (render::screenPolygonIntersectsRect(
                                            poly, box))
                                    {
                                        if (allowThroughSurfacePick)
                                        {
                                            (void)appendSurfaceMarqueeSelection(ref);
                                        }
                                        else if (auto candidate =
                                                     faceMarqueeDepthSample(sec, surf))
                                        {
                                            candidate->ref = ref;
                                            faceMarqueeCandidates.push_back(*candidate);
                                        }
                                    }
                                }
                                else
                                {
                                    (void)appendSurfaceMarqueeSelection(ref);
                                }
                            }
                        }
                    }

                    if (surfaceSubMode_ == SurfaceSubMode::Vertex
                        && !vertexMarqueeCandidates.empty())
                    {
                        std::vector<QPointF> samplePoints{};
                        if (!allowThroughSurfacePick)
                        {
                            samplePoints.reserve(
                                vertexMarqueeCandidates.size());
                            for (const VertexMarqueeCandidate& candidate :
                                 vertexMarqueeCandidates)
                            {
                                samplePoints.push_back(candidate.screen);
                            }
                        }
                        const std::vector<float> sceneDepths =
                            allowThroughSurfacePick
                            ? std::vector<float>{}
                            : pickSceneDepthsAtPoints(
                                  mvp,
                                  samplePoints,
                                  showSolid_,
                                  showThings_
                                      && thingViewMode_
                                          == ThingViewMode::Shaded,
                                  true);
                        for (std::size_t candidateIndex = 0;
                             candidateIndex < vertexMarqueeCandidates.size();
                             ++candidateIndex)
                        {
                            const VertexMarqueeCandidate& candidate =
                                vertexMarqueeCandidates[candidateIndex];
                            if (!allowThroughSurfacePick
                                && (candidateIndex >= sceneDepths.size()
                                    || !render::picking::marqueeCandidatePassesSceneDepth(
                                            candidate.depth,
                                            sceneDepths[candidateIndex],
                                            false)))
                            {
                                continue;
                            }
                            boxSelection.surfaceVertices.insert(candidate.ref);
                            boxSelection.surfaces.insert(candidate.ref.surface);
                        }
                    }
                    if (surfaceSubMode_ == SurfaceSubMode::Face
                        && !faceMarqueeCandidates.empty())
                    {
                        std::vector<QPointF> samplePoints{};
                        samplePoints.reserve(faceMarqueeCandidates.size());
                        for (const FaceMarqueeCandidate& candidate :
                             faceMarqueeCandidates)
                        {
                            samplePoints.push_back(candidate.screen);
                        }
                        const std::vector<float> sceneDepths =
                            pickSceneDepthsAtPoints(
                                mvp,
                                samplePoints,
                                showSolid_,
                                showThings_
                                    && thingViewMode_
                                        == ThingViewMode::Shaded,
                                true);
                        for (std::size_t candidateIndex = 0;
                             candidateIndex < faceMarqueeCandidates.size();
                             ++candidateIndex)
                        {
                            const FaceMarqueeCandidate& candidate =
                                faceMarqueeCandidates[candidateIndex];
                            if (candidateIndex >= sceneDepths.size()
                                || !render::picking::surfaceSelectionCandidatePassesSceneDepth(
                                    candidate.depth,
                                    sceneDepths[candidateIndex],
                                    false,
                                    false))
                            {
                                continue;
                            }
                            boxSelection.surfaces.insert(candidate.ref);
                        }
                    }
                    if (surfaceSubMode_ == SurfaceSubMode::Edge)
                    {
                        std::vector<SurfaceEdgeSelectionRef> visibleEdges{};
                        visibleEdges.reserve(edgeMarqueeCandidates.size());
                        std::vector<QPointF> samplePoints{};
                        if (!allowThroughSurfacePick)
                        {
                            samplePoints.reserve(edgeMarqueeCandidates.size());
                            for (const EdgeMarqueeCandidate& candidate :
                                 edgeMarqueeCandidates)
                            {
                                samplePoints.push_back(candidate.screen);
                            }
                        }
                        const std::vector<float> sceneDepths =
                            allowThroughSurfacePick
                            ? std::vector<float>{}
                            : pickSceneDepthsAtPoints(
                                  mvp,
                                  samplePoints,
                                  showSolid_,
                                  showThings_
                                      && thingViewMode_
                                          == ThingViewMode::Shaded,
                                  true);
                        for (std::size_t candidateIndex = 0;
                             candidateIndex < edgeMarqueeCandidates.size();
                             ++candidateIndex)
                        {
                            const EdgeMarqueeCandidate& candidate =
                                edgeMarqueeCandidates[candidateIndex];
                            if (!allowThroughSurfacePick
                                && (candidateIndex >= sceneDepths.size()
                                    || !render::picking::surfaceSelectionCandidatePassesSceneDepth(
                                        candidate.depth,
                                        sceneDepths[candidateIndex],
                                        false,
                                        false)))
                            {
                                continue;
                            }
                            visibleEdges.push_back(candidate.ref);
                        }
                        applySurfaceEdgeBoxSelection(
                            visibleEdges, boxMode);
                        boxSelection.surfaces = selectedSurfaceEdgeOwners();
                    }
                }
                else if (mode == PickMode::Sector)
                {
                    const bool canGpuRectPick =
                        !allowThroughSurfacePick
                        && glReady_
                        && pickProgram_
                        && triVao_
                        && triVertexCount_ > 0;

                    if (canGpuRectPick)
                    {
                        const auto pickedSurfaces = pickSurfacesInScreenRect(
                            mvp,
                            rect.left(),
                            rect.top(),
                            rect.right(),
                            rect.bottom(),
                            static_cast<int>(TGeoMode::Solid),
                            true);

                        for (const SurfaceRef ref : pickedSurfaces)
                        {
                            boxSelection.sectors.insert(ref.sector);
                        }
                    }
                    else
                    {
                        for (std::size_t secIndex = 0; secIndex < level_->sectors.size(); ++secIndex)
                        {
                            const Sector& sec = level_->sectors[secIndex];
                            if (!isLayerVisible(sec.layer))
                            {
                                continue;
                            }
                            if (sec.vertices.empty())
                            {
                                continue;
                            }

                            bool pickedSector = false;
                            for (const Surface& surf : sec.surfaces)
                            {
                                if (surf.vertices.size() < 3)
                                {
                                    continue;
                                }
                                if (!allowThroughSurfacePick
                                    && showSolid_
                                    && surf.geo < static_cast<std::int32_t>(TGeoMode::Solid))
                                {
                                    continue;
                                }
                                if (!allowThroughSurfacePick
                                    && !render::surfaceFacesSelectionCamera(sec, surf, eye, target, orthographicCamera))
                                {
                                    continue;
                                }

                                const std::vector<QPointF> poly =
                                    render::projectSurfacePolygonToScreen(sec, surf, mvp, width(), height());
                                if (!render::screenPolygonIntersectsRect(poly, box))
                                {
                                    continue;
                                }

                                pickedSector = true;
                                break;
                            }

                            if (pickedSector)
                            {
                                boxSelection.sectors.insert(static_cast<std::int32_t>(secIndex));
                            }
                        }
                    }
                }
            }

            Q_EMIT selectionBoxPicked(boxSelection, boxMode);
            update();
            event->accept();
            return;
        }

        update();
    }

    if (dragDelta.manhattanLength() <= 2)
    {
        const bool previewAutoSelection =
            renderPipeline_ == RenderPipeline::GamePreview
            && surfaceClickSelectionEnabled_;
        std::int32_t newSector = -1;
        std::int32_t newSurface = -1;
        std::int32_t newThing = -1;
        std::int32_t newLight = -1;
        std::optional<sed::level::SurfacePickHit> surfaceHit{};
        std::optional<QVector3D> clickedWorldPos{};

        if (level_ && width() > 0 && height() > 0)
        {
            QMatrix4x4 proj;
            QMatrix4x4 view;
            QVector3D eye;
            QVector3D target;
            float nearPlane{};
            float farPlane{};
            buildCameraMatrices(proj, view, eye, target, nearPlane, farPlane);

            const QMatrix4x4 mvp = proj * view;

            // Explicit Light-family modes pick the depth-tested category icon
            // itself, not arbitrary Things or sector walls. Raw pickMode_ is
            // intentional: an Auto preference outside Light edit mode must
            // retain the normal mixed-selection path below.
            std::optional<render::LightGizmoCategory> pickedLightCategory{};
            if (!previewAutoSelection
                && surfaceClickSelectionEnabled_
                && !cogEntityPickHighlightEnabled_)
            {
                if (pickMode_ == PickMode::Light
                    && lightGizmoCategory_ == render::LightGizmoCategory::Baked)
                {
                    pickedLightCategory = render::LightGizmoCategory::Baked;
                }
                else if (pickMode_ == PickMode::Thing
                    && lightGizmoCategory_ == render::LightGizmoCategory::Thing)
                {
                    pickedLightCategory = render::LightGizmoCategory::Thing;
                }
                else if (pickMode_ == PickMode::Sector
                    && lightGizmoCategory_ == render::LightGizmoCategory::Sector)
                {
                    pickedLightCategory = render::LightGizmoCategory::Sector;
                }
            }
            if (pickedLightCategory)
            {
                std::vector<render::LightGizmoEntry> entries{};
                switch (*pickedLightCategory)
                {
                    case render::LightGizmoCategory::Baked:
                        entries = render::bakedLightGizmoEntries(*level_);
                        break;
                    case render::LightGizmoCategory::Thing:
                        entries = render::thingLightGizmoEntries(*level_, templates_);
                        break;
                    case render::LightGizmoCategory::Sector:
                        entries = render::sectorLightGizmoEntries(*level_);
                        break;
                    case render::LightGizmoCategory::Architectural:
                        break;
                }
                bool invViewOk = false;
                const QMatrix4x4 invViewForLightIcons = view.inverted(&invViewOk);
                QVector3D lightIconRight(
                    invViewForLightIcons(0, 0),
                    invViewForLightIcons(1, 0),
                    invViewForLightIcons(2, 0));
                QVector3D lightIconUp(
                    invViewForLightIcons(0, 1),
                    invViewForLightIcons(1, 1),
                    invViewForLightIcons(2, 1));
                if (!invViewOk)
                {
                    lightIconRight = QVector3D(1.0f, 0.0f, 0.0f);
                    lightIconUp = QVector3D(0.0f, 0.0f, 1.0f);
                }
                const float lightIconHalfSize =
                    std::clamp(lightGizmoWorldSize_, 0.001f, 1.0f) * 0.5f;
                for (render::LightGizmoEntry& entry : entries)
                {
                    entry.position = lightGizmoDisplayPositionForEntry(
                        entry,
                        lightIconUp,
                        lightIconHalfSize);
                }
                std::erase_if(entries, [this](const render::LightGizmoEntry& entry)
                {
                    return !isLayerVisible(entry.layer);
                });

                const std::vector<float> sceneDepths = pickSceneDepthsAtPoints(
                    mvp,
                    std::vector<QPointF>{ QPointF(releasePos) },
                    showSolid_,
                    showThings_ && thingViewMode_ == ThingViewMode::Shaded,
                    true);
                const float sceneDepth = sceneDepths.empty() ? 1.0f : sceneDepths.front();

                const Qt::KeyboardModifiers lightClickModifiers = event->modifiers()
                    & (Qt::ShiftModifier | Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier);
                const bool lightXrayCycleEligible =
                    ui::shortcuts::selectionModifiersAllowXrayCycle(
                        lightClickModifiers,
                        selectionAddModifiers_);
                const bool repeatedLightPickContext = continuePickCycle(
                    releasePos,
                    pickMode_,
                    lightClickModifiers);
                const bool lightCyclePick = repeatedLightPickContext
                    && lightXrayCycleEligible;
                const bool selectedLightFamilyCanAdvance = [&]() -> bool
                {
                    if (!xraySelectionCyclingEnabled_
                        || !lightXrayCycleEligible)
                    {
                        return false;
                    }
                    switch (*pickedLightCategory)
                    {
                        case render::LightGizmoCategory::Baked:
                            return selectedLight_ >= 0 || !highlightedSelection_.lights.empty();
                        case render::LightGizmoCategory::Thing:
                            return selectedThing_ >= 0 || !highlightedSelection_.things.empty();
                        case render::LightGizmoCategory::Sector:
                            return selectedSector_ >= 0 || !highlightedSelection_.sectors.empty();
                        case render::LightGizmoCategory::Architectural:
                            return !highlightedSelection_.things.empty();
                    }
                    return false;
                }();
                const float lightPickDepthLimit =
                    xraySelectionCyclingEnabled_
                        && (lightCyclePick || selectedLightFamilyCanAdvance)
                        ? 1.0f
                        : sceneDepth;

                const std::vector<render::LightGizmoScreenPick> categoryPicks = render::pickLightGizmoEntries(
                    entries,
                    mvp,
                    width(),
                    height(),
                    QPointF(releasePos),
                    15.0f,
                    lightPickDepthLimit,
                    lightIconRight,
                    lightIconUp,
                    lightIconHalfSize);
                std::optional<render::LightGizmoScreenPick> categoryPick{};
                if (!categoryPicks.empty())
                {
                    const bool samePickLocation = haveLastLightGizmoPick_
                        && lastLightGizmoPickCategory_ == *pickedLightCategory
                        && (lastLightGizmoPickPosition_ - releasePos).manhattanLength() <= 4
                        && lightCyclePick;
                    const std::int32_t currentOwner =
                        lightGizmoCategory_ == render::LightGizmoCategory::Baked
                            ? selectedLight_
                            : (lightGizmoCategory_ == render::LightGizmoCategory::Thing
                                ? selectedThing_
                                : selectedSector_);
                    const bool frontOwnerSelected = [&]() -> bool
                    {
                        const std::int32_t owner = categoryPicks.front().ownerIndex;
                        switch (*pickedLightCategory)
                        {
                            case render::LightGizmoCategory::Baked:
                                return owner >= 0
                                    && (selectedLight_ == owner
                                        || highlightedSelection_.lights.contains(owner));
                            case render::LightGizmoCategory::Thing:
                                return owner >= 0
                                    && (selectedThing_ == owner
                                        || highlightedSelection_.things.contains(owner));
                            case render::LightGizmoCategory::Sector:
                                return owner >= 0
                                    && (selectedSector_ == owner
                                        || highlightedSelection_.sectors.contains(owner));
                            case render::LightGizmoCategory::Architectural:
                                return owner >= 0
                                    && (selectedThing_ == owner
                                        || highlightedSelection_.things.contains(owner));
                        }
                        return false;
                    }();
                    const bool skipSelectedFront =
                        render::picking::shouldSkipSelectedFrontCandidate(
                            xraySelectionCyclingEnabled_,
                            lightXrayCycleEligible,
                            frontOwnerSelected);
                    const std::size_t pickIndex = samePickLocation
                        ? render::lightGizmoPickIndex(
                              categoryPicks,
                              true,
                              currentOwner,
                              selectedLightGizmoKind_).value_or(0u)
                        : (skipSelectedFront && categoryPicks.size() > 1u ? 1u : 0u);
                    categoryPick = categoryPicks[pickIndex];
                }
                else if (*pickedLightCategory == render::LightGizmoCategory::Thing
                    && showThings_
                    && thingViewMode_ == ThingViewMode::Shaded)
                {
                    // A regular Thing light is an attached property rather
                    // than an independent scene object. Let the owner's
                    // visible 3DO/sprite body select that regular emitter as
                    // an alternative to clicking its lifted icon. Headlights
                    // remain icon-specific so an owner exposing both sources
                    // is never ambiguous.
                    const std::unordered_set<std::int32_t> ownerHits = pickThingsInScreenRect(
                        mvp,
                        releasePos.x(),
                        releasePos.y(),
                        releasePos.x(),
                        releasePos.y(),
                        showSolid_);
                    std::vector<std::int32_t> orderedOwners(ownerHits.begin(), ownerHits.end());
                    std::ranges::sort(orderedOwners);
                    for (const std::int32_t owner : orderedOwners)
                    {
                        const auto regular = std::find_if(
                            entries.begin(),
                            entries.end(),
                            [owner](const render::LightGizmoEntry& entry)
                            {
                                return entry.ownerIndex == owner
                                    && entry.kind == render::LightGizmoKind::ThingRegular;
                            });
                        if (regular == entries.end())
                        {
                            continue;
                        }
                        categoryPick = render::LightGizmoScreenPick{
                            static_cast<std::size_t>(std::distance(entries.begin(), regular)),
                            regular->ownerIndex,
                            regular->kind,
                            0.0f,
                            0.0f,
                        };
                        break;
                    }
                }
                if (categoryPick)
                {
                    selectedLightGizmoKind_ = categoryPick->kind;
                    setProperty(
                        "sed.selectedLightGizmoKind",
                        static_cast<int>(selectedLightGizmoKind_));
                    lastLightGizmoPickPosition_ = releasePos;
                    lastLightGizmoPickCategory_ = *pickedLightCategory;
                    haveLastLightGizmoPick_ = true;
                }
                else
                {
                    haveLastLightGizmoPick_ = false;
                    setProperty("sed.selectedLightGizmoKind", QVariant{});
                }
                const std::int32_t categorySector =
                    categoryPick && *pickedLightCategory == render::LightGizmoCategory::Sector
                        ? categoryPick->ownerIndex
                        : -1;
                const std::int32_t categoryThing =
                    categoryPick && *pickedLightCategory == render::LightGizmoCategory::Thing
                        ? categoryPick->ownerIndex
                        : -1;
                const std::int32_t categoryLight =
                    categoryPick && *pickedLightCategory == render::LightGizmoCategory::Baked
                        ? categoryPick->ownerIndex
                        : -1;

                Q_EMIT clickPicked(categorySector, -1, categoryThing, categoryLight);
                setSelection(categorySector, -1, categoryThing, categoryLight);
                Q_EMIT selectionPicked(
                    categorySector,
                    -1,
                    categoryThing,
                    categoryLight,
                    event->modifiers());
                event->accept();
                return;
            }

            // Thing-frame/path edit: allow picking frame points (without changing normal object selection).
            if (!previewAutoSelection
                && surfaceClickSelectionEnabled_
                && frameEditEnabled_
                && selectedThing_ >= 0
                && static_cast<std::size_t>(selectedThing_) < level_->things.size())
            {
                const Thing& thing = level_->things[static_cast<std::size_t>(selectedThing_)];

                constexpr float kPickTolPx = 10.0f;
                const float tol2 = kPickTolPx * kPickTolPx;

                const QPointF mp(releasePos);
                float bestD2 = tol2;
                std::int32_t bestValueIndex = -1;

                const auto considerFramePose = [&](
                    const std::int32_t valueIndex,
                    const double x,
                    const double y,
                    const double z)
                {
                    const ProjectedPoint point = projectWorldToWidget(
                        mvp,
                        QVector3D(
                            static_cast<float>(x),
                            static_cast<float>(y),
                            static_cast<float>(z)),
                        width(),
                        height());
                    if ( !point.valid )
                    {
                        return;
                    }

                    const QPointF delta = mp - point.pos;
                    const float distanceSquared = static_cast<float>(
                        (delta.x() * delta.x()) + (delta.y() * delta.y()));
                    if ( distanceSquared < bestD2 )
                    {
                        bestD2 = distanceSquared;
                        bestValueIndex = valueIndex;
                    }
                };

                if ( isLayerVisible(thing.layer) && shouldShowThing(thing) )
                {
                    FrameOwnerPoseCache* const ownerCache =
                        ensureFrameOwnerPoseCache(selectedThing_);
                    if ( ownerCache )
                    {
                        bool selectedPoseInPrefix = false;
                        for ( const render::frame_ghost::FramePose& pose :
                            ownerCache->poses )
                        {
                            considerFramePose(
                                pose.valueIndex,
                                pose.x,
                                pose.y,
                                pose.z);
                            selectedPoseInPrefix = selectedPoseInPrefix
                                || pose.valueIndex == selectedFrameValueIndex_;
                        }

                        if ( selectedFrameValueIndex_ >= 0
                            && !selectedPoseInPrefix )
                        {
                            if ( const render::frame_ghost::FramePose* selectedPose =
                                    findFramePose(
                                        selectedThing_,
                                        selectedFrameValueIndex_) )
                            {
                                considerFramePose(
                                    selectedPose->valueIndex,
                                    selectedPose->x,
                                    selectedPose->y,
                                    selectedPose->z);
                            }
                        }
                    }
                }

                if (bestValueIndex >= 0)
                {
                    selectedFrameValueIndex_ = bestValueIndex;
                    Q_EMIT framePicked(selectedThing_, bestValueIndex);
                    update();
                    event->accept();
                    return;
                }
            }

            bool invertible = false;
            const QMatrix4x4 inv = mvp.inverted(&invertible);
            if (invertible)
            {
                const float ndcX = (2.0f * static_cast<float>(releasePos.x()) / static_cast<float>(width())) - 1.0f;
                const float ndcY = 1.0f - (2.0f * static_cast<float>(releasePos.y()) / static_cast<float>(height()));

                auto toAffine = [](const QVector4D& v) -> QVector3D
                {
                    if (qFuzzyIsNull(v.w()))
                    {
                        return QVector3D(0.0f, 0.0f, 0.0f);
                    }
                    return QVector3D(v.x() / v.w(), v.y() / v.w(), v.z() / v.w());
                };

                const QVector3D nearWorld = toAffine(inv * QVector4D(ndcX, ndcY, -1.0f, 1.0f));
                const QVector3D farWorld = toAffine(inv * QVector4D(ndcX, ndcY, 1.0f, 1.0f));

                const bool isOrtho = camera_.projectionType() == app::ViewportCamera::ProjectionType::Orthographic;
                const QVector3D origin3 = isOrtho ? nearWorld : eye;

                QVector3D rayDir = farWorld - origin3;
                const double cpuRayMaxDistance = render::picking::
                    pickRaySegmentMaxDistance(
                        static_cast<double>(rayDir.x()),
                        static_cast<double>(rayDir.y()),
                        static_cast<double>(rayDir.z()));
                if (cpuRayMaxDistance > 1.0e-5)
                {
                    rayDir.normalize();

                    // Fallback placement point when no surface is hit: a point on the click ray near the camera.
                    {
                        QVector3D p{};
                        if (isOrtho)
                        {
                            // In ortho, `nearWorld` can be very far from the orbit target due to large near planes.
                            // Place new objects on the view plane that goes through the orbit target so they appear
                            // at the clicked screen position.
                            const float t = QVector3D::dotProduct(target - origin3, rayDir);
                            p = origin3 + rayDir * t;
                        }
                        else
                        {
                            // When callers want "air placement" (e.g. add cube/light), keep the point closer to
                            // the camera so new objects appear immediately in view.
                            const float distScale = surfaceClickPreferSurfaceHit_ ? 0.15f : 0.06f;
                            const float distMax = surfaceClickPreferSurfaceHit_ ? 16.0f : 6.0f;
                            const float dist = std::clamp(camera_.orbit.distance * distScale, 0.25f, distMax);
                            p = origin3 + rayDir * dist;
                        }
                        if (std::isfinite(p.x()) && std::isfinite(p.y()) && std::isfinite(p.z()))
                        {
                            clickedWorldPos = p;
                        }
                    }

                    const sed::TVector origin{ static_cast<double>(origin3.x()), static_cast<double>(origin3.y()), static_cast<double>(origin3.z()) };
                    const sed::TVector direction{ static_cast<double>(rayDir.x()), static_cast<double>(rayDir.y()), static_cast<double>(rayDir.z()) };

                    const Qt::KeyboardModifiers clickMods = event->modifiers();
                    const Qt::KeyboardModifiers kSelectionModMask =
                        Qt::ShiftModifier | Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier;
                    const Qt::KeyboardModifiers heldMods = clickMods & kSelectionModMask;
                    auto selectionModifierActive = [heldMods](
                        const ui::shortcuts::ModifierBindings& required) -> bool
                    {
                        return ui::shortcuts::modifierBindingsActive(heldMods, required);
                    };

                    const bool bypassPickFilters =
                        selectionModifierActive(selectionSurfaceBypassFiltersModifiers_);
                    // Filter bypass itself remains inclusive (for example a
                    // combined remove+bypass operation). Only the exact
                    // configured Ignore Filters chord may advance a Surface
                    // point-pick cycle.
                    const bool surfaceIgnoreFiltersCycleClick =
                        ui::shortcuts::modifierBindingsExact(
                            heldMods,
                            selectionSurfaceBypassFiltersModifiers_);
                    const bool renderThroughPickMode = !showSolid_;
                    const bool allowThroughSurfacePick = bypassPickFilters || renderThroughPickMode;
                    const bool allowThroughThingPick = bypassPickFilters || renderThroughPickMode;
                    const bool wireXraySurfacePick = renderThroughPickMode;
                    const bool wireVisible = render::picking::surfaceWirePickingEnabled(
                        showSolid_, showVertices_, showWireframe_);

                    PickMode mode = previewAutoSelection
                        ? PickMode::Auto
                        : pickMode_;
                    if (!previewAutoSelection && mode == PickMode::Auto)
                    {
                        switch (pickPreference_)
                        {
                            case PickPreference::PreferSurface: mode = PickMode::Surface; break;
                            case PickPreference::PreferThing: mode = PickMode::Thing; break;
                            case PickPreference::PreferLight: mode = PickMode::Light; break;
                            default: break;
                        }
                    }
                    const render::picking::SolidLevelOcclusionPickFamily
                        solidLevelOcclusionFamily = [&]()
                    {
                        switch ( mode )
                        {
                            case PickMode::Sector:
                                return render::picking::
                                    SolidLevelOcclusionPickFamily::Sector;
                            case PickMode::Thing:
                                return render::picking::
                                    SolidLevelOcclusionPickFamily::Thing;
                            case PickMode::Auto:
                                return render::picking::
                                    SolidLevelOcclusionPickFamily::PreviewAuto;
                            default:
                                return render::picking::
                                    SolidLevelOcclusionPickFamily::Surface;
                        }
                    }();

                    const bool xrayCycleSelectionClick =
                        ui::shortcuts::selectionModifiersAllowXrayCycle(
                            heldMods,
                            selectionAddModifiers_);
                    // Always update/reset the click context. Plain clicks and
                    // the exact Add chord can cycle. Surface Ignore Filters is
                    // itself an explicit through-pick operation, so repeated
                    // clicks with that exact chord also walk back-facing/coplanar
                    // owners. Remove and path chords retain their exact target.
                    const bool repeatedPickContext = continuePickCycle(releasePos, mode, heldMods);
                    const bool cyclePick =
                        render::picking::pointSelectionClickCanCycle(
                            repeatedPickContext,
                            previewAutoSelection,
                            xrayCycleSelectionClick,
                            mode == PickMode::Surface,
                            surfaceIgnoreFiltersCycleClick);
                    const auto sectorIsSelected = [&](const std::int32_t sectorIndex) -> bool
                    {
                        return sectorIndex >= 0
                            && render::picking::candidateBelongsToSelection(
                                sectorIndex,
                                selectedSector_,
                                highlightedSelection_.sectors);
                    };
                    const auto surfaceIsSelected = [&](const SurfaceRef ref) -> bool
                    {
                        if (ref.sector < 0 || ref.surface < 0)
                        {
                            return false;
                        }
                        if (render::picking::candidateBelongsToSelection(
                                ref,
                                SurfaceRef{ selectedSurfaceSector_, selectedSurface_ },
                                highlightedSelection_.surfaces))
                        {
                            return true;
                        }
                        return std::any_of(
                            highlightedSelection_.surfaceVertices.begin(),
                            highlightedSelection_.surfaceVertices.end(),
                            [&](const editor::SurfaceVertexRef& vertex)
                            {
                                return vertex.surface.sector == ref.sector
                                    && vertex.surface.surface == ref.surface;
                            });
                    };
                    const auto thingIsSelected = [&](const std::int32_t thingIndex) -> bool
                    {
                        return thingIndex >= 0
                            && render::picking::candidateBelongsToSelection(
                                thingIndex,
                                selectedThing_,
                                highlightedSelection_.things);
                    };
                    const auto lightIsSelected = [&](const std::int32_t lightIndex) -> bool
                    {
                        return lightIndex >= 0
                            && render::picking::candidateBelongsToSelection(
                                lightIndex,
                                selectedLight_,
                                highlightedSelection_.lights);
                    };

                    // A first unmodified X-ray click may need one deeper candidate when the
                    // visible front entity is already part of the complete selection. Probe
                    // only when the active edit family has a selection; explicit modifier
                    // clicks retain their exact add/remove/path target.
                    const bool selectedFamilyCanAdvance = [&]() -> bool
                    {
                        if (!xraySelectionCyclingEnabled_ || !xrayCycleSelectionClick)
                        {
                            return false;
                        }
                        switch (mode)
                        {
                            case PickMode::Sector:
                                return selectedSector_ >= 0 || !highlightedSelection_.sectors.empty();
                            case PickMode::Surface:
                                return (selectedSurfaceSector_ >= 0 && selectedSurface_ >= 0)
                                    || !highlightedSelection_.surfaces.empty()
                                    || !highlightedSelection_.surfaceVertices.empty();
                            case PickMode::Thing:
                                return selectedThing_ >= 0 || !highlightedSelection_.things.empty();
                            case PickMode::Light:
                                return selectedLight_ >= 0 || !highlightedSelection_.lights.empty();
                            case PickMode::Auto:
                                return selectedSector_ >= 0
                                    || (selectedSurfaceSector_ >= 0 && selectedSurface_ >= 0)
                                    || selectedThing_ >= 0
                                    || selectedLight_ >= 0
                                    || !highlightedSelection_.sectors.empty()
                                    || !highlightedSelection_.surfaces.empty()
                                    || !highlightedSelection_.surfaceVertices.empty()
                                    || !highlightedSelection_.things.empty()
                                    || !highlightedSelection_.lights.empty();
                            default:
                                return false;
                        }
                    }();
                    const bool xrayCandidateTraversal = cyclePick || selectedFamilyCanAdvance;
                    const bool xrayCyclePick = render::picking::shouldTraverseOccludedCandidates(
                        xraySelectionCyclingEnabled_,
                        xrayCandidateTraversal);
                    // X-ray cycling broadens occlusion traversal only. Surface geometry/back-face eligibility
                    // remains governed by the existing render mode and selection-bypass modifier.
                    const bool collectThroughSurfaceOcclusion =
                        allowThroughSurfacePick || xrayCyclePick;
                    const bool collectThroughThingOcclusion = allowThroughThingPick || xrayCyclePick;
                    const bool previewBakedLightPickable = !previewAutoSelection
                        || alwaysShowLightGizmoCategories_[static_cast<std::size_t>(
                            render::LightGizmoCategory::Baked)];

                    struct OverlayThingPointCandidate
                    {
                        std::int32_t index{ -1 };
                        QPointF sample{};
                        float depth{ 1.0f };
                        double distanceSquared{};
                    };
                    std::vector<OverlayThingPointCandidate> overlayThingCandidates{};
                    const bool overlayThingPickMode = mode == PickMode::Thing
                        || mode == PickMode::Auto;
                    if (overlayThingPickMode
                        && (showThingCollisionShapes_ || showThingVisualBounds_)
                        && thingMeshCache_.valid
                        && !thingMeshCache_.overlayLineVertices.empty())
                    {
                        constexpr double kOverlayPickRadiusPx = 8.0;
                        constexpr double kOverlayPickRadiusSquared =
                            kOverlayPickRadiusPx * kOverlayPickRadiusPx;
                        const QPointF pointer(releasePos);
                        const auto& overlayVertices =
                            thingMeshCache_.overlayLineVertices;
                        const auto appendOwnerRanges = [&] (
                            const std::unordered_map<int, std::vector<DrawRange>>& rangesByPickId,
                            const int pickId,
                            const std::int32_t thingIndex)
                        {
                            const auto rangeIt = rangesByPickId.find(pickId);
                            if (rangeIt == rangesByPickId.end())
                            {
                                return;
                            }

                            for (const DrawRange& range : rangeIt->second)
                            {
                                if (range.first < 0 || range.count < 2)
                                {
                                    continue;
                                }
                                const std::int64_t rangeFirst = range.first;
                                const std::int64_t rangeEnd = rangeFirst
                                    + static_cast<std::int64_t>(range.count);
                                const std::size_t first = static_cast<std::size_t>(
                                    std::max<std::int64_t>(rangeFirst, 0));
                                const std::size_t end = static_cast<std::size_t>(
                                    std::min<std::int64_t>(
                                        rangeEnd,
                                        static_cast<std::int64_t>(overlayVertices.size())));
                                if (first >= end || end - first < 2u)
                                {
                                    continue;
                                }

                                for (std::size_t vertex = first;
                                     vertex + 1u < end;
                                     vertex += 2u)
                                {
                                    const LineVertex& a = overlayVertices[vertex];
                                    const LineVertex& b = overlayVertices[vertex + 1u];
                                    const std::optional<ProjectedLineSegment> projected =
                                        projectClippedLineSegment(
                                            QVector3D(a.x, a.y, a.z),
                                            QVector3D(b.x, b.y, b.z),
                                            mvp,
                                            width(),
                                            height());
                                    if (!projected)
                                    {
                                        continue;
                                    }
                                    const std::optional<ProjectedLinePoint> hit =
                                        closestPointOnProjectedSegment(*projected, pointer);
                                    if (!hit
                                        || hit->distanceSquared
                                            > kOverlayPickRadiusSquared)
                                    {
                                        continue;
                                    }

                                    overlayThingCandidates.push_back(
                                        OverlayThingPointCandidate{
                                            thingIndex,
                                            hit->screen,
                                            hit->depth,
                                            hit->distanceSquared });
                                }
                            }
                        };

                        for (std::size_t thingIndex = 0;
                             thingIndex < level_->things.size()
                                 && thingIndex < thingPickIds_.size();
                             ++thingIndex)
                        {
                            const Thing& thing = level_->things[thingIndex];
                            if (!isLayerVisible(thing.layer)
                                || !shouldShowThing(thing)
                                || !std::isfinite(thing.x)
                                || !std::isfinite(thing.y)
                                || !std::isfinite(thing.z))
                            {
                                continue;
                            }
                            const std::uint32_t pickIdValue = thingPickIds_[thingIndex];
                            if (pickIdValue == 0u
                                || pickIdValue > static_cast<std::uint32_t>(
                                    std::numeric_limits<int>::max()))
                            {
                                continue;
                            }
                            const int pickId = static_cast<int>(pickIdValue);
                            if (showThingCollisionShapes_)
                            {
                                appendOwnerRanges(
                                    thingCollisionOverlayRangesByPickId_,
                                    pickId,
                                    static_cast<std::int32_t>(thingIndex));
                                appendOwnerRanges(
                                    thingBroadPhaseTreeGuideRangesByPickId_,
                                    pickId,
                                    static_cast<std::int32_t>(thingIndex));
                            }
                            if (showThingVisualBounds_)
                            {
                                appendOwnerRanges(
                                    thingVisualBoundsOverlayRangesByPickId_,
                                    pickId,
                                    static_cast<std::int32_t>(thingIndex));
                            }
                        }

                        // Only segments close enough to the pointer are sent
                        // through the scene-depth query. Match visible level
                        // front faces so a culled backface cannot block a pick.
                        // Explicit through/X-Ray selection keeps its normal bypass.
                        if (!overlayThingCandidates.empty()
                            && !allowThroughThingPick
                            && !xrayCyclePick)
                        {
                            std::vector<QPointF> samples{};
                            samples.reserve(overlayThingCandidates.size());
                            for (const OverlayThingPointCandidate& candidate
                                 : overlayThingCandidates)
                            {
                                samples.push_back(candidate.sample);
                            }
                            const std::vector<float> sceneDepths =
                                pickSceneDepthsAtPoints(
                                    mvp,
                                    samples,
                                    showSolid_,
                                    showThings_
                                        && thingViewMode_ == ThingViewMode::Shaded,
                                    true);
                            std::vector<OverlayThingPointCandidate> visibleCandidates{};
                            visibleCandidates.reserve(overlayThingCandidates.size());
                            for (std::size_t candidateIndex = 0;
                                 candidateIndex < overlayThingCandidates.size();
                                 ++candidateIndex)
                            {
                                const OverlayThingPointCandidate& candidate =
                                    overlayThingCandidates[candidateIndex];
                                const float sceneDepth = candidateIndex < sceneDepths.size()
                                    ? sceneDepths[candidateIndex]
                                    : std::numeric_limits<float>::quiet_NaN();
                                if ( render::picking::surfaceWireCandidatePassesSceneDepth(
                                        candidate.depth,
                                        sceneDepth,
                                        false,
                                        false) )
                                {
                                    visibleCandidates.push_back(candidate);
                                }
                            }
                            overlayThingCandidates = std::move(visibleCandidates);
                        }

                        float closestOverlayDistanceSquared =
                            std::numeric_limits<float>::infinity();
                        for (const OverlayThingPointCandidate& candidate
                             : overlayThingCandidates)
                        {
                            closestOverlayDistanceSquared = std::min(
                                closestOverlayDistanceSquared,
                                static_cast<float>(candidate.distanceSquared));
                        }
                        std::erase_if(
                            overlayThingCandidates,
                            [closestOverlayDistanceSquared](
                                const OverlayThingPointCandidate& candidate)
                            {
                                return !render::picking::surfaceWireCandidateNearCursor(
                                    static_cast<float>(candidate.distanceSquared),
                                    closestOverlayDistanceSquared,
                                    8.0f,
                                    1.5f);
                            });

                        std::vector<OverlayThingPointCandidate> nearestByOwner{};
                        nearestByOwner.reserve(overlayThingCandidates.size());
                        std::unordered_map<std::int32_t, std::size_t> candidateByOwner{};
                        candidateByOwner.reserve(overlayThingCandidates.size());
                        for (const OverlayThingPointCandidate& candidate
                             : overlayThingCandidates)
                        {
                            const auto [ownerIt, inserted] = candidateByOwner.emplace(
                                candidate.index,
                                nearestByOwner.size());
                            if (inserted)
                            {
                                nearestByOwner.push_back(candidate);
                                continue;
                            }
                            OverlayThingPointCandidate& existing =
                                nearestByOwner[ownerIt->second];
                            if (candidate.distanceSquared
                                    < existing.distanceSquared
                                || (candidate.distanceSquared
                                        == existing.distanceSquared
                                    && candidate.depth < existing.depth))
                            {
                                existing = candidate;
                            }
                        }
                        overlayThingCandidates = std::move(nearestByOwner);
                        std::sort(
                            overlayThingCandidates.begin(),
                            overlayThingCandidates.end(),
                            [](const OverlayThingPointCandidate& a,
                               const OverlayThingPointCandidate& b)
                            {
                                if (a.depth != b.depth)
                                {
                                    return a.depth < b.depth;
                                }
                                return a.index < b.index;
                            });
                    }

                    bool canGpuPick = glReady_ && (pickProgram_ != nullptr)
                        && (triVao_ != nullptr) && (triVertexCount_ > 0);
#ifdef SED_APP_MAIN_WINDOW_TEST_ACCESS
                    canGpuPick = canGpuPick && !forceCpuPickForTest_;
#endif
                    if (canGpuPick)
                    {
                        struct SurfaceCandidate
                        {
                            SurfaceRef ref{};
                            float depth{};
                        };

                        struct IndexCandidate
                        {
                            std::int32_t index{-1};
                            float depth{};
                        };

                        std::vector<SurfaceCandidate> surfaceCandidates{};
                        std::vector<IndexCandidate> thingCandidates{};
                        std::vector<IndexCandidate> lightCandidates{};

                        auto collectLightCandidates = [&]()
                        {
                            lightCandidates.clear();

                            constexpr float kRadiusPx = 10.0f;
                            const float r2 = kRadiusPx * kRadiusPx;

                            for (std::size_t i = 0; i < level_->lights.size(); ++i)
                            {
                                const SedLight& light = level_->lights[i];
                                if (!isLayerVisible(light.layer))
                                {
                                    continue;
                                }
                                const QVector4D clip = mvp * QVector4D(
                                    static_cast<float>(light.position.x),
                                    static_cast<float>(light.position.y),
                                    static_cast<float>(light.position.z),
                                    1.0f);

                                if (qFuzzyIsNull(clip.w()) || clip.w() <= 0.0f)
                                {
                                    continue;
                                }

                                const QVector3D ndc(clip.x() / clip.w(), clip.y() / clip.w(), clip.z() / clip.w());
                                const float sx = (ndc.x() * 0.5f + 0.5f) * static_cast<float>(width());
                                const float sy = (1.0f - (ndc.y() * 0.5f + 0.5f)) * static_cast<float>(height());

                                const float dx = sx - static_cast<float>(releasePos.x());
                                const float dy = sy - static_cast<float>(releasePos.y());
                                const float dsqr = dx * dx + dy * dy;
                                if (dsqr > r2)
                                {
                                    continue;
                                }

                                const float depth = ndc.z() * 0.5f + 0.5f;
                                lightCandidates.push_back(IndexCandidate{ static_cast<std::int32_t>(i), depth });
                            }

                            std::sort(lightCandidates.begin(), lightCandidates.end(), [](const IndexCandidate& a, const IndexCandidate& b)
                                {
                                    if (a.depth != b.depth)
                                    {
                                        return a.depth < b.depth;
                                    }
                                    return a.index < b.index;
                                });
                        };

                        const bool needThings = (mode == PickMode::Thing || mode == PickMode::Auto);
                        const bool needSurfaces =
                            (mode == PickMode::Sector || mode == PickMode::Surface || mode == PickMode::Auto)
                            || (needThings && !allowThroughThingPick);
                        const bool needLights =
                            (mode == PickMode::Light || mode == PickMode::Auto)
                            && previewBakedLightPickable;

                        if (needLights)
                        {
                            collectLightCandidates();
                        }

                        if (needSurfaces || needThings)
                        {
                            struct ContextGuard
                            {
                                GlView& view;
                                explicit ContextGuard(GlView& v)
                                    : view(v)
                                {
                                    view.makeCurrent();
                                }
                                ~ContextGuard()
                                {
                                    view.doneCurrent();
                                }
                            };

                            ContextGuard guard(*this);

                            const bool edgePick = !previewAutoSelection
                                && surfaceSubMode_ == SurfaceSubMode::Edge;
                            const bool vertexPick = !previewAutoSelection
                                && surfaceSubMode_ == SurfaceSubMode::Vertex;

                            const int surfaceGeoMin =
                                allowThroughSurfacePick ? 0 : static_cast<int>(TGeoMode::Solid);
                            // Sector picking is an object-volume operation, not
                            // a face-orientation operation.  A sector must be
                            // pickable when the camera is inside it or just
                            // behind one of its outward-facing walls; culling
                            // back faces here could remove the only front
                            // depth sample and make an occluded sector win.
                            // Ordinary Surface and Thing visibility follows
                            // the rendered face orientation. Through/X-Ray
                            // traversal deliberately keeps backfaces, while
                            // Sector volume picking stays two-sided.
                            const bool levelCullBackFaces = render::picking::
                                solidLevelOccludersCullBackFaces(
                                    solidLevelOcclusionFamily,
                                    collectThroughSurfaceOcclusion);

                            constexpr int kMaxPeels = 16;

                            const float dpr = devicePixelRatioF();
                            const int pixelW = std::max(1, static_cast<int>(std::lround(static_cast<double>(width()) * dpr)));
                            const int pixelH = std::max(1, static_cast<int>(std::lround(static_cast<double>(height()) * dpr)));

                            ensurePickFbo(pixelW, pixelH);

                            int px = static_cast<int>(std::lround(static_cast<double>(releasePos.x()) * dpr));
                            int py = static_cast<int>(std::lround(static_cast<double>(releasePos.y()) * dpr));
                            px = std::clamp(px, 0, pixelW - 1);
                            py = std::clamp(py, 0, pixelH - 1);

                            const int glX = px;
                            const int glY = (pixelH - 1) - py;

                            auto currentSector = [&]() -> std::int32_t
                            {
                                if (selectedSector_ >= 0)
                                {
                                    return selectedSector_;
                                }
                                if (!highlightedSelection_.sectors.empty())
                                {
                                    return *highlightedSelection_.sectors.begin();
                                }
                                return selectedSurfaceSector_;
                            };

                            auto currentSurface = [&]() -> SurfaceRef
                            {
                                if (selectedSurface_ >= 0 && selectedSurfaceSector_ >= 0)
                                {
                                    return SurfaceRef{ selectedSurfaceSector_, selectedSurface_ };
                                }
                                if (!highlightedSelection_.surfaces.empty())
                                {
                                    return *highlightedSelection_.surfaces.begin();
                                }
                                return SurfaceRef{ -1, -1 };
                            };

                            auto currentThing = [&]() -> std::int32_t
                            {
                                // Prefer the active selection for cycling, even when multiple things are highlighted.
                                if (selectedThing_ >= 0)
                                {
                                    return selectedThing_;
                                }
                                if (!highlightedSelection_.things.empty())
                                {
                                    return *highlightedSelection_.things.begin();
                                }
                                return -1;
                            };

                            auto resetPeelCache = [&](PeelCache& cache, bool& cacheValid, const int geoMin, const bool cullBackFaces) -> void
                            {
                            const bool sameContext = cacheValid
                                    && cache.mode == mode
                                    && cache.viewSerial == viewSerial_
                                    && cache.geoMin == geoMin
                                    && cache.cullBackFaces == cullBackFaces
                                    && ((cache.pos - releasePos).manhattanLength() <= 8);

                                if (sameContext)
                                {
                                    return;
                                }

                                cacheValid = true;
                                cache.pos = releasePos;
                                cache.mode = mode;
                                cache.viewSerial = viewSerial_;
                                cache.geoMin = geoMin;
                                cache.cullBackFaces = cullBackFaces;
                                cache.hits.clear();
                            };

                            auto peelOnce = [&](PeelCache& cache,
                                                const TriKind kind,
                                                const int geoMin,
                                                const bool cullBackFaces) -> bool
                            {
                                std::array<std::uint32_t, kMaxPeels> exclude{};
                                int excludeCount = 0;
                                for (const PickHit& h : cache.hits)
                                {
                                    if (excludeCount >= kMaxPeels)
                                    {
                                        break;
                                    }
                                    exclude[static_cast<std::size_t>(excludeCount)] = h.id;
                                    ++excludeCount;
                                }

                                const std::optional<PickHit> hit = pickReadPixel(
                                    mvp,
                                    kind,
                                    geoMin,
                                    cullBackFaces,
                                    glX,
                                    glY,
                                    exclude,
                                    excludeCount,
                                    true);
                                if (!hit)
                                {
                                    return false;
                                }

                                cache.hits.push_back(*hit);
                                return true;
                            };

                            if (needSurfaces)
                            {
                                resetPeelCache(levelPeelCache_, levelPeelCacheValid_, surfaceGeoMin, levelCullBackFaces);

                                if (levelPeelCache_.hits.empty())
                                {
                                    (void)peelOnce(levelPeelCache_, TriKind::Level, surfaceGeoMin, levelCullBackFaces);
                                }

                                bool advanceLevelPick = cyclePick;
                                if (!advanceLevelPick && selectedFamilyCanAdvance
                                    && !levelPeelCache_.hits.empty())
                                {
                                    const PickHit& frontHit = levelPeelCache_.hits.front();
                                    if (frontHit.id < pickObjects_.size())
                                    {
                                        const PickObject& front = pickObjects_[frontHit.id];
                                        if (front.kind == PickKind::Surface)
                                        {
                                            advanceLevelPick = (mode == PickMode::Surface || mode == PickMode::Auto)
                                                ? surfaceIsSelected(SurfaceRef{ front.sector, front.surface })
                                                : (mode == PickMode::Sector && sectorIsSelected(front.sector));
                                        }
                                    }
                                }

                                if (advanceLevelPick
                                    && (mode == PickMode::Surface || mode == PickMode::Auto))
                                {
                                    SurfaceRef cur = currentSurface();
                                    if (!cyclePick && !levelPeelCache_.hits.empty())
                                    {
                                        const PickHit& frontHit = levelPeelCache_.hits.front();
                                        if (frontHit.id < pickObjects_.size())
                                        {
                                            const PickObject& front = pickObjects_[frontHit.id];
                                            if (front.kind == PickKind::Surface)
                                            {
                                                cur = SurfaceRef{ front.sector, front.surface };
                                            }
                                        }
                                    }
                                    if (cur.sector >= 0 && cur.surface >= 0)
                                    {
                                        int curIndex = -1;
                                        for (std::size_t i = 0; i < levelPeelCache_.hits.size(); ++i)
                                        {
                                            const PickHit& h = levelPeelCache_.hits[i];
                                            if (h.id >= pickObjects_.size())
                                            {
                                                continue;
                                            }

                                            const PickObject& obj = pickObjects_[h.id];
                                            if (obj.kind == PickKind::Surface && obj.sector == cur.sector && obj.surface == cur.surface)
                                            {
                                                curIndex = static_cast<int>(i);
                                                break;
                                            }
                                        }

                                        const bool atLast = (curIndex >= 0) && (static_cast<std::size_t>(curIndex + 1) >= levelPeelCache_.hits.size());
                                        if (atLast && static_cast<int>(levelPeelCache_.hits.size()) < kMaxPeels)
                                        {
                                            (void)peelOnce(levelPeelCache_, TriKind::Level, surfaceGeoMin, levelCullBackFaces);
                                        }

                                        // In face sub-mode, cycle-picking should skip over un-face-pickable hits
                                        // (e.g. adjoins, backfaces) in a single click.
                                        if (!edgePick && !vertexPick && curIndex >= 0)
                                        {
                                            auto facePickable = [&](const SurfaceRef ref) -> bool
                                            {
                                                if (!level_ || ref.sector < 0 || ref.surface < 0)
                                                {
                                                    return false;
                                                }
                                                const std::size_t secIndex = static_cast<std::size_t>(ref.sector);
                                                if (secIndex >= level_->sectors.size())
                                                {
                                                    return false;
                                                }
                                                const Sector& sector = level_->sectors[secIndex];
                                                const std::size_t surfIndex = static_cast<std::size_t>(ref.surface);
                                                if (surfIndex >= sector.surfaces.size())
                                                {
                                                    return false;
                                                }

                                                const Surface& surface = sector.surfaces[surfIndex];
                                                if (!allowThroughSurfacePick
                                                    && surface.geo < static_cast<std::int32_t>(TGeoMode::Solid))
                                                {
                                                    return false;
                                                }
                                                if (collectThroughSurfaceOcclusion)
                                                {
                                                    return true;
                                                }
                                                if ((surface.faceFlags & FF_DoubleSided) != 0u)
                                                {
                                                    return true;
                                                }
                                                return surface.normal.Dot(direction) <= 1e-9;
                                            };

                                            auto haveNextPickable = [&]() -> bool
                                            {
                                                for (std::size_t i = static_cast<std::size_t>(curIndex + 1); i < levelPeelCache_.hits.size(); ++i)
                                                {
                                                    const PickHit& h = levelPeelCache_.hits[i];
                                                    if (h.id >= pickObjects_.size())
                                                    {
                                                        continue;
                                                    }

                                                    const PickObject& obj = pickObjects_[h.id];
                                                    if (obj.kind != PickKind::Surface)
                                                    {
                                                        continue;
                                                    }

                                                    if (facePickable(SurfaceRef{ obj.sector, obj.surface }))
                                                    {
                                                        return true;
                                                    }
                                                }
                                                return false;
                                            };

                                            while (!haveNextPickable()
                                                   && static_cast<int>(levelPeelCache_.hits.size()) < kMaxPeels)
                                            {
                                                if (!peelOnce(levelPeelCache_, TriKind::Level, surfaceGeoMin, levelCullBackFaces))
                                                {
                                                    break;
                                                }
                                            }
                                        }
                                    }
                                }
                                else if (advanceLevelPick && mode == PickMode::Sector)
                                {
                                    std::int32_t cur = currentSector();
                                    if (!cyclePick && !levelPeelCache_.hits.empty())
                                    {
                                        const PickHit& frontHit = levelPeelCache_.hits.front();
                                        if (frontHit.id < pickObjects_.size())
                                        {
                                            const PickObject& front = pickObjects_[frontHit.id];
                                            if (front.kind == PickKind::Surface)
                                            {
                                                cur = front.sector;
                                            }
                                        }
                                    }

                                    auto collectUniqueSectors = [&](const std::vector<PickHit>& hitsIn) -> std::vector<std::int32_t>
                                    {
                                        std::vector<std::int32_t> sectors{};
                                        sectors.reserve(hitsIn.size());

                                        for (const PickHit& h : hitsIn)
                                        {
                                            if (h.id >= pickObjects_.size())
                                            {
                                                continue;
                                            }

                                            const PickObject& obj = pickObjects_[h.id];
                                            if (obj.kind != PickKind::Surface)
                                            {
                                                continue;
                                            }

                                            if (std::find(sectors.begin(), sectors.end(), obj.sector) == sectors.end())
                                            {
                                                sectors.push_back(obj.sector);
                                            }
                                        }

                                        return sectors;
                                    };

                                    while (true)
                                    {
                                        const std::vector<std::int32_t> sectorsNow = collectUniqueSectors(levelPeelCache_.hits);
                                        const auto it = std::find(sectorsNow.begin(), sectorsNow.end(), cur);
                                        const bool hasNext = (it == sectorsNow.end()) ? true : ((it + 1) != sectorsNow.end());
                                        if (hasNext)
                                        {
                                            break;
                                        }

                                        if (static_cast<int>(levelPeelCache_.hits.size()) >= kMaxPeels)
                                        {
                                            break;
                                        }

                                        if (!peelOnce(levelPeelCache_, TriKind::Level, surfaceGeoMin, levelCullBackFaces))
                                        {
                                            break;
                                        }
                                }
                            }

                                // Edge/Vertex wire picking and any established
                                // through/X-Ray cycle need more than the first
                                // peel. This also exposes both coincident sides
                                // of an adjoin to repeated Ignore Filters clicks.
                                if ((mode == PickMode::Surface || mode == PickMode::Auto)
                                    && render::picking::surfacePointPickNeedsFullPeel(
                                        allowThroughSurfacePick,
                                        edgePick || vertexPick,
                                        wireXraySurfacePick,
                                        cyclePick,
                                        xrayCyclePick))
                                {
                                    while (static_cast<int>(levelPeelCache_.hits.size()) < kMaxPeels)
                                    {
                                        if (!peelOnce(levelPeelCache_, TriKind::Level, surfaceGeoMin, levelCullBackFaces))
                                        {
                                            break;
                                        }
                                    }
                                }

                                surfaceCandidates.reserve(levelPeelCache_.hits.size());
                                for (const PickHit& h : levelPeelCache_.hits)
                                {
                                    if (h.id >= pickObjects_.size())
                                    {
                                        continue;
                                    }

                                    const PickObject& obj = pickObjects_[h.id];
                                    if (obj.kind != PickKind::Surface)
                                    {
                                        continue;
                                    }

                                    surfaceCandidates.push_back(SurfaceCandidate{ SurfaceRef{ obj.sector, obj.surface }, h.depth });
                                }

                                // In face sub-mode, we may need to peel past un-face-pickable hits (e.g. adjoins,
                                // backfaces) so the user can select geometry behind them.
                                if ((mode == PickMode::Surface || mode == PickMode::Auto)
                                    && !edgePick && !vertexPick && !surfaceCandidates.empty() && !cyclePick)
                                {
                                    auto facePickable = [&](const SurfaceRef ref) -> bool
                                    {
                                        if (!level_ || ref.sector < 0 || ref.surface < 0)
                                        {
                                            return false;
                                        }
                                        const std::size_t secIndex = static_cast<std::size_t>(ref.sector);
                                        if (secIndex >= level_->sectors.size())
                                        {
                                            return false;
                                        }
                                        const Sector& sector = level_->sectors[secIndex];
                                        const std::size_t surfIndex = static_cast<std::size_t>(ref.surface);
                                        if (surfIndex >= sector.surfaces.size())
                                        {
                                            return false;
                                        }

                                        const Surface& surface = sector.surfaces[surfIndex];
                                        if (!allowThroughSurfacePick
                                            && surface.geo < static_cast<std::int32_t>(TGeoMode::Solid))
                                        {
                                            return false;
                                        }
                                        if (collectThroughSurfaceOcclusion)
                                        {
                                            return true;
                                        }
                                        if ((surface.faceFlags & FF_DoubleSided) != 0u)
                                        {
                                            return true;
                                        }

                                        const double dot = surface.normal.Dot(direction);
                                        return dot <= 1e-9;
                                    };

                                    bool haveFacePickable = std::any_of(
                                        surfaceCandidates.begin(),
                                        surfaceCandidates.end(),
                                        [&](const SurfaceCandidate& cand) { return facePickable(cand.ref); });

                                    while (!haveFacePickable
                                           && static_cast<int>(levelPeelCache_.hits.size()) < kMaxPeels)
                                    {
                                        if (!peelOnce(levelPeelCache_, TriKind::Level, surfaceGeoMin, levelCullBackFaces))
                                        {
                                            break;
                                        }

                                        const PickHit& h = levelPeelCache_.hits.back();
                                        if (h.id < pickObjects_.size())
                                        {
                                            const PickObject& obj = pickObjects_[h.id];
                                            if (obj.kind == PickKind::Surface)
                                            {
                                                const SurfaceRef ref{ obj.sector, obj.surface };
                                                surfaceCandidates.push_back(SurfaceCandidate{ ref, h.depth });
                                                haveFacePickable = haveFacePickable || facePickable(ref);
                                            }
                                        }
                                    }
                                }
                            }

                            if (needThings)
                            {
                                resetPeelCache(thingPeelCache_, thingPeelCacheValid_, 0, false);

                                if (thingPeelCache_.hits.empty())
                                {
                                    (void)peelOnce(thingPeelCache_, TriKind::Thing, 0, false);
                                }

                                bool advanceThingPeel = cyclePick;
                                if (!advanceThingPeel && selectedFamilyCanAdvance
                                    && (mode == PickMode::Thing || mode == PickMode::Auto)
                                    && !thingPeelCache_.hits.empty())
                                {
                                    const PickHit& frontHit = thingPeelCache_.hits.front();
                                    if (frontHit.id < pickObjects_.size())
                                    {
                                        const PickObject& front = pickObjects_[frontHit.id];
                                        advanceThingPeel = front.kind == PickKind::Thing
                                            && thingIsSelected(front.index);
                                    }
                                }

                                if (advanceThingPeel && (mode == PickMode::Thing || mode == PickMode::Auto))
                                {
                                    std::int32_t cur = currentThing();
                                    if (!cyclePick && !thingPeelCache_.hits.empty())
                                    {
                                        const PickHit& frontHit = thingPeelCache_.hits.front();
                                        if (frontHit.id < pickObjects_.size())
                                        {
                                            const PickObject& front = pickObjects_[frontHit.id];
                                            if (front.kind == PickKind::Thing)
                                            {
                                                cur = front.index;
                                            }
                                        }
                                    }
                                    if (cur >= 0)
                                    {
                                        int curIndex = -1;
                                        for (std::size_t i = 0; i < thingPeelCache_.hits.size(); ++i)
                                        {
                                            const PickHit& h = thingPeelCache_.hits[i];
                                            if (h.id >= pickObjects_.size())
                                            {
                                                continue;
                                            }

                                            const PickObject& obj = pickObjects_[h.id];
                                            if (obj.kind == PickKind::Thing && obj.index == cur)
                                            {
                                                curIndex = static_cast<int>(i);
                                                break;
                                            }
                                        }

                                        const bool atLast = (curIndex >= 0) && (static_cast<std::size_t>(curIndex + 1) >= thingPeelCache_.hits.size());
                                        if (atLast && static_cast<int>(thingPeelCache_.hits.size()) < kMaxPeels)
                                        {
                                            (void)peelOnce(thingPeelCache_, TriKind::Thing, 0, false);
                                        }
                                    }
                                }

                                thingCandidates.reserve(thingPeelCache_.hits.size());
                                for (const PickHit& h : thingPeelCache_.hits)
                                {
                                    if (h.id >= pickObjects_.size())
                                    {
                                        continue;
                                    }

                                    const PickObject& obj = pickObjects_[h.id];
                                    if (obj.kind != PickKind::Thing)
                                    {
                                        continue;
                                    }

                                    thingCandidates.push_back(IndexCandidate{ obj.index, h.depth });
                                }
                            }
                        }

                        std::optional<float> ordinarySurfaceSceneDepth{};
                        if (needSurfaces && showSolid_
                            && !collectThroughSurfaceOcclusion)
                        {
                            const QPointF point(releasePos);
                            const std::vector<QPointF> depthSamples{
                                point,
                                point + QPointF(1.0, 0.0),
                                point + QPointF(-1.0, 0.0),
                                point + QPointF(0.0, 1.0),
                                point + QPointF(0.0, -1.0) };
                            ordinarySurfaceSceneDepth = render::picking::
                                resolveSurfacePointSceneDepth(
                                    pickSceneDepthsAtPoints(
                                        mvp, depthSamples, true, false,
                                        render::picking::
                                            solidLevelOccludersCullBackFaces(
                                                solidLevelOcclusionFamily,
                                                false)));
                        }

                        // GPU face picking cannot return both coplanar owners
                        // of a shared line and intentionally omits GeoMode::None
                        // faces in a filled render.  When a wireframe is
                        // actually visible, supplement the face hits with
                        // exact projected-edge hits from every visible-layer
                        // Surface.  Only the narrow line is eligible; the
                        // interior of an invisible/adjoin Surface remains
                        // unpickable.  Merging these candidates also restores
                        // repeated-click cycling across both owners of a
                        // shared edge, independently of X-Ray mode.
                        if (needSurfaces && wireVisible)
                        {
                            struct ProjectedWireCandidate
                            {
                                SurfaceRef ref{};
                                float distanceSquared{};
                                float depth{1.0f};
                            };
                            std::vector<ProjectedWireCandidate> wireCandidates{};
                            float closestWireDistanceSquared =
                                std::numeric_limits<float>::infinity();
                            const QPointF mousePoint(releasePos);

                            auto projectWithDepth = [&](const Vertex& vertex,
                                                        QPointF& screen,
                                                        float& depth) -> bool
                            {
                                const QVector4D clip = mvp * QVector4D(
                                    static_cast<float>(vertex.x),
                                    static_cast<float>(vertex.y),
                                    static_cast<float>(vertex.z),
                                    1.0f);
                                if (!std::isfinite(clip.w()) || clip.w() <= 0.0f)
                                {
                                    return false;
                                }
                                const float invW = 1.0f / clip.w();
                                const float ndcX = clip.x() * invW;
                                const float ndcY = clip.y() * invW;
                                const float ndcZ = clip.z() * invW;
                                if (!std::isfinite(ndcX) || !std::isfinite(ndcY)
                                    || !std::isfinite(ndcZ)
                                    || ndcZ < -1.0f || ndcZ > 1.0f)
                                {
                                    return false;
                                }
                                screen = QPointF(
                                    (ndcX * 0.5f + 0.5f) * static_cast<float>(width()),
                                    (1.0f - (ndcY * 0.5f + 0.5f))
                                        * static_cast<float>(height()));
                                depth = ndcZ * 0.5f + 0.5f;
                                return true;
                            };

                            for (std::size_t sectorIndex = 0;
                                 sectorIndex < level_->sectors.size(); ++sectorIndex)
                            {
                                const Sector& sector = level_->sectors[sectorIndex];
                                if (!isLayerVisible(sector.layer))
                                {
                                    continue;
                                }
                                for (std::size_t surfaceIndex = 0;
                                     surfaceIndex < sector.surfaces.size(); ++surfaceIndex)
                                {
                                    const Surface& surface = sector.surfaces[surfaceIndex];
                                    if (surface.vertices.size() < 2)
                                    {
                                        continue;
                                    }
                                    if (!render::picking::surfacePointOwnerPassesFacing(
                                            (surface.faceFlags & FF_DoubleSided) != 0u,
                                            surface.normal.Dot(direction) > 1.0e-9,
                                            collectThroughSurfaceOcclusion
                                                || mode != PickMode::Surface))
                                    {
                                        continue;
                                    }

                                    float bestDistanceSq =
                                        std::numeric_limits<float>::infinity();
                                    float bestDepth = 1.0f;
                                    for (std::size_t edge = 0;
                                         edge < surface.vertices.size(); ++edge)
                                    {
                                        const std::int32_t aIndex = surface.vertices[edge];
                                        const std::int32_t bIndex = surface.vertices[
                                            (edge + 1) % surface.vertices.size()];
                                        if (aIndex < 0 || bIndex < 0
                                            || static_cast<std::size_t>(aIndex)
                                                >= sector.vertices.size()
                                            || static_cast<std::size_t>(bIndex)
                                                >= sector.vertices.size())
                                        {
                                            continue;
                                        }

                                        QPointF aScreen{};
                                        QPointF bScreen{};
                                        float aDepth{};
                                        float bDepth{};
                                        if (!projectWithDepth(
                                                sector.vertices[static_cast<std::size_t>(aIndex)],
                                                aScreen, aDepth)
                                            || !projectWithDepth(
                                                sector.vertices[static_cast<std::size_t>(bIndex)],
                                                bScreen, bDepth))
                                        {
                                            continue;
                                        }

                                        const QPointF segment = bScreen - aScreen;
                                        const double lengthSq =
                                            segment.x() * segment.x()
                                            + segment.y() * segment.y();
                                        double t = 0.0;
                                        if (lengthSq > 1.0e-9)
                                        {
                                            const QPointF fromA = mousePoint - aScreen;
                                            t = std::clamp(
                                                (fromA.x() * segment.x()
                                                    + fromA.y() * segment.y()) / lengthSq,
                                                0.0,
                                                1.0);
                                        }
                                        const QPointF closest = aScreen + segment * t;
                                        const QPointF delta = mousePoint - closest;
                                        const float distanceSq = static_cast<float>(
                                            delta.x() * delta.x() + delta.y() * delta.y());
                                        if (distanceSq < bestDistanceSq)
                                        {
                                            bestDistanceSq = distanceSq;
                                            bestDepth = aDepth
                                                + (bDepth - aDepth) * static_cast<float>(t);
                                        }
                                    }

                                    if (!std::isfinite(bestDistanceSq))
                                    {
                                        continue;
                                    }

                                    const SurfaceRef ref{
                                        static_cast<std::int32_t>(sectorIndex),
                                        static_cast<std::int32_t>(surfaceIndex) };
                                    wireCandidates.push_back(ProjectedWireCandidate{
                                        ref, bestDistanceSq, bestDepth });
                                    closestWireDistanceSquared = std::min(
                                        closestWireDistanceSquared,
                                        bestDistanceSq);
                                }
                            }

                            for (const ProjectedWireCandidate& wire : wireCandidates)
                            {
                                const bool surfaceSubElementPick =
                                    mode == PickMode::Surface
                                    && (surfaceSubMode_ == SurfaceSubMode::Edge
                                        || surfaceSubMode_ == SurfaceSubMode::Vertex);
                                const float candidateRadiusPx =
                                    surfaceSubElementPick
                                    ? render::picking::
                                        kSurfaceSubElementPickRadiusPx
                                    : 6.0f;
                                if (!render::picking::surfaceWireCandidateNearCursor(
                                        wire.distanceSquared,
                                        closestWireDistanceSquared,
                                        candidateRadiusPx)
                                    || !render::picking::surfaceWireCandidatePassesSceneDepth(
                                        wire.depth,
                                        ordinarySurfaceSceneDepth.value_or(1.0f),
                                        allowThroughSurfacePick,
                                        xrayCyclePick))
                                {
                                    continue;
                                }

                                const auto existing = std::find_if(
                                    surfaceCandidates.begin(),
                                    surfaceCandidates.end(),
                                    [ref = wire.ref](const SurfaceCandidate& candidate)
                                    {
                                        return candidate.ref == ref;
                                    });
                                if (existing == surfaceCandidates.end())
                                {
                                    surfaceCandidates.push_back(
                                        SurfaceCandidate{ wire.ref, wire.depth });
                                }
                                else
                                {
                                    existing->depth = std::min(
                                        existing->depth,
                                        wire.depth);
                                }
                            }

                            std::sort(
                                surfaceCandidates.begin(),
                                surfaceCandidates.end(),
                                [](const SurfaceCandidate& a, const SurfaceCandidate& b)
                                {
                                    if (a.depth != b.depth)
                                    {
                                        return a.depth < b.depth;
                                    }
                                    if (a.ref.sector != b.ref.sector)
                                    {
                                        return a.ref.sector < b.ref.sector;
                                    }
                                    return a.ref.surface < b.ref.surface;
                                });
                        }

                        // Keep every coplanar/shared-edge owner on the nearest
                        // facing-eligible solid boundary. The normalized-depth
                        // guard rejects only genuinely deeper walls;
                        // X-Ray/through traversal deliberately skips it.
                        if (ordinarySurfaceSceneDepth)
                        {
                            std::erase_if(
                                surfaceCandidates,
                                [&](const SurfaceCandidate& candidate)
                                {
                                    return !render::picking::
                                        surfaceSelectionCandidatePassesSceneDepth(
                                            candidate.depth,
                                            *ordinarySurfaceSceneDepth,
                                            false,
                                            false);
                                });
                        }

                        if (!previewAutoSelection
                            && needSurfaces
                            && strictSurfaceSelectionEnabled_
                            && (surfaceSubMode_ == SurfaceSubMode::Edge
                                || surfaceSubMode_ == SurfaceSubMode::Vertex))
                        {
                            std::erase_if(
                                surfaceCandidates,
                                [this](const SurfaceCandidate& candidate)
                                {
                                    return !strictSurfaceSelectionScope_.contains(
                                        candidate.ref);
                                });
                        }

                        if (needThings && thingCandidates.empty()
                            && (thingViewMode_ == ThingViewMode::Dots
                                || thingViewMode_ == ThingViewMode::Boxes
                                || thingViewMode_ == ThingViewMode::Shaded))
                        {
                            thingCandidates.clear();

                            constexpr float kRadiusPx = 10.0f;
                            const float r2 = kRadiusPx * kRadiusPx;

                            for (std::size_t i = 0; i < level_->things.size(); ++i)
                            {
                                const Thing& thing = level_->things[i];
                                if (!isLayerVisible(thing.layer))
                                {
                                    continue;
                                }
                                if (!shouldShowThing(thing))
                                {
                                    continue;
                                }

                                if (!std::isfinite(thing.x) || !std::isfinite(thing.y) || !std::isfinite(thing.z))
                                {
                                    continue;
                                }

                                if (thingViewMode_ == ThingViewMode::Shaded)
                                {
                                    QRectF spriteRect{};
                                    float spriteDepth = std::numeric_limits<float>::infinity();
                                    if (projectSpriteBounds(i, mvp, spriteRect, spriteDepth)
                                        && spriteRect.adjusted(-4.0, -4.0, 4.0, 4.0).contains(QPointF(releasePos)))
                                    {
                                        thingCandidates.push_back(IndexCandidate{
                                            static_cast<std::int32_t>(i),
                                            spriteDepth});
                                        continue;
                                    }
                                }

                                const QVector4D clip = mvp * QVector4D(
                                    static_cast<float>(thing.x),
                                    static_cast<float>(thing.y),
                                    static_cast<float>(thing.z),
                                    1.0f);

                                if (qFuzzyIsNull(clip.w()) || clip.w() <= 0.0f)
                                {
                                    continue;
                                }

                                const QVector3D ndc(clip.x() / clip.w(), clip.y() / clip.w(), clip.z() / clip.w());
                                const float sx = (ndc.x() * 0.5f + 0.5f) * static_cast<float>(width());
                                const float sy = (1.0f - (ndc.y() * 0.5f + 0.5f)) * static_cast<float>(height());

                                const float dx = sx - static_cast<float>(releasePos.x());
                                const float dy = sy - static_cast<float>(releasePos.y());
                                const float dsqr = dx * dx + dy * dy;
                                if (dsqr > r2)
                                {
                                    continue;
                                }

                                const float depth = ndc.z() * 0.5f + 0.5f;
                                thingCandidates.push_back(IndexCandidate{ static_cast<std::int32_t>(i), depth });
                            }

                            std::sort(thingCandidates.begin(), thingCandidates.end(), [](const IndexCandidate& a, const IndexCandidate& b)
                                {
                                    if (a.depth != b.depth)
                                    {
                                        return a.depth < b.depth;
                                    }
                                        return a.index < b.index;
                                });
                        }

                        if (needThings && !overlayThingCandidates.empty())
                        {
                            for (const OverlayThingPointCandidate& overlay
                                 : overlayThingCandidates)
                            {
                                const auto existing = std::find_if(
                                    thingCandidates.begin(),
                                    thingCandidates.end(),
                                    [&overlay](const IndexCandidate& candidate)
                                    {
                                        return candidate.index == overlay.index;
                                    });
                                if (existing == thingCandidates.end())
                                {
                                    thingCandidates.push_back(IndexCandidate{
                                        overlay.index,
                                        overlay.depth });
                                }
                                else
                                {
                                    // The overlay is an alternate visible
                                    // representation of its owner. Retain the
                                    // nearer of it and the ordinary model pick.
                                    existing->depth = std::min(
                                        existing->depth,
                                        overlay.depth);
                                }
                            }
                            std::sort(
                                thingCandidates.begin(),
                                thingCandidates.end(),
                                [](const IndexCandidate& a,
                                   const IndexCandidate& b)
                                {
                                    if (a.depth != b.depth)
                                    {
                                        return a.depth < b.depth;
                                    }
                                    return a.index < b.index;
                                });
                        }

                        if (needThings && !surfaceCandidates.empty() && !thingCandidates.empty())
                        {
                            const float frontSurfaceDepth = surfaceCandidates.front().depth;
                            thingCandidates.erase(
                                std::remove_if(
                                    thingCandidates.begin(),
                                    thingCandidates.end(),
                                    [frontSurfaceDepth,
                                     allowThroughThingPick,
                                     xraySelectionCyclingEnabled = xraySelectionCyclingEnabled_,
                                     xrayCandidateTraversal](const IndexCandidate& cand)
                                    {
                                        return !render::picking::candidatePassesOcclusion(
                                            cand.depth,
                                            frontSurfaceDepth,
                                            allowThroughThingPick,
                                            xraySelectionCyclingEnabled,
                                            xrayCandidateTraversal,
                                            1.0e-5f);
                                    }),
                                thingCandidates.end());
                        }
                        if (!collectThroughThingOcclusion
                            && !previewAutoSelection
                            && thingCandidates.size() > 1)
                        {
                            thingCandidates.erase(thingCandidates.begin() + 1, thingCandidates.end());
                        }

                        const auto currentSector = [&]() -> std::int32_t
                        {
                            if (selectedSector_ >= 0)
                            {
                                return selectedSector_;
                            }
                            if (!highlightedSelection_.sectors.empty())
                            {
                                return *highlightedSelection_.sectors.begin();
                            }
                            return selectedSurfaceSector_;
                        };

                        const auto currentSurface = [&]() -> SurfaceRef
                        {
                            if (selectedSurface_ >= 0 && selectedSurfaceSector_ >= 0)
                            {
                                return SurfaceRef{ selectedSurfaceSector_, selectedSurface_ };
                            }
                            if (!highlightedSelection_.surfaces.empty())
                            {
                                return *highlightedSelection_.surfaces.begin();
                            }
                            return SurfaceRef{ -1, -1 };
                        };

                        const auto currentThing = [&]() -> std::int32_t
                        {
                            if (selectedThing_ >= 0)
                            {
                                return selectedThing_;
                            }
                            if (!highlightedSelection_.things.empty())
                            {
                                return *highlightedSelection_.things.begin();
                            }
                            return -1;
                        };

                        const auto currentLight = [&]() -> std::int32_t
                        {
                            if (selectedLight_ >= 0)
                            {
                                return selectedLight_;
                            }
                            if (!highlightedSelection_.lights.empty())
                            {
                                return *highlightedSelection_.lights.begin();
                            }
                            return -1;
                        };

                        auto setSurfaceHitFromCandidate = [&](const SurfaceCandidate& cand)
                        {
                            const float ndcZ = cand.depth * 2.0f - 1.0f;
                            const QVector3D pos = toAffine(inv * QVector4D(ndcX, ndcY, ndcZ, 1.0f));

                            sed::level::SurfacePickHit hit{};
                            hit.surface = cand.ref;
                            hit.position = TVector{ static_cast<double>(pos.x()), static_cast<double>(pos.y()), static_cast<double>(pos.z()) };
                            hit.distance = (hit.position - origin).Length();
                            surfaceHit = hit;
                        };

                        const QPointF mpSector(releasePos);
                        auto projectSurfacePointToScreen = [&](const TVector& wp, QPointF& outPos) -> bool
                        {
                            const QVector4D clip = mvp * QVector4D(
                                static_cast<float>(wp.x),
                                static_cast<float>(wp.y),
                                static_cast<float>(wp.z),
                                1.0f);
                            if (qFuzzyIsNull(clip.w()) || clip.w() <= 0.0f)
                            {
                                return false;
                            }

                            const QVector3D ndc(clip.x() / clip.w(), clip.y() / clip.w(), clip.z() / clip.w());
                            const float sx = (ndc.x() * 0.5f + 0.5f) * static_cast<float>(width());
                            const float sy = (1.0f - (ndc.y() * 0.5f + 0.5f)) * static_cast<float>(height());
                            outPos = QPointF(sx, sy);
                            return true;
                        };

                        auto surfaceBestEdgeDistanceSq = [&](const SurfaceRef ref) -> std::optional<float>
                        {
                            if (!level_ || ref.sector < 0 || ref.surface < 0)
                            {
                                return std::nullopt;
                            }

                            const std::size_t sectorIndex = static_cast<std::size_t>(ref.sector);
                            if (sectorIndex >= level_->sectors.size())
                            {
                                return std::nullopt;
                            }

                            const Sector& sector = level_->sectors[sectorIndex];
                            const std::size_t surfaceIndex = static_cast<std::size_t>(ref.surface);
                            if (surfaceIndex >= sector.surfaces.size())
                            {
                                return std::nullopt;
                            }

                            const Surface& surface = sector.surfaces[surfaceIndex];
                            const auto& vxs = surface.vertices;
                            if (vxs.size() < 2)
                            {
                                return std::nullopt;
                            }

                            float best = std::numeric_limits<float>::infinity();
                            for (std::size_t i = 0; i < vxs.size(); ++i)
                            {
                                const std::int32_t aidx = vxs[i];
                                const std::int32_t bidx = vxs[(i + 1) % vxs.size()];
                                if (aidx < 0 || bidx < 0)
                                {
                                    continue;
                                }
                                if (static_cast<std::size_t>(aidx) >= sector.vertices.size()
                                    || static_cast<std::size_t>(bidx) >= sector.vertices.size())
                                {
                                    continue;
                                }

                                const Vertex& va = sector.vertices[static_cast<std::size_t>(aidx)];
                                const Vertex& vb = sector.vertices[static_cast<std::size_t>(bidx)];
                                QPointF sa{};
                                QPointF sb{};
                                if (!projectSurfacePointToScreen(TVector{ va.x, va.y, va.z }, sa)
                                    || !projectSurfacePointToScreen(TVector{ vb.x, vb.y, vb.z }, sb))
                                {
                                    continue;
                                }

                                best = std::min(best, distanceSqPointToSegment(mpSector, sa, sb));
                            }

                            if (!std::isfinite(best))
                            {
                                return std::nullopt;
                            }

                            return best;
                        };

                        if (mode == PickMode::Sector)
                        {
                            std::vector<std::int32_t> sectorCandidates{};
                            sectorCandidates.reserve(surfaceCandidates.size());

                            constexpr float kSectorWirePickTolPx = 10.0f;
                            const float sectorWirePickTol2 = kSectorWirePickTolPx * kSectorWirePickTolPx;

                            std::unordered_set<std::int32_t> seenSectors{};
                            std::vector<std::int32_t> edgeSectorCandidates{};
                            if (wireVisible)
                            {
                                std::vector<std::pair<std::int32_t, float>> rankedEdgeSectorCandidates{};
                                rankedEdgeSectorCandidates.reserve(level_->sectors.size());

                                for (std::size_t sectorIndex = 0; sectorIndex < level_->sectors.size(); ++sectorIndex)
                                {
                                    const Sector& sector = level_->sectors[sectorIndex];
                                    if (!isLayerVisible(sector.layer))
                                    {
                                        continue;
                                    }

                                    float bestD2 = std::numeric_limits<float>::infinity();
                                    bool found = false;
                                    for (std::size_t surfaceIndex = 0; surfaceIndex < sector.surfaces.size(); ++surfaceIndex)
                                    {
                                        const std::optional<float> d2 = surfaceBestEdgeDistanceSq(
                                            SurfaceRef{
                                                static_cast<std::int32_t>(sectorIndex),
                                                static_cast<std::int32_t>(surfaceIndex) });
                                        if (!d2 || *d2 > sectorWirePickTol2)
                                        {
                                            continue;
                                        }

                                        bestD2 = std::min(bestD2, *d2);
                                        found = true;
                                    }

                                    if (found)
                                    {
                                        rankedEdgeSectorCandidates.emplace_back(static_cast<std::int32_t>(sectorIndex), bestD2);
                                    }
                                }

                                std::sort(rankedEdgeSectorCandidates.begin(), rankedEdgeSectorCandidates.end(), [](const auto& a, const auto& b)
                                {
                                        if (a.second != b.second)
                                        {
                                            return a.second < b.second;
                                        }
                                        return a.first < b.first;
                                });

                                edgeSectorCandidates.reserve(rankedEdgeSectorCandidates.size());
                                for (const auto& entry : rankedEdgeSectorCandidates)
                                {
                                    edgeSectorCandidates.push_back(entry.first);
                                }
                            }

                            // With ordinary solid picking, a face/depth hit is
                            // authoritative.  Previously the 2-D wire edge
                            // list was inserted first and could select a
                            // farther sector whose projected edge happened to
                            // be a few pixels closer, making the directly
                            // visible front sector appear to be skipped.
                            const bool preferFrontFace = !allowThroughSurfacePick
                                && !collectThroughSurfaceOcclusion
                                && !surfaceCandidates.empty();
                            if (!preferFrontFace)
                            {
                                for (const std::int32_t sector : edgeSectorCandidates)
                                {
                                    if (seenSectors.insert(sector).second)
                                    {
                                        sectorCandidates.push_back(sector);
                                    }
                                }
                            }

                            for (const SurfaceCandidate& cand : surfaceCandidates)
                            {
                                if (seenSectors.insert(cand.ref.sector).second)
                                {
                                    sectorCandidates.push_back(cand.ref.sector);
                                }
                            }

                            if (!sectorCandidates.empty())
                            {
                                std::size_t pickIndex = 0;
                                if (cyclePick)
                                {
                                    const std::int32_t current = currentSector();
                                    if (const auto it = std::find(sectorCandidates.begin(), sectorCandidates.end(), current); it != sectorCandidates.end())
                                    {
                                        pickIndex = (static_cast<std::size_t>(it - sectorCandidates.begin()) + 1) % sectorCandidates.size();
                                    }
                                }
                                else if (render::picking::shouldSkipSelectedFrontCandidate(
                                             xraySelectionCyclingEnabled_,
                                             xrayCycleSelectionClick,
                                             sectorIsSelected(sectorCandidates.front())))
                                {
                                    pickIndex = 1u % sectorCandidates.size();
                                }

                                newSector = sectorCandidates[pickIndex];
                                newSurface = -1;
                                newThing = -1;
                                newLight = -1;
                            }
                        }
                        else if (mode == PickMode::Surface)
                        {
                            if (!surfaceCandidates.empty())
                            {
                                // In Edge/Vertex sub-modes, only select surfaces when the click is
                                // close to the corresponding geometry; clicking the face should not
                                // change selection.
                                const bool edgePick = (surfaceSubMode_ == SurfaceSubMode::Edge);
                                const bool vertexPick = (surfaceSubMode_ == SurfaceSubMode::Vertex);

                                auto projectToScreen = [&](const TVector& wp, QPointF& outPos) -> bool
                                {
                                    const QVector4D clip = mvp * QVector4D(
                                        static_cast<float>(wp.x),
                                        static_cast<float>(wp.y),
                                        static_cast<float>(wp.z),
                                        1.0f);
                                    if (qFuzzyIsNull(clip.w()) || clip.w() <= 0.0f)
                                    {
                                        return false;
                                    }

                                    const QVector3D ndc(clip.x() / clip.w(), clip.y() / clip.w(), clip.z() / clip.w());
                                    const float sx = (ndc.x() * 0.5f + 0.5f) * static_cast<float>(width());
                                    const float sy = (1.0f - (ndc.y() * 0.5f + 0.5f)) * static_cast<float>(height());
                                    outPos = QPointF(sx, sy);
                                    return true;
                                };

                                // Sub-mode picking (edge/vertex) is intentionally forgiving, but when a face is
                                // only pickable via wireframe (adjoins/backfaces), keep the tolerance tight so
                                // we don't accidentally select it just by being "near" an edge.
                                constexpr float kPickTolPx = render::picking::
                                    kSurfaceSubElementPickRadiusPx;
                                constexpr float kWireEdgeTolPx = 3.0f;
                                const float tol2 = kPickTolPx * kPickTolPx;
                                const float wireTol2 = kWireEdgeTolPx * kWireEdgeTolPx;
                                const QPointF mp(releasePos);

                                auto matchCandidate = [&](const SurfaceCandidate& cand, float& bestDist2, sed::level::SurfacePickHit& outHit) -> bool
                                {
                                    if (!level_)
                                    {
                                        return false;
                                    }
                                    if (cand.ref.sector < 0 || cand.ref.surface < 0)
                                    {
                                        return false;
                                    }
                                    if (static_cast<std::size_t>(cand.ref.sector) >= level_->sectors.size())
                                    {
                                        return false;
                                    }

                                    const Sector& sector = level_->sectors[static_cast<std::size_t>(cand.ref.sector)];
                                    if (static_cast<std::size_t>(cand.ref.surface) >= sector.surfaces.size())
                                    {
                                        return false;
                                    }

                                    const Surface& surface = sector.surfaces[static_cast<std::size_t>(cand.ref.surface)];
                                    const auto& vxs = surface.vertices;
                                    if (vxs.empty())
                                    {
                                        return false;
                                    }

                                    bestDist2 = std::numeric_limits<float>::infinity();
                                    TVector hitWorld{};

                                    if (vertexPick)
                                    {
                                        std::int32_t bestVi = -1;
                                        QPointF bestSp{};

                                        for (const std::int32_t vi : vxs)
                                        {
                                            if (vi < 0 || static_cast<std::size_t>(vi) >= sector.vertices.size())
                                            {
                                                continue;
                                            }

                                            const Vertex& v = sector.vertices[static_cast<std::size_t>(vi)];
                                            const TVector wp{ v.x, v.y, v.z };
                                            QPointF sp{};
                                            if (!projectToScreen(wp, sp))
                                            {
                                                continue;
                                            }

                                            const QPointF d = mp - sp;
                                            const float d2 = static_cast<float>((d.x() * d.x()) + (d.y() * d.y()));
                                            if (d2 < bestDist2)
                                            {
                                                bestDist2 = d2;
                                                bestVi = vi;
                                                bestSp = sp;
                                                hitWorld = wp;
                                            }
                                        }

                                        if (bestVi < 0 || bestDist2 > tol2)
                                        {
                                            return false;
                                        }
                                    }
                                    else if (edgePick)
                                    {
                                        if (vxs.size() < 2)
                                        {
                                            return false;
                                        }

                                        for (std::size_t i = 0; i < vxs.size(); ++i)
                                        {
                                            const std::int32_t aidx = vxs[i];
                                            const std::int32_t bidx = vxs[(i + 1) % vxs.size()];
                                            if (aidx < 0 || bidx < 0)
                                            {
                                                continue;
                                            }
                                            if (static_cast<std::size_t>(aidx) >= sector.vertices.size()
                                                || static_cast<std::size_t>(bidx) >= sector.vertices.size())
                                            {
                                                continue;
                                            }

                                            const Vertex& va = sector.vertices[static_cast<std::size_t>(aidx)];
                                            const Vertex& vb = sector.vertices[static_cast<std::size_t>(bidx)];
                                            const TVector wa{ va.x, va.y, va.z };
                                            const TVector wb{ vb.x, vb.y, vb.z };

                                            QPointF sa{};
                                            QPointF sb{};
                                            if (!projectToScreen(wa, sa) || !projectToScreen(wb, sb))
                                            {
                                                continue;
                                            }

                                            const float d2 = distanceSqPointToSegment(mp, sa, sb);
                                            if (d2 < bestDist2)
                                            {
                                                bestDist2 = d2;

                                                // Compute a world-space point on the edge for sub-mode highlighting.
                                                const QPointF ab = sb - sa;
                                                const double ab2 = (ab.x() * ab.x()) + (ab.y() * ab.y());
                                                double t = 0.0;
                                                if (ab2 > 1e-9)
                                                {
                                                    const QPointF ap = mp - sa;
                                                    t = ((ap.x() * ab.x()) + (ap.y() * ab.y())) / ab2;
                                                }
                                                t = std::clamp(t, 0.0, 1.0);

                                                hitWorld.x = wa.x + (wb.x - wa.x) * t;
                                                hitWorld.y = wa.y + (wb.y - wa.y) * t;
                                                hitWorld.z = wa.z + (wb.z - wa.z) * t;
                                            }
                                        }

                                        if (!std::isfinite(bestDist2) || bestDist2 > tol2)
                                        {
                                            return false;
                                        }
                                    }
                                    else
                                    {
                                        return false;
                                    }

                                    outHit.surface = cand.ref;
                                    outHit.position = hitWorld;
                                    outHit.distance = (outHit.position - origin).Length();
                                    return true;
                                };

                                std::size_t startIndex = 0;
                                if (cyclePick)
                                {
                                    const SurfaceRef current = currentSurface();
                                    for (std::size_t i = 0; i < surfaceCandidates.size(); ++i)
                                    {
                                        const SurfaceRef ref = surfaceCandidates[i].ref;
                                        if (ref.sector == current.sector && ref.surface == current.surface)
                                        {
                                            startIndex = (i + 1) % surfaceCandidates.size();
                                            break;
                                        }
                                    }
                                }
                                else if (render::picking::shouldSkipSelectedFrontCandidate(
                                             xraySelectionCyclingEnabled_,
                                             xrayCycleSelectionClick,
                                             surfaceIsSelected(surfaceCandidates.front().ref)))
                                {
                                    startIndex = 1u % surfaceCandidates.size();
                                }

                                bool picked = false;
                                float bestDist2 = std::numeric_limits<float>::infinity();
                                SurfaceCandidate bestCand = surfaceCandidates[startIndex];
                                sed::level::SurfacePickHit bestHit{};

                                if (edgePick || vertexPick)
                                {
                                    for (std::size_t ofs = 0; ofs < surfaceCandidates.size(); ++ofs)
                                    {
                                        const SurfaceCandidate& cand = surfaceCandidates[(startIndex + ofs) % surfaceCandidates.size()];
                                        float d2 = std::numeric_limits<float>::infinity();
                                        sed::level::SurfacePickHit hit{};
                                        if (matchCandidate(cand, d2, hit))
                                        {
                                            bestDist2 = d2;
                                            bestCand = cand;
                                            bestHit = hit;
                                            picked = true;
                                            break; // candidates are depth-sorted; pick the first match
                                        }
                                    }

                                    if (!picked)
                                    {
                                        const SurfaceRef cur = currentSurface();
                                        newSector = cur.sector;
                                        newSurface = cur.surface;
                                        newThing = -1;
                                        newLight = -1;
                                    }
                                    else
                                    {
                                        newSector = bestCand.ref.sector;
                                        newSurface = bestCand.ref.surface;
                                        newThing = -1;
                                        newLight = -1;
                                        surfaceHit = bestHit;
                                    }
                                }
                                else
                                {
                                    // Face sub-mode: in normal solid picking, keep back-facing/not-drawn surfaces
                                    // edge-only so clicks stay on visible faces. Through-pick modes (Alt or
                                    // non-solid viewport rendering) allow selecting them directly.
                                    bool pickedFace = false;
                                    SurfaceCandidate pickedCand = surfaceCandidates[startIndex];

                                    auto backfaceSingleSided = [&](const Surface& surface) -> bool
                                    {
                                        if ((surface.faceFlags & FF_DoubleSided) != 0u)
                                        {
                                            return false;
                                        }
                                        return surface.normal.Dot(direction) > 1e-9;
                                    };

                                    auto clickNearAnyEdgeOrVertex = [&](const Sector& sector, const Surface& surface) -> bool
                                    {
                                        const auto& vxs = surface.vertices;
                                        if (vxs.empty())
                                        {
                                            return false;
                                        }

                                        float best = std::numeric_limits<float>::infinity();
                                        if (vxs.size() >= 2)
                                        {
                                            for (std::size_t i = 0; i < vxs.size(); ++i)
                                            {
                                                const std::int32_t aidx = vxs[i];
                                                const std::int32_t bidx = vxs[(i + 1) % vxs.size()];
                                                if (aidx < 0 || bidx < 0)
                                                {
                                                    continue;
                                                }
                                                if (static_cast<std::size_t>(aidx) >= sector.vertices.size()
                                                    || static_cast<std::size_t>(bidx) >= sector.vertices.size())
                                                {
                                                    continue;
                                                }

                                                const Vertex& va = sector.vertices[static_cast<std::size_t>(aidx)];
                                                const Vertex& vb = sector.vertices[static_cast<std::size_t>(bidx)];

                                                QPointF sa{};
                                                QPointF sb{};
                                                if (!projectToScreen(TVector{ va.x, va.y, va.z }, sa)
                                                    || !projectToScreen(TVector{ vb.x, vb.y, vb.z }, sb))
                                                {
                                                    continue;
                                                }

                                                best = std::min(best, distanceSqPointToSegment(mp, sa, sb));
                                            }
                                        }

                                        for (const std::int32_t vidx : vxs)
                                        {
                                            if (vidx < 0 || static_cast<std::size_t>(vidx) >= sector.vertices.size())
                                            {
                                                continue;
                                            }

                                            const Vertex& vv = sector.vertices[static_cast<std::size_t>(vidx)];
                                            QPointF sp{};
                                            if (!projectToScreen(TVector{ vv.x, vv.y, vv.z }, sp))
                                            {
                                                continue;
                                            }

                                            const QPointF d = mp - sp;
                                            best = std::min(best, static_cast<float>((d.x() * d.x()) + (d.y() * d.y())));
                                        }

                                        return std::isfinite(best) && best <= wireTol2;
                                    };

                                    for (std::size_t ofs = 0; ofs < surfaceCandidates.size(); ++ofs)
                                    {
                                        const SurfaceCandidate& cand = surfaceCandidates[(startIndex + ofs) % surfaceCandidates.size()];
                                        if (cand.ref.sector < 0 || cand.ref.surface < 0)
                                        {
                                            continue;
                                        }
                                        if (static_cast<std::size_t>(cand.ref.sector) >= level_->sectors.size())
                                        {
                                            continue;
                                        }

                                        const Sector& sector = level_->sectors[static_cast<std::size_t>(cand.ref.sector)];
                                        if (static_cast<std::size_t>(cand.ref.surface) >= sector.surfaces.size())
                                        {
                                            continue;
                                        }

                                        const Surface& surface = sector.surfaces[static_cast<std::size_t>(cand.ref.surface)];

                                        const bool edgeOnly =
                                            wireXraySurfacePick
                                            || (!collectThroughSurfaceOcclusion
                                                && ((surface.geo < static_cast<std::int32_t>(TGeoMode::Solid))
                                                    || backfaceSingleSided(surface)));

                                        if (edgeOnly)
                                        {
                                            if (!wireVisible && !showVertices_)
                                            {
                                                continue;
                                            }
                                            if (!clickNearAnyEdgeOrVertex(sector, surface))
                                            {
                                                continue;
                                            }
                                        }

                                        pickedCand = cand;
                                        pickedFace = true;
                                        break;
                                    }

                                    if (!pickedFace)
                                    {
                                        const SurfaceRef cur = currentSurface();
                                        newSector = cur.sector;
                                        newSurface = cur.surface;
                                        newThing = -1;
                                        newLight = -1;
                                    }
                                    else
                                    {
                                        newSector = pickedCand.ref.sector;
                                        newSurface = pickedCand.ref.surface;
                                        newThing = -1;
                                        newLight = -1;
                                        setSurfaceHitFromCandidate(pickedCand);
                                    }
                                }
                            }
                        }
                        else if (mode == PickMode::Thing)
                        {
                            if (!thingCandidates.empty())
                            {
                                std::optional<std::size_t> currentIndex{};
                                if (cyclePick)
                                {
                                    const std::int32_t current = currentThing();
                                    if (const auto it = std::find_if(thingCandidates.begin(), thingCandidates.end(), [&](const IndexCandidate& hit)
                                        {
                                            return hit.index == current;
                                        });
                                        it != thingCandidates.end())
                                    {
                                        currentIndex = static_cast<std::size_t>(it - thingCandidates.begin());
                                    }
                                }
                                else if (render::picking::shouldSkipSelectedFrontCandidate(
                                             xraySelectionCyclingEnabled_,
                                             xrayCycleSelectionClick,
                                             thingIsSelected(thingCandidates.front().index)))
                                {
                                    currentIndex = 0u;
                                }
                                const bool advancePick = cyclePick || currentIndex.has_value();
                                const std::size_t pickIndex = render::picking::nextFrontToBackCandidateIndex(
                                    thingCandidates.size(),
                                    currentIndex,
                                    advancePick);

                                newSector = -1;
                                newSurface = -1;
                                newThing = thingCandidates[pickIndex].index;
                                newLight = -1;
                            }
                        }
                        else if (mode == PickMode::Light)
                        {
                            if (!lightCandidates.empty())
                            {
                                std::size_t pickIndex = 0;
                                if (cyclePick)
                                {
                                    const std::int32_t current = currentLight();
                                    if (const auto it = std::find_if(lightCandidates.begin(), lightCandidates.end(), [&](const IndexCandidate& hit)
                                        {
                                            return hit.index == current;
                                        });
                                        it != lightCandidates.end())
                                    {
                                        pickIndex = (static_cast<std::size_t>(it - lightCandidates.begin()) + 1) % lightCandidates.size();
                                    }
                                }
                                else if (render::picking::shouldSkipSelectedFrontCandidate(
                                             xraySelectionCyclingEnabled_,
                                             xrayCycleSelectionClick,
                                             lightIsSelected(lightCandidates.front().index)))
                                {
                                    pickIndex = 1u % lightCandidates.size();
                                }

                                newSector = -1;
                                newSurface = -1;
                                newThing = -1;
                                newLight = lightCandidates[pickIndex].index;
                            }
                        }
                        else
                        {
                            if (previewAutoSelection)
                            {
                                using render::picking::PreviewAutoPickCandidate;
                                using render::picking::PreviewAutoPickKind;

                                std::vector<PreviewAutoPickCandidate> candidates{};
                                candidates.reserve(
                                    thingCandidates.size()
                                    + lightCandidates.size()
                                    + surfaceCandidates.size() * 2u);
                                for (const IndexCandidate& candidate : thingCandidates)
                                {
                                    candidates.push_back(PreviewAutoPickCandidate{
                                        PreviewAutoPickKind::Thing,
                                        candidate.index,
                                        -1,
                                        candidate.depth });
                                }
                                for (const IndexCandidate& candidate : lightCandidates)
                                {
                                    candidates.push_back(PreviewAutoPickCandidate{
                                        PreviewAutoPickKind::BakedLight,
                                        candidate.index,
                                        -1,
                                        candidate.depth });
                                }
                                for (const SurfaceCandidate& candidate : surfaceCandidates)
                                {
                                    candidates.push_back(PreviewAutoPickCandidate{
                                        PreviewAutoPickKind::Surface,
                                        candidate.ref.sector,
                                        candidate.ref.surface,
                                        candidate.depth });
                                    candidates.push_back(PreviewAutoPickCandidate{
                                        PreviewAutoPickKind::Sector,
                                        candidate.ref.sector,
                                        -1,
                                        candidate.depth });
                                }

                                const double firstGeometryDepth =
                                    surfaceCandidates.empty()
                                    ? std::numeric_limits<double>::infinity()
                                    : static_cast<double>(surfaceCandidates.front().depth);
                                candidates = render::picking::orderedPreviewAutoPickCandidates(
                                    std::move(candidates),
                                    firstGeometryDepth,
                                    xraySelectionCyclingEnabled_ && cyclePick);

                                std::optional<PreviewAutoPickCandidate> current{};
                                if (selectedThing_ >= 0)
                                {
                                    current = PreviewAutoPickCandidate{
                                        PreviewAutoPickKind::Thing,
                                        selectedThing_, -1, 0.0 };
                                }
                                else if (selectedLight_ >= 0)
                                {
                                    current = PreviewAutoPickCandidate{
                                        PreviewAutoPickKind::BakedLight,
                                        selectedLight_, -1, 0.0 };
                                }
                                else if (selectedSurfaceSector_ >= 0
                                    && selectedSurface_ >= 0)
                                {
                                    current = PreviewAutoPickCandidate{
                                        PreviewAutoPickKind::Surface,
                                        selectedSurfaceSector_, selectedSurface_, 0.0 };
                                }
                                else if (selectedSector_ >= 0)
                                {
                                    current = PreviewAutoPickCandidate{
                                        PreviewAutoPickKind::Sector,
                                        selectedSector_, -1, 0.0 };
                                }

                                std::size_t pickIndex = 0u;
                                if (cyclePick && current)
                                {
                                    const auto it = std::find(
                                        candidates.begin(), candidates.end(), *current);
                                    if (it != candidates.end())
                                    {
                                        pickIndex = (static_cast<std::size_t>(
                                            std::distance(candidates.begin(), it)) + 1u)
                                            % candidates.size();
                                    }
                                }

                                if (!candidates.empty())
                                {
                                    const PreviewAutoPickCandidate& picked =
                                        candidates[pickIndex];
                                    newSector = -1;
                                    newSurface = -1;
                                    newThing = -1;
                                    newLight = -1;
                                    switch (picked.kind)
                                    {
                                        case PreviewAutoPickKind::Thing:
                                            newThing = picked.primaryIndex;
                                            break;
                                        case PreviewAutoPickKind::BakedLight:
                                            newLight = picked.primaryIndex;
                                            break;
                                        case PreviewAutoPickKind::Surface:
                                        {
                                            newSector = picked.primaryIndex;
                                            newSurface = picked.secondaryIndex;
                                            const auto it = std::find_if(
                                                surfaceCandidates.begin(),
                                                surfaceCandidates.end(),
                                                [&](const SurfaceCandidate& candidate)
                                                {
                                                    return candidate.ref.sector == newSector
                                                        && candidate.ref.surface == newSurface;
                                                });
                                            if (it != surfaceCandidates.end())
                                            {
                                                setSurfaceHitFromCandidate(*it);
                                            }
                                            break;
                                        }
                                        case PreviewAutoPickKind::Sector:
                                            newSector = picked.primaryIndex;
                                            break;
                                    }
                                }
                            }
                            else
                            {
                            // Auto-mode cycling: if the user repeatedly clicks in (roughly) the same spot,
                            // cycle within the currently selected kind (things, lights, etc.).
                            bool cycled = false;
                            if (cyclePick)
                            {
                                const std::int32_t curThing = currentThing();
                                if (curThing >= 0 && !thingCandidates.empty())
                                {
                                    const auto it = std::find_if(
                                        thingCandidates.begin(),
                                        thingCandidates.end(),
                                        [&](const IndexCandidate& hit)
                                        {
                                            return hit.index == curThing;
                                        });

                                    if (it != thingCandidates.end() && thingCandidates.size() > 1)
                                    {
                                        const std::size_t curIndex = static_cast<std::size_t>(it - thingCandidates.begin());
                                        const std::size_t pickIndex = (curIndex + 1u) % thingCandidates.size();

                                        newSector = -1;
                                        newSurface = -1;
                                        newThing = thingCandidates[pickIndex].index;
                                        newLight = -1;
                                        cycled = true;
                                    }
                                }
                            }

                            if (!cycled)
                            {
                                float bestDepth = std::numeric_limits<float>::infinity();
                                enum class BestKind : std::uint8_t
                                {
                                    none,
                                    surface,
                                    thing,
                                    light
                                };

                                BestKind bestKind = BestKind::none;

                                const SurfaceCandidate* bestSurface{};
                                const SurfaceCandidate* nextSurface{};
                                if (!surfaceCandidates.empty())
                                {
                                    if (surfaceSubMode_ != SurfaceSubMode::Face)
                                    {
                                        bestSurface = &surfaceCandidates.front();
                                        if (surfaceCandidates.size() > 1u)
                                        {
                                            nextSurface = &surfaceCandidates[1];
                                        }
                                    }
                                    else if (level_)
                                    {
                                        auto facePickable = [&](const SurfaceRef ref) -> bool
                                        {
                                            if (ref.sector < 0 || ref.surface < 0)
                                            {
                                                return false;
                                            }
                                            const std::size_t secIndex = static_cast<std::size_t>(ref.sector);
                                            if (secIndex >= level_->sectors.size())
                                            {
                                                return false;
                                            }
                                            const Sector& sector = level_->sectors[secIndex];
                                            const std::size_t surfIndex = static_cast<std::size_t>(ref.surface);
                                            if (surfIndex >= sector.surfaces.size())
                                            {
                                                return false;
                                            }

                                            const Surface& surface = sector.surfaces[surfIndex];
                                            if (!allowThroughSurfacePick
                                                && surface.geo < static_cast<std::int32_t>(TGeoMode::Solid))
                                            {
                                                return false;
                                            }
                                            if (allowThroughSurfacePick)
                                            {
                                                return true;
                                            }
                                            if ((surface.faceFlags & FF_DoubleSided) != 0u)
                                            {
                                                return true;
                                            }
                                            return surface.normal.Dot(direction) <= 1e-9;
                                        };

                                        for (const SurfaceCandidate& cand : surfaceCandidates)
                                        {
                                            if (facePickable(cand.ref))
                                            {
                                                if (!bestSurface)
                                                {
                                                    bestSurface = &cand;
                                                }
                                                else
                                                {
                                                    nextSurface = &cand;
                                                    break;
                                                }
                                            }
                                        }
                                    }
                                }

                                if (bestSurface && bestSurface->depth < bestDepth)
                                {
                                    bestDepth = bestSurface->depth;
                                    bestKind = BestKind::surface;
                                }
                                if (!thingCandidates.empty() && thingCandidates.front().depth < bestDepth)
                                {
                                    bestDepth = thingCandidates.front().depth;
                                    bestKind = BestKind::thing;
                                }
                                if (!lightCandidates.empty() && lightCandidates.front().depth < bestDepth)
                                {
                                    bestDepth = lightCandidates.front().depth;
                                    bestKind = BestKind::light;
                                }

                                if (bestKind == BestKind::surface)
                                {
                                    const bool skipFront = render::picking::shouldSkipSelectedFrontCandidate(
                                        xraySelectionCyclingEnabled_,
                                        xrayCycleSelectionClick,
                                        surfaceIsSelected(bestSurface->ref));
                                    const SurfaceCandidate& cand = skipFront && nextSurface
                                        ? *nextSurface
                                        : *bestSurface;
                                    newSector = cand.ref.sector;
                                    newSurface = cand.ref.surface;
                                    newThing = -1;
                                    newLight = -1;
                                    setSurfaceHitFromCandidate(cand);
                                }
                                else if (bestKind == BestKind::thing)
                                {
                                    const bool skipFront = render::picking::shouldSkipSelectedFrontCandidate(
                                        xraySelectionCyclingEnabled_,
                                        xrayCycleSelectionClick,
                                        thingIsSelected(thingCandidates.front().index));
                                    const std::size_t pickIndex = skipFront && thingCandidates.size() > 1u ? 1u : 0u;
                                    newSector = -1;
                                    newSurface = -1;
                                    newThing = thingCandidates[pickIndex].index;
                                    newLight = -1;
                                }
                                else if (bestKind == BestKind::light)
                                {
                                    const bool skipFront = render::picking::shouldSkipSelectedFrontCandidate(
                                        xraySelectionCyclingEnabled_,
                                        xrayCycleSelectionClick,
                                        lightIsSelected(lightCandidates.front().index));
                                    const std::size_t pickIndex = skipFront && lightCandidates.size() > 1u ? 1u : 0u;
                                    newSector = -1;
                                    newSurface = -1;
                                    newThing = -1;
                                    newLight = lightCandidates[pickIndex].index;
                                }
                            }
                            }
                        }
                    }

                    if (!canGpuPick)
                    {
                        std::vector<sed::level::SurfacePickHit> surfaceHits =
                            surfacePickIndex_.pickSurfaces(
                                *level_, origin, direction,
                                cpuRayMaxDistance);

                    if (!previewAutoSelection
                        && strictSurfaceSelectionEnabled_
                        && (surfaceSubMode_ == SurfaceSubMode::Edge
                            || surfaceSubMode_ == SurfaceSubMode::Vertex))
                    {
                        std::erase_if(
                            surfaceHits,
                            [this](const sed::level::SurfacePickHit& hit)
                            {
                                return !strictSurfaceSelectionScope_.contains(hit.surface);
                            });
                    }

                    if (!allowThroughSurfacePick)
                    {
                        surfaceHits.erase(std::remove_if(surfaceHits.begin(), surfaceHits.end(), [&](const sed::level::SurfacePickHit& hit)
                            {
                                const std::int32_t sectorIndex = hit.surface.sector;
                                const std::int32_t surfaceIndex = hit.surface.surface;

                                if (sectorIndex < 0 || surfaceIndex < 0)
                                {
                                    return true;
                                }
                                if (static_cast<std::size_t>(sectorIndex) >= level_->sectors.size())
                                {
                                    return true;
                                }

                                const Sector& sector = level_->sectors[static_cast<std::size_t>(sectorIndex)];
                                if (static_cast<std::size_t>(surfaceIndex) >= sector.surfaces.size())
                                {
                                    return true;
                                }

                                const Surface& surface = sector.surfaces[static_cast<std::size_t>(surfaceIndex)];
                                return surface.geo < static_cast<std::int32_t>(sed::TGeoMode::Solid);
                            }),
                            surfaceHits.end());
                    }

                    // Sector picking selects the enclosing volume, not an
                    // oriented face.  Keep back-facing walls in that mode so
                    // an inside-camera click (or a click through the outward
                    // wall) still yields the front sector. Surface, Thing, and
                    // Preview Auto picking use only visible-facing solid
                    // boundaries. This mirrors the GPU path's culling policy.
                    if (render::picking::solidLevelOccludersCullBackFaces(
                            solidLevelOcclusionFamily,
                            collectThroughSurfaceOcclusion))
                    {
                        surfaceHits.erase(std::remove_if(surfaceHits.begin(), surfaceHits.end(), [&](const sed::level::SurfacePickHit& hit)
                            {
                                const std::int32_t sectorIndex = hit.surface.sector;
                                const std::int32_t surfaceIndex = hit.surface.surface;
                                if (sectorIndex < 0 || surfaceIndex < 0)
                                {
                                    return true;
                                }
                                if (static_cast<std::size_t>(sectorIndex) >= level_->sectors.size())
                                {
                                    return true;
                                }

                                const Sector& sector = level_->sectors[static_cast<std::size_t>(sectorIndex)];
                                if (static_cast<std::size_t>(surfaceIndex) >= sector.surfaces.size())
                                {
                                    return true;
                                }

                                const Surface& surface = sector.surfaces[static_cast<std::size_t>(surfaceIndex)];
                                return !render::picking::solidLevelOccluderPassesFacing(
                                    solidLevelOcclusionFamily,
                                    (surface.faceFlags & FF_DoubleSided) != 0u,
                                    surface.normal.Dot(direction) > 1.0e-9,
                                    collectThroughSurfaceOcclusion);
                            }),
                            surfaceHits.end());
                    }

                    // Resolve the nearest eligible wall after ordinary facing
                    // filters. An invisible single-sided backface cannot hide
                    // a visible Surface element or Thing behind it. Preserve
                    // every coplanar/shared-edge eligible owner.
                    if (!collectThroughSurfaceOcclusion && surfaceHits.size() > 1)
                    {
                        const double nearestDistance = surfaceHits.front().distance;
                        std::erase_if(
                            surfaceHits,
                            [nearestDistance](const sed::level::SurfacePickHit& hit)
                            {
                                return !render::picking::
                                    surfaceRayCandidatePassesNearestBoundary(
                                        hit.distance,
                                        nearestDistance,
                                        false);
                            });
                    }

                    if (!surfaceHits.empty())
                    {
                        surfaceHit = surfaceHits.front();
                    }

                    const double surfaceT = surfaceHit ? surfaceHit->distance : std::numeric_limits<double>::infinity();

                    constexpr float kPickPixelTol = 8.0f;
                    const float surfaceWireBroadPhaseRadiusPx =
                        mode == PickMode::Surface
                            && (surfaceSubMode_ == SurfaceSubMode::Edge
                                || surfaceSubMode_ == SurfaceSubMode::Vertex)
                        ? render::picking::kSurfaceSubElementPickRadiusPx
                        : kPickPixelTol;
                    const bool isOrtho = camera_.projectionType() == app::ViewportCamera::ProjectionType::Orthographic;
                    const float tanHalfFov = [&]() -> float
                    {
                        if (isOrtho)
                        {
                            return 0.0f;
                        }

                        const float halfFovRad = deg2Rad(camera_.fovXDeg() * 0.5f);
                        return std::tan(std::max(halfFovRad, 0.001f));
                    }();

                    auto worldTolAtT = [&](
                        const double t,
                        const float logicalPixelRadius) -> double
                    {
                        if (isOrtho)
                        {
                            const double worldH = static_cast<double>(camera_.orthoTop() - camera_.orthoBottom());
                            const double perPixel = (height() > 0) ? (worldH / static_cast<double>(height())) : 0.0;
                            return std::max(
                                perPixel * static_cast<double>(logicalPixelRadius),
                                0.001);
                        }
                        return render::picking::perspectivePickWorldTolerance(
                            t,
                            static_cast<double>(tanHalfFov),
                            width(),
                            static_cast<double>(logicalPixelRadius));
                    };

                    auto tryHitPoint = [&](const double px, const double py, const double pz) -> std::optional<double>
                    {
                        const TVector v = TVector{ px, py, pz } - origin;
                        const double t = v.Dot(direction);
                        if (t < 0.0 || t > cpuRayMaxDistance)
                        {
                            return std::nullopt;
                        }

                        const TVector q = origin + direction * t;
                        const TVector d = TVector{ px, py, pz } - q;
                        const double tol = worldTolAtT(t, kPickPixelTol);
                        if (d.Dot(d) > (tol * tol))
                        {
                            return std::nullopt;
                        }

                        return t;
                    };

                    auto collectSurfaceEdgeHits = [&]() -> std::vector<sed::level::SurfacePickHit>
                    {
                        std::vector<sed::level::SurfacePickHit> hits{};
                        hits.reserve(surfaceHits.size());

                        const TVector rayEnd =
                            origin + direction * cpuRayMaxDistance;

                        for (std::size_t sectorIndex = 0; sectorIndex < level_->sectors.size(); ++sectorIndex)
                        {
                            const Sector& sector = level_->sectors[sectorIndex];
                            if (!isLayerVisible(sector.layer))
                            {
                                continue;
                            }

                            for (std::size_t surfaceIndex = 0; surfaceIndex < sector.surfaces.size(); ++surfaceIndex)
                            {
                                const Surface& surface = sector.surfaces[surfaceIndex];
                                if (!allowThroughSurfacePick
                                    && !wireVisible
                                    && surface.geo < static_cast<std::int32_t>(sed::TGeoMode::Solid))
                                {
                                    continue;
                                }

                                const auto& vxs = surface.vertices;
                                if (vxs.size() < 2)
                                {
                                    continue;
                                }

                                bool found = false;
                                double bestT = std::numeric_limits<double>::infinity();
                                TVector bestPos{};

                                for (std::size_t i = 0; i < vxs.size(); ++i)
                                {
                                    const std::int32_t aIndex = vxs[i];
                                    const std::int32_t bIndex = vxs[(i + 1) % vxs.size()];
                                    if (aIndex < 0 || bIndex < 0)
                                    {
                                        continue;
                                    }

                                    if (static_cast<std::size_t>(aIndex) >= sector.vertices.size()
                                        || static_cast<std::size_t>(bIndex) >= sector.vertices.size())
                                    {
                                        continue;
                                    }

                                    const Vertex& av = sector.vertices[static_cast<std::size_t>(aIndex)];
                                    const Vertex& bv = sector.vertices[static_cast<std::size_t>(bIndex)];

                                    const TVector a{ av.x, av.y, av.z };
                                    const TVector b{ bv.x, bv.y, bv.z };

                                    const SegmentClosestPoints cp = segmentCalcClosestPoints(origin, rayEnd, a, b);
                                    const double t = (cp.p - origin).Dot(direction);
                                    if (t < 0.0 || t > cpuRayMaxDistance)
                                    {
                                        continue;
                                    }

                                    const double tol = worldTolAtT(
                                        t,
                                        surfaceWireBroadPhaseRadiusPx);
                                    if (cp.distSqr > (tol * tol))
                                    {
                                        continue;
                                    }

                                    if (t < bestT)
                                    {
                                        bestT = t;
                                        bestPos = cp.q;
                                        found = true;
                                    }
                                }

                                if (!found)
                                {
                                    continue;
                                }

                                sed::level::SurfacePickHit hit{};
                                hit.surface.sector = static_cast<std::int32_t>(sectorIndex);
                                hit.surface.surface = static_cast<std::int32_t>(surfaceIndex);
                                hit.position = bestPos;
                                hit.distance = bestT;
                                hits.push_back(hit);
                            }
                        }

                        std::sort(hits.begin(), hits.end(), [](const sed::level::SurfacePickHit& a, const sed::level::SurfacePickHit& b)
                            {
                                if (a.distance != b.distance)
                                {
                                    return a.distance < b.distance;
                                }
                                if (a.surface.sector != b.surface.sector)
                                {
                                    return a.surface.sector < b.surface.sector;
                                }
                                return a.surface.surface < b.surface.surface;
                            });

                        return hits;
                    };

                    struct IndexHit
                    {
                        std::int32_t index{};
                        double distance{};
                    };

                    std::vector<IndexHit> thingHits{};
                    thingHits.reserve(level_->things.size());

                    double bestThingT = std::numeric_limits<double>::infinity();
                    for (std::size_t i = 0; i < level_->things.size(); ++i)
                    {
                        const Thing& thing = level_->things[i];
                        if (!isLayerVisible(thing.layer))
                        {
                            continue;
                        }
                        if (!shouldShowThing(thing))
                        {
                            continue;
                        }

                        if (!std::isfinite(thing.x) || !std::isfinite(thing.y) || !std::isfinite(thing.z))
                        {
                            continue;
                        }

                        const TplValue* modelValue = sed::level::thingFindValue(*level_, templates_, thing, "model3d");
                        if (!modelValue)
                        {
                            modelValue = sed::level::thingFindValue(*level_, templates_, thing, "model3do");
                        }

                        const std::string modelName = modelValue
                            ? (modelValue->stringValue.empty() ? modelValue->asString() : modelValue->stringValue)
                            : std::string{};

                        // The CPU fallback must test the same geoset that was
                        // last published for drawing. A camera or header edit
                        // can change the requested tier; render state records
                        // the actual visible tier, including HQ fallback for
                        // an unavailable lower geoset. An absent render state
                        // safely starts with HQ until a variant is published.
                        const std::size_t displayedLodIndex =
                            thingModelLodEnabled_
                                && level_->kind != ProjectType::IJIM
                                && i < thingRenderState_.size()
                            ? thingRenderState_[i].lodIndex
                            : 0u;
                        std::shared_ptr<const assets::Model3do> modelPtr =
                            loadModel3do(modelName, displayedLodIndex);
                        if ((!modelPtr || modelPtr->meshes.empty())
                            && displayedLodIndex != 0u)
                        {
                            modelPtr = loadModel3do(modelName);
                        }
                        if (!modelPtr || modelPtr->meshes.empty())
                        {
                            const std::int32_t thingIndex32 = static_cast<std::int32_t>(i);
                            if (const std::optional<double> t = tryHitPoint(thing.x, thing.y, thing.z))
                            {
                                thingHits.push_back(IndexHit{ thingIndex32, *t });
                                if (*t < bestThingT)
                                {
                                    bestThingT = *t;
                                    newThing = thingIndex32;
                                }
                            }
                            continue;
                        }

                        const TVector sphereCenter{ thing.x, thing.y, thing.z };
                        const double radius = modelPtr->radius;
                        if (radius > 0.0)
                        {
                            const TVector toCenter = sphereCenter - origin;
                            const double centerDist2 = toCenter.Dot(toCenter);
                            const double tca = toCenter.Dot(direction);
                            if (tca < -radius)
                            {
                                continue;
                            }

                            const double d2 = centerDist2 - (tca * tca);
                            if (d2 > (radius * radius))
                            {
                                continue;
                            }
                        }

                        TVector pyr{};
                        pyr.pitch = thing.pch;
                        pyr.yaw = thing.yaw;
                        pyr.roll = thing.rol;
                        const TMatrix43 xf = TMatrix43::Create(TVector{ thing.x, thing.y, thing.z }, pyr);

                        double bestThingLocalT = std::numeric_limits<double>::infinity();

                        for (const assets::Model3doMesh& mesh : modelPtr->meshes)
                        {
                            std::vector<TVector> worldVertices(mesh.vertices.size());
                            for (std::size_t vi = 0; vi < mesh.vertices.size(); ++vi)
                            {
                                const assets::Model3doVertex& mv = mesh.vertices[vi];
                                worldVertices[vi] = xf.TransformPoint(TVector{ mv.x, mv.y, mv.z });
                            }

                            for (const assets::Model3doFace& face : mesh.faces)
                            {
                                if (face.geo < static_cast<std::int32_t>(sed::TGeoMode::Wireframe))
                                {
                                    continue;
                                }

                                const std::size_t count = face.vertices.size();
                                if (count < 3)
                                {
                                    continue;
                                }

                                TVector normal{};
                                bool indicesValid = true;
                                for (std::size_t n = 0; n < count; ++n)
                                {
                                    const std::int32_t aIndex = face.vertices[n];
                                    const std::int32_t bIndex = face.vertices[(n + 1) % count];

                                    if (aIndex < 0 || bIndex < 0)
                                    {
                                        indicesValid = false;
                                        break;
                                    }
                                    if (static_cast<std::size_t>(aIndex) >= worldVertices.size()
                                        || static_cast<std::size_t>(bIndex) >= worldVertices.size())
                                    {
                                        indicesValid = false;
                                        break;
                                    }

                                    const TVector& a = worldVertices[static_cast<std::size_t>(aIndex)];
                                    const TVector& b = worldVertices[static_cast<std::size_t>(bIndex)];

                                    normal.x += (a.y - b.y) * (a.z + b.z);
                                    normal.y += (a.z - b.z) * (a.x + b.x);
                                    normal.z += (a.x - b.x) * (a.y + b.y);
                                }

                                if (!indicesValid || normal.IsZero())
                                {
                                    continue;
                                }

                                const bool doubleSided = (face.faceFlags & sed::FF_DoubleSided) != 0;
                                if (!doubleSided && normal.Dot(direction) > 0.0)
                                {
                                    continue;
                                }

                                const std::int32_t v0Index = face.vertices[0];
                                if (v0Index < 0 || static_cast<std::size_t>(v0Index) >= worldVertices.size())
                                {
                                    continue;
                                }

                                const TVector& v0 = worldVertices[static_cast<std::size_t>(v0Index)];
                                const double d = normal.Dot(v0);

                                const double denom = normal.Dot(direction);
                                if (std::abs(denom) < sed::math::kTinyEpsilon)
                                {
                                    continue;
                                }

                                const double t = (d - normal.Dot(origin)) / denom;
                                if (t < 0.0 || t > cpuRayMaxDistance
                                    || t > bestThingLocalT)
                                {
                                    continue;
                                }

                                const TVector p = origin + direction * t;

                                bool inside = true;
                                for (std::size_t n = 0; n < count; ++n)
                                {
                                    const std::int32_t aIndex = face.vertices[n];
                                    const std::int32_t bIndex = face.vertices[(n + 1) % count];
                                    if (aIndex < 0 || bIndex < 0)
                                    {
                                        inside = false;
                                        break;
                                    }
                                    if (static_cast<std::size_t>(aIndex) >= worldVertices.size()
                                        || static_cast<std::size_t>(bIndex) >= worldVertices.size())
                                    {
                                        inside = false;
                                        break;
                                    }

                                    const TVector& a = worldVertices[static_cast<std::size_t>(aIndex)];
                                    const TVector& b = worldVertices[static_cast<std::size_t>(bIndex)];

                                    const TVector vct = normal.Cross(b - a);
                                    const double dist = vct.Dot(p - a);
                                    if (dist < -CloseEnough)
                                    {
                                        inside = false;
                                        break;
                                    }
                                }

                                if (!inside)
                                {
                                    continue;
                                }

                                bestThingLocalT = t;
                            }
                        }

                        if (bestThingLocalT < std::numeric_limits<double>::infinity())
                        {
                            const std::int32_t thingIndex32 = static_cast<std::int32_t>(i);
                            thingHits.push_back(IndexHit{ thingIndex32, bestThingLocalT });

                            if (bestThingLocalT < bestThingT)
                            {
                                bestThingT = bestThingLocalT;
                                newThing = thingIndex32;
                            }
                        }
                    }

                    if (!overlayThingCandidates.empty())
                    {
                        bool inverseMvpOk = false;
                        const QMatrix4x4 inverseMvp = mvp.inverted(&inverseMvpOk);
                        if (inverseMvpOk && width() > 0 && height() > 0)
                        {
                            for (const OverlayThingPointCandidate& overlay
                                 : overlayThingCandidates)
                            {
                                const float ndcX = 2.0f * static_cast<float>(
                                    overlay.sample.x() / static_cast<double>(width())) - 1.0f;
                                const float ndcY = 1.0f - 2.0f * static_cast<float>(
                                    overlay.sample.y() / static_cast<double>(height()));
                                const QVector4D worldH = inverseMvp * QVector4D(
                                    ndcX,
                                    ndcY,
                                    overlay.depth * 2.0f - 1.0f,
                                    1.0f);
                                if (!std::isfinite(worldH.x())
                                    || !std::isfinite(worldH.y())
                                    || !std::isfinite(worldH.z())
                                    || !std::isfinite(worldH.w())
                                    || std::abs(worldH.w())
                                        <= std::numeric_limits<float>::epsilon())
                                {
                                    continue;
                                }

                                const QVector3D worldPoint =
                                    worldH.toVector3D() / worldH.w();
                                const TVector worldPosition{
                                    static_cast<double>(worldPoint.x()),
                                    static_cast<double>(worldPoint.y()),
                                    static_cast<double>(worldPoint.z()) };
                                const double distance =
                                    (worldPosition - origin).Dot(direction);
                                if (!std::isfinite(distance)
                                    || distance < 0.0
                                    || distance > cpuRayMaxDistance)
                                {
                                    continue;
                                }

                                const auto existing = std::find_if(
                                    thingHits.begin(),
                                    thingHits.end(),
                                    [&overlay](const IndexHit& hit)
                                    {
                                        return hit.index == overlay.index;
                                    });
                                if (existing == thingHits.end())
                                {
                                    thingHits.push_back(IndexHit{
                                        overlay.index,
                                        distance });
                                }
                                else
                                {
                                    existing->distance = std::min(
                                        existing->distance,
                                        distance);
                                }
                                if (distance < bestThingT)
                                {
                                    bestThingT = distance;
                                    newThing = overlay.index;
                                }
                            }
                        }
                    }

                    std::sort(thingHits.begin(), thingHits.end(), [](const IndexHit& a, const IndexHit& b)
                        {
                            return a.distance < b.distance;
                        });
                    if (std::isfinite(surfaceT) && !thingHits.empty())
                    {
                        constexpr double kThingOcclusionEpsilon = 1.0e-6;
                        thingHits.erase(
                            std::remove_if(
                                thingHits.begin(),
                                thingHits.end(),
                                [surfaceT,
                                 allowThroughThingPick,
                                 xraySelectionCyclingEnabled = xraySelectionCyclingEnabled_,
                                 xrayCandidateTraversal](const IndexHit& hit)
                                {
                                    return !render::picking::candidatePassesOcclusion(
                                        hit.distance,
                                        surfaceT,
                                        allowThroughThingPick,
                                        xraySelectionCyclingEnabled,
                                        xrayCandidateTraversal,
                                        kThingOcclusionEpsilon);
                                }),
                            thingHits.end());
                    }

                    if (!collectThroughThingOcclusion && !previewAutoSelection)
                    {
                        if (thingHits.size() > 1)
                        {
                            thingHits.erase(thingHits.begin() + 1, thingHits.end());
                        }

                        bestThingT = std::numeric_limits<double>::infinity();
                        newThing = -1;
                        for (const IndexHit& hit : thingHits)
                        {
                            if (hit.distance < bestThingT)
                            {
                                bestThingT = hit.distance;
                                newThing = hit.index;
                            }
                        }
                    }

                    std::vector<IndexHit> lightHits{};
                    lightHits.reserve(level_->lights.size());

                    double bestLightT = std::numeric_limits<double>::infinity();
                    if (previewBakedLightPickable)
                    {
                        for (std::size_t i = 0; i < level_->lights.size(); ++i)
                        {
                            const SedLight& light = level_->lights[i];
                            if (!isLayerVisible(light.layer))
                            {
                                continue;
                            }
                            if (const std::optional<double> t = tryHitPoint(light.position.x, light.position.y, light.position.z))
                            {
                                const std::int32_t lightIndex32 = static_cast<std::int32_t>(i);
                                lightHits.push_back(IndexHit{ lightIndex32, *t });
                                if (*t < bestLightT)
                                {
                                    bestLightT = *t;
                                    newLight = lightIndex32;
                                }
                            }
                        }
                    }

                    std::sort(lightHits.begin(), lightHits.end(), [](const IndexHit& a, const IndexHit& b)
                        {
                            return a.distance < b.distance;
                        });

                    if ((mode == PickMode::Sector || mode == PickMode::Surface)
                        && (wireVisible
                            || allowThroughSurfacePick
                            || (xrayCyclePick
                                && mode == PickMode::Surface
                                && surfaceSubMode_ != SurfaceSubMode::Face)))
                    {
                        std::vector<sed::level::SurfacePickHit> edgeHits =
                            collectSurfaceEdgeHits();
                        if (!edgeHits.empty()
                            && !allowThroughSurfacePick
                            && !xrayCyclePick)
                        {
                            if (mode == PickMode::Surface)
                            {
                                std::erase_if(
                                    edgeHits,
                                    [&](const sed::level::SurfacePickHit& hit)
                                    {
                                        if (hit.surface.sector < 0
                                            || hit.surface.surface < 0
                                            || static_cast<std::size_t>(hit.surface.sector)
                                                >= level_->sectors.size())
                                        {
                                            return true;
                                        }
                                        const Sector& sector = level_->sectors[
                                            static_cast<std::size_t>(hit.surface.sector)];
                                        if (static_cast<std::size_t>(hit.surface.surface)
                                            >= sector.surfaces.size())
                                        {
                                            return true;
                                        }
                                        const Surface& surface = sector.surfaces[
                                            static_cast<std::size_t>(hit.surface.surface)];
                                        return !render::picking::
                                            surfacePointOwnerPassesFacing(
                                                (surface.faceFlags & FF_DoubleSided) != 0u,
                                                surface.normal.Dot(direction) > 1.0e-9,
                                                false);
                                    });
                            }
                            struct EdgeDepthCandidate
                            {
                                sed::level::SurfacePickHit hit{};
                                float depth{1.0f};
                            };
                            std::vector<EdgeDepthCandidate> candidates{};
                            std::vector<QPointF> samplePoints{};
                            candidates.reserve(edgeHits.size());
                            samplePoints.reserve(edgeHits.size());
                            for (const sed::level::SurfacePickHit& hit : edgeHits)
                            {
                                const QVector3D world(
                                    static_cast<float>(hit.position.x),
                                    static_cast<float>(hit.position.y),
                                    static_cast<float>(hit.position.z));
                                QPointF screen{};
                                const QVector4D clip = mvp * QVector4D(world, 1.0f);
                                if (!render::projectWorldPointToScreen(
                                        world, mvp, width(), height(), screen)
                                    || !std::isfinite(clip.z())
                                    || !std::isfinite(clip.w())
                                    || std::abs(clip.w())
                                        <= std::numeric_limits<float>::epsilon())
                                {
                                    continue;
                                }
                                const float depth =
                                    (clip.z() / clip.w()) * 0.5f + 0.5f;
                                if (!std::isfinite(depth)
                                    || depth < 0.0f || depth > 1.0f)
                                {
                                    continue;
                                }
                                candidates.push_back(EdgeDepthCandidate{ hit, depth });
                                samplePoints.push_back(screen);
                            }
                            const std::vector<float> sceneDepths =
                                pickSceneDepthsAtPoints(
                                    mvp,
                                    samplePoints,
                                    showSolid_,
                                    showThings_
                                        && thingViewMode_
                                            == ThingViewMode::Shaded,
                                    mode == PickMode::Surface);
                            edgeHits.clear();
                            edgeHits.reserve(candidates.size());
                            for (std::size_t candidateIndex = 0;
                                 candidateIndex < candidates.size();
                                 ++candidateIndex)
                            {
                                if (candidateIndex >= sceneDepths.size()
                                    || !render::picking::surfaceSelectionCandidatePassesSceneDepth(
                                        candidates[candidateIndex].depth,
                                        sceneDepths[candidateIndex],
                                        false,
                                        false))
                                {
                                    continue;
                                }
                                edgeHits.push_back(candidates[candidateIndex].hit);
                            }
                        }
                        if (!edgeHits.empty())
                        {
                            auto surfaceKey = [](const SurfaceRef& ref) -> std::uint64_t
                            {
                                const std::uint64_t hi = static_cast<std::uint64_t>(static_cast<std::uint32_t>(ref.sector));
                                const std::uint64_t lo = static_cast<std::uint64_t>(static_cast<std::uint32_t>(ref.surface));
                                return (hi << 32) | lo;
                            };

                            std::unordered_map<std::uint64_t, sed::level::SurfacePickHit> bestBySurface{};
                            bestBySurface.reserve(surfaceHits.size() + edgeHits.size());

                            auto addHit = [&](const sed::level::SurfacePickHit& hit)
                            {
                                const std::uint64_t key = surfaceKey(hit.surface);
                                const auto it = bestBySurface.find(key);
                                if (it == bestBySurface.end())
                                {
                                    bestBySurface.emplace(key, hit);
                                    return;
                                }
                                if (hit.distance < it->second.distance)
                                {
                                    it->second = hit;
                                }
                            };

                            for (const sed::level::SurfacePickHit& hit : surfaceHits)
                            {
                                addHit(hit);
                            }
                            for (const sed::level::SurfacePickHit& hit : edgeHits)
                            {
                                addHit(hit);
                            }

                            surfaceHits.clear();
                            surfaceHits.reserve(bestBySurface.size());
                            for (const auto& entry : bestBySurface)
                            {
                                surfaceHits.push_back(entry.second);
                            }

                            std::sort(surfaceHits.begin(), surfaceHits.end(), [](const sed::level::SurfacePickHit& a, const sed::level::SurfacePickHit& b)
                                {
                                    if (a.distance != b.distance)
                                    {
                                        return a.distance < b.distance;
                                    }
                                    if (a.surface.sector != b.surface.sector)
                                    {
                                        return a.surface.sector < b.surface.sector;
                                    }
                                    return a.surface.surface < b.surface.surface;
                                });

                            if (!surfaceHits.empty())
                            {
                                surfaceHit = surfaceHits.front();
                            }
                        }
                    }

                    const auto currentSector = [&]() -> std::int32_t
                    {
                        if (selectedSector_ >= 0)
                        {
                            return selectedSector_;
                        }
                        if (!highlightedSelection_.sectors.empty())
                        {
                            return *highlightedSelection_.sectors.begin();
                        }
                        return selectedSurfaceSector_;
                    };

                    const auto currentSurface = [&]() -> SurfaceRef
                    {
                        if (selectedSurface_ >= 0 && selectedSurfaceSector_ >= 0)
                        {
                            return SurfaceRef{ selectedSurfaceSector_, selectedSurface_ };
                        }
                        if (!highlightedSelection_.surfaces.empty())
                        {
                            return *highlightedSelection_.surfaces.begin();
                        }
                        return SurfaceRef{ -1, -1 };
                    };

                    const auto currentThing = [&]() -> std::int32_t
                    {
                        if (selectedThing_ >= 0)
                        {
                            return selectedThing_;
                        }
                        if (!highlightedSelection_.things.empty())
                        {
                            return *highlightedSelection_.things.begin();
                        }
                        return -1;
                    };

                    const auto currentLight = [&]() -> std::int32_t
                    {
                        if (selectedLight_ >= 0)
                        {
                            return selectedLight_;
                        }
                        if (!highlightedSelection_.lights.empty())
                        {
                            return *highlightedSelection_.lights.begin();
                        }
                        return -1;
                    };

                    const QPointF mpSector(releasePos);
                    auto projectSectorPointToScreen = [&](const TVector& wp, QPointF& outPos) -> bool
                    {
                        const QVector4D clip = mvp * QVector4D(
                            static_cast<float>(wp.x),
                            static_cast<float>(wp.y),
                            static_cast<float>(wp.z),
                            1.0f);
                        if (qFuzzyIsNull(clip.w()) || clip.w() <= 0.0f)
                        {
                            return false;
                        }

                        const QVector3D ndc(clip.x() / clip.w(), clip.y() / clip.w(), clip.z() / clip.w());
                        const float sx = (ndc.x() * 0.5f + 0.5f) * static_cast<float>(width());
                        const float sy = (1.0f - (ndc.y() * 0.5f + 0.5f)) * static_cast<float>(height());
                        outPos = QPointF(sx, sy);
                        return true;
                    };

                    auto surfaceSectorEdgeDistanceSq = [&](const SurfaceRef ref) -> std::optional<float>
                    {
                        if (!level_ || ref.sector < 0 || ref.surface < 0)
                        {
                            return std::nullopt;
                        }

                        const std::size_t sectorIndex = static_cast<std::size_t>(ref.sector);
                        if (sectorIndex >= level_->sectors.size())
                        {
                            return std::nullopt;
                        }

                        const Sector& sector = level_->sectors[sectorIndex];
                        const std::size_t surfaceIndex = static_cast<std::size_t>(ref.surface);
                        if (surfaceIndex >= sector.surfaces.size())
                        {
                            return std::nullopt;
                        }

                        const Surface& surface = sector.surfaces[surfaceIndex];
                        const auto& vxs = surface.vertices;
                        if (vxs.size() < 2)
                        {
                            return std::nullopt;
                        }

                        float best = std::numeric_limits<float>::infinity();
                        for (std::size_t i = 0; i < vxs.size(); ++i)
                        {
                            const std::int32_t aidx = vxs[i];
                            const std::int32_t bidx = vxs[(i + 1) % vxs.size()];
                            if (aidx < 0 || bidx < 0)
                            {
                                continue;
                            }
                            if (static_cast<std::size_t>(aidx) >= sector.vertices.size()
                                || static_cast<std::size_t>(bidx) >= sector.vertices.size())
                            {
                                continue;
                            }

                            const Vertex& va = sector.vertices[static_cast<std::size_t>(aidx)];
                            const Vertex& vb = sector.vertices[static_cast<std::size_t>(bidx)];
                            QPointF sa{};
                            QPointF sb{};
                            if (!projectSectorPointToScreen(TVector{ va.x, va.y, va.z }, sa)
                                || !projectSectorPointToScreen(TVector{ vb.x, vb.y, vb.z }, sb))
                            {
                                continue;
                            }

                            best = std::min(best, distanceSqPointToSegment(mpSector, sa, sb));
                        }

                        if (!std::isfinite(best))
                        {
                            return std::nullopt;
                        }

                        return best;
                    };

                    if (mode == PickMode::Sector)
                    {
                        std::vector<std::int32_t> sectorCandidates{};
                        sectorCandidates.reserve(surfaceHits.size());

                        constexpr float kSectorWirePickTolPx = 10.0f;
                        const float sectorWirePickTol2 = kSectorWirePickTolPx * kSectorWirePickTolPx;

                        std::unordered_set<std::int32_t> seenSectors{};
                        std::vector<std::int32_t> edgeSectorCandidates{};
                        if (wireVisible)
                        {
                            std::vector<std::pair<std::int32_t, float>> rankedEdgeSectorCandidates{};
                            rankedEdgeSectorCandidates.reserve(level_->sectors.size());

                            for (std::size_t sectorIndex = 0; sectorIndex < level_->sectors.size(); ++sectorIndex)
                            {
                                const Sector& sector = level_->sectors[sectorIndex];
                                if (!isLayerVisible(sector.layer))
                                {
                                    continue;
                                }

                                float bestD2 = std::numeric_limits<float>::infinity();
                                bool found = false;
                                for (std::size_t surfaceIndex = 0; surfaceIndex < sector.surfaces.size(); ++surfaceIndex)
                                {
                                    const std::optional<float> d2 = surfaceSectorEdgeDistanceSq(
                                        SurfaceRef{
                                            static_cast<std::int32_t>(sectorIndex),
                                            static_cast<std::int32_t>(surfaceIndex) });
                                    if (!d2 || *d2 > sectorWirePickTol2)
                                    {
                                        continue;
                                    }

                                    bestD2 = std::min(bestD2, *d2);
                                    found = true;
                                }

                                if (found)
                                {
                                    rankedEdgeSectorCandidates.emplace_back(static_cast<std::int32_t>(sectorIndex), bestD2);
                                }
                            }

                            std::sort(rankedEdgeSectorCandidates.begin(), rankedEdgeSectorCandidates.end(), [](const auto& a, const auto& b)
                            {
                                    if (a.second != b.second)
                                    {
                                        return a.second < b.second;
                                    }
                                    return a.first < b.first;
                            });

                            edgeSectorCandidates.reserve(rankedEdgeSectorCandidates.size());
                            for (const auto& entry : rankedEdgeSectorCandidates)
                            {
                                edgeSectorCandidates.push_back(entry.first);
                            }
                        }

                        const bool preferFrontFace = !allowThroughSurfacePick
                            && !collectThroughSurfaceOcclusion
                            && !surfaceHits.empty();
                        if (!preferFrontFace)
                        {
                            for (const std::int32_t sector : edgeSectorCandidates)
                            {
                                if (seenSectors.insert(sector).second)
                                {
                                    sectorCandidates.push_back(sector);
                                }
                            }
                        }

                        for (const sed::level::SurfacePickHit& hit : surfaceHits)
                        {
                            if (seenSectors.insert(hit.surface.sector).second)
                            {
                                sectorCandidates.push_back(hit.surface.sector);
                            }
                        }

                        if (!sectorCandidates.empty())
                        {
                            std::size_t pickIndex = 0;
                            if (cyclePick)
                            {
                                const std::int32_t current = currentSector();
                                if (const auto it = std::find(sectorCandidates.begin(), sectorCandidates.end(), current); it != sectorCandidates.end())
                                {
                                    pickIndex = (static_cast<std::size_t>(it - sectorCandidates.begin()) + 1) % sectorCandidates.size();
                                }
                            }
                            else if (render::picking::shouldSkipSelectedFrontCandidate(
                                         xraySelectionCyclingEnabled_,
                                         xrayCycleSelectionClick,
                                         sectorIsSelected(sectorCandidates.front())))
                            {
                                pickIndex = 1u % sectorCandidates.size();
                            }

                            newSector = sectorCandidates[pickIndex];
                            newSurface = -1;
                            newThing = -1;
                            newLight = -1;
                        }
                    }
                    else if (mode == PickMode::Surface)
                    {
                        if (!surfaceHits.empty())
                        {
                            const bool edgePick = (surfaceSubMode_ == SurfaceSubMode::Edge);
                            const bool vertexPick = (surfaceSubMode_ == SurfaceSubMode::Vertex);

                            std::size_t pickIndex = 0;
                            if (cyclePick)
                            {
                                const SurfaceRef current = currentSurface();
                                for (std::size_t i = 0; i < surfaceHits.size(); ++i)
                                {
                                    const SurfaceRef& ref = surfaceHits[i].surface;
                                    if (ref.sector == current.sector && ref.surface == current.surface)
                                    {
                                        pickIndex = (i + 1) % surfaceHits.size();
                                        break;
                                    }
                                }
                            }
                            else if (render::picking::shouldSkipSelectedFrontCandidate(
                                         xraySelectionCyclingEnabled_,
                                         xrayCycleSelectionClick,
                                         surfaceIsSelected(surfaceHits.front().surface)))
                            {
                                pickIndex = 1u % surfaceHits.size();
                            }

                            auto projectToScreen = [&](const TVector& wp, QPointF& outPos) -> bool
                            {
                                const QVector4D clip = mvp * QVector4D(
                                    static_cast<float>(wp.x),
                                    static_cast<float>(wp.y),
                                    static_cast<float>(wp.z),
                                    1.0f);
                                if (qFuzzyIsNull(clip.w()) || clip.w() <= 0.0f)
                                {
                                    return false;
                                }

                                const QVector3D ndc(clip.x() / clip.w(), clip.y() / clip.w(), clip.z() / clip.w());
                                const float sx = (ndc.x() * 0.5f + 0.5f) * static_cast<float>(width());
                                const float sy = (1.0f - (ndc.y() * 0.5f + 0.5f)) * static_cast<float>(height());
                                outPos = QPointF(sx, sy);
                                return true;
                            };

                            constexpr float kPickTolPx = render::picking::
                                kSurfaceSubElementPickRadiusPx;
                            const float tol2 = kPickTolPx * kPickTolPx;
                            const QPointF mp(releasePos);

                            auto matchHit = [&](const sed::level::SurfacePickHit& inHit, float& bestDist2, sed::level::SurfacePickHit& outHit) -> bool
                            {
                                if (!level_)
                                {
                                    return false;
                                }
                                if (inHit.surface.sector < 0 || inHit.surface.surface < 0)
                                {
                                    return false;
                                }
                                if (static_cast<std::size_t>(inHit.surface.sector) >= level_->sectors.size())
                                {
                                    return false;
                                }

                                const Sector& sector = level_->sectors[static_cast<std::size_t>(inHit.surface.sector)];
                                if (static_cast<std::size_t>(inHit.surface.surface) >= sector.surfaces.size())
                                {
                                    return false;
                                }

                                const Surface& surface = sector.surfaces[static_cast<std::size_t>(inHit.surface.surface)];
                                const auto& vxs = surface.vertices;
                                if (vxs.empty())
                                {
                                    return false;
                                }

                                bestDist2 = std::numeric_limits<float>::infinity();
                                TVector hitWorld{};

                                if (vertexPick)
                                {
                                    std::int32_t bestVi = -1;
                                    for (const std::int32_t vi : vxs)
                                    {
                                        if (vi < 0 || static_cast<std::size_t>(vi) >= sector.vertices.size())
                                        {
                                            continue;
                                        }

                                        const Vertex& v = sector.vertices[static_cast<std::size_t>(vi)];
                                        const TVector wp{ v.x, v.y, v.z };
                                        QPointF sp{};
                                        if (!projectToScreen(wp, sp))
                                        {
                                            continue;
                                        }

                                        const QPointF d = mp - sp;
                                        const float d2 = static_cast<float>((d.x() * d.x()) + (d.y() * d.y()));
                                        if (d2 < bestDist2)
                                        {
                                            bestDist2 = d2;
                                            bestVi = vi;
                                            hitWorld = wp;
                                        }
                                    }

                                    if (bestVi < 0 || bestDist2 > tol2)
                                    {
                                        return false;
                                    }
                                }
                                else if (edgePick)
                                {
                                    if (vxs.size() < 2)
                                    {
                                        return false;
                                    }

                                    for (std::size_t i = 0; i < vxs.size(); ++i)
                                    {
                                        const std::int32_t aidx = vxs[i];
                                        const std::int32_t bidx = vxs[(i + 1) % vxs.size()];
                                        if (aidx < 0 || bidx < 0)
                                        {
                                            continue;
                                        }
                                        if (static_cast<std::size_t>(aidx) >= sector.vertices.size()
                                            || static_cast<std::size_t>(bidx) >= sector.vertices.size())
                                        {
                                            continue;
                                        }

                                        const Vertex& va = sector.vertices[static_cast<std::size_t>(aidx)];
                                        const Vertex& vb = sector.vertices[static_cast<std::size_t>(bidx)];
                                        const TVector wa{ va.x, va.y, va.z };
                                        const TVector wb{ vb.x, vb.y, vb.z };

                                        QPointF sa{};
                                        QPointF sb{};
                                        if (!projectToScreen(wa, sa) || !projectToScreen(wb, sb))
                                        {
                                            continue;
                                        }

                                        const float d2 = distanceSqPointToSegment(mp, sa, sb);
                                        if (d2 < bestDist2)
                                        {
                                            bestDist2 = d2;

                                            const QPointF ab = sb - sa;
                                            const double ab2 = (ab.x() * ab.x()) + (ab.y() * ab.y());
                                            double t = 0.0;
                                            if (ab2 > 1e-9)
                                            {
                                                const QPointF ap = mp - sa;
                                                t = ((ap.x() * ab.x()) + (ap.y() * ab.y())) / ab2;
                                            }
                                            t = std::clamp(t, 0.0, 1.0);

                                            hitWorld.x = wa.x + (wb.x - wa.x) * t;
                                            hitWorld.y = wa.y + (wb.y - wa.y) * t;
                                            hitWorld.z = wa.z + (wb.z - wa.z) * t;
                                        }
                                    }

                                    if (!std::isfinite(bestDist2) || bestDist2 > tol2)
                                    {
                                        return false;
                                    }
                                }
                                else
                                {
                                    return false;
                                }

                                outHit = inHit;
                                outHit.position = hitWorld;
                                outHit.distance = (outHit.position - origin).Length();
                                return true;
                            };

                            auto faceHitNearAnyEdgeOrVertex = [&](const sed::level::SurfacePickHit& inHit) -> bool
                            {
                                if (!level_)
                                {
                                    return false;
                                }
                                if (inHit.surface.sector < 0 || inHit.surface.surface < 0)
                                {
                                    return false;
                                }
                                if (static_cast<std::size_t>(inHit.surface.sector) >= level_->sectors.size())
                                {
                                    return false;
                                }

                                const Sector& sector = level_->sectors[static_cast<std::size_t>(inHit.surface.sector)];
                                if (static_cast<std::size_t>(inHit.surface.surface) >= sector.surfaces.size())
                                {
                                    return false;
                                }

                                const Surface& surface = sector.surfaces[static_cast<std::size_t>(inHit.surface.surface)];
                                const auto& vxs = surface.vertices;
                                if (vxs.empty())
                                {
                                    return false;
                                }

                                float bestDist2 = std::numeric_limits<float>::infinity();
                                if (vxs.size() >= 2)
                                {
                                    for (std::size_t i = 0; i < vxs.size(); ++i)
                                    {
                                        const std::int32_t aidx = vxs[i];
                                        const std::int32_t bidx = vxs[(i + 1) % vxs.size()];
                                        if (aidx < 0 || bidx < 0)
                                        {
                                            continue;
                                        }
                                        if (static_cast<std::size_t>(aidx) >= sector.vertices.size()
                                            || static_cast<std::size_t>(bidx) >= sector.vertices.size())
                                        {
                                            continue;
                                        }

                                        const Vertex& va = sector.vertices[static_cast<std::size_t>(aidx)];
                                        const Vertex& vb = sector.vertices[static_cast<std::size_t>(bidx)];
                                        const TVector wa{ va.x, va.y, va.z };
                                        const TVector wb{ vb.x, vb.y, vb.z };

                                        QPointF sa{};
                                        QPointF sb{};
                                        if (!projectToScreen(wa, sa) || !projectToScreen(wb, sb))
                                        {
                                            continue;
                                        }

                                        bestDist2 = std::min(bestDist2, distanceSqPointToSegment(mp, sa, sb));
                                    }
                                }

                                for (const std::int32_t vidx : vxs)
                                {
                                    if (vidx < 0 || static_cast<std::size_t>(vidx) >= sector.vertices.size())
                                    {
                                        continue;
                                    }

                                    const Vertex& vv = sector.vertices[static_cast<std::size_t>(vidx)];
                                    QPointF sp{};
                                    if (!projectToScreen(TVector{ vv.x, vv.y, vv.z }, sp))
                                    {
                                        continue;
                                    }

                                    const QPointF d = mp - sp;
                                    bestDist2 = std::min(bestDist2, static_cast<float>((d.x() * d.x()) + (d.y() * d.y())));
                                }

                                return std::isfinite(bestDist2) && bestDist2 <= tol2;
                            };

                            if (edgePick || vertexPick)
                            {
                                bool picked = false;
                                sed::level::SurfacePickHit best{};

                                for (std::size_t ofs = 0; ofs < surfaceHits.size(); ++ofs)
                                {
                                    const sed::level::SurfacePickHit& h = surfaceHits[(pickIndex + ofs) % surfaceHits.size()];
                                    float d2 = std::numeric_limits<float>::infinity();
                                    sed::level::SurfacePickHit hit{};
                                    if (matchHit(h, d2, hit))
                                    {
                                        best = hit;
                                        picked = true;
                                        break; // hits are sorted; take first that matches the sub-mode geometry
                                    }
                                }

                                if (picked)
                                {
                                    newSector = best.surface.sector;
                                    newSurface = best.surface.surface;
                                    newThing = -1;
                                    newLight = -1;
                                    surfaceHit = best;
                                }
                                else
                                {
                                    const SurfaceRef cur = currentSurface();
                                    newSector = cur.sector;
                                    newSurface = cur.surface;
                                    newThing = -1;
                                    newLight = -1;
                                    surfaceHit.reset();
                                }
                            }
                            else
                            {
                                if (wireXraySurfacePick)
                                {
                                    bool picked = false;
                                    sed::level::SurfacePickHit best{};

                                    for (std::size_t ofs = 0; ofs < surfaceHits.size(); ++ofs)
                                    {
                                        const sed::level::SurfacePickHit& h = surfaceHits[(pickIndex + ofs) % surfaceHits.size()];
                                        if (!faceHitNearAnyEdgeOrVertex(h))
                                        {
                                            continue;
                                        }

                                        best = h;
                                        picked = true;
                                        break;
                                    }

                                    if (picked)
                                    {
                                        newSector = best.surface.sector;
                                        newSurface = best.surface.surface;
                                        newThing = -1;
                                        newLight = -1;
                                        surfaceHit = best;
                                    }
                                    else
                                    {
                                        const SurfaceRef cur = currentSurface();
                                        newSector = cur.sector;
                                        newSurface = cur.surface;
                                        newThing = -1;
                                        newLight = -1;
                                        surfaceHit.reset();
                                    }
                                }
                                else
                                {
                                    newSector = surfaceHits[pickIndex].surface.sector;
                                    newSurface = surfaceHits[pickIndex].surface.surface;
                                    newThing = -1;
                                    newLight = -1;
                                    surfaceHit = surfaceHits[pickIndex];
                                }
                            }
                        }
                    }
                    else if (mode == PickMode::Thing)
                    {
                        if (!thingHits.empty())
                        {
                            std::optional<std::size_t> currentIndex{};
                            if (cyclePick)
                            {
                                const std::int32_t current = currentThing();
                                if (const auto it = std::find_if(thingHits.begin(), thingHits.end(), [&](const IndexHit& hit)
                                    {
                                        return hit.index == current;
                                    });
                                    it != thingHits.end())
                                {
                                    currentIndex = static_cast<std::size_t>(it - thingHits.begin());
                                }
                            }
                            else if (render::picking::shouldSkipSelectedFrontCandidate(
                                         xraySelectionCyclingEnabled_,
                                         xrayCycleSelectionClick,
                                         thingIsSelected(thingHits.front().index)))
                            {
                                currentIndex = 0u;
                            }
                            const bool advancePick = cyclePick || currentIndex.has_value();
                            const std::size_t pickIndex = render::picking::nextFrontToBackCandidateIndex(
                                thingHits.size(),
                                currentIndex,
                                advancePick);

                            newSector = -1;
                            newSurface = -1;
                            newThing = thingHits[pickIndex].index;
                            newLight = -1;
                        }
                    }
                    else if (mode == PickMode::Light)
                    {
                        if (!lightHits.empty())
                        {
                            std::size_t pickIndex = 0;
                            if (cyclePick)
                            {
                                const std::int32_t current = currentLight();
                                if (const auto it = std::find_if(lightHits.begin(), lightHits.end(), [&](const IndexHit& hit)
                                    {
                                        return hit.index == current;
                                    });
                                    it != lightHits.end())
                                {
                                    pickIndex = (static_cast<std::size_t>(it - lightHits.begin()) + 1) % lightHits.size();
                                }
                            }
                            else if (render::picking::shouldSkipSelectedFrontCandidate(
                                         xraySelectionCyclingEnabled_,
                                         xrayCycleSelectionClick,
                                         lightIsSelected(lightHits.front().index)))
                            {
                                pickIndex = 1u % lightHits.size();
                            }

                            newSector = -1;
                            newSurface = -1;
                            newThing = -1;
                            newLight = lightHits[pickIndex].index;
                        }
                    }
                    else
                    {
                        if (previewAutoSelection)
                        {
                            using render::picking::PreviewAutoPickCandidate;
                            using render::picking::PreviewAutoPickKind;

                            std::vector<PreviewAutoPickCandidate> candidates{};
                            candidates.reserve(
                                thingHits.size() + lightHits.size()
                                + surfaceHits.size() * 2u);
                            for (const IndexHit& candidate : thingHits)
                            {
                                candidates.push_back(PreviewAutoPickCandidate{
                                    PreviewAutoPickKind::Thing,
                                    candidate.index, -1, candidate.distance });
                            }
                            for (const IndexHit& candidate : lightHits)
                            {
                                candidates.push_back(PreviewAutoPickCandidate{
                                    PreviewAutoPickKind::BakedLight,
                                    candidate.index, -1, candidate.distance });
                            }
                            for (const sed::level::SurfacePickHit& candidate : surfaceHits)
                            {
                                candidates.push_back(PreviewAutoPickCandidate{
                                    PreviewAutoPickKind::Surface,
                                    candidate.surface.sector,
                                    candidate.surface.surface,
                                    candidate.distance });
                                candidates.push_back(PreviewAutoPickCandidate{
                                    PreviewAutoPickKind::Sector,
                                    candidate.surface.sector,
                                    -1,
                                    candidate.distance });
                            }
                            candidates = render::picking::orderedPreviewAutoPickCandidates(
                                std::move(candidates),
                                surfaceHits.empty()
                                    ? std::numeric_limits<double>::infinity()
                                    : surfaceHits.front().distance,
                                xraySelectionCyclingEnabled_ && cyclePick,
                                1.0e-6);

                            std::optional<PreviewAutoPickCandidate> current{};
                            if (selectedThing_ >= 0)
                            {
                                current = PreviewAutoPickCandidate{
                                    PreviewAutoPickKind::Thing,
                                    selectedThing_, -1, 0.0 };
                            }
                            else if (selectedLight_ >= 0)
                            {
                                current = PreviewAutoPickCandidate{
                                    PreviewAutoPickKind::BakedLight,
                                    selectedLight_, -1, 0.0 };
                            }
                            else if (selectedSurfaceSector_ >= 0
                                && selectedSurface_ >= 0)
                            {
                                current = PreviewAutoPickCandidate{
                                    PreviewAutoPickKind::Surface,
                                    selectedSurfaceSector_, selectedSurface_, 0.0 };
                            }
                            else if (selectedSector_ >= 0)
                            {
                                current = PreviewAutoPickCandidate{
                                    PreviewAutoPickKind::Sector,
                                    selectedSector_, -1, 0.0 };
                            }

                            std::size_t pickIndex = 0u;
                            if (cyclePick && current)
                            {
                                const auto it = std::find(
                                    candidates.begin(), candidates.end(), *current);
                                if (it != candidates.end())
                                {
                                    pickIndex = (static_cast<std::size_t>(
                                        std::distance(candidates.begin(), it)) + 1u)
                                        % candidates.size();
                                }
                            }

                            if (!candidates.empty())
                            {
                                const PreviewAutoPickCandidate& picked =
                                    candidates[pickIndex];
                                newSector = -1;
                                newSurface = -1;
                                newThing = -1;
                                newLight = -1;
                                switch (picked.kind)
                                {
                                    case PreviewAutoPickKind::Thing:
                                        newThing = picked.primaryIndex;
                                        break;
                                    case PreviewAutoPickKind::BakedLight:
                                        newLight = picked.primaryIndex;
                                        break;
                                    case PreviewAutoPickKind::Surface:
                                    {
                                        newSector = picked.primaryIndex;
                                        newSurface = picked.secondaryIndex;
                                        const auto it = std::find_if(
                                            surfaceHits.begin(),
                                            surfaceHits.end(),
                                            [&](const sed::level::SurfacePickHit& hit)
                                            {
                                                return hit.surface.sector == newSector
                                                    && hit.surface.surface == newSurface;
                                            });
                                        if (it != surfaceHits.end())
                                        {
                                            surfaceHit = *it;
                                        }
                                        break;
                                    }
                                    case PreviewAutoPickKind::Sector:
                                        newSector = picked.primaryIndex;
                                        surfaceHit.reset();
                                        break;
                                }
                            }
                        }
                        else
                        {
                        const double bestT = std::min({ surfaceT, bestThingT, bestLightT });
                        if (bestT == surfaceT && surfaceHit)
                        {
                            const bool skipFront = render::picking::shouldSkipSelectedFrontCandidate(
                                xraySelectionCyclingEnabled_,
                                xrayCycleSelectionClick,
                                surfaceIsSelected(surfaceHit->surface));
                            if (skipFront && surfaceHits.size() > 1u)
                            {
                                surfaceHit = surfaceHits[1];
                            }
                            newSector = surfaceHit->surface.sector;
                            newSurface = surfaceHit->surface.surface;
                            newThing = -1;
                            newLight = -1;
                        }
                        else if (bestT == bestThingT && newThing >= 0)
                        {
                            const bool skipFront = render::picking::shouldSkipSelectedFrontCandidate(
                                xraySelectionCyclingEnabled_,
                                xrayCycleSelectionClick,
                                thingIsSelected(thingHits.front().index));
                            if (skipFront && thingHits.size() > 1u)
                            {
                                newThing = thingHits[1].index;
                            }
                            newSector = -1;
                            newSurface = -1;
                            newLight = -1;
                        }
                        else if (bestT == bestLightT && newLight >= 0)
                        {
                            const bool skipFront = render::picking::shouldSkipSelectedFrontCandidate(
                                xraySelectionCyclingEnabled_,
                                xrayCycleSelectionClick,
                                lightIsSelected(lightHits.front().index));
                            if (skipFront && lightHits.size() > 1u)
                            {
                                newLight = lightHits[1].index;
                            }
                            newSector = -1;
                            newSurface = -1;
                            newThing = -1;
                        }
                        }
                    }
                    }
                }
            }
        }

        const std::int32_t currentSector = (selectedSector_ >= 0) ? selectedSector_ : selectedSurfaceSector_;
        const std::int32_t currentSurface = (selectedSurface_ >= 0) ? selectedSurface_ : -1;

        const bool selectionChanged =
            (newSector != currentSector || newSurface != currentSurface || newThing != selectedThing_ || newLight != selectedLight_);

        const bool pickedSurface = (newSector >= 0 && newSurface >= 0);
        const bool surfaceHitMatchesPick =
            pickedSurface
            && surfaceHit
            && surfaceHit->surface.sector == newSector
            && surfaceHit->surface.surface == newSurface;

        if (surfaceHitMatchesPick && !previewAutoSelection)
        {
            haveLastSurfaceHit_ = true;
            lastSurfaceHitSurface_ = surfaceHit->surface;
            lastSurfaceHitPos_ = surfaceHit->position;
        }

        const bool surfaceSubModeWantsHit = !previewAutoSelection
            && (surfaceSubMode_ == SurfaceSubMode::Edge
                || surfaceSubMode_ == SurfaceSubMode::Vertex);
        bool surfaceSubHitCleared = false;
        if (pickedSurface && surfaceSubModeWantsHit && !surfaceHitMatchesPick && haveLastSurfaceHit_)
        {
            haveLastSurfaceHit_ = false;
            lastSurfaceHitSurface_ = {};
            lastSurfaceHitPos_ = {};
            surfaceSubHitCleared = true;
        }

        const bool needSurfaceSubModeRefresh = surfaceSubModeWantsHit && (surfaceHitMatchesPick || surfaceSubHitCleared);

        // Report the object under every placement click as well as normal
        // selection clicks.  Placement tools intentionally suppress selection,
        // but still need the freshly picked Thing/sector/light rather than a
        // stale value from the last ordinary selection click.
        Q_EMIT clickPicked(newSector, newSurface, newThing, newLight);

        if (surfaceClickSelectionEnabled_)
        {
            const Qt::KeyboardModifiers clickModifiers = event->modifiers();
            const bool modifierSelectionRequested =
                ui::shortcuts::selectionModifierRequestsClickDispatch(
                    clickModifiers,
                    selectionAddModifiers_,
                    selectionRemoveModifiers_);
            if (selectionChanged || needSurfaceSubModeRefresh || modifierSelectionRequested)
            {
                setSelection(newSector, newSurface, newThing, newLight);

                if (!previewAutoSelection
                    && surfaceSubMode_ == SurfaceSubMode::Edge)
                {
                    const std::optional<SurfaceEdgeSelectionRef> pickedEdge =
                        surfaceHitMatchesPick
                        ? surfaceEdgeAtHit(surfaceHit->surface, surfaceHit->position)
                        : std::nullopt;
                    applySurfaceEdgePointSelection(pickedEdge, clickModifiers);
                }

                if (selectionChanged || modifierSelectionRequested)
                {
                    const std::int32_t outSector = (selectedSector_ >= 0) ? selectedSector_ : selectedSurfaceSector_;
                    const std::int32_t outSurface = (selectedSurface_ >= 0) ? selectedSurface_ : -1;
                    Q_EMIT selectionPicked(
                        outSector,
                        outSurface,
                        selectedThing_,
                        selectedLight_,
                        clickModifiers);
                }
            }
        }

        if (surfaceClickPreferSurfaceHit_)
        {
            if (surfaceHit)
            {
                const QVector3D hitPos(
                    static_cast<float>(surfaceHit->position.x),
                    static_cast<float>(surfaceHit->position.y),
                    static_cast<float>(surfaceHit->position.z));
                Q_EMIT surfaceClicked(hitPos, surfaceHit->surface.sector, surfaceHit->surface.surface, releasePos, event->modifiers());
            }
            else if (clickedWorldPos)
            {
                Q_EMIT surfaceClicked(*clickedWorldPos, -1, -1, releasePos, event->modifiers());
            }
        }
        else
        {
            if (clickedWorldPos)
            {
                Q_EMIT surfaceClicked(*clickedWorldPos, -1, -1, releasePos, event->modifiers());
            }
            else if (surfaceHit)
            {
                const QVector3D hitPos(
                    static_cast<float>(surfaceHit->position.x),
                    static_cast<float>(surfaceHit->position.y),
                    static_cast<float>(surfaceHit->position.z));
                Q_EMIT surfaceClicked(hitPos, surfaceHit->surface.sector, surfaceHit->surface.surface, releasePos, event->modifiers());
            }
        }

        event->accept();
        return;
    }

    QOpenGLWidget::mouseReleaseEvent(event);
}

void GlView::mouseDoubleClickEvent(QMouseEvent* event)
{
    const ViewportMouseAction action = viewportMouseActionForPress(
        effectiveViewportNavigationSettings(), event->button(), event->modifiers());
    if (action != ViewportMouseAction::pick
        || pickMode_ != PickMode::ArchitecturalLighting
        || renderPipeline_ != RenderPipeline::Editor
        || !architecturalLightingEditingActive_ || !level_)
    {
        mousePressEvent(event);
        return;
    }
    event->accept();
    (void)finishStagedCleaveWheelSession();
    if (viewportMouseGesture_.button != Qt::NoButton)
    {
        return;
    }
    viewportMouseGesture_ = { event->button(), action,
        effectiveViewportNavigationSettings().mouse[static_cast<std::size_t>(action)].modifiers };
    QMouseEvent routedEvent(event->type(), event->position(), event->globalPosition(),
        Qt::LeftButton, Qt::LeftButton, viewportMouseSemanticModifiers(event->modifiers()),
        event->pointingDevice());
    event = &routedEvent;

    if (event->button() != Qt::LeftButton
        || pickMode_ != PickMode::ArchitecturalLighting
        || renderPipeline_ != RenderPipeline::Editor
        || !architecturalLightingEditingActive_
        || !level_)
    {
        QOpenGLWidget::mouseDoubleClickEvent(event);
        return;
    }

    QMatrix4x4 projection;
    QMatrix4x4 cameraView;
    QVector3D eye{};
    QVector3D target{};
    float nearPlane{};
    float farPlane{};
    buildCameraMatrices(
        projection,
        cameraView,
        eye,
        target,
        nearPlane,
        farPlane);
    (void)eye;
    (void)target;
    (void)nearPlane;
    (void)farPlane;

    struct ContextGuard
    {
        GlView& view;
        QPointer<QOpenGLContext> previousContext{};
        bool acquired{};

        explicit ContextGuard(GlView& v)
            : view(v),
              previousContext(QOpenGLContext::currentContext())
        {
            if (previousContext.data() != view.context())
            {
                view.makeCurrent();
                acquired = true;
            }
        }

        ~ContextGuard() { release(); }

        void release() noexcept
        {
            if (!acquired)
            {
                return;
            }
            acquired = false;
            view.doneCurrent();
            if (previousContext)
            {
                if (QSurface* surface = previousContext->surface())
                {
                    previousContext->makeCurrent(surface);
                }
            }
        }

        [[nodiscard]] bool ready() const noexcept
        {
            return view.context() != nullptr
                && QOpenGLContext::currentContext() == view.context();
        }
    } contextGuard(*this);

    std::optional<ArchitecturalLightingRenderVertex> hit{};
    std::optional<ArchitecturalLightingVertexIdentity> ownerVertex{};
    std::vector<ArchitecturalLightingVertexIdentity> ownerVerticesForEdit{};
    if (contextGuard.ready())
    {
        // Exact vertex picking and its scene-depth readback require the
        // owning GL context.  The remembered owner-marker handoff below is
        // CPU-only, so it must remain available when a double-click arrives
        // while another viewport owns the current context.
        hit = architecturalLightingVertexAt(
            mouseEventPos(*event),
            projection * cameraView,
            xraySelectionCyclingEnabled_);
    }

    if (!hit
        && architecturalLightingLastOwnerClick_
        && (mouseEventPos(*event)
                - architecturalLightingLastOwnerClickPosition_)
                .manhattanLength()
            <= QGuiApplication::styleHints()->mouseDoubleClickDistance())
    {
        const std::int32_t ownerIndex = *architecturalLightingLastOwnerClick_;
        if (ownerIndex >= 0
            && static_cast<std::size_t>(ownerIndex) < level_->things.size())
        {
            const Thing& owner = level_->things[
                static_cast<std::size_t>(ownerIndex)];
            const bool activeOwner =
                level::isMotsArchThing(*level_, owner, templates_)
                && (owner.archLightingIndex == -1
                    || (owner.archLightingIndex >= 0
                        && static_cast<std::size_t>(owner.archLightingIndex)
                            < level_->motsArchLighting.size()));
            const bool selected = highlightedSelection_.things.contains(
                ownerIndex)
                || selectedThing_ == ownerIndex
                || architecturalLightingSelectedOwnerCacheByThing_.contains(
                    ownerIndex);
            if (activeOwner && selected && isLayerVisible(owner.layer)
                && !isLayerLocked(owner.layer))
            {
                const auto appendOwnerVertices =
                    [this, &ownerVerticesForEdit](
                        const std::int32_t candidateOwner)
                {
                    if (!level_ || candidateOwner < 0
                        || static_cast<std::size_t>(candidateOwner)
                            >= level_->things.size())
                    {
                        return;
                    }
                    const Thing& candidate = level_->things[
                        static_cast<std::size_t>(candidateOwner)];
                    const bool candidateActive =
                        level::isMotsArchThing(
                            *level_, candidate, templates_)
                        && (candidate.archLightingIndex == -1
                            || (candidate.archLightingIndex >= 0
                                && static_cast<std::size_t>(
                                    candidate.archLightingIndex)
                                    < level_->motsArchLighting.size()));
                    const bool candidateSelected =
                        highlightedSelection_.things.contains(candidateOwner)
                        || selectedThing_ == candidateOwner
                        || architecturalLightingSelectedOwnerCacheByThing_
                            .contains(candidateOwner);
                    if (!candidateActive || !candidateSelected
                        || !isLayerVisible(candidate.layer)
                        || isLayerLocked(candidate.layer))
                    {
                        return;
                    }
                    const std::span<const ArchitecturalLightingRenderVertex>
                        vertices = architecturalLightingVerticesForThing(
                            candidateOwner);
                    for (const ArchitecturalLightingRenderVertex& vertex
                         : vertices)
                    {
                        ownerVerticesForEdit.push_back(
                            architecturalLightingVertexIdentity(vertex));
                    }
                };

                // The published owner cache is the same selected-owner set
                // used by the marker and vertex pick paths. Expand every
                // selected effective ARCH owner here so an owner-icon double-click
                // opens the batch editor for the whole selected set.
                for (const ArchitecturalLightingRenderCache& cache
                     : architecturalLightingSelectedOwnerCaches_)
                {
                    appendOwnerVertices(cache.thingIndex);
                }
                // Keep the clicked owner eligible if a callback rebuilt the
                // cache table between its release and the double-click.
                appendOwnerVertices(ownerIndex);
                std::sort(
                    ownerVerticesForEdit.begin(),
                    ownerVerticesForEdit.end(),
                    [](const ArchitecturalLightingVertexIdentity& lhs,
                       const ArchitecturalLightingVertexIdentity& rhs)
                    {
                        if (lhs.thingIndex != rhs.thingIndex)
                        {
                            return lhs.thingIndex < rhs.thingIndex;
                        }
                        if (lhs.lodIndex != rhs.lodIndex)
                        {
                            return lhs.lodIndex < rhs.lodIndex;
                        }
                        if (lhs.meshIndex != rhs.meshIndex)
                        {
                            return lhs.meshIndex < rhs.meshIndex;
                        }
                        return lhs.vertexIndex < rhs.vertexIndex;
                    });
                ownerVerticesForEdit.erase(
                    std::unique(
                        ownerVerticesForEdit.begin(),
                        ownerVerticesForEdit.end()),
                    ownerVerticesForEdit.end());
                if (!ownerVerticesForEdit.empty())
                {
                    ownerVertex = ownerVerticesForEdit.front();
                }
            }
        }
    }
    contextGuard.release();
    if (!hit && !ownerVertex)
    {
        QOpenGLWidget::mouseDoubleClickEvent(event);
        return;
    }

    if (!hit && !ownerVerticesForEdit.empty())
    {
        setArchitecturalLightingSelectedVertices(ownerVerticesForEdit);
        // This is an intentional owner expansion, so a subsequent plain
        // vertex click can narrow it before any new explicit batch exists.
        architecturalLightingSelectedVerticesImplicitOwnerExpansion_ = true;
    }

    suppressNextArchitecturalLightingVertexRelease_ = true;
    if (!hit)
    {
        hit = ArchitecturalLightingRenderVertex{};
        hit->thingIndex = ownerVertex->thingIndex;
        hit->lodIndex = ownerVertex->lodIndex;
        hit->meshIndex = ownerVertex->meshIndex;
        hit->vertexIndex = ownerVertex->vertexIndex;
    }
    Q_EMIT architecturalLightingVertexEditRequested(
        hit->thingIndex,
        hit->lodIndex,
        hit->meshIndex,
        hit->vertexIndex,
        event->modifiers());
    architecturalLightingLastOwnerClick_.reset();
    event->accept();
}

bool GlView::eventFilter(QObject* watched, QEvent* event)
{
    if ( stagedCleaveOrderEditor_
        && watched == stagedCleaveOrderEditor_.data() )
    {
        if ( event->type() == QEvent::KeyPress )
        {
            auto* const keyEvent = static_cast<QKeyEvent*>(event);
            if ( keyEvent->key() == Qt::Key_Escape )
            {
                finishStagedCleaveOrderEdit(false, true);
                keyEvent->accept();
                return true;
            }
            if ( keyEvent->key() == Qt::Key_Return
                || keyEvent->key() == Qt::Key_Enter )
            {
                finishStagedCleaveOrderEdit(true, true);
                keyEvent->accept();
                return true;
            }
        }
        else if ( event->type() == QEvent::FocusOut )
        {
            // Commit after Qt has selected the new focus owner. The editor is
            // hidden only after its QPointer and edit baseline are detached.
            finishStagedCleaveOrderEdit(true, false);
        }
    }
    return QOpenGLWidget::eventFilter(watched, event);
}

bool GlView::prepareStagedCleaveGroupTransform()
{
#ifdef SED_APP_MAIN_WINDOW_TEST_ACCESS
    ++stagedCleaveTransformPreparationCountForTest_;
#endif
    if ( stagedCleaveTransformMode_ == StagedCleaveTransformMode::Move
        || !canTransformSelectedStagedCleave(stagedCleaveTransformMode_) )
    {
        return false;
    }
    const std::vector<StagedCleaveLine>& baselineLines =
        stagedCleaveGestureBaselineLines();
    if ( baselineLines.size() != stagedCleaveLines_.size()
        || baselineLines.empty() )
    {
        return false;
    }

    using HandleKey = std::tuple<double, double, double>;
    const auto keyFor = [](const TVector& point)
    {
        return HandleKey{point.x, point.y, point.z};
    };
    const std::size_t lineCount = stagedCleaveLines_.size();
    const std::size_t selectedCount =
        selectedStagedCleaveLines_.size();
    const std::size_t workBytes =
        selectedCount * (2u * sizeof(std::int32_t)
            + sizeof(HandleKey) + sizeof(bool) + 64u)
        + lineCount * 2u * sizeof(StagedCleaveLineMove)
        + lineCount * 2u * sizeof(StagedCleaveLine);
    const std::size_t currentBytes = stagedCleaveHistoryStorageBytes();
    if ( currentBytes > kStagedCleaveHistoryMaxBytes
        || workBytes > kStagedCleaveHistoryMaxBytes - currentBytes )
    {
        showToast(
            "Multi-cut: local history limit reached; this selection is too large to transform",
            3200,
            ToastPlacement::BottomCenter,
            ToastGroup::Error);
        return false;
    }

    std::vector<std::int32_t> movingLines =
        selectedStagedCleaveLines_;
    std::sort(movingLines.begin(), movingLines.end());
    std::map<HandleKey, bool> movingHandles{};
    TVector pivot{};
    for ( const std::int32_t selectedLine : movingLines )
    {
        if ( selectedLine < 0
            || static_cast<std::size_t>(selectedLine)
                >= baselineLines.size() )
        {
            return false;
        }
        const StagedCleaveLine& selected = baselineLines[
            static_cast<std::size_t>(selectedLine)];
        movingHandles.emplace(keyFor(selected.first), true);
        movingHandles.emplace(keyFor(selected.second), true);
    }
    if ( movingHandles.empty() )
    {
        return false;
    }
    for ( const auto& [key, unused] : movingHandles )
    {
        (void)unused;
        pivot = pivot + TVector(
            std::get<0>(key),
            std::get<1>(key),
            std::get<2>(key));
    }
    pivot = pivot * (1.0 / static_cast<double>(movingHandles.size()));
    if ( !render::cleaveFinitePoint(pivot) )
    {
        return false;
    }

    TVector planeNormal = baselineLines[
        static_cast<std::size_t>(movingLines.front())]
        .constructionPlaneNormal;
    if ( !normalizeInPlace(planeNormal) )
    {
        return false;
    }
    std::vector<StagedCleaveLineMove> affectedLines{};
    affectedLines.reserve(lineCount);
    std::vector<StagedCleaveLine> previewLines{};
    previewLines.reserve(lineCount);
    for ( std::size_t current = 0; current < lineCount; ++current )
    {
        const StagedCleaveLine& line = baselineLines[current];
        const bool selected = std::binary_search(
            movingLines.begin(),
            movingLines.end(),
            static_cast<std::int32_t>(current));
        std::uint8_t endpointMask = selected ? 3u : 0u;
        if ( !selected )
        {
            if ( movingHandles.contains(keyFor(line.first)) )
            {
                endpointMask |= 1u;
            }
            if ( movingHandles.contains(keyFor(line.second)) )
            {
                endpointMask |= 2u;
            }
        }
        if ( endpointMask != 0u )
        {
            affectedLines.push_back(StagedCleaveLineMove{
                static_cast<std::int32_t>(current), endpointMask});
            previewLines.push_back(line);
        }
    }

    stagedCleaveTransformDrag_ = {};
    stagedCleaveTransformDrag_.active = true;
    stagedCleaveTransformDrag_.mode = stagedCleaveTransformMode_;
    stagedCleaveTransformDrag_.pivot = pivot;
    stagedCleaveTransformDrag_.planeNormal = planeNormal;
    stagedCleaveTransformDrag_.selectedLines = std::move(movingLines);
    stagedCleaveTransformDrag_.affectedLines = std::move(affectedLines);
    stagedCleaveTransformDrag_.previewLines = std::move(previewLines);
    if ( stagedCleaveHistoryStorageBytes() > kStagedCleaveHistoryMaxBytes )
    {
        stagedCleaveTransformDrag_ = {};
        return false;
    }
    clearStagedCleaveTransformHud();
    return true;
}

bool GlView::updateStagedCleaveGroupTransform(
    const double angleDegrees, const double scale)
{
    const std::vector<StagedCleaveLine>& baselineLines = stagedCleaveGestureBaselineLines();
    if ( !stagedCleaveTransformDrag_.active || !std::isfinite(angleDegrees)
        || !std::isfinite(scale) || scale <= 0.0
        || baselineLines.size() != stagedCleaveLines_.size()
        || stagedCleaveTransformDrag_.previewLines.size()
            != stagedCleaveTransformDrag_.affectedLines.size() )
    {
        return false;
    }
    const double appliedAngle = angleDegrees;
    const double appliedScale = scale;
    const double angleRadians = std::remainder(appliedAngle, 360.0)
        * (3.14159265358979323846 / 180.0);
    const auto transformPoint = [this, angleRadians, appliedScale](
                                    const TVector& point)
    {
        const TVector relative = point - stagedCleaveTransformDrag_.pivot;
        const TVector transformed = stagedCleaveTransformDrag_.mode
                == StagedCleaveTransformMode::Rotate
            ? rotateCleaveVectorAroundAxis(
                relative,
                stagedCleaveTransformDrag_.planeNormal,
                angleRadians)
            : relative * appliedScale;
        return stagedCleaveTransformDrag_.pivot + transformed;
    };
    bool validPreview = true;
    for ( std::size_t affectedIndex = 0;
        affectedIndex < stagedCleaveTransformDrag_.affectedLines.size();
        ++affectedIndex )
    {
        const StagedCleaveLineMove& affected =
            stagedCleaveTransformDrag_.affectedLines[affectedIndex];
        if ( affected.line < 0
            || static_cast<std::size_t>(affected.line) >= baselineLines.size() )
        {
            validPreview = false;
            break;
        }
        const StagedCleaveLine& baseline = baselineLines[
            static_cast<std::size_t>(affected.line)];
        StagedCleaveLine& preview =
            stagedCleaveTransformDrag_.previewLines[affectedIndex];
        preview = baseline;
        if ( (affected.endpointMask & 1u) != 0u )
        {
            preview.first = transformPoint(baseline.first);
        }
        if ( (affected.endpointMask & 2u) != 0u )
        {
            preview.second = transformPoint(baseline.second);
        }
        if ( !render::cleaveFinitePoint(preview.first)
            || !render::cleaveFinitePoint(preview.second)
            || render::cleaveSameHandle(preview.first, preview.second) )
        {
            validPreview = false;
            break;
        }
        const bool selected = std::binary_search(
            stagedCleaveTransformDrag_.selectedLines.begin(),
            stagedCleaveTransformDrag_.selectedLines.end(),
            affected.line);
        if ( selected
            && stagedCleaveTransformDrag_.mode
                == StagedCleaveTransformMode::Rotate )
        {
            preview.planeNormal = rotateCleaveVectorAroundAxis(
                baseline.planeNormal,
                stagedCleaveTransformDrag_.planeNormal,
                angleRadians);
            preview.constructionPlaneNormal = rotateCleaveVectorAroundAxis(
                baseline.constructionPlaneNormal,
                stagedCleaveTransformDrag_.planeNormal,
                angleRadians);
            preview.angleReferenceAxis = rotateCleaveVectorAroundAxis(
                baseline.angleReferenceAxis,
                stagedCleaveTransformDrag_.planeNormal,
                angleRadians);
        }
        refreshStagedCleaveLinePlane(preview);
        if ( !render::cleaveFinitePoint(preview.planeNormal)
            || preview.planeNormal.lengthSqr() == 0.0 )
        {
            validPreview = false;
            break;
        }
    }

    if ( !validPreview )
    {
        return false;
    }
    stagedCleaveTransformHudBannerKind_ =
        stagedCleaveTransformDrag_.mode == StagedCleaveTransformMode::Rotate
        ? ViewportBannerKind::cleaveRotate
        : ViewportBannerKind::cleaveScale;
    bool geometryChanged = false;
    for ( std::size_t affectedIndex = 0;
        affectedIndex < stagedCleaveTransformDrag_.affectedLines.size();
        ++affectedIndex )
    {
        const std::size_t line = static_cast<std::size_t>(
            stagedCleaveTransformDrag_.affectedLines[affectedIndex].line);
        const StagedCleaveLine& preview = stagedCleaveTransformDrag_.previewLines[affectedIndex];
        geometryChanged = geometryChanged
            || !render::cleaveSameHandle(stagedCleaveLines_[line].first, preview.first)
            || !render::cleaveSameHandle(stagedCleaveLines_[line].second, preview.second);
        stagedCleaveLines_[line] = preview;
    }
    if (geometryChanged) ++stagedCleaveJointGuidePreviewRevision_;
    stagedCleaveTransformHudAnchor_ = stagedCleaveTransformDrag_.pivot;
    stagedCleaveTransformDrag_.appliedAngleDegrees = appliedAngle;
    stagedCleaveTransformDrag_.appliedScale = appliedScale;
    stagedCleaveTransformHudText_ =
        stagedCleaveTransformDrag_.mode == StagedCleaveTransformMode::Rotate
        ? QStringLiteral("Rotate %1°").arg(cleaveTransformHudNumber(appliedAngle))
        : QStringLiteral("Scale %1×").arg(cleaveTransformHudNumber(appliedScale));
    startStagedCleaveTransformHudExpiry();
    return true;
}

void GlView::keyPressEvent(QKeyEvent* event)
{
    if (flyNav_.active)
    {
        if (ui::shortcuts::isEditorModifierKey(event->key()) && !event->isAutoRepeat())
        {
            refreshFlyNavigationSpeedModifiers(ui::shortcuts::modifiersWithKeyState(
                event->modifiers(), event->key(), true));
        }
        event->accept();
        return;
    }
    if ( isStagedCleaveInteractionActive() )
    {
        if ( const auto mode = stagedCleaveTransformShortcutForKey(event->key(), event->modifiers()) )
        {
            setStagedCleaveTransformMode(*mode);
            event->accept();
            return;
        }
    }
    if ( isStagedCleaveInteractionActive() && event->key() == Qt::Key_Delete )
    {
        (void)deleteSelectedStagedCleaveLine();
        event->accept();
        return;
    }

    if ( isStagedCleaveInteractionActive() && event->key() == Qt::Key_Escape )
    {
        if ( stagedCleaveHistory_.gestureActive || stagedCleaveWheelSession_.active() )
        {
            (void)cancelActiveStagedCleaveInteraction();
            event->accept();
            return;
        }
        if ( cleaveShapeOptions_.placementArmed
            || stagedCleaveShapePlacementPlan_ )
        {
            RecoveryState::CleaveShapeOptions options = cleaveShapeOptions_;
            options.placementArmed = false;
            stagedCleaveShapePlacementPlan_.reset();
            stagedCleaveShapePreviewCenter_.reset();
            setCleaveShapeOptions(options);
            event->accept();
            return;
        }
    }

    QOpenGLWidget::keyPressEvent(event);
}

void GlView::keyReleaseEvent(QKeyEvent* event)
{
    if (handleStagedCleaveWheelModifierRelease(event->key(), event->isAutoRepeat()))
    {
        event->accept();
        return;
    }
    if (flyNav_.active)
    {
        if (ui::shortcuts::isEditorModifierKey(event->key()) && !event->isAutoRepeat())
        {
            refreshFlyNavigationSpeedModifiers(ui::shortcuts::modifiersWithKeyState(
                event->modifiers(), event->key(), false));
        }
        event->accept();
        return;
    }
    QOpenGLWidget::keyReleaseEvent(event);
}

bool GlView::event(QEvent* event)
{
    if (event->type() == QEvent::ShortcutOverride && flyNav_.active)
    {
        event->accept();
        return true;
    }
    switch (event->type())
    {
        case QEvent::FocusOut:
        case QEvent::UngrabMouse:
        case QEvent::Hide:
        case QEvent::WindowDeactivate:
            cancelViewportMouseGesture();
            break;
        default:
            break;
    }
    return QOpenGLWidget::event(event);
}

void GlView::wheelEvent(QWheelEvent* event)
{
    event->accept();
    const auto& navigation = effectiveViewportNavigationSettings();
    float cameraSteps = static_cast<float>(event->angleDelta().y()) / 120.0f;
    if (flyNav_.active)
    {
        if (!viewportWheelBindingMatches(navigation.flySpeed, event->modifiers())
            || std::abs(cameraSteps) < 0.001f)
        {
            return;
        }
        if (navigation.flySpeed.inverted) cameraSteps = -cameraSteps;
        adjustFlyNavigationSpeedBySteps(cameraSteps);
        return;
    }

    // A captured pointer edit owns its baseline. Camera drags still permit
    // the existing wheel zoom behavior, but editing never nests a wheel edit.
    if (viewportMouseGesture_.action == ViewportMouseAction::pick
        || (stagedCleaveHistory_.gestureActive && !stagedCleaveWheelSession_.active()))
    {
        return;
    }
    const Qt::KeyboardModifiers wheelModifiers = event->modifiers()
        & ui::shortcuts::editorModifierMask();
    if (stagedCleaveWheelSession_.active()
        && !wheelModifiers.testFlag(Qt::ControlModifier))
    {
        (void)finishStagedCleaveWheelSession();
    }
    const bool rotateArmedShape = cleaveShapeOptions_.placementArmed
        && wheelModifiers == (Qt::ControlModifier | Qt::ShiftModifier);
    const bool cleaveWheel = isStagedCleaveInteractionActive()
        && (wheelModifiers == Qt::ControlModifier || rotateArmedShape)
        && (cleaveShapeOptions_.placementArmed
            || stagedCleaveTransformMode_ != StagedCleaveTransformMode::Move
            || stagedCleaveWheelSession_.active());
    if (cleaveWheel)
    {
        const double steps = event->angleDelta().y() != 0
            ? static_cast<double>(event->angleDelta().y()) / 120.0
            : static_cast<double>(event->pixelDelta().y()) / 80.0;
        if (cleaveShapeOptions_.placementArmed)
        {
            (void)applyStagedCleaveShapeWheelTransform(
                steps, rotateArmedShape, event->position().toPoint(), event->modifiers());
        }
        else
        {
            (void)applyStagedCleaveWheelTransform(steps);
        }
        return;
    }
    if (stagedCleaveWheelSession_.active())
    {
        // A changed chord cannot leak one event to camera zoom while the
        // Ctrl-held draft session still owns the viewport.
        return;
    }
    if (!viewportWheelBindingMatches(navigation.zoom, event->modifiers())
        || std::abs(cameraSteps) < 0.001f)
    {
        return;
    }
    if (navigation.zoom.inverted) cameraSteps = -cameraSteps;
    zoomCameraBySteps(cameraSteps);
}

} // namespace sed
