#include "main_window/common.hpp"
#include "main_window/detail/history_dirty_utils.hpp"
#include "ui/shortcuts/key_sequence_display.hpp"

#include "ui/navigation/outliner_types.hpp"

namespace sed
{

namespace
{

enum class HistoryTargetMode : std::uint8_t
{
    sector,
    surface,
    surfaceVertex,
    thing,
    frame,
    thingLight,
    sectorLight,
    bakedLight,
};

struct HistoryTarget
{
    HistoryTargetMode mode{};
    std::int32_t index{ -1 };
    std::int32_t sector{ -1 };
    std::int32_t surface{ -1 };
    std::int32_t value{ -1 };
    std::int32_t localVertex{ -1 };
};

/**
 * Returns the valid selection values belonging to the state represented by a
 * History row.  A row names the state *after* its operation, so its after
 * selection is authoritative.  The before selection is only a fallback for
 * edits which deliberately clear selection metadata without deleting the
 * edited owner.
 *
 * Using a union here is subtly wrong for topology edits: a cleave's source
 * Sector/Surface lives in `before`, while the exact new output selected by the
 * command lives in `after`.  Unioning both made navigation frame/select the
 * source plus every result and often looked like the action did nothing.
 */
template <typename Set, typename Less, typename Valid>
auto validSortedValues(const Set& source, Less less, Valid valid)
{
    using Value = typename Set::value_type;
    std::vector<Value> values{};
    values.reserve(source.size());
    for (const Value& value : source)
    {
        if (valid(value))
        {
            values.push_back(value);
        }
    }
    std::sort(values.begin(), values.end(), less);
    values.erase(std::unique(values.begin(), values.end()), values.end());
    return values;
}

template <typename Set, typename Less, typename Valid>
auto preferredValidValues(
    const Set& after,
    const Set& before,
    Less less,
    Valid valid)
{
    using Value = typename Set::value_type;

    std::vector<Value> values = validSortedValues(after, less, valid);
    return values.empty()
        ? validSortedValues(before, less, valid)
        : values;
}

bool hasDirty(const std::uint32_t dirtyFlags, const editor::DocumentDirty flag)
{
    return (dirtyFlags & static_cast<std::uint32_t>(flag)) != 0u;
}

bool hasMotsArchLighting(const Level& level) noexcept
{
    return level.kind == ProjectType::MOTS
        && !level.motsArchLighting.empty();
}

bool hasMotsArchBakeState(const Level& level) noexcept
{
    if (level.kind != ProjectType::MOTS)
    {
        return false;
    }
    return !level.motsArchLightingBakeBaselines.empty()
        || std::ranges::any_of(level.lights, [](const SedLight& light)
            {
                return (light.flags & LF_AffectArchThings) != 0;
            });
}

bool validThing(const Level& level, const std::int32_t index)
{
    return index >= 0 && static_cast<std::size_t>(index) < level.things.size();
}

bool validSector(const Level& level, const std::int32_t index)
{
    return index >= 0 && static_cast<std::size_t>(index) < level.sectors.size();
}

bool validSurface(const Level& level, const SurfaceRef ref)
{
    return validSector(level, ref.sector)
        && ref.surface >= 0
        && static_cast<std::size_t>(ref.surface)
            < level.sectors[static_cast<std::size_t>(ref.sector)].surfaces.size();
}

bool validFrame(const Level& level, const level::FrameRef ref)
{
    if (!validThing(level, ref.thing) || ref.value < 0)
    {
        return false;
    }
    const Thing& thing = level.things[static_cast<std::size_t>(ref.thing)];
    if (static_cast<std::size_t>(ref.value) >= thing.values.size())
    {
        return false;
    }
    const TplValue& value = thing.values[static_cast<std::size_t>(ref.value)];
    return value.type == ValueType::frame
        && text::iequalsAscii(value.name, "frame");
}

bool containsAffectedIndex(
    const std::vector<std::int32_t>& indices,
    const std::int32_t value)
{
    return std::binary_search(indices.begin(), indices.end(), value);
}

/**
 * Infer targets from the concrete before/after selections and dirty domain.
 *
 * Selection metadata is part of each authoritative DocumentState, while the
 * dirty mask identifies owner-specific light edits.  The precedence is
 * deliberate: frames, surface vertices/faces, dynamic/sector/baked lights,
 * Things, then Sectors.  A transition containing no valid target is still a
 * valid history row; navigation simply becomes a no-op for that row.
 */
std::vector<HistoryTarget> inferHistoryTargets(
    const editor::DocumentHistoryTransitionState& transition,
    const Level& level)
{
    const editor::Selection& before = transition.beforeSelection;
    const editor::Selection& after = transition.afterSelection;
    const std::vector<level::FrameRef> frames = preferredValidValues(
        after.frames,
        before.frames,
        [](const level::FrameRef& a, const level::FrameRef& b)
        {
            return a.thing < b.thing
                || (a.thing == b.thing && a.value < b.value);
        },
        [&level](const level::FrameRef ref) { return validFrame(level, ref); });
    const std::vector<std::int32_t>& affectedThings =
        transition.affectedThingIndices;
    const std::vector<std::int32_t>& affectedSectors =
        transition.affectedSectorIndices;

    std::vector<HistoryTarget> result{};
    const bool allDomains = transition.dirtyFlags
        == static_cast<std::uint32_t>(editor::DocumentDirty::All);
    if ((hasDirty(transition.dirtyFlags, editor::DocumentDirty::Things) || allDomains)
        && !frames.empty())
    {
        for (const level::FrameRef ref : frames)
        {
            if (validFrame(level, ref)
                && (affectedThings.empty()
                    || containsAffectedIndex(affectedThings, ref.thing)))
            {
                result.push_back(HistoryTarget{
                    HistoryTargetMode::frame, ref.thing, -1, -1, ref.value, -1 });
            }
        }
        if (!result.empty())
        {
            return result;
        }
    }

    const bool lightsChanged = hasDirty(transition.dirtyFlags, editor::DocumentDirty::Lights)
        && !allDomains;
    if (lightsChanged)
    {
        const std::vector<std::int32_t> things = affectedThings.empty()
            ? preferredValidValues(
                after.things,
                before.things,
                std::less<std::int32_t>{},
                [&level](const std::int32_t index) { return validThing(level, index); })
            : affectedThings;
        if (!things.empty())
        {
            for (const std::int32_t index : things)
            {
                if (validThing(level, index))
                {
                    result.push_back(HistoryTarget{
                        HistoryTargetMode::thingLight, index });
                }
            }
            if (!result.empty())
            {
                return result;
            }
        }

        const std::vector<std::int32_t> sectors = affectedSectors.empty()
            ? preferredValidValues(
                after.sectors,
                before.sectors,
                std::less<std::int32_t>{},
                [&level](const std::int32_t index) { return validSector(level, index); })
            : affectedSectors;
        if (!sectors.empty())
        {
            for (const std::int32_t index : sectors)
            {
                if (validSector(level, index))
                {
                    result.push_back(HistoryTarget{
                        HistoryTargetMode::sectorLight, index });
                }
            }
            if (!result.empty())
            {
                return result;
            }
        }

        const std::vector<std::int32_t> lights = preferredValidValues(
            after.lights,
            before.lights,
            std::less<std::int32_t>{},
            [&level](const std::int32_t index)
            {
                return index >= 0
                    && static_cast<std::size_t>(index) < level.lights.size();
            });
        for (const std::int32_t index : lights)
        {
            if (index >= 0 && static_cast<std::size_t>(index) < level.lights.size())
            {
                result.push_back(HistoryTarget{
                    HistoryTargetMode::bakedLight, index });
            }
        }
        if (!result.empty())
        {
            return result;
        }
    }

    const bool geometryChanged = hasDirty(
        transition.dirtyFlags, editor::DocumentDirty::LevelGeometry) || allDomains;
    const std::vector<editor::SurfaceVertexRef> vertices = preferredValidValues(
        after.surfaceVertices,
        before.surfaceVertices,
        [](const editor::SurfaceVertexRef& a, const editor::SurfaceVertexRef& b)
        {
            if (a.surface.sector != b.surface.sector)
            {
                return a.surface.sector < b.surface.sector;
            }
            if (a.surface.surface != b.surface.surface)
            {
                return a.surface.surface < b.surface.surface;
            }
            return a.localVertex < b.localVertex;
        },
        [&level](const editor::SurfaceVertexRef ref)
        {
            return validSurface(level, ref.surface)
                && ref.localVertex >= 0
                && static_cast<std::size_t>(ref.localVertex)
                    < level.sectors[static_cast<std::size_t>(ref.surface.sector)]
                        .surfaces[static_cast<std::size_t>(ref.surface.surface)].vertices.size();
        });
    if (geometryChanged && !vertices.empty())
    {
        for (const editor::SurfaceVertexRef ref : vertices)
        {
            if (validSurface(level, ref.surface)
                && (affectedSectors.empty()
                    || containsAffectedIndex(affectedSectors, ref.surface.sector))
                && ref.localVertex >= 0
                && static_cast<std::size_t>(ref.localVertex)
                    < level.sectors[static_cast<std::size_t>(ref.surface.sector)]
                        .surfaces[static_cast<std::size_t>(ref.surface.surface)].vertices.size())
            {
                result.push_back(HistoryTarget{
                    HistoryTargetMode::surfaceVertex,
                    -1,
                    ref.surface.sector,
                    ref.surface.surface,
                    -1,
                    ref.localVertex });
            }
        }
        if (!result.empty())
        {
            return result;
        }
    }

    const std::vector<SurfaceRef> surfaces = preferredValidValues(
        after.surfaces,
        before.surfaces,
        [](const SurfaceRef& a, const SurfaceRef& b)
        {
            return a.sector < b.sector
                || (a.sector == b.sector && a.surface < b.surface);
        },
        [&level](const SurfaceRef ref) { return validSurface(level, ref); });
    if (geometryChanged && !surfaces.empty())
    {
        for (const SurfaceRef ref : surfaces)
        {
            if (validSurface(level, ref)
                && (affectedSectors.empty()
                    || containsAffectedIndex(affectedSectors, ref.sector)))
            {
                result.push_back(HistoryTarget{
                    HistoryTargetMode::surface, -1, ref.sector, ref.surface });
            }
        }
        if (!result.empty())
        {
            return result;
        }
    }

    const std::vector<std::int32_t> things = affectedThings.empty()
        ? preferredValidValues(
            after.things,
            before.things,
            std::less<std::int32_t>{},
            [&level](const std::int32_t index) { return validThing(level, index); })
        : affectedThings;
    if ((hasDirty(transition.dirtyFlags, editor::DocumentDirty::Things) || allDomains)
        && !things.empty())
    {
        for (const std::int32_t index : things)
        {
            if (validThing(level, index))
            {
                result.push_back(HistoryTarget{
                    HistoryTargetMode::thing, index });
            }
        }
        if (!result.empty())
        {
            return result;
        }
    }

    const std::vector<std::int32_t> lights = preferredValidValues(
        after.lights,
        before.lights,
        std::less<std::int32_t>{},
        [&level](const std::int32_t index)
        {
            return index >= 0
                && static_cast<std::size_t>(index) < level.lights.size();
        });
    if ((hasDirty(transition.dirtyFlags, editor::DocumentDirty::Lights) || allDomains)
        && !lights.empty())
    {
        for (const std::int32_t index : lights)
        {
            if (index >= 0 && static_cast<std::size_t>(index) < level.lights.size())
            {
                result.push_back(HistoryTarget{
                    HistoryTargetMode::bakedLight, index });
            }
        }
        if (!result.empty())
        {
            return result;
        }
    }

    if (geometryChanged)
    {
        const auto validSectorIndex = [&level](const std::int32_t index)
        {
            return validSector(level, index);
        };
        std::vector<std::int32_t> sectors = validSortedValues(
            after.sectors,
            std::less<std::int32_t>{},
            validSectorIndex);
        if (sectors.empty())
        {
            sectors = !affectedSectors.empty()
                ? validSortedValues(
                    affectedSectors,
                    std::less<std::int32_t>{},
                    validSectorIndex)
                : validSortedValues(
                    before.sectors,
                    std::less<std::int32_t>{},
                    validSectorIndex);
        }
        for (const std::int32_t index : sectors)
        {
            if (validSector(level, index))
            {
                result.push_back(HistoryTarget{
                    HistoryTargetMode::sector, index });
            }
        }
    }
    return result;
}

void includePoint(QVector3D& minv, QVector3D& maxv, bool& haveAny, const double x,
                  const double y, const double z)
{
    if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z))
    {
        return;
    }
    const QVector3D point(
        static_cast<float>(x), static_cast<float>(y), static_cast<float>(z));
    if (!haveAny)
    {
        minv = point;
        maxv = point;
        haveAny = true;
    }
    else
    {
        minv.setX(std::min(minv.x(), point.x()));
        minv.setY(std::min(minv.y(), point.y()));
        minv.setZ(std::min(minv.z(), point.z()));
        maxv.setX(std::max(maxv.x(), point.x()));
        maxv.setY(std::max(maxv.y(), point.y()));
        maxv.setZ(std::max(maxv.z(), point.z()));
    }
}

