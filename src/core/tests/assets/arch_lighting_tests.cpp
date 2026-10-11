#include "test_common.hpp"

#include "sed/assets/jkl.hpp"
#include "sed/assets/sed.hpp"
#include "sed/level/clipboard.hpp"
#include "sed/level/arch_lighting.hpp"
#include "sed/level/thing.hpp"
#include "sed/assets/resource_locator.hpp"

#include <array>
#include <cstddef>
#include <limits>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>

namespace sed::tests
{

namespace
{

std::string makeMotsArchiveLightingFixture(const bool malformed)
{
    std::ostringstream out;
    out << "SECTION: HEADER\n"
        << "VERSION 1\n"
        << "END\n"
        << "SECTION: GEORESOURCE\n"
        << "WORLD COLORMAPS 0\n"
        << "WORLD VERTICES 0\n"
        << "WORLD TEXTURE VERTICES 0\n"
        << "WORLD ADJOINS 0\n"
        << "WORLD SURFACES 0\n"
        << "END\n"
        << "SECTION: THINGS\n"
        << "WORLD THINGS 2\n"
        << "0: arch_probe first 1 2 3 0 90 0 -1 0 thingflags=0x8\n"
        << "1: arch_probe second -1 -2 -3 0 0 0 -1 0 thingflags=0x8\n"
        << "END\n"
        << "SECTION: ARCHLIGHTING\n"
        << "Num ArchObjects 2\n";

    if (malformed)
    {
        out << "END\n";
        return out.str();
    }

    out << "archobject 0\n"
        << "nummeshes: 2\n"
        << "mesh 0\n"
        << "numvertices: 2\n"
        << "0: 0.000001 -2.5 10.5 0.75\n"
        << "1: 1.0 0.25 0.5 0.75\n"
        << "mesh 1\n"
        << "numvertices: 1\n"
        << "0: 0.5 0.25 0.125 0.75\n"
        << "archobject 1\n"
        << "nummeshes: 1\n"
        << "mesh 0\n"
        << "numvertices: 1\n"
        << "0: 0.75 0.5 0.25 0.125\n"
        << "END\n";
    return out.str();
}

} // namespace

void runArchLightingTests()
{
    {
        sed::assets::JklLoadOptions options{};
        options.projectType = sed::ProjectType::MOTS;

        std::istringstream input(makeMotsArchiveLightingFixture(false));
        const sed::Level level = sed::assets::loadFromJKL(input, options);
        check(level.kind == sed::ProjectType::MOTS,
            "MotS ARCHLIGHTING: explicit project context is retained");
        check(level.things.size() == 2,
            "MotS ARCHLIGHTING: fixture loads its Thing rows");
        check(level.things.size() >= 2
                && level.things[0].archLightingIndex == 0
                && level.things[1].archLightingIndex == 0,
            "MotS ARCHLIGHTING: shared Thing references use the object ordinal");
        check(level.motsArchLighting.size() == 2
                && level.motsArchLighting[0].wireIndex == 0
                && level.motsArchLighting[0].meshes.size() == 2
                && level.motsArchLighting[1].wireIndex == 1
                && level.motsArchLighting[1].meshes.size() == 1,
            "MotS ARCHLIGHTING: multiple objects, meshes, and an orphan record load");
        check(level.motsArchLighting.size() >= 2
                && !level.motsArchLighting[0].meshes.empty()
                && level.motsArchLighting[0].meshes[0].vertices.size() == 2
                && close(level.motsArchLighting[0].meshes[0].vertices[0].mono, 0.000001)
                && close(level.motsArchLighting[0].meshes[0].vertices[0].red, -2.5)
                && close(level.motsArchLighting[0].meshes[0].vertices[0].green, 10.5)
                && close(level.motsArchLighting[0].meshes[0].vertices[0].blue, 0.75)
                && close(level.motsArchLighting[0].meshes[0].vertices[1].mono, 1.0),
            "MotS ARCHLIGHTING: mono, RGB, and HDR channels load without clamping");

        std::ostringstream exported;
        sed::assets::saveToJKL(level, exported);
        const std::string text = exported.str();

        check(text.find(
                "0: arch_probe first 1.00000000 2.00000000 3.00000000 0.00000000 90.00000000 0.00000000 -1 0 ")
                != std::string::npos,
            "MotS ARCHLIGHTING: Thing architectural-light index survives export");
        check(text.find("Num ArchObjects 1\n") != std::string::npos,
            "MotS ARCHLIGHTING: export omits the unreferenced object");
        check(text.find("archobject 0\n") != std::string::npos,
            "MotS ARCHLIGHTING: object index survives export");
        check(text.find("nummeshes: 2\n") != std::string::npos,
            "MotS ARCHLIGHTING: mesh count survives export");
        check(text.find("mesh 0\n") != std::string::npos,
            "MotS ARCHLIGHTING: mesh index survives export");
        check(text.find("numvertices: 2\n") != std::string::npos,
            "MotS ARCHLIGHTING: vertex count survives export");
        check(text.find("0: 0.00000100 -2.50000000 10.50000000 0.75000000\n")
                != std::string::npos,
            "MotS ARCHLIGHTING: first mono/RGB vertex survives export");
        check(text.find("1: 1.00000000 0.25000000 0.50000000 0.75000000\n")
                != std::string::npos,
            "MotS ARCHLIGHTING: second mono/RGB vertex survives export");
        check(text.find("0: 0.75000000 0.50000000 0.25000000 0.12500000\n")
                == std::string::npos,
            "MotS ARCHLIGHTING: export omits an unreferenced archive object payload");

        std::istringstream reimportedInput(text);
        const sed::Level reimported = sed::assets::loadFromJKL(
            reimportedInput, options);
        check(reimported.things.size() >= 2
                && reimported.motsArchLighting.size() == 1
                && reimported.motsArchLighting[0] == level.motsArchLighting[0]
                && reimported.things[0].archLightingIndex == 0
                && reimported.things[1].archLightingIndex == 0,
            "MotS ARCHLIGHTING: export and reimport preserve active data and sharing while dropping orphans");
    }


    {
        sed::Level level{};
        level.kind = sed::ProjectType::MOTS;
        level.header.version = 1;
        const auto addThing = [&level](
            const std::string_view name,
            const std::int32_t flags,
            const std::int32_t archiveIndex)
        {
            sed::Thing& thing = level.newThing();
            thing.templateName = "arch_probe";
            thing.name = std::string(name);
            thing.sector = -1;
            thing.archLightingIndex = archiveIndex;
            sed::TplValue flagValue{};
            flagValue.name = "thingflags";
            flagValue.type = sed::ValueType::integer;
            flagValue.intValue = flags;
            thing.values.push_back(std::move(flagValue));
        };
        addThing("active_first", 0x8, 1);
        addThing("dormant", 0, 0);
        addThing("active_shared", 0x8, 1);
        addThing("active_default", 0x8, -1);

        const auto makeObject = [](const std::int32_t wireIndex,
                                   const sed::MotsArchLightVertex vertex)
        {
            sed::MotsArchLightObject object{};
            object.wireIndex = wireIndex;
            sed::MotsArchLightMesh mesh{};
            mesh.wireIndex = 0;
            mesh.vertices.push_back(vertex);
            object.meshes.push_back(std::move(mesh));
            return object;
        };
        level.motsArchLighting.push_back(
            makeObject(5, sed::MotsArchLightVertex{ 0.25f, 0.5f, 0.75f, 1.0f }));
        level.motsArchLighting.push_back(
            makeObject(11, sed::MotsArchLightVertex{ 0.1f, 0.2f, 0.3f, 0.4f }));
        level.motsArchLighting.push_back(
            makeObject(17, sed::MotsArchLightVertex{ 0.6f, 0.7f, 0.8f, 0.9f }));
        const sed::Level beforeExport = level;

        sed::assets::JklSaveOptions omitDefaultOptions{};
        omitDefaultOptions.omitDefaultMotsArchLighting = true;
        std::ostringstream firstOutput{};
        std::ostringstream secondOutput{};
        sed::assets::saveToJKL(level, firstOutput, omitDefaultOptions);
        sed::assets::saveToJKL(level, secondOutput, omitDefaultOptions);
        const std::string text = firstOutput.str();
        const bool activeReferencesReindexed =
            text.find("0: arch_probe active_first 0.00000000 0.00000000 0.00000000 0.00000000 0.00000000 0.00000000 -1 0 ")
                != std::string::npos
            && text.find("2: arch_probe active_shared 0.00000000 0.00000000 0.00000000 0.00000000 0.00000000 0.00000000 -1 0 ")
                != std::string::npos;
        const bool dormantReferenceCleared =
            text.find("1: arch_probe dormant 0.00000000 0.00000000 0.00000000 0.00000000 0.00000000 0.00000000 -1 -1 ")
                != std::string::npos;
        check(activeReferencesReindexed,
            "MotS ARCHLIGHTING export: active shared record references compact to deterministic table ordinal zero");
        check(dormantReferenceCleared,
            "MotS ARCHLIGHTING export: unflagged Thing keeps no JKL archive-light reference");
        const bool oneActiveRecord =
            text.find("Num ArchObjects 1\n") != std::string::npos;
        const bool activeWireLabel =
            text.find("archobject 11\n") != std::string::npos;
        const bool activeVertexChannels =
            text.find("0: 0.10000000 0.20000000 0.30000001 0.40000001\n")
                != std::string::npos;
        const bool inactiveWireLabelsOmitted =
            text.find("archobject 5\n") == std::string::npos
            && text.find("archobject 17\n") == std::string::npos;
        check(oneActiveRecord,
            "MotS ARCHLIGHTING export: only the active archive object is emitted");
        check(activeWireLabel,
            "MotS ARCHLIGHTING export: active object retains its wire label");
        check(activeVertexChannels,
            "MotS ARCHLIGHTING export: active vertex channels match the fixed vector");
        check(inactiveWireLabelsOmitted,
            "MotS ARCHLIGHTING export: inactive source objects are omitted");
        check(firstOutput.str() == secondOutput.str(),
            "MotS ARCHLIGHTING export: filtered table and index remapping are byte deterministic");
        check(sed::assets::sedLevelsEqual(level, beforeExport),
            "MotS ARCHLIGHTING export: filtering and reindexing do not mutate editor data");

        std::istringstream reimportedInput(firstOutput.str());
        sed::assets::JklLoadOptions options{};
        options.projectType = sed::ProjectType::MOTS;
        const sed::Level reimported = sed::assets::loadFromJKL(
            reimportedInput, options);
        check(reimported.things.size() == 4
                && reimported.things[0].archLightingIndex == 0
                && reimported.things[1].archLightingIndex == -1
                && reimported.things[2].archLightingIndex == 0
                && reimported.things[3].archLightingIndex == -1
                && reimported.motsArchLighting.size() == 1
                && reimported.motsArchLighting[0] == level.motsArchLighting[1],
            "MotS ARCHLIGHTING export: reimport preserves active references, omits dormant state, and opt-out leaves model-light fallback unbound");
    }

    {
        constexpr std::string_view modelText = R"3DO(SECTION: HEADER
3DO 2.0
SECTION: GEOMETRYDEF
GEOSETS 1
GEOSET 0
MESHES 1
MESH 0
NAME default_mesh
VERTICES 3
0: 0 0 0
1: 1 0 0
2: 0 1 0
FACES 0
)3DO";
        sed::Level level{};
        level.kind = sed::ProjectType::MOTS;
        level.header.version = 1;
        sed::Thing& thing = level.newThing();
        thing.templateName = "arch_probe";
        thing.name = "unbound_arch";
        thing.sector = -1;
        sed::TplValue flags{};
        flags.name = "thingflags";
        flags.type = sed::ValueType::integer;
        flags.intValue = 0x8;
        thing.values.push_back(flags);
        sed::TplValue model{};
        model.name = "model3d";
        model.type = sed::ValueType::str;
        model.stringValue = "arch_probe.3do";
        thing.values.push_back(model);

        sed::assets::ResourceLocator resources{};
        resources.setPrimaryResourceOverride("arch_probe.3do", [modelText]
        {
            const std::string_view source = modelText;
            std::vector<std::byte> bytes{};
            bytes.reserve(source.size());
            for (const char value : source)
            {
                bytes.push_back(static_cast<std::byte>(
                    static_cast<unsigned char>(value)));
            }
            return std::optional<std::vector<std::byte>>(std::move(bytes));
        });

        const sed::Level beforeExport = level;
        sed::assets::JklSaveOptions options{};
        options.resources = &resources;
        std::ostringstream output{};
        sed::assets::saveToJKL(level, output, options);
        const std::string text = output.str();
        check(text.find("Num ArchObjects 1\n") != std::string::npos
                && text.find("archobject 0\n") != std::string::npos
                && text.find("nummeshes: 1\n") != std::string::npos
                && text.find("numvertices: 3\n") != std::string::npos
                && text.find("0: 0.00000000 0.00000000 0.00000000 0.00000000\n")
                    != std::string::npos,
            "MotS ARCHLIGHTING export: an active unbound Arch Thing receives a model-sized all-zero record by default");
        check(sed::assets::sedLevelsEqual(level, beforeExport),
            "MotS ARCHLIGHTING export: synthesized defaults do not mutate the editor Level");

        sed::assets::JklLoadOptions loadOptions{};
        loadOptions.projectType = sed::ProjectType::MOTS;
        std::istringstream reimportInput(text);
        const sed::Level reimported = sed::assets::loadFromJKL(
            reimportInput, loadOptions);
        check(reimported.things.size() == 1
                && reimported.things[0].archLightingIndex == 0
                && reimported.motsArchLighting.size() == 1
                && reimported.motsArchLighting[0].meshes.size() == 1
                && reimported.motsArchLighting[0].meshes[0].vertices.size() == 3
                && sed::level::isDefaultMotsArchLighting(reimported.motsArchLighting[0]),
            "MotS ARCHLIGHTING export: default record reimports as a bound zero-valued three-vertex mesh");

        sed::Level boundMatching = level;
        boundMatching.things[0].archLightingIndex = 0;
        sed::MotsArchLightObject matchingObject{};
        matchingObject.wireIndex = 0;
        sed::MotsArchLightMesh matchingMesh{};
        matchingMesh.wireIndex = 0;
        matchingMesh.vertices.resize(3, sed::MotsArchLightVertex{});
        matchingObject.meshes.push_back(matchingMesh);
        boundMatching.motsArchLighting.push_back(matchingObject);
        std::ostringstream matchingOutput{};
        sed::assets::saveToJKL(boundMatching, matchingOutput, options);
        check(matchingOutput.str().find("Num ArchObjects 1\n")
                != std::string::npos,
            "MotS ARCHLIGHTING export: a bound record matching the resolved 3DO topology is accepted");

        sed::Level boundMismatched = boundMatching;
        boundMismatched.motsArchLighting[0].meshes[0].vertices.resize(2);
        std::ostringstream mismatchOutput{};
        mismatchOutput << "preserved";
        checkThrowsWithMessage(
            [&]
            {
                sed::assets::saveToJKL(
                    boundMismatched, mismatchOutput, options);
            },
            "Cannot export JKL/NDY ARCHLIGHTING: Thing 0 ('unbound_arch') ARCH object 0 does not match 3DO 'arch_probe.3do' topology.",
            "MotS ARCHLIGHTING export: a bound record with stale vertex topology is rejected with a fixed actionable diagnostic");
        check(mismatchOutput.str() == "preserved",
            "MotS ARCHLIGHTING export: topology rejection leaves the destination stream unchanged");

        sed::assets::JklSaveOptions omitOptions{};
        omitOptions.omitDefaultMotsArchLighting = true;
        std::ostringstream omittedOutput{};
        sed::assets::saveToJKL(level, omittedOutput, omitOptions);
        const std::string omittedText = omittedOutput.str();
        check(omittedText.find("Num ArchObjects 0\n") != std::string::npos
                && omittedText.find("0: arch_probe unbound_arch") != std::string::npos
                && omittedText.find("archobject 0\n") == std::string::npos,
            "MotS ARCHLIGHTING export: opt-out omits a virtual zero record without resolving the 3DO");
        std::istringstream omittedInput(omittedText);
        const sed::Level omittedReload = sed::assets::loadFromJKL(
            omittedInput, loadOptions);
        check(omittedReload.things.size() == 1
                && omittedReload.things[0].archLightingIndex == -1
                && omittedReload.motsArchLighting.empty(),
            "MotS ARCHLIGHTING export: opt-out reloads unbound and uses 3DO vertex-light fallback");

        checkThrowsWithMessage(
            [&]
            {
                sed::assets::JklSaveOptions missingResources{};
                std::ostringstream unused{};
                sed::assets::saveToJKL(level, unused, missingResources);
            },
            "Cannot export JKL/NDY ARCHLIGHTING: Thing 0 ('unbound_arch') has no ARCH record; resources are required to size its 3DO default.",
            "MotS ARCHLIGHTING export: active unbound default rejects missing resource context with a fixed actionable diagnostic");

        sed::Level missingModelLevel = level;
        missingModelLevel.things[0].values.back().stringValue = "missing.3do";
        sed::assets::ResourceLocator missingModelResources{};
        sed::assets::JklSaveOptions missingModelOptions{};
        missingModelOptions.resources = &missingModelResources;
        checkThrowsWithMessage(
            [&]
            {
                std::ostringstream unused{};
                sed::assets::saveToJKL(
                    missingModelLevel, unused, missingModelOptions);
            },
            "Cannot export JKL/NDY ARCHLIGHTING: Thing 0 ('unbound_arch') references missing 3DO 'missing.3do'; its default record cannot be sized.",
            "MotS ARCHLIGHTING export: a missing 3DO is named in the fixed failure diagnostic");
    }

