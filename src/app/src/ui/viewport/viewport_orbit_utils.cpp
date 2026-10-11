#include "ui/viewport/viewport_orbit_utils.hpp"

#include "sed/math_utils.hpp"
#include "sed/text.hpp"
#include "viewport_camera.hpp"

#include <QQuaternion>

#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>
#include <new>

namespace sed::ui::viewport
{
namespace
{

bool finiteVector(const QVector3D& value) noexcept
{
    return std::isfinite(value.x()) && std::isfinite(value.y()) && std::isfinite(value.z());
}

QVector3D orbitDirection(const float yawDeg, const float pitchDeg) noexcept
{
    constexpr float degreesToRadians = std::numbers::pi_v<float> / 180.0f;
    const float yaw = yawDeg * degreesToRadians;
    const float pitch = pitchDeg * degreesToRadians;
    const float cosPitch = std::cos(pitch);
    return QVector3D(
        cosPitch * std::sin(yaw),
        -cosPitch * std::cos(yaw),
        std::sin(pitch));
}

class SelectionBounds
{
public:
    void add(const double x, const double y, const double z) noexcept
    {
        if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z)
            || x < -static_cast<double>(std::numeric_limits<float>::max())
            || x > static_cast<double>(std::numeric_limits<float>::max())
            || y < -static_cast<double>(std::numeric_limits<float>::max())
            || y > static_cast<double>(std::numeric_limits<float>::max())
            || z < -static_cast<double>(std::numeric_limits<float>::max())
            || z > static_cast<double>(std::numeric_limits<float>::max()))
        {
            return;
        }

        const QVector3D point(
            static_cast<float>(x),
            static_cast<float>(y),
            static_cast<float>(z));
        if (!have_)
        {
            min_ = point;
            max_ = point;
            have_ = true;
            return;
        }

        min_.setX(std::min(min_.x(), point.x()));
        min_.setY(std::min(min_.y(), point.y()));
        min_.setZ(std::min(min_.z(), point.z()));
        max_.setX(std::max(max_.x(), point.x()));
        max_.setY(std::max(max_.y(), point.y()));
        max_.setZ(std::max(max_.z(), point.z()));
    }

    [[nodiscard]] std::optional<QVector3D> center() const noexcept
    {
        if (!have_)
        {
            return std::nullopt;
        }
        const QVector3D result = min_ * 0.5f + max_ * 0.5f;
        return finiteVector(result) ? std::optional<QVector3D>{ result } : std::nullopt;
    }

private:
    QVector3D min_{};
    QVector3D max_{};
    bool have_{};
};

class SelectionMean
{
public:
    void add(const QVector3D& value) noexcept
    {
        if (!finiteVector(value))
        {
            return;
        }
        sum_ += value;
        ++count_;
    }

    [[nodiscard]] std::optional<QVector3D> center() const noexcept
    {
        if (count_ == 0)
        {
            return std::nullopt;
        }
        const QVector3D result = sum_ / static_cast<float>(count_);
        return finiteVector(result) ? std::optional<QVector3D>{ result } : std::nullopt;
    }

private:
    QVector3D sum_{};
    std::size_t count_{};
};

std::optional<QVector3D> surfaceCenter(
    const Level& level,
    const SurfaceRef ref) noexcept
{
    if (ref.sector < 0 || ref.surface < 0
        || static_cast<std::size_t>(ref.sector) >= level.sectors.size())
    {
        return std::nullopt;
    }

    const Sector& sector = level.sectors[static_cast<std::size_t>(ref.sector)];
    if (static_cast<std::size_t>(ref.surface) >= sector.surfaces.size())
    {
        return std::nullopt;
    }

    QVector3D sum{};
    std::size_t count = 0;
    for (const std::int32_t vertexIndex : sector.surfaces[static_cast<std::size_t>(ref.surface)].vertices)
    {
        if (vertexIndex < 0 || static_cast<std::size_t>(vertexIndex) >= sector.vertices.size())
        {
            continue;
        }
        const Vertex& vertex = sector.vertices[static_cast<std::size_t>(vertexIndex)];
        const QVector3D point(
            static_cast<float>(vertex.x),
            static_cast<float>(vertex.y),
            static_cast<float>(vertex.z));
        if (!finiteVector(point))
        {
            continue;
        }
        sum += point;
        ++count;
    }
    return count > 0
        ? std::optional<QVector3D>{sum / static_cast<float>(count)}
        : std::nullopt;
}

} // namespace

