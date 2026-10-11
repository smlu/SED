#pragma once

#include <vector>

#include <QString>

namespace sed::ui::cog
{

enum class CogScriptOutlineGroup
{
    symbols,
    code,
};

/// One row in the COG script editor outline panel.
struct CogScriptOutlineEntry
{
    QString label{};
    int line{-1};
    bool section{false};
    CogScriptOutlineGroup group{CogScriptOutlineGroup::symbols};
};

/// Builds the COG script editor outline from flags, section headers, symbols, and code labels.
std::vector<CogScriptOutlineEntry> buildCogScriptOutline(const QString& text);

} // namespace sed::ui::cog
