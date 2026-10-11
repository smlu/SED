#include "sed/level/transform.hpp"

#include "sed/level/geometry.hpp"
#include "sed/text.hpp"
#include "sed/values.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

namespace sed::level
{

namespace
{

constexpr bool isValidIndex(const std::int32_t index, const std::size_t count) noexcept
{
    return index >= 0 && static_cast<std::size_t>(index) < count;
}

bool isValidSectorIndex(const Level& level, const std::int32_t sectorIndex) noexcept
{
    return isValidIndex(sectorIndex, level.sectors.size());
}

bool isValidSurfaceIndex(const Level& level, const std::int32_t sectorIndex, const std::int32_t surfaceIndex) noexcept
{
    if (!isValidSectorIndex(level, sectorIndex))
    {
        return false;
    }

    const Sector& sector = level.sectors[static_cast<std::size_t>(sectorIndex)];
    return isValidIndex(surfaceIndex, sector.surfaces.size());
}

bool isValidSurfaceIndex(const Level& level, const SurfaceRef ref) noexcept
{
    return isValidSurfaceIndex(level, ref.sector, ref.surface);
}

bool isValidVertexIndex(const Level& level, const VertexRef ref) noexcept
{
    if (!isValidSectorIndex(level, ref.sector))
    {
        return false;
    }

    const Sector& sector = level.sectors[static_cast<std::size_t>(ref.sector)];
    return isValidIndex(ref.vertex, sector.vertices.size());
}

bool isValidEdgeIndex(const Level& level, const EdgeRef ref) noexcept
{
    if (!isValidSurfaceIndex(level, ref.sector, ref.surface))
    {
        return false;
    }

    const Sector& sector = level.sectors[static_cast<std::size_t>(ref.sector)];
    const Surface& surface = sector.surfaces[static_cast<std::size_t>(ref.surface)];
    return isValidIndex(ref.edge, surface.vertices.size());
}

bool isValidThingIndex(const Level& level, const std::int32_t thingIndex) noexcept
{
    return isValidIndex(thingIndex, level.things.size());
}

bool isValidLightIndex(const Level& level, const std::int32_t lightIndex) noexcept
{
    return isValidIndex(lightIndex, level.lights.size());
}

bool isValidFrameIndex(const Level& level, const FrameRef ref) noexcept
{
    if (!isValidThingIndex(level, ref.thing))
    {
        return false;
    }

    const Thing& thing = level.things[static_cast<std::size_t>(ref.thing)];
    if (!isValidIndex(ref.value, thing.values.size()))
    {
        return false;
    }

    const TplValue& v = thing.values[static_cast<std::size_t>(ref.value)];
    return v.type == ValueType::frame && sed::text::iequalsAscii(v.name, "frame");
}

bool isFiniteVector(const TVector& value) noexcept
{
    return std::isfinite(value.x) && std::isfinite(value.y) &&
           std::isfinite(value.z);
}

bool isFinitePoint(const double x, const double y, const double z) noexcept
{
    return std::isfinite(x) && std::isfinite(y) && std::isfinite(z);
}

bool isValidRotationInput(const TVector& axis, const double cx, const double cy,
                          const double cz, const double angleDeg) noexcept
{
    return isFiniteVector(axis) && isFinitePoint(cx, cy, cz) &&
           std::isfinite(angleDeg) &&
           axis.lengthSqr() > std::numeric_limits<double>::epsilon();
}

bool isValidScaleInput(const ScaleData& scale) noexcept
{
    if ( !isFinitePoint(scale.cx, scale.cy, scale.cz) ||
         !std::isfinite(scale.sfactor) || !isFiniteVector(scale.vec) )
    {
        return false;
    }

    switch ( scale.how )
    {
    case ScaleHow::xyz:
    case ScaleHow::x:
    case ScaleHow::y:
    case ScaleHow::z:
        return true;
    case ScaleHow::vec:
        return scale.vec.lengthSqr() >
               std::numeric_limits<double>::epsilon();
    default:
        return false;
    }
}

bool isValidAxis(const Axis axis) noexcept
{
    switch ( axis )
    {
    case Axis::x:
    case Axis::y:
    case Axis::z:
        return true;
    default:
        return false;
    }
}

bool isValidPlaneInput(const TVector& normal, const double x, const double y,
                       const double z) noexcept
{
    return isFiniteVector(normal) && isFinitePoint(x, y, z) &&
           normal.lengthSqr() > std::numeric_limits<double>::epsilon();
}

/** Converts legacy vertex coordinates into the vector type used by transform math. */
TVector vertexPosition(const Vertex& vertex) noexcept
{
    return TVector{ vertex.x, vertex.y, vertex.z };
}

/** Builds a vector point from APIs that still expose x/y/z by reference. */
TVector pointFromCoords(const double x, const double y, const double z) noexcept
{
    return TVector{ x, y, z };
}

/** Writes a vector point back into the legacy vertex coordinate fields. */
void setVertexPosition(Vertex& vertex, const TVector& position) noexcept
{
    vertex.x = position.x;
    vertex.y = position.y;
    vertex.z = position.z;
}

/** Writes a vector point back to an x/y/z reference triple. */
void setCoordsFromPoint(double& x, double& y, double& z, const TVector& point) noexcept
{
    x = point.x;
    y = point.y;
    z = point.z;
}

/** Parsed thing frame value with position and orientation split into typed fields. */
struct FramePose
{
    TVector position{};
    double pitch{};
    double yaw{};
    double roll{};
};

/** Parses a template frame value into a FramePose. */
bool loadFramePose(const TplValue& value, FramePose& pose)
{
    return value.tryGetFrame(pose.position.x, pose.position.y, pose.position.z, pose.pitch, pose.yaw, pose.roll);
}

/** Serializes a FramePose back to a template frame value. */
void storeFramePose(TplValue& value, const FramePose& pose)
{
    value.setFrame(pose.position.x, pose.position.y, pose.position.z, pose.pitch, pose.yaw, pose.roll);
}

bool sameFramePose(const FramePose& left, const FramePose& right) noexcept
{
    return left.position.x == right.position.x && left.position.y == right.position.y
        && left.position.z == right.position.z && left.pitch == right.pitch
        && left.yaw == right.yaw && left.roll == right.roll;
}

// A selected Frame batch owns only replacement strings. Neither parsing nor
// formatting may publish a prefix of a multi-owner edit if allocation fails.
class FrameTransformBatch
{
public:
    explicit FrameTransformBatch(Level& level) : level_(level) {}

    template <typename Transform>
    bool stage(const FrameRef ref, const Transform& transform)
    {
        const std::uint64_t key = (static_cast<std::uint64_t>(ref.thing) << 32)
            | static_cast<std::uint32_t>(ref.value);
        const auto found = indices_.find(key);
        TplValue candidate;
        candidate.type = ValueType::frame;
        candidate.stringValue = found == indices_.end()
            ? level_.things[static_cast<std::size_t>(ref.thing)].values[static_cast<std::size_t>(ref.value)].stringValue
            : frames_[found->second].token;
        FramePose pose{};
        if ( !loadFramePose(candidate, pose) )
        {
            return true; // Preserve malformed legacy frames verbatim.
        }
        const FramePose prior = pose;
        transform(pose);
        if ( !isFiniteVector(pose.position) || !isFinitePoint(pose.pitch, pose.yaw, pose.roll) )
        {
            return false;
        }
        if ( sameFramePose(prior, pose) )
        {
            return true;
        }
        storeFramePose(candidate, pose);
        if ( found != indices_.end() )
        {
            // Repeated selected refs intentionally transform the staged result
            // again, preserving the public span's sequential semantics.
            frames_[found->second].token.swap(candidate.stringValue);
            frames_[found->second].current = pose;
        }
        else
        {
            const std::size_t index = frames_.size();
            frames_.push_back({ref, std::move(candidate.stringValue), prior, pose});
            indices_.emplace(key, index);
        }
        return true;
    }

