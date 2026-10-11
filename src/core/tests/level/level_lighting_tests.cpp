#include "test_common.hpp"

#include "sed/level/geometry.hpp"
#include "sed/level/lighting.hpp"

#include <array>
#include <cmath>
#include <limits>
#include <string>
#include <utility>
#include <vector>

namespace sed::tests
{

namespace
{

using MotsBakeMeshInput = std::pair<std::size_t, std::vector<sed::TVector>>;
using MotsBakeLodInput = std::vector<MotsBakeMeshInput>;

sed::Level makeMotsArchBakeLevel(
    const std::string& thingName = "entry",
    const std::string& modelName = "entry.3do",
    const std::int32_t sectorIndex = 0)
{
    sed::Level level{};
    level.kind = sed::ProjectType::MOTS;
    level.sectors.push_back(makeLitTestCubeSector(0.0, sed::ColorBlack));
    if (sectorIndex > 0)
    {
        level.sectors.push_back(makeLitTestCubeSector(2.0, sed::ColorBlack));
    }

    sed::Thing& thing = level.newThing();
    thing.name = thingName;
    thing.sector = sectorIndex;

    sed::TplValue flags{};
    flags.name = "thingflags";
    flags.type = sed::ValueType::integer;
    flags.intValue = 0x8;
    thing.values.push_back(flags);

    sed::TplValue model{};
    model.name = "model3d";
    model.type = sed::ValueType::str;
    model.stringValue = modelName;
    thing.values.push_back(model);
    return level;
}

sed::level::MotsArchLightingBakeTarget makeMotsArchBakeTarget(
    const sed::Level& level,
    const sed::level::MotsArchLightingTopology& topology,
    const std::vector<MotsBakeLodInput>& lodInputs,
    const std::size_t thingIndex = 0u,
    std::string modelName = {})
{
    sed::level::MotsArchLightingBakeTarget target{};
    target.thingIndex = static_cast<std::int32_t>(thingIndex);
    target.thingId = level.things[thingIndex].id;
    target.sectorIndex = level.things[thingIndex].sector;
    target.thingName = level.things[thingIndex].name;
    if (modelName.empty())
    {
        for (const sed::TplValue& value : level.things[thingIndex].values)
        {
            if (value.name == "model3d" || value.name == "model3do")
            {
                modelName = value.asString();
                break;
            }
        }
    }
    target.modelName = std::move(modelName);
    target.topology = topology;
    for (const MotsBakeLodInput& lodInput : lodInputs)
    {
        sed::level::MotsArchLightingBakeGeoset geoset{};
        for (const auto& [sourceMeshIndex, vertices] : lodInput)
        {
            sed::level::MotsArchLightingBakeMesh mesh{};
            mesh.sourceMeshIndex = sourceMeshIndex;
            mesh.worldVertices = vertices;
            geoset.meshes.push_back(std::move(mesh));
        }
        target.lods.push_back(std::move(geoset));
    }
    return target;
}

const sed::MotsArchLightObject* archRecordForThing(
    const sed::Level& level,
    const std::size_t thingIndex) noexcept
{
    if (thingIndex >= level.things.size())
    {
        return nullptr;
    }
    const std::int32_t recordIndex = level.things[thingIndex].archLightingIndex;
    if (recordIndex < 0
        || static_cast<std::size_t>(recordIndex) >= level.motsArchLighting.size())
    {
        return nullptr;
    }
    return &level.motsArchLighting[static_cast<std::size_t>(recordIndex)];
}

bool sameArchRecords(
    const std::vector<sed::MotsArchLightObject>& lhs,
    const std::vector<sed::MotsArchLightObject>& rhs)
{
    if (lhs.size() != rhs.size())
    {
        return false;
    }
    for (std::size_t objectIndex = 0; objectIndex < lhs.size(); ++objectIndex)
    {
        if (lhs[objectIndex].wireIndex != rhs[objectIndex].wireIndex
            || lhs[objectIndex].meshes.size() != rhs[objectIndex].meshes.size())
        {
            return false;
        }
        for (std::size_t meshIndex = 0; meshIndex < lhs[objectIndex].meshes.size(); ++meshIndex)
        {
            const sed::MotsArchLightMesh& left = lhs[objectIndex].meshes[meshIndex];
            const sed::MotsArchLightMesh& right = rhs[objectIndex].meshes[meshIndex];
            if (left.wireIndex != right.wireIndex || left.vertices != right.vertices)
            {
                return false;
            }
        }
    }
    return true;
}

void addMotsArchBakeLight(
    sed::Level& level,
    const std::int32_t flags,
    const sed::TVector& position,
    const double range,
    const double intensity,
    const sed::ColorF& color)
{
    sed::SedLight& light = level.newLight();
    light.flags = flags;
    light.mark = 0;
    light.position = position;
    light.range = range;
    light.intensity = intensity;
    light.color = color;
}

} // namespace

void runLevelLightingTests()
{
    {
        sed::Level level{};
        (void)level.newSector();
        (void)level.newSector();

        sed::Sector& a = level.sectors[0];
        sed::Surface sa{};
        sa.vertices    = { 0 };
        sa.texVertices = { { 0.0f, 0.0f, sed::ColorF{ 0.1f, 0.2f, 0.3f, 0.4f } } };
        a.surfaces.push_back(sa);

        sed::Sector& b = level.sectors[1];
        sed::Surface sb{};
        sb.vertices    = { 0 };
        sb.texVertices = { { 0.0f, 0.0f, sed::ColorF{ 0.9f, 0.8f, 0.7f, 0.6f } } };
        b.surfaces.push_back(sb);

        const std::array<std::int32_t, 1> sectors{ 0 };
        sed::level::levelZeroVertexLights(level, sectors);

        const sed::ColorF ca = level.sectors[0].surfaces[0].texVertices[0].color;
        check(close(ca.r, 0.0f), "levelZeroVertexLights: r");
        check(close(ca.g, 0.0f), "levelZeroVertexLights: g");
        check(close(ca.b, 0.0f), "levelZeroVertexLights: b");
        check(close(ca.a, 0.4f), "levelZeroVertexLights: a preserved");

        const sed::ColorF cb = level.sectors[1].surfaces[0].texVertices[0].color;
        check(close(cb.r, 0.9f), "levelZeroVertexLights: other.r");
        check(close(cb.g, 0.8f), "levelZeroVertexLights: other.g");
        check(close(cb.b, 0.7f), "levelZeroVertexLights: other.b");
        check(close(cb.a, 0.6f), "levelZeroVertexLights: other.a");
    }

    {
        sed::Level level{};
        level.kind = sed::ProjectType::IJIM;
        level.sectors.push_back(makeLitTestCubeSector(0.0, sed::ColorF{ 0.0f, 0.0f, 0.0f, 0.25f }));

        sed::SedLight light{};
        light.flags     = sed::LF_NoBlock;
        light.intensity = 0.2;
        light.range     = 10.0;
        light.position  = sed::TVector{ 0.0, 0.0, 0.0 };
        light.color     = sed::ColorF{ 1.0f, 0.0f, 0.0f, 1.0f };
        level.lights.push_back(light);

        const std::array<std::int32_t, 1> sectors{ 0 };
        sed::level::levelCalcLightingNew(level, sectors);

        const sed::TexVertex& tv = level.sectors[0].surfaces[0].texVertices[0];

        const double dist = std::sqrt(3.0);
        const double expected = light.intensity * sed::sqr((light.range - dist) / light.range);

        check(close(tv.color.r, expected, kRotationEpsilon), "levelCalcLightingNew: r");
        check(close(tv.color.g, 0.0, kExactEpsilon), "levelCalcLightingNew: g");
        check(close(tv.color.b, 0.0, kExactEpsilon), "levelCalcLightingNew: b");
        check(close(tv.color.a, 0.25, kExactEpsilon), "levelCalcLightingNew: alpha preserved");
    }

    {
        sed::Level level{};
        level.kind = sed::ProjectType::IJIM;
        level.sectors.push_back(makeLitTestCubeSector(0.0, sed::ColorF{ 0.0f, 0.0f, 0.0f, 0.25f }));

        sed::SedLight light{};
        light.flags     = sed::LF_NoBlock;
        light.intensity = 0.2;
        light.range     = 10.0;
        light.position  = sed::TVector{ 0.0, 0.0, 0.0 };
        light.color     = sed::ColorF{ 1.0f, 0.0f, 0.0f, 1.0f };
        level.lights.push_back(light);

        const std::array<std::int32_t, 1> sectors{ 0 };
        sed::level::levelCalcLighting(level, sectors);

        const sed::TexVertex& tv = level.sectors[0].surfaces[0].texVertices[0];

        const double dist = std::sqrt(3.0);
        const double expected = light.intensity * sed::sqr((light.range - dist) / light.range);

        check(close(tv.color.r, expected, kRotationEpsilon), "levelCalcLighting: r");
        check(close(tv.color.g, 0.0, kExactEpsilon), "levelCalcLighting: g");
        check(close(tv.color.b, 0.0, kExactEpsilon), "levelCalcLighting: b");
        check(close(tv.color.a, 0.25, kExactEpsilon), "levelCalcLighting: alpha preserved");
    }

    {
        const auto bakeProject = [](const sed::ProjectType project)
        {
            sed::Level level{};
            level.kind = project;
            level.sectors.push_back(makeLitTestCubeSector(
                0.0, sed::ColorF{ 0.0f, 0.0f, 0.0f, 0.45f }));
            sed::SedLight& light = level.newLight();
            // Exercise immediate same-sector portal reachability rather than
            // bypassing it with LF_NoBlock.
            light.flags = 0;
            light.intensity = 0.5;
            light.range = 10.0;
            light.position = sed::TVector{ 0.0, 0.0, 0.0 };
            light.color = sed::ColorF{ 1.0f, 0.2f, 0.0f, 1.0f };
            const std::array<std::int32_t, 1> sectors{ 0 };
            sed::level::levelCalcLightingNew(level, sectors);
            return level;
        };
        const auto brightest = [](const sed::Level& level)
        {
            sed::ColorF out{};
            for (const sed::Surface& surface : level.sectors[0].surfaces)
            {
                for (const sed::TexVertex& texVertex : surface.texVertices)
                {
                    if (texVertex.color.intensity() > out.intensity())
                    {
                        out = texVertex.color;
                    }
                }
            }
            return out;
        };

        const sed::ColorF jkColor = brightest(bakeProject(sed::ProjectType::JKDF2));
        const sed::ColorF motsColor = brightest(bakeProject(sed::ProjectType::MOTS));
        check(jkColor.r > 0.0f
                && close(jkColor.r, jkColor.g)
                && close(jkColor.g, jkColor.b)
                && close(jkColor.a, 1.0f),
            "levelCalcLightingNew: JKDF2 bakes through legacy scalar grayscale colors");
        check(motsColor.r > motsColor.g
                && motsColor.g > motsColor.b
                && close(motsColor.a, 0.45f),
            "levelCalcLightingNew: MotS bakes authored colored light and preserves alpha");
    }

    {
        sed::Level level{};
        (void)level.newSector();

        sed::Sector& sector = level.sectors[0];

        sed::Surface s0{};
        s0.vertices = { 0, 1, 2 };
        s0.texVertices = {
            { 0.0f, 0.0f, sed::ColorF{ 0.2f, 0.2f, 0.2f, 0.2f } },
            { 0.0f, 0.0f, sed::ColorF{ 0.2f, 0.2f, 0.2f, 0.2f } },
            { 0.0f, 0.0f, sed::ColorF{ 0.2f, 0.2f, 0.2f, 0.2f } },
        };
        s0.extraLight = sed::ColorF{ 0.1f, 0.1f, 0.1f, 1.0f };
        sector.surfaces.push_back(s0);

        sed::Surface s1 = s0;
        for (sed::TexVertex& tv : s1.texVertices)
        {
            tv.color = sed::ColorF{ 0.4f, 0.4f, 0.4f, 0.4f };
        }
        s1.extraLight = sed::ColorF{ 0.1f, 0.1f, 0.1f, 1.0f };
        sector.surfaces.push_back(s1);

        const std::array<std::int32_t, 1> sectors{ 0 };
        sed::level::levelCalcSectorAmbients(level, sectors);

        check(close(sector.ambient.r, 0.3f, kRotationEpsilon), "levelCalcSectorAmbients: r");
        check(close(sector.ambient.g, 0.3f, kRotationEpsilon), "levelCalcSectorAmbients: g");
        check(close(sector.ambient.b, 0.3f, kRotationEpsilon), "levelCalcSectorAmbients: b");
        check(close(sector.ambient.a, 0.0f, kRotationEpsilon), "levelCalcSectorAmbients: a");
    }

    {
        sed::Level level{};
        (void)level.newSector();
        sed::Sector& sector = level.sectors[0];
        sed::Surface surface{};
        surface.vertices = { 0 };
        surface.texVertices = { { 0.0f, 0.0f, sed::ColorF{ 0.1f, 0.1f, 0.1f, 0.7f } } };
        surface.extraLight = sed::ColorF{ 0.6f, 0.4f, 0.2f, 1.0f };
        sector.surfaces.push_back(surface);

        const std::array<std::int32_t, 1> sectors{ 0 };
        sed::level::levelCalcSectorAmbients(level, sectors);
        check(close(sector.ambient.r, 0.6f)
                && close(sector.ambient.g, 0.4f)
                && close(sector.ambient.b, 0.2f)
                && close(sector.ambient.a, 0.0f),
            "levelCalcSectorAmbients: average surface Extra Light wins when brighter than baked vertices");
    }

    {
        sed::Level level{};
        (void)level.newSector();
        sed::Sector& sector = level.sectors[0];
        sed::Surface surface{};
        surface.vertices = { 0 };
        surface.texVertices = { { 0.0f, 0.0f, sed::ColorF{ 2.0f, 0.5f, -0.5f, 2.0f } } };
        sector.surfaces.push_back(surface);

        const std::array<std::int32_t, 1> sectors{ 0 };
        sed::level::levelCalcSectorAmbients(level, sectors);
        const sed::ColorF stored = sector.surfaces[0].texVertices[0].color;
        check(close(stored.r, 1.0f) && close(stored.g, 0.5f) && close(stored.b, 0.0f) && close(stored.a, 1.0f),
            "levelCalcSectorAmbients: clamps stored baked color like original SED");
    }

    {
        sed::Level level{};
        (void)level.newSector();

        sed::Sector& sector = level.sectors[0];
        sector.flags |= sed::SECF_NORGBAMBINETLIGHT;

        sed::Surface surface{};
        surface.vertices = { 0, 1, 2 };
        surface.texVertices = {
            { 0.0f, 0.0f, sed::ColorF{ 1.0f, 0.0f, 0.0f, 1.0f } },
            { 0.0f, 0.0f, sed::ColorF{ 1.0f, 0.0f, 0.0f, 1.0f } },
            { 0.0f, 0.0f, sed::ColorF{ 1.0f, 0.0f, 0.0f, 1.0f } },
        };
        surface.extraLight = sed::ColorF{ 0.0f, 0.0f, 0.1f, 1.0f };
        sector.surfaces.push_back(surface);

        const std::array<std::int32_t, 1> sectors{ 0 };
        sed::level::levelCalcSectorAmbients(level, sectors);

        check(close(sector.ambient.r, sector.ambient.g, kRotationEpsilon), "levelCalcSectorAmbients: gray r==g");
        check(close(sector.ambient.g, sector.ambient.b, kRotationEpsilon), "levelCalcSectorAmbients: gray g==b");
        check(close(sector.ambient.r, 1.0f / 3.0f, kRotationEpsilon), "levelCalcSectorAmbients: gray intensity");
    }

    {
        sed::Level level{};
        (void)level.newSector();

        sed::Sector& sector = level.sectors[0];
        sector.flags |= sed::SECF_NOAMBINETLIGHT;
        sector.ambient = sed::ColorF{ 0.9f, 0.8f, 0.7f, 0.0f };

        sed::Surface surface{};
        surface.vertices = { 0 };
        surface.texVertices = { { 0.0f, 0.0f, sed::ColorF{ 0.0f, 0.0f, 0.0f, 0.0f } } };
        surface.extraLight = sed::ColorF{ 0.0f, 0.0f, 0.0f, 0.0f };
        sector.surfaces.push_back(surface);

        const std::array<std::int32_t, 1> sectors{ 0 };
        sed::level::levelCalcSectorAmbients(level, sectors);

        check(close(sector.ambient.r, 0.9f), "levelCalcSectorAmbients: skip r");
        check(close(sector.ambient.g, 0.8f), "levelCalcSectorAmbients: skip g");
        check(close(sector.ambient.b, 0.7f), "levelCalcSectorAmbients: skip b");
        check(close(sector.ambient.a, 0.0f), "levelCalcSectorAmbients: skip a");
    }

    {
        sed::Level level{};
        (void)level.newSector();
        sed::Sector& sector = level.sectors[0];
        sector.ambient = sed::ColorWhite;

        sed::Surface surface{};
        surface.vertices = { 0 };
        surface.texVertices = { { 0.0f, 0.0f, sed::ColorF{ 0.8f, 0.7f, 0.6f, 0.35f } } };
        surface.extraLight = sed::ColorF{ 0.2f, 0.2f, 0.2f, 1.0f };
        sector.surfaces.push_back(surface);

        const std::array<std::int32_t, 1> sectors{ 0 };
        sed::level::levelCalcLightingNew(level, sectors);

        const sed::ColorF color = sector.surfaces[0].texVertices[0].color;
        check(close(color.r, 0.0f) && close(color.g, 0.0f) && close(color.b, 0.0f),
            "levelCalcLightingNew: no valid lights clears stale baked RGB");
        check(close(color.a, 0.35f),
            "levelCalcLightingNew: no valid lights preserves vertex alpha");
        check(close(sector.ambient.r, 0.2f) && close(sector.ambient.g, 0.2f) && close(sector.ambient.b, 0.2f),
            "levelCalcLightingNew: no valid lights refreshes sector ambient");
    }

    {
        sed::Level level{};
        level.kind = sed::ProjectType::IJIM;
        level.sectors.push_back(makeLitTestCubeSector(
            0.0, sed::ColorF{ 0.7f, 0.6f, 0.5f, 0.42f }));
        sed::SedLight& outside = level.newLight();
        outside.position = sed::TVector{ 100.0, 100.0, 100.0 };
        outside.range = 10.0;
        outside.intensity = 1.0;
        sed::SedLight& zeroRange = level.newLight();
        zeroRange.position = sed::TVector{};
        zeroRange.range = 0.0;
        zeroRange.intensity = 1.0;
        sed::SedLight& negativeIntensity = level.newLight();
        negativeIntensity.position = sed::TVector{};
        negativeIntensity.range = 10.0;
        negativeIntensity.intensity = -1.0;

        const std::array<std::int32_t, 1> sectors{ 0 };
        sed::level::levelCalcLightingNew(level, sectors);
        bool cleared = true;
        for (const sed::Surface& surface : level.sectors[0].surfaces)
        {
            for (const sed::TexVertex& texVertex : surface.texVertices)
            {
                cleared = cleared
                    && close(texVertex.color.r, 0.0f)
                    && close(texVertex.color.g, 0.0f)
                    && close(texVertex.color.b, 0.0f)
                    && close(texVertex.color.a, 0.42f);
            }
        }
        check(cleared,
            "levelCalcLightingNew: out-of-sector, zero-range, and negative-intensity lights are invalid");
    }

    {
        sed::Level level{};
        level.kind = sed::ProjectType::IJIM;
        level.sectors.push_back(makeLitTestCubeSector(0.0, sed::ColorF{ 0.0f, 0.0f, 0.0f, 0.25f }));
        level.sectors.push_back(makeLitTestCubeSector(10.0, sed::ColorF{ 0.0f, 0.0f, 0.0f, 0.25f }));

        sed::SedLight& light = level.newLight();
        light.flags = sed::LF_NoBlock;
        light.intensity = 1.0;
        light.range = 4.0;
        light.position = sed::TVector{ 0.0, 0.0, 0.0 };
        light.color = sed::ColorF{ 1.0f, 0.0f, 0.0f, 1.0f };

        const std::array<std::int32_t, 1> firstSector{ 0 };
        sed::level::levelCalcLightingNew(level, firstSector);
        check(level.sectors[0].ambient.r > 0.0f,
            "levelCalcLightingNew: in-range colored light establishes ambient");

        // Keep a valid light in the level, but move it wholly out of this
        // sector's range. A previously observed #330808 ambient must not
        // survive the next pass.
        level.sectors[0].ambient = sed::ColorF{ 0x33 / 255.0f, 0x08 / 255.0f, 0x08 / 255.0f, 0.0f };
        light.position = sed::TVector{ 10.0, 0.0, 0.0 };
        sed::level::levelCalcLightingNew(level, firstSector);

        check(close(level.sectors[0].ambient.r, 0.0f)
                && close(level.sectors[0].ambient.g, 0.0f)
                && close(level.sectors[0].ambient.b, 0.0f),
            "levelCalcLightingNew: colored ambient clears after light leaves sector");
        bool allBlack = true;
        for (const sed::Surface& bakedSurface : level.sectors[0].surfaces)
        {
            for (const sed::TexVertex& texVertex : bakedSurface.texVertices)
            {
                allBlack = allBlack
                    && close(texVertex.color.r, 0.0f)
                    && close(texVertex.color.g, 0.0f)
                    && close(texVertex.color.b, 0.0f);
            }
        }
        check(allBlack, "levelCalcLightingNew: vertex RGB clears after light leaves sector");
    }

    {
        sed::Level baseline{};
        baseline.kind = sed::ProjectType::IJIM;
        baseline.sectors.push_back(makeLitTestCubeSector(
            0.0, sed::ColorF{ 0.0f, 0.0f, 0.0f, 0.4f }));

        sed::SedLight& edited = baseline.newLight();
        edited.flags = sed::LF_NoBlock;
        edited.position = sed::TVector{ -0.5, 0.0, 0.0 };
        edited.range = 1.6;
        edited.intensity = 0.8;
        edited.color = sed::ColorF{ 1.0f, 0.0f, 0.0f, 1.0f };

        sed::SedLight& unchanged = baseline.newLight();
        unchanged.flags = sed::LF_NoBlock;
        unchanged.position = sed::TVector{ 0.0, 0.0, 0.0 };
        unchanged.range = 10.0;
        unchanged.intensity = 0.35;
        unchanged.color = sed::ColorF{ 0.0f, 1.0f, 0.0f, 1.0f };

        const std::array<std::int32_t, 1> sectorIndices{ 0 };
        sed::level::levelCalcLightingNew(baseline, sectorIndices);

        sed::Level partial = baseline;
        partial.lights[0].color = sed::ColorF{ 0.0f, 0.0f, 1.0f, 1.0f };
        sed::Level full = partial;

        const std::array<sed::level::LightingInfluenceSphere, 1> dirtyInfluences{
            sed::level::LightingInfluenceSphere{ sed::TVector{ -0.5, 0.0, 0.0 }, 1.6 },
        };
        const bool completed = sed::level::levelCalcLightingNewInterruptible(
            partial, sectorIndices, {}, dirtyInfluences);
        sed::level::levelCalcLightingNew(full, sectorIndices);

        bool partialMatchesFull = completed;
        bool untouchedVerticesPreserved = true;
        for (std::size_t surfaceIndex = 0; surfaceIndex < partial.sectors[0].surfaces.size(); ++surfaceIndex)
        {
            const sed::Surface& partialSurface = partial.sectors[0].surfaces[surfaceIndex];
            const sed::Surface& fullSurface = full.sectors[0].surfaces[surfaceIndex];
            const sed::Surface& baselineSurface = baseline.sectors[0].surfaces[surfaceIndex];
            for (std::size_t vertexOffset = 0; vertexOffset < partialSurface.vertices.size(); ++vertexOffset)
            {
                const sed::ColorF partialColor = partialSurface.texVertices[vertexOffset].color;
                const sed::ColorF fullColor = fullSurface.texVertices[vertexOffset].color;
                partialMatchesFull = partialMatchesFull
                    && close(partialColor.r, fullColor.r)
                    && close(partialColor.g, fullColor.g)
                    && close(partialColor.b, fullColor.b)
                    && close(partialColor.a, fullColor.a);

                const std::int32_t vertexIndex = partialSurface.vertices[vertexOffset];
                if (partial.sectors[0].vertices[static_cast<std::size_t>(vertexIndex)].x > 0.0)
                {
                    const sed::ColorF baselineColor = baselineSurface.texVertices[vertexOffset].color;
                    untouchedVerticesPreserved = untouchedVerticesPreserved
                        && close(partialColor.r, baselineColor.r)
                        && close(partialColor.g, baselineColor.g)
                        && close(partialColor.b, baselineColor.b)
                        && close(partialColor.a, baselineColor.a);
                }
            }
        }
        check(partialMatchesFull,
            "levelCalcLightingNewInterruptible: dirty-region bake matches a full bake");
        check(untouchedVerticesPreserved,
            "levelCalcLightingNewInterruptible: dirty-region bake preserves untouched vertex output");
        check(close(partial.sectors[0].ambient.r, full.sectors[0].ambient.r)
                && close(partial.sectors[0].ambient.g, full.sectors[0].ambient.g)
                && close(partial.sectors[0].ambient.b, full.sectors[0].ambient.b),
            "levelCalcLightingNewInterruptible: dirty-region bake derives the same sector ambient");
    }

    {
        sed::Level level{};
        level.kind = sed::ProjectType::IJIM;
        level.sectors.push_back(makeLitTestCubeSector(
            0.0, sed::ColorF{ 0.4f, 0.3f, 0.2f, 0.6f }));
        const std::array<std::int32_t, 1> sectorIndices{ 0 };
        const std::array<sed::level::LightingInfluenceSphere, 1> dirtyInfluences{
            sed::level::LightingInfluenceSphere{ sed::TVector{ -0.5, 0.0, 0.0 }, 1.6 },
        };

        const bool completed = sed::level::levelCalcLightingNewInterruptible(
            level, sectorIndices, {}, dirtyInfluences);
        bool dirtyCleared = completed;
        bool untouchedPreserved = true;
        for (const sed::Surface& surface : level.sectors[0].surfaces)
        {
            for (std::size_t vertexOffset = 0; vertexOffset < surface.vertices.size(); ++vertexOffset)
            {
                const std::int32_t vertexIndex = surface.vertices[vertexOffset];
                const sed::Vertex& vertex = level.sectors[0].vertices[static_cast<std::size_t>(vertexIndex)];
                const sed::ColorF color = surface.texVertices[vertexOffset].color;
                if (vertex.x < 0.0)
                {
                    dirtyCleared = dirtyCleared
                        && close(color.r, 0.0f)
                        && close(color.g, 0.0f)
                        && close(color.b, 0.0f)
                        && close(color.a, 0.6f);
                }
                else
                {
                    untouchedPreserved = untouchedPreserved
                        && close(color.r, 0.4f)
                        && close(color.g, 0.3f)
                        && close(color.b, 0.2f)
                        && close(color.a, 0.6f);
                }
            }
        }
        check(dirtyCleared,
            "levelCalcLightingNewInterruptible: partial no-light pass clears its old influence");
        check(untouchedPreserved,
            "levelCalcLightingNewInterruptible: partial no-light pass leaves outside colors intact");
    }

    {
        sed::Level level{};
        level.kind = sed::ProjectType::IJIM;
        level.sectors.push_back(makeLitTestCubeSector(
            0.0, sed::ColorF{ 0.4f, 0.3f, 0.2f, 0.7f }));
        const double nan = std::numeric_limits<double>::quiet_NaN();
        const std::array<sed::level::LightingInfluenceSphere, 5> influences{
            sed::level::LightingInfluenceSphere{ sed::TVector{ -1.0, -1.0, -1.0 }, 0.1 },
            sed::level::LightingInfluenceSphere{ sed::TVector{ 1.0, 1.0, 1.0 }, 0.1 },
            sed::level::LightingInfluenceSphere{ sed::TVector{}, 0.0 },
            sed::level::LightingInfluenceSphere{ sed::TVector{}, nan },
            sed::level::LightingInfluenceSphere{ sed::TVector{ nan, 0.0, 0.0 }, 1.0 },
        };
        const std::array<std::int32_t, 1> sectors{ 0 };
        const bool completed = sed::level::levelCalcLightingNewInterruptible(
            level, sectors, {}, influences);

        std::size_t clearedCount = 0;
        std::size_t preservedCount = 0;
        bool alphaPreserved = completed;
        for (const sed::Surface& surface : level.sectors[0].surfaces)
        {
            for (const sed::TexVertex& texVertex : surface.texVertices)
            {
                alphaPreserved = alphaPreserved && close(texVertex.color.a, 0.7f);
                if (close(texVertex.color.r, 0.0f)
                    && close(texVertex.color.g, 0.0f)
                    && close(texVertex.color.b, 0.0f))
                {
                    ++clearedCount;
                }
                else if (close(texVertex.color.r, 0.4f)
                    && close(texVertex.color.g, 0.3f)
                    && close(texVertex.color.b, 0.2f))
                {
                    ++preservedCount;
                }
            }
        }
        check(clearedCount == 6 && preservedCount == 18 && alphaPreserved,
            "levelCalcLightingNewInterruptible: dirty spheres form a union and ignore zero/nonfinite inputs");
    }

    {
        auto makeAdjoinedLevel = [](const bool blockPortal)
        {
            sed::Level level{};
            level.kind = sed::ProjectType::IJIM;
            level.sectors.push_back(makeLitTestCubeSector(0.0, sed::ColorBlack));
            level.sectors.push_back(makeLitTestCubeSector(2.0, sed::ColorBlack));
            level.sectors[0].surfaces[0].adjoin = sed::SurfaceRef{ 1, 1 };
            level.sectors[1].surfaces[1].adjoin = sed::SurfaceRef{ 0, 0 };
            if (blockPortal)
            {
                level.sectors[0].surfaces[0].adjoinFlags |= sed::SAF_BlockLight;
                level.sectors[1].surfaces[1].adjoinFlags |= sed::SAF_BlockLight;
            }

            sed::SedLight& light = level.newLight();
            light.position = sed::TVector{ 0.0, 0.0, 0.0 };
            light.range = 10.0;
            light.intensity = 1.0;
            light.color = sed::ColorWhite;
            return level;
        };
        const auto hasBakedLight = [](const sed::Sector& sector)
        {
            for (const sed::Surface& surface : sector.surfaces)
            {
                for (const sed::TexVertex& texVertex : surface.texVertices)
                {
                    if (texVertex.color.r > 0.0f
                        || texVertex.color.g > 0.0f
                        || texVertex.color.b > 0.0f)
                    {
                        return true;
                    }
                }
            }
            return false;
        };

        const std::array<std::int32_t, 1> secondSector{ 1 };
        const std::array<sed::level::LightingInfluenceSphere, 1> dirtyInfluences{
            sed::level::LightingInfluenceSphere{ sed::TVector{ 0.0, 0.0, 0.0 }, 10.0 },
        };
        sed::Level openPortal = makeAdjoinedLevel(false);
        (void)sed::level::levelCalcLightingNewInterruptible(
            openPortal, secondSector, {}, dirtyInfluences);
        check(hasBakedLight(openPortal.sectors[1]),
            "levelCalcLightingNew: generation-stamped portal walk reaches an adjoining sector");

        sed::Level blockedPortal = makeAdjoinedLevel(true);
        (void)sed::level::levelCalcLightingNewInterruptible(
            blockedPortal, secondSector, {}, dirtyInfluences);
        check(!hasBakedLight(blockedPortal.sectors[1]),
            "levelCalcLightingNew: generation-stamped portal walk respects Block Light");

        sed::Level noBlockLight = makeAdjoinedLevel(true);
        noBlockLight.lights[0].flags = sed::LF_NoBlock;
        (void)sed::level::levelCalcLightingNewInterruptible(
            noBlockLight, secondSector, {}, dirtyInfluences);
        check(hasBakedLight(noBlockLight.sectors[1]),
            "levelCalcLightingNew: LF_NoBlock bypasses an SAF_BlockLight portal");

        const auto makeCyclicLevel = [](const bool blockExit)
        {
            sed::Level level{};
            level.kind = sed::ProjectType::IJIM;
            for (int i = 0; i < 3; ++i)
            {
                level.sectors.push_back(makeLitTestCubeSector(0.0, sed::ColorBlack));
            }

            // 0 -> 1, then sector 1 encounters the 1 -> 0 back edge before
            // continuing through 1 -> 2. Zero portal normals make these graph
            // fixtures crossed by every test ray without coupling this test to
            // a particular target vertex.
            level.sectors[0].surfaces[0].adjoin = sed::SurfaceRef{ 1, 0 };
            level.sectors[0].surfaces[0].normal = sed::TVector{};
            level.sectors[1].surfaces[0].adjoin = sed::SurfaceRef{ 0, 0 };
            level.sectors[1].surfaces[0].normal = sed::TVector{};
            level.sectors[1].surfaces[1].adjoin = sed::SurfaceRef{ 2, 1 };
            level.sectors[1].surfaces[1].normal = sed::TVector{};
            level.sectors[2].surfaces[1].adjoin = sed::SurfaceRef{ 1, 1 };
            if (blockExit)
            {
                level.sectors[1].surfaces[1].adjoinFlags |= sed::SAF_BlockLight;
                level.sectors[2].surfaces[1].adjoinFlags |= sed::SAF_BlockLight;
            }

            sed::SedLight& light = level.newLight();
            light.position = sed::TVector{};
            light.range = 10.0;
            light.intensity = 1.0;
            light.color = sed::ColorWhite;
            return level;
        };
        const std::array<std::int32_t, 1> cycleTarget{ 2 };

        sed::Level openCycle = makeCyclicLevel(false);
        const bool openCycleCompleted =
            sed::level::levelCalcLightingNewInterruptible(openCycle, cycleTarget, {});
        check(openCycleCompleted && hasBakedLight(openCycle.sectors[2]),
            "levelCalcLightingNew: generation-stamped portal walk crosses a cycle and reaches its target");

        sed::Level blockedCycle = makeCyclicLevel(true);
        const bool blockedCycleCompleted =
            sed::level::levelCalcLightingNewInterruptible(blockedCycle, cycleTarget, {});
        check(blockedCycleCompleted && !hasBakedLight(blockedCycle.sectors[2]),
            "levelCalcLightingNew: generation-stamped portal walk terminates when a cycle has no open target path");
    }

    {
        sed::Level level{};
        level.kind = sed::ProjectType::IJIM;
        level.sectors.push_back(makeLitTestCubeSector(
            0.0, sed::ColorF{ 0.5f, 0.4f, 0.3f, 0.55f }));
        sed::SedLight& light = level.newLight();
        light.flags = sed::LF_NoBlock;
        light.position = sed::TVector{};
        light.range = std::sqrt(3.0);
        light.intensity = 1.0;
        light.color = sed::ColorWhite;
        const std::array<std::int32_t, 1> sectors{ 0 };
        sed::level::levelCalcLightingNew(level, sectors);

        bool allAtStrictRangeAreDark = true;
        for (const sed::Surface& surface : level.sectors[0].surfaces)
        {
            for (const sed::TexVertex& texVertex : surface.texVertices)
            {
                allAtStrictRangeAreDark = allAtStrictRangeAreDark
                    && close(texVertex.color.r, 0.0f)
                    && close(texVertex.color.g, 0.0f)
                    && close(texVertex.color.b, 0.0f)
                    && close(texVertex.color.a, 0.55f);
            }
        }
        check(allAtStrictRangeAreDark,
            "levelCalcLightingNew: vertices exactly at the light range use the strict cutoff");
    }

    {
        sed::Level level{};
        level.kind = sed::ProjectType::IJIM;
        level.sectors.push_back(makeLitTestCubeSector(0.0, sed::ColorBlack));
        level.sectors.push_back(makeLitTestCubeSector(
            4.0, sed::ColorF{ 0.5f, 0.4f, 0.3f, 0.65f }));
        sed::SedLight& light = level.newLight();
        light.flags = sed::LF_NoBlock;
        light.position = sed::TVector{};
        light.range = 10.0;
        light.intensity = 1.0;
        light.color = sed::ColorWhite;

        std::size_t backFacingSurface = level.sectors[1].surfaces.size();
        for (std::size_t i = 0; i < level.sectors[1].surfaces.size(); ++i)
        {
            const sed::Surface& surface = level.sectors[1].surfaces[i];
            const sed::Vertex& v0 = level.sectors[1].vertices[static_cast<std::size_t>(surface.vertices[0])];
            if (surface.normal.Dot(light.position.Sub(v0.x, v0.y, v0.z)) <= 0.0)
            {
                backFacingSurface = i;
                break;
            }
        }
        const std::array<std::int32_t, 1> sectors{ 1 };
        sed::level::levelCalcLightingNew(level, sectors);
        bool rejected = backFacingSurface < level.sectors[1].surfaces.size();
        if (rejected)
        {
            for (const sed::TexVertex& texVertex : level.sectors[1].surfaces[backFacingSurface].texVertices)
            {
                rejected = rejected
                    && close(texVertex.color.r, 0.0f)
                    && close(texVertex.color.g, 0.0f)
                    && close(texVertex.color.b, 0.0f)
                    && close(texVertex.color.a, 0.65f);
            }
        }
        check(rejected, "levelCalcLightingNew: a back-facing target surface rejects the light");
    }

    {
        sed::Level level{};
        level.sectors.push_back(makeLitTestCubeSector(
            0.0, sed::ColorF{ 0.4f, 0.3f, 0.2f, 0.6f }));
        const std::array<std::int32_t, 1> sectors{ 0 };
        int progressCalls = 0;
        const bool completed = sed::level::levelCalcLightingNewInterruptible(
            level,
            sectors,
            [&progressCalls](const std::size_t, const std::size_t)
            {
                ++progressCalls;
                return false;
            });
        check(!completed && progressCalls == 1
                && close(level.sectors[0].surfaces[0].texVertices[0].color.r, 0.4f),
            "levelCalcLightingNewInterruptible: initial callback rejection leaves the bake untouched");
    }

    {
        sed::Level level{};
        level.sectors.push_back(makeLitTestCubeSector(
            0.0, sed::ColorF{ 0.4f, 0.3f, 0.2f, 0.6f }));
        const std::array<std::int32_t, 1> sectors{ 0 };
        int progressCalls = 0;
        const bool completed = sed::level::levelCalcLightingNewInterruptible(
            level,
            sectors,
            [&progressCalls](const std::size_t done, const std::size_t total)
            {
                ++progressCalls;
                return done < total;
            });
        check(!completed && progressCalls == 2
                && close(level.sectors[0].surfaces[0].texVertices[0].color.r, 0.0f),
            "levelCalcLightingNewInterruptible: final callback can reject a completed no-light clear");
    }

    {
        sed::Level level{};
        level.kind = sed::ProjectType::IJIM;
        level.sectors.push_back(makeLitTestCubeSector(0.0, sed::ColorBlack));
        sed::SedLight& light = level.newLight();
        light.flags = sed::LF_NoBlock;
        light.position = sed::TVector{};
        light.range = 10.0;
        light.intensity = 1.0;
        light.color = sed::ColorWhite;
        const std::array<std::int32_t, 1> sectors{ 0 };
        int progressCalls = 0;
        const bool completed = sed::level::levelCalcLightingNewInterruptible(
            level,
            sectors,
            [&progressCalls](const std::size_t done, const std::size_t total)
            {
                ++progressCalls;
                return done < total;
            });
        check(!completed && progressCalls == 2,
            "levelCalcLightingNewInterruptible: final callback can reject a completed light bake");
    }

    {
        sed::Level level{};
        level.sectors.push_back(makeLitTestCubeSector(0.0, sed::ColorBlack));
        level.sectors.push_back(makeLitTestCubeSector(4.0, sed::ColorBlack));
        sed::SedLight& light = level.newLight();
        light.flags = sed::LF_NoBlock;
        light.position = sed::TVector{ 0.0, 0.0, 0.0 };
        light.range = 10.0;

        const std::array<std::int32_t, 2> sectors{ 0, 1 };
        std::size_t lastCompleted = 0;
        const bool completed = sed::level::levelCalcLightingNewInterruptible(
            level,
            sectors,
            [&lastCompleted](const std::size_t done, const std::size_t) -> bool
            {
                lastCompleted = done;
                return done < 1;
            });
        check(!completed && lastCompleted == 1,
            "levelCalcLightingNewInterruptible: cancellation is checked between sectors");
    }

    {
        sed::Level level{};
        level.kind = sed::ProjectType::IJIM;
        level.sectors.push_back(makeLitTestCubeSector(0.0, sed::ColorBlack));
        for (int i = 0; i < 4; ++i)
        {
            sed::SedLight& light = level.newLight();
            light.flags = sed::LF_NoBlock;
            light.position = sed::TVector{ 0.0, 0.0, 0.0 };
            light.range = 10.0;
            light.intensity = 1.0;
        }

        const std::array<std::int32_t, 1> sectors{ 0 };
        int progressCalls = 0;
        std::size_t lastCompleted = 99;
        const bool completed = sed::level::levelCalcLightingNewInterruptible(
            level,
            sectors,
            [&progressCalls, &lastCompleted](const std::size_t done, const std::size_t) -> bool
            {
                ++progressCalls;
                lastCompleted = done;
                return progressCalls < 2;
            });
        check(!completed && progressCalls == 2 && lastCompleted == 0,
            "levelCalcLightingNewInterruptible: cancellation is checked inside a busy sector");
    }

    {
        sed::Level level = makeMotsArchBakeLevel();
        sed::MotsArchLightObject authored{};
        authored.wireIndex = 4;
        authored.meshes.push_back(sed::MotsArchLightMesh{
            7,
            {
                sed::MotsArchLightVertex{ 0.8f, 0.7f, 0.6f, 0.5f },
                sed::MotsArchLightVertex{ 0.4f, 0.3f, 0.2f, 0.1f },
            },
        });
        level.motsArchLighting.push_back(authored);
        level.things[0].archLightingIndex = 0;
        addMotsArchBakeLight(
            level,
            sed::LF_AffectArchThings | sed::LF_NoBlock,
            sed::TVector{ 0.0, 0.0, 0.0 },
            4.0,
            2.0,
            sed::ColorF{ 0.2f, 0.4f, 0.6f, 1.0f });

        const auto target = makeMotsArchBakeTarget(
            level,
            sed::level::MotsArchLightingTopology{ { { 2u } } },
            { { { 0u, { sed::TVector{ 0.0, 0.0, 0.0 }, sed::TVector{ 2.0, 0.0, 0.0 } } } } });
        const std::array<sed::level::MotsArchLightingBakeTarget, 1> targets{ target };
        const auto result = sed::level::levelCalcMotsArchLighting(level, targets);
        check(result.status == sed::level::MotsArchLightingBakeStatus::Applied
                && result.changedThingIndices == std::vector<std::int32_t>{ 0 }
                && result.message
                    == "Updated MotS ARCHLIGHTING from flagged baked lights."
                && level.motsArchLightingBakeBaselines.size() == 1
                && level.motsArchLightingBakeBaselines[0].thingId == level.things[0].id
                && level.motsArchLightingBakeBaselines[0].modelName == "entry.3do"
                && level.motsArchLightingBakeBaselines[0].originalArchLightingIndex == 0
                && level.motsArchLightingBakeBaselines[0].originalArchLighting
                    == std::optional<sed::MotsArchLightObject>{ authored },
            "levelCalcMotsArchLighting: first bake stores exact per-Thing authored ARCH baseline");
        const sed::MotsArchLightObject* baked = archRecordForThing(level, 0u);
        check(baked != nullptr
                && baked->meshes.size() == 1
                && baked->meshes[0].vertices.size() == 2
                && close(baked->meshes[0].vertices[0].mono, 1.6)
                && close(baked->meshes[0].vertices[0].red, 1.1)
                && close(baked->meshes[0].vertices[0].green, 1.4)
                && close(baked->meshes[0].vertices[0].blue, 1.7)
                && close(baked->meshes[0].vertices[1].mono, 0.6)
                && close(baked->meshes[0].vertices[1].red, 0.4)
                && close(baked->meshes[0].vertices[1].green, 0.4)
                && close(baked->meshes[0].vertices[1].blue, 0.4),
            "levelCalcMotsArchLighting: baseline channels add exact uncapped RGB bake vectors");

        const std::size_t recordCount = level.motsArchLighting.size();
        const auto repeated = sed::level::levelCalcMotsArchLighting(level, targets);
        check(repeated.status == sed::level::MotsArchLightingBakeStatus::Noop
                && repeated.changedThingIndices.empty()
                && repeated.message == "MotS ARCH baked-light calculation produced no changes."
                && level.motsArchLighting.size() == recordCount
                && level.motsArchLightingBakeBaselines.size() == 1
                && level.motsArchLightingBakeBaselines[0].originalArchLighting
                    == std::optional<sed::MotsArchLightObject>{ authored },
            "levelCalcMotsArchLighting: repeated bake is idempotent and preserves one exact baseline");
    }

    {
        sed::Level noFlag = makeMotsArchBakeLevel();
        sed::MotsArchLightObject stale{};
        stale.meshes.push_back(sed::MotsArchLightMesh{
            0, { sed::MotsArchLightVertex{ 0.25f, 0.5f, 0.75f, 1.0f } } });
        noFlag.motsArchLighting.push_back(stale);
        noFlag.things[0].archLightingIndex = 0;
        addMotsArchBakeLight(
            noFlag, sed::LF_NoBlock, sed::TVector{}, 4.0, 1.0, sed::ColorWhite);
        const auto noFlagTarget = makeMotsArchBakeTarget(
            noFlag,
            sed::level::MotsArchLightingTopology{ { { 1u } } },
            { { { 0u, { sed::TVector{} } } } });
        const auto noFlagResult = sed::level::levelCalcMotsArchLighting(
            noFlag, std::array<sed::level::MotsArchLightingBakeTarget, 1>{ noFlagTarget });
        const sed::MotsArchLightObject* unchanged = archRecordForThing(noFlag, 0u);
        check(noFlagResult.status == sed::level::MotsArchLightingBakeStatus::Noop
                && unchanged != nullptr
                && unchanged->meshes.size() == 1
                && unchanged->meshes[0].vertices.size() == 1
                && unchanged->meshes[0].vertices[0]
                    == sed::MotsArchLightVertex{ 0.25f, 0.5f, 0.75f, 1.0f }
                && noFlag.motsArchLightingBakeBaselines.empty(),
            "levelCalcMotsArchLighting: ordinary baked lights preserve ARCH without generated provenance");

        sed::Level nonMots = makeMotsArchBakeLevel();
        nonMots.kind = sed::ProjectType::JKDF2;
        const auto nonMotsBefore = nonMots;
        const auto nonMotsTarget = makeMotsArchBakeTarget(
            nonMots,
            sed::level::MotsArchLightingTopology{ { { 1u } } },
            { { { 0u, { sed::TVector{} } } } });
        const auto nonMotsResult = sed::level::levelCalcMotsArchLighting(
            nonMots, std::array<sed::level::MotsArchLightingBakeTarget, 1>{ nonMotsTarget });
        check(nonMotsResult.status == sed::level::MotsArchLightingBakeStatus::Rejected
                && nonMotsResult.message == "MotS ARCH baked-light calculation requires a MotS level."
                && nonMots.things[0].archLightingIndex == nonMotsBefore.things[0].archLightingIndex
                && sameArchRecords(nonMots.motsArchLighting, nonMotsBefore.motsArchLighting),
            "levelCalcMotsArchLighting: non-MotS input rejects failure-atomically with a fixed diagnostic");
    }

    {
        sed::Level level = makeMotsArchBakeLevel("restore", "restore.3do");
        sed::MotsArchLightObject authored{};
        authored.wireIndex = 3;
        authored.meshes.push_back(sed::MotsArchLightMesh{
            0, { sed::MotsArchLightVertex{ 0.3f, 0.2f, 0.1f, 0.4f } } });
        level.motsArchLighting.push_back(authored);
        level.things[0].archLightingIndex = 0;
        addMotsArchBakeLight(
            level, sed::LF_AffectArchThings | sed::LF_NoBlock,
            sed::TVector{}, 4.0, 0.5, sed::ColorWhite);
        const auto target = makeMotsArchBakeTarget(
            level, sed::level::MotsArchLightingTopology{ { { 1u } } },
            { { { 0u, { sed::TVector{} } } } });
        const auto baked = sed::level::levelCalcMotsArchLighting(
            level, std::array<sed::level::MotsArchLightingBakeTarget, 1>{ target });
        level.lights[0].flags = sed::LF_NoBlock;
        const auto restored = sed::level::levelCalcMotsArchLighting(
            level, std::span<const sed::level::MotsArchLightingBakeTarget>{});
        const sed::MotsArchLightObject* record = archRecordForThing(level, 0u);
        check(baked.status == sed::level::MotsArchLightingBakeStatus::Applied
                && restored.status == sed::level::MotsArchLightingBakeStatus::Applied
                && record != nullptr && *record == authored
                && level.things[0].archLightingIndex == 0
                && level.motsArchLightingBakeBaselines.empty(),
            "levelCalcMotsArchLighting: removing the final source restores exact authored object and reference without model resolution");
    }

    {
        sed::Level level = makeMotsArchBakeLevel("shared_bake", "shared_bake.3do");
        sed::MotsArchLightObject authored{};
        authored.wireIndex = 3;
        authored.meshes.push_back(sed::MotsArchLightMesh{
            0, { sed::MotsArchLightVertex{ 0.3f, 0.2f, 0.1f, 0.4f } } });
        level.motsArchLighting.push_back(authored);
        level.things[0].archLightingIndex = 0;
        sed::Thing observer = level.things[0];
        observer.id += 1;
        observer.num += 1;
        observer.name = "shared_observer";
        observer.archLightingIndex = 0;
        // This Thing intentionally shares the authored archive entry without
        // participating in MotS ARCH baking. Effective Arch owners with a
        // declared model must all have a resolved bake target.
        for (sed::TplValue& value : observer.values)
        {
            if (value.name == "thingflags") value.intValue = 0;
        }
        level.things.push_back(observer);
        addMotsArchBakeLight(
            level, sed::LF_AffectArchThings | sed::LF_NoBlock,
            sed::TVector{}, 4.0, 0.5, sed::ColorWhite);
        const auto target = makeMotsArchBakeTarget(
            level, sed::level::MotsArchLightingTopology{ { { 1u } } },
            { { { 0u, { sed::TVector{} } } } });
        const auto baked = sed::level::levelCalcMotsArchLighting(
            level, std::array<sed::level::MotsArchLightingBakeTarget, 1>{ target });
        const bool sharedRecordPreserved = baked.status
                == sed::level::MotsArchLightingBakeStatus::Applied
            && level.things[0].archLightingIndex == 1
            && level.things[1].archLightingIndex == 0
            && level.motsArchLighting.size() == 2
            && level.motsArchLighting[0] == authored
            && level.motsArchLightingBakeBaselines.size() == 1;
        level.lights[0].flags = 0;
        const auto restored = sed::level::levelCalcMotsArchLighting(
            level, std::span<const sed::level::MotsArchLightingBakeTarget>{});
        check(sharedRecordPreserved
                && restored.status == sed::level::MotsArchLightingBakeStatus::Applied
                && level.things[0].archLightingIndex == 0
                && level.things[1].archLightingIndex == 0
                && level.motsArchLighting[0] == authored
                && level.motsArchLightingBakeBaselines.empty(),
            "levelCalcMotsArchLighting: shared authored record survives copy-on-write bake and exact baseline restoration");
    }

    {
        sed::Level level = makeMotsArchBakeLevel("unbound", "unbound.3do");
        addMotsArchBakeLight(
            level, sed::LF_AffectArchThings | sed::LF_NoBlock,
            sed::TVector{}, 4.0, 0.5, sed::ColorWhite);
        const auto target = makeMotsArchBakeTarget(
            level, sed::level::MotsArchLightingTopology{ { { 1u } } },
            { { { 0u, { sed::TVector{} } } } });
        const auto baked = sed::level::levelCalcMotsArchLighting(
            level, std::array<sed::level::MotsArchLightingBakeTarget, 1>{ target });
        level.things[0].x = 5.0;
        level.lights[0].flags = 0;
        const auto restored = sed::level::levelCalcMotsArchLighting(
            level, std::span<const sed::level::MotsArchLightingBakeTarget>{});
        check(baked.status == sed::level::MotsArchLightingBakeStatus::Applied
                && restored.status == sed::level::MotsArchLightingBakeStatus::Applied
                && level.things[0].archLightingIndex == -1
                && level.motsArchLightingBakeBaselines.empty(),
            "levelCalcMotsArchLighting: moved originally-unbound Thing restores -1 and clears provenance");
    }

    {
        sed::Level level = makeMotsArchBakeLevel("deleted", "deleted.3do");
        level.motsArchLightingBakeBaselines.push_back(
            sed::MotsArchLightingBakeBaseline{ level.things[0].id, "deleted.3do", -1, std::nullopt });
        level.things.clear();
        const auto pruned = sed::level::levelCalcMotsArchLighting(
            level, std::span<const sed::level::MotsArchLightingBakeTarget>{});
        check(pruned.status == sed::level::MotsArchLightingBakeStatus::Applied
                && level.motsArchLightingBakeBaselines.empty(),
            "levelCalcMotsArchLighting: deleted stable Thing identities prune orphaned baseline data");
    }

    {
        sed::Level level = makeMotsArchBakeLevel("active_owner", "active_owner.3do");
        sed::Thing& modelLess = level.newThing();
        modelLess.name = "model_less_arch";
        modelLess.sector = 0;
        sed::TplValue flags{};
        flags.name = "thingflags";
        flags.type = sed::ValueType::integer;
        flags.intValue = 0x8;
        modelLess.values.push_back(flags);
        sed::TplValue model{};
        model.name = "model3d";
        model.type = sed::ValueType::str;
        model.stringValue = "none";
        modelLess.values.push_back(model);

        sed::MotsArchLightObject authored{};
        authored.wireIndex = 8;
        authored.meshes.push_back(sed::MotsArchLightMesh{
            0, { sed::MotsArchLightVertex{ 0.2f, 0.3f, 0.4f, 0.5f } } });
        sed::MotsArchLightObject generated = authored;
        generated.meshes[0].vertices[0] = sed::MotsArchLightVertex{
            0.7f, 0.8f, 0.9f, 1.0f };
        level.motsArchLighting = { generated };
        modelLess.archLightingIndex = 0;
        level.motsArchLightingBakeBaselines.push_back(
            sed::MotsArchLightingBakeBaseline{
                modelLess.id, "model_less_arch.3do", 0, authored });
        addMotsArchBakeLight(
            level, sed::LF_AffectArchThings | sed::LF_NoBlock,
            sed::TVector{}, 4.0, 0.5, sed::ColorWhite);
        const auto activeTarget = makeMotsArchBakeTarget(
            level, sed::level::MotsArchLightingTopology{ { { 1u } } },
            { { { 0u, { sed::TVector{} } } } }, 0u, "active_owner.3do");
        const auto result = sed::level::levelCalcMotsArchLighting(
            level, std::array<sed::level::MotsArchLightingBakeTarget, 1>{ activeTarget });
        const sed::MotsArchLightObject* restoredModelLess = archRecordForThing(level, 1u);
        check(result.status == sed::level::MotsArchLightingBakeStatus::Applied
                && restoredModelLess != nullptr && *restoredModelLess == authored
                && level.motsArchLightingBakeBaselines.size() == 1
                && level.motsArchLightingBakeBaselines[0].thingId == level.things[0].id
                && level.things[0].archLightingIndex != 0
                && level.things[1].archLightingIndex == 0,
            "levelCalcMotsArchLighting: model-less effective Arch owners restore authored data while other lights rebake");
    }

    {
        sed::Level level = makeMotsArchBakeLevel("missing_target", "missing_target.3do");
        sed::Thing& second = level.newThing();
        second.name = "unresolved_declared_model";
        second.sector = 0;
        sed::TplValue flags{};
        flags.name = "thingflags";
        flags.type = sed::ValueType::integer;
        flags.intValue = 0x8;
        second.values.push_back(flags);
        sed::TplValue model{};
        model.name = "model3d";
        model.type = sed::ValueType::str;
        model.stringValue = "missing.3do";
        second.values.push_back(model);
        addMotsArchBakeLight(
            level, sed::LF_AffectArchThings | sed::LF_NoBlock,
            sed::TVector{}, 4.0, 0.5, sed::ColorWhite);
        const auto onlyTarget = makeMotsArchBakeTarget(
            level, sed::level::MotsArchLightingTopology{ { { 1u } } },
            { { { 0u, { sed::TVector{} } } } }, 0u, "missing_target.3do");
        sed::MotsArchLightObject existing{};
        existing.wireIndex = 7;
        existing.meshes.push_back(sed::MotsArchLightMesh{
            0, { sed::MotsArchLightVertex{} } });
        level.motsArchLighting.push_back(existing);
        level.things[0].archLightingIndex = 0;
        const auto before = level;
        const auto result = sed::level::levelCalcMotsArchLighting(
            level, std::array<sed::level::MotsArchLightingBakeTarget, 1>{ onlyTarget });
        check(result.status == sed::level::MotsArchLightingBakeStatus::Rejected
                && result.message == "MotS ARCH bake target geometry is missing for Thing "
                    + std::to_string(second.id) + " 'unresolved_declared_model'; the model resource could not be resolved."
                && level.things[0].archLightingIndex == before.things[0].archLightingIndex
                && level.things[1].archLightingIndex == before.things[1].archLightingIndex
                && sameArchRecords(level.motsArchLighting, before.motsArchLighting)
                && level.motsArchLightingBakeBaselines
                    == before.motsArchLightingBakeBaselines,
            "levelCalcMotsArchLighting: declared model without resolved geometry rejects atomically with owner diagnostic");
    }

    {
        sed::Level level = makeMotsArchBakeLevel("multi_lod", "multi_lod.3do");
        level.sectors.push_back(makeLitTestCubeSector(4.0, sed::ColorBlack));
        const auto target = makeMotsArchBakeTarget(
            level,
            sed::level::MotsArchLightingTopology{ { { 2u }, { 3u } } },
            {
                { { 0u, { sed::TVector{}, sed::TVector{ 1.0, 0.0, 0.0 } } } },
                { { 0u, { sed::TVector{ 2.0, 0.0, 0.0 }, sed::TVector{ 3.0, 0.0, 0.0 }, sed::TVector{ 0.5, 0.0, 0.0 } } } },
            });
        sed::SedLight& red = level.newLight();
        red.flags = sed::LF_AffectArchThings | sed::LF_NoBlock;
        red.mark = 0;
        red.range = 4.0;
        red.intensity = 1.0;
        red.color = sed::ColorF{ 1.0f, 0.0f, 0.0f, 1.0f };
        sed::SedLight& green = level.newLight();
        green.flags = sed::LF_AffectArchThings | sed::LF_NoBlock;
        green.mark = 0;
        green.position = sed::TVector{ 4.0, 0.0, 0.0 };
        green.range = 4.0;
        green.intensity = 0.25;
        green.color = sed::ColorF{ 0.0f, 1.0f, 0.0f, 1.0f };

        const auto result = sed::level::levelCalcMotsArchLighting(
            level, std::array<sed::level::MotsArchLightingBakeTarget, 1>{ target });
        const sed::MotsArchLightObject* object = archRecordForThing(level, 0u);
        check(result.status == sed::level::MotsArchLightingBakeStatus::Applied
                && object != nullptr
                && object->meshes.size() == 1
                && object->meshes[0].vertices.size() == 3,
            "levelCalcMotsArchLighting: all LODs map into one source-mesh output prefix");
        check(object != nullptr
                && object->meshes.size() == 1
                && object->meshes[0].vertices.size() == 3
                && close(object->meshes[0].vertices[0].red, 1.0)
                && close(object->meshes[0].vertices[1].red, 0.5625)
                && close(object->meshes[0].vertices[1].green, 0.140625)
                && close(object->meshes[0].vertices[2].red, 0.765625)
                && close(object->meshes[0].vertices[2].green, 0.00390625),
            "levelCalcMotsArchLighting: within-LOD light sums combine by cross-LOD channel max with sparse indexes");
    }

    {
        sed::Level level = makeMotsArchBakeLevel("cloned_mesh", "cloned_mesh.3do");
        sed::MotsArchLightObject authored{};
        authored.wireIndex = 9;
        authored.meshes.push_back(sed::MotsArchLightMesh{
            0, { sed::MotsArchLightVertex{ 0.5f, 0.5f, 0.5f, 0.5f } } });
        level.motsArchLighting.push_back(authored);
        level.things[0].archLightingIndex = 0;
        addMotsArchBakeLight(
            level, sed::LF_AffectArchThings | sed::LF_NoBlock,
            sed::TVector{}, 4.0, 1.0, sed::ColorF{ 1.0f, 0.0f, 0.0f, 1.0f });
        addMotsArchBakeLight(
            level, sed::LF_AffectArchThings | sed::LF_NoBlock,
            sed::TVector{ 0.5, 0.0, 0.0 }, 4.0, 1.0,
            sed::ColorF{ 0.0f, 1.0f, 0.0f, 1.0f });
        const auto clonedTarget = makeMotsArchBakeTarget(
            level,
            sed::level::MotsArchLightingTopology{ { { 1u } } },
            { {
                { 0u, { sed::TVector{} } },
                { 0u, { sed::TVector{ 0.5, 0.0, 0.0 } } },
            } });
        const auto result = sed::level::levelCalcMotsArchLighting(
            level, std::array<sed::level::MotsArchLightingBakeTarget, 1>{ clonedTarget });
        const sed::MotsArchLightObject* object = archRecordForThing(level, 0u);
        check(result.status == sed::level::MotsArchLightingBakeStatus::Applied
                && result.changedThingIndices == std::vector<std::int32_t>{ 0 }
                && object != nullptr && object->meshes.size() == 1
                && object->meshes[0].vertices.size() == 1
                && close(object->meshes[0].vertices[0].red, 1.5)
                && close(object->meshes[0].vertices[0].green, 1.5)
                && close(object->meshes[0].vertices[0].blue, 0.5)
                && level.motsArchLightingBakeBaselines.size() == 1
                && level.motsArchLightingBakeBaselines[0].originalArchLighting
                    == std::optional<sed::MotsArchLightObject>{ authored },
            "levelCalcMotsArchLighting: cloned same-LOD source meshes merge per-channel maxima and add authored values once");
    }

    {
        sed::Level level = makeMotsArchBakeLevel("cancelled", "cancelled.3do");
        sed::MotsArchLightObject existing{};
        existing.wireIndex = 4;
        existing.meshes.push_back(sed::MotsArchLightMesh{
            0, std::vector<sed::MotsArchLightVertex>(
                128u, sed::MotsArchLightVertex{ 0.5f, 0.5f, 0.5f, 0.5f }) });
        level.motsArchLighting.push_back(existing);
        level.things[0].archLightingIndex = 0;
        addMotsArchBakeLight(
            level, sed::LF_AffectArchThings | sed::LF_NoBlock,
            sed::TVector{}, 5.0, 1.0, sed::ColorWhite);
        const std::vector<sed::TVector> positions(128u, sed::TVector{});
        const auto target = makeMotsArchBakeTarget(
            level,
            sed::level::MotsArchLightingTopology{ { { positions.size() } } },
            { { { 0u, positions } } });
        const auto before = level;
        int progressCalls = 0;
        const auto result = sed::level::levelCalcMotsArchLighting(
            level,
            std::array<sed::level::MotsArchLightingBakeTarget, 1>{ target },
            nullptr,
            [&progressCalls](const std::size_t, const std::size_t)
            {
                ++progressCalls;
                return progressCalls == 1;
            });
        check(result.status == sed::level::MotsArchLightingBakeStatus::Cancelled
                && result.message == "MotS ARCH baked-light calculation was cancelled."
                && progressCalls >= 2
                && level.things[0].archLightingIndex == before.things[0].archLightingIndex
                && sameArchRecords(level.motsArchLighting, before.motsArchLighting),
            "levelCalcMotsArchLighting: bounded cancellation publishes no partial ARCH records");
    }
}

} // namespace sed::tests