void includeSectorBounds(const Sector& sector, QVector3D& minv, QVector3D& maxv,
                         bool& haveAny)
{
    for (const Vertex& vertex : sector.vertices)
    {
        includePoint(minv, maxv, haveAny, vertex.x, vertex.y, vertex.z);
    }
}

ThingBox resolveHistoryThingBox(const Level& level, const Thing& thing)
{
    if (!thing.bbox.IsZero())
    {
        return thing.bbox;
    }

    std::string current = thing.templateName.empty() ? thing.name : thing.templateName;
    for (int depth = 0; depth < 256 && !current.empty(); ++depth)
    {
        const ThingTemplate* templateValue = nullptr;
        for (const ThingTemplate& candidate : level.templates)
        {
            if (text::iequalsAscii(candidate.name, current))
            {
                templateValue = &candidate;
                break;
            }
        }
        if (!templateValue)
        {
            break;
        }
        if (!templateValue->bbox.IsZero())
        {
            return templateValue->bbox;
        }
        if (templateValue->parent.empty()
            || text::iequalsAscii(templateValue->parent, "none"))
        {
            break;
        }
        current = templateValue->parent;
    }

    ThingBox fallback{};
    fallback.Set(-0.5, -0.5, -0.5, 0.5, 0.5, 0.5);
    return fallback;
}

void includeThingBounds(const Level& level, const Thing& thing,
                        QVector3D& minv, QVector3D& maxv, bool& haveAny)
{
    const ThingBox box = resolveHistoryThingBox(level, thing);
    const TVector localMin{ box.x1, box.y1, box.z1 };
    const TVector localMax{ box.x2, box.y2, box.z2 };
    TVector pyr{};
    pyr.pitch = thing.pch;
    pyr.yaw = thing.yaw;
    pyr.roll = thing.rol;
    const TMatrix43 transform = TMatrix43::Create(
        TVector{ thing.x, thing.y, thing.z }, pyr);
    const TVector corners[8]{
        TVector{ localMin.x, localMin.y, localMin.z },
        TVector{ localMax.x, localMin.y, localMin.z },
        TVector{ localMax.x, localMax.y, localMin.z },
        TVector{ localMin.x, localMax.y, localMin.z },
        TVector{ localMin.x, localMin.y, localMax.z },
        TVector{ localMax.x, localMin.y, localMax.z },
        TVector{ localMax.x, localMax.y, localMax.z },
        TVector{ localMin.x, localMax.y, localMax.z },
    };
    for (const TVector& corner : corners)
    {
        const TVector world = transform.TransformPoint(corner);
        includePoint(minv, maxv, haveAny, world.x, world.y, world.z);
    }
}

void includeTargetBounds(const HistoryTarget& target, const Level& level,
                         QVector3D& minv, QVector3D& maxv, bool& haveAny)
{
    switch (target.mode)
    {
        case HistoryTargetMode::sector:
        case HistoryTargetMode::sectorLight:
            if (validSector(level, target.index))
            {
                includeSectorBounds(level.sectors[static_cast<std::size_t>(target.index)],
                                    minv, maxv, haveAny);
            }
            break;
        case HistoryTargetMode::surface:
        case HistoryTargetMode::surfaceVertex:
            if (validSurface(level, SurfaceRef{ target.sector, target.surface }))
            {
                const Sector& sector = level.sectors[static_cast<std::size_t>(target.sector)];
                const Surface& surface = sector.surfaces[static_cast<std::size_t>(target.surface)];
                if (target.mode == HistoryTargetMode::surfaceVertex
                    && target.localVertex >= 0
                    && static_cast<std::size_t>(target.localVertex) < surface.vertices.size())
                {
                    const std::int32_t vertexIndex = surface.vertices[
                        static_cast<std::size_t>(target.localVertex)];
                    if (vertexIndex >= 0
                        && static_cast<std::size_t>(vertexIndex) < sector.vertices.size())
                    {
                        const Vertex& vertex = sector.vertices[static_cast<std::size_t>(vertexIndex)];
                        includePoint(minv, maxv, haveAny, vertex.x - 0.15, vertex.y - 0.15,
                                     vertex.z - 0.15);
                        includePoint(minv, maxv, haveAny, vertex.x + 0.15, vertex.y + 0.15,
                                     vertex.z + 0.15);
                    }
                }
                else
                {
                    for (const std::int32_t vertexIndex : surface.vertices)
                    {
                        if (vertexIndex >= 0
                            && static_cast<std::size_t>(vertexIndex) < sector.vertices.size())
                        {
                            const Vertex& vertex = sector.vertices[static_cast<std::size_t>(vertexIndex)];
                            includePoint(minv, maxv, haveAny, vertex.x, vertex.y, vertex.z);
                        }
                    }
                }
            }
            break;
        case HistoryTargetMode::thing:
        case HistoryTargetMode::frame:
        case HistoryTargetMode::thingLight:
            if (validThing(level, target.index))
            {
                const Thing& thing = level.things[static_cast<std::size_t>(target.index)];
                if (target.mode == HistoryTargetMode::frame
                    && target.value >= 0
                    && static_cast<std::size_t>(target.value) < thing.values.size())
                {
                    double x{}, y{}, z{}, pch{}, yaw{}, rol{};
                    if (thing.values[static_cast<std::size_t>(target.value)].tryGetFrame(
                            x, y, z, pch, yaw, rol))
                    {
                        includePoint(minv, maxv, haveAny, x - 0.25, y - 0.25, z - 0.25);
                        includePoint(minv, maxv, haveAny, x + 0.25, y + 0.25, z + 0.25);
                    }
                    else
                    {
                        includeThingBounds(level, thing, minv, maxv, haveAny);
                    }
                }
                else
                {
                    includeThingBounds(level, thing, minv, maxv, haveAny);
                }
            }
            break;
        case HistoryTargetMode::bakedLight:
            if (target.index >= 0
                && static_cast<std::size_t>(target.index) < level.lights.size())
            {
                const SedLight& light = level.lights[static_cast<std::size_t>(target.index)];
                // Keep history framing identical to the Outliner light
                // double-click path, including its finite/range clamps.
                const double authoredRange = std::isfinite(light.range)
                    ? light.range : 0.0;
                const double extent = std::clamp(authoredRange, 0.01, 1.0e6);
                includePoint(minv, maxv, haveAny, light.position.x - extent,
                             light.position.y - extent, light.position.z - extent);
                includePoint(minv, maxv, haveAny, light.position.x + extent,
                             light.position.y + extent, light.position.z + extent);
            }
            break;
    }
}

} // namespace

namespace
{

bool captureLevelSurfaceSelectionTopologySnapshot(
    const Level& level,
    std::vector<std::int64_t>& snapshot) noexcept
{
    snapshot.clear();
    std::size_t scalarCount = 1u;
    const auto addScalarCount = [&scalarCount](const std::size_t count) noexcept
    {
        if (count > std::numeric_limits<std::size_t>::max() - scalarCount)
        {
            return false;
        }
        scalarCount += count;
        return true;
    };

    for (const Sector& sector : level.sectors)
    {
        if (!addScalarCount(4u))
        {
            return false;
        }
        for (const Surface& surface : sector.surfaces)
        {
            if (!addScalarCount(3u)
                || !addScalarCount(surface.vertices.size())
                || (surface.adjoin && !addScalarCount(2u)))
            {
                return false;
            }
        }
    }

    if (scalarCount > snapshot.max_size())
    {
        return false;
    }
    const auto sizeScalar = [](const std::size_t value,
                               std::int64_t& result) noexcept
    {
        if (value > static_cast<std::uintmax_t>(
                std::numeric_limits<std::int64_t>::max()))
        {
            return false;
        }
        result = static_cast<std::int64_t>(value);
        return true;
    };
    try
    {
        snapshot.reserve(scalarCount);
        std::int64_t encodedSize{};
        if (!sizeScalar(level.sectors.size(), encodedSize))
        {
            snapshot.clear();
            return false;
        }
        snapshot.push_back(encodedSize);
        for (const Sector& sector : level.sectors)
        {
            if (!sizeScalar(sector.vertices.size(), encodedSize))
            {
                snapshot.clear();
                return false;
            }
            snapshot.push_back(sector.id);
            snapshot.push_back(sector.num);
            snapshot.push_back(encodedSize);
            if (!sizeScalar(sector.surfaces.size(), encodedSize))
            {
                snapshot.clear();
                return false;
            }
            snapshot.push_back(encodedSize);

            for (const Surface& surface : sector.surfaces)
            {
                if (!sizeScalar(surface.vertices.size(), encodedSize))
                {
                    snapshot.clear();
                    return false;
                }
                snapshot.push_back(surface.num);
                snapshot.push_back(encodedSize);
                snapshot.push_back(surface.adjoin ? 1 : 0);
                for (const std::int32_t vertex : surface.vertices)
                {
                    snapshot.push_back(vertex);
                }
                if (surface.adjoin)
                {
                    snapshot.push_back(surface.adjoin->sector);
                    snapshot.push_back(surface.adjoin->surface);
                }
            }
        }
    }
    catch (...)
    {
        snapshot.clear();
        return false;
    }

    return snapshot.size() == scalarCount;
}

bool matchesLevelSurfaceSelectionTopologySnapshot(
    const Level& level,
    const std::span<const std::int64_t> snapshot) noexcept
{
    std::size_t cursor = 0u;
    const auto matchesScalar = [&snapshot, &cursor](
                                   const std::int64_t value) noexcept
    {
        return cursor < snapshot.size() && snapshot[cursor++] == value;
    };
    const auto matchesSize = [&matchesScalar](const std::size_t value) noexcept
    {
        if (value > static_cast<std::uintmax_t>(
                std::numeric_limits<std::int64_t>::max()))
        {
            return false;
        }
        return matchesScalar(static_cast<std::int64_t>(value));
    };

    if (!matchesSize(level.sectors.size()))
    {
        return false;
    }
    for (const Sector& sector : level.sectors)
    {
        if (!matchesScalar(sector.id)
            || !matchesScalar(sector.num)
            || !matchesSize(sector.vertices.size())
            || !matchesSize(sector.surfaces.size()))
        {
            return false;
        }
        for (const Surface& surface : sector.surfaces)
        {
            if (!matchesScalar(surface.num)
                || !matchesSize(surface.vertices.size())
                || !matchesScalar(surface.adjoin ? 1 : 0))
            {
                return false;
            }
            for (const std::int32_t vertex : surface.vertices)
            {
                if (!matchesScalar(vertex))
                {
                    return false;
                }
            }
            if (surface.adjoin
                && (!matchesScalar(surface.adjoin->sector)
                    || !matchesScalar(surface.adjoin->surface)))
            {
                return false;
            }
        }
    }
    return cursor == snapshot.size();
}

} // namespace

