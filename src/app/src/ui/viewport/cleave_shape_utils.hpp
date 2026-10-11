#pragma once

#include <cstdint>
#include <optional>
#include <vector>

namespace sed::ui::viewport
{

enum class CleaveShapeKind : std::uint8_t
{
    Square,
    Rectangle,
    RegularPolygon,
    Arch,
    Star
};

struct CleaveShapeSpec
{
    CleaveShapeKind kind{ CleaveShapeKind::Square };
    // Level/world units. Square always uses width for both axes, and a regular
    // polygon uses width as its circumcircle diameter. Other shapes honor the
    // linked/unlinked height setting.
    double width{ 1.0 };
    double height{ 1.0 };
    bool dimensionsLinked{ true };
    int segments{ 16 };
    int points{ 5 };
};

struct CleaveShapePoint2D
{
    double x{};
    double y{};
};

struct CleaveShapeStroke2D
{
    CleaveShapePoint2D first{};
    CleaveShapePoint2D second{};
};

struct CleaveShapeCell2D
{
    std::vector<CleaveShapePoint2D> vertices{};
};

struct CleaveShapePlan
{
    // CCW, centered on the origin, without a repeated closing point.
    std::vector<CleaveShapePoint2D> perimeter{};
    std::vector<CleaveShapeStroke2D> decompositionStrokes{};
    std::vector<CleaveShapeCell2D> convexCells{};
    // Geometric metadata only. Add Shape always appends ordinary bounded
    // strokes and never creates an explicit closed-chain link.
    bool convexClosedPerimeter{};
};

[[nodiscard]] std::optional<CleaveShapePlan> makeCleaveShapePlan(
    const CleaveShapeSpec& spec);

} // namespace sed::ui::viewport
