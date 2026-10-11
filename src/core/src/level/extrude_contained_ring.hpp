#pragma once

#include "sed/geometry.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace sed::level::detail
{

/** Identifies which input contour owns a generated ring corner. */
enum class ContainedRingContour : std::uint8_t
{
    source,
    rectangle,
};

/** A projected contour point and its stable source/rectangle corner identity. */
struct ContainedRingPoint
{
    TPoint2 position{};
    // This is an identity copied to output triangles; it is not the point's
    // position in the supplied span. Identities must be unique per contour.
    std::size_t index{};
};

/** A triangle corner referring to an existing input contour point. */
struct ContainedRingVertex
{
    ContainedRingContour contour{};
    std::size_t index{};

    friend bool operator==(const ContainedRingVertex& lhs,
        const ContainedRingVertex& rhs) noexcept
    {
        return lhs.contour == rhs.contour && lhs.index == rhs.index;
    }
};

struct ContainedRingTriangle
{
    std::array<ContainedRingVertex, 3> vertices{};

    friend bool operator==(const ContainedRingTriangle& lhs,
        const ContainedRingTriangle& rhs) noexcept
    {
        return lhs.vertices == rhs.vertices;
    }
};

/**
 * The validated triangulation of a source contour inside a rectangle.
 *
 * The triangles contain only references to the input contours. The source
 * contour is the preserved portal boundary and the rectangle contour is the
 * generated outer boundary; no intersection or Steiner vertices are created.
 */
struct ContainedRingResult
{
    std::vector<ContainedRingTriangle> triangles;
    /** Stable identities copied from the selected input contour points. */
    std::size_t sourceBridge{};
    std::size_t rectangleBridge{};
};

/**
 * Builds a positive-extrude ring around a simple source contour.
 *
 * The source contour may be concave because source-profile admission owns the
 * legacy rounded-face validity decision. The rectangle must be convex and
 * must strictly contain every source point. The function validates bridge
 * visibility and the resulting weakly-simple hole triangulation. On false,
 * output is left byte-for-byte unchanged; allocation failures propagate to
 * the caller so its transaction can roll back without trying another route.
 * Triangle winding follows the supplied rectangle contour winding.
 */
bool buildContainedExtrudeRing(
    std::span<const ContainedRingPoint> source,
    std::span<const ContainedRingPoint> rectangle,
    ContainedRingResult& output);

} // namespace sed::level::detail
