#include "test_common.hpp"

#include "app_settings.hpp"
#include "sed/assets/3do.hpp"
#include "sed/assets/resource_locator.hpp"
#include "sed/editor/document.hpp"
#include "gl_view.hpp"
#include "main_window/main_window.hpp"
#include "sed/level.hpp"
#include "sed/level/thing.hpp"
#include "ui/components/scrub_float_edit.hpp"
#include "ui/components/live_color_popup.hpp"
#include "ui/lighting/mots_arch_lighting_dialog.hpp"

#include <QAction>
#include <QApplication>
#include <QCoreApplication>
#include <QComboBox>
#include <QCborArray>
#include <QCborMap>
#include <QCborValue>
#include <QDialog>
#include <QDialogButtonBox>
#include <QEventLoop>
#include <QFontMetrics>
#include <QKeyEvent>
#include <QLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMenuBar>
#include <QPointer>
#include <QPushButton>
#include <QSettings>
#include <QSlider>
#include <QStyle>
#include <QToolButton>
#include <QWidget>

#include "ui/recovery/document_recovery_utils.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace sed::app_tests
{

namespace
{

using ui::lighting::MotsArchLightingPlan;
using ui::lighting::MotsArchLightingRecordStage;
using ui::lighting::MotsArchLightingTarget;

MotsArchLightObject makeStagingRecord()
{
    MotsArchLightMesh mesh{};
    mesh.wireIndex = 0;
    mesh.vertices = {
        MotsArchLightVertex{ 0.25f, 0.5f, 0.75f, 1.0f },
        MotsArchLightVertex{ 0.5f, 1.0f, 1.5f, 2.0f } };

    MotsArchLightObject object{};
    object.wireIndex = 10;
    object.meshes.push_back(std::move(mesh));
    return object;
}

MotsArchLightingTarget makeStagingTarget(
    const std::int32_t thingIndex,
    const std::int32_t originalRecordIndex,
    const std::int32_t recordStageIndex,
    const bool modelAvailable,
    const bool valueEditingAllowed)
{
    MotsArchLightingTarget target{};
    target.thingIndex = thingIndex;
    target.label = "Thing " + std::to_string(thingIndex);
    target.modelName = "staging.3do";
    target.resourceRevision = "3DO|staging-v1";
    target.topology.geosets = { { 2u } };
    target.originalRecordIndex = originalRecordIndex;
    target.recordStageIndex = recordStageIndex;
    target.hasArchFlag = true;
    target.modelAvailable = modelAvailable;
    target.valueEditingAllowed = valueEditingAllowed;
    return target;
}

MotsArchLightingPlan makeSparsePlan()
{
    const MotsArchLightObject source = makeStagingRecord();
    MotsArchLightingRecordStage stage{};
    stage.originalRecordIndex = 0;
    stage.originalRecord = source;
    stage.stagedRecord = source;

    MotsArchLightingPlan plan{};
    plan.project = ProjectType::MOTS;
    plan.resourceLocatorIdentity = 41;
    plan.records.push_back(std::move(stage));
    plan.targets = {
        makeStagingTarget(303, 0, 0, true, true),
        makeStagingTarget(7, 0, 0, true, true),
        makeStagingTarget(446, -1, -1, true, true),
        makeStagingTarget(900, -2, -1, false, false),
    };
    return plan;
}

std::vector<std::byte> stagingModelFixtureBytes()
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
        "NAME arch_staging\n"
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

std::shared_ptr<const assets::Model3do> makeStagingInteractiveModel()
{
    auto model = std::make_shared<assets::Model3do>();
    model->version = 2.3;
    assets::Model3doMesh mesh{};
    mesh.name = "arch_staging";
    mesh.geo = static_cast<std::int32_t>(TGeoMode::Solid);
    mesh.light = static_cast<std::int32_t>(TLightMode::VertexDiffuseOnly);
    mesh.vertices = {
        assets::Model3doVertex{ 0.0, 0.0, 0.0, ColorWhite },
        assets::Model3doVertex{ 1.0, 0.0, 0.0, ColorWhite } };
    model->meshes.push_back(std::move(mesh));
    return model;
}

std::shared_ptr<const assets::Model3do> makeStagingTriangleInteractiveModel()
{
    auto model = std::make_shared<assets::Model3do>();
    model->version = 2.3;
    assets::Model3doMesh mesh{};
    mesh.name = "arch_staging";
    mesh.geo = static_cast<std::int32_t>(TGeoMode::Solid);
    mesh.light = static_cast<std::int32_t>(TLightMode::VertexDiffuseOnly);
    mesh.vertices = {
        assets::Model3doVertex{ -0.25, 0.0, 0.0, ColorWhite },
        assets::Model3doVertex{ 0.25, 0.0, 0.0, ColorWhite },
        assets::Model3doVertex{ 0.0, 0.25, 0.0, ColorWhite },
    };

    assets::Model3doFace face{};
    face.geo = static_cast<std::int32_t>(TGeoMode::Solid);
    face.light = static_cast<std::int32_t>(TLightMode::VertexDiffuseOnly);
    face.faceFlags |= FF_DoubleSided;
    face.vertices = { 0, 1, 2 };
    mesh.faces.push_back(std::move(face));
    model->meshes.push_back(std::move(mesh));
    return model;
}

Level makeStagingLiveLevel(const MotsArchLightVertex firstVertex)
{
    Level level{};
    level.kind = ProjectType::MOTS;
    level.header.version = 1;
    level.layers.emplace_back("Layer0");
    level.layers.emplace_back("Layer1");
    level.things.resize(1);
    Thing& thing = level.things.front();
    thing.name = "arch_staging";

    TplValue flags{};
    flags.name = "thingflags";
    flags.type = ValueType::integer;
    flags.intValue = 0x8;
    thing.values.push_back(std::move(flags));
    TplValue model3d{};
    model3d.name = "model3d";
    model3d.type = ValueType::str;
    model3d.stringValue = "arch_staging.3do";
    thing.values.push_back(std::move(model3d));
    thing.archLightingIndex = 0;

    MotsArchLightObject object = makeStagingRecord();
    object.meshes.front().vertices.front() = firstVertex;
    level.motsArchLighting.push_back(std::move(object));
    return level;
}

Level makeStagingMultiOwnerLiveLevel()
{
    Level level = makeStagingLiveLevel(
        MotsArchLightVertex{ 0.125f, 0.25f, 0.5f, 0.75f });
    level.things.resize(3, level.things.front());
    for (std::size_t index = 0; index < level.things.size(); ++index)
    {
        level.things[index].name = "arch_staging_" + std::to_string(index);
        level.things[index].archLightingIndex = 0;
    }
    level.motsArchLighting.front().meshes.front().vertices[1] =
        MotsArchLightVertex{ 0.875f, 0.75f, 0.5f, 0.25f };
    return level;
}

void prepareStagingPopupWindow(
    MainWindow& window,
    const MotsArchLightVertex firstVertex)
{
    window.setAttribute(Qt::WA_DontShowOnScreen);
    window.show();
    QApplication::processEvents(QEventLoop::AllEvents);
    window.setPrimaryResourceOverrideForTest(
        "arch_staging.3do",
        []()
        {
            return std::optional<std::vector<std::byte>>(
                stagingModelFixtureBytes());
        });
    editor::DocumentState state{};
    state.level = makeStagingLiveLevel(firstVertex);
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
    view->primeModel3do("arch_staging.3do", makeStagingInteractiveModel());
    view->setLevel(&window.documentStateForTest().level);
    view->setArchitecturalLightingEditingActive(true);
    view->setSelectedThing(0);
    editor::Selection& selection = window.mutableDocumentStateForTest().selection;
    selection.things.insert(0);
    view->setSelectionState(-1, -1, 0, -1, selection);
    (void)window.refreshArchitecturalLightingOwnerForTest(0);
}

void prepareStagingMultiOwnerPopupWindow(MainWindow& window)
{
    window.setAttribute(Qt::WA_DontShowOnScreen);
    window.setPrimaryResourceOverrideForTest(
        "arch_staging.3do",
        []()
        {
            return std::optional<std::vector<std::byte>>(
                stagingModelFixtureBytes());
        });
    editor::DocumentState state{};
    state.level = makeStagingMultiOwnerLiveLevel();
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
    view->primeModel3do("arch_staging.3do", makeStagingInteractiveModel());
    view->setLevel(&window.documentStateForTest().level);
    view->setArchitecturalLightingEditingActive(true);
    view->setSelectedThing(0);
    editor::Selection& selection = window.mutableDocumentStateForTest().selection;
    selection.things.insert(0);
    selection.things.insert(1);
    view->setSelectionState(-1, -1, 0, -1, selection);
    (void)window.refreshArchitecturalLightingOwnerForTest(0);
}

void prepareStagingClipboardWindow(MainWindow& window)
{
    window.setAttribute(Qt::WA_DontShowOnScreen);
    window.setPrimaryResourceOverrideForTest(
        "arch_staging.3do",
        []()
        {
            return std::optional<std::vector<std::byte>>(
                stagingModelFixtureBytes());
        });
    editor::DocumentState state{};
    state.level = makeStagingMultiOwnerLiveLevel();
    state.level.things[0].archLightingIndex = -1;
    state.level.things[2].archLightingIndex = -1;
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
    view->primeModel3do("arch_staging.3do", makeStagingInteractiveModel());
    view->setLevel(&window.documentStateForTest().level);
    view->setArchitecturalLightingEditingActive(true);
    view->setSelectedThing(1);
    editor::Selection& selection = window.mutableDocumentStateForTest().selection;
    selection.things.insert(0);
    selection.things.insert(1);
    selection.things.insert(2);
    view->setSelectionState(-1, -1, 1, -1, selection);
    (void)window.refreshArchitecturalLightingOwnerForTest(1);
}

bool sameFloatBits(const float left, const float right) noexcept
{
    return std::bit_cast<std::uint32_t>(left)
        == std::bit_cast<std::uint32_t>(right);
}

bool sameVertexBits(
    const MotsArchLightVertex& left,
    const MotsArchLightVertex& right) noexcept
{
    return sameFloatBits(left.mono, right.mono)
        && sameFloatBits(left.red, right.red)
        && sameFloatBits(left.green, right.green)
        && sameFloatBits(left.blue, right.blue);
}

void setScrubTextAsUser(
    ui::ScrubFloatEdit* const edit,
    const QString& text)
{
    if (!edit || !edit->lineEdit())
    {
        return;
    }
    edit->lineEdit()->setFocus();
    edit->lineEdit()->setText(text);
    QMetaObject::invokeMethod(
        edit->lineEdit(),
        "textEdited",
        Qt::DirectConnection,
        Q_ARG(QString, text));
    QMetaObject::invokeMethod(
        edit->lineEdit(),
        "editingFinished",
        Qt::DirectConnection);
    edit->lineEdit()->clearFocus();
    QApplication::processEvents(QEventLoop::AllEvents);
}

void setScrubTextWhilePopupOpen(
    ui::ScrubFloatEdit* const edit,
    const QString& text,
    const bool processEvents = true)
{
    if (!edit || !edit->lineEdit())
    {
        return;
    }
    edit->lineEdit()->setFocus();
    edit->lineEdit()->setText(text);
    QMetaObject::invokeMethod(
        edit->lineEdit(),
        "textEdited",
        Qt::DirectConnection,
        Q_ARG(QString, text));
    if (processEvents)
    {
        QApplication::processEvents(QEventLoop::AllEvents);
    }
}

void runSparseEntityIdTests()
{
    ui::lighting::MotsArchLightingStaging staging(makeSparsePlan());
    check(staging.setSelectedThingIndices({ 303, 7 }),
        "ARCHLIGHTING staging accepts sparse entity IDs in selection order");
    const auto topologies = staging.selectedTopologies();
    check(staging.selectedThingIndices() == std::vector<std::int32_t>{ 303, 7 }
            && topologies.size() == 2
            && topologies[0].geosets == std::vector<std::vector<std::size_t>>{ { 2u } }
            && topologies[1].geosets == std::vector<std::vector<std::size_t>>{ { 2u } },
        "ARCHLIGHTING staging resolves selected topologies by Thing ID rather than target-vector position");

    check(staging.setVertex(
              0, 1, MotsArchLightVertex{ -0.0f, 2.0f, 3.0f, 4.0f }),
        "ARCHLIGHTING sparse-ID staging applies Set to the shared source stage");
    const bool hasSparseVertex = staging.records().size() == 1
        && staging.records()[0].stagedRecord
        && !staging.records()[0].stagedRecord->meshes.empty()
        && staging.records()[0].stagedRecord->meshes[0].vertices.size() > 1
        && sameVertexBits(
            staging.records()[0].stagedRecord->meshes[0].vertices[1],
            MotsArchLightVertex{ -0.0f, 2.0f, 3.0f, 4.0f });
    check(hasSparseVertex,
        "ARCHLIGHTING sparse-ID Set retains exact staged channel bits");
}

void runPendingStateTests()
{
    ui::lighting::MotsArchLightingStaging staging(makeSparsePlan());
    check(staging.setSelectedThingIndices({ 303 }),
        "ARCHLIGHTING staging selects one sparse owner before pending-state checks");
    const MotsArchLightVertex firstValue{ 2.0f, 3.0f, 4.0f, 5.0f };
    const bool initialSet = staging.setVertex(0, 0, firstValue);
    check(initialSet && staging.hasChanges(),
        "ARCHLIGHTING staging records the initial Set operation");
    if (!initialSet
        || staging.records().size() != 1
        || !staging.records()[0].stagedRecord
        || staging.records()[0].stagedRecord->meshes.empty()
        || staging.records()[0].stagedRecord->meshes[0].vertices.empty())
    {
        check(false,
            "ARCHLIGHTING pending-state fixture exposes a staged first vertex");
        return;
    }
    const MotsArchLightVertex stagedBeforeReject =
        staging.records()[0].stagedRecord->meshes[0].vertices[0];

    check(!staging.multiply(
              level::MotsArchLightingVertexMode::Rgb,
              1.0f,
              ColorF{ 2.0f, 2.0f, 2.0f, 1.0f }),
        "ARCHLIGHTING staging rejects a different operation while Set is pending");
    check(staging.hasChanges()
            && staging.plan().edit.kind
                == level::MotsArchLightingEditKind::SetVertex
            && sameVertexBits(
                staging.records()[0].stagedRecord->meshes[0].vertices[0],
                stagedBeforeReject),
        "ARCHLIGHTING rejected operation preserves the pending Set preview");

    check(!staging.setVertex(0, 1, MotsArchLightVertex{ 9.0f, 9.0f, 9.0f, 9.0f }),
        "ARCHLIGHTING staging rejects a different mesh/vertex while Set is pending");
    check(staging.hasChanges()
            && staging.plan().edit.meshIndex == 0
            && staging.plan().edit.vertexIndex == 0
            && sameVertexBits(
                staging.records()[0].stagedRecord->meshes[0].vertices[0],
                stagedBeforeReject),
        "ARCHLIGHTING Set retarget rejection preserves the original pending coordinates and values");

    check(!staging.setVertex(
              0, 0,
              MotsArchLightVertex{
                  std::numeric_limits<float>::quiet_NaN(),
                  3.0f, 4.0f, 5.0f }),
        "ARCHLIGHTING staging rejects non-finite replacement values while Set is pending");
    check(staging.hasChanges()
            && sameVertexBits(
                staging.records()[0].stagedRecord->meshes[0].vertices[0],
                stagedBeforeReject),
        "ARCHLIGHTING invalid Set values preserve the existing pending preview");

    check(staging.reset() && !staging.hasChanges(),
        "ARCHLIGHTING Reset clears the pending sparse-ID operation");
    check(staging.setSelectedThingIndices({ 303 }),
        "ARCHLIGHTING staging reselects the same sparse owner after Reset");
    check(staging.multiply(
              level::MotsArchLightingVertexMode::Rgb,
              2.0f,
              ColorF{ 2.0f, 2.0f, 2.0f, 1.0f }),
        "ARCHLIGHTING staging records a bulk RGB operation");
    check(staging.multiply(
              level::MotsArchLightingVertexMode::Rgb,
              1.0f,
              ColorF{ 1.0f, 1.0f, 1.0f, 1.0f }),
        "ARCHLIGHTING staging accepts a neutral same-kind update");
    const bool neutralRestored = staging.records().size() == 1
        && staging.records()[0].stagedRecord
        && !staging.records()[0].stagedRecord->meshes.empty()
        && !staging.records()[0].stagedRecord->meshes[0].vertices.empty()
        && sameVertexBits(
            staging.records()[0].stagedRecord->meshes[0].vertices[0],
            MotsArchLightVertex{ 0.25f, 0.5f, 0.75f, 1.0f });
    check(!staging.hasChanges() && neutralRestored,
        "ARCHLIGHTING neutral update recomputes from baseline and clears pending state");
}

void runSelectionAndResetTests()
{
    ui::lighting::MotsArchLightingStaging staging(makeSparsePlan());
    check(staging.setSelectedThingIndices({}),
        "ARCHLIGHTING staging permits clearing selection before an operation");
    check(staging.selectedThingIndices().empty()
            && staging.selectedTopologies().empty()
            && !staging.multiply(
                level::MotsArchLightingVertexMode::Rgb,
                1.0f,
                ColorF{ 1.0f, 1.0f, 1.0f, 1.0f }),
        "ARCHLIGHTING empty selection cannot stage an edit or retain hidden owners");

    check(staging.setSelectedThingIndices({ 900 }),
        "ARCHLIGHTING staging selects a corrupt stale binding by Thing ID");
    check(!staging.resetToDefault() && !staging.hasChanges(),
        "ARCHLIGHTING Reset rejects a corrupt -2 binding without changing the staged plan");

    check(staging.setSelectedThingIndices({ 303 })
            && staging.resetToDefault() && staging.hasChanges()
            && staging.plan().edit.kind == level::MotsArchLightingEditKind::Reset,
        "ARCHLIGHTING Reset stages the selected valid owner's values to default");
    const auto resetResult = ui::lighting::validateMotsArchLightingPlanResources(
        staging.plan(), staging.targets());
    check(resetResult.status == ui::lighting::MotsArchLightingPlanStatus::Applied
            && resetResult.message ==
                "MotS ARCHLIGHTING resources are current.",
        "ARCHLIGHTING Reset validation accepts current model topology with the fixed message");

    MotsArchLightingPlan arbitraryThingPlan = makeSparsePlan();
    arbitraryThingPlan.targets.push_back(
        makeStagingTarget(901, -1, -1, true, true));
    arbitraryThingPlan.targets.back().hasArchFlag = false;
    arbitraryThingPlan.targets.back().modelAvailable = false;
    arbitraryThingPlan.targets.back().valueEditingAllowed = false;
    arbitraryThingPlan.targets.back().modelName.clear();
    ui::lighting::MotsArchLightingStaging arbitraryThing(
        std::move(arbitraryThingPlan));
    check(arbitraryThing.setSelectedThingIndices({ 901 })
            && !arbitraryThing.initializeDefault()
            && !arbitraryThing.hasChanges(),
        "ARCHLIGHTING staging rejects initialization for an arbitrary Thing without the Arch flag");
}

void runExplicitVertexModeTests()
{
    ui::lighting::MotsArchLightingStaging mono(makeSparsePlan());
    check(mono.setSelectedThingIndices({ 303 })
            && mono.multiply(
                level::MotsArchLightingVertexMode::Mono,
                1.25f,
                ColorF{ 9.0f, 8.0f, 7.0f, 1.0f }),
        "ARCHLIGHTING staging accepts an explicit Mono edit mode");
    check(mono.hasChanges()
            && mono.plan().edit.mode == level::MotsArchLightingVertexMode::Mono
            && mono.plan().edit.monoMultiplier == 1.25f
            && mono.plan().edit.rgbMultiplier.r == 9.0f
            && mono.plan().edit.rgbMultiplier.g == 8.0f
            && mono.plan().edit.rgbMultiplier.b == 7.0f,
        "ARCHLIGHTING Mono staging retains the explicit mode without deriving it from channel values");
}

void runValidationStringTests()
{
    ui::lighting::MotsArchLightingStaging noop(makeSparsePlan());
    check(noop.setSelectedThingIndices({}),
        "ARCHLIGHTING validation fixture clears its initial selection");
    const auto noopResult = ui::lighting::validateMotsArchLightingPlanResources(
        noop.plan(), noop.targets());
    check(noopResult.status == ui::lighting::MotsArchLightingPlanStatus::Noop
            && noopResult.message ==
                "No MotS ARCHLIGHTING changes are staged.",
        "ARCHLIGHTING validation returns the fixed no-op message");

    ui::lighting::MotsArchLightingStaging value(makeSparsePlan());
    check(value.setSelectedThingIndices({ 303 })
            && value.setVertex(0, 0, MotsArchLightVertex{ 7.0f, 8.0f, 9.0f, 10.0f }),
        "ARCHLIGHTING validation fixture stages a value edit");
    auto staleTargets = value.targets();
    if (staleTargets.empty())
    {
        check(false,
            "ARCHLIGHTING validation fixture exposes a target for stale-resource checks");
        return;
    }
    staleTargets[0].resourceRevision += "|replaced";
    const auto staleResult =
        ui::lighting::validateMotsArchLightingPlanResources(
            value.plan(), staleTargets);
    check(staleResult.status == ui::lighting::MotsArchLightingPlanStatus::Rejected
            && staleResult.message ==
                "The selected ARCHLIGHTING model or topology changed while the dialog was open. Reopen the dialog and try again.",
        "ARCHLIGHTING validation returns the fixed stale-resource message");
}

void runInteractiveModeSeamTests()
{
    MainWindow window{};
    window.setAttribute(Qt::WA_DontShowOnScreen);
    window.setProjectTypeForTest(ProjectType::MOTS);

    window.activateArchitecturalLightingEditModeForTest();
    check(window.architecturalLightingEditModeActiveForTest(),
        "ARCHLIGHTING interactive mode exposes an explicit Light submode");
    QAction* const architecturalAction =
        window.findChild<QAction*>(QStringLiteral("viewport.light_mode.architectural"));
    check(architecturalAction != nullptr
            && architecturalAction->isCheckable()
            && architecturalAction->isChecked(),
        "ARCHLIGHTING interactive mode has a checked Light toolbar action");

    GlView* const view = window.glViewForTest();
    check(view != nullptr,
        "ARCHLIGHTING interactive mode exposes its viewport seam");
    if (!view)
    {
        return;
    }
    check(view->pickMode() == GlView::PickMode::ArchitecturalLighting,
        "ARCHLIGHTING interactive mode owns a dedicated viewport pick mode");

    // The legacy Architectural category preference no longer controls ARCH
    // owner visibility. Keep it enabled while Arch Things is off so this seam
    // proves Arch edit mode alone forces its owner markers.
    QSettings settings = settingsStore();
    const QString architecturalSetting =
        QStringLiteral("viewport/light_gizmos_always_architectural");
    const bool hadArchitecturalSetting = settings.contains(architecturalSetting);
    const QVariant savedArchitecturalSetting = settings.value(architecturalSetting);
    settings.setValue(architecturalSetting, true);
    settings.sync();
    view->setAlwaysShowLightGizmoCategory(
        render::LightGizmoCategory::Architectural, true);
    view->setShowThingGizmos(false);
    view->setShowArchThingGizmos(false);
    const std::uint64_t readsBeforeVertexQuery =
        view->model3doResourceReadCountForTest();
    view->setArchitecturalLightingEditingActive(true);
    check(view->architecturalLightingEditingActive()
            && view->architecturalLightingOwnerGizmosVisibleForTest()
            && !view->showThingGizmos(),
        "ARCHLIGHTING interactive mode can force owner gizmos independently of Thing visibility");

    const auto vertices = view->architecturalLightingVerticesForThing(0);
    check(vertices.empty()
            && view->model3doResourceReadCountForTest() == readsBeforeVertexQuery,
        "ARCHLIGHTING interactive vertex accessor is read-only and has no stale owner cache before a model is published");

    window.activateBakedLightEditModeForTest();
    check(!window.architecturalLightingEditModeActiveForTest()
            && !view->architecturalLightingEditingActive()
            && !view->architecturalLightingOwnerGizmosVisibleForTest()
            && view->alwaysShowLightGizmoCategory(
                render::LightGizmoCategory::Architectural)
            && !view->showThingGizmos()
            && (!architecturalAction || !architecturalAction->isChecked()),
        "ARCHLIGHTING exit hides owners when Arch Things is off even if the legacy Architectural category is on");
    view->setAlwaysShowLightGizmoCategory(
        render::LightGizmoCategory::Architectural, false);
    view->setShowArchThingGizmos(true);
    check(view->architecturalLightingOwnerGizmosVisibleForTest(),
        "Arch Things shows MotS owner markers when the legacy Architectural category is off");
    if (hadArchitecturalSetting)
    {
        settings.setValue(architecturalSetting, savedArchitecturalSetting);
    }
    else
    {
        settings.remove(architecturalSetting);
    }
    settings.sync();
}

void runInteractiveModeActionTests()
{
    MainWindow window{};
    window.setAttribute(Qt::WA_DontShowOnScreen);
    window.setProjectTypeForTest(ProjectType::MOTS);

    QAction* const editLightAction =
        window.findChild<QAction*>(QStringLiteral("viewport.edit_mode.light"));
    QAction* const architecturalAction =
        window.findChild<QAction*>(QStringLiteral("viewport.light_mode.architectural"));
    QAction* const bakedAction =
        window.findChild<QAction*>(QStringLiteral("viewport.light_mode.baked"));
    QAction* const thingAction =
        window.findChild<QAction*>(QStringLiteral("viewport.light_mode.thing"));
    QAction* const architecturalVisibilityAction =
        window.findChild<QAction*>(
            QStringLiteral("viewport.light_gizmos.always_architectural"));
    GlView* const view = window.glViewForTest();
    QSettings settings = settingsStore();
    const bool hadArchitecturalSetting = settings.contains(
        QStringLiteral("viewport/light_gizmos_always_architectural"));
    const QVariant savedArchitecturalSetting = settings.value(
        QStringLiteral("viewport/light_gizmos_always_architectural"));

    check(editLightAction && architecturalAction && bakedAction && thingAction
            && architecturalVisibilityAction && view,
        "ARCHLIGHTING production route exposes Light/submode and visibility actions");
    if (!editLightAction || !architecturalAction || !bakedAction || !thingAction
        || !architecturalVisibilityAction || !view)
    {
        return;
    }

    check(architecturalVisibilityAction->isCheckable()
            && architecturalVisibilityAction->isChecked()
            && view->alwaysShowLightGizmoCategory(
                render::LightGizmoCategory::Architectural),
        "the retained legacy Architectural category setting is still published to the viewport");

    window.setProjectTypeForTest(ProjectType::JKDF2);
    check(!architecturalVisibilityAction->isEnabled(),
        "the retained Architectural category setting stays disabled outside MotS");
    window.setProjectTypeForTest(ProjectType::MOTS);
    check(architecturalVisibilityAction->isEnabled(),
        "the retained Architectural category setting becomes enabled again for MotS");

    editLightAction->trigger();
    check(editLightAction->isChecked() && architecturalAction->isEnabled(),
        "ARCHLIGHTING test enters Light mode through the production QAction route");
    architecturalVisibilityAction->setChecked(false);
    settings.sync();
    check(!architecturalVisibilityAction->isChecked()
            && !view->alwaysShowLightGizmoCategory(
                render::LightGizmoCategory::Architectural)
            && !settings.value(
                QStringLiteral("viewport/light_gizmos_always_architectural"),
                true).toBool(),
        "the hidden legacy Architectural category setting still updates its stored Editor value");

    architecturalAction->trigger();
    QWidget* const contextualTools = window.findChild<QWidget*>(
        QStringLiteral("viewportToolsOverlay"));
    check(contextualTools && contextualTools->isHidden(),
        "ARCH submode hides the empty contextual tools overlay after Add ARCH was removed");
    check(architecturalAction->isChecked()
            && view->lightGizmoCategory()
                == render::LightGizmoCategory::Architectural
            && view->architecturalLightingEditingActive()
            && !view->alwaysShowLightGizmoCategory(
                render::LightGizmoCategory::Architectural),
        "ARCHLIGHTING mode forces its owner gizmo path without rewriting a disabled preference");

    view->setRenderPipeline(GlView::RenderPipeline::GamePreview);
    QCoreApplication::processEvents();
    check(!view->architecturalLightingEditingActive()
            && !view->architecturalLightingOwnerGizmosVisibleForTest(),
        "ARCHLIGHTING active editing is cancelled when the viewport enters Game Preview");
    view->setRenderPipeline(GlView::RenderPipeline::Editor);
    QCoreApplication::processEvents();
    check(view->architecturalLightingEditingActive()
            && architecturalAction->isEnabled()
            && architecturalAction->isChecked(),
        "ARCHLIGHTING submode resumes its editor interaction after returning from Game Preview");

    bakedAction->trigger();
    check(bakedAction->isChecked()
            && !view->architecturalLightingEditingActive()
            && view->lightGizmoCategory() == render::LightGizmoCategory::Baked
            && !view->alwaysShowLightGizmoCategory(
                render::LightGizmoCategory::Architectural),
        "ARCHLIGHTING exit restores normal family selection and the disabled preference");

    architecturalVisibilityAction->setChecked(true);
    thingAction->trigger();
    check(thingAction->isChecked()
            && view->lightGizmoCategory() == render::LightGizmoCategory::Thing
            && view->alwaysShowLightGizmoCategory(
                render::LightGizmoCategory::Architectural),
        "ARCHLIGHTING preference remains enabled when the ordinary Thing family is active");

    architecturalAction->trigger();
    bakedAction->trigger();
    check(!view->architecturalLightingEditingActive()
            && view->alwaysShowLightGizmoCategory(
                render::LightGizmoCategory::Architectural),
        "ARCHLIGHTING mode exit preserves the enabled preference");

    const bool hadPreviewArchitecturalSetting = settings.contains(
        QStringLiteral("viewport/preview_light_gizmos_always_architectural"));
    const QVariant savedPreviewArchitecturalSetting = settings.value(
        QStringLiteral("viewport/preview_light_gizmos_always_architectural"));
    settings.remove(QStringLiteral("viewport/preview_light_gizmos_always_architectural"));
    settings.sync();
    view->setRenderPipeline(GlView::RenderPipeline::GamePreview);
    QCoreApplication::processEvents();
    check(!architecturalVisibilityAction->isChecked()
            && !view->alwaysShowLightGizmoCategory(
                render::LightGizmoCategory::Architectural),
        "ARCHLIGHTING Preview starts with the existing light-family default off");
    view->setRenderPipeline(GlView::RenderPipeline::Editor);
    QCoreApplication::processEvents();
    check(architecturalVisibilityAction->isChecked()
            && view->alwaysShowLightGizmoCategory(
                render::LightGizmoCategory::Architectural),
        "ARCHLIGHTING returning to Editor restores its independent preference");
    if (hadPreviewArchitecturalSetting)
    {
        settings.setValue(
            QStringLiteral("viewport/preview_light_gizmos_always_architectural"),
            savedPreviewArchitecturalSetting);
    }
    else
    {
        settings.remove(QStringLiteral("viewport/preview_light_gizmos_always_architectural"));
    }

    if (hadArchitecturalSetting)
    {
        settings.setValue(
            QStringLiteral("viewport/light_gizmos_always_architectural"),
            savedArchitecturalSetting);
    }
    else
    {
        settings.remove(QStringLiteral("viewport/light_gizmos_always_architectural"));
    }
    settings.sync();
}

void runArchitecturalRecoveryCompatibilityTests()
{
    MainWindow window{};
    window.setAttribute(Qt::WA_DontShowOnScreen);
    window.setProjectTypeForTest(ProjectType::MOTS);
    QSettings settings = settingsStore();
    const std::array<QString, 4> editorSettingKeys{
        QStringLiteral("viewport/light_gizmos_always_baked"),
        QStringLiteral("viewport/light_gizmos_always_thing"),
        QStringLiteral("viewport/light_gizmos_always_sector"),
        QStringLiteral("viewport/light_gizmos_always_architectural") };
    const std::array<QString, 4> previewSettingKeys{
        QStringLiteral("viewport/preview_light_gizmos_always_baked"),
        QStringLiteral("viewport/preview_light_gizmos_always_thing"),
        QStringLiteral("viewport/preview_light_gizmos_always_sector"),
        QStringLiteral("viewport/preview_light_gizmos_always_architectural") };
    std::array<bool, 4> hadEditorSettings{};
    std::array<bool, 4> hadPreviewSettings{};
    std::array<QVariant, 4> savedEditorSettings{};
    std::array<QVariant, 4> savedPreviewSettings{};
    for (std::size_t index = 0; index < editorSettingKeys.size(); ++index)
    {
        hadEditorSettings[index] = settings.contains(editorSettingKeys[index]);
        savedEditorSettings[index] = settings.value(editorSettingKeys[index]);
        hadPreviewSettings[index] = settings.contains(previewSettingKeys[index]);
        savedPreviewSettings[index] = settings.value(previewSettingKeys[index]);
    }

    const auto setCategorySettings = [&, editorSettingKeys, previewSettingKeys](
        const bool preview,
        const std::array<bool, 4>& values)
    {
        const auto& keys = preview ? previewSettingKeys : editorSettingKeys;
        for (std::size_t index = 0; index < keys.size(); ++index)
        {
            settings.setValue(keys[index], values[index]);
        }
        settings.sync();
    };
    const auto restoreSettings = [&]()
    {
        for (std::size_t index = 0; index < editorSettingKeys.size(); ++index)
        {
            if (hadEditorSettings[index])
            {
                settings.setValue(editorSettingKeys[index], savedEditorSettings[index]);
            }
            else
            {
                settings.remove(editorSettingKeys[index]);
            }
            if (hadPreviewSettings[index])
            {
                settings.setValue(previewSettingKeys[index], savedPreviewSettings[index]);
            }
            else
            {
                settings.remove(previewSettingKeys[index]);
            }
        }
        settings.sync();
    };
    // Deliberately keep machine preferences different from the recovery
    // payload. The serialized viewport is authoritative while it is restored;
    // this catches the normal map-settings signal from overwriting it.
    const std::array<bool, 4> conflictingSettings{ true, false, true, false };
    setCategorySettings(false, conflictingSettings);
    setCategorySettings(true, conflictingSettings);
    const auto conflictingPreferencesRemain = [&]()
    {
        for (std::size_t index = 0; index < editorSettingKeys.size(); ++index)
        {
            if (settings.value(editorSettingKeys[index]).toBool()
                    != conflictingSettings[index]
                || settings.value(previewSettingKeys[index]).toBool()
                    != conflictingSettings[index])
            {
                return false;
            }
        }
        return true;
    };
    GlView* const view = window.glViewForTest();
    const QByteArray captured = window.captureRecoveryEditorStateForTest();
    const QCborValue decoded = QCborValue::fromCbor(captured);
    QCborMap root = decoded.isMap() ? decoded.toMap() : QCborMap{};
    const QCborValue viewportValue = root.value(QStringLiteral("viewport"));
    const QCborArray capturedViewport = viewportValue.isArray()
        ? viewportValue.toArray()
        : QCborArray{};
    const QCborValue categoriesValue = capturedViewport.size() > 33
        ? capturedViewport.at(33)
        : QCborValue{};
    const QCborArray capturedCategories = categoriesValue.isArray()
        ? categoriesValue.toArray()
        : QCborArray{};
    check(view && capturedCategories.size() == 4,
        "ARCHLIGHTING recovery fixture exposes the four-family viewport schema");
    if (!view || capturedCategories.size() != 4 || capturedViewport.size() <= 33)
    {
        restoreSettings();
        return;
    }

    const auto makePayload = [&](const GlView::RenderPipeline pipeline,
                                 const QCborArray& categories)
    {
        QCborMap payloadRoot = root;
        QCborArray payloadViewport = capturedViewport;
        payloadViewport[2] = static_cast<int>(pipeline);
        payloadViewport[33] = categories;
        payloadRoot.insert(QStringLiteral("viewport"), payloadViewport);
        return ui::recovery::serializeCanonicalCbor(QCborValue(payloadRoot));
    };

    QCborArray legacyCategories{};
    legacyCategories.append(capturedCategories.at(0));
    legacyCategories.append(capturedCategories.at(1));
    legacyCategories.append(capturedCategories.at(2));
    QString warning{};
    const bool legacyEditorRestored = window.restoreRecoveryEditorStateForTest(
        makePayload(GlView::RenderPipeline::Editor, legacyCategories), &warning);
    check(legacyEditorRestored
            && view->renderPipeline() == GlView::RenderPipeline::Editor
            && view->alwaysShowLightGizmoCategory(
                render::LightGizmoCategory::Architectural),
        "ARCHLIGHTING legacy three-family Editor recovery defaults the missing family on");

    warning.clear();
    const bool legacyPreviewRestored = window.restoreRecoveryEditorStateForTest(
        makePayload(GlView::RenderPipeline::GamePreview, legacyCategories), &warning);
    check(legacyPreviewRestored
            && view->renderPipeline() == GlView::RenderPipeline::GamePreview
            && !view->alwaysShowLightGizmoCategory(
                render::LightGizmoCategory::Architectural),
        "ARCHLIGHTING legacy three-family Preview recovery defaults the missing family off");

    QCborArray fourCategories{};
    fourCategories.append(false);
    fourCategories.append(true);
    fourCategories.append(false);
    fourCategories.append(true);
    warning.clear();
    const bool fourFamilyRestored = window.restoreRecoveryEditorStateForTest(
        makePayload(GlView::RenderPipeline::Editor, fourCategories), &warning);
    check(fourFamilyRestored
            && view->alwaysShowLightGizmoCategory(render::LightGizmoCategory::Baked) == false
            && view->alwaysShowLightGizmoCategory(render::LightGizmoCategory::Thing)
            && view->alwaysShowLightGizmoCategory(render::LightGizmoCategory::Sector) == false
            && view->alwaysShowLightGizmoCategory(
                render::LightGizmoCategory::Architectural),
        "ARCHLIGHTING four-family recovery round-trips the explicit ARCH preference");

    // Exercise both ordinary viewport refresh and the persistent Map Settings
    // panel after the recovery guard has ended. Neither path may reinterpret
    // the recovered categories as machine preferences.
    const bool showGridBeforeRefresh = view->showGrid();
    view->setShowGrid(!showGridBeforeRefresh);
    QCoreApplication::processEvents();
    QMenu* const mapSettingsMenu = window.findChild<QMenu*>(
        QStringLiteral("viewportMapSettingsMenu"));
    if (mapSettingsMenu)
    {
        mapSettingsMenu->show();
        QCoreApplication::processEvents();
    }
    const bool recoveredCategoriesStillLive =
        !view->alwaysShowLightGizmoCategory(render::LightGizmoCategory::Baked)
        && view->alwaysShowLightGizmoCategory(render::LightGizmoCategory::Thing)
        && !view->alwaysShowLightGizmoCategory(render::LightGizmoCategory::Sector)
        && view->alwaysShowLightGizmoCategory(
            render::LightGizmoCategory::Architectural);
    const std::array<const char*, 4> lightActionNames{
        "viewport.light_gizmos.always_baked",
        "viewport.light_gizmos.always_thing",
        "viewport.light_gizmos.always_sector",
        "viewport.light_gizmos.always_architectural" };
    const std::array<bool, 4> recoveredCategoryValues{
        false, true, false, true };
    bool recoveredActionsStillLive = true;
    for (std::size_t index = 0; index < lightActionNames.size(); ++index)
    {
        const QAction* const action = window.findChild<QAction*>(
            QString::fromLatin1(lightActionNames[index]));
        recoveredActionsStillLive = recoveredActionsStillLive
            && action && action->isChecked() == recoveredCategoryValues[index];
    }
    check(mapSettingsMenu && recoveredCategoriesStillLive
            && recoveredActionsStillLive && conflictingPreferencesRemain(),
        "ARCHLIGHTING recovery survives a later viewport refresh and Map Settings reopen without rewriting preferences");
    if (mapSettingsMenu)
    {
        mapSettingsMenu->hide();
    }
    view->setShowGrid(showGridBeforeRefresh);
    QCoreApplication::processEvents();

    QCborArray malformedCategories = fourCategories;
    malformedCategories[3] = QStringLiteral("true");
    const QByteArray beforeMalformed = window.captureRecoveryEditorStateForTest();
    warning.clear();
    const bool malformedRejected = !window.restoreRecoveryEditorStateForTest(
        makePayload(GlView::RenderPipeline::Editor, malformedCategories), &warning);
    check(malformedRejected
            && window.captureRecoveryEditorStateForTest() == beforeMalformed,
        "ARCHLIGHTING recovery rejects a malformed fourth family atomically");
    restoreSettings();
}

void runInteractivePopupSizingTests()
{
    MainWindow window{};
    prepareStagingPopupWindow(
        window,
        MotsArchLightVertex{ -0.0f, 0.25f, 65504.0f, -0.0f });
    window.openArchitecturalLightingVertexEditorForTest(0, 0, 0, 0);
    QApplication::processEvents(QEventLoop::AllEvents);
    QDialog* const dialog = window.architecturalLightingVertexDialogForTest();
    auto* const mode = dialog
        ? dialog->findChild<QComboBox*>(
            QStringLiteral("motsArchLightingInteractiveMode"))
        : nullptr;
    const std::array<const char*, 2> channelNames{
        "motsArchLightingInteractiveMono",
        "motsArchLightingInteractiveRgbIntensity" };
    bool allReadable = dialog != nullptr && mode != nullptr;
    if (dialog)
    {
        if (dialog->layout())
        {
            dialog->layout()->activate();
        }
        dialog->adjustSize();
        QApplication::processEvents(QEventLoop::AllEvents);
    }
    const QString signedNineText = QStringLiteral("-1000000.000000000");
    for (const char* const name : channelNames)
    {
        const bool monoChannel = std::string_view(name)
            == "motsArchLightingInteractiveMono";
        if (mode)
        {
            mode->setCurrentIndex(monoChannel ? 1 : 0);
            QApplication::processEvents(QEventLoop::AllEvents);
            if (dialog->layout())
            {
                dialog->layout()->activate();
            }
            dialog->adjustSize();
            QApplication::processEvents(QEventLoop::AllEvents);
        }
        auto* const scrub = dialog
            ? dialog->findChild<ui::ScrubFloatEdit*>(QString::fromLatin1(name))
            : nullptr;
        QLineEdit* const lineEdit = scrub ? scrub->lineEdit() : nullptr;
        if (!scrub || !lineEdit || !scrub->isVisible())
        {
            allReadable = false;
            continue;
        }

        scrub->setValue(-1'000'000.0);
        QEvent enter(QEvent::Enter);
        QApplication::sendEvent(lineEdit, &enter);
        scrub->updateGeometry();
        if (scrub->layout())
        {
            scrub->layout()->activate();
        }
        if (dialog->layout())
        {
            dialog->layout()->activate();
        }
        QApplication::processEvents(QEventLoop::AllEvents);

        const QFontMetrics metrics(lineEdit->font());
        const int signedNineWidth = metrics.horizontalAdvance(signedNineText);
        const QMargins margins = lineEdit->textMargins();
        const int frameWidth = lineEdit->style()->pixelMetric(
            QStyle::PM_DefaultFrameWidth, nullptr, lineEdit);
        const int textAreaWidth = lineEdit->width()
            - margins.left() - margins.right() - 2 * frameWidth;
        const auto steppers = scrub->findChildren<QToolButton*>();
        const auto compactArrowCap = [](const QToolButton* const button)
        {
            if (!button)
            {
                return 0;
            }
            const QFontMetrics buttonMetrics(button->font());
            const int styleMargin = button->style()->pixelMetric(
                QStyle::PM_ButtonMargin, nullptr, button);
            const int horizontalPadding = std::max({
                2,
                buttonMetrics.height() / 8,
                styleMargin / 2 });
            const int frame = button->style()->pixelMetric(
                QStyle::PM_DefaultFrameWidth, nullptr, button);
            return buttonMetrics.horizontalAdvance(button->text())
                + 2 * horizontalPadding + 2 * frame;
        };
        const bool twoSteppers = steppers.size() == 2;
        const bool compactSteppers = twoSteppers
            && std::all_of(
                steppers.begin(), steppers.end(),
                [&compactArrowCap](const QToolButton* const button)
                {
                    return button && button->width() > 0
                        && button->width() <= compactArrowCap(button);
                });
        const int arrowWidth = twoSteppers
            ? steppers[0]->width() + steppers[1]->width()
            : 0;
        const QRect lineRect = lineEdit->geometry();
        bool childrenFit = twoSteppers;
        bool childrenDoNotOverlap = twoSteppers;
        for (const QToolButton* const button : steppers)
        {
            if (!button)
            {
                childrenFit = false;
                childrenDoNotOverlap = false;
                continue;
            }
            const QRect buttonRect = button->geometry();
            childrenFit = childrenFit && scrub->rect().contains(buttonRect);
            childrenDoNotOverlap = childrenDoNotOverlap
                && !lineRect.intersects(buttonRect);
        }
        const bool enabled = scrub->isEnabled();
        const bool arrowsReadable = !enabled
            || (std::all_of(
                    steppers.begin(), steppers.end(),
                    [](const QToolButton* const button)
                    {
                        return button && button->isVisible();
                    })
                && textAreaWidth > arrowWidth
                && compactSteppers
                && childrenFit
                && childrenDoNotOverlap);
        allReadable = allReadable
            && lineEdit->text() == signedNineText
            && lineEdit->width() >= signedNineWidth
            && textAreaWidth >= signedNineWidth
            && scrub->width() >= lineEdit->width()
            && arrowsReadable;

        if (enabled && twoSteppers)
        {
            steppers[0]->setDown(true);
            if (scrub->layout())
            {
                scrub->layout()->activate();
            }
            QApplication::processEvents(QEventLoop::AllEvents);
            const int pressedTextAreaWidth = lineEdit->width()
                - margins.left() - margins.right() - 2 * frameWidth;
            allReadable = allReadable
                && steppers[0]->isDown()
                && pressedTextAreaWidth >= signedNineWidth
                && std::all_of(
                    steppers.begin(), steppers.end(),
                    [](const QToolButton* const button)
                    {
                        return button && button->isVisible();
                    })
                && compactSteppers;
            steppers[0]->setDown(false);
        }

        if (!allReadable)
        {
            std::cerr << "ARCHLIGHTING sizing field=" << name
                      << " text=" << lineEdit->text().toStdString()
                      << " scrubVisible=" << scrub->isVisible()
                      << " lineWidth=" << lineEdit->width()
                      << " scrubWidth=" << scrub->width()
                      << " steppers=" << steppers.size()
                      << "\n";
        }
    }
    check(allReadable,
        "ARCHLIGHTING popup keeps Mono and RGB intensity signed-nine values readable with compact scrub arrows");
    if (dialog)
    {
        dialog->reject();
        QApplication::processEvents(QEventLoop::AllEvents);
    }
}

void runUnifiedArchitecturalPopupContractTests()
{
    MainWindow window{};
    prepareStagingPopupWindow(
        window,
        MotsArchLightVertex{ -0.0f, 0.25f, 65504.0f, -0.0f });

    const QAction* const targetModeAction = window.findChild<QAction*>(
        QStringLiteral("viewport.arch_lighting.target_mode"));
    const QToolButton* const targetModeButton = window.findChild<QToolButton*>(
        QStringLiteral("viewportArchitecturalLightingTargetModeButton"));
    check(!targetModeAction && !targetModeButton,
        "ARCHLIGHTING exposes unified vertex-first selection without an Owner/Vertices toggle");

    window.openArchitecturalLightingVertexEditorForTest(0, 0, 0, 0);
    QApplication::processEvents(QEventLoop::AllEvents);
    QDialog* const dialog = window.architecturalLightingVertexDialogForTest();
    auto* const mode = dialog
        ? dialog->findChild<QComboBox*>(
            QStringLiteral("motsArchLightingInteractiveMode"))
        : nullptr;
    auto* const colorIntensity = dialog
        ? dialog->findChild<ui::ScrubFloatEdit*>(
            QStringLiteral("motsArchLightingInteractiveRgbIntensity"))
        : nullptr;
    auto* const monoIntensity = dialog
        ? dialog->findChild<ui::ScrubFloatEdit*>(
            QStringLiteral("motsArchLightingInteractiveMono"))
        : nullptr;
    const bool hasLegacyRawRgbEditors = dialog
        && (dialog->findChild<ui::ScrubFloatEdit*>(
                QStringLiteral("motsArchLightingInteractiveRed"))
            || dialog->findChild<ui::ScrubFloatEdit*>(
                QStringLiteral("motsArchLightingInteractiveGreen"))
            || dialog->findChild<ui::ScrubFloatEdit*>(
                QStringLiteral("motsArchLightingInteractiveBlue")));
    const auto* const colorWidget = dialog
        ? dialog->findChild<QWidget*>(
            QStringLiteral("motsArchLightingInteractiveColor"))
        : nullptr;
    const auto* const colorEditor =
        dynamic_cast<const ui::LiveColorPopup*>(colorWidget);
    const bool hasDialogButtons = dialog
        && dialog->findChild<QDialogButtonBox*>() != nullptr;
    const bool popupContract = dialog
            && dialog->objectName() == QStringLiteral("lightParametersPopup")
            && dialog->windowFlags().testFlag(Qt::Popup)
            && dialog->windowFlags().testFlag(Qt::FramelessWindowHint)
            && mode && colorIntensity && monoIntensity && colorEditor
            && !hasLegacyRawRgbEditors
            && !hasDialogButtons;
    if (!popupContract)
    {
        std::cerr << "ARCHLIGHTING popup contract dialog=" << (dialog != nullptr)
                  << " object=" << (dialog
                        && dialog->objectName() == QStringLiteral("lightParametersPopup"))
                  << " popupFlag=" << (dialog
                        && dialog->windowFlags().testFlag(Qt::Popup))
                  << " framelessFlag=" << (dialog
                        && dialog->windowFlags().testFlag(Qt::FramelessWindowHint))
                  << " mode=" << (mode != nullptr)
                  << " rgbIntensity=" << (colorIntensity != nullptr)
                  << " monoIntensity=" << (monoIntensity != nullptr)
                  << " colorEditor=" << (colorEditor != nullptr)
                  << " dialogButtons=" << hasDialogButtons
                  << " legacyRgb=" << hasLegacyRawRgbEditors
                  << "\n";
    }
    check(popupContract,
        "ARCHLIGHTING vertex editing uses the live RGB/Mono light popup without Apply or Cancel buttons");
    if (dialog)
    {
        dialog->close();
        QApplication::processEvents(QEventLoop::AllEvents);
    }
}

void runArchitecturalLightFamilyVisibilityTests()
{
    const auto verifyProject = [](const ProjectType project, const bool expectedVisible)
    {
        MainWindow window{};
        window.setProjectTypeForTest(project);
        window.updateEditActionsForTest();
        QAction* const archAction = window.findChild<QAction*>(
            QStringLiteral("viewport.light_mode.architectural"));
        check(archAction && archAction->isVisible() == expectedVisible,
            expectedVisible
                ? "MotS exposes the ARCH light submode"
                : "non-MotS projects hide the ARCH light submode action");
        QToolButton* const archButton = window.findChild<QToolButton*>(
            QStringLiteral("viewportEditLightArchitecturalButton"));
        check(archButton && archButton->isHidden() != expectedVisible,
            expectedVisible
                ? "MotS shows the ARCH light submode button"
                : "non-MotS projects hide the ARCH light submode button");
        if (project == ProjectType::MOTS)
        {
            check(archAction
                    && archAction->text() == QStringLiteral("Arch"),
                "MotS names the ARCH light submode Arch");
        }
    };

    verifyProject(ProjectType::JKDF2, false);
    verifyProject(ProjectType::IJIM, false);
    verifyProject(ProjectType::MOTS, true);
}

void runArchitecturalPreviewIdentityInvalidationTests()
{
    MainWindow window{};
    const MotsArchLightVertex baseline{
        0.125f, 0.25f, 0.5f, 0.75f };
    prepareStagingPopupWindow(window, baseline);
    GlView* const view = window.glViewForTest();
    check(view != nullptr,
        "ARCHLIGHTING identity regression fixture exposes its viewport");
    if (!view)
    {
        return;
    }

    view->setArchitecturalLightingAuthoringMode(
        level::MotsArchLightingVertexMode::Mono);
    const MotsArchLightVertex staged{
        0.625f, baseline.red, baseline.green, baseline.blue };
    const bool stagedVertex = window.stageArchitecturalLightingVertexForTest(
        0, 0, 0, 0, staged, level::MotsArchLightingVertexMode::Mono);
    const auto markerColor = view->architecturalLightingPointColorForTest(
        GlView::ArchitecturalLightingVertexIdentity{ 0, 0, 0, 0 });
    check(stagedVertex && view->architecturalLightingPreviewActive()
            && markerColor && (*markerColor)[0] == staged.mono
            && (*markerColor)[1] == staged.mono
            && (*markerColor)[2] == staged.mono,
        "ARCHLIGHTING vertex-only FACES 0 model keeps valid staged marker data");

    const std::array<GlView::ArchitecturalLightingPreviewVertex, 1> malformed{
        GlView::ArchitecturalLightingPreviewVertex{
            .identity = GlView::ArchitecturalLightingRenderVertex{
                .thingIndex = 0,
                .lodIndex = 0,
                .meshIndex = 9,
                .vertexIndex = 0,
            },
            .lighting = staged,
        },
    };
    view->setArchitecturalLightingPreview(
        malformed, level::MotsArchLightingVertexMode::Mono);
    check(!view->architecturalLightingPreviewActive(),
        "ARCHLIGHTING malformed mesh identity cancels staged preview data");

    const bool restagedVertex = window.stageArchitecturalLightingVertexForTest(
        0, 0, 0, 0, staged, level::MotsArchLightingVertexMode::Mono);
    auto replacement = std::make_shared<assets::Model3do>(
        *makeStagingInteractiveModel());
    replacement->meshes.front().vertices.front().x = -0.125;
    view->primeModel3do("arch_staging.3do", std::move(replacement));
    check(restagedVertex && !view->architecturalLightingPreviewActive(),
        "ARCHLIGHTING replacement model publication cancels staged data tied to the old identity");
}

void runArchitecturalMonoCommitPresentationTests()
{
    MainWindow window{};
    const MotsArchLightVertex baseline{
        0.125f, 0.25f, 0.5f, 0.75f };
    prepareStagingPopupWindow(window, baseline);
    const std::size_t beforeUndo = window.undoDepthForTest();
    window.openArchitecturalLightingVertexEditorForTest(0, 0, 0, 0);
    QApplication::processEvents(QEventLoop::AllEvents);

    QDialog* const dialog = window.architecturalLightingVertexDialogForTest();
    auto* const mode = dialog
        ? dialog->findChild<QComboBox*>(
            QStringLiteral("motsArchLightingInteractiveMode"))
        : nullptr;
    auto* const mono = dialog
        ? dialog->findChild<ui::ScrubFloatEdit*>(
            QStringLiteral("motsArchLightingInteractiveMono"))
        : nullptr;
    auto* const view = window.glViewForTest();
    if (mode)
    {
        mode->setCurrentIndex(1);
    }
    setScrubTextWhilePopupOpen(mono, QStringLiteral("0.625"), false);
    const bool previewActiveBeforeEvents = view
        && view->architecturalLightingPreviewActive();
    const auto pointBeforeEvents = view
        ? view->architecturalLightingPointColorForTest(
            GlView::ArchitecturalLightingVertexIdentity{ 0, 0, 0, 0 })
        : std::optional<std::array<float, 3>>{};
    QApplication::processEvents(QEventLoop::AllEvents);
    const bool previewActiveAfterEvents = view
        && view->architecturalLightingPreviewActive();
    if (previewActiveBeforeEvents != previewActiveAfterEvents)
    {
        std::cerr << "ARCHLIGHTING Mono event-loop diagnostics: preview before="
                  << previewActiveBeforeEvents << " after=" << previewActiveAfterEvents
                  << " authoringMode=" << (view
                          ? static_cast<int>(view->architecturalLightingAuthoringMode()) : -1)
                  << " displayMode=" << (view
                          ? static_cast<int>(view->architecturalLightingDisplayModeForTest()) : -1)
                  << " pointBefore=";
        if (pointBeforeEvents)
        {
            std::cerr << (*pointBeforeEvents)[0] << ','
                      << (*pointBeforeEvents)[1] << ',' << (*pointBeforeEvents)[2];
        }
        else
        {
            std::cerr << "<missing>";
        }
        std::cerr << '\n';
    }

    const Level& previewLevel = window.documentStateForTest().level;
    bool previewLeavesAuthoredStateUntouched = false;
    if (!previewLevel.motsArchLighting.empty()
        && !previewLevel.motsArchLighting.front().meshes.empty()
        && !previewLevel.motsArchLighting.front().meshes.front().vertices.empty())
    {
        previewLeavesAuthoredStateUntouched = sameVertexBits(
            previewLevel.motsArchLighting.front().meshes.front().vertices.front(),
            baseline);
    }
    const bool dialogStillOpen = dialog
        && window.architecturalLightingVertexDialogForTest() == dialog;
    const bool popupStillVisible = dialog && dialog->isVisible();
    const bool stagedPreviewActive = view
        && view->architecturalLightingPreviewActive();
    const bool undoUnchanged = window.undoDepthForTest() == beforeUndo;
    if (!previewLeavesAuthoredStateUntouched || !undoUnchanged
        || !dialogStillOpen || !popupStillVisible || !stagedPreviewActive)
    {
        std::cerr << "ARCHLIGHTING Mono pre-dismiss diagnostics: authoredUnchanged="
                  << previewLeavesAuthoredStateUntouched
                  << " undoUnchanged=" << undoUnchanged
                  << " dialogStillOpen=" << dialogStillOpen
                  << " popupVisible=" << popupStillVisible
                  << " previewActive=" << stagedPreviewActive
                  << " selectedVertices="
                  << window.architecturalLightingSelectedVertexCountForTest()
                  << " mode=" << (mode ? mode->currentData().toInt() : -1)
                  << " editText=" << (mono && mono->lineEdit()
                          ? mono->lineEdit()->text().toStdString() : "<none>")
                  << " scrubValue=" << (mono ? mono->value() : -1.0)
                  << " monoEnabled=" << (mono && mono->isEnabled())
                  << " monoVisible=" << (mono && mono->isVisible())
                  << " status=" << (dialog && dialog->findChild<QLabel*>(
                          QStringLiteral("motsArchLightingInteractiveStatus"))
                          ? dialog->findChild<QLabel*>(QStringLiteral("motsArchLightingInteractiveStatus"))
                              ->text().toStdString() : "<none>")
                  << " authoringMode=" << (view
                          ? static_cast<int>(view->architecturalLightingAuthoringMode()) : -1)
                  << " displayMode=" << (view
                          ? static_cast<int>(view->architecturalLightingDisplayModeForTest()) : -1)
                  << " undoDepth=" << window.undoDepthForTest()
                  << " expectedUndoDepth=" << beforeUndo << '\n';
    }
    check(previewLeavesAuthoredStateUntouched
            && undoUnchanged && dialogStillOpen && popupStillVisible
            && stagedPreviewActive,
        "ARCHLIGHTING FACES 0 model keeps staged Mono data and markers without publishing authored data");
    const auto stagedMonoPointColor = view
        ? view->architecturalLightingPointColorForTest(
            GlView::ArchitecturalLightingVertexIdentity{ 0, 0, 0, 0 })
        : std::optional<std::array<float, 3>>{};
    if (!stagedMonoPointColor
        || (*stagedMonoPointColor)[0] != 0.625f
        || (*stagedMonoPointColor)[1] != 0.625f
        || (*stagedMonoPointColor)[2] != 0.625f)
    {
        std::cerr << "ARCHLIGHTING Mono staged point diagnostics: color=";
        if (stagedMonoPointColor)
        {
            std::cerr << (*stagedMonoPointColor)[0] << ','
                      << (*stagedMonoPointColor)[1] << ','
                      << (*stagedMonoPointColor)[2];
        }
        else
        {
            std::cerr << "<missing>";
        }
        std::cerr << " pointCount=" << (view ? view->architecturalLightingPointCountForTest() : -1)
                  << " previewActive=" << (view && view->architecturalLightingPreviewActive())
                  << " mode=" << (view
                          ? static_cast<int>(view->architecturalLightingDisplayModeForTest()) : -1)
                  << '\n';
    }
    check(stagedMonoPointColor
            && (*stagedMonoPointColor)[0] == 0.625f
            && (*stagedMonoPointColor)[1] == 0.625f
            && (*stagedMonoPointColor)[2] == 0.625f,
        "ARCHLIGHTING Mono control updates the editor viewport while its popup remains open");

    if (dialog)
    {
        dialog->close();
        QApplication::processEvents(QEventLoop::AllEvents);
    }

    const Level& committedLevel = window.documentStateForTest().level;
    bool storedMonoAndRgb = false;
    if (!committedLevel.things.empty()
        && committedLevel.things.front().archLightingIndex >= 0
        && static_cast<std::size_t>(committedLevel.things.front().archLightingIndex)
            < committedLevel.motsArchLighting.size())
    {
        const auto& object = committedLevel.motsArchLighting[
            static_cast<std::size_t>(committedLevel.things.front().archLightingIndex)];
        if (!object.meshes.empty() && !object.meshes.front().vertices.empty())
        {
            const MotsArchLightVertex& value = object.meshes.front().vertices.front();
            storedMonoAndRgb = value.mono == 0.625f
                && std::bit_cast<std::uint32_t>(value.red)
                    == std::bit_cast<std::uint32_t>(baseline.red)
                && std::bit_cast<std::uint32_t>(value.green)
                    == std::bit_cast<std::uint32_t>(baseline.green)
                && std::bit_cast<std::uint32_t>(value.blue)
                    == std::bit_cast<std::uint32_t>(baseline.blue);
        }
    }
    const bool oneUndo = window.undoDepthForTest() == beforeUndo + 1;
    check(storedMonoAndRgb && oneUndo,
        "ARCHLIGHTING popup dismissal commits the Mono channel while preserving RGB in one Undo step");
    const auto monoPointColor = view
        ? view->architecturalLightingPointColorForTest(
            GlView::ArchitecturalLightingVertexIdentity{ 0, 0, 0, 0 })
        : std::nullopt;
    check(view && !view->architecturalLightingPreviewActive()
            && view->architecturalLightingDisplayModeForTest()
                == level::MotsArchLightingVertexMode::Mono,
        "ARCHLIGHTING Mono authoring channel stays selected after staged popup data clears");
    check(monoPointColor && (*monoPointColor)[0] == 0.625f
            && (*monoPointColor)[1] == 0.625f
            && (*monoPointColor)[2] == 0.625f,
        "ARCHLIGHTING Mono point color remains visible after the popup closes");
    if (view)
    {
        // The staging popup fixture intentionally has FACES 0. Keep that
        // vertex-only case above, then publish a separate triangle-bearing
        // model so Game Preview checks the actual 3DO mesh color rather than
        // an ARCH gizmo that is hidden in that pipeline.
        Level& renderLevel = window.mutableDocumentStateForTest().level;
        if (!renderLevel.motsArchLighting.empty()
            && !renderLevel.motsArchLighting.front().meshes.empty())
        {
            renderLevel.motsArchLighting.front().meshes.front().vertices.push_back(
                MotsArchLightVertex{ 0.375f, 0.25f, 0.5f, 0.75f });
        }
        view->primeModel3do(
            "arch_staging.3do", makeStagingTriangleInteractiveModel());
        view->setLevel(&renderLevel);
        editor::Selection renderSelection{};
        renderSelection.things.insert(0);
        window.mutableDocumentStateForTest().selection = renderSelection;
        view->setSelectionState(-1, -1, 0, -1, renderSelection);
        (void)window.refreshArchitecturalLightingOwnerForTest(0);

        const auto editorMeshColor = view->thingModelVertexColorForTest(0, 0, 0);
        if (!editorMeshColor || editorMeshColor->r != 0.625f
            || editorMeshColor->g != 0.625f || editorMeshColor->b != 0.625f)
        {
            std::cerr << "ARCHLIGHTING initial Editor mesh diagnostics: color=";
            if (editorMeshColor)
            {
                std::cerr << editorMeshColor->r << ','
                          << editorMeshColor->g << ','
                          << editorMeshColor->b;
            }
            else
            {
                std::cerr << "<missing>";
            }
            std::cerr << " active=" << view->architecturalLightingEditingActive()
                      << " authoringMode=" << static_cast<int>(view->architecturalLightingAuthoringMode())
                      << " displayMode=" << static_cast<int>(view->architecturalLightingDisplayModeForTest())
                      << " pointCount=" << view->architecturalLightingPointCountForTest()
                      << " previewActive=" << view->architecturalLightingPreviewActive()
                      << '\n';
        }
        check(editorMeshColor && editorMeshColor->r == 0.625f
                && editorMeshColor->g == 0.625f
                && editorMeshColor->b == 0.625f,
            "ARCHLIGHTING Mono authoring presentation colors the rendered 3DO mesh");

        view->setRenderPipeline(GlView::RenderPipeline::GamePreview);
        const auto previewMeshColor = view->thingModelVertexColorForTest(0, 0, 0);
        check(view->architecturalLightingDisplayModeForTest()
                == level::MotsArchLightingVertexMode::Rgb,
            "ARCHLIGHTING authoring Mono presentation is disabled in Game Preview");
        if (!previewMeshColor || previewMeshColor->r != baseline.red
            || previewMeshColor->g != baseline.green
            || previewMeshColor->b != baseline.blue)
        {
            std::cerr << "ARCHLIGHTING Game Preview mesh diagnostics: color=";
            if (previewMeshColor)
            {
                std::cerr << previewMeshColor->r << ','
                          << previewMeshColor->g << ','
                          << previewMeshColor->b;
            }
            else
            {
                std::cerr << "<missing>";
            }
            std::cerr << " pipeline=" << static_cast<int>(view->renderPipeline())
                      << " authoringMode=" << static_cast<int>(view->architecturalLightingAuthoringMode())
                      << " displayMode=" << static_cast<int>(view->architecturalLightingDisplayModeForTest())
                      << '\n';
        }
        check(previewMeshColor && previewMeshColor->r == baseline.red
                && previewMeshColor->g == baseline.green
                && previewMeshColor->b == baseline.blue,
            "ARCHLIGHTING Game Preview renders the stored RGB mesh channel");
        view->setRenderPipeline(GlView::RenderPipeline::Editor);
        const auto editorPointColor = view->architecturalLightingPointColorForTest(
            GlView::ArchitecturalLightingVertexIdentity{ 0, 0, 0, 0 });
        check(view->architecturalLightingDisplayModeForTest()
                == level::MotsArchLightingVertexMode::Mono,
            "ARCHLIGHTING Mono authoring channel resumes on return to the ARCH Editor");
        if (!editorPointColor || (*editorPointColor)[0] != 0.625f
            || (*editorPointColor)[1] != 0.625f
            || (*editorPointColor)[2] != 0.625f)
        {
            std::cerr << "ARCHLIGHTING Editor-return point diagnostics: color=";
            if (editorPointColor)
            {
                std::cerr << (*editorPointColor)[0] << ','
                          << (*editorPointColor)[1] << ','
                          << (*editorPointColor)[2];
            }
            else
            {
                std::cerr << "<missing>";
            }
            std::cerr << " pointCount=" << view->architecturalLightingPointCountForTest()
                      << " pipeline=" << static_cast<int>(view->renderPipeline())
                      << " authoringMode=" << static_cast<int>(view->architecturalLightingAuthoringMode())
                      << " displayMode=" << static_cast<int>(view->architecturalLightingDisplayModeForTest())
                      << '\n';
        }
        check(editorPointColor && (*editorPointColor)[0] == 0.625f
                && (*editorPointColor)[1] == 0.625f
                && (*editorPointColor)[2] == 0.625f,
            "ARCHLIGHTING Mono point color resumes on return to the ARCH Editor");
        const auto resumedMeshColor = view->thingModelVertexColorForTest(0, 0, 0);
        check(resumedMeshColor && resumedMeshColor->r == 0.625f
                && resumedMeshColor->g == 0.625f
                && resumedMeshColor->b == 0.625f,
            "ARCHLIGHTING Mono authoring presentation resumes on the rendered 3DO mesh");
    }

    if (oneUndo)
    {
        check(window.undoForTest(),
            "ARCHLIGHTING popup session is undone as one edit");
        const Level& undone = window.documentStateForTest().level;
        bool restored = false;
        if (!undone.motsArchLighting.empty()
            && !undone.motsArchLighting.front().meshes.empty()
            && !undone.motsArchLighting.front().meshes.front().vertices.empty())
        {
            restored = sameVertexBits(
                undone.motsArchLighting.front().meshes.front().vertices.front(),
                baseline);
        }
        check(restored,
            "ARCHLIGHTING Undo restores all original Mono and RGB channel bits");
    }
    else
    {
        check(false,
            "ARCHLIGHTING popup session creates one Undo step when dismissed");
    }
}

void runArchitecturalPopupModeStateTests()
{
    MainWindow switchWindow{};
    prepareStagingPopupWindow(
        switchWindow,
        MotsArchLightVertex{ 0.2f, 0.25f, 0.5f, 0.75f });
    switchWindow.openArchitecturalLightingVertexEditorForTest(0, 0, 0, 0);
    QApplication::processEvents(QEventLoop::AllEvents);
    QDialog* const switchDialog =
        switchWindow.architecturalLightingVertexDialogForTest();
    auto* const switchMode = switchDialog
        ? switchDialog->findChild<QComboBox*>(
            QStringLiteral("motsArchLightingInteractiveMode"))
        : nullptr;
    auto* const switchColorWidget = switchDialog
        ? switchDialog->findChild<QWidget*>(
            QStringLiteral("motsArchLightingInteractiveColor"))
        : nullptr;
    auto* const switchColor =
        dynamic_cast<ui::LiveColorPopup*>(switchColorWidget);
    const QColor rgbBefore = switchColor
        ? switchColor->currentColor()
        : QColor{};
    if (switchMode)
    {
        switchMode->setCurrentIndex(1);
        switchMode->setCurrentIndex(0);
    }
    const QColor rgbAfter = switchColor
        ? switchColor->currentColor()
        : QColor{};
    check(switchMode && switchColor && rgbBefore == rgbAfter,
        "ARCHLIGHTING RGB picker state survives a Mono-to-RGB mode switch");
    if (switchDialog)
    {
        switchDialog->reject();
        QApplication::processEvents(QEventLoop::AllEvents);
    }

    MainWindow blackWindow{};
    prepareStagingPopupWindow(
        blackWindow,
        MotsArchLightVertex{ 0.0f, 0.0f, 0.0f, 0.0f });
    blackWindow.openArchitecturalLightingVertexEditorForTest(0, 0, 0, 0);
    QApplication::processEvents(QEventLoop::AllEvents);
    QDialog* const blackDialog =
        blackWindow.architecturalLightingVertexDialogForTest();
    auto* const blackIntensity = blackDialog
        ? blackDialog->findChild<ui::ScrubFloatEdit*>(
            QStringLiteral("motsArchLightingInteractiveRgbIntensity"))
        : nullptr;
    setScrubTextAsUser(blackIntensity, QStringLiteral("0.5"));
    if (blackDialog)
    {
        blackDialog->close();
    }
    QApplication::processEvents(QEventLoop::AllEvents);
    const Level& blackLevel = blackWindow.documentStateForTest().level;
    const std::int32_t blackRecord = blackLevel.things.empty()
        ? -1
        : blackLevel.things.front().archLightingIndex;
    bool seededWhite = false;
    if (blackRecord >= 0
        && static_cast<std::size_t>(blackRecord)
            < blackLevel.motsArchLighting.size()
        && !blackLevel.motsArchLighting[static_cast<std::size_t>(blackRecord)]
                .meshes.empty()
        && !blackLevel.motsArchLighting[static_cast<std::size_t>(blackRecord)]
                .meshes.front().vertices.empty())
    {
        const MotsArchLightVertex& value = blackLevel.motsArchLighting[
            static_cast<std::size_t>(blackRecord)].meshes.front().vertices.front();
        seededWhite = value.red == 0.5f
            && value.green == 0.5f
            && value.blue == 0.5f;
    }
    check(blackIntensity && seededWhite,
        "ARCHLIGHTING intensity editing a black RGB payload starts from neutral white");
}

void runArchitecturalPopupCancellationTests()
{
    MainWindow window{};
    const MotsArchLightVertex baseline{
        0.125f, 0.25f, 0.5f, 0.75f };
    const MotsArchLightVertex staged{
        0.625f, 0.75f, 0.875f, 1.0f };
    prepareStagingPopupWindow(window, baseline);
    const std::size_t undoBefore = window.undoDepthForTest();
    window.openArchitecturalLightingVertexEditorForTest(0, 0, 0, 0);
    QApplication::processEvents(QEventLoop::AllEvents);
    QPointer<QDialog> dialog = window.architecturalLightingVertexDialogForTest();
    const bool popupOpened = dialog && dialog->isVisible();
    const bool previewStaged = popupOpened
        && window.stageArchitecturalLightingVertexForTest(
            0, 0, 0, 0, staged,
            level::MotsArchLightingVertexMode::Mono)
        && window.glViewForTest()
        && window.glViewForTest()->architecturalLightingPreviewActive();
    check(popupOpened && previewStaged,
        "ARCHLIGHTING cancellation fixture opens a popup with staged viewport preview");

    window.cancelArchitecturalLightingVertexForTest();
    const bool synchronouslyDismissed =
        window.architecturalLightingVertexDialogForTest() == nullptr
        && (!dialog || !dialog->isVisible());
    QApplication::processEvents(QEventLoop::AllEvents);
    const Level& afterCancel = window.documentStateForTest().level;
    const bool authoredValuePreserved = !afterCancel.motsArchLighting.empty()
        && !afterCancel.motsArchLighting.front().meshes.empty()
        && !afterCancel.motsArchLighting.front().meshes.front().vertices.empty()
        && sameVertexBits(
            afterCancel.motsArchLighting.front().meshes.front().vertices.front(),
            baseline);
    GlView* const view = window.glViewForTest();
    check(synchronouslyDismissed
            && authoredValuePreserved
            && window.undoDepthForTest() == undoBefore
            && view && !view->architecturalLightingPreviewActive()
            && window.architecturalLightingSelectedVertexCountForTest() == 0,
        "ARCHLIGHTING explicit cancel dismisses the popup, clears preview, and discards staged values without an Undo record");

    window.openArchitecturalLightingVertexEditorForTest(0, 0, 0, 0);
    QApplication::processEvents(QEventLoop::AllEvents);
    QDialog* const reopenedDialog =
        window.architecturalLightingVertexDialogForTest();
    const bool popupReopened = reopenedDialog && reopenedDialog->isVisible();
    check(popupReopened,
        "ARCHLIGHTING popup can reopen immediately after explicit cancellation");
    window.cancelArchitecturalLightingVertexForTest();
    QApplication::processEvents(QEventLoop::AllEvents);
}

void runArchitecturalPopupPresentationTests()
{
    MainWindow window{};
    const MotsArchLightVertex baseline{
        0.125f, -0.25f, 65504.0f, -0.0f };
    prepareStagingPopupWindow(window, baseline);
    const std::size_t beforeUndo = window.undoDepthForTest();
    window.openArchitecturalLightingVertexEditorForTest(0, 0, 0, 0);
    QApplication::processEvents(QEventLoop::AllEvents);

    QDialog* const dialog = window.architecturalLightingVertexDialogForTest();
    auto* const mode = dialog
        ? dialog->findChild<QComboBox*>(
            QStringLiteral("motsArchLightingInteractiveMode"))
        : nullptr;
    auto* const mono = dialog
        ? dialog->findChild<ui::ScrubFloatEdit*>(
            QStringLiteral("motsArchLightingInteractiveMono"))
        : nullptr;
    auto* const monoSlider = dialog
        ? dialog->findChild<QSlider*>(
            QStringLiteral("motsArchLightingInteractiveMonoSlider"))
        : nullptr;
    auto* const rgbIntensity = dialog
        ? dialog->findChild<ui::ScrubFloatEdit*>(
            QStringLiteral("motsArchLightingInteractiveRgbIntensity"))
        : nullptr;
    auto* const colorWidget = dialog
        ? dialog->findChild<QWidget*>(
            QStringLiteral("motsArchLightingInteractiveColor"))
        : nullptr;
    auto* const colorEditor =
        dynamic_cast<ui::LiveColorPopup*>(colorWidget);
    const QSize rgbSize = dialog ? dialog->size() : QSize{};

    const bool rgbPresentation = dialog && mode
        && mode->currentData().toInt()
            == static_cast<int>(level::MotsArchLightingVertexMode::Rgb)
        && colorEditor && colorEditor->isVisible()
        && rgbIntensity && rgbIntensity->isVisible()
        && mono && !mono->isVisible()
        && monoSlider && !monoSlider->isVisible();
    check(rgbPresentation,
        "ARCHLIGHTING RGB mode shows its color picker and intensity while hiding Mono controls");

    if (mode)
    {
        mode->setCurrentIndex(1);
    }
    QApplication::processEvents(QEventLoop::AllEvents);
    const bool monoPresentation = dialog && mode
        && mode->currentData().toInt()
            == static_cast<int>(level::MotsArchLightingVertexMode::Mono)
        && monoSlider && monoSlider->minimum() == 0
        && monoSlider->maximum() == 1000 && monoSlider->isVisible()
        && mono && mono->isVisible()
        && colorEditor && !colorEditor->isVisible()
        && rgbIntensity && !rgbIntensity->isVisible();
    check(monoPresentation,
        "ARCHLIGHTING Mono mode shows a 0..1000 grayscale slider and hides RGB controls");
    const QSize monoSize = dialog ? dialog->size() : QSize{};
    check(monoPresentation && !rgbSize.isEmpty()
            && monoSize.height() < rgbSize.height()
            && monoSize.height() >= dialog->sizeHint().height(),
        "ARCHLIGHTING Mono popup resizes compactly without clipping its visible controls");

    if (monoSlider)
    {
        monoSlider->setValue(500);
    }
    QApplication::processEvents(QEventLoop::AllEvents);
    check(mono && mono->value() == 0.5,
        "ARCHLIGHTING Mono slider value 500 maps exactly to numeric intensity 0.5");

    if (dialog)
    {
        dialog->close();
    }
    QApplication::processEvents(QEventLoop::AllEvents);

    const Level& afterDismissal = window.documentStateForTest().level;
    bool monoDismissalPreservedRgb = false;
    if (!afterDismissal.things.empty()
        && afterDismissal.things.front().archLightingIndex >= 0
        && static_cast<std::size_t>(afterDismissal.things.front().archLightingIndex)
            < afterDismissal.motsArchLighting.size())
    {
        const MotsArchLightObject& object = afterDismissal.motsArchLighting[
            static_cast<std::size_t>(afterDismissal.things.front().archLightingIndex)];
        if (!object.meshes.empty() && !object.meshes.front().vertices.empty())
        {
            const MotsArchLightVertex& value = object.meshes.front().vertices.front();
            monoDismissalPreservedRgb = value.mono == 0.5f
                && std::bit_cast<std::uint32_t>(value.red)
                    == std::bit_cast<std::uint32_t>(baseline.red)
                && std::bit_cast<std::uint32_t>(value.green)
                    == std::bit_cast<std::uint32_t>(baseline.green)
                && std::bit_cast<std::uint32_t>(value.blue)
                    == std::bit_cast<std::uint32_t>(baseline.blue);
        }
    }
    check(monoDismissalPreservedRgb
            && window.undoDepthForTest() == beforeUndo + 1,
        "ARCHLIGHTING popup dismissal changes only Mono and preserves signed/HDR RGB channels in one Undo step");

    check(window.undoForTest(),
        "ARCHLIGHTING popup dismissal edit can be undone");
    const Level& afterUndo = window.documentStateForTest().level;
    const bool undoRestored = !afterUndo.motsArchLighting.empty()
        && !afterUndo.motsArchLighting.front().meshes.empty()
        && !afterUndo.motsArchLighting.front().meshes.front().vertices.empty()
        && sameVertexBits(
            afterUndo.motsArchLighting.front().meshes.front().vertices.front(),
            baseline)
        && window.undoDepthForTest() == beforeUndo;
    check(undoRestored,
        "ARCHLIGHTING Undo restores exact signed/HDR Mono and RGB values");

    window.openArchitecturalLightingVertexEditorForTest(0, 0, 0, 0);
    QApplication::processEvents(QEventLoop::AllEvents);
    QDialog* const invalidDialog =
        window.architecturalLightingVertexDialogForTest();
    auto* const invalidMode = invalidDialog
        ? invalidDialog->findChild<QComboBox*>(
            QStringLiteral("motsArchLightingInteractiveMode"))
        : nullptr;
    auto* const invalidMono = invalidDialog
        ? invalidDialog->findChild<ui::ScrubFloatEdit*>(
            QStringLiteral("motsArchLightingInteractiveMono"))
        : nullptr;
    auto* const invalidStatus = invalidDialog
        ? invalidDialog->findChild<QLabel*>(
            QStringLiteral("motsArchLightingInteractiveStatus"))
        : nullptr;
    if (invalidMode)
    {
        invalidMode->setCurrentIndex(1);
    }
    setScrubTextWhilePopupOpen(invalidMono, QStringLiteral("not-a-float"));
    check(invalidStatus
            && invalidStatus->text()
                == QStringLiteral("Enter a finite float for every edited intensity."),
        "ARCHLIGHTING popup reports malformed edited channel input before dismissal");
    if (invalidDialog)
    {
        invalidDialog->close();
    }
    QApplication::processEvents(QEventLoop::AllEvents);
    const Level& afterInvalidDismissal = window.documentStateForTest().level;
    check(!afterInvalidDismissal.motsArchLighting.empty()
            && sameVertexBits(
                afterInvalidDismissal.motsArchLighting.front()
                    .meshes.front().vertices.front(),
                baseline)
            && window.undoDepthForTest() == beforeUndo,
        "ARCHLIGHTING invalid popup input is discarded on dismissal without changing history");
}

void runInteractiveMultiVertexTransactionTests()
{
    MainWindow window{};
    prepareStagingMultiOwnerPopupWindow(window);
    const std::vector<std::array<std::int32_t, 4>> selections{
        std::array<std::int32_t, 4>{ 0, 0, 0, 0 },
        std::array<std::int32_t, 4>{ 0, 0, 0, 1 },
        std::array<std::int32_t, 4>{ 1, 0, 0, 0 },
        std::array<std::int32_t, 4>{ 1, 0, 0, 1 } };
    const std::size_t beforeUndo = window.undoDepthForTest();
    check(window.prepareArchitecturalLightingVertexSelectionForTest(selections)
            && window.architecturalLightingSelectedVertexCountForTest() == 4,
        "ARCHLIGHTING popup selection retains multiple vertex identities across selected owners");

    window.openArchitecturalLightingVertexEditorsForTest(selections);
    QApplication::processEvents(QEventLoop::AllEvents);
    QDialog* const dialog = window.architecturalLightingVertexDialogForTest();
    QLabel* const status = dialog
        ? dialog->findChild<QLabel*>(QStringLiteral("motsArchLightingInteractiveStatus"))
        : nullptr;
    check(dialog && status
            && status->text()
                == QStringLiteral(
                    "Mixed values across selected vertices; edit a channel to apply it to all selected vertices."),
        "ARCHLIGHTING mixed multi-vertex values remain explicit until a channel is edited");
    if (dialog)
    {
        for (ui::ScrubFloatEdit* const control :
             dialog->findChildren<ui::ScrubFloatEdit*>())
        {
            control->setFocus();
            control->clearFocus();
        }
        QApplication::processEvents(QEventLoop::AllEvents);
        dialog->close();
    }
    QApplication::processEvents(QEventLoop::AllEvents);
    check(window.undoDepthForTest() == beforeUndo
            && window.documentStateForTest().level.motsArchLighting.size() == 1
            && sameVertexBits(
                window.documentStateForTest().level.motsArchLighting.front()
                    .meshes.front().vertices.front(),
                MotsArchLightVertex{ 0.125f, 0.25f, 0.5f, 0.75f }),
        "ARCHLIGHTING focus-only traversal of a mixed popup is a safe no-op");

    check(window.prepareArchitecturalLightingVertexSelectionForTest(selections),
        "ARCHLIGHTING popup prepares the selected vertex identities for editing");
    window.openArchitecturalLightingVertexEditorsForTest(selections);
    QApplication::processEvents(QEventLoop::AllEvents);
    QDialog* const editDialog = window.architecturalLightingVertexDialogForTest();
    auto* const monoEdit = editDialog
        ? editDialog->findChild<ui::ScrubFloatEdit*>(
            QStringLiteral("motsArchLightingInteractiveMono"))
        : nullptr;
    auto* const editMode = editDialog
        ? editDialog->findChild<QComboBox*>(
            QStringLiteral("motsArchLightingInteractiveMode"))
        : nullptr;
    if (editMode)
    {
        editMode->setCurrentIndex(1);
    }
    setScrubTextAsUser(monoEdit, QStringLiteral("0.9"));
    if (editDialog)
    {
        editDialog->close();
    }
    QApplication::processEvents(QEventLoop::AllEvents);
    check(window.documentStateForTest().level.things[0].archLightingIndex > 0,
        "ARCHLIGHTING multi-vertex popup commits one explicit Mono edit atomically");
    const Level& committed = window.documentStateForTest().level;
    check(committed.things.size() == 3
            && committed.things[0].archLightingIndex > 0
            && committed.things[0].archLightingIndex
                == committed.things[1].archLightingIndex
            && committed.things[2].archLightingIndex == 0
            && committed.motsArchLighting.size() == 2,
        "ARCHLIGHTING multi-owner edit preserves one shared COW clone and untouched owners");
    if (committed.things.size() == 3
        && committed.things[0].archLightingIndex > 0
        && committed.things[0].archLightingIndex
            == committed.things[1].archLightingIndex)
    {
        const MotsArchLightObject& edited = committed.motsArchLighting[
            static_cast<std::size_t>(committed.things[0].archLightingIndex)];
        check(edited.meshes.size() == 1
                && edited.meshes[0].vertices.size() == 2
                && sameVertexBits(
                    edited.meshes[0].vertices[0],
                    MotsArchLightVertex{ 0.9f, 0.25f, 0.5f, 0.75f })
                && sameVertexBits(
                    edited.meshes[0].vertices[1],
                    MotsArchLightVertex{ 0.9f, 0.75f, 0.5f, 0.25f }),
            "ARCHLIGHTING multi-vertex Mono edit preserves untouched RGB channels for every selected vertex");
    }

    check(window.undoForTest(),
        "ARCHLIGHTING multi-vertex edit has one undo record");
    const Level& undone = window.documentStateForTest().level;
    check(undone.things.size() == 3
            && undone.things[0].archLightingIndex == 0
            && undone.things[1].archLightingIndex == 0
            && undone.things[2].archLightingIndex == 0,
        "ARCHLIGHTING undo restores all shared owner bindings atomically");
    check(window.redoForTest(),
        "ARCHLIGHTING multi-vertex edit has one redo record");
}

void runInteractiveVirtualDefaultTransactionTests()
{
    MainWindow window{};
    prepareStagingPopupWindow(
        window, MotsArchLightVertex{ 0.0f, 0.0f, 0.0f, 0.0f });
    editor::DocumentState& state = window.mutableDocumentStateForTest();
    state.level.things[0].archLightingIndex = -1;
    GlView* const view = window.glViewForTest();
    if (view)
    {
        view->setLevel(&state.level);
        view->setArchitecturalLightingEditingActive(true);
        // setLevel(resetView) clears viewport selection. Republish the owner
        // after switching this fixture from its explicit record to the
        // virtual all-zero default so the selected-owner cache used by
        // interactive vertex editing is available.
        editor::Selection selection{};
        selection.things.insert(0);
        state.selection = selection;
        view->setSelectionState(-1, -1, 0, -1, selection);
    }
    check(view && window.refreshArchitecturalLightingOwnerForTest(0),
        "ARCHLIGHTING virtual-default fixture resolves the selected model owner");

    const auto rendered = view
        ? view->architecturalLightingVerticesForThing(0)
        : std::span<const GlView::ArchitecturalLightingRenderVertex>{};
    check(state.level.things[0].archLightingIndex == -1
            && rendered.size() == 2
            && std::all_of(rendered.begin(), rendered.end(),
                [](const GlView::ArchitecturalLightingRenderVertex& vertex)
                {
                    return vertex.lighting == MotsArchLightVertex{};
                }),
        "an unbound effective Arch Thing exposes every model vertex at zero without a stored record");

    const std::vector<std::array<std::int32_t, 4>> selections{
        std::array<std::int32_t, 4>{ 0, 0, 0, 0 },
        std::array<std::int32_t, 4>{ 0, 0, 0, 1 } };
    check(window.prepareArchitecturalLightingVertexSelectionForTest(selections)
            && window.architecturalLightingSelectedVertexCountForTest() == 2,
        "ARCHLIGHTING virtual-default owner allows selecting multiple model vertices");

    const std::size_t beforeUndo = window.undoDepthForTest();
    window.openArchitecturalLightingVertexEditorsForTest(selections);
    QApplication::processEvents(QEventLoop::AllEvents);
    QDialog* const dialog = window.architecturalLightingVertexDialogForTest();
    auto* const identity = dialog
        ? dialog->findChild<QLabel*>(QStringLiteral("motsArchLightingVertexIdentity"))
        : nullptr;
    auto* const mono = dialog
        ? dialog->findChild<ui::ScrubFloatEdit*>(
            QStringLiteral("motsArchLightingInteractiveMono"))
        : nullptr;
    auto* const mode = dialog
        ? dialog->findChild<QComboBox*>(
            QStringLiteral("motsArchLightingInteractiveMode"))
        : nullptr;
    check(dialog && identity && mono && mode
            && identity->text() == QStringLiteral("2 selected vertices across 1 Things")
            && mono->value() == 0.0,
        "ARCHLIGHTING multi-vertex popup reads the zero virtual default for every selected vertex");
    if (mode)
    {
        mode->setCurrentIndex(1);
    }
    setScrubTextAsUser(mono, QStringLiteral("0.5"));
    if (dialog)
    {
        dialog->close();
    }
    QApplication::processEvents(QEventLoop::AllEvents);

    const Level& committed = window.documentStateForTest().level;
    check(committed.things[0].archLightingIndex == 1
            && committed.motsArchLighting.size() == 2
            && committed.motsArchLighting[1].meshes.size() == 1
            && committed.motsArchLighting[1].meshes[0].vertices.size() == 2
            && sameVertexBits(
                committed.motsArchLighting[1].meshes[0].vertices[0],
                MotsArchLightVertex{ 0.5f, 0.0f, 0.0f, 0.0f })
            && sameVertexBits(
                committed.motsArchLighting[1].meshes[0].vertices[1],
                MotsArchLightVertex{ 0.5f, 0.0f, 0.0f, 0.0f })
            && window.undoDepthForTest() == beforeUndo + 1,
        "editing virtual-default vertices materializes one zero-based COW record for all selected vertices");
    check(window.undoForTest()
            && window.documentStateForTest().level.things[0].archLightingIndex == -1
            && window.documentStateForTest().level.motsArchLighting.size() == 1,
        "ARCHLIGHTING virtual-default edit Undo restores its implicit unbound state");
    check(window.redoForTest()
            && window.documentStateForTest().level.things[0].archLightingIndex == 1
            && window.documentStateForTest().level.motsArchLighting.size() == 2
            && sameVertexBits(
                window.documentStateForTest().level.motsArchLighting[1]
                    .meshes[0].vertices[1],
                MotsArchLightVertex{ 0.5f, 0.0f, 0.0f, 0.0f }),
        "ARCHLIGHTING virtual-default edit Redo restores the materialized multi-vertex record");
}

void runInteractiveArchitecturalClipboardAndResetTests()
{
    MainWindow defaultWindow{};
    prepareStagingClipboardWindow(defaultWindow);
    const std::size_t defaultUndo = defaultWindow.undoDepthForTest();
    QAction* const addAction = defaultWindow.findChild<QAction*>(
        QStringLiteral("viewport.arch_lighting.add"));
    const Level& defaultLevel = defaultWindow.documentStateForTest().level;
    check(addAction == nullptr
            && defaultLevel.things.size() == 3
            && defaultLevel.things[2].archLightingIndex == -1
            && defaultLevel.motsArchLighting.size() == 1
            && defaultWindow.undoDepthForTest() == defaultUndo,
        "ARCHLIGHTING uses the implicit zero default without an Add placement action or document mutation");

    MainWindow deleteWindow{};
    prepareStagingClipboardWindow(deleteWindow);
    editor::Selection& deleteSelection =
        deleteWindow.mutableDocumentStateForTest().selection;
    deleteSelection.clear();
    deleteSelection.things.insert(1);
    if (GlView* const view = deleteWindow.glViewForTest())
    {
        view->setSelectedThing(1);
        view->setSelectionState(-1, -1, 1, -1, deleteSelection);
    }
    deleteWindow.updateEditActionsForTest();
    const std::size_t deleteUndo = deleteWindow.undoDepthForTest();
    QAction* const deleteAction = deleteWindow.findChild<QAction*>(
        QStringLiteral("edit.delete"));
    if (deleteAction)
    {
        deleteAction->trigger();
    }
    check(deleteAction && deleteAction->isEnabled()
            && deleteWindow.documentStateForTest().level.things.size() == 3
            && deleteWindow.documentStateForTest().level.things[1].archLightingIndex == 1
            && deleteWindow.documentStateForTest().level.motsArchLighting.size() == 2
            && level::isDefaultMotsArchLighting(
                deleteWindow.documentStateForTest().level.motsArchLighting[1])
            && deleteWindow.undoDepthForTest() == deleteUndo + 1,
        "ARCHLIGHTING Delete resets the selected binding to zero while preserving its Thing");
    check(deleteWindow.undoForTest()
            && deleteWindow.documentStateForTest().level.things[1].archLightingIndex == 0
            && deleteWindow.documentStateForTest().level.motsArchLighting.size() == 1
            && deleteWindow.redoForTest()
            && deleteWindow.documentStateForTest().level.things[1].archLightingIndex == 1
            && deleteWindow.documentStateForTest().level.motsArchLighting.size() == 2
            && level::isDefaultMotsArchLighting(
                deleteWindow.documentStateForTest().level.motsArchLighting[1]),
        "ARCHLIGHTING Delete reset is one undoable operation");

    MainWindow pasteWindow{};
    prepareStagingClipboardWindow(pasteWindow);
    const std::size_t pasteUndo = pasteWindow.undoDepthForTest();
    QAction* const copyAction = pasteWindow.findChild<QAction*>(
        QStringLiteral("edit.copy_entities"));
    QMenu* editMenu = nullptr;
    for (QAction* const menuAction : pasteWindow.menuBar()->actions())
    {
        if (menuAction && menuAction->menu()
            && menuAction->text().remove('&').compare(
                QStringLiteral("Edit"), Qt::CaseInsensitive) == 0)
        {
            editMenu = menuAction->menu();
            break;
        }
    }
    check(editMenu && copyAction && editMenu->actions().contains(copyAction),
        "ARCHLIGHTING clipboard Copy is installed on the production Edit menu");
    const bool copiedBefore = copyAction && copyAction->isEnabled();
    if (QWidget* const focused = QApplication::focusWidget())
    {
        focused->clearFocus();
    }
    QKeyEvent copyPress(
        QEvent::KeyPress,
        Qt::Key_C,
        Qt::ControlModifier,
        QStringLiteral("c"));
    const bool copyConsumed = pasteWindow.glViewForTest()
        && pasteWindow.eventFilterForTest(
            pasteWindow.glViewForTest(), &copyPress);
    const bool copied = copiedBefore
        && copyConsumed
        && pasteWindow.documentStateForTest().level.things[0].archLightingIndex == -1
        && pasteWindow.documentStateForTest().level.things[1].archLightingIndex == 0;
    check(copied
            && pasteWindow.undoDepthForTest() == pasteUndo
            && pasteWindow.documentStateForTest().level.things[0].archLightingIndex == -1
            && pasteWindow.documentStateForTest().level.things[1].archLightingIndex == 0
            && pasteWindow.documentStateForTest().level.things[2].archLightingIndex == -1,
        "ARCHLIGHTING Ctrl+C snapshots the active higher-index source without changing the document");

    editor::DocumentState& changedSource =
        pasteWindow.mutableDocumentStateForTest();
    changedSource.level.motsArchLighting.front().meshes.front().vertices.front() =
        MotsArchLightVertex{ 9.0f, 8.0f, 7.0f, 6.0f };
    changedSource.selection.clear();
    changedSource.selection.things.insert(0);
    if (GlView* const view = pasteWindow.glViewForTest())
    {
        view->setSelectedThing(0);
        view->setSelectionState(-1, -1, 0, -1, changedSource.selection);
    }
    pasteWindow.updateEditActionsForTest();
    QAction* const pasteAction = pasteWindow.findChild<QAction*>(
        QStringLiteral("edit.paste_entities"));
    QKeyEvent pastePress(
        QEvent::KeyPress,
        Qt::Key_V,
        Qt::ControlModifier,
        QStringLiteral("v"));
    const bool pasteConsumed = pasteWindow.glViewForTest()
        && pasteWindow.eventFilterForTest(
            pasteWindow.glViewForTest(), &pastePress);
    check(pasteAction && pasteAction->isEnabled()
            && pasteConsumed
            && pasteWindow.documentStateForTest().level.things[0].archLightingIndex > 0
            && pasteWindow.documentStateForTest().level.things[1].archLightingIndex == 0
            && pasteWindow.documentStateForTest().level.motsArchLighting.size() == 2
            && sameVertexBits(
                pasteWindow.documentStateForTest().level.motsArchLighting[1]
                    .meshes.front().vertices.front(),
                MotsArchLightVertex{ 0.125f, 0.25f, 0.5f, 0.75f })
            && pasteWindow.undoDepthForTest() == pasteUndo + 1,
        "ARCHLIGHTING Ctrl+V uses the copied payload after the source changes and targets the lower Thing");
    check(pasteWindow.undoForTest()
            && pasteWindow.documentStateForTest().level.things[0].archLightingIndex == -1
            && pasteWindow.documentStateForTest().level.things[1].archLightingIndex == 0
            && pasteWindow.documentStateForTest().level.motsArchLighting.size() == 1
            && pasteWindow.redoForTest()
            && pasteWindow.documentStateForTest().level.things[0].archLightingIndex > 0,
        "ARCHLIGHTING clipboard paste has one atomic undo and redo");

    const std::size_t beforeNoopPasteUndo = pasteWindow.undoDepthForTest();
    const Level beforeNoopPaste = pasteWindow.documentStateForTest().level;
    const bool noopPasteAccepted =
        pasteWindow.pasteArchitecturalLightingClipboardForTest();
    const Level& afterNoopPaste = pasteWindow.documentStateForTest().level;
    check(noopPasteAccepted
            && pasteWindow.undoDepthForTest() == beforeNoopPasteUndo
            && afterNoopPaste.things.size() == beforeNoopPaste.things.size()
            && afterNoopPaste.motsArchLighting.size()
                == beforeNoopPaste.motsArchLighting.size()
            && std::equal(
                afterNoopPaste.things.begin(),
                afterNoopPaste.things.end(),
                beforeNoopPaste.things.begin(),
                [](const Thing& left, const Thing& right)
                {
                    return left.archLightingIndex == right.archLightingIndex;
                }),
        "ARCHLIGHTING clipboard accepts an already-bound no-op without history or mutation");

    editor::DocumentState& lockedTarget = pasteWindow.mutableDocumentStateForTest();
    lockedTarget.level.things[2].layer = 1;
    lockedTarget.level.things[2].archLightingIndex = 0;
    lockedTarget.selection.clear();
    lockedTarget.selection.things.insert(2);
    if (GlView* const view = pasteWindow.glViewForTest())
    {
        view->setSelectedThing(2);
        view->setSelectionState(-1, -1, 2, -1, lockedTarget.selection);
    }
    pasteWindow.setLayerVisualStateForTest({ true, true }, { false, true });
    pasteWindow.updateEditActionsForTest();
    const Level beforeLockedPaste = pasteWindow.documentStateForTest().level;
    const std::size_t beforeLockedUndo = pasteWindow.undoDepthForTest();
    check(!pasteWindow.pasteArchitecturalLightingClipboardForTest()
            && pasteWindow.undoDepthForTest() == beforeLockedUndo
            && pasteWindow.documentStateForTest().level.things.size()
                == beforeLockedPaste.things.size()
            && pasteWindow.documentStateForTest().level.things[0].archLightingIndex
                == beforeLockedPaste.things[0].archLightingIndex
            && pasteWindow.documentStateForTest().level.things[1].archLightingIndex
                == beforeLockedPaste.things[1].archLightingIndex
            && pasteWindow.documentStateForTest().level.things[2].archLightingIndex
                == beforeLockedPaste.things[2].archLightingIndex
            && pasteWindow.documentStateForTest().level.motsArchLighting.size()
                == beforeLockedPaste.motsArchLighting.size()
            && sameVertexBits(
                pasteWindow.documentStateForTest().level.motsArchLighting.front()
                    .meshes.front().vertices.front(),
                beforeLockedPaste.motsArchLighting.front().meshes.front().vertices.front()),
        "ARCHLIGHTING clipboard paste rejects a locked target atomically");
}

void runArchFlagPromotionTests()
{
    const auto setThingFlags = [](Thing& thing, const std::int32_t flags)
    {
        for (auto it = thing.values.rbegin(); it != thing.values.rend(); ++it)
        {
            if (it->name == "thingflags")
            {
                it->type = ValueType::integer;
                it->intValue = flags;
                it->stringValue.clear();
                return;
            }
        }
    };

    MainWindow initialize{};
    prepareStagingClipboardWindow(initialize);
    editor::DocumentState& initializeState = initialize.mutableDocumentStateForTest();
    setThingFlags(initializeState.level.things[2], 0x00000100);
    initializeState.selection.clear();
    initializeState.selection.things.insert(2);
    if (GlView* const view = initialize.glViewForTest())
    {
        view->setSelectedThing(2);
        view->setSelectionState(-1, -1, 2, -1, initializeState.selection);
    }
    const std::size_t initializeDepth = initialize.undoDepthForTest();
    const std::size_t initializeRecords = initializeState.level.motsArchLighting.size();
    check(initialize.initializeArchitecturalLightingForTest()
            && initialize.undoDepthForTest() == initializeDepth + 1
            && initialize.documentStateForTest().level.things[2].archLightingIndex
                >= static_cast<std::int32_t>(initializeRecords)
            && sed::level::thingFlags(
                initialize.documentStateForTest().level,
                nullptr,
                initialize.documentStateForTest().level.things[2]) == 0x00000108
            && initialize.documentStateForTest().level.motsArchLighting.size()
                == initializeRecords + 1
            && sed::level::isDefaultMotsArchLighting(
                initialize.documentStateForTest().level.motsArchLighting.back()),
        "ARCHLIGHTING Initialize binds a zero default and promotes an unflagged model Thing");
    const std::int32_t initializedIndex = initialize.documentStateForTest()
        .level.things[2].archLightingIndex;
    check(initialize.undoForTest()
            && initialize.documentStateForTest().level.things[2].archLightingIndex == -1
            && sed::level::thingFlags(
                initialize.documentStateForTest().level,
                nullptr,
                initialize.documentStateForTest().level.things[2]) == 0x00000100
            && initialize.documentStateForTest().level.motsArchLighting.size()
                == initializeRecords,
        "ARCHLIGHTING Initialize Undo restores the unflagged Thing and record table");
    check(initialize.redoForTest()
            && initialize.documentStateForTest().level.things[2].archLightingIndex
                == initializedIndex
            && sed::level::thingFlags(
                initialize.documentStateForTest().level,
                nullptr,
                initialize.documentStateForTest().level.things[2]) == 0x00000108,
        "ARCHLIGHTING Initialize Redo restores the default and required flag");

    MainWindow paste{};
    prepareStagingClipboardWindow(paste);
    editor::DocumentState& pasteState = paste.mutableDocumentStateForTest();
    setThingFlags(pasteState.level.things[2], 0x00000100);
    pasteState.selection.clear();
    pasteState.selection.things.insert(1);
    if (GlView* const view = paste.glViewForTest())
    {
        view->setSelectedThing(1);
        view->setSelectionState(-1, -1, 1, -1, pasteState.selection);
    }
    check(paste.copyArchitecturalLightingClipboardForTest(),
        "ARCHLIGHTING Copy snapshots a source before promoting an unflagged target");
    pasteState.selection.clear();
    pasteState.selection.things.insert(2);
    if (GlView* const view = paste.glViewForTest())
    {
        view->setSelectedThing(2);
        view->setSelectionState(-1, -1, 2, -1, pasteState.selection);
    }
    const std::size_t pasteDepth = paste.undoDepthForTest();
    check(paste.pasteArchitecturalLightingClipboardForTest()
            && paste.undoDepthForTest() == pasteDepth + 1
            && paste.documentStateForTest().level.things[2].archLightingIndex >= 0
            && sed::level::thingFlags(
                paste.documentStateForTest().level,
                nullptr,
                paste.documentStateForTest().level.things[2]) == 0x00000108,
        "ARCHLIGHTING Paste promotes a compatible unbound model Thing in the same transaction");
    check(paste.undoForTest()
            && paste.documentStateForTest().level.things[2].archLightingIndex == -1
            && sed::level::thingFlags(
                paste.documentStateForTest().level,
                nullptr,
                paste.documentStateForTest().level.things[2]) == 0x00000100,
        "ARCHLIGHTING Paste Undo restores the target flags and binding atomically");
}

} // namespace

void runMotsArchLightingStagingTests()
{
    runSparseEntityIdTests();
    runPendingStateTests();
    runSelectionAndResetTests();
    runExplicitVertexModeTests();
    runValidationStringTests();
    runInteractiveModeSeamTests();
    runInteractiveModeActionTests();
    runArchitecturalRecoveryCompatibilityTests();
    runInteractivePopupSizingTests();
    runUnifiedArchitecturalPopupContractTests();
    runArchitecturalLightFamilyVisibilityTests();
    runArchitecturalPreviewIdentityInvalidationTests();
    runArchitecturalMonoCommitPresentationTests();
    runArchitecturalPopupModeStateTests();
    runArchitecturalPopupCancellationTests();
    runArchitecturalPopupPresentationTests();
    runInteractiveMultiVertexTransactionTests();
    runInteractiveVirtualDefaultTransactionTests();
    runInteractiveArchitecturalClipboardAndResetTests();
    runArchFlagPromotionTests();
}

} // namespace sed::app_tests
