#pragma once

#include "geometry_precision.hpp"
#include "exact_orientation.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <map>
#include <optional>
#include <utility>
#include <vector>

namespace sed::level::detail
{

// Transaction-local identities. No spatial lookup merges authored corners.
// Geometry failure leaves published identities unchanged; allocation failures
// propagate to the enclosing candidate transaction, which discards this object.
class ExtrudeArrangementRegistry
{
public:
    using NodeId = std::size_t;
    using LineId = std::size_t;
    static constexpr NodeId kInvalidNode = std::numeric_limits<NodeId>::max();
    static constexpr LineId kInvalidLine = std::numeric_limits<LineId>::max();

    struct Line
    {
        NodeId firstNode{};
        NodeId secondNode{};
        TVector first;
        TVector second;
        TVector plane;
    };

    ExtrudeArrangementRegistry(const TVector& unitNormal, const TVector& anchor)
        : normal_(unitNormal), anchor_(anchor)
    {
        const double length = std::hypot(normal_.x, normal_.y, normal_.z);
        if ( !precision::finite(normal_) || !precision::finite(anchor_)
            || !std::isfinite(length) || std::abs(length - 1.0) > precision::kRoundoff )
        {
            return;
        }
        const TVector reference = std::abs(normal_.x) <= std::abs(normal_.y) && std::abs(normal_.x) <= std::abs(normal_.z)
            ? TVector{1,0,0} : std::abs(normal_.y) <= std::abs(normal_.z) ? TVector{0,1,0} : TVector{0,0,1};
        axisU_ = normal_.cross(reference);
        const double axisLength = std::hypot(axisU_.x, axisU_.y, axisU_.z);
        axisU_ = {axisU_.x / axisLength, axisU_.y / axisLength, axisU_.z / axisLength};
        axisV_ = normal_.cross(axisU_);
        valid_ = precision::finite(axisU_) && precision::finite(axisV_);
    }

    bool valid() const noexcept
    {
        return valid_;
    }
    const TVector& point(const NodeId node) const
    {
        return nodes_.at(node).point;
    }
    const Line& line(const LineId id) const
    {
        return lines_.at(id).geometry;
    }

    NodeId addOriginal(const TVector& point)
    {
        return addOriginalInternal(point,std::nullopt);
    }

    // Preserve the immutable source operand alongside its projected registry
    // point. The registry recomputes the projection from its own frame before
    // accepting the certificate, so a caller cannot attach provenance from a
    // different affine projection.
    NodeId addOriginal(const TVector& point, const TVector& sourceWorld)
    {
        if ( !valid_ || !precision::finite(sourceWorld) )
        {
            return kInvalidNode;
        }
        const auto projected = projectedSourcePoint(sourceWorld);
        if ( !projected || !samePointExact(point,*projected) )
        {
            return kInvalidNode;
        }
        return addOriginalInternal(point,sourceWorld);
    }

    LineId addLine(const NodeId a, const NodeId b)
    {
        if ( a >= nodes_.size() || b >= nodes_.size() || a == b || !nodes_[a].original || !nodes_[b].original )
        {
            return kInvalidLine;
        }
        const std::pair<NodeId,NodeId> key = std::minmax(a, b);
        if ( const auto found = endpointLines_.find(key); found != endpointLines_.end() )
        {
            return found->second;
        }
        const Point2 delta{nodes_[b].local.x - nodes_[a].local.x, nodes_[b].local.y - nodes_[a].local.y};
        const double scale = std::max(std::abs(delta.x), std::abs(delta.y));
        if ( !finite(delta) || !(scale > 0.0) || !std::isfinite(scale) )
        {
            return kInvalidLine;
        }
        const Point2 direction{delta.x / scale, delta.y / scale};
        const Point2 error{
            (nodes_[a].error.x + nodes_[b].error.x + precision::kRoundoff * std::abs(delta.x)) / scale
                + precision::kRoundoff * std::abs(direction.x),
            (nodes_[a].error.y + nodes_[b].error.y + precision::kRoundoff * std::abs(delta.y)) / scale
                + precision::kRoundoff * std::abs(direction.y)};
        TVector plane = (axisU_ * direction.x + axisV_ * direction.y).cross(normal_);
        const double planeLength = std::hypot(plane.x, plane.y, plane.z);
        if ( !finite(error) || !(planeLength > 0.0) || !std::isfinite(planeLength) )
        {
            return kInvalidLine;
        }
        plane = {plane.x / planeLength, plane.y / planeLength, plane.z / planeLength};
        const LineId id = lines_.size();
        lines_.push_back({{a,b,nodes_[a].point,nodes_[b].point,plane}, direction, error});
        generatedIntersectionNeighbors_.emplace_back();
        projectedSupportRepresentatives_.push_back(id);
        for ( const LineId representative : projectedSupportClassRepresentatives_ )
        {
            const auto exact = projectedSupportsAreExactlyCoincident(id,representative);
            if ( exact && *exact )
            {
                projectedSupportRepresentatives_.back() = representative;
                break;
            }
        }
        if ( projectedSupportRepresentatives_.back() == id )
        {
            projectedSupportClassRepresentatives_.push_back(id);
        }
        sourceSupportRepresentatives_.push_back(kInvalidLine);
        if ( lineHasSourceSupport(id) )
        {
            sourceSupportRepresentatives_.back() = id;
            for ( const LineId representative : sourceSupportClassRepresentatives_ )
            {
                const auto exact = sourceSupportsAreExactlyCoincident(id,representative);
                if ( exact && *exact )
                {
                    sourceSupportRepresentatives_.back() = representative;
                    break;
                }
            }
            if ( sourceSupportRepresentatives_.back() == id )
            {
                sourceSupportClassRepresentatives_.push_back(id);
            }
        }
        endpointLines_.emplace(key, id);
        for ( const auto node : {a,b} )
        {
            for ( const auto other : nodes_[node].lines )
            {
                cacheIntersection(id,other,node);
            }
            nodes_[node].lines.push_back(id);
        }
        return id;
    }

    bool incident(const NodeId node, const LineId id) const noexcept
    {
        return node < nodes_.size() && id < lines_.size()
            && std::find(nodes_[node].lines.begin(), nodes_[node].lines.end(), id) != nodes_[node].lines.end();
    }

    bool isOriginalNode(const NodeId node) const noexcept
    {
        return node < nodes_.size() && nodes_[node].original;
    }