    checkThrowsWithMessage(
        []
        {
            sed::assets::JklLoadOptions options{};
            options.projectType = sed::ProjectType::MOTS;
            std::istringstream input(makeMotsArchiveLightingFixture(true));
            (void)sed::assets::loadFromJKL(input, options);
        },
        "Cannot load JKL/NDY ARCHLIGHTING: archobject record is truncated.",
        "MotS ARCHLIGHTING: truncated object records are rejected");

    {
        std::string emptyMarker = makeMotsArchiveLightingFixture(false);
        const std::size_t section = emptyMarker.find("SECTION: ARCHLIGHTING\n");
        check(section != std::string::npos,
            "MotS ARCHLIGHTING: empty-marker fixture has a section");
        if (section != std::string::npos)
        {
            emptyMarker.erase(section);
            emptyMarker += "SECTION: ARCHLIGHTING\nEND\n";
            std::size_t row = 0;
            while ((row = emptyMarker.find(" -1 0 thingflags=0x8\n", row)) != std::string::npos)
            {
                const std::string old = " -1 0 thingflags=0x8\n";
                const std::string replacement = " -1 thingflags=0x8\n";
                emptyMarker.replace(row, old.size(), replacement);
                row += replacement.size();
            }
            sed::assets::JklLoadOptions options{};
            std::istringstream input(emptyMarker);
            const sed::Level level = sed::assets::loadFromJKL(input, options);
            check(level.kind == sed::ProjectType::MOTS
                    && level.motsArchLighting.empty()
                    && level.things.size() == 2
                    && level.things[0].archLightingIndex == -1,
                "MotS ARCHLIGHTING: empty marker detects MotS and defaults absent indices");
        }
    }

