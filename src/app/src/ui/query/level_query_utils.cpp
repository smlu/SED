#include "ui/query/level_query_utils.hpp"
#include "ui/components/dialog_window_policy.hpp"
#include "ui/components/ui_decimal_format.hpp"

#include "sed/level/thing.hpp"
#include "sed/text.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>
#include <string>
#include <vector>

#include <QDialog>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QSignalBlocker>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>
#include <QVariant>

namespace sed::ui::query
{

std::vector<LevelQuerySpec::Criterion> levelQueryEffectiveCriteria(const LevelQuerySpec& spec)
{
    if (!spec.criteria.empty())
    {
        std::vector<LevelQuerySpec::Criterion> criteria{};
        criteria.reserve(spec.criteria.size());
        for (const LevelQuerySpec::Criterion& criterion : spec.criteria)
        {
            if (QString::fromStdString(criterion.value).trimmed().isEmpty())
            {
                continue;
            }
            criteria.push_back(criterion);
        }
        return criteria;
    }

    if (!QString::fromStdString(spec.value).trimmed().isEmpty())
    {
        return { LevelQuerySpec::Criterion{ spec.field, spec.matchMode, spec.value } };
    }

    return {};
}

LevelQueryEntityKind levelQueryEntityKindFromInt(const int value) noexcept
{
    switch (value)
    {
        case 0: return LevelQueryEntityKind::sector;
        case 1: return LevelQueryEntityKind::surface;
        case 2: return LevelQueryEntityKind::thing;
        default: return LevelQueryEntityKind::surface;
    }
}

LevelQueryField levelQueryFieldFromInt(const int value) noexcept
{
    switch (value)
    {
        case 0: return LevelQueryField::sectorId;
        case 1: return LevelQueryField::sectorLayer;
        case 2: return LevelQueryField::sectorColormap;
        case 3: return LevelQueryField::sectorSound;
        case 4: return LevelQueryField::sectorNumber;
        case 5: return LevelQueryField::sectorFlags;
        case 6: return LevelQueryField::surfaceMaterial;
        case 7: return LevelQueryField::surfaceSurfaceFlags;
        case 8: return LevelQueryField::surfaceFaceFlags;
        case 9: return LevelQueryField::surfaceAdjoinSector;
        case 10: return LevelQueryField::surfaceAdjoinSurface;
        case 11: return LevelQueryField::surfaceGeo;
        case 12: return LevelQueryField::surfaceLight;
        case 13: return LevelQueryField::surfaceNumber;
        case 14: return LevelQueryField::surfaceGlobalNumber;
        case 15: return LevelQueryField::thingName;
        case 16: return LevelQueryField::thingTemplate;
        case 17: return LevelQueryField::thingSector;
        case 18: return LevelQueryField::thingLayer;
        case 19: return LevelQueryField::thingType;
        case 20: return LevelQueryField::thingNumber;
        case 21: return LevelQueryField::thingFlags;
        case 22: return LevelQueryField::thingModel;
        case 23: return LevelQueryField::thingSprite;
        case 24: return LevelQueryField::thingCog;
        case 25: return LevelQueryField::sectorSurfaceCount;
        case 26: return LevelQueryField::sectorExtraLight;
        case 27: return LevelQueryField::sectorTint;
        case 28: return LevelQueryField::sectorSoundVolume;
        case 29: return LevelQueryField::surfaceAdjoinFlags;
        case 30: return LevelQueryField::surfaceExtraLight;
        case 31: return LevelQueryField::surfaceTex;
        case 32: return LevelQueryField::surfaceLayer;
        case 33: return LevelQueryField::thingPch;
        case 34: return LevelQueryField::thingYaw;
        case 35: return LevelQueryField::thingRol;
        case 36: return LevelQueryField::thingX;
        case 37: return LevelQueryField::thingY;
        case 38: return LevelQueryField::thingZ;
        default: return LevelQueryField::surfaceMaterial;
    }
}

LevelQueryResultMode levelQueryResultModeFromInt(const int value) noexcept
{
    switch (value)
    {
        case 0: return LevelQueryResultMode::focus;
        case 1: return LevelQueryResultMode::add;
        case 2: return LevelQueryResultMode::subtract;
        default: return LevelQueryResultMode::focus;
    }
}

LevelQueryMatchMode levelQueryMatchModeFromInt(const int value) noexcept
{
    switch (value)
    {
        case 0: return LevelQueryMatchMode::contains;
        case 1: return LevelQueryMatchMode::exact;
        case 2: return LevelQueryMatchMode::startsWith;
        case 3: return LevelQueryMatchMode::endsWith;
        case 4: return LevelQueryMatchMode::notEqual;
        case 5: return LevelQueryMatchMode::above;
        case 6: return LevelQueryMatchMode::below;
        case 7: return LevelQueryMatchMode::notContains;
        case 8: return LevelQueryMatchMode::bitSet;
        case 9: return LevelQueryMatchMode::bitNotSet;
        default: return LevelQueryMatchMode::contains;
    }
}

LevelQueryFieldKind levelQueryFieldKind(const LevelQueryField field) noexcept
{
    switch (field)
    {
        case LevelQueryField::sectorId:
        case LevelQueryField::sectorNumber:
        case LevelQueryField::sectorSurfaceCount:
        case LevelQueryField::surfaceAdjoinSector:
        case LevelQueryField::surfaceAdjoinSurface:
        case LevelQueryField::surfaceGeo:
        case LevelQueryField::surfaceLight:
        case LevelQueryField::surfaceTex:
        case LevelQueryField::surfaceNumber:
        case LevelQueryField::surfaceGlobalNumber:
        case LevelQueryField::thingSector:
        case LevelQueryField::thingNumber:
            return LevelQueryFieldKind::integer;

        case LevelQueryField::sectorFlags:
        case LevelQueryField::surfaceAdjoinFlags:
        case LevelQueryField::surfaceSurfaceFlags:
        case LevelQueryField::surfaceFaceFlags:
        case LevelQueryField::thingFlags:
            return LevelQueryFieldKind::flags;

        case LevelQueryField::sectorSoundVolume:
        case LevelQueryField::thingPch:
        case LevelQueryField::thingYaw:
        case LevelQueryField::thingRol:
        case LevelQueryField::thingX:
        case LevelQueryField::thingY:
        case LevelQueryField::thingZ:
            return LevelQueryFieldKind::real;

        case LevelQueryField::sectorExtraLight:
        case LevelQueryField::sectorTint:
        case LevelQueryField::surfaceExtraLight:
            return LevelQueryFieldKind::color;

        case LevelQueryField::sectorLayer:
        case LevelQueryField::sectorColormap:
        case LevelQueryField::sectorSound:
        case LevelQueryField::surfaceMaterial:
        case LevelQueryField::surfaceLayer:
        case LevelQueryField::thingName:
        case LevelQueryField::thingTemplate:
        case LevelQueryField::thingLayer:
        case LevelQueryField::thingType:
        case LevelQueryField::thingModel:
        case LevelQueryField::thingSprite:
        case LevelQueryField::thingCog:
            return LevelQueryFieldKind::string;
    }

    return LevelQueryFieldKind::string;
}

const std::vector<std::pair<LevelQueryField, QString>>& levelQueryFieldChoices(const LevelQueryEntityKind kind)
{
    static const std::vector<std::pair<LevelQueryField, QString>> kSectorFields{
        { LevelQueryField::sectorNumber, QStringLiteral("Number") },
        { LevelQueryField::sectorSurfaceCount, QStringLiteral("Surfaces") },
        { LevelQueryField::sectorFlags, QStringLiteral("Flags") },
        { LevelQueryField::sectorExtraLight, QStringLiteral("Extra Light") },
        { LevelQueryField::sectorColormap, QStringLiteral("Colormap") },
        { LevelQueryField::sectorTint, QStringLiteral("Tint") },
        { LevelQueryField::sectorSound, QStringLiteral("Sound") },
        { LevelQueryField::sectorSoundVolume, QStringLiteral("Volume") },
        { LevelQueryField::sectorLayer, QStringLiteral("Layer") },
        { LevelQueryField::sectorId, QStringLiteral("Sector ID") },
    };
    static const std::vector<std::pair<LevelQueryField, QString>> kSurfaceFields{
        { LevelQueryField::surfaceNumber, QStringLiteral("Number") },
        { LevelQueryField::surfaceMaterial, QStringLiteral("Material") },
        { LevelQueryField::surfaceAdjoinSector, QStringLiteral("Adjoined Sector") },
        { LevelQueryField::surfaceAdjoinSurface, QStringLiteral("Adjoined Surface") },
        { LevelQueryField::surfaceAdjoinFlags, QStringLiteral("Adjoin Flags") },
        { LevelQueryField::surfaceSurfaceFlags, QStringLiteral("Surface Flags") },
        { LevelQueryField::surfaceFaceFlags, QStringLiteral("Face Flags") },
        { LevelQueryField::surfaceGeo, QStringLiteral("Geo") },
        { LevelQueryField::surfaceLight, QStringLiteral("Light") },
        { LevelQueryField::surfaceTex, QStringLiteral("Tex") },
        { LevelQueryField::surfaceExtraLight, QStringLiteral("Extra Light") },
        { LevelQueryField::surfaceLayer, QStringLiteral("Layer") },
        { LevelQueryField::surfaceGlobalNumber, QStringLiteral("Global Surface Number") },
    };
    static const std::vector<std::pair<LevelQueryField, QString>> kThingFields{
        { LevelQueryField::thingNumber, QStringLiteral("Number") },
        { LevelQueryField::thingName, QStringLiteral("Name") },
        { LevelQueryField::thingSector, QStringLiteral("Sector") },
        { LevelQueryField::thingPch, QStringLiteral("PCH") },
        { LevelQueryField::thingYaw, QStringLiteral("YAW") },
        { LevelQueryField::thingRol, QStringLiteral("ROL") },
        { LevelQueryField::thingX, QStringLiteral("X") },
        { LevelQueryField::thingY, QStringLiteral("Y") },
        { LevelQueryField::thingZ, QStringLiteral("Z") },
        { LevelQueryField::thingLayer, QStringLiteral("Layer") },
        { LevelQueryField::thingTemplate, QStringLiteral("Template") },
        { LevelQueryField::thingType, QStringLiteral("Type") },
        { LevelQueryField::thingFlags, QStringLiteral("Thing Flags") },
        { LevelQueryField::thingModel, QStringLiteral("Model") },
        { LevelQueryField::thingSprite, QStringLiteral("Sprite") },
        { LevelQueryField::thingCog, QStringLiteral("COG") },
    };

    switch (kind)
    {
        case LevelQueryEntityKind::sector: return kSectorFields;
        case LevelQueryEntityKind::surface: return kSurfaceFields;
        case LevelQueryEntityKind::thing: return kThingFields;
    }

    return kSurfaceFields;
}

LevelQueryField normalizeLevelQueryField(const LevelQueryEntityKind kind, const LevelQueryField preferred) noexcept
{
    for (const auto& option : levelQueryFieldChoices(kind))
    {
        if (option.first == preferred)
        {
            return preferred;
        }
    }

    const auto& options = levelQueryFieldChoices(kind);
    if (!options.empty())
    {
        return options.front().first;
    }

    return LevelQueryField::surfaceMaterial;
}

const std::vector<std::pair<LevelQueryMatchMode, QString>>& levelQueryMatchChoices(const LevelQueryField field)
{
    static const std::vector<std::pair<LevelQueryMatchMode, QString>> kStringModes{
        { LevelQueryMatchMode::exact, QStringLiteral("=") },
        { LevelQueryMatchMode::notEqual, QStringLiteral("<>") },
        { LevelQueryMatchMode::above, QStringLiteral(">") },
        { LevelQueryMatchMode::below, QStringLiteral("<") },
        { LevelQueryMatchMode::contains, QStringLiteral("Contains") },
        { LevelQueryMatchMode::startsWith, QStringLiteral("Starts with") },
        { LevelQueryMatchMode::endsWith, QStringLiteral("Ends with") },
        { LevelQueryMatchMode::notContains, QStringLiteral("Doesn't contain") },
    };
    static const std::vector<std::pair<LevelQueryMatchMode, QString>> kNumberModes{
        { LevelQueryMatchMode::exact, QStringLiteral("Is") },
        { LevelQueryMatchMode::notEqual, QStringLiteral("Is not") },
        { LevelQueryMatchMode::above, QStringLiteral(">") },
        { LevelQueryMatchMode::below, QStringLiteral("<") },
        { LevelQueryMatchMode::bitSet, QStringLiteral("Set") },
        { LevelQueryMatchMode::bitNotSet, QStringLiteral("No set") },
    };
    static const std::vector<std::pair<LevelQueryMatchMode, QString>> kFlagModes{
        { LevelQueryMatchMode::exact, QStringLiteral("Is") },
        { LevelQueryMatchMode::notEqual, QStringLiteral("Is not") },
        { LevelQueryMatchMode::above, QStringLiteral(">") },
        { LevelQueryMatchMode::below, QStringLiteral("<") },
        { LevelQueryMatchMode::bitSet, QStringLiteral("Set") },
        { LevelQueryMatchMode::bitNotSet, QStringLiteral("No set") },
    };

    switch (levelQueryFieldKind(field))
    {
        case LevelQueryFieldKind::flags:
            return kFlagModes;
        case LevelQueryFieldKind::integer:
        case LevelQueryFieldKind::real:
        case LevelQueryFieldKind::color:
            return kNumberModes;
        case LevelQueryFieldKind::string:
        default:
            return kStringModes;
    }
}

LevelQueryMatchMode normalizeLevelQueryMatchMode(
    const LevelQueryField field,
    const LevelQueryMatchMode preferred) noexcept
{
    for (const auto& option : levelQueryMatchChoices(field))
    {
        if (option.first == preferred)
        {
            return preferred;
        }
    }

    const auto& options = levelQueryMatchChoices(field);
    if (!options.empty())
    {
        return options.front().first;
    }

    return LevelQueryMatchMode::contains;
}

QString levelQueryEntityLabel(const LevelQueryEntityKind kind, const int count)
{
    switch (kind)
    {
        case LevelQueryEntityKind::sector:
            return (count == 1) ? QStringLiteral("sector") : QStringLiteral("sectors");
        case LevelQueryEntityKind::surface:
            return (count == 1) ? QStringLiteral("surface") : QStringLiteral("surfaces");
        case LevelQueryEntityKind::thing:
            return (count == 1) ? QStringLiteral("thing") : QStringLiteral("things");
    }

    return QStringLiteral("entities");
}

QString levelQueryDialogTitle(const std::optional<LevelQueryEntityKind> forcedKind)
{
    if (!forcedKind.has_value())
    {
        return QStringLiteral("Find");
    }

    switch (*forcedKind)
    {
        case LevelQueryEntityKind::sector:
            return QStringLiteral("Find sectors");
        case LevelQueryEntityKind::surface:
            return QStringLiteral("Find surfaces");
        case LevelQueryEntityKind::thing:
            return QStringLiteral("Find things");
    }

    return QStringLiteral("Find");
}

QString levelQueryDialogIntro(const std::optional<LevelQueryEntityKind> forcedKind)
{
    if (!forcedKind.has_value())
    {
        return QStringLiteral("Find sectors, surfaces, or things by editor-visible properties. "
                              "Use Select All to select every matching entity.");
    }

    switch (*forcedKind)
    {
        case LevelQueryEntityKind::sector:
            return QStringLiteral("Find sectors by number, surface count, flags, colormap, sound, "
                                  "layer, and other sector properties.");
        case LevelQueryEntityKind::surface:
            return QStringLiteral("Find surfaces by material, adjoin data, flags, geometry, light, "
                                  "texture settings, and other surface properties.");
        case LevelQueryEntityKind::thing:
            return QStringLiteral("Find things by name, position, orientation, sector, layer, "
                                  "template, model, sprite, COG, and other thing properties.");
    }

    return QStringLiteral("Find sectors, surfaces, or things by editor-visible properties.");
}

QString levelQueryPlaceholder(const LevelQueryField field)
{
    switch (field)
    {
        case LevelQueryField::sectorId:
        case LevelQueryField::sectorNumber:
        case LevelQueryField::sectorFlags:
        case LevelQueryField::sectorSurfaceCount:
        case LevelQueryField::surfaceSurfaceFlags:
        case LevelQueryField::surfaceFaceFlags:
        case LevelQueryField::surfaceAdjoinFlags:
        case LevelQueryField::surfaceAdjoinSector:
        case LevelQueryField::surfaceAdjoinSurface:
        case LevelQueryField::surfaceGeo:
        case LevelQueryField::surfaceLight:
        case LevelQueryField::surfaceTex:
        case LevelQueryField::surfaceNumber:
        case LevelQueryField::surfaceGlobalNumber:
        case LevelQueryField::thingSector:
        case LevelQueryField::thingNumber:
        case LevelQueryField::thingFlags:
            return QStringLiteral("Enter number or hex value, for example 16 or 0x10");
        case LevelQueryField::sectorSoundVolume:
        case LevelQueryField::thingPch:
        case LevelQueryField::thingYaw:
        case LevelQueryField::thingRol:
        case LevelQueryField::thingX:
        case LevelQueryField::thingY:
        case LevelQueryField::thingZ:
            return QStringLiteral("Enter number, for example %1").arg(formatUiNumber(0.5, 'f', 6));
        case LevelQueryField::sectorExtraLight:
        case LevelQueryField::sectorTint:
        case LevelQueryField::surfaceExtraLight:
            return QStringLiteral("Enter color, for example %1 %1 %1 or %1 %1 %1 %2")
                .arg(formatUiNumber(0.25, 'f', 3), formatUiNumber(0.0, 'f', 3));
        case LevelQueryField::sectorLayer:
        case LevelQueryField::surfaceLayer:
        case LevelQueryField::thingLayer:
            return QStringLiteral("Enter layer name or number");
        case LevelQueryField::surfaceMaterial:
            return QStringLiteral("Enter material name, for example dflt.mat");
        case LevelQueryField::sectorColormap:
            return QStringLiteral("Enter colormap name");
        case LevelQueryField::sectorSound:
            return QStringLiteral("Enter sector sound name");
        case LevelQueryField::thingName:
            return QStringLiteral("Enter thing instance name");
        case LevelQueryField::thingTemplate:
            return QStringLiteral("Enter template name");
        case LevelQueryField::thingType:
            return QStringLiteral("Enter resolved thing type");
        case LevelQueryField::thingModel:
            return QStringLiteral("Enter 3DO model name");
        case LevelQueryField::thingSprite:
            return QStringLiteral("Enter sprite name");
        case LevelQueryField::thingCog:
            return QStringLiteral("Enter COG script name");
    }

    return QStringLiteral("Enter value...");
}

QString levelQueryHex(std::uint32_t value)
{
    return QString("0x%1").arg(value, 0, 16);
}

bool levelQueryUsesFlagPicker(const LevelQueryField field) noexcept
{
    switch (field)
    {
        case LevelQueryField::sectorFlags:
        case LevelQueryField::surfaceAdjoinFlags:
        case LevelQueryField::surfaceSurfaceFlags:
        case LevelQueryField::surfaceFaceFlags:
        case LevelQueryField::thingFlags:
            return true;
        default:
            return false;
    }
}

bool levelQueryUsesColorPicker(const LevelQueryField field) noexcept
{
    switch (field)
    {
        case LevelQueryField::sectorExtraLight:
        case LevelQueryField::sectorTint:
        case LevelQueryField::surfaceExtraLight:
            return true;
        default:
            return false;
    }
}

ColorF levelQueryDefaultColor(const LevelQueryField field) noexcept
{
    switch (field)
    {
        case LevelQueryField::sectorTint:
            return ColorWhite;
        case LevelQueryField::sectorExtraLight:
        case LevelQueryField::surfaceExtraLight:
        default:
            return ColorBlack;
    }
}

bool levelQueryColorPickerShowsAlpha(const LevelQueryField field) noexcept
{
    return field == LevelQueryField::surfaceExtraLight;
}

QString levelQueryColorText(const ColorF& color, const bool includeAlpha)
{
    if (includeAlpha)
    {
        return QStringLiteral("%1 %2 %3 %4")
            .arg(formatUiNumber(color.r, 'f', 3))
            .arg(formatUiNumber(color.g, 'f', 3))
            .arg(formatUiNumber(color.b, 'f', 3))
            .arg(formatUiNumber(color.a, 'f', 3));
    }

    return QStringLiteral("%1 %2 %3")
        .arg(formatUiNumber(color.r, 'f', 3))
        .arg(formatUiNumber(color.g, 'f', 3))
        .arg(formatUiNumber(color.b, 'f', 3));
}

std::vector<LevelQueryFlagDef> levelQueryFlagDefs(const LevelQueryField field, const ProjectType levelKind)
{
    switch (field)
    {
        case LevelQueryField::sectorFlags:
        {
            std::vector<LevelQueryFlagDef> defs{
                { 0x01u, "No gravity", "Sector has no gravity." },
                { 0x02u, "Underwater", "Sector is underwater." },
                { 0x04u, "COG linked", "Sector is linked to COG logic." },
                { 0x08u, "Use thrust", "Sector has thrust behavior enabled." },
                { 0x10u, "Hide on map", "Do not show sector on the map." },
                { 0x20u, "No AI actor", "AI actors cannot enter this sector." },
                { 0x40u, "Fall death", "Player is instantly killed when falling into this sector." },
                { 0x80u, "Adjoins off", "All sector adjoins are not rendered. Engine flag.", false },
                { 0x1000u, "Collide box", "Sector has collision box data. Usually auto-set." },
                { 0x4000u, "Seen", "Sector has been seen by the camera. Engine flag." },
                { static_cast<std::uint32_t>(::sed::SECF_NOAMBINETLIGHT), "No ambient light", "Skip ambient light contribution during baked light calculation." },
                { static_cast<std::uint32_t>(::sed::SECF_3DO), "Preview as 3DO", "Show the sector inverted in preview. SED/editor flag." },
            };
            if (levelKind == ProjectType::IJIM)
            {
                defs.push_back({ static_cast<std::uint32_t>(::sed::SECF_IJIM_Aetherim), "Aetherium", "Sector is part of the aetherium." });
                defs.push_back({ static_cast<std::uint32_t>(::sed::SECF_NORGBAMBINETLIGHT), "No RGB ambient light", "Calculate only ambient light intensity when baking lights." });
            }
            return defs;
        }

        case LevelQueryField::surfaceAdjoinFlags:
            return {
                { static_cast<std::uint32_t>(::sed::SAF_Visible), "Visible", "Render geometry through this adjoin." },
                { static_cast<std::uint32_t>(::sed::SAF_Move), "Allow movement", "Allow movement through the adjoin portal." },
                { static_cast<std::uint32_t>(::sed::SAF_AllowSoundPass), "Allow sound pass", "Allow sound to propagate through the adjoin." },
                { static_cast<std::uint32_t>(::sed::SAF_NoAiMove), "No AI move", "Block movement for AI actors (requires Move)." },
                { static_cast<std::uint32_t>(::sed::SAF_NoPlayerMove), "No player move", "Block movement for player (requires Move)." },
                { static_cast<std::uint32_t>(::sed::SAF_BlockLight), "Block light", "SED flag: block baked light transfer across the adjoin." },
            };

        case LevelQueryField::surfaceSurfaceFlags:
        {
            std::vector<LevelQueryFlagDef> defs{
                { 0x01u, "Floor", "Surface is ground floor that thing objects can move on." },
                { 0x02u, "COG linked", "Surface is linked to COG logic." },
                { 0x04u, "Collision", "Solid surface with face collision." },
                { 0x08u, "No AI move", "AI actors cannot move onto the surface." },
                { 0x10u, "2x scrolling size", "Double scrolling surface size." },
                { 0x20u, "1/2 scrolling size", "Half scrolling surface size." },
                { 0x40u, "1/8 scrolling size", "Eighth scrolling surface size." },
                { 0x200u, "Horizon sky", "Surface is part of the horizon sky." },
                { 0x400u, "Ceiling sky", "Surface is part of the ceiling sky." },
                { 0x800u, "Scrolling", "Surface is currently scrolling/sliding." },
                { 0x10000u, "Metal", "Metal surface type." },
                { 0x40000u, "Shallow water", "Surface plays shallow-water sounds." },
                { 0x80000u, "Dirt", "Dirt surface type." },
            };
            if (levelKind == ProjectType::IJIM)
            {
                defs.push_back({ 0x80u, "Aetherium", "Aetherium surface type." });
                defs.push_back({ 0x1000u, "Kill floor", "Actor, player, or weapon is destroyed on contact." });
                defs.push_back({ 0x2000u, "Climbable", "Player can climb this surface." });
                defs.push_back({ 0x4000u, "Track", "Railroad track surface type." });
                defs.push_back({ 0x8000u, "Changed", "Engine flag marking the surface as changed.", false });
                defs.push_back({ 0x20000u, "Water", "Water surface type." });
                defs.push_back({ 0x100000u, "Web", "Spider web surface type." });
                defs.push_back({ 0x200000u, "Lava", "Lava surface type." });
                defs.push_back({ 0x400000u, "Snow", "Snow surface type." });
                defs.push_back({ 0x800000u, "Wood", "Wood surface type." });
                defs.push_back({ 0x1000000u, "Ledge", "Surface can be grabbed as a ledge." });
                defs.push_back({ 0x2000000u, "Water ledge", "Climb-out ledge from water." });
                defs.push_back({ 0x4000000u, "1/4 scrolling size", "Quarter scrolling surface size." });
                defs.push_back({ 0x8000000u, "4x scrolling size", "Quadruple scrolling surface size." });
                defs.push_back({ 0x10000000u, "Whip aim", "Triggers whip-aim assistance." });
                defs.push_back({ 0x20000000u, "Echo", "Echo / indoor surface type." });
                defs.push_back({ 0x40000000u, "Wood echo", "Wood echo / hollow surface type." });
                defs.push_back({ 0x80000000u, "Dirt echo", "Dirt echo / hollow surface type." });
            }
            else
            {
                defs.push_back({ 0x80u, "No damage from fall", "Disable fall-damage on this surface." });
                defs.push_back({ 0x1000u, "Icy", "Icy surface type." });
                defs.push_back({ 0x2000u, "Very icy", "Very icy surface type." });
                defs.push_back({ 0x4000u, "Magsealed", "Reflects shots." });
                defs.push_back({ 0x20000u, "Deep water", "Deep-water surface type." });
                defs.push_back({ 0x100000u, "Very deep water", "Very deep-water surface type." });
            }
            return defs;
        }

        case LevelQueryField::surfaceFaceFlags:
        {
            std::vector<LevelQueryFlagDef> defs{
                { ::sed::FF_DoubleSided, "Double sided", "Disable backface culling (mostly for non-world geometry)." },
                { ::sed::FF_Transluent, "Translucent", "Enable texture transparency blending." },
                { ::sed::FF_TexClampX, "Clamp texture X", "Clamp texture in U instead of wrapping." },
                { ::sed::FF_TexClampY, "Clamp texture Y", "Clamp texture in V instead of wrapping." },
                { ::sed::FF_TexNoFiltering, "Disable filtering", "Use nearest filtering (pixelated look)." },
                { ::sed::FF_ZWriteDisabled, "Disable Z-write", "Do not write this surface into depth buffer." },
            };
            if (levelKind == ProjectType::IJIM)
            {
                defs.push_back({ ::sed::FF_IJIM_3doLedge, "3DO ledge (IJIM)", "3DO face supports ledge interaction." });
                defs.push_back({ ::sed::FF_IJIM_FogEnabled, "Fog enabled (IJIM)", "Enable fog blend on this face (non-sky surfaces)." });
                defs.push_back({ ::sed::FF_IJIM_3doWhipAim, "3DO whip aim (IJIM)", "3DO face participates in whip-aim interactions." });
            }
            return defs;
        }

        case LevelQueryField::thingFlags:
        {
            auto bitMask = [](const int bit) -> std::uint32_t
            {
                return static_cast<std::uint32_t>(1u) << bit;
            };

            std::vector<LevelQueryFlagDef> defs(32);
            for (int bit = 0; bit < 32; ++bit)
            {
                const std::uint32_t mask = bitMask(bit);
                defs[static_cast<std::size_t>(bit)] = LevelQueryFlagDef{
                    mask,
                    QStringLiteral("Unknown"),
                    QStringLiteral("No known meaning in editor; preserved as a raw bit."),
                };
            }

            auto setKnown = [&](const int bit, const QString& name, const QString& desc)
            {
                if (bit < 0 || bit >= 32)
                {
                    return;
                }
                defs[static_cast<std::size_t>(bit)] = LevelQueryFlagDef{
                    bitMask(bit),
                    name,
                    desc,
                };
            };

            setKnown(0, "0x01 - Emits light", "Object emits dynamic light.");
            setKnown(1, "0x02 - Dead", "Object is dead.");
            setKnown(4, "0x10 - Invisible", "Object is invisible but still interactable.");
            setKnown(6, "0x40 - Stand on", "Object can be stood on.");
            setKnown(9, "0x200 - Dying", "Object is about to be removed from the game.");
            setKnown(10, "0x400 - COG linked", "Object is linked in COG script logic.");
            setKnown(11, "0x800 - No crush", levelKind == ProjectType::IJIM
                    ? QStringLiteral("Object cannot be crushed by other objects.")
                    : QStringLiteral("Object does not receive impact damage."));
            setKnown(19, "0x80000 - Disabled", "Object is disabled and hidden.");
            setKnown(22, "0x400000 - Metal", "Object is metal-typed for sound effects.");
            setKnown(23, "0x800000 - Dirt", "Object is dirt-typed for sound effects.");
            setKnown(24, "0x1000000 - No sound", "Object produces no sound effects.");
            setKnown(27, "0x8000000 - Destroyed in water", "Object is destroyed when entering water.");
            setKnown(28, "0x10000000 - Destroyed in air", "Object is destroyed when entering air.");
            setKnown(29, "0x20000000 - Makes splash", "Sends splash message to linked COGs.");

            if (levelKind == ProjectType::IJIM)
            {
                setKnown(2, "0x04 - No weapon collision", "Disable collision against weapons/projectiles.");
                setKnown(3, "0x08 - Whip climb", "Player can whip this object and climb.");
                setKnown(7, "0x80 - Mountable", "Object can be mounted.");
                setKnown(8, "0x100 - Remote", "Non-local object controlled remotely over network.");
                setKnown(13, "0x2000 - Wood", "Object is wood-typed for sound effects.");
                setKnown(14, "0x4000 - Has shadow", "Object has shadow rendered.");
                setKnown(15, "0x8000 - Jeep block", "Blocks jeep player movement.");
                setKnown(16, "0x10000 - Snow", "Object is snow-typed for sound effects.");
                setKnown(17, "0x20000 - Pulse set", "Object has pulse timer set by engine.");
                setKnown(18, "0x40000 - Timer set", "Object has timer set by engine.");
                setKnown(20, "0x100000 - Seen", "Object has been seen by camera (engine flag).");
                setKnown(25, "0x2000000 - Submerged", "Object is submerged in water or aetherium.");
                setKnown(26, "0x4000000 - Climbable crate", "Object is climbable crate-type.");
                setKnown(30, "0x40000000 - Movable", "Object is pushable/pullable.");
                setKnown(31, "0x80000000 - Whip swing", "Player can whip this object and swing.");
            }
            else
            {
                setKnown(2, "0x04 - Magsealed", "Reflects shots.");
                setKnown(3, "0x08 - Architecture", "Object should be rendered as world architecture.");
                setKnown(8, "0x100 - Controlled remotely", "Object is controlled remotely over network.");
                setKnown(12, "0x1000 - Not on Easy", "Object is hidden in easy difficulty.");
                setKnown(13, "0x2000 - Not on Medium", "Object is hidden in medium difficulty.");
                setKnown(14, "0x4000 - Not on Hard", "Object is hidden in hard difficulty.");
                setKnown(15, "0x8000 - Not on Multiplayer", "Object is hidden in multiplayer.");
                setKnown(16, "0x10000 - Not on Singleplayer", "Object is hidden in singleplayer.");
                setKnown(25, "0x2000000 - Submerged", "Object is submerged.");
                setKnown(26, "0x4000000 - Prelit", "Object uses prelit behavior.");
            }

            return defs;
        }

        default:
            return {};
    }
}

QString levelQueryFlagDisplayName(const QString& name)
{
    QString displayName = name.trimmed();
    if (displayName.startsWith(QStringLiteral("0x"), Qt::CaseInsensitive))
    {
        const int separator = displayName.indexOf(QStringLiteral(" - "));
        if (separator >= 0)
        {
            displayName = displayName.mid(separator + 3);
        }
    }
    return displayName;
}

bool levelQueryFlagMatchesFilter(
    const std::uint32_t bit,
    const QString& name,
    const QString& description,
    const QString& filter)
{
    const QStringList terms = filter.simplified().split(QChar(' '), Qt::SkipEmptyParts);
    if (terms.empty())
    {
        return true;
    }

    int bitIndex = 0;
    if (bit != 0u)
    {
        std::uint32_t shifted = bit;
        while ((shifted & 1u) == 0u)
        {
            shifted >>= 1u;
            ++bitIndex;
        }
    }
    const QString haystack = levelQueryHex(bit) + QChar(' ')
        + QString::number(bit) + QChar(' ')
        + QString::number(bitIndex) + QChar(' ')
        + levelQueryFlagDisplayName(name) + QChar(' ')
        + description;
    for (const QString& term : terms)
    {
        if (!haystack.contains(term, Qt::CaseInsensitive))
        {
            return false;
        }
    }
    return true;
}

std::optional<QString> openLevelQueryFlagEditor(
    QWidget* parent,
    const QString& title,
    const QString& currentValue,
    std::vector<LevelQueryFlagDef> defs,
    QWidget* anchor)
{
    if (!parent)
    {
        return std::nullopt;
    }

    std::uint32_t parsedValue = 0u;
    const std::string trimmed = currentValue.trimmed().toStdString();
    if (!trimmed.empty())
    {
        if (const std::optional<std::uint32_t> asHex = ::sed::text::parseHex(trimmed))
        {
            parsedValue = *asHex;
        }
        else if (const std::optional<std::int64_t> asInt = ::sed::text::parseInt<std::int64_t>(trimmed))
        {
            parsedValue = static_cast<std::uint32_t>(*asInt);
        }
    }

    QDialog dialog(parent);
    dialog.setWindowTitle(title);
    dialog.setModal(true);
    dialog.resize(860, 520);
    dialog.setMinimumSize(640, 380);
    ::sed::ui::applyDialogWindowPolicy(dialog);
    ::sed::ui::positionDialogNearWidget(dialog, anchor);

    auto* layout = new QVBoxLayout(&dialog);
    layout->setContentsMargins(10, 10, 10, 10);
    layout->setSpacing(8);

    auto* info = new QLabel(QStringLiteral("Toggle known flags or edit the hex value directly."), &dialog);
    info->setWordWrap(true);
    layout->addWidget(info);

    auto* filterEdit = new QLineEdit(&dialog);
    filterEdit->setObjectName(QStringLiteral("flag_filter_edit"));
    filterEdit->setPlaceholderText(QStringLiteral("Search bit, flag, or description..."));
    filterEdit->setClearButtonEnabled(true);
    filterEdit->setMaximumHeight(28);
    layout->addWidget(filterEdit);

    auto* list = new QTreeWidget(&dialog);
    list->setColumnCount(3);
    list->setHeaderLabels({ QStringLiteral("Bit"), QStringLiteral("Flag"), QStringLiteral("Description") });
    list->setRootIsDecorated(false);
    list->setUniformRowHeights(true);
    list->setAlternatingRowColors(true);
    list->header()->setStretchLastSection(true);
    list->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    list->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    list->header()->setSectionResizeMode(2, QHeaderView::Stretch);
    layout->addWidget(list, 1);

    auto* hexRow = new QWidget(&dialog);
    auto* hexLayout = new QHBoxLayout(hexRow);
    hexLayout->setContentsMargins(0, 0, 0, 0);
    hexLayout->setSpacing(6);
    hexLayout->addWidget(new QLabel(QStringLiteral("Hex:"), hexRow));
    auto* hexEdit = new QLineEdit(hexRow);
    hexEdit->setText(levelQueryHex(parsedValue));
    hexLayout->addWidget(hexEdit, 1);
    layout->addWidget(hexRow);

    auto* unknownBitsLabel = new QLabel(&dialog);
    unknownBitsLabel->setStyleSheet(QStringLiteral("color: rgba(255,255,255,160);"));
    layout->addWidget(unknownBitsLabel);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    layout->addWidget(buttons);

    std::sort(defs.begin(), defs.end(), [](const LevelQueryFlagDef& lhs, const LevelQueryFlagDef& rhs)
        {
            if (lhs.bit != rhs.bit)
            {
                return lhs.bit < rhs.bit;
            }
            return lhs.name < rhs.name;
        });

    std::uint32_t knownMask = 0u;
    for (const auto& def : defs)
    {
        knownMask |= def.bit;
        auto* item = new QTreeWidgetItem(list);
        item->setText(0, levelQueryHex(def.bit));
        item->setText(1, levelQueryFlagDisplayName(def.name));
        item->setText(2, def.description);
        item->setToolTip(1, def.description);
        item->setToolTip(2, def.description);
        Qt::ItemFlags itemFlags = item->flags() | Qt::ItemIsUserCheckable;
        if (!def.editable)
        {
            itemFlags &= ~Qt::ItemIsEnabled;
        }
        item->setFlags(itemFlags);
        item->setCheckState(
            0,
            def.bit != 0u && (parsedValue & def.bit) == def.bit
                ? Qt::Checked
                : Qt::Unchecked);
        item->setData(0, Qt::UserRole, QVariant::fromValue<qulonglong>(def.bit));
    }

    QObject::connect(filterEdit, &QLineEdit::textChanged, &dialog, [list](const QString& text)
        {
            for (int row = 0; row < list->topLevelItemCount(); ++row)
            {
                QTreeWidgetItem* item = list->topLevelItem(row);
                if (!item)
                {
                    continue;
                }

                const std::uint32_t bit = static_cast<std::uint32_t>(
                    item->data(0, Qt::UserRole).toULongLong());
                item->setHidden(!levelQueryFlagMatchesFilter(
                    bit,
                    item->text(1),
                    item->text(2),
                    text));
            }
        });

    const auto unknownBits = std::make_shared<std::uint32_t>(parsedValue & ~knownMask);
    auto updateUnknownBitsLabel = [unknownBitsLabel, knownMask, unknownBits](const std::uint32_t value)
    {
        *unknownBits = value & ~knownMask;
        const std::uint32_t unknown = *unknownBits;
        if (unknown == 0u)
        {
            unknownBitsLabel->setText(QStringLiteral("Unknown bits: none"));
        }
        else
        {
            unknownBitsLabel->setText(QStringLiteral("Unknown bits: %1").arg(levelQueryHex(unknown)));
        }
    };
    updateUnknownBitsLabel(parsedValue);

    auto applyValueToChecks = [list](const std::uint32_t value)
    {
        const QSignalBlocker blocker(list);
        for (int row = 0; row < list->topLevelItemCount(); ++row)
        {
            QTreeWidgetItem* item = list->topLevelItem(row);
            if (!item)
            {
                continue;
            }
            const std::uint32_t bit = static_cast<std::uint32_t>(item->data(0, Qt::UserRole).toULongLong());
            item->setCheckState(
                0,
                bit != 0u && (value & bit) == bit
                    ? Qt::Checked
                    : Qt::Unchecked);
        }
    };

    auto updateFromChecks = [list, hexEdit, updateUnknownBitsLabel, unknownBits]()
    {
        // Checkbox edits replace only the bits represented by editable rows.
        // Keep bits that the current schema does not know so opening a picker
        // never destroys a mod-specific or newer-engine flag.
        std::uint32_t value = *unknownBits;
        for (int row = 0; row < list->topLevelItemCount(); ++row)
        {
            QTreeWidgetItem* item = list->topLevelItem(row);
            if (!item || item->checkState(0) != Qt::Checked)
            {
                continue;
            }
            value |= static_cast<std::uint32_t>(item->data(0, Qt::UserRole).toULongLong());
        }

        const QSignalBlocker blocker(hexEdit);
        hexEdit->setText(levelQueryHex(value));
        updateUnknownBitsLabel(value);
    };

    QObject::connect(list, &QTreeWidget::itemChanged, &dialog, [updateFromChecks](QTreeWidgetItem*, int)
        {
            updateFromChecks();
        });

    QObject::connect(hexEdit, &QLineEdit::editingFinished, &dialog, [hexEdit, applyValueToChecks, updateUnknownBitsLabel]()
        {
            const std::string text = hexEdit->text().trimmed().toStdString();
            std::optional<std::uint32_t> parsed = ::sed::text::parseHex(text);
            if (!parsed)
            {
                if (const std::optional<std::int64_t> asInt = ::sed::text::parseInt<std::int64_t>(text))
                {
                    parsed = static_cast<std::uint32_t>(*asInt);
                }
            }

            if (!parsed)
            {
                return;
            }

            applyValueToChecks(*parsed);
            hexEdit->setText(levelQueryHex(*parsed));
            updateUnknownBitsLabel(*parsed);
        });

    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    if (dialog.exec() != QDialog::Accepted)
    {
        return std::nullopt;
    }

    const std::string finalText = hexEdit->text().trimmed().toStdString();
    if (const std::optional<std::uint32_t> asHex = ::sed::text::parseHex(finalText))
    {
        return levelQueryHex(*asHex);
    }
    if (const std::optional<std::int64_t> asInt = ::sed::text::parseInt<std::int64_t>(finalText))
    {
        return levelQueryHex(static_cast<std::uint32_t>(*asInt));
    }

    return levelQueryHex(parsedValue);
}

std::optional<ui::ResourcePickerKind> levelQueryPickerKind(const LevelQueryField field) noexcept
{
    switch (field)
    {
        case LevelQueryField::sectorColormap: return ui::ResourcePickerKind::cmp;
        case LevelQueryField::sectorSound:    return ui::ResourcePickerKind::wav;
        case LevelQueryField::surfaceMaterial:return ui::ResourcePickerKind::material;
        case LevelQueryField::thingTemplate:  return ui::ResourcePickerKind::template_;
        case LevelQueryField::thingModel:     return ui::ResourcePickerKind::model3do;
        case LevelQueryField::thingSprite:    return ui::ResourcePickerKind::spr;
        case LevelQueryField::thingCog:       return ui::ResourcePickerKind::cog;
        default:                              return std::nullopt;
    }
}

bool levelQueryUsesLayerPicker(const LevelQueryField field) noexcept
{
    switch (field)
    {
        case LevelQueryField::sectorLayer:
        case LevelQueryField::surfaceLayer:
        case LevelQueryField::thingLayer:
            return true;
        default:
            return false;
    }
}

bool levelQueryUsesChoicePicker(const LevelQueryField field) noexcept
{
    switch (field)
    {
        case LevelQueryField::sectorNumber:
        case LevelQueryField::sectorId:
        case LevelQueryField::thingName:
        case LevelQueryField::thingNumber:
        case LevelQueryField::thingSector:
        case LevelQueryField::thingType:
        case LevelQueryField::surfaceAdjoinSector:
        case LevelQueryField::surfaceAdjoinSurface:
        case LevelQueryField::surfaceGeo:
        case LevelQueryField::surfaceLight:
        case LevelQueryField::surfaceNumber:
        case LevelQueryField::surfaceGlobalNumber:
        case LevelQueryField::surfaceTex:
            return true;
        default:
            return false;
    }
}

QString levelQueryBrowseTitle(const LevelQueryField field)
{
    switch (field)
    {
        case LevelQueryField::sectorFlags:    return QStringLiteral("Edit Sector Flags");
        case LevelQueryField::sectorExtraLight: return QStringLiteral("Choose Extra Light");
        case LevelQueryField::sectorColormap: return QStringLiteral("Choose Colormap");
        case LevelQueryField::sectorId:       return QStringLiteral("Choose Sector ID");
        case LevelQueryField::sectorNumber:   return QStringLiteral("Choose Sector");
        case LevelQueryField::sectorTint:     return QStringLiteral("Choose Tint");
        case LevelQueryField::sectorSound:    return QStringLiteral("Choose Sound");
        case LevelQueryField::surfaceAdjoinSector: return QStringLiteral("Choose Adjoined Sector");
        case LevelQueryField::surfaceAdjoinSurface: return QStringLiteral("Choose Adjoined Surface");
        case LevelQueryField::surfaceAdjoinFlags: return QStringLiteral("Edit Adjoin Flags");
        case LevelQueryField::surfaceExtraLight: return QStringLiteral("Choose Extra Light");
        case LevelQueryField::surfaceMaterial:return QStringLiteral("Choose Material");
        case LevelQueryField::surfaceSurfaceFlags: return QStringLiteral("Edit Surface Flags");
        case LevelQueryField::surfaceFaceFlags: return QStringLiteral("Edit Face Flags");
        case LevelQueryField::thingTemplate:  return QStringLiteral("Choose Template");
        case LevelQueryField::thingName:      return QStringLiteral("Choose Thing");
        case LevelQueryField::thingNumber:    return QStringLiteral("Choose Thing");
        case LevelQueryField::thingSector:    return QStringLiteral("Choose Sector");
        case LevelQueryField::thingType:      return QStringLiteral("Choose Thing Type");
        case LevelQueryField::thingFlags:     return QStringLiteral("Edit Thing Flags");
        case LevelQueryField::thingModel:     return QStringLiteral("Choose 3DO Model");
        case LevelQueryField::thingSprite:    return QStringLiteral("Choose Sprite");
        case LevelQueryField::thingCog:       return QStringLiteral("Choose COG Script");
        case LevelQueryField::surfaceGeo:     return QStringLiteral("Choose Geometry");
        case LevelQueryField::surfaceLight:   return QStringLiteral("Choose Light");
        case LevelQueryField::surfaceNumber:  return QStringLiteral("Choose Surface");
        case LevelQueryField::surfaceGlobalNumber: return QStringLiteral("Choose Global Surface");
        case LevelQueryField::surfaceTex:     return QStringLiteral("Choose Texture");
        case LevelQueryField::sectorLayer:
        case LevelQueryField::surfaceLayer:
        case LevelQueryField::thingLayer:
            return QStringLiteral("Choose Layer");
    }

    return QStringLiteral("Choose Value");
}

QStringList levelQueryLayerChoices(const Level& level)
{
    QStringList items{};
    const std::size_t layerCount = std::max<std::size_t>(1, level.layers.size());
    for (std::size_t i = 0; i < layerCount; ++i)
    {
        QString value = QString::number(static_cast<int>(i));
        if (i < level.layers.size())
        {
            const QString layerName = QString::fromStdString(level.layers[i]).trimmed();
            if (!layerName.isEmpty())
            {
                value = layerName;
            }
        }
        if (!items.contains(value))
        {
            items.push_back(value);
        }
    }
    return items;
}

std::vector<std::pair<QString, QString>> levelQueryChoiceItems(
    const Level& level,
    const assets::Templates* templatesDb,
    const LevelQueryField field)
{
    std::vector<std::pair<QString, QString>> items{};
    auto sectorLabel = [&](const Sector& sector) -> QString
    {
        QString label = QStringLiteral("Sector %1").arg(sector.num);
        const QString layerName = QString::fromStdString(level.getLayerName(sector.layer)).trimmed();
        if (sector.id >= 0)
        {
            label += QStringLiteral(" [ID %1]").arg(sector.id);
        }
        if (!layerName.isEmpty())
        {
            label += QStringLiteral(" - %1").arg(layerName);
        }
        return label;
    };
    auto surfaceLabel = [&](const Sector& sector, const Surface& surface) -> QString
    {
        QString label = QStringLiteral("Sector %1 / Surface %2").arg(sector.num).arg(surface.num);
        const QString material = QString::fromStdString(surface.material).trimmed();
        if (!material.isEmpty())
        {
            label += QStringLiteral(" - %1").arg(material);
        }
        return label;
    };
    switch (field)
    {
        case LevelQueryField::sectorNumber:
        {
            for (const Sector& sector : level.sectors)
            {
                items.emplace_back(sectorLabel(sector), QString::number(sector.num));
            }
            break;
        }

        case LevelQueryField::sectorId:
        {
            for (const Sector& sector : level.sectors)
            {
                items.emplace_back(sectorLabel(sector), QString::number(sector.id));
            }
            break;
        }

        case LevelQueryField::thingName:
        {
            for (const Thing& thing : level.things)
            {
                const QString name = QString::fromStdString(thing.name).trimmed();
                if (name.isEmpty())
                {
                    continue;
                }

                const auto existing = std::find_if(items.begin(), items.end(), [&](const auto& item)
                    {
                        return item.second == name;
                    });
                if (existing == items.end())
                {
                    items.emplace_back(name, name);
                }
            }
            break;
        }

        case LevelQueryField::thingNumber:
        {
            for (const Thing& thing : level.things)
            {
                QString label = QStringLiteral("Thing %1").arg(thing.num);
                const QString name = QString::fromStdString(thing.name).trimmed();
                const QString templateName = QString::fromStdString(thing.templateName).trimmed();
                if (!name.isEmpty())
                {
                    label += QStringLiteral(" - %1").arg(name);
                }
                else if (!templateName.isEmpty())
                {
                    label += QStringLiteral(" - %1").arg(templateName);
                }
                items.emplace_back(label, QString::number(thing.num));
            }
            break;
        }

        case LevelQueryField::thingType:
        {
            for (const Thing& thing : level.things)
            {
                const TplValue* typeValue = ::sed::level::thingFindValue(level, templatesDb, thing, "type");
                const QString typeName = QString::fromStdString(typeValue ? typeValue->asString() : std::string{}).trimmed();
                if (typeName.isEmpty())
                {
                    continue;
                }

                const auto existing = std::find_if(items.begin(), items.end(), [&](const auto& item)
                    {
                        return item.second.compare(typeName, Qt::CaseInsensitive) == 0;
                    });
                if (existing == items.end())
                {
                    items.emplace_back(typeName, typeName);
                }
            }
            break;
        }

        case LevelQueryField::thingSector:
        case LevelQueryField::surfaceAdjoinSector:
        {
            for (const Sector& sector : level.sectors)
            {
                items.emplace_back(sectorLabel(sector), QString::number(sector.num));
            }
            break;
        }

        case LevelQueryField::surfaceAdjoinSurface:
        case LevelQueryField::surfaceNumber:
        {
            for (const Sector& sector : level.sectors)
            {
                for (const Surface& surface : sector.surfaces)
                {
                    items.emplace_back(surfaceLabel(sector, surface), QString::number(surface.num));
                }
            }
            break;
        }

        case LevelQueryField::surfaceGlobalNumber:
        {
            for (std::size_t sectorIndex = 0; sectorIndex < level.sectors.size(); ++sectorIndex)
            {
                const Sector& sector = level.sectors[sectorIndex];
                for (std::size_t surfaceIndex = 0; surfaceIndex < sector.surfaces.size(); ++surfaceIndex)
                {
                    const Surface& surface = sector.surfaces[surfaceIndex];
                    const std::int32_t globalNumber = level.getGlobalSurfaceNumber(
                        static_cast<std::int32_t>(sectorIndex),
                        static_cast<std::int32_t>(surfaceIndex));
                    items.emplace_back(
                        QStringLiteral("%1 [Global %2]").arg(surfaceLabel(sector, surface)).arg(globalNumber),
                        QString::number(globalNumber));
                }
            }
            break;
        }

        case LevelQueryField::surfaceGeo:
            items = {
                { QStringLiteral("Not rendered"), QStringLiteral("0") },
                { QStringLiteral("Draw vertices"), QStringLiteral("1") },
                { QStringLiteral("Draw wireframe"), QStringLiteral("2") },
                { QStringLiteral("Draw solid"), QStringLiteral("3") },
                { QStringLiteral("Draw textured"), QStringLiteral("4") },
            };
            break;

        case LevelQueryField::surfaceLight:
            items = {
                { QStringLiteral("Fully lit"), QStringLiteral("0") },
                { QStringLiteral("Not lit"), QStringLiteral("1") },
                { QStringLiteral("Diffuse"), QStringLiteral("2") },
                { QStringLiteral("Gouraud"), QStringLiteral("3") },
            };
            break;

        case LevelQueryField::surfaceTex:
            items = {
                { QStringLiteral("Affine"), QStringLiteral("0") },
                { QStringLiteral("Perspective"), QStringLiteral("1") },
                { QStringLiteral("Perspective (2)"), QStringLiteral("2") },
                { QStringLiteral("Perspective (3)"), QStringLiteral("3") },
            };
            break;

        default:
            break;
    }

    return items;
}

bool levelQueryUsesTextComparison(const LevelQueryMatchMode mode) noexcept
{
    switch (mode)
    {
        case LevelQueryMatchMode::contains:
        case LevelQueryMatchMode::startsWith:
        case LevelQueryMatchMode::endsWith:
        case LevelQueryMatchMode::notContains:
            return true;
        case LevelQueryMatchMode::exact:
        case LevelQueryMatchMode::notEqual:
        case LevelQueryMatchMode::above:
        case LevelQueryMatchMode::below:
        case LevelQueryMatchMode::bitSet:
        case LevelQueryMatchMode::bitNotSet:
            return false;
    }

    return false;
}

bool levelQueryTextMatches(const QString& haystack, const LevelQuerySpec& spec)
{
    const QString needle = QString::fromStdString(spec.value).trimmed();
    if (needle.isEmpty())
    {
        return false;
    }

    const Qt::CaseSensitivity caseSensitivity = spec.caseSensitive ? Qt::CaseSensitive : Qt::CaseInsensitive;
    switch (spec.matchMode)
    {
        case LevelQueryMatchMode::contains:
            return haystack.contains(needle, caseSensitivity);
        case LevelQueryMatchMode::exact:
            return haystack.compare(needle, caseSensitivity) == 0;
        case LevelQueryMatchMode::startsWith:
            return haystack.startsWith(needle, caseSensitivity);
        case LevelQueryMatchMode::endsWith:
            return haystack.endsWith(needle, caseSensitivity);
        case LevelQueryMatchMode::notEqual:
            return haystack.compare(needle, caseSensitivity) != 0;
        case LevelQueryMatchMode::notContains:
            return !haystack.contains(needle, caseSensitivity);
        case LevelQueryMatchMode::above:
            return haystack.compare(needle, caseSensitivity) > 0;
        case LevelQueryMatchMode::below:
            return haystack.compare(needle, caseSensitivity) < 0;
        case LevelQueryMatchMode::bitSet:
        case LevelQueryMatchMode::bitNotSet:
            return false;
    }

    return false;
}

bool levelQueryTextMatchesAny(const std::vector<QString>& haystacks, const LevelQuerySpec& spec)
{
    if (haystacks.empty())
    {
        return false;
    }

    if (spec.matchMode == LevelQueryMatchMode::notEqual
        || spec.matchMode == LevelQueryMatchMode::notContains)
    {
        for (const QString& haystack : haystacks)
        {
            if (!levelQueryTextMatches(haystack, spec))
            {
                return false;
            }
        }
        return true;
    }

    for (const QString& haystack : haystacks)
    {
        if (levelQueryTextMatches(haystack, spec))
        {
            return true;
        }
    }
    return false;
}

std::vector<QString> levelQueryNumericStrings(const std::int64_t value)
{
    std::vector<QString> values{};
    values.push_back(QString::number(value));
    if (value >= 0)
    {
        values.push_back(QStringLiteral("0x%1").arg(static_cast<qulonglong>(value), 0, 16));
    }
    return values;
}

std::vector<QString> levelQueryUnsignedStrings(const std::uint64_t value)
{
    std::vector<QString> values{};
    values.push_back(QString::number(static_cast<qulonglong>(value)));
    values.push_back(QStringLiteral("0x%1").arg(static_cast<qulonglong>(value), 0, 16));
    return values;
}

std::vector<QString> levelQueryDoubleStrings(const double value)
{
    std::vector<QString> values{};
    values.push_back(QString::number(value, 'g', 16));
    values.push_back(QString::number(value, 'f', 6));
    return values;
}

std::vector<QString> levelQueryColorStrings(const ColorF& color)
{
    std::vector<QString> values{};
    values.push_back(
        QStringLiteral("%1 %2 %3")
            .arg(QString::number(color.r, 'f', 3))
            .arg(QString::number(color.g, 'f', 3))
            .arg(QString::number(color.b, 'f', 3)));
    values.push_back(
        QStringLiteral("%1 %2 %3 %4")
            .arg(QString::number(color.r, 'f', 3))
            .arg(QString::number(color.g, 'f', 3))
            .arg(QString::number(color.b, 'f', 3))
            .arg(QString::number(color.a, 'f', 3)));
    return values;
}

std::optional<std::int64_t> parseLevelQuerySigned(const LevelQuerySpec& spec)
{
    const QString text = QString::fromStdString(spec.value).trimmed();
    if (text.isEmpty())
    {
        return std::nullopt;
    }

    bool ok = false;
    const qlonglong value = text.toLongLong(&ok, 0);
    if (ok)
    {
        return static_cast<std::int64_t>(value);
    }

    const qulonglong unsignedValue = text.toULongLong(&ok, 0);
    if (ok && unsignedValue <= static_cast<qulonglong>(std::numeric_limits<std::int64_t>::max()))
    {
        return static_cast<std::int64_t>(unsignedValue);
    }

    return std::nullopt;
}

std::optional<std::uint64_t> parseLevelQueryUnsigned(const LevelQuerySpec& spec)
{
    const QString text = QString::fromStdString(spec.value).trimmed();
    if (text.isEmpty())
    {
        return std::nullopt;
    }

    bool ok = false;
    const qulonglong value = text.toULongLong(&ok, 0);
    if (ok)
    {
        return static_cast<std::uint64_t>(value);
    }

    const qlonglong signedValue = text.toLongLong(&ok, 0);
    if (ok && signedValue >= 0)
    {
        return static_cast<std::uint64_t>(signedValue);
    }

    return std::nullopt;
}

std::optional<double> parseLevelQueryDouble(const LevelQuerySpec& spec)
{
    const QString text = QString::fromStdString(spec.value).trimmed();
    if (text.isEmpty())
    {
        return std::nullopt;
    }

    bool ok = false;
    const double value = text.toDouble(&ok);
    return ok ? std::optional<double>(value) : std::nullopt;
}

std::optional<LevelQueryParsedColor> parseLevelQueryColor(const LevelQuerySpec& spec)
{
    QString text = QString::fromStdString(spec.value).trimmed();
    if (text.isEmpty())
    {
        return std::nullopt;
    }

    text.replace('(', ' ');
    text.replace(')', ' ');
    text.replace('/', ' ');
    text.replace(',', ' ');
    text = text.simplified();
    const QStringList parts = text.split(' ', Qt::SkipEmptyParts);
    if (parts.size() < 3 || parts.size() > 4)
    {
        return std::nullopt;
    }

    bool okR = false;
    bool okG = false;
    bool okB = false;
    bool okA = true;
    const float r = parts[0].toFloat(&okR);
    const float g = parts[1].toFloat(&okG);
    const float b = parts[2].toFloat(&okB);
    float a = 0.0f;
    if (parts.size() >= 4)
    {
        a = parts[3].toFloat(&okA);
    }

    if (!okR || !okG || !okB || !okA)
    {
        return std::nullopt;
    }

    return LevelQueryParsedColor{ ColorF{ r, g, b, a }, parts.size() >= 4 };
}

bool levelQueryMatchSignedValue(const std::int64_t value, const LevelQuerySpec& spec)
{
    if (levelQueryUsesTextComparison(spec.matchMode))
    {
        return levelQueryTextMatchesAny(levelQueryNumericStrings(value), spec);
    }

    const std::optional<std::int64_t> parsed = parseLevelQuerySigned(spec);
    if (!parsed)
    {
        return false;
    }

    switch (spec.matchMode)
    {
        case LevelQueryMatchMode::exact: return value == *parsed;
        case LevelQueryMatchMode::notEqual: return value != *parsed;
        case LevelQueryMatchMode::above: return value > *parsed;
        case LevelQueryMatchMode::below: return value < *parsed;
        case LevelQueryMatchMode::bitSet: return (value & *parsed) != 0;
        case LevelQueryMatchMode::bitNotSet: return (value & *parsed) == 0;
        case LevelQueryMatchMode::contains:
        case LevelQueryMatchMode::startsWith:
        case LevelQueryMatchMode::endsWith:
        case LevelQueryMatchMode::notContains:
            return false;
    }

    return false;
}

bool levelQueryMatchUnsignedValue(const std::uint64_t value, const LevelQuerySpec& spec)
{
    if (levelQueryUsesTextComparison(spec.matchMode))
    {
        return levelQueryTextMatchesAny(levelQueryUnsignedStrings(value), spec);
    }

    const std::optional<std::uint64_t> parsed = parseLevelQueryUnsigned(spec);
    if (!parsed)
    {
        return false;
    }

    switch (spec.matchMode)
    {
        case LevelQueryMatchMode::exact: return value == *parsed;
        case LevelQueryMatchMode::notEqual: return value != *parsed;
        case LevelQueryMatchMode::above: return value > *parsed;
        case LevelQueryMatchMode::below: return value < *parsed;
        case LevelQueryMatchMode::bitSet: return (value & *parsed) != 0;
        case LevelQueryMatchMode::bitNotSet: return (value & *parsed) == 0;
        case LevelQueryMatchMode::contains:
        case LevelQueryMatchMode::startsWith:
        case LevelQueryMatchMode::endsWith:
        case LevelQueryMatchMode::notContains:
            return false;
    }

    return false;
}

bool levelQueryMatchDoubleValue(const double value, const LevelQuerySpec& spec)
{
    if (levelQueryUsesTextComparison(spec.matchMode))
    {
        return levelQueryTextMatchesAny(levelQueryDoubleStrings(value), spec);
    }

    const std::optional<double> parsed = parseLevelQueryDouble(spec);
    if (!parsed)
    {
        return false;
    }

    constexpr double kEpsilon = 1.0e-6;
    switch (spec.matchMode)
    {
        case LevelQueryMatchMode::exact: return std::abs(value - *parsed) <= kEpsilon;
        case LevelQueryMatchMode::notEqual: return std::abs(value - *parsed) > kEpsilon;
        case LevelQueryMatchMode::above: return value > *parsed;
        case LevelQueryMatchMode::below: return value < *parsed;
        case LevelQueryMatchMode::contains:
        case LevelQueryMatchMode::startsWith:
        case LevelQueryMatchMode::endsWith:
        case LevelQueryMatchMode::notContains:
        case LevelQueryMatchMode::bitSet:
        case LevelQueryMatchMode::bitNotSet:
            return false;
    }

    return false;
}

bool levelQueryMatchColorValue(const ColorF& value, const LevelQuerySpec& spec)
{
    if (levelQueryUsesTextComparison(spec.matchMode))
    {
        return levelQueryTextMatchesAny(levelQueryColorStrings(value), spec);
    }

    const std::optional<LevelQueryParsedColor> parsed = parseLevelQueryColor(spec);
    if (!parsed)
    {
        return false;
    }

    constexpr double kEpsilon = 1.0e-6;
    auto exact = [&](const double a, const double b)
    {
        return std::abs(a - b) <= kEpsilon;
    };
    auto notExact = [&](const double a, const double b)
    {
        return std::abs(a - b) > kEpsilon;
    };

    auto testChannel = [&](const double actual, const double expected) -> bool
    {
        switch (spec.matchMode)
        {
            case LevelQueryMatchMode::exact: return exact(actual, expected);
            case LevelQueryMatchMode::notEqual: return notExact(actual, expected);
            case LevelQueryMatchMode::above: return actual > expected;
            case LevelQueryMatchMode::below: return actual < expected;
            case LevelQueryMatchMode::contains:
            case LevelQueryMatchMode::startsWith:
            case LevelQueryMatchMode::endsWith:
            case LevelQueryMatchMode::notContains:
            case LevelQueryMatchMode::bitSet:
            case LevelQueryMatchMode::bitNotSet:
                return false;
        }

        return false;
    };

    const bool rgbOk =
        testChannel(value.r, parsed->value.r)
        && testChannel(value.g, parsed->value.g)
        && testChannel(value.b, parsed->value.b);
    if (!rgbOk)
    {
        return false;
    }

    return !parsed->hasAlpha || testChannel(value.a, parsed->value.a);
}

LevelQuerySpec levelQuerySpecForCriterion(const LevelQuerySpec& base, const LevelQuerySpec::Criterion& criterion)
{
    LevelQuerySpec spec = base;
    spec.field = criterion.field;
    spec.matchMode = criterion.matchMode;
    spec.value = criterion.value;
    spec.criteria.clear();
    return spec;
}

bool levelQuerySectorMatches(
    const Level& level,
    const Sector& sector,
    const LevelQuerySpec& spec)
{
    switch (spec.field)
    {
        case LevelQueryField::sectorId:
            return levelQueryMatchSignedValue(sector.id, spec);
        case LevelQueryField::sectorNumber:
            return levelQueryMatchSignedValue(sector.num, spec);
        case LevelQueryField::sectorSurfaceCount:
            return levelQueryMatchSignedValue(static_cast<std::int64_t>(sector.surfaces.size()), spec);
        case LevelQueryField::sectorFlags:
            return levelQueryMatchUnsignedValue(static_cast<std::uint32_t>(sector.flags), spec);
        case LevelQueryField::sectorExtraLight:
            return levelQueryMatchColorValue(sector.extraLight, spec);
        case LevelQueryField::sectorTint:
            return levelQueryMatchColorValue(sector.tint, spec);
        case LevelQueryField::sectorSoundVolume:
            return levelQueryMatchDoubleValue(sector.soundVolume, spec);
        case LevelQueryField::sectorLayer:
        {
            std::vector<QString> values = levelQueryNumericStrings(sector.layer);
            values.push_back(QString::fromStdString(level.getLayerName(sector.layer)));
            return levelQueryTextMatchesAny(values, spec);
        }
        case LevelQueryField::sectorColormap:
            return levelQueryTextMatches(QString::fromStdString(sector.colormap), spec);
        case LevelQueryField::sectorSound:
            return levelQueryTextMatches(QString::fromStdString(sector.sound), spec);
        default:
            return false;
    }
}

bool levelQuerySurfaceMatches(
    const Level& level,
    const Sector& sector,
    const Surface& surface,
    const std::size_t sectorIndex,
    const std::size_t surfaceIndex,
    const LevelQuerySpec& spec)
{
    switch (spec.field)
    {
        case LevelQueryField::surfaceMaterial:
            return levelQueryTextMatches(QString::fromStdString(surface.material), spec);
        case LevelQueryField::surfaceAdjoinFlags:
            return levelQueryMatchUnsignedValue(static_cast<std::uint32_t>(surface.adjoinFlags), spec);
        case LevelQueryField::surfaceExtraLight:
            return levelQueryMatchColorValue(surface.extraLight, spec);
        case LevelQueryField::surfaceSurfaceFlags:
            return levelQueryMatchUnsignedValue(static_cast<std::uint32_t>(surface.surfaceFlags), spec);
        case LevelQueryField::surfaceFaceFlags:
            return levelQueryMatchUnsignedValue(surface.faceFlags, spec);
        case LevelQueryField::surfaceAdjoinSector:
        {
            std::int32_t value = -1;
            if (surface.adjoin
                && surface.adjoin->sector >= 0
                && static_cast<std::size_t>(surface.adjoin->sector) < level.sectors.size())
            {
                value = level.sectors[static_cast<std::size_t>(surface.adjoin->sector)].num;
            }
            return levelQueryMatchSignedValue(value, spec);
        }
        case LevelQueryField::surfaceAdjoinSurface:
        {
            std::int32_t value = -1;
            if (surface.adjoin
                && surface.adjoin->sector >= 0
                && static_cast<std::size_t>(surface.adjoin->sector) < level.sectors.size())
            {
                const Sector& adjoinSector = level.sectors[static_cast<std::size_t>(surface.adjoin->sector)];
                if (surface.adjoin->surface >= 0
                    && static_cast<std::size_t>(surface.adjoin->surface) < adjoinSector.surfaces.size())
                {
                    value = adjoinSector.surfaces[static_cast<std::size_t>(surface.adjoin->surface)].num;
                }
            }
            return levelQueryMatchSignedValue(value, spec);
        }
        case LevelQueryField::surfaceGeo:
            return levelQueryMatchSignedValue(surface.geo, spec);
        case LevelQueryField::surfaceLight:
            return levelQueryMatchSignedValue(surface.light, spec);
        case LevelQueryField::surfaceTex:
            return levelQueryMatchSignedValue(surface.tex, spec);
        case LevelQueryField::surfaceLayer:
        {
            std::vector<QString> values = levelQueryNumericStrings(sector.layer);
            values.push_back(QString::fromStdString(level.getLayerName(sector.layer)));
            return levelQueryTextMatchesAny(values, spec);
        }
        case LevelQueryField::surfaceNumber:
            return levelQueryMatchSignedValue(surface.num, spec);
        case LevelQueryField::surfaceGlobalNumber:
        {
            const std::int32_t globalNumber = level.getGlobalSurfaceNumber(
                static_cast<std::int32_t>(sectorIndex),
                static_cast<std::int32_t>(surfaceIndex));
            return levelQueryMatchSignedValue(globalNumber, spec);
        }
        default:
            return false;
    }
}

bool levelQueryThingMatches(
    const Level& level,
    const assets::Templates* templatesDb,
    const Thing& thing,
    const LevelQuerySpec& spec)
{
    switch (spec.field)
    {
        case LevelQueryField::thingName:
            return levelQueryTextMatches(QString::fromStdString(thing.name), spec);
        case LevelQueryField::thingNumber:
            return levelQueryMatchSignedValue(thing.num, spec);
        case LevelQueryField::thingPch:
            return levelQueryMatchDoubleValue(thing.pch, spec);
        case LevelQueryField::thingYaw:
            return levelQueryMatchDoubleValue(thing.yaw, spec);
        case LevelQueryField::thingRol:
            return levelQueryMatchDoubleValue(thing.rol, spec);
        case LevelQueryField::thingX:
            return levelQueryMatchDoubleValue(thing.x, spec);
        case LevelQueryField::thingY:
            return levelQueryMatchDoubleValue(thing.y, spec);
        case LevelQueryField::thingZ:
            return levelQueryMatchDoubleValue(thing.z, spec);
        case LevelQueryField::thingTemplate:
        {
            const QString templateName = QString::fromStdString(!thing.templateName.empty() ? thing.templateName : thing.name);
            return levelQueryTextMatches(templateName, spec);
        }
        case LevelQueryField::thingType:
        {
            const TplValue* typeValue = ::sed::level::thingFindValue(level, templatesDb, thing, "type");
            return levelQueryTextMatches(typeValue ? QString::fromStdString(typeValue->asString()) : QString{}, spec);
        }
        case LevelQueryField::thingSector:
        {
            std::int32_t value = -1;
            if (thing.sector >= 0 && static_cast<std::size_t>(thing.sector) < level.sectors.size())
            {
                value = level.sectors[static_cast<std::size_t>(thing.sector)].num;
            }
            return levelQueryMatchSignedValue(value, spec);
        }
        case LevelQueryField::thingFlags:
            return levelQueryMatchUnsignedValue(
                static_cast<std::uint32_t>(::sed::level::thingFlags(level, templatesDb, thing)),
                spec);
        case LevelQueryField::thingModel:
        {
            const TplValue* modelValue = ::sed::level::thingFindValue(level, templatesDb, thing, "model3d");
            if (!modelValue)
            {
                modelValue = ::sed::level::thingFindValue(level, templatesDb, thing, "model3do");
            }
            return levelQueryTextMatches(modelValue ? QString::fromStdString(modelValue->asString()) : QString{}, spec);
        }
        case LevelQueryField::thingSprite:
        {
            const TplValue* spriteValue = ::sed::level::thingFindValue(level, templatesDb, thing, "sprite");
            return levelQueryTextMatches(spriteValue ? QString::fromStdString(spriteValue->asString()) : QString{}, spec);
        }
        case LevelQueryField::thingCog:
        {
            const TplValue* cogValue = ::sed::level::thingFindValue(level, templatesDb, thing, "cog");
            return levelQueryTextMatches(cogValue ? QString::fromStdString(cogValue->asString()) : QString{}, spec);
        }
        case LevelQueryField::thingLayer:
        {
            std::vector<QString> values = levelQueryNumericStrings(thing.layer);
            values.push_back(QString::fromStdString(level.getLayerName(thing.layer)));
            return levelQueryTextMatchesAny(values, spec);
        }
        default:
            return false;
    }
}

std::vector<LevelQueryHit> collectLevelQueryHits(const Level& level, const assets::Templates* templatesDb, const LevelQuerySpec& spec)
{
    std::vector<LevelQueryHit> hits{};
    const std::vector<LevelQuerySpec::Criterion> criteria = levelQueryEffectiveCriteria(spec);
    if (criteria.empty())
    {
        return hits;
    }

    switch (spec.kind)
    {
        case LevelQueryEntityKind::sector:
        {
            hits.reserve(level.sectors.size());
            for (std::size_t i = 0; i < level.sectors.size(); ++i)
            {
                const Sector& sector = level.sectors[i];
                bool matches = true;
                for (const LevelQuerySpec::Criterion& criterion : criteria)
                {
                    if (!levelQuerySectorMatches(level, sector, levelQuerySpecForCriterion(spec, criterion)))
                    {
                        matches = false;
                        break;
                    }
                }

                if (!matches)
                {
                    continue;
                }

                hits.push_back(LevelQueryHit{
                    OutlinerKind::sector,
                    static_cast<std::int32_t>(i),
                    -1,
                    -1,
                });
            }
            break;
        }

        case LevelQueryEntityKind::surface:
        {
            for (std::size_t sectorIndex = 0; sectorIndex < level.sectors.size(); ++sectorIndex)
            {
                const Sector& sector = level.sectors[sectorIndex];
                for (std::size_t surfaceIndex = 0; surfaceIndex < sector.surfaces.size(); ++surfaceIndex)
                {
                    const Surface& surface = sector.surfaces[surfaceIndex];
                    bool matches = true;
                    for (const LevelQuerySpec::Criterion& criterion : criteria)
                    {
                    if (!levelQuerySurfaceMatches(
                            level,
                            sector,
                            surface,
                            sectorIndex,
                            surfaceIndex,
                            levelQuerySpecForCriterion(spec, criterion)))
                        {
                            matches = false;
                            break;
                        }
                    }

                    if (!matches)
                    {
                        continue;
                    }

                    hits.push_back(LevelQueryHit{
                        OutlinerKind::surface,
                        static_cast<std::int32_t>(surfaceIndex),
                        static_cast<std::int32_t>(sectorIndex),
                        static_cast<std::int32_t>(surfaceIndex),
                    });
                }
            }
            break;
        }

        case LevelQueryEntityKind::thing:
        {
            hits.reserve(level.things.size());
            for (std::size_t i = 0; i < level.things.size(); ++i)
            {
                const Thing& thing = level.things[i];
                bool matches = true;
                for (const LevelQuerySpec::Criterion& criterion : criteria)
                {
                    if (!levelQueryThingMatches(level, templatesDb, thing, levelQuerySpecForCriterion(spec, criterion)))
                    {
                        matches = false;
                        break;
                    }
                }

                if (!matches)
                {
                    continue;
                }

                hits.push_back(LevelQueryHit{
                    OutlinerKind::thing,
                    static_cast<std::int32_t>(i),
                    -1,
                    -1,
                });
            }
            break;
        }
    }

    return hits;
}

bool levelQuerySelectionContains(const editor::Selection& selection, const LevelQueryHit& hit) noexcept
{
    switch (hit.kind)
    {
        case OutlinerKind::sector:
            return selection.sectors.contains(hit.index);
        case OutlinerKind::surface:
            return selection.surfaces.contains(SurfaceRef{ hit.sector, hit.surface });
        case OutlinerKind::thing:
            return selection.things.contains(hit.index);
        default:
            return false;
    }
}

std::vector<LevelQueryHit> filterLevelQueryHitsToSelection(
    const std::vector<LevelQueryHit>& hits,
    const editor::Selection& selection)
{
    std::vector<LevelQueryHit> filtered{};
    filtered.reserve(hits.size());
    for (const LevelQueryHit& hit : hits)
    {
        if (levelQuerySelectionContains(selection, hit))
        {
            filtered.push_back(hit);
        }
    }
    return filtered;
}


} // namespace sed::ui::query
