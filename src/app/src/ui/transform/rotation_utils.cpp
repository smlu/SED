#include "ui/transform/rotation_utils.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace sed::ui::transform
{

namespace
{

double wrap180(double deg) noexcept
{
    if (!std::isfinite(deg))
    {
        return 0.0;
    }

    deg = std::remainder(deg, 360.0);
    return deg == 180.0 ? -180.0 : deg;
}

} // namespace

TVector extractPYRCreateRotateCompatible(const TMatrix43& rot) noexcept
{
    // Inverse of TMatrix43::CreateRotate (pitch/yaw/roll in degrees).
    constexpr double kRadToDeg = 57.295779513082320876798154814105; // 180/pi

    TVector out{};

    const double cosPitch = std::hypot(rot.lvec.x, rot.lvec.y);
    const double pitchRad = std::atan2(rot.lvec.z, cosPitch);

    out.pitch = pitchRad * kRadToDeg;

    if (cosPitch > 8.0 * std::numeric_limits<double>::epsilon())
    {
        out.yaw = std::atan2(-rot.lvec.x, rot.lvec.y) * kRadToDeg;
        out.roll = std::atan2(-rot.rvec.z, rot.uvec.z) * kRadToDeg;
    }
    else
    {
        // At a roundoff-sized pole, yaw and roll are not uniquely defined.
        // Pick a stable solution with roll = 0.
        out.yaw = std::atan2(rot.rvec.y, rot.rvec.x) * kRadToDeg;
        out.roll = 0.0;
    }

    out.pitch = wrap180(out.pitch);
    out.yaw = wrap180(out.yaw);
    out.roll = wrap180(out.roll);
    return out;
}

} // namespace sed::ui::transform
