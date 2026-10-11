#include "test_common.hpp"

#include "thing/thing_parameter_editor_utils.hpp"
#include "thing/thing_parameter_schema.hpp"

#include <algorithm>
#include <cstdint>
#include <string_view>
#include <vector>

namespace sed::app_tests
{
namespace
{

using namespace sed::thing::parameter_schema;

bool hasName(const std::vector<std::string_view>& names, const std::string_view wanted)
{
    return std::find(names.begin(), names.end(), wanted) != names.end();
}

const EnumChoice* findChoice(
    const std::vector<EnumChoice>& choices,
    const std::string_view value)
{
    const auto it = std::find_if(choices.begin(), choices.end(), [value](const EnumChoice& choice)
        {
            return choice.value == value;
        });
    return it == choices.end() ? nullptr : &*it;
}

const FlagDefinition* findFlag(
    const std::vector<FlagDefinition>& definitions,
    const std::uint32_t mask)
{
    const auto it = std::find_if(definitions.begin(), definitions.end(), [mask](const FlagDefinition& definition)
        {
            return definition.mask == mask;
        });
    return it == definitions.end() ? nullptr : &*it;
}

const ui::query::LevelQueryFlagDef* findEditorFlag(
    const std::vector<ui::query::LevelQueryFlagDef>& definitions,
    const std::uint32_t mask)
{
    const auto it = std::find_if(definitions.begin(), definitions.end(), [mask](const auto& definition)
        {
            return definition.bit == mask;
        });
    return it == definitions.end() ? nullptr : &*it;
}

} // namespace

void runThingParameterSchemaTests()
{
    const std::vector<std::string_view> jkNames = completionNames(ProjectType::JKDF2);
    const std::vector<std::string_view> motsNames = completionNames(ProjectType::MOTS);
    const std::vector<std::string_view> ijimNames = completionNames(ProjectType::IJIM);

    check(std::is_sorted(jkNames.begin(), jkNames.end())
            && std::adjacent_find(jkNames.begin(), jkNames.end()) == jkNames.end(),
        "thing_parameter_schema completion is sorted and unique");
    check(hasName(jkNames, "thingflags") && hasName(jkNames, "weapon2")
            && !hasName(jkNames, "treesize") && !hasName(jkNames, "respawnfactor"),
        "thing_parameter_schema JK completion uses the 72-name classic parser surface");
    check(hasName(motsNames, "treesize") && hasName(motsNames, "respawnfactor")
            && hasName(motsNames, "trailrandangle"),
        "thing_parameter_schema MotS completion includes JKM parser additions");
    check(hasName(ijimNames, "userval") && hasName(ijimNames, "voicecolor")
            && hasName(ijimNames, "collwidth") && !hasName(ijimNames, "weapon2")
            && !hasName(ijimNames, "trailthing"),
        "thing_parameter_schema IJIM completion uses the OpenJones3D parser surface");
    check(!hasName(jkNames, "attach") && !hasName(jkNames, "points")
            && !hasName(ijimNames, "attach"),
        "thing_parameter_schema omits registered-but-unparsed names from completion");
    check(isRepeatableParameter(ProjectType::JKDF2, "frame")
            && isRepeatableParameter(ProjectType::MOTS, "DEBRIS")
            && isRepeatableParameter(ProjectType::IJIM, "frame")
            && !isRepeatableParameter(ProjectType::IJIM, "numframes")
            && !isRepeatableParameter(ProjectType::JKDF2, "light")
            && !isRepeatableParameter(ProjectType::JKDF2, "future_mod_parameter"),
        "thing_parameter_schema identifies only engine-consumed repeated parameter families");
    check(repeatableParameterLimit(ProjectType::JKDF2, "debris") == std::size_t{ 4 }
            && repeatableParameterLimit(ProjectType::MOTS, "DEBRIS") == std::size_t{ 4 }
            && repeatableParameterLimit(ProjectType::IJIM, "debris") == std::size_t{ 16 }
            && !repeatableParameterLimit(ProjectType::JKDF2, "frame")
            && !repeatableParameterLimit(ProjectType::IJIM, "light"),
        "thing_parameter_schema exposes fixed debris capacity without inventing a frame limit");

    const std::vector<ParameterDefinition> jkDiagnostics = parameterDefinitions(ProjectType::JKDF2, true);
    const std::vector<ParameterDefinition> ijimDiagnostics = parameterDefinitions(ProjectType::IJIM, true);
    const auto noEffect = [](const std::vector<ParameterDefinition>& definitions, const std::string_view name)
    {
        const auto it = std::find_if(definitions.begin(), definitions.end(), [name](const ParameterDefinition& definition)
            {
                return definition.name == name;
            });
        return it != definitions.end() && !it->runtimeEffective;
    };
    check(noEffect(jkDiagnostics, "attach") && noEffect(jkDiagnostics, "points")
            && noEffect(ijimDiagnostics, "attach") && !noEffect(ijimDiagnostics, "points"),
        "thing_parameter_schema exposes no-effect hash names only for diagnostics");

    const std::vector<EnumChoice> moveChoices = enumChoices(ProjectType::JKDF2, "MOVE");
    const EnumChoice* physicsChoice = findChoice(moveChoices, "physics");
    check(physicsChoice && physicsChoice->name == "Physics",
        "thing_parameter_schema enum separates serialized move value from display label");

    const std::vector<EnumChoice> jkCollide = enumChoices(ProjectType::JKDF2, "collide");
    const std::vector<EnumChoice> motsCollide = enumChoices(ProjectType::MOTS, "collide");
    const std::vector<EnumChoice> ijimCollide = enumChoices(ProjectType::IJIM, "collide");
    const EnumChoice* sphereTree = findChoice(motsCollide, "2");
    check(!findChoice(jkCollide, "2") && sphereTree && sphereTree->name == "Sphere tree"
            && !findChoice(ijimCollide, "2") && findChoice(ijimCollide, "3"),
        "thing_parameter_schema collide picker writes numeric values and scopes sphere-tree to MotS");

    const std::vector<EnumChoice> classicTypes = enumChoices(ProjectType::MOTS, "type");
    const std::vector<EnumChoice> ijimTypes = enumChoices(ProjectType::IJIM, "type");
    check(!findChoice(classicTypes, "sprite") && findChoice(ijimTypes, "sprite")
            && findChoice(ijimTypes, "polyline"),
        "thing_parameter_schema type picker exposes IJIM-only Thing types");

    check(flagFamily("thingflags") == FlagFamily::thing
            && flagFamily("PHYSFLAGS") == FlagFamily::physics
            && flagFamily("damageclass") == FlagFamily::damage
            && flagFamily("typeflags", "player") == FlagFamily::actor
            && flagFamily("typeflags", "weapon") == FlagFamily::weapon
            && flagFamily("typeflags", "sprite") == std::nullopt
            && flagFamilyName(FlagFamily::explosion) == "Explosion",
        "thing_parameter_schema resolves flag parameter and contextual type family labels");

    const std::vector<FlagDefinition> classicThing = flagDefinitions(ProjectType::JKDF2, FlagFamily::thing);
    const std::vector<FlagDefinition> ijimThing = flagDefinitions(ProjectType::IJIM, FlagFamily::thing);
    const FlagDefinition* classicBit8 = findFlag(classicThing, 0x8u);
    const FlagDefinition* ijimBit8 = findFlag(ijimThing, 0x8u);
    check(classicBit8 && classicBit8->name == "Arch"
            && classicBit8->description
                == "Marks an Arch Thing rendered as part of level geometry."
            && classicBit8->sourceSymbol == "SITH_TF_ARCH"
            && ijimBit8 && ijimBit8->sourceSymbol == "SITH_TF_WHIPCLIMB",
        "thing_parameter_schema describes classic Thing bit 0x8 Arch without changing the IJIM bit meaning");

    const std::vector<FlagDefinition> classicPhysics = flagDefinitions(ProjectType::MOTS, FlagFamily::physics);
    const std::vector<FlagDefinition> ijimPhysics = flagDefinitions(ProjectType::IJIM, FlagFamily::physics);
    check(findFlag(classicPhysics, 0x01000000u)
            && findFlag(classicPhysics, 0x01000000u)->sourceSymbol == "SITH_PF_1000000"
            && findFlag(ijimPhysics, 0x01000000u)
            && findFlag(ijimPhysics, 0x01000000u)->sourceSymbol == "SITH_PF_MINECAR",
        "thing_parameter_schema keeps project-specific high physics bits");
    const FlagDefinition* ijimStartOrientMove = findFlag(ijimPhysics, 0x00020000u);
    check(ijimStartOrientMove
            && ijimStartOrientMove->description
                == "Thing starts moving in its oriented direction when created."
            && !ijimStartOrientMove->runtimeManaged,
        "thing_parameter_schema uses the authored IJIM start-orient-move meaning from Jones3D documentation");
    const FlagDefinition* classicFlying = findFlag(classicPhysics, 0x00002000u);
    const FlagDefinition* ijimFlying = findFlag(ijimPhysics, 0x00002000u);
    check(classicFlying
            && classicFlying->name == "Flying"
            && classicFlying->description
                == "Object is flying. Gravity applies only if Use gravity is also enabled."
            && classicFlying->sourceSymbol == "SITH_PF_FLY"
            && ijimFlying
            && ijimFlying->name == "Flying"
            && ijimFlying->description == classicFlying->description
            && ijimFlying->sourceSymbol == "SITH_PF_FLY",
        "thing_parameter_schema describes physics 0x2000 as active flying state with independent gravity control");

    const std::vector<FlagDefinition> classicActor = flagDefinitions(ProjectType::JKDF2, "typeflags", "actor");
    const std::vector<FlagDefinition> motsActor = flagDefinitions(ProjectType::MOTS, "typeflags", "actor");
    const std::vector<FlagDefinition> ijimActor = flagDefinitions(ProjectType::IJIM, "typeflags", "actor");
    check(!findFlag(classicActor, 0x08000000u)
            && findFlag(motsActor, 0x08000000u)
            && findFlag(motsActor, 0x08000000u)->sourceSymbol == "SITH_AF_FREEZE_MOVEMENT"
            && findFlag(ijimActor, 0x08000000u)
            && findFlag(ijimActor, 0x08000000u)->sourceSymbol == "SITH_AF_HUMAN",
        "thing_parameter_schema scopes high Actor typeflags by JK, MotS, and IJIM semantics");
    check(findFlag(ijimActor, 0x00400000u)
            && findFlag(ijimActor, 0x00400000u)->description
                == "Player is killed when entering a fall-death sector."
            && findFlag(ijimActor, 0x40000000u)
            && findFlag(ijimActor, 0x40000000u)->description
                == "Player has an electric whip.",
        "thing_parameter_schema uses the documented IJIM fall-death and electric-whip Actor meanings");

    check(findFlag(ijimThing, 0x00000800u)
            && findFlag(ijimThing, 0x00000800u)->description
                == "Object cannot be crushed by other objects.",
        "thing_parameter_schema uses the documented IJIM no-crush direction");

    const std::vector<FlagDefinition> classicWeapon = flagDefinitions(ProjectType::MOTS, "typeflags", "weapon");
    const std::vector<FlagDefinition> ijimWeapon = flagDefinitions(ProjectType::IJIM, "typeflags", "weapon");
    check(findFlag(classicWeapon, 0x00400000u)
            && findFlag(classicWeapon, 0x00400000u)->sourceSymbol == "SITH_WF_ACTORKILLDESTROY"
            && findFlag(ijimWeapon, 0x00400000u)
            && findFlag(ijimWeapon, 0x00400000u)->sourceSymbol == "SITH_WF_BLOODSPLATTER",
        "thing_parameter_schema keeps project-specific Weapon typeflags");
    check(findFlag(ijimWeapon, 0x00400000u)
            && findFlag(ijimWeapon, 0x00400000u)->description
                == "Destroy the weapon when it kills an AI actor; the default behavior is to explode.",
        "thing_parameter_schema exposes the documented IJIM high Weapon-flag behavior");

    const std::vector<FlagDefinition> jkItem = flagDefinitions(ProjectType::JKDF2, "typeflags", "item");
    const std::vector<FlagDefinition> motsItem = flagDefinitions(ProjectType::MOTS, "typeflags", "item");
    check(!findFlag(jkItem, 0x10u) && findFlag(motsItem, 0x10u)
            && findFlag(motsItem, 0x10u)->sourceSymbol == "SITH_ITEM_10",
        "thing_parameter_schema exposes MotS-only Item collision typeflag");

    const std::vector<FlagDefinition> ijimDamage = flagDefinitions(ProjectType::IJIM, FlagFamily::damage);
    const FlagDefinition* imp5 = findFlag(ijimDamage, 0x5000u);
    check(imp5 && imp5->kind == FlagKind::compositePreset,
        "thing_parameter_schema marks IJIM IMP5 as a composite preset rather than an independent bit");

    const std::vector<FlagDefinition> classicParticle =
        flagDefinitions(ProjectType::JKDF2, FlagFamily::particle);
    const std::vector<FlagDefinition> ijimParticle =
        flagDefinitions(ProjectType::IJIM, FlagFamily::particle);
    check(findFlag(classicParticle, 0x2u)
            && findFlag(classicParticle, 0x2u)->name == "Animate cel"
            && findFlag(ijimParticle, 0x2u)
            && findFlag(ijimParticle, 0x2u)->name == "Unused"
            && findFlag(ijimParticle, 0x4u)
            && findFlag(ijimParticle, 0x4u)->description == "Animate the particle texture cels.",
        "thing_parameter_schema keeps classic particle flags while using documented IJIM bit meanings");

    const std::vector<ui::query::LevelQueryFlagDef> ijimPhysicsEditor =
        thing::parameter_editor::flagEditorDefinitions(ijimPhysics);
    const ui::query::LevelQueryFlagDef* startOrientEditor =
        findEditorFlag(ijimPhysicsEditor, 0x00020000u);
    const ui::query::LevelQueryFlagDef* alignedEditor =
        findEditorFlag(ijimPhysicsEditor, 0x00000100u);
    const bool descriptionsAreUserFacing = std::all_of(
        ijimPhysicsEditor.begin(), ijimPhysicsEditor.end(), [](const auto& definition)
        {
            return !definition.description.contains(QStringLiteral("SITH_"))
                && !definition.description.contains(QStringLiteral("SITHEXPLOSION_"))
                && !definition.description.contains(QStringLiteral("----"));
        });
    check(descriptionsAreUserFacing
            && startOrientEditor && startOrientEditor->editable
            && alignedEditor && !alignedEditor->editable
            && alignedEditor->description.endsWith(
                QStringLiteral("Runtime-managed state; shown read-only.")),
        "thing_parameter_editor shows clean behavioral descriptions without engine enum constants");

    bool unknownEditorNamesAreClean = true;
    for (const ProjectType project : { ProjectType::JKDF2, ProjectType::MOTS, ProjectType::IJIM })
    {
        for (const FlagFamily family : {
                 FlagFamily::thing,
                 FlagFamily::physics,
                 FlagFamily::damage,
                 FlagFamily::actor,
                 FlagFamily::weapon,
                 FlagFamily::item,
                 FlagFamily::explosion,
                 FlagFamily::particle })
        {
            const std::vector<FlagDefinition> schema = flagDefinitions(project, family);
            const std::vector<ui::query::LevelQueryFlagDef> editor =
                thing::parameter_editor::flagEditorDefinitions(schema);
            for (const FlagDefinition& definition : schema)
            {
                if (!definition.name.starts_with("Unknown"))
                {
                    continue;
                }
                const ui::query::LevelQueryFlagDef* row = findEditorFlag(editor, definition.mask);
                unknownEditorNamesAreClean = unknownEditorNamesAreClean
                    && row && row->name == QStringLiteral("Unknown");
            }
        }
    }
    check(unknownEditorNamesAreClean,
        "thing_parameter_editor names every unknown flag simply Unknown in all projects");

    const std::vector<FlagDefinition> classicDamage = flagDefinitions(ProjectType::JKDF2, FlagFamily::damage);
    const std::uint32_t original = 0x80000001u;
    const std::uint32_t merged = mergeKnownSingleBitFlags(original, classicDamage, 0x2u);
    check(merged == 0x80000002u
            && setFlagMask(merged, 0x4u, true) == 0x80000006u
            && setFlagMask(0x80000006u, 0x2u, false) == 0x80000004u,
        "thing_parameter_schema flag updates preserve bits unknown to the active family");

    const std::uint32_t editableClassicThingMask =
        thing::parameter_editor::editableFlagEditMask(classicThing);
    check((editableClassicThingMask & 0x00000001u) != 0u
            && (editableClassicThingMask & 0x00080000u) != 0u
            && (editableClassicThingMask & 0x00000002u) == 0u
            && (editableClassicThingMask & 0x00000200u) == 0u
            && (editableClassicThingMask & 0x00000400u) == 0u,
        "thing_parameter_editor editable mask excludes runtime-managed Thing bits");

    const std::vector<FlagDefinition> protectedEditDefinitions{
        { 0x00000001u, "Editable A", {}, {}, FlagKind::singleBit, false },
        { 0x00000002u, "Runtime", {}, {}, FlagKind::singleBit, true },
        { 0x0000000cu, "Composite", {}, {}, FlagKind::compositePreset, false },
        { 0x00000010u, "Editable B", {}, {}, FlagKind::singleBit, false },
    };
    check(thing::parameter_editor::editableFlagEditMask(protectedEditDefinitions)
            == 0x00000011u,
        "thing_parameter_editor editable mask excludes composites as well as runtime bits");

    const std::optional<std::uint32_t> protectedMerge =
        thing::parameter_editor::applyProtectedFlagEditDelta(
            0x40000003u,
            0x00000003u,
            0x80000010u,
            protectedEditDefinitions);
    check(protectedMerge && *protectedMerge == 0x40000012u,
        "thing_parameter_editor mixed edit changes only editable bits and preserves each target's protected bits");

    check(!thing::parameter_editor::applyProtectedFlagEditDelta(
              0x40000003u,
              0x00000002u,
              0x80000000u,
              protectedEditDefinitions)
            && !thing::parameter_editor::applyProtectedFlagEditDelta(
                0x40000001u,
                0x00000000u,
                0x00000001u,
                protectedEditDefinitions),
        "thing_parameter_editor protected-only and already-satisfied flag edits are no-ops");
}

} // namespace sed::app_tests
