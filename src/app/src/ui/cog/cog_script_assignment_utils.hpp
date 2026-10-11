#pragma once

#include <optional>

#include <QChar>
#include <QString>
#include <QStringView>

#include "ui/resources/resource_picker.hpp"

namespace sed::ui::cog
{

enum class CogScriptEditorSection
{
    outside,
    symbols,
    code,
};

enum class CogScriptInlineAssignmentKind
{
    resource,
    flags,
    mask,
};

/** A line-local target for the inline COG assignment helper. */
struct CogScriptInlineAssignmentSpan
{
    CogScriptInlineAssignmentKind kind{CogScriptInlineAssignmentKind::resource};
    CogScriptEditorSection section{CogScriptEditorSection::outside};
    int equalsStart{-1};
    int valueStart{-1};
    int valueLength{};
    int symbolNameStart{-1};
    int symbolNameLength{};
    QString typeName{};
    QString symbolName{};
    QString currentValue{};
    QChar quote{};
    std::optional<ResourcePickerKind> resourceKind{};
    bool hasInitializer{true};

    friend bool operator==(const CogScriptInlineAssignmentSpan&, const CogScriptInlineAssignmentSpan&) = default;
};

/** The declared variable name range in one COG symbols line. */
struct CogScriptSymbolDeclarationNameSpan
{
    int start{-1};
    int length{};

    friend bool operator==(const CogScriptSymbolDeclarationNameSpan&, const CogScriptSymbolDeclarationNameSpan&) = default;
};

/** Finds a supported resource initializer or flags value under `column`. */
std::optional<CogScriptInlineAssignmentSpan> cogScriptInlineAssignmentAtColumn(
    const QString& rawLine,
    CogScriptEditorSection section,
    int column);

/** Returns the declaration-name span from a symbols line, excluding metadata. */
std::optional<CogScriptSymbolDeclarationNameSpan> cogScriptSymbolDeclarationNameSpan(
    const QString& rawLine);

/** Returns whether `column` is over the declared symbol name in a symbols line. */
bool cogScriptCursorIsSymbolDeclarationName(
    const QString& rawLine,
    CogScriptEditorSection section,
    int column);

/** Accepts only a single safe resource token, with no quotes or script syntax. */
bool isSafeCogScriptResourceName(QStringView name) noexcept;

/** Revalidates and replaces one resource value while preserving its line text. */
std::optional<QString> replaceCogScriptInlineResourceValue(
    const QString& currentLine,
    const CogScriptInlineAssignmentSpan& expected,
    const QString& selectedName);

} // namespace sed::ui::cog
