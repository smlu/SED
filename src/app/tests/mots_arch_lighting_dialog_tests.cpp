#include "test_common.hpp"

#include "sed/assets/3do.hpp"
#include "gl_view.hpp"
#include "render/gizmos/gizmo_icon_utils.hpp"
#include "sed/assets/resource_locator.hpp"
#include "main_window/main_window.hpp"
#include "sed/editor/document.hpp"
#include "sed/level/arch_lighting.hpp"
#include "ui/components/scrub_float_edit.hpp"
#include "ui/components/live_color_popup.hpp"
#include "ui/lighting/mots_arch_lighting_dialog.hpp"

#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFontMetrics>
#include <QGroupBox>
#include <QImage>
#include <QLabel>
#include <QLayout>
#include <QLineEdit>
#include <QMargins>
#include <QPointer>
#include <QMetaObject>
#include <QPushButton>
#include <QTimer>
#include <QToolButton>
#include <QStyle>
#include <QWidget>

#include <algorithm>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
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
using ui::lighting::MotsArchLightingStaging;
using ui::lighting::MotsArchLightingTarget;

MotsArchLightObject makeArchiveObject(
    const std::int32_t wireIndex,
    const MotsArchLightVertex first,
    const MotsArchLightVertex second)
{
    MotsArchLightMesh mesh{};
    mesh.wireIndex = 0;
    mesh.vertices = { first, second };

    MotsArchLightObject object{};
    object.wireIndex = wireIndex;
    object.meshes.push_back(std::move(mesh));
    return object;
}

MotsArchLightingTarget makeTarget(
    const std::int32_t thingIndex,
    const std::int32_t originalRecordIndex,
    const std::string& revision,
    const bool valueEditingAllowed,
    const bool modelAvailable = true)
{
    MotsArchLightingTarget target{};
    target.thingIndex = thingIndex;
    target.label = "Thing " + std::to_string(thingIndex);
    target.modelName = "arch_probe.3do";
    target.resourceRevision = revision;
    target.topology.geosets = { { 2 } };
    target.originalRecordIndex = originalRecordIndex;
    target.hasArchFlag = true;
    target.modelAvailable = modelAvailable;
    target.valueEditingAllowed = valueEditingAllowed;
    return target;
}

MotsArchLightingRecordStage makeStage(
    const std::int32_t originalRecordIndex,
    const std::optional<MotsArchLightObject>& originalRecord)
{
    MotsArchLightingRecordStage stage{};
    stage.originalRecordIndex = originalRecordIndex;
    stage.originalRecord = originalRecord;
    stage.stagedRecord = originalRecord;
    return stage;
}

MotsArchLightingPlan makePlan()
{
    const MotsArchLightObject shared = makeArchiveObject(
        10,
        MotsArchLightVertex{ 0.25f, 2.0f, 0.0f, 4.0f },
        MotsArchLightVertex{ 0.5f, 3.0f, 5.0f, 7.0f });
    MotsArchLightingPlan plan{};
    plan.project = ProjectType::MOTS;
    plan.resourceLocatorIdentity = 42;
    plan.records = {
        makeStage(0, shared),
    };
    plan.targets = {
        makeTarget(0, 0, "3DO|shared-v1", true),
        makeTarget(1, 0, "3DO|shared-v1", true),
        makeTarget(2, 1, "3DO|missing", false, false),
        makeTarget(3, -1, "3DO|new-v1", true),
        makeTarget(4, 0, "3DO|shared-v1", true),
    };
    plan.targets[0].recordStageIndex = 0;
    plan.targets[1].recordStageIndex = 0;
    plan.targets[4].recordStageIndex = 0;
    return plan;
}

Level makeLiveLevel()
{
    Level level{};
    level.kind = ProjectType::MOTS;
    level.header.version = 1;
    level.things.resize(5);

    TplValue archFlags{};
    archFlags.name = "thingflags";
    archFlags.type = ValueType::integer;
    archFlags.intValue = 0x8;
    for (Thing& thing : level.things)
    {
        thing.name = "arch_probe";
        thing.values.push_back(archFlags);
    }
    for (std::size_t index = 0; index < level.things.size(); ++index)
    {
        level.things[index].archLightingIndex = index == 2 ? 1 : index == 3 ? -1 : 0;
    }

    level.motsArchLighting.push_back(makeArchiveObject(
        10,
        MotsArchLightVertex{ 0.25f, 2.0f, 0.0f, 4.0f },
        MotsArchLightVertex{ 0.5f, 3.0f, 5.0f, 7.0f }));
    level.motsArchLighting.push_back(makeArchiveObject(
        11,
        MotsArchLightVertex{ 0.0f, 0.0f, 0.0f, 0.0f },
        MotsArchLightVertex{ 0.0f, 0.0f, 0.0f, 0.0f }));
    return level;
}

Level makeModelLiveLevel()
{
    Level level = makeLiveLevel();
    for (Thing& thing : level.things)
    {
        TplValue model3d{};
        model3d.name = "model3d";
        model3d.type = ValueType::str;
        model3d.stringValue = "arch_probe.3do";
        thing.values.push_back(std::move(model3d));
    }
    return level;
}

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
        "NAME arch_probe\n"
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

std::vector<std::byte> sharedHierarchyModelFixtureBytes()
{
    // Two hierarchy nodes deliberately reference one authored mesh. A raw
    // ARCH topology must retain one source mesh; a hierarchy-expanded load
    // would incorrectly publish two clone meshes.
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
        "NAME arch_probe\n"
        "VERTICES 2\n"
        "0: 0 0 0\n"
        "1: 1 0 0\n"
        "FACES 0\n"
        "SECTION: HIERARCHYDEF\n"
        "HIERARCHY NODES 2\n"
        "0: 0x0 0x1 0 -1 -1 -1 0 0 0 0 0 0 0 0 0 0 arch_probe\n"
        "1: 0x0 0x1 0 -1 -1 -1 0 2 0 0 0 0 0 0 0 0 0 arch_probe_clone\n";
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
    mesh.name = "arch_probe";
    mesh.geo = static_cast<std::int32_t>(TGeoMode::Solid);
    mesh.light = static_cast<std::int32_t>(TLightMode::VertexDiffuseOnly);
    mesh.vertices = {
        assets::Model3doVertex{ 0.0, 0.0, 0.0, ColorWhite },
        assets::Model3doVertex{ 1.0, 0.0, 0.0, ColorWhite } };
    model->meshes.push_back(std::move(mesh));
    return model;
}

void prepareInteractiveWindow(
    MainWindow& window,
    const MotsArchLightVertex firstVertex)
{
    window.setAttribute(Qt::WA_DontShowOnScreen);
    window.setPrimaryResourceOverrideForTest(
        "arch_probe.3do",
        []()
        {
            return std::optional<std::vector<std::byte>>(modelFixtureBytes());
        });
    editor::DocumentState state{};
    state.level = makeModelLiveLevel();
    state.level.motsArchLighting[0].meshes[0].vertices[0] = firstVertex;
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
    view->primeModel3do("arch_probe.3do", makeInteractiveModel());
    view->setLevel(&window.documentStateForTest().level);
    view->setArchitecturalLightingEditingActive(true);
    view->setSelectedThing(0);
    editor::Selection& selection = window.mutableDocumentStateForTest().selection;
    selection.things.insert(0);
    view->setSelectionState(-1, -1, 0, -1, selection);
    (void)window.refreshArchitecturalLightingOwnerForTest(0);
}

