#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace sed
{
struct Thing;
}

namespace sed::render::frame_ghost
{

inline constexpr std::size_t kMaximumCachedFramePosesPerOwner = 65536u;

struct FramePose
{
    std::int32_t valueIndex{-1};
    double x{};
    double y{};
    double z{};
    double pitch{};
    double yaw{};
    double roll{};
    bool finiteOrientation{};
};

struct ParsedFramePoses
{
    std::vector<FramePose> poses{};
    std::size_t parseCalls{};
    bool complete{true};
};

/** Parse Frame values once, retaining their original value indices/order. */
[[nodiscard]] ParsedFramePoses parseFramePoses(const Thing& thing);

/** Convert one authored Frame pose to the column-major mat4 consumed by GLSL. */
[[nodiscard]] std::optional<std::array<float, 16u>> framePoseMatrix(
    const FramePose& pose) noexcept;

} // namespace sed::render::frame_ghost
