#include "render/overlays/thing_shape_overlay_utils.hpp"

#include "sed/level/thing.hpp"
#include "sed/text.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <unordered_set>

namespace sed::render::thing_overlay
{
namespace
{

std::optional<double> numericValue(const TplValue* value) noexcept
{
    if (!value)
    {
        return std::nullopt;
    }

    double parsed{};
    switch (value->type)
    {
    case ValueType::integer:
    case ValueType::flag:
    case ValueType::ptr:
        parsed = static_cast<double>(value->intValue);
        break;
    case ValueType::floating:
        parsed = value->floatValue;
        break;
    case ValueType::str:
    case ValueType::unknown:
        if (const std::optional<double> textValue = text::parseDouble(value->stringValue))
        {
            parsed = *textValue;
        }
        else
        {
            return std::nullopt;
        }
        break;
    case ValueType::vect:
    case ValueType::frame:
        return std::nullopt;
    }

    return std::isfinite(parsed) ? std::optional<double>{ parsed } : std::nullopt;
}

std::optional<std::int32_t> integerValue(const TplValue* value) noexcept
{
    const std::optional<double> parsed = numericValue(value);
    if (!parsed
        || std::trunc(*parsed) != *parsed
        || *parsed < static_cast<double>(std::numeric_limits<std::int32_t>::min())
        || *parsed > static_cast<double>(std::numeric_limits<std::int32_t>::max()))
    {
        return std::nullopt;
    }

    return static_cast<std::int32_t>(*parsed);
}

bool finite(const assets::Model3doVertex& vertex) noexcept
{
    return std::isfinite(vertex.x)
        && std::isfinite(vertex.y)
        && std::isfinite(vertex.z);
}

TMatrix43 composeAffine(const TMatrix43& parent, const TMatrix43& local) noexcept
{
    // TMatrix43's public operator* currently swaps `uvec` and `dvec` while
    // composing. Build the affine basis and translation explicitly here.
    TMatrix43 result{};
    result.rvec = parent.TransformVector(local.rvec);
    result.lvec = parent.TransformVector(local.lvec);
    result.uvec = parent.TransformVector(local.uvec);
    result.dvec = parent.TransformPoint(local.dvec);
    return result;
}

std::uint64_t edgeKey(const std::uint32_t first, const std::uint32_t second) noexcept
{
    const std::uint32_t low = std::min(first, second);
    const std::uint32_t high = std::max(first, second);
    return (static_cast<std::uint64_t>(low) << 32u) | high;
}

constexpr std::size_t kSphereTreeSegments = 24u;
constexpr std::size_t kSphereTreePointsPerMesh = kSphereTreeSegments * 6u;

constexpr std::array<std::pair<std::size_t, std::size_t>, 12> kBoundsEdges{{
    { 0, 1 }, { 1, 2 }, { 2, 3 }, { 3, 0 },
    { 4, 5 }, { 5, 6 }, { 6, 7 }, { 7, 4 },
    { 0, 4 }, { 1, 5 }, { 2, 6 }, { 3, 7 },
}};

} // namespace

CollisionDescription resolveCollisionDescription(
    const Level& level,
    const assets::Templates* templates,
    const Thing& thing) noexcept
{
    const std::optional<std::int32_t> collide = integerValue(
        level::thingFindValue(level, templates, thing, "collide"));
    if (!collide || *collide == 0)
    {
        return {};
    }

    if (*collide == 1)
    {
        const std::optional<double> radius = numericValue(
            level::thingFindValue(level, templates, thing, "size"));
        if (radius && *radius > 0.0)
        {
            return CollisionDescription{ CollisionShape::sphere, *radius, true };
        }
        return {};
    }

    if (*collide == 2 && level.kind == ProjectType::MOTS)
    {
        // MotS uses `treesize` for the broad-phase sphere. Narrow-phase tree
        // collision is one authored-radius sphere per active hierarchy mesh.
        const std::optional<double> treeSize = numericValue(
            level::thingFindValue(level, templates, thing, "treesize"));
        return CollisionDescription{
            CollisionShape::sphereTree,
            0.0,
            false,
            treeSize && *treeSize > 0.0 ? treeSize : std::nullopt,
        };
    }

    if (*collide == 3)
    {
        return CollisionDescription{ CollisionShape::face };
    }

    return {};
}

std::vector<TVector> makeSphereWirePoints(const double radius, const std::size_t segments)
{
    if (!std::isfinite(radius) || !(radius > 0.0) || segments < 3 || segments > 4096)
    {
        return {};
    }

    constexpr double kTau = 6.283185307179586476925286766559;
    std::vector<TVector> points{};
    points.reserve(segments * 6);
    for (std::size_t ring = 0; ring < 3; ++ring)
    {
        for (std::size_t segment = 0; segment < segments; ++segment)
        {
            const double angle0 = kTau * static_cast<double>(segment) / static_cast<double>(segments);
            const double angle1 = kTau * static_cast<double>(segment + 1) / static_cast<double>(segments);
            const double x0 = radius * std::cos(angle0);
            const double y0 = radius * std::sin(angle0);
            const double x1 = radius * std::cos(angle1);
            const double y1 = radius * std::sin(angle1);
            if (ring == 0)
            {
                points.emplace_back(x0, y0, 0.0);
                points.emplace_back(x1, y1, 0.0);
            }
            else if (ring == 1)
            {
                points.emplace_back(x0, 0.0, y0);
                points.emplace_back(x1, 0.0, y1);
            }
            else
            {
                points.emplace_back(0.0, x0, y0);
                points.emplace_back(0.0, x1, y1);
            }
        }
    }
    return points;
}

std::size_t appendSphereTreeWirePoints(
    std::vector<TVector>& out,
    const assets::Model3do& model,
    const TMatrix43& thingTransform,
    const double meshRadiusFactor)
{
    const std::size_t nodeCount = model.nodes.size();
    if (nodeCount == 0u || model.meshes.empty()
        || !std::isfinite(meshRadiusFactor) || !(meshRadiusFactor > 0.0))
    {
        return 0u;
    }

    const auto finiteVector = [](const TVector& value) noexcept
    {
        return std::isfinite(value.x)
            && std::isfinite(value.y)
            && std::isfinite(value.z);
    };
    const auto finiteMatrix = [](const TMatrix43& matrix) noexcept
    {
        return std::isfinite(matrix.rvec.x) && std::isfinite(matrix.rvec.y)
            && std::isfinite(matrix.rvec.z) && std::isfinite(matrix.lvec.x)
            && std::isfinite(matrix.lvec.y) && std::isfinite(matrix.lvec.z)
            && std::isfinite(matrix.uvec.x) && std::isfinite(matrix.uvec.y)
            && std::isfinite(matrix.uvec.z) && std::isfinite(matrix.dvec.x)
            && std::isfinite(matrix.dvec.y) && std::isfinite(matrix.dvec.z);
    };
    if (!finiteMatrix(thingTransform))
    {
        return 0u;
    }

    // Resolve each node transform iteratively so a malformed cyclic hierarchy
    // cannot recurse indefinitely. The matrices follow rdThing's static pose:
    // parent placement * T(-parent pivot) * node pose * T(node pivot).
    std::vector<TMatrix43> nodeTransforms(nodeCount);
    std::vector<std::uint8_t> state(nodeCount, 0u);
    std::vector<std::size_t> chain{};
    chain.reserve(nodeCount);
    for (std::size_t start = 0u; start < nodeCount; ++start)
    {
        if (state[start] != 0u)
        {
            continue;
        }

        chain.clear();
        std::size_t current = start;
        bool usable = true;

        while (true)
        {
            if (current >= nodeCount)
            {
                usable = false;
                break;
            }
            if (state[current] == 2u)
            {
                break;
            }
            if (state[current] == 3u || state[current] == 1u)
            {
                usable = false;
                break;
            }

            state[current] = 1u;
            chain.push_back(current);
            const std::int32_t parent = model.nodes[current].parent;
            if (parent == -1)
            {
                break;
            }
            if (parent < 0 || static_cast<std::size_t>(parent) >= nodeCount)
            {
                usable = false;
                break;
            }
            current = static_cast<std::size_t>(parent);
        }

        if (!usable)
        {
            for (const std::size_t nodeIndex : chain)
            {
                state[nodeIndex] = 3u;
            }
            continue;
        }

        for (auto nodeIt = chain.rbegin(); nodeIt != chain.rend(); ++nodeIt)
        {
            const std::size_t nodeIndex = *nodeIt;
            const assets::Model3doNode& node = model.nodes[nodeIndex];
            const std::int32_t parent = node.parent;
            TMatrix43 parentTransform = thingTransform;
            TMatrix43 localTransform = composeAffine(
                TMatrix43::Create(node.position, node.rotation),
                TMatrix43::CreateTranslate(node.pivot));

            if (parent >= 0)
            {
                const std::size_t parentIndex = static_cast<std::size_t>(parent);
                if (state[parentIndex] != 2u)
                {
                    usable = false;
                    break;
                }
                parentTransform = nodeTransforms[parentIndex];
                const TVector inverseParentPivot{
                    -model.nodes[parentIndex].pivot.x,
                    -model.nodes[parentIndex].pivot.y,
                    -model.nodes[parentIndex].pivot.z,
                };
                localTransform = composeAffine(
                    TMatrix43::CreateTranslate(inverseParentPivot),
                    localTransform);
            }

            nodeTransforms[nodeIndex] = composeAffine(parentTransform, localTransform);
            if (!finiteVector(node.position)
                || !finiteVector(node.rotation)
                || !finiteVector(node.pivot)
                || !finiteMatrix(nodeTransforms[nodeIndex]))
            {
                usable = false;
                break;
            }
            state[nodeIndex] = 2u;
        }

        if (!usable)
        {
            for (const std::size_t nodeIndex : chain)
            {
                state[nodeIndex] = 3u;
            }
        }
    }

    std::size_t appended = 0u;
    for (std::size_t nodeIndex = 0u; nodeIndex < nodeCount; ++nodeIndex)
    {
        if (state[nodeIndex] != 2u)
        {
            continue;
        }

        const std::int32_t meshIndex = model.nodes[nodeIndex].mesh;
        if (meshIndex < 0 || static_cast<std::size_t>(meshIndex) >= model.meshes.size())
        {
            continue;
        }
        double effectiveAuthoredRadius =
            model.meshes[static_cast<std::size_t>(meshIndex)].authoredRadius;
        if ((model.nodes[nodeIndex].flags & 0x2u) != 0u)
        {
            // MotS DrawHNode copies a flagged node's radius from the nearest
            // ancestor without the same flag. Guard malformed hierarchy data.
            std::int32_t ancestorIndex = model.nodes[nodeIndex].parent;
            std::size_t ancestorDepth = 0u;
            while (ancestorIndex >= 0
                && static_cast<std::size_t>(ancestorIndex) < nodeCount)
            {
                const assets::Model3doNode& ancestor =
                    model.nodes[static_cast<std::size_t>(ancestorIndex)];
                if ((ancestor.flags & 0x2u) == 0u)
                {
                    break;
                }
                if (ancestorDepth++ >= nodeCount)
                {
                    ancestorIndex = -1;
                    break;
                }
                ancestorIndex = ancestor.parent;
            }
            if (ancestorIndex >= 0 && static_cast<std::size_t>(ancestorIndex) < nodeCount)
            {
                const std::int32_t ancestorMesh =
                    model.nodes[static_cast<std::size_t>(ancestorIndex)].mesh;
                if (ancestorMesh >= 0
                    && static_cast<std::size_t>(ancestorMesh) < model.meshes.size())
                {
                    effectiveAuthoredRadius =
                        model.meshes[static_cast<std::size_t>(ancestorMesh)].authoredRadius;
                }
            }
        }
        const double radius = effectiveAuthoredRadius * meshRadiusFactor;
        std::vector<TVector> spherePoints = makeSphereWirePoints(radius, kSphereTreeSegments);
        if (spherePoints.empty())
        {
            continue;
        }

        const TVector center = nodeTransforms[nodeIndex].TransformPoint(TVector::zero);
        if (!finiteVector(center))
        {
            continue;
        }
        for (TVector& point : spherePoints)
        {
            point.x += center.x;
            point.y += center.y;
            point.z += center.z;
        }
        spherePoints = finiteWireSegments(spherePoints);
        if (spherePoints.size() != kSphereTreePointsPerMesh)
        {
            continue;
        }
        out.insert(out.end(), spherePoints.begin(), spherePoints.end());
        ++appended;
    }
    return appended;
}

std::vector<TVector> finiteWireSegments(const std::vector<TVector>& points)
{
    std::vector<TVector> result{};
    result.reserve(points.size());
    if ((points.size() % 2) != 0)
    {
        return result;
    }

    const auto representable = [](const TVector& point) noexcept
    {
        return std::isfinite(point.x)
            && std::isfinite(point.y)
            && std::isfinite(point.z)
            && std::isfinite(static_cast<float>(point.x))
            && std::isfinite(static_cast<float>(point.y))
            && std::isfinite(static_cast<float>(point.z));
    };
    for (std::size_t endpoint = 0; endpoint < points.size(); endpoint += 2)
    {
        if (representable(points[endpoint]) && representable(points[endpoint + 1]))
        {
            result.push_back(points[endpoint]);
            result.push_back(points[endpoint + 1]);
        }
    }
    return result;
}

std::optional<Bounds3d> modelLocalVertexBounds(const assets::Model3do& model) noexcept
{
    Bounds3d result{};
    bool haveVertex = false;
    for (const assets::Model3doMesh& mesh : model.meshes)
    {
        for (const assets::Model3doVertex& vertex : mesh.vertices)
        {
            if (!finite(vertex))
            {
                continue;
            }

            if (!haveVertex)
            {
                result.min = TVector{ vertex.x, vertex.y, vertex.z };
                result.max = result.min;
                haveVertex = true;
                continue;
            }

            result.min.x = std::min(result.min.x, vertex.x);
            result.min.y = std::min(result.min.y, vertex.y);
            result.min.z = std::min(result.min.z, vertex.z);
            result.max.x = std::max(result.max.x, vertex.x);
            result.max.y = std::max(result.max.y, vertex.y);
            result.max.z = std::max(result.max.z, vertex.z);
        }
    }
    return haveVertex ? std::optional<Bounds3d>{ result } : std::nullopt;
}

std::optional<Bounds3d> spriteQuadWorldBounds(
    const std::array<TVector, 4>& corners) noexcept
{
    const auto representable = [](const double value) noexcept
    {
        return std::isfinite(value) && std::isfinite(static_cast<float>(value));
    };

    Bounds3d bounds{};
    for (std::size_t index = 0; index < corners.size(); ++index)
    {
        const TVector& corner = corners[index];
        if (!representable(corner.x)
            || !representable(corner.y)
            || !representable(corner.z))
        {
            return std::nullopt;
        }

        if (index == 0u)
        {
            bounds.min = corner;
            bounds.max = corner;
            continue;
        }

        bounds.min.x = std::min(bounds.min.x, corner.x);
        bounds.min.y = std::min(bounds.min.y, corner.y);
        bounds.min.z = std::min(bounds.min.z, corner.z);
        bounds.max.x = std::max(bounds.max.x, corner.x);
        bounds.max.y = std::max(bounds.max.y, corner.y);
        bounds.max.z = std::max(bounds.max.z, corner.z);
    }
    return bounds;
}

std::optional<double> spriteBillboardConservativeRadius(
    const double width,
    const double height,
    const std::array<double, 3>& offset) noexcept
{
    if (!std::isfinite(width) || !std::isfinite(height)
        || !(width > 0.0) || !(height > 0.0)
        || !std::isfinite(offset[0])
        || !std::isfinite(offset[1])
        || !std::isfinite(offset[2]))
    {
        return std::nullopt;
    }

    const double offsetRadius = std::hypot(offset[0], offset[1], offset[2]);
    const double halfDiagonal = std::hypot(width * 0.5, height * 0.5);
    const double radius = offsetRadius + halfDiagonal;
    return std::isfinite(radius) && radius > 0.0
        ? std::optional<double>{ radius }
        : std::nullopt;
}

std::array<TVector, 24> makeBoundsWirePoints(
    const Bounds3d& bounds,
    const TMatrix43& transform) noexcept
{
    const std::array<TVector, 8> corners{{
        transform.TransformPoint(TVector{ bounds.min.x, bounds.min.y, bounds.min.z }),
        transform.TransformPoint(TVector{ bounds.max.x, bounds.min.y, bounds.min.z }),
        transform.TransformPoint(TVector{ bounds.max.x, bounds.max.y, bounds.min.z }),
        transform.TransformPoint(TVector{ bounds.min.x, bounds.max.y, bounds.min.z }),
        transform.TransformPoint(TVector{ bounds.min.x, bounds.min.y, bounds.max.z }),
        transform.TransformPoint(TVector{ bounds.max.x, bounds.min.y, bounds.max.z }),
        transform.TransformPoint(TVector{ bounds.max.x, bounds.max.y, bounds.max.z }),
        transform.TransformPoint(TVector{ bounds.min.x, bounds.max.y, bounds.max.z }),
    }};

    std::array<TVector, 24> points{};
    std::size_t output = 0;
    for (const auto& [first, second] : kBoundsEdges)
    {
        points[output++] = corners[first];
        points[output++] = corners[second];
    }
    return points;
}

void appendModelFaceWirePoints(
    std::vector<TVector>& out,
    const assets::Model3do& model,
    const TMatrix43& transform)
{
    for (const assets::Model3doMesh& mesh : model.meshes)
    {
        std::unordered_set<std::uint64_t> seenEdges{};
        for (const assets::Model3doFace& face : mesh.faces)
        {
            if (face.vertices.size() < 2)
            {
                continue;
            }

            for (std::size_t corner = 0; corner < face.vertices.size(); ++corner)
            {
                const std::int32_t first = face.vertices[corner];
                const std::int32_t second = face.vertices[(corner + 1) % face.vertices.size()];
                if (first < 0 || second < 0
                    || static_cast<std::size_t>(first) >= mesh.vertices.size()
                    || static_cast<std::size_t>(second) >= mesh.vertices.size()
                    || first == second)
                {
                    continue;
                }

                const std::uint64_t key = edgeKey(
                    static_cast<std::uint32_t>(first),
                    static_cast<std::uint32_t>(second));
                if (!seenEdges.insert(key).second)
                {
                    continue;
                }

                const assets::Model3doVertex& a = mesh.vertices[static_cast<std::size_t>(first)];
                const assets::Model3doVertex& b = mesh.vertices[static_cast<std::size_t>(second)];
                if (!finite(a) || !finite(b))
                {
                    continue;
                }
                out.push_back(transform.TransformPoint(TVector{ a.x, a.y, a.z }));
                out.push_back(transform.TransformPoint(TVector{ b.x, b.y, b.z }));
            }
        }
    }
}

std::vector<DrawRange> selectedThingOverlayRanges(
    const std::unordered_set<std::int32_t>& selectedThingIndices,
    const std::int32_t activeThingIndex,
    const std::vector<std::uint32_t>& thingPickIds,
    const std::unordered_map<int, std::vector<DrawRange>>& rangesByPickId,
    const int composedVertexOffset)
{
    if (composedVertexOffset < 0 || thingPickIds.empty())
    {
        return {};
    }

    std::vector<std::int32_t> ownerIndices{};
    ownerIndices.reserve(selectedThingIndices.size() + 1u);
    for (const std::int32_t index : selectedThingIndices)
    {
        ownerIndices.push_back(index);
    }
    if (activeThingIndex >= 0 && !selectedThingIndices.contains(activeThingIndex))
    {
        ownerIndices.push_back(activeThingIndex);
    }
    std::sort(ownerIndices.begin(), ownerIndices.end());

    std::vector<DrawRange> result{};
    const auto maxPickId = static_cast<std::uint32_t>(std::numeric_limits<int>::max());
    for (const std::int32_t ownerIndex : ownerIndices)
    {
        if (ownerIndex < 0 || static_cast<std::size_t>(ownerIndex) >= thingPickIds.size())
        {
            continue;
        }

        const std::uint32_t pickId = thingPickIds[static_cast<std::size_t>(ownerIndex)];
        if (pickId == 0u || pickId > maxPickId)
        {
            continue;
        }
        const auto rangesIt = rangesByPickId.find(static_cast<int>(pickId));
        if (rangesIt == rangesByPickId.end())
        {
            continue;
        }

        for (const DrawRange& range : rangesIt->second)
        {
            if (range.first < 0 || range.count <= 0
                || range.first > std::numeric_limits<int>::max() - composedVertexOffset)
            {
                continue;
            }
            result.push_back(DrawRange{
                composedVertexOffset + range.first,
                range.count,
            });
        }
    }
    return result;
}

} // namespace sed::render::thing_overlay
