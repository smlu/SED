#pragma once

#include "sed/assets/resource_locator.hpp"
#include "ui/components/preview_resource_revision_utils.hpp"
#include <QByteArray>
#include <QCryptographicHash>
#include <algorithm>
#include <array>
#include <exception>
#include <span>
#include <string_view>

namespace sed::ui::audio
{
inline void appendSoundIdentityPart(QCryptographicHash& hash, const std::span<const std::byte> bytes)
{
    // Length framing keeps arbitrary logical keys unambiguous. Chunked views
    // avoid another copy of the supplied WAV and work with Qt's int-size API.
    std::array<char, 8> length{};
    const auto size = static_cast<std::uint64_t>(bytes.size());
    for (unsigned i = 0; i < length.size(); ++i)
        length[i] = static_cast<char>((size >> (i * 8u)) & 0xffu);
    hash.addData(QByteArray::fromRawData(length.data(), static_cast<int>(length.size())));
    std::size_t offset = 0;
    while (offset < bytes.size())
    {
        const auto count = static_cast<int>(std::min<std::size_t>(bytes.size() - offset, 1024u * 1024u));
        hash.addData(QByteArray::fromRawData(reinterpret_cast<const char*>(bytes.data() + offset), count));
        offset += static_cast<std::size_t>(count);
    }
}

inline std::string hashedSoundRequestKey(const std::string_view kind,
    const std::string_view logicalName, const std::span<const std::byte> revision)
{
    QCryptographicHash hash(QCryptographicHash::Sha256);
    appendSoundIdentityPart(hash, std::as_bytes(std::span(kind.data(), kind.size())));
    appendSoundIdentityPart(hash, std::as_bytes(std::span(logicalName.data(), logicalName.size())));
    appendSoundIdentityPart(hash, revision);
    return std::string(kind) + ':' + hash.result().toHex().toStdString();
}

// The same bounded key feeds the active-owner comparison and decoded cache.
inline std::string soundBytesRequestKey(const std::string_view key, const std::span<const std::byte> bytes)
{
    return hashedSoundRequestKey("BYTES", key, bytes);
}

inline std::string soundResourceRequestKey(
    const assets::ResourceLocator& resources, const std::string& soundName)
{
    // Repeated ambient requests resolve metadata only, without rereading or
    // hashing sound contents. Explicit archive reloads carry a new mount ID.
    const auto revision = ::sed::ui::detail::previewResourceRevisionToken(&resources, soundName, ".wav");
    return hashedSoundRequestKey("RESOURCE", soundName,
        std::as_bytes(std::span(revision.data(), revision.size())));
}

inline std::optional<std::vector<std::byte>> readSoundResource(
    const assets::ResourceLocator& resources, const std::string& soundName)
{
    try { return resources.tryReadBinaryWithExt(soundName, ".wav"); }
    catch (const std::exception&) { return std::nullopt; }
}
inline bool ambientSoundTargetChanged(const std::string_view previousName,
    const assets::ResourceLocator* previousLocator, const std::string_view previousKey,
    const std::string_view requestedName, const assets::ResourceLocator* requestedLocator,
    const std::string_view requestedKey)
{
    return previousName != requestedName || previousLocator != requestedLocator
        || previousKey != requestedKey;
}

inline bool ambientSoundResultCurrent(const std::string_view pendingKey,
    const std::string_view trackedKey, const std::string_view liveKey)
{
    return pendingKey == trackedKey && pendingKey == liveKey;
}

}
