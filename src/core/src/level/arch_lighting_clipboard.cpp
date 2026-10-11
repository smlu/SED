#include "sed/level/arch_lighting_clipboard.hpp"

#include "sed/level/thing.hpp"
#include "sed/text.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>

namespace sed::level
{

namespace
{

constexpr std::array<std::byte, 8> kClipboardMagic{
    std::byte{ 'S' },
    std::byte{ 'E' },
    std::byte{ 'D' },
    std::byte{ 'A' },
    std::byte{ 'R' },
    std::byte{ 'C' },
    std::byte{ 'H' },
    std::byte{ 0 } };
constexpr std::uint32_t kClipboardVersion = 1u;
constexpr std::int32_t kArchFlag = 0x00000008;

constexpr std::string_view kAppliedMessage =
    "Pasted MotS ARCHLIGHTING to selected Things.";
constexpr std::string_view kNoopMessage =
    "No MotS ARCHLIGHTING clipboard changes were necessary.";
constexpr std::string_view kProjectMessage =
    "MotS ARCHLIGHTING clipboard requires a MotS level.";
constexpr std::string_view kSourceIndexMessage =
    "MotS ARCHLIGHTING clipboard source Thing index is invalid.";
constexpr std::string_view kTargetIndexMessage =
    "MotS ARCHLIGHTING clipboard target Thing index is invalid.";
constexpr std::string_view kDuplicateTargetMessage =
    "MotS ARCHLIGHTING clipboard target selection contains a duplicate index.";
constexpr std::string_view kSourceIdentityMessage =
    "MotS ARCHLIGHTING clipboard requires a non-empty model identity.";
constexpr std::string_view kTargetIdentityMessage =
    "MotS ARCHLIGHTING clipboard target model identity is empty.";
constexpr std::string_view kIdentityMismatchMessage =
    "MotS ARCHLIGHTING clipboard target model identity is incompatible.";
constexpr std::string_view kSourceThingMessage =
    "MotS ARCHLIGHTING clipboard source Thing must have the Arch flag.";
constexpr std::string_view kTargetThingMessage =
    "MotS ARCHLIGHTING clipboard target Thing must have the Arch flag.";
constexpr std::string_view kSourceRecordMessage =
    "MotS ARCHLIGHTING clipboard source has no valid ARCHLIGHTING record.";
constexpr std::string_view kTargetRecordMessage =
    "MotS ARCHLIGHTING clipboard target has an invalid ARCHLIGHTING record.";
constexpr std::string_view kTopologyMessage =
    "MotS ARCHLIGHTING clipboard topology is invalid or incompatible.";
constexpr std::string_view kRecordMessage =
    "MotS ARCHLIGHTING clipboard record is invalid.";
constexpr std::string_view kFiniteMessage =
    "MotS ARCHLIGHTING clipboard channels must be finite.";
constexpr std::string_view kBudgetMessage =
    "MotS ARCHLIGHTING clipboard exceeds its bounded record budget.";
constexpr std::string_view kEmptyMessage =
    "MotS ARCHLIGHTING clipboard payload is empty.";
constexpr std::string_view kTooLargeMessage =
    "MotS ARCHLIGHTING clipboard payload is too large.";
constexpr std::string_view kMagicMessage =
    "MotS ARCHLIGHTING clipboard signature is invalid.";
constexpr std::string_view kVersionMessage =
    "MotS ARCHLIGHTING clipboard version is unsupported.";
constexpr std::string_view kTruncatedMessage =
    "MotS ARCHLIGHTING clipboard payload is truncated.";
constexpr std::string_view kTrailingMessage =
    "MotS ARCHLIGHTING clipboard payload has trailing bytes.";
constexpr std::string_view kIdentityInvalidMessage =
    "MotS ARCHLIGHTING clipboard model identity is invalid.";

bool hasUsableModel(
    const Level& level,
    const assets::Templates* const templates,
    const Thing& thing)
{
    const TplValue* model = thingFindValue(level, templates, thing, "model3d");
    if (!model)
    {
        model = thingFindValue(level, templates, thing, "model3do");
    }
    if (!model)
    {
        return false;
    }
    const std::string modelName = model->asString();
    const std::string_view trimmed = text::trimView(modelName);
    return !trimmed.empty() && !text::iequalsAscii(trimmed, "none");
}

void promoteThingFlags(
    std::vector<TplValue>& values,
    const std::int32_t effectiveFlags)
{
    for (auto it = values.rbegin(); it != values.rend(); ++it)
    {
        if (!text::iequalsAscii(it->name, "thingflags"))
        {
            continue;
        }
        if (it->type == ValueType::integer
            || it->type == ValueType::flag
            || it->type == ValueType::ptr)
        {
            it->intValue |= kArchFlag;
            return;
        }
        break;
    }

    TplValue overrideValue{};
    overrideValue.name = "thingflags";
    overrideValue.type = ValueType::integer;
    overrideValue.intValue = effectiveFlags | kArchFlag;
    values.push_back(std::move(overrideValue));
}

void clearError(std::string* const error) noexcept
{
    if (error)
    {
        error->clear();
    }
}

void setError(
    std::string* const error,
    const std::string_view message) noexcept
{
    if (!error)
    {
        return;
    }
    try
    {
        error->assign(message.data(), message.size());
    }
    catch (...)
    {
        // Error reporting is best effort on a bad-allocation path.  The
        // no-throw codec boundary must never terminate while assigning text.
        error->clear();
    }
}

bool addWithin(
    std::size_t& total,
    const std::size_t amount,
    const std::size_t limit) noexcept
{
    if (total > limit || amount > limit - total)
    {
        return false;
    }
    total += amount;
    return true;
}

bool finiteVertex(const MotsArchLightVertex& vertex) noexcept
{
    return std::isfinite(vertex.mono)
        && std::isfinite(vertex.red)
        && std::isfinite(vertex.green)
        && std::isfinite(vertex.blue);
}

bool sameFloatBits(const float left, const float right) noexcept
{
    return std::bit_cast<std::uint32_t>(left)
        == std::bit_cast<std::uint32_t>(right);
}

bool sameVertexBits(
    const MotsArchLightVertex& left,
    const MotsArchLightVertex& right) noexcept
{
    return sameFloatBits(left.mono, right.mono)
        && sameFloatBits(left.red, right.red)
        && sameFloatBits(left.green, right.green)
        && sameFloatBits(left.blue, right.blue);
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

bool validIdentity(const std::string_view identity) noexcept
{
    return !identity.empty()
        && identity.size() <= kMotsArchLightingClipboardMaxIdentityBytes
        && identity.find('\0') == std::string_view::npos;
}

bool validTopologyShape(const MotsArchLightingTopology& topology) noexcept
{
    if (topology.geosets.empty()
        || topology.geosets.size() > kMotsArchLightingClipboardMaxItems)
    {
        return false;
    }

    std::size_t totalMeshPrefixes = 0;
    std::size_t totalVertexPrefixes = 0;
    for (const std::vector<std::size_t>& geoset : topology.geosets)
    {
        if (geoset.empty()
            || geoset.size() > kMotsArchLightingClipboardMaxItems
            || !addWithin(
                totalMeshPrefixes,
                geoset.size(),
                kMotsArchLightingClipboardMaxItems))
        {
            return false;
        }
        for (const std::size_t vertexCount : geoset)
        {
            if (vertexCount > kMotsArchLightingClipboardMaxItems
                || !addWithin(
                    totalVertexPrefixes,
                    vertexCount,
                    kMotsArchLightingClipboardMaxItems))
            {
                return false;
            }
        }
    }
    return true;
}

bool validArchiveStructure(const MotsArchLightObject& object) noexcept
{
    if (object.wireIndex < 0
        || object.meshes.size() > kMotsArchLightingClipboardMaxItems)
    {
        return false;
    }

    std::unordered_set<std::int32_t> meshWires{};
    try
    {
        meshWires.reserve(object.meshes.size());
    }
    catch (...)
    {
        return false;
    }

    std::size_t vertexTotal = 0;
    try
    {
        for (const MotsArchLightMesh& mesh : object.meshes)
        {
            if (mesh.wireIndex < 0
                || !meshWires.insert(mesh.wireIndex).second
                || mesh.vertices.size() > kMotsArchLightingClipboardMaxItems
                || !addWithin(
                    vertexTotal,
                    mesh.vertices.size(),
                    kMotsArchLightingClipboardMaxItems))
            {
                return false;
            }
        }
    }
    catch (...)
    {
        return false;
    }
    return true;
}

bool finiteArchiveChannels(const MotsArchLightObject& object) noexcept
{
    for (const MotsArchLightMesh& mesh : object.meshes)
    {
        for (const MotsArchLightVertex& vertex : mesh.vertices)
        {
            if (!finiteVertex(vertex))
            {
                return false;
            }
        }
    }
    return true;
}

bool topologyFitsObject(
    const MotsArchLightingTopology& topology,
    const MotsArchLightObject& object) noexcept
{
    if (!validTopologyShape(topology))
    {
        return false;
    }
    for (const std::vector<std::size_t>& geoset : topology.geosets)
    {
        if (geoset.size() > object.meshes.size())
        {
            return false;
        }
        for (std::size_t meshIndex = 0; meshIndex < geoset.size(); ++meshIndex)
        {
            if (geoset[meshIndex]
                > object.meshes[meshIndex].vertices.size())
            {
                return false;
            }
        }
    }
    return true;
}

bool archiveBudgetAllowsAppend(
    const Level& level,
    const MotsArchLightObject& additional) noexcept
{
    if (level.motsArchLighting.size()
            >= kMotsArchLightingClipboardMaxItems)
    {
        return false;
    }

    std::size_t meshTotal = 0;
    std::size_t vertexTotal = 0;
    for (const MotsArchLightObject& object : level.motsArchLighting)
    {
        if (!addWithin(
                meshTotal,
                object.meshes.size(),
                kMotsArchLightingClipboardMaxItems))
        {
            return false;
        }
        for (const MotsArchLightMesh& mesh : object.meshes)
        {
            if (!addWithin(
                    vertexTotal,
                    mesh.vertices.size(),
                    kMotsArchLightingClipboardMaxItems))
            {
                return false;
            }
        }
    }
    if (!addWithin(
            meshTotal,
            additional.meshes.size(),
            kMotsArchLightingClipboardMaxItems))
    {
        return false;
    }
    for (const MotsArchLightMesh& mesh : additional.meshes)
    {
        if (!addWithin(
                vertexTotal,
                mesh.vertices.size(),
                kMotsArchLightingClipboardMaxItems))
        {
            return false;
        }
    }
    return true;
}

std::string_view validatePayloadShape(
    const MotsArchLightingClipboardPayload& payload,
    const std::string_view normalizedIdentity) noexcept
{
    if (!validIdentity(normalizedIdentity)
        || normalizedIdentity.size() > kMotsArchLightingClipboardMaxIdentityBytes)
    {
        return kIdentityInvalidMessage;
    }
    if (!validArchiveStructure(payload.object))
    {
        return kRecordMessage;
    }
    if (!finiteArchiveChannels(payload.object))
    {
        return kFiniteMessage;
    }
    if (!topologyFitsObject(payload.topology, payload.object))
    {
        return kTopologyMessage;
    }
    return {};
}

bool encodedSizeForPayload(
    const MotsArchLightingClipboardPayload& payload,
    const std::string_view identity,
    std::size_t& size) noexcept
{
    size = 0;
    if (!addWithin(size, kClipboardMagic.size(), kMotsArchLightingClipboardMaxBytes)
        || !addWithin(size, 4u, kMotsArchLightingClipboardMaxBytes)
        || !addWithin(size, 4u, kMotsArchLightingClipboardMaxBytes)
        || !addWithin(size, identity.size(), kMotsArchLightingClipboardMaxBytes)
        || !addWithin(size, 4u, kMotsArchLightingClipboardMaxBytes))
    {
        return false;
    }

    for (const std::vector<std::size_t>& geoset : payload.topology.geosets)
    {
        if (!addWithin(size, 4u, kMotsArchLightingClipboardMaxBytes)
            || geoset.size() > (kMotsArchLightingClipboardMaxBytes - size) / 4u
            || !addWithin(
                size,
                geoset.size() * 4u,
                kMotsArchLightingClipboardMaxBytes))
        {
            return false;
        }
    }
    if (!addWithin(size, 4u, kMotsArchLightingClipboardMaxBytes)
        || !addWithin(size, 4u, kMotsArchLightingClipboardMaxBytes))
    {
        return false;
    }
    for (const MotsArchLightMesh& mesh : payload.object.meshes)
    {
        if (!addWithin(size, 4u, kMotsArchLightingClipboardMaxBytes))
        {
            return false;
        }
        if (size > kMotsArchLightingClipboardMaxBytes - 4u
            || mesh.vertices.size()
                > (kMotsArchLightingClipboardMaxBytes - size - 4u) / 16u
            || !addWithin(size, 4u, kMotsArchLightingClipboardMaxBytes)
            || !addWithin(
                size,
                mesh.vertices.size() * 16u,
                kMotsArchLightingClipboardMaxBytes))
        {
            return false;
        }
    }
    return true;
}

void appendU32(std::vector<std::byte>& bytes, const std::uint32_t value)
{
    bytes.push_back(static_cast<std::byte>(value & 0xffu));
    bytes.push_back(static_cast<std::byte>((value >> 8u) & 0xffu));
    bytes.push_back(static_cast<std::byte>((value >> 16u) & 0xffu));
    bytes.push_back(static_cast<std::byte>((value >> 24u) & 0xffu));
}

void appendI32(std::vector<std::byte>& bytes, const std::int32_t value)
{
    appendU32(bytes, std::bit_cast<std::uint32_t>(value));
}

void appendFloat(std::vector<std::byte>& bytes, const float value)
{
    appendU32(bytes, std::bit_cast<std::uint32_t>(value));
}

class ClipboardReader final
{
public:
    explicit ClipboardReader(const std::span<const std::byte> bytes) noexcept
        : bytes_(bytes)
    {
    }

    bool take(
        const std::size_t count,
        std::span<const std::byte>& result) noexcept
    {
        if (count > bytes_.size() - offset_)
        {
            return false;
        }
        result = bytes_.subspan(offset_, count);
        offset_ += count;
        return true;
    }

    bool u32(std::uint32_t& value) noexcept
    {
        std::span<const std::byte> bytes{};
        if (!take(4u, bytes))
        {
            return false;
        }
        value = static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(bytes[0]))
            | (static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(bytes[1])) << 8u)
            | (static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(bytes[2])) << 16u)
            | (static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(bytes[3])) << 24u);
        return true;
    }

    bool i32(std::int32_t& value) noexcept
    {
        std::uint32_t raw = 0;
        if (!u32(raw))
        {
            return false;
        }
        value = std::bit_cast<std::int32_t>(raw);
        return true;
    }

    bool floatValue(float& value) noexcept
    {
        std::uint32_t raw = 0;
        if (!u32(raw))
        {
            return false;
        }
        value = std::bit_cast<float>(raw);
        return true;
    }

    std::size_t remaining() const noexcept
    {
        return bytes_.size() - offset_;
    }

