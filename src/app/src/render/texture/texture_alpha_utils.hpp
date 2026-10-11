#pragma once

#include <cstdint>

#include <QImage>

namespace sed::render
{

/**
 * Infers the alpha precision hint for an uploaded texture.
 *
 * Returns 0 for opaque/no-alpha images, 1 for binary cutouts, and 8 when any
 * sampled pixel has partial translucency.
 */
std::int32_t inferTextureAlphaBpp(const QImage& img) noexcept;

} // namespace sed::render
