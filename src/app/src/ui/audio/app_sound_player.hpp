#pragma once

#include "ui/audio/playback_lease_gate.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>
#ifdef SED_APP_MAIN_WINDOW_TEST_ACCESS
#include <functional>
#endif

namespace sed::assets
{
class ResourceLocator;
}

namespace sed::ui::audio
{

/**
 * Plays one logically owned sound resource at a time. Ambient viewport sounds
 * use 500 ms volume ramps and briefly overlap during a crossfade, while modal
 * resource previews remain immediate. Decoded PCM is retained in a bounded
 * cache, including decompressed IJIM INDIWV resources.
 */
class AppSoundPlayer final
{
public:
    static AppSoundPlayer& instance() noexcept;

    AppSoundPlayer(const AppSoundPlayer&) = delete;
    AppSoundPlayer& operator=(const AppSoundPlayer&) = delete;

    bool playResource(
        SoundPlaybackOwner owner,
        const assets::ResourceLocator& resources,
        const std::string& soundName,
        bool loop,
        double volume,
        std::uint64_t lease = 0U);
    bool playBytes(
        SoundPlaybackOwner owner,
        std::string soundKey,
        std::vector<std::byte> bytes,
        bool loop,
        double volume,
        std::uint64_t lease = 0U);

    /**
     * Changes the active owner's normalized volume. Resource-picker changes
     * are immediate; viewport changes retain the ambient fade behavior.
     * Returns false when another feature owns the shared sound channel.
     */
    bool setPlaybackVolume(SoundPlaybackOwner owner, double volume,
        std::uint64_t lease = 0U) noexcept;

    /**
     * Rewinds and plays the active owner's sound at the supplied normalized
     * volume, even when that one-shot sound is still playing.
     */
    bool replay(SoundPlaybackOwner owner, double volume,
        std::uint64_t lease = 0U) noexcept;

    void stop(SoundPlaybackOwner owner, std::uint64_t lease = 0U) noexcept;
    void stopAll() noexcept;
    /** Stops playback and releases Qt multimedia objects before QApplication exits. */
    void shutdown() noexcept;
    bool isOwnedBy(SoundPlaybackOwner owner, std::uint64_t lease = 0U) const noexcept;
    bool hasPreviewOwner() const noexcept;
    bool isPlaying(SoundPlaybackOwner owner, std::uint64_t lease = 0U) const noexcept;

#ifdef SED_APP_MAIN_WINDOW_TEST_ACCESS
    enum class RequestOperationForTest
    {
        playResource,
        playBytes,
        volume,
        replay,
        stop,
    };
    struct RequestForTest
    {
        RequestOperationForTest operation{};
        SoundPlaybackOwner owner{};
        std::uint64_t lease{};
        double volume{};
        bool loop{};
        std::string soundName{};
    };
    void setRequestObserverForTest(std::function<void(const RequestForTest&)> observer);
    /** Replaces the physical sink only, after normal resource decoding and owner acquisition. */
    void setDeviceFreePlaybackForTest(bool enabled);
    double playbackVolumeForTest() const noexcept;
#endif

private:
    struct Impl;

    AppSoundPlayer();
    ~AppSoundPlayer();

    bool matches(SoundPlaybackOwner owner, const std::string& soundKey, bool loop,
        std::uint64_t lease) const noexcept;
    void applyVolume(double volume, bool smoothTransition) noexcept;

    std::unique_ptr<Impl> impl_;
    std::string soundKey_{};
    detail::PlaybackLeaseGate playbackLease_{};
    bool loop_{};
#ifdef SED_APP_MAIN_WINDOW_TEST_ACCESS
    std::function<void(const RequestForTest&)> requestObserverForTest_{};
    bool deviceFreePlaybackForTest_{};
    bool playbackRunningForTest_{};
    double playbackVolumeForTest_{};
#endif
};

} // namespace sed::ui::audio
