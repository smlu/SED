#pragma once

#include "extrude_arrangement_registry.hpp"
#include "sed/level.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <utility>
#include <vector>

namespace sed::level::detail
{

// Mesh indices belong to one trial; only the immutable registry is shared by
// retries. A failed trial therefore cannot leave a cached index into a later
// owner's unrelated vertex pool. No pointer into a movable profile is stored.
class ExtrudeArrangementContext
{
public:
    using Registry = ExtrudeArrangementRegistry;
    using NodeId = Registry::NodeId;
    using LineId = Registry::LineId;

    ExtrudeArrangementContext(Registry& registry, const std::int32_t source,
        const TVector& normal, const TVector& anchor)
        : registry_(&registry), source_(source), normal_(normal), anchor_(anchor)
    {
    }

    bool addCell(const Level& level, const SurfaceRef ref)
    {
        const auto& owner = level.sectors.at(static_cast<std::size_t>(ref.sector));
        const auto& face = owner.surfaces.at(static_cast<std::size_t>(ref.surface));
        if ( face.vertices.size() < 3 || face.vertices.size() != face.texVertices.size()
            || (ref.sector == source_ && face.vertices.size() != 3) )
        {
            return false;
        }
        Cell cell;
        std::vector<NodeId> corners;
        for ( std::size_t corner = 0; corner < face.vertices.size(); ++corner )
        {
            const auto index = face.vertices[corner];
            if ( index < 0 || static_cast<std::size_t>(index) >= owner.vertices.size() )
            {
                return false;
            }
            const TVector world = precision::position(owner.vertices[index]);
            const TVector projected = world - normal_ * normal_.dot(world - anchor_);
            auto found = meshNodes_.find({ref.sector,index});
            NodeId node = Registry::kInvalidNode;
            if ( found != meshNodes_.end() )
            {
                node = found->second;
            }
            else
            {
                // Exact cross-owner coincidence may share an identity. Two
                // distinct authored source indices never merge by distance.
                if ( ref.sector != source_ )
                {
                    for ( const auto& [original, position] : originalWorld_ )
                    {
                        (void)position;
                        const auto& point = registry_->point(original);
                        if ( point.x == projected.x && point.y == projected.y && point.z == projected.z )
                        {
                            if ( node != Registry::kInvalidNode )
                            {
                                return false;
                            }
                            node = original;
                        }
                    }
                }
                if ( node == Registry::kInvalidNode )
                {
                    node = registry_->addOriginal(projected,world);
                }
                if ( node == Registry::kInvalidNode )
                {
                    return false;
                }
                meshNodes_.emplace(std::pair{ref.sector,index}, node);
                ownerIndices_.emplace(std::pair{ref.sector,node}, index);
            }
            corners.push_back(node);
            if ( ref.sector == source_ )
            {
                originalWorld_.emplace(node, world);
                sourceWorld_.emplace(node, world);
                cell.originalNodes.push_back(node);
                cell.originalTuples.push_back(face.texVertices[corner]);
            }
        }
        for ( std::size_t corner = 0; corner < corners.size(); ++corner )
        {
            const auto line = registry_->addLine(corners[corner], corners[(corner + 1) % corners.size()]);
            if ( line == Registry::kInvalidLine )
            {
                return false;
            }
            cell.lines.push_back(line);
        }
        cell.originalLines = cell.lines;
        cells_.emplace(key(ref), std::move(cell));
        return true;
    }

    NodeId node(const std::int32_t owner, const std::int32_t index) const noexcept
    {
        const auto found = meshNodes_.find({owner,index});
        return found == meshNodes_.end() ? Registry::kInvalidNode : found->second;
    }

    LineId edgeLine(const SurfaceRef ref, const std::int32_t first, const std::int32_t second) const
    {
        const auto found = cells_.find(key(ref));
        const auto a = node(ref.sector,first), b = node(ref.sector,second);
        if ( found == cells_.end() || a == Registry::kInvalidNode || b == Registry::kInvalidNode || a == b )
        {
            return Registry::kInvalidLine;
        }
        for ( const auto line : found->second.lines )
        {
            if ( registry_->incident(a,line) && registry_->incident(b,line) )
            {
                return line;
            }
        }
        return Registry::kInvalidLine;
    }

    const Registry::Line& line(const LineId id) const
    {
        return registry_->line(id);
    }

    bool isOriginalNode(const NodeId id) const noexcept
    {
        return registry_->isOriginalNode(id);
    }

    std::optional<int> exactNodeOrderOnLine(const NodeId first, const NodeId second,
        const LineId common) const noexcept
    {
        return registry_->exactNodeOrderOnLine(first,second,common);
    }

