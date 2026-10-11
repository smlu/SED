#include "test_common.hpp"

#include "app_settings.hpp"
#include "main_window/main_window.hpp"
#include "gl_view.hpp"
#include "sed/assets/3do.hpp"
#include "sed/level.hpp"
#include "sed/level/geometry.hpp"
#include "render/mesh/render_lighting_utils.hpp"

#include <QAction>
#include <QApplication>
#include <QEventLoop>
#include <QStatusBar>
#include <QTimer>
#include <QSettings>
#include <QToolButton>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace sed::app_tests
{
namespace
{

std::vector<std::byte> modelBytes(const std::string_view text)
{
    std::vector<std::byte> bytes{};
    bytes.reserve(text.size());
    for (const unsigned char character : text)
    {
        bytes.push_back(static_cast<std::byte>(character));
    }
    return bytes;
}

std::string modelFixture(const int geosetCount)
{
    std::string text =
        "SECTION: HEADER\n"
        "3DO 2.3\n"
        "SECTION: GEOMETRYDEF\n"
        // INSERT OFFSET is intentionally nonzero: the MotS render path does
        // not apply it when transforming ARCH model vertices.
        "INSERT OFFSET 100 200 300\n"
        "GEOSETS " + std::to_string(geosetCount) + "\n";
    for (int lod = 0; lod < geosetCount; ++lod)
    {
        text += "GEOSET " + std::to_string(lod) + "\n"
            "MESHES 1\n"
            "MESH 0\n"
            "NAME arch_bake_mesh\n"
            "VERTICES " + std::to_string(3 + lod) + "\n";
        for (int vertex = 0; vertex < 3 + lod; ++vertex)
        {
            text += std::to_string(vertex) + ": "
                + std::to_string(vertex) + " 0 0\n";
        }
        text += "FACES 0\n";
    }
    text +=
        "SECTION: HIERARCHYDEF\n"
        "HIERARCHY NODES 1\n"
        // The hierarchy adds (3,0,0) before the Thing transform.
        "0: 0x0 0x1 0 -1 -1 -1 0 3 0 0 0 0 0 0 0 0 arch_bake_mesh\n";
    return text;
}

std::shared_ptr<const assets::Model3do> makeBakedRenderModel()
{
    auto model = std::make_shared<assets::Model3do>();
    model->version = 2.3;
    assets::Model3doMesh mesh{};
    mesh.name = "arch_bake_mesh";
    mesh.geo = static_cast<std::int32_t>(TGeoMode::Solid);
    mesh.light = static_cast<std::int32_t>(TLightMode::VertexDiffuseOnly);
    mesh.vertices = {
        assets::Model3doVertex{ 0.0, 0.0, 0.0, ColorF{ 0.2f, 0.3f, 0.4f, 0.375f } },
        assets::Model3doVertex{ 1.0, 0.0, 0.0, ColorWhite },
        assets::Model3doVertex{ 0.0, 1.0, 0.0, ColorWhite },
    };
    assets::Model3doFace face{};
    face.geo = mesh.geo;
    face.light = mesh.light;
    face.extraLight = ColorBlack;
    face.vertices = { 0, 1, 2 };
    mesh.faces.push_back(std::move(face));
    model->meshes.push_back(std::move(mesh));
    model->meshSourceIndices.push_back(0);
    return model;
}

Sector makeClosedCubeSector()
{
    Sector sector{};
    sector.layer = 0;
    sector.vertices = {
        Vertex{ -10.0, -10.0, -10.0, 0 },
        Vertex{ 10.0, -10.0, -10.0, 0 },
        Vertex{ 10.0, 10.0, -10.0, 0 },
        Vertex{ -10.0, 10.0, -10.0, 0 },
        Vertex{ -10.0, -10.0, 10.0, 0 },
        Vertex{ 10.0, -10.0, 10.0, 0 },
        Vertex{ 10.0, 10.0, 10.0, 0 },
        Vertex{ -10.0, 10.0, 10.0, 0 },
    };
    constexpr std::array<std::array<std::int32_t, 4>, 6> faces{{
        {{ 1, 5, 6, 2 }},
        {{ 0, 3, 7, 4 }},
        {{ 3, 2, 6, 7 }},
        {{ 0, 4, 5, 1 }},
        {{ 4, 7, 6, 5 }},
        {{ 0, 1, 2, 3 }},
    }};
    for (const auto& face : faces)
    {
        Surface surface{};
        surface.vertices.assign(face.begin(), face.end());
        surface.texVertices.resize(face.size());
        for (TexVertex& vertex : surface.texVertices)
        {
            vertex.color = ColorBlack;
        }
        surface.normal = level::surfaceCalcNormalNewell(sector, surface);
        sector.surfaces.push_back(std::move(surface));
    }
    return sector;
}

Level makeLevel(const std::string_view modelName)
{
    Level level{};
    level.kind = ProjectType::MOTS;
    level.layers = { "Layer0" };

    Sector sector = makeClosedCubeSector();
    sector.id = 1;
    level.sectors.push_back(std::move(sector));

    Thing thing{};
    thing.id = 7001;
    thing.num = 0;
    thing.sector = 0;
    thing.name = "arch_bake_fixture";
    thing.templateName = "arch_bake_fixture";
    thing.layer = 0;
    thing.x = 0.0;
    thing.y = 0.0;
    thing.z = 0.0;
    thing.yaw = 90.0;
    thing.archLightingIndex = 0;

    TplValue flags{};
    flags.name = "thingflags";
    flags.type = ValueType::integer;
    flags.intValue = 0x8;
    thing.values.push_back(std::move(flags));

    TplValue model{};
    model.name = "model3d";
    model.type = ValueType::str;
    model.stringValue = std::string(modelName);
    thing.values.push_back(std::move(model));
    level.things.push_back(std::move(thing));

    MotsArchLightMesh mesh{};
    mesh.wireIndex = 0;
    mesh.vertices = {
        MotsArchLightVertex{ 0.125f, 0.25f, 0.375f, 0.5f },
        MotsArchLightVertex{ 0.0f, 0.0f, 0.0f, 0.0f },
        MotsArchLightVertex{ 0.0f, 0.0f, 0.0f, 0.0f },
    };
    MotsArchLightObject arch{};
    arch.wireIndex = 0;
    arch.meshes.push_back(std::move(mesh));
    level.motsArchLighting.push_back(std::move(arch));

    SedLight& light = level.lights.emplace_back();
    light.id = 8001;
    light.layer = 0;
    light.flags = LF_AffectArchThings;
    light.position = TVector{ 0.0, 0.0, 0.0 };
    light.range = 20.0;
    light.intensity = 2.0;
    light.color = ColorWhite;
    return level;
}

void prepareWindow(
    MainWindow& window,
    const std::string_view modelName,
    const std::optional<std::vector<std::byte>>& bytes)
{
    window.setAttribute(Qt::WA_DontShowOnScreen);
    editor::DocumentState state{};
    state.level = makeLevel(modelName);
    window.setDocumentStateForTest(std::move(state));
    window.setProjectTypeForTest(ProjectType::MOTS);
    if (!modelName.empty())
    {
        window.setPrimaryResourceOverrideForTest(
            modelName,
            [bytes]() -> std::optional<std::vector<std::byte>>
            {
                return bytes;
            });
    }
}

bool sameTexVertices(
    const std::vector<TexVertex>& lhs,
    const std::vector<TexVertex>& rhs)
{
    if (lhs.size() != rhs.size())
    {
        return false;
    }
    for (std::size_t index = 0; index < lhs.size(); ++index)
    {
        if (lhs[index].u != rhs[index].u
            || lhs[index].v != rhs[index].v
            || lhs[index].color != rhs[index].color)
        {
            return false;
        }
    }
    return true;
}

bool waitForBake(MainWindow& window, const int timeoutMs = 15000)
{
    if (!window.lightBakeRunningForTest())
    {
        return true;
    }

    QEventLoop loop{};
    QTimer poll{};
    poll.setInterval(5);
    QObject::connect(&poll, &QTimer::timeout, &loop, [&window, &loop]()
        {
            if (!window.lightBakeRunningForTest())
            {
                loop.quit();
            }
        });
    poll.start();
    QTimer::singleShot(timeoutMs, &loop, &QEventLoop::quit);
    loop.exec();
    return !window.lightBakeRunningForTest();
}

void runManualPublicationAndHistoryTest()
{
    MainWindow window{};
    const std::vector<std::byte> bytes = modelBytes(modelFixture(1));
    prepareWindow(window, "arch_bake.3do", bytes);
    GlView* const view = window.glViewForTest();
    std::optional<ColorF> initialRenderedColor{};
    std::uint64_t initialMeshRebuildCount{};
    const Level* const expectedViewLevel =
        &window.documentStateForTest().level;
    const auto renderModel = makeBakedRenderModel();
    if (view)
    {
        view->setShowTextures(false);
        view->setThingViewMode(GlView::ThingViewMode::Shaded);
        view->primeModel3do("arch_bake.3do", renderModel);
        view->setLevel(expectedViewLevel);
        initialMeshRebuildCount = view->meshRebuildCountForTest();
        initialRenderedColor = view->thingModelVertexColorForTest(0, 0, 0);
    }
    const Level& initialLevel = window.documentStateForTest().level;
    const MotsArchLightObject authored = initialLevel.motsArchLighting.front();
    check(initialLevel.sectors.size() == 1
            && level::levelFindSectorForXYZ(initialLevel, 0.0, 0.0, 0.0) == 0
            && level::levelFindSectorForThing(initialLevel, initialLevel.things.front()) == 0,
        "mots_arch_bake fixture places the Thing and source light inside a closed convex sector");

    window.queueManualLightBakeForTest(
        { 0 }, "Baked Lighting: MotS ARCH test", false);
    const bool completed = waitForBake(window);
    const Level& baked = window.documentStateForTest().level;
    const bool published = completed
        && baked.motsArchLighting.size() == 1
        && baked.motsArchLighting.front() != authored
        && baked.motsArchLightingBakeBaselines.size() == 1
        && baked.motsArchLightingBakeBaselines.front().thingId == 7001
        && baked.motsArchLightingBakeBaselines.front().modelName == "arch_bake.3do"
        && baked.motsArchLightingBakeBaselines.front().originalArchLightingIndex == 0
        && baked.motsArchLightingBakeBaselines.front().originalArchLighting
        && *baked.motsArchLightingBakeBaselines.front().originalArchLighting == authored
        && window.undoDepthForTest() == 1;
    check(published,
        "mots_arch_bake manual publication adds flagged-light output over an exact authored baseline");
    const std::optional<ColorF> renderedBakedColor = view
        ? view->thingModelVertexColorForTest(0, 0, 0)
        : std::nullopt;
    const MotsArchLightVertex renderedArchVertex = baked.motsArchLighting.empty()
        || baked.motsArchLighting.front().meshes.empty()
        || baked.motsArchLighting.front().meshes.front().vertices.empty()
        ? MotsArchLightVertex{}
        : baked.motsArchLighting.front().meshes.front().vertices.front();
    constexpr MotsArchLightVertex expectedArchVertex{
        1.57f, 1.695f, 1.82f, 1.945f,
    };
    constexpr ColorF expectedRenderedBakedColor{
        1.695f, 1.82f, 1.945f, 1.0f,
    };
    const ColorF sourceAlphaPreserved = render::motsArchLightingVertexColor(
        renderModel->meshes.front().vertices.front().color,
        renderedArchVertex,
        level::MotsArchLightingVertexMode::Rgb);
    check(renderedArchVertex == expectedArchVertex,
        "mots_arch_bake emits the fixed ARCH vertex vector for the analytic fixture");
    check(sourceAlphaPreserved.a == renderModel->meshes.front().vertices.front().color.a,
        "mots_arch_bake RGB replacement preserves source model alpha before face opacity");
    check(initialRenderedColor && renderedBakedColor
            && *renderedBakedColor == expectedRenderedBakedColor
            && *renderedBakedColor != *initialRenderedColor
            && view
            && view->attachedLevelForTest() == &baked
            && view->thingMeshCacheValidForTest()
            && view->meshRebuildCountForTest() > initialMeshRebuildCount,
        "mots_arch_bake publication refreshes the renderer's cached opaque 3DO vertex color from ARCH output");

    const std::optional<MotsArchLightObject> bakedValue = baked.motsArchLighting.empty()
        ? std::nullopt
        : std::optional<MotsArchLightObject>(baked.motsArchLighting.front());
    const auto bakedBaselines = baked.motsArchLightingBakeBaselines;
    const bool undid = window.undoDocumentForTest();
    const Level& undone = window.documentStateForTest().level;
    check(undid
            && undone.motsArchLighting.size() == 1
            && undone.motsArchLighting.front() == authored
            && undone.motsArchLightingBakeBaselines.empty(),
        "mots_arch_bake manual publication Undo restores exact ARCH values and removes provenance");

    const bool redid = window.redoDocumentForTest();
    const Level& redone = window.documentStateForTest().level;
    check(redid && bakedValue
            && redone.motsArchLighting.size() == 1
            && redone.motsArchLighting.front() == *bakedValue
            && redone.motsArchLightingBakeBaselines == bakedBaselines,
        "mots_arch_bake manual publication Redo restores generated ARCH values and provenance exactly");
}

bool waitForAutomaticBake(
    MainWindow& window,
    const MotsArchLightObject& authored,
    bool& sawDebounce,
    const int timeoutMs = 15000)
{
    QEventLoop loop{};
    QTimer poll{};
    poll.setInterval(5);
    bool finished = false;
    QObject::connect(&poll, &QTimer::timeout, &loop,
        [&window, &loop, &authored, &sawDebounce, &finished]()
        {
            sawDebounce = sawDebounce || window.lightBakeDebounceActiveForTest();
            const Level& current = window.documentStateForTest().level;
            const bool outputPublished = current.motsArchLighting.size() == 1
                && current.motsArchLighting.front() != authored
                && !current.motsArchLightingBakeBaselines.empty();
            if (sawDebounce && outputPublished
                && !window.lightBakeDebounceActiveForTest()
                && !window.lightBakeRunningForTest())
            {
                finished = true;
                loop.quit();
            }
        });
    poll.start();
    QTimer::singleShot(timeoutMs, &loop, &QEventLoop::quit);
    loop.exec();
    poll.stop();
    return finished;
}

void runAutomaticPublicationRenderTest()
{
    constexpr auto kAutoSetting = "lighting/auto_recalculate";
    QSettings settings = settingsStore();
    const bool hadAuto = settings.contains(kAutoSetting);
    const QVariant savedAuto = settings.value(kAutoSetting);
    settings.setValue(kAutoSetting, false);
    settings.sync();

    {
        MainWindow window{};
        const std::vector<std::byte> bytes = modelBytes(modelFixture(1));
        prepareWindow(window, "arch_bake.3do", bytes);
        GlView* const view = window.glViewForTest();
        const auto renderModel = makeBakedRenderModel();
        std::optional<ColorF> initialRenderedColor{};
        std::uint64_t initialMeshRebuildCount{};
        if (view)
        {
            view->setShowTextures(false);
            view->setThingViewMode(GlView::ThingViewMode::Shaded);
            view->primeModel3do("arch_bake.3do", renderModel);
            view->setLevel(&window.documentStateForTest().level);
            initialMeshRebuildCount = view->meshRebuildCountForTest();
            initialRenderedColor = view->thingModelVertexColorForTest(0, 0, 0);
        }

        const MotsArchLightObject authored =
            window.documentStateForTest().level.motsArchLighting.front();
        const float authoredIntensity = static_cast<float>(
            window.documentStateForTest().level.lights.front().intensity);
        QToolButton* const autoButton = window.findChild<QToolButton*>(
            QStringLiteral("lightBakeAutoButton"));
        check(view && initialRenderedColor && autoButton && autoButton->isCheckable(),
            "mots_arch_bake Auto fixture primes the 3DO cache and finds the Auto control");

        window.resetLightBakeSourceTrackingForTest();
        if (autoButton)
        {
            autoButton->setChecked(true);
        }
        check(autoButton && autoButton->isChecked(),
            "mots_arch_bake Auto fixture enables the production automatic control");
        window.resetLightBakeSourceTrackingForTest();

        window.pushUndoForTitleForTest(editor::DocumentDirty::Lights);
        window.mutableDocumentStateForTest().level.lights.front().intensity = 3.0;
        bool sawDebounce = false;
        const bool completed = waitForAutomaticBake(window, authored, sawDebounce);
        const Level& baked = window.documentStateForTest().level;
        const std::optional<ColorF> rendered = view
            ? view->thingModelVertexColorForTest(0, 0, 0)
            : std::nullopt;
        const MotsArchLightVertex bakedVertex = baked.motsArchLighting.empty()
                || baked.motsArchLighting.front().meshes.empty()
                || baked.motsArchLighting.front().meshes.front().vertices.empty()
            ? MotsArchLightVertex{}
            : baked.motsArchLighting.front().meshes.front().vertices.front();
        const std::optional<MotsArchLightObject> generatedArch =
            baked.motsArchLighting.empty()
                ? std::nullopt
                : std::optional<MotsArchLightObject>(baked.motsArchLighting.front());
        const ColorF expectedRendered{
            bakedVertex.red, bakedVertex.green, bakedVertex.blue, 1.0f };
        check(completed
                && sawDebounce
                && baked.motsArchLighting.size() == 1
                && baked.motsArchLighting.front() != authored
                && baked.motsArchLightingBakeBaselines.size() == 1
                && baked.motsArchLightingBakeBaselines.front().originalArchLighting
                && *baked.motsArchLightingBakeBaselines.front().originalArchLighting == authored
                && window.undoDepthForTest() == 1,
            "mots_arch_bake Auto source polling debounces and publishes ARCH over its authored baseline in the source edit history step");
        check(initialRenderedColor && rendered
                && *rendered == expectedRendered
                && *rendered != *initialRenderedColor
                && view
                && view->attachedLevelForTest() == &baked
                && view->thingMeshCacheValidForTest()
                && view->meshRebuildCountForTest() > initialMeshRebuildCount,
            "mots_arch_bake Auto debounce refreshes the shaded 3DO cache without opening a color picker");

        if (autoButton)
        {
            autoButton->setChecked(false);
        }
        window.updateEditActionsForTest();
        QAction* const undoAction = window.findChild<QAction*>(
            QStringLiteral("edit.undo"));
        if (undoAction)
        {
            undoAction->trigger();
        }
        const Level& undone = window.documentStateForTest().level;
        const std::optional<ColorF> renderedUndo = view
            ? view->thingModelVertexColorForTest(0, 0, 0)
            : std::nullopt;
        check(undoAction != nullptr
                && window.undoDepthForTest() == 0
                && window.redoDepthForTest() == 1,
            "mots_arch_bake Auto test invokes the production Undo action exactly once");
        check(undone.lights.size() == 1
                && undone.lights.front().intensity == authoredIntensity,
            "mots_arch_bake Auto Undo restores the original baked-light intensity");
        check(undone.motsArchLighting.size() == 1
                && undone.motsArchLighting.front() == authored
                && undone.motsArchLightingBakeBaselines.empty(),
            "mots_arch_bake Auto Undo restores authored ARCH and removes bake provenance");
        check(renderedUndo.has_value(),
            "mots_arch_bake Auto Undo leaves the model's rendered vertex available");
        check(initialRenderedColor.has_value()
                && renderedUndo == initialRenderedColor
                && view
                && view->attachedLevelForTest() == &undone
                && view->thingMeshCacheValidForTest()
                && view->meshRebuildCountForTest() > initialMeshRebuildCount,
            "mots_arch_bake Auto Undo refreshes the shaded 3DO cache to its original color");
        const std::uint64_t undoMeshRebuildCount = view
            ? view->meshRebuildCountForTest()
            : 0;
        window.updateEditActionsForTest();
        QAction* const redoAction = window.findChild<QAction*>(
            QStringLiteral("edit.redo"));
        if (redoAction)
        {
            redoAction->trigger();
        }
        const Level& redone = window.documentStateForTest().level;
        const std::optional<ColorF> renderedRedo = view
            ? view->thingModelVertexColorForTest(0, 0, 0)
            : std::nullopt;
        check(redoAction != nullptr
                && window.undoDepthForTest() == 1
                && window.redoDepthForTest() == 0,
            "mots_arch_bake Auto test invokes the production Redo action exactly once");
        check(redone.lights.size() == 1
                && redone.lights.front().intensity == 3.0,
            "mots_arch_bake Auto Redo restores the edited baked-light intensity");
        check(generatedArch
                && redone.motsArchLighting.size() == 1
                && redone.motsArchLighting.front() == *generatedArch
                && redone.motsArchLightingBakeBaselines.size() == 1
                && redone.motsArchLightingBakeBaselines.front().originalArchLighting
                && *redone.motsArchLightingBakeBaselines.front().originalArchLighting == authored,
            "mots_arch_bake Auto Redo restores generated ARCH and its authored baseline");
        check(renderedRedo.has_value(),
            "mots_arch_bake Auto Redo leaves the model's rendered vertex available");
        check(renderedRedo == rendered
                && renderedRedo != initialRenderedColor
                && view
                && view->attachedLevelForTest() == &redone
                && view->thingMeshCacheValidForTest()
                && view->meshRebuildCountForTest() > undoMeshRebuildCount,
            "mots_arch_bake Auto Redo refreshes the shaded 3DO cache to generated ARCH color");
        const std::uint64_t redoMeshRebuildCount = view
            ? view->meshRebuildCountForTest()
            : 0;
        window.jumpToUndoDepthForTest(0);
        const Level& timelineUndone = window.documentStateForTest().level;
        const std::optional<ColorF> timelineUndoColor = view
            ? view->thingModelVertexColorForTest(0, 0, 0)
            : std::nullopt;
        check(timelineUndone.motsArchLighting.size() == 1
                && timelineUndone.motsArchLighting.front() == authored
                && timelineUndone.motsArchLightingBakeBaselines.empty()
                && timelineUndoColor == initialRenderedColor
                && view
                && view->meshRebuildCountForTest() > redoMeshRebuildCount,
            "mots_arch_bake Auto history-timeline Undo restores authored ARCH and invalidates the Thing cache");
        const std::uint64_t timelineUndoRebuildCount = view
            ? view->meshRebuildCountForTest()
            : 0;
        window.jumpToUndoDepthForTest(1);
        const Level& timelineRedone = window.documentStateForTest().level;
        const std::optional<ColorF> timelineRedoColor = view
            ? view->thingModelVertexColorForTest(0, 0, 0)
            : std::nullopt;
        check(generatedArch
                && timelineRedone.motsArchLighting.size() == 1
                && timelineRedone.motsArchLighting.front() == *generatedArch
                && timelineRedoColor == rendered
                && view
                && view->meshRebuildCountForTest() > timelineUndoRebuildCount,
            "mots_arch_bake Auto history-timeline Redo restores generated ARCH and invalidates the Thing cache");
    }

    if (hadAuto)
    {
        settings.setValue(kAutoSetting, savedAuto);
    }
    else
    {
        settings.remove(kAutoSetting);
    }
    settings.sync();
}

void runStaleInputAtomicityTest(
    const char* const assertion,
    const std::function<void(MainWindow&)>& mutateAfterSnapshot)
{
    MainWindow window{};
    const std::vector<std::byte> bytes = modelBytes(modelFixture(1));
    prepareWindow(window, "arch_bake.3do", bytes);
    const Level before = window.documentStateForTest().level;
    const std::size_t undoBefore = window.undoDepthForTest();
    const std::size_t redoBefore = window.redoDepthForTest();
    window.queueManualLightBakeForTest(
        { 0 }, "Baked Lighting: MotS stale test", false);
    mutateAfterSnapshot(window);
    const bool completed = waitForBake(window);
    const Level& after = window.documentStateForTest().level;
    const QString message = window.statusBar()->currentMessage();
    check(completed
            && after.motsArchLighting == before.motsArchLighting
            && after.motsArchLightingBakeBaselines
                == before.motsArchLightingBakeBaselines
            && after.sectors.size() == before.sectors.size()
            && !after.sectors.empty()
            && after.sectors.front().surfaces.size()
                == before.sectors.front().surfaces.size()
            && sameTexVertices(
                after.sectors.front().surfaces.front().texVertices,
                before.sectors.front().surfaces.front().texVertices)
            && window.undoDepthForTest() == undoBefore
            && window.redoDepthForTest() == redoBefore
            && message == QStringLiteral(
                "Lighting changed while calculating; stale results were discarded."),
        assertion);
}

void runNoModelSkipTest()
{
    MainWindow window{};
    prepareWindow(window, {}, std::nullopt);
    const Level& before = window.documentStateForTest().level;
    const MotsArchLightObject authored = before.motsArchLighting.front();
    window.queueManualLightBakeForTest(
        { 0 }, "Baked Lighting: MotS model-less test", false);
    const bool completed = waitForBake(window);
    const Level& after = window.documentStateForTest().level;
    check(completed
            && after.motsArchLighting.size() == 1
            && after.motsArchLighting.front() == authored
            && after.motsArchLightingBakeBaselines.empty()
            && window.undoDepthForTest() == 1,
        "mots_arch_bake effective Arch Thing without a declared model is skipped without blocking or rewriting authored ARCH data");
}

void runResourceFailureAtomicityTest(
    const std::string_view resourceName,
    const std::optional<std::vector<std::byte>>& bytes,
    const char* const expectedMessage,
    const char* const assertion)
{
    MainWindow window{};
    prepareWindow(window, resourceName, bytes);
    const Level before = window.documentStateForTest().level;
    const std::size_t undoBefore = window.undoDepthForTest();
    const std::size_t redoBefore = window.redoDepthForTest();
    window.queueManualLightBakeForTest(
        { 0 }, "Baked Lighting: MotS ARCH rejected", false);
    const bool completed = waitForBake(window);
    const Level& after = window.documentStateForTest().level;
    const QString message = window.statusBar()->currentMessage();
    check(completed
            && after.motsArchLighting == before.motsArchLighting
            && after.motsArchLightingBakeBaselines
                == before.motsArchLightingBakeBaselines
            && after.things.size() == before.things.size()
            && after.things.front().id == before.things.front().id
            && after.things.front().sector == before.things.front().sector
            && after.things.front().archLightingIndex
                == before.things.front().archLightingIndex
            && after.sectors.size() == before.sectors.size()
            && !after.sectors.empty()
            && after.sectors.front().surfaces.size()
                == before.sectors.front().surfaces.size()
            && !after.sectors.front().surfaces.empty()
            && sameTexVertices(
                after.sectors.front().surfaces.front().texVertices,
                before.sectors.front().surfaces.front().texVertices)
            && window.undoDepthForTest() == undoBefore
            && window.redoDepthForTest() == redoBefore
            && message == QString::fromLatin1(expectedMessage),
        assertion);
}

} // namespace

void runMotsArchBakePipelineTests()
{
    runManualPublicationAndHistoryTest();
    runAutomaticPublicationRenderTest();
    runStaleInputAtomicityTest(
        "mots_arch_bake rejects a stale Thing ID before any output or history publication",
        [](MainWindow& window)
        {
            ++window.mutableDocumentStateForTest().level.things.front().id;
        });
    runStaleInputAtomicityTest(
        "mots_arch_bake rejects a stale Thing model before any output or history publication",
        [](MainWindow& window)
        {
            window.mutableDocumentStateForTest().level.things.front().values.back().stringValue =
                "changed_arch_model.3do";
        });
    runStaleInputAtomicityTest(
        "mots_arch_bake rejects a stale Thing sector before any output or history publication",
        [](MainWindow& window)
        {
            window.mutableDocumentStateForTest().level.things.front().sector = -1;
        });
    runStaleInputAtomicityTest(
        "mots_arch_bake rejects a changed 3DO resource snapshot before publication",
        [](MainWindow& window)
        {
            window.setPrimaryResourceOverrideForTest(
                "arch_bake.3do",
                []() -> std::optional<std::vector<std::byte>>
                {
                    return modelBytes(modelFixture(1));
                });
        });
    runNoModelSkipTest();
    runResourceFailureAtomicityTest(
        "missing_arch_model.3do",
        std::nullopt,
        "MotS ARCH bake rejected: Thing 'arch_bake_fixture' (ID 7001), model 'missing_arch_model.3do': resource not found.",
        "mots_arch_bake missing 3DO has the exact diagnostic and publishes no partial surface, ARCH, binding, or history output");
    runResourceFailureAtomicityTest(
        "malformed_arch_model.3do",
        modelBytes("SECTION: HEADER\n3DO 2.3\nSECTION: GEOMETRYDEF\nGEOSETS 0\n"),
        "MotS ARCH bake rejected: Thing 'arch_bake_fixture' (ID 7001), model 'malformed_arch_model.3do': invalid GEOSETS count 0 (expected 1..4).",
        "mots_arch_bake malformed GEOSETS has the exact diagnostic and rejects surface, ARCH, binding, and history output atomically");
}

} // namespace sed::app_tests
