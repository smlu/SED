#pragma once

#include <cstdint>
#include <optional>
#include <vector>

#include <QVector3D>

#include "sed/editor/selection.hpp"
#include "sed/level.hpp"
#include "sed/level/cleave.hpp"

namespace sed::ui::sector
{

struct SectorJoinPreviewResult
{
    Level level{};
    editor::Selection selection{};
    std::int32_t activeSector{-1};
    std::vector<std::int32_t> previewSectors{};
};

/**
 * Builds the same sector-join result that Apply would commit, but on a copy of
 * the level so the viewport can preview the selected mode without mutating the
 * document.
 */
std::optional<SectorJoinPreviewResult> buildSectorJoinPreview(
    const Level& level,
    const editor::Selection& selection,
    std::int32_t activeSector,
    level::SectorConnectKnifeMode mode);

/** Preview variant that distinguishes ID exhaustion from ordinary geometry rejection. */
std::optional<SectorJoinPreviewResult> buildSectorJoinPreview(
    const Level& level,
    const editor::Selection& selection,
    std::int32_t activeSector,
    level::SectorConnectKnifeMode mode,
    bool* idExhausted);

/** Builds wireframe line segments for the sectors affected by a Join preview. */
std::vector<QVector3D> buildSectorJoinPreviewLines(const SectorJoinPreviewResult& preview);

} // namespace sed::ui::sector