private:
    std::span<const std::byte> bytes_{};
    std::size_t offset_{};
};

bool readIdentity(
    ClipboardReader& reader,
    std::string& identity)
{
    std::uint32_t length = 0;
    if (!reader.u32(length)
        || length == 0
        || length > kMotsArchLightingClipboardMaxIdentityBytes)
    {
        return false;
    }
    std::span<const std::byte> bytes{};
    if (!reader.take(length, bytes))
    {
        return false;
    }
    identity.assign(
        reinterpret_cast<const char*>(bytes.data()),
        bytes.size());
    identity = normalizeMotsArchLightingModelIdentity(identity);
    return validIdentity(identity);
}

MotsArchLightingClipboardResult rejectedResult(
    const std::string_view message,
    std::string* const error)
{
    MotsArchLightingClipboardResult result{};
    result.status = MotsArchLightingClipboardStatus::Rejected;
    try
    {
        result.message.assign(message.data(), message.size());
    }
    catch (...)
    {
        result.message.clear();
    }
    setError(error, message);
    return result;
}

} // namespace

std::string normalizeMotsArchLightingModelIdentity(const std::string_view identity)
{
    const std::string trimmed = sed::text::trim(identity);
    std::string normalized{};
    normalized.reserve(trimmed.size());
    for (const char character : trimmed)
    {
        char slash = character == '\\' ? '/' : character;
        if (slash >= 'A' && slash <= 'Z')
        {
            slash = static_cast<char>(slash + ('a' - 'A'));
        }
        normalized.push_back(slash);
    }
    return normalized;
}

