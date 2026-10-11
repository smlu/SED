#include "sed/assets/tpl.hpp"
#include "atomic_file_write.hpp"

#include "sed/assets/resource_locator.hpp"
#include "sed/text.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <limits>
#include <fstream>
#include <iomanip>
#include <locale>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace sed::assets
{

namespace
{

std::string normalizeName(const std::string_view name)
{
    return sed::text::upperAscii(sed::text::trimView(name));
}

} // namespace

std::size_t Templates::NameHash::operator()(const std::string_view name) const noexcept
{
    std::size_t hash = 2166136261u;
    for (const unsigned char ch : sed::text::trimView(name))
    {
        const auto folded = static_cast<unsigned char>(std::toupper(ch));
        hash = (hash ^ folded) * 16777619u;
    }
    return hash;
}

bool Templates::NameEqual::operator()(
    const std::string_view left, const std::string_view right) const noexcept
{
    return sed::text::iequalsAscii(sed::text::trimView(left), sed::text::trimView(right));
}

void Templates::clear()
{
    items_.clear();
    indices_.clear();
}

std::size_t Templates::size() const noexcept
{
    return items_.size();
}

const ThingTemplate* Templates::find(const std::string_view name) const noexcept
{
    const std::string_view key = sed::text::trimView(name);
    if (key.empty())
    {
        return nullptr;
    }

    const auto it = indices_.find(key);
    if (it == indices_.end())
    {
        return nullptr;
    }

    const std::size_t idx = it->second;
    if (idx >= items_.size())
    {
        return nullptr;
    }

    return &items_[idx];
}

const ThingTemplate* Templates::get(const std::size_t index) const noexcept
{
    return (index < items_.size()) ? &items_[index] : nullptr;
}

std::size_t Templates::addOrUpdate(ThingTemplate tpl, const bool update)
{
    const std::string key = normalizeName(tpl.name);
    if (key.empty())
    {
        throw std::runtime_error("Template name is empty.");
    }

    const auto it = indices_.find(key);
    if (it != indices_.end())
    {
        const std::size_t idx = it->second;
        if (!update)
        {
            return idx;
        }
        if (idx >= items_.size())
        {
            throw std::runtime_error("Template index map corrupted.");
        }

        items_[idx] = std::move(tpl);
        return idx;
    }

    const std::size_t idx = items_.size();
    items_.push_back(std::move(tpl));
    try
    {
        indices_.emplace(key, idx);
    }
    catch (...)
    {
        items_.pop_back();
        throw;
    }
    return idx;
}

const TplValue* Templates::findField(const std::string_view tplName, const std::string_view fieldName) const noexcept
{
    std::string_view current = tplName;
    for (int depth = 0; depth < 1000; ++depth)
    {
        const ThingTemplate* tpl = find(current);
        if (!tpl)
        {
            return nullptr;
        }

        // The game parsers consume arguments from left to right, so the last
        // scalar occurrence authored in a template is the effective one.
        // Callers that need repeatable parameters must enumerate `values`.
        for (auto it = tpl->values.rbegin(); it != tpl->values.rend(); ++it)
        {
            if (sed::text::iequalsAscii(it->name, fieldName))
            {
                return &*it;
            }
        }

        if (tpl->parent.empty() || sed::text::iequalsAscii(tpl->parent, "none"))
        {
            return nullptr;
        }

        current = tpl->parent;
    }

    return nullptr;
}

Templates loadTemplates(std::istream& in)
{
    Templates out;

    std::string currentDesc;
    ThingBox currentBox{};

    std::string line;
    while (std::getline(in, line))
    {
        std::string trimmed = sed::text::trim(line);
        if (trimmed.empty())
        {
            continue;
        }

        if (!trimmed.empty() && trimmed.front() == '#')
        {
            const auto tokens = sed::text::splitWords(trimmed);
            if (tokens.size() >= 2 && sed::text::iequalsAscii(tokens[1], "DESC:"))
            {
                std::string desc;
                for (std::size_t i = 2; i < tokens.size(); ++i)
                {
                    if (!desc.empty())
                    {
                        desc.push_back(' ');
                    }
                    desc.append(tokens[i]);
                }
                currentDesc = std::move(desc);
            }
            else if (tokens.size() >= 8 && sed::text::iequalsAscii(tokens[1], "BBOX:"))
            {
                currentBox.Set(
                    sed::text::parseDouble(tokens[2]).value_or(0.0),
                    sed::text::parseDouble(tokens[3]).value_or(0.0),
                    sed::text::parseDouble(tokens[4]).value_or(0.0),
                    sed::text::parseDouble(tokens[5]).value_or(0.0),
                    sed::text::parseDouble(tokens[6]).value_or(0.0),
                    sed::text::parseDouble(tokens[7]).value_or(0.0));
            }
            continue;
        }

        const auto tokens = sed::text::splitWords(trimmed);
        if (tokens.size() < 2)
        {
            continue;
        }

        const std::string_view name = tokens[0];
        const std::string_view parent = tokens[1];

        if (out.find(name) != nullptr)
        {
            continue;
        }

        ThingTemplate tpl{};
        tpl.name = std::string(name);
        tpl.parent = std::string(parent);
        tpl.desc = currentDesc;
        tpl.bbox = currentBox;

        for (std::size_t i = 2; i < tokens.size(); ++i)
        {
            const std::string_view kv = tokens[i];
            const std::size_t eq = kv.find('=');
            if (eq == std::string_view::npos)
            {
                continue;
            }

            const std::string_view k = kv.substr(0, eq);
            const std::string_view v = kv.substr(eq + 1);

            tpl.values.push_back(sed::deserializeTplValue(k, v));
        }

        (void)out.addOrUpdate(std::move(tpl), true);

        currentDesc.clear();
        currentBox = ThingBox{};
    }

    if (in.bad() || (in.fail() && !in.eof()))
    {
        throw std::runtime_error("Failed to read templates.");
    }
    return out;
}

Templates loadTemplates(const std::filesystem::path& path)
{
    std::ifstream in(path);
    if (!in)
    {
        throw std::runtime_error("Failed to open templates: " + path.string());
    }

    return loadTemplates(in);
}

Templates loadTemplates(const ResourceLocator& resources, const std::string_view name)
{
    const std::string text = resources.readText(name);
    std::istringstream iss(text);
    return loadTemplates(iss);
}

void saveTemplates(std::ostream& out, const Templates& templates)
{
    // Validate all authored bounds before writing any part of the database.
    for (std::size_t i = 0; i < templates.size(); ++i)
    {
        const ThingTemplate* tpl = templates.get(i);
        if (tpl && (!std::isfinite(tpl->bbox.x1) || !std::isfinite(tpl->bbox.y1)
            || !std::isfinite(tpl->bbox.z1) || !std::isfinite(tpl->bbox.x2)
            || !std::isfinite(tpl->bbox.y2) || !std::isfinite(tpl->bbox.z2)))
        {
            throw std::runtime_error("Cannot save templates with non-finite bounds.");
        }
    }
    out.imbue(std::locale::classic());
    out << std::defaultfloat << std::setprecision(std::numeric_limits<double>::max_digits10);

    for (std::size_t i = 0; i < templates.size(); ++i)
    {
        const ThingTemplate* tpl = templates.get(i);
        if (!tpl)
        {
            continue;
        }

        out << "# DESC: " << tpl->desc << '\n';
        out << "# BBOX: "
            << tpl->bbox.x1 << ' '
            << tpl->bbox.y1 << ' '
            << tpl->bbox.z1 << ' '
            << tpl->bbox.x2 << ' '
            << tpl->bbox.y2 << ' '
            << tpl->bbox.z2 << '\n';

        out << tpl->name << ' ' << tpl->parent;
        for (const TplValue& value : tpl->values)
        {
            out << ' ' << value.name << '=' << sed::serializeTplValue(value);
        }
        out << '\n';
    }
    if (!out) throw std::runtime_error("Failed to write templates.");
}

void saveTemplates(const std::filesystem::path& path, const Templates& templates)
{
    std::ostringstream out;
    saveTemplates(out, templates);
    const std::string bytes = out.str();
    detail::StagedAssetFile staged(path);
    staged.writeAndClose(std::as_bytes(std::span(bytes.data(), bytes.size())));
    staged.publish(path);
}

} // namespace sed::assets
