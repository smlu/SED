#include "test_common.hpp"

#include "main_window/detail/undo_labels.hpp"

namespace sed::app_tests
{

void runUndoLabelTests()
{
    editor::Selection selection{};
    selection.things.insert(4);
    check(ui::undo::selectionAction(selection, "Moved") == "Thing: Moved",
          "Undo labels: singular Thing uses entity prefix");
    check(ui::undo::relabelForSelection("Thing: Position changed", selection) == "Thing: Position changed",
          "Undo labels: property label remains singular for one Thing");

    selection.things.insert(7);
    check(ui::undo::selectionAction(selection, "Moved") == "Things: Moved",
          "Undo labels: plural Things uses entity prefix");
    check(ui::undo::relabelForSelection("Thing: Position changed", selection) == "Things: Position changed",
          "Undo labels: property label becomes plural for multiple Things");
    check(ui::undo::thingParameterAction(selection, "health", "changed") == "Things: Param 'health' changed",
          "Undo labels: Thing parameter includes its name and pluralizes");
    check(ui::undo::relabelForSelection("Thing: Param 'health' changed", selection) == "Things: Param 'health' changed",
          "Undo labels: named Thing parameter relabels for multi-selection");

    selection.things.erase(7);
    check(ui::undo::relabelForSelection("Things: Position changed", selection) == "Thing: Position changed",
          "Undo labels: an already plural property label returns to singular");

    selection.clear();
    check(ui::undo::thingParameterAction(selection, "", "added") == "Thing: Param '<unnamed>' added",
          "Undo labels: empty parameter names use a bounded placeholder");

    const std::string longParameterName(512u, 'x');
    const std::string escapedLongName = ui::undo::escapedParameterName(longParameterName);
    check(escapedLongName ==
              "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx...",
          "Undo labels: long name matches fixed full truncated vector");

    const std::string unusualParameterName = std::string("health\nrate'\\") + std::string(1u, '\x01');
    check(ui::undo::escapedParameterName(unusualParameterName) == "health\\x0Arate\\'\\\\\\x01",
          "Undo labels: full control, quote and backslash escape vector");
    check(ui::undo::escapedParameterName(std::string(17u, '\x01')) ==
              "\\x01\\x01\\x01\\x01\\x01\\x01\\x01\\x01"
              "\\x01\\x01\\x01\\x01\\x01\\x01\\x01...",
          "Undo labels: truncation retains complete escaped tokens");

    selection.clear();
    selection.sectors.insert(3);
    check(ui::undo::selectionAction(selection, "Rotated") == "Sector: Rotated",
          "Undo labels: singular Sector uses entity prefix");

    selection.sectors.insert(5);
    check(ui::undo::selectionAction(selection, "Rotated") == "Sectors: Rotated",
          "Undo labels: plural Sectors uses entity prefix");

    selection.clear();
    selection.surfaces.insert(SurfaceRef{1, 2});
    check(ui::undo::selectionAction(selection, "Cleaved") == "Surface: Cleaved",
          "Undo labels: singular Surface uses entity prefix");

    selection.surfaces.insert(SurfaceRef{1, 3});
    check(ui::undo::selectionAction(selection, "Cleaved") == "Surfaces: Cleaved",
          "Undo labels: plural Surfaces uses entity prefix");

    selection.clear();
    selection.surfaceVertices.insert(editor::SurfaceVertexRef{SurfaceRef{1, 2}, 0});
    check(ui::undo::action("Vertex", selection.surfaceVertices.size(), "Moved") == "Vertex: Moved",
          "Undo labels: vertex operation identifies the edited sub-object");

    selection.clear();
    selection.lights.insert(2);
    check(ui::undo::selectionAction(selection, "Scaled") == "Baked Light: Scaled",
          "Undo labels: persisted light selection uses Baked Light prefix");
    selection.lights.insert(5);
    check(ui::undo::selectionAction(selection, "Scaled") == "Baked Lights: Scaled",
          "Undo labels: persisted light selection uses a real plural Baked Lights prefix");

    check(ui::undo::propertyAction("Sector", "Sectors", 1u, "Point light color changed") ==
              "Sector: Point light color changed",
          "Undo labels: sector point-light property uses the owning Sector");
    check(ui::undo::propertyAction("Sector", "Sectors", 2u, "Point light range changed") ==
              "Sectors: Point light range changed",
          "Undo labels: multi-sector point-light property pluralizes owner");
    selection.clear();
    selection.sectors.insert(3);
    check(ui::undo::relabelForSelection("Sector: Point light range changed", selection) ==
              "Sector: Point light range changed",
          "Undo labels: sector point-light property retains a singular owner");
    selection.sectors.insert(5);
    check(ui::undo::relabelForSelection("Sector: Point light range changed", selection) ==
              "Sectors: Point light range changed",
          "Undo labels: sector point-light property pluralizes a live selection");
    selection.sectors.clear();
    check(ui::undo::parameterAction("Thing", 1u, "light", "changed") == "Thing: Param 'light' changed",
          "Undo labels: Thing light parameter uses the Thing owner");
    check(ui::undo::parameterAction("Thing", 2u, "lightintensity", "changed") ==
              "Things: Param 'lightintensity' changed",
          "Undo labels: multi-Thing headlight parameter pluralizes owner");
    check(ui::undo::parameterAction("Thing", 1u, "lightoffset", "changed") == "Thing: Param 'lightoffset' changed",
          "Undo labels: Headlight gizmo offset identifies the exact parameter");
    check(ui::undo::parameterAction("Thing", 2u, "debris", "removed") == "Things: Param 'debris' removed",
          "Undo labels: repeatable debris edits use the Thing parameter vocabulary");
    check(ui::undo::parameterPairAction("Thing", 1u, "lightintensity", "lightoffset", "changed") ==
              "Thing: Params 'lightintensity' and 'lightoffset' changed",
          "Undo labels: combined Headlight popup names both authored parameters");
    check(ui::undo::parameterPairAction("Thing", 2u, "lightintensity", "lightoffset", "changed") ==
              "Things: Params 'lightintensity' and 'lightoffset' changed",
          "Undo labels: combined Headlight popup pluralizes Things");
    check(ui::undo::propertyAction("Baked Light", "Baked Lights", 1u, "Range changed") == "Baked Light: Range changed",
          "Undo labels: baked-light property keeps its persisted entity name");
    check(ui::undo::action("Surface", "Surfaces", 1u, "UVs auto-aligned") == "Surface: UVs auto-aligned" &&
              ui::undo::action("Surface", "Surfaces", 2u, "UVs auto-aligned") == "Surfaces: UVs auto-aligned",
          "Undo labels: UV edits use the owning Surface singular/plural form");

    selection.clear();
    selection.cogs.insert(6);
    check(ui::undo::selectionAction(selection, "Value changed") == "COG: Value changed",
          "Undo labels: COG value edits identify COG");

    selection.clear();
    selection.sectors.insert(1);
    selection.lights.insert(2);
    check(ui::undo::selectionAction(selection, "Deleted") == "Selection: Deleted",
          "Undo labels: mixed entity operation is explicitly Selection");

    check(ui::undo::action("COG Script", 2u, "Changed") == "COG Scripts: Changed",
          "Undo labels: COG Script plural is readable");
}

} // namespace sed::app_tests