    std::optional<int> exactBoundaryOrientation(const NodeId first, const NodeId second,
        const LineId common, const TVector& boundaryNormal) const noexcept
    {
        const auto order = exactNodeOrderOnLine(first,second,common);
        const double alignment = boundaryNormal.dot(normal_);
        if ( !order || !std::isfinite(alignment) || alignment == 0.0 )
        {
            return std::nullopt;
        }
        return *order == 0 ? std::optional<int>{0}
            : std::optional<int>{*order * (alignment < 0.0 ? -1 : 1)};
    }

    std::optional<int> boundaryOrientation(const NodeId first, const NodeId second,
        const LineId common, const TVector& boundaryNormal) const noexcept
    {
        const auto order = registry_->nodeOrderOnLine(first,second,common);
        const double alignment = boundaryNormal.dot(normal_);
        if ( !order || !std::isfinite(alignment) || alignment == 0.0 )
        {
            return std::nullopt;
        }
        return *order == 0 ? std::optional<int>{0}
            : std::optional<int>{*order * (alignment < 0.0 ? -1 : 1)};
    }

    void setCut(const LineId id, const int orientation = 1) noexcept
    {
        cut_ = id;
        orientation_ = orientation;
    }

    bool tracksSource(const SurfaceRef ref) const noexcept
    {
        return ref.sector == source_ && cells_.find(key(ref)) != cells_.end();
    }

    bool isSource(const SurfaceRef ref) const noexcept
    {
        return ref.sector == source_;
    }

    int classify(const std::int32_t owner, const std::int32_t index)
    {
        const auto id = node(owner,index);
        if ( id == Registry::kInvalidNode || cut_ == Registry::kInvalidLine )
        {
            return 2;
        }
        const auto side = registry_->classify(id,cut_);
        return side ? orientation_*(*side) : 2;
    }

    void splitFace(const SurfaceRef original, const SurfaceRef added)
    {
        auto& cell = cells_.at(key(original));
        if ( std::find(cell.lines.begin(),cell.lines.end(),cut_) == cell.lines.end() )
        {
            cell.lines.push_back(cut_);
        }
        cells_.emplace(key(added),cell);
    }

    std::optional<std::int32_t> cutVertex(Sector& owner, const SurfaceRef ref,
        const std::int32_t first, const std::int32_t second)
    {
        const auto edge = edgeLine(ref,first,second);
        if ( edge == Registry::kInvalidLine )
        {
            return std::nullopt;
        }
        const auto id = registry_->intersection(edge,cut_);
        if ( !id )
        {
            return std::nullopt;
        }
        if ( const auto found = ownerIndices_.find({ref.sector,*id}); found != ownerIndices_.end() )
        {
            return found->second;
        }
        auto position = registry_->point(*id);
        if ( ref.sector == source_ )
        {
            const auto world = sourcePosition(*id,cells_.at(key(ref)));
            if ( !world )
            {
                return std::nullopt;
            }
            position = *world;
        }
        const auto index = owner.newVertex(position.x,position.y,position.z);
        meshNodes_.emplace(std::pair{ref.sector,index},*id);
        ownerIndices_.emplace(std::pair{ref.sector,*id},index);
        return index;
    }

    std::optional<TexVertex> sourceTuple(const SurfaceRef ref, const std::int32_t index) const
    {
        if ( ref.sector != source_ )
        {
            return std::nullopt;
        }
        const auto& cell = cells_.at(key(ref));
        const auto id = node(ref.sector,index);
        for ( std::size_t corner = 0; corner < cell.originalNodes.size(); ++corner )
        {
            if ( id == cell.originalNodes[corner] )
            {
                return cell.originalTuples[corner];
            }
        }
        const auto weights = triangleWeights(id,cell);
        if ( !weights )
        {
            return std::nullopt;
        }
        const auto base = static_cast<std::size_t>(std::max_element(weights->begin(),weights->end())-weights->begin());
        const auto second = (base+1)%3, third = (base+2)%3;
        const auto interpolate = [&](const float a, const float b, const float c)
        {
            return static_cast<float>(std::fma((*weights)[second],static_cast<double>(b)-a,
                std::fma((*weights)[third],static_cast<double>(c)-a,static_cast<double>(a))));
        };
        const auto& a = cell.originalTuples[base];
        const auto& b = cell.originalTuples[second];
        const auto& c = cell.originalTuples[third];
        TexVertex result = a;
        result.u = interpolate(a.u,b.u,c.u);
        result.v = interpolate(a.v,b.v,c.v);
        result.color = {interpolate(a.color.r,b.color.r,c.color.r),interpolate(a.color.g,b.color.g,c.color.g),
            interpolate(a.color.b,b.color.b,c.color.b),interpolate(a.color.a,b.color.a,c.color.a)};
        if ( !std::isfinite(result.u) || !std::isfinite(result.v)
            || !std::isfinite(result.color.r) || !std::isfinite(result.color.g)
            || !std::isfinite(result.color.b) || !std::isfinite(result.color.a) )
        {
            return std::nullopt;
        }
        return result;
    }

