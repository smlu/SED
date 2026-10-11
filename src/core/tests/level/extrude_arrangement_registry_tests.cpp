#include "test_common.hpp"
#include "../../src/level/extrude_arrangement_registry.hpp"
#include "../../src/level/exact_orientation.hpp"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <limits>
#include <stdexcept>

namespace sed::tests
{

void runExtrudeArrangementRegistryTests()
{
    using Registry = level::detail::ExtrudeArrangementRegistry;
    {
        using level::detail::exactOrientationSign;
        struct Vector
        {
            std::array<double, 6> coordinates;
            int sign;
        };
        const double smallest = std::numeric_limits<double>::denorm_min();
        const double largest = std::numeric_limits<double>::max();
        const double adjacent = std::nextafter(1.0, 2.0);
        const double belowLargest = std::nextafter(largest, 0.0);
        const std::array<Vector, 9> vectors{{
            {{{0,0, 1,0, 0,1}}, 1},
            {{{0,0, 1,1, 2,2}}, 0},
            {{{-0.0,0, 0,-0.0, 0,0}}, 0},
            {{{1,1, adjacent,1, 1,adjacent}}, 1},
            {{{0,0, smallest,0, 0,smallest}}, 1},
            {{{0,0, largest,smallest, largest,2 * smallest}}, 1},
            {{{-largest,-largest, largest,-largest, -largest,largest}}, 1},
            {{{largest,largest, belowLargest,largest, largest,belowLargest}}, 1},
            {{{largest,-largest, largest,largest, -largest,largest}}, 1}
        }};
        for ( const auto& vector : vectors )
        {
            const auto& p = vector.coordinates;
            check(exactOrientationSign(p[0],p[1],p[2],p[3],p[4],p[5]) == std::optional<int>{vector.sign}
                    && exactOrientationSign(p[0],p[1],p[4],p[5],p[2],p[3]) == std::optional<int>{-vector.sign}
                    && exactOrientationSign(p[2],p[3],p[4],p[5],p[0],p[1]) == std::optional<int>{vector.sign},
                "Exact binary orientation: fixed sign/zero survives reversal, cyclic order, cancellation and full exponent range");
        }
        for ( const double invalid : {std::numeric_limits<double>::infinity(),
                -std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN()} )
        {
            for ( std::size_t component = 0; component < 6; ++component )
            {
                std::array<double, 6> p{{0,0, 1,0, 0,1}};
                p[component] = invalid;
                check(!exactOrientationSign(p[0],p[1],p[2],p[3],p[4],p[5]),
                    "Exact binary orientation: every nonfinite input component rejects with an empty return");
            }
        }
    }
    {
        using level::detail::exactDirectionCrossSign;
        struct Directions
        {
            std::array<double, 8> coordinates;
            int sign;
        };
        const double smallest = std::numeric_limits<double>::denorm_min();
        const double largest = std::numeric_limits<double>::max();
        const std::array<Directions, 6> vectors{{
            {{{0,0, 1,0, 0,0, 0,1}}, 1},
            {{{0,0, 1,0, 0,0, 0,-1}}, -1},
            {{{0,0, 1,1, 2,2, 3,3}}, 0},
            {{{0,0, largest,smallest, 0,0, largest,2 * smallest}}, 1},
            {{{-largest,0, largest,0, 0,-largest, 0,largest}}, 1},
            {{{largest,largest, std::nextafter(largest,0.0),largest,
                largest,largest, largest,std::nextafter(largest,0.0)}}, 1}}};
        for ( const auto& vector : vectors )
        {
            const auto& p = vector.coordinates;
            check(exactDirectionCrossSign(p[0],p[1],p[2],p[3],p[4],p[5],p[6],p[7])
                    == std::optional<int>{vector.sign}
                    && exactDirectionCrossSign(p[2],p[3],p[0],p[1],p[6],p[7],p[4],p[5])
                    == std::optional<int>{vector.sign},
                "Exact binary direction cross: fixed sign survives operand and direction reversal across exponent range");
        }
        for ( const double invalid : {std::numeric_limits<double>::infinity(),
                -std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN()} )
        {
            for ( std::size_t component = 0; component < 8; ++component )
            {
                std::array<double, 8> p{{0,0, 1,0, 0,0, 0,1}};
                p[component] = invalid;
                check(!exactDirectionCrossSign(p[0],p[1],p[2],p[3],p[4],p[5],p[6],p[7]),
                    "Exact binary direction cross: every nonfinite input component rejects with an empty return");
            }
        }
    }
    {
        using level::detail::exactDirectionDotSign;
        struct Directions
        {
            std::array<double, 8> coordinates;
            int sign;
        };
        const double largest = std::numeric_limits<double>::max();
        const double smallest = std::numeric_limits<double>::denorm_min();
        const std::array<Directions, 6> vectors{{
            {{{0,0, 1,0, 0,0, 1,0}}, 1},
            {{{0,0, 1,0, 0,0, -1,0}}, -1},
            {{{0,0, 1,0, 0,0, 0,1}}, 0},
            {{{0,0, largest,smallest, 0,0, largest,smallest}}, 1},
            {{{-largest,0, largest,0, 0,-largest, 0,largest}}, 0},
            {{{largest,largest, std::nextafter(largest,0.0),largest,
                largest,largest, largest,std::nextafter(largest,0.0)}}, 0}}};
        for ( const auto& vector : vectors )
        {
            const auto& p = vector.coordinates;
            check(exactDirectionDotSign(p[0],p[1],p[2],p[3],p[4],p[5],p[6],p[7])
                    == std::optional<int>{vector.sign}
                    && exactDirectionDotSign(p[2],p[3],p[0],p[1],p[4],p[5],p[6],p[7])
                    == std::optional<int>{-vector.sign},
                "Exact binary direction dot: fixed sign survives endpoint reversal and cancellation across the exponent range");
        }
        const std::array<double, 8> valid{{0,0, 1,0, 0,0, 1,0}};
        for ( const double invalid : {std::numeric_limits<double>::infinity(),
                -std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN()} )
        {
            for ( std::size_t component = 0; component < valid.size(); ++component )
            {
                auto values = valid;
                values[component] = invalid;
                check(!exactDirectionDotSign(values[0],values[1],values[2],values[3],
                        values[4],values[5],values[6],values[7]),
                    "Exact binary direction dot: every nonfinite coordinate rejects");
            }
        }
    }
    {
        using level::detail::exactIntersectionSideSign;
        struct Intersection
        {
            std::array<double, 12> coordinates;
            std::optional<int> side;
        };
        const double largest = std::numeric_limits<double>::max();
        const double smallest = std::numeric_limits<double>::denorm_min();
        const double adjacent = std::nextafter(1.0, 2.0);
        const std::array<Intersection, 6> vectors{{
            {{{0,0, 1,0, .5,-1, .5,1, 0,1, 1,1}}, -1},
            {{{0,0, 1,0, .5,-1, .5,1, 0,-1, 1,-1}}, 1},
            {{{0,0, 1,0, .5,-1, .5,1, 0,-1, 1,1}}, 0},
            {{{-largest,-largest, largest,-largest, 0,-largest, 0,largest,
                -largest,largest, largest,largest}}, -1},
            {{{0,0, smallest,0, 0,-smallest, 0,smallest,
                0,smallest, smallest,smallest}}, -1},
            {{{0,0, 1,0, 0,1, 1,adjacent, 0,-1, 1,-1}}, 1}}};
        for ( const auto& vector : vectors )
        {
            const auto& p = vector.coordinates;
            check(exactIntersectionSideSign(p[0],p[1],p[2],p[3],p[4],p[5],
                    p[6],p[7],p[8],p[9],p[10],p[11]) == vector.side,
                "Exact binary line intersection: fixed side vectors include concurrent, reversed and full-range coordinates");
            if ( vector.side )
            {
                check(exactIntersectionSideSign(p[2],p[3],p[0],p[1],p[4],p[5],p[6],p[7],
                        p[8],p[9],p[10],p[11]) == vector.side
                        && exactIntersectionSideSign(p[2],p[3],p[0],p[1],p[6],p[7],p[4],p[5],
                            p[8],p[9],p[10],p[11]) == vector.side
                        && exactIntersectionSideSign(p[0],p[1],p[2],p[3],p[4],p[5],p[6],p[7],
                            p[10],p[11],p[8],p[9])
                            == (*vector.side == 0 ? std::optional<int>{0}
                                : std::optional<int>{-*vector.side}),
                    "Exact binary line intersection: support exchange, endpoint reversal and target reversal preserve the fixed side law");
            }
        }
        const std::array<double, 12> valid{{0,0, 1,0, .5,-1, .5,1, 0,1, 1,1}};
        for ( const double invalidValue : {std::numeric_limits<double>::infinity(),
                -std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN()} )
        {
            for ( std::size_t component = 0; component < valid.size(); ++component )
            {
                auto invalid = valid;
                invalid[component] = invalidValue;
                check(!exactIntersectionSideSign(invalid[0],invalid[1],invalid[2],invalid[3],
                        invalid[4],invalid[5],invalid[6],invalid[7],invalid[8],invalid[9],
                        invalid[10],invalid[11]),
                    "Exact binary line intersection: every nonfinite endpoint coordinate rejects");
            }
        }
        check(!exactIntersectionSideSign(0,0,0,0, .5,-1, .5,1, 0,1, 1,1)
                && !exactIntersectionSideSign(0,0,1,0, 0,1, 1,1, 0,1, 1,2)
                && !exactIntersectionSideSign(0,0,1,0, .5,-1, .5,1, 0,1, 0,1),
            "Exact binary line intersection: degenerate defining, parallel and target supports remain unresolved");
    }
    {
        // The normalized directions differ by only 2.996e-14 while their
        // immutable dyadic supports have a finite crossing just beyond the
        // first segment. The propagated projection interval is wider than
        // that determinant, so a floating-point tolerance check alone would
        // reject a valid arrangement node.
        Registry registry({0,0,1}, {.5,0,0});
        const auto world = [](const double x, const double y)
        {
            return TVector{.5-y,x,0};
        };
        // These local ordinates are exact multiples of the .5-frame ulp, so
        // world -> local projection preserves the immutable diagnostic
        // operands through the deliberately cancelling translated frame.
        constexpr double firstY = -0x1.08p-47;
        constexpr double secondY = -0x1.cp-49;
        constexpr double thirdY = 0x1.88p-48;
        const auto a = registry.addOriginal(world(0,0));
        const auto b = registry.addOriginal(world(0.38358335421907808,firstY));
        const auto c = registry.addOriginal(world(0.77726101512840584,secondY));
        const auto d = registry.addOriginal(world(1.5646163369470609,thirdY));
        const auto first = registry.addLine(a,b);
        const auto second = registry.addLine(c,d);
        const auto parameter = level::detail::exactLineIntersectionParameter(
            0,0,0.38358335421907808,firstY,
            0.77726101512840584,secondY,
            1.5646163369470609,thirdY);
        constexpr double expectedParameter = 0x1.013c8f509f568p+0;
        check(parameter && std::isfinite(parameter->value)
                && std::abs(parameter->value-expectedParameter) <= parameter->error
                && parameter->error > 0.0 && std::isfinite(parameter->error),
            "Extrude registry near-parallel crossing: fixed endpoint ratio stays within its independent conversion bound");
        const auto reversedFirstParameter = level::detail::exactLineIntersectionParameter(
            0.38358335421907808,firstY,0,0,
            0.77726101512840584,secondY,
            1.5646163369470609,thirdY);
        const auto secondParameter = level::detail::exactLineIntersectionParameter(
            0.77726101512840584,secondY,
            1.5646163369470609,thirdY,
            0,0,0.38358335421907808,firstY);
        const auto reversedSecondParameter = level::detail::exactLineIntersectionParameter(
            1.5646163369470609,thirdY,
            0.77726101512840584,secondY,
            0,0,0.38358335421907808,firstY);
        const auto complementsWithinBounds = [](const level::detail::ExactBinary64Ratio& first,
            const level::detail::ExactBinary64Ratio& reversed)
        {
            const double roundoff = 4.0 * std::numeric_limits<double>::epsilon()
                * std::max({1.0,std::abs(first.value),std::abs(reversed.value)});
            return std::isfinite(first.value) && std::isfinite(reversed.value)
                && std::isfinite(first.error) && std::isfinite(reversed.error)
                && first.error >= 0.0 && reversed.error >= 0.0
                && std::abs((first.value+reversed.value)-1.0)
                    <= first.error+reversed.error+roundoff;
        };
        check(parameter && reversedFirstParameter && secondParameter && reversedSecondParameter
                && complementsWithinBounds(*parameter,*reversedFirstParameter)
                && complementsWithinBounds(*secondParameter,*reversedSecondParameter),
            "Extrude registry near-parallel crossing: independently reconstructed forward and reversed parameters preserve complement identities");
        const auto crossing = registry.intersection(first,second);
        check(crossing && registry.intersection(second,first) == crossing,
            "Extrude registry near-parallel crossing: exact immutable ratio publishes one order-independent node");
        if ( crossing )
        {
            const auto& point = registry.point(*crossing);
            constexpr double expectedX = 0x1.0000000000042p-1;
            constexpr double expectedY = 0x1.8aafc87adccf9p-2;
            constexpr double kUlpBound = 8.0 * std::numeric_limits<double>::epsilon();
            const bool fixedVector = std::abs(point.x-expectedX) <= kUlpBound * std::max(1.0,std::abs(expectedX))
                    && std::abs(point.y-expectedY) <= kUlpBound * std::max(1.0,std::abs(expectedY))
                    && point.z == 0.0;
            if ( !fixedVector )
            {
                const auto& firstLine = registry.line(first);
                const auto& secondLine = registry.line(second);
                std::cerr << std::setprecision(17)
                    << "near-parallel diagnostic point=(" << point.x << "," << point.y << "," << point.z
                    << ") expected=(" << expectedX << "," << expectedY << ",0)"
                    << " ratio-forward=(" << (parameter ? parameter->value : 0.0) << ","
                    << (parameter ? parameter->error : 0.0) << ")"
                    << " ratio-reversed-first=(" << (reversedFirstParameter ? reversedFirstParameter->value : 0.0)
                    << "," << (reversedFirstParameter ? reversedFirstParameter->error : 0.0) << ")"
                    << " ratio-second=(" << (secondParameter ? secondParameter->value : 0.0) << ","
                    << (secondParameter ? secondParameter->error : 0.0) << ")"
                    << " ratio-reversed-second=(" << (reversedSecondParameter ? reversedSecondParameter->value : 0.0)
                    << "," << (reversedSecondParameter ? reversedSecondParameter->error : 0.0) << ")"
                    << " first=(" << firstLine.first.x << "," << firstLine.first.y << "," << firstLine.first.z
                    << ")->(" << firstLine.second.x << "," << firstLine.second.y << "," << firstLine.second.z
                    << ") second=(" << secondLine.first.x << "," << secondLine.first.y << "," << secondLine.first.z
                    << ")->(" << secondLine.second.x << "," << secondLine.second.y << "," << secondLine.second.z
                    << ")\n";
            }
            check(fixedVector,
                "Extrude registry near-parallel crossing: fixed dyadic ratio reconstructs the documented vector within the fixed scaled binary64 bound");
            check(registry.incident(*crossing,first) && registry.incident(*crossing,second),
                "Extrude registry near-parallel crossing: generated node retains both immutable support incidences");
        }
    }
    for ( int order = 0; order < 8; ++order )
    {
        // The same immutable supports are entered with each endpoint and line
        // insertion order. This protects the exact fallback from selecting a
        // different origin and reintroducing cancellation at a far endpoint.
        Registry registry({0,0,1}, {.5,0,0});
        const auto world = [](const double x, const double y)
        {
            return TVector{.5-y,x,0};
        };
        constexpr double firstY = -0x1.08p-47;
        constexpr double secondY = -0x1.cp-49;
        constexpr double thirdY = 0x1.88p-48;
        const auto a = registry.addOriginal(world(0,0));
        const auto b = registry.addOriginal(world(0.38358335421907808,firstY));
        const auto c = registry.addOriginal(world(0.77726101512840584,secondY));
        const auto d = registry.addOriginal(world(1.5646163369470609,thirdY));
        const auto addFirst = [&]()
        {
            return (order & 1) ? registry.addLine(b,a) : registry.addLine(a,b);
        };
        const auto addSecond = [&]()
        {
            return (order & 2) ? registry.addLine(d,c) : registry.addLine(c,d);
        };
        Registry::LineId first, second;
        if ( order & 4 )
        {
            second = addSecond();
            first = addFirst();
        }
        else
        {
            first = addFirst();
            second = addSecond();
        }
        const auto hit = registry.intersection(first,second);
        check(hit && registry.intersection(second,first) == hit,
            "Extrude registry near-parallel crossing: endpoint and query order retain one intersection identity");
        if ( hit )
        {
            const auto& point = registry.point(*hit);
            constexpr double expectedX = 0x1.0000000000042p-1;
            constexpr double expectedY = 0x1.8aafc87adccf9p-2;
            constexpr double kUlpBound = 8.0 * std::numeric_limits<double>::epsilon();
            check(std::abs(point.x-expectedX) <= kUlpBound * std::max(1.0,std::abs(expectedX))
                    && std::abs(point.y-expectedY) <= kUlpBound * std::max(1.0,std::abs(expectedY))
                    && point.z == 0.0 && registry.incident(*hit,first)
                    && registry.incident(*hit,second),
                "Extrude registry near-parallel crossing: every support orientation retains the fixed finite crossing and both incidences within the scaled binary64 bound");
        }
    }
    for ( int order = 0; order < 16; ++order )
    {
        Registry registry({0,0,1}, {.5,0,0});
        // All coordinates are independently exact dyadic values. The two
        // near-collinear supports meet at corner2; point0 lies beyond both
        // segments and has strictly opposite orientation signs to their lines.
        const double unit = std::ldexp(1.0, -48);
        const std::array<TVector, 4> points{{{.5 + 5 * unit,.375,0},
            {.5 + .5 * unit,.75,0}, {.5 + 9 * unit,1.5,0}, {.5,1.84375,0}}};
        std::array<Registry::NodeId, 4> nodes{};
        for ( std::size_t index = 0; index < points.size(); ++index )
        {
            const auto source = (order & 1) ? points.size() - 1 - index : index;
            nodes[source] = registry.addOriginal(points[source]);
        }
        const auto addFirst = [&]()
        {
            return registry.addLine(nodes[(order & 4) ? 2 : 1], nodes[(order & 4) ? 1 : 2]);
        };
        const auto addSecond = [&]()
        {
            return registry.addLine(nodes[(order & 8) ? 3 : 2], nodes[(order & 8) ? 2 : 3]);
        };
        Registry::LineId first, second;
        if ( order & 2 )
        {
            second = addSecond();
            first = addFirst();
        }
        else
        {
            first = addFirst();
            second = addSecond();
        }
        check(registry.initializeOriginalIncidences() && registry.initializeOriginalIncidences()
                && registry.classify(nodes[0],first) == std::optional<int>{(order & 4) ? -1 : 1}
                && registry.classify(nodes[0],second) == std::optional<int>{(order & 8) ? 1 : -1}
                && !registry.incident(nodes[0],first) && !registry.incident(nodes[0],second)
                && !registry.registerIncidence(nodes[0],first) && !registry.registerIncidence(nodes[0],second),
            "Extrude registry shared corner: exact strict signs replace competing extension incidences independent of every input order");
        const auto retained = registry.intersection(first,second);
        check(retained == std::optional<Registry::NodeId>{nodes[2]}
                && registry.point(nodes[0]).x == points[0].x && registry.point(nodes[0]).y == points[0].y
                && registry.point(nodes[2]).x == points[2].x && registry.point(nodes[2]).y == points[2].y
                && registry.addOriginal({4,4,0}) == 4,
            "Extrude registry shared corner: exact authored crossing and exterior point remain distinct without consuming IDs");
    }
    {
        // Case30 from the generic quantized extrusion probe has a determinant
        // that is certain after direction normalization, while the signed
        // distance denominator for the second support is inside its propagated
        // uncertainty interval. The operands below are the immutable local
        // values captured by that trace, rebuilt through the same translated
        // .5-frame used by the production projection path.
        constexpr double firstEndX = 0x1.88ca1326a4c94p-2;
        constexpr double firstEndY = -0x1.3ffffffe00000p-44;
        constexpr double secondStartX = 0x1.8df527e0dd51cp-1;
        constexpr double secondStartY = -0x1.0bfffff400000p-45;
        constexpr double secondEndX = 0x1.908ab23df995fp+0;
        constexpr double secondEndY = 0x1.da80000800000p-45;
        const auto world = [](const double x, const double y)
        {
            return TVector{.5-y,x,0};
        };
        constexpr double expectedWorldX = 0x1.0000000000280p-1;
        constexpr double expectedWorldY = 0x1.887eca2027db4p-2;
        constexpr double kFixedCoordinateBound = 8.0 * std::numeric_limits<double>::epsilon();
        const auto fixedCrossing = [&](const Registry& registry, const std::optional<Registry::NodeId> hit)
        {
            if ( !hit )
            {
                return false;
            }
            const auto& point = registry.point(*hit);
            return std::abs(point.x-expectedWorldX) <= kFixedCoordinateBound
                && std::abs(point.y-expectedWorldY) <= kFixedCoordinateBound
                && point.z == 0.0;
        };
        Registry registry({0,0,1}, {.5,0,0});
        const auto a = registry.addOriginal(world(0,0));
        const auto b = registry.addOriginal(world(firstEndX,firstEndY));
        const auto c = registry.addOriginal(world(secondStartX,secondStartY));
        const auto d = registry.addOriginal(world(secondEndX,secondEndY));
        const auto first = registry.addLine(a,b);
        const auto second = registry.addLine(c,d);
        const auto hit = registry.intersection(first,second);
        check(hit && registry.intersection(second,first) == hit && fixedCrossing(registry,hit)
                && registry.incident(*hit,first) && registry.incident(*hit,second),
            "Extrude registry case30: determinant-certain crossing survives an uncertain signed-distance denominator with fixed Fraction-derived coordinates");

        for ( int order = 0; order < 4; ++order )
        {
            Registry ordered({0,0,1}, {.5,0,0});
            const auto oa = ordered.addOriginal(world(0,0));
            const auto ob = ordered.addOriginal(world(firstEndX,firstEndY));
            const auto oc = ordered.addOriginal(world(secondStartX,secondStartY));
            const auto od = ordered.addOriginal(world(secondEndX,secondEndY));
            const auto orderedFirst = (order & 1) ? ordered.addLine(ob,oa) : ordered.addLine(oa,ob);
            const auto orderedSecond = (order & 2) ? ordered.addLine(od,oc) : ordered.addLine(oc,od);
            const auto orderedHit = ordered.intersection(orderedFirst,orderedSecond);
            check(orderedHit && ordered.intersection(orderedSecond,orderedFirst) == orderedHit
                    && fixedCrossing(ordered,orderedHit)
                    && ordered.incident(*orderedHit,orderedFirst)
                    && ordered.incident(*orderedHit,orderedSecond),
                "Extrude registry case30: endpoint and query order preserve the same finite crossing and both support incidences");
        }

        Registry parallel({0,0,1}, {.5,0,0});
        const auto pa = parallel.addOriginal(world(0,0));
        const auto pb = parallel.addOriginal(world(1,1));
        const auto pc = parallel.addOriginal(world(0,1));
        const auto pd = parallel.addOriginal(world(1,2));
        const auto parallelFirst = parallel.addLine(pa,pb);
        const auto parallelSecond = parallel.addLine(pc,pd);
        check(!parallel.intersection(parallelFirst,parallelSecond),
            "Extrude registry case30: exact parallel supports remain unresolved instead of fabricating a crossing");
    }
    {
        // The generated crossing is only 5e-10 from the target support while
        // the translated local-coordinate error interval is about 1e-9. It
        // is nevertheless on a fixed side by the immutable construction
        // lines. A rounded generated point must not be registered as a second
        // crossing at the explicit shared endpoint.
        Registry registry({0,0,1}, {0,0,0});
        constexpr double origin = 100000.0;
        constexpr double slope = 1e-9;
        const auto shared = registry.addOriginal({-origin,origin,0});
        const auto alongEnd = registry.addOriginal({-origin,origin + 1.0,0});
        const auto targetEnd = registry.addOriginal({-origin - slope,origin + 1.0,0});
        const auto crossStart = registry.addOriginal({-origin + 1.0,origin + .5,0});
        const auto crossEnd = registry.addOriginal({-origin - 1.0,origin + .5,0});
        const auto along = registry.addLine(shared,alongEnd);
        const auto target = registry.addLine(shared,targetEnd);
        const auto crossingLine = registry.addLine(crossStart,crossEnd);
        const bool initialized = registry.initializeOriginalIncidences();
        const auto generated = registry.intersection(along,crossingLine);
        check(initialized && generated && *generated >= 5,
            "Extrude registry generated-side fixture: immutable crossing and original incidences are available");
        if ( generated )
        {
            check(registry.classify(*generated,target) == std::optional<int>{1}
                    && !registry.incident(*generated,target)
                    && !registry.registerIncidence(*generated,target),
                "Extrude registry generated-side classification: defining-line certificate resolves a near shared-endpoint side");
        }
    }
    {
        // Two distinct immutable supports cross the same common support at
        // coordinates which round to one stored generated point. The exact
        // crossings still have a strict order along the common support.
        constexpr TVector normal{-1.9893756370215481e-06,-0.99065819971714875,0.13636836630692789};
        constexpr TVector anchor{3.5,3.1353142,3.983};
        const std::array<TVector, 5> points{{
            {3.000000000000008,2.8747356039919749,2.0899999994504874},
            {3.5000000000000711,2.9270432353574907,2.469999995132889},
            {3.5000000000000298,2.9807284148273347,2.8599999979589534},
            {3.4999999999999472,3.0880987737670229,3.6400000036110822},
            {3.5,3.1353141999999998,3.9830000000000001}}};
        for ( int order = 0; order < 8; ++order )
        {
            Registry registry(normal,anchor);
            const auto node0 = registry.addOriginal(points[0]);
            const auto node2 = registry.addOriginal(points[1]);
            const auto node3 = registry.addOriginal(points[2]);
            const auto node4 = registry.addOriginal(points[3]);
            const auto node5 = registry.addOriginal(points[4]);
            const auto firstSupport = registry.addLine((order & 1) ? node3 : node2,
                (order & 1) ? node2 : node3);
            const auto secondSupport = registry.addLine((order & 2) ? node4 : node3,
                (order & 2) ? node3 : node4);
            const auto common = registry.addLine((order & 4) ? node0 : node5,
                (order & 4) ? node5 : node0);
            const auto firstCrossing = registry.intersection(firstSupport,common);
            const auto secondCrossing = registry.intersection(secondSupport,common);
            const int expectedCrossingOrder = (order & 4) ? 1 : -1;
            const int expectedFromStart = (order & 4) ? -1 : 1;
            check(registry.valid() && firstCrossing && secondCrossing
                    && *firstCrossing != *secondCrossing
                    && !registry.isOriginalNode(*firstCrossing)
                    && !registry.isOriginalNode(*secondCrossing)
                    && registry.point(*firstCrossing).x == registry.point(*secondCrossing).x
                    && registry.point(*firstCrossing).y == registry.point(*secondCrossing).y
                    && registry.point(*firstCrossing).z == registry.point(*secondCrossing).z,
                "Extrude registry node ordering: distinct generated crossings retain identity after rounded coordinates collapse");
            if ( firstCrossing && secondCrossing )
            {
                check(registry.exactNodeOrderOnLine(*firstCrossing,*secondCrossing,common)
                        == std::optional<int>{expectedCrossingOrder}
                        && registry.exactNodeOrderOnLine(*secondCrossing,*firstCrossing,common)
                        == std::optional<int>{-expectedCrossingOrder},
                    "Extrude registry node ordering: immutable defining supports recover both directed orders");
                check(registry.exactNodeOrderOnLine(node5,*firstCrossing,common)
                        == std::optional<int>{expectedFromStart}
                        && registry.exactNodeOrderOnLine(*firstCrossing,node5,common)
                        == std::optional<int>{-expectedFromStart}
                        && registry.exactNodeOrderOnLine(node5,node0,common)
                        == std::optional<int>{expectedFromStart}
                        && registry.exactNodeOrderOnLine(node0,node5,common)
                        == std::optional<int>{-expectedFromStart},
                    "Extrude registry node ordering: mixed generated/authored and authored/authored endpoints use immutable support order");
            }
            check(!registry.exactNodeOrderOnLine(Registry::kInvalidNode,node5,common)
                    && !registry.exactNodeOrderOnLine(node5,node5,common)
                    && !registry.exactNodeOrderOnLine(node5,node0,Registry::kInvalidLine),
                "Extrude registry node ordering: invalid and duplicate endpoint requests remain unresolved");
        }
    }
    for ( const double normalZ : {1.0,-1.0} )
    {
        Registry registry({0,0,normalZ},{0,0,0});
        const auto commonStart = registry.addOriginal({0,0,0});
        const auto commonFinish = registry.addOriginal({0,2,0});
        const auto firstStart = registry.addOriginal({-1,1,0});
        const auto firstFinish = registry.addOriginal({1,1,0});
        const auto secondStart = registry.addOriginal({-2,1,0});
        const auto secondFinish = registry.addOriginal({2,1,0});
        const auto common = registry.addLine(commonStart,commonFinish);
        const auto first = registry.addLine(firstStart,firstFinish);
        const auto second = registry.addLine(secondStart,secondFinish);
        const auto firstCrossing = registry.intersection(common,first);
        const auto secondCrossing = registry.intersection(common,second);
        check(firstCrossing && secondCrossing && *firstCrossing == *secondCrossing
                && !registry.isOriginalNode(*firstCrossing)
                && registry.point(*firstCrossing).x == 0
                && registry.point(*firstCrossing).y == 1
                && registry.point(*firstCrossing).z == 0
                && registry.classify(*firstCrossing,common) == std::optional<int>{0}
                && registry.classify(*firstCrossing,first) == std::optional<int>{0}
                && registry.classify(*firstCrossing,second) == std::optional<int>{0}
                && registry.incident(*firstCrossing,common)
                && registry.incident(*firstCrossing,first)
                && registry.incident(*firstCrossing,second)
                && registry.exactNodeOrderOnLine(commonStart,*firstCrossing,common)
                    == std::optional<int>{1}
                && registry.exactNodeOrderOnLine(*firstCrossing,commonStart,common)
                    == std::optional<int>{-1},
            "Extrude registry node ordering: exact coincident supports reuse one fixed crossing while preserving directed authored order");
    }
    for ( const double normalZ : {1.0,-1.0} )
    {
        // Distinct supports can meet a common support at the same exact
        // point. They retain separate generated identities so exact ordering
        // can resolve their concurrent crossings to zero.
        Registry registry({0,0,normalZ},{0,0,0});
        const auto commonStart = registry.addOriginal({0,0,0});
        const auto commonFinish = registry.addOriginal({0,2,0});
        const auto firstStart = registry.addOriginal({-1,1,0});
        const auto firstFinish = registry.addOriginal({1,1,0});
        const auto secondStart = registry.addOriginal({-2,0,0});
        const auto secondFinish = registry.addOriginal({2,2,0});
        const auto common = registry.addLine(commonStart,commonFinish);
        const auto first = registry.addLine(firstStart,firstFinish);
        const auto second = registry.addLine(secondStart,secondFinish);
        const auto firstCrossing = registry.intersection(common,first);
        const auto secondCrossing = registry.intersection(common,second);
        check(firstCrossing && secondCrossing && *firstCrossing != *secondCrossing
                && !registry.isOriginalNode(*firstCrossing)
                && !registry.isOriginalNode(*secondCrossing)
                && registry.point(*firstCrossing).x == 0
                && registry.point(*firstCrossing).y == 1
                && registry.point(*firstCrossing).z == 0
                && registry.point(*firstCrossing).x == registry.point(*secondCrossing).x
                && registry.point(*firstCrossing).y == registry.point(*secondCrossing).y
                && registry.point(*firstCrossing).z == registry.point(*secondCrossing).z
                && registry.exactNodeOrderOnLine(*firstCrossing,*secondCrossing,common)
                    == std::optional<int>{0}
                && registry.nodeOrderOnLine(*firstCrossing,*secondCrossing,common)
                    == std::optional<int>{0}
                && registry.exactNodeOrderOnLine(commonStart,*firstCrossing,common)
                    == std::optional<int>{1}
                && registry.exactNodeOrderOnLine(*firstCrossing,commonStart,common)
                    == std::optional<int>{-1},
            "Extrude registry node ordering: distinct concurrent crossings retain separate identities and exact zero order");
    }
    for ( const bool reverseQuery : {false,true} )
    {
        Registry registry({0,0,1},{0,0,0});
        const double adjacent = std::nextafter(1.0,2.0);
        const auto baseStart = registry.addOriginal({0,1,0});
        const auto baseFinish = registry.addOriginal({2,1,0});
        const auto nearStart = registry.addOriginal({0,adjacent,0});
        const auto nearFinish = registry.addOriginal({2,adjacent,0});
        const auto transversalStart = registry.addOriginal({1,-1,0});
        const auto transversalFinish = registry.addOriginal({1,3,0});
        const auto base = registry.addLine(baseStart,baseFinish);
        const auto near = registry.addLine(nearStart,nearFinish);
        const auto transversal = registry.addLine(transversalStart,transversalFinish);
        const auto firstCrossing = reverseQuery
            ? registry.intersection(transversal,base)
            : registry.intersection(base,transversal);
        const auto secondCrossing = reverseQuery
            ? registry.intersection(transversal,near)
            : registry.intersection(near,transversal);
        const auto reverseFirst = registry.intersection(base,transversal);
        const auto reverseSecond = registry.intersection(near,transversal);
        check(base != near && firstCrossing && secondCrossing
                && *firstCrossing != *secondCrossing
                && reverseFirst == firstCrossing && reverseSecond == secondCrossing
                && registry.point(*firstCrossing).x == 1
                && registry.point(*firstCrossing).y == 1
                && registry.point(*firstCrossing).z == 0
                && registry.point(*secondCrossing).x == 1
                && registry.point(*secondCrossing).y == adjacent
                && registry.point(*secondCrossing).z == 0
                && !registry.incident(*firstCrossing,near)
                && !registry.incident(*secondCrossing,base)
                && !registry.intersection(base,near)
                && registry.point(baseStart).x == 0 && registry.point(baseStart).y == 1
                && registry.point(nearStart).x == 0 && registry.point(nearStart).y == adjacent,
            "Extrude registry: near-but-distinct parallel supports never alias a crossing under either query order");
    }
    for ( const double normalZ : {1.0,-1.0} )
    {
        for ( const bool reverseCommon : {false,true} )
        {
            Registry registry({0,0,normalZ},{0,0,0});
            const double adjacent = std::nextafter(1.0,2.0);
            const auto commonStart = registry.addOriginal({0,1,0});
            const auto commonFinish = registry.addOriginal({10,1,0});
            const auto supportStart = registry.addOriginal({3,0,0});
            const auto supportFinish = registry.addOriginal({3,2,0});
            const auto nearCommon = registry.addOriginal({5,adjacent,0});
            const auto ambiguous = registry.addOriginal({3,adjacent,0});
            const auto offSupport = registry.addOriginal({5,2,0});
            const auto common = reverseCommon
                ? registry.addLine(commonFinish,commonStart)
                : registry.addLine(commonStart,commonFinish);
            const auto support = registry.addLine(supportStart,supportFinish);
            const auto crossing = registry.intersection(common,support);
            const int expected = reverseCommon ? -1 : 1;
            const bool registeredNearCommon = registry.registerIncidence(nearCommon,common);
            const bool registeredAmbiguous = registry.registerIncidence(ambiguous,common);
            check(crossing && registeredNearCommon && registeredAmbiguous,
                "Extrude registry node ordering: near-line fixture admits both authored identities with nonzero stored uncertainty");
            check(crossing && registeredNearCommon
                    && registeredAmbiguous
                    && !registry.exactNodeOrderOnLine(*crossing,nearCommon,common)
                    && registry.certifiedNodeOrderOnLine(*crossing,nearCommon,common)
                        == std::optional<int>{expected}
                    && registry.nodeOrderOnLine(*crossing,nearCommon,common)
                        == std::optional<int>{expected}
                    && registry.nodeOrderOnLine(nearCommon,*crossing,common)
                        == std::optional<int>{-expected},
                "Extrude registry node ordering: interval-certified mixed generated/authored order survives normal and support reversal");
            check(crossing && registeredNearCommon
                    && registeredAmbiguous
                    && !registry.exactNodeOrderOnLine(*crossing,ambiguous,common)
                    && !registry.certifiedNodeOrderOnLine(*crossing,ambiguous,common)
                    && !registry.nodeOrderOnLine(*crossing,ambiguous,common),
                "Extrude registry node ordering: an interval spanning zero remains unresolved instead of inventing order");
            check(crossing
                    && !registry.certifiedNodeOrderOnLine(*crossing,offSupport,common)
                    && !registry.nodeOrderOnLine(*crossing,offSupport,common),
                "Extrude registry node ordering: an off-support pair cannot acquire order from a projected dot product");
        }
    }
    for ( const bool reverseCommon : {false,true} )
    {
        for ( const bool reverseSupports : {false,true} )
        {
            Registry registry({0,0,1},{0,0,0});
            const auto commonStart = registry.addOriginal({0,0,0});
            const auto commonFinish = registry.addOriginal({10,0,0});
            const auto firstStart = registry.addOriginal({3,-1,0});
            const auto firstFinish = registry.addOriginal({3,1,0});
            const auto firstAlongStart = registry.addOriginal({0.1,0,0});
            const auto firstAlongFinish = registry.addOriginal({10.1,0,0});
            const auto secondStart = registry.addOriginal({7,-1,0});
            const auto secondFinish = registry.addOriginal({7,1,0});
            const auto secondAlongStart = registry.addOriginal({0.2,0,0});
            const auto secondAlongFinish = registry.addOriginal({10.2,0,0});
            const auto common = reverseCommon
                ? registry.addLine(commonFinish,commonStart)
                : registry.addLine(commonStart,commonFinish);
            const auto first = reverseSupports
                ? registry.addLine(firstFinish,firstStart)
                : registry.addLine(firstStart,firstFinish);
            const auto firstAlong = registry.addLine(firstAlongStart,firstAlongFinish);
            const auto second = reverseSupports
                ? registry.addLine(secondFinish,secondStart)
                : registry.addLine(secondStart,secondFinish);
            const auto secondAlong = registry.addLine(secondAlongStart,secondAlongFinish);
            const auto firstCrossing = registry.intersection(first,firstAlong);
            const auto secondCrossing = registry.intersection(second,secondAlong);
            const int expected = reverseCommon ? -1 : 1;
            const bool registeredCrossings = firstCrossing && secondCrossing
                && registry.registerIncidence(*firstCrossing,common)
                && registry.registerIncidence(*secondCrossing,common);
            check(firstCrossing && secondCrossing && registeredCrossings
                    && !registry.exactNodeOrderOnLine(*firstCrossing,*secondCrossing,common)
                    && registry.certifiedNodeOrderOnLine(*firstCrossing,*secondCrossing,common)
                        == std::optional<int>{expected}
                    && registry.nodeOrderOnLine(*firstCrossing,*secondCrossing,common)
                        == std::optional<int>{expected}
                    && registry.nodeOrderOnLine(*secondCrossing,*firstCrossing,common)
                        == std::optional<int>{-expected},
                "Extrude registry node ordering: third-support generated crossings use a nonzero interval certificate independent of support order");
        }
    }
    {
        Registry registry({0,0,1},{0,0,0});
        const auto first = registry.addOriginal({0,0,0});
        const auto second = registry.addOriginal({1,0,0});
        const auto line = registry.addLine(first,second);
        const auto invalidNode = registry.addOriginal({std::numeric_limits<double>::infinity(),0,0});
        check(!registry.certifiedNodeOrderOnLine(Registry::kInvalidNode,second,line)
                && !registry.certifiedNodeOrderOnLine(first,first,line)
                && !registry.certifiedNodeOrderOnLine(first,second,Registry::kInvalidLine)
                && !registry.certifiedNodeOrderOnLine(invalidNode,second,line),
            "Extrude registry node ordering: invalid and nonfinite interval operands fail closed");
    }
    {
        const double smallest = std::numeric_limits<double>::denorm_min();
        Registry registry({0,0,1},{0,0,0});
        const auto first = registry.addOriginal({0,smallest,0});
        const auto second = registry.addOriginal({0,4.0 * smallest,0});
        const auto line = registry.addLine(first,second);
        check(line != Registry::kInvalidLine
                && registry.certifiedNodeOrderOnLine(first,second,line) == std::optional<int>{1}
                && registry.certifiedNodeOrderOnLine(second,first,line) == std::optional<int>{-1},
            "Extrude registry node ordering: subnormal local coordinates use relative scale without losing a directed order");
    }
    {
        const double adjacent = std::nextafter(1.0,2.0);
        Registry registry({0,0,1},{0,0,0});
        const auto start = registry.addOriginal({0,0,0});
        const auto finish = registry.addOriginal({2,0,0});
        const auto first = registry.addOriginal({1,0,0});
        const auto second = registry.addOriginal({adjacent,0,0});
        const auto line = registry.addLine(start,finish);
        const bool registered = registry.registerIncidence(first,line)
            && registry.registerIncidence(second,line);
        check(registered && !registry.certifiedNodeOrderOnLine(first,second,line)
                && !registry.certifiedNodeOrderOnLine(second,first,line),
            "Extrude registry node ordering: a one-ULP separation remains unresolved when its scaled interval includes zero");
    }
    {
        const double base = std::ldexp(1.0,900);
        const double step = std::ldexp(1.0,880);
        Registry registry({0,0,1},{0,0,0});
        const auto first = registry.addOriginal({base,0,0});
        const auto second = registry.addOriginal({base + step,0,0});
        const auto line = registry.addLine(first,second);
        check(line != Registry::kInvalidLine
                && registry.certifiedNodeOrderOnLine(first,second,line) == std::optional<int>{1}
                && registry.certifiedNodeOrderOnLine(second,first,line) == std::optional<int>{-1},
            "Extrude registry node ordering: large translated finite coordinates use a relative interval scale");
    }
    {
        const double largest = std::numeric_limits<double>::max();
        Registry registry({0,0,1},{0,0,0});
        const auto first = registry.addOriginal({-largest,0,0});
        const auto second = registry.addOriginal({largest,0,0});
        const auto line = registry.addLine(first,second);
        check(line == Registry::kInvalidLine
                && !registry.certifiedNodeOrderOnLine(first,second,line),
            "Extrude registry node ordering: overflowing support operands fail closed instead of wrapping an interval");
    }
    {
        Registry registry({0,0,1}, {0,0,0});
        constexpr double origin = 100000.0;
        const double middle = origin + .5;
        const double next = std::nextafter(middle, std::numeric_limits<double>::infinity());
        const double step = next - middle;
        const auto a = registry.addOriginal({-origin,origin-.5,0});
        const auto b = registry.addOriginal({-origin,origin+1.5,0});
        const auto c = registry.addOriginal({-origin-1,origin+.5,0});
        const auto d = registry.addOriginal({-origin+1,origin+.5,0});
        const auto e = registry.addOriginal({-origin-1,origin+1.5+step,0});
        const auto f = registry.addOriginal({-origin+1,origin-.5+step,0});
        const auto definingA = registry.addLine(a,b);
        const auto definingB = registry.addLine(c,d);
        const auto target = registry.addLine(e,f);
        const bool initialized = registry.initializeOriginalIncidences();
        const auto generated = registry.intersection(definingA,definingB);
        const auto expected = level::detail::exactIntersectionSideSign(
            origin * -1,origin - .5,origin * -1,origin + 1.5,
            origin * -1 - 1,origin + .5,origin * -1 + 1,origin + .5,
            origin * -1 - 1,origin + 1.5 + step,origin * -1 + 1,origin - .5 + step);
        check(initialized && generated && expected == std::optional<int>{-1}
                && registry.classify(*generated,target) == std::optional<int>{1}
                && !registry.incident(*generated,target),
            "Extrude registry generated-side classification: exact four-coordinate intersection resolves an unrelated near support");
    }
    for ( const bool reverseOrder : {false, true} )
    {
        Registry registry({0,0,1}, {0,0,0});
        const double adjacent = std::nextafter(1.0, 2.0);
        const auto a = registry.addOriginal({0,1,0});
        const auto b = registry.addOriginal({2,1,0});
        const auto authored = registry.addOriginal({1,adjacent,0});
        const auto d = registry.addOriginal({1,2,0});
        const auto e = registry.addOriginal({1.5,-2,0});
        const auto f = registry.addOriginal({1.5,3,0});
        Registry::LineId horizontal, vertical;
        if ( reverseOrder )
        {
            vertical = registry.addLine(d,authored);
            horizontal = registry.addLine(b,a);
        }
        else
        {
            horizontal = registry.addLine(a,b);
            vertical = registry.addLine(authored,d);
        }
        const auto unrelated = registry.addLine(e,f);
        const bool initialized = registry.initializeOriginalIncidences();
        check(initialized && registry.initializeOriginalIncidences()
                && registry.incident(authored,horizontal) && registry.incident(authored,vertical),
            "Extrude registry initialization: all original incidences are seeded idempotently before generated crossings");
        const auto firstCrossing = registry.intersection(horizontal,unrelated);
        const auto retained = registry.intersection(horizontal,vertical);
        check(firstCrossing && *firstCrossing == 6 && retained == std::optional<Registry::NodeId>{authored}
                && registry.point(authored).x == 1 && registry.point(authored).y == adjacent
                && registry.point(authored).z == 0 && registry.classify(authored,horizontal) == std::optional<int>{0},
            "Extrude registry initialization: unrelated earlier crossing cannot steal the exact near-collinear original identity");
        check(!registry.initializeOriginalIncidences()
                && registry.intersection(vertical,horizontal) == retained && registry.addOriginal({4,4,0}) == 7,
            "Extrude registry initialization: late request rejects without consuming IDs or changing cached intersections");
    }
    {
        Registry registry({0,0,1}, {0,0,0});
        const auto a = registry.addOriginal({0,1,0});
        const auto b = registry.addOriginal({2,1,0});
        const auto c = registry.addOriginal({1,1,0});
        const auto d = registry.addOriginal({3,1,0});
        const auto first = registry.addLine(a,b);
        const auto second = registry.addLine(c,d);
        check(registry.initializeOriginalIncidences() && registry.incident(a,second)
                && registry.incident(b,second) && registry.incident(c,first) && registry.incident(d,first)
                && !registry.intersection(first,second) && registry.addOriginal({4,1,0}) == 4,
            "Extrude registry initialization: coincident supporting lines retain every distinct authored identity");
    }
    {
        Registry registry({0,0,1}, {0,0,0});
        const auto original = registry.addOriginal({1,1,0});
        const auto duplicate = registry.addOriginal({1,1,0});
        const auto a = registry.addOriginal({2,1,0});
        const auto b = registry.addOriginal({1,2,0});
        const auto first = registry.addLine(original,a);
        const auto second = registry.addLine(original,b);
        check(!registry.initializeOriginalIncidences() && !registry.incident(duplicate,first)
                && !registry.incident(duplicate,second)
                && registry.intersection(first,second) == std::optional<Registry::NodeId>{original}
                && registry.addOriginal({3,3,0}) == 4,
            "Extrude registry initialization: conflicting original identities reject atomically without partial incidence publication");
    }
    {
        // At a large translated frame the immutable local-coordinate error
        // interval is deliberately wider than these dyadic line offsets.
        // The two supports are distinct, have no shared endpoint, and every
        // endpoint of either support is therefore near the other support.
        // Their exact orientations at those endpoints are all positive, so
        // the registry-side classification is the fixed value -1. Two
        // different authored nodes are also near both supports; an input
        // traversal must not let the first one claim the other's crossing.
        constexpr double frame = 100000.0;
        constexpr double quantum = 0x1.0p-35;
        const auto world = [frame](const double x, const double y)
        {
            return TVector{frame-y,frame+x,0};
        };
        const std::array<TVector, 6> authored{{
            world(0,0), world(1,quantum),
            world(2,3*quantum), world(3,6*quantum),
            world(1.5,2.5*quantum), world(1.5,3.5*quantum)}};
        const auto samePoint = [](const TVector& first, const TVector& second)
        {
            return first.x == second.x && first.y == second.y && first.z == second.z;
        };
        for ( const bool reverseLines : {false,true} )
        {
            for ( const bool reverseCandidates : {false,true} )
            {
                Registry registry({0,0,1}, {frame,frame,0});
                const auto firstStart = registry.addOriginal(authored[0]);
                const auto firstFinish = registry.addOriginal(authored[1]);
                const auto secondStart = registry.addOriginal(authored[2]);
                const auto secondFinish = registry.addOriginal(authored[3]);
                Registry::NodeId candidateA{};
                Registry::NodeId candidateB{};
                if ( reverseCandidates )
                {
                    candidateB = registry.addOriginal(authored[5]);
                    candidateA = registry.addOriginal(authored[4]);
                }
                else
                {
                    candidateA = registry.addOriginal(authored[4]);
                    candidateB = registry.addOriginal(authored[5]);
                }
                const std::array<TVector, 6> before{{registry.point(firstStart),
                    registry.point(firstFinish),registry.point(secondStart),registry.point(secondFinish),
                    registry.point(candidateA),registry.point(candidateB)}};
                Registry::LineId first;
                Registry::LineId second;
                if ( reverseLines )
                {
                    second = registry.addLine(secondStart,secondFinish);
                    first = registry.addLine(firstFinish,firstStart);
                }
                else
                {
                    first = registry.addLine(firstStart,firstFinish);
                    second = registry.addLine(secondStart,secondFinish);
                }
                const auto distinctSupports = level::detail::exactDirectionCrossSign(
                    reverseLines ? 1.0 : 0.0,reverseLines ? quantum : 0.0,
                    reverseLines ? 0.0 : 1.0,reverseLines ? 0.0 : quantum,
                    2.0,3*quantum,3.0,6*quantum);
                check(distinctSupports && *distinctSupports == (reverseLines ? -1 : 1),
                    "Extrude registry initialization: near-parallel fixture supports are exactly distinct despite overlapping error intervals");
                const bool initialized = registry.initializeOriginalIncidences();
                check(initialized && registry.initializeOriginalIncidences(),
                    "Extrude registry initialization: distinct near-parallel supports seed atomically in every traversal order");
                if ( initialized )
                {
                    bool originalCoordinates = true;
                    const std::array<Registry::NodeId, 6> ids{{firstStart,firstFinish,secondStart,
                        secondFinish,candidateA,candidateB}};
                    for ( std::size_t index = 0; index < ids.size(); ++index )
                    {
                        originalCoordinates = originalCoordinates && samePoint(registry.point(ids[index]),before[index]);
                    }
                    const int expectedFirstSide = reverseLines ? 1 : -1;
                    const int expectedSecondSide = -1;
                    const bool strictSides = registry.classify(firstStart,second)
                            == std::optional<int>{expectedSecondSide}
                        && registry.classify(firstFinish,second)
                            == std::optional<int>{expectedSecondSide}
                        && registry.classify(secondStart,first)
                            == std::optional<int>{expectedFirstSide}
                        && registry.classify(secondFinish,first)
                            == std::optional<int>{expectedFirstSide}
                        && registry.classify(candidateA,first)
                            == std::optional<int>{expectedFirstSide}
                        && registry.classify(candidateA,second)
                            == std::optional<int>{expectedSecondSide}
                        && registry.classify(candidateB,first)
                            == std::optional<int>{expectedFirstSide}
                        && registry.classify(candidateB,second)
                            == std::optional<int>{expectedSecondSide};
                    const bool noInventedCrossings = !registry.incident(firstStart,second)
                        && !registry.incident(firstFinish,second)
                        && !registry.incident(secondStart,first)
                        && !registry.incident(secondFinish,first)
                        && !registry.incident(candidateA,first)
                        && !registry.incident(candidateA,second)
                        && !registry.incident(candidateB,first)
                        && !registry.incident(candidateB,second)
                        && !registry.registerIncidence(candidateA,first)
                        && !registry.registerIncidence(candidateB,second)
                        && registry.addOriginal({frame+4,frame+4,0}) == 6;
                    check(originalCoordinates,
                        "Extrude registry initialization: exact authored coordinates and IDs survive every input order");
                    check(strictSides,
                        "Extrude registry initialization: exact nonzero sides use canonical support orientation after reversal");
                    check(noInventedCrossings,
                        "Extrude registry initialization: exact nonzero sides reject fabricated endpoint crossings");
                }
            }
        }
    }
    {
        // This is the minimal form of the original 54/55 conflict. The
        // supports have a shallow angle, their endpoints are outside each
        // other's uncertainty interval, and two distinct authored nodes sit
        // within the uncertainty interval of both supports. The first node
        // must not claim the line-pair identity before the second node is
        // classified by the immutable orientations.
        constexpr double frame = 100000.0;
        constexpr double slope = 0x1.0p-20;
        constexpr double quantum = 0x1.0p-35;
        const auto world = [frame](const double x, const double y)
        {
            return TVector{frame-y,frame+x,0};
        };
        const std::array<TVector, 6> authored{{
            world(-1,0), world(1,0),
            world(-1,-slope), world(1,slope),
            world(0,quantum), world(0,2*quantum)}};
        const auto samePoint = [](const TVector& first, const TVector& second)
        {
            return first.x == second.x && first.y == second.y && first.z == second.z;
        };
        for ( const bool reverseLines : {false,true} )
        {
            for ( const bool reverseCandidates : {false,true} )
            {
                Registry registry({0,0,1}, {frame,frame,0});
                const auto firstStart = registry.addOriginal(authored[0]);
                const auto firstFinish = registry.addOriginal(authored[1]);
                const auto secondStart = registry.addOriginal(authored[2]);
                const auto secondFinish = registry.addOriginal(authored[3]);
                Registry::NodeId candidateA{};
                Registry::NodeId candidateB{};
                if ( reverseCandidates )
                {
                    candidateB = registry.addOriginal(authored[5]);
                    candidateA = registry.addOriginal(authored[4]);
                }
                else
                {
                    candidateA = registry.addOriginal(authored[4]);
                    candidateB = registry.addOriginal(authored[5]);
                }
                const std::array<TVector, 6> before{{registry.point(firstStart),
                    registry.point(firstFinish),registry.point(secondStart),registry.point(secondFinish),
                    registry.point(candidateA),registry.point(candidateB)}};
                Registry::LineId first;
                Registry::LineId second;
                if ( reverseLines )
                {
                    second = registry.addLine(secondStart,secondFinish);
                    first = registry.addLine(firstFinish,firstStart);
                }
                else
                {
                    first = registry.addLine(firstStart,firstFinish);
                    second = registry.addLine(secondStart,secondFinish);
                }
                const auto distinctSupports = level::detail::exactDirectionCrossSign(
                    reverseLines ? 1.0 : -1.0,0.0,
                    reverseLines ? -1.0 : 1.0,0.0,
                    -1.0,-slope,1.0,slope);
                check(distinctSupports && *distinctSupports == (reverseLines ? -1 : 1),
                    "Extrude registry initialization: 54/55 supports are exactly distinct and have no shared endpoint");
                const bool initialized = registry.initializeOriginalIncidences();
                check(initialized && registry.initializeOriginalIncidences(),
                    "Extrude registry initialization: 54/55 near-line conflict resolves independently of node and line order");
                if ( initialized )
                {
                    const int expectedFirstCandidateSide = reverseLines ? 1 : -1;
                    const bool strictSides = registry.classify(firstStart,second) == std::optional<int>{-1}
                        && registry.classify(firstFinish,second) == std::optional<int>{1}
                        && registry.classify(secondStart,first)
                            == std::optional<int>{reverseLines ? -1 : 1}
                        && registry.classify(secondFinish,first)
                            == std::optional<int>{reverseLines ? 1 : -1}
                        && registry.classify(candidateA,first)
                            == std::optional<int>{expectedFirstCandidateSide}
                        && registry.classify(candidateA,second) == std::optional<int>{-1}
                        && registry.classify(candidateB,first)
                            == std::optional<int>{expectedFirstCandidateSide}
                        && registry.classify(candidateB,second) == std::optional<int>{-1};
                    bool originalCoordinates = true;
                    const std::array<Registry::NodeId, 6> ids{{firstStart,firstFinish,secondStart,
                        secondFinish,candidateA,candidateB}};
                    for ( std::size_t index = 0; index < ids.size(); ++index )
                    {
                        originalCoordinates = originalCoordinates && samePoint(registry.point(ids[index]),before[index]);
                    }
                    const bool noInventedCrossings = !registry.incident(firstStart,second)
                        && !registry.incident(firstFinish,second)
                        && !registry.incident(secondStart,first)
                        && !registry.incident(secondFinish,first)
                        && !registry.incident(candidateA,first)
                        && !registry.incident(candidateA,second)
                        && !registry.incident(candidateB,first)
                        && !registry.incident(candidateB,second)
                        && !registry.registerIncidence(candidateA,first)
                        && !registry.registerIncidence(candidateB,second)
                        && registry.addOriginal({frame+4,frame+4,0}) == 6;
                    check(strictSides,
                        "Extrude registry initialization: 54/55 immutable endpoint signs remain fixed after line reversal");
                    check(originalCoordinates,
                        "Extrude registry initialization: 54/55 authored coordinates and IDs remain unchanged");
                    check(noInventedCrossings,
                        "Extrude registry initialization: 54/55 does not publish a fabricated crossing identity");
                }
            }
        }
    }
    {
        // Exact coincident supports remain an ON-incidence case even when
        // their endpoints are distinct authored identities.
        constexpr double frame = 100000.0;
        constexpr double quantum = 0x1.0p-35;
        const auto world = [frame](const double x, const double y)
        {
            return TVector{frame-y,frame+x,0};
        };
        Registry registry({0,0,1}, {frame,frame,0});
        const auto firstStart = registry.addOriginal(world(0,0));
        const auto firstFinish = registry.addOriginal(world(1,quantum));
        const auto secondStart = registry.addOriginal(world(2,2*quantum));
        const auto secondFinish = registry.addOriginal(world(3,3*quantum));
        const auto first = registry.addLine(firstStart,firstFinish);
        const auto second = registry.addLine(secondStart,secondFinish);
        check(registry.initializeOriginalIncidences()
                && registry.incident(firstStart,second) && registry.incident(firstFinish,second)
                && registry.incident(secondStart,first) && registry.incident(secondFinish,first)
                && !registry.intersection(first,second)
                && registry.addOriginal(world(4,4*quantum)) == 4,
            "Extrude registry initialization: exact coincident supports retain distinct ON identities without a crossing");
    }
    for ( const bool reverse : {false,true} )
    {
        Registry registry({0,0,1}, {0,0,0});
        const auto a = registry.addOriginal({0,0,0});
        const auto b = registry.addOriginal({2,0,0});
        const auto c = registry.addOriginal({1,-1,0});
        const auto d = registry.addOriginal({1,1,0});
        const auto horizontal = registry.addLine(reverse ? b : a, reverse ? a : b);
        const auto vertical = registry.addLine(reverse ? d : c, reverse ? c : d);
        check(registry.valid() && a == 0 && b == 1 && c == 2 && d == 3
                && horizontal == 0 && vertical == 1 && registry.addLine(b,a) == horizontal,
            "Extrude registry: fixed input identities and undirected endpoint-line reuse are deterministic");
        const auto hit = registry.intersection(horizontal,vertical);
        check(hit && *hit == 4,
            "Extrude registry: the unique crossing publishes one new fixed node identity");
        if ( hit )
        {
            const auto& point = registry.point(*hit);
            check(point.x == 1 && point.y == 0 && point.z == 0
                    && registry.incident(*hit,horizontal) && registry.incident(*hit,vertical),
                "Extrude registry: both orientations yield the fixed crossing and both generating incidences");
            check(registry.intersection(vertical,horizontal) == hit && registry.intersection(horizontal,vertical) == hit
                    && registry.addOriginal({3,3,0}) == 5,
                "Extrude registry: repeated and reversed cache queries cannot create duplicate nodes");
            check(registry.addLine(*hit,a) == Registry::kInvalidLine && registry.addLine(b,c) == 2,
                "Extrude registry: a descendant intersection cannot become a new immutable line or consume a line identity");
        }
        check(!registry.intersection(horizontal,horizontal)
                && !registry.intersection(Registry::kInvalidLine,vertical),
            "Extrude registry: identical or invalid line requests have no unique intersection");
    }
    {
        Registry registry({0,0,1}, {0,0,0});
        const auto original = registry.addOriginal({0.123,0.456,0});
        const auto a = registry.addOriginal({2,0,0});
        const auto b = registry.addOriginal({0,2,0});
        const auto first = registry.addLine(original,a);
        const auto second = registry.addLine(b,original);
        const auto hit = registry.intersection(first,second);
        check(hit && *hit == original && registry.point(original).x == 0.123 && registry.point(original).y == 0.456,
            "Extrude registry: preseeded original corner identity and coordinates survive line intersection exactly");
        const auto duplicate = registry.addOriginal({0.123,0.456,0});
        check(duplicate != original && registry.registerIncidence(duplicate,first)
                && !registry.registerIncidence(duplicate,second) && !registry.incident(duplicate,second),
            "Extrude registry: equal coordinates keep distinct original identities and conflicting incidence rejects atomically");
    }
    {
        Registry registry({0,0,1}, {0,0,0});
        const auto a = registry.addOriginal({0,0,0});
        const auto b = registry.addOriginal({2,0,0});
        const auto c = registry.addOriginal({0,1,0});
        const auto d = registry.addOriginal({2,1,0});
        const auto e = registry.addOriginal({4,0,0});
        const auto f = registry.addOriginal({6,0,0});
        const auto first = registry.addLine(a,b);
        const auto parallel = registry.addLine(c,d);
        const auto coincident = registry.addLine(e,f);
        check(!registry.intersection(first,parallel) && !registry.intersection(first,coincident)
                && !registry.registerIncidence(c,first) && !registry.incident(c,first)
                && registry.addOriginal({9,9,0}) == 6,
            "Extrude registry: parallel/coincident lines and off-line incidence preserve node identities without invented crossings");
        check(registry.addOriginal({std::numeric_limits<double>::infinity(),0,0}) == Registry::kInvalidNode
                && registry.addOriginal({0,0,1}) == Registry::kInvalidNode
                && registry.addLine(a,a) == Registry::kInvalidLine
                && registry.addLine(a,Registry::kInvalidNode) == Registry::kInvalidLine
                && !registry.registerIncidence(Registry::kInvalidNode,first)
                && registry.addOriginal({10,10,0}) == 7,
            "Extrude registry: malformed geometry rejects without publishing partial node or incidence state");
    }
    {
        Registry registry({0,0,1}, {0,0,0});
        const auto middle = registry.addOriginal({1,0,0});
        const auto a = registry.addOriginal({0,0,0});
        const auto b = registry.addOriginal({2,0,0});
        const auto c = registry.addOriginal({1,-1,0});
        const auto d = registry.addOriginal({1,1,0});
        const auto first = registry.addLine(a,b);
        const auto second = registry.addLine(c,d);
        check(registry.registerIncidence(middle,first) && registry.registerIncidence(middle,second)
                && registry.registerIncidence(middle,second)
                && registry.intersection(first,second) == std::optional<Registry::NodeId>{middle},
            "Extrude registry: explicit original incidence pre-registers one shared multi-line node idempotently");
    }
    {
        Registry registry({0.6,0.8,0}, {10000,-20000,30000});
        const auto a = registry.addOriginal({10000,-20000,30000});
        const auto b = registry.addOriginal({9998.4,-19998.8,30000});
        const auto c = registry.addOriginal({9999.2,-19999.4,29999});
        const auto d = registry.addOriginal({9999.2,-19999.4,30001});
        const auto hit = registry.intersection(registry.addLine(a,b),registry.addLine(c,d));
        bool exactFrame = false;
        if ( hit )
        {
            const auto& point = registry.point(*hit);
            // Fixed world vector with eight coordinate roundoffs allowed for
            // independent local-frame projection and reconstruction.
            constexpr double error = 8 * std::numeric_limits<double>::epsilon();
            exactFrame = std::abs(point.x-9999.2) <= error*9999.2
                && std::abs(point.y+19999.4) <= error*19999.4 && point.z == 30000;
        }
        check(exactFrame,
            "Extrude registry: an oblique translated frame retains the independent fixed world crossing");
    }
    {
        Registry registry({0,0,1}, {0,0,0});
        const auto a = registry.addOriginal({0,0,0});
        const auto shared = registry.addOriginal({1,0,0});
        const auto b = registry.addOriginal({2,0,0});
        const auto first = registry.addLine(a,shared);
        const auto second = registry.addLine(shared,b);
        check(registry.intersection(first,second) == std::optional<Registry::NodeId>{shared},
            "Extrude registry: collinear edges retain a preseeded shared endpoint without claiming a unique infinite-line crossing");
    }
    {
        // Distinct authored segments on one exact immutable support share a
        // generated crossing with a third support. The crossing is away from
        // their authored shared endpoint, so preserving the endpoint identity
        // cannot account for this reuse.
        Registry registry({0,0,1}, {0,0,0});
        const auto a = registry.addOriginal({0,0,0});
        const auto b = registry.addOriginal({1,0,0});
        const auto c = registry.addOriginal({2,0,0});
        const auto transversalStart = registry.addOriginal({.5,-1,0});
        const auto transversalFinish = registry.addOriginal({.5,1,0});
        const auto first = registry.addLine(a,b);
        const auto second = registry.addLine(b,c);
        const auto transversal = registry.addLine(transversalStart,transversalFinish);
        const auto firstCrossing = registry.intersection(first,transversal);
        const auto secondCrossing = registry.intersection(transversal,second);
        const auto reverseCrossing = registry.intersection(second,transversal);
        bool sameCoordinates = false;
        bool fixedCoordinates = false;
        if ( firstCrossing && secondCrossing )
        {
            const auto& firstPoint = registry.point(*firstCrossing);
            const auto& secondPoint = registry.point(*secondCrossing);
            sameCoordinates = firstPoint.x == secondPoint.x
                && firstPoint.y == secondPoint.y && firstPoint.z == secondPoint.z;
            fixedCoordinates = firstPoint.x == .5 && firstPoint.y == 0 && firstPoint.z == 0;
        }
        check(first != second && registry.intersection(first,second) == std::optional<Registry::NodeId>{b}
                && firstCrossing && secondCrossing && *firstCrossing == *secondCrossing
                && reverseCrossing == firstCrossing && sameCoordinates && fixedCoordinates
                && !registry.isOriginalNode(*firstCrossing)
                && registry.classify(*firstCrossing,first) == std::optional<int>{0}
                && registry.classify(*firstCrossing,second) == std::optional<int>{0}
                && registry.classify(*firstCrossing,transversal) == std::optional<int>{0}
                && registry.incident(*firstCrossing,first)
                && registry.incident(*firstCrossing,second)
                && registry.incident(*firstCrossing,transversal),
            "Extrude registry: exact-collinear authored segments reuse one fixed generated crossing away from their shared endpoint");
    }
    {
        // Alternate authored endpoint identities can describe the same two
        // immutable supports without sharing either actual LineID. Generated
        // crossings must still canonicalize by exact support identity while
        // preserving every authored endpoint and rejecting a one-ULP support.
        Registry registry({0,0,1}, {0,0,0});
        const double adjacent = std::nextafter(1.0,2.0);
        const auto firstStart = registry.addOriginal({0,1,0});
        const auto firstFinish = registry.addOriginal({1,1,0});
        const auto firstCrossStart = registry.addOriginal({.5,0,0});
        const auto firstCrossFinish = registry.addOriginal({.5,2,0});
        const auto secondStart = registry.addOriginal({-.5,1,0});
        const auto secondFinish = registry.addOriginal({1.5,1,0});
        const auto secondCrossStart = registry.addOriginal({.5,-1,0});
        const auto secondCrossFinish = registry.addOriginal({.5,3,0});
        const auto nearStart = registry.addOriginal({0,adjacent,0});
        const auto nearFinish = registry.addOriginal({1,adjacent,0});
        const auto first = registry.addLine(firstStart,firstFinish);
        const auto firstCross = registry.addLine(firstCrossStart,firstCrossFinish);
        const auto second = registry.addLine(secondStart,secondFinish);
        const auto secondCross = registry.addLine(secondCrossStart,secondCrossFinish);
        const auto near = registry.addLine(nearStart,nearFinish);
        const auto firstNode = registry.intersection(first,firstCross);
        const auto secondNode = registry.intersection(second,secondCross);
        const auto reverseFirst = registry.intersection(firstCross,first);
        const auto reverseSecond = registry.intersection(secondCross,second);
        const auto nearNode = registry.intersection(near,firstCross);
        bool fixedCoordinates = false;
        bool sameCoordinates = false;
        bool nearCoordinates = false;
        if ( firstNode && secondNode && nearNode )
        {
            const auto& firstPoint = registry.point(*firstNode);
            const auto& secondPoint = registry.point(*secondNode);
            const auto& nearPoint = registry.point(*nearNode);
            fixedCoordinates = firstPoint.x == .5 && firstPoint.y == 1 && firstPoint.z == 0;
            sameCoordinates = firstPoint.x == secondPoint.x
                && firstPoint.y == secondPoint.y && firstPoint.z == secondPoint.z;
            nearCoordinates = nearPoint.x == .5 && nearPoint.y == adjacent && nearPoint.z == 0;
        }
        const bool authoredIdentity = firstStart != secondStart && firstFinish != secondFinish
            && firstCrossStart != secondCrossStart && firstCrossFinish != secondCrossFinish
            && registry.line(first).firstNode == firstStart
            && registry.line(first).secondNode == firstFinish
            && registry.line(second).firstNode == secondStart
            && registry.line(second).secondNode == secondFinish
            && registry.line(firstCross).firstNode == firstCrossStart
            && registry.line(secondCross).firstNode == secondCrossStart;
        const bool allIncidences = firstNode && secondNode
            && registry.incident(*firstNode,first) && registry.incident(*firstNode,firstCross)
            && registry.incident(*firstNode,second) && registry.incident(*firstNode,secondCross)
            && registry.classify(*firstNode,first) == std::optional<int>{0}
            && registry.classify(*firstNode,firstCross) == std::optional<int>{0}
            && registry.classify(*firstNode,second) == std::optional<int>{0}
            && registry.classify(*firstNode,secondCross) == std::optional<int>{0};
        const bool nearDistinct = firstNode && secondNode && nearNode
            && *nearNode != *firstNode && *nearNode != *secondNode
            && !registry.intersection(first,second)
            && registry.intersection(firstCross,secondCross) == std::nullopt;
        check(first != second && firstCross != secondCross && authoredIdentity
                && firstNode && secondNode && *firstNode == *secondNode
                && reverseFirst == firstNode && reverseSecond == secondNode
                && sameCoordinates && fixedCoordinates,
            "Extrude registry: exact alternate support IDs reuse one fixed generated crossing in both query orders");
        check(allIncidences,
            "Extrude registry: alternate support alias retains all four exact support incidences and ON classifications");
        check(nearDistinct && nearCoordinates,
            "Extrude registry: one-ULP parallel support remains a distinct crossing and does not invent a support intersection");
    }
    {
        // Source-world provenance certifies an exact three-dimensional support
        // even when the registry stores its common projected plane.
        const TVector normal{0,0,1};
        const TVector anchor{0,0,0};
        Registry registry(normal,anchor);
        const auto project = [&](const TVector& world)
        {
            return world - normal * normal.dot(world-anchor);
        };
        const auto add = [&](const TVector& world)
        {
            return registry.addOriginal(project(world),world);
        };
        const TVector firstStart{0,0,1};
        const TVector firstFinish{1,1,2};
        const TVector secondFinish{2,2,3};
        const TVector transversalStart{.5,-1,4};
        const TVector transversalFinish{.5,2,5};
        const auto a = add(firstStart);
        const auto b = add(firstFinish);
        const auto c = add(secondFinish);
        const auto crossStart = add(transversalStart);
        const auto crossFinish = add(transversalFinish);
        const auto first = registry.addLine(a,b);
        const auto second = registry.addLine(b,c);
        const auto transversal = registry.addLine(crossStart,crossFinish);
        const auto firstCrossing = registry.intersection(first,transversal);
        const auto secondCrossing = registry.intersection(transversal,second);
        bool fixedCrossing = false;
        if ( firstCrossing )
        {
            const auto& point = registry.point(*firstCrossing);
            fixedCrossing = point.x == .5 && point.y == .5 && point.z == 0;
        }
        Registry differentFrame({0,0,1},{0,0,1});
        check(firstCrossing && secondCrossing && *firstCrossing == *secondCrossing
                && fixedCrossing && registry.incident(*firstCrossing,first)
                && registry.incident(*firstCrossing,second)
                && registry.incident(*firstCrossing,transversal)
                && registry.addOriginal(firstStart,firstStart) == Registry::kInvalidNode
                && differentFrame.addOriginal(project(firstStart),firstStart) == Registry::kInvalidNode,
            "Extrude registry provenance: exact source-world collinearity reuses a projected crossing only with the registry frame");
    }
    {
        // These fixed source corners are exactly collinear in 3D (constant x
        // and z), while the shared tilted projection rounds the two authored
        // support segments to a noncollinear pair. The source certificate must
        // recover one generated identity without changing its projected point.
        const TVector normal{0.98287207540928467,5.1066521358000269e-7,
            0.18428912984868262};
        const TVector anchor{0,0,0};
        Registry registry(normal,anchor);
        const auto project = [&](const TVector& world)
        {
            return world - normal * normal.dot(world-anchor);
        };
        const auto add = [&](const TVector& world)
        {
            return registry.addOriginal(project(world),world);
        };
        constexpr double x = 1.09375;
        constexpr double z = 0.30000000999999998;
        const auto firstStart = add({x,-0.93000000999999999,z});
        const auto firstFinish = add({x,-0.87774700000000005,z});
        const auto secondStart = add({x,-0.92,z});
        const auto secondFinish = add({x,-0.85225998999999997,z});
        const auto transversalStart = add({0,-0.9,z});
        const auto transversalFinish = add({2,-0.9,z});
        const auto first = registry.addLine(firstStart,firstFinish);
        const auto second = registry.addLine(secondStart,secondFinish);
        const auto transversal = registry.addLine(transversalStart,transversalFinish);
        constexpr TVector projectedFirstStart{-0.017192860688386435,
            -0.93000058720621781,0.091697527682945701};
        constexpr TVector projectedFirstFinish{-0.01719288691514298,
            -0.87774757720623153,0.091697522765412431};
        constexpr TVector projectedSecondStart{-0.017192865707577276,
            -0.92000057720622042,0.091697526741844254};
        constexpr TVector projectedSecondFinish{-0.017192899707546783,
            -0.852260567206238,0.091697520366828683};
        const auto roundedSupportCross =
            (projectedFirstFinish-projectedFirstStart).cross(projectedSecondFinish-projectedSecondStart);
        const bool roundedSupportsDiffer = roundedSupportCross.x != 0.0
            || roundedSupportCross.y != 0.0 || roundedSupportCross.z != 0.0;
        const auto firstCrossing = registry.intersection(first,transversal);
        const auto secondCrossing = registry.intersection(transversal,second);
        const bool crossingsPublished = firstCrossing && secondCrossing;
        const bool canonicalAlias = crossingsPublished && *firstCrossing == *secondCrossing
            && registry.intersection(second,transversal) == firstCrossing;
        bool fixedCrossing = false;
        bool projectedEndpoints = false;
        bool incidences = false;
        if ( firstCrossing )
        {
            constexpr TVector expected{-0.017192875745948744,
                -0.90000057720622562,0.091697524859643331};
            const auto& actual = registry.point(*firstCrossing);
            constexpr double bound = 256.0 * std::numeric_limits<double>::epsilon();
            fixedCrossing = std::abs(actual.x-expected.x) <= bound
                && std::abs(actual.y-expected.y) <= bound
                && std::abs(actual.z-expected.z) <= bound;
            projectedEndpoints = registry.point(firstStart).x == projectedFirstStart.x
                && registry.point(firstStart).y == projectedFirstStart.y
                && registry.point(firstStart).z == projectedFirstStart.z
                && registry.point(secondStart).x == projectedSecondStart.x
                && registry.point(secondStart).y == projectedSecondStart.y
                && registry.point(secondStart).z == projectedSecondStart.z;
            incidences = registry.incident(*firstCrossing,first)
                && registry.incident(*firstCrossing,second)
                && registry.incident(*firstCrossing,transversal)
                && registry.classify(*firstCrossing,first) == std::optional<int>{0}
                && registry.classify(*firstCrossing,second) == std::optional<int>{0}
                && registry.classify(*firstCrossing,transversal) == std::optional<int>{0};
        }
        check(roundedSupportsDiffer,
            "Extrude registry provenance: tilted source-collinear fixed supports have independently noncollinear rounded projections");
        check(first != second && crossingsPublished && canonicalAlias,
            "Extrude registry provenance: tilted source-collinear supports reuse one generated NodeID in both query orders");
        check(fixedCrossing,
            "Extrude registry provenance: tilted source-collinear alias retains the fixed projected crossing coordinates");
        check(projectedEndpoints,
            "Extrude registry provenance: tilted source-world inputs retain their fixed projected endpoint coordinates");
        check(incidences,
            "Extrude registry provenance: tilted source-collinear alias publishes all support incidences and ON classifications");
    }
    {
        // Mixed source/projected certificates can share one representative
        // without making the relation transitive. Keep both generated
        // candidates, recover a unique direct match, and reject a query that
        // directly matches both candidates.
        const TVector normal{0.98287207540928467,5.1066521358000269e-7,
            0.18428912984868262};
        const TVector anchor{0,0,0};
        Registry registry(normal,anchor);
        const auto project = [&](const TVector& world)
        {
            return world - normal * normal.dot(world-anchor);
        };
        const auto addCertified = [&](const TVector& world)
        {
            return registry.addOriginal(project(world),world);
        };
        const auto addProjected = [&](const TVector& world)
        {
            return registry.addOriginal(project(world));
        };
        const auto samePointExact = [](const TVector& first, const TVector& second)
        {
            return first.x == second.x && first.y == second.y && first.z == second.z;
        };
        constexpr double x = 1.09375;
        constexpr double z = 0.30000000999999998;
        const TVector aStart{x,-0.93000000999999999,z};
        const TVector aFinish{x,-0.87774700000000005,z};
        const TVector bStart{x,-0.92,z};
        const TVector bFinish{x,-0.85225998999999997,z};
        const TVector dStart{0,-0.9,z};
        const TVector dFinish{2,-0.9,z};
        const auto a0 = addCertified(aStart);
        const auto a1 = addCertified(aFinish);
        const auto b0 = addCertified(bStart);
        const auto b1 = addCertified(bFinish);
        const auto c0 = addProjected(aStart);
        const auto c1 = addProjected(aFinish);
        const auto d0 = addProjected(dStart);
        const auto d1 = addProjected(dFinish);
        const auto lineA = registry.addLine(a0,a1);
        const auto lineB = registry.addLine(b0,b1);
        const auto lineC = registry.addLine(c0,c1);
        const auto lineD = registry.addLine(d0,d1);
        const auto firstNode = registry.intersection(lineC,lineD);
        const auto secondNode = registry.intersection(lineB,lineD);
        constexpr TVector expectedFirst{-0.017192875745948744,
            -0.90000057720622562,0.091697524859643331};
        constexpr double coordinateBound = 256.0 * std::numeric_limits<double>::epsilon();
        bool fixedFirstCoordinates = false;
        bool roundedSupportsDiffer = false;
        if ( firstNode && secondNode )
        {
            const auto& firstPoint = registry.point(*firstNode);
            const auto& secondPoint = registry.point(*secondNode);
            fixedFirstCoordinates = std::abs(firstPoint.x-expectedFirst.x) <= coordinateBound
                && std::abs(firstPoint.y-expectedFirst.y) <= coordinateBound
                && std::abs(firstPoint.z-expectedFirst.z) <= coordinateBound;
            const auto& bLine = registry.line(lineB);
            const auto& cLine = registry.line(lineC);
            const auto roundedSupportCross =
                (registry.point(bLine.secondNode)-registry.point(bLine.firstNode)).cross(
                    registry.point(cLine.secondNode)-registry.point(cLine.firstNode));
            roundedSupportsDiffer = roundedSupportCross.x != 0.0
                || roundedSupportCross.y != 0.0 || roundedSupportCross.z != 0.0;
            check(!samePointExact(firstPoint,secondPoint),
                "Extrude registry mixed provenance: rounded source supports retain distinct generated coordinates");
        }
        check(lineA != lineB && lineB != lineC && lineC != lineD
                && firstNode == std::optional<Registry::NodeId>{8}
                && secondNode == std::optional<Registry::NodeId>{9}
                && firstNode && secondNode && *firstNode != *secondNode
                && registry.intersection(lineD,lineC) == firstNode
                && registry.intersection(lineD,lineB) == secondNode
                && roundedSupportsDiffer && fixedFirstCoordinates,
            "Extrude registry mixed provenance: both generated candidates publish fixed IDs, coordinates and reverse identities");
        check(firstNode && registry.classify(*firstNode,lineC) == std::optional<int>{0}
                && registry.classify(*firstNode,lineD) == std::optional<int>{0}
                && secondNode && registry.classify(*secondNode,lineB) == std::optional<int>{0}
                && registry.classify(*secondNode,lineD) == std::optional<int>{0},
            "Extrude registry mixed provenance: each distinct candidate retains ON classification for its defining supports");

        const auto c20 = addProjected(aStart);
        const auto c21 = addProjected(aFinish);
        const auto d20 = addProjected(dStart);
        const auto d21 = addProjected(dFinish);
        const auto lineC2 = registry.addLine(c20,c21);
        const auto lineD2 = registry.addLine(d20,d21);
        const auto recovered = registry.intersection(lineC2,lineD2);
        const auto recoveredReverse = registry.intersection(lineD2,lineC2);
        const auto repeated = registry.intersection(lineC2,lineD2);
        check(c20 != c0 && c21 != c1 && lineC2 != lineC && lineD2 != lineD
                && recovered == firstNode && recoveredReverse == firstNode
                && repeated == firstNode
                && recovered && registry.classify(*recovered,lineC2) == std::optional<int>{0}
                && registry.classify(*recovered,lineD2) == std::optional<int>{0},
            "Extrude registry mixed provenance: a unique C2/D2 direct match recovers the first candidate in both and repeated query orders");

        const auto a20 = addCertified(aStart);
        const auto a21 = addCertified(aFinish);
        const auto lineA2 = registry.addLine(a20,a21);
        const auto ambiguous = registry.intersection(lineA2,lineD2);
        const auto ambiguousReverse = registry.intersection(lineD2,lineA2);
        check(a20 != a0 && a21 != a1 && lineA2 != lineA
                && !ambiguous && !ambiguousReverse,
            "Extrude registry mixed provenance: an A2/D2 query matching both candidates fails closed in both orders");
    }
    {
        // A one-ULP source-world offset is a distinct parallel support. Its
        // generated crossing must remain separate under both query orders.
        const TVector normal{0,0,1};
        const TVector anchor{0,0,0};
        Registry registry(normal,anchor);
        const auto project = [&](const TVector& world)
        {
            return world - normal * normal.dot(world-anchor);
        };
        const auto add = [&](const TVector& world)
        {
            return registry.addOriginal(project(world),world);
        };
        const double adjacent = std::nextafter(1.0,2.0);
        const auto baseStart = add({0,1,0});
        const auto baseFinish = add({2,1,0});
        const auto nearStart = add({0,adjacent,0});
        const auto nearFinish = add({2,adjacent,0});
        const auto transversalStart = add({1,-1,0});
        const auto transversalFinish = add({1,3,0});
        const auto base = registry.addLine(baseStart,baseFinish);
        const auto near = registry.addLine(nearStart,nearFinish);
        const auto transversal = registry.addLine(transversalStart,transversalFinish);
        const auto firstCrossing = registry.intersection(base,transversal);
        const auto secondCrossing = registry.intersection(transversal,near);
        check(firstCrossing && secondCrossing && *firstCrossing != *secondCrossing
                && registry.intersection(transversal,base) == firstCrossing
                && registry.intersection(near,transversal) == secondCrossing
                && registry.point(*firstCrossing).y == 1
                && registry.point(*secondCrossing).y == adjacent
                && !registry.intersection(base,near),
            "Extrude registry provenance: one-ULP genuinely distinct parallel supports never alias");
    }
    {
        // Source and projected support certificates can bridge different
        // representative classes. A fresh pair with no actual LineID in
        // common must still recover the one cached generated crossing.
        const TVector normal{0.98287207540928467,5.1066521358000269e-7,
            0.18428912984868262};
        const TVector anchor{0,0,0};
        Registry registry(normal,anchor);
        const auto project = [&](const TVector& world)
        {
            return world - normal * normal.dot(world-anchor);
        };
        const auto addCertified = [&](const TVector& world)
        {
            return registry.addOriginal(project(world),world);
        };
        const auto addProjected = [&](const TVector& world)
        {
            return registry.addOriginal(project(world));
        };
        const auto samePointExact = [](const TVector& first, const TVector& second)
        {
            return first.x == second.x && first.y == second.y && first.z == second.z;
        };
        const auto sameLineEndpoints = [&](const Registry::LineId first,
            const Registry::LineId second)
        {
            const auto& firstLine = registry.line(first);
            const auto& secondLine = registry.line(second);
            return samePointExact(registry.point(firstLine.firstNode),
                    registry.point(secondLine.firstNode))
                && samePointExact(registry.point(firstLine.secondNode),
                    registry.point(secondLine.secondNode));
        };
        constexpr double x = 1.09375;
        constexpr double z = 0.30000000999999998;
        const TVector aStart{x,-0.93000000999999999,z};
        const TVector aFinish{x,-0.87774700000000005,z};
        const TVector bStart{x,-0.92,z};
        const TVector bFinish{x,-0.85225998999999997,z};
        const TVector dStart{0,-0.9,z};
        const TVector dFinish{2,-0.9,z};

        const auto a0 = addCertified(aStart);
        const auto a1 = addCertified(aFinish);
        const auto b0 = addCertified(bStart);
        const auto b1 = addCertified(bFinish);
        const auto r0 = addProjected(bStart);
        const auto r1 = addProjected(bFinish);
        const auto x0 = addProjected(bStart);
        const auto x1 = addProjected(bFinish);
        const auto d0 = addProjected(dStart);
        const auto d1 = addProjected(dFinish);
        const auto lineA = registry.addLine(a0,a1);
        const auto lineB = registry.addLine(b0,b1);
        const auto lineR = registry.addLine(r0,r1);
        const auto lineX = registry.addLine(x0,x1);
        const auto lineD = registry.addLine(d0,d1);
        const auto firstNode = registry.intersection(lineB,lineD);
        constexpr TVector expectedFirst{-0.017192875745948841,
            -0.90000057720622562,0.091697524859643303};
        constexpr double coordinateBound = 256.0 * std::numeric_limits<double>::epsilon();
        bool fixedFirstCoordinates = false;
        if ( firstNode )
        {
            const auto& actual = registry.point(*firstNode);
            fixedFirstCoordinates = std::abs(actual.x-expectedFirst.x) <= coordinateBound
                && std::abs(actual.y-expectedFirst.y) <= coordinateBound
                && std::abs(actual.z-expectedFirst.z) <= coordinateBound;
        }
        const bool firstPublished = firstNode && *firstNode == 10;
        const bool firstOn = firstNode
            && registry.classify(*firstNode,lineB) == std::optional<int>{0}
            && registry.classify(*firstNode,lineD) == std::optional<int>{0}
            && registry.incident(*firstNode,lineB)
            && registry.incident(*firstNode,lineD);
        const bool representativeBridge = sameLineEndpoints(lineB,lineR)
            && sameLineEndpoints(lineR,lineX)
            && !sameLineEndpoints(lineA,lineR)
            && !sameLineEndpoints(lineA,lineX);

        const auto x20 = addProjected(bStart);
        const auto x21 = addProjected(bFinish);
        const auto d20 = addProjected(dStart);
        const auto d21 = addProjected(dFinish);
        const auto lineX2 = registry.addLine(x20,x21);
        const auto lineD2 = registry.addLine(d20,d21);
        const auto secondNode = registry.intersection(lineX2,lineD2);
        const auto reverseSecond = registry.intersection(lineD2,lineX2);
        const auto repeated = registry.intersection(lineX2,lineD2);
        const bool sameCrossingCoordinates = firstNode && secondNode
            && samePointExact(registry.point(*firstNode),registry.point(*secondNode));
        const bool directSupportMatch = sameLineEndpoints(lineX2,lineB)
            && sameLineEndpoints(lineD2,lineD);
        const bool distinctActualLineIDs = lineX2 != lineB && lineD2 != lineD;
        const bool secondOn = secondNode
            && registry.classify(*secondNode,lineX2) == std::optional<int>{0}
            && registry.classify(*secondNode,lineD2) == std::optional<int>{0}
            && registry.incident(*secondNode,lineX2)
            && registry.incident(*secondNode,lineD2);
        const bool originalIdentities = registry.line(lineB).firstNode == b0
            && registry.line(lineB).secondNode == b1
            && registry.line(lineD).firstNode == d0
            && registry.line(lineD).secondNode == d1
            && registry.line(lineX2).firstNode == x20
            && registry.line(lineX2).secondNode == x21
            && registry.line(lineD2).firstNode == d20
            && registry.line(lineD2).secondNode == d21;
        const bool authoredPointCoordinates = samePointExact(registry.point(b0),project(bStart))
            && samePointExact(registry.point(b1),project(bFinish))
            && samePointExact(registry.point(d0),project(dStart))
            && samePointExact(registry.point(d1),project(dFinish))
            && samePointExact(registry.point(x20),project(bStart))
            && samePointExact(registry.point(x21),project(bFinish))
            && samePointExact(registry.point(d20),project(dStart))
            && samePointExact(registry.point(d21),project(dFinish));
        check(firstPublished && fixedFirstCoordinates && firstOn && representativeBridge,
            "Extrude registry cross-bucket: source/projected bridge publishes the fixed NodeID and preserves its first incidences");
        check(firstNode && secondNode && *secondNode == *firstNode && reverseSecond == firstNode
                && repeated == firstNode && sameCrossingCoordinates
                && directSupportMatch && distinctActualLineIDs && secondOn
                && originalIdentities && authoredPointCoordinates,
            "Extrude registry cross-bucket: fresh mixed-support query reuses NodeID 10 in reverse and repeated orders without welding authored endpoints");

        const auto a20 = addCertified(aStart);
        const auto a21 = addCertified(aFinish);
        const auto d30 = addProjected(dStart);
        const auto d31 = addProjected(dFinish);
        const auto lineA2 = registry.addLine(a20,a21);
        const auto lineD3 = registry.addLine(d30,d31);
        const auto sourceProjectedNode = registry.intersection(lineA2,lineD3);
        const auto sourceProjectedReverse = registry.intersection(lineD3,lineA2);
        const auto sourceProjectedRepeated = registry.intersection(lineA2,lineD3);
        const bool sourceProjectedFreshIDs = lineA2 != lineA && lineA2 != lineB
            && lineA2 != lineR && lineA2 != lineX
            && lineD3 != lineD && lineD3 != lineD2;
        const bool sourceProjectedOn = sourceProjectedNode
            && registry.classify(*sourceProjectedNode,lineA2) == std::optional<int>{0}
            && registry.classify(*sourceProjectedNode,lineD3) == std::optional<int>{0}
            && registry.incident(*sourceProjectedNode,lineA2)
            && registry.incident(*sourceProjectedNode,lineD3);
        const bool sourceProjectedOriginalCoordinates = samePointExact(
                registry.point(a20),project(aStart))
            && samePointExact(registry.point(a21),project(aFinish))
            && samePointExact(registry.point(d30),project(dStart))
            && samePointExact(registry.point(d31),project(dFinish));
        check(firstNode && sourceProjectedNode && *sourceProjectedNode == *firstNode
                && sourceProjectedReverse == firstNode
                && sourceProjectedRepeated == firstNode
                && sourceProjectedFreshIDs && sourceProjectedOn
                && sourceProjectedOriginalCoordinates,
            "Extrude registry cross-bucket: fresh source/projected S/P query reuses NodeID 10 with reverse/repeat identity and ON incidence");
    }
    {
        Registry registry({0,0,1}, {std::numeric_limits<double>::infinity(),0,0});
        check(!registry.valid() && registry.addOriginal({0,0,0}) == Registry::kInvalidNode,
            "Extrude registry: a nonfinite frame anchor cannot publish geometry");
    }
    {
        Registry registry({0,0,1}, {0,0,0});
        const auto crossing = registry.addOriginal({1,0,0});
        const auto a = registry.addOriginal({0,0,0});
        const auto b = registry.addOriginal({2,0,0});
        const auto c = registry.addOriginal({1,-1,0});
        const auto d = registry.addOriginal({1,1,0});
        const auto horizontal = registry.addLine(a,b);
        const auto vertical = registry.addLine(c,d);
        check(registry.registerIncidence(crossing,horizontal) && registry.registerIncidence(crossing,vertical),
            "Extrude registry classify: explicit original intersection seeds the classification fixture");
        const auto conflicting = registry.addOriginal({std::nextafter(1.0,2.0),0,0});
        check(registry.registerIncidence(conflicting,horizontal)
                && !registry.classify(conflicting,vertical) && !registry.incident(conflicting,vertical),
            "Extrude registry classify: uncertain original identity conflict cannot become a fabricated positive side");
        const auto above = registry.addOriginal({1,1,0});
        const auto below = registry.addOriginal({1,-1,0});
        check(registry.classify(crossing,horizontal) == std::optional<int>{0}
                && registry.classify(above,horizontal) == std::optional<int>{-1}
                && registry.classify(below,horizontal) == std::optional<int>{1}
                && !registry.classify(Registry::kInvalidNode,horizontal)
                && !registry.classify(crossing,Registry::kInvalidLine),
            "Extrude registry classify: fixed line orientation signs and invalid request results are explicit");
    }
    {
        Registry registry({0,0,1}, {0,0,0});
        const auto a = registry.addOriginal({0,0,0});
        const auto b = registry.addOriginal({2,0,0});
        const auto c = registry.addOriginal({1,0,0});
        const auto d = registry.addOriginal({3,0,0});
        const auto first = registry.addLine(a,b);
        const auto second = registry.addLine(c,d);
        check(registry.registerIncidence(a,second) && registry.registerIncidence(b,second)
                && registry.registerIncidence(c,first) && registry.registerIncidence(d,first)
                && registry.classify(a,second) == std::optional<int>{0}
                && registry.classify(b,second) == std::optional<int>{0}
                && !registry.intersection(first,second),
            "Extrude registry classify: coincident lines retain multiple distinct ON originals without inventing one crossing identity");
    }
    // Orthogonal immutable lines provide independently exact components even
    // when solving along the other line cancels large endpoint coordinates.
    for ( int axis = 0; axis < 3; ++axis )
    {
        const auto rotate = [axis](const TVector& p)
        {
            return axis == 0 ? p : axis == 1 ? TVector{p.z,p.x,p.y} : TVector{p.y,p.z,p.x};
        };
        for ( int order = 0; order < 8; ++order )
        {
            constexpr double tiny = 4.8491766156e-12;
            Registry registry(rotate({0,0,1}), {0,0,0});
            const auto a = registry.addOriginal(rotate({.1,.25,0}));
            const auto b = registry.addOriginal(rotate({.15,.25,0}));
            const auto c = registry.addOriginal(rotate({tiny,0,0}));
            const auto d = registry.addOriginal(rotate({tiny,1,0}));
            const auto addFirst = [&]() { return registry.addLine((order & 1) ? b : a, (order & 1) ? a : b); };
            const auto addSecond = [&]() { return registry.addLine((order & 2) ? d : c, (order & 2) ? c : d); };
            Registry::LineId first, second;
            if ( order & 4 )
            {
                second = addSecond();
                first = addFirst();
            }
            else
            {
                first = addFirst();
                second = addSecond();
            }
            const auto intersection = registry.intersection(first,second);
            check(intersection.has_value(), "Extrude registry tiny-component crossing is admitted");
            if ( !intersection )
            {
                continue;
            }
            const auto expected = rotate({tiny,.25,0});
            const auto& actual = registry.point(*intersection);
            check(actual.x == expected.x && actual.y == expected.y && actual.z == expected.z,
                "Extrude registry reconstructs all fixed tiny crossing components without cancellation");
            check(registry.intersection(second,first) == intersection
                    && registry.incident(*intersection,first) && registry.incident(*intersection,second)
                    && registry.addOriginal(rotate({2,2,0})) == 5,
                "Extrude registry reversed crossing query retains one node and both immutable incidences");
        }
    }
    {
        Registry registry({0,0,1}, {0,0,0});
        const auto a = registry.addOriginal({0,0,0});
        const auto b = registry.addOriginal({1,0,0});
        const auto c = registry.addOriginal({0,1,0});
        const auto d = registry.addOriginal({1,1,0});
        const auto first = registry.addLine(a,b);
        const auto second = registry.addLine(c,d);
        check(!registry.intersection(first,second) && !registry.intersection(second,first)
                && registry.addOriginal({2,2,0}) == 4,
            "Extrude registry exactly parallel supports reject without publishing a node");
    }
    for ( const TVector normal : {TVector{0,0,0}, TVector{0,0,2}, TVector{0,0,std::numeric_limits<double>::quiet_NaN()}} )
    {
        Registry registry(normal,{0,0,0});
        check(!registry.valid() && registry.addOriginal({0,0,0}) == Registry::kInvalidNode,
            "Extrude registry: invalid frame cannot publish usable geometry identities");
    }
}

} // namespace sed::tests
