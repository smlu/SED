#include "render/mesh/thing_render_geometry_utils.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <utility>

#include "thing/thing_resource_utils.hpp"

namespace sed::render
{
namespace
{

constexpr std::pair<std::size_t, std::size_t> kThingBoxEdges[12] = {
    {0, 1},
    {1, 2},
    {2, 3},
    {3, 0},
    {4, 5},
    {5, 6},
    {6, 7},
    {7, 4},
    {0, 4},
    {1, 5},
    {2, 6},
    {3, 7},
};

constexpr std::size_t kThingCubeFaces[6][4] = {
    {4, 5, 6, 7},
    {1, 0, 3, 2},
    {1, 2, 6, 5},
    {0, 4, 7, 3},
    {3, 7, 6, 2},
    {0, 1, 5, 4},
};

ThingWirePoint makeWirePoint(const TVector& point, const float pickId) noexcept
{
    return ThingWirePoint{
        point.x,
        point.y,
        point.z,
        pickId,
    };
}

std::array<ThingWirePoint, 24> makeWirePointsFromCorners(
    const std::array<TVector, 8>& corners,
    const float pickId) noexcept
{
    std::array<ThingWirePoint, 24> out{};
    std::size_t write = 0;

    // Each edge contributes two line-list vertices in the renderer's expected order.
    for (const auto& [a, b] : kThingBoxEdges)
    {
        out[write++] = makeWirePoint(corners[a], pickId);
        out[write++] = makeWirePoint(corners[b], pickId);
    }

    return out;
}

std::pair<float, float> cubeUvForCorner(
    const std::size_t faceIndex,
    const TVector& local,
    const double denom) noexcept
{
    static const std::array<TVector, 6> kNormal = {
        TVector{0.0, 0.0, 1.0},   // +Z
        TVector{0.0, 0.0, -1.0},  // -Z
        TVector{1.0, 0.0, 0.0},   // +X
        TVector{-1.0, 0.0, 0.0},  // -X
        TVector{0.0, 1.0, 0.0},   // +Y
        TVector{0.0, -1.0, 0.0},  // -Y
    };

    // Side faces keep +Z as icon up; top/bottom faces use +Y for top-down consistency.
    static const std::array<TVector, 6> kUp = {
        TVector{0.0, 1.0, 0.0},
        TVector{0.0, 1.0, 0.0},
        TVector{0.0, 0.0, 1.0},
        TVector{0.0, 0.0, 1.0},
        TVector{0.0, 0.0, 1.0},
        TVector{0.0, 0.0, 1.0},
    };

    const TVector& n = kNormal[faceIndex];
    const TVector& vAxis = kUp[faceIndex];
    const TVector uAxis = vAxis.Cross(n); // right = up x normal

    const double u = (local.Dot(uAxis) / denom) + 0.5;
    const double v = (local.Dot(vAxis) / denom) + 0.5;
    return {
        static_cast<float>(std::clamp(u, 0.0, 1.0)),
        static_cast<float>(std::clamp(v, 0.0, 1.0)),
    };
}

bool transformModelVertex(
    const assets::Model3doMesh& mesh,
    const std::int32_t index,
    const TMatrix43& xf,
    TVector& out) noexcept
{
    if (index < 0 || static_cast<std::size_t>(index) >= mesh.vertices.size())
    {
        return false;
    }

    const assets::Model3doVertex& v = mesh.vertices[static_cast<std::size_t>(index)];
    out = xf.TransformPoint(TVector{ v.x, v.y, v.z });
    return true;
}

} // namespace

std::array<TVector, 8> localThingCubeCorners(const double halfSize) noexcept
{
    const double h = halfSize;
    return {
        TVector{-h, -h, -h},
        TVector{ h, -h, -h},
        TVector{ h,  h, -h},
        TVector{-h,  h, -h},
        TVector{-h, -h,  h},
        TVector{ h, -h,  h},
        TVector{ h,  h,  h},
        TVector{-h,  h,  h},
    };
}

std::array<TVector, 8> transformedThingBoxCorners(const TMatrix43& xf, const ThingBox& box) noexcept
{
    const double x1 = box.x1;
    const double x2 = box.x2;
    const double y1 = box.y1;
    const double y2 = box.y2;
    const double z1 = box.z1;
    const double z2 = box.z2;

    return {
        xf.TransformPoint(TVector{ x1, y1, z1 }),
        xf.TransformPoint(TVector{ x2, y1, z1 }),
        xf.TransformPoint(TVector{ x2, y2, z1 }),
        xf.TransformPoint(TVector{ x1, y2, z1 }),
        xf.TransformPoint(TVector{ x1, y1, z2 }),
        xf.TransformPoint(TVector{ x2, y1, z2 }),
        xf.TransformPoint(TVector{ x2, y2, z2 }),
        xf.TransformPoint(TVector{ x1, y2, z2 }),
    };
}

std::array<TVector, 8> transformedThingCubeCorners(const TMatrix43& xf, const double halfSize) noexcept
{
    const std::array<TVector, 8> localCorners = localThingCubeCorners(halfSize);
    std::array<TVector, 8> out{};
    for (std::size_t i = 0; i < localCorners.size(); ++i)
    {
        out[i] = xf.TransformPoint(localCorners[i]);
    }
    return out;
}

ThingBox resolveThingRenderBox(
    const Level& level,
    const assets::Templates* templates,
    const Thing& thing) noexcept
{
    const std::string_view tplName = !thing.templateName.empty()
        ? std::string_view(thing.templateName)
        : std::string_view(thing.name);
    if (const std::optional<ThingBox> box =
            thing::templateThingBox(level, templates, tplName))
    {
        return *box;
    }
    return thing.bbox;
}

std::optional<std::size_t> wireframeSurfacePlaneReferenceVertexIndex(
    const Sector& sector,
    const Surface& surface,
    const std::size_t boundaryEdgeIndex) noexcept
{
    const std::size_t vertexCount = surface.vertices.size();
    if (vertexCount < 3 || boundaryEdgeIndex >= vertexCount)
    {
        return std::nullopt;
    }

    const std::size_t nextEdgeIndex = (boundaryEdgeIndex + 1u) % vertexCount;
    const std::int32_t edgeA = surface.vertices[boundaryEdgeIndex];
    const std::int32_t edgeB = surface.vertices[nextEdgeIndex];
    if (edgeA < 0 || edgeB < 0 || edgeA == edgeB
        || static_cast<std::size_t>(edgeA) >= sector.vertices.size()
        || static_cast<std::size_t>(edgeB) >= sector.vertices.size())
    {
        return std::nullopt;
    }

    // Level faces are filled by triangulatePolyZigZag(). Match this boundary
    // edge to the exact triangle that rasterizes beside it; a different corner
    // on a warped legacy polygon can define a different plane and cause its
    // wire overlay to fail depth testing at certain camera angles.
    std::size_t triangleA = 0;
    std::size_t triangleB = 1;
    std::size_t triangleC = vertexCount - 1u;
    for (std::size_t triangle = 0; triangle < vertexCount - 2u; ++triangle)
    {
        const std::array<std::size_t, 3> localIndices{
            triangleA, triangleB, triangleC };
        bool hasA = false;
        bool hasB = false;
        std::optional<std::size_t> thirdLocal{};
        for (const std::size_t localIndex : localIndices)
        {
            if (localIndex >= vertexCount)
            {
                return std::nullopt;
            }
            if (localIndex == boundaryEdgeIndex && !hasA)
            {
                hasA = true;
            }
            else if (localIndex == nextEdgeIndex && !hasB)
            {
                hasB = true;
            }
            else
            {
                thirdLocal = localIndex;
            }
        }
        if (hasA && hasB && thirdLocal)
        {
            const std::int32_t referenceIndex = surface.vertices[*thirdLocal];
            if (referenceIndex < 0
                || referenceIndex == edgeA
                || referenceIndex == edgeB
                || static_cast<std::size_t>(referenceIndex) >= sector.vertices.size())
            {
                return std::nullopt;
            }
            return static_cast<std::size_t>(referenceIndex);
        }

        if ((triangle & 1u) == 0u)
        {
            triangleA = triangleB;
            ++triangleB;
        }
        else
        {
            triangleA = triangleC;
            --triangleC;
        }
    }
    return std::nullopt;
}

std::array<ThingWirePoint, 24> makeThingBoxWirePoints(
    const TMatrix43& xf,
    const ThingBox& box,
    const float pickId) noexcept
{
    return makeWirePointsFromCorners(transformedThingBoxCorners(xf, box), pickId);
}

std::array<ThingWirePoint, 24> makeThingCubeWirePoints(
    const TMatrix43& xf,
    const double halfSize,
    const float pickId) noexcept
{
    return makeWirePointsFromCorners(transformedThingCubeCorners(xf, halfSize), pickId);
}

std::array<ThingCubeFaceVertex, 36> makeThingCubeFaceVertices(
    const TMatrix43& xf,
    const double halfSize) noexcept
{
    const std::array<TVector, 8> localCorners = localThingCubeCorners(halfSize);
    const std::array<TVector, 8> corners = transformedThingCubeCorners(xf, halfSize);
    const double denom = std::max(2.0 * halfSize, 1e-9);

    std::array<ThingCubeFaceVertex, 36> out{};
    std::size_t write = 0;

    auto pushCorner = [&](const std::size_t faceIndex, const std::size_t cornerIndex) noexcept
    {
        const auto [u, v] = cubeUvForCorner(faceIndex, localCorners[cornerIndex], denom);
        out[write++] = ThingCubeFaceVertex{
            corners[cornerIndex],
            u,
            v,
        };
    };

    for (std::size_t f = 0; f < 6; ++f)
    {
        const std::size_t c0 = kThingCubeFaces[f][0];
        const std::size_t c1 = kThingCubeFaces[f][1];
        const std::size_t c2 = kThingCubeFaces[f][2];
        const std::size_t c3 = kThingCubeFaces[f][3];

        // Two triangles per quad face, keeping the renderer's original winding.
        pushCorner(f, c0);
        pushCorner(f, c1);
        pushCorner(f, c2);
        pushCorner(f, c0);
        pushCorner(f, c2);
        pushCorner(f, c3);
    }

    return out;
}

void appendModelWireframeWirePoints(
    std::vector<ThingWirePoint>& out,
    const assets::Model3do& model,
    const TMatrix43& xf,
    const float pickId)
{
    for (const assets::Model3doMesh& mesh : model.meshes)
    {
        for (const assets::Model3doFace& face : mesh.faces)
        {
            const std::size_t count = face.vertices.size();
            if (count < 2)
            {
                continue;
            }

            // Walk the polygon perimeter and keep any edge with two valid endpoints.
            for (std::size_t i = 0; i < count; ++i)
            {
                const std::int32_t aIndex = face.vertices[i];
                const std::int32_t bIndex = face.vertices[(i + 1) % count];

                TVector a{};
                TVector b{};
                if (!transformModelVertex(mesh, aIndex, xf, a) || !transformModelVertex(mesh, bIndex, xf, b))
                {
                    continue;
                }

                out.push_back(makeWirePoint(a, pickId));
                out.push_back(makeWirePoint(b, pickId));
            }
        }
    }
}

} // namespace sed::render