    // Return the sign used by a boundary direction test for the ordered pair
    // of nodes on one immutable support. A positive result means that the
    // second node is forward from the first in the support's stored endpoint
    // direction. Generated nodes are ordered from their immutable defining
    // supports, because their reconstructed coordinates can round to the
    // same point even when their exact crossings are distinct.
    std::optional<int> exactNodeOrderOnLine(const NodeId first, const NodeId second,
        const LineId common) const noexcept
    {
        if ( first >= nodes_.size() || second >= nodes_.size() || common >= lines_.size()
            || first == second )
        {
            return std::nullopt;
        }
        const auto& commonLine = lines_[common].geometry;
        if ( commonLine.firstNode >= nodes_.size() || commonLine.secondNode >= nodes_.size()
            || commonLine.firstNode == commonLine.secondNode )
        {
            return std::nullopt;
        }
        const auto& commonStart = nodes_[commonLine.firstNode].local;
        const auto& commonFinish = nodes_[commonLine.secondNode].local;
        const auto onCommonSupport = [&](const NodeId node)
        {
            const auto side = exactOrientationSign(commonStart.x,commonStart.y,
                commonFinish.x,commonFinish.y,nodes_[node].local.x,nodes_[node].local.y);
            return side && *side == 0;
        };
        const auto definingOther = [&](const NodeId node) -> std::optional<LineId>
        {
            if ( node >= nodes_.size() || nodes_[node].original || nodes_[node].lines.size() < 2 )
            {
                return std::nullopt;
            }
            const auto firstDefining = nodes_[node].lines[0];
            const auto secondDefining = nodes_[node].lines[1];
            if ( firstDefining >= lines_.size() || secondDefining >= lines_.size()
                || firstDefining == secondDefining )
            {
                return std::nullopt;
            }
            const auto generated = intersections_.find(std::minmax(firstDefining,secondDefining));
            if ( generated == intersections_.end() || generated->second != node )
            {
                return std::nullopt;
            }
            if ( firstDefining == common )
            {
                return secondDefining;
            }
            if ( secondDefining == common )
            {
                return firstDefining;
            }
            return std::nullopt;
        };
        const bool firstGenerated = !nodes_[first].original;
        const bool secondGenerated = !nodes_[second].original;
        if ( !firstGenerated && !secondGenerated )
        {
            if ( !onCommonSupport(first) || !onCommonSupport(second) )
            {
                return std::nullopt;
            }
            return exactDirectionDotSign(nodes_[first].local.x,nodes_[first].local.y,
                nodes_[second].local.x,nodes_[second].local.y,
                commonStart.x,commonStart.y,commonFinish.x,commonFinish.y);
        }

        const auto firstOther = firstGenerated ? definingOther(first) : std::nullopt;
        const auto secondOther = secondGenerated ? definingOther(second) : std::nullopt;
        const auto lineEndpoints = [&](const LineId id)
        {
            const auto& geometry = lines_[id].geometry;
            const auto& start = nodes_[geometry.firstNode].local;
            const auto& finish = nodes_[geometry.secondNode].local;
            return std::array<double,4>{{start.x,start.y,finish.x,finish.y}};
        };
        if ( firstGenerated && secondGenerated )
        {
            if ( !firstOther || !secondOther )
            {
                return std::nullopt;
            }
            const auto firstSupport = lineEndpoints(*firstOther);
            const auto secondSupport = lineEndpoints(*secondOther);
            const auto side = exactIntersectionSideSign(
                commonStart.x,commonStart.y,commonFinish.x,commonFinish.y,
                firstSupport[0],firstSupport[1],firstSupport[2],firstSupport[3],
                secondSupport[0],secondSupport[1],secondSupport[2],secondSupport[3]);
            const auto direction = exactDirectionCrossSign(
                secondSupport[0],secondSupport[1],secondSupport[2],secondSupport[3],
                commonStart.x,commonStart.y,commonFinish.x,commonFinish.y);
            if ( !side || !direction || *direction == 0 )
            {
                return std::nullopt;
            }
            // side is orient(secondSupport, firstIntersection). Multiplying
            // by the second support's direction relative to the common line
            // converts that side into the ordered parameter difference.
            return *side == 0 ? std::optional<int>{0}
                : std::optional<int>{-*side * *direction};
        }
        if ( firstGenerated )
        {
            if ( !firstOther || !onCommonSupport(second) )
            {
                return std::nullopt;
            }
            const auto support = lineEndpoints(*firstOther);
            const auto side = exactOrientationSign(support[0],support[1],support[2],support[3],
                nodes_[second].local.x,nodes_[second].local.y);
            const auto direction = exactDirectionCrossSign(
                support[0],support[1],support[2],support[3],
                commonStart.x,commonStart.y,commonFinish.x,commonFinish.y);
            if ( !side || !direction || *direction == 0 )
            {
                return std::nullopt;
            }
            // side is orient(firstSupport, secondOriginal), so its product
            // with the support/common direction gives t(second)-t(first).
            return *side == 0 ? std::optional<int>{0}
                : std::optional<int>{*side * *direction};
        }
        if ( !secondOther || !onCommonSupport(first) )
        {
            return std::nullopt;
        }
        const auto support = lineEndpoints(*secondOther);
        const auto side = exactOrientationSign(support[0],support[1],support[2],support[3],
            nodes_[first].local.x,nodes_[first].local.y);
        const auto direction = exactDirectionCrossSign(
            support[0],support[1],support[2],support[3],
            commonStart.x,commonStart.y,commonFinish.x,commonFinish.y);
        if ( !side || !direction || *direction == 0 )
        {
            return std::nullopt;
        }
        // side is orient(secondSupport, firstOriginal), hence the negative
        // product gives t(secondGenerated)-t(firstOriginal).
        return *side == 0 ? std::optional<int>{0}
            : std::optional<int>{-*side * *direction};
    }

    // Certify the directed order of two nodes which are already incident to
    // one immutable support. The exact provenance path above remains the
    // authority whenever it can decide the order; this interval path only
    // accepts a sign when the stored local coordinates and normalized support
    // direction leave no possible zero or opposite result.
    std::optional<int> certifiedNodeOrderOnLine(const NodeId first, const NodeId second,
        const LineId common) const noexcept
    {
        if ( first >= nodes_.size() || second >= nodes_.size() || common >= lines_.size()
            || first == second || !incident(first,common) || !incident(second,common) )
        {
            return std::nullopt;
        }
        const auto& firstNode = nodes_[first];
        const auto& secondNode = nodes_[second];
        const auto& support = lines_[common];
        if ( !finite(firstNode.local) || !finite(secondNode.local)
            || !finite(firstNode.error) || !finite(secondNode.error)
            || !finite(support.direction) || !finite(support.error)
            || firstNode.error.x < 0.0 || firstNode.error.y < 0.0
            || secondNode.error.x < 0.0 || secondNode.error.y < 0.0
            || support.error.x < 0.0 || support.error.y < 0.0 )
        {
            return std::nullopt;
        }
        const double scale = std::max({std::abs(firstNode.local.x), std::abs(firstNode.local.y),
            std::abs(secondNode.local.x), std::abs(secondNode.local.y)});
        if ( !std::isfinite(scale) || !(scale > 0.0) )
        {
            return std::nullopt;
        }
        const Point2 firstScaled{firstNode.local.x / scale, firstNode.local.y / scale};
        const Point2 secondScaled{secondNode.local.x / scale, secondNode.local.y / scale};
        const Point2 delta{secondScaled.x-firstScaled.x,secondScaled.y-firstScaled.y};
        const auto outwardNonnegative = [](const double value) -> std::optional<double>
        {
            if ( !std::isfinite(value) || value < 0.0 )
            {
                return std::nullopt;
            }
            // Every operation here bounds an uncertainty, rather than a
            // designer-facing distance. Moving a finite nonnegative result
            // one representable step outward preserves terms which would
            // otherwise underflow independently to zero.
            const double result = std::nextafter(value,std::numeric_limits<double>::infinity());
            return std::isfinite(result) ? std::optional<double>{result} : std::nullopt;
        };
        const auto outwardAdd = [&](const double first, const double second) -> std::optional<double>
        {
            if ( !std::isfinite(first) || !std::isfinite(second) || first < 0.0 || second < 0.0 )
            {
                return std::nullopt;
            }
            return outwardNonnegative(first + second);
        };
        const auto outwardMultiply = [&](const double first, const double second) -> std::optional<double>
        {
            if ( !std::isfinite(first) || !std::isfinite(second) || first < 0.0 || second < 0.0 )
            {
                return std::nullopt;
            }
            return outwardNonnegative(first * second);
        };
        const auto outwardDivide = [&](const double numerator, const double denominator)
            -> std::optional<double>
        {
            if ( !std::isfinite(numerator) || !std::isfinite(denominator)
                || numerator < 0.0 || !(denominator > 0.0) )
            {
                return std::nullopt;
            }
            return outwardNonnegative(numerator / denominator);
        };
        const auto addBound = [&](std::optional<double>& sum, const std::optional<double>& term)
        {
            if ( !sum || !term )
            {
                sum.reset();
                return;
            }
            sum = outwardAdd(*sum,*term);
        };
        const auto scaledCoordinateError = [&](const double firstError, const double secondError,
            const double firstValue, const double secondValue) -> std::optional<double>
        {
            std::optional<double> result{0.0};
            addBound(result,outwardDivide(firstError,scale));
            addBound(result,outwardDivide(secondError,scale));
            addBound(result,outwardMultiply(precision::kRoundoff,std::abs(firstValue)));
            addBound(result,outwardMultiply(precision::kRoundoff,std::abs(secondValue)));
            return result;
        };
        const auto deltaErrorX = scaledCoordinateError(firstNode.error.x,secondNode.error.x,
            firstScaled.x,secondScaled.x);
        const auto deltaErrorY = scaledCoordinateError(firstNode.error.y,secondNode.error.y,
            firstScaled.y,secondScaled.y);
        if ( !finite(delta) || !deltaErrorX || !deltaErrorY )
        {
            return std::nullopt;
        }
        const Point2 deltaError{*deltaErrorX,*deltaErrorY};
        const double value = std::fma(delta.x,support.direction.x,
            delta.y * support.direction.y);
        if ( !std::isfinite(value) )
        {
            return std::nullopt;
        }
        std::optional<double> arithmeticMagnitude{0.0};
        addBound(arithmeticMagnitude,outwardMultiply(std::abs(delta.x),
            std::abs(support.direction.x)));
        addBound(arithmeticMagnitude,outwardMultiply(std::abs(delta.y),
            std::abs(support.direction.y)));
        addBound(arithmeticMagnitude,outwardNonnegative(std::abs(value)));
        const auto arithmeticError = arithmeticMagnitude
            ? outwardMultiply(precision::kRoundoff,*arithmeticMagnitude) : std::nullopt;
        std::optional<double> uncertainty{0.0};
        addBound(uncertainty,arithmeticError);
        addBound(uncertainty,outwardMultiply(std::abs(support.direction.x),deltaError.x));
        addBound(uncertainty,outwardMultiply(std::abs(support.direction.y),deltaError.y));
        addBound(uncertainty,outwardMultiply(std::abs(delta.x),support.error.x));
        addBound(uncertainty,outwardMultiply(std::abs(delta.y),support.error.y));
        addBound(uncertainty,outwardMultiply(deltaError.x,support.error.x));
        addBound(uncertainty,outwardMultiply(deltaError.y,support.error.y));
        if ( !uncertainty || std::abs(value) <= *uncertainty )
        {
            return std::nullopt;
        }
        return value < 0.0 ? std::optional<int>{-1} : std::optional<int>{1};
    }

