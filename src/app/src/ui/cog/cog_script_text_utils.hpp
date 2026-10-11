#pragma once

#include <QString>

namespace sed::ui::cog
{

int findCommentStart(const QString& line);
QString stripComment(const QString& line);
QString sanitizeLineForAnalysis(const QString& line);

} // namespace sed::ui::cog
