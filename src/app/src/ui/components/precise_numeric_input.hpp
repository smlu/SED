#pragma once

class QWidget;
class QString;

namespace sed::ui
{

// Accepts finite locale-aware input within the inclusive range without decimal
// quantization. Cancellation/rejection returns initial and writes false to ok.
// The nullable ok output is always assigned when provided.
double preciseNumericInput(QWidget* parent, const QString& title, const QString& label,
    double initial, double minimum, double maximum, bool* ok);

} // namespace sed::ui