bool MainWindow::hasSurfaceSelectionContextForTopologyInvalidation() const noexcept
{
    const editor::Selection& selection = document_.selection();
    return (surfaceStrictSelectionAction_
            && surfaceStrictSelectionAction_->isChecked())
        || surfaceSelectionMode_ != SurfaceSelectionMode::face
        || !strictSurfaceSelectionScope_.empty()
        || !rememberedSurfaceFaceSelection_.empty()
        || !lastSurfaceSelection_.surfaces.empty()
        || !lastSurfaceSelection_.surfaceVertices.empty()
        || !selection.surfaceVertices.empty();
}

bool MainWindow::captureSurfaceSelectionTopologySnapshot(
    std::vector<std::int64_t>& snapshot) const noexcept
{
    return captureLevelSurfaceSelectionTopologySnapshot(
        document_.level(), snapshot);
}

bool MainWindow::matchesSurfaceSelectionTopologySnapshot(
    const std::span<const std::int64_t> snapshot) const noexcept
{
    return matchesLevelSurfaceSelectionTopologySnapshot(
        document_.level(), snapshot);
}

std::optional<MainWindow::CleaveApplyWorkspaceMemento>
MainWindow::captureCleaveApplyWorkspaceMemento()
{
    // Discard attachments whose concrete records have already left the
    // document timeline before charging the next Apply.  The pruning walk is
    // allocation-free and never evicts a still-live transition.
    pruneCleaveApplyWorkspaceMementos();
    if (!glView_ || !glView_->hasStagedCleaveDraft())
    {
        return std::nullopt;
    }

    constexpr std::uint64_t kMaximumAttachmentBytes = 16ull * 1024ull * 1024ull;
    constexpr std::size_t kMaximumAttachmentCount = 64u;
    const auto addBytes = [](std::uint64_t& total,
                             const std::uint64_t count,
                             const std::uint64_t elementBytes) noexcept
    {
        if (elementBytes != 0u
            && count > (std::numeric_limits<std::uint64_t>::max() - total)
                / elementBytes)
        {
            return false;
        }
        total += count * elementBytes;
        return true;
    };
    const auto snapshotBytes = [&addBytes](
        const GlView::RecoveryState::CleaveDraftSnapshot& snapshot,
        std::uint64_t& total) noexcept
    {
        if (!addBytes(total, 1u, sizeof(snapshot))
            || !addBytes(total, snapshot.lines.capacity(),
                sizeof(GlView::RecoveryState::CleaveLine))
            || !addBytes(total, snapshot.context.targetSectors.capacity(),
                sizeof(std::int32_t))
            || !addBytes(total, snapshot.context.targetSurfaces.capacity(),
                sizeof(SurfaceRef))
            || !addBytes(total, snapshot.applyOrder.capacity(),
                sizeof(std::int32_t))
            || !addBytes(total, snapshot.selectedCleaveLines.capacity(),
                sizeof(std::int32_t)))
        {
            return false;
        }
        return true;
    };
    const auto selectionBytes = [&addBytes](
        const editor::Selection& selection,
        std::uint64_t& total) noexcept
    {
        // The standard containers do not expose node capacity. Charge each
        // node plus a conservative allocator/link allowance so the attachment
        // budget remains an actual upper bound rather than a wire-size proxy.
        constexpr std::uint64_t kSetNodeAllowance = 64u;
        return addBytes(total, selection.sectors.size(), sizeof(std::int32_t) + kSetNodeAllowance)
            && addBytes(total, selection.surfaces.size(), sizeof(SurfaceRef) + kSetNodeAllowance)
            && addBytes(total, selection.surfaceVertices.size(), sizeof(editor::SurfaceVertexRef) + kSetNodeAllowance)
            && addBytes(total, selection.things.size(), sizeof(std::int32_t) + kSetNodeAllowance)
            && addBytes(total, selection.frames.size(), sizeof(level::FrameRef) + kSetNodeAllowance)
            && addBytes(total, selection.lights.size(), sizeof(std::int32_t) + kSetNodeAllowance)
            && addBytes(total, selection.cogs.size(), sizeof(std::int32_t) + kSetNodeAllowance);
    };

    try
    {
        const GlView::CleaveWorkspaceFamily family =
            glView_->cleaveWorkspaceFamily();
        std::uint64_t projectedAttachmentBytes = sizeof(CleaveApplyWorkspaceMemento);
        if (!addBytes(projectedAttachmentBytes, 1u,
                static_cast<std::uint64_t>(
                    glView_->stagedCleaveWorkspaceOwnedBytesEstimate(family)))
            || !selectionBytes(document_.selection(), projectedAttachmentBytes)
            || !canFitCleaveWorkspaceAttachmentBudget(
                projectedAttachmentBytes, true))
        {
            return std::nullopt;
        }

        CleaveApplyWorkspaceMemento memento{};
        memento.family = family;
        memento.documentGeneration = documentLifetimeGeneration_;
        memento.draft = glView_->captureStagedCleaveDraftState();
        memento.history = glView_->captureStagedCleaveHistory();
        if (memento.draft.lines.empty())
        {
            return std::nullopt;
        }
        if (!memento.history)
        {
            GlView::RecoveryState::CleaveHistory baseline{};
            baseline.snapshots.push_back(memento.draft);
            baseline.cursor = 0u;
            memento.history = std::move(baseline);
        }
        if (memento.history
            && (memento.history->snapshots.empty()
                || memento.history->snapshots.size() > kMaximumAttachmentCount
                || memento.history->cursor >= memento.history->snapshots.size()))
        {
            return std::nullopt;
        }

        memento.bounded = glView_->cleaveBounded();
        memento.stagedMode = glView_->cleaveStagedMode();
        memento.chainEnabled = glView_->cleaveChainEnabled();
        memento.measurementsVisible = glView_->cleaveMeasurementsVisible();
        memento.measureUnit = glView_->cleaveMeasureUnit();
        memento.anglesVisible = glView_->cleaveAnglesVisible();
        memento.lengthsVisible = glView_->cleaveLengthsVisible();
        memento.lineNumbersVisible = glView_->cleaveLineNumbersVisible();
        memento.angleStepDegrees = glView_->cleaveAngleStepDegrees();
        memento.shape = glView_->cleaveShapeOptions();
        memento.transformMode = glView_->stagedCleaveTransformMode();
        memento.sectorToolEnabled = glView_->sectorCleaveToolEnabled();
        memento.surfaceToolEnabled = glView_->surfaceCleaveToolEnabled();
        memento.documentSelection = document_.selection();
        memento.activeSector = glView_->activeSectorIndex();
        memento.activeSurfaceSector = glView_->activeSurfaceSectorIndex();
        memento.activeSurface = glView_->activeSurfaceIndex();
        memento.activeThing = glView_->activeThingIndex();
        memento.activeLight = glView_->activeLightIndex();
        const std::int32_t candidateFrameThing = glView_->activeThingIndex();
        const std::int32_t candidateFrameValue =
            glView_->activeFrameValueIndex();
        if (candidateFrameThing >= 0 && candidateFrameValue >= 0
            && document_.selection().frames.contains(level::FrameRef{
                candidateFrameThing, candidateFrameValue }))
        {
            memento.activeFrameThing = candidateFrameThing;
            memento.activeFrameValue = candidateFrameValue;
        }
        memento.editMode = editMode_;
        memento.surfaceSelectionMode = surfaceSelectionMode_;
        memento.lightEditSubMode = lightEditSubMode_;

        std::uint64_t ownedBytes = sizeof(memento);
        if (!snapshotBytes(memento.draft, ownedBytes)
            || !selectionBytes(memento.documentSelection, ownedBytes))
        {
            return std::nullopt;
        }
        if (memento.history)
        {
            if (!addBytes(ownedBytes, 1u, sizeof(GlView::RecoveryState::CleaveHistory))
                || !addBytes(ownedBytes, memento.history->snapshots.capacity(),
                    sizeof(GlView::RecoveryState::CleaveDraftSnapshot)))
            {
                return std::nullopt;
            }
            for (const auto& snapshot : memento.history->snapshots)
            {
                if (!snapshotBytes(snapshot, ownedBytes))
                {
                    return std::nullopt;
                }
            }
        }
        if (ownedBytes > kMaximumAttachmentBytes)
        {
            return std::nullopt;
        }
        memento.ownedBytes = ownedBytes;
        if (ownedBytes > projectedAttachmentBytes
            && !canFitCleaveWorkspaceAttachmentBudget(ownedBytes, true))
        {
            return std::nullopt;
        }
        // Reserve only after all semantic and aggregate-budget checks pass.
        // A refused Apply must not trim or otherwise mutate existing history
        // attachments, and successful commit can publish without allocation.
        if (cleaveApplyWorkspaceMementos_.capacity()
            < cleaveApplyWorkspaceMementos_.size() + 1u)
        {
            cleaveApplyWorkspaceMementos_.reserve(
                cleaveApplyWorkspaceMementos_.size() + 1u);
        }
        return memento;
    }
    catch (...)
    {
        return std::nullopt;
    }
}

void MainWindow::recordCleaveApplyWorkspaceMemento(
    CleaveApplyWorkspaceMemento memento,
    const std::uint64_t recordId)
{
    if (recordId == 0u || memento.ownedBytes == 0u)
    {
        return;
    }
    memento.recordId = recordId;
    memento.documentGeneration = documentLifetimeGeneration_;
    const auto existing = std::find_if(
        cleaveApplyWorkspaceMementos_.begin(),
        cleaveApplyWorkspaceMementos_.end(),
        [recordId](const CleaveApplyWorkspaceMemento& value)
        {
            return value.recordId == recordId;
        });
    if (existing != cleaveApplyWorkspaceMementos_.end())
    {
        *existing = std::move(memento);
    }
    else
    {
        cleaveApplyWorkspaceMementos_.push_back(std::move(memento));
    }
    // The prepared workspace and vector slot were allocated before Apply.
    // Do only allocation-free stale-record pruning after the commit.
    pruneCleaveApplyWorkspaceMementos();
}

std::optional<MainWindow::CleaveApplyWorkspaceMemento>
MainWindow::prepareCleaveApplyWorkspaceRestore(
    const std::uint64_t recordId) const
{
    const auto found = std::find_if(
        cleaveApplyWorkspaceMementos_.begin(),
        cleaveApplyWorkspaceMementos_.end(),
        [recordId, this](const CleaveApplyWorkspaceMemento& value)
        {
            return value.recordId == recordId
                && value.documentGeneration == documentLifetimeGeneration_;
        });
    if (found == cleaveApplyWorkspaceMementos_.end())
    {
        return std::nullopt;
    }
    if (!canFitCleaveWorkspaceAttachmentBudget(found->ownedBytes, false))
    {
        return std::nullopt;
    }
    try
    {
        // All vector/set allocations happen while the Document cursor still
        // points at the post-Apply Level. The original remains immutable in
        // the sidecar for repeated Undo/Redo and History navigation.
        return *found;
    }
    catch (...)
    {
        return std::nullopt;
    }
}

