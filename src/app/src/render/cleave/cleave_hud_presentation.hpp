#pragma once

#include <chrono>
#include <cstdint>

namespace sed::render::cleave
{

/** Timestamp model for the transient Cleave transform readout. */
class CleaveHudPresentation
{
public:
    using Clock = std::chrono::steady_clock;
    using TimePoint = Clock::time_point;

    void sample(TimePoint now, int durationMs) noexcept;
    void clear() noexcept;

    [[nodiscard]] bool activeAt(TimePoint now) const noexcept;
    [[nodiscard]] bool expiredAt(TimePoint now) const noexcept;
    [[nodiscard]] TimePoint startedAt() const noexcept { return startedAt_; }
    [[nodiscard]] TimePoint deadline() const noexcept { return deadline_; }
    [[nodiscard]] int durationMs() const noexcept { return durationMs_; }
    [[nodiscard]] std::uint64_t generation() const noexcept { return generation_; }

    /** Fade window at either edge, constrained so the edges cannot overlap. */
    [[nodiscard]] int fadeEdgeDurationMs() const noexcept;

    /** Opacity envelope for a sampled readout; values are zero to one. */
    [[nodiscard]] float opacityAt(TimePoint now) const noexcept;

    /** Delay until the next presentation-only repaint, or zero when inactive. */
    [[nodiscard]] int nextWakeDelayMs(TimePoint now) const noexcept;

private:
    TimePoint startedAt_{};
    TimePoint deadline_{};
    TimePoint recoveryStartedAt_{};
    int durationMs_{};
    float recoveryStartOpacity_{ 1.0f };
    std::uint64_t generation_{};
    bool hasSample_{};
    bool recovering_{};
};

} // namespace sed::render::cleave
