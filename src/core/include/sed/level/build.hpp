#pragma once

#include "sed/geometry.hpp"
#include "sed/level.hpp"
#include "sed/level/uv.hpp"

#include <iosfwd>
#include <optional>
#include <string>
#include <vector>

namespace sed::level
{

enum class SectorPrimitive
{
    Cube,
    Cone,
    Cylinder,
    UvSphere,
    IcoSphere,
};

struct SectorShape
{
    std::string name{};
    Sector sector{};
    // Historic shapes.tpl files have pixel-space UVs and no tag. New saved
    // shapes append an explicit coordinate-space marker to the sector settings
    // line so normalized editor UVs are not converted a second time.
    UvCoordinateSpace uvCoordinateSpace{ UvCoordinateSpace::TexturePixels };
};

/** Preview geometry for a surface extrusion before the sector is committed. */
struct SurfaceExtrudePreview
{
    std::vector<TVector> innerLoop{};
    std::vector<TVector> outerLoop{};
};

enum class SurfaceExtrudeError
{
    None,
    InvalidSurface,
    InvalidLength,
    InvalidExpand,
    AlreadyAdjoined,
    InvalidSourceTopology,
    NonPlanarSource,
    NonConvexSource,
    CollapsedExpand,
    InvalidResult,
    InvalidAdjoin,
    AllocationFailure,
};

struct SurfaceExtrudeReport
{
    SurfaceExtrudeError error{SurfaceExtrudeError::None};
    SurfaceRef surface{-1, -1};
};

/** Face-space limits used by UI controls for Surface Extrude expand. */
struct SurfaceExtrudeExpandBounds
{
    double negativeCollapseLimit{};
    double minSpan{};
};

/** Canonical dimensions used by built-in dflt.mat primitive construction. */
inline constexpr UvTextureDimensions DefaultGeneratedSectorTextureDimensions{
    64.0,
    64.0,
};

// Overloads without a resolver use the canonical built-in dflt.mat dimensions.
// Resolver overloads stage generated pixel-space UVs and publish only after
// every Surface has been converted to the Level's declared coordinate space.
void levelCreateCubeSec(Level& level, Sector& sector, double x, double y, double z, const TVector& planeNormal, const TVector& edge) noexcept;
bool levelCreateCubeSec(
    Level& level,
    Sector& sector,
    double x,
    double y,
    double z,
    const TVector& planeNormal,
    const TVector& edge,
    const UvTextureDimensionsResolver& resolveDimensions) noexcept;
Sector& levelCreateCube(Level& level, double x, double y, double z, const TVector& planeNormal, const TVector& edge);
std::optional<std::int32_t> levelCreateCube(
    Level& level,
    double x,
    double y,
    double z,
    const TVector& planeNormal,
    const TVector& edge,
    const UvTextureDimensionsResolver& resolveDimensions) noexcept;

Sector makeSectorPrimitiveShape(ProjectType projectType, SectorPrimitive primitive);
Sector makeSectorPrimitiveShape(ProjectType projectType, SectorPrimitive primitive, int sides);
std::vector<SectorShape> loadSectorShapesTpl(std::istream& input);

// Tagged overloads admit a raw Sector only after all UVs and UV scales have
// been converted from sourceUvCoordinateSpace to the Level's declared space.
// Any invalid tag, missing/invalid texture dimensions, or unrepresentable UV
// leaves the complete Level unchanged.
std::int32_t levelCreateSectorFromShapeBoxAxes(
    Level& level,
    const Sector& shape,
    UvCoordinateSpace sourceUvCoordinateSpace,
    const TVector& minCorner,
    const TVector& maxCorner,
    int localXAxis,
    int localYAxis,
    int localZAxis,
    const UvTextureDimensionsResolver& resolveDimensions) noexcept;
std::int32_t levelCreateSectorFromShapeBox(
    Level& level,
    const Sector& shape,
    UvCoordinateSpace sourceUvCoordinateSpace,
    const TVector& minCorner,
    const TVector& maxCorner,
    const UvTextureDimensionsResolver& resolveDimensions) noexcept;
std::int32_t levelCreateSectorFromShape(
    Level& level,
    const Sector& shape,
    UvCoordinateSpace sourceUvCoordinateSpace,
    const TVector& center,
    double size,
    const UvTextureDimensionsResolver& resolveDimensions) noexcept;

// Compatibility overloads have a same-space contract: the raw Sector's UVs
// must already use level.uvCoordinateSpace. They never guess or silently
// convert an untagged Sector's coordinate space.
std::int32_t levelCreateSectorFromShapeBoxAxes(
    Level& level,
    const Sector& shape,
    const TVector& minCorner,
    const TVector& maxCorner,
    int localXAxis,
    int localYAxis,
    int localZAxis) noexcept;
std::int32_t levelCreateSectorFromShapeBox(Level& level, const Sector& shape, const TVector& minCorner, const TVector& maxCorner) noexcept;
std::int32_t levelCreateSectorFromShape(Level& level, const Sector& shape, const TVector& center, double size) noexcept;
std::int32_t levelCreateSectorPrimitiveBoxAxes(
    Level& level,
    SectorPrimitive primitive,
    const TVector& minCorner,
    const TVector& maxCorner,
    int localXAxis,
    int localYAxis,
    int localZAxis,
    int sides) noexcept;
std::int32_t levelCreateSectorPrimitiveBoxAxes(
    Level& level,
    SectorPrimitive primitive,
    const TVector& minCorner,
    const TVector& maxCorner,
    int localXAxis,
    int localYAxis,
    int localZAxis,
    int sides,
    const UvTextureDimensionsResolver& resolveDimensions) noexcept;
std::int32_t levelCreateSectorPrimitiveBox(Level& level, SectorPrimitive primitive, const TVector& minCorner, const TVector& maxCorner) noexcept;
std::int32_t levelCreateSectorPrimitiveBox(
    Level& level,
    SectorPrimitive primitive,
    const TVector& minCorner,
    const TVector& maxCorner,
    const UvTextureDimensionsResolver& resolveDimensions) noexcept;
std::int32_t levelCreateSectorPrimitiveBox(Level& level, SectorPrimitive primitive, const TVector& minCorner, const TVector& maxCorner, int sides) noexcept;
std::int32_t levelCreateSectorPrimitiveBox(
    Level& level,
    SectorPrimitive primitive,
    const TVector& minCorner,
    const TVector& maxCorner,
    int sides,
    const UvTextureDimensionsResolver& resolveDimensions) noexcept;
std::int32_t levelCreateSectorPrimitive(Level& level, SectorPrimitive primitive, const TVector& center, double size) noexcept;
std::int32_t levelCreateSectorPrimitive(
    Level& level,
    SectorPrimitive primitive,
    const TVector& center,
    double size,
    const UvTextureDimensionsResolver& resolveDimensions) noexcept;
std::int32_t levelCreateSectorPrimitive(Level& level, SectorPrimitive primitive, const TVector& center, double size, int sides) noexcept;
std::int32_t levelCreateSectorPrimitive(
    Level& level,
    SectorPrimitive primitive,
    const TVector& center,
    double size,
    int sides,
    const UvTextureDimensionsResolver& resolveDimensions) noexcept;

std::optional<SurfaceExtrudePreview> levelBuildSurfaceExtrudePreview(
    const Level& level,
    SurfaceRef surface,
    double length,
    double expand,
    SurfaceExtrudeReport* report = nullptr) noexcept;
std::optional<SurfaceExtrudeExpandBounds> levelSurfaceExtrudeExpandBounds(
    const Level& level,
    SurfaceRef surface,
    SurfaceExtrudeReport* report = nullptr) noexcept;
std::optional<Sector> levelBuildSurfaceExtrudeSectorPreview(
    const Level& level,
    SurfaceRef surface,
    double length,
    double expand,
    SurfaceExtrudeReport* report = nullptr) noexcept;
/**
 * Builds a temporary replacement for the selected source face when a negative
 * Surface Extrude expand value creates an inset opening. The returned sector is
 * preview-only geometry; callers should hide the real source surface while
 * drawing it so the user sees the visible face with a rectangular cut hole.
 */
std::optional<Sector> levelBuildSurfaceExtrudeSourceCutPreview(
    const Level& level,
    SurfaceRef surface,
    double expand,
    SurfaceExtrudeReport* report = nullptr) noexcept;
bool levelExtrudeSurface(Level& level, SurfaceRef surface, double by, double expand = 0.0,
    SurfaceExtrudeReport* report = nullptr) noexcept;

} // namespace sed::level
