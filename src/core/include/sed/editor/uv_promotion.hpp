#pragma once

#include <cstddef>
#include <optional>
#include <string>

#include "sed/editor/document.hpp"
#include "sed/level/uv.hpp"

namespace sed::editor
{

/** Failure-atomic result for changing one complete document UV space.
 *
 * Native SED history must never mix pixel-space and normalized Levels. The
 * optional history location identifies the first undo/redo state which could
 * not be converted; an absent location means the current Level failed.
 */
struct DocumentUvConversionResult
{
    std::optional<SurfaceRef> failedSurface{};
    std::optional<DocumentRecoveryHistoryStack> failedHistoryStack{};
    std::size_t failedHistoryIndex{};
    std::string failedMaterial{};
    std::string error{};

    explicit operator bool() const noexcept
    {
        return error.empty();
    }
};

/** Converts the current Level and every UV-bearing undo/redo state together.
 *
 * The input is unchanged unless the current Level and every historical Level
 * can be converted. Compact recovery history stays compact in the published
 * result; this function does not silently discard or flatten the timeline.
 */
DocumentUvConversionResult convertDocumentUvCoordinateSpace(
    Level& current,
    DocumentRecoveryHistory& history,
    UvCoordinateSpace destination,
    const level::UvTextureDimensionsResolver& resolveDimensions);

} // namespace sed::editor
