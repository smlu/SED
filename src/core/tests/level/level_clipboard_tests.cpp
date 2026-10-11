#include "test_common.hpp"

#include "sed/level/clipboard.hpp"
#include "sed/assets/sed.hpp"
#include "sed/level/geometry.hpp"

#include <array>
#include <cstddef>
#include <limits>
#include <string>
#include <variant>

namespace sed::tests
{
namespace
{

sed::TplValue makeFrame(
    const double x,
    const double y,
    const double z,
    const double pitch,
    const double yaw,
    const double roll)
{
    sed::TplValue frame{};
    frame.name = "frame";
    frame.type = sed::ValueType::frame;
    frame.adjType = sed::AdjType::frame;
    frame.setFrame(x, y, z, pitch, yaw, roll);
    return frame;
}

} // namespace

void runLevelClipboardTests()
{
    {
        const auto maximumId = std::numeric_limits<std::int32_t>::max();
        for ( const std::int32_t nextId : {maximumId - 1, maximumId} )
        {
            sed::Level destination;
            destination.layers = {"Existing"};
            destination.curId = nextId;
            sed::level::ThingClipboardPayload payload;
            sed::level::ThingClipboardEntry entry;
            entry.layer = {0, "Imported"};
            payload.things.push_back(entry);
            const auto before = sed::assets::saveToSedBytes(destination);
            const auto pasted = sed::level::pasteEntityClipboard(
                destination, sed::level::EntityClipboard{payload}, std::nullopt);
            if ( nextId == maximumId )
            {
                check(!pasted && sed::assets::saveToSedBytes(destination) == before,
                    "Clipboard.Thing exhausted next ID rejects without publishing layer or owner");
            }
            else
            {
                check(pasted && destination.curId == maximumId && destination.things.size() == 1
                        && destination.things[0].id == maximumId - 1,
                    "Clipboard.Thing last representable next ID boundary admits exactly one owner");
                const auto boundary = sed::assets::saveToSedBytes(destination);
                const auto rejected = sed::level::pasteEntityClipboard(
                    destination, sed::level::EntityClipboard{payload}, std::nullopt);
                check(!rejected && sed::assets::saveToSedBytes(destination) == boundary,
                    "Clipboard.Thing ID exhaustion after last admission remains atomic");
            }
        }
    }
    {
        const std::int32_t maximumId =
            std::numeric_limits<std::int32_t>::max();
        sed::level::ThingClipboardPayload thingPayload{};
        sed::level::ThingClipboardEntry thingEntry{};
        thingEntry.layer = {0, "Imported"};
        thingPayload.things = {thingEntry, thingEntry};

        sed::Level thingDestination{};
        thingDestination.layers = {"Existing"};
        thingDestination.curId = maximumId - 1;
        const auto thingBefore = sed::assets::saveToSedBytes(thingDestination);
        const auto thingPaste = sed::level::pasteEntityClipboard(
            thingDestination,
            sed::level::EntityClipboard{thingPayload},
            std::nullopt);
        check(!thingPaste
                && thingDestination.things.empty()
                && thingDestination.layers
                    == std::vector<std::string>{"Existing"}
                && thingDestination.curId == maximumId - 1
                && sed::assets::saveToSedBytes(thingDestination) == thingBefore,
            "Clipboard.Thing whole-count ID preflight rejects two owners with one remaining ID before layer import");

        sed::level::SectorClipboardPayload sectorPayload{};
        sectorPayload.layers = {{0, "Imported"}, {0, "Imported"}};
        sectorPayload.sectors = {
            makeTestCubeSector(0.0),
            makeTestCubeSector(3.0),
        };
        // Clipboard paste validates a UV entry for every source surface vertex.
        for ( sed::Sector& sector : sectorPayload.sectors )
        {
            for ( sed::Surface& surface : sector.surfaces )
            {
                surface.texVertices.resize(surface.vertices.size());
            }
        }
        sed::Level sectorDestination{};
        sectorDestination.layers = {"Existing"};
        sectorDestination.curId = maximumId - 1;
        const auto sectorBefore = sed::assets::saveToSedBytes(sectorDestination);
        const auto sectorPaste = sed::level::pasteEntityClipboard(
            sectorDestination,
            sed::level::EntityClipboard{sectorPayload},
            std::nullopt);
        check(!sectorPaste
                && sectorDestination.sectors.empty()
                && sectorDestination.layers
                    == std::vector<std::string>{"Existing"}
                && sectorDestination.curId == maximumId - 1
                && sed::assets::saveToSedBytes(sectorDestination)
                    == sectorBefore,
            "Clipboard.Sector whole-count ID preflight rejects two owners before layer import or append");

        sectorDestination.curId = maximumId - 2;
        const auto exactSectorPaste = sed::level::pasteEntityClipboard(
            sectorDestination,
            sed::level::EntityClipboard{sectorPayload},
            std::nullopt);
        check(exactSectorPaste
                && exactSectorPaste->indices == std::vector<std::int32_t>{0, 1}
                && sectorDestination.sectors.size() == 2
                && sectorDestination.sectors[0].id == maximumId - 2
                && sectorDestination.sectors[1].id == maximumId - 1
                && sectorDestination.curId == maximumId
                && sectorDestination.layers
                    == std::vector<std::string>{"Existing", "Imported"},
            "Clipboard.Sector exact two-ID capacity succeeds and imports its layer once");

        sed::level::LightClipboardPayload lightPayload{};
        sed::level::LightClipboardEntry lightEntry{};
        lightEntry.layer = {0, "Imported"};
        lightEntry.light.position = {1.0, 2.0, 3.0};
        lightPayload.lights = {lightEntry, lightEntry};
        sed::Level lightDestination{};
        lightDestination.layers = {"Existing"};
        lightDestination.curId = maximumId - 1;
        const auto lightBefore = sed::assets::saveToSedBytes(lightDestination);
        const auto lightPaste = sed::level::pasteEntityClipboard(
            lightDestination,
            sed::level::EntityClipboard{lightPayload},
            std::nullopt);
        check(!lightPaste
                && lightDestination.lights.empty()
                && lightDestination.layers
                    == std::vector<std::string>{"Existing"}
                && lightDestination.curId == maximumId - 1
                && sed::assets::saveToSedBytes(lightDestination) == lightBefore,
            "Clipboard.Light whole-count ID preflight rejects two owners before layer import or append");

        lightDestination.curId = maximumId - 2;
        const auto exactLightPaste = sed::level::pasteEntityClipboard(
            lightDestination,
            sed::level::EntityClipboard{lightPayload},
            std::nullopt);
        check(exactLightPaste
                && exactLightPaste->indices == std::vector<std::int32_t>{0, 1}
                && lightDestination.lights.size() == 2
                && lightDestination.lights[0].id == maximumId - 2
                && lightDestination.lights[1].id == maximumId - 1
                && lightDestination.curId == maximumId
                && lightDestination.layers
                    == std::vector<std::string>{"Existing", "Imported"},
            "Clipboard.Light exact two-ID capacity succeeds and imports its layer once");
    }
    {
        const double maximum = std::numeric_limits<double>::max();
        for (int invalid = 0; invalid < 5; ++invalid)
        {
            sed::Level destination;
            destination.layers = {"Existing"};
            destination.curId = 42;
            destination.things.emplace_back().name = "retained owner";
            const auto before = sed::assets::saveToSedBytes(destination);
            sed::level::ThingClipboardPayload payload;
            sed::level::ThingClipboardEntry first;
            first.layer = {0,"Imported"};
            first.thing.x = 1; first.thing.y = 2; first.thing.z = 3;
            payload.things.push_back(first);
            payload.things.push_back(first);
            std::optional<sed::TVector> target = sed::TVector{maximum,0,0};
            if (invalid == 0) target = sed::TVector{std::numeric_limits<double>::infinity(),0,0};
            else if (invalid == 1) target = sed::TVector{std::numeric_limits<double>::quiet_NaN(),0,0};
            else if (invalid == 2) payload.anchor = sed::TVector{-maximum,0,0};
            else if (invalid == 3) payload.things[1].thing.x = maximum;
            else
            {
                sed::TplValue frame;
                frame.name = "frame"; frame.type = sed::ValueType::frame;
                frame.stringValue = "(1.7976931348623157e+308/2/3:4/5/6)";
                payload.things[1].thing.values.push_back(frame);
            }
            const auto pasted = sed::level::pasteEntityClipboard(destination,sed::level::EntityClipboard{payload},target);
            check(!pasted && sed::assets::saveToSedBytes(destination) == before,
                "Clipboard.Thing rejection preserves full destination including earlier entries imported layers and next ID");
        }
        for (int mode = 0; mode < 4; ++mode)
        {
            sed::Level destination;
            destination.layers = {"Existing"}; destination.curId = 42;
            destination.things.emplace_back().name = "retained owner";
            sed::level::ThingClipboardPayload payload;
            sed::level::ThingClipboardEntry entry;
            entry.layer = {0,"Imported"};
            entry.thing.x = mode == 2 ? maximum : 1;
            entry.thing.y = 2; entry.thing.z = 3;
            sed::TplValue frame;
            frame.name = "frame"; frame.type = sed::ValueType::frame;
            frame.stringValue = mode == 2 ? " (1.7976931348623157e+308/5/6:7/8/9) " : " (4/5/6:7/8/9) ";
            entry.thing.values.push_back(frame);
            payload.things.push_back(entry);
            const std::optional<sed::TVector> target = mode == 0 ? std::nullopt
                : std::optional<sed::TVector>{sed::TVector{mode == 1 ? 0.0 : mode == 2 ? 0.25 : 0.5,0,0}};
            const auto pasted = sed::level::pasteEntityClipboard(destination,sed::level::EntityClipboard{payload},target);
            check(pasted && pasted->kind == sed::level::EntityClipboardKind::things
                    && pasted->indices == std::vector<std::int32_t>{1} && pasted->addedLayers
                    && destination.things.size() == 2 && destination.curId == 43
                    && destination.layers == std::vector<std::string>{"Existing","Imported"},
                "Clipboard.Thing no-op and translated paste publish complete fixed result indices layers and identity");
            if (!pasted || destination.things.size() != 2) continue;
            const auto& actual = destination.things[1];
            check(actual.x == (mode == 2 ? maximum : mode == 3 ? 1.5 : 1.0)
                    && actual.y == 2 && actual.z == 3 && actual.id == 42 && actual.num == 1
                    && actual.layer == 1 && actual.sector == -1
                    && actual.values[0].asString() == (mode == 3 ? "(4.5/5/6:7/8/9)" : frame.stringValue),
                "Clipboard.Thing Paste in Place explicit-zero and rounded no-op preserve exact authored Frame strings");
        }
    }
    {
        sed::Level level{};
        level.kind = sed::ProjectType::IJIM;
        level.layers = { "World", "Detail" };

        sed::Sector a = makeTestCubeSector(0.0);
        sed::Sector b = makeTestCubeSector(3.0);
        sed::Sector external = makeTestCubeSector(20.0);
        for (sed::Sector* const sector : { &a, &b, &external })
        {
            for (sed::Surface& surface : sector->surfaces)
            {
                surface.texVertices.resize(surface.vertices.size());
            }
        }
        a.layer = 1;
        b.layer = 0;
        a.pointLight.color = sed::ColorF{ 1.0, 0.5, 0.25, 1.0 };
        a.pointLight.position = sed::TVector{ 0.25, 0.0, 0.0 };
        a.pointLight.minRange = 4.0;
        a.collideBox = sed::TBox{ -0.75, -0.5, -0.25, 0.75, 0.5, 0.25 };
        a.surfaces[0].adjoin = sed::SurfaceRef{ 1, 1 };
        a.surfaces[0].nAdj = 44;
        b.surfaces[1].adjoin = sed::SurfaceRef{ 0, 0 };
        b.surfaces[1].nAdj = 45;
        // This relationship leaves the copied set and must not survive.
        b.surfaces[2].adjoin = sed::SurfaceRef{ 2, 3 };
        external.surfaces[3].adjoin = sed::SurfaceRef{ 1, 2 };
        sed::level::surfaceSetAdjoinParams(level.kind, a.surfaces[0]);
        sed::level::surfaceSetAdjoinParams(level.kind, b.surfaces[1]);
        sed::level::surfaceSetAdjoinParams(level.kind, b.surfaces[2]);
        sed::level::surfaceSetAdjoinParams(level.kind, external.surfaces[3]);
        a.surfaces[0].material = "portal.mat";
        a.surfaces[0].uScale = 1.75f;
        a.surfaces[0].texVertices.resize(a.surfaces[0].vertices.size());
        a.surfaces[0].texVertices[0].u = 12.345678f;
        a.surfaces[0].texVertices[0].v = -4.25f;

        level.sectors.push_back(std::move(a));
        level.sectors.push_back(std::move(b));
        level.sectors.push_back(std::move(external));
        for (std::size_t i = 0; i < level.sectors.size(); ++i)
        {
            level.sectors[i].id = level.newId();
            level.sectors[i].num = static_cast<std::int32_t>(i);
            for (std::size_t sf = 0; sf < level.sectors[i].surfaces.size(); ++sf)
            {
                level.sectors[i].surfaces[sf].num = static_cast<std::int32_t>(sf);
            }
        }

        const std::array<std::int32_t, 4> selection{ 1, 0, 1, -1 };
        const std::optional<sed::level::EntityClipboard> clipboard =
            sed::level::copySectorsToClipboard(level, selection, 1);
        check(clipboard.has_value(), "Clipboard.sectors.copy: creates payload");
        if (!clipboard)
        {
            return;
        }

        check(
            sed::level::entityClipboardKind(*clipboard) == sed::level::EntityClipboardKind::sectors,
            "Clipboard.sectors.copy: typed kind");
        check(sed::level::entityClipboardCount(*clipboard) == 2, "Clipboard.sectors.copy: sorted unique count");

        const auto* sectorPayload = std::get_if<sed::level::SectorClipboardPayload>(&*clipboard);
        check(sectorPayload != nullptr, "Clipboard.sectors.copy: sector payload");
        if (sectorPayload)
        {
            check(sectorPayload->uvCoordinateSpace
                    == sed::UvCoordinateSpace::Normalized,
                "Clipboard.sectors.copy: retains source UV coordinate space");
            check(
                sectorPayload->sectors[0].surfaces[0].adjoin == sed::SurfaceRef{ 1, 1 },
                "Clipboard.sectors.copy: forward internal adjoin is local");
            check(
                sectorPayload->sectors[1].surfaces[1].adjoin == sed::SurfaceRef{ 0, 0 },
                "Clipboard.sectors.copy: reverse internal adjoin is local");
            check(
                !sectorPayload->sectors[1].surfaces[2].adjoin,
                "Clipboard.sectors.copy: external adjoin is severed");
            check(sectorPayload->sectors[1].surfaces[2].geo == 4
                    && (sectorPayload->sectors[1].surfaces[2].surfaceFlags & sed::SF_Collision) != 0,
                "Clipboard.sectors.copy: severed portal becomes a solid boundary");
        }

        std::string encodeError{};
        const std::optional<std::vector<std::byte>> encoded =
            sed::level::encodeEntityClipboard(*clipboard, &encodeError);
        std::string decodeError{};
        const std::optional<sed::level::EntityClipboard> decoded = encoded
            ? sed::level::decodeEntityClipboard(*encoded, &decodeError)
            : std::nullopt;
        const auto* decodedSectors = decoded
            ? std::get_if<sed::level::SectorClipboardPayload>(&*decoded)
            : nullptr;
        check(encoded && encodeError.empty() && decoded && decodeError.empty()
                && decodedSectors
                && decodedSectors->sourceProject == sed::ProjectType::IJIM
                && decodedSectors->uvCoordinateSpace
                    == sed::UvCoordinateSpace::Normalized
                && decodedSectors->layers.size() == 2
                && decodedSectors->layers[0].name == "Detail"
                && decodedSectors->layers[1].name == "World"
                && closeVec(decodedSectors->anchor,
                    sed::level::sectorCalcCenter(level.sectors[1]))
                && decodedSectors->sectors[0].surfaces[0].adjoin
                    == sed::SurfaceRef{ 1, 1 }
                && decodedSectors->sectors[0].surfaces[0].material
                    == "portal.mat"
                && close(
                    decodedSectors->sectors[0].surfaces[0]
                        .texVertices[0].u,
                    12.345678),
            "Clipboard.codec: protected cross-process payload preserves project, layers, anchor, adjoins, materials, and UVs");

        std::vector<std::byte> truncated = encoded.value_or(
            std::vector<std::byte>{});
        if (!truncated.empty())
        {
            truncated.resize(truncated.size() / 2);
        }
        std::string malformedError{};
        check(!sed::level::decodeEntityClipboard(
                    truncated, &malformedError)
                && !malformedError.empty(),
            "Clipboard.codec: truncated external payload is rejected with a diagnostic");

        const std::optional<sed::level::EntityPasteResult> pasted =
            sed::level::pasteEntityClipboard(level, *clipboard, sed::TVector{ 10.0, 0.0, 0.0 });
        check(pasted.has_value(), "Clipboard.sectors.paste: succeeds");
        if (!pasted)
        {
            return;
        }

        check(pasted->indices == std::vector<std::int32_t>({ 3, 4 }), "Clipboard.sectors.paste: appended indices");
        check(!pasted->addedLayers, "Clipboard.sectors.paste: reuses named layers");
        check(level.sectors[3].layer == 1 && level.sectors[4].layer == 0, "Clipboard.sectors.paste: preserves layers");
        check(level.sectors[3].num == 3 && level.sectors[4].num == 4, "Clipboard.sectors.paste: fresh numbers");
        check(level.sectors[3].id != level.sectors[0].id, "Clipboard.sectors.paste: fresh ids");
        check(
            level.sectors[3].surfaces[0].adjoin == sed::SurfaceRef{ 4, 1 },
            "Clipboard.sectors.paste: forward adjoin remapped");
        check(
            level.sectors[4].surfaces[1].adjoin == sed::SurfaceRef{ 3, 0 },
            "Clipboard.sectors.paste: reverse adjoin remapped");
        check(!level.sectors[4].surfaces[2].adjoin, "Clipboard.sectors.paste: external adjoin remains absent");
        check(level.sectors[3].surfaces[0].nAdj == -1, "Clipboard.sectors.paste: derived adjoin id reset");
        check(level.sectors[3].surfaces[0].material == "portal.mat"
                && close(level.sectors[3].surfaces[0].uScale, 1.75)
                && close(level.sectors[3].surfaces[0].texVertices[0].u, 12.345678)
                && close(level.sectors[3].surfaces[0].texVertices[0].v, -4.25),
            "Clipboard.sectors.paste: material and exact authored UV data survive translation");

        // Primary source B is centered at x=3, so the group translated by +7.
        check(close(level.sectors[3].vertices[0].x, 6.0), "Clipboard.sectors.paste: preserves group offset");
        check(close(level.sectors[4].vertices[0].x, 9.0), "Clipboard.sectors.paste: primary reaches target");
        check(
            closeVec(level.sectors[3].pointLight.position, sed::TVector{ 7.25, 0.0, 0.0 }),
            "Clipboard.sectors.paste: owned point light translates");
        check(close(level.sectors[3].collideBox.x1, 6.25)
                && close(level.sectors[3].collideBox.x2, 7.75),
            "Clipboard.sectors.paste: authored collision bounds translate");
    }

    {
        sed::Level source{};
        source.uvCoordinateSpace = sed::UvCoordinateSpace::TexturePixels;
        source.layers = { "Legacy" };
        sed::Sector legacySector = makeTestCubeSector(0.0);
        legacySector.layer = 0;
        for (sed::Surface& surface : legacySector.surfaces)
        {
            surface.texVertices.resize(surface.vertices.size());
        }
        legacySector.surfaces[0].material = "anisotropic.mat";
        legacySector.surfaces[0].uScale = 2.0f;
        legacySector.surfaces[0].vScale = 3.0f;
        legacySector.surfaces[0].texVertices.resize(
            legacySector.surfaces[0].vertices.size());
        legacySector.surfaces[0].texVertices[0].u = 64.0f;
        legacySector.surfaces[0].texVertices[0].v = -16.0f;
        source.sectors.push_back(std::move(legacySector));

        const std::array<std::int32_t, 1> selection{ 0 };
        const std::optional<sed::level::EntityClipboard> clipboard =
            sed::level::copySectorsToClipboard(source, selection, 0);
        const auto* payload = clipboard
            ? std::get_if<sed::level::SectorClipboardPayload>(&*clipboard)
            : nullptr;
        check(payload && payload->uvCoordinateSpace
                    == sed::UvCoordinateSpace::TexturePixels,
            "Clipboard.UV conversion: pixel-space source tag is captured");

        std::string encodeError{};
        const std::optional<std::vector<std::byte>> encoded = clipboard
            ? sed::level::encodeEntityClipboard(*clipboard, &encodeError)
            : std::nullopt;
        std::string decodeError{};
        const std::optional<sed::level::EntityClipboard> decoded = encoded
            ? sed::level::decodeEntityClipboard(*encoded, &decodeError)
            : std::nullopt;
        const auto* decodedPayload = decoded
            ? std::get_if<sed::level::SectorClipboardPayload>(&*decoded)
            : nullptr;
        check(encoded && encodeError.empty() && decodedPayload
                && decodeError.empty()
                && decodedPayload->uvCoordinateSpace
                    == sed::UvCoordinateSpace::TexturePixels,
            "Clipboard.UV conversion: cross-process codec preserves the legacy UV-space tag");

        sed::Level destination{};
        destination.uvCoordinateSpace = sed::UvCoordinateSpace::Normalized;
        destination.layers = { "Target" };
        const std::optional<sed::level::EntityPasteResult> pasted = decoded
            ? sed::level::pasteEntityClipboard(
                destination,
                *decoded,
                std::nullopt,
                [](const sed::Surface&)
                    -> std::optional<sed::level::UvTextureDimensions>
                {
                    return sed::level::UvTextureDimensions{ 128.0, 32.0 };
                })
            : std::nullopt;
        check(pasted && destination.sectors.size() == 1u
                && destination.uvCoordinateSpace
                    == sed::UvCoordinateSpace::Normalized
                && close(destination.sectors[0].surfaces[0]
                        .texVertices[0].u,
                    0.5)
                && close(destination.sectors[0].surfaces[0]
                        .texVertices[0].v,
                    -0.5)
                && close(destination.sectors[0].surfaces[0].uScale,
                    2.0 / 128.0)
                && close(destination.sectors[0].surfaces[0].vScale,
                    3.0 / 32.0),
            "Clipboard.UV conversion: pixel sectors normalize through anisotropic MAT dimensions before paste");

        sed::Level unresolvedDestination{};
        unresolvedDestination.uvCoordinateSpace =
            sed::UvCoordinateSpace::Normalized;
        unresolvedDestination.layers = { "Target" };
        const std::optional<sed::level::EntityPasteResult> unresolved = decoded
            ? sed::level::pasteEntityClipboard(
                unresolvedDestination,
                *decoded,
                std::nullopt,
                {})
            : std::nullopt;
        check(!unresolved && unresolvedDestination.sectors.empty()
                && unresolvedDestination.layers
                    == std::vector<std::string>{ "Target" }
                && unresolvedDestination.uvCoordinateSpace
                    == sed::UvCoordinateSpace::Normalized,
            "Clipboard.UV conversion: missing MAT dimensions reject a cross-space paste atomically");
    }

    {
        enum class MalformedUvKind
        {
            missingCorner,
            excessCorner,
            nonFiniteCoordinate,
            nonFiniteScale,
        };
        struct MalformedUvCase
        {
            const char* name{};
            MalformedUvKind kind{};
        };
        const std::array malformedCases{
            MalformedUvCase{
                "missing corner coordinate",
                MalformedUvKind::missingCorner },
            MalformedUvCase{
                "excess corner coordinate",
                MalformedUvKind::excessCorner },
            MalformedUvCase{
                "non-finite coordinate",
                MalformedUvKind::nonFiniteCoordinate },
            MalformedUvCase{
                "non-finite scale",
                MalformedUvKind::nonFiniteScale },
        };

        for (const MalformedUvCase& testCase : malformedCases)
        {
            sed::Level source{};
            source.uvCoordinateSpace = sed::UvCoordinateSpace::Normalized;
            source.layers = { "Source" };
            sed::Sector sector = makeTestCubeSector(0.0);
            for (sed::Surface& surface : sector.surfaces)
            {
                surface.texVertices.resize(surface.vertices.size());
            }
            source.sectors.push_back(std::move(sector));

            const std::array<std::int32_t, 1> selection{ 0 };
            std::optional<sed::level::EntityClipboard> clipboard =
                sed::level::copySectorsToClipboard(
                    source, selection, 0);
            auto* payload = clipboard
                ? std::get_if<sed::level::SectorClipboardPayload>(
                    &*clipboard)
                : nullptr;
            if (!payload)
            {
                check(false,
                    "Clipboard malformed UV fixture creates a Sector payload");
                continue;
            }

            sed::Surface& malformed = payload->sectors[0].surfaces[0];
            switch (testCase.kind)
            {
                case MalformedUvKind::missingCorner:
                    malformed.texVertices.pop_back();
                    break;
                case MalformedUvKind::excessCorner:
                    malformed.texVertices.push_back(sed::TexVertex{
                        7.0f, -9.0f, sed::ColorRed });
                    break;
                case MalformedUvKind::nonFiniteCoordinate:
                    malformed.texVertices[0].u =
                        std::numeric_limits<float>::quiet_NaN();
                    break;
                case MalformedUvKind::nonFiniteScale:
                    malformed.vScale =
                        std::numeric_limits<float>::infinity();
                    break;
            }

            sed::Level destination{};
            destination.uvCoordinateSpace =
                sed::UvCoordinateSpace::Normalized;
            destination.layers = { "Target" };
            const std::optional<sed::level::EntityPasteResult> pasted =
                sed::level::pasteEntityClipboard(
                    destination, *clipboard);
            const std::string message =
                "Clipboard same-space paste rejects "
                + std::string(testCase.name)
                + " atomically";
            check(!pasted
                    && destination.sectors.empty()
                    && destination.layers
                        == std::vector<std::string>{ "Target" }
                    && destination.uvCoordinateSpace
                        == sed::UvCoordinateSpace::Normalized,
                message.c_str());
        }
    }

    {
        sed::Level level{};
        level.layers = { "Gameplay" };
        sed::Sector room = makeTestCubeSector(0.0);
        room.id = level.newId();
        room.num = 17;
        level.sectors.push_back(std::move(room));

        sed::Thing& first = level.newThing();
        first.name = "first";
        first.x = -0.5;
        first.y = 0.0;
        first.z = 0.0;
        first.sector = 0;
        first.layer = 0;
        first.values.push_back(makeFrame(-0.25, 0.0, 0.0, 10.0, 20.0, 30.0));

        sed::Thing& second = level.newThing();
        second.name = "second";
        second.x = 0.5;
        second.y = 0.0;
        second.z = 0.0;
        second.sector = 0;
        second.layer = 0;
        const std::int32_t firstSourceId = level.things[0].id;

        const std::array<std::int32_t, 3> selection{ 1, 0, 1 };
        const std::optional<sed::level::EntityClipboard> clipboard =
            sed::level::copyThingsToClipboard(level, selection, 1);
        check(clipboard.has_value(), "Clipboard.things.copy: creates payload");
        if (!clipboard)
        {
            return;
        }

        std::string thingEncodeError{};
        const std::optional<std::vector<std::byte>> encodedThings =
            sed::level::encodeEntityClipboard(
                *clipboard, &thingEncodeError);
        std::string thingDecodeError{};
        const std::optional<sed::level::EntityClipboard> decodedThings =
            encodedThings
                ? sed::level::decodeEntityClipboard(
                    *encodedThings, &thingDecodeError)
                : std::nullopt;
        const auto* decodedThingPayload = decodedThings
            ? std::get_if<sed::level::ThingClipboardPayload>(
                &*decodedThings)
            : nullptr;
        check(encodedThings && thingEncodeError.empty()
                && decodedThingPayload && thingDecodeError.empty()
                && decodedThingPayload->things.size() == 2
                && decodedThingPayload->things[0].thing.name == "first"
                && decodedThingPayload->things[0].layer.name == "Gameplay"
                && decodedThingPayload->things[0].thing.values.size() == 1,
            "Clipboard.codec: cross-process Thing payload preserves order, layer, and frame values");

        const std::optional<sed::level::EntityPasteResult> pasted =
            sed::level::pasteEntityClipboard(level, *clipboard, sed::TVector{ 0.25, 0.0, 0.0 });
        check(pasted.has_value(), "Clipboard.things.paste: succeeds");
        if (!pasted)
        {
            return;
        }

        check(pasted->indices == std::vector<std::int32_t>({ 2, 3 }), "Clipboard.things.paste: appended indices");
        const sed::Thing& pastedFirst = level.things[2];
        const sed::Thing& pastedSecond = level.things[3];
        check(close(pastedFirst.x, -0.75), "Clipboard.things.paste: relative offset");
        check(close(pastedSecond.x, 0.25), "Clipboard.things.paste: primary reaches target");
        check(pastedFirst.id != firstSourceId && pastedFirst.num == 2, "Clipboard.things.paste: fresh identity");
        check(pastedFirst.layer == 0 && pastedSecond.layer == 0, "Clipboard.things.paste: preserves layer");
        check(pastedFirst.sector == 0 && pastedSecond.sector == 0, "Clipboard.things.paste: recomputes owner Sector");

        double x{};
        double y{};
        double z{};
        double pitch{};
        double yaw{};
        double roll{};
        const bool parsed = pastedFirst.values[0].tryGetFrame(x, y, z, pitch, yaw, roll);
        check(parsed, "Clipboard.things.paste: frame remains parseable");
        check(close(x, -0.5), "Clipboard.things.paste: frame translates with Thing");
        check(close(pitch, 10.0) && close(yaw, 20.0) && close(roll, 30.0),
            "Clipboard.things.paste: frame orientation preserved");

        // Do not blindly restore a copied owner when containment proves that
        // the authored position is outside that Sector. The checker must be
        // able to report the resulting missing owner instead of receiving a
        // plausible-looking but geometrically false Sector index.
        level.things[0].x = 50.0;
        level.things[0].sector = 0;
        const std::array<std::int32_t, 1> outsideSelection{ 0 };
        const std::optional<sed::level::EntityClipboard> outsideClipboard =
            sed::level::copyThingsToClipboard(level, outsideSelection, 0);
        const std::optional<sed::level::EntityPasteResult> outsidePaste = outsideClipboard
            ? sed::level::pasteEntityClipboard(level, *outsideClipboard)
            : std::nullopt;
        check(outsidePaste && level.things.back().sector == -1,
            "Clipboard.things.paste: invalid copied owner is not restored after containment fails");
    }

    {
        sed::Level source{};
        source.layers = { "Lights" };
        sed::SedLight& a = source.newLight();
        a.layer = 0;
        a.position = sed::TVector{ 1.0, 2.0, 3.0 };
        a.color = sed::ColorF{ 0.2, 0.3, 0.4, 1.0 };
        a.intensity = 5.0;
        sed::SedLight& b = source.newLight();
        b.layer = 0;
        b.position = sed::TVector{ 3.0, 2.0, 3.0 };
        b.range = 8.0;

        const std::array<std::int32_t, 2> selection{ 0, 1 };
        const std::optional<sed::level::EntityClipboard> clipboard =
            sed::level::copyLightsToClipboard(source, selection, 1);
        check(clipboard.has_value(), "Clipboard.lights.copy: creates payload");
        if (!clipboard)
        {
            return;
        }

        std::string lightEncodeError{};
        const std::optional<std::vector<std::byte>> encodedLights =
            sed::level::encodeEntityClipboard(
                *clipboard, &lightEncodeError);
        std::string lightDecodeError{};
        const std::optional<sed::level::EntityClipboard> decodedLights =
            encodedLights
                ? sed::level::decodeEntityClipboard(
                    *encodedLights, &lightDecodeError)
                : std::nullopt;
        const auto* decodedLightPayload = decodedLights
            ? std::get_if<sed::level::LightClipboardPayload>(
                &*decodedLights)
            : nullptr;
        check(encodedLights && lightEncodeError.empty()
                && decodedLightPayload && lightDecodeError.empty()
                && decodedLightPayload->lights.size() == 2
                && decodedLightPayload->lights[0].layer.name == "Lights"
                && close(decodedLightPayload->lights[0].light.intensity, 5.0)
                && closeVec(decodedLightPayload->lights[1].light.position,
                    sed::TVector{ 3.0, 2.0, 3.0 }),
            "Clipboard.codec: cross-process baked-light payload preserves layer, order, position, and parameters");

        sed::Level destination{};
        destination.layers = { "World" };
        check(
            sed::level::entityClipboardRequiresLayerImport(destination, *clipboard),
            "Clipboard.lights.paste: detects cross-document layer import");
        const std::optional<sed::level::EntityPasteResult> pasted =
            sed::level::pasteEntityClipboard(destination, *clipboard, sed::TVector{ 10.0, 4.0, 5.0 });
        check(pasted.has_value(), "Clipboard.lights.paste: succeeds");
        if (!pasted)
        {
            return;
        }

        check(pasted->addedLayers, "Clipboard.lights.paste: imports missing named layer");
        check(destination.layers.size() == 2 && destination.layers[1] == "Lights",
            "Clipboard.lights.paste: named layer appended");
        check(destination.lights.size() == 2, "Clipboard.lights.paste: count");
        check(closeVec(destination.lights[0].position, sed::TVector{ 8.0, 4.0, 5.0 }),
            "Clipboard.lights.paste: preserves relative position");
        check(closeVec(destination.lights[1].position, sed::TVector{ 10.0, 4.0, 5.0 }),
            "Clipboard.lights.paste: primary reaches target");
        check(destination.lights[0].layer == 1 && destination.lights[1].layer == 1,
            "Clipboard.lights.paste: remaps layer by name");
        check(close(destination.lights[0].color.r, 0.2) && close(destination.lights[0].intensity, 5.0),
            "Clipboard.lights.paste: preserves authored values");
    }

    {
        sed::Level level{};
        const std::array<std::int32_t, 2> invalid{ -1, 99 };
        check(
            !sed::level::copySectorsToClipboard(level, invalid).has_value(),
            "Clipboard.invalidCopy: no empty payload");
        const sed::level::EntityClipboard empty{};
        check(
            !sed::level::pasteEntityClipboard(level, empty).has_value(),
            "Clipboard.emptyPaste: no mutation");
        check(level.sectors.empty() && level.things.empty() && level.lights.empty(),
            "Clipboard.emptyPaste: level unchanged");
    }
}

} // namespace sed::tests
