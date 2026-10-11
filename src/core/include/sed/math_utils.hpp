#pragma once

#include <numbers>

namespace sed::math
{

/** Epsilon for exact-ish transform and snap residual checks. */
inline constexpr double kTinyEpsilon = 1.0e-12;

/** Float-space counterpart to kTinyEpsilon for squared-length checks. */
inline constexpr float kTinyEpsilonF = 1.0e-12f;

/** Epsilon for editor geometry operations where source data is legacy text. */
inline constexpr double kGeometryEpsilon = 1.0e-9;

/** Epsilon for trigonometric rotation and matrix round-trip comparisons. */
inline constexpr double kRotationEpsilon = 1.0e-6;

/** Epsilon for Qt/OpenGL float-space comparisons. */
inline constexpr float kGeometryEpsilonF = 1.0e-5f;

/** Float-space counterpart to kRotationEpsilon. */
inline constexpr float kRotationEpsilonF = 1.0e-6f;

/** Converts degrees to radians using the standard library pi constant. */
inline constexpr double degToRad(const double deg) noexcept
{
    return deg * (std::numbers::pi_v<double> / 180.0);
}

/** Converts radians to degrees using the standard library pi constant. */
inline constexpr double radToDeg(const double rad) noexcept
{
    return rad * (180.0 / std::numbers::pi_v<double>);
}

/** Returns true when a scalar is within the default exact-ish tolerance of zero. */
bool isZero(double value) noexcept;

/** Returns true when a float scalar is within the default exact-ish tolerance of zero. */
bool isZero(float value) noexcept;

/** Returns true when a scalar is within a caller-supplied tolerance of zero. */
bool isZero(double value, double epsilon) noexcept;

/** Returns true when a float scalar is within a caller-supplied tolerance of zero. */
bool isZero(float value, float epsilon) noexcept;

/** Returns true when two scalars differ by no more than the default exact-ish tolerance. */
bool equal(double a, double b) noexcept;

/** Returns true when two float scalars differ by no more than the default exact-ish tolerance. */
bool equal(float a, float b) noexcept;

/** Returns true when two scalars differ by no more than a caller-supplied tolerance. */
bool equal(double a, double b, double epsilon) noexcept;

/** Returns true when two float scalars differ by no more than a caller-supplied tolerance. */
bool equal(float a, float b, float epsilon) noexcept;

} // namespace sed::math
