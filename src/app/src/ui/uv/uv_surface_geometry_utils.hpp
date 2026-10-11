#pragma once

#include "sed/level.hpp"

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <QVector3D>

namespace sed::ui::uv
{

/** Normalizes a finite QVector3D in-place; returns false for non-finite or near-zero vectors. */
bool normalizeUvVector(QVector3D& value);

/** Returns a surface vertex in world coordinates, or nullopt when any ref/index is invalid. */
std::optional<QVector3D> uvSurfaceVertexWorld(const Level& level, SurfaceRef ref, std::size_t localIndex);

/** Returns finite authoritative double coordinates, with no presentation rounding. */
std::optional<TVector> uvSurfaceVertexPosition(const Level& level, SurfaceRef ref, std::size_t localIndex);

/** Finds local vertex-index pairs whose world positions are shared by two surfaces. */
std::vector<std::pair<std::size_t, std::size_t>> uvSharedVertexPairs(const Level& level, SurfaceRef a, SurfaceRef b);

/** Returns a normalized surface normal, recalculating from vertices when the stored normal is empty. */
QVector3D uvSurfaceNormal(const Level& level, SurfaceRef ref);

/** Returns true when two surfaces share an edge in either vertex order. */
bool uvSurfacesShareEdge(const Level& level, SurfaceRef a, SurfaceRef b);

/** Returns the sector colormap for a surface, falling back to the level master colormap. */
std::string uvSurfaceColormapName(const Level& level, SurfaceRef ref);

} // namespace sed::ui::uv