    const std::array<std::string_view, 4> channelValues{
        "0.000001", "-2.5", "10.5", "0.75" };
    const std::array<std::string_view, 4> channelNames{
        "mono", "red", "green", "blue" };
    for (std::size_t channelIndex = 0;
         channelIndex < channelValues.size();
         ++channelIndex)
    {
        std::string invalid = makeMotsArchiveLightingFixture(false);
        const std::size_t value = invalid.find(channelValues[channelIndex]);
        check(value != std::string::npos,
            "MotS ARCHLIGHTING: invalid-channel fixture anchor exists");
        if (value != std::string::npos)
        {
            invalid.replace(value, channelValues[channelIndex].size(), "nan");
            sed::assets::JklLoadOptions options{};
            const std::string message =
                "MotS ARCHLIGHTING: non-finite "
                + std::string(channelNames[channelIndex]) + " channel is rejected";
            checkThrowsWithMessage(
                [&invalid, &options]
                {
                    std::istringstream input(invalid);
                    (void)sed::assets::loadFromJKL(input, options);
                },
                "Cannot load JKL/NDY ARCHLIGHTING: a vertex channel is invalid or outside float range.",
                message.c_str());
        }
    }

    {
        // Per-object limits are not enough: a pair of individually valid
        // objects must not bypass the aggregate decoder/export budget.
        sed::assets::JklLoadOptions options{};
        options.projectType = sed::ProjectType::MOTS;
        std::istringstream input(makeMotsArchiveLightingFixture(false));
        sed::Level level = sed::assets::loadFromJKL(input, options);
        constexpr std::size_t halfBudget = 500'001;
        level.motsArchLighting.clear();
        level.motsArchLighting.reserve(2);
        for (std::int32_t objectIndex = 0; objectIndex < 2; ++objectIndex)
        {
            sed::MotsArchLightObject object{};
            object.wireIndex = objectIndex;
            object.meshes.reserve(halfBudget);
            for (std::size_t meshIndex = 0; meshIndex < halfBudget; ++meshIndex)
            {
                sed::MotsArchLightMesh mesh{};
                mesh.wireIndex = static_cast<std::int32_t>(meshIndex);
                object.meshes.push_back(std::move(mesh));
            }
            level.motsArchLighting.push_back(std::move(object));
        }
        // Both oversized records must be active; the JKL export snapshot now
        // deliberately drops unreachable archive objects before budgeting.
        level.things[1].archLightingIndex = 1;
        std::ostringstream output;
        output << "preserved";
        checkThrowsWithMessage(
            [&level, &output]
            {
                sed::assets::saveToJKL(level, output);
            },
            "Cannot export JKL/NDY ARCHLIGHTING: total mesh record count is too large.",
            "MotS ARCHLIGHTING: aggregate mesh budget rejects multiple valid objects");
        check(output.str() == "preserved",
            "MotS ARCHLIGHTING: aggregate budget failure leaves output unchanged");
    }

