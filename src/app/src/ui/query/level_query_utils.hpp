#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <QString>
#include <QStringList>

#include "sed/assets/tpl.hpp"
#include "sed/editor/selection.hpp"
#include "sed/level.hpp"
#include "ui/navigation/outliner_types.hpp"
#include "ui/resources/resource_picker.hpp"

class QWidget;

namespace sed::ui::query
{

/** Entity type searched by the level query dialog. */
enum class LevelQueryEntityKind : int
{
    sector = 0,
    surface = 1,
    thing = 2,
};

/** Searchable field exposed by the level query dialog. */
enum class LevelQueryField : int
{
    sectorId = 0,
    sectorLayer = 1,
    sectorColormap = 2,
    sectorSound = 3,
    sectorNumber = 4,
    sectorFlags = 5,
    surfaceMaterial = 6,
    surfaceSurfaceFlags = 7,
    surfaceFaceFlags = 8,
    surfaceAdjoinSector = 9,
    surfaceAdjoinSurface = 10,
    surfaceGeo = 11,
    surfaceLight = 12,
    surfaceNumber = 13,
    surfaceGlobalNumber = 14,
    thingName = 15,
    thingTemplate = 16,
    thingSector = 17,
    thingLayer = 18,
    thingType = 19,
    thingNumber = 20,
    thingFlags = 21,
    thingModel = 22,
    thingSprite = 23,
    thingCog = 24,
    sectorSurfaceCount = 25,
    sectorExtraLight = 26,
    sectorTint = 27,
    sectorSoundVolume = 28,
    surfaceAdjoinFlags = 29,
    surfaceExtraLight = 30,
    surfaceTex = 31,
    surfaceLayer = 32,
    thingPch = 33,
    thingYaw = 34,
    thingRol = 35,
    thingX = 36,
    thingY = 37,
    thingZ = 38,
};

/** Match operation used by each query criterion. */
enum class LevelQueryMatchMode : int
{
    contains = 0,
    exact = 1,
    startsWith = 2,
    endsWith = 3,
    notEqual = 4,
    above = 5,
    below = 6,
    notContains = 7,
    bitSet = 8,
    bitNotSet = 9,
};

/** Storage kind for a query field, used to choose parser and UI controls. */
enum class LevelQueryFieldKind : int
{
    string = 0,
    integer = 1,
    flags = 2,
    real = 3,
    color = 4,
};

/** How matching query results should affect the editor selection. */
enum class LevelQueryResultMode : int
{
    focus = 0,
    add = 1,
    subtract = 2,
};

/** Full query request, including the legacy single-value fields and the newer
 * multi-criterion list used by the dialog. */
struct LevelQuerySpec
{
    LevelQueryEntityKind kind{ LevelQueryEntityKind::surface };
    LevelQueryField field{ LevelQueryField::surfaceMaterial };
    LevelQueryMatchMode matchMode{ LevelQueryMatchMode::contains };
    bool caseSensitive{ false };
    std::string value{};
    struct Criterion
    {
        LevelQueryField field{ LevelQueryField::surfaceMaterial };
        LevelQueryMatchMode matchMode{ LevelQueryMatchMode::contains };
        std::string value{};
    };
    std::vector<Criterion> criteria{};
    LevelQueryResultMode resultMode{ LevelQueryResultMode::focus };
    bool selectionOnly{ false };
};

/** Concrete editor entity matched by a query. */
struct LevelQueryHit
{
    OutlinerKind kind{ OutlinerKind::none };
    std::int32_t index{ -1 };
    std::int32_t sector{ -1 };
    std::int32_t surface{ -1 };
};

/** Describes a named bit for flag picker UI and query tests. */
struct LevelQueryFlagDef
{
    std::uint32_t bit{};
    QString name{};
    QString description{};
    bool editable{ true };
};

/** Parsed color plus whether the query text explicitly supplied alpha. */
struct LevelQueryParsedColor
{
    ColorF value{};
    bool hasAlpha{ false };
};

/** Returns the non-empty criteria that should be evaluated for a query. */
std::vector<LevelQuerySpec::Criterion> levelQueryEffectiveCriteria(const LevelQuerySpec& spec);

LevelQueryEntityKind levelQueryEntityKindFromInt(int value) noexcept;
constexpr int levelQueryEntityKindIndex(const LevelQueryEntityKind kind) noexcept
{
    switch (kind)
    {
        case LevelQueryEntityKind::sector: return 0;
        case LevelQueryEntityKind::surface: return 1;
        case LevelQueryEntityKind::thing: return 2;
    }

    return 1;
}

LevelQueryField levelQueryFieldFromInt(int value) noexcept;
LevelQueryResultMode levelQueryResultModeFromInt(int value) noexcept;
LevelQueryMatchMode levelQueryMatchModeFromInt(int value) noexcept;
LevelQueryFieldKind levelQueryFieldKind(LevelQueryField field) noexcept;

const std::vector<std::pair<LevelQueryField, QString>>& levelQueryFieldChoices(LevelQueryEntityKind kind);
LevelQueryField normalizeLevelQueryField(LevelQueryEntityKind kind, LevelQueryField preferred) noexcept;
const std::vector<std::pair<LevelQueryMatchMode, QString>>& levelQueryMatchChoices(LevelQueryField field);
LevelQueryMatchMode normalizeLevelQueryMatchMode(LevelQueryField field, LevelQueryMatchMode preferred) noexcept;

QString levelQueryEntityLabel(LevelQueryEntityKind kind, int count);
QString levelQueryDialogTitle(std::optional<LevelQueryEntityKind> forcedKind);
QString levelQueryDialogIntro(std::optional<LevelQueryEntityKind> forcedKind);
QString levelQueryPlaceholder(LevelQueryField field);

QString levelQueryHex(std::uint32_t value);
bool levelQueryUsesFlagPicker(LevelQueryField field) noexcept;
bool levelQueryUsesColorPicker(LevelQueryField field) noexcept;
ColorF levelQueryDefaultColor(LevelQueryField field) noexcept;
bool levelQueryColorPickerShowsAlpha(LevelQueryField field) noexcept;
QString levelQueryColorText(const ColorF& color, bool includeAlpha);
std::vector<LevelQueryFlagDef> levelQueryFlagDefs(LevelQueryField field, ProjectType levelKind);
QString levelQueryFlagDisplayName(const QString& name);
bool levelQueryFlagMatchesFilter(
    std::uint32_t bit,
    const QString& name,
    const QString& description,
    const QString& filter);
std::optional<QString> openLevelQueryFlagEditor(
    QWidget* parent,
    const QString& title,
    const QString& currentValue,
    std::vector<LevelQueryFlagDef> defs,
    QWidget* anchor = nullptr);

std::optional<ui::ResourcePickerKind> levelQueryPickerKind(LevelQueryField field) noexcept;
bool levelQueryUsesLayerPicker(LevelQueryField field) noexcept;
bool levelQueryUsesChoicePicker(LevelQueryField field) noexcept;
QString levelQueryBrowseTitle(LevelQueryField field);
QStringList levelQueryLayerChoices(const Level& level);
std::vector<std::pair<QString, QString>> levelQueryChoiceItems(
    const Level& level,
    const assets::Templates* templatesDb,
    LevelQueryField field);

bool levelQueryUsesTextComparison(LevelQueryMatchMode mode) noexcept;
bool levelQueryTextMatches(const QString& haystack, const LevelQuerySpec& spec);
bool levelQueryTextMatchesAny(const std::vector<QString>& haystacks, const LevelQuerySpec& spec);
std::vector<QString> levelQueryNumericStrings(std::int64_t value);
std::vector<QString> levelQueryUnsignedStrings(std::uint64_t value);
std::vector<QString> levelQueryDoubleStrings(double value);
std::vector<QString> levelQueryColorStrings(const ColorF& color);

std::optional<std::int64_t> parseLevelQuerySigned(const LevelQuerySpec& spec);
std::optional<std::uint64_t> parseLevelQueryUnsigned(const LevelQuerySpec& spec);
std::optional<double> parseLevelQueryDouble(const LevelQuerySpec& spec);
std::optional<LevelQueryParsedColor> parseLevelQueryColor(const LevelQuerySpec& spec);

bool levelQueryMatchSignedValue(std::int64_t value, const LevelQuerySpec& spec);
bool levelQueryMatchUnsignedValue(std::uint64_t value, const LevelQuerySpec& spec);
bool levelQueryMatchDoubleValue(double value, const LevelQuerySpec& spec);
bool levelQueryMatchColorValue(const ColorF& value, const LevelQuerySpec& spec);

LevelQuerySpec levelQuerySpecForCriterion(const LevelQuerySpec& base, const LevelQuerySpec::Criterion& criterion);
bool levelQuerySectorMatches(const Level& level, const Sector& sector, const LevelQuerySpec& spec);
bool levelQuerySurfaceMatches(
    const Level& level,
    const Sector& sector,
    const Surface& surface,
    std::size_t sectorIndex,
    std::size_t surfaceIndex,
    const LevelQuerySpec& spec);
bool levelQueryThingMatches(
    const Level& level,
    const assets::Templates* templatesDb,
    const Thing& thing,
    const LevelQuerySpec& spec);

/** Collects all sectors, surfaces, or things that satisfy every effective
 * criterion in the query. */
std::vector<LevelQueryHit> collectLevelQueryHits(
    const Level& level,
    const assets::Templates* templatesDb,
    const LevelQuerySpec& spec);

bool levelQuerySelectionContains(const editor::Selection& selection, const LevelQueryHit& hit) noexcept;
std::vector<LevelQueryHit> filterLevelQueryHitsToSelection(
    const std::vector<LevelQueryHit>& hits,
    const editor::Selection& selection);

} // namespace sed::ui::query
