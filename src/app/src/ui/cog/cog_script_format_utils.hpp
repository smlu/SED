#pragma once

#include <cstdint>
#include <optional>

#include <QString>

namespace sed::ui::cog
{

enum class BraceStyle
{
    sameLine,
    nextLine,
};

/**
 * Formatting policy for COG source.
 *
 * The first five fields are the original editor formatter settings.  The
 * remaining switches deliberately describe lexical whitespace only; they do
 * not rewrite strings, comments, or authored identifiers.
 */
struct CogScriptFormatOptions
{
    bool useTabs{false};
    int indentWidth{4};
    BraceStyle braceStyle{BraceStyle::sameLine};
    bool indentSectionBodies{true};
    bool indentCodeBodies{true};
    bool keywordSpacing{true};
    bool insideParenthesesSpacing{false};
    // Function invocation parentheses are intentionally independent from
    // control/grouping parentheses.  COG authors commonly prefer
    // `if ( x )` together with compact calls such as `Print(x)`.
    bool functionParenthesesSpacing{false};
    bool forSemicolonSpacing{true};
    bool alignFinalReturnLabel{false};
    bool trimExcessBlankLines{true};
    // Standalone comments retain their authored indentation unless the user
    // explicitly opts into aligning them with the surrounding COG scope.
    bool indentComments{false};
    // Full editor formatting canonicalizes function-call parentheses even
    // when compact spacing is selected. The legacy indentation-only overload
    // disables this so its original intra-line preservation contract holds.
    bool canonicalizeFunctionParentheses{true};
    // The same full-format contract applies to control/grouping parentheses.
    // Keeping a separate compatibility flag lets the legacy overload retain
    // authored intra-line whitespace exactly when its lexical options are off.
    bool canonicalizeNonCallParentheses{true};
};

std::optional<std::uint32_t> parseMaskValue(const QString& text);
QString formatMaskHex(std::uint32_t mask);

QString flagsLineSnippet();
QString sectionSnippet(const QString& sectionName);
QString symbolSnippet(const QString& typeName);
QString handlerSnippet(const QString& label);
QString skeletonSnippet();

QString indentString(int level, bool useTabs, int indentWidth);
QString braceStyleId(BraceStyle style);
BraceStyle braceStyleFromId(const QString& id);

/// Formats a COG script using the editor's indentation and brace-style rules.
QString formatCogScriptText(
    const QString& text,
    bool useTabs,
    int indentWidth,
    BraceStyle braceStyle,
    bool indentSectionBodies,
    bool indentCodeBodies);

/// Formats COG source using the complete persisted formatter policy.
QString formatCogScriptText(
    const QString& text,
    const CogScriptFormatOptions& options);

} // namespace sed::ui::cog