std::optional<QVector3D> selectionOrbitPivot(
    const Level& level,
    const editor::Selection& selection,
    const SelectionOrbitFallback& fallback)
{
    SelectionMean selectedCenters{};

    const auto addSector = [&level, &selectedCenters](const std::int32_t sectorIndex)
    {
        if (sectorIndex < 0 || static_cast<std::size_t>(sectorIndex) >= level.sectors.size())
        {
            return;
        }
        SelectionBounds bounds{};
        for (const Vertex& vertex : level.sectors[static_cast<std::size_t>(sectorIndex)].vertices)
        {
            bounds.add(vertex.x, vertex.y, vertex.z);
        }
        if (const auto center = bounds.center())
        {
            selectedCenters.add(*center);
        }
    };

    const auto addSurface = [&level, &selectedCenters](const SurfaceRef ref)
    {
        if (const auto center = surfaceCenter(level, ref))
        {
            selectedCenters.add(*center);
        }
    };

    const auto addThing = [&level, &selectedCenters](const std::int32_t thingIndex)
    {
        if (thingIndex < 0 || static_cast<std::size_t>(thingIndex) >= level.things.size())
        {
            return;
        }
        const Thing& thing = level.things[static_cast<std::size_t>(thingIndex)];
        selectedCenters.add(QVector3D(
            static_cast<float>(thing.x),
            static_cast<float>(thing.y),
            static_cast<float>(thing.z)));
    };

    const auto addFrame = [&level, &selectedCenters](const level::FrameRef& ref)
    {
        if (ref.thing < 0 || ref.value < 0
            || static_cast<std::size_t>(ref.thing) >= level.things.size())
        {
            return;
        }
        const Thing& thing = level.things[static_cast<std::size_t>(ref.thing)];
        if (static_cast<std::size_t>(ref.value) >= thing.values.size())
        {
            return;
        }
        const TplValue& value = thing.values[static_cast<std::size_t>(ref.value)];
        double x{};
        double y{};
        double z{};
        double pitch{};
        double yaw{};
        double roll{};
        if (value.type == ValueType::frame
            && sed::text::iequalsAscii(value.name, "frame")
            && value.tryGetFrame(x, y, z, pitch, yaw, roll))
        {
            selectedCenters.add(QVector3D(
                static_cast<float>(x),
                static_cast<float>(y),
                static_cast<float>(z)));
        }
    };

    const auto addLight = [&level, &selectedCenters](const std::int32_t lightIndex)
    {
        if (lightIndex < 0 || static_cast<std::size_t>(lightIndex) >= level.lights.size())
        {
            return;
        }
        const SedLight& light = level.lights[static_cast<std::size_t>(lightIndex)];
        selectedCenters.add(QVector3D(
            static_cast<float>(light.position.x),
            static_cast<float>(light.position.y),
            static_cast<float>(light.position.z)));
    };

    for (const std::int32_t sectorIndex : selection.sectors)
    {
        addSector(sectorIndex);
    }

    // Surface-vertex and Frame edit selections retain their parent entity for
    // item-editor context. The more specific semantic selection must own the
    // pivot rather than being averaged a second time with that parent.
    if (selection.surfaceVertices.empty())
    {
        for (const SurfaceRef ref : selection.surfaces)
        {
            addSurface(ref);
        }
    }

    for (const editor::SurfaceVertexRef& ref : selection.surfaceVertices)
    {
        if (ref.surface.sector < 0 || ref.surface.surface < 0 || ref.localVertex < 0
            || static_cast<std::size_t>(ref.surface.sector) >= level.sectors.size())
        {
            continue;
        }
        const Sector& sector = level.sectors[static_cast<std::size_t>(ref.surface.sector)];
        if (static_cast<std::size_t>(ref.surface.surface) >= sector.surfaces.size())
        {
            continue;
        }
        const Surface& surface = sector.surfaces[static_cast<std::size_t>(ref.surface.surface)];
        if (static_cast<std::size_t>(ref.localVertex) >= surface.vertices.size())
        {
            continue;
        }
        const std::int32_t vertexIndex = surface.vertices[static_cast<std::size_t>(ref.localVertex)];
        if (vertexIndex < 0 || static_cast<std::size_t>(vertexIndex) >= sector.vertices.size())
        {
            continue;
        }
        const Vertex& vertex = sector.vertices[static_cast<std::size_t>(vertexIndex)];
        selectedCenters.add(QVector3D(
            static_cast<float>(vertex.x),
            static_cast<float>(vertex.y),
            static_cast<float>(vertex.z)));
    }

    if (selection.frames.empty())
    {
        for (const std::int32_t thingIndex : selection.things)
        {
            addThing(thingIndex);
        }
    }

    for (const sed::level::FrameRef& ref : selection.frames)
    {
        addFrame(ref);
    }

    for (const std::int32_t lightIndex : selection.lights)
    {
        addLight(lightIndex);
    }

    if (const auto center = selectedCenters.center())
    {
        return center;
    }

    if (fallback.thing >= 0 && fallback.frameValue >= 0)
    {
        addFrame(level::FrameRef{ fallback.thing, fallback.frameValue });
    }
    else if (fallback.thing >= 0)
    {
        addThing(fallback.thing);
    }
    else if (fallback.light >= 0)
    {
        addLight(fallback.light);
    }
    else if (fallback.surfaceSector >= 0 && fallback.surface >= 0)
    {
        addSurface(SurfaceRef{ fallback.surfaceSector, fallback.surface });
    }
    else if (fallback.sector >= 0)
    {
        addSector(fallback.sector);
    }
    return selectedCenters.center();
}

