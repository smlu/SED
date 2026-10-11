#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

#include "sed/level.hpp"

namespace sed::thing::parameter_schema
{

/** Runtime subsystem which consumes a Thing/template parameter. */
enum class Scope : std::uint16_t
{
    none      = 0,
    common    = 1u << 0,
    physics   = 1u << 1,
    path      = 1u << 2,
    ai        = 1u << 3,
    actor     = 1u << 4,
    weapon    = 1u << 5,
    item      = 1u << 6,
    explosion = 1u << 7,
    particle  = 1u << 8,
};

constexpr Scope operator|(const Scope lhs, const Scope rhs) noexcept
{
    return static_cast<Scope>(
        static_cast<std::uint16_t>(lhs) | static_cast<std::uint16_t>(rhs));
}

constexpr bool hasScope(const Scope value, const Scope test) noexcept
{
    return (static_cast<std::uint16_t>(value) & static_cast<std::uint16_t>(test)) != 0u;
}

/** Name metadata used by both Thing-instance and template-editor completion. */
struct ParameterDefinition
{
    std::string_view name{};
    std::string_view description{};
    Scope scopes{ Scope::none };
    bool repeatable{ false };
    bool runtimeEffective{ true };
};

/** Exact stored value plus a user-facing label for small enum quick-pickers. */
struct EnumChoice
{
    std::string_view value{};
    std::string_view name{};
    std::string_view description{};
};

/** The bitfield selected by a parameter name and, for typeflags, Thing type. */
enum class FlagFamily : std::uint8_t
{
    thing,
    physics,
    damage,
    actor,
    weapon,
    item,
    explosion,
    particle,
};

enum class FlagKind : std::uint8_t
{
    singleBit,
    compositePreset,
};

/** A source-backed flag definition. Unknown bits are intentionally not lost. */
struct FlagDefinition
{
    std::uint32_t mask{};
    std::string_view name{};
    std::string_view description{};
    std::string_view sourceSymbol{};
    FlagKind kind{ FlagKind::singleBit };
    bool runtimeManaged{ false };
};

/**
 * Returns project-appropriate parameter metadata, sorted by runtime name.
 *
 * By default this excludes names which occur in the engine hash table but have
 * no consuming parser (`attach`, and classic `points`). Passing true exposes
 * them for diagnostics without presenting them as normal completion choices.
 */
std::vector<ParameterDefinition> parameterDefinitions(
    ProjectType project,
    bool includeRecognizedNoEffect = false);

/** Lowercase, sorted and unique runtime-effective completion names. */
std::vector<std::string_view> completionNames(ProjectType project);

/** Whether repeated tokens with this name are meaningful to the runtime. */
bool isRepeatableParameter(ProjectType project, std::string_view parameterName) noexcept;

/** Fixed runtime capacity, or nullopt when capacity is data-defined/unbounded. */
std::optional<std::size_t> repeatableParameterLimit(
    ProjectType project,
    std::string_view parameterName) noexcept;

/** Choices for `type`, `move`, or `collide`; other names return an empty list. */
std::vector<EnumChoice> enumChoices(ProjectType project, std::string_view parameterName);

/**
 * Resolves a flag family. `typeflags` requires the effective inherited Thing
 * type; player uses Actor flags. Types without a type-specific parser (for
 * example sprite, debris, ghost, and cog) return nullopt.
 */
std::optional<FlagFamily> flagFamily(
    std::string_view parameterName,
    std::string_view effectiveThingType = {}) noexcept;

/** Stable family label suitable for contextual picker titles. */
std::string_view flagFamilyName(FlagFamily family) noexcept;

/** Project-aware named definitions for a resolved flag family. */
std::vector<FlagDefinition> flagDefinitions(ProjectType project, FlagFamily family);

/** Convenience overload combining flagFamily() and flagDefinitions(). */
std::vector<FlagDefinition> flagDefinitions(
    ProjectType project,
    std::string_view parameterName,
    std::string_view effectiveThingType = {});

/** Mask of independent rows; composite presets are excluded. */
std::uint32_t knownSingleBitMask(std::span<const FlagDefinition> definitions) noexcept;

/** Toggle one definition while preserving every unrelated/unknown bit. */
std::uint32_t setFlagMask(
    std::uint32_t original,
    std::uint32_t mask,
    bool enabled) noexcept;

/**
 * Replaces only independently defined bits with a UI selection and preserves
 * all bits unknown to the current project/family schema.
 */
std::uint32_t mergeKnownSingleBitFlags(
    std::uint32_t original,
    std::span<const FlagDefinition> definitions,
    std::uint32_t selectedKnownBits) noexcept;

} // namespace sed::thing::parameter_schema