    {
        // The same aggregate limit must be enforced before a Thing clipboard
        // paste publishes its combined table.  Both the destination record
        // and the incoming record are individually valid, but their combined
        // vertex count is not exportable.
        constexpr std::size_t maxVertexRecords = 1'000'000;
        sed::Level destination{};
        destination.kind = sed::ProjectType::MOTS;
        sed::Thing& existingThing = destination.newThing();
        existingThing.name = "existing";
        existingThing.archLightingIndex = 0;

        sed::MotsArchLightObject existingObject{};
        existingObject.wireIndex = 7;
        sed::MotsArchLightMesh existingMesh{};
        existingMesh.wireIndex = 3;
        existingMesh.vertices.resize(
            maxVertexRecords - 1,
            sed::MotsArchLightVertex{ 0.25f, 0.5f, 0.75f, 1.0f });
        existingObject.meshes.push_back(std::move(existingMesh));
        destination.motsArchLighting.push_back(std::move(existingObject));

        const auto makePayload = [](const std::int32_t wireIndex)
        {
            sed::level::ThingClipboardPayload payload{};
            payload.sourceProject = sed::ProjectType::MOTS;
            sed::level::ThingClipboardEntry entry{};
            entry.thing.name = "incoming";
            entry.thing.archLightingIndex = 0;
            payload.things.push_back(std::move(entry));

            sed::MotsArchLightObject incomingObject{};
            incomingObject.wireIndex = wireIndex;
            sed::MotsArchLightMesh incomingMesh{};
            incomingMesh.wireIndex = 4;
            incomingMesh.vertices.push_back(
                sed::MotsArchLightVertex{ -1.0f, 2.0f, -3.0f, 4.0f });
            incomingObject.meshes.push_back(std::move(incomingMesh));
            payload.motsArchLighting.push_back(std::move(incomingObject));
            payload.motsArchLightingSourceIndices.push_back(-1);
            return payload;
        };

        // Exactly one additional vertex reaches the inclusive export limit.
        const auto exactLimitPaste = sed::level::pasteEntityClipboard(
            destination,
            sed::level::EntityClipboard{ makePayload(8) });
        const bool exactLimitShape = exactLimitPaste
            && destination.things.size() == 2
            && destination.things[1].archLightingIndex == 1
            && destination.motsArchLighting.size() == 2
                && destination.motsArchLighting[0].meshes.size() == 1
                && destination.motsArchLighting[0].meshes[0].vertices.size()
                    == maxVertexRecords - 1
                && destination.motsArchLighting[1].meshes.size() == 1
                && destination.motsArchLighting[1].meshes[0].vertices.size() == 1;
        check(exactLimitShape,
            "MotS ARCHLIGHTING clipboard paste accepts the inclusive aggregate vertex limit");

        if (exactLimitShape)
        {
            const sed::Level beforeOverLimit = destination;
            const auto overLimitPaste = sed::level::pasteEntityClipboard(
                destination,
                sed::level::EntityClipboard{ makePayload(9) });
            check(!overLimitPaste
                    && sed::assets::sedLevelsEqual(destination, beforeOverLimit),
                "MotS ARCHLIGHTING clipboard paste rejects an aggregate vertex overflow atomically");
        }
    }

