#pragma once

#include <cstddef>
#include <optional>
#include <span>
#include <vector>

namespace sed::assets
{

/** Returns whether the byte stream starts with the legacy INDIWV signature. */
bool isIndyWvSound(std::span<const std::byte> bytes) noexcept;

/**
 * Decodes an Indiana Jones INDIWV stream into an in-memory PCM RIFF/WAVE file.
 * Returns no value when the stream is truncated or uses an unsupported codec.
 */
std::optional<std::vector<std::byte>> convertIndyWvToWav(std::span<const std::byte> bytes);

} // namespace sed::assets
