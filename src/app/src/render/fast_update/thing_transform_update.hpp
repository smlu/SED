#pragma once

#include <cstddef>

#include <QMatrix4x4>
#include <QVector3D>

#include "sed/level.hpp"

namespace sed::render::fast_update
{

/**
 * Compact 3x4 affine transform used by the thing fast-update path.
 *
 * Keeping this separate from QMatrix4x4 makes per-vertex cache updates cheap and
 * keeps the large viewport implementation free of ad hoc transform structs.
 */
struct Affine3f
{
    float m00{}, m01{}, m02{}, m03{};
    float m10{}, m11{}, m12{}, m13{};
    float m20{}, m21{}, m22{}, m23{};

    /** Maps a point through the affine transform. */
    QVector3D mapPoint(const QVector3D& point) const noexcept;
};

/**
 * Per-thing cached-render update state keyed by pick id.
 *
 * The viewport computes these once, then applies them to triangle, line, point,
 * bounds, sprite, and overlay buffers.
 */
struct ThingTransformDelta
{
    Affine3f xf{};
    QVector3D colorMul{ 1.0f, 1.0f, 1.0f };
    ColorF ambientLight{ ColorWhite };
    PointLight sectorPointLight{};
    std::int32_t sectorLightOwner{ -1 };
    QVector3D editorSectorTint{ 1.0f, 1.0f, 1.0f };
    bool recolor{};
    bool updateSectorPointLight{};
    bool updateEditorSectorTint{};
    std::size_t thingIndex{};
    bool xformChanged{};
};

/** Converts the upper 3x4 part of a Qt matrix into Affine3f. */
Affine3f affineFromMatrix(const QMatrix4x4& matrix) noexcept;

/** Updates cached dynamic-light geometry; normals deliberately ignore translation. */
void transformDynamicLightGeometry(
    QVector3D& position,
    QVector3D& normal,
    QVector3D* diffuseFaceAnchor,
    const Affine3f& transform) noexcept;

/** Exact authored-double comparison; callers validate finite transform fields. */
bool nearlySameTransform(double a, double b) noexcept;

/** Float comparison used for colors and Qt-space vectors in fast updates. */
bool nearlySameRenderValue(float a, float b) noexcept;

/** True when two Qt vectors are close enough for cached render state checks. */
bool nearlySameRenderValue(const QVector3D& a, const QVector3D& b) noexcept;

/** True when two colors are close enough for cached render state checks. */
bool nearlySameRenderValue(const ColorF& a, const ColorF& b) noexcept;

/**
 * True when an SPR uses the original camera-facing construction.
 *
 * OpenJones3D rdSprite_Draw constructs type-0 vertices directly in view space,
 * so authored Thing pitch/yaw never turn that card away from the camera. Type-2
 * sprites instead build their plane around the Thing forward axis.
 */
bool usesOriginalSpriteCameraFacingFrame(int spriteType, bool gamePreview) noexcept;

/**
 * Type-2 SPR plane axes produced by the original rdSprite construction.
 * `forward` is the Thing's normalized lvec; `right` uses cross(+Z, forward)
 * except near the vertical pole, where rdSprite falls back to +X; `up` is
 * cross(right, forward).  Degenerate/non-finite input falls back to +Y.
 */
struct OriginalSpriteType2Frame
{
    QVector3D right{};
    QVector3D forward{};
    QVector3D up{};
};

OriginalSpriteType2Frame makeOriginalSpriteType2Frame(
    const QVector3D& thingForward) noexcept;

/** Converts a float pick id written into a vertex back into its integer key. */
int pickIdKey(float pickId) noexcept;

/** Expands an optional bounds union by a min/max pair. */
void includeBounds(
    QVector3D& outMin,
    QVector3D& outMax,
    bool& haveBounds,
    const QVector3D& min,
    const QVector3D& max) noexcept;

/** Transforms an axis-aligned bounds through an affine matrix and refits it. */
void transformBounds(QVector3D& min, QVector3D& max, const Affine3f& xf) noexcept;

/** Transforms an axis-aligned bounds and updates its center. */
void transformBounds(QVector3D& min, QVector3D& max, QVector3D& center, const Affine3f& xf) noexcept;

} // namespace sed::render::fast_update
