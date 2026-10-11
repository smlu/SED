#include "test_common.hpp"
#include "ui/templates/template_editor_precision_utils.hpp"

namespace sed::app_tests
{
void runTemplatePrecisionEqualityTests()
{
    ThingTemplate original;
    original.name = "precise_template";
    original.parent = "none";
    original.bbox.Set(0, 0, 0, 1, 1, 1);
    auto changed = original;
    changed.bbox.x2 = std::nextafter(1.0, 2.0);
    check(!ui::templateEditorAuthoredValuesEqual(original, changed),
        "Template Editor local synchronization detects sub-epsilon authored bounds changes");
    TplValue value;
    value.name = "mass";
    value.type = ValueType::floating;
    value.floatValue = 1.0;
    original.values.push_back(value);
    changed = original;
    changed.values[0].floatValue = std::nextafter(1.0, 2.0);
    check(!ui::templateEditorAuthoredValuesEqual(original, changed),
        "Template Editor local synchronization detects sub-epsilon authored parameter changes");
    check(ui::templateEditorAuthoredValuesEqual(original, original),
        "Template Editor local synchronization still skips exactly unchanged templates");
}
}
