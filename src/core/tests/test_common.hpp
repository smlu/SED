#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <initializer_list>
#include <iostream>
#include <ostream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "sed/geometry.hpp"
#include "sed/level/geometry.hpp"
#include "sed/math_utils.hpp"

namespace sed::tests
{

inline int failures = 0;

/** Exact-ish tolerance for deterministic transform branches. */
inline constexpr double kExactEpsilon = sed::math::kTinyEpsilon;

/** Geometry tolerance for legacy text/model calculations. */
inline constexpr double kGeometryEpsilon = sed::math::kGeometryEpsilon;

/** Rotation/matrix tolerance for trig-heavy paths. */
inline constexpr double kRotationEpsilon = sed::math::kRotationEpsilon;

inline void check(const bool ok, const char* msg)
{
    if (ok)
    {
        return;
    }

    ++failures;
    std::cerr << "FAIL: " << msg << "\n";
}

template <typename Function>
inline void checkThrowsWithMessage(
    Function&& function,
    const std::string_view expectedMessage,
    const char* const message)
{
    try
    {
        std::forward<Function>(function)();
    }
    catch (const std::exception& exception)
    {
        const std::string_view actualMessage(exception.what());
        const bool matches = actualMessage == expectedMessage;
        check(matches, message);
        if (!matches)
        {
            std::cerr << "  expected: [" << expectedMessage
                      << "]\n  actual:   [" << actualMessage << "]\n";
        }
        return;
    }
    catch (...)
    {
        check(false, message);
        return;
    }

    check(false, message);
}

inline bool close(const double a, const double b, const double eps = kRotationEpsilon)
{
    return sed::math::equal(a, b, eps);
}

inline bool closeVec(const sed::TVector& a, const sed::TVector& b, const double eps = kRotationEpsilon)
{
    return close(a.x, b.x, eps) && close(a.y, b.y, eps) && close(a.z, b.z, eps);
}

/** Appends a test surface and recalculates its normal from the sector geometry. */
inline void appendTestSurface(sed::Sector& sector, const std::initializer_list<std::int32_t> indices)
{
    sed::Surface surface{};
    surface.vertices.assign(indices.begin(), indices.end());
    surface.normal = sed::level::surfaceCalcNormalNewell(sector, surface);
    sector.surfaces.push_back(std::move(surface));
}

/** Appends a test surface with uniform vertex light color. */
inline void appendTestSurface(
    sed::Sector& sector,
    const std::initializer_list<std::int32_t> indices,
    const sed::ColorF& vertexColor)
{
    sed::Surface surface{};
    surface.vertices.assign(indices.begin(), indices.end());
    surface.texVertices.resize(surface.vertices.size());
    for (sed::TexVertex& texVertex : surface.texVertices)
    {
        texVertex.color = vertexColor;
    }
    surface.normal = sed::level::surfaceCalcNormalNewell(sector, surface);
    sector.surfaces.push_back(std::move(surface));
}

/** Appends the canonical eight cube vertices used by level geometry tests. */
inline void appendTestCubeVertices(sed::Sector& sector, const double offsetX)
{
    sector.addVertex(-1.0 + offsetX, -1.0, -1.0);
    sector.addVertex(1.0 + offsetX, -1.0, -1.0);
    sector.addVertex(1.0 + offsetX, 1.0, -1.0);
    sector.addVertex(-1.0 + offsetX, 1.0, -1.0);
    sector.addVertex(-1.0 + offsetX, -1.0, 1.0);
    sector.addVertex(1.0 + offsetX, -1.0, 1.0);
    sector.addVertex(1.0 + offsetX, 1.0, 1.0);
    sector.addVertex(-1.0 + offsetX, 1.0, 1.0);
}

/** Builds a closed cube sector with calculated SED-wound face normals. */
inline sed::Sector makeTestCubeSector(const double offsetX)
{
    sed::Sector sector{};
    appendTestCubeVertices(sector, offsetX);

    appendTestSurface(sector, { 1, 5, 6, 2 });
    appendTestSurface(sector, { 0, 3, 7, 4 });
    appendTestSurface(sector, { 3, 2, 6, 7 });
    appendTestSurface(sector, { 0, 4, 5, 1 });
    appendTestSurface(sector, { 4, 7, 6, 5 });
    appendTestSurface(sector, { 0, 1, 2, 3 });

    return sector;
}

/** Builds a closed cube sector whose surfaces all start with one light color. */
inline sed::Sector makeLitTestCubeSector(const double offsetX, const sed::ColorF& vertexColor)
{
    sed::Sector sector{};
    appendTestCubeVertices(sector, offsetX);

    appendTestSurface(sector, { 1, 5, 6, 2 }, vertexColor);
    appendTestSurface(sector, { 0, 3, 7, 4 }, vertexColor);
    appendTestSurface(sector, { 3, 2, 6, 7 }, vertexColor);
    appendTestSurface(sector, { 0, 4, 5, 1 }, vertexColor);
    appendTestSurface(sector, { 4, 7, 6, 5 }, vertexColor);
    appendTestSurface(sector, { 0, 1, 2, 3 }, vertexColor);

    return sector;
}

inline void writeU32LE(std::ostream& out, const std::uint32_t value)
{
    const std::array<unsigned char, 4> bytes{
        static_cast<unsigned char>(value & 0xFFu),
        static_cast<unsigned char>((value >> 8) & 0xFFu),
        static_cast<unsigned char>((value >> 16) & 0xFFu),
        static_cast<unsigned char>((value >> 24) & 0xFFu),
    };

    out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
}

inline void appendU32LE(
    std::vector<std::byte>& bytes,
    const std::uint32_t value)
{
    bytes.push_back(static_cast<std::byte>(value & 0xffu));
    bytes.push_back(static_cast<std::byte>((value >> 8u) & 0xffu));
    bytes.push_back(static_cast<std::byte>((value >> 16u) & 0xffu));
    bytes.push_back(static_cast<std::byte>((value >> 24u) & 0xffu));
}

/** Builds the smallest indexed one-cel MAT accepted by the production parser. */
inline std::vector<std::byte> makeIndexedMatFixture(
    const std::int32_t width,
    const std::int32_t height)
{
    std::vector<std::byte> bytes{
        std::byte{ 'M' },
        std::byte{ 'A' },
        std::byte{ 'T' },
        std::byte{ ' ' },
    };
    appendU32LE(bytes, 0x32u);
    appendU32LE(bytes, 2u);
    appendU32LE(bytes, 1u);
    appendU32LE(bytes, 1u);

    appendU32LE(bytes, 1u);
    appendU32LE(bytes, 8u);
    for (std::size_t index = 0; index < 12u; ++index)
    {
        appendU32LE(bytes, 0u);
    }

    appendU32LE(bytes, 8u);
    appendU32LE(bytes, 0u);
    for (std::size_t index = 0; index < 8u; ++index)
    {
        appendU32LE(bytes, 0u);
    }

    appendU32LE(bytes, static_cast<std::uint32_t>(width));
    appendU32LE(bytes, static_cast<std::uint32_t>(height));
    appendU32LE(bytes, 0u);
    appendU32LE(bytes, 0u);
    appendU32LE(bytes, 0u);
    appendU32LE(bytes, 1u);
    bytes.resize(
        bytes.size()
        + static_cast<std::size_t>(width)
            * static_cast<std::size_t>(height));
    return bytes;
}

} // namespace sed::tests
