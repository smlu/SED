#include "test_common.hpp"
#include <streambuf>

#include "sed/assets/sed.hpp"
#include "sed/assets/jkl.hpp"
#include "sed/assets/resource_locator.hpp"
#include "sed/editor/document.hpp"
#include "sed/level/arch_lighting.hpp"
#include "sed/level/thing.hpp"
#include "sed/assets/tpl.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <utility>
#include <vector>

namespace sed::assets::detail
{

std::optional<std::size_t> recoveryFramingSizeForTesting(
    std::size_t sectionBytes, bool survivingMirror) noexcept;

bool sedFileSizeWithinLimitForTesting(std::uint64_t fileSize) noexcept;

std::vector<std::byte> compressSedLzssForTesting(
    std::span<const std::byte> input);

std::vector<std::byte> decompressSedLzssForTesting(
    std::span<const std::byte> input,
    std::size_t expandedSize);

} // namespace sed::assets::detail

namespace sed::tests
{
namespace
{

void check(const bool condition, const std::string& message)
{
    sed::tests::check(condition, message.c_str());
}

bool exactSedText(std::string_view actual, std::string_view expected)
{
    if (actual == expected) return true;
    std::cerr << "SED diagnostic mismatch\nexpected: " << expected
              << "\nactual: " << actual << '\n';
    return false;
}

Level richLevel(const ProjectType kind)
{
    Level level{};
    level.clear();
    // The checked-in native vectors deliberately remain SED V1 pixel-space
    // documents. Dedicated V2 coverage below exercises normalized storage.
    level.uvCoordinateSpace = UvCoordinateSpace::TexturePixels;
    level.kind = kind;
    level.header.version = kind == ProjectType::IJIM ? 3 : 1;
    level.header.gravity = 3.125;
    level.header.horizonSky = HorizonSky{ 321.25, 777.5, { -2.25, 4.5 } };
    level.header.ceilingSky = CeilingSky{ 19.75, { 8.5, -9.25 } };
    level.header.mipmapDistances = { 1.25, 2.5, 5.0, 10.0 };
    level.header.lodDistances = { 0.15, 0.35, 0.75, 1.5 };
    level.header.perspectiveDistance = 12.75;
    level.header.gouraudDistance = 6.375;
    level.header.fog.enabled = true;
    level.header.fog.color = ColorF{ 0.11f, 0.22f, 0.33f, 0.44f };
    level.header.fog.fogStart = 7.25;
    level.header.fog.fogEnd = 987.5;
    level.exportBufferExtras = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12 };
    level.masterCMP = "master.cmp";
    level.ppunit = 128.125;
    level.lvisstring = "101:visible;010:locked";
    level.curId = 9142;
    level.colormaps = { "master.cmp", "alternate.cmp" };
    level.materials = { "stone.mat", "alpha.mat", "stone.mat" };
    level.layers = { "Main", "Gameplay", "Hidden detail" };

    Sector sector{};
    sector.id = 1001;
    sector.num = 17;
    sector.flags = static_cast<std::int32_t>(0x81234567u);
    sector.ambient = ColorF{ 0.19f, 0.31f, 0.47f, 0.63f };
    sector.extraLight = ColorF{ -0.2f, 0.13f, 0.29f, 0.77f };
    sector.pointLight.color = ColorF{ 0.81f, 0.53f, 0.27f, 0.91f };
    sector.pointLight.position = { 1.125, -2.25, 3.5 };
    sector.pointLight.minRange = 4.75;
    sector.pointLight.maxRange = 23.875;
    sector.colormap = "alternate.cmp";
    sector.tint = ColorF{ 0.2f, 0.4f, 0.6f, 0.8f };
    sector.sound = "wind.wav";
    sector.soundVolume = 0.375;
    sector.thrust = { -0.125, 9.25, 1.5 };
    sector.collideBox = { -1.0, -2.0, -3.0, 11.0, 12.0, 13.0 };
    sector.layer = 2;
    sector.mark = 73;
    sector.vertices = {
        Vertex{ 0.125, 0.25, 0.5, 7 },
        Vertex{ 10.125, 0.25, 0.5, 8 },
        Vertex{ 0.125, 10.25, 0.5, 9 },
    };
    Surface surface{};
    surface.num = 44;
    surface.mark = 45;
    surface.d3dId = 46;
    surface.surfaceFlags = 0x123456;
    surface.faceFlags = 0x87654321u;
    surface.geo = -1;
    surface.light = 4;
    surface.tex = 3;
    surface.extraLight = ColorF{ -0.9f, 0.2f, 0.7f, 0.35f };
    surface.adjoinFlags = static_cast<std::int32_t>(0x80000005u);
    surface.adjoin = SurfaceRef{ 22, 33 };
    surface.material = "alpha.mat";
    surface.nAdj = 23;
    surface.nMat = 24;
    surface.uScale = 1.75f;
    surface.vScale = -0.625f;
    surface.normal = { 0.1, -0.2, 0.3 };
    surface.vertices = { 2, 0, 1 };
    surface.texVertices = {
        TexVertex{ 0.125f, 0.25f, ColorF{ 0.1f, 0.2f, 0.3f, 0.4f } },
        TexVertex{ 1.125f, 0.25f, ColorF{ 0.5f, 0.6f, 0.7f, 0.8f } },
        TexVertex{ 0.125f, 1.25f, ColorF{ 0.9f, 0.8f, 0.7f, 0.6f } },
    };
    sector.surfaces.push_back(surface);
    level.sectors.push_back(sector);

    TplValue tplValue{};
    tplValue.name = "lightintensity";
    tplValue.type = ValueType::vect;
    tplValue.intValue = -15;
    tplValue.floatValue = 3.141592653589793;
    tplValue.stringValue = "(0.125/0.25/0.5/17.75)";
    tplValue.adjType = AdjType::frame;

    Thing thing{};
    thing.id = 2001;
    thing.num = 91;
    thing.sector = 17;
    thing.templateName = "+derived_actor";
    thing.name = "instance_name_not_template";
    thing.x = 1.25; thing.y = -2.5; thing.z = 3.75;
    thing.pch = 11.125; thing.yaw = -22.25; thing.rol = 33.5;
    thing.values = { tplValue };
    thing.layer = 1;
    thing.bbox.Set(-0.4, -0.5, -0.6, 0.7, 0.8, 0.9);
    level.things.push_back(thing);

    ThingTemplate tpl{};
    tpl.name = "+derived_actor";
    tpl.parent = "_actor";
    tpl.values = { tplValue };
    tpl.desc = "A complete editor-only template description.";
    tpl.bbox.Set(-1.4, -1.5, -1.6, 1.7, 1.8, 1.9);
    level.templates.push_back(tpl);

    CogValue cogValue{};
    cogValue.name = "target";
    cogValue.type = ValueType::ptr;
    cogValue.intValue = 812;
    cogValue.floatValue = -0.03125;
    cogValue.stringValue = "17 44";
    cogValue.cogType = CogType::surface;
    cogValue.local = true;
    cogValue.description = "Linked authored surface";
    cogValue.mask = static_cast<std::int32_t>(0xa5a5a5a5u);
    level.cogs.push_back(Cog{ 3001, 14, "logic.cog", { cogValue } });

    SedLight light{};
    light.id = 4001; light.flags = 5; light.mark = 6; light.layer = 2;
    light.color = ColorF{ 0.135f, 0.246f, 0.579f, 0.864f };
    light.intensity = 83.125; light.range = 29.625;
    light.position = { 12.25, 13.5, -14.75 };
    level.lights.push_back(light);

    HNode node{};
    node.id = 5001; node.num = 27; node.type = 0xf1234567u;
    node.mesh = 9; node.parent = -1;
    node.position = { 1.0, 2.0, 3.0 };
    node.rotation = { 4.0, 5.0, 6.0 };
    node.pivot = { 7.0, 8.0, 9.0 };
    node.name = "mesh hierarchy node";
    level.h3doNodes.push_back(node);
    return level;
}

Level resourceCategoryLevel()
{
    Level source = richLevel(ProjectType::IJIM);
    const auto makeTplResource = [](const std::string& name,
        const AdjType type, const std::string& resource)
    {
        TplValue value{};
        value.name = name;
        value.type = ValueType::str;
        value.stringValue = resource;
        value.adjType = type;
        return value;
    };
    TplValue unknown{};
    unknown.name = "unknown_param";
    unknown.type = ValueType::str;
    unknown.stringValue = "some_string";
    unknown.adjType = AdjType::unk;
    const std::vector<TplValue> typedTemplateValues{
        unknown,
        makeTplResource("material_param", AdjType::mat, "typed.mat"),
        makeTplResource("cog_param", AdjType::cog, "typed_class.cog"),
        makeTplResource("soundclass_param", AdjType::snd, "actor.snd"),
        makeTplResource("puppet_param", AdjType::pup, "actor.pup"),
        makeTplResource("sprite_param", AdjType::spr, "actor.spr"),
        makeTplResource("particle_param", AdjType::par, "smoke.par"),
        makeTplResource("model_param", AdjType::model3do, "actor.3do"),
        makeTplResource("ai_param", AdjType::ai, "actor.ai"),
        makeTplResource("template_param", AdjType::tpl, "typed_template"),
        makeTplResource("empty_sprite_param", AdjType::spr, ""),
    };
    source.things[0].values.insert(source.things[0].values.end(),
        typedTemplateValues.begin(), typedTemplateValues.end());
    source.templates[0].values.insert(source.templates[0].values.end(),
        typedTemplateValues.begin(), typedTemplateValues.end());

    const auto makeCogResource = [](const std::string& name,
        const CogType type, const std::string& resource)
    {
        CogValue value{};
        value.name = name;
        value.type = ValueType::str;
        value.stringValue = resource;
        value.cogType = type;
        return value;
    };
    source.cogs[0].values.push_back(
        makeCogResource("key_param", CogType::key, "walk.key"));
    source.cogs[0].values.push_back(
        makeCogResource("wav_param", CogType::wav, "voice.wav"));
    source.cogs[0].values.push_back(
        makeCogResource("cog_model_param", CogType::model3do,
            "cog_actor.3do"));
    source.cogs[0].values.push_back(
        makeCogResource("cog_ai_param", CogType::ai, "cog_actor.ai"));
    source.cogs[0].values.push_back(
        makeCogResource("cog_mat_param", CogType::mat, "cog.mat"));
    source.cogs[0].values.push_back(
        makeCogResource("linked_cog_param", CogType::cog, "linked.cog"));
    source.cogs[0].values.push_back(
        makeCogResource("cog_template_param", CogType::template_,
            "cog_template"));
    return source;
}

struct ResourceEncodingEstimate final
{
    std::size_t occurrenceCount{};
    std::size_t uniqueStringCount{};
    std::size_t inlineStringBytes{};
    std::size_t catalogAndCompactReferenceBytes{};
    std::size_t catalogAndFixedReferenceBytes{};
};

ResourceEncodingEstimate estimateResourceEncoding(const Level& level)
{
    // Keep this estimator independent from the production ResourceIndex. It
    // models only the bytes affected by the V1 catalog decision: every string
    // occurrence inline, or one categorized spelling plus a compact/fixed
    // local reference at each occurrence. Collection counts are common to
    // both layouts and intentionally excluded.
    constexpr std::size_t categoryCount = 14;
    enum Category : std::size_t
    {
        names, colormaps, materials, sounds, aiClasses, models3do, sprites,
        keyframes, puppets, soundClasses, cogScripts, templates, particles,
        layers
    };
    std::array<std::vector<std::string>, categoryCount> catalogs{};
    std::vector<std::pair<Category, std::string>> occurrences{};
    const auto record = [&](const Category category, const std::string& value)
    {
        occurrences.emplace_back(category, value);
        if (value.empty()) return;
        auto& catalog = catalogs[category];
        if (std::find(catalog.begin(), catalog.end(), value) == catalog.end())
            catalog.push_back(value);
    };
    const auto tplCategory = [](const AdjType type)
    {
        switch (type)
        {
        case AdjType::mat: return materials;
        case AdjType::cog: return cogScripts;
        case AdjType::snd: return soundClasses;
        case AdjType::pup: return puppets;
        case AdjType::spr: return sprites;
        case AdjType::par: return particles;
        case AdjType::model3do: return models3do;
        case AdjType::ai: return aiClasses;
        case AdjType::tpl: return templates;
        default: return names;
        }
    };
    const auto cogCategory = [](const CogType type)
    {
        switch (type)
        {
        case CogType::ai: return aiClasses;
        case CogType::cog: return cogScripts;
        case CogType::key: return keyframes;
        case CogType::mat: return materials;
        case CogType::model3do: return models3do;
        case CogType::wav: return sounds;
        case CogType::template_: return templates;
        default: return names;
        }
    };
    const auto recordTplValue = [&](const TplValue& value)
    {
        record(names, value.name);
        record(tplCategory(value.adjType), value.stringValue);
    };
    const auto recordCogValue = [&](const CogValue& value)
    {
        record(names, value.name);
        record(cogCategory(value.cogType), value.stringValue);
        record(names, value.description);
    };

    record(colormaps, level.masterCMP);
    for (const std::string& value : level.colormaps) record(colormaps, value);
    for (const std::string& value : level.materials) record(materials, value);
    for (const std::string& value : level.layers) record(layers, value);
    for (const Sector& sector : level.sectors)
    {
        record(colormaps, sector.colormap);
        record(sounds, sector.sound);
        for (const Surface& surface : sector.surfaces)
            record(materials, surface.material);
    }
    for (const Thing& thing : level.things)
    {
        record(templates, thing.templateName);
        record(names, thing.name);
        for (const TplValue& value : thing.values) recordTplValue(value);
    }
    for (const ThingTemplate& value : level.templates)
    {
        record(templates, value.name);
        record(templates, value.parent);
        for (const TplValue& parameter : value.values) recordTplValue(parameter);
        record(names, value.desc);
    }
    for (const Cog& cog : level.cogs)
    {
        record(cogScripts, cog.name);
        for (const CogValue& value : cog.values) recordCogValue(value);
    }
    for (const HNode& node : level.h3doNodes) record(names, node.name);

    ResourceEncodingEstimate result{};
    result.occurrenceCount = occurrences.size();
    result.catalogAndCompactReferenceBytes = categoryCount * sizeof(std::uint32_t);
    result.catalogAndFixedReferenceBytes = categoryCount * sizeof(std::uint32_t);
    for (const auto& catalog : catalogs)
    {
        result.uniqueStringCount += catalog.size();
        for (const std::string& value : catalog)
        {
            const std::size_t encoded = sizeof(std::uint32_t) + value.size();
            result.catalogAndCompactReferenceBytes += encoded;
            result.catalogAndFixedReferenceBytes += encoded;
        }
    }
    const auto ulebBytes = [](std::uint32_t value)
    {
        std::size_t bytes = 1;
        while (value >= 0x80u)
        {
            value >>= 7u;
            ++bytes;
        }
        return bytes;
    };
    for (const auto& [category, value] : occurrences)
    {
        result.inlineStringBytes += sizeof(std::uint32_t) + value.size();
        std::uint32_t wire{};
        if (!value.empty())
        {
            const auto& catalog = catalogs[category];
            const auto found = std::find(catalog.begin(), catalog.end(), value);
            wire = static_cast<std::uint32_t>(std::distance(catalog.begin(), found)) + 1u;
        }
        result.catalogAndCompactReferenceBytes += ulebBytes(wire);
        result.catalogAndFixedReferenceBytes += sizeof(std::uint32_t);
    }
    return result;
}

void verifyCriticalFields(const Level& value, const ProjectType kind, const std::string& label)
{
    check(value.kind == kind, label + ": project type");
    check(value.header.fog.enabled
            && value.header.fog.color == ColorF{ 0.11f, 0.22f, 0.33f, 0.44f }
            && value.header.fog.fogStart == 7.25
            && value.header.fog.fogEnd == 987.5,
        label + ": fog RGBA and range are exact");
    check(value.exportBufferExtras == LevelExportBufferExtras{ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12 },
        label + ": export buffer extras are exact");
    check(value.curId == 9142 && value.materials.size() == 3 && value.templates.size() == 1,
        label + ": editor resources, templates, and ID state survive");
    check(value.sectors.size() == 1
            && value.sectors[0].id == 1001
            && value.sectors[0].ambient == ColorF{ 0.19f, 0.31f, 0.47f, 0.63f }
            && value.sectors[0].extraLight == ColorF{ -0.2f, 0.13f, 0.29f, 0.77f }
            && value.sectors[0].pointLight.color == ColorF{ 0.81f, 0.53f, 0.27f, 0.91f }
            && value.sectors[0].pointLight.position == TVector{ 1.125, -2.25, 3.5 }
            && value.sectors[0].thrust == TVector{ -0.125, 9.25, 1.5 }
            && value.sectors[0].collideBox.z2 == 13.0,
        label + ": RGB(A), point light, thrust, and collide box survive");
    check(value.sectors.size() == 1 && value.sectors[0].surfaces.size() == 1
            && value.sectors[0].surfaces[0].extraLight == ColorF{ -0.9f, 0.2f, 0.7f, 0.35f }
            && value.sectors[0].surfaces[0].texVertices[1].color == ColorF{ 0.5f, 0.6f, 0.7f, 0.8f }
            && value.sectors[0].surfaces[0].adjoin == SurfaceRef{ 22, 33 },
        label + ": Surface RGB(A), UV colors, and adjoin survive");
    check(value.things.size() == 1
            && value.things[0].id == 2001
            && value.things[0].templateName == "+derived_actor"
            && value.things[0].name == "instance_name_not_template"
            && value.things[0].values.size() == 1
            && value.things[0].values[0].adjType == AdjType::frame,
        label + ": Thing identity, template name, and typed values survive");
    check(value.cogs.size() == 1 && value.cogs[0].values.size() == 1
            && value.cogs[0].values[0].local
            && value.cogs[0].values[0].mask == static_cast<std::int32_t>(0xa5a5a5a5u)
            && value.cogs[0].values[0].description == "Linked authored surface",
        label + ": COG value metadata survives");
    check(value.lights.size() == 1
            && value.lights[0].color == ColorF{ 0.135f, 0.246f, 0.579f, 0.864f }
            && value.lights[0].intensity == 83.125,
        label + ": baked light RGBA and intensity survive");
    check(value.h3doNodes.size() == 1 && value.h3doNodes[0].type == 0xf1234567u,
        label + ": hierarchy data survives");
}

void verifyCompleteLevelRoundTrip(const Level& source, const Level& loaded,
    const std::string& label)
{
    const auto sameColor = [](const ColorF& a, const ColorF& b)
    {
        return a == b;
    };
    const auto sameVector = [](const TVector& a, const TVector& b)
    {
        return a.x == b.x && a.y == b.y && a.z == b.z;
    };
    const auto sameBox = [](const TBox& a, const TBox& b)
    {
        return a.x1 == b.x1 && a.y1 == b.y1 && a.z1 == b.z1
            && a.x2 == b.x2 && a.y2 == b.y2 && a.z2 == b.z2;
    };
    const auto sameValue = [](const Value& a, const Value& b)
    {
        return a.name == b.name && a.type == b.type
            && a.intValue == b.intValue && a.floatValue == b.floatValue
            && a.stringValue == b.stringValue;
    };
    const auto sameTplValue = [&](const TplValue& a, const TplValue& b)
    {
        return sameValue(a, b) && a.adjType == b.adjType;
    };
    const auto sameCogValue = [&](const CogValue& a, const CogValue& b)
    {
        return sameValue(a, b) && a.cogType == b.cogType
            && a.local == b.local && a.description == b.description
            && a.mask == b.mask;
    };
    bool equal = source.kind == loaded.kind
        && source.header.version == loaded.header.version
        && source.header.gravity == loaded.header.gravity
        && source.header.horizonSky.distance == loaded.header.horizonSky.distance
        && source.header.horizonSky.pixelsPerRev == loaded.header.horizonSky.pixelsPerRev
        && source.header.horizonSky.offset.x == loaded.header.horizonSky.offset.x
        && source.header.horizonSky.offset.y == loaded.header.horizonSky.offset.y
        && source.header.ceilingSky.height == loaded.header.ceilingSky.height
        && source.header.ceilingSky.offset.x == loaded.header.ceilingSky.offset.x
        && source.header.ceilingSky.offset.y == loaded.header.ceilingSky.offset.y
        && source.header.mipmapDistances == loaded.header.mipmapDistances
        && source.header.lodDistances == loaded.header.lodDistances
        && source.header.perspectiveDistance == loaded.header.perspectiveDistance
        && source.header.gouraudDistance == loaded.header.gouraudDistance
        && source.header.fog.enabled == loaded.header.fog.enabled
        && sameColor(source.header.fog.color, loaded.header.fog.color)
        && source.header.fog.fogStart == loaded.header.fog.fogStart
        && source.header.fog.fogEnd == loaded.header.fog.fogEnd
        && source.exportBufferExtras == loaded.exportBufferExtras
        && source.masterCMP == loaded.masterCMP
        && source.ppunit == loaded.ppunit
        && source.lvisstring == loaded.lvisstring
        && source.curId == loaded.curId
        && source.colormaps == loaded.colormaps
        && source.materials == loaded.materials
        && source.layers == loaded.layers
        && source.sectors.size() == loaded.sectors.size()
        && source.cogs.size() == loaded.cogs.size()
        && source.templates.size() == loaded.templates.size()
        && source.things.size() == loaded.things.size()
        && source.lights.size() == loaded.lights.size()
        && source.h3doNodes.size() == loaded.h3doNodes.size();
    if (equal)
    {
        for (std::size_t i = 0; i < source.sectors.size() && equal; ++i)
        {
            const Sector& a = source.sectors[i];
            const Sector& b = loaded.sectors[i];
            equal = a.id == b.id && a.num == b.num && a.flags == b.flags
                && sameColor(a.ambient, b.ambient)
                && sameColor(a.extraLight, b.extraLight)
                && sameColor(a.pointLight.color, b.pointLight.color)
                && sameVector(a.pointLight.position, b.pointLight.position)
                && a.pointLight.minRange == b.pointLight.minRange
                && a.pointLight.maxRange == b.pointLight.maxRange
                && a.colormap == b.colormap && sameColor(a.tint, b.tint)
                && a.sound == b.sound && a.soundVolume == b.soundVolume
                && sameVector(a.thrust, b.thrust)
                && sameBox(a.collideBox, b.collideBox)
                && a.layer == b.layer && a.mark == b.mark
                && a.vertices.size() == b.vertices.size()
                && a.surfaces.size() == b.surfaces.size();
            if (!equal) break;
            for (std::size_t j = 0; j < a.vertices.size() && equal; ++j)
            {
                equal = a.vertices[j].x == b.vertices[j].x
                    && a.vertices[j].y == b.vertices[j].y
                    && a.vertices[j].z == b.vertices[j].z
                    && a.vertices[j].mark == b.vertices[j].mark;
            }
            for (std::size_t j = 0; j < a.surfaces.size() && equal; ++j)
            {
                const Surface& sa = a.surfaces[j];
                const Surface& sb = b.surfaces[j];
                equal = sa.num == sb.num && sa.mark == sb.mark
                    && sa.d3dId == sb.d3dId && sa.surfaceFlags == sb.surfaceFlags
                    && sa.faceFlags == sb.faceFlags && sa.geo == sb.geo
                    && sa.light == sb.light && sa.tex == sb.tex
                    && sameColor(sa.extraLight, sb.extraLight)
                    && sa.adjoinFlags == sb.adjoinFlags && sa.adjoin == sb.adjoin
                    && sa.material == sb.material && sa.nAdj == sb.nAdj
                    && sa.nMat == sb.nMat && sa.uScale == sb.uScale
                    && sa.vScale == sb.vScale && sameVector(sa.normal, sb.normal)
                    && sa.vertices == sb.vertices
                    && sa.texVertices.size() == sb.texVertices.size();
                for (std::size_t k = 0; k < sa.texVertices.size() && equal; ++k)
                {
                    equal = sa.texVertices[k].u == sb.texVertices[k].u
                        && sa.texVertices[k].v == sb.texVertices[k].v
                        && sameColor(sa.texVertices[k].color, sb.texVertices[k].color);
                }
            }
        }
        for (std::size_t i = 0; i < source.things.size() && equal; ++i)
        {
            const Thing& a = source.things[i];
            const Thing& b = loaded.things[i];
            equal = a.id == b.id && a.num == b.num && a.sector == b.sector
                && a.templateName == b.templateName && a.name == b.name
                && a.x == b.x && a.y == b.y && a.z == b.z
                && a.pch == b.pch && a.yaw == b.yaw && a.rol == b.rol
                && a.layer == b.layer && sameBox(a.bbox, b.bbox)
                && a.values.size() == b.values.size();
            for (std::size_t j = 0; j < a.values.size() && equal; ++j)
            {
                equal = sameTplValue(a.values[j], b.values[j]);
            }
        }
        for (std::size_t i = 0; i < source.templates.size() && equal; ++i)
        {
            const ThingTemplate& a = source.templates[i];
            const ThingTemplate& b = loaded.templates[i];
            equal = a.name == b.name && a.parent == b.parent
                && a.desc == b.desc && sameBox(a.bbox, b.bbox)
                && a.values.size() == b.values.size();
            for (std::size_t j = 0; j < a.values.size() && equal; ++j)
            {
                equal = sameTplValue(a.values[j], b.values[j]);
            }
        }
        for (std::size_t i = 0; i < source.cogs.size() && equal; ++i)
        {
            const Cog& a = source.cogs[i];
            const Cog& b = loaded.cogs[i];
            equal = a.id == b.id && a.num == b.num && a.name == b.name
                && a.values.size() == b.values.size();
            for (std::size_t j = 0; j < a.values.size() && equal; ++j)
            {
                equal = sameCogValue(a.values[j], b.values[j]);
            }
        }
        for (std::size_t i = 0; i < source.lights.size() && equal; ++i)
        {
            const SedLight& a = source.lights[i];
            const SedLight& b = loaded.lights[i];
            equal = a.id == b.id && a.flags == b.flags && a.mark == b.mark
                && a.layer == b.layer && sameColor(a.color, b.color)
                && a.intensity == b.intensity && a.range == b.range
                && sameVector(a.position, b.position);
        }
        for (std::size_t i = 0; i < source.h3doNodes.size() && equal; ++i)
        {
            const HNode& a = source.h3doNodes[i];
            const HNode& b = loaded.h3doNodes[i];
            equal = a.id == b.id && a.num == b.num && a.type == b.type
                && a.mesh == b.mesh && a.parent == b.parent
                && sameVector(a.position, b.position)
                && sameVector(a.rotation, b.rotation)
                && sameVector(a.pivot, b.pivot) && a.name == b.name;
        }
    }
    check(equal, label + ": every authored Level field is present");
    check(assets::saveToSedBytes(source) == assets::saveToSedBytes(loaded),
        label + ": complete field-by-field semantic round trip is byte exact");
}

std::string deterministicNoise(const std::size_t size)
{
    std::string result(size, ' ');
    std::uint32_t state = 0x6d2b'79f5u;
    for (char& value : result)
    {
        state ^= state << 13u;
        state ^= state >> 17u;
        state ^= state << 5u;
        value = static_cast<char>(33u + state % 90u);
    }
    return result;
}

std::uint64_t readU64Le(const std::span<const std::byte> bytes, const std::size_t offset)
{
    std::uint64_t value{};
    for (unsigned i = 0; i < 8; ++i)
    {
        value |= static_cast<std::uint64_t>(std::to_integer<std::uint8_t>(bytes[offset + i]))
            << (i * 8);
    }
    return value;
}

std::uint32_t readU32Le(const std::span<const std::byte> bytes, const std::size_t offset)
{
    std::uint32_t value{};
    for (unsigned i = 0; i < 4; ++i)
    {
        value |= static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(bytes[offset + i]))
            << (i * 8);
    }
    return value;
}

std::int32_t readSedIndex(const std::span<const std::byte> bytes,
    std::size_t& cursor)
{
    std::uint32_t encoded{};
    for (unsigned byteIndex = 0; byteIndex < 5; ++byteIndex)
    {
        if (cursor >= bytes.size()) throw std::runtime_error("truncated test varint");
        const std::uint8_t byte = std::to_integer<std::uint8_t>(bytes[cursor++]);
        if (byteIndex == 4 && (byte & 0xf0u) != 0)
            throw std::runtime_error("overflowing test varint");
        encoded |= static_cast<std::uint32_t>(byte & 0x7fu) << (byteIndex * 7);
        if ((byte & 0x80u) == 0)
        {
            if (byteIndex != 0 && (byte & 0x7fu) == 0)
                throw std::runtime_error("noncanonical test varint");
            if (encoded == 0) return -1;
            const std::uint32_t value = encoded - 1u;
            if (value > static_cast<std::uint32_t>(
                    std::numeric_limits<std::int32_t>::max()))
            {
                throw std::runtime_error("test index exceeds model range");
            }
            return static_cast<std::int32_t>(value);
        }
    }
    throw std::runtime_error("overlong test varint");
}

void skipSedIndex(const std::span<const std::byte> bytes,
    std::size_t& cursor)
{
    (void)readSedIndex(bytes, cursor);
}

std::string readTestString(const std::span<const std::byte> bytes,
    std::size_t& cursor)
{
    const std::uint32_t size = readU32Le(bytes, cursor);
    cursor += 4;
    if (size > bytes.size() - cursor)
        throw std::runtime_error("truncated test string");
    const auto chars = bytes.subspan(cursor, size);
    cursor += size;
    return std::string(reinterpret_cast<const char*>(chars.data()), chars.size());
}

std::vector<std::string> readTestStringVector(
    const std::span<const std::byte> bytes, std::size_t& cursor)
{
    const std::uint32_t count = readU32Le(bytes, cursor);
    cursor += 4;
    std::vector<std::string> values{};
    values.reserve(count);
    for (std::uint32_t index = 0; index < count; ++index)
        values.push_back(readTestString(bytes, cursor));
    return values;
}

void writeU32Le(std::vector<std::byte>& bytes, const std::size_t offset,
    const std::uint32_t value)
{
    for (unsigned i = 0; i < 4; ++i)
    {
        bytes[offset + i] = static_cast<std::byte>(
            static_cast<std::uint8_t>(value >> (i * 8)));
    }
}

constexpr std::size_t kTestHeaderSize = 28;
constexpr std::size_t kTestSectionDescriptorSize = 100;
constexpr std::size_t kTestSectionTrailerSize = 116;
constexpr std::size_t kTestFecSlotBytes = 4096;
constexpr std::size_t kTestFecPayloadBytes = 4092;

struct SectionLayout final
{
    std::size_t descriptorOffset{};
    std::size_t payloadOffset{};
    std::size_t trailerOffset{};
    std::uint64_t storedSize{};
    std::uint64_t logicalStoredSize{};
    std::uint32_t dataStreams{};
    std::uint32_t parityStreams{};
    std::uint32_t chunkPayloadSize{};
    std::uint32_t stripeCount{};
    std::uint32_t codedOffset{};
};

SectionLayout findSection(const std::vector<std::byte>& file,
    const std::array<char, 4>& type)
{
    const auto bytes = std::span<const std::byte>(file);
    std::size_t cursor = kTestHeaderSize;
    const std::uint32_t count = readU32Le(bytes, 12);
    for (std::uint32_t section = 0; section < count; ++section)
    {
        check(cursor + kTestSectionDescriptorSize <= bytes.size(),
            "SED section fixture descriptor is in range");
        if (cursor + kTestSectionDescriptorSize > bytes.size()) return {};
        const std::array<char, 4> found{
            static_cast<char>(std::to_integer<unsigned char>(bytes[cursor + 8])),
            static_cast<char>(std::to_integer<unsigned char>(bytes[cursor + 9])),
            static_cast<char>(std::to_integer<unsigned char>(bytes[cursor + 10])),
            static_cast<char>(std::to_integer<unsigned char>(bytes[cursor + 11])) };
        const std::uint64_t storedSize = readU64Le(bytes, cursor + 28);
        const std::size_t payloadOffset = cursor + kTestSectionDescriptorSize;
        const std::size_t trailerOffset = payloadOffset
            + static_cast<std::size_t>(storedSize);
        check(trailerOffset + kTestSectionTrailerSize <= bytes.size(),
            "SED section fixture payload is in range");
        if (found == type)
        {
            return SectionLayout{
                cursor,
                payloadOffset,
                trailerOffset,
                storedSize,
                readU64Le(bytes, cursor + 36),
                readU32Le(bytes, cursor + 44),
                readU32Le(bytes, cursor + 48),
                readU32Le(bytes, cursor + 52),
                readU32Le(bytes, cursor + 56),
                readU32Le(bytes, cursor + 60) };
        }
        cursor = trailerOffset + kTestSectionTrailerSize;
    }
    check(false, "SED section fixture exposes requested semantic section");
    return {};
}

std::size_t slotOffset(const SectionLayout& section,
    const std::size_t stream, const std::size_t stripe)
{
    return section.payloadOffset + section.codedOffset
        + (stream * section.stripeCount + stripe) * kTestFecSlotBytes;
}

std::vector<std::byte> logicalStoredBytes(
    const std::vector<std::byte>& file, const SectionLayout& section)
{
    std::vector<std::byte> result(
        static_cast<std::size_t>(section.logicalStoredSize), std::byte{0});
    for (std::size_t stream = 0; stream < section.dataStreams; ++stream)
    {
        for (std::size_t stripe = 0; stripe < section.stripeCount; ++stripe)
        {
            const std::uint64_t logicalOffset = (stream * section.stripeCount + stripe)
                * static_cast<std::uint64_t>(kTestFecPayloadBytes);
            if (logicalOffset >= section.logicalStoredSize) continue;
            const std::size_t count = static_cast<std::size_t>(std::min<std::uint64_t>(
                kTestFecPayloadBytes, section.logicalStoredSize - logicalOffset));
            std::copy_n(file.begin() + static_cast<std::ptrdiff_t>(
                    slotOffset(section, stream, stripe)), count,
                result.begin() + static_cast<std::ptrdiff_t>(logicalOffset));
        }
    }
    return result;
}

struct TestLzssToken final
{
    bool match{};
    std::size_t decodedOffset{};
    std::size_t length{};
    std::size_t backwardOffset{};
};

struct TestLzssDecode final
{
    std::vector<std::byte> bytes{};
    std::vector<TestLzssToken> tokens{};
};

TestLzssDecode decodeTestLzss(const std::span<const std::byte> stored,
    const std::size_t expectedSize)
{
    TestLzssDecode result{};
    std::size_t pos{};
    while (pos < stored.size())
    {
        const std::size_t decodedOffset = result.bytes.size();
        const std::uint8_t token = std::to_integer<std::uint8_t>(stored[pos++]);
        if ((token & 0x80u) == 0)
        {
            const std::size_t count = static_cast<std::size_t>(token) + 1;
            if (count > stored.size() - pos) throw std::logic_error("bad test LZSS literal");
            result.tokens.push_back(TestLzssToken{ false, decodedOffset, count, 0 });
            result.bytes.insert(result.bytes.end(),
                stored.begin() + static_cast<std::ptrdiff_t>(pos),
                stored.begin() + static_cast<std::ptrdiff_t>(pos + count));
            pos += count;
            continue;
        }

        if (stored.size() - pos < 2) throw std::logic_error("bad test LZSS match");
        const std::size_t count = static_cast<std::size_t>(token & 0x7fu) + 4;
        const std::size_t offset = std::to_integer<std::uint8_t>(stored[pos])
            | (static_cast<std::size_t>(std::to_integer<std::uint8_t>(stored[pos + 1])) << 8);
        pos += 2;
        if (offset == 0 || offset > result.bytes.size())
            throw std::logic_error("bad test LZSS offset");
        result.tokens.push_back(TestLzssToken{ true, decodedOffset, count, offset });
        for (std::size_t i = 0; i < count; ++i)
        {
            result.bytes.push_back(result.bytes[result.bytes.size() - offset]);
        }
    }
    if (result.bytes.size() != expectedSize)
        throw std::logic_error("bad test LZSS expanded size");
    return result;
}

std::vector<std::byte> bytesFromHex(const std::string_view hex)
{
    if ((hex.size() & 1u) != 0) throw std::logic_error("odd test-vector hex length");
    const auto nibble = [](const char value) -> unsigned
    {
        if (value >= '0' && value <= '9') return static_cast<unsigned>(value - '0');
        if (value >= 'a' && value <= 'f') return 10u + static_cast<unsigned>(value - 'a');
        if (value >= 'A' && value <= 'F') return 10u + static_cast<unsigned>(value - 'A');
        throw std::logic_error("invalid test-vector hex digit");
    };
    std::vector<std::byte> result(hex.size() / 2u);
    for (std::size_t i = 0; i < result.size(); ++i)
    {
        result[i] = static_cast<std::byte>((nibble(hex[i * 2u]) << 4u)
            | nibble(hex[i * 2u + 1u]));
    }
    return result;
}

void damageSlots(std::vector<std::byte>& file, const SectionLayout& section,
    const std::span<const std::size_t> streams, const std::size_t stripe = 0,
    const bool crcOnly = false)
{
    check(section.chunkPayloadSize == kTestFecPayloadBytes
            && stripe < section.stripeCount
            && section.dataStreams + section.parityStreams >= streams.size(),
        "SED FEC fixture exposes requested checked slots");
    for (std::size_t i = 0; i < streams.size(); ++i)
    {
        check(streams[i] < section.dataStreams + section.parityStreams,
            "SED FEC fixture stream index is valid");
        const std::size_t offset = slotOffset(section, streams[i], stripe)
            + (crcOnly ? kTestFecPayloadBytes : std::min<std::size_t>(17 + i,
                kTestFecPayloadBytes - 1));
        file[offset] ^= std::byte{ static_cast<unsigned char>(0x11u + i) };
    }
}

void damageFirstSlots(std::vector<std::byte>& file, const SectionLayout& section,
    const std::size_t count, const bool crcOnly = false)
{
    std::vector<std::size_t> streams(count);
    for (std::size_t i = 0; i < count; ++i) streams[i] = i;
    damageSlots(file, section, streams, 0, crcOnly);
}

using TestGfMatrix = std::array<std::array<std::uint8_t, 16>, 16>;
using TestGfGenerator = std::array<std::array<std::uint8_t, 16>, 20>;

std::uint8_t testGfMul(std::uint8_t lhs, std::uint8_t rhs) noexcept
{
    std::uint8_t result{};
    for (unsigned bit = 0; bit < 8; ++bit)
    {
        if ((rhs & 1u) != 0) result ^= lhs;
        const bool high = (lhs & 0x80u) != 0;
        lhs = static_cast<std::uint8_t>(lhs << 1u);
        if (high) lhs ^= 0x1du; // x^8 + x^4 + x^3 + x^2 + 1 (0x11d)
        rhs = static_cast<std::uint8_t>(rhs >> 1u);
    }
    return result;
}

std::uint8_t testGfPow(std::uint8_t value, unsigned exponent) noexcept
{
    std::uint8_t result = 1;
    while (exponent != 0)
    {
        if ((exponent & 1u) != 0) result = testGfMul(result, value);
        value = testGfMul(value, value);
        exponent >>= 1u;
    }
    return result;
}

