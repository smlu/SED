#pragma once

#include <cstddef>
#include <cstdint>
#include <unordered_set>

#include "sed/level.hpp"
#include "sed/level/transform.hpp"

namespace sed::editor
{

struct SurfaceRefHash
{
    std::size_t operator()(const sed::SurfaceRef& ref) const noexcept
    {
        const std::uint64_t a = static_cast<std::uint32_t>(ref.sector);
        const std::uint64_t b = static_cast<std::uint32_t>(ref.surface);
        return static_cast<std::size_t>((a << 32u) ^ b);
    }
};

struct SurfaceRefEq
{
    bool operator()(const sed::SurfaceRef& a, const sed::SurfaceRef& b) const noexcept
    {
        return a.sector == b.sector && a.surface == b.surface;
    }
};

struct FrameRefHash
{
    std::size_t operator()(const sed::level::FrameRef& ref) const noexcept
    {
        const std::uint64_t a = static_cast<std::uint32_t>(ref.thing);
        const std::uint64_t b = static_cast<std::uint32_t>(ref.value);
        return static_cast<std::size_t>((a << 32u) ^ b);
    }
};

struct FrameRefEq
{
    bool operator()(const sed::level::FrameRef& a, const sed::level::FrameRef& b) const noexcept
    {
        return a.thing == b.thing && a.value == b.value;
    }
};

struct SurfaceVertexRef
{
    sed::SurfaceRef surface{};
    std::int32_t localVertex{ -1 };

    constexpr bool operator==(const SurfaceVertexRef&) const noexcept = default;
};

struct SurfaceVertexRefHash
{
    std::size_t operator()(const SurfaceVertexRef& ref) const noexcept
    {
        const std::uint64_t a = static_cast<std::uint32_t>(ref.surface.sector);
        const std::uint64_t b = static_cast<std::uint32_t>(ref.surface.surface);
        const std::uint64_t c = static_cast<std::uint32_t>(ref.localVertex);
        return static_cast<std::size_t>(((a << 32u) ^ b) ^ (c * 2654435761u));
    }
};

struct SurfaceVertexRefEq
{
    bool operator()(const SurfaceVertexRef& a, const SurfaceVertexRef& b) const noexcept
    {
        return a.surface.sector == b.surface.sector
            && a.surface.surface == b.surface.surface
            && a.localVertex == b.localVertex;
    }
};

struct Selection
{
    std::unordered_set<std::int32_t> sectors;
    std::unordered_set<sed::SurfaceRef, SurfaceRefHash, SurfaceRefEq> surfaces;
    std::unordered_set<SurfaceVertexRef, SurfaceVertexRefHash, SurfaceVertexRefEq> surfaceVertices;
    std::unordered_set<std::int32_t> things;
    std::unordered_set<sed::level::FrameRef, FrameRefHash, FrameRefEq> frames;
    std::unordered_set<std::int32_t> lights;
    std::unordered_set<std::int32_t> cogs;

    void clear()
    {
        sectors.clear();
        surfaces.clear();
        surfaceVertices.clear();
        things.clear();
        frames.clear();
        lights.clear();
        cogs.clear();
    }

    bool empty() const noexcept
    {
        return sectors.empty() && surfaces.empty() && surfaceVertices.empty() && things.empty() && frames.empty() && lights.empty() && cogs.empty();
    }
};

} // namespace sed::editor
