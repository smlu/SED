#pragma once

#include <algorithm>
#include <cstddef>
#include <optional>

class QOpenGLBuffer;

namespace sed::render::upload
{

/**
 * Returns a grow-only byte capacity for GL buffer storage.
 *
 * Existing buffers grow by at least half their current size or 4 KiB,
 * whichever is larger. This matches the historical upload path while keeping
 * the policy reusable and unit-testable.
 */
int grownBufferCapacity(int currentBytes, int requiredBytes) noexcept;

/**
 * Converts a byte count to the signed integer size required by Qt GL APIs.
 *
 * Qt's QOpenGLBuffer::write/allocate methods accept int byte counts, so
 * oversize CPU buffers must be detected before upload instead of overflowing.
 */
std::optional<int> safeByteCount(std::size_t bytes) noexcept;

/**
 * Ensures a QOpenGLBuffer has enough allocated bytes.
 *
 * Returns true when the backing storage was reallocated. A non-positive
 * requirement is treated as an empty upload and does not allocate.
 */
bool ensureBufferCapacity(QOpenGLBuffer& buffer, int& allocatedBytes, int requiredBytes);

/**
 * Writes a buffer payload in bounded pieces and pulses after every piece.
 *
 * QOpenGLBuffer exposes only a synchronous write. A monolithic multi-megabyte
 * transfer can therefore prevent a delayed UI indicator from becoming
 * observable until publication has already ended. Bounded writes retain the
 * exact byte stream while giving the caller safe, non-reentrant repaint
 * points between driver calls.
 */
template <typename WriteChunk, typename ProgressPulse>
void writeBufferInProgressChunks(
    const int destinationOffset,
    const void* source,
    const int byteCount,
    WriteChunk&& writeChunk,
    ProgressPulse&& progressPulse,
    const int maxChunkBytes = 8 * 1024 * 1024)
{
    if (!source || byteCount <= 0 || maxChunkBytes <= 0)
    {
        return;
    }

    const auto* sourceBytes = static_cast<const std::byte*>(source);
    for (int written = 0; written < byteCount;)
    {
        const int chunkBytes = std::min(maxChunkBytes, byteCount - written);
        writeChunk(
            destinationOffset + written,
            sourceBytes + written,
            chunkBytes);
        written += chunkBytes;
        progressPulse();
    }
}

} // namespace sed::render::upload
