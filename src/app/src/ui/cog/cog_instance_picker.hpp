#pragma once

#include <cstdint>
#include <optional>

class QWidget;

namespace sed
{

struct Level;

namespace ui
{

/** Opens a searchable picker for an existing placed COG instance.
 *
 * The returned value is the COG's index in `Level::cogs`, or -1 for an unset
 * reference. Cancellation returns `std::nullopt` and does not edit the Level.
 */
std::optional<std::int32_t> pickPlacedCogInstance(
    QWidget* parent,
    const Level& level,
    std::int32_t currentIndex);

} // namespace ui
} // namespace sed
