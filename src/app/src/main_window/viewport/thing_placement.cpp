#include "main_window/common.hpp"
#include "ui/layers/layer_color_utils.hpp"
#include "ui/components/bounded_two_queue_cache.hpp"
#include "ui/components/preview_resource_revision_utils.hpp"
#include "ui/shortcuts/selection_modifier_utils.hpp"
#include "ui/transform/rotation_utils.hpp"
#include "ui/viewport/thing_brush_precision_utils.hpp"

namespace sed
{

namespace
{

constexpr int kMinimumThingPlacementBrushSampleCount = 1;
constexpr int kMaximumThingPlacementBrushSampleCount = 256;

int clampedThingPlacementBrushSampleCount(const int requestedCount) noexcept
{
    return std::clamp(
        requestedCount,
        kMinimumThingPlacementBrushSampleCount,
        kMaximumThingPlacementBrushSampleCount);
}

} // namespace

#ifdef SED_APP_MAIN_WINDOW_TEST_ACCESS
void MainWindow::setThingPlacementTemplateForTest(
    std::string templateName,
    const double groundOffset)
{
    for (const ThingTemplate& authored : document_.level().templates)
    {
        if (authored.name == templateName)
        {
            (void)templates_.addOrUpdate(authored, true);
            thingPlacementTemplateListDirty_ = true;
            if (glView_)
            {
                glView_->setTemplateDatabase(&templates_);
            }
            break;
        }
    }
    selectedThingTemplateName_ = std::move(templateName);
    ensureThingPlacementTemplateCache(selectedThingTemplateName_);
    thingPlacementTemplateGroundOffsetCache_ = groundOffset;
}
#endif

std::string MainWindow::currentThingTemplateName() const
{
    const std::string selected = sed::text::trim(selectedThingTemplateName_);
    if (!selected.empty() && !sed::text::iequalsAscii(selected, "none"))
    {
        return selected;
    }

    return {};
}

QString MainWindow::selectedThingTemplateLabel() const
{
    const std::string name = currentThingTemplateName();
    return name.empty() ? QStringLiteral("template") : QString::fromStdString(name);
}

std::int32_t MainWindow::resolvedThingPlacementLayer(const std::int32_t targetSector) const noexcept
{
    const Level& level = document_.level();
    std::int32_t targetLayer = -1;
    if (targetSector >= 0 && static_cast<std::size_t>(targetSector) < level.sectors.size())
    {
        targetLayer = level.sectors[static_cast<std::size_t>(targetSector)].layer;
    }
    return ui::layers::resolvePlacementLayer(
        level.layers.size(),
        thingPlacementLayerOverride_,
        targetLayer,
        layerVisible_);
}

MainWindow::ThingPlacementVisualKind MainWindow::thingTemplateVisualKind(const std::string_view thingName) const
{
    if (thingName.empty())
    {
        return ThingPlacementVisualKind::none;
    }

    const Level& level = document_.level();
    const assets::Templates* templatesDb = (templates_.size() > 0) ? &templates_ : nullptr;

    Thing temp{};
    temp.name = std::string(thingName);
    temp.templateName = std::string(thingName);

    auto valueString = [](const TplValue* value) -> std::string
    {
        if (!value)
        {
            return {};
        }
        return value->stringValue.empty() ? value->asString() : value->stringValue;
    };

    const std::string spriteName = sed::text::trim(valueString(
        sed::level::thingFindValue(level, templatesDb, temp, "sprite")));
    if (!spriteName.empty() && !sed::text::iequalsAscii(spriteName, "none"))
    {
        return ThingPlacementVisualKind::sprite;
    }

    const TplValue* modelValue = sed::level::thingFindValue(level, templatesDb, temp, "model3d");
    if (!modelValue)
    {
        modelValue = sed::level::thingFindValue(level, templatesDb, temp, "model3do");
    }

    const std::string modelName = sed::text::trim(valueString(modelValue));
    if (modelName.empty() || sed::text::iequalsAscii(modelName, "none"))
    {
        return ThingPlacementVisualKind::none;
    }

    return ThingPlacementVisualKind::model;
}

std::optional<ThingBox> MainWindow::resolvedThingTemplateBox(const std::string_view thingName) const
{
    if (templates_.size() == 0 || thingName.empty())
    {
        return std::nullopt;
    }

    std::string current = std::string(thingName);
    for (int depth = 0; depth < 256; ++depth)
    {
        const ThingTemplate* tpl = templates_.find(current);
        if (!tpl)
        {
            break;
        }

        if (!tpl->bbox.IsZero())
        {
            return tpl->bbox;
        }

        if (tpl->parent.empty() || sed::text::iequalsAscii(tpl->parent, "none"))
        {
            break;
        }
        current = tpl->parent;
    }

    const Level& level = document_.level();
    const assets::Templates* templatesDb = (templates_.size() > 0) ? &templates_ : nullptr;

    if (const std::optional<ThingBox> box = sed::thing::loadModel3doBox(
            resources_,
            sed::thing::templateModel3doName(level, templatesDb, thingName)))
    {
        return box;
    }

    if (const std::optional<ThingBox> box = sed::thing::loadSpriteBox(
            resources_,
            sed::thing::templateSpriteName(level, templatesDb, thingName)))
    {
        return box;
    }

    return std::nullopt;
}

std::optional<ThingBox> MainWindow::resolvedPlacedThingBox(const Thing& thing) const
{
    if (!thing.bbox.IsZero())
    {
        return thing.bbox;
    }

    const std::string_view templateName =
        !thing.templateName.empty() ? std::string_view(thing.templateName)
                                    : std::string_view(thing.name);
    return resolvedThingTemplateBox(templateName);
}

std::vector<MainWindow::ThingPlacementConnector> MainWindow::resolvedThingTemplateConnectors(
    const std::string_view thingName) const
{
    if (templates_.size() == 0 || thingName.empty())
    {
        return {};
    }

    const Level& level = document_.level();
    const assets::Templates* templatesDb = (templates_.size() > 0) ? &templates_ : nullptr;
    return sed::thing::resolveRailTrackConnectors(
        resources_,
        sed::thing::templateModel3doName(level, templatesDb, thingName),
        thingName);
}

std::vector<MainWindow::ThingPlacementConnector> MainWindow::resolvedPlacedThingConnectors(const Thing& thing) const
{
    const Level& level = document_.level();
    const assets::Templates* templatesDb = (templates_.size() > 0) ? &templates_ : nullptr;
    return sed::thing::resolveThingSnapConnectors(level, templatesDb, &resources_, thing);
}

void MainWindow::invalidateThingPlacementSnapCaches() noexcept
{
    thingPlacementSnapAnchorCacheValid_ = false;
    thingPlacementSnapTemplateCacheValid_ = false;
    framePlacementOwnerProxyCache_.reset();
    framePlacementPreview_.reset();
}

MainWindow::FramePlacementOptions MainWindow::framePlacementOptions(
    const Qt::KeyboardModifiers modifiers) const
{
    const Qt::KeyboardModifiers held =
        modifiers & ui::shortcuts::editorModifierMask();
    const QSettings settings = settingsStore();
    return FramePlacementOptions{
        .surfaceStick = ui::shortcuts::modifierToggledSetting(
            settings.value("thing_move/surface_stick", true).toBool(),
            held,
            thingMoveSurfaceStickInvertModifierBindings()),
        .surfaceAlign = ui::shortcuts::modifierToggledSetting(
            settings.value("thing_move/surface_align", false).toBool(),
            held,
            thingMoveSurfaceAlignInvertModifierBindings()),
        .snapToThing = ui::shortcuts::modifierToggledSetting(
            settings.value("thing_move/snap_to_thing", true).toBool(),
            held,
            thingMoveSnapInvertModifierBindings()),
        .rotateToSnap = ui::shortcuts::modifierToggledSetting(
            settings.value("thing_move/snap_rotate", true).toBool(),
            held,
            thingMoveSnapRotateInvertModifierBindings()),
    };
}

std::optional<std::int32_t> MainWindow::framePlacementOwnerThingIndex() const
{
    const editor::Selection& selection = document_.selection();
    std::optional<std::int32_t> owner{};
    if (selection.things.size() == 1u)
    {
        owner = *selection.things.begin();
    }
    else if (selection.things.empty() && !selection.frames.empty())
    {
        for (const level::FrameRef ref : selection.frames)
        {
            if (!owner)
            {
                owner = ref.thing;
            }
            else if (*owner != ref.thing)
            {
                return std::nullopt;
            }
        }
    }
    else
    {
        return std::nullopt;
    }

    if (!owner || *owner < 0
        || static_cast<std::size_t>(*owner) >= document_.level().things.size())
    {
        return std::nullopt;
    }
    if (!selection.frames.empty()
        && std::any_of(
            selection.frames.begin(), selection.frames.end(),
            [owner](const level::FrameRef ref)
            {
                return ref.thing != *owner;
            }))
    {
        return std::nullopt;
    }
    return owner;
}

std::optional<MainWindow::FramePlacementPreview>
MainWindow::resolveFramePlacementPreview(
    const QPoint& widgetPos,
    const Qt::KeyboardModifiers modifiers) const
{
#ifdef SED_APP_MAIN_WINDOW_TEST_ACCESS
    const auto setDiagnostic = [this](const QString& message)
    {
        const_cast<MainWindow*>(this)->setProperty(
            "sed.framePlacementPreviewDiagnostic", message);
    };
    setDiagnostic(QStringLiteral("checking placement guards"));
#endif
    const bool pointerInsidePlacementPanel = thingPlacementPointerInsidePanel();
    if (!glView_
        || editMode_ != EditMode::frame
        || !viewportFrameAddAction_
        || !viewportFrameAddAction_->isChecked()
        || placementMode_ != PlacementMode::none
        || glView_->renderPipeline() != GlView::RenderPipeline::Editor
        || glView_->flyNavigationActive()
        || pointerInsidePlacementPanel)
    {
#ifdef SED_APP_MAIN_WINDOW_TEST_ACCESS
        QStringList failedGuards{};
        if (!glView_)
        {
            failedGuards.push_back(QStringLiteral("no viewport"));
        }
        else
        {
            if (editMode_ != EditMode::frame)
            {
                failedGuards.push_back(
                    QStringLiteral("edit mode=%1").arg(static_cast<int>(editMode_)));
            }
            if (!viewportFrameAddAction_)
            {
                failedGuards.push_back(QStringLiteral("no Frame Add action"));
            }
            else if (!viewportFrameAddAction_->isChecked())
            {
                failedGuards.push_back(QStringLiteral("Frame Add unchecked"));
            }
            if (placementMode_ != PlacementMode::none)
            {
                failedGuards.push_back(
                    QStringLiteral("placement mode=%1")
                        .arg(static_cast<int>(placementMode_)));
            }
            if (glView_->renderPipeline() != GlView::RenderPipeline::Editor)
            {
                failedGuards.push_back(QStringLiteral("non-editor pipeline"));
            }
            if (glView_->flyNavigationActive())
            {
                failedGuards.push_back(QStringLiteral("fly navigation active"));
            }
            if (pointerInsidePlacementPanel)
            {
                failedGuards.push_back(QStringLiteral("pointer inside placement panel"));
            }
        }
        setDiagnostic(QStringLiteral("guard rejected: %1; cursor=(%2,%3)")
            .arg(failedGuards.join(QStringLiteral(", ")))
            .arg(widgetPos.x())
            .arg(widgetPos.y()));
#endif
        return std::nullopt;
    }

    const std::optional<std::int32_t> owner = framePlacementOwnerThingIndex();
    if (!owner)
    {
#ifdef SED_APP_MAIN_WINDOW_TEST_ACCESS
        setDiagnostic(QStringLiteral("no owner; selected things=%1, selected frames=%2")
            .arg(static_cast<qulonglong>(document_.selection().things.size()))
            .arg(static_cast<qulonglong>(document_.selection().frames.size())));
#endif
        return std::nullopt;
    }

    const FramePlacementOptions options = framePlacementOptions(modifiers);
    bool haveSurfaceContact = false;
    SurfaceRef surface{};
    QVector3D surfaceNormal{};
    TVector documentPosition{};
    const std::optional<QVector3D> position = glView_->placementPointForViewport(
        widgetPos,
        &haveSurfaceContact,
        &surface,
        &surfaceNormal,
        options.surfaceStick,
        !options.surfaceStick,
        &documentPosition);
    if (!position
        || !std::isfinite(documentPosition.x)
        || !std::isfinite(documentPosition.y)
        || !std::isfinite(documentPosition.z))
    {
#ifdef SED_APP_MAIN_WINDOW_TEST_ACCESS
        setDiagnostic(QStringLiteral(
            "no target position; owner=%1, position=%2, contact=%3, doc=(%4,%5,%6), modifiers=%7")
            .arg(*owner)
            .arg(position.has_value())
            .arg(haveSurfaceContact)
            .arg(documentPosition.x, 0, 'g', 17)
            .arg(documentPosition.y, 0, 'g', 17)
            .arg(documentPosition.z, 0, 'g', 17)
            .arg(static_cast<int>(modifiers)));
#endif
        return std::nullopt;
    }

    const std::optional<Thing> pose = buildFramePlacementPose(
        *owner,
        *position,
        surfaceNormal,
        modifiers,
        haveSurfaceContact,
        &documentPosition);
    if (!pose)
    {
#ifdef SED_APP_MAIN_WINDOW_TEST_ACCESS
        setDiagnostic(QStringLiteral("pose rejected; owner=%1, contact=%2, surfaceStick=%3, surfaceAlign=%4, snap=%5")
            .arg(*owner)
            .arg(haveSurfaceContact)
            .arg(options.surfaceStick)
            .arg(options.surfaceAlign)
            .arg(options.snapToThing));
#endif
        return std::nullopt;
    }

    const bool surfaceRefValid = haveSurfaceContact
        && surface.sector >= 0
        && static_cast<std::size_t>(surface.sector)
            < document_.level().sectors.size()
        && surface.surface >= 0
        && static_cast<std::size_t>(surface.surface)
            < document_.level().sectors[
                static_cast<std::size_t>(surface.sector)].surfaces.size();
    FramePlacementPreview preview{
        .ownerThingIndex = *owner,
        .widgetPosition = widgetPos,
        .modifiers = modifiers & ui::shortcuts::editorModifierMask(),
        .documentSignature = document_.undoSignature(),
        .resourceIdentity = resources_.previewCacheIdentity(),
        .options = options,
        .position = *position,
        .documentPosition = documentPosition,
        .surfaceNormal = surfaceNormal,
        .surfaceSector = surfaceRefValid ? surface.sector : -1,
        .surfaceIndex = surfaceRefValid ? surface.surface : -1,
        .haveSurfaceContact = haveSurfaceContact,
        .pose = *pose,
    };
#ifdef SED_APP_MAIN_WINDOW_TEST_ACCESS
    setDiagnostic(QStringLiteral(
        "candidate owner=%1 pose=(%2,%3,%4;%5,%6,%7), options=%8/%9/%10/%11, modifiers=%12, contact=%13 surface=%14/%15")
        .arg(*owner)
        .arg(pose->x, 0, 'g', 17)
        .arg(pose->y, 0, 'g', 17)
        .arg(pose->z, 0, 'g', 17)
        .arg(pose->pch, 0, 'g', 17)
        .arg(pose->yaw, 0, 'g', 17)
        .arg(pose->rol, 0, 'g', 17)
        .arg(options.surfaceStick)
        .arg(options.surfaceAlign)
        .arg(options.snapToThing)
        .arg(options.rotateToSnap)
        .arg(static_cast<int>(modifiers))
        .arg(haveSurfaceContact)
        .arg(surfaceRefValid ? surface.sector : -1)
        .arg(surfaceRefValid ? surface.surface : -1));
#endif
    return preview;
}

std::optional<MainWindow::FramePlacementPreview>
MainWindow::framePlacementPreviewForCommit(
    const QPoint& widgetPos,
    const Qt::KeyboardModifiers modifiers)
{
    const std::optional<std::int32_t> owner = framePlacementOwnerThingIndex();
    const Qt::KeyboardModifiers held =
        modifiers & ui::shortcuts::editorModifierMask();
    const FramePlacementOptions options = framePlacementOptions(held);
    const std::uint64_t documentSignature = document_.undoSignature();
    const std::uint64_t resourceIdentity = resources_.previewCacheIdentity();
    if (framePlacementPreview_
        && owner
        && framePlacementPreview_->ownerThingIndex == *owner
        && framePlacementPreview_->widgetPosition == widgetPos
        && framePlacementPreview_->modifiers == held
        && framePlacementPreview_->documentSignature == documentSignature
        && framePlacementPreview_->resourceIdentity == resourceIdentity
        && framePlacementPreview_->options == options)
    {
        return framePlacementPreview_;
    }

    framePlacementPreview_ = resolveFramePlacementPreview(widgetPos, held);
    if (!framePlacementPreview_ && glView_)
    {
        glView_->clearPlacementPreviewThing();
    }
    return framePlacementPreview_;
}

void MainWindow::updateFramePlacementPreview(
    const QPoint& widgetPos,
    const Qt::KeyboardModifiers modifiers)
{
    if (!glView_)
    {
        framePlacementPreview_.reset();
        return;
    }
    framePlacementPreview_ = resolveFramePlacementPreview(widgetPos, modifiers);
    if (!framePlacementPreview_)
    {
        glView_->clearPlacementPreviewThing();
        return;
    }
    glView_->clearPlacementPreviewLines();
    glView_->setPlacementPreviewThing(framePlacementPreview_->pose);
}

void MainWindow::clearFramePlacementPreview()
{
    const bool frameAddOwnsViewportPreview = framePlacementPreview_.has_value()
        || (editMode_ == EditMode::frame
            && viewportFrameAddAction_
            && viewportFrameAddAction_->isChecked());
    framePlacementPreview_.reset();
    if (frameAddOwnsViewportPreview && glView_)
    {
        glView_->clearPlacementPreviewThing();
    }
}

void MainWindow::ensureThingPlacementTemplateCache(
    const std::string_view thingName) const
{
    const assets::Templates* templatesDb = templates_.size() > 0 ? &templates_ : nullptr;
    Thing candidate{};
    candidate.name = std::string(thingName);
    candidate.templateName = std::string(thingName);
    const std::string model = sed::thing::thingModel3doName(document_.level(), templatesDb, candidate);
    const std::string sprite = sed::thing::templateSpriteName(document_.level(), templatesDb, thingName);
#ifdef SED_APP_MAIN_WINDOW_TEST_ACCESS
    ++thingPlacementTemplateRevisionChecks_;
#endif
    const std::string revision = model + "|" + ui::detail::previewResourceRevisionToken(&resources_, model, ".3do")
        + "|" + sprite + "|" + ui::detail::previewResourceRevisionToken(&resources_, sprite, ".spr");
    if (thingPlacementSnapTemplateCacheValid_
        && thingPlacementSnapTemplateCacheName_ == thingName
        && thingPlacementSnapTemplateResourceRevision_ == revision)
    {
        return;
    }

    thingPlacementSnapTemplateResourceRevision_ = revision;
    thingPlacementSnapAnchorCacheValid_ = false;
    thingPlacementSnapTemplateCacheName_ = std::string(thingName);
    thingPlacementSnapTemplateBoxCache_ = resolvedThingTemplateBox(thingName);
    thingPlacementSnapTemplateConnectorCache_ =
        resolvedThingTemplateConnectors(thingName);
    thingPlacementTemplateVisualKindCache_ = thingTemplateVisualKind(thingName);
    thingPlacementTemplateGroundOffsetCache_ = thingPlacementGroundOffsetZ(thingName);
    thingPlacementTemplateSpriteOffsetCache_ = thingPlacementSpriteOffset(thingName);
    thingPlacementSnapTemplateCacheValid_ = true;
}

void MainWindow::ensureThingPlacementSnapAnchorCache() const
{
    const Level& level = document_.level();
    const std::uint64_t undoSignature = document_.undoSignature();
    if (thingPlacementSnapAnchorCacheValid_
        && thingPlacementSnapAnchorCacheUndoSignature_ == undoSignature
        && thingPlacementSnapAnchorCacheThingCount_ == level.things.size())
    {
        return;
    }

    thingPlacementSnapAnchorCache_.clear();
    thingPlacementSnapAnchorCache_.reserve(level.things.size());
    for (std::size_t thingIndex = 0; thingIndex < level.things.size(); ++thingIndex)
    {
        const Thing& anchor = level.things[thingIndex];
        ThingPlacementSnapAnchorCacheEntry cached{};
        cached.thingIndex = static_cast<std::int32_t>(thingIndex);
        cached.finite =
            std::isfinite(anchor.x) && std::isfinite(anchor.y) && std::isfinite(anchor.z)
            && std::isfinite(anchor.pch) && std::isfinite(anchor.yaw) && std::isfinite(anchor.rol);
        if (!cached.finite)
        {
            thingPlacementSnapAnchorCache_.push_back(std::move(cached));
            continue;
        }

        cached.origin = TVector{ anchor.x, anchor.y, anchor.z };
        cached.pyr.pitch = anchor.pch;
        cached.pyr.yaw = anchor.yaw;
        cached.pyr.roll = anchor.rol;
        cached.transform = TMatrix43::Create(cached.origin, cached.pyr);
        cached.inverseTransform = cached.transform.InvertOrtho();

        cached.box = resolvedPlacedThingBox(anchor);
        if (cached.box && !cached.box->IsZero())
        {
            const sed::thing::BoxMinMax bounds = sed::thing::boxMinMax(*cached.box);
            cached.boxMin = bounds.min;
            cached.boxMax = bounds.max;
            cached.boxCenter = sed::thing::boxCenter(bounds);
            const double hx = std::max(
                std::abs(cached.boxMin[0]),
                std::abs(cached.boxMax[0]));
            const double hy = std::max(
                std::abs(cached.boxMin[1]),
                std::abs(cached.boxMax[1]));
            const double hz = std::max(
                std::abs(cached.boxMin[2]),
                std::abs(cached.boxMax[2]));
            cached.boxRadius = std::sqrt(hx * hx + hy * hy + hz * hz);
        }

        const std::vector<ThingPlacementConnector> connectors =
            resolvedPlacedThingConnectors(anchor);
        cached.connectors.reserve(connectors.size());
        for (const ThingPlacementConnector& connector : connectors)
        {
            ThingPlacementCachedConnector worldConnector{};
            worldConnector.connector = connector;
            worldConnector.worldPosition = cached.transform.TransformPoint(connector.position);
            if (const std::optional<TVector> tangent = sed::thing::flattenNormalize(
                    cached.transform.TransformVector(connector.tangent)))
            {
                worldConnector.worldTangent = *tangent;
                worldConnector.haveWorldTangent = true;
            }
            cached.connectors.push_back(std::move(worldConnector));
        }

        thingPlacementSnapAnchorCache_.push_back(std::move(cached));
    }

    thingPlacementSnapAnchorCacheUndoSignature_ = undoSignature;
    thingPlacementSnapAnchorCacheThingCount_ = level.things.size();
    thingPlacementSnapAnchorCacheValid_ = true;
}

double MainWindow::thingPlacementGroundOffsetZ(const std::string_view thingName) const
{
    static ui::detail::BoundedTwoQueueCache<double> modelInsertZCache(512u, [](const double&) { return 1u; });
    static ui::detail::BoundedTwoQueueCache<double> spriteOffsetZCache(512u, [](const double&) { return 1u; });

    const Level& level = document_.level();
    const assets::Templates* templatesDb = (templates_.size() > 0) ? &templates_ : nullptr;

    Thing temp{};
    temp.name = std::string(thingName);
    temp.templateName = std::string(thingName);

    auto valueString = [](const TplValue* value) -> std::string
    {
        if (!value)
        {
            return {};
        }
        return value->stringValue.empty() ? value->asString() : value->stringValue;
    };

    auto valueDouble = [](const TplValue* value) -> std::optional<double>
    {
        if (!value)
        {
            return std::nullopt;
        }
        if (value->type == ValueType::floating)
        {
            return value->floatValue;
        }
        if (value->type == ValueType::integer)
        {
            return static_cast<double>(value->intValue);
        }

        const std::string text = value->stringValue.empty() ? value->asString() : value->stringValue;
        try
        {
            std::size_t end = 0;
            const double d = std::stod(text, &end);
            return (end > 0) ? std::optional<double>(d) : std::nullopt;
        }
        catch (...)
        {
            return std::nullopt;
        }
    };

    const std::string move = sed::text::lowerAscii(sed::text::trimView(valueString(
        sed::level::thingFindValue(level, templatesDb, temp, "move"))));
    if (move == "physics")
    {
        if (const std::optional<double> height =
                valueDouble(sed::level::thingFindValue(level, templatesDb, temp, "height")))
        {
            if (std::isfinite(*height) && std::abs(*height) > 1e-9)
            {
                return *height;
            }
        }
    }

    const TplValue* modelValue = sed::level::thingFindValue(level, templatesDb, temp, "model3d");
    if (!modelValue)
    {
        modelValue = sed::level::thingFindValue(level, templatesDb, temp, "model3do");
    }

    const std::string modelName = sed::text::trim(valueString(modelValue));
    if (!modelName.empty())
    {
        std::string key = modelName;
        if (!sed::text::iendsWithAscii(key, ".3do"))
        {
            key += ".3do";
        }

        const std::string cacheKey = key + "|" + ui::detail::previewResourceRevisionToken(&resources_, key, ".3do");
        double cached{};
        if (modelInsertZCache.find(cacheKey, cached))
        {
            return cached;
        }

        double z = 0.0;
        try
        {
            if (const std::optional<std::string> text = resources_.tryReadTextWithExt(key, ".3do"))
            {
                std::istringstream iss(*text);
                const assets::Model3do model = assets::loadFrom3DO(iss);
                z = model.insertOffset.z;
            }
        }
        catch (...)
        {
            z = 0.0;
        }

        modelInsertZCache.insertOrAssign(cacheKey, z);
        return z;
    }

    const std::string spriteName = sed::text::trim(valueString(
        sed::level::thingFindValue(level, templatesDb, temp, "sprite")));
    if (!spriteName.empty())
    {
        std::string key = spriteName;
        if (!sed::text::iendsWithAscii(key, ".spr"))
        {
            key += ".spr";
        }

        const std::string cacheKey = key + "|" + ui::detail::previewResourceRevisionToken(&resources_, key, ".spr");
        double cached{};
        if (spriteOffsetZCache.find(cacheKey, cached))
        {
            return cached;
        }

        double z = 0.0;
        try
        {
            if (const std::optional<std::string> text = resources_.tryReadTextWithExt(key, ".spr"))
            {
                std::istringstream iss(*text);
                const assets::SprFile spr = assets::loadFromSPR(iss);
                z = static_cast<double>(spr.offsetXYZ[2]);
            }
        }
        catch (...)
        {
            z = 0.0;
        }

        spriteOffsetZCache.insertOrAssign(cacheKey, z);
        return z;
    }

    return 0.0;
}

std::optional<QVector3D> MainWindow::thingPlacementSpriteOffset(const std::string_view thingName) const
{
    static ui::detail::BoundedTwoQueueCache<QVector3D> spriteOffsetCache(512u, [](const QVector3D&) { return 1u; });

    const Level& level = document_.level();
    const assets::Templates* templatesDb = (templates_.size() > 0) ? &templates_ : nullptr;

    Thing temp{};
    temp.name = std::string(thingName);
    temp.templateName = std::string(thingName);

    auto valueString = [](const TplValue* value) -> std::string
    {
        if (!value)
        {
            return {};
        }
        return value->stringValue.empty() ? value->asString() : value->stringValue;
    };

    std::string key = sed::text::trim(valueString(
        sed::level::thingFindValue(level, templatesDb, temp, "sprite")));
    if (key.empty())
    {
        return std::nullopt;
    }
    if (!sed::text::iendsWithAscii(key, ".spr"))
    {
        key += ".spr";
    }

    const std::string cacheKey = key + "|" + ui::detail::previewResourceRevisionToken(&resources_, key, ".spr");
    QVector3D cached{};
    if (spriteOffsetCache.find(cacheKey, cached))
    {
        return cached;
    }

    try
    {
        if (const std::optional<std::string> text = resources_.tryReadTextWithExt(key, ".spr"))
        {
            std::istringstream iss(*text);
            const assets::SprFile spr = assets::loadFromSPR(iss);
            const QVector3D offset(spr.offsetXYZ[0], spr.offsetXYZ[1], spr.offsetXYZ[2]);
            spriteOffsetCache.insertOrAssign(cacheKey, offset);
            return offset;
        }
    }
    catch (...)
    {
    }

    return std::nullopt;
}

std::optional<QVector3D> MainWindow::thingPlacementSurfaceNormal(
    const std::int32_t sectorIndex,
    const std::int32_t surfaceIndex) const
{
    const Level& level = document_.level();
    if (sectorIndex < 0
        || surfaceIndex < 0
        || static_cast<std::size_t>(sectorIndex) >= level.sectors.size())
    {
        return std::nullopt;
    }

    const Sector& sector = level.sectors[static_cast<std::size_t>(sectorIndex)];
    if (static_cast<std::size_t>(surfaceIndex) >= sector.surfaces.size())
    {
        return std::nullopt;
    }

    const Surface& surface = sector.surfaces[static_cast<std::size_t>(surfaceIndex)];
    TVector normal = surface.normal;
    if (normal.IsZero())
    {
        normal = sed::level::surfaceCalcNormalNewell(sector, surface);
    }
    if (normal.Normalize() == 0.0)
    {
        return std::nullopt;
    }

    return QVector3D(
        static_cast<float>(normal.x),
        static_cast<float>(normal.y),
        static_cast<float>(normal.z));
}

std::optional<TVector> MainWindow::thingPlacementSurfaceAlignedPyr(const QVector3D& surfaceNormal) const
{
    TVector up{
        static_cast<double>(surfaceNormal.x()),
        static_cast<double>(surfaceNormal.y()),
        static_cast<double>(surfaceNormal.z())
    };
    if (up.Normalize() == 0.0)
    {
        return std::nullopt;
    }

    TVector forward = TVector::forward - up.Scaled(TVector::forward.Dot(up));
    if (forward.Normalize() == 0.0)
    {
        forward = TVector::right - up.Scaled(TVector::right.Dot(up));
        if (forward.Normalize() == 0.0)
        {
            return std::nullopt;
        }
    }

    TVector right = forward.Cross(up);
    if (right.Normalize() == 0.0)
    {
        return std::nullopt;
    }

    TMatrix43 rot{};
    rot.rvec = right;
    rot.lvec = forward;
    rot.uvec = up;
    rot.dvec = TVector::zero;
    return ui::transform::extractPYRCreateRotateCompatible(rot);
}

std::optional<TVector> MainWindow::thingPlacementSpriteSurfaceAlignedPyr(const QVector3D& surfaceNormal) const
{
    TVector forward{
        static_cast<double>(surfaceNormal.x()),
        static_cast<double>(surfaceNormal.y()),
        static_cast<double>(surfaceNormal.z())
    };
    if (forward.Normalize() == 0.0)
    {
        return std::nullopt;
    }

    TVector up = TVector::up - forward.Scaled(TVector::up.Dot(forward));
    if (up.Normalize() == 0.0)
    {
        up = TVector::forward - forward.Scaled(TVector::forward.Dot(forward));
        if (up.Normalize() == 0.0)
        {
            up = TVector::right - forward.Scaled(TVector::right.Dot(forward));
            if (up.Normalize() == 0.0)
            {
                return std::nullopt;
            }
        }
    }

    TVector right = forward.Cross(up);
    if (right.Normalize() == 0.0)
    {
        return std::nullopt;
    }

    TMatrix43 rot{};
    rot.rvec = right;
    rot.lvec = forward;
    rot.uvec = up;
    rot.dvec = TVector::zero;
    return ui::transform::extractPYRCreateRotateCompatible(rot);
}

Thing MainWindow::buildThingPlacementPreviewThing(
    const QVector3D& surfacePosition,
    const QVector3D& surfaceNormal,
    const Qt::KeyboardModifiers modifiers,
    const std::int32_t fallbackSector,
    const TVector* documentSurfacePosition,
    const bool templateCacheValidated) const
{
    const std::string thingName = currentThingTemplateName();
    if (!templateCacheValidated)
    {
        ensureThingPlacementTemplateCache(thingName);
    }
    const double groundOffsetZ = thingPlacementTemplateGroundOffsetCache_;
    const bool freeInsert = modifiers.testFlag(Qt::ShiftModifier);
    const bool controlTilt = modifiers.testFlag(Qt::ControlModifier);
    const ThingPlacementVisualKind visualKind = thingPlacementTemplateVisualKindCache_;
    const bool surfaceAlignedSprite = !freeInsert && controlTilt && visualKind == ThingPlacementVisualKind::sprite;
    const bool surfaceAlignedObject = !freeInsert && controlTilt && visualKind != ThingPlacementVisualKind::sprite;
    std::optional<TVector> alignedPyr{};
    if (!freeInsert)
    {
        if (surfaceAlignedSprite)
        {
            alignedPyr = thingPlacementSpriteSurfaceAlignedPyr(surfaceNormal);
        }
        else if (surfaceAlignedObject)
        {
            alignedPyr = thingPlacementSurfaceAlignedPyr(surfaceNormal);
        }
    }

    QVector3D offsetDir = (surfaceAlignedSprite || surfaceAlignedObject)
        ? QVector3D(surfaceNormal).normalized()
        : QVector3D(0.0f, 0.0f, 1.0f);
    if (offsetDir.lengthSquared() <= 1e-8f)
    {
        offsetDir = QVector3D(0.0f, 0.0f, 1.0f);
    }

    double surfaceNormalOffset = groundOffsetZ;
    if (surfaceAlignedSprite)
    {
        // Sprite rendering offsets the textured plane along local forward. Counter that offset
        // so the actual quad, not just the thing origin, lands on the picked surface.
        surfaceNormalOffset = 0.0;
        if (thingPlacementTemplateSpriteOffsetCache_)
        {
            surfaceNormalOffset = -static_cast<double>(
                thingPlacementTemplateSpriteOffsetCache_->y());
        }
    }

    const TVector surfacePoint = documentSurfacePosition
        ? *documentSurfacePosition : TVector{surfacePosition.x(), surfacePosition.y(), surfacePosition.z()};
    const TVector thingPosition = surfacePoint
        + TVector{offsetDir.x(), offsetDir.y(), offsetDir.z()} * surfaceNormalOffset;

    Thing thing{};
    thing.name = thingName;
    thing.templateName = thingName;
    if (thingPlacementSnapTemplateBoxCache_)
    {
        thing.bbox = *thingPlacementSnapTemplateBoxCache_;
    }
    thing.x = thingPosition.x;
    thing.y = thingPosition.y;
    thing.z = thingPosition.z;
    if (alignedPyr)
    {
        thing.pch = alignedPyr->pitch;
        thing.yaw = alignedPyr->yaw;
        thing.rol = alignedPyr->roll;
    }
    thing.yaw += thingPlacementYawOffset_;
    if (glView_)
    {
        thing.sector = glView_->findContainingSector(thingPosition, fallbackSector);
    }
    else
    {
        const bool fallbackContainsFinalPoint = fallbackSector >= 0
            && static_cast<std::size_t>(fallbackSector) < document_.level().sectors.size()
            && sed::level::sectorContainsPoint(
                document_.level().sectors[static_cast<std::size_t>(fallbackSector)],
                thingPosition.x,
                thingPosition.y,
                thingPosition.z);
        thing.sector = fallbackContainsFinalPoint
            ? fallbackSector
            : sed::level::levelFindSectorForXYZ(
                document_.level(), thing.x, thing.y, thing.z);
    }
    thing.layer = resolvedThingPlacementLayer(thing.sector);
    return thing;
}

std::optional<Thing> MainWindow::buildFramePlacementPose(
    const std::int32_t ownerThingIndex,
    const QVector3D& placementPosition,
    QVector3D surfaceNormal,
    const Qt::KeyboardModifiers modifiers,
    const bool haveSurfaceContact,
    const TVector* documentPlacementPosition) const
{
    const Level& level = document_.level();
    if (ownerThingIndex < 0
        || static_cast<std::size_t>(ownerThingIndex) >= level.things.size())
    {
        return std::nullopt;
    }

    const std::uint64_t documentSignature = document_.undoSignature();
    const std::uint64_t resourceIdentity = resources_.previewCacheIdentity();
    if (!framePlacementOwnerProxyCache_
        || framePlacementOwnerProxyCache_->thingIndex != ownerThingIndex
        || framePlacementOwnerProxyCache_->documentSignature != documentSignature
        || framePlacementOwnerProxyCache_->resourceIdentity != resourceIdentity)
    {
        const Thing& owner = level.things[static_cast<std::size_t>(ownerThingIndex)];
        FramePlacementOwnerProxy proxy{};
        proxy.thingIndex = ownerThingIndex;
        proxy.documentSignature = documentSignature;
        proxy.resourceIdentity = resourceIdentity;
        proxy.thing = owner;
        proxy.lastFrameOrientation = TVector{
            owner.pch, owner.yaw, owner.rol };

        // Frame values belong to the path, not the visual owner used for a
        // temporary placement preview. Parse the last authored point once,
        // then keep the owner's local overrides, template identity and layer.
        for (std::size_t i = owner.values.size(); i-- > 0;)
        {
            const TplValue& value = owner.values[i];
            if (value.type != ValueType::frame
                || !sed::text::iequalsAscii(value.name, "frame"))
            {
                continue;
            }
            if (const std::optional<Thing> previous =
                    sed::thing::thingAtFramePose(owner, value))
            {
                proxy.lastFrameOrientation = TVector{
                    previous->pch, previous->yaw, previous->rol };
                break;
            }
        }
        std::erase_if(
            proxy.thing.values,
            [](const TplValue& value)
            {
                return value.type == ValueType::frame
                    && sed::text::iequalsAscii(value.name, "frame");
            });
        framePlacementOwnerProxyCache_ = std::move(proxy);
    }

    const FramePlacementOwnerProxy& ownerProxy = *framePlacementOwnerProxyCache_;
    Thing posed = ownerProxy.thing;
    posed.pch = ownerProxy.lastFrameOrientation.x;
    posed.yaw = ownerProxy.lastFrameOrientation.y;
    posed.rol = ownerProxy.lastFrameOrientation.z;

    // Resolve against persistent base preferences. The shortcut state is
    // applied here once, without reading the temporarily checked overlay
    // buttons which already reflect that same held modifier.
    const FramePlacementOptions options = framePlacementOptions(modifiers);
    const bool surfaceStick = options.surfaceStick;
    const bool surfaceAlign = options.surfaceAlign;
    const bool snapToThing = options.snapToThing;
    const bool rotateToSnap = options.rotateToSnap;

    if (surfaceNormal.lengthSquared() <= 1e-8f)
    {
        surfaceNormal = QVector3D(0.0f, 0.0f, 1.0f);
    }
    surfaceNormal.normalize();

    if (surfaceStick && surfaceAlign && haveSurfaceContact)
    {
        auto valueString = [](const TplValue* value) -> std::string
        {
            if (!value)
            {
                return {};
            }
            return value->stringValue.empty() ? value->asString() : value->stringValue;
        };
        const TplValue* spriteValue = sed::level::thingFindValue(
            level,
            templates_.size() > 0 ? &templates_ : nullptr,
            posed,
            "sprite");
        const std::string spriteName = sed::text::trim(valueString(spriteValue));
        const bool isSprite = !spriteName.empty()
            && !sed::text::iequalsAscii(spriteName, "none");
        const std::optional<TVector> aligned =
            isSprite
                ? thingPlacementSpriteSurfaceAlignedPyr(surfaceNormal)
                : thingPlacementSurfaceAlignedPyr(surfaceNormal);
        if (aligned)
        {
            posed.pch = aligned->pitch;
            posed.yaw = aligned->yaw;
            posed.rol = aligned->roll;
        }
    }

    const TVector point = documentPlacementPosition ? *documentPlacementPosition
        : TVector{placementPosition.x(), placementPosition.y(), placementPosition.z()};
    posed.x = point.x;
    posed.y = point.y;
    posed.z = point.z;

    if (surfaceStick && haveSurfaceContact)
    {
        // Resolve support geometry around zero so a small model/sprite extent
        // is not lost by subtracting two rounded absolute world positions.
        Thing localPose = posed;
        localPose.x = 0.0;
        localPose.y = 0.0;
        localPose.z = 0.0;
        const std::vector<TVector> supports = sed::thing::resolveThingDocumentSupportPoints(
            level, &templates_, &resources_, localPose, 0.025);
        TVector normal{surfaceNormal.x(), surfaceNormal.y(), surfaceNormal.z()};
        const double normalLength = std::hypot(normal.x, normal.y, normal.z);
        if (normalLength > 0.0) normal = TVector{normal.x / normalLength, normal.y / normalLength, normal.z / normalLength};
        double minimumProjection = 0.0;
        bool haveProjection = false;
        for (const TVector& point : supports)
        {
            const double projection = point.Dot(normal);
            if (!haveProjection || projection < minimumProjection)
            {
                minimumProjection = projection;
                haveProjection = true;
            }
        }
        if (haveProjection)
        {
            posed.x -= normal.x * minimumProjection;
            posed.y -= normal.y * minimumProjection;
            posed.z -= normal.z * minimumProjection;
        }
    }

    if (snapToThing)
    {
        if (std::optional<Thing> snapped = snapThingPlacementPose(
                posed,
                placementPosition,
                surfaceNormal,
                rotateToSnap,
                0.0,
                ownerThingIndex,
                false))
        {
            posed = std::move(*snapped);
        }
    }

    return sed::thing::finiteThingTransform(posed)
        ? std::optional<Thing>{std::move(posed)}
        : std::nullopt;
}

std::optional<Thing> MainWindow::buildThingPlacementSnapThing(
    const QVector3D& surfacePosition,
    const QVector3D& surfaceNormal,
    const Qt::KeyboardModifiers modifiers,
    const std::int32_t fallbackSector,
    const TVector* documentSurfacePosition,
    const bool templateCacheValidated) const
{
    const std::string thingName = currentThingTemplateName();
    if (!templateCacheValidated)
    {
        ensureThingPlacementTemplateCache(thingName);
    }

    Thing baseThing = buildThingPlacementPreviewThing(
        surfacePosition,
        surfaceNormal,
        modifiers,
        fallbackSector, documentSurfacePosition, true);
    return snapThingPlacementPose(
        std::move(baseThing),
        surfacePosition,
        surfaceNormal,
        thingPlacementSnapRotate_,
        thingPlacementYawOffset_,
        -1,
        true);
}

std::optional<Thing> MainWindow::snapThingPlacementPose(
    Thing baseThing,
    const QVector3D& probePosition,
    const QVector3D& surfaceNormal,
    const bool rotateToSnap,
    const double yawOffset,
    const std::int32_t excludedThingIndex,
    const bool requireInsideLevel) const
{
    const std::optional<ThingBox> newBoxOpt = resolvedPlacedThingBox(baseThing);

    if (!newBoxOpt || newBoxOpt->IsZero())
    {
        return std::nullopt;
    }

    const Level& level = document_.level();
    if (level.things.empty())
    {
        return std::nullopt;
    }
    ensureThingPlacementSnapAnchorCache();

    const sed::thing::BoxMinMax newBounds = sed::thing::boxMinMax(*newBoxOpt);
    const std::array<double, 3> newCenter = sed::thing::boxCenter(newBounds);
    const std::array<double, 3>& newMin = newBounds.min;
    const std::array<double, 3>& newMax = newBounds.max;

    float unitsPerPixel = glView_ ? glView_->worldUnitsPerPixelAtWorld(probePosition) : 0.01f;
    if (!std::isfinite(unitsPerPixel) || unitsPerPixel <= 0.0f)
    {
        unitsPerPixel = 0.01f;
    }
    const double baseSnapRadius = glView_
        ? static_cast<double>(glView_->thingSnapRadius())
        : 0.16;
    const double snapReach = std::clamp(
        std::max(baseSnapRadius * 0.3125,
                 static_cast<double>(unitsPerPixel) * 18.0),
        baseSnapRadius * 0.3125,
        baseSnapRadius * 2.1875);
    const double snapRotationLimit = glView_
        ? static_cast<double>(glView_->snapRotationMaxDegrees())
        : 180.0;

    const TVector surfaceVec{
        static_cast<double>(probePosition.x()),
        static_cast<double>(probePosition.y()),
        static_cast<double>(probePosition.z())
    };
    TVector surfaceNormalVec{
        static_cast<double>(surfaceNormal.x()),
        static_cast<double>(surfaceNormal.y()),
        static_cast<double>(surfaceNormal.z())
    };
    if (surfaceNormalVec.Normalize() == 0.0)
    {
        surfaceNormalVec = TVector::up;
    }

    struct SnapCandidate
    {
        Thing thing{};
        double score{std::numeric_limits<double>::max()};
    };
    std::optional<SnapCandidate> best{};

    TVector basePyr{};
    basePyr.pitch = baseThing.pch;
    basePyr.yaw = baseThing.yaw;
    basePyr.roll = baseThing.rol;
    const TMatrix43 baseRot = TMatrix43::Create(TVector::zero, basePyr);

    const std::vector<ThingPlacementConnector> newConnectors =
        resolvedPlacedThingConnectors(baseThing);
    if (!newConnectors.empty())
    {
        const TVector baseOrigin{ baseThing.x, baseThing.y, baseThing.z };
        const TMatrix43 baseXf = TMatrix43::Create(baseOrigin, basePyr);

        for (const ThingPlacementSnapAnchorCacheEntry& anchor : thingPlacementSnapAnchorCache_)
        {
            if (anchor.thingIndex == excludedThingIndex
                || !anchor.finite || anchor.connectors.empty())
            {
                continue;
            }

            for (const ThingPlacementCachedConnector& cachedConnector : anchor.connectors)
            {
                const ThingPlacementConnector& anchorConnector = cachedConnector.connector;
                const TVector& anchorWorld = cachedConnector.worldPosition;
                const double cursorDistSq = surfaceVec.DistanceSqr(anchorWorld);

                if (!cachedConnector.haveWorldTangent)
                {
                    continue;
                }
                const TVector& anchorTangentWorld = cachedConnector.worldTangent;
                const TVector desiredNewTangent = -anchorTangentWorld;

                for (const ThingPlacementConnector& newConnector : newConnectors)
                {
                    const TVector baseConnectorWorld = baseXf.TransformPoint(newConnector.position);
                    const double connectorDistSq = baseConnectorWorld.DistanceSqr(anchorWorld);
                    const bool railPairSnap =
                        sed::thing::connectorHasRailPair(anchorConnector)
                        && sed::thing::connectorHasRailPair(newConnector);
                    const double connectorReach =
                        sed::thing::connectorSnapReach(
                            static_cast<double>(unitsPerPixel),
                            railPairSnap,
                            baseSnapRadius);
                    const double connectorReachSq =
                        connectorReach * connectorReach;
                    const double probeDistSq = sed::thing::connectorSnapProbeDistanceSq(
                        connectorDistSq,
                        cursorDistSq,
                        railPairSnap);
                    if (probeDistSq > connectorReachSq)
                    {
                        continue;
                    }

                    if (railPairSnap)
                    {
                        const std::vector<sed::thing::RailPairSnapCandidate> railCandidates =
                            sed::thing::buildRailPairSnapCandidates(
                                newConnector,
                                anchorConnector,
                                basePyr,
                                baseRot,
                                anchor.transform,
                                desiredNewTangent,
                                rotateToSnap,
                                0.45,
                                snapRotationLimit);

                        for (const sed::thing::RailPairSnapCandidate& railSnap : railCandidates)
                        {
                            const TVector& newOrigin = railSnap.origin;
                            const double originJumpSq = baseOrigin.DistanceSqr(newOrigin);
                            const double score =
                                railSnap.pairResidualSq * 220.0
                                + connectorDistSq * 0.65
                                + originJumpSq * 0.030
                                + railSnap.tangentPenalty;
                            if (best && score >= best->score)
                            {
                                continue;
                            }

                            Thing snapped = baseThing;
                            snapped.pch = railSnap.pyr.pitch;
                            snapped.yaw = railSnap.pyr.yaw;
                            snapped.rol = railSnap.pyr.roll;
                            snapped.x = newOrigin.x;
                            snapped.y = newOrigin.y;
                            snapped.z = newOrigin.z;
                            if (newBoxOpt)
                            {
                                snapped.bbox = *newBoxOpt;
                            }
                            snapped.sector = glView_
                                ? glView_->findContainingSector(
                                    TVector{snapped.x, snapped.y, snapped.z},
                                    snapped.sector)
                                : sed::level::levelFindSectorForXYZ(
                                    level, snapped.x, snapped.y, snapped.z);
                            if (requireInsideLevel && snapped.sector < 0)
                            {
                                continue;
                            }
                            snapped.layer = resolvedThingPlacementLayer(snapped.sector);

                            best = SnapCandidate{ std::move(snapped), score };
                        }
                        continue;
                    }

                    TVector newPyr = basePyr;
                    TMatrix43 newRot = baseRot;
                    double tangentPenalty = 0.0;
                    bool rotatedToConnector = false;
                    if (rotateToSnap)
                    {
                        std::optional<TVector> currentNewTangent =
                            sed::thing::flattenNormalize(newRot.TransformVector(newConnector.tangent));
                        if (!currentNewTangent)
                        {
                            continue;
                        }
                        const double proposedYaw = sed::thing::yawDeltaDeg(
                            *currentNewTangent, desiredNewTangent);
                        if (std::abs(proposedYaw)
                            <= snapRotationLimit + 1.0e-8)
                        {
                            newPyr.yaw += proposedYaw;
                            newRot = TMatrix43::Create(
                                TVector::zero, newPyr);
                            rotatedToConnector = true;
                        }
                    }
                    if (!rotatedToConnector)
                    {
                        std::optional<TVector> newTangentWorld =
                            sed::thing::flattenNormalize(newRot.TransformVector(newConnector.tangent));
                        if (newTangentWorld)
                        {
                            const double tangentDot = std::clamp(
                                newTangentWorld->Dot(anchorTangentWorld),
                                -1.0,
                                1.0);
                            tangentPenalty = (1.0 + tangentDot) * (1.0 + tangentDot) * 0.015;
                        }
                    }

                    const TVector newOrigin = anchorWorld - newRot.TransformVector(newConnector.position);
                    const double originJumpSq = baseOrigin.DistanceSqr(newOrigin);
                    const double score =
                        probeDistSq
                        + originJumpSq * 0.035
                        + (cursorDistSq > connectorReachSq ? connectorDistSq * 0.25 : cursorDistSq * 0.05)
                        + tangentPenalty;
                    if (best && score >= best->score)
                    {
                        continue;
                    }

                    Thing snapped = baseThing;
                    snapped.pch = newPyr.pitch;
                    snapped.yaw = newPyr.yaw;
                    snapped.rol = newPyr.roll;
                    snapped.x = newOrigin.x;
                    snapped.y = newOrigin.y;
                    snapped.z = newOrigin.z;
                    if (newBoxOpt)
                    {
                        snapped.bbox = *newBoxOpt;
                    }
                    snapped.sector = glView_
                        ? glView_->findContainingSector(
                            TVector{snapped.x, snapped.y, snapped.z},
                            snapped.sector)
                        : sed::level::levelFindSectorForXYZ(
                            level, snapped.x, snapped.y, snapped.z);
                    if (requireInsideLevel && snapped.sector < 0)
                    {
                        continue;
                    }
                    snapped.layer = resolvedThingPlacementLayer(snapped.sector);

                    best = SnapCandidate{ std::move(snapped), score };
                }
            }
        }

        if (best)
        {
            return best->thing;
        }
    }

    for (const ThingPlacementSnapAnchorCacheEntry& anchor : thingPlacementSnapAnchorCache_)
    {
        if (anchor.thingIndex == excludedThingIndex
            || !anchor.finite || !anchor.box || anchor.box->IsZero())
        {
            continue;
        }

        // Bound the exact scorer conservatively. Its normal-axis allowance is
        // 2.5 radii before the weighted penalty begins, and the remaining
        // three-axis score can extend the accepted point to just under 4.4
        // radii from the box. Keep a little numerical margin here.
        const double broadReach = anchor.boxRadius + snapReach * 4.5;
        if (surfaceVec.DistanceSqr(anchor.origin) > broadReach * broadReach)
        {
            continue;
        }

        const std::array<double, 3>& anchorMin = anchor.boxMin;
        const std::array<double, 3>& anchorMax = anchor.boxMax;
        const std::array<double, 3>& anchorCenter = anchor.boxCenter;
        const TVector localPoint = anchor.inverseTransform.TransformPoint(surfaceVec);
        const TVector localNormal = anchor.inverseTransform.TransformVector(surfaceNormalVec);

        int normalAxis = 2;
        double normalAxisAbs = std::abs(sed::thing::vectorComponent(localNormal, normalAxis));
        for (int axis = 0; axis < 3; ++axis)
        {
            const double value = std::abs(sed::thing::vectorComponent(localNormal, axis));
            if (value > normalAxisAbs)
            {
                normalAxis = axis;
                normalAxisAbs = value;
            }
        }

        for (int snapAxis = 0; snapAxis < 3; ++snapAxis)
        {
            if (snapAxis == normalAxis)
            {
                continue;
            }

            const double pAxis = sed::thing::vectorComponent(localPoint, snapAxis);
            const double distToMin = std::abs(pAxis - anchorMin[static_cast<std::size_t>(snapAxis)]);
            const double distToMax = std::abs(pAxis - anchorMax[static_cast<std::size_t>(snapAxis)]);
            const int sign =
                (pAxis > anchorMax[static_cast<std::size_t>(snapAxis)]) ? 1
                : (pAxis < anchorMin[static_cast<std::size_t>(snapAxis)]) ? -1
                : (distToMax <= distToMin ? 1 : -1);
            const double faceDistance = sign > 0 ? distToMax : distToMin;

            double lateralPenaltySq = 0.0;
            for (int axis = 0; axis < 3; ++axis)
            {
                if (axis == snapAxis)
                {
                    continue;
                }
                const double allowedReach = axis == normalAxis ? snapReach * 2.5 : snapReach;
                const double outside = sed::thing::outsideDistance(
                    sed::thing::vectorComponent(localPoint, axis),
                    anchorMin[static_cast<std::size_t>(axis)] - allowedReach,
                    anchorMax[static_cast<std::size_t>(axis)] + allowedReach);
                lateralPenaltySq += outside * outside * (axis == normalAxis ? 0.5 : 2.0);
            }

            const double score = (faceDistance * faceDistance) + lateralPenaltySq;
            if (score > snapReach * snapReach)
            {
                continue;
            }
            if (best && score >= best->score)
            {
                continue;
            }

            const double anchorFace = sign > 0
                ? anchorMax[static_cast<std::size_t>(snapAxis)]
                : anchorMin[static_cast<std::size_t>(snapAxis)];
            TVector anchorContactLocal{};
            for (int axis = 0; axis < 3; ++axis)
            {
                sed::thing::setVectorComponent(anchorContactLocal, axis, anchorCenter[static_cast<std::size_t>(axis)]);
            }
            sed::thing::setVectorComponent(anchorContactLocal, snapAxis, anchorFace);
            const TVector anchorContactWorld = anchor.transform.TransformPoint(anchorContactLocal);

            TMatrix43 newRot = baseRot;
            TVector newPyr = basePyr;
            int attachAxis = 0;
            int attachSign = 1;
            const double proposedYaw = std::remainder(
                anchor.pyr.yaw + yawOffset - basePyr.yaw, 360.0);
            const bool rotateThisCandidate = rotateToSnap
                && std::abs(proposedYaw) <= snapRotationLimit + 1.0e-8;
            if (rotateThisCandidate)
            {
                newPyr = anchor.pyr;
                newPyr.yaw += yawOffset;
                newRot = TMatrix43::Create(TVector::zero, newPyr);
                attachAxis = snapAxis;
                attachSign = -sign;
            }
            else
            {
                TVector anchorOut = sed::thing::matrixAxisVector(anchor.transform, snapAxis).Scaled(static_cast<double>(sign));
                if (anchorOut.Normalize() == 0.0)
                {
                    continue;
                }

                double bestDot = -std::numeric_limits<double>::infinity();
                for (int axis = 0; axis < 3; ++axis)
                {
                    TVector newAxis = sed::thing::matrixAxisVector(newRot, axis);
                    if (newAxis.Normalize() == 0.0)
                    {
                        continue;
                    }
                    for (const int sideSign : { -1, 1 })
                    {
                        const double dot = newAxis.Scaled(static_cast<double>(sideSign)).Dot(-anchorOut);
                        if (dot > bestDot)
                        {
                            bestDot = dot;
                            attachAxis = axis;
                            attachSign = sideSign;
                        }
                    }
                }
            }

            TVector newContactLocal{};
            for (int axis = 0; axis < 3; ++axis)
            {
                sed::thing::setVectorComponent(newContactLocal, axis, newCenter[static_cast<std::size_t>(axis)]);
            }
            sed::thing::setVectorComponent(
                newContactLocal,
                attachAxis,
                attachSign > 0
                    ? newMax[static_cast<std::size_t>(attachAxis)]
                    : newMin[static_cast<std::size_t>(attachAxis)]);

            const TVector newOrigin = anchorContactWorld - newRot.TransformVector(newContactLocal);
            Thing snapped = baseThing;
            snapped.pch = newPyr.pitch;
            snapped.yaw = newPyr.yaw;
            snapped.rol = newPyr.roll;
            snapped.x = newOrigin.x;
            snapped.y = newOrigin.y;
            snapped.z = newOrigin.z;
            if (newBoxOpt)
            {
                snapped.bbox = *newBoxOpt;
            }
            snapped.sector = glView_
                ? glView_->findContainingSector(
                    TVector{snapped.x, snapped.y, snapped.z},
                    snapped.sector)
                : sed::level::levelFindSectorForXYZ(
                    level, snapped.x, snapped.y, snapped.z);
            if (requireInsideLevel && snapped.sector < 0)
            {
                continue;
            }
            snapped.layer = resolvedThingPlacementLayer(snapped.sector);

            best = SnapCandidate{ std::move(snapped), score };
        }
    }

    if (!best)
    {
        return std::nullopt;
    }
    return best->thing;
}

std::vector<QVector3D> MainWindow::buildThingPlacementPreviewLines(
    const QVector3D& surfacePosition,
    const std::string_view thingName,
    const QVector3D& surfaceNormal,
    const Qt::KeyboardModifiers modifiers,
    const bool templateCacheValidated) const
{
    std::vector<QVector3D> lines{};
    if (!templateCacheValidated)
    {
        ensureThingPlacementTemplateCache(thingName);
    }
    const double groundOffset = thingPlacementTemplateGroundOffsetCache_;
    const bool freeInsert = modifiers.testFlag(Qt::ShiftModifier);
    const bool controlTilt = modifiers.testFlag(Qt::ControlModifier);
    const ThingPlacementVisualKind visualKind = thingPlacementTemplateVisualKindCache_;
    const bool surfaceAlignedSprite = !freeInsert && controlTilt && visualKind == ThingPlacementVisualKind::sprite;
    const bool surfaceAlignedObject = !freeInsert && controlTilt && visualKind != ThingPlacementVisualKind::sprite;
    std::optional<TVector> alignedPyr{};
    if (surfaceAlignedSprite)
    {
        alignedPyr = thingPlacementSpriteSurfaceAlignedPyr(surfaceNormal);
    }
    else if (surfaceAlignedObject)
    {
        alignedPyr = thingPlacementSurfaceAlignedPyr(surfaceNormal);
    }
    QVector3D up = (surfaceAlignedSprite || surfaceAlignedObject)
        ? QVector3D(surfaceNormal).normalized()
        : QVector3D(0.0f, 0.0f, 1.0f);
    if (up.lengthSquared() <= 1e-8f)
    {
        up = QVector3D(0.0f, 0.0f, 1.0f);
    }
    const QVector3D origin = surfacePosition + up * static_cast<float>(groundOffset);

    const TVector originVec{
        static_cast<double>(origin.x()),
        static_cast<double>(origin.y()),
        static_cast<double>(origin.z())
    };
    TVector previewPyr = alignedPyr.value_or(TVector{});
    previewPyr.yaw += thingPlacementYawOffset_;
    const TMatrix43 localToWorld = TMatrix43::Create(originVec, previewPyr);

    auto transformLocal = [&](const float x, const float y, const float z) -> QVector3D
    {
        const TVector p = localToWorld.TransformPoint(TVector{
            static_cast<double>(x),
            static_cast<double>(y),
            static_cast<double>(z)
        });
        return QVector3D(static_cast<float>(p.x), static_cast<float>(p.y), static_cast<float>(p.z));
    };

    QVector3D axisA = QVector3D(
        static_cast<float>(localToWorld.rvec.x),
        static_cast<float>(localToWorld.rvec.y),
        static_cast<float>(localToWorld.rvec.z));
    QVector3D axisB = QVector3D(
        static_cast<float>(localToWorld.lvec.x),
        static_cast<float>(localToWorld.lvec.y),
        static_cast<float>(localToWorld.lvec.z));
    QVector3D axisC = QVector3D(
        static_cast<float>(localToWorld.uvec.x),
        static_cast<float>(localToWorld.uvec.y),
        static_cast<float>(localToWorld.uvec.z));
    if (axisA.lengthSquared() <= 1e-8f)
    {
        axisA = QVector3D(1.0f, 0.0f, 0.0f);
    }
    if (axisB.lengthSquared() <= 1e-8f)
    {
        axisB = QVector3D(0.0f, 1.0f, 0.0f);
    }
    if (axisC.lengthSquared() <= 1e-8f)
    {
        axisC = QVector3D(0.0f, 0.0f, 1.0f);
    }
    axisA.normalize();
    axisB.normalize();
    axisC.normalize();

    float unitsPerPixel = glView_ ? glView_->worldUnitsPerPixelAtWorld(origin) : 0.01f;
    if (!std::isfinite(unitsPerPixel) || unitsPerPixel <= 0.0f)
    {
        unitsPerPixel = 0.01f;
    }
    const float marker = std::clamp(unitsPerPixel * 28.0f, 0.025f, 0.65f);

    auto addLine = [&](const QVector3D& a, const QVector3D& b)
    {
        lines.push_back(a);
        lines.push_back(b);
    };

    if (thingPlacementSnapTemplateBoxCache_)
    {
        const ThingBox& box = *thingPlacementSnapTemplateBoxCache_;
        const float minX = static_cast<float>(std::min(box.x1, box.x2));
        const float minY = static_cast<float>(std::min(box.y1, box.y2));
        const float minZ = static_cast<float>(std::min(box.z1, box.z2));
        const float maxX = static_cast<float>(std::max(box.x1, box.x2));
        const float maxY = static_cast<float>(std::max(box.y1, box.y2));
        const float maxZ = static_cast<float>(std::max(box.z1, box.z2));

        if (std::isfinite(minX) && std::isfinite(minY) && std::isfinite(minZ)
            && std::isfinite(maxX) && std::isfinite(maxY) && std::isfinite(maxZ)
            && (maxX - minX) > 1e-6f
            && (maxY - minY) > 1e-6f
            && (maxZ - minZ) > 1e-6f)
        {
            const std::array<QVector3D, 8> c = {
                transformLocal(minX, minY, minZ),
                transformLocal(maxX, minY, minZ),
                transformLocal(maxX, maxY, minZ),
                transformLocal(minX, maxY, minZ),
                transformLocal(minX, minY, maxZ),
                transformLocal(maxX, minY, maxZ),
                transformLocal(maxX, maxY, maxZ),
                transformLocal(minX, maxY, maxZ),
            };

            addLine(c[0], c[1]);
            addLine(c[1], c[2]);
            addLine(c[2], c[3]);
            addLine(c[3], c[0]);
            addLine(c[4], c[5]);
            addLine(c[5], c[6]);
            addLine(c[6], c[7]);
            addLine(c[7], c[4]);
            addLine(c[0], c[4]);
            addLine(c[1], c[5]);
            addLine(c[2], c[6]);
            addLine(c[3], c[7]);
        }
    }

    if (lines.empty())
    {
        addLine(origin - axisA * marker - axisB * marker, origin + axisA * marker + axisB * marker);
        addLine(origin - axisA * marker + axisB * marker, origin + axisA * marker - axisB * marker);
        addLine(origin - axisC * marker, origin + axisC * marker);
    }

    const float cross = marker * 0.78f;
    addLine(surfacePosition - axisA * cross, surfacePosition + axisA * cross);
    addLine(surfacePosition - axisB * cross, surfacePosition + axisB * cross);
    constexpr int kRingSegments = 28;
    constexpr double kPi = 3.141592653589793238462643383279502884;
    for (int i = 0; i < kRingSegments; ++i)
    {
        const double a0 = (2.0 * kPi * static_cast<double>(i)) / static_cast<double>(kRingSegments);
        const double a1 = (2.0 * kPi * static_cast<double>(i + 1)) / static_cast<double>(kRingSegments);
        addLine(
            surfacePosition
                + axisA * static_cast<float>(std::cos(a0) * static_cast<double>(marker))
                + axisB * static_cast<float>(std::sin(a0) * static_cast<double>(marker)),
            surfacePosition
                + axisA * static_cast<float>(std::cos(a1) * static_cast<double>(marker))
                + axisB * static_cast<float>(std::sin(a1) * static_cast<double>(marker)));
    }
    if (alignedPyr)
    {
        addLine(surfacePosition, surfacePosition + axisC * (marker * 1.65f));
    }
    if (std::abs(static_cast<float>(groundOffset)) > 1e-6f)
    {
        addLine(surfacePosition, origin);
    }

    return lines;
}

std::optional<MainWindow::ThingPlacementTarget> MainWindow::thingPlacementTargetForViewport(
    const QPoint& widgetPos,
    const Qt::KeyboardModifiers modifiers) const
{
    ThingPlacementTarget target{};
    if (!glView_)
    {
        return std::nullopt;
    }

    const Qt::KeyboardModifiers effectiveModifiers = thingPlacementEffectiveModifiers(modifiers);
    const bool snapToSurface = !effectiveModifiers.testFlag(Qt::ShiftModifier);
    bool haveSurface = false;
    SurfaceRef surfaceRef{};
    QVector3D normal{};
    const std::optional<QVector3D> point =
        glView_->placementPointForViewport(widgetPos, &haveSurface, &surfaceRef, &normal, snapToSurface, !snapToSurface, &target.documentPosition);
    if (!point)
    {
        return std::nullopt;
    }

    target.position = *point;
    target.haveSurfaceHit = haveSurface;
    target.surfaceNormal = normal.lengthSquared() > 1e-8f ? normal : QVector3D(0.0f, 0.0f, 1.0f);
    target.sectorFallback = surfaceRef.sector;

    if (snapToSurface)
    {
        target.havePlacementTarget = haveSurface;
        return target;
    }

    // placementPointForViewport already resolves the containing sector while
    // walking the indexed ray. Reusing it avoids a second all-sector scan on
    // every free-placement mouse move.
    std::int32_t sector = surfaceRef.sector;
    if (sector < 0
        || static_cast<std::size_t>(sector) >= document_.level().sectors.size())
    {
        sector = glView_->findContainingSector(target.documentPosition, sector);
    }
    target.havePlacementTarget = haveSurface || sector >= 0;
    if (sector >= 0)
    {
        target.sectorFallback = sector;
    }
    return target;
}

std::pair<double, double> MainWindow::thingPlacementBrushSampleUv(
    const int index,
    const int count,
    const QVector3D& center) const
{
    const double t = count <= 1 ? 0.0 : (static_cast<double>(index) + 0.5) / static_cast<double>(count);
    const double angle = static_cast<double>(index) * 2.39996322972865332;
    double baseU = std::cos(angle) * std::sqrt(t);
    double baseV = std::sin(angle) * std::sqrt(t);
    if (thingPlacementBrushShape_ != ThingPlacementBrushShape::circle)
    {
        baseU = std::clamp(baseU * 1.25, -1.0, 1.0);
        baseV = std::clamp(baseV * 1.25, -1.0, 1.0);
    }

    const double scatter = std::clamp(thingPlacementBrushScatter_, 0.0, 1.0);
    if (scatter <= 1e-6)
    {
        return { baseU, baseV };
    }

    std::uint32_t seed = 2166136261u;
    auto mix = [&seed](const std::uint32_t value)
    {
        seed ^= value;
        seed *= 16777619u;
    };
    auto mixCoord = [&mix](const float value)
    {
        mix(ui::viewport::thingBrushCoordinateSeed(value));
    };
    mixCoord(center.x());
    mixCoord(center.y());
    mixCoord(center.z());
    mix(static_cast<std::uint32_t>(std::max(0, count)));
    mix(static_cast<std::uint32_t>(std::max(0, index)));
    mix(static_cast<std::uint32_t>(static_cast<int>(thingPlacementBrushShape_)));

    std::mt19937 rng(seed);
    std::uniform_real_distribution<double> unitDist(0.0, 1.0);
    double randomU = 0.0;
    double randomV = 0.0;
    if (thingPlacementBrushShape_ == ThingPlacementBrushShape::circle)
    {
        constexpr double kPi = 3.141592653589793238462643383279502884;
        const double randomAngle = unitDist(rng) * 2.0 * kPi;
        const double randomRadius = std::sqrt(unitDist(rng));
        randomU = std::cos(randomAngle) * randomRadius;
        randomV = std::sin(randomAngle) * randomRadius;
    }
    else
    {
        randomU = (unitDist(rng) * 2.0) - 1.0;
        randomV = (unitDist(rng) * 2.0) - 1.0;
    }

    const double keep = 1.0 - scatter;
    return {
        (baseU * keep) + (randomU * scatter),
        (baseV * keep) + (randomV * scatter)
    };
}

std::optional<MainWindow::ThingPlacementTarget> MainWindow::thingPlacementBrushSampleTarget(
    const TVector& samplePoint,
    QVector3D brushNormal,
    const Qt::KeyboardModifiers modifiers,
    const std::int32_t preferredSector) const
{
    ThingPlacementTarget target{};
    if (brushNormal.lengthSquared() <= 1e-8f)
    {
        brushNormal = QVector3D(0.0f, 0.0f, 1.0f);
    }
    brushNormal.normalize();

    const bool freeInsert = modifiers.testFlag(Qt::ShiftModifier);
    if (freeInsert || thingPlacementBrushAllowAir_)
    {
        const Level& level = document_.level();
        std::int32_t sector = -1;
        if (preferredSector >= 0
            && static_cast<std::size_t>(preferredSector) < level.sectors.size()
            && sed::level::sectorContainsPoint(
                level.sectors[static_cast<std::size_t>(preferredSector)],
                static_cast<double>(samplePoint.x),
                static_cast<double>(samplePoint.y),
                static_cast<double>(samplePoint.z)))
        {
            sector = preferredSector;
        }
        else
        {
            sector = glView_
                ? glView_->findContainingSector(samplePoint, preferredSector)
                : sed::level::levelFindSectorForXYZ(
                    level,
                    static_cast<double>(samplePoint.x),
                    static_cast<double>(samplePoint.y),
                    static_cast<double>(samplePoint.z));
        }
        if (sector < 0)
        {
            return std::nullopt;
        }

        target.documentPosition = samplePoint;
        target.position = QVector3D(static_cast<float>(samplePoint.x),
            static_cast<float>(samplePoint.y), static_cast<float>(samplePoint.z));
        target.surfaceNormal = brushNormal;
        target.haveSurfaceHit = false;
        target.havePlacementTarget = true;
        target.sectorFallback = sector;
        return target;
    }

    const Level& level = document_.level();
    auto normalForHit = [&level](const sed::level::SurfacePickHit& hit) -> std::optional<TVector>
    {
        if (hit.surface.sector < 0
            || static_cast<std::size_t>(hit.surface.sector) >= level.sectors.size())
        {
            return std::nullopt;
        }

        const Sector& sector = level.sectors[static_cast<std::size_t>(hit.surface.sector)];
        if (hit.surface.surface < 0
            || static_cast<std::size_t>(hit.surface.surface) >= sector.surfaces.size())
        {
            return std::nullopt;
        }

        const Surface& surface = sector.surfaces[static_cast<std::size_t>(hit.surface.surface)];
        TVector normal = surface.normal;
        if (normal.IsZero())
        {
            normal = sed::level::surfaceCalcNormalNewell(sector, surface);
        }
        if (normal.Normalize() == 0.0)
        {
            return std::nullopt;
        }
        return normal;
    };

    auto isPassThroughPlacementSurface = [&level](const sed::level::SurfacePickHit& hit) -> bool
    {
        if (hit.surface.sector < 0
            || static_cast<std::size_t>(hit.surface.sector) >= level.sectors.size())
        {
            return false;
        }

        const Sector& sector = level.sectors[static_cast<std::size_t>(hit.surface.sector)];
        if (hit.surface.surface < 0
            || static_cast<std::size_t>(hit.surface.surface) >= sector.surfaces.size())
        {
            return false;
        }

        const Surface& surface = sector.surfaces[static_cast<std::size_t>(hit.surface.surface)];
        if (!surface.adjoin)
        {
            return false;
        }

        const bool noCollision = (surface.surfaceFlags & SF_Collision) == 0;
        const bool notSolidGeo = surface.geo < static_cast<std::int32_t>(TGeoMode::Solid);
        const std::string material = sed::text::lowerAscii(sed::text::trimView(surface.material));
        const bool noMaterial = material.empty() || material == "-" || material == "none";
        return noCollision && (notSolidGeo || noMaterial);
    };

    const float lift = std::clamp(static_cast<float>(thingPlacementBrushSize_) * 0.20f + 0.03f, 0.04f, 0.75f);
    const sed::TVector origin{
        static_cast<double>(samplePoint.x + brushNormal.x() * lift),
        static_cast<double>(samplePoint.y + brushNormal.y() * lift),
        static_cast<double>(samplePoint.z + brushNormal.z() * lift)
    };
    const sed::TVector direction{
        static_cast<double>(-brushNormal.x()),
        static_cast<double>(-brushNormal.y()),
        static_cast<double>(-brushNormal.z())
    };
    const double maxDistance = static_cast<double>(lift * 2.0f + 0.08f);

    const std::vector<sed::level::SurfacePickHit> hits = glView_
        ? glView_->placementSurfaceHits(origin, direction, maxDistance)
        : sed::level::levelPickSurfaces(level, origin, direction, maxDistance);
    for (const sed::level::SurfacePickHit& hit : hits)
    {
        if (isPassThroughPlacementSurface(hit))
        {
            continue;
        }

        const std::optional<TVector> normal = normalForHit(hit);
        if (!normal || normal->Dot(direction) >= -0.025)
        {
            continue;
        }

        target.documentPosition = hit.position;
        target.position = QVector3D(
            static_cast<float>(hit.position.x),
            static_cast<float>(hit.position.y),
            static_cast<float>(hit.position.z));
        target.surfaceNormal = QVector3D(
            static_cast<float>(normal->x),
            static_cast<float>(normal->y),
            static_cast<float>(normal->z));
        target.haveSurfaceHit = true;
        target.havePlacementTarget = true;
        target.sectorFallback = hit.surface.sector;
        return target;
    }

    return std::nullopt;
}

bool MainWindow::stampThingPlacementBrushAt(
    const QPoint& widgetPos,
    const Qt::KeyboardModifiers modifiers,
    const bool force)
{
    if (placementMode_ != PlacementMode::thing || thingPlacementToolMode_ != ThingPlacementToolMode::brush)
    {
        return false;
    }

    const std::optional<ThingPlacementTarget> target = thingPlacementTargetForViewport(widgetPos, modifiers);
    if (!target || !target->havePlacementTarget)
    {
        return false;
    }

    const double minSpacing = std::max(0.015, thingPlacementBrushSize_ * thingPlacementBrushSpacing_);
    if (!force && thingPlacementBrushStrokeHaveLast_)
    {
        const TVector delta = target->documentPosition - thingPlacementBrushStrokeLastPoint_;
        if (std::hypot(delta.x, delta.y, delta.z) < minSpacing)
        {
            return false;
        }
    }

    if (!commitThingPlacementAt(
            target->position,
            target->haveSurfaceHit || target->havePlacementTarget,
            target->surfaceNormal,
            modifiers,
            target->sectorFallback,
            &widgetPos, &target->documentPosition))
    {
        return false;
    }

    thingPlacementBrushStrokeHaveLast_ = true;
    thingPlacementBrushStrokeLastPoint_ = target->documentPosition;
    thingPlacementBrushStrokePlaced_ = true;
    return true;
}

bool MainWindow::finalizeThingPlacementBrushStroke()
{
    const bool placed = thingPlacementBrushStrokePlaced_;
    thingPlacementBrushStrokeActive_ = false;
    thingPlacementBrushStrokeHaveLast_ = false;
    thingPlacementBrushStrokePlaced_ = false;
    thingPlacementBrushStrokeUndoPushed_ = false;

    if (!placed)
    {
        return false;
    }

    // The live stroke already updated the document and viewport. Publish the
    // editor-side views once, at the gesture boundary, rather than rebuilding
    // the outliner, properties, and full history tree for every spacing stamp.
    const std::size_t firstUnpublishedThing = std::min(
        thingPlacementBrushStrokeFirstThing_,
        document_.level().things.size());
    appendThingsToOutliner(firstUnpublishedThing);
    rebuildProperties();
    updateEditActions();
    std::vector<std::int32_t> appendedThings{};
    appendedThings.reserve(document_.level().things.size() - firstUnpublishedThing);
    for (std::size_t index = firstUnpublishedThing;
         index < document_.level().things.size();
         ++index)
    {
        appendedThings.push_back(static_cast<std::int32_t>(index));
    }
    refreshThingChecks(appendedThings, true);
    return true;
}

void MainWindow::updateThingPlacementPreview(
    const QPoint& widgetPos,
    const Qt::KeyboardModifiers modifiers)
{
    if (!glView_ || placementMode_ != PlacementMode::thing)
    {
        return;
    }
    if (glView_->flyNavigationActive() || thingPlacementPointerInsidePanel())
    {
        glView_->clearPlacementPreviewThing();
        glView_->clearPlacementPreviewLines();
        return;
    }
    const std::string thingName = currentThingTemplateName();
    if (thingName.empty())
    {
        glView_->clearPlacementPreviewThing();
        glView_->clearPlacementPreviewLines();
        return;
    }
    ensureThingPlacementTemplateCache(thingName);

    const Qt::KeyboardModifiers effectiveModifiers = thingPlacementEffectiveModifiers(modifiers);
    const std::optional<ThingPlacementTarget> target = thingPlacementTargetForViewport(widgetPos, modifiers);
    if (!target)
    {
        glView_->clearPlacementPreviewThing();
        glView_->clearPlacementPreviewLines();
        return;
    }
    const QVector3D& point = target->position;
    const QVector3D& surfaceNormal = target->surfaceNormal;
    const bool havePlacementTarget = target->havePlacementTarget;
    const bool snapToThingMode =
        thingPlacementToolMode_ == ThingPlacementToolMode::single
        && thingPlacementSnapToThing_;

    auto buildBrushLines = [this](const QVector3D& center, QVector3D normal) -> std::vector<QVector3D>
    {
        std::vector<QVector3D> lines{};
        if (thingPlacementToolMode_ != ThingPlacementToolMode::brush)
        {
            return lines;
        }

        if (normal.lengthSquared() <= 1e-8f)
        {
            normal = QVector3D(0.0f, 0.0f, 1.0f);
        }
        normal.normalize();
        QVector3D axisA = QVector3D::crossProduct(normal, QVector3D(0.0f, 0.0f, 1.0f));
        if (axisA.lengthSquared() <= 1e-8f)
        {
            axisA = QVector3D::crossProduct(normal, QVector3D(1.0f, 0.0f, 0.0f));
        }
        axisA.normalize();
        QVector3D axisB = QVector3D::crossProduct(normal, axisA);
        axisB.normalize();

        const float radius = std::max(0.01f, static_cast<float>(thingPlacementBrushSize_));
        const float aspect = thingPlacementBrushShape_ == ThingPlacementBrushShape::rectangle ? 1.65f : 1.0f;
        auto addLine = [&lines](const QVector3D& a, const QVector3D& b)
        {
            lines.push_back(a);
            lines.push_back(b);
        };

        constexpr double kPi = 3.141592653589793238462643383279502884;
        if (thingPlacementBrushShape_ == ThingPlacementBrushShape::circle)
        {
            constexpr int kSegments = 48;
            for (int i = 0; i < kSegments; ++i)
            {
                const double a0 = (2.0 * kPi * static_cast<double>(i)) / static_cast<double>(kSegments);
                const double a1 = (2.0 * kPi * static_cast<double>(i + 1)) / static_cast<double>(kSegments);
                addLine(
                    center + axisA * static_cast<float>(std::cos(a0) * radius) + axisB * static_cast<float>(std::sin(a0) * radius),
                    center + axisA * static_cast<float>(std::cos(a1) * radius) + axisB * static_cast<float>(std::sin(a1) * radius));
            }
        }
        else
        {
            const QVector3D a = axisA * (radius * aspect);
            const QVector3D b = axisB * radius;
            addLine(center - a - b, center + a - b);
            addLine(center + a - b, center + a + b);
            addLine(center + a + b, center - a + b);
            addLine(center - a + b, center - a - b);
        }

        const int count = clampedThingPlacementBrushSampleCount(thingPlacementBrushCount_);
        for (int i = 0; i < count; ++i)
        {
            const auto [u, v] = thingPlacementBrushSampleUv(i, count, center);
            QVector3D p = center
                + axisA * static_cast<float>(u * static_cast<double>(radius) * static_cast<double>(aspect))
                + axisB * static_cast<float>(v * static_cast<double>(radius));
            const float mark = radius * 0.035f;
            addLine(p - axisA * mark, p + axisA * mark);
            addLine(p - axisB * mark, p + axisB * mark);
        }
        return lines;
    };

    if (thingPlacementToolMode_ == ThingPlacementToolMode::brush)
    {
        std::vector<QVector3D> brushLines = buildBrushLines(point, surfaceNormal);
        std::vector<Thing> brushPreviewThings{};
        if (havePlacementTarget)
        {
            QVector3D normal = surfaceNormal;
            if (normal.lengthSquared() <= 1e-8f)
            {
                normal = QVector3D(0.0f, 0.0f, 1.0f);
            }
            normal.normalize();
            QVector3D axisA = QVector3D::crossProduct(normal, QVector3D(0.0f, 0.0f, 1.0f));
            if (axisA.lengthSquared() <= 1e-8f)
            {
                axisA = QVector3D::crossProduct(normal, QVector3D(1.0f, 0.0f, 0.0f));
            }
            axisA.normalize();
            QVector3D axisB = QVector3D::crossProduct(normal, axisA);
            axisB.normalize();

            const int count = clampedThingPlacementBrushSampleCount(thingPlacementBrushCount_);
            const float radius = std::max(0.01f, static_cast<float>(thingPlacementBrushSize_));
            const float aspect = thingPlacementBrushShape_ == ThingPlacementBrushShape::rectangle ? 1.65f : 1.0f;
            brushPreviewThings.reserve(static_cast<std::size_t>(count));

            auto applyPreviewRotation = [&](Thing& thing, const int index)
            {
                auto applyRotation = [&](const double value)
                {
                    if (thingPlacementBrushRotatePitch_)
                    {
                        thing.pch += value;
                    }
                    if (thingPlacementBrushRotateYaw_)
                    {
                        thing.yaw += value;
                    }
                    if (thingPlacementBrushRotateRoll_)
                    {
                        thing.rol += value;
                    }
                };
                if (thingPlacementBrushRandomRotation_)
                {
                    applyRotation(std::fmod(static_cast<double>(index) * 137.50776405, 360.0));
                }
                else
                {
                    applyRotation(thingPlacementBrushFixedAngle_);
                }
            };

            for (int i = 0; i < count; ++i)
            {
                const auto [u, v] = thingPlacementBrushSampleUv(i, count, point);

                const TVector samplePoint = target->documentPosition
                    + TVector{axisA.x(), axisA.y(), axisA.z()} * (u * radius * aspect)
                    + TVector{axisB.x(), axisB.y(), axisB.z()} * (v * radius);
                const std::optional<ThingPlacementTarget> sampleTarget =
                    thingPlacementBrushSampleTarget(
                        samplePoint,
                        normal,
                        effectiveModifiers,
                        target->sectorFallback);
                if (!sampleTarget || !sampleTarget->havePlacementTarget)
                {
                    continue;
                }

                Thing previewThing = buildThingPlacementPreviewThing(
                    sampleTarget->position,
                    sampleTarget->surfaceNormal,
                    effectiveModifiers,
                    sampleTarget->sectorFallback, &sampleTarget->documentPosition, true);
                if (previewThing.name.empty() || previewThing.sector < 0)
                {
                    continue;
                }

                applyPreviewRotation(previewThing, i);
                brushPreviewThings.push_back(std::move(previewThing));
            }
        }

        if (brushLines.empty())
        {
            glView_->clearPlacementPreviewLines();
        }
        else
        {
            glView_->setPlacementPreviewLines(
                std::move(brushLines),
                false,
                !havePlacementTarget || brushPreviewThings.empty());
        }
        if (brushPreviewThings.empty())
        {
            glView_->clearPlacementPreviewThing();
        }
        else
        {
            glView_->setPlacementPreviewThings(std::move(brushPreviewThings), !havePlacementTarget);
        }
        return;
    }

    if (havePlacementTarget)
    {
        glView_->clearPlacementPreviewLines();
    }
    else
    {
        glView_->setPlacementPreviewLines(
            buildThingPlacementPreviewLines(point, thingName, surfaceNormal, effectiveModifiers, true),
            false,
            true);
    }
    Thing previewThing = buildThingPlacementPreviewThing(
        point,
        surfaceNormal,
        effectiveModifiers,
        target->sectorFallback, &target->documentPosition, true);
    if (snapToThingMode)
    {
        if (std::optional<Thing> snappedThing = buildThingPlacementSnapThing(
                point,
                surfaceNormal,
                effectiveModifiers,
                target->sectorFallback, &target->documentPosition, true))
        {
            previewThing = std::move(*snappedThing);
        }
    }
    glView_->setPlacementPreviewThing(std::move(previewThing), !havePlacementTarget);
}

bool MainWindow::commitThingPlacementAt(
    const QVector3D& surfacePosition,
    const bool haveSurfaceHit,
    const QVector3D& surfaceNormal,
    const Qt::KeyboardModifiers modifiers,
    const std::int32_t fallbackSector,
    const QPoint* widgetPos,
    const TVector* documentSurfacePosition)
{
    const auto reportIdExhaustion = [this]()
    {
        constexpr const char* message = "Thing placement failed: entity ID space exhausted";
        if ( glView_ )
        {
            glView_->showToast(
                message,
                3000,
                GlView::ToastPlacement::BottomCenter,
                GlView::ToastGroup::Error);
        }
        if ( statusBar() )
        {
            statusBar()->showMessage(message, 4000);
        }
    };
    const TVector surfacePoint = documentSurfacePosition
        ? *documentSurfacePosition : TVector{surfacePosition.x(), surfacePosition.y(), surfacePosition.z()};
    const Qt::KeyboardModifiers effectiveModifiers = thingPlacementEffectiveModifiers(modifiers);
    const bool freeInsert = effectiveModifiers.testFlag(Qt::ShiftModifier);
    const bool havePlacementTarget = haveSurfaceHit
        || (freeInsert
            && ((glView_
                    ? glView_->findContainingSector(surfacePoint, fallbackSector)
                    : sed::level::levelFindSectorForXYZ(
                        document_.level(),
                        static_cast<double>(surfacePosition.x()),
                        static_cast<double>(surfacePosition.y()),
                        static_cast<double>(surfacePosition.z()))) >= 0));
    if (!havePlacementTarget)
    {
        if (glView_)
        {
            glView_->showToast(
                "Can't place thing outside level geometry",
                2600,
                GlView::ToastPlacement::BottomCenter,
                GlView::ToastGroup::Error);
        }
        return false;
    }

    Thing placedThing = buildThingPlacementPreviewThing(surfacePosition, surfaceNormal, effectiveModifiers, fallbackSector, &surfacePoint);
    if (thingPlacementToolMode_ == ThingPlacementToolMode::single && thingPlacementSnapToThing_)
    {
        if (std::optional<Thing> snappedThing = buildThingPlacementSnapThing(
                surfacePosition,
                surfaceNormal,
                effectiveModifiers,
                fallbackSector, &surfacePoint))
        {
            placedThing = std::move(*snappedThing);
        }
    }
    if (placedThing.name.empty())
    {
        return false;
    }
    if ( !document_.level().canAllocateIds(1) )
    {
        reportIdExhaustion();
        return false;
    }

    auto finishSuccessfulThingPlacement = [this, widgetPos](
        const std::int32_t firstThing,
        const std::int32_t activeThing) -> bool
    {
        // Finalize the append-only history record after the new Things are
        // present.  This records its bounded tail count for history target
        // navigation without copying the complete Thing vector.
        (void)document_.touchLatestUndoRecord();
        if (glView_)
        {
            glView_->notifyThingsAppended(
                firstThing >= 0
                    ? static_cast<std::size_t>(firstThing)
                    : document_.level().things.size());
            glView_->setSelectionState(-1, -1, activeThing, -1, document_.selection());
        }

        if (thingPlacementBrushStrokeActive_)
        {
            if (glView_ && !currentThingTemplateName().empty())
            {
                const QPoint previewPos = widgetPos
                    ? *widgetPos
                    : glView_->mapFromGlobal(QCursor::pos());
                updateThingPlacementPreview(
                    previewPos,
                    QApplication::keyboardModifiers());
            }
            return true;
        }

        appendThingsToOutliner(
            firstThing >= 0
                ? static_cast<std::size_t>(firstThing)
                : document_.level().things.size());
        rebuildProperties();
        updateEditActions();
        std::vector<std::int32_t> appendedThings{};
        if (firstThing >= 0)
        {
            appendedThings.reserve(document_.level().things.size()
                - static_cast<std::size_t>(firstThing));
            for (std::size_t index = static_cast<std::size_t>(firstThing);
                 index < document_.level().things.size();
                 ++index)
            {
                appendedThings.push_back(static_cast<std::int32_t>(index));
            }
        }
        refreshThingChecks(appendedThings, true);
        if (thingPlacementKeepActive_ && !currentThingTemplateName().empty())
        {
            syncThingPlacementPanel();
            if (glView_)
            {
                updateThingPlacementPreview(glView_->mapFromGlobal(QCursor::pos()), QApplication::keyboardModifiers());
            }
        }
        else
        {
            deselectThingPlacementTemplate();
        }
        return true;
    };

    if (thingPlacementToolMode_ == ThingPlacementToolMode::brush)
    {
        QVector3D normal = surfaceNormal;
        if (normal.lengthSquared() <= 1e-8f)
        {
            normal = QVector3D(0.0f, 0.0f, 1.0f);
        }
        normal.normalize();
        QVector3D axisA = QVector3D::crossProduct(normal, QVector3D(0.0f, 0.0f, 1.0f));
        if (axisA.lengthSquared() <= 1e-8f)
        {
            axisA = QVector3D::crossProduct(normal, QVector3D(1.0f, 0.0f, 0.0f));
        }
        axisA.normalize();
        QVector3D axisB = QVector3D::crossProduct(normal, axisA);
        axisB.normalize();

        const int count = clampedThingPlacementBrushSampleCount(thingPlacementBrushCount_);
        const float radius = std::max(0.01f, static_cast<float>(thingPlacementBrushSize_));
        const float aspect = thingPlacementBrushShape_ == ThingPlacementBrushShape::rectangle ? 1.65f : 1.0f;
        std::vector<Thing> brushThings{};
        brushThings.reserve(static_cast<std::size_t>(count));
        std::mt19937 rng(std::random_device{}());
        std::uniform_real_distribution<double> angleDist(0.0, 360.0);

        for (int i = 0; i < count; ++i)
        {
            const auto [u, v] = thingPlacementBrushSampleUv(i, count, surfacePosition);
            const TVector p = surfacePoint
                + TVector{axisA.x(), axisA.y(), axisA.z()} * (u * radius * aspect)
                + TVector{axisB.x(), axisB.y(), axisB.z()} * (v * radius);
            const std::optional<ThingPlacementTarget> sampleTarget =
                thingPlacementBrushSampleTarget(
                    p,
                    normal,
                    effectiveModifiers,
                    fallbackSector);
            if (!sampleTarget || !sampleTarget->havePlacementTarget)
            {
                continue;
            }

            Thing brushThing = buildThingPlacementPreviewThing(
                sampleTarget->position,
                sampleTarget->surfaceNormal,
                effectiveModifiers,
                sampleTarget->sectorFallback, &sampleTarget->documentPosition);
            if (brushThing.name.empty())
            {
                continue;
            }
            if (brushThing.sector < 0)
            {
                continue;
            }

            auto applyRotation = [&](const double value)
            {
                if (thingPlacementBrushRotatePitch_)
                {
                    brushThing.pch += value;
                }
                if (thingPlacementBrushRotateYaw_)
                {
                    brushThing.yaw += value;
                }
                if (thingPlacementBrushRotateRoll_)
                {
                    brushThing.rol += value;
                }
            };
            if (thingPlacementBrushRandomRotation_)
            {
                applyRotation(angleDist(rng));
            }
            else
            {
                applyRotation(thingPlacementBrushFixedAngle_);
            }
            brushThings.push_back(std::move(brushThing));
        }

        if (brushThings.empty())
        {
            if (glView_)
            {
                glView_->showToast(
                    "Brush did not find any valid thing positions",
                    2400,
                    GlView::ToastPlacement::BottomCenter,
                    GlView::ToastGroup::Error);
            }
            return false;
        }
        if ( !document_.level().canAllocateIds(brushThings.size()) )
        {
            reportIdExhaustion();
            return false;
        }

        std::int32_t firstThing = -1;
        std::int32_t activeThing = -1;
        const std::string undoLabel = ui::undo::action("Thing", brushThings.size(), "Added by brush");
        auto appendBrushThings = [this, &firstThing, &activeThing](
            editor::DocumentState& state,
            const std::vector<Thing>& things) -> bool
        {
            editor::Selection selection{};
            for (const Thing& source : things)
            {
                if (firstThing < 0)
                {
                    firstThing = static_cast<std::int32_t>(state.level.things.size());
                }
                Thing& thing = state.level.newThing();
                const std::int32_t allocatedId = thing.id;
                const std::int32_t destinationNum = thing.num;
                thing = source;
                thing.id = allocatedId;
                thing.num = destinationNum;
                activeThing = static_cast<std::int32_t>(state.level.things.size() - 1);
                selection.things.insert(activeThing);
            }
            if (selection.things.empty())
            {
                return false;
            }
            state.selection = std::move(selection);
            return true;
        };

        bool applied = false;
        if (thingPlacementBrushStrokeActive_)
        {
            const bool firstStrokeStamp = !thingPlacementBrushStrokeUndoPushed_;
            if (firstStrokeStamp)
            {
                document_.pushThingTailUndo(
                    undoLabel,
                    editor::DocumentDirty::Structure | editor::DocumentDirty::Things);
                thingPlacementBrushStrokeUndoPushed_ = true;
            }

            applied = appendBrushThings(document_.current(), brushThings);
            if (applied && !firstStrokeStamp)
            {
                // Keep the stroke as one Undo item while rotating its identity
                // so save-state and asynchronous recovery observe every later
                // append without copying the complete Things vector again.
                (void)document_.touchLatestUndoRecord();
            }
        }
        else
        {
            document_.pushThingTailUndo(
                undoLabel,
                editor::DocumentDirty::Structure | editor::DocumentDirty::Things);
            applied = appendBrushThings(document_.current(), brushThings);
        }

        if (!applied)
        {
            return false;
        }
        return finishSuccessfulThingPlacement(firstThing, activeThing);
    }

    std::int32_t selectedThing = -1;
    const std::int32_t firstThing =
        static_cast<std::int32_t>(document_.level().things.size());
    document_.pushThingTailUndo(
        "Thing: Added",
        editor::DocumentDirty::Structure | editor::DocumentDirty::Things);
    editor::DocumentState& state = document_.current();
    Thing& thing = state.level.newThing();
    const std::int32_t allocatedId = thing.id;
    const std::int32_t destinationNum = thing.num;
    thing = std::move(placedThing);
    thing.id = allocatedId;
    thing.num = destinationNum;

    selectedThing = static_cast<std::int32_t>(state.level.things.size() - 1);
    editor::Selection selection{};
    selection.things.insert(selectedThing);
    state.selection = std::move(selection);

    return finishSuccessfulThingPlacement(firstThing, selectedThing);
}

} // namespace sed
