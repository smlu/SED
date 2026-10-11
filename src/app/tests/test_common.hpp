#pragma once

#include <cmath>
#include <iostream>

#include <QVector3D>

#include "sed/geometry.hpp"
#include "sed/math_utils.hpp"

namespace sed::app_tests
{

inline int failures = 0;

inline void check(const bool ok, const char* msg)
{
    if (ok)
    {
        return;
    }

    ++failures;
    std::cerr << "FAIL: " << msg << "\n";
}

inline bool close(const double a, const double b, const double eps = sed::math::kGeometryEpsilon)
{
    return sed::math::equal(a, b, eps);
}

inline bool closeVec(const TVector& a, const TVector& b, const double eps = sed::math::kGeometryEpsilon)
{
    return close(a.x, b.x, eps) && close(a.y, b.y, eps) && close(a.z, b.z, eps);
}

inline bool closeQVec(const QVector3D& a, const QVector3D& b, const float eps = sed::math::kGeometryEpsilonF)
{
    return sed::math::isZero((a - b).length(), eps);
}

} // namespace sed::app_tests
