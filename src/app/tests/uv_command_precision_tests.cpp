#include "test_common.hpp"
#include "main_window/main_window.hpp"
#include "gl_view.hpp"
#include "sed/assets/sed.hpp"
#include "sed/level/geometry.hpp"
#include <algorithm>
#include <QAction>
#include <QApplication>
#include <QCborArray>
#include <QCborMap>
#include <QCborValue>
#include <QMessageBox>
#include <QTimer>
#include <limits>
#include <map>
#include <bit>
#include <array>
#include <cmath>
#include <cstdio>

namespace sed::app_tests
{
namespace
{
void selectUvSurfaces(MainWindow& window, editor::DocumentState state)
{
    window.setDocumentStateForTest(std::move(state));
    window.activateSurfaceEditModeForTest();
    window.mutableDocumentStateForTest().selection.surfaces = {{0,0},{0,1}};
    window.glViewForTest()->setSelectedSurface(0,0);
    window.updateEditActionsForTest();
}
bool triggerUvCommand(MainWindow& window, const char* name)
{
    auto* action = window.findChild<QAction*>(QString::fromLatin1(name));
    check(action && action->isEnabled(), "UV precision fixture exposes enabled command");
    if (!action || !action->isEnabled()) return false;
    QTimer dismiss;
    dismiss.setInterval(5);
    QObject::connect(&dismiss, &QTimer::timeout, []
    {
        if (auto* message = qobject_cast<QMessageBox*>(QApplication::activeModalWidget()))
            message->accept();
    });
    dismiss.start();
    action->trigger();
    return true;
}
}
void runUvCommandPrecisionTests()
{
    {
        MainWindow window;
        window.setAttribute(Qt::WA_DontShowOnScreen);
        editor::DocumentState state;
        state.level.layers = {"Layer0"};
        state.level.uvCoordinateSpace = UvCoordinateSpace::Normalized;
        auto& sector = state.level.sectors.emplace_back();
        sector.layer = 0;
        // Degenerate source geometry intentionally selects the documented
        // equal-corner-count copy fallback, independently of Stitch's solver.
        sector.vertices = {{0,0,0,0},{1,0,0,0},{2,0,0,0},
            {0,2,0,0},{1,2,0,0},{0,3,0,0}};
        Surface source;
        source.vertices = {0,1,2}; source.normal = {0,0,1};
        source.uScale = source.vScale = std::nextafter(1.0f, 2.0f);
        source.texVertices.assign(3, TexVertex{std::nextafter(1.0f,2.0f), std::nextafter(1.0f,2.0f), ColorWhite});
        Surface target = source;
        target.vertices = {3,4,5}; target.uScale = target.vScale = 1.0f;
        target.texVertices.assign(3, TexVertex{1.0f,1.0f,ColorWhite});
        for (std::size_t i = 0; i < target.texVertices.size(); ++i)
            target.texVertices[i].color = ColorF{float(i) * 0.25f, 0.5f, 0.75f, float(i + 1) * 0.2f};
        sector.surfaces = {source,target};
        selectUvSurfaces(window, std::move(state));
        const auto depth = window.undoDepthForTest();
        const auto before = assets::saveToSedBytes(window.documentStateForTest().level);
        if (triggerUvCommand(window, "surface.uv.copy_from_reference"))
        {
            const auto& copied = window.documentStateForTest().level.sectors[0].surfaces[1];
            bool exact = copied.uScale == source.uScale && copied.vScale == source.vScale;
            for (std::size_t i = 0; i < copied.texVertices.size(); ++i)
                exact = exact && copied.texVertices[i].u == source.texVertices[0].u
                    && copied.texVertices[i].v == source.texVertices[0].v
                    && copied.texVertices[i].color == target.texVertices[i].color;
            check(exact && window.undoDepthForTest() == depth + 1,
                "Copy Texture/UV fallback retains next-representable authored UVs and scales as one edit");
            if (!exact || window.undoDepthForTest() != depth + 1)
                std::fprintf(stderr, "DIAG Copy exact=%d depth=%zu/%zu scale=%.9g expected=%.9g uv=%.9g/%.9g expected=%.9g colors=%d\n", exact, window.undoDepthForTest(), depth, copied.uScale, source.uScale, copied.texVertices[0].u, copied.texVertices[0].v, source.texVertices[0].u, copied.texVertices[0].color == target.texVertices[0].color);
            const auto after = assets::saveToSedBytes(window.documentStateForTest().level);
            triggerUvCommand(window, "edit.undo");
            const auto& restored = window.documentStateForTest().level.sectors[0].surfaces[1];
            check(restored.uScale == 1.0f && restored.vScale == 1.0f && restored.texVertices[0].u == 1.0f,
                "Copy Texture/UV precise fallback Undo restores prior authored values");
            check(assets::saveToSedBytes(window.documentStateForTest().level) == before,
                "Copy Texture/UV precise fallback Undo restores exact native document bytes");
            triggerUvCommand(window, "edit.redo");
            check(assets::saveToSedBytes(window.documentStateForTest().level) == after,
                "Copy Texture/UV precise fallback Redo restores exact UVs and target colors");
        }
    }
    for (double origin : {0.0, 1000000.0})
    for (double gap : {0.0, 1e-6})
    for (int portal : {0, 1, 2})
    {
        MainWindow window;
        window.setAttribute(Qt::WA_DontShowOnScreen);
        editor::DocumentState state;
        state.level.layers = {"Layer0"}; state.level.ppunit = 1.0;
        state.level.uvCoordinateSpace = UvCoordinateSpace::Normalized;
        auto& sector = state.level.sectors.emplace_back(); sector.layer = 0;
        sector.vertices = {{origin,origin,0,0},{origin+1,origin,0,0},
            {origin+1,origin+1,0,0},{origin,origin+1,0,0},
            {origin,origin+1+gap,0,0},{origin+1,origin+1+gap,0,0},
            {origin+1,origin+2+gap,0,0},{origin,origin+2+gap,0,0}};
        Surface source; source.vertices={0,1,2,3}; source.normal={0,0,1};
        source.texVertices={{0,0,ColorWhite},{1,0,ColorWhite},{1,1,ColorWhite},{0,1,ColorWhite}};
        Surface target=source; target.vertices={4,5,6,7};
        target.texVertices.assign(4, TexVertex{7,8,ColorWhite});
        if (portal >= 1) source.adjoin = SurfaceRef{0, 1};
        if (portal >= 2) target.adjoin = SurfaceRef{0, 0};
        sector.surfaces={source,target};
        selectUvSurfaces(window,std::move(state));
        const auto depth=window.undoDepthForTest();
        if (!triggerUvCommand(window,"surface.uv.stitch_connected")) continue;
        const auto& output=window.documentStateForTest().level.sectors[0].surfaces[1];
        if (gap == 0.0)
            check(window.undoDepthForTest()==depth+1 && output.texVertices[0].u==0.0f
                    && output.texVertices[0].v==1.0f && output.texVertices[1].u==1.0f
                    && output.texVertices[1].v==1.0f,
                "Connected Stitch preserves the real shared-edge control after scene translation");
        else
        {
            bool unchanged=window.undoDepthForTest()==depth;
            for (const auto& uv:output.texVertices) unchanged=unchanged && uv.u==7.0f && uv.v==8.0f;
            check(unchanged,"Connected Stitch does not cross a real micron gap or publish history");
        }
    }
    // Two neighboring closed convex sectors share a real mirrored portal.
    // Split every portal edge on the second owner and its adjacent wall, so
    // no identical whole edge exists between the four- and eight-corner faces.
    for (double origin : {0.0, 1000000.0})
    for (double gap : {0.0, 1.0e-6})
    {
        MainWindow window;
        window.setAttribute(Qt::WA_DontShowOnScreen);
        editor::DocumentState state;
        state.level.layers = {"Layer0"}; state.level.ppunit = 1.0;
        state.level.uvCoordinateSpace = UvCoordinateSpace::Normalized;
        const auto addCube = [&](const double lowerZ, const double upperZ)
        {
            Sector& sector = state.level.newSector();
            sector.layer = 0;
            sector.vertices = {
                {origin,origin,lowerZ,0},{origin+1,origin,lowerZ,0},
                {origin+1,origin+1,lowerZ,0},{origin,origin+1,lowerZ,0},
                {origin,origin,upperZ,0},{origin+1,origin,upperZ,0},
                {origin+1,origin+1,upperZ,0},{origin,origin+1,upperZ,0}};
            for (const auto& loop : std::array<std::array<std::int32_t,4>,6>{{
                    {1,5,6,2},{0,3,7,4},{3,2,6,7},
                    {0,4,5,1},{4,7,6,5},{0,1,2,3}}})
            {
                Surface face{};
                face.vertices.assign(loop.begin(), loop.end());
                face.texVertices.resize(loop.size());
                face.normal = level::surfaceCalcNormalNewell(sector, face);
                sector.addSurface(std::move(face));
            }
        };
        addCube(origin - 1.0, origin);
        addCube(origin + gap, origin + 1.0 + gap);
        Sector& sourceSector = state.level.sectors[0];
        Sector& targetSector = state.level.sectors[1];
        // Midpoint ids 8..11 correspond to ordered lower edges 0-1,1-2,2-3,3-0.
        for (int edge = 0; edge < 4; ++edge)
        {
            const Vertex a = targetSector.vertices[static_cast<std::size_t>(edge)];
            const Vertex b = targetSector.vertices[static_cast<std::size_t>((edge + 1) % 4)];
            targetSector.newVertex(a.x + (b.x - a.x) * 0.5,
                a.y + (b.y - a.y) * 0.5, a.z + (b.z - a.z) * 0.5);
        }
        for (Surface& face : targetSector.surfaces)
        {
            const auto oldLoop = face.vertices;
            face.vertices.clear();
            for (std::size_t edge = 0; edge < oldLoop.size(); ++edge)
            {
                const auto a = oldLoop[edge];
                const auto b = oldLoop[(edge + 1) % oldLoop.size()];
                face.vertices.push_back(a);
                if (a < 4 && b < 4)
                {
                    const auto lowerEdge = (a + 1) % 4 == b ? a : b;
                    face.vertices.push_back(8 + lowerEdge);
                }
            }
            face.texVertices.resize(face.vertices.size());
            face.normal = level::surfaceCalcNormalNewell(targetSector, face);
        }
        Surface& source = sourceSector.surfaces[4];
        Surface& target = targetSector.surfaces[5];
        for (std::size_t corner = 0; corner < source.vertices.size(); ++corner)
        {
            const Vertex& vertex = sourceSector.vertices[static_cast<std::size_t>(source.vertices[corner])];
            source.texVertices[corner] = {static_cast<float>(vertex.x - origin),
                static_cast<float>(vertex.y - origin), ColorWhite};
        }
        target.texVertices.assign(8, TexVertex{7,8,ColorWhite});
        source.adjoin = SurfaceRef{1,5}; target.adjoin = SurfaceRef{0,4};
        const auto sourceUv = source.texVertices;
        for (const Sector& sector : state.level.sectors)
        {
            // Independent directed-edge incidence oracle: the source and the
            // split portal owner each remain complete closed manifold shells.
            std::map<std::pair<std::int32_t,std::int32_t>, std::pair<int,int>> incidence;
            for (const Surface& face : sector.surfaces)
            for (std::size_t edge = 0; edge < face.vertices.size(); ++edge)
            {
                const auto a = face.vertices[edge];
                const auto b = face.vertices[(edge + 1) % face.vertices.size()];
                auto& count = incidence[{std::min(a,b), std::max(a,b)}];
                ++count.first; count.second += a < b ? 1 : -1;
            }
            bool closed = sector.surfaces.size() == 6;
            for (const auto& [edge, count] : incidence)
                closed = closed && count.first == 2 && count.second == 0;
            check(closed && level::sectorIsConvex(sector),
                "Connected Stitch split-portal fixture owns closed convex sectors with matched wall subdivisions");
        }
        check(level::surfaceDoOverlap(sourceSector, source, targetSector, target) == (gap == 0.0),
            "Connected Stitch fixture distinguishes a real four/eight-corner portal from a separated boundary");
        window.setDocumentStateForTest(std::move(state));
        window.activateSurfaceEditModeForTest();
        window.mutableDocumentStateForTest().selection.surfaces = {{0,4},{1,5}};
        window.updateEditActionsForTest();
        const auto before = assets::saveToSedBytes(window.documentStateForTest().level);
        const auto depth = window.undoDepthForTest();
        if (!triggerUvCommand(window, "surface.uv.stitch_connected")) continue;
        const auto& result = window.documentStateForTest().level;
        const auto& output = result.sectors[1].surfaces[5];
        const auto& retained = result.sectors[0].surfaces[4];
        bool sourceUnchanged = retained.texVertices.size() == sourceUv.size();
        for (std::size_t corner = 0; corner < sourceUv.size(); ++corner)
            sourceUnchanged = sourceUnchanged && retained.texVertices[corner].u == sourceUv[corner].u
                && retained.texVertices[corner].v == sourceUv[corner].v
                && retained.texVertices[corner].color == sourceUv[corner].color;
        check(sourceUnchanged, "Connected Stitch preserves every source portal UV and lighting tuple");
        if (gap == 0.0)
        {
            bool exact = window.undoDepthForTest() == depth + 1;
            for (std::size_t corner = 0; corner < output.vertices.size(); ++corner)
            {
                const Vertex& vertex = result.sectors[1].vertices[static_cast<std::size_t>(output.vertices[corner])];
                exact = exact && output.texVertices[corner].u == static_cast<float>(vertex.x - origin)
                    && output.texVertices[corner].v == static_cast<float>(vertex.y - origin);
                if (output.texVertices[corner].u != static_cast<float>(vertex.x - origin)
                    || output.texVertices[corner].v != static_cast<float>(vertex.y - origin))
                    std::fprintf(stderr, "DIAG portal origin=%.0f corner=%zu actual=%.9g,%.9g expected=%.9g,%.9g scale=%.9g,%.9g depth=%zu/%zu\n", origin, corner, output.texVertices[corner].u, output.texVertices[corner].v, vertex.x-origin, vertex.y-origin, output.uScale, output.vScale, window.undoDepthForTest(), depth);
            }
            check(exact, "Connected Stitch preserves analytic U and V across a complete subdivided mirrored portal");
            const auto after = assets::saveToSedBytes(result);
            check(window.undoDocumentForTest() && assets::saveToSedBytes(window.documentStateForTest().level) == before,
                "Connected Stitch split-portal Undo restores exact native document bytes");
            check(window.redoDocumentForTest() && assets::saveToSedBytes(window.documentStateForTest().level) == after,
                "Connected Stitch split-portal Redo restores exact native document bytes");
        }
        else
            check(window.undoDepthForTest() == depth
                    && assets::saveToSedBytes(result) == before,
                "Connected Stitch rejects separated closed portal owners without any authored mutation or history");
    }
    for (int malformed = 0; malformed < 9; ++malformed)
    {
        MainWindow window;
        window.setAttribute(Qt::WA_DontShowOnScreen);
        editor::DocumentState state;
        state.level.layers = {"Layer0"};
        state.level.uvCoordinateSpace = UvCoordinateSpace::Normalized;
        auto& sector = state.level.sectors.emplace_back();
        sector.layer = 0;
        sector.vertices = {{0,0,0,0},{1,0,0,0},{2,0,0,0},
            {0,2,0,0},{1,2,0,0},{0,3,0,0}};
        Surface source;
        source.vertices = {0,1,2}; source.normal = {0,0,1};
        source.material = "reference.mat";
        source.texVertices.assign(3, TexVertex{2.0f, 3.0f, ColorWhite});
        Surface target = source;
        target.vertices = {3,4,5}; target.material = "target.mat";
        target.texVertices.assign(3, TexVertex{7.0f, 8.0f, ColorF{0.1f,0.2f,0.3f,0.4f}});
        sector.surfaces = {source, target};
        selectUvSurfaces(window, std::move(state));
        // Establish the active reference while its UV island is displayable.
        // A malformed UV payload is intentionally omitted from the canvas;
        // installing it before UI synchronization could select the healthy
        // target as reference and test a different, valid repair command.
        auto& malformedSector = window.mutableDocumentStateForTest().level.sectors[0];
        auto& malformedSource = malformedSector.surfaces[0];
        auto& malformedTarget = malformedSector.surfaces[1];
        if (malformed == 0) malformedSource.texVertices[1].u = std::numeric_limits<float>::quiet_NaN();
        if (malformed == 1) malformedSource.texVertices[1].v = std::numeric_limits<float>::infinity();
        if (malformed == 2) malformedSource.uScale = std::numeric_limits<float>::quiet_NaN();
        if (malformed == 3) malformedSource.texVertices.pop_back();
        if (malformed == 4) malformedTarget.texVertices.pop_back();
        if (malformed == 5) malformedSource.vertices[1] = 99;
        if (malformed == 6) malformedTarget.vertices[1] = -1;
        if (malformed == 7) malformedSector.vertices[1].x = std::numeric_limits<double>::quiet_NaN();
        if (malformed == 8) malformedSector.vertices[4].y = std::numeric_limits<double>::infinity();
        const auto activeReference = window.glViewForTest()->selectedSurfaceRef();
        const auto panelReference = QCborValue::fromCbor(window.captureUvEditorWorkspaceStateForTest())
            .toMap().value(QStringLiteral("active")).toArray();
        check(activeReference && activeReference->sector == 0 && activeReference->surface == 0,
            "Copy malformed-source fixture retains its explicit reference immediately before command");
        check(panelReference.isEmpty() || panelReference.at(0).toInteger() < 0
                || (panelReference.at(0).toInteger() == 0 && panelReference.at(1).toInteger() == 0),
            "Copy malformed-source fixture canvas reference agrees with the command source");
        const auto depth = window.undoDepthForTest();
        const auto before = window.documentStateForTest().level.sectors[0].surfaces;
        const auto beforeVertices = window.documentStateForTest().level.sectors[0].vertices;
        if (!triggerUvCommand(window, "surface.uv.copy_from_reference")) continue;
        const auto& after = window.documentStateForTest().level.sectors[0].surfaces;
        const auto sameFloatBits = [](float a, float b)
        { return std::bit_cast<std::uint32_t>(a) == std::bit_cast<std::uint32_t>(b); };
        bool exact = window.undoDepthForTest() == depth && after.size() == before.size();
        const auto& afterVertices = window.documentStateForTest().level.sectors[0].vertices;
        exact = exact && beforeVertices.size() == afterVertices.size();
        for (std::size_t vertex = 0; vertex < beforeVertices.size() && vertex < afterVertices.size(); ++vertex)
        {
            const auto& a = beforeVertices[vertex]; const auto& b = afterVertices[vertex];
            exact = exact && std::bit_cast<std::uint64_t>(a.x) == std::bit_cast<std::uint64_t>(b.x)
                && std::bit_cast<std::uint64_t>(a.y) == std::bit_cast<std::uint64_t>(b.y)
                && std::bit_cast<std::uint64_t>(a.z) == std::bit_cast<std::uint64_t>(b.z) && a.mark == b.mark;
        }
        for (std::size_t face = 0; face < before.size() && face < after.size(); ++face)
        {
            const auto& a = before[face]; const auto& b = after[face];
            exact = exact && a.material == b.material && a.nMat == b.nMat && a.tex == b.tex
                && a.faceFlags == b.faceFlags && a.vertices == b.vertices
                && sameFloatBits(a.uScale, b.uScale) && sameFloatBits(a.vScale, b.vScale)
                && a.texVertices.size() == b.texVertices.size();
            for (std::size_t corner = 0; corner < a.texVertices.size() && corner < b.texVertices.size(); ++corner)
                exact = exact && sameFloatBits(a.texVertices[corner].u, b.texVertices[corner].u)
                    && sameFloatBits(a.texVertices[corner].v, b.texVertices[corner].v)
                    && a.texVertices[corner].color == b.texVertices[corner].color;
        }
        check(exact, "Copy Texture/UV rejects malformed source or target atomically, including NaN bits and history");
        if (!exact) std::fprintf(stderr, "DIAG malformed=%d depth=%zu/%zu\n", malformed, window.undoDepthForTest(), depth);
    }
}
}
