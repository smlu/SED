#pragma once

#include <cstddef>
#include <filesystem>
#include <istream>
#include <ostream>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "sed/level.hpp"
#include "sed/values.hpp"

namespace sed::assets
{

class ResourceLocator;

using ThingTemplate = sed::ThingTemplate;

class Templates
{
public:
    void clear();
    std::size_t size() const noexcept;

    const ThingTemplate* find(const std::string_view name) const noexcept;
    const ThingTemplate* get(std::size_t index) const noexcept;

    std::size_t addOrUpdate(ThingTemplate tpl, bool update = true);

    const TplValue* findField(const std::string_view tplName, const std::string_view fieldName) const noexcept;

private:
    struct NameHash
    {
        using is_transparent = void;
        std::size_t operator()(std::string_view name) const noexcept;
    };
    struct NameEqual
    {
        using is_transparent = void;
        bool operator()(std::string_view left, std::string_view right) const noexcept;
    };
    std::vector<ThingTemplate> items_{};
    std::unordered_map<std::string, std::size_t, NameHash, NameEqual> indices_{};
};

Templates loadTemplates(std::istream& in);
Templates loadTemplates(const std::filesystem::path& path);
Templates loadTemplates(const ResourceLocator& resources, const std::string_view name);
void saveTemplates(std::ostream& out, const Templates& templates);
void saveTemplates(const std::filesystem::path& path, const Templates& templates);

} // namespace sed::assets