std::optional<MotsArchLightingClipboardPayload>
copyMotsArchLightingClipboard(
    const Level& level,
    const std::int32_t sourceThingIndex,
    const std::string_view modelIdentity,
    const MotsArchLightingTopology& topology,
    const assets::Templates* const templates,
    std::string* const error)
{
    clearError(error);
    try
    {
        if (level.kind != ProjectType::MOTS)
        {
            setError(error, kProjectMessage);
            return std::nullopt;
        }
        if (sourceThingIndex < 0
            || static_cast<std::size_t>(sourceThingIndex) >= level.things.size())
        {
            setError(error, kSourceIndexMessage);
            return std::nullopt;
        }
        const std::string normalizedIdentity =
            normalizeMotsArchLightingModelIdentity(modelIdentity);
        if (!validIdentity(normalizedIdentity))
        {
            setError(error, kSourceIdentityMessage);
            return std::nullopt;
        }

        const Thing& sourceThing = level.things[
            static_cast<std::size_t>(sourceThingIndex)];
        if ((thingFlags(level, templates, sourceThing) & kArchFlag) == 0)
        {
            setError(error, kSourceThingMessage);
            return std::nullopt;
        }
        if (sourceThing.archLightingIndex < 0
            || static_cast<std::size_t>(sourceThing.archLightingIndex)
                >= level.motsArchLighting.size())
        {
            setError(error, kSourceRecordMessage);
            return std::nullopt;
        }

        MotsArchLightingClipboardPayload payload{};
        payload.modelIdentity = normalizedIdentity;
        payload.topology = topology;
        payload.object = level.motsArchLighting[
            static_cast<std::size_t>(sourceThing.archLightingIndex)];
        if (!validArchiveStructure(payload.object))
        {
            setError(error, kRecordMessage);
            return std::nullopt;
        }
        if (!finiteArchiveChannels(payload.object))
        {
            setError(error, kFiniteMessage);
            return std::nullopt;
        }
        if (!topologyFitsObject(payload.topology, payload.object))
        {
            setError(error, kTopologyMessage);
            return std::nullopt;
        }
        return payload;
    }
    catch (...)
    {
        setError(error, kBudgetMessage);
        return std::nullopt;
    }
}

