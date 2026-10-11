#include "main_window/common.hpp"
#include "render/cleave/cleave_geometry_utils.hpp"
#include "render/cleave/cleave_measurement_utils.hpp"
#include "sed/assets/material_dimensions.hpp"
#include "sed/level/uv.hpp"
#include "ui/project/project_startup_utils.hpp"
#include "ui/sector/sector_shape_utils.hpp"
#include "ui/sector/sector_wire_utils.hpp"
#include <limits>

namespace sed
{
namespace
{
bool validSectorDrawDimension(const double minimum, const double maximum) noexcept
{
    const double extent = maximum - minimum;
    if (!std::isfinite(minimum) || !std::isfinite(maximum) || !std::isfinite(extent) || !(extent > 0.0))
        return false;
    // Translation quantizes endpoints before their difference is measured.
    // Admit only this arithmetic uncertainty at the tool's dimension limits.
    const double error = 64.0 * std::numeric_limits<double>::epsilon()
        * std::max({std::abs(minimum), std::abs(maximum), std::abs(extent)});
    return extent >= 0.005 - error && extent <= 10000.0 + error;
}

bool validSectorDrawBounds(const TVector& minimum, const TVector& maximum) noexcept
{
    return validSectorDrawDimension(minimum.x, maximum.x)
        && validSectorDrawDimension(minimum.y, maximum.y)
        && validSectorDrawDimension(minimum.z, maximum.z);
}
} // namespace

#ifdef SED_APP_MAIN_WINDOW_TEST_ACCESS
std::optional<Sector> MainWindow::sectorDrawFrameForTest(
    const TVector& origin, const TVector& axisA, const TVector& axisB,
    const TVector& axisHeight, const TVector& dimensions, const bool viewOriented) const
{
    SectorDrawBox box{};
    box.min = viewOriented ? TVector{} : origin;
    box.max = box.min + dimensions;
    box.origin = origin;
    box.axisA = axisA;
    box.axisB = axisB;
    box.axisHeight = axisHeight;
    box.viewOriented = viewOriented;
    box.valid = true;
    Level preview{};
    preview.clear();
    const auto index = createSectorFromDrawBox(preview, box);
    if (index < 0 || static_cast<std::size_t>(index) >= preview.sectors.size()) return std::nullopt;
    return preview.sectors[static_cast<std::size_t>(index)];
}

bool MainWindow::beginSectorDrawForTest(
    const int planeAxis,
    const bool atView,
    const bool freeView,
    const QPoint& widgetPos)
{
    sectorDrawPlaneAxis_ = static_cast<SectorDrawPlaneAxis>(
        std::clamp(planeAxis,
            static_cast<int>(SectorDrawPlaneAxis::x),
            static_cast<int>(SectorDrawPlaneAxis::z)));
    sectorDrawAtCameraView_ = atView;
    sectorDrawFreeView_ = freeView;
    setPlacementMode(PlacementMode::cube);
    const QVector3D position = glView_ ? glView_->cameraTarget() : QVector3D{};
    return beginSectorDrawPlacement(
        position, false, widgetPos, Qt::NoModifier);
}

bool MainWindow::finishSectorDrawFootprintForTest(
    const QPoint& widgetPos,
    const Qt::KeyboardModifiers modifiers,
    const bool dragged)
{
    return finishSectorDrawFootprint(widgetPos, modifiers, dragged);
}

bool MainWindow::commitSectorDrawPlacementForTest(
    const QPoint& widgetPos,
    const Qt::KeyboardModifiers modifiers)
{
    updateSectorDrawPreview(widgetPos, modifiers);
    return commitSectorDrawPlacement();
}

std::optional<Sector> MainWindow::sectorDrawPreviewSectorForTest(
    const QPoint& widgetPos,
    const Qt::KeyboardModifiers modifiers) const
{
    const SectorDrawBox box = sectorDrawBoxForMouse(widgetPos, modifiers);
    Level preview{};
    preview.clear();
    preview.kind = document_.level().kind;
    preview.ppunit = document_.level().ppunit;
    const std::int32_t index = createSectorFromDrawBox(preview, box);
    if (index < 0 || static_cast<std::size_t>(index) >= preview.sectors.size())
    {
        return std::nullopt;
    }
    return preview.sectors[static_cast<std::size_t>(index)];
}

void MainWindow::cancelSectorDrawForTest()
{
    cancelSectorDrawPlacement();
    setPlacementMode(PlacementMode::none);
}
#endif

TVector MainWindow::sectorDrawPlaneNormal() const noexcept
{
    if (sectorDrawFreeView_ && glView_)
    {
        return render::cleaveDocumentPoint(glView_->cameraForward());
    }
    switch (sectorDrawPlaneAxis_)
    {
        case SectorDrawPlaneAxis::x:
            return TVector{1.0f, 0.0f, 0.0f};
        case SectorDrawPlaneAxis::y:
            return TVector{0.0f, 1.0f, 0.0f};
        case SectorDrawPlaneAxis::z:
        default:
            return TVector{0.0f, 0.0f, 1.0f};
    }
}

TVector MainWindow::sectorDrawPlacementPlanePoint() const noexcept
{
    return ((sectorDrawAtCameraView_ || sectorDrawFreeView_) && glView_)
        ? render::cleaveDocumentPoint(glView_->cameraTarget())
        : TVector{0.0f, 0.0f, 0.0f};
}

std::array<int, 3> MainWindow::sectorDrawWorldAxes() const noexcept
{
    switch (sectorDrawPlaneAxis_)
    {
        case SectorDrawPlaneAxis::x:
            return { 1, 2, 0 };
        case SectorDrawPlaneAxis::y:
            return { 0, 2, 1 };
        case SectorDrawPlaneAxis::z:
        default:
            return { 0, 1, 2 };
    }
}

bool MainWindow::selectedSectorShapeSupportsSides() const
{
    const std::optional<sed::level::SectorPrimitive> primitive =
        ui::sector::primitiveForName(selectedSectorShapeName_.empty() ? std::string_view("Cube") : std::string_view(selectedSectorShapeName_));
    return primitive
        && (*primitive == sed::level::SectorPrimitive::Cone
            || *primitive == sed::level::SectorPrimitive::Cylinder
            || *primitive == sed::level::SectorPrimitive::UvSphere);
}

int MainWindow::selectedSectorShapeSides() const noexcept
{
    const std::string_view name = selectedSectorShapeName_.empty() ? std::string_view("Cube") : std::string_view(selectedSectorShapeName_);
    if (sed::text::iequalsAscii(name, "Cone"))
    {
        return addSectorConeSides_;
    }
    if (sed::text::iequalsAscii(name, "Cylinder"))
    {
        return addSectorCylinderSides_;
    }
    if (sed::text::iequalsAscii(name, "UV Sphere"))
    {
        return addSectorUvSphereSegments_;
    }
    return 0;
}

void MainWindow::setSelectedSectorShapeSides(const int sides) noexcept
{
    const std::string_view name = selectedSectorShapeName_.empty() ? std::string_view("Cube") : std::string_view(selectedSectorShapeName_);
    if (sed::text::iequalsAscii(name, "Cone"))
    {
        addSectorConeSides_ = std::clamp(sides, 3, 128);
    }
    else if (sed::text::iequalsAscii(name, "Cylinder"))
    {
        addSectorCylinderSides_ = std::clamp(sides, 3, 128);
    }
    else if (sed::text::iequalsAscii(name, "UV Sphere"))
    {
        addSectorUvSphereSegments_ = std::clamp(sides, 4, 96);
    }
}

QString MainWindow::selectedSectorSidesLabel() const
{
    const std::string_view name = selectedSectorShapeName_.empty() ? std::string_view("Cube") : std::string_view(selectedSectorShapeName_);
    if (sed::text::iequalsAscii(name, "UV Sphere"))
    {
        return QStringLiteral("Segments");
    }
    return QStringLiteral("Sides");
}

void MainWindow::cancelSectorDrawPlacement()
{
    sectorDrawStage_ = SectorDrawStage::none;
    sectorDrawAnchor_ = {};
    sectorDrawFootprint_ = {};
    sectorDrawAnchorWidgetPos_ = {};
    sectorDrawHeightWidgetPos_ = {};
    sectorDrawLastBox_ = {};
    sectorDrawGridFrame_ = {};

    if (glView_)
    {
        glView_->clearPlacementPreviewThing();
        glView_->clearPlacementPreviewLines();
    }
}

bool MainWindow::sectorDrawGridSnapActive() const noexcept
{
    return viewportTransformGridSnapAction_
        && viewportTransformGridSnapAction_->isChecked();
}

TVector MainWindow::snapSectorDrawPointToGrid(
    const TVector& point,
    const TVector& planePoint,
    const TVector& planeNormal) const noexcept
{
    if (!sectorDrawGridSnapActive() || !sectorDrawGridFrame_.valid)
    {
        return point;
    }

    return render::snapPointToGridOnPlane(
        point, planePoint, planeNormal, sectorDrawGridFrame_);
}

MainWindow::SectorDrawBox MainWindow::sectorDrawBoxForMouse(
    const QPoint& widgetPos,
    const Qt::KeyboardModifiers modifiers) const
{
    if (!glView_ || sectorDrawStage_ == SectorDrawStage::none)
    {
        return {};
    }

    constexpr double kMinDim = 0.005;
    constexpr double kMaxDim = 10000.0;

    const TVector anchor = sectorDrawAnchor_;

    auto finiteOrZero = [](const double value) noexcept -> double
    {
        return std::isfinite(value) ? value : 0.0;
    };

    auto signOrPositive = [](const double value) noexcept -> double
    {
        return (value < 0.0) ? -1.0 : 1.0;
    };

    auto clampMagnitude = [&](const double value) noexcept -> double
    {
        const double sign = signOrPositive(value);
        const double mag = std::clamp(std::abs(finiteOrZero(value)), kMinDim, kMaxDim);
        return sign * mag;
    };

    const bool hardGridSnap = sectorDrawGridSnapActive()
        && sectorDrawGridFrame_.valid
        && std::isfinite(sectorDrawGridFrame_.spacing)
        && sectorDrawGridFrame_.spacing > 0.0;

    const std::array<int, 3> axes = sectorDrawWorldAxes();
    auto readTVectorAxis = [](const TVector& value, const int axis) noexcept -> double
    {
        switch (axis)
        {
            case 0: return value.x;
            case 1: return value.y;
            default: return value.z;
        }
    };
    auto writeTVectorAxis = [](TVector& value, const int axis, const double v) noexcept
    {
        switch (axis)
        {
            case 0:
                value.x = v;
                break;
            case 1:
                value.y = v;
                break;
            default:
                value.z = v;
                break;
        }
    };

    TVector footprint = sectorDrawFootprint_;
    if (sectorDrawStage_ == SectorDrawStage::footprint)
    {
        TVector cursor = anchor;
        const TVector interactionNormal = sectorDrawFreeView_
            ? sectorDrawFreeAxisHeight_
            : (sectorDrawAtCameraView_
                ? render::cleaveDocumentPoint(glView_->cameraForward())
                : sectorDrawPlaneNormal());
        if (const std::optional<TVector> planeHit =
                glView_->viewportPointOnPlane(widgetPos, anchor, interactionNormal))
        {
            cursor = *planeHit;
            if (sectorDrawFreeView_)
            {
                cursor = snapSectorDrawPointToGrid(
                    cursor, anchor, interactionNormal);
            }
        }

        double dx{};
        double dy{};
        if (sectorDrawFreeView_)
        {
            const TVector delta = cursor - anchor;
            dx = finiteOrZero(render::cleaveDot(delta, sectorDrawFreeAxisA_));
            dy = finiteOrZero(render::cleaveDot(delta, sectorDrawFreeAxisB_));
        }
        else
        {
            dx = finiteOrZero(readTVectorAxis(cursor, axes[0]) - readTVectorAxis(sectorDrawAnchor_, axes[0]));
            dy = finiteOrZero(readTVectorAxis(cursor, axes[1]) - readTVectorAxis(sectorDrawAnchor_, axes[1]));

            // At View uses a camera-facing interaction plane so a selected
            // global construction plane can never become edge-on and stop
            // receiving pointer hits. Retain global-axis output; if one of
            // those axes points almost directly into the view, use the
            // corresponding screen delta as a stable size control.
            if (sectorDrawAtCameraView_)
            {
                double unitsPerPixel = glView_->worldUnitsPerPixelAtWorld(anchor);
                if (!std::isfinite(unitsPerPixel) || unitsPerPixel <= 0.0f)
                {
                    unitsPerPixel = 0.01f;
                }
                const QPoint screenDelta = widgetPos - sectorDrawAnchorWidgetPos_;
                if (std::abs(dx) < kMinDim && std::abs(screenDelta.x()) > 2)
                {
                    dx = static_cast<double>(screenDelta.x()) * unitsPerPixel;
                }
                if (std::abs(dy) < kMinDim && std::abs(screenDelta.y()) > 2)
                {
                    dy = static_cast<double>(-screenDelta.y()) * unitsPerPixel;
                }
            }

            TVector outputPoint = anchor;
            writeTVectorAxis(
                outputPoint,
                axes[0],
                readTVectorAxis(sectorDrawAnchor_, axes[0]) + dx);
            writeTVectorAxis(
                outputPoint,
                axes[1],
                readTVectorAxis(sectorDrawAnchor_, axes[1]) + dy);
            outputPoint = snapSectorDrawPointToGrid(
                outputPoint, anchor, sectorDrawPlaneNormal());
            dx = finiteOrZero(
                readTVectorAxis(outputPoint, axes[0])
                - readTVectorAxis(sectorDrawAnchor_, axes[0]));
            dy = finiteOrZero(
                readTVectorAxis(outputPoint, axes[1])
                - readTVectorAxis(sectorDrawAnchor_, axes[1]));
        }

        if (hardGridSnap)
        {
            // The snapped endpoint is authoritative. In particular, do not
            // replace a zero-cell result with the generic 0.005 minimum: that
            // would manufacture an off-lattice corner. A footprint remains
            // invalid until the pointer reaches a different cell on both
            // construction axes. Grid fidelity also takes precedence over
            // Shift's exact-square constraint on a rotated/projected grid.
            dx = finiteOrZero(dx);
            dy = finiteOrZero(dy);
        }
        else if (modifiers.testFlag(Qt::ShiftModifier))
        {
            const double side = std::clamp(std::max({ std::abs(dx), std::abs(dy), kMinDim }), kMinDim, kMaxDim);
            dx = signOrPositive(dx) * side;
            dy = signOrPositive(dy) * side;
        }
        else
        {
            dx = clampMagnitude(dx);
            dy = clampMagnitude(dy);
        }

        if (sectorDrawFreeView_)
        {
            footprint = TVector{dx, dy, 0.0};
        }
        else
        {
            footprint = sectorDrawAnchor_;
            writeTVectorAxis(footprint, axes[0], readTVectorAxis(sectorDrawAnchor_, axes[0]) + dx);
            writeTVectorAxis(footprint, axes[1], readTVectorAxis(sectorDrawAnchor_, axes[1]) + dy);
        }
    }

    double height = kMinDim;
    if (sectorDrawStage_ == SectorDrawStage::height)
    {
        const double width = sectorDrawFreeView_
            ? std::abs(footprint.x)
            : std::abs(readTVectorAxis(footprint, axes[0]) - readTVectorAxis(sectorDrawAnchor_, axes[0]));
        const double depth = sectorDrawFreeView_
            ? std::abs(footprint.y)
            : std::abs(readTVectorAxis(footprint, axes[1]) - readTVectorAxis(sectorDrawAnchor_, axes[1]));
        if (modifiers.testFlag(Qt::ShiftModifier))
        {
            height = std::clamp(std::max({ width, depth, kMinDim }), kMinDim, kMaxDim);
        }
        else
        {
            double unitsPerPixel = glView_->worldUnitsPerPixelAtWorld(anchor);
            if (!std::isfinite(unitsPerPixel) || unitsPerPixel <= 0.0f)
            {
                unitsPerPixel = 0.01f;
            }

            height = std::abs(static_cast<double>(widgetPos.y() - sectorDrawHeightWidgetPos_.y()))
                * static_cast<double>(unitsPerPixel);
            height = std::clamp(finiteOrZero(height), kMinDim, kMaxDim);
        }

        if (hardGridSnap)
        {
            const double spacing = sectorDrawGridFrame_.spacing;
            const double minSteps = std::max(1.0, std::ceil(kMinDim / spacing));
            const double maxSteps = std::floor(kMaxDim / spacing);
            if (maxSteps < minSteps)
            {
                height = 0.0;
            }
            else
            {
                const double steps = std::clamp(
                    std::round(height / spacing), minSteps, maxSteps);
                height = steps * spacing;
            }
        }
    }

    SectorDrawBox box{};
    if (sectorDrawFreeView_)
    {
        box.min = TVector{std::min(0.0, footprint.x), std::min(0.0, footprint.y), 0.0};
        box.max = TVector{std::max(0.0, footprint.x), std::max(0.0, footprint.y), height};
        box.origin = anchor;
        box.axisA = sectorDrawFreeAxisA_;
        box.axisB = sectorDrawFreeAxisB_;
        box.axisHeight = sectorDrawFreeAxisHeight_;
        box.viewOriented = true;
        box.valid = validSectorDrawBounds(box.min, box.max);
        return box;
    }

    TVector heightPoint = sectorDrawAnchor_;
    writeTVectorAxis(heightPoint, axes[2], readTVectorAxis(sectorDrawAnchor_, axes[2]) + height);
    box.min = sectorDrawAnchor_;
    box.max = sectorDrawAnchor_;
    writeTVectorAxis(box.min, axes[0], std::min(readTVectorAxis(sectorDrawAnchor_, axes[0]), readTVectorAxis(footprint, axes[0])));
    writeTVectorAxis(box.max, axes[0], std::max(readTVectorAxis(sectorDrawAnchor_, axes[0]), readTVectorAxis(footprint, axes[0])));
    writeTVectorAxis(box.min, axes[1], std::min(readTVectorAxis(sectorDrawAnchor_, axes[1]), readTVectorAxis(footprint, axes[1])));
    writeTVectorAxis(box.max, axes[1], std::max(readTVectorAxis(sectorDrawAnchor_, axes[1]), readTVectorAxis(footprint, axes[1])));
    writeTVectorAxis(box.min, axes[2], std::min(readTVectorAxis(sectorDrawAnchor_, axes[2]), readTVectorAxis(heightPoint, axes[2])));
    writeTVectorAxis(box.max, axes[2], std::max(readTVectorAxis(sectorDrawAnchor_, axes[2]), readTVectorAxis(heightPoint, axes[2])));
    box.valid = validSectorDrawBounds(box.min, box.max);
    return box;
}

std::int32_t MainWindow::createSectorFromDrawBox(Level& level, const SectorDrawBox& box) const
{
    if (!box.valid || !validSectorDrawBounds(box.min, box.max))
    {
        return -1;
    }

    const std::string shapeName = selectedSectorShapeName_.empty() ? std::string("Cube") : selectedSectorShapeName_;
    const std::optional<sed::level::SectorPrimitive> primitive = ui::sector::primitiveForName(shapeName);
    const std::optional<sed::level::SectorShape> legacyShape =
        primitive ? std::nullopt : ui::sector::legacyShapeByName(shapeName);
    if (!primitive && !legacyShape)
    {
        return -1;
    }

    const std::array<int, 3> axes = box.viewOriented
        ? std::array<int, 3>{0, 1, 2}
        : sectorDrawWorldAxes();
    const std::int32_t sectorIndex = primitive
        ? sed::level::levelCreateSectorPrimitiveBoxAxes(
            level, *primitive, box.min, box.max, axes[0], axes[1], axes[2], selectedSectorShapeSides())
        : createSectorFromTaggedShapeBoxAxes(
            level,
            *legacyShape,
            box.min,
            box.max,
            axes[0],
            axes[1],
            axes[2]);
    if (sectorIndex < 0 || static_cast<std::size_t>(sectorIndex) >= level.sectors.size())
    {
        return -1;
    }

    if (box.viewOriented)
    {
        Sector& sector = level.sectors[static_cast<std::size_t>(sectorIndex)];
        for (Vertex& vertex : sector.vertices)
        {
            const TVector world = box.origin
                + box.axisA * vertex.x
                + box.axisB * vertex.y
                + box.axisHeight * vertex.z;
            vertex.x = world.x;
            vertex.y = world.y;
            vertex.z = world.z;
        }
        // This staged sector has no adjoins, so refresh its normals directly.
        // Avoid preparing Level-wide traversal scratch on every preview update.
        for ( Surface& surface : sector.surfaces )
        {
            surface.normal = sed::level::surfaceCalcNormalNewell(sector, surface);
        }
    }
    return sectorIndex;
}

sed::level::UvTextureDimensionsResolver
MainWindow::generatedSectorUvDimensionsResolver() const
{
    auto cache = std::make_shared<std::unordered_map<std::string,
        std::optional<::sed::level::UvTextureDimensions>>>();
    return [this, cache](const Surface& surface)
        -> std::optional<::sed::level::UvTextureDimensions>
    {
        const std::string key = text::lowerAscii(surface.material);
        const auto cached = cache->find(key);
        if (cached != cache->end())
        {
            return cached->second;
        }

        std::optional<::sed::level::UvTextureDimensions> dimensions =
            assets::resolveMaterialTextureDimensions(
                resources_,
                surface.material);
        if (!dimensions
            && text::iequalsAscii(surface.material, "dflt.mat"))
        {
            dimensions = ui::project::DefaultStarterMaterialDimensions;
        }
        cache->emplace(key, dimensions);
        return dimensions;
    };
}

std::int32_t MainWindow::createSectorFromTaggedShape(
    Level& level,
    const sed::level::SectorShape& shape,
    const TVector& center,
    const double size) const
{
    return sed::level::levelCreateSectorFromShape(
        level,
        shape.sector,
        shape.uvCoordinateSpace,
        center,
        size,
        generatedSectorUvDimensionsResolver());
}

std::int32_t MainWindow::createSectorFromTaggedShapeBoxAxes(
    Level& level,
    const sed::level::SectorShape& shape,
    const TVector& minCorner,
    const TVector& maxCorner,
    const int localXAxis,
    const int localYAxis,
    const int localZAxis) const
{
    return sed::level::levelCreateSectorFromShapeBoxAxes(
        level,
        shape.sector,
        shape.uvCoordinateSpace,
        minCorner,
        maxCorner,
        localXAxis,
        localYAxis,
        localZAxis,
        generatedSectorUvDimensionsResolver());
}

std::vector<TVector> MainWindow::buildSectorDrawPreviewLines(const SectorDrawBox& box) const
{
    Level preview{};
    preview.clear();
    preview.kind = document_.level().kind;
    preview.ppunit = document_.level().ppunit;
    const std::int32_t sectorIndex = createSectorFromDrawBox(preview, box);
    if (sectorIndex < 0 || static_cast<std::size_t>(sectorIndex) >= preview.sectors.size())
    {
        return {};
    }
    return ui::sector::documentWireLinesFromSector(preview.sectors[static_cast<std::size_t>(sectorIndex)]);
}

std::vector<TVector> MainWindow::buildSectorDrawCursorGuideLines(const QPoint& widgetPos) const
{
    if (!glView_)
    {
        return {};
    }

    const std::optional<TVector> centerOpt =
        glView_->viewportPointOnPlane(
            widgetPos,
            sectorDrawPlacementPlanePoint(),
            (sectorDrawAtCameraView_ || sectorDrawFreeView_)
                ? render::cleaveDocumentPoint(glView_->cameraForward())
                : sectorDrawPlaneNormal());
    if (!centerOpt)
    {
        return {};
    }

    TVector center = *centerOpt;
    if (sectorDrawGridSnapActive())
    {
        const render::TransformGridFrame liveGridFrame =
            glView_->activeGridSnapFrame();
        const TVector snapPlaneNormal = sectorDrawFreeView_
            ? render::cleaveDocumentPoint(glView_->cameraForward())
            : sectorDrawPlaneNormal();
        center = render::snapPointToGridOnPlane(
            center,
            sectorDrawFreeView_ ? sectorDrawPlacementPlanePoint() : center,
            snapPlaneNormal,
            liveGridFrame);
    }
    double unitsPerPixel = glView_->worldUnitsPerPixelAtWorld(center);
    if (!std::isfinite(unitsPerPixel) || unitsPerPixel <= 0.0f)
    {
        unitsPerPixel = 0.01f;
    }

    const double radius = std::clamp(unitsPerPixel * 46.0, 0.008, 256.0);
    const std::array<int, 3> axes = sectorDrawWorldAxes();
    auto axisVector = [](const int axis) noexcept -> TVector
    {
        switch (axis)
        {
            case 0: return TVector{1.0f, 0.0f, 0.0f};
            case 1: return TVector{0.0f, 1.0f, 0.0f};
            default: return TVector{0.0f, 0.0f, 1.0f};
        }
    };

    const TVector axisA = (sectorDrawAtCameraView_ || sectorDrawFreeView_)
        ? render::cleaveDocumentPoint(glView_->cameraRight())
        : axisVector(axes[0]);
    const TVector axisB = (sectorDrawAtCameraView_ || sectorDrawFreeView_)
        ? render::cleaveDocumentPoint(glView_->cameraUp())
        : axisVector(axes[1]);

    std::vector<TVector> lines{};
    constexpr int kRingSegments = 56;
    constexpr double kPi = 3.141592653589793238462643383279502884;
    lines.reserve(static_cast<std::size_t>(kRingSegments) * 2u + 32u);

    const double crossHalfSize = radius * 0.16f;
    lines.push_back(center - axisA * crossHalfSize);
    lines.push_back(center + axisA * crossHalfSize);
    lines.push_back(center - axisB * crossHalfSize);
    lines.push_back(center + axisB * crossHalfSize);

    auto pointAt = [&](const double angle, const double scale = 1.0) -> TVector
    {
        return center
            + axisA * (std::cos(angle) * radius * scale)
            + axisB * (std::sin(angle) * radius * scale);
    };

    for (int i = 0; i < kRingSegments; ++i)
    {
        const double a0 = (2.0 * kPi * static_cast<double>(i)) / static_cast<double>(kRingSegments);
        const double a1 = (2.0 * kPi * static_cast<double>(i + 1)) / static_cast<double>(kRingSegments);
        lines.push_back(pointAt(a0));
        lines.push_back(pointAt(a1));
    }

    for (int i = -3; i <= 3; ++i)
    {
        const double offset = static_cast<double>(i) * radius / 4.0;
        const double half = std::sqrt(std::max(0.0, radius * radius - offset * offset));
        lines.push_back(center + axisB * offset - axisA * half);
        lines.push_back(center + axisB * offset + axisA * half);
        lines.push_back(center + axisA * offset - axisB * half);
        lines.push_back(center + axisA * offset + axisB * half);
    }

    return lines;
}

void MainWindow::updateSectorDrawCursorGuide(const QPoint& widgetPos)
{
    if (!glView_)
    {
        return;
    }

    if (placementMode_ == PlacementMode::cube && sectorDrawStage_ == SectorDrawStage::none)
    {
        glView_->setPlacementPreviewLines(buildSectorDrawCursorGuideLines(widgetPos), true);
    }
}

void MainWindow::updateSectorDrawPreview(
    const QPoint& widgetPos,
    const Qt::KeyboardModifiers modifiers)
{
    if (placementMode_ != PlacementMode::cube)
    {
        return;
    }

    if (sectorDrawStage_ == SectorDrawStage::none)
    {
        updateSectorDrawCursorGuide(widgetPos);
        return;
    }

    sectorDrawLastBox_ = sectorDrawBoxForMouse(widgetPos, modifiers);
    if (glView_)
    {
        glView_->setPlacementPreviewLines(buildSectorDrawPreviewLines(sectorDrawLastBox_));
    }

    if (sectorDrawLastBox_.valid && statusBar())
    {
        const std::array<int, 3> axes = sectorDrawLastBox_.viewOriented
            ? std::array<int, 3>{0, 1, 2}
            : sectorDrawWorldAxes();
        auto readAxis = [](const TVector& value, const int axis) noexcept -> double
        {
            switch (axis)
            {
                case 0: return value.x;
                case 1: return value.y;
                default: return value.z;
            }
        };
        const double width = readAxis(sectorDrawLastBox_.max, axes[0]) - readAxis(sectorDrawLastBox_.min, axes[0]);
        const double depth = readAxis(sectorDrawLastBox_.max, axes[1]) - readAxis(sectorDrawLastBox_.min, axes[1]);
        const double height = readAxis(sectorDrawLastBox_.max, axes[2]) - readAxis(sectorDrawLastBox_.min, axes[2]);
        if (sectorDrawStage_ == SectorDrawStage::footprint)
        {
            statusBar()->showMessage(QString("Drawing footprint: %1 x %2 (hold Shift for square, release LMB for height, Esc to cancel).")
                .arg(render::compactCleaveMeasurementNumber(width, 6))
                .arg(render::compactCleaveMeasurementNumber(depth, 6)));
        }
        else
        {
            statusBar()->showMessage(QString("Drawing sector: %1 x %2 x %3 (Shift: equal height, click to create, Esc to cancel).")
                .arg(render::compactCleaveMeasurementNumber(width, 6))
                .arg(render::compactCleaveMeasurementNumber(depth, 6))
                .arg(render::compactCleaveMeasurementNumber(height, 6)));
        }
    }
}

bool MainWindow::beginSectorDrawPlacement(
    const QVector3D& position,
    const bool haveSurfaceHit,
    const QPoint& widgetPos,
    const Qt::KeyboardModifiers modifiers)
{
    if (!glView_)
    {
        return false;
    }

    sectorDrawGridFrame_ = {};

    const std::string shapeName = selectedSectorShapeName_.empty() ? std::string("Cube") : selectedSectorShapeName_;
    const std::optional<sed::level::SectorPrimitive> primitive = ui::sector::primitiveForName(shapeName);
    const std::optional<sed::level::SectorShape> legacyShape =
        primitive ? std::nullopt : ui::sector::legacyShapeByName(shapeName);
    if (!primitive && !legacyShape)
    {
        glView_->showToast(
            QString("Can't find sector shape: %1").arg(ui::sector::shapeLabel(shapeName)),
            2600,
            GlView::ToastPlacement::BottomCenter,
            GlView::ToastGroup::Error);
        return false;
    }

    // Freeze the exact authored grid origin/orientation and the adaptive minor
    // spacing visible at press time. Toggling Grid Snap during the gesture can
    // then change only whether this frame is used, never the lattice itself.
    sectorDrawGridFrame_ = glView_->activeGridSnapFrame();

    TVector anchor = render::cleaveDocumentPoint(position);
    if (haveSurfaceHit)
    {
        TVector precisePosition{};
        if (glView_->placementPointForViewport(widgetPos, nullptr, nullptr, nullptr, false, false, &precisePosition))
        {
            anchor = precisePosition;
        }
    }
    if (sectorDrawAtCameraView_ || sectorDrawFreeView_)
    {
        if (const std::optional<TVector> viewPlaneHit =
                glView_->viewportPointOnPlane(
                    widgetPos, sectorDrawPlacementPlanePoint(), render::cleaveDocumentPoint(glView_->cameraForward())))
        {
            anchor = *viewPlaneHit;
        }
        else
        {
            sectorDrawGridFrame_ = {};
            return false;
        }
    }
    else if (!haveSurfaceHit)
    {
        if (const std::optional<TVector> gridHit =
                glView_->viewportPointOnPlane(
                    widgetPos, sectorDrawPlacementPlanePoint(), sectorDrawPlaneNormal()))
        {
            anchor = *gridHit;
        }
        else
        {
            switch (sectorDrawPlaneAxis_)
            {
                case SectorDrawPlaneAxis::x:
                    anchor.x = 0.0f;
                    break;
                case SectorDrawPlaneAxis::y:
                    anchor.y = 0.0f;
                    break;
                case SectorDrawPlaneAxis::z:
                default:
                    anchor.z = 0.0f;
                    break;
            }
        }
    }

    const TVector interactionNormal =
        sectorDrawFreeView_
            ? render::cleaveDocumentPoint(glView_->cameraForward())
            : sectorDrawPlaneNormal();
    anchor = snapSectorDrawPointToGrid(
        anchor, anchor, interactionNormal);

    sectorDrawStage_ = SectorDrawStage::footprint;
    sectorDrawAnchor_ = anchor;
    if (sectorDrawFreeView_)
    {
        sectorDrawFreeAxisA_ = render::cleaveDocumentPoint(glView_->cameraRight());
        sectorDrawFreeAxisB_ = -render::cleaveDocumentPoint(glView_->cameraUp());
        sectorDrawFreeAxisHeight_ = render::cleaveDocumentPoint(glView_->cameraForward());
        sectorDrawFootprint_ = TVector::zero;
    }
    else
    {
        sectorDrawFootprint_ = sectorDrawAnchor_;
    }
    sectorDrawAnchorWidgetPos_ = widgetPos;
    sectorDrawHeightWidgetPos_ = widgetPos;
    sectorDrawLastBox_ = {};

    updateSectorDrawPreview(widgetPos, modifiers);
    if (statusBar())
    {
        const QString shape = ui::sector::shapeLabel(shapeName);
        statusBar()->showMessage(QString("Drawing %1: drag footprint, release for height (Shift: square, Esc: cancel).").arg(shape));
    }
    return true;
}

bool MainWindow::finishSectorDrawFootprint(
    const QPoint& widgetPos,
    const Qt::KeyboardModifiers modifiers,
    const bool dragged)
{
    if (placementMode_ != PlacementMode::cube || sectorDrawStage_ != SectorDrawStage::footprint)
    {
        return false;
    }

    if (!dragged || (widgetPos - sectorDrawAnchorWidgetPos_).manhattanLength() <= 2)
    {
        cancelSectorDrawPlacement();
        if (statusBar())
        {
            statusBar()->showMessage("Drag to draw the sector footprint (Esc to cancel).", 1800);
        }
        return false;
    }

    updateSectorDrawPreview(widgetPos, modifiers);
    if (!sectorDrawLastBox_.valid)
    {
        cancelSectorDrawPlacement();
        return false;
    }

    const std::array<int, 3> axes = sectorDrawWorldAxes();
    auto readAxis = [](const TVector& value, const int axis) noexcept -> double
    {
        switch (axis)
        {
            case 0: return value.x;
            case 1: return value.y;
            default: return value.z;
        }
    };
    auto writeAxis = [](TVector& value, const int axis, const double v) noexcept
    {
        switch (axis)
        {
            case 0:
                value.x = v;
                break;
            case 1:
                value.y = v;
                break;
            default:
                value.z = v;
                break;
        }
    };

    if (sectorDrawFreeView_)
    {
        sectorDrawFootprint_ = TVector{
            (std::abs(sectorDrawLastBox_.min.x) > std::abs(sectorDrawLastBox_.max.x))
                ? sectorDrawLastBox_.min.x : sectorDrawLastBox_.max.x,
            (std::abs(sectorDrawLastBox_.min.y) > std::abs(sectorDrawLastBox_.max.y))
                ? sectorDrawLastBox_.min.y : sectorDrawLastBox_.max.y,
            0.0};
    }
    else
    {
        sectorDrawFootprint_ = sectorDrawAnchor_;
        for (const int axis : { axes[0], axes[1] })
        {
            const double minDelta = std::abs(readAxis(sectorDrawLastBox_.min, axis) - readAxis(sectorDrawAnchor_, axis));
            const double maxDelta = std::abs(readAxis(sectorDrawLastBox_.max, axis) - readAxis(sectorDrawAnchor_, axis));
            writeAxis(
                sectorDrawFootprint_,
                axis,
                (minDelta > maxDelta) ? readAxis(sectorDrawLastBox_.min, axis) : readAxis(sectorDrawLastBox_.max, axis));
        }
    }
    sectorDrawHeightWidgetPos_ = widgetPos;
    sectorDrawStage_ = SectorDrawStage::height;
    updateSectorDrawPreview(widgetPos, modifiers);

    if (statusBar())
    {
        statusBar()->showMessage("Move to set sector height, click to create (Shift: equal height, Esc: cancel).");
    }
    return true;
}

bool MainWindow::commitSectorDrawPlacement()
{
    if (sectorDrawStage_ == SectorDrawStage::none)
    {
        return false;
    }

    const SectorDrawBox box = sectorDrawLastBox_.valid
        ? sectorDrawLastBox_
        : sectorDrawBoxForMouse(sectorDrawAnchorWidgetPos_, QApplication::keyboardModifiers());
    if (!box.valid)
    {
        return false;
    }

    const std::string shapeName = selectedSectorShapeName_.empty() ? std::string("Cube") : selectedSectorShapeName_;
    const std::optional<sed::level::SectorPrimitive> primitive = ui::sector::primitiveForName(shapeName);
    const std::optional<sed::level::SectorShape> legacyShape =
        primitive ? std::nullopt : ui::sector::legacyShapeByName(shapeName);
    if (!primitive && !legacyShape)
    {
        if (glView_)
        {
            glView_->showToast(
                QString("Can't find sector shape: %1").arg(ui::sector::shapeLabel(shapeName)),
                2600,
                GlView::ToastPlacement::BottomCenter,
                GlView::ToastGroup::Error);
        }
        return false;
    }

    std::int32_t selectedSector = -1;
    const bool applied = document_.applyUndoable(
        "Sector: Added",
        editor::DocumentDirty::Structure | editor::DocumentDirty::LevelGeometry,
        [&](editor::DocumentState& state) -> bool
        {
            const std::int32_t newSectorIndex = createSectorFromDrawBox(state.level, box);

            if (newSectorIndex < 0)
            {
                return false;
            }

            editor::Selection selection{};
            selection.sectors.insert(newSectorIndex);
            state.selection = std::move(selection);
            selectedSector = newSectorIndex;
            return true;
        });

    if (!applied)
    {
        return false;
    }

    invalidateSurfaceSelectionContextForTopologyEdit();

    cancelSectorDrawPlacement();

    if (glView_)
    {
        glView_->notifyLevelGeometryChanged();
        glView_->setSelectionState(selectedSector, -1, -1, -1, document_.selection());
    }

    rebuildOutliner();
    rebuildProperties();
    updateEditActions();
    setPlacementMode(PlacementMode::none);
    return true;
}

} // namespace sed
