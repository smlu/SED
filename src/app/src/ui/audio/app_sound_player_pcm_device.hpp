#pragma once

#include <algorithm>
#include <cstring>
#include <limits>
#include <utility>

#include <QByteArray>
#include <QIODevice>

namespace sed::ui::audio::detail
{

/** Supplies one-shot or endlessly looping PCM bytes directly to QAudioSink. */
class PcmAudioDevice final : public QIODevice
{
public:
    bool load(
        QByteArray bytes,
        const bool loop,
        const qint64 drainPaddingBytes,
        const char silenceByte)
    {
        clear();
        if ( bytes.isEmpty() )
        {
            return false;
        }

        bytes_ = std::move(bytes);
        loop_ = loop;
        position_ = 0;
        drainPaddingBytes_ = loop ? 0 : std::max<qint64>(0, drainPaddingBytes);
        drainPaddingPosition_ = 0;
        silenceByte_ = silenceByte;
        // All readable state belongs to this cursor. Avoid QIODevice retaining
        // prefetched tail bytes in a separate buffer that rewind() cannot seek.
        if ( !open(QIODevice::ReadOnly | QIODevice::Unbuffered) )
        {
            clear();
            return false;
        }
        return true;
    }

    void clear() noexcept
    {
        close();
        bytes_.clear();
        position_ = 0;
        drainPaddingBytes_ = 0;
        drainPaddingPosition_ = 0;
        silenceByte_ = {};
        loop_ = false;
    }

    void rewind() noexcept
    {
        position_ = 0;
        drainPaddingPosition_ = 0;
    }

    bool isSequential() const override
    {
        // This device owns its cursor and can synthesize looping or drain-tail
        // bytes; it does not implement QIODevice's random-access seek contract.
        return true;
    }

    bool atEnd() const override
    {
        return !loop_
            && position_ >= bytes_.size()
            && drainPaddingPosition_ >= drainPaddingBytes_;
    }

    qint64 bytesAvailable() const override
    {
        if (loop_)
        {
            // The generator can always satisfy the sink's whole free region;
            // do not make short loops refill the ring buffer one clip at a time.
            return std::numeric_limits<qint64>::max();
        }

        const qint64 sourceRemaining = std::max<qint64>(0, bytes_.size() - position_);
        const qint64 paddingRemaining =
            std::max<qint64>(0, drainPaddingBytes_ - drainPaddingPosition_);
        const qint64 bufferedRemaining =
            std::max<qint64>(0, QIODevice::bytesAvailable());
        constexpr qint64 kMaximum = std::numeric_limits<qint64>::max();
        if ( paddingRemaining > kMaximum - sourceRemaining )
        {
            return kMaximum;
        }

        const qint64 readableRemaining = sourceRemaining + paddingRemaining;
        if ( bufferedRemaining > kMaximum - readableRemaining )
        {
            return kMaximum;
        }
        return readableRemaining + bufferedRemaining;
    }

protected:
    qint64 readData(char* destination, const qint64 maxSize) override
    {
        if (!destination || maxSize <= 0 || bytes_.isEmpty())
        {
            return 0;
        }

        qint64 copied = 0;
        while (copied < maxSize)
        {
            const qint64 remaining = bytes_.size() - position_;
            if (remaining <= 0)
            {
                if (loop_)
                {
                    position_ = 0;
                    continue;
                }

                const qint64 paddingRemaining = drainPaddingBytes_ - drainPaddingPosition_;
                if (paddingRemaining <= 0)
                {
                    break;
                }
                const qint64 chunkSize = std::min(maxSize - copied, paddingRemaining);
                std::memset(
                    destination + copied,
                    static_cast<unsigned char>(silenceByte_),
                    static_cast<std::size_t>(chunkSize));
                copied += chunkSize;
                drainPaddingPosition_ += chunkSize;
                continue;
            }

            const qint64 chunkSize = std::min(maxSize - copied, remaining);
            std::memcpy(
                destination + copied,
                bytes_.constData() + position_,
                static_cast<std::size_t>(chunkSize));
            copied += chunkSize;
            position_ += chunkSize;
        }
        return copied;
    }

    qint64 writeData(const char*, qint64) override
    {
        return -1;
    }

private:
    QByteArray bytes_{};
    qint64 position_{};
    qint64 drainPaddingBytes_{};
    qint64 drainPaddingPosition_{};
    char silenceByte_{};
    bool loop_{};
};

} // namespace sed::ui::audio::detail
