#include "ui/audio/app_sound_player.hpp"

#include "app_logging.hpp"
#include "ui/audio/app_sound_player_pcm_device.hpp"
#include "ui/audio/audio_transition_utils.hpp"
#include "ui/audio/audio_resource_identity_utils.hpp"

#include "sed/assets/indy_wv.hpp"
#include "sed/assets/resource_locator.hpp"
#include "sed/assets/wav.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <memory>
#include <span>
#include <string_view>
#include <unordered_map>
#include <utility>

#include <QByteArray>
#include <QCoreApplication>
#include <QDebug>
#include <QtGlobal>

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#include <QAudioDevice>
#include <QAudioFormat>
#include <QAudioSink>
#include <QElapsedTimer>
#include <QMediaDevices>
#include <QTimer>
#else
#include <QBuffer>
#include <QMediaContent>
#include <QMediaPlayer>
#include <QUrl>
#endif

namespace sed::ui::audio
{
namespace
{

double normalizedPlaybackVolume(const double volume) noexcept
{
    return std::isfinite(volume)
        ? std::clamp(volume, 0.0, 1.0)
        : 0.0;
}

bool isRiffWave(const std::span<const std::byte> bytes) noexcept
{
    constexpr std::array<std::byte, 4> kRiff{
        std::byte{'R'}, std::byte{'I'}, std::byte{'F'}, std::byte{'F'},
    };
    constexpr std::array<std::byte, 4> kWave{
        std::byte{'W'}, std::byte{'A'}, std::byte{'V'}, std::byte{'E'},
    };
    return bytes.size() >= 12U
        && std::equal(kRiff.begin(), kRiff.end(), bytes.begin())
        && std::equal(kWave.begin(), kWave.end(), bytes.begin() + 8);
}

std::optional<std::vector<std::byte>> prepareWavBytes(std::vector<std::byte> bytes)
{
    if (assets::isIndyWvSound(bytes))
    {
        return assets::convertIndyWvToWav(bytes);
    }
    if (!isRiffWave(bytes))
    {
        return std::nullopt;
    }
    return bytes;
}

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)

constexpr int kFadeTimerIntervalMs = 16;
constexpr std::size_t kMaxFadingVoiceCount = 4;
constexpr std::size_t kMaxDecodedSoundCacheEntries = 64;
constexpr std::size_t kMaxDecodedSoundCacheBytes = 64U * 1024U * 1024U;
constexpr double kVolumeComparisonEpsilon = 1.0e-6;
constexpr std::int64_t kFallbackSinkBufferDurationUs = 250'000;

QAudioFormat::SampleFormat toQtSampleFormat(const assets::PcmSampleFormat format) noexcept
{
    switch (format)
    {
        case assets::PcmSampleFormat::unsigned8: return QAudioFormat::UInt8;
        case assets::PcmSampleFormat::signed16:  return QAudioFormat::Int16;
        case assets::PcmSampleFormat::signed32:  return QAudioFormat::Int32;
        case assets::PcmSampleFormat::float32:   return QAudioFormat::Float;
    }
    return QAudioFormat::Unknown;
}

QAudioFormat makeAudioFormat(const assets::PcmWave& wave) noexcept
{
    QAudioFormat format;
    format.setSampleRate(static_cast<int>(wave.sampleRate));
    format.setChannelCount(static_cast<int>(wave.channelCount));
    format.setSampleFormat(toQtSampleFormat(wave.sampleFormat));
    return format;
}

qint64 sinkDrainPaddingBytes(
    const QAudioFormat& format,
    const qsizetype sinkBufferFrames) noexcept
{
    const qint64 bytesPerFrame = format.bytesPerFrame();
    if (bytesPerFrame <= 0)
    {
        return 0;
    }

    // QAudioSink's QIODevice path reads ahead through a ring buffer. Keep a
    // one-buffer silent runway after a one-shot so its final real frames and
    // any resampler delay reach the device before source starvation puts the
    // sink in IdleState. Qt documents 250 ms as the default ring-buffer size;
    // use that only when a backend does not report its frame count.
    const qint64 fallbackFrames = format.framesForDuration(kFallbackSinkBufferDurationUs);
    const qint64 frameCount = sinkBufferFrames > 0
        ? static_cast<qint64>(sinkBufferFrames)
        : fallbackFrames;
    if (frameCount <= 0
        || frameCount > std::numeric_limits<qint64>::max() / bytesPerFrame)
    {
        return 0;
    }
    return frameCount * bytesPerFrame;
}

qsizetype reportedSinkBufferFrames(
    const QAudioSink& sink,
    const QAudioFormat& format) noexcept
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 10, 0)
    return sink.bufferFrameCount();