    TVector projected(const std::int32_t owner, const std::int32_t index) const
    {
        return registry_->point(node(owner,index));
    }

    std::optional<TVector> sourcePosition(const SurfaceRef ref, const NodeId id)
    {
        const auto found = cells_.find(key(ref));
        if ( ref.sector != source_ || found == cells_.end() )
        {
            return std::nullopt;
        }
        return sourcePosition(id,found->second);
    }

    bool interiorNodeOnEdge(const SurfaceRef ref, const std::int32_t first,
        const std::int32_t second, const NodeId id) const
    {
        const auto line = edgeLine(ref,first,second);
        if ( line == Registry::kInvalidLine || !registry_->incident(id,line) )
        {
            return false;
        }
        const auto a = projected(ref.sector,first);
        const auto b = projected(ref.sector,second);
        const auto delta = b-a;
        const auto point = registry_->point(id);
        const double amount = std::abs(delta.x) >= std::abs(delta.y) && std::abs(delta.x) >= std::abs(delta.z)
            ? (point.x-a.x)/delta.x : std::abs(delta.y) >= std::abs(delta.z)
            ? (point.y-a.y)/delta.y : (point.z-a.z)/delta.z;
        return std::isfinite(amount) && amount > 0.0 && amount < 1.0;
    }

    std::int32_t materializeNode(Sector& owner, const std::int32_t ownerIndex,
        const NodeId id, const TVector& point)
    {
        if ( const auto found = ownerIndices_.find({ownerIndex,id}); found != ownerIndices_.end() )
        {
            return found->second;
        }
        const auto index = owner.newVertex(point.x,point.y,point.z);
        meshNodes_.emplace(std::pair{ownerIndex,index},id);
        ownerIndices_.emplace(std::pair{ownerIndex,id},index);
        return index;
    }

private:
    struct Cell
    {
        std::vector<LineId> lines;
        std::vector<LineId> originalLines;
        std::vector<NodeId> originalNodes;
        std::vector<TexVertex> originalTuples;
    };
    static std::pair<std::int32_t,std::int32_t> key(const SurfaceRef ref) noexcept
    {
        return {ref.sector,ref.surface};
    }

    std::optional<std::array<double,3>> triangleWeights(const NodeId id, const Cell& cell) const
    {
        if ( cell.originalNodes.size() != 3 || id == Registry::kInvalidNode )
        {
            return std::nullopt;
        }
        for ( std::size_t edge = 0; edge < 3; ++edge )
        {
            if ( !registry_->incident(id,cell.originalLines[edge]) )
            {
                continue;
            }
            const auto next = (edge+1)%3;
            const auto first = registry_->point(cell.originalNodes[edge]);
            const auto second = registry_->point(cell.originalNodes[next]);
            const auto delta = second-first;
            const auto point = registry_->point(id);
            const auto component = [&](const TVector& value)
            {
                return std::abs(delta.x) >= std::abs(delta.y) && std::abs(delta.x) >= std::abs(delta.z)
                    ? value.x : std::abs(delta.y) >= std::abs(delta.z) ? value.y : value.z;
            };
            const double firstDistance = component(point)-component(first);
            const double secondDistance = component(second)-component(point);
            const double length = component(delta);
            if ( length == 0.0 || !std::isfinite(length) )
            {
                return std::nullopt;
            }
            std::array<double,3> weights{};
            if ( std::abs(firstDistance) <= std::abs(secondDistance) )
            {
                weights[next] = firstDistance/length;
                weights[edge] = 1.0-weights[next];
            }
            else
            {
                weights[edge] = secondDistance/length;
                weights[next] = 1.0-weights[edge];
            }
            if ( !std::isfinite(weights[edge]) || !std::isfinite(weights[next]) )
            {
                return std::nullopt;
            }
            return weights;
        }
        const auto& a = registry_->point(cell.originalNodes[0]);
        const auto ab = registry_->point(cell.originalNodes[1]) - a;
        const auto ac = registry_->point(cell.originalNodes[2]) - a;
        const auto ap = registry_->point(id) - a;
        const double scale = std::max({std::abs(ab.x),std::abs(ab.y),std::abs(ab.z),
            std::abs(ac.x),std::abs(ac.y),std::abs(ac.z)});
        if ( !(scale > 0.0) || !std::isfinite(scale) )
        {
            return std::nullopt;
        }
        const TVector b{ab.x/scale,ab.y/scale,ab.z/scale};
        const TVector c{ac.x/scale,ac.y/scale,ac.z/scale};
        const TVector p{ap.x/scale,ap.y/scale,ap.z/scale};
        const double denominator = b.cross(c).dot(normal_);
        const double v = p.cross(c).dot(normal_) / denominator;
        const double w = b.cross(p).dot(normal_) / denominator;
        if ( !std::isfinite(v) || !std::isfinite(w) || denominator == 0.0 )
        {
            return std::nullopt;
        }
        return std::array<double,3>{{1.0-v-w,v,w}};
    }

