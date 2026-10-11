#pragma once

#include "gl_view.hpp"
#include "ui/components/thing_preview_render_policy.hpp"

#include <string>

namespace sed::ui::detail
{

/**
 * Resolves the CMP identity shared by Thing preview producers and consumers.
 * Loaded classic levels may keep the Level Header field empty and inherit the
 * first Sector CMP, so using the raw field would create two different cache
 * keys and leave the UI polling a result that no worker can publish.
 */
inline std::string thingAssetPreviewColormap(const Level& level)
{
    return level.getMasterCMP();
}

/** Applies the deliberately narrow model/template preview render contract. */
inline void applyThingAssetPreviewRenderMode(GlView& view, const int storedMode)
{
    const ThingAssetPreviewRenderPolicy policy = thingAssetPreviewRenderPolicy(storedMode);
    // These controls describe the Thing model, not the empty helper Level.
    // Bounding boxes and Thing-origin dots are intentionally not exposed.
    // Asset authoring previews must remain visible even when a template
    // inherits Disabled or Invisible Thing flags used by the running game.
    view.setShowHiddenThings(true);
    view.setShowWireframe(false);
    view.setShowVertices(false);
    view.setShowSolid(false);
    view.setShowTextures(policy.textures);
    view.setThingViewMode(
        policy.thingMode == ThingAssetPreviewThingMode::wireframes
            ? GlView::ThingViewMode::Wireframes
            : (policy.thingMode == ThingAssetPreviewThingMode::vertices
                ? GlView::ThingViewMode::Vertices
                : GlView::ThingViewMode::Shaded));
}

} // namespace sed::ui::detail
