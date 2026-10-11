#include "test_common.hpp"

#include "gl_view.hpp"
#include "main_window/main_window.hpp"
#include "app_settings.hpp"
#include "sed/assets/sed.hpp"
#include "sed/editor/document.hpp"
#include "sed/level/cleave.hpp"
#include "sed/level/geometry.hpp"
#include "ui/layers/layer_color_utils.hpp"
#include "ui/viewport/cleave_plane_equivalence_utils.hpp"

#include <array>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <iterator>
#include <limits>
#include <map>
#include <sstream>
#include <string>
#include <optional>
#include <utility>
#include <vector>

#include <QScopeGuard>
#include <QSettings>
#include <QAction>
#include <QCborArray>
#include <QCborMap>
#include <QCborValue>
#include <QToolButton>

namespace sed::app_tests
{
namespace
{

struct SurfaceMultiCutLine
{
    TVector first{};
    TVector second{};
    TVector normal{};
};

struct SurfaceCleavePlane
{
    TVector normal{};
    TVector point{};
};

constexpr std::array<SurfaceMultiCutLine, 10> kOpenSurfaceArchLines{{
    {
        { -0.2996833463956355, 1.0, 0.003908259996932173 },
        { -0.30000000447034836, 1.0, 0.3999999985098839 },
        { -0.9999996804348867, 0.0, -0.0007994561429350398 },
    },
    {
        { -0.30000000447034836, 1.0, 0.3999999985098839 },
        { -0.2500000037252903, 1.0, 0.5 },
        { -0.8944271909999159, 0.0, 0.4472135954999579 },
    },
    {
        { -0.2500000037252903, 1.0, 0.5 },
        { -0.20000000298023224, 1.0, 0.5500000007450581 },
        { -0.7071067811865475, 0.0, 0.7071067811865475 },
    },
    {
        { -0.20000000298023224, 1.0, 0.5500000007450581 },
        { -0.10000000149011612, 1.0, 0.6000000014901161 },
        { -0.4472135954999579, 0.0, 0.8944271909999159 },
    },
    {
        { -0.10000000149011612, 1.0, 0.6000000014901161 },
        { 0.0, 1.0, 0.6000000014901161 },
        { 0.0, 0.0, 1.0 },
    },
    {
        { 0.0, 1.0, 0.6000000014901161 },
        { 0.05000000074505806, 1.0, 0.6000000014901161 },
        { 0.0, 0.0, 1.0 },
    },
    {
        { 0.05000000074505806, 1.0, 0.6000000014901161 },
        { 0.15000000223517418, 1.0, 0.5500000007450581 },
        { 0.4472135954999579, 0.0, 0.8944271909999159 },
    },
    {
        { 0.15000000223517418, 1.0, 0.5500000007450581 },
        { 0.20000000298023224, 1.0, 0.5 },
        { 0.7071067811865475, 0.0, 0.7071067811865475 },
    },
    {
        { 0.20000000298023224, 1.0, 0.5 },
        { 0.2500000037252903, 1.0, 0.3999999985098839 },
        { 0.8944271909999159, 0.0, 0.4472135954999579 },
    },
    {
        { 0.2500000037252903, 1.0, 0.3999999985098839 },
        { 0.2500000037252903, 1.0, -7.450580596923828e-09 },
        { 1.0, 0.0, 0.0 },
    },
}};

// Fixed geometry-only regression vectors from a shared-endpoint arch, a rotated
// Rectangle and a second Filled arch on a synthetic two-by-one wall. No Level,
// resource, texture or editor-state bytes from the private diagnostic are retained.
constexpr std::array<SurfaceMultiCutLine, 33> kRoundoffSurfaceShapeLines{{
    { { -0.29968334639563554, 1.0, 0.003908259996931951 },
      { -0.3000000044703483, 1.0, 0.3999999985098839 },
      { -0.9999996804348867, -0.0, -0.000799456142934759 } },
    { { -0.3000000044703483, 1.0, 0.3999999985098839 },
      { -0.2500000037252903, 1.0, 0.5 },
      { -0.894427190999916, 0.0, 0.4472135954999575 } },
    { { -0.2500000037252903, 1.0, 0.5 },
      { -0.20000000298023232, 1.0, 0.5500000007450581 },
      { -0.7071067811865481, 0.0, 0.7071067811865469 } },
    { { -0.20000000298023232, 1.0, 0.5500000007450581 },
      { -0.10000000149011615, 1.0, 0.6000000014901161 },
      { -0.44721359549995776, 0.0, 0.894427190999916 } },
    { { -0.10000000149011615, 1.0, 0.6000000014901161 },
      { 3.122502256758253e-17, 1.0, 0.6000000014901163 },
      { -2.2204460161630878e-15, 0.0, 1.0 } },
    { { 3.122502256758253e-17, 1.0, 0.6000000014901163 },
      { 0.05000000074505806, 1.0, 0.6000000014901162 },
      { 2.22044601616309e-15, 0.0, 1.0 } },
    { { 0.05000000074505806, 1.0, 0.6000000014901162 },
      { 0.15000000223517423, 1.0, 0.5500000007450581 },
      { 0.4472135954999585, 0.0, 0.8944271909999155 } },
    { { 0.15000000223517423, 1.0, 0.5500000007450581 },
      { 0.20000000298023227, 1.0, 0.5 },
      { 0.7071067811865478, 0.0, 0.7071067811865474 } },
    { { 0.20000000298023227, 1.0, 0.5 },
      { 0.25000000372529024, 1.0, 0.39999999850988394 },
      { 0.894427190999916, 0.0, 0.4472135954999575 } },
    { { 0.25000000372529024, 1.0, 0.39999999850988394 },
      { 0.25000000372529035, 1.0, -7.45058065243498e-09 },
      { 1.0, 0.0, 2.7755575202038607e-16 } },
    { { -0.5695157033734266, 1.0, 0.46644321936813643 },
      { -0.3290981415620227, 1.0, 0.6681775068126262 },
      { -0.6427876096865391, 0.0, 0.7660444431189782 } },
    { { -0.3290981415620227, 1.0, 0.6681775068126262 },
      { -0.5308324290065126, 1.0, 0.9085950686240302 },
      { -0.7660444431189782, -0.0, -0.6427876096865393 } },
    { { -0.5308324290065126, 1.0, 0.9085950686240302 },
      { -0.7712499908179165, 1.0, 0.7068607811795403 },
      { 0.6427876096865393, 0.0, -0.766044443118978 } },
    { { -0.7712499908179165, 1.0, 0.7068607811795403 },
      { -0.5695157033734266, 1.0, 0.46644321936813643 },
      { 0.7660444431189782, 0.0, 0.6427876096865393 } },
    { { 0.5082622150721174, 1.0, 0.5235429498830235 },
      { 0.8220622150721175, 1.0, 0.5235429498830235 },
      { -0.0, 0.0, 1.0 } },
    { { 0.8220622150721175, 1.0, 0.5235429498830235 },
      { 0.8220622150721175, 1.0, 0.6804429498830236 },
      { -1.0, 0.0, 0.0 } },
    { { 0.8220622150721175, 1.0, 0.6804429498830236 },
      { 0.8190474255673844, 1.0, 0.711052621407354 },
      { -0.9951847266721969, -0.0, -0.09801714032955959 } },
    { { 0.8190474255673844, 1.0, 0.711052621407354 },
      { 0.8101189137231384, 1.0, 0.7404859804211061 },
      { -0.9569403357322083, -0.0, -0.290284677254464 } },
    { { 0.8101189137231384, 1.0, 0.7404859804211061 },
      { 0.7956197972423869, 1.0, 0.7676119194437991 },
      { -0.8819212643483553, -0.0, -0.4713967368259973 } },
    { { 0.7956197972423869, 1.0, 0.7676119194437991 },
      { 0.7761072690402868, 1.0, 0.7913880038511929 },
      { -0.7730104533627377, -0.0, -0.6343932841636446 } },
    { { 0.7761072690402868, 1.0, 0.7913880038511929 },
      { 0.7523311846328931, 1.0, 0.8109005320532929 },
      { -0.6343932841636446, -0.0, -0.7730104533627377 } },
    { { 0.7523311846328931, 1.0, 0.8109005320532929 },
      { 0.7252052456102, 1.0, 0.8253996485340445 },
      { -0.4713967368259973, -0.0, -0.8819212643483553 } },
    { { 0.7252052456102, 1.0, 0.8253996485340445 },
      { 0.6957718865964481, 1.0, 0.8343281603782905 },
      { -0.29028467725446505, -0.0, -0.956940335732208 } },
    { { 0.6957718865964481, 1.0, 0.8343281603782905 },
      { 0.6651622150721175, 1.0, 0.8373429498830236 },
      { -0.09801714032955927, -0.0, -0.9951847266721972 } },
    { { 0.6651622150721175, 1.0, 0.8373429498830236 },
      { 0.634552543547787, 1.0, 0.8343281603782905 },
      { 0.09801714032955959, 0.0, -0.9951847266721969 } },
    { { 0.634552543547787, 1.0, 0.8343281603782905 },
      { 0.6051191845340349, 1.0, 0.8253996485340445 },
      { 0.290284677254464, 0.0, -0.9569403357322083 } },
    { { 0.6051191845340349, 1.0, 0.8253996485340445 },
      { 0.5779932455113419, 1.0, 0.8109005320532929 },
      { 0.4713967368259973, 0.0, -0.8819212643483553 } },
    { { 0.5779932455113419, 1.0, 0.8109005320532929 },
      { 0.5542171611039481, 1.0, 0.7913880038511929 },
      { 0.6343932841636446, 0.0, -0.7730104533627377 } },
    { { 0.5542171611039481, 1.0, 0.7913880038511929 },
      { 0.5347046329018481, 1.0, 0.7676119194437991 },
      { 0.7730104533627377, 0.0, -0.6343932841636446 } },
    { { 0.5347046329018481, 1.0, 0.7676119194437991 },
      { 0.5202055164210966, 1.0, 0.7404859804211061 },
      { 0.8819212643483553, 0.0, -0.4713967368259973 } },
    { { 0.5202055164210966, 1.0, 0.7404859804211061 },
      { 0.5112770045768505, 1.0, 0.7110526214073541 },
      { 0.956940335732208, 0.0, -0.29028467725446505 } },
    { { 0.5112770045768505, 1.0, 0.7110526214073541 },
      { 0.5082622150721174, 1.0, 0.6804429498830236 },
      { 0.9951847266721972, 0.0, -0.09801714032955927 } },
    { { 0.5082622150721174, 1.0, 0.6804429498830236 },
      { 0.5082622150721174, 1.0, 0.5235429498830235 },
      { 1.0, 0.0, 0.0 } },
}};

editor::DocumentState makeSurfaceArchState()
{
    editor::DocumentState state{};
    state.level.kind = ProjectType::IJIM;
    state.level.uvCoordinateSpace = UvCoordinateSpace::Normalized;
    state.level.layers = { "Layer0" };
    state.level.lvisstring = ui::layers::buildLayerVisibilityString(
        state.level.layers.size(), std::vector<bool>(state.level.layers.size(), true));

    Sector sector{};
    sector.layer = 0;
    const std::array<TVector, 8> points{{
        { -1.0, -1.0, 0.0 },
        { 1.0, -1.0, 0.0 },
        { 1.0, 1.0, 0.0 },
        { -1.0, 1.0, 0.0 },
        { -1.0, -1.0, 1.0 },
        { 1.0, -1.0, 1.0 },
        { 1.0, 1.0, 1.0 },
        { -1.0, 1.0, 1.0 },
    }};
    for (const TVector& point : points)
    {
        sector.newVertex(point.x, point.y, point.z);
    }

    const std::array<std::array<std::int32_t, 4>, 6> faces{{
        { 1, 5, 6, 2 },
        { 0, 3, 7, 4 },
        { 0, 4, 5, 1 },
        { 4, 7, 6, 5 },
        { 0, 1, 2, 3 },
        { 3, 2, 6, 7 },
    }};
    for (const auto& indices : faces)
    {
        Surface surface{};
        surface.vertices.assign(indices.begin(), indices.end());
        surface.texVertices.resize(surface.vertices.size());
        surface.normal = level::surfaceCalcNormalNewell(sector, surface);
        sector.surfaces.push_back(std::move(surface));
    }

    Surface& authoredFace = sector.surfaces[5];
    authoredFace.texVertices = {
        TexVertex{ 0.0f, 0.0f, ColorF{ 0.0f, 0.0f, 0.0f, 1.0f } },
        TexVertex{ 20.0f, 0.0f, ColorF{ 0.0f, 0.0f, 0.0f, 1.0f } },
        TexVertex{ 20.0f, -10.0f, ColorF{ 0.0f, 0.0f, 0.0f, 1.0f } },
        TexVertex{ 0.0f, -10.0f, ColorF{ 0.0f, 0.0f, 0.0f, 1.0f } },
    };
    state.level.sectors.push_back(std::move(sector));
    return state;
}

editor::DocumentState makeCrossedSurfaceBatchFailureState()
{
    editor::DocumentState state = makeSurfaceArchState();
    state.level.sectors.push_back(state.level.sectors.front());
    state.level.sectors[1].id = 1;
    state.level.sectors[1].num = 1;
    state.level.sectors[1].surfaces[5].texVertices.pop_back();
    return state;
}

bool normalizeCleavePlaneNormal(TVector& normal)
{
    const double magnitude = std::max({
        std::abs(normal.x), std::abs(normal.y), std::abs(normal.z) });
    if (!std::isfinite(magnitude) || !(magnitude > 0.0))
    {
        return false;
    }
    const TVector scaled{
        normal.x / magnitude,
        normal.y / magnitude,
        normal.z / magnitude,
    };
    const double length = std::sqrt(
        scaled.x * scaled.x + scaled.y * scaled.y + scaled.z * scaled.z);
    if (!std::isfinite(length) || !(length > 0.0))
    {
        return false;
    }
    normal = scaled * (1.0 / length);
    return std::isfinite(normal.x) && std::isfinite(normal.y) && std::isfinite(normal.z);
}

bool applySurfacePlanesDirectly(
    Level& level,
    const SurfaceRef source,
    const std::vector<SurfaceCleavePlane>& planes,
    std::vector<SurfaceRef>& appendedDescendants)
{
    std::vector<SurfaceRef> descendants{ source };
    for (const SurfaceCleavePlane& plane : planes)
    {
        TVector normal = plane.normal;
        // GlView normalizes the staged line, then MainWindow normalizes the
        // submitted plane once more before passing it into the core.
        if (!normalizeCleavePlaneNormal(normal)
            || !normalizeCleavePlaneNormal(normal))
        {
            return false;
        }

        const std::vector<SurfaceRef> beforePlane = descendants;
        std::vector<SurfaceRef> newDescendants{};
        bool splitAnySurface = false;
        for (const SurfaceRef ref : beforePlane)
        {
            if (ref.sector < 0 || ref.surface < 0
                || static_cast<std::size_t>(ref.sector) >= level.sectors.size())
            {
                return false;
            }
            Sector& sector = level.sectors[static_cast<std::size_t>(ref.sector)];
            if (static_cast<std::size_t>(ref.surface) >= sector.surfaces.size())
            {
                return false;
            }
            const Surface& surface = sector.surfaces[static_cast<std::size_t>(ref.surface)];
            bool hasNegative = false;
            bool hasPositive = false;
            for (const std::int32_t vertexIndex : surface.vertices)
            {
                if (vertexIndex < 0 || static_cast<std::size_t>(vertexIndex) >= sector.vertices.size())
                {
                    return false;
                }
                const Vertex& vertex = sector.vertices[static_cast<std::size_t>(vertexIndex)];
                const double distance = normal.x * (vertex.x - plane.point.x)
                    + normal.y * (vertex.y - plane.point.y)
                    + normal.z * (vertex.z - plane.point.z);
                if (std::abs(distance) <= sed::math::kGeometryEpsilon)
                {
                    continue;
                }
                hasNegative = hasNegative || distance < 0.0;
                hasPositive = hasPositive || distance > 0.0;
            }
            if (!hasNegative || !hasPositive)
            {
                continue;
            }

            const std::size_t beforeCount = sector.surfaces.size();
            if (!level::levelCleaveSurface(
                    level, ref, normal,
                    plane.point.x, plane.point.y, plane.point.z)
                || level.sectors[static_cast<std::size_t>(ref.sector)].surfaces.size()
                    != beforeCount + 1)
            {
                return false;
            }
            splitAnySurface = true;
            newDescendants.push_back(SurfaceRef{
                ref.sector,
                static_cast<std::int32_t>(beforeCount),
            });
        }
        if (!splitAnySurface)
        {
            return false;
        }
        descendants.insert(descendants.end(), newDescendants.begin(), newDescendants.end());
    }

    appendedDescendants.assign(descendants.begin() + 1, descendants.end());
    return true;
}

bool authoredSurfaceCornersRemainExact(
    const Level& result,
    const Surface& authored,
    const std::vector<SurfaceRef>& descendants)
{
    std::vector<bool> found(authored.vertices.size(), false);
    for (const SurfaceRef ref : descendants)
    {
        if (ref.sector < 0 || ref.surface < 0
            || static_cast<std::size_t>(ref.sector) >= result.sectors.size())
        {
            return false;
        }
        const Sector& sector = result.sectors[static_cast<std::size_t>(ref.sector)];
        if (static_cast<std::size_t>(ref.surface) >= sector.surfaces.size())
        {
            return false;
        }
        const Surface& surface = sector.surfaces[static_cast<std::size_t>(ref.surface)];
        if (surface.vertices.size() != surface.texVertices.size())
        {
            return false;
        }
        for (std::size_t corner = 0; corner < surface.vertices.size(); ++corner)
        {
            const auto authoredCorner = std::find(
                authored.vertices.begin(), authored.vertices.end(), surface.vertices[corner]);
            if (authoredCorner == authored.vertices.end())
            {
                continue;
            }
            const std::size_t authoredIndex = static_cast<std::size_t>(
                std::distance(authored.vertices.begin(), authoredCorner));
            const TexVertex& expected = authored.texVertices[authoredIndex];
            const TexVertex& actual = surface.texVertices[corner];
            if (actual.u != expected.u || actual.v != expected.v
                || actual.color.r != expected.color.r
                || actual.color.g != expected.color.g
                || actual.color.b != expected.color.b
                || actual.color.a != expected.color.a)
            {
                return false;
            }
            found[authoredIndex] = true;
        }
    }
    return std::all_of(found.begin(), found.end(), [](const bool value) { return value; });
}

bool surfaceSelectionContainsExactly(
    const editor::DocumentState& state,
    const std::vector<SurfaceRef>& expected)
{
    return state.selection.surfaces.size() == expected.size()
        && std::all_of(expected.begin(), expected.end(), [&state](const SurfaceRef ref)
            {
                return state.selection.surfaces.contains(ref);
            });
}

bool stageSurfaceLines(
    MainWindow& window,
    const std::vector<SurfaceMultiCutLine>& lines,
    const bool linkAdjacentLines,
    const bool bounded = false)
{
    GlView* const view = window.glViewForTest();
    QAction* const cleaveAction = window.findChild<QAction*>(
        QStringLiteral("viewport.surface.cleave"));
    QToolButton* const multiCutButton = window.findChild<QToolButton*>(
        QStringLiteral("cleaveMultiCutButton"));
    QToolButton* const chainButton = window.findChild<QToolButton*>(
        QStringLiteral("cleaveChainButton"));
    QToolButton* const boundedButton = window.findChild<QToolButton*>(
        QStringLiteral("cleaveBoundedButton"));
    if (!view || !cleaveAction || !multiCutButton || !chainButton || !boundedButton)
    {
        return false;
    }

    window.updateEditActionsForTest();
    if (!cleaveAction->isChecked())
    {
        cleaveAction->setChecked(true);
    }
    multiCutButton->setChecked(true);
    chainButton->setChecked(linkAdjacentLines);
    boundedButton->setChecked(bounded);
    const bool controlsOwnTheTool = cleaveAction->isEnabled()
        && cleaveAction->isChecked()
        && multiCutButton->isChecked()
        && view->surfaceCleaveToolEnabled()
        && view->cleaveStagedMode()
        && view->cleaveChainEnabled() == linkAdjacentLines
        && view->cleaveBounded() == bounded;
    if (!controlsOwnTheTool)
    {
        return false;
    }
    view->captureStagedCleaveContextForTest(true);
    for (std::size_t index = 0; index < lines.size(); ++index)
    {
        const SurfaceMultiCutLine& line = lines[index];
        view->addStagedCleaveDocumentLineForTest(
            line.first,
            line.second,
            line.normal,
            linkAdjacentLines && index > 0,
            linkAdjacentLines && index + 1 < lines.size());
    }
    const bool staged = view->stagedCleaveLineCount() == lines.size()
        && (!linkAdjacentLines || view->stagedCleaveLinesFormSingleOpenChainForTest());
    return staged;
}

bool stageSurfaceLinesAndApply(
    MainWindow& window,
    const std::vector<SurfaceMultiCutLine>& lines,
    const bool linkAdjacentLines,
    const bool bounded = false)
{
    const bool staged = stageSurfaceLines(window, lines, linkAdjacentLines, bounded);
    if ( staged )
    {
        window.glViewForTest()->requestApplyStagedCleave();
    }
    return staged;
}

editor::DocumentState makeRoundedSurfaceState()
{
    editor::DocumentState state{};
    state.level.kind = ProjectType::IJIM;
    state.level.layers = { "Layer0" };

    Sector sector{};
    sector.layer = 0;
    const std::array<TVector, 8> points{{
        { -1.0, -1.0, -1.0 },
        { 1.0, -1.0, -1.0 },
        { 1.0, 1.0, -1.0 },
        { -1.0, 1.0, -1.0 },
        { -1.0, -1.0, 1.0 },
        { 1.0, -1.0, 1.0 },
        { 1.0, 1.0, 1.00002 },
        { -1.0, 1.0, 1.0 },
    }};
    for ( const TVector& point : points )
    {
        sector.newVertex(point.x, point.y, point.z);
    }

    const auto appendSurface = [&sector](const std::initializer_list<std::int32_t> indices)
    {
        Surface surface{};
        surface.vertices.assign(indices.begin(), indices.end());
        surface.texVertices.resize(surface.vertices.size());
        surface.normal = level::surfaceCalcNormalNewell(sector, surface);
        sector.surfaces.push_back(std::move(surface));
    };
    appendSurface({ 1, 5, 6, 2 });
    appendSurface({ 0, 3, 7, 4 });
    appendSurface({ 3, 2, 6, 7 });
    appendSurface({ 0, 4, 5, 1 });
    appendSurface({ 4, 7, 6, 5 });
    appendSurface({ 0, 1, 2, 3 });
    state.level.sectors.push_back(std::move(sector));
    return state;
}

editor::DocumentState makeRoundedPortalState()
{
    editor::DocumentState state = makeRoundedSurfaceState();
    state.level.layers = { "Layer0" };
    state.level.sectors.push_back(state.level.sectors.front());

    Sector& source = state.level.sectors[0];
    Sector& mirror = state.level.sectors[1];
    mirror.num = 1;
    for (Vertex& vertex : mirror.vertices)
    {
        vertex.z += 2.0;
    }
    // The copied source carries the rounded top corner on vertex 6. The
    // translated portal uses vertex 2 on its lower face, matching the core
    // rounded reciprocal-portal fixture without embedding an asset.
    mirror.vertices[6].z = 3.0;
    mirror.vertices[2].z = 1.00002;
    for (Surface& surface : source.surfaces)
    {
        surface.normal = level::surfaceCalcNormalNewell(source, surface);
    }
    for (Surface& surface : mirror.surfaces)
    {
        surface.normal = level::surfaceCalcNormalNewell(mirror, surface);
    }

    constexpr double warp = 0.00002;
    const double normalLength = std::sqrt(1.0 + warp * warp / 8.0);
    source.surfaces[4].normal = {
        warp / (4.0 * normalLength),
        warp / (4.0 * normalLength),
        -1.0 / normalLength };
    mirror.surfaces[5].normal = -source.surfaces[4].normal;
    std::rotate(
        mirror.surfaces[5].vertices.begin(),
        mirror.surfaces[5].vertices.begin() + 1,
        mirror.surfaces[5].vertices.end());
    std::rotate(
        mirror.surfaces[5].texVertices.begin(),
        mirror.surfaces[5].texVertices.begin() + 1,
        mirror.surfaces[5].texVertices.end());
    source.surfaces[4].adjoin = SurfaceRef{ 1, 5 };
    mirror.surfaces[5].adjoin = SurfaceRef{ 0, 4 };
    return state;
}

bool allAdjoinsAreReciprocal(const Level& level)
{
    for (std::size_t sectorIndex = 0; sectorIndex < level.sectors.size(); ++sectorIndex)
    {
        const Sector& sector = level.sectors[sectorIndex];
        for (std::size_t surfaceIndex = 0; surfaceIndex < sector.surfaces.size(); ++surfaceIndex)
        {
            const Surface& surface = sector.surfaces[surfaceIndex];
            if (!surface.adjoin)
            {
                continue;
            }
            const SurfaceRef partner = *surface.adjoin;
            if (partner.sector < 0 || partner.surface < 0
                || static_cast<std::size_t>(partner.sector) >= level.sectors.size())
            {
                return false;
            }
            const Sector& partnerSector = level.sectors[static_cast<std::size_t>(partner.sector)];
            if (static_cast<std::size_t>(partner.surface) >= partnerSector.surfaces.size())
            {
                return false;
            }
            const std::optional<SurfaceRef>& backlink =
                partnerSector.surfaces[static_cast<std::size_t>(partner.surface)].adjoin;
            if (!backlink || backlink->sector != static_cast<std::int32_t>(sectorIndex)
                || backlink->surface != static_cast<std::int32_t>(surfaceIndex))
            {
                return false;
            }
        }
    }
    return true;
}

bool allAdjoinBoundaryCoordinatesMatch(const Level& level)
{
    const auto sameCoordinate = [](const double first, const double second)
    {
        const double scale = std::max({ 1.0, std::abs(first), std::abs(second) });
        return std::isfinite(first) && std::isfinite(second)
            && std::abs(first - second) <= 64.0 * std::numeric_limits<double>::epsilon() * scale;
    };
    if (!allAdjoinsAreReciprocal(level))
    {
        return false;
    }
    for (const Sector& sector : level.sectors)
    {
        for (const Surface& surface : sector.surfaces)
        {
            if (!surface.adjoin)
            {
                continue;
            }
            const Sector& partnerSector = level.sectors[static_cast<std::size_t>(surface.adjoin->sector)];
            const Surface& partner = partnerSector.surfaces[static_cast<std::size_t>(surface.adjoin->surface)];
            if (surface.vertices.size() != partner.vertices.size())
            {
                return false;
            }
            for (const std::int32_t vertexIndex : surface.vertices)
            {
                if (vertexIndex < 0 || static_cast<std::size_t>(vertexIndex) >= sector.vertices.size())
                {
                    return false;
                }
                const Vertex& vertex = sector.vertices[static_cast<std::size_t>(vertexIndex)];
                bool found = false;
                for (const std::int32_t partnerIndex : partner.vertices)
                {
                    if (partnerIndex < 0 || static_cast<std::size_t>(partnerIndex) >= partnerSector.vertices.size())
                    {
                        return false;
                    }
                    const Vertex& partnerVertex = partnerSector.vertices[static_cast<std::size_t>(partnerIndex)];
                    found = found
                        || (sameCoordinate(vertex.x, partnerVertex.x)
                            && sameCoordinate(vertex.y, partnerVertex.y)
                            && sameCoordinate(vertex.z, partnerVertex.z));
                }
                if (!found)
                {
                    return false;
                }
            }
        }
    }
    return true;
}

void selectSurface(MainWindow& window, const SurfaceRef ref)
{
    window.activateSurfaceEditModeForTest();
    window.mutableDocumentStateForTest().selection.surfaces = { ref };
    window.updateEditActionsForTest();
    if ( GlView* const view = window.glViewForTest() )
    {
        view->setSelectionState(ref.sector, ref.surface, -1, -1,
            window.documentStateForTest().selection);
    }
}

std::string firstSurfaceLevelDifference(const Level& expected, const Level& actual)
{
    if (expected.kind != actual.kind) return "kind";
    if (expected.uvCoordinateSpace != actual.uvCoordinateSpace) return "uvCoordinateSpace";
    if (expected.exportBufferExtras != actual.exportBufferExtras) return "exportBufferExtras";
    if (expected.header.version != actual.header.version) return "header.version";
    if (expected.header.gravity != actual.header.gravity) return "header.gravity";
    if (expected.header.perspectiveDistance != actual.header.perspectiveDistance) return "header.perspectiveDistance";
    if (expected.header.gouraudDistance != actual.header.gouraudDistance) return "header.gouraudDistance";
    if (expected.materials != actual.materials) return "materials";
    if (expected.layers != actual.layers) return "layers";
    if (expected.templates.size() != actual.templates.size()) return "templates.size";
    if (expected.things.size() != actual.things.size()) return "things.size";
    if (expected.cogs.size() != actual.cogs.size()) return "cogs.size";
    if (expected.lights.size() != actual.lights.size()) return "lights.size";
    if (expected.colormaps != actual.colormaps) return "colormaps";
    if (expected.masterCMP != actual.masterCMP) return "masterCMP";
    if (expected.ppunit != actual.ppunit) return "ppunit";
    if (expected.lvisstring != actual.lvisstring) return "lvisstring";
    if (expected.curId != actual.curId) return "curId";
    if (expected.sectors.size() != actual.sectors.size()) return "sectors.size";

    for (std::size_t sectorIndex = 0; sectorIndex < expected.sectors.size(); ++sectorIndex)
    {
        const Sector& lhs = expected.sectors[sectorIndex];
        const Sector& rhs = actual.sectors[sectorIndex];
        const std::string prefix = "sectors[" + std::to_string(sectorIndex) + "].";
        if (lhs.id != rhs.id) return prefix + "id";
        if (lhs.num != rhs.num) return prefix + "num";
        if (lhs.flags != rhs.flags) return prefix + "flags";
        if (lhs.layer != rhs.layer) return prefix + "layer";
        if (lhs.mark != rhs.mark) return prefix + "mark";
        if (lhs.colormap != rhs.colormap) return prefix + "colormap";
        if (lhs.sound != rhs.sound) return prefix + "sound";
        if (lhs.soundVolume != rhs.soundVolume) return prefix + "soundVolume";
        if (lhs.vertices.size() != rhs.vertices.size()) return prefix + "vertices.size";
        for (std::size_t vertexIndex = 0; vertexIndex < lhs.vertices.size(); ++vertexIndex)
        {
            const Vertex& first = lhs.vertices[vertexIndex];
            const Vertex& second = rhs.vertices[vertexIndex];
            if (first.x != second.x || first.y != second.y || first.z != second.z || first.mark != second.mark)
            {
                std::ostringstream field;
                field << prefix << "vertices[" << vertexIndex << "] expected ("
                      << first.x << ',' << first.y << ',' << first.z << ", mark " << first.mark
                      << ") actual (" << second.x << ',' << second.y << ',' << second.z
                      << ", mark " << second.mark << ')';
                return field.str();
            }
        }
        if (lhs.surfaces.size() != rhs.surfaces.size()) return prefix + "surfaces.size";
        for (std::size_t surfaceIndex = 0; surfaceIndex < lhs.surfaces.size(); ++surfaceIndex)
        {
            const Surface& first = lhs.surfaces[surfaceIndex];
            const Surface& second = rhs.surfaces[surfaceIndex];
            const std::string surfacePrefix = prefix + "surfaces[" + std::to_string(surfaceIndex) + "].";
            if (first.vertices != second.vertices) return surfacePrefix + "vertices";
            if (first.texVertices.size() != second.texVertices.size()) return surfacePrefix + "texVertices.size";
            if (first.normal.x != second.normal.x || first.normal.y != second.normal.y || first.normal.z != second.normal.z)
            {
                std::ostringstream field;
                field << surfacePrefix << "normal expected (" << first.normal.x << ','
                      << first.normal.y << ',' << first.normal.z << ") actual ("
                      << second.normal.x << ',' << second.normal.y << ',' << second.normal.z << ')';
                return field.str();
            }
            if (first.num != second.num) return surfacePrefix + "num";
            if (first.mark != second.mark) return surfacePrefix + "mark";
            if (first.d3dId != second.d3dId) return surfacePrefix + "d3dId";
            if (first.surfaceFlags != second.surfaceFlags) return surfacePrefix + "surfaceFlags";
            if (first.faceFlags != second.faceFlags) return surfacePrefix + "faceFlags";
            if (first.geo != second.geo) return surfacePrefix + "geo";
            if (first.light != second.light) return surfacePrefix + "light";
            if (first.tex != second.tex) return surfacePrefix + "tex";
            if (first.material != second.material) return surfacePrefix + "material";
            if (first.adjoin != second.adjoin) return surfacePrefix + "adjoin";
            if (first.uScale != second.uScale) return surfacePrefix + "uScale";
            if (first.vScale != second.vScale) return surfacePrefix + "vScale";
            for (std::size_t corner = 0; corner < first.texVertices.size(); ++corner)
            {
                const TexVertex& lhsTex = first.texVertices[corner];
                const TexVertex& rhsTex = second.texVertices[corner];
                if (lhsTex.u != rhsTex.u || lhsTex.v != rhsTex.v
                    || lhsTex.color.r != rhsTex.color.r || lhsTex.color.g != rhsTex.color.g
                    || lhsTex.color.b != rhsTex.color.b || lhsTex.color.a != rhsTex.color.a)
                {
                    std::ostringstream field;
                    field << surfacePrefix << "texVertices[" << corner << ']';
                    return field.str();
                }
            }
        }
    }
    return "unexamined Level field";
}

void reportSurfaceLevelDifference(
    const char* label,
    const Level& expected,
    const Level& actual)
{
    std::cerr << "DIFF: " << label << ": " << firstSurfaceLevelDifference(expected, actual) << '\n';
    const auto expectedBytes = assets::saveToSedBytes(expected);
    const auto actualBytes = assets::saveToSedBytes(actual);
    const std::size_t commonSize = std::min(expectedBytes.size(), actualBytes.size());
    const auto mismatch = std::mismatch(expectedBytes.begin(), expectedBytes.begin() + commonSize, actualBytes.begin());
    const std::size_t offset = static_cast<std::size_t>(std::distance(expectedBytes.begin(), mismatch.first));
    std::cerr << "DIFF: " << label << " SED bytes expected=" << expectedBytes.size()
              << " actual=" << actualBytes.size() << " first_offset=" << offset;
    if (offset < commonSize)
    {
        std::cerr << " expected_byte=" << std::to_integer<unsigned int>(expectedBytes[offset])
                  << " actual_byte=" << std::to_integer<unsigned int>(actualBytes[offset]);
    }
    std::cerr << '\n';
}

} // namespace

void runSurfaceMultiCutRegressionTests()
{
    QSettings boundedSettings = settingsStore();
    const bool hadBoundedSetting = boundedSettings.contains(QStringLiteral("cleave/bounded"));
    const QVariant savedBoundedSetting = boundedSettings.value(QStringLiteral("cleave/bounded"));
    boundedSettings.remove(QStringLiteral("cleave/bounded"));
    boundedSettings.sync();
    const auto restoreBoundedSetting = qScopeGuard(
        [hadBoundedSetting, savedBoundedSetting]()
        {
            QSettings settings = settingsStore();
            if (hadBoundedSetting)
            {
                settings.setValue(QStringLiteral("cleave/bounded"), savedBoundedSetting);
            }
            else
            {
                settings.remove(QStringLiteral("cleave/bounded"));
            }
            settings.sync();
        });

    MainWindow boundedDefaultWindow{};
    boundedDefaultWindow.setAttribute(Qt::WA_DontShowOnScreen);
    QToolButton* const boundedDefaultButton = boundedDefaultWindow.findChild<QToolButton*>(
        QStringLiteral("cleaveBoundedButton"));
    check(boundedDefaultButton && boundedDefaultButton->isCheckable()
            && boundedDefaultButton->isChecked()
            && !boundedDefaultButton->toolTip().trimmed().isEmpty(),
        "Multi-cut exposes a checkable, tooltipped Bounded control that defaults on for fresh settings");

    constexpr SurfaceRef firstCrossingSurface{ 0, 5 };
    constexpr SurfaceRef malformedCrossingSurface{ 1, 5 };
    const editor::DocumentState crossedFaceBaseline = makeCrossedSurfaceBatchFailureState();
    Level crossedFaceCoreProbe = crossedFaceBaseline.level;
    const bool validCrossingAccepted = level::levelCleaveSurface(
        crossedFaceCoreProbe,
        firstCrossingSurface,
        TVector{ 1.0, 0.0, 0.0 },
        0.0,
        0.0,
        0.0);
    const bool malformedCrossingRejected = !level::levelCleaveSurface(
        crossedFaceCoreProbe,
        malformedCrossingSurface,
        TVector{ 1.0, 0.0, 0.0 },
        0.0,
        0.0,
        0.0);
    check(validCrossingAccepted && malformedCrossingRejected,
        "surface Multi-cut rejection fixture: core accepts the first crossed face and rejects the malformed crossed face");

    MainWindow crossedFaceWindow{};
    crossedFaceWindow.setAttribute(Qt::WA_DontShowOnScreen);
    crossedFaceWindow.setDocumentStateForTest(crossedFaceBaseline);
    crossedFaceWindow.activateSurfaceEditModeForTest();
    crossedFaceWindow.mutableDocumentStateForTest().selection.surfaces = {
        firstCrossingSurface,
        malformedCrossingSurface,
    };
    crossedFaceWindow.updateEditActionsForTest();
    if (GlView* const view = crossedFaceWindow.glViewForTest())
    {
        view->setSelectionState(
            firstCrossingSurface.sector,
            firstCrossingSurface.surface,
            -1,
            -1,
            crossedFaceWindow.documentStateForTest().selection);
    }
    const auto crossedFaceBeforeBytes =
        assets::saveToSedBytes(crossedFaceWindow.documentStateForTest().level);
    const auto crossedFaceBeforeSelection =
        crossedFaceWindow.documentStateForTest().selection.surfaces;
    const std::size_t crossedFaceUndoBefore = crossedFaceWindow.undoDepthForTest();
    const std::size_t crossedFaceRedoBefore = crossedFaceWindow.redoDepthForTest();
    if (GlView* const view = crossedFaceWindow.glViewForTest())
    {
        view->surfacesCleaveBatchRequested(
            QList<TVector>{ TVector{ 1.0, 0.0, 0.0 }, TVector{ 1.0, 0.0, 0.0 } },
            QList<TVector>{ TVector{}, TVector{} },
            false,
            TVector{});
    }
    const editor::DocumentState& crossedFaceResult = crossedFaceWindow.documentStateForTest();
    check(assets::saveToSedBytes(crossedFaceResult.level) == crossedFaceBeforeBytes
            && crossedFaceResult.selection.surfaces == crossedFaceBeforeSelection
            && crossedFaceWindow.undoDepthForTest() == crossedFaceUndoBefore
            && crossedFaceWindow.redoDepthForTest() == crossedFaceRedoBefore,
        "surface Multi-cut rolls back when core rejects a crossed selected face, even if another selected face split and the next line repeats that plane");

    check(ui::viewport::cleaveSupportingPlanesEquivalent(
            TVector{ 1.0, 2.0, 3.0 },
            TVector{ 3.0, -1.0, 0.0 },
            TVector{ -7.0, -14.0, -21.0 },
            TVector{ 5.0, -2.0, 0.0 }),
        "supporting-plane proof accepts diagonal planes with reversed/scaled normals and distinct points on one plane");
    check(!ui::viewport::cleaveSupportingPlanesEquivalent(
            TVector{ 0.0, 0.0, 1.0 },
            TVector{ 0.0, 0.0, 1.0e15 },
            TVector{ 0.0, 0.0, 1.0 },
            TVector{ 0.0, 0.0, 1.0e15 + 1.0 }),
        "supporting-plane proof rejects a representable unit offset at a far world origin");
    check(!ui::viewport::cleaveSupportingPlanesEquivalent(
            TVector{ 1.0, 1.0, 0.0 },
            TVector{},
            TVector{ 1.0, 1.0, 0.0 },
            TVector{ 1.0e15, -1.0e15 + 1.0, 0.0 }),
        "supporting-plane proof rejects a representable diagonal offset despite large tangential coordinate cancellation");
    const double tinyScale = std::ldexp(1.0, -1000);
    const double unitRoundoff = std::ldexp(1.0, -52);
    const TVector underflowSensitiveNormal{
        tinyScale * (1.0 + unitRoundoff),
        0.0,
        1.0,
    };
    const TVector underflowSensitivePoint{
        1.0 + unitRoundoff,
        0.0,
        -tinyScale * (1.0 + std::ldexp(1.0, -51)),
    };
    check(!ui::viewport::cleaveSupportingPlanesEquivalent(
            underflowSensitiveNormal,
            TVector{},
            underflowSensitiveNormal,
            underflowSensitivePoint),
        "supporting-plane proof rejects a tiny nonzero offset when its exact product residual is below subnormal range");
    check(!ui::viewport::cleaveSupportingPlanesEquivalent(
            TVector{ 0.0, 0.0, 1.0 },
            TVector{},
            TVector{ 1.0e-16, 0.0, 1.0 },
            TVector{}),
        "supporting-plane proof rejects a tiny intentional normal tilt with a zero cross-product operand");
    check(!ui::viewport::cleaveSupportingPlanesEquivalent(
            TVector{}, TVector{}, TVector{ 0.0, 0.0, 1.0 }, TVector{}),
        "supporting-plane proof rejects a zero normal");
    check(!ui::viewport::cleaveSupportingPlanesEquivalent(
            TVector{ std::numeric_limits<double>::infinity(), 0.0, 0.0 },
            TVector{},
            TVector{ 1.0, 0.0, 0.0 },
            TVector{}),
        "supporting-plane proof rejects non-finite normals");
    check(!ui::viewport::cleaveSupportingPlanesEquivalent(
            TVector{ 1.0, 0.0, 0.0 },
            TVector{ -std::numeric_limits<double>::max(), 0.0, 0.0 },
            TVector{ 1.0, 0.0, 0.0 },
            TVector{ std::numeric_limits<double>::max(), 0.0, 0.0 }),
        "supporting-plane proof rejects finite coordinates whose translated displacement overflows");

    constexpr SurfaceRef archSource{ 0, 5 };
    const editor::DocumentState archBaseline = makeSurfaceArchState();
    const Surface& authoredArchFace = archBaseline.level.sectors[0].surfaces[5];
    std::vector<SurfaceCleavePlane> uniqueArchPlanes{};
    uniqueArchPlanes.reserve(kOpenSurfaceArchLines.size() - 1);
    for (std::size_t index = 0; index < kOpenSurfaceArchLines.size(); ++index)
    {
        // The sixth staged line shares the fifth line's supporting plane. It
        // is a boundary-following continuation of the same open arch chain.
        if (index == 5)
        {
            continue;
        }
        uniqueArchPlanes.push_back(SurfaceCleavePlane{
            kOpenSurfaceArchLines[index].normal,
            kOpenSurfaceArchLines[index].first,
        });
    }

    Level directArch = archBaseline.level;
    std::vector<SurfaceRef> directArchAppended{};
    const bool directArchAccepted = applySurfacePlanesDirectly(
        directArch, archSource, uniqueArchPlanes, directArchAppended);
    check(directArchAccepted && directArch.sectors.size() == 1
            && directArch.sectors[0].surfaces.size() == 6 + directArchAppended.size()
            && !directArchAppended.empty(),
        "surface Multi-cut arch fixture: direct core sequence accepts the unique supporting planes in author order");

    MainWindow archWindow{};
    archWindow.setAttribute(Qt::WA_DontShowOnScreen);
    archWindow.setDocumentStateForTest(archBaseline);
    selectSurface(archWindow, archSource);
    const auto archBeforeBytes =
        assets::saveToSedBytes(archWindow.documentStateForTest().level);
    if (archBeforeBytes != assets::saveToSedBytes(archBaseline.level))
    {
        reportSurfaceLevelDifference(
            "arch live baseline vs authored fixture",
            archBaseline.level,
            archWindow.documentStateForTest().level);
    }
    check(archBeforeBytes == assets::saveToSedBytes(archBaseline.level),
        "surface Multi-cut arch fixture: installing the document preserves the authored Level bytes before Apply");
    const auto archBeforeSelection = archWindow.documentStateForTest().selection.surfaces;
    const std::size_t archUndoBefore = archWindow.undoDepthForTest();
    const std::size_t archRedoBefore = archWindow.redoDepthForTest();
    const bool archStaged = stageSurfaceLinesAndApply(
        archWindow,
        std::vector<SurfaceMultiCutLine>(kOpenSurfaceArchLines.begin(), kOpenSurfaceArchLines.end()),
        true);
    const editor::DocumentState& archResult = archWindow.documentStateForTest();
    const auto archResultBytes = assets::saveToSedBytes(archResult.level);
    if (archResultBytes != assets::saveToSedBytes(directArch))
    {
        reportSurfaceLevelDifference("arch UI result vs unique-plane core result", directArch, archResult.level);
    }
    std::vector<SurfaceRef> allArchDescendants{ archSource };
    allArchDescendants.insert(
        allArchDescendants.end(), directArchAppended.begin(), directArchAppended.end());
    check(archStaged,
        "surface Multi-cut arch fixture: GlView stages the ten authored lines as one linked open chain and requests Apply");
    check(directArchAccepted && archResultBytes == assets::saveToSedBytes(directArch)
            && archBeforeBytes != archResultBytes
            && archResult.level.sectors.size() == 1
            && archResult.level.sectors[0].surfaces.size() == directArch.sectors[0].surfaces.size(),
        "surface Multi-cut arch fixture: GlView Apply matches the complete Level bytes from the unique-plane core sequence");
    check(surfaceSelectionContainsExactly(archResult, directArchAppended)
            && !archResult.selection.surfaces.contains(archSource)
            && archWindow.undoDepthForTest() == archUndoBefore + 1
            && archWindow.redoDepthForTest() == archRedoBefore,
        "surface Multi-cut arch fixture: successful Apply selects only appended face descendants and creates one Undo");
    check(archResult.level.sectors.size() == 1
            && archResult.level.sectors[0].surfaces.size() > static_cast<std::size_t>(archSource.surface)
            && authoredSurfaceCornersRemainExact(
                archResult.level, authoredArchFace, allArchDescendants),
        "surface Multi-cut arch fixture: the source face identity remains in slot five and original UV/RGBA corners remain exact");
    check(archWindow.undoForTest()
            && assets::saveToSedBytes(archWindow.documentStateForTest().level) == archBeforeBytes
            && archWindow.documentStateForTest().selection.surfaces == archBeforeSelection
            && archWindow.undoDepthForTest() == archUndoBefore
            && archWindow.redoDepthForTest() == archRedoBefore + 1,
        "surface Multi-cut arch fixture: one Undo restores the exact authored Level and original selection");
    check(archWindow.redoForTest()
            && assets::saveToSedBytes(archWindow.documentStateForTest().level)
                == assets::saveToSedBytes(directArch)
            && surfaceSelectionContainsExactly(
                archWindow.documentStateForTest(), directArchAppended)
            && archWindow.undoDepthForTest() == archUndoBefore + 1
            && archWindow.redoDepthForTest() == archRedoBefore,
        "surface Multi-cut arch fixture: one Redo restores the exact core-equivalent Level and appended selection");
    if (assets::saveToSedBytes(archWindow.documentStateForTest().level)
        != assets::saveToSedBytes(directArch))
    {
        reportSurfaceLevelDifference(
            "arch Redo result vs unique-plane core result",
            directArch,
            archWindow.documentStateForTest().level);
    }

    constexpr SurfaceRef duplicateSource{ 0, 5 };
    const editor::DocumentState duplicateBaseline = makeSurfaceArchState();
    Level directDuplicate = duplicateBaseline.level;
    std::vector<SurfaceRef> directDuplicateAppended{};
    const bool directDuplicateAccepted = applySurfacePlanesDirectly(
        directDuplicate,
        duplicateSource,
        { SurfaceCleavePlane{ TVector{ 1.0, 0.0, 0.0 }, TVector{} } },
        directDuplicateAppended);

    MainWindow duplicateWindow{};
    duplicateWindow.setAttribute(Qt::WA_DontShowOnScreen);
    duplicateWindow.setDocumentStateForTest(duplicateBaseline);
    selectSurface(duplicateWindow, duplicateSource);
    const std::size_t duplicateUndoBefore = duplicateWindow.undoDepthForTest();
    if (GlView* const view = duplicateWindow.glViewForTest())
    {
        view->surfacesCleaveBatchRequested(
            QList<TVector>{
                TVector{ 1.0, 0.0, 0.0 },
                TVector{ -4.0, 0.0, 0.0 },
                TVector{ 0.25, 0.0, 0.0 },
            },
            QList<TVector>{ TVector{}, TVector{}, TVector{} },
            false,
            TVector{});
    }
    const editor::DocumentState& duplicateResult = duplicateWindow.documentStateForTest();
    if (directDuplicateAccepted
        && assets::saveToSedBytes(duplicateResult.level) != assets::saveToSedBytes(directDuplicate))
    {
        reportSurfaceLevelDifference(
            "repeated-plane UI result vs direct unique-plane core result",
            directDuplicate,
            duplicateResult.level);
    }
    check(directDuplicateAccepted
            && assets::saveToSedBytes(duplicateResult.level)
                == assets::saveToSedBytes(directDuplicate)
            && surfaceSelectionContainsExactly(duplicateResult, directDuplicateAppended)
            && duplicateWindow.undoDepthForTest() == duplicateUndoBefore + 1,
        "surface Multi-cut accepts multiple proven duplicates across reversed and scaled normals after the plane splits once");

    const editor::DocumentState nearPlaneBaseline = makeSurfaceArchState();
    MainWindow nearPlaneWindow{};
    nearPlaneWindow.setAttribute(Qt::WA_DontShowOnScreen);
    nearPlaneWindow.setDocumentStateForTest(nearPlaneBaseline);
    selectSurface(nearPlaneWindow, duplicateSource);
    const auto nearPlaneBeforeBytes =
        assets::saveToSedBytes(nearPlaneWindow.documentStateForTest().level);
    const auto nearPlaneBeforeSelection =
        nearPlaneWindow.documentStateForTest().selection.surfaces;
    const std::size_t nearPlaneUndoBefore = nearPlaneWindow.undoDepthForTest();
    const std::size_t nearPlaneRedoBefore = nearPlaneWindow.redoDepthForTest();
    if (GlView* const view = nearPlaneWindow.glViewForTest())
    {
        view->surfacesCleaveBatchRequested(
            QList<TVector>{ TVector{ 0.0, 0.0, 1.0 }, TVector{ 0.0, 0.0, 1.0 } },
            QList<TVector>{
                TVector{ 0.0, 0.0, 0.5 },
                TVector{ 0.0, 0.0, 0.5 + 5.0e-10 },
            },
            false,
            TVector{});
    }
    const editor::DocumentState& nearPlaneResult = nearPlaneWindow.documentStateForTest();
    const GlView* const nearPlaneView = nearPlaneWindow.glViewForTest();
    check(assets::saveToSedBytes(nearPlaneResult.level) == nearPlaneBeforeBytes
            && nearPlaneResult.selection.surfaces == nearPlaneBeforeSelection
            && nearPlaneWindow.undoDepthForTest() == nearPlaneUndoBefore
            && nearPlaneWindow.redoDepthForTest() == nearPlaneRedoBefore
            && nearPlaneView
                ->toastTextForTest(GlView::ToastGroup::Error).contains(QStringLiteral("line 2")),
        "surface Multi-cut rejects and rolls back a distinct parallel plane 5e-10 away instead of treating it as redundant");

    std::vector<SurfaceMultiCutLine> archThenOutsideLines(
        kOpenSurfaceArchLines.begin(), kOpenSurfaceArchLines.end());
    archThenOutsideLines.push_back(SurfaceMultiCutLine{
        TVector{ 5.0, 1.0, 0.0 },
        TVector{ 5.0, 1.0, 1.0 },
        TVector{ 1.0, 0.0, 0.0 },
    });
    MainWindow lateOutsideWindow{};
    lateOutsideWindow.setAttribute(Qt::WA_DontShowOnScreen);
    lateOutsideWindow.setDocumentStateForTest(archBaseline);
    selectSurface(lateOutsideWindow, archSource);
    const auto lateOutsideBeforeBytes =
        assets::saveToSedBytes(lateOutsideWindow.documentStateForTest().level);
    const auto lateOutsideBeforeSelection =
        lateOutsideWindow.documentStateForTest().selection.surfaces;
    const std::size_t lateOutsideUndoBefore = lateOutsideWindow.undoDepthForTest();
    const std::size_t lateOutsideRedoBefore = lateOutsideWindow.redoDepthForTest();
    const bool lateOutsideStaged = stageSurfaceLinesAndApply(
        lateOutsideWindow, archThenOutsideLines, false);
    const editor::DocumentState& lateOutsideResult = lateOutsideWindow.documentStateForTest();
    const GlView* const lateOutsideView = lateOutsideWindow.glViewForTest();
    check(lateOutsideStaged
            && assets::saveToSedBytes(lateOutsideResult.level) == lateOutsideBeforeBytes
            && lateOutsideResult.selection.surfaces == lateOutsideBeforeSelection
            && lateOutsideWindow.undoDepthForTest() == lateOutsideUndoBefore
            && lateOutsideWindow.redoDepthForTest() == lateOutsideRedoBefore
            && lateOutsideView
                ->toastTextForTest(GlView::ToastGroup::Error).contains(QStringLiteral("line 11")),
        "surface Multi-cut late outside line rejects after the arch chain and restores exact geometry, selection, and history");

    constexpr SurfaceRef source{ 0, 4 };
    const TVector normal{ 1.0, 0.0, 0.0 };
    const TVector point{ 0.0, 0.0, 0.0 };
    const editor::DocumentState baseline = makeRoundedSurfaceState();

    Level direct = baseline.level;
    const bool directAccepted = level::levelCleaveSurface(
        direct, source, normal, point.x, point.y, point.z);
    check(directAccepted && direct.sectors.size() == 1
            && direct.sectors.front().surfaces.size() == 7,
        "surface Multi-cut fixture: regular one-line cleave accepts inherited rounded source");
    check(!level::surfaceIsPlanar(baseline.level.sectors.front(),
              baseline.level.sectors.front().surfaces[source.surface]),
        "surface Multi-cut fixture: source retains the documented rounded planarity defect");
    if ( directAccepted && direct.sectors.front().surfaces.size() == 7 )
    {
        const Surface& appended = direct.sectors.front().surfaces.back();
        check(!level::surfaceIsPlanar(direct.sectors.front(), appended),
            "surface Multi-cut fixture: appended descendant exercises inherited profile validation");
    }

    MainWindow regularWindow{};
    regularWindow.setAttribute(Qt::WA_DontShowOnScreen);
    regularWindow.setDocumentStateForTest(baseline);
    selectSurface(regularWindow, source);
    const auto regularBeforeBytes =
        assets::saveToSedBytes(regularWindow.documentStateForTest().level);
    const auto regularBeforeSelection =
        regularWindow.documentStateForTest().selection.surfaces;
    const std::size_t regularBeforeUndo = regularWindow.undoDepthForTest();
    if ( GlView* const view = regularWindow.glViewForTest() )
    {
        view->surfacesCleaveRequested(normal, point);
    }
    const editor::DocumentState& regularResult =
        regularWindow.documentStateForTest();
    const auto regularResultBytes = assets::saveToSedBytes(regularResult.level);
    check(regularResult.level.sectors.size() == 1
            && regularResult.level.sectors.front().surfaces.size() == 7
            && regularResult.selection.surfaces.size() == 1
            && regularResult.selection.surfaces.find(SurfaceRef{ 0, 6 })
                != regularResult.selection.surfaces.end()
            && regularWindow.undoDepthForTest() == regularBeforeUndo + 1
            && regularResultBytes != regularBeforeBytes,
        "surface Multi-cut fixture: regular UI cleave publishes the rounded descendant and one Undo entry");

    MainWindow multiCutWindow{};
    multiCutWindow.setAttribute(Qt::WA_DontShowOnScreen);
    multiCutWindow.setDocumentStateForTest(baseline);
    selectSurface(multiCutWindow, source);
    const auto multiBeforeBytes =
        assets::saveToSedBytes(multiCutWindow.documentStateForTest().level);
    const auto multiBeforeSelection =
        multiCutWindow.documentStateForTest().selection.surfaces;
    const std::size_t multiBeforeUndo = multiCutWindow.undoDepthForTest();
    check(multiBeforeBytes == regularBeforeBytes
            && multiBeforeSelection == regularBeforeSelection
            && multiBeforeUndo == regularBeforeUndo,
        "surface Multi-cut fixture: window initialization leaves regular and batch baselines identical");
    if ( GlView* const view = multiCutWindow.glViewForTest() )
    {
        view->surfacesCleaveBatchRequested(
            QList<TVector>{ normal },
            QList<TVector>{ point },
            false,
            TVector{});
    }

    const editor::DocumentState& multiResult =
        multiCutWindow.documentStateForTest();
    const auto multiResultBytes = assets::saveToSedBytes(multiResult.level);
    check(multiResult.level.sectors.size() == 1
            && multiResult.level.sectors.front().surfaces.size() == 7
            && multiResult.selection.surfaces.size() == 1
            && multiResult.selection.surfaces.find(SurfaceRef{ 0, 6 })
                != multiResult.selection.surfaces.end()
            && multiCutWindow.undoDepthForTest() == multiBeforeUndo + 1
            && multiResultBytes == regularResultBytes,
        "surface Multi-cut fixture: batch matches regular cleave for an inherited rounded source");
    check(assets::saveToSedBytes(multiResult.level)
            != multiBeforeBytes,
        "surface Multi-cut fixture: accepted batch publishes the geometry transaction");

    MainWindow failedBatchWindow{};
    failedBatchWindow.setAttribute(Qt::WA_DontShowOnScreen);
    failedBatchWindow.setDocumentStateForTest(baseline);
    selectSurface(failedBatchWindow, source);
    const auto failedBeforeBytes =
        assets::saveToSedBytes(failedBatchWindow.documentStateForTest().level);
    const auto failedBeforeSelection =
        failedBatchWindow.documentStateForTest().selection.surfaces;
    const std::size_t failedBeforeUndo = failedBatchWindow.undoDepthForTest();
    const std::size_t failedBeforeRedo = failedBatchWindow.redoDepthForTest();
    if ( GlView* const view = failedBatchWindow.glViewForTest() )
    {
        view->surfacesCleaveBatchRequested(
            QList<TVector>{ normal, normal },
            QList<TVector>{ point, TVector{ 5.0, 0.0, 0.0 } },
            false,
            TVector{});
    }
    const editor::DocumentState& failedResult =
        failedBatchWindow.documentStateForTest();
    check(assets::saveToSedBytes(failedResult.level) == failedBeforeBytes
            && failedResult.selection.surfaces == failedBeforeSelection
            && failedBatchWindow.undoDepthForTest() == failedBeforeUndo
            && failedBatchWindow.redoDepthForTest() == failedBeforeRedo,
        "surface Multi-cut fixture: a later no-op plane rolls back geometry, selection, and history atomically");

    const editor::DocumentState portalBaseline = makeRoundedPortalState();
    constexpr SurfaceRef portalSource{ 0, 4 };
    const TVector portalXNormal{ 1.0, 0.0, 0.0 };
    const TVector portalYNormal{ 0.0, 1.0, 0.0 };
    const TVector portalPoint{};
    check(allAdjoinBoundaryCoordinatesMatch(portalBaseline.level),
        "surface Multi-cut portal fixture: rounded reciprocal portal starts coincident and reciprocal");

    MainWindow portalSingleWindow{};
    portalSingleWindow.setAttribute(Qt::WA_DontShowOnScreen);
    portalSingleWindow.setDocumentStateForTest(portalBaseline);
    selectSurface(portalSingleWindow, portalSource);
    Level expectedPortalSingle = portalSingleWindow.documentStateForTest().level;
    const bool expectedSingleAccepted = level::levelCleaveSurface(
        expectedPortalSingle, portalSource, portalXNormal,
        portalPoint.x, portalPoint.y, portalPoint.z);
    check(expectedSingleAccepted
            && expectedPortalSingle.sectors.size() == 2
            && expectedPortalSingle.sectors[0].surfaces.size() == 7
            && expectedPortalSingle.sectors[1].surfaces.size() == 7
            && allAdjoinBoundaryCoordinatesMatch(expectedPortalSingle),
        "surface Multi-cut portal fixture: core one-line cleave accepts the rounded reciprocal portal");

    const auto portalSingleBeforeBytes =
        assets::saveToSedBytes(portalSingleWindow.documentStateForTest().level);
    if ( GlView* const view = portalSingleWindow.glViewForTest() )
    {
        view->surfacesCleaveBatchRequested(
            QList<TVector>{ portalXNormal },
            QList<TVector>{ portalPoint },
            false,
            TVector{});
    }
    const editor::DocumentState& portalSingleResult =
        portalSingleWindow.documentStateForTest();
    check(assets::saveToSedBytes(portalSingleResult.level)
            == assets::saveToSedBytes(expectedPortalSingle)
            && portalSingleResult.level.sectors.size() == 2
            && portalSingleResult.level.sectors[0].surfaces.size() == 7
            && portalSingleResult.level.sectors[1].surfaces.size() == 7
            && allAdjoinBoundaryCoordinatesMatch(portalSingleResult.level)
            && portalSingleResult.selection.surfaces.size() == 1
            && portalSingleResult.selection.surfaces.contains(SurfaceRef{ 0, 6 })
            && portalSingleWindow.undoDepthForTest() == 1
            && assets::saveToSedBytes(portalSingleResult.level) != portalSingleBeforeBytes,
        "surface Multi-cut portal fixture: one-plane batch preserves rounded reciprocal adjoins atomically");

    MainWindow portalMultiWindow{};
    portalMultiWindow.setAttribute(Qt::WA_DontShowOnScreen);
    portalMultiWindow.setDocumentStateForTest(portalBaseline);
    selectSurface(portalMultiWindow, portalSource);
    Level expectedPortalMulti = portalMultiWindow.documentStateForTest().level;
    const bool expectedMultiFirst = level::levelCleaveSurface(
        expectedPortalMulti, portalSource, portalXNormal,
        portalPoint.x, portalPoint.y, portalPoint.z);
    const bool expectedMultiSource = expectedMultiFirst && level::levelCleaveSurface(
        expectedPortalMulti, portalSource, portalYNormal,
        portalPoint.x, portalPoint.y, portalPoint.z);
    const bool expectedMultiAppended = expectedMultiSource && level::levelCleaveSurface(
        expectedPortalMulti, SurfaceRef{ 0, 6 }, portalYNormal,
        portalPoint.x, portalPoint.y, portalPoint.z);
    check(expectedMultiAppended
            && expectedPortalMulti.sectors.size() == 2
            && expectedPortalMulti.sectors[0].surfaces.size() == 9
            && expectedPortalMulti.sectors[1].surfaces.size() == 9
            && allAdjoinBoundaryCoordinatesMatch(expectedPortalMulti),
        "surface Multi-cut portal fixture: direct two-plane reference preserves every reciprocal fragment");

    if ( GlView* const view = portalMultiWindow.glViewForTest() )
    {
        view->surfacesCleaveBatchRequested(
            QList<TVector>{ portalXNormal, portalYNormal },
            QList<TVector>{ portalPoint, portalPoint },
            false,
            TVector{});
    }
    const editor::DocumentState& portalMultiResult =
        portalMultiWindow.documentStateForTest();
    check(assets::saveToSedBytes(portalMultiResult.level)
            == assets::saveToSedBytes(expectedPortalMulti)
            && portalMultiResult.level.sectors.size() == 2
            && portalMultiResult.level.sectors[0].surfaces.size() == 9
            && portalMultiResult.level.sectors[1].surfaces.size() == 9
            && allAdjoinBoundaryCoordinatesMatch(portalMultiResult.level)
            && portalMultiResult.selection.surfaces.size() == 3
            && portalMultiResult.selection.surfaces.contains(SurfaceRef{ 0, 6 })
            && portalMultiResult.selection.surfaces.contains(SurfaceRef{ 0, 7 })
            && portalMultiResult.selection.surfaces.contains(SurfaceRef{ 0, 8 })
            && portalMultiWindow.undoDepthForTest() == 1,
        "surface Multi-cut portal fixture: two-plane batch matches the direct core sequence and keeps reciprocal fragments");

    const editor::DocumentState boundedSurfaceBaseline = makeSurfaceArchState();
    constexpr SurfaceRef boundedSurfaceSource{ 0, 0 };
    const std::vector<SurfaceMultiCutLine> boundedOneSideLines{
        SurfaceMultiCutLine{
            TVector{ 1.0, 0.0, 0.0 },
            TVector{ 1.0, 0.0, 1.0 },
            TVector{ 0.0, 1.0, 0.0 },
        },
        SurfaceMultiCutLine{
            TVector{ 1.0, 0.25, 0.5 },
            TVector{ 1.0, 0.9, 0.5 },
            TVector{ 0.0, 0.0, 1.0 },
        },
    };
    const std::vector<SurfaceMultiCutLine> boundedCrossingLines{
        boundedOneSideLines[0],
        SurfaceMultiCutLine{
            TVector{ 1.0, -0.75, 0.5 },
            TVector{ 1.0, 0.75, 0.5 },
            TVector{ 0.0, 0.0, 1.0 },
        },
    };

    MainWindow boundedOneSideWindow{};
    boundedOneSideWindow.setAttribute(Qt::WA_DontShowOnScreen);
    boundedOneSideWindow.setDocumentStateForTest(boundedSurfaceBaseline);
    selectSurface(boundedOneSideWindow, boundedSurfaceSource);
    const bool boundedOneSideStaged = stageSurfaceLinesAndApply(
        boundedOneSideWindow, boundedOneSideLines, false, true);
    const editor::DocumentState& boundedOneSideResult =
        boundedOneSideWindow.documentStateForTest();
    check(boundedOneSideStaged
            && boundedOneSideResult.level.sectors.size() == 1
            && boundedOneSideResult.level.sectors.front().surfaces.size() == 8
            && boundedOneSideWindow.undoDepthForTest() == 1,
        "surface Multi-cut Bounded: a later stroke on one side splits only the reached face descendant");

    MainWindow boundedCrossingWindow{};
    boundedCrossingWindow.setAttribute(Qt::WA_DontShowOnScreen);
    boundedCrossingWindow.setDocumentStateForTest(boundedSurfaceBaseline);
    selectSurface(boundedCrossingWindow, boundedSurfaceSource);
    const bool boundedCrossingStaged = stageSurfaceLinesAndApply(
        boundedCrossingWindow, boundedCrossingLines, false, true);
    const editor::DocumentState& boundedCrossingResult =
        boundedCrossingWindow.documentStateForTest();
    check(boundedCrossingStaged
            && boundedCrossingResult.level.sectors.size() == 1
            && boundedCrossingResult.level.sectors.front().surfaces.size() == 9
            && boundedCrossingWindow.undoDepthForTest() == 1,
        "surface Multi-cut Bounded: a later stroke crossing the prior split reaches both face descendants");

    MainWindow unboundedSurfaceWindow{};
    unboundedSurfaceWindow.setAttribute(Qt::WA_DontShowOnScreen);
    unboundedSurfaceWindow.setDocumentStateForTest(boundedSurfaceBaseline);
    selectSurface(unboundedSurfaceWindow, boundedSurfaceSource);
    const bool unboundedSurfaceStaged = stageSurfaceLinesAndApply(
        unboundedSurfaceWindow, boundedOneSideLines, false, false);
    const editor::DocumentState& unboundedSurfaceResult =
        unboundedSurfaceWindow.documentStateForTest();
    check(unboundedSurfaceStaged
            && unboundedSurfaceResult.level.sectors.size() == 1
            && unboundedSurfaceResult.level.sectors.front().surfaces.size() == 9
            && unboundedSurfaceWindow.undoDepthForTest() == 1,
        "surface Multi-cut Bounded off: the old global plane still splits every face descendant");

    MainWindow boundedPortalWindow{};
    boundedPortalWindow.setAttribute(Qt::WA_DontShowOnScreen);
    boundedPortalWindow.setDocumentStateForTest(portalBaseline);
    selectSurface(boundedPortalWindow, portalSource);
    boundedPortalWindow.mutableDocumentStateForTest().selection.surfaces = {
        portalSource,
        SurfaceRef{ 1, 5 },
    };
    boundedPortalWindow.updateEditActionsForTest();
    GlView* const boundedPortalView = boundedPortalWindow.glViewForTest();
    if ( boundedPortalView )
    {
        boundedPortalView->setSelectionState(
            portalSource.sector,
            portalSource.surface,
            -1,
            -1,
            boundedPortalWindow.documentStateForTest().selection);
    }
    const editor::Selection boundedPortalBeforeSelection =
        boundedPortalWindow.documentStateForTest().selection;
    const auto boundedPortalBeforeBytes =
        assets::saveToSedBytes(boundedPortalWindow.documentStateForTest().level);
    Level expectedBoundedPortal = boundedPortalWindow.documentStateForTest().level;
    const bool expectedBoundedPortalFirst = level::levelCleaveSurface(
        expectedBoundedPortal,
        portalSource,
        portalXNormal,
        portalPoint.x,
        portalPoint.y,
        portalPoint.z);
    const bool expectedBoundedPortalSecond = expectedBoundedPortalFirst
        && level::levelCleaveSurface(
            expectedBoundedPortal,
            SurfaceRef{ 0, 6 },
            portalYNormal,
            0.5,
            0.0,
            1.0);
    const auto portalFragmentCount = [](const Level& level, const std::size_t sectorIndex)
    {
        if ( sectorIndex >= level.sectors.size() )
        {
            return std::size_t{};
        }
        const Sector& sector = level.sectors[sectorIndex];
        return static_cast<std::size_t>(std::count_if(
            sector.surfaces.begin(),
            sector.surfaces.end(),
            [sectorIndex](const Surface& surface)
            {
                return surface.adjoin
                    && surface.adjoin->sector != static_cast<std::int32_t>(sectorIndex);
            }));
    };
    check(expectedBoundedPortalSecond
            && expectedBoundedPortal.sectors.size() == 2
            && portalFragmentCount(expectedBoundedPortal, 0) == 3
            && portalFragmentCount(expectedBoundedPortal, 1) == 3
            && allAdjoinBoundaryCoordinatesMatch(expectedBoundedPortal),
        "surface Multi-cut portal fixture: one bounded-side split produces three reciprocal portal fragments per owner");
    if ( boundedPortalView )
    {
        boundedPortalView->surfacesCleaveBatchRequested(
            QList<TVector>{ portalXNormal, portalYNormal },
            QList<TVector>{ portalPoint, TVector{ 0.5, 0.0, 1.0 } },
            false,
            TVector{},
            QList<TVector>{ TVector{ 0.0, 0.0, 1.0 }, TVector{ 0.5, 0.0, 0.0 } },
            true);
    }
    const editor::DocumentState& boundedPortalResult =
        boundedPortalWindow.documentStateForTest();
    const auto boundedPortalAfterBytes =
        assets::saveToSedBytes(boundedPortalResult.level);
    check(boundedPortalView
            && boundedPortalResult.level.sectors.size() == 2
            && boundedPortalResult.level.sectors[0].surfaces.size() == 8
            && boundedPortalResult.level.sectors[1].surfaces.size() == 8
            && portalFragmentCount(boundedPortalResult.level, 0) == 3
            && portalFragmentCount(boundedPortalResult.level, 1) == 3
            && allAdjoinBoundaryCoordinatesMatch(boundedPortalResult.level)
            && boundedPortalAfterBytes == assets::saveToSedBytes(expectedBoundedPortal)
            && boundedPortalResult.selection.surfaces.size() == 4
            && boundedPortalResult.selection.surfaces.contains(SurfaceRef{ 0, 6 })
            && boundedPortalResult.selection.surfaces.contains(SurfaceRef{ 0, 7 })
            && boundedPortalResult.selection.surfaces.contains(SurfaceRef{ 1, 6 })
            && boundedPortalResult.selection.surfaces.contains(SurfaceRef{ 1, 7 })
            && boundedPortalWindow.undoDepthForTest() == 1,
        "surface Multi-cut Bounded: dual-selected reciprocal owners follow one reached lineage and match exact geometry and UVs");
    check(boundedPortalWindow.undoForTest()
            && assets::saveToSedBytes(boundedPortalWindow.documentStateForTest().level)
                == boundedPortalBeforeBytes
            && boundedPortalWindow.documentStateForTest().selection.surfaces
                == boundedPortalBeforeSelection.surfaces
            && boundedPortalWindow.undoDepthForTest() == 0
            && boundedPortalWindow.redoDepthForTest() == 1,
        "surface Multi-cut Bounded: Undo restores both selected reciprocal owners and their exact pre-cut state");
    check(boundedPortalWindow.redoForTest()
            && assets::saveToSedBytes(boundedPortalWindow.documentStateForTest().level)
                == boundedPortalAfterBytes
            && boundedPortalWindow.documentStateForTest().selection.surfaces.size() == 4
            && boundedPortalWindow.documentStateForTest().selection.surfaces.contains(SurfaceRef{ 0, 6 })
            && boundedPortalWindow.documentStateForTest().selection.surfaces.contains(SurfaceRef{ 0, 7 })
            && boundedPortalWindow.documentStateForTest().selection.surfaces.contains(SurfaceRef{ 1, 6 })
            && boundedPortalWindow.documentStateForTest().selection.surfaces.contains(SurfaceRef{ 1, 7 })
            && boundedPortalWindow.undoDepthForTest() == 1
            && boundedPortalWindow.redoDepthForTest() == 0,
        "surface Multi-cut Bounded: Redo restores the exact reached portal descendants and authored UVs");

    editor::DocumentState boundedSectorBaseline{};
    boundedSectorBaseline.level.kind = ProjectType::IJIM;
    boundedSectorBaseline.level.layers = { "Layer0" };
    level::levelCreateCube(
        boundedSectorBaseline.level,
        0.0,
        0.0,
        0.0,
        TVector::up,
        TVector::right);
    boundedSectorBaseline.selection.sectors.insert(0);

    MainWindow boundedSectorWindow{};
    boundedSectorWindow.setAttribute(Qt::WA_DontShowOnScreen);
    boundedSectorWindow.setDocumentStateForTest(boundedSectorBaseline);
    boundedSectorWindow.activateSectorEditModeForTest();
    boundedSectorWindow.mutableDocumentStateForTest().selection.sectors = { 0 };
    boundedSectorWindow.updateEditActionsForTest();
    QAction* const boundedSectorCleaveAction = boundedSectorWindow.findChild<QAction*>(
        QStringLiteral("viewport.sector.cleave"));
    QToolButton* const boundedSectorMultiCutButton =
        boundedSectorWindow.findChild<QToolButton*>(
            QStringLiteral("cleaveMultiCutButton"));
    QToolButton* const boundedSectorChainButton =
        boundedSectorWindow.findChild<QToolButton*>(
            QStringLiteral("cleaveChainButton"));
    QToolButton* const boundedSectorBoundedButton =
        boundedSectorWindow.findChild<QToolButton*>(
            QStringLiteral("cleaveBoundedButton"));
    if (boundedSectorCleaveAction && boundedSectorMultiCutButton
        && boundedSectorChainButton && boundedSectorBoundedButton)
    {
        boundedSectorCleaveAction->setChecked(true);
        boundedSectorMultiCutButton->setChecked(true);
        boundedSectorChainButton->setChecked(false);
        boundedSectorBoundedButton->setChecked(true);
    }
    GlView* const boundedSectorView = boundedSectorWindow.glViewForTest();
    if (boundedSectorView)
    {
        boundedSectorView->setSelectionState(
            0,
            -1,
            -1,
            -1,
            boundedSectorWindow.documentStateForTest().selection);
        boundedSectorView->captureStagedCleaveContextForTest(false);
        check(boundedSectorCleaveAction
                && boundedSectorCleaveAction->isEnabled()
                && boundedSectorCleaveAction->isChecked()
                && boundedSectorMultiCutButton
                && boundedSectorMultiCutButton->isChecked()
                && boundedSectorView->sectorCleaveToolEnabled()
                && boundedSectorView->cleaveStagedMode()
                && boundedSectorView->cleaveBounded()
                && boundedSectorWindow.documentStateForTest().selection.sectors.size() == 1
                && boundedSectorWindow.documentStateForTest().selection.sectors.contains(0)
                && boundedSectorView->stagedCleaveTargetsMatchCurrentForTest(false),
            "sector Multi-cut Bounded fixture: actual Cleave and Multi-cut controls own the selected target");
        boundedSectorView->addStagedCleaveDocumentLineForTest(
            TVector{ 0.5, 0.0, 0.5 },
            TVector{ 0.5, 1.0, 0.5 },
            TVector{ 1.0, 0.0, 0.0 });
        boundedSectorView->addStagedCleaveDocumentLineForTest(
            TVector{ 0.75, 0.5, 0.0 },
            TVector{ 0.75, 0.5, 1.0 },
            TVector{ 0.0, 1.0, 0.0 });
        boundedSectorView->requestApplyStagedCleave();
    }
    check(boundedSectorView
            && boundedSectorWindow.documentStateForTest().level.sectors.size() == 3
            && boundedSectorWindow.undoDepthForTest() == 1,
        "sector Multi-cut Bounded: the real staged Apply signal carries finite reach to the Sector batch operation");
    (void)restoreBoundedSetting;
}

void runSurfaceRoundoffMultiCutRegressionTests()
{
    constexpr SurfaceRef source{ 0, 5 };
    const auto cornerTuple = [](const Vertex& point)
    {
        return TexVertex{
            static_cast<float>(2.0 * point.x + 3.0 * point.z - 7.0),
            static_cast<float>(-4.0 * point.x + 5.0 * point.z + 0.25),
            ColorF{
                static_cast<float>(0.25 + 0.125 * point.x + 0.25 * point.z),
                static_cast<float>(0.75 - 0.0625 * point.x - 0.25 * point.z),
                static_cast<float>(0.125 + 0.125 * point.z),
                static_cast<float>(0.5 + 0.125 * point.x - 0.125 * point.z) },
        };
    };
    const auto captureEditor = [](MainWindow& window)
    {
        return QCborValue::fromCbor(window.captureRecoveryEditorStateForTest()).toMap();
    };
    const auto draft = [](const QCborMap& editor)
    {
        return editor.value(QStringLiteral("viewport")).toArray().at(38);
    };

    const auto& preceding = kRoundoffSurfaceShapeLines[4];
    const auto& continuation = kRoundoffSurfaceShapeLines[5];
    check(!ui::viewport::cleaveSupportingPlanesEquivalent(preceding.normal, preceding.first,
            continuation.normal, continuation.first),
        "Surface roundoff fixture keeps the exact plane-only equivalence rule conservative");
    for ( const double scale : {0x1p-40, 1.0, 0x1p40} )
    {
        check(ui::viewport::cleaveStrokesShareRoundedSupportingPlane(preceding.normal,
                preceding.first * scale, preceding.second * scale, continuation.normal,
                continuation.first * scale, continuation.second * scale)
                && ui::viewport::cleaveStrokesShareRoundedSupportingPlane(preceding.normal,
                    preceding.first * scale, preceding.second * scale, continuation.normal * -7.0,
                    continuation.second * scale, continuation.first * scale),
            "Surface construction-roundoff proof accepts the fixed shared joint across world units and reversed/scaled normals");
    }
    check(!ui::viewport::cleaveStrokesShareRoundedSupportingPlane(
            TVector{0.0, 0.0, 1.0}, TVector{-0.5, 1.0, 0.0}, TVector{0.0, 1.0, 0.0},
            TVector{1.0e-16, 0.0, 1.0}, TVector{0.0, 1.0, 0.0}, TVector{0.5, 1.0, 0.0}),
        "Surface construction-roundoff proof rejects a tiny normal tilt unsupported by its exact zero-height stroke");
    check(!ui::viewport::cleaveStrokesShareRoundedSupportingPlane(preceding.normal,
            preceding.first, preceding.second, continuation.normal,
            continuation.first + TVector{0.0, 0.0, 0x1p-30},
            continuation.second + TVector{0.0, 0.0, 0x1p-30}),
        "Surface construction-roundoff proof requires an exactly shared endpoint and cannot bridge a nearby offset");
    check(!ui::viewport::cleaveStrokesShareRoundedSupportingPlane(
            TVector{0.0, 0.0, 1.0}, TVector{-0.5, 1.0, 0.0}, TVector{0.0, 1.0, 0.0},
            TVector{0.0, 1.0, 1.0}, TVector{0.0, 1.0, 0.0}, TVector{0.5, 1.0, 0.0}),
        "Surface construction-roundoff proof rejects materially different planes containing the same shared straight stroke");
    const TVector farPoint{1.0e15, 1.0e15, 1.0e15};
    check(!ui::viewport::cleaveStrokesShareRoundedSupportingPlane(
            TVector{0.0, 0.0, 1.0}, farPoint, farPoint + TVector{0.125, 0.0, 0.0},
            TVector{0.0, 0.0, 1.0}, farPoint + TVector{0.125, 0.0, 0.0},
            farPoint + TVector{0.25, 0.0, 0.0}),
        "Surface construction-roundoff proof rejects a far-origin stroke unresolved by its represented-coordinate bounds");
    for ( const TVector invalidNormal : {TVector{},
            TVector{std::numeric_limits<double>::quiet_NaN(), 0.0, 1.0},
            TVector{std::numeric_limits<double>::infinity(), 0.0, 1.0}} )
    {
        check(!ui::viewport::cleaveStrokesShareRoundedSupportingPlane(invalidNormal,
                preceding.first, preceding.second, continuation.normal,
                continuation.first, continuation.second),
            "Surface construction-roundoff proof rejects zero and non-finite normals");
    }
    check(!ui::viewport::cleaveStrokesShareRoundedSupportingPlane(preceding.normal,
            preceding.first, preceding.second, continuation.normal, continuation.first,
            TVector{0.5, 1.0, std::numeric_limits<double>::quiet_NaN()}),
        "Surface construction-roundoff proof rejects non-finite endpoints");
    const double underflowScale = std::ldexp(1.0, -1000);
    const double unitRoundoff = std::ldexp(1.0, -52);
    const TVector underflowNormal{underflowScale * (1.0 + unitRoundoff), 0.0, 1.0};
    const TVector underflowPoint{1.0 + unitRoundoff, 0.0,
        -underflowScale * (1.0 + std::ldexp(1.0, -51))};
    check(!ui::viewport::cleaveStrokesShareRoundedSupportingPlane(underflowNormal,
            TVector{}, underflowPoint, underflowNormal, underflowPoint, underflowPoint * 2.0),
        "Surface construction-roundoff proof rejects a product whose exact residual is below subnormal range");

    for ( const bool bounded : {true, false} )
    {
        editor::DocumentState baseline = makeSurfaceArchState();
        Surface& authored = baseline.level.sectors[0].surfaces[source.surface];
        authored.uScale = 0.125f;
        authored.vScale = -0.375f;
        authored.extraLight = {0.125f, -0.25f, 0.5f, 0.75f};
        for ( std::size_t corner = 0; corner < authored.vertices.size(); ++corner )
        {
            authored.texVertices[corner] = cornerTuple(
                baseline.level.sectors[0].vertices[authored.vertices[corner]]);
        }
        MainWindow window;
        window.setAttribute(Qt::WA_DontShowOnScreen);
        window.setDocumentStateForTest(baseline);
        selectSurface(window, source);
        GlView* const view = window.glViewForTest();
        const auto beforeLevel = assets::saveToSedBytes(window.documentStateForTest().level);
        const std::size_t undoBefore = window.undoDepthForTest();
        const std::size_t redoBefore = window.redoDepthForTest();
        const bool staged = stageSurfaceLines(window,
            std::vector<SurfaceMultiCutLine>(kRoundoffSurfaceShapeLines.begin(),
                kRoundoffSurfaceShapeLines.end()), false, bounded);
        check(staged && view && view->stagedCleaveLineCount() == 33,
            "Surface roundoff Multi-cut stages all 33 shape lines in authored execution order");
        if ( !staged || !view )
        {
            continue;
        }
        // Direct geometry injection does not record authored line commands.
        // Install an admitted timeline with both Undo and Redo before capturing
        // the exact baseline; Apply otherwise supplies a fresh baseline for a
        // history-free legacy draft, as documented by its workspace memento.
        auto stagedDraft = view->captureStagedCleaveDraftState();
        auto precedingDraft = stagedDraft;
        precedingDraft.lines.pop_back();
        precedingDraft.applyOrder.pop_back();
        precedingDraft.selectedCleaveLine = 31;
        precedingDraft.selectedCleaveLines = {31};
        auto futureDraft = stagedDraft;
        std::swap(futureDraft.applyOrder[31], futureDraft.applyOrder[32]);
        GlView::RecoveryState::CleaveHistory fixtureHistory{};
        fixtureHistory.snapshots = {std::move(precedingDraft), stagedDraft, std::move(futureDraft)};
        fixtureHistory.cursor = 1u;
        view->restoreStagedCleaveDraftState(std::move(stagedDraft), std::move(fixtureHistory));
        const GlView::StagedCleaveTransformMode beforeTransformMode = bounded
            ? GlView::StagedCleaveTransformMode::Rotate
            : GlView::StagedCleaveTransformMode::Scale;
        check(view->canTransformSelectedStagedCleave(beforeTransformMode),
            "Surface roundoff Multi-cut fixture selection admits its saved Rotate or Scale mode");
        view->setStagedCleaveTransformMode(beforeTransformMode);
        window.updateEditActionsForTest();
        const QCborMap beforeEditor = captureEditor(window);
        check(view->stagedCleaveTransformMode() == beforeTransformMode
                && draft(beforeEditor).toMap().value(QStringLiteral("transformMode")).toInteger()
                    == static_cast<int>(beforeTransformMode),
            "Surface roundoff Multi-cut captures the exact saved Rotate or Scale mode before Apply");
        check(view->canUndoStagedCleaveHistory() && view->canRedoStagedCleaveHistory()
                && draft(beforeEditor).toMap().value(QStringLiteral("history")).isMap(),
            "Surface roundoff Multi-cut fixture owns a retained local Undo and Redo timeline before Apply");
        view->requestApplyStagedCleave();
        const auto& result = window.documentStateForTest();
        const auto appliedLevel = assets::saveToSedBytes(result.level);
        const bool accepted = appliedLevel != beforeLevel
            && result.level.sectors.size() == 1
            && result.level.sectors[0].surfaces.size() > 6
            && !view->hasStagedCleaveDraft()
            && window.undoDepthForTest() == undoBefore + 1;
        check(accepted, bounded
            ? "Surface roundoff Multi-cut Bounded accepts the shared-endpoint arch continuation and overlapping shape arrangement"
            : "Surface roundoff Multi-cut unbounded accepts the shared-endpoint arch continuation and overlapping shape arrangement");
        if ( !accepted )
        {
            std::cerr << "DIAG: Surface roundoff Multi-cut bounded=" << bounded
                << " faces=" << result.level.sectors[0].surfaces.size()
                << " error=" << view->toastTextForTest(GlView::ToastGroup::Error).toStdString() << '\n';
            check(appliedLevel == beforeLevel && draft(captureEditor(window)) == draft(beforeEditor)
                    && window.undoDepthForTest() == undoBefore
                    && window.redoDepthForTest() == redoBefore,
                "Rejected Surface roundoff Multi-cut preserves exact Level, draft and both history depths");
            continue;
        }

        const Sector& sector = result.level.sectors[0];
        std::vector<SurfaceRef> descendants{source};
        std::vector<SurfaceRef> appended;
        std::map<std::pair<std::int32_t, std::int32_t>, std::pair<int, int>> incidence;
        bool valid = level::sectorIsConvex(sector);
        long double area{};
        for ( std::size_t faceIndex = 0; faceIndex < sector.surfaces.size(); ++faceIndex )
        {
            const Surface& face = sector.surfaces[faceIndex];
            valid = valid && level::surfaceIsPlanar(sector, face)
                && level::surfaceIsConvex(sector, face);
            for ( std::size_t corner = 0; corner < face.vertices.size(); ++corner )
            {
                const auto first = face.vertices[corner];
                const auto second = face.vertices[(corner + 1) % face.vertices.size()];
                auto& edge = incidence[std::minmax(first, second)];
                ++edge.first;
                edge.second += first < second ? 1 : -1;
            }
            if ( faceIndex != static_cast<std::size_t>(source.surface) && faceIndex < 6 )
            {
                continue;
            }
            if ( faceIndex >= 6 )
            {
                appended.push_back({0, static_cast<std::int32_t>(faceIndex)});
                descendants.push_back(appended.back());
            }
            valid = valid && face.uScale == authored.uScale && face.vScale == authored.vScale
                && face.extraLight == authored.extraLight
                && face.texVertices.size() == face.vertices.size();
            long double doubleArea{};
            for ( std::size_t corner = 0; valid && corner < face.vertices.size(); ++corner )
            {
                const Vertex& point = sector.vertices[face.vertices[corner]];
                const Vertex& next = sector.vertices[face.vertices[(corner + 1) % face.vertices.size()]];
                doubleArea += static_cast<long double>(point.x) * next.z
                    - static_cast<long double>(next.x) * point.z;
                const TexVertex expected = cornerTuple(point);
                const TexVertex& actual = face.texVertices[corner];
                const auto agrees = [](const float first, const float second)
                {
                    return std::isfinite(first) && std::isfinite(second)
                        && std::abs(first - second) <= 16.0f * std::numeric_limits<float>::epsilon()
                            * std::max(1.0f, std::abs(second));
                };
                valid = agrees(actual.u, expected.u) && agrees(actual.v, expected.v)
                    && agrees(actual.color.r, expected.color.r) && agrees(actual.color.g, expected.color.g)
                    && agrees(actual.color.b, expected.color.b) && agrees(actual.color.a, expected.color.a);
            }
            area += std::abs(doubleArea) / 2.0L;
        }
        valid = valid && std::all_of(incidence.begin(), incidence.end(), [](const auto& edge)
            {
                return edge.second.first == 2 && edge.second.second == 0;
            });
        const QCborMap appliedEditor = captureEditor(window);
        check(valid && std::abs(area - 2.0L) <= 1.0e-12L
                && result.level.uvCoordinateSpace == UvCoordinateSpace::Normalized
                && authoredSurfaceCornersRemainExact(result.level, authored, descendants),
            "Surface roundoff Multi-cut preserves closed convex geometry, wall area, exact original UV/RGBA and analytic generated tuples");
        check(surfaceSelectionContainsExactly(result, appended)
                && !result.selection.surfaces.contains(source)
                && appliedEditor.value(QStringLiteral("active")).toArray().at(1).toInteger() == 0
                && appliedEditor.value(QStringLiteral("active")).toArray().at(2).toInteger()
                    == static_cast<std::int64_t>(sector.surfaces.size() - 1)
                && window.redoDepthForTest() == redoBefore,
            "Surface roundoff Multi-cut selects only appended descendants and activates the newest face in one Level transaction");
        const Level appliedGeometry = result.level;
        QAction* const undoAction = window.findChild<QAction*>(QStringLiteral("edit.undo"));
        QAction* const redoAction = window.findChild<QAction*>(QStringLiteral("edit.redo"));
        const bool undoReady = undoAction && undoAction->isEnabled();
        check(undoReady, "Surface roundoff Multi-cut exposes the real application Undo action after Apply");
        if ( undoReady ) undoAction->trigger();
        check(undoReady && assets::saveToSedBytes(window.documentStateForTest().level) == beforeLevel
                && window.undoDepthForTest() == undoBefore && window.redoDepthForTest() == redoBefore + 1,
            "Surface roundoff Multi-cut Undo restores exact authored Level bytes");
        const QCborMap afterUndo = captureEditor(window);
        const bool exactWorkspace = draft(afterUndo) == draft(beforeEditor)
                && view->stagedCleaveTransformMode() == beforeTransformMode
                && afterUndo.value(QStringLiteral("selection")) == beforeEditor.value(QStringLiteral("selection"))
                && afterUndo.value(QStringLiteral("active")) == beforeEditor.value(QStringLiteral("active"))
                && afterUndo.value(QStringLiteral("edit")) == beforeEditor.value(QStringLiteral("edit"))
                && window.undoDepthForTest() == undoBefore && window.redoDepthForTest() == redoBefore + 1;
        check(exactWorkspace,
            "Surface roundoff Multi-cut Undo restores all 33 lines, local history, ranks, options, selection and active face exactly");
        if ( !exactWorkspace )
        {
            const auto firstDifference = [](auto&& self, const QCborValue& expected,
                const QCborValue& actual, const QString& path) -> QString
            {
                if ( expected == actual ) return {};
                if ( expected.isMap() && actual.isMap() )
                {
                    const auto expectedMap = expected.toMap();
                    const auto actualMap = actual.toMap();
                    for ( auto item = expectedMap.constBegin(); item != expectedMap.constEnd(); ++item )
                    {
                        const QString child = path + QLatin1Char('.') + item.key().toString();
                        if ( !actualMap.contains(item.key()) ) return child + QStringLiteral(" (missing)");
                        if ( const auto difference = self(self, item.value(), actualMap.value(item.key()), child);
                             !difference.isEmpty() ) return difference;
                    }
                    for ( auto item = actualMap.constBegin(); item != actualMap.constEnd(); ++item )
                    {
                        if ( !expectedMap.contains(item.key()) )
                            return path + QLatin1Char('.') + item.key().toString() + QStringLiteral(" (added)");
                    }
                }
                if ( expected.isArray() && actual.isArray() )
                {
                    const auto expectedArray = expected.toArray();
                    const auto actualArray = actual.toArray();
                    if ( expectedArray.size() != actualArray.size() ) return path + QStringLiteral(".size");
                    for ( qsizetype index = 0; index < expectedArray.size(); ++index )
                    {
                        if ( const auto difference = self(self, expectedArray.at(index), actualArray.at(index),
                                path + QStringLiteral("[%1]").arg(index)); !difference.isEmpty() ) return difference;
                    }
                }
                return path + QStringLiteral(" expected=") + expected.toDiagnosticNotation()
                    + QStringLiteral(" actual=") + actual.toDiagnosticNotation();
            };
            std::cerr << "DIAG: Surface roundoff Undo bounded=" << bounded
                << " cleave=" << firstDifference(firstDifference, draft(beforeEditor), draft(afterUndo),
                    QStringLiteral("cleave")).toStdString();
            for ( const auto& key : {QStringLiteral("selection"), QStringLiteral("active"), QStringLiteral("edit")} )
            {
                std::cerr << ' ' << firstDifference(firstDifference, beforeEditor.value(key), afterUndo.value(key), key).toStdString();
            }
            std::cerr << " undo=" << window.undoDepthForTest() << '/' << undoBefore
                << " redo=" << window.redoDepthForTest() << '/' << redoBefore + 1 << '\n';
        }
        const bool redoReady = redoAction && redoAction->isEnabled();
        check(redoReady, "Surface roundoff Multi-cut exposes the paired application Redo despite retained local Redo");
        const QCborMap beforeRedo = captureEditor(window);
        const auto beforeRedoSurfaces = window.documentStateForTest().selection.surfaces;
        if ( redoReady ) redoAction->trigger();
        const auto redoLevel = assets::saveToSedBytes(window.documentStateForTest().level);
        const QCborMap afterRedo = captureEditor(window);
        const bool redoSelectionExact = afterRedo.value(QStringLiteral("selection"))
            == beforeRedo.value(QStringLiteral("selection"));
        const bool redoActiveExact = afterRedo.value(QStringLiteral("active"))
            == beforeRedo.value(QStringLiteral("active"));
        const bool exactRedo = redoReady && redoLevel == appliedLevel
                && redoSelectionExact && redoActiveExact
                && !view->hasStagedCleaveDraft()
                && window.undoDepthForTest() == undoBefore + 1 && window.redoDepthForTest() == redoBefore;
        check(exactRedo,
            "Surface roundoff Multi-cut Redo restores exact applied geometry and UV/light data while preserving the current selection and active object");
        if ( !exactRedo )
        {
            const auto& actualSelection = window.documentStateForTest().selection.surfaces;
            const auto missing = std::find_if(beforeRedoSurfaces.begin(), beforeRedoSurfaces.end(),
                [&actualSelection](const SurfaceRef ref) { return !actualSelection.contains(ref); });
            const auto added = std::find_if(actualSelection.begin(), actualSelection.end(),
                [&beforeRedoSurfaces](const SurfaceRef ref) { return !beforeRedoSurfaces.contains(ref); });
            std::cerr << "DIAG: Surface roundoff Redo bounded=" << bounded
                << " levelExact=" << (redoLevel == appliedLevel)
                << " firstLevel=" << firstSurfaceLevelDifference(appliedGeometry, window.documentStateForTest().level)
                << " selectionExact=" << redoSelectionExact
                << " activeExact=" << redoActiveExact
                << " surfaceSelection=" << actualSelection.size() << '/' << beforeRedoSurfaces.size();
            if ( missing != beforeRedoSurfaces.end() )
                std::cerr << " missingSurface=" << missing->sector << ',' << missing->surface;
            if ( added != actualSelection.end() )
                std::cerr << " addedSurface=" << added->sector << ',' << added->surface;
            std::cerr << " activeExpected=" << beforeRedo.value(QStringLiteral("active")).toDiagnosticNotation().toStdString()
                << " activeActual=" << afterRedo.value(QStringLiteral("active")).toDiagnosticNotation().toStdString()
                << " draftLines=" << view->stagedCleaveLineCount()
                << " undo=" << window.undoDepthForTest() << '/' << undoBefore + 1
                << " redo=" << window.redoDepthForTest() << '/' << redoBefore << '\n';
        }
    }

    for ( const bool bounded : {true, false} )
    {
        constexpr std::array<std::pair<bool, bool>, 4> cases{{
            {true, false}, {true, true}, {false, false}, {false, true}}};
        for ( const auto [subThreshold, offset] : cases )
        {
            const double intentionalTilt = subThreshold ? 0x1p-32 : 1.0e-7;
            const double intentionalOffset = subThreshold ? 0x1p-31 : 1.0e-8;
            MainWindow window;
            window.setAttribute(Qt::WA_DontShowOnScreen);
            window.setDocumentStateForTest(makeSurfaceArchState());
            selectSurface(window, source);
            const std::vector<SurfaceMultiCutLine> lines{
                {{-0.5, 1.0, 0.5}, {0.0, 1.0, 0.5}, {0.0, 0.0, 1.0}},
                offset
                    ? SurfaceMultiCutLine{{0.0, 1.0, 0.5 + intentionalOffset},
                        {0.5, 1.0, 0.5 + intentionalOffset}, {0.0, 0.0, 1.0}}
                    : SurfaceMultiCutLine{{0.0, 1.0, 0.5},
                        {0.5, 1.0, 0.5 - intentionalTilt * 0.5}, {intentionalTilt, 0.0, 1.0}},
                {{0.25, 1.0, 0.0}, {0.25, 1.0, 1.0}, {1.0, 0.0, 0.0}},
            };
            const auto beforeLevel = assets::saveToSedBytes(window.documentStateForTest().level);
            const auto beforeSelection = window.documentStateForTest().selection.surfaces;
            const std::size_t undoBefore = window.undoDepthForTest();
            const std::size_t redoBefore = window.redoDepthForTest();
            const bool staged = stageSurfaceLines(window, lines, false, bounded);
            const QCborMap beforeEditor = captureEditor(window);
            if ( staged )
            {
                window.glViewForTest()->requestApplyStagedCleave();
            }
            if ( !subThreshold )
            {
                check(staged && assets::saveToSedBytes(window.documentStateForTest().level) != beforeLevel
                        && window.undoDepthForTest() == undoBefore + 1
                        && !window.glViewForTest()->hasStagedCleaveDraft(),
                    offset
                        ? "Surface roundoff Multi-cut preserves a genuinely crossing small parallel offset"
                        : "Surface roundoff Multi-cut preserves a genuinely crossing small shared-endpoint tilt");
                continue;
            }
            check(staged && assets::saveToSedBytes(window.documentStateForTest().level) == beforeLevel
                    && window.documentStateForTest().selection.surfaces == beforeSelection
                    && draft(captureEditor(window)) == draft(beforeEditor)
                    && window.undoDepthForTest() == undoBefore && window.redoDepthForTest() == redoBefore,
                offset
                    ? "Surface roundoff Multi-cut rejects a distinct nonzero offset atomically despite the app's no-crossing threshold"
                    : "Surface roundoff Multi-cut rejects a meaningful shared-endpoint tilt atomically despite the app's no-crossing threshold");
        }
    }
}

} // namespace sed::app_tests