    std::optional<TVector> sourcePosition(const NodeId id, const Cell& cell)
    {
        // Callers are either a crossing of a descendant edge of this ear, or
        // an exactly matched generated cell. The same node can consequently
        // reach another ear only on their common immutable source boundary;
        // its shared-edge evaluation below is independent of visit order.
        if ( const auto found = sourceWorld_.find(id); found != sourceWorld_.end() )
        {
            return found->second;
        }
        for ( const auto lineId : cell.originalLines )
        {
            if ( !registry_->incident(id,lineId) )
            {
                continue;
            }
            const auto& line = registry_->line(lineId);
            const auto firstNode = std::min(line.firstNode,line.secondNode);
            const auto secondNode = std::max(line.firstNode,line.secondNode);
            const auto& firstProjected = registry_->point(firstNode);
            const auto& secondProjected = registry_->point(secondNode);
            const auto delta = secondProjected-firstProjected;
            const auto component = [&](const TVector& point)
            {
                return std::abs(delta.x) >= std::abs(delta.y) && std::abs(delta.x) >= std::abs(delta.z)
                    ? point.x : std::abs(delta.y) >= std::abs(delta.z) ? point.y : point.z;
            };
            const double length = component(delta);
            const double fromFirst = component(registry_->point(id))-component(firstProjected);
            const double fromSecond = component(secondProjected)-component(registry_->point(id));
            if ( !std::isfinite(length) || length == 0.0 )
            {
                return std::nullopt;
            }
            const bool nearSecond = std::abs(fromSecond) < std::abs(fromFirst);
            const double amount = (nearSecond ? fromSecond : fromFirst)/length;
            if ( !std::isfinite(amount) || amount < 0.0 || amount > 1.0 )
            {
                return std::nullopt;
            }
            const auto& first = originalWorld_.at(nearSecond ? secondNode : firstNode);
            const auto& second = originalWorld_.at(nearSecond ? firstNode : secondNode);
            const auto interpolate = [amount](const double a, const double b)
            {
                if ( a == b )
                {
                    return a;
                }
                const double difference = b-a;
                const double value = std::fma(amount,difference,a);
                return std::isfinite(difference) && std::isfinite(value) ? value : std::lerp(a,b,amount);
            };
            const TVector world{interpolate(first.x,second.x),interpolate(first.y,second.y),
                interpolate(first.z,second.z)};
            if ( !precision::finite(world) )
            {
                return std::nullopt;
            }
            // Projected identity belongs to the shared registry. World points
            // on an original edge instead come directly from its immutable
            // endpoints, preserving constant components without height
            // subtraction/addition cancellation (for example an exact z=0).
            sourceWorld_.emplace(id,world);
            return world;
        }
        const auto weights = triangleWeights(id,cell);
        if ( !weights )
        {
            return std::nullopt;
        }
        const auto base = static_cast<std::size_t>(std::max_element(weights->begin(),weights->end())-weights->begin());
        const auto second = (base+1)%3, third = (base+2)%3;
        const auto& a = originalWorld_.at(cell.originalNodes[base]);
        const auto& b = originalWorld_.at(cell.originalNodes[second]);
        const auto& c = originalWorld_.at(cell.originalNodes[third]);
        // Keep the one canonical projected coordinate. Only the inherited
        // height comes from the immutable original source triangle.
        const double heightA = normal_.dot(a-anchor_);
        const double height = std::fma((*weights)[second],normal_.dot(b-a),
            std::fma((*weights)[third],normal_.dot(c-a),heightA));
        const auto world = registry_->point(id) + normal_*height;
        if ( !precision::finite(world) )
        {
            return std::nullopt;
        }
        sourceWorld_.emplace(id,world);
        return world;
    }

    Registry* registry_;
    std::int32_t source_;
    TVector normal_;
    TVector anchor_;
    LineId cut_{Registry::kInvalidLine};
    int orientation_{1};
    std::map<std::pair<std::int32_t,std::int32_t>,NodeId> meshNodes_;
    std::map<std::pair<std::int32_t,NodeId>,std::int32_t> ownerIndices_;
    std::map<std::pair<std::int32_t,std::int32_t>,Cell> cells_;
    std::map<NodeId,TVector> originalWorld_;
    std::map<NodeId,TVector> sourceWorld_;
};

} // namespace sed::level::detail
