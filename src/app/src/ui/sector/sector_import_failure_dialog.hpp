#pragma once

#include <QString>

class QWidget;

namespace sed::ui::sector
{

/** Shows one designer-facing failure dialog and writes its one Core log record. */
void showSectorImportFailure(
    QWidget* parent,
    const QString& filename,
    const QString& summary,
    const QString& details = {});

} // namespace sed::ui::sector
