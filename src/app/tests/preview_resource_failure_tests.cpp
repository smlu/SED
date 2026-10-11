#include "test_common.hpp"
#include "sed/assets/resource_locator.hpp"
#include "ui/components/template_model_preview_icon.hpp"

#include <array>
#include <functional>
#include <stdexcept>
#include <string>

namespace sed::app_tests
{

void runPreviewResourceFailureTests()
{
    using Loader = std::function<bool(const assets::ResourceLocator*)>;
    const std::array<std::pair<std::string, Loader>, 4> cases{{
        {"unreadable.spr", [](const auto* resources)
            { return bool(ui::detail::loadSharedPreviewSprite(resources, "unreadable.spr")); }},
        {"unreadable.cmp", [](const auto* resources)
            { return bool(ui::detail::loadPreviewCmp(resources, "unreadable.cmp")); }},
        {"unreadable.mat", [](const auto* resources)
            { return bool(ui::detail::loadPreviewMaterialTextureSource(resources, "none.cmp", "unreadable.mat")); }},
        {"unreadable.3do", [](const auto* resources)
            { return bool(ui::detail::loadSharedPreviewModel(resources, "unreadable.3do")); }},
    }};
    for (const auto& [name, load] : cases)
    {
        assets::ResourceLocator resources;
        int reads = 0;
        resources.setPrimaryResourceOverride(name, [&]() -> std::optional<std::vector<std::byte>>
            {
                ++reads;
                throw std::runtime_error("Truncated selected archive entry");
            });
        bool threw = false;
        bool available = false;
        try
        {
            available = load(&resources);
            available = load(&resources) || available;
        }
        catch (const std::exception&)
        {
            threw = true;
        }
        check(!threw && !available && reads == 1,
            "Unreadable preview dependency reports unavailable without escaping the UI or worker");
    }
}

} // namespace sed::app_tests