#else
    const int bytesPerFrame = format.bytesPerFrame();
    return bytesPerFrame > 0
        ? sink.bufferSize() / bytesPerFrame
        : 0;
#endif
}

char pcmSilenceByte(const QAudioFormat& format) noexcept
{
    // Unsigned PCM is centred at 0x80. Every signed integer and floating-point
    // format accepted by preparePcmWave represents silence with all-zero bytes.
    return format.sampleFormat() == QAudioFormat::UInt8
        ? static_cast<char>(0x80)
        : char{};
}

/** Audio format and sample bytes retained by the decoded ambient-sound cache. */
struct PreparedPcm final
{
    QAudioFormat format;
    QByteArray samples;
};

std::shared_ptr<const PreparedPcm> preparePcmWave(assets::PcmWave wave)
{
    if (wave.samples.empty()
        || wave.samples.size() > static_cast<std::size_t>(std::numeric_limits<qsizetype>::max()))
    {
        return {};
    }

    QAudioFormat format = makeAudioFormat(wave);
    if (!format.isValid())
    {
        return {};
    }

    auto prepared = std::make_shared<PreparedPcm>();
    prepared->format = std::move(format);
    prepared->samples = QByteArray(
        reinterpret_cast<const char*>(wave.samples.data()),
        static_cast<qsizetype>(wave.samples.size()));
    return prepared;
}

/** Owns one Qt audio sink and its independently ramped volume. */
class AudioVoice final
{
public:
    ~AudioVoice()
    {
        reset();
    }

    bool start(
        std::shared_ptr<const PreparedPcm> prepared,
        const bool loop,
        const double initialVolume)
    {
        reset();
        if (!prepared || prepared->samples.isEmpty())
        {
            return false;
        }

        const QAudioDevice outputDevice = QMediaDevices::defaultAudioOutput();
        if (outputDevice.isNull())
        {
            qCWarning(logResources)
                << "Sound playback failed: no default audio output device";
            return false;
        }
        if (!outputDevice.isFormatSupported(prepared->format))
        {
            qCWarning(logResources).noquote()
                << "Sound playback failed: output device"
                << outputDevice.description()
                << "does not support" << prepared->format;
            return false;
        }

        prepared_ = std::move(prepared);
        sink_ = std::make_unique<QAudioSink>(outputDevice, prepared_->format);
        if (sink_->isNull())
        {
            qCWarning(logResources)
                << "Sound playback failed: Qt could not create an audio sink";
            reset();
            return false;
        }

        const qint64 drainPadding = loop
            ? 0
            : sinkDrainPaddingBytes(
                prepared_->format,
                reportedSinkBufferFrames(*sink_, prepared_->format));
        if (!audioDevice_.load(
                prepared_->samples,
                loop,
                drainPadding,
                pcmSilenceByte(prepared_->format)))
        {
            qCWarning(logResources)
                << "Sound playback failed: could not open decoded PCM stream";
            reset();
            return false;
        }

        QObject::connect(
            sink_.get(),
            &QAudioSink::stateChanged,
            sink_.get(),
            [this](const QAudio::State state)
            {
                if (!stopping_
                    && state == QAudio::StoppedState
                    && sink_
                    && sink_->error() != QAudio::NoError)
                {
                    qCWarning(logResources)
                        << "Sound playback failed: audio sink error"
                        << sink_->error();
                }
            });

        setVolumeImmediately(initialVolume);
        sink_->start(&audioDevice_);
        return true;
    }