std::optional<std::vector<std::byte>> encodeMotsArchLightingClipboard(
    const MotsArchLightingClipboardPayload& payload,
    std::string* const error) noexcept
{
    clearError(error);
    try
    {
        const std::string normalizedIdentity =
            normalizeMotsArchLightingModelIdentity(payload.modelIdentity);
        const std::string_view validation = validatePayloadShape(
            payload, normalizedIdentity);
        if (!validation.empty())
        {
            setError(error, validation);
            return std::nullopt;
        }
        std::size_t encodedSize = 0;
        if (!encodedSizeForPayload(payload, normalizedIdentity, encodedSize))
        {
            setError(error, kTooLargeMessage);
            return std::nullopt;
        }
        std::vector<std::byte> bytes{};
        bytes.reserve(encodedSize);
        bytes.insert(bytes.end(), kClipboardMagic.begin(), kClipboardMagic.end());
        appendU32(bytes, kClipboardVersion);
        appendU32(bytes, static_cast<std::uint32_t>(normalizedIdentity.size()));
        for (const char character : normalizedIdentity)
        {
            bytes.push_back(static_cast<std::byte>(
                static_cast<unsigned char>(character)));
        }
        appendU32(
            bytes,
            static_cast<std::uint32_t>(payload.topology.geosets.size()));
        for (const std::vector<std::size_t>& geoset : payload.topology.geosets)
        {
            appendU32(bytes, static_cast<std::uint32_t>(geoset.size()));
            for (const std::size_t vertexCount : geoset)
            {
                appendU32(bytes, static_cast<std::uint32_t>(vertexCount));
            }
        }
        appendI32(bytes, payload.object.wireIndex);
        appendU32(bytes, static_cast<std::uint32_t>(payload.object.meshes.size()));
        for (const MotsArchLightMesh& mesh : payload.object.meshes)
        {
            appendI32(bytes, mesh.wireIndex);
            appendU32(bytes, static_cast<std::uint32_t>(mesh.vertices.size()));
            for (const MotsArchLightVertex& vertex : mesh.vertices)
            {
                appendFloat(bytes, vertex.mono);
                appendFloat(bytes, vertex.red);
                appendFloat(bytes, vertex.green);
                appendFloat(bytes, vertex.blue);
            }
        }
        return bytes;
    }
    catch (...)
    {
        setError(error, kTooLargeMessage);
        return std::nullopt;
    }
}