    {
        std::string invalid = makeMotsArchiveLightingFixture(false);
        const std::size_t row = invalid.find(" -1 0 thingflags=0x8\n");
        check(row != std::string::npos,
            "MotS ARCHLIGHTING: invalid-index fixture anchor exists");
        if (row != std::string::npos)
        {
            invalid.replace(row, std::string(" -1 0").size(), " -1 9");
            sed::assets::JklLoadOptions options{};
            checkThrowsWithMessage(
                [&invalid, &options]
                {
                    std::istringstream input(invalid);
                    (void)sed::assets::loadFromJKL(input, options);
                },
                "Cannot load JKL/NDY ARCHLIGHTING: Thing architectural-light index is invalid.",
                "MotS ARCHLIGHTING: Thing index outside the decoded table is rejected");
        }
    }

    {
        // Keep malformed header/count/float cases in one table so each
        // declared record shape has an explicit failure-path assertion.
        struct InvalidFixture
        {
            const char* name;
            const char* needle;
            const char* replacement;
            const char* expected;
        };
        const std::array<InvalidFixture, 4> invalidFixtures{
            InvalidFixture{
                "wrong ARCHLIGHTING header",
                "Num ArchObjects 2\n",
                "Num Wrong 2\n",
                "Cannot load JKL/NDY ARCHLIGHTING: object count is invalid." },
            InvalidFixture{
                "declared object count larger than records",
                "Num ArchObjects 2\n",
                "Num ArchObjects 3\n",
                "Cannot load JKL/NDY ARCHLIGHTING: archobject record is truncated." },
            InvalidFixture{
                "declared object count smaller than records",
                "Num ArchObjects 2\n",
                "Num ArchObjects 1\n",
                "Cannot load JKL/NDY ARCHLIGHTING: unexpected record after declared objects." },
            InvalidFixture{
                "float channel outside representable range",
                "0.000001 -2.5 10.5 0.75\n",
                "0.000001 -2.5 1e100 0.75\n",
                "Cannot load JKL/NDY ARCHLIGHTING: a vertex channel is invalid or outside float range." },
        };

        for (const InvalidFixture& fixture : invalidFixtures)
        {
            std::string invalid = makeMotsArchiveLightingFixture(false);
            const std::size_t anchor = invalid.find(fixture.needle);
            check(anchor != std::string::npos, fixture.name);
            if (anchor == std::string::npos)
            {
                continue;
            }
            invalid.replace(anchor, std::string(fixture.needle).size(), fixture.replacement);
            sed::assets::JklLoadOptions options{};
            options.projectType = sed::ProjectType::MOTS;
            checkThrowsWithMessage(
                [&invalid, &options]
                {
                    std::istringstream input(invalid);
                    (void)sed::assets::loadFromJKL(input, options);
                },
                fixture.expected,
                fixture.name);
        }
    }

    {
        // An old MotS row may omit the optional architectural-light token;
        // this is distinct from an explicit -1 sentinel and must retain the
        // same unbound meaning for every Thing row.
        std::string absentIndex = makeMotsArchiveLightingFixture(false);
        std::size_t row = 0;
        while ((row = absentIndex.find(" -1 0 thingflags=0x8\n", row)) != std::string::npos)
        {
            absentIndex.replace(row, std::string(" -1 0").size(), " -1");
            row += std::string(" -1\n").size();
        }
        sed::assets::JklLoadOptions options{};
        options.projectType = sed::ProjectType::MOTS;
        std::istringstream input(absentIndex);
        const sed::Level level = sed::assets::loadFromJKL(input, options);
        check(level.things.size() >= 2
                && level.things[0].archLightingIndex == -1
                && level.things[1].archLightingIndex == -1,
            "MotS ARCHLIGHTING: absent Thing architectural-light tokens default to -1");
    }

    {
        sed::assets::JklLoadOptions options{};
        options.projectType = sed::ProjectType::MOTS;
        std::istringstream input(makeMotsArchiveLightingFixture(false));
        sed::Level level = sed::assets::loadFromJKL(input, options);
        const bool shape = level.motsArchLighting.size() >= 1
            && !level.motsArchLighting[0].meshes.empty()
            && !level.motsArchLighting[0].meshes[0].vertices.empty();
        check(shape, "MotS ARCHLIGHTING: export-validation fixture has a vertex");
        if (shape)
        {
            level.motsArchLighting[0].meshes[0].vertices[0].blue =
                std::numeric_limits<float>::infinity();
            std::ostringstream output;
            output << "preserved";
            checkThrowsWithMessage(
                [&level, &output]
                {
                    sed::assets::saveToJKL(level, output);
                },
                "Cannot export JKL/NDY ARCHLIGHTING: a vertex channel is not finite.",
                "MotS ARCHLIGHTING: invalid channels reject before output mutation");
            check(output.str() == "preserved",
                "MotS ARCHLIGHTING: failed export leaves the destination stream unchanged");
        }
    }

