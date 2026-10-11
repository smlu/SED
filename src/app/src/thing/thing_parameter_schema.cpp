#include "thing/thing_parameter_schema.hpp"

#include <algorithm>
#include <array>
#include <cstddef>

namespace sed::thing::parameter_schema
{
namespace
{

// Audited against:
// - OpenJones3D develop b9c0eaa56bb51307b0787add4b614d9468e5d372
// - OpenJKDF2 master 0dbd1ea8bb2211c158f6af928b5e4fb14dfa0dac
// - Jones3D Documentation flags.md 94206bbe5efdaaa80290ecd2e5e4228a8b2b0ad0
// OpenJKDF2 has no develop branch. Its JKM_PARAMS comments identify the two
// MotS-only parser additions used below (treesize and respawnfactor).

constexpr std::uint8_t kJkdf2 = 1u << 0;
constexpr std::uint8_t kMots  = 1u << 1;
constexpr std::uint8_t kIjim  = 1u << 2;
constexpr std::uint8_t kClassic = kJkdf2 | kMots;
constexpr std::uint8_t kAll = kClassic | kIjim;

struct ParameterEntry
{
    ParameterDefinition definition{};
    std::uint8_t projects{};
};

constexpr Scope kActorOrParticle = Scope::actor | Scope::particle;
constexpr Scope kWeaponOrExplosion = Scope::weapon | Scope::explosion;
constexpr Scope kWeaponExplosionParticle = Scope::weapon | Scope::explosion | Scope::particle;
constexpr Scope kPathOrAi = Scope::path | Scope::ai;

constexpr auto kParameterEntries = std::to_array<ParameterEntry>({
    { { "type", "Thing runtime type.", Scope::common }, kAll },
    { { "collide", "Collision representation.", Scope::common }, kAll },
    { { "treesize", "MotS sphere-tree collision size.", Scope::common }, kMots },
    { { "move", "Movement controller.", Scope::common }, kAll },
    { { "size", "Collision and movement size.", Scope::common }, kAll },
    { { "thingflags", "General Thing flags.", Scope::common }, kAll },
    { { "timer", "Lifetime in seconds.", Scope::common }, kAll },
    { { "light", "Thing light; scalar in JK/MotS and RGB/range in IJIM.", Scope::common }, kAll },
    { { "attach", "Registered by the engine but not consumed by ParseArg.", Scope::common, false, false }, kAll },
    { { "soundclass", "Sound-class resource.", Scope::common }, kAll },
    { { "model3d", "3DO model resource.", Scope::common }, kAll },
    { { "sprite", "SPR resource.", Scope::common }, kAll },
    { { "surfdrag", "Surface drag.", Scope::physics }, kAll },
    { { "airdrag", "Air drag.", Scope::physics }, kAll },
    { { "staticdrag", "Static drag.", Scope::physics }, kAll },
    { { "mass", "Physics mass.", Scope::physics }, kAll },
    { { "height", "Physics height.", Scope::physics }, kAll },
    { { "physflags", "Physics movement flags.", Scope::physics }, kAll },
    { { "maxrotvel", "Maximum rotation velocity.", Scope::physics }, kAll },
    { { "maxvel", "Maximum linear velocity.", Scope::physics }, kAll },
    { { "vel", "Initial linear velocity vector.", Scope::physics }, kAll },
    { { "angvel", "Initial angular velocity vector.", Scope::physics }, kAll },
    { { "typeflags", "Type-specific Actor, Weapon, Item, Explosion, or Particle flags.",
          Scope::actor | Scope::weapon | Scope::item | Scope::explosion | Scope::particle }, kAll },
    { { "health", "Current Actor health.", Scope::actor }, kAll },
    { { "maxthrust", "Actor thrust or Particle growth speed.", kActorOrParticle }, kAll },
    { { "maxrotthrust", "Actor maximum rotational thrust.", Scope::actor }, kAll },
    { { "maxheadvel", "Actor maximum head velocity.", Scope::actor }, kIjim },
    { { "maxheadyaw", "Actor maximum head yaw.", Scope::actor }, kIjim },
    { { "jumpspeed", "Actor jump speed.", Scope::actor }, kAll },
    { { "weapon", "Actor weapon template.", Scope::actor }, kAll },
    { { "weapon2", "Second Actor weapon template.", Scope::actor }, kClassic },
    { { "damage", "Weapon or Explosion damage.", kWeaponOrExplosion }, kAll },
    { { "mindamage", "Weapon minimum damage.", Scope::weapon }, kAll },
    { { "damageclass", "Weapon or Explosion damage-class flags.", kWeaponOrExplosion }, kAll },
    { { "explode", "Actor or Weapon explosion template.", Scope::actor | Scope::weapon }, kAll },
    { { "frame", "Ordered Path frame or AI waypoint; capacity is declared by numframes.", kPathOrAi, true }, kAll },
    { { "numframes", "Number of Path frames or AI waypoints; must precede frame values.", kPathOrAi }, kAll },
    { { "puppet", "Puppet-class resource.", Scope::common }, kAll },
    { { "blasttime", "Explosion blast-phase time in seconds.", Scope::explosion }, kAll },
    { { "babytime", "IJIM child-explosion time in seconds.", Scope::explosion }, kIjim },
    { { "force", "Weapon or Explosion force.", kWeaponOrExplosion }, kAll },
    { { "maxlight", "Explosion maximum light.", Scope::explosion }, kAll },
    { { "range", "Weapon, Explosion, or Particle range.", kWeaponExplosionParticle }, kAll },
    { { "flashrgb", "Explosion flash RGB integer tuple.", Scope::explosion }, kAll },
    { { "expandtime", "IJIM Explosion expansion time in seconds.", Scope::explosion }, kIjim },
    { { "fadetime", "IJIM Explosion fade time in seconds.", Scope::explosion }, kIjim },
    { { "aiclass", "AI-class resource; selects AI control.", Scope::common }, kAll },
    { { "cog", "COG resource.", Scope::common }, kAll },
    { { "respawn", "Item respawn interval.", Scope::item }, kAll },
    { { "respawnfactor", "MotS Item respawn factor.", Scope::item }, kMots },
    { { "material", "Particle MAT resource.", Scope::particle }, kAll },
    { { "rate", "Weapon rate or Particle timeout rate.", Scope::weapon | Scope::particle }, kAll },
    { { "count", "Particle count.", Scope::particle }, kAll },
    { { "elementsize", "Particle element size; also a classic Weapon field.", Scope::weapon | Scope::particle }, kAll },
    { { "particle", "PAR resource.", Scope::common }, kAll },
    { { "maxhealth", "Actor maximum health.", Scope::actor }, kAll },
    { { "movesize", "Movement collision size; ignored for Actor/Player by the runtime.", Scope::common }, kAll },
    { { "orientspeed", "Physics orientation speed.", Scope::physics }, kAll },
    { { "buoyancy", "Physics buoyancy.", Scope::physics }, kAll },
    { { "eyeoffset", "Actor eye offset vector.", Scope::actor }, kAll },
    { { "minheadpitch", "Actor minimum head pitch.", Scope::actor }, kAll },
    { { "maxheadpitch", "Actor maximum head pitch.", Scope::actor }, kAll },
    { { "fireoffset", "Actor firing offset vector.", Scope::actor }, kAll },
    { { "lightoffset", "Actor headlight offset vector.", Scope::actor }, kAll },
    { { "lightintensity", "Actor headlight; scalar in JK/MotS and RGB in IJIM.", Scope::actor }, kAll },
    { { "points", "Registered by the classic engine but not consumed by ParseArg.", Scope::common, false, false }, kClassic },
    { { "debris", "Appended Explosion debris template (4 classic slots; 16 IJIM slots).", Scope::explosion, true }, kAll },
    { { "creatething", "Template created by this Thing.", Scope::common }, kAll },
    { { "trailthing", "Classic Weapon trail template.", Scope::weapon }, kClassic },
    { { "trailcylradius", "Classic Weapon trail cylinder radius.", Scope::weapon }, kClassic },
    { { "trailrandangle", "Classic Weapon trail random angle.", Scope::weapon }, kClassic },
    { { "minsize", "Particle minimum radius.", Scope::particle }, kAll },
    { { "pitchrange", "Particle pitch range.", Scope::particle }, kAll },
    { { "yawrange", "Particle yaw range.", Scope::particle }, kAll },
    { { "error", "Classic Actor aim error.", Scope::actor }, kClassic },
    { { "fov", "Classic Actor field of view.", Scope::actor }, kClassic },
    { { "chance", "Classic Actor chance value.", Scope::actor }, kClassic },
    { { "orient", "Thing orientation tuple.", Scope::common }, kAll },
    { { "userval", "IJIM user value.", Scope::common }, kIjim },
    { { "fleshhit", "Classic Weapon flesh-hit template.", Scope::weapon }, kClassic },
    { { "spritething", "IJIM Explosion sprite template.", Scope::explosion }, kIjim },
    { { "spritestart", "IJIM Explosion sprite start vector.", Scope::explosion }, kIjim },
    { { "spriteend", "IJIM Explosion sprite end vector.", Scope::explosion }, kIjim },
    { { "collheight", "IJIM collision height.", Scope::common }, kIjim },
    { { "collwidth", "IJIM collision width.", Scope::common }, kIjim },
    { { "voicecolor", "IJIM Actor 16-component voice-color tuple.", Scope::actor }, kIjim },
    { { "perflevel", "IJIM performance level.", Scope::common }, kIjim },
});

constexpr FlagDefinition bit(
    const std::uint32_t mask,
    const std::string_view name,
    const std::string_view description,
    const std::string_view sourceSymbol,
    const bool runtimeManaged = false)
{
    return { mask, name, description, sourceSymbol, FlagKind::singleBit, runtimeManaged };
}

constexpr FlagDefinition preset(
    const std::uint32_t mask,
    const std::string_view name,
    const std::string_view description,
    const std::string_view sourceSymbol)
{
    return { mask, name, description, sourceSymbol, FlagKind::compositePreset, false };
}

constexpr auto kThingClassic = std::to_array<FlagDefinition>({
    bit(0x00000001u, "Emit light", "Thing emits light.", "SITH_TF_EMITLIGHT"),
    bit(0x00000002u, "Destroyed", "Runtime destroyed state.", "SITH_TF_DESTROYED", true),
    bit(0x00000004u, "Unknown 0x00000004", "Meaning is not named by OpenJKDF2.", "SITH_TF_4"),
    bit(0x00000008u, "Arch", "Marks an Arch Thing rendered as part of level geometry.", "SITH_TF_ARCH"),
    bit(0x00000010u, "Unknown 0x00000010", "Meaning is not named by OpenJKDF2.", "SITH_TF_10"),
    bit(0x00000020u, "Unknown 0x00000020", "Meaning is not named by OpenJKDF2.", "SITH_TF_20"),
    bit(0x00000040u, "Standable", "Thing can be stood on.", "SITH_TF_STANDABLE"),
    bit(0x00000080u, "Unknown 0x00000080", "Meaning is not named by OpenJKDF2.", "SITH_TF_80"),
    bit(0x00000100u, "Invulnerable", "Thing is invulnerable.", "SITH_TF_INVULN"),
    bit(0x00000200u, "Dead", "Runtime dead state.", "SITH_TF_DEAD", true),
    bit(0x00000400u, "Captured / COG linked", "Thing is captured by local COG logic.", "SITH_TF_CAPTURED", true),
    bit(0x00000800u, "No impact damage", "Thing does not receive impact damage.", "SITH_TF_NOIMPACTDAMAGE"),
    bit(0x00001000u, "Not on Easy", "Exclude Thing on Easy difficulty.", "SITH_TF_NOEASY"),
    bit(0x00002000u, "Not on Medium", "Exclude Thing on Medium difficulty.", "SITH_TF_NOMEDIUM"),
    bit(0x00004000u, "Not on Hard", "Exclude Thing on Hard difficulty.", "SITH_TF_NOHARD"),
    bit(0x00008000u, "Not in multiplayer", "World loader excludes this Thing in multiplayer.", "SITH_TF_8000"),
    bit(0x00010000u, "Not in single-player", "World loader excludes this Thing in single-player.", "SITH_TF_10000"),
    bit(0x00020000u, "Pulse set", "Runtime pulse timer state.", "SITH_TF_PULSESET", true),
    bit(0x00040000u, "Timer set", "Runtime timer state.", "SITH_TF_TIMERSET", true),
    bit(0x00080000u, "Disabled", "Thing is disabled.", "SITH_TF_DISABLED"),
    bit(0x00100000u, "In camera FOV", "Runtime camera visibility state.", "SITH_TF_INCAMFOV", true),
    bit(0x00200000u, "Render weapon", "Render the Thing as a weapon.", "SITH_TF_RENDERWEAPON"),
    bit(0x00400000u, "Metal", "Metal Thing material behavior.", "SITH_TF_METAL"),
    bit(0x00800000u, "Earth", "Earth Thing material behavior.", "SITH_TF_EARTH"),
    bit(0x01000000u, "Unknown 0x01000000", "Meaning is not named by OpenJKDF2.", "SITH_TF_1000000"),
    bit(0x02000000u, "Water", "Water Thing behavior.", "SITH_TF_WATER"),
    bit(0x04000000u, "Ignore Gouraud distance", "Ignore Gouraud distance limits.", "SITH_TF_IGNOREGOURAUDDISTANCE"),
    bit(0x08000000u, "Drowns", "Thing can drown.", "SITH_TF_DROWNS"),
    bit(0x10000000u, "Water creature", "Thing is a water creature.", "SITH_TF_WATERCREATURE"),
    bit(0x20000000u, "Splashes", "Thing produces splash behavior.", "SITH_TF_SPLASHES"),
    bit(0x40000000u, "Unknown 0x40000000", "Meaning is not named by OpenJKDF2.", "SITH_TF_40000000"),
    bit(0x80000000u, "Unknown 0x80000000", "Meaning is not named by OpenJKDF2.", "SITH_TF_80000000"),
});

constexpr auto kThingIjim = std::to_array<FlagDefinition>({
    bit(0x00000001u, "Emit light", "Object emits light.", "SITH_TF_EMITLIGHT"),
    bit(0x00000002u, "Destroyed", "Object is destroyed.", "SITH_TF_DESTROYED", true),
    bit(0x00000004u, "No weapon collision", "Weapon projectiles do not collide with the object.", "SITH_TF_NOWEAPONCOLLIDE"),
    bit(0x00000008u, "Whip climb", "Object is a whip-climb structure.", "SITH_TF_WHIPCLIMB"),
    bit(0x00000010u, "Invisible", "Object is invisible but still interacts with other objects.", "SITH_TF_INVISIBLE"),
    bit(0x00000020u, "Unknown / reserved", "Unknown or reserved.", "SITH_TF_NOSYNC"),
    bit(0x00000040u, "Stand on", "Object can be stood on.", "SITH_TF_STANDON"),
    bit(0x00000080u, "Mountable", "Object is mountable.", "SITH_TF_MOUNTABLE"),
    bit(0x00000100u, "Remote", "Object is remotely controlled in multiplayer.", "SITH_TF_REMOTE", true),
    bit(0x00000200u, "Dying", "Object is dying and will be removed from the game.", "SITH_TF_DYING", true),
    bit(0x00000400u, "COG linked", "Object is linked to COG logic.", "SITH_TF_COGLINKED", true),
    bit(0x00000800u, "No crush", "Object cannot be crushed by other objects.", "SITH_TF_NOCRUSH"),
    bit(0x00001000u, "Unknown 0x00001000", "Unknown; related to stand-on collision.", "SITH_TF_UNKNOWN_1000"),
    bit(0x00002000u, "Wood", "Object is made of wood and uses wood impact sounds.", "SITH_TF_WOOD"),
    bit(0x00004000u, "Shadow", "Object casts a shadow.", "SITH_TF_SHADOW"),
    bit(0x00008000u, "Jeep stop", "Object blocks jeep movement without colliding with other object types.", "SITH_TF_JEEPSTOP"),
    bit(0x00010000u, "Snow", "Object is made of snow and uses snow impact sounds.", "SITH_TF_SNOW"),
    bit(0x00020000u, "Pulse set", "Object's COG pulse timer is set.", "SITH_TF_PULSESET", true),
    bit(0x00040000u, "Timer set", "Object's COG timer is set.", "SITH_TF_TIMERSET", true),
    bit(0x00080000u, "Disabled", "Object is disabled, not rendered, and cannot interact with other objects.", "SITH_TF_DISABLED"),
    bit(0x00100000u, "Seen", "Object has already been seen by the camera.", "SITH_TF_SEEN", true),
    bit(0x00200000u, "Unknown 0x00200000", "Meaning is not known.", "SITH_TF_UNKNOWN_200000"),
    bit(0x00400000u, "Metal", "Object is made of metal and uses metal impact sounds.", "SITH_TF_METAL"),
    bit(0x00800000u, "Earth", "Object is made of dirt and uses dirt impact sounds.", "SITH_TF_EARTH"),
    bit(0x01000000u, "No sound", "Object makes no sound.", "SITH_TF_NOSOUND"),
    bit(0x02000000u, "Submerged", "Object is submerged in water or aetherium.", "SITH_TF_SUBMERGED", true),
    bit(0x04000000u, "Climbable crate", "Object is a crate and can be climbed on.", "SITH_TF_CLIMBCRATE"),
    bit(0x08000000u, "Destroyed in water", "Object is destroyed in water.", "SITH_TF_WATERDESTROYED"),
    bit(0x10000000u, "Destroyed in air", "Object is destroyed in air.", "SITH_TF_AIRDESTROYED"),
    bit(0x20000000u, "Splash", "Object creates a splash effect and sends a splash message to its COG.", "SITH_TF_SPLASH"),
    bit(0x40000000u, "Movable", "Object can be pushed or pulled.", "SITH_TF_MOVABLE"),
    bit(0x80000000u, "Whip swing", "Object is a whip-swing structure.", "SITH_TF_WHIPSWING"),
});

constexpr auto kPhysicsClassic = std::to_array<FlagDefinition>({
    bit(0x00000001u, "Use gravity", "Apply gravity.", "SITH_PF_USEGRAVITY"),
    bit(0x00000002u, "Use thrust", "Apply linear thrust.", "SITH_PF_USETHRUST"),
    bit(0x00000004u, "Unknown 0x00000004", "Meaning is not named by OpenJKDF2.", "SITH_PF_4"),
    bit(0x00000008u, "Unknown 0x00000008", "Meaning is not named by OpenJKDF2.", "SITH_PF_8"),
    bit(0x00000010u, "Align surface", "Align to contacted surfaces.", "SITH_PF_ALIGNSURFACE"),
    bit(0x00000020u, "Surface bounce", "Bounce from surfaces.", "SITH_PF_SURFACEBOUNCE"),
    bit(0x00000040u, "Floor stick", "Stick to floors.", "SITH_PF_FLOORSTICK"),
    bit(0x00000080u, "Wall stick", "Stick to walls.", "SITH_PF_WALLSTICK"),
    bit(0x00000100u, "Unknown 0x00000100", "Meaning is not named by OpenJKDF2.", "SITH_PF_100"),
    bit(0x00000200u, "Use rotation velocity", "Use angular velocity.", "SITH_PF_USEROTATIONVELOCITY"),
    bit(0x00000400u, "Banked turn", "Use banked turning.", "SITH_PF_BANKEDTURN"),
    bit(0x00000800u, "Unknown 0x00000800", "Meaning is not named by OpenJKDF2.", "SITH_PF_800"),
    bit(0x00001000u, "Use angular thrust", "Apply angular thrust.", "SITH_PF_USEANGULARTHRUST"),
    bit(0x00002000u, "Flying", "Object is flying. Gravity applies only if Use gravity is also enabled.", "SITH_PF_FLY"),
    bit(0x00004000u, "Use blast force", "Respond to blast force.", "SITH_PF_USEBLASTFORCE"),
    bit(0x00008000u, "Unknown 0x00008000", "Meaning is not named by OpenJKDF2.", "SITH_PF_8000"),
    bit(0x00010000u, "Crouching", "Runtime crouching state.", "SITH_PF_CROUCHING", true),
    bit(0x00020000u, "Unknown 0x00020000", "Meaning is not named by OpenJKDF2.", "SITH_PF_20000"),
    bit(0x00040000u, "Partial gravity", "Use partial gravity.", "SITH_PF_PARTIALGRAVITY"),
    bit(0x00080000u, "Unknown 0x00080000", "Meaning is not named by OpenJKDF2.", "SITH_PF_80000"),
    bit(0x00100000u, "On water surface", "Runtime water-surface state.", "SITH_PF_ONWATERSURFACE", true),
    bit(0x00200000u, "Unknown 0x00200000", "Meaning is not named by OpenJKDF2.", "SITH_PF_200000"),
    bit(0x00400000u, "No thrust", "Disable thrust.", "SITH_PF_NOTHRUST"),
    bit(0x00800000u, "Unknown 0x00800000", "Meaning is not named by OpenJKDF2.", "SITH_PF_800000"),
    bit(0x01000000u, "Unknown 0x01000000", "Meaning is not named by OpenJKDF2.", "SITH_PF_1000000"),
    bit(0x02000000u, "Unknown 0x02000000", "Meaning is not named by OpenJKDF2.", "SITH_PF_2000000"),
    bit(0x04000000u, "Unknown 0x04000000", "Meaning is not named by OpenJKDF2.", "SITH_PF_4000000"),
    bit(0x08000000u, "Unknown 0x08000000", "Meaning is not named by OpenJKDF2.", "SITH_PF_8000000"),
});

constexpr auto kPhysicsIjim = std::to_array<FlagDefinition>({
    bit(0x00000001u, "Use gravity", "Object uses gravity.", "SITH_PF_USEGRAVITY"),
    bit(0x00000002u, "Use thrust", "Object uses thrust.", "SITH_PF_USETHRUST"),
    bit(0x00000004u, "Unknown 0x00000004", "Meaning is not known.", "SITH_PF_UNKNOWN_4"),
    bit(0x00000008u, "Unknown 0x00000008", "Meaning is not known.", "SITH_PF_UNKNOWN_8"),
    bit(0x00000010u, "Align surface", "Object aligns to surfaces.", "SITH_PF_ALIGNSURFACE"),
    bit(0x00000020u, "Surface bounce", "Object bounces off surfaces.", "SITH_PF_SURFACEBOUNCE"),
    bit(0x00000040u, "Floor stick", "Object sticks to floors.", "SITH_PF_FLOORSTICK"),
    bit(0x00000080u, "Wall stick", "Object sticks to walls.", "SITH_PF_WALLSTICK"),
    bit(0x00000100u, "Aligned", "Object is aligned.", "SITH_PF_ALIGNED", true),
    bit(0x00000200u, "Use rotation velocity", "Object uses rotational velocity.", "SITH_PF_USEROTATIONVELOCITY"),
    bit(0x00000400u, "Banked turn", "Object banks when turning.", "SITH_PF_BANKEDTURN"),
    bit(0x00000800u, "Align up", "Object aligns up, keeping pitch at zero.", "SITH_PF_ALIGNUP"),
    bit(0x00001000u, "Use angular thrust", "Object uses angular thrust.", "SITH_PF_USEANGULARTHRUST"),
    bit(0x00002000u, "Flying", "Object is flying. Gravity applies only if Use gravity is also enabled.", "SITH_PF_FLY"),
    bit(0x00004000u, "Use blast force", "Object is affected by explosion blast force.", "SITH_PF_USEBLASTFORCE"),
    bit(0x00008000u, "Unknown 0x00008000", "Unknown.", "SITH_PF_FORCEAPPLIED"),
    bit(0x00010000u, "Crouching", "Object is crouching.", "SITH_PF_CROUCHING", true),
    bit(0x00020000u, "Start orient move", "Thing starts moving in its oriented direction when created.", "SITH_PF_STARTORIENTMOVE"),
    bit(0x00040000u, "Partial gravity", "Object uses partial gravity.", "SITH_PF_PARTIALGRAVITY"),
    bit(0x00080000u, "Unknown 0x00080000", "Meaning is not known.", "SITH_PF_UNKNOWN_80000"),
    bit(0x00100000u, "On water surface", "Thing is afloat on the water surface.", "SITH_PF_ONWATERSURFACE", true),
    bit(0x00200000u, "Unknown 0x00200000", "Meaning is not known.", "SITH_PF_UNKNOWN_200000"),
    bit(0x00400000u, "No thrust", "Object is not affected by thrust.", "SITH_PF_NOTHRUST"),
    bit(0x00800000u, "Physics disabled", "Physics is disabled for the object.", "SITH_PF_NOUPDATE"),
    bit(0x01000000u, "Minecar", "Object is a minecar.", "SITH_PF_MINECAR"),
    bit(0x02000000u, "Raft", "Object is a raft.", "SITH_PF_RAFT"),
    bit(0x04000000u, "Jeep", "Object is a jeep.", "SITH_PF_JEEP"),
    bit(0x08000000u, "Unknown 0x08000000", "Meaning is not known.", "SITH_PF_UNKNOWN_8000000"),
    bit(0x10000000u, "Unknown 0x10000000", "Meaning is not known.", "SITH_PF_UNKNOWN_10000000"),
    bit(0x20000000u, "Unknown 0x20000000", "Meaning is not known.", "SITH_PF_UNKNOWN_20000000"),
    bit(0x40000000u, "Unknown 0x40000000", "Meaning is not known.", "SITH_PF_UNKNOWN_40000000"),
    bit(0x80000000u, "Unknown 0x80000000", "Meaning is not known.", "SITH_PF_UNKNOWN_80000000"),
});

constexpr auto kDamageClassic = std::to_array<FlagDefinition>({
    bit(0x00000001u, "Impact", "Impact damage.", "SITH_DAMAGE_IMPACT"),
    bit(0x00000002u, "Energy", "Energy damage.", "SITH_DAMAGE_ENERGY"),
    bit(0x00000004u, "Fire", "Fire damage.", "SITH_DAMAGE_FIRE"),
    bit(0x00000008u, "Force", "Force damage.", "SITH_DAMAGE_FORCE"),
    bit(0x00000010u, "Saber", "Lightsaber damage.", "SITH_DAMAGE_SABER"),
    bit(0x00000020u, "Drown", "Drowning damage.", "SITH_DAMAGE_DROWN"),
    bit(0x00000040u, "Fall", "Falling damage.", "SITH_DAMAGE_FALL"),
});

constexpr auto kDamageIjim = std::to_array<FlagDefinition>({
    bit(0x00000001u, "Impact", "Impact damage, such as a bullet projectile.", "SITH_DAMAGE_IMPACT"),
    bit(0x00000002u, "Energy", "Energy damage.", "SITH_DAMAGE_ENERGY"),
    bit(0x00000004u, "Fire", "Fire damage.", "SITH_DAMAGE_FIRE"),
    bit(0x00000008u, "Fists", "Fist damage.", "SITH_DAMAGE_FISTS"),
    bit(0x00000010u, "Whip", "Whip damage.", "SITH_DAMAGE_WHIP"),
    bit(0x00000020u, "Machete", "Machete damage.", "SITH_DAMAGE_MACHETE"),
    bit(0x00000040u, "Drown", "Drowning damage.", "SITH_DAMAGE_DROWN"),
    bit(0x00000080u, "Crush", "Crushing damage.", "SITH_DAMAGE_CRUSH"),
    bit(0x00000100u, "Poison", "Poison damage.", "SITH_DAMAGE_POISON"),
    bit(0x00000200u, "Lava", "Lava damage.", "SITH_DAMAGE_LAVA"),
    bit(0x00000400u, "Unknown 0x00000400", "Meaning is not known.", "SITH_DAMAGE_400"),
    bit(0x00000800u, "Electro-whip", "Electrified whip damage.", "SITH_DAMAGE_ELECTROWHIP"),
    bit(0x00001000u, "IMP1", "Infernal Machine part 1 damage.", "SITH_DAMAGE_IMP1"),
    bit(0x00002000u, "Unknown 0x00002000", "Unknown damage; possibly reserved for Infernal Machine part 2.", "SITH_DAMAGE_2000"),
    bit(0x00004000u, "IMP4", "Infernal Machine part 4 damage.", "SITH_DAMAGE_IMP4"),
    preset(0x00005000u, "IMP5 preset", "Named composite of the 0x1000 and 0x4000 bits.", "SITH_DAMAGE_IMP5"),
    bit(0x00100000u, "Lightning", "Lightning damage.", "SITH_DAMAGE_LIGHTNING"),
    bit(0x00200000u, "Laser", "Laser damage.", "SITH_DAMAGE_LASER"),
    bit(0x00400000u, "Razor rock", "Razor-rock damage.", "SITH_DAMAGE_RAZOR_ROCK"),
    bit(0x00800000u, "Raft leak", "Raft-leak damage.", "SITH_DAMAGE_RAFT_LEAK"),
    bit(0x01000000u, "Scrape", "Scraping damage.", "SITH_DAMAGE_SCRAPE"),
    bit(0x02000000u, "Vehicle", "Vehicle impact damage.", "SITH_DAMAGE_VEHICLE"),
    bit(0x04000000u, "Bonk", "Head-impact damage, such as hitting a pipe while riding a minecar.", "SITH_DAMAGE_BONK"),
    bit(0x08000000u, "Debris", "Debris damage.", "SITH_DAMAGE_DEBRIS"),
    bit(0x10000000u, "IMP blast", "Infernal Machine blast damage.", "SITH_DAMAGE_IMP_BLAST"),
    bit(0x20000000u, "Hit", "Hit damage.", "SITH_DAMAGE_HIT"),
    bit(0x40000000u, "Cold water", "Cold-water damage.", "SITH_DAMAGE_COLD_WATER"),
    bit(0x80000000u, "Dart", "Dart damage.", "SITH_DAMAGE_DART"),
});

constexpr auto kActorClassic = std::to_array<FlagDefinition>({
    bit(0x00000001u, "Can rotate head", "Actor can rotate its head.", "SITH_AF_CANROTATEHEAD"),
    bit(0x00000002u, "View centring", "Actor view is centring.", "SITH_AF_VIEWCENTRING", true),
    bit(0x00000004u, "Headlight", "Actor has a headlight.", "SITH_AF_HEADLIGHT"),
    bit(0x00000008u, "Invulnerable", "Actor is invulnerable.", "SITH_AF_INVULNERABLE"),
    bit(0x00000010u, "View centred", "Actor view is centred.", "SITH_AF_VIEWCENTRED", true),
    bit(0x00000020u, "Explode when killed", "Use the Actor explosion template on death.", "SITH_AF_EXPLODE_WHEN_KILLED"),
    bit(0x00000040u, "Breathe underwater", "Actor can breathe underwater.", "SITH_AF_BREATHEUNDERWATER"),
    bit(0x00000080u, "Invisible", "Actor is invisible.", "SITH_AF_INVISIBLE"),
    bit(0x00000100u, "Droid", "Actor is a droid.", "SITH_AF_DROID"),
    bit(0x00000200u, "Boss", "Actor is a boss.", "SITH_AF_BOSS"),
    bit(0x00000400u, "Deaf", "Actor cannot hear.", "SITH_AF_DEAF"),
    bit(0x00000800u, "Blind", "Actor cannot see.", "SITH_AF_BLIND"),
    bit(0x00001000u, "Unknown 0x00001000", "Meaning is not named by OpenJKDF2.", "SITH_AF_1000"),
    bit(0x00002000u, "Bleeds", "Actor bleeds.", "SITH_AF_BLEEDS"),
    bit(0x00004000u, "Unknown 0x00004000", "Meaning is not named by OpenJKDF2.", "SITH_AF_4000"),
    bit(0x00008000u, "See invisible", "Actor can see invisible targets.", "SITH_AF_SEEINVISIBLE"),
    bit(0x00010000u, "Screaming", "Actor uses screaming behavior.", "SITH_AF_SCREAMING"),
    bit(0x00020000u, "Delay fire", "Actor delays firing.", "SITH_AF_DELAYFIRE"),
    bit(0x00040000u, "Immobile", "Actor is immobile.", "SITH_AF_IMMOBILE"),
    bit(0x00080000u, "No underwater fire", "Actor cannot fire underwater.", "SITH_AF_NOUNDERWATERFIRE"),
    bit(0x00100000u, "No target", "Actor cannot be targeted.", "SITH_AF_NOTARGET"),
    bit(0x00200000u, "Controls disabled", "Actor controls are disabled.", "SITH_AF_CONTROLSDISABLED"),
    bit(0x00400000u, "Fall killed", "Actor has been killed by falling.", "SITH_AF_FALLKILLED", true),
    bit(0x00800000u, "No idle camera", "Disable idle camera behavior.", "SITH_AF_NOIDLECAMERA"),
    bit(0x01000000u, "Full damage", "Actor receives full damage.", "SITH_AF_FULLDAMAGE"),
    bit(0x02000000u, "See in dark", "Actor can see in darkness.", "SITH_AF_SEEINDARK"),
    bit(0x04000000u, "Unknown 0x04000000", "Meaning is not named by OpenJKDF2.", "SITH_AF_4000000"),
    bit(0x08000000u, "Freeze movement (MotS)", "MotS movement-freeze flag.", "SITH_AF_FREEZE_MOVEMENT"),
    bit(0x10000000u, "Flyer move (MotS)", "MotS flyer movement flag.", "SITH_AF_FLYERMOVE"),
    bit(0x20000000u, "Scope HUD (MotS)", "MotS scoped HUD behavior.", "SITH_AF_SCOPEHUD"),
    bit(0x40000000u, "Electric whip (MotS)", "MotS-gated electric-whip behavior in OpenJKDF2.", "SITH_AF_ELECTRICWHIP"),
    bit(0x80000000u, "Arachnid (MotS)", "MotS arachnid camera/HUD behavior.", "SITH_AF_ARACHNID"),
});

constexpr auto kActorIjim = std::to_array<FlagDefinition>({
    bit(0x00000001u, "Can rotate head", "Actor can rotate its head.", "SITH_AF_CANROTATEHEAD"),
    bit(0x00000002u, "View centring", "Actor's view is being centred by the engine.", "SITH_AF_VIEWCENTRING", true),
    bit(0x00000004u, "Headlight", "Actor has a headlight.", "SITH_AF_HEADLIGHT"),
    bit(0x00000008u, "Invulnerable", "Actor is invulnerable.", "SITH_AF_INVULNERABLE"),
    bit(0x00000010u, "View centred", "Actor's view is centred by the engine.", "SITH_AF_VIEWCENTRED", true),
    bit(0x00000020u, "Explode when killed", "Actor explodes when killed.", "SITH_AF_EXPLODE_WHEN_KILLED"),
    bit(0x00000040u, "Breathe underwater", "Actor can breathe underwater.", "SITH_AF_BREATHEUNDERWATER"),
    bit(0x00000080u, "Invisible", "Actor is invisible to other actors.", "SITH_AF_INVISIBLE"),
    bit(0x00000100u, "Droid", "Actor is a droid.", "SITH_AF_DROID"),
    bit(0x00000200u, "Boss", "Actor is a boss.", "SITH_AF_BOSS"),
    bit(0x00000400u, "Deaf", "Actor is deaf.", "SITH_AF_DEAF"),
    bit(0x00000800u, "Blind", "Actor is blind.", "SITH_AF_BLIND"),
    bit(0x00001000u, "See invisible", "Actor can see through walls and see invisible objects.", "SITH_AF_SEEINVISIBLE"),
    bit(0x00002000u, "Poisoned", "Player is poisoned and the poisoned HUD is rendered.", "SITH_AF_POISONED", true),
    bit(0x00004000u, "Move 15% faster", "Actor moves 15% faster.", "SITH_AF_FASTMOVE15"),
    bit(0x00008000u, "Move 10% faster", "Actor moves 10% faster.", "SITH_AF_FASTMOVE10"),
    bit(0x00010000u, "No slope move", "Actor cannot move onto slopes and slides down after landing on one.", "SITH_AF_NOSLOPEMOVE"),
    bit(0x00020000u, "Delay fire", "Actor delays firing.", "SITH_AF_DELAYFIRE"),
    bit(0x00040000u, "Immobile", "Actor is immobile.", "SITH_AF_IMMOBILE"),
    bit(0x00080000u, "No underwater fire", "Actor cannot fire underwater.", "SITH_AF_NOUNDERWATERFIRE"),
    bit(0x00100000u, "No target", "Actor cannot be targeted.", "SITH_AF_NOTARGET"),
    bit(0x00200000u, "Controls disabled", "Player controls are disabled.", "SITH_AF_CONTROLSDISABLED"),
    bit(0x00400000u, "Fall death", "Player is killed when entering a fall-death sector.", "SITH_AF_FALLKILLED", true),
    bit(0x00800000u, "No idle camera", "Idle camera is disabled for the external camera.", "SITH_AF_NOIDLECAMERA"),
    bit(0x01000000u, "Full damage", "Actor receives full projectile and explosion damage instead of the default half damage.", "SITH_AF_FULLDAMAGE"),
    bit(0x02000000u, "See in dark", "Actor can see in darkness.", "SITH_AF_SEEINDARK"),
    bit(0x04000000u, "Unknown 0x04000000", "Meaning is not known.", "SITH_AF_UNKNOWN_4000000"),
    bit(0x08000000u, "Human", "Actor is human.", "SITH_AF_HUMAN"),
    bit(0x10000000u, "Aircraft movement", "Actor moves like an aircraft.", "SITH_AF_FLYERMOVE"),
    bit(0x20000000u, "Unknown 0x20000000", "Meaning is not known.", "SITH_AF_UNKNOWN_20000000"),
    bit(0x40000000u, "Electric whip", "Player has an electric whip.", "SITH_AF_ELECTRICWHIP"),
    bit(0x80000000u, "Arachnid", "Actor is an arachnid, such as a spider or scorpion.", "SITH_AF_ARACHNID"),
});

constexpr auto kWeaponClassic = std::to_array<FlagDefinition>({
    bit(0x00000001u, "No shooter damage", "Do not damage the shooter.", "SITH_WF_NOSHOOTERDAMAGE"),
    bit(0x00000002u, "Unknown 0x00000002", "Meaning is not named by OpenJKDF2.", "SITH_WF_2"),
    bit(0x00000004u, "Explode on face hit", "Explode when hitting a face.", "SITH_WF_FACEHITEXPLODE"),
    bit(0x00000008u, "Explode on Thing hit", "Explode when hitting a Thing.", "SITH_WF_THINGHITEXPLODE"),
    bit(0x00000010u, "Unknown 0x00000010", "Meaning is not named by OpenJKDF2.", "SITH_WF_10"),
    bit(0x00000020u, "Unknown 0x00000020", "Meaning is not named by OpenJKDF2.", "SITH_WF_20"),
    bit(0x00000040u, "Unknown 0x00000040", "Meaning is not named by OpenJKDF2.", "SITH_WF_40"),
    bit(0x00000080u, "Attach to face", "Attach projectile to a face.", "SITH_WF_ATTACHFACE"),
    bit(0x00000100u, "Explode", "Weapon is in explode state.", "SITH_WF_EXPLODE", true),
    bit(0x00000200u, "Damage destroys", "Damage schedules weapon destruction.", "SITH_WF_DAMAGEDESTROY"),
    bit(0x00000400u, "Impact sound", "Play impact sound.", "SITH_WF_IMPACTSOUND"),
    bit(0x00000800u, "Attach to Thing", "Attach projectile to a Thing.", "SITH_WF_ATTACHTHING"),
    bit(0x00001000u, "Proximity", "Use proximity detonation.", "SITH_WF_PROXIMITY"),
    bit(0x00002000u, "Instant impact", "Use instant-impact behavior.", "SITH_WF_INSTANT_IMPACT"),
    bit(0x00004000u, "Damage decay", "Damage decays over time/range.", "SITH_WF_DAMAGE_DECAY"),
    bit(0x00008000u, "Object trail", "Create an object trail.", "SITH_WF_OBJECT_TRAIL"),
    bit(0x00010000u, "Unknown 0x00010000", "Meaning is not named by OpenJKDF2.", "SITH_WF_10000"),
    bit(0x00020000u, "Unknown 0x00020000", "Meaning is not named by OpenJKDF2.", "SITH_WF_20000"),
    bit(0x00040000u, "Decay emits sound awareness", "Decay emits an AI sound-awareness event.", "SITH_WF_DECAYEMITSOUNDAWARENESSEVENT"),
    bit(0x00080000u, "Surface ricochet", "Ricochet from surfaces.", "SITH_WF_SURFACERICOCHET"),
    bit(0x00100000u, "Unknown 0x00100000", "Meaning is not named by OpenJKDF2.", "SITH_WF_100000"),
    bit(0x00200000u, "Emit AI targeted event", "Emit an AI-targeted event.", "SITH_WF_EMITAITARGETEDEVENT"),
    bit(0x00400000u, "Actor kill destroys", "Destroy weapon when it kills an Actor.", "SITH_WF_ACTORKILLDESTROY"),
    bit(0x00800000u, "Mophia bomb", "Source marks this as Jones-specific.", "SITH_WF_MOPHIABOMB"),
});

constexpr auto kWeaponIjim = std::to_array<FlagDefinition>({
    bit(0x00000001u, "No shooter damage", "Do not damage the shooter.", "SITH_WF_NOSHOOTERDAMAGE"),
    bit(0x00000004u, "Explode on surface hit", "Explode when hitting a surface.", "SITH_WF_FACEHITEXPLODE"),
    bit(0x00000008u, "Explode on Thing hit", "Explode when hitting a Thing.", "SITH_WF_THINGHITEXPLODE"),
    bit(0x00000080u, "Stick to surface", "Stick the projectile to a surface.", "SITH_WF_ATTACHFACE"),
    bit(0x00000100u, "Explode timer set", "Weapon explosion timer is set.", "SITH_WF_EXPLODE", true),
    bit(0x00000200u, "Damage destroys", "Damage destroys the weapon Thing.", "SITH_WF_DAMAGEDESTROY"),
    bit(0x00000400u, "Impact sound", "Produce an impact sound effect.", "SITH_WF_IMPACTSOUND"),
    bit(0x00000800u, "Stick to Thing", "Stick the projectile to another Thing.", "SITH_WF_ATTACHTHING"),
    bit(0x00001000u, "Proximity trigger", "Use a close-proximity trigger.", "SITH_WF_PROXIMITY"),
    bit(0x00002000u, "Instant impact", "Use instant-impact behavior.", "SITH_WF_INSTANTIMPACT"),
    bit(0x00004000u, "Damage decay", "Weapon damage decays over time.", "SITH_WF_DAMAGEDECAY"),
    bit(0x00008000u, "Object trail", "Weapon Thing leaves a trail.", "SITH_WF_OBJECTTRAIL"),
    bit(0x00020000u, "AI target no dodge", "Targeted AI actors cannot dodge the weapon.", "SITH_WF_AITARGETNODODGE"),
    bit(0x00040000u, "Decay emits sound awareness", "Decay emits an AI sound-awareness event.", "SITH_WF_DECAYEMITSOUNDAWARENESSEVENT"),
    bit(0x00080000u, "Surface ricochet", "Weapon ricochets off surfaces.", "SITH_WF_SURFACERICOCHET"),
    bit(0x00200000u, "Emit AI targeted event", "Emit an AI-targeted event.", "SITH_WF_EMITAITARGETEDEVENT"),
    bit(0x00400000u, "Destroy after actor kill", "Destroy the weapon when it kills an AI actor; the default behavior is to explode.", "SITH_WF_BLOODSPLATTER"),
    bit(0x00800000u, "Marduk Mophia bomb", "Weapon is a Marduk Mophia bomb.", "SITH_WF_MOPHIABOMB"),
});

constexpr auto kItemJk = std::to_array<FlagDefinition>({
    bit(0x00000001u, "Respawn in multiplayer", "Item respawns in multiplayer.", "SITH_ITEM_RESPAWN_MP"),
    bit(0x00000002u, "Respawn in single-player", "Item respawns in single-player.", "SITH_ITEM_RESPAWN_SP"),
    bit(0x00000004u, "Backpack", "Item is a backpack.", "SITH_ITEM_BACKPACK"),
});

constexpr auto kItemMots = std::to_array<FlagDefinition>({
    bit(0x00000001u, "Respawn in multiplayer", "Item respawns in multiplayer.", "SITH_ITEM_RESPAWN_MP"),
    bit(0x00000002u, "Respawn in single-player", "Item respawns in single-player.", "SITH_ITEM_RESPAWN_SP"),
    bit(0x00000004u, "Backpack", "Item is a backpack.", "SITH_ITEM_BACKPACK"),
    bit(0x00000008u, "Unknown 0x00000008", "Meaning is not named by OpenJKDF2.", "SITH_ITEM_8"),
    bit(0x00000010u, "Weapon-collidable", "MotS weapon collision accepts Items carrying this bit.", "SITH_ITEM_10"),
});

constexpr auto kExplosionClassic = std::to_array<FlagDefinition>({
    bit(0x00000001u, "Animated sprite", "Animate the explosion sprite.", "SITHEXPLOSION_FLAG_ANIMATED_SPRITE"),
    bit(0x00000002u, "Blast phase", "Explosion has a blast phase.", "SITHEXPLOSION_FLAG_HAS_BLAST_PHASE"),
    bit(0x00000004u, "Blast-radius damage", "Damage within blast radius.", "SITHEXPLOSION_FLAG_DAMAGE_IN_BLAST_RADIUS"),
    bit(0x00000008u, "Child explosion", "Explosion has a child explosion.", "SITHEXPLOSION_FLAG_HAS_CHILD_EXPLOSION"),
    bit(0x00000010u, "Variable light", "Explosion uses variable light.", "SITHEXPLOSION_FLAG_VARIABLE_LIGHT"),
    bit(0x00000040u, "No shooter damage", "Do not damage the shooter.", "SITHEXPLOSION_FLAG_NO_DAMAGE_TO_SHOOTER"),
    bit(0x00000080u, "Random debris", "Select debris randomly.", "SITHEXPLOSION_FLAG_RANDOM_DEBRIS"),
    bit(0x00000100u, "Flash blinds Things", "Explosion flash blinds Things.", "SITHEXPLOSION_FLAG_FLASH_BLINDS_THINGS"),
    bit(0x00000200u, "Animate debris material", "Source notes this may be Jones-specific.", "SITHEXPLOSION_FLAG_ANIMATE_DEBRIS_MATERIAL"),
    bit(0x00000400u, "Update debris material", "Source notes this may be Jones-specific.", "SITHEXPLOSION_FLAG_UPDATE_DEBRIS_MATERIAL"),
    bit(0x00000800u, "Use expand time", "Use explosion expansion time.", "SITHEXPLOSION_USE_EXPAND_TIME"),
    bit(0x00001000u, "Use fade time", "Use explosion fade time.", "SITHEXPLOSION_FLAG_USE_FADE_TIME"),
});

constexpr auto kExplosionIjim = std::to_array<FlagDefinition>({
    bit(0x00000001u, "Animate sprite", "Animate the explosion sprite.", "SITH_EF_ANIMATESPRITE"),
    bit(0x00000002u, "Blast phase", "Explosion has a blast phase.", "SITH_EF_BLASTPHASE"),
    bit(0x00000004u, "Blast damage", "Damage within blast radius.", "SITH_EF_BLASTDAMAGE"),
    bit(0x00000008u, "Child explosion", "Explosion has a child explosion.", "SITH_EF_CHILDEXPLOSION"),
    bit(0x00000010u, "Variable light", "Explosion uses variable light.", "SITH_EF_VARIABLELIGHT"),
    bit(0x00000020u, "Unknown 0x00000020", "Unknown explosion behavior.", "SITH_EF_RANDOMROLL"),
    bit(0x00000040u, "No shooter damage", "Do not damage the shooter.", "SITH_EF_NOSHOOTERDAMAGE"),
    bit(0x00000080u, "Random debris", "Select debris randomly.", "SITH_EF_RANDOMDEBRIS"),
    bit(0x00000100u, "Blind Things", "Explosion flash blinds Things.", "SITH_EF_BLINDPLAYER"),
    bit(0x00000200u, "Animate debris texture", "Animate the debris texture.", "SITH_EF_ANIMATEDEBRISMATERIAL"),
    bit(0x00000400u, "Use hit-surface texture", "Set debris texture from the hit surface.", "SITH_EF_UPDATEDEBRISMATERIAL"),
    bit(0x00000800u, "Expand time", "Use the expand-time effect.", "SITH_EF_EXPAND"),
    bit(0x00001000u, "Fade time", "Use the fade-time effect.", "SITH_EF_FADE"),
});

constexpr auto kParticleClassic = std::to_array<FlagDefinition>({
    bit(0x00000001u, "Outward expanding", "Particles expand outward.", "SITHPARTICLE_FLAG_OUTWARD_EXPANDING"),
    bit(0x00000002u, "Animate cel", "Animate material cels.", "SITHPARTICLE_FLAG_ANIMATE_CEL"),
    bit(0x00000004u, "Random start cel", "Choose a random initial cel.", "SITHPARTICLE_FLAG_RANDOM_START_CEL"),
    bit(0x00000008u, "Fade over time", "Fade particles over time.", "SITHPARTICLE_FLAG_FADE_OUT_OVER_TIME"),
    bit(0x00000010u, "Emit light", "Particles emit light.", "SITHPARTICLE_FLAG_EMIT_LIGHT"),
    bit(0x00000020u, "Random cel change", "Change cels randomly.", "SITHPARTICLE_FLAG_RANDOM_CEL_CHANGE"),
    bit(0x00000040u, "Timeout rate", "Use timeout-rate particle growth.", "SITHPARTICLE_FLAG_TIMEOUT_RATE"),
});

constexpr auto kParticleIjim = std::to_array<FlagDefinition>({
    bit(0x00000001u, "Outward expanding", "Particles expand outward.", "SITHPARTICLE_FLAG_OUTWARD_EXPANDING"),
    bit(0x00000002u, "Unused", "Unused particle flag.", "SITHPARTICLE_FLAG_UNUSED_2"),
    bit(0x00000004u, "Animate texture cels", "Animate the particle texture cels.", "SITHPARTICLE_FLAG_ANIMATE_CEL"),
    bit(0x00000008u, "Fade over time", "Fade particles over time.", "SITHPARTICLE_FLAG_FADE_OUT_OVER_TIME"),
    bit(0x00000010u, "Emit light", "Particles emit light.", "SITHPARTICLE_FLAG_EMIT_LIGHT"),
    bit(0x00000020u, "Random cel change", "Randomly change animated texture cels.", "SITHPARTICLE_FLAG_RANDOM_CEL_CHANGE"),
    bit(0x00000040u, "Timeout rate", "Use the timeout rate.", "SITHPARTICLE_FLAG_TIMEOUT_RATE"),
});

constexpr bool asciiIequals(const std::string_view lhs, const std::string_view rhs) noexcept
{
    if (lhs.size() != rhs.size())
    {
        return false;
    }

    for (std::size_t i = 0; i < lhs.size(); ++i)
    {
        const auto lower = [](const char ch) constexpr -> char
        {
            return ch >= 'A' && ch <= 'Z' ? static_cast<char>(ch - 'A' + 'a') : ch;
        };
        if (lower(lhs[i]) != lower(rhs[i]))
        {
            return false;
        }
    }
    return true;
}

constexpr std::uint8_t projectMask(const ProjectType project) noexcept
{
    switch (project)
    {
        case ProjectType::JKDF2: return kJkdf2;
        case ProjectType::MOTS:  return kMots;
        case ProjectType::IJIM:  return kIjim;
    }
    return 0u;
}

template <std::size_t N>
std::vector<FlagDefinition> copyFlags(const std::array<FlagDefinition, N>& source)
{
    return { source.begin(), source.end() };
}

} // namespace

std::vector<ParameterDefinition> parameterDefinitions(
    const ProjectType project,
    const bool includeRecognizedNoEffect)
{
    const std::uint8_t mask = projectMask(project);
    std::vector<ParameterDefinition> result;
    result.reserve(kParameterEntries.size());

    for (const ParameterEntry& entry : kParameterEntries)
    {
        if ((entry.projects & mask) == 0u)
        {
            continue;
        }
        if (!entry.definition.runtimeEffective && !includeRecognizedNoEffect)
        {
            continue;
        }
        result.push_back(entry.definition);
    }

    std::sort(result.begin(), result.end(), [](const ParameterDefinition& lhs, const ParameterDefinition& rhs)
        {
            return lhs.name < rhs.name;
        });
    return result;
}

std::vector<std::string_view> completionNames(const ProjectType project)
{
    const std::vector<ParameterDefinition> definitions = parameterDefinitions(project);
    std::vector<std::string_view> result;
    result.reserve(definitions.size());
    for (const ParameterDefinition& definition : definitions)
    {
        result.push_back(definition.name);
    }
    return result;
}

bool isRepeatableParameter(
    const ProjectType project,
    const std::string_view parameterName) noexcept
{
    const std::uint8_t mask = projectMask(project);
    const auto it = std::find_if(
        kParameterEntries.begin(),
        kParameterEntries.end(),
        [mask, parameterName](const ParameterEntry& entry)
        {
            return (entry.projects & mask) != 0u
                && entry.definition.runtimeEffective
                && asciiIequals(entry.definition.name, parameterName);
        });
    return it != kParameterEntries.end() && it->definition.repeatable;
}

std::optional<std::size_t> repeatableParameterLimit(
    const ProjectType project,
    const std::string_view parameterName) noexcept
{
    if (!isRepeatableParameter(project, parameterName))
    {
        return std::nullopt;
    }
    if (asciiIequals(parameterName, "debris"))
    {
        return project == ProjectType::IJIM ? std::size_t{ 16 } : std::size_t{ 4 };
    }
    return std::nullopt;
}

std::vector<EnumChoice> enumChoices(
    const ProjectType project,
    const std::string_view parameterName)
{
    if (asciiIequals(parameterName, "move"))
    {
        return {
            { "none", "None", "No movement controller." },
            { "physics", "Physics", "Physics movement controller." },
            { "path", "Path", "Keyframed path movement controller." },
        };
    }

    if (asciiIequals(parameterName, "collide"))
    {
        std::vector<EnumChoice> result{
            { "0", "None", "No collision shape." },
            { "1", "Sphere", "Sphere collision." },
        };
        if (project == ProjectType::MOTS)
        {
            result.push_back({ "2", "Sphere tree", "MotS hierarchical sphere collision." });
        }
        result.push_back({ "3", "Face", "Face/mesh collision." });
        return result;
    }

    if (asciiIequals(parameterName, "type"))
    {
        std::vector<EnumChoice> result{
            { "free", "Free", "Unused/free Thing slot." },
            { "camera", "Camera", "Camera Thing." },
            { "actor", "Actor", "AI Actor Thing." },
            { "weapon", "Weapon", "Weapon/projectile Thing." },
            { "debris", "Debris", "Debris Thing." },
            { "item", "Item", "Inventory/pickup Item Thing." },
            { "explosion", "Explosion", "Explosion Thing." },
            { "cog", "COG", "COG helper Thing." },
            { "ghost", "Ghost", "Ghost Thing." },
            { "corpse", "Corpse", "Corpse Thing." },
            { "player", "Player", "Player Actor Thing." },
            { "particle", "Particle", "Particle-cloud Thing." },
        };
        if (project == ProjectType::IJIM)
        {
            result.push_back({ "hint", "Hint", "IJIM AI hint Thing." });
            result.push_back({ "sprite", "Sprite", "IJIM Sprite Thing." });
            result.push_back({ "polyline", "Polyline", "IJIM Polyline Thing." });
        }
        return result;
    }

    return {};
}

std::optional<FlagFamily> flagFamily(
    const std::string_view parameterName,
    const std::string_view effectiveThingType) noexcept
{
    if (asciiIequals(parameterName, "thingflags"))
    {
        return FlagFamily::thing;
    }
    if (asciiIequals(parameterName, "physflags"))
    {
        return FlagFamily::physics;
    }
    if (asciiIequals(parameterName, "damageclass"))
    {
        return FlagFamily::damage;
    }
    if (!asciiIequals(parameterName, "typeflags"))
    {
        return std::nullopt;
    }

    if (asciiIequals(effectiveThingType, "actor") || asciiIequals(effectiveThingType, "player"))
    {
        return FlagFamily::actor;
    }
    if (asciiIequals(effectiveThingType, "weapon"))
    {
        return FlagFamily::weapon;
    }
    if (asciiIequals(effectiveThingType, "item"))
    {
        return FlagFamily::item;
    }
    if (asciiIequals(effectiveThingType, "explosion"))
    {
        return FlagFamily::explosion;
    }
    if (asciiIequals(effectiveThingType, "particle"))
    {
        return FlagFamily::particle;
    }
    return std::nullopt;
}

std::string_view flagFamilyName(const FlagFamily family) noexcept
{
    switch (family)
    {
        case FlagFamily::thing:     return "Thing";
        case FlagFamily::physics:   return "Physics";
        case FlagFamily::damage:    return "Damage class";
        case FlagFamily::actor:     return "Actor";
        case FlagFamily::weapon:    return "Weapon";
        case FlagFamily::item:      return "Item";
        case FlagFamily::explosion: return "Explosion";
        case FlagFamily::particle:  return "Particle";
    }
    return "Flags";
}

std::vector<FlagDefinition> flagDefinitions(
    const ProjectType project,
    const FlagFamily family)
{
    switch (family)
    {
        case FlagFamily::thing:
            return project == ProjectType::IJIM ? copyFlags(kThingIjim) : copyFlags(kThingClassic);

        case FlagFamily::physics:
            return project == ProjectType::IJIM ? copyFlags(kPhysicsIjim) : copyFlags(kPhysicsClassic);

        case FlagFamily::damage:
            return project == ProjectType::IJIM ? copyFlags(kDamageIjim) : copyFlags(kDamageClassic);

        case FlagFamily::actor:
        {
            if (project == ProjectType::IJIM)
            {
                return copyFlags(kActorIjim);
            }
            std::vector<FlagDefinition> result = copyFlags(kActorClassic);
            if (project == ProjectType::JKDF2)
            {
                // OpenJKDF2 explicitly gates these high-bit meanings to MotS.
                result.erase(
                    std::remove_if(result.begin(), result.end(), [](const FlagDefinition& definition)
                        {
                            return definition.mask >= 0x08000000u;
                        }),
                    result.end());
            }
            return result;
        }

        case FlagFamily::weapon:
            return project == ProjectType::IJIM ? copyFlags(kWeaponIjim) : copyFlags(kWeaponClassic);

        case FlagFamily::item:
            return project == ProjectType::MOTS ? copyFlags(kItemMots) : copyFlags(kItemJk);

        case FlagFamily::explosion:
            return project == ProjectType::IJIM ? copyFlags(kExplosionIjim) : copyFlags(kExplosionClassic);

        case FlagFamily::particle:
            return project == ProjectType::IJIM ? copyFlags(kParticleIjim) : copyFlags(kParticleClassic);
    }
    return {};
}

std::vector<FlagDefinition> flagDefinitions(
    const ProjectType project,
    const std::string_view parameterName,
    const std::string_view effectiveThingType)
{
    const std::optional<FlagFamily> family = flagFamily(parameterName, effectiveThingType);
    return family ? flagDefinitions(project, *family) : std::vector<FlagDefinition>{};
}

std::uint32_t knownSingleBitMask(const std::span<const FlagDefinition> definitions) noexcept
{
    std::uint32_t result = 0u;
    for (const FlagDefinition& definition : definitions)
    {
        if (definition.kind == FlagKind::singleBit)
        {
            result |= definition.mask;
        }
    }
    return result;
}

std::uint32_t setFlagMask(
    const std::uint32_t original,
    const std::uint32_t mask,
    const bool enabled) noexcept
{
    return enabled ? original | mask : original & ~mask;
}

std::uint32_t mergeKnownSingleBitFlags(
    const std::uint32_t original,
    const std::span<const FlagDefinition> definitions,
    const std::uint32_t selectedKnownBits) noexcept
{
    const std::uint32_t known = knownSingleBitMask(definitions);
    return (original & ~known) | (selectedKnownBits & known);
}

} // namespace sed::thing::parameter_schema
