#pragma once

#include <array>
#include <filesystem>
#include <istream>
#include <ostream>
#include <string>

namespace sed::assets
{

struct SprFile
{
    std::string material{};
    int type{};
    float width{};
    float height{};
    int geometryMode{};
    int lightMode{};
    int textureMode{};
    std::array<float, 4> extraLightRGBA{ 0.0f, 0.0f, 0.0f, 1.0f };
    std::array<float, 3> offsetXYZ{ 0.0f, 0.0f, 0.0f };
};

/** Selects the classic scalar-extra-light or IJIM RGBA SPR text profile. */
enum class SprProfile
{
    classic,
    ijim,
};

std::string matFromSPR(std::istream& in);
std::string matFromSPR(const std::filesystem::path& path);

SprFile loadFromSPR(std::istream& in);
SprFile loadFromSPR(const std::filesystem::path& path);

/** Serializes one SPR record; material names are limited to 31 classic or 63 IJIM bytes. */
std::string saveToSPRText(const SprFile& spr, SprProfile profile);
void saveToSPRText(std::ostream& out, const SprFile& spr, SprProfile profile);

} // namespace sed::assets