    // Resolve exact topology first, then use only the finite nonzero interval
    // certificate for cases whose generated provenance is unavailable.
    std::optional<int> nodeOrderOnLine(const NodeId first, const NodeId second,
        const LineId common) const noexcept
    {
        if ( const auto exact = exactNodeOrderOnLine(first,second,common) )
        {
            return exact;
        }
        return certifiedNodeOrderOnLine(first,second,common);
    }

    bool registerIncidence(const NodeId node, const LineId id)
    {
        if ( node >= nodes_.size() || id >= lines_.size() )
        {
            return false;
        }
        if ( originalStrictSides_.contains({node,id}) )
        {
            return false;
        }
        if ( incident(node,id) )
        {
            return true;
        }
        const bool sourceExactIncidence = sourceNodeIsExactlyOnLine(node,id);
        const bool sourceExactSupport = !nodes_[node].original
            && generatedNodeHasSourceCoincidentSupport(node,id);
        if ( !sourceExactIncidence && !sourceExactSupport )
        {
            const auto [distance,error] = incidenceMeasurement(node,id);
            if ( !std::isfinite(distance) || !std::isfinite(error) || std::abs(distance) > error )
            {
                return false;
            }
            if ( !nodes_[node].original && classifyGeneratedNearLine(node,id) )
            {
                return false;
            }
        }
        for ( const auto other : nodes_[node].lines )
        {
            if ( coincident(id,other) )
            {
                continue;
            }
            if ( const auto found = intersections_.find(std::minmax(id,other));
                found != intersections_.end() && found->second != node )
            {
                return false;
            }
        }
        for ( const auto other : nodes_[node].lines )
        {
            // Coincident supporting lines can contain many distinct authored
            // nodes. Incidence must not invent a unique crossing or merge them.
            if ( !coincident(id,other) )
            {
                cacheIntersection(id,other,node);
            }
        }
        nodes_[node].lines.push_back(id);
        return true;
    }

    // Complete authored node/line incidence before generating any crossings.
    // Failure preserves all previously published identities and incidences.
    bool initializeOriginalIncidences()
    {
        if ( !valid_ || std::any_of(nodes_.begin(), nodes_.end(),
                [](const Node& node) { return !node.original; }) )
        {
            return false;
        }
        // Stage only mutable incidence sidecars. Immutable lines, frame and
        // endpoint identities remain the original operands for every test.
        auto stagedNodes = nodes_;
        auto stagedIntersections = intersections_;
        auto stagedStrictSides = originalStrictSides_;
        // Discover all near-ON memberships before registering any of them.
        // Exact side certificates are needed only when multiple authored
        // identities compete for one exactly distinct support pair; a single
        // uncertain membership remains the original ON identity.
        std::map<std::pair<LineId,LineId>,std::vector<NodeId>> nearPairCandidates;
        for ( NodeId node = 0; node < nodes_.size(); ++node )
        {
            std::vector<LineId> nearLines;
            for ( LineId line = 0; line < lines_.size(); ++line )
            {
                const bool sourceExact = sourceNodeIsExactlyOnLine(node,line);
                const auto [distance, error] = incidenceMeasurement(node, line);
                if ( !sourceExact && (!std::isfinite(distance) || !std::isfinite(error) || error < 0.0) )
                {
                    return false;
                }
                if ( incident(node,line) || sourceExact || std::abs(distance) <= error )
                {
                    nearLines.push_back(line);
                }
            }
            for ( std::size_t first = 0; first < nearLines.size(); ++first )
            {
                for ( std::size_t second = first + 1; second < nearLines.size(); ++second )
                {
                    const auto a = nearLines[first], b = nearLines[second];
                    const auto exactlyCoincident = supportsAreExactlyCoincident(a,b);
                    if ( !exactlyCoincident )
                    {
                        return false;
                    }
                    if ( *exactlyCoincident )
                    {
                        continue;
                    }
                    const std::pair<LineId,LineId> key = std::minmax(a,b);
                    nearPairCandidates[key].push_back(node);
                }
            }
        }
        for ( const auto& [pair,candidates] : nearPairCandidates )
        {
            if ( candidates.size() < 2 )
            {
                continue;
            }
            for ( const auto node : candidates )
            {
                // Exact immutable orientation decides every nonzero
                // near-line side, including supports with no shared
                // authored endpoint. A propagated interval is not an
                // ON identity when the fixed endpoint operands prove a
                // strict side.
                for ( const auto line : {pair.first,pair.second} )
                {
                    const auto& edge = lines_[line].geometry;
                    const auto& start = nodes_[edge.firstNode].local;
                    const auto& finish = nodes_[edge.secondNode].local;
                    const auto& point = nodes_[node].local;
                    const auto orientation = exactOrientationSign(start.x,start.y,
                        finish.x,finish.y,point.x,point.y);
                    if ( !orientation )
                    {
                        return false;
                    }
                    if ( *orientation != 0 )
                    {
                        const int side = -*orientation;
                        const auto key = std::pair{node,line};
                        if ( const auto found = stagedStrictSides.find(key);
                            found != stagedStrictSides.end() && found->second != side )
                        {
                            return false;
                        }
                        stagedStrictSides[key] = side;
                    }
                    else if ( stagedStrictSides.contains({node,line}) )
                    {
                        // The immutable operands cannot be both exactly ON
                        // and strictly sided for one node/support.
                        return false;
                    }
                }
            }
        }
        for ( const auto& [key, side] : stagedStrictSides )
        {
            (void)side;
            auto& incidentLines = stagedNodes[key.first].lines;
            incidentLines.erase(std::remove(incidentLines.begin(),incidentLines.end(),key.second),
                incidentLines.end());
        }
        for ( auto entry = stagedIntersections.begin(); entry != stagedIntersections.end(); )
        {
            if ( stagedStrictSides.contains({entry->second,entry->first.first})
                || stagedStrictSides.contains({entry->second,entry->first.second}) )
            {
                entry = stagedIntersections.erase(entry);
            }
            else
            {
                ++entry;
            }
        }
        for ( NodeId node = 0; node < stagedNodes.size(); ++node )
        {
            auto& incidentLines = stagedNodes[node].lines;
            for ( LineId line = 0; line < lines_.size(); ++line )
            {
                if ( stagedStrictSides.contains({node,line})
                    || std::find(incidentLines.begin(), incidentLines.end(), line) != incidentLines.end() )
                {
                    continue;
                }
                const bool sourceExact = sourceNodeIsExactlyOnLine(node,line);
                const auto [distance, error] = incidenceMeasurement(node, line);
                if ( !sourceExact && (!std::isfinite(distance) || !std::isfinite(error) || error < 0.0) )
                {
                    return false;
                }
                if ( !sourceExact && std::abs(distance) > error )
                {
                    continue;
                }
                for ( const auto other : incidentLines )
                {
                    if ( coincident(line, other) )
                    {
                        continue;
                    }
                    const auto found = stagedIntersections.find(std::minmax(line, other));
                    if ( found != stagedIntersections.end() && found->second != node )
                    {
                        return false;
                    }
                }
                for ( const auto other : incidentLines )
                {
                    if ( !coincident(line, other) )
                    {
                        stagedIntersections.emplace(std::minmax(line, other), node);
                    }
                }
                incidentLines.push_back(line);
            }
        }
        nodes_.swap(stagedNodes);
        intersections_.swap(stagedIntersections);
        originalStrictSides_.swap(stagedStrictSides);
        return true;
    }

