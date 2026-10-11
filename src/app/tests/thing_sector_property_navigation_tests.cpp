#include "test_common.hpp"

#include "gl_view.hpp"
#include "main_window/main_window.hpp"
#include "sed/level/build.hpp"
#include "sed/assets/sed.hpp"

#include <QTreeWidget>

#include <QString>
#include <cstddef>
#include <utility>
#include <vector>

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
    const QList<QTreeWidgetItem*> matches = properties->findItems(
        name, Qt::MatchContains | Qt::MatchRecursive, 0);
    for ( QTreeWidgetItem* item : matches )
    {
        if ( item && item->text(0).trimmed() == name )
        {
            return item;
        }
    }
    return nullptr;
}

} // namespace

void runThingSectorPropertyNavigationTests()
{
    MainWindow window{};
    window.setAttribute(Qt::WA_DontShowOnScreen);

    editor::DocumentState state{};
    state.level.kind = ProjectType::IJIM;
    state.level.layers.emplace_back("Layer0");
    level::levelCreateCube(state.level, 0.0, 0.0, 0.0, TVector::up, TVector::right);
    level::levelCreateCube(state.level, 8.0, 0.0, 0.0, TVector::up, TVector::right);

    Thing thing{};
    thing.sector = 1;
    thing.x = 8.5;
    thing.y = 0.5;
    thing.z = 0.5;
    state.level.things.push_back(std::move(thing));
    state.selection.things.insert(0);
    window.activateThingEditModeForTest();
    window.setDocumentStateForTest(std::move(state));

    const std::vector<std::byte> levelBefore = assets::saveToSedBytes(
        window.documentStateForTest().level);
    const std::size_t undoBefore = window.undoDepthForTest();
    QTreeWidget* properties = window.findChild<QTreeWidget*>(
        QStringLiteral("propertiesTree"));
    QTreeWidgetItem* sectorRow = propertyRow(properties, QStringLiteral("Sector"));
    check(sectorRow != nullptr && sectorRow->text(1) == QStringLiteral("1"),
        "Thing Item Edit exposes its real owning Sector value as a property row");
    if ( !sectorRow || !properties )
    {
        return;
    }

    Q_EMIT properties->itemDoubleClicked(sectorRow, 1);

    const editor::DocumentState& after = window.documentStateForTest();
    const GlView* view = window.glViewForTest();
    check(window.editModeForTest() == window.sectorEditModeValueForTest()
            && after.selection.sectors.size() == 1
            && after.selection.sectors.contains(1)
            && after.selection.things.empty()
            && view != nullptr
            && view->activeSectorIndex() == 1
            && view->activeThingIndex() == -1,
        "double-clicking a Thing Item Edit Sector row selects and shows its owning Sector");
    check(assets::saveToSedBytes(after.level) == levelBefore
            && window.undoDepthForTest() == undoBefore,
        "Thing Sector property navigation preserves the Level and history");

    const auto invalidSectorCase = [](const std::int32_t sectorIndex)
    {
        MainWindow invalidWindow{};
        invalidWindow.setAttribute(Qt::WA_DontShowOnScreen);
        editor::DocumentState invalidState{};
        invalidState.level.kind = ProjectType::IJIM;
        invalidState.level.layers.emplace_back("Layer0");
        level::levelCreateCube(
            invalidState.level, 0.0, 0.0, 0.0, TVector::up, TVector::right);
        Thing invalidThing{};
        invalidThing.sector = sectorIndex;
        invalidState.level.things.push_back(std::move(invalidThing));
        invalidState.selection.things.insert(0);
        invalidWindow.activateThingEditModeForTest();
        invalidWindow.setDocumentStateForTest(std::move(invalidState));

        const std::vector<std::byte> before = assets::saveToSedBytes(
            invalidWindow.documentStateForTest().level);
        QTreeWidget* invalidProperties = invalidWindow.findChild<QTreeWidget*>(
            QStringLiteral("propertiesTree"));
        QTreeWidgetItem* invalidRow = propertyRow(
            invalidProperties, QStringLiteral("Sector"));
        if ( !invalidRow || !invalidProperties )
        {
            return false;
        }
        Q_EMIT invalidProperties->itemDoubleClicked(invalidRow, 1);
        const editor::DocumentState& unchanged =
            invalidWindow.documentStateForTest();
        return assets::saveToSedBytes(unchanged.level) == before
            && unchanged.selection.things.contains(0)
            && unchanged.selection.sectors.empty()
            && invalidWindow.undoDepthForTest() == 0u;
    };
    check(invalidSectorCase(-1),
        "double-clicking a Thing with no owning Sector is harmless");
    check(invalidSectorCase(99),
        "double-clicking a Thing with an invalid owning Sector is harmless");
}

} // namespace sed::app_tests
