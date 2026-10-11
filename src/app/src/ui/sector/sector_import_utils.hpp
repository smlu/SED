#pragma once

#include "sed/assets/3do.hpp"
#include "sed/level.hpp"
#include "sed/level/uv.hpp"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <istream>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace sed::ui::sector
{

struct ImportedSectorFace
{
    std::vector<std::int32_t> vertices{};
    std::vector<std::int32_t> texVertices{};
    std::string material{};
    std::string sourceMesh{};
    std::optional<std::size_t> sourceFaceIndex{};
};

struct ImportedSectorMesh
{
    std::string name{};
    std::vector<TVector> vertices{};
    std::vector<TexVertex> texVertices{};
    std::vector<ImportedSectorFace> faces{};
    UvCoordinateSpace uvCoordinateSpace{ UvCoordinateSpace::Normalized };
};

struct ImportedSectorMeshResult
{
    std::vector<ImportedSectorMesh> meshes{};
    std::string error{};

    [[nodiscard]] explicit operator bool() const noexcept
    {
        return error.empty() && !meshes.empty();
    }
};

enum class ImportedSectorDiagnosticKind : std::uint8_t
{
    emptyMesh,
    nonFiniteVertex,
    invalidVertexReference,
    incompletePolygon,
    invalidPolygon,
    nonPlanarPolygon,
    nonConvexPolygon,
    zeroLengthEdge,
    openBoundaryEdge,
    overSharedEdge,
};

struct ImportedSectorSourceFace
{
    std::string mesh{};
    std::size_t faceIndex{};

    [[nodiscard]] bool operator==(
        const ImportedSectorSourceFace&) const noexcept = default;
};

/**
 * One bounded, machine-readable problem found while building a Sector.
 * Vertex indices refer to the candidate Sector after coincident imported
 * vertices have been welded. Face indices are local to the named source
 * mesh/object and preserve its authored face order.
 */
struct ImportedSectorDiagnostic
{
    ImportedSectorDiagnosticKind kind{
        ImportedSectorDiagnosticKind::invalidPolygon };
    std::string mesh{};
    std::optional<std::size_t> faceIndex{};
    std::optional<std::size_t> cornerIndex{};
    std::optional<std::int32_t> firstVertexIndex{};
    std::optional<std::int32_t> secondVertexIndex{};
    std::optional<TVector> firstVertexPosition{};
    std::optional<TVector> secondVertexPosition{};
    std::vector<std::size_t> owningFaceIndices{};
    std::vector<ImportedSectorSourceFace> owningFaces{};
    std::size_t omittedOwningFaceCount{};
    std::size_t edgeUseCount{};
};

struct ImportedSectorBuildResult
{
    std::vector<std::int32_t> sectors{};
    std::string error{};
    std::string details{};
    std::vector<ImportedSectorDiagnostic> diagnostics{};
    std::size_t omittedDiagnosticCount{};

    [[nodiscard]] explicit operator bool() const noexcept
    {
        return error.empty() && !sectors.empty();
    }
};

struct ExportedSectorTextResult
{
    std::string text{};
    std::string error{};

    [[nodiscard]] explicit operator bool() const noexcept
    {
        return error.empty() && !text.empty();
    }
};

struct ObjSectorImportOptions
{
    /**
     * Convert conventional right-handed OBJ Y-up positions to the
     * right-handed Sith Z-up frame: (x, y, z) -> (x, -z, y).
     * Texture coordinates are never transformed.
     */
    bool convertYUpToSithZUp{ true };

    struct TextureSize
    {
        float width{ 1.0f };
        float height{ 1.0f };
    };

    /** Standard OBJ UVs remain normalized. This resolver is used only to
     * migrate an older SED OBJ carrying the explicit pixel-space marker.
     */
    std::function<std::optional<TextureSize>(std::string_view material)>
        materialTextureSize{};
};

using SectorTextureDimensionsResolver = std::function<
    std::optional<level::UvTextureDimensions>(std::string_view material)>;

struct ImportedSectorBuildOptions
{
    /** Required only when the imported mesh and destination Level use
     * different UV spaces, or when a face without authored UVs needs a
     * default normalized frame.
     */
    SectorTextureDimensionsResolver materialTextureDimensions{};
};

struct SectorExportOptions
{
    /** Resolves actual first-cel MAT dimensions for legacy pixel boundaries. */
    SectorTextureDimensionsResolver materialTextureDimensions{};
};

/**
 * Loads Wavefront OBJ positions, 2D UVs, polygon faces, object/group names,
 * and usemtl names. Authored normals are deliberately ignored because Sector
 * normals are derived from converted polygon geometry.
 */
ImportedSectorMeshResult loadObjSectorMeshes(
    std::istream& input,
    ObjSectorImportOptions options = {});

/**
 * Extracts non-empty node meshes (or meshes from a node-less 3DO). Raw 3DO
 * loads are independently instantiated through each node's full parent chain,
 * so multiple nodes may safely reference one authored mesh.
 */
ImportedSectorMeshResult importedSectorMeshesFrom3do(const assets::Model3do& model);

/**
 * Transactionally appends imported geometry as one sector or one sector per
 * non-empty mesh.  Split sectors are auto-adjoined across compatible matching
 * faces. A face without one authored UV per corner receives the standard SED
 * default frame; normalized destinations require that face's real MAT
 * dimensions. Invalid geometry or an unresolved conversion leaves the Level
 * unchanged.
 */
ImportedSectorBuildResult appendImportedSectors(
    Level& level,
    std::span<const ImportedSectorMesh> meshes,
    bool splitMeshes,
    const ImportedSectorBuildOptions& options = {});

/** Serializes selected sectors as deterministic Wavefront OBJ polygon groups. */
ExportedSectorTextResult exportSectorsToObj(
    const Level& level,
    std::span<const std::int32_t> sectors,
    const SectorExportOptions& options = {});

/**
 * Serializes one zero-transform 3DO mesh/hierarchy node per selected sector.
 * Polygon geometry, per-corner UVs, and material names are preserved; editor
 * metadata and sector adjoin relationships are not representable in 3DO.
 */
ExportedSectorTextResult exportSectorsTo3do(
    const Level& level,
    std::span<const std::int32_t> sectors,
    const SectorExportOptions& options = {});

} // namespace sed::ui::sector