    void reset() noexcept
    {
        stopping_ = true;
        if (sink_)
        {
            // Unlike stop(), reset() never waits for or preserves queued audio.
            // Selecting another preview and Replay must not emit the old tail.
            sink_->reset();
            sink_.reset();
        }
        audioDevice_.clear();
        prepared_.reset();
        currentVolume_ = 0.0;
        startVolume_ = 0.0;
        targetVolume_ = 0.0;
        fadeStartedAtMs_ = 0;
        fading_ = false;
        stopWhenFadeCompletes_ = false;
        stopping_ = false;
    }

    void setVolumeImmediately(const double volume) noexcept
    {
        const double clamped = normalizedPlaybackVolume(volume);
        currentVolume_ = clamped;
        startVolume_ = clamped;
        targetVolume_ = clamped;
        fading_ = false;
        stopWhenFadeCompletes_ = false;
        if (sink_)
        {
            sink_->setVolume(static_cast<qreal>(clamped));
        }
    }

    void beginFade(
        const double targetVolume,
        const std::int64_t nowMilliseconds,
        const bool stopWhenComplete) noexcept
    {
        if (!sink_)
        {
            return;
        }

        const double target = normalizedPlaybackVolume(targetVolume);
        stopWhenFadeCompletes_ = stopWhenComplete;

        // The viewport polls every 125 ms. Preserve an existing ramp when its
        // target is unchanged instead of repeatedly restarting it.
        if (std::abs(targetVolume_ - target) <= kVolumeComparisonEpsilon)
        {
            if (!fading_ && stopWhenComplete && currentVolume_ <= kVolumeComparisonEpsilon)
            {
                stopPlayback();
            }
            return;
        }

        startVolume_ = currentVolume_;
        targetVolume_ = target;
        fadeStartedAtMs_ = nowMilliseconds;
        fading_ = std::abs(startVolume_ - targetVolume_) > kVolumeComparisonEpsilon;
        if (!fading_)
        {
            setCurrentVolume(targetVolume_);
            if (stopWhenFadeCompletes_)
            {
                stopPlayback();
            }
        }
    }

    /** Advances the current ramp and reports when an outgoing voice is done. */
    bool advanceFade(const std::int64_t nowMilliseconds) noexcept
    {
        if (!fading_)
        {
            return stopWhenFadeCompletes_ && isStopped();
        }

        const std::int64_t elapsed = nowMilliseconds - fadeStartedAtMs_;
        setCurrentVolume(interpolateSoundVolume(
            startVolume_,
            targetVolume_,
            elapsed,
            kAmbientSoundFadeDurationMs));
        if (elapsed < kAmbientSoundFadeDurationMs)
        {
            return false;
        }

        fading_ = false;
        if (stopWhenFadeCompletes_)
        {
            stopPlayback();
            return true;
        }
        return false;
    }

    void stopPlayback() noexcept
    {
        if (!sink_)
        {
            return;
        }
        stopping_ = true;
        sink_->stop();
        stopping_ = false;
    }

    void restart() noexcept
    {
        if (!sink_)
        {
            return;
        }
        stopping_ = true;
        sink_->reset();
        audioDevice_.rewind();
        sink_->setVolume(static_cast<qreal>(currentVolume_));
        sink_->start(&audioDevice_);
        stopping_ = false;
    }

    bool isStopped() const noexcept
    {
        return !sink_
            || sink_->state() == QAudio::StoppedState
            || sink_->state() == QAudio::IdleState;
    }

    bool isFading() const noexcept
    {
        return fading_;
    }

private:
    void setCurrentVolume(const double volume) noexcept
    {
        currentVolume_ = normalizedPlaybackVolume(volume);
        if (sink_)
        {
            sink_->setVolume(static_cast<qreal>(currentVolume_));
        }
    }

    std::shared_ptr<const PreparedPcm> prepared_;
    detail::PcmAudioDevice audioDevice_;
    std::unique_ptr<QAudioSink> sink_;
    double currentVolume_{};
    double startVolume_{};
    double targetVolume_{};
    std::int64_t fadeStartedAtMs_{};
    bool fading_{};
    bool stopWhenFadeCompletes_{};
    bool stopping_{};
};

