#pragma once

#include <cstdint>
#include <vector>

#include <QList>
#include <QString>
#include <QStringList>
#include <QStringView>

#include "sed/level.hpp"

namespace sed::ui::cog
{

/// Thing-type entry used by the COG `mask` attribute editor.
struct CogMaskThingTypeOption
{
    QString label{};
    int typeNumber{};
};

/// One engine-defined bit accepted by the optional COG `flags=` line.
struct CogScriptFlagOption
{
    std::uint32_t bit{};
    QString label{};
    QString description{};
    /// Runtime-managed bits are valid but normally should not be authored.
    bool runtimeManaged{};

    friend bool operator==(const CogScriptFlagOption&, const CogScriptFlagOption&) = default;
};

/// Classification returned while parsing the hexadecimal value from `flags=`.
enum class CogScriptFlagsValueParseState
{
    missing,
    invalid,
    outOfRange,
    ok,
};

struct CogScriptFlagsValueParseResult
{
    CogScriptFlagsValueParseState state{CogScriptFlagsValueParseState::missing};
    std::uint32_t value{};

    friend bool operator==(const CogScriptFlagsValueParseResult&, const CogScriptFlagsValueParseResult&) = default;
};

/// One case-insensitive COG identifier occurrence in source text.
struct CogScriptIdentifierOccurrence
{
    int position{};
    int length{};
};

/// Returns section names understood by classic COG scripts.
const QStringList& cogScriptSectionWords();

/// Returns control-flow and script-language keywords for completion/highlighting.
const QStringList& cogScriptKeywordWords();

/// Returns symbol declaration types accepted by the COG editor.
const QStringList& cogScriptTypeWords();

/// Returns symbol types exposed by the insert-symbol menu.
const QStringList& cogScriptInsertTypeWords();

/// Returns symbol attributes accepted by the COG editor.
const QStringList& cogScriptAttributeWords();

/// Returns the broad message vocabulary used when the project type is unknown.
const QStringList& cogScriptMessageWords();

/// Returns the project-specific message vocabulary for completion and validation.
QStringList cogScriptProjectMessageWords(ProjectType kind);

/// Returns `global0` through `global15`.
const QStringList& cogScriptGlobalWords();

/// Returns whether a numeric symbol type may back a named code label.
bool cogScriptTypeSupportsCodeLabel(const QString& typeName);

/// Builds a sorted, de-duplicated completion list from built-ins plus identifiers in `text`.
QStringList buildCogScriptCompletionWords(const QString& text, const QStringList& messageWords);

/// Builds a sorted list of valid call targets from code labels and flex/float symbols.
QStringList buildCogScriptCallableWords(const QString& text);

/**
 * Finds semantic occurrences of one complete COG identifier.
 *
 * COG identifiers are case-insensitive. In the symbols section, only the
 * declared variable name is a semantic occurrence; the type, initializer,
 * and metadata are parsed as literal configuration. In code sections,
 * identifier tokens remain navigable references. Quoted strings and line
 * comments are excluded from both sections.
 */
std::vector<CogScriptIdentifierOccurrence> findCogScriptIdentifierOccurrences(
    const QString& text,
    QStringView identifier);

/// Returns thing types that can participate in `mask` trigger filtering.
QList<CogMaskThingTypeOption> cogScriptMaskThingTypeOptions(ProjectType kind);

/// Returns the complete supported bitset for the current project's `mask` editor.
std::uint32_t cogScriptSupportedMaskBits(ProjectType kind);

/**
 * Returns the engine-defined COG script flags for a project.
 *
 * The current classic and IJIM engines share this vocabulary. The project
 * parameter is retained at the model boundary so a verified engine-specific
 * distinction can be added without changing editor callers.
 */
QList<CogScriptFlagOption> cogScriptFlagOptions(ProjectType kind);

/// Returns the complete known bitset for the current project's COG flags editor.
std::uint32_t cogScriptSupportedFlagBits(ProjectType kind);

/**
 * Parses the value token from a COG `flags=` line.
 *
 * Engine syntax is hexadecimal even when the optional `0x` prefix is absent:
 * `flags=10` therefore has value `0x10`, not decimal ten.
 */
CogScriptFlagsValueParseResult parseCogScriptFlagsValue(QStringView text);

/// Formats a COG flags value in the editor's canonical engine-compatible form.
QString formatCogScriptFlagsValue(std::uint32_t value);

/// Returns context help for a known symbol attribute, or an empty string.
QString cogScriptAttributeHelpText(const QString& attributeName);

/// Returns context help for the script flags line.
QString cogScriptFlagsHelpText();

/// Returns the identifier touching `column`, including the previous token when the column is just after it.
QString cogScriptIdentifierAtColumn(const QString& line, int column);

/// Returns hover help for the token under `column`, or an empty string when no help applies.
QString cogScriptHoverHelpText(const QString& rawLine, int column);

/// Returns whether symbols of `typeName` may use trigger-filter attributes.
bool cogScriptTypeSupportsTriggerFilterAttributes(const QString& typeName);

} // namespace sed::ui::cog