bool MainWindow::restoreCleaveApplyWorkspaceAfterUndo(
    const std::uint64_t recordId,
    CleaveApplyWorkspaceMemento prepared)
{
    const auto found = std::find_if(
        cleaveApplyWorkspaceMementos_.begin(),
        cleaveApplyWorkspaceMementos_.end(),
        [recordId, this](const CleaveApplyWorkspaceMemento& value)
        {
            return value.recordId == recordId
                && value.documentGeneration == documentLifetimeGeneration_;
        });
    if (found == cleaveApplyWorkspaceMementos_.end() || !glView_
        || prepared.recordId != recordId
        || prepared.documentGeneration != documentLifetimeGeneration_)
    {
        return false;
    }

    pendingCleaveApplyRedoRecordId_.reset();
    const QScopedValueRollback<bool> restoreGuard(
        recoveryEditorModeRestoreInProgress_, true);
    // GlView publishes viewportStateChanged while the exact authored draft
    // and local timeline are being installed. The normal Cleave overlay reads
    // its QAction and option widgets on that signal, so hold that contextual
    // synchronizer until the whole memento is coherent, then perform one
    // final sync below.
    const bool viewportRestoreWasInProgress =
        recoveryViewportStateRestoreInProgress_;
    const QScopedValueRollback<bool> restoreViewportGuard(
        recoveryViewportStateRestoreInProgress_, true);
    setLightEditSubMode(prepared.lightEditSubMode);
    setSurfaceSelectionMode(prepared.surfaceSelectionMode);
    setEditMode(prepared.editMode);
    document_.selection() = std::move(prepared.documentSelection);
    glView_->setSelectionState(
        prepared.activeSurface >= 0
            ? prepared.activeSurfaceSector : prepared.activeSector,
        prepared.activeSurface,
        prepared.activeThing,
        prepared.activeLight,
        document_.selection());
    if (prepared.activeFrameValue >= 0)
    {
        glView_->setSelectedFrameValueIndex(prepared.activeFrameValue);
    }
    else
    {
        glView_->clearFrameSelection();
    }

    // Publish the owning QAction state without allowing its ordinary toggle
    // handlers to clear the just-restored workspace midway through setup.
    if (viewportSectorCleaveAction_)
    {
        const QSignalBlocker blocker(viewportSectorCleaveAction_);
        viewportSectorCleaveAction_->setChecked(prepared.sectorToolEnabled);
    }
    if (viewportSurfaceCleaveAction_)
    {
        const QSignalBlocker blocker(viewportSurfaceCleaveAction_);
        viewportSurfaceCleaveAction_->setChecked(prepared.surfaceToolEnabled);
    }
    glView_->setSectorCleaveToolEnabled(prepared.sectorToolEnabled);
    glView_->setSurfaceCleaveToolEnabled(prepared.surfaceToolEnabled);
    glView_->setCleaveStagedMode(prepared.stagedMode);
    glView_->setCleaveChainEnabled(prepared.chainEnabled);
    if (prepared.bounded)
    {
        glView_->setCleaveBounded(*prepared.bounded);
    }
    glView_->setCleaveMeasurementsVisible(prepared.measurementsVisible);
    glView_->setCleaveMeasureUnit(prepared.measureUnit);
    glView_->setCleaveAnglesVisible(prepared.anglesVisible);
    glView_->setCleaveLengthsVisible(prepared.lengthsVisible);
    glView_->setCleaveLineNumbersVisible(prepared.lineNumbersVisible);
    glView_->setCleaveAngleStepDegrees(prepared.angleStepDegrees);
    glView_->setCleaveShapeOptions(prepared.shape);
    if (prepared.history)
    {
        glView_->restoreStagedCleaveDraftState(
            std::move(prepared.draft), std::move(*prepared.history));
    }
    else
    {
        glView_->restoreStagedCleaveDraftState(std::move(prepared.draft));
    }
    // Rotate and Scale admission depends on the restored line selection.
    // Apply has consumed that selection, so restore the draft before the
    // ordinary validity-checked setter can restore its saved mode.
    glView_->setStagedCleaveTransformMode(prepared.transformMode);

    // The option widgets are stateful inputs to syncCleaveOptionsOverlay().
    // Mirror the restored document memento into them before that contextual
    // refresh, or a stale Multi-cut/Bounded/unit choice can overwrite the
    // restored GlView values.
    if (viewportCleaveStagedButton_)
    {
        const QSignalBlocker blocker(viewportCleaveStagedButton_);
        viewportCleaveStagedButton_->setChecked(prepared.stagedMode);
    }
    if (viewportCleaveChainButton_)
    {
        const QSignalBlocker blocker(viewportCleaveChainButton_);
        viewportCleaveChainButton_->setChecked(prepared.chainEnabled);
    }
    if (viewportCleaveBoundedButton_ && prepared.bounded)
    {
        const QSignalBlocker blocker(viewportCleaveBoundedButton_);
        viewportCleaveBoundedButton_->setChecked(*prepared.bounded);
    }
    if (viewportCleaveUnitsCombo_)
    {
        const int index = viewportCleaveUnitsCombo_->findData(
            static_cast<int>(prepared.measureUnit));
        if (index >= 0)
        {
            const QSignalBlocker blocker(viewportCleaveUnitsCombo_);
            viewportCleaveUnitsCombo_->setCurrentIndex(index);
        }
    }
    if (viewportCleaveShowAnglesButton_)
    {
        const QSignalBlocker blocker(viewportCleaveShowAnglesButton_);
        viewportCleaveShowAnglesButton_->setChecked(prepared.anglesVisible);
    }
    if (viewportCleaveShowLengthsButton_)
    {
        const QSignalBlocker blocker(viewportCleaveShowLengthsButton_);
        viewportCleaveShowLengthsButton_->setChecked(prepared.lengthsVisible);
    }
    if (viewportCleaveShowNumbersButton_)
    {
        const QSignalBlocker blocker(viewportCleaveShowNumbersButton_);
        viewportCleaveShowNumbersButton_->setChecked(prepared.lineNumbersVisible);
    }
    if (viewportCleaveAngleStepSpin_)
    {
        const QSignalBlocker blocker(viewportCleaveAngleStepSpin_);
        viewportCleaveAngleStepSpin_->setValue(prepared.angleStepDegrees);
    }
    pendingCleaveApplyRedoRecordId_ = recordId;
    pendingCleaveApplyRedoFamily_ = prepared.family;
    pendingCleaveApplyRedoAuthoredRevision_ =
        glView_->stagedCleaveAuthoredRevision(prepared.family);
    recoveryViewportStateRestoreInProgress_ = viewportRestoreWasInProgress;
    syncCleaveOptionsOverlay();
    syncOutlinerSelectionOnly();
    rebuildProperties();
    syncUvEditorPanel();
    updateEditActions();
    syncCogScriptEditorActionFocus();
    return true;
}

bool MainWindow::clearCleaveApplyWorkspaceAfterRedo(
    const std::uint64_t recordId)
{
    const std::optional<GlView::CleaveWorkspaceFamily> family =
        cleaveApplyWorkspaceFamilyForRecord(recordId);
    if ( !family || !glView_ )
    {
        return false;
    }
    pendingCleaveApplyRedoRecordId_.reset();
    pendingCleaveApplyRedoAuthoredRevision_ = 0u;
    glView_->cancelStagedCleaveWorkspace(*family);
    QAction* const action = *family == GlView::CleaveWorkspaceFamily::Sector
        ? viewportSectorCleaveAction_ : viewportSurfaceCleaveAction_;
    if ( action && action->isChecked() )
    {
        action->setChecked(false);
    }
    return true;
}

bool MainWindow::restoreCleaveApplyWorkspaceAtCurrentCursor()
{
    if (const std::optional<std::uint64_t> nextRedo = document_.nextRedoRecordId())
    {
        std::optional<CleaveApplyWorkspaceMemento> prepared =
            prepareCleaveApplyWorkspaceRestore(*nextRedo);
        return prepared
            && restoreCleaveApplyWorkspaceAfterUndo(*nextRedo,
                std::move(*prepared));
    }
    return false;
}

bool MainWindow::hasPairedCleaveApplyRedo() const noexcept
{
    if (!glView_ || !pendingCleaveApplyRedoRecordId_
        || glView_->stagedCleaveAuthoredRevision(pendingCleaveApplyRedoFamily_)
            != pendingCleaveApplyRedoAuthoredRevision_)
    {
        return false;
    }
    const std::optional<std::uint64_t> nextRedo = document_.nextRedoRecordId();
    if (!nextRedo || *nextRedo != *pendingCleaveApplyRedoRecordId_)
    {
        return false;
    }
    return std::any_of(
        cleaveApplyWorkspaceMementos_.begin(),
        cleaveApplyWorkspaceMementos_.end(),
        [this, nextRedo](const CleaveApplyWorkspaceMemento& value)
        {
            return value.recordId == *nextRedo
                && value.documentGeneration == documentLifetimeGeneration_
                && value.family == pendingCleaveApplyRedoFamily_;
        });
}

std::optional<GlView::CleaveWorkspaceFamily>
MainWindow::cleaveApplyWorkspaceFamilyForRecord(
    const std::uint64_t recordId) const noexcept
{
    const auto found = std::find_if(
        cleaveApplyWorkspaceMementos_.begin(),
        cleaveApplyWorkspaceMementos_.end(),
        [this, recordId](const CleaveApplyWorkspaceMemento& value)
        {
            return value.recordId == recordId
                && value.documentGeneration == documentLifetimeGeneration_;
        });
    return found == cleaveApplyWorkspaceMementos_.end()
        ? std::nullopt : std::optional{ found->family };
}

bool MainWindow::cleaveApplyDocumentRedoHasPriority() const noexcept
{
    if ( !glView_ || !glView_->stagedCleaveHistoryOwnsUndo() )
    {
        return false;
    }
    const std::optional<std::uint64_t> nextRedo = document_.nextRedoRecordId();
    const auto family = nextRedo
        ? cleaveApplyWorkspaceFamilyForRecord(*nextRedo) : std::nullopt;
    if ( !family )
    {
        return false;
    }
    if ( glView_->cleaveWorkspaceFamily() == *family )
    {
        return hasPairedCleaveApplyRedo();
    }
    return !glView_->canRedoStagedCleaveHistory();
}

bool MainWindow::canFitCleaveWorkspaceAttachmentBudget(
    const std::uint64_t additionalBytes,
    const bool addsTransition) const noexcept
{
    constexpr std::uint64_t kMaximumAttachmentBytes = 16ull * 1024ull * 1024ull;
    constexpr std::size_t kMaximumAttachmentCount = 64u;
    std::uint64_t totalBytes = additionalBytes;
    std::size_t entryCount = addsTransition ? 1u : 0u;
    const auto addBytes = [&totalBytes](const std::uint64_t count,
                                        const std::uint64_t elementBytes) noexcept
    {
        if (elementBytes != 0u
            && count > (std::numeric_limits<std::uint64_t>::max() - totalBytes)
                / elementBytes)
        {
            totalBytes = std::numeric_limits<std::uint64_t>::max();
            return false;
        }
        totalBytes += count * elementBytes;
        return true;
    };
    if (glView_
        && !addBytes(static_cast<std::uint64_t>(
                glView_->stagedCleaveHistoryStorageBytesEstimate()), 1u))
    {
        return false;
    }
    const auto layerBytes = [](const LayerVisualHistoryTransition& transition) noexcept
    {
        std::uint64_t bytes = sizeof(transition) + 64u;
        const auto add = [&bytes](const std::uint64_t count,
                                  const std::uint64_t elementBytes) noexcept
        {
            if (elementBytes != 0u
                && count > (std::numeric_limits<std::uint64_t>::max() - bytes)
                    / elementBytes)
            {
                bytes = std::numeric_limits<std::uint64_t>::max();
                return;
            }
            bytes += count * elementBytes;
        };
        const auto bitVectorBytes = [](const std::vector<bool>& values) noexcept
        {
            return static_cast<std::uint64_t>(values.capacity() / 8u
                + (values.capacity() % 8u != 0u ? 1u : 0u));
        };
        add(bitVectorBytes(transition.before.visible), 1u);
        add(bitVectorBytes(transition.before.locked), 1u);
        add(transition.before.colors.capacity(), sizeof(QColor));
        add(bitVectorBytes(transition.after.visible), 1u);
        add(bitVectorBytes(transition.after.locked), 1u);
        add(transition.after.colors.capacity(), sizeof(QColor));
        return bytes;
    };

    for (std::size_t index = 0;
         index < cleaveApplyWorkspaceMementos_.size(); ++index)
    {
        const CleaveApplyWorkspaceMemento& memento =
            cleaveApplyWorkspaceMementos_[index];
        if (!addBytes(memento.ownedBytes, 1u))
        {
            return false;
        }
        bool seen = false;
        for (std::size_t previous = 0; previous < index; ++previous)
        {
            if (cleaveApplyWorkspaceMementos_[previous].recordId
                == memento.recordId)
            {
                seen = true;
                break;
            }
        }
        if (!seen)
        {
            for (const auto& [signature, transition] : layerVisualHistoryByAfterSignature_)
            {
                (void)signature;
                if (transition.recordId == memento.recordId)
                {
                    seen = true;
                    break;
                }
            }
        }
        if (!seen)
        {
            ++entryCount;
        }
    }

    for (auto iterator = layerVisualHistoryByAfterSignature_.begin();
         iterator != layerVisualHistoryByAfterSignature_.end();
         ++iterator)
    {
        if (!addBytes(layerBytes(iterator->second), 1u))
        {
            return false;
        }
        bool seen = std::any_of(
            cleaveApplyWorkspaceMementos_.begin(),
            cleaveApplyWorkspaceMementos_.end(),
            [&iterator](const CleaveApplyWorkspaceMemento& memento)
            {
                return memento.recordId == iterator->second.recordId;
            });
        if (!seen)
        {
            for (auto previous = layerVisualHistoryByAfterSignature_.begin();
                 previous != iterator; ++previous)
            {
                if (previous->second.recordId == iterator->second.recordId)
                {
                    seen = true;
                    break;
                }
            }
        }
        if (!seen)
        {
            ++entryCount;
        }
    }
    return entryCount <= kMaximumAttachmentCount
        && totalBytes <= kMaximumAttachmentBytes;
}

