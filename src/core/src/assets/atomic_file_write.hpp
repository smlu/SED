#pragma once

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <span>
#include <stdexcept>
#include <string>
#include <system_error>
#include <utility>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif

namespace sed::assets::detail
{
enum class StagedAssetFileFaultPointForTests : std::uint8_t
{
    none,
    write,
    flush,
    close,
    publish,
};

struct StagedAssetFileFaultStateForTests
{
    StagedAssetFileFaultPointForTests point{
        StagedAssetFileFaultPointForTests::none };
    bool triggered{};
};

inline thread_local StagedAssetFileFaultStateForTests
    stagedAssetFileFaultStateForTests{};

class ScopedStagedAssetFileFaultForTests
{
public:
    explicit ScopedStagedAssetFileFaultForTests(
        const StagedAssetFileFaultPointForTests point) noexcept
        : previous_(stagedAssetFileFaultStateForTests)
    {
        stagedAssetFileFaultStateForTests = { point, false };
    }

    ScopedStagedAssetFileFaultForTests(
        const ScopedStagedAssetFileFaultForTests&) = delete;
    ScopedStagedAssetFileFaultForTests& operator=(
        const ScopedStagedAssetFileFaultForTests&) = delete;

    ~ScopedStagedAssetFileFaultForTests()
    {
        stagedAssetFileFaultStateForTests = previous_;
    }

    bool triggered() const noexcept
    {
        return stagedAssetFileFaultStateForTests.triggered;
    }

private:
    StagedAssetFileFaultStateForTests previous_{};
};

inline void stagedAssetFileFaultCheckpointForTests(
    const StagedAssetFileFaultPointForTests point)
{
    if ( stagedAssetFileFaultStateForTests.point != point )
    {
        return;
    }

    stagedAssetFileFaultStateForTests.point =
        StagedAssetFileFaultPointForTests::none;
    stagedAssetFileFaultStateForTests.triggered = true;
    throw std::runtime_error("Injected staged asset file failure.");
}

// Own a newly and exclusively created sibling. Never truncate the destination
// or remove it before replacement: serialization, write, flush, close, and
// publication failures must retain the last complete file.
class StagedAssetFile
{
public:
    explicit StagedAssetFile(const std::filesystem::path& destination)
    {
        static std::atomic<std::uint64_t> serial{};
        const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        for ( unsigned attempt = 0; attempt < 128; ++attempt )
        {
            path_ = destination.parent_path() / (".sed-save-"
                + std::to_string(stamp) + "-"
                + std::to_string(serial.fetch_add(1, std::memory_order_relaxed)) + ".tmp");
#ifdef _WIN32
            handle_ = CreateFileW(path_.c_str(), GENERIC_WRITE, FILE_SHARE_READ,
                nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
            if ( handle_ != INVALID_HANDLE_VALUE ) return;
            const DWORD error = GetLastError();
            if ( error != ERROR_FILE_EXISTS && error != ERROR_ALREADY_EXISTS )
            {
                throw std::system_error(static_cast<int>(error), std::system_category(),
                    "Could not stage asset file");
            }
#else
            descriptor_ = ::open(path_.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0666);
            if ( descriptor_ >= 0 ) return;
            const int error = errno;
            if ( error != EEXIST )
            {
                throw std::system_error(error, std::generic_category(),
                    "Could not stage asset file");
            }
#endif
        }
        throw std::runtime_error("Could not create a unique asset staging file.");
    }

    StagedAssetFile(const StagedAssetFile&) = delete;
    StagedAssetFile& operator=(const StagedAssetFile&) = delete;

    ~StagedAssetFile()
    {
#ifdef _WIN32
        if ( handle_ != INVALID_HANDLE_VALUE ) CloseHandle(handle_);
#else
        if ( descriptor_ >= 0 ) ::close(descriptor_);
#endif
        if ( !published_ )
        {
            std::error_code error{};
            std::filesystem::remove(path_, error);
        }
    }

    void writeAndClose(const std::span<const std::byte> bytes)
    {
        std::size_t offset{};
        while ( offset < bytes.size() )
        {
            const std::size_t count = std::min<std::size_t>(bytes.size() - offset, 1024 * 1024);
#ifdef _WIN32
            DWORD written{};
            if ( !WriteFile(handle_, bytes.data() + offset,
                    static_cast<DWORD>(count), &written, nullptr) )
            {
                throw std::system_error(static_cast<int>(GetLastError()), std::system_category(),
                    "Failed to write asset staging file");
            }
#else
            const auto written = ::write(descriptor_, bytes.data() + offset, count);
            if ( written < 0 )
            {
                if ( errno == EINTR ) continue;
                throw std::system_error(errno, std::generic_category(),
                    "Failed to write asset staging file");
            }
#endif
            if ( written == 0 ) throw std::runtime_error("Short write of asset staging file.");
            offset += static_cast<std::size_t>(written);
        }
        stagedAssetFileFaultCheckpointForTests(
            StagedAssetFileFaultPointForTests::write);
#ifdef _WIN32
        stagedAssetFileFaultCheckpointForTests(
            StagedAssetFileFaultPointForTests::flush);
        if ( !FlushFileBuffers(handle_) )
        {
            throw std::system_error(static_cast<int>(GetLastError()), std::system_category(),
                "Failed to flush asset staging file");
        }
        stagedAssetFileFaultCheckpointForTests(
            StagedAssetFileFaultPointForTests::close);
        const HANDLE handle = std::exchange(handle_, INVALID_HANDLE_VALUE);
        if ( !CloseHandle(handle) )
        {
            throw std::system_error(static_cast<int>(GetLastError()), std::system_category(),
                "Failed to close asset staging file");
        }
#else
        stagedAssetFileFaultCheckpointForTests(
            StagedAssetFileFaultPointForTests::flush);
        while ( ::fsync(descriptor_) != 0 )
        {
            if ( errno == EINTR ) continue;
            throw std::system_error(errno, std::generic_category(),
                "Failed to flush asset staging file");
        }
        stagedAssetFileFaultCheckpointForTests(
            StagedAssetFileFaultPointForTests::close);
        const int descriptor = std::exchange(descriptor_, -1);
        if ( ::close(descriptor) != 0 )
        {
            throw std::system_error(errno, std::generic_category(),
                "Failed to close asset staging file");
        }
#endif
    }

    void publish(const std::filesystem::path& destination)
    {
        stagedAssetFileFaultCheckpointForTests(
            StagedAssetFileFaultPointForTests::publish);
#ifdef _WIN32
        // The sibling stays on the same volume. Do not enable COPY_ALLOWED,
        // whose fallback would turn publication into a non-atomic copy.
        if ( !MoveFileExW(path_.c_str(), destination.c_str(),
                MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) )
        {
            throw std::system_error(static_cast<int>(GetLastError()), std::system_category(),
                "Could not publish asset file");
        }
#else
        if ( ::rename(path_.c_str(), destination.c_str()) != 0 )
        {
            throw std::system_error(errno, std::generic_category(),
                "Could not publish asset file");
        }
#endif
        published_ = true;
    }

private:
    std::filesystem::path path_{};
    bool published_{};
#ifdef _WIN32
    HANDLE handle_{INVALID_HANDLE_VALUE};
#else
    int descriptor_{-1};
#endif
};
} // namespace