#endif

} // namespace

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)

struct AppSoundPlayer::Impl final
{
    struct CacheEntry final
    {
        std::shared_ptr<const PreparedPcm> prepared;
        std::size_t byteSize{};
        std::uint64_t lastUse{};
    };

    Impl()
    {
        clock.start();
        fadeTimer.setInterval(kFadeTimerIntervalMs);
        fadeTimer.setTimerType(Qt::PreciseTimer);
        QObject::connect(
            &fadeTimer,
            &QTimer::timeout,
            &fadeTimer,
            [this]
            {
                advanceFades();
            });
    }

    ~Impl()
    {
        detachMedia();
    }

    bool setMedia(
        std::shared_ptr<const PreparedPcm> prepared,
        const bool shouldLoop,
        const double volume,
        const bool smoothTransition)
    {
        if (!prepared || prepared->samples.isEmpty())
        {
            qCWarning(logResources)
                << "Sound playback failed: decoded PCM payload is empty or too large";
            return false;
        }

        auto nextVoice = std::make_unique<AudioVoice>();
        const double initialVolume = smoothTransition ? 0.0 : volume;
        if (!nextVoice->start(std::move(prepared), shouldLoop, initialVolume))
        {
            return false;
        }

        if (!smoothTransition)
        {
            detachMedia();
            currentVoice = std::move(nextVoice);
            return true;
        }

        const std::int64_t now = clock.elapsed();
        moveCurrentVoiceToFadeOut(now);
        currentVoice = std::move(nextVoice);
        currentVoice->beginFade(volume, now, false);
        ensureFadeTimerRunning();
        return true;
    }

    std::shared_ptr<const PreparedPcm> findCached(const std::string_view key)
    {
        const auto item = decodedCache.find(std::string{key});
        if (item == decodedCache.end())
        {
            return {};
        }
        item->second.lastUse = ++cacheUseCounter;
        return item->second.prepared;
    }

    void cache(
        std::string key,
        std::shared_ptr<const PreparedPcm> prepared)
    {
        if (!prepared)
        {
            return;
        }

        const std::size_t byteSize = static_cast<std::size_t>(prepared->samples.size());
        if (byteSize > kMaxDecodedSoundCacheBytes)
        {
            return;
        }

        if (const auto existing = decodedCache.find(key); existing != decodedCache.end())
        {
            decodedCacheBytes -= existing->second.byteSize;
            decodedCache.erase(existing);
        }

        decodedCacheBytes += byteSize;
        decodedCache.emplace(
            std::move(key),
            CacheEntry{
                .prepared = std::move(prepared),
                .byteSize = byteSize,
                .lastUse = ++cacheUseCounter,
            });
        evictDecodedCacheEntries();
    }

    void detachMedia() noexcept
    {
        fadeTimer.stop();
        if (currentVoice)
        {
            currentVoice->reset();
            currentVoice.reset();
        }
        fadingVoices.clear();
    }

    void fadeOutCurrent() noexcept
    {
        if (!currentVoice || currentVoice->isStopped())
        {
            return;
        }
        currentVoice->beginFade(0.0, clock.elapsed(), true);
        ensureFadeTimerRunning();
    }

    void setVolume(const double volume, const bool smoothTransition) noexcept
    {
        if (!currentVoice)
        {
            return;
        }
        if (smoothTransition)
        {
            currentVoice->beginFade(volume, clock.elapsed(), false);
            ensureFadeTimerRunning();
            return;
        }
        currentVoice->setVolumeImmediately(volume);
    }

    bool isStopped() const noexcept
    {
        return !currentVoice || currentVoice->isStopped();
    }

    void play() noexcept
    {
        if (currentVoice && currentVoice->isStopped())
        {
            currentVoice->restart();
        }
    }

