#include "thing_placement_drop_utils.hpp"

#include <QMimeData>

namespace sed::ui::viewport
{

QString thingTemplateNameFromMime(const QMimeData* mime)
{
    if (!mime)
    {
        return {};
    }

    // Step 1: prefer the structured MIME payload created by the template list.
    if (mime->hasFormat(kThingPlacementTemplateMimeType))
    {
        const QString custom = QString::fromUtf8(mime->data(kThingPlacementTemplateMimeType)).trimmed();
        if (!custom.isEmpty())
        {
            return custom;
        }
    }

    // Step 2: fall back to plain text so copied names and external drags still work.
    if (!mime->hasText())
    {
        return {};
    }

    // Step 3: keep only the first plain-text field, matching the former drop lambda.
    QString text = mime->text().trimmed();
    const int newline = text.indexOf(QLatin1Char('\n'));
    if (newline >= 0)
    {
        text = text.left(newline).trimmed();
    }
    const int tab = text.indexOf(QLatin1Char('\t'));
    if (tab >= 0)
    {
        text = text.left(tab).trimmed();
    }
    return text;
}

bool isThingPlacementTemplateDropEvent(const QEvent::Type type) noexcept
{
    return type == QEvent::DragEnter
        || type == QEvent::DragMove
        || type == QEvent::Drop;
}

} // namespace sed::ui::viewport
