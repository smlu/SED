#include "test_common.hpp"

#include "render/overlays/template_collision_bounds_guide_utils.hpp"
#include "render/overlays/thing_shape_overlay_utils.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <vector>

namespace sed::app_tests
{
namespace
{

TplValue floatField(const std::string& name, const double value)
{
    TplValue field{};
    field.name = name;
    field.type = ValueType::floating;
    field.floatValue = value;
    return field;
}

TplValue integerField(const std::string& name, const std::int32_t value)
{
    TplValue field{};
    field.name = name;
    field.type = ValueType::integer;
    field.intValue = value;
    return field;
}

TplValue stringField(const std::string& name, const std::string& value)
{
    TplValue field{};
    field.name = name;
    field.type = ValueType::str;
    field.stringValue = value;
    return field;
}

bool sameVector(const TVector& actual, const TVector& expected)
{
    return actual.x == expected.x && actual.y == expected.y && actual.z == expected.z;
}

} // namespace

void runTemplateCollisionBoundsGuideTests()
{
    using namespace render::thing_overlay;

    Level level{};
    level.kind = ProjectType::IJIM;

    ThingTemplate base{};
    base.name = "bounds_base";
    base.parent = "none";
    base.values = {
        floatField("collwidth", 1.25),
        floatField("collheight", 2.5),
        integerField("collide", 1),
        floatField("size", 3.0),
    };

    ThingTemplate derived{};
    derived.name = "bounds_derived";
    derived.parent = "bounds_base";
    derived.values = { floatField("collheight", 4.0) };
    level.templates = { base, derived };

    Thing thing{};
    thing.templateName = "bounds_derived";

    const std::optional<AuthoredCollisionBoundsGuide> inherited =
        resolveIjimAuthoredCollisionBoundsGuide(level, nullptr, thing);
    check(inherited.has_value()
            && inherited->halfWidth == 1.25
            && inherited->halfHeight == 4.0,
        "IJIM guide resolves inherited width and local height as a complete authored box");
    if (inherited)
    {
        const std::vector<TVector> identityLines = makeAuthoredCollisionBoundsGuideWirePoints(
            *inherited, TMatrix43::identity());
        const std::array<TVector, 24> expectedIdentityLines{{
            { -1.25, -1.25, -4.0 }, { 1.25, -1.25, -4.0 },
            { 1.25, -1.25, -4.0 }, { 1.25, 1.25, -4.0 },
            { 1.25, 1.25, -4.0 }, { -1.25, 1.25, -4.0 },
            { -1.25, 1.25, -4.0 }, { -1.25, -1.25, -4.0 },
            { -1.25, -1.25, 4.0 }, { 1.25, -1.25, 4.0 },
            { 1.25, -1.25, 4.0 }, { 1.25, 1.25, 4.0 },
            { 1.25, 1.25, 4.0 }, { -1.25, 1.25, 4.0 },
            { -1.25, 1.25, 4.0 }, { -1.25, -1.25, 4.0 },
            { -1.25, -1.25, -4.0 }, { -1.25, -1.25, 4.0 },
            { 1.25, -1.25, -4.0 }, { 1.25, -1.25, 4.0 },
            { 1.25, 1.25, -4.0 }, { 1.25, 1.25, 4.0 },
            { -1.25, 1.25, -4.0 }, { -1.25, 1.25, 4.0 },
        }};
        check(identityLines.size() == expectedIdentityLines.size()
                && std::equal(identityLines.begin(), identityLines.end(), expectedIdentityLines.begin(), sameVector),
            "IJIM guide emits all twelve exact AABB edges at authored half-extents");

        const TMatrix43 rotatedTranslatedThing = TMatrix43::Create(
            TVector{ 10.0, -20.0, 30.0 }, TVector{ 0.0, 0.7, 0.0 });
        const std::vector<TVector> worldLines = makeAuthoredCollisionBoundsGuideWirePoints(
            *inherited, rotatedTranslatedThing);
        const std::array<TVector, 8> expectedFirstEdges{{
            { 8.75, -21.25, 26.0 }, { 11.25, -21.25, 26.0 },
            { 11.25, -21.25, 26.0 }, { 11.25, -18.75, 26.0 },
            { 11.25, -18.75, 26.0 }, { 8.75, -18.75, 26.0 },
            { 8.75, -18.75, 26.0 }, { 8.75, -21.25, 26.0 },
        }};
        check(worldLines.size() == 24
                && std::equal(worldLines.begin(), worldLines.begin() + 8, expectedFirstEdges.begin(), sameVector)
                && std::all_of(worldLines.begin(), worldLines.end(), [](const TVector& point)
                    {
                        return (point.x == 8.75 || point.x == 11.25)
                            && (point.y == -21.25 || point.y == -18.75)
                            && (point.z == 26.0 || point.z == 34.0);
                    }),
            "IJIM guide is centered on the Thing but remains axis-aligned when the Thing rotates");
        if (worldLines.size() == 24)
        {
            double minX = std::numeric_limits<double>::infinity();
            double minY = std::numeric_limits<double>::infinity();
            double minZ = std::numeric_limits<double>::infinity();
            double maxX = -std::numeric_limits<double>::infinity();
            double maxY = -std::numeric_limits<double>::infinity();
            double maxZ = -std::numeric_limits<double>::infinity();
            for (const TVector& point : worldLines)
            {
                minX = std::min(minX, point.x);
                minY = std::min(minY, point.y);
                minZ = std::min(minZ, point.z);
                maxX = std::max(maxX, point.x);
                maxY = std::max(maxY, point.y);
                maxZ = std::max(maxZ, point.z);
            }
            check(minX == 8.75 && maxX == 11.25
                    && minY == -21.25 && maxY == -18.75
                    && minZ == 26.0 && maxZ == 34.0,
                "IJIM world guide uses exact unrotated extents around the transformed Thing origin");
        }
    }

    // The authored parameter guide is independent of collide and size.
    level.templates[1].values.push_back(integerField("collide", 0));
    const std::optional<AuthoredCollisionBoundsGuide> guideWithNoCollision =
        resolveIjimAuthoredCollisionBoundsGuide(level, nullptr, thing);
    check(guideWithNoCollision.has_value()
            && resolveCollisionDescription(level, nullptr, thing).shape == CollisionShape::none,
        "IJIM parameter guide remains separate when effective collide mode is none");

    level.templates[1].values.push_back(floatField("collwidth", 2.75));
    level.templates[1].values.push_back(floatField("collheight", 5.5));
    level.templates[1].values.push_back(integerField("collide", 1));
    level.templates[1].values.push_back(floatField("size", 3.75));
    const std::optional<AuthoredCollisionBoundsGuide> edited =
        resolveIjimAuthoredCollisionBoundsGuide(level, nullptr, thing);
    const CollisionDescription editedCollision = resolveCollisionDescription(level, nullptr, thing);
    check(edited.has_value()
            && edited->halfWidth == 2.75
            && edited->halfHeight == 5.5
            && editedCollision.shape == CollisionShape::sphere
            && editedCollision.hasRadius
            && editedCollision.radius == 3.75,
        "IJIM bounds and effective collide/size values resolve independently after edits");

    for (const ProjectType kind : { ProjectType::JKDF2, ProjectType::MOTS })
    {
        level.kind = kind;
        check(!resolveIjimAuthoredCollisionBoundsGuide(level, nullptr, thing),
            "IJIM authored bounds guide is hidden for non-IJIM project types");
    }

    level.kind = ProjectType::IJIM;
    level.templates[0].values = { integerField("collide", 1), floatField("size", 3.0) };
    level.templates[1].values = { integerField("collide", 1), floatField("size", 3.75) };
    check(!resolveIjimAuthoredCollisionBoundsGuide(level, nullptr, thing),
        "IJIM guide omits missing dimensions that use engine model/movesize fallback");

    level.templates[1].values = { floatField("collwidth", 1.5) };
    check(!resolveIjimAuthoredCollisionBoundsGuide(level, nullptr, thing),
        "IJIM guide omits the whole box when authored height is missing");
    level.templates[1].values = { floatField("collheight", 2.25) };
    check(!resolveIjimAuthoredCollisionBoundsGuide(level, nullptr, thing),
        "IJIM guide omits the whole box when authored width is missing");

    level.templates[1].values = {
        floatField("collwidth", -0.25),
        floatField("collheight", 1.0),
    };
    check(!resolveIjimAuthoredCollisionBoundsGuide(level, nullptr, thing),
        "IJIM guide rejects a box with negative width rather than showing a partial height marker");

    level.templates[1].values = {
        floatField("collwidth", 1.0),
        floatField("collheight", std::numeric_limits<double>::quiet_NaN()),
    };
    check(!resolveIjimAuthoredCollisionBoundsGuide(level, nullptr, thing),
        "IJIM guide rejects a box with a non-finite height");

    level.templates[1].values = {
        floatField("collwidth", static_cast<double>(std::numeric_limits<float>::max()) * 2.0),
        floatField("collheight", 1.0),
    };
    check(!resolveIjimAuthoredCollisionBoundsGuide(level, nullptr, thing),
        "IJIM guide rejects width outside the engine float range");

    level.templates[1].values = {
        stringField("collwidth", "1.125"),
        stringField("collheight", "2.25"),
    };
    const std::optional<AuthoredCollisionBoundsGuide> numericStrings =
        resolveIjimAuthoredCollisionBoundsGuide(level, nullptr, thing);
    check(numericStrings.has_value()
            && numericStrings->halfWidth == 1.125
            && numericStrings->halfHeight == 2.25,
        "IJIM guide parses both exact numeric string parameter values");

    level.templates[1].values = {
        stringField("collwidth", "not-a-number"),
        stringField("collheight", "2.25"),
    };
    check(!resolveIjimAuthoredCollisionBoundsGuide(level, nullptr, thing),
        "IJIM guide rejects malformed width instead of retaining a partial box");

    level.templates[1].values = {
        floatField("collwidth", 0.0),
        floatField("collheight", 2.25),
    };
    check(!resolveIjimAuthoredCollisionBoundsGuide(level, nullptr, thing),
        "IJIM guide omits zero width rather than inventing a zero-size side");
    level.templates[1].values = {
        floatField("collwidth", 1.75),
        floatField("collheight", 0.0),
    };
    check(!resolveIjimAuthoredCollisionBoundsGuide(level, nullptr, thing),
        "IJIM guide omits zero height rather than inventing a zero-size side");

    check(makeAuthoredCollisionBoundsGuideWirePoints(
              AuthoredCollisionBoundsGuide{ 0.0, 1.0 }, TMatrix43::identity()).empty()
            && makeAuthoredCollisionBoundsGuideWirePoints(
                   AuthoredCollisionBoundsGuide{ 1.0, -1.0 }, TMatrix43::identity()).empty(),
        "IJIM geometry helper refuses incomplete or nonpositive manually constructed guides");
}

} // namespace sed::app_tests
