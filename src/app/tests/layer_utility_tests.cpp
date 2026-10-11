#include "test_common.hpp"

#include "ui/layers/layer_color_utils.hpp"

#include <vector>

namespace sed::app_tests
{

void runLayerUtilityTests()
{
    check(ui::layers::defaultLayerColor(0) == QColor(255, 255, 255),
        "layer zero defaults to opaque white");

    const QColor geometryColor(15, 25, 35, 70);
    const QColor unusedColor(45, 55, 65, 80);
    const QColor gameplayColor(75, 85, 95, 90);
    const QColor lightingColor(105, 115, 125, 100);

    const ui::layers::LayerVisualState remapped = ui::layers::remapLayerVisualState(
        { 2, -1, 1, 0 },
        4,
        { false, true, true, false },
        { true, false, false, true },
        { geometryColor, unusedColor, gameplayColor, lightingColor });

    check(remapped.visible == std::vector<bool>({ false, true, false, true }),
        "layer visual remap carries visibility through reorder and compaction");
    check(remapped.locked == std::vector<bool>({ true, false, true, false }),
        "layer visual remap carries locks through reorder and compaction");
    check(remapped.colors.size() == 4
            && remapped.colors[0] == QColor(105, 115, 125)
            && remapped.colors[1] == QColor(75, 85, 95)
            && remapped.colors[2] == QColor(15, 25, 35)
            && remapped.colors[3] == ui::layers::defaultLayerColor(3),
        "layer visual remap carries colors and defaults a new layer");

    const ui::layers::LayerVisualState unusedReordered = ui::layers::remapLayerVisualState(
        { 1, 0 },
        2,
        { true, false },
        { false, true },
        { QColor(10, 20, 30), QColor(40, 50, 60) });
    check(!unusedReordered.visible[0]
            && unusedReordered.locked[0]
            && unusedReordered.colors[0] == QColor(40, 50, 60),
        "layer visual remap preserves an unused layer's state when reordered");
}

} // namespace sed::app_tests
