#include "thing/thing_resource_utils.hpp"
#include "ui/components/preview_resource_revision_utils.hpp"

#include <algorithm>
#include <sstream>
#include <string>
#include <unordered_map>
#include <utility>

#include "sed/assets/3do.hpp"
#include "sed/assets/spr.hpp"
#include "sed/level/thing.hpp"
#include "sed/text.hpp"

namespace sed::thing
{

std::string tplValueString(const TplValue* value)
{
    if (!value)
    {
        return {};
    }
    return value->stringValue.empty() ? value->asString() : value->stringValue;
}

std::string normalizedModel3doName(std::string_view modelName)
{
    std::string normalized = sed::text::trim(std::string(modelName));
    if (normalized.empty() || sed::text::iequalsAscii(normalized, "none"))
    {
        return {};
    }
    if (!sed::text::iendsWithAscii(normalized, ".3do"))
    {
        normalized += ".3do";
    }
    return normalized;
}

std::string normalizedSpriteName(std::string_view spriteName)
{
    std::string normalized = sed::text::trim(std::string(spriteName));
    if (normalized.empty() || sed::text::iequalsAscii(normalized, "none"))
    {
        return {};
    }
    if (!sed::text::iendsWithAscii(normalized, ".spr"))
    {
        normalized += ".spr";
    }
    return normalized;
}

std::string thingTemplateKey(const Thing& thing)
{
    return !thing.templateName.empty() ? thing.templateName : thing.name;
}

std::optional<ThingBox> templateThingBox(
    const Level& level,
    const assets::Templates* templates,
    const std::string_view templateName) noexcept
{
    for (const ThingTemplate& tpl : level.templates)
    {
        if (sed::text::iequalsAscii(tpl.name, templateName))
        {
            return tpl.bbox;
        }
    }
    if (templates)
    {
        if (const ThingTemplate* tpl = templates->find(templateName))
        {
            return tpl->bbox;
        }
    }
    return std::nullopt;
}

bool applyThingTemplateSelection(
    Thing& thing,
    std::string templateName,
    const std::optional<ThingBox>& templateBox,
    const bool overrideThingName)
{
    bool changed = false;
    if (thing.templateName != templateName)
    {
        thing.templateName = templateName;
        if (templateBox)
        {
            thing.bbox = *templateBox;
        }
        changed = true;
    }
    if (overrideThingName && thing.name != templateName)
    {
        thing.name = std::move(templateName);
        changed = true;
    }
    return changed;
}

std::string templateModel3doName(
    const Level& level,
    const assets::Templates* templates,
    const std::string_view thingName)
{
    if (thingName.empty())
    {
        return {};
    }

    Thing temp{};
    temp.name = std::string(thingName);
    temp.templateName = std::string(thingName);

    const TplValue* modelValue = sed::level::thingFindValue(level, templates, temp, "model3d");
    if (!modelValue)
    {
        modelValue = sed::level::thingFindValue(level, templates, temp, "model3do");
    }
    return normalizedModel3doName(tplValueString(modelValue));
}

std::string thingModel3doName(
    const Level& level,
    const assets::Templates* templates,
    const Thing& thing)
{
    const TplValue* modelValue = sed::level::thingFindValue(level, templates, thing, "model3d");
    if (!modelValue)
    {
        modelValue = sed::level::thingFindValue(level, templates, thing, "model3do");
    }
    return normalizedModel3doName(tplValueString(modelValue));
}

bool templateHasModel3dParam(
    const Level& level,
    const assets::Templates* templates,
    const std::string_view thingName)
{
    return !templateModel3doName(level, templates, thingName).empty();
}

bool thingHasModel3dParam(
    const Level& level,
    const assets::Templates* templates,
    const Thing& thing)
{
    return !thingModel3doName(level, templates, thing).empty();
}

std::string templateSpriteName(
    const Level& level,
    const assets::Templates* templates,
    const std::string_view thingName)
{
    if (thingName.empty())
    {
        return {};
    }

    Thing temp{};
    temp.name = std::string(thingName);
    temp.templateName = std::string(thingName);
    return normalizedSpriteName(tplValueString(
        sed::level::thingFindValue(level, templates, temp, "sprite")));
}

bool isRailOrTrackThingKey(const std::string_view modelName, const std::string_view thingName)
{
    const std::string modelKey = sed::text::upperAscii(std::string(modelName));
    const std::string thingKey = sed::text::upperAscii(std::string(thingName));
    return modelKey.find("TRACK") != std::string::npos
        || modelKey.find("RAIL") != std::string::npos
        || thingKey.find("TRACK") != std::string::npos
        || thingKey.find("RAIL") != std::string::npos;
}

std::optional<ThingBox> loadModel3doBox(
    const assets::ResourceLocator& resources,
    const std::string_view modelName)
{
    const std::string normalized = normalizedModel3doName(modelName);
    if (normalized.empty())
    {
        return std::nullopt;
    }

    static std::unordered_map<std::string, std::optional<ThingBox>> cache{};
    const std::string cacheKey = "3do:" + sed::text::upperAscii(normalized)
        + "|" + ui::detail::previewResourceRevisionToken(&resources, normalized, ".3do");
    if (const auto it = cache.find(cacheKey); it != cache.end())
    {
        return it->second;
    }

    std::optional<ThingBox> result{};
    try
    {
        if (const std::optional<std::string> text = resources.tryReadTextWithExt(normalized, ".3do"))
        {
            std::istringstream iss(*text);
            const assets::Model3do model = assets::loadFrom3DO(iss);
            bool haveVertex = false;
            ThingBox box{};
            for (const assets::Model3doMesh& mesh : model.meshes)
            {
                for (const assets::Model3doVertex& vertex : mesh.vertices)
                {
                    if (!haveVertex)
                    {
                        box.Set(vertex.x, vertex.y, vertex.z, vertex.x, vertex.y, vertex.z);
                        haveVertex = true;
                    }
                    else
                    {
                        box.x1 = std::min(box.x1, static_cast<double>(vertex.x));
                        box.y1 = std::min(box.y1, static_cast<double>(vertex.y));
                        box.z1 = std::min(box.z1, static_cast<double>(vertex.z));
                        box.x2 = std::max(box.x2, static_cast<double>(vertex.x));
                        box.y2 = std::max(box.y2, static_cast<double>(vertex.y));
                        box.z2 = std::max(box.z2, static_cast<double>(vertex.z));
                    }
                }
            }
            if (haveVertex && !box.IsZero())
            {
                result = box;
            }
        }
    }
    catch (const std::exception&)
    {
    }

    if (cache.size() >= 256u) cache.erase(cache.begin());
    cache.emplace(cacheKey, result);
    return result;
}

std::optional<ThingBox> loadSpriteBox(
    const assets::ResourceLocator& resources,
    const std::string_view spriteName)
{
    const std::string normalized = normalizedSpriteName(spriteName);
    if (normalized.empty())
    {
        return std::nullopt;
    }

    static std::unordered_map<std::string, std::optional<ThingBox>> cache{};
    const std::string cacheKey = "spr:" + sed::text::upperAscii(normalized)
        + "|" + ui::detail::previewResourceRevisionToken(&resources, normalized, ".spr");
    if (const auto it = cache.find(cacheKey); it != cache.end())
    {
        return it->second;
    }

    std::optional<ThingBox> result{};
    try
    {
        if (const std::optional<std::string> text = resources.tryReadTextWithExt(normalized, ".spr"))
        {
            std::istringstream iss(*text);
            const assets::SprFile spr = assets::loadFromSPR(iss);
            const double halfW = std::max(0.1, static_cast<double>(spr.width) * 0.5);
            const double halfH = std::max(0.1, static_cast<double>(spr.height) * 0.5);
            const double offsetX = static_cast<double>(spr.offsetXYZ[0]);
            const double offsetY = static_cast<double>(spr.offsetXYZ[1]);
            const double offsetZ = static_cast<double>(spr.offsetXYZ[2]);
            const double halfDepth = std::max(0.1, std::max(halfW, halfH) * 0.08);

            ThingBox box{};
            if (spr.type == 2)
            {
                const double radialHalf = std::max(halfW, halfDepth);
                box.Set(
                    offsetX - radialHalf,
                    offsetY - radialHalf,
                    offsetZ - halfH,
                    offsetX + radialHalf,
                    offsetY + radialHalf,
                    offsetZ + halfH);
            }
            else
            {
                box.Set(
                    offsetX - halfW,
                    offsetY - halfDepth,
                    offsetZ - halfH,
                    offsetX + halfW,
                    offsetY + halfDepth,
                    offsetZ + halfH);
            }
            if (!box.IsZero())
            {
                result = box;
            }
        }
    }
    catch (const std::exception&)
    {
    }

    if (cache.size() >= 256u) cache.erase(cache.begin());
    cache.emplace(cacheKey, result);
    return result;
}

} // namespace sed::thing
