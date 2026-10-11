#pragma once

#include <optional>

#include <QVector3D>

#include "sed/editor/selection.hpp"
#include "sed/level.hpp"

namespace sed::ui::viewport
{

struct OrbitRigUpdate
{
    QVector3D target{};
    float yawDeg{};
    float pitchDeg{};
    float rollDeg{};
    float distance{};
};

/** Active viewport members used when the aggregate Selection has no pivot. */
struct SelectionOrbitFallback
{
    std::int32_t sector{ -1 };
    std::int32_t surfaceSector{ -1 };
    std::int32_t surface{ -1 };
    std::int32_t thing{ -1 };
    std::int32_t light{ -1 };
    std::int32_t frameValue{ -1 };
};

/**
 * Finds one stable world-space pivot for the complete semantic selection.
 * Sectors contribute their bounds center, Surfaces their vertex centroid,
 * selected Surface vertices their exact positions, and Thing, Frame, and
 * Light selections their authored positions. Frame parsing may allocate;
 * allocation failure propagates without modifying the Level or Selection.
 */
[[nodiscard]] std::optional<QVector3D> selectionOrbitPivot(
    const Level& level,
    const editor::Selection& selection,
    const SelectionOrbitFallback& fallback = {});

/**
 * UI action boundary: true publishes a complete pivot (possibly empty); false
 * reports allocation failure and preserves the caller's previous output.
 */
[[nodiscard]] bool trySelectionOrbitPivot(
    const Level& level,
    const editor::Selection& selection,
    const SelectionOrbitFallback& fallback,
    std::optional<QVector3D>& output);

/**
 * Rigidly revolves the camera eye and look target around a world-space pivot.
 * Re-expressing the result as the existing orbit rig avoids a camera change at
 * MMB press and keeps the pivot at the same screen position during the drag.
 */
[[nodiscard]] std::optional<OrbitRigUpdate> orbitRigAroundPivot(
    const QVector3D& currentTarget,
    float currentDistance,
    float currentYawDeg,
    float currentPitchDeg,
    float currentRollDeg,
    const QVector3D& pivot,
    float yawDeltaDeg,
    float pitchDeltaDeg) noexcept;

} // namespace sed::ui::viewport
