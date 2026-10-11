#include "ui/components/precise_numeric_input.hpp"
#include "main_window/common.hpp"
#include "ui/shortcuts/key_sequence_display.hpp"
#include "ui/components/ui_decimal_format.hpp"
#include "main_window/detail/undo_labels.hpp"
#include "ui/components/compact_double_spin_box.hpp"
#include "ui/components/flow_layout.hpp"
#include "ui/uv/follow_active_quads_utils.hpp"
#include "ui/uv/smart_uv_project_dialog.hpp"
#include "ui/uv/smart_uv_project_utils.hpp"
#include "ui/uv/uv_island_geometry_utils.hpp"
#include "ui/uv/uv_surface_geometry_utils.hpp"
#include "ui/uv/uv_connection_utils.hpp"
#include "ui/shortcuts/shortcut_context_utils.hpp"
#include "ui/viewport/thing_placement_panel_resize_utils.hpp"
#include "ui/viewport/viewport_zoom_utils.hpp"
#include "render/viewport_color_theme.hpp"

#include "sed/assets/material_dimensions.hpp"

#include <QCborArray>
#include <QCborMap>
#include <QCborValue>
#include <QComboBox>
#include <QElapsedTimer>
#include <QDoubleSpinBox>
#include <QTimer>

#include <limits>
#include <numbers>
#include <optional>
#include <ranges>
#include <vector>

namespace sed
{

namespace
{

using ::sed::ui::FlowLayout;
using ::sed::ui::CompactDoubleSpinBox;
using ::sed::ui::uv::denormalizeStoredUv;
using ::sed::ui::uv::estimateSmartUvIslandScale;
using ::sed::ui::uv::followActiveQuads;
using ::sed::ui::uv::followActiveQuadsErrorMessage;
using ::sed::ui::uv::FollowActiveQuadsMode;
using ::sed::ui::uv::FollowActiveQuadsReport;
using ::sed::ui::uv::islandBounds;
using ::sed::ui::uv::islandCentroid;
using ::sed::ui::uv::islandDisplayFromStoredUv;
using ::sed::ui::uv::islandStoredFromDisplayUv;
using ::sed::ui::uv::islandsBounds;
using ::sed::ui::uv::normalizeStoredUv;
using ::sed::ui::uv::normalizeUvVector;
using ::sed::ui::uv::packIslands;
using ::sed::ui::uv::polygonCentroid;
using ::sed::ui::uv::polygonSignedArea;
using ::sed::ui::uv::sanitizeUvTextureSize;
using ::sed::ui::uv::SmartUvProjectDialog;
using ::sed::ui::uv::SmartUvProjectOptions;
using ::sed::ui::uv::smartUvProject;
using ::sed::ui::uv::translateIslandDisplay;
using ::sed::ui::uv::UvIslandState;
using ::sed::ui::uv::UvViewOrientation;
using ::sed::ui::uv::uvIslandStatesStoreSameUv;
using ::sed::ui::uv::uvCollectSnapGeometry;
using ::sed::ui::uv::uvNearestEdgeSnapDelta;
using ::sed::ui::uv::uvNearestVisibleGridSnapDelta;
using ::sed::ui::uv::uvNearestPointSnapDelta;
using ::sed::ui::uv::uvPointBounds;
using ::sed::ui::uv::uvPointCloudCenter;
using ::sed::ui::uv::uvRotateTexVerticesAroundCenter;
using ::sed::ui::uv::uvScaleTexVerticesAroundCenter;
using ::sed::ui::uv::UvSnapGeometry;
using ::sed::ui::uv::UvSnapEdge;
using ::sed::ui::uv::uvViewOrientationForActiveSurface;
using ::sed::ui::uv::uvViewPanForAnchor;
using ::sed::ui::uv::uvViewScreenFromUv;
using ::sed::ui::uv::uvViewUvFromScreen;
using ::sed::ui::uv::uvSurfaceColormapName;
using ::sed::ui::uv::uvSurfaceNormal;
using ::sed::ui::uv::uvSurfaceVertexWorld;
using ::sed::ui::uv::uvSurfacesShareEdge;
using ::sed::ui::viewport::compensatedOrthoScaleForPanelResize;
using ::sed::ui::viewport::compensatedPerspectiveDistanceForPanelResize;
using ::sed::ui::viewport::viewportSizeAfterPanelSplitterMove;

/**
 * UV edits are edits to the selected Surface owners, not to a pseudo UV
 * entity. Keep the completed operation text separate so the
 * normal undo label helper can select a real singular/plural owner.
 */
QString surfaceUvUndoLabel(const std::size_t surfaceCount,
                           const std::string_view operation)
{
    return QString::fromStdString(ui::undo::action(
        "Surface", "Surfaces", surfaceCount, operation));
}

constexpr int kUvEditorPanelTransitionMs = 180;

QEasingCurve makeUvEditorPanelEasingCurve()
{
    QEasingCurve curve(QEasingCurve::BezierSpline);
    curve.addCubicBezierSegment(QPointF(0.42, 0.0), QPointF(0.58, 1.0), QPointF(1.0, 1.0));
    return curve;
}

void setUvEditorPanelTransitionCollapsible(
    QSplitter* splitter,
    QWidget* panelHost,
    const bool collapsible)
{
    if (!splitter || !panelHost)
    {
        return;
    }

    const int panelIndex = splitter->indexOf(panelHost);
    if (panelIndex >= 0)
    {
        splitter->setCollapsible(panelIndex, collapsible);
    }
}

/**
 * Returns the splitter axis available to child widgets at this layout state.
 *
 * QSplitter::sizes() can temporarily expose the last requested weights while
 * a newly shown/collapsible child has not completed its layout pass. Summing
 * those values can therefore include the handle a second time. Endpoint
 * requests must instead use physical splitter geometry minus handles that
 * are visibly or logically participating between two allocated children.
 */
int uvSplitterAvailableChildExtent(const QSplitter* splitter)
{
    if (!splitter)
    {
        return 1;
    }

    const bool vertical = splitter->orientation() == Qt::Vertical;
    const int axisExtent = vertical ? splitter->height() : splitter->width();
    const QList<int> sizes = splitter->sizes();
    int visibleHandleExtent = 0;
    for (int index = 1; index < splitter->count(); ++index)
    {
        const QSplitterHandle* handle = splitter->handle(index);
        if (!handle)
        {
            continue;
        }

        // A newly revealed child can have a positive splitter allocation one
        // layout turn before Qt clears the handle's hidden flag. Both adjacent
        // widgets are already participating at that point, so omitting the
        // handle would overstate the logical child extent by handleWidth().
        // That oversized request is later proportionally normalized and moves
        // an otherwise exact restored UV endpoint by several pixels.
        const QWidget* before = splitter->widget(index - 1);
        const QWidget* after = splitter->widget(index);
        const bool adjacentChildrenParticipate = before && after
            && !before->isHidden() && !after->isHidden()
            && index - 1 < sizes.size() && index < sizes.size()
            && sizes[index - 1] > 0 && sizes[index] > 0;
        if (handle->isHidden() && !adjacentChildrenParticipate)
        {
            continue;
        }
        const int realizedExtent = handle->isHidden()
            ? 0
            : (vertical ? handle->height() : handle->width());
        visibleHandleExtent += realizedExtent > 0
            ? realizedExtent
            : splitter->handleWidth();
    }
    return std::max(1, axisExtent - visibleHandleExtent);
}

QColor uvEditorColorForIndex(int index);

QIcon makeUvPanelChevronIcon(const qreal angleDeg, const QColor& color)
{
    QPixmap pixmap(18, 18);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.translate(9.0, 9.0);
    painter.rotate(angleDeg);
    painter.translate(-9.0, -9.0);
    painter.setPen(QPen(color, 1.8, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.setBrush(Qt::NoBrush);
    QPolygonF chevron{};
    chevron << QPointF(4.5, 6.0) << QPointF(9.0, 11.0) << QPointF(13.5, 6.0);
    painter.drawPolyline(chevron);
    return QIcon(pixmap);
}

std::optional<assets::CmpPalette> tryLoadCmpPalette(const assets::ResourceLocator& resources, const std::string_view cmpName)
{
    const std::string trimmed = ::sed::text::trim(cmpName);
    if (trimmed.empty() || trimmed == "-")
    {
        return std::nullopt;
    }

    const auto cmpBytes = resources.tryReadBinaryWithExt(trimmed, ".cmp");
    if (!cmpBytes)
    {
        return std::nullopt;
    }

    return assets::parseCmpPalette(*cmpBytes);
}

std::optional<QImage> loadUvMaterialImage(
    const assets::ResourceLocator& resources,
    const QString& material,
    const std::string_view cmpName = {})
{
    if (material.trimmed().isEmpty())
    {
        return std::nullopt;
    }

    try
    {
        const auto bytes = resources.tryReadBinaryWithExt(material.toStdString(), ".mat");
        if (!bytes)
        {
            return std::nullopt;
        }
        const std::optional<assets::CmpPalette> paletteStorage = tryLoadCmpPalette(resources, cmpName);
        const assets::CmpPalette* palette = paletteStorage ? &*paletteStorage : nullptr;
        const assets::MatFile mat = assets::MatFile::parse(std::vector<std::byte>(bytes->begin(), bytes->end()), palette);
        const ::sed::Rgba8Image img = mat.decodeCelRgba8(0);
        if (img.empty())
        {
            return std::nullopt;
        }

        QImage out(img.width, img.height, QImage::Format_RGBA8888);
        if (out.isNull())
        {
            return std::nullopt;
        }

        const std::size_t byteCount = static_cast<std::size_t>(img.width) * static_cast<std::size_t>(img.height) * 4u;
        std::copy_n(
            reinterpret_cast<const std::byte*>(img.rgba.data()),
            byteCount,
            reinterpret_cast<std::byte*>(out.bits()));
        return out;
    }
    catch (...)
    {
        return std::nullopt;
    }
}

std::optional<float> checkedUvFloat(const double value) noexcept
{
    if (!std::isfinite(value)
        || value < -static_cast<double>(std::numeric_limits<float>::max())
        || value > static_cast<double>(std::numeric_limits<float>::max()))
    {
        return std::nullopt;
    }
    return static_cast<float>(value);
}

Surface* uvSurfaceForRef(Level& level, const SurfaceRef ref) noexcept
{
    if (ref.sector < 0 || ref.surface < 0
        || static_cast<std::size_t>(ref.sector) >= level.sectors.size())
    {
        return nullptr;
    }
    Sector& sector = level.sectors[static_cast<std::size_t>(ref.sector)];
    if (static_cast<std::size_t>(ref.surface) >= sector.surfaces.size())
    {
        return nullptr;
    }
    return &sector.surfaces[static_cast<std::size_t>(ref.surface)];
}

std::optional<std::vector<QSize>> uvActionTextureSizes(
    Level& level,
    const std::vector<SurfaceRef>& refs,
    const assets::ResourceLocator& resources)
{
    std::vector<QSize> sizes(refs.size(), QSize(1, 1));
    if (level.uvCoordinateSpace == UvCoordinateSpace::Normalized)
    {
        return sizes;
    }
    if (level.uvCoordinateSpace != UvCoordinateSpace::TexturePixels)
    {
        return std::nullopt;
    }

    std::unordered_map<QString, QSize> cache{};
    for (std::size_t index = 0; index < refs.size(); ++index)
    {
        const Surface* surface = uvSurfaceForRef(level, refs[index]);
        if (!surface)
        {
            return std::nullopt;
        }

        const QString material = QString::fromStdString(surface->material)
            .trimmed();
        if (material.isEmpty())
        {
            return std::nullopt;
        }
        const QString materialKey = material.toLower();

        QSize size{};
        if (const auto found = cache.find(materialKey); found != cache.end())
        {
            size = found->second;
        }
        else
        {
            if (const std::optional<level::UvTextureDimensions> dimensions =
                    assets::resolveMaterialTextureDimensions(
                        resources, material.toStdString()))
            {
                if (std::isfinite(dimensions->width)
                    && std::isfinite(dimensions->height)
                    && dimensions->width > 0.0
                    && dimensions->height > 0.0
                    && dimensions->width
                        <= static_cast<double>(std::numeric_limits<int>::max())
                    && dimensions->height
                        <= static_cast<double>(std::numeric_limits<int>::max()))
                {
                    size = QSize(
                        static_cast<int>(dimensions->width),
                        static_cast<int>(dimensions->height));
                }
            }
            cache.emplace(materialKey, size);
        }
        if (!size.isValid() || size.width() <= 0 || size.height() <= 0)
        {
            return std::nullopt;
        }
        sizes[index] = size;
    }
    return sizes;
}

bool convertSurfaceUvStorage(
    Surface& surface,
    const UvCoordinateSpace source,
    const UvCoordinateSpace destination,
    const QSize textureSize)
{
    const auto validSpace = [](const UvCoordinateSpace space) noexcept
    {
        return space == UvCoordinateSpace::TexturePixels
            || space == UvCoordinateSpace::Normalized;
    };
    if (!validSpace(source)
        || !validSpace(destination)
        || surface.texVertices.size() != surface.vertices.size())
    {
        return false;
    }
    if (source == destination)
    {
        return true;
    }
    if (!textureSize.isValid()
        || textureSize.width() <= 0 || textureSize.height() <= 0)
    {
        return false;
    }

    const double width = static_cast<double>(textureSize.width());
    const double height = static_cast<double>(textureSize.height());
    const double uFactor = source == UvCoordinateSpace::TexturePixels
        ? 1.0 / width
        : width;
    const double vFactor = source == UvCoordinateSpace::TexturePixels
        ? 1.0 / height
        : height;
    const std::optional<float> uScale = checkedUvFloat(
        static_cast<double>(surface.uScale) * uFactor);
    const std::optional<float> vScale = checkedUvFloat(
        static_cast<double>(surface.vScale) * vFactor);
    if (!uScale || !vScale)
    {
        return false;
    }

    std::vector<TexVertex> converted{};
    converted.reserve(surface.texVertices.size());
    for (const TexVertex& texVertex : surface.texVertices)
    {
        const std::optional<float> u = checkedUvFloat(
            static_cast<double>(texVertex.u) * uFactor);
        const std::optional<float> v = checkedUvFloat(
            static_cast<double>(texVertex.v) * vFactor);
        if (!u || !v)
        {
            return false;
        }
        TexVertex next = texVertex;
        next.u = *u;
        next.v = *v;
        converted.push_back(next);
    }

    surface.texVertices = std::move(converted);
    surface.uScale = *uScale;
    surface.vScale = *vScale;
    return true;
}

struct PixelUvSurfaceStage
{
    SurfaceRef ref{};
    QSize textureSize{};
    std::vector<TexVertex> originalTexVertices{};
    float originalUScale{};
    float originalVScale{};
};

std::optional<std::vector<PixelUvSurfaceStage>> normalizePixelUvSurfaces(
    Level& level,
    const std::vector<SurfaceRef>& refs,
    const assets::ResourceLocator& resources)
{
    if (level.uvCoordinateSpace != UvCoordinateSpace::TexturePixels)
    {
        return std::nullopt;
    }
    const std::optional<std::vector<QSize>> sizes =
        uvActionTextureSizes(level, refs, resources);
    if (!sizes)
    {
        return std::nullopt;
    }

    std::vector<PixelUvSurfaceStage> stages{};
    stages.reserve(refs.size());
    for (std::size_t index = 0; index < refs.size(); ++index)
    {
        Surface* surface = uvSurfaceForRef(level, refs[index]);
        if (!surface)
        {
            return std::nullopt;
        }
        stages.push_back(PixelUvSurfaceStage{
            refs[index],
            (*sizes)[index],
            surface->texVertices,
            surface->uScale,
            surface->vScale });
    }

    for (const PixelUvSurfaceStage& stage : stages)
    {
        Surface* surface = uvSurfaceForRef(level, stage.ref);
        if (!surface
            || !convertSurfaceUvStorage(
                *surface,
                UvCoordinateSpace::TexturePixels,
                UvCoordinateSpace::Normalized,
                stage.textureSize))
        {
            return std::nullopt;
        }
    }
    level.uvCoordinateSpace = UvCoordinateSpace::Normalized;
    return stages;
}

bool finishPixelUvSurfaces(
    Level& level,
    const std::vector<PixelUvSurfaceStage>& stages,
    const std::vector<SurfaceRef>& changedRefs)
{
    if (level.uvCoordinateSpace != UvCoordinateSpace::Normalized)
    {
        return false;
    }

    const auto wasChanged = [&changedRefs](const SurfaceRef ref)
    {
        return std::ranges::any_of(changedRefs, [ref](const SurfaceRef changed)
            {
                return changed.sector == ref.sector
                    && changed.surface == ref.surface;
            });
    };
    for (const PixelUvSurfaceStage& stage : stages)
    {
        Surface* surface = uvSurfaceForRef(level, stage.ref);
        if (!surface)
        {
            return false;
        }
        if (!wasChanged(stage.ref))
        {
            surface->texVertices = stage.originalTexVertices;
            surface->uScale = stage.originalUScale;
            surface->vScale = stage.originalVScale;
            continue;
        }
        if (!convertSurfaceUvStorage(
                *surface,
                UvCoordinateSpace::Normalized,
                UvCoordinateSpace::TexturePixels,
                stage.textureSize))
        {
            return false;
        }
    }
    level.uvCoordinateSpace = UvCoordinateSpace::TexturePixels;
    return true;
}

struct UvEditorBuildResult
{
    std::vector<UvIslandState> islands{};
    QImage previewTexture{};
};

std::optional<std::vector<QPointF>> projectSurfacePolygon(
    const Level& level,
    const SurfaceRef ref,
    ::sed::GlView* glView)
{
    if (!glView || ref.sector < 0 || ref.surface < 0 || static_cast<std::size_t>(ref.sector) >= level.sectors.size())
    {
        return std::nullopt;
    }

    const Sector& sector = level.sectors[static_cast<std::size_t>(ref.sector)];
    if (static_cast<std::size_t>(ref.surface) >= sector.surfaces.size())
    {
        return std::nullopt;
    }

    const Surface& surface = sector.surfaces[static_cast<std::size_t>(ref.surface)];
    QVector3D surfaceNormal(
        static_cast<float>(surface.normal.x),
        static_cast<float>(surface.normal.y),
        static_cast<float>(surface.normal.z));
    if (surfaceNormal.lengthSquared() <= 1.0e-12f)
    {
        return std::nullopt;
    }
    surfaceNormal.normalize();
    const float facing = QVector3D::dotProduct(surfaceNormal, glView->cameraForward());
    if (!std::isfinite(facing) || std::abs(facing) < 0.01f)
    {
        // Near edge-on faces have no stable screen-space winding. Keep the
        // previous whole-view orientation until the face is clearly visible.
        return std::nullopt;
    }

    std::vector<QPointF> projected{};
    projected.reserve(surface.vertices.size());
    for (std::size_t i = 0; i < surface.vertices.size(); ++i)
    {
        const std::optional<QVector3D> world = uvSurfaceVertexWorld(level, ref, i);
        if (!world)
        {
            return std::nullopt;
        }

        const std::optional<QPointF> screen = glView->projectWorldToViewport(*world);
        if (!screen)
        {
            return std::nullopt;
        }
        projected.push_back(*screen);
    }

    const qreal area = polygonSignedArea(projected);
    if (!std::isfinite(area) || std::abs(area) < 4.0)
    {
        return std::nullopt;
    }
    return projected;
}

std::optional<UvEditorBuildResult> buildUvEditorState(
    const Level& level,
    const editor::Selection& selection,
    const assets::ResourceLocator& resources,
    ::sed::GlView* glView,
    const std::optional<SurfaceRef>& activeSurfaceRef = std::nullopt,
    const std::vector<UvIslandState>* previousDisplayIslands = nullptr,
    const bool selectionChanged = false)
{
    Q_UNUSED(selectionChanged);
    Q_UNUSED(glView);

    if (level.uvCoordinateSpace != UvCoordinateSpace::Normalized
        && level.uvCoordinateSpace != UvCoordinateSpace::TexturePixels)
    {
        return std::nullopt;
    }

    std::vector<SurfaceRef> refs{};
    refs.reserve(selection.surfaces.size());
    for (const SurfaceRef ref : selection.surfaces)
    {
        refs.push_back(ref);
    }

    std::sort(refs.begin(), refs.end(), [](const SurfaceRef& a, const SurfaceRef& b)
        {
            if (a.sector != b.sector)
            {
                return a.sector < b.sector;
            }
            return a.surface < b.surface;
        });

    if (refs.empty())
    {
        return std::nullopt;
    }

    UvEditorBuildResult result{};
    result.islands.reserve(refs.size());

    std::unordered_map<QString, QSize> uvTextureSizeCache{};
    auto textureSizeForMaterial = [&resources, &uvTextureSizeCache](const QString& material) -> QSize
    {
        const QString key = material.trimmed().toLower();
        if (key.isEmpty())
        {
            return {};
        }

        if (const auto it = uvTextureSizeCache.find(key); it != uvTextureSizeCache.end())
        {
            return it->second;
        }

        QSize size{};
        if (const std::optional<QImage> image = loadUvMaterialImage(resources, material))
        {
            size = sanitizeUvTextureSize(image->size());
        }
        uvTextureSizeCache.emplace(key, size);
        return size;
    };

    for (std::size_t i = 0; i < refs.size(); ++i)
    {
        const SurfaceRef ref = refs[i];
        if (ref.sector < 0 || ref.surface < 0 || static_cast<std::size_t>(ref.sector) >= level.sectors.size())
        {
            continue;
        }

        const Sector& sector = level.sectors[static_cast<std::size_t>(ref.sector)];
        if (static_cast<std::size_t>(ref.surface) >= sector.surfaces.size())
        {
            continue;
        }

        const Surface& surface = sector.surfaces[static_cast<std::size_t>(ref.surface)];
        if (surface.vertices.empty()
            || surface.texVertices.size() != surface.vertices.size()
            || !std::isfinite(surface.uScale)
            || !std::isfinite(surface.vScale)
            || std::ranges::any_of(
                surface.texVertices,
                [](const TexVertex& texVertex)
                {
                    return !std::isfinite(texVertex.u)
                        || !std::isfinite(texVertex.v);
                }))
        {
            continue;
        }

        UvIslandState island{};
        island.ref = ref;
        island.storedCoordinateSpace = level.uvCoordinateSpace;
        island.label = QString("S%1/%2").arg(ref.sector).arg(ref.surface);
        island.color = QColor(uvEditorColorForIndex(static_cast<int>(i)));
        island.material = QString::fromStdString(surface.material);
        island.textureSize = textureSizeForMaterial(island.material);
        if (level.uvCoordinateSpace == UvCoordinateSpace::TexturePixels
            && !island.textureSize.isValid())
        {
            // A legacy pixel-space document cannot be edited safely without
            // the real MAT dimensions. Never substitute a synthetic 1x1 size.
            continue;
        }
        if (!island.textureSize.isValid())
        {
            island.textureSize = QSize(1, 1);
        }
        island.selected = true;

        island.uv.reserve(surface.texVertices.size());
        for (const TexVertex& tv : surface.texVertices)
        {
            const QPointF stored(
                static_cast<qreal>(tv.u),
                static_cast<qreal>(tv.v));
            island.uv.push_back(
                level.uvCoordinateSpace == UvCoordinateSpace::Normalized
                    ? stored
                    : normalizeStoredUv(stored, island.textureSize));
        }

        const QRectF storedBounds = uvPointBounds(island.uv);
        island.displayMirrorAxisX = storedBounds.isValid() ? storedBounds.center().x() : 0.0;

        TVector xAxis{};
        TVector yAxis{};
        double minx{};
        double miny{};
        double maxx{};
        double maxy{};
        if (::sed::level::surfaceCalcAxesAndBounds(sector, surface, xAxis, yAxis, minx, miny, maxx, maxy))
        {
            const std::int32_t firstVertexIndex = surface.vertices.front();
            if (firstVertexIndex >= 0 && static_cast<std::size_t>(firstVertexIndex) < sector.vertices.size())
            {
                const Vertex& origin = sector.vertices[static_cast<std::size_t>(firstVertexIndex)];
                island.unwrapUv.reserve(surface.vertices.size());
                for (const std::int32_t vi : surface.vertices)
                {
                    if (vi < 0 || static_cast<std::size_t>(vi) >= sector.vertices.size())
                    {
                        island.unwrapUv.clear();
                        break;
                    }

                    const Vertex& v = sector.vertices[static_cast<std::size_t>(vi)];
                    const TVector d{ v.x - origin.x, v.y - origin.y, v.z - origin.z };
                    island.unwrapUv.emplace_back(
                        static_cast<qreal>(xAxis.Dot(d)),
                        static_cast<qreal>(yAxis.Dot(d)));
                }
            }
        }

        if (island.unwrapUv.empty())
        {
            island.unwrapUv = island.uv;
        }

        result.islands.push_back(std::move(island));
    }

    if (result.islands.empty())
    {
        return std::nullopt;
    }

    struct PreviousDisplayState
    {
        const UvIslandState* island{};
        bool displayMirrorX{false};
    };
    const auto refKey = [](const SurfaceRef ref) -> std::uint64_t
    {
        return (static_cast<std::uint64_t>(static_cast<std::uint32_t>(ref.sector)) << 32u)
            | static_cast<std::uint32_t>(ref.surface);
    };
    const auto sameSurfaceRef = [](const SurfaceRef& lhs, const SurfaceRef& rhs) noexcept
    {
        return lhs.sector == rhs.sector && lhs.surface == rhs.surface;
    };
    std::unordered_map<std::uint64_t, PreviousDisplayState> previousDisplayStates{};
    if (previousDisplayIslands)
    {
        previousDisplayStates.reserve(previousDisplayIslands->size());
        for (const UvIslandState& island : *previousDisplayIslands)
        {
            previousDisplayStates.emplace(
                refKey(island.ref),
                PreviousDisplayState{ &island, island.displayMirrorX });
        }
    }
    struct ReusedDisplayGeometry
    {
        bool uv{ false };
        bool unwrap{ false };
    };
    std::unordered_map<std::uint64_t, ReusedDisplayGeometry> reusedDisplayGeometry{};
    const auto sameStoredPoints = ui::uv::uvDisplayPointsStoreSameCoordinates;

    const auto computeIslandMirrorX = [](const UvIslandState&) -> bool
    {
        // Keep the editable UV overlay in true texture space. Mirroring only
        // the island while the texture background remains unmirrored makes
        // drag edits move opposite to the UVs written back to the level.
        return false;
    };

    for (UvIslandState& island : result.islands)
    {
        const auto previousIt = previousDisplayStates.find(refKey(island.ref));
        if (previousIt != previousDisplayStates.end() && previousIt->second.island)
        {
            const UvIslandState& previous = *previousIt->second.island;
            const bool isActiveIsland =
                activeSurfaceRef && sameSurfaceRef(island.ref, *activeSurfaceRef);
            const bool sameDisplayedUvStored = sameStoredPoints(previous, previous.uv, island, island.uv);
            const bool sameDisplayedUnwrapStored = sameStoredPoints(previous, previous.unwrapUv, island, island.unwrapUv);

            if (isActiveIsland && sameDisplayedUvStored)
            {
                // Surfaces that were already displayed should keep their exact
                // current overlay only for the active/root island. The sync
                // path always resolves a deterministic root island before it
                // calls into the builder, so there is no reason to keep the
                // older "reuse everything when activeSurfaceRef is empty"
                // fallback around. Only the real displayed UV polygon needs
                // to match exactly here; requiring unwrapUv to match too is
                // too strict and can throw away a correct active overlay just
                // because the helper unwrap representation was rebuilt
                // differently on selection change. Forced rebuild paths
                // intentionally do not pass previousDisplayIslands, so this
                // only preserves genuinely unchanged active islands.
                island.displayMirrorAxisX = previous.displayMirrorAxisX;
                island.displayOffset = previous.displayOffset;
                island.displayMirrorX = previous.displayMirrorX;
                island.uv = previous.uv;
                ReusedDisplayGeometry reused{};
                reused.uv = true;
                if (sameDisplayedUnwrapStored)
                {
                    island.unwrapUv = previous.unwrapUv;
                    reused.unwrap = true;
                }
                reusedDisplayGeometry.emplace(refKey(island.ref), reused);
            }
            else if (isActiveIsland)
            {
                // When the selection changes, the previously active/root island
                // keeps its prior display basis so the main anchor surface
                // stays on the same repeated texture tile even if we are not
                // reusing the exact previous displayed polygon. Preserving only
                // handedness/offset while rebuilding around a fresh mirror axis
                // can still shift the island sideways onto another tile.
                island.displayMirrorAxisX = previous.displayMirrorAxisX;
                island.displayMirrorX = previous.displayMirrorX;
                island.displayOffset = previous.displayOffset;
            }
            else
            {
                island.displayMirrorX = computeIslandMirrorX(island);
            }
        }
        else
        {
            island.displayMirrorX = computeIslandMirrorX(island);
        }
    }

    for (UvIslandState& island : result.islands)
    {
        const auto reusedIt = reusedDisplayGeometry.find(refKey(island.ref));
        const bool reusedUv = reusedIt != reusedDisplayGeometry.end() && reusedIt->second.uv;
        const bool reusedUnwrap = reusedIt != reusedDisplayGeometry.end() && reusedIt->second.unwrap;

        if (!island.displayMirrorX && std::abs(island.displayOffset.x()) <= sed::math::kTinyEpsilon && std::abs(island.displayOffset.y()) <= sed::math::kTinyEpsilon)
        {
            continue;
        }

        if (!reusedUv)
        {
            for (QPointF& p : island.uv)
            {
                p = islandDisplayFromStoredUv(island, p);
            }
        }
        if (!reusedUnwrap)
        {
            for (QPointF& p : island.unwrapUv)
            {
                p = islandDisplayFromStoredUv(island, p);
            }
        }
    }

    QString commonMaterial{};
    QString commonCmp{};
    bool sameMaterial = true;
    bool sameCmp = true;
    bool haveCommonCmp = false;
    for (const UvIslandState& island : result.islands)
    {
        if (island.material.trimmed().isEmpty())
        {
            sameMaterial = false;
            break;
        }

        if (commonMaterial.isEmpty())
        {
            commonMaterial = island.material.trimmed();
        }
        else if (commonMaterial.compare(island.material.trimmed(), Qt::CaseInsensitive) != 0)
        {
            sameMaterial = false;
            break;
        }

        const QString islandCmp = QString::fromStdString(uvSurfaceColormapName(level, island.ref)).trimmed();
        if (!haveCommonCmp)
        {
            commonCmp = islandCmp;
            haveCommonCmp = true;
        }
        else if (commonCmp.compare(islandCmp, Qt::CaseInsensitive) != 0)
        {
            sameCmp = false;
        }
    }
    if (sameMaterial && !commonMaterial.isEmpty())
    {
        QString previewCmp = commonCmp;
        if (!sameCmp && !result.islands.empty())
        {
            previewCmp = QString::fromStdString(uvSurfaceColormapName(level, result.islands.front().ref));
        }

        if (const std::optional<QImage> loaded = loadUvMaterialImage(resources, commonMaterial, previewCmp.toStdString()))
        {
            result.previewTexture = *loaded;
        }
    }

    return result;
}

bool computeSmartUvProject(
    const Level& level,
    std::vector<UvIslandState>& islands,
    const SmartUvProjectOptions& options,
    const QSize& previewTextureSize,
    const std::optional<SurfaceRef>& activeSurfaceRef = std::nullopt)
{
    // Blender uses the active face's image aspect for the entire operation.
    // Keep one coherent projection basis when selected surfaces use different
    // materials instead of applying incompatible per-face aspect transforms.
    QSize aspectTextureSize = previewTextureSize;
    if (activeSurfaceRef)
    {
        const auto active = std::find_if(islands.begin(), islands.end(), [activeSurfaceRef](const UvIslandState& island)
            {
                return island.ref.sector == activeSurfaceRef->sector
                    && island.ref.surface == activeSurfaceRef->surface;
            });
        if (active != islands.end())
        {
            aspectTextureSize = active->textureSize;
        }
    }

    return smartUvProject(level, islands, options, aspectTextureSize);
}

constexpr qint64 kUvEditorWorkspaceStateVersion = 1;
constexpr std::uint64_t kMaximumUvEditorWorkspaceStateBytes = 16u * 1024u * 1024u;
constexpr qreal kUvEditorZoomDefault = 48.0;
constexpr qreal kUvEditorZoomMinimum = 8.0;
constexpr qreal kUvEditorZoomMaximumDefault = 48'000.0;
constexpr qreal kUvEditorZoomPreferenceMinimum = 480.0;
constexpr qreal kUvEditorZoomPreferenceMaximum = 480'000.0;

qreal uvEditorZoomMaximum()
{
    const QVariant value = settingsStore().value(
        QStringLiteral("uv_editor/max_zoom"),
        static_cast<double>(kUvEditorZoomMaximumDefault));
    bool ok = false;
    const double configured = value.toDouble(&ok);
    if (!ok || !std::isfinite(configured))
    {
        return kUvEditorZoomMaximumDefault;
    }
    return std::clamp(
        static_cast<qreal>(configured),
        kUvEditorZoomPreferenceMinimum,
        kUvEditorZoomPreferenceMaximum);
}

struct UvEditorWorkspaceIslandState
{
    SurfaceRef ref{};
    bool faceSelected{};
    std::vector<bool> vertexSelected{};
    std::vector<bool> edgeSelected{};
    qreal displayMirrorAxisX{};
    QPointF displayOffset{};
    bool displayMirrorX{};
};

struct UvEditorWorkspaceState
{
    qreal zoom{kUvEditorZoomDefault};
    QPointF pan{320.0, 240.0};
    bool viewMirrorX{};
    qreal viewRotationRadians{};
    std::optional<SurfaceRef> activeSurface{};
    std::vector<UvEditorWorkspaceIslandState> islands{};
};

QByteArray uvSelectionMaskToBytes(const std::vector<bool>& mask)
{
    QByteArray bytes{};
    bytes.resize(static_cast<qsizetype>(mask.size()));
    for (std::size_t i = 0; i < mask.size(); ++i)
    {
        bytes[static_cast<qsizetype>(i)] = mask[i] ? '\1' : '\0';
    }
    return bytes;
}

bool uvSelectionMaskFromBytes(
    const QCborValue& value,
    const std::size_t expectedSize,
    std::vector<bool>* out)
{
    if (!out || !value.isByteArray())
    {
        return false;
    }

    const QByteArray bytes = value.toByteArray();
    if (bytes.size() != static_cast<qsizetype>(expectedSize))
    {
        return false;
    }

    out->assign(expectedSize, false);
    for (std::size_t i = 0; i < expectedSize; ++i)
    {
        const unsigned char byte = static_cast<unsigned char>(
            bytes.at(static_cast<qsizetype>(i)));
        if (byte > 1u)
        {
            return false;
        }
        (*out)[i] = byte != 0u;
    }
    return true;
}

bool uvWorkspaceNumber(
    const QCborValue& value,
    const double minimum,
    const double maximum,
    qreal* out)
{
    if (!out || (!value.isDouble() && !value.isInteger()))
    {
        return false;
    }

    const double recovered = value.isDouble()
        ? value.toDouble(std::numeric_limits<double>::quiet_NaN())
        : static_cast<double>(value.toInteger());
    if (!std::isfinite(recovered) || recovered < minimum || recovered > maximum)
    {
        return false;
    }
    *out = static_cast<qreal>(recovered);
    return true;
}

QByteArray serializeUvEditorWorkspaceState(const UvEditorWorkspaceState& state)
{
    QCborArray islands{};
    for (const UvEditorWorkspaceIslandState& island : state.islands)
    {
        islands.append(QCborArray{
            island.ref.sector,
            island.ref.surface,
            island.faceSelected,
            uvSelectionMaskToBytes(island.vertexSelected),
            uvSelectionMaskToBytes(island.edgeSelected),
            island.displayMirrorAxisX,
            island.displayOffset.x(),
            island.displayOffset.y(),
            island.displayMirrorX,
        });
    }

    const SurfaceRef active = state.activeSurface.value_or(SurfaceRef{-1, -1});
    QCborArray view{};
    view.append(state.zoom);
    view.append(state.pan.x());
    view.append(state.pan.y());
    view.append(state.viewMirrorX);
    view.append(state.viewRotationRadians);
    QCborMap root{};
    root.insert(QStringLiteral("version"), kUvEditorWorkspaceStateVersion);
    root.insert(QStringLiteral("view"), view);
    root.insert(QStringLiteral("active"), QCborArray{active.sector, active.surface});
    root.insert(QStringLiteral("islands"), islands);
    return ui::recovery::serializeCanonicalCbor(QCborValue(root));
}

std::optional<UvEditorWorkspaceState> parseUvEditorWorkspaceState(
    const QByteArray& payload,
    const Level& level,
    const editor::Selection& documentSelection,
    const bool recoveredSurfaceEditorActive,
    const int recoveredPickMode,
    const bool recoveredViewportFacing,
    QString* error)
{
    const auto fail = [error](const QString& text) -> std::optional<UvEditorWorkspaceState>
    {
        if (error)
        {
            *error = text;
        }
        return std::nullopt;
    };

    const ui::recovery::RecoveryCborLimits limits{
        .maximumInputBytes = kMaximumUvEditorWorkspaceStateBytes,
        .maximumNodes = 2'000'000,
        .maximumDepth = 8,
        .maximumTextBytes = 1'024,
        .maximumByteStringBytes = 2'000'000,
        .maximumMaterializedBytes = 64u * 1024u * 1024u,
    };
    QString cborError{};
    const std::optional<QCborValue> decoded = ui::recovery::deserializeCanonicalCbor(
        payload, limits, &cborError);
    if (!decoded || !decoded->isMap())
    {
        return fail(cborError.isEmpty()
            ? QStringLiteral("UV editor state is not a canonical CBOR map.")
            : QStringLiteral("UV editor state %1.").arg(cborError));
    }

    const QCborMap root = decoded->toMap();
    if (root.size() != 4
        || root.value(QStringLiteral("version")).toInteger(-1)
            != kUvEditorWorkspaceStateVersion
        || !root.value(QStringLiteral("view")).isArray()
        || !root.value(QStringLiteral("active")).isArray()
        || !root.value(QStringLiteral("islands")).isArray())
    {
        return fail(QStringLiteral("UV editor state has an unsupported schema."));
    }

    const QCborArray view = root.value(QStringLiteral("view")).toArray();
    const QCborArray active = root.value(QStringLiteral("active")).toArray();
    const QCborArray islands = root.value(QStringLiteral("islands")).toArray();
    UvEditorWorkspaceState recovered{};
    qreal panX = 0.0;
    qreal panY = 0.0;
    if (view.size() != 5
        || !uvWorkspaceNumber(view.at(0), kUvEditorZoomMinimum,
            kUvEditorZoomPreferenceMaximum, &recovered.zoom)
        || !uvWorkspaceNumber(view.at(1), -1.0e9, 1.0e9, &panX)
        || !uvWorkspaceNumber(view.at(2), -1.0e9, 1.0e9, &panY)
        || !view.at(3).isBool()
        || !uvWorkspaceNumber(view.at(4), -1.0e9, 1.0e9,
            &recovered.viewRotationRadians))
    {
        return fail(QStringLiteral("UV editor view state is invalid."));
    }
    recovered.pan = QPointF(panX, panY);
    recovered.zoom = std::min(recovered.zoom, uvEditorZoomMaximum());
    recovered.viewMirrorX = view.at(3).toBool();
    if (!recoveredViewportFacing
        && (recovered.viewMirrorX
            || std::abs(recovered.viewRotationRadians) > 1.0e-12))
    {
        return fail(QStringLiteral(
            "UV editor View Face basis is active while View Face is disabled."));
    }

    if (active.size() != 2
        || !active.at(0).isInteger() || !active.at(1).isInteger()
        || active.at(0).toInteger() < -1
        || active.at(0).toInteger() > std::numeric_limits<std::int32_t>::max()
        || active.at(1).toInteger() < -1
        || active.at(1).toInteger() > std::numeric_limits<std::int32_t>::max()
        || ((active.at(0).toInteger() == -1) != (active.at(1).toInteger() == -1)))
    {
        return fail(QStringLiteral("UV editor active-surface state is invalid."));
    }
    if (active.at(0).toInteger() >= 0)
    {
        recovered.activeSurface = SurfaceRef{
            static_cast<std::int32_t>(active.at(0).toInteger()),
            static_cast<std::int32_t>(active.at(1).toInteger()) };
    }

    if (islands.size() > 2'000'000)
    {
        return fail(QStringLiteral("UV editor selection has too many islands."));
    }
    if (!islands.isEmpty()
        && (!recoveredSurfaceEditorActive
            || !documentSelection.sectors.empty()
            || !documentSelection.things.empty()
            || !documentSelection.lights.empty()
            || !documentSelection.cogs.empty()
            || islands.size() != static_cast<qsizetype>(documentSelection.surfaces.size())))
    {
        return fail(QStringLiteral(
            "UV editor islands do not match the recovered Surface-mode selection."));
    }

    recovered.islands.reserve(static_cast<std::size_t>(islands.size()));
    std::optional<SurfaceRef> previousRef{};
    bool activeFound = !recovered.activeSurface.has_value();
    bool activeSelected = !recovered.activeSurface.has_value();
    for (const QCborValue& islandValue : islands)
    {
        if (!islandValue.isArray())
        {
            return fail(QStringLiteral("UV editor island state is malformed."));
        }
        const QCborArray island = islandValue.toArray();
        if (island.size() != 9
            || !island.at(0).isInteger() || !island.at(1).isInteger()
            || island.at(0).toInteger() < 0
            || island.at(0).toInteger() > std::numeric_limits<std::int32_t>::max()
            || island.at(1).toInteger() < 0
            || island.at(1).toInteger() > std::numeric_limits<std::int32_t>::max()
            || !island.at(2).isBool() || !island.at(8).isBool())
        {
            return fail(QStringLiteral("UV editor island state is malformed."));
        }

        UvEditorWorkspaceIslandState recoveredIsland{};
        recoveredIsland.ref = SurfaceRef{
            static_cast<std::int32_t>(island.at(0).toInteger()),
            static_cast<std::int32_t>(island.at(1).toInteger()) };
        if (recoveredIsland.ref.sector < 0
            || static_cast<std::size_t>(recoveredIsland.ref.sector) >= level.sectors.size())
        {
            return fail(QStringLiteral("UV editor state references a missing Sector."));
        }
        const Sector& sector = level.sectors[
            static_cast<std::size_t>(recoveredIsland.ref.sector)];
        if (recoveredIsland.ref.surface < 0
            || static_cast<std::size_t>(recoveredIsland.ref.surface) >= sector.surfaces.size())
        {
            return fail(QStringLiteral("UV editor state references a missing Surface."));
        }
        if (!documentSelection.surfaces.contains(recoveredIsland.ref))
        {
            return fail(QStringLiteral(
                "UV editor state references a Surface outside the recovered selection."));
        }
        if (previousRef
            && (recoveredIsland.ref.sector < previousRef->sector
                || (recoveredIsland.ref.sector == previousRef->sector
                    && recoveredIsland.ref.surface <= previousRef->surface)))
        {
            return fail(QStringLiteral(
                "UV editor island records are not in canonical Surface order."));
        }
        previousRef = recoveredIsland.ref;

        const std::size_t vertexCount = sector.surfaces[
            static_cast<std::size_t>(recoveredIsland.ref.surface)].vertices.size();
        qreal displayOffsetX = 0.0;
        qreal displayOffsetY = 0.0;
        if (!uvSelectionMaskFromBytes(
                island.at(3), vertexCount, &recoveredIsland.vertexSelected)
            || !uvSelectionMaskFromBytes(
                island.at(4), vertexCount, &recoveredIsland.edgeSelected)
            || !uvWorkspaceNumber(island.at(5), -1.0e12, 1.0e12,
                &recoveredIsland.displayMirrorAxisX)
            || !uvWorkspaceNumber(island.at(6), -1.0e12, 1.0e12,
                &displayOffsetX)
            || !uvWorkspaceNumber(island.at(7), -1.0e12, 1.0e12,
                &displayOffsetY))
        {
            return fail(QStringLiteral(
                "UV editor component selection or display alignment is invalid."));
        }
        recoveredIsland.displayOffset = QPointF(displayOffsetX, displayOffsetY);
        recoveredIsland.faceSelected = island.at(2).toBool();
        recoveredIsland.displayMirrorX = island.at(8).toBool();

        const bool selected = recoveredPickMode == 0
            ? recoveredIsland.faceSelected
            : recoveredPickMode == 1
                ? std::any_of(
                    recoveredIsland.edgeSelected.begin(),
                    recoveredIsland.edgeSelected.end(),
                    [](const bool value) { return value; })
                : std::any_of(
                    recoveredIsland.vertexSelected.begin(),
                    recoveredIsland.vertexSelected.end(),
                    [](const bool value) { return value; });
        const bool anyVertex = std::any_of(
            recoveredIsland.vertexSelected.begin(),
            recoveredIsland.vertexSelected.end(),
            [](const bool value) { return value; });
        const bool anyEdge = std::any_of(
            recoveredIsland.edgeSelected.begin(),
            recoveredIsland.edgeSelected.end(),
            [](const bool value) { return value; });
        if ((recoveredPickMode == 0 && (anyVertex || anyEdge))
            || (recoveredPickMode != 0
                && recoveredIsland.faceSelected != selected))
        {
            return fail(QStringLiteral(
                "UV editor selection masks are not canonical for the recovered submode."));
        }
        if (recovered.activeSurface
            && recoveredIsland.ref.sector == recovered.activeSurface->sector
            && recoveredIsland.ref.surface == recovered.activeSurface->surface)
        {
            activeFound = true;
            activeSelected = selected;
        }
        recovered.islands.push_back(std::move(recoveredIsland));
    }

    if (!activeFound || !activeSelected
        || (recovered.islands.empty() && recovered.activeSurface))
    {
        return fail(QStringLiteral(
            "UV editor active Surface is not part of its settled selection."));
    }
    return recovered;
}

bool viewportFlyOwnsUvHover(const QWidget& canvas)
{
    // Resolve the canvas's own viewport, including every UV docking position.
    // Direct children exclude the separate resource/template preview views.
    for (const QWidget* owner = canvas.parentWidget(); owner; owner = owner->parentWidget())
    {
        const auto* const splitter = qobject_cast<const QSplitter*>(owner);
        if (splitter && splitter->objectName() == QStringLiteral("viewportUvSplitter"))
        {
            const auto* const viewport = splitter->findChild<GlView*>(
                QString{}, Qt::FindDirectChildrenOnly);
            return viewport && viewport->flyNavigationActive();
        }
    }
    return false;
}

class UvCanvas final : public QWidget
{
public:
    enum class PickMode : std::uint8_t
    {
        face,
        edge,
        vertex,
    };

    enum class ModalTransformMode : std::uint8_t
    {
        none,
        grab,
        grabSlide,
        rotate,
        rotateTrackball,
        scale,
    };

    enum class SlideAxis : std::uint8_t
    {
        none,
        x,
        y,
    };

    struct ComponentRef
    {
        SurfaceRef surface{};
        std::size_t component{};

        bool operator==(const ComponentRef& other) const noexcept
        {
            return surface.sector == other.surface.sector
                && surface.surface == other.surface.surface
                && component == other.component;
        }
    };

    struct PickCandidate
    {
        std::size_t island{};
        std::size_t component{};

        bool operator==(const PickCandidate&) const noexcept = default;
    };

    explicit UvCanvas(QWidget* parent = nullptr)
        : QWidget(parent)
    {
        setObjectName("uvEditorCanvas");
        ui::onUiNumericFormatChanged(this, [this]() { update(); });
        setMouseTracking(true);
        setFocusPolicy(Qt::StrongFocus);
        setMinimumSize(640, 420);
        QSettings settings = settingsStore();
        zoomMaximum_ = uvEditorZoomMaximum();
        const render::ViewportColorThemeSelection colorTheme =
            render::resolveViewportColorTheme(settings);
        setProperty("sed.uvCanvasBackgroundColor", colorTheme.palette.uvCanvasBackground);
        setProperty("sed.uvGridMainColor", colorTheme.palette.uvGridMain);
        setProperty("sed.uvGridSubColor", colorTheme.palette.uvGridSub);
        setProperty("sed.uvGridAxisUColor", colorTheme.palette.uvGridAxisU);
        setProperty("sed.uvGridAxisVColor", colorTheme.palette.uvGridAxisV);
        setGridStep(0.25);
        setSubGridEnabled(false);
        setRepeatTileGridEnabled(false);
    }

    void setGridStep(const qreal step)
    {
        gridStep_ = std::clamp(step, 0.01, 16.0);
        setProperty("sed.uvGridStep", gridStep_);
        setProperty("sed.uvGridSnapStep", gridStep_);
        setProperty("sed.uvSubGridStep", gridStep_ / 4.0);
        update();
    }

    void setSubGridEnabled(const bool enabled)
    {
        subGridEnabled_ = enabled;
        setProperty("sed.uvSubGridEnabled", subGridEnabled_);
        update();
    }

    void setSubGridMinorDots(const bool enabled)
    {
        subGridMinorDots_ = enabled;
        setProperty("sed.uvGridMinorDots", subGridMinorDots_);
        update();
    }

    void setSubGridDotSizePx(const qreal diameter)
    {
        if ( !std::isfinite(diameter) )
        {
            return;
        }
        subGridDotSizePx_ = std::clamp(diameter, 0.5, 10.0);
        setProperty("sed.uvGridMinorDotSizePx", subGridDotSizePx_);
        update();
    }

    void setRepeatTileGridEnabled(const bool enabled)
    {
        repeatTileGridEnabled_ = enabled;
        setProperty("sed.uvRepeatTileGridEnabled", repeatTileGridEnabled_);
        update();
    }

    void publishViewProperties()
    {
        setProperty("sed.uvZoom", zoom_);
        setProperty("sed.uvPanX", pan_.x());
        setProperty("sed.uvPanY", pan_.y());
    }

    void clampZoomToConfiguredMaximum()
    {
        zoomMaximum_ = uvEditorZoomMaximum();
        const QPointF anchor = uvFromScreen(QPointF(width() * 0.5, height() * 0.5));
        const qreal clamped = std::clamp(
            zoom_, kUvEditorZoomMinimum, zoomMaximum_);
        if (clamped == zoom_)
        {
            return;
        }
        zoom_ = clamped;
        pan_ = uvViewPanForAnchor(
            anchor, QPointF(width() * 0.5, height() * 0.5), zoom_,
            viewMirrorX_, viewRotationRadians_);
        publishViewProperties();
        update();
        notifySettledWorkspaceChanged();
    }

    UvEditorWorkspaceState recoveryWorkspaceState() const
    {
        UvEditorWorkspaceState state{};
        state.zoom = zoom_;
        state.pan = pan_;
        state.viewMirrorX = viewMirrorX_;
        state.viewRotationRadians = viewRotationRadians_;
        state.activeSurface = activeIslandRef();
        state.islands.reserve(islands_.size());
        for (std::size_t islandIndex = 0; islandIndex < islands_.size(); ++islandIndex)
        {
            const UvIslandState& island = islands_[islandIndex];
            UvEditorWorkspaceIslandState recoveredIsland{};
            recoveredIsland.ref = island.ref;
            recoveredIsland.faceSelected = island.selected;
            recoveredIsland.vertexSelected.assign(island.uv.size(), false);
            recoveredIsland.edgeSelected.assign(island.uv.size(), false);
            if (islandIndex < vertexSelected_.size())
            {
                const std::vector<bool>& selected = vertexSelected_[islandIndex];
                const std::size_t count = std::min(
                    selected.size(), recoveredIsland.vertexSelected.size());
                for (std::size_t vertexIndex = 0; vertexIndex < count; ++vertexIndex)
                {
                    recoveredIsland.vertexSelected[vertexIndex] = selected[vertexIndex];
                }
            }
            if (islandIndex < edgeSelected_.size())
            {
                const std::vector<bool>& selected = edgeSelected_[islandIndex];
                const std::size_t count = std::min(
                    selected.size(), recoveredIsland.edgeSelected.size());
                for (std::size_t edgeIndex = 0; edgeIndex < count; ++edgeIndex)
                {
                    recoveredIsland.edgeSelected[edgeIndex] = selected[edgeIndex];
                }
            }
            recoveredIsland.displayMirrorAxisX = island.displayMirrorAxisX;
            recoveredIsland.displayOffset = island.displayOffset;
            recoveredIsland.displayMirrorX = island.displayMirrorX;
            state.islands.push_back(std::move(recoveredIsland));
        }
        return state;
    }

    bool applyRecoveryWorkspaceState(const UvEditorWorkspaceState& state)
    {
        if (state.islands.size() != islands_.size())
        {
            return false;
        }

        if (modalTransformActive_)
        {
            cancelModalTransform();
        }
        clearPointerInteractionState();
        fitToContentPending_ = false;

        for (std::size_t islandIndex = 0; islandIndex < islands_.size(); ++islandIndex)
        {
            UvIslandState& island = islands_[islandIndex];
            const UvEditorWorkspaceIslandState& recovered = state.islands[islandIndex];
            if (island.ref.sector != recovered.ref.sector
                || island.ref.surface != recovered.ref.surface
                || island.uv.size() != recovered.vertexSelected.size()
                || island.uv.size() != recovered.edgeSelected.size())
            {
                return false;
            }

            std::vector<QPointF> storedUv{};
            storedUv.reserve(island.uv.size());
            for (const QPointF& uv : island.uv)
            {
                storedUv.push_back(islandStoredFromDisplayUv(island, uv));
            }
            std::vector<QPointF> storedUnwrap{};
            storedUnwrap.reserve(island.unwrapUv.size());
            for (const QPointF& uv : island.unwrapUv)
            {
                storedUnwrap.push_back(islandStoredFromDisplayUv(island, uv));
            }

            island.displayMirrorAxisX = recovered.displayMirrorAxisX;
            island.displayOffset = recovered.displayOffset;
            island.displayMirrorX = recovered.displayMirrorX;
            for (std::size_t pointIndex = 0; pointIndex < island.uv.size(); ++pointIndex)
            {
                island.uv[pointIndex] = islandDisplayFromStoredUv(
                    island, storedUv[pointIndex]);
            }
            for (std::size_t pointIndex = 0; pointIndex < island.unwrapUv.size(); ++pointIndex)
            {
                island.unwrapUv[pointIndex] = islandDisplayFromStoredUv(
                    island, storedUnwrap[pointIndex]);
            }
            island.selected = recovered.faceSelected;
        }

        syncComponentStorage();
        for (std::size_t islandIndex = 0; islandIndex < islands_.size(); ++islandIndex)
        {
            vertexSelected_[islandIndex] = state.islands[islandIndex].vertexSelected;
            edgeSelected_[islandIndex] = state.islands[islandIndex].edgeSelected;
            if (pickMode_ != PickMode::face)
            {
                syncIslandSelectionFromComponents(islandIndex);
            }
        }
        faceModeCachedVertexSelected_.clear();
        faceModeCachedEdgeSelected_.clear();
        faceModeCachedSelectedVertices_.clear();
        faceModeHasComponentCache_ = false;
        componentModeCachedVertexSelected_.clear();
        componentModeCachedEdgeSelected_.clear();
        componentModeCachedSelectedVertices_.clear();
        componentModeHasCache_ = false;

        activeIslandIndex_.reset();
        if (state.activeSurface)
        {
            for (std::size_t islandIndex = 0; islandIndex < islands_.size(); ++islandIndex)
            {
                if (islands_[islandIndex].ref.sector == state.activeSurface->sector
                    && islands_[islandIndex].ref.surface == state.activeSurface->surface)
                {
                    activeIslandIndex_ = islandIndex;
                    break;
                }
            }
        }
        // Exact component pivots are transient interaction state and are not
        // part of the settled recovery wire contract. Never leak one from the
        // replaced workspace into restored selection masks.
        activeVertex_.reset();
        activeEdge_.reset();
        facePivotVertex_.reset();
        normalizeActiveIsland();
        normalizeActiveComponentMarkers();
        zoom_ = state.zoom;
        pan_ = state.pan;
        viewMirrorX_ = state.viewMirrorX;
        viewRotationRadians_ = state.viewRotationRadians;
        publishViewProperties();
        setProperty("sed.uvViewMirrorX", viewMirrorX_);
        setProperty(
            "sed.uvViewRotationDegrees",
            viewRotationRadians_ * (180.0 / std::numbers::pi_v<qreal>));
        publishStatus();
        update();
        return true;
    }

    void resetRecoveryWorkspaceState()
    {
        if (modalTransformActive_)
        {
            cancelModalTransform();
        }
        clearPointerInteractionState();
        islands_.clear();
        vertexSelected_.clear();
        edgeSelected_.clear();
        activeIslandIndex_.reset();
        activeVertex_.reset();
        activeEdge_.reset();
        facePivotVertex_.reset();
        clearFaceModeComponentCache();
        componentModeCachedVertexSelected_.clear();
        componentModeCachedEdgeSelected_.clear();
        componentModeCachedSelectedVertices_.clear();
        componentModeHasCache_ = false;
        zoom_ = kUvEditorZoomDefault;
        pan_ = QPointF(320.0, 240.0);
        viewportFacingViewEnabled_ = false;
        viewMirrorX_ = false;
        viewRotationRadians_ = 0.0;
        fitToContentPending_ = false;
        publishViewProperties();
        setProperty("sed.uvViewportFacing", false);
        setProperty("sed.uvViewMirrorX", false);
        setProperty("sed.uvViewRotationDegrees", 0.0);
        publishStatus();
        update();
    }

    void setIslands(std::vector<UvIslandState> islands, const bool fitView = true)
    {
        clearPointerInteractionState();

        std::optional<SurfaceRef> previousActiveRef{};
        struct CachedFaceModeSelectionState
        {
            std::vector<bool> vertices{};
            std::vector<bool> edges{};
            std::vector<bool> selectedVertices{};
        };
        struct CachedComponentModeSelectionState
        {
            std::vector<bool> vertices{};
            std::vector<bool> edges{};
            std::vector<bool> selectedVertices{};
        };
        struct PreviousSelectionState
        {
            bool faceSelected{ true };
            std::vector<bool> vertices{};
            std::vector<bool> edges{};
        };
        std::unordered_map<std::uint64_t, PreviousSelectionState> previousSelections{};
        std::unordered_map<std::uint64_t, CachedFaceModeSelectionState> previousFaceModeSelections{};
        std::unordered_map<std::uint64_t, CachedComponentModeSelectionState> previousComponentModeSelections{};
        const auto refKey = [](const SurfaceRef ref) -> std::uint64_t
        {
            return (static_cast<std::uint64_t>(static_cast<std::uint32_t>(ref.sector)) << 32u)
                | static_cast<std::uint32_t>(ref.surface);
        };
        const bool sameIslandRefs =
            islands_.size() == islands.size()
            && std::equal(
                islands_.begin(),
                islands_.end(),
                islands.begin(),
                [](const UvIslandState& lhs, const UvIslandState& rhs)
                {
                    return lhs.ref.sector == rhs.ref.sector
                        && lhs.ref.surface == rhs.ref.surface;
                });
        if (const std::optional<std::size_t> previousActiveIndex = activeSelectedIslandIndex())
        {
            previousActiveRef = islands_[*previousActiveIndex].ref;
        }
        if (!islands_.empty())
        {
            previousSelections.reserve(islands_.size());
            for (std::size_t islandIndex = 0; islandIndex < islands_.size(); ++islandIndex)
            {
                const UvIslandState& island = islands_[islandIndex];
                PreviousSelectionState state{};
                state.faceSelected = islandSelectedInCurrentMode(islandIndex);
                if (islandIndex < vertexSelected_.size())
                {
                    state.vertices = vertexSelected_[islandIndex];
                }
                if (islandIndex < edgeSelected_.size())
                {
                    state.edges = edgeSelected_[islandIndex];
                }
                previousSelections.emplace(
                    refKey(island.ref),
                    std::move(state));
            }
        }
        if (faceModeHasComponentCache_ && sameIslandRefs)
        {
            previousFaceModeSelections.reserve(islands_.size());
            for (std::size_t islandIndex = 0; islandIndex < islands_.size(); ++islandIndex)
            {
                CachedFaceModeSelectionState state{};
                if (islandIndex < faceModeCachedVertexSelected_.size())
                {
                    state.vertices = faceModeCachedVertexSelected_[islandIndex];
                }
                if (islandIndex < faceModeCachedEdgeSelected_.size())
                {
                    state.edges = faceModeCachedEdgeSelected_[islandIndex];
                }
                if (islandIndex < faceModeCachedSelectedVertices_.size())
                {
                    state.selectedVertices = faceModeCachedSelectedVertices_[islandIndex];
                }
                previousFaceModeSelections.emplace(refKey(islands_[islandIndex].ref), std::move(state));
            }
        }
        if (componentModeHasCache_ && sameIslandRefs)
        {
            previousComponentModeSelections.reserve(islands_.size());
            for (std::size_t islandIndex = 0; islandIndex < islands_.size(); ++islandIndex)
            {
                CachedComponentModeSelectionState state{};
                if (islandIndex < componentModeCachedVertexSelected_.size())
                {
                    state.vertices = componentModeCachedVertexSelected_[islandIndex];
                }
                if (islandIndex < componentModeCachedEdgeSelected_.size())
                {
                    state.edges = componentModeCachedEdgeSelected_[islandIndex];
                }
                if (islandIndex < componentModeCachedSelectedVertices_.size())
                {
                    state.selectedVertices = componentModeCachedSelectedVertices_[islandIndex];
                }
                previousComponentModeSelections.emplace(refKey(islands_[islandIndex].ref), std::move(state));
            }
        }

        islands_ = std::move(islands);
        faceModeCachedVertexSelected_.clear();
        faceModeCachedEdgeSelected_.clear();
        faceModeCachedSelectedVertices_.clear();
        faceModeHasComponentCache_ = false;
        componentModeCachedVertexSelected_.clear();
        componentModeCachedEdgeSelected_.clear();
        componentModeCachedSelectedVertices_.clear();
        componentModeHasCache_ = false;
        if (!previousFaceModeSelections.empty())
        {
            faceModeCachedVertexSelected_.resize(islands_.size());
            faceModeCachedEdgeSelected_.resize(islands_.size());
            faceModeCachedSelectedVertices_.resize(islands_.size());
            for (std::size_t islandIndex = 0; islandIndex < islands_.size(); ++islandIndex)
            {
                const auto it = previousFaceModeSelections.find(refKey(islands_[islandIndex].ref));
                if (it == previousFaceModeSelections.end())
                {
                    continue;
                }

                const std::size_t islandVertexCount = islands_[islandIndex].uv.size();
                const bool topologyMatches =
                    it->second.vertices.size() == islandVertexCount
                    && it->second.edges.size() == islandVertexCount
                    && it->second.selectedVertices.size() == islandVertexCount;
                if (!topologyMatches)
                {
                    continue;
                }

                faceModeCachedVertexSelected_[islandIndex] = it->second.vertices;
                faceModeCachedEdgeSelected_[islandIndex] = it->second.edges;
                faceModeCachedSelectedVertices_[islandIndex] = it->second.selectedVertices;
                faceModeHasComponentCache_ = true;
            }
        }
        if (!previousComponentModeSelections.empty())
        {
            componentModeCachedVertexSelected_.resize(islands_.size());
            componentModeCachedEdgeSelected_.resize(islands_.size());
            componentModeCachedSelectedVertices_.resize(islands_.size());
            for (std::size_t islandIndex = 0; islandIndex < islands_.size(); ++islandIndex)
            {
                const auto it = previousComponentModeSelections.find(refKey(islands_[islandIndex].ref));
                if (it == previousComponentModeSelections.end())
                {
                    continue;
                }

                const std::size_t islandVertexCount = islands_[islandIndex].uv.size();
                const bool topologyMatches =
                    it->second.vertices.size() == islandVertexCount
                    && it->second.edges.size() == islandVertexCount
                    && it->second.selectedVertices.size() == islandVertexCount;
                if (!topologyMatches)
                {
                    continue;
                }

                componentModeCachedVertexSelected_[islandIndex] = it->second.vertices;
                componentModeCachedEdgeSelected_[islandIndex] = it->second.edges;
                componentModeCachedSelectedVertices_[islandIndex] = it->second.selectedVertices;
                componentModeHasCache_ = true;
            }
        }
        syncComponentStorage();
        clearComponentSelection();
        activeIslandIndex_.reset();
        auto seedFullComponentSelectionForCurrentMode = [this](const std::size_t islandIndex)
        {
            ensureComponentStorage(islandIndex);
            if (pickMode_ == PickMode::vertex)
            {
                std::fill(vertexSelected_[islandIndex].begin(), vertexSelected_[islandIndex].end(), true);
            }
            else
            {
                std::fill(edgeSelected_[islandIndex].begin(), edgeSelected_[islandIndex].end(), true);
            }
        };
        if (!previousSelections.empty())
        {
            for (std::size_t islandIndex = 0; islandIndex < islands_.size(); ++islandIndex)
            {
                UvIslandState& island = islands_[islandIndex];
                const auto it = previousSelections.find(refKey(island.ref));
                if (it == previousSelections.end())
                {
                    if (pickMode_ != PickMode::face && island.selected)
                    {
                        seedFullComponentSelectionForCurrentMode(islandIndex);
                        syncIslandSelectionFromComponents(islandIndex);
                    }
                    continue;
                }

                island.selected = it->second.faceSelected;

                const bool topologyMatches =
                    it->second.vertices.size() == island.uv.size()
                    && it->second.edges.size() == island.uv.size();

                if (topologyMatches && islandIndex < vertexSelected_.size())
                {
                    vertexSelected_[islandIndex].assign(island.uv.size(), false);
                    for (std::size_t i = 0; i < vertexSelected_[islandIndex].size(); ++i)
                    {
                        vertexSelected_[islandIndex][i] = it->second.vertices[i];
                    }
                }
                if (topologyMatches && islandIndex < edgeSelected_.size())
                {
                    edgeSelected_[islandIndex].assign(island.uv.size(), false);
                    for (std::size_t i = 0; i < edgeSelected_[islandIndex].size(); ++i)
                    {
                        edgeSelected_[islandIndex][i] = it->second.edges[i];
                    }
                }

                if (pickMode_ != PickMode::face)
                {
                    bool haveComponentSelection = false;
                    if (pickMode_ == PickMode::vertex && islandIndex < vertexSelected_.size())
                    {
                        haveComponentSelection = std::any_of(
                            vertexSelected_[islandIndex].begin(),
                            vertexSelected_[islandIndex].end(),
                            [](const bool value) { return value; });
                    }
                    else if (pickMode_ == PickMode::edge && islandIndex < edgeSelected_.size())
                    {
                        haveComponentSelection = std::any_of(
                            edgeSelected_[islandIndex].begin(),
                            edgeSelected_[islandIndex].end(),
                            [](const bool value) { return value; });
                    }

                    if (island.selected && !haveComponentSelection)
                    {
                        seedFullComponentSelectionForCurrentMode(islandIndex);
                    }

                    syncIslandSelectionFromComponents(islandIndex);
                }
            }
        }
        else if (pickMode_ != PickMode::face)
        {
            for (std::size_t islandIndex = 0; islandIndex < islands_.size(); ++islandIndex)
            {
                if (!islands_[islandIndex].selected)
                {
                    continue;
                }

                seedFullComponentSelectionForCurrentMode(islandIndex);
                syncIslandSelectionFromComponents(islandIndex);
            }
        }
        if (previousActiveRef)
        {
            for (std::size_t i = 0; i < islands_.size(); ++i)
            {
                if (islands_[i].ref.sector == previousActiveRef->sector
                    && islands_[i].ref.surface == previousActiveRef->surface
                    && islandSelectedInCurrentMode(i))
                {
                    activeIslandIndex_ = i;
                    break;
                }
            }
        }
        normalizeActiveIsland();
        normalizeActiveComponentMarkers();
        fitToContentPending_ = fitView;
        if (fitView)
        {
            QTimer::singleShot(0, this, [this]()
                {
                    consumePendingFitToContent();
                });
        }
        publishStatus();
        update();
    }

    const std::vector<UvIslandState>& islands() const noexcept
    {
        return islands_;
    }

    void setPreviewTexture(QImage texture)
    {
        previewTexture_ = std::move(texture);
        update();
    }

    void setViewportFacingView(
        const bool enabled,
        const bool mirrorX,
        const qreal rotationRadians)
    {
        const bool nextMirrorX = enabled && mirrorX;
        const qreal nextRotationRadians = enabled ? rotationRadians : 0.0;
        if (viewportFacingViewEnabled_ == enabled
            && viewMirrorX_ == nextMirrorX
            && qFuzzyCompare(viewRotationRadians_ + 1.0, nextRotationRadians + 1.0))
        {
            return;
        }

        const QPointF screenAnchor(
            static_cast<qreal>(width()) * 0.5,
            static_cast<qreal>(height()) * 0.5);
        const QPointF uvAnchor = uvFromScreen(screenAnchor);
        resetPickCycle();
        viewportFacingViewEnabled_ = enabled;
        viewMirrorX_ = nextMirrorX;
        viewRotationRadians_ = nextRotationRadians;
        pan_ = uvViewPanForAnchor(
            uvAnchor,
            screenAnchor,
            zoom_,
            viewMirrorX_,
            viewRotationRadians_);
        publishViewProperties();
        setProperty("sed.uvViewportFacing", viewportFacingViewEnabled_);
        setProperty("sed.uvViewMirrorX", viewMirrorX_);
        setProperty(
            "sed.uvViewRotationDegrees",
            viewRotationRadians_ * (180.0 / std::numbers::pi_v<qreal>));
        publishStatus();
        update();
    }

    bool viewMirrorX() const noexcept
    {
        return viewMirrorX_;
    }

    qreal viewRotationRadians() const noexcept
    {
        return viewRotationRadians_;
    }

    void setRepeatTexturePreviewEnabled(const bool enabled)
    {
        repeatTexturePreviewEnabled_ = enabled;
        update();
    }

    bool repeatTexturePreviewEnabled() const noexcept
    {
        return repeatTexturePreviewEnabled_;
    }

    void setPickMode(const PickMode mode)
    {
        if (pickMode_ == mode)
        {
            return;
        }

        if (modalTransformActive_)
        {
            cancelModalTransform();
        }
        clearPointerInteractionState();

        const PickMode previousMode = pickMode_;
        const std::vector<std::vector<bool>> previousVertexSelected = vertexSelected_;
        const std::vector<std::vector<bool>> previousEdgeSelected = edgeSelected_;
        const std::vector<std::vector<bool>> previousComponentModeCachedVertexSelected = componentModeCachedVertexSelected_;
        const std::vector<std::vector<bool>> previousComponentModeCachedEdgeSelected = componentModeCachedEdgeSelected_;
        const std::vector<std::vector<bool>> previousComponentModeCachedSelectedVertices = componentModeCachedSelectedVertices_;
        const bool previousComponentModeHasCache = componentModeHasCache_;
        auto islandSelectedInMode = [this](const PickMode modeToCheck, const std::size_t islandIndex) -> bool
        {
            if (islandIndex >= islands_.size())
            {
                return false;
            }

            if (modeToCheck == PickMode::face)
            {
                return islands_[islandIndex].selected;
            }

            if (modeToCheck == PickMode::vertex)
            {
                return islandIndex < vertexSelected_.size()
                    && std::any_of(
                        vertexSelected_[islandIndex].begin(),
                        vertexSelected_[islandIndex].end(),
                        [](const bool value) { return value; });
            }

            return islandIndex < edgeSelected_.size()
                && std::any_of(
                    edgeSelected_[islandIndex].begin(),
                    edgeSelected_[islandIndex].end(),
                    [](const bool value) { return value; });
        };
        std::vector<bool> previouslySelected{};
        previouslySelected.reserve(islands_.size());
        std::vector<std::vector<bool>> previouslySelectedVertices{};
        previouslySelectedVertices.reserve(islands_.size());
        for (std::size_t islandIndex = 0; islandIndex < islands_.size(); ++islandIndex)
        {
            previouslySelected.push_back(islandSelectedInMode(previousMode, islandIndex));
            previouslySelectedVertices.push_back(selectedVertexMask(islandIndex));
        }
        pickMode_ = mode;

        syncComponentStorage();
        auto restoreComponentSelectionFromVertexMask = [this, &previouslySelectedVertices](const std::size_t islandIndex)
        {
            if (islandIndex >= islands_.size() || islandIndex >= previouslySelectedVertices.size())
            {
                return;
            }

            ensureComponentStorage(islandIndex);
            const std::vector<bool>& vertexMask = previouslySelectedVertices[islandIndex];
            if (pickMode_ == PickMode::vertex)
            {
                vertexSelected_[islandIndex].assign(islands_[islandIndex].uv.size(), false);
                const std::size_t copyCount = std::min(vertexSelected_[islandIndex].size(), vertexMask.size());
                for (std::size_t vertexIndex = 0; vertexIndex < copyCount; ++vertexIndex)
                {
                    vertexSelected_[islandIndex][vertexIndex] = vertexMask[vertexIndex];
                }
                return;
            }

            if (pickMode_ == PickMode::edge)
            {
                edgeSelected_[islandIndex].assign(islands_[islandIndex].uv.size(), false);
                if (edgeSelected_[islandIndex].empty())
                {
                    return;
                }

                for (std::size_t edgeIndex = 0; edgeIndex < edgeSelected_[islandIndex].size(); ++edgeIndex)
                {
                    const bool startSelected =
                        edgeIndex < vertexMask.size()
                        && vertexMask[edgeIndex];
                    const std::size_t nextVertexIndex = (edgeIndex + 1u) % edgeSelected_[islandIndex].size();
                    const bool endSelected =
                        nextVertexIndex < vertexMask.size()
                        && vertexMask[nextVertexIndex];
                    edgeSelected_[islandIndex][edgeIndex] = startSelected && endSelected;
                }
            }
        };
        auto restoreCachedComponentSelection = [this](const std::size_t islandIndex) -> bool
        {
            if (!faceModeHasComponentCache_ || islandIndex >= islands_.size())
            {
                return false;
            }

            ensureComponentStorage(islandIndex);
            const auto hasAnySelected = [](const std::vector<bool>& values) -> bool
            {
                return std::any_of(values.begin(), values.end(), [](const bool value) { return value; });
            };
            const std::vector<bool>* cachedSelectedVertices = nullptr;
            if (islandIndex < faceModeCachedSelectedVertices_.size())
            {
                cachedSelectedVertices = &faceModeCachedSelectedVertices_[islandIndex];
            }
            bool restoredAny = false;
            if (pickMode_ == PickMode::vertex)
            {
                vertexSelected_[islandIndex].assign(islands_[islandIndex].uv.size(), false);
                if (islandIndex < faceModeCachedVertexSelected_.size())
                {
                    const std::vector<bool>& cached = faceModeCachedVertexSelected_[islandIndex];
                    if (hasAnySelected(cached))
                    {
                        const std::size_t copyCount = std::min(vertexSelected_[islandIndex].size(), cached.size());
                        for (std::size_t vertexIndex = 0; vertexIndex < copyCount; ++vertexIndex)
                        {
                            vertexSelected_[islandIndex][vertexIndex] = cached[vertexIndex];
                            restoredAny = restoredAny || cached[vertexIndex];
                        }
                    }
                }
                if (!restoredAny && cachedSelectedVertices)
                {
                    const std::size_t copyCount = std::min(vertexSelected_[islandIndex].size(), cachedSelectedVertices->size());
                    for (std::size_t vertexIndex = 0; vertexIndex < copyCount; ++vertexIndex)
                    {
                        vertexSelected_[islandIndex][vertexIndex] = (*cachedSelectedVertices)[vertexIndex];
                        restoredAny = restoredAny || (*cachedSelectedVertices)[vertexIndex];
                    }
                }
                return restoredAny;
            }

            if (pickMode_ == PickMode::edge)
            {
                edgeSelected_[islandIndex].assign(islands_[islandIndex].uv.size(), false);
                if (islandIndex < faceModeCachedEdgeSelected_.size())
                {
                    const std::vector<bool>& cached = faceModeCachedEdgeSelected_[islandIndex];
                    if (hasAnySelected(cached))
                    {
                        const std::size_t copyCount = std::min(edgeSelected_[islandIndex].size(), cached.size());
                        for (std::size_t edgeIndex = 0; edgeIndex < copyCount; ++edgeIndex)
                        {
                            edgeSelected_[islandIndex][edgeIndex] = cached[edgeIndex];
                            restoredAny = restoredAny || cached[edgeIndex];
                        }
                    }
                }
                if (!restoredAny && cachedSelectedVertices && !edgeSelected_[islandIndex].empty())
                {
                    for (std::size_t edgeIndex = 0; edgeIndex < edgeSelected_[islandIndex].size(); ++edgeIndex)
                    {
                        const bool startSelected =
                            edgeIndex < cachedSelectedVertices->size()
                            && (*cachedSelectedVertices)[edgeIndex];
                        const std::size_t nextVertexIndex = (edgeIndex + 1u) % edgeSelected_[islandIndex].size();
                        const bool endSelected =
                            nextVertexIndex < cachedSelectedVertices->size()
                            && (*cachedSelectedVertices)[nextVertexIndex];
                        edgeSelected_[islandIndex][edgeIndex] = startSelected && endSelected;
                        restoredAny = restoredAny || edgeSelected_[islandIndex][edgeIndex];
                    }
                }
                return restoredAny;
            }

            return false;
        };
        auto sameVertexMask = [](const std::vector<bool>& lhs, const std::vector<bool>& rhs) -> bool
        {
            if (lhs.size() != rhs.size())
            {
                return false;
            }
            for (std::size_t i = 0; i < lhs.size(); ++i)
            {
                if (lhs[i] != rhs[i])
                {
                    return false;
                }
            }
            return true;
        };
        auto restoreRoundTripComponentSelection = [&](const std::size_t islandIndex) -> bool
        {
            if (!previousComponentModeHasCache
                || islandIndex >= islands_.size()
                || islandIndex >= previouslySelectedVertices.size()
                || islandIndex >= previousComponentModeCachedSelectedVertices.size()
                || !sameVertexMask(previouslySelectedVertices[islandIndex], previousComponentModeCachedSelectedVertices[islandIndex]))
            {
                return false;
            }

            ensureComponentStorage(islandIndex);
            if (pickMode_ == PickMode::vertex)
            {
                if (islandIndex >= previousComponentModeCachedVertexSelected.size())
                {
                    return false;
                }

                vertexSelected_[islandIndex].assign(islands_[islandIndex].uv.size(), false);
                const std::vector<bool>& cached = previousComponentModeCachedVertexSelected[islandIndex];
                const std::size_t copyCount = std::min(vertexSelected_[islandIndex].size(), cached.size());
                bool restoredAny = false;
                for (std::size_t vertexIndex = 0; vertexIndex < copyCount; ++vertexIndex)
                {
                    vertexSelected_[islandIndex][vertexIndex] = cached[vertexIndex];
                    restoredAny = restoredAny || cached[vertexIndex];
                }
                return restoredAny;
            }

            if (pickMode_ == PickMode::edge)
            {
                if (islandIndex >= previousComponentModeCachedEdgeSelected.size())
                {
                    return false;
                }

                edgeSelected_[islandIndex].assign(islands_[islandIndex].uv.size(), false);
                const std::vector<bool>& cached = previousComponentModeCachedEdgeSelected[islandIndex];
                const std::size_t copyCount = std::min(edgeSelected_[islandIndex].size(), cached.size());
                bool restoredAny = false;
                for (std::size_t edgeIndex = 0; edgeIndex < copyCount; ++edgeIndex)
                {
                    edgeSelected_[islandIndex][edgeIndex] = cached[edgeIndex];
                    restoredAny = restoredAny || cached[edgeIndex];
                }
                return restoredAny;
            }

            return false;
        };
        if (previousMode == PickMode::face && pickMode_ != PickMode::face)
        {
            clearComponentSelection();
            for (std::size_t islandIndex = 0; islandIndex < islands_.size(); ++islandIndex)
            {
                if (islandIndex >= previouslySelected.size() || !previouslySelected[islandIndex])
                {
                    continue;
                }

                if (!restoreCachedComponentSelection(islandIndex))
                {
                    restoreComponentSelectionFromVertexMask(islandIndex);
                }
            }
            faceModeCachedVertexSelected_.clear();
            faceModeCachedEdgeSelected_.clear();
            faceModeCachedSelectedVertices_.clear();
            faceModeHasComponentCache_ = false;
        }
        else if (previousMode != PickMode::face && pickMode_ == PickMode::face)
        {
            faceModeCachedVertexSelected_ = vertexSelected_;
            faceModeCachedEdgeSelected_ = edgeSelected_;
            faceModeCachedSelectedVertices_ = previouslySelectedVertices;
            faceModeHasComponentCache_ = true;
            for (std::size_t islandIndex = 0; islandIndex < islands_.size(); ++islandIndex)
            {
                islands_[islandIndex].selected =
                    islandIndex < previouslySelected.size()
                    && previouslySelected[islandIndex];
            }
            clearComponentSelection();
        }
        else if (previousMode != pickMode_)
        {
            clearComponentSelection();
            if (pickMode_ != PickMode::face)
            {
                for (std::size_t islandIndex = 0; islandIndex < islands_.size(); ++islandIndex)
                {
                    if (islandIndex >= previouslySelected.size() || !previouslySelected[islandIndex])
                    {
                        continue;
                    }

                    if (!restoreRoundTripComponentSelection(islandIndex))
                    {
                        restoreComponentSelectionFromVertexMask(islandIndex);
                    }
                }
            }
        }

        if (previousMode != PickMode::face && pickMode_ != PickMode::face && previousMode != pickMode_)
        {
            componentModeCachedVertexSelected_ = previousVertexSelected;
            componentModeCachedEdgeSelected_ = previousEdgeSelected;
            componentModeCachedSelectedVertices_ = previouslySelectedVertices;
            componentModeHasCache_ = true;
        }
        else if (pickMode_ == PickMode::face)
        {
            componentModeCachedVertexSelected_.clear();
            componentModeCachedEdgeSelected_.clear();
            componentModeCachedSelectedVertices_.clear();
            componentModeHasCache_ = false;
        }

        if (pickMode_ != PickMode::face)
        {
            for (std::size_t islandIndex = 0; islandIndex < islands_.size(); ++islandIndex)
            {
                syncIslandSelectionFromComponents(islandIndex);
            }
        }

        normalizeActiveIsland();
        normalizeActiveComponentMarkers();
        publishStatus();
        update();
        notifySettledWorkspaceChanged();
    }

    PickMode pickMode() const noexcept
    {
        return pickMode_;
    }

    std::optional<SurfaceRef> activeIslandRef() const
    {
        if (const std::optional<std::size_t> index = activeSelectedIslandIndex())
        {
            return islands_[*index].ref;
        }
        return std::nullopt;
    }

    std::optional<SurfaceRef> firstSelectedIslandRef() const
    {
        for (std::size_t i = 0; i < islands_.size(); ++i)
        {
            if (islandSelectedInCurrentMode(i))
            {
                return islands_[i].ref;
            }
        }
        return std::nullopt;
    }

    void setActiveIslandRef(const std::optional<SurfaceRef>& ref)
    {
        if (!ref)
        {
            normalizeActiveIsland();
            publishStatus();
            update();
            return;
        }

        for (std::size_t i = 0; i < islands_.size(); ++i)
        {
            const SurfaceRef islandRef = islands_[i].ref;
            if (islandRef.sector == ref->sector && islandRef.surface == ref->surface)
            {
                if (islandSelectedInCurrentMode(i))
                {
                    activeIslandIndex_ = i;
                }
                else if (!activeSelectedIslandIndex())
                {
                    // Do not keep a stale raw active index for an island that
                    // exists but is no longer selected. Leaving that stale
                    // index behind can let later selection normalization
                    // "resurrect" the wrong anchor island after a rebuild.
                    activeIslandIndex_.reset();
                }
                publishStatus();
                update();
                return;
            }
        }

        if (activeSelectedIslandIndex())
        {
            // If the requested ref is stale, keep the current preserved active
            // island instead of silently re-normalizing to the first selected
            // island. This helps selection-change rebuilds keep the same anchor
            // surface when setIslands() has already carried it forward.
            publishStatus();
            update();
            return;
        }

        normalizeActiveIsland();
        publishStatus();
        update();
    }

    void setLiveApplyCallback(std::function<void(const std::vector<UvIslandState>&)> callback)
    {
        onLiveApply_ = std::move(callback);
    }

    void setSettledWorkspaceChangedCallback(std::function<void()> callback)
    {
        onSettledWorkspaceChanged_ = std::move(callback);
    }

    void setStatusTextCallback(std::function<void(const QString&)> callback)
    {
        onStatusText_ = std::move(callback);
        publishStatus();
    }

    void setModalCommitCallback(std::function<void(const std::vector<UvIslandState>&, const std::vector<UvIslandState>&)> callback)
    {
        onModalCommit_ = std::move(callback);
    }

    void previewCurrentState()
    {
        emitLiveApply();
        update();
    }

    std::vector<bool> selectedIslandMask() const
    {
        std::vector<bool> mask{};
        mask.reserve(islands_.size());
        for (std::size_t islandIndex = 0; islandIndex < islands_.size(); ++islandIndex)
        {
            mask.push_back(islandSelectedInCurrentMode(islandIndex));
        }
        return mask;
    }

    bool hasSelection() const
    {
        return hasSelectedIslands();
    }

    void setPersistentSnapEnabled(const bool enabled)
    {
        persistentSnapEnabled_ = enabled;
        publishStatus();
        update();
    }

    bool persistentSnapEnabled() const noexcept
    {
        return persistentSnapEnabled_;
    }

    bool hasActiveModalTransform() const noexcept
    {
        return modalTransformActive_;
    }

    bool handleShortcutKey(const int key, const Qt::KeyboardModifiers modifiers = Qt::NoModifier)
    {
        constexpr Qt::KeyboardModifiers kShortcutBlockingModifiers =
            Qt::ShiftModifier | Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier;
        const bool hasShortcutBlockingModifier = (modifiers & kShortcutBlockingModifiers) != Qt::NoModifier;
        switch (key)
        {
        case Qt::Key_Escape:
            if (hasShortcutBlockingModifier)
            {
                return false;
            }
            if (modalTransformActive_)
            {
                cancelModalTransform();
                return true;
            }
            return false;
        case Qt::Key_Return:
        case Qt::Key_Enter:
            if (hasShortcutBlockingModifier)
            {
                return false;
            }
            if (modalTransformActive_)
            {
                commitModalTransform();
                return true;
            }
            return false;
        case Qt::Key_G:
            if (hasShortcutBlockingModifier)
            {
                return false;
            }
            if (modalTransformActive_ && modalTransformMode_ == ModalTransformMode::grab)
            {
                beginModalTransform(ModalTransformMode::grabSlide);
            }
            else
            {
                beginModalTransform(ModalTransformMode::grab);
            }
            return true;
        case Qt::Key_R:
            if (hasShortcutBlockingModifier)
            {
                return false;
            }
            if (modalTransformActive_ && modalTransformMode_ == ModalTransformMode::rotate)
            {
                beginModalTransform(ModalTransformMode::rotateTrackball);
            }
            else
            {
                beginModalTransform(ModalTransformMode::rotate);
            }
            return true;
        case Qt::Key_S:
            if (hasShortcutBlockingModifier)
            {
                return false;
            }
            beginModalTransform(ModalTransformMode::scale);
            return true;
        case Qt::Key_A:
            if (hasShortcutBlockingModifier)
            {
                return false;
            }
            if (modalTransformActive_)
            {
                return true;
            }
            selectAll();
            return true;
        default:
            return false;
        }
    }

    void commitModalTransform()
    {
        if (!modalTransformActive_)
        {
            return;
        }

        const std::vector<UvIslandState> before =
            modalTransformChainStartIslands_.empty() ? modalTransformStartIslands_ : modalTransformChainStartIslands_;
        const std::vector<UvIslandState> after = islands_;
        modalTransformActive_ = false;
        modalTransformMode_ = ModalTransformMode::none;
        modalSlideAxis_ = SlideAxis::none;
        modalTransformStartIslands_.clear();
        modalTransformChainStartIslands_.clear();
        modalReferenceVertex_.reset();
        modalReferenceEdge_.reset();
        releaseKeyboard();
        updateCanvasCursor();
        publishStatus();
        update();
        if (onModalCommit_)
        {
            if (!uvIslandStatesStoreSameUv(before, after))
            {
                onModalCommit_(before, after);
            }
        }
    }

    void cancelModalTransform()
    {
        if (!modalTransformActive_)
        {
            return;
        }

        islands_ = modalTransformChainStartIslands_.empty() ? modalTransformStartIslands_ : modalTransformChainStartIslands_;
        modalTransformActive_ = false;
        modalTransformMode_ = ModalTransformMode::none;
        modalSlideAxis_ = SlideAxis::none;
        modalTransformStartIslands_.clear();
        modalTransformChainStartIslands_.clear();
        modalReferenceVertex_.reset();
        modalReferenceEdge_.reset();
        releaseKeyboard();
        emitLiveApply();
        updateCanvasCursor();
        publishStatus();
        update();
    }

    void fitToContent(
        const bool selectedOnly,
        const bool notifyRecovery = true)
    {
        if (modalTransformActive_)
        {
            return;
        }

        const QRectF b = selectedOnly ? selectedBounds() : islandsBounds(islands_, false);
        if (!b.isValid() || width() <= 0 || height() <= 0)
        {
            return;
        }

        const std::vector<QPointF> orientedCorners{
            uvViewScreenFromUv(b.topLeft(), {}, 1.0, viewMirrorX_, viewRotationRadians_),
            uvViewScreenFromUv(b.topRight(), {}, 1.0, viewMirrorX_, viewRotationRadians_),
            uvViewScreenFromUv(b.bottomRight(), {}, 1.0, viewMirrorX_, viewRotationRadians_),
            uvViewScreenFromUv(b.bottomLeft(), {}, 1.0, viewMirrorX_, viewRotationRadians_)
        };
        const QRectF orientedBounds = uvPointBounds(orientedCorners);

        const qreal usableW = std::max(100, width() - 64);
        const qreal usableH = std::max(100, height() - 64);
        const qreal sx = usableW / std::max<qreal>(orientedBounds.width(), 0.25);
        const qreal sy = usableH / std::max<qreal>(orientedBounds.height(), 0.25);
        zoom_ = std::clamp(
            std::min(sx, sy), kUvEditorZoomMinimum, zoomMaximum_);
        const QPointF screenCenter(width() * 0.5, height() * 0.5);
        pan_ = uvViewPanForAnchor(
            b.center(),
            screenCenter,
            zoom_,
            viewMirrorX_,
            viewRotationRadians_);
        publishViewProperties();
        update();
        if (notifyRecovery)
        {
            notifySettledWorkspaceChanged();
        }
    }

    void selectAll()
    {
        if (modalTransformActive_)
        {
            return;
        }

        if (pickMode_ == PickMode::face)
        {
            clearFaceModeComponentCache();
        }
        clearComponentSelection();
        for (std::size_t islandIndex = 0; islandIndex < islands_.size(); ++islandIndex)
        {
            UvIslandState& island = islands_[islandIndex];
            island.selected = true;
            ensureComponentStorage(islandIndex);
            if (pickMode_ == PickMode::vertex)
            {
                std::fill(vertexSelected_[islandIndex].begin(), vertexSelected_[islandIndex].end(), true);
                syncIslandSelectionFromComponents(islandIndex);
            }
            else if (pickMode_ == PickMode::edge)
            {
                std::fill(edgeSelected_[islandIndex].begin(), edgeSelected_[islandIndex].end(), true);
                syncIslandSelectionFromComponents(islandIndex);
            }
        }
        normalizeActiveIsland();
        publishStatus();
        update();
        notifySettledWorkspaceChanged();
    }

    void rotateSelected(const qreal degrees)
    {
        if (modalTransformActive_ || !hasSelectedIslands())
        {
            return;
        }

        const QPointF pivot = activeTransformCenter();
        const qreal radians = degrees * (std::numbers::pi_v<qreal> / 180.0);
        const qreal cs = std::cos(radians);
        const qreal sn = std::sin(radians);
        forEachSelectedPoint([&](QPointF& p)
            {
                const QPointF d = p - pivot;
                p = QPointF(pivot.x() + d.x() * cs - d.y() * sn, pivot.y() + d.x() * sn + d.y() * cs);
            });
        emitLiveApply();
        update();
    }

    void scaleSelected(const qreal sx, const qreal sy)
    {
        if (modalTransformActive_ || !hasSelectedIslands())
        {
            return;
        }

        const QPointF pivot = activeTransformCenter();
        forEachSelectedPoint([&](QPointF& p)
            {
                p = QPointF(pivot.x() + (p.x() - pivot.x()) * sx, pivot.y() + (p.y() - pivot.y()) * sy);
            });
        emitLiveApply();
        update();
    }

    void mirrorSelected(const bool mirrorX, const bool mirrorY)
    {
        if (modalTransformActive_ || !hasSelectedIslands())
        {
            return;
        }

        const QPointF c = activeTransformCenter();
        forEachSelectedPoint([&](QPointF& p)
            {
                p = QPointF(
                    mirrorX ? (c.x() - (p.x() - c.x())) : p.x(),
                    mirrorY ? (c.y() - (p.y() - c.y())) : p.y());
            });

        emitLiveApply();
        update();
    }

    void unwrapSelected()
    {
        if (modalTransformActive_ || !hasSelectedIslands())
        {
            return;
        }

        if (pickMode_ != PickMode::face)
        {
            for (std::size_t islandIndex = 0; islandIndex < islands_.size(); ++islandIndex)
            {
                syncIslandSelectionFromComponents(islandIndex);
            }
        }

        for (std::size_t islandIndex = 0; islandIndex < islands_.size(); ++islandIndex)
        {
            UvIslandState& island = islands_[islandIndex];
            if (islandSelectedInCurrentMode(islandIndex) && !island.unwrapUv.empty())
            {
                island.uv = island.unwrapUv;
            }
        }
        packIslands(islands_, true);
        emitLiveApply();
        update();
    }

    void packSelected()
    {
        if (modalTransformActive_ || !hasSelectedIslands())
        {
            return;
        }

        if (pickMode_ != PickMode::face)
        {
            for (std::size_t islandIndex = 0; islandIndex < islands_.size(); ++islandIndex)
            {
                syncIslandSelectionFromComponents(islandIndex);
            }
        }

        packIslands(islands_, true);
        emitLiveApply();
        update();
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);
        p.setRenderHint(QPainter::SmoothPixmapTransform, true);
        const QColor canvasBackgroundColor =
            property("sed.uvCanvasBackgroundColor").value<QColor>();
        p.fillRect(rect(), canvasBackgroundColor.isValid()
            ? canvasBackgroundColor
            : QColor(15, 20, 29));

        if (!previewTexture_.isNull())
        {
            if (repeatTexturePreviewEnabled_)
            {
                drawTextureTiles(p);
            }
            else
            {
                drawSingleTextureTile(p);
            }
        }

        drawGrid(p);

        const std::optional<PickCandidate> activeVertex =
            resolveComponent(activeVertex_);
        const std::optional<PickCandidate> activeEdge =
            resolveComponent(activeEdge_);
        const std::optional<PickCandidate> facePivotVertex =
            resolveComponent(facePivotVertex_);

        for (std::size_t i = 0; i < islands_.size(); ++i)
        {
            const UvIslandState& island = islands_[i];
            if (island.uv.size() < 2)
            {
                continue;
            }

            QPolygonF poly{};
            poly.reserve(static_cast<int>(island.uv.size()));
            for (const QPointF& uv : island.uv)
            {
                poly.push_back(screenFromUv(uv));
            }

            const bool isSelected = islandSelectedInCurrentMode(i);
            QColor fill = island.color;
            fill.setAlpha(isSelected ? 92 : 46);
            QColor outline = isSelected ? QColor(240, 244, 255) : QColor(160, 172, 188);
            p.setPen(QPen(outline, isSelected ? 2.2 : 1.2));
            p.setBrush(fill);
            p.drawPolygon(poly);

            if (pickMode_ == PickMode::edge)
            {
                for (std::size_t edgeIndex = 0; edgeIndex < island.uv.size(); ++edgeIndex)
                {
                    if (!edgeSelectedAt(i, edgeIndex))
                    {
                        continue;
                    }

                    const QPointF a = screenFromUv(island.uv[edgeIndex]);
                    const QPointF b = screenFromUv(island.uv[(edgeIndex + 1u) % island.uv.size()]);
                    p.setPen(QPen(QColor(255, 182, 86, 230), 2.4));
                    p.drawLine(a, b);
                }
            }

            p.setBrush(QColor(255, 255, 255, isSelected ? 230 : 140));
            p.setPen(Qt::NoPen);
            const std::vector<bool> selectedMask = selectedVertexMask(i);
            for (std::size_t vertexIndex = 0; vertexIndex < island.uv.size(); ++vertexIndex)
            {
                const bool selectedVertex =
                    (pickMode_ == PickMode::vertex && vertexSelectedAt(i, vertexIndex))
                    || (pickMode_ == PickMode::edge
                        && vertexIndex < selectedMask.size()
                        && selectedMask[vertexIndex]);
                const QPointF screen = screenFromUv(island.uv[vertexIndex]);
                p.setBrush(selectedVertex
                        ? QColor(255, 182, 86, 235)
                        : QColor(255, 255, 255, isSelected ? 230 : 140));
                const qreal radius = selectedVertex ? 4.2 : 3.0;
                p.drawEllipse(screen, radius, radius);
            }

            p.setPen(QColor(220, 228, 240));
            p.drawText(screenFromUv(islandCentroid(island)) + QPointF(8.0, -8.0), island.label);
        }

        drawIslandCentroids(p);
        drawExactComponentMarker(p, activeVertex, activeEdge, facePivotVertex);
        drawTransformOverlay(p);

        if (selectionRectActive_)
        {
            p.save();
            p.setPen(QPen(QColor(112, 178, 255, 210), 1.25, Qt::DashLine));
            p.setBrush(QColor(112, 178, 255, 34));
            p.drawRect(selectionRectPx_.normalized());
            p.restore();
        }
    }

    void mousePressEvent(QMouseEvent* event) override
    {
        setFocus(Qt::MouseFocusReason);
        lastMousePos_ = event->pos();
        pointerPositionKnown_ = true;
        panLastMousePos_ = event->pos();
        lastUvPos_ = uvFromScreen(event->pos());

        if (modalTransformActive_)
        {
            if (event->button() == Qt::RightButton)
            {
                cancelModalTransform();
            }
            event->accept();
            return;
        }

        if (event->button() == Qt::MiddleButton || (event->button() == Qt::LeftButton && event->modifiers().testFlag(Qt::AltModifier)))
        {
            resetPickCycle();
            leftSelectionPending_ = false;
            selectionRectActive_ = false;
            selectionRectPx_ = QRect();
            selectionRectModifiers_ = Qt::NoModifier;
            panning_ = true;
            panButton_ = event->button();
            updateCanvasCursor();
            return;
        }

        if (event->button() != Qt::LeftButton)
        {
            return;
        }

        leftSelectionPending_ = true;
        panning_ = false;
        panButton_ = Qt::NoButton;
        selectionRectActive_ = false;
        selectionRectStart_ = event->pos();
        selectionRectPx_ = QRect(event->pos(), event->pos());
        selectionRectModifiers_ = event->modifiers();
        updateCanvasCursor();
    }

    void mouseMoveEvent(QMouseEvent* event) override
    {
        lastMousePos_ = event->pos();
        pointerPositionKnown_ = true;

        if (panning_)
        {
            pan_ += QPointF(event->pos() - panLastMousePos_);
            panLastMousePos_ = event->pos();
            publishViewProperties();
            updateCanvasCursor();
            update();
            return;
        }

        if (modalTransformActive_)
        {
            applyModalTransform(event->pos());
            return;
        }

        if (!leftSelectionPending_)
        {
            return;
        }

        if (!selectionRectActive_ && (event->pos() - selectionRectStart_).manhattanLength() >= 5)
        {
            selectionRectActive_ = true;
        }

        if (selectionRectActive_)
        {
            selectionRectPx_ = QRect(selectionRectStart_, event->pos()).normalized();
        }

        updateCanvasCursor();
        update();
    }

    void leaveEvent(QEvent* event) override
    {
        pointerPositionKnown_ = false;
        QWidget::leaveEvent(event);
    }

    void mouseReleaseEvent(QMouseEvent* event) override
    {
        const bool completedPan = panning_ && event->button() == panButton_;
        if (panning_ && event->button() == panButton_)
        {
            panning_ = false;
            panButton_ = Qt::NoButton;
        }

        if (modalTransformActive_)
        {
            if (event->button() == Qt::LeftButton)
            {
                applyModalTransform(event->pos());
                commitModalTransform();
            }
            else if (event->button() == Qt::RightButton)
            {
                cancelModalTransform();
            }
        }
        else if (event->button() == Qt::LeftButton && leftSelectionPending_)
        {
            if (selectionRectActive_)
            {
                applyRectangleSelection(selectionRectPx_, selectionRectModifiers_);
            }
            else
            {
                handleSelectionClick(event->pos(), selectionRectModifiers_);
            }
        }

        leftSelectionPending_ = false;
        selectionRectActive_ = false;
        selectionRectPx_ = QRect();
        selectionRectModifiers_ = Qt::NoModifier;

        updateCanvasCursor();
        update();
        if (completedPan)
        {
            notifySettledWorkspaceChanged();
        }
    }

    void wheelEvent(QWheelEvent* event) override
    {
        if (modalTransformActive_)
        {
            event->accept();
            return;
        }

        const QPointF before = uvFromScreen(event->position());
        resetPickCycle();
        const qreal factor = (event->angleDelta().y() > 0) ? 1.12 : (1.0 / 1.12);
        zoom_ = std::clamp(
            zoom_ * factor, kUvEditorZoomMinimum, zoomMaximum_);
        const QPointF afterScreen = screenFromUv(before);
        pan_ += event->position() - afterScreen;
        publishViewProperties();
        update();
        notifySettledWorkspaceChanged();
    }

    void resizeEvent(QResizeEvent* event) override
    {
        QWidget::resizeEvent(event);
        consumePendingFitToContent();
    }

    void hideEvent(QHideEvent* event) override
    {
        if (modalTransformActive_)
        {
            cancelModalTransform();
        }
        clearPointerInteractionState();
        QWidget::hideEvent(event);
    }

    void focusOutEvent(QFocusEvent* event) override
    {
        if (modalTransformActive_)
        {
            cancelModalTransform();
        }
        clearPointerInteractionState();
        QWidget::focusOutEvent(event);
    }

    void enterEvent(QEnterEvent* event) override
    {
        QWidget::enterEvent(event);
        if (!hasFocus() && !viewportFlyOwnsUvHover(*this))
        {
            setFocus(Qt::OtherFocusReason);
        }
    }

    void keyPressEvent(QKeyEvent* event) override
    {
        constexpr Qt::KeyboardModifiers kShortcutBlockingModifiers =
            Qt::ShiftModifier | Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier;
        if (event->isAutoRepeat()
            && (event->modifiers() & kShortcutBlockingModifiers) == Qt::NoModifier
            && (event->key() == Qt::Key_G
                || event->key() == Qt::Key_R
                || event->key() == Qt::Key_S
                || event->key() == Qt::Key_A))
        {
            event->accept();
            return;
        }

        if (event->key() == Qt::Key_Control)
        {
            publishStatus();
            update();
            event->accept();
            return;
        }

        if (handleShortcutKey(event->key(), event->modifiers()))
        {
            event->accept();
            return;
        }

        QWidget::keyPressEvent(event);
    }

    void keyReleaseEvent(QKeyEvent* event) override
    {
        if (event->key() == Qt::Key_Control)
        {
            publishStatus();
            update();
            event->accept();
            return;
        }

        QWidget::keyReleaseEvent(event);
    }

private:
    static qreal screenPointSegmentDistance(
        const QPointF& point,
        const QPointF& start,
        const QPointF& end)
    {
        const QPointF edge = end - start;
        const qreal lengthSquared = QPointF::dotProduct(edge, edge);
        if (lengthSquared <= 1.0e-9)
        {
            return QLineF(point, start).length();
        }

        const qreal t = std::clamp(
            QPointF::dotProduct(point - start, edge) / lengthSquared,
            0.0,
            1.0);
        return QLineF(point, start + edge * t).length();
    }

    QPointF screenFromUv(const QPointF& uv) const
    {
        return uvViewScreenFromUv(uv, pan_, zoom_, viewMirrorX_, viewRotationRadians_);
    }

    QPointF uvFromScreen(const QPointF& pos) const
    {
        return uvViewUvFromScreen(pos, pan_, zoom_, viewMirrorX_, viewRotationRadians_);
    }

    std::vector<PickCandidate> hitIslands(const QPointF& screenPos) const
    {
        constexpr qreal kBoundaryThresholdPx = 8.0;
        std::vector<PickCandidate> hits{};
        for (int i = static_cast<int>(islands_.size()) - 1; i >= 0; --i)
        {
            QPolygonF poly{};
            for (const QPointF& uv : islands_[static_cast<std::size_t>(i)].uv)
            {
                poly.push_back(screenFromUv(uv));
            }
            bool hitsBoundary = false;
            for (int pointIndex = 0;
                 pointIndex < poly.size() && !hitsBoundary;
                 ++pointIndex)
            {
                hitsBoundary = screenPointSegmentDistance(
                    screenPos,
                    poly[pointIndex],
                    poly[(pointIndex + 1) % poly.size()])
                    <= kBoundaryThresholdPx;
            }
            if (poly.containsPoint(screenPos, Qt::OddEvenFill)
                || hitsBoundary)
            {
                hits.push_back(PickCandidate{
                    static_cast<std::size_t>(i), 0u });
            }
        }
        return hits;
    }

    std::vector<PickCandidate> hitVertices(const QPointF& screenPos) const
    {
        constexpr qreal kThresholdPx = 10.0;
        struct ScoredHit
        {
            PickCandidate candidate{};
            qreal distance{};
        };
        std::vector<ScoredHit> scored{};
        for (int islandIndex = static_cast<int>(islands_.size()) - 1; islandIndex >= 0; --islandIndex)
        {
            const UvIslandState& island = islands_[static_cast<std::size_t>(islandIndex)];
            for (int vertexIndex = 0; vertexIndex < static_cast<int>(island.uv.size()); ++vertexIndex)
            {
                const qreal distance = QLineF(screenFromUv(island.uv[static_cast<std::size_t>(vertexIndex)]), screenPos).length();
                if (distance <= kThresholdPx)
                {
                    scored.push_back(ScoredHit{
                        PickCandidate{
                            static_cast<std::size_t>(islandIndex),
                            static_cast<std::size_t>(vertexIndex) },
                        distance });
                }
            }
        }
        std::stable_sort(
            scored.begin(),
            scored.end(),
            [](const ScoredHit& lhs, const ScoredHit& rhs)
            {
                if (std::abs(lhs.distance - rhs.distance) > 1.0e-6)
                {
                    return lhs.distance < rhs.distance;
                }
                if (lhs.candidate.island != rhs.candidate.island)
                {
                    return lhs.candidate.island > rhs.candidate.island;
                }
                return lhs.candidate.component < rhs.candidate.component;
            });
        std::vector<PickCandidate> hits{};
        hits.reserve(scored.size());
        for (const ScoredHit& hit : scored)
        {
            hits.push_back(hit.candidate);
        }
        return hits;
    }

    std::vector<PickCandidate> hitEdges(const QPointF& screenPos) const
    {
        constexpr qreal kThresholdPx = 8.0;
        struct ScoredHit
        {
            PickCandidate candidate{};
            qreal distance{};
        };
        std::vector<ScoredHit> scored{};
        for (int islandIndex = static_cast<int>(islands_.size()) - 1; islandIndex >= 0; --islandIndex)
        {
            const UvIslandState& island = islands_[static_cast<std::size_t>(islandIndex)];
            for (int edgeIndex = 0; edgeIndex < static_cast<int>(island.uv.size()); ++edgeIndex)
            {
                const QPointF a = screenFromUv(island.uv[static_cast<std::size_t>(edgeIndex)]);
                const QPointF b = screenFromUv(island.uv[(static_cast<std::size_t>(edgeIndex) + 1u) % island.uv.size()]);
                const qreal distance = screenPointSegmentDistance(
                    screenPos, a, b);
                if (distance <= kThresholdPx)
                {
                    scored.push_back(ScoredHit{
                        PickCandidate{
                            static_cast<std::size_t>(islandIndex),
                            static_cast<std::size_t>(edgeIndex) },
                        distance });
                }
            }
        }
        std::stable_sort(
            scored.begin(),
            scored.end(),
            [](const ScoredHit& lhs, const ScoredHit& rhs)
            {
                if (std::abs(lhs.distance - rhs.distance) > 1.0e-6)
                {
                    return lhs.distance < rhs.distance;
                }
                if (lhs.candidate.island != rhs.candidate.island)
                {
                    return lhs.candidate.island > rhs.candidate.island;
                }
                return lhs.candidate.component < rhs.candidate.component;
            });
        std::vector<PickCandidate> hits{};
        hits.reserve(scored.size());
        for (const ScoredHit& hit : scored)
        {
            hits.push_back(hit.candidate);
        }
        return hits;
    }

    std::optional<std::pair<int, int>> hitVertex(const QPointF& screenPos) const
    {
        const std::vector<PickCandidate> hits = hitVertices(screenPos);
        if (hits.empty())
        {
            return std::nullopt;
        }
        return std::make_pair(
            static_cast<int>(hits.front().island),
            static_cast<int>(hits.front().component));
    }

    void resetPickCycle()
    {
        pickCycleCandidates_.clear();
        pickCycleNextIndex_ = 0;
        pickCycleTimer_.invalidate();
    }

    std::optional<PickCandidate> cyclePickCandidates(
        const std::vector<PickCandidate>& candidates,
        const QPoint& position,
        const Qt::KeyboardModifiers modifiers,
        const bool includeNoActiveStep)
    {
        if (candidates.empty())
        {
            resetPickCycle();
            return std::nullopt;
        }

        constexpr int kCycleDistancePx = 8;
        constexpr qint64 kCycleTimeoutMs = 1'500;
        const Qt::KeyboardModifiers selectionModifiers = modifiers
            & (Qt::ShiftModifier | Qt::ControlModifier);
        const bool continuing = pickCycleTimer_.isValid()
            && pickCycleTimer_.elapsed() <= kCycleTimeoutMs
            && (position - pickCyclePosition_).manhattanLength()
                <= kCycleDistancePx
            && pickCycleMode_ == pickMode_
            && pickCycleModifiers_ == selectionModifiers
            && pickCycleCandidates_ == candidates;
        if (!continuing)
        {
            pickCycleNextIndex_ = 0;
        }

        const std::size_t span = candidates.size()
            + (includeNoActiveStep ? 1u : 0u);
        const std::size_t index = span > 0u
            ? pickCycleNextIndex_ % span
            : 0u;
        const std::optional<PickCandidate> result = index < candidates.size()
            ? std::optional<PickCandidate>{ candidates[index] }
            : std::nullopt;

        pickCycleCandidates_ = candidates;
        pickCyclePosition_ = position;
        pickCycleMode_ = pickMode_;
        pickCycleModifiers_ = selectionModifiers;
        pickCycleNextIndex_ = span > 0u ? (index + 1u) % span : 0u;
        pickCycleTimer_.restart();
        return result;
    }

    ComponentRef componentRef(const PickCandidate candidate) const
    {
        return ComponentRef{
            candidate.island < islands_.size()
                ? islands_[candidate.island].ref
                : SurfaceRef{ -1, -1 },
            candidate.component };
    }

    std::optional<std::size_t> islandIndexForRef(
        const SurfaceRef ref) const
    {
        for (std::size_t islandIndex = 0;
             islandIndex < islands_.size();
             ++islandIndex)
        {
            if (islands_[islandIndex].ref.sector == ref.sector
                && islands_[islandIndex].ref.surface == ref.surface)
            {
                return islandIndex;
            }
        }
        return std::nullopt;
    }

    std::optional<PickCandidate> resolveComponent(
        const std::optional<ComponentRef>& ref) const
    {
        if (!ref)
        {
            return std::nullopt;
        }
        const std::optional<std::size_t> islandIndex =
            islandIndexForRef(ref->surface);
        if (!islandIndex
            || ref->component >= islands_[*islandIndex].uv.size())
        {
            return std::nullopt;
        }
        return PickCandidate{ *islandIndex, ref->component };
    }

    std::optional<QPointF> activeComponentPivot() const
    {
        if (pickMode_ == PickMode::vertex)
        {
            const std::optional<PickCandidate> active =
                resolveComponent(activeVertex_);
            if (active
                && vertexSelectedAt(active->island, active->component))
            {
                return islands_[active->island].uv[active->component];
            }
        }
        else if (pickMode_ == PickMode::edge)
        {
            const std::optional<PickCandidate> active =
                resolveComponent(activeEdge_);
            if (active && edgeSelectedAt(active->island, active->component))
            {
                const UvIslandState& island = islands_[active->island];
                const QPointF& a = island.uv[active->component];
                const QPointF& b = island.uv[
                    (active->component + 1u) % island.uv.size()];
                return (a + b) * 0.5;
            }
        }
        else
        {
            const std::optional<PickCandidate> pivot =
                resolveComponent(facePivotVertex_);
            if (pivot && islands_[pivot->island].selected)
            {
                return islands_[pivot->island].uv[pivot->component];
            }
        }
        return std::nullopt;
    }

    QPointF selectionCenterFallback() const
    {
        std::vector<QPointF> points{};
        for (std::size_t islandIndex = 0; islandIndex < islands_.size(); ++islandIndex)
        {
            const std::vector<bool> mask = selectedVertexMask(islandIndex);
            for (std::size_t vertexIndex = 0; vertexIndex < mask.size(); ++vertexIndex)
            {
                if (mask[vertexIndex])
                {
                    points.push_back(islands_[islandIndex].uv[vertexIndex]);
                }
            }
        }
        if (!points.empty())
        {
            return uvPointCloudCenter(points);
        }
        const QRectF bounds = selectedBounds();
        return bounds.isValid() ? bounds.center() : QPointF();
    }

    void ensureComponentStorage(const std::size_t islandIndex)
    {
        if (vertexSelected_.size() < islands_.size())
        {
            vertexSelected_.resize(islands_.size());
        }
        if (edgeSelected_.size() < islands_.size())
        {
            edgeSelected_.resize(islands_.size());
        }
        if (islandIndex < islands_.size())
        {
            if (vertexSelected_[islandIndex].size() != islands_[islandIndex].uv.size())
            {
                vertexSelected_[islandIndex].assign(islands_[islandIndex].uv.size(), false);
            }
            if (edgeSelected_[islandIndex].size() != islands_[islandIndex].uv.size())
            {
                edgeSelected_[islandIndex].assign(islands_[islandIndex].uv.size(), false);
            }
        }
    }

    void syncComponentStorage()
    {
        vertexSelected_.resize(islands_.size());
        edgeSelected_.resize(islands_.size());
        for (std::size_t i = 0; i < islands_.size(); ++i)
        {
            if (vertexSelected_[i].size() != islands_[i].uv.size())
            {
                vertexSelected_[i].assign(islands_[i].uv.size(), false);
            }
            if (edgeSelected_[i].size() != islands_[i].uv.size())
            {
                edgeSelected_[i].assign(islands_[i].uv.size(), false);
            }
        }
    }

    void clearComponentSelection()
    {
        syncComponentStorage();
        for (std::size_t i = 0; i < vertexSelected_.size(); ++i)
        {
            std::fill(vertexSelected_[i].begin(), vertexSelected_[i].end(), false);
            std::fill(edgeSelected_[i].begin(), edgeSelected_[i].end(), false);
        }
    }

    void clearFaceModeComponentCache()
    {
        faceModeCachedVertexSelected_.clear();
        faceModeCachedEdgeSelected_.clear();
        faceModeCachedSelectedVertices_.clear();
        faceModeHasComponentCache_ = false;
    }

    bool vertexSelectedAt(const std::size_t islandIndex, const std::size_t vertexIndex) const
    {
        return islandIndex < vertexSelected_.size()
            && vertexIndex < vertexSelected_[islandIndex].size()
            && vertexSelected_[islandIndex][vertexIndex];
    }

    bool edgeSelectedAt(const std::size_t islandIndex, const std::size_t edgeIndex) const
    {
        return islandIndex < edgeSelected_.size()
            && edgeIndex < edgeSelected_[islandIndex].size()
            && edgeSelected_[islandIndex][edgeIndex];
    }

    std::vector<bool> selectedVertexMask(const std::size_t islandIndex) const
    {
        std::vector<bool> mask{};
        if (islandIndex >= islands_.size())
        {
            return mask;
        }

        mask.assign(islands_[islandIndex].uv.size(), false);
        if (pickMode_ == PickMode::face)
        {
            if (islands_[islandIndex].selected)
            {
                std::fill(mask.begin(), mask.end(), true);
            }
            return mask;
        }

        if (pickMode_ == PickMode::vertex)
        {
            if (islandIndex < vertexSelected_.size())
            {
                const std::vector<bool>& selected = vertexSelected_[islandIndex];
                const std::size_t count = std::min(mask.size(), selected.size());
                for (std::size_t vertexIndex = 0; vertexIndex < count; ++vertexIndex)
                {
                    mask[vertexIndex] = selected[vertexIndex];
                }
            }
            return mask;
        }

        if (islandIndex < edgeSelected_.size())
        {
            const std::vector<bool>& selected = edgeSelected_[islandIndex];
            const std::size_t count = std::min(mask.size(), selected.size());
            for (std::size_t edgeIndex = 0; edgeIndex < count; ++edgeIndex)
            {
                if (!selected[edgeIndex] || islands_[islandIndex].uv.empty())
                {
                    continue;
                }

                mask[edgeIndex] = true;
                mask[(edgeIndex + 1u) % islands_[islandIndex].uv.size()] = true;
            }
        }
        return mask;
    }

    void syncIslandSelectionFromComponents(const std::size_t islandIndex)
    {
        if (pickMode_ == PickMode::face || islandIndex >= islands_.size())
        {
            return;
        }

        if (pickMode_ == PickMode::vertex)
        {
            islands_[islandIndex].selected = islandIndex < vertexSelected_.size()
                && std::any_of(vertexSelected_[islandIndex].begin(), vertexSelected_[islandIndex].end(), [](const bool value) { return value; });
            return;
        }

        islands_[islandIndex].selected = islandIndex < edgeSelected_.size()
            && std::any_of(edgeSelected_[islandIndex].begin(), edgeSelected_[islandIndex].end(), [](const bool value) { return value; });
    }

    void clearFaceSelection()
    {
        for (UvIslandState& island : islands_)
        {
            island.selected = false;
        }
        activeIslandIndex_.reset();
    }

    void normalizeActiveComponentMarkers()
    {
        if (const std::optional<PickCandidate> active =
                resolveComponent(activeVertex_);
            !active
            || !vertexSelectedAt(active->island, active->component))
        {
            activeVertex_.reset();
        }
        if (const std::optional<PickCandidate> active =
                resolveComponent(activeEdge_);
            !active || !edgeSelectedAt(active->island, active->component))
        {
            activeEdge_.reset();
        }
        if (const std::optional<PickCandidate> pivot =
                resolveComponent(facePivotVertex_);
            !pivot || !islands_[pivot->island].selected)
        {
            facePivotVertex_.reset();
        }
    }

    void handleSelectionClick(const QPoint& pos, const Qt::KeyboardModifiers modifiers)
    {
        const bool addSelection = modifiers.testFlag(Qt::ShiftModifier);
        const bool removeSelection = modifiers.testFlag(Qt::ControlModifier) && !addSelection;

        if (pickMode_ == PickMode::face)
        {
            const std::vector<bool> selectedBefore = selectedIslandMask();
            const std::vector<PickCandidate> hits = hitIslands(pos);
            const std::optional<PickCandidate> hit =
                cyclePickCandidates(hits, pos, modifiers, false);
            if (!hit)
            {
                if (!addSelection && !removeSelection)
                {
                    clearFaceSelection();
                    facePivotVertex_.reset();
                }
            }
            else if (addSelection)
            {
                islands_[hit->island].selected = true;
                activeIslandIndex_ = hit->island;
            }
            else if (removeSelection)
            {
                islands_[hit->island].selected = false;
                if (activeIslandIndex_ && *activeIslandIndex_ == hit->island)
                {
                    activeIslandIndex_.reset();
                }
            }
            else
            {
                clearFaceSelection();
                islands_[hit->island].selected = true;
                activeIslandIndex_ = hit->island;
            }

            if (hit && !removeSelection)
            {
                constexpr qreal kCenterResetThresholdPx = 8.0;
                const QPointF centerPx = screenFromUv(
                    islandCentroid(islands_[hit->island]));
                const std::vector<PickCandidate> vertexHits = hitVertices(pos);
                const auto matchingVertex = std::find_if(
                    vertexHits.begin(),
                    vertexHits.end(),
                    [hit](const PickCandidate candidate)
                    {
                        return candidate.island == hit->island;
                    });
                if (matchingVertex != vertexHits.end())
                {
                    // Face selection remains a whole-face selection. This is
                    // only an explicit transform pivot and never seeds the
                    // component-selection masks.
                    facePivotVertex_ = componentRef(*matchingVertex);
                }
                else if (QLineF(centerPx, pos).length()
                    <= kCenterResetThresholdPx)
                {
                    facePivotVertex_.reset();
                }
            }

            // A pivot-only click on an already selected face must not discard
            // the component masks cached when Face mode was entered. Those
            // masks make returning to Vertex/Edge mode non-destructive. A
            // genuine face selection change invalidates the cache as before.
            if (selectedIslandMask() != selectedBefore)
            {
                clearFaceModeComponentCache();
            }
        }
        else
        {
            syncComponentStorage();
            constexpr qreal kCenterResetThresholdPx = 8.0;
            const bool exactPivotActive = pickMode_ == PickMode::vertex
                ? activeVertex_.has_value()
                : activeEdge_.has_value();
            if (!addSelection && !removeSelection && exactPivotActive
                && hasSelectedIslands()
                && QLineF(screenFromUv(selectionCenterFallback()), pos).length()
                    <= kCenterResetThresholdPx)
            {
                // Clicking the aggregate selection marker returns transforms
                // to the center pivot. It intentionally leaves every selected
                // vertex/edge bit untouched.
                if (pickMode_ == PickMode::vertex)
                {
                    activeVertex_.reset();
                }
                else
                {
                    activeEdge_.reset();
                }
                resetPickCycle();
                normalizeActiveIsland();
                normalizeActiveComponentMarkers();
                publishStatus();
                updateCanvasCursor();
                update();
                notifySettledWorkspaceChanged();
                return;
            }
            if (pickMode_ == PickMode::vertex)
            {
                const std::vector<PickCandidate> hits = hitVertices(pos);
                const std::optional<PickCandidate> hit =
                    cyclePickCandidates(hits, pos, modifiers, true);
                const bool terminalCycleStep = !hit && !hits.empty();
                if (!addSelection && !removeSelection && !terminalCycleStep)
                {
                    clearFaceSelection();
                    clearComponentSelection();
                }
                if (hit)
                {
                    const std::size_t islandIndex = hit->island;
                    const std::size_t vertexIndex = hit->component;
                    ensureComponentStorage(islandIndex);
                    vertexSelected_[islandIndex][vertexIndex] = !removeSelection;
                    activeIslandIndex_ = islandIndex;
                    activeVertex_ = removeSelection
                        ? std::nullopt
                        : std::optional<ComponentRef>{ componentRef(*hit) };
                    syncIslandSelectionFromComponents(islandIndex);
                }
                else if (!hits.empty())
                {
                    // One deliberate terminal cycle step removes only the
                    // exact primary/pivot marker. The component selection is
                    // retained and transforms fall back to its center.
                    activeVertex_.reset();
                }
                else
                {
                    activeVertex_.reset();
                }
            }
            else
            {
                const std::vector<PickCandidate> hits = hitEdges(pos);
                const std::optional<PickCandidate> hit =
                    cyclePickCandidates(hits, pos, modifiers, true);
                const bool terminalCycleStep = !hit && !hits.empty();
                if (!addSelection && !removeSelection && !terminalCycleStep)
                {
                    clearFaceSelection();
                    clearComponentSelection();
                }
                if (hit)
                {
                    const std::size_t islandIndex = hit->island;
                    const std::size_t edgeIndex = hit->component;
                    ensureComponentStorage(islandIndex);
                    edgeSelected_[islandIndex][edgeIndex] = !removeSelection;
                    activeIslandIndex_ = islandIndex;
                    activeEdge_ = removeSelection
                        ? std::nullopt
                        : std::optional<ComponentRef>{ componentRef(*hit) };
                    syncIslandSelectionFromComponents(islandIndex);
                }
                else if (!hits.empty())
                {
                    activeEdge_.reset();
                }
                else
                {
                    activeEdge_.reset();
                }
            }
        }

        normalizeActiveIsland();
        normalizeActiveComponentMarkers();
        publishStatus();
        updateCanvasCursor();
        update();
        notifySettledWorkspaceChanged();
    }

    bool islandIntersectsRect(const std::size_t islandIndex, const QRectF& rect) const
    {
        if (islandIndex >= islands_.size())
        {
            return false;
        }

        QPolygonF poly{};
        poly.reserve(static_cast<int>(islands_[islandIndex].uv.size()));
        for (const QPointF& uv : islands_[islandIndex].uv)
        {
            poly.push_back(screenFromUv(uv));
        }
        if (poly.isEmpty())
        {
            return false;
        }

        for (const QPointF& point : poly)
        {
            if (rect.contains(point))
            {
                return true;
            }
        }
        for (int i = 0; i < poly.size(); ++i)
        {
            QLineF edge(poly[i], poly[(i + 1) % poly.size()]);
            if (rect.contains(edge.p1()) || rect.contains(edge.p2()))
            {
                return true;
            }
            if (edge.intersects(QLineF(rect.topLeft(), rect.topRight()), nullptr) == QLineF::BoundedIntersection
                || edge.intersects(QLineF(rect.topRight(), rect.bottomRight()), nullptr) == QLineF::BoundedIntersection
                || edge.intersects(QLineF(rect.bottomRight(), rect.bottomLeft()), nullptr) == QLineF::BoundedIntersection
                || edge.intersects(QLineF(rect.bottomLeft(), rect.topLeft()), nullptr) == QLineF::BoundedIntersection)
            {
                return true;
            }
        }
        return poly.containsPoint(rect.center(), Qt::OddEvenFill);
    }

    void applyRectangleSelection(const QRect& rectPx, const Qt::KeyboardModifiers modifiers)
    {
        const QRectF rect = rectPx.normalized();
        const bool addSelection = modifiers.testFlag(Qt::ShiftModifier);
        const bool removeSelection = modifiers.testFlag(Qt::ControlModifier) && !addSelection;

        syncComponentStorage();
        if (pickMode_ == PickMode::face)
        {
            clearFaceModeComponentCache();
        }
        if (!addSelection && !removeSelection)
        {
            clearFaceSelection();
            clearComponentSelection();
        }

        for (std::size_t islandIndex = 0; islandIndex < islands_.size(); ++islandIndex)
        {
            if (pickMode_ == PickMode::face)
            {
                if (islandIntersectsRect(islandIndex, rect))
                {
                    islands_[islandIndex].selected = !removeSelection;
                    if (!removeSelection)
                    {
                        activeIslandIndex_ = islandIndex;
                    }
                }
                continue;
            }

            ensureComponentStorage(islandIndex);
            if (pickMode_ == PickMode::vertex)
            {
                for (std::size_t vertexIndex = 0; vertexIndex < islands_[islandIndex].uv.size(); ++vertexIndex)
                {
                    if (!rect.contains(screenFromUv(islands_[islandIndex].uv[vertexIndex])))
                    {
                        continue;
                    }
                    vertexSelected_[islandIndex][vertexIndex] = !removeSelection;
                    if (!removeSelection)
                    {
                        activeIslandIndex_ = islandIndex;
                    }
                }
            }
            else
            {
                for (std::size_t edgeIndex = 0; edgeIndex < islands_[islandIndex].uv.size(); ++edgeIndex)
                {
                    const QPointF a = screenFromUv(islands_[islandIndex].uv[edgeIndex]);
                    const QPointF b = screenFromUv(islands_[islandIndex].uv[(edgeIndex + 1u) % islands_[islandIndex].uv.size()]);
                    const QLineF edge(a, b);
                    const bool hit =
                        rect.contains(a)
                        || rect.contains(b)
                        || edge.intersects(QLineF(rect.topLeft(), rect.topRight()), nullptr) == QLineF::BoundedIntersection
                        || edge.intersects(QLineF(rect.topRight(), rect.bottomRight()), nullptr) == QLineF::BoundedIntersection
                        || edge.intersects(QLineF(rect.bottomRight(), rect.bottomLeft()), nullptr) == QLineF::BoundedIntersection
                        || edge.intersects(QLineF(rect.bottomLeft(), rect.topLeft()), nullptr) == QLineF::BoundedIntersection;
                    if (!hit)
                    {
                        continue;
                    }
                    edgeSelected_[islandIndex][edgeIndex] = !removeSelection;
                    if (!removeSelection)
                    {
                        activeIslandIndex_ = islandIndex;
                    }
                }
            }

            syncIslandSelectionFromComponents(islandIndex);
        }

        normalizeActiveIsland();
        normalizeActiveComponentMarkers();
        publishStatus();
        updateCanvasCursor();
        notifySettledWorkspaceChanged();
    }

    template <typename Fn>
    void forEachSelectedPoint(const Fn& fn)
    {
        for (std::size_t islandIndex = 0; islandIndex < islands_.size(); ++islandIndex)
        {
            std::vector<bool> mask = selectedVertexMask(islandIndex);
            for (std::size_t vertexIndex = 0; vertexIndex < mask.size(); ++vertexIndex)
            {
                if (mask[vertexIndex])
                {
                    fn(islands_[islandIndex].uv[vertexIndex]);
                }
            }
        }
    }

    bool islandSelectedInCurrentMode(const std::size_t islandIndex) const
    {
        if (islandIndex >= islands_.size())
        {
            return false;
        }

        if (pickMode_ == PickMode::face)
        {
            return islands_[islandIndex].selected;
        }

        const std::vector<bool> mask = selectedVertexMask(islandIndex);
        return std::any_of(mask.begin(), mask.end(), [](const bool value) { return value; });
    }

    void normalizeActiveIsland()
    {
        if (activeIslandIndex_ && islandSelectedInCurrentMode(*activeIslandIndex_))
        {
            return;
        }

        activeIslandIndex_.reset();
        for (std::size_t i = 0; i < islands_.size(); ++i)
        {
            if (islandSelectedInCurrentMode(i))
            {
                activeIslandIndex_ = i;
                return;
            }
        }
    }

    std::optional<std::size_t> activeSelectedIslandIndex() const
    {
        if (activeIslandIndex_ && islandSelectedInCurrentMode(*activeIslandIndex_))
        {
            return activeIslandIndex_;
        }

        for (std::size_t i = 0; i < islands_.size(); ++i)
        {
            if (islandSelectedInCurrentMode(i))
            {
                return i;
            }
        }

        return std::nullopt;
    }

    QRectF selectedBounds() const
    {
        if (pickMode_ == PickMode::face)
        {
            return islandsBounds(islands_, true);
        }

        std::vector<QPointF> points{};
        for (std::size_t islandIndex = 0; islandIndex < islands_.size(); ++islandIndex)
        {
            const std::vector<bool> mask = selectedVertexMask(islandIndex);
            for (std::size_t vertexIndex = 0; vertexIndex < mask.size(); ++vertexIndex)
            {
                if (mask[vertexIndex])
                {
                    points.push_back(islands_[islandIndex].uv[vertexIndex]);
                }
            }
        }
        return uvPointBounds(points);
    }

    QRectF activeTransformBounds() const
    {
        const QRectF bounds = selectedBounds();
        return bounds.isValid() ? bounds : QRectF();
    }

    QPointF activeTransformCenter() const
    {
        if (const std::optional<QPointF> activePivot = activeComponentPivot())
        {
            return *activePivot;
        }

        return selectionCenterFallback();
    }

    bool hasSelectedIslands() const
    {
        for (std::size_t islandIndex = 0; islandIndex < islands_.size(); ++islandIndex)
        {
            if (islandSelectedInCurrentMode(islandIndex))
            {
                return true;
            }
        }
        return false;
    }

    bool snappingActive() const
    {
        return persistentSnapEnabled_ || QApplication::keyboardModifiers().testFlag(Qt::ControlModifier);
    }

    void consumePendingFitToContent()
    {
        if (!fitToContentPending_ || modalTransformActive_ || islands_.empty() || width() <= 0 || height() <= 0)
        {
            return;
        }

        fitToContentPending_ = false;
        // This deferred fit is initialization derived from authoritative UV
        // geometry, not a user workspace edit. In particular, it must not
        // make an untouched file appear recovery-eligible after Open.
        fitToContent(false, false);
    }

    QString modalTransformLabel() const
    {
        switch (modalTransformMode_)
        {
        case ModalTransformMode::grab:
            return "G - Move";
        case ModalTransformMode::grabSlide:
            return "GG - Slide";
        case ModalTransformMode::rotate:
            return "R - Rotate";
        case ModalTransformMode::rotateTrackball:
            return "RR - Pitch/Yaw Rotate";
        case ModalTransformMode::scale:
            return "S - Scale";
        default:
            return {};
        }
    }

    void publishInteractionProperties()
    {
        const std::optional<PickCandidate> vertex =
            resolveComponent(activeVertex_);
        const std::optional<PickCandidate> edge =
            resolveComponent(activeEdge_);
        const std::optional<PickCandidate> facePivot =
            resolveComponent(facePivotVertex_);
        const std::optional<std::size_t> activeIsland =
            activeSelectedIslandIndex();
        setProperty("sed.uvActiveIsland",
            activeIsland ? static_cast<int>(*activeIsland) : -1);
        int selectedFaceCount = 0;
        int selectedVertexCount = 0;
        int selectedEdgeCount = 0;
        for (std::size_t islandIndex = 0;
             islandIndex < islands_.size();
             ++islandIndex)
        {
            selectedFaceCount += islands_[islandIndex].selected ? 1 : 0;
            if (islandIndex < vertexSelected_.size())
            {
                selectedVertexCount += static_cast<int>(std::count(
                    vertexSelected_[islandIndex].begin(),
                    vertexSelected_[islandIndex].end(),
                    true));
            }
            if (islandIndex < edgeSelected_.size())
            {
                selectedEdgeCount += static_cast<int>(std::count(
                    edgeSelected_[islandIndex].begin(),
                    edgeSelected_[islandIndex].end(),
                    true));
            }
        }
        setProperty("sed.uvSelectedFaceCount", selectedFaceCount);
        setProperty("sed.uvSelectedVertexCount", selectedVertexCount);
        setProperty("sed.uvSelectedEdgeCount", selectedEdgeCount);
        setProperty("sed.uvActiveVertexIsland",
            vertex ? static_cast<int>(vertex->island) : -1);
        setProperty("sed.uvActiveVertexIndex",
            vertex ? static_cast<int>(vertex->component) : -1);
        setProperty("sed.uvActiveEdgeIsland",
            edge ? static_cast<int>(edge->island) : -1);
        setProperty("sed.uvActiveEdgeIndex",
            edge ? static_cast<int>(edge->component) : -1);
        setProperty("sed.uvFacePivotVertexIsland",
            facePivot ? static_cast<int>(facePivot->island) : -1);
        setProperty("sed.uvFacePivotVertexIndex",
            facePivot ? static_cast<int>(facePivot->component) : -1);
        const QPointF pivot = activeTransformCenter();
        setProperty("sed.uvTransformPivotX", pivot.x());
        setProperty("sed.uvTransformPivotY", pivot.y());
        setProperty("sed.uvPickCycleCandidates",
            static_cast<int>(pickCycleCandidates_.size()));
        publishViewProperties();
    }

    void publishStatus()
    {
        publishInteractionProperties();
        if (!onStatusText_)
        {
            return;
        }

        const QString pickModeText = [this]() -> QString
        {
            switch (pickMode_)
            {
                case PickMode::face: return "Face mode";
                case PickMode::edge: return "Edge mode";
                case PickMode::vertex: return "Vertex mode";
                default: return {};
            }
        }();
        const QString orientationText = viewportFacingViewEnabled_
            ? QString("View-facing (%1%2 deg)")
                  .arg(viewMirrorX_ ? "mirrored, " : "")
                  .arg(
                      viewRotationRadians_ * (180.0 / std::numbers::pi_v<qreal>),
                      0,
                      'f',
                      1)
            : "Texture space";

        if (modalTransformActive_)
        {
            onStatusText_(QString(
                "%1 | %2 | %3 | %4 | Enter/LMB confirm | Esc/RMB cancel")
                    .arg(
                        modalTransformLabel(),
                        pickModeText,
                        orientationText,
                        snappingActive() ? "Snap active" : "Ctrl - Snap"));
            return;
        }

        onStatusText_(
            QString(
                "%1 | %2 | G - Move | GG - Slide | R - Rotate | RR - Pitch/Yaw Rotate | "
                "S - Scale | A - Select all | %3 | Shift-click adds | Ctrl-click removes | "
                "Repeat-click cycles overlaps | Red primary is pivot | "
                "LMB-drag selects | Wheel zooms | Middle-drag pans")
                .arg(pickModeText)
                .arg(orientationText)
                .arg(snappingActive() ? "Snap active" : "Ctrl/Snap - Snap"));
    }

    void emitLiveApply()
    {
        if (onLiveApply_)
        {
            onLiveApply_(islands_);
        }
    }

    void notifySettledWorkspaceChanged()
    {
        // An in-progress G/S/R preview is intentionally not a checkpointed
        // workspace state. Its Level commit schedules recovery only after the
        // user accepts the transaction.
        if (!modalTransformActive_ && onSettledWorkspaceChanged_)
        {
            onSettledWorkspaceChanged_();
        }
    }

    void beginModalTransform(const ModalTransformMode mode)
    {
        if (!hasSelectedIslands())
        {
            publishStatus();
            return;
        }

        clearPointerInteractionState();
        const bool continuingChain = modalTransformActive_;
        QPoint startMousePos = lastMousePos_;
        if (!pointerPositionKnown_ || !rect().contains(startMousePos))
        {
            startMousePos = mapFromGlobal(QCursor::pos());
            if (!rect().contains(startMousePos))
            {
                startMousePos = rect().center();
            }
        }
        lastMousePos_ = startMousePos;
        lastUvPos_ = uvFromScreen(lastMousePos_);
        modalTransformActive_ = true;
        modalTransformMode_ = mode;
        if (!continuingChain)
        {
            modalTransformChainStartIslands_ = islands_;
        }
        modalTransformStartIslands_ = islands_;
        modalTransformStartUv_ = lastUvPos_;
        modalTransformStartMousePos_ = lastMousePos_;
        const QRectF bounds = activeTransformBounds();
        modalTransformCenter_ = activeTransformCenter();
        modalTransformBounds_ = bounds;
        modalTransformAngleDeg_ = 0.0;
        modalSlideAxis_ = SlideAxis::none;
        modalSlideAnchor_ = 0.0;
        modalSlideStartCoord_ = 0.0;
        modalReferenceVertex_.reset();
        modalReferenceEdge_.reset();
        if (pickMode_ == PickMode::vertex)
        {
            if (const std::optional<PickCandidate> active =
                    resolveComponent(activeVertex_);
                active
                && vertexSelectedAt(active->island, active->component))
            {
                modalReferenceVertex_ = std::make_pair(
                    active->island, active->component);
            }
            else if (const auto hit = hitVertex(lastMousePos_); hit)
            {
                const std::size_t islandIndex =
                    static_cast<std::size_t>(hit->first);
                const std::size_t vertexIndex =
                    static_cast<std::size_t>(hit->second);
                const std::vector<bool> mask = selectedVertexMask(islandIndex);
                if (vertexIndex < mask.size() && mask[vertexIndex])
                {
                    modalReferenceVertex_ = std::make_pair(islandIndex, vertexIndex);
                }
            }

            if (!modalReferenceVertex_)
            {
                qreal bestDistance = std::numeric_limits<qreal>::max();
                for (std::size_t islandIndex = 0; islandIndex < islands_.size(); ++islandIndex)
                {
                    const UvIslandState& island = islands_[islandIndex];
                    const std::vector<bool> mask = selectedVertexMask(islandIndex);
                    for (std::size_t vertexIndex = 0; vertexIndex < island.uv.size(); ++vertexIndex)
                    {
                        if (vertexIndex >= mask.size() || !mask[vertexIndex])
                        {
                            continue;
                        }

                        const qreal distance = QLineF(island.uv[vertexIndex], modalTransformStartUv_).length();
                        if (distance < bestDistance)
                        {
                            bestDistance = distance;
                            modalReferenceVertex_ = std::make_pair(islandIndex, vertexIndex);
                        }
                    }
                }
            }
        }
        else if (pickMode_ == PickMode::edge)
        {
            if (const std::optional<PickCandidate> active =
                    resolveComponent(activeEdge_);
                active && edgeSelectedAt(active->island, active->component))
            {
                modalReferenceEdge_ = std::make_pair(
                    active->island, active->component);
            }
        }
        else if (const std::optional<PickCandidate> facePivot =
                     resolveComponent(facePivotVertex_);
                 facePivot && islands_[facePivot->island].selected)
        {
            modalReferenceVertex_ = std::make_pair(
                facePivot->island, facePivot->component);
        }
        grabKeyboard();
        if (mode == ModalTransformMode::grabSlide && bounds.isValid())
        {
            const qreal distMinX = std::abs(modalTransformStartUv_.x() - bounds.left());
            const qreal distMaxX = std::abs(modalTransformStartUv_.x() - bounds.right());
            const qreal distMinY = std::abs(modalTransformStartUv_.y() - bounds.top());
            const qreal distMaxY = std::abs(modalTransformStartUv_.y() - bounds.bottom());
            const qreal bestX = std::min(distMinX, distMaxX);
            const qreal bestY = std::min(distMinY, distMaxY);
            if (bestX <= bestY)
            {
                modalSlideAxis_ = SlideAxis::x;
                if (distMinX <= distMaxX)
                {
                    modalSlideAnchor_ = bounds.right();
                    modalSlideStartCoord_ = bounds.left();
                }
                else
                {
                    modalSlideAnchor_ = bounds.left();
                    modalSlideStartCoord_ = bounds.right();
                }
            }
            else
            {
                modalSlideAxis_ = SlideAxis::y;
                if (distMinY <= distMaxY)
                {
                    modalSlideAnchor_ = bounds.bottom();
                    modalSlideStartCoord_ = bounds.top();
                }
                else
                {
                    modalSlideAnchor_ = bounds.top();
                    modalSlideStartCoord_ = bounds.bottom();
                }
            }
        }
        updateCanvasCursor();
        publishStatus();
        update();
    }

    void applyModalTransform(const QPointF& mousePos)
    {
        const QPointF curUv = uvFromScreen(mousePos);
        islands_ = modalTransformStartIslands_;

        switch (modalTransformMode_)
        {
        case ModalTransformMode::grab:
        {
            const QPointF delta = curUv - modalTransformStartUv_;
            forEachSelectedPoint([&](QPointF& p)
                {
                    p += delta;
                });
            break;
        }
        case ModalTransformMode::grabSlide:
        {
            if (modalSlideAxis_ == SlideAxis::none)
            {
                break;
            }

            const qreal currentCoord = (modalSlideAxis_ == SlideAxis::x) ? curUv.x() : curUv.y();
            const qreal startCoord = (modalSlideAxis_ == SlideAxis::x) ? modalTransformStartUv_.x() : modalTransformStartUv_.y();
            const qreal movedCoord = modalSlideStartCoord_ + (currentCoord - startCoord);
            const qreal denom = modalSlideStartCoord_ - modalSlideAnchor_;
            if (std::abs(denom) <= 1.0e-9)
            {
                break;
            }

            const qreal factor = std::clamp((movedCoord - modalSlideAnchor_) / denom, -16.0, 16.0);
            forEachSelectedPoint([&](QPointF& p)
                {
                    if (modalSlideAxis_ == SlideAxis::x)
                    {
                        p.setX(modalSlideAnchor_ + (p.x() - modalSlideAnchor_) * factor);
                    }
                    else
                    {
                        p.setY(modalSlideAnchor_ + (p.y() - modalSlideAnchor_) * factor);
                    }
                });
            break;
        }
        case ModalTransformMode::rotate:
        {
            constexpr qreal kRadToDeg = 57.295779513082320876;
            const qreal startAngle = std::atan2(modalTransformStartUv_.y() - modalTransformCenter_.y(), modalTransformStartUv_.x() - modalTransformCenter_.x());
            const qreal curAngle = std::atan2(curUv.y() - modalTransformCenter_.y(), curUv.x() - modalTransformCenter_.x());
            qreal degrees = (curAngle - startAngle) * kRadToDeg;
            if (snappingActive())
            {
                degrees = std::round(degrees / 10.0) * 10.0;
                const qreal axisSnap = std::round(degrees / 90.0) * 90.0;
                if (std::abs(degrees - axisSnap) <= 5.0)
                {
                    degrees = axisSnap;
                }
            }
            modalTransformAngleDeg_ = degrees;
            const qreal radians = degrees * (std::numbers::pi_v<qreal> / 180.0);
            const qreal cs = std::cos(radians);
            const qreal sn = std::sin(radians);
            forEachSelectedPoint([&](QPointF& p)
                {
                    const QPointF d = p - modalTransformCenter_;
                    p = QPointF(
                        modalTransformCenter_.x() + d.x() * cs - d.y() * sn,
                        modalTransformCenter_.y() + d.x() * sn + d.y() * cs);
                });
            break;
        }
        case ModalTransformMode::rotateTrackball:
        {
            const QPointF deltaPx = mousePos - modalTransformStartMousePos_;
            const qreal yaw = deltaPx.x() / 180.0;
            const qreal pitch = -deltaPx.y() / 180.0;
            modalTransformAngleDeg_ = 0.0;
            const qreal cy = std::cos(yaw);
            const qreal sy = std::sin(yaw);
            const qreal cx = std::cos(pitch);
            const qreal sx = std::sin(pitch);
            forEachSelectedPoint([&](QPointF& p)
                {
                    const QVector3D local(
                        static_cast<float>(p.x() - modalTransformCenter_.x()),
                        static_cast<float>(p.y() - modalTransformCenter_.y()),
                        0.0f);

                    const QVector3D yawRot(
                        static_cast<float>(local.x() * cy + local.z() * sy),
                        local.y(),
                        static_cast<float>(-local.x() * sy + local.z() * cy));
                    const QVector3D pitchRot(
                        yawRot.x(),
                        static_cast<float>(yawRot.y() * cx - yawRot.z() * sx),
                        static_cast<float>(yawRot.y() * sx + yawRot.z() * cx));

                    const qreal perspective = 1.0 / std::max<qreal>(0.35, 1.0 + static_cast<qreal>(pitchRot.z()) * 0.3);
                    p = QPointF(
                        modalTransformCenter_.x() + static_cast<qreal>(pitchRot.x()) * perspective,
                        modalTransformCenter_.y() + static_cast<qreal>(pitchRot.y()) * perspective);
                });
            break;
        }
        case ModalTransformMode::scale:
        {
            const qreal dy = static_cast<qreal>(mousePos.y() - modalTransformStartMousePos_.y());
            const qreal factor = std::pow(2.0, -dy / 180.0);
            modalTransformAngleDeg_ = 0.0;
            forEachSelectedPoint([&](QPointF& p)
                {
                    p = QPointF(
                        modalTransformCenter_.x() + (p.x() - modalTransformCenter_.x()) * factor,
                        modalTransformCenter_.y() + (p.y() - modalTransformCenter_.y()) * factor);
                });
            break;
        }
        case ModalTransformMode::none:
        default:
            break;
        }

        if (modalTransformMode_ != ModalTransformMode::rotate
            && modalTransformMode_ != ModalTransformMode::rotateTrackball)
        {
            applySnapToSelected();
        }
        publishInteractionProperties();
        emitLiveApply();
        updateCanvasCursor();
        update();
    }

    QRectF visibleUvBounds() const
    {
        return uvPointBounds({
            uvFromScreen(QPointF(0.0, 0.0)),
            uvFromScreen(QPointF(static_cast<qreal>(width()), 0.0)),
            uvFromScreen(QPointF(static_cast<qreal>(width()), static_cast<qreal>(height()))),
            uvFromScreen(QPointF(0.0, static_cast<qreal>(height())))
        });
    }

    QTransform uvToScreenTransform() const
    {
        const QPointF origin = screenFromUv({});
        const QPointF xAxis = screenFromUv(QPointF(1.0, 0.0)) - origin;
        const QPointF yAxis = screenFromUv(QPointF(0.0, 1.0)) - origin;
        return QTransform(
            xAxis.x(), xAxis.y(),
            yAxis.x(), yAxis.y(),
            origin.x(), origin.y());
    }

    QPolygonF textureTilePolygon(const int tx, const int ty) const
    {
        QPolygonF polygon{};
        polygon << screenFromUv(QPointF(tx, ty))
                << screenFromUv(QPointF(tx + 1, ty))
                << screenFromUv(QPointF(tx + 1, ty + 1))
                << screenFromUv(QPointF(tx, ty + 1));
        return polygon;
    }

    void drawTextureTiles(QPainter& p)
    {
        const QRectF uvBounds = visibleUvBounds();
        const int startX = static_cast<int>(std::floor(uvBounds.left())) - 1;
        const int endX = static_cast<int>(std::ceil(uvBounds.right())) + 1;
        const int startY = static_cast<int>(std::floor(uvBounds.top())) - 1;
        const int endY = static_cast<int>(std::ceil(uvBounds.bottom())) + 1;

        p.save();
        p.setOpacity(0.9);
        p.setTransform(uvToScreenTransform());
        for (int tx = startX; tx <= endX; ++tx)
        {
            for (int ty = startY; ty <= endY; ++ty)
            {
                p.drawImage(QRectF(tx, ty, 1.0, 1.0), previewTexture_);
            }
        }
        if (repeatTileGridEnabled_)
        {
            p.resetTransform();
            p.setOpacity(1.0);
            p.setPen(QPen(QColor(255, 255, 255, 34), 1.0));
            for (int tx = startX; tx <= endX; ++tx)
            {
                for (int ty = startY; ty <= endY; ++ty)
                {
                    p.drawPolygon(textureTilePolygon(tx, ty));
                }
            }
        }
        p.restore();
    }

    void drawSingleTextureTile(QPainter& p)
    {
        p.save();
        p.setOpacity(0.9);
        p.setTransform(uvToScreenTransform());
        p.drawImage(QRectF(0.0, 0.0, 1.0, 1.0), previewTexture_);
        p.resetTransform();
        p.setOpacity(1.0);
        p.setPen(QPen(QColor(255, 255, 255, 34), 1.0));
        p.drawPolygon(textureTilePolygon(0, 0));
        p.restore();
    }

    void drawGrid(QPainter& p)
    {
        const QRectF uvBounds = visibleUvBounds();
        const QPointF uvMin(uvBounds.left(), uvBounds.top());
        const QPointF uvMax(uvBounds.right(), uvBounds.bottom());

        struct LatticeBounds
        {
            std::int64_t startX{};
            std::int64_t endX{};
            std::int64_t startY{};
            std::int64_t endY{};
            std::size_t countX{};
            std::size_t countY{};
        };
        constexpr std::size_t maximumGridCandidates = 65'536;
        const auto checkedLatticeBounds = [&](const qreal step)
            -> std::optional<LatticeBounds>
        {
            if ( !std::isfinite(step) || step <= 0.0
                || !std::isfinite(uvMin.x()) || !std::isfinite(uvMax.x())
                || !std::isfinite(uvMin.y()) || !std::isfinite(uvMax.y()) )
            {
                return std::nullopt;
            }
            const qreal startX = std::floor(uvMin.x() / step) - 1.0;
            const qreal endX = std::ceil(uvMax.x() / step) + 1.0;
            const qreal startY = std::floor(uvMin.y() / step) - 1.0;
            const qreal endY = std::ceil(uvMax.y() / step) + 1.0;
            // Keep lattice indices inside the exact-integer range of qreal
            // before converting them. This also avoids undefined casts near
            // int64 limits on platforms where long double is just double.
            constexpr qreal maximumSafeIndex = 9'007'199'254'740'000.0;
            constexpr qreal minimumSafeIndex = -maximumSafeIndex;
            if ( !std::isfinite(startX) || !std::isfinite(endX)
                || !std::isfinite(startY) || !std::isfinite(endY)
                || startX < minimumSafeIndex || endX > maximumSafeIndex
                || startY < minimumSafeIndex || endY > maximumSafeIndex )
            {
                return std::nullopt;
            }

            const std::int64_t firstX = static_cast<std::int64_t>(startX);
            const std::int64_t lastX = static_cast<std::int64_t>(endX);
            const std::int64_t firstY = static_cast<std::int64_t>(startY);
            const std::int64_t lastY = static_cast<std::int64_t>(endY);
            const long double countX = static_cast<long double>(lastX)
                - static_cast<long double>(firstX) + 1.0L;
            const long double countY = static_cast<long double>(lastY)
                - static_cast<long double>(firstY) + 1.0L;
            if ( countX <= 0.0L || countY <= 0.0L
                || countX > static_cast<long double>(maximumGridCandidates)
                || countY > static_cast<long double>(maximumGridCandidates) )
            {
                return std::nullopt;
            }
            return LatticeBounds{
                firstX,
                lastX,
                firstY,
                lastY,
                static_cast<std::size_t>(countX),
                static_cast<std::size_t>(countY) };
        };

        auto drawStep = [&](const qreal step, const QColor& color, const qreal width)
        {
            const std::optional<LatticeBounds> bounds = checkedLatticeBounds(step);
            if ( !bounds
                || bounds->countX + bounds->countY > maximumGridCandidates )
            {
                return;
            }
            p.setPen(QPen(color, width));
            for ( std::int64_t x = bounds->startX; x <= bounds->endX; ++x )
            {
                const QPointF a = screenFromUv(QPointF(static_cast<qreal>(x) * step, uvMin.y()));
                const QPointF b = screenFromUv(QPointF(static_cast<qreal>(x) * step, uvMax.y()));
                p.drawLine(a, b);
            }
            for ( std::int64_t y = bounds->startY; y <= bounds->endY; ++y )
            {
                const QPointF a = screenFromUv(QPointF(uvMin.x(), static_cast<qreal>(y) * step));
                const QPointF b = screenFromUv(QPointF(uvMax.x(), static_cast<qreal>(y) * step));
                p.drawLine(a, b);
            }
        };

        const QColor mainColor = property("sed.uvGridMainColor").value<QColor>();
        const QColor subColor = property("sed.uvGridSubColor").value<QColor>();
        const QColor axisUColor = property("sed.uvGridAxisUColor").value<QColor>();
        const QColor axisVColor = property("sed.uvGridAxisVColor").value<QColor>();
        if ( subGridEnabled_ )
        {
            const qreal minorStep = gridStep_ / 4.0;
            const QColor minorColor = subColor.isValid()
                ? subColor : QColor(48, 58, 74);
            if ( subGridMinorDots_ )
            {
                const std::optional<LatticeBounds> bounds =
                    checkedLatticeBounds(minorStep);
                const long double candidateCount = bounds
                    ? static_cast<long double>(bounds->countX)
                        * static_cast<long double>(bounds->countY)
                    : static_cast<long double>(maximumGridCandidates) + 1.0L;
                const QPointF origin = screenFromUv(QPointF(0.0, 0.0));
                const QPointF uStep = screenFromUv(QPointF(minorStep, 0.0))
                    - origin;
                const QPointF vStep = screenFromUv(QPointF(0.0, minorStep))
                    - origin;
                const qreal minimumDotSpacing = std::max(
                    2.0, subGridDotSizePx_ + 1.0);
                const qreal projectedUSpacing = std::hypot(
                    uStep.x(), uStep.y());
                const qreal projectedVSpacing = std::hypot(
                    vStep.x(), vStep.y());
                if ( bounds
                    && candidateCount <= static_cast<long double>(maximumGridCandidates)
                    && std::isfinite(projectedUSpacing)
                    && std::isfinite(projectedVSpacing)
                    && projectedUSpacing >= minimumDotSpacing
                    && projectedVSpacing >= minimumDotSpacing )
                {
                    p.save();
                    p.setRenderHint(QPainter::Antialiasing, true);
                    p.setPen(Qt::NoPen);
                    p.setBrush(minorColor);
                    const qreal radius = subGridDotSizePx_ / 2.0;
                    for ( std::int64_t x = bounds->startX; x <= bounds->endX; ++x )
                    {
                        for ( std::int64_t y = bounds->startY; y <= bounds->endY; ++y )
                        {
                            const QPointF center = screenFromUv(QPointF(
                                static_cast<qreal>(x) * minorStep,
                                static_cast<qreal>(y) * minorStep));
                            if ( std::isfinite(center.x()) && std::isfinite(center.y()) )
                            {
                                p.drawEllipse(center, radius, radius);
                            }
                        }
                    }
                    p.restore();
                }
            }
            else
            {
                drawStep(minorStep, minorColor, 1.0);
            }
        }
        drawStep(gridStep_,
            mainColor.isValid() ? mainColor : QColor(66, 78, 96), 1.0);
        p.setPen(QPen(axisUColor.isValid() ? axisUColor : QColor(160, 78, 78), 1.5));
        p.drawLine(screenFromUv(QPointF(uvMin.x(), 0.0)), screenFromUv(QPointF(uvMax.x(), 0.0)));
        p.setPen(QPen(axisVColor.isValid() ? axisVColor : QColor(78, 160, 112), 1.5));
        p.drawLine(screenFromUv(QPointF(0.0, uvMin.y())), screenFromUv(QPointF(0.0, uvMax.y())));
    }

    void drawIslandCentroids(QPainter& p)
    {
        p.save();
        if (pickMode_ != PickMode::face)
        {
            if (hasSelectedIslands())
            {
                const QPointF centerPx = screenFromUv(selectionCenterFallback());
                p.setPen(QPen(QColor(232, 238, 246, 220), 1.2));
                p.setBrush(QColor(36, 43, 55, 150));
                p.drawEllipse(centerPx, 6.0, 6.0);
            }
            p.restore();
            return;
        }
        const std::optional<std::size_t> activeIndex = activeSelectedIslandIndex();
        auto drawCentroid = [&](const std::size_t i)
        {
            const UvIslandState& island = islands_[i];
            if (island.uv.size() < 3)
            {
                return;
            }

            const QPointF centerPx = screenFromUv(islandCentroid(island));
            const bool isActive = pickMode_ == PickMode::face
                && activeIndex && (*activeIndex == i);
            const bool isSelected = islandSelectedInCurrentMode(i);
            const QColor stroke = isActive
                ? QColor(255, 182, 86, 235)
                : (isSelected ? QColor(232, 238, 246, 180) : QColor(168, 180, 196, 120));
            const QColor fill = isActive
                ? QColor(255, 182, 86, 220)
                : (isSelected ? QColor(232, 238, 246, 132) : QColor(168, 180, 196, 84));
            p.setPen(QPen(stroke, isActive ? 1.3 : 1.0));
            p.setBrush(fill);
            const qreal radius = isActive ? 4.0 : 3.2;
            p.drawEllipse(centerPx, radius, radius);
        };
        for (std::size_t i = 0; i < islands_.size(); ++i)
        {
            if (!activeIndex || i != *activeIndex)
            {
                drawCentroid(i);
            }
        }
        // The primary face marker must stay visible when identical UV faces
        // overlap. Draw it after every secondary centroid regardless of
        // island storage/paint order.
        if (activeIndex && *activeIndex < islands_.size())
        {
            drawCentroid(*activeIndex);
        }
        p.restore();
    }

    void drawExactComponentMarker(
        QPainter& painter,
        const std::optional<PickCandidate>& activeVertex,
        const std::optional<PickCandidate>& activeEdge,
        const std::optional<PickCandidate>& facePivotVertex)
    {
        painter.save();
        const QColor activeColor(244, 76, 76, 245);
        if (pickMode_ == PickMode::edge
            && activeEdge
            && activeEdge->island < islands_.size()
            && edgeSelectedAt(activeEdge->island, activeEdge->component))
        {
            const UvIslandState& island = islands_[activeEdge->island];
            if (activeEdge->component < island.uv.size())
            {
                const std::size_t next =
                    (activeEdge->component + 1u) % island.uv.size();
                painter.setPen(QPen(activeColor, 3.4));
                painter.setBrush(Qt::NoBrush);
                painter.drawLine(
                    screenFromUv(island.uv[activeEdge->component]),
                    screenFromUv(island.uv[next]));
            }
        }

        const std::optional<PickCandidate> activePoint =
            pickMode_ == PickMode::vertex ? activeVertex
            : (pickMode_ == PickMode::face ? facePivotVertex : std::nullopt);
        if (activePoint && activePoint->island < islands_.size())
        {
            const UvIslandState& island = islands_[activePoint->island];
            if (activePoint->component < island.uv.size())
            {
                painter.setPen(Qt::NoPen);
                painter.setBrush(activeColor);
                painter.drawEllipse(
                    screenFromUv(island.uv[activePoint->component]),
                    5.0,
                    5.0);
            }
        }
        painter.restore();
    }

    void drawTransformOverlay(QPainter& p)
    {
        if (!modalTransformActive_)
        {
            return;
        }

        const QPointF centerPx = screenFromUv(modalTransformCenter_);
        const QPointF startPx = screenFromUv(modalTransformStartUv_);
        const QPointF currentPx = lastMousePos_;
        p.save();

        switch (modalTransformMode_)
        {
        case ModalTransformMode::grab:
            p.setPen(QPen(QColor(121, 184, 255, 210), 1.5, Qt::DashLine));
            p.drawLine(startPx, currentPx);
            break;
        case ModalTransformMode::grabSlide:
            p.setPen(QPen(QColor(116, 228, 178, 210), 1.5, Qt::DashLine));
            if (modalSlideAxis_ == SlideAxis::x)
            {
                p.drawLine(QPointF(0.0, centerPx.y()), QPointF(static_cast<qreal>(width()), centerPx.y()));
            }
            else if (modalSlideAxis_ == SlideAxis::y)
            {
                p.drawLine(QPointF(centerPx.x(), 0.0), QPointF(centerPx.x(), static_cast<qreal>(height())));
            }
            p.drawLine(centerPx, currentPx);
            break;
        case ModalTransformMode::rotate:
        {
            const qreal radius = std::clamp(QLineF(centerPx, startPx).length(), 12.0, 24.0);
            p.setBrush(Qt::NoBrush);
            p.setPen(QPen(QColor(255, 255, 255, 10), 1.0, Qt::DashLine));
            p.drawEllipse(centerPx, radius, radius);
            if (QLineF(centerPx, startPx).length() > 1.0)
            {
                const QPointF dir = (startPx - centerPx) / QLineF(centerPx, startPx).length();
                p.setPen(QPen(QColor(112, 184, 255, 150), 1.1));
                p.drawLine(centerPx, centerPx + dir * radius);
            }
            p.setPen(QPen(QColor(112, 255, 172, 220), 1.7));
            p.drawLine(centerPx, currentPx);
            p.setPen(QColor(232, 238, 246, 210));
            QString angleText = ui::formatUiNumber(modalTransformAngleDeg_, 'f', 1);
            if ( !std::signbit(modalTransformAngleDeg_) )
            {
                angleText.prepend(ui::uiNumericLocale().positiveSign());
            }
            p.drawText(currentPx + QPointF(10.0, -10.0), angleText + QStringLiteral(" deg"));
            break;
        }
        case ModalTransformMode::rotateTrackball:
            p.setPen(QPen(QColor(88, 148, 255, 200), 1.2, Qt::DashLine));
            p.drawLine(QPointF(0.0, centerPx.y()), QPointF(static_cast<qreal>(width()), centerPx.y()));
            p.drawLine(QPointF(centerPx.x(), 0.0), QPointF(centerPx.x(), static_cast<qreal>(height())));
            p.setPen(QPen(QColor(112, 255, 172, 220), 1.6));
            p.drawLine(centerPx, currentPx);
            break;
        case ModalTransformMode::scale:
            p.setPen(QPen(QColor(214, 164, 92, 220), 1.5, Qt::DashLine));
            p.drawLine(centerPx, currentPx);
            break;
        case ModalTransformMode::none:
        default:
            break;
        }

        p.restore();
    }

    void updateCanvasCursor()
    {
        auto trackballCursor = []() -> QCursor
        {
            QPixmap pix(32, 32);
            pix.fill(Qt::transparent);
            QPainter painter(&pix);
            painter.setRenderHint(QPainter::Antialiasing, true);

            painter.setPen(QPen(QColor(70, 220, 120), 2.0));
            painter.drawLine(16, 4, 16, 28);
            painter.drawLine(12, 8, 16, 4);
            painter.drawLine(20, 8, 16, 4);
            painter.drawLine(12, 24, 16, 28);
            painter.drawLine(20, 24, 16, 28);

            painter.setPen(QPen(QColor(236, 92, 92), 2.0));
            painter.drawLine(4, 16, 28, 16);
            painter.drawLine(8, 12, 4, 16);
            painter.drawLine(8, 20, 4, 16);
            painter.drawLine(24, 12, 28, 16);
            painter.drawLine(24, 20, 28, 16);

            painter.setPen(QPen(QColor(255, 255, 255, 160), 1.25));
            painter.setBrush(QColor(255, 255, 255, 60));
            painter.drawEllipse(QPointF(16.0, 16.0), 4.5, 4.5);
            painter.end();
            return QCursor(pix, 16, 16);
        };

        std::optional<QCursor> cursor{};
        if (modalTransformActive_)
        {
            switch (modalTransformMode_)
            {
            case ModalTransformMode::grab:
                cursor = QCursor(Qt::SizeAllCursor);
                break;
            case ModalTransformMode::grabSlide:
                cursor = QCursor((modalSlideAxis_ == SlideAxis::x) ? Qt::SizeHorCursor : Qt::SizeVerCursor);
                break;
            case ModalTransformMode::rotate:
                cursor = QCursor(Qt::SizeVerCursor);
                break;
            case ModalTransformMode::rotateTrackball:
                cursor = trackballCursor();
                break;
            case ModalTransformMode::scale:
                cursor = QCursor(Qt::SizeFDiagCursor);
                break;
            case ModalTransformMode::none:
            default:
                break;
            }
        }
        else if (panning_)
        {
            cursor = QCursor(Qt::ClosedHandCursor);
        }
        else if (selectionRectActive_)
        {
            cursor = QCursor(Qt::CrossCursor);
        }
        setCursor(cursor.value_or(QCursor(Qt::ArrowCursor)));
    }

    void clearPointerInteractionState()
    {
        panning_ = false;
        panButton_ = Qt::NoButton;
        leftSelectionPending_ = false;
        selectionRectActive_ = false;
        selectionRectPx_ = QRect();
        selectionRectModifiers_ = Qt::NoModifier;
        resetPickCycle();
        updateCanvasCursor();
    }

    void applySnapToSelected()
    {
        const std::optional<QPointF> delta = computeSnapDelta();
        if (!delta)
        {
            return;
        }

        forEachSelectedPoint([&](QPointF& p)
            {
                p += *delta;
            });
    }

    std::optional<QPointF> computeSnapDelta() const
    {
        if (!snappingActive() || !hasSelectedIslands())
        {
            return std::nullopt;
        }

        std::vector<std::vector<bool>> movingVertexMasks{};
        movingVertexMasks.reserve(islands_.size());
        for (std::size_t islandIndex = 0; islandIndex < islands_.size(); ++islandIndex)
        {
            movingVertexMasks.push_back(selectedVertexMask(islandIndex));
        }
        UvSnapGeometry snapGeometry = uvCollectSnapGeometry(
            islands_, movingVertexMasks, edgeSelected_,
            pickMode_ == PickMode::edge);
        std::vector<QPointF>& selectedPoints = snapGeometry.movingPoints;
        const std::vector<QPointF>& otherPoints = snapGeometry.targetPoints;
        const std::vector<QPointF>& movingSurfacePoints =
            snapGeometry.movingSurfacePoints;
        const std::vector<QPointF>& targetSurfacePoints =
            snapGeometry.targetSurfacePoints;
        std::vector<UvSnapEdge>& movingEdges = snapGeometry.movingEdges;
        const std::vector<UvSnapEdge>& targetEdges = snapGeometry.targetEdges;

        if (selectedPoints.empty())
        {
            return std::nullopt;
        }

        const bool vertexReferenceSnapActive = modalTransformActive_
            && modalReferenceVertex_.has_value();

        if (vertexReferenceSnapActive)
        {
            const auto [islandIndex, vertexIndex] = *modalReferenceVertex_;
            if (islandIndex < islands_.size() && vertexIndex < islands_[islandIndex].uv.size())
            {
                selectedPoints.assign(1, islands_[islandIndex].uv[vertexIndex]);
            }
        }

        const bool edgeReferenceSnapActive = modalTransformActive_
            && pickMode_ == PickMode::edge
            && modalReferenceEdge_.has_value();
        if (edgeReferenceSnapActive)
        {
            const auto [islandIndex, edgeIndex] = *modalReferenceEdge_;
            if (islandIndex < islands_.size()
                && edgeIndex < islands_[islandIndex].uv.size())
            {
                const UvIslandState& island = islands_[islandIndex];
                movingEdges.assign(1, UvSnapEdge{
                    island.uv[edgeIndex],
                    island.uv[(edgeIndex + 1u) % island.uv.size()] });
            }
        }

        const qreal vertexThreshold = 14.0 / zoom_;
        const qreal edgeResidualThreshold =
            std::max<qreal>(3.0 / zoom_, 1.0e-6);
        if (!movingEdges.empty() && !targetEdges.empty())
        {
            if (const std::optional<QPointF> edgeDelta =
                    uvNearestEdgeSnapDelta(
                        movingEdges,
                        targetEdges,
                        vertexThreshold,
                        edgeResidualThreshold))
            {
                return edgeDelta;
            }
        }

        if (const std::optional<QPointF> vertexDelta =
                uvNearestPointSnapDelta(
                    selectedPoints, otherPoints, vertexThreshold))
        {
            return vertexDelta;
        }

        if (const std::optional<QPointF> surfacePointDelta =
                uvNearestPointSnapDelta(
                    movingSurfacePoints,
                    targetSurfacePoints,
                    vertexThreshold))
        {
            return surfacePointDelta;
        }

        const std::optional<QPointF> exactPivot = activeComponentPivot();
        const std::vector<QPointF> gridReferencePoints = exactPivot
            ? std::vector<QPointF>{ *exactPivot }
            : selectedPoints;
        // The visible lattice is the unconditional fallback. With Snap active,
        // a drag never publishes a free pointer position merely because it is
        // farther than the feature-acquisition threshold.
        return uvNearestVisibleGridSnapDelta(
            gridReferencePoints, gridStep_, subGridEnabled_);
    }

    std::vector<UvIslandState> islands_{};
    std::vector<std::vector<bool>> vertexSelected_{};
    std::vector<std::vector<bool>> edgeSelected_{};
    QImage previewTexture_{};
    qreal zoom_{48.0};
    qreal zoomMaximum_{kUvEditorZoomMaximumDefault};
    QPointF pan_{320.0, 240.0};
    bool viewportFacingViewEnabled_{};
    bool viewMirrorX_{};
    qreal viewRotationRadians_{};
    PickMode pickMode_{PickMode::face};
    std::optional<std::size_t> activeIslandIndex_{};
    bool panning_{};
    Qt::MouseButton panButton_{Qt::NoButton};
    bool leftSelectionPending_{};
    bool selectionRectActive_{};
    QRect selectionRectPx_{};
    QPoint selectionRectStart_{};
    Qt::KeyboardModifiers selectionRectModifiers_{};
    QPoint lastMousePos_{};
    bool pointerPositionKnown_{};
    QPoint panLastMousePos_{};
    QPointF lastUvPos_{};
    bool modalTransformActive_{};
    ModalTransformMode modalTransformMode_{ModalTransformMode::none};
    std::vector<UvIslandState> modalTransformStartIslands_{};
    std::vector<UvIslandState> modalTransformChainStartIslands_{};
    QPointF modalTransformStartUv_{};
    QPoint modalTransformStartMousePos_{};
    QPointF modalTransformCenter_{};
    QRectF modalTransformBounds_{};
    qreal modalTransformAngleDeg_{0.0};
    SlideAxis modalSlideAxis_{SlideAxis::none};
    qreal modalSlideAnchor_{0.0};
    qreal modalSlideStartCoord_{0.0};
    std::optional<std::pair<std::size_t, std::size_t>> modalReferenceVertex_{};
    std::optional<std::pair<std::size_t, std::size_t>> modalReferenceEdge_{};
    std::optional<ComponentRef> activeVertex_{};
    std::optional<ComponentRef> activeEdge_{};
    std::optional<ComponentRef> facePivotVertex_{};
    std::vector<PickCandidate> pickCycleCandidates_{};
    std::size_t pickCycleNextIndex_{};
    QPoint pickCyclePosition_{};
    PickMode pickCycleMode_{PickMode::face};
    Qt::KeyboardModifiers pickCycleModifiers_{Qt::NoModifier};
    QElapsedTimer pickCycleTimer_{};
    std::vector<std::vector<bool>> faceModeCachedVertexSelected_{};
    std::vector<std::vector<bool>> faceModeCachedEdgeSelected_{};
    std::vector<std::vector<bool>> faceModeCachedSelectedVertices_{};
    bool faceModeHasComponentCache_{};
    std::vector<std::vector<bool>> componentModeCachedVertexSelected_{};
    std::vector<std::vector<bool>> componentModeCachedEdgeSelected_{};
    std::vector<std::vector<bool>> componentModeCachedSelectedVertices_{};
    bool componentModeHasCache_{};
    bool persistentSnapEnabled_{};
    bool repeatTexturePreviewEnabled_{true};
    bool repeatTileGridEnabled_{};
    qreal gridStep_{0.25};
    bool subGridEnabled_{};
    bool subGridMinorDots_{};
    qreal subGridDotSizePx_{2.0};
    bool fitToContentPending_{};
    std::function<void(const std::vector<UvIslandState>&)> onLiveApply_{};
    std::function<void(const QString&)> onStatusText_{};
    std::function<void(const std::vector<UvIslandState>&, const std::vector<UvIslandState>&)> onModalCommit_{};
    std::function<void()> onSettledWorkspaceChanged_{};
};

class UvEditorPanel final : public QWidget
{
public:
    UvEditorPanel(
        std::function<void(std::vector<UvIslandState>&, const SmartUvProjectOptions&, const QSize&)> onSmartProject,
        std::function<bool(
            std::vector<UvIslandState>&,
            const std::vector<bool>&,
            std::optional<SurfaceRef>,
            FollowActiveQuadsMode,
            FollowActiveQuadsReport*)> onFollowActiveQuads,
        std::function<void(const std::vector<UvIslandState>&)> onPreview,
        std::function<void(const std::vector<UvIslandState>&, const std::vector<UvIslandState>&, const QString&)> onCommit,
        std::function<void(bool)> onViewportFacingChanged,
        QAction* autoAlignAction,
        QAction* moveAction,
        QAction* rotateAction,
        QAction* scaleAction,
        QAction* stitchFromReferenceAction,
        QAction* stitchConnectedAction,
        QAction* copyFromReferenceAction,
        QWidget* parent = nullptr)
        : QWidget(parent)
        , onSmartProject_(std::move(onSmartProject))
        , onFollowActiveQuads_(std::move(onFollowActiveQuads))
        , onPreview_(std::move(onPreview))
        , onCommit_(std::move(onCommit))
        , onViewportFacingChanged_(std::move(onViewportFacingChanged))
    {
        setObjectName("uvEditorPanel");

        setFocusPolicy(Qt::StrongFocus);
        installEventFilter(this);
        setStyleSheet(
            "QPushButton, QToolButton {"
            " background-color: rgba(42, 55, 76, 0.88);"
            " border: 1px solid rgba(92, 118, 156, 0.72);"
            " border-radius: 6px;"
            " color: #eef3fb;"
            " font-size: 12px;"
            " font-weight: 500;"
            " padding: 0 10px;"
            "}"
            "QPushButton:hover, QToolButton:hover {"
            " background-color: rgba(56, 73, 100, 0.95);"
            " border-color: rgba(122, 150, 190, 0.92);"
            "}"
            "QPushButton:pressed, QToolButton:pressed, QToolButton:checked {"
            " background-color: rgba(64, 90, 128, 0.95);"
            " border-color: rgba(132, 172, 224, 0.96);"
            "}"
            "QPushButton:disabled, QToolButton:disabled {"
            " color: rgba(220, 228, 240, 0.48);"
            " background-color: rgba(32, 40, 52, 0.62);"
            " border-color: rgba(86, 102, 126, 0.38);"
            "}");

        auto* layout = new QVBoxLayout(this);
        layout->setContentsMargins(8, 6, 8, 8);
        layout->setSpacing(6);

        controlsHost_ = new QWidget(this);
        auto* controlsLayout = new QVBoxLayout(controlsHost_);
        controlsLayout->setContentsMargins(0, 0, 0, 0);
        controlsLayout->setSpacing(6);
        controlsLayout->setAlignment(Qt::AlignTop);

        auto makeSectionRow = [this, controlsLayout](const QString& title) -> FlowLayout*
        {
            auto* section = new QWidget(controlsHost_);
            auto* sectionLayout = new QVBoxLayout(section);
            sectionLayout->setContentsMargins(0, 4, 0, 4);
            sectionLayout->setSpacing(4);

            auto* label = new QLabel(title, section);
            label->setStyleSheet("color:#edf3fb;font-weight:850;font-size:16px;letter-spacing:0.18px;");
            sectionLayout->addWidget(label);

            auto* buttonsRow = new QWidget(section);
            auto* rowLayout = new FlowLayout(buttonsRow, 0, 4, 4);
            sectionLayout->addWidget(buttonsRow);

            controlsLayout->addWidget(section);
            controlRows_.push_back(section);
            return rowLayout;
        };

        auto addButton = [this](
                             QLayout* rowLayout,
                             const QString& text,
                             const QString& objectName,
                             const QString& toolTip) -> QPushButton*
        {
            auto* button = new QPushButton(text, this);
            button->setObjectName(objectName);
            button->setToolTip(toolTip);
            button->setFixedHeight(26);
            button->setCursor(Qt::PointingHandCursor);
            button->setFocusPolicy(Qt::NoFocus);
            rowLayout->addWidget(button);
            actionButtons_.push_back(button);
            return button;
        };

        auto addActionButton = [this](
                                   QLayout* rowLayout,
                                   QAction* action,
                                   const QString& text,
                                   const QString& objectName) -> QToolButton*
        {
            auto* button = new QToolButton(this);
            button->setObjectName(objectName);
            button->setText(text);
            button->setToolButtonStyle(Qt::ToolButtonTextOnly);
            button->setAutoRaise(false);
            button->setFixedHeight(26);
            button->setCursor(Qt::PointingHandCursor);
            button->setFocusPolicy(Qt::NoFocus);
            if (action)
            {
                button->setToolTip(action->toolTip());
                button->setEnabled(action->isEnabled());
                connect(action, &QAction::changed, button, [button, action]()
                    {
                        if (!button || !action)
                        {
                            return;
                        }
                        if (button->isEnabled() != action->isEnabled())
                        {
                            button->setEnabled(action->isEnabled());
                        }
                        if (button->toolTip() != action->toolTip())
                        {
                            button->setToolTip(action->toolTip());
                        }
                    });
                connect(button, &QToolButton::clicked, this, [this, action]()
                    {
                        if (canvas_ && canvas_->hasActiveModalTransform())
                        {
                            return;
                        }
                        if (action && action->isEnabled())
                        {
                            action->trigger();
                        }
                    });
            }
            else
            {
                button->setEnabled(false);
            }
            rowLayout->addWidget(button);
            actionButtons_.push_back(button);
            return button;
        };

        auto normalizeButtonMetrics = [this]()
        {
            int maxWidth = 0;
            for (QAbstractButton* button : actionButtons_)
            {
                if (button)
                {
                    maxWidth = std::max(maxWidth, button->sizeHint().width());
                }
            }

            maxWidth = std::max(maxWidth, 132);
            for (QAbstractButton* button : actionButtons_)
            {
                if (button)
                {
                    button->setFixedSize(maxWidth, 28);
                }
            }
        };

        auto* unwrapRow = makeSectionRow("Unwrap");
        auto* unwrapButton = addButton(
            unwrapRow,
            "Smart UV Project...",
            "uvEditorSmartProjectButton",
            "Project selected surface geometry into UV islands using configurable angle, orientation, aspect, margin, and scale options.");
        auto* followActiveButton = addButton(
            unwrapRow,
            "Follow Active Quads...",
            "uvEditorFollowActiveQuadsButton",
            "Extend the active quad's UV layout through connected selected quads using Even, Length, or Length Average spacing.");
        auto* autoAlignButton = addActionButton(
            unwrapRow,
            autoAlignAction,
            "Auto Align",
            "uvEditorAutoAlignButton");
        Q_UNUSED(autoAlignButton);
        auto* packButton = addButton(
            unwrapRow,
            "Pack",
            "uvEditorPackButton",
            "Arrange selected UV islands into compact non-overlapping rows without changing their shape, rotation, or scale.");
        auto* stitchButton = addActionButton(
            unwrapRow,
            stitchFromReferenceAction,
            "Stitch Ref",
            "uvEditorStitchReferenceButton");
        auto* chainButton = addActionButton(
            unwrapRow,
            stitchConnectedAction,
            "Stitch Chain",
            "uvEditorStitchConnectedButton");
        auto* copyButton = addActionButton(
            unwrapRow,
            copyFromReferenceAction,
            "Copy Ref",
            "uvEditorCopyReferenceButton");

        auto* transformRow = makeSectionRow("Transform");
        auto* rotateLeftButton = addButton(
            transformRow,
            "Rotate -90",
            "uvEditorRotateLeftButton",
            "Rotate the selected UVs 90 degrees counterclockwise around the active selection pivot.");
        auto* rotateRightButton = addButton(
            transformRow,
            "Rotate +90",
            "uvEditorRotateRightButton",
            "Rotate the selected UVs 90 degrees clockwise around the active selection pivot.");
        auto* scaleFactorField = new QWidget(this);
        scaleFactorField->setObjectName(QStringLiteral(
            "uvEditorScaleFactorField"));
        auto* scaleFactorLayout = new QHBoxLayout(scaleFactorField);
        scaleFactorLayout->setContentsMargins(3, 0, 3, 0);
        scaleFactorLayout->setSpacing(3);
        auto* scaleFactorLabel = new QLabel(
            QStringLiteral("Scale ×"), scaleFactorField);
        scaleFactorLabel->setObjectName(QStringLiteral(
            "uvEditorScaleFactorLabel"));
        scaleFactorLabel->setStyleSheet(
            QStringLiteral("color:#cfd7e3;font-weight:600;"));
        scaleFactorLayout->addWidget(scaleFactorLabel);
        auto* scaleFactorSpin = new CompactDoubleSpinBox(scaleFactorField);
        scaleFactorSpin->setObjectName(QStringLiteral(
            "uvEditorScaleFactorSpin"));
        scaleFactorSpin->setRange(0.01, 1'000.0);
        scaleFactorSpin->setDecimals(3);
        scaleFactorSpin->setSingleStep(0.1);
        scaleFactorSpin->setValue(1.0);
        scaleFactorSpin->setLocale(ui::uiNumericLocale());
        scaleFactorSpin->setPreferredCompactSize(24, 62);
        scaleFactorSpin->setAlignment(Qt::AlignRight);
        const auto refreshScaleFactorToolTip = [scaleFactorSpin]()
        {
            scaleFactorSpin->setToolTip(QStringLiteral(
                "Enter any uniform scale multiplier from %1 to %2. Values such as %3 and %4 replace the old fixed shortcuts.")
                .arg(ui::formatUiNumber(0.01), ui::formatUiNumber(1000.0),
                    ui::formatUiNumber(0.5), ui::formatUiNumber(2.0)));
        };
        refreshScaleFactorToolTip();
        ui::onUiNumericFormatChanged(scaleFactorSpin, refreshScaleFactorToolTip);
        scaleFactorLayout->addWidget(scaleFactorSpin);
        auto* applyScaleFactorButton = new QPushButton(
            QStringLiteral("Apply"), scaleFactorField);
        applyScaleFactorButton->setObjectName(QStringLiteral(
            "uvEditorApplyScaleFactorButton"));
        applyScaleFactorButton->setToolTip(QStringLiteral(
            "Scale the selected UVs by this multiplier around the active vertex, active edge midpoint, face pivot, or selection center."));
        applyScaleFactorButton->setFixedSize(60, 24);
        applyScaleFactorButton->setCursor(Qt::PointingHandCursor);
        applyScaleFactorButton->setFocusPolicy(Qt::NoFocus);
        scaleFactorLayout->addWidget(applyScaleFactorButton);
        scaleFactorField->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
        scaleFactorField->setFixedSize(scaleFactorField->sizeHint());
        auto* mirrorXButton = addButton(
            transformRow,
            "Mirror X",
            "uvEditorMirrorXButton",
            "Mirror the selected UVs horizontally around the active selection pivot.");
        auto* mirrorYButton = addButton(
            transformRow,
            "Mirror Y",
            "uvEditorMirrorYButton",
            "Mirror the selected UVs vertically around the active selection pivot.");
        auto* fitButton = addButton(
            transformRow,
            "Fit",
            "uvEditorFitButton",
            "Center and zoom the UV view to show all islands. This does not change stored UVs.");
        auto* selectAllButton = addButton(
            transformRow,
            "Select all",
            "uvEditorSelectAllButton",
            "Select every face, edge, or vertex in the current UV selection mode.");
        // Keep arbitrary numeric scaling as the final Transform control so
        // the common one-click operations remain aligned first.
        transformRow->addWidget(scaleFactorField);

        auto* editRow = makeSectionRow("Edit");
        auto* moveButton = addActionButton(
            editRow,
            moveAction,
            "Move...",
            "uvEditorMoveButton");
        Q_UNUSED(moveButton);
        auto* rotateButton = addActionButton(
            editRow,
            rotateAction,
            "Rotate...",
            "uvEditorRotateButton");
        Q_UNUSED(rotateButton);
        auto* scaleButton = addActionButton(
            editRow,
            scaleAction,
            "Scale...",
            "uvEditorScaleButton");
        Q_UNUSED(scaleButton);

        auto* previewRow = makeSectionRow("Preview");
        auto* repeatButton = new QToolButton(this);
        repeatButton->setObjectName("uvEditorRepeatButton");
        repeatButton->setText("Repeat");
        repeatButton->setCheckable(true);
        repeatButton->setChecked(true);
        repeatButton->setFixedHeight(28);
        repeatButton->setCursor(Qt::PointingHandCursor);
        repeatButton->setFocusPolicy(Qt::NoFocus);
        repeatButton->setToolTip("Show the preview texture repeated across UV space.");
        previewRow->addWidget(repeatButton);
        actionButtons_.push_back(repeatButton);
        auto* snapButton = new QToolButton(this);
        snapButton->setObjectName("uvEditorSnapButton");
        snapButton->setText("Snap");
        snapButton->setCheckable(true);
        snapButton->setFixedHeight(28);
        snapButton->setCursor(Qt::PointingHandCursor);
        snapButton->setFocusPolicy(Qt::NoFocus);
        snapButton->setToolTip(
            "Constrain movement to nearby unselected vertices, compatible edges, unselected face centers, or the visible main grid/enabled sub-grid. Ctrl enables snapping temporarily.");
        actionButtons_.push_back(snapButton);
        viewportFacingButton_ = new QToolButton(this);
        viewportFacingButton_->setObjectName("uvEditorViewportFacingButton");
        viewportFacingButton_->setText("View Face");
        viewportFacingButton_->setCheckable(true);
        viewportFacingButton_->setFixedHeight(28);
        viewportFacingButton_->setCursor(Qt::PointingHandCursor);
        viewportFacingButton_->setFocusPolicy(Qt::NoFocus);
        viewportFacingButton_->setToolTip(
            "Orient the entire UV view to the active viewport face. The texture, grid, all selected islands, hit testing, and edit directions rotate and mirror together.");
        previewRow->addWidget(viewportFacingButton_);
        actionButtons_.push_back(viewportFacingButton_);

        auto* snapGridColumn = new QWidget(this);
        snapGridColumn->setObjectName(QStringLiteral("uvEditorSnapGridColumn"));
        auto* snapGridLayout = new QVBoxLayout(snapGridColumn);
        snapGridLayout->setContentsMargins(0, 0, 0, 0);
        snapGridLayout->setSpacing(2);
        snapGridLayout->addWidget(snapButton);

        auto* gridField = new QWidget(snapGridColumn);
        gridField->setObjectName("uvEditorGridStepField");
        auto* gridFieldLayout = new QHBoxLayout(gridField);
        gridFieldLayout->setContentsMargins(0, 0, 0, 0);
        gridFieldLayout->setSpacing(3);
        auto* subGridCheck = new QCheckBox(QStringLiteral("Sub-grid"), gridField);
        subGridCheck->setObjectName(QStringLiteral("uvEditorSubGridCheck"));
        subGridCheck->setChecked(false);
        subGridCheck->setToolTip(QStringLiteral(
            "Show and snap to four subdivisions inside each main UV grid cell."));
        gridFieldLayout->addWidget(subGridCheck);
        auto* gridLabel = new QLabel("Grid size", gridField);
        gridLabel->setObjectName("uvEditorGridStepLabel");
        gridLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        gridLabel->setStyleSheet("color:#cfd7e3;font-weight:600;");
        gridFieldLayout->addWidget(gridLabel);
        auto* gridStepSpin = new CompactDoubleSpinBox(gridField);
        gridStepSpin->setObjectName("uvEditorGridStepSpin");
        gridStepSpin->setRange(0.01, 16.0);
        gridStepSpin->setDecimals(3);
        gridStepSpin->setSingleStep(0.05);
        gridStepSpin->setValue(0.25);
        gridStepSpin->setLocale(ui::uiNumericLocale());
        // This is only a preferred minimum: CompactDoubleSpinBox still grows
        // to the measured width required by the formatted 16.000 bound.
        gridStepSpin->setPreferredCompactSize(24, 58);
        gridStepSpin->setAlignment(Qt::AlignRight);
        gridStepSpin->setToolTip("Set the main UV grid spacing used for drawing and snapping.");
        gridFieldLayout->addWidget(gridStepSpin);
        gridField->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
        gridField->setFixedHeight(gridField->sizeHint().height());
        snapGridLayout->addWidget(gridField);

        QSettings uvGridAppearanceSettings = settingsStore();
        const bool minorDotsEnabled = uvGridAppearanceSettings.value(
            QStringLiteral("uv_editor/grid_minor_dots"), false).toBool();
        bool minorDotSizeOk = false;
        double minorDotSizePx = uvGridAppearanceSettings.value(
            QStringLiteral("uv_editor/grid_minor_dot_size_px"), 2.0)
            .toDouble(&minorDotSizeOk);
        if ( !minorDotSizeOk || !std::isfinite(minorDotSizePx) )
        {
            minorDotSizePx = 2.0;
        }
        minorDotSizePx = std::clamp(minorDotSizePx, 0.5, 10.0);
        auto* minorAppearanceField = new QWidget(snapGridColumn);
        minorAppearanceField->setObjectName(
            QStringLiteral("uvEditorSubGridAppearanceField"));
        auto* minorAppearanceLayout = new QHBoxLayout(minorAppearanceField);
        minorAppearanceLayout->setContentsMargins(0, 0, 0, 0);
        minorAppearanceLayout->setSpacing(3);
        auto* minorStyleLabel = new QLabel(
            QStringLiteral("Minor"), minorAppearanceField);
        minorStyleLabel->setStyleSheet("color:#cfd7e3;font-weight:600;");
        minorAppearanceLayout->addWidget(minorStyleLabel);
        auto* minorStyleCombo = new QComboBox(minorAppearanceField);
        minorStyleCombo->setObjectName(
            QStringLiteral("uvEditorSubGridStyleCombo"));
        minorStyleCombo->addItem(QStringLiteral("Lines"), false);
        minorStyleCombo->addItem(QStringLiteral("Points"), true);
        minorStyleCombo->setCurrentIndex(minorStyleCombo->findData(
            minorDotsEnabled));
        minorStyleCombo->setFixedWidth(72);
        minorStyleCombo->setToolTip(QStringLiteral(
            "Choose lines or bounded logical-pixel dots for the visible UV sub-grid."));
        minorAppearanceLayout->addWidget(minorStyleCombo);
        auto* minorDotSizeSpin = new CompactDoubleSpinBox(minorAppearanceField);
        minorDotSizeSpin->setObjectName(
            QStringLiteral("uvEditorSubGridDotSizeSpin"));
        minorDotSizeSpin->setRange(0.5, 10.0);
        minorDotSizeSpin->setDecimals(1);
        minorDotSizeSpin->setSingleStep(0.5);
        minorDotSizeSpin->setValue(minorDotSizePx);
        minorDotSizeSpin->setLocale(ui::uiNumericLocale());
        minorDotSizeSpin->setPreferredCompactSize(24, 48);
        minorDotSizeSpin->setAlignment(Qt::AlignRight);
        minorDotSizeSpin->setToolTip(QStringLiteral(
            "Set the UV sub-grid point diameter in logical pixels."));
        minorDotSizeSpin->setEnabled(minorDotsEnabled);
        minorAppearanceLayout->addWidget(minorDotSizeSpin);
        minorAppearanceField->setSizePolicy(
            QSizePolicy::Fixed, QSizePolicy::Fixed);
        minorAppearanceField->setFixedHeight(
            minorAppearanceField->sizeHint().height());
        snapGridLayout->addWidget(minorAppearanceField);

        auto* tileGridField = new QWidget(snapGridColumn);
        tileGridField->setObjectName(QStringLiteral("uvEditorTileGridField"));
        auto* tileGridLayout = new QHBoxLayout(tileGridField);
        tileGridLayout->setContentsMargins(0, 0, 0, 0);
        tileGridLayout->setSpacing(4);
        auto* tileGridCheck = new QCheckBox(
            QStringLiteral("Tile grid"), tileGridField);
        tileGridCheck->setObjectName(QStringLiteral(
            "uvEditorRepeatTileGridCheck"));
        tileGridCheck->setChecked(false);
        tileGridCheck->setToolTip(QStringLiteral(
            "Draw faint borders between repeated 1 UV texture tiles without changing texture repetition or snap spacing."));
        tileGridLayout->addWidget(tileGridCheck);
        auto* useTileGridButton = new QPushButton(
            QStringLiteral("1 UV"), tileGridField);
        useTileGridButton->setObjectName(QStringLiteral(
            "uvEditorUseTileGridButton"));
        useTileGridButton->setFixedSize(44, 22);
        useTileGridButton->setToolTip(QStringLiteral(
            "Set the main Grid size to the repeated texture-tile spacing of 1.0 UV."));
        tileGridLayout->addWidget(useTileGridButton);
        tileGridLayout->addStretch(1);
        snapGridLayout->addWidget(tileGridField);
        snapGridColumn->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
        previewRow->addWidget(snapGridColumn);
        normalizeButtonMetrics();
        const int snapGridWidth = std::max(
            { snapButton->width(), gridField->sizeHint().width(),
                tileGridField->sizeHint().width(),
                minorAppearanceField->sizeHint().width() });
        snapButton->setFixedWidth(snapGridWidth);
        gridField->setFixedWidth(snapGridWidth);
        tileGridField->setFixedWidth(snapGridWidth);
        minorAppearanceField->setFixedWidth(snapGridWidth);
        snapGridColumn->setFixedWidth(snapGridWidth);

        hintLabel_ = new QLabel(
            "Face mode | G - Move | GG - Slide | R - Rotate | RR - Pitch/Yaw Rotate | S - Scale | A - Select all",
            this);
        hintLabel_->setStyleSheet("color:#cfd7e3;");
        hintLabel_->setWordWrap(true);
        layout->addWidget(hintLabel_);

        emptyLabel_ = new QLabel("Select one or more surfaces in Surface mode to edit UVs.", this);
        emptyLabel_->setStyleSheet("color:#9ba8bb;");
        emptyLabel_->setWordWrap(true);
        QSizePolicy emptyPolicy = emptyLabel_->sizePolicy();
        emptyPolicy.setRetainSizeWhenHidden(true);
        emptyLabel_->setSizePolicy(emptyPolicy);
        layout->addWidget(emptyLabel_);

        canvas_ = new UvCanvas(this);
        canvas_->installEventFilter(this);
        canvas_->setGridStep(gridStepSpin->value());
        canvas_->setSubGridEnabled(subGridCheck->isChecked());
        canvas_->setSubGridMinorDots(
            minorStyleCombo->currentData().toBool());
        canvas_->setSubGridDotSizePx(minorDotSizeSpin->value());
        canvas_->setRepeatTileGridEnabled(tileGridCheck->isChecked());
        canvas_->setLiveApplyCallback([this](const std::vector<UvIslandState>& islands)
            {
                if (onPreview_)
                {
                    onPreview_(islands);
                }
            });
        canvas_->setStatusTextCallback([this](const QString& text)
            {
                if (hintLabel_)
                {
                    hintLabel_->setText(text);
                }
            });
        canvas_->setModalCommitCallback([this](const std::vector<UvIslandState>& before, const std::vector<UvIslandState>& after)
            {
                if (onCommit_ && !uvIslandStatesStoreSameUv(before, after))
                {
            onCommit_(before, after, QStringLiteral("UVs edited"));
                }
            });

        auto* contentHost = new QWidget(this);
        contentHost->installEventFilter(this);
        auto* contentHostLayout = new QVBoxLayout(contentHost);
        contentHostLayout->setContentsMargins(0, 0, 0, 0);
        contentHostLayout->setSpacing(0);
        contentSplitter_ = new QSplitter(Qt::Horizontal, contentHost);
        contentSplitter_->setObjectName(QStringLiteral("uvEditorContentSplitter"));
        contentSplitter_->setChildrenCollapsible(false);
        contentSplitter_->setHandleWidth(4);
        canvas_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        controlsScroll_ = new QScrollArea(contentHost);
        controlsScroll_->setWidgetResizable(true);
        controlsScroll_->setFrameShape(QFrame::NoFrame);
        controlsScroll_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        controlsScroll_->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        controlsScroll_->setWidget(controlsHost_);
        controlsHost_->installEventFilter(this);
        controlsScroll_->installEventFilter(this);
        controlsScroll_->viewport()->installEventFilter(this);
        hintLabel_->installEventFilter(this);
        emptyLabel_->installEventFilter(this);
        controlsHost_->setFocusProxy(canvas_);
        controlsScroll_->setFocusProxy(canvas_);
        controlsScroll_->viewport()->setFocusProxy(canvas_);
        hintLabel_->setFocusProxy(canvas_);
        emptyLabel_->setFocusProxy(canvas_);
        setFocusProxy(canvas_);
        contentSplitter_->addWidget(controlsScroll_);
        contentSplitter_->addWidget(canvas_);
        contentHostLayout->addWidget(contentSplitter_);
        layout->addWidget(contentHost, 1);

        auto commitCanvasEdit = [this](const std::vector<UvIslandState>& before,
                                       const QString& operation)
        {
            if (!canvas_ || !onCommit_)
            {
                return;
            }

            const std::vector<UvIslandState>& after = canvas_->islands();
            if (uvIslandStatesStoreSameUv(before, after))
            {
                return;
            }

            onCommit_(before, after, operation);
        };

        connect(unwrapButton, &QPushButton::clicked, this, [this, commitCanvasEdit]()
            {
                if (!canvas_ || canvas_->hasActiveModalTransform() || !canvas_->hasSelection())
                {
                    return;
                }

                if (!onSmartProject_)
                {
                    SmartUvProjectDialog optionsDialog(smartUvOptions_, this);
                    if (optionsDialog.exec() != QDialog::Accepted)
                    {
                        return;
                    }
                    smartUvOptions_ = optionsDialog.options();
                    if (canvas_->hasActiveModalTransform() || !canvas_->hasSelection())
                    {
                        return;
                    }
                    const std::vector<UvIslandState> before = canvas_->islands();
                    canvas_->unwrapSelected();
                    commitCanvasEdit(before, QStringLiteral("UVs projected"));
                    return;
                }

                const std::vector<UvIslandState> before = canvas_->islands();
                const std::vector<bool> selectedMask = canvas_->selectedIslandMask();
                std::vector<UvIslandState> selectedIslands{};
                std::vector<std::size_t> selectedIndices{};
                selectedIslands.reserve(before.size());
                selectedIndices.reserve(before.size());
                for (std::size_t islandIndex = 0; islandIndex < before.size(); ++islandIndex)
                {
                    if (islandIndex >= selectedMask.size() || !selectedMask[islandIndex])
                    {
                        continue;
                    }
                    selectedIndices.push_back(islandIndex);
                    selectedIslands.push_back(before[islandIndex]);
                }

                if (selectedIslands.empty())
                {
                    return;
                }

                SmartUvProjectOptions initialOptions = smartUvOptions_;
                initialOptions.islandScale = 1.0;
                std::vector<UvIslandState> unitScaleProjection = selectedIslands;
                onSmartProject_(
                    unitScaleProjection,
                    initialOptions,
                    previewTextureSize_);
                if (unitScaleProjection.size() == selectedIslands.size())
                {
                    initialOptions.islandScale = estimateSmartUvIslandScale(
                        selectedIslands,
                        unitScaleProjection);
                }
                else
                {
                    initialOptions.islandScale = 1.0;
                }

                SmartUvProjectDialog optionsDialog(initialOptions, this);
                if (optionsDialog.exec() != QDialog::Accepted)
                {
                    return;
                }
                smartUvOptions_ = optionsDialog.options();

                onSmartProject_(selectedIslands, smartUvOptions_, previewTextureSize_);
                if (selectedIslands.size() != selectedIndices.size())
                {
                    QMessageBox::warning(this, "Smart UV Project", "The selected surfaces could not be unwrapped.");
                    return;
                }

                std::vector<UvIslandState> next = before;
                for (std::size_t selectedIndex = 0; selectedIndex < selectedIndices.size(); ++selectedIndex)
                {
                    UvIslandState& destination = next[selectedIndices[selectedIndex]];
                    const UvIslandState& projected = selectedIslands[selectedIndex];
                    destination.uv = projected.uv;
                    destination.unwrapUv = projected.unwrapUv;
                    destination.displayOffset = projected.displayOffset;
                    destination.displayMirrorAxisX = projected.displayMirrorAxisX;
                    destination.displayMirrorX = projected.displayMirrorX;
                }
                canvas_->setIslands(std::move(next), false);
                canvas_->previewCurrentState();
                commitCanvasEdit(before, QStringLiteral("UVs smart-projected"));
            });
        connect(followActiveButton, &QPushButton::clicked, this, [this, commitCanvasEdit]()
            {
                if (!canvas_ || canvas_->hasActiveModalTransform() || !canvas_->hasSelection()
                    || !onFollowActiveQuads_)
                {
                    return;
                }

                const QStringList modeNames{
                    QStringLiteral("Even"),
                    QStringLiteral("Length"),
                    QStringLiteral("Length Average"),
                };
                const int initialMode = followActiveQuadsMode_ == FollowActiveQuadsMode::even
                    ? 0
                    : (followActiveQuadsMode_ == FollowActiveQuadsMode::length ? 1 : 2);
                bool accepted = false;
                const QString selectedMode = QInputDialog::getItem(
                    this,
                    QStringLiteral("Follow Active Quads"),
                    QStringLiteral("Edge Length Mode:"),
                    modeNames,
                    initialMode,
                    false,
                    &accepted);
                if (!accepted)
                {
                    return;
                }
                const int modeIndex = modeNames.indexOf(selectedMode);
                followActiveQuadsMode_ = modeIndex == 0
                    ? FollowActiveQuadsMode::even
                    : (modeIndex == 1
                        ? FollowActiveQuadsMode::length
                        : FollowActiveQuadsMode::lengthAverage);

                const std::vector<UvIslandState> before = canvas_->islands();
                std::vector<UvIslandState> next = before;
                FollowActiveQuadsReport report{};
                if (!onFollowActiveQuads_(
                        next,
                        canvas_->selectedIslandMask(),
                        canvas_->activeIslandRef(),
                        followActiveQuadsMode_,
                        &report))
                {
                    QMessageBox::warning(
                        this,
                        QStringLiteral("Follow Active Quads"),
                        QString::fromUtf8(followActiveQuadsErrorMessage(report.error)));
                    return;
                }

                canvas_->setIslands(std::move(next), false);
                canvas_->previewCurrentState();
                commitCanvasEdit(before, QStringLiteral("UVs followed active quads"));
            });
        connect(packButton, &QPushButton::clicked, this, [this, commitCanvasEdit]()
            {
                if (canvas_->hasActiveModalTransform() || !canvas_->hasSelection())
                {
                    return;
                }
                const std::vector<UvIslandState> before = canvas_->islands();
                canvas_->packSelected();
                commitCanvasEdit(before, QStringLiteral("UVs packed"));
            });
        connect(rotateLeftButton, &QPushButton::clicked, this, [this, commitCanvasEdit]()
            {
                if (canvas_->hasActiveModalTransform() || !canvas_->hasSelection())
                {
                    return;
                }
                const std::vector<UvIslandState> before = canvas_->islands();
                canvas_->rotateSelected(-90.0);
            commitCanvasEdit(before, QStringLiteral("UVs rotated"));
            });
        connect(rotateRightButton, &QPushButton::clicked, this, [this, commitCanvasEdit]()
            {
                if (canvas_->hasActiveModalTransform() || !canvas_->hasSelection())
                {
                    return;
                }
                const std::vector<UvIslandState> before = canvas_->islands();
                canvas_->rotateSelected(90.0);
            commitCanvasEdit(before, QStringLiteral("UVs rotated"));
            });
        connect(applyScaleFactorButton, &QPushButton::clicked, this,
            [this, scaleFactorSpin, commitCanvasEdit]()
            {
                if (!scaleFactorSpin || canvas_->hasActiveModalTransform()
                    || !canvas_->hasSelection())
                {
                    return;
                }
                scaleFactorSpin->interpretText();
                const qreal factor = scaleFactorSpin->value();
                const std::vector<UvIslandState> before = canvas_->islands();
                canvas_->scaleSelected(factor, factor);
                commitCanvasEdit(before, QStringLiteral("UVs scaled"));
            });
        connect(mirrorXButton, &QPushButton::clicked, this, [this, commitCanvasEdit]()
            {
                if (canvas_->hasActiveModalTransform() || !canvas_->hasSelection())
                {
                    return;
                }
                const std::vector<UvIslandState> before = canvas_->islands();
                canvas_->mirrorSelected(true, false);
            commitCanvasEdit(before, QStringLiteral("UVs mirrored"));
            });
        connect(mirrorYButton, &QPushButton::clicked, this, [this, commitCanvasEdit]()
            {
                if (canvas_->hasActiveModalTransform() || !canvas_->hasSelection())
                {
                    return;
                }
                const std::vector<UvIslandState> before = canvas_->islands();
                canvas_->mirrorSelected(false, true);
            commitCanvasEdit(before, QStringLiteral("UVs mirrored"));
            });
        connect(fitButton, &QPushButton::clicked, this, [this]()
            {
                if (canvas_->hasActiveModalTransform())
                {
                    return;
                }
                canvas_->fitToContent(false);
            });
        connect(selectAllButton, &QPushButton::clicked, this, [this]()
            {
                if (canvas_->hasActiveModalTransform())
                {
                    return;
                }
                canvas_->selectAll();
            });
        connect(repeatButton, &QToolButton::toggled, this, [this](const bool checked)
            {
                canvas_->setRepeatTexturePreviewEnabled(checked);
            });
        connect(snapButton, &QToolButton::toggled, this, [this](const bool checked)
            {
                canvas_->setPersistentSnapEnabled(checked);
            });
        connect(subGridCheck, &QCheckBox::toggled, this, [this](const bool checked)
            {
                canvas_->setSubGridEnabled(checked);
            });
        connect(minorStyleCombo,
            QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this, minorStyleCombo, minorDotSizeSpin](const int)
            {
                const bool points = minorStyleCombo->currentData().toBool();
                minorDotSizeSpin->setEnabled(points);
                if ( canvas_ )
                {
                    canvas_->setSubGridMinorDots(points);
                }
                QSettings settings = settingsStore();
                settings.setValue(QStringLiteral(
                    "uv_editor/grid_minor_dots"), points);
                settings.setValue(QStringLiteral(
                    "uv_editor/grid_minor_dot_size_px"),
                    minorDotSizeSpin->value());
            });
        connect(minorDotSizeSpin,
            QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
            [this](const double diameter)
            {
                if ( canvas_ )
                {
                    canvas_->setSubGridDotSizePx(diameter);
                }
                QSettings settings = settingsStore();
                settings.setValue(QStringLiteral(
                    "uv_editor/grid_minor_dot_size_px"), diameter);
            });
        connect(tileGridCheck, &QCheckBox::toggled, this, [this](const bool checked)
            {
                canvas_->setRepeatTileGridEnabled(checked);
            });
        connect(useTileGridButton, &QPushButton::clicked, this, [gridStepSpin]()
            {
                gridStepSpin->setValue(1.0);
            });
        connect(viewportFacingButton_, &QToolButton::toggled, this, [this](const bool checked)
            {
                if (onViewportFacingChanged_)
                {
                    onViewportFacingChanged_(checked);
                }
            });
        connect(gridStepSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
            [this](const double step)
            {
                if (canvas_)
                {
                    canvas_->setGridStep(step);
                }
            });

        setEditorState({}, {}, true);
        setHorizontalDock(true);
    }

    void setHorizontalDock(const bool horizontalDock)
    {
        if (!contentSplitter_ || !controlsHost_ || !controlsScroll_ || !canvas_)
        {
            return;
        }

        contentSplitter_->setOrientation(horizontalDock ? Qt::Horizontal : Qt::Vertical);
        if (horizontalDock)
        {
            contentSplitter_->insertWidget(0, controlsScroll_);
            contentSplitter_->insertWidget(1, canvas_);
            contentSplitter_->setStretchFactor(0, 0);
            contentSplitter_->setStretchFactor(1, 1);
        }
        else
        {
            contentSplitter_->insertWidget(0, canvas_);
            contentSplitter_->insertWidget(1, controlsScroll_);
            contentSplitter_->setStretchFactor(0, 1);
            contentSplitter_->setStretchFactor(1, 0);
        }
        controlsScroll_->setSizePolicy(
            horizontalDock ? QSizePolicy::Preferred : QSizePolicy::Expanding,
            horizontalDock ? QSizePolicy::Expanding : QSizePolicy::Preferred);
        controlsScroll_->setMinimumWidth(horizontalDock ? 360 : 0);
        controlsScroll_->setMaximumWidth(horizontalDock ? 520 : QWIDGETSIZE_MAX);
        controlsScroll_->setMinimumHeight(horizontalDock ? 0 : 156);
        controlsScroll_->setMaximumHeight(horizontalDock ? QWIDGETSIZE_MAX : 240);
        canvas_->setMinimumSize(horizontalDock ? QSize(420, 260) : QSize(260, 220));
        if (horizontalDock)
        {
            contentSplitter_->setSizes(QList<int>{ 380, 820 });
        }
        else
        {
            contentSplitter_->setSizes(QList<int>{ 760, 196 });
        }
        updateGeometry();
    }

    void setEditorState(
        std::vector<UvIslandState> islands,
        QImage previewTexture,
        const bool fitView,
        const std::optional<SurfaceRef>& activeSurfaceRef = std::nullopt,
        const bool preserveDisplayState = true)
    {
        if (!canvas_)
        {
            return;
        }

        if (canvas_->hasActiveModalTransform())
        {
            canvas_->cancelModalTransform();
        }

        if (!fitView && preserveDisplayState)
        {
            const auto sameStoredPoints = ui::uv::uvDisplayPointsStoreSameCoordinates;

            const auto refKey = [](const SurfaceRef ref) -> std::uint64_t
            {
                return (static_cast<std::uint64_t>(static_cast<std::uint32_t>(ref.sector)) << 32u)
                    | static_cast<std::uint32_t>(ref.surface);
            };

            std::unordered_map<std::uint64_t, const UvIslandState*> previousByRef{};
            previousByRef.reserve(canvas_->islands().size());
            for (const UvIslandState& island : canvas_->islands())
            {
                previousByRef.emplace(refKey(island.ref), &island);
            }

            for (UvIslandState& island : islands)
            {
                const auto it = previousByRef.find(refKey(island.ref));
                if (it == previousByRef.end() || !it->second)
                {
                    continue;
                }

                const UvIslandState& previous = *it->second;
                if (previous.uv.size() != island.uv.size())
                {
                    continue;
                }

                const bool sameDisplayedUvStored =
                    sameStoredPoints(previous, previous.uv, island, island.uv);
                const bool sameDisplayedUnwrapStored =
                    previous.unwrapUv.size() == island.unwrapUv.size()
                    && sameStoredPoints(previous, previous.unwrapUv, island, island.unwrapUv);

                if (sameDisplayedUvStored)
                {
                    std::vector<QPointF> storedUnwrap{};
                    if (!sameDisplayedUnwrapStored)
                    {
                        storedUnwrap.reserve(island.unwrapUv.size());
                        for (const QPointF& uv : island.unwrapUv)
                        {
                            storedUnwrap.push_back(islandStoredFromDisplayUv(island, uv));
                        }
                    }

                    // Keep the exact displayed UV polygon when it still maps back
                    // to the same stored UVs, even if the helper unwrap
                    // representation was rebuilt differently on selection change.
                    island.displayMirrorAxisX = previous.displayMirrorAxisX;
                    island.displayOffset = previous.displayOffset;
                    island.displayMirrorX = previous.displayMirrorX;
                    island.uv = previous.uv;
                    if (sameDisplayedUnwrapStored)
                    {
                        island.unwrapUv = previous.unwrapUv;
                    }
                    else
                    {
                        island.unwrapUv.resize(storedUnwrap.size());
                        for (std::size_t pointIndex = 0; pointIndex < island.unwrapUv.size(); ++pointIndex)
                        {
                            island.unwrapUv[pointIndex] = islandDisplayFromStoredUv(island, storedUnwrap[pointIndex]);
                        }
                    }
                    continue;
                }

                if (previous.displayMirrorX != island.displayMirrorX)
                {
                    std::vector<QPointF> storedUv{};
                    storedUv.reserve(island.uv.size());
                    for (const QPointF& uv : island.uv)
                    {
                        storedUv.push_back(islandStoredFromDisplayUv(island, uv));
                    }

                    std::vector<QPointF> storedUnwrap{};
                    storedUnwrap.reserve(island.unwrapUv.size());
                    for (const QPointF& uv : island.unwrapUv)
                    {
                        storedUnwrap.push_back(islandStoredFromDisplayUv(island, uv));
                    }

                    island.displayOffset = previous.displayOffset;
                    island.displayMirrorX = previous.displayMirrorX;

                    for (std::size_t pointIndex = 0; pointIndex < island.uv.size(); ++pointIndex)
                    {
                        island.uv[pointIndex] = islandDisplayFromStoredUv(island, storedUv[pointIndex]);
                    }
                    for (std::size_t pointIndex = 0; pointIndex < island.unwrapUv.size(); ++pointIndex)
                    {
                        island.unwrapUv[pointIndex] = islandDisplayFromStoredUv(island, storedUnwrap[pointIndex]);
                    }
                    continue;
                }

                // Keep the previous handedness/translation for matching islands,
                // but keep the freshly built mirror axis from the current stored
                // UV bounds. After adopting that prior display basis, rebuild the
                // displayed points too; otherwise the metadata and the displayed
                // polygon can diverge on same-selection refreshes.
                std::vector<QPointF> storedUv{};
                storedUv.reserve(island.uv.size());
                for (const QPointF& uv : island.uv)
                {
                    storedUv.push_back(islandStoredFromDisplayUv(island, uv));
                }

                std::vector<QPointF> storedUnwrap{};
                storedUnwrap.reserve(island.unwrapUv.size());
                for (const QPointF& uv : island.unwrapUv)
                {
                    storedUnwrap.push_back(islandStoredFromDisplayUv(island, uv));
                }

                island.displayOffset = previous.displayOffset;
                island.displayMirrorX = previous.displayMirrorX;

                for (std::size_t pointIndex = 0; pointIndex < island.uv.size(); ++pointIndex)
                {
                    island.uv[pointIndex] = islandDisplayFromStoredUv(island, storedUv[pointIndex]);
                }
                for (std::size_t pointIndex = 0; pointIndex < island.unwrapUv.size(); ++pointIndex)
                {
                    island.unwrapUv[pointIndex] = islandDisplayFromStoredUv(island, storedUnwrap[pointIndex]);
                }
            }
        }

        previewTextureSize_ = previewTexture.size();
        canvas_->setPreviewTexture(std::move(previewTexture));
        canvas_->setIslands(std::move(islands), fitView);
        canvas_->setActiveIslandRef(activeSurfaceRef);

        const bool haveIslands = !canvas_->islands().empty();
        emptyLabel_->setVisible(!haveIslands);
        canvas_->setVisible(true);
        hintLabel_->setVisible(true);
        for (QWidget* row : controlRows_)
        {
            if (row)
            {
                row->setEnabled(haveIslands);
            }
        }
    }

    void clearEditorState(const QString& message)
    {
        if (!canvas_)
        {
            return;
        }

        if (canvas_->hasActiveModalTransform())
        {
            canvas_->cancelModalTransform();
        }
        canvas_->setIslands({}, false);
        canvas_->setPreviewTexture({});
        previewTextureSize_ = {};
        emptyLabel_->setText(message);
        emptyLabel_->setVisible(true);
        canvas_->setVisible(true);
        hintLabel_->setVisible(true);
        for (QWidget* row : controlRows_)
        {
            if (row)
            {
                row->setEnabled(false);
            }
        }
    }

    void focusCanvas()
    {
        if (canvas_)
        {
            canvas_->setFocus(Qt::OtherFocusReason);
        }
    }

    bool hasActiveModalTransform() const noexcept
    {
        return canvas_ && canvas_->hasActiveModalTransform();
    }

    bool handleShortcutKey(const int key, const Qt::KeyboardModifiers modifiers = Qt::NoModifier)
    {
        if (!canvas_)
        {
            return false;
        }

        canvas_->setFocus(Qt::OtherFocusReason);
        return canvas_->handleShortcutKey(key, modifiers);
    }

    void cancelActiveModalTransform()
    {
        if (canvas_)
        {
            canvas_->cancelModalTransform();
        }
    }

    bool hasEditorState() const noexcept
    {
        return canvas_ && !canvas_->islands().empty();
    }

    UvEditorWorkspaceState recoveryWorkspaceState() const
    {
        return canvas_ ? canvas_->recoveryWorkspaceState() : UvEditorWorkspaceState{};
    }

    void setSettledWorkspaceChangedCallback(std::function<void()> callback)
    {
        if (canvas_)
        {
            canvas_->setSettledWorkspaceChangedCallback(std::move(callback));
        }
    }

    bool applyRecoveryWorkspaceState(const UvEditorWorkspaceState& state)
    {
        return canvas_ && canvas_->applyRecoveryWorkspaceState(state);
    }

    void resetRecoveryWorkspaceState()
    {
        if (canvas_)
        {
            canvas_->resetRecoveryWorkspaceState();
            canvas_->setPreviewTexture({});
        }
        previewTextureSize_ = {};
        if (emptyLabel_)
        {
            emptyLabel_->setText(
                "Select one or more surfaces in Surface mode to edit UVs.");
            emptyLabel_->setVisible(true);
        }
        for (QWidget* row : controlRows_)
        {
            if (row)
            {
                row->setEnabled(false);
            }
        }
    }

    void setPickMode(const UvCanvas::PickMode mode)
    {
        if (canvas_)
        {
            canvas_->setPickMode(mode);
        }
    }

    std::vector<SurfaceRef> surfaceRefs() const
    {
        std::vector<SurfaceRef> refs{};
        if (!canvas_)
        {
            return refs;
        }

        refs.reserve(canvas_->islands().size());
        for (const UvIslandState& island : canvas_->islands())
        {
            refs.push_back(island.ref);
        }
        std::sort(refs.begin(), refs.end(), [](const SurfaceRef& a, const SurfaceRef& b)
            {
                if (a.sector != b.sector)
                {
                    return a.sector < b.sector;
                }
                return a.surface < b.surface;
        });
        return refs;
    }

    std::optional<SurfaceRef> activeSurfaceRef() const
    {
        if (!canvas_)
        {
            return std::nullopt;
        }
        return canvas_->activeIslandRef();
    }

    std::optional<SurfaceRef> firstSelectedSurfaceRef() const
    {
        if (!canvas_)
        {
            return std::nullopt;
        }
        return canvas_->firstSelectedIslandRef();
    }

    const std::vector<UvIslandState>& islands() const noexcept
    {
        static const std::vector<UvIslandState> empty{};
        return canvas_ ? canvas_->islands() : empty;
    }

    void setViewportFacingOrientation(
        const bool enabled,
        const bool mirrorX,
        const qreal rotationRadians)
    {
        if (viewportFacingButton_)
        {
            const QSignalBlocker blocker(viewportFacingButton_);
            viewportFacingButton_->setChecked(enabled);
        }
        if (canvas_)
        {
            canvas_->setViewportFacingView(enabled, mirrorX, rotationRadians);
        }
    }

    bool viewportFacingMirrorX() const noexcept
    {
        return canvas_ && canvas_->viewMirrorX();
    }

    qreal viewportFacingRotationRadians() const noexcept
    {
        return canvas_ ? canvas_->viewRotationRadians() : 0.0;
    }

    void clampCanvasZoomToConfiguredMaximum()
    {
        if (canvas_)
        {
            canvas_->clampZoomToConfiguredMaximum();
        }
    }

protected:
    bool eventFilter(QObject* watched, QEvent* event) override
    {
        if (canvas_ && event)
        {
            switch (event->type())
            {
                case QEvent::Enter:
                case QEvent::MouseMove:
                    if (!canvas_->hasFocus() && !viewportFlyOwnsUvHover(*canvas_))
                    {
                        canvas_->setFocus(Qt::OtherFocusReason);
                    }
                    break;
                case QEvent::MouseButtonPress:
                    canvas_->setFocus(Qt::OtherFocusReason);
                    break;
                case QEvent::ShortcutOverride:
                case QEvent::KeyPress:
                {
                    auto* keyEvent = static_cast<QKeyEvent*>(event);
                    if (!keyEvent)
                    {
                        break;
                    }

                    const int key = keyEvent->key();
                    const QWidget* activeModal = QApplication::activeModalWidget();
                    const QWidget* focusWidget = QApplication::focusWidget();
                    const bool canvasPointerContext =
                        canvas_->isVisible()
                        && ui::viewport::visibleWidgetContainsGlobalPoint(
                            canvas_, QCursor::pos());
                    const bool modalTransformKey = canvas_->hasActiveModalTransform()
                        && (key == Qt::Key_Escape
                            || key == Qt::Key_Return
                            || key == Qt::Key_Enter);
                    if ((activeModal && activeModal != window())
                        || ui::shortcuts::focusAcceptsTextInput(focusWidget)
                        || (!canvasPointerContext && !modalTransformKey))
                    {
                        break;
                    }

                    constexpr Qt::KeyboardModifiers kShortcutBlockingModifiers =
                        Qt::ShiftModifier | Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier;
                    const bool hasShortcutBlockingModifier =
                        (keyEvent->modifiers() & kShortcutBlockingModifiers) != Qt::NoModifier;
                    const bool uvShortcut =
                        !hasShortcutBlockingModifier
                        && (key == Qt::Key_G
                            || key == Qt::Key_R
                            || key == Qt::Key_S
                            || key == Qt::Key_A
                            || key == Qt::Key_Escape
                            || key == Qt::Key_Return
                            || key == Qt::Key_Enter);
                    if (uvShortcut)
                    {
                        if (keyEvent->isAutoRepeat()
                            && (key == Qt::Key_G
                                || key == Qt::Key_R
                                || key == Qt::Key_S
                                || key == Qt::Key_A))
                        {
                            keyEvent->accept();
                            return true;
                        }

                        if (event->type() == QEvent::ShortcutOverride)
                        {
                            keyEvent->accept();
                            return true;
                        }
                        return handleShortcutKey(key, keyEvent->modifiers());
                    }
                    break;
                }
                default:
                    break;
            }
        }
        return QWidget::eventFilter(watched, event);
    }

private:
    UvCanvas* canvas_{};
    QWidget* controlsHost_{};
    QScrollArea* controlsScroll_{};
    QSplitter* contentSplitter_{};
    QLabel* hintLabel_{};
    QLabel* emptyLabel_{};
    std::vector<QWidget*> controlRows_{};
    std::vector<QAbstractButton*> actionButtons_{};
    QSize previewTextureSize_{};
    SmartUvProjectOptions smartUvOptions_{};
    FollowActiveQuadsMode followActiveQuadsMode_{ FollowActiveQuadsMode::lengthAverage };
    QToolButton* viewportFacingButton_{};
    std::function<void(std::vector<UvIslandState>&, const SmartUvProjectOptions&, const QSize&)> onSmartProject_{};
    std::function<bool(
        std::vector<UvIslandState>&,
        const std::vector<bool>&,
        std::optional<SurfaceRef>,
        FollowActiveQuadsMode,
        FollowActiveQuadsReport*)> onFollowActiveQuads_{};
    std::function<void(const std::vector<UvIslandState>&)> onPreview_{};
    std::function<void(const std::vector<UvIslandState>&, const std::vector<UvIslandState>&, const QString&)> onCommit_{};
    std::function<void(bool)> onViewportFacingChanged_{};
};

QColor uvEditorColorForIndex(const int index)
{
    static const std::array<QColor, 10> kColors = {
        QColor(88, 166, 255),
        QColor(92, 219, 149),
        QColor(249, 196, 74),
        QColor(195, 119, 255),
        QColor(255, 139, 92),
        QColor(96, 206, 214),
        QColor(215, 110, 169),
        QColor(154, 224, 92),
        QColor(102, 135, 245),
        QColor(244, 95, 111),
    };
    return kColors[static_cast<std::size_t>(index) % kColors.size()];
}

} // namespace

#if defined(SED_APP_MAIN_WINDOW_TEST_ACCESS)
double preciseUvNumericInputForTest(QWidget* parent, const double initial,
    const double minimum, const double maximum, bool* accepted)
{
    return ui::preciseNumericInput(parent, QStringLiteral("UV numeric contract"), QStringLiteral("Value"),
        initial, minimum, maximum, accepted);
}

std::vector<QPointF> MainWindow::uvEditorStoredPointsForTest(const SurfaceRef ref) const
{
    const auto* panel = static_cast<const UvEditorPanel*>(uvEditorPanelWidget_);
    std::vector<QPointF> points{};
    if (panel)
    {
        for (const UvIslandState& island : panel->islands())
        {
            if (island.ref.sector == ref.sector && island.ref.surface == ref.surface)
            {
                for (const QPointF& point : island.uv)
                {
                    const QPointF normalized = islandStoredFromDisplayUv(island, point);
                    points.push_back(island.storedCoordinateSpace == UvCoordinateSpace::TexturePixels
                        ? ui::uv::denormalizeStoredUv(normalized, island.textureSize) : normalized);
                }
                break;
            }
        }
    }
    return points;
}

#endif

QByteArray MainWindow::captureUvEditorWorkspaceState() const
{
    // Modal G/S/R bookkeeping is intentionally absent. The Level owns any
    // already-committed UV coordinates; this sidecar contains only the settled
    // canvas view, display basis, and selection state.
    const auto* panel = uvEditorPanelWidget_
        ? static_cast<const UvEditorPanel*>(uvEditorPanelWidget_)
        : nullptr;
    UvEditorWorkspaceState state = panel
        ? panel->recoveryWorkspaceState()
        : UvEditorWorkspaceState{};
    const editor::Selection& selection = document_.selection();
    const bool surfaceOnlySelection = editMode_ == EditMode::surface
        && !selection.surfaces.empty()
        && selection.sectors.empty()
        && selection.things.empty()
        && selection.lights.empty()
        && selection.cogs.empty();
    const bool panelMatchesSelection = surfaceOnlySelection
        && state.islands.size() == selection.surfaces.size()
        && std::all_of(
            state.islands.begin(), state.islands.end(),
            [&selection](const UvEditorWorkspaceIslandState& island)
            {
                return selection.surfaces.contains(island.ref);
            });
    if (!panelMatchesSelection)
    {
        // A hidden panel may still own the previous Surface mode's canvas.
        // The document selection is authoritative, so never publish stale UV
        // references under a different edit mode.
        state.activeSurface.reset();
        state.islands.clear();
    }
    return serializeUvEditorWorkspaceState(state);
}

bool MainWindow::validateUvEditorWorkspaceState(
    const QByteArray& payload,
    const editor::Selection& documentSelection,
    const EditMode recoveredEditMode,
    const SurfaceSelectionMode recoveredSurfaceMode,
    const bool recoveredViewportFacing,
    QString* warning) const
{
    QString error{};
    const std::optional<UvEditorWorkspaceState> state = parseUvEditorWorkspaceState(
        payload,
        document_.level(),
        documentSelection,
        recoveredEditMode == EditMode::surface,
        static_cast<int>(recoveredSurfaceMode),
        recoveredViewportFacing,
        &error);
    if (!state && warning)
    {
        *warning = error;
    }
    return state.has_value();
}

bool MainWindow::restoreUvEditorWorkspaceState(
    const QByteArray& payload,
    const editor::Selection& documentSelection,
    const EditMode recoveredEditMode,
    const SurfaceSelectionMode recoveredSurfaceMode,
    const bool recoveredViewportFacing,
    QString* warning)
{
    QString error{};
    const std::optional<UvEditorWorkspaceState> state = parseUvEditorWorkspaceState(
        payload,
        document_.level(),
        documentSelection,
        recoveredEditMode == EditMode::surface,
        static_cast<int>(recoveredSurfaceMode),
        recoveredViewportFacing,
        &error);
    auto* panel = uvEditorPanelWidget_
        ? static_cast<UvEditorPanel*>(uvEditorPanelWidget_)
        : nullptr;
    if (!state || !panel)
    {
        if (warning)
        {
            *warning = !error.isEmpty()
                ? error
                : QStringLiteral("UV editor panel is unavailable.");
        }
        return false;
    }

    panel->setViewportFacingOrientation(
        recoveredViewportFacing,
        state->viewMirrorX,
        state->viewRotationRadians);
    if (!panel->applyRecoveryWorkspaceState(*state))
    {
        if (warning)
        {
            *warning = QStringLiteral(
                "UV editor state no longer matches the rebuilt Surface selection.");
        }
        return false;
    }
    // The recovered islands are now the authoritative display rebuild. Do
    // not let the next routine updateEditActions() pass treat an earlier
    // pre-restore invalidation as a reason to derive a new View Face basis.
    uvEditorForceDisplayRebuild_ = false;
    return true;
}

void MainWindow::resetUvEditorWorkspaceState()
{
    uvEditorViewportFacing_ = false;
    if (auto* facing = findChild<QToolButton*>(
            QStringLiteral("uvEditorViewportFacingButton")))
    {
        const QSignalBlocker blocker(facing);
        facing->setChecked(false);
    }
    if (auto* repeat = findChild<QToolButton*>(
            QStringLiteral("uvEditorRepeatButton")))
    {
        repeat->setChecked(true);
    }
    if (auto* snap = findChild<QToolButton*>(
            QStringLiteral("uvEditorSnapButton")))
    {
        snap->setChecked(false);
    }
    if (auto* grid = findChild<QDoubleSpinBox*>(
            QStringLiteral("uvEditorGridStepSpin")))
    {
        grid->setValue(0.25);
    }
    if (auto* subGrid = findChild<QCheckBox*>(
            QStringLiteral("uvEditorSubGridCheck")))
    {
        subGrid->setChecked(false);
    }
    if (auto* tileGrid = findChild<QCheckBox*>(
            QStringLiteral("uvEditorRepeatTileGridCheck")))
    {
        tileGrid->setChecked(false);
    }
    if (uvEditorPanelWidget_)
    {
        static_cast<UvEditorPanel*>(uvEditorPanelWidget_)
            ->resetRecoveryWorkspaceState();
    }
    uvEditorForceDisplayRebuild_ = false;
}

    bool MainWindow::cancelActiveUvEditorModalTransform()
{
    if (!uvEditorPanelWidget_)
    {
        return false;
    }

    auto* panel = static_cast<UvEditorPanel*>(uvEditorPanelWidget_);
    if (!panel->hasActiveModalTransform())
    {
        return false;
    }

    panel->cancelActiveModalTransform();
    return true;
}

void MainWindow::setupUvEditorSurfaceActions(QMenu* surfaceMenu)
{
    if (!surfaceMenu)
    {
        return;
    }
    surfaceMenu->setToolTipsVisible(true);

    auto collectSelectedSurfaceRefsSorted = [this]() -> std::vector<SurfaceRef>
    {
        std::vector<SurfaceRef> refs{};
        refs.reserve(document_.selection().surfaces.size());
        for (const SurfaceRef ref : document_.selection().surfaces)
        {
            refs.push_back(ref);
        }

        std::sort(refs.begin(), refs.end(), [](const SurfaceRef& a, const SurfaceRef& b)
            {
                if (a.sector != b.sector)
                {
                    return a.sector < b.sector;
                }
                return a.surface < b.surface;
            });
        return refs;
    };

    auto sameSurfaceRef = [](const SurfaceRef& a, const SurfaceRef& b) noexcept
    {
        return a.sector == b.sector && a.surface == b.surface;
    };

    auto uvEditorHasActiveModalTransform = [this]() -> bool
    {
        return uvEditorPanelWidget_
            && static_cast<UvEditorPanel*>(uvEditorPanelWidget_)->hasActiveModalTransform();
    };

    auto currentUvReferenceSurfaceRef = [this, sameSurfaceRef]() -> std::optional<SurfaceRef>
    {
        auto isSelected = [this, sameSurfaceRef](const SurfaceRef& ref) -> bool
        {
            return std::any_of(
                document_.selection().surfaces.begin(),
                document_.selection().surfaces.end(),
                [&ref, &sameSurfaceRef](const SurfaceRef& selected)
                {
                    return sameSurfaceRef(selected, ref);
                });
        };

        if (uvEditorPanelWidget_)
        {
            if (const auto ref = static_cast<UvEditorPanel*>(uvEditorPanelWidget_)->activeSurfaceRef())
            {
                if (isSelected(*ref))
                {
                    return ref;
                }
            }
        }

        if (glView_)
        {
            if (const auto ref = glView_->selectedSurfaceRef())
            {
                if (isSelected(*ref))
                {
                    return ref;
                }
            }
        }

        return std::nullopt;
    };

    auto collectSelectedSurfaceRefsReferenceFirst =
        [collectSelectedSurfaceRefsSorted, currentUvReferenceSurfaceRef, sameSurfaceRef]() -> std::vector<SurfaceRef>
    {
        std::vector<SurfaceRef> refs = collectSelectedSurfaceRefsSorted();
        if (refs.empty())
        {
            return refs;
        }

        if (const auto referenceRef = currentUvReferenceSurfaceRef())
        {
            const auto it = std::find_if(
                refs.begin(),
                refs.end(),
                [&referenceRef, &sameSurfaceRef](const SurfaceRef& ref)
                {
                    return sameSurfaceRef(ref, *referenceRef);
                });
            if (it != refs.end() && it != refs.begin())
            {
                std::rotate(refs.begin(), it, it + 1);
            }
        }

        return refs;
    };

    auto refreshAfterUvEdit = [this]()
    {
        uvEditorForceDisplayRebuild_ = true;
        if (glView_)
        {
            glView_->notifyLevelGeometryChanged();
            glView_->setHighlightedSelection(document_.selection());
        }
        rebuildOutliner();
        rebuildProperties();
        updateEditActions();
        syncUvEditorPanel();
    };

    surfaceMenu->addSeparator();

    uvEditorAction_ = new QAction("&UV Editor...", this);
    uvEditorAction_->setObjectName("surface.uv.editor");
    uvEditorAction_->setToolTip("Show the docked UV editor for the selected surfaces.");
    connect(uvEditorAction_, &QAction::triggered, this, [this]()
        {
            showUvEditorDialog();
        });
    surfaceMenu->addAction(uvEditorAction_);

    uvAutoAlignAction_ = new QAction("&Auto Align Surface UV", this);
    uvAutoAlignAction_->setObjectName("surface.uv.auto_align");
    uvAutoAlignAction_->setToolTip(
        "Recalculate each selected surface's UVs from its geometry while preserving the existing U and V directions when possible.");
    connect(uvAutoAlignAction_, &QAction::triggered, this, [this, collectSelectedSurfaceRefsSorted, refreshAfterUvEdit, uvEditorHasActiveModalTransform]()
        {
            if (uvEditorHasActiveModalTransform())
            {
                return;
            }
            if (selectionTouchesNonEditableLayer(document_.selection()))
            {
                QMessageBox::information(this, "Auto Align UV", "Selection contains surfaces on hidden or locked layers.");
                return;
            }

            const std::vector<SurfaceRef> refs = collectSelectedSurfaceRefsSorted();
            if (refs.empty())
            {
                QMessageBox::information(this, "Auto Align UV", "Select one or more surfaces first.");
                return;
            }

            const bool applied = document_.applyUndoable(
                surfaceUvUndoLabel(refs.size(), "UVs auto-aligned").toStdString(),
                editor::DocumentDirty::LevelGeometry,
                [refs](editor::DocumentState& state) -> bool
                {
                    bool any = false;
                    const double pixelsPerUnit = ::sed::level::levelPixelPerUnit(state.level);
                    for (const SurfaceRef ref : refs)
                    {
                        if (ref.sector < 0 || ref.surface < 0 || static_cast<std::size_t>(ref.sector) >= state.level.sectors.size())
                        {
                            continue;
                        }

                        Sector& sec = state.level.sectors[static_cast<std::size_t>(ref.sector)];
                        if (static_cast<std::size_t>(ref.surface) >= sec.surfaces.size())
                        {
                            continue;
                        }

                        Surface& surf = sec.surfaces[static_cast<std::size_t>(ref.surface)];
                        if (surf.vertices.empty())
                        {
                            continue;
                        }

                        const std::vector<TexVertex> beforeUv = surf.texVertices;
                        if (!::sed::level::surfaceAutoAlignUV(pixelsPerUnit, sec, surf))
                        {
                            continue;
                        }
                        bool uvChanged = beforeUv.size() != surf.texVertices.size();
                        if (!uvChanged)
                        {
                            for (std::size_t uvIndex = 0; uvIndex < surf.texVertices.size(); ++uvIndex)
                            {
                                if (beforeUv[uvIndex].u != surf.texVertices[uvIndex].u
                                    || beforeUv[uvIndex].v != surf.texVertices[uvIndex].v)
                                {
                                    uvChanged = true;
                                    break;
                                }
                            }
                        }
                        any = any || uvChanged;
                    }
                    return any;
                });

            if (!applied)
            {
                QMessageBox::warning(this, "Auto Align UV", "No UVs were changed.");
                return;
            }

            refreshAfterUvEdit();
        });
    surfaceMenu->addAction(uvAutoAlignAction_);

    uvMoveAction_ = new QAction("Move Surface U&V...", this);
    uvMoveAction_->setObjectName("surface.uv.move");
    uvMoveAction_->setToolTip("Move the selected UVs by exact U and V offsets.");
    connect(uvMoveAction_, &QAction::triggered, this, [this, collectSelectedSurfaceRefsSorted, refreshAfterUvEdit, uvEditorHasActiveModalTransform]()
        {
            if (uvEditorHasActiveModalTransform())
            {
                return;
            }
            if (selectionTouchesNonEditableLayer(document_.selection()))
            {
                QMessageBox::information(this, "Move UV", "Selection contains surfaces on hidden or locked layers.");
                return;
            }

            const std::vector<SurfaceRef> refs = collectSelectedSurfaceRefsSorted();
            if (refs.empty())
            {
                QMessageBox::information(this, "Move UV", "Select one or more surfaces first.");
                return;
            }

            bool ok = false;
            const double du = ui::preciseNumericInput(this, "Move UV", "U offset", 0.0, -1'000'000.0, 1'000'000.0, &ok);
            if (!ok)
            {
                return;
            }
            const double dv = ui::preciseNumericInput(this, "Move UV", "V offset", 0.0, -1'000'000.0, 1'000'000.0, &ok);
            if (!ok)
            {
                return;
            }

            const bool applied = document_.applyUndoable(
                surfaceUvUndoLabel(refs.size(), "UVs moved").toStdString(),
                editor::DocumentDirty::LevelGeometry,
                [&](editor::DocumentState& state) -> bool
                {
                    const std::optional<std::vector<QSize>> textureSizes =
                        uvActionTextureSizes(state.level, refs, resources_);
                    if (!textureSizes)
                    {
                        return false;
                    }

                    bool any = false;
                    for (std::size_t refIndex = 0;
                         refIndex < refs.size();
                         ++refIndex)
                    {
                        Surface* surf = uvSurfaceForRef(
                            state.level, refs[refIndex]);
                        if (!surf
                            || surf->texVertices.size()
                                != surf->vertices.size()
                            || surf->texVertices.empty())
                        {
                            return false;
                        }
                        const double uOffset = du
                            * static_cast<double>((*textureSizes)[refIndex].width());
                        const double vOffset = dv
                            * static_cast<double>((*textureSizes)[refIndex].height());
                        std::vector<TexVertex> nextTexVertices =
                            surf->texVertices;
                        bool changed = false;
                        for (TexVertex& tv : nextTexVertices)
                        {
                            const std::optional<float> nextU = checkedUvFloat(
                                static_cast<double>(tv.u) + uOffset);
                            const std::optional<float> nextV = checkedUvFloat(
                                static_cast<double>(tv.v) + vOffset);
                            if (!nextU || !nextV)
                            {
                                return false;
                            }
                            if (tv.u != *nextU
                                || tv.v != *nextV)
                            {
                                tv.u = *nextU;
                                tv.v = *nextV;
                                changed = true;
                            }
                        }
                        if (changed)
                        {
                            surf->texVertices = std::move(nextTexVertices);
                        }
                        any = any || changed;
                    }
                    return any;
                });

            if (!applied)
            {
                QMessageBox::warning(this, "Move UV", "No UVs were changed.");
                return;
            }

            refreshAfterUvEdit();
        });
    surfaceMenu->addAction(uvMoveAction_);

    uvRotateAction_ = new QAction("Rotate Surface UV...", this);
    uvRotateAction_->setObjectName("surface.uv.rotate");
    uvRotateAction_->setToolTip("Rotate the selected UVs by an exact angle around their center.");
    connect(uvRotateAction_, &QAction::triggered, this, [this, collectSelectedSurfaceRefsSorted, refreshAfterUvEdit, uvEditorHasActiveModalTransform]()
        {
            if (uvEditorHasActiveModalTransform())
            {
                return;
            }
            if (selectionTouchesNonEditableLayer(document_.selection()))
            {
                QMessageBox::information(this, "Rotate UV", "Selection contains surfaces on hidden or locked layers.");
                return;
            }

            const std::vector<SurfaceRef> refs = collectSelectedSurfaceRefsSorted();
            if (refs.empty())
            {
                QMessageBox::information(this, "Rotate UV", "Select one or more surfaces first.");
                return;
            }

            bool ok = false;
            const double angleDeg =
                ui::preciseNumericInput(this, "Rotate UV", "Angle (deg)", 0.0, -3600.0, 3600.0, &ok);
            if (!ok)
            {
                return;
            }

            const bool applied = document_.applyUndoable(
                surfaceUvUndoLabel(refs.size(), "UVs rotated").toStdString(),
                editor::DocumentDirty::LevelGeometry,
                [&](editor::DocumentState& state) -> bool
                {
                    const std::optional<std::vector<QSize>> textureSizes =
                        uvActionTextureSizes(state.level, refs, resources_);
                    if (!textureSizes)
                    {
                        return false;
                    }

                    bool any = false;
                    for (std::size_t refIndex = 0;
                         refIndex < refs.size();
                         ++refIndex)
                    {
                        Surface* surf = uvSurfaceForRef(
                            state.level, refs[refIndex]);
                        if (!surf
                            || surf->texVertices.size()
                                != surf->vertices.size()
                            || surf->texVertices.empty())
                        {
                            return false;
                        }

                        const std::optional<bool> changed =
                            uvRotateTexVerticesAroundCenter(
                                surf->texVertices,
                                state.level.uvCoordinateSpace,
                                (*textureSizes)[refIndex],
                                angleDeg);
                        if (!changed)
                        {
                            return false;
                        }
                        any = any || *changed;
                    }
                    return any;
                });

            if (!applied)
            {
                QMessageBox::warning(this, "Rotate UV", "No UVs were changed.");
                return;
            }

            refreshAfterUvEdit();
        });
    surfaceMenu->addAction(uvRotateAction_);

    uvScaleAction_ = new QAction("Scale Surface UV...", this);
    uvScaleAction_->setObjectName("surface.uv.scale");
    uvScaleAction_->setToolTip("Scale the selected UVs by exact U and V factors around their center.");
    connect(uvScaleAction_, &QAction::triggered, this, [this, collectSelectedSurfaceRefsSorted, refreshAfterUvEdit, uvEditorHasActiveModalTransform]()
        {
            if (uvEditorHasActiveModalTransform())
            {
                return;
            }
            if (selectionTouchesNonEditableLayer(document_.selection()))
            {
                QMessageBox::information(this, "Scale UV", "Selection contains surfaces on hidden or locked layers.");
                return;
            }

            const std::vector<SurfaceRef> refs = collectSelectedSurfaceRefsSorted();
            if (refs.empty())
            {
                QMessageBox::information(this, "Scale UV", "Select one or more surfaces first.");
                return;
            }

            bool ok = false;
            const double scaleU = ui::preciseNumericInput(this, "Scale UV", "U factor", 1.0, -1'000'000.0, 1'000'000.0, &ok);
            if (!ok)
            {
                return;
            }
            const double scaleV = ui::preciseNumericInput(this, "Scale UV", "V factor", 1.0, -1'000'000.0, 1'000'000.0, &ok);
            if (!ok)
            {
                return;
            }

            const bool applied = document_.applyUndoable(
                surfaceUvUndoLabel(refs.size(), "UVs scaled").toStdString(),
                editor::DocumentDirty::LevelGeometry,
                [&](editor::DocumentState& state) -> bool
                {
                    bool any = false;
                    for (const SurfaceRef ref : refs)
                    {
                        if (ref.sector < 0 || ref.surface < 0 || static_cast<std::size_t>(ref.sector) >= state.level.sectors.size())
                        {
                            continue;
                        }
                        Sector& sec = state.level.sectors[static_cast<std::size_t>(ref.sector)];
                        if (static_cast<std::size_t>(ref.surface) >= sec.surfaces.size())
                        {
                            continue;
                        }
                        Surface& surf = sec.surfaces[static_cast<std::size_t>(ref.surface)];
                        if (surf.texVertices.size() != surf.vertices.size()
                            || surf.texVertices.empty())
                        {
                            return false;
                        }

                        const std::optional<bool> changed =
                            uvScaleTexVerticesAroundCenter(
                                surf.texVertices, scaleU, scaleV);
                        if (!changed)
                        {
                            return false;
                        }
                        any = any || *changed;
                    }
                    return any;
                });

            if (!applied)
            {
                QMessageBox::warning(this, "Scale UV", "No UVs were changed.");
                return;
            }

            refreshAfterUvEdit();
        });
    surfaceMenu->addAction(uvScaleAction_);

    uvStitchFromReferenceAction_ = new QAction("Stitch UV From &Reference", this);
    uvStitchFromReferenceAction_->setObjectName("surface.uv.stitch_from_reference");
    uvStitchFromReferenceAction_->setToolTip(
        "Align every selected target's UVs from the active reference surface. Each target is aligned independently; materials are unchanged.");
    connect(uvStitchFromReferenceAction_, &QAction::triggered, this, [this, collectSelectedSurfaceRefsReferenceFirst, refreshAfterUvEdit, uvEditorHasActiveModalTransform]()
        {
            if (uvEditorHasActiveModalTransform())
            {
                return;
            }
            if (selectionTouchesNonEditableLayer(document_.selection()))
            {
                QMessageBox::information(this, "Stitch UV", "Selection contains surfaces on hidden or locked layers.");
                return;
            }

            const std::vector<SurfaceRef> refs = collectSelectedSurfaceRefsReferenceFirst();
            if (refs.size() < 2)
            {
                QMessageBox::information(this, "Stitch UV", "Select at least two surfaces.");
                return;
            }

            const SurfaceRef source = refs.front();
            const bool applied = document_.applyUndoable(
                surfaceUvUndoLabel(refs.size(), "UVs stitched from reference").toStdString(),
                editor::DocumentDirty::LevelGeometry,
                [&](editor::DocumentState& state) -> bool
                {
                    const bool pixelSpace = state.level.uvCoordinateSpace
                        == UvCoordinateSpace::TexturePixels;
                    std::optional<std::vector<PixelUvSurfaceStage>>
                        pixelStages{};
                    if (pixelSpace)
                    {
                        pixelStages = normalizePixelUvSurfaces(
                            state.level, refs, resources_);
                        if (!pixelStages)
                        {
                            return false;
                        }
                    }

                    bool any = false;
                    std::vector<SurfaceRef> changedRefs{};
                    for (std::size_t i = 1; i < refs.size(); ++i)
                    {
                        if (::sed::level::levelStitchSurfaces(state.level, source, refs[i]))
                        {
                            any = true;
                            changedRefs.push_back(refs[i]);
                        }
                    }
                    if (pixelStages
                        && !finishPixelUvSurfaces(
                            state.level, *pixelStages, changedRefs))
                    {
                        return false;
                    }
                    return any;
                });

            if (!applied)
            {
                QMessageBox::warning(this, "Stitch UV", "No compatible target surfaces were stitched.");
                return;
            }

            refreshAfterUvEdit();
        });
    surfaceMenu->addAction(uvStitchFromReferenceAction_);

    uvStitchConnectedAction_ = new QAction("Stitch UV Across &Connected Selection", this);
    uvStitchConnectedAction_->setObjectName("surface.uv.stitch_connected");
    uvStitchConnectedAction_->setToolTip(
        "Align UVs outward from the active reference through selected surfaces joined by adjoins or shared edges. Use this for angled or banked surface runs; materials are unchanged.");
    connect(uvStitchConnectedAction_, &QAction::triggered, this, [this, collectSelectedSurfaceRefsReferenceFirst, refreshAfterUvEdit, uvEditorHasActiveModalTransform]()
        {
            if (uvEditorHasActiveModalTransform())
            {
                return;
            }
            if (selectionTouchesNonEditableLayer(document_.selection()))
            {
                QMessageBox::information(this, "Stitch UV (connected)", "Selection contains surfaces on hidden or locked layers.");
                return;
            }

            const std::vector<SurfaceRef> refs = collectSelectedSurfaceRefsReferenceFirst();
            if (refs.size() < 2)
            {
                QMessageBox::information(this, "Stitch UV (connected)", "Select at least two surfaces.");
                return;
            }

            std::size_t stitchedCount = 1;
            const bool applied = document_.applyUndoable(
                surfaceUvUndoLabel(refs.size(), "UVs stitched across connected selection").toStdString(),
                editor::DocumentDirty::LevelGeometry,
                [&](editor::DocumentState& state) -> bool
                {
                    const bool pixelSpace = state.level.uvCoordinateSpace
                        == UvCoordinateSpace::TexturePixels;
                    std::optional<std::vector<PixelUvSurfaceStage>>
                        pixelStages{};
                    if (pixelSpace)
                    {
                        pixelStages = normalizePixelUvSurfaces(
                            state.level, refs, resources_);
                        if (!pixelStages)
                        {
                            return false;
                        }
                    }

                    auto isValidRef = [&state](const SurfaceRef ref) -> bool
                    {
                        if (ref.sector < 0 || ref.surface < 0)
                        {
                            return false;
                        }
                        const std::size_t secIdx = static_cast<std::size_t>(ref.sector);
                        if (secIdx >= state.level.sectors.size())
                        {
                            return false;
                        }
                        const Sector& sec = state.level.sectors[secIdx];
                        return static_cast<std::size_t>(ref.surface) < sec.surfaces.size();
                    };

                    const auto verticesClose = ui::uv::uvVerticesCoincide;

                    auto surfacesShareEdge = [&state, &isValidRef, &verticesClose](const SurfaceRef aRef, const SurfaceRef bRef) -> bool
                    {
                        if (!isValidRef(aRef) || !isValidRef(bRef))
                        {
                            return false;
                        }

                        const Sector& aSec = state.level.sectors[static_cast<std::size_t>(aRef.sector)];
                        const Sector& bSec = state.level.sectors[static_cast<std::size_t>(bRef.sector)];
                        const Surface& a = aSec.surfaces[static_cast<std::size_t>(aRef.surface)];
                        const Surface& b = bSec.surfaces[static_cast<std::size_t>(bRef.surface)];

                        if (a.vertices.size() < 2 || b.vertices.size() < 2)
                        {
                            return false;
                        }

                        auto validIndex = [](const std::int32_t i, const std::size_t n) -> bool
                        {
                            return i >= 0 && static_cast<std::size_t>(i) < n;
                        };

                        for (std::size_t ai = 0; ai < a.vertices.size(); ++ai)
                        {
                            const std::int32_t a0i = a.vertices[ai];
                            const std::int32_t a1i = a.vertices[(ai + 1) % a.vertices.size()];
                            if (!validIndex(a0i, aSec.vertices.size()) || !validIndex(a1i, aSec.vertices.size()))
                            {
                                continue;
                            }
                            const Vertex& a0 = aSec.vertices[static_cast<std::size_t>(a0i)];
                            const Vertex& a1 = aSec.vertices[static_cast<std::size_t>(a1i)];

                            for (std::size_t bi = 0; bi < b.vertices.size(); ++bi)
                            {
                                const std::int32_t b0i = b.vertices[bi];
                                const std::int32_t b1i = b.vertices[(bi + 1) % b.vertices.size()];
                                if (!validIndex(b0i, bSec.vertices.size()) || !validIndex(b1i, bSec.vertices.size()))
                                {
                                    continue;
                                }

                                const Vertex& b0 = bSec.vertices[static_cast<std::size_t>(b0i)];
                                const Vertex& b1 = bSec.vertices[static_cast<std::size_t>(b1i)];
                                const bool sameDir = verticesClose(a0, b0) && verticesClose(a1, b1);
                                const bool oppositeDir = verticesClose(a0, b1) && verticesClose(a1, b0);
                                if (sameDir || oppositeDir)
                                {
                                    return true;
                                }
                            }
                        }

                        return false;
                    };

                    auto surfacesConnected = [&state, &isValidRef, &surfacesShareEdge](
                        const SurfaceRef aRef, const SurfaceRef bRef) -> bool
                    {
                        if (!isValidRef(aRef) || !isValidRef(bRef)) return false;
                        if (surfacesShareEdge(aRef, bRef)) return true;
                        const Sector& aSector = state.level.sectors[static_cast<std::size_t>(aRef.sector)];
                        const Sector& bSector = state.level.sectors[static_cast<std::size_t>(bRef.sector)];
                        const Surface& a = aSector.surfaces[static_cast<std::size_t>(aRef.surface)];
                        const Surface& b = bSector.surfaces[static_cast<std::size_t>(bRef.surface)];
                        const bool linked = (a.adjoin && *a.adjoin == bRef)
                            || (b.adjoin && *b.adjoin == aRef);
                        // A real portal can have different collinear edge subdivisions.
                        // Stored references alone must not connect separated geometry.
                        return linked && ::sed::level::surfaceDoOverlap(aSector, a, bSector, b);
                    };

                    auto isStitched = [](const std::vector<SurfaceRef>& stitched, const SurfaceRef ref) -> bool
                    {
                        return std::any_of(stitched.begin(), stitched.end(), [ref](const SurfaceRef v)
                            {
                                return v.sector == ref.sector && v.surface == ref.surface;
                            });
                    };

                    std::vector<SurfaceRef> stitchedRefs{};
                    stitchedRefs.reserve(refs.size());
                    if (isValidRef(refs.front()))
                    {
                        stitchedRefs.push_back(refs.front());
                    }
                    else
                    {
                        return false;
                    }

                    bool any = false;
                    std::vector<SurfaceRef> changedRefs{};
                    bool progress = true;
                    while (progress)
                    {
                        progress = false;
                        for (const SurfaceRef target : refs)
                        {
                            if (isStitched(stitchedRefs, target) || !isValidRef(target))
                            {
                                continue;
                            }

                            for (const SurfaceRef source : stitchedRefs)
                            {
                                if (!surfacesConnected(source, target))
                                {
                                    continue;
                                }

                                if (::sed::level::levelStitchSurfaces(state.level, source, target))
                                {
                                    stitchedRefs.push_back(target);
                                    changedRefs.push_back(target);
                                    any = true;
                                    progress = true;
                                    break;
                                }
                            }
                        }
                    }

                    stitchedCount = stitchedRefs.size();
                    if (pixelStages
                        && !finishPixelUvSurfaces(
                            state.level, *pixelStages, changedRefs))
                    {
                        return false;
                    }
                    return any;
                });

            if (!applied)
            {
                QMessageBox::warning(this, "Stitch UV (connected)", "No connected surfaces could be stitched.");
                return;
            }

            if (stitchedCount < refs.size())
            {
                QMessageBox::information(
                    this,
                    "Stitch UV (connected)",
                    QString("Stitched %1 of %2 selected surfaces.\nOnly connected surfaces are propagated.")
                        .arg(static_cast<int>(stitchedCount))
                        .arg(static_cast<int>(refs.size())));
            }

            refreshAfterUvEdit();
        });
    surfaceMenu->addAction(uvStitchConnectedAction_);

    uvCopyFromReferenceAction_ = new QAction("Copy Texture && UV From Reference", this);
    uvCopyFromReferenceAction_->setObjectName("surface.uv.copy_from_reference");
    uvCopyFromReferenceAction_->setToolTip(
        "Copy material, texture, scale, and flip from the active reference, then align each selected target's UVs. Matching topology can copy UV coordinates directly when alignment is unavailable.");
    connect(uvCopyFromReferenceAction_, &QAction::triggered, this, [this, collectSelectedSurfaceRefsReferenceFirst, refreshAfterUvEdit, uvEditorHasActiveModalTransform]()
        {
            if (uvEditorHasActiveModalTransform())
            {
                return;
            }
            if (selectionTouchesNonEditableLayer(document_.selection()))
            {
                QMessageBox::information(this, "Copy Texture/UV", "Selection contains surfaces on hidden or locked layers.");
                return;
            }

            const std::vector<SurfaceRef> refs = collectSelectedSurfaceRefsReferenceFirst();
            if (refs.size() < 2)
            {
                QMessageBox::information(this, "Copy Texture/UV", "Select at least two surfaces.");
                return;
            }

            const SurfaceRef source = refs.front();
            const bool applied = document_.applyUndoable(
                surfaceUvUndoLabel(refs.size(), "UVs copied from reference").toStdString(),
                editor::DocumentDirty::LevelGeometry,
                [&](editor::DocumentState& state) -> bool
                {
                    if (source.sector < 0 || source.surface < 0
                        || static_cast<std::size_t>(source.sector) >= state.level.sectors.size()
                        || static_cast<std::size_t>(source.surface) >= state.level.sectors[static_cast<std::size_t>(source.sector)].surfaces.size())
                    {
                        return false;
                    }

                    const Surface sourceSurface =
                        state.level.sectors[static_cast<std::size_t>(source.sector)].surfaces[static_cast<std::size_t>(source.surface)];

                    const auto hasFiniteCorners = [](const Sector& sector, const Surface& surface)
                    {
                        return std::ranges::all_of(surface.vertices, [&sector](const std::int32_t index)
                        {
                            if (index < 0 || static_cast<std::size_t>(index) >= sector.vertices.size()) return false;
                            const Vertex& vertex = sector.vertices[static_cast<std::size_t>(index)];
                            return std::isfinite(vertex.x) && std::isfinite(vertex.y) && std::isfinite(vertex.z);
                        });
                    };

                    // Stitch can reject a degenerate source and the equal-corner
                    // fallback may still copy its authored UVs. Validate those
                    // payloads independently before copying any material metadata.
                    if (sourceSurface.vertices.size() < 3
                        || !hasFiniteCorners(state.level.sectors[static_cast<std::size_t>(source.sector)], sourceSurface)
                        || sourceSurface.texVertices.size() != sourceSurface.vertices.size()
                        || !std::isfinite(sourceSurface.uScale)
                        || !std::isfinite(sourceSurface.vScale)
                        || std::ranges::any_of(sourceSurface.texVertices, [](const TexVertex& vertex)
                            { return !std::isfinite(vertex.u) || !std::isfinite(vertex.v); }))
                    {
                        return false;
                    }

                    bool any = false;
                    for (std::size_t i = 1; i < refs.size(); ++i)
                    {
                        const SurfaceRef target = refs[i];
                        if (target.sector < 0 || target.surface < 0 || static_cast<std::size_t>(target.sector) >= state.level.sectors.size())
                        {
                            continue;
                        }
                        Sector& sec = state.level.sectors[static_cast<std::size_t>(target.sector)];
                        if (static_cast<std::size_t>(target.surface) >= sec.surfaces.size())
                        {
                            continue;
                        }

                        Surface& dst = sec.surfaces[static_cast<std::size_t>(target.surface)];
                        if (dst.vertices.size() < 3 || dst.texVertices.size() != dst.vertices.size()
                            || !hasFiniteCorners(sec, dst))
                        {
                            continue;
                        }
                        bool changed = false;

                        if (dst.material != sourceSurface.material)
                        {
                            dst.material = sourceSurface.material;
                            changed = true;
                        }
                        if (dst.nMat != sourceSurface.nMat)
                        {
                            dst.nMat = sourceSurface.nMat;
                            changed = true;
                        }
                        if (dst.tex != sourceSurface.tex)
                        {
                            dst.tex = sourceSurface.tex;
                            changed = true;
                        }
                        if (dst.uScale != sourceSurface.uScale)
                        {
                            dst.uScale = sourceSurface.uScale;
                            changed = true;
                        }
                        if (dst.vScale != sourceSurface.vScale)
                        {
                            dst.vScale = sourceSurface.vScale;
                            changed = true;
                        }

                        const std::uint32_t srcFlip = sourceSurface.faceFlags & FF_SF_FLIP;
                        const std::uint32_t dstFlip = dst.faceFlags & FF_SF_FLIP;
                        if (srcFlip != dstFlip)
                        {
                            dst.faceFlags = (dst.faceFlags & ~FF_SF_FLIP) | srcFlip;
                            changed = true;
                        }

                        bool uvChanged = ::sed::level::levelStitchSurfaces(state.level, source, target);
                        if (!uvChanged && dst.vertices.size() == sourceSurface.vertices.size())
                        {
                            bool texVerticesDiffer = dst.texVertices.size() != sourceSurface.texVertices.size();
                            if (!texVerticesDiffer)
                            {
                                for (std::size_t vertexIndex = 0; vertexIndex < dst.texVertices.size(); ++vertexIndex)
                                {
                                    if (dst.texVertices[vertexIndex].u != sourceSurface.texVertices[vertexIndex].u
                                        || dst.texVertices[vertexIndex].v != sourceSurface.texVertices[vertexIndex].v)
                                    {
                                        texVerticesDiffer = true;
                                        break;
                                    }
                                }
                            }

                            if (texVerticesDiffer)
                            {
                                // Texture/UV copying does not edit target corner
                                // lighting. Preserve RGB and intensity individually.
                                for (std::size_t vertexIndex = 0; vertexIndex < dst.texVertices.size(); ++vertexIndex)
                                {
                                    dst.texVertices[vertexIndex].u = sourceSurface.texVertices[vertexIndex].u;
                                    dst.texVertices[vertexIndex].v = sourceSurface.texVertices[vertexIndex].v;
                                }
                                uvChanged = true;
                            }
                        }

                        if (uvChanged || changed)
                        {
                            any = true;
                        }
                    }

                    return any;
                });

            if (!applied)
            {
                QMessageBox::warning(this, "Copy Texture/UV", "No compatible target surfaces were updated.");
                return;
            }

            refreshAfterUvEdit();
        });
    surfaceMenu->addAction(uvCopyFromReferenceAction_);
}

void MainWindow::setupUvEditorPanel()
{
    if (!uvEditorPanelHost_)
    {
        return;
    }

    auto* hostLayout = new QVBoxLayout(uvEditorPanelHost_);
    hostLayout->setContentsMargins(0, 0, 0, 0);
    hostLayout->setSpacing(0);

    uvEditorPanelHeader_ = new QWidget(uvEditorPanelHost_);
    uvEditorPanelHeader_->setObjectName("uvEditorPanelHeader");
    uvEditorPanelHeader_->installEventFilter(this);
    uvEditorPanelHeader_->setFixedHeight(28);
    uvEditorPanelHeader_->setToolTip("Drag the UV editor header to dock it on another viewport edge.");
    auto* headerLayout = new QHBoxLayout(uvEditorPanelHeader_);
    headerLayout->setContentsMargins(5, 1, 4, 1);
    headerLayout->setSpacing(3);

    auto* titleLabel = new QLabel("UV Editor", uvEditorPanelHeader_);
    titleLabel->setObjectName("uvEditorPanelTitleLabel");
    titleLabel->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    titleLabel->setStyleSheet("color:#e7edf8;font-weight:700;font-size:11px;");
    titleLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    headerLayout->addWidget(titleLabel);
    headerLayout->addStretch(1);
    uvEditorPanelHeader_->setCursor(Qt::OpenHandCursor);

    uvEditorPanelDockButton_ = new QToolButton(uvEditorPanelHeader_);
    uvEditorPanelDockButton_->setObjectName("uvEditorDockButton");
    uvEditorPanelDockButton_->setAutoRaise(true);
    uvEditorPanelDockButton_->setText("Dock");
    uvEditorPanelDockButton_->setMinimumWidth(58);
    uvEditorPanelDockButton_->setToolButtonStyle(Qt::ToolButtonTextOnly);
    uvEditorPanelDockButton_->setPopupMode(QToolButton::InstantPopup);
    uvEditorPanelDockButton_->setFocusPolicy(Qt::NoFocus);
    uvEditorPanelDockButton_->setToolTip("Choose which side of the viewport contains the UV editor.");
    auto* dockMenu = new ui::shortcuts::KeySequenceDisplayMenu(uvEditorPanelDockButton_);
    dockMenu->setToolTipsVisible(true);
    auto addDockAction = [this, dockMenu](
                             const QString& text,
                             const QString& objectName,
                             const QString& toolTip,
                             const UvEditorDockPosition position)
    {
        QAction* action = dockMenu->addAction(text);
        action->setObjectName(objectName);
        action->setToolTip(toolTip);
        connect(action, &QAction::triggered, this, [this, position]()
            {
                applyUvEditorDockPosition(position);
            });
    };
    addDockAction(
        "Bottom",
        "uvEditorDockBottomAction",
        "Dock the UV editor below the viewport.",
        UvEditorDockPosition::bottom);
    addDockAction(
        "Top",
        "uvEditorDockTopAction",
        "Dock the UV editor above the viewport.",
        UvEditorDockPosition::top);
    addDockAction(
        "Left",
        "uvEditorDockLeftAction",
        "Dock the UV editor to the left of the viewport.",
        UvEditorDockPosition::left);
    addDockAction(
        "Right",
        "uvEditorDockRightAction",
        "Dock the UV editor to the right of the viewport.",
        UvEditorDockPosition::right);
    uvEditorPanelDockButton_->setMenu(dockMenu);
    headerLayout->addWidget(uvEditorPanelDockButton_);

    uvEditorPanelToggleButton_ = new QToolButton(uvEditorPanelHeader_);
    uvEditorPanelToggleButton_->setObjectName("uvEditorToggleButton");
    uvEditorPanelToggleButton_->setAutoRaise(true);
    uvEditorPanelToggleButton_->setToolButtonStyle(Qt::ToolButtonIconOnly);
    uvEditorPanelToggleButton_->setIconSize(QSize(16, 16));
    uvEditorPanelToggleButton_->setFocusPolicy(Qt::NoFocus);
    uvEditorPanelToggleButton_->setIcon(makeUvPanelChevronIcon(0.0, QColor(159, 176, 200)));
    uvEditorPanelToggleButton_->setProperty("uvChevronAngle", 0.0);
    uvEditorPanelToggleButton_->setToolTip("Collapse UV editor");
    headerLayout->addWidget(uvEditorPanelToggleButton_);
    hostLayout->addWidget(uvEditorPanelHeader_);

    uvEditorPanelBody_ = new QWidget(uvEditorPanelHost_);
    auto* bodyLayout = new QVBoxLayout(uvEditorPanelBody_);
    bodyLayout->setContentsMargins(0, 0, 0, 0);
    bodyLayout->setSpacing(0);

    auto applyUvIslandsToState = [](
        editor::DocumentState& state,
        const std::vector<UvIslandState>& updatedIslands,
        std::vector<SurfaceRef>* changedRefs) -> std::optional<bool>
    {
        if (state.level.uvCoordinateSpace != UvCoordinateSpace::Normalized
            && state.level.uvCoordinateSpace
                != UvCoordinateSpace::TexturePixels)
        {
            return std::nullopt;
        }

        struct PendingUvEdit
        {
            Surface* surface{};
            SurfaceRef ref{};
            std::vector<TexVertex> texVertices{};
        };
        std::vector<PendingUvEdit> pending{};
        pending.reserve(updatedIslands.size());

        for (const UvIslandState& island : updatedIslands)
        {
            const SurfaceRef ref = island.ref;
            if (ref.sector < 0 || ref.surface < 0 || static_cast<std::size_t>(ref.sector) >= state.level.sectors.size())
            {
                return std::nullopt;
            }

            Sector& sector = state.level.sectors[static_cast<std::size_t>(ref.sector)];
            if (static_cast<std::size_t>(ref.surface) >= sector.surfaces.size())
            {
                return std::nullopt;
            }

            Surface& surface = sector.surfaces[static_cast<std::size_t>(ref.surface)];
            if (surface.texVertices.size() != surface.vertices.size()
                || island.uv.size() != surface.vertices.size()
                || !std::isfinite(surface.uScale)
                || !std::isfinite(surface.vScale)
                || std::ranges::any_of(
                    surface.texVertices,
                    [](const TexVertex& texVertex)
                    {
                        return !std::isfinite(texVertex.u)
                            || !std::isfinite(texVertex.v);
                    })
                || std::ranges::any_of(
                    pending,
                    [&surface](const PendingUvEdit& edit)
                    {
                        return edit.surface == &surface;
                    }))
            {
                return std::nullopt;
            }

            std::vector<TexVertex> candidate = surface.texVertices;
            bool surfaceChanged = false;
            for (std::size_t i = 0; i < candidate.size(); ++i)
            {
                const QPointF canonicalUv = islandStoredFromDisplayUv(
                    island, island.uv[i]);
                const QPointF storedUv =
                    state.level.uvCoordinateSpace
                        == UvCoordinateSpace::Normalized
                    ? canonicalUv
                    : denormalizeStoredUv(
                        canonicalUv, island.textureSize);
                if (!std::isfinite(storedUv.x())
                    || !std::isfinite(storedUv.y())
                    || storedUv.x()
                        < -static_cast<qreal>(
                            std::numeric_limits<float>::max())
                    || storedUv.x()
                        > static_cast<qreal>(
                            std::numeric_limits<float>::max())
                    || storedUv.y()
                        < -static_cast<qreal>(
                            std::numeric_limits<float>::max())
                    || storedUv.y()
                        > static_cast<qreal>(
                            std::numeric_limits<float>::max()))
                {
                    return std::nullopt;
                }
                const float nextU = static_cast<float>(storedUv.x());
                const float nextV = static_cast<float>(storedUv.y());
                // Compare the values actually stored in the document, not a
                // display-space epsilon that can swallow valid small edits.
                if ( candidate[i].u != nextU || candidate[i].v != nextV )
                {
                    candidate[i].u = nextU;
                    candidate[i].v = nextV;
                    surfaceChanged = true;
                }
            }
            if (surfaceChanged)
            {
                pending.push_back(PendingUvEdit{
                    &surface,
                    ref,
                    std::move(candidate),
                });
            }
        }

        for (PendingUvEdit& edit : pending)
        {
            edit.surface->texVertices = std::move(edit.texVertices);
            if (changedRefs)
            {
                changedRefs->push_back(edit.ref);
            }
        }
        return !pending.empty();
    };

    auto notifyUvPreviewChanged = [this](const std::vector<SurfaceRef>* changedRefs = nullptr)
    {
        if (glView_)
        {
            if (changedRefs)
            {
                glView_->notifySurfaceUvsChanged(*changedRefs);
            }
            else
            {
                glView_->notifyLevelGeometryChanged();
                glView_->setHighlightedSelection(document_.selection());
            }
        }
    };

    if (!uvEditorPanelDockPreviewBand_)
    {
        // QSplitter automatically adopts every direct non-window QWidget
        // child as a pane.  A rubber band parented to the splitter therefore
        // becomes an unintended hidden third pane and makes all two-pane
        // setSizes() requests subject to splitter normalization.  Keep the
        // visual overlay in the containing workspace instead; the drag path
        // maps splitter-local preview geometry into this parent.
        uvEditorPanelDockPreviewBand_ = new QRubberBand(
            QRubberBand::Rectangle,
            workspaceWindow_ ? static_cast<QWidget*>(workspaceWindow_) : this);
        uvEditorPanelDockPreviewBand_->setObjectName(
            QStringLiteral("uvEditorPanelDockPreviewBand"));
        uvEditorPanelDockPreviewBand_->setStyleSheet(
            "QRubberBand {"
            "  border:2px solid rgba(112,178,255,220);"
            "  background:rgba(112,178,255,42);"
            "  border-radius:8px;"
            "}");
        uvEditorPanelDockPreviewBand_->hide();
    }

    UvEditorPanel* panel = nullptr;
    panel = new UvEditorPanel(
        [this](std::vector<UvIslandState>& updatedIslands, const SmartUvProjectOptions& options, const QSize& previewTextureSize)
        {
            if (!computeSmartUvProject(
                document_.level(),
                updatedIslands,
                options,
                previewTextureSize,
                uvEditorPanelWidget_
                    ? static_cast<UvEditorPanel*>(uvEditorPanelWidget_)->activeSurfaceRef()
                    : std::nullopt))
            {
                updatedIslands.clear();
                return;
            }
            for (UvIslandState& island : updatedIslands)
            {
                if (!island.unwrapUv.empty())
                {
                    island.uv = island.unwrapUv;
                    const QRectF projectedBounds = islandBounds(island);
                    island.displayOffset = {};
                    island.displayMirrorX = false;
                    island.displayMirrorAxisX = projectedBounds.isValid() ? projectedBounds.center().x() : 0.0;
                }
            }
        },
        [this](
            std::vector<UvIslandState>& islands,
            const std::vector<bool>& selectedFaces,
            const std::optional<SurfaceRef> activeFace,
            const FollowActiveQuadsMode mode,
            FollowActiveQuadsReport* report)
        {
            return followActiveQuads(
                document_.level(),
                islands,
                selectedFaces,
                activeFace,
                mode,
                report);
        },
        [this, applyUvIslandsToState, notifyUvPreviewChanged](const std::vector<UvIslandState>& updatedIslands)
        {
            std::vector<SurfaceRef> changedRefs{};
            changedRefs.reserve(updatedIslands.size());
            if (applyUvIslandsToState(
                    document_.current(), updatedIslands, &changedRefs)
                    .value_or(false))
            {
                // Modal mouse motion changes only existing texture coordinates.
                // Patch the affected GPU ranges instead of rebuilding all level
                // geometry on every pointer event.
                notifyUvPreviewChanged(&changedRefs);
            }
        },
        [this, applyUvIslandsToState, notifyUvPreviewChanged](const std::vector<UvIslandState>& before, const std::vector<UvIslandState>& after, const QString& label)
        {
            const std::optional<bool> restored = applyUvIslandsToState(
                document_.current(), before, nullptr);
            if (!restored)
            {
                uvEditorForceDisplayRebuild_ = true;
                notifyUvPreviewChanged();
                syncUvEditorPanel();
                return;
            }

            // The panel supplies only the completed UV operation. Resolve the
            // owner at the document boundary so a multi-surface edit gets a
            // real plural label even when its UV islands were rebuilt.
            const QString ownerLabel = surfaceUvUndoLabel(
                document_.selection().surfaces.size(),
                label.toStdString());

            const bool applied = document_.applyUndoable(
                ownerLabel.toStdString(),
                editor::DocumentDirty::LevelGeometry,
                [&after, &applyUvIslandsToState](editor::DocumentState& state) -> bool
                {
                    return applyUvIslandsToState(
                        state, after, nullptr).value_or(false);
                });

            if (!applied)
            {
                if (applyUvIslandsToState(
                        document_.current(), after, nullptr)
                        .value_or(false))
                {
                    uvEditorForceDisplayRebuild_ = true;
                    notifyUvPreviewChanged();
                    syncUvEditorPanel();
                }
                return;
            }

            uvEditorForceDisplayRebuild_ = true;
            notifyUvPreviewChanged();
            rebuildOutliner();
            rebuildProperties();
            updateEditActions();
            syncUvEditorPanel();
        },
        [this](const bool enabled)
        {
            uvEditorViewportFacing_ = enabled;
            syncUvEditorViewportFacingOrientation();
            noteRecoveryStateChanged();
        },
        uvAutoAlignAction_,
        uvMoveAction_,
        uvRotateAction_,
        uvScaleAction_,
        uvStitchFromReferenceAction_,
        uvStitchConnectedAction_,
        uvCopyFromReferenceAction_,
        uvEditorPanelBody_);
    uvEditorPanelWidget_ = panel;
    panel->setSettledWorkspaceChangedCallback(
        [this]() { noteRecoveryStateChanged(); });
    panel->setHorizontalDock(
        uvEditorDockPosition_ == UvEditorDockPosition::bottom
        || uvEditorDockPosition_ == UvEditorDockPosition::top);
    bodyLayout->addWidget(panel);
    hostLayout->addWidget(uvEditorPanelBody_, 1);

    uvEditorPanelHost_->setStyleSheet(
        "#uvEditorPanelHost {"
        "  background:#141a23;"
        "  border-top:1px solid rgba(255,255,255,0.08);"
        "}"
        "#uvEditorPanelHeader {"
        "  background:#1a212c;"
        "  border-bottom:1px solid rgba(255,255,255,0.08);"
        "}"
        "#uvEditorPanelHeader QToolButton {"
        "  background:transparent;"
        "  border:0;"
        "  color:#9fb0c8;"
        "  padding:3px 7px;"
        "  border-radius:4px;"
        "  min-height:20px;"
        "  font-size:11px;"
        "}"
        "#uvEditorPanelHeader QToolButton#uvEditorDockButton {"
        "  min-width:58px;"
        "  padding:3px 18px 3px 7px;"
        "}"
        "#uvEditorPanelHeader QToolButton#uvEditorDockButton::menu-indicator {"
        "  subcontrol-origin:padding;"
        "  subcontrol-position:right center;"
        "  right:5px;"
        "}"
        "#uvEditorPanelHeader QToolButton:hover {"
        "  background:rgba(255,255,255,0.06);"
        "  color:#eef2f8;"
        "}"
        "#uvEditorPanel QPushButton, #uvEditorPanel QToolButton {"
        "  background:#2a313c;"
        "  border:1px solid rgba(255,255,255,0.08);"
        "  color:#ecf1f8;"
        "  padding:1px 6px;"
        "  border-radius:5px;"
        "  min-height:10px;"
        "  font-size:9px;"
        "  font-weight:500;"
        "}"
        "#uvEditorPanel QPushButton:hover, #uvEditorPanel QToolButton:hover {"
        "  background:#333c49;"
        "}"
        "#uvEditorPanel QPushButton:pressed, #uvEditorPanel QToolButton:pressed {"
        "  background:#252c36;"
        "}"
        "#uvEditorPanel QPushButton:disabled, #uvEditorPanel QToolButton:disabled {"
        "  color:#7f8794;"
        "  border-color:#454b53;"
        "  background:#252a31;"
        "}");

    connect(uvEditorPanelToggleButton_, &QToolButton::clicked, this, [this]()
        {
            setUvEditorPanelExpanded(!uvEditorPanelExpanded_);
        });

    connect(viewportUvSplitter_, &QSplitter::splitterMoved, this, [this](const int handlePosition, int)
        {
            if (!uvEditorPanelHost_ || !uvEditorPanelHost_->isVisible() || !uvEditorPanelExpanded_)
            {
                return;
            }
            if (uvEditorPanelProgrammaticResize_
                || previewUvEditorRestoreInProgress_)
            {
                return;
            }

            const bool vertical =
                uvEditorDockPosition_ == UvEditorDockPosition::bottom
                || uvEditorDockPosition_ == UvEditorDockPosition::top;
            uvEditorPanelExpandedHeight_ = std::max(
                1,
                vertical ? uvEditorPanelHost_->height() : uvEditorPanelHost_->width());
            if (glView_)
            {
                const bool panelFirst =
                    uvEditorDockPosition_ == UvEditorDockPosition::top
                    || uvEditorDockPosition_ == UvEditorDockPosition::left;
                applyUvEditorViewportZoomCompensation(viewportSizeAfterPanelSplitterMove(
                    glView_->size(),
                    viewportUvSplitter_->size(),
                    handlePosition,
                    viewportUvSplitter_->handleWidth(),
                    vertical,
                    panelFirst));
            }
        });

    uvEditorPanelBody_->setMinimumHeight(0);
    uvEditorPanelBody_->setMinimumWidth(0);
    uvEditorPanelHost_->hide();
    applyUvEditorDockPosition(uvEditorDockPosition_, false);
    setUvEditorPanelExpanded(true);
}

void MainWindow::prepareUvEditorViewportZoomCompensation(const QSize& baselineViewportSize)
{
    if (!glView_ || uvEditorViewportZoomSaved_ || baselineViewportSize.width() <= 0 || baselineViewportSize.height() <= 0)
    {
        return;
    }

    uvEditorViewportZoomSaved_ = true;
    uvEditorViewportZoomBaseSize_ = baselineViewportSize;
    uvEditorViewportZoomLastTargetSize_ = baselineViewportSize;
}

void MainWindow::applyUvEditorViewportZoomCompensation(const QSize& targetViewportSize)
{
    if (!glView_ || !uvEditorViewportZoomSaved_ || targetViewportSize.width() <= 0 || targetViewportSize.height() <= 0)
    {
        return;
    }
    if (uvEditorViewportZoomLastTargetSize_ == targetViewportSize)
    {
        return;
    }

    const QSize baselineSize =
        (uvEditorViewportZoomLastTargetSize_.width() > 0 && uvEditorViewportZoomLastTargetSize_.height() > 0)
            ? uvEditorViewportZoomLastTargetSize_
            : uvEditorViewportZoomBaseSize_;
    if (glView_->cameraProjectionType() == app::ViewportCamera::ProjectionType::Orthographic)
    {
        glView_->setCameraOrthoScale(compensatedOrthoScaleForPanelResize(
            glView_->cameraOrthoScale(),
            baselineSize,
            targetViewportSize));
        uvEditorViewportZoomLastTargetSize_ = targetViewportSize;
        return;
    }

    glView_->setCameraOrbitDistance(compensatedPerspectiveDistanceForPanelResize(
        glView_->cameraOrbitDistance(),
        baselineSize,
        targetViewportSize));
    uvEditorViewportZoomLastTargetSize_ = targetViewportSize;
}

void MainWindow::restoreUvEditorViewportZoomCompensation()
{
    if (!uvEditorViewportZoomSaved_)
    {
        return;
    }

    // Compensation is applied incrementally while the splitter changes size.
    // Do not restore the camera values captured when the panel opened: doing
    // so discards navigation performed in Surface mode and can add a second,
    // much larger zoom step when switching back to another edit mode.  Once
    // the viewport has expanded again, its final incremental compensation is
    // already the exact inverse of the opening resize.
    uvEditorViewportZoomSaved_ = false;
    uvEditorViewportZoomBaseSize_ = {};
    uvEditorViewportZoomLastTargetSize_ = {};
}

void MainWindow::hideUvEditorPanelWithTransition()
{
    if (!uvEditorPanelHost_ || !uvEditorPanelBody_ || !viewportUvSplitter_)
    {
        restoreUvEditorViewportZoomCompensation();
        if (uvEditorPanelHost_)
        {
            uvEditorPanelHost_->setVisible(false);
        }
        return;
    }

    if (uvEditorPanelHideTransitionActive_)
    {
        return;
    }

    // Track the panel's own presentation state, not the effective visibility
    // inherited from the MainWindow.  Project/edit-mode setup can run before
    // the top-level window is shown; treating that as a hidden UV panel makes
    // every routine refresh try to present it again.
    if (uvEditorPanelHost_->isHidden())
    {
        restoreUvEditorViewportZoomCompensation();
        setUvEditorPanelTransitionCollapsible(viewportUvSplitter_, uvEditorPanelHost_, false);
        uvEditorPanelHost_->setVisible(false);
        return;
    }

    if (uvEditorPanelAnimation_)
    {
        uvEditorPanelAnimation_->stop();
        uvEditorPanelAnimation_->deleteLater();
        uvEditorPanelAnimation_ = nullptr;
    }

    const bool vertical = (uvEditorDockPosition_ == UvEditorDockPosition::bottom || uvEditorDockPosition_ == UvEditorDockPosition::top);
    const bool panelFirst = (uvEditorDockPosition_ == UvEditorDockPosition::top || uvEditorDockPosition_ == UvEditorDockPosition::left);
    const QList<int> sizes = viewportUvSplitter_->sizes();
    const int currentTotal = uvSplitterAvailableChildExtent(
        viewportUvSplitter_);
    const int currentExtent = (sizes.size() >= 2)
        ? sizes[panelFirst ? 0 : 1]
        : std::max(1, vertical ? uvEditorPanelHost_->height() : uvEditorPanelHost_->width());
    const int totalExtent = currentTotal > 1
        ? currentTotal
        : std::max(1, vertical ? viewportUvSplitter_->height() : viewportUvSplitter_->width());

    if (uvEditorViewportZoomSaved_ && glView_)
    {
        // Rebase after ordinary window/splitter resizing so the hide animation
        // compensates only for the UV panel transition itself.
        uvEditorViewportZoomLastTargetSize_ = glView_->size();
    }

    uvEditorPanelHideTransitionActive_ = true;
    // The workspace splitter is normally non-collapsible so a user cannot
    // accidentally drag the UV editor away.  Temporarily allow only the UV
    // child to pass below its layout minimum while the transition owns it.
    setUvEditorPanelTransitionCollapsible(viewportUvSplitter_, uvEditorPanelHost_, true);
    uvEditorPanelBody_->setMaximumHeight(0);
    uvEditorPanelBody_->setMaximumWidth(0);
    uvEditorPanelHost_->setMinimumHeight(0);
    uvEditorPanelHost_->setMinimumWidth(0);
    uvEditorPanelHost_->setMaximumHeight(QWIDGETSIZE_MAX);
    uvEditorPanelHost_->setMaximumWidth(QWIDGETSIZE_MAX);
    if (uvEditorPanelDockButton_)
    {
        uvEditorPanelDockButton_->setVisible(false);
    }
    if (uvEditorPanelToggleButton_)
    {
        uvEditorPanelToggleButton_->setToolTip("Expand UV editor");
        uvEditorPanelToggleButton_->setProperty("uvChevronAngle", 180.0);
        uvEditorPanelToggleButton_->setIcon(makeUvPanelChevronIcon(180.0, QColor(159, 176, 200)));
    }

    auto viewportSizeForPanelExtent = [this, vertical, totalExtent](const int panelExtent) -> QSize
    {
        QSize targetViewportSize = glView_ ? glView_->size() : QSize{};
        // QSplitter removes its handle when the panel is hidden. Include that
        // final recovered strip in the inverse zoom compensation rather than
        // leaving a small mode-switch framing residue.
        const int recoveredHandle = panelExtent <= 0 && viewportUvSplitter_
            ? viewportUvSplitter_->handleWidth()
            : 0;
        const int targetViewportExtent = std::max(1, totalExtent - panelExtent + recoveredHandle);
        if (vertical)
        {
            targetViewportSize.setHeight(targetViewportExtent);
        }
        else
        {
            targetViewportSize.setWidth(targetViewportExtent);
        }
        return targetViewportSize;
    };

    auto applySplitterExtent = [this, panelFirst, totalExtent, viewportSizeForPanelExtent](const double rawExtent)
    {
        const int extent = std::clamp(static_cast<int>(std::lround(rawExtent)), 0, std::max(1, totalExtent - 1));
        uvEditorPanelProgrammaticResize_ = true;
        if (panelFirst)
        {
            viewportUvSplitter_->setSizes({ extent, std::max(1, totalExtent - extent) });
        }
        else
        {
            viewportUvSplitter_->setSizes({ std::max(1, totalExtent - extent), extent });
        }
        uvEditorPanelProgrammaticResize_ = false;
        applyUvEditorViewportZoomCompensation(viewportSizeForPanelExtent(extent));
    };

    uvEditorPanelAnimation_ = new QVariantAnimation(this);
    uvEditorPanelAnimation_->setObjectName(
        QStringLiteral("uvEditorPanelAnimation"));
    ++uvEditorPanelTransitionSerial_;
    uvEditorPanelAnimation_->setStartValue(static_cast<double>(currentExtent));
    uvEditorPanelAnimation_->setEndValue(0.0);
    uvEditorPanelAnimation_->setDuration(kUvEditorPanelTransitionMs);
    uvEditorPanelAnimation_->setEasingCurve(makeUvEditorPanelEasingCurve());
    connect(uvEditorPanelAnimation_, &QVariantAnimation::valueChanged, this, [applySplitterExtent](const QVariant& value)
        {
            applySplitterExtent(value.toDouble());
        });
    connect(uvEditorPanelAnimation_, &QVariantAnimation::finished, this, [this]()
        {
            uvEditorPanelHideTransitionActive_ = false;
            uvEditorPanelHost_->setVisible(false);
            setUvEditorPanelTransitionCollapsible(viewportUvSplitter_, uvEditorPanelHost_, false);
            uvEditorPanelBody_->hide();
            uvEditorPanelBody_->setMaximumHeight(QWIDGETSIZE_MAX);
            uvEditorPanelBody_->setMaximumWidth(QWIDGETSIZE_MAX);
            restoreUvEditorViewportZoomCompensation();
            if (uvEditorPanelAnimation_)
            {
                uvEditorPanelAnimation_->deleteLater();
                uvEditorPanelAnimation_ = nullptr;
            }
        });
    uvEditorPanelAnimation_->start();
}

void MainWindow::settleUvEditorPanelTransitionForGamePreview()
{
    // This sidecar is valid only for the capture which immediately follows
    // this settle call while entering Preview.
    uvEditorPreviewSettledLogicalSplitterSizes_.clear();
    uvEditorPreviewSettledLogicalDocumentEpoch_ = 0;
    if (!uvEditorPanelAnimation_)
    {
        return;
    }

    const bool finishingHide = uvEditorPanelHideTransitionActive_;
    const int requestedEndpoint = static_cast<int>(std::lround(
        uvEditorPanelAnimation_->endValue().toDouble()));
    uvEditorPanelAnimation_->stop();
    uvEditorPanelAnimation_->deleteLater();
    uvEditorPanelAnimation_ = nullptr;

    if (!finishingHide)
    {
        // Expansion/collapse already records its requested endpoint in
        // uvEditorPanelExpanded_. Apply it through the normal non-animated
        // splitter path before Preview captures the workspace allocation.
        // Compute the complete logical allocation before applying it because
        // QSplitter may defer realizing/clamping those weights until its next
        // layout pass. Preview must not pump events merely to replace that
        // intent with timing-dependent pixels.
        const bool panelFirst = uvEditorDockPosition_ == UvEditorDockPosition::top
            || uvEditorDockPosition_ == UvEditorDockPosition::left;
        const int totalExtent = std::max(
            2, uvSplitterAvailableChildExtent(viewportUvSplitter_));
        const int panelExtent = std::clamp(
            requestedEndpoint, 1, totalExtent - 1);
        const QList<int> logicalEndpoint = panelFirst
            ? QList<int>{ panelExtent, totalExtent - panelExtent }
            : QList<int>{ totalExtent - panelExtent, panelExtent };

        setUvEditorPanelExpanded(uvEditorPanelExpanded_, false);

        // setSizes() can immediately read back a weight list whose total still
        // includes a not-yet-realized handle. Preserve the exact authored
        // endpoint for the one synchronous Preview snapshot instead.
        if (viewportUvSplitter_
            && logicalEndpoint.size() == viewportUvSplitter_->count())
        {
            uvEditorPreviewSettledLogicalSplitterSizes_ = logicalEndpoint;
            uvEditorPreviewSettledLogicalDocumentEpoch_ =
                recoveryDocumentEpoch_;
        }
        return;
    }

    // Complete the full panel-hide endpoint and every invariant normally
    // repaired by hideUvEditorPanelWithTransition()'s finished callback.
    uvEditorPanelHideTransitionActive_ = false;
    if (uvEditorPanelHost_)
    {
        uvEditorPanelHost_->setVisible(false);
    }
    setUvEditorPanelTransitionCollapsible(
        viewportUvSplitter_, uvEditorPanelHost_, false);
    if (uvEditorPanelBody_)
    {
        uvEditorPanelBody_->hide();
        uvEditorPanelBody_->setMaximumHeight(QWIDGETSIZE_MAX);
        uvEditorPanelBody_->setMaximumWidth(QWIDGETSIZE_MAX);
    }
    restoreUvEditorViewportZoomCompensation();
}

void MainWindow::setUvEditorPanelExpanded(
    const bool expanded,
    const bool animate,
    const bool startFromCollapsedPresentation)
{
    const bool expansionChanged = uvEditorPanelExpanded_ != expanded;
    uvEditorPanelExpanded_ = expanded;
    uvEditorPanelHideTransitionActive_ = false;
    if (expansionChanged)
    {
        noteRecoveryStateChanged();
    }

    if (!uvEditorPanelHost_ || !uvEditorPanelBody_ || !viewportUvSplitter_)
    {
        return;
    }

    if (!expanded && uvEditorPanelWidget_)
    {
        static_cast<UvEditorPanel*>(uvEditorPanelWidget_)->cancelActiveModalTransform();
    }

    auto toggleAngle = [this, expanded]() -> qreal
    {
        switch (uvEditorDockPosition_)
        {
            case UvEditorDockPosition::bottom: return expanded ? 0.0 : 180.0;
            case UvEditorDockPosition::top:    return expanded ? 180.0 : 0.0;
            case UvEditorDockPosition::left:   return expanded ? 90.0 : -90.0;
            case UvEditorDockPosition::right:  return expanded ? -90.0 : 90.0;
        }
        return expanded ? 0.0 : 180.0;
    };
    if (uvEditorPanelToggleButton_)
    {
        uvEditorPanelToggleButton_->setToolTip(expanded ? "Collapse UV editor" : "Expand UV editor");
    }
    if (uvEditorPanelDockButton_)
    {
        uvEditorPanelDockButton_->setVisible(expanded);
    }

    if (uvEditorPanelHost_->isHidden())
    {
        if (uvEditorPanelToggleButton_)
        {
            uvEditorPanelToggleButton_->setProperty("uvChevronAngle", toggleAngle());
            uvEditorPanelToggleButton_->setIcon(makeUvPanelChevronIcon(toggleAngle(), QColor(159, 176, 200)));
        }
        return;
    }

    const bool vertical = (uvEditorDockPosition_ == UvEditorDockPosition::bottom || uvEditorDockPosition_ == UvEditorDockPosition::top);
    const bool panelFirst = (uvEditorDockPosition_ == UvEditorDockPosition::top || uvEditorDockPosition_ == UvEditorDockPosition::left);
    const QList<int> sizes = viewportUvSplitter_->sizes();
    const int currentTotal = uvSplitterAvailableChildExtent(
        viewportUvSplitter_);
    const int currentExtent = (sizes.size() >= 2) ? sizes[panelFirst ? 0 : 1] : (expanded ? uvEditorPanelExpandedHeight_ : 36);
    const int totalExtent = currentTotal > 1
        ? currentTotal
        : std::max(1, vertical ? viewportUvSplitter_->height() : viewportUvSplitter_->width());
    const int headerExtent = std::max(28, uvEditorPanelHeader_ ? uvEditorPanelHeader_->sizeHint().height() : 28);
    const int minExpandedExtent = vertical ? 220 : 280;
    const int targetExtent = expanded
        ? std::clamp(uvEditorPanelExpandedHeight_, minExpandedExtent, std::max(minExpandedExtent, totalExtent / 2))
        : headerExtent;

    auto viewportSizeForPanelExtent = [this, vertical, totalExtent](const int panelExtent) -> QSize
    {
        QSize targetViewportSize = glView_ ? glView_->size() : QSize{};
        const int targetViewportExtent = std::max(1, totalExtent - panelExtent);
        if (vertical)
        {
            targetViewportSize.setHeight(targetViewportExtent);
        }
        else
        {
            targetViewportSize.setWidth(targetViewportExtent);
        }
        return targetViewportSize;
    };

    auto applySplitterExtent = [this, panelFirst, totalExtent, viewportSizeForPanelExtent](const double rawExtent)
    {
        const int extent = std::clamp(static_cast<int>(std::lround(rawExtent)), 1, std::max(1, totalExtent - 1));
        // QSplitter's collapsible flag permits an exact zero size, but a
        // positive request below minimumSizeHint() is still clamped to the
        // panel's full layout minimum. Own the animated axis explicitly so a
        // one-pixel first-show extent (and every following sample) is real.
        if (uvEditorDockPosition_ == UvEditorDockPosition::bottom
            || uvEditorDockPosition_ == UvEditorDockPosition::top)
        {
            uvEditorPanelHost_->setMaximumHeight(extent);
        }
        else
        {
            uvEditorPanelHost_->setMaximumWidth(extent);
        }
        uvEditorPanelProgrammaticResize_ = true;
        if (panelFirst)
        {
            viewportUvSplitter_->setSizes({ extent, std::max(1, totalExtent - extent) });
        }
        else
        {
            viewportUvSplitter_->setSizes({ std::max(1, totalExtent - extent), extent });
        }
        uvEditorPanelProgrammaticResize_ = false;
        applyUvEditorViewportZoomCompensation(viewportSizeForPanelExtent(extent));
    };

    if (expanded)
    {
        uvEditorPanelBody_->setMaximumHeight(QWIDGETSIZE_MAX);
        uvEditorPanelBody_->setMaximumWidth(QWIDGETSIZE_MAX);
        uvEditorPanelBody_->show();
    }
    else
    {
        uvEditorPanelBody_->setMaximumHeight(QWIDGETSIZE_MAX);
        uvEditorPanelBody_->setMaximumWidth(QWIDGETSIZE_MAX);
        if (vertical)
        {
            uvEditorPanelBody_->setMaximumHeight(0);
        }
        else
        {
            uvEditorPanelBody_->setMaximumWidth(0);
        }
    }

    // Do not impose the final minimum before the splitter animation owns the
    // panel extent. Doing so makes QSplitter jump to the expanded body (or a
    // collapsed header) for one layout pass before animating.
    const bool ownsPresentationExtent = animate && (expanded || startFromCollapsedPresentation);
    const int transitionMinimum = ownsPresentationExtent ? 0 : (expanded ? minExpandedExtent : headerExtent);
    if (vertical)
    {
        uvEditorPanelHost_->setMinimumHeight(transitionMinimum);
        uvEditorPanelHost_->setMaximumHeight(QWIDGETSIZE_MAX);
        uvEditorPanelHost_->setMinimumWidth(0);
        uvEditorPanelHost_->setMaximumWidth(QWIDGETSIZE_MAX);
    }
    else
    {
        uvEditorPanelHost_->setMinimumWidth(transitionMinimum);
        uvEditorPanelHost_->setMaximumWidth(QWIDGETSIZE_MAX);
        uvEditorPanelHost_->setMinimumHeight(0);
        uvEditorPanelHost_->setMaximumHeight(QWIDGETSIZE_MAX);
    }

    if (uvEditorPanelAnimation_)
    {
        uvEditorPanelAnimation_->stop();
        uvEditorPanelAnimation_->deleteLater();
        uvEditorPanelAnimation_ = nullptr;
    }

    const auto applyFinalHostConstraints = [this, vertical, expanded, minExpandedExtent, headerExtent]()
    {
        const int minimum = expanded ? minExpandedExtent : headerExtent;
        if (vertical)
        {
            uvEditorPanelHost_->setMinimumHeight(minimum);
            uvEditorPanelHost_->setMaximumHeight(QWIDGETSIZE_MAX);
            uvEditorPanelHost_->setMinimumWidth(0);
            uvEditorPanelHost_->setMaximumWidth(QWIDGETSIZE_MAX);
        }
        else
        {
            uvEditorPanelHost_->setMinimumWidth(minimum);
            uvEditorPanelHost_->setMaximumWidth(QWIDGETSIZE_MAX);
            uvEditorPanelHost_->setMinimumHeight(0);
            uvEditorPanelHost_->setMaximumHeight(QWIDGETSIZE_MAX);
        }
        setUvEditorPanelTransitionCollapsible(viewportUvSplitter_, uvEditorPanelHost_, false);
    };

    // QSplitter can defer or clamp setSizes() while an ancestor is hidden.
    // A newly revealed UV host nevertheless has a defined presentation state:
    // it starts at the controlled one-pixel extent established by
    // syncUvEditorPanel(), then expands to its saved size.  Do not derive that
    // start value by reading the deferred splitter geometry back, because it
    // can already report the target extent and incorrectly suppress the only
    // presentation animation.
    const int transitionStartExtent = startFromCollapsedPresentation ? 1 : currentExtent;
    if (startFromCollapsedPresentation)
    {
        setUvEditorPanelTransitionCollapsible(viewportUvSplitter_, uvEditorPanelHost_, true);
        applySplitterExtent(static_cast<double>(transitionStartExtent));
    }

    if (!animate || std::abs(transitionStartExtent - targetExtent) <= 1)
    {
        applySplitterExtent(static_cast<double>(targetExtent));
        applyFinalHostConstraints();
        if (!expanded && uvEditorPanelBody_)
        {
            uvEditorPanelBody_->hide();
        }
        else if (expanded && uvEditorPanelBody_)
        {
            uvEditorPanelBody_->setMaximumHeight(QWIDGETSIZE_MAX);
            uvEditorPanelBody_->setMaximumWidth(QWIDGETSIZE_MAX);
        }
        if (uvEditorPanelToggleButton_)
        {
            const qreal finalAngle = toggleAngle();
            uvEditorPanelToggleButton_->setProperty("uvChevronAngle", finalAngle);
            uvEditorPanelToggleButton_->setIcon(makeUvPanelChevronIcon(finalAngle, QColor(159, 176, 200)));
        }
        uvEditorPanelHost_->updateGeometry();
        if (glView_)
        {
            glView_->update();
        }
        return;
    }

    uvEditorPanelAnimation_ = new QVariantAnimation(this);
    uvEditorPanelAnimation_->setObjectName(
        QStringLiteral("uvEditorPanelAnimation"));
    ++uvEditorPanelTransitionSerial_;
    uvEditorPanelAnimation_->setStartValue(static_cast<double>(transitionStartExtent));
    uvEditorPanelAnimation_->setEndValue(static_cast<double>(targetExtent));
    uvEditorPanelAnimation_->setDuration(kUvEditorPanelTransitionMs);
    uvEditorPanelAnimation_->setEasingCurve(makeUvEditorPanelEasingCurve());
    const qreal startAngle = uvEditorPanelToggleButton_ ? uvEditorPanelToggleButton_->property("uvChevronAngle").toReal() : toggleAngle();
    const qreal endAngle = toggleAngle();
    connect(uvEditorPanelAnimation_, &QVariantAnimation::valueChanged, this, [applySplitterExtent](const QVariant& value)
        {
            applySplitterExtent(value.toDouble());
        });
    connect(uvEditorPanelAnimation_, &QVariantAnimation::valueChanged, this, [this, startAngle, endAngle](const QVariant& value)
        {
            if (!uvEditorPanelToggleButton_ || !uvEditorPanelAnimation_)
            {
                return;
            }

            const qreal startExtent = uvEditorPanelAnimation_->startValue().toReal();
            const qreal endExtent = uvEditorPanelAnimation_->endValue().toReal();
            const qreal denom = endExtent - startExtent;
            const qreal t = (std::abs(denom) <= 1.0e-6)
                ? 1.0
                : std::clamp((value.toReal() - startExtent) / denom, 0.0, 1.0);
            const qreal angle = startAngle + ((endAngle - startAngle) * t);
            uvEditorPanelToggleButton_->setProperty("uvChevronAngle", angle);
            uvEditorPanelToggleButton_->setIcon(makeUvPanelChevronIcon(angle, QColor(159, 176, 200)));
        });
    connect(uvEditorPanelAnimation_, &QVariantAnimation::finished, this, [this, expanded, applyFinalHostConstraints]()
        {
            applyFinalHostConstraints();
            if (!expanded && uvEditorPanelBody_)
            {
                uvEditorPanelBody_->hide();
            }
            else if (expanded && uvEditorPanelBody_)
            {
                uvEditorPanelBody_->setMaximumHeight(QWIDGETSIZE_MAX);
                uvEditorPanelBody_->setMaximumWidth(QWIDGETSIZE_MAX);
            }
            if (uvEditorPanelToggleButton_)
            {
                const qreal finalAngle = [this, expanded]() -> qreal
                    {
                        switch (uvEditorDockPosition_)
                        {
                            case UvEditorDockPosition::bottom: return expanded ? 0.0 : 180.0;
                            case UvEditorDockPosition::top:    return expanded ? 180.0 : 0.0;
                            case UvEditorDockPosition::left:   return expanded ? 90.0 : -90.0;
                            case UvEditorDockPosition::right:  return expanded ? -90.0 : 90.0;
                        }
                        return expanded ? 0.0 : 180.0;
                    }();
                uvEditorPanelToggleButton_->setProperty("uvChevronAngle", finalAngle);
                uvEditorPanelToggleButton_->setIcon(makeUvPanelChevronIcon(finalAngle, QColor(159, 176, 200)));
            }
            uvEditorPanelHost_->updateGeometry();
            if (glView_)
            {
                glView_->update();
            }
            if (uvEditorPanelAnimation_)
            {
                uvEditorPanelAnimation_->deleteLater();
                uvEditorPanelAnimation_ = nullptr;
            }
        });
    uvEditorPanelAnimation_->start();
}

void MainWindow::applyUvEditorDockPosition(const UvEditorDockPosition position, const bool preserveSize)
{
    if (!viewportUvSplitter_ || !uvEditorPanelHost_ || !glView_)
    {
        return;
    }

    const UvEditorDockPosition previousPosition = uvEditorDockPosition_;
    if (preserveSize && uvEditorPanelHost_->isVisible())
    {
        uvEditorPanelExpandedHeight_ = std::max(
            uvEditorPanelExpandedHeight_,
            (previousPosition == UvEditorDockPosition::bottom || previousPosition == UvEditorDockPosition::top)
                ? uvEditorPanelHost_->height()
                : uvEditorPanelHost_->width());
    }

    uvEditorDockPosition_ = position;

    const bool vertical = (position == UvEditorDockPosition::bottom || position == UvEditorDockPosition::top);
    const bool sideDock = !vertical;
    const bool panelFirst = (position == UvEditorDockPosition::top || position == UvEditorDockPosition::left);
    viewportUvSplitter_->setOrientation(vertical ? Qt::Vertical : Qt::Horizontal);
    viewportUvSplitter_->insertWidget(panelFirst ? 0 : 1, uvEditorPanelHost_);
    viewportUvSplitter_->insertWidget(panelFirst ? 1 : 0, glView_);

    if (uvEditorPanelWidget_)
    {
        static_cast<UvEditorPanel*>(uvEditorPanelWidget_)->setHorizontalDock(
            position == UvEditorDockPosition::bottom
            || position == UvEditorDockPosition::top);
    }

    if (uvEditorPanelHeader_)
    {
        if (auto* headerLayout = qobject_cast<QBoxLayout*>(uvEditorPanelHeader_->layout()))
        {
            headerLayout->setDirection(QBoxLayout::LeftToRight);
            headerLayout->setContentsMargins(sideDock ? 3 : 5, sideDock ? 1 : 2, sideDock ? 3 : 4, sideDock ? 1 : 2);
            headerLayout->setSpacing(sideDock ? 2 : 3);
        }
        if (auto* titleLabel = uvEditorPanelHeader_->findChild<QLabel*>("uvEditorPanelTitleLabel"))
        {
            titleLabel->setVisible(!sideDock);
        }
    }

    if (!uvEditorPanelHost_->isHidden())
    {
        setUvEditorPanelExpanded(uvEditorPanelExpanded_);
    }
    if (previousPosition != position)
    {
        noteRecoveryStateChanged();
    }
}

void MainWindow::syncUvEditorViewportFacingOrientation()
{
    if (!uvEditorPanelWidget_ || !glView_)
    {
        return;
    }

    auto* panel = static_cast<UvEditorPanel*>(uvEditorPanelWidget_);
    UvViewOrientation orientation{
        uvEditorViewportFacing_ && panel->viewportFacingMirrorX(),
        uvEditorViewportFacing_ ? panel->viewportFacingRotationRadians() : 0.0
    };
    if (uvEditorViewportFacing_)
    {
        std::optional<SurfaceRef> referenceRef = glView_->selectedSurfaceRef();
        const auto refPresent = [panel](const std::optional<SurfaceRef>& ref)
        {
            return ref && std::any_of(
                panel->islands().begin(),
                panel->islands().end(),
                [&ref](const UvIslandState& island)
                {
                    return island.ref.sector == ref->sector
                        && island.ref.surface == ref->surface;
                });
        };
        if (!refPresent(referenceRef))
        {
            referenceRef = panel->activeSurfaceRef();
        }

        if (referenceRef)
        {
            const std::optional<std::vector<QPointF>> projectedVertices = projectSurfacePolygon(
                document_.level(),
                *referenceRef,
                glView_);
            if (projectedVertices)
            {
                if (const std::optional<UvViewOrientation> resolved =
                        uvViewOrientationForActiveSurface(
                            panel->islands(),
                            referenceRef,
                            *projectedVertices))
                {
                    orientation = *resolved;
                }
            }
        }
    }

    panel->setViewportFacingOrientation(
        uvEditorViewportFacing_,
        orientation.mirrorX,
        orientation.rotationRadians);
}

void MainWindow::syncUvEditorPanel()
{
    if (!uvEditorPanelHost_ || !uvEditorPanelWidget_)
    {
        return;
    }

    auto* panel = static_cast<UvEditorPanel*>(uvEditorPanelWidget_);
    if (!documentEditingAllowed())
    {
        if (panel->hasActiveModalTransform())
        {
            panel->cancelActiveModalTransform();
        }
        if (uvEditorPanelAnimation_)
        {
            uvEditorPanelAnimation_->stop();
            uvEditorPanelAnimation_->deleteLater();
            uvEditorPanelAnimation_ = nullptr;
        }
        uvEditorPanelHideTransitionActive_ = false;
        uvEditorPanelHost_->hide();
        return;
    }
    const auto panelPickMode = [this]()
    {
        switch (surfaceSelectionMode_)
        {
            case SurfaceSelectionMode::face: return UvCanvas::PickMode::face;
            case SurfaceSelectionMode::edge: return UvCanvas::PickMode::edge;
            case SurfaceSelectionMode::vertex: return UvCanvas::PickMode::vertex;
        }
        return UvCanvas::PickMode::face;
    }();
    const bool surfaceMode = (editMode_ == EditMode::surface);
    if (!surfaceMode)
    {
        if (panel->hasActiveModalTransform())
        {
            panel->cancelActiveModalTransform();
        }
        hideUvEditorPanelWithTransition();
        return;
    }
    panel->setPickMode(panelPickMode);
    // QWidget::isVisible() also depends on every ancestor.  The UV panel can
    // be explicitly shown while the MainWindow itself is still hidden during
    // startup/tests, so use the host's own hidden state for transition
    // lifecycle decisions.
    const bool panelWasVisible = !uvEditorPanelHost_->isHidden();
    const bool panelNeedsPresentation = !panelWasVisible || uvEditorPanelHideTransitionActive_;
    if (!panelWasVisible && glView_)
    {
        prepareUvEditorViewportZoomCompensation(glView_->size());
    }
    if (!panelWasVisible)
    {
        const bool vertical = uvEditorDockPosition_ == UvEditorDockPosition::bottom
            || uvEditorDockPosition_ == UvEditorDockPosition::top;
        const bool panelFirst = uvEditorDockPosition_ == UvEditorDockPosition::top
            || uvEditorDockPosition_ == UvEditorDockPosition::left;

        // Reveal the host at a controlled zero-size starting point. Without a
        // temporary maximum, QSplitter briefly allocates the body's size hint
        // before setUvEditorPanelExpanded() starts its animation.
        uvEditorPanelHost_->setMinimumHeight(0);
        uvEditorPanelHost_->setMinimumWidth(0);
        if (vertical)
        {
            uvEditorPanelHost_->setMaximumHeight(0);
        }
        else
        {
            uvEditorPanelHost_->setMaximumWidth(0);
        }
        // A one-pixel request is otherwise clamped to the panel's layout
        // minimum because the workspace splitter is deliberately configured
        // as non-collapsible.  Permit only this child to occupy the controlled
        // transition extent; finalization restores the user-facing invariant.
        setUvEditorPanelTransitionCollapsible(viewportUvSplitter_, uvEditorPanelHost_, true);
        uvEditorPanelHost_->setVisible(true);
        uvEditorPanelHost_->setMaximumHeight(QWIDGETSIZE_MAX);
        uvEditorPanelHost_->setMaximumWidth(QWIDGETSIZE_MAX);

        const int totalExtent = std::max(
            2,
            uvSplitterAvailableChildExtent(viewportUvSplitter_));
        uvEditorPanelProgrammaticResize_ = true;
        if (panelFirst)
        {
            viewportUvSplitter_->setSizes({ 1, totalExtent - 1 });
        }
        else
        {
            viewportUvSplitter_->setSizes({ totalExtent - 1, 1 });
        }
        uvEditorPanelProgrammaticResize_ = false;
    }

    const editor::Selection& selection = document_.selection();
    std::vector<SurfaceRef> selectionRefs{};
    selectionRefs.reserve(selection.surfaces.size());
    for (const SurfaceRef ref : selection.surfaces)
    {
        selectionRefs.push_back(ref);
    }
    std::sort(selectionRefs.begin(), selectionRefs.end(), [](const SurfaceRef& a, const SurfaceRef& b)
        {
            if (a.sector != b.sector)
            {
                return a.sector < b.sector;
            }
            return a.surface < b.surface;
        });
    const std::vector<SurfaceRef> previousRefs = panel->surfaceRefs();
    const bool sameSurfaceSetAsPrevious =
        !previousRefs.empty()
        && previousRefs.size() == selectionRefs.size()
        && std::equal(
            previousRefs.begin(),
            previousRefs.end(),
            selectionRefs.begin(),
            [](const SurfaceRef& a, const SurfaceRef& b)
            {
                return a.sector == b.sector && a.surface == b.surface;
            });
    const bool refreshViewportFacingOrientation =
        !sameSurfaceSetAsPrevious || uvEditorForceDisplayRebuild_;
    const bool selectionReadOnly = selectionTouchesNonEditableLayer(selection);
    const bool hasSurfaceOnlySelection =
        !selection.surfaces.empty()
        && selection.sectors.empty()
        && selection.things.empty()
        && selection.lights.empty()
        && selection.cogs.empty();

    if (selectionReadOnly)
    {
        panel->clearEditorState("Selection contains surfaces on hidden or locked layers.");
        syncUvEditorViewportFacingOrientation();
        if (panelNeedsPresentation)
        {
            setUvEditorPanelExpanded(uvEditorPanelExpanded_, true, !panelWasVisible);
        }
        return;
    }

    if (!hasSurfaceOnlySelection)
    {
        panel->clearEditorState("Select one or more surfaces in Surface mode to edit UVs.");
        syncUvEditorViewportFacingOrientation();
        if (panelNeedsPresentation)
        {
            setUvEditorPanelExpanded(uvEditorPanelExpanded_, true, !panelWasVisible);
        }
        return;
    }

    const auto isStillSelected = [&selectionRefs](const std::optional<SurfaceRef>& ref) -> bool
    {
        if (!ref)
        {
            return false;
        }

        return std::any_of(
            selectionRefs.begin(),
            selectionRefs.end(),
            [&ref](const SurfaceRef& candidate)
            {
                return candidate.sector == ref->sector && candidate.surface == ref->surface;
            });
    };

    const std::optional<SurfaceRef> previousPanelActiveRef = panel->activeSurfaceRef();
    const auto firstStillSelectedPreviousPanelRef = [&]() -> std::optional<SurfaceRef>
    {
        if (!panel->hasEditorState())
        {
            return std::nullopt;
        }

        const std::optional<SurfaceRef> firstSelectedRef = panel->firstSelectedSurfaceRef();
        return isStillSelected(firstSelectedRef) ? firstSelectedRef : std::nullopt;
    };
    const std::optional<SurfaceRef> previousDisplayedAnchorRef =
        isStillSelected(previousPanelActiveRef)
            ? previousPanelActiveRef
            : firstStillSelectedPreviousPanelRef();
    const bool keepDisplayFrameOnSelectionChange =
        panel->hasEditorState()
        && !uvEditorForceDisplayRebuild_
        && !sameSurfaceSetAsPrevious
        && previousDisplayedAnchorRef.has_value();
    const bool fitView =
        !panel->hasEditorState()
        || (!sameSurfaceSetAsPrevious && !keepDisplayFrameOnSelectionChange);

    std::optional<SurfaceRef> activeSurfaceRef{};

    // When the UV selection expands or shrinks, keep the previous UV active island
    // as the root if it is still selected. Re-rooting immediately to the viewport's
    // current active surface can make a previously correct island shift when a
    // neighbor is added to the multiselect.
    if (!sameSurfaceSetAsPrevious && previousDisplayedAnchorRef)
    {
        activeSurfaceRef = previousDisplayedAnchorRef;
    }

    if (!activeSurfaceRef && glView_)
    {
        const std::optional<SurfaceRef> viewportActiveRef = glView_->selectedSurfaceRef();
        if (isStillSelected(viewportActiveRef))
        {
            activeSurfaceRef = viewportActiveRef;
        }
    }

    if (!activeSurfaceRef && previousDisplayedAnchorRef)
    {
        activeSurfaceRef = previousDisplayedAnchorRef;
    }

    if (!activeSurfaceRef)
    {
        activeSurfaceRef = firstStillSelectedPreviousPanelRef();
    }

    if (!activeSurfaceRef && !selectionRefs.empty())
    {
        // Always keep a deterministic active island for UV commands and status
        // refreshes; display orientation is recomputed consistently below.
        activeSurfaceRef = selectionRefs.front();
    }

    if (const std::optional<UvEditorBuildResult> state = buildUvEditorState(
            document_.level(),
            selection,
            resources_,
            glView_,
            activeSurfaceRef,
            nullptr,
            !sameSurfaceSetAsPrevious))
    {
        // Late action/layout refreshes run after native SED/recovery restore.
        // Preserve the settled display basis for matching stored UVs so that
        // a harmless sync cannot replace restored zoom-facing island
        // alignment with the freshly built default representation.
        panel->setEditorState(
            state->islands,
            state->previewTexture,
            fitView,
            activeSurfaceRef,
            true);
        uvEditorForceDisplayRebuild_ = false;
    }
    else
    {
        panel->clearEditorState("No editable UV surfaces were found in the current selection.");
    }

    // View Face is a persisted presentation basis. A routine action/property
    // refresh for the same surfaces must not rederive it from the viewport and
    // shift the restored pan/orientation. Recompute only when the displayed
    // surface set or its UV representation genuinely changed; toggling View
    // Face still calls the resolver directly.
    if (refreshViewportFacingOrientation)
    {
        syncUvEditorViewportFacingOrientation();
    }

    if (panelNeedsPresentation)
    {
        setUvEditorPanelExpanded(uvEditorPanelExpanded_, true, !panelWasVisible);
    }
}

void MainWindow::applyUvEditorMaximumZoomForPreferences()
{
    if (uvEditorPanelWidget_)
    {
        static_cast<UvEditorPanel*>(uvEditorPanelWidget_)
            ->clampCanvasZoomToConfiguredMaximum();
    }
}

void MainWindow::showUvEditorDialog()
{
    if (editMode_ != EditMode::surface)
    {
        setEditMode(EditMode::surface);
    }

    if (!uvEditorPanelHost_ || !uvEditorPanelWidget_)
    {
        return;
    }

    // setEditMode() synchronously refreshes and presents the UV panel through
    // updateEditActions(). Only recover a hidden/interrupted presentation
    // here; showing and syncing it again restarted the same animation.
    if (uvEditorPanelHost_->isHidden() || uvEditorPanelHideTransitionActive_)
    {
        syncUvEditorPanel();
    }
    if (!uvEditorPanelExpanded_)
    {
        setUvEditorPanelExpanded(true);
    }
    static_cast<UvEditorPanel*>(uvEditorPanelWidget_)->focusCanvas();
}

bool MainWindow::dispatchUvEditorShortcut(const int key, const Qt::KeyboardModifiers modifiers)
{
    if (!uvEditorPanelWidget_
        || !uvEditorPanelHost_
        || !uvEditorPanelHost_->isVisible()
        || !uvEditorPanelExpanded_)
    {
        return false;
    }

    if (auto* panel = static_cast<UvEditorPanel*>(uvEditorPanelWidget_))
    {
        return panel->handleShortcutKey(key, modifiers);
    }
    return false;
}

} // namespace sed
