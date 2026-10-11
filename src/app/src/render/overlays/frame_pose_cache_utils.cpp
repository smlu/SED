#include "frame_pose_cache_utils.hpp"

#include "sed/geometry.hpp"
#include "sed/level.hpp"
#include "sed/text.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace sed::render::frame_ghost
{

ParsedFramePoses parseFramePoses(const Thing& thing)
{
    ParsedFramePoses result{};
    std::size_t frameValuesSeen = 0u;
    result.poses.reserve(std::min(
        thing.values.size(),
        kMaximumCachedFramePosesPerOwner));

    for ( std::size_t valueIndex = 0u; valueIndex < thing.values.size(); ++valueIndex )
    {
        const TplValue& value = thing.values[valueIndex];
        if ( value.type != ValueType::frame
            || !sed::text::iequalsAscii(value.name, "frame") )
        {
            continue;
        }

        if ( frameValuesSeen >= kMaximumCachedFramePosesPerOwner )
        {
            result.complete = false;
            break;
        }
        ++frameValuesSeen;

        FramePose pose{};
        if ( valueIndex > static_cast<std::size_t>(
                std::numeric_limits<std::int32_t>::max()) )
        {
            result.complete = false;
            break;
        }
        pose.valueIndex = static_cast<std::int32_t>(valueIndex);
        ++result.parseCalls;
        if ( !value.tryGetFrame(
                pose.x,
                pose.y,
                pose.z,
                pose.pitch,
                pose.yaw,
                pose.roll)
            || !std::isfinite(pose.x)
            || !std::isfinite(pose.y)
            || !std::isfinite(pose.z) )
        {
            continue;
        }

        pose.finiteOrientation =
            std::isfinite(pose.pitch)
            && std::isfinite(pose.yaw)
            && std::isfinite(pose.roll);
        result.poses.push_back(pose);
    }

    return result;
}

std::optional<std::array<float, 16u>> framePoseMatrix(
    const FramePose& pose) noexcept
{
    if ( !pose.finiteOrientation )
    {
        return std::nullopt;
    }

    TVector position{};
    position.x = pose.x;
    position.y = pose.y;
    position.z = pose.z;
    TVector orientation{};
    orientation.pitch = pose.pitch;
    orientation.yaw = pose.yaw;
    orientation.roll = pose.roll;
    const TMatrix43 transform = TMatrix43::Create(position, orientation);

    const std::array<float, 16u> matrix{
        static_cast<float>(transform.rvec.x),
        static_cast<float>(transform.rvec.y),
        static_cast<float>(transform.rvec.z),
        0.0f,
        static_cast<float>(transform.lvec.x),
        static_cast<float>(transform.lvec.y),
        static_cast<float>(transform.lvec.z),
        0.0f,
        static_cast<float>(transform.uvec.x),
        static_cast<float>(transform.uvec.y),
        static_cast<float>(transform.uvec.z),
        0.0f,
        static_cast<float>(transform.dvec.x),
        static_cast<float>(transform.dvec.y),
        static_cast<float>(transform.dvec.z),
        1.0f,
    };
    for ( const float component : matrix )
    {
        if ( !std::isfinite(component) )
        {
            return std::nullopt;
        }
    }
    return matrix;
}

} // namespace sed::render::frame_ghost
