#pragma once

#include <array>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <QString>

#include "sed/level.hpp"
#include "sed/level/build.hpp"
#include "sed/level/uv.hpp"

namespace sed::ui::sector
{

struct ShapeBuiltin
{
    const char* name{};
    sed::level::SectorPrimitive primitive{};
};

const std::array<ShapeBuiltin, 5>& builtinShapes() noexcept;

std::optional<sed::level::SectorPrimitive> primitiveForName(std::string_view name);
QString shapeLabel(std::string_view name);

std::vector<std::filesystem::path> shapeTplCandidatePaths();
std::vector<sed::level::SectorShape> loadLegacyShapes();
std::string uniqueShapeName(const std::vector<sed::level::SectorShape>& existingShapes);

bool appendShapeTpl(
    const std::filesystem::path& path,
    const std::string& name,
    const sed::Sector& sector,
    sed::UvCoordinateSpace uvCoordinateSpace,
    QString* error);
std::optional<std::filesystem::path> saveShapeTpl(
    const std::string& name,
    const sed::Sector& sector,
    sed::UvCoordinateSpace uvCoordinateSpace,
    QString* error);
std::optional<sed::level::SectorShape> legacyShapeByName(std::string_view name);

/** Admits a copied shape Sector into the owning Level's declared UV space.
 * New normalized shapes are a no-op; untagged legacy shapes are converted
 * exactly once through real material dimensions.
 */
sed::level::LevelUvConversionResult admitPlacedSectorShapeUvs(
    sed::Level& level,
    std::int32_t sectorIndex,
    sed::UvCoordinateSpace source,
    const sed::level::UvTextureDimensionsResolver& resolveDimensions);

} // namespace sed::ui::sector
