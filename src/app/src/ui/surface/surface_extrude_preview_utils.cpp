#include "ui/surface/surface_extrude_preview_utils.hpp"

#include "sed/level/build.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace sed::ui::surface
{
namespace
{

// Expand's sign is exact: a finite negative value, however small, still
// requests an inset opening and must remain visible in preview. The core
// exposes the actual collapse boundary, so the spinbox uses the next
// representable value above it instead of imposing an absolute UI epsilon.
constexpr double kSurfaceExtrudePreferredMinimumStep = 0.001;
constexpr double kSurfaceExtrudeStepSpanDivisor = 64.0;

double safeSpinMinimumAboveCollapse(const double collapseLimit) noexcept
{
    if ( !std::isfinite(collapseLimit) )
    {
        return collapseLimit;
    }
    return std::nextafter(collapseLimit, std::numeric_limits<double>::infinity());
}

void appendLoopLines(std::vector<TVector>& out, const std::vector<TVector>& loop)
{
    if ( loop.size() < 2 )
    {
        return;
    }

    for ( std::size_t i = 0; i < loop.size(); ++i )
    {
        out.push_back(loop[i]);
        out.push_back(loop[(i + 1) % loop.size()]);
    }
}

void appendConnectorLines(std::vector<TVector>& out, const level::SurfaceExtrudePreview& preview)
{
    const std::size_t count = std::min(preview.innerLoop.size(), preview.outerLoop.size());
    for ( std::size_t i = 0; i < count; ++i )
    {
        // The inner face is stored reversed for correct adjoin winding, so
        // connector i maps back from the end of that loop to outer loop i.
        out.push_back(preview.innerLoop[count - 1 - i]);
        out.push_back(preview.outerLoop[i]);
    }
}

} // namespace

QString surfaceExtrudeFailureMessage(const level::SurfaceExtrudeReport& report)
{
    using Error = level::SurfaceExtrudeError;
    if ( report.error == Error::None )
    {
        return {};
    }
    QString prefix = QStringLiteral("Cannot extrude");
    if ( report.surface.sector >= 0 )
    {
        prefix += QStringLiteral(" Sector %1").arg(report.surface.sector);
        if ( report.surface.surface >= 0 )
        {
            prefix += QStringLiteral(" / Surface %1").arg(report.surface.surface);
        }
    }
    prefix += QStringLiteral(": ");
    switch ( report.error )
    {
    case Error::InvalidSurface:
        return prefix + QStringLiteral("the selected surface does not exist. Select an existing face.");
    case Error::InvalidLength:
        return prefix + QStringLiteral("length must be a finite number greater than zero.");
    case Error::InvalidExpand:
        return prefix + QStringLiteral("expand must be a finite number.");
    case Error::AlreadyAdjoined:
        return prefix + QStringLiteral("the source surface is already adjoined. Choose an unadjoined face.");
    case Error::InvalidSourceTopology:
        return prefix + QStringLiteral("the source surface has invalid topology or texture coordinates. "
                                       "Repair it before extruding.");
    case Error::NonPlanarSource:
        return prefix + QStringLiteral(
                            "the source surface is not planar. Flatten the surface or choose another face.");
    case Error::NonConvexSource:
        return prefix + QStringLiteral("the source surface is not convex. Split it into convex faces first.");
    case Error::CollapsedExpand:
        return prefix + QStringLiteral("expand would collapse the opening. Increase Expand toward zero.");
    case Error::InvalidResult:
        return prefix +
               QStringLiteral(
                   "the requested extrusion would create invalid geometry. Change Length or Expand.");
    case Error::InvalidAdjoin:
        return prefix +
               QStringLiteral(
                   "the generated opening cannot form a valid adjoin. Change Expand or choose another face.");
    case Error::AllocationFailure:
        return prefix +
               QStringLiteral("there is not enough memory to prepare the extrusion. Free memory and retry.");
    case Error::None:
        return {};
    }
    return prefix + QStringLiteral("the operation was rejected.");
}

SurfaceExtrudePreviewRenderPlan buildSurfaceExtrudePreviewRenderPlan(const Level& level,
                                                                     const SurfaceRef surface,
                                                                     const double expand) noexcept
{
    SurfaceExtrudePreviewRenderPlan plan{};

    // Step 1: zero and positive expand fully cover the source face, so the
    // renderer can simply hide the committed surface under the preview.
    if ( expand >= 0.0 )
    {
        plan.hiddenSourceSurface = surface;
        return plan;
    }

    // Step 2: negative expand needs an inset opening. Hide the source only when
    // core can provide a faithful temporary replacement surface with a hole.
    plan.sourceCutPreview = level::levelBuildSurfaceExtrudeSourceCutPreview(level, surface, expand);
    if ( plan.sourceCutPreview )
    {
        plan.hiddenSourceSurface = surface;
    }

    return plan;
}

SurfaceExtrudePreviewSetup prepareSurfaceExtrudePreview(const Level& level, const SurfaceRef surface,
                                                        const double length, const double expand) noexcept
{
    SurfaceExtrudePreviewSetup setup{};
    setup.expand = expand;
    setup.previewSector =
        level::levelBuildSurfaceExtrudeSectorPreview(level, surface, length, setup.expand, &setup.report);

    // Step 1: a negative expand can collapse smaller surfaces. Instead of
    // failing the command outright, reset only that case to a plain extrusion.
    if ( !setup.previewSector && setup.expand < 0.0 &&
         setup.report.error == level::SurfaceExtrudeError::CollapsedExpand )
    {
        setup.expand = 0.0;
        setup.resetCollapsedNegativeExpand = true;
        setup.previewSector =
            level::levelBuildSurfaceExtrudeSectorPreview(level, surface, length, setup.expand, &setup.report);
    }

    return setup;
}

std::vector<TVector> buildSurfaceExtrudePreviewLines(const Level& level, const SurfaceRef surface,
                                                     const double length, const double expand)
{
    const std::optional<level::SurfaceExtrudePreview> preview =
        level::levelBuildSurfaceExtrudePreview(level, surface, length, expand);
    if ( !preview )
    {
        return {};
    }

    std::vector<TVector> lines{};
    lines.reserve((preview->innerLoop.size() + preview->outerLoop.size()) * 2 +
                  preview->outerLoop.size() * 2);

    // Step 1: draw both cap loops so the user can read the extrusion length.
    appendLoopLines(lines, preview->innerLoop);
    appendLoopLines(lines, preview->outerLoop);

    // Step 2: draw side connectors so the preview reads as a sector volume.
    appendConnectorLines(lines, *preview);

    return lines;
}

std::optional<SurfaceRef> singleSurfaceOnlySelection(const editor::Selection& selection) noexcept
{
    if ( selection.surfaces.size() != 1 || !selection.sectors.empty() || !selection.surfaceVertices.empty() ||
         !selection.things.empty() || !selection.frames.empty() || !selection.lights.empty() ||
         !selection.cogs.empty() )
    {
        return std::nullopt;
    }

    return *selection.surfaces.begin();
}

SurfaceExtrudeExpandSpinConfig surfaceExtrudeExpandSpinConfig(const Level& level, const SurfaceRef surface,
                                                              const double fallbackMinimum,
                                                              const double fallbackStep) noexcept
{
    SurfaceExtrudeExpandSpinConfig config{
        .minimum = fallbackMinimum,
        .singleStep = fallbackStep,
        .surfaceLimited = false,
    };

    const std::optional<level::SurfaceExtrudeExpandBounds> bounds =
        level::levelSurfaceExtrudeExpandBounds(level, surface);
    if ( !bounds || !std::isfinite(fallbackMinimum) || !std::isfinite(fallbackStep) || fallbackStep <= 0.0 )
    {
        return config;
    }

    const double maxStep = std::max(kSurfaceExtrudePreferredMinimumStep, fallbackStep);
    config.minimum = std::max(fallbackMinimum, safeSpinMinimumAboveCollapse(bounds->negativeCollapseLimit));
    config.singleStep = std::clamp(bounds->minSpan / kSurfaceExtrudeStepSpanDivisor,
                                   kSurfaceExtrudePreferredMinimumStep, maxStep);
    config.surfaceLimited = true;
    return config;
}

double quantizeSurfaceExtrudeScalarToGrid(const double value, const double gridSpacing, const double minimum,
                                          const double maximum) noexcept
{
    if ( !std::isfinite(value) || !std::isfinite(minimum) || !std::isfinite(maximum) || minimum > maximum )
    {
        return value;
    }

    const double bounded = std::clamp(value, minimum, maximum);
    if ( !std::isfinite(gridSpacing) || gridSpacing <= 0.0 )
    {
        return bounded;
    }

    const double index = bounded / gridSpacing;
    const double firstIndex = std::ceil(minimum / gridSpacing);
    const double lastIndex = std::floor(maximum / gridSpacing);
    if ( !std::isfinite(index) || !std::isfinite(firstIndex) || !std::isfinite(lastIndex) ||
         firstIndex > lastIndex )
    {
        // No grid cell lies inside the requested interval. This is possible
        // with an intentionally huge grid; keep the precise bounded value.
        return bounded;
    }

    const double snappedIndex = std::clamp(std::round(index), firstIndex, lastIndex);
    const double snapped = snappedIndex * gridSpacing;
    return std::isfinite(snapped) ? std::clamp(snapped, minimum, maximum) : bounded;
}

} // namespace sed::ui::surface
