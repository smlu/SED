#pragma once

#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>

namespace sed::ui::audio
{

/** Duration used when ambient sector audio starts, stops, or changes volume. */
inline constexpr std::int64_t kAmbientSoundFadeDurationMs = 500;

/**
 * Linearly interpolates a normalized volume over a bounded time interval.
 * Inputs and the result are clamped to the QAudioSink volume range [0, 1].
 */
[[nodiscard]] double interpolateSoundVolume(
    double startVolume,
    double targetVolume,
    std::int64_t elapsedMilliseconds,
    std::int64_t durationMilliseconds = kAmbientSoundFadeDurationMs) noexcept;

/**
 * Builds a cache key that isolates identically named sounds mounted by
 * different project resource configurations.
 */
[[nodiscard]] std::string makeSoundResourceCacheKey(
    std::string_view soundName,
    std::span<const std::filesystem::path> roots,
    std::span<const std::filesystem::path> archives);

} // namespace sed::ui::audio
