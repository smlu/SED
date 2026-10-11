#pragma once

#include <optional>
#include <vector>

#include <QString>
#include <QTextDocument>

namespace sed::ui::cog
{

struct CogScriptTextMatch
{
    int position{};
    int length{};

    friend bool operator==(const CogScriptTextMatch&, const CogScriptTextMatch&) = default;
};

/**
 * Enumerates literal editor Find matches without moving a live editor cursor.
 *
 * The implementation deliberately uses QTextDocument::find so Match case and
 * Whole word have exactly the same Unicode/boundary semantics as navigation.
 */
std::vector<CogScriptTextMatch> findCogScriptTextMatches(
    const QTextDocument& document,
    const QString& needle,
    bool caseSensitive,
    bool wholeWords);

/// Finds either a symbol declaration or a code label matching `identifier`.
std::optional<int> findCogScriptDefinitionLine(const QString& text, const QString& identifier);

/// Finds a symbol declaration matching `identifier`.
std::optional<int> findCogScriptSymbolLine(const QString& text, const QString& identifier);

/// Finds a code label matching `identifier`.
std::optional<int> findCogScriptCodeLabelLine(const QString& text, const QString& identifier);

} // namespace sed::ui::cog
