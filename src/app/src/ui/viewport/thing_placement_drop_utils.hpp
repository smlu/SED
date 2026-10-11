#pragma once

#include <QEvent>
#include <QString>

class QMimeData;

namespace sed::ui::viewport
{

inline constexpr const char* kThingPlacementTemplateMimeType = "application/x-sed-thing-template";

/**
 * Extracts the thing template name carried by a drag/drop payload.
 *
 * The thing placement list writes a custom MIME value plus plain text. External
 * drags may only provide text, so this helper preserves the legacy fallback
 * rules used by MainWindow drops.
 */
QString thingTemplateNameFromMime(const QMimeData* mime);

/**
 * Returns whether an event type belongs to the viewport thing-template drag/drop flow.
 */
bool isThingPlacementTemplateDropEvent(QEvent::Type type) noexcept;

} // namespace sed::ui::viewport
