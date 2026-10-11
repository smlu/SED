#include "test_common.hpp"

#include "sed/level.hpp"

namespace sed::tests
{

void runLevelLayerTests()
{
    {
        sed::Level level{};
        level.clear();

        check(level.getMasterCMP() == "dflt.cmp", "Level.getMasterCMP.default");

        level.masterCMP = "main.cmp";
        check(level.getMasterCMP() == "main.cmp", "Level.getMasterCMP.masterCMP");
    }

    {
        sed::Level level{};
        level.clear();

        sed::Sector& sector = level.newSector();
        sector.colormap = "sec.cmp";

        check(level.getMasterCMP() == "sec.cmp", "Level.getMasterCMP.sector0.colormap");
    }

    {
        sed::Level level{};
        level.clear();

        level.layers.push_back("Main");
        check(level.getLayerName(0) == "Main", "Level.getLayerName.0");
        check(level.getLayerName(3) == "LAYER3", "Level.getLayerName.3");
        check(level.getLayerName(-1) == "LAYER-1", "Level.getLayerName.-1");
    }

    {
        sed::Level level{};
        level.clear();

        level.layers.push_back("Main");

        const std::int32_t mainIndex = level.addLayer("Main");
        const std::int32_t newIndex = level.addLayer("New");

        check(mainIndex == 0, "Level.addLayer.mainIndex");
        check(newIndex == 1, "Level.addLayer.newIndex");
        check(level.layers.size() == 2, "Level.addLayer.size");
        if (level.layers.size() == 2)
        {
            check(level.layers[0] == "Main", "Level.addLayer.layers[0]");
            check(level.layers[1] == "New", "Level.addLayer.layers[1]");
        }
    }

    {
        sed::Level level{};
        level.clear();

        level.layers.push_back("Main");

        sed::Sector& sector = level.newSector();
        sector.layer = 5;

        sed::Thing& thing = level.newThing();
        thing.layer = 2;

        sed::SedLight& light = level.newLight();
        light.layer = -1;

        level.addMissingLayers();

        check(level.layers.size() == 4, "Level.addMissingLayers.layers.size");
        if (level.layers.size() == 4)
        {
            check(level.layers[0] == "Main", "Level.addMissingLayers.layers[0]");
            check(level.layers[1] == "Layer5", "Level.addMissingLayers.layers[1]");
            check(level.layers[2] == "Layer2", "Level.addMissingLayers.layers[2]");
            check(level.layers[3] == "Layer-1", "Level.addMissingLayers.layers[3]");
        }

        check(sector.layer == 1, "Level.addMissingLayers.sector.layer");
        check(thing.layer == 2, "Level.addMissingLayers.thing.layer");
        check(light.layer == 3, "Level.addMissingLayers.light.layer");
    }

    {
        sed::Level level{};
        level.clear();
        level.layers = { "Geometry", "Unused", "Gameplay", "Lighting" };

        level.newSector().layer = 0;
        level.newSector().layer = 2;
        level.newThing().layer = 2;
        level.newLight().layer = 3;

        const std::vector<std::size_t> usage = level.layerUsageCounts();
        check(usage == std::vector<std::size_t>({ 1, 0, 2, 1 }),
            "Level.layerUsageCounts.all entity kinds");

        // Delete the unused layer and reorder the remaining semantic layers.
        const bool remapped = level.remapLayers(
            { "Lighting", "Gameplay", "Geometry" },
            { 2, -1, 1, 0 });
        check(remapped, "Level.remapLayers.reorder and compact succeeds");
        check(level.layers == std::vector<std::string>({ "Lighting", "Gameplay", "Geometry" }),
            "Level.remapLayers.names");
        check(level.sectors.size() == 2 && level.sectors[0].layer == 2 && level.sectors[1].layer == 1,
            "Level.remapLayers.sectors preserve semantic membership");
        check(level.things.size() == 1 && level.things[0].layer == 1,
            "Level.remapLayers.things preserve semantic membership");
        check(level.lights.size() == 1 && level.lights[0].layer == 0,
            "Level.remapLayers.lights preserve semantic membership");
    }

    {
        sed::Level level{};
        level.clear();
        level.layers = { "Used", "Unused" };
        level.newThing().layer = 0;

        const sed::Level before = level;
        check(!level.remapLayers({ "Unused" }, { -1, 0 }),
            "Level.remapLayers.rejects deleting a used layer");
        check(level.layers == before.layers && level.things[0].layer == before.things[0].layer,
            "Level.remapLayers.used deletion is atomic");

        check(!level.remapLayers({ "Used", "Unused" }, { 0, 0 }),
            "Level.remapLayers.rejects semantic merge");
        check(!level.remapLayers({ "Used" }, { 0 }),
            "Level.remapLayers.rejects incomplete old mapping");
    }

    {
        sed::Level level{};
        level.clear();
        level.layers = { "Main" };
        level.newSector().layer = 9;
        level.newThing().layer = 9;
        level.newLight().layer = -2;

        check(!level.remapLayers({ "Main" }, { 0 }),
            "Level.remapLayers.rejects malformed entity layer references");

        level.addMissingLayers();
        check(level.layers == std::vector<std::string>({ "Main", "Layer9", "Layer-2" }),
            "Level.addMissingLayers.normalizes gapped indices by semantic source name");
        check(level.sectors[0].layer == 1 && level.things[0].layer == 1 && level.lights[0].layer == 2,
            "Level.addMissingLayers.coalesces repeated malformed indices");

        check(level.remapLayers({ "Layer9", "Layer-2", "Main" }, { 2, 0, 1 }),
            "Level.remapLayers.accepts normalized malformed data");
        check(level.sectors[0].layer == 0 && level.things[0].layer == 0 && level.lights[0].layer == 1,
            "Level.remapLayers.normalized data retains semantic membership");
    }
}

} // namespace sed::tests