void MainWindow::pruneCleaveApplyWorkspaceMementos()
{
    const std::size_t totalDepth = document_.undoDepth() + document_.redoDepth();
    const auto isLiveRecord = [this, totalDepth](const std::uint64_t id)
    {
        for (std::size_t depth = 1; depth <= totalDepth; ++depth)
        {
            const std::optional<std::uint64_t> liveId =
                document_.historyRecordIdAtDepth(depth);
            if (liveId && *liveId == id)
            {
                return true;
            }
        }
        return false;
    };

    std::erase_if(cleaveApplyWorkspaceMementos_,
        [this, &isLiveRecord](const CleaveApplyWorkspaceMemento& value)
        {
            return value.documentGeneration != documentLifetimeGeneration_
                || !isLiveRecord(value.recordId);
        });
    for (auto iterator = layerVisualHistoryByAfterSignature_.begin();
         iterator != layerVisualHistoryByAfterSignature_.end();)
    {
        if (!isLiveRecord(iterator->second.recordId))
        {
            const std::uint64_t before = iterator->second.beforeSignature;
            const std::uint64_t after = iterator->second.afterSignature;
            if (const auto reverse = layerVisualHistoryAfterByBeforeSignature_.find(before);
                reverse != layerVisualHistoryAfterByBeforeSignature_.end()
                && reverse->second == after)
            {
                layerVisualHistoryAfterByBeforeSignature_.erase(reverse);
            }
            iterator = layerVisualHistoryByAfterSignature_.erase(iterator);
        }
        else
        {
            ++iterator;
        }
    }

}

void MainWindow::rebuildHistoryView(const bool force)
{
    if (!history_ || (!history_->isVisible() && !force))
    {
        // History is opt-in and normally hidden. Building every historical
        // row after each compact transform made arrow clicks increasingly
        // expensive even though no timeline was on screen. Its dock refreshes
        // once when it becomes visible.
        return;
    }

    const std::vector<std::string> undoLabels = document_.undoLabels();
    const std::vector<std::string> redoLabels = document_.redoLabels();
    const std::size_t currentDepth = undoLabels.size();

    const QSignalBlocker blocker(history_);
    const std::size_t desiredRowCount = undoLabels.size() + redoLabels.size() + 1;
    const bool recreateRows = history_->topLevelItemCount()
        != static_cast<int>(desiredRowCount);
    if (recreateRows)
    {
        history_->clear();
    }

    int row = 0;
    auto addEntry = [this, &row, recreateRows](
        const std::size_t targetDepth,
        const QString& text,
        const bool isFuture,
        const bool isCurrent)
    {
        if (!history_)
        {
            return;
        }

        QTreeWidgetItem* item = recreateRows
            ? new QTreeWidgetItem(history_)
            : history_->topLevelItem(row);
        if (!item)
        {
            return;
        }
        ++row;
        item->setText(0, text);
        item->setData(0, Qt::UserRole, QVariant::fromValue<qulonglong>(static_cast<qulonglong>(targetDepth)));

        // Reused rows can have been current/future in the previous cursor
        // position. Reset presentation state before applying the new role.
        item->setForeground(0, QBrush());
        QFont font = item->font(0);
        font.setBold(false);
        item->setFont(0, font);

        if (isFuture)
        {
            item->setForeground(0, QBrush(QColor(145, 145, 145)));
        }

        if (isCurrent)
        {
            font.setBold(true);
            item->setFont(0, font);
            item->setForeground(0, QBrush(QColor(220, 235, 255)));
        }
    };

    addEntry(0, "Start", false, currentDepth == 0);

    for (std::size_t i = 0; i < undoLabels.size(); ++i)
    {
        const std::size_t depth = i + 1;
        addEntry(depth, QString::fromStdString(undoLabels[i]), false, depth == currentDepth);
    }

    for (std::size_t i = 0; i < redoLabels.size(); ++i)
    {
        const std::size_t depth = currentDepth + i + 1;
        addEntry(depth, QString::fromStdString(redoLabels[i]), true, false);
    }

    for (int row = 0; row < history_->topLevelItemCount(); ++row)
    {
        QTreeWidgetItem *item = history_->topLevelItem(row);
        if (!item)
        {
            continue;
        }

        const std::size_t depth = static_cast<std::size_t>(item->data(0, Qt::UserRole).toULongLong());
        if (depth == currentDepth)
        {
            history_->setCurrentItem(item);
            // A row click is followed by Qt's second-click/double-click
            // event at the original viewport coordinate.  Do not move that
            // row while reconciling an unchanged timeline; the first click
            // already selected it and re-centering would make the second
            // event target a different row.  Initial construction (or a
            // changed row count) may still center the current checkpoint.
            if (recreateRows)
            {
                history_->scrollToItem(item, QAbstractItemView::PositionAtCenter);
            }
            break;
        }
    }
}

void MainWindow::populateHistoryContextMenu(
    QMenu* const menu,
    const std::size_t targetUndoDepth)
{
    if (!menu)
    {
        return;
    }
    // QMenu defaults to hiding QAction tooltips. History actions carry
    // concise explanations because their effects differ (camera-only versus
    // changing the selection), so make those descriptions reachable from the
    // actual RMB menu as well as from accessibility/property inspection.
    menu->setToolTipsVisible(true);
    QAction* const goToSelect = menu->addAction(
        QStringLiteral("Go To & Select"));
    goToSelect->setToolTip(
        QStringLiteral("Move the viewport to the changed entity and select it."));
    QAction* const goTo = menu->addAction(QStringLiteral("Go To"));
    goTo->setToolTip(
        QStringLiteral("Move the viewport to the changed entity without changing the current selection."));
    goToSelect->setEnabled(documentEditingAllowed());
    QObject::connect(goToSelect, &QAction::triggered, this,
        [this, targetUndoDepth]()
        {
            (void)goToHistoryDepth(targetUndoDepth, true);
        });
    QObject::connect(goTo, &QAction::triggered, this,
        [this, targetUndoDepth]()
        {
            (void)goToHistoryDepth(targetUndoDepth, false);
        });
}

#ifdef SED_APP_MAIN_WINDOW_TEST_ACCESS
bool MainWindow::emitHistoryClickForTest(
    const std::size_t targetUndoDepth)
{
    if (!history_)
    {
        return false;
    }
    rebuildHistoryView(true);
    for (int row = 0; row < history_->topLevelItemCount(); ++row)
    {
        QTreeWidgetItem* const item = history_->topLevelItem(row);
        if (item && static_cast<std::size_t>(
                item->data(0, Qt::UserRole).toULongLong()) == targetUndoDepth)
        {
            // Match QTreeWidget's production ordering: it moves the current
            // row first, then emits itemClicked.
            history_->setCurrentItem(item);
            Q_EMIT history_->itemClicked(item, 0);
            return true;
        }
    }
    return false;
}

void MainWindow::emitHistoryDoubleClickForTest(
    const std::size_t targetUndoDepth)
{
    if (!history_)
    {
        return;
    }
    const bool wasVisible = history_->isVisible();
    if (!wasVisible)
    {
        history_->show();
    }
    rebuildHistoryView(true);
    for (int row = 0; row < history_->topLevelItemCount(); ++row)
    {
        QTreeWidgetItem* const item = history_->topLevelItem(row);
        if (item && static_cast<std::size_t>(
                item->data(0, Qt::UserRole).toULongLong()) == targetUndoDepth)
        {
            Q_EMIT history_->itemDoubleClicked(item, 0);
            break;
        }
    }
    if (!wasVisible)
    {
        history_->hide();
    }
}

bool MainWindow::dispatchHistoryMouseDoubleClickForTest(
    const std::size_t targetUndoDepth)
{
    if (!history_)
    {
        return false;
    }
    const bool wasWindowVisible = isVisible();
    if (!wasWindowVisible)
    {
        // A hidden MainWindow keeps its dock children non-visible even when
        // the tree itself is shown.  The test seam needs real QWidget event
        // delivery so the first click can exercise the production rebuild.
        show();
    }
    QDockWidget* const historyDock = qobject_cast<QDockWidget*>(
        history_->parentWidget());
    const bool wasDockVisible = historyDock && historyDock->isVisible();
    if (historyDock && !wasDockVisible)
    {
        historyDock->show();
    }
    const bool wasVisible = history_->isVisible();
    if (!wasVisible)
    {
        history_->show();
    }
    const auto restoreVisibility = [&]()
    {
        if (!wasVisible)
        {
            history_->hide();
        }
        if (historyDock && !wasDockVisible)
        {
            historyDock->hide();
        }
        if (!wasWindowVisible)
        {
            hide();
        }
    };
    history_->resize(640, 480);
    rebuildHistoryView(true);

    QTreeWidgetItem* target = nullptr;
    for (int row = 0; row < history_->topLevelItemCount(); ++row)
    {
        QTreeWidgetItem* const item = history_->topLevelItem(row);
        if (item && static_cast<std::size_t>(
                item->data(0, Qt::UserRole).toULongLong()) == targetUndoDepth)
        {
            target = item;
            break;
        }
    }
    if (!target)
    {
        restoreVisibility();
        return false;
    }

    // Put a deliberately non-current row under the cursor first.  The
    // following left click changes the current depth; rebuildHistoryView must
    // not then re-center it before Qt delivers MouseButtonDblClick.
    history_->scrollToItem(target, QAbstractItemView::PositionAtTop);
    QCoreApplication::processEvents(QEventLoop::AllEvents);
    const QPoint position = history_->visualItemRect(target).center();
    QWidget* const viewport = history_->viewport();
    if (!viewport || !viewport->rect().contains(position))
    {
        restoreVisibility();
        return false;
    }

    auto sendMouse = [viewport, position](
        const QEvent::Type type,
        const Qt::MouseButton button,
        const Qt::MouseButtons buttons)
    {
        QMouseEvent event(
            type,
            QPointF(position),
            button,
            buttons,
            Qt::NoModifier);
        return QApplication::sendEvent(viewport, &event);
    };
    const bool sent = sendMouse(
        QEvent::MouseButtonPress, Qt::LeftButton, Qt::LeftButton)
        && sendMouse(
            QEvent::MouseButtonRelease, Qt::LeftButton, Qt::NoButton)
        && sendMouse(
            QEvent::MouseButtonDblClick, Qt::LeftButton, Qt::LeftButton)
        && sendMouse(
            QEvent::MouseButtonRelease, Qt::LeftButton, Qt::NoButton);
    QCoreApplication::processEvents(QEventLoop::AllEvents);
    restoreVisibility();
    return sent;
}