    bool commit() noexcept
    {
        bool changed = false;
        for ( auto& frame : frames_ )
        {
            if ( sameFramePose(frame.original, frame.current) )
            {
                continue;
            }
            changed = true;
            auto& value = level_.things[static_cast<std::size_t>(frame.ref.thing)]
                .values[static_cast<std::size_t>(frame.ref.value)];
            value.stringValue.swap(frame.token);
            value.type = ValueType::frame;
        }
        return changed;
    }

private:
    struct PendingFrame
    {
        FrameRef ref;
        std::string token;
        FramePose original;
        FramePose current;
    };
    Level& level_;
    std::vector<PendingFrame> frames_;
    std::unordered_map<std::uint64_t, std::size_t> indices_;
};

/** Returns the world position of a thing as a vector. */
TVector thingPosition(const Thing& thing) noexcept
{
    return TVector{ thing.x, thing.y, thing.z };
}

/** Writes a vector world position back to a thing. */
void setThingPosition(Thing& thing, const TVector& position) noexcept
{
    thing.x = position.x;
    thing.y = position.y;
    thing.z = position.z;
}

/** Resolves the Sector owner after one or more selected Things move. */
class ThingSectorResolver
{
public:
    ThingSectorResolver(const Level& level, const std::size_t selectedCount) noexcept
        : level_(level), useIndex_(selectedCount > 1u)
    {
    }

    std::int32_t find(const std::int32_t previousSector, const TVector& position) noexcept
    {
        // Strict interior avoids pinning a Thing to its previous side when a
        // transform lands on a shared portal. Boundary and moved-out cases use
        // the global resolver, which preserves lowest-index ownership.
        if ( isValidSectorIndex(level_, previousSector)
            && sectorContainsPointNoSides(
                level_.sectors[static_cast<std::size_t>(previousSector)],
                position.x, position.y, position.z) )
        {
            return previousSector;
        }

        if ( !useIndex_ )
        {
            return levelFindSectorForXYZ(level_, position.x, position.y, position.z);
        }

        if ( !indexAttempted_ )
        {
            indexAttempted_ = true;
            try
            {
                index_.rebuild(level_);
                indexReady_ = true;
            }
            catch ( ... )
            {
                // The transforms are noexcept. If the broad phase cannot be
                // allocated, keep the exact existing fallback behavior.
            }
        }
        if ( indexReady_ )
        {
            return index_.findContainingSector(
                level_, position.x, position.y, position.z);
        }
        return levelFindSectorForXYZ(level_, position.x, position.y, position.z);
    }

private:
    const Level& level_;
    const bool useIndex_;
    bool indexAttempted_{};
    bool indexReady_{};
    SectorContainmentIndex index_{};
};

/** Rotates one point with precomputed rotation data. */
TVector rotatedPoint(const TVector& point, const RotateData& rd) noexcept
{
    TVector rotated = point;
    rotateXYZ(rotated.x, rotated.y, rotated.z, rd);
    return rotated;
}

/** Scales one point using the editor's scale mode data. */
TVector scaledPoint(const TVector& point, const ScaleData& sd) noexcept
{
    TVector scaled = point;
    scaleXYZ(scaled.x, scaled.y, scaled.z, sd);
    return scaled;
}

/** Mirrors one point across an arbitrary plane. */
TVector flippedPointOverPlane(const TVector& point, const TVector& planeNormal, const double cx, const double cy, const double cz) noexcept
{
    TVector flipped = point;
    flipXYZ(flipped.x, flipped.y, flipped.z, planeNormal, cx, cy, cz);
    return flipped;
}

/** Mirrors one point across a principal axis through the given center. */
TVector flippedPointAcrossAxis(const TVector& point, const double cx, const double cy, const double cz, const Axis axis) noexcept
{
    TVector flipped = point;
    switch (axis)
    {
    case Axis::x:
        flipped.x = cx - (flipped.x - cx);
        break;
    case Axis::y:
        flipped.y = cy - (flipped.y - cy);
        break;
    case Axis::z:
        flipped.z = cz - (flipped.z - cz);
        break;
    default:
        break;
    }
    return flipped;
}

/** Applies one node transform, moving a local point into the parent frame. */
TVector transformPointToNodeParent(const HNode& node, const TVector& point) noexcept
{
    const TMatrix43 rotation = TMatrix43::CreateRotate(node.rotation);
    return rotation.TransformVector(point) + node.position;
}

/** Applies the inverse of one node transform, moving a parent-space point into node-local space. */
TVector transformPointFromNodeParent(const HNode& node, const TVector& point) noexcept
{
    const TMatrix43 rotation = TMatrix43::CreateRotate(node.rotation);
    return rotation.TransformVectorOrtho(point - node.position);
}

/** Collects a complete finite child-to-root chain; malformed ancestry returns no branch. */
std::vector<std::int32_t> collectNodeBranchToRoot(const Level& level, const std::int32_t nodeIndex)
{
    std::vector<std::int32_t> branch{};
    if ( !isValidIndex(nodeIndex, level.h3doNodes.size()) )
    {
        return branch;
    }

    std::int32_t current = nodeIndex;
    while ( branch.size() < level.h3doNodes.size() )
    {
        branch.push_back(current);

        const HNode& node = level.h3doNodes[static_cast<std::size_t>(current)];
        if ( !isFiniteVector(node.position) || !isFiniteVector(node.rotation) )
        {
            return {};
        }
        if ( node.parent == -1 )
        {
            return branch;
        }
        if ( !isValidIndex(node.parent, level.h3doNodes.size()) )
        {
            return {};
        }

        current = node.parent;
    }

    // A valid ancestry cannot visit more nodes than the model contains.
    return {};
}

/** Computes the duplicated position of one sector vertex in a caller-supplied basis. */
TVector duplicateVertexPosition(
    const Vertex& vertex,
    const TVector& center,
    const TVector& nx,
    const TVector& ny,
    const TVector& nz,
    const TVector& translation) noexcept
{
    // Step 1: convert the source vertex into the duplicate frame, then move it
    // back into world space with the requested translation.
    const TVector offset = vertexPosition(vertex) - center;
    return center + translation + nx * offset.x + ny * offset.y + nz * offset.z;
}

/** Copies non-geometric sector metadata during duplication. */
void copySectorMetadata(const Sector& source, Sector& target)
{
    target.flags       = source.flags;
    target.ambient     = source.ambient;
    target.extraLight  = source.extraLight;
    target.pointLight  = source.pointLight;
    target.colormap    = source.colormap;
    target.tint        = source.tint;
    target.sound       = source.sound;
    target.soundVolume = source.soundVolume;
    target.thrust      = source.thrust;
    target.collideBox  = source.collideBox;
    target.layer       = source.layer;
    target.mark        = source.mark;
}

/** Copies a surface for a duplicated sector and clears cross-sector adjoin state. */
Surface makeDuplicatedSurface(const Surface& source, const std::int32_t num)
{
    Surface surface = source;
    surface.adjoin.reset();
    surface.nAdj = -1;
    surface.num = num;
    return surface;
}

/** Applies a transform to every vertex in a sector. */
template <typename VertexTransform>
void transformSectorVertices(Sector& sector, const VertexTransform& transform) noexcept
{
    for (Vertex& vertex : sector.vertices)
    {
        transform(vertex);
    }
}

/** Applies a transform to every valid vertex referenced by a surface. */
template <typename VertexTransform>
void transformSurfaceVertices(Sector& sector, const Surface& surface, const VertexTransform& transform) noexcept
{
    // Step 1: walk the surface's vertex references and ignore stale indices so
    // selection transforms remain tolerant of partially invalid imported data.
    for (const std::int32_t vertexIndex : surface.vertices)
    {
        if (!isValidIndex(vertexIndex, sector.vertices.size()))
        {
            continue;
        }

        transform(sector.vertices[static_cast<std::size_t>(vertexIndex)]);
    }
}

/** Applies a transform to the valid vertices on a surface's adjoined mate. */
template <typename VertexTransform>
void transformAdjoinedSurfaceVertices(Level& level, const Surface& surface, const VertexTransform& transform) noexcept
{
    if (!surface.adjoin || !isValidSurfaceIndex(level, *surface.adjoin))
    {
        return;
    }

    Sector& adjoinSector = level.sectors[static_cast<std::size_t>(surface.adjoin->sector)];
    const Surface& adjoinSurface = adjoinSector.surfaces[static_cast<std::size_t>(surface.adjoin->surface)];
    transformSurfaceVertices(adjoinSector, adjoinSurface, transform);
}

/** Preserves visual texel density when geometry is uniformly scaled. */
void scaleSurfaceTexture(Surface& surface, const ScaleData& sd, const bool scaleTextures) noexcept
{
    if (!scaleTextures
        || !std::isfinite(sd.sfactor)
        || sd.sfactor == 0.0
        || !std::isfinite(surface.uScale)
        || !std::isfinite(surface.vScale))
    {
        return;
    }

    const double uScale = static_cast<double>(surface.uScale) / sd.sfactor;
    const double vScale = static_cast<double>(surface.vScale) / sd.sfactor;
    constexpr double maximum = static_cast<double>(
        std::numeric_limits<float>::max());
    if (!std::isfinite(uScale)
        || !std::isfinite(vScale)
        || uScale < -maximum
        || uScale > maximum
        || vScale < -maximum
        || vScale > maximum)
    {
        return;
    }

    surface.uScale = static_cast<float>(uScale);
    surface.vScale = static_cast<float>(vScale);
}

bool prepareSurfaceVisitMap(
    const Level& level,
    std::vector<std::vector<bool>>& visited) noexcept
{
    try
    {
        visited.reserve(level.sectors.size());
        for ( const Sector& sector : level.sectors )
        {
            visited.emplace_back(sector.surfaces.size(), false);
        }
        return true;
    }
    catch (...)
    {
        return false;
    }
}

/** Prepares bounded, allocation-free sector-normal traversal for one operation. */
class SectorRecalcScratch
{
public:
    bool prepare(const Level& level) noexcept
    {
        try
        {
            visited_.assign(level.sectors.size(), false);
            queue_.reserve(level.sectors.size());
            return true;
        }
        catch (...)
        {
            return false;
        }
    }

