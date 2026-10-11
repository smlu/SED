#include "test_common.hpp"

#include "sed/assets/jed.hpp"
#include "sed/assets/resource_locator.hpp"
#include "sed/assets/sed.hpp"
#include "sed/level/edit.hpp"
#include "sed/level.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace sed::tests
{

void runJedLevelTests()
{
    {
        sed::Level source{};
        source.clear();
        source.kind = sed::ProjectType::JKDF2;

        sed::Thing& thing = source.newThing();
        thing.name = "tuple_thing";
        thing.sector = -1;
        thing.values.push_back(sed::deserializeTplValue("typeflags", "0x5"));

        sed::TplValue offset{};
        offset.name = "lightoffset";
        offset.type = sed::ValueType::str;
        offset.stringValue = "0/.07/.04";
        thing.values.push_back(std::move(offset));

        std::ostringstream savedStream;
        sed::assets::saveToJED(source, savedStream);
        const std::string saved = savedStream.str();
        check(saved.find("typeflags=0x5") != std::string::npos
                && saved.find("lightoffset=(0/.07/.04)") != std::string::npos,
            "JED Thing params: typed flags and tuple values serialize canonically");

        std::istringstream reparseStream(saved);
        const sed::Level reparsed = sed::assets::loadFromJED(reparseStream);
        check(reparsed.things.size() == 1 && reparsed.things[0].values.size() == 2,
            "JED Thing params: serialized values reload");
        if (reparsed.things.size() == 1 && reparsed.things[0].values.size() == 2)
        {
            check(reparsed.things[0].values[0].type == sed::ValueType::flag
                    && reparsed.things[0].values[0].intValue == 0x5
                    && reparsed.things[0].values[1].asString() == "(0/.07/.04)",
                "JED Thing params: standard deserializer restores typed values");
        }
    }

    {
        sed::Level src{};
        src.clear();
        src.kind                           = sed::ProjectType::JKDF2;
        src.uvCoordinateSpace              = sed::UvCoordinateSpace::TexturePixels;
        src.header.version                 = 1;
        src.header.gravity                 = 4.0;
        src.header.ceilingSky.height       = 15.0;
        src.header.ceilingSky.offset       = sed::TPoint2{ 1.5, 2.5 };
        src.header.horizonSky.distance     = 500.0;
        src.header.horizonSky.pixelsPerRev = 999.0;
        src.header.horizonSky.offset       = sed::TPoint2{ 7.0, 8.0 };
        src.header.mipmapDistances         = { 11.0, 22.0, 33.0, 44.0 };
        src.header.lodDistances            = { 1.1, 2.2, 3.3, 4.4 };
        src.header.perspectiveDistance     = 5.5;
        src.header.gouraudDistance         = 6.6;
        src.masterCMP                      = "dflt.cmp";
        src.ppunit                         = 16.0;
        src.layers.push_back("Main");
        src.lvisstring                     = "lvis test";

        {
            auto& sector = src.newSector();
            sector.colormap = "dflt.cmp";
            sector.sound = "amb.wav";
            sector.soundVolume = 0.5;
            sector.flags = 0x123;
            sector.ambient = sed::intensityToColor(0.25f);
            sector.extraLight = sed::intensityToColor(0.1f);
            sector.tint = sed::ColorF{ 0.2f, 0.3f, 0.4f, 1.0f };
            sector.layer = 0;

            sector.addVertex(0.0, 0.0, 0.0);
            sector.addVertex(1.0, 0.0, 0.0);
            sector.addVertex(0.0, 1.0, 0.0);

            sed::Surface surf{};
            surf.material = "test.mat";
            surf.surfaceFlags = 0x1;
            surf.faceFlags = 0x2;
            surf.geo = 0;
            surf.light = 0;
            surf.tex = 0;
            surf.extraLight = sed::signedIntensityToColor(-0.25f);
            surf.adjoinFlags = 0;
            surf.uScale = 1.0f;
            surf.vScale = 1.0f;
            surf.vertices = { 0, 1, 2 };
            surf.texVertices = {
                sed::TexVertex{ 0.0f, 0.0f, sed::ColorF{ 0.5f, 0.5f, 0.5f, 0.5f } },
                sed::TexVertex{ 1.0f, 0.0f, sed::ColorF{ 0.5f, 0.5f, 0.5f, 0.5f } },
                sed::TexVertex{ 0.0f, 1.0f, sed::ColorF{ 0.5f, 0.5f, 0.5f, 0.5f } },
            };
            sector.addSurface(std::move(surf));
        }

        {
            auto& thing = src.newThing();
            thing.name = "walkplayer";
            thing.x = 1.0;
            thing.y = 2.0;
            thing.z = 3.0;
            thing.sector = 0;
            thing.layer = 0;

            sed::TplValue v{};
            v.name = "health";
            v.type = sed::ValueType::str;
            v.stringValue = "100";
            thing.values.push_back(std::move(v));
        }

        {
            auto& light = src.newLight();
            light.position = sed::TVector{ 1.0, 2.0, 3.0 };
            light.layer = 0;
            light.intensity = 10.0;
            light.range = 4.0;
            light.flags = sed::LF_NoBlock;
            light.color = sed::ColorF{ 0.25f, 0.5f, 0.75f, 1.0f };
        }

        {
            auto& cog = src.newCog();
            cog.name = "test.cog";

            sed::CogValue v{};
            v.name = "score";
            v.cogType = sed::CogType::int_;
            v.type = sed::ValueType::integer;
            (void)v.parseJed("42");
            cog.values.push_back(std::move(v));
        }

        {
            sed::HNode& node0 = src.new3doNode();
            node0.type          = 0x123;
            node0.mesh          = 7;
            node0.parent        = -1;
            node0.position      = sed::TVector{ 1.0, 2.0, 3.0 };
            node0.rotation      = sed::TVector{ 10.0, 20.0, 30.0 };
            node0.pivot         = sed::TVector{ 0.1, 0.2, 0.3 };
            node0.name          = "root";

            sed::HNode& node1 = src.new3doNode();
            node1.type          = 0x456;
            node1.mesh          = 8;
            node1.parent        = 0;
            node1.position      = sed::TVector{ 4.0, 5.0, 6.0 };
            node1.rotation      = sed::TVector{ -10.0, -20.0, -30.0 };
            node1.pivot         = sed::TVector{ 1.1, 1.2, 1.3 };
            node1.name          = "child";
        }

        std::ostringstream oss;
        sed::assets::saveToJED(src, oss);

        std::istringstream iss(oss.str());
        const sed::Level dst = sed::assets::loadFromJED(iss);

        check(dst.kind == sed::ProjectType::JKDF2, "JED: kind");

        check(dst.header.version == 1, "JED: version");
        check(close(dst.header.gravity, 4.0), "JED: gravity");
        check(close(dst.header.ceilingSky.height, 15.0), "JED: ceilingSky.height");
        check(close(dst.header.ceilingSky.offset.x, 1.5), "JED: ceilingSky.offset.x");
        check(close(dst.header.ceilingSky.offset.y, 2.5), "JED: ceilingSky.offset.y");
        check(close(dst.header.horizonSky.distance, 500.0), "JED: horizonSky.distance");
        check(close(dst.header.horizonSky.pixelsPerRev, 999.0), "JED: horizonSky.pixelsPerRev");
        check(close(dst.header.horizonSky.offset.x, 7.0), "JED: horizonSky.offset.x");
        check(close(dst.header.horizonSky.offset.y, 8.0), "JED: horizonSky.offset.y");
        check(close(dst.header.mipmapDistances[0], 11.0), "JED: mipmapDistances[0]");
        check(close(dst.header.mipmapDistances[1], 22.0), "JED: mipmapDistances[1]");
        check(close(dst.header.mipmapDistances[2], 33.0), "JED: mipmapDistances[2]");
        check(close(dst.header.mipmapDistances[3], 44.0), "JED: mipmapDistances[3]");
        check(close(dst.header.lodDistances[0], 1.1), "JED: lodDistances[0]");
        check(close(dst.header.lodDistances[1], 2.2), "JED: lodDistances[1]");
        check(close(dst.header.lodDistances[2], 3.3), "JED: lodDistances[2]");
        check(close(dst.header.lodDistances[3], 4.4), "JED: lodDistances[3]");
        check(close(dst.header.perspectiveDistance, 5.5), "JED: perspectiveDistance");
        check(close(dst.header.gouraudDistance, 6.6), "JED: gouraudDistance");

        check(dst.masterCMP == "dflt.cmp", "JED: masterCMP");
        check(close(dst.ppunit, 16.0), "JED: ppunit");

        check(dst.sectors.size() == 1, "JED: sectors");
        check(dst.sectors.size() == 1 && dst.sectors[0].id > 0, "JED: sector.id");
        check(dst.sectors.size() == 1 && dst.sectors[0].num == 0, "JED: sector.num");
        check(dst.sectors.size() == 1 && dst.sectors[0].vertices.size() == 3, "JED: vertices");
        check(dst.sectors.size() == 1 && dst.sectors[0].surfaces.size() == 1, "JED: surfaces");
        check(dst.sectors.size() == 1 && dst.sectors[0].surfaces.size() == 1 && dst.sectors[0].surfaces[0].num == 0,
              "JED: surface.num");
        check(dst.sectors.size() == 1 && dst.sectors[0].surfaces.size() == 1 && dst.sectors[0].surfaces[0].material == "test.mat",
              "JED: material");
        check(dst.things.size() == 1, "JED: things");
        check(dst.lights.size() == 1, "JED: lights");
        check(dst.cogs.size() == 1, "JED: cogs");
        check(dst.layers.size() == 1 && dst.layers[0] == "Main", "JED: layers");
        check(dst.lvisstring == "lvis test", "JED: lvis");
        check(dst.h3doNodes.size() == 2, "JED: HIERARCHYDEF nodes");

        check(dst.cogs.size() == 1 && dst.cogs[0].name == "test.cog", "JED: cog name");
        check(dst.cogs.size() == 1 && dst.cogs[0].values.size() == 1, "JED: cog values");
        check(dst.cogs.size() == 1 && dst.cogs[0].values.size() == 1 && dst.cogs[0].values[0].name == "score", "JED: cog value name");
        check(dst.cogs.size() == 1 && dst.cogs[0].values.size() == 1 && dst.cogs[0].values[0].cogType == sed::CogType::int_,
              "JED: cog value type");
        check(dst.cogs.size() == 1 && dst.cogs[0].values.size() == 1 && dst.cogs[0].values[0].intValue == 42, "JED: cog value int");

        if (dst.h3doNodes.size() == 2)
        {
            const sed::HNode& node0 = dst.h3doNodes[0];
            check(node0.id > 0, "JED: h3doNodes[0].id");
            check(node0.num == 0, "JED: h3doNodes[0].num");
            check(node0.type == 0x123, "JED: h3doNodes[0].type");
            check(node0.mesh == 7, "JED: h3doNodes[0].mesh");
            check(node0.parent == -1, "JED: h3doNodes[0].parent");
            check(close(node0.position.x, 1.0), "JED: h3doNodes[0].position.x");
            check(close(node0.position.y, 2.0), "JED: h3doNodes[0].position.y");
            check(close(node0.position.z, 3.0), "JED: h3doNodes[0].position.z");
            check(close(node0.rotation.pitch, 10.0), "JED: h3doNodes[0].rotation.pitch");
            check(close(node0.rotation.yaw, 20.0), "JED: h3doNodes[0].rotation.yaw");
            check(close(node0.rotation.roll, 30.0), "JED: h3doNodes[0].rotation.roll");
            check(close(node0.pivot.x, 0.1), "JED: h3doNodes[0].pivot.x");
            check(close(node0.pivot.y, 0.2), "JED: h3doNodes[0].pivot.y");
            check(close(node0.pivot.z, 0.3), "JED: h3doNodes[0].pivot.z");
            check(node0.name == "root", "JED: h3doNodes[0].name");

            const sed::HNode& node1 = dst.h3doNodes[1];
            check(node1.id > 0, "JED: h3doNodes[1].id");
            check(node1.num == 1, "JED: h3doNodes[1].num");
            check(node1.type == 0x456, "JED: h3doNodes[1].type");
            check(node1.mesh == 8, "JED: h3doNodes[1].mesh");
            check(node1.parent == 0, "JED: h3doNodes[1].parent");
            check(close(node1.position.x, 4.0), "JED: h3doNodes[1].position.x");
            check(close(node1.position.y, 5.0), "JED: h3doNodes[1].position.y");
            check(close(node1.position.z, 6.0), "JED: h3doNodes[1].position.z");
            check(close(node1.rotation.pitch, -10.0), "JED: h3doNodes[1].rotation.pitch");
            check(close(node1.rotation.yaw, -20.0), "JED: h3doNodes[1].rotation.yaw");
            check(close(node1.rotation.roll, -30.0), "JED: h3doNodes[1].rotation.roll");
            check(close(node1.pivot.x, 1.1), "JED: h3doNodes[1].pivot.x");
            check(close(node1.pivot.y, 1.2), "JED: h3doNodes[1].pivot.y");
            check(close(node1.pivot.z, 1.3), "JED: h3doNodes[1].pivot.z");
            check(node1.name == "child", "JED: h3doNodes[1].name");
        }

        if (dst.sectors.size() == 1)
        {
            const sed::Sector& sector = dst.sectors[0];

            check(sector.colormap == "dflt.cmp", "JED: sector.colormap");
            check(sector.sound == "amb.wav", "JED: sector.sound");
            check(close(sector.soundVolume, 0.5), "JED: sector.soundVolume");
            check(sector.flags == 0x123, "JED: sector.flags");

            check(close(sector.ambient.r, 0.25), "JED: sector.ambient.r");
            check(close(sector.ambient.g, 0.25), "JED: sector.ambient.g");
            check(close(sector.ambient.b, 0.25), "JED: sector.ambient.b");
            check(close(sector.ambient.a, 1.0), "JED: sector.ambient.a");

            check(close(sector.extraLight.r, 0.1), "JED: sector.extraLight.r");
            check(close(sector.extraLight.g, 0.1), "JED: sector.extraLight.g");
            check(close(sector.extraLight.b, 0.1), "JED: sector.extraLight.b");
            check(close(sector.extraLight.a, 1.0), "JED: sector.extraLight.a");

            check(close(sector.tint.r, 0.2), "JED: sector.tint.r");
            check(close(sector.tint.g, 0.3), "JED: sector.tint.g");
            check(close(sector.tint.b, 0.4), "JED: sector.tint.b");
            check(close(sector.tint.a, 1.0), "JED: sector.tint.a");

            check(sector.layer == 0, "JED: sector.layer");

            check(sector.vertices.size() == 3, "JED: sector.vertices.size");
            if (sector.vertices.size() >= 3)
            {
                check(close(sector.vertices[0].x, 0.0), "JED: sector.v0.x");
                check(close(sector.vertices[0].y, 0.0), "JED: sector.v0.y");
                check(close(sector.vertices[0].z, 0.0), "JED: sector.v0.z");

                check(close(sector.vertices[1].x, 1.0), "JED: sector.v1.x");
                check(close(sector.vertices[1].y, 0.0), "JED: sector.v1.y");
                check(close(sector.vertices[1].z, 0.0), "JED: sector.v1.z");

                check(close(sector.vertices[2].x, 0.0), "JED: sector.v2.x");
                check(close(sector.vertices[2].y, 1.0), "JED: sector.v2.y");
                check(close(sector.vertices[2].z, 0.0), "JED: sector.v2.z");
            }

            check(sector.surfaces.size() == 1, "JED: sector.surfaces.size");
            if (!sector.surfaces.empty())
            {
                const sed::Surface& surf = sector.surfaces[0];
                check(surf.num == 0, "JED: surface.num");
                check(surf.material == "test.mat", "JED: surface.material");
                check(surf.surfaceFlags == 0x1, "JED: surface.surfaceFlags");
                check(surf.faceFlags == 0x2u, "JED: surface.faceFlags");
                check(surf.geo == 0, "JED: surface.geo");
                check(surf.light == 0, "JED: surface.light");
                check(surf.tex == 0, "JED: surface.tex");
                check(!surf.adjoin.has_value(), "JED: surface.adjoin");
                check(surf.adjoinFlags == 0, "JED: surface.adjoinFlags");
                check(surf.uScale == 1.0f, "JED: surface.uScale");
                check(surf.vScale == 1.0f, "JED: surface.vScale");

                check(close(surf.extraLight.r, -0.25), "JED: surface.extraLight.r");
                check(close(surf.extraLight.g, -0.25), "JED: surface.extraLight.g");
                check(close(surf.extraLight.b, -0.25), "JED: surface.extraLight.b");
                check(close(surf.extraLight.a, 1.0), "JED: surface.extraLight.a");

                check(surf.vertices.size() == 3, "JED: surface.vertices.size");
                check(surf.vertices.size() >= 3 && surf.vertices[0] == 0 && surf.vertices[1] == 1 && surf.vertices[2] == 2,
                      "JED: surface.vertices");

                check(surf.texVertices.size() == 3, "JED: surface.texVertices.size");
                if (surf.texVertices.size() >= 3)
                {
                    check(close(surf.texVertices[0].u, 0.0), "JED: surface.tv0.u");
                    check(close(surf.texVertices[0].v, 0.0), "JED: surface.tv0.v");
                    check(close(surf.texVertices[0].color.a, 0.5), "JED: surface.tv0.color.a");
                    check(close(surf.texVertices[0].color.r, 0.5), "JED: surface.tv0.color.r");
                    check(close(surf.texVertices[0].color.g, 0.5), "JED: surface.tv0.color.g");
                    check(close(surf.texVertices[0].color.b, 0.5), "JED: surface.tv0.color.b");

                    check(close(surf.texVertices[1].u, 1.0), "JED: surface.tv1.u");
                    check(close(surf.texVertices[1].v, 0.0), "JED: surface.tv1.v");
                    check(close(surf.texVertices[1].color.a, 0.5), "JED: surface.tv1.color.a");

                    check(close(surf.texVertices[2].u, 0.0), "JED: surface.tv2.u");
                    check(close(surf.texVertices[2].v, 1.0), "JED: surface.tv2.v");
                    check(close(surf.texVertices[2].color.a, 0.5), "JED: surface.tv2.color.a");
                }

                check(surf.normal == sed::TVector::zero, "JED: surface.normal");
            }
        }

        if (dst.things.size() == 1)
        {
            const sed::Thing& thing = dst.things[0];
            check(thing.id > 0, "JED: thing.id");
            check(thing.num == 0, "JED: thing.num");
            check(thing.sector == 0, "JED: thing.sector");
            check(thing.layer == 0, "JED: thing.layer");
            check(thing.name == "walkplayer", "JED: thing.name");
            check(close(thing.x, 1.0), "JED: thing.x");
            check(close(thing.y, 2.0), "JED: thing.y");
            check(close(thing.z, 3.0), "JED: thing.z");
            check(close(thing.pch, 0.0), "JED: thing.pch");
            check(close(thing.yaw, 0.0), "JED: thing.yaw");
            check(close(thing.rol, 0.0), "JED: thing.rol");

            check(thing.values.size() == 1, "JED: thing.values.size");
            if (!thing.values.empty())
            {
                check(thing.values[0].name == "health", "JED: thing.values[0].name");
                check(thing.values[0].stringValue == "100", "JED: thing.values[0].value");
            }
        }

        if (dst.lights.size() == 1)
        {
            const sed::SedLight& light = dst.lights[0];
            check(close(light.position.x, 1.0), "JED: light.position.x");
            check(close(light.position.y, 2.0), "JED: light.position.y");
            check(close(light.position.z, 3.0), "JED: light.position.z");
            check(light.layer == 0, "JED: light.layer");
            check(close(light.intensity, 10.0), "JED: light.intensity");
            check(close(light.range, 4.0), "JED: light.range");
            check(light.flags == 1, "JED: light.flags");

            // JKDF2: loader forces white, ignoring RGB from file.
            check(light.color == sed::ColorWhite, "JED: light.color.JKDF2");
        }
    }

    {
        const char* jed =
            R"JED(
JED 0.1
LEVELHEADER
VERSION 1
SECTORS 1
SECTOR 0 0x0 0 0 0 1 1 1 0 0 0 0 0 0
VERTICES 4
0 0 0
1 0 0
1 1 0
0 1 0
SURFACES 2
SURFACE 0 0 4 0 0 0 0 1 1 0 0 0 test0.mat
0 1 2 3
0 0  1 0  1 1  0 1
SURFACE 0 0 4 0 0 0 0 1 1 0 0 0 test1.mat
0 1 2 3
0 0  1 0  1 1  0 1
THINGS 0
LIGHTS 0
COGS 0
JED)JED";

        std::istringstream iss(jed);
        const sed::Level level = sed::assets::loadFromJED(iss);

        check(level.sectors.size() == 1, "JED.multiSurface: sectors");
        check(level.sectors.size() == 1 && level.sectors[0].surfaces.size() == 2, "JED.multiSurface: surfaces");
        if (level.sectors.size() == 1 && level.sectors[0].surfaces.size() == 2)
        {
            check(level.sectors[0].surfaces[0].num == 0, "JED.multiSurface: surface0.num");
            check(level.sectors[0].surfaces[1].num == 1, "JED.multiSurface: surface1.num");
        }
    }

    {
        const char* const jed = R"JED(
JED 1.0
SEC: COGS
COGS 2
cog0 refCog:cog=0 secRef:sec=1 surfRef:srf=2 thingRef:thg=3 soundRef:sound=door_open.wav
cog1 refCog:cog=0
END
)JED";

        std::istringstream iss(jed);
        sed::Level level = sed::assets::loadFromJED(iss);

        check(level.cogs.size() == 2, "JED.cogTypes: cogs");
        if (level.cogs.size() == 2)
        {
            const sed::Cog& cog0 = level.cogs[0];
            check(cog0.name == "cog0", "JED.cogTypes: cog0.name");
            check(cog0.values.size() == 5, "JED.cogTypes: cog0.values");

            if (cog0.values.size() == 5)
            {
                const sed::CogValue& refCog = cog0.values[0];
                check(refCog.name == "refCog", "JED.cogTypes: refCog.name");
                check(refCog.cogType == sed::CogType::cog, "JED.cogTypes: refCog.cogType");
                check(refCog.type == sed::ValueType::integer, "JED.cogTypes: refCog.type");
                check(refCog.intValue == 0, "JED.cogTypes: refCog.intValue");

                const sed::CogValue& secRef = cog0.values[1];
                check(secRef.name == "secRef", "JED.cogTypes: secRef.name");
                check(secRef.cogType == sed::CogType::sec, "JED.cogTypes: secRef.cogType");
                check(secRef.type == sed::ValueType::ptr, "JED.cogTypes: secRef.type");
                check(secRef.intValue == 1, "JED.cogTypes: secRef.intValue");

                const sed::CogValue& surfRef = cog0.values[2];
                check(surfRef.name == "surfRef", "JED.cogTypes: surfRef.name");
                check(surfRef.cogType == sed::CogType::surface, "JED.cogTypes: surfRef.cogType");
                check(surfRef.type == sed::ValueType::ptr, "JED.cogTypes: surfRef.type");
                check(surfRef.intValue == 2, "JED.cogTypes: surfRef.intValue");

                const sed::CogValue& thingRef = cog0.values[3];
                check(thingRef.name == "thingRef", "JED.cogTypes: thingRef.name");
                check(thingRef.cogType == sed::CogType::thing, "JED.cogTypes: thingRef.cogType");
                check(thingRef.type == sed::ValueType::ptr, "JED.cogTypes: thingRef.type");
                check(thingRef.intValue == 3, "JED.cogTypes: thingRef.intValue");

                const sed::CogValue& soundRef = cog0.values[4];
                check(soundRef.name == "soundRef", "JED.cogTypes: soundRef.name");
                check(soundRef.cogType == sed::CogType::wav, "JED.cogTypes: soundRef.cogType");
                check(soundRef.type == sed::ValueType::str, "JED.cogTypes: soundRef.type");
                check(soundRef.stringValue == "door_open.wav", "JED.cogTypes: soundRef.stringValue");
            }
        }

        const bool deleted = sed::level::levelDeleteCog(level, 0);
        check(deleted, "JED.cogTypes: deleteCog");
        check(level.cogs.size() == 1, "JED.cogTypes: deleteCog.cogs");

        if (!level.cogs.empty())
        {
            const sed::Cog& cog = level.cogs[0];
            check(cog.num == 0, "JED.cogTypes: deleteCog.cog.num");
            check(cog.values.size() == 1, "JED.cogTypes: deleteCog.cog.values");

            if (!cog.values.empty())
            {
                const sed::CogValue& refCog = cog.values[0];
                check(refCog.cogType == sed::CogType::cog, "JED.cogTypes: deleteCog.refCog.cogType");
                check(refCog.type == sed::ValueType::integer, "JED.cogTypes: deleteCog.refCog.type");
                check(refCog.intValue == -1, "JED.cogTypes: deleteCog.refCog.intValue");
            }
        }
    }

    {
        sed::Level level{};
        level.clear();

        {
            sed::Sector& sector0 = level.newSector();
            sector0.addVertex(0.0, 0.0, 0.0);

            sed::Surface surf0{};
            surf0.num = 0;
            sector0.addSurface(std::move(surf0));

            sed::Surface surf1{};
            surf1.num = 1;
            sector0.addSurface(std::move(surf1));
        }

        {
            sed::Sector& sector1 = level.newSector();
            sector1.addVertex(0.0, 0.0, 0.0);

            for (std::int32_t i = 0; i < 3; ++i)
            {
                sed::Surface surf{};
                surf.num = i;
                sector1.addSurface(std::move(surf));
            }
        }

        (void)level.newThing();
        (void)level.newThing();

        check(level.getGlobalSurfaceNumber(0, 0) == 0, "CogValue.JED: global surface (0,0)");
        check(level.getGlobalSurfaceNumber(0, 1) == 1, "CogValue.JED: global surface (0,1)");
        check(level.getGlobalSurfaceNumber(1, 0) == 2, "CogValue.JED: global surface (1,0)");
        check(level.getGlobalSurfaceNumber(1, 2) == 4, "CogValue.JED: global surface (1,2)");
        check(level.getGlobalSurfaceNumber(99, 0) == -1, "CogValue.JED: global surface invalid sector");
        check(level.getGlobalSurfaceNumber(0, 99) == -1, "CogValue.JED: global surface invalid surface");

        {
            const std::optional<sed::SurfaceRef> ref = level.tryGetSurfaceRefByGlobalSurfaceNumber(0);
            check(ref.has_value(), "CogValue.JED: ref from global 0 exists");
            if (ref)
            {
                check(ref->sector == 0, "CogValue.JED: ref from global 0 sector");
                check(ref->surface == 0, "CogValue.JED: ref from global 0 surface");
            }
        }

        {
            const std::optional<sed::SurfaceRef> ref = level.tryGetSurfaceRefByGlobalSurfaceNumber(4);
            check(ref.has_value(), "CogValue.JED: ref from global 4 exists");
            if (ref)
            {
                check(ref->sector == 1, "CogValue.JED: ref from global 4 sector");
                check(ref->surface == 2, "CogValue.JED: ref from global 4 surface");
            }
        }

        check(!level.tryGetSurfaceRefByGlobalSurfaceNumber(-1).has_value(), "CogValue.JED: ref from global -1 invalid");
        check(!level.tryGetSurfaceRefByGlobalSurfaceNumber(999).has_value(), "CogValue.JED: ref from global 999 invalid");

        {
            sed::CogValue v{};
            v.cogType = sed::CogType::surface;
            v.type = sed::ValueType::ptr;

            check(v.parseJed(level, "-1"), "CogValue.JED: parse surface -1");
            check(v.intValue == -1, "CogValue.JED: surface -1 intValue");
            check(v.asJedString(level) == "-1", "CogValue.JED: surface -1 asJedString");

            check(v.parseJed(level, "1 2"), "CogValue.JED: parse surface 1 2");
            check(v.intValue == 4, "CogValue.JED: surface 1 2 intValue");
            check(v.asJedString(level) == "1 2", "CogValue.JED: surface 1 2 asJedString");

            check(!v.parseJed(level, "0"), "CogValue.JED: parse surface missing index");
            check(!v.parseJed(level, "1 99"), "CogValue.JED: parse surface invalid surface");
            check(!v.parseJed(level, "99 0"), "CogValue.JED: parse surface invalid sector");
        }

        {
            sed::CogValue v{};
            v.cogType = sed::CogType::sec;
            v.type = sed::ValueType::ptr;

            check(v.parseJed(level, "-1"), "CogValue.JED: parse sec -1");
            check(v.intValue == -1, "CogValue.JED: sec -1 intValue");
            check(v.parseJed(level, "1"), "CogValue.JED: parse sec 1");
            check(v.intValue == 1, "CogValue.JED: sec 1 intValue");
            check(!v.parseJed(level, "2"), "CogValue.JED: parse sec 2 invalid");
        }

        {
            sed::CogValue v{};
            v.cogType = sed::CogType::thing;
            v.type = sed::ValueType::ptr;

            check(v.parseJed(level, "-1"), "CogValue.JED: parse thing -1");
            check(v.intValue == -1, "CogValue.JED: thing -1 intValue");
            check(v.parseJed(level, "0"), "CogValue.JED: parse thing 0");
            check(v.intValue == 0, "CogValue.JED: thing 0 intValue");
            check(!v.parseJed(level, "2"), "CogValue.JED: parse thing 2 invalid");
        }
    }

    {
        const char* jed =
            R"JED(
                jed 1.0

                sec: header
                hdr 1 4 15 2 2 - 16
                end

                sec: geo
                secs 1
                - - 0 0x0 0.5 0 0 0 0 0
                vxs 3
                0 0 0
                1 0 0
                0 1 0
                surfs 1
                test.mat 0x0 0x0 0 0 0 0 3 -1 -1 0x0 1 1
                0 0 0 1 1 1 0 1 2 0 1 1
                end
            )JED";

        std::istringstream iss(jed);
        const sed::Level level = sed::assets::loadFromJED(iss);

        check(level.kind == sed::ProjectType::JKDF2, "JED.lowercase: kind");
        check(level.header.version == 1, "JED.lowercase: version");
        check(close(level.header.gravity, 4.0), "JED.lowercase: gravity");
        check(close(level.header.ceilingSky.height, 15.0), "JED.lowercase: ceilingSky.height");
        check(close(level.header.perspectiveDistance, 2.0), "JED.lowercase: perspectiveDistance");
        check(close(level.header.gouraudDistance, 2.0), "JED.lowercase: gouraudDistance");
        check(level.masterCMP.empty(), "JED.lowercase: masterCMP");
        check(close(level.ppunit, 16.0), "JED.lowercase: ppunit");

        check(level.sectors.size() == 1, "JED.lowercase: sectors");
        check(level.sectors[0].vertices.size() == 3, "JED.lowercase: vertices");
        check(level.sectors[0].surfaces.size() == 1, "JED.lowercase: surfaces");
        check(level.sectors[0].surfaces[0].material == "test.mat", "JED.lowercase: material");

        if (!level.sectors.empty())
        {
            const sed::Sector& sector = level.sectors[0];
            check(sector.colormap.empty(), "JED.lowercase: sector.colormap");
            check(sector.sound.empty(), "JED.lowercase: sector.sound");
            check(close(sector.soundVolume, 0.0), "JED.lowercase: sector.soundVolume");
            check(sector.flags == 0, "JED.lowercase: sector.flags");
            check(close(sector.ambient.intensity(), 0.5), "JED.lowercase: sector.ambient.intensity");
            check(close(sector.extraLight.intensity(), 0.0), "JED.lowercase: sector.extraLight.intensity");
            check(sector.layer == 0, "JED.lowercase: sector.layer");

            if (!sector.surfaces.empty())
            {
                const sed::Surface& surf = sector.surfaces[0];
                check(surf.surfaceFlags == 0, "JED.lowercase: surface.surfaceFlags");
                check(surf.faceFlags == 0u, "JED.lowercase: surface.faceFlags");
                check(surf.vertices.size() == 3, "JED.lowercase: surface.vertices.size");
                check(surf.texVertices.size() == 3, "JED.lowercase: surface.texVertices.size");
                check(!surf.adjoin.has_value(), "JED.lowercase: surface.adjoin");
            }
        }
    }

    {
        sed::Level source{};
        source.clear();
        source.exportBufferExtras = sed::LevelExportBufferExtras{
            .sounds = 1,
            .materials = 2,
            .aiClasses = 3,
            .models = 4,
            .sprites = 5,
            .keyframes = 6,
            .puppets = 7,
            .soundClasses = 8,
            .cogScripts = 9,
            .cogs = 10,
            .templates = 11,
            .things = 12,
        };

        std::ostringstream out;
        sed::assets::saveToJED(source, out);
        const std::string saved = out.str();
        check(saved.find("SEC: EXPORTBUFFERS") != std::string::npos
                && saved.find("COGS 10") != std::string::npos
                && saved.find("THINGS 12") != std::string::npos,
            "JED export buffers: advanced per-section allowances serialize");

        std::istringstream in(saved);
        const sed::Level loaded = sed::assets::loadFromJED(in);
        check(loaded.exportBufferExtras == source.exportBufferExtras,
            "JED export buffers: advanced per-section allowances round-trip");

        sed::Level defaults{};
        defaults.clear();
        std::ostringstream defaultsOut;
        sed::assets::saveToJED(defaults, defaultsOut);
        check(defaultsOut.str().find("SEC: EXPORTBUFFERS") == std::string::npos,
            "JED export buffers: default allowances keep legacy files uncluttered");
    }

    {
        const char* jed =
            R"JED(
                JeD 1.0

                SeC: HeAdEr
                HdR 1 4 15 2 2 - 16
                eNd

                sEc: GeO
                sEcS 1
                - - 0 0x0 0.5 0 0 0 0 0
                VxS 3
                0 0 0
                1 0 0
                0 1 0
                SuRfS 1
                test.mat 0x0 0x0 0 0 0 0 3 -1 -1 0x0 1 1
                0 0 0 1 1 1 0 1 2 0 1 1
                EnD
            )JED";

        std::istringstream iss(jed);
        const sed::Level level = sed::assets::loadFromJED(iss);

        check(level.kind == sed::ProjectType::JKDF2, "JED.mixedcase: kind");
        check(level.header.version == 1, "JED.mixedcase: version");
        check(close(level.header.gravity, 4.0), "JED.mixedcase: gravity");
        check(close(level.header.ceilingSky.height, 15.0), "JED.mixedcase: ceilingSky.height");
        check(close(level.header.perspectiveDistance, 2.0), "JED.mixedcase: perspectiveDistance");
        check(close(level.header.gouraudDistance, 2.0), "JED.mixedcase: gouraudDistance");
        check(level.masterCMP.empty(), "JED.mixedcase: masterCMP");
        check(close(level.ppunit, 16.0), "JED.mixedcase: ppunit");

        check(level.sectors.size() == 1, "JED.mixedcase: sectors");
        check(level.sectors[0].vertices.size() == 3, "JED.mixedcase: vertices");
        check(level.sectors[0].surfaces.size() == 1, "JED.mixedcase: surfaces");
        check(level.sectors[0].surfaces[0].material == "test.mat", "JED.mixedcase: material");

        if (!level.sectors.empty())
        {
            const sed::Sector& sector = level.sectors[0];
            check(sector.colormap.empty(), "JED.mixedcase: sector.colormap");
            check(sector.sound.empty(), "JED.mixedcase: sector.sound");
            check(close(sector.soundVolume, 0.0), "JED.mixedcase: sector.soundVolume");
            check(sector.flags == 0, "JED.mixedcase: sector.flags");
            check(close(sector.ambient.intensity(), 0.5), "JED.mixedcase: sector.ambient.intensity");
            check(close(sector.extraLight.intensity(), 0.0), "JED.mixedcase: sector.extraLight.intensity");
            check(sector.layer == 0, "JED.mixedcase: sector.layer");

            if (!sector.surfaces.empty())
            {
                const sed::Surface& surf = sector.surfaces[0];
                check(surf.surfaceFlags == 0, "JED.mixedcase: surface.surfaceFlags");
                check(surf.faceFlags == 0u, "JED.mixedcase: surface.faceFlags");
                check(surf.vertices.size() == 3, "JED.mixedcase: surface.vertices.size");
                check(surf.texVertices.size() == 3, "JED.mixedcase: surface.texVertices.size");
                check(!surf.adjoin.has_value(), "JED.mixedcase: surface.adjoin");
            }
        }
    }

    {
        const char* jed = R"JED(
            JED 1.0
            SEC: HEADER
            HDR 1 4 15 2 2 - 16
            END
            SEC: GEO
            SECS 1
            - - 0 0x0 0.5 0 0 0 0 0
            VXS 3
            0 0 0
            2 0 0
            0 1 0
            SURFS 1
            wide.mat 0x0 0x0 0 0 0 0 3 -1 -1 0x0 2 3
            0 -32 112 1 1 320 -16 1 2 64 32 1 1 1 1 1 1 1 1 1 1
            END
        )JED";

        const auto overflowLoadRejected = [jed](
            const std::string_view target,
            const std::string_view replacement)
        {
            std::string malformed(jed);
            const std::size_t position = malformed.find(target);
            if (position == std::string::npos)
            {
                return false;
            }
            malformed.replace(position, target.size(), replacement);
            try
            {
                std::istringstream input(malformed);
                (void)sed::assets::loadFromJED(input);
            }
            catch (const std::runtime_error& exception)
            {
                return std::string_view(exception.what()).find("UV")
                    != std::string_view::npos;
            }
            return false;
        };
        check(overflowLoadRejected("0x0 2 3", "0x0 3.5e38 3"),
            "JED loader rejects a finite UV-scale double that overflows float storage");
        check(overflowLoadRejected("-32 112", "-3.5e38 112"),
            "JED loader rejects a finite texture-coordinate double that overflows float storage");

        std::istringstream pixelStream(jed);
        const sed::Level pixelLevel = sed::assets::loadFromJED(pixelStream);
        check(pixelLevel.uvCoordinateSpace
                == sed::UvCoordinateSpace::TexturePixels,
            "JED UV boundary: resource-independent import retains the explicit legacy pixel tag");

        const std::filesystem::path root =
            std::filesystem::temp_directory_path()
            / ("sed_jed_uv_"
                + std::to_string(
                    std::chrono::steady_clock::now()
                        .time_since_epoch().count()));
        const std::filesystem::path matPath = root / "mat" / "wide.mat";
        std::error_code error{};
        std::filesystem::create_directories(matPath.parent_path(), error);
        {
            const std::vector<std::byte> bytes =
                makeIndexedMatFixture(128, 64);
            std::ofstream output(
                matPath,
                std::ios::binary | std::ios::trunc);
            output.write(
                reinterpret_cast<const char*>(bytes.data()),
                static_cast<std::streamsize>(bytes.size()));
        }

        sed::assets::ResourceLocator resources{};
        resources.addRoot(root);
        sed::assets::JedLoadOptions options{};
        options.resources = &resources;
        std::vector<std::string> diagnostics{};
        options.onDiagnostic = [&diagnostics](
            const sed::DiagnosticSeverity,
            const std::string_view message)
        {
            diagnostics.emplace_back(message);
        };
        std::istringstream normalizedStream(jed);
        const sed::Level normalizedLevel =
            sed::assets::loadFromJED(normalizedStream, options);
        const sed::Surface& normalizedSurface =
            normalizedLevel.sectors[0].surfaces[0];
        check(!error && diagnostics.empty()
                && normalizedLevel.uvCoordinateSpace
                    == sed::UvCoordinateSpace::Normalized
                && close(normalizedSurface.texVertices[0].u, -0.25)
                && close(normalizedSurface.texVertices[0].v, 1.75)
                && close(normalizedSurface.texVertices[1].u, 2.5)
                && close(normalizedSurface.texVertices[1].v, -0.25)
                && close(normalizedSurface.uScale, 2.0 / 128.0)
                && close(normalizedSurface.vScale, 3.0 / 64.0),
            "JED UV boundary: real anisotropic MAT dimensions normalize signed multi-tile coordinates and scales before publication");

        sed::assets::JedSaveOptions saveOptions{};
        saveOptions.resources = &resources;
        std::ostringstream normalizedOutput{};
        sed::assets::saveToJED(
            normalizedLevel,
            normalizedOutput,
            saveOptions);
        std::istringstream exportedPixelsInput(normalizedOutput.str());
        const sed::Level exportedPixels = sed::assets::loadFromJED(
            exportedPixelsInput);
        const sed::Surface& exportedPixelSurface =
            exportedPixels.sectors[0].surfaces[0];
        check(exportedPixels.uvCoordinateSpace
                    == sed::UvCoordinateSpace::TexturePixels
                && close(exportedPixelSurface.texVertices[0].u, -32.0)
                && close(exportedPixelSurface.texVertices[0].v, 112.0)
                && close(exportedPixelSurface.texVertices[1].u, 320.0)
                && close(exportedPixelSurface.texVertices[1].v, -16.0)
                && close(exportedPixelSurface.uScale, 2.0)
                && close(exportedPixelSurface.vScale, 3.0),
            "JED UV boundary: normalized Level export writes legacy pixel coordinates and scales per MAT axis");

        std::ostream failedOutput(nullptr);
        bool writeFailureReported = false;
        try
        {
            sed::assets::saveToJED(
                normalizedLevel,
                failedOutput,
                saveOptions);
        }
        catch (const std::runtime_error& exception)
        {
            writeFailureReported = std::string_view(exception.what()).find(
                "Failed to write JED output") != std::string_view::npos;
        }
        check(writeFailureReported,
            "JED stream export reports a destination write failure after successful staging");

        diagnostics.clear();
        std::istringstream normalizedRoundTripInput(normalizedOutput.str());
        const sed::Level normalizedRoundTrip = sed::assets::loadFromJED(
            normalizedRoundTripInput,
            options);
        const sed::Surface& roundTripSurface =
            normalizedRoundTrip.sectors[0].surfaces[0];
        check(diagnostics.empty()
                && normalizedRoundTrip.uvCoordinateSpace
                    == sed::UvCoordinateSpace::Normalized
                && close(roundTripSurface.texVertices[0].u, -0.25)
                && close(roundTripSurface.texVertices[0].v, 1.75)
                && close(roundTripSurface.texVertices[1].u, 2.5)
                && close(roundTripSurface.texVertices[1].v, -0.25)
                && close(roundTripSurface.uScale, 2.0 / 128.0)
                && close(roundTripSurface.vScale, 3.0 / 64.0),
            "JED UV boundary: normalized export and resource-backed reload round-trip signed multi-tile UVs");

        bool unresolvedExportRejected = false;
        std::ostringstream unresolvedOutput{};
        try
        {
            sed::assets::JedSaveOptions unresolvedSaveOptions{};
            sed::assets::saveToJED(
                normalizedLevel,
                unresolvedOutput,
                unresolvedSaveOptions);
        }
        catch (const std::runtime_error& exception)
        {
            unresolvedExportRejected = std::string_view(exception.what())
                .find("MAT dimensions") != std::string_view::npos;
        }
        check(unresolvedExportRejected && unresolvedOutput.str().empty(),
            "JED UV boundary: normalized export rejects unavailable MAT dimensions instead of guessing or partly writing");

        const auto checkAtomicPathFailure = [&root](
            const sed::Level& candidate,
            const sed::assets::JedSaveOptions& candidateOptions,
            const std::string_view fixtureName,
            const std::string_view expectedError)
        {
            const std::filesystem::path failurePath =
                root / (std::string(fixtureName) + ".jed");
            const std::string sentinel =
                "pre-existing JED destination must survive\n";
            {
                std::ofstream output(
                    failurePath,
                    std::ios::binary | std::ios::trunc);
                output.write(
                    sentinel.data(),
                    static_cast<std::streamsize>(sentinel.size()));
                check(static_cast<bool>(output),
                    "JED atomic path fixture writes pre-existing destination data");
            }

            const sed::Level sourceBaseline = candidate;
            bool rejected = false;
            try
            {
                sed::assets::saveToJED(
                    candidate, failurePath, candidateOptions);
            }
            catch (const std::runtime_error& exception)
            {
                rejected = std::string_view(exception.what()).find(
                    expectedError) != std::string_view::npos;
            }

            std::ifstream input(failurePath, std::ios::binary);
            std::ostringstream contents{};
            contents << input.rdbuf();
            const std::string message =
                "JED filesystem export preserves its destination and source for "
                + std::string(fixtureName);
            check(rejected
                    && static_cast<bool>(input)
                    && contents.str() == sentinel
                    && sed::assets::sedLevelsEqual(
                        candidate, sourceBaseline),
                message.c_str());
        };

        enum class InvalidExportUvData
        {
            coordinate,
            scale,
            incomplete,
        };
        for (const sed::UvCoordinateSpace space : {
                 sed::UvCoordinateSpace::TexturePixels,
                 sed::UvCoordinateSpace::Normalized })
        {
            for (const InvalidExportUvData invalid : {
                     InvalidExportUvData::coordinate,
                     InvalidExportUvData::scale,
                     InvalidExportUvData::incomplete })
            {
                sed::Level candidate = normalizedLevel;
                candidate.uvCoordinateSpace = space;
                sed::Surface& surface =
                    candidate.sectors[0].surfaces[0];
                std::string_view expectedError{};
                std::string invalidName{};
                switch (invalid)
                {
                case InvalidExportUvData::coordinate:
                    surface.texVertices[0].u =
                        std::numeric_limits<float>::infinity();
                    expectedError = "UV value is invalid";
                    invalidName = "non-finite-coordinate";
                    break;
                case InvalidExportUvData::scale:
                    surface.vScale =
                        -std::numeric_limits<float>::infinity();
                    expectedError = "UV value is invalid";
                    invalidName = "non-finite-scale";
                    break;
                case InvalidExportUvData::incomplete:
                    surface.texVertices.pop_back();
                    expectedError = "texture-vertex data is incomplete";
                    invalidName = "incomplete-texture-vertices";
                    break;
                }

                const sed::Level sourceBaseline = candidate;
                bool streamRejected = false;
                std::ostringstream streamOutput{};
                try
                {
                    sed::assets::saveToJED(
                        candidate, streamOutput, saveOptions);
                }
                catch (const std::runtime_error& exception)
                {
                    streamRejected = std::string_view(exception.what()).find(
                        expectedError) != std::string_view::npos;
                }
                const std::string spaceName =
                    space == sed::UvCoordinateSpace::TexturePixels
                    ? "pixel"
                    : "normalized";
                const std::string streamMessage =
                    "JED export rejects atomically " + spaceName + ' '
                    + invalidName;
                check(streamRejected
                        && streamOutput.str().empty()
                        && sed::assets::sedLevelsEqual(
                            candidate, sourceBaseline),
                    streamMessage.c_str());

                checkAtomicPathFailure(
                    candidate,
                    saveOptions,
                    spaceName + '-' + invalidName,
                    expectedError);
            }
        }

        sed::assets::JedSaveOptions unavailableSaveOptions{};
        checkAtomicPathFailure(
            normalizedLevel,
            unavailableSaveOptions,
            "missing-mat-dimensions",
            "MAT dimensions");

        sed::Level nonFiniteLevel = normalizedLevel;
        nonFiniteLevel.sectors[0].surfaces[0].texVertices[0].u =
            std::numeric_limits<float>::infinity();
        checkAtomicPathFailure(
            nonFiniteLevel,
            saveOptions,
            "non-finite-texture-coordinate",
            "UV value is invalid");

        sed::Level invalidSpaceLevel = normalizedLevel;
        invalidSpaceLevel.uvCoordinateSpace =
            static_cast<sed::UvCoordinateSpace>(99);
        bool invalidSpaceRejected = false;
        std::ostringstream invalidSpaceOutput{};
        try
        {
            sed::assets::saveToJED(
                invalidSpaceLevel,
                invalidSpaceOutput,
                saveOptions);
        }
        catch (const std::runtime_error& exception)
        {
            invalidSpaceRejected = std::string_view(exception.what())
                .find("UV coordinate space is invalid")
                != std::string_view::npos;
        }
        check(invalidSpaceRejected && invalidSpaceOutput.str().empty(),
            "JED UV boundary: an invalid Level coordinate-space tag rejects before emitting partial output");
        checkAtomicPathFailure(
            invalidSpaceLevel,
            saveOptions,
            "invalid-uv-coordinate-space",
            "UV coordinate space is invalid");

        sed::assets::ResourceLocator missingResources{};
        options.resources = &missingResources;
        diagnostics.clear();
        std::istringstream missingStream(jed);
        const sed::Level missingLevel =
            sed::assets::loadFromJED(missingStream, options);
        check(missingLevel.uvCoordinateSpace
                    == sed::UvCoordinateSpace::TexturePixels
                && diagnostics.size() == 1u
                && diagnostics.front().find("sector 0, surface 0")
                    != std::string::npos,
            "JED UV boundary: missing MAT dimensions reject normalization atomically with one precise diagnostic");

        {
            const std::vector<std::byte> bytes =
                makeIndexedMatFixture(0, 64);
            std::ofstream output(
                matPath,
                std::ios::binary | std::ios::trunc);
            output.write(
                reinterpret_cast<const char*>(bytes.data()),
                static_cast<std::streamsize>(bytes.size()));
        }
        options.resources = &resources;
        diagnostics.clear();
        std::istringstream dimensionlessStream(jed);
        const sed::Level dimensionlessLevel =
            sed::assets::loadFromJED(dimensionlessStream, options);
        const sed::Surface& dimensionlessSurface =
            dimensionlessLevel.sectors[0].surfaces[0];
        check(dimensionlessLevel.uvCoordinateSpace
                    == sed::UvCoordinateSpace::TexturePixels
                && dimensionlessSurface.texVertices[0].u == -32.0f
                && dimensionlessSurface.texVertices[0].v == 112.0f
                && dimensionlessSurface.uScale == 2.0f
                && dimensionlessSurface.vScale == 3.0f
                && diagnostics.size() == 1u
                && diagnostics.front().find("sector 0, surface 0")
                    != std::string::npos,
            "JED UV boundary: a dimensionless MAT preserves every pixel value and scale without partial conversion");

        checkAtomicPathFailure(
            normalizedLevel,
            saveOptions,
            "invalid-mat-dimensions",
            "MAT dimensions");

        std::filesystem::remove_all(root, error);
    }
}

} // namespace sed::tests