    void restart() noexcept
    {
        if (currentVoice)
        {
            currentVoice->restart();
        }
    }

private:
    void moveCurrentVoiceToFadeOut(const std::int64_t nowMilliseconds)
    {
        if (!currentVoice)
        {
            return;
        }
        if (currentVoice->isStopped())
        {
            currentVoice.reset();
            return;
        }

        currentVoice->beginFade(0.0, nowMilliseconds, true);
        if (fadingVoices.size() >= kMaxFadingVoiceCount)
        {
            fadingVoices.front()->reset();
            fadingVoices.erase(fadingVoices.begin());
        }
        fadingVoices.push_back(std::move(currentVoice));
    }

    void ensureFadeTimerRunning()
    {
        if (!fadeTimer.isActive()
            && ((currentVoice && currentVoice->isFading()) || !fadingVoices.empty()))
        {
            fadeTimer.start();
        }
    }

    void advanceFades()
    {
        const std::int64_t now = clock.elapsed();
        if (currentVoice)
        {
            currentVoice->advanceFade(now);
        }

        std::erase_if(
            fadingVoices,
            [now](const std::unique_ptr<AudioVoice>& voice)
            {
                return voice->advanceFade(now) || voice->isStopped();
            });

        if ((!currentVoice || !currentVoice->isFading()) && fadingVoices.empty())
        {
            fadeTimer.stop();
        }
    }

    void evictDecodedCacheEntries()
    {
        while (decodedCache.size() > kMaxDecodedSoundCacheEntries
               || decodedCacheBytes > kMaxDecodedSoundCacheBytes)
        {
            const auto oldest = std::min_element(
                decodedCache.begin(),
                decodedCache.end(),
                [](const auto& lhs, const auto& rhs)
                {
                    return lhs.second.lastUse < rhs.second.lastUse;
                });
            if (oldest == decodedCache.end())
            {
                break;
            }
            decodedCacheBytes -= oldest->second.byteSize;
            decodedCache.erase(oldest);
        }
    }

    QElapsedTimer clock;
    QTimer fadeTimer;
    std::unique_ptr<AudioVoice> currentVoice;
    std::vector<std::unique_ptr<AudioVoice>> fadingVoices;
    std::unordered_map<std::string, CacheEntry> decodedCache;
    std::size_t decodedCacheBytes{};
    std::uint64_t cacheUseCounter{};
};

#else

struct AppSoundPlayer::Impl final
{
    QBuffer buffer;
    QMediaPlayer player;
    bool loop{};

    Impl()
    {
        QObject::connect(
            &player,
            &QMediaPlayer::mediaStatusChanged,
            &player,
            [this](const QMediaPlayer::MediaStatus status)
            {
                if (loop && status == QMediaPlayer::EndOfMedia)
                {
                    player.setPosition(0);
                    player.play();
                }
            });
        QObject::connect(
            &player,
            QOverload<QMediaPlayer::Error>::of(&QMediaPlayer::error),
            &player,
            [this](const QMediaPlayer::Error error)
            {
                if (error != QMediaPlayer::NoError)
                {
                    qCWarning(logResources).noquote()
                        << "Sound playback failed:" << player.errorString();
                }
            });
    }

    bool setMedia(const std::span<const std::byte> bytes, const bool shouldLoop)
    {
        if (bytes.size() > static_cast<std::size_t>(std::numeric_limits<int>::max()))
        {
            return false;
        }

        detachMedia();
        buffer.setData(QByteArray(
            reinterpret_cast<const char*>(bytes.data()),
            static_cast<int>(bytes.size())));
        if (!buffer.open(QIODevice::ReadOnly))
        {
            buffer.setData({});
            return false;
        }

        const QUrl sourceHint(QStringLiteral("memory:/sed-preview.wav"));
        loop = shouldLoop;
        player.setMedia(QMediaContent(sourceHint), &buffer);
        return true;
    }

    void detachMedia() noexcept
    {
        player.stop();
        player.setMedia({});
        buffer.close();
        buffer.setData({});
        loop = false;
    }

    void setVolume(const double volume) noexcept
    {
        const double clamped = normalizedPlaybackVolume(volume);
        player.setVolume(static_cast<int>(std::lround(clamped * 100.0)));
    }

