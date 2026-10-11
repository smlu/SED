#pragma once

#include <QColor>
#include <QImage>
#include <QString>

#include <string>
#include <string_view>
#include <unordered_map>

namespace sed::render
{

using GizmoTypeColorMap = std::unordered_map<std::string, QColor>;

/**
 * Chooses the marker color for a thing gizmo type.
 *
 * User-provided type colors win case-insensitively. A user "default" color is
 * used before the built-in fallback palette.
 */
QColor gizmoIconColorForType(std::string_view type, const GizmoTypeColorMap& userTypeColors);

/**
 * Builds the compact two-character label drawn inside thing gizmo markers.
 *
 * Alphanumeric characters are preferred so noisy template names still produce
 * readable labels.
 */
QString gizmoIconLabelForType(std::string_view type);

/**
 * Renders the regular Thing-light bulb shared by its toolbar action and
 * viewport gizmo. The authored tint forms a fully opaque source body.
 */
QImage renderThingRegularLightIconImage(QColor tint, int size = 128);

/**
 * Renders the actor Headlight glyph shared by its toolbar action and viewport
 * gizmo. A bright actor body beside a radial point source distinguishes it
 * from the standalone Thing-light bulb without implying a spotlight beam.
 */
QImage renderThingHeadlightIconImage(QColor tint, int size = 128);

/**
 * Renders the square icon image used by thing icon-cube textures.
 *
 * The returned image is in normal Qt top-left image coordinates. GL callers
 * should mirror it if their texture upload path expects OpenGL-style origin.
 */
QImage renderGizmoIconImage(
    std::string_view type,
    const GizmoTypeColorMap& userTypeColors,
    QColor baseTint,
    int size = 256);

/**
 * Renders the IJIM sector point-light glyph shared by the viewport gizmo and
 * toolbar action: a rounded source square with eight outward light rays.
 *
 * The image is returned in Qt top-left coordinates. GL callers should mirror
 * it for upload when their texture path uses an OpenGL-style origin.
 */
QImage renderSectorPointLightIconImage(QColor tint, int size = 128);

/**
 * Renders the MotS ARCHLIGHTING owner badge, sharing the JKDF2 Arch Thing icon.
 * The tint argument is retained for callers; both projects use the same gold
 * owner marker because ARCHLIGHTING applies to Arch Things.
 */
QImage renderArchitecturalLightingIconImage(QColor tint, int size = 128);

/** Renders the standalone gold cube marker for Arch Things. */
QImage renderArchThingBadgeImage(int size = 64);

} // namespace sed::render