    std::optional<int> classify(const NodeId node, const LineId id)
    {
        if ( node >= nodes_.size() || id >= lines_.size() )
        {
            return std::nullopt;
        }
        if ( const auto strict = originalStrictSides_.find({node,id}); strict != originalStrictSides_.end() )
        {
            return strict->second;
        }
        if ( incident(node,id) )
        {
            return 0;
        }
        if ( sourceNodeIsExactlyOnLine(node,id)
            || (!nodes_[node].original && generatedNodeHasSourceCoincidentSupport(node,id)) )
        {
            return registerIncidence(node,id) ? std::optional<int>{0} : std::nullopt;
        }
        const auto [distance,error] = incidenceMeasurement(node,id);
        if ( !std::isfinite(distance) || !std::isfinite(error) )
        {
            return std::nullopt;
        }
        if ( std::abs(distance) > error )
        {
            return distance < 0.0 ? -1 : 1;
        }
        if ( !nodes_[node].original )
        {
            if ( const auto strict = classifyGeneratedNearLine(node,id); strict )
            {
                return strict;
            }
        }
        // A failed identity registration inside the arithmetic uncertainty
        // interval is ambiguous, never evidence for either strict halfspace.
        return registerIncidence(node,id) ? std::optional<int>{0} : std::nullopt;
    }

