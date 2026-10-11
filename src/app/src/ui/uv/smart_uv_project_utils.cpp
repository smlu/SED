#include "ui/uv/smart_uv_project_utils.hpp"

#include "ui/uv/uv_surface_geometry_utils.hpp"

#include <algorithm>
#include <cmath>
#include <iterator>
#include <limits>
#include <numbers>
#include <numeric>
#include <optional>
#include <utility>

#include <QLineF>

namespace sed::ui::uv
{
namespace
{

constexpr double kIgnoredFaceArea = 1.0e-12;
constexpr qreal kLayoutEpsilon = 1.0e-12;

struct SmartFace
{
    std::size_t islandIndex{};
    double area{};
    QVector3D normal{};
    std::vector<TVector> points{};
    int projectionGroup{ -1 };
};

struct PackedComponent
{
    std::vector<std::size_t> faceIndices{};
    QRectF bounds{};
    std::size_t stableIndex{};
};

bool collectFaceGeometry(
    const Level& level,
    const UvIslandState& island,
    std::vector<TVector>& points,
    double& area,
    QVector3D& geometryNormal)
{
    if (island.ref.sector < 0
        || island.ref.surface < 0
        || static_cast<std::size_t>(island.ref.sector) >= level.sectors.size())
    {
        return false;
    }

    const Sector& sector = level.sectors[static_cast<std::size_t>(island.ref.sector)];
    if (static_cast<std::size_t>(island.ref.surface) >= sector.surfaces.size())
    {
        return false;
    }

    const Surface& surface = sector.surfaces[static_cast<std::size_t>(island.ref.surface)];
    points.clear();
    points.reserve(surface.vertices.size());
    for (std::size_t localIndex = 0; localIndex < surface.vertices.size(); ++localIndex)
    {
        const std::optional<TVector> point = uvSurfaceVertexPosition(level, island.ref, localIndex);
        if (!point)
        {
            return false;
        }
        points.push_back(*point);
    }

    if (points.size() < 3)
    {
        area = 0.0;
        geometryNormal = {};
        return true;
    }

    // Vector-area form of Newell's polygon area. It handles convex and
    // concave planar faces without depending on an arbitrary fan diagonal.
    double crossX = 0.0;
    double crossY = 0.0;
    double crossZ = 0.0;
    for (std::size_t i = 0; i < points.size(); ++i)
    {
        const TVector a = points[i] - points.front();
        const TVector b = points[(i + 1u) % points.size()] - points.front();
        crossX += a.y * b.z
            - a.z * b.y;
        crossY += a.z * b.x
            - a.x * b.z;
        crossZ += a.x * b.y
            - a.y * b.x;
    }

    const double normalLength = std::hypot(crossX, crossY, crossZ);
    area = 0.5 * normalLength;
    if (!std::isfinite(area))
    {
        return false;
    }
    if (area <= kIgnoredFaceArea)
    {
        geometryNormal = {};
        return true;
    }
    geometryNormal = QVector3D(
        static_cast<float>(crossX / normalLength),
        static_cast<float>(crossY / normalLength),
        static_cast<float>(crossZ / normalLength));
    return normalizeUvVector(geometryNormal);
}

std::vector<QVector3D> calculateProjectionNormals(
    const std::vector<SmartFace>& faces,
    const double angleLimitDeg,
    const double areaWeight)
{
    std::vector<std::size_t> byArea(faces.size());
    std::iota(byArea.begin(), byArea.end(), std::size_t{0});
    std::stable_sort(byArea.begin(), byArea.end(), [&faces](const std::size_t lhs, const std::size_t rhs)
        {
            if (faces[lhs].area != faces[rhs].area)
            {
                return faces[lhs].area > faces[rhs].area;
            }
            return faces[lhs].islandIndex < faces[rhs].islandIndex;
        });

    const double angle = std::clamp(angleLimitDeg, 0.0, 89.0)
        * std::numbers::pi_v<double> / 180.0;
    const float halfLimitCos = static_cast<float>(std::cos(angle * 0.5));
    const float limitCos = static_cast<float>(std::cos(angle));
    const double clampedAreaWeight = std::clamp(areaWeight, 0.0, 1.0);

    std::vector<bool> tagged(faces.size(), false);
    QVector3D seedNormal = faces[byArea.front()].normal;
    std::vector<std::size_t> seedFaces{};
    std::vector<QVector3D> projectionNormals{};

    while (true)
    {
        // Blender scans from the small-area end after seeding with the
        // largest face. Preserve that deterministic order for equal normals.
        for (auto it = byArea.rbegin(); it != byArea.rend(); ++it)
        {
            const std::size_t faceIndex = *it;
            if (tagged[faceIndex])
            {
                continue;
            }
            const float seedDot = QVector3D::dotProduct(faces[faceIndex].normal, seedNormal);
            const bool withinSeed = halfLimitCos >= 1.0f
                ? seedDot >= (1.0f - 1.0e-6f)
                : seedDot > halfLimitCos;
            if (withinSeed)
            {
                seedFaces.push_back(faceIndex);
                tagged[faceIndex] = true;
            }
        }

        QVector3D averageNormal{};
        for (const std::size_t faceIndex : seedFaces)
        {
            const double blend = (faces[faceIndex].area * clampedAreaWeight)
                + (1.0 - clampedAreaWeight);
            averageNormal += faces[faceIndex].normal * static_cast<float>(blend);
        }
        if (normalizeUvVector(averageNormal))
        {
            projectionNormals.push_back(averageNormal);
        }

        float mostUniqueDot = 1.0f;
        std::optional<std::size_t> mostUniqueFace{};
        for (auto it = byArea.rbegin(); it != byArea.rend(); ++it)
        {
            const std::size_t faceIndex = *it;
            if (tagged[faceIndex])
            {
                continue;
            }

            float closestProjectionDot = -1.0f;
            for (const QVector3D& projectionNormal : projectionNormals)
            {
                closestProjectionDot = std::max(
                    closestProjectionDot,
                    QVector3D::dotProduct(projectionNormal, faces[faceIndex].normal));
            }
            if (closestProjectionDot < mostUniqueDot)
            {
                mostUniqueDot = closestProjectionDot;
                mostUniqueFace = faceIndex;
            }
        }

        if (mostUniqueFace && mostUniqueDot < limitCos)
        {
            seedNormal = faces[*mostUniqueFace].normal;
            seedFaces.clear();
            seedFaces.push_back(*mostUniqueFace);
            tagged[*mostUniqueFace] = true;
            continue;
        }
        break;
    }

    return projectionNormals;
}

std::pair<QVector3D, QVector3D> projectionAxes(const QVector3D& normal)
{
    const float ax = std::abs(normal.x());
    const float ay = std::abs(normal.y());
    const float az = std::abs(normal.z());

    QVector3D reference{};
    if (az >= ax && az >= ay)
    {
        reference = QVector3D(0.0f, 1.0f, 0.0f);
    }
    else if (ax >= ay)
    {
        reference = QVector3D(0.0f, 0.0f, 1.0f);
    }
    else
    {
        reference = QVector3D(1.0f, 0.0f, 0.0f);
    }

    QVector3D uAxis = QVector3D::crossProduct(reference, normal);
    if (!normalizeUvVector(uAxis))
    {
        uAxis = QVector3D(1.0f, 0.0f, 0.0f);
    }
    QVector3D vAxis = QVector3D::crossProduct(normal, uAxis);
    if (!normalizeUvVector(vAxis))
    {
        vAxis = QVector3D(0.0f, 1.0f, 0.0f);
    }
    return { uAxis, vAxis };
}

QRectF componentBounds(
    const std::vector<UvIslandState>& islands,
    const std::vector<SmartFace>& faces,
    const std::vector<std::size_t>& faceIndices)
{
    QRectF bounds{};
    bool haveBounds = false;
    for (const std::size_t faceIndex : faceIndices)
    {
        const QRectF faceBounds = uvPointBounds(islands[faces[faceIndex].islandIndex].unwrapUv);
        if (!faceBounds.isValid())
        {
            continue;
        }
        bounds = haveBounds ? bounds.united(faceBounds) : faceBounds;
        haveBounds = true;
    }
    return haveBounds ? bounds : QRectF{};
}

qreal normalizeQuarterTurn(const qreal radians)
{
    qreal result = radians;
    constexpr qreal quarterTurn = std::numbers::pi_v<qreal> * 0.5;
    constexpr qreal eighthTurn = std::numbers::pi_v<qreal> * 0.25;
    while (result > eighthTurn)
    {
        result -= quarterTurn;
    }
    while (result < -eighthTurn)
    {
        result += quarterTurn;
    }
    return result;
}

QRectF rotatedComponentBounds(
    const std::vector<UvIslandState>& islands,
    const std::vector<SmartFace>& faces,
    const std::vector<std::size_t>& faceIndices,
    const qreal radians)
{
    const qreal cs = std::cos(radians);
    const qreal sn = std::sin(radians);
    std::vector<QPointF> rotated{};
    for (const std::size_t faceIndex : faceIndices)
    {
        const std::vector<QPointF>& points = islands[faces[faceIndex].islandIndex].unwrapUv;
        rotated.reserve(rotated.size() + points.size());
        for (const QPointF& point : points)
        {
            rotated.emplace_back(
                point.x() * cs - point.y() * sn,
                point.x() * sn + point.y() * cs);
        }
    }
    return uvPointBounds(rotated);
}

qreal minimalAxisAlignedRotation(
    const std::vector<UvIslandState>& islands,
    const std::vector<SmartFace>& faces,
    const std::vector<std::size_t>& faceIndices)
{
    std::vector<qreal> candidates{ 0.0 };
    for (const std::size_t faceIndex : faceIndices)
    {
        const std::vector<QPointF>& points = islands[faces[faceIndex].islandIndex].unwrapUv;
        for (std::size_t i = 0; i < points.size(); ++i)
        {
            const QPointF edge = points[(i + 1u) % points.size()] - points[i];
            if ((edge.x() * edge.x()) + (edge.y() * edge.y()) <= kLayoutEpsilon)
            {
                continue;
            }
            candidates.push_back(normalizeQuarterTurn(-std::atan2(edge.y(), edge.x())));
        }
    }

    qreal bestRotation = 0.0;
    qreal bestArea = std::numeric_limits<qreal>::max();
    qreal bestMagnitude = std::numeric_limits<qreal>::max();
    for (const qreal candidate : candidates)
    {
        const QRectF bounds = rotatedComponentBounds(islands, faces, faceIndices, candidate);
        const qreal area = bounds.isValid() ? bounds.width() * bounds.height() : 0.0;
        const qreal magnitude = std::abs(candidate);
        if (area < bestArea - kLayoutEpsilon
            || (std::abs(area - bestArea) <= kLayoutEpsilon && magnitude < bestMagnitude - kLayoutEpsilon)
            || (std::abs(area - bestArea) <= kLayoutEpsilon
                && std::abs(magnitude - bestMagnitude) <= kLayoutEpsilon
                && candidate < bestRotation))
        {
            bestRotation = candidate;
            bestArea = area;
            bestMagnitude = magnitude;
        }
    }
    return bestRotation;
}

std::optional<QPointF> geometryProjectedWorldUp(
    const QVector3D& projectionNormal,
    const QPointF& projectionScale)
{
    const auto [uAxis, vAxis] = projectionAxes(projectionNormal);
    const QVector3D worldUp(0.0f, 0.0f, 1.0f);
    const QPointF projectedUp(
        QVector3D::dotProduct(worldUp, uAxis) * projectionScale.x(),
        QVector3D::dotProduct(worldUp, vAxis) * projectionScale.y());
    if ((projectedUp.x() * projectedUp.x())
            + (projectedUp.y() * projectedUp.y()) <= kLayoutEpsilon)
    {
        // A horizontal face has no meaningful in-plane world-up direction.
        return std::nullopt;
    }
    return projectedUp;
}

std::optional<qreal> componentWorldVerticalRotation(
    const std::vector<UvIslandState>& islands,
    const std::vector<SmartFace>& faces,
    const std::vector<std::size_t>& faceIndices,
    const QVector3D& projectionNormal,
    const QPointF& projectionScale)
{
    QPointF projectedUpSum{};
    double totalWeight = 0.0;
    double authoredUpVSum = 0.0;
    double authoredWeight = 0.0;
    constexpr double kMinimumVerticalAlignment = 0.35;
    for (const std::size_t faceIndex : faceIndices)
    {
        const SmartFace& face = faces[faceIndex];
        const std::vector<QPointF>& projected =
            islands[face.islandIndex].unwrapUv;
        if (face.points.size() != projected.size())
        {
            continue;
        }
        for (std::size_t edgeIndex = 0; edgeIndex < face.points.size(); ++edgeIndex)
        {
            TVector worldEdge = face.points[(edgeIndex + 1u) % face.points.size()]
                - face.points[edgeIndex];
            const double worldLength = std::hypot(worldEdge.x, worldEdge.y, worldEdge.z);
            if (!std::isfinite(worldLength) || worldLength <= kLayoutEpsilon)
            {
                continue;
            }
            worldEdge.Scale(1.0 / worldLength);
            const double signedVertical = worldEdge.z;
            const double verticalAlignment = std::abs(signedVertical);
            if (verticalAlignment < kMinimumVerticalAlignment)
            {
                continue;
            }

            QPointF projectedEdge = projected[(edgeIndex + 1u) % projected.size()]
                - projected[edgeIndex];
            if (signedVertical < 0.0)
            {
                projectedEdge = -projectedEdge;
            }
            const double projectedLength = std::hypot(
                projectedEdge.x(), projectedEdge.y());
            if (!std::isfinite(projectedLength)
                || projectedLength <= kLayoutEpsilon)
            {
                continue;
            }
            const double weight = verticalAlignment * worldLength;
            projectedUpSum += projectedEdge * (weight / projectedLength);
            totalWeight += weight;

            // Smart projection should not arbitrarily turn an already upright
            // texture upside-down. Preserve the component's authored 180-degree
            // sign when its current UVs provide a reliable vertical direction;
            // projection still determines the new angle and scale.
            const UvIslandState& island = islands[face.islandIndex];
            if (island.uv.size() == face.points.size())
            {
                QPointF authoredEdge =
                    islandStoredFromDisplayUv(
                        island,
                        island.uv[(edgeIndex + 1u) % island.uv.size()])
                    - islandStoredFromDisplayUv(
                        island,
                        island.uv[edgeIndex]);
                if (signedVertical < 0.0)
                {
                    authoredEdge = -authoredEdge;
                }
                const double authoredLength = std::hypot(
                    authoredEdge.x(), authoredEdge.y());
                if (std::isfinite(authoredLength)
                    && authoredLength > kLayoutEpsilon)
                {
                    authoredUpVSum += weight
                        * authoredEdge.y() / authoredLength;
                    authoredWeight += weight;
                }
            }
        }
    }

    const double sumLength = std::hypot(
        projectedUpSum.x(), projectedUpSum.y());
    const std::optional<QPointF> analyticWorldUp = geometryProjectedWorldUp(
        projectionNormal, projectionScale);
    constexpr double kMinimumAuthoredSignCoherence = 0.2;
    const qreal targetVerticalAngle = authoredWeight > kLayoutEpsilon
            && std::abs(authoredUpVSum) / authoredWeight
                >= kMinimumAuthoredSignCoherence
            && authoredUpVSum < 0.0
        ? -std::numbers::pi_v<qreal> * 0.5
        : std::numbers::pi_v<qreal> * 0.5;
    if (totalWeight > kLayoutEpsilon && sumLength > kLayoutEpsilon)
    {
        // Broad or curved projection groups can contain upward boundary edges
        // whose horizontal components nearly cancel. A tiny residual must not
        // choose the component's 180-degree sign. Fall back to projected world
        // up when the edge vote is incoherent.
        constexpr double kMinimumDirectionCoherence = 0.2;
        if (analyticWorldUp
            && sumLength / totalWeight < kMinimumDirectionCoherence)
        {
            return targetVerticalAngle
                - std::atan2(analyticWorldUp->y(), analyticWorldUp->x());
        }
        return targetVerticalAngle
            - std::atan2(projectedUpSum.y(), projectedUpSum.x());
    }
    if (analyticWorldUp)
    {
        return targetVerticalAngle
            - std::atan2(analyticWorldUp->y(), analyticWorldUp->x());
    }
    return std::nullopt;
}

void rotateComponent(
    std::vector<UvIslandState>& islands,
    const std::vector<SmartFace>& faces,
    const std::vector<std::size_t>& faceIndices,
    const qreal radians)
{
    if (std::abs(radians) <= kLayoutEpsilon)
    {
        return;
    }
    const qreal cs = std::cos(radians);
    const qreal sn = std::sin(radians);
    for (const std::size_t faceIndex : faceIndices)
    {
        for (QPointF& point : islands[faces[faceIndex].islandIndex].unwrapUv)
        {
            point = QPointF(
                point.x() * cs - point.y() * sn,
                point.x() * sn + point.y() * cs);
        }
    }
}

void translateComponent(
    std::vector<UvIslandState>& islands,
    const std::vector<SmartFace>& faces,
    const std::vector<std::size_t>& faceIndices,
    const QPointF& delta)
{
    for (const std::size_t faceIndex : faceIndices)
    {
        for (QPointF& point : islands[faces[faceIndex].islandIndex].unwrapUv)
        {
            point += delta;
        }
    }
}

bool packComponents(
    std::vector<UvIslandState>& islands,
    const std::vector<SmartFace>& faces,
    std::vector<PackedComponent>& components,
    const std::vector<QVector3D>& projectionNormals,
    const SmartUvProjectOptions& options,
    const QPointF& projectionScale)
{
    if (components.empty())
    {
        return false;
    }

    double totalArea = 0.0;
    qreal maxWidth = 0.0;
    qreal maxExtent = 0.0;
    for (PackedComponent& component : components)
    {
        qreal rotation = minimalAxisAlignedRotation(islands, faces, component.faceIndices);
        if (options.rotationMethod == SmartUvRotationMethod::geometryWorldVertical
            && !component.faceIndices.empty())
        {
            const int group = faces[component.faceIndices.front()].projectionGroup;
            if (group >= 0
                && static_cast<std::size_t>(group) < projectionNormals.size())
            {
                rotation = componentWorldVerticalRotation(
                    islands,
                    faces,
                    component.faceIndices,
                    projectionNormals[static_cast<std::size_t>(group)],
                    projectionScale)
                    .value_or(rotation);
            }
        }
        QRectF rotatedBounds = rotatedComponentBounds(islands, faces, component.faceIndices, rotation);
        if (options.rotationMethod == SmartUvRotationMethod::axisAlignedHorizontal
            && rotatedBounds.height() > rotatedBounds.width())
        {
            rotation += std::numbers::pi_v<qreal> * 0.5;
        }
        else if (options.rotationMethod == SmartUvRotationMethod::axisAlignedVertical
                 && rotatedBounds.width() > rotatedBounds.height())
        {
            rotation += std::numbers::pi_v<qreal> * 0.5;
        }

        rotateComponent(islands, faces, component.faceIndices, rotation);
        component.bounds = componentBounds(islands, faces, component.faceIndices);
        if (!component.bounds.isValid()
            || component.bounds.width() <= kLayoutEpsilon
            || component.bounds.height() <= kLayoutEpsilon)
        {
            return false;
        }
        const double boxArea = static_cast<double>(component.bounds.width() * component.bounds.height());
        totalArea += std::max(boxArea, static_cast<double>(kLayoutEpsilon));
        maxWidth = std::max(maxWidth, component.bounds.width());
        maxExtent = std::max({ maxExtent, component.bounds.width(), component.bounds.height() });
    }

    const qreal margin = std::clamp<qreal>(static_cast<qreal>(options.islandMargin), 0.0, 1.0);
    const qreal characteristic = std::sqrt(static_cast<qreal>(totalArea) / static_cast<qreal>(components.size()));
    qreal gap = 0.0;
    switch (options.marginMethod)
    {
    case SmartUvMarginMethod::scaled:
        gap = 2.0 * margin * characteristic;
        break;
    case SmartUvMarginMethod::add:
        gap = 2.0 * margin;
        break;
    case SmartUvMarginMethod::fraction:
    {
        const qreal fraction = std::min<qreal>(margin, 0.49);
        const qreal provisionalSpan = std::max(maxExtent, std::sqrt(static_cast<qreal>(totalArea)));
        gap = (2.0 * fraction * provisionalSpan) / std::max<qreal>(0.02, 1.0 - (2.0 * fraction));
        break;
    }
    }

    std::vector<std::size_t> packOrder(components.size());
    std::iota(packOrder.begin(), packOrder.end(), std::size_t{0});
    std::stable_sort(packOrder.begin(), packOrder.end(), [&components](const std::size_t lhs, const std::size_t rhs)
        {
            if (components[lhs].bounds.height() != components[rhs].bounds.height())
            {
                return components[lhs].bounds.height() > components[rhs].bounds.height();
            }
            const qreal lhsArea = components[lhs].bounds.width() * components[lhs].bounds.height();
            const qreal rhsArea = components[rhs].bounds.width() * components[rhs].bounds.height();
            if (lhsArea != rhsArea)
            {
                return lhsArea > rhsArea;
            }
            return components[lhs].stableIndex < components[rhs].stableIndex;
        });

    const qreal targetRowWidth = std::max(
        maxWidth,
        std::sqrt(static_cast<qreal>(totalArea)) * 1.25 + gap);
    qreal cursorX = 0.0;
    qreal cursorY = 0.0;
    qreal rowHeight = 0.0;
    for (const std::size_t componentIndex : packOrder)
    {
        PackedComponent& component = components[componentIndex];
        if (cursorX > 0.0 && cursorX + component.bounds.width() > targetRowWidth)
        {
            cursorX = 0.0;
            cursorY += rowHeight + gap;
            rowHeight = 0.0;
        }

        const QPointF delta(
            cursorX - component.bounds.left(),
            cursorY - component.bounds.top());
        translateComponent(islands, faces, component.faceIndices, delta);
        component.bounds.translate(delta);
        cursorX += component.bounds.width() + gap;
        rowHeight = std::max(rowHeight, component.bounds.height());
    }

    QRectF layoutBounds{};
    bool haveLayoutBounds = false;
    for (const PackedComponent& component : components)
    {
        layoutBounds = haveLayoutBounds ? layoutBounds.united(component.bounds) : component.bounds;
        haveLayoutBounds = true;
    }
    if (!haveLayoutBounds
        || layoutBounds.width() <= kLayoutEpsilon
        || layoutBounds.height() <= kLayoutEpsilon)
    {
        return false;
    }

    const qreal outerPadding = gap * 0.5;
    const qreal uniformScale = std::min(
        1.0 / (layoutBounds.width() + (2.0 * outerPadding)),
        1.0 / (layoutBounds.height() + (2.0 * outerPadding)));
    for (const SmartFace& face : faces)
    {
        for (QPointF& point : islands[face.islandIndex].unwrapUv)
        {
            point = QPointF(
                (point.x() - layoutBounds.left() + outerPadding) * uniformScale,
                (point.y() - layoutBounds.top() + outerPadding) * uniformScale);
        }
    }

    if (options.scaleToBounds)
    {
        QRectF packedBounds{};
        bool havePackedBounds = false;
        for (const SmartFace& face : faces)
        {
            const QRectF faceBounds = uvPointBounds(islands[face.islandIndex].unwrapUv);
            packedBounds = havePackedBounds ? packedBounds.united(faceBounds) : faceBounds;
            havePackedBounds = true;
        }
        if (!havePackedBounds
            || packedBounds.width() <= kLayoutEpsilon
            || packedBounds.height() <= kLayoutEpsilon)
        {
            return false;
        }

        for (const SmartFace& face : faces)
        {
            for (QPointF& point : islands[face.islandIndex].unwrapUv)
            {
                point = QPointF(
                    (point.x() - packedBounds.left()) / packedBounds.width(),
                    (point.y() - packedBounds.top()) / packedBounds.height());
            }
        }
    }

    const qreal islandScale = static_cast<qreal>(options.islandScale);
    if (!std::isfinite(islandScale) || islandScale <= 0.0 || islandScale > 100.0)
    {
        return false;
    }
    if (std::abs(islandScale - 1.0) > kLayoutEpsilon)
    {
        totalArea = 0.0;
        maxWidth = 0.0;
        for (PackedComponent& component : components)
        {
            component.bounds = componentBounds(islands, faces, component.faceIndices);
            if (!component.bounds.isValid())
            {
                return false;
            }
            const QPointF center = component.bounds.center();
            for (const std::size_t faceIndex : component.faceIndices)
            {
                for (QPointF& point : islands[faces[faceIndex].islandIndex].unwrapUv)
                {
                    point = center + ((point - center) * islandScale);
                }
            }
            component.bounds = componentBounds(islands, faces, component.faceIndices);
            if (!component.bounds.isValid()
                || component.bounds.width() <= kLayoutEpsilon
                || component.bounds.height() <= kLayoutEpsilon)
            {
                return false;
            }
            totalArea += static_cast<double>(
                component.bounds.width() * component.bounds.height());
            maxWidth = std::max(maxWidth, component.bounds.width());
        }

        std::vector<std::size_t> scaledPackOrder(components.size());
        std::iota(scaledPackOrder.begin(), scaledPackOrder.end(), std::size_t{0});
        std::stable_sort(
            scaledPackOrder.begin(),
            scaledPackOrder.end(),
            [&components](const std::size_t lhs, const std::size_t rhs)
            {
                if (components[lhs].bounds.height() != components[rhs].bounds.height())
                {
                    return components[lhs].bounds.height() > components[rhs].bounds.height();
                }
                return components[lhs].stableIndex < components[rhs].stableIndex;
            });

        // Repack expanded islands so the scale multiplier never creates
        // accidental overlap. The layout may intentionally exceed 0..1: SED
        // textures repeat, and that is the purpose of this control.
        const qreal scaledGap = std::clamp<qreal>(
            static_cast<qreal>(options.islandMargin) * 2.0, 0.0, 2.0);
        const qreal scaledRowWidth = std::max(
            maxWidth,
            std::sqrt(static_cast<qreal>(totalArea)) * 1.25 + scaledGap);
        cursorX = 0.0;
        cursorY = 0.0;
        rowHeight = 0.0;
        for (const std::size_t componentIndex : scaledPackOrder)
        {
            PackedComponent& component = components[componentIndex];
            if (cursorX > 0.0
                && cursorX + component.bounds.width() > scaledRowWidth)
            {
                cursorX = 0.0;
                cursorY += rowHeight + scaledGap;
                rowHeight = 0.0;
            }
            const QPointF delta(
                cursorX - component.bounds.left(),
                cursorY - component.bounds.top());
            translateComponent(islands, faces, component.faceIndices, delta);
            component.bounds.translate(delta);
            cursorX += component.bounds.width() + scaledGap;
            rowHeight = std::max(rowHeight, component.bounds.height());
        }
    }

    for (const SmartFace& face : faces)
    {
        for (const QPointF& point : islands[face.islandIndex].unwrapUv)
        {
            if (!std::isfinite(point.x()) || !std::isfinite(point.y()))
            {
                return false;
            }
        }
    }
    return true;
}

} // namespace

bool smartUvProject(
    const Level& level,
    std::vector<UvIslandState>& islands,
    const SmartUvProjectOptions& options,
    const QSize& textureSize,
    SmartUvProjectReport* report)
{
    const bool validMarginMethod =
        options.marginMethod == SmartUvMarginMethod::scaled
        || options.marginMethod == SmartUvMarginMethod::add
        || options.marginMethod == SmartUvMarginMethod::fraction;
    const bool validRotationMethod =
        options.rotationMethod == SmartUvRotationMethod::axisAligned
        || options.rotationMethod
            == SmartUvRotationMethod::axisAlignedHorizontal
        || options.rotationMethod
            == SmartUvRotationMethod::axisAlignedVertical
        || options.rotationMethod
            == SmartUvRotationMethod::geometryWorldVertical;
    if (islands.empty()
        || !validMarginMethod
        || !validRotationMethod
        || !std::isfinite(options.angleLimitDeg)
        || !std::isfinite(options.islandMargin)
        || !std::isfinite(options.areaWeight)
        || !std::isfinite(options.islandScale))
    {
        return false;
    }

    std::vector<UvIslandState> next = islands;
    SmartUvProjectReport nextReport{};
    nextReport.faceProjectionGroups.assign(islands.size(), -1);

    std::vector<SmartFace> faces{};
    faces.reserve(islands.size());
    for (std::size_t islandIndex = 0; islandIndex < islands.size(); ++islandIndex)
    {
        std::vector<TVector> points{};
        double area = 0.0;
        QVector3D geometryNormal{};
        if (!collectFaceGeometry(level, islands[islandIndex], points, area, geometryNormal))
        {
            return false;
        }

        if (!(area > kIgnoredFaceArea))
        {
            ++nextReport.ignoredFaceCount;
            next[islandIndex].unwrapUv.assign(points.size(), QPointF{});
            continue;
        }

        // Blender projects from evaluated mesh geometry normals. SED stores a
        // serialized Surface::normal as well, but that value may be rounded or
        // stale while editing. A dominant-axis switch in that stale normal can
        // rotate a slightly banked wall by a quarter turn, so the projection
        // and world-vertical basis must use the vertices collected above.
        faces.push_back(SmartFace{ islandIndex, area, geometryNormal, std::move(points), -1 });
    }

    if (faces.empty())
    {
        return false;
    }

    nextReport.projectionNormals = calculateProjectionNormals(
        faces,
        options.angleLimitDeg,
        options.areaWeight);
    if (nextReport.projectionNormals.empty())
    {
        return false;
    }
    nextReport.projectionGroupCount = nextReport.projectionNormals.size();

    const TVector projectionOrigin = faces.front().points.front();
    for (SmartFace& face : faces)
    {
        float bestDot = QVector3D::dotProduct(face.normal, nextReport.projectionNormals.front());
        int bestGroup = 0;
        for (int groupIndex = 1; groupIndex < static_cast<int>(nextReport.projectionNormals.size()); ++groupIndex)
        {
            const float dot = QVector3D::dotProduct(
                face.normal,
                nextReport.projectionNormals[static_cast<std::size_t>(groupIndex)]);
            if (dot > bestDot)
            {
                bestDot = dot;
                bestGroup = groupIndex;
            }
        }
        face.projectionGroup = bestGroup;
        nextReport.faceProjectionGroups[face.islandIndex] = bestGroup;

        const auto [uAxis, vAxis] = projectionAxes(
            nextReport.projectionNormals[static_cast<std::size_t>(bestGroup)]);
        std::vector<QPointF>& projected = next[face.islandIndex].unwrapUv;
        projected.clear();
        projected.reserve(face.points.size());
        for (const TVector& point : face.points)
        {
            // Keep authored detail in a common local frame; absolute world
            // coordinates must never pass through QVector3D before subtraction.
            const TVector local = point - projectionOrigin;
            projected.emplace_back(
                local.x * uAxis.x() + local.y * uAxis.y() + local.z * uAxis.z(),
                local.x * vAxis.x() + local.y * vAxis.y() + local.z * vAxis.z());
        }
    }

    QPointF projectionScale(1.0, 1.0);
    if (options.correctAspect)
    {
        const QSize safeSize = sanitizeUvTextureSize(textureSize);
        const qreal aspect = static_cast<qreal>(safeSize.width())
            / static_cast<qreal>(safeSize.height());
        if (aspect > 1.0)
        {
            projectionScale.setX(1.0 / aspect);
            for (const SmartFace& face : faces)
            {
                for (QPointF& point : next[face.islandIndex].unwrapUv)
                {
                    point.setX(point.x() / aspect);
                }
            }
        }
        else if (aspect > 0.0 && aspect < 1.0)
        {
            projectionScale.setY(aspect);
            for (const SmartFace& face : faces)
            {
                for (QPointF& point : next[face.islandIndex].unwrapUv)
                {
                    point.setY(point.y() * aspect);
                }
            }
        }
    }

    std::vector<std::size_t> parent(faces.size());
    std::iota(parent.begin(), parent.end(), std::size_t{0});
    auto findParent = [&parent](std::size_t index)
    {
        std::size_t root = index;
        while (parent[root] != root)
        {
            root = parent[root];
        }
        while (parent[index] != index)
        {
            const std::size_t nextIndex = parent[index];
            parent[index] = root;
            index = nextIndex;
        }
        return root;
    };
    auto unite = [&parent, &findParent](const std::size_t lhs, const std::size_t rhs)
    {
        const std::size_t lhsRoot = findParent(lhs);
        const std::size_t rhsRoot = findParent(rhs);
        if (lhsRoot != rhsRoot)
        {
            parent[rhsRoot] = lhsRoot;
        }
    };

    for (std::size_t i = 0; i < faces.size(); ++i)
    {
        for (std::size_t j = i + 1; j < faces.size(); ++j)
        {
            if (faces[i].projectionGroup == faces[j].projectionGroup
                && uvSurfacesShareEdge(
                    level,
                    islands[faces[i].islandIndex].ref,
                    islands[faces[j].islandIndex].ref))
            {
                unite(i, j);
            }
        }
    }

    std::vector<PackedComponent> components{};
    for (std::size_t faceIndex = 0; faceIndex < faces.size(); ++faceIndex)
    {
        const std::size_t root = findParent(faceIndex);
        auto component = std::find_if(components.begin(), components.end(), [root](const PackedComponent& candidate)
            {
                return candidate.stableIndex == root;
            });
        if (component == components.end())
        {
            components.push_back(PackedComponent{ {}, {}, root });
            component = std::prev(components.end());
        }
        component->faceIndices.push_back(faceIndex);
    }
    std::sort(components.begin(), components.end(), [](const PackedComponent& lhs, const PackedComponent& rhs)
        {
            return lhs.stableIndex < rhs.stableIndex;
        });
    nextReport.packedIslandCount = components.size();

    if (!packComponents(
        next,
        faces,
        components,
        nextReport.projectionNormals,
        options,
        projectionScale))
    {
        return false;
    }

    islands = std::move(next);
    if (report)
    {
        *report = std::move(nextReport);
    }
    return true;
}

double estimateSmartUvIslandScale(
    const std::vector<UvIslandState>& current,
    const std::vector<UvIslandState>& projected) noexcept
{
    if (current.size() != projected.size())
    {
        return 1.0;
    }

    std::vector<double> ratios{};
    for (std::size_t islandIndex = 0; islandIndex < current.size(); ++islandIndex)
    {
        const std::vector<QPointF>& before = current[islandIndex].unwrapUv;
        const std::vector<QPointF>& after = projected[islandIndex].unwrapUv;
        if (before.size() != after.size() || before.size() < 2)
        {
            continue;
        }
        for (std::size_t pointIndex = 0; pointIndex < before.size(); ++pointIndex)
        {
            const double beforeLength = QLineF(
                before[pointIndex], before[(pointIndex + 1u) % before.size()]).length();
            const double afterLength = QLineF(
                after[pointIndex], after[(pointIndex + 1u) % after.size()]).length();
            if (std::isfinite(beforeLength)
                && std::isfinite(afterLength)
                && beforeLength > kLayoutEpsilon
                && afterLength > kLayoutEpsilon)
            {
                ratios.push_back(beforeLength / afterLength);
            }
        }
    }
    if (ratios.empty())
    {
        return 1.0;
    }
    const auto middle = ratios.begin()
        + static_cast<std::ptrdiff_t>(ratios.size() / 2u);
    std::nth_element(ratios.begin(), middle, ratios.end());
    double estimate = *middle;
    if ((ratios.size() % 2u) == 0u)
    {
        const auto lower = std::max_element(ratios.begin(), middle);
        estimate = (*lower + *middle) * 0.5;
    }
    return std::clamp(estimate, 0.01, 100.0);
}

} // namespace sed::ui::uv
