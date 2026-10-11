#include "ui/sector/sector_import_utils.hpp"

#include "sed/level/geometry.hpp"
#include "sed/level/uv.hpp"
#include "sed/text.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <map>
#include <optional>
#include <iomanip>
#include <sstream>
#include <string_view>
#include <unordered_map>
#include <utility>

namespace sed::ui::sector
{
namespace
{

constexpr std::string_view kObjPixelUvMarker =
    "# SED texture coordinates: pixels";
constexpr std::string_view kObjNormalizedUvMarker =
    "# SED texture coordinates: normalized";

bool isRepresentableFloat(const double value) noexcept
{
    return std::isfinite(value)
        && value >= -static_cast<double>(std::numeric_limits<float>::max())
        && value <= static_cast<double>(std::numeric_limits<float>::max());
}

std::string_view objRecordArgument(const std::string_view line) noexcept
{
    const std::string_view trimmed = text::trimView(line);
    const std::size_t separator = trimmed.find_first_of(" \t\r\n");
    if (separator == std::string_view::npos)
    {
        return {};
    }
    return text::trimView(trimmed.substr(separator + 1));
}

TVector rotateObjYUpToSithZUp(const TVector value) noexcept
{
    // Pure +90-degree rotation around +X. It applies equally to points and
    // direction vectors because it has no translation component.
    return TVector{ value.x, -value.z, value.y };
}

std::optional<std::int32_t> objIndex(
    const std::string_view token,
    const std::size_t count) noexcept
{
    const std::optional<std::int32_t> parsed = text::parseInt<std::int32_t>(token);
    if (!parsed || *parsed == 0)
    {
        return std::nullopt;
    }
    const std::int64_t resolved = *parsed > 0
        ? static_cast<std::int64_t>(*parsed) - 1
        : static_cast<std::int64_t>(count) + static_cast<std::int64_t>(*parsed);
    if (resolved < 0 || resolved >= static_cast<std::int64_t>(count))
    {
        return std::nullopt;
    }
    return static_cast<std::int32_t>(resolved);
}

bool parseObjFaceToken(
    const std::string_view token,
    const std::size_t vertexCount,
    const std::size_t texVertexCount,
    std::int32_t& vertex,
    std::int32_t& texVertex) noexcept
{
    const std::size_t firstSlash = token.find('/');
    const std::string_view vertexToken = token.substr(0, firstSlash);
    const std::optional<std::int32_t> parsedVertex = objIndex(vertexToken, vertexCount);
    if (!parsedVertex)
    {
        return false;
    }
    vertex = *parsedVertex;
    texVertex = -1;

    if (firstSlash == std::string_view::npos)
    {
        return true;
    }
    const std::size_t secondSlash = token.find('/', firstSlash + 1);
    // A possible suffix after secondSlash is the OBJ normal index. Sector
    // import intentionally ignores authored normals: final surface normals are
    // derived from converted polygon geometry and normalized winding.
    const std::string_view texToken = token.substr(
        firstSlash + 1,
        secondSlash == std::string_view::npos
            ? std::string_view::npos
            : secondSlash - firstSlash - 1);
    if (texToken.empty())
    {
        return true;
    }
    const std::optional<std::int32_t> parsedTex = objIndex(texToken, texVertexCount);
    if (!parsedTex)
    {
        return false;
    }
    texVertex = *parsedTex;
    return true;
}

void appendMesh(ImportedSectorMesh& destination, const ImportedSectorMesh& source)
{
    const std::int32_t vertexBase = static_cast<std::int32_t>(destination.vertices.size());
    const std::int32_t texVertexBase = static_cast<std::int32_t>(destination.texVertices.size());
    destination.vertices.insert(
        destination.vertices.end(), source.vertices.begin(), source.vertices.end());
    destination.texVertices.insert(
        destination.texVertices.end(), source.texVertices.begin(), source.texVertices.end());
    destination.faces.reserve(destination.faces.size() + source.faces.size());
    for (const ImportedSectorFace& sourceFace : source.faces)
    {
        ImportedSectorFace face = sourceFace;
        for (std::int32_t& vertex : face.vertices)
        {
            vertex += vertexBase;
        }
        for (std::int32_t& texVertex : face.texVertices)
        {
            if (texVertex >= 0)
            {
                texVertex += texVertexBase;
            }
        }
        destination.faces.push_back(std::move(face));
    }
}

bool compactImportedMeshIndices(ImportedSectorMesh& mesh, std::string& error)
{
    std::vector<std::int32_t> vertexRemap(mesh.vertices.size(), -1);
    std::vector<std::int32_t> texVertexRemap(mesh.texVertices.size(), -1);
    std::vector<TVector> compactVertices{};
    std::vector<TexVertex> compactTexVertices{};

    for (ImportedSectorFace& face : mesh.faces)
    {
        for (std::int32_t& vertex : face.vertices)
        {
            if (vertex < 0 || static_cast<std::size_t>(vertex) >= mesh.vertices.size())
            {
                error = "Imported polygon references an invalid vertex.";
                return false;
            }
            std::int32_t& mapped = vertexRemap[static_cast<std::size_t>(vertex)];
            if (mapped < 0)
            {
                mapped = static_cast<std::int32_t>(compactVertices.size());
                compactVertices.push_back(mesh.vertices[static_cast<std::size_t>(vertex)]);
            }
            vertex = mapped;
        }
        for (std::int32_t& texVertex : face.texVertices)
        {
            if (texVertex < 0)
            {
                continue;
            }
            if (static_cast<std::size_t>(texVertex) >= mesh.texVertices.size())
            {
                error = "Imported polygon references an invalid texture coordinate.";
                return false;
            }
            std::int32_t& mapped = texVertexRemap[static_cast<std::size_t>(texVertex)];
            if (mapped < 0)
            {
                mapped = static_cast<std::int32_t>(compactTexVertices.size());
                compactTexVertices.push_back(mesh.texVertices[static_cast<std::size_t>(texVertex)]);
            }
            texVertex = mapped;
        }
    }
    mesh.vertices = std::move(compactVertices);
    mesh.texVertices = std::move(compactTexVertices);
    return true;
}

std::uint64_t edgeKey(const std::int32_t first, const std::int32_t second) noexcept
{
    const std::uint32_t low = static_cast<std::uint32_t>(std::min(first, second));
    const std::uint32_t high = static_cast<std::uint32_t>(std::max(first, second));
    return (static_cast<std::uint64_t>(low) << 32u) | static_cast<std::uint64_t>(high);
}

std::vector<ImportedSectorMesh> partitionSequentialClosedShells(
    ImportedSectorMesh mesh)
{
    // Level OBJ exporters commonly write every sector into one `o` block but
    // keep each sector's faces contiguous.  A global edge count then reports
    // 4/6/... uses at shared portals even though every individual sector is a
    // valid two-use closed shell.  Partition only at deterministic face-order
    // boundaries where all edges in the current prefix have exactly two uses.
    // This does not relax manifold validation and deliberately does not guess
    // arbitrary connected components.
    std::vector<ImportedSectorMesh> shells{};
    std::unordered_map<std::uint64_t, std::uint32_t> edgeCounts{};
    std::size_t shellFirstFace = 0;
    // Count 1-use edges incrementally so testing a face boundary is O(1)
    // instead of rescanning every edge accumulated in the prefix.
    std::size_t unmatchedEdges = 0;
    bool prefixCanClose = true;
    for (std::size_t faceIndex = 0; faceIndex < mesh.faces.size(); ++faceIndex)
    {
        const ImportedSectorFace& face = mesh.faces[faceIndex];
        if (face.vertices.size() < 3)
        {
            prefixCanClose = false;
        }
        for (std::size_t edge = 0; edge < face.vertices.size(); ++edge)
        {
            const std::int32_t first = face.vertices[edge];
            const std::int32_t second = face.vertices[(edge + 1) % face.vertices.size()];
            if (first == second)
            {
                prefixCanClose = false;
                continue;
            }
            std::uint32_t& count = edgeCounts[edgeKey(first, second)];
            if (count == 0u)
            {
                ++unmatchedEdges;
            }
            else if (count == 1u)
            {
                --unmatchedEdges;
            }
            else
            {
                prefixCanClose = false;
            }
            ++count;
        }

        const std::size_t prefixFaceCount = faceIndex + 1 - shellFirstFace;
        const bool closed = prefixCanClose && prefixFaceCount >= 4
            && unmatchedEdges == 0;
        if (!closed)
        {
            continue;
        }

        ImportedSectorMesh shell{};
        shell.name = mesh.name + " [" + std::to_string(shells.size() + 1) + "]";
        shell.uvCoordinateSpace = mesh.uvCoordinateSpace;
        shell.vertices = mesh.vertices;
        shell.texVertices = mesh.texVertices;
        shell.faces.insert(shell.faces.end(),
            std::make_move_iterator(mesh.faces.begin()
                + static_cast<std::ptrdiff_t>(shellFirstFace)),
            std::make_move_iterator(mesh.faces.begin()
                + static_cast<std::ptrdiff_t>(faceIndex + 1)));
        shells.push_back(std::move(shell));
        shellFirstFace = faceIndex + 1;
        edgeCounts.clear();
        unmatchedEdges = 0;
        prefixCanClose = true;
    }

    // Keep a non-closed suffix intact. The normal transactional sector
    // validator will produce the precise open/non-manifold diagnostic and
    // reject the entire import rather than silently dropping geometry.
    if (shellFirstFace < mesh.faces.size())
    {
        ImportedSectorMesh remainder{};
        remainder.name = shells.empty()
            ? mesh.name
            : mesh.name + " [incomplete]";
        remainder.vertices = std::move(mesh.vertices);
        remainder.texVertices = std::move(mesh.texVertices);
        remainder.uvCoordinateSpace = mesh.uvCoordinateSpace;
        remainder.faces.insert(remainder.faces.end(),
            std::make_move_iterator(mesh.faces.begin()
                + static_cast<std::ptrdiff_t>(shellFirstFace)),
            std::make_move_iterator(mesh.faces.end()));
        shells.push_back(std::move(remainder));
    }
    else if (shells.size() == 1)
    {
        // One explicit object/group that contains one shell keeps its authored
        // name. Only a true multi-shell partition needs numeric suffixes.
        shells.front().name = mesh.name;
    }
    return shells.empty() ? std::vector<ImportedSectorMesh>{ std::move(mesh) }
                          : shells;
}

constexpr std::size_t kMaximumImportedSectorDiagnostics = 16;
constexpr std::size_t kMaximumImportedSectorOwningFaces = 8;
constexpr std::size_t kMaximumImportedSectorMeshNameBytes = 128;

std::string diagnosticMeshName(const std::string_view meshName)
{
    std::string sanitized{};
    sanitized.reserve(std::min(
        meshName.size(), kMaximumImportedSectorMeshNameBytes));
    for (const char raw : meshName)
    {
        const unsigned char value = static_cast<unsigned char>(raw);
        sanitized.push_back(value < 0x20u || value == 0x7fu ? ' ' : raw);
    }
    sanitized = text::trim(sanitized);
    if (sanitized.size() > kMaximumImportedSectorMeshNameBytes)
    {
        std::size_t truncateAt =
            kMaximumImportedSectorMeshNameBytes - 3;
        while (truncateAt > 0
            && (static_cast<unsigned char>(sanitized[truncateAt]) & 0xc0u)
                == 0x80u)
        {
            --truncateAt;
        }
        sanitized.resize(truncateAt);
        sanitized += "...";
    }
    return sanitized.empty() ? "unnamed mesh" : sanitized;
}

ImportedSectorSourceFace sourceFaceReference(
    const ImportedSectorFace& face,
    const std::string_view fallbackMesh,
    const std::size_t fallbackFaceIndex)
{
    ImportedSectorSourceFace source{};
    source.mesh = diagnosticMeshName(
        face.sourceMesh.empty() ? fallbackMesh : face.sourceMesh);
    source.faceIndex = face.sourceFaceIndex.value_or(fallbackFaceIndex);
    return source;
}

void appendDiagnostic(
    ImportedSectorBuildResult& failure,
    ImportedSectorDiagnostic diagnostic)
{
    if (failure.diagnostics.size() < kMaximumImportedSectorDiagnostics)
    {
        failure.diagnostics.push_back(std::move(diagnostic));
    }
    else
    {
        ++failure.omittedDiagnosticCount;
    }
}

std::string formatDiagnosticNumber(double value)
{
    if (std::abs(value) <= 1.0e-12)
    {
        value = 0.0;
    }
    std::ostringstream output{};
    output << std::setprecision(12) << value;
    return output.str();
}

std::string formatDiagnosticPosition(const TVector& position)
{
    return "(" + formatDiagnosticNumber(position.x) + ", "
        + formatDiagnosticNumber(position.y) + ", "
        + formatDiagnosticNumber(position.z) + ")";
}

std::string formatOwningFaces(const std::vector<std::size_t>& faces)
{
    std::ostringstream output{};
    for (std::size_t index = 0; index < faces.size(); ++index)
    {
        if (index > 0)
        {
            output << ", ";
        }
        output << faces[index];
    }
    return output.str();
}

std::string formatOwningSourceFaces(
    const std::vector<ImportedSectorSourceFace>& faces)
{
    std::ostringstream output{};
    std::size_t first = 0;
    while (first < faces.size())
    {
        std::size_t last = first + 1;
        while (last < faces.size()
            && faces[last].mesh == faces[first].mesh)
        {
            ++last;
        }
        if (first > 0)
        {
            output << ", ";
        }
        output << '\'' << faces[first].mesh << "' "
            << (last - first == 1 ? "face " : "faces ");
        for (std::size_t index = first; index < last; ++index)
        {
            if (index > first)
            {
                output << ", ";
            }
            output << faces[index].faceIndex;
        }
        first = last;
    }
    return output.str();
}

const char* diagnosticKindText(
    const ImportedSectorDiagnosticKind kind) noexcept
{
    switch (kind)
    {
        case ImportedSectorDiagnosticKind::emptyMesh:
            return "mesh has no geometry";
        case ImportedSectorDiagnosticKind::nonFiniteVertex:
            return "non-finite vertex";
        case ImportedSectorDiagnosticKind::invalidVertexReference:
            return "invalid polygon vertex reference";
        case ImportedSectorDiagnosticKind::incompletePolygon:
            return "incomplete polygon";
        case ImportedSectorDiagnosticKind::invalidPolygon:
            return "invalid polygon";
        case ImportedSectorDiagnosticKind::nonPlanarPolygon:
            return "non-planar polygon";
        case ImportedSectorDiagnosticKind::nonConvexPolygon:
            return "non-convex polygon";
        case ImportedSectorDiagnosticKind::zeroLengthEdge:
            return "zero-length polygon edge";
        case ImportedSectorDiagnosticKind::openBoundaryEdge:
            return "open boundary edge";
        case ImportedSectorDiagnosticKind::overSharedEdge:
            return "over-shared edge";
    }
    return "invalid geometry";
}

std::string formatImportedSectorDiagnostics(
    const ImportedSectorBuildResult& failure)
{
    std::ostringstream output{};
    for (const ImportedSectorDiagnostic& diagnostic : failure.diagnostics)
    {
        output << "Mesh '" << diagnosticMeshName(diagnostic.mesh) << "': "
            << diagnosticKindText(diagnostic.kind);
        if (diagnostic.firstVertexIndex && diagnostic.secondVertexIndex)
        {
            output << " between vertices " << *diagnostic.firstVertexIndex;
            if (diagnostic.firstVertexPosition)
            {
                output << ' '
                    << formatDiagnosticPosition(*diagnostic.firstVertexPosition);
            }
            output << " and " << *diagnostic.secondVertexIndex;
            if (diagnostic.secondVertexPosition)
            {
                output << ' '
                    << formatDiagnosticPosition(*diagnostic.secondVertexPosition);
            }
        }
        else if (diagnostic.firstVertexIndex)
        {
            output << " at vertex " << *diagnostic.firstVertexIndex;
            if (diagnostic.firstVertexPosition)
            {
                output << ' '
                    << formatDiagnosticPosition(*diagnostic.firstVertexPosition);
            }
        }
        if (diagnostic.faceIndex)
        {
            output << "; face: " << *diagnostic.faceIndex;
        }
        if (diagnostic.cornerIndex)
        {
            output << "; corner: " << *diagnostic.cornerIndex;
        }
        const bool ownersNeedSourceNames = std::any_of(
            diagnostic.owningFaces.begin(),
            diagnostic.owningFaces.end(),
            [&diagnostic](const ImportedSectorSourceFace& source)
            {
                return source.mesh != diagnostic.mesh;
            });
        if (ownersNeedSourceNames)
        {
            output << "; owning faces: "
                << formatOwningSourceFaces(diagnostic.owningFaces);
        }
        else if (!diagnostic.owningFaceIndices.empty())
        {
            output << "; owning "
                << (diagnostic.owningFaceIndices.size() == 1
                        ? "face: " : "faces: ")
                << formatOwningFaces(diagnostic.owningFaceIndices);
        }
        if (diagnostic.omittedOwningFaceCount > 0)
        {
            output << "; " << diagnostic.omittedOwningFaceCount
                << " additional owning faces omitted";
        }
        if (diagnostic.edgeUseCount > 0)
        {
            output << "; edge use count: " << diagnostic.edgeUseCount;
        }
        output << ".\n";
    }
    if (failure.omittedDiagnosticCount > 0)
    {
        output << failure.omittedDiagnosticCount
            << " additional topology issues omitted.\n";
    }
    std::string details = output.str();
    if (!details.empty())
    {
        details.pop_back();
    }
    return details;
}

ImportedSectorDiagnostic makeFaceDiagnostic(
    const ImportedSectorDiagnosticKind kind,
    const ImportedSectorSourceFace& source)
{
    ImportedSectorDiagnostic diagnostic{};
    diagnostic.kind = kind;
    diagnostic.mesh = diagnosticMeshName(source.mesh);
    diagnostic.faceIndex = source.faceIndex;
    return diagnostic;
}

bool validateImportedSurfaceUvData(
    const Surface& surface,
    const ImportedSectorFace& importedFace,
    const std::string_view fallbackMesh,
    const std::size_t fallbackFaceIndex,
    ImportedSectorBuildResult& failure)
{
    const ImportedSectorSourceFace source = sourceFaceReference(
        importedFace, fallbackMesh, fallbackFaceIndex);
    for (std::size_t cornerIndex = 0;
         cornerIndex < surface.texVertices.size();
         ++cornerIndex)
    {
        const TexVertex& coordinate = surface.texVertices[cornerIndex];
        if (!std::isfinite(coordinate.u)
            || !std::isfinite(coordinate.v))
        {
            failure.error = "face " + std::to_string(source.faceIndex)
                + " corner " + std::to_string(cornerIndex)
                + " has a non-finite or float-overflow texture coordinate.";
            ImportedSectorDiagnostic diagnostic = makeFaceDiagnostic(
                ImportedSectorDiagnosticKind::invalidPolygon,
                source);
            diagnostic.cornerIndex = cornerIndex;
            appendDiagnostic(failure, std::move(diagnostic));
            return false;
        }
    }
    if (!std::isfinite(surface.uScale)
        || !std::isfinite(surface.vScale))
    {
        failure.error = "face " + std::to_string(source.faceIndex)
            + " has a non-finite or float-overflow UV scale.";
        appendDiagnostic(failure, makeFaceDiagnostic(
            ImportedSectorDiagnosticKind::invalidPolygon,
            source));
        return false;
    }
    return true;
}

bool orientAndValidateSector(
    Sector& sector,
    const ImportedSectorMesh& importedMesh,
    ImportedSectorBuildResult& failure)
{
    TVector center{};
    for (const Vertex& vertex : sector.vertices)
    {
        center.x += vertex.x;
        center.y += vertex.y;
        center.z += vertex.z;
    }
    center = center * (1.0 / static_cast<double>(sector.vertices.size()));

    struct EdgeOwners
    {
        std::int32_t first{};
        std::int32_t second{};
        std::size_t useCount{};
        std::vector<ImportedSectorSourceFace> faces{};
        std::size_t omittedFaceCount{};
    };
    std::map<std::uint64_t, EdgeOwners> edgeOwners{};
    for (std::size_t faceIndex = 0;
         faceIndex < sector.surfaces.size();
         ++faceIndex)
    {
        Surface& surface = sector.surfaces[faceIndex];
        const ImportedSectorSourceFace sourceFace = sourceFaceReference(
            importedMesh.faces[faceIndex], importedMesh.name, faceIndex);
        if (surface.vertices.size() < 3
            || surface.vertices.size() != surface.texVertices.size())
        {
            failure.error = "Imported mesh contains an incomplete polygon.";
            appendDiagnostic(failure, makeFaceDiagnostic(
                ImportedSectorDiagnosticKind::incompletePolygon,
                sourceFace));
            return false;
        }
        surface.normal = level::surfaceCalcNormalNewell(sector, surface);
        TVector surfaceCenter{};
        if (!level::surfaceCalcCenter(sector, surface, surfaceCenter))
        {
            failure.error = "Imported mesh contains an invalid polygon.";
            appendDiagnostic(failure, makeFaceDiagnostic(
                ImportedSectorDiagnosticKind::invalidPolygon,
                sourceFace));
            return false;
        }
        if (surface.normal.Dot(center - surfaceCenter) < 0.0)
        {
            std::reverse(surface.vertices.begin(), surface.vertices.end());
            std::reverse(surface.texVertices.begin(), surface.texVertices.end());
            surface.normal = level::surfaceCalcNormalNewell(sector, surface);
        }
        if (!level::surfaceIsPlanar(sector, surface))
        {
            failure.error = "Imported mesh contains a non-planar polygon.";
            appendDiagnostic(failure, makeFaceDiagnostic(
                ImportedSectorDiagnosticKind::nonPlanarPolygon,
                sourceFace));
            return false;
        }
        if (!level::surfaceIsConvex(sector, surface))
        {
            failure.error = "Imported mesh contains a non-convex polygon.";
            appendDiagnostic(failure, makeFaceDiagnostic(
                ImportedSectorDiagnosticKind::nonConvexPolygon,
                sourceFace));
            return false;
        }

        for (std::size_t edge = 0; edge < surface.vertices.size(); ++edge)
        {
            const std::int32_t first = surface.vertices[edge];
            const std::int32_t second = surface.vertices[(edge + 1) % surface.vertices.size()];
            if (first == second)
            {
                failure.error =
                    "Imported mesh contains a zero-length polygon edge.";
                ImportedSectorDiagnostic diagnostic = makeFaceDiagnostic(
                    ImportedSectorDiagnosticKind::zeroLengthEdge,
                    sourceFace);
                diagnostic.cornerIndex = edge;
                diagnostic.firstVertexIndex = first;
                if (first >= 0
                    && static_cast<std::size_t>(first) < sector.vertices.size())
                {
                    const Vertex& vertex =
                        sector.vertices[static_cast<std::size_t>(first)];
                    diagnostic.firstVertexPosition =
                        TVector{ vertex.x, vertex.y, vertex.z };
                }
                appendDiagnostic(failure, std::move(diagnostic));
                return false;
            }
            const std::int32_t low = std::min(first, second);
            const std::int32_t high = std::max(first, second);
            EdgeOwners& owners = edgeOwners[edgeKey(low, high)];
            owners.first = low;
            owners.second = high;
            ++owners.useCount;
            if (owners.faces.size() < kMaximumImportedSectorOwningFaces)
            {
                owners.faces.push_back(sourceFace);
            }
            else
            {
                ++owners.omittedFaceCount;
            }
        }
    }

    std::size_t openEdgeCount = 0;
    std::size_t overSharedEdgeCount = 0;
    std::size_t maximumEdgeUseCount = 0;
    for (const auto& [edge, owners] : edgeOwners)
    {
        (void)edge;
        const std::size_t useCount = owners.useCount;
        maximumEdgeUseCount = std::max(maximumEdgeUseCount, useCount);
        if (useCount < 2)
        {
            ++openEdgeCount;
        }
        else if (useCount > 2)
        {
            ++overSharedEdgeCount;
        }
        else
        {
            continue;
        }

        ImportedSectorDiagnostic diagnostic{};
        diagnostic.kind = useCount < 2
            ? ImportedSectorDiagnosticKind::openBoundaryEdge
            : ImportedSectorDiagnosticKind::overSharedEdge;
        diagnostic.mesh = owners.faces.empty()
            ? diagnosticMeshName(importedMesh.name)
            : owners.faces.front().mesh;
        diagnostic.firstVertexIndex = owners.first;
        diagnostic.secondVertexIndex = owners.second;
        if (owners.first >= 0
            && static_cast<std::size_t>(owners.first) < sector.vertices.size())
        {
            const Vertex& vertex =
                sector.vertices[static_cast<std::size_t>(owners.first)];
            diagnostic.firstVertexPosition =
                TVector{ vertex.x, vertex.y, vertex.z };
        }
        if (owners.second >= 0
            && static_cast<std::size_t>(owners.second) < sector.vertices.size())
        {
            const Vertex& vertex =
                sector.vertices[static_cast<std::size_t>(owners.second)];
            diagnostic.secondVertexPosition =
                TVector{ vertex.x, vertex.y, vertex.z };
        }
        diagnostic.owningFaces = owners.faces;
        diagnostic.owningFaceIndices.reserve(owners.faces.size());
        for (const ImportedSectorSourceFace& source : owners.faces)
        {
            diagnostic.owningFaceIndices.push_back(source.faceIndex);
        }
        diagnostic.omittedOwningFaceCount = owners.omittedFaceCount;
        diagnostic.edgeUseCount = owners.useCount;
        appendDiagnostic(failure, std::move(diagnostic));
    }
    if (openEdgeCount > 0 && overSharedEdgeCount == 0)
    {
        failure.error = "The mesh has " + std::to_string(openEdgeCount)
            + (openEdgeCount == 1
                    ? " open boundary edge. "
                    : " open boundary edges. ")
            + "Close or fill the holes so every object is a sealed volume before importing it as a Sector.";
        return false;
    }
    if (overSharedEdgeCount > 0 && openEdgeCount == 0)
    {
        failure.error = "The mesh is closed, but it cannot form one SED sector: "
            + std::to_string(overSharedEdgeCount)
            + (overSharedEdgeCount == 1
                    ? " edge is shared by more than two faces"
                    : " edges are shared by more than two faces")
            + " (maximum " + std::to_string(maximumEdgeUseCount)
            + "). Remove internal or overlapping faces, or split the model into separate closed convex objects.";
        return false;
    }
    if (openEdgeCount > 0 || overSharedEdgeCount > 0)
    {
        failure.error = "The mesh cannot form a SED sector: "
            + std::to_string(openEdgeCount) + " open boundary "
            + (openEdgeCount == 1 ? "edge and " : "edges and ")
            + std::to_string(overSharedEdgeCount) + " over-shared "
            + (overSharedEdgeCount == 1 ? "edge. " : "edges. ")
            + "Close all holes and remove internal or overlapping faces before importing.";
        return false;
    }
    if (sector.vertices.size() < 4 || sector.surfaces.size() < 4)
    {
        failure.error = "The mesh does not enclose a usable volume. A Sector needs at least four vertices and four faces.";
        return false;
    }
    if (!level::sectorIsConvex(sector))
    {
        failure.error = "Imported mesh is not a convex sector. Split it into convex objects/groups before importing.";
        return false;
    }
    return true;
}

bool appendOneSector(
    Level& level,
    const ImportedSectorMesh& mesh,
    std::int32_t& sectorIndex,
    ImportedSectorBuildResult& failure,
    const ImportedSectorBuildOptions& options)
{
    if (mesh.vertices.empty() || mesh.faces.empty())
    {
        failure.error = "Imported mesh has no geometry.";
        ImportedSectorDiagnostic diagnostic{};
        diagnostic.kind = ImportedSectorDiagnosticKind::emptyMesh;
        diagnostic.mesh = diagnosticMeshName(mesh.name);
        appendDiagnostic(failure, std::move(diagnostic));
        return false;
    }
    if (mesh.uvCoordinateSpace != UvCoordinateSpace::TexturePixels
        && mesh.uvCoordinateSpace != UvCoordinateSpace::Normalized)
    {
        failure.error = "Imported mesh has an unknown UV coordinate space.";
        ImportedSectorDiagnostic diagnostic{};
        diagnostic.kind = ImportedSectorDiagnosticKind::invalidPolygon;
        diagnostic.mesh = diagnosticMeshName(mesh.name);
        appendDiagnostic(failure, std::move(diagnostic));
        return false;
    }

    sectorIndex = static_cast<std::int32_t>(level.sectors.size());
    Sector& sector = level.newSector();
    sector.layer = 0;
    if (level.kind != ProjectType::IJIM)
    {
        sector.colormap = level.getMasterCMP();
    }
    sector.vertices.reserve(mesh.vertices.size());
    std::vector<std::int32_t> vertexRemap{};
    vertexRemap.reserve(mesh.vertices.size());
    for (std::size_t vertexIndex = 0;
         vertexIndex < mesh.vertices.size();
         ++vertexIndex)
    {
        const TVector& vertex = mesh.vertices[vertexIndex];
        if (!std::isfinite(vertex.x) || !std::isfinite(vertex.y) || !std::isfinite(vertex.z))
        {
            failure.error = "Imported mesh contains a non-finite vertex.";
            ImportedSectorDiagnostic diagnostic{};
            diagnostic.kind = ImportedSectorDiagnosticKind::nonFiniteVertex;
            diagnostic.mesh = diagnosticMeshName(mesh.name);
            diagnostic.firstVertexIndex = static_cast<std::int32_t>(vertexIndex);
            appendDiagnostic(failure, std::move(diagnostic));
            return false;
        }
        // addVertex welds coincident positions. Keep the returned mapping:
        // combined OBJ groups and 3DO meshes commonly repeat boundary
        // vertices even though their faces belong to one sector volume.
        vertexRemap.push_back(sector.addVertex(vertex.x, vertex.y, vertex.z));
    }

    sector.surfaces.reserve(mesh.faces.size());
    std::vector<bool> completeAuthoredUvs{};
    completeAuthoredUvs.reserve(mesh.faces.size());
    for (std::size_t faceIndex = 0;
         faceIndex < mesh.faces.size();
         ++faceIndex)
    {
        const ImportedSectorFace& importedFace = mesh.faces[faceIndex];
        Surface surface{};
        surface.material = importedFace.material.empty() ? "dflt.mat" : importedFace.material;
        surface.surfaceFlags = SF_Collision;
        surface.geo = static_cast<std::int32_t>(TGeoMode::Texture);
        surface.light = static_cast<std::int32_t>(TLightMode::Gouraud);
        surface.tex = level.kind == ProjectType::IJIM ? 3 : 1;
        surface.uScale = 1.0f;
        surface.vScale = 1.0f;
        surface.adjoinFlags = SAF_Visible | SAF_Move;
        if (level.kind != ProjectType::IJIM)
        {
            surface.adjoinFlags |= SAF_AllowSoundPass;
        }
        surface.vertices.reserve(importedFace.vertices.size());
        for (std::size_t cornerIndex = 0;
             cornerIndex < importedFace.vertices.size();
             ++cornerIndex)
        {
            const std::int32_t importedVertex =
                importedFace.vertices[cornerIndex];
            if (importedVertex < 0
                || static_cast<std::size_t>(importedVertex) >= vertexRemap.size())
            {
                failure.error =
                    "Imported polygon references an invalid vertex.";
                ImportedSectorDiagnostic diagnostic = makeFaceDiagnostic(
                    ImportedSectorDiagnosticKind::invalidVertexReference,
                    sourceFaceReference(
                        importedFace, mesh.name, faceIndex));
                diagnostic.cornerIndex = cornerIndex;
                diagnostic.firstVertexIndex = importedVertex;
                appendDiagnostic(failure, std::move(diagnostic));
                return false;
            }
            surface.vertices.push_back(vertexRemap[static_cast<std::size_t>(importedVertex)]);
        }
        surface.texVertices.resize(surface.vertices.size());
        bool completeUvs = importedFace.texVertices.size()
            == surface.vertices.size();
        std::optional<level::UvTextureDimensions> dimensions{};
        const bool conversionRequired = mesh.uvCoordinateSpace
            != level.uvCoordinateSpace;
        if (conversionRequired)
        {
            dimensions = options.materialTextureDimensions
                ? options.materialTextureDimensions(surface.material)
                : std::nullopt;
            if (!dimensions)
            {
                failure.error = "Cannot convert UVs for material '"
                    + surface.material
                    + "' because its texture dimensions are unavailable.";
                ImportedSectorDiagnostic diagnostic = makeFaceDiagnostic(
                    ImportedSectorDiagnosticKind::invalidPolygon,
                    sourceFaceReference(
                        importedFace, mesh.name, faceIndex));
                appendDiagnostic(failure, std::move(diagnostic));
                return false;
            }
        }
        for (std::size_t index = 0; index < surface.vertices.size(); ++index)
        {
            if (index < importedFace.texVertices.size())
            {
                const std::int32_t texVertex = importedFace.texVertices[index];
                if (texVertex >= 0 && static_cast<std::size_t>(texVertex) < mesh.texVertices.size())
                {
                    const TexVertex sourceUv =
                        mesh.texVertices[static_cast<std::size_t>(texVertex)];
                    if (!std::isfinite(sourceUv.u)
                        || !std::isfinite(sourceUv.v))
                    {
                        failure.error = "face "
                            + std::to_string(sourceFaceReference(
                                importedFace, mesh.name, faceIndex).faceIndex)
                            + " corner " + std::to_string(index)
                            + " has a non-finite or float-overflow texture coordinate.";
                        ImportedSectorDiagnostic diagnostic =
                            makeFaceDiagnostic(
                                ImportedSectorDiagnosticKind::invalidPolygon,
                                sourceFaceReference(
                                    importedFace, mesh.name, faceIndex));
                        diagnostic.cornerIndex = index;
                        appendDiagnostic(failure, std::move(diagnostic));
                        return false;
                    }
                    if (conversionRequired)
                    {
                        const std::optional<TexVertex> converted =
                            level::convertUvCoordinate(
                                sourceUv,
                                mesh.uvCoordinateSpace,
                                level.uvCoordinateSpace,
                                *dimensions);
                        if (!converted)
                        {
                            failure.error = "Imported UV cannot be represented for material '"
                                + surface.material + "'.";
                            ImportedSectorDiagnostic diagnostic =
                                makeFaceDiagnostic(
                                    ImportedSectorDiagnosticKind::invalidPolygon,
                                    sourceFaceReference(
                                        importedFace, mesh.name, faceIndex));
                            diagnostic.cornerIndex = index;
                            appendDiagnostic(failure, std::move(diagnostic));
                            return false;
                        }
                        surface.texVertices[index] = *converted;
                    }
                    else
                    {
                        surface.texVertices[index] = sourceUv;
                    }
                    continue;
                }
            }
            completeUvs = false;
        }
        sector.addSurface(std::move(surface));
        completeAuthoredUvs.push_back(completeUvs);
    }

    if (!orientAndValidateSector(sector, mesh, failure))
    {
        return false;
    }

    for (std::size_t surfaceIndex = 0;
         surfaceIndex < sector.surfaces.size();
         ++surfaceIndex)
    {
        Surface& surface = sector.surfaces[surfaceIndex];
        if (surfaceIndex < completeAuthoredUvs.size()
            && completeAuthoredUvs[surfaceIndex])
        {
            if (!level::surfaceFindUVScales(
                    level::levelPixelPerUnit(level),
                    sector,
                    surface))
            {
                const ImportedSectorSourceFace source =
                    sourceFaceReference(
                        mesh.faces[surfaceIndex],
                        mesh.name,
                        surfaceIndex);
                failure.error = "face "
                    + std::to_string(source.faceIndex)
                    + " has a non-finite or float-overflow UV scale.";
                appendDiagnostic(failure, makeFaceDiagnostic(
                    ImportedSectorDiagnosticKind::invalidPolygon,
                    source));
                return false;
            }
            if (!validateImportedSurfaceUvData(
                    surface,
                    mesh.faces[surfaceIndex],
                    mesh.name,
                    surfaceIndex,
                    failure))
            {
                return false;
            }
            continue;
        }

        if (level.uvCoordinateSpace != UvCoordinateSpace::TexturePixels
            && level.uvCoordinateSpace != UvCoordinateSpace::Normalized)
        {
            failure.error = "Cannot create default UVs because the Level uses an unknown UV coordinate space.";
            ImportedSectorDiagnostic diagnostic = makeFaceDiagnostic(
                ImportedSectorDiagnosticKind::invalidPolygon,
                sourceFaceReference(
                    mesh.faces[surfaceIndex], mesh.name, surfaceIndex));
            appendDiagnostic(failure, std::move(diagnostic));
            return false;
        }

        std::optional<level::UvTextureDimensions> dimensions{};
        if (level.uvCoordinateSpace == UvCoordinateSpace::Normalized)
        {
            dimensions =
                options.materialTextureDimensions
                ? options.materialTextureDimensions(surface.material)
                : std::nullopt;
            if (!dimensions
                || !std::isfinite(dimensions->width)
                || !std::isfinite(dimensions->height)
                || dimensions->width <= 0.0
                || dimensions->height <= 0.0
                || dimensions->width
                    > static_cast<double>(std::numeric_limits<float>::max())
                || dimensions->height
                    > static_cast<double>(std::numeric_limits<float>::max()))
            {
                failure.error = "Cannot create default UVs for material '"
                    + surface.material
                    + "' because its texture dimensions are unavailable or invalid.";
                ImportedSectorDiagnostic diagnostic = makeFaceDiagnostic(
                    ImportedSectorDiagnosticKind::invalidPolygon,
                    sourceFaceReference(
                        mesh.faces[surfaceIndex], mesh.name, surfaceIndex));
                appendDiagnostic(failure, std::move(diagnostic));
                return false;
            }
        }

        surface.texVertices.assign(surface.vertices.size(), TexVertex{});
        surface.uScale = 1.0f;
        surface.vScale = 1.0f;
        // Preserve the established default projection and its pixel-space
        // rounding, then normalize this newly generated face exactly once.
        // Complete authored OBJ/3DO UVs never enter this branch.
        level::surfaceCalcUV(
            level::levelPixelPerUnit(level),
            sector,
            surface,
            0);
        if (dimensions)
        {
            std::vector<TexVertex> normalizedCoordinates{};
            normalizedCoordinates.reserve(surface.texVertices.size());
            for (const TexVertex& coordinate : surface.texVertices)
            {
                const std::optional<TexVertex> normalized =
                    level::convertUvCoordinate(
                        coordinate,
                        UvCoordinateSpace::TexturePixels,
                        UvCoordinateSpace::Normalized,
                        *dimensions);
                if (!normalized)
                {
                    failure.error = "Cannot create default UVs for material '"
                        + surface.material
                        + "' because a generated coordinate cannot be normalized.";
                    ImportedSectorDiagnostic diagnostic = makeFaceDiagnostic(
                        ImportedSectorDiagnosticKind::invalidPolygon,
                        sourceFaceReference(
                            mesh.faces[surfaceIndex], mesh.name, surfaceIndex));
                    appendDiagnostic(failure, std::move(diagnostic));
                    return false;
                }
                normalizedCoordinates.push_back(*normalized);
            }
            surface.texVertices = std::move(normalizedCoordinates);
            surface.uScale = static_cast<float>(
                static_cast<double>(surface.uScale) / dimensions->width);
            surface.vScale = static_cast<float>(
                static_cast<double>(surface.vScale) / dimensions->height);
        }
        if (!validateImportedSurfaceUvData(
                surface,
                mesh.faces[surfaceIndex],
                mesh.name,
                surfaceIndex,
                failure))
        {
            return false;
        }
    }
    return true;
}

} // namespace

ImportedSectorMeshResult loadObjSectorMeshes(
    std::istream& input,
    const ObjSectorImportOptions options)
{
    ImportedSectorMeshResult result{};
    std::vector<TVector> vertices{};
    std::vector<TexVertex> texVertices{};
    std::string material{};
    result.meshes.push_back(ImportedSectorMesh{ "default" });
    std::optional<UvCoordinateSpace> declaredUvCoordinateSpace{};

    std::string line{};
    std::size_t lineNumber = 0;
    while (std::getline(input, line))
    {
        ++lineNumber;
        const std::string_view trimmedLine = text::trimView(line);
        std::optional<UvCoordinateSpace> lineUvCoordinateSpace{};
        if (text::iequalsAscii(trimmedLine, kObjPixelUvMarker))
        {
            lineUvCoordinateSpace = UvCoordinateSpace::TexturePixels;
        }
        else if (text::iequalsAscii(trimmedLine, kObjNormalizedUvMarker))
        {
            lineUvCoordinateSpace = UvCoordinateSpace::Normalized;
        }
        if (lineUvCoordinateSpace)
        {
            if (declaredUvCoordinateSpace
                && *declaredUvCoordinateSpace != *lineUvCoordinateSpace)
            {
                result.meshes.clear();
                result.error = "OBJ line " + std::to_string(lineNumber)
                    + ": contradictory SED texture-coordinate space markers.";
                return result;
            }
            declaredUvCoordinateSpace = lineUvCoordinateSpace;
        }
        if (const std::size_t comment = line.find('#'); comment != std::string::npos)
        {
            line.erase(comment);
        }
        const std::vector<std::string_view> words = text::splitWords(line);
        if (words.empty())
        {
            continue;
        }
        if (words[0] == "v" && words.size() >= 4)
        {
            const auto x = text::parseDouble(words[1]);
            const auto y = text::parseDouble(words[2]);
            const auto z = text::parseDouble(words[3]);
            if (!x || !y || !z)
            {
                result.error = "OBJ line " + std::to_string(lineNumber) + ": invalid vertex.";
                return result;
            }
            double homogeneousWeight = 1.0;
            if (words.size() == 5)
            {
                const auto parsedWeight = text::parseDouble(words[4]);
                if (!parsedWeight || !std::isfinite(*parsedWeight)
                    || *parsedWeight == 0.0)
                {
                    result.error = "OBJ line " + std::to_string(lineNumber)
                        + ": invalid homogeneous vertex weight.";
                    return result;
                }
                homogeneousWeight = *parsedWeight;
            }
            const TVector authored{
                *x / homogeneousWeight,
                *y / homogeneousWeight,
                *z / homogeneousWeight };
            if (options.convertYUpToSithZUp)
            {
                // Unlike a bare Y/Z swap this preserves handedness and polygon
                // winding. UVs remain in their authored 2D coordinate system.
                vertices.push_back(rotateObjYUpToSithZUp(authored));
            }
            else
            {
                vertices.push_back(authored);
            }
        }
        else if (words[0] == "vt" && words.size() >= 3)
        {
            const auto u = text::parseDouble(words[1]);
            const auto v = text::parseDouble(words[2]);
            if (!u || !v)
            {
                result.meshes.clear();
                result.error = "OBJ line " + std::to_string(lineNumber) + ": invalid texture coordinate.";
                return result;
            }
            if (!isRepresentableFloat(*u) || !isRepresentableFloat(*v))
            {
                result.meshes.clear();
                result.error = "OBJ line " + std::to_string(lineNumber)
                    + ": texture coordinate is non-finite or outside the supported float range.";
                return result;
            }
            TexVertex texVertex{};
            texVertex.u = static_cast<float>(*u);
            texVertex.v = static_cast<float>(*v);
            texVertices.push_back(texVertex);
        }
        else if ((words[0] == "o" || words[0] == "g") && words.size() >= 2)
        {
            const std::string name(objRecordArgument(line));
            if (result.meshes.back().faces.empty())
            {
                result.meshes.back().name = name;
            }
            else
            {
                result.meshes.push_back(ImportedSectorMesh{ name });
            }
        }
        else if (words[0] == "usemtl" && words.size() >= 2)
        {
            material = std::string(objRecordArgument(line));
        }
        else if (words[0] == "f" && words.size() >= 4)
        {
            ImportedSectorFace face{};
            face.material = material;
            face.sourceMesh = result.meshes.back().name;
            face.sourceFaceIndex = result.meshes.back().faces.size();
            face.vertices.reserve(words.size() - 1);
            face.texVertices.reserve(words.size() - 1);
            for (std::size_t i = 1; i < words.size(); ++i)
            {
                std::int32_t vertex{};
                std::int32_t texVertex{};
                if (!parseObjFaceToken(words[i], vertices.size(), texVertices.size(), vertex, texVertex))
                {
                    result.error = "OBJ line " + std::to_string(lineNumber) + ": invalid face index.";
                    return result;
                }
                face.vertices.push_back(vertex);
                face.texVertices.push_back(texVertex);
            }
            result.meshes.back().faces.push_back(std::move(face));
        }
    }

    result.meshes.erase(std::remove_if(result.meshes.begin(), result.meshes.end(), [](const ImportedSectorMesh& mesh)
        {
            return mesh.faces.empty();
        }), result.meshes.end());
    std::vector<ImportedSectorMesh> partitionedMeshes{};
    for (ImportedSectorMesh& mesh : result.meshes)
    {
        mesh.vertices = vertices;
        mesh.texVertices = texVertices;
        std::vector<ImportedSectorMesh> shells =
            partitionSequentialClosedShells(std::move(mesh));
        for (ImportedSectorMesh& shell : shells)
        {
            if (!compactImportedMeshIndices(shell, result.error))
            {
                return result;
            }
            shell.uvCoordinateSpace = declaredUvCoordinateSpace.value_or(
                UvCoordinateSpace::Normalized);
            if (shell.uvCoordinateSpace == UvCoordinateSpace::TexturePixels
                && options.materialTextureSize)
            {
                std::vector<TexVertex> normalizedTexVertices{};
                for (ImportedSectorFace& face : shell.faces)
                {
                    const std::string material = face.material.empty()
                        ? std::string("dflt.mat")
                        : face.material;
                    const std::optional<ObjSectorImportOptions::TextureSize>
                        textureSize = options.materialTextureSize(material);
                    if (!textureSize
                        || !std::isfinite(textureSize->width)
                        || !std::isfinite(textureSize->height)
                        || textureSize->width <= 0.0f
                        || textureSize->height <= 0.0f)
                    {
                        result.error = "Cannot convert legacy pixel-space OBJ UVs for material '"
                            + material
                            + "' because its texture dimensions are unavailable.";
                        return result;
                    }
                    for (std::int32_t& texVertexIndex : face.texVertices)
                    {
                        if (texVertexIndex < 0
                            || static_cast<std::size_t>(texVertexIndex)
                                >= shell.texVertices.size())
                        {
                            continue;
                        }
                        TexVertex texVertex = shell.texVertices[
                            static_cast<std::size_t>(texVertexIndex)];
                        const std::optional<TexVertex> converted =
                            level::convertUvCoordinate(
                                texVertex,
                                UvCoordinateSpace::TexturePixels,
                                UvCoordinateSpace::Normalized,
                                level::UvTextureDimensions{
                                    textureSize->width,
                                    textureSize->height });
                        if (!converted)
                        {
                            result.error = "A legacy pixel-space OBJ UV cannot be represented after normalization.";
                            return result;
                        }
                        texVertexIndex = static_cast<std::int32_t>(
                            normalizedTexVertices.size());
                        normalizedTexVertices.push_back(*converted);
                    }
                }
                shell.texVertices = std::move(normalizedTexVertices);
                shell.uvCoordinateSpace = UvCoordinateSpace::Normalized;
            }
            partitionedMeshes.push_back(std::move(shell));
        }
    }
    result.meshes = std::move(partitionedMeshes);
    if (result.meshes.empty())
    {
        result.error = "OBJ contains no polygon geometry.";
    }
    return result;
}

ImportedSectorMeshResult importedSectorMeshesFrom3do(const assets::Model3do& model)
{
    ImportedSectorMeshResult result{};
    if (!std::isfinite(model.version))
    {
        result.error = "3DO version is non-finite. Sector import supports only 2.1 and 2.3.";
        return result;
    }
    if (model.version == 0.0)
    {
        result.error = "3DO version is missing. Sector import supports only 2.1 and 2.3.";
        return result;
    }
    if (model.version != 2.1 && model.version != 2.3)
    {
        result.error = "3DO version " + formatDiagnosticNumber(model.version)
            + " is unsupported for Sector import; only 2.1 and 2.3 have defined UV units.";
        return result;
    }

    const UvCoordinateSpace modelUvCoordinateSpace =
        model.version == 2.3
        ? UvCoordinateSpace::Normalized
        : UvCoordinateSpace::TexturePixels;
    for (std::size_t nodeIndex = 0; nodeIndex < model.nodes.size(); ++nodeIndex)
    {
        const assets::Model3doNode& node = model.nodes[nodeIndex];
        if (node.mesh < 0 || static_cast<std::size_t>(node.mesh) >= model.meshes.size())
        {
            continue;
        }
        std::vector<bool> visited(model.nodes.size(), false);
        visited[nodeIndex] = true;
        std::int32_t parent = node.parent;
        while (parent != -1)
        {
            if (parent < 0 || static_cast<std::size_t>(parent) >= model.nodes.size())
            {
                result.error = "3DO node hierarchy references an invalid parent.";
                return result;
            }
            const std::size_t parentIndex = static_cast<std::size_t>(parent);
            if (visited[parentIndex])
            {
                result.error = "3DO node hierarchy contains a parent cycle.";
                return result;
            }
            visited[parentIndex] = true;
            parent = model.nodes[parentIndex].parent;
        }
    }

    const auto transformRawNodePoint = [&](
        TVector point,
        const assets::Model3doNode& node) noexcept
    {
        point = point + node.pivot;
        point = TMatrix43::CreateRotate(node.rotation).TransformVector(point);
        point = point + node.position;

        std::int32_t parent = node.parent;
        std::size_t depth = 0;
        while (parent >= 0
            && static_cast<std::size_t>(parent) < model.nodes.size()
            && depth < model.nodes.size())
        {
            const assets::Model3doNode& parentNode =
                model.nodes[static_cast<std::size_t>(parent)];
            point = TMatrix43::CreateRotate(parentNode.rotation).TransformVector(point);
            point = point + parentNode.position;
            parent = parentNode.parent;
            ++depth;
        }
        return point;
    };
    const auto appendModelMesh = [&](const assets::Model3doMesh& source,
        const std::string& name,
        const assets::Model3doNode* rawNode) -> bool
    {
        if (source.vertices.empty() || source.faces.empty())
        {
            return true;
        }
        ImportedSectorMesh mesh{};
        mesh.name = name.empty() ? source.name : name;
        mesh.uvCoordinateSpace = modelUvCoordinateSpace;
        mesh.vertices.reserve(source.vertices.size());
        for (const assets::Model3doVertex& vertex : source.vertices)
        {
            TVector point{ vertex.x, vertex.y, vertex.z };
            if (rawNode)
            {
                point = transformRawNodePoint(point, *rawNode);
            }
            mesh.vertices.push_back(point);
        }
        mesh.texVertices.reserve(source.texVertices.size());
        for (const assets::Model3doTexVertex& texVertex : source.texVertices)
        {
            TexVertex output{};
            output.u = texVertex.u;
            output.v = texVertex.v;
            mesh.texVertices.push_back(output);
        }
        for (std::size_t sourceFaceIndex = 0;
             sourceFaceIndex < source.faces.size();
             ++sourceFaceIndex)
        {
            const assets::Model3doFace& sourceFace =
                source.faces[sourceFaceIndex];
            if (sourceFace.vertices.size() < 3)
            {
                continue;
            }
            for (std::size_t cornerIndex = 0;
                 cornerIndex < sourceFace.texVertices.size();
                 ++cornerIndex)
            {
                const std::int32_t texVertexIndex =
                    sourceFace.texVertices[cornerIndex];
                if (texVertexIndex < 0
                    || static_cast<std::size_t>(texVertexIndex)
                        >= source.texVertices.size())
                {
                    continue;
                }
                const assets::Model3doTexVertex& texVertex =
                    source.texVertices[
                        static_cast<std::size_t>(texVertexIndex)];
                if (!std::isfinite(texVertex.u)
                    || !std::isfinite(texVertex.v))
                {
                    result.meshes.clear();
                    result.error = "3DO mesh '"
                        + diagnosticMeshName(mesh.name) + "' face "
                        + std::to_string(sourceFaceIndex) + " corner "
                        + std::to_string(cornerIndex)
                        + " has a non-finite or float-overflow texture coordinate.";
                    return false;
                }
            }
            ImportedSectorFace face{};
            face.vertices = sourceFace.vertices;
            face.texVertices = sourceFace.texVertices;
            face.sourceMesh = mesh.name;
            face.sourceFaceIndex = sourceFaceIndex;
            if (sourceFace.material >= 0
                && static_cast<std::size_t>(sourceFace.material) < model.materials.size())
            {
                face.material = model.materials[static_cast<std::size_t>(sourceFace.material)];
            }
            mesh.faces.push_back(std::move(face));
        }
        if (!mesh.faces.empty())
        {
            if (!compactImportedMeshIndices(mesh, result.error))
            {
                result.meshes.clear();
                return false;
            }
            result.meshes.push_back(std::move(mesh));
        }
        return true;
    };

    for (const assets::Model3doNode& node : model.nodes)
    {
        if (node.mesh < 0 || static_cast<std::size_t>(node.mesh) >= model.meshes.size())
        {
            continue;
        }
        if (!appendModelMesh(
                model.meshes[static_cast<std::size_t>(node.mesh)],
                node.name,
                model.hierarchyTransformsApplied ? nullptr : &node))
        {
            return result;
        }
    }
    if (model.nodes.empty())
    {
        for (std::size_t index = 0; index < model.meshes.size(); ++index)
        {
            if (!appendModelMesh(
                    model.meshes[index], model.meshes[index].name, nullptr))
            {
                return result;
            }
        }
    }
    if (result.meshes.empty())
    {
        result.error = "3DO contains no non-empty node mesh geometry.";
    }
    return result;
}

ImportedSectorBuildResult appendImportedSectors(
    Level& level,
    const std::span<const ImportedSectorMesh> meshes,
    const bool splitMeshes,
    const ImportedSectorBuildOptions& options)
{
    ImportedSectorBuildResult result{};
    if (meshes.empty())
    {
        result.error = "The selected file contains no importable geometry.";
        return result;
    }

    std::vector<ImportedSectorMesh> buildMeshes{};
    const auto withProvenance = [](const ImportedSectorMesh& source)
    {
        ImportedSectorMesh mesh = source;
        for (std::size_t faceIndex = 0;
             faceIndex < mesh.faces.size();
             ++faceIndex)
        {
            ImportedSectorFace& face = mesh.faces[faceIndex];
            if (face.sourceMesh.empty())
            {
                face.sourceMesh = source.name;
            }
            if (!face.sourceFaceIndex)
            {
                face.sourceFaceIndex = faceIndex;
            }
        }
        return mesh;
    };
    if (splitMeshes)
    {
        buildMeshes.reserve(meshes.size());
        for (const ImportedSectorMesh& mesh : meshes)
        {
            buildMeshes.push_back(withProvenance(mesh));
        }
    }
    else
    {
        ImportedSectorMesh combined{};
        combined.name = "Combined imported geometry";
        combined.uvCoordinateSpace = meshes.front().uvCoordinateSpace;
        for (const ImportedSectorMesh& mesh : meshes)
        {
            if (mesh.uvCoordinateSpace != combined.uvCoordinateSpace)
            {
                result.error = "Imported meshes use incompatible UV coordinate spaces.";
                return result;
            }
            appendMesh(combined, withProvenance(mesh));
        }
        buildMeshes.push_back(std::move(combined));
    }

    Level candidate = level;
    for (const ImportedSectorMesh& mesh : buildMeshes)
    {
        std::int32_t sectorIndex = -1;
        if (!appendOneSector(
                candidate,
                mesh,
                sectorIndex,
                result,
                options))
        {
            std::string errorContext = diagnosticMeshName(mesh.name);
            if (!splitMeshes && result.diagnostics.size() == 1
                && result.diagnostics.front().faceIndex)
            {
                errorContext = result.diagnostics.front().mesh;
            }
            if (!errorContext.empty())
            {
                result.error = errorContext + ": " + result.error;
            }
            result.details = formatImportedSectorDiagnostics(result);
            result.sectors.clear();
            return result;
        }
        result.sectors.push_back(sectorIndex);
    }

    if (splitMeshes)
    {
        for (std::size_t first = 0; first < result.sectors.size(); ++first)
        {
            for (std::size_t second = first + 1; second < result.sectors.size(); ++second)
            {
                (void)level::levelAdjoinSectors(
                    candidate, result.sectors[first], result.sectors[second]);
            }
        }
    }

    level = std::move(candidate);
    return result;
}

namespace
{

std::vector<std::int32_t> normalizedExportSectorIndices(
    const Level& level,
    const std::span<const std::int32_t> sectors,
    std::string& error)
{
    // Validate every selected face before either writer emits its header.
    // Missing per-corner UVs are malformed authored state, not permission to
    // manufacture zero texture coordinates at an interchange boundary.
    if (level.uvCoordinateSpace != UvCoordinateSpace::TexturePixels
        && level.uvCoordinateSpace != UvCoordinateSpace::Normalized)
    {
        error = "The Level UV coordinate space cannot be converted for export.";
        return {};
    }

    std::vector<std::int32_t> indices(sectors.begin(), sectors.end());
    std::sort(indices.begin(), indices.end());
    indices.erase(std::unique(indices.begin(), indices.end()), indices.end());
    if (indices.empty())
    {
        error = "Select at least one sector to export.";
        return {};
    }
    for (const std::int32_t index : indices)
    {
        if (index < 0 || static_cast<std::size_t>(index) >= level.sectors.size())
        {
            error = "The sector selection contains an invalid index.";
            return {};
        }
        const Sector& sector = level.sectors[static_cast<std::size_t>(index)];
        if (sector.vertices.empty() || sector.surfaces.empty())
        {
            error = "A selected sector contains no polygon geometry.";
            return {};
        }
        for (std::size_t faceIndex = 0;
             faceIndex < sector.surfaces.size();
             ++faceIndex)
        {
            const Surface& surface = sector.surfaces[faceIndex];
            if (surface.vertices.size() < 3)
            {
                error = "A selected sector contains an incomplete polygon.";
                return {};
            }
            if (surface.texVertices.size() != surface.vertices.size())
            {
                error = "Selected sector " + std::to_string(index)
                    + " face " + std::to_string(faceIndex)
                    + " must have one texture coordinate per polygon corner.";
                return {};
            }
            if (!std::isfinite(surface.uScale)
                || !std::isfinite(surface.vScale))
            {
                error = "Selected sector " + std::to_string(index)
                    + " face " + std::to_string(faceIndex)
                    + " has a non-finite or float-overflow UV scale.";
                return {};
            }
            for (const std::int32_t vertex : surface.vertices)
            {
                if (vertex < 0
                    || static_cast<std::size_t>(vertex) >= sector.vertices.size())
                {
                    error = "A selected sector contains an invalid polygon vertex index.";
                    return {};
                }
            }
            for (std::size_t cornerIndex = 0;
                 cornerIndex < surface.texVertices.size();
                 ++cornerIndex)
            {
                const TexVertex& coordinate =
                    surface.texVertices[cornerIndex];
                if (!std::isfinite(coordinate.u)
                    || !std::isfinite(coordinate.v))
                {
                    error = "Selected sector " + std::to_string(index)
                        + " face " + std::to_string(faceIndex)
                        + " corner " + std::to_string(cornerIndex)
                        + " has a non-finite or float-overflow texture coordinate.";
                    return {};
                }
            }
        }
    }
    return indices;
}

std::string safe3doName(const std::int32_t sectorIndex)
{
    return "sector_" + std::to_string(sectorIndex);
}

std::optional<TexVertex> convertedSurfaceUvForExport(
    const Level& level,
    const Surface& surface,
    const TexVertex& uv,
    const UvCoordinateSpace destination,
    const SectorExportOptions& options,
    std::unordered_map<std::string,
        std::optional<level::UvTextureDimensions>>& dimensionCache,
    std::string& error)
{
    if (level.uvCoordinateSpace == destination)
    {
        return uv;
    }
    if ((level.uvCoordinateSpace != UvCoordinateSpace::TexturePixels
            && level.uvCoordinateSpace != UvCoordinateSpace::Normalized)
        || !options.materialTextureDimensions)
    {
        error = "The Level UV coordinate space cannot be converted for export.";
        return std::nullopt;
    }

    const std::string material = surface.material.empty()
        ? std::string("dflt.mat")
        : surface.material;
    const std::string key = text::upperAscii(material);
    auto found = dimensionCache.find(key);
    if (found == dimensionCache.end())
    {
        found = dimensionCache.emplace(
            key,
            options.materialTextureDimensions(material)).first;
    }
    if (!found->second)
    {
        error = "Cannot export UVs for material '" + material
            + "' because its texture dimensions are unavailable.";
        return std::nullopt;
    }

    const std::optional<TexVertex> converted = level::convertUvCoordinate(
        uv,
        level.uvCoordinateSpace,
        destination,
        *found->second);
    if (!converted)
    {
        error = "A UV for material '" + material
            + "' cannot be represented in the export coordinate space.";
    }
    return converted;
}

} // namespace

ExportedSectorTextResult exportSectorsToObj(
    const Level& level,
    const std::span<const std::int32_t> sectors,
    const SectorExportOptions& options)
{
    ExportedSectorTextResult result{};
    const std::vector<std::int32_t> indices =
        normalizedExportSectorIndices(level, sectors, result.error);
    if (!result.error.empty())
    {
        return result;
    }

    std::ostringstream output{};
    output << std::setprecision(17)
           << "# SED selected sector export\n"
           << "# SED texture coordinates: normalized\n";
    std::unordered_map<std::string,
        std::optional<level::UvTextureDimensions>> dimensionCache{};
    std::size_t vertexBase = 1;
    std::size_t texVertexBase = 1;
    for (const std::int32_t sectorIndex : indices)
    {
        const Sector& sector = level.sectors[static_cast<std::size_t>(sectorIndex)];
        output << "\no " << safe3doName(sectorIndex) << '\n';
        for (const Vertex& vertex : sector.vertices)
        {
            output << "v " << vertex.x << ' ' << vertex.y << ' ' << vertex.z << '\n';
        }
        for (const Surface& surface : sector.surfaces)
        {
            for (std::size_t corner = 0; corner < surface.vertices.size(); ++corner)
            {
                const std::optional<TexVertex> converted =
                    convertedSurfaceUvForExport(
                        level,
                        surface,
                        surface.texVertices[corner],
                        UvCoordinateSpace::Normalized,
                        options,
                        dimensionCache,
                        result.error);
                if (!converted)
                {
                    return result;
                }
                const TexVertex& uv = *converted;
                output << "vt " << uv.u << ' ' << uv.v << '\n';
            }
        }

        std::size_t nextTexVertex = texVertexBase;
        std::string currentMaterial{};
        for (const Surface& surface : sector.surfaces)
        {
            const std::string material = surface.material.empty()
                ? std::string("dflt.mat") : surface.material;
            if (material != currentMaterial)
            {
                output << "usemtl " << material << '\n';
                currentMaterial = material;
            }
            output << 'f';
            for (std::size_t corner = 0; corner < surface.vertices.size(); ++corner)
            {
                output << ' '
                       << vertexBase + static_cast<std::size_t>(surface.vertices[corner])
                       << '/' << nextTexVertex++;
            }
            output << '\n';
        }
        vertexBase += sector.vertices.size();
        texVertexBase = nextTexVertex;
    }
    result.text = output.str();
    return result;
}

ExportedSectorTextResult exportSectorsTo3do(
    const Level& level,
    const std::span<const std::int32_t> sectors,
    const SectorExportOptions& options)
{
    ExportedSectorTextResult result{};
    const std::vector<std::int32_t> indices =
        normalizedExportSectorIndices(level, sectors, result.error);
    if (!result.error.empty())
    {
        return result;
    }

    std::vector<std::string> materials{};
    for (const std::int32_t sectorIndex : indices)
    {
        const Sector& sector = level.sectors[static_cast<std::size_t>(sectorIndex)];
        for (const Surface& surface : sector.surfaces)
        {
            const std::string material = surface.material.empty()
                ? std::string("dflt.mat") : surface.material;
            if (std::find(materials.begin(), materials.end(), material) == materials.end())
            {
                materials.push_back(material);
            }
        }
    }
    if (materials.empty())
    {
        materials.push_back("dflt.mat");
    }

    double modelRadius = 0.0;
    for (const std::int32_t sectorIndex : indices)
    {
        for (const Vertex& vertex : level.sectors[static_cast<std::size_t>(sectorIndex)].vertices)
        {
            modelRadius = std::max(modelRadius,
                std::sqrt(vertex.x * vertex.x + vertex.y * vertex.y + vertex.z * vertex.z));
        }
    }

    const UvCoordinateSpace wireUvCoordinateSpace =
        level.kind == ProjectType::IJIM
        ? UvCoordinateSpace::Normalized
        : UvCoordinateSpace::TexturePixels;
    const double modelVersion = level.kind == ProjectType::IJIM ? 2.3 : 2.1;
    std::unordered_map<std::string,
        std::optional<level::UvTextureDimensions>> dimensionCache{};

    std::ostringstream output{};
    output << std::setprecision(17)
           << "SECTION: HEADER\n3DO " << modelVersion << "\nEND\n\n"
           << "SECTION: MODELRESOURCE\nMATERIALS " << materials.size() << '\n';
    for (std::size_t material = 0; material < materials.size(); ++material)
    {
        output << material << ": " << materials[material] << '\n';
    }
    output << "END\n\nSECTION: GEOMETRYDEF\n"
           << "RADIUS " << modelRadius << "\nINSERT OFFSET 0 0 0\n"
           << "GEOSETS 1\nGEOSET 0\nMESHES " << indices.size() << '\n';

    for (std::size_t meshIndex = 0; meshIndex < indices.size(); ++meshIndex)
    {
        const std::int32_t sectorIndex = indices[meshIndex];
        const Sector& sector = level.sectors[static_cast<std::size_t>(sectorIndex)];
        std::size_t texVertexCount = 0;
        double meshRadius = 0.0;
        for (const Vertex& vertex : sector.vertices)
        {
            meshRadius = std::max(meshRadius,
                std::sqrt(vertex.x * vertex.x + vertex.y * vertex.y + vertex.z * vertex.z));
        }
        for (const Surface& surface : sector.surfaces)
        {
            texVertexCount += surface.vertices.size();
        }

        output << "\nMESH " << meshIndex
               << "\nNAME " << safe3doName(sectorIndex)
               << "\nRADIUS " << meshRadius
               << "\nGEOMETRYMODE 4\nLIGHTINGMODE 3\nTEXTUREMODE 3\n"
               << "VERTICES " << sector.vertices.size() << '\n';
        for (std::size_t vertexIndex = 0; vertexIndex < sector.vertices.size(); ++vertexIndex)
        {
            const Vertex& vertex = sector.vertices[vertexIndex];
            output << vertexIndex << ": " << vertex.x << ' ' << vertex.y << ' '
                   << vertex.z << " 1\n";
        }
        output << "TEXTURE VERTICES " << texVertexCount << '\n';
        std::size_t texVertexIndex = 0;
        for (const Surface& surface : sector.surfaces)
        {
            for (std::size_t corner = 0; corner < surface.vertices.size(); ++corner)
            {
                const std::optional<TexVertex> converted =
                    convertedSurfaceUvForExport(
                        level,
                        surface,
                        surface.texVertices[corner],
                        wireUvCoordinateSpace,
                        options,
                        dimensionCache,
                        result.error);
                if (!converted)
                {
                    return result;
                }
                const TexVertex& uv = *converted;
                output << texVertexIndex++ << ": " << uv.u << ' ' << uv.v << '\n';
            }
        }
        output << "FACES " << sector.surfaces.size() << '\n';
        texVertexIndex = 0;
        for (std::size_t faceIndex = 0; faceIndex < sector.surfaces.size(); ++faceIndex)
        {
            const Surface& surface = sector.surfaces[faceIndex];
            const std::string materialName = surface.material.empty()
                ? std::string("dflt.mat") : surface.material;
            const auto materialIt = std::find(
                materials.begin(), materials.end(), materialName);
            const std::size_t materialIndex = static_cast<std::size_t>(
                std::distance(materials.begin(), materialIt));
            output << faceIndex << ": " << materialIndex << " 0x0 "
                   << surface.geo << ' ' << surface.light << ' ' << surface.tex
                   << " 0 " << surface.vertices.size();
            for (const std::int32_t vertex : surface.vertices)
            {
                output << ' ' << vertex << ',' << texVertexIndex++;
            }
            output << '\n';
        }
        output << "FACE NORMALS\n";
        for (std::size_t faceIndex = 0; faceIndex < sector.surfaces.size(); ++faceIndex)
        {
            TVector normal = level::surfaceCalcNormalNewell(
                sector, sector.surfaces[faceIndex]);
            if (normal.Normalize() == 0.0)
            {
                normal = sector.surfaces[faceIndex].normal;
            }
            output << faceIndex << ": " << normal.x << ' ' << normal.y << ' '
                   << normal.z << '\n';
        }
    }
    output << "END\n\nSECTION: HIERARCHYDEF\nHIERARCHY NODES "
           << indices.size() << '\n';
    for (std::size_t meshIndex = 0; meshIndex < indices.size(); ++meshIndex)
    {
        output << meshIndex << ": 0 0x0 " << meshIndex
               << " -1 -1 -1 0 0 0 0 0 0 0 0 0 0 "
               << safe3doName(indices[meshIndex]) << '\n';
    }
    output << "END\n";
    result.text = output.str();
    return result;
}

} // namespace sed::ui::sector
