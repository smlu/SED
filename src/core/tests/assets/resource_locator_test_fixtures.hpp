#pragma once
#include <chrono>
#include <cstdint>
#include <stdexcept>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace sed::tests
{
struct ResourceLocatorFixture
{
    std::filesystem::path root = std::filesystem::temp_directory_path()
        / ("sed-resource-fixture-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::path firstGob = root / "first.gob";
    std::filesystem::path secondGob = root / "second.gob";
    ResourceLocatorFixture()
    {
        std::filesystem::create_directories(root);
        std::string bytes("GOB ", 4);
        const auto append = [&](std::uint32_t value)
        { for (int i=0; i<4; ++i) bytes += static_cast<char>((value >> (8*i)) & 255); };
        append(0x14); append(0x0c); append(1); append(152); append(1);
        std::string name = "3do/fixture.3do";
        name.resize(128, '\0');
        bytes += name;
        bytes += 'x';
        for (const auto& path : {firstGob, secondGob})
        {
            std::ofstream out(path, std::ios::binary);
            out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
            if (!out) throw std::runtime_error("Could not write resource locator fixture");
        }
    }
    ~ResourceLocatorFixture()
    {
        std::error_code ec;
        std::filesystem::remove_all(root, ec);
    }
};
}
