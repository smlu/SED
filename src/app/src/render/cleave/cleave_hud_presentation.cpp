#include "render/cleave/cleave_hud_presentation.hpp"

#include <algorithm>
#include <limits>

namespace sed::render::cleave
{
namespace
{

constexpr int kMaximumFadeEdgeMs = 40;
constexpr int kMaximumWakeIntervalMs = 12;

int boundedEdgeWakeDelayMs(
    const CleaveHudPresentation::TimePoint now,
    const CleaveHudPresentation::TimePoint edgeEnd,
    const int edgeDurationMs) noexcept
{
    const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(
        edgeEnd - now).count();
    const int maximumDelay = std::min(kMaximumWakeIntervalMs,
        std::max(1, edgeDurationMs / 2));
    return static_cast<int>(std::clamp<std::int64_t>(
        remaining, 1, maximumDelay));
}

} // namespace

void CleaveHudPresentation::sample(const TimePoint now, const int durationMs) noexcept
{
    const bool wasActive = activeAt(now);
    const int previousEdgeDurationMs = fadeEdgeDurationMs();
    const bool previousFadeOut = wasActive
        && now >= deadline_ - std::chrono::milliseconds(previousEdgeDurationMs);
    const float previousOpacity = opacityAt(now);
    const bool recoveryStillActive = recovering_ && wasActive
        && now < recoveryStartedAt_ + std::chrono::milliseconds(previousEdgeDurationMs);

    if ( !wasActive )
    {
        startedAt_ = now;
        recovering_ = false;
        recoveryStartOpacity_ = 1.0f;
        recoveryStartedAt_ = {};
    }
    else if ( !recoveryStillActive )
    {
        recovering_ = previousFadeOut && previousOpacity < 1.0f;
        if ( recovering_ )
        {
            recoveryStartedAt_ = now;
            recoveryStartOpacity_ = previousOpacity;
        }
    }
    durationMs_ = std::max(1, durationMs);
    deadline_ = now + std::chrono::milliseconds(durationMs_);
    hasSample_ = true;
    ++generation_;
}

void CleaveHudPresentation::clear() noexcept
{
    startedAt_ = {};
    deadline_ = {};
    recoveryStartedAt_ = {};
    durationMs_ = 0;
    recoveryStartOpacity_ = 1.0f;
    hasSample_ = false;
    recovering_ = false;
    ++generation_;
}

bool CleaveHudPresentation::activeAt(const TimePoint now) const noexcept
{
    return hasSample_ && now < deadline_;
}

bool CleaveHudPresentation::expiredAt(const TimePoint now) const noexcept
{
    return hasSample_ && now >= deadline_;
}

int CleaveHudPresentation::fadeEdgeDurationMs() const noexcept
{
    if ( !hasSample_ )
    {
        return 0;
    }
    const int requested = std::min(kMaximumFadeEdgeMs,
        std::max(1, durationMs_ / 10));
    return std::max(1, std::min(requested, durationMs_ / 2));
}

float CleaveHudPresentation::opacityAt(const TimePoint now) const noexcept
{
    if ( !activeAt(now) )
    {
        return 0.0f;
    }

    const int edgeDurationMs = fadeEdgeDurationMs();
    const float edge = static_cast<float>(edgeDurationMs);
    const float elapsedMs = static_cast<float>(
        std::chrono::duration_cast<std::chrono::milliseconds>(now - startedAt_).count());
    const float remainingMs = static_cast<float>(
        std::chrono::duration_cast<std::chrono::milliseconds>(deadline_ - now).count());
    const float fadeIn = std::clamp(elapsedMs / edge, 0.0f, 1.0f);
    const float fadeOut = std::clamp(remainingMs / edge, 0.0f, 1.0f);
    float opacity = std::min(fadeIn, fadeOut);

    if ( recovering_ )
    {
        const float recoveryElapsedMs = static_cast<float>(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                now - recoveryStartedAt_).count());
        const float recoveryProgress = std::clamp(
            recoveryElapsedMs / edge, 0.0f, 1.0f);
        const float recoveryOpacity = recoveryStartOpacity_
            + (1.0f - recoveryStartOpacity_) * recoveryProgress;
        opacity = std::min(opacity, recoveryOpacity);
    }

    return opacity;
}

int CleaveHudPresentation::nextWakeDelayMs(const TimePoint now) const noexcept
{
    if ( !activeAt(now) )
    {
        return 0;
    }

    const auto edge = std::chrono::milliseconds(fadeEdgeDurationMs());
    const TimePoint fadeInEnd = startedAt_ + edge;
    const TimePoint fadeOutStart = deadline_ - edge;
    if ( now < fadeInEnd )
    {
        return boundedEdgeWakeDelayMs(now, fadeInEnd, fadeEdgeDurationMs());
    }
    const TimePoint recoveryEnd = recoveryStartedAt_ + edge;
    if ( recovering_ && now < recoveryEnd )
    {
        return boundedEdgeWakeDelayMs(now, recoveryEnd, fadeEdgeDurationMs());
    }
    if ( now < fadeOutStart )
    {
        const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(
            fadeOutStart - now).count();
        return static_cast<int>(std::clamp<std::int64_t>(
            remaining, 1, std::numeric_limits<int>::max()));
    }
    return boundedEdgeWakeDelayMs(now, deadline_, fadeEdgeDurationMs());
}

} // namespace sed::render::cleave
