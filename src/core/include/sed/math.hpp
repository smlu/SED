#pragma once

#include <cmath>

namespace sed
{

inline constexpr double sqr(const double v) noexcept
{
    return v * v;
}

inline void VectorAdd2(const double x1, const double y1, const double x2, const double y2, double& rx, double& ry) noexcept
{
    rx = x1 + x2;
    ry = y1 + y2;
}

inline constexpr double VectorCross2(const double x1, const double y1, const double x2, const double y2) noexcept
{
    return x1 * y2 - x2 * y1;
}

inline constexpr double VectorDot2(const double x1, const double y1, const double x2, const double y2) noexcept
{
    return x1 * x2 + y1 * y2;
}

inline double VectorDistSqr2(const double x1, const double y1, const double x2, const double y2) noexcept
{
    return sqr(x1 - x2) + sqr(y1 - y2);
}

inline double VectorDist2(const double x1, const double y1, const double x2, const double y2) noexcept
{
    return std::sqrt(VectorDistSqr2(x1, y1, x2, y2));
}

inline constexpr double VectorLenSqr2(const double x, const double y) noexcept
{
    return x * x + y * y;
}

inline double VectorLen2(const double x, const double y) noexcept
{
    return std::sqrt(VectorLenSqr2(x, y));
}

inline double VectorNormalize2(const double x, const double y, double& nx, double& ny) noexcept
{
    const double len = VectorLen2(x, y);
    if (len == 0.0)
    {
        nx = 0.0;
        ny = 0.0;
        return 0.0;
    }
    nx = x / len;
    ny = y / len;
    return len;
}

inline double VectorNormalize2(double& x, double& y) noexcept
{
    return VectorNormalize2(x, y, x, y);
}

inline void VectorScale2(const double x, const double y, const double scalar, double& sx, double& sy) noexcept
{
    sx = x * scalar;
    sy = y * scalar;
}

inline void VectorSub2(const double x1, const double y1, const double x2, const double y2, double& rx, double& ry) noexcept
{
    rx = x1 - x2;
    ry = y1 - y2;
}

inline void VectorAdd3(
    const double x1,
    const double y1,
    const double z1,
    const double x2,
    const double y2,
    const double z2,
    double& rx,
    double& ry,
    double& rz) noexcept
{
    rx = x1 + x2;
    ry = y1 + y2;
    rz = z1 + z2;
}

inline void VectorCross3(
    const double x1,
    const double y1,
    const double z1,
    const double x2,
    const double y2,
    const double z2,
    double& cx,
    double& cy,
    double& cz) noexcept
{
    cx = y1 * z2 - z1 * y2;
    cy = z1 * x2 - x1 * z2;
    cz = x1 * y2 - x2 * y1;
}

inline double VectorDistSqr3(const double x1, const double y1, const double z1, const double x2, const double y2, const double z2) noexcept
{
    return sqr(x1 - x2) + sqr(y1 - y2) + sqr(z1 - z2);
}

inline double VectorDist3(const double x1, const double y1, const double z1, const double x2, const double y2, const double z2) noexcept
{
    return std::sqrt(VectorDistSqr3(x1, y1, z1, x2, y2, z2));
}

inline constexpr double VectorDot3(
    const double x1,
    const double y1,
    const double z1,
    const double x2,
    const double y2,
    const double z2) noexcept
{
    return x1 * x2 + y1 * y2 + z1 * z2;
}

inline constexpr double VectorLenSqr3(const double x, const double y, const double z) noexcept
{
    return x * x + y * y + z * z;
}

inline double VectorLen3(const double x, const double y, const double z) noexcept
{
    return std::sqrt(VectorLenSqr3(x, y, z));
}

inline double VectorNormalize3(const double x, const double y, const double z, double& nx, double& ny, double& nz) noexcept
{
    const double len = VectorLen3(x, y, z);
    if (len == 0.0)
    {
        nx = 0.0;
        ny = 0.0;
        nz = 0.0;
        return 0.0;
    }
    nx = x / len;
    ny = y / len;
    nz = z / len;
    return len;
}

inline double VectorNormalize3(double& x, double& y, double& z) noexcept
{
    return VectorNormalize3(x, y, z, x, y, z);
}

inline void VectorProject3(
    const double ax,
    const double ay,
    const double az,
    const double bx,
    const double by,
    const double bz,
    double& px,
    double& py,
    double& pz) noexcept
{
    const double denom = VectorDot3(bx, by, bz, bx, by, bz);
    if (denom == 0.0)
    {
        px = 0.0;
        py = 0.0;
        pz = 0.0;
        return;
    }
    const double len = VectorDot3(ax, ay, az, bx, by, bz) / denom;
    px = bx * len;
    py = by * len;
    pz = bz * len;
}

inline void VectorProjectOnToNormal3(
    const double ax,
    const double ay,
    const double az,
    const double nx,
    const double ny,
    const double nz,
    double& px,
    double& py,
    double& pz) noexcept
{
    const double len = VectorDot3(ax, ay, az, nx, ny, nz);
    px = nx * len;
    py = ny * len;
    pz = nz * len;
}

inline void VectorScale3(const double x, const double y, const double z, const double scalar, double& sx, double& sy, double& sz) noexcept
{
    sx = x * scalar;
    sy = y * scalar;
    sz = z * scalar;
}

inline void VectorSub3(
    const double x1,
    const double y1,
    const double z1,
    const double x2,
    const double y2,
    const double z2,
    double& rx,
    double& ry,
    double& rz) noexcept
{
    rx = x1 - x2;
    ry = y1 - y2;
    rz = z1 - z2;
}

} // namespace sed