    void recalc(Level& level, const std::int32_t sectorIndex) noexcept
    {
        if ( !isValidSectorIndex(level, sectorIndex) )
        {
            return;
        }

        std::fill(visited_.begin(), visited_.end(), false);
        queue_.clear();
        visited_[static_cast<std::size_t>(sectorIndex)] = true;
        queue_.push_back(sectorIndex);
        for ( std::size_t next = 0; next < queue_.size(); ++next )
        {
            const std::size_t visitedIndex = static_cast<std::size_t>(queue_[next]);
            Sector& sector = level.sectors[visitedIndex];
            for ( Surface& surface : sector.surfaces )
            {
                surface.normal = surfaceCalcNormalNewell(sector, surface);
                if ( !surface.adjoin || !isValidSectorIndex(level, surface.adjoin->sector) )
                {
                    continue;
                }

                const std::size_t adjacentIndex = static_cast<std::size_t>(surface.adjoin->sector);
                if ( visited_[adjacentIndex] )
                {
                    continue;
                }
                visited_[adjacentIndex] = true;
                queue_.push_back(surface.adjoin->sector);
            }
        }
    }

private:
    std::vector<bool> visited_{};
    std::vector<std::int32_t> queue_{};
};

void scaleSurfaceTextureOnce(
    Level& level,
    std::vector<std::vector<bool>>& visited,
    const SurfaceRef ref,
    const ScaleData& sd,
    const bool scaleTextures) noexcept
{
    if (!scaleTextures
        || !isValidSurfaceIndex(level, ref)
        || static_cast<std::size_t>(ref.sector) >= visited.size()
        || static_cast<std::size_t>(ref.surface)
            >= visited[static_cast<std::size_t>(ref.sector)].size())
    {
        return;
    }

    const std::size_t sectorIndex = static_cast<std::size_t>(ref.sector);
    const std::size_t surfaceIndex = static_cast<std::size_t>(ref.surface);
    if (visited[sectorIndex][surfaceIndex])
    {
        return;
    }

    visited[sectorIndex][surfaceIndex] = true;
    scaleSurfaceTexture(
        level.sectors[static_cast<std::size_t>(ref.sector)]
            .surfaces[static_cast<std::size_t>(ref.surface)],
        sd,
        true);
}

/** Builds a fast lookup table for selected sectors. */
std::vector<bool> buildSelectedSectorMap(const Level& level, const std::span<const std::int32_t> sectorIndices)
{
    std::vector<bool> selected(level.sectors.size(), false);
    for (const std::int32_t sectorIndex : sectorIndices)
    {
        if (isValidSectorIndex(level, sectorIndex))
        {
            selected[static_cast<std::size_t>(sectorIndex)] = true;
        }
    }
    return selected;
}

/** Removes adjoins that would point from flipped sectors to unselected sectors. */
void unadjoinSurfacesOutsideSelection(
    Level& level,
    const std::span<const std::int32_t> sectorIndices,
    const std::vector<bool>& selected) noexcept
{
    // Step 1: remove adjoins that point outside the selected set before the
    // flip changes winding/positions. This mirrors legacy behavior while
    // sharing the bookkeeping between axis and arbitrary-plane flips.
    for (const std::int32_t sectorIndex : sectorIndices)
    {
        if (!isValidSectorIndex(level, sectorIndex))
        {
            continue;
        }

        Sector& sector = level.sectors[static_cast<std::size_t>(sectorIndex)];
        for (std::size_t surfaceIndex = 0; surfaceIndex < sector.surfaces.size(); ++surfaceIndex)
        {
            const Surface& surface = sector.surfaces[surfaceIndex];
            if (!surface.adjoin)
            {
                continue;
            }

            const SurfaceRef ref{ sectorIndex, static_cast<std::int32_t>(surfaceIndex) };
            if (!isValidSectorIndex(level, surface.adjoin->sector))
            {
                (void)levelUnadjoinSurface(level, ref);
                continue;
            }

            if (!selected[static_cast<std::size_t>(surface.adjoin->sector)])
            {
                (void)levelUnadjoinSurface(level, ref);
            }
        }
    }
}

/** Translates an unmarked vertex and marks it so shared selections move once. */
void translateVertexOnce(Vertex& v, const double dx, const double dy, const double dz) noexcept
{
    if (v.mark != 0)
    {
        return;
    }

    setVertexPosition(v, vertexPosition(v) + TVector{ dx, dy, dz });
    v.mark = 1;
}

/** Rotates an unmarked vertex and marks it so shared selections rotate once. */
void rotateVertexOnce(Vertex& v, const RotateData& rd) noexcept
{
    if (v.mark != 0)
    {
        return;
    }

    rotateXYZ(v.x, v.y, v.z, rd);
    v.mark = 1;
}

/** Scales an unmarked vertex and marks it so shared selections scale once. */
void scaleVertexOnce(Vertex& v, const ScaleData& sd) noexcept
{
    if (v.mark != 0)
    {
        return;
    }

    switch (sd.how)
    {
    case ScaleHow::xyz:
        v.x = sd.cx + sd.sfactor * (v.x - sd.cx);
        v.y = sd.cy + sd.sfactor * (v.y - sd.cy);
        v.z = sd.cz + sd.sfactor * (v.z - sd.cz);
        break;
    case ScaleHow::x:
        v.x = sd.cx + sd.sfactor * (v.x - sd.cx);
        break;
    case ScaleHow::y:
        v.y = sd.cy + sd.sfactor * (v.y - sd.cy);
        break;
    case ScaleHow::z:
        v.z = sd.cz + sd.sfactor * (v.z - sd.cz);
        break;
    case ScaleHow::vec:
    default:
        break;
    }

    if (sd.how == ScaleHow::vec)
    {
        const TVector origin{ sd.cx, sd.cy, sd.cz };
        TVector axis = sd.vec;
        axis.Normalize();
        const double distanceAlongAxis = axis.dot(vertexPosition(v) - origin);
        const double scaledDistance = (sd.sfactor - 1.0) * distanceAlongAxis;
        setVertexPosition(v, vertexPosition(v) + axis * scaledDistance);
    }

    v.mark = 1;
}

/** Mirrors an unmarked vertex across one principal axis and marks it. */
void flipVertexOnce(Vertex& v, const double cx, const double cy, const double cz, const Axis axis) noexcept
{
    if (v.mark != 0)
    {
        return;
    }

    setVertexPosition(v, flippedPointAcrossAxis(vertexPosition(v), cx, cy, cz, axis));
    v.mark = 1;
}

/** Mirrors an unmarked vertex across an arbitrary plane and marks it. */
void flipVertexOverPlaneOnce(Vertex& v, const TVector& planeNormal, const double cx, const double cy, const double cz) noexcept
{
    if (v.mark != 0)
    {
        return;
    }

    setVertexPosition(v, flippedPointOverPlane(vertexPosition(v), planeNormal, cx, cy, cz));
    v.mark = 1;
}

/** Reverses surface winding, recalculates its normal, and updates the texture flip flag. */
void flipSurface(Sector& sector, Surface& surface) noexcept
{
    std::reverse(surface.vertices.begin(), surface.vertices.end());
    std::reverse(surface.texVertices.begin(), surface.texVertices.end());

    surface.normal = surfaceCalcNormalNewell(sector, surface);
    if (surfaceIsTextureFlipped(sector, surface))
    {
        surface.faceFlags |= FF_SF_FLIP;
    }
    else
    {
        surface.faceFlags &= ~FF_SF_FLIP;
    }
}

/** Preserves the legacy one-turn angle clamp used by PYR rotations. */
double clampAngle(const double angle) noexcept
{
    double result = angle;
    if (result > 359.0)
    {
        result = result - 360.0;
    }
    else if (result < -359.0)
    {
        result = result + 360.0;
    }
    return result;
}

} // namespace

/** Clears the one-shot marks used to prevent shared selected vertices from moving twice. */
void levelClearVertexMarks(Level& level) noexcept
{
    for (Sector& sector : level.sectors)
    {
        for (Vertex& v : sector.vertices)
        {
            v.mark = 0;
        }
    }
}

/** Recalculates derived geometry while preserving authored per-corner texture data. */
bool levelRecalcSectorChecked(Level& level, const std::int32_t sectorIndex) noexcept
{
    if ( !isValidSectorIndex(level, sectorIndex) )
    {
        return false;
    }

    SectorRecalcScratch scratch{};
    if ( !scratch.prepare(level) )
    {
        return false;
    }
    scratch.recalc(level, sectorIndex);
    return true;
}

void levelRecalcSector(Level& level, const std::int32_t sectorIndex) noexcept
{
    (void)levelRecalcSectorChecked(level, sectorIndex);
}

/** Translates selected sectors plus the vertices on their adjoined mate surfaces. */
bool levelTranslateSectorsChecked(
    Level& level,
    const std::span<const std::int32_t> sectorIndices,
    const double dx,
    const double dy,
    const double dz) noexcept
{
    if ( !isFinitePoint(dx, dy, dz) )
    {
        return false;
    }

    SectorRecalcScratch scratch{};
    if ( !scratch.prepare(level) )
    {
        return false;
    }

    // Every allocation is complete before this operation clears visit marks
    // or publishes transformed coordinates and normals.
    levelClearVertexMarks(level);
    const auto translate = [dx, dy, dz](Vertex& vertex) noexcept
    {
        translateVertexOnce(vertex, dx, dy, dz);
    };
    for (const std::int32_t sectorIndex : sectorIndices)
    {
        if (!isValidSectorIndex(level, sectorIndex))
        {
            continue;
        }
        Sector& sector = level.sectors[static_cast<std::size_t>(sectorIndex)];
        transformSectorVertices(sector, translate);
        for (const Surface& surface : sector.surfaces)
        {
            transformAdjoinedSurfaceVertices(level, surface, translate);
        }
        scratch.recalc(level, sectorIndex);
    }
    return true;
}

void levelTranslateSectors(
    Level& level, const std::span<const std::int32_t> sectorIndices,
    const double dx, const double dy, const double dz) noexcept
{
    (void)levelTranslateSectorsChecked(level, sectorIndices, dx, dy, dz);
}

/** Translates selected surfaces and their valid adjoined mate surfaces. */
bool levelTranslateSurfacesChecked(
    Level& level,
    const std::span<const SurfaceRef> surfaceIndices,
    const double dx,
    const double dy,
    const double dz) noexcept
{
    if ( !isFinitePoint(dx, dy, dz) )
    {
        return false;
    }
    SectorRecalcScratch scratch{};
    if ( !scratch.prepare(level) )
    {
        return false;
    }

    levelClearVertexMarks(level);
    const auto translate = [dx, dy, dz](Vertex& vertex) noexcept
    {
        translateVertexOnce(vertex, dx, dy, dz);
    };
    for (const SurfaceRef ref : surfaceIndices)
    {
        if (!isValidSurfaceIndex(level, ref))
        {
            continue;
        }
        Sector& sector = level.sectors[static_cast<std::size_t>(ref.sector)];
        const Surface& surface = sector.surfaces[static_cast<std::size_t>(ref.surface)];
        transformSurfaceVertices(sector, surface, translate);
        transformAdjoinedSurfaceVertices(level, surface, translate);
        scratch.recalc(level, ref.sector);
    }
    return true;
}

void levelTranslateSurfaces(
    Level& level, const std::span<const SurfaceRef> surfaceIndices,
    const double dx, const double dy, const double dz) noexcept
{
    (void)levelTranslateSurfacesChecked(level, surfaceIndices, dx, dy, dz);
}

/** Translates both vertices of each selected edge. */
bool levelTranslateEdgesChecked(
    Level& level,
    const std::span<const EdgeRef> edgeIndices,
    const double dx,
    const double dy,
    const double dz) noexcept
{
    if ( !isFinitePoint(dx, dy, dz) )
    {
        return false;
    }
    SectorRecalcScratch scratch{};
    if ( !scratch.prepare(level) )
    {
        return false;
    }

    levelClearVertexMarks(level);
    for (const EdgeRef ref : edgeIndices)
    {
        if (!isValidEdgeIndex(level, ref))
        {
            continue;
        }
        Sector& sector = level.sectors[static_cast<std::size_t>(ref.sector)];
        const Surface& surface = sector.surfaces[static_cast<std::size_t>(ref.surface)];
        const std::int32_t edgeCount = static_cast<std::int32_t>(surface.vertices.size());
        const std::int32_t vx0 = surface.vertices[static_cast<std::size_t>(ref.edge)];
        const std::int32_t vx1 = surface.vertices[static_cast<std::size_t>(nextIndex(ref.edge, edgeCount))];
        if (isValidIndex(vx0, sector.vertices.size()))
        {
            translateVertexOnce(sector.vertices[static_cast<std::size_t>(vx0)], dx, dy, dz);
        }
        if (isValidIndex(vx1, sector.vertices.size()))
        {
            translateVertexOnce(sector.vertices[static_cast<std::size_t>(vx1)], dx, dy, dz);
        }
        scratch.recalc(level, ref.sector);
    }
    return true;
}

void levelTranslateEdges(
    Level& level, const std::span<const EdgeRef> edgeIndices,
    const double dx, const double dy, const double dz) noexcept
{
    (void)levelTranslateEdgesChecked(level, edgeIndices, dx, dy, dz);
}

/** Translates individually selected vertices. */
bool levelTranslateVerticesChecked(
    Level& level,
    const std::span<const VertexRef> vertexIndices,
    const double dx,
    const double dy,
    const double dz) noexcept
{
    if ( !isFinitePoint(dx, dy, dz) )
    {
        return false;
    }
    SectorRecalcScratch scratch{};
    if ( !scratch.prepare(level) )
    {
        return false;
    }

    levelClearVertexMarks(level);
    for (const VertexRef ref : vertexIndices)
    {
        if (!isValidVertexIndex(level, ref))
        {
            continue;
        }
        Sector& sector = level.sectors[static_cast<std::size_t>(ref.sector)];
        translateVertexOnce(sector.vertices[static_cast<std::size_t>(ref.vertex)], dx, dy, dz);
        scratch.recalc(level, ref.sector);
    }
    return true;
}

void levelTranslateVertices(
    Level& level, const std::span<const VertexRef> vertexIndices,
    const double dx, const double dy, const double dz) noexcept
{
    (void)levelTranslateVerticesChecked(level, vertexIndices, dx, dy, dz);
}

/** Translates selected things and optionally translates all frame values attached to them. */
bool levelTranslateThings(
    Level& level,
    const std::span<const std::int32_t> thingIndices,
    const double dx,
    const double dy,
    const double dz,
    const bool moveFrames,
    const bool updateSector)
{
    if ( !isFinitePoint(dx, dy, dz)
        || (!updateSector && dx == 0.0 && dy == 0.0 && dz == 0.0) )
    {
        return false;
    }
    struct PendingThing
    {
        std::int32_t index;
        TVector position;
        std::int32_t sector;
    };
    const TVector translation{dx, dy, dz};
    FrameTransformBatch frames(level);
    std::vector<PendingThing> things;
    std::unordered_map<std::int32_t, std::size_t> indices;
    for ( const std::int32_t thingIndex : thingIndices )
    {
        if ( !isValidThingIndex(level, thingIndex) )
        {
            continue;
        }
        const Thing& source = level.things[static_cast<std::size_t>(thingIndex)];
        const auto found = indices.find(thingIndex);
        std::size_t index;
        if ( found == indices.end() )
        {
            index = things.size();
            things.push_back({thingIndex, thingPosition(source), source.sector});
            indices.emplace(thingIndex, index);
        }
        else
        {
            index = found->second;
        }
        auto& pending = things[index];
        pending.position = pending.position + translation;
        if ( !isFiniteVector(pending.position) )
        {
            return false;
        }
        if ( updateSector )
        {
            pending.sector = levelFindSectorForXYZ(level, pending.position.x,
                pending.position.y, pending.position.z);
        }
        if ( moveFrames )
        {
            for ( std::size_t valueIndex = 0; valueIndex < source.values.size(); ++valueIndex )
            {
                if ( source.values[valueIndex].type != ValueType::frame )
                {
                    continue;
                }
                if ( !frames.stage({thingIndex, static_cast<std::int32_t>(valueIndex)},
                    [&translation](FramePose& pose) { pose.position = pose.position + translation; }) )
                {
                    return false;
                }
            }
        }
    }
    // All fallible work is complete. String swaps and scalar writes cannot
    // allocate, so no caller can observe a partially published batch.
    bool changed = frames.commit();
    for ( const auto& pending : things )
    {
        Thing& thing = level.things[static_cast<std::size_t>(pending.index)];
        if ( thing.x == pending.position.x && thing.y == pending.position.y
            && thing.z == pending.position.z && thing.sector == pending.sector )
        {
            // Numeric no-ops retain exact authored bits, including signed zero.
            continue;
        }
        changed = true;
        setThingPosition(thing, pending.position);
        thing.sector = pending.sector;
    }
    return changed;
}

/** Translates explicitly selected thing frame values. */
bool levelTranslateFrames(Level& level, const std::span<const FrameRef> frameIndices, const double dx, const double dy, const double dz)
{
    if ( !isFinitePoint(dx, dy, dz) || (dx == 0.0 && dy == 0.0 && dz == 0.0) )
    {
        return false;
    }

    const TVector translation{ dx, dy, dz };

    FrameTransformBatch batch(level);
    for ( const FrameRef ref : frameIndices )
    {
        if ( !isValidFrameIndex(level, ref) )
        {
            continue;
        }
        if ( !batch.stage(ref, [&](FramePose& pose)
            {
                pose.position = pose.position + translation;
            }) )
        {
            return false;
        }
    }
    return batch.commit();
}

/** Translates selected level lights. */
void levelTranslateLights(Level& level, const std::span<const std::int32_t> lightIndices, const double dx, const double dy, const double dz) noexcept
{
    if ( !isFinitePoint(dx, dy, dz) )
    {
        return;
    }

    for (const std::int32_t lightIndex : lightIndices)
    {
        if (!isValidLightIndex(level, lightIndex))
        {
            continue;
        }

        SedLight& light = level.lights[static_cast<std::size_t>(lightIndex)];
        light.position = light.position + TVector{ dx, dy, dz };
    }
}

/** Rotates selected sectors plus the vertices on their adjoined mate surfaces. */
bool levelRotateSectorsChecked(
    Level& level,
    const std::span<const std::int32_t> sectorIndices,
    const TVector& axis,
    const double cx,
    const double cy,
    const double cz,
    const double angleDeg) noexcept
{
    if ( !isValidRotationInput(axis, cx, cy, cz, angleDeg) )
    {
        return false;
    }

    SectorRecalcScratch scratch{};
    if ( !scratch.prepare(level) )
    {
        return false;
    }

    RotateData rd{};
    prepareRotateData(rd, axis, cx, cy, cz, angleDeg);

    levelClearVertexMarks(level);
    const auto rotate = [&rd](Vertex& vertex) noexcept {
        rotateVertexOnce(vertex, rd);
    };

    for (const std::int32_t sectorIndex : sectorIndices)
    {
        if (!isValidSectorIndex(level, sectorIndex))
        {
            continue;
        }

        Sector& sector = level.sectors[static_cast<std::size_t>(sectorIndex)];
        transformSectorVertices(sector, rotate);

        for (const Surface& surface : sector.surfaces)
        {
            transformAdjoinedSurfaceVertices(level, surface, rotate);
        }

        scratch.recalc(level, sectorIndex);
    }
    return true;
}

/** Rotates selected surfaces and their valid adjoined mate surfaces. */
bool levelRotateSurfacesChecked(
    Level& level,
    const std::span<const SurfaceRef> surfaceIndices,
    const TVector& axis,
    const double cx,
    const double cy,
    const double cz,
    const double angleDeg) noexcept
{
    if ( !isValidRotationInput(axis, cx, cy, cz, angleDeg) )
    {
        return false;
    }

    SectorRecalcScratch scratch{};
    if ( !scratch.prepare(level) )
    {
        return false;
    }

    RotateData rd{};
    prepareRotateData(rd, axis, cx, cy, cz, angleDeg);

    levelClearVertexMarks(level);
    const auto rotate = [&rd](Vertex& vertex) noexcept {
        rotateVertexOnce(vertex, rd);
    };

    for (const SurfaceRef ref : surfaceIndices)
    {
        if (!isValidSurfaceIndex(level, ref))
        {
            continue;
        }

        Sector& sector = level.sectors[static_cast<std::size_t>(ref.sector)];
        const Surface& surface = sector.surfaces[static_cast<std::size_t>(ref.surface)];

        transformSurfaceVertices(sector, surface, rotate);
        transformAdjoinedSurfaceVertices(level, surface, rotate);

        scratch.recalc(level, ref.sector);
    }
    return true;
}

void levelRotateSectors(
    Level& level,
    const std::span<const std::int32_t> sectorIndices,
    const TVector& axis,
    const double cx,
    const double cy,
    const double cz,
    const double angleDeg) noexcept
{
    (void)levelRotateSectorsChecked(level, sectorIndices, axis, cx, cy, cz, angleDeg);
}

void levelRotateSurfaces(
    Level& level,
    const std::span<const SurfaceRef> surfaceIndices,
    const TVector& axis,
    const double cx,
    const double cy,
    const double cz,
    const double angleDeg) noexcept
{
    (void)levelRotateSurfacesChecked(level, surfaceIndices, axis, cx, cy, cz, angleDeg);
}

/** Rotates both vertices of each selected edge around a world-space pivot. */
bool levelRotateEdgesChecked(
    Level& level,
    const std::span<const EdgeRef> edgeIndices,
    const TVector& axis,
    const double cx,
    const double cy,
    const double cz,
    const double angleDeg) noexcept
{
    if ( !isValidRotationInput(axis, cx, cy, cz, angleDeg) )
    {
        return false;
    }

    SectorRecalcScratch scratch{};
    if ( !scratch.prepare(level) )
    {
        return false;
    }

    RotateData rd{};
    prepareRotateData(rd, axis, cx, cy, cz, angleDeg);

    levelClearVertexMarks(level);
    for (const EdgeRef ref : edgeIndices)
    {
        if (!isValidEdgeIndex(level, ref))
        {
            continue;
        }

        Sector& sector = level.sectors[static_cast<std::size_t>(ref.sector)];
        const Surface& surface = sector.surfaces[static_cast<std::size_t>(ref.surface)];
        const std::int32_t edgeCount = static_cast<std::int32_t>(surface.vertices.size());
        const std::int32_t vx0 = surface.vertices[static_cast<std::size_t>(ref.edge)];
        const std::int32_t vx1 = surface.vertices[
            static_cast<std::size_t>(nextIndex(ref.edge, edgeCount))];

        if (isValidIndex(vx0, sector.vertices.size()))
        {
            rotateVertexOnce(sector.vertices[static_cast<std::size_t>(vx0)], rd);
        }
        if (isValidIndex(vx1, sector.vertices.size()))
        {
            rotateVertexOnce(sector.vertices[static_cast<std::size_t>(vx1)], rd);
        }

        scratch.recalc(level, ref.sector);
    }
    return true;
}

void levelRotateEdges(
    Level& level,
    const std::span<const EdgeRef> edgeIndices,
    const TVector& axis,
    const double cx,
    const double cy,
    const double cz,
    const double angleDeg) noexcept
{
    (void)levelRotateEdgesChecked(
        level, edgeIndices, axis, cx, cy, cz, angleDeg);
}

/** Rotates editor pitch/yaw/roll values around a principal-axis-style vector. */
void rotatePYR(double& pitch, double& yaw, double& roll, const TVector& axis, const double angleDeg) noexcept
{
    if ( !std::isfinite(pitch) || !std::isfinite(yaw) ||
         !std::isfinite(roll) || !isFiniteVector(axis) ||
         !std::isfinite(angleDeg) )
    {
        return;
    }

    TVector normalizedAxis = axis;
    if ( normalizedAxis.Normalize() == 0.0 )
    {
        return;
    }

    pitch = clampAngle(pitch + normalizedAxis.x * angleDeg);
    yaw = clampAngle(yaw + normalizedAxis.z * angleDeg);
    roll = clampAngle(roll + normalizedAxis.y * angleDeg);
}

/** Rotates selected things and optionally rotates their pitch/yaw/roll orientation. */
void levelRotateThings(
    Level& level,
    const std::span<const std::int32_t> thingIndices,
    const TVector& axis,
    const double cx,
    const double cy,
    const double cz,
    const double angleDeg,
    const bool rotatePYRValues) noexcept
{
    if ( !isValidRotationInput(axis, cx, cy, cz, angleDeg) )
    {
        return;
    }

    RotateData rd{};
    prepareRotateData(rd, axis, cx, cy, cz, angleDeg);
    ThingSectorResolver sectorResolver(level, thingIndices.size());

    for (const std::int32_t thingIndex : thingIndices)
    {
        if (!isValidThingIndex(level, thingIndex))
        {
            continue;
        }

        Thing& thing = level.things[static_cast<std::size_t>(thingIndex)];
        if (rotatePYRValues)
        {
            rotatePYR(thing.pch, thing.yaw, thing.rol, axis, -angleDeg);
        }
        const TVector position = rotatedPoint(thingPosition(thing), rd);
        const std::int32_t sector = sectorResolver.find(thing.sector, position);
        setThingPosition(thing, position);
        thing.sector = sector;
    }
}

/** Rotates selected lights around the requested axis and center. */
void levelRotateLights(
    Level& level,
    const std::span<const std::int32_t> lightIndices,
    const TVector& axis,
    const double cx,
    const double cy,
    const double cz,
    const double angleDeg) noexcept
{
    if ( !isValidRotationInput(axis, cx, cy, cz, angleDeg) )
    {
        return;
    }

    RotateData rd{};
    prepareRotateData(rd, axis, cx, cy, cz, angleDeg);

    for (const std::int32_t lightIndex : lightIndices)
    {
        if (!isValidLightIndex(level, lightIndex))
        {
            continue;
        }

        SedLight& light = level.lights[static_cast<std::size_t>(lightIndex)];
        light.position = rotatedPoint(light.position, rd);
    }
}

/** Rotates selected frame positions and optionally rotates their pitch/yaw/roll orientation. */
bool levelRotateFrames(
    Level& level,
    const std::span<const FrameRef> frameIndices,
    const TVector& axis,
    const double cx,
    const double cy,
    const double cz,
    const double angleDeg,
    const bool rotatePYRValues)
{
    if ( !isValidRotationInput(axis, cx, cy, cz, angleDeg) || angleDeg == 0.0 )
    {
        return false;
    }

    RotateData rd{};
    prepareRotateData(rd, axis, cx, cy, cz, angleDeg);

    FrameTransformBatch batch(level);
    for ( const FrameRef ref : frameIndices )
    {
        if ( !isValidFrameIndex(level, ref) )
        {
            continue;
        }
        if ( !batch.stage(ref, [&](FramePose& pose)
            {
                pose.position = rotatedPoint(pose.position, rd);
                if ( rotatePYRValues )
                {
                    rotatePYR(pose.pitch, pose.yaw, pose.roll, axis, -angleDeg);
                }
            }) )
        {
            return false;
        }
    }
    return batch.commit();
}

/** Applies a 3DO node's transform chain from child space up to world/root space. */
void levelOffsetByNode(const Level& level, const std::int32_t nodeIndex, double& x, double& y, double& z) noexcept
{
    if ( !isValidIndex(nodeIndex, level.h3doNodes.size()) || !isFinitePoint(x, y, z) )
    {
        return;
    }

    TVector point = pointFromCoords(x, y, z);
    std::int32_t current = nodeIndex;

    // Step 1: walk from the child node toward the root, applying each local
    // rotation and translation in parent order.
    for ( std::size_t visited = 0; visited < level.h3doNodes.size(); ++visited )
    {
        const HNode& node = level.h3doNodes[static_cast<std::size_t>(current)];
        if ( !isFiniteVector(node.position) || !isFiniteVector(node.rotation) )
        {
            return;
        }
        point = transformPointToNodeParent(node, point);
        if ( !isFiniteVector(point) )
        {
            return;
        }
        if ( node.parent == -1 )
        {
            setCoordsFromPoint(x, y, z, point);
            return;
        }
        if ( !isValidIndex(node.parent, level.h3doNodes.size()) )
        {
            return;
        }

        current = node.parent;
    }

    // A cyclic chain never publishes its partially transformed point.
}

/** Applies the inverse 3DO node transform chain from world/root space back to child space. */
void levelUnoffsetByNode(const Level& level, const std::int32_t nodeIndex, double& x, double& y, double& z) noexcept
{
    if ( !isValidIndex(nodeIndex, level.h3doNodes.size()) || !isFinitePoint(x, y, z) )
    {
        return;
    }

    const std::vector<std::int32_t> branch = collectNodeBranchToRoot(level, nodeIndex);
    if ( branch.empty() )
    {
        return;
    }
    TVector point = pointFromCoords(x, y, z);

    // Step 1: walk from root to child, undoing each parent transform in the
    // reverse order of levelOffsetByNode().
    for ( std::size_t i = branch.size(); i-- > 0; )
    {
        const std::int32_t idx = branch[i];
        const HNode& node = level.h3doNodes[static_cast<std::size_t>(idx)];
        point = transformPointFromNodeParent(node, point);
        if ( !isFiniteVector(point) )
        {
            return;
        }
    }

    setCoordsFromPoint(x, y, z, point);
}

/** Scales selected sectors plus adjoined mate vertices, optionally preserving texture density. */
bool levelScaleSectorsChecked(
    Level& level,
    const std::span<const std::int32_t> sectorIndices,
    const ScaleData& sd,
    const bool scaleTextures) noexcept
{
    if ( !isValidScaleInput(sd) )
    {
        return false;
    }

    SectorRecalcScratch scratch{};
    if ( !scratch.prepare(level) )
    {
        return false;
    }

    std::vector<std::vector<bool>> scaledSurfaces{};
    if ( scaleTextures && !prepareSurfaceVisitMap(level, scaledSurfaces) )
    {
        return false;
    }

    levelClearVertexMarks(level);
    const auto scale = [&sd](Vertex& vertex) noexcept {
        scaleVertexOnce(vertex, sd);
    };

    for (const std::int32_t sectorIndex : sectorIndices)
    {
        if (!isValidSectorIndex(level, sectorIndex))
        {
            continue;
        }

        Sector& sector = level.sectors[static_cast<std::size_t>(sectorIndex)];
        transformSectorVertices(sector, scale);

        for (std::size_t surfaceIndex = 0;
             surfaceIndex < sector.surfaces.size();
             ++surfaceIndex)
        {
            Surface& surface = sector.surfaces[surfaceIndex];
            scaleSurfaceTextureOnce(
                level,
                scaledSurfaces,
                SurfaceRef{
                    sectorIndex,
                    static_cast<std::int32_t>(surfaceIndex) },
                sd,
                scaleTextures);

            if (!surface.adjoin || !isValidSurfaceIndex(level, *surface.adjoin))
            {
                continue;
            }

            Sector& adjoinSector = level.sectors[static_cast<std::size_t>(surface.adjoin->sector)];
            Surface& adjoinSurface = adjoinSector.surfaces[static_cast<std::size_t>(surface.adjoin->surface)];

            scaleSurfaceTextureOnce(
                level,
                scaledSurfaces,
                *surface.adjoin,
                sd,
                scaleTextures);
            transformSurfaceVertices(adjoinSector, adjoinSurface, scale);
        }

        scratch.recalc(level, sectorIndex);
    }
    return true;
}

/** Scales selected surfaces plus valid adjoined mate surfaces. */
bool levelScaleSurfacesChecked(
    Level& level,
    const std::span<const SurfaceRef> surfaceIndices,
    const ScaleData& sd,
    const bool scaleTextures) noexcept
{
    if ( !isValidScaleInput(sd) )
    {
        return false;
    }

    SectorRecalcScratch scratch{};
    if ( !scratch.prepare(level) )
    {
        return false;
    }

    std::vector<std::vector<bool>> scaledSurfaces{};
    if ( scaleTextures && !prepareSurfaceVisitMap(level, scaledSurfaces) )
    {
        return false;
    }

    levelClearVertexMarks(level);
    const auto scale = [&sd](Vertex& vertex) noexcept {
        scaleVertexOnce(vertex, sd);
    };

    for (const SurfaceRef ref : surfaceIndices)
    {
        if (!isValidSurfaceIndex(level, ref))
        {
            continue;
        }

        Sector& sector = level.sectors[static_cast<std::size_t>(ref.sector)];
        Surface& surface = sector.surfaces[static_cast<std::size_t>(ref.surface)];

        transformSurfaceVertices(sector, surface, scale);
        scaleSurfaceTextureOnce(
            level, scaledSurfaces, ref, sd, scaleTextures);

        if (surface.adjoin && isValidSurfaceIndex(level, *surface.adjoin))
        {
            Sector& adjoinSector = level.sectors[static_cast<std::size_t>(surface.adjoin->sector)];
            Surface& adjoinSurface = adjoinSector.surfaces[static_cast<std::size_t>(surface.adjoin->surface)];

            scaleSurfaceTextureOnce(
                level,
                scaledSurfaces,
                *surface.adjoin,
                sd,
                scaleTextures);
            transformSurfaceVertices(adjoinSector, adjoinSurface, scale);
        }

        scratch.recalc(level, ref.sector);
    }
    return true;
}

void levelScaleSectors(
    Level& level,
    const std::span<const std::int32_t> sectorIndices,
    const ScaleData& sd,
    const bool scaleTextures) noexcept
{
    (void)levelScaleSectorsChecked(level, sectorIndices, sd, scaleTextures);
}

void levelScaleSurfaces(
    Level& level,
    const std::span<const SurfaceRef> surfaceIndices,
    const ScaleData& sd,
    const bool scaleTextures) noexcept
{
    (void)levelScaleSurfacesChecked(level, surfaceIndices, sd, scaleTextures);
}

/** Scales selected things. */
void levelScaleThings(Level& level, const std::span<const std::int32_t> thingIndices, const ScaleData& sd) noexcept
{
    if ( !isValidScaleInput(sd) )
    {
        return;
    }

    ThingSectorResolver sectorResolver(level, thingIndices.size());
    for (const std::int32_t thingIndex : thingIndices)
    {
        if (!isValidThingIndex(level, thingIndex))
        {
            continue;
        }

        Thing& thing = level.things[static_cast<std::size_t>(thingIndex)];
        const TVector position = scaledPoint(thingPosition(thing), sd);
        const std::int32_t sector = sectorResolver.find(thing.sector, position);
        setThingPosition(thing, position);
        thing.sector = sector;
    }
}

/** Scales selected light positions. */
void levelScaleLights(Level& level, const std::span<const std::int32_t> lightIndices, const ScaleData& sd) noexcept
{
    if ( !isValidScaleInput(sd) )
    {
        return;
    }

    for (const std::int32_t lightIndex : lightIndices)
    {
        if (!isValidLightIndex(level, lightIndex))
        {
            continue;
        }

        SedLight& light = level.lights[static_cast<std::size_t>(lightIndex)];
        light.position = scaledPoint(light.position, sd);
    }
}

/** Scales selected frame positions without changing frame orientation. */
bool levelScaleFrames(Level& level, const std::span<const FrameRef> frameIndices, const ScaleData& sd)
{
    if ( !isValidScaleInput(sd) || sd.sfactor == 1.0 )
    {
        return false;
    }

    FrameTransformBatch batch(level);
    for ( const FrameRef ref : frameIndices )
    {
        if ( !isValidFrameIndex(level, ref) )
        {
            continue;
        }
        if ( !batch.stage(ref, [&](FramePose& pose)
            {
                pose.position = scaledPoint(pose.position, sd);
            }) )
        {
            return false;
        }
    }
    return batch.commit();
}

/** Mirrors selected things across one principal axis. */
void levelFlipThings(Level& level, const std::span<const std::int32_t> thingIndices, const double cx, const double cy, const double cz, const Axis axis) noexcept
{
    if ( !isFinitePoint(cx, cy, cz) || !isValidAxis(axis) )
    {
        return;
    }

    ThingSectorResolver sectorResolver(level, thingIndices.size());
    for (const std::int32_t thingIndex : thingIndices)
    {
        if (!isValidThingIndex(level, thingIndex))
        {
            continue;
        }

        Thing& thing = level.things[static_cast<std::size_t>(thingIndex)];
        const TVector position = flippedPointAcrossAxis(thingPosition(thing), cx, cy, cz, axis);
        const std::int32_t sector = sectorResolver.find(thing.sector, position);
        setThingPosition(thing, position);
        thing.sector = sector;
    }
}

/** Mirrors selected things across an arbitrary plane. */
void levelFlipThingsOverPlane(
    Level& level,
    const std::span<const std::int32_t> thingIndices,
    const TVector& planeNormal,
    const double cx,
    const double cy,
    const double cz) noexcept
{
    if ( !isValidPlaneInput(planeNormal, cx, cy, cz) )
    {
        return;
    }

    ThingSectorResolver sectorResolver(level, thingIndices.size());
    for (const std::int32_t thingIndex : thingIndices)
    {
        if (!isValidThingIndex(level, thingIndex))
        {
            continue;
        }

        Thing& thing = level.things[static_cast<std::size_t>(thingIndex)];
        const TVector position = flippedPointOverPlane(thingPosition(thing), planeNormal, cx, cy, cz);
        const std::int32_t sector = sectorResolver.find(thing.sector, position);
        setThingPosition(thing, position);
        thing.sector = sector;
    }
}

/** Mirrors selected lights across an arbitrary plane. */
void levelFlipLightsOverPlane(
    Level& level,
    const std::span<const std::int32_t> lightIndices,
    const TVector& planeNormal,
    const double cx,
    const double cy,
    const double cz) noexcept
{
    if ( !isValidPlaneInput(planeNormal, cx, cy, cz) )
    {
        return;
    }

    for (const std::int32_t lightIndex : lightIndices)
    {
        if (!isValidLightIndex(level, lightIndex))
        {
            continue;
        }

        SedLight& light = level.lights[static_cast<std::size_t>(lightIndex)];
        light.position = flippedPointOverPlane(light.position, planeNormal, cx, cy, cz);
    }
}

/** Mirrors selected frame positions across an arbitrary plane. */
bool levelFlipFramesOverPlane(
    Level& level,
    const std::span<const FrameRef> frameIndices,
    const TVector& planeNormal,
    const double cx,
    const double cy,
    const double cz)
{
    if ( !isValidPlaneInput(planeNormal, cx, cy, cz) )
    {
        return false;
    }

    FrameTransformBatch batch(level);
    for ( const FrameRef ref : frameIndices )
    {
        if ( !isValidFrameIndex(level, ref) )
        {
            continue;
        }
        if ( !batch.stage(ref, [&](FramePose& pose)
            {
                pose.position = flippedPointOverPlane(pose.position, planeNormal, cx, cy, cz);
            }) )
        {
            return false;
        }
    }
    return batch.commit();
}

/** Flips winding for selected surfaces. */
void levelFlipSurfaces(Level& level, const std::span<const SurfaceRef> surfaceIndices) noexcept
{
    for (const SurfaceRef ref : surfaceIndices)
    {
        if (!isValidSurfaceIndex(level, ref))
        {
            continue;
        }

        Sector& sector = level.sectors[static_cast<std::size_t>(ref.sector)];
        Surface& surface = sector.surfaces[static_cast<std::size_t>(ref.surface)];
        flipSurface(sector, surface);
    }
}

/** Mirrors selected sectors across one principal axis and removes adjoins to unselected sectors. */
bool levelFlipSectorsChecked(
    Level& level,
    const std::span<const std::int32_t> sectorIndices,
    const double cx,
    const double cy,
    const double cz,
    const Axis axis) noexcept
{
    if ( !isFinitePoint(cx, cy, cz) || !isValidAxis(axis) )
    {
        return false;
    }

    SectorRecalcScratch scratch{};
    if ( !scratch.prepare(level) )
    {
        return false;
    }

    std::vector<bool> selected{};
    try
    {
        selected = buildSelectedSectorMap(level, sectorIndices);
    }
    catch (...)
    {
        return false;
    }

    levelClearVertexMarks(level);
    unadjoinSurfacesOutsideSelection(level, sectorIndices, selected);

    const auto flip = [cx, cy, cz, axis](Vertex& vertex) noexcept {
        flipVertexOnce(vertex, cx, cy, cz, axis);
    };

    for (const std::int32_t sc : sectorIndices)
    {
        if (!isValidSectorIndex(level, sc))
        {
            continue;
        }

        Sector& sector = level.sectors[static_cast<std::size_t>(sc)];
        transformSectorVertices(sector, flip);

        for (Surface& surface : sector.surfaces)
        {
            flipSurface(sector, surface);
        }

        scratch.recalc(level, sc);
    }
    return true;
}

/** Mirrors selected sectors across an arbitrary plane and removes adjoins to unselected sectors. */
bool levelFlipSectorsOverPlaneChecked(
    Level& level,
    const std::span<const std::int32_t> sectorIndices,
    const TVector& planeNormal,
    const double px,
    const double py,
    const double pz) noexcept
{
    if ( !isValidPlaneInput(planeNormal, px, py, pz) )
    {
        return false;
    }

    SectorRecalcScratch scratch{};
    if ( !scratch.prepare(level) )
    {
        return false;
    }

    std::vector<bool> selected{};
    try
    {
        selected = buildSelectedSectorMap(level, sectorIndices);
    }
    catch (...)
    {
        return false;
    }

    levelClearVertexMarks(level);
    unadjoinSurfacesOutsideSelection(level, sectorIndices, selected);

    const auto flip = [&planeNormal, px, py, pz](Vertex& vertex) noexcept {
        flipVertexOverPlaneOnce(vertex, planeNormal, px, py, pz);
    };

    for (const std::int32_t sc : sectorIndices)
    {
        if (!isValidSectorIndex(level, sc))
        {
            continue;
        }

        Sector& sector = level.sectors[static_cast<std::size_t>(sc)];
        transformSectorVertices(sector, flip);

        for (Surface& surface : sector.surfaces)
        {
            flipSurface(sector, surface);
        }

        scratch.recalc(level, sc);
    }
    return true;
}

void levelFlipSectors(
    Level& level,
    const std::span<const std::int32_t> sectorIndices,
    const double cx,
    const double cy,
    const double cz,
    const Axis axis) noexcept
{
    (void)levelFlipSectorsChecked(level, sectorIndices, cx, cy, cz, axis);
}

void levelFlipSectorsOverPlane(
    Level& level,
    const std::span<const std::int32_t> sectorIndices,
    const TVector& planeNormal,
    const double px,
    const double py,
    const double pz) noexcept
{
    (void)levelFlipSectorsOverPlaneChecked(level, sectorIndices, planeNormal, px, py, pz);
}

/** Duplicates one sector into a caller-supplied basis and clears copied adjoins. */
std::int32_t levelDuplicateSector(
    Level& level,
    const std::int32_t sectorIndex,
    const double cx,
    const double cy,
    const double cz,
    const TVector& nx,
    const TVector& ny,
    const TVector& nz,
    const double dx,
    const double dy,
    const double dz) noexcept
{
    if ( !isValidSectorIndex(level, sectorIndex)
        || !isFinitePoint(cx, cy, cz) || !isFiniteVector(nx)
        || !isFiniteVector(ny) || !isFiniteVector(nz)
        || !isFinitePoint(dx, dy, dz)
        || !level.canAllocateIds(1) )
    {
        return -1;
    }

    const std::size_t srcIndex  = static_cast<std::size_t>(sectorIndex);
    if ( level.sectors.size() > static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max()) )
    {
        return -1;
    }
    const std::int32_t dstIndex = static_cast<std::int32_t>(level.sectors.size());
    const Sector& src = level.sectors[srcIndex];

