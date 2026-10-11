#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include "sed/geometry.hpp"
#include "sed/level.hpp"

namespace sed::level
{

enum class SectorConnectKnifeMode
{
    /** Preserve the current behavior: keep target-side pieces produced by the knife cut. */
    KeepTargetPieces,
    /** Use the knife sector to cut portals, then remove target-side pieces inside the knife volume. */
    CutTargetHolesOnly,
};

/** One infinite plane used by an atomic staged Sector cleave. */
struct SectorCleavePlane
{
    TVector normal{};
    TVector point{};
};

/** The finite authored stroke that defines one staged cleave plane. */
struct SectorCleaveStroke
{
    TVector first{};
    TVector second{};
};

/** Optional topology-aware behavior for an atomic staged Sector cleave. */
struct SectorCleaveBatchOptions
{
    /**
     * Treat the planes as the ordered boundary of one closed convex shape.
     * Only the descendant on the same side as `interiorPoint` is processed by
     * the next plane, avoiding the N-by-N grid produced by unrelated planes.
     */
    bool closedShape{};
    TVector interiorPoint{};

    /** Finite strokes in the same order as the staged planes, when supplied. */
    std::span<const SectorCleaveStroke> strokes{};

    /** Restrict each independent plane to descendants reached by its stroke. */
    bool bounded{};
};

/** Why an atomic staged Sector cleave was rejected. */
enum class SectorCleaveBatchError
{
    None,
    InvalidSector,
    EmptyPlaneSet,
    InvalidPlane,
    InvalidInputGeometry,
    PlaneDoesNotSplit,
    CleaveFailed,
    InvalidResultTopology,
    NonPlanarResultSurface,
    NonConvexResultSurface,
    NonConvexResultSector,
    InvalidResultAdjoin,
};

/**
 * Outcome of an atomic staged Sector cleave.
 *
 * `failedPlane`, `failedSector`, and `failedSurface` are -1 when that detail
 * does not apply. On success, `outputSectors` contains the original Sector
 * followed by every appended descendant Sector. On failure the Level is
 * unchanged and `outputSectors` is empty.
 */
struct SectorCleaveBatchResult
{
    SectorCleaveBatchError error{ SectorCleaveBatchError::None };
    std::int32_t failedPlane{ -1 };
    std::int32_t failedSector{ -1 };
    std::int32_t failedSurface{ -1 };
    std::vector<std::int32_t> outputSectors{};

    [[nodiscard]] bool succeeded() const noexcept
    {
        return error == SectorCleaveBatchError::None;
    }
};

/** Splits one surface by a plane and preserves/rebuilds adjoins when the source surface was adjoined. */
bool levelCleaveSurface(Level& level, SurfaceRef surface, const TVector& planeNormal, double px, double py, double pz) noexcept;

/** Inserts a plane intersection vertex on one selected edge and propagates it to same-sector matching edges. */
bool levelCleaveEdge(Level& level, SurfaceRef surface, std::int32_t edge, const TVector& planeNormal, double px, double py, double pz) noexcept;

/**
 * Splits a Sector by a plane, creates cap surfaces, and relayers contained Things.
 * The body containing the pre-cut center keeps the source id/number; the
 * cut-off body is appended. The operation is failure-atomic: every rejected
 * split leaves the complete input Level unchanged.
 */
bool levelCleaveSector(Level& level, std::int32_t sector, const TVector& planeNormal, double px, double py, double pz) noexcept;

/**
 * Atomically splits one Sector and all of its descendants by a sequence of planes.
 *
 * Independent planes that split no admitted descendant are skipped; the batch
 * must contain at least one effective split. Closed shapes must retain a
 * nonempty interior after every plane. Planes are applied only to descendants
 * they actually cross, avoiding destructive no-op cleaves. The
 * candidate Level is committed only when every output surface is planar and
 * convex, every output Sector is convex, and all affected adjoins are valid.
 * Each individual split retains the source identity on the body containing
 * that descendant's pre-cut center.
 */
[[nodiscard]] SectorCleaveBatchResult levelCleaveSectorByPlanes(
    Level& level,
    std::int32_t sector,
    std::span<const SectorCleavePlane> planes,
    const SectorCleaveBatchOptions& options = {}) noexcept;

/** Connects two overlapping opposite-facing surfaces from different sectors by creating an adjoin pair. */
bool levelConnectSurfaces(Level& level, SurfaceRef surf1, SurfaceRef surf2) noexcept;

/** Connects two sectors by finding and adjoining their overlapping portal surfaces. */
bool levelConnectSectors(Level& level, std::int32_t sector1, std::int32_t sector2) noexcept;

/** Uses one sector as a cutting volume and connects it to the target. */
bool levelConnectSectorsKnife(
    Level& level,
    std::int32_t knifeSector,
    std::int32_t targetSector,
    SectorConnectKnifeMode mode = SectorConnectKnifeMode::KeepTargetPieces) noexcept;

/** Knife-sector connection variant that reports identity exhaustion explicitly. */
bool levelConnectSectorsKnife(
    Level& level,
    std::int32_t knifeSector,
    std::int32_t targetSector,
    SectorConnectKnifeMode mode,
    bool* idExhausted) noexcept;

/**
 * Connects any overlapping or splittable portal surfaces between two explicit sector sets.
 *
 * This is the set-based form of sector auto-adjoin used by higher-level tools after
 * they have already decided which sectors are allowed to participate.
 */
bool levelConnectSectorSurfaceCandidates(
    Level& level,
    const std::vector<std::int32_t>& sourceSectors,
    const std::vector<std::int32_t>& targetSectors) noexcept;

/** Deletes a sector and rewrites or clears references that pointed past/into it. */
bool levelDeleteSector(Level& level, std::int32_t sector) noexcept;

/** Deletes a surface and rewrites or clears references that pointed past/to it. */
bool levelDeleteSurface(Level& level, SurfaceRef surface) noexcept;

} // namespace sed::level