    bool isStopped() const noexcept
    {
        return player.state() == QMediaPlayer::StoppedState;
    }

    void play()
    {
        if (isStopped())
        {
            player.setPosition(0);
        }
        player.play();
    }


    void restart()
    {
        player.stop();
        player.setPosition(0);
        player.play();
    }
};

#endif

AppSoundPlayer& AppSoundPlayer::instance() noexcept
{
    static AppSoundPlayer player;
    return player;
}

AppSoundPlayer::AppSoundPlayer()
{
}

AppSoundPlayer::~AppSoundPlayer()
{
    stopAll();
}

bool AppSoundPlayer::matches(
    const SoundPlaybackOwner owner,
    const std::string& soundKey,
    const bool loop,
    const std::uint64_t lease) const noexcept
{
    return playbackLease_.owns(owner, lease) && loop_ == loop && soundKey_ == soundKey;
}

#ifdef SED_APP_MAIN_WINDOW_TEST_ACCESS
void AppSoundPlayer::setRequestObserverForTest(std::function<void(const RequestForTest&)> observer)
{
    requestObserverForTest_ = std::move(observer);
}

void AppSoundPlayer::setDeviceFreePlaybackForTest(const bool enabled)
{
    stopAll();
    deviceFreePlaybackForTest_ = enabled;
}

double AppSoundPlayer::playbackVolumeForTest() const noexcept
{
    return playbackVolumeForTest_;
}
#endif

bool AppSoundPlayer::playResource(
    const SoundPlaybackOwner owner,
    const assets::ResourceLocator& resources,
    const std::string& soundName,
    const bool loop,
    const double volume,
    const std::uint64_t lease)
{
#ifdef SED_APP_MAIN_WINDOW_TEST_ACCESS
    if ( requestObserverForTest_ )
    {
        requestObserverForTest_(RequestForTest{RequestOperationForTest::playResource, owner, lease, volume, loop, soundName});
    }
#endif
    if (soundName.empty())
    {
        stop(owner, lease);
        return false;
    }

    std::string resourceKey = soundResourceRequestKey(resources, soundName);

    const bool smoothTransition = owner == SoundPlaybackOwner::viewport;
    if (matches(owner, resourceKey, loop, lease))
    {
        applyVolume(volume, smoothTransition);
#ifdef SED_APP_MAIN_WINDOW_TEST_ACCESS
        if ( deviceFreePlaybackForTest_ )
        {
            playbackRunningForTest_ = true;
            return true;
        }
#endif
        if (impl_ && impl_->isStopped())
        {
            impl_->play();
        }
        return true;
    }

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    if (!QCoreApplication::instance())
    {
        qCWarning(logResources).noquote()
            << "Sound playback failed: no Qt application:" << soundName;
        stop(owner, lease);
        return false;
    }
    if (!impl_)
    {
        impl_ = std::make_unique<Impl>();
    }

    std::shared_ptr<const PreparedPcm> prepared = impl_->findCached(resourceKey);
    if (!prepared)
    {
        auto bytes = readSoundResource(resources, soundName);
        if (!bytes)
        {
            qCWarning(logResources).noquote()
                << "Sound playback failed: resource not found:" << soundName;
            stop(owner, lease);
            return false;
        }

        auto wavBytes = prepareWavBytes(std::move(*bytes));
        if (!wavBytes)
        {
            qCWarning(logResources).noquote()
                << "Sound playback failed: invalid WAV resource:" << soundName;
            stop(owner, lease);
            return false;
        }

        auto pcm = assets::decodePcmWave(*wavBytes);
        if (!pcm)
        {
            qCWarning(logResources).noquote()
                << "Sound playback failed: unsupported WAV encoding:" << soundName;
            stop(owner, lease);
            return false;
        }

        prepared = preparePcmWave(std::move(*pcm));
        if (!prepared)
        {
            qCWarning(logResources).noquote()
                << "Sound playback failed: invalid decoded PCM:" << soundName;
            stop(owner, lease);
            return false;
        }
        impl_->cache(resourceKey, prepared);
    }

#ifdef SED_APP_MAIN_WINDOW_TEST_ACCESS
    if ( deviceFreePlaybackForTest_ )
    {
        soundKey_ = std::move(resourceKey);
        playbackLease_.acquire(owner, lease);
        loop_ = loop;
        playbackRunningForTest_ = true;
        playbackVolumeForTest_ = normalizedPlaybackVolume(volume);
        return true;
    }
#endif
    if (!impl_->setMedia(prepared, loop, volume, smoothTransition))
    {
        stop(owner, lease);
        return false;
    }

    soundKey_ = std::move(resourceKey);
    playbackLease_.acquire(owner, lease);
    loop_ = loop;
    return true;
#else
    auto bytes = readSoundResource(resources, soundName);
    if (!bytes)
    {
        qCWarning(logResources).noquote()
            << "Sound playback failed: resource not found:" << soundName;
        stop(owner, lease);
        return false;
    }

    return playBytes(owner, std::move(resourceKey), std::move(*bytes), loop, volume, lease);
#endif
}

