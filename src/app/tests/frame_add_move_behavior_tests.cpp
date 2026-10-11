#include "test_common.hpp"

#include <QAction>
#include <QApplication>
#include <QCursor>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QKeySequence>
#include <QLineEdit>
#include <QMetaObject>
#include <QScreen>
#include <QSettings>
#include <QTemporaryDir>
#include <QThread>
#include <QTreeWidget>
#include <QToolButton>

#include <array>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

#include "app_settings.hpp"
#include "gl_view.hpp"
#include "main_window/main_window.hpp"
#include "sed/assets/sed.hpp"
#include "sed/level/build.hpp"
#include "thing/thing_resource_utils.hpp"

namespace sed::app_tests
{
namespace
{

TplValue stringValue(const std::string& name, const std::string& value)
{
    TplValue result{};
    result.name = name;
    result.type = ValueType::str;
    result.stringValue = value;
    return result;
}

class ScopedThingMoveSettings final
{
public:
    explicit ScopedThingMoveSettings(
        const std::array<std::pair<QString, bool>, 4>& overrides)
        : settings_(settingsStore())
    {
        saved_.reserve(overrides.size());
        for (const auto& [key, value] : overrides)
        {
            const bool existed = settings_.contains(key);
            saved_.push_back(SavedSetting{
                key,
                existed,
                existed ? settings_.value(key) : QVariant{}});
            settings_.setValue(key, value);
        }
        settings_.sync();
    }

    ~ScopedThingMoveSettings()
    {
        for (const SavedSetting& setting : saved_)
        {
            if (setting.existed)
            {
                settings_.setValue(setting.key, setting.value);
            }
            else
            {
                settings_.remove(setting.key);
            }
        }
        settings_.sync();
    }

    ScopedThingMoveSettings(const ScopedThingMoveSettings&) = delete;
    ScopedThingMoveSettings& operator=(const ScopedThingMoveSettings&) = delete;

private:
    struct SavedSetting
    {
        QString key;
        bool existed{};
        QVariant value{};
    };