    {
        sed::assets::JklLoadOptions options{};
        options.projectType = sed::ProjectType::MOTS;
        std::istringstream input(makeMotsArchiveLightingFixture(false));
        sed::Level source = sed::assets::loadFromJKL(input, options);
        const std::array<std::int32_t, 2> selection{ 0, 1 };
        const auto clipboard = sed::level::copyThingsToClipboard(
            source, selection, 0);
        const auto* payload = clipboard
            ? std::get_if<sed::level::ThingClipboardPayload>(&*clipboard)
            : nullptr;
        check(payload && payload->motsArchLighting.size() == 1
                && payload->motsArchLightingSourceIndices
                    == std::vector<std::int32_t>{ 0 }
                && payload->things.size() == 2
                && payload->things[0].thing.archLightingIndex == 0
                && payload->things[1].thing.archLightingIndex == 0,
            "MotS ARCHLIGHTING: Thing clipboard keeps one shared referenced object and local indices");

        const bool payloadShape = payload != nullptr
            && payload->motsArchLighting.size() == 1
            && payload->things.size() >= 2;
        if (payloadShape)
        {
            std::string encodeError{};
            const auto encoded = sed::level::encodeEntityClipboard(
                *clipboard, &encodeError);
            std::string decodeError{};
            const auto decoded = encoded
                ? sed::level::decodeEntityClipboard(*encoded, &decodeError)
                : std::nullopt;
            const auto* decodedPayload = decoded
                ? std::get_if<sed::level::ThingClipboardPayload>(&*decoded)
                : nullptr;
            const bool decodedShape = decodedPayload != nullptr
                && decodedPayload->motsArchLighting.size() == 1
                && decodedPayload->things.size() >= 2;
            check(decodedShape && encodeError.empty() && decodeError.empty()
                    && decodedPayload->motsArchLighting[0]
                        == payload->motsArchLighting[0]
                    && decodedPayload->things[0].thing.archLightingIndex == 0
                    && decodedPayload->things[1].thing.archLightingIndex == 0,
                "MotS ARCHLIGHTING: encoded Thing clipboard preserves table and shared references");

            if (decodedShape && encodeError.empty() && decodeError.empty())
            {
                sed::Level destination{};
                destination.kind = sed::ProjectType::MOTS;
                destination.motsArchLighting.push_back(
                    payload->motsArchLighting[0]);
                destination.motsArchLighting[0].meshes[0].vertices[0].mono = -2.0f;
                const auto pasted = sed::level::pasteEntityClipboard(
                    destination, *clipboard);
                check(pasted && destination.things.size() == 2
                        && destination.motsArchLighting.size() == 2
                        && destination.things[0].archLightingIndex == 1
                        && destination.things[1].archLightingIndex == 1
                        && destination.motsArchLighting[1].meshes
                            == payload->motsArchLighting[0].meshes
                        && destination.motsArchLighting[1].wireIndex !=
                            destination.motsArchLighting[0].wireIndex,
                    "MotS ARCHLIGHTING: cross-level paste remaps shared refs and wire-label collisions");

                const auto duplicate = sed::level::pasteEntityClipboard(
                    source, *clipboard);
                check(duplicate && source.things.size() == 4
                        && source.motsArchLighting.size() == 2
                        && source.things[2].archLightingIndex == 0
                        && source.things[3].archLightingIndex == 0,
                    "MotS ARCHLIGHTING: same-level duplication reuses the authored table slot");
            }
        }
    }

