#include "render/mesh/render_mesh_utils.hpp"

#include <algorithm>

namespace sed::render
{

int pickIdKey(const float pickId) noexcept
{
    return static_cast<int>(pickId + 0.5f);
}

void includePoint(ThingBounds& bounds, const float x, const float y, const float z) noexcept
{
    if (!bounds.valid)
    {
        bounds.min = QVector3D(x, y, z);
        bounds.max = QVector3D(x, y, z);
        bounds.valid = true;
        return;
    }

    bounds.min.setX(std::min(bounds.min.x(), x));
    bounds.min.setY(std::min(bounds.min.y(), y));
    bounds.min.setZ(std::min(bounds.min.z(), z));
    bounds.max.setX(std::max(bounds.max.x(), x));
    bounds.max.setY(std::max(bounds.max.y(), y));
    bounds.max.setZ(std::max(bounds.max.z(), z));
}

void includePoint(ThingBounds& bounds, const QVector3D& point) noexcept
{
    includePoint(bounds, point.x(), point.y(), point.z());
}

void appendOrExtendDrawRange(std::vector<DrawRange>& ranges, const int first, const int count)
{
    if (count <= 0)
    {
        return;
    }

    if (!ranges.empty())
    {
        DrawRange& last = ranges.back();
        if (last.first + last.count == first)
        {
            last.count += count;
            return;
        }
    }

    ranges.push_back(DrawRange{ first, count });
}

void appendOrExtendTriBuildRange(
    std::vector<TriBuildRange>& ranges,
    const std::uint32_t buildIndex,
    const int first,
    const int count)
{
    if (count <= 0)
    {
        return;
    }

    if (!ranges.empty())
    {
        TriBuildRange& last = ranges.back();
        if (last.buildIndex == buildIndex && last.first + last.count == first)
        {
            last.count += count;
            return;
        }
    }

    ranges.push_back(TriBuildRange{ buildIndex, first, count });
}

void mergeDrawRanges(std::vector<DrawRange>& ranges)
{
    ranges.erase(
        std::remove_if(ranges.begin(), ranges.end(), [](const DrawRange& range) { return range.count <= 0; }),
        ranges.end());
    std::sort(ranges.begin(), ranges.end(), [](const DrawRange& a, const DrawRange& b) {
        return a.first < b.first;
    });

    std::vector<DrawRange> merged{};
    merged.reserve(ranges.size());
    for (const DrawRange& range : ranges)
    {
        if (merged.empty())
        {
            merged.push_back(range);
            continue;
        }

        DrawRange& last = merged.back();
        const int lastEnd = last.first + last.count;
        const int rangeEnd = range.first + range.count;
        if (range.first <= lastEnd)
        {
            last.count = std::max(lastEnd, rangeEnd) - last.first;
        }
        else
        {
            merged.push_back(range);
        }
    }

    ranges = std::move(merged);
}

} // namespace sed::render
