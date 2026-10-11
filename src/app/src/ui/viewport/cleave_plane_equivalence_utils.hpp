#pragma once

#include "sed/geometry.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace sed::ui::viewport
{

namespace cleave_plane_equivalence_detail
{

inline constexpr double kParallelRoundoffFactor = 16.0;

inline bool scaleNormalForProof(const TVector& input, TVector& output) noexcept
{
    if (!std::isfinite(input.x) || !std::isfinite(input.y) || !std::isfinite(input.z))
    {
        return false;
    }

    const double magnitude = std::max({ std::abs(input.x), std::abs(input.y), std::abs(input.z) });
    if (!(magnitude > 0.0))
    {
        return false;
    }

    int exponent{};
    (void)std::frexp(magnitude, &exponent);
    output = TVector{
        std::scalbn(input.x, -exponent),
        std::scalbn(input.y, -exponent),
        std::scalbn(input.z, -exponent),
    };
    return std::isfinite(output.x) && std::isfinite(output.y) && std::isfinite(output.z)
        && (input.x == 0.0 || output.x != 0.0)
        && (input.y == 0.0 || output.y != 0.0)
        && (input.z == 0.0 || output.z != 0.0);
}

inline bool productIsUsable(const double lhs, const double rhs, const double product) noexcept
{
    return std::isfinite(product)
        && (!(lhs != 0.0 && rhs != 0.0) || (product != 0.0 && std::fpclassify(product) != FP_SUBNORMAL));
}

inline bool crossComponentWithinRoundoff(
    const double firstLeft,
    const double firstRight,
    const double secondLeft,
    const double secondRight) noexcept
{
    const double firstProduct = firstLeft * firstRight;
    const double secondProduct = secondLeft * secondRight;
    if (!productIsUsable(firstLeft, firstRight, firstProduct)
        || !productIsUsable(secondLeft, secondRight, secondProduct))
    {
        return false;
    }

    const double residual = firstProduct - secondProduct;
    const double productMagnitude = std::abs(firstProduct) + std::abs(secondProduct);
    const double roundoffBound = kParallelRoundoffFactor
        * std::numeric_limits<double>::epsilon() * productMagnitude;
    return std::isfinite(residual) && std::isfinite(productMagnitude)
        && std::isfinite(roundoffBound) && std::abs(residual) <= roundoffBound;
}

inline bool twoSum(
    const double lhs,
    const double rhs,
    double& sum,
    double& error) noexcept
{
    sum = lhs + rhs;
    if (!std::isfinite(sum))
    {
        return false;
    }
    const double rhsVirtual = sum - lhs;
    const double lhsVirtual = sum - rhsVirtual;
    const double rhsRoundoff = rhs - rhsVirtual;
    const double lhsRoundoff = lhs - lhsVirtual;
    error = lhsRoundoff + rhsRoundoff;
    return std::isfinite(error);
}

inline bool twoDifference(
    const double lhs,
    const double rhs,
    double& difference,
    double& error) noexcept
{
    difference = lhs - rhs;
    if (!std::isfinite(difference))
    {
        return false;
    }
    const double rhsVirtual = lhs - difference;
    const double lhsVirtual = difference + rhsVirtual;
    const double rhsRoundoff = rhsVirtual - rhs;
    const double lhsRoundoff = lhs - lhsVirtual;
    error = lhsRoundoff + rhsRoundoff;
    return std::isfinite(error);
}

// Grows a fixed-size, non-overlapping floating-point expansion. Returning
// false on overflow keeps the proof conservative and requires no allocation.
inline bool addExpansionTerm(
    std::array<double, 16>& expansion,
    std::size_t& count,
    const double term) noexcept
{
    if (term == 0.0)
    {
        return true;
    }

    std::array<double, 16> grown{};
    std::size_t grownCount{};
    double accumulator = term;
    for (std::size_t index = 0; index < count; ++index)
    {
        double sum{};
        double error{};
        if (!twoSum(accumulator, expansion[index], sum, error))
        {
            return false;
        }
        if (error != 0.0)
        {
            if (grownCount == grown.size())
            {
                return false;
            }
            grown[grownCount++] = error;
        }
        accumulator = sum;
    }
    if (accumulator != 0.0)
    {
        if (grownCount == grown.size())
        {
            return false;
        }
        grown[grownCount++] = accumulator;
    }
    expansion = grown;
    count = grownCount;
    return true;
}

inline bool addExactProduct(
    std::array<double, 16>& expansion,
    std::size_t& count,
    const double lhs,
    const double rhs) noexcept
{
    if (lhs == 0.0 || rhs == 0.0)
    {
        return true;
    }

    int lhsExponent{};
    int rhsExponent{};
    (void)std::frexp(std::abs(lhs), &lhsExponent);
    (void)std::frexp(std::abs(rhs), &rhsExponent);
    // A product of two `digits`-bit significands can have its lowest exact
    // product bit at lhsExponent + rhsExponent - 2 * digits. Keep that bit at
    // or above the least subnormal exponent before trusting an FMA residual;
    // below this threshold a nonzero tail can round to zero and an ambiguous
    // offset must be rejected.
    constexpr int kMinimumFrexpExponentSumForExactTail =
        std::numeric_limits<double>::min_exponent + std::numeric_limits<double>::digits;
    if (lhsExponent + rhsExponent < kMinimumFrexpExponentSumForExactTail)
    {
        return false;
    }

    const double product = lhs * rhs;
    if (!productIsUsable(lhs, rhs, product))
    {
        return false;
    }
    const double productError = std::fma(lhs, rhs, -product);
    if (!std::isfinite(productError)
        || (productError != 0.0 && std::fpclassify(productError) == FP_SUBNORMAL)
        || !addExpansionTerm(expansion, count, product)
        || !addExpansionTerm(expansion, count, productError))
    {
        return false;
    }
    return true;
}

inline bool translatedDotIsExactlyZero(
    const TVector& normal,
    const TVector& firstPoint,
    const TVector& secondPoint) noexcept
{
    const std::array<double, 3> coefficients{ normal.x, normal.y, normal.z };
    const std::array<double, 3> firstCoordinates{ firstPoint.x, firstPoint.y, firstPoint.z };
    const std::array<double, 3> secondCoordinates{ secondPoint.x, secondPoint.y, secondPoint.z };
    std::array<double, 16> expansion{};
    std::size_t count{};

    for (std::size_t axis = 0; axis < coefficients.size(); ++axis)
    {
        if (coefficients[axis] == 0.0)
        {
            continue;
        }

        double difference{};
        double differenceError{};
        if (!twoDifference(secondCoordinates[axis], firstCoordinates[axis], difference, differenceError)
            || (difference != 0.0 && std::fpclassify(difference) == FP_SUBNORMAL)
            || (differenceError != 0.0 && std::fpclassify(differenceError) == FP_SUBNORMAL)
            || !addExactProduct(expansion, count, coefficients[axis], difference)
            || !addExactProduct(expansion, count, coefficients[axis], differenceError))
        {
            return false;
        }
    }

    return count == 0;
}

} // namespace cleave_plane_equivalence_detail

// A non-splitting line is redundant only when the two finite input planes
// admit an arithmetic proof of parallel normals and exactly zero translated
// signed displacement. Ambiguous underflow/overflow is rejected; this does
// not weld nearby planes based on a world-space tolerance.
inline bool cleaveSupportingPlanesEquivalent(
    const TVector& firstNormal,
    const TVector& firstPoint,
    const TVector& secondNormal,
    const TVector& secondPoint) noexcept
{
    using namespace cleave_plane_equivalence_detail;

    if (!std::isfinite(firstPoint.x) || !std::isfinite(firstPoint.y) || !std::isfinite(firstPoint.z)
        || !std::isfinite(secondPoint.x) || !std::isfinite(secondPoint.y) || !std::isfinite(secondPoint.z))
    {
        return false;
    }

    TVector firstScaled{};
    TVector secondScaled{};
    if (!scaleNormalForProof(firstNormal, firstScaled)
        || !scaleNormalForProof(secondNormal, secondScaled))
    {
        return false;
    }

    const double orientation = firstScaled.dot(secondScaled);
    if (!std::isfinite(orientation))
    {
        return false;
    }
    if (orientation < 0.0)
    {
        secondScaled = secondScaled * -1.0;
    }

    if (!crossComponentWithinRoundoff(
            firstScaled.y, secondScaled.z,
            firstScaled.z, secondScaled.y)
        || !crossComponentWithinRoundoff(
            firstScaled.z, secondScaled.x,
            firstScaled.x, secondScaled.z)
        || !crossComponentWithinRoundoff(
            firstScaled.x, secondScaled.y,
            firstScaled.y, secondScaled.x))
    {
        return false;
    }

    return translatedDotIsExactlyZero(firstScaled, firstPoint, secondPoint);
}

// Finite authoring can round a straight shared-endpoint continuation into two
// slightly different planes. This separate admission requires the exact joint,
// normals consistent with their own strokes, and collinearity bounded by the
// represented endpoint coordinates. It supplies no world-space or angle floor
// and cannot admit an offset line or an unrelated plane-only request.
inline bool cleaveStrokesShareRoundedSupportingPlane(
    const TVector& firstNormal,
    const TVector& firstPoint,
    const TVector& firstEnd,
    const TVector& secondNormal,
    const TVector& secondPoint,
    const TVector& secondEnd) noexcept
{
    using namespace cleave_plane_equivalence_detail;

    const auto finitePoint = [](const TVector& point) noexcept
    {
        return std::isfinite(point.x) && std::isfinite(point.y) && std::isfinite(point.z);
    };
    const auto samePoint = [](const TVector& first, const TVector& second) noexcept
    {
        return first.x == second.x && first.y == second.y && first.z == second.z;
    };
    if ( !finitePoint(firstPoint) || !finitePoint(firstEnd)
        || !finitePoint(secondPoint) || !finitePoint(secondEnd) )
    {
        return false;
    }

    TVector firstScaled{};
    TVector secondScaled{};
    if ( !scaleNormalForProof(firstNormal, firstScaled)
        || !scaleNormalForProof(secondNormal, secondScaled) )
    {
        return false;
    }
    const auto containsOwnStroke = [&](const TVector& normal,
        const TVector& point, const TVector& end) noexcept
    {
        const TVector delta = end - point;
        if ( !finitePoint(delta) || (delta.x == 0.0 && delta.y == 0.0 && delta.z == 0.0) )
        {
            return false;
        }
        const std::array<double, 3> coefficients{normal.x, normal.y, normal.z};
        const std::array<double, 3> components{delta.x, delta.y, delta.z};
        const std::array<double, 3> pointComponents{point.x, point.y, point.z};
        const std::array<double, 3> endComponents{end.x, end.y, end.z};
        double distance{};
        double magnitude{};
        double coordinateRoundoff{};
        std::array<double, 16> products{};
        std::size_t productCount{};
        for ( std::size_t axis = 0; axis < coefficients.size(); ++axis )
        {
            const double product = coefficients[axis] * components[axis];
            if ( !productIsUsable(coefficients[axis], components[axis], product)
                || !addExactProduct(products, productCount, coefficients[axis], components[axis]) )
            {
                return false;
            }
            distance += product;
            magnitude += std::abs(product);
            coordinateRoundoff += std::abs(coefficients[axis])
                * (std::numeric_limits<double>::epsilon() * std::abs(pointComponents[axis])
                    + std::numeric_limits<double>::epsilon() * std::abs(endComponents[axis]));
        }
        const double error = coordinateRoundoff
            + kParallelRoundoffFactor * std::numeric_limits<double>::epsilon() * magnitude;
        return std::isfinite(distance) && std::isfinite(error) && std::abs(distance) <= error;
    };
    if ( !containsOwnStroke(firstScaled, firstPoint, firstEnd)
        || !containsOwnStroke(secondScaled, secondPoint, secondEnd) )
    {
        return false;
    }

    const TVector* shared{};
    const TVector* firstOther{};
    const TVector* secondOther{};
    for ( const bool firstAtEnd : {false, true} )
    {
        for ( const bool secondAtEnd : {false, true} )
        {
            const TVector& first = firstAtEnd ? firstEnd : firstPoint;
            const TVector& second = secondAtEnd ? secondEnd : secondPoint;
            if ( samePoint(first, second) )
            {
                shared = &first;
                firstOther = firstAtEnd ? &firstPoint : &firstEnd;
                secondOther = secondAtEnd ? &secondPoint : &secondEnd;
            }
        }
    }
    if ( !shared )
    {
        return false;
    }

    const TVector firstDirection = *firstOther - *shared;
    const TVector secondDirection = *secondOther - *shared;
    if ( !finitePoint(firstDirection) || !finitePoint(secondDirection) )
    {
        return false;
    }
    constexpr double epsilon = std::numeric_limits<double>::epsilon();
    const auto coordinateError = [shared, epsilon](const TVector& endpoint) noexcept
    {
        // Multiply before addition so two finite far-origin coordinates do
        // not overflow solely while computing their representation budget.
        return TVector{
            epsilon * std::abs(endpoint.x) + epsilon * std::abs(shared->x),
            epsilon * std::abs(endpoint.y) + epsilon * std::abs(shared->y),
            epsilon * std::abs(endpoint.z) + epsilon * std::abs(shared->z),
        };
    };
    const TVector firstError = coordinateError(*firstOther);
    const TVector secondError = coordinateError(*secondOther);
    const double firstLength = std::hypot(firstDirection.x, firstDirection.y, firstDirection.z);
    const double secondLength = std::hypot(secondDirection.x, secondDirection.y, secondDirection.z);
    const double firstErrorLength = std::hypot(firstError.x, firstError.y, firstError.z);
    const double secondErrorLength = std::hypot(secondError.x, secondError.y, secondError.z);
    if ( !std::isfinite(firstLength) || !std::isfinite(secondLength)
        || !(firstLength > firstErrorLength) || !(secondLength > secondErrorLength) )
    {
        return false;
    }
    const double directionError = 2.0 * firstErrorLength / (firstLength - firstErrorLength)
        + 2.0 * secondErrorLength / (secondLength - secondErrorLength);
    if ( !std::isfinite(directionError) || directionError >= 1.0 )
    {
        return false;
    }

    const double scale = std::max({std::abs(firstDirection.x), std::abs(firstDirection.y),
        std::abs(firstDirection.z), std::abs(secondDirection.x),
        std::abs(secondDirection.y), std::abs(secondDirection.z)});
    int exponent{};
    (void)std::frexp(scale, &exponent);
    const auto scaledComponents = [exponent](const TVector& value) noexcept
    {
        return std::array<double, 3>{std::scalbn(value.x, -exponent),
            std::scalbn(value.y, -exponent), std::scalbn(value.z, -exponent)};
    };
    const auto first = scaledComponents(firstDirection);
    const auto second = scaledComponents(secondDirection);
    const auto firstBounds = scaledComponents(firstError);
    const auto secondBounds = scaledComponents(secondError);
    const std::array<double, 3> firstNative{firstDirection.x, firstDirection.y, firstDirection.z};
    const std::array<double, 3> secondNative{secondDirection.x, secondDirection.y, secondDirection.z};
    const std::array<double, 3> firstNormalComponents{firstScaled.x, firstScaled.y, firstScaled.z};
    const std::array<double, 3> secondNormalComponents{secondScaled.x, secondScaled.y, secondScaled.z};
    const double normalError = directionError
        * std::hypot(firstScaled.x, firstScaled.y, firstScaled.z)
        * std::hypot(secondScaled.x, secondScaled.y, secondScaled.z);
    for ( std::size_t axis = 0; axis < first.size(); ++axis )
    {
        if ( (firstNative[axis] != 0.0 && first[axis] == 0.0)
            || (secondNative[axis] != 0.0 && second[axis] == 0.0) )
        {
            return false;
        }
        const auto next = (axis + 1) % first.size();
        const auto last = (axis + 2) % first.size();
        const double left = first[next] * second[last];
        const double right = first[last] * second[next];
        if ( !productIsUsable(first[next], second[last], left)
            || !productIsUsable(first[last], second[next], right) )
        {
            return false;
        }
        const double error = kParallelRoundoffFactor * epsilon * (std::abs(left) + std::abs(right))
            + std::abs(first[next]) * secondBounds[last]
            + firstBounds[next] * std::abs(second[last]) + firstBounds[next] * secondBounds[last]
            + std::abs(first[last]) * secondBounds[next]
            + firstBounds[last] * std::abs(second[next]) + firstBounds[last] * secondBounds[next];
        const double normalLeft = firstNormalComponents[next] * secondNormalComponents[last];
        const double normalRight = firstNormalComponents[last] * secondNormalComponents[next];
        const double normalBound = normalError
            + kParallelRoundoffFactor * epsilon * (std::abs(normalLeft) + std::abs(normalRight));
        if ( !productIsUsable(firstNormalComponents[next], secondNormalComponents[last], normalLeft)
            || !productIsUsable(firstNormalComponents[last], secondNormalComponents[next], normalRight)
            || !std::isfinite(error) || !std::isfinite(normalBound)
            || std::abs(left - right) > error || std::abs(normalLeft - normalRight) > normalBound )
        {
            return false;
        }
    }
    return true;
}

} // namespace sed::ui::viewport
