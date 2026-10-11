#pragma once

#include <optional>

#include <QString>

namespace sed::ui::cog
{

/** Parsed COG symbols-section line with its script line number. */
struct CogScriptSymbolLineContext
{
    int line{-1};
    QString typeName{};
    QString identifier{};
};

/** Location and value details for one known COG symbol-line attribute. */
struct CogScriptSymbolLineAttributeMatch
{
    QString name{};
    QString value{};
    int removeStart{-1};
    int removeLength{0};
    int replaceStart{-1};
    int replaceLength{0};
    int tokenStart{-1};
    int tokenLength{0};
    int valueStart{-1};
    int valueLength{0};
};

/** Parses a symbol declaration at a zero-based line, if the line is in the symbols section. */
std::optional<CogScriptSymbolLineContext> cogScriptSymbolLineContextAtLine(const QString& text, int targetLine);

/** Finds a named attribute on a symbol line, optionally requiring the attribute token to cover column. */
std::optional<CogScriptSymbolLineAttributeMatch> cogScriptSymbolLineAttributeMatch(
    const QString& text,
    const CogScriptSymbolLineContext& context,
    const QString& attributeName,
    std::optional<int> column = std::nullopt);

/** Returns the value for a named symbol-line attribute, or an empty value for valueless attributes. */
std::optional<QString> cogScriptSymbolLineAttributeValue(
    const QString& text,
    const CogScriptSymbolLineContext& context,
    const QString& attributeName,
    std::optional<int> column = std::nullopt);

/** Returns true when a symbol line contains a named attribute. */
bool cogScriptSymbolLineHasAttribute(
    const QString& text,
    const CogScriptSymbolLineContext& context,
    const QString& attributeName);

/** Counts repeated occurrences of a named attribute on a symbol line. */
int cogScriptSymbolLineAttributeCount(
    const QString& text,
    const CogScriptSymbolLineContext& context,
    const QString& attributeName);

/** Finds any known COG symbol-line attribute at the given column. */
std::optional<CogScriptSymbolLineAttributeMatch> cogScriptSymbolLineAttributeAtColumn(
    const QString& text,
    const CogScriptSymbolLineContext& context,
    int column);

/** Returns true when the column is inside the parsed value portion of an attribute. */
bool cogScriptSymbolLineAttributeValueHit(const CogScriptSymbolLineAttributeMatch& match, int column);

/** Returns a script with one matching symbol-line attribute removed, or nullopt when no edit applies. */
std::optional<QString> removeCogScriptSymbolLineAttribute(
    const QString& text,
    const CogScriptSymbolLineContext& context,
    const QString& attributeName,
    std::optional<int> column = std::nullopt);

/** Returns a script with duplicate attributes collapsed to one kept occurrence, or nullopt when no edit applies. */
std::optional<QString> dedupeCogScriptSymbolLineAttribute(
    const QString& text,
    const CogScriptSymbolLineContext& context,
    const QString& attributeName,
    std::optional<int> keepColumn = std::nullopt);

/** Returns a script with a symbol-line attribute inserted or replaced, or nullopt when no edit applies. */
std::optional<QString> upsertCogScriptSymbolLineAttribute(
    const QString& text,
    const CogScriptSymbolLineContext& context,
    const QString& attributeName,
    const std::optional<QString>& attributeValue,
    std::optional<int> column = std::nullopt);

} // namespace sed::ui::cog
