#include "test_common.hpp"

#include "sed/level/arch_lighting_clipboard.hpp"
#include "sed/level/thing.hpp"

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace sed::tests
{

namespace
{

constexpr std::int32_t kArchFlag = 0x00000008;

using sed::level::MotsArchLightingClipboardPayload;
using sed::level::MotsArchLightingClipboardResult;
using sed::level::MotsArchLightingClipboardStatus;
using sed::level::MotsArchLightingClipboardTarget;
using sed::level::MotsArchLightingTopology;
using sed::level::copyMotsArchLightingClipboard;
using sed::level::decodeMotsArchLightingClipboard;
using sed::level::encodeMotsArchLightingClipboard;
using sed::level::pasteMotsArchLightingClipboard;

bool sameBits(const float left, const float right) noexcept
{
    return std::bit_cast<std::uint32_t>(left)
        == std::bit_cast<std::uint32_t>(right);
}

bool sameVertexBits(
    const MotsArchLightVertex& left,
    const MotsArchLightVertex& right) noexcept
{
    return sameBits(left.mono, right.mono)
        && sameBits(left.red, right.red)
        && sameBits(left.green, right.green)
        && sameBits(left.blue, right.blue);
}

bool sameObjectBits(
    const MotsArchLightObject& left,
    const MotsArchLightObject& right) noexcept
{
    if (left.wireIndex != right.wireIndex
        || left.meshes.size() != right.meshes.size())
    {
        return false;
    }
    for (std::size_t meshIndex = 0; meshIndex < left.meshes.size(); ++meshIndex)
    {
        const MotsArchLightMesh& leftMesh = left.meshes[meshIndex];
        const MotsArchLightMesh& rightMesh = right.meshes[meshIndex];
        if (leftMesh.wireIndex != rightMesh.wireIndex
            || leftMesh.vertices.size() != rightMesh.vertices.size())
        {
            return false;
        }
        for (std::size_t vertexIndex = 0;
             vertexIndex < leftMesh.vertices.size();
             ++vertexIndex)
        {
            if (!sameVertexBits(
                    leftMesh.vertices[vertexIndex],
                    rightMesh.vertices[vertexIndex]))
            {
                return false;
            }
        }
    }
    return true;
}

bool sameObjectValues(
    const MotsArchLightObject& left,
    const MotsArchLightObject& right) noexcept
{
    if (left.meshes.size() != right.meshes.size())
    {
        return false;
    }
    for (std::size_t meshIndex = 0; meshIndex < left.meshes.size(); ++meshIndex)
    {
        const MotsArchLightMesh& leftMesh = left.meshes[meshIndex];
        const MotsArchLightMesh& rightMesh = right.meshes[meshIndex];
        if (leftMesh.wireIndex != rightMesh.wireIndex
            || leftMesh.vertices.size() != rightMesh.vertices.size())
        {
            return false;
        }
        for (std::size_t vertexIndex = 0;
             vertexIndex < leftMesh.vertices.size();
             ++vertexIndex)
        {
            if (!sameVertexBits(
                    leftMesh.vertices[vertexIndex],
                    rightMesh.vertices[vertexIndex]))
            {
                return false;
            }
        }
    }
    return true;
}

MotsArchLightObject makeObject(
    const std::int32_t wireIndex,
    const float red = -2.5f)
{
    MotsArchLightObject object{};
    object.wireIndex = wireIndex;
    MotsArchLightMesh mesh{};
    mesh.wireIndex = 17;
    mesh.vertices.push_back(MotsArchLightVertex{
        -0.0f,
        red,
        std::numeric_limits<float>::max(),
        0.75f });
    object.meshes.push_back(std::move(mesh));
    return object;
}

MotsArchLightingTopology makeTopology()
{
    MotsArchLightingTopology topology{};
    topology.geosets = { { 1u }, { 1u }, { 1u }, { 1u } };
    return topology;
}

void markArchThing(Thing& thing)
{
    TplValue flags{};
    flags.name = "thingflags";
    flags.type = ValueType::integer;
    flags.intValue = kArchFlag;
    thing.values.push_back(std::move(flags));
}

void markModel(Thing& thing)
{
    TplValue model{};
    model.name = "model3d";
    model.type = ValueType::str;
    model.stringValue = "models/door.3do";
    thing.values.push_back(std::move(model));
}

Level makeLevel()
{
    Level level{};
    level.kind = ProjectType::MOTS;
    level.things.resize(3);
    markArchThing(level.things[0]);
    markArchThing(level.things[1]);
    markArchThing(level.things[2]);
    level.things[0].archLightingIndex = 0;
    level.things[2].archLightingIndex = 1;
    level.motsArchLighting.push_back(makeObject(17));
    level.motsArchLighting.push_back(makeObject(99, 3.5f));
    return level;
}

MotsArchLightingClipboardTarget makeTarget(
    const std::int32_t thingIndex,
    const std::string_view identity,
    const MotsArchLightingTopology& topology)
{
    return MotsArchLightingClipboardTarget{
        thingIndex,
        std::string(identity),
        topology };
}

void setU32At(
    std::vector<std::byte>& bytes,
    const std::size_t offset,
    const std::uint32_t value)
{
    bytes[offset + 0] = static_cast<std::byte>(value & 0xffu);
    bytes[offset + 1] = static_cast<std::byte>((value >> 8u) & 0xffu);
    bytes[offset + 2] = static_cast<std::byte>((value >> 16u) & 0xffu);
    bytes[offset + 3] = static_cast<std::byte>((value >> 24u) & 0xffu);
}

void setLastU32(std::vector<std::byte>& bytes, const std::uint32_t value)
{
    setU32At(bytes, bytes.size() - sizeof(value), value);
}

} // namespace

void runMotsArchLightingClipboardTests()
{
    const MotsArchLightingTopology topology = makeTopology();
    Level source = makeLevel();
    std::string error = "stale";
    const auto payload = copyMotsArchLightingClipboard(
        source,
        0,
        " Models\\Door.3DO ",
        topology,
        nullptr,
        &error);
    check(payload.has_value(),
        "ARCHLIGHTING clipboard copy snapshots a bound MotS Arch Thing record");
    check(error.empty(),
        "ARCHLIGHTING clipboard copy clears a successful error output");
    if (!payload)
    {
        return;
    }
    check(payload->modelIdentity == "models/door.3do",
        "ARCHLIGHTING clipboard copy canonicalizes model identity name, case, and slashes");
    check(payload->topology.geosets == topology.geosets,
        "ARCHLIGHTING clipboard copy owns the complete all-LOD topology");
    check(sameObjectBits(payload->object, source.motsArchLighting[0]),
        "ARCHLIGHTING clipboard copy preserves authored float bits");

    const MotsArchLightObject copiedObject = payload->object;
    source.motsArchLighting[0].meshes[0].vertices[0].red = 42.0f;
    check(sameObjectBits(payload->object, copiedObject),
        "ARCHLIGHTING clipboard payload has no source-record dependency");

    error = "stale";
    const auto encoded = encodeMotsArchLightingClipboard(*payload, &error);
    check(encoded.has_value() && !encoded->empty(),
        "ARCHLIGHTING clipboard codec encodes the bounded value payload");
    check(error.empty(),
        "ARCHLIGHTING clipboard codec clears a successful error output");
    if (encoded)
    {
        const std::vector<std::byte> expectedBytes{
            std::byte{ 0x53 }, std::byte{ 0x45 }, std::byte{ 0x44 }, std::byte{ 0x41 },
            std::byte{ 0x52 }, std::byte{ 0x43 }, std::byte{ 0x48 }, std::byte{ 0x00 },
            std::byte{ 0x01 }, std::byte{ 0x00 }, std::byte{ 0x00 }, std::byte{ 0x00 },
            std::byte{ 0x0f }, std::byte{ 0x00 }, std::byte{ 0x00 }, std::byte{ 0x00 },
            std::byte{ 0x6d }, std::byte{ 0x6f }, std::byte{ 0x64 }, std::byte{ 0x65 },
            std::byte{ 0x6c }, std::byte{ 0x73 }, std::byte{ 0x2f }, std::byte{ 0x64 },
            std::byte{ 0x6f }, std::byte{ 0x6f }, std::byte{ 0x72 }, std::byte{ 0x2e },
            std::byte{ 0x33 }, std::byte{ 0x64 }, std::byte{ 0x6f },
            std::byte{ 0x04 }, std::byte{ 0x00 }, std::byte{ 0x00 }, std::byte{ 0x00 },
            std::byte{ 0x01 }, std::byte{ 0x00 }, std::byte{ 0x00 }, std::byte{ 0x00 },
            std::byte{ 0x01 }, std::byte{ 0x00 }, std::byte{ 0x00 }, std::byte{ 0x00 },
            std::byte{ 0x01 }, std::byte{ 0x00 }, std::byte{ 0x00 }, std::byte{ 0x00 },
            std::byte{ 0x01 }, std::byte{ 0x00 }, std::byte{ 0x00 }, std::byte{ 0x00 },
            std::byte{ 0x01 }, std::byte{ 0x00 }, std::byte{ 0x00 }, std::byte{ 0x00 },
            std::byte{ 0x01 }, std::byte{ 0x00 }, std::byte{ 0x00 }, std::byte{ 0x00 },
            std::byte{ 0x01 }, std::byte{ 0x00 }, std::byte{ 0x00 }, std::byte{ 0x00 },
            std::byte{ 0x01 }, std::byte{ 0x00 }, std::byte{ 0x00 }, std::byte{ 0x00 },
            std::byte{ 0x11 }, std::byte{ 0x00 }, std::byte{ 0x00 }, std::byte{ 0x00 },
            std::byte{ 0x01 }, std::byte{ 0x00 }, std::byte{ 0x00 }, std::byte{ 0x00 },
            std::byte{ 0x11 }, std::byte{ 0x00 }, std::byte{ 0x00 }, std::byte{ 0x00 },
            std::byte{ 0x01 }, std::byte{ 0x00 }, std::byte{ 0x00 }, std::byte{ 0x00 },
            std::byte{ 0x00 }, std::byte{ 0x00 }, std::byte{ 0x00 }, std::byte{ 0x80 },
            std::byte{ 0x00 }, std::byte{ 0x00 }, std::byte{ 0x20 }, std::byte{ 0xc0 },
            std::byte{ 0xff }, std::byte{ 0xff }, std::byte{ 0x7f }, std::byte{ 0x7f },
            std::byte{ 0x00 }, std::byte{ 0x00 }, std::byte{ 0x40 }, std::byte{ 0x3f } };
        check(*encoded == expectedBytes,
            "ARCHLIGHTING clipboard codec matches the fixed version-1 byte vector");

        error = "stale";
        const auto decoded = decodeMotsArchLightingClipboard(*encoded, &error);
        check(decoded.has_value()
                && decoded->modelIdentity == payload->modelIdentity
                && decoded->topology.geosets == payload->topology.geosets
                && sameObjectBits(decoded->object, payload->object),
            "ARCHLIGHTING clipboard codec round-trips identity, topology, and exact float bits");
        check(error.empty(),
            "ARCHLIGHTING clipboard decode clears a successful error output");

        auto truncated = *encoded;
        truncated.pop_back();
        error.clear();
        check(!decodeMotsArchLightingClipboard(truncated, &error)
                && error == "MotS ARCHLIGHTING clipboard payload is truncated.",
            "ARCHLIGHTING clipboard codec rejects truncated payloads");

        auto trailing = *encoded;
        trailing.push_back(std::byte{ 0x7f });
        error.clear();
        check(!decodeMotsArchLightingClipboard(trailing, &error)
                && error == "MotS ARCHLIGHTING clipboard payload has trailing bytes.",
            "ARCHLIGHTING clipboard codec rejects trailing payload bytes");

        auto nonFinite = *encoded;
        setLastU32(nonFinite, 0x7fc00000u);
        error.clear();
        check(!decodeMotsArchLightingClipboard(nonFinite, &error)
                && error == "MotS ARCHLIGHTING clipboard channels must be finite.",
            "ARCHLIGHTING clipboard codec rejects non-finite channel bits");

        auto hugeGeosetCount = *encoded;
        setU32At(hugeGeosetCount, 31u, 0xffffffffu);
        error.clear();
        check(!decodeMotsArchLightingClipboard(hugeGeosetCount, &error)
                && error
                    == "MotS ARCHLIGHTING clipboard topology is invalid or incompatible.",
            "ARCHLIGHTING clipboard codec rejects a huge geoset count before allocation");

        auto hugeMeshCount = *encoded;
        setU32At(hugeMeshCount, 35u, 0xffffffffu);
        error.clear();
        check(!decodeMotsArchLightingClipboard(hugeMeshCount, &error)
                && error
                    == "MotS ARCHLIGHTING clipboard topology is invalid or incompatible.",
            "ARCHLIGHTING clipboard codec rejects a huge mesh count before allocation");

        auto hugeVertexCount = *encoded;
        setU32At(hugeVertexCount, 79u, 0xffffffffu);
        error.clear();
        check(!decodeMotsArchLightingClipboard(hugeVertexCount, &error)
                && error == "MotS ARCHLIGHTING clipboard record is invalid.",
            "ARCHLIGHTING clipboard codec rejects a huge vertex count before allocation");
    }

    {
        Level destination = makeLevel();
        destination.things[1].archLightingIndex = -1;
        const std::array<MotsArchLightingClipboardTarget, 2> targets{
            makeTarget(1, "models/door.3do", topology),
            makeTarget(2, "MODELS\\DOOR.3DO", topology) };
        error = "stale";
        const MotsArchLightingClipboardResult result =
            pasteMotsArchLightingClipboard(
                destination, *payload, targets, nullptr, &error);
        check(result.status == MotsArchLightingClipboardStatus::Applied
                && result.message
                    == "Pasted MotS ARCHLIGHTING to selected Things."
                && result.changedThingIndices
                    == std::vector<std::int32_t>{ 1, 2 }
                && result.recordIndex == 0
                && destination.things[1].archLightingIndex == 0
                && destination.things[2].archLightingIndex == 0
                && destination.motsArchLighting.size() == 2
                && error.empty(),
            "ARCHLIGHTING clipboard paste reuses an identical existing record for bound and unbound targets");
    }

    {
        Level destination = makeLevel();
        destination.things[1].values.clear();
        markModel(destination.things[1]);
        destination.things[1].archLightingIndex = -1;
        const std::array<MotsArchLightingClipboardTarget, 1> targets{
            makeTarget(1, "models/door.3do", topology) };
        error = "stale";
        const MotsArchLightingClipboardResult result =
            pasteMotsArchLightingClipboard(
                destination, *payload, targets, nullptr, &error);
        check(result.status == MotsArchLightingClipboardStatus::Applied
                && result.message
                    == "Pasted MotS ARCHLIGHTING to selected Things."
                && result.changedThingIndices
                    == std::vector<std::int32_t>{ 1 }
                && result.recordIndex == 0
                && destination.things[1].archLightingIndex == 0
                && sed::level::thingFlags(
                    destination, nullptr, destination.things[1])
                    == kArchFlag
                && destination.motsArchLighting.size() == 2
                && error.empty(),
            "ARCHLIGHTING clipboard Paste promotes an eligible unbound model Thing by setting the Arch flag atomically");
    }

    {
        Level destination = makeLevel();
        destination.things[1].values.clear();
        markModel(destination.things[1]);
        destination.things[1].archLightingIndex = 0;
        const std::array<MotsArchLightingClipboardTarget, 1> targets{
            makeTarget(1, "models/door.3do", topology) };
        error = "stale";
        const MotsArchLightingClipboardResult result =
            pasteMotsArchLightingClipboard(
                destination, *payload, targets, nullptr, &error);
        check(result.status == MotsArchLightingClipboardStatus::Applied
                && result.message
                    == "Pasted MotS ARCHLIGHTING to selected Things."
                && result.changedThingIndices
                    == std::vector<std::int32_t>{ 1 }
                && result.recordIndex == 0
                && destination.things[1].archLightingIndex == 0
                && sed::level::thingFlags(
                    destination, nullptr, destination.things[1])
                    == kArchFlag
                && destination.motsArchLighting.size() == 2
                && error.empty(),
            "ARCHLIGHTING clipboard Paste promotes an already-bound unflagged owner instead of reporting a false no-op");
    }

    {
        Level destination = makeLevel();
        const std::array<MotsArchLightingClipboardTarget, 1> targets{
            makeTarget(0, "models/door.3do", topology) };
        error = "stale";
        const MotsArchLightingClipboardResult result =
            pasteMotsArchLightingClipboard(
                destination, *payload, targets, nullptr, &error);
        check(result.status == MotsArchLightingClipboardStatus::Noop
                && result.message
                    == "No MotS ARCHLIGHTING clipboard changes were necessary."
                && result.changedThingIndices.empty()
                && result.recordIndex == 0
                && error.empty(),
            "ARCHLIGHTING clipboard paste reports an exact no-op result without mutation");
    }

    {
        Level destination = makeLevel();
        destination.things[1].archLightingIndex = -1;
        MotsArchLightingClipboardPayload changed = *payload;
        changed.object.meshes[0].vertices[0].red = 8.5f;
        const std::array<MotsArchLightingClipboardTarget, 2> targets{
            makeTarget(1, "models/door.3do", topology),
            makeTarget(2, "models/door.3do", topology) };
        const MotsArchLightingClipboardResult result =
            pasteMotsArchLightingClipboard(
                destination, changed, targets, nullptr, &error);
        const std::int32_t newIndex = result.recordIndex;
        check(result.status == MotsArchLightingClipboardStatus::Applied
                && result.message
                    == "Pasted MotS ARCHLIGHTING to selected Things."
                && result.changedThingIndices
                    == std::vector<std::int32_t>{ 1, 2 }
                && newIndex == 2
                && destination.things[1].archLightingIndex == newIndex
                && destination.things[2].archLightingIndex == newIndex
                && destination.motsArchLighting.size() == 3
                && destination.motsArchLighting[0].wireIndex == 17
                && destination.motsArchLighting[1].wireIndex == 99
                && destination.motsArchLighting[2].wireIndex != 17
                && destination.motsArchLighting[2].wireIndex != 99
                && sameObjectValues(destination.motsArchLighting[2], changed.object)
                && error.empty(),
            "ARCHLIGHTING clipboard paste appends one uniquely wired record without deleting existing records");
    }

    {
        Level destination = makeLevel();
        destination.things[1].archLightingIndex = -1;
        destination.motsArchLighting[0].wireIndex = -1;
        const std::array<MotsArchLightingClipboardTarget, 1> targets{
            makeTarget(1, "models/door.3do", topology) };
        error = "stale";
        const MotsArchLightingClipboardResult result =
            pasteMotsArchLightingClipboard(
                destination, *payload, targets, nullptr, &error);
        check(result.status == MotsArchLightingClipboardStatus::Applied
                && result.message
                    == "Pasted MotS ARCHLIGHTING to selected Things."
                && result.changedThingIndices
                    == std::vector<std::int32_t>{ 1 }
                && result.recordIndex == 2
                && error.empty()
                && destination.things[1].archLightingIndex == 2
                && destination.motsArchLighting.size() == 3
                && destination.motsArchLighting[0].wireIndex == -1
                && destination.motsArchLighting[2].wireIndex >= 0
                && destination.motsArchLighting[2].wireIndex != 99,
            "ARCHLIGHTING clipboard skips a malformed matching record and preserves it while appending");
    }

    {
        MotsArchLightingClipboardPayload boundary = *payload;
        MotsArchLightObject nearBudget = makeObject(400);
        nearBudget.meshes[0].vertices.resize(
            sed::level::kMotsArchLightingClipboardMaxItems - 1u,
            nearBudget.meshes[0].vertices.front());

        Level destination = makeLevel();
        destination.motsArchLighting.clear();
        destination.motsArchLighting.push_back(std::move(nearBudget));
        destination.things[0].archLightingIndex = 0;
        destination.things[1].archLightingIndex = -1;
        destination.things[2].archLightingIndex = -1;
        const std::array<MotsArchLightingClipboardTarget, 1> targets{
            makeTarget(1, "models/door.3do", topology) };
        error = "stale";
        const MotsArchLightingClipboardResult result =
            pasteMotsArchLightingClipboard(
                destination, boundary, targets, nullptr, &error);
        check(result.status == MotsArchLightingClipboardStatus::Applied
                && result.recordIndex == 1
                && result.changedThingIndices
                    == std::vector<std::int32_t>{ 1 }
                && destination.motsArchLighting.size() == 2
                && destination.things[1].archLightingIndex == 1
                && error.empty(),
            "ARCHLIGHTING clipboard accepts the exact aggregate vertex budget boundary");

        MotsArchLightingClipboardPayload overBudget = boundary;
        overBudget.object.meshes[0].vertices.push_back(
            overBudget.object.meshes[0].vertices.front());
        MotsArchLightingTopology overTopology = topology;
        for (std::vector<std::size_t>& geoset : overTopology.geosets)
        {
            geoset[0] = 2u;
        }
        overBudget.topology = overTopology;
        const std::array<MotsArchLightingClipboardTarget, 1> overTargets{
            makeTarget(1, "models/door.3do", overTopology) };

        Level rejectedDestination = makeLevel();
        rejectedDestination.motsArchLighting.clear();
        MotsArchLightObject rejectedNearBudget = makeObject(400);
        rejectedNearBudget.meshes[0].vertices.resize(
            sed::level::kMotsArchLightingClipboardMaxItems - 1u,
            rejectedNearBudget.meshes[0].vertices.front());
        rejectedDestination.motsArchLighting.push_back(
            std::move(rejectedNearBudget));
        rejectedDestination.things[0].archLightingIndex = 0;
        rejectedDestination.things[1].archLightingIndex = -1;
        rejectedDestination.things[2].archLightingIndex = -1;
        const std::size_t recordsBefore =
            rejectedDestination.motsArchLighting.size();
        error = "stale";
        const MotsArchLightingClipboardResult rejected =
            pasteMotsArchLightingClipboard(
                rejectedDestination,
                overBudget,
                overTargets,
                nullptr,
                &error);
        check(rejected.status == MotsArchLightingClipboardStatus::Rejected
                && rejected.message
                    == "MotS ARCHLIGHTING clipboard exceeds its bounded record budget."
                && rejected.changedThingIndices.empty()
                && rejected.recordIndex == -1
                && error
                    == "MotS ARCHLIGHTING clipboard exceeds its bounded record budget."
                && rejectedDestination.motsArchLighting.size() == recordsBefore
                && rejectedDestination.things[1].archLightingIndex == -1,
            "ARCHLIGHTING clipboard rejects aggregate vertex budget overflow atomically");
    }

    {
        Level destination = makeLevel();
        destination.things[1].archLightingIndex = -1;
        const std::array<MotsArchLightingClipboardTarget, 2> targets{
            makeTarget(1, "models/door.3do", topology),
            makeTarget(2, "other/door.3do", topology) };
        const std::size_t recordsBefore = destination.motsArchLighting.size();
        const std::int32_t firstBefore = destination.things[1].archLightingIndex;
        const std::int32_t secondBefore = destination.things[2].archLightingIndex;
        error.clear();
        const MotsArchLightingClipboardResult result =
            pasteMotsArchLightingClipboard(
                destination, *payload, targets, nullptr, &error);
        check(result.status == MotsArchLightingClipboardStatus::Rejected
                && result.message
                    == "MotS ARCHLIGHTING clipboard target model identity is incompatible."
                && result.changedThingIndices.empty()
                && result.recordIndex == -1
                && error
                    == "MotS ARCHLIGHTING clipboard target model identity is incompatible."
                && destination.motsArchLighting.size() == recordsBefore
                && destination.things[1].archLightingIndex == firstBefore
                && destination.things[2].archLightingIndex == secondBefore,
            "ARCHLIGHTING clipboard paste rejects an incompatible batch atomically");
    }

    {
        Level destination = makeLevel();
        destination.things[1].archLightingIndex = -1;
        destination.things[2].values.clear();
        const std::array<MotsArchLightingClipboardTarget, 1> targets{
            makeTarget(2, "models/door.3do", topology) };
        error.clear();
        const MotsArchLightingClipboardResult result =
            pasteMotsArchLightingClipboard(
                destination, *payload, targets, nullptr, &error);
        check(result.status == MotsArchLightingClipboardStatus::Rejected
                && result.message
                    == "MotS ARCHLIGHTING clipboard target Thing must have the Arch flag."
                && result.changedThingIndices.empty()
                && result.recordIndex == -1
                && error
                    == "MotS ARCHLIGHTING clipboard target Thing must have the Arch flag."
                && destination.things[2].archLightingIndex == 1,
            "ARCHLIGHTING clipboard paste rejects a target without the Arch flag and leaves it unchanged");
    }

    {
        MotsArchLightingClipboardPayload invalid = *payload;
        invalid.topology.geosets.front().front() = 2u;
        error.clear();
        check(!encodeMotsArchLightingClipboard(invalid, &error)
                && error
                    == "MotS ARCHLIGHTING clipboard topology is invalid or incompatible.",
            "ARCHLIGHTING clipboard codec rejects topology that does not fit the record");

        invalid = *payload;
        invalid.object.meshes[0].vertices[0].green =
            std::numeric_limits<float>::quiet_NaN();
        error.clear();
        check(!encodeMotsArchLightingClipboard(invalid, &error)
                && error == "MotS ARCHLIGHTING clipboard channels must be finite.",
            "ARCHLIGHTING clipboard codec rejects non-finite authored channels");

        invalid = *payload;
        invalid.modelIdentity.assign(
            sed::level::kMotsArchLightingClipboardMaxIdentityBytes + 1u,
            'x');
        error.clear();
        check(!encodeMotsArchLightingClipboard(invalid, &error)
                && error
                    == "MotS ARCHLIGHTING clipboard model identity is invalid.",
            "ARCHLIGHTING clipboard codec rejects an oversized model identity");
    }

    {
        Level destination = makeLevel();
        error = "stale";
        const MotsArchLightingClipboardResult result =
            pasteMotsArchLightingClipboard(
                destination,
                *payload,
                std::span<const MotsArchLightingClipboardTarget>{},
                nullptr,
                &error);
        check(result.status == MotsArchLightingClipboardStatus::Noop
                && result.message
                    == "No MotS ARCHLIGHTING clipboard changes were necessary."
                && result.changedThingIndices.empty()
                && result.recordIndex == -1
                && error.empty(),
            "ARCHLIGHTING clipboard paste reports an empty-target no-op");
    }

    {
        Level wrongProject = makeLevel();
        wrongProject.kind = ProjectType::JKDF2;
        error = "stale";
        check(!copyMotsArchLightingClipboard(
                    wrongProject, 0, "models/door.3do", topology, nullptr, &error)
                && error == "MotS ARCHLIGHTING clipboard requires a MotS level.",
            "ARCHLIGHTING clipboard copy reports an exact project rejection");

        Level unbound = makeLevel();
        unbound.things[0].archLightingIndex = -1;
        error.clear();
        check(!copyMotsArchLightingClipboard(
                    unbound, 0, "models/door.3do", topology, nullptr, &error)
                && error
                    == "MotS ARCHLIGHTING clipboard source has no valid ARCHLIGHTING record.",
            "ARCHLIGHTING clipboard copy reports an exact unbound-source rejection");
    }
}

} // namespace sed::tests