    // Stage every nested allocation before appending a Sector or advancing the
    // Level id counter. The final move publishes the complete duplicate at once.
    Sector staged{};
    try
    {
        copySectorMetadata(src, staged);

        const TVector center{ cx, cy, cz };
        const TVector translation{ dx, dy, dz };

        staged.vertices.reserve(src.vertices.size());
        for ( const Vertex& vertex : src.vertices )
        {
            Vertex copy{};
            setVertexPosition(copy,
                duplicateVertexPosition(vertex, center, nx, ny, nz, translation));
            copy.mark = 0;
            staged.vertices.push_back(copy);
        }

        staged.surfaces.reserve(src.surfaces.size());
        for ( const Surface& surface : src.surfaces )
        {
            const std::int32_t surfaceNum =
                static_cast<std::int32_t>(staged.surfaces.size());
            staged.surfaces.push_back(makeDuplicatedSurface(surface, surfaceNum));
        }
    }
    catch (...)
    {
        return -1;
    }

    // The duplicate has no adjoins, so its normals can be prepared entirely
    // inside the staged value without allocating or touching the source Level.
    for ( Surface& surface : staged.surfaces )
    {
        surface.normal = surfaceCalcNormalNewell(staged, surface);
    }

    try
    {
        level.sectors.reserve(level.sectors.size() + 1u);
    }
    catch (...)
    {
        return -1;
    }

    static_assert(std::is_nothrow_move_assignable_v<Sector>);
    Sector& dst = level.newSector();
    const std::int32_t duplicateId = dst.id;
    dst = std::move(staged);
    dst.id = duplicateId;
    dst.num = dstIndex;
    return dstIndex;
}

} // namespace sed::level
