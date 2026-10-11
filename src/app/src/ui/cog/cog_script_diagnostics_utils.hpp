#pragma once

#include <vector>

#include <QString>
#include <QStringList>

#include "sed/level.hpp"

namespace sed::ui::cog
{

/// One diagnostic or warning produced by COG script analysis.
struct CogScriptDiagnostic
{
    QString message{};
    int line{-1};
    bool error{false};
    int column{-1};
    int length{0};
};

/// Analyzes COG script text and returns editor diagnostics with line/column spans.
std::vector<CogScriptDiagnostic> buildCogScriptDiagnostics(
    const QString& text,
    const QStringList& knownMessages,
    ProjectType projectType);

} // namespace sed::ui::cog