std::optional<TestGfMatrix> invertTestGfMatrix(
    TestGfMatrix matrix, const std::size_t size)
{
    TestGfMatrix inverse{};
    for (std::size_t row = 0; row < size; ++row) inverse[row][row] = 1;
    for (std::size_t column = 0; column < size; ++column)
    {
        std::size_t pivot = column;
        while (pivot < size && matrix[pivot][column] == 0) ++pivot;
        if (pivot == size) return std::nullopt;
        if (pivot != column)
        {
            std::swap(matrix[pivot], matrix[column]);
            std::swap(inverse[pivot], inverse[column]);
        }
        const std::uint8_t scale = testGfPow(matrix[column][column], 254u);
        for (std::size_t item = 0; item < size; ++item)
        {
            matrix[column][item] = testGfMul(matrix[column][item], scale);
            inverse[column][item] = testGfMul(inverse[column][item], scale);
        }
        for (std::size_t row = 0; row < size; ++row)
        {
            if (row == column) continue;
            const std::uint8_t factor = matrix[row][column];
            if (factor == 0) continue;
            for (std::size_t item = 0; item < size; ++item)
            {
                matrix[row][item] ^= testGfMul(factor, matrix[column][item]);
                inverse[row][item] ^= testGfMul(factor, inverse[column][item]);
            }
        }
    }
    return inverse;
}

TestGfGenerator independentTestFecGenerator(const std::size_t dataStreams)
{
    TestGfGenerator vandermonde{};
    for (std::size_t row = 0; row < dataStreams + 4u; ++row)
    {
        std::uint8_t value = 1;
        for (std::size_t column = 0; column < dataStreams; ++column)
        {
            vandermonde[row][column] = value;
            value = testGfMul(value, static_cast<std::uint8_t>(row));
        }
    }
    TestGfMatrix top{};
    for (std::size_t row = 0; row < dataStreams; ++row)
    {
        std::copy_n(vandermonde[row].begin(), dataStreams, top[row].begin());
    }
    const auto inverse = invertTestGfMatrix(top, dataStreams);
    check(inverse.has_value(), "FEC independent Vandermonde top matrix is invertible");
    TestGfGenerator generator{};
    if (!inverse) return generator;
    for (std::size_t row = 0; row < dataStreams + 4u; ++row)
    {
        for (std::size_t column = 0; column < dataStreams; ++column)
        {
            for (std::size_t source = 0; source < dataStreams; ++source)
            {
                generator[row][column] ^= testGfMul(
                    vandermonde[row][source], (*inverse)[source][column]);
            }
        }
    }
    return generator;
}

void resealProtectedSectionPayload(
    std::vector<std::byte>& file,
    const SectionLayout& section,
    const std::span<const std::byte> payload)
{
    check(section.parityStreams == 4u
            && section.logicalStoredSize == payload.size()
            && section.dataStreams >= 1u
            && section.dataStreams <= 16u,
        "SED protected fixture accepts an uncompressed core-tier payload");
    if (section.parityStreams != 4u
        || section.logicalStoredSize != payload.size()
        || section.dataStreams == 0u || section.dataStreams > 16u)
    {
        return;
    }

    const TestGfGenerator generator =
        independentTestFecGenerator(section.dataStreams);
    for (std::size_t stripe = 0; stripe < section.stripeCount; ++stripe)
    {
        for (std::size_t stream = 0; stream < section.dataStreams; ++stream)
        {
            const std::size_t logicalOffset =
                (stream * section.stripeCount + stripe) * kTestFecPayloadBytes;
            const std::size_t copied = logicalOffset < payload.size()
                ? std::min(kTestFecPayloadBytes, payload.size() - logicalOffset)
                : 0u;
            const std::size_t destination = slotOffset(section, stream, stripe);
            std::fill_n(file.begin() + static_cast<std::ptrdiff_t>(destination),
                kTestFecPayloadBytes, std::byte{0});
            if (copied != 0u)
            {
                std::copy_n(payload.begin()
                        + static_cast<std::ptrdiff_t>(logicalOffset),
                    copied,
                    file.begin() + static_cast<std::ptrdiff_t>(destination));
            }
        }
        for (std::size_t parity = 0; parity < section.parityStreams; ++parity)
        {
            const std::size_t destination = slotOffset(
                section, section.dataStreams + parity, stripe);
            for (std::size_t byteIndex = 0;
                byteIndex < kTestFecPayloadBytes; ++byteIndex)
            {
                std::uint8_t value{};
                for (std::size_t stream = 0; stream < section.dataStreams; ++stream)
                {
                    const std::uint8_t source = std::to_integer<std::uint8_t>(
                        file[slotOffset(section, stream, stripe) + byteIndex]);
                    value ^= testGfMul(
                        generator[section.dataStreams + parity][stream], source);
                }
                file[destination + byteIndex] = static_cast<std::byte>(value);
            }
        }
        for (std::size_t stream = 0;
            stream < section.dataStreams + section.parityStreams; ++stream)
        {
            const std::size_t slot = slotOffset(section, stream, stripe);
            writeU32Le(file, slot + kTestFecPayloadBytes,
                assets::calculateSedCrc32c(
                    std::span<const std::byte>(file).subspan(
                        slot, kTestFecPayloadBytes)));
        }
    }

    constexpr std::size_t descriptorHashOffset = 64u;
    constexpr std::size_t descriptorCrcOffset = 96u;
    constexpr std::size_t mirroredDescriptorOffset = 12u;
    const auto digest = assets::calculateSedSha256(payload);
    std::copy(digest.begin(), digest.end(),
        file.begin() + static_cast<std::ptrdiff_t>(
            section.descriptorOffset + descriptorHashOffset));
    const std::size_t mirror = section.trailerOffset
        + mirroredDescriptorOffset;
    std::copy(digest.begin(), digest.end(),
        file.begin() + static_cast<std::ptrdiff_t>(
            mirror + descriptorHashOffset));
    writeU32Le(file, section.descriptorOffset + descriptorCrcOffset,
        assets::calculateSedCrc32c(std::span<const std::byte>(file)
            .subspan(section.descriptorOffset, descriptorCrcOffset)));
    writeU32Le(file, mirror + descriptorCrcOffset,
        assets::calculateSedCrc32c(std::span<const std::byte>(file)
            .subspan(mirror, descriptorCrcOffset)));
    writeU32Le(file,
        section.trailerOffset + kTestSectionTrailerSize - 4u,
        assets::calculateSedCrc32c(std::span<const std::byte>(file)
            .subspan(section.trailerOffset,
                kTestSectionTrailerSize - 4u)));
}

void renameProtectedSectionType(
    std::vector<std::byte>& file,
    const SectionLayout& section,
    const std::array<char, 4>& type)
{
    constexpr std::size_t typeOffset = 8u;
    constexpr std::size_t mirroredDescriptorOffset = 12u;
    constexpr std::size_t descriptorCrcOffset = 96u;
    for (std::size_t index = 0; index < type.size(); ++index)
    {
        file[section.descriptorOffset + typeOffset + index] =
            static_cast<std::byte>(type[index]);
        file[section.trailerOffset + mirroredDescriptorOffset
            + typeOffset + index] = static_cast<std::byte>(type[index]);
    }
    const std::size_t mirror = section.trailerOffset
        + mirroredDescriptorOffset;
    writeU32Le(file, section.descriptorOffset + descriptorCrcOffset,
        assets::calculateSedCrc32c(std::span<const std::byte>(file)
            .subspan(section.descriptorOffset, descriptorCrcOffset)));
    writeU32Le(file, mirror + descriptorCrcOffset,
        assets::calculateSedCrc32c(std::span<const std::byte>(file)
            .subspan(mirror, descriptorCrcOffset)));
    writeU32Le(file,
        section.trailerOffset + kTestSectionTrailerSize - 4u,
        assets::calculateSedCrc32c(std::span<const std::byte>(file)
            .subspan(section.trailerOffset,
                kTestSectionTrailerSize - 4u)));
}

bool allTestFecErasureMatricesInvertible(const TestGfGenerator& generator,
    const std::size_t dataStreams)
{
    const std::size_t streamCount = dataStreams + 4u;
    std::array<bool, 20> erased{};
    for (std::size_t erasureCount = 1; erasureCount <= 4; ++erasureCount)
    {
        const auto visit = [&](const auto& self, const std::size_t depth,
            const std::size_t next) -> bool
        {
            if (depth == erasureCount)
            {
                TestGfMatrix decode{};
                std::size_t selected{};
                for (std::size_t stream = 0;
                    stream < streamCount && selected < dataStreams; ++stream)
                {
                    if (!erased[stream]) decode[selected++] = generator[stream];
                }
                return selected == dataStreams
                    && invertTestGfMatrix(decode, dataStreams).has_value();
            }
            const std::size_t remaining = erasureCount - depth;
            for (std::size_t stream = next;
                stream + remaining <= streamCount; ++stream)
            {
                erased[stream] = true;
                if (!self(self, depth + 1, stream + 1)) return false;
                erased[stream] = false;
            }
            return true;
        };
        if (!visit(visit, 0, 0)) return false;
    }
    return true;
}

std::pair<std::size_t, std::size_t> firstSectionDescriptorAndMirror(
    const std::vector<std::byte>& file)
{
    const SectionLayout section = findSection(file, { 'h', 'e', 'a', 'd' });
    return { section.descriptorOffset, section.trailerOffset };
}

constexpr const char* kSedVectorRoot =
    "src/core/tests/assets/fixtures/sed_vectors";

struct SedProjectVector final
{
    ProjectType kind{};
    const char* stem{};
    const char* inputExtension{};
    const char* outputExtension{};
};

constexpr std::array<SedProjectVector, 3> kSedProjectVectors{{
    { ProjectType::JKDF2, "jkdf2", ".jkl", ".jkl" },
    { ProjectType::MOTS, "mots", ".jkl", ".jkl" },
    { ProjectType::IJIM, "ijim", ".ndy", ".ndy" },
}};

std::filesystem::path vectorPath(const SedProjectVector& vector,
    const std::string_view suffix, const std::string_view extension)
{
    return std::filesystem::path(kSedVectorRoot)
        / (std::string(vector.stem) + std::string(suffix)
            + std::string(extension));
}

std::vector<std::byte> readBinaryVector(const std::filesystem::path& path)
{
    std::ifstream input(path, std::ios::binary | std::ios::ate);
    if (!input) throw std::runtime_error("Could not open test vector: " + path.string());
    const std::streampos end = input.tellg();
    if (end < 0) throw std::runtime_error("Could not size test vector: " + path.string());
    std::vector<std::byte> bytes(static_cast<std::size_t>(end));
    input.seekg(0);
    input.read(reinterpret_cast<char*>(bytes.data()),
        static_cast<std::streamsize>(bytes.size()));
    if (!input && !bytes.empty())
        throw std::runtime_error("Could not read test vector: " + path.string());
    return bytes;
}

std::string readTextVector(const std::filesystem::path& path)
{
    std::ifstream input(path, std::ios::binary);
    if (!input) throw std::runtime_error("Could not open test vector: " + path.string());
    return std::string(std::istreambuf_iterator<char>(input), {});
}

void writeBinaryVector(const std::filesystem::path& path,
    const std::span<const std::byte> bytes)
{
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output) throw std::runtime_error("Could not create test vector: " + path.string());
    output.write(reinterpret_cast<const char*>(bytes.data()),
        static_cast<std::streamsize>(bytes.size()));
    if (!output) throw std::runtime_error("Could not write test vector: " + path.string());
}

void writeTextVector(const std::filesystem::path& path,
    const std::string_view text)
{
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output) throw std::runtime_error("Could not create test vector: " + path.string());
    output.write(text.data(), static_cast<std::streamsize>(text.size()));
    if (!output) throw std::runtime_error("Could not write test vector: " + path.string());
}

} // namespace

bool writeSedLevelTestVectors()
{
    try
    {
        std::filesystem::create_directories(kSedVectorRoot);
        for (const SedProjectVector& vector : kSedProjectVectors)
        {
            assets::JklLoadOptions options{};
            options.projectType = vector.kind;
            const Level imported = assets::loadFromJKL(
                vectorPath(vector, "_input", vector.inputExtension), options);
            const std::vector<std::byte> native = assets::saveToSedBytes(imported);
            writeBinaryVector(vectorPath(vector, "_native", ".sed"), native);

            std::ostringstream expected{};
            assets::saveToJKL(imported, expected);
            writeTextVector(vectorPath(vector, "_expected", vector.outputExtension),
                expected.str());

            std::vector<std::byte> unrecoverable = native;
            damageFirstSlots(unrecoverable,
                findSection(unrecoverable, { 's', 'e', 'c', 't' }), 5u);
            writeBinaryVector(vectorPath(
                vector, "_sect_unrecoverable", ".sed"), unrecoverable);
            const assets::SedRecoveryLoadResult recovered =
                assets::recoverFromSedBytes(unrecoverable);
            std::ostringstream partial{};
            assets::saveToJKL(recovered.level, partial);
            writeTextVector(vectorPath(
                vector, "_expected_partial", vector.outputExtension),
                partial.str());
        }

        const std::vector<std::byte> resourceVector =
            assets::saveToSedBytes(resourceCategoryLevel());
        writeBinaryVector(std::filesystem::path(kSedVectorRoot)
            / "all_resource_categories.sed", resourceVector);

        // Keep one small, valid HIST envelope in the checked-in corpus.  The
        // nested document is deliberately a real HistoryNested stream rather
        // than arbitrary bytes, so the fixture exercises both the optional
        // p=2 container and the history-specific inner checksum profile.
        const Level historySource = richLevel(ProjectType::IJIM);
        const std::vector<std::byte> historyNested = assets::saveToSedBytes(
            historySource, {}, assets::SedSerializationProfile::HistoryNested);
        const assets::SedOptionalSection historySection{
            { 'h', 'i', 's', 't' }, historyNested };
        const std::vector<std::byte> historyVector = assets::saveToSedBytes(
            historySource,
            std::span<const assets::SedOptionalSection>(&historySection, 1),
            assets::SedSerializationProfile::ProtectedV1, 0);
        writeBinaryVector(std::filesystem::path(kSedVectorRoot)
            / "hist_small.sed", historyVector);
        const SectionLayout historyLayout = findSection(historyVector,
            { 'h', 'i', 's', 't' });
        for (std::size_t damageCount = 1; damageCount <= 3; ++damageCount)
        {
            std::vector<std::byte> damaged = historyVector;
            std::array<std::size_t, 3> streams{};
            for (std::size_t i = 0; i < damageCount; ++i)
                streams[i] = i;
            for (std::size_t stripe = 0;
                 stripe < historyLayout.stripeCount; ++stripe)
            {
                damageSlots(damaged,
                    historyLayout,
                    std::span<const std::size_t>(streams).first(damageCount),
                    stripe);
            }
            writeBinaryVector(std::filesystem::path(kSedVectorRoot)
                / ("hist_small_" + std::to_string(damageCount)
                    + "_damage.sed"), damaged);
        }
        return true;
    }
    catch (const std::exception& exception)
    {
        std::cerr << "Could not write SED test vectors: "
            << exception.what() << '\n';
        return false;
    }
}

bool writeSedIjimTestVectors()
{
    try
    {
        const SedProjectVector& vector = kSedProjectVectors[2];
        assets::JklLoadOptions options{};
        options.projectType = vector.kind;
        const Level imported = assets::loadFromJKL(
            vectorPath(vector, "_input", vector.inputExtension), options);
        if ( imported.sectors.size() != 1u
            || imported.sectors[0].vertices.size() != 3u
            || imported.sectors[0].surfaces.size() != 1u
            || imported.sectors[0].surfaces[0].vertices.size() != 3u )
        {
            throw std::runtime_error(
                "IJIM input did not import the expected one-sector triangle.");
        }

        const std::vector<std::byte> native = assets::saveToSedBytes(imported);
        writeBinaryVector(vectorPath(vector, "_native", ".sed"), native);

        std::vector<std::byte> unrecoverable = native;
        damageFirstSlots(unrecoverable,
            findSection(unrecoverable, { 's', 'e', 'c', 't' }), 5u);
        writeBinaryVector(vectorPath(
            vector, "_sect_unrecoverable", ".sed"), unrecoverable);

        std::ostringstream expected{};
        assets::saveToJKL(imported, expected);
        writeTextVector(vectorPath(
            vector, "_expected", vector.outputExtension), expected.str());

        const assets::SedRecoveryLoadResult recovered =
            assets::recoverFromSedBytes(unrecoverable);
        std::ostringstream partial{};
        assets::saveToJKL(recovered.level, partial);
        writeTextVector(vectorPath(
            vector, "_expected_partial", vector.outputExtension),
            partial.str());
        return true;
    }
    catch (const std::exception& exception)
    {
        std::cerr << "Could not write IJIM SED test vectors: "
            << exception.what() << '\n';
        return false;
    }
}