std::optional<MotsArchLightingClipboardPayload> decodeMotsArchLightingClipboard(
    const std::span<const std::byte> bytes,
    std::string* const error) noexcept
{
    clearError(error);
    try
    {
        if (bytes.empty())
        {
            setError(error, kEmptyMessage);
            return std::nullopt;
        }
        if (bytes.size() > kMotsArchLightingClipboardMaxBytes)
        {
            setError(error, kTooLargeMessage);
            return std::nullopt;
        }

        ClipboardReader reader(bytes);
        std::span<const std::byte> magic{};
        if (!reader.take(kClipboardMagic.size(), magic)
            || !std::equal(magic.begin(), magic.end(), kClipboardMagic.begin()))
        {
            setError(error, kMagicMessage);
            return std::nullopt;
        }
        std::uint32_t version = 0;
        if (!reader.u32(version))
        {
            setError(error, kTruncatedMessage);
            return std::nullopt;
        }
        if (version != kClipboardVersion)
        {
            setError(error, kVersionMessage);
            return std::nullopt;
        }

        MotsArchLightingClipboardPayload payload{};
        if (!readIdentity(reader, payload.modelIdentity))
        {
            setError(error, kIdentityInvalidMessage);
            return std::nullopt;
        }

        std::uint32_t geosetCount = 0;
        if (!reader.u32(geosetCount)
            || geosetCount == 0
            || geosetCount > kMotsArchLightingClipboardMaxItems
            || geosetCount > reader.remaining() / 8u)
        {
            setError(error, kTopologyMessage);
            return std::nullopt;
        }
        payload.topology.geosets.reserve(geosetCount);
        std::size_t topologyMeshPrefixes = 0;
        std::size_t topologyVertexPrefixes = 0;
        for (std::uint32_t geosetIndex = 0;
             geosetIndex < geosetCount;
             ++geosetIndex)
        {
            std::uint32_t meshCount = 0;
            if (!reader.u32(meshCount)
                || meshCount == 0
                || meshCount > kMotsArchLightingClipboardMaxItems
                || meshCount > reader.remaining() / 4u
                || !addWithin(
                    topologyMeshPrefixes,
                    meshCount,
                    kMotsArchLightingClipboardMaxItems))
            {
                setError(error, kTopologyMessage);
                return std::nullopt;
            }
            std::vector<std::size_t> geoset{};
            geoset.reserve(meshCount);
            for (std::uint32_t meshIndex = 0;
                 meshIndex < meshCount;
                 ++meshIndex)
            {
                std::uint32_t vertexCount = 0;
                if (!reader.u32(vertexCount)
                    || vertexCount > kMotsArchLightingClipboardMaxItems)
                {
                    setError(error, kTopologyMessage);
                    return std::nullopt;
                }
                if (!addWithin(
                        topologyVertexPrefixes,
                        vertexCount,
                        kMotsArchLightingClipboardMaxItems))
                {
                    setError(error, kTopologyMessage);
                    return std::nullopt;
                }
                geoset.push_back(vertexCount);
            }
            payload.topology.geosets.push_back(std::move(geoset));
        }

        if (!reader.i32(payload.object.wireIndex)
            || payload.object.wireIndex < 0)
        {
            setError(error, kRecordMessage);
            return std::nullopt;
        }
        std::uint32_t meshCount = 0;
        if (!reader.u32(meshCount)
            || meshCount > kMotsArchLightingClipboardMaxItems
            || meshCount > reader.remaining() / 8u)
        {
            setError(error, kRecordMessage);
            return std::nullopt;
        }
        payload.object.meshes.reserve(meshCount);
        std::size_t vertexTotal = 0;
        for (std::uint32_t meshIndex = 0;
             meshIndex < meshCount;
             ++meshIndex)
        {
            MotsArchLightMesh mesh{};
            std::uint32_t vertexCount = 0;
            if (!reader.i32(mesh.wireIndex)
                || mesh.wireIndex < 0
                || !reader.u32(vertexCount)
                || vertexCount > kMotsArchLightingClipboardMaxItems
                || !addWithin(
                    vertexTotal,
                    vertexCount,
                    kMotsArchLightingClipboardMaxItems))
            {
                setError(error, kRecordMessage);
                return std::nullopt;
            }
            if (vertexCount > reader.remaining() / 16u)
            {
                setError(error, kTruncatedMessage);
                return std::nullopt;
            }
            mesh.vertices.reserve(vertexCount);
            for (std::uint32_t vertexIndex = 0;
                 vertexIndex < vertexCount;
                 ++vertexIndex)
            {
                MotsArchLightVertex vertex{};
                if (!reader.floatValue(vertex.mono)
                    || !reader.floatValue(vertex.red)
                    || !reader.floatValue(vertex.green)
                    || !reader.floatValue(vertex.blue))
                {
                    setError(error, kTruncatedMessage);
                    return std::nullopt;
                }
                if (!finiteVertex(vertex))
                {
                    setError(error, kFiniteMessage);
                    return std::nullopt;
                }
                mesh.vertices.push_back(vertex);
            }
            payload.object.meshes.push_back(std::move(mesh));
        }
        if (reader.remaining() != 0)
        {
            setError(error, kTrailingMessage);
            return std::nullopt;
        }
        const std::string_view validation = validatePayloadShape(
            payload, payload.modelIdentity);
        if (!validation.empty())
        {
            setError(error, validation);
            return std::nullopt;
        }
        return payload;
    }
    catch (...)
    {
        setError(error, kTooLargeMessage);
        return std::nullopt;
    }
}

