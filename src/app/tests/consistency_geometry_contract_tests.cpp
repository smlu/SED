#include "test_common.hpp"
#include "ui/checks/level_check_utils.hpp"

#include <QSettings>
#include <QTemporaryDir>

#include <algorithm>
#include <array>
#include <initializer_list>
#include <limits>

namespace sed::app_tests
{
namespace
{
using namespace ui::checks;

Sector diagnosticCube()
{
    Sector sector;
    sector.colormap = "dflt.cmp";
    sector.vertices = {{0,0,0}, {1,0,0}, {1,1,0}, {0,1,0},
        {0,0,1}, {1,0,1}, {1,1,1}, {0,1,1}};
    const std::array<std::vector<std::int32_t>, 6> loops{{
        {0,1,2,3}, {4,7,6,5}, {0,3,7,4}, {1,5,6,2}, {0,4,5,1}, {3,2,6,7}}};
    const std::array<TVector, 6> normals{{{0,0,1}, {0,0,-1}, {1,0,0}, {-1,0,0}, {0,1,0}, {0,-1,0}}};
    for ( std::size_t index = 0; index < loops.size(); ++index )
    {
        Surface surface;
        surface.vertices = loops[index];
        surface.normal = normals[index];
        surface.geo = 4;
        surface.surfaceFlags = SF_Collision;
        surface.texVertices.resize(4);
        sector.surfaces.push_back(std::move(surface));
    }
    return sector;
}

CheckRuleSelection onlyRules(const std::initializer_list<CheckRule> rules,
    const GeometryCheckPrecision precision = GeometryCheckPrecision::GameCompatible)
{
    CheckRuleSelection selection;
    selection.enabled.fill(false);
    selection.geometryPrecision = precision;
    for ( const auto rule : rules )
    {
        selection.setEnabled(rule, true);
    }
    return selection;
}

void checkIssues(const std::vector<CheckIssue>& actual, const std::initializer_list<CheckIssue> expected,
    const char* label)
{
    bool equal = actual.size() == expected.size();
    auto next = expected.begin();
    for ( std::size_t index = 0; equal && index < actual.size(); ++index, ++next )
    {
        const auto& issue = actual[index];
        equal = issue.severity == next->severity && issue.message == next->message
            && issue.navKind == next->navKind && issue.navIndex == next->navIndex
            && issue.navSector == next->navSector && issue.navSurface == next->navSurface;
    }
    if ( !equal )
    {
        std::cerr << label << " actualCount=" << actual.size() << " expectedCount=" << expected.size() << '\n';
        for ( const auto& issue : actual )
        {
            std::cerr << "  " << static_cast<int>(issue.severity) << " " << issue.message.toStdString() << '\n';
        }
    }
    check(equal, label);
}
} // namespace

void runConsistencyGeometryContractTests()
{
    using namespace ui::checks;
    for ( const ProjectType project : {ProjectType::JKDF2, ProjectType::MOTS, ProjectType::IJIM} )
    {
        Level level;
        level.kind = project;
        level.layers = {"Layer0"};
        level.sectors.push_back(diagnosticCube());
        checkIssues(collectConsistencyCheckIssues(level), {},
            "Consistency complete checker accepts an independently defined inward cube in every game");
        check(defaultCheckRuleSelection(project).geometryPrecision == GeometryCheckPrecision::GameCompatible,
            "Consistency defaults to game-compatible geometry precision for every project");

        auto preciseDefaults = defaultCheckRuleSelection(project);
        preciseDefaults.geometryPrecision = GeometryCheckPrecision::Precise;
        checkIssues(collectConsistencyCheckIssues(level, preciseDefaults), {},
            "Consistency precise mode accepts the complete valid inward cube");

        level.sectors[0].vertices[6].z = 1.00005;
        checkIssues(collectConsistencyCheckIssues(level), {},
            "Consistency default checker accepts original-format sized plane quantization");
        const auto precisePlanarity = onlyRules({CheckRule::surfacePlanarity}, GeometryCheckPrecision::Precise);
        checkIssues(collectConsistencyCheckIssues(level, precisePlanarity), {
            {CheckSeverity::info, QStringLiteral("Sector 0 / Surface 1: face is not planar"),
                OutlinerKind::surface, 1, 0, std::nullopt}},
            "Consistency precise mode retains the exact fine-planarity issue vector");

        checkIssues(collectConsistencyCheckIssues(level,
            onlyRules({CheckRule::sectorConvexity}, GeometryCheckPrecision::Precise)), {
            {CheckSeverity::error, QStringLiteral("Sector 0: sector is not convex"),
                OutlinerKind::sector, 0, std::nullopt, std::nullopt}},
            "Consistency precise sector rule reports the small outside vertex with its complete issue vector");

        level.sectors[0].vertices[6].z = 1.01;
        checkIssues(collectConsistencyCheckIssues(level, onlyRules({CheckRule::sectorConvexity})), {
            {CheckSeverity::error, QStringLiteral("Sector 0: sector is not convex"),
                OutlinerKind::sector, 0, std::nullopt, std::nullopt}},
            "Consistency game-compatible sector rule still rejects a real outside vertex");
        checkIssues(collectConsistencyCheckIssues(level, onlyRules({CheckRule::surfacePlanarity})), {
            {CheckSeverity::info, QStringLiteral("Sector 0 / Surface 1: face is not planar"),
                OutlinerKind::surface, 1, 0, std::nullopt}},
            "Consistency game-compatible planarity retains genuine large face warp");
        checkIssues(collectConsistencyCheckIssues(level, onlyRules({})), {},
            "Consistency disabled geometry rules return an exactly empty issue vector");

        level.sectors[0] = diagnosticCube();
        level.sectors[0].vertices.push_back({0.5, 0.999975, 0});
        level.sectors[0].surfaces[0].vertices = {0,1,2,8,3};
        level.sectors[0].surfaces[0].texVertices.resize(5);
        checkIssues(collectConsistencyCheckIssues(level), {},
            "Consistency default accepts a historical subthreshold negative polygon turn");
        checkIssues(collectConsistencyCheckIssues(level,
            onlyRules({CheckRule::surfaceConvexity}, GeometryCheckPrecision::Precise)), {
            {CheckSeverity::info, QStringLiteral("Sector 0 / Surface 0: polygon is not convex"),
                OutlinerKind::surface, 0, 0, std::nullopt}},
            "Consistency precise polygon rule reports the subthreshold notch with its complete issue vector");
        level.sectors[0].vertices[8].y = 0.999;
        checkIssues(collectConsistencyCheckIssues(level, onlyRules({CheckRule::surfaceConvexity})), {
            {CheckSeverity::info, QStringLiteral("Sector 0 / Surface 0: polygon is not convex"),
                OutlinerKind::surface, 0, 0, std::nullopt}},
            "Consistency game-compatible polygon rule retains a true inward notch");

        level.sectors[0] = diagnosticCube();
        level.sectors[0].surfaces[0].vertices[2] = 99;
        level.sectors[0].surfaces[0].adjoin = SurfaceRef{99,4};
        level.sectors[0].surfaces[1].normal = TVector{};
        const auto malformedRules = onlyRules({CheckRule::surfaceVertexReferences,
            CheckRule::adjoinReference, CheckRule::surfaceNormal});
        checkIssues(collectConsistencyCheckIssues(level, malformedRules), {
            {CheckSeverity::error, QStringLiteral("Sector 0 / Surface 0: invalid vertex index 99 (sector has 8 vertices)"),
                OutlinerKind::surface, 0, 0, std::nullopt},
            {CheckSeverity::error, QStringLiteral("Sector 0 / Surface 0: invalid adjoin reference (99:4)"),
                OutlinerKind::surface, 0, 0, std::nullopt},
            {CheckSeverity::warning, QStringLiteral("Sector 0 / Surface 1: incorrect normal - surface may be invalid"),
                OutlinerKind::surface, 1, 0, std::nullopt}},
            "Consistency malformed geometry preserves complete ordered severity and navigation vectors");

        level.sectors = {diagnosticCube(), diagnosticCube()};
        level.sectors[0].vertices[2].z = 0.00005;
        level.sectors[1].vertices[2].z = 0.00005;
        std::reverse(level.sectors[1].surfaces[0].vertices.begin(), level.sectors[1].surfaces[0].vertices.end());
        level.sectors[1].surfaces[0].normal = {0,0,-1};
        level.sectors[0].surfaces[0].adjoin = SurfaceRef{1,0};
        level.sectors[1].surfaces[0].adjoin = SurfaceRef{0,0};
        const auto adjoinRules = onlyRules({CheckRule::adjoinOverlap, CheckRule::adjoinBackLink});
        checkIssues(collectConsistencyCheckIssues(level, adjoinRules), {},
            "Consistency existing reciprocal quantized portal boundaries match without a false planar prerequisite");
        checkIssues(collectConsistencyCheckIssues(level,
            onlyRules({CheckRule::adjoinOverlap, CheckRule::adjoinBackLink}, GeometryCheckPrecision::Precise)), {},
            "Consistency precise portal rule accepts identical quantized directed boundaries");
        for ( auto& vertex : level.sectors[1].vertices )
        {
            vertex.x += 0.000001;
        }
        checkIssues(collectConsistencyCheckIssues(level, adjoinRules), {
            {CheckSeverity::warning, QStringLiteral("Sector 0 / Surface 0: adjoined surfaces do not overlap"),
                OutlinerKind::surface, 0, 0, std::nullopt},
            {CheckSeverity::warning, QStringLiteral("Sector 1 / Surface 0: adjoined surfaces do not overlap"),
                OutlinerKind::surface, 0, 1, std::nullopt}},
            "Consistency boundary identity still rejects a genuine micron gap in both directed portal records");
    }

    QTemporaryDir directory;
    check(directory.isValid(), "Consistency precision settings use an isolated temporary store");
    if ( !directory.isValid() )
    {
        return;
    }
    QSettings settings(directory.filePath(QStringLiteral("geometry.ini")), QSettings::IniFormat);
    const std::array projects{ProjectType::JKDF2, ProjectType::MOTS, ProjectType::IJIM};
    const std::array keys{QStringLiteral("checks/geometry_precision/jkdf2"),
        QStringLiteral("checks/geometry_precision/mots"), QStringLiteral("checks/geometry_precision/ijim")};
    for ( std::size_t index = 0; index < projects.size(); ++index )
    {
        const auto project = projects[index];
        check(geometryCheckPrecisionSettingsKey(project) == keys[index],
            "Consistency geometry precision setting uses the complete fixed per-game key");
        check(loadCheckRuleSelection(settings, project).geometryPrecision == GeometryCheckPrecision::GameCompatible,
            "Consistency missing precision setting loads game-compatible default");
        auto selection = defaultCheckRuleSelection(project);
        selection.geometryPrecision = GeometryCheckPrecision::Precise;
        saveCheckRuleSelection(settings, project, CheckRuleGroup::consistency, selection);
        check(settings.value(keys[index]).toInt() == 1
                && loadCheckRuleSelection(settings, project).geometryPrecision == GeometryCheckPrecision::Precise,
            "Consistency precise setting saves fixed integer one and loads exact enum");
        selection.geometryPrecision = GeometryCheckPrecision::GameCompatible;
        saveCheckRuleSelection(settings, project, CheckRuleGroup::resources, selection);
        check(settings.value(keys[index]).toInt() == 1
                && loadCheckRuleSelection(settings, project).geometryPrecision == GeometryCheckPrecision::Precise,
            "Resource rule saving cannot clobber independent geometry precision");
        saveCheckRuleSelection(settings, project, CheckRuleGroup::consistency, selection);
        check(settings.contains(keys[index]) && settings.value(keys[index]).toInt() == 0
                && loadCheckRuleSelection(settings, project).geometryPrecision == GeometryCheckPrecision::GameCompatible,
            "Consistency compatible setting saves fixed integer zero and reloads exact enum");
        for ( const QVariant invalid : {QVariant{-1}, QVariant{2}, QVariant{QStringLiteral("precise")},
            QVariant{1.2}, QVariant{1.0}, QVariant{true}, QVariant{false},
            QVariant{QStringLiteral("1.0")}, QVariant{QStringLiteral("18446744073709551616")},
            QVariant::fromValue(std::numeric_limits<qulonglong>::max())} )
        {
            settings.setValue(keys[index], invalid);
            check(loadCheckRuleSelection(settings, project).geometryPrecision == GeometryCheckPrecision::GameCompatible,
                "Consistency unknown persisted precision values fall back to game compatibility");
        }
        settings.setValue(keys[index], QStringLiteral("1"));
        check(loadCheckRuleSelection(settings, project).geometryPrecision == GeometryCheckPrecision::Precise,
            "Consistency canonical INI integer string one retains precise mode");
        settings.setValue(keys[index], QStringLiteral("0"));
        check(loadCheckRuleSelection(settings, project).geometryPrecision == GeometryCheckPrecision::GameCompatible,
            "Consistency canonical INI integer string zero retains compatible mode");
        settings.remove(keys[index]);
    }
    const auto keysBeforeUnknown = settings.allKeys();
    auto unknownSelection = defaultCheckRuleSelection(ProjectType::IJIM);
    unknownSelection.geometryPrecision = GeometryCheckPrecision::Precise;
    saveCheckRuleSelection(settings, static_cast<ProjectType>(-1), CheckRuleGroup::consistency, unknownSelection);
    check(settings.allKeys() == keysBeforeUnknown,
        "Consistency unknown project save cannot publish an empty or invented precision key");
    check(geometryCheckPrecisionSettingsKey(static_cast<ProjectType>(-1)).isEmpty(),
        "Consistency unknown project has exactly empty geometry setting key");
}
} // namespace sed::app_tests