void runSedLevelTests()
{
    {
        const char* const jkl = R"JKL(SECTION: HEADER
VERSION 1
END
SECTION: GEORESOURCE
WORLD COLORMAPS 0
WORLD VERTICES 3
0: 0 0 0
1: 1 0 0
2: 0 1 0
WORLD TEXTURE VERTICES 1
0: 0 0
WORLD ADJOINS 0
WORLD SURFACES 1
0: -1 0x0 0x0 0 3 3 -1 0 3 0,0 1,0 2,0
END
SECTION: SECTORS
WORLD SECTORS 1
SECTOR 0
FLAGS 0
AMBIENT LIGHT 0
EXTRA LIGHT 0
VERTICES 3
0: 0
1: 1
2: 2
SURFACES 0 1
END
SECTION: COGS
WORLD COGS 1
0: defaults.cog
END
)JKL";
        std::istringstream input(jkl);
        Level imported = assets::loadFromJKL(input);
        const auto exactPendingImport = [](const Level& level)
        {
            return level.importedJklCogRowsPresent
                && level.importedJklSurfaceRefs.size() == 1u
                && level.importedJklSurfaceRefs[0] == SurfaceRef{ 0, 0 }
                && level.cogs.size() == 1u
                && level.cogs[0].name == "defaults.cog"
                && level.cogs[0].values.empty();
        };

        const std::string unique = std::to_string(
            std::chrono::steady_clock::now().time_since_epoch().count());
        const std::filesystem::path root =
            std::filesystem::temp_directory_path()
            / ("sed_pending_cog_import_" + unique);
        std::error_code ec{};
        std::filesystem::create_directories(root, ec);
        const std::filesystem::path nativePath = root / "pending.sed";
        bool nativeRoundTrip = !ec && exactPendingImport(imported);
        try
        {
            for (int pass = 0; pass < 2 && nativeRoundTrip; ++pass)
            {
                assets::saveToSED(imported, nativePath);
                imported = assets::loadFromSED(nativePath);
                nativeRoundTrip = exactPendingImport(imported);
            }
        }
        catch (const std::exception&)
        {
            nativeRoundTrip = false;
        }
        check(nativeRoundTrip,
            "SED JKL COG hydration: two native Save/Open cycles retain the explicit pending-row marker and exact GEORESOURCE surface map");

        bool missingResourceRetry = false;
        try
        {
            assets::ResourceLocator unavailable{};
            assets::finalizeJklImportResources(imported, unavailable);
            missingResourceRetry = exactPendingImport(imported);

            std::filesystem::create_directories(root / "cog", ec);
            std::ofstream script(root / "cog" / "defaults.cog");
            script << "symbols\n"
                << "    int count=0\n"
                << "    sector target\n"
                << "    vector direction\n"
                << "    wav sound\n"
                << "end\n";
            script.close();
            const bool scriptWritten = !script.fail() && !script.is_open();
            assets::ResourceLocator available{};
            available.addRoot(root);
            assets::finalizeJklImportResources(imported, available);
            const Cog& cog = imported.cogs[0];
            missingResourceRetry = missingResourceRetry
                && !imported.importedJklCogRowsPresent
                && imported.importedJklSurfaceRefs.empty()
                && cog.values.size() == 4u
                && cog.values[0].name == "count"
                && cog.values[0].type == ValueType::integer
                && cog.values[0].intValue == 0
                && cog.values[1].name == "target"
                && cog.values[1].cogType == CogType::sec
                && cog.values[1].type == ValueType::ptr
                && cog.values[1].intValue == -1
                && cog.values[2].name == "direction"
                && cog.values[2].type == ValueType::vect
                && cog.values[2].stringValue == "(0/0/0)"
                && cog.values[3].name == "sound"
                && cog.values[3].cogType == CogType::wav
                && cog.values[3].type == ValueType::str
                && cog.values[3].stringValue.empty()
                && scriptWritten;
        }
        catch (const std::exception&)
        {
            missingResourceRetry = false;
        }
        check(missingResourceRetry,
            "SED JKL COG hydration: an unavailable script remains retryable and a later script supplies exact typed defaults without changing the import map early");

        Level unusedMap = imported;
        unusedMap.cogs.clear();
        unusedMap.importedJklCogRowsPresent = false;
        unusedMap.importedJklSurfaceRefs = { SurfaceRef{ 0, 0 } };
        Level emptyImportState = unusedMap;
        emptyImportState.importedJklSurfaceRefs.clear();
        bool unusedMapIgnored = false;
        try
        {
            const std::vector<std::byte> withUnusedMap =
                assets::saveToSedBytes(unusedMap);
            const std::vector<std::byte> withoutMap =
                assets::saveToSedBytes(emptyImportState);
            const Level loaded = assets::loadFromSedBytes(withUnusedMap);
            unusedMapIgnored = withUnusedMap == withoutMap
                && assets::sedLevelsEqual(unusedMap, emptyImportState)
                && assets::estimateSedLevelDecodedFootprint(unusedMap)
                    == assets::estimateSedLevelDecodedFootprint(emptyImportState)
                && !loaded.importedJklCogRowsPresent
                && loaded.importedJklSurfaceRefs.empty()
                && unusedMap.importedJklSurfaceRefs.size() == 1u;
        }
        catch (const std::exception&)
        {
            unusedMapIgnored = false;
        }
        check(unusedMapIgnored,
            "SED JKL COG hydration: a false marker with no pending COG values treats the parser's unused surface map as empty and does not churn native bytes");
        std::filesystem::remove_all(root, ec);
    }

    {
        const char* const zeroGeometryJkl = R"JKL(SECTION: HEADER
VERSION 1
END
SECTION: GEORESOURCE
WORLD COLORMAPS 0
WORLD VERTICES 0
WORLD TEXTURE VERTICES 0
WORLD ADJOINS 0
WORLD SURFACES 0
END
SECTION: SECTORS
WORLD SECTORS 0
END
SECTION: COGS
WORLD COGS 1
0: defaults.cog
END
)JKL";
        std::istringstream input(zeroGeometryJkl);
        Level imported = assets::loadFromJKL(input);
        const auto pendingWithoutGeometry = [](const Level& level)
        {
            return level.sectors.empty()
                && level.importedJklCogRowsPresent
                && level.importedJklSurfaceRefs.empty()
                && level.cogs.size() == 1u
                && level.cogs[0].name == "defaults.cog"
                && level.cogs[0].values.empty();
        };
        const std::string unique = std::to_string(
            std::chrono::steady_clock::now().time_since_epoch().count());
        const std::filesystem::path root = std::filesystem::temp_directory_path()
            / ("sed_zero_geometry_cog_import_" + unique);
        std::error_code ec{};
        std::filesystem::create_directories(root, ec);
        const std::filesystem::path nativePath = root / "zero.sed";
        bool markerRoundTrips = !ec && pendingWithoutGeometry(imported);
        try
        {
            for (int pass = 0; pass < 2 && markerRoundTrips; ++pass)
            {
                assets::saveToSED(imported, nativePath);
                imported = assets::loadFromSED(nativePath);
                markerRoundTrips = pendingWithoutGeometry(imported);
            }
        }
        catch (const std::exception&)
        {
            markerRoundTrips = false;
        }

        bool laterScriptHydratesDefaults = false;
        try
        {
            assets::ResourceLocator unavailable{};
            assets::finalizeJklImportResources(imported, unavailable);
            const bool missingScriptPreserved = pendingWithoutGeometry(imported);
            std::filesystem::create_directories(root / "cog", ec);
            std::ofstream script(root / "cog" / "defaults.cog");
            script << "symbols\n"
                << "    int count=0\n"
                << "    sector target\n"
                << "    vector direction\n"
                << "    wav sound\n"
                << "end\n";
            script.close();
            const bool scriptWritten = !script.fail() && !script.is_open();
            assets::ResourceLocator available{};
            available.addRoot(root);
            assets::finalizeJklImportResources(imported, available);
            const Cog& cog = imported.cogs[0];
            laterScriptHydratesDefaults = missingScriptPreserved && scriptWritten
                && !imported.importedJklCogRowsPresent
                && imported.importedJklSurfaceRefs.empty()
                && cog.values.size() == 4u
                && cog.values[0].name == "count"
                && cog.values[0].type == ValueType::integer
                && cog.values[0].intValue == 0
                && cog.values[1].name == "target"
                && cog.values[1].cogType == CogType::sec
                && cog.values[1].type == ValueType::ptr
                && cog.values[1].intValue == -1
                && cog.values[2].name == "direction"
                && cog.values[2].type == ValueType::vect
                && cog.values[2].stringValue == "(0/0/0)"
                && cog.values[3].name == "sound"
                && cog.values[3].cogType == CogType::wav
                && cog.values[3].type == ValueType::str
                && cog.values[3].stringValue.empty();
        }
        catch (const std::exception&)
        {
            laterScriptHydratesDefaults = false;
        }
        check(markerRoundTrips,
            "SED JKL COG hydration: zero-geometry import keeps the explicit empty-map marker across two native Save/Open cycles");
        check(laterScriptHydratesDefaults,
            "SED JKL COG hydration: zero-geometry missing-script retry later supplies exact typed defaults");
        std::filesystem::remove_all(root, ec);
    }

    {
        Level pending = richLevel(ProjectType::IJIM);
        pending.importedJklCogRowsPresent = true;
        pending.importedJklSurfaceRefs = { SurfaceRef{ 0, 0 } };
        std::vector<std::byte> encoded{};
        bool writerEmitsJimp = false;
        try
        {
            encoded = assets::saveToSedBytes(pending);
            const SectionLayout section = findSection(encoded,
                { 'j', 'i', 'm', 'p' });
            writerEmitsJimp = section.descriptorOffset != 0u
                && section.parityStreams == 4u
                && section.logicalStoredSize == 17u;
        }
        catch (const std::exception&)
        {
            writerEmitsJimp = false;
        }

        const auto malformedJimpRejected = [&](const auto& mutate)
        {
            if ( !writerEmitsJimp ) return false;
            std::vector<std::byte> damaged = encoded;
            const SectionLayout section = findSection(damaged,
                { 'j', 'i', 'm', 'p' });
            std::vector<std::byte> payload = logicalStoredBytes(damaged, section);
            if ( payload.size() != 17u ) return false;
            mutate(payload);
            resealProtectedSectionPayload(damaged, section, payload);
            try
            {
                (void)assets::loadFromSedBytes(damaged);
            }
            catch (const std::exception&)
            {
                return true;
            }
            return false;
        };
        const bool unsupportedVersion = malformedJimpRejected(
            [](std::vector<std::byte>& payload)
            {
                writeU32Le(payload, 0u, 2u);
            });
        const bool nonCanonicalBoolean = malformedJimpRejected(
            [](std::vector<std::byte>& payload)
            {
                payload[4] = std::byte{2};
            });
        const bool excessiveReferenceCount = malformedJimpRejected(
            [](std::vector<std::byte>& payload)
            {
                writeU32Le(payload, 5u, std::numeric_limits<std::uint32_t>::max());
            });
        const bool outOfRangeReference = malformedJimpRejected(
            [](std::vector<std::byte>& payload)
            {
                writeU32Le(payload, 13u, 99u);
            });

        bool permissiveJimpLossIsOptional = false;
        bool permissiveInvalidJimpIsOptional = false;
        if ( writerEmitsJimp )
        {
            std::vector<std::byte> invalidReference = encoded;
            const SectionLayout jimpSection = findSection(
                invalidReference, { 'j', 'i', 'm', 'p' });
            std::vector<std::byte> invalidReferencePayload =
                logicalStoredBytes(invalidReference, jimpSection);
            writeU32Le(invalidReferencePayload, 13u, 99u);
            resealProtectedSectionPayload(
                invalidReference, jimpSection, invalidReferencePayload);
            try
            {
                const assets::SedRecoveryLoadResult recovered =
                    assets::recoverFromSedBytes(invalidReference);
                Level expected = pending;
                expected.importedJklCogRowsPresent = false;
                expected.importedJklSurfaceRefs.clear();
                permissiveInvalidJimpIsOptional = recovered.hadCorruption
                    && recovered.partialRecoveryFromCorruption
                    && std::find(recovered.missingOptionalSections.begin(),
                        recovered.missingOptionalSections.end(), "jimp")
                        != recovered.missingOptionalSections.end()
                    && recovered.missingRequiredSections.empty()
                    && !recovered.level.importedJklCogRowsPresent
                    && recovered.level.importedJklSurfaceRefs.empty()
                    && assets::sedLevelsEqual(recovered.level, expected);
            }
            catch (const std::exception&)
            {
                permissiveInvalidJimpIsOptional = false;
            }

            const SectionLayout physicalJimpSection = findSection(
                encoded, { 'j', 'i', 'm', 'p' });
            const std::array<std::size_t, 3> repairableJimpParityStreams{
                physicalJimpSection.dataStreams,
                physicalJimpSection.dataStreams + 1u,
                physicalJimpSection.dataStreams + 2u };
            std::vector<std::byte> repairableJimp = encoded;
            repairableJimp[slotOffset(physicalJimpSection, 0u, 0u)]
                ^= std::byte{ 0x41 };
            damageSlots(repairableJimp, physicalJimpSection,
                repairableJimpParityStreams, 0u);
            bool fourJimpErasuresRepair = false;
            try
            {
                const assets::SedFile repaired =
                    assets::loadSedFileBytes(repairableJimp);
                fourJimpErasuresRepair = repaired.repairedChunks == 4u
                    && repaired.level.importedJklCogRowsPresent
                    && repaired.level.importedJklSurfaceRefs
                        == pending.importedJklSurfaceRefs
                    && std::find(repaired.repairedSections.begin(),
                        repaired.repairedSections.end(), "jimp")
                        != repaired.repairedSections.end();
            }
            catch (const std::exception&)
            {
                fourJimpErasuresRepair = false;
            }

            const std::array<std::size_t, 4> unrecoverableJimpParityStreams{
                physicalJimpSection.dataStreams,
                physicalJimpSection.dataStreams + 1u,
                physicalJimpSection.dataStreams + 2u,
                physicalJimpSection.dataStreams + 3u };
            std::vector<std::byte> damaged = encoded;
            damaged[slotOffset(physicalJimpSection, 0u, 0u)]
                ^= std::byte{ 0x41 };
            damageSlots(damaged, physicalJimpSection,
                unrecoverableJimpParityStreams, 0u);
            try
            {
                const assets::SedRecoveryLoadResult recovered =
                    assets::recoverFromSedBytes(damaged);
                Level expected = pending;
                expected.importedJklCogRowsPresent = false;
                expected.importedJklSurfaceRefs.clear();
                permissiveJimpLossIsOptional = recovered.hadCorruption
                    && recovered.partialRecoveryFromCorruption
                    && std::find(recovered.missingOptionalSections.begin(),
                        recovered.missingOptionalSections.end(), "jimp")
                        != recovered.missingOptionalSections.end()
                    && recovered.missingRequiredSections.empty()
                    && !recovered.level.importedJklCogRowsPresent
                    && recovered.level.importedJklSurfaceRefs.empty()
                    && assets::sedLevelsEqual(recovered.level, expected);
            }
            catch (const std::exception&)
            {
                permissiveJimpLossIsOptional = false;
            }
            check(fourJimpErasuresRepair,
                "SED JKL COG hydration: four occupied jimp erasures remain repairable");
        }

        bool duplicateTagRejected = false;
        if ( writerEmitsJimp )
        {
            try
            {
                const SectionLayout originalJimp = findSection(encoded,
                    { 'j', 'i', 'm', 'p' });
                assets::SedOptionalSection duplicateEdit{};
                duplicateEdit.type = { 'e', 'd', 'i', 't' };
                duplicateEdit.payload = logicalStoredBytes(encoded, originalJimp);
                std::vector<std::byte> duplicate = assets::saveToSedBytes(
                    pending, std::span<const assets::SedOptionalSection>(
                        &duplicateEdit, 1u));
                const SectionLayout section = findSection(duplicate,
                    duplicateEdit.type);
                if ( section.parityStreams != 4u
                    || duplicateEdit.payload.size() != 17u
                    || readU32Le(duplicateEdit.payload, 0u) != 1u
                    || logicalStoredBytes(duplicate, section) != duplicateEdit.payload )
                {
                    throw std::runtime_error(
                        "The EDIT p4 control does not contain the well-formed JIMP V1 payload.");
                }
                renameProtectedSectionType(duplicate, section, { 'j', 'i', 'm', 'p' });
                (void)assets::loadFromSedBytes(duplicate);
            }
            catch (const std::exception& exception)
            {
                duplicateTagRejected = std::string_view(exception.what())
                    == "Invalid binary SED file: duplicate section";
            }
        }

        assets::SedOptionalSection reservedJimp{};
        reservedJimp.type = { 'j', 'i', 'm', 'p' };
        reservedJimp.payload = { std::byte{0x01} };
        bool userJimpRejected = false;
        try
        {
            (void)assets::saveToSedBytes(pending,
                std::span<const assets::SedOptionalSection>(&reservedJimp, 1u));
        }
        catch (const std::exception&)
        {
            userJimpRejected = true;
        }

        Level ordinaryRawCog = richLevel(ProjectType::IJIM);
        Cog rawCog{};
        rawCog.id = 701;
        rawCog.num = 17;
        rawCog.name = "ordinary.cog";
        CogValue rawParameter{};
        rawParameter.name = "param0";
        rawParameter.type = ValueType::str;
        rawParameter.cogType = CogType::unk;
        rawParameter.stringValue = "authored";
        rawCog.values.push_back(rawParameter);
        ordinaryRawCog.cogs = { rawCog };
        ordinaryRawCog.importedJklCogRowsPresent = false;
        ordinaryRawCog.importedJklSurfaceRefs.clear();
        bool ordinaryRawCogHasNoJimp = false;
        try
        {
            const std::vector<std::byte> ordinaryBytes =
                assets::saveToSedBytes(ordinaryRawCog);
            const Level loaded = assets::loadFromSedBytes(ordinaryBytes);
            ordinaryRawCogHasNoJimp = readU32Le(ordinaryBytes, 12u) == 8u
                && !loaded.importedJklCogRowsPresent
                && loaded.importedJklSurfaceRefs.empty()
                && loaded.cogs.size() == 1u
                && loaded.cogs[0].values.size() == 1u
                && loaded.cogs[0].values[0].name == "param0"
                && loaded.cogs[0].values[0].stringValue == "authored";
        }
        catch (const std::exception&)
        {
            ordinaryRawCogHasNoJimp = false;
        }
        check(writerEmitsJimp,
            "SED JKL COG hydration: ProtectedV1 writer emits one core-owned jimp section");
        check(unsupportedVersion,
            "SED JKL COG hydration: strict loading rejects an unsupported jimp version");
        check(nonCanonicalBoolean,
            "SED JKL COG hydration: strict loading rejects a noncanonical jimp Boolean");
        check(excessiveReferenceCount,
            "SED JKL COG hydration: strict loading rejects a jimp reference count above the bounded collection limit");
        check(outOfRangeReference,
            "SED JKL COG hydration: strict loading rejects a jimp geometry reference outside the Level");
        check(duplicateTagRejected,
            "SED JKL COG hydration: strict loading rejects duplicate reserved jimp sections");
        check(userJimpRejected,
            "SED JKL COG hydration: public writers reject a caller-supplied reserved jimp section");
        check(ordinaryRawCogHasNoJimp,
            "SED JKL COG hydration: authored raw COG rows without import provenance keep ordinary native bytes");

        Level leadingZeroRawCog = ordinaryRawCog;
        leadingZeroRawCog.cogs[0].values[0].name = "param00";
        leadingZeroRawCog.importedJklSurfaceRefs = { SurfaceRef{ 0, 0 } };
        bool leadingZeroRawCogDoesNotRepresentImport = false;
        try
        {
            const std::vector<std::byte> leadingZeroBytes =
                assets::saveToSedBytes(leadingZeroRawCog);
            const Level loaded = assets::loadFromSedBytes(leadingZeroBytes);
            leadingZeroRawCogDoesNotRepresentImport =
                readU32Le(leadingZeroBytes, 12u) == 8u
                && !loaded.importedJklCogRowsPresent
                && loaded.importedJklSurfaceRefs.empty()
                && loaded.cogs.size() == 1u
                && loaded.cogs[0].values.size() == 1u
                && loaded.cogs[0].values[0].name == "param00";
        }
        catch (const std::exception&)
        {
            leadingZeroRawCogDoesNotRepresentImport = false;
        }
        check(leadingZeroRawCogDoesNotRepresentImport,
            "SED JKL COG hydration: noncanonical param00 does not make an unused import map meaningful");
        check(permissiveJimpLossIsOptional,
            "SED JKL COG hydration: permissive recovery reports unrecoverable jimp as optional loss and keeps required Level");
        check(permissiveInvalidJimpIsOptional,
            "SED JKL COG hydration: permissive recovery drops integrity-valid jimp with an out-of-range ref and keeps required Level");
    }

    {
        const auto appendThingFlags = [](Thing& thing, const std::int32_t flags)
        {
            TplValue value{};
            value.name = "thingflags";
            value.type = ValueType::integer;
            value.intValue = flags;
            thing.values.push_back(std::move(value));
        };

        Level dormant = richLevel(ProjectType::MOTS);
        dormant.uvCoordinateSpace = UvCoordinateSpace::Normalized;
        dormant.things[0].archLightingIndex = 0;
        MotsArchLightObject custom{};
        custom.wireIndex = 7;
        MotsArchLightMesh customMesh{};
        customMesh.wireIndex = 11;
        customMesh.vertices = {
            MotsArchLightVertex{ 0.2f, 0.3f, 0.4f, 0.5f },
        };
        custom.meshes.push_back(customMesh);
        dormant.motsArchLighting.push_back(custom);
        appendThingFlags(dormant.things[0], 0);
        // An explicit local cleared Arch flag makes this custom record
        // unambiguously dormant; SED retains it for later reactivation.
        const std::vector<std::byte> dormantBytes = assets::saveToSedBytes(dormant);
        bool dormantRoundTrip = false;
        try
        {
            Level loaded = assets::loadFromSedBytes(dormantBytes);
            dormantRoundTrip = loaded.kind == ProjectType::MOTS
                && loaded.things[0].archLightingIndex == 0
                && loaded.motsArchLighting == dormant.motsArchLighting
                && level::thingFlags(loaded, loaded.things[0]) == 0;
            if (dormantRoundTrip)
            {
                appendThingFlags(loaded.things[0], 0x8);
                dormantRoundTrip = level::isMotsArchThing(
                        loaded, loaded.things[0])
                    && level::findActiveMotsArchLighting(
                        loaded, loaded.things[0]) != nullptr
                    && *level::findActiveMotsArchLighting(
                        loaded, loaded.things[0]) == custom;
            }
        }
        catch (const std::exception&)
        {
            dormantRoundTrip = false;
        }
        check(dormantRoundTrip,
            "SED MotS ARCHLIGHTING: nonzero dormant data and Thing reference persist, then become active when the Arch flag is restored");

        Level externalOnly{};
        externalOnly.kind = ProjectType::MOTS;
        Thing& externalThing = externalOnly.newThing();
        externalThing.templateName = "+external_arch";
        externalThing.archLightingIndex = 0;
        MotsArchLightObject externalDefault{};
        externalDefault.wireIndex = 0;
        MotsArchLightMesh externalMesh{};
        externalMesh.wireIndex = 0;
        externalMesh.vertices.resize(1, MotsArchLightVertex{});
        externalDefault.meshes.push_back(externalMesh);
        externalOnly.motsArchLighting.push_back(externalDefault);
        assets::Templates externalTemplates{};
        ThingTemplate externalTemplate{};
        externalTemplate.name = "+external_arch";
        TplValue externalFlags{};
        externalFlags.name = "thingflags";
        externalFlags.type = ValueType::integer;
        externalFlags.intValue = 0x8;
        externalTemplate.values.push_back(externalFlags);
        (void)externalTemplates.addOrUpdate(std::move(externalTemplate), true);
        const std::vector<std::byte> externalOnlyBytes =
            assets::saveToSedBytes(externalOnly);
        bool externalOnlyPreserved =
            externalOnlyBytes.size() >= 8u
            && externalOnlyBytes[4] == std::byte{ 3 }
            && level::thingFlags(
                externalOnly, &externalTemplates, externalOnly.things[0]) == 0x8;
        try
        {
            const Level loaded = assets::loadFromSedBytes(externalOnlyBytes);
            externalOnlyPreserved = externalOnlyPreserved
                && loaded.things[0].archLightingIndex == 0
                && loaded.motsArchLighting == externalOnly.motsArchLighting
                && level::thingFlags(
                    loaded, &externalTemplates, loaded.things[0]) == 0x8;
        }
        catch (const std::exception&)
        {
            externalOnlyPreserved = false;
        }
        check(externalOnlyPreserved,
            "SED MotS ARCHLIGHTING: zero binding remains when the Arch flag is supplied only by an external template database");

        Level zeroDormant = richLevel(ProjectType::MOTS);
        zeroDormant.uvCoordinateSpace = UvCoordinateSpace::Normalized;
        zeroDormant.things[0].archLightingIndex = 0;
        MotsArchLightObject zeroObject{};
        zeroObject.wireIndex = 9;
        MotsArchLightMesh zeroMesh{};
        zeroMesh.wireIndex = 13;
        zeroMesh.vertices.resize(1, MotsArchLightVertex{});
        zeroObject.meshes.push_back(zeroMesh);
        zeroDormant.motsArchLighting.push_back(zeroObject);
        appendThingFlags(zeroDormant.things[0], 0);
        const std::vector<std::byte> zeroDormantBytes =
            assets::saveToSedBytes(zeroDormant);
        bool zeroDormantDropped = zeroDormantBytes.size() >= 8u
            && zeroDormantBytes[4] == std::byte{ 2 }
            && zeroDormant.motsArchLighting.size() == 1
            && zeroDormant.things[0].archLightingIndex == 0;
        try
        {
            const Level loaded = assets::loadFromSedBytes(zeroDormantBytes);
            zeroDormantDropped = zeroDormantDropped
                && loaded.motsArchLighting.empty()
                && loaded.things[0].archLightingIndex == -1;
        }
        catch (const std::exception&)
        {
            zeroDormantDropped = false;
        }
        check(zeroDormantDropped,
            "SED MotS ARCHLIGHTING: zero-valued dormant records are omitted without mutating the editor Level");

        Level activeZero = zeroDormant;
        appendThingFlags(activeZero.things[0], 0x8);
        const std::vector<std::byte> activeZeroBytes = assets::saveToSedBytes(activeZero);
        bool activeZeroRoundTrip = activeZeroBytes.size() >= 8u
            && activeZeroBytes[4] == std::byte{ 3 };
        try
        {
            const Level loaded = assets::loadFromSedBytes(activeZeroBytes);
            activeZeroRoundTrip = activeZeroRoundTrip
                && loaded.motsArchLighting == activeZero.motsArchLighting
                && loaded.things[0].archLightingIndex == 0
                && level::isMotsArchThing(loaded, loaded.things[0]);
        }
        catch (const std::exception&)
        {
            activeZeroRoundTrip = false;
        }
        check(activeZeroRoundTrip,
            "SED MotS ARCHLIGHTING: an active Arch Thing keeps its bound all-zero default record");
    }

    {
        // MotS architectural lighting is a native editor domain. A populated
        // table selects the new binary version while preserving the legacy V1
        // and V2 layout for levels that have no table.
        Level normalized = richLevel(ProjectType::MOTS);
        normalized.uvCoordinateSpace = UvCoordinateSpace::Normalized;
        normalized.things[0].archLightingIndex = 0;
        TplValue archFlags{};
        archFlags.name = "thingflags";
        archFlags.type = ValueType::integer;
        archFlags.intValue = 0x8;
        normalized.things[0].values.push_back(archFlags);
        MotsArchLightObject object{};
        object.wireIndex = 7;
        MotsArchLightMesh mesh{};
        mesh.wireIndex = 11;
        mesh.vertices = {
            MotsArchLightVertex{ -0.0f, -2.5f, 10.5f, 0.75f },
            MotsArchLightVertex{ 1.0f, 0.25f, 0.5f, 3.0f },
        };
        object.meshes.push_back(mesh);
        normalized.motsArchLighting.push_back(object);

        const std::vector<std::byte> normalizedBytes =
            assets::saveToSedBytes(normalized);
        check(normalizedBytes.size() >= 8u
                && normalizedBytes[4] == std::byte{ 3 }
                && normalizedBytes[5] == std::byte{ 0 }
                && normalizedBytes[6] == std::byte{ 0 }
                && normalizedBytes[7] == std::byte{ 0 },
            "SED MotS ARCHLIGHTING: normalized table selects binary V3");
        bool normalizedLoaded = false;
        try
        {
            const Level loaded = assets::loadFromSedBytes(normalizedBytes);
            normalizedLoaded = loaded.kind == ProjectType::MOTS
                && loaded.uvCoordinateSpace == UvCoordinateSpace::Normalized
                && loaded.things.size() == normalized.things.size()
                && loaded.things[0].archLightingIndex == 0
                && loaded.motsArchLighting == normalized.motsArchLighting
                && std::bit_cast<std::uint32_t>(
                    loaded.motsArchLighting[0].meshes[0].vertices[0].mono)
                    == 0x8000'0000u;
        }
        catch (const std::exception&)
        {
            normalizedLoaded = false;
        }
        check(normalizedLoaded,
            "SED MotS ARCHLIGHTING: normalized table round-trips exact values and Thing reference");
        bool normalizedBytesStable = false;
        try
        {
            normalizedBytesStable = assets::saveToSedBytes(
                assets::loadFromSedBytes(normalizedBytes)) == normalizedBytes;
        }
        catch (const std::exception&) {}
        check(normalizedBytesStable,
            "SED MotS ARCHLIGHTING: signed-zero table bits remain stable on re-save");

        Level pixels = normalized;
        pixels.uvCoordinateSpace = UvCoordinateSpace::TexturePixels;
        const std::vector<std::byte> pixelBytes = assets::saveToSedBytes(pixels);
        check(pixelBytes.size() >= 8u
                && pixelBytes[4] == std::byte{ 4 }
                && pixelBytes[5] == std::byte{ 0 }
                && pixelBytes[6] == std::byte{ 0 }
                && pixelBytes[7] == std::byte{ 0 },
            "SED MotS ARCHLIGHTING: pixel table selects binary V4");
        bool pixelsLoaded = false;
        try
        {
            const Level loaded = assets::loadFromSedBytes(pixelBytes);
            pixelsLoaded = loaded.kind == ProjectType::MOTS
                && loaded.uvCoordinateSpace == UvCoordinateSpace::TexturePixels
                && loaded.things.size() == pixels.things.size()
                && loaded.things[0].archLightingIndex == 0
                && loaded.motsArchLighting == pixels.motsArchLighting;
        }
        catch (const std::exception&)
        {
            pixelsLoaded = false;
        }
        check(pixelsLoaded,
            "SED MotS ARCHLIGHTING: pixel table round-trips exact values and Thing reference");

        Level changedTable = normalized;
        changedTable.motsArchLighting[0].meshes[0].vertices[1].blue = 3.5f;
        check(!assets::sedLevelsEqual(normalized, changedTable),
            "SED MotS ARCHLIGHTING: semantic equality notices table-only edits");

        const std::vector<std::byte> historyBefore = assets::saveToSedBytes(
            normalized, {}, assets::SedSerializationProfile::HistoryNested);
        const std::vector<std::byte> historyAfter = assets::saveToSedBytes(
            changedTable, {}, assets::SedSerializationProfile::HistoryNested);
        bool historyLoaded = false;
        try
        {
            assets::SedNestedHistoryDecoder decoder(normalized);
            decoder.advance(historyBefore);
            decoder.advance(historyAfter);
            historyLoaded = decoder.currentLevel().motsArchLighting
                    == changedTable.motsArchLighting
                && decoder.currentLevel().things[0].archLightingIndex == 0;
        }
        catch (const std::exception&)
        {
            historyLoaded = false;
        }
        check(historyLoaded,
            "SED MotS ARCHLIGHTING: HistoryNested transition preserves table-only edits");

        bool tableChangeRequiresFullThingRefresh = false;
        try
        {
            assets::SedNestedHistoryDecoder deltaDecoder(normalized);
            deltaDecoder.advance(historyBefore);
            deltaDecoder.advance(historyAfter);
            const auto& changedThings =
                deltaDecoder.lastChangedThingIndices();
            // A table-only edit cannot be represented by a pose/index subset:
            // the renderer must refresh the complete Things domain even when
            // every Thing row is byte-identical.
            tableChangeRequiresFullThingRefresh = !changedThings.has_value();
        }
        catch (const std::exception&)
        {
            tableChangeRequiresFullThingRefresh = false;
        }
        check(tableChangeRequiresFullThingRefresh,
            "SED MotS ARCHLIGHTING: table-only history edits force a full Things refresh");

        bool recoveryLoaded = false;
        try
        {
            const assets::SedRecoveryLoadResult recovered =
                assets::recoverFromSedBytes(normalizedBytes);
            recoveryLoaded = recovered.level.motsArchLighting
                    == normalized.motsArchLighting
                && recovered.level.things[0].archLightingIndex == 0;
        }
        catch (const std::exception&)
        {
            recoveryLoaded = false;
        }
        check(recoveryLoaded,
            "SED MotS ARCHLIGHTING: recovery preserves table and Thing reference");

        // Version transitions keep the UV coordinate space but change only
        // the Things payload. The incremental reader must disable stale frame
        // reuse and restore both directions without losing the table.
        Level normalizedWithoutArch = richLevel(ProjectType::MOTS);
        normalizedWithoutArch.uvCoordinateSpace = UvCoordinateSpace::Normalized;
        const std::vector<std::byte> normalizedWithoutArchBytes =
            assets::saveToSedBytes(normalizedWithoutArch, {},
                assets::SedSerializationProfile::HistoryNested);
        bool versionTransitionLoaded = false;
        bool reverseVersionTransitionLoaded = false;
        try
        {
            assets::SedNestedHistoryDecoder forward(normalizedWithoutArch);
            forward.advance(normalizedWithoutArchBytes);
            forward.advance(historyAfter);
            versionTransitionLoaded = forward.currentLevel().motsArchLighting
                    == changedTable.motsArchLighting
                && forward.currentLevel().things[0].archLightingIndex == 0;

            assets::SedNestedHistoryDecoder reverse(normalized);
            reverse.advance(historyAfter);
            reverse.advance(normalizedWithoutArchBytes);
            reverseVersionTransitionLoaded =
                reverse.currentLevel().motsArchLighting.empty()
                && std::all_of(reverse.currentLevel().things.begin(),
                    reverse.currentLevel().things.end(),
                    [](const Thing& thing)
                    {
                        return thing.archLightingIndex == -1;
                    });
        }
        catch (const std::exception&) {}
        check(versionTransitionLoaded && reverseVersionTransitionLoaded,
            "SED MotS ARCHLIGHTING: HistoryNested V2/V3 transitions preserve and remove the table atomically");

        // Protected top-level repair must restore the complete Things domain,
        // including its table, while beyond-parity loss must not leave an
        // orphan table or dangling Thing reference in the salvaged Level.
        const SectionLayout thingsSection = findSection(
            normalizedBytes, { 't', 'h', 'n', 'g' });
        std::vector<std::byte> repairedBytes = normalizedBytes;
        damageFirstSlots(repairedBytes, thingsSection, 1);
        bool repairedArchLoaded = false;
        try
        {
            const assets::SedRecoveryLoadResult repaired =
                assets::recoverFromSedBytes(repairedBytes);
            repairedArchLoaded = repaired.level.motsArchLighting
                    == normalized.motsArchLighting
                && repaired.level.things.size() == normalized.things.size()
                && !repaired.level.things.empty()
                && repaired.level.things[0].archLightingIndex == 0;
        }
        catch (const std::exception&) {}
        check(repairedArchLoaded,
            "SED MotS ARCHLIGHTING: protected Things repair restores table and references");

        std::vector<std::byte> lostThingsBytes = normalizedBytes;
        damageFirstSlots(lostThingsBytes, thingsSection, 5);
        bool lostThingsSafe = false;
        try
        {
            const assets::SedRecoveryLoadResult recovered =
                assets::recoverFromSedBytes(lostThingsBytes);
            lostThingsSafe = recovered.partialRecoveryFromCorruption
                && recovered.level.things.empty()
                && recovered.level.motsArchLighting.empty();
        }
        catch (const std::exception&) {}
        check(lostThingsSafe,
            "SED MotS ARCHLIGHTING: unrecoverable Things salvage drops table and references together");

        // A late checked-frame failure must roll back an already decoded
        // ARCHLIGHTING transition and retain the last accepted bytes.
        assets::SedNestedHistoryDecoder rollback(normalized);
        rollback.advance(historyAfter);
        const std::vector<std::byte> acceptedArchBytes(
            rollback.currentBytes().begin(), rollback.currentBytes().end());
        // Use the preceding table state so the decoder must stage a real
        // ARCHLIGHTING change before the later light-frame failure. A test
        // candidate identical to the accepted state would not exercise the
        // rollback boundary.
        std::vector<std::byte> lateFailure = historyBefore;
        const SectionLayout lateLights = findSection(
            lateFailure, { 'l', 'i', 't', 'e' });
        lateFailure[lateLights.payloadOffset] ^= std::byte{ 0x40 };
        bool lateFailureRolledBack = false;
        try { rollback.advance(std::move(lateFailure)); }
        catch (const std::exception&) { lateFailureRolledBack = true; }
        lateFailureRolledBack = lateFailureRolledBack
            && rollback.currentLevel().motsArchLighting
                == changedTable.motsArchLighting
            && rollback.currentLevel().things[0].archLightingIndex == 0
            && std::equal(acceptedArchBytes.begin(), acceptedArchBytes.end(),
                rollback.currentBytes().begin(), rollback.currentBytes().end());
        check(lateFailureRolledBack,
            "SED MotS ARCHLIGHTING: late HistoryNested failure rolls back table state");
    }

    {
        const auto appendThingFlags = [](
            Thing& thing,
            const std::int32_t flags)
        {
            TplValue value{};
            value.name = "thingflags";
            value.type = ValueType::integer;
            value.intValue = flags;
            thing.values.push_back(std::move(value));
        };
        const auto zeroRecord = [](const std::int32_t wireIndex)
        {
            MotsArchLightObject object{};
            object.wireIndex = wireIndex;
            object.meshes.push_back(MotsArchLightMesh{
                0, { MotsArchLightVertex{} } });
            return object;
        };

        // The expected Level has two dormant all-zero table entries around
        // one retained record. The writer must drop both zero entries and
        // remap the active Thing reference from 1 to 0.
        Level authored = richLevel(ProjectType::MOTS);
        appendThingFlags(authored.things.front(), 0x8);
        authored.things.front().archLightingIndex = 1;
        Thing dormant = authored.things.front();
        dormant.id += 1;
        dormant.num += 1;
        dormant.name = "dormant_zero_arch_binding";
        dormant.archLightingIndex = 0;
        dormant.values.back().intValue = 0;
        authored.things.push_back(dormant);
        MotsArchLightObject retained{};
        retained.wireIndex = 1;
        retained.meshes.push_back(MotsArchLightMesh{
            0, { MotsArchLightVertex{ 0.25f, 0.5f, 0.75f, 1.0f } } });
        authored.motsArchLighting = {
            zeroRecord(0), retained, zeroRecord(2) };
        const Level canonical = assets::loadFromSedBytes(
            assets::saveToSedBytes(authored));
        check(canonical.motsArchLighting.size() == 1u
                && canonical.things.front().archLightingIndex == 0
                && canonical.things.back().archLightingIndex == -1
                && assets::sedLevelNativeSerializationMatches(authored, canonical),
            "SED native serialization equality accepts only the canonical MotS ARCH filter and remap");

        Level wrongHeader = canonical;
        wrongHeader.header.gravity += 0.5;
        Level wrongThing = canonical;
        wrongThing.things.front().x += 0.5;
        Level wrongThingValue = canonical;
        wrongThingValue.things.front().values.front().floatValue += 0.5;
        Level wrongRetainedRecord = canonical;
        wrongRetainedRecord.motsArchLighting.front()
            .meshes.front().vertices.front().red += 0.5f;
        Level wrongRetainedReference = canonical;
        wrongRetainedReference.things.front().archLightingIndex = -1;
        check(!assets::sedLevelNativeSerializationMatches(authored, wrongHeader)
                && !assets::sedLevelNativeSerializationMatches(authored, wrongThing)
                && !assets::sedLevelNativeSerializationMatches(authored, wrongThingValue)
                && !assets::sedLevelNativeSerializationMatches(authored, wrongRetainedRecord)
                && !assets::sedLevelNativeSerializationMatches(authored, wrongRetainedReference),
            "SED native serialization equality rejects non-ARCH field, Thing, value, retained-record, and reference changes");

        Level forbiddenNonMots = richLevel(ProjectType::JKDF2);
        forbiddenNonMots.things.front().archLightingIndex = 0;
        forbiddenNonMots.motsArchLighting.push_back(retained);
        Level silentlyStrippedNonMots = forbiddenNonMots;
        silentlyStrippedNonMots.things.front().archLightingIndex = -1;
        silentlyStrippedNonMots.motsArchLighting.clear();
        Level forbiddenNonMotsReference = richLevel(ProjectType::JKDF2);
        forbiddenNonMotsReference.things.front().archLightingIndex = 0;
        Level strippedNonMotsReference = forbiddenNonMotsReference;
        strippedNonMotsReference.things.front().archLightingIndex = -1;
        check(!assets::sedLevelNativeSerializationMatches(
                forbiddenNonMots, silentlyStrippedNonMots),
            "SED native serialization equality does not bless stripping forbidden ARCH data from a non-MotS Level");
        check(!assets::sedLevelNativeSerializationMatches(
                forbiddenNonMotsReference, strippedNonMotsReference),
            "SED native serialization equality does not canonicalize a non-MotS Thing ARCH reference when its table is empty");

        MotsArchLightObject original{};
        original.wireIndex = 0;
        original.meshes.push_back(MotsArchLightMesh{
            0, { MotsArchLightVertex{ 0.0f, 0.25f, 0.5f, 0.75f } } });
        Level positiveZeroBaseline = richLevel(ProjectType::MOTS);
        positiveZeroBaseline.things.front().archLightingIndex = 0;
        positiveZeroBaseline.motsArchLighting.push_back(original);
        positiveZeroBaseline.motsArchLightingBakeBaselines.push_back(
            MotsArchLightingBakeBaseline{
                positiveZeroBaseline.things.front().id,
                "actor.3do", 0, original });
        Level negativeZeroBaseline = positiveZeroBaseline;
        negativeZeroBaseline.motsArchLightingBakeBaselines.front()
            .originalArchLighting->meshes.front().vertices.front().mono =
                std::bit_cast<float>(0x8000'0000u);
        const bool strictComparatorsRejectBaselineSignedZero =
            !assets::sedLevelsEqual(positiveZeroBaseline, negativeZeroBaseline)
            && !assets::sedLevelNativeSerializationMatches(
                positiveZeroBaseline, negativeZeroBaseline)
            && !assets::sedLevelsEqualIgnoringSectors(
                positiveZeroBaseline, negativeZeroBaseline)
            && !assets::sedLevelsEqualIgnoringHistoryDomains(
                positiveZeroBaseline, negativeZeroBaseline,
                false, false, false);

        Level bakedPositiveZeroBaseline = positiveZeroBaseline;
        bakedPositiveZeroBaseline.lights.front().flags |= LF_AffectArchThings;
        Level bakedNegativeZeroBaseline = bakedPositiveZeroBaseline;
        bakedNegativeZeroBaseline.motsArchLightingBakeBaselines.front()
            .originalArchLighting->meshes.front().vertices.front().mono =
                std::bit_cast<float>(0x8000'0000u);
        const bool bakeHistoryComparatorRejectsBaselineSignedZero =
            !assets::sedLevelsEqualIgnoringHistoryDomains(
                bakedPositiveZeroBaseline, bakedNegativeZeroBaseline,
                false, true, false);

        bool incrementalHistoryRejectsBaselineSignedZero = false;
        try
        {
            const std::vector<std::byte> positiveBytes = assets::saveToSedBytes(
                positiveZeroBaseline, {},
                assets::SedSerializationProfile::HistoryNested);
            const std::vector<std::byte> negativeBytes = assets::saveToSedBytes(
                negativeZeroBaseline, {},
                assets::SedSerializationProfile::HistoryNested);
            assets::SedNestedHistoryDecoder decoder(positiveZeroBaseline);
            decoder.advance(positiveBytes);
            decoder.advance(negativeBytes);
            incrementalHistoryRejectsBaselineSignedZero =
                !decoder.lastTransitionEqualsIgnoringHistoryDomains(
                    false, false, false)
                && !decoder.lastChangedThingIndices()
                && !decoder.lastChangedSectorIndices();
        }
        catch (const std::exception&)
        {
            incrementalHistoryRejectsBaselineSignedZero = false;
        }
        check(strictComparatorsRejectBaselineSignedZero
                && bakeHistoryComparatorRejectsBaselineSignedZero
                && incrementalHistoryRejectsBaselineSignedZero,
            "SED exact Level and history comparisons preserve signed-zero bits inside ARCH bake provenance");
    }

    {
        // The JKL/NDY writer admits only canonical architectural-light labels
        // and bounded aggregate records.  Native SED must apply the same
        // admission before it writes its first byte; otherwise a malformed
        // native document can be saved successfully and fail on engine
        // export, or leave a partial stream behind after a late rejection.
        const auto nativeArchFixture = []
        {
            Level level = richLevel(ProjectType::MOTS);
            level.uvCoordinateSpace = UvCoordinateSpace::Normalized;
            level.things[0].archLightingIndex = 0;
            TplValue archFlags{};
            archFlags.name = "thingflags";
            archFlags.type = ValueType::integer;
            archFlags.intValue = 0x8;
            level.things[0].values.push_back(archFlags);
            MotsArchLightObject object{};
            object.wireIndex = 7;
            MotsArchLightMesh mesh{};
            mesh.wireIndex = 11;
            mesh.vertices.push_back(MotsArchLightVertex{
                0.25f, 0.5f, 0.75f, 1.0f });
            object.meshes.push_back(std::move(mesh));
            level.motsArchLighting.push_back(std::move(object));
            return level;
        };
        const auto rejectsAtomically = [](Level candidate,
                const std::string_view expectedError,
                const std::string_view label)
        {
            std::ostringstream output;
            output << "native-arch-sentinel";
            bool rejected = false;
            try
            {
                assets::saveToSED(candidate, output);
            }
            catch (const std::runtime_error& error)
            {
                rejected = exactSedText(error.what(), expectedError);
            }
            catch (const std::exception&)
            {
                rejected = false;
            }
            check(rejected, std::string(label) + ": rejects with the fixed diagnostic");
            check(output.str() == "native-arch-sentinel",
                std::string(label) + ": rejection leaves the output stream unchanged");
        };

        Level negativeObject = nativeArchFixture();
        negativeObject.motsArchLighting[0].wireIndex = -1;
        rejectsAtomically(std::move(negativeObject),
            "Invalid binary SED file: thng ARCHLIGHTING object wire index is invalid or duplicated",
            "SED MotS ARCHLIGHTING native validation: negative object label");

        Level duplicateObject = nativeArchFixture();
        duplicateObject.motsArchLighting.push_back(
            duplicateObject.motsArchLighting.front());
        duplicateObject.motsArchLighting.back().meshes.front().wireIndex = 12;
        rejectsAtomically(std::move(duplicateObject),
            "Invalid binary SED file: thng ARCHLIGHTING object wire index is invalid or duplicated",
            "SED MotS ARCHLIGHTING native validation: duplicate object label");

        Level negativeMesh = nativeArchFixture();
        negativeMesh.motsArchLighting[0].meshes[0].wireIndex = -1;
        rejectsAtomically(std::move(negativeMesh),
            "Invalid binary SED file: thng ARCHLIGHTING mesh wire index is invalid or duplicated",
            "SED MotS ARCHLIGHTING native validation: negative mesh label");

        Level duplicateMesh = nativeArchFixture();
        duplicateMesh.motsArchLighting[0].meshes.push_back(
            duplicateMesh.motsArchLighting[0].meshes.front());
        rejectsAtomically(std::move(duplicateMesh),
            "Invalid binary SED file: thng ARCHLIGHTING mesh wire index is invalid or duplicated",
            "SED MotS ARCHLIGHTING native validation: duplicate mesh label");

        constexpr std::size_t archRecordLimit = 1'000'000u;
        Level objectBudget = nativeArchFixture();
        objectBudget.motsArchLighting.clear();
        objectBudget.things[0].archLightingIndex = -1;
        objectBudget.motsArchLighting.resize(archRecordLimit + 1u);
        for (std::size_t index = 0; index < objectBudget.motsArchLighting.size(); ++index)
        {
            objectBudget.motsArchLighting[index].wireIndex =
                static_cast<std::int32_t>(index);
        }
        rejectsAtomically(std::move(objectBudget),
            "Invalid binary SED file: thng ARCHLIGHTING object count is too large",
            "SED MotS ARCHLIGHTING native validation: object budget");

        Level meshBudget = nativeArchFixture();
        meshBudget.motsArchLighting.clear();
        meshBudget.motsArchLighting.resize(2);
        for (std::size_t objectIndex = 0; objectIndex < 2; ++objectIndex)
        {
            meshBudget.motsArchLighting[objectIndex].wireIndex =
                static_cast<std::int32_t>(objectIndex);
            auto& meshes = meshBudget.motsArchLighting[objectIndex].meshes;
            meshes.resize(archRecordLimit / 2u + 1u);
            for (std::size_t meshIndex = 0; meshIndex < meshes.size(); ++meshIndex)
            {
                meshes[meshIndex].wireIndex = static_cast<std::int32_t>(meshIndex);
            }
        }
        rejectsAtomically(std::move(meshBudget),
            "Invalid binary SED file: thng ARCHLIGHTING total mesh record count is too large",
            "SED MotS ARCHLIGHTING native validation: aggregate mesh budget");

        Level vertexBudget = nativeArchFixture();
        vertexBudget.motsArchLighting[0].meshes.clear();
        vertexBudget.motsArchLighting[0].meshes.resize(2);
        for (std::size_t meshIndex = 0;
             meshIndex < vertexBudget.motsArchLighting[0].meshes.size();
             ++meshIndex)
        {
            auto& mesh = vertexBudget.motsArchLighting[0].meshes[meshIndex];
            mesh.wireIndex = static_cast<std::int32_t>(meshIndex);
            mesh.vertices.resize(archRecordLimit / 2u + 1u,
                MotsArchLightVertex{ 0.0f, 0.0f, 0.0f, 0.0f });
        }
        rejectsAtomically(std::move(vertexBudget),
            "Invalid binary SED file: thng ARCHLIGHTING total vertex record count is too large",
            "SED MotS ARCHLIGHTING native validation: aggregate vertex budget");
    }

    {
        class RejectingOutput final : public std::streambuf
        {
            std::streamsize xsputn(const char*, std::streamsize) override { return 0; }
            int_type overflow(int_type = traits_type::eof()) override { return traits_type::eof(); }
        } buffer;
        std::ostream stream(&buffer);
        bool rejected = false;
        try { assets::saveToSED(Level{}, stream); }
        catch (const std::runtime_error& error)
        {
            rejected = exactSedText(error.what(), "Failed to write binary SED file.");
        }
        check(rejected && stream.fail(), "Native SED stream write failure reports complete diagnostic and failed stream state");
    }
    const auto shaMatchesBytes = [](const std::span<const std::byte> input,
        const std::string_view expectedHex)
    {
        const auto actual = assets::calculateSedSha256(input);
        const std::vector<std::byte> expected = bytesFromHex(expectedHex);
        return expected.size() == actual.size()
            && std::equal(actual.begin(), actual.end(), expected.begin());
    };
    const auto shaMatches = [&shaMatchesBytes](const std::string_view input,
        const std::string_view expectedHex)
    {
        return shaMatchesBytes(
            std::as_bytes(std::span(input.data(), input.size())), expectedHex);
    };

    {
        Level normalized = richLevel(ProjectType::JKDF2);
        normalized.uvCoordinateSpace = UvCoordinateSpace::Normalized;
        normalized.sectors[0].surfaces[0].texVertices[0].u = -0.25f;
        normalized.sectors[0].surfaces[0].texVertices[0].v = 1.75f;
        normalized.sectors[0].surfaces[0].texVertices[1].u = 2.5f;
        normalized.sectors[0].surfaces[0].texVertices[1].v = -3.125f;
        normalized.sectors[0].surfaces[0].uScale = -0.125f;
        normalized.sectors[0].surfaces[0].vScale = 3.75f;

        const std::vector<std::byte> normalizedBytes =
            assets::saveToSedBytes(normalized);
        const Level normalizedLoaded =
            assets::loadFromSedBytes(normalizedBytes);
        check(normalizedBytes.size() >= 8u
                && normalizedBytes[4] == std::byte{ 2 }
                && normalizedBytes[5] == std::byte{ 0 }
                && normalizedBytes[6] == std::byte{ 0 }
                && normalizedBytes[7] == std::byte{ 0 },
            "SED V2 normalized UVs: writer uses file version 2");
        check(normalizedLoaded.uvCoordinateSpace
                    == UvCoordinateSpace::Normalized
                && normalizedLoaded.sectors[0].surfaces[0].uScale
                    == -0.125f
                && normalizedLoaded.sectors[0].surfaces[0].vScale
                    == 3.75f
                && assets::sedLevelsEqual(normalized, normalizedLoaded),
            "SED V2 normalized UVs: reader publishes exact signed multi-tile coordinates and per-axis scales");
        check(assets::saveToSedBytes(normalizedLoaded) == normalizedBytes,
            "SED V2 normalized UVs: load and save are byte-exact");

        Level pixels = normalized;
        pixels.uvCoordinateSpace = UvCoordinateSpace::TexturePixels;
        pixels.sectors[0].surfaces[0].texVertices[0].u = -32.0f;
        pixels.sectors[0].surfaces[0].texVertices[0].v = 56.0f;
        pixels.sectors[0].surfaces[0].uScale = -32.0f;
        pixels.sectors[0].surfaces[0].vScale = 240.0f;
        const std::vector<std::byte> pixelBytes = assets::saveToSedBytes(pixels);
        const Level pixelsLoaded = assets::loadFromSedBytes(pixelBytes);
        check(pixelBytes.size() >= 8u
                && pixelBytes[4] == std::byte{ 1 }
                && pixelBytes[5] == std::byte{ 0 }
                && pixelBytes[6] == std::byte{ 0 }
                && pixelBytes[7] == std::byte{ 0 },
            "SED V1 pixel UVs: compatibility writer retains file version 1");
        check(pixelsLoaded.uvCoordinateSpace
                    == UvCoordinateSpace::TexturePixels
                && pixelsLoaded.sectors[0].surfaces[0].uScale == -32.0f
                && pixelsLoaded.sectors[0].surfaces[0].vScale == 240.0f
                && assets::sedLevelsEqual(pixels, pixelsLoaded),
            "SED V1 pixel UVs: reader preserves exact signed legacy coordinates and per-axis scales");
        check(assets::saveToSedBytes(pixelsLoaded) == pixelBytes,
            "SED V1 pixel UVs: load and save remain byte-exact");

        Level invalid = normalized;
        invalid.uvCoordinateSpace = static_cast<UvCoordinateSpace>(99);
        bool invalidSpaceRejected = false;
        try
        {
            (void)assets::saveToSedBytes(invalid);
        }
        catch (const std::runtime_error& error)
        {
            invalidSpaceRejected = exactSedText(error.what(), "Invalid binary SED file: Level has an unknown UV coordinate space");
        }
        check(invalidSpaceRejected,
            "SED writer rejects an unknown Level UV coordinate space");
    }

    // RFC 6234 section 8.5 byte-aligned SHA-256 tests 1, 2, 3, 4, 6,
    // 8, and 10. The empty and 55/56-byte messages below are local
    // digest/padding-boundary cases, not vectors from that RFC table.
    check(shaMatches("",
            "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"),
        "SHA-256 local reference: empty message matches");
    check(shaMatches("abc",
            "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"),
        "SHA-256 RFC 6234: abc matches");
    check(shaMatches(
            "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq",
            "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1"),
        "SHA-256 RFC 6234: multi-block message matches");
    check(shaMatches(std::string(1'000'000, 'a'),
            "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0"),
        "SHA-256 RFC 6234: one-million-a long message matches");

    std::string rfcTest4{};
    constexpr std::string_view rfcTest4Block =
        "0123456701234567012345670123456701234567012345670123456701234567";
    rfcTest4.reserve(rfcTest4Block.size() * 10);
    for ( int repetition = 0; repetition < 10; ++repetition )
    {
        rfcTest4.append(rfcTest4Block);
    }
    check(rfcTest4.size() == 640,
        "SHA-256 RFC 6234 test 4: input contains ten exact 512-bit blocks");
    check(shaMatches(rfcTest4,
            "594847328451bdfa85056225462cc1d867d877fb388df0ce35f25ab5562bfbb5"),
        "SHA-256 RFC 6234 test 4: repeated exact-block message matches");

    const std::vector<std::byte> rfcTest6 = bytesFromHex("19");
    check(rfcTest6.size() == 1,
        "SHA-256 RFC 6234 test 6: input contains one complete byte");
    check(shaMatchesBytes(rfcTest6,
            "68aa2e2ee5dff96e3355e6c7ee373e3d6a4e17f75f9518d843709c0c9bc3e3d4"),
        "SHA-256 RFC 6234 test 6: single byte matches");

    const std::vector<std::byte> rfcTest8 = bytesFromHex(
        "e3d72570dcdd787ce3887ab2cd684652");
    check(rfcTest8.size() == 16,
        "SHA-256 RFC 6234 test 8: input contains 128 bits");
    check(shaMatchesBytes(rfcTest8,
            "175ee69b02ba9b58e2b0a5fd13819cea573f3940a94f825128cf4209beabb4e8"),
        "SHA-256 RFC 6234 test 8: 128-bit message matches");

    const std::vector<std::byte> rfcTest10 = bytesFromHex(
        "8326754e2277372f4fc12b20527afef0"
        "4d8a056971b11ad57123a7c137760000"
        "d7bef6f3c1f7a9083aa39d810db31077"
        "7dab8b1e7f02b84a26c773325f8b2374"
        "de7a4b5a58cb5c5cf35bcee6fb946e5b"
        "d694fa593a8beb3f9d6592ecedaa66ca"
        "82a29d0c51bcf9336230e5d784e4c0a4"
        "3f8d79a30a165cbabe452b774b9c7109"
        "a97d138f129228966f6c0adc106aad5a"
        "9fdd30825769b2c671af6759df28eb39"
        "3d54d6");
    check(rfcTest10.size() == 163,
        "SHA-256 RFC 6234 test 10: input contains 1304 bits");
    check(shaMatchesBytes(rfcTest10,
            "97dbca7df46d62c8a422c941dd7e835b8ad3361763f7e9b2d95f4f0da6e1ccbc"),
        "SHA-256 RFC 6234 test 10: 1304-bit message matches");

    check(shaMatches(std::string(55, 'a'),
            "9f4390f8d30c2dd92ec9f095b65e2b9ae9b0a925a5258e241c9f1e910f734318")
            && shaMatches(std::string(56, 'a'),
                "b35439a4ac6f0948b6d6f9e3c6af0f5f590ce20f1bde7090ef7970686ec6738a"),
        "SHA-256: one- and two-block padding boundary vectors match");

    const std::string crcVector = "123456789";
    const auto crcBytes = std::as_bytes(std::span(crcVector.data(), crcVector.size()));
    check(assets::calculateSedCrc32c(crcBytes, false) == 0xe306'9283u,
        "CRC32C: portable implementation matches the standard check vector");
    check(assets::calculateSedCrc32c(crcBytes, true)
            == assets::calculateSedCrc32c(crcBytes, false),
        "CRC32C: runtime-dispatched and portable implementations agree");

    // RFC 3720 Appendix B.4 CRC32C test vectors.
    std::array<std::byte, 32> crcZeros{};
    std::array<std::byte, 32> crcOnes{};
    crcOnes.fill(std::byte{0xFF});
    std::array<std::byte, 32> crcAscending{};
    std::array<std::byte, 32> crcDescending{};
    for ( std::size_t index = 0; index < crcAscending.size(); ++index )
    {
        crcAscending[index] = static_cast<std::byte>(index);
        crcDescending[index] = static_cast<std::byte>(31U - index);
    }

    constexpr std::array<std::byte, 48> crcRead10Pdu{
        std::byte{ 0x01 }, std::byte{ 0xc0 }, std::byte{ 0x00 }, std::byte{ 0x00 },
        std::byte{ 0x00 }, std::byte{ 0x00 }, std::byte{ 0x00 }, std::byte{ 0x00 },
        std::byte{ 0x00 }, std::byte{ 0x00 }, std::byte{ 0x00 }, std::byte{ 0x00 },
        std::byte{ 0x00 }, std::byte{ 0x00 }, std::byte{ 0x00 }, std::byte{ 0x00 },
        std::byte{ 0x14 }, std::byte{ 0x00 }, std::byte{ 0x00 }, std::byte{ 0x00 },
        std::byte{ 0x00 }, std::byte{ 0x00 }, std::byte{ 0x04 }, std::byte{ 0x00 },
        std::byte{ 0x00 }, std::byte{ 0x00 }, std::byte{ 0x00 }, std::byte{ 0x14 },
        std::byte{ 0x00 }, std::byte{ 0x00 }, std::byte{ 0x00 }, std::byte{ 0x18 },
        std::byte{ 0x28 }, std::byte{ 0x00 }, std::byte{ 0x00 }, std::byte{ 0x00 },
        std::byte{ 0x00 }, std::byte{ 0x00 }, std::byte{ 0x00 }, std::byte{ 0x00 },
        std::byte{ 0x02 }, std::byte{ 0x00 }, std::byte{ 0x00 }, std::byte{ 0x00 },
        std::byte{ 0x00 }, std::byte{ 0x00 }, std::byte{ 0x00 }, std::byte{ 0x00 },
    };

    const auto checkCrcVector = [](const std::span<const std::byte> input,
        const std::uint32_t expected, const std::string& name)
    {
        check(assets::calculateSedCrc32c(input, false) == expected,
            name + ": portable implementation matches RFC 3720");
        check(assets::calculateSedCrc32c(input, true) == expected,
            name + ": runtime-dispatched implementation matches RFC 3720");
    };
    checkCrcVector(crcZeros, 0x8a91'36aau, "CRC32C 32 zero bytes");
    checkCrcVector(crcOnes, 0x62a8'ab43u, "CRC32C 32 0xFF bytes");
    checkCrcVector(crcAscending, 0x46dd'794eu, "CRC32C ascending bytes");
    checkCrcVector(crcDescending, 0x113f'db5cu, "CRC32C descending bytes");
    checkCrcVector(crcRead10Pdu, 0xd996'3a56u, "CRC32C iSCSI Read(10) PDU");
    check(assets::SedMaximumFileBytes == 256ull * 1024ull * 1024ull
            && assets::SedMaximumSemanticSectionBytes == 128ull * 1024ull * 1024ull
            && assets::SedMaximumExpandedPayloadBytes == 256ull * 1024ull * 1024ull
            && assets::SedMaximumDecodedObjectBytes == 128ull * 1024ull * 1024ull
            && assets::SedMaximumDecodedItems == 2'000'000ull
            && assets::SedMaximumCodedSectionBytes
                > assets::SedMaximumSemanticSectionBytes * 5ull / 4ull
            && assets::SedMaximumCodedSectionBytes < assets::SedMaximumFileBytes,
        "SED limits: practical file, semantic, expanded, and 16+4 coded ceilings agree");

    {
        constexpr std::size_t headerBytes = 28;
        constexpr std::size_t headerMirrorBytes = 40;
        constexpr std::size_t footerBytes = 8;
        const Level source = richLevel(ProjectType::IJIM);
        constexpr std::int64_t first = 4'102'444'800'123ll;
        constexpr std::int64_t second = 4'102'444'700'789ll;
        const auto firstBytes = assets::saveToSedBytes(source, {},
            assets::SedSerializationProfile::ProtectedV1, first);
        const auto secondBytes = assets::saveToSedBytes(source, {},
            assets::SedSerializationProfile::ProtectedV1, second);
        const assets::SedFile firstFile = assets::loadSedFileBytes(firstBytes);
        const assets::SedFile secondFile = assets::loadSedFileBytes(secondBytes);
        const auto firstSpan = std::span<const std::byte>(firstBytes);
        const auto secondSpan = std::span<const std::byte>(secondBytes);
        const std::size_t semanticEnd = firstBytes.size()
            - headerMirrorBytes - footerBytes;
        check(firstBytes.size() == secondBytes.size()
                && firstBytes != secondBytes
                && firstFile.timestamp == first
                && secondFile.timestamp == second
                && assets::sedLevelsEqual(firstFile.level, secondFile.level)
                && std::equal(firstSpan.begin() + headerBytes,
                    firstSpan.begin() + static_cast<std::ptrdiff_t>(semanticEnd),
                    secondSpan.begin() + headerBytes)
                && std::equal(firstSpan.end() - footerBytes, firstSpan.end(),
                    secondSpan.end() - footerBytes),
            "SED document timestamp: signed 64-bit post-2038 values round-trip and only fixed-header/mirror bytes change");

        const auto nested = assets::saveToSedBytes(source, {},
            assets::SedSerializationProfile::UnprotectedNested, first);
        check(assets::loadSedFileBytes(firstBytes).timestamp == first
                && assets::loadNestedSedLevelBytes(nested).kind == source.kind
                && nested == assets::saveToSedBytes(source, {},
                    assets::SedSerializationProfile::UnprotectedNested),
            "SED document timestamp: nested history/content encoding canonicalizes creation time to zero");

        bool rejectedNegative = false;
        try
        {
            (void)assets::saveToSedBytes(source, {},
                assets::SedSerializationProfile::ProtectedV1,
                -1);
        }
        catch (const std::runtime_error&)
        {
            rejectedNegative = true;
        }
        check(rejectedNegative,
            "SED document timestamp: a negative creation time is rejected");
    }

    {
        const Level source = resourceCategoryLevel();

        const ResourceEncodingEstimate resourceEstimate =
            estimateResourceEncoding(source);
        check(resourceEstimate.occurrenceCount == 91
                && resourceEstimate.uniqueStringCount == 54
                && resourceEstimate.inlineStringBytes == 1366
                && resourceEstimate.catalogAndCompactReferenceBytes == 1031
                && resourceEstimate.catalogAndFixedReferenceBytes == 1304,
            "SED categorized-resource measurement: independent all-category vector remains 1366 inline bytes, 1304 catalog+u32 bytes, and 1031 catalog+ULEB128 bytes");

        const std::vector<std::byte> protectedBytes =
            assets::saveToSedBytes(source);
        const std::filesystem::path resourceVectorPath =
            std::filesystem::path(kSedVectorRoot)
                / "all_resource_categories.sed";
        const std::vector<std::byte> protectedVector = readBinaryVector(
            resourceVectorPath);
        const Level vectorLevel = assets::loadFromSED(resourceVectorPath);
        check(protectedBytes == protectedVector
                && assets::sedLevelsEqual(source, vectorLevel),
            "SED categorized-resource file vector is deterministic and opens with every authored resource reference intact");

        const auto nested = assets::saveToSedBytes(source, {},
            assets::SedSerializationProfile::UnprotectedNested);
        const SectionLayout resources = findSection(nested,
            { 'r', 's', 'r', 'c' });
        const auto payload = std::span<const std::byte>(nested).subspan(
            resources.payloadOffset, static_cast<std::size_t>(resources.storedSize));
        std::size_t cursor{};
        std::array<std::vector<std::string>, 14> catalogs{};
        for (auto& catalog : catalogs)
            catalog = readTestStringVector(payload, cursor);

        check(std::find(catalogs[0].begin(), catalogs[0].end(),
                    "unknown_param") != catalogs[0].end()
                && std::find(catalogs[0].begin(), catalogs[0].end(),
                    "some_string") != catalogs[0].end()
                && catalogs[1] == std::vector<std::string>{ "master.cmp", "alternate.cmp" }
                && catalogs[2] == std::vector<std::string>{ "stone.mat", "alpha.mat", "typed.mat", "cog.mat" }
                && catalogs[3] == std::vector<std::string>{ "wind.wav", "voice.wav" }
                && catalogs[4] == std::vector<std::string>{ "actor.ai", "cog_actor.ai" }
                && catalogs[5] == std::vector<std::string>{ "actor.3do", "cog_actor.3do" }
                && catalogs[6] == std::vector<std::string>{ "actor.spr" }
                && catalogs[7] == std::vector<std::string>{ "walk.key" }
                && catalogs[8] == std::vector<std::string>{ "actor.pup" }
                && catalogs[9] == std::vector<std::string>{ "actor.snd" }
                && catalogs[10] == std::vector<std::string>{ "typed_class.cog", "logic.cog", "linked.cog" }
                && catalogs[11] == std::vector<std::string>{ "+derived_actor", "typed_template", "_actor", "cog_template" }
                && catalogs[12] == std::vector<std::string>{ "smoke.par" }
                && catalogs[13] == std::vector<std::string>{ "Main", "Gameplay", "Hidden detail" },
            "SED categorized resources: all 14 fixed resource kinds own independent local index ranges");

        const auto readInventory = [&](const std::vector<std::int32_t>& expected)
        {
            const std::uint32_t count = readU32Le(payload, cursor);
            cursor += 4;
            if (count != expected.size()) return false;
            for (const std::int32_t expectedIndex : expected)
            {
                const std::size_t before = cursor;
                if (readSedIndex(payload, cursor) != expectedIndex
                    || cursor - before != 1)
                {
                    return false;
                }
            }
            return true;
        };
        check(readInventory({ 0, 1 })
                && readInventory({ 0, 1, 0 })
                && readInventory({ 0, 1, 2 })
                && cursor == payload.size(),
            "SED categorized resources: inventories use one-byte category-local biased ULEB128 references");

        const SectionLayout things = findSection(nested,
            { 't', 'h', 'n', 'g' });
        const auto thingPayload = std::span<const std::byte>(nested).subspan(
            things.payloadOffset, static_cast<std::size_t>(things.storedSize));
        std::size_t thingCursor = 4u + 4u; // Thing count and stable id
        skipSedIndex(thingPayload, thingCursor); // Thing number
        skipSedIndex(thingPayload, thingCursor); // Sector
        skipSedIndex(thingPayload, thingCursor); // Template
        skipSedIndex(thingPayload, thingCursor); // Instance name
        thingCursor += 6u * 8u; // position and orientation
        const std::uint32_t valueCount = readU32Le(thingPayload, thingCursor);
        thingCursor += 4u;
        std::vector<std::int32_t> stringReferences{};
        stringReferences.reserve(valueCount);
        bool allSmallReferencesCanonical = true;
        for (std::uint32_t value = 0; value < valueCount; ++value)
        {
            thingCursor += 4u; // AdjType
            const std::size_t nameStart = thingCursor;
            (void)readSedIndex(thingPayload, thingCursor);
            allSmallReferencesCanonical = allSmallReferencesCanonical
                && thingCursor - nameStart == 1u;
            thingCursor += 4u + 4u + 8u; // ValueType, intValue, floatValue
            const std::size_t resourceStart = thingCursor;
            stringReferences.push_back(
                readSedIndex(thingPayload, thingCursor));
            allSmallReferencesCanonical = allSmallReferencesCanonical
                && thingCursor - resourceStart == 1u;
        }
        check(valueCount == 12u && allSmallReferencesCanonical
                && stringReferences
                    == std::vector<std::int32_t>{
                        2, 4, 2, 0, 0, 0, 0, 0, 0, 0, 1, -1 },
            "SED categorized resources: typed Thing/template params store one local biased ULEB128 index and empty uses -1");

        const SectionLayout cogs = findSection(nested,
            { 'c', 'o', 'g', 's' });
        const auto cogPayload = std::span<const std::byte>(nested).subspan(
            cogs.payloadOffset, static_cast<std::size_t>(cogs.storedSize));
        std::size_t cogCursor = 4u + 4u; // COG count and stable id
        skipSedIndex(cogPayload, cogCursor); // COG number
        const std::size_t scriptStart = cogCursor;
        const std::int32_t scriptReference = readSedIndex(
            cogPayload, cogCursor);
        const bool compactScriptReference = cogCursor - scriptStart == 1u;
        const std::uint32_t cogValueCount = readU32Le(cogPayload, cogCursor);
        cogCursor += 4u;
        std::vector<std::int32_t> cogResourceReferences{};
        cogResourceReferences.reserve(cogValueCount);
        bool allCogReferencesCanonical = true;
        for (std::uint32_t value = 0; value < cogValueCount; ++value)
        {
            const CogType cogType = static_cast<CogType>(
                std::bit_cast<std::int32_t>(readU32Le(cogPayload, cogCursor)));
            cogCursor += 4u;
            const std::size_t nameStart = cogCursor;
            skipSedIndex(cogPayload, cogCursor);
            allCogReferencesCanonical = allCogReferencesCanonical
                && cogCursor - nameStart == 1u;
            const ValueType valueType = static_cast<ValueType>(
                std::bit_cast<std::int32_t>(readU32Le(cogPayload, cogCursor)));
            cogCursor += 4u;
            if (valueType == ValueType::ptr
                && (cogType == CogType::sec || cogType == CogType::surface
                    || cogType == CogType::thing))
            {
                skipSedIndex(cogPayload, cogCursor);
            }
            else
            {
                cogCursor += 4u;
            }
            cogCursor += 8u; // floatValue
            const std::size_t resourceStart = cogCursor;
            cogResourceReferences.push_back(readSedIndex(
                cogPayload, cogCursor));
            allCogReferencesCanonical = allCogReferencesCanonical
                && cogCursor - resourceStart == 1u;
            cogCursor += 1u; // local
            const std::size_t descriptionStart = cogCursor;
            skipSedIndex(cogPayload, cogCursor); // description
            allCogReferencesCanonical = allCogReferencesCanonical
                && cogCursor - descriptionStart == 1u;
            cogCursor += 4u; // mask
        }
        check(scriptReference == 1 && compactScriptReference
                && cogValueCount == 8u && allCogReferencesCanonical
                && cogResourceReferences
                    == std::vector<std::int32_t>{
                        17, 0, 1, 1, 1, 3, 2, 3 }
                && cogCursor == cogPayload.size(),
            "SED categorized resources: COG MAT/3DO/COG/AI/KEY/WAV/template values use category-local biased ULEB128 indices");

        const Level loaded = assets::loadNestedSedLevelBytes(nested);
        check(assets::sedLevelsEqual(source, loaded)
                && loaded.things[0].values[1].name == "unknown_param"
                && loaded.things[0].values[1].stringValue == "some_string"
                && loaded.things[0].values[1].adjType == AdjType::unk
                && loaded.templates[0].values[1].name == "unknown_param"
                && loaded.templates[0].values[1].stringValue == "some_string",
            "SED categorized resources: every known category and unknown Thing/template parameters round-trip losslessly");
    }

    {
        Level boundary = richLevel(ProjectType::IJIM);
        boundary.h3doNodes[0].num = 0;
        boundary.h3doNodes[0].mesh = 126;
        boundary.h3doNodes[0].parent = 127;
        const auto nested = assets::saveToSedBytes(boundary, {},
            assets::SedSerializationProfile::UnprotectedNested);
        const SectionLayout hierarchy = findSection(nested,
            { 'h', 'n', 'o', 'd' });
        const auto payload = std::span<const std::byte>(nested).subspan(
            hierarchy.payloadOffset,
            static_cast<std::size_t>(hierarchy.storedSize));
        std::size_t cursor = 4u + 4u; // node count and stable id
        const std::size_t numberStart = cursor;
        const std::int32_t number = readSedIndex(payload, cursor);
        const std::size_t numberBytes = cursor - numberStart;
        cursor += 4u; // node type
        const std::size_t meshStart = cursor;
        const std::int32_t mesh = readSedIndex(payload, cursor);
        const std::size_t meshBytes = cursor - meshStart;
        const std::size_t parentStart = cursor;
        const std::int32_t parent = readSedIndex(payload, cursor);
        const std::size_t parentBytes = cursor - parentStart;
        check(number == 0 && numberBytes == 1u
                && payload[numberStart] == std::byte{ 0x01 }
                && mesh == 126 && meshBytes == 1u
                && payload[meshStart] == std::byte{ 0x7f }
                && parent == 127 && parentBytes == 2u
                && payload[parentStart] == std::byte{ 0x80 }
                && payload[parentStart + 1u] == std::byte{ 0x01 },
            "SED biased index boundary: 0 and 126 use one byte while 127 canonically crosses to two bytes");
    }

    {
        for (const SedProjectVector& vector : kSedProjectVectors)
        {
            assets::JklLoadOptions options{};
            options.projectType = vector.kind;
            const std::filesystem::path inputPath = vectorPath(
                vector, "_input", vector.inputExtension);
            Level imported{};
            try
            {
                imported = assets::loadFromJKL(inputPath, options);
            }
            catch (const std::exception& exception)
            {
                throw std::runtime_error(
                    "SED format vector input '" + inputPath.string()
                    + "' failed to load: " + exception.what());
            }
            const bool hasExpectedGeometry = imported.sectors.size() == 1u
                && imported.sectors[0].vertices.size() == 3u
                && imported.sectors[0].surfaces.size() == 1u
                && imported.sectors[0].surfaces[0].vertices.size() == 3u;
            check(hasExpectedGeometry,
                "SED format vector " + inputPath.filename().string()
                    + ": importer retains one sector with one three-vertex surface");
            const std::vector<std::byte> generatedSed =
                assets::saveToSedBytes(imported);
            const std::filesystem::path nativePath = vectorPath(
                vector, "_native", ".sed");
            const std::vector<std::byte> sedBytes = readBinaryVector(
                nativePath);
            const std::string expectedLegacy = readTextVector(
                vectorPath(vector, "_expected", vector.outputExtension));
            const Level fromSed = assets::loadFromSED(nativePath);
            std::ostringstream actualLegacy{};
            assets::saveToJKL(fromSed, actualLegacy);
            const std::vector<std::byte> resavedSed = assets::saveToSedBytes(
                fromSed, {}, assets::SedSerializationProfile::ProtectedV1, 0);
            const std::string label = inputPath.filename().string();
            check(generatedSed == sedBytes,
                "SED format vector " + label
                    + ": checked-in native bytes match the deterministic V1 writer");
            check(resavedSed == sedBytes,
                "SED format vector " + label
                    + ": load -> canonical save is byte-identical to the checked-in V1 document");
            check(fromSed.kind == vector.kind,
                "SED format vector " + label + ": project type survives native round-trip");
            check(assets::sedLevelsEqual(imported, fromSed),
                "SED format vector " + label + ": complete Level survives native round-trip");
            check(expectedLegacy == actualLegacy.str(),
                "SED format vector " + label
                    + ": opening checked-in SED and exporting canonical JKL/NDY byte-matches the checked-in text vector");

            const SectionLayout geometry = findSection(sedBytes,
                { 's', 'e', 'c', 't' });
            std::vector<std::byte> repairable = sedBytes;
            const std::array<std::size_t, 4> fourSlots{ 0, 1, 2, 3 };
            damageSlots(repairable, geometry, fourSlots, 0);
            const auto repaired = assets::loadSedFileBytes(repairable);
            std::ostringstream repairedLegacy{};
            assets::saveToJKL(repaired.level, repairedLegacy);
            check(repaired.repairedChunks == 4
                    && assets::sedLevelsEqual(repaired.level, imported)
                    && repairedLegacy.str() == expectedLegacy,
                "SED recovery vector " + label
                    + ": p4 repairs geometry and canonical JKL/NDY export byte-matches");

            std::vector<std::byte> generatedBeyondParity = sedBytes;
            const std::array<std::size_t, 5> fiveSlots{ 0, 1, 2, 3, 4 };
            damageSlots(generatedBeyondParity, geometry, fiveSlots, 0);
            const std::vector<std::byte> beyondParity = readBinaryVector(
                vectorPath(vector, "_sect_unrecoverable", ".sed"));
            const auto partial = assets::recoverFromSedBytes(beyondParity);
            std::ostringstream partialLegacy{};
            assets::saveToJKL(partial.level, partialLegacy);
            const std::string expectedPartial = readTextVector(vectorPath(
                vector, "_expected_partial", vector.outputExtension));
            const auto findUnknownParameter = [](const Level& level)
                -> const TplValue*
            {
                if (level.templates.empty()) return nullptr;
                const auto& values = level.templates[0].values;
                const auto found = std::find_if(values.begin(), values.end(),
                    [](const TplValue& value)
                    {
                        return value.name == "unknown_param";
                    });
                return found == values.end() ? nullptr : &*found;
            };
            const TplValue* const importedUnknown =
                findUnknownParameter(imported);
            const TplValue* const partialUnknown =
                findUnknownParameter(partial.level);
            check(generatedBeyondParity == beyondParity
                    && partial.partialRecoveryFromCorruption
                    && std::find(partial.missingRequiredSections.begin(),
                        partial.missingRequiredSections.end(), "sect")
                        != partial.missingRequiredSections.end()
                    && partial.level.kind == vector.kind
                    && partial.level.sectors.empty()
                    && partial.level.things.size() == imported.things.size()
                    && partial.level.templates.size() == imported.templates.size()
                    && partial.level.things[0].name == imported.things[0].name
                    && importedUnknown != nullptr && partialUnknown != nullptr
                    && partialUnknown->stringValue == importedUnknown->stringValue
                    && partialLegacy.str() == expectedPartial,
                "SED corruption vector " + label
                    + ": checked-in beyond-p4 loss exports the healthy domains to the exact checked-in partial JKL/NDY vector");
        }
    }

    {
        // HIST is an optional editor-history domain and intentionally uses
        // the lighter p=2 protection tier.  This small checked-in document
        // exercises every slot in every stripe: one and two erased chunks
        // must reconstruct the exact history payload, while three erasures
        // discard only HIST during permissive recovery and leave Level data
        // intact.
        const std::filesystem::path historyPath =
            std::filesystem::path(kSedVectorRoot) / "hist_small.sed";
        const std::vector<std::byte> historyBytes = readBinaryVector(historyPath);
        const Level source = richLevel(ProjectType::IJIM);
        const assets::SedFile baseline = assets::loadSedFileBytes(historyBytes);
        const SectionLayout history = findSection(historyBytes,
            { 'h', 'i', 's', 't' });
        const auto findHistory = [](const std::vector<assets::SedOptionalSection>& sections)
            -> const assets::SedOptionalSection*
        {
            const auto found = std::find_if(sections.begin(), sections.end(),
                [](const assets::SedOptionalSection& section)
                {
                    return section.type == std::array<char, 4>{
                        'h', 'i', 's', 't' };
                });
            return found == sections.end() ? nullptr : &*found;
        };
        const assets::SedOptionalSection* const baselineHistory =
            findHistory(baseline.optionalSections);
        const auto canonicalWithHistory = assets::saveToSedBytes(
            baseline.level, baseline.optionalSections,
            assets::SedSerializationProfile::ProtectedV1, 0);
        check(history.parityStreams == 2
                && history.stripeCount >= 1
                && baseline.level.kind == ProjectType::IJIM
                && assets::sedLevelsEqual(baseline.level, source)
                && baselineHistory != nullptr
                && assets::loadHistoryNestedSedLevelBytes(
                    baselineHistory->payload).kind == ProjectType::IJIM
                && canonicalWithHistory == historyBytes,
            "SED HIST golden: checked-in V1 p=2 envelope and inner HistoryNested stream are canonical");

        for (std::size_t damageCount = 1; damageCount <= 2; ++damageCount)
        {
            const std::vector<std::byte> damaged = readBinaryVector(
                std::filesystem::path(kSedVectorRoot)
                / ("hist_small_" + std::to_string(damageCount)
                    + "_damage.sed"));
            std::vector<std::byte> expected = historyBytes;
            std::array<std::size_t, 3> streams{};
            for (std::size_t i = 0; i < damageCount; ++i)
                streams[i] = i;
            for (std::size_t stripe = 0;
                 stripe < history.stripeCount; ++stripe)
            {
                damageSlots(expected, history,
                    std::span<const std::size_t>(streams).first(damageCount),
                    stripe);
            }
            const assets::SedFile repaired = assets::loadSedFileBytes(damaged);
            const assets::SedOptionalSection* const repairedHistory =
                findHistory(repaired.optionalSections);
            check(damaged == expected
                    && repaired.repairedChunks
                        == damageCount * history.stripeCount
                    && assets::sedLevelsEqual(repaired.level, source)
                    && repairedHistory != nullptr
                    && baselineHistory != nullptr
                    && repairedHistory->payload == baselineHistory->payload,
                "SED HIST golden: " + std::to_string(damageCount)
                    + " damaged chunk(s) per stripe are repaired exactly");
        }

        const std::vector<std::byte> beyondParity = readBinaryVector(
            std::filesystem::path(kSedVectorRoot) / "hist_small_3_damage.sed");
        std::vector<std::byte> expectedBeyondParity = historyBytes;
        const std::array<std::size_t, 3> threeStreams{ 0, 1, 2 };
        for (std::size_t stripe = 0; stripe < history.stripeCount; ++stripe)
            damageSlots(expectedBeyondParity, history, threeStreams, stripe);
        bool strictRejected = false;
        try { (void)assets::loadSedFileBytes(beyondParity); }
        catch (const std::exception&) { strictRejected = true; }
        const assets::SedRecoveryLoadResult partial =
            assets::recoverFromSedBytes(beyondParity);
        check(beyondParity == expectedBeyondParity
                && strictRejected
                && partial.partialRecoveryFromCorruption
                && partial.missingRequiredSections.empty()
                && std::find(partial.missingOptionalSections.begin(),
                    partial.missingOptionalSections.end(), "hist")
                    != partial.missingOptionalSections.end()
                && findHistory(partial.optionalSections) == nullptr
                && assets::sedLevelsEqual(partial.level, source),
            "SED HIST golden: three damaged chunks per stripe drop only optional HIST");
    }

    {
        const Level source = richLevel(ProjectType::IJIM);
        assets::SedLevelDecodedFootprint expected{ sizeof(Level), 1u };
        const auto addString = [&](const std::string& value)
        {
            expected.ownedBytes += value.size() + 1u;
        };
        const auto addVector = [&]<typename T>(const std::vector<T>& values)
        {
            expected.ownedBytes += values.size() * sizeof(T);
            expected.itemCount += values.size();
        };
        const auto addValue = [&](const Value& value)
        {
            addString(value.name);
            addString(value.stringValue);
        };

        addString(source.masterCMP);
        addString(source.lvisstring);
        addVector(source.colormaps);
        for (const std::string& value : source.colormaps) addString(value);
        addVector(source.materials);
        for (const std::string& value : source.materials) addString(value);
        addVector(source.layers);
        for (const std::string& value : source.layers) addString(value);
        addVector(source.sectors);
        for (const Sector& sector : source.sectors)
        {
            addString(sector.colormap);
            addString(sector.sound);
            addVector(sector.vertices);
            addVector(sector.surfaces);
            for (const Surface& surface : sector.surfaces)
            {
                addString(surface.material);
                addVector(surface.vertices);
                addVector(surface.texVertices);
            }
        }
        addVector(source.things);
        for (const Thing& thing : source.things)
        {
            addString(thing.templateName);
            addString(thing.name);
            addVector(thing.values);
            for (const TplValue& value : thing.values) addValue(value);
        }
        addVector(source.templates);
        for (const ThingTemplate& thingTemplate : source.templates)
        {
            addString(thingTemplate.name);
            addString(thingTemplate.parent);
            addVector(thingTemplate.values);
            for (const TplValue& value : thingTemplate.values) addValue(value);
            addString(thingTemplate.desc);
        }
        addVector(source.cogs);
        for (const Cog& cog : source.cogs)
        {
            addString(cog.name);
            addVector(cog.values);
            for (const CogValue& value : cog.values)
            {
                addValue(value);
                addString(value.description);
            }
        }
        addVector(source.lights);
        addVector(source.h3doNodes);
        for (const HNode& node : source.h3doNodes) addString(node.name);

        const assets::SedLevelDecodedFootprint estimated =
            assets::estimateSedLevelDecodedFootprint(source);
        Level capacityOnly{};
        const assets::SedLevelDecodedFootprint empty =
            assets::estimateSedLevelDecodedFootprint(capacityOnly);
        capacityOnly.materials.reserve(4096);
        const assets::SedLevelDecodedFootprint reserved =
            assets::estimateSedLevelDecodedFootprint(capacityOnly);
        check(estimated == expected && estimated.itemCount == 28u
                && empty == assets::SedLevelDecodedFootprint{
                    sizeof(Level) + 2u, 1u }
                && reserved == empty,
            "SED decoded footprint: exact root, nested vector, string, and logical-size accounting");

        assets::SedLevelDecodedFootprint accumulated = estimated;
        const bool accumulatedSafely =
            assets::tryAccumulateSedLevelDecodedFootprint(accumulated, empty);
        assets::SedLevelDecodedFootprint overflow{
            std::numeric_limits<std::uint64_t>::max(), 7u };
        const assets::SedLevelDecodedFootprint beforeOverflow = overflow;
        const bool rejectedByteOverflow =
            !assets::tryAccumulateSedLevelDecodedFootprint(
                overflow, assets::SedLevelDecodedFootprint{ 1u, 0u });
        assets::SedLevelDecodedFootprint itemOverflow{
            9u, std::numeric_limits<std::uint64_t>::max() };
        const assets::SedLevelDecodedFootprint beforeItemOverflow = itemOverflow;
        const bool rejectedItemOverflow =
            !assets::tryAccumulateSedLevelDecodedFootprint(
                itemOverflow, assets::SedLevelDecodedFootprint{ 0u, 1u });
        check(accumulatedSafely
                && accumulated.ownedBytes == estimated.ownedBytes + empty.ownedBytes
                && accumulated.itemCount == estimated.itemCount + empty.itemCount
                && rejectedByteOverflow && overflow == beforeOverflow
                && rejectedItemOverflow && itemOverflow == beforeItemOverflow,
            "SED decoded footprint: cumulative HIST accounting is checked and atomic on overflow");
    }

    {
        constexpr std::size_t kCanonicalHeaderBytes = 28u;
        constexpr std::size_t kSynthesizedHeaderMirrorFooterBytes = 76u;
        const std::size_t maximum = static_cast<std::size_t>(
            assets::SedMaximumFileBytes);
        const auto mirroredBoundary = assets::detail::recoveryFramingSizeForTesting(
            maximum - kCanonicalHeaderBytes, true);
        const auto synthesizedBoundary = assets::detail::recoveryFramingSizeForTesting(
            maximum - kSynthesizedHeaderMirrorFooterBytes, false);
        check(mirroredBoundary && *mirroredBoundary == maximum
                && !assets::detail::recoveryFramingSizeForTesting(
                    maximum - kCanonicalHeaderBytes + 1u, true)
                && synthesizedBoundary && *synthesizedBoundary == maximum
                && !assets::detail::recoveryFramingSizeForTesting(
                    maximum - kSynthesizedHeaderMirrorFooterBytes + 1u, false),
            "SED recovery limits: synthesized framing is rejected before a near-cap allocation");
    }

    {
        constexpr std::array<std::byte, 8> kAnchor{
            std::byte{'s'}, std::byte{'e'}, std::byte{'c'}, std::byte{'t'},
            std::byte{'v'}, std::byte{'1'}, std::byte{'\n'}, std::byte{0} };
        // One pass sees 4,097 exact descriptor markers. The later fallback
        // pass shares the same 8,192-candidate budget and must fail closed
        // instead of performing another attacker-controlled CRC sweep.
        std::vector<std::byte> markerFlood{};
        markerFlood.reserve(kAnchor.size() * 4097u);
        for (std::size_t i = 0; i < 4097u; ++i)
        {
            markerFlood.insert(markerFlood.end(), kAnchor.begin(), kAnchor.end());
        }
        bool rejected = false;
        std::string reason{};
        try
        {
            (void)assets::recoverFromSedBytes(markerFlood);
        }
        catch (const std::runtime_error& error)
        {
            rejected = true;
            reason = error.what();
        }
        check(rejected
                && reason == "Invalid binary SED file: recovery marker candidate limit exceeded",
            "SED recovery budget: exact-marker flood is bounded across fallback passes");
    }

    {
        constexpr std::array<std::byte, 4> kMagic{
            std::byte{'s'}, std::byte{'e'}, std::byte{'d'}, std::byte{0} };
        std::vector<std::byte> markerFlood{};
        markerFlood.reserve(kMagic.size() * 8193u + kTestHeaderSize);
        for (std::size_t i = 0; i < 8193u; ++i)
        {
            markerFlood.insert(markerFlood.end(), kMagic.begin(), kMagic.end());
        }
        markerFlood.insert(markerFlood.end(), kTestHeaderSize, std::byte{0});
        bool rejected = false;
        std::string reason{};
        try
        {
            (void)assets::findEmbeddedSedDocument(markerFlood);
        }
        catch (const std::runtime_error& error)
        {
            rejected = true;
            reason = error.what();
        }
        check(rejected
                && reason == "Invalid binary SED file: recovery marker candidate limit exceeded",
            "SED embedded scanner budget: sed-magic flood is bounded before header validation amplification");
    }

    {
        assets::SedOptionalSection compressed{};
        compressed.type = { 't', 's', 't', '1' };
        compressed.payload.assign(4096, std::byte{0x41});
        const std::array optional{ compressed };
        const std::vector<std::byte> encoded = assets::saveToSedBytes(
            richLevel(ProjectType::JKDF2), optional);
        const SectionLayout layout = findSection(encoded, compressed.type);
        const std::vector<std::byte> logical = logicalStoredBytes(encoded, layout);
        const std::vector<std::byte> golden = bytesFromHex(
            "0041ff0100ff0300ff0300ff0300ff0300ff0300ff0300ff0300ff0300ff0300"
            "ff0300ff0300ff0300ff0300ff0300ff0300ff0300ff0300ff0300ff0300ff03"
            "00ff0300ff0300ff0300ff0300ff0300ff0300ff0300ff0300ff0300ff03009e0300");
        const auto logicalDigest = assets::calculateSedSha256(logical);
        const std::vector<std::byte> goldenDigest = bytesFromHex(
            "447a283a1f4a5880d054ae6790844b23770458c1941a44dc295adfc522b87e29");
        check(layout.logicalStoredSize == 98 && logical == golden
                && std::equal(logicalDigest.begin(), logicalDigest.end(),
                    goldenDigest.begin()),
            "LZSS: canonical 4096-byte repeated literal matches independent bytes and digest");
    }

    {
        // The newest ABC candidate is a four-byte mismatch. The two older
        // candidates carry equal 64-byte matches, so the bounded chain must
        // reach them and deterministically retain the nearer one.
        assets::SedOptionalSection chained{};
        chained.type = { 'l', 'z', 'c', 'h' };
        const std::string noise = deterministicNoise(4096);
        chained.payload.reserve(noise.size());
        for (const unsigned char value : noise)
            chained.payload.push_back(static_cast<std::byte>(value));
        std::array<std::byte, 64> pattern{};
        pattern[0] = std::byte{0xf1};
        pattern[1] = std::byte{0xe2};
        pattern[2] = std::byte{0xd3};
        for (std::size_t i = 3; i < pattern.size(); ++i)
            pattern[i] = static_cast<std::byte>(static_cast<std::uint8_t>(i * 37u + 11u));
        constexpr std::size_t older = 128;
        constexpr std::size_t nearer = 512;
        constexpr std::size_t decoy = 768;
        constexpr std::size_t target = 1024;
        std::copy(pattern.begin(), pattern.end(), chained.payload.begin() + older);
        std::copy(pattern.begin(), pattern.end(), chained.payload.begin() + nearer);
        std::copy_n(pattern.begin(), 3, chained.payload.begin() + decoy);
        chained.payload[decoy + 3] = pattern[3] ^ std::byte{0xff};
        std::copy(pattern.begin(), pattern.end(), chained.payload.begin() + target);
        chained.payload[older + pattern.size()] = std::byte{0x01};
        chained.payload[nearer + pattern.size()] = std::byte{0x02};
        chained.payload[target + pattern.size()] = std::byte{0x03};

        const std::array optional{ chained };
        const std::vector<std::byte> encoded = assets::saveToSedBytes(
            richLevel(ProjectType::IJIM), optional);
        const SectionLayout layout = findSection(encoded, chained.type);
        const bool compressed = (readU32Le(std::span<const std::byte>(encoded),
            layout.descriptorOffset + 16) & 1u) != 0;
        const TestLzssDecode decoded = decodeTestLzss(
            logicalStoredBytes(encoded, layout), chained.payload.size());
        const auto targetToken = std::find_if(decoded.tokens.begin(), decoded.tokens.end(),
            [](const TestLzssToken& token)
            {
                return token.match && token.decodedOffset == target;
            });
        check(compressed && decoded.bytes == chained.payload
                && targetToken != decoded.tokens.end()
                && targetToken->length == pattern.size()
                && targetToken->backwardOffset == target - nearer,
            "LZSS chain: older collision candidate is found and nearest equal match wins");
    }

    {
        // Positions whose low 16 bits alias exercise the predecessor-ring wrap.
        // The largest encodable offset must match; one byte farther must not.
        constexpr std::size_t source = 64;
        constexpr std::size_t acceptedTarget = source + 65'535;
        constexpr std::size_t rejectedTarget = source + 65'536;
        std::array<std::byte, 32> pattern{};
        for (std::size_t i = 0; i < pattern.size(); ++i)
            pattern[i] = static_cast<std::byte>(static_cast<std::uint8_t>(0x80u + i));
        const auto makeBoundaryPayload = [&](const std::size_t target,
            const std::array<char, 4>& type)
        {
            assets::SedOptionalSection section{};
            section.type = type;
            section.payload.assign(target + pattern.size() + 256u, std::byte{0});
            std::copy(pattern.begin(), pattern.end(), section.payload.begin() + source);
            std::copy(pattern.begin(), pattern.end(), section.payload.begin() + target);
            return section;
        };
        const std::array optional{
            makeBoundaryPayload(acceptedTarget, { 'l', 'z', 'w', '1' }),
            makeBoundaryPayload(rejectedTarget, { 'l', 'z', 'w', '2' }) };
        const std::vector<std::byte> encoded = assets::saveToSedBytes(
            richLevel(ProjectType::IJIM), optional);
        bool accepted = false;
        bool rejected = true;
        for (std::size_t i = 0; i < optional.size(); ++i)
        {
            const SectionLayout layout = findSection(encoded, optional[i].type);
            const TestLzssDecode decoded = decodeTestLzss(
                logicalStoredBytes(encoded, layout), optional[i].payload.size());
            const std::size_t target = i == 0 ? acceptedTarget : rejectedTarget;
            const auto token = std::find_if(decoded.tokens.begin(), decoded.tokens.end(),
                [&](const TestLzssToken& item)
                {
                    return item.match && item.decodedOffset == target;
                });
            if (i == 0)
            {
                accepted = decoded.bytes == optional[i].payload
                    && token != decoded.tokens.end()
                    && token->length >= pattern.size()
                    && token->backwardOffset == 65'535;
            }
            else
            {
                rejected = decoded.bytes == optional[i].payload
                    && token == decoded.tokens.end();
            }
        }
        check(accepted && rejected,
            "LZSS window: offset 65535 survives ring wrap and offset 65536 is rejected");
    }

    {
        // Exercise compression/decompression across the minimum block and
        // 16-bit-window boundaries without adding a large timing-sensitive test.
        constexpr std::array<std::size_t, 5> sizes{
            4096, 4097, 65'535, 65'536, 65'537 };
        std::vector<assets::SedOptionalSection> optional{};
        optional.reserve(sizes.size());
        for (std::size_t i = 0; i < sizes.size(); ++i)
        {
            assets::SedOptionalSection section{};
            section.type = { 'l', 'z', static_cast<char>('0' + i), 'd' };
            section.payload.resize(sizes[i]);
            // A non-power-of-two repeating period crosses both tested
            // boundaries while remaining compressible enough that the file
            // writer must select the LZSS representation.
            for (std::size_t j = 0; j < section.payload.size(); ++j)
            {
                const std::size_t phase = j % 257u;
                section.payload[j] = static_cast<std::byte>(static_cast<std::uint8_t>(
                    phase * 73u + (phase / 17u) * 29u + i * 11u));
            }
            optional.push_back(std::move(section));
        }
        const std::vector<std::byte> encoded = assets::saveToSedBytes(
            richLevel(ProjectType::IJIM), optional);
        bool allCompressed = true;
        for (const assets::SedOptionalSection& section : optional)
        {
            const SectionLayout layout = findSection(encoded, section.type);
            allCompressed = allCompressed
                && (readU32Le(std::span<const std::byte>(encoded),
                    layout.descriptorOffset + 16) & 1u) != 0;
        }
        const assets::SedFile decoded = assets::loadSedFileBytes(encoded);
        const bool sameOptionalSections = decoded.optionalSections.size() == optional.size()
            && std::equal(decoded.optionalSections.begin(), decoded.optionalSections.end(),
                optional.begin(), [](const assets::SedOptionalSection& left,
                    const assets::SedOptionalSection& right)
                {
                    return left.type == right.type && left.payload == right.payload;
                });
        check(allCompressed && sameOptionalSections,
            "LZSS differential: compressed boundary payloads round-trip exactly");
    }

    {
        // Production descriptors admit this bound first, but the decoder also
        // has direct test/fuzz seams. A forged size must fail before reserve().
        bool rejectedBeforeReserve = false;
        try
        {
            (void)assets::detail::decompressSedLzssForTesting({},
                static_cast<std::size_t>(
                    assets::SedMaximumHistorySectionBytes + 1u));
        }
        catch (const std::runtime_error& error)
        {
            rejectedBeforeReserve = exactSedText(error.what(), "Corrupted binary SED file: expanded compressed section exceeds the safe limit");
        }
        check(rejectedBeforeReserve,
            "SED LZSS allocation bound: oversized expansion is rejected before output reservation");
    }

    {
        // Pin the systematic 16+4 wire equations independently from the
        // decoder. Each byte column is one data-stream basis vector, so the
        // four parity chunks must expose these exact Vandermonde/MDS rows.
        std::string noise = deterministicNoise(16u * kTestFecPayloadBytes);
        assets::SedOptionalSection edit{};
        edit.type = { 'e', 'd', 'i', 't' };
        edit.payload.reserve(noise.size());
        for (const unsigned char value : noise)
            edit.payload.push_back(static_cast<std::byte>(value));
        for (std::size_t stream = 0; stream < 16; ++stream)
        {
            for (std::size_t column = 0; column < 16; ++column)
            {
                edit.payload[stream * kTestFecPayloadBytes + column]
                    = static_cast<std::byte>(stream == column ? 1u : 0u);
            }
        }
        const std::array optional{ edit };
        const std::vector<std::byte> encoded = assets::saveToSedBytes(
            richLevel(ProjectType::IJIM), optional);
        const SectionLayout layout = findSection(encoded, edit.type);
        constexpr std::array<std::array<std::uint8_t, 16>, 4> goldenRows{{
            {{ 0x21, 0xb5, 0xf6, 0x85, 0xdf, 0x02, 0xb7, 0x87,
                0x3e, 0xdd, 0x4a, 0xa4, 0x8d, 0xda, 0x61, 0x30 }},
            {{ 0xb5, 0x21, 0x85, 0xf6, 0x02, 0xdf, 0x87, 0xb7,
                0xdd, 0x3e, 0xa4, 0x4a, 0xda, 0x8d, 0x30, 0x61 }},
            {{ 0xf6, 0x85, 0x21, 0xb5, 0xb7, 0x87, 0xdf, 0x02,
                0x4a, 0xa4, 0x3e, 0xdd, 0x61, 0x30, 0x8d, 0xda }},
            {{ 0x85, 0xf6, 0xb5, 0x21, 0x87, 0xb7, 0x02, 0xdf,
                0xa4, 0x4a, 0xdd, 0x3e, 0x30, 0x61, 0xda, 0x8d }},
        }};
        bool goldenMatches = layout.dataStreams == 16
            && layout.parityStreams == 4 && layout.stripeCount == 1
            && layout.logicalStoredSize == edit.payload.size();
        for (std::size_t parity = 0; goldenMatches && parity < goldenRows.size(); ++parity)
        {
            const std::size_t offset = slotOffset(
                layout, layout.dataStreams + parity, 0);
            for (std::size_t column = 0; column < goldenRows[parity].size(); ++column)
            {
                if (std::to_integer<std::uint8_t>(encoded[offset + column])
                    != goldenRows[parity][column])
                {
                    goldenMatches = false;
                    break;
                }
            }
        }
        check(goldenMatches,
            "FEC: 16+4 systematic parity rows match the independent GF(256) golden");
    }

    {
        // Pin every adaptive k=1..16 production generator against an
        // independently implemented GF(256) construction. Enumerate every
        // one- through four-erasure survivor matrix without repeatedly decoding
        // complete Levels, then exercise one maximum-budget production repair
        // for each k. This closes the intermediate-k gap at low test cost.
        bool allAdaptiveGeneratorsMatch = true;
        bool allAdaptiveMatricesInvert = true;
        bool allAdaptiveRepairsSucceed = true;
        for (std::size_t dataStreams = 1; dataStreams <= 16; ++dataStreams)
        {
            const TestGfGenerator generator = independentTestFecGenerator(dataStreams);
            allAdaptiveMatricesInvert = allAdaptiveMatricesInvert
                && allTestFecErasureMatricesInvertible(generator, dataStreams);

            assets::SedOptionalSection edit{};
            edit.type = { 'e', 'd', 'i', 't' };
            edit.payload.resize(dataStreams * kTestFecPayloadBytes);
            std::uint32_t state = 0xa511'e9b3u;
            for (std::byte& value : edit.payload)
            {
                state ^= state << 13u;
                state ^= state >> 17u;
                state ^= state << 5u;
                value = static_cast<std::byte>(static_cast<std::uint8_t>(state));
            }
            for (std::size_t stream = 0; stream < dataStreams; ++stream)
            {
                for (std::size_t column = 0; column < dataStreams; ++column)
                {
                    edit.payload[stream * kTestFecPayloadBytes + column]
                        = static_cast<std::byte>(stream == column ? 1u : 0u);
                }
            }
            const std::vector<std::byte> encoded = assets::saveToSedBytes(
                richLevel(ProjectType::JKDF2),
                std::span<const assets::SedOptionalSection>(&edit, 1));
            const SectionLayout layout = findSection(encoded, edit.type);
            const bool layoutMatches = layout.dataStreams == dataStreams
                && layout.parityStreams == 4 && layout.stripeCount == 1
                && layout.logicalStoredSize == edit.payload.size();
            allAdaptiveGeneratorsMatch = allAdaptiveGeneratorsMatch && layoutMatches;
            if (!layoutMatches) continue;

            for (std::size_t parity = 0; parity < 4; ++parity)
            {
                const std::size_t offset = slotOffset(
                    layout, dataStreams + parity, 0);
                for (std::size_t column = 0; column < dataStreams; ++column)
                {
                    allAdaptiveGeneratorsMatch = allAdaptiveGeneratorsMatch
                        && std::to_integer<std::uint8_t>(encoded[offset + column])
                            == generator[dataStreams + parity][column];
                }
            }

            std::vector<std::byte> damaged = encoded;
            const std::array<std::size_t, 4> erased{ 0, 1, 2, 3 };
            damageSlots(damaged, layout, erased, 0, true);
            try
            {
                const assets::SedFile repaired = assets::loadSedFileBytes(damaged);
                allAdaptiveRepairsSucceed = allAdaptiveRepairsSucceed
                    && repaired.optionalSections.size() == 1
                    && repaired.optionalSections[0].type == edit.type
                    && repaired.optionalSections[0].payload == edit.payload
                    && repaired.repairedChunks == 4;
            }
            catch (const std::exception&)
            {
                allAdaptiveRepairsSucceed = false;
            }
        }
        check(allAdaptiveGeneratorsMatch,
            "FEC adaptive k=1..16: production parity matches independent systematic matrices");
        check(allAdaptiveMatricesInvert,
            "FEC adaptive k=1..16: every one- through four-erasure decode matrix is invertible");
        check(allAdaptiveRepairsSucceed,
            "FEC adaptive k=1..16: production repairs a maximum-budget stripe for every k");
    }

    {
        Level source = richLevel(ProjectType::MOTS);
        MotsArchLightObject original{};
        original.wireIndex = 7;
        original.meshes.push_back(MotsArchLightMesh{
            2, { MotsArchLightVertex{ -0.125f, 0.25f, 0.5f, 0.75f } } });
        MotsArchLightObject generated{};
        generated.wireIndex = 8;
        generated.meshes.push_back(MotsArchLightMesh{
            2, { MotsArchLightVertex{ 0.875f, 1.25f, 1.5f, 1.75f } } });
        source.motsArchLighting.push_back(generated);
        source.things[0].archLightingIndex = 0;
        source.motsArchLightingBakeBaselines.push_back(
            MotsArchLightingBakeBaseline{
                source.things[0].id, "actor.3do", 3, original });
        Thing unbound = source.things[0];
        unbound.id += 1;
        unbound.num += 1;
        unbound.name = "unbound_instance";
        unbound.archLightingIndex = -1;
        source.things.push_back(unbound);
        source.motsArchLightingBakeBaselines.push_back(
            MotsArchLightingBakeBaseline{
                unbound.id, "unbound.3do", -1, std::nullopt });

        const std::vector<std::byte> bytes = assets::saveToSedBytes(source);
        const Level loaded = assets::loadFromSedBytes(bytes);
        Level changedBaseline = loaded;
        bool changedBaselineDiffers = false;
        if (!changedBaseline.motsArchLightingBakeBaselines.empty())
        {
            ++changedBaseline.motsArchLightingBakeBaselines[0]
                .originalArchLightingIndex;
            changedBaselineDiffers =
                !assets::sedLevelsEqual(loaded, changedBaseline);
        }
        check(loaded.motsArchLightingBakeBaselines
                    == source.motsArchLightingBakeBaselines
                && assets::sedLevelsEqual(source, loaded)
                && changedBaselineDiffers,
            "SED MotS ARCH bake provenance: IDs, model, original -1/index reference, and authored object round-trip and compare semantically");

        Level absent = richLevel(ProjectType::MOTS);
        const std::vector<std::byte> oldBytes = assets::saveToSedBytes(absent);
        const Level oldLoaded = assets::loadFromSedBytes(oldBytes);
        check(oldLoaded.motsArchLightingBakeBaselines.empty()
                && assets::sedLevelsEqual(absent, oldLoaded),
            "SED MotS ARCH bake provenance: old files without the optional trailer remain readable");
    }

    {
        Level source = richLevel(ProjectType::IJIM);
        source.header.gravity = std::bit_cast<double>(0x8000'0000'0000'0000ull);
        const std::vector<std::byte> bytes = assets::saveToSedBytes(source);
        const Level loaded = assets::loadFromSedBytes(bytes);
        check(assets::sedLevelsEqual(source, loaded)
                && assets::saveToSedBytes(loaded) == bytes,
            "SED IEEE values: finite signed-zero bits round-trip exactly");
        Level changedZero = loaded;
        changedZero.header.gravity = 0.0;
        check(!assets::sedLevelsEqual(loaded, changedZero),
            "SED IEEE values: semantic equality distinguishes signed zero");

    }

    {
        const auto writerRejects = [](const Level& malformed)
        {
            try { (void)assets::saveToSedBytes(malformed); }
            catch (const std::exception&) { return true; }
            return false;
        };

        Level nonFinite = richLevel(ProjectType::IJIM);
        nonFinite.ppunit = std::numeric_limits<double>::quiet_NaN();
        Level invalidValueType = richLevel(ProjectType::IJIM);
        invalidValueType.templates[0].values[0].type = static_cast<ValueType>(12345);
        Level invalidAdjustmentType = richLevel(ProjectType::IJIM);
        invalidAdjustmentType.things[0].values[0].adjType = static_cast<AdjType>(12345);
        Level invalidCogType = richLevel(ProjectType::IJIM);
        invalidCogType.cogs[0].values[0].cogType = static_cast<CogType>(12345);
        Level duplicateArchBakeBaselines = richLevel(ProjectType::MOTS);
        duplicateArchBakeBaselines.motsArchLightingBakeBaselines = {
            MotsArchLightingBakeBaseline{
                duplicateArchBakeBaselines.things[0].id, "actor.3do", -1, std::nullopt },
            MotsArchLightingBakeBaseline{
                duplicateArchBakeBaselines.things[0].id, "actor.3do", -1, std::nullopt },
        };
        Level mismatchedArchBakeBaseline = richLevel(ProjectType::MOTS);
        MotsArchLightObject orphanedOriginal{};
        orphanedOriginal.wireIndex = 2;
        mismatchedArchBakeBaseline.motsArchLightingBakeBaselines.push_back(
            MotsArchLightingBakeBaseline{
                mismatchedArchBakeBaseline.things[0].id, "actor.3do", -1,
                orphanedOriginal });

        check(writerRejects(nonFinite)
                && writerRejects(invalidValueType)
                && writerRejects(invalidAdjustmentType)
                && writerRejects(invalidCogType)
                && writerRejects(duplicateArchBakeBaselines)
                && writerRejects(mismatchedArchBakeBaseline),
            "SED semantic validation: writer rejects malformed values and duplicate/inconsistent ARCH bake provenance");

        const auto writeU64 = [](std::vector<std::byte>& bytes,
            const std::size_t offset, const std::uint64_t value)
        {
            for (unsigned i = 0; i < 8; ++i)
            {
                bytes[offset + i] = static_cast<std::byte>(
                    static_cast<std::uint8_t>(value >> (i * 8)));
            }
        };
        const auto resealUnprotectedSection = [](std::vector<std::byte>& bytes,
            const SectionLayout& section)
        {
            constexpr std::size_t descriptorHashOffset = 64;
            constexpr std::size_t descriptorCrcOffset = 96;
            constexpr std::size_t mirroredDescriptorOffset = 12;
            const auto payload = std::span<const std::byte>(bytes).subspan(
                section.payloadOffset, static_cast<std::size_t>(section.storedSize));
            const auto digest = assets::calculateSedSha256(payload);
            std::copy(digest.begin(), digest.end(),
                bytes.begin() + static_cast<std::ptrdiff_t>(
                    section.descriptorOffset + descriptorHashOffset));
            const std::size_t mirror = section.trailerOffset
                + mirroredDescriptorOffset;
            std::copy(digest.begin(), digest.end(),
                bytes.begin() + static_cast<std::ptrdiff_t>(
                    mirror + descriptorHashOffset));
            writeU32Le(bytes, section.descriptorOffset + descriptorCrcOffset,
                assets::calculateSedCrc32c(std::span<const std::byte>(bytes)
                    .subspan(section.descriptorOffset, descriptorCrcOffset)));
            writeU32Le(bytes, mirror + descriptorCrcOffset,
                assets::calculateSedCrc32c(std::span<const std::byte>(bytes)
                    .subspan(mirror, descriptorCrcOffset)));
            writeU32Le(bytes,
                section.trailerOffset + kTestSectionTrailerSize - 4,
                assets::calculateSedCrc32c(std::span<const std::byte>(bytes)
                    .subspan(section.trailerOffset,
                        kTestSectionTrailerSize - 4)));
        };
        const auto nestedLoaderRejects = [&](const std::array<char, 4> type,
            const auto& mutate)
        {
            std::vector<std::byte> bytes = assets::saveToSedBytes(
                richLevel(ProjectType::IJIM), {},
                assets::SedSerializationProfile::UnprotectedNested);
            const SectionLayout section = findSection(bytes, type);
            mutate(bytes, section.payloadOffset);
            resealUnprotectedSection(bytes, section);
            try { (void)assets::loadNestedSedLevelBytes(bytes); }
            catch (const std::exception&) { return true; }
            return false;
        };

        const bool rejectedNonFinite = nestedLoaderRejects(
            { 'h', 'e', 'a', 'd' }, [&](std::vector<std::byte>& bytes,
                const std::size_t payload)
            {
                writeU64(bytes, payload + 8,
                    std::bit_cast<std::uint64_t>(
                        std::numeric_limits<double>::quiet_NaN()));
            });
        const bool rejectedValueType = nestedLoaderRejects(
            { 't', 'm', 'p', 'l' }, [&](std::vector<std::byte>& bytes,
                const std::size_t payload)
            {
                std::size_t cursor = payload + 4; // template count
                skipSedIndex(bytes, cursor); // name
                skipSedIndex(bytes, cursor); // parent
                cursor += 4; // parameter count
                cursor += 4; // adjType
                skipSedIndex(bytes, cursor); // parameter name
                writeU32Le(bytes, cursor, 12345u);
            });
        const bool rejectedAdjustmentType = nestedLoaderRejects(
            { 't', 'h', 'n', 'g' }, [&](std::vector<std::byte>& bytes,
                const std::size_t payload)
            {
                std::size_t cursor = payload + 4 + 4; // count and id
                skipSedIndex(bytes, cursor); // thing number
                skipSedIndex(bytes, cursor); // sector
                skipSedIndex(bytes, cursor); // template
                skipSedIndex(bytes, cursor); // instance name
                cursor += 6 * 8 + 4; // pose and value count
                writeU32Le(bytes, cursor, 12345u);
            });
        const bool rejectedCogType = nestedLoaderRejects(
            { 'c', 'o', 'g', 's' }, [&](std::vector<std::byte>& bytes,
                const std::size_t payload)
            {
                std::size_t cursor = payload + 4 + 4; // count and id
                skipSedIndex(bytes, cursor); // COG number
                skipSedIndex(bytes, cursor); // script
                cursor += 4; // value count
                writeU32Le(bytes, cursor, 12345u);
            });
        const bool rejectedNonCanonicalResourceIndex = nestedLoaderRejects(
            { 't', 'm', 'p', 'l' }, [&](std::vector<std::byte>& bytes,
                const std::size_t payload)
            {
                // The first template name reference is canonically one byte.
                // An overlong zero must fail before it can shift interpretation
                // of the fields that follow it.
                bytes[payload + 4] = std::byte{ 0x80 };
                bytes[payload + 5] = std::byte{ 0x00 };
            });
        const bool rejectedNonCanonicalGeometryIndex = nestedLoaderRejects(
            { 's', 'e', 'c', 't' }, [&](std::vector<std::byte>& bytes,
                const std::size_t payload)
            {
                // sector count (4) + stable id (4), then sector.num.
                bytes[payload + 8] = std::byte{ 0x80 };
                bytes[payload + 9] = std::byte{ 0x00 };
            });
        const bool rejectedNonCanonicalCogEntityIndex = nestedLoaderRejects(
            { 'c', 'o', 'g', 's' }, [&](std::vector<std::byte>& bytes,
                const std::size_t payload)
            {
                std::size_t cursor = payload + 4 + 4; // count and stable id
                skipSedIndex(bytes, cursor); // COG number
                skipSedIndex(bytes, cursor); // script reference
                cursor += 4; // value count
                cursor += 4; // CogType::surface
                skipSedIndex(bytes, cursor); // parameter-name reference
                cursor += 4; // ValueType::ptr
                // The linked surface is a SED index. Replace its canonical
                // two-byte value with an overlong zero of the same width.
                bytes[cursor] = std::byte{ 0x80 };
                bytes[cursor + 1] = std::byte{ 0x00 };
            });
        check(rejectedNonFinite && rejectedValueType
                && rejectedAdjustmentType && rejectedCogType
                && rejectedNonCanonicalResourceIndex
                && rejectedNonCanonicalGeometryIndex
                && rejectedNonCanonicalCogEntityIndex,
            "SED semantic validation: fully resealed hostile sections are rejected by the loader");

        Level invalidIndex = richLevel(ProjectType::IJIM);
        invalidIndex.things[0].sector = -2;
        bool writerRejectedIndexBelowSentinel = false;
        try
        {
            (void)assets::saveToSedBytes(invalidIndex);
        }
        catch (const std::runtime_error& error)
        {
            writerRejectedIndexBelowSentinel = exactSedText(error.what(), "Binary SED index is below the reserved -1 sentinel.");
        }
        check(writerRejectedIndexBelowSentinel,
            "SED index sentinel: writer rejects -2 while preserving -1 as the sole not-set value");

        // A compact count field must be rejected by the shared semantic
        // admission budget before vector::reserve can turn it into a large
        // allocation. Exercise both a resource string vector and a vector
        // nested inside one otherwise valid Thing.
        const auto semanticBudgetRejects = [&](const std::array<char, 4> type,
            const std::string_view expectedError, const auto& mutate)
        {
            std::vector<std::byte> bytes = assets::saveToSedBytes(
                richLevel(ProjectType::IJIM), {},
                assets::SedSerializationProfile::UnprotectedNested);
            const SectionLayout section = findSection(bytes, type);
            mutate(bytes, section.payloadOffset);
            resealUnprotectedSection(bytes, section);
            try
            {
                (void)assets::loadNestedSedLevelBytes(bytes);
            }
            catch (const std::runtime_error& error)
            {
                return exactSedText(error.what(), expectedError);
            }
            return false;
        };
        const bool rejectedCompactResourceAmplification = semanticBudgetRejects(
            { 'r', 's', 'r', 'c' },
            "Invalid binary SED file: semantic decoded-item work exceeds the safe limit",
            [&](std::vector<std::byte>& bytes,
                const std::size_t payload)
            {
                writeU32Le(bytes, payload, static_cast<std::uint32_t>(
                    assets::SedMaximumDecodedItems));
            });
        const bool rejectedNestedCollectionAmplification = semanticBudgetRejects(
            { 's', 'e', 'c', 't' },
            "Invalid binary SED file: semantic decoded-item work exceeds the safe limit",
            [&](std::vector<std::byte>& bytes,
                const std::size_t payload)
            {
                std::size_t cursor = payload + 4u; // sector count
                cursor += 4u; // id
                skipSedIndex(bytes, cursor); // sector number
                cursor += 4u; // flags
                cursor += 3u * 4u * 4u; // ambient, extra, point-light colors
                cursor += 3u * 8u + 2u * 8u; // point-light position/ranges
                skipSedIndex(bytes, cursor); // colormap resource reference
                cursor += 4u * 4u; // tint
                skipSedIndex(bytes, cursor); // sound resource reference
                cursor += 8u + 3u * 8u + 6u * 8u; // volume, thrust, box
                skipSedIndex(bytes, cursor); // layer
                cursor += 4u; // mark
                writeU32Le(bytes, cursor, static_cast<std::uint32_t>(
                    assets::SedMaximumDecodedItems)); // Sector vertices
            });
        const bool rejectedDecodedObjectStorageAmplification = semanticBudgetRejects(
            { 's', 'e', 'c', 't' },
            "Invalid binary SED file: semantic decoded-object storage exceeds the safe limit",
            [&](std::vector<std::byte>& bytes, const std::size_t payload)
            {
                // Below the aggregate item ceiling, but count * sizeof(Sector)
                // is above the 128 MiB decoded-object ceiling. Admission must
                // fail before vector::reserve sees this count.
                writeU32Le(bytes, payload, 1'000'000u);
            });
        check(rejectedCompactResourceAmplification
                && rejectedNestedCollectionAmplification
                && rejectedDecodedObjectStorageAmplification,
            "SED semantic budget: compact resource, nested collection, and decoded-object amplification reject before allocation");

        Level writerAmplification = richLevel(ProjectType::IJIM);
        writerAmplification.sectors[0].surfaces[0].vertices.resize(
            static_cast<std::size_t>(assets::SedMaximumDecodedItems));
        const assets::SedLevelDecodedFootprint writerFootprint =
            assets::estimateSedLevelDecodedFootprint(writerAmplification);
        bool writerBudgetRejected = false;
        try
        {
            (void)assets::saveToSedBytes(writerAmplification);
        }
        catch (const std::runtime_error& error)
        {
            writerBudgetRejected = exactSedText(error.what(), "Invalid binary SED file: semantic decoded-item work exceeds the safe limit");
        }
        check(writerFootprint.itemCount > assets::SedMaximumDecodedItems
                && writerBudgetRejected,
            "SED semantic budget: estimator exposes and writer rejects a Level that the bounded reader could not reopen");
    }

    for (const auto& [kind, name] : {
             std::pair{ ProjectType::JKDF2, std::string("SED JKDF2") },
             std::pair{ ProjectType::MOTS, std::string("SED MotS") },
             std::pair{ ProjectType::IJIM, std::string("SED IJIM") } })
    {
        const Level source = richLevel(kind);
        const std::vector<std::byte> bytes = assets::saveToSedBytes(source);
        check(assets::hasSedBinarySignature(bytes), name + ": binary signature");
        check(bytes.size() >= 8
                && bytes[0] == std::byte{'s'}
                && bytes[1] == std::byte{'e'}
                && bytes[2] == std::byte{'d'}
                && bytes[3] == std::byte{0}
                && bytes[4] == std::byte{1}
                && bytes[5] == std::byte{0}
                && bytes[6] == std::byte{0}
                && bytes[7] == std::byte{0},
            name + ": file begins with exact four-byte sed\\0 magic followed by V1");
        check(bytes.size() > 100, name + ": non-empty versioned container");
        for (const std::array<char, 4> required : {
                 std::array<char, 4>{ 'h', 'e', 'a', 'd' },
                 std::array<char, 4>{ 'r', 's', 'r', 'c' },
                 std::array<char, 4>{ 's', 'e', 'c', 't' },
                 std::array<char, 4>{ 't', 'h', 'n', 'g' },
                 std::array<char, 4>{ 't', 'm', 'p', 'l' },
                 std::array<char, 4>{ 'c', 'o', 'g', 's' },
                 std::array<char, 4>{ 'l', 'i', 't', 'e' },
                 std::array<char, 4>{ 'h', 'n', 'o', 'd' } })
        {
            const SectionLayout layout = findSection(bytes, required);
            check(layout.parityStreams == 4,
                name + ": every required Level section uses the p4 protection tier");
        }
        const Level loaded = assets::loadFromSedBytes(bytes);
        verifyCriticalFields(loaded, kind, name);
        verifyCompleteLevelRoundTrip(source, loaded, name);
        check(assets::sedLevelsEqual(source, loaded),
            name + ": exact semantic Level comparison covers the complete model");
        check(assets::saveToSedBytes(loaded) == bytes, name + ": deterministic byte-exact round trip");

        std::vector<std::byte> damagedHeader = bytes;
        damagedHeader[0] ^= std::byte{ 0x01 };
        const assets::SedFile repairedHeader = assets::loadSedFileBytes(damagedHeader);
        check(assets::saveToSedBytes(repairedHeader.level) == bytes
                && repairedHeader.hadCorruption,
            name + ": primary fixed-header damage is repaired from its mirror");

        std::vector<std::byte> shortBytes(bytes.begin() + 1, bytes.end());
        shortBytes[0] = std::byte{'s'};
        check(!assets::hasSedBinarySignature(shortBytes),
            name + ": three-byte lowercase development signature is not V1");
        bool rejected = false;
        try { (void)assets::loadFromSedBytes(shortBytes); }
        catch (const std::exception&) { rejected = true; }
        check(rejected, name + ": three-byte development signature is rejected");

        std::vector<std::byte> legacyBytes{
            std::byte{'S'}, std::byte{'E'}, std::byte{'D'}, std::byte{'B'},
            std::byte{'I'}, std::byte{'N'}, std::byte{0x0d}, std::byte{0x0a} };
        legacyBytes.insert(legacyBytes.end(), bytes.begin() + 4, bytes.end());
        check(!assets::hasSedBinarySignature(legacyBytes),
            name + ": SEDBIN development signature is not V1");
        rejected = false;
        try { (void)assets::loadFromSedBytes(legacyBytes); }
        catch (const std::exception&) { rejected = true; }
        check(rejected, name + ": SEDBIN development signature is rejected");

        const auto sections = assets::splitSedFileSections(bytes);
        check(sections.size() == 9, name + ": fixed header and eight semantic sections split independently");
        if (sections.size() == 9)
        {
            std::vector<std::byte> damagedHeaderForSplit = bytes;
            damagedHeaderForSplit[0] ^= std::byte{ 0x01 };
            const auto repairedHeaderSections = assets::splitSedFileSections(
                damagedHeaderForSplit);
            check(repairedHeaderSections == sections,
                name + ": splitter emits the checked mirror header when the primary magic is damaged");

            std::vector<std::byte> withoutHeader{};
            for (std::size_t i = 1; i < sections.size(); ++i)
            {
                withoutHeader.insert(withoutHeader.end(), sections[i].begin(), sections[i].end());
            }
            const auto recoveredHeader = assets::recoverFromSedBytes(withoutHeader);
            check(recoveredHeader.level.kind == kind
                    && recoveredHeader.level.sectors.size() == 1
                    && assets::saveToSedBytes(recoveredHeader.level) == bytes
                    && std::find(recoveredHeader.missingSections.begin(),
                        recoveredHeader.missingSections.end(), "head")
                        == recoveredHeader.missingSections.end(),
                name + ": a lost fixed header is reconstructed from checked HEAD data");

            std::vector<std::byte> prefixedDocument{
                std::byte{0x52}, std::byte{0x45}, std::byte{0x43}, std::byte{0x56},
                std::byte{0x01}, std::byte{0x00}, std::byte{0x7f} };
            prefixedDocument.insert(prefixedDocument.end(), bytes.begin(), bytes.end());
            const auto recoveredEmbedded = assets::recoverFromSedBytes(prefixedDocument);
            check(assets::saveToSedBytes(recoveredEmbedded.level) == bytes
                    && recoveredEmbedded.level.kind == kind,
                name + ": recovery prefers an intact embedded document over prefix bytes");

            constexpr std::size_t trailingHeaderMirrorAndFooter = 40 + 8;
            std::vector<std::byte> bareSections = withoutHeader;
            check(bareSections.size() > trailingHeaderMirrorAndFooter,
                name + ": headerless fixture contains removable framing");
            bareSections.resize(bareSections.size() - trailingHeaderMirrorAndFooter);
            const auto recoveredBareSections = assets::recoverFromSedBytes(bareSections);
            check(recoveredBareSections.level.kind == kind
                    && recoveredBareSections.level.sectors.size() == 1
                    && assets::saveToSedBytes(recoveredBareSections.level) == bytes
                    && recoveredBareSections.missingRequiredSections.empty(),
                name + ": headerless section stream without mirror/footer is reframed in order");

            // Lose the first descriptor and its redundant trailer in a
            // headerless stream. Keep the damaged prefix bytes in place so
            // later descriptors retain their canonical (+28) FEC phase.
            std::vector<std::byte> lostFirstDescriptor = bareSections;
            const SectionLayout firstHeaderless = findSection(bytes,
                { 'h', 'e', 'a', 'd' });
            check(lostFirstDescriptor.size() >= kTestSectionDescriptorSize,
                name + ": first headerless descriptor fixture is in range");
            if (lostFirstDescriptor.size() >= kTestSectionDescriptorSize)
            {
                std::fill(lostFirstDescriptor.begin(),
                    lostFirstDescriptor.begin()
                        + static_cast<std::ptrdiff_t>(kTestSectionDescriptorSize),
                    std::byte{0});
                const std::size_t firstTrailer = firstHeaderless.trailerOffset
                    - kTestHeaderSize;
                if (firstTrailer + kTestSectionTrailerSize <= lostFirstDescriptor.size())
                {
                    std::fill(lostFirstDescriptor.begin()
                            + static_cast<std::ptrdiff_t>(firstTrailer),
                        lostFirstDescriptor.begin()
                            + static_cast<std::ptrdiff_t>(firstTrailer
                                + kTestSectionTrailerSize), std::byte{0});
                }
            }
            const auto recoveredLostFirst = assets::recoverFromSedBytes(
                lostFirstDescriptor);
            check(!recoveredLostFirst.projectType.has_value()
                    && !recoveredLostFirst.projectTypeTrusted
                    && recoveredLostFirst.level.things.size() == source.things.size()
                    && recoveredLostFirst.level.h3doNodes.size() == source.h3doNodes.size()
                    && std::find(recoveredLostFirst.missingRequiredSections.begin(),
                        recoveredLostFirst.missingRequiredSections.end(), "head")
                        != recoveredLostFirst.missingRequiredSections.end(),
                name + ": first headerless descriptor loss preserves later absolute FEC alignment without inventing a project type");

            // A payload occurrence with a valid descriptor CRC is not an
            // anchor unless its exact mirrored trailer and absolute alignment
            // phase also validate.
            std::vector<std::byte> falseDescriptorAnchor = lostFirstDescriptor;
            const SectionLayout resourceSection = findSection(bytes,
                { 'r', 's', 'r', 'c' });
            constexpr std::size_t falseAnchorOffset = 200;
            if (falseAnchorOffset + kTestSectionDescriptorSize
                    <= falseDescriptorAnchor.size())
            {
                std::copy_n(bytes.begin()
                        + static_cast<std::ptrdiff_t>(resourceSection.descriptorOffset),
                    kTestSectionDescriptorSize,
                    falseDescriptorAnchor.begin()
                        + static_cast<std::ptrdiff_t>(falseAnchorOffset));
            }
            const auto recoveredFalseAnchor = assets::recoverFromSedBytes(
                falseDescriptorAnchor);
            check(recoveredFalseAnchor.level.things.size() == source.things.size()
                    && recoveredFalseAnchor.level.h3doNodes.size() == source.h3doNodes.size()
                    && std::find(recoveredFalseAnchor.missingRequiredSections.begin(),
                        recoveredFalseAnchor.missingRequiredSections.end(), "head")
                        != recoveredFalseAnchor.missingRequiredSections.end(),
                name + ": descriptor-like payload bytes cannot become a resync anchor");

            // Bootstrap a headerless stream when every primary descriptor is
            // lost but each checked trailer survives. The earliest mirrored
            // descriptor must provide the initial absolute phase.
            std::vector<std::byte> allPrimaryDescriptorsLost = bareSections;
            for (std::size_t fragment = 0; fragment < sections.size() - 1; ++fragment)
            {
                const SectionLayout original = findSection(bytes,
                    fragment == 0 ? std::array<char, 4>{ 'h', 'e', 'a', 'd' }
                        : fragment == 1 ? std::array<char, 4>{ 'r', 's', 'r', 'c' }
                        : fragment == 2 ? std::array<char, 4>{ 's', 'e', 'c', 't' }
                        : fragment == 3 ? std::array<char, 4>{ 't', 'h', 'n', 'g' }
                        : fragment == 4 ? std::array<char, 4>{ 't', 'm', 'p', 'l' }
                        : fragment == 5 ? std::array<char, 4>{ 'c', 'o', 'g', 's' }
                        : fragment == 6 ? std::array<char, 4>{ 'l', 'i', 't', 'e' }
                        : std::array<char, 4>{ 'h', 'n', 'o', 'd' });
                const std::size_t descriptorOffset = original.descriptorOffset
                    - kTestHeaderSize;
                if (descriptorOffset + kTestSectionDescriptorSize
                    <= allPrimaryDescriptorsLost.size())
                {
                    std::fill(allPrimaryDescriptorsLost.begin()
                            + static_cast<std::ptrdiff_t>(descriptorOffset),
                        allPrimaryDescriptorsLost.begin()
                            + static_cast<std::ptrdiff_t>(descriptorOffset
                                + kTestSectionDescriptorSize), std::byte{0});
                }
            }
            const auto recoveredAllPrimary = assets::recoverFromSedBytes(
                allPrimaryDescriptorsLost);
            check(recoveredAllPrimary.level.kind == kind
                    && recoveredAllPrimary.level.things.size() == source.things.size()
                    && recoveredAllPrimary.level.h3doNodes.size() == source.h3doNodes.size()
                    && recoveredAllPrimary.missingRequiredSections.empty(),
                name + ": checked trailers bootstrap all-primary-descriptor loss");

            // A damaged primary descriptor must not truncate healthy sections
            // that follow it when the fixed header mirror is also absent. The
            // scanner can recover the damaged section boundary from its
            // checked trailer and continue with the remaining Level records.
            std::vector<std::byte> damagedBareSections{};
            for (std::size_t fragment = 1; fragment < sections.size(); ++fragment)
            {
                std::vector<std::byte> copy = sections[fragment];
                if (fragment == 4 && copy.size() > 28) // THNG primary descriptor
                {
                    copy[28] ^= std::byte{ 0x40 };
                }
                damagedBareSections.insert(damagedBareSections.end(),
                    copy.begin(), copy.end());
            }
            const auto recoveredDamagedBare = assets::recoverFromSedBytes(
                damagedBareSections);
            check(recoveredDamagedBare.level.kind == kind
                    && recoveredDamagedBare.level.things.size() == 1
                    && recoveredDamagedBare.level.h3doNodes.size() == 1
                    && recoveredDamagedBare.missingRequiredSections.empty(),
                name + ": headerless damaged primary resumes at its checked trailer");

            std::vector<std::byte> withoutTail(bytes.begin(),
                bytes.end() - static_cast<std::ptrdiff_t>(trailingHeaderMirrorAndFooter));
            const auto recoveredWithoutTail = assets::recoverFromSedBytes(withoutTail);
            check(recoveredWithoutTail.level.h3doNodes.size() == 1
                    && std::find(recoveredWithoutTail.missingSections.begin(),
                        recoveredWithoutTail.missingSections.end(), "hnod")
                        == recoveredWithoutTail.missingSections.end(),
                name + ": removing exactly mirror+footer preserves final HNOD section");

            std::vector<std::byte> badHeaderNoTail = withoutTail;
            badHeaderNoTail[16] ^= std::byte{ 0x20 }; // magic survives; CRC does not
            const auto recoveredBadHeaderNoTail = assets::recoverFromSedBytes(
                badHeaderNoTail);
            check(recoveredBadHeaderNoTail.level.kind == kind
                    && recoveredBadHeaderNoTail.level.h3doNodes.size() == 1
                    && recoveredBadHeaderNoTail.missingRequiredSections.empty(),
                name + ": intact magic with damaged header/absent tail resyncs sections");

            std::vector<std::byte> damagedMirrorMarker = bytes;
            damagedMirrorMarker[damagedMirrorMarker.size()
                - trailingHeaderMirrorAndFooter] ^= std::byte{ 0x01 };
            bool damagedMirrorRejected = false;
            try { (void)assets::loadFromSedBytes(damagedMirrorMarker); }
            catch (const std::exception&) { damagedMirrorRejected = true; }
            const auto recoveredDamagedMirror = assets::recoverFromSedBytes(
                damagedMirrorMarker);
            check(damagedMirrorRejected
                    && recoveredDamagedMirror.hadCorruption
                    && !recoveredDamagedMirror.partialRecoveryFromCorruption
                    && recoveredDamagedMirror.unidentifiedMissingSections == 0
                    && recoveredDamagedMirror.missingRequiredSections.empty(),
                name + ": damaged mirror marker is strict-fail but preserves all sections");

            // A non-final trailer is redundant fixed-width framing. A fully
            // erased trailer must not make the decoder skip the immediately
            // following Level section; resynchronization uses that section's
            // checked primary/mirror anchor instead.
            const SectionLayout nonFinalHead = findSection(bytes,
                { 'h', 'e', 'a', 'd' });
            std::vector<std::byte> zeroedNonFinalTrailer = bytes;
            std::fill_n(zeroedNonFinalTrailer.begin()
                    + static_cast<std::ptrdiff_t>(nonFinalHead.trailerOffset),
                kTestSectionTrailerSize, std::byte{0});
            bool zeroedNonFinalRejected = false;
            try { (void)assets::loadFromSedBytes(zeroedNonFinalTrailer); }
            catch (const std::exception&) { zeroedNonFinalRejected = true; }
            const auto recoveredZeroedNonFinal = assets::recoverFromSedBytes(
                zeroedNonFinalTrailer);
            check(zeroedNonFinalRejected
                    && recoveredZeroedNonFinal.level.materials.size()
                        == source.materials.size()
                    && recoveredZeroedNonFinal.level.h3doNodes.size()
                        == source.h3doNodes.size()
                    && recoveredZeroedNonFinal.missingRequiredSections.empty(),
                name + ": zeroed non-final trailer resynchronizes the next Level section");

            // A trailer whose CRC alone is damaged has the same fixed extent;
            // its embedded descriptor copy is not a valid next anchor.
            std::vector<std::byte> crcDamagedNonFinalTrailer = bytes;
            crcDamagedNonFinalTrailer[nonFinalHead.trailerOffset
                + kTestSectionTrailerSize - 4] ^= std::byte{ 0x80 };
            bool crcDamagedNonFinalRejected = false;
            try { (void)assets::loadFromSedBytes(crcDamagedNonFinalTrailer); }
            catch (const std::exception&) { crcDamagedNonFinalRejected = true; }
            const auto recoveredCrcDamagedNonFinal = assets::recoverFromSedBytes(
                crcDamagedNonFinalTrailer);
            check(crcDamagedNonFinalRejected
                    && recoveredCrcDamagedNonFinal.level.materials.size()
                        == source.materials.size()
                    && recoveredCrcDamagedNonFinal.level.h3doNodes.size()
                        == source.h3doNodes.size()
                    && recoveredCrcDamagedNonFinal.missingRequiredSections.empty(),
                name + ": CRC-damaged non-final trailer resynchronizes the next Level section");

            // A valid footer must not cause a missing or short mirror to be
            // reserved as phantom semantic bytes. Recovery stops at the
            // footer, preserving the complete final HNOD payload; strict
            // opening still rejects the missing framing unit.
            const std::size_t canonicalMirrorOffset = bytes.size()
                - trailingHeaderMirrorAndFooter;
            std::vector<std::byte> missingMirror = bytes;
            missingMirror.erase(missingMirror.begin()
                    + static_cast<std::ptrdiff_t>(canonicalMirrorOffset),
                missingMirror.begin() + static_cast<std::ptrdiff_t>(
                    canonicalMirrorOffset + 40));
            bool missingMirrorRejected = false;
            try { (void)assets::loadFromSedBytes(missingMirror); }
            catch (const std::exception&) { missingMirrorRejected = true; }
            const auto recoveredMissingMirror = assets::recoverFromSedBytes(missingMirror);
            check(missingMirrorRejected
                    && recoveredMissingMirror.level.h3doNodes.size()
                        == source.h3doNodes.size()
                    && recoveredMissingMirror.missingRequiredSections.empty(),
                name + ": valid footer plus absent mirror preserves final HNOD");

            std::vector<std::byte> shortMirror = bytes;
            shortMirror.erase(shortMirror.begin()
                    + static_cast<std::ptrdiff_t>(canonicalMirrorOffset + 17),
                shortMirror.begin() + static_cast<std::ptrdiff_t>(
                    canonicalMirrorOffset + 40));
            bool shortMirrorRejected = false;
            try { (void)assets::loadFromSedBytes(shortMirror); }
            catch (const std::exception&) { shortMirrorRejected = true; }
            const auto recoveredShortMirror = assets::recoverFromSedBytes(shortMirror);
            check(shortMirrorRejected
                    && recoveredShortMirror.hadCorruption
                    && !recoveredShortMirror.partialRecoveryFromCorruption
                    && recoveredShortMirror.unidentifiedMissingSections == 0
                    && recoveredShortMirror.level.h3doNodes.size()
                        == source.h3doNodes.size()
                    && recoveredShortMirror.missingRequiredSections.empty(),
                name + ": valid footer plus short mirror preserves final HNOD");

            // A crash can truncate both the mirrored header and footer after
            // every declared section is durable.  A surviving canonical
            // prefix is redundant framing damage, not an unidentified lost
            // semantic section.
            std::vector<std::byte> truncatedMirrorAndFooter = bytes;
            truncatedMirrorAndFooter.resize(
                canonicalMirrorOffset + 14);
            bool truncatedMirrorAndFooterRejected = false;
            try { (void)assets::loadFromSedBytes(truncatedMirrorAndFooter); }
            catch (const std::exception&) { truncatedMirrorAndFooterRejected = true; }
            const auto recoveredTruncatedMirrorAndFooter =
                assets::recoverFromSedBytes(truncatedMirrorAndFooter);
            check(truncatedMirrorAndFooterRejected
                    && recoveredTruncatedMirrorAndFooter.hadCorruption
                    && !recoveredTruncatedMirrorAndFooter.partialRecoveryFromCorruption
                    && recoveredTruncatedMirrorAndFooter.unidentifiedMissingSections == 0
                    && recoveredTruncatedMirrorAndFooter.level.h3doNodes.size()
                        == source.h3doNodes.size()
                    && recoveredTruncatedMirrorAndFooter.missingRequiredSections.empty(),
                name + ": truncated canonical mirror/footer prefix is framing-only damage");

            std::vector<std::byte> corruptMirrorCrc = bytes;
            corruptMirrorCrc[canonicalMirrorOffset + 47] ^= std::byte{ 0x40 };
            bool corruptMirrorRejected = false;
            try { (void)assets::loadFromSedBytes(corruptMirrorCrc); }
            catch (const std::exception&) { corruptMirrorRejected = true; }
            const auto recoveredCorruptMirror = assets::recoverFromSedBytes(
                corruptMirrorCrc);
            check(corruptMirrorRejected
                    && recoveredCorruptMirror.hadCorruption
                    && !recoveredCorruptMirror.partialRecoveryFromCorruption
                    && recoveredCorruptMirror.level.h3doNodes.size()
                        == source.h3doNodes.size()
                    && recoveredCorruptMirror.missingRequiredSections.empty(),
                name + ": valid footer plus corrupt mirror preserves final HNOD");

            // Footer damage must not hide a still-valid checked header mirror.
            // Strict opening rejects the damaged framing; permissive recovery
            // independently probes the mirror and keeps the complete final
            // HNOD payload.
            std::vector<std::byte> footerBitFlip = bytes;
            footerBitFlip.back() ^= std::byte{0x01};
            bool footerRejected = false;
            try { (void)assets::loadFromSedBytes(footerBitFlip); }
            catch (const std::exception&) { footerRejected = true; }
            const auto recoveredFooterBitFlip = assets::recoverFromSedBytes(footerBitFlip);
            check(footerRejected
                    && recoveredFooterBitFlip.level.h3doNodes.size()
                        == source.h3doNodes.size()
                    && recoveredFooterBitFlip.missingRequiredSections.empty(),
                name + ": footer bit flip is strict-fail but mirror-probed recovery keeps tail");

            // Dispatch must still trust a valid near-EOF mirror when the
            // primary header, footer, and one semantic unit are damaged at
            // the same time.
            std::vector<std::byte> combinedFramingDamage = bytes;
            combinedFramingDamage[16] ^= std::byte{0x20}; // primary CRC
            combinedFramingDamage.back() ^= std::byte{0x01}; // footer
            const SectionLayout combinedHead = findSection(bytes,
                { 'h', 'e', 'a', 'd' });
            damageFirstSlots(combinedFramingDamage, combinedHead, 5);
            const auto recoveredCombined = assets::recoverFromSedBytes(
                combinedFramingDamage);
            check(recoveredCombined.level.kind == kind
                    && recoveredCombined.level.h3doNodes.size()
                        == source.h3doNodes.size()
                    && std::find(recoveredCombined.missingRequiredSections.begin(),
                        recoveredCombined.missingRequiredSections.end(), "head")
                        != recoveredCombined.missingRequiredSections.end(),
                name + ": near-EOF mirror dispatch survives primary/footer/HEAD damage");

            std::vector<std::byte> combinedMagicDamage = bytes;
            combinedMagicDamage[0] ^= std::byte{0x01}; // primary magic
            combinedMagicDamage[16] ^= std::byte{0x20}; // primary CRC
            combinedMagicDamage.back() ^= std::byte{0x02}; // footer
            damageFirstSlots(combinedMagicDamage, combinedHead, 5);
            const auto recoveredCombinedMagic = assets::recoverFromSedBytes(
                combinedMagicDamage);
            check(recoveredCombinedMagic.level.kind == kind
                    && recoveredCombinedMagic.level.h3doNodes.size()
                        == source.h3doNodes.size()
                    && std::find(recoveredCombinedMagic.missingRequiredSections.begin(),
                        recoveredCombinedMagic.missingRequiredSections.end(), "head")
                        != recoveredCombinedMagic.missingRequiredSections.end(),
                name + ": valid near-EOF mirror survives primary magic/header/footer/HEAD damage");

            std::vector<std::byte> truncatedFooter = bytes;
            truncatedFooter.pop_back();
            bool truncatedFooterRejected = false;
            try { (void)assets::loadFromSedBytes(truncatedFooter); }
            catch (const std::exception&) { truncatedFooterRejected = true; }
            const auto recoveredTruncatedFooter = assets::recoverFromSedBytes(truncatedFooter);
            check(truncatedFooterRejected
                    && recoveredTruncatedFooter.level.h3doNodes.size()
                        == source.h3doNodes.size()
                    && recoveredTruncatedFooter.missingRequiredSections.empty(),
                name + ": truncated footer is strict-fail but mirror-probed recovery keeps tail");

            // If both fixed headers and the HEAD semantic section disappear,
            // the scanner may still recover the other Level sections, but it
            // must not silently label the result as JKDF2.
            std::vector<std::byte> unknownProject = bytes;
            std::fill(unknownProject.begin(),
                unknownProject.begin() + static_cast<std::ptrdiff_t>(kTestHeaderSize),
                std::byte{0});
            const SectionLayout headSection = findSection(bytes, { 'h', 'e', 'a', 'd' });
            std::fill(unknownProject.begin()
                    + static_cast<std::ptrdiff_t>(headSection.descriptorOffset),
                unknownProject.begin()
                    + static_cast<std::ptrdiff_t>(headSection.descriptorOffset
                        + kTestSectionDescriptorSize), std::byte{0});
            std::fill(unknownProject.begin()
                    + static_cast<std::ptrdiff_t>(headSection.trailerOffset),
                unknownProject.begin()
                    + static_cast<std::ptrdiff_t>(headSection.trailerOffset
                        + kTestSectionTrailerSize), std::byte{0});
            std::fill(unknownProject.end()
                    - static_cast<std::ptrdiff_t>(32 + 8),
                unknownProject.end(), std::byte{0});
            const auto unknownProjectRecovery = assets::recoverFromSedBytes(unknownProject);
            check(!unknownProjectRecovery.projectType.has_value()
                    && !unknownProjectRecovery.projectTypeTrusted
                    && unknownProjectRecovery.level.sectors.size() == source.sectors.size(),
                name + ": recovery exposes unknown project type when headers and HEAD are lost");

            // Headerless resynchronization must survive a middle section whose
            // primary and mirrored descriptors are both gone. The untouched
            // later descriptor retains its absolute offset, so its 4 KiB FEC
            // alignment remains valid.
            std::vector<std::byte> lostMiddleDescriptors = bytes;
            std::fill(lostMiddleDescriptors.begin(),
                lostMiddleDescriptors.begin()
                    + static_cast<std::ptrdiff_t>(kTestHeaderSize), std::byte{0});
            const SectionLayout middle = findSection(bytes, { 's', 'e', 'c', 't' });
            std::fill(lostMiddleDescriptors.begin()
                    + static_cast<std::ptrdiff_t>(middle.descriptorOffset),
                lostMiddleDescriptors.begin()
                    + static_cast<std::ptrdiff_t>(middle.descriptorOffset
                        + kTestSectionDescriptorSize), std::byte{0});
            std::fill(lostMiddleDescriptors.begin()
                    + static_cast<std::ptrdiff_t>(middle.trailerOffset),
                lostMiddleDescriptors.begin()
                    + static_cast<std::ptrdiff_t>(middle.trailerOffset
                        + kTestSectionTrailerSize), std::byte{0});
            std::fill(lostMiddleDescriptors.end()
                    - static_cast<std::ptrdiff_t>(32 + 8),
                lostMiddleDescriptors.end(), std::byte{0});
            const auto middleRecovery = assets::recoverFromSedBytes(
                lostMiddleDescriptors);
            check(middleRecovery.level.things.size() == source.things.size()
                    && middleRecovery.level.h3doNodes.size() == source.h3doNodes.size()
                    && std::find(middleRecovery.missingRequiredSections.begin(),
                        middleRecovery.missingRequiredSections.end(), "sect")
                        != middleRecovery.missingRequiredSections.end(),
                name + ": lost middle descriptors resynchronize later Level sections");

            std::vector<std::byte> withoutSectors{};
            for (std::size_t i = 0; i < sections.size(); ++i)
            {
                if (i == 3) continue; // fixed header, HEAD, RSRC, then SECT
                withoutSectors.insert(withoutSectors.end(), sections[i].begin(), sections[i].end());
            }
            const auto recoveredSections = assets::recoverFromSedBytes(withoutSectors);
            check(recoveredSections.level.kind == kind
                    && recoveredSections.level.sectors.empty()
                    && recoveredSections.level.things.size() == 1
                    && std::find(recoveredSections.missingSections.begin(),
                        recoveredSections.missingSections.end(), "sect")
                        != recoveredSections.missingSections.end(),
                name + ": partial recovery defaults only the missing semantic section");

            // A checksum-valid header that disagrees with the checksum-valid
            // HEAD payload is a semantic contradiction, not recoverable
            // corruption. Recovery must not silently accept it merely because
            // the permissive API is enabled.
            std::vector<std::byte> projectDisagreement = bytes;
            const ProjectType alternate = kind == ProjectType::JKDF2
                ? ProjectType::MOTS : ProjectType::JKDF2;
            writeU32Le(projectDisagreement, 8,
                std::bit_cast<std::uint32_t>(static_cast<std::int32_t>(alternate)));
            writeU32Le(projectDisagreement, 24,
                assets::calculateSedCrc32c(std::span<const std::byte>(projectDisagreement).first(24)));
            const std::size_t headerMirrorOffset = projectDisagreement.size()
                - trailingHeaderMirrorAndFooter;
            constexpr std::size_t mirroredHeaderOffset = 8;
            writeU32Le(projectDisagreement,
                headerMirrorOffset + mirroredHeaderOffset + 8,
                std::bit_cast<std::uint32_t>(static_cast<std::int32_t>(alternate)));
            writeU32Le(projectDisagreement,
                headerMirrorOffset + mirroredHeaderOffset + 24,
                assets::calculateSedCrc32c(std::span<const std::byte>(projectDisagreement)
                    .subspan(headerMirrorOffset + mirroredHeaderOffset, 24)));
            writeU32Le(projectDisagreement, headerMirrorOffset + 36,
                assets::calculateSedCrc32c(std::span<const std::byte>(projectDisagreement)
                    .subspan(headerMirrorOffset + mirroredHeaderOffset,
                        kTestHeaderSize)));
            bool projectMismatchRejected = false;
            try { (void)assets::recoverFromSedBytes(projectDisagreement); }
            catch (const std::exception&) { projectMismatchRejected = true; }
            check(projectMismatchRejected,
                name + ": recovery rejects checksum-valid header/HEAD project contradiction");

            std::vector<std::byte> projectMismatchNoTail = withoutTail;
            writeU32Le(projectMismatchNoTail, 8,
                std::bit_cast<std::uint32_t>(static_cast<std::int32_t>(alternate)));
            writeU32Le(projectMismatchNoTail, 24,
                assets::calculateSedCrc32c(std::span<const std::byte>(projectMismatchNoTail)
                    .first(24)));
            bool projectMismatchNoTailRejected = false;
            try { (void)assets::recoverFromSedBytes(projectMismatchNoTail); }
            catch (const std::exception&) { projectMismatchNoTailRejected = true; }
            check(projectMismatchNoTailRejected,
                name + ": valid primary header contradiction stays hard without tail framing");
        }

        std::ostringstream stream(std::ios::binary);
        assets::saveToSED(source, stream);
        std::istringstream input(stream.str(), std::ios::binary);
        verifyCriticalFields(assets::loadFromSED(input), kind, name + " stream");

        std::vector<std::byte> damaged = bytes;
        const SectionLayout headSection = findSection(bytes, { 'h', 'e', 'a', 'd' });
        check(headSection.parityStreams == 4,
            name + ": required Level sections use four parity streams");
        damageFirstSlots(damaged, headSection, 1);
        const assets::SedFile repairedOne = assets::loadSedFileBytes(damaged);
        verifyCriticalFields(repairedOne.level, kind, name + " one-shard repair");
        check(repairedOne.hadCorruption && repairedOne.repairedChunks == 1,
            name + ": one damaged checked chunk is repaired and reported");

        damaged = bytes;
        damageFirstSlots(damaged, headSection, 4);
        const assets::SedFile repairedTwo = assets::loadSedFileBytes(damaged);
        check(assets::saveToSedBytes(repairedTwo.level) == bytes
                && repairedTwo.repairedChunks == 4,
            name + ": four damaged required-section chunks are repaired transparently");

        damaged = bytes;
        damageFirstSlots(damaged, headSection, 5);
        rejected = false;
        try { (void)assets::loadFromSedBytes(damaged); }
        catch (const std::exception&) { rejected = true; }
        check(rejected, name + ": damage beyond four chunks remains strict");
        const auto partial = assets::recoverFromSedBytes(damaged);
        check(std::find(partial.missingSections.begin(), partial.missingSections.end(), "head")
                != partial.missingSections.end(),
            name + ": recovery reports a semantic unit beyond parity capacity");

        for (const std::array<char, 4> requiredType : {
                 std::array<char, 4>{ 'h', 'e', 'a', 'd' },
                 std::array<char, 4>{ 'r', 's', 'r', 'c' },
                 std::array<char, 4>{ 's', 'e', 'c', 't' },
                 std::array<char, 4>{ 't', 'h', 'n', 'g' },
                 std::array<char, 4>{ 't', 'm', 'p', 'l' },
                 std::array<char, 4>{ 'c', 'o', 'g', 's' },
                 std::array<char, 4>{ 'l', 'i', 't', 'e' },
                 std::array<char, 4>{ 'h', 'n', 'o', 'd' } })
        {
            const SectionLayout requiredSection = findSection(bytes, requiredType);
            std::vector<std::byte> fourDamaged = bytes;
            damageFirstSlots(fourDamaged, requiredSection, 4);
            const assets::SedFile repaired = assets::loadSedFileBytes(fourDamaged);
            check(repaired.level.kind == kind
                    && assets::saveToSedBytes(repaired.level) == bytes
                    && repaired.repairedChunks == 4,
                name + ": four-shard p4 repair succeeds for every required Level section");
            std::vector<std::byte> fiveDamaged = bytes;
            damageFirstSlots(fiveDamaged, requiredSection, 5);
            bool beyondParityRejected = false;
            try { (void)assets::loadFromSedBytes(fiveDamaged); }
            catch (const std::exception&) { beyondParityRejected = true; }
            check(beyondParityRejected,
                name + ": five-shard damage exceeds p4 for every required Level section");
        }

        const auto [primaryDescriptor, mirrorTrailer] =
            firstSectionDescriptorAndMirror(bytes);
        damaged = bytes;
        // storedSize starts at descriptor offset 28 (anchor/type/version/
        // flags/rawSize precede it); the mirror descriptor repairs this copy.
        damaged[primaryDescriptor + 28] ^= std::byte{ 0x40 };
        const assets::SedFile repairedDescriptor = assets::loadSedFileBytes(damaged);
        check(assets::saveToSedBytes(repairedDescriptor.level) == bytes
                && repairedDescriptor.hadCorruption
                && std::find(repairedDescriptor.repairedSections.begin(),
                    repairedDescriptor.repairedSections.end(), "head")
                    != repairedDescriptor.repairedSections.end(),
            name + ": primary section descriptor damage is repaired from its mirror");

        damaged = bytes;
        damaged[mirrorTrailer + 12 + 28] ^= std::byte{ 0x20 };
        const assets::SedRecoveryLoadResult repairedMirror =
            assets::recoverFromSedBytes(damaged);
        check(assets::saveToSedBytes(repairedMirror.level) == bytes
                && repairedMirror.hadCorruption
                && !repairedMirror.partialRecoveryFromCorruption
                && repairedMirror.missingSections.empty(),
            name + ": recovery reports mirror-only descriptor damage while retaining the primary");

        damaged = bytes;
        damaged[primaryDescriptor + 28] ^= std::byte{ 0x40 };
        damaged[mirrorTrailer + 12 + 28] ^= std::byte{ 0x20 };
        rejected = false;
        try { (void)assets::loadFromSedBytes(damaged); }
        catch (const std::exception&) { rejected = true; }
        check(rejected, name + ": both descriptor copies damaged are strict-fail");
        const auto descriptorPartial = assets::recoverFromSedBytes(damaged);
        check(std::find(descriptorPartial.missingSections.begin(),
                    descriptorPartial.missingSections.end(), "head")
                != descriptorPartial.missingSections.end()
                && descriptorPartial.level.materials.size() == source.materials.size(),
            name + ": recovery resynchronizes after an unrecoverable descriptor");

        // A descriptor with a valid CRC but an unsupported V1 schema is not
        // an erasure. Both strict and permissive loaders must fail rather
        // than defaulting the semantic section as if it were damaged.
        std::vector<std::byte> schemaInvalid = bytes;
        writeU32Le(schemaInvalid, primaryDescriptor + 12, 2u); // unsupported descriptor version
        writeU32Le(schemaInvalid, primaryDescriptor + kTestSectionDescriptorSize - 4,
            assets::calculateSedCrc32c(std::span<const std::byte>(schemaInvalid)
                .subspan(primaryDescriptor, kTestSectionDescriptorSize - 4)));
        writeU32Le(schemaInvalid, mirrorTrailer + 12 + 12, 2u);
        writeU32Le(schemaInvalid, mirrorTrailer + 12 + kTestSectionDescriptorSize - 4,
            assets::calculateSedCrc32c(std::span<const std::byte>(schemaInvalid)
                .subspan(mirrorTrailer + 12, kTestSectionDescriptorSize - 4)));
        writeU32Le(schemaInvalid, mirrorTrailer + kTestSectionTrailerSize - 4,
            assets::calculateSedCrc32c(std::span<const std::byte>(schemaInvalid)
                .subspan(mirrorTrailer, kTestSectionTrailerSize - 4)));
        bool schemaRejected = false;
        try { (void)assets::recoverFromSedBytes(schemaInvalid); }
        catch (const std::exception&) { schemaRejected = true; }
        check(schemaRejected,
            name + ": checksum-valid unsupported descriptor schema remains a hard error");

        std::vector<std::byte> typeInvalid = bytes;
        typeInvalid[primaryDescriptor + 8] = std::byte{'H'};
        typeInvalid[mirrorTrailer + 12 + 8] = std::byte{'H'};
        writeU32Le(typeInvalid, primaryDescriptor + kTestSectionDescriptorSize - 4,
            assets::calculateSedCrc32c(std::span<const std::byte>(typeInvalid)
                .subspan(primaryDescriptor, kTestSectionDescriptorSize - 4)));
        writeU32Le(typeInvalid, mirrorTrailer + 12 + kTestSectionDescriptorSize - 4,
            assets::calculateSedCrc32c(std::span<const std::byte>(typeInvalid)
                .subspan(mirrorTrailer + 12, kTestSectionDescriptorSize - 4)));
        writeU32Le(typeInvalid, mirrorTrailer + kTestSectionTrailerSize - 4,
            assets::calculateSedCrc32c(std::span<const std::byte>(typeInvalid)
                .subspan(mirrorTrailer, kTestSectionTrailerSize - 4)));
        bool typeRejected = false;
        try { (void)assets::recoverFromSedBytes(typeInvalid); }
        catch (const std::exception&) { typeRejected = true; }
        check(typeRejected,
            name + ": checksum-valid uppercase descriptor type remains a hard error");

        damaged = bytes;
        damaged.resize(damaged.size() - 7);
        rejected = false;
        try { (void)assets::loadFromSedBytes(damaged); }
        catch (const std::exception&) { rejected = true; }
        check(rejected, name + ": truncation is rejected");
    }

    const auto verifyAllRequiredErasures = [](Level source,
        const std::uint32_t expectedDataStreams, const std::string& label)
    {
        std::vector<std::byte> bytes = assets::saveToSedBytes(source);
        const SectionLayout head = findSection(bytes, { 'h', 'e', 'a', 'd' });
        check(head.dataStreams == expectedDataStreams && head.parityStreams == 4
                && head.stripeCount == 1,
            label + ": fixture has the requested one-stripe k+4 layout");
        const std::size_t streamCount = head.dataStreams + head.parityStreams;
        std::size_t combinations{};
        std::size_t failures{};
        std::vector<std::size_t> selected{};
        for (std::size_t erasures = 1; erasures <= 4; ++erasures)
        {
            selected.resize(erasures);
            const auto visit = [&](const auto& self, const std::size_t depth,
                const std::size_t next) -> void
            {
                if (depth == selected.size())
                {
                    damageSlots(bytes, head, selected, 0, true);
                    try
                    {
                        const Level loaded = assets::loadFromSedBytes(bytes);
                        if (loaded.kind != source.kind
                            || loaded.lvisstring != source.lvisstring)
                        {
                            ++failures;
                        }
                    }
                    catch (const std::exception&)
                    {
                        ++failures;
                    }
                    damageSlots(bytes, head, selected, 0, true);
                    ++combinations;
                    return;
                }
                const std::size_t remaining = selected.size() - depth;
                for (std::size_t stream = next;
                    stream + remaining <= streamCount; ++stream)
                {
                    selected[depth] = stream;
                    self(self, depth + 1, stream + 1);
                }
            };
            visit(visit, 0, 0);
        }
        check(failures == 0,
            label + ": every data/parity erasure combination through p=4 repairs");
        if (expectedDataStreams == 16)
        {
            check(combinations == 6195,
                label + ": exhaustive C(20,1..4) MDS coverage executed");
        }
    };

    verifyAllRequiredErasures(richLevel(ProjectType::JKDF2), 1,
        "SED MDS k=1");
    Level mediumMds = richLevel(ProjectType::MOTS);
    mediumMds.lvisstring = deterministicNoise(18'000);
    verifyAllRequiredErasures(std::move(mediumMds), 5,
        "SED MDS adaptive k=5");
    Level fullMds = richLevel(ProjectType::IJIM);
    fullMds.lvisstring = deterministicNoise(63'000);
    verifyAllRequiredErasures(std::move(fullMds), 16,
        "SED MDS full k=16");

    {
        const Level source = richLevel(ProjectType::JKDF2);
        const std::vector<std::byte> nestedBytes = assets::saveToSedBytes(
            source, {}, assets::SedSerializationProfile::UnprotectedNested);
        verifyCriticalFields(assets::loadNestedSedLevelBytes(nestedBytes),
            ProjectType::JKDF2, "SED explicit unprotected nested profile");
        bool topLevelRejected = false;
        try { (void)assets::loadFromSedBytes(nestedBytes); }
        catch (const std::exception&) { topLevelRejected = true; }
        check(topLevelRejected,
            "SED protection policy: top-level loader rejects unprotected nested bytes");
        const auto validatedNestedSections = assets::splitSedFileSections(
            nestedBytes, assets::SedSerializationProfile::UnprotectedNested);
        const auto generatedNestedSections = assets::splitGeneratedSedFileSections(
            nestedBytes, assets::SedSerializationProfile::UnprotectedNested);
        check(validatedNestedSections.size() == 9,
            "SED explicit nested profile: validated semantic splitting remains available");
        check(generatedNestedSections == validatedNestedSections,
            "SED explicit nested profile: writer-trusted splitting is byte-identical to strict splitting");

        const std::vector<std::byte> historyNestedBytes =
            assets::saveToSedBytes(
                source, {}, assets::SedSerializationProfile::HistoryNested);
        verifyCriticalFields(
            assets::loadHistoryNestedSedLevelBytes(historyNestedBytes),
            ProjectType::JKDF2,
            "SED explicit history-nested checksum profile");
        const SectionLayout historyHead = findSection(
            historyNestedBytes, { 'h', 'e', 'a', 'd' });
        const auto historySpan = std::span<const std::byte>(historyNestedBytes);
        const bool historyChecksumLayout =
            readU32Le(historySpan, historyHead.descriptorOffset + 16) == 4u
            && readU32Le(historySpan, historyHead.descriptorOffset + 64)
                == assets::calculateSedCrc32c(historySpan.subspan(
                    historyHead.payloadOffset,
                    static_cast<std::size_t>(historyHead.storedSize)))
            && std::all_of(
                historySpan.begin() + static_cast<std::ptrdiff_t>(
                    historyHead.descriptorOffset + 68),
                historySpan.begin() + static_cast<std::ptrdiff_t>(
                    historyHead.descriptorOffset + 96),
                [](const std::byte value) { return value == std::byte{0}; });
        bool genericNestedRejectedHistory = false;
        bool historyRejectedGenericNested = false;
        bool topLevelRejectedHistory = false;
        try { (void)assets::loadNestedSedLevelBytes(historyNestedBytes); }
        catch (const std::exception&) { genericNestedRejectedHistory = true; }
        try { (void)assets::loadHistoryNestedSedLevelBytes(nestedBytes); }
        catch (const std::exception&) { historyRejectedGenericNested = true; }
        try { (void)assets::loadFromSedBytes(historyNestedBytes); }
        catch (const std::exception&) { topLevelRejectedHistory = true; }
        check(historyChecksumLayout
                && genericNestedRejectedHistory
                && historyRejectedGenericNested
                && topLevelRejectedHistory,
            "SED history-nested profile is canonical and cannot cross generic/top-level trust boundaries");

        std::vector<std::byte> nonCanonicalHistoryDigest = historyNestedBytes;
        nonCanonicalHistoryDigest[historyHead.descriptorOffset + 68]
            = std::byte{ 1 };
        writeU32Le(nonCanonicalHistoryDigest,
            historyHead.descriptorOffset + kTestSectionDescriptorSize - 4,
            assets::calculateSedCrc32c(std::span<const std::byte>(
                nonCanonicalHistoryDigest).subspan(
                    historyHead.descriptorOffset,
                    kTestSectionDescriptorSize - 4)));
        std::copy_n(
            nonCanonicalHistoryDigest.begin()
                + static_cast<std::ptrdiff_t>(historyHead.descriptorOffset),
            kTestSectionDescriptorSize,
            nonCanonicalHistoryDigest.begin()
                + static_cast<std::ptrdiff_t>(historyHead.trailerOffset + 12));
        writeU32Le(nonCanonicalHistoryDigest,
            historyHead.trailerOffset + kTestSectionTrailerSize - 4,
            assets::calculateSedCrc32c(std::span<const std::byte>(
                nonCanonicalHistoryDigest).subspan(
                    historyHead.trailerOffset,
                    kTestSectionTrailerSize - 4)));
        bool nonCanonicalHistoryDigestRejected = false;
        try
        {
            (void)assets::loadHistoryNestedSedLevelBytes(
                nonCanonicalHistoryDigest);
        }
        catch (const std::exception&)
        {
            nonCanonicalHistoryDigestRejected = true;
        }
        check(nonCanonicalHistoryDigestRejected,
            "SED history-nested profile rejects nonzero reserved checksum bytes with valid descriptor copies");

        const assets::SedOptionalSection forbiddenHistoryOptional{
            { 't', 'e', 's', 't' }, { std::byte{ 1 } } };
        bool historyOptionalRejected = false;
        try
        {
            (void)assets::saveToSedBytes(source,
                std::span<const assets::SedOptionalSection>(
                    &forbiddenHistoryOptional, 1),
                assets::SedSerializationProfile::HistoryNested);
        }
        catch (const std::exception&) { historyOptionalRejected = true; }
        check(historyOptionalRejected,
            "SED history-nested profile rejects optional sections before framing");

        // Adjacent HIST states reuse only core-generated, offset-independent
        // HistoryNested frames. Every incremental result must remain the
        // exact canonical byte stream produced by the complete writer.
        for (const ProjectType project : {
                 ProjectType::JKDF2, ProjectType::MOTS, ProjectType::IJIM })
        {
            Level base = richLevel(project);
            if (project == ProjectType::MOTS)
            {
                TplValue flags{};
                flags.name = "thingflags";
                flags.type = ValueType::integer;
                flags.intValue = 0x8;
                base.things.front().values.push_back(flags);
                TplValue model{};
                model.name = "model3d";
                model.type = ValueType::str;
                model.adjType = AdjType::model3do;
                model.stringValue = "arch_actor.3do";
                base.things.front().values.push_back(model);
            }
            const assets::SedGeneratedUnprotectedDocument generatedBase =
                assets::generateUnprotectedSedDocument(base);
            check(generatedBase.bytes == assets::saveToSedBytes(
                    base, {}, assets::SedSerializationProfile::HistoryNested)
                    && generatedBase.byteSize == generatedBase.bytes.size()
                    && generatedBase.regeneratedSections
                        == assets::SedRequiredSection::All,
                "SED incremental nested writer: initial generation is canonical for every project");

            enum class SnapshotTransition
            {
                full,
                things,
                cogs,
                lights,
                sectorSubset,
                surfaceSubset,
            };
            const auto generateTransition = [&base, &generatedBase](
                    const Level& historical,
                    const SnapshotTransition kind,
                    const bool materializeBytes,
                    bool* const mismatchedLevelRejected = nullptr)
            {
                editor::Document walk(editor::DocumentState{ historical, {} });
                switch (kind)
                {
                case SnapshotTransition::full:
                    walk.pushUndo("full transition",
                        editor::DocumentDirty::Structure);
                    break;
                case SnapshotTransition::things:
                    walk.pushUndo("Thing transition",
                        editor::DocumentDirty::Things);
                    break;
                case SnapshotTransition::cogs:
                    walk.pushUndo("COG transition",
                        editor::DocumentDirty::Cogs);
                    break;
                case SnapshotTransition::lights:
                    walk.pushUndo("Light transition",
                        editor::DocumentDirty::Lights);
                    break;
                case SnapshotTransition::sectorSubset:
                    walk.pushSectorSubsetUndo("Sector transition",
                        editor::DocumentDirty::LevelGeometry, { 0 });
                    break;
                case SnapshotTransition::surfaceSubset:
                    walk.pushSurfaceSubsetUndo("Surface transition",
                        editor::DocumentDirty::LevelGeometry, { 0 });
                    break;
                }
                // The private Snapshot contains `historical`; undoing from
                // the adjacent base reconstructs it and mints a token bound
                // to that exact temporary Level object.
                walk.level() = base;
                std::optional<assets::SedGeneratedUnprotectedDocument> result{};
                const bool visited = walk.visitRecoveryHistoryTransitions(
                    [&](const editor::DocumentRecoveryHistoryStack,
                        const std::string_view,
                        const std::uint32_t,
                        const editor::DocumentHistoryTransitionDomain,
                        const assets::SedHistoryTransitionToken& transition,
                        editor::DocumentState& state)
                    {
                        if (mismatchedLevelRejected)
                        {
                            Level unrelated = state.level;
                            try
                            {
                                (void)assets::detail::
                                    generateHistoryNestedTransitionDocument(
                                        unrelated, transition,
                                        &generatedBase.requiredSections,
                                        false);
                            }
                            catch (const std::invalid_argument&)
                            {
                                *mismatchedLevelRejected = true;
                            }
                        }
                        result.emplace(assets::detail::
                            generateHistoryNestedTransitionDocument(
                                state.level, transition,
                                &generatedBase.requiredSections,
                                materializeBytes));
                        return true;
                    });
                if (!visited || !result)
                {
                    throw std::logic_error(
                        "SED test could not visit its history transition.");
                }
                return std::move(*result);
            };

            bool mismatchedLevelRejected = false;
            const assets::SedGeneratedUnprotectedDocument framesOnly =
                generateTransition(base, SnapshotTransition::full, false,
                    &mismatchedLevelRejected);
            bool exactOwnersReused = framesOnly.bytes.empty()
                && framesOnly.byteSize == generatedBase.bytes.size();
            for (std::size_t section = 0;
                 section < generatedBase.requiredSections.exactFrames().size();
                 ++section)
            {
                exactOwnersReused = exactOwnersReused
                    && framesOnly.requiredSections.exactFrames()[section]
                        == generatedBase.requiredSections.exactFrames()[section];
            }
            check(exactOwnersReused && mismatchedLevelRejected,
                "SED incremental nested writer: frames-only mode avoids materializing, preserves exact unchanged owners, and rejects a token used with another Level object");

            const assets::SedGeneratedUnprotectedDocument semanticNoOp =
                generateTransition(
                    base, SnapshotTransition::things, false);
            check(semanticNoOp.requiredSections.exactFrames()[3]
                    == generatedBase.requiredSections.exactFrames()[3]
                    && assets::hasAny(semanticNoOp.regeneratedSections,
                        assets::SedRequiredSection::Things),
                "SED incremental nested writer: exact regenerated no-op retains its proven shared frame owner");

            const auto verifyTransition = [&](Level changed,
                    const SnapshotTransition kind,
                    const assets::SedRequiredSection expectedRegenerated,
                    const bool expectedResourceChange,
                    const std::string& label)
            {
                const assets::SedGeneratedUnprotectedDocument incremental =
                    generateTransition(changed, kind, true);
                const std::vector<std::byte> complete = assets::saveToSedBytes(
                    changed, {},
                    assets::SedSerializationProfile::HistoryNested);
                check(incremental.bytes == complete
                        && incremental.regeneratedSections == expectedRegenerated
                        && incremental.resourceCatalogChanged
                            == expectedResourceChange,
                    "SED incremental nested writer: " + label);
            };

            verifyTransition(base, SnapshotTransition::full,
                assets::SedRequiredSection::All, false,
                "full no-op transition retains canonical bytes");

            Level head = base;
            ++head.curId;
            verifyTransition(std::move(head), SnapshotTransition::things,
                assets::SedRequiredSection::Head
                    | assets::SedRequiredSection::Things
                    | assets::SedRequiredSection::Resources,
                false, "head transition is byte-identical");

            Level sectors = base;
            sectors.sectors.front().ambient.r += 0.03125f;
            verifyTransition(std::move(sectors),
                SnapshotTransition::sectorSubset,
                assets::SedRequiredSection::Head
                    | assets::SedRequiredSection::Sectors
                    | assets::SedRequiredSection::Resources,
                false, "sector transition is byte-identical");
            Level trustedSector = base;
            trustedSector.sectors.front().ambient.r += 0.03125f;
            const std::vector<std::byte> trustedSectorComplete =
                assets::saveToSedBytes(trustedSector, {},
                    assets::SedSerializationProfile::HistoryNested);
            const assets::SedGeneratedUnprotectedDocument trustedSectorNested =
                generateTransition(trustedSector,
                    SnapshotTransition::sectorSubset, true);
            check(trustedSectorNested.bytes == trustedSectorComplete,
                "SED trusted nested writer: SectorSubset payload patch is canonical");

            Level trustedSurface = base;
            trustedSurface.sectors.front().surfaces.front().extraLight.g
                += 0.03125f;
            const std::vector<std::byte> trustedSurfaceComplete =
                assets::saveToSedBytes(trustedSurface, {},
                    assets::SedSerializationProfile::HistoryNested);
            const assets::SedGeneratedUnprotectedDocument trustedSurfaceNested =
                generateTransition(trustedSurface,
                    SnapshotTransition::surfaceSubset, true);
            check(trustedSurfaceNested.bytes == trustedSurfaceComplete,
                "SED trusted nested writer: SurfaceSubset payload patch is canonical");

            Level things = base;
            things.things.front().x += 1.25;
            verifyTransition(std::move(things),
                SnapshotTransition::things,
                assets::SedRequiredSection::Head
                    | assets::SedRequiredSection::Things
                    | assets::SedRequiredSection::Resources,
                false, "Thing transition is byte-identical");

            Level templates = base;
            templates.templates.front().bbox.x1 -= 0.5;
            verifyTransition(std::move(templates),
                SnapshotTransition::full,
                assets::SedRequiredSection::All,
                false, "template transition is byte-identical");

            Level cogs = base;
            cogs.cogs.front().values.front().intValue += 1;
            verifyTransition(std::move(cogs),
                SnapshotTransition::cogs,
                assets::SedRequiredSection::Head
                    | assets::SedRequiredSection::Cogs
                    | assets::SedRequiredSection::Resources,
                false, "COG transition is byte-identical");

            Level lights = base;
            lights.lights.front().position.x += 0.75;
            auto lightTransitionSections = assets::SedRequiredSection::Head
                | assets::SedRequiredSection::Sectors
                | assets::SedRequiredSection::Lights
                | assets::SedRequiredSection::Resources;
            if (project == ProjectType::MOTS)
            {
                // An automatic 0x20 bake changes ARCH refs, generated values,
                // and baseline provenance as part of a Lights-only undo item.
                lights.lights.front().flags |= LF_AffectArchThings;
                MotsArchLightObject generatedArch{};
                generatedArch.wireIndex = 12;
                generatedArch.meshes.push_back(MotsArchLightMesh{
                    0, { MotsArchLightVertex{ 0.0f, 0.25f, 0.5f, 0.75f } } });
                lights.motsArchLighting.push_back(generatedArch);
                lights.things.front().archLightingIndex = 0;
                lights.motsArchLightingBakeBaselines.push_back(
                    MotsArchLightingBakeBaseline{
                        lights.things.front().id, "arch_actor.3do", -1,
                        std::nullopt });
                // The base fixture has no serialized ARCH section, so this
                // transition upgrades the normalized MotS binary version and
                // correctly regenerates every required section once.
                lightTransitionSections = assets::SedRequiredSection::All;
                const Level forgedThing = [&lights]
                {
                    Level value = lights;
                    value.things.front().x += 0.25;
                    return value;
                }();
                Level guardedBase = base;
                Thing unrelated{};
                unrelated.id = guardedBase.things.front().id + 90'000;
                unrelated.num = 1;
                unrelated.name = "unrelated_arch_observer";
                guardedBase.things.push_back(unrelated);
                Level guardedLights = lights;
                guardedLights.things.push_back(unrelated);
                const Level forgedReference = [&guardedLights]
                {
                    Level value = guardedLights;
                    value.things.back().archLightingIndex = 0;
                    return value;
                }();
                const Level forgedUnreferencedRecord = [&guardedLights]
                {
                    Level value = guardedLights;
                    MotsArchLightObject extra{};
                    extra.wireIndex = 13;
                    extra.meshes.push_back(MotsArchLightMesh{
                        0, { MotsArchLightVertex{ 0.0f, 0.0f, 0.0f, 0.0f } } });
                    value.motsArchLighting.push_back(std::move(extra));
                    return value;
                }();
                const Level forgedProvenance = [&guardedLights]
                {
                    Level value = guardedLights;
                    value.motsArchLightingBakeBaselines.front().modelName =
                        "different_model.3do";
                    return value;
                }();
                const Level duplicateProvenance = [&guardedLights]
                {
                    Level value = guardedLights;
                    value.motsArchLightingBakeBaselines.push_back(
                        value.motsArchLightingBakeBaselines.front());
                    return value;
                }();
                check(assets::sedLevelsEqualIgnoringHistoryDomains(
                            base, lights, false, true, false)
                        && !assets::sedLevelsEqualIgnoringHistoryDomains(
                            base, forgedThing, false, true, false)
                        && assets::sedLevelsEqualIgnoringHistoryDomains(
                            guardedBase, guardedLights, false, true, false)
                        && !assets::sedLevelsEqualIgnoringHistoryDomains(
                            guardedBase, forgedReference, false, true, false)
                        && !assets::sedLevelsEqualIgnoringHistoryDomains(
                            guardedBase, forgedUnreferencedRecord, false, true, false)
                        && !assets::sedLevelsEqualIgnoringHistoryDomains(
                            guardedBase, forgedProvenance, false, true, false)
                        && !assets::sedLevelsEqualIgnoringHistoryDomains(
                            guardedBase, duplicateProvenance, false, true, false),
                    "SED MotS Lights history admits provenance-owned ARCH bake output but rejects unrelated refs, table edits, and forged or duplicate provenance");
            }
            verifyTransition(std::move(lights),
                SnapshotTransition::lights,
                lightTransitionSections,
                false, "Light transition is byte-identical");

            Level hierarchy = base;
            hierarchy.h3doNodes.front().position.y -= 0.25;
            verifyTransition(std::move(hierarchy),
                SnapshotTransition::full,
                assets::SedRequiredSection::All,
                false, "hierarchy transition is byte-identical");

            Level resourceChange = base;
            resourceChange.things.front().name += "_new_catalog_entry";
            constexpr assets::SedRequiredSection indexedRegeneration =
                assets::SedRequiredSection::Head
                | assets::SedRequiredSection::Resources
                | assets::SedRequiredSection::Sectors
                | assets::SedRequiredSection::Things
                | assets::SedRequiredSection::Templates
                | assets::SedRequiredSection::Cogs
                | assets::SedRequiredSection::Hierarchy;
            verifyTransition(std::move(resourceChange),
                SnapshotTransition::things,
                indexedRegeneration, true,
                "resource-catalog change regenerates every indexed section");

            // The matching load-side path validates every checked frame but
            // only rematerializes semantic domains whose exact frame changed.
            assets::SedNestedHistoryDecoder decoder(base);
            decoder.advance(generatedBase.bytes);
            check(assets::sedLevelsEqual(decoder.currentLevel(), base)
                    && decoder.lastStats().decodedRequiredSections == 1
                    && decoder.lastStats().reusedRequiredSections == 7
                    && decoder.lastStats().transactionallyReplacedRequiredSections == 0
                    && decoder.lastStats().usedTransactionalDomainReuse
                    && !decoder.lastStats().resourceSectionChanged,
                "SED incremental nested reader: unchanged state decodes only rsrc");

            Level movedThing = base;
            movedThing.things.front().x += 1.25;
            const std::vector<std::byte> movedThingBytes = assets::saveToSedBytes(
                movedThing, {},
                assets::SedSerializationProfile::HistoryNested);
            decoder.advance(movedThingBytes);
            const auto& movedThingIndices = decoder.lastChangedThingIndices();
            check(assets::sedLevelsEqual(decoder.currentLevel(), movedThing)
                    && assets::sedLevelsEqual(decoder.previousLevel(), base)
                    && decoder.lastStats().decodedRequiredSections == 2
                    && decoder.lastStats().reusedRequiredSections == 6
                    && decoder.lastStats().transactionallyReplacedRequiredSections == 1
                    && decoder.lastStats().usedTransactionalDomainReuse
                    && !decoder.lastStats().resourceSectionChanged,
                "SED incremental nested reader: one changed Thing frame is decoded exactly");
            check(decoder.lastTransitionEqualsIgnoringHistoryDomains(
                        true, false, false)
                    && !decoder.lastTransitionEqualsIgnoringHistoryDomains(
                        false, false, false)
                    && movedThingIndices
                    && *movedThingIndices == std::vector<std::int32_t>{ 0 }
                    && !decoder.lastChangedSectorIndices(),
                "SED incremental nested reader: compact transition facts are exact across a changed Thing domain");

            Level indexedShift = movedThing;
            indexedShift.things.front().name += "_new_catalog_entry";
            const std::vector<std::byte> indexedShiftBytes = assets::saveToSedBytes(
                indexedShift, {},
                assets::SedSerializationProfile::HistoryNested);
            decoder.advance(indexedShiftBytes);
            const bool indexedShiftEqual = assets::sedLevelsEqual(
                decoder.currentLevel(), indexedShift);
            check(indexedShiftEqual
                    && decoder.lastStats().decodedRequiredSections == 7
                    && decoder.lastStats().reusedRequiredSections == 1
                    && decoder.lastStats().transactionallyReplacedRequiredSections == 7
                    && decoder.lastStats().usedTransactionalDomainReuse
                    && decoder.lastStats().resourceSectionChanged,
                "SED incremental nested reader: rsrc change decodes every indexed domain"
                    " (equal=" + std::to_string(indexedShiftEqual)
                    + ", decoded=" + std::to_string(
                        decoder.lastStats().decodedRequiredSections)
                    + ", reused=" + std::to_string(
                        decoder.lastStats().reusedRequiredSections)
                    + ", rsrc=" + std::to_string(
                        decoder.lastStats().resourceSectionChanged) + ")");

            // Exercise rollback after an earlier changed semantic domain was
            // already installed. Corrupting HNOD is deliberately later than
            // THNG in canonical order; failure must retain both accepted
            // Levels, bytes, stats, and transition caches exactly.
            Level lateFailureCandidate = indexedShift;
            lateFailureCandidate.things.front().x += 0.5;
            std::vector<std::byte> lateFailureBytes = assets::saveToSedBytes(
                lateFailureCandidate, {},
                assets::SedSerializationProfile::HistoryNested);
            const SectionLayout lateHierarchy = findSection(
                lateFailureBytes, { 'h', 'n', 'o', 'd' });
            lateFailureBytes[lateHierarchy.payloadOffset] ^= std::byte{ 0x40 };
            const std::vector<std::byte> acceptedBytes(
                decoder.currentBytes().begin(), decoder.currentBytes().end());
            bool lateFailureRejected = false;
            try { decoder.advance(std::move(lateFailureBytes)); }
            catch (const std::exception&) { lateFailureRejected = true; }
            check(lateFailureRejected
                    && assets::sedLevelsEqual(decoder.currentLevel(), indexedShift)
                    && assets::sedLevelsEqual(decoder.previousLevel(), movedThing)
                    && std::equal(acceptedBytes.begin(), acceptedBytes.end(),
                        decoder.currentBytes().begin(), decoder.currentBytes().end())
                    && decoder.lastStats().decodedRequiredSections == 7
                    && decoder.lastStats().reusedRequiredSections == 1
                    && decoder.lastStats().transactionallyReplacedRequiredSections == 7
                    && decoder.lastStats().resourceSectionChanged,
                "SED incremental nested reader: late strict failure rolls back every installed domain and leaves trust state unchanged");

            std::vector<std::byte> tampered = movedThingBytes;
            const SectionLayout nestedThings = findSection(
                tampered, { 't', 'h', 'n', 'g' });
            tampered[nestedThings.payloadOffset] ^= std::byte{ 0x01 };
            bool tamperRejected = false;
            try { decoder.advance(tampered); }
            catch (const std::exception&) { tamperRejected = true; }
            check(tamperRejected
                    && assets::sedLevelsEqual(decoder.currentLevel(), indexedShift)
                    && decoder.lastStats().decodedRequiredSections == 7
                    && decoder.lastStats().reusedRequiredSections == 1
                    && decoder.lastStats().resourceSectionChanged,
                "SED incremental nested reader: changed payload cannot bypass history checksum validation or mutate trust state");

            auto reorderedFrames = assets::splitGeneratedSedFileSections(
                movedThingBytes,
                assets::SedSerializationProfile::HistoryNested);
            std::swap(reorderedFrames[1], reorderedFrames[2]);
            std::vector<std::byte> reordered{};
            for (const auto& frame : reorderedFrames)
            {
                reordered.insert(reordered.end(), frame.begin(), frame.end());
            }
            bool orderRejected = false;
            try { (void)assets::loadHistoryNestedSedLevelBytes(reordered); }
            catch (const std::exception&) { orderRejected = true; }
            check(orderRejected,
                "SED nested reader: required-section reordering is noncanonical");

            const assets::SedOptionalSection unexpectedNestedSection{
                { 't', 'e', 's', 't' },
                { std::byte{ 0x01 }, std::byte{ 0x02 } } };
            const std::vector<std::byte> nestedWithOptional = assets::saveToSedBytes(
                movedThing,
                std::span<const assets::SedOptionalSection>(
                    &unexpectedNestedSection, 1),
                assets::SedSerializationProfile::UnprotectedNested);
            bool optionalRejected = false;
            try { (void)assets::loadNestedSedLevelBytes(nestedWithOptional); }
            catch (const std::exception&) { optionalRejected = true; }
            check(optionalRejected,
                "SED nested reader: optional HIST sections are noncanonical");

            assets::SedNestedHistoryDecoder compactOnlyDecoder(base);
            Level compactOnlyState = base;
            bool compactFactsExact = true;
            constexpr std::size_t compactOnlyEntries = 32;
            for (std::size_t index = 0; index < compactOnlyEntries; ++index)
            {
                compactOnlyState.things.front().x += 0.125;
                compactOnlyDecoder.advance(assets::saveToSedBytes(
                    compactOnlyState, {},
                    assets::SedSerializationProfile::HistoryNested));
                const auto& indices =
                    compactOnlyDecoder.lastChangedThingIndices();
                compactFactsExact = compactFactsExact
                    && compactOnlyDecoder.
                        lastTransitionEqualsIgnoringHistoryDomains(
                            true, false, false)
                    && indices
                    && *indices == std::vector<std::int32_t>{ 0 };
            }
            check(compactFactsExact
                    && compactOnlyDecoder.previousLevelMaterializationCount() == 0,
                "SED incremental nested reader: long compact Thing timeline performs zero full previous-Level compatibility materializations");
        }

        const auto invalidProfile =
            static_cast<assets::SedSerializationProfile>(99);
        const auto rejectsInvalidProfile = [](const auto& operation)
        {
            try
            {
                operation();
            }
            catch (const std::invalid_argument&)
            {
                return true;
            }
            catch (const std::exception&)
            {
            }
            return false;
        };
        check(rejectsInvalidProfile([&]
                {
                    (void)assets::saveToSedBytes(source, {}, invalidProfile);
                })
                && rejectsInvalidProfile([&]
                {
                    (void)assets::splitGeneratedSedFileSections(
                        nestedBytes, invalidProfile);
                })
                && rejectsInvalidProfile([&]
                {
                    (void)assets::findEmbeddedSedDocument(
                        nestedBytes, 0, invalidProfile);
                }),
            "SED profile policy: invalid enum values cannot bypass top-level or nested protection");

        std::vector<std::byte> damagedGeneratedHeader = nestedBytes;
        damagedGeneratedHeader.front() ^= std::byte{ 0x01 };
        const auto repairedGeneratedSections =
            assets::splitGeneratedSedFileSections(
                damagedGeneratedHeader,
                assets::SedSerializationProfile::UnprotectedNested);
        check(repairedGeneratedSections == validatedNestedSections,
            "SED generated splitter distinguishes a damaged present header from a genuinely headerless stream");

        // The generated splitter deliberately skips a second semantic decode,
        // but it is still a public entry point.  Keep its fixed-framing and
        // section-count admission checks active so short/hostile spans cannot
        // underflow the footer calculation or reserve an attacker-sized
        // vector before the first descriptor is examined.
        bool shortSplitRejected = false;
        try
        {
            const std::vector<std::byte> shortSed{
                std::byte{'s'}, std::byte{'e'}, std::byte{'d'}, std::byte{0} };
            (void)assets::splitGeneratedSedFileSections(shortSed,
                assets::SedSerializationProfile::UnprotectedNested);
        }
        catch (const std::exception&)
        {
            shortSplitRejected = true;
        }
        check(shortSplitRejected,
            "SED splitter safety: short primary signature is rejected before footer arithmetic");

        bool forgedCountRejected = false;
        try
        {
            std::vector<std::byte> forgedCount = nestedBytes;
            writeU32Le(forgedCount, 12, std::numeric_limits<std::uint32_t>::max());
            (void)assets::splitGeneratedSedFileSections(forgedCount,
                assets::SedSerializationProfile::UnprotectedNested);
        }
        catch (const std::exception&)
        {
            forgedCountRejected = true;
        }
        check(forgedCountRejected,
            "SED splitter safety: forged section count is rejected before allocation");

        std::vector<std::byte> embedded(13, std::byte{0xa5});
        embedded.insert(embedded.end(), nestedBytes.begin(), nestedBytes.end());
        const auto locatedNested = assets::findEmbeddedSedDocument(embedded, 0,
            assets::SedSerializationProfile::UnprotectedNested);
        check(locatedNested && locatedNested->offset == 13
                && locatedNested->size == nestedBytes.size(),
            "SED explicit nested profile: embedded-document scanner returns exact framing");

        std::vector<std::byte> plausibleHeaderPrefix{};
        for (std::size_t candidate = 0; candidate < 32; ++candidate)
        {
            std::vector<std::byte> header(kTestHeaderSize, std::byte{0});
            header[0] = std::byte{'s'};
            header[1] = std::byte{'e'};
            header[2] = std::byte{'d'};
            writeU32Le(header, 4, 1);
            writeU32Le(header, 8, 0);
            writeU32Le(header, 12, 1);
            writeU32Le(header, 24, assets::calculateSedCrc32c(
                std::span<const std::byte>(header).first(24)));
            plausibleHeaderPrefix.insert(plausibleHeaderPrefix.end(),
                header.begin(), header.end());
        }
        const std::size_t protectedOffset = plausibleHeaderPrefix.size();
        plausibleHeaderPrefix.insert(plausibleHeaderPrefix.end(),
            nestedBytes.begin(), nestedBytes.end());
        const auto afterPlausibleHeaders = assets::findEmbeddedSedDocument(
            plausibleHeaderPrefix, 0,
            assets::SedSerializationProfile::UnprotectedNested);
        check(afterPlausibleHeaders
                && afterPlausibleHeaders->offset == protectedOffset
                && afterPlausibleHeaders->size == nestedBytes.size(),
            "SED embedded scanner: bounded candidate search retains a later strict document");

        const std::vector<std::byte> protectedBytes = assets::saveToSedBytes(source);
        std::vector<std::byte> fakeHeaderlessPrefix{
            std::byte{'s'}, std::byte{'e'}, std::byte{'c'}, std::byte{'t'},
            std::byte{'v'}, std::byte{'1'}, std::byte{'\n'}, std::byte{0} };
        fakeHeaderlessPrefix.insert(fakeHeaderlessPrefix.end(),
            protectedBytes.begin(), protectedBytes.end());
        const assets::SedRecoveryLoadResult embeddedRecovery =
            assets::recoverFromSedBytes(fakeHeaderlessPrefix);
        check(assets::sedLevelsEqual(embeddedRecovery.level, source)
                && embeddedRecovery.missingRequiredSections.empty(),
            "SED embedded recovery: a bare section-magic prefix cannot mask an intact document");

        Level second = richLevel(ProjectType::IJIM);
        const std::vector<std::byte> secondProtected = assets::saveToSedBytes(second);
        const auto protectedSections = assets::splitSedFileSections(protectedBytes);
        check(protectedSections.size() >= 2,
            "SED embedded recovery: source provides a checked HEAD fragment");
        if (protectedSections.size() >= 2)
        {
            std::vector<std::byte> checkedFragmentPrefix = protectedSections[1];
            checkedFragmentPrefix.insert(checkedFragmentPrefix.end(),
                secondProtected.begin(), secondProtected.end());
            const assets::SedRecoveryLoadResult fromCheckedFragment =
                assets::recoverFromSedBytes(checkedFragmentPrefix);
            check(assets::sedLevelsEqual(fromCheckedFragment.level, second)
                    && fromCheckedFragment.missingRequiredSections.empty(),
                "SED embedded recovery: a checked headerless HEAD fragment cannot mask a later complete document");
        }

        std::vector<std::byte> staleHeaderPrefix(
            protectedBytes.begin(), protectedBytes.begin() + kTestHeaderSize);
        staleHeaderPrefix.insert(staleHeaderPrefix.end(),
            secondProtected.begin(), secondProtected.end());
        const assets::SedRecoveryLoadResult fromStaleHeader =
            assets::recoverFromSedBytes(staleHeaderPrefix);
        check(assets::sedLevelsEqual(fromStaleHeader.level, second)
                && fromStaleHeader.missingRequiredSections.empty(),
            "SED embedded recovery: a checksum-valid stale header cannot mask a later complete document");

        std::vector<std::byte> schemaInvalid = assets::saveToSedBytes(source);
        const SectionLayout head = findSection(schemaInvalid, { 'h', 'e', 'a', 'd' });
        const auto patchType = [&](const std::size_t descriptorOffset)
        {
            for (std::size_t i = 0; i < 4; ++i)
            {
                schemaInvalid[descriptorOffset + 8 + i]
                    = static_cast<std::byte>(static_cast<unsigned char>("sect"[i]));
            }
            writeU32Le(schemaInvalid, descriptorOffset + kTestSectionDescriptorSize - 4,
                assets::calculateSedCrc32c(std::span<const std::byte>(schemaInvalid).subspan(
                    descriptorOffset, kTestSectionDescriptorSize - 4)));
        };
        patchType(head.descriptorOffset);
        patchType(head.trailerOffset + 12);
        writeU32Le(schemaInvalid, head.trailerOffset + kTestSectionTrailerSize - 4,
            assets::calculateSedCrc32c(std::span<const std::byte>(schemaInvalid).subspan(
                head.trailerOffset, kTestSectionTrailerSize - 4)));

        bool recoveryRejected = false;
        try { (void)assets::recoverFromSedBytes(schemaInvalid); }
        catch (const std::exception&) { recoveryRejected = true; }
        check(recoveryRejected,
            "SED recovery classification: checksum-valid semantic/schema errors remain hard");
    }

    {
        std::vector<std::byte> disagreement = assets::saveToSedBytes(
            richLevel(ProjectType::JKDF2));
        constexpr std::size_t headerMirrorSize = 40;
        constexpr std::size_t footerSize = 8;
        const std::size_t mirrorOffset = disagreement.size()
            - footerSize - headerMirrorSize;
        const std::size_t mirrorHeaderOffset = mirrorOffset + 8;
        writeU32Le(disagreement, mirrorHeaderOffset + 8,
            static_cast<std::uint32_t>(ProjectType::MOTS));
        writeU32Le(disagreement, mirrorHeaderOffset + 24,
            assets::calculateSedCrc32c(std::span<const std::byte>(disagreement).subspan(
                mirrorHeaderOffset, 24)));
        writeU32Le(disagreement, mirrorOffset + 36,
            assets::calculateSedCrc32c(std::span<const std::byte>(disagreement).subspan(
                mirrorHeaderOffset, kTestHeaderSize)));
        bool rejected = false;
        try { (void)assets::recoverFromSedBytes(disagreement); }
        catch (const std::exception&) { rejected = true; }
        check(rejected,
            "SED metadata mirrors: two valid but disagreeing fixed headers hard-fail");
    }

    {
        std::vector<std::byte> invalidInnerMirror = assets::saveToSedBytes(
            richLevel(ProjectType::JKDF2));
        constexpr std::size_t headerMirrorSize = 40;
        constexpr std::size_t footerSize = 8;
        const std::size_t mirrorOffset = invalidInnerMirror.size()
            - footerSize - headerMirrorSize;
        const std::size_t mirrorHeaderOffset = mirrorOffset + 8;

        // Make the primary unusable, then give the mirror a valid outer CRC
        // around an embedded header whose own CRC is deliberately wrong.
        // The two integrity domains must both validate before the mirror can
        // become authoritative framing.
        invalidInnerMirror[24] ^= std::byte{0x80};
        invalidInnerMirror[mirrorHeaderOffset + 24] ^= std::byte{0x80};
        writeU32Le(invalidInnerMirror, mirrorOffset + 36,
            assets::calculateSedCrc32c(
                std::span<const std::byte>(invalidInnerMirror).subspan(
                    mirrorHeaderOffset, kTestHeaderSize)));
        bool rejected = false;
        try { (void)assets::loadFromSedBytes(invalidInnerMirror); }
        catch (const std::exception&) { rejected = true; }
        check(rejected,
            "SED metadata mirrors: an outer-valid mirror with an invalid embedded header CRC cannot replace a damaged primary header");
    }

    {
        const Level source = richLevel(ProjectType::IJIM);
        const std::vector<std::byte> bytes = assets::saveToSedBytes(source);
        std::vector<std::byte> zeroedTrailingFraming = bytes;
        std::fill(zeroedTrailingFraming.end() - 48,
            zeroedTrailingFraming.end(), std::byte{0});
        bool zeroedTrailingFramingRejected = false;
        try { (void)assets::loadFromSedBytes(zeroedTrailingFraming); }
        catch (const std::exception&) { zeroedTrailingFramingRejected = true; }
        const assets::SedRecoveryLoadResult zeroedTrailingRecovery =
            assets::recoverFromSedBytes(zeroedTrailingFraming);
        check(zeroedTrailingFramingRejected
                && zeroedTrailingRecovery.hadCorruption
                && !zeroedTrailingRecovery.partialRecoveryFromCorruption
                && zeroedTrailingRecovery.missingRequiredSections.empty()
                && zeroedTrailingRecovery.level.sectors.size() == source.sectors.size(),
            "SED framing: strict rejects missing mandatory mirror/footer while recovery keeps Level");

        std::vector<std::byte> damagedMirrorCrcAndFooter = bytes;
        constexpr std::size_t headerMirrorSize = 40;
        constexpr std::size_t footerSize = 8;
        const std::size_t mirrorOffset = damagedMirrorCrcAndFooter.size()
            - headerMirrorSize - footerSize;
        damagedMirrorCrcAndFooter[mirrorOffset + headerMirrorSize - 1]
            ^= std::byte{0x80};
        damagedMirrorCrcAndFooter[damagedMirrorCrcAndFooter.size() - footerSize]
            ^= std::byte{0x01};
        const assets::SedRecoveryLoadResult damagedFramingRecovery =
            assets::recoverFromSedBytes(damagedMirrorCrcAndFooter);
        check(damagedFramingRecovery.hadCorruption
                && !damagedFramingRecovery.partialRecoveryFromCorruption
                && damagedFramingRecovery.unidentifiedMissingSections == 0
                && damagedFramingRecovery.missingRequiredSections.empty()
                && assets::sedLevelsEqual(damagedFramingRecovery.level, source),
            "SED framing: bad mirror CRC plus bad footer remains identified framing-only damage");

        std::vector<std::byte> lostFraming = bytes;
        std::fill_n(lostFraming.begin(), kTestHeaderSize, std::byte{0});
        std::fill(lostFraming.end() - 48, lostFraming.end(), std::byte{0});
        const assets::SedRecoveryLoadResult recovered =
            assets::recoverFromSedBytes(lostFraming);
        check(recovered.hadCorruption && recovered.missingRequiredSections.empty()
                && recovered.level.sectors.size() == source.sectors.size()
                && recovered.level.kind == source.kind,
            "SED recovery resync: checked section anchors survive both header copies/footer loss");

        std::vector<std::byte> ambiguous = bytes;
        const SectionLayout head = findSection(ambiguous, { 'h', 'e', 'a', 'd' });
        const std::size_t mirrorDescriptor = head.trailerOffset + 12;
        for (std::size_t i = 0; i < 4; ++i)
        {
            ambiguous[mirrorDescriptor + 8 + i]
                = static_cast<std::byte>(static_cast<unsigned char>("sect"[i]));
        }
        writeU32Le(ambiguous, mirrorDescriptor + kTestSectionDescriptorSize - 4,
            assets::calculateSedCrc32c(std::span<const std::byte>(ambiguous).subspan(
                mirrorDescriptor, kTestSectionDescriptorSize - 4)));
        writeU32Le(ambiguous, head.trailerOffset + kTestSectionTrailerSize - 4,
            assets::calculateSedCrc32c(std::span<const std::byte>(ambiguous).subspan(
                head.trailerOffset, kTestSectionTrailerSize - 4)));
        bool strictRejected = false;
        try { (void)assets::loadSedFileBytes(ambiguous); }
        catch (const std::exception&) { strictRejected = true; }
        check(strictRejected,
            "SED descriptor ambiguity: strict load never chooses a valid disagreeing copy");
        const assets::SedRecoveryLoadResult partial = assets::recoverFromSedBytes(ambiguous);
        check(partial.partialRecoveryFromCorruption
                && partial.unidentifiedMissingSections >= 1
                && std::find(partial.missingRequiredSections.begin(),
                    partial.missingRequiredSections.end(), "head")
                    != partial.missingRequiredSections.end()
                && partial.level.materials.size() == source.materials.size(),
            "SED descriptor ambiguity: recovery discards one unit and resyncs checked sections");
    }

    Level packed = richLevel(ProjectType::IJIM);
    packed.lvisstring = deterministicNoise(68'000);
    const std::vector<std::byte> packedBytes = assets::saveToSedBytes(packed);
    const SectionLayout packedHead = findSection(packedBytes, { 'h', 'e', 'a', 'd' });
    check(packedHead.dataStreams == 9 && packedHead.stripeCount == 2,
        "SED adaptive packing: seventeen logical chunks use k=9 and two stripes");

    Level striped = richLevel(ProjectType::IJIM);
    striped.lvisstring = deterministicNoise(200'000);
    const std::vector<std::byte> stripedBytes = assets::saveToSedBytes(striped);
    const SectionLayout stripedHead = findSection(stripedBytes, { 'h', 'e', 'a', 'd' });
    check(stripedHead.stripeCount >= 3,
        "SED striped recovery: large required section has independent stripes");
    std::vector<std::byte> damagedStripes = stripedBytes;
    for (std::size_t stripe = 0; stripe < stripedHead.stripeCount; ++stripe)
    {
        const std::array<std::size_t, 1> stream{ 0 };
        damageSlots(damagedStripes, stripedHead, stream, stripe);
    }
    const assets::SedFile repairedStripes = assets::loadSedFileBytes(damagedStripes);
    check(repairedStripes.level.lvisstring == striped.lvisstring
            && repairedStripes.repairedChunks == stripedHead.stripeCount,
        "SED striped recovery: corruption across many independent stripes repairs");

    damagedStripes = stripedBytes;
    damageFirstSlots(damagedStripes, stripedHead, 5);
    bool stripedRejected = false;
    try { (void)assets::loadSedFileBytes(damagedStripes); }
    catch (const std::exception&) { stripedRejected = true; }
    check(stripedRejected,
        "SED striped recovery: five erasures in one strong stripe remain unrecoverable");
    const assets::SedRecoveryLoadResult stripedPartial =
        assets::recoverFromSedBytes(damagedStripes);
    check(stripedPartial.partialRecoveryFromCorruption
            && std::find(stripedPartial.missingRequiredSections.begin(),
                stripedPartial.missingRequiredSections.end(), "head")
                != stripedPartial.missingRequiredSections.end(),
        "SED striped recovery: over-budget required unit is explicitly reported");

    {
        Level grouped = richLevel(ProjectType::IJIM);
        grouped.sectors.clear();
        grouped.things.clear();
        const std::string groupedNoise = deterministicNoise(900u * 192u);
        std::uint64_t numericState = 0x9e37'79b9'7f4a'7c15ull;
        const auto nextNumber = [&numericState]
        {
            numericState ^= numericState >> 12u;
            numericState ^= numericState << 25u;
            numericState ^= numericState >> 27u;
            return static_cast<double>(numericState & ((1ull << 53u) - 1u))
                / static_cast<double>(1ull << 40u);
        };
        for (std::size_t i = 0; i < 900; ++i)
        {
            Sector sector{};
            sector.id = static_cast<std::int32_t>(10'000 + i);
            sector.num = static_cast<std::int32_t>(i);
            for (std::int32_t vertex = 0; vertex < 12; ++vertex)
            {
                sector.vertices.push_back(Vertex{
                    nextNumber(), nextNumber(), nextNumber(), vertex });
            }
            sector.sound = groupedNoise.substr(i * 192u, 96u);
            grouped.sectors.push_back(std::move(sector));
            Thing thing{};
            thing.id = static_cast<std::int32_t>(20'000 + i);
            thing.num = static_cast<std::int32_t>(i);
            thing.templateName = "+derived_actor";
            thing.name = groupedNoise.substr(i * 192u + 96u, 96u);
            thing.x = nextNumber(); thing.y = nextNumber(); thing.z = nextNumber();
            thing.pch = nextNumber(); thing.yaw = nextNumber(); thing.rol = nextNumber();
            thing.bbox.x1 = nextNumber(); thing.bbox.y1 = nextNumber();
            thing.bbox.z1 = nextNumber(); thing.bbox.x2 = nextNumber();
            thing.bbox.y2 = nextNumber(); thing.bbox.z2 = nextNumber();
            thing.layer = 0;
            grouped.things.push_back(std::move(thing));
        }
        const std::vector<std::byte> groupedBytes = assets::saveToSedBytes(grouped);
        const SectionLayout groupedSectors = findSection(groupedBytes, { 's', 'e', 'c', 't' });
        const SectionLayout groupedThings = findSection(groupedBytes, { 't', 'h', 'n', 'g' });
        check(groupedSectors.stripeCount >= 2 && groupedThings.stripeCount >= 2,
            "SED striped Level core: SECT and THNG have independent multi-stripe payloads");
        if (groupedSectors.stripeCount >= 2 && groupedThings.stripeCount >= 2)
        {
            std::vector<std::byte> damaged = groupedBytes;
            for (std::size_t stripe = 0; stripe < groupedSectors.stripeCount; ++stripe)
            {
                const std::array<std::size_t, 1> stream{ 0 };
                damageSlots(damaged, groupedSectors, stream, stripe);
            }
            for (std::size_t stripe = 0; stripe < groupedThings.stripeCount; ++stripe)
            {
                const std::array<std::size_t, 1> stream{ 0 };
                damageSlots(damaged, groupedThings, stream, stripe);
            }
            const assets::SedFile recovered = assets::loadSedFileBytes(damaged);
            check(recovered.level.sectors.size() == grouped.sectors.size()
                    && recovered.level.things.size() == grouped.things.size()
                    && recovered.level.things.back().name == grouped.things.back().name,
                "SED striped Level core: SECT/THNG repair independently across every stripe");
        }
    }

    {
        Level source = richLevel(ProjectType::IJIM);
        // Ensure the exercised section has at least two independent data
        // streams; otherwise a coarse EIO range can accidentally erase only
        // parity slots in the tiny k=1 fixture.
        source.lvisstring = deterministicNoise(68'000);
        const std::vector<std::byte> bytes = assets::saveToSedBytes(source);
        const SectionLayout head = findSection(bytes, { 'h', 'e', 'a', 'd' });
        const SectionLayout resources = findSection(bytes, { 'r', 's', 'r', 'c' });
        const std::size_t firstSlot = head.payloadOffset + head.codedOffset;
        check(head.dataStreams >= 2,
            "SED resilient reader: EIO fixture exposes two data streams");
        check(firstSlot % kTestFecSlotBytes == 0,
            "SED physical layout: protected slot extent is 4 KiB aligned");

        std::vector<std::byte> crcDamaged = bytes;
        const std::array<std::size_t, 1> oneStream{ 0 };
        damageSlots(crcDamaged, head, oneStream, 0, true);
        const assets::SedFile crcRepaired = assets::loadSedFileBytes(crcDamaged);
        check(crcRepaired.level.lvisstring == source.lvisstring
                && crcRepaired.repairedChunks == 1,
            "SED checked slots: corruption confined to a CRC field is repaired");

        std::vector<std::byte> crcConsistentDamage = bytes;
        crcConsistentDamage[firstSlot + 29] ^= std::byte{0x80};
        writeU32Le(crcConsistentDamage, firstSlot + kTestFecPayloadBytes,
            assets::calculateSedCrc32c(std::span<const std::byte>(crcConsistentDamage)
                .subspan(firstSlot, kTestFecPayloadBytes)));
        bool hashRejected = false;
        try { (void)assets::loadSedFileBytes(crcConsistentDamage); }
        catch (const std::exception&) { hashRejected = true; }
        check(hashRejected,
            "SED strong digest: CRC-consistent data corruption strict-fails");
        const assets::SedRecoveryLoadResult hashPartial =
            assets::recoverFromSedBytes(crcConsistentDamage);
        check(hashPartial.partialRecoveryFromCorruption
                && std::find(hashPartial.missingRequiredSections.begin(),
                    hashPartial.missingRequiredSections.end(), "head")
                    != hashPartial.missingRequiredSections.end(),
            "SED strong digest: CRC-consistent corruption loses only its semantic unit");

        const std::array<std::size_t, 5> overwriteStarts{
            firstSlot,
            firstSlot + 1,
            firstSlot + kTestFecSlotBytes - 1,
            head.descriptorOffset,
            head.trailerOffset - kTestFecSlotBytes / 2 };
        for (const std::size_t start : overwriteStarts)
        {
            std::vector<std::byte> overwritten = bytes;
            check(start + kTestFecSlotBytes <= overwritten.size(),
                "SED physical layout: overwrite fixture is in range");
            if (start + kTestFecSlotBytes <= overwritten.size())
            {
                std::fill_n(overwritten.begin() + static_cast<std::ptrdiff_t>(start),
                    kTestFecSlotBytes, std::byte{0x5a});
                const std::size_t end = start + kTestFecSlotBytes;
                const bool damagesDescriptorOrPadding = start < firstSlot
                    && end > head.descriptorOffset;
                const bool damagesTrailer = start
                        < head.trailerOffset + kTestSectionTrailerSize
                    && end > head.trailerOffset;
                const bool damagesCanonicalFraming = damagesDescriptorOrPadding
                    || damagesTrailer;
                if (damagesCanonicalFraming)
                {
                    bool strictRejected = false;
                    try { (void)assets::loadSedFileBytes(overwritten); }
                    catch (const std::exception&) { strictRejected = true; }
                    const assets::SedRecoveryLoadResult repaired =
                        assets::recoverFromSedBytes(overwritten);
                    const bool damagesFollowingPrimary = start
                            < resources.descriptorOffset + kTestSectionDescriptorSize
                        && end > resources.descriptorOffset;
                    check(strictRejected
                            && (!damagesTrailer || damagesFollowingPrimary)
                            && assets::sedLevelsEqual(repaired.level, source)
                            && repaired.missingRequiredSections.empty()
                            && !repaired.partialRecoveryFromCorruption
                            && repaired.unidentifiedMissingSections == 0
                            && repaired.hadCorruption,
                        "SED physical layout: framing-straddling 4 KiB overwrite is strict-fail but mirror-derived recovery preserves the complete Level");
                }
                else
                {
                    const assets::SedFile repaired = assets::loadSedFileBytes(overwritten);
                    check(repaired.level.lvisstring == source.lvisstring
                            && repaired.level.sectors.size() == source.sectors.size()
                            && repaired.hadCorruption,
                        "SED physical layout: localized 4 KiB overwrite repairs at payload/descriptor boundary");
                }
            }
        }

        std::vector<std::byte> lostTailFraming = bytes;
        const SectionLayout hierarchy = findSection(bytes, { 'h', 'n', 'o', 'd' });
        std::fill(lostTailFraming.begin()
                + static_cast<std::ptrdiff_t>(hierarchy.trailerOffset),
            lostTailFraming.end(), std::byte{0});
        bool lostTailRejected = false;
        try { (void)assets::loadFromSedBytes(lostTailFraming); }
        catch (const std::exception&) { lostTailRejected = true; }
        const auto tailRecovered = assets::recoverFromSedBytes(lostTailFraming);
        check(lostTailRejected
                && tailRecovered.level.h3doNodes.size() == source.h3doNodes.size()
                && tailRecovered.missingRequiredSections.empty()
                && tailRecovered.hadCorruption,
            "SED framing: strict rejects damaged final trailer while recovery keeps HNOD");

        // A complete final payload with a damaged trailer is still recoverable
        // in permissive mode, but the valid primary descriptor does not make
        // strict V1 opening accept the bad trailer.
        std::vector<std::byte> damagedFinalTrailer = bytes;
        damagedFinalTrailer[hierarchy.trailerOffset] ^= std::byte{ 0x01 };
        bool damagedFinalTrailerRejected = false;
        try { (void)assets::loadFromSedBytes(damagedFinalTrailer); }
        catch (const std::exception&) { damagedFinalTrailerRejected = true; }
        const auto recoveredDamagedFinalTrailer = assets::recoverFromSedBytes(
            damagedFinalTrailer);
        check(damagedFinalTrailerRejected
                && recoveredDamagedFinalTrailer.level.h3doNodes.size()
                    == source.h3doNodes.size()
                && recoveredDamagedFinalTrailer.missingRequiredSections.empty(),
            "SED framing: damaged final trailer is strict-fail but payload survives recovery");

        // The strict framing exception above is deliberately narrow: a
        // zero-filled, fixed-size mirror/footer suffix is recognizable as
        // framing damage only after every declared semantic section has
        // passed its own checksum and digest. Arbitrary trailing bytes must
        // remain a hard failure rather than being silently accepted.
        std::vector<std::byte> nonZeroTail = lostTailFraming;
        nonZeroTail.back() = std::byte{0x01};
        bool nonZeroTailRejected = false;
        try
        {
            (void)assets::loadSedFileBytes(nonZeroTail);
        }
        catch (const std::exception&)
        {
            nonZeroTailRejected = true;
        }
        check(nonZeroTailRejected,
            "SED framing: arbitrary trailing bytes remain strict-fail");

        // Remove the final trailer while leaving the checked mirror/footer in
        // place. This specifically exercises the missing-trailer path with a
        // valid primary descriptor and valid file framing.
        std::vector<std::byte> missingFinalTrailer = bytes;
        missingFinalTrailer.erase(missingFinalTrailer.begin()
                + static_cast<std::ptrdiff_t>(hierarchy.trailerOffset),
            missingFinalTrailer.begin() + static_cast<std::ptrdiff_t>(
                hierarchy.trailerOffset + kTestSectionTrailerSize));
        bool missingFinalTrailerRejected = false;
        try { (void)assets::loadFromSedBytes(missingFinalTrailer); }
        catch (const std::exception&) { missingFinalTrailerRejected = true; }
        const auto recoveredMissingFinalTrailer = assets::recoverFromSedBytes(
            missingFinalTrailer);
        check(missingFinalTrailerRejected
                && recoveredMissingFinalTrailer.level.h3doNodes.size()
                    == source.h3doNodes.size()
                && recoveredMissingFinalTrailer.missingRequiredSections.empty(),
            "SED framing: missing final trailer is strict-fail but payload survives recovery");

        std::vector<std::byte> truncatedFinalTrailer = bytes;
        truncatedFinalTrailer.resize(hierarchy.trailerOffset + kTestSectionTrailerSize - 7);
        bool finalTrailerRejected = false;
        try { (void)assets::loadFromSedBytes(truncatedFinalTrailer); }
        catch (const std::exception&) { finalTrailerRejected = true; }
        const auto recoveredFinalTrailer = assets::recoverFromSedBytes(
            truncatedFinalTrailer);
        check(finalTrailerRejected
                && recoveredFinalTrailer.level.h3doNodes.size()
                    == source.h3doNodes.size()
                && recoveredFinalTrailer.missingRequiredSections.empty(),
            "SED framing: final section trailer truncation keeps validated payload in recovery");

        const std::vector<std::byte> compactRichBytes = assets::saveToSedBytes(
            richLevel(ProjectType::IJIM));
        check(compactRichBytes.size() < 256u * 1024u,
            "SED overhead: a rich tiny Level remains below 256 KiB with 16+4 protection");
        check(bytes.size() < 512u * 1024u,
            "SED overhead: the enlarged 68 KiB multi-stream fixture remains below 512 KiB with 16+4 protection");

        const std::uint64_t failedBlock = firstSlot;
        std::size_t attempts{};
        const assets::SedRangeReader transientReader = [&](const std::uint64_t offset,
            const std::span<std::byte> destination)
        {
            if (offset == failedBlock && attempts++ < 2) return false;
            std::copy_n(bytes.begin() + static_cast<std::ptrdiff_t>(offset),
                destination.size(), destination.begin());
            return true;
        };
        std::optional<assets::SedFile> retried{};
        try { retried = assets::loadSedFileResilient(bytes.size(), transientReader); }
        catch (const std::exception&) {}
        check(retried && retried->level.sectors.size() == source.sectors.size()
                && retried->unreadableRanges.empty() && attempts == 3,
            "SED resilient reader: bounded retries avoid declaring transient I/O loss");

        const assets::SedRangeReader unreadableReader = [&](const std::uint64_t offset,
            const std::span<std::byte> destination)
        {
            if (offset == failedBlock) return false;
            std::copy_n(bytes.begin() + static_cast<std::ptrdiff_t>(offset),
                destination.size(), destination.begin());
            return true;
        };
        std::optional<assets::SedFile> resilient{};
        try { resilient = assets::loadSedFileResilient(bytes.size(), unreadableReader); }
        catch (const std::exception&) {}
        check(resilient && resilient->level.sectors.size() == source.sectors.size()
                && resilient->repairedChunks >= 1
                && resilient->unreadableRanges.size() == 1
                && resilient->unreadableRanges[0].offset == failedBlock
                && resilient->unreadableRanges[0].length == kTestFecSlotBytes,
            "SED resilient reader: persistent 4 KiB I/O erasure repairs and is reported");

        // Use 4 KiB reader blocks so the two failed reads correspond to the
        // first two independent *data* streams in this enlarged HEAD fixture.
        // The resilient reader coalesces adjacent unreadable blocks into one
        // 8 KiB range; this must exercise MDS reconstruction rather than
        // merely erasing two parity slots in one coarse request.
        const std::uint64_t twoSlotBlock = firstSlot;
        const std::uint64_t twoSlotBlockNext = firstSlot + kTestFecSlotBytes;
        const assets::SedRangeReader twoSlotReader = [&](const std::uint64_t offset,
            const std::span<std::byte> destination)
        {
            if (offset == twoSlotBlock || offset == twoSlotBlockNext) return false;
            std::copy_n(bytes.begin() + static_cast<std::ptrdiff_t>(offset),
                destination.size(), destination.begin());
            return true;
        };
        std::optional<assets::SedFile> twoSlotRecovered{};
        try
        {
            twoSlotRecovered = assets::loadSedFileResilient(
                bytes.size(), twoSlotReader,
                assets::SedResilientReadOptions{ kTestFecSlotBytes, 2 });
        }
        catch (const std::exception&) {}
        check(twoSlotRecovered
                && twoSlotRecovered->level.sectors.size() == source.sectors.size()
                && twoSlotRecovered->repairedChunks >= 2
                && twoSlotRecovered->unreadableRanges.size() == 1
                && twoSlotRecovered->unreadableRanges[0].length == 8192,
            "SED resilient reader: one unreadable range spanning two slots repairs");

        const std::uint64_t crossingBlock = firstSlot - kTestFecSlotBytes;
        const assets::SedRangeReader metadataReader = [&](const std::uint64_t offset,
            const std::span<std::byte> destination)
        {
            if (offset == crossingBlock) return false;
            std::copy_n(bytes.begin() + static_cast<std::ptrdiff_t>(offset),
                destination.size(), destination.begin());
            return true;
        };
        std::optional<assets::SedFile> metadataRecovered{};
        try { metadataRecovered = assets::loadSedFileResilient(bytes.size(), metadataReader); }
        catch (const std::exception&) {}
        check(metadataRecovered && metadataRecovered->level.kind == source.kind
                && metadataRecovered->hadCorruption
                && metadataRecovered->unreadableRanges.size() == 1,
            "SED resilient reader: unreadable metadata/alignment block uses descriptor mirrors");
    }

    Level repetitive = richLevel(ProjectType::IJIM);
    repetitive.materials.assign(500'000, "very_repetitive_material_name.mat");
    const std::vector<std::byte> compressed = assets::saveToSedBytes(repetitive);
    const std::vector<std::byte> uncompressed = assets::saveToSedBytes(
        repetitive, {}, assets::SedSerializationProfile::UnprotectedNested);
    check(compressed.size() < uncompressed.size() / 2,
        "SED compression: repeated resources outweigh protected-slot overhead");
    {
        const SectionLayout resources = findSection(compressed, { 'r', 's', 'r', 'c' });
        check((readU32Le(std::span<const std::byte>(compressed),
                    resources.descriptorOffset + 16) & 1u) != 0,
            "SED compression: repetitive resource section uses compressed payload");
        std::vector<std::byte> malformedCompressed = compressed;
        const std::size_t compressedSlot = resources.payloadOffset + resources.codedOffset;
        malformedCompressed[compressedSlot] ^= std::byte{ 0xff };
        writeU32Le(malformedCompressed, compressedSlot + kTestFecPayloadBytes,
            assets::calculateSedCrc32c(std::span<const std::byte>(malformedCompressed)
                .subspan(compressedSlot, kTestFecPayloadBytes)));
        bool compressedRejected = false;
        try { (void)assets::loadSedFileBytes(malformedCompressed); }
        catch (const std::exception&) { compressedRejected = true; }
        check(compressedRejected,
            "SED compression: CRC-consistent malformed stream is strict-fail");
        const auto compressedPartial = assets::recoverFromSedBytes(malformedCompressed);
        check(std::find(compressedPartial.missingRequiredSections.begin(),
                    compressedPartial.missingRequiredSections.end(), "rsrc")
                != compressedPartial.missingRequiredSections.end(),
            "SED compression: malformed stream drops only its resource section");

        std::vector<std::byte> nonCanonicalLayout = compressed;
        const auto rewriteResourceDescriptorChecksums = [&]()
        {
            writeU32Le(nonCanonicalLayout,
                resources.descriptorOffset + kTestSectionDescriptorSize - 4,
                assets::calculateSedCrc32c(std::span<const std::byte>(nonCanonicalLayout)
                    .subspan(resources.descriptorOffset, kTestSectionDescriptorSize - 4)));
            const std::size_t mirror = resources.trailerOffset + 12;
            writeU32Le(nonCanonicalLayout, mirror + kTestSectionDescriptorSize - 4,
                assets::calculateSedCrc32c(std::span<const std::byte>(nonCanonicalLayout)
                    .subspan(mirror, kTestSectionDescriptorSize - 4)));
            writeU32Le(nonCanonicalLayout,
                resources.trailerOffset + kTestSectionTrailerSize - 4,
                assets::calculateSedCrc32c(std::span<const std::byte>(nonCanonicalLayout)
                    .subspan(resources.trailerOffset, kTestSectionTrailerSize - 4)));
        };
        // Keep the descriptor CRCs valid while making the adaptive layout
        // inconsistent with logicalStoredSize. This is a schema error, not
        // a recoverable payload erasure.
        const std::uint32_t originalDataStreams = readU32Le(
            std::span<const std::byte>(compressed), resources.descriptorOffset + 44);
        const std::uint32_t wrongDataStreams = originalDataStreams == 1 ? 2u : 1u;
        check(originalDataStreams != wrongDataStreams,
            "SED layout: canonical fixture exposes the adaptive data-stream count");
        writeU32Le(nonCanonicalLayout, resources.descriptorOffset + 44,
            wrongDataStreams);
        writeU32Le(nonCanonicalLayout, resources.trailerOffset + 12 + 44,
            wrongDataStreams);
        rewriteResourceDescriptorChecksums();
        bool layoutRejected = false;
        try { (void)assets::recoverFromSedBytes(nonCanonicalLayout); }
        catch (const std::exception&) { layoutRejected = true; }
        check(layoutRejected,
            "SED layout: checksum-valid noncanonical adaptive layout remains a hard error");

        // Compression is bounded on write and must be bounded on read as
        // well. Keep both descriptor copies and the trailer CRC-valid while
        // advertising a compressed raw size just above the canonical 64 MiB
        // temporary-buffer limit; this is a schema error, not an erasure.
        std::vector<std::byte> compressedRawTooLarge = compressed;
        writeU32Le(compressedRawTooLarge, resources.descriptorOffset + 20,
            64u * 1024u * 1024u + 1u);
        writeU32Le(compressedRawTooLarge,
            resources.descriptorOffset + kTestSectionDescriptorSize - 4,
            assets::calculateSedCrc32c(std::span<const std::byte>(compressedRawTooLarge)
                .subspan(resources.descriptorOffset, kTestSectionDescriptorSize - 4)));
        const std::size_t rawTooLargeMirror = resources.trailerOffset + 12;
        writeU32Le(compressedRawTooLarge, rawTooLargeMirror + 20,
            64u * 1024u * 1024u + 1u);
        writeU32Le(compressedRawTooLarge,
            rawTooLargeMirror + kTestSectionDescriptorSize - 4,
            assets::calculateSedCrc32c(std::span<const std::byte>(compressedRawTooLarge)
                .subspan(rawTooLargeMirror, kTestSectionDescriptorSize - 4)));
        writeU32Le(compressedRawTooLarge,
            resources.trailerOffset + kTestSectionTrailerSize - 4,
            assets::calculateSedCrc32c(std::span<const std::byte>(compressedRawTooLarge)
                .subspan(resources.trailerOffset, kTestSectionTrailerSize - 4)));
        bool compressedRawTooLargeRejected = false;
        try { (void)assets::recoverFromSedBytes(compressedRawTooLarge); }
        catch (const std::exception&) { compressedRawTooLargeRejected = true; }
        check(compressedRawTooLargeRejected,
            "SED compression: checksum-valid compressed raw size above 64 MiB is a hard schema error");
    }

    {
        // Compression must skip semantic records above the bounded temporary
        // LZSS threshold rather than reserving another near-record-sized
        // buffer. The descriptor remains canonical and parity-protected.
        const std::size_t compressionLimit = 64u * 1024u * 1024u;
        assets::SedOptionalSection largeUncompressible{
            { 'l', 'g', '6', '4' }, {} };
        largeUncompressible.payload.reserve(compressionLimit + 1u);
        largeUncompressible.payload.resize(compressionLimit);
        std::uint32_t state = 0x8f31'7a21u;
        for (std::byte& value : largeUncompressible.payload)
        {
            state ^= state << 13u;
            state ^= state >> 17u;
            state ^= state << 5u;
            value = static_cast<std::byte>(static_cast<unsigned char>(state >> 24u));
        }
        const std::vector<std::byte> losingBoundaryCandidate =
            assets::detail::compressSedLzssForTesting(
                largeUncompressible.payload);
        check(losingBoundaryCandidate.empty(),
            "SED compression: exact 64 MiB incompressible input aborts its losing candidate before FEC allocation");

        largeUncompressible.payload.push_back(std::byte{});
        const std::vector<std::byte> largeBytes = assets::saveToSedBytes(
            richLevel(ProjectType::IJIM),
            std::span<const assets::SedOptionalSection>(&largeUncompressible, 1));
        const SectionLayout largeSection = findSection(largeBytes,
            { 'l', 'g', '6', '4' });
        check((readU32Le(std::span<const std::byte>(largeBytes),
                    largeSection.descriptorOffset + 16) & 1u) == 0,
            "SED compression: >64 MiB semantic section skips temporary compression");
    }

    {
        // HIST has a deliberately smaller V1 raw-size ceiling than required
        // Level/EDIT domains. Exercise the writer boundary with one reusable,
        // highly compressible allocation so this test does not retain multiple
        // limit-sized payloads or protected outputs at once.
        const std::size_t historyLimit = static_cast<std::size_t>(
            assets::SedMaximumHistorySectionBytes);
        assets::SedOptionalSection historyBoundary{
            { 'h', 'i', 's', 't' }, {} };
        historyBoundary.payload.reserve(historyLimit + 1u);
        historyBoundary.payload.resize(historyLimit, std::byte{});

        bool exactLimitAccepted = false;
        try
        {
            const std::vector<std::byte> exactBytes = assets::saveToSedBytes(
                richLevel(ProjectType::IJIM),
                std::span<const assets::SedOptionalSection>(&historyBoundary, 1));
            const SectionLayout history = findSection(exactBytes,
                { 'h', 'i', 's', 't' });
            exactLimitAccepted = readU64Le(
                std::span<const std::byte>(exactBytes),
                history.descriptorOffset + 20) == assets::SedMaximumHistorySectionBytes;
        }
        catch (const std::exception&)
        {
        }
        check(exactLimitAccepted,
            "SED HIST limit: the exact 64 MiB V1 raw boundary is accepted");

        historyBoundary.payload.push_back(std::byte{});
        bool aboveLimitRejected = false;
        try
        {
            (void)assets::saveToSedBytes(
                richLevel(ProjectType::IJIM),
                std::span<const assets::SedOptionalSection>(&historyBoundary, 1));
        }
        catch (const std::runtime_error& ex)
        {
            aboveLimitRejected = std::string_view(ex.what()) == "Binary SED hist section exceeds the V1 64 MiB safety limit.";
        }
        catch (const std::exception&)
        {
        }
        check(aboveLimitRejected,
            "SED HIST limit: a raw payload one byte above 64 MiB is rejected by the writer");
    }

    {
        const std::array optional{
            assets::SedOptionalSection{
                { 'e', 'd', 'i', 't' },
                { std::byte{ 0x01 }, std::byte{ 0x7f }, std::byte{ 0xff } } },
            assets::SedOptionalSection{
                { 'h', 'i', 's', 't' },
                std::vector<std::byte>(32'768, std::byte{ 0x2a }) },
        };
        const Level source = richLevel(ProjectType::IJIM);
        const std::vector<std::byte> bytes = assets::saveToSedBytes(source, optional);
        const assets::SedFile loaded = assets::loadSedFileBytes(bytes);
        check(loaded.optionalSections.size() == optional.size()
                && loaded.optionalSections[0].type == optional[0].type
                && loaded.optionalSections[0].payload == optional[0].payload
                && loaded.optionalSections[1].type == optional[1].type
                && loaded.optionalSections[1].payload == optional[1].payload,
            "SED optional sections: application state and history round trip exactly");
        verifyCriticalFields(loaded.level, ProjectType::IJIM,
            "SED optional sections preserve Level");
        check(assets::splitSedFileSections(bytes).size() == 11,
            "SED optional sections: header plus ten semantic sections split independently");

        for (const std::array<char, 4> type : {
                 std::array<char, 4>{ 'e', 'd', 'i', 't' },
                 std::array<char, 4>{ 'h', 'i', 's', 't' } })
        {
            const SectionLayout section = findSection(bytes, type);
            const std::size_t expectedParity = type
                == std::array<char, 4>{ 'e', 'd', 'i', 't' } ? 4u : 2u;
            check(section.parityStreams == expectedParity,
                "SED optional sections: EDIT uses p4 while HIST uses p2");
            for (std::size_t damageCount = 1;
                 damageCount <= expectedParity; ++damageCount)
            {
                std::vector<std::byte> damaged = bytes;
                damageFirstSlots(damaged, section, damageCount, true);
                const assets::SedFile repaired = assets::loadSedFileBytes(damaged);
                check(repaired.optionalSections.size() == optional.size()
                        && repaired.repairedChunks == damageCount,
                    "SED optional sections: every in-budget EDIT/HIST erasure repairs");
            }

            std::vector<std::byte> damaged = bytes;
            damageFirstSlots(damaged, section, expectedParity + 1u, true);
            bool strictRejected = false;
            try { (void)assets::loadSedFileBytes(damaged); }
            catch (const std::exception&) { strictRejected = true; }
            check(strictRejected,
                "SED optional sections: over-budget same-stripe erasures strict-fail");
            const assets::SedRecoveryLoadResult partial = assets::recoverFromSedBytes(damaged);
            const std::string name(type.data(), type.size());
            check(partial.partialRecoveryFromCorruption
                    && std::find(partial.missingOptionalSections.begin(),
                        partial.missingOptionalSections.end(), name)
                        != partial.missingOptionalSections.end()
                    && partial.missingRequiredSections.empty()
                    && partial.level.sectors.size() == source.sectors.size(),
                "SED optional sections: recovery loses only over-budget optional data");
        }

        // Exercise every one- and two-erasure combination for a full-k=16
        // optional p2 stripe, including data and parity streams.
        assets::SedOptionalSection fullKOptional{
            { 'f', 'k', '1', '6' },
            std::vector<std::byte>(63'000) };
        const std::string fullKNoise = deterministicNoise(fullKOptional.payload.size());
        std::transform(fullKNoise.begin(), fullKNoise.end(), fullKOptional.payload.begin(),
            [](const char value) { return static_cast<std::byte>(value); });
        const std::vector<std::byte> fullKBytes = assets::saveToSedBytes(
            source, std::span<const assets::SedOptionalSection>(&fullKOptional, 1));
        const SectionLayout fullKLayout = findSection(fullKBytes,
            { 'f', 'k', '1', '6' });
        check(fullKLayout.dataStreams == 16 && fullKLayout.parityStreams == 2
                && fullKLayout.stripeCount == 1,
            "SED optional sections: full-k=16 p2 fixture is one stripe");
        // A parity slot can be modified together with a recomputed CRC, so
        // its per-slot checksum alone cannot detect the damage.  The decoder
        // recomputes parity rows from the intact data and reports the anomaly
        // while still returning the complete semantic optional payload.
        std::vector<std::byte> wrongParity = fullKBytes;
        const std::size_t wrongParityOffset = slotOffset(
            fullKLayout, fullKLayout.dataStreams, 0);
        wrongParity[wrongParityOffset + 23] ^= std::byte{0x6d};
        writeU32Le(wrongParity, wrongParityOffset + kTestFecPayloadBytes,
            assets::calculateSedCrc32c(std::span<const std::byte>(wrongParity)
                .subspan(wrongParityOffset, kTestFecPayloadBytes)));
        const assets::SedFile wrongParityLoaded = assets::loadSedFileBytes(wrongParity);
        check(wrongParityLoaded.optionalSections.size() == 1
                && wrongParityLoaded.optionalSections[0].payload
                    == fullKOptional.payload
                && wrongParityLoaded.hadCorruption,
            "SED optional sections: CRC-valid wrong parity is reported without losing data");
        const std::size_t fullKStreamCount = fullKLayout.dataStreams
            + fullKLayout.parityStreams;
        std::vector<std::size_t> fullKSelected{};
        std::size_t fullKFailures{};
        for (std::size_t erasures = 1; erasures <= 2; ++erasures)
        {
            fullKSelected.resize(erasures);
            const auto visit = [&](const auto& self, const std::size_t depth,
                const std::size_t next) -> void
            {
                if (depth == fullKSelected.size())
                {
                    std::vector<std::byte> damaged = fullKBytes;
                    damageSlots(damaged, fullKLayout, fullKSelected, 0, true);
                    try
                    {
                        const assets::SedFile repaired = assets::loadSedFileBytes(damaged);
                        if (repaired.optionalSections.size() != 1
                            || repaired.optionalSections[0].payload
                                != fullKOptional.payload)
                        {
                            ++fullKFailures;
                        }
                    }
                    catch (const std::exception&)
                    {
                        ++fullKFailures;
                    }
                    return;
                }
                const std::size_t remaining = fullKSelected.size() - depth;
                for (std::size_t stream = next;
                    stream + remaining <= fullKStreamCount; ++stream)
                {
                    fullKSelected[depth] = stream;
                    self(self, depth + 1, stream + 1);
                }
            };
            visit(visit, 0, 0);
        }
        check(fullKFailures == 0,
            "SED optional sections: exhaustive full-k=16 p2 erasures repair");

        bool rejected = false;
        try
        {
            const std::array duplicate{
                optional[0], optional[0],
            };
            (void)assets::saveToSedBytes(source, duplicate);
        }
        catch (const std::exception&)
        {
            rejected = true;
        }
        check(rejected, "SED optional sections: duplicate types are rejected");

        rejected = false;
        try
        {
            const std::array collision{
                assets::SedOptionalSection{ { 'h', 'e', 'a', 'd' }, {} },
            };
            (void)assets::saveToSedBytes(source, collision);
        }
        catch (const std::exception&)
        {
            rejected = true;
        }
        check(rejected, "SED optional sections: required-type collisions are rejected");
    }

    {
        // A short final data slot contains defined zero padding. The strict
        // loader rejects a noncanonical physical tail even when recovery can
        // normalize it without spending one of its two p2 erasures.
        std::vector<std::byte> paddingPayload(5000);
        for (std::size_t i = 0; i < paddingPayload.size(); ++i)
        {
            paddingPayload[i] = static_cast<std::byte>(
                static_cast<unsigned char>((i * 131u + 17u) & 0xffu));
        }
        const assets::SedOptionalSection paddingSection{
            { 'p', 'a', 'd', 'd' }, paddingPayload };
        const Level source = richLevel(ProjectType::IJIM);
        const std::vector<std::byte> bytes = assets::saveToSedBytes(
            source, std::span<const assets::SedOptionalSection>(&paddingSection, 1));
        const SectionLayout section = findSection(bytes, { 'p', 'a', 'd', 'd' });
        const std::uint64_t chunkCount = (section.logicalStoredSize
            + kTestFecPayloadBytes - 1) / kTestFecPayloadBytes;
        const std::size_t lastChunk = static_cast<std::size_t>(chunkCount - 1);
        const std::size_t stripe = lastChunk % section.stripeCount;
        const std::size_t stream = lastChunk / section.stripeCount;
        const std::size_t meaningful = static_cast<std::size_t>(section.logicalStoredSize
            - static_cast<std::uint64_t>(lastChunk) * kTestFecPayloadBytes);
        check(meaningful < kTestFecPayloadBytes,
            "SED padding: fixture exposes a short final data slot");
        if (meaningful < kTestFecPayloadBytes)
        {
            std::vector<std::byte> damaged = bytes;
            damaged[slotOffset(section, stream, stripe) + meaningful] ^= std::byte{0x5a};
            bool strictRejected = false;
            try { (void)assets::loadSedFileBytes(damaged); }
            catch (const std::exception&) { strictRejected = true; }
            std::optional<assets::SedRecoveryLoadResult> recovered{};
            try { recovered = assets::recoverFromSedBytes(damaged); }
            catch (const std::exception&) {}
            check(strictRejected && recovered
                    && recovered->optionalSections.size() == 1
                    && recovered->optionalSections[0].payload
                        == paddingPayload
                    && recovered->hadCorruption
                    && !recovered->partialRecoveryFromCorruption
                    && recovered->repairedChunks == 0,
                "SED padding: strict rejects noncanonical final tail and recovery normalizes it");

            // A recomputed CRC authenticates the noncanonical physical slot,
            // not the normalized prefix. Recovery therefore spends one p2
            // erasure even when this fixture changed only the tail.
            std::vector<std::byte> recomputedTail = bytes;
            const std::size_t tailOffset = slotOffset(section, stream, stripe);
            recomputedTail[tailOffset + meaningful] ^= std::byte{0x33};
            writeU32Le(recomputedTail, tailOffset + kTestFecPayloadBytes,
                assets::calculateSedCrc32c(std::span<const std::byte>(recomputedTail)
                    .subspan(tailOffset, kTestFecPayloadBytes)));
            bool recomputedStrictRejected = false;
            try { (void)assets::loadSedFileBytes(recomputedTail); }
            catch (const std::exception&) { recomputedStrictRejected = true; }
            std::optional<assets::SedRecoveryLoadResult> recomputedRecovered{};
            try { recomputedRecovered = assets::recoverFromSedBytes(recomputedTail); }
            catch (const std::exception&) {}
            check(recomputedStrictRejected && recomputedRecovered
                    && recomputedRecovered->optionalSections.size() == 1
                    && recomputedRecovered->optionalSections[0].payload
                        == paddingPayload
                    && recomputedRecovered->hadCorruption
                    && !recomputedRecovered->partialRecoveryFromCorruption
                    && recomputedRecovered->repairedChunks == 1,
                "SED padding: strict rejects recomputed-CRC tail mutation and recovery reconstructs its slot");

            // If the same CRC-valid physical slot also changes meaningful
            // bytes, zeroing only its tail would preserve an unauthenticated
            // semantic prefix. The noncanonical tail localizes the complete
            // slot, so permissive recovery spends one erasure and reconstructs
            // it while strict loading still rejects the physical padding.
            std::vector<std::byte> mixedDamage = bytes;
            mixedDamage[tailOffset + meaningful / 2u] ^= std::byte{0x6d};
            mixedDamage[tailOffset + meaningful] ^= std::byte{0x47};
            writeU32Le(mixedDamage, tailOffset + kTestFecPayloadBytes,
                assets::calculateSedCrc32c(std::span<const std::byte>(mixedDamage)
                    .subspan(tailOffset, kTestFecPayloadBytes)));
            bool mixedStrictRejected = false;
            try { (void)assets::loadSedFileBytes(mixedDamage); }
            catch (const std::exception&) { mixedStrictRejected = true; }
            std::optional<assets::SedRecoveryLoadResult> mixedRecovered{};
            try { mixedRecovered = assets::recoverFromSedBytes(mixedDamage); }
            catch (const std::exception&) {}
            check(mixedStrictRejected && mixedRecovered
                    && mixedRecovered->optionalSections.size() == 1
                    && mixedRecovered->optionalSections[0].payload == paddingPayload
                    && mixedRecovered->hadCorruption
                    && !mixedRecovered->partialRecoveryFromCorruption
                    && mixedRecovered->repairedChunks == 1,
                "SED padding: CRC-valid prefix plus tail damage is localized and FEC-repaired");
        }
    }

    {
        // Adaptive k=9/two-stripe packing leaves one canonical unused data
        // slot. Mutating it and recomputing its CRC must be rejected by strict
        // loading, while permissive recovery normalizes it to zero without
        // spending p2 parity capacity.
        assets::SedOptionalSection unusedPadding{
            { 'u', 'n', 'u', 's' },
            std::vector<std::byte>(68'000) };
        const std::string noise = deterministicNoise(unusedPadding.payload.size());
        std::transform(noise.begin(), noise.end(), unusedPadding.payload.begin(),
            [](const char value) { return static_cast<std::byte>(value); });
        const Level source = richLevel(ProjectType::IJIM);
        const std::vector<std::byte> bytes = assets::saveToSedBytes(source,
            std::span<const assets::SedOptionalSection>(&unusedPadding, 1));
        const SectionLayout section = findSection(bytes, { 'u', 'n', 'u', 's' });
        check(section.dataStreams * section.stripeCount
                > (section.logicalStoredSize + kTestFecPayloadBytes - 1)
                    / kTestFecPayloadBytes,
            "SED padding: fixture exposes an unused adaptive data slot");
        bool exercised = false;
        std::vector<std::byte> damaged = bytes;
        for (std::size_t stream = 0; stream < section.dataStreams && !exercised; ++stream)
        {
            for (std::size_t stripe = 0; stripe < section.stripeCount && !exercised; ++stripe)
            {
                const std::uint64_t logicalOffset =
                    (stream * section.stripeCount + stripe)
                    * static_cast<std::uint64_t>(kTestFecPayloadBytes);
                if (logicalOffset < section.logicalStoredSize) continue;
                const std::size_t offset = slotOffset(section, stream, stripe);
                damaged[offset + 7] ^= std::byte{0x4d};
                writeU32Le(damaged, offset + kTestFecPayloadBytes,
                    assets::calculateSedCrc32c(std::span<const std::byte>(damaged)
                        .subspan(offset, kTestFecPayloadBytes)));
                exercised = true;
            }
        }
        if (exercised)
        {
            bool strictRejected = false;
            try { (void)assets::loadSedFileBytes(damaged); }
            catch (const std::exception&) { strictRejected = true; }
            std::optional<assets::SedRecoveryLoadResult> recovered{};
            try { recovered = assets::recoverFromSedBytes(damaged); }
            catch (const std::exception&) {}
            check(strictRejected && recovered
                    && recovered->optionalSections.size() == 1
                    && recovered->optionalSections[0].payload == unusedPadding.payload
                    && recovered->hadCorruption
                    && !recovered->partialRecoveryFromCorruption
                    && recovered->repairedChunks == 0,
                "SED padding: strict rejects recomputed-CRC unused slot and recovery normalizes it");
        }
    }

    {
        const assets::SedOptionalSection emptySection{ { 'e', 'm', 'p', 't' }, {} };
        const Level source = richLevel(ProjectType::JKDF2);
        const std::vector<std::byte> bytes = assets::saveToSedBytes(
            source, std::span<const assets::SedOptionalSection>(&emptySection, 1));
        const SectionLayout section = findSection(bytes, { 'e', 'm', 'p', 't' });
        constexpr std::array<std::uint8_t, 32> emptySha256{
            0xe3, 0xb0, 0xc4, 0x42, 0x98, 0xfc, 0x1c, 0x14,
            0x9a, 0xfb, 0xf4, 0xc8, 0x99, 0x6f, 0xb9, 0x24,
            0x27, 0xae, 0x41, 0xe4, 0x64, 0x9b, 0x93, 0x4c,
            0xa4, 0x95, 0x99, 0x1b, 0x78, 0x52, 0xb8, 0x55 };
        bool digestMatches = true;
        for (std::size_t i = 0; i < emptySha256.size(); ++i)
        {
            digestMatches = digestMatches
                && std::to_integer<std::uint8_t>(bytes[section.descriptorOffset + 64 + i])
                    == emptySha256[i];
        }
        check(section.logicalStoredSize == 0 && digestMatches,
            "SED SHA-256: empty semantic payload uses the standard known digest");
    }

    {
        const auto directory = std::filesystem::temp_directory_path()
            / ("sed-atomic-save-" + std::to_string(
                std::chrono::steady_clock::now().time_since_epoch().count()));
        std::filesystem::create_directory(directory);
        struct Cleanup
        {
            std::filesystem::path path;
            ~Cleanup()
            {
                std::error_code error{};
                std::filesystem::remove_all(path, error);
            }
        } cleanup{directory};
        const auto destination = directory / "level.sed";
        const std::string previous = "previous native document bytes\n";
        const auto writePrevious = [&]
        {
            std::ofstream output(destination, std::ios::binary | std::ios::trunc);
            output << previous;
            output.close();
            check(static_cast<bool>(output), "SED atomic save: sentinel fixture writes successfully");
        };
        Level invalid = richLevel(ProjectType::IJIM);
        invalid.ppunit = std::numeric_limits<double>::quiet_NaN();
        writePrevious();
        bool rejected = false;
        try { assets::saveToSED(invalid, destination); }
        catch ( const std::exception& ) { rejected = true; }
        check(rejected && readTextVector(destination) == previous,
            "SED atomic save: serialization rejection preserves existing destination bytes");
        check(std::distance(std::filesystem::directory_iterator(directory),
                std::filesystem::directory_iterator{}) == 1,
            "SED atomic save: serialization rejection leaves no staging files");

        const Level valid = richLevel(ProjectType::IJIM);
        assets::saveToSED(valid, destination);
        check(readBinaryVector(destination) == assets::saveToSedBytes(valid),
            "SED atomic save: replacing existing destination preserves native canonical bytes");
        std::filesystem::remove(destination);
        rejected = false;
        try { assets::saveToSED(invalid, destination); }
        catch ( const std::exception& ) { rejected = true; }
        check(rejected && !std::filesystem::exists(destination),
            "SED atomic save: serialization rejection never creates an empty destination");

        std::filesystem::remove(destination);

        // A directory at the publication path is a deterministic real rename
        // failure on both platforms, independent of permissions or root access.
        std::filesystem::create_directory(destination);
        const auto protectedFile = destination / "existing.sed";
        {
            std::ofstream output(protectedFile, std::ios::binary);
            output << previous;
        }
        rejected = false;
        try { assets::saveToSED(valid, destination); }
        catch ( const std::exception& ) { rejected = true; }
        check(rejected && std::filesystem::is_directory(destination)
                && readTextVector(protectedFile) == previous,
            "SED atomic save: I/O publication failure preserves preexisting destination contents");
        check(std::distance(std::filesystem::directory_iterator(directory),
                std::filesystem::directory_iterator{}) == 1,
            "SED atomic save: I/O publication failure cleans its exclusive staging file");
    }

    const std::filesystem::path filePath = std::filesystem::temp_directory_path()
        / ("sed-binary-v1-" + std::to_string(
            std::chrono::steady_clock::now().time_since_epoch().count()) + ".sed");
    Level source = richLevel(ProjectType::IJIM);
    source.uvCoordinateSpace = UvCoordinateSpace::Normalized;
    Level legacy = source;
    legacy.uvCoordinateSpace = UvCoordinateSpace::TexturePixels;
    bool fileRoundTrip = false;
    bool v2VersionProbe = false;
    bool v1VersionProbe = false;
    bool damagedPrimaryMirrorProbe = false;
    bool truncatedProbeRejected = false;
    try
    {
        assets::saveToSED(source, filePath);
        const Level loaded = assets::loadFromSED(filePath);
        fileRoundTrip = assets::readSedProjectType(filePath) == ProjectType::IJIM
            && assets::saveToSedBytes(loaded) == assets::saveToSedBytes(source);
    }
    catch ( ... )
    {
        fileRoundTrip = false;
    }

    try
    {
        assets::saveToSED(source, filePath);
        v2VersionProbe = assets::readSedBinaryVersion(filePath)
            == assets::SedBinaryVersion;
    }
    catch ( ... )
    {
        v2VersionProbe = false;
    }

    try
    {
        assets::saveToSED(legacy, filePath);
        v1VersionProbe = assets::readSedBinaryVersion(filePath)
            == assets::SedLegacyBinaryVersion;
    }
    catch ( ... )
    {
        v1VersionProbe = false;
    }

    try
    {
        std::vector<std::byte> damagedPrimary =
            assets::saveToSedBytes(legacy);
        damagedPrimary[0] ^= std::byte{ 0x40 };
        {
            std::ofstream output(filePath,
                std::ios::binary | std::ios::trunc);
            output.write(
                reinterpret_cast<const char*>(damagedPrimary.data()),
                static_cast<std::streamsize>(damagedPrimary.size()));
        }
        damagedPrimaryMirrorProbe = assets::readSedBinaryVersion(filePath)
            == assets::SedLegacyBinaryVersion;
    }
    catch ( ... )
    {
        damagedPrimaryMirrorProbe = false;
    }

    try
    {
        {
            std::ofstream output(filePath,
                std::ios::binary | std::ios::trunc);
            const std::array<char, 4> truncated{ 'S', 'E', 'D', 'B' };
            output.write(truncated.data(),
                static_cast<std::streamsize>(truncated.size()));
        }
        try
        {
            (void)assets::readSedBinaryVersion(filePath);
        }
        catch ( const std::exception& )
        {
            truncatedProbeRejected = true;
        }
    }
    catch ( ... )
    {
        truncatedProbeRejected = false;
    }
    std::error_code removeError{};
    const bool removed = std::filesystem::remove(filePath, removeError);
    check(fileRoundTrip,
        "SED file API: path save/load and project probe preserve an existing V2 file exactly");
    check(v2VersionProbe,
        "SED file API: version probe identifies ordinary normalized V2 framing");
    check(v1VersionProbe,
        "SED file API: version probe identifies ordinary texture-pixel V1 framing");
    check(damagedPrimaryMirrorProbe,
        "SED file API: version probe recovers V1 from the checked mirror after primary-header damage");
    check(truncatedProbeRejected,
        "SED file API: version probe rejects a truncated fixed header");
    check(removed && !removeError,
        "SED file API: path-probe fixture cleanup removes the temporary file");

    check(!assets::detail::sedFileSizeWithinLimitForTesting(0)
            && assets::detail::sedFileSizeWithinLimitForTesting(
                assets::SedMaximumFileBytes)
            && !assets::detail::sedFileSizeWithinLimitForTesting(
                assets::SedMaximumFileBytes + 1u)
            && !assets::detail::sedFileSizeWithinLimitForTesting(4ull
                * 1024ull * 1024ull * 1024ull),
        "SED file API: every path probe shares the complete-file admission cap");
}

} // namespace sed::tests
