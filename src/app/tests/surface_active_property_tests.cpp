#include "test_common.hpp"

#include <QAction>
#include <QComboBox>
#include <QMetaObject>
#include <QTreeWidget>

#include <memory>
#include <optional>

#include "gl_view.hpp"
#include "main_window/main_window.hpp"
#include "sed/level/build.hpp"
#include "ui/navigation/outliner_types.hpp"

namespace sed::app_tests
{
namespace
{

QTreeWidgetItem* propertyRow(QTreeWidget* properties, const QString& name)
{
    if ( !properties )
    {
        return nullptr;
    }
    for ( int row = 0; row < properties->topLevelItemCount(); ++row )
    {
        QTreeWidgetItem* item = properties->topLevelItem(row);
        if ( item && item->text(0).trimmed() == name )
        {
            return item;
        }
    }
    return nullptr;
}

std::optional<std::int32_t> displayedSurfaceOwnerSector(MainWindow& window)
{
    QTreeWidget* properties = window.findChild<QTreeWidget*>(QStringLiteral("propertiesTree"));
    QTreeWidgetItem* row = propertyRow(properties, QStringLiteral("Sector"));
    if ( !row )
    {
        return std::nullopt;
    }
    bool ok = false;
    const int sector = row->text(1).toInt(&ok);
    return ok ? std::optional<std::int32_t>{sector} : std::nullopt;
}

std::optional<int> displayedSurfaceOwnerLayer(MainWindow& window)
{
    QTreeWidget* properties = window.findChild<QTreeWidget*>(QStringLiteral("propertiesTree"));
    QTreeWidgetItem* row = propertyRow(properties, QStringLiteral("Layer"));
    auto* combo = row && properties ? qobject_cast<QComboBox*>(properties->itemWidget(row, 1)) : nullptr;
    return combo ? std::optional<int>{combo->currentData().toInt()} : std::nullopt;
}

std::optional<SurfaceRef> currentOutlinerSurface(MainWindow& window)
{
    QTreeWidget* outliner = window.findChild<QTreeWidget*>(QStringLiteral("outlinerTree"));
    QTreeWidgetItem* current = outliner ? outliner->currentItem() : nullptr;
    if ( !current )
    {
        return std::nullopt;
    }
    const OutlinerKind kind = static_cast<OutlinerKind>(current->data(0, kOutlinerKindRole).toInt());
    if ( kind == OutlinerKind::surface )
    {
        return SurfaceRef{current->data(0, kOutlinerSectorRole).toInt(), current->data(0, kOutlinerIndexRole).toInt()};
    }
    if ( kind == OutlinerKind::surfaceVertex )
    {
        return SurfaceRef{current->data(0, kOutlinerSectorRole).toInt(),
                          current->data(0, kOutlinerSurfaceRole).toInt()};
    }
    return std::nullopt;
}

bool invokeFacePick(GlView* view, const SurfaceRef ref, const Qt::KeyboardModifiers modifiers)
{
    return view &&
           QMetaObject::invokeMethod(view, "selectionPicked", Qt::DirectConnection, Q_ARG(std::int32_t, ref.sector),
                                     Q_ARG(std::int32_t, ref.surface), Q_ARG(std::int32_t, -1), Q_ARG(std::int32_t, -1),
                                     Q_ARG(Qt::KeyboardModifiers, modifiers));
}

QVector3D surfacePoint(const Level& level, const SurfaceRef ref, const std::int32_t localVertex)
{
    const Sector& sector = level.sectors[static_cast<std::size_t>(ref.sector)];
    const Surface& surface = sector.surfaces[static_cast<std::size_t>(ref.surface)];
    const std::int32_t vertexIndex = surface.vertices[static_cast<std::size_t>(localVertex)];
    const Vertex& vertex = sector.vertices[static_cast<std::size_t>(vertexIndex)];
    return QVector3D(static_cast<float>(vertex.x), static_cast<float>(vertex.y), static_cast<float>(vertex.z));
}

QVector3D surfaceEdgeMidpoint(const Level& level, const SurfaceRef ref, const std::int32_t localEdge)
{
    const Sector& sector = level.sectors[static_cast<std::size_t>(ref.sector)];
    const Surface& surface = sector.surfaces[static_cast<std::size_t>(ref.surface)];
    const std::size_t first = static_cast<std::size_t>(localEdge);
    const std::size_t second = (first + 1u) % surface.vertices.size();
    const Vertex& a = sector.vertices[static_cast<std::size_t>(surface.vertices[first])];
    const Vertex& b = sector.vertices[static_cast<std::size_t>(surface.vertices[second])];
    return QVector3D(static_cast<float>((a.x + b.x) * 0.5), static_cast<float>((a.y + b.y) * 0.5),
                     static_cast<float>((a.z + b.z) * 0.5));
}

bool invokeSubElementPick(MainWindow& window, const SurfaceRef ref, const QVector3D& point,
                          const Qt::KeyboardModifiers modifiers)
{
    GlView* view = window.glViewForTest();
    return invokeFacePick(view, ref, modifiers) &&
           QMetaObject::invokeMethod(view, "surfaceClicked", Qt::DirectConnection, Q_ARG(QVector3D, point),
                                     Q_ARG(std::int32_t, ref.sector), Q_ARG(std::int32_t, ref.surface),
                                     Q_ARG(QPoint, QPoint(12, 12)), Q_ARG(Qt::KeyboardModifiers, modifiers));
}

bool invokeEdgePick(MainWindow& window, const SurfaceRef ref, const std::int32_t localEdge,
                    const Qt::KeyboardModifiers modifiers)
{
    GlView* view = window.glViewForTest();
    if ( !view )
    {
        return false;
    }

    // Match GlView::mouseReleaseEvent exactly: the viewport mutates its exact
    // transient edge set first, MainWindow then publishes the derived semantic
    // owner selection, and surfaceClicked supplies the hit-position context.
    view->applySurfaceEdgePointSelectionForTest(ref, localEdge, modifiers);
    return invokeFacePick(view, ref, modifiers) &&
           QMetaObject::invokeMethod(
               view, "surfaceClicked", Qt::DirectConnection,
               Q_ARG(QVector3D, surfaceEdgeMidpoint(window.documentStateForTest().level, ref, localEdge)),
               Q_ARG(std::int32_t, ref.sector), Q_ARG(std::int32_t, ref.surface), Q_ARG(QPoint, QPoint(12, 12)),
               Q_ARG(Qt::KeyboardModifiers, modifiers));
}

bool displaysOwner(MainWindow& window, const SurfaceRef ref)
{
    return displayedSurfaceOwnerSector(window) == std::optional<std::int32_t>{ref.sector} &&
           displayedSurfaceOwnerLayer(window) == std::optional<int>{ref.sector};
}

} // namespace

void runSurfaceActivePropertyTests()
{
    auto window = std::make_unique<MainWindow>();
    window->setAttribute(Qt::WA_DontShowOnScreen);

    editor::DocumentState state{};
    state.level.kind = ProjectType::IJIM;
    state.level.layers = {"Layer0", "Layer1"};
    Sector& firstSector = level::levelCreateCube(state.level, 0.0, 0.0, 0.0, TVector::up, TVector::right);
    firstSector.layer = 0;
    firstSector.surfaces.front().geo = 1;
    Sector& secondSector = level::levelCreateCube(state.level, 4.0, 0.0, 0.0, TVector::up, TVector::right);
    secondSector.layer = 1;
    secondSector.surfaces.front().geo = 4;

    window->setDocumentStateForTest(std::move(state));
    window->activateSurfaceEditModeForTest();
    window->activateSurfaceFaceSubModeForTest();
    GlView* view = window->glViewForTest();
    const SurfaceRef first{0, 0};
    const SurfaceRef second{1, 0};

    const bool firstFacePicked = invokeFacePick(view, first, Qt::NoModifier);
    const bool secondFacePicked = invokeFacePick(view, second, Qt::ShiftModifier);
    check(firstFacePicked && secondFacePicked && view->selectedSurfaceRef() == std::optional<SurfaceRef>{second} &&
              window->documentStateForTest().selection.surfaces.contains(first) &&
              window->documentStateForTest().selection.surfaces.contains(second) && displaysOwner(*window, second) &&
              currentOutlinerSurface(*window) == std::optional<SurfaceRef>{second},
          "Surface Face Shift+LMB publishes the clicked primary to Item Edit and "
          "the Outliner instead of unordered-set order");

    const bool activeFaceRemoved = invokeFacePick(view, second, Qt::ControlModifier);
    check(activeFaceRemoved && view->selectedSurfaceRef() == std::optional<SurfaceRef>{first} &&
              displaysOwner(*window, first) && currentOutlinerSurface(*window) == std::optional<SurfaceRef>{first},
          "Surface Face removal promotes one surviving owner consistently across "
          "viewport, Item Edit, and Outliner");

    editor::Selection marquee{};
    marquee.surfaces.insert(second);
    marquee.surfaces.insert(first);
    const bool marqueePublished =
        view &&
        QMetaObject::invokeMethod(view, "selectionBoxPicked", Qt::DirectConnection, Q_ARG(editor::Selection, marquee),
                                  Q_ARG(GlView::SelectionBoxMode, GlView::SelectionBoxMode::Replace));
    check(marqueePublished && view->selectedSurfaceRef() == std::optional<SurfaceRef>{first} &&
              displaysOwner(*window, first) && currentOutlinerSurface(*window) == std::optional<SurfaceRef>{first},
          "Surface marquee uses its documented deterministic primary for Item "
          "Edit and Outliner focus");

    window->activateSurfaceVertexSubModeForTest();
    const Level& vertexLevel = window->documentStateForTest().level;
    const bool firstVertexPicked =
        invokeSubElementPick(*window, first, surfacePoint(vertexLevel, first, 0), Qt::NoModifier);
    const bool secondVertexPicked =
        invokeSubElementPick(*window, second, surfacePoint(vertexLevel, second, 1), Qt::ShiftModifier);
    check(firstVertexPicked && secondVertexPicked &&
              view->activeSurfaceVertexForTest() ==
                  std::optional<editor::SurfaceVertexRef>{editor::SurfaceVertexRef{second, 1}} &&
              view->selectedSurfaceRef() == std::optional<SurfaceRef>{second} && displaysOwner(*window, second) &&
              currentOutlinerSurface(*window) == std::optional<SurfaceRef>{second},
          "Surface Vertex Shift+LMB changes the authoritative owner face shown "
          "by Item Edit and Outliner");

    const bool activeVertexRemoved =
        invokeSubElementPick(*window, second, surfacePoint(vertexLevel, second, 1), Qt::ControlModifier);
    check(activeVertexRemoved && view->selectedSurfaceRef() == std::optional<SurfaceRef>{first} &&
              displaysOwner(*window, first),
          "Surface Vertex removal refreshes Item Edit from the promoted "
          "surviving vertex owner");

    window->activateSurfaceFaceSubModeForTest();
    window->activateSurfaceEdgeSubModeForTest();
    const bool firstEdgePicked = invokeEdgePick(*window, first, 0, Qt::NoModifier);
    const bool secondEdgePicked = invokeEdgePick(*window, second, 0, Qt::ShiftModifier);
    check(firstEdgePicked && secondEdgePicked && view->selectedSurfaceEdgeCountForTest() == 2u &&
              view->activeSurfaceEdgeIsSelectedForTest() &&
              view->activeSurfaceEdgeOwnerForTest() == std::optional<SurfaceRef>{second} &&
              view->selectedSurfaceRef() == std::optional<SurfaceRef>{second} && displaysOwner(*window, second) &&
              currentOutlinerSurface(*window) == std::optional<SurfaceRef>{second},
          "Surface Edge Shift+LMB changes the authoritative owner face shown by "
          "Item Edit and Outliner");

    const bool activeEdgeRemoved = invokeEdgePick(*window, second, 0, Qt::ControlModifier);
    check(activeEdgeRemoved && view->selectedSurfaceEdgeCountForTest() == 1u &&
              view->activeSurfaceEdgeIsSelectedForTest() &&
              view->activeSurfaceEdgeOwnerForTest() == std::optional<SurfaceRef>{first} &&
              view->selectedSurfaceRef() == std::optional<SurfaceRef>{first} && displaysOwner(*window, first),
          "Surface Edge removal refreshes Item Edit from the promoted surviving "
          "edge owner");

    // Model the state installed by Undo/Redo and native editor-state recovery:
    // the semantic selection is multi-valued while GlView restores its exact
    // primary sidecar separately. Item Edit must consume that primary rather
    // than whichever hash bucket happens to be first.
    window->activateSurfaceFaceSubModeForTest();
    editor::Selection& restoredSelection = window->mutableDocumentStateForTest().selection;
    restoredSelection.clear();
    restoredSelection.surfaces.insert(first);
    restoredSelection.surfaces.insert(second);
    view->setSelectionState(1, 0, -1, -1, restoredSelection);
    window->rebuildPropertiesForTest();
    check(displaysOwner(*window, second), "restored multi-Surface editor state binds Item Edit to its restored "
                                          "active face");

    window->pushUndoForTitleForTest(editor::DocumentDirty::LevelGeometry);
    window->mutableDocumentStateForTest().level.sectors[1].surfaces[0].geo = 2;
    QAction* undo = window->findChild<QAction*>(QStringLiteral("edit.undo"));
    QAction* redo = window->findChild<QAction*>(QStringLiteral("edit.redo"));
    if ( undo )
    {
        undo->trigger();
    }
    const bool undoKeptActive =
        displaysOwner(*window, second) && view->selectedSurfaceRef() == std::optional<SurfaceRef>{second};
    if ( redo )
    {
        redo->trigger();
    }
    check(undo && redo && undoKeptActive && displaysOwner(*window, second) &&
              view->selectedSurfaceRef() == std::optional<SurfaceRef>{second},
          "Surface Undo/Redo refresh preserves the explicit active face in Item "
          "Edit");
}

} // namespace sed::app_tests
