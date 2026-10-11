#pragma once

#include <optional>
#include <vector>

#include <QString>

namespace sed::ui::cog
{

/** Line ranges for top-level COG script sections, using zero-based lines. */
struct CogScriptSectionsInfo
{
    int flagsLine{-1};
    int symbolsLine{-1};
    int symbolsEndLine{-1};
    int codeLine{-1};
    int codeEndLine{-1};
};

/** Cursor endpoints mapped through one old-text to new-text replacement. */
struct CogScriptTextEditSelection
{
    int anchor{};
    int position{};
};

/**
 * Maps a selection through the smallest changed span between two texts.
 *
 * Endpoints before the span remain fixed, endpoints after it move by its
 * length delta, and endpoints inside it clamp into the replacement span.
 * Anchor/position order is retained so reversed selections stay reversed.
 */
CogScriptTextEditSelection mapCogScriptTextEditSelection(
    const QString& oldText,
    const QString& newText,
    int anchor,
    int position);

/** Logical insertion context at a target COG script line. */
enum class CogScriptInsertContext
{
    outside,
    flags,
    symbols,
    code,
};

/** Scans the script for flags, symbols, code, and their matching end lines. */
CogScriptSectionsInfo scanCogScriptSections(const QString& text);

/** Classifies a line as outside the structured script sections, flags, symbols, or code. */
CogScriptInsertContext cogScriptInsertContextAtLine(const QString& text, int targetLine);

/**
 * Builds the keyboard Insert context for each source line.
 *
 * Complete section markers and the shared string/comment sanitizer determine
 * structure. Symbols lines accept declarations; code accepts handlers only
 * before the first label or after its final unconditional return. Conditional
 * and nested returns, unfinished handlers and malformed code keep native Insert.
 */
std::vector<CogScriptInsertContext> buildCogScriptShortcutInsertContexts(const QString& text);

/** Returns the best line for inserting a regular symbol declaration, or -1 when unavailable. */
int preferredCogScriptSymbolInsertLine(const QString& text, std::optional<int> lineHint = std::nullopt);

/** Returns the best line for inserting a message declaration, keeping messages grouped together. */
int preferredCogScriptMessageSymbolInsertLine(const QString& text, std::optional<int> lineHint = std::nullopt);

/** Returns the best line for inserting a code handler/label, or -1 when unavailable. */
int preferredCogScriptCodeInsertLine(const QString& text, std::optional<int> lineHint = std::nullopt);

/** Returns the best line for inserting a missing top-level section, or -1 when already present. */
int preferredCogScriptSectionInsertLine(
    const QString& text,
    const QString& sectionName,
    std::optional<int> lineHint = std::nullopt);

} // namespace sed::ui::cog
