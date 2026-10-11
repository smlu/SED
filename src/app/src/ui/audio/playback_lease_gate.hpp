#pragma once

#include <cstdint>

namespace sed::ui::audio
{
/** Identifies the UI feature currently owning the editor's single sound channel. */
enum class SoundPlaybackOwner : std::uint8_t
{
    viewport,
    resourcePicker,
    resourceEditor,
};

namespace detail
{
/** Device-independent authorization for the application's single playback owner. */
class PlaybackLeaseGate final
{
public:
    void acquire(const SoundPlaybackOwner owner, const std::uint64_t lease = 0U) noexcept
    {
        owner_ = owner;
        lease_ = lease;
        active_ = true;
    }

    bool ownsOwner(const SoundPlaybackOwner owner) const noexcept
    {
        return active_ && owner_ == owner;
    }

    bool owns(const SoundPlaybackOwner owner, const std::uint64_t lease = 0U) const noexcept
    {
        return ownsOwner(owner) && lease_ == lease;
    }

    bool hasPreviewOwner(const bool playbackRunning = true) const noexcept
    {
        return ownsOwner(SoundPlaybackOwner::resourcePicker) ||
               (ownsOwner(SoundPlaybackOwner::resourceEditor) && playbackRunning);
    }

    bool ownsActivePlayback(const SoundPlaybackOwner owner, const std::uint64_t lease,
                            const bool playbackRunning) const noexcept
    {
        return playbackRunning && owns(owner, lease);
    }

    void clear() noexcept
    {
        active_ = false;
        lease_ = 0U;
    }

private:
    SoundPlaybackOwner owner_{};
    std::uint64_t lease_{};
    bool active_{};
};
} // namespace detail
} // namespace sed::ui::audio
