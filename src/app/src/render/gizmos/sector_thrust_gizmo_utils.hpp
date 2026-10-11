#pragma once

#include <QVector3D>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>

namespace sed::render
{

// The level unit is deliberately tiny (0.1 JK/IJ units represent one metre).
// Give normalized and sub-unit vectors enough screen length to read their
// direction accurately without letting exceptional values span the viewport.
// This is about 1.4x the original display scale, comfortably below the 2x UX
// ceiling requested for the more precise handle.
inline constexpr float kSectorThrustArrowMinLengthPx = 60.0f;
inline constexpr float kSectorThrustArrowMaxLengthPx = 280.0f;
inline constexpr float kSectorThrustArrowLogScalePx = 42.0f;

/** Whether a Sector has authored thrust rather than only the editor's zero handle. */
[[nodiscard]] inline bool sectorThrustIsSet(
    const std::int32_t flags,
    const QVector3D& thrust,
    const std::int32_t useThrustFlag) noexcept
{
    return (flags & useThrustFlag) != 0
        || (std::isfinite(thrust.x()) && std::isfinite(thrust.y()) && std::isfinite(thrust.z())
            && thrust.lengthSquared() > 1.0e-18f);
}

/** Whether a vector has only finite components and at least one nonzero component. */
[[nodiscard]] inline bool sectorThrustVectorIsFiniteNonzero(const QVector3D& thrust) noexcept
{
    return std::isfinite(thrust.x()) && std::isfinite(thrust.y()) && std::isfinite(thrust.z())
        && (thrust.x() != 0.0f || thrust.y() != 0.0f || thrust.z() != 0.0f);
}

/**
 * Separates passive vector visibility from the direct-edit tool.
 *
 * The direct tool always owns visibility and may optionally include zero-vector
 * handles. Outside that tool, the viewport preference shows every finite,
 * nonzero authored vector, independent of selection.
 */
[[nodiscard]] inline bool sectorThrustGizmoVisible(
    const bool toolEnabled,
    const bool showPassiveVectors,
    const bool showAllToolGizmos,
    const bool vectorIsSet) noexcept
{
    if (toolEnabled)
    {
        return showAllToolGizmos || vectorIsSet;
    }
    return showPassiveVectors && vectorIsSet;
}

/** Maps an exact gameplay magnitude to the bounded viewport handle length. */
[[nodiscard]] inline float sectorThrustDisplayLengthPx(const float magnitude) noexcept
{
    if (!std::isfinite(magnitude) || magnitude <= 0.0f)
    {
        return 0.0f;
    }
    return std::clamp(
        kSectorThrustArrowMinLengthPx + std::log2(1.0f + magnitude) * kSectorThrustArrowLogScalePx,
        kSectorThrustArrowMinLengthPx,
        kSectorThrustArrowMaxLengthPx);
}

/** Inverse of the unsaturated portion of sectorThrustDisplayLengthPx. */
[[nodiscard]] inline float sectorThrustMagnitudeForDisplayLengthPx(const float lengthPx) noexcept
{
    if (!std::isfinite(lengthPx) || lengthPx < kSectorThrustArrowMinLengthPx)
    {
        return 0.0f;
    }
    const float bounded = std::min(lengthPx, kSectorThrustArrowMaxLengthPx);
    return std::max(
        0.0f,
        std::exp2((bounded - kSectorThrustArrowMinLengthPx) / kSectorThrustArrowLogScalePx) - 1.0f);
}

/**
 * Applies a raw logarithmic radial delta relative to an exact magnitude.
 * This low-level transform is intentionally unbounded; viewport handles must
 * call sectorThrustMagnitudeAfterHandleDragPx below. Keeping the transform
 * separate allows an already-authored over-cap magnitude to be reduced
 * without first collapsing it to the visual range.
 */
[[nodiscard]] inline float sectorThrustMagnitudeAfterDisplayDeltaPx(
    const float baselineMagnitude,
    const float displayDeltaPx) noexcept
{
    if (!std::isfinite(baselineMagnitude) || baselineMagnitude < 0.0f
        || !std::isfinite(displayDeltaPx))
    {
        return 0.0f;
    }

    constexpr double kLn2 = 0.69314718055994530942;
    const double nextLog = std::log1p(static_cast<double>(baselineMagnitude))
        + (static_cast<double>(displayDeltaPx) / static_cast<double>(kSectorThrustArrowLogScalePx)) * kLn2;
    if (!std::isfinite(nextLog) || nextLog <= 0.0)
    {
        return 0.0f;
    }

    const double maxLog = std::log(static_cast<double>(std::numeric_limits<float>::max()));
    if (nextLog >= maxLog)
    {
        return std::numeric_limits<float>::max();
    }
    return static_cast<float>(std::expm1(nextLog));
}

/**
 * Converts a direct handle drag into a gameplay magnitude without allowing an
 * off-handle cursor to create an arbitrarily large value.
 *
 * Both radii are clamped to the same range the viewport can actually draw.
 * Values imported above that range are preserved when the pointer first moves
 * and may be reduced, but direct manipulation cannot increase them further.
 * This keeps exact authored data available through the property editor while
 * making the bounded viewport handle a bounded control as well.
 */
[[nodiscard]] inline float sectorThrustMagnitudeAfterHandleDragPx(
    const float baselineMagnitude,
    const float baselineHandleRadiusPx,
    const float currentHandleRadiusPx) noexcept
{
    if (!std::isfinite(baselineMagnitude) || baselineMagnitude < 0.0f
        || !std::isfinite(baselineHandleRadiusPx)
        || !std::isfinite(currentHandleRadiusPx))
    {
        return 0.0f;
    }

    const float boundedBaselineRadius = std::clamp(
        baselineHandleRadiusPx,
        0.0f,
        kSectorThrustArrowMaxLengthPx);
    const float boundedCurrentRadius = std::clamp(
        currentHandleRadiusPx,
        0.0f,
        kSectorThrustArrowMaxLengthPx);
    const float candidate = sectorThrustMagnitudeAfterDisplayDeltaPx(
        baselineMagnitude,
        boundedCurrentRadius - boundedBaselineRadius);

    // The largest magnitude represented by a non-saturated arrow is the
    // natural ceiling for values created through direct manipulation. An
    // imported value above it is its own ceiling, so merely grabbing the
    // already-saturated handle cannot alter or inflate it.
    const float directlyRepresentableMaximum =
        sectorThrustMagnitudeForDisplayLengthPx(kSectorThrustArrowMaxLengthPx);
    return std::min(candidate, std::max(baselineMagnitude, directlyRepresentableMaximum));
}

[[nodiscard]] inline QVector3D normalizedSectorThrustDirection(const QVector3D& thrust) noexcept
{
    const float magnitude = thrust.length();
    if (!std::isfinite(magnitude) || magnitude <= 1.0e-9f)
    {
        return {};
    }
    return thrust / magnitude;
}

/**
 * Replaces one normalized direction component while keeping that exact typed
 * component. The other two components retain their relative direction and are
 * rescaled onto the unit sphere. If no perpendicular direction exists (for a
 * zero vector or an axis-aligned vector), a deterministic positive axis is
 * chosen for the remaining length.
 */
[[nodiscard]] inline QVector3D sectorThrustDirectionWithComponent(
    const QVector3D& direction,
    const int component,
    const float rawValue) noexcept
{
    QVector3D normalized = normalizedSectorThrustDirection(direction);
    if (component < 0 || component > 2)
    {
        return normalized;
    }

    float values[3]{ normalized.x(), normalized.y(), normalized.z() };
    const float typed = std::isfinite(rawValue) ? std::clamp(rawValue, -1.0f, 1.0f) : 0.0f;
    const int otherA = (component + 1) % 3;
    const int otherB = (component + 2) % 3;
    const float otherLength = std::hypot(values[otherA], values[otherB]);
    const float targetOtherLength = std::sqrt(std::max(0.0f, 1.0f - typed * typed));

    if (targetOtherLength <= 1.0e-7f)
    {
        values[otherA] = 0.0f;
        values[otherB] = 0.0f;
    }
    else if (std::isfinite(otherLength) && otherLength > 1.0e-7f)
    {
        const float scale = targetOtherLength / otherLength;
        values[otherA] *= scale;
        values[otherB] *= scale;
    }
    else
    {
        values[otherA] = targetOtherLength;
        values[otherB] = 0.0f;
    }
    values[component] = typed;
    return QVector3D(values[0], values[1], values[2]);
}

/** Recombines a user direction and magnitude while enforcing a unit direction. */
[[nodiscard]] inline QVector3D composeSectorThrust(
    QVector3D direction,
    const float magnitude) noexcept
{
    const float directionLength = direction.length();
    if (!std::isfinite(directionLength) || directionLength <= 1.0e-9f
        || !std::isfinite(magnitude) || magnitude <= 0.0f)
    {
        return {};
    }
    direction /= directionLength;
    return direction * magnitude;
}

struct SectorThrustArrowGeometry
{
    QVector3D tip{};
    QVector3D headLeft{};
    QVector3D headRight{};
};

/**
 * Builds a camera-facing arrow for an IJIM sector thrust vector.
 *
 * The direction remains exact. Length is logarithmic and screen-relative so
 * small gameplay accelerations remain visible without allowing unusually large
 * values to cover the viewport. Exact normalized components and magnitude stay
 * available in the Sector properties panel.
 */
[[nodiscard]] inline std::optional<SectorThrustArrowGeometry> sectorThrustArrowGeometry(
    const QVector3D& origin,
    const QVector3D& thrust,
    const QVector3D& eye,
    const float worldUnitsPerPixel)
{
    const double magnitude = std::hypot(
        static_cast<double>(thrust.x()),
        static_cast<double>(thrust.y()),
        static_cast<double>(thrust.z()));
    if (!std::isfinite(magnitude) || magnitude <= 0.0
        || !std::isfinite(worldUnitsPerPixel) || worldUnitsPerPixel <= 0.0f)
    {
        return std::nullopt;
    }

    const QVector3D direction(
        static_cast<float>(static_cast<double>(thrust.x()) / magnitude),
        static_cast<float>(static_cast<double>(thrust.y()) / magnitude),
        static_cast<float>(static_cast<double>(thrust.z()) / magnitude));
    const float displayMagnitude = static_cast<float>(std::min(
        magnitude,
        static_cast<double>(std::numeric_limits<float>::max())));
    const float lengthPx = sectorThrustDisplayLengthPx(displayMagnitude);
    const float headLength = worldUnitsPerPixel * 23.0f;
    const float headWidth = worldUnitsPerPixel * 13.0f;
    const QVector3D tip = origin + direction * (worldUnitsPerPixel * lengthPx);

    auto finiteSideLength = [](const QVector3D& value) -> double
    {
        if (!std::isfinite(value.x()) || !std::isfinite(value.y()) || !std::isfinite(value.z()))
        {
            return 0.0;
        }
        return std::hypot(
            static_cast<double>(value.x()),
            static_cast<double>(value.y()),
            static_cast<double>(value.z()));
    };
    QVector3D side = QVector3D::crossProduct(direction, eye - origin);
    double sideLength = finiteSideLength(side);
    if (!std::isfinite(sideLength) || sideLength <= 1.0e-6)
    {
        side = QVector3D::crossProduct(direction, QVector3D(0.0f, 0.0f, 1.0f));
        sideLength = finiteSideLength(side);
    }
    if (!std::isfinite(sideLength) || sideLength <= 1.0e-6)
    {
        side = QVector3D::crossProduct(direction, QVector3D(0.0f, 1.0f, 0.0f));
        sideLength = finiteSideLength(side);
    }
    if (!std::isfinite(sideLength) || sideLength <= 1.0e-6)
    {
        return std::nullopt;
    }
    side = QVector3D(
        static_cast<float>(static_cast<double>(side.x()) / sideLength),
        static_cast<float>(static_cast<double>(side.y()) / sideLength),
        static_cast<float>(static_cast<double>(side.z()) / sideLength));

    const QVector3D headBase = tip - direction * headLength;
    return SectorThrustArrowGeometry{
        tip,
        headBase + side * headWidth,
        headBase - side * headWidth,
    };
}

/** Matches original SED vector-edit behavior for SECF_IJIM_UseThrust. */
[[nodiscard]] inline std::int32_t sectorThrustFlagsForVector(
    const std::int32_t flags,
    const QVector3D& thrust,
    const std::int32_t useThrustFlag) noexcept
{
    return thrust.lengthSquared() <= 1.0e-18f
        ? (flags & ~useThrustFlag)
        : (flags | useThrustFlag);
}

} // namespace sed::render
