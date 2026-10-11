#pragma once

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>

namespace sed::level::detail
{

struct ExactBinary64ProductTerm
{
    std::size_t first{};
    std::size_t second{};
    bool subtract{};
};

struct ExactBinary64Product4Term
{
    std::array<std::size_t, 4> factors{};
    bool subtract{};
};

// Evaluate a short sum of products of finite binary64 values as an exact
// dyadic integer. The fixed limb array is deliberately local to this helper:
// it performs no allocation and its result does not depend on the host
// long-double format. Six terms are sufficient for a three-point orientation
// and eight terms cover the cross product of two independent directions.
template <std::size_t ValueCount, std::size_t TermCount>
inline std::optional<int> exactBinary64ProductSumSign(
    const std::array<double, ValueCount>& values,
    const std::array<ExactBinary64ProductTerm, TermCount>& terms) noexcept
{
    static_assert(ValueCount <= 8 && TermCount <= 8,
        "the binary64 pair-product accumulator is bounded for eight terms");
    static_assert(sizeof(double) == sizeof(std::uint64_t)
        && std::numeric_limits<double>::is_iec559
        && std::numeric_limits<double>::radix == 2
        && std::numeric_limits<double>::digits == 53
        && std::numeric_limits<double>::min_exponent == -1021
        && std::numeric_limits<double>::max_exponent == 1024);
    struct Dyadic
    {
        std::uint64_t mantissa;
        int exponent;
        bool negative;
    };
    std::array<Dyadic, ValueCount> operands{};
    for ( std::size_t index = 0; index < values.size(); ++index )
    {
        if ( !std::isfinite(values[index]) )
        {
            return std::nullopt;
        }
        const auto bits = std::bit_cast<std::uint64_t>(values[index]);
        const auto exponent = static_cast<int>((bits >> 52) & 0x7ff);
        const auto fraction = bits & ((std::uint64_t{1} << 52) - 1);
        operands[index] = {exponent == 0 ? fraction : fraction | (std::uint64_t{1} << 52),
            exponent == 0 ? -1074 : exponent - 1023 - 52, (bits >> 63) != 0};
    }
    // Every binary64 is an integer mantissa times 2^exponent, with exponent
    // [-1074, 971] and at most 53 mantissa bits. Products therefore occupy at
    // most 4196 bits above 2^-2148. The largest supported sum needs only
    // three additional carry bits.
    constexpr int kMinimumProductExponent = -2148;
    constexpr std::size_t kLimbs = 68;
    static_assert(kLimbs * 64 >= 2 * 971 + 106 - kMinimumProductExponent + 3);
    using Accumulator = std::array<std::uint64_t, kLimbs>;
    Accumulator positive{}, negative{};
    const auto addWord = [](Accumulator& accumulator, std::size_t index, std::uint64_t value)
    {
        while ( value != 0 && index < accumulator.size() )
        {
            const auto previous = accumulator[index];
            accumulator[index] += value;
            value = accumulator[index] < previous ? 1 : 0;
            ++index;
        }
    };
    const auto product = [&](const ExactBinary64ProductTerm term)
    {
        if ( term.first >= operands.size() || term.second >= operands.size() )
        {
            return false;
        }
        const auto& a = operands[term.first];
        const auto& b = operands[term.second];
        if ( a.mantissa == 0 || b.mantissa == 0 )
        {
            return true;
        }
        constexpr std::uint64_t kWordMask = 0xffffffff;
        const auto a0 = a.mantissa & kWordMask, a1 = a.mantissa >> 32;
        const auto b0 = b.mantissa & kWordMask, b1 = b.mantissa >> 32;
        const auto lowProduct = a0 * b0;
        const auto firstCross = a0 * b1;
        const auto secondCross = a1 * b0;
        const auto middle = (lowProduct >> 32) + (firstCross & kWordMask) + (secondCross & kWordMask);
        const auto low = (lowProduct & kWordMask) | (middle << 32);
        const auto high = a1 * b1 + (firstCross >> 32) + (secondCross >> 32) + (middle >> 32);
        auto& accumulator = (a.negative != b.negative) != term.subtract ? negative : positive;
        const auto offset = static_cast<unsigned>(a.exponent + b.exponent - kMinimumProductExponent);
        const auto word = static_cast<std::size_t>(offset / 64);
        const auto shift = offset % 64;
        if ( shift == 0 )
        {
            addWord(accumulator, word, low);
            addWord(accumulator, word + 1, high);
        }
        else
        {
            addWord(accumulator, word, low << shift);
            addWord(accumulator, word + 1, (low >> (64 - shift)) | (high << shift));
            addWord(accumulator, word + 2, high >> (64 - shift));
        }
        return true;
    };
    for ( const auto term : terms )
    {
        if ( !product(term) )
        {
            return std::nullopt;
        }
    }
    for ( std::size_t index = kLimbs; index > 0; --index )
    {
        if ( positive[index - 1] != negative[index - 1] )
        {
            return positive[index - 1] > negative[index - 1] ? 1 : -1;
        }
    }
    return 0;
}

struct ExactBinary64Ratio
{
    double value{};
    double error{};
};

struct ExactBinary64SignedMagnitude
{
    std::array<std::uint64_t, 68> words{};
    bool negative{};
    bool zero{true};
};

// Materialize a short exact binary64 product sum as a signed magnitude. The
// fixed result is used only for a certified ratio; all accepted coordinates
// still pass the caller's geometric residual checks.
template <std::size_t ValueCount, std::size_t TermCount>
inline std::optional<ExactBinary64SignedMagnitude> exactBinary64ProductSumMagnitude(
    const std::array<double, ValueCount>& values,
    const std::array<ExactBinary64ProductTerm, TermCount>& terms) noexcept
{
    static_assert(ValueCount <= 8 && TermCount <= 8,
        "the binary64 pair-product magnitude is bounded for eight terms");
    static_assert(sizeof(double) == sizeof(std::uint64_t)
        && std::numeric_limits<double>::is_iec559
        && std::numeric_limits<double>::radix == 2
        && std::numeric_limits<double>::digits == 53
        && std::numeric_limits<double>::min_exponent == -1021
        && std::numeric_limits<double>::max_exponent == 1024);
    struct Dyadic
    {
        std::uint64_t mantissa;
        int exponent;
        bool negative;
    };
    std::array<Dyadic, ValueCount> operands{};
    for ( std::size_t index = 0; index < values.size(); ++index )
    {
        if ( !std::isfinite(values[index]) )
        {
            return std::nullopt;
        }
        const auto bits = std::bit_cast<std::uint64_t>(values[index]);
        const auto exponent = static_cast<int>((bits >> 52) & 0x7ff);
        const auto fraction = bits & ((std::uint64_t{1} << 52) - 1);
        operands[index] = {exponent == 0 ? fraction : fraction | (std::uint64_t{1} << 52),
            exponent == 0 ? -1074 : exponent - 1023 - 52, (bits >> 63) != 0};
    }

    constexpr int kMinimumProductExponent = -2148;
    constexpr std::size_t kLimbs = 68;
    static_assert(kLimbs * 64 >= 2 * 971 + 106 - kMinimumProductExponent + 3);
    using Accumulator = std::array<std::uint64_t, kLimbs>;
    Accumulator positive{}, negative{};
    const auto addWord = [](Accumulator& accumulator, std::size_t index, std::uint64_t value)
    {
        while ( value != 0 )
        {
            if ( index >= accumulator.size() )
            {
                return false;
            }
            const auto previous = accumulator[index];
            accumulator[index] += value;
            value = accumulator[index] < previous ? 1 : 0;
            ++index;
        }
        return true;
    };
    constexpr std::uint64_t kWordMask = 0xffffffff;
    const auto appendProduct = [&](const ExactBinary64ProductTerm term)
    {
        if ( term.first >= operands.size() || term.second >= operands.size() )
        {
            return false;
        }
        const auto& first = operands[term.first];
        const auto& second = operands[term.second];
        if ( first.mantissa == 0 || second.mantissa == 0 )
        {
            return true;
        }
        const auto firstLow = first.mantissa & kWordMask;
        const auto firstHigh = first.mantissa >> 32;
        const auto secondLow = second.mantissa & kWordMask;
        const auto secondHigh = second.mantissa >> 32;
        const auto lowProduct = firstLow * secondLow;
        const auto firstCross = firstLow * secondHigh;
        const auto secondCross = firstHigh * secondLow;
        const auto middle = (lowProduct >> 32) + (firstCross & kWordMask)
            + (secondCross & kWordMask);
        const auto low = (lowProduct & kWordMask) | (middle << 32);
        const auto high = firstHigh * secondHigh + (firstCross >> 32)
            + (secondCross >> 32) + (middle >> 32);
        auto& accumulator = (first.negative != second.negative) != term.subtract
            ? negative : positive;
        const auto offset = static_cast<unsigned>(first.exponent + second.exponent
            - kMinimumProductExponent);
        const auto word = static_cast<std::size_t>(offset / 64);
        const auto shift = offset % 64;
        if ( shift == 0 )
        {
            return addWord(accumulator, word, low)
                && addWord(accumulator, word + 1, high);
        }
        return addWord(accumulator, word, low << shift)
            && addWord(accumulator, word + 1, (low >> (64 - shift)) | (high << shift))
            && addWord(accumulator, word + 2, high >> (64 - shift));
    };
    for ( const auto term : terms )
    {
        if ( !appendProduct(term) )
        {
            return std::nullopt;
        }
    }
    const auto compare = [](const Accumulator& first, const Accumulator& second)
    {
        for ( std::size_t index = first.size(); index-- > 0; )
        {
            if ( first[index] != second[index] )
            {
                return first[index] < second[index] ? -1 : 1;
            }
        }
        return 0;
    };
    const int comparison = compare(positive,negative);
    if ( comparison == 0 )
    {
        return ExactBinary64SignedMagnitude{};
    }
    const auto subtract = [](Accumulator& result, const Accumulator& first,
        const Accumulator& second)
    {
        std::uint64_t borrow = 0;
        for ( std::size_t index = 0; index < result.size(); ++index )
        {
            const auto secondWithBorrow = second[index] + borrow;
            const bool addBorrow = secondWithBorrow < second[index];
            result[index] = first[index] - secondWithBorrow;
            borrow = (addBorrow || first[index] < secondWithBorrow) ? 1 : 0;
        }
        return borrow == 0;
    };
    ExactBinary64SignedMagnitude result;
    result.zero = false;
    result.negative = comparison < 0;
    if ( comparison > 0 )
    {
        if ( !subtract(result.words,positive,negative) )
        {
            return std::nullopt;
        }
    }
    else if ( !subtract(result.words,negative,positive) )
    {
        return std::nullopt;
    }
    return result;
}

template <std::size_t ValueCount, std::size_t NumeratorTermCount, std::size_t DenominatorTermCount>
inline std::optional<ExactBinary64Ratio> exactBinary64ProductRatio(
    const std::array<double, ValueCount>& values,
    const std::array<ExactBinary64ProductTerm, NumeratorTermCount>& numeratorTerms,
    const std::array<ExactBinary64ProductTerm, DenominatorTermCount>& denominatorTerms) noexcept
{
    static_assert(ValueCount <= 8 && NumeratorTermCount <= 8 && DenominatorTermCount <= 8,
        "the binary64 ratio accumulator is bounded for eight terms per expression");
    const auto numerator = exactBinary64ProductSumMagnitude(values,numeratorTerms);
    const auto denominator = exactBinary64ProductSumMagnitude(values,denominatorTerms);
    if ( !numerator || !denominator || denominator->zero )
    {
        return std::nullopt;
    }
    if ( numerator->zero )
    {
        return ExactBinary64Ratio{0.0,0.0};
    }
    const auto highestBit = [](const auto& words)
    {
        for ( std::size_t index = words.size(); index-- > 0; )
        {
            if ( words[index] != 0 )
            {
                return static_cast<int>(index * 64 + (63 - std::countl_zero(words[index])));
            }
        }
        return -1;
    };
    const auto bitAt = [](const auto& words, const int index)
    {
        return index >= 0 && static_cast<std::size_t>(index / 64) < words.size()
            ? (words[static_cast<std::size_t>(index / 64)]
                >> static_cast<unsigned>(index % 64)) & 1u
            : 0u;
    };
    const auto leadingBits = [&](const auto& words, const int top)
    {
        std::uint64_t result = 0;
        const int first = std::max(0,top - 52);
        for ( int index = top; index >= first; --index )
        {
            result = (result << 1) | bitAt(words,index);
        }
        if ( top < 52 )
        {
            result <<= static_cast<unsigned>(52 - top);
        }
        return result;
    };
    const int numeratorTop = highestBit(numerator->words);
    const int denominatorTop = highestBit(denominator->words);
    if ( numeratorTop < 0 || denominatorTop < 0 )
    {
        return std::nullopt;
    }
    const double numeratorLeading = static_cast<double>(leadingBits(numerator->words,numeratorTop));
    const double denominatorLeading = static_cast<double>(leadingBits(denominator->words,denominatorTop));
    const int exponent = numeratorTop - denominatorTop;
    const double unsignedValue = std::ldexp(numeratorLeading / denominatorLeading,exponent);
    if ( !std::isfinite(unsignedValue) || unsignedValue == 0.0 )
    {
        return std::nullopt;
    }
    // Truncating each exact magnitude after its leading 53 bits contributes
    // at most 2^-52 relative error. The quotient division and its binary64
    // rounding add less than another 2^-52; sixteen epsilons leave room for
    // both signs, the exponent scaling, and the outward narrowing below.
    constexpr double kRelativeError = 16.0 * std::numeric_limits<double>::epsilon();
    const double error = kRelativeError * unsignedValue + std::numeric_limits<double>::denorm_min();
    if ( !std::isfinite(error) )
    {
        return std::nullopt;
    }
    const double roundedError = std::nextafter(error,std::numeric_limits<double>::infinity());
    return ExactBinary64Ratio{numerator->negative != denominator->negative ? -unsignedValue : unsignedValue,
        roundedError};
}

// Evaluate a bounded sum of products of four finite binary64 values as an
// exact dyadic integer. This is used by the line-intersection predicate below;
// its operands are the immutable endpoint coordinates, never a rounded
// reconstructed intersection. The product mantissas use eight base-2^32
// words, and the sum uses 264 such words, which covers the full binary64
// exponent range and the 128 terms in the expanded predicate.
template <std::size_t ValueCount, std::size_t TermCount>
inline std::optional<int> exactBinary64Product4SumSign(
    const std::array<double, ValueCount>& values,
    const std::array<ExactBinary64Product4Term, TermCount>& terms) noexcept
{
    static_assert(ValueCount <= 16 && TermCount <= 128,
        "the binary64 four-product accumulator is bounded for 128 terms");
    static_assert(sizeof(double) == sizeof(std::uint64_t)
        && std::numeric_limits<double>::is_iec559
        && std::numeric_limits<double>::radix == 2
        && std::numeric_limits<double>::digits == 53
        && std::numeric_limits<double>::min_exponent == -1021
        && std::numeric_limits<double>::max_exponent == 1024);
    struct Dyadic
    {
        std::uint64_t mantissa;
        int exponent;
        bool negative;
    };
    std::array<Dyadic, ValueCount> operands{};
    for ( std::size_t index = 0; index < values.size(); ++index )
    {
        if ( !std::isfinite(values[index]) )
        {
            return std::nullopt;
        }
        const auto bits = std::bit_cast<std::uint64_t>(values[index]);
        const auto exponent = static_cast<int>((bits >> 52) & 0x7ff);
        const auto fraction = bits & ((std::uint64_t{1} << 52) - 1);
        operands[index] = {exponent == 0 ? fraction : fraction | (std::uint64_t{1} << 52),
            exponent == 0 ? -1074 : exponent - 1023 - 52, (bits >> 63) != 0};
    }

    constexpr int kMinimumProductExponent = 4 * -1074;
    constexpr std::size_t kLimbs = 264;
    static_assert(kLimbs * 32 >= 4 * 971 + 4 * 53 - kMinimumProductExponent + 7,
        "the four-product dyadic accumulator must cover every finite binary64 product");
    using Accumulator = std::array<std::uint32_t, kLimbs>;
    Accumulator positive{}, negative{};
    constexpr std::uint64_t kWordMask = 0xffffffffu;
    const auto addWord = [](Accumulator& accumulator, std::size_t index, std::uint64_t value)
    {
        while ( value != 0 )
        {
            if ( index >= accumulator.size() )
            {
                return false;
            }
            const auto sum = static_cast<std::uint64_t>(accumulator[index]) + value;
            accumulator[index] = static_cast<std::uint32_t>(sum & kWordMask);
            value = sum >> 32;
            ++index;
        }
        return true;
    };

    for ( const auto& term : terms )
    {
        std::array<std::uint32_t, 8> product{};
        product[0] = 1;
        std::size_t used = 1;
        int exponent = 0;
        bool negativeProduct = term.subtract;
        bool zero = false;
        for ( const auto factor : term.factors )
        {
            if ( factor >= operands.size() )
            {
                return std::nullopt;
            }
            const auto& operand = operands[factor];
            exponent += operand.exponent;
            negativeProduct = negativeProduct != operand.negative;
            if ( operand.mantissa == 0 )
            {
                zero = true;
                continue;
            }

            const std::array<std::uint32_t, 2> multiplier{{
                static_cast<std::uint32_t>(operand.mantissa & kWordMask),
                static_cast<std::uint32_t>(operand.mantissa >> 32)}};
            const auto input = product;
            product.fill(0);
            for ( std::size_t first = 0; first < used; ++first )
            {
                std::uint64_t carry = 0;
                for ( std::size_t second = 0; second < multiplier.size(); ++second )
                {
                    const auto index = first + second;
                    const auto value = static_cast<std::uint64_t>(input[first]) * multiplier[second]
                        + product[index] + carry;
                    product[index] = static_cast<std::uint32_t>(value & kWordMask);
                    carry = value >> 32;
                }
                std::size_t index = first + multiplier.size();
                while ( carry != 0 )
                {
                    if ( index >= product.size() )
                    {
                        return std::nullopt;
                    }
                    const auto value = static_cast<std::uint64_t>(product[index]) + carry;
                    product[index] = static_cast<std::uint32_t>(value & kWordMask);
                    carry = value >> 32;
                    ++index;
                }
            }
            used = std::min(product.size(), used + multiplier.size());
        }
        if ( zero )
        {
            continue;
        }

        const int offset = exponent - kMinimumProductExponent;
        if ( offset < 0 )
        {
            return std::nullopt;
        }
        const auto word = static_cast<std::size_t>(offset / 32);
        const auto shift = static_cast<unsigned>(offset % 32);
        auto& accumulator = negativeProduct ? negative : positive;
        for ( std::size_t index = 0; index < used; ++index )
        {
            const auto shifted = static_cast<std::uint64_t>(product[index]) << shift;
            if ( !addWord(accumulator, word + index, shifted & kWordMask)
                || !addWord(accumulator, word + index + 1, shifted >> 32) )
            {
                return std::nullopt;
            }
        }
    }
    for ( std::size_t index = kLimbs; index > 0; --index )
    {
        if ( positive[index - 1] != negative[index - 1] )
        {
            return positive[index - 1] > negative[index - 1] ? 1 : -1;
        }
    }
    return 0;
}

// Exact orientation of three finite binary64 coordinate pairs. Coordinates
// are the immutable stored operands; no proximity allowance changes the sign.
// Nonfinite input returns nullopt, collinearity returns zero.
inline std::optional<int> exactOrientationSign(const double ax, const double ay,
    const double bx, const double by, const double cx, const double cy) noexcept
{
    const std::array<double, 6> values{{ax, ay, bx, by, cx, cy}};
    constexpr std::array<ExactBinary64ProductTerm, 6> terms{{
        {0,3,false}, {2,5,false}, {4,1,false},
        {1,2,true}, {3,4,true}, {5,0,true}}};
    return exactBinary64ProductSumSign(values, terms);
}

// Exact sign of (b-a) x (d-c), where each coordinate is an immutable
// binary64 operand. This avoids subtracting endpoint coordinates in binary64
// before deciding whether two independently authored line directions cross.
inline std::optional<int> exactDirectionCrossSign(const double ax, const double ay,
    const double bx, const double by, const double cx, const double cy,
    const double dx, const double dy) noexcept
{
    const std::array<double, 8> values{{ax,ay,bx,by,cx,cy,dx,dy}};
    // (bx-ax)*(dy-cy) - (by-ay)*(dx-cx), expanded without rounded
    // coordinate differences.
    constexpr std::array<ExactBinary64ProductTerm, 8> terms{{
        {2,7,false}, {2,5,true}, {0,7,true}, {0,5,false},
        {3,6,true}, {3,4,false}, {1,6,false}, {1,4,true}}};
    return exactBinary64ProductSumSign(values, terms);
}

// Exact sign of (b-a) dot (d-c), where each coordinate is an immutable
// binary64 operand. This is used to order authored points along a supporting
// line without first rounding either endpoint difference.
inline std::optional<int> exactDirectionDotSign(const double ax, const double ay,
    const double bx, const double by, const double cx, const double cy,
    const double dx, const double dy) noexcept
{
    const std::array<double, 8> values{{ax,ay,bx,by,cx,cy,dx,dy}};
    constexpr std::array<ExactBinary64ProductTerm, 8> terms{{
        {2,6,false}, {2,4,true}, {0,6,true}, {0,4,false},
        {3,7,false}, {3,5,true}, {1,7,true}, {1,5,false}}};
    return exactBinary64ProductSumSign(values, terms);
}

// Exact binary64 ratio used to reconstruct a line intersection parameter. It
// evaluates cross(C-A,D-C) / cross(B-A,D-C) from immutable endpoints and
// returns a conservative absolute conversion error for the finite result.
// A zero or nonfinite denominator is unresolved rather than approximated.
inline std::optional<ExactBinary64Ratio> exactLineIntersectionParameter(
    const double ax, const double ay, const double bx, const double by,
    const double cx, const double cy, const double dx, const double dy) noexcept
{
    const std::array<double, 8> values{{ax,ay,bx,by,cx,cy,dx,dy}};
    constexpr std::array<ExactBinary64ProductTerm, 8> numerator{ {
        {4,7,false}, {4,5,true}, {0,7,true}, {0,5,false},
        {5,6,true}, {5,4,false}, {1,6,false}, {1,4,true}} };
    constexpr std::array<ExactBinary64ProductTerm, 8> denominator{ {
        {2,7,false}, {2,5,true}, {0,7,true}, {0,5,false},
        {3,6,true}, {3,4,false}, {1,6,false}, {1,4,true}} };
    return exactBinary64ProductRatio(values,numerator,denominator);
}

// Exact side of the intersection of lines AB and CD relative to EF. The
// numerator is obtained from
//
//   f_EF(A) * cross(AB, CD) + cross(EF, AB) * cross(CA, CD)
//
// and the denominator is cross(AB, CD). Both expressions use immutable
// endpoint coordinates; no rounded intersection or propagated tolerance
// participates in the sign decision. A parallel defining pair returns an
// empty result, while an exact target-line hit returns zero.
inline std::optional<int> exactIntersectionSideSign(
    const double ax, const double ay, const double bx, const double by,
    const double cx, const double cy, const double dx, const double dy,
    const double ex, const double ey, const double fx, const double fy) noexcept
{
    if ( ex == fx && ey == fy )
    {
        return std::nullopt;
    }
    const std::array<double, 12> values{{ax,ay,bx,by,cx,cy,dx,dy,ex,ey,fx,fy}};
    using Product = ExactBinary64Product4Term;
    std::array<Product, 128> terms{};
    std::size_t count = 0;
    const auto appendCrossProduct = [&](const std::array<Product, 8>& first,
        const std::array<Product, 8>& second)
    {
        for ( const auto& left : first )
        {
            for ( const auto& right : second )
            {
                terms[count++] = {{{left.factors[0],left.factors[1],
                    right.factors[0],right.factors[1]}}, left.subtract != right.subtract};
            }
        }
    };
    const auto crossTerms = [](const std::size_t ux, const std::size_t uy,
        const std::size_t vx, const std::size_t vy, const std::size_t rx, const std::size_t ry,
        const std::size_t sx, const std::size_t sy)
    {
        return std::array<Product, 8>{{
            {{{ux,ry,0,0}}, false}, {{{ux,sy,0,0}}, true},
            {{{vx,ry,0,0}}, true},  {{{vx,sy,0,0}}, false},
            {{{uy,rx,0,0}}, true},  {{{uy,sx,0,0}}, false},
            {{{vy,rx,0,0}}, false}, {{{vy,sx,0,0}}, true}}};
    };
    // Each local cross expression is represented as a pair-product list. The
    // unused two factors are ignored while the lists are combined into four
    // factor terms below.
    const auto f0 = crossTerms(10,11,8,9,0,1,8,9);
    const auto denominator = crossTerms(2,3,0,1,6,7,4,5);
    const auto fDirection = crossTerms(10,11,8,9,2,3,0,1);
    const auto tNumerator = crossTerms(4,5,0,1,6,7,4,5);
    appendCrossProduct(f0, denominator);
    appendCrossProduct(fDirection, tNumerator);
    const auto numerator = exactBinary64Product4SumSign(values, terms);
    const auto direction = exactDirectionCrossSign(ax,ay,bx,by,cx,cy,dx,dy);
    if ( !numerator || !direction || *direction == 0 )
    {
        return std::nullopt;
    }
    return *numerator == 0 ? std::optional<int>{0}
        : std::optional<int>{*numerator * *direction};
}

} // namespace sed::level::detail
