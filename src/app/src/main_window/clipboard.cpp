#include "main_window/common.hpp"

#include "sed/assets/material_dimensions.hpp"
#include "sed/level/clipboard.hpp"
#include "ui/project/project_startup_utils.hpp"
#include "ui/shortcuts/shortcut_context_utils.hpp"

namespace sed
{
namespace
{

constexpr auto kClipboardControllerName = "viewportEntityClipboardController";
constexpr auto kCopyActionName = "edit.copy_entities";
constexpr auto kPasteActionName = "edit.paste_entities";
constexpr auto kTestPasteTargetProperty = "sed_entity_clipboard_test_target";
constexpr auto kEntityClipboardMimeType =
    "application/x-sed-entities-v1";
constexpr auto kMotsArchLightingClipboardMimeType =
    "application/x-sed-mots-archlighting-v1";

class EntityClipboardController final : public QObject
{
public:
    explicit EntityClipboardController(QObject* parent)
        : QObject(parent)
    {
        setObjectName(QString::fromLatin1(kClipboardControllerName));
    }

    level::EntityClipboard clipboard{};
    QAction* copyAction{};
    QAction* pasteAction{};
};

template <typename Set>
std::vector<std::int32_t> sortedIndices(const Set& values)
{
    std::vector<std::int32_t> indices(values.begin(), values.end());
    std::sort(indices.begin(), indices.end());
    indices.erase(std::unique(indices.begin(), indices.end()), indices.end());
    return indices;
}

QString entityNoun(const level::EntityClipboardKind kind, const std::size_t count)
{
    switch (kind)
    {
        case level::EntityClipboardKind::sectors:
            return count == 1 ? QStringLiteral("Sector") : QStringLiteral("Sectors");
        case level::EntityClipboardKind::things:
            return count == 1 ? QStringLiteral("Thing") : QStringLiteral("Things");
        case level::EntityClipboardKind::lights:
            return count == 1 ? QStringLiteral("light") : QStringLiteral("lights");
        case level::EntityClipboardKind::none:
        default:
            return QStringLiteral("items");
    }
}

bool systemClipboardHasEntities() noexcept
{
    const QClipboard* clipboard = QApplication::clipboard();
    const QMimeData* mime = clipboard ? clipboard->mimeData() : nullptr;
    return mime && mime->hasFormat(QString::fromLatin1(kEntityClipboardMimeType));
}

bool systemClipboardHasArchitecturalLighting() noexcept
{
    const QClipboard* const clipboard = QApplication::clipboard();
    const QMimeData* const mime = clipboard ? clipboard->mimeData() : nullptr;
    return mime && mime->hasFormat(
        QString::fromLatin1(kMotsArchLightingClipboardMimeType));
}

std::optional<level::EntityClipboard> readSystemEntityClipboard(
    QString& error)
{
    error.clear();
    const QClipboard* clipboard = QApplication::clipboard();
    const QMimeData* mime = clipboard ? clipboard->mimeData() : nullptr;
    if (!mime || !mime->hasFormat(QString::fromLatin1(kEntityClipboardMimeType)))
    {
        error = QStringLiteral("The system clipboard does not contain SED entities.");
        return std::nullopt;
    }
    const QByteArray encoded = mime->data(
        QString::fromLatin1(kEntityClipboardMimeType));
    const std::span<const std::byte> bytes(
        reinterpret_cast<const std::byte*>(encoded.constData()),
        static_cast<std::size_t>(encoded.size()));
    std::string decodeError{};
    std::optional<level::EntityClipboard> result =
        level::decodeEntityClipboard(bytes, &decodeError);
    if (!result)
    {
        error = decodeError.empty()
            ? QStringLiteral("The SED entity clipboard is invalid.")
            : QString::fromStdString(decodeError);
    }
    return result;
}

} // namespace

void MainWindow::setupEditClipboardActions(QMenu* editMenu)
{
    if (!editMenu)
    {
        return;
    }

    auto* controller = new EntityClipboardController(this);

    controller->copyAction = new QAction("&Copy Entities", this);
    controller->copyAction->setObjectName(QString::fromLatin1(kCopyActionName));
    controller->copyAction->setIcon(ui::makeViewportIcon(ui::ViewportIcon::actionCopy));
    controller->copyAction->setShortcut(QKeySequence::Copy);
    controller->copyAction->setShortcutContext(Qt::WidgetShortcut);
    controller->copyAction->setToolTip("Copy selected Sectors, Things, or baked editor lights");

    controller->pasteAction = new QAction("&Paste Entities", this);
    controller->pasteAction->setObjectName(QString::fromLatin1(kPasteActionName));
    controller->pasteAction->setIcon(ui::makeViewportIcon(ui::ViewportIcon::actionPaste));
    controller->pasteAction->setShortcut(QKeySequence::Paste);
    controller->pasteAction->setShortcutContext(Qt::WidgetShortcut);
    controller->pasteAction->setToolTip("Paste copied entities at the viewport pointer");
    const bool systemHasEntities = systemClipboardHasEntities();
    const bool systemHasArchitecturalLighting =
        systemClipboardHasArchitecturalLighting();
    controller->pasteAction->setProperty(
        "sed.clipboard_has_entities", systemHasEntities);
    controller->pasteAction->setProperty(
        "sed.clipboard_has_arch_lighting", systemHasArchitecturalLighting);
    controller->pasteAction->setEnabled(
        documentEditingAllowed()
        && (editMode_ == EditMode::light
            && lightEditSubMode_ == LightEditSubMode::architectural
            ? systemHasArchitecturalLighting
            : systemHasEntities));

    if (QClipboard* systemClipboard = QApplication::clipboard())
    {
        connect(systemClipboard, &QClipboard::dataChanged, controller,
            [this, controller]()
            {
                const bool hasEntities = systemClipboardHasEntities();
                const bool hasArchitecturalLighting =
                    systemClipboardHasArchitecturalLighting();
                if (!hasEntities)
                {
                    controller->clipboard = level::EntityClipboard{};
                }
                controller->pasteAction->setProperty(
                    "sed.clipboard_has_entities", hasEntities);
                controller->pasteAction->setProperty(
                    "sed.clipboard_has_arch_lighting", hasArchitecturalLighting);
                controller->pasteAction->setEnabled(
                    documentEditingAllowed()
                    && (editMode_ == EditMode::light
                        && lightEditSubMode_ == LightEditSubMode::architectural
                        ? hasArchitecturalLighting
                        : hasEntities));
            });
    }

    connect(controller->copyAction, &QAction::triggered, this, [this, controller]()
        {
            // Text controls retain their native Copy shortcut. The editor
            // clipboard is only for viewport/editor selections.
            if (ui::shortcuts::focusAcceptsTextInput(QApplication::focusWidget()))
            {
                return;
            }
            const editor::Selection& selection = document_.selection();
            std::optional<level::EntityClipboard> copied{};

            auto copySectors = [&]()
            {
                const std::vector<std::int32_t> indices = sortedIndices(selection.sectors);
                const std::int32_t active = glView_ ? glView_->activeSectorIndex() : lastViewportClickSector_;
                copied = level::copySectorsToClipboard(document_.level(), indices, active);
            };
            auto copyThings = [&]()
            {
                const std::vector<std::int32_t> indices = sortedIndices(selection.things);
                const std::int32_t active = glView_ ? glView_->activeThingIndex() : lastViewportClickThing_;
                copied = level::copyThingsToClipboard(document_.level(), indices, active);
            };
            auto copyLights = [&]()
            {
                const std::vector<std::int32_t> indices = sortedIndices(selection.lights);
                copied = level::copyLightsToClipboard(document_.level(), indices, lastViewportClickLight_);
            };

            if (editMode_ == EditMode::sector)
            {
                copySectors();
            }
            else if (editMode_ == EditMode::thing || editMode_ == EditMode::frame)
            {
                copyThings();
            }
            else if (editMode_ == EditMode::light)
            {
                switch (lightEditSubMode_)
                {
                    case LightEditSubMode::thing:
                        copyThings();
                        break;
                    case LightEditSubMode::sector:
                        copySectors();
                        break;
                    case LightEditSubMode::baked:
                        copyLights();
                        break;
                    case LightEditSubMode::architectural:
                        (void)copyArchitecturalLightingClipboard();
                        return;
                }
            }
            else if (editMode_ == EditMode::none)
            {
                const int populatedFamilies = static_cast<int>(!selection.sectors.empty())
                    + static_cast<int>(!selection.things.empty())
                    + static_cast<int>(!selection.lights.empty());
                if (populatedFamilies == 1)
                {
                    if (!selection.sectors.empty())
                    {
                        copySectors();
                    }
                    else if (!selection.things.empty())
                    {
                        copyThings();
                    }
                    else
                    {
                        copyLights();
                    }
                }
            }

            if (!copied || level::entityClipboardCount(*copied) == 0)
            {
                if (glView_)
                {
                    glView_->showToast(
                        QStringLiteral("Select Sectors, Things, or baked lights to copy"),
                        2200,
                        GlView::ToastPlacement::BottomCenter,
                        GlView::ToastGroup::Error);
                }
                return;
            }

            std::string encodeError{};
            const std::optional<std::vector<std::byte>> encoded =
                level::encodeEntityClipboard(*copied, &encodeError);
            if (!encoded)
            {
                const QString message = encodeError.empty()
                    ? QStringLiteral("Could not prepare the selected entities for copying.")
                    : QString::fromStdString(encodeError);
                qWarning().noquote() << "Copy entities failed:" << message;
                if (glView_)
                {
                    glView_->showToast(
                        QStringLiteral("Copy failed: %1").arg(message),
                        2600,
                        GlView::ToastPlacement::BottomCenter,
                        GlView::ToastGroup::Error);
                }
                return;
            }

            controller->clipboard = std::move(*copied);
            if (QClipboard* systemClipboard = QApplication::clipboard())
            {
                auto* mime = new QMimeData();
                mime->setData(
                    QString::fromLatin1(kEntityClipboardMimeType),
                    QByteArray(
                        reinterpret_cast<const char*>(encoded->data()),
                        static_cast<qsizetype>(encoded->size())));
                const std::size_t count =
                    level::entityClipboardCount(controller->clipboard);
                mime->setText(QStringLiteral("SED: %1 %2")
                    .arg(static_cast<qulonglong>(count))
                    .arg(entityNoun(
                        level::entityClipboardKind(controller->clipboard),
                        count)));
                systemClipboard->setMimeData(mime);
            }
            controller->pasteAction->setProperty("sed.clipboard_has_entities", true);
            controller->pasteAction->setEnabled(documentEditingAllowed());

            if (glView_)
            {
                const std::size_t count = level::entityClipboardCount(controller->clipboard);
                glView_->showToast(
                    QStringLiteral("Copied %1 %2")
                        .arg(static_cast<qulonglong>(count))
                        .arg(entityNoun(level::entityClipboardKind(controller->clipboard), count)),
                    1600,
                    GlView::ToastPlacement::BottomCenter,
                    GlView::ToastGroup::General);
            }
        });

    connect(controller->pasteAction, &QAction::triggered, this, [this, controller]()
        {
            if (ui::shortcuts::focusAcceptsTextInput(QApplication::focusWidget()))
            {
                return;
            }
            if (!documentEditingAllowed())
            {
                return;
            }
            if (editMode_ == EditMode::light
                && lightEditSubMode_ == LightEditSubMode::architectural)
            {
                (void)pasteArchitecturalLightingClipboard();
                return;
            }
            QString clipboardError{};
            const std::optional<level::EntityClipboard> systemClipboard =
                readSystemEntityClipboard(clipboardError);
            if (!systemClipboard)
            {
                qWarning().noquote()
                    << "Paste entities failed:" << clipboardError;
                if (glView_)
                {
                    glView_->showToast(
                        QStringLiteral("Paste failed: %1").arg(clipboardError),
                        2800,
                        GlView::ToastPlacement::BottomCenter,
                        GlView::ToastGroup::Error);
                }
                return;
            }
            controller->clipboard = *systemClipboard;
            const level::EntityClipboardKind kind =
                level::entityClipboardKind(controller->clipboard);
            if (kind == level::EntityClipboardKind::none || !glView_)
            {
                return;
            }

            setPlacementMode(PlacementMode::none);
            if (kind == level::EntityClipboardKind::sectors)
            {
                setEditMode(EditMode::sector);
            }
            else if (kind == level::EntityClipboardKind::things)
            {
                setEditMode(EditMode::thing);
            }
            else
            {
                setLightEditSubMode(LightEditSubMode::baked);
                setEditMode(EditMode::light);
            }

            // Normal Paste follows the legacy editor's viewport-targeted
            // behavior. Tests can inject an exact world target without moving
            // the process-global OS cursor.
            std::optional<TVector> target{};
            const QVariant forcedTarget = property(kTestPasteTargetProperty);
            if (forcedTarget.isValid() && forcedTarget.canConvert<QVector3D>())
            {
                const QVector3D point = forcedTarget.value<QVector3D>();
                target = TVector{
                    static_cast<double>(point.x()),
                    static_cast<double>(point.y()),
                    static_cast<double>(point.z()) };
            }
            else
            {
                const QPoint localCursor = glView_->mapFromGlobal(QCursor::pos());
                std::optional<QVector3D> point{};
                if (glView_->rect().contains(localCursor))
                {
                    point = glView_->placementPointForViewport(
                        localCursor,
                        nullptr,
                        nullptr,
                        nullptr,
                        false,
                        true);
                }
                const QVector3D fallback = glView_->cameraTarget();
                const QVector3D resolved = point.value_or(fallback);
                target = TVector{
                    static_cast<double>(resolved.x()),
                    static_cast<double>(resolved.y()),
                    static_cast<double>(resolved.z()) };
            }

            editor::DocumentDirty dirty = editor::DocumentDirty::None;
            QString undoLabel{};
            const std::size_t clipboardCount = level::entityClipboardCount(controller->clipboard);
            switch (kind)
            {
                case level::EntityClipboardKind::sectors:
                    dirty = editor::DocumentDirty::LevelGeometry | editor::DocumentDirty::Structure;
                    undoLabel = QString::fromStdString(
                        ui::undo::action("Sector", clipboardCount, "Pasted"));
                    break;
                case level::EntityClipboardKind::things:
                    dirty = editor::DocumentDirty::Things | editor::DocumentDirty::Structure;
                    undoLabel = QString::fromStdString(
                        ui::undo::action("Thing", clipboardCount, "Pasted"));
                    break;
                case level::EntityClipboardKind::lights:
                    dirty = editor::DocumentDirty::Lights | editor::DocumentDirty::Structure;
                    undoLabel = QString::fromStdString(
                        ui::undo::action("Baked Light", "Baked Lights", clipboardCount, "Pasted"));
                    break;
                case level::EntityClipboardKind::none:
                    return;
            }
            if (level::entityClipboardRequiresLayerImport(document_.level(), controller->clipboard))
            {
                // Layer vectors live outside the optimized Things/Lights
                // history slices, so cross-document layer import needs a full
                // snapshot to remain exactly undoable.
                dirty = editor::DocumentDirty::All;
            }

            std::optional<level::EntityPasteResult> pasteResult{};
            const bool applied = document_.applyUndoable(
                undoLabel.toStdString(),
                dirty,
                [this, controller, &pasteResult, target](editor::DocumentState& state) -> bool
                {
                    std::unordered_map<std::string,
                        std::optional<level::UvTextureDimensions>>
                        dimensionCache{};
                    pasteResult = level::pasteEntityClipboard(
                        state.level,
                        controller->clipboard,
                        target,
                        [this, &dimensionCache](const Surface& surface)
                            -> std::optional<level::UvTextureDimensions>
                        {
                            const std::string key =
                                text::lowerAscii(surface.material);
                            if (const auto found = dimensionCache.find(key);
                                found != dimensionCache.end())
                            {
                                return found->second;
                            }

                            std::optional<level::UvTextureDimensions>
                                dimensions =
                                    assets::resolveMaterialTextureDimensions(
                                        resources_,
                                        surface.material);
                            if (!dimensions
                                && text::iequalsAscii(
                                    surface.material,
                                    "dflt.mat"))
                            {
                                dimensions = ui::project::
                                    DefaultStarterMaterialDimensions;
                            }
                            dimensionCache.emplace(key, dimensions);
                            return dimensions;
                        });
                    if (!pasteResult || pasteResult->indices.empty())
                    {
                        return false;
                    }

                    state.selection.clear();
                    for (const std::int32_t index : pasteResult->indices)
                    {
                        switch (pasteResult->kind)
                        {
                            case level::EntityClipboardKind::sectors:
                                state.selection.sectors.insert(index);
                                break;
                            case level::EntityClipboardKind::things:
                                state.selection.things.insert(index);
                                break;
                            case level::EntityClipboardKind::lights:
                                state.selection.lights.insert(index);
                                break;
                            case level::EntityClipboardKind::none:
                                break;
                        }
                    }
                    return true;
                });

            if (!applied || !pasteResult)
            {
                qWarning().noquote()
                    << "Paste entities failed after clipboard validation:"
                    << entityNoun(kind, clipboardCount)
                    << "payload could not be applied to the destination level";
                glView_->showToast(
                    QStringLiteral("Paste failed"),
                    2200,
                    GlView::ToastPlacement::BottomCenter,
                    GlView::ToastGroup::Error);
                return;
            }

            if (editor::hasAny(dirty, editor::DocumentDirty::Structure)
                && editor::hasAny(dirty, editor::DocumentDirty::LevelGeometry))
            {
                invalidateSurfaceSelectionContextForTopologyEdit();
            }

            if (pasteResult->addedLayers)
            {
                syncLayerViewState();
            }

            const std::int32_t active = pasteResult->indices.back();
            switch (pasteResult->kind)
            {
                case level::EntityClipboardKind::sectors:
                    glView_->notifyLevelGeometryChanged();
                    glView_->setSelectionState(active, -1, -1, -1, document_.selection());
                    break;
                case level::EntityClipboardKind::things:
                    glView_->notifyThingsChanged();
                    glView_->setSelectionState(-1, -1, active, -1, document_.selection());
                    break;
                case level::EntityClipboardKind::lights:
                    glView_->notifyLightsChanged();
                    glView_->setSelectionState(-1, -1, -1, active, document_.selection());
                    break;
                case level::EntityClipboardKind::none:
                    break;
            }

            rebuildOutliner();
            rebuildProperties();
            updateEditActions();
            scheduleRebuildChecks(dirty);
            refreshWindowTitle();

            const std::size_t count = pasteResult->indices.size();
            glView_->showToast(
                QStringLiteral("Pasted %1 %2")
                    .arg(static_cast<qulonglong>(count))
                    .arg(entityNoun(pasteResult->kind, count)),
                1800,
                GlView::ToastPlacement::BottomCenter,
                GlView::ToastGroup::General);
        });

    editMenu->addAction(controller->copyAction);
    editMenu->addAction(controller->pasteAction);
}

} // namespace sed
