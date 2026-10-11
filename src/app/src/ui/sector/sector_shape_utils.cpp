#include "ui/sector/sector_shape_utils.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <limits>
#include <unordered_set>

#include <QCoreApplication>

#include "sed/text.hpp"

namespace sed::ui::sector
{
namespace
{

void toLowerAsciiInPlace(std::string& s) noexcept
{
    for (char& ch : s)
    {
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    }
}

std::string shapeTplToken(const std::string& value)
{
    return value.empty() ? std::string("-") : value;
}

class ShapeStreamState
{
public:
    explicit ShapeStreamState(std::ostream& out) : out_(out), flags_(out.flags()), precision_(out.precision())
    {
    }

    ~ShapeStreamState()
    {
        out_.flags(flags_);
        out_.precision(precision_);
    }

    ShapeStreamState(const ShapeStreamState&) = delete;
    ShapeStreamState& operator=(const ShapeStreamState&) = delete;

private:
    std::ostream& out_;
    std::ios::fmtflags flags_;
    std::streamsize precision_;
};

void writeShapeHex(std::ostream& out, const std::uint32_t value)
{
    ShapeStreamState scoped(out);
    out << std::uppercase << std::hex << value;
}

bool validateShapeUvData(const sed::Sector& sector, QString* error)
{
    for (std::size_t faceIndex = 0;
         faceIndex < sector.surfaces.size();
         ++faceIndex)
    {
        const sed::Surface& surface = sector.surfaces[faceIndex];
        if (surface.texVertices.size() != surface.vertices.size())
        {
            if (error)
            {
                *error = QStringLiteral(
                    "Selected sector face %1 must have one texture coordinate per polygon corner.")
                    .arg(static_cast<qulonglong>(faceIndex));
            }
            return false;
        }
        if (!std::isfinite(surface.uScale)
            || !std::isfinite(surface.vScale))
        {
            if (error)
            {
                *error = QStringLiteral(
                    "Selected sector face %1 has a non-finite UV scale.")
                    .arg(static_cast<qulonglong>(faceIndex));
            }
            return false;
        }
        for (std::size_t cornerIndex = 0;
             cornerIndex < surface.texVertices.size();
             ++cornerIndex)
        {
            const sed::TexVertex& coordinate =
                surface.texVertices[cornerIndex];
            if (!std::isfinite(coordinate.u)
                || !std::isfinite(coordinate.v))
            {
                if (error)
                {
                    *error = QStringLiteral(
                        "Selected sector face %1 corner %2 has a non-finite texture coordinate.")
                        .arg(static_cast<qulonglong>(faceIndex))
                        .arg(static_cast<qulonglong>(cornerIndex));
                }
                return false;
            }
        }
    }
    return true;
}

} // namespace

const std::array<ShapeBuiltin, 5>& builtinShapes() noexcept
{
    static constexpr std::array<ShapeBuiltin, 5> kBuiltins{ {
        { "Cube", sed::level::SectorPrimitive::Cube },
        { "Cone", sed::level::SectorPrimitive::Cone },
        { "Cylinder", sed::level::SectorPrimitive::Cylinder },
        { "UV Sphere", sed::level::SectorPrimitive::UvSphere },
        { "Ico Sphere", sed::level::SectorPrimitive::IcoSphere },
    } };
    return kBuiltins;
}

std::optional<sed::level::SectorPrimitive> primitiveForName(const std::string_view name)
{
    for (const ShapeBuiltin& builtin : builtinShapes())
    {
        if (sed::text::iequalsAscii(name, builtin.name))
        {
            return builtin.primitive;
        }
    }
    return std::nullopt;
}

QString shapeLabel(const std::string_view name)
{
    return QString::fromStdString(std::string(name));
}

std::vector<std::filesystem::path> shapeTplCandidatePaths()
{
    std::vector<std::filesystem::path> paths{};

    const std::filesystem::path appDir = QCoreApplication::applicationDirPath().toStdString();
    if (!appDir.empty())
    {
        paths.push_back(appDir / "data" / "shapes.tpl");
    }

    std::error_code ec;
    const std::filesystem::path cwd = std::filesystem::current_path(ec);
    if (!ec && !cwd.empty())
    {
        paths.push_back(cwd / "data" / "shapes.tpl");
        paths.push_back(cwd / ".." / "data" / "shapes.tpl");
        paths.push_back(cwd / ".." / ".." / "data" / "shapes.tpl");
    }

    std::sort(paths.begin(), paths.end());
    paths.erase(std::unique(paths.begin(), paths.end()), paths.end());
    return paths;
}

std::vector<sed::level::SectorShape> loadLegacyShapes()
{
    std::vector<sed::level::SectorShape> allShapes{};
    std::unordered_set<std::string> seenNames{};

    for (const std::filesystem::path& path : shapeTplCandidatePaths())
    {
        std::error_code ec;
        if (!std::filesystem::exists(path, ec) || ec)
        {
            continue;
        }

        std::ifstream input(path);
        if (!input)
        {
            continue;
        }

        std::vector<sed::level::SectorShape> shapes = sed::level::loadSectorShapesTpl(input);
        for (sed::level::SectorShape& shape : shapes)
        {
            std::string key = shape.name;
            toLowerAsciiInPlace(key);
            if (seenNames.insert(key).second)
            {
                allShapes.push_back(std::move(shape));
            }
        }
    }

    return allShapes;
}

std::string uniqueShapeName(const std::vector<sed::level::SectorShape>& existingShapes)
{
    std::unordered_set<std::string> existing{};
    for (const sed::level::SectorShape& shape : existingShapes)
    {
        std::string key = shape.name;
        toLowerAsciiInPlace(key);
        existing.insert(std::move(key));
    }

    for (int i = 1; i < 100000; ++i)
    {
        std::string name = "Sector Shape " + std::to_string(i);
        std::string key = name;
        toLowerAsciiInPlace(key);
        if (!existing.contains(key))
        {
            return name;
        }
    }

    return "Sector Shape";
}

std::vector<std::filesystem::path> shapeTplSaveCandidatePaths()
{
    std::vector<std::filesystem::path> candidates = shapeTplCandidatePaths();

    std::error_code ec;
    const std::filesystem::path cwd = std::filesystem::current_path(ec);
    if (!ec && !cwd.empty())
    {
        const std::filesystem::path defaultPath = cwd / "data" / "shapes.tpl";
        if (std::find(candidates.begin(), candidates.end(), defaultPath) == candidates.end())
        {
            candidates.push_back(defaultPath);
        }
    }

    std::vector<std::filesystem::path> ordered{};
    for (const std::filesystem::path& path : candidates)
    {
        std::error_code existsEc;
        if (std::filesystem::exists(path, existsEc) && !existsEc)
        {
            ordered.push_back(path);
        }
    }

    for (const std::filesystem::path& path : candidates)
    {
        std::error_code existsEc;
        if (!std::filesystem::exists(path, existsEc) && !existsEc)
        {
            ordered.push_back(path);
        }
    }

    ordered.erase(std::unique(ordered.begin(), ordered.end()), ordered.end());
    return ordered;
}

bool appendShapeTpl(
    const std::filesystem::path& path,
    const std::string& name,
    const sed::Sector& sector,
    const sed::UvCoordinateSpace uvCoordinateSpace,
    QString* error)
{
    if (sector.vertices.empty() || sector.surfaces.empty())
    {
        if (error)
        {
            *error = QStringLiteral("Selected sector has no reusable geometry.");
        }
        return false;
    }
    if (uvCoordinateSpace != sed::UvCoordinateSpace::TexturePixels
        && uvCoordinateSpace != sed::UvCoordinateSpace::Normalized)
    {
        if (error)
        {
            *error = QStringLiteral("Selected sector has an unknown UV coordinate space.");
        }
        return false;
    }
    // Complete logical validation must precede directory creation and opening
    // the append stream so a rejected shape cannot alter an existing file.
    if (!validateShapeUvData(sector, error))
    {
        return false;
    }

    std::error_code ec;
    const std::filesystem::path parent = path.parent_path();
    if (!parent.empty())
    {
        std::filesystem::create_directories(parent, ec);
        if (ec)
        {
            if (error)
            {
                *error = QStringLiteral("Could not create %1").arg(QString::fromStdString(parent.string()));
            }
            return false;
        }
    }

    const bool needsLeadingBlank =
        std::filesystem::exists(path, ec) && !ec && std::filesystem::file_size(path, ec) > 0 && !ec;

    std::ofstream out(path, std::ios::app);
    if (!out)
    {
        if (error)
        {
            *error = QStringLiteral("Could not write %1").arg(QString::fromStdString(path.string()));
        }
        return false;
    }

    if (needsLeadingBlank)
    {
        out << '\n';
    }

    out << "SEC " << name << '\n';

    {
        ShapeStreamState scoped(out);
        out << std::setprecision(6);
        out << shapeTplToken(sector.colormap) << ' ' << shapeTplToken(sector.sound) << ' ' << sector.soundVolume << ' ';
        writeShapeHex(out, static_cast<std::uint32_t>(sector.flags));
        out << ' ' << sector.ambient.intensity() << ' ' << sector.extraLight.intensity() << ' ' << sector.tint.r << ' ' << sector.tint.g << ' '
            << sector.tint.b << ' ' << sector.layer << " UV "
            << (uvCoordinateSpace == sed::UvCoordinateSpace::Normalized
                    ? "normalized"
                    : "pixels")
            << '\n';
    }

    out << "VXS " << sector.vertices.size() << '\n';
    {
        ShapeStreamState scoped(out);
        out << std::setprecision(6);
        for (const sed::Vertex& vertex : sector.vertices)
        {
            out << vertex.x << ' ' << vertex.y << ' ' << vertex.z << '\n';
        }
    }

    out << "SURFS " << sector.surfaces.size() << '\n';
    for (const sed::Surface& surface : sector.surfaces)
    {
        {
            ShapeStreamState scoped(out);
            out << std::setprecision(6);
            out << shapeTplToken(surface.material) << ' ';
            writeShapeHex(out, static_cast<std::uint32_t>(surface.surfaceFlags));
            out << ' ';
            writeShapeHex(out, surface.faceFlags);
            out << ' ' << surface.geo << ' ' << surface.light << ' ' << surface.tex << ' '
                << sed::clampSignedUnit(surface.extraLight.intensity()) << ' '
                << surface.vertices.size() << " -1 -1 ";
            writeShapeHex(out, static_cast<std::uint32_t>(surface.adjoinFlags));
            out << std::setprecision(std::numeric_limits<float>::max_digits10)
                << ' ' << surface.uScale << ' ' << surface.vScale << '\n';
        }

        {
            ShapeStreamState scoped(out);
            for (std::size_t i = 0; i < surface.vertices.size(); ++i)
            {
                const sed::TexVertex& tv = surface.texVertices[i];
                out << ' ' << surface.vertices[i];
                out << std::setprecision(std::numeric_limits<float>::max_digits10)
                    << ' ' << tv.u << ' ' << tv.v;
                out << std::setprecision(4) << ' ' << tv.color.a;
            }
            out << std::setprecision(4);
            for (std::size_t i = 0; i < surface.vertices.size(); ++i)
            {
                const sed::TexVertex& tv = surface.texVertices[i];
                out << ' ' << tv.color.r << ' ' << tv.color.g << ' ' << tv.color.b;
            }
            out << '\n';
        }
    }

    out << "END\n";
    if (!out)
    {
        if (error)
        {
            *error = QStringLiteral("Could not finish writing %1").arg(QString::fromStdString(path.string()));
        }
        return false;
    }

    return true;
}

std::optional<std::filesystem::path> saveShapeTpl(
    const std::string& name,
    const sed::Sector& sector,
    const sed::UvCoordinateSpace uvCoordinateSpace,
    QString* error)
{
    QString lastError{};
    for (const std::filesystem::path& path : shapeTplSaveCandidatePaths())
    {
        QString pathError{};
        if (appendShapeTpl(
                path, name, sector, uvCoordinateSpace, &pathError))
        {
            return path;
        }
        if (!pathError.isEmpty())
        {
            lastError = pathError;
        }
    }

    if (error)
    {
        *error = lastError.isEmpty() ? QStringLiteral("No writable shapes.tpl path found.") : lastError;
    }
    return std::nullopt;
}

std::optional<sed::level::SectorShape> legacyShapeByName(
    const std::string_view name)
{
    for (const sed::level::SectorShape& shape : loadLegacyShapes())
    {
        if (sed::text::iequalsAscii(shape.name, name))
        {
            return shape;
        }
    }
    return std::nullopt;
}

sed::level::LevelUvConversionResult admitPlacedSectorShapeUvs(
    sed::Level& level,
    const std::int32_t sectorIndex,
    const sed::UvCoordinateSpace source,
    const sed::level::UvTextureDimensionsResolver& resolveDimensions)
{
    if (sectorIndex < 0
        || static_cast<std::size_t>(sectorIndex) >= level.sectors.size())
    {
        return { {}, "Placed Sector shape index is invalid." };
    }

    return sed::level::convertSectorUvCoordinateSpace(
        level.sectors[static_cast<std::size_t>(sectorIndex)],
        sectorIndex,
        source,
        level.uvCoordinateSpace,
        resolveDimensions);
}

} // namespace sed::ui::sector
