#pragma once

#include <cstdint>
#include <filesystem>
#include <istream>
#include <string>
#include <vector>

#include "sed/geometry.hpp"

namespace sed::assets
{

struct KeyEntry
{
    std::string label;
    std::int32_t frame{};
    std::uint32_t flags{};

    TVector position{};
    TVector rotation{};

    TVector dPosition{};
    TVector dRotation{};
};

struct KeyNode
{
    std::string meshName;
    std::vector<KeyEntry> entries;
    // The NODE number identifies the model joint, even when tracks are sparse.
    std::int32_t nodeIndex{ -1 };
};

struct KeyFile
{
    std::uint32_t flags{};
    std::int32_t frames{};
    std::int32_t fps{};
    std::vector<KeyNode> nodes;
    std::uint32_t type{};
    // JOINTS includes joints without tracks. A missing count is distinct from zero.
    std::int32_t joints{ -1 };
};

KeyFile loadFromKEY(std::istream& in);
KeyFile loadFromKEY(const std::filesystem::path& path);

} // namespace sed::assets