    {
        // RED contract for the interactive authoring copy path.  The source
        // and target model identities/topology are supplied by the immutable
        // application resource snapshot; the core must validate every target
        // before publishing any binding or record.
        sed::Level copyLevel{};
        copyLevel.kind = sed::ProjectType::MOTS;
        copyLevel.layers.emplace_back("ARCH copy fixture");

        const auto addFlagAndModel = [](sed::Thing& thing)
        {
            sed::TplValue flags{};
            flags.name = "thingflags";
            flags.type = sed::ValueType::integer;
            flags.intValue = 0x8;
            thing.values.push_back(flags);

            sed::TplValue model{};
            model.name = "model3d";
            model.type = sed::ValueType::str;
            model.stringValue = "arch_copy.3do";
            thing.values.push_back(model);
        };

        sed::Thing sourceThing{};
        sourceThing.name = "source";
        sourceThing.layer = 0;
        sourceThing.archLightingIndex = 0;
        addFlagAndModel(sourceThing);
        copyLevel.things.push_back(sourceThing);

        sed::Thing sourceAlias = sourceThing;
        sourceAlias.name = "source_alias";
        copyLevel.things.push_back(sourceAlias);

        sed::Thing sharedDestination = sourceThing;
        sharedDestination.name = "shared_destination";
        sharedDestination.archLightingIndex = 1;
        copyLevel.things.push_back(sharedDestination);

        sed::Thing sharedOwner = sourceThing;
        sharedOwner.name = "shared_owner";
        sharedOwner.archLightingIndex = 1;
        copyLevel.things.push_back(sharedOwner);

        sed::Thing unboundDestination = sourceThing;
        unboundDestination.name = "unbound_destination";
        unboundDestination.archLightingIndex = -1;
        copyLevel.things.push_back(unboundDestination);

        sed::MotsArchLightMesh sourceMesh{};
        sourceMesh.wireIndex = 0;
        sourceMesh.vertices = {
            sed::MotsArchLightVertex{ 0.25f, 0.5f, 0.75f, 1.0f },
            sed::MotsArchLightVertex{ 0.5f, 1.0f, 1.5f, 2.0f } };
        sed::MotsArchLightObject sourceObject{};
        sourceObject.wireIndex = 10;
        sourceObject.meshes.push_back(sourceMesh);
        copyLevel.motsArchLighting.push_back(sourceObject);

        sed::MotsArchLightMesh destinationMesh{};
        destinationMesh.wireIndex = 0;
        destinationMesh.vertices = {
            sed::MotsArchLightVertex{ 9.0f, 8.0f, 7.0f, 6.0f },
            sed::MotsArchLightVertex{ 5.0f, 4.0f, 3.0f, 2.0f } };
        sed::MotsArchLightObject destinationObject{};
        destinationObject.wireIndex = 11;
        destinationObject.meshes.push_back(destinationMesh);
        copyLevel.motsArchLighting.push_back(destinationObject);

        const sed::level::MotsArchLightingTopology topology{
            { { 2u } } };
        const std::string modelIdentity = "3DO|arch_copy.3do|revision-1";
        const std::array<sed::level::MotsArchLightingCopyTarget, 2> targets{
            sed::level::MotsArchLightingCopyTarget{
                2, modelIdentity, topology },
            sed::level::MotsArchLightingCopyTarget{
                4, modelIdentity, topology } };

        const auto copied = sed::level::copyMotsArchLighting(
            copyLevel,
            0,
            modelIdentity,
            topology,
            targets);
        check(copied.status == sed::level::MotsArchLightingEditStatus::Applied
                && copied.message
                    == "Copied MotS ARCHLIGHTING to selected owners."
                && copied.changedThingIndices
                    == std::vector<std::int32_t>{ 2, 4 }
                && copyLevel.things[2].archLightingIndex == 0
                && copyLevel.things[4].archLightingIndex == 0
                && copyLevel.things[2].archLightingIndex
                    != copyLevel.things[3].archLightingIndex
                && copyLevel.motsArchLighting.size() == 2
                && copyLevel.motsArchLighting[1] == destinationObject
                && copyLevel.motsArchLighting[0] == sourceObject,
            "MotS ARCHLIGHTING copy binds selected targets to the source record and preserves an unselected destination owner");

        const auto noOp = sed::level::copyMotsArchLighting(
            copyLevel,
            0,
            modelIdentity,
            topology,
            std::array<sed::level::MotsArchLightingCopyTarget, 1>{
                sed::level::MotsArchLightingCopyTarget{
                    1, modelIdentity, topology } });
        check(noOp.status == sed::level::MotsArchLightingEditStatus::Noop,
            "MotS ARCHLIGHTING copy treats a target already bound to the source record as a no-op");
        check(noOp.message == "No MotS ARCHLIGHTING changes were necessary."
                && noOp.changedThingIndices.empty(),
            "MotS ARCHLIGHTING copy no-op has a fixed message and no changed owners");

        const sed::Level beforeEmptyCopy = copyLevel;
        const auto emptyCopy = sed::level::copyMotsArchLighting(
            copyLevel,
            0,
            modelIdentity,
            topology,
            std::array<sed::level::MotsArchLightingCopyTarget, 0>{});
        check(emptyCopy.status == sed::level::MotsArchLightingEditStatus::Noop
                && emptyCopy.message
                    == "No MotS ARCHLIGHTING changes were necessary."
                && emptyCopy.changedThingIndices.empty()
                && sed::assets::sedLevelsEqual(copyLevel, beforeEmptyCopy),
            "MotS ARCHLIGHTING copy treats an empty target set as an atomic no-op");

        // Reset the first destination to its old record so the mixed-invalid
        // batch would mutate it before the second target rejects.  Equality
        // below therefore proves validation happened before publication.
        copyLevel.things[2].archLightingIndex = 1;
        const sed::Level beforeRejectedCopy = copyLevel;
        const auto rejectedIdentity = sed::level::copyMotsArchLighting(
            copyLevel,
            0,
            modelIdentity,
            topology,
            std::array<sed::level::MotsArchLightingCopyTarget, 2>{
                sed::level::MotsArchLightingCopyTarget{
                    2, modelIdentity, topology },
                sed::level::MotsArchLightingCopyTarget{
                    4, "3DO|different.3do|revision-1", topology } });
        check(rejectedIdentity.status
                    == sed::level::MotsArchLightingEditStatus::Rejected
                && rejectedIdentity.message
                    == "MotS ARCHLIGHTING copy requires matching model identity for every target."
                && rejectedIdentity.changedThingIndices.empty()
                && sed::assets::sedLevelsEqual(copyLevel, beforeRejectedCopy),
            "MotS ARCHLIGHTING copy rejects a mixed model identity atomically");

        const auto rejectedTopology = sed::level::copyMotsArchLighting(
            copyLevel,
            0,
            modelIdentity,
            topology,
            std::array<sed::level::MotsArchLightingCopyTarget, 1>{
                sed::level::MotsArchLightingCopyTarget{
                    4, modelIdentity, sed::level::MotsArchLightingTopology{
                        { { 1u } } } } });
        check(rejectedTopology.status
                    == sed::level::MotsArchLightingEditStatus::Rejected
                && rejectedTopology.message
                    == "MotS ARCHLIGHTING copy requires compatible all-LOD topology for every target."
                && rejectedTopology.changedThingIndices.empty()
                && sed::assets::sedLevelsEqual(copyLevel, beforeRejectedCopy),
            "MotS ARCHLIGHTING copy rejects an incompatible all-LOD topology atomically");

        const auto rejectedDuplicate = sed::level::copyMotsArchLighting(
            copyLevel,
            0,
            modelIdentity,
            topology,
            std::array<sed::level::MotsArchLightingCopyTarget, 2>{
                sed::level::MotsArchLightingCopyTarget{
                    2, modelIdentity, topology },
                sed::level::MotsArchLightingCopyTarget{
                    2, modelIdentity, topology } });
        check(rejectedDuplicate.status
                    == sed::level::MotsArchLightingEditStatus::Rejected
                && rejectedDuplicate.message
                    == "MotS ARCHLIGHTING Thing selection contains a duplicate index."
                && rejectedDuplicate.changedThingIndices.empty()
                && sed::assets::sedLevelsEqual(copyLevel, beforeRejectedCopy),
            "MotS ARCHLIGHTING copy rejects duplicate targets before publication");

        const auto rejectedOutOfRange = sed::level::copyMotsArchLighting(
            copyLevel,
            0,
            modelIdentity,
            topology,
            std::array<sed::level::MotsArchLightingCopyTarget, 2>{
                sed::level::MotsArchLightingCopyTarget{
                    2, modelIdentity, topology },
                sed::level::MotsArchLightingCopyTarget{
                    99, modelIdentity, topology } });
        check(rejectedOutOfRange.status
                    == sed::level::MotsArchLightingEditStatus::Rejected
                && rejectedOutOfRange.message
                    == "MotS ARCHLIGHTING Thing selection contains an invalid index."
                && rejectedOutOfRange.changedThingIndices.empty()
                && sed::assets::sedLevelsEqual(copyLevel, beforeRejectedCopy),
            "MotS ARCHLIGHTING copy rejects an out-of-range target atomically");

        const auto rejectedEmptyIdentity = sed::level::copyMotsArchLighting(
            copyLevel,
            0,
            "",
            topology,
            std::array<sed::level::MotsArchLightingCopyTarget, 1>{
                sed::level::MotsArchLightingCopyTarget{
                    4, modelIdentity, topology } });
        check(rejectedEmptyIdentity.status
                    == sed::level::MotsArchLightingEditStatus::Rejected
                && rejectedEmptyIdentity.message
                    == "MotS ARCHLIGHTING copy requires a non-empty source model identity."
                && rejectedEmptyIdentity.changedThingIndices.empty()
                && sed::assets::sedLevelsEqual(copyLevel, beforeRejectedCopy),
            "MotS ARCHLIGHTING copy rejects an empty source model identity atomically");

        const auto rejectedTargetIdentity = sed::level::copyMotsArchLighting(
            copyLevel,
            0,
            modelIdentity,
            topology,
            std::array<sed::level::MotsArchLightingCopyTarget, 1>{
                sed::level::MotsArchLightingCopyTarget{
                    4, "", topology } });
        check(rejectedTargetIdentity.status
                    == sed::level::MotsArchLightingEditStatus::Rejected
                && rejectedTargetIdentity.message
                    == "MotS ARCHLIGHTING copy requires a non-empty model identity for every target."
                && rejectedTargetIdentity.changedThingIndices.empty()
                && sed::assets::sedLevelsEqual(copyLevel, beforeRejectedCopy),
            "MotS ARCHLIGHTING copy rejects an empty target model identity atomically");

        sed::Level nonGeometryTarget = copyLevel;
        nonGeometryTarget.things[4].values[0].intValue = 0;
        const auto promotedNonGeometryTarget = sed::level::copyMotsArchLighting(
            nonGeometryTarget,
            0,
            modelIdentity,
            topology,
            std::array<sed::level::MotsArchLightingCopyTarget, 2>{
                sed::level::MotsArchLightingCopyTarget{
                    2, modelIdentity, topology },
                sed::level::MotsArchLightingCopyTarget{
                    4, modelIdentity, topology } });
        check(promotedNonGeometryTarget.status
                    == sed::level::MotsArchLightingEditStatus::Applied
                && promotedNonGeometryTarget.message
                    == "Copied MotS ARCHLIGHTING to selected owners."
                && promotedNonGeometryTarget.changedThingIndices
                    == std::vector<std::int32_t>{ 2, 4 }
                && nonGeometryTarget.things[2].archLightingIndex == 0
                && nonGeometryTarget.things[4].archLightingIndex == 0
                && sed::level::thingFlags(
                    nonGeometryTarget, nullptr, nonGeometryTarget.things[4])
                    == 0x8,
            "MotS ARCHLIGHTING copy promotes an eligible model target without mutating its template");

        sed::Level staleTarget = copyLevel;
        staleTarget.things[4].archLightingIndex = 99;
        const sed::Level beforeStaleTarget = staleTarget;
        const auto rejectedStaleTarget = sed::level::copyMotsArchLighting(
            staleTarget,
            0,
            modelIdentity,
            topology,
            std::array<sed::level::MotsArchLightingCopyTarget, 1>{
                sed::level::MotsArchLightingCopyTarget{
                    4, modelIdentity, topology } });
        check(rejectedStaleTarget.status
                    == sed::level::MotsArchLightingEditStatus::Rejected
                && rejectedStaleTarget.message
                    == "Selected Thing has no valid MotS ARCHLIGHTING record."
                && rejectedStaleTarget.changedThingIndices.empty()
                && sed::assets::sedLevelsEqual(staleTarget, beforeStaleTarget),
            "MotS ARCHLIGHTING copy rejects a stale destination binding atomically");

        sed::Level unboundSource = copyLevel;
        unboundSource.things[0].archLightingIndex = -1;
        const sed::Level beforeUnboundSource = unboundSource;
        const auto rejectedUnboundSource = sed::level::copyMotsArchLighting(
            unboundSource,
            0,
            modelIdentity,
            topology,
            std::array<sed::level::MotsArchLightingCopyTarget, 1>{
                sed::level::MotsArchLightingCopyTarget{
                    4, modelIdentity, topology } });
        check(rejectedUnboundSource.status
                    == sed::level::MotsArchLightingEditStatus::Rejected
                && rejectedUnboundSource.message
                    == "Selected Thing has no valid MotS ARCHLIGHTING record."
                && rejectedUnboundSource.changedThingIndices.empty()
                && sed::assets::sedLevelsEqual(
                    unboundSource, beforeUnboundSource),
            "MotS ARCHLIGHTING copy rejects an unbound source atomically");

        sed::Level wrongProject = copyLevel;
        wrongProject.kind = sed::ProjectType::JKDF2;
        const sed::Level beforeWrongProject = wrongProject;
        const auto rejectedProject = sed::level::copyMotsArchLighting(
            wrongProject,
            0,
            modelIdentity,
            topology,
            std::array<sed::level::MotsArchLightingCopyTarget, 1>{
                sed::level::MotsArchLightingCopyTarget{
                    4, modelIdentity, topology } });
        check(rejectedProject.status
                    == sed::level::MotsArchLightingEditStatus::Rejected
                && rejectedProject.message
                    == "MotS ARCHLIGHTING editing requires a MotS level."
                && rejectedProject.changedThingIndices.empty()
                && sed::assets::sedLevelsEqual(wrongProject, beforeWrongProject),
            "MotS ARCHLIGHTING copy rejects a non-MotS level atomically");

        copyLevel.motsArchLighting[0].meshes[0].vertices[0].red =
            std::numeric_limits<float>::quiet_NaN();
        const sed::Level beforeNonFiniteCopy = copyLevel;
        const auto rejectedNonFinite = sed::level::copyMotsArchLighting(
            copyLevel,
            0,
            modelIdentity,
            topology,
            std::array<sed::level::MotsArchLightingCopyTarget, 1>{
                sed::level::MotsArchLightingCopyTarget{
                    4, modelIdentity, topology } });
        check(rejectedNonFinite.status
                    == sed::level::MotsArchLightingEditStatus::Rejected
                && rejectedNonFinite.message
                    == "MotS ARCHLIGHTING copy source channels must be finite."
                && rejectedNonFinite.changedThingIndices.empty()
                && sed::assets::sedLevelsEqual(copyLevel, beforeNonFiniteCopy),
            "MotS ARCHLIGHTING copy rejects non-finite source channels without partial mutation");

        copyLevel.motsArchLighting[0] = sourceObject;
        copyLevel.motsArchLighting[0].wireIndex = -1;
        const sed::Level beforeInvalidStructureCopy = copyLevel;
        const auto rejectedStructure = sed::level::copyMotsArchLighting(
            copyLevel,
            0,
            modelIdentity,
            topology,
            std::array<sed::level::MotsArchLightingCopyTarget, 1>{
                sed::level::MotsArchLightingCopyTarget{
                    4, modelIdentity, topology } });
        check(rejectedStructure.status
                    == sed::level::MotsArchLightingEditStatus::Rejected
                && rejectedStructure.message
                    == "MotS ARCHLIGHTING copy source record structure is invalid."
                && rejectedStructure.changedThingIndices.empty()
                && sed::assets::sedLevelsEqual(
                    copyLevel, beforeInvalidStructureCopy),
            "MotS ARCHLIGHTING copy reports invalid source structure separately from non-finite channels");
    }
}

} // namespace sed::tests