    std::optional<NodeId> intersection(const LineId first, const LineId second)
    {
        if ( first >= lines_.size() || second >= lines_.size() || first == second )
        {
            return std::nullopt;
        }
        const std::pair<LineId,LineId> key = std::minmax(first,second);
        if ( const auto found = intersections_.find(key); found != intersections_.end() )
        {
            // A shared original endpoint is an explicit topological identity,
            // even where the supporting infinite lines happen to be collinear.
            return found->second;
        }
        const auto alias = findGeneratedIntersectionAlias(first,second);
        if ( alias.conflict )
        {
            return std::nullopt;
        }
        if ( alias.node )
        {
            const NodeId node = *alias.node;
            // Attach every missing exact-support incidence before publishing
            // the requested pair cache entry. The alias may have come from a
            // different typed support pair or from the shared-line fallback.
            if ( !incident(node,first) && !registerIncidence(node,first) )
            {
                return std::nullopt;
            }
            if ( !incident(node,second) && !registerIncidence(node,second) )
            {
                return std::nullopt;
            }
            cacheIntersection(first,second,node);
            return node;
        }
        // Use canonical line order for a new solve. Query order never chooses
        // a different interpolation line or a different coordinate rounding.
        const auto& a = lines_[key.first];
        const auto& b = lines_[key.second];
        const double determinant = cross(a.direction,b.direction);
        const double determinantError = crossError(a.direction,a.error,b.direction,b.error);
        if ( !std::isfinite(determinant) || !std::isfinite(determinantError) )
        {
            return std::nullopt;
        }
        struct Reconstruction
        {
            TVector position;
            TVector error;
        };
        const auto reconstructExact = [&](const StoredLine& along, const ExactBinary64Ratio& parameter,
            const bool reverse) -> std::optional<Reconstruction>
        {
            const auto& start = nodes_[reverse ? along.geometry.secondNode : along.geometry.firstNode];
            const auto& finish = nodes_[reverse ? along.geometry.firstNode : along.geometry.secondNode];
            const double amount = parameter.value;
            const double amountError = parameter.error;
            if ( !std::isfinite(amount) || !(amountError >= 0.0) || !std::isfinite(amountError) )
            {
                return std::nullopt;
            }
            const std::array<double,3> startPoint{start.point.x,start.point.y,start.point.z};
            const std::array<double,3> finishPoint{finish.point.x,finish.point.y,finish.point.z};
            std::array<double,3> position{}, error{};
            for ( std::size_t component = 0; component < position.size(); ++component )
            {
                const double first = startPoint[component];
                const double second = finishPoint[component];
                if ( first == second )
                {
                    position[component] = first;
                    continue;
                }
                const double delta = second-first;
                const double value = std::isfinite(delta)
                    ? std::fma(amount,delta,first) : std::lerp(first,second,amount);
                const long double extendedDelta = static_cast<long double>(second)-first;
                if ( !std::isfinite(value) || !std::isfinite(extendedDelta) )
                {
                    return std::nullopt;
                }
                position[component] = value;
                const long double bound = std::abs(extendedDelta)
                    * static_cast<long double>(amountError)
                    + precision::kRoundoff
                        * (std::abs(static_cast<long double>(amount) * delta)
                            + std::abs(static_cast<long double>(value)))
                    + std::numeric_limits<double>::denorm_min();
                if ( !std::isfinite(bound) || bound > std::numeric_limits<double>::max() )
                {
                    return std::nullopt;
                }
                error[component] = std::nextafter(static_cast<double>(bound),
                    std::numeric_limits<double>::infinity());
                if ( !std::isfinite(error[component]) )
                {
                    return std::nullopt;
                }
            }
            return Reconstruction{{position[0],position[1],position[2]},
                {error[0],error[1],error[2]}};
        };
        const auto reconstruct = [&](const StoredLine& along, const StoredLine& against)
            -> std::optional<Reconstruction>
        {
            const auto& start = nodes_[along.geometry.firstNode];
            const auto& finish = nodes_[along.geometry.secondNode];
            const auto& other = nodes_[against.geometry.firstNode];
            const auto signedDistance = [&](const Node& node)
            {
                const Point2 delta{node.local.x-other.local.x,node.local.y-other.local.y};
                const Point2 error{node.error.x+other.error.x,node.error.y+other.error.y};
                return std::pair{cross(delta,against.direction),crossError(delta,error,against.direction,against.error)};
            };
            auto [startDistance,startError] = signedDistance(start);
            auto [finishDistance,finishError] = signedDistance(finish);
            if ( !std::isfinite(startDistance) || !std::isfinite(finishDistance)
                || !std::isfinite(startError) || !std::isfinite(finishError) )
            {
                return std::nullopt;
            }
            const bool fromFinish = std::abs(finishDistance) < std::abs(startDistance);
            double nearDistance = fromFinish ? finishDistance : startDistance;
            double farDistance = fromFinish ? startDistance : finishDistance;
            double nearError = fromFinish ? finishError : startError;
            double farError = fromFinish ? startError : finishError;
            if ( !std::isfinite(nearDistance-farDistance) )
            {
                const double scale = std::max(std::abs(nearDistance),std::abs(farDistance));
                nearDistance /= scale;
                farDistance /= scale;
                nearError /= scale;
                farError /= scale;
            }
            const double denominator = nearDistance-farDistance;
            const double denominatorError = nearError+farError
                + precision::kRoundoff*(std::abs(nearDistance)+std::abs(farDistance));
            if ( !std::isfinite(denominatorError) || std::abs(denominator) <= denominatorError )
            {
                return std::nullopt;
            }
            const double amount = nearDistance/denominator;
            const double amountError = (nearError+std::abs(amount)*denominatorError)
                / (std::abs(denominator)-denominatorError) + precision::kRoundoff*std::abs(amount);
            if ( !std::isfinite(amount) || !std::isfinite(amountError) )
            {
                return std::nullopt;
            }
            const auto& near = fromFinish ? finish : start;
            const auto& far = fromFinish ? start : finish;
            const std::array<double,3> nearPoint{near.point.x,near.point.y,near.point.z};
            const std::array<double,3> farPoint{far.point.x,far.point.y,far.point.z};
            std::array<double,3> position{}, error{};
            for ( std::size_t component = 0; component < 3; ++component )
            {
                const double first = nearPoint[component], second = farPoint[component];
                if ( first == second )
                {
                    // A constant immutable coordinate has no interpolation
                    // uncertainty, even for an extrapolated line crossing.
                    position[component] = first;
                    continue;
                }
                const double delta = second-first;
                position[component] = std::isfinite(delta)
                    ? std::fma(amount,delta,first) : std::lerp(first,second,amount);
                const long double extendedDelta = static_cast<long double>(second)-first;
                const long double bound = std::abs(extendedDelta)*amountError
                    + precision::kRoundoff*(std::abs(static_cast<long double>(amount)*extendedDelta)
                        + std::abs(static_cast<long double>(position[component])))
                    + std::numeric_limits<double>::denorm_min();
                // Round the nonnegative bound outward when narrowing it;
                // this also covers platforms where long double is double.
                error[component] = std::nextafter(static_cast<double>(bound),
                    std::numeric_limits<double>::infinity());
                if ( !std::isfinite(position[component]) || !std::isfinite(error[component]) )
                {
                    return std::nullopt;
                }
            }
            return Reconstruction{{position[0],position[1],position[2]}, {error[0],error[1],error[2]}};
        };
        // No semantic role (source edge, ear diagonal, or rectangle edge)
        // decides the stored point. Each component uses the tighter of two
        // independently propagated immutable-line construction bounds.
        std::optional<Reconstruction> firstReconstruction;
        std::optional<Reconstruction> secondReconstruction;
        const auto reconstructFromExactSupports = [&]()
        {
            const auto& firstStart = nodes_[a.geometry.firstNode].local;
            const auto& firstFinish = nodes_[a.geometry.secondNode].local;
            const auto& secondStart = nodes_[b.geometry.firstNode].local;
            const auto& secondFinish = nodes_[b.geometry.secondNode].local;
            const auto firstParameter = exactLineIntersectionParameter(
                firstStart.x,firstStart.y,firstFinish.x,firstFinish.y,
                secondStart.x,secondStart.y,secondFinish.x,secondFinish.y);
            const auto reversedFirstParameter = exactLineIntersectionParameter(
                firstFinish.x,firstFinish.y,firstStart.x,firstStart.y,
                secondStart.x,secondStart.y,secondFinish.x,secondFinish.y);
            const auto secondParameter = exactLineIntersectionParameter(
                secondStart.x,secondStart.y,secondFinish.x,secondFinish.y,
                firstStart.x,firstStart.y,firstFinish.x,firstFinish.y);
            const auto reversedSecondParameter = exactLineIntersectionParameter(
                secondFinish.x,secondFinish.y,secondStart.x,secondStart.y,
                firstStart.x,firstStart.y,firstFinish.x,firstFinish.y);
            if ( !firstParameter || !reversedFirstParameter || !secondParameter || !reversedSecondParameter )
            {
                return false;
            }
            const bool reverseFirst = std::abs(reversedFirstParameter->value)
                < std::abs(firstParameter->value);
            const bool reverseSecond = std::abs(reversedSecondParameter->value)
                < std::abs(secondParameter->value);
            firstReconstruction = reconstructExact(a,
                reverseFirst ? *reversedFirstParameter : *firstParameter,reverseFirst);
            secondReconstruction = reconstructExact(b,
                reverseSecond ? *reversedSecondParameter : *secondParameter,reverseSecond);
            return firstReconstruction.has_value() && secondReconstruction.has_value();
        };
        if ( std::abs(determinant) <= determinantError )
        {
            if ( !reconstructFromExactSupports() )
            {
                return std::nullopt;
            }
        }
        else
        {
            firstReconstruction = reconstruct(a,b);
            secondReconstruction = reconstruct(b,a);
            if ( !firstReconstruction || !secondReconstruction )
            {
                // The signed-distance path is a useful fast path, but its
                // denominator can be uncertain even when the immutable
                // endpoint operands provide an exact finite crossing. Retry
                // both supports from independent exact ratios so that one
                // uncertain reconstruction never gets paired with a rounded
                // reconstruction from the other support.
                firstReconstruction.reset();
                secondReconstruction.reset();
                if ( !reconstructFromExactSupports() )
                {
                    return std::nullopt;
                }
            }
        }
        if ( !firstReconstruction || !secondReconstruction )
        {
            return std::nullopt;
        }
        const std::array<double,3> firstPosition{firstReconstruction->position.x,firstReconstruction->position.y,firstReconstruction->position.z};
        const std::array<double,3> secondPosition{secondReconstruction->position.x,secondReconstruction->position.y,secondReconstruction->position.z};
        const std::array<double,3> firstError{firstReconstruction->error.x,firstReconstruction->error.y,firstReconstruction->error.z};
        const std::array<double,3> secondError{secondReconstruction->error.x,secondReconstruction->error.y,secondReconstruction->error.z};
        std::array<double,3> components{}, componentError{};
        for ( std::size_t component = 0; component < 3; ++component )
        {
            if ( std::abs(static_cast<long double>(firstPosition[component])-secondPosition[component])
                > static_cast<long double>(firstError[component])+secondError[component] )
            {
                return std::nullopt;
            }
            const bool chooseFirst = firstError[component] < secondError[component]
                || (firstError[component] == secondError[component] && firstPosition[component] <= secondPosition[component]);
            components[component] = chooseFirst ? firstPosition[component] : secondPosition[component];
            componentError[component] = chooseFirst ? firstError[component] : secondError[component];
        }
        const TVector position{components[0],components[1],components[2]};
        const Point2 local = coordinates(position);
        const Point2 reconstructionError = coordinateError(position);
        const auto projectedError = [&](const TVector& axis)
        {
            return std::abs(axis.x)*componentError[0] + std::abs(axis.y)*componentError[1]
                + std::abs(axis.z)*componentError[2];
        };
        const Point2 error{reconstructionError.x+projectedError(axisU_), reconstructionError.y+projectedError(axisV_)};
        const double height = normal_.dot(position-anchor_);
        const double heightError = precision::planeError(normal_,position,anchor_)+projectedError(normal_);
        if ( !finite(local) || !finite(error) || !std::isfinite(height) || !std::isfinite(heightError)
            || std::abs(height) > heightError )
        {
            return std::nullopt;
        }
        // Component selection is permitted only if the reconstructed point
        // still carries certificates for both immutable supporting lines.
        for ( const auto* line : {&a,&b} )
        {
            const auto& origin = nodes_[line->geometry.firstNode];
            const Point2 delta{local.x-origin.local.x,local.y-origin.local.y};
            const double residual = cross(delta,line->direction);
            const double allowance = crossError(delta,{error.x+origin.error.x,error.y+origin.error.y},line->direction,line->error);
            if ( !std::isfinite(residual) || !std::isfinite(allowance) || std::abs(residual) > allowance )
            {
                return std::nullopt;
            }
        }
        const NodeId id = nodes_.size();
        nodes_.push_back({position,local,error,{key.first,key.second},false,std::nullopt});
        cacheIntersection(key.first,key.second,id);
        return id;
    }

private:
    struct Point2
    {
        double x{};
        double y{};
    };
    struct Node
    {
        TVector point;
        Point2 local;
        Point2 error;
        std::vector<LineId> lines;
        bool original{};
        std::optional<TVector> sourceWorld;
    };
    struct StoredLine
    {
        Line geometry;
        Point2 direction;
        Point2 error;
    };
    struct GeneratedIntersectionAlias
    {
        std::optional<NodeId> node;
        bool conflict{};
    };
    struct GeneratedSupportIntersectionBucket
    {
        NodeId first{kInvalidNode};
        std::vector<NodeId> overflow;
    };
    using TypedSupportRepresentative = std::pair<LineId,bool>;
    using TypedSupportKey = std::pair<TypedSupportRepresentative,TypedSupportRepresentative>;
    static bool samePointExact(const TVector& first, const TVector& second) noexcept
    {
        return first.x == second.x && first.y == second.y && first.z == second.z;
    }
    LineId projectedSupportRepresentative(const LineId line) const noexcept
    {
        return line < projectedSupportRepresentatives_.size()
            ? projectedSupportRepresentatives_[line] : kInvalidLine;
    }
    LineId sourceSupportRepresentative(const LineId line) const noexcept
    {
        return line < sourceSupportRepresentatives_.size()
            ? sourceSupportRepresentatives_[line] : kInvalidLine;
    }
    std::array<TypedSupportRepresentative,2> typedSupportRepresentatives(
        const LineId line) const noexcept
    {
        return {{{projectedSupportRepresentative(line),false},
            {sourceSupportRepresentative(line),true}}};
    }
    std::optional<TVector> projectedSourcePoint(const TVector& sourceWorld) const noexcept
    {
        if ( !precision::finite(sourceWorld) )
        {
            return std::nullopt;
        }
        const TVector projected = sourceWorld - normal_ * normal_.dot(sourceWorld - anchor_);
        return precision::finite(projected) ? std::optional<TVector>{projected} : std::nullopt;
    }
    bool lineHasSourceSupport(const LineId line) const noexcept
    {
        if ( line >= lines_.size() )
        {
            return false;
        }
        const auto& geometry = lines_[line].geometry;
        return geometry.firstNode < nodes_.size() && geometry.secondNode < nodes_.size()
            && nodes_[geometry.firstNode].sourceWorld.has_value()
            && nodes_[geometry.secondNode].sourceWorld.has_value();
    }
    NodeId addOriginalInternal(const TVector& point, const std::optional<TVector>& sourceWorld)
    {
        if ( !valid_ || !precision::finite(point)
            || (sourceWorld && !precision::finite(*sourceWorld)) )
        {
            return kInvalidNode;
        }
        const auto local = coordinates(point);
        const auto error = coordinateError(point);
        const double height = normal_.dot(point - anchor_);
        if ( !finite(local) || !finite(error) || !std::isfinite(height)
            || std::abs(height) > precision::planeError(normal_, point, anchor_) )
        {
            return kInvalidNode;
        }
        nodes_.push_back({point, local, error, {}, true, sourceWorld});
        return nodes_.size() - 1;
    }
    void cacheIntersection(const LineId first, const LineId second, const NodeId node)
    {
        if ( first >= lines_.size() || second >= lines_.size() || first == second )
        {
            return;
        }
        const auto key = std::minmax(first,second);
        const auto [found, inserted] = intersections_.emplace(key,node);
        if ( !inserted || node >= nodes_.size() || nodes_[node].original )
        {
            return;
        }
        const auto firstRepresentatives = typedSupportRepresentatives(key.first);
        const auto secondRepresentatives = typedSupportRepresentatives(key.second);
        for ( const auto firstRepresentative : firstRepresentatives )
        {
            if ( firstRepresentative.first == kInvalidLine )
            {
                continue;
            }
            for ( const auto secondRepresentative : secondRepresentatives )
            {
                if ( secondRepresentative.first == kInvalidLine
                    || firstRepresentative == secondRepresentative )
                {
                    continue;
                }
                const TypedSupportKey supportKey = firstRepresentative < secondRepresentative
                    ? TypedSupportKey{firstRepresentative,secondRepresentative}
                    : TypedSupportKey{secondRepresentative,firstRepresentative};
                cacheGeneratedSupportCandidate(supportKey,node);
            }
        }
        generatedIntersectionNeighbors_[key.first].push_back(key.second);
        generatedIntersectionNeighbors_[key.second].push_back(key.first);
        (void)found;
    }
    void cacheGeneratedSupportCandidate(const TypedSupportKey& supportKey, const NodeId node)
    {
        const auto [found, inserted] = generatedSupportIntersections_.emplace(
            supportKey,GeneratedSupportIntersectionBucket{});
        auto& bucket = found->second;
        if ( bucket.first == kInvalidNode )
        {
            bucket.first = node;
        }
        else if ( bucket.first != node
            && std::find(bucket.overflow.begin(),bucket.overflow.end(),node) == bucket.overflow.end() )
        {
            // Retain colliding generated candidates so a later direct
            // support proof can select one unique match. Overflow is
            // allocated only for an actual collision.
            bucket.overflow.push_back(node);
        }
        (void)inserted;
    }
    GeneratedIntersectionAlias findGeneratedIntersectionAlias(
        const LineId first, const LineId second) const noexcept
    {
        GeneratedIntersectionAlias result;
        const auto firstRepresentatives = typedSupportRepresentatives(first);
        const auto secondRepresentatives = typedSupportRepresentatives(second);
        const auto inspectIndexedCandidate = [&](const NodeId node)
        {
            if ( node >= nodes_.size() || nodes_[node].original
                || nodes_[node].lines.size() < 2 )
            {
                return true;
            }
            // The typed representative index is only a bounded candidate
            // lookup. Rounded projection certificates and immutable source
            // certificates are not generally transitive across domains, so
            // the requested supports must be certified directly against the
            // candidate's two original defining lines.
            const LineId candidateFirst = nodes_[node].lines[0];
            const LineId candidateSecond = nodes_[node].lines[1];
            const auto exactlyMatches = [&](const LineId requested, const LineId candidate)
            {
                const auto exact = supportsAreExactlyCoincident(requested,candidate);
                return exact && *exact;
            };
            const bool directPair = (exactlyMatches(first,candidateFirst)
                    && exactlyMatches(second,candidateSecond))
                || (exactlyMatches(first,candidateSecond)
                    && exactlyMatches(second,candidateFirst));
            if ( !directPair )
            {
                return true;
            }
            if ( result.node && *result.node != node )
            {
                // Multiple direct matches are a genuine ambiguous canonical
                // crossing. Fail closed rather than pick a candidate based
                // on insertion order.
                result.conflict = true;
                return false;
            }
            result.node = node;
            return true;
        };
        const auto inspectIndexedBucket = [&](const GeneratedSupportIntersectionBucket& bucket)
        {
            if ( !inspectIndexedCandidate(bucket.first) )
            {
                return false;
            }
            for ( const auto node : bucket.overflow )
            {
                if ( !inspectIndexedCandidate(node) )
                {
                    return false;
                }
            }
            return true;
        };
        for ( const auto firstRepresentative : firstRepresentatives )
        {
            if ( firstRepresentative.first == kInvalidLine )
            {
                continue;
            }
            for ( const auto secondRepresentative : secondRepresentatives )
            {
                if ( secondRepresentative.first == kInvalidLine
                    || firstRepresentative == secondRepresentative )
                {
                    continue;
                }
                const TypedSupportKey supportKey = firstRepresentative < secondRepresentative
                    ? TypedSupportKey{firstRepresentative,secondRepresentative}
                    : TypedSupportKey{secondRepresentative,firstRepresentative};
                if ( const auto found = generatedSupportIntersections_.find(supportKey);
                    found != generatedSupportIntersections_.end()
                    && !inspectIndexedBucket(found->second) )
                {
                    return result;
                }
            }
        }
        if ( result.conflict )
        {
            return result;
        }
        const auto inspect = [&](const LineId requested, const LineId common)
        {
            if ( common >= generatedIntersectionNeighbors_.size() )
            {
                return true;
            }
            for ( const auto alternate : generatedIntersectionNeighbors_[common] )
            {
                if ( alternate == requested || alternate >= lines_.size() )
                {
                    continue;
                }
                const auto found = intersections_.find(std::minmax(alternate,common));
                if ( found == intersections_.end() || found->second >= nodes_.size()
                    || nodes_[found->second].original || !incident(found->second,alternate)
                    || !incident(found->second,common) )
                {
                    continue;
                }
                // Shared-LineID discovery is only a candidate source. Apply
                // the same direct two-support proof used by the typed index
                // before allowing a registered crossing to answer the new
                // pair, since mixed source/projected certificates are not
                // transitive.
                if ( !inspectIndexedCandidate(found->second) )
                {
                    return false;
                }
            }
            return true;
        };
        if ( !inspect(first,second) || !inspect(second,first) )
        {
            result.node.reset();
            result.conflict = true;
        }
        return result;
    }
    static bool finite(const Point2& p) noexcept
    {
        return std::isfinite(p.x) && std::isfinite(p.y);
    }
    static double cross(const Point2& a, const Point2& b) noexcept
    {
        const double product = a.y * b.x;
        return std::fma(a.x,b.y,-product) - std::fma(a.y,b.x,-product);
    }
    static double crossError(const Point2& a, const Point2& ae, const Point2& b, const Point2& be) noexcept
    {
        return precision::kRoundoff * (std::abs(a.x*b.y) + std::abs(a.y*b.x))
            + std::abs(a.x)*be.y + std::abs(a.y)*be.x + std::abs(b.y)*ae.x + std::abs(b.x)*ae.y
            + ae.x*be.y + ae.y*be.x;
    }
    static std::optional<int> exactDifferenceCrossSign(
        const double firstStartFirst, const double firstFinishFirst,
        const double secondStartFirst, const double secondFinishFirst,
        const double firstStartSecond, const double firstFinishSecond,
        const double secondStartSecond, const double secondFinishSecond) noexcept
    {
        const std::array<double,8> values{{firstStartFirst,firstFinishFirst,
            secondStartFirst,secondFinishFirst,firstStartSecond,firstFinishSecond,
            secondStartSecond,secondFinishSecond}};
        // (firstFinish-firstStart) x (secondFinish-secondStart), expanded
        // without rounding either endpoint difference first.
        constexpr std::array<ExactBinary64ProductTerm,8> terms{{
            {1,7,false}, {1,6,true}, {0,7,true}, {0,6,false},
            {5,3,true}, {5,2,false}, {4,3,false}, {4,2,true}}};
        return exactBinary64ProductSumSign(values,terms);
    }
    std::pair<double,double> incidenceMeasurement(const NodeId node, const LineId id) const noexcept
    {
        const auto& geometry = lines_[id];
        const auto& first = nodes_[geometry.geometry.firstNode];
        const Point2 delta{nodes_[node].local.x-first.local.x,nodes_[node].local.y-first.local.y};
        return {cross(delta,geometry.direction),crossError(delta,
            {nodes_[node].error.x+first.error.x,nodes_[node].error.y+first.error.y},
            geometry.direction,geometry.error)};
    }
    bool coincident(const LineId first, const LineId second) const noexcept
    {
        if ( const auto exact = supportsAreExactlyCoincident(first,second); exact && *exact )
        {
            return true;
        }
        const auto& a = lines_[first];
        const auto& b = lines_[second];
        const double determinant = cross(a.direction,b.direction);
        const double error = crossError(a.direction,a.error,b.direction,b.error);
        if ( !std::isfinite(determinant) || !std::isfinite(error) || std::abs(determinant) > error )
        {
            return false;
        }
        const auto onLine = [&](const NodeId node, const LineId id)
        {
            const auto [distance,allowance] = incidenceMeasurement(node,id);
            return std::isfinite(distance) && std::isfinite(allowance) && std::abs(distance) <= allowance;
        };
        return onLine(a.geometry.firstNode,second) && onLine(a.geometry.secondNode,second)
            && onLine(b.geometry.firstNode,first) && onLine(b.geometry.secondNode,first);
    }
    std::optional<bool> projectedSupportsAreExactlyCoincident(const LineId first,
        const LineId second) const noexcept
    {
        if ( first >= lines_.size() || second >= lines_.size() )
        {
            return std::nullopt;
        }
        const auto& a = lines_[first].geometry;
        const auto& b = lines_[second].geometry;
        const auto& aFirst = nodes_[a.firstNode].local;
        const auto& aSecond = nodes_[a.secondNode].local;
        const auto& bFirst = nodes_[b.firstNode].local;
        const auto& bSecond = nodes_[b.secondNode].local;
        const auto direction = exactDirectionCrossSign(aFirst.x,aFirst.y,aSecond.x,aSecond.y,
            bFirst.x,bFirst.y,bSecond.x,bSecond.y);
        if ( !direction )
        {
            return std::nullopt;
        }
        if ( *direction != 0 )
        {
            return false;
        }
        const auto firstOnSecond = exactOrientationSign(bFirst.x,bFirst.y,bSecond.x,bSecond.y,
            aFirst.x,aFirst.y);
        const auto secondOnSecond = exactOrientationSign(bFirst.x,bFirst.y,bSecond.x,bSecond.y,
            aSecond.x,aSecond.y);
        if ( firstOnSecond && secondOnSecond && *firstOnSecond == 0 && *secondOnSecond == 0 )
        {
            return true;
        }
        if ( !firstOnSecond || !secondOnSecond )
        {
            return std::nullopt;
        }
        return false;
    }
    std::optional<bool> supportsAreExactlyCoincident(const LineId first, const LineId second) const noexcept
    {
        const auto source = sourceSupportsAreExactlyCoincident(first,second);
        if ( source && *source )
        {
            return true;
        }
        const auto projected = projectedSupportsAreExactlyCoincident(first,second);
        if ( projected && *projected )
        {
            return true;
        }
        if ( source )
        {
            return source;
        }
        return projected;
    }
    std::optional<bool> sourceSupportsAreExactlyCoincident(const LineId first,
        const LineId second) const noexcept
    {
        if ( first >= lines_.size() || second >= lines_.size() )
        {
            return std::nullopt;
        }
        const auto& firstLine = lines_[first].geometry;
        const auto& secondLine = lines_[second].geometry;
        if ( firstLine.firstNode >= nodes_.size() || firstLine.secondNode >= nodes_.size()
            || secondLine.firstNode >= nodes_.size() || secondLine.secondNode >= nodes_.size() )
        {
            return std::nullopt;
        }
        const auto& firstStart = nodes_[firstLine.firstNode].sourceWorld;
        const auto& firstFinish = nodes_[firstLine.secondNode].sourceWorld;
        const auto& secondStart = nodes_[secondLine.firstNode].sourceWorld;
        const auto& secondFinish = nodes_[secondLine.secondNode].sourceWorld;
        if ( !firstStart || !firstFinish || !secondStart || !secondFinish )
        {
            return std::nullopt;
        }
        const TVector& a = *firstStart;
        const TVector& b = *firstFinish;
        const TVector& c = *secondStart;
        const TVector& d = *secondFinish;
        const auto directionX = exactDifferenceCrossSign(
            a.y,b.y,c.y,d.y,a.z,b.z,c.z,d.z);
        const auto directionY = exactDifferenceCrossSign(
            a.z,b.z,c.z,d.z,a.x,b.x,c.x,d.x);
        const auto directionZ = exactDifferenceCrossSign(
            a.x,b.x,c.x,d.x,a.y,b.y,c.y,d.y);
        const auto offsetX = exactDifferenceCrossSign(
            a.y,c.y,a.y,b.y,a.z,c.z,a.z,b.z);
        const auto offsetY = exactDifferenceCrossSign(
            a.z,c.z,a.z,b.z,a.x,c.x,a.x,b.x);
        const auto offsetZ = exactDifferenceCrossSign(
            a.x,c.x,a.x,b.x,a.y,c.y,a.y,b.y);
        if ( !directionX || !directionY || !directionZ || !offsetX || !offsetY || !offsetZ )
        {
            return std::nullopt;
        }
        return *directionX == 0 && *directionY == 0 && *directionZ == 0
            && *offsetX == 0 && *offsetY == 0 && *offsetZ == 0;
    }
    bool sourceNodeIsExactlyOnLine(const NodeId node, const LineId line) const noexcept
    {
        if ( node >= nodes_.size() || line >= lines_.size() || !nodes_[node].sourceWorld )
        {
            return false;
        }
        const auto& geometry = lines_[line].geometry;
        if ( geometry.firstNode >= nodes_.size() || geometry.secondNode >= nodes_.size()
            || !nodes_[geometry.firstNode].sourceWorld || !nodes_[geometry.secondNode].sourceWorld )
        {
            return false;
        }
        const TVector& start = *nodes_[geometry.firstNode].sourceWorld;
        const TVector& finish = *nodes_[geometry.secondNode].sourceWorld;
        const TVector& point = *nodes_[node].sourceWorld;
        const auto crossX = exactDifferenceCrossSign(
            start.y,finish.y,start.y,point.y,start.z,finish.z,start.z,point.z);
        const auto crossY = exactDifferenceCrossSign(
            start.z,finish.z,start.z,point.z,start.x,finish.x,start.x,point.x);
        const auto crossZ = exactDifferenceCrossSign(
            start.x,finish.x,start.x,point.x,start.y,finish.y,start.y,point.y);
        return crossX && crossY && crossZ && *crossX == 0 && *crossY == 0 && *crossZ == 0;
    }
    bool generatedNodeHasSourceCoincidentSupport(const NodeId node,
        const LineId target) const noexcept
    {
        if ( node >= nodes_.size() || target >= lines_.size() || nodes_[node].original
            || nodes_[node].lines.size() < 2 )
        {
            return false;
        }
        for ( const auto defining : {nodes_[node].lines[0],nodes_[node].lines[1]} )
        {
            const auto exact = sourceSupportsAreExactlyCoincident(defining,target);
            if ( exact && *exact )
            {
                return true;
            }
        }
        return false;
    }
    std::optional<int> classifyGeneratedNearLine(const NodeId node, const LineId target) const noexcept
    {
        if ( node >= nodes_.size() || target >= lines_.size() || nodes_[node].original
            || nodes_[node].lines.size() < 2 )
        {
            return std::nullopt;
        }
        const auto firstDefining = nodes_[node].lines[0];
        const auto secondDefining = nodes_[node].lines[1];
        if ( firstDefining >= lines_.size() || secondDefining >= lines_.size()
            || firstDefining == secondDefining || firstDefining == target || secondDefining == target )
        {
            return std::nullopt;
        }
        const auto generatedIntersection = intersections_.find(std::minmax(firstDefining,secondDefining));
        if ( generatedIntersection == intersections_.end() || generatedIntersection->second != node )
        {
            return std::nullopt;
        }
        if ( generatedNodeHasSourceCoincidentSupport(node,target) )
        {
            return std::nullopt;
        }

        std::optional<int> result;
        for ( const auto& [alongId, otherId] : std::array<std::pair<LineId,LineId>,2>{
                std::pair{firstDefining,secondDefining}, std::pair{secondDefining,firstDefining}} )
        {
            const auto shared = intersections_.find(std::minmax(alongId,target));
            if ( shared == intersections_.end() )
            {
                continue;
            }
            const NodeId sharedNode = shared->second;
            if ( sharedNode >= nodes_.size() || !nodes_[sharedNode].original )
            {
                continue;
            }
            const auto& along = lines_[alongId].geometry;
            const auto& targetLine = lines_[target].geometry;
            const auto isEndpoint = [](const Line& line, const NodeId value)
            {
                return line.firstNode == value || line.secondNode == value;
            };
            if ( !isEndpoint(along,sharedNode) || !isEndpoint(targetLine,sharedNode) )
            {
                continue;
            }
            const NodeId otherEndpoint = along.firstNode == sharedNode
                ? along.secondNode : along.firstNode;
            if ( otherEndpoint >= nodes_.size() || otherEndpoint == sharedNode )
            {
                continue;
            }
            const auto& sharedPoint = nodes_[sharedNode].local;
            const auto& otherPoint = nodes_[otherEndpoint].local;
            const auto& otherLine = lines_[otherId].geometry;
            const auto& otherFirst = nodes_[otherLine.firstNode].local;
            const auto& otherSecond = nodes_[otherLine.secondNode].local;
            const auto& targetFirst = nodes_[targetLine.firstNode].local;
            const auto& targetSecond = nodes_[targetLine.secondNode].local;
            const auto otherAtShared = exactOrientationSign(otherFirst.x,otherFirst.y,
                otherSecond.x,otherSecond.y,sharedPoint.x,sharedPoint.y);
            const auto targetAtOther = exactOrientationSign(targetFirst.x,targetFirst.y,
                targetSecond.x,targetSecond.y,otherPoint.x,otherPoint.y);
            const auto direction = exactDirectionCrossSign(sharedPoint.x,sharedPoint.y,
                otherPoint.x,otherPoint.y,otherFirst.x,otherFirst.y,otherSecond.x,otherSecond.y);
            if ( !otherAtShared || !targetAtOther || !direction
                || *otherAtShared == 0 || *targetAtOther == 0 || *direction == 0 )
            {
                continue;
            }
            // The generated point X is the intersection of along and
            // otherId. Since target and along meet at sharedNode P, the exact
            // sign of target(X) follows from the immutable construction:
            // sign(target(X)) = sign(other(P)) * sign(target(Q))
            //                    * sign((Q-P) x otherDirection).
            const int targetOrientation = *otherAtShared * *targetAtOther * *direction;
            const int targetSide = -targetOrientation;
            if ( result && *result != targetSide )
            {
                return std::nullopt;
            }
            result = targetSide;
        }

        // The generated crossing can be close to a target support even when
        // neither defining line shares an authored endpoint with that target.
        // In that case the propagated local-coordinate interval cannot decide
        // a side, and registering the rounded point would invent a second
        // topological crossing. Evaluate the target at the exact intersection
        // of the two immutable defining supports instead. A zero or parallel
        // result stays unresolved so the caller retains its fail-closed path.
        const auto& firstLine = lines_[firstDefining].geometry;
        const auto& secondLine = lines_[secondDefining].geometry;
        const auto& targetLine = lines_[target].geometry;
        const auto& firstStart = nodes_[firstLine.firstNode].local;
        const auto& firstFinish = nodes_[firstLine.secondNode].local;
        const auto& secondStart = nodes_[secondLine.firstNode].local;
        const auto& secondFinish = nodes_[secondLine.secondNode].local;
        const auto& targetStart = nodes_[targetLine.firstNode].local;
        const auto& targetFinish = nodes_[targetLine.secondNode].local;
        const auto exactSide = exactIntersectionSideSign(
            firstStart.x,firstStart.y,firstFinish.x,firstFinish.y,
            secondStart.x,secondStart.y,secondFinish.x,secondFinish.y,
            targetStart.x,targetStart.y,targetFinish.x,targetFinish.y);
        if ( exactSide && *exactSide == 0 )
        {
            return std::nullopt;
        }
        if ( exactSide )
        {
            const int targetSide = -*exactSide;
            if ( result && *result != targetSide )
            {
                return std::nullopt;
            }
            result = targetSide;
        }
        return result;
    }
    Point2 coordinates(const TVector& p) const noexcept
    {
        const auto delta = p - anchor_;
        return {axisU_.dot(delta), axisV_.dot(delta)};
    }
    Point2 coordinateError(const TVector& p) const noexcept
    {
        const auto error = [&](const TVector& axis)
        {
            return precision::kRoundoff * (std::abs(axis.x)*(std::abs(p.x)+std::abs(anchor_.x))
                + std::abs(axis.y)*(std::abs(p.y)+std::abs(anchor_.y))
                + std::abs(axis.z)*(std::abs(p.z)+std::abs(anchor_.z)));
        };
        return {error(axisU_),error(axisV_)};
    }
    bool valid_{};
    TVector normal_;
    TVector anchor_;
    TVector axisU_;
    TVector axisV_;
    std::vector<Node> nodes_;
    std::vector<StoredLine> lines_;
    std::map<std::pair<NodeId,NodeId>,LineId> endpointLines_;
    std::map<std::pair<LineId,LineId>,NodeId> intersections_;
    std::vector<std::vector<LineId>> generatedIntersectionNeighbors_;
    std::vector<LineId> projectedSupportRepresentatives_;
    std::vector<LineId> projectedSupportClassRepresentatives_;
    std::vector<LineId> sourceSupportRepresentatives_;
    std::vector<LineId> sourceSupportClassRepresentatives_;
    std::map<TypedSupportKey,GeneratedSupportIntersectionBucket> generatedSupportIntersections_;
    std::map<std::pair<NodeId,LineId>,int> originalStrictSides_;
};

} // namespace sed::level::detail