QStringList MainWindow::historyContextMenuActionTextsForTest(
    const std::size_t targetUndoDepth)
{
    ui::shortcuts::KeySequenceDisplayMenu menu{};
    populateHistoryContextMenu(&menu, targetUndoDepth);
    QStringList result{};
    for (QAction* const action : menu.actions())
    {
        if (action)
        {
            result.push_back(action->text());
        }
    }
    return result;
}

QStringList MainWindow::historyContextMenuActionToolTipsForTest(
    const std::size_t targetUndoDepth)
{
    ui::shortcuts::KeySequenceDisplayMenu menu{};
    populateHistoryContextMenu(&menu, targetUndoDepth);
    QStringList result{};
    for (QAction* const action : menu.actions())
    {
        if (action)
        {
            result.push_back(action->toolTip());
        }
    }
    return result;
}

bool MainWindow::historyContextMenuToolTipsVisibleForTest(
    const std::size_t targetUndoDepth)
{
    ui::shortcuts::KeySequenceDisplayMenu menu{};
    populateHistoryContextMenu(&menu, targetUndoDepth);
    return menu.toolTipsVisible();
}

bool MainWindow::triggerHistoryContextActionForTest(
    const std::size_t targetUndoDepth,
    const bool selectTarget)
{
    ui::shortcuts::KeySequenceDisplayMenu menu{};
    populateHistoryContextMenu(&menu, targetUndoDepth);
    const QList<QAction*> actions = menu.actions();
    const int actionIndex = selectTarget ? 0 : 1;
    if (actionIndex < 0 || actionIndex >= actions.size() || !actions[actionIndex])
    {
        return false;
    }

    // Queue the actual QAction signal while the temporary menu remains alive.
    // This exercises the same connection used by the RMB menu without
    // entering a blocking QMenu::exec() loop in a headless test.
    QAction* const action = actions[actionIndex];
    QMetaObject::invokeMethod(action, "trigger", Qt::QueuedConnection);
    QCoreApplication::processEvents(QEventLoop::AllEvents);
    return true;
}
#endif

