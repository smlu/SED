#pragma once

#include <QVector3D>

#include "sed/level.hpp"

namespace sed::render
{

/** Normalizes a transform basis in-place, leaving zero-length axes untouched. */
void normalizeBasis(QVector3D& x, QVector3D& y, QVector3D& z) noexcept;

/** True when all basis axes have enough length for transform gizmo math. */
bool basisIsUsable(const QVector3D& x, const QVector3D& y, const QVector3D& z) noexcept;

/** Expands an optional axis-aligned Qt-space bounds by one point. */
void addPointToBounds(
    QVector3D& min,
    QVector3D& max,
    bool& haveBounds,
    float x,
    float y,
    float z) noexcept;

/**
 * Computes a stable local frame for a level surface.
 *
 * The returned X/Y axes lie on the surface and Z is the surface normal. Invalid
 * refs, short surfaces, bad vertex indices, and degenerate triangles return
 * false without modifying the outputs.
 */
bool computeSurfaceLocalBasis(
    const Level& level,
    SurfaceRef ref,
    QVector3D& outX,
    QVector3D& outY,
    QVector3D& outZ) noexcept;

} // namespace sed::render
