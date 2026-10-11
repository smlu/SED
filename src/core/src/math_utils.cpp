#include "sed/math_utils.hpp"

#include <cmath>

namespace sed::math
{

bool isZero(const double value) noexcept
{
    return isZero(value, kTinyEpsilon);
}

bool isZero(const float value) noexcept
{
    return isZero(value, kTinyEpsilonF);
}

bool isZero(const double value, const double epsilon) noexcept
{
    return std::abs(value) <= epsilon;
}

bool isZero(const float value, const float epsilon) noexcept
{
    return std::abs(value) <= epsilon;
}

bool equal(const double a, const double b) noexcept
{
    return equal(a, b, kTinyEpsilon);
}

bool equal(const float a, const float b) noexcept
{
    return equal(a, b, kTinyEpsilonF);
}

bool equal(const double a, const double b, const double epsilon) noexcept
{
    return std::abs(a - b) <= epsilon;
}

bool equal(const float a, const float b, const float epsilon) noexcept
{
    return std::abs(a - b) <= epsilon;
}

} // namespace sed::math