void MainWindow::jumpToUndoDepth(
    const std::size_t targetUndoDepth,
    const bool handleCleaveWorkspaceMementos)
{
    if (!documentEditingAllowed())
    {
        return;
    }
    const std::size_t totalDepth = document_.undoDepth() + document_.redoDepth();
    if (targetUndoDepth > totalDepth)
    {
        return;
    }

    const std::size_t currentDepth = document_.undoDepth();
    if (targetUndoDepth == currentDepth)
    {
        return;
    }

    std::optional<CleaveApplyWorkspaceMemento> preparedCleaveWorkspace{};
    std::optional<std::uint64_t> cleaveApplyRecordId{};
    if (handleCleaveWorkspaceMementos && targetUndoDepth < currentDepth
        && targetUndoDepth < std::numeric_limits<std::size_t>::max())
    {
        const auto identity = document_.historyTransitionIdentityAtDepth(
            targetUndoDepth + 1u);
        if (identity)
        {
            const bool hasAttachment = std::any_of(
                cleaveApplyWorkspaceMementos_.begin(),
                cleaveApplyWorkspaceMementos_.end(),
                [&identity, this](const CleaveApplyWorkspaceMemento& value)
                {
                    return value.recordId == identity->recordId
                        && value.documentGeneration == documentLifetimeGeneration_;
                });
            if (hasAttachment)
            {
                cleaveApplyRecordId = identity->recordId;
                preparedCleaveWorkspace = prepareCleaveApplyWorkspaceRestore(
                    *cleaveApplyRecordId);
                if (!preparedCleaveWorkspace)
                {
                    return;
                }
            }
        }
    }
    const bool undoing = targetUndoDepth < currentDepth;
    const std::size_t steps = undoing
        ? currentDepth - targetUndoDepth
        : targetUndoDepth - currentDepth;

    std::vector<std::int64_t> surfaceTopologyAtJumpStart{};
    const bool trackSurfaceTopology =
        hasSurfaceSelectionContextForTopologyInvalidation();
    bool mixedTopologyPreflight = false;
    std::optional<std::size_t> ambiguousTopologyChangedAtStep{};
    bool topologyPreparationFailed = false;
    if (trackSurfaceTopology && steps > 1u)
    {
        try
        {
            std::vector<std::uint32_t> transitionDirtyFlags{};
            transitionDirtyFlags.reserve(steps);
            bool sawKnownTopologyTransition = false;
            bool hasKnownThenAmbiguousTransition = false;
            for (std::size_t step = 0; step < steps; ++step)
            {
                const std::size_t transitionDepth = undoing
                    ? currentDepth - step
                    : currentDepth + step + 1u;
                const std::optional<editor::DocumentHistoryTransitionState>
                    transition = document_.transitionAtDepth(transitionDepth);
                const std::uint32_t dirtyFlags = transition
                    ? transition->dirtyFlags
                    : static_cast<std::uint32_t>(editor::DocumentDirty::None);
                transitionDirtyFlags.push_back(dirtyFlags);
                const bool knownTopologyTransition = transition
                    && ui::history_detail::isKnownTopologyHistoryTransition(
                        dirtyFlags);
                const bool mayNeedTopologyComparison =
                    !knownTopologyTransition
                    && ui::history_detail::historyDirtyMayChangeSurfaceTopology(
                        dirtyFlags);
                hasKnownThenAmbiguousTransition =
                    hasKnownThenAmbiguousTransition
                    || (sawKnownTopologyTransition
                        && mayNeedTopologyComparison);
                sawKnownTopologyTransition =
                    sawKnownTopologyTransition || knownTopologyTransition;
            }

            if (hasKnownThenAmbiguousTransition)
            {
                mixedTopologyPreflight = true;
                {
                    // The detached history walk is deliberately cold and
                    // conditional. Complete its copies, swaps, and exact
                    // comparisons before the live cursor can change. Keeping
                    // this scope local also releases the copied history before
                    // live Undo/Redo begins.
                    editor::Document probe = document_;
                    std::vector<std::int64_t> topologyBeforeStep{};
                    for (std::size_t step = 0; step < steps; ++step)
                    {
                        const std::uint32_t dirtyFlags =
                            transitionDirtyFlags[step];
                        const bool knownTopologyTransition =
                            ui::history_detail::isKnownTopologyHistoryTransition(
                                dirtyFlags);
                        const bool mayNeedTopologyComparison =
                            !knownTopologyTransition
                            && ui::history_detail::historyDirtyMayChangeSurfaceTopology(
                                dirtyFlags);
                        if (mayNeedTopologyComparison
                            && !captureLevelSurfaceSelectionTopologySnapshot(
                                probe.level(), topologyBeforeStep))
                        {
                            topologyPreparationFailed = true;
                            break;
                        }

                        editor::DocumentDirty probeDirty =
                            editor::DocumentDirty::None;
                        const bool applied = undoing
                            ? probe.undo(&probeDirty)
                            : probe.redo(&probeDirty);
                        if (!applied)
                        {
                            topologyPreparationFailed = true;
                            break;
                        }
                        if (mayNeedTopologyComparison
                            && !matchesLevelSurfaceSelectionTopologySnapshot(
                                probe.level(), topologyBeforeStep))
                        {
                            ambiguousTopologyChangedAtStep = step;
                            break;
                        }
                    }
                }
            }
        }
        catch (...)
        {
            topologyPreparationFailed = true;
        }
    }

    if (trackSurfaceTopology && !mixedTopologyPreflight
        && !captureSurfaceSelectionTopologySnapshot(surfaceTopologyAtJumpStart))
    {
        topologyPreparationFailed = true;
    }
    if (topologyPreparationFailed)
    {
        if (glView_)
        {
            glView_->showToast(
                "History navigation could not prepare the surface topology check; the document was left unchanged",
                3500,
                GlView::ToastPlacement::BottomCenter,
                GlView::ToastGroup::Error);
        }
        return;
    }

    // A history-row click is one cursor transition. Triggering QAction once
    // per row used to rebuild meshes, the Outliner, checks, properties, UV
    // state, and recovery publication at every intermediate checkpoint. Keep
    // the exact Document swaps and per-step layer history, then publish the
    // final state to the UI once.
    cancelArchitecturalLightingInteractiveEdit();
    const editor::Selection previousSelection = document_.selection();
    const bool hadMotsArchLighting = hasMotsArchLighting(document_.level());
    const bool hadMotsArchBakeState = hasMotsArchBakeState(document_.level());
    editor::DocumentDirty aggregateDirty = editor::DocumentDirty::None;
    bool unknownDirty = false;
    bool sawThingsTransition = false;
    bool allThingsTransitionsCompact = true;
    std::vector<std::int32_t> compactThingTargets{};
    bool layerCardinalityMayHaveChanged = false;
    bool restoredLayerVisuals = false;
    bool invalidatesCleaveTopology = false;
    bool invalidatesSurfaceSelectionContext = false;
    bool surfaceTopologyChangedDuringJump = false;
    std::size_t completedSteps = 0;

    QProgressDialog progress(this);
    progress.setObjectName(QStringLiteral("historyJumpProgress"));
    progress.setWindowTitle(QStringLiteral("Moving Through History"));
    progress.setLabelText(QStringLiteral("Applying undo/redo checkpoints..."));
    progress.setWindowFlags(
        Qt::Dialog | Qt::CustomizeWindowHint | Qt::WindowTitleHint);
    progress.setWindowModality(Qt::WindowModal);
    progress.setCancelButton(nullptr);
    progress.setRange(0, static_cast<int>(std::min<std::size_t>(
        steps, static_cast<std::size_t>(std::numeric_limits<int>::max()))));
    progress.setMinimumDuration(650);
    progress.setAutoClose(false);
    progress.setAutoReset(false);
    progress.setMinimumWidth(520);
    progress.setStyleSheet(
        "QProgressDialog { background-color: #25282d; border: 1px solid #1c1f24; border-radius: 14px; }"
        "QProgressDialog QLabel { color: #eef2f7; font-size: 14px; min-height: 28px; }"
        "QProgressBar { background-color: #1a2028; border: 1px solid #1f2c3a; border-radius: 9px; min-height: 16px; text-align: center; }"
        "QProgressBar::chunk { background-color: #2f6db1; border-radius: 8px; }");
    progress.setValue(0);
    QElapsedTimer elapsed;
    elapsed.start();

    for (std::size_t step = 0; step < steps; ++step)
    {
        const std::uint64_t fromSignature = document_.undoSignature();
        const std::optional<std::uint64_t> transitionRecordId = undoing
            ? document_.nextUndoRecordId() : document_.nextRedoRecordId();
        const bool knownCleaveApply = handleCleaveWorkspaceMementos
            && transitionRecordId
            && cleaveApplyWorkspaceFamilyForRecord(*transitionRecordId).has_value();
        editor::DocumentDirty dirty = editor::DocumentDirty::None;
        std::vector<std::int32_t> stepThingTargets{};
        const bool applied = undoing
            ? document_.undo(&dirty, &stepThingTargets)
            : document_.redo(&dirty, &stepThingTargets);
        if (!applied)
        {
            break;
        }
        const std::uint32_t rawDirtyFlags =
            static_cast<std::uint32_t>(dirty);
        if ( !undoing && knownCleaveApply )
        {
            (void)clearCleaveApplyWorkspaceAfterRedo(*transitionRecordId);
        }
        invalidatesCleaveTopology = invalidatesCleaveTopology
            || (!knownCleaveApply
                && (rawDirtyFlags
                        & static_cast<std::uint32_t>(
                            editor::DocumentDirty::LevelGeometry)) != 0u
                && (rawDirtyFlags
                        & static_cast<std::uint32_t>(
                            editor::DocumentDirty::Structure)) != 0u);
        const bool knownTopologyTransition =
            ui::history_detail::isKnownTopologyHistoryTransition(rawDirtyFlags);
        invalidatesSurfaceSelectionContext =
            invalidatesSurfaceSelectionContext || knownTopologyTransition;
        const bool dirtyMayChangeSurfaceTopology =
            ui::history_detail::historyDirtyMayChangeSurfaceTopology(
                rawDirtyFlags);
        const bool ambiguousTopologyChangedAtCurrentStep =
            mixedTopologyPreflight
            && ambiguousTopologyChangedAtStep
            && *ambiguousTopologyChangedAtStep == step;
        const bool baselineTopologyChangedAtCurrentStep =
            !mixedTopologyPreflight
            && !invalidatesSurfaceSelectionContext
            && !surfaceTopologyChangedDuringJump
            && trackSurfaceTopology
            && dirtyMayChangeSurfaceTopology
            && !ui::history_detail::isKnownTopologyHistoryTransition(
                rawDirtyFlags)
            && !matchesSurfaceSelectionTopologySnapshot(
                surfaceTopologyAtJumpStart);
        if (ambiguousTopologyChangedAtCurrentStep
            || baselineTopologyChangedAtCurrentStep)
        {
            // Exact comparisons are prepared before the live cursor changes.
            // The jump loop only consumes the first differing step and keeps
            // invalidation sticky for the remainder of this history move.
            surfaceTopologyChangedDuringJump = true;
            invalidatesSurfaceSelectionContext = true;
        }
        dirty = static_cast<editor::DocumentDirty>(
            ui::history_detail::normalizeUnsupportedHistoryDirtyFlags(
                rawDirtyFlags));

        if (editor::hasAny(dirty, editor::DocumentDirty::Things))
        {
            sawThingsTransition = true;
            if (dirty == editor::DocumentDirty::Things
                && !stepThingTargets.empty())
            {
                compactThingTargets.insert(
                    compactThingTargets.end(),
                    stepThingTargets.begin(),
                    stepThingTargets.end());
            }
            else
            {
                allThingsTransitionsCompact = false;
            }
        }

        if (dirty == editor::DocumentDirty::None)
        {
            unknownDirty = true;
        }
        else
        {
            aggregateDirty |= dirty;
        }

        if (dirty == editor::DocumentDirty::All)
        {
            layerCardinalityMayHaveChanged = true;
            const std::uint64_t toSignature = document_.undoSignature();
            restoredLayerVisuals = (undoing
                ? restoreLayerVisualHistoryForUndo(fromSignature, toSignature)
                : restoreLayerVisualHistoryForRedo(fromSignature, toSignature))
                || restoredLayerVisuals;
        }
        else
        {
            refreshLayerVisualHistoryForCurrentSignature();
        }

        completedSteps = step + 1;
        if (completedSteps == steps
            || (elapsed.elapsed() >= 250 && completedSteps % 4 == 0))
        {
            progress.setLabelText(QStringLiteral("Applying checkpoint %1 of %2...")
                .arg(static_cast<qulonglong>(completedSteps))
                .arg(static_cast<qulonglong>(steps)));
            progress.setValue(static_cast<int>(std::min<std::size_t>(
                completedSteps,
                static_cast<std::size_t>(std::numeric_limits<int>::max()))));
            if (elapsed.elapsed() >= 650)
            {
                // Let only non-input events paint the delayed modal progress
                // dialog. User actions cannot interleave a second edit with
                // this atomic cursor transition.
                QApplication::processEvents(
                    QEventLoop::ExcludeUserInputEvents
                        | QEventLoop::ExcludeSocketNotifiers,
                    5);
            }
        }
    }

    if (completedSteps == 0)
    {
        return;
    }

    // A successful History jump is local navigation. Retain a newly restored
    // token only when the target below the Apply is about to rearm it; the
    // restore helper clears any stale token and publishes the exact one.
    pendingCleaveApplyRedoRecordId_.reset();
    pendingCleaveApplyRedoAuthoredRevision_ = 0u;

    if (elapsed.elapsed() >= 350 || steps >= 64)
    {
        progress.setLabelText(QStringLiteral("Refreshing the editor..."));
        progress.show();
        QApplication::processEvents(
            QEventLoop::ExcludeUserInputEvents
                | QEventLoop::ExcludeSocketNotifiers,
            5);
    }

    if (layerCardinalityMayHaveChanged)
    {
        syncLayerViewState();
        if (restoredLayerVisuals)
        {
            persistLayerColors();
        }
    }

    auto selectionEquals = [](const editor::Selection& lhs,
                               const editor::Selection& rhs)
    {
        return lhs.sectors == rhs.sectors
            && lhs.surfaces == rhs.surfaces
            && lhs.surfaceVertices == rhs.surfaceVertices
            && lhs.things == rhs.things
            && lhs.frames == rhs.frames
            && lhs.lights == rhs.lights
            && lhs.cogs == rhs.cogs;
    };
    auto sanitizeSelection = [this](editor::Selection& selection)
    {
        const Level& level = document_.level();
        std::erase_if(selection.sectors, [&level](const std::int32_t index)
            {
                return index < 0
                    || static_cast<std::size_t>(index) >= level.sectors.size();
            });
        std::erase_if(selection.surfaces, [&level](const SurfaceRef& ref)
            {
                return ref.sector < 0 || ref.surface < 0
                    || static_cast<std::size_t>(ref.sector) >= level.sectors.size()
                    || static_cast<std::size_t>(ref.surface)
                        >= level.sectors[static_cast<std::size_t>(ref.sector)].surfaces.size();
            });
        std::erase_if(selection.surfaceVertices, [&level](const editor::SurfaceVertexRef& ref)
            {
                if (ref.surface.sector < 0 || ref.surface.surface < 0
                    || ref.localVertex < 0
                    || static_cast<std::size_t>(ref.surface.sector) >= level.sectors.size())
                {
                    return true;
                }
                const Sector& sector = level.sectors[static_cast<std::size_t>(ref.surface.sector)];
                if (static_cast<std::size_t>(ref.surface.surface) >= sector.surfaces.size())
                {
                    return true;
                }
                return static_cast<std::size_t>(ref.localVertex)
                    >= sector.surfaces[static_cast<std::size_t>(ref.surface.surface)].vertices.size();
            });
        std::erase_if(selection.things, [&level](const std::int32_t index)
            {
                return index < 0
                    || static_cast<std::size_t>(index) >= level.things.size();
            });
        std::erase_if(selection.frames, [&level](const sed::level::FrameRef& ref)
            {
                if (ref.thing < 0 || ref.value < 0
                    || static_cast<std::size_t>(ref.thing) >= level.things.size())
                {
                    return true;
                }
                const Thing& thing = level.things[static_cast<std::size_t>(ref.thing)];
                if (static_cast<std::size_t>(ref.value) >= thing.values.size())
                {
                    return true;
                }
                const TplValue& value = thing.values[static_cast<std::size_t>(ref.value)];
                return value.type != ValueType::frame
                    || !text::iequalsAscii(value.name, "frame");
            });
        std::erase_if(selection.lights, [&level](const std::int32_t index)
            {
                return index < 0
                    || static_cast<std::size_t>(index) >= level.lights.size();
            });
        std::erase_if(selection.cogs, [&level](const std::int32_t index)
            {
                return index < 0
                    || static_cast<std::size_t>(index) >= level.cogs.size();
            });
    };

    editor::Selection preservedSelection = previousSelection;
    sanitizeSelection(preservedSelection);
    document_.selection() = std::move(preservedSelection);
    if (surfaceTopologyChangedDuringJump)
    {
        // Unknown and partial dirty masks cannot vouch for the restored
        // positional face selection. Clear it before the shared invalidation
        // helper records its replacement Face context.
        document_.selection().surfaces.clear();
    }
    if (invalidatesSurfaceSelectionContext)
    {
        invalidateSurfaceSelectionContextForTopologyEdit();
    }
    const bool selectionUnchanged = selectionEquals(
        previousSelection, document_.selection());

    const bool levelChanged = editor::hasAny(
        aggregateDirty, editor::DocumentDirty::LevelGeometry);
    const bool thingsChanged = editor::hasAny(
        aggregateDirty, editor::DocumentDirty::Things);
    const bool lightsChanged = editor::hasAny(
        aggregateDirty, editor::DocumentDirty::Lights);
    const bool structureChanged = editor::hasAny(
        aggregateDirty, editor::DocumentDirty::Structure);
    std::sort(compactThingTargets.begin(), compactThingTargets.end());
    compactThingTargets.erase(
        std::unique(compactThingTargets.begin(), compactThingTargets.end()),
        compactThingTargets.end());
    const bool compactThingHistory =
        sawThingsTransition
        && allThingsTransitionsCompact
        && !compactThingTargets.empty();
    const bool archLightingMayHaveChanged =
        hadMotsArchLighting || hasMotsArchLighting(document_.level());
    const bool archBakeMayHaveChanged = hadMotsArchBakeState
        || hasMotsArchBakeState(document_.level());

    if (glView_)
    {
        if (invalidatesCleaveTopology)
        {
            const bool hadPendingCuts =
                !glView_->stagedCleaveDraftView(GlView::CleaveWorkspaceFamily::Sector).lines.empty()
                || !glView_->stagedCleaveDraftView(GlView::CleaveWorkspaceFamily::Surface).lines.empty();
            glView_->cancelAllStagedCleaveWorkspaces();
            if (hadPendingCuts)
            {
                glView_->showToast(
                    "Multi-cut: level topology changed; the pending draft was cleared",
                    3200, GlView::ToastPlacement::BottomCenter, GlView::ToastGroup::Error);
            }
        }
        if (levelChanged || unknownDirty)
        {
            uvEditorForceDisplayRebuild_ = true;
        }
        if (unknownDirty || aggregateDirty == editor::DocumentDirty::All
            || (levelChanged && thingsChanged))
        {
            glView_->notifyAllMeshesChanged();
        }
        else
        {
            if (levelChanged) glView_->notifyLevelGeometryChanged();
            if (thingsChanged)
            {
                if (compactThingHistory)
                {
                    glView_->notifyThingTransformsChanged(compactThingTargets);
                }
                else if (archLightingMayHaveChanged)
                {
                    glView_->notifyThingAppearanceChanged();
                }
                else
                {
                    glView_->notifyThingsChanged();
                }
            }
        }
        if (lightsChanged)
        {
            glView_->notifyLightsChanged();
            std::vector<std::int32_t> bakedTargets{};
            bakedTargets.reserve(document_.level().sectors.size());
            for (std::size_t index = 0; index < document_.level().sectors.size(); ++index)
            {
                bakedTargets.push_back(static_cast<std::int32_t>(index));
            }
            const bool thingAppearanceAlreadyInvalidated = unknownDirty
                || aggregateDirty == editor::DocumentDirty::All
                || (thingsChanged && !compactThingHistory);
            const bool archAppearanceNeedsRebuild = archBakeMayHaveChanged
                && !thingAppearanceAlreadyInvalidated;
            glView_->notifyBakedLightingChanged(
                bakedTargets,
                !thingAppearanceAlreadyInvalidated && !archBakeMayHaveChanged);
            if (archAppearanceNeedsRebuild)
            {
                // Lights-domain history can restore generated MotS ARCH
                // channels without changing the Thing records themselves.
                glView_->notifyThingAppearanceChanged();
            }
        }

        if (!selectionUnchanged)
        {
            const editor::Selection& selection = document_.selection();
            const auto minIndex = [](const auto& values) -> std::int32_t
            {
                return values.empty() ? -1
                    : *std::min_element(values.begin(), values.end());
            };
            const auto minSurface = [&selection]() -> SurfaceRef
            {
                SurfaceRef best{ -1, -1 };
                for (const SurfaceRef& ref : selection.surfaces)
                {
                    if (best.sector < 0 || ref.sector < best.sector
                        || (ref.sector == best.sector && ref.surface < best.surface))
                    {
                        best = ref;
                    }
                }
                return best;
            };
            std::int32_t sector = -1;
            std::int32_t surface = -1;
            std::int32_t thing = -1;
            std::int32_t light = -1;
            if (editMode_ == EditMode::surface)
            {
                const SurfaceRef ref = minSurface();
                sector = ref.sector >= 0 ? ref.sector : minIndex(selection.sectors);
                surface = ref.surface;
            }
            else if (editMode_ == EditMode::sector)
            {
                sector = !selection.sectors.empty()
                    ? minIndex(selection.sectors) : minSurface().sector;
            }
            else if (editMode_ == EditMode::thing || editMode_ == EditMode::frame)
            {
                thing = minIndex(selection.things);
            }
            else if (editMode_ == EditMode::light)
            {
                if (lightEditSubMode_ == LightEditSubMode::thing
                    || lightEditSubMode_ == LightEditSubMode::architectural)
                    thing = minIndex(selection.things);
                else if (lightEditSubMode_ == LightEditSubMode::sector)
                    sector = minIndex(selection.sectors);
                else
                    light = minIndex(selection.lights);
            }
            glView_->setSelectionState(
                sector, surface, thing, light, selection);
        }
    }

    bool rebuildOutlinerNeeded = unknownDirty;
    if (!rebuildOutlinerNeeded && structureChanged)
    {
        const int tab = outlinerTabs_ ? outlinerTabs_->currentIndex() : 0;
        rebuildOutlinerNeeded = (tab == 0 && levelChanged)
            || (tab == 1 && thingsChanged)
            || (tab == 2 && editor::hasAny(
                aggregateDirty, editor::DocumentDirty::Cogs))
            || (tab == 3 && lightsChanged);
    }
    if (rebuildOutlinerNeeded)
    {
        rebuildOutliner();
    }
    else if (!selectionUnchanged)
    {
        syncOutlinerSelectionOnly();
    }

    scheduleRebuildChecks(
        editor::DocumentDirty::LevelGeometry
        | editor::DocumentDirty::Things
        | editor::DocumentDirty::Lights
        | editor::DocumentDirty::Cogs
        | editor::DocumentDirty::Structure);
    rebuildProperties();
    updateEditActions();
    syncUvEditorPanel();
    if (completedSteps == steps && preparedCleaveWorkspace
        && cleaveApplyRecordId)
    {
        (void)restoreCleaveApplyWorkspaceAfterUndo(
            *cleaveApplyRecordId, std::move(*preparedCleaveWorkspace));
    }
    pruneCleaveApplyWorkspaceMementos();
    noteRecoveryStateChanged(true);
    progress.close();
}

