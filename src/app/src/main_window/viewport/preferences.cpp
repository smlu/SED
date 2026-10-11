#include "main_window/common.hpp"
#include "ui/components/preview_worker_pool.hpp"
#include "render/viewport_color_theme.hpp"

#include <cmath>

namespace sed
{

void MainWindow::applyArchThingGizmoPreferences()
{
    if (!glView_)
    {
        return;
    }

    const QSettings settings = settingsStore();
    const bool classicArchProject = document_.level().kind == ProjectType::JKDF2
        || document_.level().kind == ProjectType::MOTS;
    const QVariant editorFallback = classicArchProject
        ? settings.value("viewport/level_geometry_gizmos", true)
        : QVariant(true);
    const QVariant previewFallback = classicArchProject
        ? settings.value("viewport/preview_level_geometry_gizmos", false)
        : QVariant(false);
    glView_->setEditorDefaultShowArchThingGizmos(settings.value(
        "viewport/arch_thing_gizmos", editorFallback).toBool());
    glView_->setPreviewDefaultShowArchThingGizmos(settings.value(
        "viewport/preview_arch_thing_gizmos", previewFallback).toBool());
}

void MainWindow::loadViewportPreferences()
{
    QSettings settings = settingsStore();
    glView_->setViewportNavigationSettings(loadViewportNavigationSettings(settings));
    (void)glView_->setViewportBannerSettings(loadViewportBannerSettings(settings));

    ui::detail::setPreviewWorkerCount(settings.value(
        "resource_previews/worker_count",
        ui::detail::defaultPreviewWorkerCount()).toInt());
    ui::detail::setPreviewPublishBatchCount(settings.value(
        "resource_previews/publish_batch_count",
        ui::detail::kDefaultPreviewPublishBatchCount).toInt());

    const int storedDynamicLightShading =
        settings.value("viewport/dynamic_light_shading", 0).toInt();
    const GlView::DynamicLightShading dynamicLightShading =
        storedDynamicLightShading == static_cast<int>(GlView::DynamicLightShading::Phong)
        ? GlView::DynamicLightShading::Phong
        : GlView::DynamicLightShading::Gouraud;
    glView_->setDynamicLightShading(
        dynamicLightShading);
    glView_->setThingLightRenderingEnabled(settings.value(
        "viewport/render_thing_lights", true).toBool());
    glView_->setSectorPointLightRenderingEnabled(settings.value(
        "viewport/render_sector_point_lights", true).toBool());
    if ( thingLightRenderingAction_ )
    {
        const QSignalBlocker blocker(thingLightRenderingAction_);
        thingLightRenderingAction_->setChecked(
            glView_->thingLightRenderingEnabled());
    }
    if ( sectorPointLightRenderingAction_ )
    {
        const QSignalBlocker blocker(sectorPointLightRenderingAction_);
        sectorPointLightRenderingAction_->setChecked(
            glView_->sectorPointLightRenderingEnabled());
        const bool available = document_.level().kind == ProjectType::IJIM;
        sectorPointLightRenderingAction_->setVisible(true);
        sectorPointLightRenderingAction_->setEnabled(available);
    }
    glView_->setSurfaceExtrudePreviewOpacity(static_cast<float>(std::clamp(
        settings.value("viewport/surface_extrude_preview_opacity", 0.74).toDouble(),
        0.05,
        1.0)));

    bool motsTreeRadiusFactorParsed = false;
    const double motsTreeRadiusFactor = settings.value(
        "viewport/mots_tree_collision_preview_mesh_radius_factor",
        0.75).toDouble(&motsTreeRadiusFactorParsed);
    glView_->setMotsTreeCollisionPreviewMeshRadiusFactor(
        motsTreeRadiusFactorParsed && std::isfinite(motsTreeRadiusFactor)
            ? static_cast<float>(std::clamp(motsTreeRadiusFactor, 0.01, 10.0))
            : 0.75f);

    if (settings.contains("viewport/thing_cube_half_size"))
    {
        glView_->setThingCubeHalfSize(static_cast<float>(settings.value("viewport/thing_cube_half_size").toDouble()));
    }

    glView_->setShowGrid(settings.value("viewport/show_grid", true).toBool());
    glView_->setXraySelectionCyclingEnabled(
        settings.value("viewport/xray_selection_cycling", false).toBool());
    glView_->setOrbitAroundSelectionEnabled(
        settings.value("viewport/orbit_around_selection", true).toBool());
    glView_->setGridFollowCamera(settings.value("viewport/grid_follow_camera", true).toBool());
    glView_->setGridRotateWithView(settings.value("viewport/grid_rotate_with_view", false).toBool());
    glView_->setGridKeepViewAlignedVisible(
        settings.value("viewport/grid_keep_view_aligned_visible", false).toBool());
    glView_->setGridMajorSpacing(static_cast<float>(settings.value("viewport/grid_spacing", 1.0).toDouble()));
    glView_->setGridMinorSpacing(static_cast<float>(settings.value("viewport/grid_minor_spacing", 0.1).toDouble()));
    glView_->setGridAdaptiveSpacing(settings.value("viewport/grid_adaptive_spacing", false).toBool());
    glView_->setGridOpacity(static_cast<float>(settings.value("viewport/grid_opacity", 0.70).toDouble()));
    const render::ViewportColorThemeSelection themeSelection =
        render::resolveViewportColorTheme(settings);
    const render::ViewportColorPalette& activeThemeColors =
        themeSelection.palette;
    glView_->setBackgroundClearColor(activeThemeColors.backgroundClear);
    glView_->setWireframeColor(activeThemeColors.wireframe);
    glView_->setGridColor(activeThemeColors.grid);
    glView_->setGridMajorColor(activeThemeColors.gridMajor);
    glView_->setGridAxisXColor(activeThemeColors.gridAxisX);
    glView_->setGridAxisYColor(activeThemeColors.gridAxisY);
    glView_->setGridAxisZColor(activeThemeColors.gridAxisZ);
    glView_->setSelectionPrimaryColor(activeThemeColors.selectionPrimary);
    glView_->setSelectionMultiColor(activeThemeColors.selectionMulti);
    glView_->setSelectionEdgeMultiColor(activeThemeColors.selectionEdgeMulti);
    glView_->setSelectionActiveColor(activeThemeColors.selectionActive);
    glView_->setSelectionVertexColor(activeThemeColors.selectionVertex);
    glView_->setSelectionVertexMultiColor(activeThemeColors.selectionVertexMulti);
    glView_->setCogEntityPickPrimaryColor(activeThemeColors.cogPickPrimary);
    glView_->setCogEntityPickActiveColor(activeThemeColors.cogPickActive);
    if (auto* uvCanvas = findChild<QWidget*>(QStringLiteral("uvEditorCanvas")))
    {
        uvCanvas->setProperty(
            "sed.uvCanvasBackgroundColor", activeThemeColors.uvCanvasBackground);
        uvCanvas->setProperty("sed.uvGridMainColor", activeThemeColors.uvGridMain);
        uvCanvas->setProperty("sed.uvGridSubColor", activeThemeColors.uvGridSub);
        uvCanvas->setProperty("sed.uvGridAxisUColor", activeThemeColors.uvGridAxisU);
        uvCanvas->setProperty("sed.uvGridAxisVColor", activeThemeColors.uvGridAxisV);
        uvCanvas->update();
    }
    glView_->setGridLineWidthPx(
        static_cast<float>(settings.value("viewport/grid_line_width_px", 1.0).toDouble()));
    glView_->setGridMinorDots(
        settings.value("viewport/grid_minor_dots", false).toBool());
    glView_->setGridMinorDotSizePx(static_cast<float>(
        settings.value("viewport/grid_minor_dot_size_px", 2.0).toDouble()));
    const bool gridAlwaysBehind = settings.value("viewport/grid_always_behind", false).toBool();
    const bool gridVisibleThroughWalls = !gridAlwaysBehind
        && settings.value("viewport/grid_visible_through_walls", false).toBool();
    glView_->setGridVisibleThroughWalls(gridVisibleThroughWalls);
    glView_->setGridAlwaysBehind(gridAlwaysBehind);
    glView_->setGridResetOnCameraRotate(settings.value("viewport/grid_reset_on_rotate", true).toBool());
    const bool editorRenderFog = settings.value("viewport/editor_render_fog", false).toBool();
    const bool previewRenderFog = settings.value("viewport/preview_render_fog", true).toBool();
    glView_->setDefaultShowFog(GlView::RenderPipeline::Editor, editorRenderFog);
    glView_->setDefaultShowFog(GlView::RenderPipeline::GamePreview, previewRenderFog);
    glView_->setShowFog(
        (glView_->renderPipeline() == GlView::RenderPipeline::GamePreview)
            ? previewRenderFog
            : editorRenderFog);
    glView_->setDefaultShowThingCollisionShapes(
        GlView::RenderPipeline::Editor,
        settings.value("viewport/editor_show_thing_collision_shapes", true).toBool());
    glView_->setDefaultShowThingCollisionShapes(
        GlView::RenderPipeline::GamePreview,
        settings.value("viewport/preview_show_thing_collision_shapes", false).toBool());
    glView_->setDefaultShowThingVisualBounds(
        GlView::RenderPipeline::Editor,
        settings.value("viewport/editor_show_thing_visual_bounds", true).toBool());
    glView_->setDefaultShowThingVisualBounds(
        GlView::RenderPipeline::GamePreview,
        settings.value("viewport/preview_show_thing_visual_bounds", false).toBool());
    glView_->setDefaultThingModelLodEnabled(
        GlView::RenderPipeline::Editor,
        settings.value("viewport/editor_thing_model_lod_enabled", false).toBool());
    glView_->setDefaultThingModelLodEnabled(
        GlView::RenderPipeline::GamePreview,
        settings.value("viewport/preview_thing_model_lod_enabled", false).toBool());
    // Preferences load before viewportStateChanged is connected during startup.
    syncThingModelLodActionAvailability();
    editorViewportSoundMuted_ = settings.value("viewport/editor_mute_sound", false).toBool();
    previewViewportSoundMuted_ = settings.value("viewport/preview_mute_sound", false).toBool();
    syncViewportSoundControls();
    const bool geometryTransformSnap =
        settings.value("viewport/transform_geometry_snap", true).toBool();
    glView_->setGeometryTransformSnapEnabled(geometryTransformSnap);
    glView_->setVertexSnapRadius(static_cast<float>(
        settings.value("viewport/snap_radius_vertex", 0.20).toDouble()));
    glView_->setEdgeSnapRadius(static_cast<float>(
        settings.value("viewport/snap_radius_edge", 0.20).toDouble()));
    glView_->setFaceSnapRadius(static_cast<float>(
        settings.value("viewport/snap_radius_face", 2.0).toDouble()));
    glView_->setSectorSnapRadius(static_cast<float>(
        settings.value("viewport/snap_radius_sector", 2.0).toDouble()));
    glView_->setThingSnapRadius(static_cast<float>(
        settings.value("viewport/snap_radius_thing", 0.16).toDouble()));
    glView_->setSnapRotationMaxDegrees(static_cast<float>(
        settings.value("viewport/snap_rotation_max_degrees", 180.0).toDouble()));
    glView_->setSurfaceStickRadius(static_cast<float>(
        settings.value("viewport/surface_stick_radius", 0.22).toDouble()));
    glView_->setGridPlaneStickRadius(static_cast<float>(
        settings.value("viewport/grid_plane_stick_radius", 0.08).toDouble()));
    if (viewportSurfaceGeometrySnapButton_)
    {
        const QSignalBlocker blocker(viewportSurfaceGeometrySnapButton_);
        viewportSurfaceGeometrySnapButton_->setChecked(geometryTransformSnap);
    }
    const bool geometryTransformRotate = settings.value("viewport/transform_geometry_rotate", true).toBool();
    glView_->setGeometryTransformRotateToNearbyEnabled(geometryTransformRotate);
    if ( viewportSurfaceGeometryRotateButton_ )
    {
        const QSignalBlocker blocker(viewportSurfaceGeometryRotateButton_);
        viewportSurfaceGeometryRotateButton_->setChecked(geometryTransformRotate);
    }
    const bool preserveConnectedSurfaceUvs = settings.value("viewport/preserve_connected_surface_uvs", true).toBool();
    glView_->setPreserveConnectedSurfaceUvsEnabled(preserveConnectedSurfaceUvs);
    if ( viewportPreserveConnectedSurfaceUvsButton_ )
    {
        const QSignalBlocker blocker(viewportPreserveConnectedSurfaceUvsButton_);
        viewportPreserveConnectedSurfaceUvsButton_->setChecked(preserveConnectedSurfaceUvs);
    }
    const bool transformGridSnap = settings.value("viewport/transform_grid_snap", false).toBool();
    const bool transformGridStick = settings.value("viewport/transform_grid_stick_through", true).toBool();
    glView_->setTransformGridSnapEnabled(transformGridSnap);
    glView_->setTransformGridPlaneStickEnabled(transformGridStick);
    if (viewportTransformGridSnapAction_)
    {
        const QSignalBlocker blocker(viewportTransformGridSnapAction_);
        viewportTransformGridSnapAction_->setChecked(transformGridSnap);
    }
    if (viewportTransformGridStickAction_)
    {
        const QSignalBlocker blocker(viewportTransformGridStickAction_);
        viewportTransformGridStickAction_->setChecked(transformGridStick);
    }
    if (viewportTransformGridStickButton_)
    {
        const QSignalBlocker blocker(viewportTransformGridStickButton_);
        viewportTransformGridStickButton_->setChecked(transformGridStick);
    }
    if (viewportTransformGridSnapButton_)
    {
        const QSignalBlocker blocker(viewportTransformGridSnapButton_);
        viewportTransformGridSnapButton_->setChecked(transformGridSnap);
    }
    if (viewportThingMoveGridSnapButton_)
    {
        const QSignalBlocker blocker(viewportThingMoveGridSnapButton_);
        viewportThingMoveGridSnapButton_->setChecked(transformGridSnap);
    }
    if (addSectorGridSnapButton_)
    {
        const QSignalBlocker blocker(addSectorGridSnapButton_);
        addSectorGridSnapButton_->setChecked(transformGridSnap);
    }
    if (viewportThingMoveGridStickButton_)
    {
        const QSignalBlocker blocker(viewportThingMoveGridStickButton_);
        viewportThingMoveGridStickButton_->setChecked(transformGridStick);
    }
    const bool sectorMoveSnapAlign = settings.value("sector_move/snap_align", true).toBool();
    const bool thingMoveSurfaceStick = settings.value("thing_move/surface_stick", true).toBool();
    const bool thingMoveSurfaceAlign = settings.value("thing_move/surface_align", false).toBool();
    const bool thingMoveSnapToThing = settings.value("thing_move/snap_to_thing", true).toBool();
    const bool thingMoveSnapRotate = settings.value("thing_move/snap_rotate", true).toBool();
    glView_->setSectorMoveSnapAlignEnabled(sectorMoveSnapAlign);
    glView_->setThingMoveSurfaceStickEnabled(thingMoveSurfaceStick);
    glView_->setThingMoveSurfaceAlignEnabled(thingMoveSurfaceAlign);
    glView_->setThingMoveSnapToThingEnabled(thingMoveSnapToThing);
    glView_->setThingMoveSnapRotateEnabled(thingMoveSnapRotate);
    if (viewportSectorMoveSnapAlignButton_)
    {
        const QSignalBlocker blocker(viewportSectorMoveSnapAlignButton_);
        viewportSectorMoveSnapAlignButton_->setChecked(sectorMoveSnapAlign);
    }
    if (viewportThingMoveStickButton_)
    {
        const QSignalBlocker blocker(viewportThingMoveStickButton_);
        viewportThingMoveStickButton_->setChecked(thingMoveSurfaceStick);
    }
    if (viewportThingMoveSurfaceAlignButton_)
    {
        const QSignalBlocker blocker(viewportThingMoveSurfaceAlignButton_);
        viewportThingMoveSurfaceAlignButton_->setChecked(thingMoveSurfaceAlign);
    }
    if (viewportThingMoveSnapButton_)
    {
        const QSignalBlocker blocker(viewportThingMoveSnapButton_);
        viewportThingMoveSnapButton_->setChecked(thingMoveSnapToThing);
    }
    if (viewportThingMoveSnapRotateButton_)
    {
        const QSignalBlocker blocker(viewportThingMoveSnapRotateButton_);
        viewportThingMoveSnapRotateButton_->setChecked(thingMoveSnapRotate);
    }
    syncThingMoveOptionsOverlay();
    if (viewportGridVisibleThroughWallsAction_)
    {
        viewportGridVisibleThroughWallsAction_->setChecked(glView_->gridVisibleThroughWalls());
    }
    if (viewportGridAlwaysBehindAction_)
    {
        viewportGridAlwaysBehindAction_->setChecked(glView_->gridAlwaysBehind());
    }
    glView_->setWireframeLineWidthPx(static_cast<float>(settings.value("viewport/wireframe_line_width_px", 1.35).toDouble()));
    glView_->setWireframeAntialiasingEnabled(settings.value("viewport/wireframe_antialiasing", true).toBool());
    glView_->setFramePathLineWorldSize(static_cast<float>(settings.value("viewport/frame_path_line_world_size", 0.0025).toDouble()));
    glView_->setFramePathPointWorldSize(static_cast<float>(settings.value("viewport/frame_path_point_world_size", 0.0050).toDouble()));

    // Game Preview is a view-only workspace. Legacy persisted overlay flags
    // are intentionally ignored; editing panels are always hidden there.
    previewShowEditModeOverlay_ = false;
    previewShowTransformOverlay_ = false;
    previewShowToolsOverlay_ = false;
    previewRespectHiddenLayers_ = settings.value("viewport/preview_respect_hidden_layers", false).toBool();
    outlinerColorByLayer_ = settings.value("viewport/layers/color_outliner_entities", true).toBool();
    wireframeColorByLayer_ = settings.value("viewport/layers/color_wireframe", true).toBool();

    layerColors_.clear();
    for (const QString &value : settings.value("viewport/layers/colors").toStringList())
    {
        QColor parsed(value.trimmed());
        if (parsed.isValid())
        {
            parsed.setAlpha(255);
        }
        layerColors_.push_back(parsed);
    }

    if (settings.contains("viewport/thing_icon_cube_tint"))
    {
        const QColor parsed(settings.value("viewport/thing_icon_cube_tint").toString().trimmed());
        if (parsed.isValid())
        {
            glView_->setThingIconCubeTintColor(parsed);
        }
    }

    glView_->setThingGizmoHeightAboveModel(static_cast<float>(settings.value(
        "viewport/thing_gizmo_height_above_model", 0.0).toDouble()));
    glView_->setThingGizmoStackSpacing(static_cast<float>(settings.value(
        "viewport/thing_gizmo_stack_spacing",
        static_cast<double>(render::kThingGizmoStackSpacingDefault)).toDouble()));

    glView_->setThingLightGizmoOffsetAboveModel(static_cast<float>(settings.value(
        "viewport/thing_light_gizmo_offset_above_model", 0.0).toDouble()));
    glView_->setArchThingGizmoOffsetAboveModel(static_cast<float>(settings.value(
        "viewport/arch_thing_gizmo_offset_above_model", 0.0).toDouble()));
    glView_->setLevelGeometryVertexMarkerSizePx(static_cast<float>(settings.value(
        "viewport/level_geometry_vertex_marker_size_px",
        static_cast<double>(GlView::kLevelGeometryVertexMarkerDefaultSizePx)).toDouble()));

    if (settings.contains("viewport/thing_gizmo_world_size"))
    {
        glView_->setThingGizmoWorldSize(static_cast<float>(settings.value("viewport/thing_gizmo_world_size").toDouble()));
    }
    else if (settings.contains("viewport/thing_gizmo_base_px"))
    {
        // Back-compat: old pixel-based size. Assume 24px corresponds to 0.03 world
        // units and scale proportionally.
        const float basePx = static_cast<float>(settings.value("viewport/thing_gizmo_base_px").toDouble());
        glView_->setThingGizmoWorldSize(0.03f * (basePx / 24.0f));
    }

    if (settings.contains("viewport/light_gizmo_world_size"))
    {
        glView_->setLightGizmoWorldSize(static_cast<float>(settings.value("viewport/light_gizmo_world_size").toDouble()));
    }
    else if (settings.contains("viewport/light_gizmo_base_px"))
    {
        // Back-compat: old pixel-based size. Assume 18px corresponds to 0.04 world
        // units and scale proportionally.
        const float basePx = static_cast<float>(settings.value("viewport/light_gizmo_base_px").toDouble());
        glView_->setLightGizmoWorldSize(0.04f * (basePx / 18.0f));
    }

    glView_->setLightGizmoUseLightColor(settings.value("viewport/light_gizmo_use_light_color", true).toBool());
    glView_->setArchitecturalLightingMarkerSizePx(static_cast<float>(settings.value(
        "viewport/architectural_lighting_marker_size_px",
        static_cast<double>(GlView::kArchitecturalLightingMarkerDefaultSizePx)).toDouble()));
    glView_->setArchitecturalLightingMarkerUseAuthoredColor(settings.value(
        "viewport/architectural_lighting_marker_use_authored_color", true).toBool());
    const QColor storedArchitecturalLightingMarkerColor(settings.value(
        "viewport/architectural_lighting_marker_color",
        QStringLiteral("#ffcc33")).toString().trimmed());
    glView_->setArchitecturalLightingMarkerColor(
        storedArchitecturalLightingMarkerColor);

    if (settings.contains("viewport/show_preview_cubes"))
    {
        glView_->setShowPreviewCubes(settings.value("viewport/show_preview_cubes").toBool());
    }

    glView_->setEditorDefaultShowThingGizmos(settings.value("viewport/editor_show_gizmos", false).toBool());
    glView_->setPreviewDefaultShowThingGizmos(settings.value("viewport/preview_show_gizmos", false).toBool());
    applyArchThingGizmoPreferences();

    // Thing type colors (optional; when unset, GlView uses built-in defaults).
    {
        struct ColorDef
        {
            const char *key;
            QColor def;
        };
        const std::vector<ColorDef> defs = {
            {"camera", QColor(77, 153, 255)},    {"actor", QColor(230, 89, 51)},      {"weapon", QColor(242, 153, 38)}, {"item", QColor(51, 217, 77)},
            {"player", QColor(26, 230, 230)},    {"cog", QColor(204, 128, 230)},      {"ghost", QColor(153, 153, 166)}, {"corpse", QColor(140, 89, 51)},
            {"explosion", QColor(255, 102, 26)}, {"particle", QColor(242, 242, 77)},  {"debris", QColor(166, 128, 89)}, {"hint", QColor(102, 230, 204)},
            {"sprite", QColor(217, 179, 217)},   {"polyline", QColor(191, 191, 102)}, {"free", QColor(128, 128, 128)},  {"default", QColor(179, 179, 179)},
        };

        std::unordered_map<std::string, QColor> colors{};
        bool any = false;
        for (const ColorDef &def : defs)
        {
            const QString k = QString("viewport/thing_type_color/%1").arg(QString::fromUtf8(def.key));
            QColor c = def.def;
            if (settings.contains(k))
            {
                any = true;
                const QString v = settings.value(k).toString().trimmed();
                const QColor parsed(v);
                if (parsed.isValid())
                {
                    c = parsed;
                }
            }
            colors.emplace(def.key, c);
        }

        if (any)
        {
            glView_->setThingTypeColors(colors);
        }
    }

    normalizeLayerVisualState();
    syncLayerViewState();
}

} // namespace sed