bool sameFloatBits(const float left, const float right) noexcept
{
    return std::bit_cast<std::uint32_t>(left) == std::bit_cast<std::uint32_t>(right);
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

std::optional<MotsArchLightVertex> thingArchVertex(
    const MainWindow& window,
    const std::int32_t thingIndex = 0,
    const std::int32_t meshIndex = 0,
    const std::int32_t vertexIndex = 0)
{
    const Level& level = window.documentStateForTest().level;
    if (thingIndex < 0
        || static_cast<std::size_t>(thingIndex) >= level.things.size())
    {
        return std::nullopt;
    }
    const std::int32_t recordIndex = level.things[
        static_cast<std::size_t>(thingIndex)].archLightingIndex;
    if (recordIndex < 0
        || static_cast<std::size_t>(recordIndex) >= level.motsArchLighting.size())
    {
        return std::nullopt;
    }
    const MotsArchLightObject& object = level.motsArchLighting[
        static_cast<std::size_t>(recordIndex)];
    if (meshIndex < 0
        || static_cast<std::size_t>(meshIndex) >= object.meshes.size())
    {
        return std::nullopt;
    }
    const MotsArchLightMesh& mesh = object.meshes[
        static_cast<std::size_t>(meshIndex)];
    if (vertexIndex < 0
        || static_cast<std::size_t>(vertexIndex) >= mesh.vertices.size())
    {
        return std::nullopt;
    }
    return mesh.vertices[static_cast<std::size_t>(vertexIndex)];
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
    // ScrubFloatEdit deliberately distinguishes programmatic display updates
    // from authored text.  Emit the same signal a real key edit produces so
    // this test exercises the production dirty-channel path.
    QMetaObject::invokeMethod(
        edit->lineEdit(),
        "textEdited",
        Qt::DirectConnection,
        Q_ARG(QString, text));
    // A focus transition normally emits this signal, but the offscreen test
    // windows do not always have another focusable widget. Invoke it
    // explicitly so the edit is committed through ScrubFloatEdit's normal
    // user-text path rather than only changing the line edit display.
    QMetaObject::invokeMethod(
        edit->lineEdit(),
        "editingFinished",
        Qt::DirectConnection);
    edit->lineEdit()->clearFocus();
    QApplication::processEvents(QEventLoop::AllEvents);
}

void runBulkScrubFieldGeometryTests()
{
    // The bulk dialog places two ScrubFloatEdit widgets in each grid row.  On
    // a narrow grid column the hover steppers used to consume the line edit's
    // entire text area, which hid the sign and decimal tail of HDR values.
    // Exercise the real dialog and all eight fields so the sizing contract is
    // checked independently from the staging/Apply paths below.
    Level level = makeLiveLevel();
    ui::lighting::MotsArchLightingDialog dialog(level, nullptr, nullptr);
    check(dialog.windowTitle() == QStringLiteral("Arch lighting"),
        "ARCHLIGHTING dialog uses the Arch name in its title");
    const auto* description = dialog.findChild<QLabel*>(
        QStringLiteral("motsArchLightingDescription"));
    const auto* targetsGroup = dialog.findChild<QGroupBox*>(
        QStringLiteral("motsArchLightingTargetsGroup"));
    check(description
            && description->text()
                == QStringLiteral("Edit MotS ARCHLIGHTING for Arch Things. Apply commits one edit in one undo step.")
            && targetsGroup
            && targetsGroup->title() == QStringLiteral("Arch Things"),
        "ARCHLIGHTING dialog names its eligible targets as Arch Things");
    dialog.setAttribute(Qt::WA_DontShowOnScreen);
    dialog.show();
    QApplication::processEvents(QEventLoop::AllEvents);
    if (dialog.layout())
    {
        dialog.layout()->activate();
    }
    dialog.adjustSize();
    QApplication::processEvents(QEventLoop::AllEvents);

    constexpr std::array<const char*, 8> fieldNames{
        "motsArchLightingMono",
        "motsArchLightingRed",
        "motsArchLightingGreen",
        "motsArchLightingBlue",
        "motsArchLightingMonoMultiplier",
        "motsArchLightingRedMultiplier",
        "motsArchLightingGreenMultiplier",
        "motsArchLightingBlueMultiplier" };
    const QString signedHdrText = QStringLiteral("-1000000.000000000");

    for (const char* const fieldName : fieldNames)
    {
        auto* const edit = dialog.findChild<ui::ScrubFloatEdit*>(
            QString::fromLatin1(fieldName));
        check(edit != nullptr,
            "ARCHLIGHTING bulk dialog exposes every numeric scrub field");
        if (!edit || !edit->lineEdit())
        {
            continue;
        }

        edit->setValue(-1'000'000.0);
        QEvent enter(QEvent::Enter);
        QApplication::sendEvent(edit->lineEdit(), &enter);
        edit->updateGeometry();
        if (edit->layout())
        {
            edit->layout()->activate();
        }
        QApplication::processEvents(QEventLoop::AllEvents);

        const QList<QToolButton*> buttons = edit->findChildren<QToolButton*>();
        check(buttons.size() == 2,
            "ARCHLIGHTING bulk scrub owns one decrease and one increase stepper");
        if (buttons.size() != 2)
        {
            continue;
        }

        const bool buttonsVisible = std::all_of(
            buttons.begin(), buttons.end(), [](const QToolButton* const button)
            {
                return button && button->isVisible();
            });
        const QFontMetrics metrics(edit->lineEdit()->font());
        const int valueWidth = metrics.horizontalAdvance(signedHdrText);
        const QMargins margins = edit->lineEdit()->textMargins();
        const int frameWidth = edit->lineEdit()->style()->pixelMetric(
            QStyle::PM_DefaultFrameWidth, nullptr, edit->lineEdit());
        const int textAreaWidth = edit->lineEdit()->width()
            - margins.left() - margins.right() - 2 * frameWidth;
        const int arrowWidth = buttons[0]->width() + buttons[1]->width();
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
        const bool compactArrows = std::all_of(
            buttons.begin(), buttons.end(), [&compactArrowCap](const QToolButton* const button)
            {
                return button && button->width() <= compactArrowCap(button);
            });
        const QRect lineRect = edit->lineEdit()->geometry();
        bool childrenFit = true;
        bool childrenDoNotOverlap = true;
        for (const QToolButton* const button : buttons)
        {
            if (!button)
            {
                childrenFit = false;
                childrenDoNotOverlap = false;
                continue;
            }
            const QRect buttonRect = button->geometry();
            childrenFit = childrenFit && edit->rect().contains(buttonRect);
            childrenDoNotOverlap = childrenDoNotOverlap
                && !lineRect.intersects(buttonRect);
        }
        check(buttonsVisible && textAreaWidth >= valueWidth
                && textAreaWidth > arrowWidth
                && compactArrows
                && childrenFit && childrenDoNotOverlap,
            "ARCHLIGHTING bulk scrub keeps signed HDR decimals readable beside compact visible arrows");
        check(edit->lineEdit()->text() == signedHdrText
                && edit->value() == -1'000'000.0,
            "ARCHLIGHTING bulk scrub preserves exact signed HDR display values");

        buttons[0]->setDown(true);
        if (edit->layout())
        {
            edit->layout()->activate();
        }
        const int pressedTextAreaWidth = edit->lineEdit()->width()
            - margins.left() - margins.right() - 2 * frameWidth;
        check(buttons[0]->isDown()
                && std::all_of(buttons.begin(), buttons.end(), [](const QToolButton* const button)
                {
                    return button && button->isVisible();
                })
                && pressedTextAreaWidth >= valueWidth
                && compactArrows,
            "ARCHLIGHTING bulk scrub keeps compact arrows and signed HDR text readable while pressed");
        buttons[0]->setDown(false);
    }

    auto* const red = dialog.findChild<ui::ScrubFloatEdit*>(
        QStringLiteral("motsArchLightingRed"));
    constexpr double expectedTypedValue = -12345.678901234;
    setScrubTextAsUser(red, QStringLiteral("-12345.678901234"));
    check(red && red->lineEdit()
            && red->lineEdit()->text() == QStringLiteral("-12345.678901234")
            && red->value() == expectedTypedValue,
        "ARCHLIGHTING bulk scrub keeps a typed signed nine-decimal value without a roundtrip rewrite");
    dialog.hide();
}

bool sameThingSnapshot(const Thing& left, const Thing& right) noexcept
{
    if (left.id != right.id
        || left.num != right.num
        || left.sector != right.sector
        || left.archLightingIndex != right.archLightingIndex
        || left.templateName != right.templateName
        || left.name != right.name
        || std::bit_cast<std::uint64_t>(left.x)
            != std::bit_cast<std::uint64_t>(right.x)
        || std::bit_cast<std::uint64_t>(left.y)
            != std::bit_cast<std::uint64_t>(right.y)
        || std::bit_cast<std::uint64_t>(left.z)
            != std::bit_cast<std::uint64_t>(right.z)
        || std::bit_cast<std::uint64_t>(left.pch)
            != std::bit_cast<std::uint64_t>(right.pch)
        || std::bit_cast<std::uint64_t>(left.yaw)
            != std::bit_cast<std::uint64_t>(right.yaw)
        || std::bit_cast<std::uint64_t>(left.rol)
            != std::bit_cast<std::uint64_t>(right.rol)
        || left.layer != right.layer
        || left.bbox.x1 != right.bbox.x1
        || left.bbox.y1 != right.bbox.y1
        || left.bbox.z1 != right.bbox.z1
        || left.bbox.x2 != right.bbox.x2
        || left.bbox.y2 != right.bbox.y2
        || left.bbox.z2 != right.bbox.z2
        || left.values.size() != right.values.size())
    {
        return false;
    }
    for (std::size_t index = 0; index < left.values.size(); ++index)
    {
        const TplValue& a = left.values[index];
        const TplValue& b = right.values[index];
        if (a.name != b.name
            || a.type != b.type
            || a.intValue != b.intValue
            || std::bit_cast<std::uint64_t>(a.floatValue)
                != std::bit_cast<std::uint64_t>(b.floatValue)
            || a.stringValue != b.stringValue
            || a.adjType != b.adjType)
        {
            return false;
        }
    }
    return true;
}

bool sameSelectionSnapshot(
    const editor::Selection& left,
    const editor::Selection& right) noexcept
{
    return left.sectors == right.sectors
        && left.surfaces == right.surfaces
        && left.surfaceVertices == right.surfaceVertices
        && left.things == right.things
        && left.frames == right.frames
        && left.lights == right.lights
        && left.cogs == right.cogs;
}

bool sameArchitecturalLevelSnapshot(
    const Level& left,
    const Level& right) noexcept
{
    if (left.kind != right.kind
        || left.header.version != right.header.version
        || left.things.size() != right.things.size()
        || left.motsArchLighting != right.motsArchLighting
        || left.sectors.size() != right.sectors.size()
        || left.cogs.size() != right.cogs.size()
        || left.templates.size() != right.templates.size()
        || left.lights.size() != right.lights.size()
        || left.h3doNodes.size() != right.h3doNodes.size()
        || left.colormaps != right.colormaps
        || left.materials != right.materials
        || left.layers != right.layers)
    {
        return false;
    }
    for (std::size_t index = 0; index < left.things.size(); ++index)
    {
        if (!sameThingSnapshot(left.things[index], right.things[index]))
        {
            return false;
        }
    }
    return true;
}

void runStagingTests()
{
    MotsArchLightingStaging staging(makePlan());
    staging.setSelectedThingIndices({ 0, 1 });

    const MotsArchLightVertex exactValue{ -0.0f, 0.0f, 65504.0f, -12.75f };
    check(staging.setVertex(0, 1, exactValue),
        "ARCHLIGHTING dialog stages an exact per-vertex Set operation including zero channels");
    check(staging.targets().size() == 5
            && staging.records().size() == 1
            && staging.records()[0].stagedRecord
            && !staging.records()[0].stagedRecord->meshes.empty()
            && staging.records()[0].stagedRecord->meshes[0].vertices.size() > 1
            && sameVertexBits(
                staging.records()[0].stagedRecord->meshes[0].vertices[1],
                exactValue),
        "ARCHLIGHTING dialog stages Set once for all selected owners without quantizing the value");
    if (staging.targets().size() <= 4 || staging.records().empty())
    {
        return;
    }
    if (!staging.records()[0].stagedRecord
        || staging.records()[0].stagedRecord->meshes.empty()
        || staging.records()[0].stagedRecord->meshes[0].vertices.size() < 2)
    {
        return;
    }
    check(staging.targets().size() > 4
            && staging.targets()[4].recordStageIndex == staging.targets()[0].recordStageIndex,
        "ARCHLIGHTING staging deduplicates a large source record shared by selected and unselected Things");
    check(staging.targets().size() > 2 && staging.targets()[2].recordStageIndex < 0,
        "ARCHLIGHTING missing-model rows do not load an archive payload");

    check(!staging.setSelectedThingIndices({ 0 }),
        "ARCHLIGHTING staging locks selected owners after a Set operation");
    const bool mixedOperationRejected = !staging.multiply(
        level::MotsArchLightingVertexMode::Rgb,
        1.0f,
        ColorF{ 2.0f, 3.0f, 4.0f, 1.0f });
    check(mixedOperationRejected
            && staging.plan().edit.kind == level::MotsArchLightingEditKind::SetVertex
            && staging.records()[0].stagedRecord
            && !staging.records()[0].stagedRecord->meshes.empty()
            && staging.records()[0].stagedRecord->meshes[0].vertices.size() > 1
            && sameVertexBits(
                staging.records()[0].stagedRecord->meshes[0].vertices[1],
                exactValue),
        "ARCHLIGHTING staging rejects a second operation kind without discarding Set");
    check(staging.reset(),
        "ARCHLIGHTING Reset clears the one pending Set operation before another kind");
    check(!staging.hasChanges(),
        "ARCHLIGHTING Reset leaves no pending mutation for Cancel");
    check(staging.setSelectedThingIndices({ 0 }),
        "ARCHLIGHTING staging permits a new owner selection after Reset");
    check(staging.multiply(
            level::MotsArchLightingVertexMode::Rgb,
            1.0f,
            ColorF{ 2.0f, 3.0f, 4.0f, 1.0f }),
        "ARCHLIGHTING dialog stages a bulk RGB multiplier");
    const auto& multiplied = *staging.records()[0].stagedRecord;
    check(multiplied.meshes[0].vertices[0].red == 4.0f
            && multiplied.meshes[0].vertices[0].green == 0.0f
            && multiplied.meshes[0].vertices[0].blue == 16.0f,
        "ARCHLIGHTING RGB multiplier preserves gradients and cannot revive zero channels");

    check(staging.reset(),
        "ARCHLIGHTING dialog Reset restores the selected staged record baseline");
    check(!staging.hasChanges(),
        "ARCHLIGHTING dialog Reset leaves no pending mutation for Cancel");

    check(staging.setSelectedThingIndices({ 3 }),
        "ARCHLIGHTING staging permits selecting an initialization target after Reset");
    check(staging.initializeDefault(),
        "ARCHLIGHTING dialog stages the zero-valued default for an unbound Thing");
    const std::int32_t initializedStage = staging.targets().size() > 3
        ? staging.targets()[3].recordStageIndex : -1;
    check(initializedStage >= 0
            && static_cast<std::size_t>(initializedStage) < staging.records().size()
            && staging.records()[static_cast<std::size_t>(initializedStage)].stagedRecord
            && staging.records()[static_cast<std::size_t>(initializedStage)].stagedRecord->meshes.size() == 1
            && staging.records()[static_cast<std::size_t>(initializedStage)].stagedRecord->meshes[0].vertices.size() == 2
            && staging.records()[static_cast<std::size_t>(initializedStage)].stagedRecord->meshes[0].vertices[0]
                == MotsArchLightVertex{ 0.0f, 0.0f, 0.0f, 0.0f },
        "ARCHLIGHTING initialization sizes the all-zero default to model vertex prefixes");
}

void runPlanAndHistoryTests()
{
    MotsArchLightingStaging staging(makePlan());
    staging.setSelectedThingIndices({ 0, 1 });
    check(staging.setVertex(0, 0, MotsArchLightVertex{ 0.0f, 0.0f, 0.0f, 0.0f }),
        "ARCHLIGHTING dialog prepares one shared-owner plan for Apply");

    const auto& plan = staging.plan();
    const std::vector<level::MotsArchLightingTopology> topologies = staging.selectedTopologies();

    editor::DocumentState state{};
    state.level = makeLiveLevel();
    editor::Document document(std::move(state));
    const std::size_t beforeDepth = document.undoDepth();
    const auto beforeTable = document.current().level.motsArchLighting;

    const bool committed = document.applyUndoable(
        "Things: Architectural lighting changed",
        editor::DocumentDirty::Things,
        [&plan, &topologies](editor::DocumentState& current)
        {
            const auto result = level::applyMotsArchLightingEdit(
                current.level,
                plan.selectedThingIndices,
                topologies,
                plan.edit);
            return result.status == level::MotsArchLightingEditStatus::Applied;
        });
    check(committed && document.undoDepth() == beforeDepth + 1,
        "ARCHLIGHTING modal Apply publishes one staged undo transaction");
    check(document.current().level.things[0].archLightingIndex != 0
            && document.current().level.things[1].archLightingIndex != 0
            && document.current().level.things[4].archLightingIndex == 0
            && document.current().level.things[2].archLightingIndex == 1,
        "ARCHLIGHTING shared selected owners detach while an unselected owner remains bound");
    check(document.current().level.motsArchLighting[0] == beforeTable[0],
        "ARCHLIGHTING shared-owner Apply preserves the imported source record");
    check(document.current().level.motsArchLighting.size() == beforeTable.size() + 1
            && document.current().level.motsArchLighting.back().meshes[0].vertices[0]
                == MotsArchLightVertex{ 0.0f, 0.0f, 0.0f, 0.0f },
        "ARCHLIGHTING Apply publishes the exact zero-valued staged record");

    MotsArchLightingStaging noop(makePlan());
    noop.setSelectedThingIndices({ 0 });
    noop.multiply(
        level::MotsArchLightingVertexMode::Rgb,
        1.0f,
        ColorF{ 1.0f, 1.0f, 1.0f, 1.0f });
    check(!noop.hasChanges(),
        "ARCHLIGHTING dialog keeps a neutral multiplier as a staged no-op");
    const auto& noopPlan = noop.plan();
    const std::vector<level::MotsArchLightingTopology> noopTopologies = noop.selectedTopologies();
    editor::DocumentState noopState{};
    noopState.level = makeLiveLevel();
    editor::Document noopDocument(std::move(noopState));
    const auto noopBefore = noopDocument.current().level;
    const std::size_t noopDepth = noopDocument.undoDepth();
    const bool noopCommitted = noopDocument.applyUndoable(
        "Things: Architectural lighting changed",
        editor::DocumentDirty::Things,
        [&noopPlan, &noopTopologies](editor::DocumentState& current)
        {
            const auto result = level::applyMotsArchLightingEdit(
                current.level,
                noopPlan.selectedThingIndices,
                noopTopologies,
                noopPlan.edit);
            return result.status == level::MotsArchLightingEditStatus::Applied;
        });
    check(!noopCommitted && noopDocument.undoDepth() == noopDepth
            && noopDocument.current().level.motsArchLighting == noopBefore.motsArchLighting,
        "ARCHLIGHTING neutral Apply publishes no history and no authored mutation");

    // Exercise the real dialog rejection path against a fresh document. A
    // rejected dialog must leave both authored records and history untouched.
    editor::DocumentState cancelState{};
    cancelState.level = makeModelLiveLevel();
    editor::Document cancelDocument(std::move(cancelState));
    assets::ResourceLocator cancelResources{};
    cancelResources.setPrimaryResourceOverride(
        "arch_probe.3do", []()
        {
            return std::optional<std::vector<std::byte>>(modelFixtureBytes());
        });
    const auto cancelBeforeTable = cancelDocument.current().level.motsArchLighting;
    std::vector<std::int32_t> cancelBeforeReferences{};
    for (const Thing& thing : cancelDocument.current().level.things)
    {
        cancelBeforeReferences.push_back(thing.archLightingIndex);
    }
    const std::size_t cancelBeforeDepth = cancelDocument.undoDepth();
    ui::lighting::MotsArchLightingDialog cancelDialog(
        cancelDocument.current().level,
        nullptr,
        &cancelResources);
    QPushButton* const cancelReset =
        cancelDialog.findChild<QPushButton*>(
            QStringLiteral("motsArchLightingResetValues"));
    if (cancelReset)
    {
        cancelReset->click();
    }
    cancelDialog.reject();
    bool cancelReferencesUnchanged =
        cancelBeforeReferences.size() == cancelDocument.current().level.things.size();
    if (cancelReferencesUnchanged)
    {
        for (std::size_t index = 0; index < cancelBeforeReferences.size(); ++index)
        {
            if (cancelBeforeReferences[index]
                != cancelDocument.current().level.things[index].archLightingIndex)
            {
                cancelReferencesUnchanged = false;
                break;
            }
        }
    }
    check(cancelReset
            && cancelDialog.plan().hasPendingEdit
            && cancelDialog.result() == QDialog::Rejected
            && cancelDocument.undoDepth() == cancelBeforeDepth
            && cancelDocument.current().level.motsArchLighting == cancelBeforeTable
            && cancelReferencesUnchanged,
        "ARCHLIGHTING modal Cancel rejects staging without changing the document or history");
}

void runInvalidResourceAndResetTests()
{
    MotsArchLightingStaging missingModel(makePlan());
    missingModel.setSelectedThingIndices({ 2 });
    check(!missingModel.setVertex(0, 0,
              MotsArchLightVertex{ 0.0f, 0.0f, 0.0f, 0.0f }),
        "ARCHLIGHTING missing-model target rejects value edits in the dialog");
    check(!missingModel.resetToDefault() && !missingModel.hasChanges(),
        "ARCHLIGHTING Reset rejects a missing model instead of clearing its binding");

    MotsArchLightingStaging staging(makePlan());
    staging.setSelectedThingIndices({ 0 });
    check(staging.resetToDefault() && staging.hasChanges(),
        "ARCHLIGHTING Reset stages the all-zero default for a bound owner");
    auto staleTargets = staging.plan().targets;
    staleTargets[0].resourceRevision += "|replaced";
    check(ui::lighting::validateMotsArchLightingPlanResources(
              staging.plan(), staleTargets).status
            == ui::lighting::MotsArchLightingPlanStatus::Rejected,
        "ARCHLIGHTING Reset rejects model replacement before changing authored data");

    Level live = makeLiveLevel();
    const auto before = live;
    const auto topologies = staging.selectedTopologies();
    const auto result = level::applyMotsArchLightingEdit(
        live, staging.selectedThingIndices(), topologies, staging.plan().edit);
    check(result.status == level::MotsArchLightingEditStatus::Applied
            && live.things[0].archLightingIndex == 2
            && live.motsArchLighting.size() == before.motsArchLighting.size() + 1
            && level::isDefaultMotsArchLighting(live.motsArchLighting.back())
            && live.things[1].archLightingIndex == before.things[1].archLightingIndex
            && live.motsArchLighting[0] == before.motsArchLighting[0],
        "ARCHLIGHTING Reset uses copy-on-write and stores a bound all-zero record");

    MotsArchLightingStaging valueStaging(makePlan());
    valueStaging.setSelectedThingIndices({ 0 });
    check(valueStaging.setVertex(0, 0,
              MotsArchLightVertex{ 2.0f, 2.0f, 2.0f, 2.0f }),
        "ARCHLIGHTING value staging accepts the original resource identity");
    auto staleValueTargets = valueStaging.plan().targets;
    staleValueTargets[0].resourceRevision += "|replaced";
    const auto staleResult = ui::lighting::validateMotsArchLightingPlanResources(
        valueStaging.plan(), staleValueTargets);
    check(staleResult.status == ui::lighting::MotsArchLightingPlanStatus::Rejected,
        "ARCHLIGHTING Apply rejects stale resource identity before changing authored values");
}

void runInteractiveVertexPopupTests()
{
    const MotsArchLightVertex authored{
        -0.0f,
        std::numeric_limits<float>::denorm_min(),
        65504.0f,
        -0.0f };
    MotsArchLightVertex edited = authored;
    edited.mono = 0.25f;

    const auto buttonlessPopup = [](QDialog* const dialog)
    {
        return dialog
            && dialog->windowFlags().testFlag(Qt::Popup)
            && dialog->windowFlags().testFlag(Qt::FramelessWindowHint)
            && !dialog->findChild<QDialogButtonBox*>();
    };
    const auto closePopup = [](QDialog* const dialog)
    {
        if (dialog)
        {
            dialog->close();
        }
        QApplication::processEvents(QEventLoop::AllEvents);
    };

    MainWindow window{};
    prepareInteractiveWindow(window, authored);
    const std::size_t beforeDepth = window.undoDepthForTest();
    window.openArchitecturalLightingVertexEditorForTest(0, 0, 0, 0);
    QApplication::processEvents(QEventLoop::AllEvents);
    QDialog* const dialog = window.architecturalLightingVertexDialogForTest();
    auto* const mono = dialog
        ? dialog->findChild<ui::ScrubFloatEdit*>(
            QStringLiteral("motsArchLightingInteractiveMono"))
        : nullptr;
    auto* const mode = dialog
        ? dialog->findChild<QComboBox*>(
            QStringLiteral("motsArchLightingInteractiveMode"))
        : nullptr;
    check(buttonlessPopup(dialog) && mono && mode,
        "ARCHLIGHTING vertex picking opens real controls in a buttonless popup");
    if (!buttonlessPopup(dialog) || !mono || !mode)
    {
        closePopup(dialog);
        return;
    }
    mode->setCurrentIndex(1);
    QApplication::processEvents(QEventLoop::AllEvents);
    setScrubTextAsUser(mono, QStringLiteral("0.25"));

    const std::optional<MotsArchLightVertex> whilePreview =
        thingArchVertex(window);
    check(whilePreview && sameVertexBits(*whilePreview, authored)
            && window.undoDepthForTest() == beforeDepth,
        "ARCHLIGHTING vertex popup keeps document and history unchanged during live preview");

    closePopup(dialog);
    const std::optional<MotsArchLightVertex> committed = thingArchVertex(window);
    check(!window.architecturalLightingVertexDialogForTest()
            && window.undoDepthForTest() == beforeDepth + 1
            && committed && sameVertexBits(*committed, edited),
        "ARCHLIGHTING popup dismissal commits valid Mono input exactly once");

    check(window.undoForTest(),
        "ARCHLIGHTING popup edit exposes one atomic Undo step");
    const std::optional<MotsArchLightVertex> undone = thingArchVertex(window);
    check(undone && sameVertexBits(*undone, authored),
        "ARCHLIGHTING popup Undo restores every original channel bit");
    check(window.redoForTest(),
        "ARCHLIGHTING popup Redo reapplies the atomic edit");
    const std::optional<MotsArchLightVertex> redone = thingArchVertex(window);
    check(redone && sameVertexBits(*redone, edited),
        "ARCHLIGHTING popup Redo restores the exact edited channel bits");

    MainWindow neutral{};
    prepareInteractiveWindow(neutral, authored);
    const std::size_t neutralDepth = neutral.undoDepthForTest();
    neutral.openArchitecturalLightingVertexEditorForTest(0, 0, 0, 0);
    QApplication::processEvents(QEventLoop::AllEvents);
    QDialog* const neutralDialog =
        neutral.architecturalLightingVertexDialogForTest();
    const bool neutralButtonless = buttonlessPopup(neutralDialog);
    if (neutralDialog)
    {
        closePopup(neutralDialog);
    }
    const std::optional<MotsArchLightVertex> neutralValue =
        thingArchVertex(neutral);
    check(neutralButtonless
            && !neutral.architecturalLightingVertexDialogForTest()
            && neutral.undoDepthForTest() == neutralDepth
            && neutralValue && sameVertexBits(*neutralValue, authored),
        "ARCHLIGHTING neutral popup close is a safe no-op without history");

    MainWindow invalidClose{};
    prepareInteractiveWindow(invalidClose, authored);
    const std::size_t invalidCloseDepth = invalidClose.undoDepthForTest();
    invalidClose.openArchitecturalLightingVertexEditorForTest(0, 0, 0, 0);
    QApplication::processEvents(QEventLoop::AllEvents);
    QDialog* const invalidCloseDialog =
        invalidClose.architecturalLightingVertexDialogForTest();
    auto* const invalidCloseMono = invalidCloseDialog
        ? invalidCloseDialog->findChild<ui::ScrubFloatEdit*>(
            QStringLiteral("motsArchLightingInteractiveMono"))
        : nullptr;
    auto* const invalidCloseMode = invalidCloseDialog
        ? invalidCloseDialog->findChild<QComboBox*>(
            QStringLiteral("motsArchLightingInteractiveMode"))
        : nullptr;
    const bool invalidCloseButtonless = buttonlessPopup(invalidCloseDialog);
    if (invalidCloseMode)
    {
        invalidCloseMode->setCurrentIndex(1);
    }
    setScrubTextAsUser(invalidCloseMono, QStringLiteral("0.5"));
    setScrubTextAsUser(invalidCloseMono, QStringLiteral("1e100"));
    const std::optional<MotsArchLightVertex> beforeInvalidClose =
        thingArchVertex(invalidClose);
    check(invalidCloseButtonless && invalidCloseMono && invalidCloseMode
            && beforeInvalidClose && sameVertexBits(*beforeInvalidClose, authored)
            && invalidClose.undoDepthForTest() == invalidCloseDepth,
        "ARCHLIGHTING malformed text leaves the document unchanged before popup close");
    closePopup(invalidCloseDialog);
    const std::optional<MotsArchLightVertex> invalidCloseValue =
        thingArchVertex(invalidClose);
    check(!invalidClose.architecturalLightingVertexDialogForTest()
            && invalidClose.undoDepthForTest() == invalidCloseDepth
            && invalidCloseValue && sameVertexBits(*invalidCloseValue, authored),
        "ARCHLIGHTING invalid text on popup close discards prior staged preview");

    MainWindow corrected{};
    prepareInteractiveWindow(corrected, authored);
    const std::size_t correctedDepth = corrected.undoDepthForTest();
    corrected.openArchitecturalLightingVertexEditorForTest(0, 0, 0, 0);
    QApplication::processEvents(QEventLoop::AllEvents);
    QDialog* const correctedDialog =
        corrected.architecturalLightingVertexDialogForTest();
    auto* const correctedMono = correctedDialog
        ? correctedDialog->findChild<ui::ScrubFloatEdit*>(
            QStringLiteral("motsArchLightingInteractiveMono"))
        : nullptr;
    auto* const correctedMode = correctedDialog
        ? correctedDialog->findChild<QComboBox*>(
            QStringLiteral("motsArchLightingInteractiveMode"))
        : nullptr;
    const bool correctedButtonless = buttonlessPopup(correctedDialog);
    const QPointer<QDialog> correctedDialogGuard(correctedDialog);
    const QPointer<ui::ScrubFloatEdit> correctedMonoGuard(correctedMono);
    if (correctedMode)
    {
        correctedMode->setCurrentIndex(1);
    }
    QLineEdit* const correctedLineEdit = correctedMonoGuard
        ? correctedMonoGuard->lineEdit()
        : nullptr;
    if (correctedLineEdit)
    {
        correctedLineEdit->setFocus();
    }
    const auto setCorrectedPopupText = [correctedLineEdit](const QString& text)
    {
        if (!correctedLineEdit)
        {
            return;
        }
        correctedLineEdit->setText(text);
        // Keep the popup open while the same focused editor corrects invalid
        // text, as it does during an ordinary in-place edit.
        QMetaObject::invokeMethod(
            correctedLineEdit,
            "textEdited",
            Qt::DirectConnection,
            Q_ARG(QString, text));
    };
    setCorrectedPopupText(QStringLiteral("1e100"));
    setCorrectedPopupText(QStringLiteral("0.75"));
    const std::optional<MotsArchLightVertex> beforeCorrectedClose =
        thingArchVertex(corrected);
    check(correctedButtonless && correctedMono && correctedMode
            && beforeCorrectedClose
            && sameVertexBits(*beforeCorrectedClose, authored)
            && corrected.undoDepthForTest() == correctedDepth,
        "ARCHLIGHTING corrected popup input remains staged until dismissal");
    closePopup(correctedDialogGuard.data());
    MotsArchLightVertex correctedExpected = authored;
    correctedExpected.mono = 0.75f;
    const std::optional<MotsArchLightVertex> correctedValue =
        thingArchVertex(corrected);
    const bool correctedCommitSucceeded =
        !corrected.architecturalLightingVertexDialogForTest()
        && corrected.undoDepthForTest() == correctedDepth + 1
        && correctedValue && sameVertexBits(*correctedValue, correctedExpected);
    check(correctedCommitSucceeded,
        "ARCHLIGHTING corrected input commits once when the popup closes");
    check(corrected.undoForTest(),
        "ARCHLIGHTING corrected popup edit exposes one Undo step");
    const std::optional<MotsArchLightVertex> correctedUndoValue =
        thingArchVertex(corrected);
    check(correctedUndoValue && sameVertexBits(*correctedUndoValue, authored),
        "ARCHLIGHTING corrected popup Undo restores every original channel bit");
    check(corrected.redoForTest(),
        "ARCHLIGHTING corrected popup edit exposes one Redo step");
    const std::optional<MotsArchLightVertex> correctedRedoValue =
        thingArchVertex(corrected);
    check(correctedRedoValue
            && sameVertexBits(*correctedRedoValue, correctedExpected),
        "ARCHLIGHTING corrected popup Redo restores exact channel bits");

    MainWindow stale{};
    prepareInteractiveWindow(stale, authored);
    const std::size_t staleDepth = stale.undoDepthForTest();
    stale.openArchitecturalLightingVertexEditorForTest(0, 0, 0, 0);
    QApplication::processEvents(QEventLoop::AllEvents);
    QDialog* const staleDialog = stale.architecturalLightingVertexDialogForTest();
    auto* const staleMono = staleDialog
        ? staleDialog->findChild<ui::ScrubFloatEdit*>(
            QStringLiteral("motsArchLightingInteractiveMono"))
        : nullptr;
    auto* const staleMode = staleDialog
        ? staleDialog->findChild<QComboBox*>(
            QStringLiteral("motsArchLightingInteractiveMode"))
        : nullptr;
    const bool staleButtonless = buttonlessPopup(staleDialog);
    if (staleMode)
    {
        staleMode->setCurrentIndex(1);
    }
    setScrubTextAsUser(staleMono, QStringLiteral("0.5"));
    const std::int32_t staleRecordIndex = stale.documentStateForTest()
        .level.things.empty()
        ? -1
        : stale.documentStateForTest().level.things[0].archLightingIndex;
    bool externalMutationApplied = false;
    MotsArchLightVertex externalValue = authored;
    externalValue.green = 123.0f;
    if (staleRecordIndex >= 0
        && static_cast<std::size_t>(staleRecordIndex)
            < stale.documentStateForTest().level.motsArchLighting.size())
    {
        stale.mutableDocumentStateForTest().level.motsArchLighting[
            static_cast<std::size_t>(staleRecordIndex)]
            .meshes[0].vertices[0].green = externalValue.green;
        externalMutationApplied = true;
    }
    else
    {
        check(false, "ARCHLIGHTING stale popup fixture retains its source record");
    }
    closePopup(staleDialog);
    const std::optional<MotsArchLightVertex> staleValue = thingArchVertex(stale);
    check(staleButtonless && staleMono && staleMode && externalMutationApplied
            && !stale.architecturalLightingVertexDialogForTest()
            && stale.undoDepthForTest() == staleDepth
            && staleValue && sameVertexBits(*staleValue, externalValue),
        "ARCHLIGHTING popup close rejects a stale external vertex mutation atomically");

    MainWindow modeExit{};
    prepareInteractiveWindow(modeExit, authored);
    const std::size_t modeDepth = modeExit.undoDepthForTest();
    modeExit.openArchitecturalLightingVertexEditorForTest(0, 0, 0, 0);
    QApplication::processEvents(QEventLoop::AllEvents);
    QDialog* const modeDialog = modeExit.architecturalLightingVertexDialogForTest();
    auto* const modeMono = modeDialog
        ? modeDialog->findChild<ui::ScrubFloatEdit*>(
            QStringLiteral("motsArchLightingInteractiveMono"))
        : nullptr;
    auto* const modeChoice = modeDialog
        ? modeDialog->findChild<QComboBox*>(
            QStringLiteral("motsArchLightingInteractiveMode"))
        : nullptr;
    const bool modeButtonless = buttonlessPopup(modeDialog);
    if (modeChoice)
    {
        modeChoice->setCurrentIndex(1);
    }
    setScrubTextAsUser(modeMono, QStringLiteral("0.5"));
    modeExit.activateBakedLightEditModeForTest();
    QApplication::processEvents(QEventLoop::AllEvents);
    const std::optional<MotsArchLightVertex> modeExitValue =
        thingArchVertex(modeExit);
    check(modeButtonless && modeMono && modeChoice
            && !modeExit.architecturalLightingVertexDialogForTest()
            && modeExit.undoDepthForTest() == modeDepth
            && modeExitValue && sameVertexBits(*modeExitValue, authored),
        "ARCHLIGHTING mode exit cancels the staged popup and clears its preview safely");

    MainWindow resourceExit{};
    prepareInteractiveWindow(resourceExit, authored);
    const std::size_t resourceDepth = resourceExit.undoDepthForTest();
    resourceExit.openArchitecturalLightingVertexEditorForTest(0, 0, 0, 0);
    QApplication::processEvents(QEventLoop::AllEvents);
    QDialog* const resourceDialog =
        resourceExit.architecturalLightingVertexDialogForTest();
    auto* const resourceMono = resourceDialog
        ? resourceDialog->findChild<ui::ScrubFloatEdit*>(
            QStringLiteral("motsArchLightingInteractiveMono"))
        : nullptr;
    auto* const resourceMode = resourceDialog
        ? resourceDialog->findChild<QComboBox*>(
            QStringLiteral("motsArchLightingInteractiveMode"))
        : nullptr;
    GlView* const resourceView = resourceExit.glViewForTest();
    const bool resourceButtonless = buttonlessPopup(resourceDialog);
    if (resourceMode)
    {
        resourceMode->setCurrentIndex(1);
    }
    setScrubTextAsUser(resourceMono, QStringLiteral("0.5"));
    const std::optional<MotsArchLightVertex> beforeResourceReload =
        thingArchVertex(resourceExit);
    if (resourceView)
    {
        Q_EMIT resourceView->resourceFilesChanged();
    }
    QApplication::processEvents(QEventLoop::AllEvents);
    const std::optional<MotsArchLightVertex> resourceValue =
        thingArchVertex(resourceExit);
    check(resourceButtonless && resourceMono && resourceMode && resourceView
            && beforeResourceReload
            && sameVertexBits(*beforeResourceReload, authored)
            && !resourceExit.architecturalLightingVertexDialogForTest()
            && resourceExit.undoDepthForTest() == resourceDepth
            && resourceValue && sameVertexBits(*resourceValue, authored),
        "ARCHLIGHTING resource invalidation cancels the popup without committing preview data");

    MainWindow pickerExit{};
    prepareInteractiveWindow(pickerExit, authored);
    const std::size_t pickerDepth = pickerExit.undoDepthForTest();
    pickerExit.openArchitecturalLightingVertexEditorForTest(0, 0, 0, 0);
    QApplication::processEvents(QEventLoop::AllEvents);
    QDialog* const pickerEditor =
        pickerExit.architecturalLightingVertexDialogForTest();
    QWidget* const colorWidget = pickerEditor
        ? pickerEditor->findChild<QWidget*>(
            QStringLiteral("motsArchLightingInteractiveColor"))
        : nullptr;
    auto* const colorEditor = dynamic_cast<ui::LiveColorPopup*>(colorWidget);
    const bool pickerButtonless = buttonlessPopup(pickerEditor);
    const bool colorEditorAvailable = colorEditor != nullptr;
    QTimer::singleShot(0, &pickerExit,
        [&pickerExit]() { pickerExit.activateBakedLightEditModeForTest(); });
    if (colorEditor)
    {
        colorEditor->setCurrentColor(QColor(96, 128, 192), true);
    }
    const std::optional<MotsArchLightVertex> beforePickerExit =
        thingArchVertex(pickerExit);
    check(beforePickerExit && sameVertexBits(*beforePickerExit, authored)
            && pickerExit.undoDepthForTest() == pickerDepth,
        "ARCHLIGHTING nested color popup keeps document state unchanged while staged");
    QApplication::processEvents(QEventLoop::AllEvents);
    const std::optional<MotsArchLightVertex> pickerExitValue =
        thingArchVertex(pickerExit);
    check(pickerButtonless && colorEditorAvailable
            && !pickerExit.architecturalLightingVertexDialogForTest()
            && pickerExit.undoDepthForTest() == pickerDepth
            && pickerExitValue && sameVertexBits(*pickerExitValue, authored),
        "ARCHLIGHTING mode exit aborts a nested color control without mutation");
}

void runArchitecturalBindingTransactionTests()
{
    MainWindow copy{};
    copy.setAttribute(Qt::WA_DontShowOnScreen);
    copy.setPrimaryResourceOverrideForTest(
        "arch_probe.3do",
        []()
        {
            return std::optional<std::vector<std::byte>>(modelFixtureBytes());
        });
    copy.setDocumentStateForTest(editor::DocumentState{ makeModelLiveLevel(), {} });
    copy.setProjectTypeForTest(ProjectType::MOTS);
    copy.activateArchitecturalLightingEditModeForTest();
    editor::Selection& copySelection = copy.mutableDocumentStateForTest().selection;
    copySelection.things = { 4, 3 };
    copy.glViewForTest()->setSelectedThing(4);
    copy.glViewForTest()->setSelectionState(-1, -1, 4, -1, copySelection);
    const std::size_t copyDepth = copy.undoDepthForTest();
    const std::size_t copyRecordCount =
        copy.documentStateForTest().level.motsArchLighting.size();
    const auto copyBefore = copy.documentStateForTest().level;
    const bool copied = copy.copyArchitecturalLightingClipboardForTest();
    check(copied
            && copy.undoDepthForTest() == copyDepth
            && copy.documentStateForTest().level.things[4].archLightingIndex == 0
            && copy.documentStateForTest().level.things[3].archLightingIndex == -1
            && copy.documentStateForTest().level.motsArchLighting.size() == copyRecordCount
            && sameArchitecturalLevelSnapshot(
                copy.documentStateForTest().level, copyBefore),
        "ARCHLIGHTING Copy snapshots the active higher-index source without mutating a lower unbound Thing");
    copySelection.things = { 3 };
    copy.glViewForTest()->setSelectedThing(3);
    copy.glViewForTest()->setSelectionState(-1, -1, 3, -1, copySelection);
    check(copy.pasteArchitecturalLightingClipboardForTest()
            && copy.undoDepthForTest() == copyDepth + 1
            && copy.documentStateForTest().level.things[4].archLightingIndex == 0
            && copy.documentStateForTest().level.things[3].archLightingIndex == 0
            && copy.documentStateForTest().level.motsArchLighting.size() == copyRecordCount,
        "ARCHLIGHTING Paste applies the copied active source to the lower compatible Thing");
    check(copy.undoForTest()
            && copy.documentStateForTest().level.things[3].archLightingIndex == -1,
        "ARCHLIGHTING Paste Undo restores the lower destination binding");
    check(copy.redoForTest()
            && copy.documentStateForTest().level.things[3].archLightingIndex == 0,
        "ARCHLIGHTING Paste Redo restores the lower destination binding");

    MainWindow locked{};
    locked.setAttribute(Qt::WA_DontShowOnScreen);
    locked.setPrimaryResourceOverrideForTest(
        "arch_probe.3do",
        []()
        {
            return std::optional<std::vector<std::byte>>(modelFixtureBytes());
        });
    locked.setDocumentStateForTest(editor::DocumentState{ makeModelLiveLevel(), {} });
    locked.setProjectTypeForTest(ProjectType::MOTS);
    locked.activateArchitecturalLightingEditModeForTest();
    editor::Selection& lockedSelection = locked.mutableDocumentStateForTest().selection;
    lockedSelection.things = { 4, 3 };
    locked.glViewForTest()->setSelectedThing(4);
    locked.glViewForTest()->setSelectionState(-1, -1, 4, -1, lockedSelection);
    check(locked.copyArchitecturalLightingClipboardForTest(),
        "ARCHLIGHTING Copy snapshot remains available before a locked Paste");
    lockedSelection.things = { 3 };
    locked.glViewForTest()->setSelectedThing(3);
    locked.glViewForTest()->setSelectionState(-1, -1, 3, -1, lockedSelection);
    locked.setLayerVisualStateForTest({ true }, { true });
    const auto lockedBefore = locked.documentStateForTest().level;
    const auto lockedBeforeSelection = locked.documentStateForTest().selection;
    const std::size_t lockedDepth = locked.undoDepthForTest();
    check(!locked.pasteArchitecturalLightingClipboardForTest()
            && locked.undoDepthForTest() == lockedDepth
            && sameArchitecturalLevelSnapshot(
                locked.documentStateForTest().level, lockedBefore)
            && sameSelectionSnapshot(
                locked.documentStateForTest().selection,
                lockedBeforeSelection),
        "ARCHLIGHTING Copy rejects a locked batch without level, selection, or history mutation");

    MainWindow initialize{};
    initialize.setAttribute(Qt::WA_DontShowOnScreen);
    initialize.setPrimaryResourceOverrideForTest(
        "arch_probe.3do",
        []()
        {
            return std::optional<std::vector<std::byte>>(modelFixtureBytes());
        });
    initialize.setDocumentStateForTest(editor::DocumentState{ makeModelLiveLevel(), {} });
    initialize.setProjectTypeForTest(ProjectType::MOTS);
    initialize.activateArchitecturalLightingEditModeForTest();
    editor::Selection& initializeSelection = initialize.mutableDocumentStateForTest().selection;
    initializeSelection.things = { 3 };
    initialize.glViewForTest()->setSelectionState(-1, -1, 3, -1, initializeSelection);
    const std::size_t initializeDepth = initialize.undoDepthForTest();
    const std::size_t initializeRecords = initialize.documentStateForTest()
        .level.motsArchLighting.size();
    check(initialize.initializeArchitecturalLightingForTest()
            && initialize.undoDepthForTest() == initializeDepth + 1
            && initialize.documentStateForTest().level.things[3].archLightingIndex
                >= static_cast<std::int32_t>(initializeRecords)
            && initialize.documentStateForTest().level.motsArchLighting.size()
                == initializeRecords + 1
            && level::isDefaultMotsArchLighting(
                initialize.documentStateForTest().level.motsArchLighting.back()),
        "ARCHLIGHTING bulk initialization materializes the zero default in one transaction");
    const std::int32_t initializedIndex = initialize.documentStateForTest()
        .level.things[3].archLightingIndex;
    check(initialize.undoForTest()
            && initialize.documentStateForTest().level.things[3].archLightingIndex == -1
            && initialize.documentStateForTest().level.motsArchLighting.size()
                == initializeRecords,
        "ARCHLIGHTING initialization Undo removes only its default record");
    check(initialize.redoForTest()
            && initialize.documentStateForTest().level.things[3].archLightingIndex
                == initializedIndex
            && initialize.documentStateForTest().level.motsArchLighting.size()
                == initializeRecords + 1,
        "ARCHLIGHTING initialization Redo restores the exact default binding");

    MainWindow remove{};
    remove.setAttribute(Qt::WA_DontShowOnScreen);
    remove.setPrimaryResourceOverrideForTest(
        "arch_probe.3do", []()
        {
            return std::optional<std::vector<std::byte>>(modelFixtureBytes());
        });
    remove.setDocumentStateForTest(editor::DocumentState{ makeModelLiveLevel(), {} });
    remove.setProjectTypeForTest(ProjectType::MOTS);
    remove.activateArchitecturalLightingEditModeForTest();
    editor::Selection& removeSelection = remove.mutableDocumentStateForTest().selection;
    removeSelection.things = { 4 };
    remove.glViewForTest()->setSelectedThing(4);
    remove.glViewForTest()->setSelectionState(-1, -1, 4, -1, removeSelection);
    QAction* const deleteAction = remove.findChild<QAction*>(
        QStringLiteral("edit.delete"));
    const auto removeBeforeRecords = remove.documentStateForTest()
        .level.motsArchLighting;
    const auto removeBeforeSelection = remove.documentStateForTest().selection;
    const std::size_t removeDepth = remove.undoDepthForTest();
    remove.updateEditActionsForTest();
    if (deleteAction)
    {
        deleteAction->trigger();
    }
    const Level& resetLevel = remove.documentStateForTest().level;
    const std::int32_t resetIndex = resetLevel.things[4].archLightingIndex;
    check(deleteAction
            && remove.undoDepthForTest() == removeDepth + 1
            && resetIndex >= static_cast<std::int32_t>(removeBeforeRecords.size())
            && resetLevel.motsArchLighting.size() == removeBeforeRecords.size() + 1
            && level::isDefaultMotsArchLighting(resetLevel.motsArchLighting.back()),
        "ARCHLIGHTING Delete resets the selected owner's values to a COW zero record");
    check(remove.undoForTest()
            && remove.documentStateForTest().level.things[4].archLightingIndex == 0
            && remove.documentStateForTest().level.motsArchLighting == removeBeforeRecords
            && sameSelectionSnapshot(
                remove.documentStateForTest().selection, removeBeforeSelection),
        "ARCHLIGHTING Delete Undo restores the authored binding without changing selection");
    check(remove.redoForTest()
            && remove.documentStateForTest().level.things[4].archLightingIndex == resetIndex
            && remove.documentStateForTest().level.motsArchLighting.size()
                == removeBeforeRecords.size() + 1
            && level::isDefaultMotsArchLighting(
                remove.documentStateForTest().level.motsArchLighting.back()),
        "ARCHLIGHTING Delete Redo restores the default record");
}

void runArchitecturalTargetBatchCacheTests()
{
    Level level = makeModelLiveLevel();
    assets::ResourceLocator resources{};
    std::size_t resourceReads = 0;
    resources.setPrimaryResourceOverride(
        "arch_probe.3do",
        [&resourceReads]()
        {
            ++resourceReads;
            return std::optional<std::vector<std::byte>>(
                sharedHierarchyModelFixtureBytes());
        });
    const std::vector<std::int32_t> indices{ 4, 3, 2, 1, 0 };
    const auto targets = ui::lighting::collectMotsArchLightingTargetsForThings(
        level, nullptr, &resources, indices);
    check(targets.size() == indices.size()
            && resourceReads == 1
            && std::all_of(
                targets.begin(), targets.end(),
                [](const MotsArchLightingTarget& target)
                {
                    return target.modelName == "arch_probe.3do";
                }),
        "ARCHLIGHTING reset/copy target batches share one model/LOD resource read");
    const bool sourceTopology = !targets.empty()
        && std::all_of(
            targets.front().topology.geosets.begin(),
            targets.front().topology.geosets.end(),
            [](const std::vector<std::size_t>& geoset)
            {
                return geoset.size() == 1u && geoset.front() == 2u;
            });
    check(sourceTopology,
        "ARCHLIGHTING target batches keep shared hierarchy topology in authored source-mesh order");
}

void runArchitecturalToolbarLayoutTests()
{
    MainWindow window{};
    window.setAttribute(Qt::WA_DontShowOnScreen);
    window.setProjectTypeForTest(ProjectType::MOTS);
    window.resize(520, 360);
    window.show();
    window.activateArchitecturalLightingEditModeForTest();
    QApplication::processEvents(QEventLoop::AllEvents);
    GlView* const view = window.glViewForTest();
    QWidget* const editOverlay = view
        ? view->findChild<QWidget*>(QStringLiteral("viewportEditModeOverlay"))
        : nullptr;
    QWidget* const toolOverlay = view
        ? view->findChild<QWidget*>(QStringLiteral("viewportToolsOverlay"))
        : nullptr;
    QWidget* const rightOverlay = view
        ? view->findChild<QWidget*>(QStringLiteral("viewportOverlay"))
        : nullptr;
    QToolButton* const addButton = toolOverlay
        ? toolOverlay->findChild<QToolButton*>(
            QStringLiteral("viewportArchitecturalLightingAddButton"))
        : nullptr;
    QToolButton* const removeButton = toolOverlay
        ? toolOverlay->findChild<QToolButton*>(
            QStringLiteral("viewportArchitecturalLightingRemoveButton"))
        : nullptr;
    QToolButton* const copyButton = toolOverlay
        ? toolOverlay->findChild<QToolButton*>(
            QStringLiteral("viewportArchitecturalLightingCopyButton"))
        : nullptr;
    const bool compact = editOverlay && toolOverlay && rightOverlay
        && editOverlay->isVisible()
        && toolOverlay->isHidden()
        && !editOverlay->geometry().intersects(rightOverlay->geometry())
        && editOverlay->geometry().right() < view->width()
        && !addButton && !removeButton && !copyButton
        && window.findChild<QAction*>(QStringLiteral("viewport.arch_lighting.add")) == nullptr;
    check(compact,
        "ARCHLIGHTING narrow viewport hides the obsolete empty tools overlay without collisions");

    QAction* const bakedAction = window.findChild<QAction*>(
        QStringLiteral("viewport.light_mode.baked"));
    QAction* const architecturalAction = window.findChild<QAction*>(
        QStringLiteral("viewport.light_mode.architectural"));
    constexpr int expectedIconSize = 32;
    const QImage actualArchitecturalIcon = architecturalAction
        ? architecturalAction->icon().pixmap(
            expectedIconSize, expectedIconSize).toImage()
            .convertToFormat(QImage::Format_ARGB32_Premultiplied)
        : QImage{};
    const QImage expectedArchitecturalIcon =
        render::renderArchitecturalLightingIconImage(
            QColor(255, 211, 82), expectedIconSize)
            .convertToFormat(QImage::Format_ARGB32_Premultiplied);
    check(bakedAction && architecturalAction
            && !architecturalAction->icon().isNull()
            && actualArchitecturalIcon == expectedArchitecturalIcon
            && actualArchitecturalIcon != bakedAction->icon().pixmap(
                expectedIconSize, expectedIconSize).toImage()
                    .convertToFormat(QImage::Format_ARGB32_Premultiplied),
        "ARCHLIGHTING submode uses its owner gizmo icon instead of the baked-light glyph");
    window.hide();
}

} // namespace

void runMotsArchLightingDialogTests()
{
    runBulkScrubFieldGeometryTests();
    runStagingTests();
    runPlanAndHistoryTests();
    runInvalidResourceAndResetTests();
    runInteractiveVertexPopupTests();
    runArchitecturalBindingTransactionTests();
    runArchitecturalTargetBatchCacheTests();
    runArchitecturalToolbarLayoutTests();
}

} // namespace sed::app_tests