bool trySelectionOrbitPivot(
    const Level& level,
    const editor::Selection& selection,
    const SelectionOrbitFallback& fallback,
    std::optional<QVector3D>& output)
{
    try
    {
        const auto next = selectionOrbitPivot(level, selection, fallback);
        output = next;
        return true;
    }
    catch (const std::bad_alloc&)
    {
        return false;
    }
}

std::optional<OrbitRigUpdate> orbitRigAroundPivot(
    const QVector3D& currentTarget,
    const float currentDistance,
    const float currentYawDeg,
    const float currentPitchDeg,
    const float currentRollDeg,
    const QVector3D& pivot,
    const float yawDeltaDeg,
    const float pitchDeltaDeg) noexcept
{
    if (!finiteVector(currentTarget) || !finiteVector(pivot)
        || !std::isfinite(currentDistance) || currentDistance <= 0.0f
        || !std::isfinite(currentYawDeg) || !std::isfinite(currentPitchDeg)
        || !std::isfinite(currentRollDeg)
        || !std::isfinite(yawDeltaDeg) || !std::isfinite(pitchDeltaDeg))
    {
        return std::nullopt;
    }

    const app::ViewportCamera::OrbitAngles nextAngles =
        app::ViewportCamera::RotatedOrbit(
            currentYawDeg,
            currentPitchDeg,
            currentRollDeg,
            yawDeltaDeg,
            pitchDeltaDeg);
    if (!nextAngles.valid)
    {
        return std::nullopt;
    }

    // Preserve an exact no-op for an MMB press/release or a move event whose
    // pixels clamp to no angular change. Besides avoiding needless work, this
    // prevents identity-quaternion roundoff from nudging the camera rig.
    if (yawDeltaDeg == 0.0f && pitchDeltaDeg == 0.0f)
    {
        return OrbitRigUpdate{
            currentTarget,
            currentYawDeg,
            currentPitchDeg,
            currentRollDeg,
            currentDistance };
    }

    const QVector3D currentDirection = orbitDirection(currentYawDeg, currentPitchDeg);
    const QVector3D eye = currentTarget + currentDirection * currentDistance;
    if (!finiteVector(eye))
    {
        return std::nullopt;
    }

    const QQuaternion yawRotation = QQuaternion::fromAxisAndAngle(
        QVector3D(0.0f, 0.0f, 1.0f),
        yawDeltaDeg);
    const app::ViewportCamera::OrbitBasis currentBasis =
        app::ViewportCamera::BasisFromOrbit(
            currentYawDeg, currentPitchDeg, currentRollDeg);
    QVector3D pitchAxis = yawRotation.rotatedVector(currentBasis.right);
    if (pitchAxis.lengthSquared() <= sed::math::kTinyEpsilonF)
    {
        return std::nullopt;
    }
    pitchAxis.normalize();
    const QQuaternion pitchRotation = QQuaternion::fromAxisAndAngle(
        pitchAxis,
        -pitchDeltaDeg);
    const QQuaternion rigRotation = pitchRotation * yawRotation;

    const QVector3D nextTarget = pivot + rigRotation.rotatedVector(currentTarget - pivot);
    const QVector3D nextEye = pivot + rigRotation.rotatedVector(eye - pivot);
    const float nextDistance = (nextEye - nextTarget).length();
    if (!finiteVector(nextTarget) || !finiteVector(nextEye)
        || !std::isfinite(nextDistance) || nextDistance <= 0.0f)
    {
        return std::nullopt;
    }

    return OrbitRigUpdate{
        nextTarget,
        nextAngles.yawDeg,
        nextAngles.pitchDeg,
        nextAngles.rollDeg,
        nextDistance };
}

} // namespace sed::ui::viewport
