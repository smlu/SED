#include "render/upload/buffer_upload_utils.hpp"

#include <algorithm>
#include <limits>

#include <QOpenGLBuffer>

namespace sed::render::upload
{

int grownBufferCapacity(const int currentBytes, const int requiredBytes) noexcept
{
    if (requiredBytes <= currentBytes)
    {
        return currentBytes;
    }

    if (currentBytes <= 0)
    {
        return requiredBytes;
    }

    const int half = currentBytes / 2;
    const int grown = currentBytes + std::max(half, 4096);
    return std::max(requiredBytes, grown);
}

std::optional<int> safeByteCount(const std::size_t bytes) noexcept
{
    const std::size_t maxInt = static_cast<std::size_t>(std::numeric_limits<int>::max());
    if (bytes > maxInt)
    {
        return std::nullopt;
    }
    return static_cast<int>(bytes);
}

bool ensureBufferCapacity(QOpenGLBuffer& buffer, int& allocatedBytes, const int requiredBytes)
{
    if (requiredBytes <= 0)
    {
        return false;
    }

    if (allocatedBytes >= requiredBytes)
    {
        return false;
    }

    const int newCapacity = grownBufferCapacity(allocatedBytes, requiredBytes);
    buffer.allocate(nullptr, newCapacity);
    allocatedBytes = newCapacity;
    return true;
}

} // namespace sed::render::upload
