#pragma once

#include "sed/editor/selection.hpp"
#include "sed/level.hpp"
#include "sed/level/build.hpp"
#include <QString>

#include <QVector3D>

#include <optional>
#include <vector>

namespace sed::ui::surface
{

/** Describes how the renderer should treat the selected source face while Surface Extrude is previewed. */
struct SurfaceExtrudePreviewRenderPlan
{
    std::optional<SurfaceRef> hiddenSourceSurface{};
    std::optional<Sector> sourceCutPreview{};
};

/** Result of validating the current Surface Extrude settings before live preview starts. */
struct SurfaceExtrudePreviewSetup
{
    std::optional<Sector> previewSector{};
    double expand{};
    bool resetCollapsedNegativeExpand{};
    level::SurfaceExtrudeReport report{};
};

/** Returns an actionable error with the available source target, or empty on success. */
QString surfaceExtrudeFailureMessage(const level::SurfaceExtrudeReport& report);

/** Dynamic Expand spinbox settings derived from the selected source surface. */
struct SurfaceExtrudeExpandSpinConfig
{
    double minimum{};
    double singleStep{};
    bool surfaceLimited{};
};

/**
 * Chooses whether the live preview should hide the committed source face and,
 * for negative expand, whether a temporary cut-hole replacement can be drawn.
 */
SurfaceExtrudePreviewRenderPlan buildSurfaceExtrudePreviewRenderPlan(const Level& level, SurfaceRef surface,
                                                                     double expand) noexcept;

/**
 * Builds initial preview geometry and resets only collapsed negative expand
 * values to zero, matching the editor's existing forgiving UX.
 */
SurfaceExtrudePreviewSetup prepareSurfaceExtrudePreview(const Level& level, SurfaceRef surface, double length,
                                                        double expand) noexcept;

/** Builds wire line vertices for the live Surface Extrude volume preview. */
std::vector<TVector> buildSurfaceExtrudePreviewLines(const Level& level, SurfaceRef surface, double length,
                                                     double expand);

/** Returns the selected surface only when no other entity type is selected. */
std::optional<SurfaceRef> singleSurfaceOnlySelection(const editor::Selection& selection) noexcept;

/**
 * Computes a source-aware minimum and step for the Expand spinbox. The minimum
 * stays just above the geometry collapse limit so stepping down cannot enter an
 * invalid negative expand value.
 */
SurfaceExtrudeExpandSpinConfig surfaceExtrudeExpandSpinConfig(const Level& level, SurfaceRef surface,
                                                              double fallbackMinimum,
                                                              double fallbackStep) noexcept;

/**
 * Quantizes an Extrude Length/Expand displacement to the active grid step.
 * Bounds are honored on the lattice itself: if the nearest cell is outside
 * the legal range, the closest in-range cell is selected.
 */
double quantizeSurfaceExtrudeScalarToGrid(double value, double gridSpacing, double minimum,
                                          double maximum) noexcept;

} // namespace sed::ui::surface
