#include "test_common.hpp"

#include "render/cleave/cleave_measurement_utils.hpp"

#include <algorithm>
#include <array>
#include <limits>
#include <span>
#include <vector>

namespace sed::app_tests
{
namespace
{

struct MeasurementLine
{
    TVector first{};
    TVector second{};
};

std::vector<MeasurementLine> rectangle(
    const TVector& origin,
    const TVector& axisA,
    const TVector& axisB,
    const double width,
    const double height)
{
    const std::array<TVector, 4> points{{origin, origin + axisA * width,
        origin + axisA * width + axisB * height, origin + axisB * height}};
    std::vector<MeasurementLine> lines{};
    for ( std::size_t index = 0; index < points.size(); ++index )
    {
        lines.push_back({points[index], points[(index + 1u) % points.size()]});
    }
    return lines;
}

} // namespace

void runCleaveMeasurementUtilsTests()
{
    constexpr double radians = 3.14159265358979323846 / 180.0;
    const TVector axisA{std::cos(24.0 * radians), std::sin(24.0 * radians), 0.0};
    const TVector axisB{-std::sin(24.0 * radians) * std::cos(41.0 * radians),
        std::cos(24.0 * radians) * std::cos(41.0 * radians), std::sin(41.0 * radians)};
    for ( const double origin : {0.0, 1000000.0} )
    for ( const double scale : {1.0, 0.00001} )
    {
        auto lines = rectangle({origin, origin, origin}, axisA, axisB, 1.6 * scale, scale);
        const auto measurements = render::makeCleaveJointAngleMeasurements(
            std::span<const MeasurementLine>(lines));
        check(measurements.size() == 4,
            "unlinked Rectangle perimeter produces exactly four unique joined-corner angles");
        for ( const auto& measurement : measurements )
        {
            check(close(measurement.degrees, 90.0, origin == 0.0 ? 1.0e-9 : 0.001)
                    && measurement.firstLine != measurement.secondLine
                    && measurement.firstLine < lines.size()
                    && measurement.secondLine < lines.size(),
                "Rectangle corners measure 90 degrees in their rotated construction plane at ordinary and small translated scales");
        }
        std::reverse(lines.begin(), lines.end());
        const auto reordered = render::makeCleaveJointAngleMeasurements(
            std::span<const MeasurementLine>(lines));
        check(reordered.size() == measurements.size()
                && std::all_of(reordered.begin(), reordered.end(), [origin](const auto& measurement)
                {
                    return close(measurement.degrees, 90.0, origin == 0.0 ? 1.0e-9 : 0.001);
                }),
            "joined-corner measurements depend on geometry rather than vector or Apply execution order");
    }
    {
        auto lines = rectangle({}, {1.0, 0.0, 0.0}, {0.0, 1.0, 0.0}, 2.0, 1.0);
        lines[1].second.x = 3.0;
        lines[2].first.x = 3.0;
        const auto measurements = render::makeCleaveJointAngleMeasurements(
            std::span<const MeasurementLine>(lines));
        const auto countAngle = [&measurements](const double angle)
        {
            return std::count_if(measurements.begin(), measurements.end(), [angle](const auto& measurement)
            {
                return close(measurement.degrees, angle, 1.0e-9);
            });
        };
        check(measurements.size() == 4 && countAngle(90.0) == 2
                && countAngle(135.0) == 1 && countAngle(45.0) == 1,
            "an authored endpoint deformation reports its actual corner angles instead of assuming every Rectangle stays square");
    }
    {
        std::vector<MeasurementLine> lines{{{-1.0, 0.0, 0.0}, {}},
            {{}, {0.0, 1.0, 0.0}}, {{}, {1.0, 0.0, 0.0}}};
        check(render::makeCleaveJointAngleMeasurements(
                std::span<const MeasurementLine>(lines)).empty(),
            "a degree-three branch has no invented unique corner measurement");
        lines.pop_back();
        lines.push_back({{}, {}});
        lines.push_back({{std::numeric_limits<double>::quiet_NaN(), 0.0, 0.0}, {}});
        const auto finite = render::makeCleaveJointAngleMeasurements(
            std::span<const MeasurementLine>(lines));
        check(finite.size() == 1 && close(finite.front().degrees, 90.0, 1.0e-9),
            "degenerate and nonfinite lines cannot create or conceal a valid unique joint");
        lines[1].first.x = std::nextafter(0.0, 1.0);
        check(render::makeCleaveJointAngleMeasurements(
                std::span<const MeasurementLine>(lines)).empty(),
            "nearby independent endpoints remain independent rather than being merged by angle presentation");
    }

    struct ScratchEndpoint { TVector point{}, other{}; std::size_t line{}; };
    constexpr std::size_t perLine = 2u * sizeof(ScratchEndpoint)
        + sizeof(render::CleaveJointAngle) + sizeof(std::uint8_t);
    check(render::cleaveJointGuideRebuildFitsBudget(0u, 0u, 0u)
            && render::cleaveJointGuideRebuildFitsBudget(1u, 0u, perLine)
            && !render::cleaveJointGuideRebuildFitsBudget(1u, 0u, perLine - 1u),
        "joint-guide presentation admission includes endpoint scratch, output joints and line masks at the exact budget boundary");
    check(!render::cleaveJointGuideRebuildFitsBudget(1u, 20u, perLine)
            && render::cleaveJointGuideRebuildFitsBudget(64u, 20u, 64u * perLine + 20u)
            && !render::cleaveJointGuideRebuildFitsBudget(0u, 21u, 20u),
        "joint-guide presentation admission charges old retained capacity without touching authored history admission");
    check(!render::cleaveJointGuideRebuildFitsBudget(std::numeric_limits<std::size_t>::max(),
                0u, std::numeric_limits<std::size_t>::max()),
        "overflowing joint-guide line counts are rejected before multiplication or allocation");

    const QRectF viewport(0.0, 0.0, 640.0, 480.0);
    const QSizeF textSize(116.0, 19.0);
    const auto centered = render::cleaveTransformHudBadgeRect(viewport, {320.0, 240.0}, textSize);
    check(centered && centered->center().x() == 320.0 && centered->top() >= 264.0
            && centered->width() >= textSize.width() + 24.0
            && centered->height() >= textSize.height() + 12.0,
        "transform HUD has font-sized padding and sits just below the transformed selection center");
    for ( const QPointF anchor : {QPointF(1.0, 1.0), QPointF(639.0, 1.0),
        QPointF(1.0, 479.0), QPointF(639.0, 479.0), QPointF(-10000.0, 10000.0)} )
    {
        const auto badge = render::cleaveTransformHudBadgeRect(viewport, anchor, textSize);
        check(badge && viewport.adjusted(8.0, 8.0, -8.0, -8.0).contains(*badge),
            "the whole transform HUD badge stays inside the viewport for every edge and offscreen selection center");
    }
    const auto narrow = render::cleaveTransformHudBadgeRect(
        QRectF(0.0, 0.0, 100.0, 50.0), {95.0, 45.0}, textSize);
    check(narrow && QRectF(8.0, 8.0, 84.0, 34.0).contains(*narrow),
        "a narrow viewport clamps the complete badge instead of clipping its right edge");
    check(!render::cleaveTransformHudBadgeRect(viewport,
                {std::numeric_limits<double>::quiet_NaN(), 0.0}, textSize)
            && !render::cleaveTransformHudBadgeRect(viewport, {},
                {std::numeric_limits<double>::infinity(), 19.0})
            && !render::cleaveTransformHudBadgeRect(QRectF(), {}, textSize),
        "HUD layout rejects invalid viewport, projected center, and font measurements before drawing");
}

} // namespace sed::app_tests