bool AppSoundPlayer::playBytes(
    const SoundPlaybackOwner owner,
    std::string soundKey,
    std::vector<std::byte> bytes,
    const bool loop,
    const double volume,
    const std::uint64_t lease)
{
#ifdef SED_APP_MAIN_WINDOW_TEST_ACCESS
    if ( requestObserverForTest_ )
    {
        requestObserverForTest_(RequestForTest{RequestOperationForTest::playBytes, owner, lease, volume, loop, soundKey});
    }
#endif
    if (soundKey.empty() || bytes.empty())
    {
        stop(owner, lease);
        return false;
    }

    std::string requestKey = soundBytesRequestKey(soundKey, bytes);

    const bool smoothTransition = owner == SoundPlaybackOwner::viewport;
    if (matches(owner, requestKey, loop, lease))
    {
        applyVolume(volume, smoothTransition);
#ifdef SED_APP_MAIN_WINDOW_TEST_ACCESS
        if ( deviceFreePlaybackForTest_ )
        {
            playbackRunningForTest_ = true;
            return true;
        }
#endif
        if (impl_ && impl_->isStopped())
        {
            impl_->play();
        }
        return true;
    }

    auto wavBytes = prepareWavBytes(std::move(bytes));
    if (!wavBytes || !QCoreApplication::instance())
    {
        qCWarning(logResources).noquote()
            << "Sound playback failed: invalid WAV resource:" << soundKey;
        stop(owner, lease);
        return false;
    }

    if (!impl_)
    {
        impl_ = std::make_unique<Impl>();
    }
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    std::shared_ptr<const PreparedPcm> prepared = impl_->findCached(requestKey);
    if (!prepared)
    {
        auto pcm = assets::decodePcmWave(*wavBytes);
        if (!pcm)
        {
            qCWarning(logResources).noquote()
                << "Sound playback failed: unsupported WAV encoding:" << soundKey;
            stop(owner, lease);
            return false;
        }
        prepared = preparePcmWave(std::move(*pcm));
        if (!prepared)
        {
            qCWarning(logResources).noquote()
                << "Sound playback failed: invalid decoded PCM:" << soundKey;
            stop(owner, lease);
            return false;
        }
        impl_->cache(requestKey, prepared);
    }
#ifdef SED_APP_MAIN_WINDOW_TEST_ACCESS
    if ( deviceFreePlaybackForTest_ )
    {
        soundKey_ = std::move(requestKey);
        playbackLease_.acquire(owner, lease);
        loop_ = loop;
        playbackRunningForTest_ = true;
        playbackVolumeForTest_ = normalizedPlaybackVolume(volume);
        return true;
    }
#endif
    if (!impl_->setMedia(std::move(prepared), loop, volume, smoothTransition))
#else
    if (!impl_->setMedia(*wavBytes, loop))
#endif
    {
        return false;
    }

    soundKey_ = std::move(requestKey);
    playbackLease_.acquire(owner, lease);
    loop_ = loop;
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    applyVolume(volume, false);
    impl_->play();
#endif
    return true;
}

