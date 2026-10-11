#pragma once

#include <cstdint>
#include <limits>

namespace sed::render
{

enum class CleaveWheelOperation : std::uint8_t
{
    None,
    LineRotate,
    LineScale,
    ShapeRotate,
    ShapeScale,
};

/**
 * Scalar wheel state relative to one admitted gesture baseline. The viewport
 * owns the exact line rollback and the prepared sparse transform workset.
 * Copying this value before a sample permits failure-atomic preview admission
 * without allocating or retaining another draft.
 */
class CleaveWheelSession
{
public:
    [[nodiscard]] bool active() const noexcept
    {
        return operation_ != CleaveWheelOperation::None;
    }

    [[nodiscard]] CleaveWheelOperation operation() const noexcept
    {
        return operation_;
    }

    [[nodiscard]] bool transformsLines() const noexcept
    {
        return operation_ == CleaveWheelOperation::LineRotate
            || operation_ == CleaveWheelOperation::LineScale;
    }

    [[nodiscard]] bool transformsShape() const noexcept
    {
        return operation_ == CleaveWheelOperation::ShapeRotate
            || operation_ == CleaveWheelOperation::ShapeScale;
    }

    [[nodiscard]] double angleDegrees() const noexcept
    {
        return angleDegrees_;
    }

    [[nodiscard]] double scale() const noexcept
    {
        return scale_;
    }
    [[nodiscard]] bool identity() const noexcept;

    /** Invalid operations or nonfinite/out-of-range steps preserve this state. */
    bool begin(CleaveWheelOperation operation, double angleStepDegrees) noexcept;

    /**
     * Adds one wheel sample using the existing 1.1 scale and 5-degree Free
     * rotation policy. Scale's per-event factor remains in 0.05..1000. The
     * optional total bounds support uniform armed-shape dimension clamping.
     * Invalid, zero, or unrepresentable samples preserve every scalar.
     */
    bool accumulate(double steps, double minimumScale = 0.0,
        double maximumScale = std::numeric_limits<double>::max()) noexcept;

    void reset() noexcept;

private:
    CleaveWheelOperation operation_{CleaveWheelOperation::None};
    double angleStepDegrees_{};
    double angleDegrees_{};
    double scale_{1.0};
    double scaleSteps_{};
};

} // namespace sed::render