MotsArchLightingClipboardResult pasteMotsArchLightingClipboard(
    Level& level,
    const MotsArchLightingClipboardPayload& payload,
    const std::span<const MotsArchLightingClipboardTarget> targets,
    const assets::Templates* const templates,
    std::string* const error)
{
    clearError(error);
    try
    {
        if (level.kind != ProjectType::MOTS)
        {
            return rejectedResult(kProjectMessage, error);
        }
        if (targets.empty())
        {
            MotsArchLightingClipboardResult result{};
            result.status = MotsArchLightingClipboardStatus::Noop;
            result.message.assign(kNoopMessage.data(), kNoopMessage.size());
            return result;
        }
        if (targets.size() > kMotsArchLightingClipboardMaxItems)
        {
            return rejectedResult(kBudgetMessage, error);
        }

        const std::string normalizedIdentity =
            normalizeMotsArchLightingModelIdentity(payload.modelIdentity);
        const std::string_view payloadValidation = validatePayloadShape(
            payload, normalizedIdentity);
        if (!payloadValidation.empty())
        {
            return rejectedResult(payloadValidation, error);
        }

        std::unordered_set<std::size_t> seenTargets{};
        seenTargets.reserve(targets.size());
        std::vector<bool> promoteTargets(targets.size(), false);
        std::vector<std::pair<std::size_t, std::vector<TplValue>>> stagedFlags{};
        stagedFlags.reserve(targets.size());
        for (std::size_t targetOrdinal = 0;
             targetOrdinal < targets.size();
             ++targetOrdinal)
        {
            const MotsArchLightingClipboardTarget& target = targets[targetOrdinal];
            if (target.thingIndex < 0
                || static_cast<std::size_t>(target.thingIndex)
                    >= level.things.size())
            {
                return rejectedResult(kTargetIndexMessage, error);
            }
            const std::size_t thingIndex =
                static_cast<std::size_t>(target.thingIndex);
            if (!seenTargets.insert(thingIndex).second)
            {
                return rejectedResult(kDuplicateTargetMessage, error);
            }
            const std::string normalizedTargetIdentity =
                normalizeMotsArchLightingModelIdentity(target.modelIdentity);
            if (!validIdentity(normalizedTargetIdentity))
            {
                return rejectedResult(kTargetIdentityMessage, error);
            }
            if (normalizedTargetIdentity != normalizedIdentity)
            {
                return rejectedResult(kIdentityMismatchMessage, error);
            }
            if (target.topology.geosets != payload.topology.geosets)
            {
                return rejectedResult(kTopologyMessage, error);
            }
            const Thing& thing = level.things[thingIndex];
            const std::int32_t targetFlags = thingFlags(
                level, templates, thing);
            const bool promoteTarget = (targetFlags & kArchFlag) == 0;
            if (promoteTarget
                && ((targetFlags & TF_INVISIBLE) != 0
                    || !hasUsableModel(level, templates, thing)))
            {
                return rejectedResult(kTargetThingMessage, error);
            }
            promoteTargets[targetOrdinal] = promoteTarget;
            if (promoteTarget)
            {
                std::vector<TplValue> values = thing.values;
                values.reserve(values.size() + 1u);
                promoteThingFlags(values, targetFlags);
                stagedFlags.emplace_back(thingIndex, std::move(values));
            }
            if (thing.archLightingIndex < -1
                || (thing.archLightingIndex >= 0
                    && static_cast<std::size_t>(thing.archLightingIndex)
                        >= level.motsArchLighting.size()))
            {
                return rejectedResult(kTargetRecordMessage, error);
            }
            if (thing.archLightingIndex >= 0)
            {
                const MotsArchLightObject& object = level.motsArchLighting[
                    static_cast<std::size_t>(thing.archLightingIndex)];
                if (!validArchiveStructure(object)
                    || !finiteArchiveChannels(object))
                {
                    return rejectedResult(kTargetRecordMessage, error);
                }
            }
        }

        std::int32_t recordIndex = -1;
        for (std::size_t index = 0;
             index < level.motsArchLighting.size();
             ++index)
        {
            if (validArchiveStructure(level.motsArchLighting[index])
                && finiteArchiveChannels(level.motsArchLighting[index])
                && sameObjectValues(
                    level.motsArchLighting[index], payload.object))
            {
                if (index > static_cast<std::size_t>(
                        std::numeric_limits<std::int32_t>::max()))
                {
                    return rejectedResult(kBudgetMessage, error);
                }
                recordIndex = static_cast<std::int32_t>(index);
                break;
            }
        }

        MotsArchLightObject stagedObject{};
        bool append = false;
        if (recordIndex < 0)
        {
            append = true;
            if (!archiveBudgetAllowsAppend(level, payload.object))
            {
                return rejectedResult(kBudgetMessage, error);
            }
            stagedObject = payload.object;
            std::unordered_set<std::int32_t> usedWires{};
            usedWires.reserve(level.motsArchLighting.size() + 1u);
            for (const MotsArchLightObject& object : level.motsArchLighting)
            {
                usedWires.insert(object.wireIndex);
            }
            std::int64_t nextWire = 0;
            while (nextWire <= std::numeric_limits<std::int32_t>::max()
                && usedWires.contains(static_cast<std::int32_t>(nextWire)))
            {
                ++nextWire;
            }
            if (nextWire > std::numeric_limits<std::int32_t>::max())
            {
                return rejectedResult(kBudgetMessage, error);
            }
            stagedObject.wireIndex = static_cast<std::int32_t>(nextWire);
            if (level.motsArchLighting.size()
                    > static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max()))
            {
                return rejectedResult(kBudgetMessage, error);
            }
            recordIndex = static_cast<std::int32_t>(
                level.motsArchLighting.size());
        }

        MotsArchLightingClipboardResult result{};
        result.recordIndex = recordIndex;
        result.changedThingIndices.reserve(targets.size());
        for (std::size_t targetOrdinal = 0;
             targetOrdinal < targets.size();
             ++targetOrdinal)
        {
            const MotsArchLightingClipboardTarget& target = targets[targetOrdinal];
            const Thing& thing = level.things[
                static_cast<std::size_t>(target.thingIndex)];
            if (thing.archLightingIndex != recordIndex
                || promoteTargets[targetOrdinal])
            {
                result.changedThingIndices.push_back(target.thingIndex);
            }
        }
        if (result.changedThingIndices.empty())
        {
            result.status = MotsArchLightingClipboardStatus::Noop;
            result.message.assign(kNoopMessage.data(), kNoopMessage.size());
            return result;
        }

        result.status = MotsArchLightingClipboardStatus::Applied;
        result.message.assign(kAppliedMessage.data(), kAppliedMessage.size());
        if (append)
        {
            level.motsArchLighting.reserve(level.motsArchLighting.size() + 1u);
            level.motsArchLighting.push_back(std::move(stagedObject));
        }
        for (auto& [thingIndex, values] : stagedFlags)
        {
            level.things[thingIndex].values = std::move(values);
        }
        for (const std::int32_t thingIndex : result.changedThingIndices)
        {
            level.things[static_cast<std::size_t>(thingIndex)].archLightingIndex =
                recordIndex;
        }
        return result;
    }
    catch (...)
    {
        return rejectedResult(kBudgetMessage, error);
    }
}

} // namespace sed::level
