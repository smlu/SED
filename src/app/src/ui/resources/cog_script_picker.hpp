#pragma once

#include <filesystem>
#include <optional>
#include <string>

class QWidget;

namespace sed::assets
{
class ResourceLocator;
}

namespace sed::ui
{

struct CogScriptPickerResult
{
    std::string scriptName{};
    bool createdNew{};
};

// Shows a picker for COG scripts from currently mounted resources (disk roots + gobs).
// - Lists all discovered `.cog` scripts.
// - Can optionally extract an archive-only script to disk so it can be edited/watched.
std::optional<CogScriptPickerResult> pickCogScript(
    QWidget* parent,
    const sed::assets::ResourceLocator& resources,
    const std::optional<std::filesystem::path>& extractionRoot);

} // namespace sed::ui
