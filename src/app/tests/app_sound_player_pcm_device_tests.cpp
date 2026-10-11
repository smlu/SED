#include "test_common.hpp"

#include "ui/audio/app_sound_player_pcm_device.hpp"
#include "ui/audio/app_sound_player.hpp"

#include <array>
#include <limits>
#include <string_view>

#include <QByteArray>

namespace sed::app_tests
{

void checkSoundPreviewOwnerLease()
{
    using ui::audio::SoundPlaybackOwner;
    ui::audio::detail::PlaybackLeaseGate gate{};
    check(!gate.owns(SoundPlaybackOwner::viewport),
        "resource workspace audio owner: empty channel rejects control requests");
    gate.acquire(SoundPlaybackOwner::viewport);
    check(gate.owns(SoundPlaybackOwner::viewport)
            && !gate.owns(SoundPlaybackOwner::resourcePicker),
        "resource workspace audio owner: legacy viewport ownership remains isolated");
    gate.acquire(SoundPlaybackOwner::resourcePicker);
    check(gate.owns(SoundPlaybackOwner::resourcePicker)
            && !gate.owns(SoundPlaybackOwner::viewport),
        "resource workspace audio owner: legacy picker acquisition replaces the viewport");
    gate.acquire(SoundPlaybackOwner::resourceEditor, 11U);
    check(gate.owns(SoundPlaybackOwner::resourceEditor, 11U)
            && !gate.owns(SoundPlaybackOwner::resourcePicker),
        "resource workspace audio lease: a pane acquisition excludes other features");
    gate.acquire(SoundPlaybackOwner::resourceEditor, 22U);
    check(gate.owns(SoundPlaybackOwner::resourceEditor, 22U)
            && !gate.owns(SoundPlaybackOwner::resourceEditor, 11U)
            && !gate.owns(SoundPlaybackOwner::resourceEditor),
        "resource workspace audio lease: replacement rejects stale pane stop replay and volume authorization");
    gate.acquire(SoundPlaybackOwner::resourcePicker);
    check(gate.owns(SoundPlaybackOwner::resourcePicker)
            && !gate.owns(SoundPlaybackOwner::resourceEditor, 22U),
        "resource workspace audio lease: picker replacement invalidates the pane lease");
    gate.clear();
    check(!gate.owns(SoundPlaybackOwner::resourcePicker)
            && !gate.owns(SoundPlaybackOwner::resourceEditor, 22U),
        "resource workspace audio owner: stopAll clears authorization");
}

void checkSoundPreviewPriority()
{
    using ui::audio::SoundPlaybackOwner;
    ui::audio::detail::PlaybackLeaseGate gate{};
    check(!gate.hasPreviewOwner(),
        "resource workspace audio priority: an empty channel allows ambient playback");
    gate.acquire(SoundPlaybackOwner::viewport);
    check(!gate.hasPreviewOwner(),
        "resource workspace audio priority: ambient playback retains legacy ownership");
    gate.acquire(SoundPlaybackOwner::resourcePicker);
    check(gate.hasPreviewOwner(),
        "resource workspace audio priority: the existing picker preview blocks ambient playback");
    gate.acquire(SoundPlaybackOwner::resourceEditor, 37U);
    check(gate.hasPreviewOwner(),
        "resource workspace audio priority: a docked pane preview blocks ambient playback");
    check(gate.ownsActivePlayback(SoundPlaybackOwner::resourceEditor, 37U, true)
            && !gate.ownsActivePlayback(SoundPlaybackOwner::resourceEditor, 36U, true)
            && !gate.ownsActivePlayback(SoundPlaybackOwner::resourceEditor, 37U, false),
        "resource workspace audio controls: a preview Stop toggle requires active playback and the exact pane lease");
    check(!gate.hasPreviewOwner(false) && gate.owns(SoundPlaybackOwner::resourceEditor, 37U),
        "resource workspace audio priority: an ended pane one-shot releases ambient priority while retaining its replay lease");
    gate.acquire(SoundPlaybackOwner::resourcePicker);
    check(gate.hasPreviewOwner(false),
        "resource workspace audio priority: an ended modal picker retains its established replay ownership");
    gate.clear();
    check(!gate.hasPreviewOwner(),
        "resource workspace audio priority: releasing a preview permits ambient playback to resume");
}

void runAppSoundPlayerPcmDeviceTests()
{
    checkSoundPreviewOwnerLease();
    checkSoundPreviewPriority();
    using ui::audio::detail::PcmAudioDevice;

    {
        PcmAudioDevice device;
        check(device.load(QByteArray("abc", 3), false, 2, static_cast<char>(0x80)),
              "sound PCM device loads a non-empty one-shot");
        check(device.isSequential(),
              "sound PCM device declares its custom cursor as sequential");
        check(device.bytesAvailable() == 5,
              "sound PCM one-shot reports source plus drain padding");

        std::array<char, 4> firstRead{};
        check(device.read(firstRead.data(), firstRead.size()) == 4,
              "sound PCM one-shot fills a read across the source-padding boundary");
        check(std::string_view(firstRead.data(), 3) == "abc"
                  && static_cast<unsigned char>(firstRead[3]) == 0x80U,
              "sound PCM one-shot emits format-specific silence after source bytes");
        check(device.bytesAvailable() == 1 && !device.atEnd(),
              "sound PCM one-shot retains the remaining drain byte");

        std::array<char, 2> finalRead{};
        check(device.read(finalRead.data(), finalRead.size()) == 1
                  && static_cast<unsigned char>(finalRead[0]) == 0x80U,
              "sound PCM one-shot emits all finite drain padding");
        check(device.atEnd() && device.bytesAvailable() == 0,
              "sound PCM one-shot reaches EOF only after drain padding");

        device.rewind();
        std::array<char, 5> replayRead{};
        check(device.bytesAvailable() == 5
                  && device.read(replayRead.data(), replayRead.size()) == 5,
              "sound PCM rewind restores both source and drain cursors");
        check(std::string_view(replayRead.data(), 3) == "abc"
                  && static_cast<unsigned char>(replayRead[3]) == 0x80U
                  && static_cast<unsigned char>(replayRead[4]) == 0x80U,
              "sound PCM replay emits the source before a fresh silent runway");
    }

    {
        PcmAudioDevice device;
        check(device.load(QByteArray("ab", 2), true, 64, static_cast<char>(0x55)),
              "sound PCM device loads a looping source");
        check(device.bytesAvailable() == std::numeric_limits<qint64>::max(),
              "sound PCM loop advertises an effectively unbounded readable stream");

        std::array<char, 1> firstByte{};
        check(device.read(firstByte.data(), 0) == 0
                  && device.write("x", 1) == -1
                  && device.read(firstByte.data(), firstByte.size()) == 1
                  && firstByte[0] == 'a',
              "sound PCM zero-length reads and rejected writes do not advance its cursor");

        std::array<char, 7> loopRead{};
        check(device.read(loopRead.data(), loopRead.size()) == 7,
              "sound PCM loop fills reads larger than its source");
        check(std::string_view(loopRead.data(), loopRead.size()) == "bababab",
              "sound PCM loop is gapless and never includes drain padding");
        check(!device.atEnd(),
              "sound PCM loop never reaches EOF");

        device.rewind();
        std::array<char, 3> replayRead{};
        check(device.read(replayRead.data(), replayRead.size()) == 3
                  && std::string_view(replayRead.data(), replayRead.size()) == "aba",
              "sound PCM rewind after a partial loop restores the source boundary");
    }

    {
        PcmAudioDevice device;
        check(!device.load(QByteArray{}, false, 8, char{}),
              "sound PCM device rejects an empty source");
        check(!device.isOpen() && device.atEnd() && device.bytesAvailable() == 0,
              "sound PCM device keeps rejected empty input closed and exhausted");
    }

    {
        PcmAudioDevice device;
        check(device.load(QByteArray("xy", 2), false, -4, static_cast<char>(0x7f)),
              "sound PCM device accepts a one-shot with negative requested padding");

        std::array<char, 4> read{};
        check(device.bytesAvailable() == 2
                  && device.read(read.data(), read.size()) == 2
                  && std::string_view(read.data(), 2) == "xy"
                  && device.atEnd(),
              "sound PCM device clamps negative drain padding to zero");

        device.clear();
        check(!device.isOpen() && device.atEnd() && device.bytesAvailable() == 0,
              "sound PCM clear closes the device and resets every readable cursor");

        std::array<char, 1> afterClear{};
        check(device.read(afterClear.data(), afterClear.size()) == -1,
              "sound PCM cleared device rejects reads instead of exposing old bytes");
    }

    {
        PcmAudioDevice device;
        check(device.load(QByteArray("old", 3), false, 2, char{}),
              "sound PCM replacement fixture loads its initial source");
        std::array<char, 2> discarded{};
        (void)device.read(discarded.data(), discarded.size());

        check(device.load(QByteArray("new", 3), false, 0, char{}),
              "sound PCM load replaces an already-open source");
        std::array<char, 3> replacement{};
        check(device.bytesAvailable() == 3
                  && device.read(replacement.data(), replacement.size()) == 3
                  && std::string_view(replacement.data(), replacement.size()) == "new"
                  && device.atEnd(),
              "sound PCM replacement clears the old cursor and drain state");
    }

    {
        PcmAudioDevice device;
        check(device.load(
                  QByteArray("x", 1),
                  false,
                  std::numeric_limits<qint64>::max(),
                  char{}),
              "sound PCM device accepts the largest representable drain runway");
        check(device.bytesAvailable() == std::numeric_limits<qint64>::max(),
              "sound PCM bytesAvailable saturates instead of overflowing for an extreme drain runway");
    }
}

} // namespace sed::app_tests