    QSettings settings_;
    std::vector<SavedSetting> saved_{};
};

std::array<std::pair<QString, bool>, 4> frameAddPreferences()
{
    return {{
        { QStringLiteral("thing_move/surface_stick"), true },
        { QStringLiteral("thing_move/surface_align"), false },
        { QStringLiteral("thing_move/snap_to_thing"), false },
        { QStringLiteral("thing_move/snap_rotate"), true },
    }};
}

void reportFramePlacementDiagnostic(
    const MainWindow& window,
    const char* context)
{
    const QString diagnostic = window.property(
        "sed.framePlacementPreviewDiagnostic").toString();
    std::cerr << context << ": " << diagnostic.toStdString() << '\n';
}

void selectFrameOwnerForTest(
    MainWindow& window,
    GlView* const view,
    const bool selectFrame)
{
    editor::Selection selection{};
    selection.things.insert(0);
    if (selectFrame)
    {
        selection.frames.insert(level::FrameRef{0, 0});
    }
    window.mutableDocumentStateForTest().selection = selection;
    if (view)
    {
        view->setSelectionState(-1, -1, 0, -1, selection);
        if (selectFrame)
        {
            view->setSelectedFrameValueIndex(0);
        }
    }
}

TplValue frameValue(
    const double x,
    const double y,
    const double z,
    const double pitch,
    const double yaw,
    const double roll)
{
    TplValue result{};
    result.name = "frame";
    result.type = ValueType::frame;
    result.adjType = AdjType::frame;
    result.setFrame(x, y, z, pitch, yaw, roll);
    return result;
}

editor::DocumentState framePlacementState(const bool localSpriteOverride)
{
    editor::DocumentState state{};
    state.level.kind = ProjectType::IJIM;
    state.level.layers = { "Layer0" };
    level::levelCreateCube(
        state.level,
        0.0,
        0.0,
        0.0,
        TVector::up,
        TVector::right);

    ThingTemplate thingTemplate{};
    thingTemplate.name = "+frame_probe";
    thingTemplate.values.push_back(stringValue("model3do", "inherited.3do"));
    state.level.templates.push_back(std::move(thingTemplate));

    Thing owner{};
    owner.name = "+frame_probe";
    owner.templateName = "+frame_probe";
    owner.layer = 0;
    owner.x = 0.5;
    owner.y = 0.5;
    owner.z = 1.5;
    owner.pch = 13.0;
    owner.yaw = 31.0;
    owner.rol = -7.0;
    owner.values.push_back(frameValue(0.5, 0.5, 1.5, 13.0, 31.0, -7.0));
    if (localSpriteOverride)
    {
        owner.values.push_back(stringValue("sprite", "authored_override.spr"));
    }
    state.level.things.push_back(std::move(owner));
    state.selection.things.insert(0);
    return state;
}

editor::DocumentState frameMoveState()
{
    editor::DocumentState state{};
    state.level.kind = ProjectType::IJIM;
    state.level.layers = { "Layer0" };

    ThingTemplate thingTemplate{};
    thingTemplate.name = "+frame_snap_probe";
    thingTemplate.values.push_back(
        stringValue("model3d", "frame_move_tiny.3do"));
    state.level.templates.push_back(std::move(thingTemplate));

    Thing owner{};
    owner.name = "+frame_snap_probe";
    owner.templateName = owner.name;
    owner.layer = 0;
    owner.x = 0.005;
    owner.y = 0.006;
    owner.z = 0.18;
    owner.values.push_back(frameValue(0.0, 0.0, 0.0, 0.0, 0.0, 0.0));
    state.level.things.push_back(std::move(owner));

    Thing distantAnchor{};
    distantAnchor.name = "+frame_snap_probe";
    distantAnchor.templateName = distantAnchor.name;
    distantAnchor.layer = 0;
    distantAnchor.x = 100.0;
    distantAnchor.y = 100.0;
    distantAnchor.z = 100.0;
    state.level.things.push_back(std::move(distantAnchor));

    state.selection.things.insert(0);
    state.selection.frames.insert(level::FrameRef{ 0, 0 });
    return state;
}

QByteArray frameMoveTinyModelFixture()
{
    return QByteArrayLiteral(
        "SECTION: HEADER\n"
        "3DO 2.3\n"
        "SECTION: GEOMETRYDEF\n"
        "RADIUS 1\n"
        "INSERT OFFSET 0 0 0\n"
        "GEOSETS 1\n"
        "GEOSET 0\n"
        "MESHES 1\n"
        "MESH 0\n"
        "NAME frame_move_tiny\n"
        "VERTICES 8\n"
        "0: -0.001 -0.001 -0.001\n"
        "1: 0.001 -0.001 -0.001\n"
        "2: 0.001 0.001 -0.001\n"
        "3: -0.001 0.001 -0.001\n"
        "4: -0.001 -0.001 0.001\n"
        "5: 0.001 -0.001 0.001\n"
        "6: 0.001 0.001 0.001\n"
        "7: -0.001 0.001 0.001\n"
        "FACES 0\n"
        "SECTION: HIERARCHYDEF\n"
        "HIERARCHY NODES 1\n"
        "0: 0x0 0x1 0 -1 -1 -1 0 0 0 0 0 0 0 0 0 0 0 frame_move_tiny\n");
}

bool writeFrameMoveTinyModelFixture(
    const std::filesystem::path& root)
{
    const QByteArray bytes = frameMoveTinyModelFixture();
    std::ofstream output(root / "frame_move_tiny.3do", std::ios::binary);
    output.write(bytes.constData(), bytes.size());
    return output.good();
}

struct SurfacePlacementTarget
{
    QPoint cursor{};
    QVector3D point{};
    SurfaceRef surface{};
    QVector3D normal{};
    TVector documentPoint{};
};

std::optional<SurfacePlacementTarget> findProjectedSolidContact(
    GlView* const view,
    const Level& level)
{
    if (!view || view->width() <= 0 || view->height() <= 0)
    {
        return std::nullopt;
    }

    for (std::size_t sectorIndex = 0;
         sectorIndex < level.sectors.size();
         ++sectorIndex)
    {
        const Sector& sector = level.sectors[sectorIndex];
        for (std::size_t surfaceIndex = 0;
             surfaceIndex < sector.surfaces.size();
             ++surfaceIndex)
        {
            const Surface& surface = sector.surfaces[surfaceIndex];
            if ((surface.surfaceFlags & SF_Collision) == 0
                || surface.vertices.size() < 3u)
            {
                continue;
            }

            TVector center{};
            std::size_t validVertexCount = 0;
            for (const std::int32_t vertexIndex : surface.vertices)
            {
                if (vertexIndex < 0
                    || static_cast<std::size_t>(vertexIndex) >= sector.vertices.size())
                {
                    continue;
                }
                const Vertex& vertex = sector.vertices[
                    static_cast<std::size_t>(vertexIndex)];
                center.x += vertex.x;
                center.y += vertex.y;
                center.z += vertex.z;
                ++validVertexCount;
            }
            if (validVertexCount < 3u)
            {
                continue;
            }
            const double inverseCount = 1.0 / static_cast<double>(validVertexCount);
            center.x *= inverseCount;
            center.y *= inverseCount;
            center.z *= inverseCount;
            const auto projected = view->projectWorldToViewport(QVector3D(
                static_cast<float>(center.x),
                static_cast<float>(center.y),
                static_cast<float>(center.z)));
            if (!projected)
            {
                continue;
            }

            const QPoint centerPixel = projected->toPoint();
            for (const QPoint offset : {
                     QPoint{}, QPoint(1, 0), QPoint(-1, 0),
                     QPoint(0, 1), QPoint(0, -1),
                     QPoint(1, 1), QPoint(-1, 1),
                     QPoint(1, -1), QPoint(-1, -1) })
            {
                const QPoint cursor = centerPixel + offset;
                if (!view->rect().contains(cursor))
                {
                    continue;
                }

                bool haveSurface = false;
                SurfaceRef surfaceRef{};
                QVector3D surfaceNormal{};
                TVector documentPoint{};
                const std::optional<QVector3D> point =
                    view->placementPointForViewport(
                        cursor,
                        &haveSurface,
                        &surfaceRef,
                        &surfaceNormal,
                        true,
                        false,
                        &documentPoint);
                if (point && haveSurface && surfaceRef.sector >= 0
                    && static_cast<std::size_t>(surfaceRef.sector)
                        < level.sectors.size()
                    && surfaceRef.surface >= 0
                    && static_cast<std::size_t>(surfaceRef.surface)
                        < level.sectors[static_cast<std::size_t>(
                            surfaceRef.sector)].surfaces.size())
                {
                    const Surface& hitSurface = level.sectors[
                        static_cast<std::size_t>(surfaceRef.sector)].surfaces[
                            static_cast<std::size_t>(surfaceRef.surface)];
                    if ((hitSurface.surfaceFlags & SF_Collision) != 0)
                    {
                        return SurfacePlacementTarget{
                            cursor,
                            *point,
                            surfaceRef,
                            surfaceNormal,
                            documentPoint };
                    }
                }
            }
        }
    }
    return std::nullopt;
}

std::optional<SurfacePlacementTarget> resolveSolidContactAt(
    GlView* const view,
    const Level& level,
    const QPoint& cursor)
{
    if ( !view || !view->rect().contains(cursor) )
    {
        return std::nullopt;
    }

    bool haveSurface = false;
    SurfaceRef surfaceRef{};
    QVector3D surfaceNormal{};
    TVector documentPoint{};
    const std::optional<QVector3D> point = view->placementPointForViewport(
        cursor,
        &haveSurface,
        &surfaceRef,
        &surfaceNormal,
        true,
        false,
        &documentPoint);
    if ( !point || !haveSurface || surfaceRef.sector < 0
        || static_cast<std::size_t>(surfaceRef.sector) >= level.sectors.size()
        || surfaceRef.surface < 0
        || static_cast<std::size_t>(surfaceRef.surface)
            >= level.sectors[static_cast<std::size_t>(surfaceRef.sector)]
                   .surfaces.size() )
    {
        return std::nullopt;
    }

    const Surface& surface = level.sectors[
        static_cast<std::size_t>(surfaceRef.sector)].surfaces[
        static_cast<std::size_t>(surfaceRef.surface)];
    if ( (surface.surfaceFlags & SF_Collision) == 0 )
    {
        return std::nullopt;
    }
    return SurfacePlacementTarget{
        cursor, *point, surfaceRef, surfaceNormal, documentPoint };
}

bool waitForCursorGlobalPosition(
    const QPoint& expectedGlobal,
    QPoint* const actualGlobal)
{
    QElapsedTimer timeout{};
    timeout.start();
    QPoint observed = QCursor::pos();
    while ( timeout.elapsed() < 750 )
    {
        QApplication::processEvents(QEventLoop::AllEvents, 5);
        observed = QCursor::pos();
        if ( observed == expectedGlobal )
        {
            if ( actualGlobal )
            {
                *actualGlobal = observed;
            }
            return true;
        }
        QThread::msleep(1);
    }
    QApplication::processEvents(QEventLoop::AllEvents, 5);
    observed = QCursor::pos();
    if ( actualGlobal )
    {
        *actualGlobal = observed;
    }
    return observed == expectedGlobal;
}

void reportFramePlacementCursorDiagnostic(
    const QMainWindow& window,
    const GlView* const view,
    const QPoint& expectedLocal,
    const QPoint& expectedGlobal,
    const QPoint& actualGlobal,
    const QPoint& actualLocal)
{
    const auto printRect = [](const char* const label, const QRect& rect)
    {
        std::cerr << label << '=' << rect.x() << ',' << rect.y() << ','
                  << rect.width() << 'x' << rect.height();
    };
    QScreen* const expectedScreen = window.screen();
    QScreen* const actualScreen = QGuiApplication::screenAt(actualGlobal);
    std::cerr << "Frame Add cursor mapping diagnostic: expectedLocal="
              << expectedLocal.x() << ',' << expectedLocal.y()
              << " expectedGlobal=" << expectedGlobal.x() << ','
              << expectedGlobal.y() << " actualGlobal=" << actualGlobal.x()
              << ',' << actualGlobal.y() << " actualLocal=" << actualLocal.x()
              << ',' << actualLocal.y();
    printRect(" windowGeometry", window.geometry());
    printRect(" windowFrame", window.frameGeometry());
    if ( view )
    {
        printRect(" viewGeometry", view->geometry());
        printRect(" viewGlobalGeometry",
            QRect(view->mapToGlobal(QPoint{}), view->size()));
        std::cerr << " viewDpr=" << view->devicePixelRatioF();
    }
    if ( expectedScreen )
    {
        printRect(" expectedScreenAvailable",
            expectedScreen->availableGeometry());
        std::cerr << " expectedScreenDpr="
                  << expectedScreen->devicePixelRatio();
    }
    if ( actualScreen )
    {
        printRect(" actualScreenAvailable",
            actualScreen->availableGeometry());
        std::cerr << " actualScreenDpr=" << actualScreen->devicePixelRatio();
    }
    std::cerr << " windowVisible=" << window.isVisible()
              << " windowActive=" << window.isActiveWindow()
              << " activeWindow="
              << (QApplication::activeWindow()
                      ? QApplication::activeWindow()->objectName().toStdString()
                      : std::string("<none>"))
              << '\n';
}

bool invokePlacementHover(
    GlView* const view,
    const QPoint& position,
    const Qt::KeyboardModifiers modifiers)
{
    return view && QMetaObject::invokeMethod(
        view,
        "placementPreviewMouseMoved",
        Qt::DirectConnection,
        Q_ARG(QPoint, position),
        Q_ARG(Qt::KeyboardModifiers, modifiers));
}

bool invokeFrameAddClick(
    GlView* const view,
    const QVector3D& point,
    const std::int32_t sector,
    const std::int32_t surface,
    const QPoint& widgetPosition,
    const Qt::KeyboardModifiers modifiers)
{
    return view && QMetaObject::invokeMethod(
        view,
        "surfaceClicked",
        Qt::DirectConnection,
        Q_ARG(QVector3D, point),
        Q_ARG(std::int32_t, sector),
        Q_ARG(std::int32_t, surface),
        Q_ARG(QPoint, widgetPosition),
        Q_ARG(Qt::KeyboardModifiers, modifiers));
}

std::optional<std::array<double, 6>> lastFramePose(const Thing& thing)
{
    for (std::size_t index = thing.values.size(); index > 0; --index)
    {
        const TplValue& value = thing.values[index - 1];
        if (value.name != "frame" || value.type != ValueType::frame)
        {
            continue;
        }
        std::array<double, 6> result{};
        if (!value.tryGetFrame(
                result[0], result[1], result[2],
                result[3], result[4], result[5]))
        {
            return std::nullopt;
        }
        return result;
    }
    return std::nullopt;
}

std::optional<std::int32_t> lastFrameValueIndex(const Thing& thing)
{
    for (std::size_t index = thing.values.size(); index > 0; --index)
    {
        const TplValue& value = thing.values[index - 1];
        if (value.name == "frame" && value.type == ValueType::frame)
        {
            return static_cast<std::int32_t>(index - 1);
        }
    }
    return std::nullopt;
}

std::optional<std::int32_t> frameValueIndexAtOrdinal(
    const Thing& thing,
    const std::size_t ordinal)
{
    std::size_t found = 0;
    for (std::size_t index = 0; index < thing.values.size(); ++index)
    {
        const TplValue& value = thing.values[index];
        if (value.type != ValueType::frame
            || !sed::text::iequalsAscii(value.name, "frame"))
        {
            continue;
        }
        if (found == ordinal)
        {
            return static_cast<std::int32_t>(index);
        }
        ++found;
    }
    return std::nullopt;
}

QLineEdit* framePropertyEditor(
    MainWindow& window,
    const std::int32_t valueIndex)
{
    QTreeWidget* const tree = window.findChild<QTreeWidget*>(
        QStringLiteral("thingParametersTree"));
    if (!tree)
    {
        return nullptr;
    }

    std::vector<QTreeWidgetItem*> pendingItems{};
    pendingItems.reserve(static_cast<std::size_t>(tree->topLevelItemCount()));
    for (int index = 0; index < tree->topLevelItemCount(); ++index)
    {
        pendingItems.push_back(tree->topLevelItem(index));
    }
    while (!pendingItems.empty())
    {
        QTreeWidgetItem* const item = pendingItems.back();
        pendingItems.pop_back();
        if (!item)
        {
            continue;
        }
        for (int childIndex = 0; childIndex < item->childCount(); ++childIndex)
        {
            pendingItems.push_back(item->child(childIndex));
        }

        QWidget* const row = tree->itemWidget(item, 1);
        if (!row)
        {
            continue;
        }
        for (QLineEdit* edit : row->findChildren<QLineEdit*>())
        {
            if (edit
                && edit->property("sed.frameThingIndex").isValid()
                && edit->property("sed.frameValueIndex").isValid()
                && edit->property("sed.frameThingIndex").toInt() == 0
                && edit->property("sed.frameValueIndex").toInt() == valueIndex
                && item->text(0).trimmed().startsWith(QStringLiteral("Frame ")))
            {
                return edit;
            }
        }
    }

    std::cerr << "Frame property editor lookup: treeItems="
              << tree->topLevelItemCount() << " requestedValueIndex="
              << valueIndex << " liveEditors="
              << tree->findChildren<QLineEdit*>().size() << '\n';
    return nullptr;
}

QToolButton* framePropertyAction(
    QLineEdit* const edit,
    const QString& text)
{
    QWidget* const row = edit ? edit->parentWidget() : nullptr;
    if (!row)
    {
        return nullptr;
    }
    for (QToolButton* button : row->findChildren<QToolButton*>())
    {
        if (button && button->text() == text)
        {
            return button;
        }
    }
    std::cerr << "Frame property action lookup: editor="
              << (edit ? edit->objectName().toStdString() : "<null>")
              << " editorText="
              << (edit ? edit->text().toStdString() : "<null>")
              << " parent=" << row->metaObject()->className()
              << " requested=" << text.toStdString() << " buttons=";
    for (QToolButton* button : row->findChildren<QToolButton*>())
    {
        std::cerr << '[' << button->text().toStdString() << ']';
    }
    std::cerr << '\n';
    return nullptr;
}

std::array<double, 6> thingPose(const Thing& thing)
{
    return { thing.x, thing.y, thing.z, thing.pch, thing.yaw, thing.rol };
}

bool samePose(
    const std::array<double, 6>& lhs,
    const Thing& rhs)
{
    return close(lhs[0], rhs.x, 1.0e-6)
        && close(lhs[1], rhs.y, 1.0e-6)
        && close(lhs[2], rhs.z, 1.0e-6)
        && close(lhs[3], rhs.pch, 1.0e-6)
        && close(lhs[4], rhs.yaw, 1.0e-6)
        && close(lhs[5], rhs.rol, 1.0e-6);
}

void sendModifierKey(
    GlView* const view,
    const QEvent::Type type,
    const int key,
    const Qt::KeyboardModifiers modifiers)
{
    if (!view)
    {
        return;
    }
    QKeyEvent event(type, key, modifiers);
    QApplication::sendEvent(view, &event);
}

void testFrameAddHoverMatchesCommittedPose()
{
    ScopedThingMoveSettings settings(frameAddPreferences());
    MainWindow window{};
    window.setAttribute(Qt::WA_DontShowOnScreen);
    window.resize(700, 520);
    window.setDocumentStateForTest(framePlacementState(true));
    GlView* const view = window.glViewForTest();
    if (view)
    {
        view->setLevel(
            &window.mutableDocumentStateForTest().level,
            GlView::SetLevelMode::resetView);
    }
    window.activateFrameEditModeForTest();
    selectFrameOwnerForTest(window, view, false);
    window.updateEditActionsForTest();
    window.show();
    QApplication::processEvents();

    QAction* const add = window.findChild<QAction*>(
        QStringLiteral("viewport.frame.add"));
    QToolButton* const stick = window.findChild<QToolButton*>(
        QStringLiteral("thingMoveSurfaceStickButton"));
    QToolButton* const align = window.findChild<QToolButton*>(
        QStringLiteral("thingMoveSurfaceAlignButton"));
    QToolButton* const snap = window.findChild<QToolButton*>(
        QStringLiteral("thingMoveSnapToThingButton"));
    QWidget* const options = window.findChild<QWidget*>(
        QStringLiteral("thingMoveOptionsOverlay"));

    check(view && add && stick && align && snap,
        "Frame Add uses the shared Thing contact and snap option controls");
    if (!view || !add || !stick || !align || !snap)
    {
        return;
    }

    view->fitViewToLevel();
    view->setCameraProjectionType(
        app::ViewportCamera::ProjectionType::Orthographic);
    view->setOrbitViewImmediate(0.0f, 90.0f, 0.0f);
    view->setCameraOrbitDistance(8.0f);
    add->setChecked(true);
    QApplication::processEvents();

    const std::optional<SurfacePlacementTarget> contact =
        findProjectedSolidContact(
            view, window.mutableDocumentStateForTest().level);
    check(add->isChecked() && options && !options->isHidden()
            && stick->isVisible() && align->isVisible() && snap->isVisible()
            && contact.has_value(),
        "Frame Add exposes the Thing options and resolves the same real surface target as the click");
    if (!contact)
    {
        std::cerr << "Frame Add surface fixture: mode="
                  << window.editModeForTest() << '/' << window.frameEditModeValueForTest()
                  << " viewport=" << view->width() << 'x' << view->height()
                  << " rectCenter=" << view->rect().center().x() << ','
                  << view->rect().center().y()
                  << " sectors="
                  << window.mutableDocumentStateForTest().level.sectors.size()
                  << " optionsVisible=" << (options && !options->isHidden())
                  << " buttonStates=" << stick->isChecked() << ','
                  << align->isChecked() << ',' << snap->isChecked() << '\n';
        return;
    }
    const QPoint cursor = contact->cursor;
    QCursor::setPos(view->mapToGlobal(cursor));

    const std::vector<std::byte> levelBefore = assets::saveToSedBytes(
        window.documentStateForTest().level);
    const std::size_t undoBefore = window.undoDepthForTest();
    check(invokePlacementHover(view, cursor, Qt::NoModifier),
        "Frame Add hover reaches the production placement-preview signal");
    const std::optional<Thing> preview = view->placementPreviewThingForTest();
    check(preview.has_value(),
        "Frame Add hover publishes the owning Thing at the candidate frame pose");
    check(assets::saveToSedBytes(window.documentStateForTest().level) == levelBefore
            && window.undoDepthForTest() == undoBefore,
        "Frame Add hover leaves the document and undo history unchanged");
    if (!preview)
    {
        reportFramePlacementDiagnostic(window, "Frame Add hover preview");
        return;
    }

    const std::optional<Thing> expectedBase = window.buildFramePlacementPoseForTest(
        0, contact->point, contact->normal, Qt::NoModifier, true);
    const bool previewMatchesBase = expectedBase
        && samePose(thingPose(*preview), *expectedBase);
    check(previewMatchesBase,
        "Frame Add hover resolves all six base-option pose fields from the actual surface target");
    if ( !previewMatchesBase )
    {
        reportFramePlacementDiagnostic(window, "Frame Add base pose mismatch");
    }

    const bool clicked = invokeFrameAddClick(
        view,
        contact->point,
        contact->surface.sector,
        contact->surface.surface,
        cursor,
        Qt::NoModifier);
    const editor::DocumentState committed = window.documentStateForTest();
    const std::optional<std::array<double, 6>> stored =
        committed.level.things.empty()
            ? std::nullopt : lastFramePose(committed.level.things.front());
    const std::optional<std::int32_t> storedIndex =
        committed.level.things.empty()
            ? std::nullopt : lastFrameValueIndex(committed.level.things.front());
    check(clicked && stored && samePose(*stored, *preview),
        "Frame Add commits the exact six-field pose shown by hover");
    check(storedIndex
            && committed.selection.frames.size() == 1u
            && committed.selection.frames.contains(
                level::FrameRef{ 0, *storedIndex })
            && view->activeThingIndex() == 0
            && view->activeFrameValueIndex() == *storedIndex
            && window.undoDepthForTest() == undoBefore + 1u,
        "Frame Add keeps the raw FrameRef selected after numframes synchronization inside one undo transaction");
    const auto localSprite = std::find_if(
        preview->values.begin(), preview->values.end(),
        [](const TplValue& value)
        {
            return value.name == "sprite" && value.type == ValueType::str;
        });
    check(localSprite != preview->values.end()
            && localSprite->stringValue == "authored_override.spr",
        "Frame Add preview preserves the owner's authored local SPR override");
}

void testFrameAddRefreshesSamePixelAfterCameraChange()
{
    ScopedThingMoveSettings settings(frameAddPreferences());
    MainWindow window{};
    window.setAttribute(Qt::WA_DontShowOnScreen);
    window.resize(700, 520);
    window.setDocumentStateForTest(framePlacementState(true));
    GlView* const view = window.glViewForTest();
    if ( view )
    {
        view->setLevel(
            &window.mutableDocumentStateForTest().level,
            GlView::SetLevelMode::resetView);
    }
    window.activateFrameEditModeForTest();
    selectFrameOwnerForTest(window, view, false);
    window.updateEditActionsForTest();
    window.show();
    QApplication::processEvents();

    QAction* const add = window.findChild<QAction*>(
        QStringLiteral("viewport.frame.add"));
    check(view && add,
        "Frame Add camera invalidation uses the production viewport and action");
    if ( !view || !add )
    {
        return;
    }

    view->fitViewToLevel();
    view->setCameraProjectionType(
        app::ViewportCamera::ProjectionType::Orthographic);
    view->setOrbitViewImmediate(0.0f, 90.0f, 0.0f);
    view->setCameraOrbitDistance(8.0f);
    add->setChecked(true);
    QApplication::processEvents();

    const std::optional<SurfacePlacementTarget> initialContact =
        findProjectedSolidContact(
            view, window.mutableDocumentStateForTest().level);
    check(initialContact.has_value(),
        "Frame Add camera invalidation starts from a real collision surface");
    if ( !initialContact )
    {
        return;
    }

    const QPoint cursor = initialContact->cursor;
    QCursor::setPos(view->mapToGlobal(cursor));
    check(invokePlacementHover(view, cursor, Qt::NoModifier),
        "Frame Add camera invalidation seeds the production hover preview");
    const std::optional<Thing> beforeCameraChange =
        view->placementPreviewThingForTest();
    check(beforeCameraChange.has_value(),
        "Frame Add camera invalidation captures the first candidate pose");
    if ( !beforeCameraChange )
    {
        reportFramePlacementDiagnostic(window, "Frame Add camera initial preview");
        return;
    }

    // Keep the pointer on one pixel while changing the ray through the model.
    // The refreshed candidate must track the new surface hit before click.
    view->setOrbitViewImmediate(30.0f, 80.0f, 0.0f);
    QApplication::processEvents();
    bool haveSurface = false;
    SurfaceRef surface{};
    QVector3D surfaceNormal{};
    TVector documentPosition{};
    const std::optional<QVector3D> position = view->placementPointForViewport(
        cursor,
        &haveSurface,
        &surface,
        &surfaceNormal,
        true,
        false,
        &documentPosition);
    const std::optional<Thing> expected = position && haveSurface
        ? window.buildFramePlacementPoseForTest(
              0,
              *position,
              surfaceNormal,
              Qt::NoModifier,
              haveSurface)
        : std::nullopt;
    const std::optional<Thing> refreshed = view->placementPreviewThingForTest();
    const bool cameraMovedTarget = expected
        && !samePose(thingPose(*beforeCameraChange), *expected);
    const bool refreshedMatches = position && haveSurface
        && surface.sector >= 0 && surface.surface >= 0
        && expected && refreshed && cameraMovedTarget
        && samePose(thingPose(*refreshed), *expected);
    check(refreshedMatches,
        "Frame Add refreshes its same-pixel hover pose after the camera changes");
    if ( !refreshedMatches )
    {
        reportFramePlacementDiagnostic(window, "Frame Add camera pose mismatch");
    }
    if ( !expected || !refreshed || !cameraMovedTarget )
    {
        return;
    }

    const std::vector<std::byte> levelBefore = assets::saveToSedBytes(
        window.documentStateForTest().level);
    const std::size_t undoBefore = window.undoDepthForTest();
    const bool clicked = invokeFrameAddClick(
        view,
        initialContact->point,
        initialContact->surface.sector,
        initialContact->surface.surface,
        cursor,
        Qt::NoModifier);
    const editor::DocumentState committed = window.documentStateForTest();
    const std::optional<std::array<double, 6>> stored =
        committed.level.things.empty()
            ? std::nullopt : lastFramePose(committed.level.things.front());
    check(clicked && stored && samePose(*stored, *refreshed)
            && assets::saveToSedBytes(window.documentStateForTest().level)
                != levelBefore
            && window.undoDepthForTest() == undoBefore + 1u,
        "Frame Add commits the refreshed same-pixel camera target exactly once");
}

void testFrameAddModifierIsResolvedOnceAndRestored()
{
    ScopedThingMoveSettings settings(frameAddPreferences());

    MainWindow window{};
    window.setAttribute(Qt::WA_DontShowOnScreen);
    window.resize(700, 520);
    window.setDocumentStateForTest(framePlacementState(false));
    GlView* const view = window.glViewForTest();
    if (view)
    {
        view->setLevel(
            &window.mutableDocumentStateForTest().level,
            GlView::SetLevelMode::resetView);
    }
    window.activateFrameEditModeForTest();
    selectFrameOwnerForTest(window, view, false);
    window.updateEditActionsForTest();
    window.show();
    QApplication::processEvents();
    QAction* const add = window.findChild<QAction*>(
        QStringLiteral("viewport.frame.add"));
    QToolButton* const stick = window.findChild<QToolButton*>(
        QStringLiteral("thingMoveSurfaceStickButton"));
    QToolButton* const snap = window.findChild<QToolButton*>(
        QStringLiteral("thingMoveSnapToThingButton"));
    if (view)
    {
        view->fitViewToLevel();
        view->setCameraProjectionType(
            app::ViewportCamera::ProjectionType::Orthographic);
        view->setOrbitViewImmediate(0.0f, 90.0f, 0.0f);
        view->setCameraOrbitDistance(8.0f);
    }
    if (add)
    {
        add->setChecked(true);
    }
    QApplication::processEvents();

    check(view && add && stick && stick->isChecked()
            && snap && !snap->isChecked(),
        "Frame Add modifier fixture starts from the persisted Surface Stick and Thing Snap settings");
    if (!view || !add || !stick || !snap)
    {
        return;
    }

    const std::optional<SurfacePlacementTarget> contact =
        findProjectedSolidContact(
            view, window.mutableDocumentStateForTest().level);
    check(contact.has_value(),
        "Frame Add modifier fixture resolves a projected solid contact");
    if (!contact)
    {
        QWidget* const options = window.findChild<QWidget*>(
            QStringLiteral("thingMoveOptionsOverlay"));
        std::cerr << "Frame Add modifier surface fixture: mode="
                  << window.editModeForTest() << '/' << window.frameEditModeValueForTest()
                  << " viewport=" << view->width() << 'x' << view->height()
                  << " rectCenter=" << view->rect().center().x() << ','
                  << view->rect().center().y()
                  << " sectors="
                  << window.mutableDocumentStateForTest().level.sectors.size()
                  << " optionsVisible=" << (options && !options->isHidden())
                  << " stick=" << stick->isChecked()
                  << " snap=" << snap->isChecked() << '\n';
        return;
    }
    const QPoint cursor = contact->cursor;
    QCursor::setPos(view->mapToGlobal(cursor));
    check(invokePlacementHover(view, cursor, Qt::NoModifier),
        "Frame Add base hover reaches the production placement-preview signal");
    const std::optional<Thing> basePreview = view->placementPreviewThingForTest();
    const std::vector<std::byte> levelBefore = assets::saveToSedBytes(
        window.documentStateForTest().level);
    const std::size_t undoBefore = window.undoDepthForTest();
    const std::optional<Thing> baseExpected = window.buildFramePlacementPoseForTest(
        0, contact->point, contact->normal, Qt::NoModifier, true);

    sendModifierKey(view, QEvent::KeyPress, Qt::Key_Shift, Qt::ShiftModifier);
    check(!stick->isChecked(),
        "Frame Add shows the configured Shift inversion while the key is held");

    bool freeSurfaceHit = true;
    SurfaceRef freeSurfaceRef{};
    QVector3D freeSurfaceNormal{};
    TVector freeDocumentPoint{};
    const std::optional<QVector3D> freeTarget = view->placementPointForViewport(
        cursor,
        &freeSurfaceHit,
        &freeSurfaceRef,
        &freeSurfaceNormal,
        false,
        true,
        &freeDocumentPoint);
    const std::optional<Thing> heldExpected = freeTarget
        ? window.buildFramePlacementPoseForTest(
              0, *freeTarget, freeSurfaceNormal, Qt::NoModifier,
              freeSurfaceHit)
        : std::nullopt;

    check(invokePlacementHover(view, cursor, Qt::ShiftModifier),
        "Frame Add Shift preview reaches the production placement-preview signal");
    const std::optional<Thing> heldPreview = view->placementPreviewThingForTest();
    const bool baseMatches = basePreview && baseExpected
        && samePose(thingPose(*basePreview), *baseExpected);
    check(baseMatches,
        "Frame Add base hover matches the independently resolved surface-contact pose");
    const bool heldMatches = freeTarget && !freeSurfaceHit && heldPreview
        && heldExpected && samePose(thingPose(*heldPreview), *heldExpected);
    check(heldMatches,
        "Frame Add Shift hover applies the modifier once and matches the independently resolved free-placement pose");
    if ( !baseMatches || !heldMatches )
    {
        reportFramePlacementDiagnostic(window, "Frame Add modifier preview");
    }
    check(assets::saveToSedBytes(window.documentStateForTest().level) == levelBefore
            && window.undoDepthForTest() == undoBefore,
        "Frame Add base and modifier previews leave the document and undo history unchanged");

    const bool clicked = invokeFrameAddClick(
        view,
        contact->point,
        contact->surface.sector,
        contact->surface.surface,
        cursor,
        Qt::ShiftModifier);
    const editor::DocumentState committed = window.documentStateForTest();
    const std::optional<std::array<double, 6>> stored =
        committed.level.things.empty()
            ? std::nullopt : lastFramePose(committed.level.things.front());
    const bool sameCommittedPose = heldPreview && stored
        && samePose(*stored, *heldPreview);

    sendModifierKey(view, QEvent::KeyRelease, Qt::Key_Shift, Qt::NoModifier);
    const QSettings restoredSettings = settingsStore();
    check(stick->isChecked()
            && restoredSettings.value(QStringLiteral("thing_move/surface_stick")).toBool()
            && restoredSettings.value(QStringLiteral("thing_move/surface_align")).toBool() == false
            && restoredSettings.value(QStringLiteral("thing_move/snap_to_thing")).toBool() == false
            && restoredSettings.value(QStringLiteral("thing_move/snap_rotate")).toBool(),
        "Frame Add restores all four saved option controls after the real Shift key is released");
    check(clicked && heldPreview && stored && heldExpected
            && sameCommittedPose && samePose(*stored, *heldExpected),
        "Frame Add applies a held modifier exactly once and commits the same six-field free-placement pose shown by hover");
}

void testLocalSpriteOverrideControlsFrameAlignment()
{
    ScopedThingMoveSettings settings({{
        { QStringLiteral("thing_move/surface_stick"), true },
        { QStringLiteral("thing_move/surface_align"), true },
        { QStringLiteral("thing_move/snap_to_thing"), false },
        { QStringLiteral("thing_move/snap_rotate"), true },
    }});
    MainWindow window{};
    window.setAttribute(Qt::WA_DontShowOnScreen);
    window.resize(700, 520);
    window.setDocumentStateForTest(framePlacementState(true));
    GlView* const view = window.glViewForTest();
    if (view)
    {
        view->setLevel(
            &window.mutableDocumentStateForTest().level,
            GlView::SetLevelMode::resetView);
    }
    window.activateFrameEditModeForTest();
    selectFrameOwnerForTest(window, view, false);
    window.updateEditActionsForTest();
    window.show();
    QApplication::processEvents();
    QAction* const add = window.findChild<QAction*>(
        QStringLiteral("viewport.frame.add"));
    QToolButton* const stick = window.findChild<QToolButton*>(
        QStringLiteral("thingMoveSurfaceStickButton"));
    QToolButton* const align = window.findChild<QToolButton*>(
        QStringLiteral("thingMoveSurfaceAlignButton"));
    if (view)
    {
        view->fitViewToLevel();
        view->setCameraProjectionType(
            app::ViewportCamera::ProjectionType::Orthographic);
        view->setOrbitViewImmediate(0.0f, 90.0f, 0.0f);
        view->setCameraOrbitDistance(8.0f);
    }
    if (add)
    {
        add->setChecked(true);
    }
    QApplication::processEvents();

    const std::optional<SurfacePlacementTarget> contact = view
        ? findProjectedSolidContact(
              view, window.mutableDocumentStateForTest().level)
        : std::nullopt;
    if (!contact)
    {
        const QWidget* const options = window.findChild<QWidget*>(
            QStringLiteral("thingMoveOptionsOverlay"));
        std::cerr << "Frame Add local SPR surface fixture: mode="
                  << window.editModeForTest() << '/'
                  << window.frameEditModeValueForTest()
                  << " viewport=" << (view ? view->width() : -1) << 'x'
                  << (view ? view->height() : -1)
                  << " sectors="
                  << window.mutableDocumentStateForTest().level.sectors.size()
                  << " addChecked=" << (add && add->isChecked())
                  << " optionsVisible=" << (options && !options->isHidden())
                  << " buttons=" << (stick && stick->isChecked()) << ','
                  << (align && align->isChecked()) << '\n';
        check(false,
            "Frame Add local SPR fixture resolves a projected solid contact");
        return;
    }
    const QPoint cursor = contact->cursor;
    QCursor::setPos(view->mapToGlobal(cursor));
    const bool invoked = invokePlacementHover(view, cursor, Qt::NoModifier);
    const std::optional<Thing> preview = view->placementPreviewThingForTest();
    if ( !preview )
    {
        reportFramePlacementDiagnostic(window, "Frame Add local sprite preview");
    }
    const TVector angles = preview
        ? TVector{preview->pch, preview->yaw, preview->rol}
        : TVector::zero;
    const TMatrix43 aligned = TMatrix43::Create(TVector::zero, angles);
    const TVector surfaceVector{
        contact->normal.x(), contact->normal.y(), contact->normal.z()};
    const auto localSprite = preview
        ? std::find_if(
              preview->values.begin(), preview->values.end(),
              [](const TplValue& value)
              {
                  return value.name == "sprite" && value.type == ValueType::str;
              })
        : std::vector<TplValue>::const_iterator{};
    const bool alignedPreview = view && add && stick && align
            && contact->surface.sector == 0
            && contact->surface.surface >= 0
            && invoked && preview && localSprite != preview->values.end()
            && localSprite->stringValue == "authored_override.spr"
            && aligned.lvec.Dot(surfaceVector) > 0.999;
    check(alignedPreview,
        "Frame Add preview aligns the local sprite override's forward axis to the real contacted surface");
    if ( !alignedPreview )
    {
        reportFramePlacementDiagnostic(window, "Frame Add local SPR alignment");
    }
}

void testFrameAddSurfaceAlignHoldBindingIsVisibleAndConfigurable()
{
    ScopedThingMoveSettings settings({{
        { QStringLiteral("thing_move/surface_stick"), true },
        { QStringLiteral("thing_move/surface_align"), false },
        { QStringLiteral("thing_move/snap_to_thing"), false },
        { QStringLiteral("thing_move/snap_rotate"), true },
    }});
    MainWindow window{};
    const QRect availableGeometry = QApplication::primaryScreen()
        ? QApplication::primaryScreen()->availableGeometry()
        : QRect(0, 0, 700, 520);
    window.resize(
        std::min(700, availableGeometry.width()),
        std::min(520, availableGeometry.height()));
    window.move(availableGeometry.center() - window.rect().center());
    window.setDocumentStateForTest(framePlacementState(false));
    GlView* const view = window.glViewForTest();
    if (view)
    {
        view->setLevel(
            &window.mutableDocumentStateForTest().level,
            GlView::SetLevelMode::resetView);
    }
    window.activateFrameEditModeForTest();
    selectFrameOwnerForTest(window, view, false);
    window.updateEditActionsForTest();
    window.show();
    QApplication::processEvents();

    QAction* const add = window.findChild<QAction*>(
        QStringLiteral("viewport.frame.add"));
    QAction* const alignBinding = window.findChild<QAction*>(
        QStringLiteral("thing_move.modifier.invert_surface_align"));
    QToolButton* const stick = window.findChild<QToolButton*>(
        QStringLiteral("thingMoveSurfaceStickButton"));
    QToolButton* const align = window.findChild<QToolButton*>(
        QStringLiteral("thingMoveSurfaceAlignButton"));
    QToolButton* const snap = window.findChild<QToolButton*>(
        QStringLiteral("thingMoveSnapToThingButton"));
    if (view)
    {
        view->fitViewToLevel();
        view->setCameraProjectionType(
            app::ViewportCamera::ProjectionType::Orthographic);
        view->setOrbitViewImmediate(0.0f, 90.0f, 0.0f);
        view->setCameraOrbitDistance(8.0f);
    }
    if (add)
    {
        add->setChecked(true);
    }
    QApplication::processEvents();

    check(view && add && alignBinding && stick && align && snap,
        "Frame Add surface-alignment binding fixture exposes its production controls");
    if (!view || !add || !alignBinding || !stick || !align || !snap)
    {
        return;
    }

    const QList<QKeySequence> originalBindings = alignBinding->shortcuts();
    alignBinding->setShortcut(QKeySequence(
        QStringLiteral("Ctrl+Shift"), QKeySequence::PortableText));
    window.syncSelectionModifierBindingsForTest();
    window.updateEditActionsForTest();
    QApplication::processEvents();
    check(align->toolTip().contains(QStringLiteral("Ctrl Shift")),
        "Frame Add Align to surface tooltip exposes the configured temporary hold binding");

    const std::optional<SurfacePlacementTarget> plannedContact =
        findProjectedSolidContact(
            view, window.mutableDocumentStateForTest().level);
    check(plannedContact.has_value(),
        "Frame Add align-binding fixture resolves a projected solid contact");
    if ( !plannedContact )
    {
        alignBinding->setShortcuts(originalBindings);
        window.syncSelectionModifierBindingsForTest();
        return;
    }
    const QPoint expectedGlobal = view->mapToGlobal(plannedContact->cursor);
    QCursor::setPos(expectedGlobal);
    QPoint actualGlobal{};
    const bool requestedPixelReached = waitForCursorGlobalPosition(
        expectedGlobal, &actualGlobal);
    const QPoint actualPointer = view->mapFromGlobal(actualGlobal);
    const std::optional<SurfacePlacementTarget> actualContact =
        resolveSolidContactAt(
            view,
            window.mutableDocumentStateForTest().level,
            actualPointer);
    const bool sameCollisionSurface = actualContact
        && actualContact->surface == plannedContact->surface;
    const bool realCursorContactValid = sameCollisionSurface;
    if ( !realCursorContactValid )
    {
        reportFramePlacementCursorDiagnostic(
            window,
            view,
            plannedContact->cursor,
            expectedGlobal,
            actualGlobal,
            actualPointer);
        std::cerr << "Frame Add cursor contact diagnostic: requestedPixelReached="
                  << requestedPixelReached
                  << " plannedSurface=" << plannedContact->surface.sector
                  << '/' << plannedContact->surface.surface
                  << " actualContact=" << actualContact.has_value()
                  << " actualSurface=";
        if ( actualContact )
        {
            std::cerr << actualContact->surface.sector << '/'
                      << actualContact->surface.surface;
        }
        else
        {
            std::cerr << "<none>";
        }
        std::cerr << " sameCollisionSurface=" << sameCollisionSurface << '\n';
    }
    check(realCursorContactValid,
        "Frame Add align-binding fixture resolves the settled pointer on the planned solid face");
    if (!realCursorContactValid)
    {
        alignBinding->setShortcuts(originalBindings);
        window.syncSelectionModifierBindingsForTest();
        return;
    }
    const SurfacePlacementTarget contact = *actualContact;
    const QPoint cursor = actualPointer;
    const std::vector<std::byte> levelBefore = assets::saveToSedBytes(
        window.documentStateForTest().level);
    const std::size_t undoBefore = window.undoDepthForTest();
    check(invokePlacementHover(view, cursor, Qt::NoModifier),
        "Frame Add alignment fixture starts from its real unmodified hover");
    const std::optional<Thing> basePreview = view->placementPreviewThingForTest();

    sendModifierKey(view, QEvent::KeyPress, Qt::Key_Control,
        Qt::ControlModifier);
    sendModifierKey(view, QEvent::KeyPress, Qt::Key_Shift,
        Qt::ControlModifier | Qt::ShiftModifier);
    const std::optional<Thing> alignedPreview =
        view->placementPreviewThingForTest();
    const TVector alignedAngles = alignedPreview
        ? TVector{alignedPreview->pch, alignedPreview->yaw, alignedPreview->rol}
        : TVector::zero;
    const TMatrix43 aligned = TMatrix43::Create(TVector::zero, alignedAngles);
    const TVector surfaceVector{
        contact.normal.x(), contact.normal.y(), contact.normal.z()};
    check(align->isChecked(),
        "Frame Add Ctrl+Shift marks surface alignment active in the option strip");
    if (!alignedPreview || aligned.uvec.Dot(surfaceVector) <= 0.999)
    {
        reportFramePlacementDiagnostic(window, "Frame Add Ctrl+Shift pose");
        std::cerr << "Frame Add Ctrl+Shift state: stick=" << stick->isChecked()
                  << " align=" << align->isChecked()
                  << " snap=" << snap->isChecked()
                  << " base=";
        if (basePreview)
        {
            std::cerr << basePreview->pch << ',' << basePreview->yaw << ','
                      << basePreview->rol;
        }
        else
        {
            std::cerr << "<none>";
        }
        std::cerr << " held=";
        if (alignedPreview)
        {
            std::cerr << alignedPreview->pch << ',' << alignedPreview->yaw << ','
                      << alignedPreview->rol << " dot="
                      << aligned.uvec.Dot(surfaceVector);
        }
        else
        {
            std::cerr << "<none>";
        }
        std::cerr << '\n';
    }
    check(stick->isChecked() && align->isChecked() && !snap->isChecked()
            && basePreview && alignedPreview
            && aligned.uvec.Dot(surfaceVector) > 0.999,
        "Frame Add Ctrl+Shift toggles only surface alignment and aligns the preview owner to the picked surface");
    check(assets::saveToSedBytes(window.documentStateForTest().level) == levelBefore
            && window.undoDepthForTest() == undoBefore,
        "Frame Add temporary surface alignment leaves the document and history unchanged");

    sendModifierKey(view, QEvent::KeyRelease, Qt::Key_Shift,
        Qt::ControlModifier | Qt::ShiftModifier);
    sendModifierKey(view, QEvent::KeyRelease, Qt::Key_Control,
        Qt::ControlModifier);
    check(!align->isChecked()
            && settingsStore().value(QStringLiteral("thing_move/surface_align"), false).toBool() == false,
        "Frame Add restores the saved alignment preference when Ctrl+Shift is released");

    alignBinding->setShortcut(QKeySequence(Qt::Key_Alt));
    window.syncSelectionModifierBindingsForTest();
    window.updateEditActionsForTest();
    QApplication::processEvents();
    check(window.thingMoveSurfaceAlignInvertModifierBindingsForTest()
              == ui::shortcuts::ModifierBindings{Qt::AltModifier},
        "Frame Add custom Alt binding publishes its exact cached modifier before native key input");
    sendModifierKey(view, QEvent::KeyPress, Qt::Key_Alt, Qt::AltModifier);
    const std::optional<Thing> customAlignedPreview =
        view->placementPreviewThingForTest();
    const TVector customAngles = customAlignedPreview
        ? TVector{customAlignedPreview->pch, customAlignedPreview->yaw, customAlignedPreview->rol}
        : TVector::zero;
    const TMatrix43 customAligned = TMatrix43::Create(TVector::zero, customAngles);
    check(align->isChecked(),
        "Frame Add custom Alt binding marks surface alignment active in the option strip");
    if (!customAlignedPreview || customAligned.uvec.Dot(surfaceVector) <= 0.999)
    {
        reportFramePlacementDiagnostic(window, "Frame Add custom Alt pose");
        std::cerr << "Frame Add custom Alt state: stick=" << stick->isChecked()
                  << " align=" << align->isChecked()
                  << " snap=" << snap->isChecked() << " held=";
        if (customAlignedPreview)
        {
            std::cerr << customAlignedPreview->pch << ','
                      << customAlignedPreview->yaw << ','
                      << customAlignedPreview->rol << " dot="
                      << customAligned.uvec.Dot(surfaceVector);
        }
        else
        {
            std::cerr << "<none>";
        }
        std::cerr << '\n';
    }
    check(align->isChecked() && customAlignedPreview
            && customAligned.uvec.Dot(surfaceVector) > 0.999,
        "Frame Add surface alignment follows a custom Alt binding while the pointer remains still");
    sendModifierKey(view, QEvent::KeyRelease, Qt::Key_Alt, Qt::AltModifier);
    check(!align->isChecked(),
        "Frame Add custom surface-alignment binding restores the persisted option on release");
    alignBinding->setShortcuts(originalBindings);
    window.syncSelectionModifierBindingsForTest();
}

void testFrameMoveCachesSupportsAndKeepsOwnerSnap()
{
    ScopedThingMoveSettings settings({{
        { QStringLiteral("thing_move/surface_stick"), false },
        { QStringLiteral("thing_move/surface_align"), false },
        { QStringLiteral("thing_move/snap_to_thing"), true },
        { QStringLiteral("thing_move/snap_rotate"), false },
    }});
    QTemporaryDir resourceDirectory{};
    const std::filesystem::path resourceRoot = resourceDirectory.isValid()
        ? std::filesystem::path(resourceDirectory.path().toStdWString())
        : std::filesystem::path{};
    const bool wroteModel = resourceDirectory.isValid()
        && writeFrameMoveTinyModelFixture(resourceRoot);
    check(wroteModel,
        "Frame Move snap fixture writes its tiny model resource");
    if (!wroteModel)
    {
        return;
    }

    MainWindow window{};
    window.setAttribute(Qt::WA_DontShowOnScreen);
    window.resize(700, 520);
    window.setResourcePathsForTest({ resourceRoot }, {});
    const std::optional<ThingBox> modelBox = sed::thing::loadModel3doBox(
        window.resourceLocatorForTest(), "frame_move_tiny.3do");
    check(modelBox
            && close(modelBox->x1, -0.001, 1.0e-12)
            && close(modelBox->x2, 0.001, 1.0e-12)
            && close(modelBox->y1, -0.001, 1.0e-12)
            && close(modelBox->y2, 0.001, 1.0e-12)
            && close(modelBox->z1, -0.001, 1.0e-12)
            && close(modelBox->z2, 0.001, 1.0e-12),
        "Frame Move snap fixture loads the authored tiny 3DO bounds used by the expected contact");
    if (!modelBox)
    {
        return;
    }
    window.setDocumentStateForTest(frameMoveState());
    GlView* const view = window.glViewForTest();
    if (view)
    {
        view->setLevel(
            &window.mutableDocumentStateForTest().level,
            GlView::SetLevelMode::resetView);
    }
    window.activateFrameEditModeForTest();
    selectFrameOwnerForTest(window, view, true);
    window.updateEditActionsForTest();
    window.show();
    QApplication::processEvents();

    check(view != nullptr,
        "Frame Move cache fixture exposes the production viewport");
    if (!view)
    {
        return;
    }
    const std::vector<std::byte> levelBefore = assets::saveToSedBytes(
        window.documentStateForTest().level);
    const std::size_t undoBefore = window.undoDepthForTest();
    const bool began = view->beginFrameMoveDragForTest(0, 0, false, true);
    if (!began)
    {
        std::cerr << "Frame Move gesture fixture: mode="
                  << window.editModeForTest() << '/'
                  << window.frameEditModeValueForTest()
                  << " frameEditEnabled=" << view->frameEditEnabled()
                  << " active=" << view->activeThingIndex() << ':'
                  << view->activeFrameValueIndex()
                  << " selection="
                  << window.mutableDocumentStateForTest().selection.things.size()
                  << '/' << window.mutableDocumentStateForTest().selection.frames.size()
                  << " things="
                  << window.mutableDocumentStateForTest().level.things.size()
                  << " viewport=" << view->width() << 'x' << view->height()
                  << '\n';
    }
    check(began && view->transformPreviewActiveForTest(),
        "Frame Move starts one production transform gesture for the selected virtual frame");
    if (!began)
    {
        return;
    }

    view->updateFrameMoveDragSampleForTest(0);
    const std::uint64_t firstResourceWork =
        view->thingMoveResourceResolveCountForTest();
    const std::uint64_t firstBoundsWork =
        view->thingMoveBoundsResolveCountForTest();
    constexpr std::uint64_t kPointerSamples = 5;
    for (std::uint64_t sample = 1; sample < kPointerSamples; ++sample)
    {
        view->updateFrameMoveDragSampleForTest(0);
    }
    const std::uint64_t visits = view->thingMoveSnapAnchorVisitCountForTest();
    const QVector3D preview = view->transformPreviewPointForTest(QVector3D{});
    check(view->thingMoveDragSampleCountForTest() == kPointerSamples
            && view->thingMoveResourceResolveCountForTest() == firstResourceWork
            && view->thingMoveBoundsResolveCountForTest() == firstBoundsWork,
        "Frame Move reuses captured moving and stationary resource/bounds work across real drag samples");
    check(visits == kPointerSamples,
        "Frame Move retains its stationary owning Thing as the valid snap candidate while culling the distant anchor");
    // Both moving and owner 3DO boxes span [-.001, .001]. The owner is at
    // (.005, .006, .18), so joining the moving box's +X face to the owner's
    // -X face gives .004 - .001 = .003; weighted lateral scoring keeps the
    // other two coordinates at their authored values.
    check(close(preview.x(), 0.003, 2.0e-6)
            && close(preview.y(), 0.006, 2.0e-6)
            && close(preview.z(), 0.18, 2.0e-6),
        "Frame Move's conservative culling keeps the owner's exact weighted snap candidate");
    if (visits != kPointerSamples
        || !close(preview.x(), 0.003, 2.0e-6)
        || !close(preview.y(), 0.006, 2.0e-6)
        || !close(preview.z(), 0.18, 2.0e-6))
    {
        std::cerr << "Frame Move snap diagnostic: visits=" << visits
                  << " samples=" << kPointerSamples
                  << " preview=" << preview.x() << ',' << preview.y()
                  << ',' << preview.z()
                  << " resourceResolves="
                  << view->thingMoveResourceResolveCountForTest()
                  << " boundsResolves="
                  << view->thingMoveBoundsResolveCountForTest() << '\n';
    }
    check(assets::saveToSedBytes(window.documentStateForTest().level) == levelBefore
            && window.undoDepthForTest() == undoBefore,
        "Frame Move samples keep the authored path and undo history unchanged until release");

    view->endTransformDragForTest();
    const editor::DocumentState committed = window.documentStateForTest();
    const std::optional<std::array<double, 6>> committedPose =
        committed.level.things.empty()
            ? std::nullopt : lastFramePose(committed.level.things.front());
    check(committedPose
            && close((*committedPose)[0], 0.003, 2.0e-6)
            && close((*committedPose)[1], 0.006, 2.0e-6)
            && close((*committedPose)[2], 0.18, 2.0e-6)
            && window.undoDepthForTest() == undoBefore + 1u,
        "Frame Move commits the exact owner snap result in one undoable frame edit");
}

void testFrameMoveDoesNotCopyLongOwnerFramePayloadPerSample()
{
    QTemporaryDir resourceDirectory{};
    const std::filesystem::path resourceRoot = resourceDirectory.isValid()
        ? std::filesystem::path(resourceDirectory.path().toStdWString())
        : std::filesystem::path{};
    const bool wroteModel = resourceDirectory.isValid()
        && writeFrameMoveTinyModelFixture(resourceRoot);
    check(wroteModel,
        "Long-path Frame Move fixture writes its tiny model resource");
    if (!wroteModel)
    {
        return;
    }

    assets::ResourceLocator resources{};
    resources.addRoot(resourceRoot);
    editor::DocumentState state = frameMoveState();
    Thing& owner = state.level.things.front();
    constexpr std::size_t kFrameCount = 4096u;
    owner.values.reserve(kFrameCount);
    for (std::size_t frameIndex = 1u; frameIndex < kFrameCount; ++frameIndex)
    {
        owner.values.push_back(frameValue(
            static_cast<double>(frameIndex) * 0.001,
            0.0,
            0.0,
            0.0,
            0.0,
            0.0));
    }
    Thing nearAnchor = state.level.things[1];
    nearAnchor.id = 3;
    nearAnchor.num = 2;
    nearAnchor.x = 0.02;
    nearAnchor.y = 0.0;
    nearAnchor.z = 0.0;
    for (std::size_t frameIndex = 0; frameIndex < 256u; ++frameIndex)
    {
        nearAnchor.values.push_back(frameValue(
            static_cast<double>(frameIndex) * 0.002,
            0.0,
            0.0,
            0.0,
            0.0,
            0.0));
    }
    state.level.things.push_back(std::move(nearAnchor));

    const std::optional<ThingBox> modelBox = sed::thing::loadModel3doBox(
        resources, "frame_move_tiny.3do");
    check(modelBox
            && close(modelBox->x1, -0.001, 1.0e-12)
            && close(modelBox->x2, 0.001, 1.0e-12)
            && close(modelBox->y1, -0.001, 1.0e-12)
            && close(modelBox->y2, 0.001, 1.0e-12)
            && close(modelBox->z1, -0.001, 1.0e-12)
            && close(modelBox->z2, 0.001, 1.0e-12),
        "Long-path Frame Move fixture loads its authored tiny 3DO bounds");
    if (!modelBox)
    {
        return;
    }
    GlView view{};
    view.resize(700, 520);
    view.setResourceLocator(&resources, false);
    view.setTemplateDatabase(nullptr);
    view.setLevel(&state.level, GlView::SetLevelMode::resetView);
    view.setFrameEditEnabled(true);
    QApplication::processEvents();

    const bool began = view.beginFrameMoveDragForTest(0, 0, false, true);
    check(began,
        "Long-path Frame Move begins through the real viewport gesture path");
    if (!began)
    {
        return;
    }

    view.setProperty(
        "sed.frameMoveCopiedFrameValues",
        QVariant::fromValue<qulonglong>(0));
    constexpr std::uint64_t kPointerSamples = 5u;
    for (std::uint64_t sample = 0u; sample < kPointerSamples; ++sample)
    {
        view.updateFrameMoveDragSampleForTest(0);
    }
    const std::uint64_t copiedFrameValues = view.property(
        "sed.frameMoveCopiedFrameValues").toULongLong();
    const qulonglong nonOwnerProxyValueCapacity = view.property(
        "sed.frameMoveStationaryProxyNonOwnerValueCapacity").toULongLong();
    const bool sampled = view.thingMoveDragSampleCountForTest()
        == kPointerSamples;
    check(sampled && copiedFrameValues == 0u
            && nonOwnerProxyValueCapacity < 16u,
        "Warm Frame Move samples reuse compact stationary anchors without copying raw Frame payloads or retaining oversized value capacity");
    if (!sampled || copiedFrameValues != 0u
        || nonOwnerProxyValueCapacity >= 16u)
    {
        std::cerr << "Long-path Frame Move copy diagnostic: frames="
                  << kFrameCount << " samples="
                  << view.thingMoveDragSampleCountForTest()
                  << " copiedFrameValues=" << copiedFrameValues
                  << " nonOwnerProxyValueCapacity="
                  << nonOwnerProxyValueCapacity << '\n';
    }
    view.endTransformDragForTest();
}

void testFramePropertiesUseCompactOwnerHistory()
{
    MainWindow window{};
    window.setAttribute(Qt::WA_DontShowOnScreen);
    window.resize(800, 600);
    editor::DocumentState state = framePlacementState(false);
    window.setDocumentStateForTest(std::move(state));
    GlView* const view = window.glViewForTest();
    if (view)
    {
        view->setLevel(
            &window.mutableDocumentStateForTest().level,
            GlView::SetLevelMode::resetView);
    }
    window.activateFrameEditModeForTest();
    selectFrameOwnerForTest(window, view, false);
    const std::optional<std::int32_t> initialFrameIndex =
        window.documentStateForTest().level.things.empty()
            ? std::nullopt
            : frameValueIndexAtOrdinal(
                  window.documentStateForTest().level.things.front(), 0u);
    if (initialFrameIndex)
    {
        editor::Selection selection{};
        selection.things.insert(0);
        selection.frames.insert(level::FrameRef{ 0, *initialFrameIndex });
        window.mutableDocumentStateForTest().selection = selection;
        if (view)
        {
            view->setSelectionState(-1, -1, 0, -1, selection);
            view->setSelectedFrameValueIndex(*initialFrameIndex);
        }
    }
    window.updateEditActionsForTest();
    window.rebuildPropertiesForTest();
    window.show();
    QApplication::processEvents();
    if (!view)
    {
        check(false, "Frame property actions have the production viewport");
        return;
    }

    const std::uint64_t meshRebuildsBefore = view->meshRebuildCountForTest();
    std::vector<std::int32_t> subsetIndices{};
    check(initialFrameIndex.has_value(),
        "Frame property fixture resolves the first raw Frame value index");
    QLineEdit* edit = initialFrameIndex
        ? framePropertyEditor(window, *initialFrameIndex)
        : nullptr;
    check(edit != nullptr, "Frame property edit uses the actual Properties row");
    if (!edit)
    {
        return;
    }
    edit->setText(QStringLiteral("2/3/4:5/6/7"));
    const bool editInvoked = QMetaObject::invokeMethod(
        edit, "editingFinished", Qt::DirectConnection);
    QApplication::processEvents();
    const editor::DocumentState& afterEdit = window.documentStateForTest();
    const std::optional<std::array<double, 6>> editedPose =
        afterEdit.level.things.empty()
            ? std::nullopt : lastFramePose(afterEdit.level.things.front());
    const bool editedValues = editedPose
        && close((*editedPose)[0], 2.0)
        && close((*editedPose)[1], 3.0)
        && close((*editedPose)[2], 4.0)
        && close((*editedPose)[3], 5.0)
        && close((*editedPose)[4], 6.0)
        && close((*editedPose)[5], 7.0);
    if (!editedValues)
    {
        std::cerr << "Frame property edit diagnostic: editObject="
                  << edit->objectName().toStdString() << " editText="
                  << edit->text().toStdString() << " rawIndex="
                  << *initialFrameIndex << " stored=";
        if (afterEdit.level.things.empty())
        {
            std::cerr << "<no owner>";
        }
        else
        {
            for (std::size_t valueIndex = 0;
                 valueIndex < afterEdit.level.things.front().values.size();
                 ++valueIndex)
            {
                const TplValue& value = afterEdit.level.things.front().values[valueIndex];
                std::cerr << '[' << valueIndex << ':' << value.name << ':'
                          << value.asString() << ']';
            }
        }
        std::cerr << '\n';
    }
    const bool editedFrameSelection = afterEdit.selection.frames.size() == 1u
        && initialFrameIndex
        && afterEdit.selection.frames.contains(
            level::FrameRef{ 0, *initialFrameIndex });
    const bool editUndo = window.undoForTest(&subsetIndices);
    const bool editSubset = editUndo && subsetIndices == std::vector<std::int32_t>{ 0 };
    const bool editRedo = window.redoForTest();
    check(editInvoked,
        "Frame property edit commits through the actual Properties row");
    check(editedValues,
        "Frame property edit writes the six parsed frame coordinates and angles");
    check(editedFrameSelection,
        "Frame property edit preserves its raw FrameRef");
    check(editSubset,
        "Frame property edit records an owner-subset undo");
    check(editRedo,
        "Frame property edit redoes its owner-subset transaction");
    check(view->meshRebuildCountForTest() == meshRebuildsBefore,
        "Frame property edit keeps the warm Thing geometry cache published");

    window.rebuildPropertiesForTest();
    QApplication::processEvents();
    const std::optional<std::int32_t> firstFrame = frameValueIndexAtOrdinal(
        window.documentStateForTest().level.things.front(), 0u);
    edit = firstFrame ? framePropertyEditor(window, *firstFrame) : nullptr;
    QToolButton* const duplicate = framePropertyAction(edit, QStringLiteral("Copy"));
    subsetIndices.clear();
    if (duplicate)
    {
        duplicate->click();
    }
    QApplication::processEvents();
    const editor::DocumentState& afterDuplicate = window.documentStateForTest();
    const std::optional<std::int32_t> selectedAfterDuplicate =
        frameValueIndexAtOrdinal(afterDuplicate.level.things.front(), 0u);
    const bool duplicateSelectedOriginal = selectedAfterDuplicate
        && afterDuplicate.selection.frames.size() == 1u
        && afterDuplicate.selection.frames.contains(
            level::FrameRef{ 0, *selectedAfterDuplicate });
    const bool duplicateUndo = window.undoForTest(&subsetIndices);
    const bool duplicateSubset = duplicateUndo
        && subsetIndices == std::vector<std::int32_t>{ 0 };
    const bool duplicateRedo = window.redoForTest();
    check(duplicate != nullptr,
        "Frame property Duplicate exposes its actual Copy action");
    check(level::thingCountFrames(afterDuplicate.level.things.front()) == 2u,
        "Frame property Duplicate adds one Frame value");
    check(duplicateSelectedOriginal,
        "Frame property Duplicate preserves a valid raw FrameRef selection");
    check(duplicateSubset,
        "Frame property Duplicate records a compact owner-subset undo");
    check(duplicateRedo,
        "Frame property Duplicate redoes its compact owner transaction");
    check(view->meshRebuildCountForTest() == meshRebuildsBefore,
        "Frame property Duplicate keeps the warm Thing geometry cache published");

    window.rebuildPropertiesForTest();
    QApplication::processEvents();
    const std::optional<std::int32_t> duplicateFrame = frameValueIndexAtOrdinal(
        window.documentStateForTest().level.things.front(), 1u);
    edit = duplicateFrame ? framePropertyEditor(window, *duplicateFrame) : nullptr;
    QToolButton* const remove = framePropertyAction(edit, QStringLiteral("-"));
    subsetIndices.clear();
    if (remove)
    {
        remove->click();
    }
    QApplication::processEvents();
    const editor::DocumentState& afterDelete = window.documentStateForTest();
    const std::optional<std::int32_t> selectedAfterDelete =
        frameValueIndexAtOrdinal(afterDelete.level.things.front(), 0u);
    const bool deleteSelectionValid = selectedAfterDelete
        && afterDelete.selection.frames.size() == 1u
        && afterDelete.selection.frames.contains(
            level::FrameRef{ 0, *selectedAfterDelete });
    const bool deleteUndo = window.undoForTest(&subsetIndices);
    const bool deleteSubset = deleteUndo
        && subsetIndices == std::vector<std::int32_t>{ 0 };
    const bool deleteRedo = window.redoForTest();
    check(remove != nullptr,
        "Frame property Delete exposes its actual remove action");
    check(level::thingCountFrames(afterDelete.level.things.front()) == 1u,
        "Frame property Delete removes one Frame value");
    check(deleteSelectionValid,
        "Frame property Delete preserves a valid raw FrameRef selection");
    check(deleteSubset,
        "Frame property Delete records a compact owner-subset undo");
    check(deleteRedo,
        "Frame property Delete redoes its compact owner transaction");
    check(view->meshRebuildCountForTest() == meshRebuildsBefore,
        "Frame property Delete keeps the warm Thing geometry cache published");
}

void testFramePlacementUsesBaseOrientationWithoutValidFrames()
{
    ScopedThingMoveSettings settings(frameAddPreferences());
    auto poseForState = [](editor::DocumentState state)
    {
        MainWindow window{};
        window.setDocumentStateForTest(std::move(state));
        return window.buildFramePlacementPoseForTest(
            0,
            QVector3D(8.0f, 9.0f, 10.0f),
            QVector3D(0.0f, 0.0f, 1.0f),
            Qt::NoModifier,
            false);
    };

    editor::DocumentState noFrames = framePlacementState(false);
    noFrames.level.things.front().values.clear();
    noFrames.level.things.front().pch = 11.0;
    noFrames.level.things.front().yaw = 23.0;
    noFrames.level.things.front().rol = -17.0;
    const std::optional<Thing> basePose = poseForState(std::move(noFrames));
    check(basePose && close(basePose->pch, 11.0)
            && close(basePose->yaw, 23.0) && close(basePose->rol, -17.0),
        "Frame Add without prior frames keeps the owner's authored orientation");

    editor::DocumentState malformedTail = framePlacementState(false);
    TplValue previous = frameValue(1.0, 2.0, 3.0, 7.0, 91.0, -4.0);
    previous.name = "Frame";
    malformedTail.level.things.front().values.push_back(std::move(previous));
    TplValue malformed{};
    malformed.name = "frame";
    malformed.type = ValueType::frame;
    malformed.adjType = AdjType::frame;
    malformed.stringValue = "not a frame pose";
    malformedTail.level.things.front().values.push_back(std::move(malformed));
    const std::optional<Thing> recoveredPose = poseForState(std::move(malformedTail));
    check(recoveredPose && close(recoveredPose->pch, 7.0)
            && close(recoveredPose->yaw, 91.0)
            && close(recoveredPose->rol, -4.0),
        "Frame Add skips a malformed trailing value and keeps the last valid case-insensitive frame orientation");
}

} // namespace

void runFrameAddMoveBehaviorTests()
{
    testFrameAddHoverMatchesCommittedPose();
    testFrameAddRefreshesSamePixelAfterCameraChange();
    testFrameAddModifierIsResolvedOnceAndRestored();
    testLocalSpriteOverrideControlsFrameAlignment();
    testFrameAddSurfaceAlignHoldBindingIsVisibleAndConfigurable();
    testFrameMoveCachesSupportsAndKeepsOwnerSnap();
    testFrameMoveDoesNotCopyLongOwnerFramePayloadPerSample();
    testFramePropertiesUseCompactOwnerHistory();
    testFramePlacementUsesBaseOrientationWithoutValidFrames();
}

} // namespace sed::app_tests
