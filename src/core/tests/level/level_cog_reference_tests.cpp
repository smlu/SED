#include "test_common.hpp"

#include "sed/assets/sed.hpp"
#include "sed/editor/document.hpp"
#include "sed/level/build.hpp"
#include "sed/level/cleave.hpp"
#include "sed/level/edit.hpp"

#include <array>
#include <string>
#include <string_view>

namespace sed::tests
{
namespace
{
Level cogFixture(const std::string_view text)
{
    Level level;
    level.sectors.push_back(makeLitTestCubeSector(0, ColorWhite));
    level.sectors.push_back(makeLitTestCubeSector(6, ColorWhite));
    for ( std::size_t i = 0; i < level.sectors.size(); ++i )
    {
        level.sectors[i].num = static_cast<std::int32_t>(i);
        level.sectors[i].id = static_cast<std::int32_t>(i + 1);
        for ( std::size_t f = 0; f < level.sectors[i].surfaces.size(); ++f )
        {
            level.sectors[i].surfaces[f].num = static_cast<std::int32_t>(f);
        }
    }
    level.curId = 2;
    Cog cog;
    CogValue value;
    value.name = "authored_surface_reference";
    value.floatValue = -123.125;
    value.local = true;
    value.description = "Preserve authored COG metadata.";
    value.mask = 0x12345678;
    value.type = ValueType::ptr;
    value.cogType = CogType::surface;
    value.intValue = 10;
    value.stringValue = text;
    cog.values.push_back(value);
    value.intValue = 5;
    value.stringValue = text.empty() ? "" : text == "10" ? "5" : "0 5";
    cog.values.push_back(value);
    value.intValue = -1;
    value.stringValue = "-1";
    cog.values.push_back(value);
    value.cogType = CogType::sec;
    value.intValue = 1;
    value.stringValue = "1";
    cog.values.push_back(value);
    level.cogs.push_back(std::move(cog));
    return level;
}
void expectedReference(const Level& level, const std::size_t index, const std::int32_t global,
                       const std::string_view exported, const std::string_view jed,
                       const std::string_view stored = "", const CogType expectedType = CogType::surface)
{
    const bool exists = level.cogs.size() == 1 && index < level.cogs[0].values.size();
    check(exists, "Structural editing preserves the inspected COG and value indices");
    if ( !exists )
    {
        return;
    }
    const auto& value = level.cogs[0].values[index];
    check(value.name == "authored_surface_reference" && value.floatValue == -123.125
              && value.local && value.description == "Preserve authored COG metadata."
              && value.mask == 0x12345678,
          "Structural editing preserves every fixed non-reference CogValue field");
    check(value.type == ValueType::ptr && value.cogType == expectedType && value.intValue == global,
          "Structural editing preserves the fixed semantic COG target and pointer type");
    check(value.stringValue == stored, "Structural editing preserves the complete fixed COG text state");
    check(value.asString() == exported,
          "Structural editing emits the exact fixed global COG token used by game export");
    check(value.asJedString(level) == jed,
          "Structural editing emits the exact fixed semantic JED COG reference");
}
void expectedHorizontalFace(const Level& level, const std::size_t valueIndex, const double z)
{
    if ( level.cogs.size() != 1 || valueIndex >= level.cogs[0].values.size() )
    {
        check(false, "Referenced horizontal face retains its COG value");
        return;
    }
    std::int32_t local = level.cogs[0].values[valueIndex].intValue;
    const std::array<TVector, 4> bottom{{{-1, -1, -1}, {1, -1, -1}, {1, 1, -1}, {-1, 1, -1}}};
    const std::array<TVector, 4> top{{{-1, -1, 1}, {-1, 1, 1}, {1, 1, 1}, {1, -1, 1}}};
    const auto& expected = z < 0 ? bottom : top;
    for ( const auto& sector : level.sectors )
    {
        if ( local < 0 )
        {
            break;
        }
        if ( static_cast<std::size_t>(local) >= sector.surfaces.size() )
        {
            local -= static_cast<std::int32_t>(sector.surfaces.size());
            continue;
        }
        const auto& face = sector.surfaces[static_cast<std::size_t>(local)];
        bool matched = false;
        if ( face.vertices.size() == 4 )
        {
            for ( std::size_t start = 0; start < 4; ++start )
            {
                bool equal = true;
                for ( std::size_t corner = 0; corner < 4; ++corner )
                {
                    const auto index = face.vertices[corner];
                    if ( index < 0 || static_cast<std::size_t>(index) >= sector.vertices.size() )
                    {
                        equal = false;
                        break;
                    }
                    const auto& point = sector.vertices[static_cast<std::size_t>(index)];
                    const auto& fixed = expected[(start + corner) % 4];
                    equal = equal && point.x == fixed.x && point.y == fixed.y && point.z == fixed.z;
                }
                matched = matched || equal;
            }
        }
        check(matched, "COG target preserves the independently fixed complete horizontal face and winding");
        return;
    }
    check(false, "Referenced horizontal face exists in the flattened output geometry");
}
void laterOwnerCases()
{
    for ( const std::string_view text : {"", "10", "1 4"} )
    {
        for ( const int operation : {0, 1, 2, 3, 4} )
        {
            editor::Document document;
            document.level() = cogFixture(text);
            const Level before = document.level();
            document.pushUndo("Edit geometry with COG references", editor::DocumentDirty::All);
            bool accepted = false;
            if ( operation == 0 )
            {
                accepted = level::levelCleaveSurface(document.level(), {0, 0}, {0, 0, 1}, 0, 0, 0);
            }
            else if ( operation == 1 )
            {
                accepted = level::levelExtrudeSurface(document.level(), {0, 0}, .2, -.1);
            }
            else if ( operation == 2 )
            {
                accepted = level::levelDeleteSurface(document.level(), {0, 0});
            }
            else if ( operation == 3 )
            {
                accepted = level::levelDeleteSector(document.level(), 0);
            }
            else
            {
                const std::array<level::SectorCleavePlane, 1> planes{{{{0, 0, 1}, {0, 0, 0}}}};
                accepted = level::levelCleaveSectorByPlanes(document.level(), 0, planes).succeeded();
            }
            check(accepted, "COG reference fixture geometry edit succeeds");
            if ( !accepted )
            {
                continue;
            }
            const std::array<std::int32_t, 5> globals{11, 14, 9, 4, 10};
            const std::array<std::string_view, 5> exported{"11", "14", "9", "4", "10"};
            expectedReference(document.level(), 0, globals[operation], exported[operation],
                              operation == 3 ? "0 4" : "1 4", operation == 4 ? text : "");
            expectedReference(document.level(), 2, -1, "-1", "-1", "-1");
            expectedReference(document.level(), 3, operation == 3 ? 0 : 1, operation == 3 ? "0" : "1",
                              operation == 3 ? "0" : "1", operation == 3 ? "" : "1", CogType::sec);
            if ( operation < 2 )
            {
                expectedReference(document.level(), 1, 5, "5", "0 5",
                                  text.empty()   ? ""
                                  : text == "10" ? "5"
                                                 : "0 5");
            }
            else if ( operation == 2 )
            {
                expectedReference(document.level(), 1, 4, "4", "0 4");
            }
            else if ( operation == 3 )
            {
                expectedReference(document.level(), 1, -1, "-1", "-1");
            }
            else if ( operation == 4 )
            {
                // The complete z=-1 face follows the appended lower body.
                expectedReference(document.level(), 1, 12, "12", "2 0");
                expectedHorizontalFace(document.level(), 1, -1);
            }
            const Level committed = document.level();
            const auto restored = assets::loadFromSedBytes(assets::saveToSedBytes(committed));
            check(restored.cogs.size() == 1 && committed.cogs.size() == 1 &&
                      restored.cogs[0].values.size() == committed.cogs[0].values.size(),
                  "Native SED retains the complete COG collection cardinality");
            if ( restored.cogs.size() == 1 && committed.cogs.size() == 1 &&
                 restored.cogs[0].values.size() == committed.cogs[0].values.size() )
            {
                for ( std::size_t i = 0; i < committed.cogs[0].values.size(); ++i )
                {
                    const auto& actual = restored.cogs[0].values[i];
                    const auto& expected = committed.cogs[0].values[i];
                    check(actual.type == expected.type && actual.cogType == expected.cogType &&
                              actual.intValue == expected.intValue &&
                              actual.stringValue == expected.stringValue && actual.name == expected.name
                              && actual.floatValue == expected.floatValue && actual.local == expected.local
                              && actual.description == expected.description && actual.mask == expected.mask,
                          "Native SED retains all nine verified CogValue fields");
                }
            }
            expectedReference(restored, 0, globals[operation], exported[operation],
                              operation == 3 ? "0 4" : "1 4", operation == 4 ? text : "");
            check(document.undo() && assets::sedLevelsEqual(document.level(), before),
                  "Undo restores complete geometry and original authored COG integer/text state");
            expectedReference(document.level(), 0, 10, "10", "1 4", text);
            check(document.redo() && assets::sedLevelsEqual(document.level(), committed),
                  "Redo restores complete geometry and remapped COG state");
            expectedReference(document.level(), 0, globals[operation], exported[operation],
                              operation == 3 ? "0 4" : "1 4", operation == 4 ? text : "");
        }
        auto level = cogFixture(text);
        check(level::levelCleaveSurface(level, {0, 0}, {0, 0, 1}, 0, 0, 0) &&
                  level::levelDeleteSurface(level, {0, 6}),
              "Chained append/delete structural edits succeed");
        expectedReference(level, 0, 10, "10", "1 4");
    }
}
} // namespace
void joinAndIdentityCases()
{
    for ( const std::string_view text : {"", "16", "2 4"} )
    {
        for ( const auto mode : {level::SectorConnectKnifeMode::KeepTargetPieces,
                                 level::SectorConnectKnifeMode::CutTargetHolesOnly} )
        {
            auto input = cogFixture("");
            input.sectors.insert(input.sectors.begin() + 1, makeLitTestCubeSector(1, ColorWhite));
            for ( std::size_t i = 0; i < input.sectors.size(); ++i )
            {
                input.sectors[i].num = static_cast<std::int32_t>(i);
                input.sectors[i].id = static_cast<std::int32_t>(i + 1);
            }
            input.curId = 3;
            input.cogs[0].values[0].intValue = 16;
            input.cogs[0].values[0].stringValue = text;
            input.cogs[0].values[3].intValue = 2;
            input.cogs[0].values[3].stringValue = "2";
            check(level::levelConnectSectorsKnife(input, 0, 1, mode), "COG reference Join fixture succeeds");
            expectedReference(input, 0, 10, "10", "1 4");
            expectedReference(input, 3, 1, "1", "1", "", CogType::sec);
            const auto restored = assets::loadFromSedBytes(assets::saveToSedBytes(input));
            expectedReference(restored, 0, 10, "10", "1 4");
        }
    }
    for ( const std::string_view text : {"", "10", "1 4"} )
    {
        auto input = cogFixture(text);
        CogValue top = input.cogs[0].values[1];
        top.intValue = 4;
        top.stringValue = text.empty() ? "" : text == "10" ? "4" : "0 4";
        input.cogs[0].values.push_back(top);
        const std::array<level::SectorCleavePlane, 1> planes{{{{0, 0, 1}, {0, 0, .5}}}};
        check(level::levelCleaveSectorByPlanes(input, 0, planes).succeeded(),
              "Off-centre Sector Cleave retains the original main body identity");
        expectedReference(input, 0, 10, "10", "1 4", text);
        expectedReference(input, 1, 0, "0", "0 0");
        expectedHorizontalFace(input, 1, -1);
        expectedReference(input, 4, 12, "12", "2 0");
        expectedHorizontalFace(input, 4, 1);
    }
}
void runLevelCogReferenceTests()
{
    laterOwnerCases();
    joinAndIdentityCases();
    auto level = cogFixture("1 4");
    level.sectors[1].surfaces[4].num = 44;
    level.cogs[0].values[0].stringValue = "1 44";
    check(level::levelDeleteSurface(level, {1, 5}), "Noncanonical surface-number fixture edit succeeds");
    expectedReference(level, 0, 10, "10", "1 4");
    auto edge = cogFixture("");
    edge.sectors[0].surfaces[3].num = 99;
    edge.sectors[0].surfaces[4].num = 3;
    edge.cogs[0].values[0].intValue = 4;
    edge.cogs[0].values[0].stringValue = "0 3";
    check(level::levelCleaveEdge(edge, {0, 0}, 0, {0, 0, 1}, 0, 0, 0),
          "Edge Cleave with noncanonical surface numbers succeeds");
    expectedReference(edge, 0, 4, "4", "0 4");
    auto malformed = cogFixture("");
    malformed.cogs[0].values[0].intValue = 5;
    malformed.cogs[0].values[0].stringValue = "0 6";
    check(malformed.cogs[0].values[0].asJedString(malformed) == "0 5",
          "Initially invalid preferred pair text falls back to the fixed valid global target");
    check(level::levelCleaveSurface(malformed, {0, 0}, {0, 0, 1}, 0, 0, 0),
          "Malformed preferred COG text fixture adds a new source face");
    expectedReference(malformed, 0, 5, "5", "0 5");

    auto invalid = cogFixture("");
    invalid.cogs[0].values[0].intValue = 99;
    invalid.cogs[0].values[0].stringValue = "not a reference";
    invalid.cogs[0].values[1].type = ValueType::integer;
    invalid.cogs[0].values[1].intValue = 77;
    invalid.cogs[0].values[1].stringValue = "opaque authored text";
    const auto invalidBefore = invalid.cogs[0].values;
    check(level::levelCleaveSurface(invalid, {0, 0}, {0, 0, 1}, 0, 0, 0),
          "Geometry edit preserves unrelated malformed and non-pointer COG values");
    for ( std::size_t i = 0; i < 2; ++i )
    {
        const auto& actual = invalid.cogs[0].values[i];
        const auto& expected = invalidBefore[i];
        check(actual.type == expected.type && actual.cogType == expected.cogType &&
                  actual.intValue == expected.intValue && actual.stringValue == expected.stringValue && actual.name == expected.name
                              && actual.floatValue == expected.floatValue && actual.local == expected.local
                              && actual.description == expected.description && actual.mask == expected.mask,
              "Unresolved and non-pointer COG values retain every authored type and scalar field");
    }
    for ( const bool fallback : {false, true} )
    {
        auto extruded = cogFixture("");
        auto& value = extruded.cogs[0].values[0];
        value.intValue = fallback ? 5 : 12;
        value.stringValue = fallback ? "2 0" : "";
        check(value.asJedString(extruded) == (fallback ? "0 5" : "-1"),
            "Pre-append Extrude COG fixture has the fixed valid fallback or unresolved target");
        check(level::levelExtrudeSurface(extruded, {0, 0}, .2, -.1),
            "Expanded Extrude with pre-append COG reference controls succeeds");
        if ( fallback )
        {
            expectedReference(extruded, 0, 5, "5", "0 5");
        }
        else
        {
            const auto& actual = extruded.cogs[0].values[0];
            check(actual.type == ValueType::ptr && actual.cogType == CogType::surface
                && actual.intValue == 12 && actual.stringValue.empty()
                && actual.name == "authored_surface_reference" && actual.floatValue == -123.125
                && actual.local && actual.description == "Preserve authored COG metadata."
                && actual.mask == 0x12345678,
                "Originally unresolved COG pointer fields remain unchanged when a new Sector is appended");
        }
    }

    {
        // Safe vertex deletion compacts the owner's authored Surface::num
        // fields even when no polygon references the removed vertex.  A
        // paired JED surface token must follow its original face identity,
        // rather than remaining textually bound to the newly numbered face.
        Level level{};
        Sector& sector = level.newSector();
        (void)sector.newVertex(0.0, 0.0, 0.0);
        (void)sector.newVertex(10.0, 10.0, 0.0); // unused vertex
        (void)sector.newVertex(1.0, 0.0, 0.0);
        (void)sector.newVertex(0.0, 1.0, 0.0);
        (void)sector.newVertex(2.0, 0.0, 0.0);
        (void)sector.newVertex(3.0, 0.0, 0.0);
        (void)sector.newVertex(2.0, 1.0, 0.0);

        Surface first{};
        first.num = 1;
        first.vertices = { 0, 2, 3 };
        first.texVertices.resize(first.vertices.size());
        (void)sector.addSurface(std::move(first));

        Surface second{};
        second.num = 0;
        second.vertices = { 4, 5, 6 };
        second.texVertices.resize(second.vertices.size());
        (void)sector.addSurface(std::move(second));
        sector.surfaces[0].num = 1;
        sector.surfaces[1].num = 0;

        Cog& cog = level.newCog();
        CogValue target{};
        target.name = "target";
        target.type = ValueType::ptr;
        target.cogType = CogType::surface;
        target.intValue = 1;
        target.stringValue = "0 0";
        cog.values.push_back(std::move(target));

        check(level.cogs[0].values[0].asJedString(level) == "0 0",
              "COG vertex-delete fixture starts with the fixed paired surface token");
        check(level::levelDeleteSectorVertex(level, 0, 1, false),
              "COG vertex-delete fixture removes the unused vertex");
        check(level.sectors[0].surfaces[0].num == 0
                && level.sectors[0].surfaces[1].num == 1,
              "COG vertex-delete fixture renumbers the surviving surfaces");
        check(level.cogs[0].values[0].intValue == 1
                && level.cogs[0].values[0].asJedString(level) == "0 1",
              "COG vertex deletion preserves paired surface identity after renumbering");

        Level forced = level;
        const std::int32_t unused = forced.sectors[0].newVertex(20.0, 20.0, 0.0);
        check(level::levelForceDeleteSectorVertex(forced, 0, unused),
              "COG force vertex deletion removes an unused vertex");
        check(forced.cogs[0].values[0].intValue == 1
                && forced.cogs[0].values[0].asJedString(forced) == "0 1",
              "COG force vertex deletion preserves paired surface identity");

        Level rejected = level;
        const Level rejectedBefore = rejected;
        check(!level::levelDeleteSectorVertex(rejected, 0, 0, false),
              "COG safe vertex deletion rejects a nonredundant vertex");
        check(assets::sedLevelsEqual(rejected, rejectedBefore)
                && rejected.cogs[0].values[0].intValue == 1
                && rejected.cogs[0].values[0].stringValue == "",
              "COG rejected vertex deletion preserves geometry and paired reference state");
    }

}
} // namespace sed::tests
