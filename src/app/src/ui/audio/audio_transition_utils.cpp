#include "ui/audio/audio_transition_utils.hpp"

#include <algorithm>

namespace sed::ui::audio
{
namespace
{

void appendCacheKeyPart(std::string& key, const std::string_view value)
{
    key.append(std::to_string(value.size()));
    key.push_back(':');
    key.append(value);
}

void appendCacheKeyPaths(
    std::string& key,
    const std::span<const std::filesystem::path> paths)
{
    key.append(std::to_string(paths.size()));
    key.push_back('[');
    for (const std::filesystem::path& path : paths)
    {
        appendCacheKeyPart(key, path.lexically_normal().generic_string());
    }
    key.push_back(']');
}

} // namespace

double interpolateSoundVolume(
    const double startVolume,
    const double targetVolume,
    const std::int64_t elapsedMilliseconds,
    const std::int64_t durationMilliseconds) noexcept
{
    const double start = std::clamp(startVolume, 0.0, 1.0);
    const double target = std::clamp(targetVolume, 0.0, 1.0);
    if (durationMilliseconds <= 0 || elapsedMilliseconds >= durationMilliseconds)
    {
        return target;
    }
    if (elapsedMilliseconds <= 0)
    {
        return start;
    }

    const double progress = static_cast<double>(elapsedMilliseconds)
        / static_cast<double>(durationMilliseconds);
    return start + (target - start) * progress;
}

std::string makeSoundResourceCacheKey(
    const std::string_view soundName,
    const std::span<const std::filesystem::path> roots,
    const std::span<const std::filesystem::path> archives)
{
    std::string key;
    key.reserve(soundName.size() + (roots.size() + archives.size()) * 48U);
    appendCacheKeyPart(key, soundName);
    appendCacheKeyPaths(key, roots);
    appendCacheKeyPaths(key, archives);
    return key;
}

} // namespace sed::ui::audio