bool MainWindow::goToHistoryDepth(
    const std::size_t targetUndoDepth,
    const bool selectTarget)
{
    if (targetUndoDepth == 0
        || targetUndoDepth > document_.undoDepth() + document_.redoDepth())
    {
        return false;
    }

    // Capture the transition before changing the live cursor.  The compact
    // Document history owns the authoritative before/after states; labels
    // are only presentation text and are never inspected here.
    const std::optional<editor::DocumentHistoryTransitionState> transition =
        document_.transitionAtDepth(targetUndoDepth);
    if (!transition)
    {
        return false;
    }

    const bool changeDepth = documentEditingAllowed();
    // History navigation itself is the same operation as selecting a row in
    // the existing timeline.  It preserves the current selection wherever
    // the resulting level still contains it, while also making future rows
    // available for target inference after a redo jump.
    std::optional<CleaveApplyWorkspaceMemento> preparedCleaveApply{};
    std::optional<std::uint64_t> cleaveApplyRecordId{};
    if (changeDepth)
    {
        if (targetUndoDepth < std::numeric_limits<std::size_t>::max())
        {
            const auto nextTransition = document_.historyTransitionIdentityAtDepth(
                targetUndoDepth + 1u);
            if (nextTransition)
            {
                const bool hasAttachment = std::any_of(
                    cleaveApplyWorkspaceMementos_.begin(),
                    cleaveApplyWorkspaceMementos_.end(),
                    [&nextTransition](const CleaveApplyWorkspaceMemento& value)
                    {
                        return value.recordId == nextTransition->recordId;
                    });
                if (hasAttachment)
                {
                    cleaveApplyRecordId = nextTransition->recordId;
                    preparedCleaveApply = prepareCleaveApplyWorkspaceRestore(
                        *cleaveApplyRecordId);
                    if (!preparedCleaveApply)
                    {
                        return false;
                    }
                }
            }
        }
        jumpToUndoDepth(targetUndoDepth, false);
        if (document_.undoDepth() != targetUndoDepth)
        {
            return false;
        }
    }
    const std::vector<HistoryTarget> targets = inferHistoryTargets(
        *transition, document_.level());

    if (!selectTarget || !changeDepth)
    {
        // This path is intentionally camera-only.  In particular, do not use
        // navigateToEntity(..., preserveCogContext=true): that helper updates
        // GlView's active selection even though the document selection stays
        // unchanged.  Direct bounds framing leaves both selection and the
        // current mode/tool untouched.
        if (targets.empty())
        {
            if (preparedCleaveApply && cleaveApplyRecordId)
            {
                (void)restoreCleaveApplyWorkspaceAfterUndo(
                    *cleaveApplyRecordId,
                    std::move(*preparedCleaveApply));
            }
            return true;
        }

        QVector3D minv(
            std::numeric_limits<float>::max(),
            std::numeric_limits<float>::max(),
            std::numeric_limits<float>::max());
        QVector3D maxv(
            -std::numeric_limits<float>::max(),
            -std::numeric_limits<float>::max(),
            -std::numeric_limits<float>::max());
        bool haveAny = false;
        // A history transition may select several outputs (for example a
        // multi-pair surface merge).  Keep the complete selection below, but
        // frame the deterministic primary output just like an Outliner
        // double-click frames the one item the user activated.  Combining
        // disjoint outputs would move the camera to an unrelated midpoint and
        // would no longer match the established navigation contract.
        includeTargetBounds(targets.front(), document_.level(), minv, maxv, haveAny);
        if (haveAny)
        {
            frameBoundsWithNavigationPrefs(minv, maxv, true);
        }
        if (preparedCleaveApply && cleaveApplyRecordId)
        {
            (void)restoreCleaveApplyWorkspaceAfterUndo(
                *cleaveApplyRecordId,
                std::move(*preparedCleaveApply));
        }
        return true;
    }

    if (targets.empty())
    {
        if (preparedCleaveApply && cleaveApplyRecordId)
        {
            (void)restoreCleaveApplyWorkspaceAfterUndo(
                *cleaveApplyRecordId,
                std::move(*preparedCleaveApply));
        }
        return true;
    }

    // Go to & select is an explicit selection operation.  Cancel every
    // modification/placement owner first; then arm Select so a frame target
    // is genuinely selectable even when Cleave, Join, Brush, or Add Frame was
    // active before the history action.
    cancelPlacementForViewportTool(viewportTransformSelectAction_);
    deactivateViewportEditFunctions();
    setViewportTransformActionChecks(viewportTransformSelectAction_);
    rememberedViewportTransformAction_ = viewportTransformSelectAction_;
    if (glView_)
    {
        glView_->setTransformTool(GlView::TransformTool::Select);
        glView_->setTransformInteractionEnabled(true);
    }
    syncViewportSelectionInputEnabled();

    const HistoryTargetMode mode = targets.front().mode;
    if (mode == HistoryTargetMode::frame)
    {
        setEditMode(EditMode::frame);
    }
    else if (mode == HistoryTargetMode::surfaceVertex)
    {
        setEditMode(EditMode::surface);
        setSurfaceSelectionMode(SurfaceSelectionMode::vertex);
    }
    else if (mode == HistoryTargetMode::surface)
    {
        setEditMode(EditMode::surface);
        setSurfaceSelectionMode(SurfaceSelectionMode::face);
    }
    else if (mode == HistoryTargetMode::thingLight)
    {
        setLightEditSubMode(LightEditSubMode::thing);
        setEditMode(EditMode::light);
    }
    else if (mode == HistoryTargetMode::sectorLight)
    {
        setLightEditSubMode(LightEditSubMode::sector);
        setEditMode(EditMode::light);
    }
    else if (mode == HistoryTargetMode::bakedLight)
    {
        setLightEditSubMode(LightEditSubMode::baked);
        setEditMode(EditMode::light);
    }
    else if (mode == HistoryTargetMode::thing)
    {
        if (lightEditSubMode_ == LightEditSubMode::architectural
            && document_.level().kind == ProjectType::MOTS)
        {
            setLightEditSubMode(LightEditSubMode::architectural);
            setEditMode(EditMode::light);
        }
        else
        {
            setEditMode(EditMode::thing);
        }
    }
    else
    {
        setEditMode(EditMode::sector);
    }

    editor::Selection selected{};
    for (const HistoryTarget& target : targets)
    {
        switch (target.mode)
        {
            case HistoryTargetMode::sector:
            case HistoryTargetMode::sectorLight:
                selected.sectors.insert(target.index);
                break;
            case HistoryTargetMode::surface:
                selected.surfaces.insert(SurfaceRef{ target.sector, target.surface });
                break;
            case HistoryTargetMode::surfaceVertex:
                selected.surfaceVertices.insert(editor::SurfaceVertexRef{
                    SurfaceRef{ target.sector, target.surface }, target.localVertex });
                selected.surfaces.insert(SurfaceRef{ target.sector, target.surface });
                break;
            case HistoryTargetMode::thing:
            case HistoryTargetMode::thingLight:
                selected.things.insert(target.index);
                break;
            case HistoryTargetMode::frame:
                selected.things.insert(target.index);
                selected.frames.insert(level::FrameRef{ target.index, target.value });
                break;
            case HistoryTargetMode::bakedLight:
                selected.lights.insert(target.index);
                break;
        }
    }
    document_.selection() = std::move(selected);

    std::int32_t selectedSector = -1;
    std::int32_t selectedSurface = -1;
    std::int32_t selectedThing = -1;
    std::int32_t selectedLight = -1;
    if (mode == HistoryTargetMode::surface
        || mode == HistoryTargetMode::surfaceVertex)
    {
        selectedSector = targets.front().sector;
        selectedSurface = targets.front().surface;
    }
    else if (mode == HistoryTargetMode::sector
        || mode == HistoryTargetMode::sectorLight)
    {
        selectedSector = targets.front().index;
    }
    else if (mode == HistoryTargetMode::thing
        || mode == HistoryTargetMode::thingLight
        || mode == HistoryTargetMode::frame)
    {
        selectedThing = targets.front().index;
    }
    else if (mode == HistoryTargetMode::bakedLight)
    {
        selectedLight = targets.front().index;
    }

    if (glView_)
    {
        glView_->setSelectionState(
            selectedSector, selectedSurface, selectedThing, selectedLight,
            document_.selection());
    }

    QVector3D minv(
        std::numeric_limits<float>::max(),
        std::numeric_limits<float>::max(),
        std::numeric_limits<float>::max());
    QVector3D maxv(
        -std::numeric_limits<float>::max(),
        -std::numeric_limits<float>::max(),
        -std::numeric_limits<float>::max());
    bool haveAny = false;
    // Selection remains multi-target, but navigation is anchored to the
    // primary target.  This is the same one-entity framing used by the
    // Outliner and avoids centering between unrelated merge outputs.
    includeTargetBounds(targets.front(), document_.level(), minv, maxv, haveAny);
    if (haveAny)
    {
        // Start the same animated bounds framing used by outliner navigation
        // before rebuilding the heavier tree/property widgets.  The camera
        // can then advance with the UI refresh instead of visibly waiting for
        // undo/redo finalization and jumping afterwards.
        frameBoundsWithNavigationPrefs(minv, maxv, true);
    }

    rebuildOutliner();
    updateEditActions();
    rebuildProperties();
    if (preparedCleaveApply && cleaveApplyRecordId)
    {
        (void)restoreCleaveApplyWorkspaceAfterUndo(
            *cleaveApplyRecordId,
            std::move(*preparedCleaveApply));
    }

    return true;
}

} // namespace sed