bool AppSoundPlayer::setPlaybackVolume(
    const SoundPlaybackOwner owner,
    const double volume,
    const std::uint64_t lease) noexcept
{
#ifdef SED_APP_MAIN_WINDOW_TEST_ACCESS
    if ( requestObserverForTest_ )
    {
        requestObserverForTest_(RequestForTest{RequestOperationForTest::volume, owner, lease, volume, loop_});
    }
#endif
    if (!isOwnedBy(owner, lease) || !impl_)
    {
        return false;
    }

    applyVolume(volume, owner == SoundPlaybackOwner::viewport);
    return true;
}

bool AppSoundPlayer::replay(
    const SoundPlaybackOwner owner,
    const double volume,
    const std::uint64_t lease) noexcept
{
#ifdef SED_APP_MAIN_WINDOW_TEST_ACCESS
    if ( requestObserverForTest_ )
    {
        requestObserverForTest_(RequestForTest{RequestOperationForTest::replay, owner, lease, volume, loop_});
    }
#endif
    if (!setPlaybackVolume(owner, volume, lease))
    {
        return false;
    }

#ifdef SED_APP_MAIN_WINDOW_TEST_ACCESS
    if ( deviceFreePlaybackForTest_ )
    {
        playbackRunningForTest_ = true;
        return true;
    }
#endif
    impl_->restart();
    return true;
}

void AppSoundPlayer::stop(const SoundPlaybackOwner owner, const std::uint64_t lease) noexcept
{
#ifdef SED_APP_MAIN_WINDOW_TEST_ACCESS
    if ( requestObserverForTest_ )
    {
        requestObserverForTest_(RequestForTest{RequestOperationForTest::stop, owner, lease, 0.0, loop_});
    }
#endif
    if (playbackLease_.owns(owner, lease))
    {
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
        if (owner == SoundPlaybackOwner::viewport && impl_)
        {
            impl_->fadeOutCurrent();
            return;
        }
#endif
        stopAll();
    }
}

void AppSoundPlayer::stopAll() noexcept
{
    if (impl_)
    {
        impl_->detachMedia();
    }
    soundKey_.clear();
    playbackLease_.clear();
    loop_ = false;
#ifdef SED_APP_MAIN_WINDOW_TEST_ACCESS
    playbackRunningForTest_ = false;
    playbackVolumeForTest_ = 0.0;
#endif
}

void AppSoundPlayer::shutdown() noexcept
{
    stopAll();
    impl_.reset();
}

bool AppSoundPlayer::isOwnedBy(const SoundPlaybackOwner owner, const std::uint64_t lease) const noexcept
{
    return playbackLease_.owns(owner, lease);
}

bool AppSoundPlayer::hasPreviewOwner() const noexcept
{
#ifdef SED_APP_MAIN_WINDOW_TEST_ACCESS
    if ( deviceFreePlaybackForTest_ )
    {
        return playbackLease_.hasPreviewOwner(playbackRunningForTest_);
    }
#endif
    return playbackLease_.hasPreviewOwner(impl_ && !impl_->isStopped());
}

bool AppSoundPlayer::isPlaying(const SoundPlaybackOwner owner, const std::uint64_t lease) const noexcept
{
#ifdef SED_APP_MAIN_WINDOW_TEST_ACCESS
    if ( deviceFreePlaybackForTest_ )
    {
        return playbackLease_.ownsActivePlayback(owner, lease, playbackRunningForTest_);
    }
#endif
    return playbackLease_.ownsActivePlayback(owner, lease, impl_ && !impl_->isStopped());
}

void AppSoundPlayer::applyVolume(
    const double volume,
    const bool smoothTransition) noexcept
{
#ifdef SED_APP_MAIN_WINDOW_TEST_ACCESS
    if ( deviceFreePlaybackForTest_ )
    {
        playbackVolumeForTest_ = normalizedPlaybackVolume(volume);
        return;
    }
#endif
    if (impl_)
    {
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
        impl_->setVolume(volume, smoothTransition);
#else
        impl_->setVolume(volume);
#endif
    }
}

} // namespace sed::ui::audio
