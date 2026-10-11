#include "test_common.hpp"

#include "gl_view.hpp"
#include "main_window/main_window.hpp"
#include "sed/assets/3do.hpp"

#include <QAction>
#include <QApplication>
#include <QCoreApplication>
#include <QDialog>
#include <QEventLoop>

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

namespace sed::app_tests
{
namespace
{

std::vector<std::byte> modelFixtureBytes()
{
    constexpr std::string_view model =
        "SECTION: HEADER\n"
        "3DO 2.3\n"
        "SECTION: GEOMETRYDEF\n"
        "RADIUS 1\n"
        "INSERT OFFSET 0 0 0\n"
        "GEOSETS 1\n"
        "GEOSET 0\n"
        "MESHES 1\n"
        "MESH 0\n"
        "NAME arch_invalidation\n"
        "VERTICES 2\n"
        "0: 0 0 0\n"
        "1: 1 0 0\n"
        "FACES 0\n";
    std::vector<std::byte> bytes{};
    bytes.reserve(model.size());
    for (const unsigned char character : model)
    {
        bytes.push_back(static_cast<std::byte>(character));
    }
    return bytes;
}

std::shared_ptr<const assets::Model3do> makeInteractiveModel()
{
    auto model = std::make_shared<assets::Model3do>();
    model->version = 2.3;
    assets::Model3doMesh mesh{};
    mesh.name = "arch_invalidation";
    mesh.geo = static_cast<std::int32_t>(TGeoMode::Solid);
    mesh.light = static_cast<std::int32_t>(TLightMode::VertexDiffuseOnly);
    mesh.vertices = {
        assets::Model3doVertex{ 0.0, 0.0, 0.0, ColorWhite },
        assets::Model3doVertex{ 1.0, 0.0, 0.0, ColorWhite } };
    model->meshes.push_back(std::move(mesh));
    return model;
}

Level makeLiveLevel(const std::int32_t layer)
{
    Level level{};
    level.kind = ProjectType::MOTS;
    level.header.version = 1;
    level.layers = { "Layer0", "Layer1" };

    Thing thing{};
    thing.name = "arch_invalidation";
    thing.layer = layer;
    thing.archLightingIndex = 0;

    TplValue flags{};
    flags.name = "thingflags";
    flags.type = ValueType::integer;
    flags.intValue = 0x00000008;
    thing.values.push_back(std::move(flags));

    TplValue model{};
    model.name = "model3d";
    model.type = ValueType::str;
    model.stringValue = "arch_invalidation.3do";
    thing.values.push_back(std::move(model));
    level.things.push_back(std::move(thing));

    MotsArchLightMesh mesh{};
    mesh.wireIndex = 0;
    mesh.vertices = {
        MotsArchLightVertex{ 0.25f, 0.5f, 0.75f, 1.0f },
        MotsArchLightVertex{ 0.5f, 1.0f, 1.5f, 2.0f } };
    MotsArchLightObject object{};
    object.wireIndex = 0;
    object.meshes.push_back(std::move(mesh));
    level.motsArchLighting.push_back(std::move(object));
    return level;
}

void prepareWindow(MainWindow& window, const std::int32_t layer)
{
    window.setAttribute(Qt::WA_DontShowOnScreen);
    window.setPrimaryResourceOverrideForTest(
        "arch_invalidation.3do",
        []()
        {
            return std::optional<std::vector<std::byte>>(modelFixtureBytes());
        });
    editor::DocumentState state{};
    state.level = makeLiveLevel(layer);
    window.setDocumentStateForTest(std::move(state));
    window.setProjectTypeForTest(ProjectType::MOTS);
    window.activateArchitecturalLightingEditModeForTest();

    GlView* const view = window.glViewForTest();
    if (!view)
    {
        return;
    }
    view->setRenderPipeline(GlView::RenderPipeline::Editor);
    view->setShowTextures(false);
    view->primeModel3do("arch_invalidation.3do", makeInteractiveModel());
    view->setLevel(&window.documentStateForTest().level);
    view->setArchitecturalLightingEditingActive(true);
    view->setSelectedThing(0);
    editor::Selection& selection = window.mutableDocumentStateForTest().selection;
    selection.things.insert(0);
    view->setSelectionState(-1, -1, 0, -1, selection);
    (void)window.refreshArchitecturalLightingOwnerForTest(0);
}

std::optional<GlView::ArchitecturalLightingVertexIdentity> firstVertex(
    GlView* const view)
{
    if (!view)
    {
        return std::nullopt;
    }
    const auto vertices = view->architecturalLightingVerticesForThing(0);
    if (vertices.empty())
    {
        return std::nullopt;
    }
    return GlView::ArchitecturalLightingVertexIdentity{
        vertices.front().thingIndex,
        vertices.front().lodIndex,
        vertices.front().meshIndex,
        vertices.front().vertexIndex };
}

void selectFirstVertex(MainWindow& window)
{
    GlView* const view = window.glViewForTest();
    const auto identity = firstVertex(view);
    if (!view || !identity)
    {
        return;
    }
    const std::array identities{ *identity };
    view->setArchitecturalLightingSelectedVertices(identities);
    window.updateEditActionsForTest();
}

void runResourceReloadInvalidationTests()
{
    MainWindow window{};
    prepareWindow(window, 0);
    GlView* const view = window.glViewForTest();
    selectFirstVertex(window);

    const bool selectedBeforeReload = view
        && view->architecturalLightingSelectedVertices().size() == 1;
    check(selectedBeforeReload,
        "ARCHLIGHTING resource invalidation fixture selects a vertex");
    if (!view)
    {
        return;
    }

    view->reloadCachedResources();
    check(view->architecturalLightingSelectedVertices().empty(),
        "ARCHLIGHTING resource cache reload clears stale vertex identities");
}

void runOwnerGizmoBindingInvalidationTests()
{
    MainWindow window{};
    prepareWindow(window, 0);
    window.resize(640, 480);
    window.show();
    QApplication::processEvents(QEventLoop::AllEvents);

    GlView* const view = window.glViewForTest();
    if (!view)
    {
        check(false,
            "ARCHLIGHTING owner-gizmo binding invalidation fixture exposes the viewport");
        return;
    }

    view->setShowThingGizmos(false);
    view->update();
    QApplication::processEvents(QEventLoop::AllEvents);
    check(view->architecturalLightingOwnerGizmoEntryCountForTest() >= 1,
        "ARCHLIGHTING bound owner publishes an owner icon in active ARCH mode");

    Level& live = window.mutableDocumentStateForTest().level;
    live.things[0].archLightingIndex = -1;
    view->notifyThingAppearanceChanged();
    view->update();
    QApplication::processEvents(QEventLoop::AllEvents);
    check(view->architecturalLightingOwnerGizmoEntryCountForTest() >= 1,
        "ARCHLIGHTING resetting a binding keeps the effective LevelGeo owner icon after appearance notification");

    live.things[0].archLightingIndex = 0;
    view->notifyThingAppearanceChanged();
    view->update();
    QApplication::processEvents(QEventLoop::AllEvents);
    check(view->architecturalLightingOwnerGizmoEntryCountForTest() >= 1,
        "ARCHLIGHTING rebinding the owner restores its owner icon after appearance notification");
}

void runLayerStateInvalidationTests()
{
    {
        MainWindow window{};
        prepareWindow(window, 1);
        GlView* const view = window.glViewForTest();
        selectFirstVertex(window);
        if (view)
        {
            // Keep owner markers alive while the exact vertex selection is
            // empty. A layer-lock refresh used to leave this point payload
            // and its VBO visible after the owner cache was pruned.
            view->clearArchitecturalLightingSelectedVertices();
        }
        check(view
                && view->architecturalLightingSelectedVertices().empty()
                && !view->architecturalLightingVerticesForThing(0).empty()
                && view->architecturalLightingPointCountForTest() > 0,
            "ARCHLIGHTING visible owner keeps its marker cache with no selected vertices");

        if (view)
        {
            view->setLockedLayers({ 1 });
        }
        check(view
                && view->architecturalLightingSelectedVertices().empty()
                && view->architecturalLightingVerticesForThing(0).empty()
                && view->architecturalLightingPointCountForTest() == 0,
            "ARCHLIGHTING locking an owner clears its marker cache and point payload after vertex selection is empty");

        if (view)
        {
            view->setLockedLayers({});
        }
        check(view
                && view->architecturalLightingSelectedVertices().empty()
                && !view->architecturalLightingVerticesForThing(0).empty()
                && view->architecturalLightingPointCountForTest() > 0,
            "ARCHLIGHTING unlocking an owner restores its marker cache and point payload with no selected vertices");
    }

    {
        MainWindow window{};
        prepareWindow(window, 1);
        GlView* const view = window.glViewForTest();
        selectFirstVertex(window);
        window.setLayerVisualStateForTest(
            { false, true },
            { false, false });
        check(view
                && view->architecturalLightingSelectedVertices().size() == 1
                && !view->architecturalLightingVerticesForThing(0).empty(),
            "ARCHLIGHTING unrelated hidden layer preserves a valid visible-owner vertex selection");
        window.setLayerVisualStateForTest(
            { false, true },
            { true, false });
        check(view
                && view->architecturalLightingSelectedVertices().size() == 1
                && !view->architecturalLightingVerticesForThing(0).empty(),
            "ARCHLIGHTING unrelated locked layer preserves a valid visible-owner vertex selection");
    }

    for (const bool lockLayer : { false, true })
    {
        MainWindow window{};
        prepareWindow(window, 1);
        GlView* const view = window.glViewForTest();
        selectFirstVertex(window);
        window.openArchitecturalLightingVertexEditorForTest(0, 0, 0, 0);
        QApplication::processEvents(QEventLoop::AllEvents);

        const bool dialogOpen = view
            && window.architecturalLightingVertexDialogForTest()
            && window.architecturalLightingSelectedVertexCountForTest() == 1;
        check(dialogOpen,
            lockLayer
                ? "ARCHLIGHTING lock invalidation fixture opens a staged vertex popup"
                : "ARCHLIGHTING visibility invalidation fixture opens a staged vertex popup");

        window.setLayerVisualStateForTest(
            { true, lockLayer },
            { false, lockLayer });
        QApplication::processEvents(QEventLoop::AllEvents);

        const bool canceled = !window.architecturalLightingVertexDialogForTest()
            && window.architecturalLightingSelectedVertexCountForTest() == 0
            && (!view
                || (view->architecturalLightingSelectedVertices().empty()
                    && view->architecturalLightingVerticesForThing(0).empty()));
        check(canceled,
            lockLayer
                ? "ARCHLIGHTING layer lock cancels the staged vertex popup and clears its viewport selection"
                : "ARCHLIGHTING layer hide cancels the staged vertex popup and clears its viewport selection");
    }

    QObject signalProbe{};
    int teardownSelectionNotifications = 0;
    {
        MainWindow window{};
        prepareWindow(window, 0);
        GlView* const view = window.glViewForTest();
        selectFirstVertex(window);
        if (view)
        {
            QObject::connect(
                view,
                &GlView::architecturalLightingVertexSelectionChanged,
                &signalProbe,
                [&teardownSelectionNotifications]()
                {
                    ++teardownSelectionNotifications;
                });
        }
    }
    check(teardownSelectionNotifications == 0,
        "ARCHLIGHTING GlView teardown suppresses selection notifications after MainWindow member teardown");
}

} // namespace

void runMotsArchLightingInvalidationTests()
{
    runResourceReloadInvalidationTests();
    runOwnerGizmoBindingInvalidationTests();
    runLayerStateInvalidationTests();
}

} // namespace sed::app_tests
