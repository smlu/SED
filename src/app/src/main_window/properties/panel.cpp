#include "main_window/common.hpp"
#include "ui/components/typed_numeric_text.hpp"
#include "main_window/properties/sector_point_light_clipboard_utils.hpp"
#include "render/gizmos/sector_thrust_gizmo_utils.hpp"
#include "thing/thing_light_authoring_utils.hpp"
#include "thing/thing_parameter_editor_utils.hpp"
#include "thing/thing_parameter_utils.hpp"
#include "thing/thing_resource_utils.hpp"
#include "ui/query/level_query_utils.hpp"
#include "ui/cog/cog_instance_picker.hpp"
#include "ui/resources/project_resource_settings.hpp"
#include "ui/resources/resource_picker_kind_utils.hpp"
#include "ui/surface/surface_extra_light_utils.hpp"
#include "ui/transform/rotation_utils.hpp"

#include <QBrush>
#include <QGraphicsOpacityEffect>
#include <QGuiApplication>
#include <QPainter>
#include <QPropertyAnimation>
#include <QPointer>
#include <QScopeGuard>
#include <QScreen>
#include <QTreeWidgetItemIterator>

#include <cmath>
#include <limits>
#include <optional>
#include <vector>

namespace sed {

#include "main_window/detail/helpers.hpp"

namespace {

constexpr int kNumericPropertyValueRole = Qt::UserRole + 220;

struct SelectedFrameOrdinal {
    level::FrameRef ref{};
    std::size_t ordinal{};
};

std::optional<std::size_t> frameOrdinalForValueIndex(
    const Thing& thing, const std::int32_t valueIndex)
{
    if (valueIndex < 0
        || static_cast<std::size_t>(valueIndex) >= thing.values.size())
    {
        return std::nullopt;
    }

    std::size_t ordinal = 0;
    for (std::size_t index = 0; index < thing.values.size(); ++index)
    {
        const TplValue& value = thing.values[index];
        if (value.type != ValueType::frame
            || !text::iequalsAscii(value.name, "frame"))
        {
            continue;
        }
        if (index == static_cast<std::size_t>(valueIndex))
        {
            return ordinal;
        }
        ++ordinal;
    }
    return std::nullopt;
}

std::optional<std::int32_t> frameValueIndexForOrdinal(
    const Thing& thing, const std::size_t targetOrdinal)
{
    std::size_t ordinal = 0;
    for (std::size_t index = 0; index < thing.values.size(); ++index)
    {
        const TplValue& value = thing.values[index];
        if (value.type != ValueType::frame
            || !text::iequalsAscii(value.name, "frame"))
        {
            continue;
        }
        if (ordinal == targetOrdinal)
        {
            if (index > static_cast<std::size_t>(
                    std::numeric_limits<std::int32_t>::max()))
            {
                return std::nullopt;
            }
            return static_cast<std::int32_t>(index);
        }
        ++ordinal;
    }
    return std::nullopt;
}

std::vector<SelectedFrameOrdinal> selectedFrameOrdinalsForThing(
    const editor::Selection& selection, const Thing& thing,
    const std::int32_t thingIndex)
{
    std::vector<SelectedFrameOrdinal> result{};
    for (const level::FrameRef ref : selection.frames)
    {
        if (ref.thing != thingIndex)
        {
            continue;
        }
        if (const auto ordinal = frameOrdinalForValueIndex(thing, ref.value))
        {
            result.push_back({ ref, *ordinal });
        }
    }
    return result;
}

void remapSelectedFramesForThing(
    editor::Selection& selection, const std::int32_t thingIndex,
    const std::vector<SelectedFrameOrdinal>& oldSelection,
    const Thing& updatedThing,
    const std::optional<std::size_t> deletedOrdinal = std::nullopt)
{
    for (auto it = selection.frames.begin(); it != selection.frames.end();)
    {
        if (it->thing == thingIndex)
        {
            it = selection.frames.erase(it);
        }
        else
        {
            ++it;
        }
    }

    for (const SelectedFrameOrdinal& selected : oldSelection)
    {
        if (deletedOrdinal && selected.ordinal == *deletedOrdinal)
        {
            continue;
        }
        const std::size_t mappedOrdinal = deletedOrdinal
                && selected.ordinal > *deletedOrdinal
            ? selected.ordinal - 1u
            : selected.ordinal;
        if (const auto valueIndex = frameValueIndexForOrdinal(
                updatedThing, mappedOrdinal))
        {
            selection.frames.insert({ thingIndex, *valueIndex });
        }
    }
}

template <typename Remove>
bool removeThingParameterPreservingFrameSelection(
    editor::DocumentState& state, const std::int32_t thingIndex,
    Remove&& remove)
{
    if (thingIndex < 0
        || static_cast<std::size_t>(thingIndex) >= state.level.things.size())
    {
        return false;
    }

    Thing& thing = state.level.things[static_cast<std::size_t>(thingIndex)];
    if (state.selection.frames.empty())
    {
        return std::forward<Remove>(remove)(thing);
    }
    const std::vector<SelectedFrameOrdinal> oldSelection =
        selectedFrameOrdinalsForThing(state.selection, thing, thingIndex);
    if (oldSelection.empty())
    {
        return std::forward<Remove>(remove)(thing);
    }

    const bool changed = std::forward<Remove>(remove)(thing);
    if (changed)
    {
        remapSelectedFramesForThing(
            state.selection, thingIndex, oldSelection, thing);
    }
    return changed;
}

class PropertyActionFeedbackLabel final : public QLabel {
public:
  PropertyActionFeedbackLabel(const QString &message, QWidget *parent,
                              const bool error)
      : QLabel(message, parent,
               Qt::Tool | Qt::FramelessWindowHint |
                   Qt::WindowDoesNotAcceptFocus | Qt::NoDropShadowWindowHint),
        error_(error) {
    setContentsMargins(7, 4, 7, 4);

    QPalette colors = palette();
    colors.setColor(QPalette::WindowText,
                    error_ ? QColor(QStringLiteral("#ffe7e4"))
                           : QColor(QStringLiteral("#e7f7eb")));
    setPalette(colors);
  }

protected:
  void paintEvent(QPaintEvent *event) override {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    // A translucent top-level widget supplies genuinely transparent pixels
    // outside the rounded rectangle. Paint the panel itself explicitly so
    // native tooltip styling cannot reduce it to floating text on Windows.
    painter.setCompositionMode(QPainter::CompositionMode_Source);
    painter.fillRect(rect(), Qt::transparent);
    painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
    painter.setPen(QPen(error_ ? QColor(QStringLiteral("#e06a62"))
                               : QColor(QStringLiteral("#55bd74")),
                        1.0));
    painter.setBrush(error_ ? QColor(QStringLiteral("#3a2422"))
                            : QColor(QStringLiteral("#203326")));
    painter.drawRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), 5.0,
                            5.0);
    painter.end();

    QLabel::paintEvent(event);
  }

private:
  bool error_{};
};

std::optional<std::uint32_t> thingParameterFlagBits(const TplValue *value) {
  if (!value) {
    return std::nullopt;
  }
  if (value->type == ValueType::integer || value->type == ValueType::ptr ||
      value->type == ValueType::flag) {
    return static_cast<std::uint32_t>(value->intValue);
  }

  TplValue parsed{};
  parsed.name = value->name;
  parsed.type = ValueType::flag;
  if (!parsed.parse(value->asString())) {
    return std::nullopt;
  }
  return static_cast<std::uint32_t>(parsed.intValue);
}

std::int32_t activeSelectedThingIndex(const Level &level,
                                      const editor::Selection &selection,
                                      const GlView *view) noexcept {
  const std::int32_t viewportActive = view ? view->activeThingIndex() : -1;
  if (viewportActive >= 0 &&
      static_cast<std::size_t>(viewportActive) < level.things.size() &&
      selection.things.contains(viewportActive)) {
    return viewportActive;
  }

  // Selection containers are unordered. Use a deterministic fallback when
  // no viewport primary exists rather than allowing hash iteration order to
  // move the Item Edit values and transform pivot between selected Things.
  std::int32_t firstValid = -1;
  for (const std::int32_t thingIndex : selection.things) {
    if (thingIndex < 0 ||
        static_cast<std::size_t>(thingIndex) >= level.things.size()) {
      continue;
    }
    if (firstValid < 0 || thingIndex < firstValid) {
      firstValid = thingIndex;
    }
  }
  return firstValid;
}

double parameterTextContrastRatio(const QColor &foreground,
                                  const QColor &background) {
  const auto luminance = [](const QColor &color) {
    const auto linear = [](const double channel) {
      const double normalized = channel / 255.0;
      return normalized <= 0.04045
                 ? normalized / 12.92
                 : std::pow((normalized + 0.055) / 1.055, 2.4);
    };
    return 0.2126 * linear(color.red()) +
           0.7152 * linear(color.green()) +
           0.0722 * linear(color.blue());
  };
  const double foregroundLuminance = luminance(foreground);
  const double backgroundLuminance = luminance(background);
  const double lighter = std::max(foregroundLuminance, backgroundLuminance);
  const double darker = std::min(foregroundLuminance, backgroundLuminance);
  return (lighter + 0.05) / (darker + 0.05);
}

QColor inheritedParameterTextColor(const QPalette &palette) {
  const QColor text = palette.color(QPalette::Active, QPalette::Text);
  const QColor base = palette.color(QPalette::Active, QPalette::Base);
  // Keep inherited values visibly subordinate without using widget opacity or
  // the disabled state. A 2:1 blend toward the normal text color remains
  // readable in both the shipped dark palette and a light system palette.
  const QColor muted((text.red() * 2 + base.red()) / 3,
                     (text.green() * 2 + base.green()) / 3,
                     (text.blue() * 2 + base.blue()) / 3);
  // A low-contrast host palette cannot afford additional dimming. Retain its
  // normal text color rather than making inherited data hard to read.
  return parameterTextContrastRatio(muted, base) >= 4.5 ? muted : text;
}

void applyInheritedParameterAppearance(QLineEdit *edit,
                                       QTreeWidgetItem *item,
                                       const bool inheritedOnly) {
  if (!edit) {
    return;
  }

  edit->setProperty("sed.inheritedTemplateParameter", inheritedOnly);
  if (!inheritedOnly) {
    return;
  }

  const QColor muted = inheritedParameterTextColor(edit->palette());
  QPalette editPalette = edit->palette();
  editPalette.setColor(QPalette::Active, QPalette::Text, muted);
  editPalette.setColor(QPalette::Inactive, QPalette::Text, muted);
  edit->setPalette(editPalette);
  // The application theme specifies QLineEdit text through a style-sheet
  // selector, which otherwise takes precedence over a per-widget palette.
  edit->setStyleSheet(
      QStringLiteral("QLineEdit { color: %1; }").arg(muted.name()));
  edit->setProperty("sed.inheritedTemplateParameterTextColor", muted);
  edit->setProperty(
      "sed.inheritedTemplateParameterContrastRatio",
      parameterTextContrastRatio(
          muted, edit->palette().color(QPalette::Active, QPalette::Base)));
  if (item) {
    item->setForeground(0, QBrush(muted));
    item->setForeground(1, QBrush(muted));
  }
}

bool sameCogValueSnapshot(const CogValue& current, const CogValue& snapshot)
{
  return current.name == snapshot.name
      && current.type == snapshot.type
      && current.intValue == snapshot.intValue
      && current.floatValue == snapshot.floatValue
      && current.stringValue == snapshot.stringValue
      && current.cogType == snapshot.cogType
      && current.local == snapshot.local
      && current.description == snapshot.description
      && current.mask == snapshot.mask;
}

bool sameEditorSelectionSnapshot(
    const editor::Selection& current,
    const editor::Selection& snapshot)
{
  const auto sameSet = [](const auto& left, const auto& right)
  {
    return left.size() == right.size() &&
           std::all_of(right.begin(), right.end(), [&left](const auto& item)
           {
             return left.find(item) != left.end();
           });
  };
  return sameSet(current.sectors, snapshot.sectors) &&
         sameSet(current.surfaces, snapshot.surfaces) &&
         sameSet(current.surfaceVertices, snapshot.surfaceVertices) &&
         sameSet(current.things, snapshot.things) &&
         sameSet(current.frames, snapshot.frames) &&
         sameSet(current.lights, snapshot.lights) &&
         sameSet(current.cogs, snapshot.cogs);
}

} // namespace

std::optional<SurfaceRef>
MainWindow::activeSelectedSurfaceRef() const noexcept {
  const Level &level = document_.level();
  const editor::Selection &selection = document_.selection();

  const auto valid = [&level](const SurfaceRef ref) noexcept {
    return ref.sector >= 0 && ref.surface >= 0 &&
           static_cast<std::size_t>(ref.sector) < level.sectors.size() &&
           static_cast<std::size_t>(ref.surface) <
               level.sectors[static_cast<std::size_t>(ref.sector)]
                   .surfaces.size();
  };
  const auto selected = [&selection](const SurfaceRef ref) noexcept {
    if (selection.surfaces.contains(ref)) {
      return true;
    }
    return std::any_of(selection.surfaceVertices.begin(),
                       selection.surfaceVertices.end(),
                       [ref](const editor::SurfaceVertexRef vertex) noexcept {
                         return vertex.surface == ref;
                       });
  };

  // In Vertex mode the exact active sub-element is the strongest owner hint.
  // This also makes a newly Shift-picked vertex update Item Edit immediately,
  // even if another UI publication is still reconciling the face sidecar.
  if (surfaceSelectionMode_ == SurfaceSelectionMode::vertex && glView_) {
    if (const auto vertex = glView_->activeSurfaceVertex();
        vertex && selection.surfaceVertices.contains(*vertex) &&
        valid(vertex->surface)) {
      return vertex->surface;
    }
  }

  // Topology Undo/Redo revalidates transient exact edges before republishing
  // the broader viewport selection sidecar. If the former active edge was
  // dropped, the surviving promoted edge owner is authoritative immediately.
  if (surfaceSelectionMode_ == SurfaceSelectionMode::edge && glView_) {
    if (const auto edgeOwner = glView_->activeSurfaceEdgeOwner();
        edgeOwner && valid(*edgeOwner) && selected(*edgeOwner)) {
      return edgeOwner;
    }
  }

  // Face mode and the settled Edge/Vertex paths publish the active owner
  // through this explicit GlView primary. Never accept a stale sidecar which
  // is no longer in the semantic selection.
  if (glView_) {
    if (const auto viewport = glView_->selectedSurfaceRef();
        viewport && valid(*viewport) && selected(*viewport)) {
      return viewport;
    }
  }

  // Documents loaded without editor-sidecar state and marquee selections
  // still need a stable current face. Lexicographic order is deterministic;
  // unlike unordered_set::begin(), it is independent of hash layout.
  std::optional<SurfaceRef> fallback{};
  const auto consider = [&](const SurfaceRef ref) {
    if (!valid(ref)) {
      return;
    }
    if (!fallback || ref.sector < fallback->sector ||
        (ref.sector == fallback->sector && ref.surface < fallback->surface)) {
      fallback = ref;
    }
  };
  for (const SurfaceRef ref : selection.surfaces) {
    consider(ref);
  }
  for (const editor::SurfaceVertexRef vertex : selection.surfaceVertices) {
    consider(vertex.surface);
  }
  return fallback;
}

void MainWindow::refreshPropertiesEditability()
{
    if (!properties_)
    {
        return;
    }

    const bool editingAllowed = documentEditingAllowed();
    const bool selectionReadOnly =
        selectionTouchesNonEditableLayer(document_.selection());
    const bool propertyEditingAllowed = editingAllowed && !selectionReadOnly;

    properties_->setEnabled(editingAllowed);
    if (thingParametersPanel_)
    {
        thingParametersPanel_->setEnabled(editingAllowed);
    }
    if (thingParameters_)
    {
        thingParameters_->setEnabled(propertyEditingAllowed);
    }
    if (thingParameterSearch_)
    {
        thingParameterSearch_->setEnabled(propertyEditingAllowed);
    }
    if (cogLinksPanel_)
    {
        cogLinksPanel_->setEnabled(editingAllowed);
    }
    if (cogLinksList_)
    {
        cogLinksList_->setEnabled(propertyEditingAllowed);
    }

    const auto applyItem =
        [this, editingAllowed, selectionReadOnly](
            auto&& self,
            QTreeWidgetItem* item) -> void
    {
        if (!item)
        {
            return;
        }

        const bool isGroup = item->isFirstColumnSpanned();
        const bool isLayer =
            item->text(0).trimmed() == QStringLiteral("Layer");
        // Group rows remain visually enabled so the Item Edit layout stays
        // readable. A hidden/locked selection keeps only Layer editable.
        item->setDisabled(
            !isGroup && (!editingAllowed || (selectionReadOnly && !isLayer)));
        for (int column = 0; column < properties_->columnCount(); ++column)
        {
            if (QWidget* editor = properties_->itemWidget(item, column))
            {
                editor->setEnabled(
                    editingAllowed && (!selectionReadOnly || isLayer));
            }
        }
        for (int child = 0; child < item->childCount(); ++child)
        {
            self(self, item->child(child));
        }
    };

    for (int row = 0; row < properties_->topLevelItemCount(); ++row)
    {
        applyItem(applyItem, properties_->topLevelItem(row));
    }
}

void MainWindow::rebuildProperties() {
  if (!properties_) {
    return;
  }
  if (!properties_->property("sed.numericReadoutsConnected").toBool()) {
    properties_->setProperty("sed.numericReadoutsConnected", true);
    ui::onUiNumericFormatChanged(properties_, [tree = properties_]() {
      const QSignalBlocker blocker(tree);
      for (QTreeWidgetItemIterator item(tree); *item; ++item) {
        const QVariant value = (*item)->data(1, kNumericPropertyValueRole);
        if (value.isValid()) {
          (*item)->setText(1, ui::formatUiNumber(value.toDouble(), 'f', 6));
        }
      }
    });
  }

  // A selection/undo/other UI refresh may rebuild the tree while a numeric
  // Thing scrub is still transient. QTreeWidget is allowed to defer deletion
  // of its old item widgets, so cancel the shader override explicitly before
  // clearing rows rather than depending on QObject::destroyed timing.
  if (glView_) {
    glView_->clearThingPropertyTransformPreview();
  }

  const Level &level = document_.level();
  const bool editingAllowed = documentEditingAllowed();
  const bool selectionReadOnly =
      selectionTouchesNonEditableLayer(document_.selection());
  const bool propertyEditingAllowed = editingAllowed && !selectionReadOnly;

  const QSignalBlocker blocker(properties_);
  properties_->clear();
  // A locked-layer selection keeps the tree live so its Layer row can move
  // the entity back to an editable layer. Preview is stricter: it displays
  // the populated tree but disables the complete editing surface. The scope
  // guard reapplies either policy after every selection-specific early return.
  properties_->setEnabled(editingAllowed);
  if (thingParameters_) {
    const QSignalBlocker thingParametersBlocker(thingParameters_);
    thingParameters_->clear();
    thingParameters_->setEnabled(propertyEditingAllowed);
  }
  if (thingParametersPanel_) {
    thingParametersPanel_->setVisible(false);
    thingParametersPanel_->setEnabled(editingAllowed);
  }
  if (cogLinksList_) {
    const QSignalBlocker cogLinksBlocker(cogLinksList_);
    cogLinksList_->clear();
    cogLinksList_->setEnabled(propertyEditingAllowed);
  }
  if (cogLinksPanel_) {
    cogLinksPanel_->setVisible(false);
    cogLinksPanel_->setEnabled(editingAllowed);
  }

  if (thingParameterSearch_) {
    thingParameterSearch_->setEnabled(propertyEditingAllowed);
  }

  [[maybe_unused]] const auto selectionEditabilityGuard =
      qScopeGuard([this]()
      {
        refreshPropertiesEditability();
      });

  auto notifyViewport = [this](const editor::DocumentDirty dirty) {
    if (!glView_) {
      return;
    }

    const bool levelChanged =
        editor::hasAny(dirty, editor::DocumentDirty::LevelGeometry);
    const bool thingsChanged =
        editor::hasAny(dirty, editor::DocumentDirty::Things);
    const bool lightsChanged =
        editor::hasAny(dirty, editor::DocumentDirty::Lights);

    if (levelChanged) {
      glView_->notifyLevelGeometryChanged();
    }
    if (thingsChanged) {
      glView_->notifyThingsChanged();
    }

    if (lightsChanged) {
      glView_->notifyLightsChanged();
    }
  };

  auto notifyDirty = [this, notifyViewport](const editor::DocumentDirty dirty) {
    scheduleRebuildChecks(dirty);

    if (editor::hasAny(dirty, editor::DocumentDirty::Cogs)) {
      syncCogScriptWatches();
    }

    notifyViewport(dirty);
  };

  auto queueRebuild = [this]() {
    const int verticalScroll = properties_ && properties_->verticalScrollBar()
                                   ? properties_->verticalScrollBar()->value()
                                   : 0;
    const int horizontalScroll =
        properties_ && properties_->horizontalScrollBar()
            ? properties_->horizontalScrollBar()->value()
            : 0;
    const int parameterVerticalScroll =
        thingParameters_ && thingParameters_->verticalScrollBar()
            ? thingParameters_->verticalScrollBar()->value()
            : 0;
    QMetaObject::invokeMethod(
        this,
        [this, verticalScroll, horizontalScroll, parameterVerticalScroll]() {
          rebuildProperties();
          // Property commits rebuild the row widgets. Keep that
          // implementation detail from teleporting a long Sector panel
          // back to its first row after every thrust edit.
          if (properties_ && properties_->verticalScrollBar()) {
            properties_->verticalScrollBar()->setValue(verticalScroll);
          }
          if (properties_ && properties_->horizontalScrollBar()) {
            properties_->horizontalScrollBar()->setValue(horizontalScroll);
          }
          if (thingParameters_ && thingParameters_->verticalScrollBar()) {
            thingParameters_->verticalScrollBar()->setValue(
                parameterVerticalScroll);
          }
        },
        Qt::QueuedConnection);
  };

  auto showPropertyActionFeedback = [this](QWidget *anchor,
                                           const QString &message,
                                           const bool error = false) {
    if (!anchor || message.isEmpty()) {
      return;
    }

    // Keep clipboard feedback directly below the action that produced it.
    // The compact, non-activating popup leaves adjacent action buttons
    // unobstructed while preserving focus in the property editor.
    const auto previous = findChildren<QLabel *>(
        QStringLiteral("propertyActionFeedback"), Qt::FindDirectChildrenOnly);
    for (QLabel *label : previous) {
      if (label) {
        label->setObjectName({});
        label->close();
      }
    }

    auto *feedback = new PropertyActionFeedbackLabel(message, this, error);
    feedback->setObjectName(QStringLiteral("propertyActionFeedback"));
    feedback->setProperty("sed.feedbackError", error);
    feedback->setProperty("sed.feedbackPlacement", QStringLiteral("below"));
    feedback->setProperty("sed.feedbackScreenFixed", true);
    feedback->setProperty("sed.feedbackOpaquePanel", true);
    feedback->setAttribute(Qt::WA_DeleteOnClose, true);
    feedback->setAttribute(Qt::WA_ShowWithoutActivating, true);
    feedback->setAttribute(Qt::WA_TranslucentBackground, true);
    feedback->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    feedback->setMargin(0);
    feedback->setWordWrap(true);
    feedback->setMaximumWidth(200);
    QFont compactFont = feedback->font();
    if (compactFont.pointSizeF() > 0.0) {
      compactFont.setPointSizeF(std::min(compactFont.pointSizeF(), 8.25));
    }
    feedback->setFont(compactFont);
    feedback->adjustSize();

    const QPoint anchorBottom =
        anchor->mapToGlobal(QPoint(0, anchor->height() + 3));
    QPoint position{anchorBottom.x() +
                        (anchor->width() - feedback->width()) / 2,
                    anchorBottom.y()};
    if (const QScreen *screen = QGuiApplication::screenAt(position)) {
      const QRect available =
          screen->availableGeometry().adjusted(4, 4, -4, -4);
      position.setX(std::clamp(
          position.x(), available.left(),
          std::max(available.left(), available.right() - feedback->width())));
      position.setY(std::clamp(
          position.y(), available.top(),
          std::max(available.top(), available.bottom() - feedback->height())));
    }
    auto *opacity = new QGraphicsOpacityEffect(feedback);
    opacity->setOpacity(0.0);
    feedback->setGraphicsEffect(opacity);
    feedback->show();
    // Move after the native top-level window exists. In particular, the
    // Windows tooltip implementation may otherwise re-anchor the window
    // to its owner when the property tree scrolls or rebuilds. A Tool
    // window plus this post-show global move keeps the feedback at the
    // exact screen position where the action was invoked.
    feedback->move(position);
    feedback->raise();

    auto *fadeIn = new QPropertyAnimation(opacity, "opacity", feedback);
    fadeIn->setDuration(100);
    fadeIn->setStartValue(0.0);
    fadeIn->setEndValue(1.0);
    fadeIn->setEasingCurve(QEasingCurve::OutCubic);
    fadeIn->start(QAbstractAnimation::DeleteWhenStopped);

    constexpr int kFadeOutDurationMs = 160;
    const int totalDurationMs = error ? 2800 : 1500;
    QTimer::singleShot(
        totalDurationMs - kFadeOutDurationMs, feedback, [feedback, opacity]() {
          auto *fadeOut = new QPropertyAnimation(opacity, "opacity", feedback);
          fadeOut->setDuration(kFadeOutDurationMs);
          fadeOut->setStartValue(opacity->opacity());
          fadeOut->setEndValue(0.0);
          fadeOut->setEasingCurve(QEasingCurve::InCubic);
          connect(fadeOut, &QPropertyAnimation::finished, feedback,
                  &QWidget::close);
          fadeOut->start(QAbstractAnimation::DeleteWhenStopped);
        });
  };

  auto openAuxiliaryEditorOnDoubleClick = [this](QLineEdit *edit,
                                                 QAbstractButton *button) {
    if (!edit || !button) {
      return;
    }

    edit->setProperty("sed.auxiliaryButton",
                      QVariant::fromValue<QObject *>(button));
    edit->installEventFilter(this);
  };

  auto applyUndoable = [this, notifyDirty, queueRebuild](
                           const std::string &label,
                           const editor::DocumentDirty dirty, auto &&mutate,
                           const bool allowReadOnlySelection = false) -> bool {
    if (!documentEditingAllowed()) {
      return false;
    }
    if (!allowReadOnlySelection &&
        selectionTouchesNonEditableLayer(document_.selection())) {
      return false;
    }
    const std::string effectiveLabel =
        ui::undo::relabelForSelection(label, document_.selection());
    if (!document_.applyUndoable(effectiveLabel, dirty,
                                 std::forward<decltype(mutate)>(mutate))) {
      return false;
    }

    notifyDirty(dirty);
    updateEditActions();
    queueRebuild();
    return true;
  };

  auto applyFrameThingUndoable = [this, queueRebuild](
                                     const std::string &label,
                                     const std::int32_t thingIndex,
                                     auto &&mutate) -> bool {
    if (!documentEditingAllowed() ||
        selectionTouchesNonEditableLayer(document_.selection())) {
      return false;
    }

    const std::string effectiveLabel =
        ui::undo::relabelForSelection(label, document_.selection());
    if (!document_.applyThingSubsetUndoable(
            effectiveLabel, std::vector<std::int32_t>{thingIndex},
            std::forward<decltype(mutate)>(mutate))) {
      return false;
    }

    scheduleRebuildChecks(editor::DocumentDirty::Things);
    if (glView_) {
      glView_->notifyThingFramesChanged(
          std::vector<std::int32_t>{thingIndex});
    }
    updateEditActions();
    queueRebuild();
    return true;
  };

  auto add = [&](const QString &key, const QString &value) {
    auto *item = new QTreeWidgetItem(properties_);
    item->setText(0, key);
    item->setText(1, value);
  };

  auto hex = [](const std::uint32_t value) -> QString {
    return QString("0x%1").arg(value, 0, 16);
  };

  auto connectLiveScrub =
      [this, notifyDirty, queueRebuild, applyUndoable](
          ui::ScrubFloatEdit *scrub, const std::string &undoLabel,
          const editor::DocumentDirty dirty, auto &&applyLiveValue,
          const bool viewportOverlayOnly = false,
          const bool deferUndoUntilFinished = false,
          const std::function<void(const editor::DocumentState &)> &captureOriginal = {},
          const std::function<bool(editor::DocumentState &)> &restoreOriginal = {}) -> void {
    if (!scrub) {
      return;
    }
    if (deferUndoUntilFinished) {
      scrub->setStepperUsesScrubSignals(true);
    }

    static constexpr const char *kStartProp = "sed.liveScrubStart";
    static constexpr const char *kPushedProp = "sed.liveScrubUndoPushed";

    const auto rollbackDeferredPreview =
        [this, scrub, applyLiveValue, dirty, notifyDirty, queueRebuild,
         deferUndoUntilFinished, restoreOriginal]() -> bool {
      if (!deferUndoUntilFinished ||
          !scrub->property(kPushedProp).toBool()) {
        scrub->setProperty(kPushedProp, false);
        return false;
      }
      const double start = scrub->property(kStartProp).toDouble();
      const bool restored = restoreOriginal
          ? restoreOriginal(document_.current())
          : applyLiveValue(document_.current(), start);
      scrub->setProperty(kPushedProp, false);
      if (restored) {
        notifyDirty(dirty);
      }
      queueRebuild();
      return restored;
    };

    connect(scrub, &ui::ScrubFloatEdit::scrubStarted, this,
            [this, scrub, captureOriginal,
             deferUndoUntilFinished](double startValue) {
              scrub->setProperty(kStartProp, startValue);
              scrub->setProperty(kPushedProp, false);
              if (deferUndoUntilFinished && captureOriginal) {
                captureOriginal(document_.current());
              }
            });

    connect(scrub, &ui::ScrubFloatEdit::scrubValueChanged, this,
            [this, scrub, undoLabel, dirty, applyLiveValue, notifyDirty,
             queueRebuild, viewportOverlayOnly,
             deferUndoUntilFinished,
             rollbackDeferredPreview](double v) {
              if (!documentEditingAllowed() || !scrub->isEnabled() ||
                  selectionTouchesNonEditableLayer(document_.selection())) {
                if (scrub->property(kPushedProp).toBool()) {
                  if (deferUndoUntilFinished) {
                    rollbackDeferredPreview();
                  } else {
                    if (viewportOverlayOnly) {
                      scheduleRebuildChecks(dirty);
                    }
                    queueRebuild();
                    scrub->setProperty(kPushedProp, false);
                  }
                }
                return;
              }
              const bool pushed = scrub->property(kPushedProp).toBool();
              const double start = scrub->property(kStartProp).toDouble();
              if (!pushed && std::abs(v - start) <= sed::math::kTinyEpsilon) {
                return;
              }

              if (!pushed) {
                if (!deferUndoUntilFinished) {
                  document_.pushUndo(ui::undo::relabelForSelection(
                                         undoLabel, document_.selection()),
                                     dirty);
                  updateEditActions();
                }
                scrub->setProperty(kPushedProp, true);
              }

              if (!applyLiveValue(document_.current(), v)) {
                return;
              }

              if (viewportOverlayOnly) {
                // Sector thrust changes only its authoring overlay. Avoid
                // rebuilding and uploading the static level mesh for every
                // pointer sample; validation still runs once on release.
                if (glView_) {
                  glView_->update();
                }
              } else {
                notifyDirty(dirty);
              }
            });

    connect(scrub, &ui::ScrubFloatEdit::scrubFinished, this,
            [this, scrub, undoLabel, dirty, applyLiveValue, applyUndoable,
             notifyDirty, queueRebuild, viewportOverlayOnly,
             deferUndoUntilFinished, restoreOriginal,
             rollbackDeferredPreview](double finalValue) {
              if (!documentEditingAllowed() || !scrub->isEnabled() ||
                  selectionTouchesNonEditableLayer(document_.selection())) {
                if (deferUndoUntilFinished) {
                  rollbackDeferredPreview();
                } else {
                  if (scrub->property(kPushedProp).toBool()) {
                    if (viewportOverlayOnly) {
                      scheduleRebuildChecks(dirty);
                    }
                    queueRebuild();
                  }
                  scrub->setProperty(kPushedProp, false);
                }
                return;
              }
              if (!scrub->property(kPushedProp).toBool()) {
                return;
              }
              if (deferUndoUntilFinished) {
                const double start = scrub->property(kStartProp).toDouble();
                const bool restored = restoreOriginal
                    ? restoreOriginal(document_.current())
                    : applyLiveValue(document_.current(), start);
                const bool committed = applyUndoable(
                    undoLabel, dirty,
                    [applyLiveValue, finalValue](
                        editor::DocumentState &state) -> bool {
                      return applyLiveValue(state, finalValue);
                    });
                scrub->setProperty(kPushedProp, false);
                if (!committed) {
                  if (restored) {
                    notifyDirty(dirty);
                  }
                  queueRebuild();
                }
                return;
              }
              if (viewportOverlayOnly) {
                scheduleRebuildChecks(dirty);
              }
              queueRebuild();
            });

    connect(scrub, &ui::ScrubFloatEdit::scrubCanceled, this,
            [scrub, rollbackDeferredPreview,
             deferUndoUntilFinished]() {
              if (deferUndoUntilFinished) {
                rollbackDeferredPreview();
              } else {
                scrub->setProperty(kPushedProp, false);
              }
            });
  };

  struct DeferredThingApplyResult {
    bool applied{};
    bool positionChanged{};
    bool containmentResolved{};
  };

  auto connectDeferredThingTransformScrub =
      [this](ui::ScrubFloatEdit *scrub, const std::string &undoLabel,
             const std::int32_t fallbackThingIndex, auto &&applyFinalValue,
             auto &&previewValue) -> void {
    if (!scrub) {
      return;
    }

    struct DeferredThingScrubState {
      std::vector<std::int32_t> targetedIndices{};
      double startValue{};
      bool shaderPreview{};
    };
    const auto state = std::make_shared<DeferredThingScrubState>();
    scrub->setStepperUsesScrubSignals(true);

    const auto selectionMatchesTargets = [this, state]() -> bool {
      const auto &selection = document_.selection().things;
      if (selection.empty()) {
        return state->targetedIndices.size() == 1u;
      }
      if (selection.size() != state->targetedIndices.size()) {
        return false;
      }
      for (const std::int32_t index : state->targetedIndices) {
        if (!selection.contains(index)) {
          return false;
        }
      }
      return true;
    };

    connect(scrub, &ui::ScrubFloatEdit::scrubStarted, this,
            [this, state, fallbackThingIndex](const double startValue) {
              state->targetedIndices.clear();
              state->shaderPreview = false;
              if (!documentEditingAllowed()) {
                if (glView_) {
                  glView_->clearThingPropertyTransformPreview();
                }
                return;
              }
              state->targetedIndices.assign(
                  document_.selection().things.begin(),
                  document_.selection().things.end());
              if (state->targetedIndices.empty() && fallbackThingIndex >= 0 &&
                  static_cast<std::size_t>(fallbackThingIndex) <
                      document_.level().things.size()) {
                state->targetedIndices.push_back(fallbackThingIndex);
              }
              state->startValue = startValue;
            });

    connect(scrub, &ui::ScrubFloatEdit::scrubValueChanged, this,
            [this, scrub, state, selectionMatchesTargets,
             previewValue](const double value) {
              if (!documentEditingAllowed() || !scrub->isEnabled() ||
                  selectionTouchesNonEditableLayer(document_.selection()) ||
                  state->targetedIndices.empty() ||
                  !selectionMatchesTargets()) {
                state->targetedIndices.clear();
                state->shaderPreview = false;
                if (glView_) {
                  glView_->clearThingPropertyTransformPreview();
                }
                return;
              }
              state->shaderPreview = previewValue(value, state->startValue,
                                                  state->targetedIndices);
            });

    connect(
        scrub, &ui::ScrubFloatEdit::scrubFinished, this,
        [this, scrub, state, selectionMatchesTargets, undoLabel,
         applyFinalValue](const double value) {
          if (!documentEditingAllowed() || !scrub->isEnabled() ||
              selectionTouchesNonEditableLayer(document_.selection()) ||
              state->targetedIndices.empty() || !selectionMatchesTargets()) {
            state->targetedIndices.clear();
            state->shaderPreview = false;
            if (glView_) {
              glView_->clearThingPropertyTransformPreview();
            }
            return;
          }

          const bool changed =
              std::abs(value - state->startValue) > sed::math::kTinyEpsilon;
          if (changed) {
            document_.pushThingSubsetUndo(
                ui::undo::relabelForSelection(undoLabel, document_.selection()),
                editor::DocumentDirty::Things, state->targetedIndices);
            const DeferredThingApplyResult applyResult =
                applyFinalValue(document_.current(), value, state->startValue,
                                state->targetedIndices);
            if (applyResult.applied) {
              if (glView_) {
                glView_->notifyThingTransformsChanged(state->targetedIndices);
              }
              if (applyResult.positionChanged) {
                refreshThingChecks(state->targetedIndices, false,
                                   applyResult.containmentResolved);
              }
            }
          }
          if (glView_) {
            glView_->clearThingPropertyTransformPreview();
          }
          updateEditActions();
          // Position/rotation rows already exist and are updated by the
          // preview path. Avoid a deferred full Item Edit reconstruction
          // (including inherited parameters/resources) at gesture end.
          updateThingPropertiesPreview(false);

          state->shaderPreview = false;
        });

    // A property-panel rebuild can destroy the active scrub without a
    // scrubFinished signal. Never leave its shader override behind.
    const QPointer<GlView> previewView(glView_);
    connect(scrub, &QObject::destroyed, this, [previewView]() {
      if (previewView) {
        previewView->clearThingPropertyTransformPreview();
      }
    });
  };

  auto connectLiveColor =
      [this, notifyDirty, notifyViewport, queueRebuild](
          QPushButton *button, const QColor &initialColor, const bool showAlpha,
          const std::string &undoLabel, const editor::DocumentDirty dirty,
          auto applyLiveColor,
          const bool deferExpensiveRefreshUntilClose = false) -> void {
    if (!button || !initialColor.isValid()) {
      return;
    }

    const QPointer<QPushButton> anchor(button);
    connect(
        button, &QPushButton::clicked, this,
        [this, anchor, initialColor, showAlpha, undoLabel, dirty,
         applyLiveColor, notifyDirty, notifyViewport, queueRebuild,
         deferExpensiveRefreshUntilClose]() mutable {
          if (!documentEditingAllowed() ||
              selectionTouchesNonEditableLayer(document_.selection())) {
            return;
          }
          if (!anchor) {
            return;
          }

          // One popup interaction is one undo step, regardless of how many
          // colors the user previews while dragging through the palette.
          const auto undoPushed = std::make_shared<bool>(false);
          const QColor displayedColor =
              anchor->property("sedColor").value<QColor>();
          auto *popup = new ui::LiveColorPopup(
              this, displayedColor.isValid() ? displayedColor : initialColor,
              showAlpha,
              [this, anchor, undoPushed, undoLabel, dirty, applyLiveColor,
               notifyDirty, notifyViewport,
               deferExpensiveRefreshUntilClose](const QColor &color) mutable {
                if (!documentEditingAllowed()) {
                  return;
                }
                if (!*undoPushed) {
                  document_.pushUndo(ui::undo::relabelForSelection(
                                         undoLabel, document_.selection()),
                                     dirty);
                  updateEditActions();
                  *undoPushed = true;
                }

                if (!applyLiveColor(document_.current(), color)) {
                  return;
                }

                // Property rows are rebuilt when an automatic light bake
                // completes.  The popup deliberately belongs to the main
                // window, so keep its document transaction alive even if
                // that rebuild deletes the old swatch button.
                if (anchor) {
                  ui::configureColorSwatchButton(anchor, color);
                }
                if (deferExpensiveRefreshUntilClose) {
                  notifyViewport(dirty);
                } else {
                  notifyDirty(dirty);
                }
              },
              [this, undoPushed, queueRebuild, dirty,
               deferExpensiveRefreshUntilClose]() {
                if (*undoPushed) {
                  if (deferExpensiveRefreshUntilClose) {
                    scheduleRebuildChecks(dirty);
                  }
                  queueRebuild();
                }
              });
          popup->showFor(anchor);
        });
  };

  auto connectLiveSignedColor =
      [this, notifyDirty, queueRebuild](
          QPushButton *button, const QColor &initialDisplayColor,
          const double initialIntensity, const bool showAlpha,
          const std::string &undoLabel, const editor::DocumentDirty dirty,
          auto applyLiveColor) -> void {
    if (!button || !initialDisplayColor.isValid()) {
      return;
    }

    const QPointer<QPushButton> anchor(button);
    connect(
        button, &QPushButton::clicked, this,
        [this, anchor, initialDisplayColor, initialIntensity, showAlpha,
         undoLabel, dirty, applyLiveColor, notifyDirty,
         queueRebuild]() mutable {
          if (!documentEditingAllowed() ||
              selectionTouchesNonEditableLayer(document_.selection())) {
            return;
          }
          if (!anchor) {
            return;
          }

          // Color and signed intensity are one surface-light property and
          // therefore share one live-preview undo transaction.
          const auto undoPushed = std::make_shared<bool>(false);
          const QColor displayedColor =
              anchor->property("sedColor").value<QColor>();
          const QVariant displayedIntensity =
              anchor->property("sed.surfaceExtraLightIntensity");
          auto *popup = new ui::LiveColorPopup(
              this,
              displayedColor.isValid() ? displayedColor : initialDisplayColor,
              showAlpha,
              displayedIntensity.isValid() ? displayedIntensity.toDouble()
                                           : initialIntensity,
              [this, anchor, undoPushed, undoLabel, dirty, applyLiveColor,
               notifyDirty](const QColor &color,
                            const qreal intensity) mutable {
                if (!documentEditingAllowed()) {
                  return;
                }
                if (!*undoPushed) {
                  document_.pushUndo(ui::undo::relabelForSelection(
                                         undoLabel, document_.selection()),
                                     dirty);
                  updateEditActions();
                  *undoPushed = true;
                }

                if (!applyLiveColor(document_.current(), color, intensity)) {
                  return;
                }

                if (anchor) {
                  ui::configureColorSwatchButton(anchor, color);
                  anchor->setProperty("sed.surfaceExtraLightIntensity",
                                      intensity);
                }
                notifyDirty(dirty);
              },
              [undoPushed, queueRebuild]() {
                if (*undoPushed) {
                  queueRebuild();
                }
              });
          popup->showFor(anchor);
        });
  };

  auto pickPreferredResourceRoot =
      [this]() -> std::optional<std::filesystem::path> {
    if (!resourceRoots_.empty()) {
      for (const std::filesystem::path &root : resourceRoots_) {
        if (sed::text::iequalsAscii(root.filename().string(), "resource")) {
          return root;
        }
      }
      return resourceRoots_.front();
    }
    if (!resources_.roots().empty()) {
      for (const std::filesystem::path &root : resources_.roots()) {
        if (sed::text::iequalsAscii(root.filename().string(), "resource")) {
          return root;
        }
      }
      return resources_.roots().front();
    }
    return std::nullopt;
  };

  auto openResourcePicker =
      [this, pickPreferredResourceRoot](
          const ui::ResourcePickerKind kind, const QString &currentValue,
          const QString &title,
          const bool allowBaseTemplates = true) -> std::optional<QString> {
    std::optional<assets::Templates> templateDatabaseSnapshot;
    std::optional<std::vector<ThingTemplate>> templateOverridesSnapshot;
    if ( kind == ui::ResourcePickerKind::template_ )
    {
      if ( templates_.size() > 0 )
      {
        templateDatabaseSnapshot = templates_;
      }
      templateOverridesSnapshot = document_.level().templates;
    }
    ui::ResourcePickerOptions options{};
    options.kind = kind;
    options.initialName = currentValue.trimmed().toStdString();
    options.preferredDiskRoot = pickPreferredResourceRoot();
    if (!currentPath_.empty()) {
      options.projectDirectory = currentPath_.parent_path();
    }
    options.title = title.toStdString();
    options.previewCmp = document_.level().getMasterCMP();
    options.previewProjectType = document_.level().kind;
    options.templateDatabaseFallback = templateDatabaseSnapshot
        ? &*templateDatabaseSnapshot
        : nullptr;
    options.templateOverrides = templateOverridesSnapshot
        ? &*templateOverridesSnapshot
        : nullptr;
    options.allowBaseTemplates = allowBaseTemplates;
    const auto picked = ui::pickResource(
        this, resourceLocatorSnapshot_, options);
    if (!picked) {
      return std::nullopt;
    }
    return QString::fromStdString(*picked);
  };

  auto makeMaterialPreviewPixmap = [this](const QString &materialName,
                                          const int iconPx) -> QPixmap {
    const QString trimmed = materialName.trimmed();
    if (trimmed.isEmpty() ||
        trimmed.compare(QStringLiteral("none"), Qt::CaseInsensitive) == 0) {
      return {};
    }

    const std::optional<QImage> image = ui::detail::loadPreviewMaterialImage(
        &resources_, document_.level().getMasterCMP(), trimmed.toStdString());
    if (!image || image->isNull()) {
      return {};
    }

    return QPixmap::fromImage(ui::materialPreviewImageForWidget(*image).scaled(
        iconPx, iconPx, Qt::KeepAspectRatio, Qt::SmoothTransformation));
  };

  auto updateMaterialPreviewButton =
      [makeMaterialPreviewPixmap](QToolButton *button,
                                  const QString &materialName) -> void {
    if (!button) {
      return;
    }

    const QString trimmed = materialName.trimmed();
    const int iconPx = std::max(20, button->iconSize().width());
    const QPixmap pix = makeMaterialPreviewPixmap(trimmed, iconPx);
    if (!pix.isNull()) {
      button->setText(QString{});
      button->setIcon(QIcon(pix));
    } else {
      button->setIcon(QIcon{});
      if (trimmed.isEmpty() ||
          trimmed.compare(QStringLiteral("none"), Qt::CaseInsensitive) == 0) {
        button->setText(QStringLiteral("none"));
      } else {
        button->setText(QStringLiteral("mat"));
      }
    }

    button->setToolTip(
        trimmed.isEmpty()
            ? QStringLiteral("No material set. Click to pick one.")
            : QStringLiteral("Material: %1\nClick to pick material.")
                  .arg(trimmed));
  };

  struct LinkedCogRef {
    std::int32_t cogIndex{-1};
    std::int32_t valueIndex{-1};
    QString label{};
  };

  auto buildLinkedCogRefs =
      [&](const CogType targetType,
          const std::int32_t targetIndex) -> std::vector<LinkedCogRef> {
    std::vector<LinkedCogRef> refs{};
    if (targetIndex < 0) {
      return refs;
    }

    for (std::size_t cogIndex = 0; cogIndex < level.cogs.size(); ++cogIndex) {
      const Cog &cog = level.cogs[cogIndex];
      for (std::size_t valueIndex = 0; valueIndex < cog.values.size();
           ++valueIndex) {
        const CogValue &value = cog.values[valueIndex];
        if (value.cogType != targetType || value.intValue != targetIndex) {
          continue;
        }

        refs.push_back(LinkedCogRef{
            static_cast<std::int32_t>(cogIndex),
            static_cast<std::int32_t>(valueIndex),
            QString("%1\\%2")
                .arg(QString::fromStdString(cog.name))
                .arg(QString::fromStdString(value.name)),
        });
      }
    }

    return refs;
  };

  auto tryResolveSurfaceCogValueRef =
      [&](const CogValue &value) -> std::optional<SurfaceRef> {
    if (value.cogType != CogType::surface) {
      return std::nullopt;
    }

    const QString raw = QString::fromStdString(value.stringValue).trimmed();
    if (!raw.isEmpty() && raw.contains(QRegularExpression("\\s"))) {
      const QStringList parts =
          raw.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
      bool firstOk = false;
      const std::int32_t first = parts.value(0).toInt(&firstOk);
      if (firstOk && parts.size() >= 2) {
        bool secondOk = false;
        const std::int32_t second = parts.value(1).toInt(&secondOk);
        if (secondOk) {
          if (const std::optional<SurfaceRef> ref =
                  level.tryGetSurfaceRefBySectorAndSurfaceNum(first, second)) {
            return ref;
          }
        }
      }
    }

    if (value.type == ValueType::ptr && value.intValue >= 0) {
      if (const std::optional<SurfaceRef> ref =
              level.tryGetSurfaceRefByGlobalSurfaceNumber(value.intValue)) {
        return ref;
      }
    }
    if (raw.isEmpty()) {
      return std::nullopt;
    }

    const QStringList parts =
        raw.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
    bool firstOk = false;
    const std::int32_t first = parts.value(0).toInt(&firstOk);
    if (!firstOk) {
      return std::nullopt;
    }

    if (parts.size() == 1) {
      return level.tryGetSurfaceRefByGlobalSurfaceNumber(first);
    }

    bool secondOk = false;
    const std::int32_t second = parts.value(1).toInt(&secondOk);
    if (!secondOk) {
      return std::nullopt;
    }

    if (const std::optional<SurfaceRef> ref =
            level.tryGetSurfaceRefBySectorAndSurfaceNum(first, second)) {
      return ref;
    }
    return std::nullopt;
  };

  auto buildLinkedSurfaceCogRefs =
      [&](const SurfaceRef ref) -> std::vector<LinkedCogRef> {
    std::vector<LinkedCogRef> refs{};
    if (ref.sector < 0 || ref.surface < 0) {
      return refs;
    }

    for (std::size_t cogIndex = 0; cogIndex < level.cogs.size(); ++cogIndex) {
      const Cog &cog = level.cogs[cogIndex];
      for (std::size_t valueIndex = 0; valueIndex < cog.values.size();
           ++valueIndex) {
        const CogValue &value = cog.values[valueIndex];
        const std::optional<SurfaceRef> valueRef =
            tryResolveSurfaceCogValueRef(value);
        if (!valueRef || *valueRef != ref) {
          continue;
        }

        refs.push_back(LinkedCogRef{
            static_cast<std::int32_t>(cogIndex),
            static_cast<std::int32_t>(valueIndex),
            QString("%1\\%2")
                .arg(QString::fromStdString(cog.name))
                .arg(QString::fromStdString(value.name)),
        });
      }
    }

    return refs;
  };

  auto formatSurfaceCogEditorValue =
      [&](const SurfaceRef ref) -> std::optional<QString> {
    if (ref.sector < 0 || ref.surface < 0) {
      return std::nullopt;
    }

    const std::size_t sectorIndex = static_cast<std::size_t>(ref.sector);
    if (sectorIndex >= level.sectors.size()) {
      return std::nullopt;
    }

    const Sector &sector = level.sectors[sectorIndex];
    const std::size_t surfaceIndex = static_cast<std::size_t>(ref.surface);
    if (surfaceIndex >= sector.surfaces.size()) {
      return std::nullopt;
    }

    const Surface &surface = sector.surfaces[surfaceIndex];
    return QString("%1 %2").arg(sector.num).arg(surface.num);
  };

  auto cogValueEditorText = [&](const CogValue &value) -> QString {
    return QString::fromStdString(value.asJedString(level));
  };

  auto parseCogValueEditorText = [&](const Level &lvl, const CogValue &current,
                                     const QString &text,
                                     CogValue &parsed) -> bool {
    parsed = current;
    return parsed.parseJed(lvl, text.toStdString());
  };

  auto setLinkedCogList =
      [this](const std::vector<LinkedCogRef> &refs) -> void {
    if (!cogLinksPanel_ || !cogLinksList_) {
      return;
    }

    cogLinksPanel_->setVisible(true);
    cogLinksList_->clear();

    if (refs.empty()) {
      auto *item = new QListWidgetItem("NONE", cogLinksList_);
      item->setFlags(Qt::NoItemFlags);
      item->setTextAlignment(Qt::AlignCenter);
      item->setForeground(QColor(180, 184, 192));
      item->setToolTip("No linked COG variables");
      return;
    }

    for (const LinkedCogRef &ref : refs) {
      auto *item = new QListWidgetItem(ref.label, cogLinksList_);
      item->setToolTip("Open linked COG variable");
      item->setData(kPropertyCogRole, static_cast<int>(ref.cogIndex));
      item->setData(kPropertyValueIndexRole, static_cast<int>(ref.valueIndex));
    }
  };

  struct FlagDef {
    std::uint32_t bit{};
    QString name{};
    QString description{};
    bool editable{true};
  };

  using FlagToggleHandler =
      std::function<void(std::uint32_t &, std::uint32_t, bool)>;
  using FlagNormalizeHandler = std::function<void(std::uint32_t &)>;

  auto openFlagEditor =
      [this, hex](
          const QString &title, const std::uint32_t currentValue,
          std::vector<FlagDef> defs, FlagToggleHandler onToggle = {},
          FlagNormalizeHandler normalize = {}) -> std::optional<std::uint32_t> {
    QDialog dialog(this);
    dialog.setWindowTitle(title);
    dialog.setModal(true);
    dialog.resize(860, 520);
    dialog.setMinimumSize(640, 380);

    auto *layout = new QVBoxLayout(&dialog);
    layout->setContentsMargins(10, 10, 10, 10);
    layout->setSpacing(8);

    auto *info = new QLabel(
        "Toggle known flags or edit the hex value directly.", &dialog);
    info->setWordWrap(true);
    layout->addWidget(info);

    auto *filterEdit = new QLineEdit(&dialog);
    filterEdit->setObjectName("flag_filter_edit");
    filterEdit->setPlaceholderText("Search bit, flag, or description...");
    filterEdit->setClearButtonEnabled(true);
    filterEdit->setMaximumHeight(28);
    layout->addWidget(filterEdit);

    auto *list = new QTreeWidget(&dialog);
    list->setObjectName("flag_bits_list");
    list->setColumnCount(3);
    list->setHeaderLabels({"Bit", "Flag", "Description"});
    list->setRootIsDecorated(false);
    list->setUniformRowHeights(true);
    list->setAlternatingRowColors(true);
    list->header()->setStretchLastSection(true);
    list->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    list->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    list->header()->setSectionResizeMode(2, QHeaderView::Stretch);
    list->setStyleSheet("QTreeWidget#flag_bits_list {"
                        " background-color: #2b2b2b;"
                        " alternate-background-color: #303030;"
                        " border: 1px solid #202020;"
                        " border-radius: 6px;"
                        "}"
                        "QTreeWidget#flag_bits_list::item {"
                        " border-bottom: 1px solid rgba(214, 218, 224, 36);"
                        "}"
                        "QTreeWidget#flag_bits_list::item:selected {"
                        " background-color: rgba(150, 156, 164, 104);"
                        " color: #ffffff;"
                        "}"
                        "QTreeWidget#flag_bits_list::item:selected:active {"
                        " background-color: rgba(150, 156, 164, 104);"
                        " color: #ffffff;"
                        "}"
                        "QTreeWidget#flag_bits_list::item:selected:!active {"
                        " background-color: rgba(150, 156, 164, 72);"
                        " color: #ffffff;"
                        "}");
    layout->addWidget(list, 1);

    auto *hexRow = new QWidget(&dialog);
    auto *hexLayout = new QHBoxLayout(hexRow);
    hexLayout->setContentsMargins(0, 0, 0, 0);
    hexLayout->setSpacing(6);
    hexLayout->addWidget(new QLabel("Hex:", hexRow));
    auto *hexEdit = new QLineEdit(hexRow);
    hexEdit->setText(hex(currentValue));
    hexLayout->addWidget(hexEdit, 1);
    layout->addWidget(hexRow);

    auto *unknownBitsLabel = new QLabel(&dialog);
    unknownBitsLabel->setStyleSheet("color: rgba(255,255,255,160);");
    layout->addWidget(unknownBitsLabel);

    auto *buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    layout->addWidget(buttons);

    std::sort(defs.begin(), defs.end(),
              [](const FlagDef &lhs, const FlagDef &rhs) {
                if (lhs.bit != rhs.bit) {
                  return lhs.bit < rhs.bit;
                }
                return lhs.name < rhs.name;
              });

    std::uint32_t knownMask = 0u;
    for (const auto &def : defs) {
      knownMask |= def.bit;
      auto *item = new QTreeWidgetItem(list);
      item->setText(0, hex(def.bit));
      item->setText(1, ui::query::levelQueryFlagDisplayName(def.name));
      item->setText(2, def.description);
      item->setToolTip(1, def.description);
      item->setToolTip(2, def.description);
      Qt::ItemFlags flags = item->flags() | Qt::ItemIsUserCheckable;
      if (!def.editable) {
        flags &= ~Qt::ItemIsEnabled;
      }
      item->setFlags(flags);
      const bool checked = (currentValue & def.bit) != 0u;
      item->setCheckState(0, checked ? Qt::Checked : Qt::Unchecked);
      item->setData(0, Qt::UserRole,
                    QVariant::fromValue(static_cast<qulonglong>(def.bit)));
    }

    connect(filterEdit, &QLineEdit::textChanged, &dialog,
            [list](const QString &text) {
              for (int row = 0; row < list->topLevelItemCount(); ++row) {
                QTreeWidgetItem *item = list->topLevelItem(row);
                if (!item) {
                  continue;
                }

                const std::uint32_t bit = static_cast<std::uint32_t>(
                    item->data(0, Qt::UserRole).toULongLong());
                item->setHidden(!ui::query::levelQueryFlagMatchesFilter(
                    bit, item->text(1), item->text(2), text));
              }
            });

    bool syncing = false;
    std::uint32_t unknownBits = currentValue & ~knownMask;

    auto updateUnknownLabel = [&]() {
      if (unknownBits == 0u) {
        unknownBitsLabel->clear();
        unknownBitsLabel->setVisible(false);
      } else {
        unknownBitsLabel->setText(
            QString("Bits without known meaning preserved: %1")
                .arg(hex(unknownBits)));
        unknownBitsLabel->setVisible(true);
      }
    };

    auto valueFromChecks = [&]() -> std::uint32_t {
      std::uint32_t value = unknownBits;
      for (int i = 0; i < list->topLevelItemCount(); ++i) {
        QTreeWidgetItem *item = list->topLevelItem(i);
        if (!item) {
          continue;
        }

        const std::uint32_t bit = static_cast<std::uint32_t>(
            item->data(0, Qt::UserRole).toULongLong());
        if (item->checkState(0) == Qt::Checked) {
          value |= bit;
        }
      }
      return value;
    };

    auto applyValueToUi = [&](const std::uint32_t value) {
      if (syncing) {
        return;
      }
      syncing = true;

      for (int i = 0; i < list->topLevelItemCount(); ++i) {
        QTreeWidgetItem *item = list->topLevelItem(i);
        if (!item) {
          continue;
        }
        const std::uint32_t bit = static_cast<std::uint32_t>(
            item->data(0, Qt::UserRole).toULongLong());
        item->setCheckState(0,
                            (value & bit) != 0u ? Qt::Checked : Qt::Unchecked);
      }

      unknownBits = value & ~knownMask;
      hexEdit->setText(hex(value));
      updateUnknownLabel();
      syncing = false;
    };

    auto syncChecksFromHex = [&]() {
      if (syncing) {
        return;
      }
      const std::optional<std::uint32_t> parsed =
          sed::text::parseHex(hexEdit->text().trimmed().toStdString());
      if (!parsed) {
        return;
      }

      std::uint32_t value = *parsed;
      if (normalize) {
        normalize(value);
      }
      applyValueToUi(value);
    };

    updateUnknownLabel();

    connect(list, &QTreeWidget::itemChanged, &dialog,
            [&, onToggle, normalize](QTreeWidgetItem *item, int column) {
              if (!item || column != 0 || syncing) {
                return;
              }

              std::uint32_t value = valueFromChecks();
              if (onToggle) {
                const std::uint32_t bit = static_cast<std::uint32_t>(
                    item->data(0, Qt::UserRole).toULongLong());
                onToggle(value, bit, item->checkState(0) == Qt::Checked);
              }
              if (normalize) {
                normalize(value);
              }
              applyValueToUi(value);
            });
    connect(hexEdit, &QLineEdit::editingFinished, &dialog,
            [syncChecksFromHex]() { syncChecksFromHex(); });

    std::optional<std::uint32_t> acceptedValue{};
    connect(buttons, &QDialogButtonBox::accepted, &dialog, [&]() {
      syncChecksFromHex();
      const std::optional<std::uint32_t> parsed =
          sed::text::parseHex(hexEdit->text().trimmed().toStdString());
      if (!parsed) {
        QMessageBox::information(
            &dialog, "Invalid flags",
            "Enter a valid hex value (for example: 0x20).");
        return;
      }
      acceptedValue = *parsed;
      dialog.accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, &dialog,
            [&]() { dialog.reject(); });

    if (dialog.exec() != QDialog::Accepted) {
      return std::nullopt;
    }
    return acceptedValue;
  };

  if (!document_.selection().sectors.empty()) {
    const auto &selectedSectors = document_.selection().sectors;
    std::int32_t sectorIndex =
        *std::min_element(selectedSectors.begin(), selectedSectors.end());
    if (glView_ && selectedSectors.contains(glView_->activeSectorIndex())) {
      sectorIndex = glView_->activeSectorIndex();
    }

    auto addChild = [&](QTreeWidgetItem *parent, const QString &key,
                        const QString &value = {}) -> QTreeWidgetItem * {
      const int parentIndent =
          parent ? parent->data(0, kPropertyIndentRole).toInt() : -1;
      const int indent = parent ? (parentIndent + 1) : 0;

      auto *item = new QTreeWidgetItem(properties_);
      item->setText(0, QString(indent * 2, QChar(' ')) + key);
      item->setText(1, value);
      item->setData(0, kPropertyIndentRole, indent);
      return item;
    };

    auto addGroup = [&](QTreeWidgetItem *parent,
                        const QString &title) -> QTreeWidgetItem * {
      const int parentIndent =
          parent ? parent->data(0, kPropertyIndentRole).toInt() : -1;
      const int indent = parent ? (parentIndent + 1) : 0;

      auto *item = new QTreeWidgetItem(properties_);
      item->setText(0, QString(indent * 2, QChar(' ')) + title);
      item->setFirstColumnSpanned(true);
      item->setChildIndicatorPolicy(
          QTreeWidgetItem::DontShowIndicatorWhenChildless);
      item->setFlags(item->flags() & ~Qt::ItemIsSelectable);
      item->setData(0, kPropertyIndentRole, indent);
      QFont f = item->font(0);
      f.setBold(true);
      item->setFont(0, f);
      item->setExpanded(true);
      return item;
    };

    auto *root = addGroup(nullptr, "General");

    auto colorToQ = [](const ColorF &c, const bool alpha) -> QColor {
      const float a = alpha ? c.a : 1.0f;
      return QColor::fromRgbF(std::clamp(static_cast<double>(c.r), 0.0, 1.0),
                              std::clamp(static_cast<double>(c.g), 0.0, 1.0),
                              std::clamp(static_cast<double>(c.b), 0.0, 1.0),
                              std::clamp(static_cast<double>(a), 0.0, 1.0));
    };

    auto qToColor = [](const QColor &qc) -> ColorF {
      return ColorF{
          clamp01(static_cast<float>(qc.redF())),
          clamp01(static_cast<float>(qc.greenF())),
          clamp01(static_cast<float>(qc.blueF())),
          clamp01(static_cast<float>(qc.alphaF())),
      };
    };

    auto setSwatch = [](QPushButton *button, const QColor &c) {
      ui::configureColorSwatchButton(button, c);
    };

    auto applyUndoableNoNotify =
        [this, queueRebuild](const std::string &label,
                             const editor::DocumentDirty dirty,
                             auto &&mutate) -> bool {
      if (!documentEditingAllowed()) {
        return false;
      }
      if (selectionTouchesNonEditableLayer(document_.selection())) {
        return false;
      }
      const std::string effectiveLabel =
          ui::undo::relabelForSelection(label, document_.selection());
      if (!document_.applyUndoable(effectiveLabel, dirty,
                                   std::forward<decltype(mutate)>(mutate))) {
        return false;
      }

      updateEditActions();
      scheduleRebuildChecks(dirty);
      queueRebuild();
      return true;
    };

    if (sectorIndex < 0 ||
        static_cast<std::size_t>(sectorIndex) >= level.sectors.size()) {
      return;
    }

    const Sector &sector = level.sectors[static_cast<std::size_t>(sectorIndex)];

    // Flags (editable, hex)
    {
      auto *item = addChild(root, "Flags");
      auto *row = new QWidget(properties_);
      auto *rowLayout = new QHBoxLayout(row);
      rowLayout->setContentsMargins(0, 0, 0, 0);
      rowLayout->setSpacing(4);

      auto *edit = new QLineEdit(row);
      edit->setText(hex(static_cast<std::uint32_t>(sector.flags)));
      rowLayout->addWidget(edit, 1);

      auto *pickBtn = new QToolButton(row);
      pickBtn->setText("...");
      pickBtn->setToolTip("Edit flags");
      pickBtn->setAutoRaise(true);
      rowLayout->addWidget(pickBtn);
      openAuxiliaryEditorOnDoubleClick(edit, pickBtn);

      properties_->setItemWidget(item, 1, row);
      connect(
          edit, &QLineEdit::editingFinished, this,
          [this, edit, sectorIndex, hex, applyUndoable]() {
            const Level &lvl = document_.level();
            if (sectorIndex < 0 ||
                static_cast<std::size_t>(sectorIndex) >= lvl.sectors.size()) {
              return;
            }

            const Sector &sec =
                lvl.sectors[static_cast<std::size_t>(sectorIndex)];
            const QString cur = hex(static_cast<std::uint32_t>(sec.flags));
            const QString next = edit->text().trimmed();
            if (next == cur) {
              return;
            }

            const std::optional<std::uint32_t> parsed =
                sed::text::parseHex(next.toStdString());
            if (!parsed) {
              edit->setText(cur);
              return;
            }

            (void)applyUndoable(
                "Sector: Flags changed", editor::DocumentDirty::LevelGeometry,
                [&](editor::DocumentState &state) -> bool {
                  const std::int32_t flags = static_cast<std::int32_t>(*parsed);

                  bool any = false;
                  if (!state.selection.sectors.empty()) {
                    for (const std::int32_t idx : state.selection.sectors) {
                      if (idx < 0 || static_cast<std::size_t>(idx) >=
                                         state.level.sectors.size()) {
                        continue;
                      }

                      Sector &s =
                          state.level.sectors[static_cast<std::size_t>(idx)];
                      if (s.flags != flags) {
                        s.flags = flags;
                        any = true;
                      }
                    }
                    return any;
                  }

                  if (sectorIndex < 0 ||
                      static_cast<std::size_t>(sectorIndex) >=
                          state.level.sectors.size()) {
                    return false;
                  }

                  Sector &s =
                      state.level
                          .sectors[static_cast<std::size_t>(sectorIndex)];
                  if (s.flags != flags) {
                    s.flags = flags;
                    return true;
                  }
                  return false;
                });
          });

      connect(
          pickBtn, &QToolButton::clicked, this,
          [this, edit, openFlagEditor,
           currentFlags = static_cast<std::uint32_t>(sector.flags),
           levelKind = level.kind]() {
            if (!edit) {
              return;
            }

            std::vector<FlagDef> defs{
                {0x01u, "No gravity", "Sector has no gravity."},
                {0x02u, "Underwater", "Sector is underwater."},
                {0x04u, "COG linked", "Sector is linked to COG logic."},
                {0x08u, "Use thrust", "Sector has thrust behavior enabled."},
                {0x10u, "Hide on map", "Do not show sector on the map."},
                {0x20u, "No AI actor", "AI actors cannot enter this sector."},
                {0x40u, "Fall death",
                 "Player is instantly killed when falling into this sector."},
                {0x80u, "Adjoins off",
                 "All sector adjoins are not rendered. Engine flag.", false},
                {0x1000u, "Collide box",
                 "Sector has collision box data. Usually auto-set."},
                {0x4000u, "Seen",
                 "Sector has been seen by the camera. Engine flag."},
                {static_cast<std::uint32_t>(sed::SECF_NOAMBINETLIGHT),
                 "No ambient light",
                 "Skip ambient light contribution during baked light "
                 "calculation."},
                {static_cast<std::uint32_t>(sed::SECF_3DO), "Preview as 3DO",
                 "Show the sector inverted in preview. SED/editor flag."},
            };
            if (levelKind == ProjectType::IJIM) {
              defs.push_back(
                  {static_cast<std::uint32_t>(sed::SECF_IJIM_Aetherim),
                   "Aetherium", "Sector is part of the aetherium."});
              defs.push_back(
                  {static_cast<std::uint32_t>(sed::SECF_NORGBAMBINETLIGHT),
                   "No RGB ambient light",
                   "Calculate only ambient light intensity when baking "
                   "lights."});
            }

            const auto picked =
                openFlagEditor("Sector: Flags changed", currentFlags, defs);
            if (!picked) {
              return;
            }

            edit->setText(QString("0x%1").arg(*picked, 0, 16));
            QMetaObject::invokeMethod(edit, "editingFinished",
                                      Qt::DirectConnection);
          });
    }

    addChild(root, "Surfaces", QString::number(sector.surfaces.size()));
    {
      const int layerCount = std::max(1, static_cast<int>(level.layers.size()));
      const int currentLayer = std::clamp(sector.layer, 0, layerCount - 1);

      auto *item = addChild(root, "Layer");
      auto *combo = new QComboBox(properties_);
      combo->setObjectName(QStringLiteral("prop.sector.layer"));
      for (int i = 0; i < layerCount; ++i) {
        QString layerName = QString("Layer%1").arg(i);
        if (static_cast<std::size_t>(i) < level.layers.size()) {
          const std::string &raw = level.layers[static_cast<std::size_t>(i)];
          if (!raw.empty()) {
            layerName = QString::fromStdString(raw);
          }
        }
        combo->addItem(QString("%1: %2").arg(i).arg(layerName), i);
      }
      combo->setCurrentIndex(currentLayer);
      properties_->setItemWidget(item, 1, combo);

      connect(
          combo, qOverload<int>(&QComboBox::currentIndexChanged), this,
          [this, combo, sectorIndex, applyUndoable](int row) {
            if (!combo || row < 0) {
              return;
            }
            const int newLayer = combo->itemData(row).toInt();
            if (applyUndoable(
                    "Sector: Layer changed",
                    editor::DocumentDirty::LevelGeometry,
                    [sectorIndex,
                     newLayer](editor::DocumentState &state) -> bool {
                      auto applyOne = [newLayer](Sector &sec) -> bool {
                        if (sec.layer == newLayer) {
                          return false;
                        }
                        sec.layer = newLayer;
                        return true;
                      };

                      bool any = false;
                      if (!state.selection.sectors.empty()) {
                        for (const std::int32_t idx : state.selection.sectors) {
                          if (idx < 0 || static_cast<std::size_t>(idx) >=
                                             state.level.sectors.size()) {
                            continue;
                          }
                          any |= applyOne(
                              state.level
                                  .sectors[static_cast<std::size_t>(idx)]);
                        }
                        return any;
                      }

                      if (sectorIndex < 0 ||
                          static_cast<std::size_t>(sectorIndex) >=
                              state.level.sectors.size()) {
                        return false;
                      }

                      return applyOne(
                          state.level
                              .sectors[static_cast<std::size_t>(sectorIndex)]);
                    },
                    true)) {
              rebuildOutliner();
            }
          });
    }

    // Lighting
    {
      auto *lighting = addGroup(nullptr, "Lighting");
      const bool isIJIM = level.kind == ProjectType::IJIM;

      auto calculateAmbientFromSurfaceLighting = [this, sectorIndex,
                                                  applyUndoableNoNotify]() {
        const bool changed = applyUndoableNoNotify(
            "Calculate sector ambient from surface colors",
            editor::DocumentDirty::LevelGeometry |
                editor::DocumentDirty::Things,
            [sectorIndex](editor::DocumentState &state) -> bool {
              std::vector<std::int32_t> targets;
              if (!state.selection.sectors.empty()) {
                targets.reserve(state.selection.sectors.size());
                for (const std::int32_t idx : state.selection.sectors) {
                  if (idx >= 0 && static_cast<std::size_t>(idx) <
                                      state.level.sectors.size()) {
                    targets.push_back(idx);
                  }
                }
              } else if (sectorIndex >= 0 &&
                         static_cast<std::size_t>(sectorIndex) <
                             state.level.sectors.size()) {
                targets.push_back(sectorIndex);
              }

              if (targets.empty()) {
                return false;
              }

              struct AmbientInputSnapshot {
                ColorF ambient{};
                std::vector<ColorF> vertexColors{};
              };

              std::vector<AmbientInputSnapshot> before;
              before.reserve(targets.size());
              for (const std::int32_t idx : targets) {
                const Sector &selectedSector =
                    state.level.sectors[static_cast<std::size_t>(idx)];
                AmbientInputSnapshot snapshot{};
                snapshot.ambient = selectedSector.ambient;
                for (const Surface &surface : selectedSector.surfaces) {
                  for (const TexVertex &texVertex : surface.texVertices) {
                    snapshot.vertexColors.push_back(texVertex.color);
                  }
                }
                before.push_back(std::move(snapshot));
              }

              ::sed::level::levelCalcSectorAmbients(state.level, targets);

              auto colorsDiffer = [](const ColorF &a, const ColorF &b) -> bool {
                return a.r != b.r || a.g != b.g || a.b != b.b || a.a != b.a;
              };

              for (std::size_t target = 0; target < targets.size(); ++target) {
                const Sector &selectedSector =
                    state.level
                        .sectors[static_cast<std::size_t>(targets[target])];
                const AmbientInputSnapshot &snapshot = before[target];
                if (colorsDiffer(selectedSector.ambient, snapshot.ambient)) {
                  return true;
                }

                std::size_t colorIndex = 0;
                for (const Surface &surface : selectedSector.surfaces) {
                  for (const TexVertex &texVertex : surface.texVertices) {
                    if (colorIndex >= snapshot.vertexColors.size() ||
                        colorsDiffer(texVertex.color,
                                     snapshot.vertexColors[colorIndex])) {
                      return true;
                    }
                    ++colorIndex;
                  }
                }
                if (colorIndex != snapshot.vertexColors.size()) {
                  return true;
                }
              }

              return false;
            });

        if (changed && glView_) {
          // This command changes two independent render inputs:
          // normalized baked surface-vertex colors and sector
          // ambient used by Things. A LevelOnly rebuild followed by
          // the Thing fast-update can initialize its light cache to
          // the already-changed value and therefore miss the color
          // delta. Rebuild both caches together so the click is
          // visible before the next unrelated viewport edit.
          glView_->notifyAllMeshesChanged();
        }
      };

      auto makeAmbientFromSurfacesButton =
          [this, calculateAmbientFromSurfaceLighting](
              QWidget *parent) -> QToolButton * {
        auto *button = new QToolButton(parent);
        button->setObjectName(
            QStringLiteral("sectorAmbientFromSurfacesButton"));
        button->setAutoRaise(true);
        button->setIcon(
            ui::makeViewportIcon(ui::ViewportIcon::lightingGouraud));
        button->setIconSize(QSize(16, 16));
        button->setFixedWidth(24);
        button->setToolTip(
            QStringLiteral("Calculate ambient from surface colors\n"
                           "Uses the brighter average of baked vertex colors "
                           "and surface extra light."));
        button->setAccessibleName(
            QStringLiteral("Calculate ambient from surface colors"));
        connect(button, &QToolButton::clicked, this,
                calculateAmbientFromSurfaceLighting);
        return button;
      };

      if (isIJIM) {
        auto addColorRow = [&](const QString &key,
                               const std::function<ColorF(const Sector &)> &get,
                               const std::function<void(Sector &,
                                                        const ColorF &)> &set,
                               const std::string &undoLabel,
                               const editor::DocumentDirty dirty,
                               const bool showAmbientCalculation = false) {
          auto *item = addChild(lighting, key);
          auto *row = new QWidget(properties_);
          auto *rowLayout = new QHBoxLayout(row);
          rowLayout->setContentsMargins(0, 0, 0, 0);
          rowLayout->setSpacing(4);

          auto *button = new QPushButton(row);
          setSwatch(button, colorToQ(get(sector), false));
          rowLayout->addWidget(button, 1);
          if (showAmbientCalculation) {
            rowLayout->addWidget(makeAmbientFromSurfacesButton(row));
          }
          properties_->setItemWidget(item, 1, row);
          connectLiveColor(
              button, colorToQ(get(sector), false), false, undoLabel, dirty,
              [sectorIndex, get, set, qToColor](editor::DocumentState &state,
                                                const QColor &color) -> bool {
                const ColorF chosen = qToColor(color);
                auto applyOne = [&](Sector &s) -> bool {
                  const ColorF before = get(s);
                  set(s, chosen);
                  const ColorF after = get(s);
                  return before.r != after.r || before.g != after.g ||
                         before.b != after.b || before.a != after.a;
                };

                bool any = false;
                if (!state.selection.sectors.empty()) {
                  for (const std::int32_t idx : state.selection.sectors) {
                    if (idx < 0 || static_cast<std::size_t>(idx) >=
                                       state.level.sectors.size()) {
                      continue;
                    }
                    any |= applyOne(
                        state.level.sectors[static_cast<std::size_t>(idx)]);
                  }
                  return any;
                }

                if (sectorIndex < 0 || static_cast<std::size_t>(sectorIndex) >=
                                           state.level.sectors.size()) {
                  return false;
                }

                return applyOne(
                    state.level.sectors[static_cast<std::size_t>(sectorIndex)]);
              });
        };

        addColorRow(
            "Ambient", [](const Sector &s) { return s.ambient; },
            [](Sector &s, const ColorF &c) {
              s.ambient.r = c.r;
              s.ambient.g = c.g;
              s.ambient.b = c.b;
            },
            "Sector: Ambient changed",
            editor::DocumentDirty::LevelGeometry |
                editor::DocumentDirty::Things,
            true);

        addColorRow(
            "Extra", [](const Sector &s) { return s.extraLight; },
            [](Sector &s, const ColorF &c) {
              s.extraLight.r = c.r;
              s.extraLight.g = c.g;
              s.extraLight.b = c.b;
            },
            "Sector: Extra light changed",
            editor::DocumentDirty::LevelGeometry |
                editor::DocumentDirty::Things);

        // IJIM: Point light section
        const bool hasPointLight =
            pointLightHasAuthoredParameters(sector.pointLight);
        if (!hasPointLight) {
          // A newly added light starts linked even if the previously
          // removed light had been edited in unlinked mode.
          properties_->setProperty("sed.sectorPointLightRangesLinked", true);
          // Keep the absent state deliberately flat: one full-width
          // action, without a dormant Point Light field hierarchy.
          auto *item = addChild(lighting, {});
          item->setFirstColumnSpanned(true);
          auto *button = new QToolButton(properties_);
          button->setObjectName(QStringLiteral("prop.sector.point_light.add"));
          button->setText(QStringLiteral("Add Point Light"));
          button->setIcon(ui::makeViewportIcon(ui::ViewportIcon::lightSector));
          button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
          button->setSizePolicy(QSizePolicy::MinimumExpanding,
                                QSizePolicy::Fixed);
          button->setToolTip(QStringLiteral("Adds sector point light"));
          properties_->setItemWidget(item, 0, button);
          connect(button, &QToolButton::clicked, this,
                  [this, button, sectorIndex]() {
                    // placeSectorLightInSector rebuilds this panel. Do
                    // that after QAbstractButton has finished emitting
                    // clicked, and prevent a rapid second activation
                    // while the structural state transition is queued.
                    button->setEnabled(false);
                    QMetaObject::invokeMethod(
                        this,
                        [this, sectorIndex]() {
                          if (!placeSectorLightInSector(sectorIndex)) {
                            rebuildProperties();
                          }
                        },
                        Qt::QueuedConnection);
                  });
        } else {
          auto *pointLight = addChild(lighting, "Point Light");
          {
            auto *row = new QWidget(properties_);
            auto *layout = new QHBoxLayout(row);
            layout->setContentsMargins(0, 0, 0, 0);
            layout->setSpacing(2);

            auto makeAction =
                [row, layout](const QString &objectName, const QIcon &icon,
                              const QString &accessibleName,
                              const QString &toolTip) -> QToolButton * {
              auto *button = new QToolButton(row);
              button->setObjectName(objectName);
              button->setIcon(icon);
              button->setIconSize(QSize(18, 18));
              button->setAccessibleName(accessibleName);
              button->setToolTip(toolTip);
              button->setAutoRaise(true);
              button->setToolButtonStyle(Qt::ToolButtonIconOnly);
              button->setStyleSheet(
                  QStringLiteral("QToolButton { border: 0; background: "
                                 "transparent; padding: 1px; }"));
              button->setFixedSize(24, 24);
              layout->addWidget(button);
              return button;
            };

            QToolButton *removeButton = makeAction(
                QStringLiteral("prop.sector.point_light.remove"),
                ui::makeViewportIcon(ui::ViewportIcon::actionDelete),
                QStringLiteral("Remove sector point light"),
                QStringLiteral("Remove sector point light.\nNote, removes "
                               "point light from every selected sector."));
            QToolButton *copyButton = makeAction(
                QStringLiteral("prop.sector.point_light.copy"),
                ui::makeViewportIcon(ui::ViewportIcon::actionCopy),
                QStringLiteral("Copy sector point light"),
                QStringLiteral("Copy this sector's point-light parameters."));
            QToolButton *pasteButton =
                makeAction(QStringLiteral("prop.sector.point_light.paste"),
                           ui::makeViewportIcon(ui::ViewportIcon::actionPaste),
                           QStringLiteral("Paste sector point light"),
                           QStringLiteral("Paste point-light parameters into "
                                          "every selected unlocked sector."));
            layout->addStretch(1);
            properties_->setItemWidget(pointLight, 1, row);

            connect(removeButton, &QToolButton::clicked, this,
                    [this, row, sectorIndex, notifyDirty]() {
                      if (!documentEditingAllowed()) {
                        return;
                      }
                      // Avoid a model/UI split where the light is already
                      // removed but the stale action row can still be
                      // activated. Perform the mutation after the button
                      // click returns, then rebuild the absent state in
                      // that same queued transition.
                      row->setEnabled(false);
                      QMetaObject::invokeMethod(
                          this,
                          [this, sectorIndex, notifyDirty]() {
                            if (!documentEditingAllowed()) {
                              return;
                            }
                            constexpr editor::DocumentDirty dirty =
                                editor::DocumentDirty::LevelGeometry |
                                editor::DocumentDirty::Things;
                            const bool applied = document_.applyUndoable(
                                "Sector: Point light removed", dirty,
                                [this, sectorIndex](
                                    editor::DocumentState &state) -> bool {
                                  const auto removeOne =
                                      [this, &state](
                                          const std::int32_t index) -> bool {
                                    if (index < 0 ||
                                        static_cast<std::size_t>(index) >=
                                            state.level.sectors.size()) {
                                      return false;
                                    }
                                    Sector &target =
                                        state.level
                                            .sectors[static_cast<std::size_t>(
                                                index)];
                                    if (isLayerLocked(target.layer) ||
                                        !pointLightHasAuthoredParameters(
                                            target.pointLight)) {
                                      return false;
                                    }
                                    target.pointLight = PointLight{};
                                    return true;
                                  };

                                  bool changed = false;
                                  if (!state.selection.sectors.empty()) {
                                    for (const std::int32_t index :
                                         state.selection.sectors) {
                                      changed = removeOne(index) || changed;
                                    }
                                  } else {
                                    changed = removeOne(sectorIndex);
                                  }
                                  return changed;
                                });
                            if (applied) {
                              notifyDirty(dirty);
                              updateEditActions();
                            }
                            rebuildProperties();
                          },
                          Qt::QueuedConnection);
                    });

            connect(
                copyButton, &QToolButton::clicked, this,
                [this, sectorIndex, copyButton, showPropertyActionFeedback]() {
                  const Level &currentLevel = document_.level();
                  if (sectorIndex < 0 ||
                      static_cast<std::size_t>(sectorIndex) >=
                          currentLevel.sectors.size()) {
                    return;
                  }

                  const PointLight &light =
                      currentLevel
                          .sectors[static_cast<std::size_t>(sectorIndex)]
                          .pointLight;
                  const QLocale c = QLocale::c();
                  QApplication::clipboard()->setText(
                      QStringLiteral(
                          "SED sector point light: %1 %2 %3 %4 %5 %6 %7 %8")
                          .arg(c.toString(light.color.r, 'g', 9),
                               c.toString(light.color.g, 'g', 9),
                               c.toString(light.color.b, 'g', 9),
                               c.toString(light.position.x, 'g', 17),
                               c.toString(light.position.y, 'g', 17),
                               c.toString(light.position.z, 'g', 17),
                               c.toString(light.minRange, 'g', 17),
                               c.toString(light.maxRange, 'g', 17)));
                  showPropertyActionFeedback(copyButton,
                                             QStringLiteral("Copied"));
                });

            connect(
                pasteButton, &QToolButton::clicked, this,
                [this, sectorIndex, pasteButton, applyUndoable,
                 showPropertyActionFeedback]() {
                  QString raw = QApplication::clipboard()->text().trimmed();
                  if (raw.isEmpty()) {
                    showPropertyActionFeedback(
                        pasteButton, QStringLiteral("Nothing to paste"), true);
                    return;
                  }
                  const std::optional<PointLight> parsed =
                      ui::properties::detail::parseSectorPointLightClipboard(raw);
                  if (!parsed) {
                    showPropertyActionFeedback(
                        pasteButton,
                        QStringLiteral("Point-light data has the wrong format"),
                        true);
                    return;
                  }
                  const PointLight pasted = *parsed;

                  // Applying an identical, valid payload is a
                  // successful paste even though it correctly does
                  // not create an empty undo step.
                  const editor::DocumentState &before = document_.current();
                  const auto isEditableTarget =
                      [this, &before](const std::int32_t index) {
                        return index >= 0 &&
                               static_cast<std::size_t>(index) <
                                   before.level.sectors.size() &&
                               !isLayerLocked(
                                   before.level
                                       .sectors[static_cast<std::size_t>(index)]
                                       .layer);
                      };
                  bool hasEditableTarget = false;
                  if (!before.selection.sectors.empty()) {
                    hasEditableTarget = std::ranges::any_of(
                        before.selection.sectors, isEditableTarget);
                  } else {
                    hasEditableTarget = isEditableTarget(sectorIndex);
                  }
                  const bool applied = applyUndoable(
                      "Sector: Point light pasted",
                      editor::DocumentDirty::LevelGeometry |
                          editor::DocumentDirty::Things,
                      [this, sectorIndex,
                       pasted](editor::DocumentState &state) -> bool {
                        const auto pasteOne =
                            [this, &state,
                             &pasted](const std::int32_t index) -> bool {
                          if (index < 0 || static_cast<std::size_t>(index) >=
                                               state.level.sectors.size()) {
                            return false;
                          }
                          Sector &target =
                              state.level
                                  .sectors[static_cast<std::size_t>(index)];
                          if (isLayerLocked(target.layer)) {
                            return false;
                          }
                          if (target.pointLight.color == pasted.color &&
                              target.pointLight.position == pasted.position &&
                              target.pointLight.minRange == pasted.minRange &&
                              target.pointLight.maxRange == pasted.maxRange) {
                            return false;
                          }
                          target.pointLight = pasted;
                          return true;
                        };

                        bool changed = false;
                        if (!state.selection.sectors.empty()) {
                          for (const std::int32_t index :
                               state.selection.sectors) {
                            changed = pasteOne(index) || changed;
                          }
                        } else {
                          changed = pasteOne(sectorIndex);
                        }
                        return changed;
                      });
                  showPropertyActionFeedback(
                      pasteButton,
                      applied || hasEditableTarget
                          ? QStringLiteral("Pasted")
                          : QStringLiteral("Could not paste to the selection"),
                      !applied && !hasEditableTarget);
                });
          }

          // IJIM sector-point-light parameters are also packed into the
          // 3DO Thing vertices, so every edit must refresh both meshes.

          {
            auto *item = addChild(pointLight, "Color");
            auto *button = new QPushButton(properties_);
            button->setObjectName(
                QStringLiteral("prop.sector.point_light.color"));
            setSwatch(button, colorToQ(sector.pointLight.color, false));
            properties_->setItemWidget(item, 1, button);
            connectLiveColor(
                button, colorToQ(sector.pointLight.color, false), false,
                "Sector: Point light color changed",
                editor::DocumentDirty::LevelGeometry |
                    editor::DocumentDirty::Things,
                [sectorIndex, qToColor](editor::DocumentState &state,
                                        const QColor &color) -> bool {
                  const ColorF chosen = qToColor(color);
                  auto applyOne = [&](Sector &s) -> bool {
                    const ColorF before = s.pointLight.color;
                    s.pointLight.color = chosen;
                    const ColorF after = s.pointLight.color;
                    return before.r != after.r || before.g != after.g ||
                           before.b != after.b || before.a != after.a;
                  };

                  bool any = false;
                  if (!state.selection.sectors.empty()) {
                    for (const std::int32_t idx : state.selection.sectors) {
                      if (idx < 0 || static_cast<std::size_t>(idx) >=
                                         state.level.sectors.size()) {
                        continue;
                      }
                      any |= applyOne(
                          state.level.sectors[static_cast<std::size_t>(idx)]);
                    }
                    return any;
                  }

                  if (sectorIndex < 0 ||
                      static_cast<std::size_t>(sectorIndex) >=
                          state.level.sectors.size()) {
                    return false;
                  }

                  return applyOne(
                      state.level
                          .sectors[static_cast<std::size_t>(sectorIndex)]);
                });
          }

          auto addPointFloat = [&](const QString &key,
                                   const std::function<double(const Sector &)>
                                       &get,
                                   const std::function<void(Sector &, double)>
                                       &set,
                                   const std::string &undoLabel) {
            auto *item = addChild(pointLight, key);
            auto *scrub = new ui::ScrubFloatEdit(properties_);
            QString objectKey = key.toLower();
            objectKey.replace(QChar(' '), QChar('_'));
            scrub->setObjectName(
                QStringLiteral("prop.sector.point_light.%1").arg(objectKey));
            scrub->setDecimals(6);
            scrub->setSingleStep(0.01);
            scrub->setValue(get(sector));
            properties_->setItemWidget(item, 1, scrub);
            auto onCommit = [this, sectorIndex, get, set, undoLabel,
                             applyUndoable](double v) {
              (void)applyUndoable(
                  undoLabel,
                  editor::DocumentDirty::LevelGeometry |
                      editor::DocumentDirty::Things,
                  [&](editor::DocumentState &state) -> bool {
                    auto applyOne = [&](Sector &s) -> bool {
                      const PointLight before = s.pointLight;
                      set(s, v);
                      return before.color != s.pointLight.color ||
                             before.position != s.pointLight.position ||
                             before.minRange != s.pointLight.minRange ||
                             before.maxRange != s.pointLight.maxRange;
                    };

                    bool any = false;
                    if (!state.selection.sectors.empty()) {
                      for (const std::int32_t idx : state.selection.sectors) {
                        if (idx < 0 || static_cast<std::size_t>(idx) >=
                                           state.level.sectors.size()) {
                          continue;
                        }
                        any |= applyOne(
                            state.level.sectors[static_cast<std::size_t>(idx)]);
                      }
                      return any;
                    }

                    if (sectorIndex < 0 ||
                        static_cast<std::size_t>(sectorIndex) >=
                            state.level.sectors.size()) {
                      return false;
                    }

                    return applyOne(
                        state.level
                            .sectors[static_cast<std::size_t>(sectorIndex)]);
                  });
            };
            connect(scrub, &ui::ScrubFloatEdit::valueCommitted, this, onCommit);
            connect(scrub, &ui::ScrubFloatEdit::scrubFinished, this, onCommit);
          };

          addPointFloat(
              "Pos X", [](const Sector &s) { return s.pointLight.position.x; },
              [](Sector &s, const double v) { s.pointLight.position.x = v; },
              "Sector: Point light position changed");
          addPointFloat(
              "Pos Y", [](const Sector &s) { return s.pointLight.position.y; },
              [](Sector &s, const double v) { s.pointLight.position.y = v; },
              "Sector: Point light position changed");
          addPointFloat(
              "Pos Z", [](const Sector &s) { return s.pointLight.position.z; },
              [](Sector &s, const double v) { s.pointLight.position.z = v; },
              "Sector: Point light position changed");

          const QVariant linkedProperty =
              properties_->property("sed.sectorPointLightRangesLinked");
          const bool rangesLinked =
              !linkedProperty.isValid() || linkedProperty.toBool();
          if (!linkedProperty.isValid()) {
            properties_->setProperty("sed.sectorPointLightRangesLinked", true);
          }

          auto *rangeItem = addChild(pointLight, "Ranges");
          auto *rangeRow = new QWidget(properties_);
          auto *rangeLayout = new QHBoxLayout(rangeRow);
          rangeLayout->setContentsMargins(0, 0, 0, 0);
          rangeLayout->setSpacing(4);

          auto *rangeEditors = new QWidget(rangeRow);
          auto *rangeEditorsLayout = new QGridLayout(rangeEditors);
          rangeEditorsLayout->setContentsMargins(0, 0, 0, 0);
          rangeEditorsLayout->setHorizontalSpacing(5);
          rangeEditorsLayout->setVerticalSpacing(3);

          auto *minLabel = new QLabel(QStringLiteral("Min"), rangeRow);
          auto *minRange = new ui::ScrubFloatEdit(rangeRow);
          minRange->setObjectName(
              QStringLiteral("prop.sector.point_light.min_range"));
          minRange->setDecimals(6);
          minRange->setSingleStep(0.01);
          minRange->setRange(
              0.0, static_cast<double>(std::numeric_limits<float>::max()));
          minRange->setValue(sector.pointLight.minRange);
          minRange->setToolTip(
              QStringLiteral("Outer point-light falloff range."));

          auto *rangeLinkButton = new QToolButton(rangeRow);
          rangeLinkButton->setObjectName(
              QStringLiteral("prop.sector.point_light.link_ranges"));
          rangeLinkButton->setAccessibleName(
              QStringLiteral("Link point-light ranges"));
          rangeLinkButton->setCheckable(true);
          rangeLinkButton->setChecked(rangesLinked);
          rangeLinkButton->setAutoRaise(true);
          rangeLinkButton->setToolButtonStyle(Qt::ToolButtonIconOnly);
          rangeLinkButton->setIconSize(QSize(18, 18));
          rangeLinkButton->setFixedSize(24, 48);
          rangeLinkButton->setStyleSheet(
              QStringLiteral("QToolButton { border: 0; background: "
                             "transparent; padding: 0; }"));

          auto *maxLabel = new QLabel(QStringLiteral("Max"), rangeRow);
          auto *maxRange = new ui::ScrubFloatEdit(rangeRow);
          maxRange->setObjectName(
              QStringLiteral("prop.sector.point_light.max_range"));
          maxRange->setDecimals(6);
          maxRange->setSingleStep(0.01);
          maxRange->setRange(
              0.0, static_cast<double>(std::numeric_limits<float>::max()));
          maxRange->setValue(sector.pointLight.maxRange);
          maxRange->setToolTip(
              QStringLiteral("Inner point-light full-strength range."));

          minLabel->setMinimumWidth(std::max(minLabel->sizeHint().width(),
                                             maxLabel->sizeHint().width()));
          rangeEditorsLayout->addWidget(minLabel, 0, 0);
          rangeEditorsLayout->addWidget(minRange, 0, 1);
          rangeEditorsLayout->addWidget(maxLabel, 1, 0);
          rangeEditorsLayout->addWidget(maxRange, 1, 1);
          rangeEditorsLayout->setColumnStretch(1, 1);
          rangeLayout->addWidget(rangeEditors, 1);
          rangeLayout->addWidget(rangeLinkButton, 0, Qt::AlignVCenter);
          properties_->setItemWidget(rangeItem, 1, rangeRow);

          const auto updateRangeLinkAppearance = [](QToolButton *button,
                                                    const bool linked) {
            if (!button) {
              return;
            }
            button->setIcon(
                ui::makeViewportIcon(linked ? ui::ViewportIcon::rangeLinked
                                            : ui::ViewportIcon::rangeUnlinked));
            button->setToolTip(
                linked ? QStringLiteral("Ranges linked: editing either "
                                        "preserves the current min:max ratio.")
                       : QStringLiteral("Ranges unlinked: edit minimum and "
                                        "maximum independently."));
          };
          updateRangeLinkAppearance(rangeLinkButton, rangesLinked);
          connect(rangeLinkButton, &QToolButton::toggled, this,
                  [this, button = rangeLinkButton,
                   updateRangeLinkAppearance](const bool checked) {
                    properties_->setProperty("sed.sectorPointLightRangesLinked",
                                             checked);
                    updateRangeLinkAppearance(button, checked);
                  });

          constexpr double kFallbackRangeRatio = 4.0;
          const double initialRangeRatio =
              std::isfinite(sector.pointLight.minRange) &&
                      std::isfinite(sector.pointLight.maxRange) &&
                      sector.pointLight.minRange > sed::math::kTinyEpsilon &&
                      sector.pointLight.maxRange > sed::math::kTinyEpsilon
                  ? sector.pointLight.minRange / sector.pointLight.maxRange
                  : kFallbackRangeRatio;
          const double safeInitialRangeRatio =
              std::isfinite(initialRangeRatio) &&
                      initialRangeRatio > sed::math::kTinyEpsilon
                  ? initialRangeRatio
                  : kFallbackRangeRatio;
          const auto previewLinkedRange =
              [this, minRange, maxRange, safeInitialRangeRatio](
                  const bool editingMin, const double value) {
                if (!properties_->property("sed.sectorPointLightRangesLinked")
                         .toBool()) {
                  return;
                }
                if (editingMin) {
                  maxRange->setValue(value / safeInitialRangeRatio);
                } else {
                  minRange->setValue(value * safeInitialRangeRatio);
                }
              };
          connect(minRange, &ui::ScrubFloatEdit::scrubValueChanged, this,
                  [previewLinkedRange](const double value) {
                    previewLinkedRange(true, value);
                  });
          connect(maxRange, &ui::ScrubFloatEdit::scrubValueChanged, this,
                  [previewLinkedRange](const double value) {
                    previewLinkedRange(false, value);
                  });

          const auto applyRangeValue =
              [this, sectorIndex](editor::DocumentState &state,
                                  const bool editingMin,
                                  const double value) -> bool {
            const bool linked =
                properties_->property("sed.sectorPointLightRangesLinked")
                    .toBool();
            constexpr double kFallbackRatio = 4.0;
            const auto applyOne = [editingMin, value,
                                   linked](Sector &target) -> bool {
              const double oldMin = target.pointLight.minRange;
              const double oldMax = target.pointLight.maxRange;
              const double ratio = std::isfinite(oldMin) &&
                                           std::isfinite(oldMax) &&
                                           oldMin > sed::math::kTinyEpsilon &&
                                           oldMax > sed::math::kTinyEpsilon
                                       ? oldMin / oldMax
                                       : kFallbackRatio;
              const double safeRatio =
                  std::isfinite(ratio) && ratio > sed::math::kTinyEpsilon
                      ? ratio
                      : kFallbackRatio;
              if (editingMin) {
                target.pointLight.minRange = value;
                if (linked) {
                  target.pointLight.maxRange = value / safeRatio;
                }
              } else {
                target.pointLight.maxRange = value;
                if (linked) {
                  target.pointLight.minRange = value * safeRatio;
                }
              }
              return oldMin != target.pointLight.minRange ||
                     oldMax != target.pointLight.maxRange;
            };

            bool changed = false;
            if (!state.selection.sectors.empty()) {
              for (const std::int32_t index : state.selection.sectors) {
                if (index >= 0 && static_cast<std::size_t>(index) <
                                      state.level.sectors.size()) {
                  changed =
                      applyOne(state.level
                                   .sectors[static_cast<std::size_t>(index)]) ||
                      changed;
                }
              }
              return changed;
            }
            if (sectorIndex < 0 || static_cast<std::size_t>(sectorIndex) >=
                                       state.level.sectors.size()) {
              return false;
            }
            return applyOne(
                state.level.sectors[static_cast<std::size_t>(sectorIndex)]);
          };

          constexpr editor::DocumentDirty kPointLightRangeDirty =
              editor::DocumentDirty::LevelGeometry |
              editor::DocumentDirty::Things;
          connectLiveScrub(minRange, "Sector: Point light range changed",
                           kPointLightRangeDirty,
                           [applyRangeValue](editor::DocumentState &state,
                                             const double value) {
                             return applyRangeValue(state, true, value);
                           });
          connectLiveScrub(maxRange, "Sector: Point light range changed",
                           kPointLightRangeDirty,
                           [applyRangeValue](editor::DocumentState &state,
                                             const double value) {
                             return applyRangeValue(state, false, value);
                           });

          const auto commitRange = [applyUndoable,
                                    applyRangeValue](const bool editingMin,
                                                     const double value) {
            (void)applyUndoable(
                "Sector: Point light range changed", kPointLightRangeDirty,
                [applyRangeValue, editingMin,
                 value](editor::DocumentState &state) -> bool {
                  return applyRangeValue(state, editingMin, value);
                });
          };
          connect(
              minRange, &ui::ScrubFloatEdit::valueCommitted, this,
              [commitRange](const double value) { commitRange(true, value); });
          connect(
              minRange, &ui::ScrubFloatEdit::scrubFinished, this,
              [commitRange](const double value) { commitRange(true, value); });
          connect(
              maxRange, &ui::ScrubFloatEdit::valueCommitted, this,
              [commitRange](const double value) { commitRange(false, value); });
          connect(
              maxRange, &ui::ScrubFloatEdit::scrubFinished, this,
              [commitRange](const double value) { commitRange(false, value); });
        }
      } else {
        const auto parseScalarIntensity = [](const double value)
            -> std::optional<float> {
          if (!std::isfinite(value)) {
            return std::nullopt;
          }
          const double maxFloat =
              static_cast<double>(std::numeric_limits<float>::max());
          if (value < -maxFloat || value > maxFloat) {
            return std::nullopt;
          }
          return static_cast<float>(value);
        };
        const auto scalarIntensity = [](const ColorF &color) -> double {
          // Do the average in double. ColorF::intensity() adds float
          // channels first, which can overflow for valid finite HDR values.
          return (static_cast<double>(color.r) +
                  static_cast<double>(color.g) +
                  static_cast<double>(color.b)) /
                 3.0;
        };
        const auto scalarIntensityColor = [](const float intensity) {
          // Classic Sector Ambient/Extra are authored scalar values. Keep
          // their signed and HDR range in the document; the viewport's
          // completed-light path decides where display clamping belongs.
          return ColorF{ intensity, intensity, intensity, 1.0f };
        };
        auto addIntensityRow =
            [&](const QString &key,
                const std::function<ColorF(const Sector &)> &getColor,
                const std::function<void(Sector &, float)> &setIntensity,
                const std::function<void(Sector &, const ColorF &)> &setExactColor,
                const std::string &undoLabel, const editor::DocumentDirty dirty,
                const bool showAmbientCalculation = false) {
              auto *item = addChild(lighting, key);
              auto *row = new QWidget(properties_);
              auto *rowLayout = new QHBoxLayout(row);
              rowLayout->setContentsMargins(0, 0, 0, 0);
              rowLayout->setSpacing(4);

              auto *scrub = new ui::ScrubFloatEdit(row);
              scrub->setDecimals(6);
              scrub->setSingleStep(0.01);
              scrub->clearRange();
              scrub->setValue(scalarIntensity(getColor(sector)));
              rowLayout->addWidget(scrub, 1);
              if (showAmbientCalculation) {
                rowLayout->addWidget(makeAmbientFromSurfacesButton(row));
              }
              properties_->setItemWidget(item, 1, row);
              std::vector<std::int32_t> targetSectorIndices;
              if (!document_.selection().sectors.empty()) {
                targetSectorIndices.assign(
                    document_.selection().sectors.begin(),
                    document_.selection().sectors.end());
              } else if (sectorIndex >= 0) {
                targetSectorIndices.push_back(sectorIndex);
              }
              auto originalColors =
                  std::make_shared<std::vector<std::pair<std::int32_t, ColorF>>>();
              auto captureOriginal =
                  [targetSectorIndices, getColor, originalColors](
                      const editor::DocumentState &state) {
                    originalColors->clear();
                    for (const std::int32_t index : targetSectorIndices) {
                      if (index < 0 || static_cast<std::size_t>(index) >=
                                           state.level.sectors.size()) {
                        continue;
                      }
                      originalColors->emplace_back(
                          index, getColor(state.level.sectors[
                              static_cast<std::size_t>(index)]));
                    }
                  };
              auto restoreOriginal =
                  [originalColors, getColor, setExactColor](
                      editor::DocumentState &state) -> bool {
                    bool changed = false;
                    for (const auto &[index, original] : *originalColors) {
                      if (index < 0 || static_cast<std::size_t>(index) >=
                                           state.level.sectors.size()) {
                        continue;
                      }
                      Sector &target = state.level.sectors[
                          static_cast<std::size_t>(index)];
                      const ColorF before = getColor(target);
                      setExactColor(target, original);
                      changed = before != original || changed;
                    }
                    return changed;
                  };
              auto applyScalarIntensity =
                  [targetSectorIndices, getColor, setIntensity,
                   parseScalarIntensity](editor::DocumentState &state,
                                         const double value) -> bool {
                const std::optional<float> intensity =
                    parseScalarIntensity(value);
                if (!intensity) {
                  return false;
                }

                bool any = false;
                for (const std::int32_t index : targetSectorIndices) {
                  if (index < 0 || static_cast<std::size_t>(index) >=
                                       state.level.sectors.size()) {
                    continue;
                  }
                  Sector &target = state.level.sectors[
                      static_cast<std::size_t>(index)];
                  const ColorF before = getColor(target);
                  setIntensity(target, *intensity);
                  any = before != getColor(target) || any;
                }
                return any;
              };
              auto onCommit = [applyUndoable, applyScalarIntensity,
                               undoLabel, dirty](const double value) {
                (void)applyUndoable(
                    undoLabel, dirty,
                    [applyScalarIntensity, value](
                        editor::DocumentState &state) -> bool {
                      return applyScalarIntensity(state, value);
                    });
              };
              connect(scrub, &ui::ScrubFloatEdit::valueCommitted, this,
                      onCommit);
              connectLiveScrub(scrub, undoLabel, dirty,
                               applyScalarIntensity, false, true,
                               captureOriginal, restoreOriginal);
            };

        addIntensityRow(
            "Ambient", [](const Sector &s) { return s.ambient; },
            [scalarIntensityColor](Sector &s, const float intensity) {
              s.ambient = scalarIntensityColor(intensity);
              s.ambient.a = 0.0f;
            },
            [](Sector &s, const ColorF &color) { s.ambient = color; },
            "Sector: Ambient changed",
            editor::DocumentDirty::LevelGeometry |
                editor::DocumentDirty::Things,
            true);

        addIntensityRow(
            "Extra", [](const Sector &s) { return s.extraLight; },
            [scalarIntensityColor](Sector &s, const float intensity) {
              s.extraLight = scalarIntensityColor(intensity);
              s.extraLight.a = 0.0f;
            },
            [](Sector &s, const ColorF &color) { s.extraLight = color; },
            "Sector: Extra light changed",
            editor::DocumentDirty::LevelGeometry |
                editor::DocumentDirty::Things);

        // Tint (color)
        {
          auto *item = addChild(lighting, "Tint");
          auto *button = new QPushButton(properties_);
          setSwatch(button, colorToQ(sector.tint, false));
          properties_->setItemWidget(item, 1, button);
          connectLiveColor(
              button, colorToQ(sector.tint, false), false,
              "Sector: Tint changed",
              editor::DocumentDirty::LevelGeometry |
                  editor::DocumentDirty::Things,
              [sectorIndex, qToColor](editor::DocumentState &state,
                                      const QColor &color) -> bool {
                const ColorF chosen = qToColor(color);
                auto applyOne = [&](Sector &s) -> bool {
                  const ColorF before = s.tint;
                  s.tint.r = chosen.r;
                  s.tint.g = chosen.g;
                  s.tint.b = chosen.b;
                  const ColorF after = s.tint;
                  return before.r != after.r || before.g != after.g ||
                         before.b != after.b;
                };

                bool any = false;
                if (!state.selection.sectors.empty()) {
                  for (const std::int32_t idx : state.selection.sectors) {
                    if (idx < 0 || static_cast<std::size_t>(idx) >=
                                       state.level.sectors.size()) {
                      continue;
                    }
                    any |= applyOne(
                        state.level.sectors[static_cast<std::size_t>(idx)]);
                  }
                  return any;
                }

                if (sectorIndex < 0 || static_cast<std::size_t>(sectorIndex) >=
                                           state.level.sectors.size()) {
                  return false;
                }

                return applyOne(
                    state.level.sectors[static_cast<std::size_t>(sectorIndex)]);
              });
        }

        // Colormap (editable)
        {
          auto *item = addChild(lighting, "Colormap");
          auto *row = new QWidget(properties_);
          auto *rowLayout = new QHBoxLayout(row);
          rowLayout->setContentsMargins(0, 0, 0, 0);
          rowLayout->setSpacing(4);

          auto *edit = new QLineEdit(row);
          edit->setText(QString::fromStdString(sector.colormap));
          rowLayout->addWidget(edit, 1);

          auto *pickBtn = new QToolButton(row);
          pickBtn->setText("...");
          pickBtn->setAutoRaise(true);
          pickBtn->setToolTip("Pick colormap from resources");
          rowLayout->addWidget(pickBtn);
          openAuxiliaryEditorOnDoubleClick(edit, pickBtn);

          properties_->setItemWidget(item, 1, row);
          connect(
              edit, &QLineEdit::editingFinished, this,
              [this, edit, sectorIndex, applyUndoable]() {
                const Level &lvl = document_.level();
                if (sectorIndex < 0 || static_cast<std::size_t>(sectorIndex) >=
                                           lvl.sectors.size()) {
                  return;
                }

                const Sector &sec =
                    lvl.sectors[static_cast<std::size_t>(sectorIndex)];
                const QString cur = QString::fromStdString(sec.colormap);
                const QString next = edit->text().trimmed();
                if (next == cur) {
                  return;
                }

                const std::string nextStd = next.toStdString();
                applyUndoable(
                    "Sector: Colormap changed",
                    editor::DocumentDirty::LevelGeometry,
                    [&](editor::DocumentState &state) -> bool {
                      auto applyOne = [&](Sector &s) -> bool {
                        if (s.colormap == nextStd) {
                          return false;
                        }
                        s.colormap = nextStd;
                        return true;
                      };

                      bool any = false;
                      if (!state.selection.sectors.empty()) {
                        for (const std::int32_t idx : state.selection.sectors) {
                          if (idx < 0 || static_cast<std::size_t>(idx) >=
                                             state.level.sectors.size()) {
                            continue;
                          }
                          any |= applyOne(
                              state.level
                                  .sectors[static_cast<std::size_t>(idx)]);
                        }
                        return any;
                      }

                      if (sectorIndex < 0 ||
                          static_cast<std::size_t>(sectorIndex) >=
                              state.level.sectors.size()) {
                        return false;
                      }

                      return applyOne(
                          state.level
                              .sectors[static_cast<std::size_t>(sectorIndex)]);
                    });
              });

          connect(pickBtn, &QToolButton::clicked, this,
                  [this, edit, openResourcePicker]() {
                    if (!edit) {
                      return;
                    }
                    const auto picked =
                        openResourcePicker(ui::ResourcePickerKind::cmp,
                                           edit->text(), "Pick Colormap");
                    if (!picked) {
                      return;
                    }
                    edit->setText(*picked);
                    QMetaObject::invokeMethod(edit, "editingFinished",
                                              Qt::DirectConnection);
                  });
        }
      }
    }

    // Sound
    {
      bool hasSound = !sector.sound.empty() || sector.soundVolume != 0.0;
      for (const std::int32_t idx : document_.selection().sectors) {
        if (idx < 0 || static_cast<std::size_t>(idx) >= level.sectors.size()) {
          continue;
        }
        const Sector &selected = level.sectors[static_cast<std::size_t>(idx)];
        hasSound =
            hasSound || !selected.sound.empty() || selected.soundVolume != 0.0;
      }
      if (!hasSound) {
        auto *item = addChild(nullptr, {});
        item->setFirstColumnSpanned(true);
        auto *button = new QToolButton(properties_);
        button->setObjectName(QStringLiteral("prop.sector.sound.add"));
        button->setText(QStringLiteral("Add Sound"));
        button->setIcon(ui::makeViewportIcon(ui::ViewportIcon::sound));
        button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        button->setSizePolicy(QSizePolicy::MinimumExpanding,
                              QSizePolicy::Fixed);
        button->setToolTip(QStringLiteral("Adds sector sound"));
        properties_->setItemWidget(item, 0, button);
        connect(button, &QToolButton::clicked, this,
                [this, button, sectorIndex, openResourcePicker,
                 applyUndoableNoNotify]() {
                  const auto picked =
                      openResourcePicker(ui::ResourcePickerKind::wav, {},
                                         QStringLiteral("Add Sector Sound"));
                  if (!picked) {
                    return;
                  }
                  button->setEnabled(false);
                  const std::string soundName = picked->trimmed().toStdString();
                  (void)applyUndoableNoNotify(
                      "Sector: Sound added",
                      editor::DocumentDirty::LevelGeometry,
                      [this, sectorIndex,
                       soundName](editor::DocumentState &state) -> bool {
                        if (soundName.empty() || sectorIndex < 0 ||
                            static_cast<std::size_t>(sectorIndex) >=
                                state.level.sectors.size()) {
                          return false;
                        }
                        Sector &target =
                            state.level
                                .sectors[static_cast<std::size_t>(sectorIndex)];
                        if (isLayerLocked(target.layer)) {
                          return false;
                        }
                        target.sound = soundName;
                        if (!(target.soundVolume > 0.0)) {
                          target.soundVolume = 1.0;
                        }
                        return true;
                      });
                });
      } else {
        auto *sound = addGroup(nullptr, "Sound");

        {
          auto *item = addChild(sound, "Name");
          auto *row = new QWidget(properties_);
          auto *rowLayout = new QHBoxLayout(row);
          rowLayout->setContentsMargins(0, 0, 0, 0);
          rowLayout->setSpacing(4);

          auto *edit = new QLineEdit(row);
          edit->setText(QString::fromStdString(sector.sound));
          rowLayout->addWidget(edit, 1);

          auto *pickBtn = new QToolButton(row);
          pickBtn->setText("...");
          pickBtn->setAutoRaise(true);
          pickBtn->setToolTip("Pick sector sound (WAV) from resources");
          rowLayout->addWidget(pickBtn);
          openAuxiliaryEditorOnDoubleClick(edit, pickBtn);

          properties_->setItemWidget(item, 1, row);
          connect(
              edit, &QLineEdit::editingFinished, this,
              [this, edit, sectorIndex, applyUndoableNoNotify]() {
                const Level &lvl = document_.level();
                if (sectorIndex < 0 || static_cast<std::size_t>(sectorIndex) >=
                                           lvl.sectors.size()) {
                  return;
                }

                const Sector &sec =
                    lvl.sectors[static_cast<std::size_t>(sectorIndex)];
                const QString cur = QString::fromStdString(sec.sound);
                const QString next = edit->text().trimmed();
                if (next == cur) {
                  return;
                }

                const std::string nextStd = next.toStdString();
                (void)applyUndoableNoNotify(
                    "Sector: Sound changed",
                    editor::DocumentDirty::LevelGeometry,
                    [&](editor::DocumentState &state) -> bool {
                      auto applyOne = [&](Sector &s) -> bool {
                        if (s.sound == nextStd) {
                          return false;
                        }
                        s.sound = nextStd;
                        return true;
                      };

                      bool any = false;
                      if (!state.selection.sectors.empty()) {
                        for (const std::int32_t idx : state.selection.sectors) {
                          if (idx < 0 || static_cast<std::size_t>(idx) >=
                                             state.level.sectors.size()) {
                            continue;
                          }
                          any |= applyOne(
                              state.level
                                  .sectors[static_cast<std::size_t>(idx)]);
                        }
                        return any;
                      }

                      if (sectorIndex < 0 ||
                          static_cast<std::size_t>(sectorIndex) >=
                              state.level.sectors.size()) {
                        return false;
                      }
                      return applyOne(
                          state.level
                              .sectors[static_cast<std::size_t>(sectorIndex)]);
                    });
              });

          connect(pickBtn, &QToolButton::clicked, this,
                  [this, edit, openResourcePicker]() {
                    if (!edit) {
                      return;
                    }
                    const auto picked =
                        openResourcePicker(ui::ResourcePickerKind::wav,
                                           edit->text(), "Pick Sector Sound");
                    if (!picked) {
                      return;
                    }
                    edit->setText(*picked);
                    QMetaObject::invokeMethod(edit, "editingFinished",
                                              Qt::DirectConnection);
                  });
        }

        {
          auto *item = addChild(sound, "Volume");
          auto *scrub = new ui::ScrubFloatEdit(properties_);
          scrub->setDecimals(3);
          scrub->setSingleStep(0.01);
          scrub->setRange(0.0, 1.0);
          scrub->setValue(sector.soundVolume);
          properties_->setItemWidget(item, 1, scrub);
          auto onCommit = [this, sectorIndex, applyUndoableNoNotify](double v) {
            (void)applyUndoableNoNotify(
                "Sector: Sound volume changed",
                editor::DocumentDirty::LevelGeometry,
                [&](editor::DocumentState &state) -> bool {
                  const double next = std::clamp(v, 0.0, 1.0);

                  auto applyOne = [&](Sector &s) -> bool {
                    if (s.soundVolume == next) {
                      return false;
                    }
                    s.soundVolume = next;
                    return true;
                  };

                  bool any = false;
                  if (!state.selection.sectors.empty()) {
                    for (const std::int32_t idx : state.selection.sectors) {
                      if (idx < 0 || static_cast<std::size_t>(idx) >=
                                         state.level.sectors.size()) {
                        continue;
                      }
                      any |= applyOne(
                          state.level.sectors[static_cast<std::size_t>(idx)]);
                    }
                    return any;
                  }

                  if (sectorIndex < 0 ||
                      static_cast<std::size_t>(sectorIndex) >=
                          state.level.sectors.size()) {
                    return false;
                  }
                  return applyOne(
                      state.level
                          .sectors[static_cast<std::size_t>(sectorIndex)]);
                });
          };
          connect(scrub, &ui::ScrubFloatEdit::valueCommitted, this, onCommit);
          connect(scrub, &ui::ScrubFloatEdit::scrubFinished, this, onCommit);
        }

        bool selectionHasSound =
            !sector.sound.empty() || sector.soundVolume != 0.0;
        for (const std::int32_t idx : document_.selection().sectors) {
          if (idx < 0 ||
              static_cast<std::size_t>(idx) >= level.sectors.size()) {
            continue;
          }
          const Sector &selected = level.sectors[static_cast<std::size_t>(idx)];
          selectionHasSound = selectionHasSound || !selected.sound.empty() ||
                              selected.soundVolume != 0.0;
        }
        if (selectionHasSound) {
          auto *row = new QWidget(properties_);
          auto *rowLayout = new QHBoxLayout(row);
          rowLayout->setContentsMargins(0, 0, 0, 0);
          rowLayout->setSpacing(2);
          auto *removeButton = new QToolButton(row);
          removeButton->setObjectName("prop.sector.sound.remove");
          removeButton->setIcon(
              ui::makeViewportIcon(ui::ViewportIcon::actionDelete));
          removeButton->setIconSize(QSize(18, 18));
          removeButton->setAccessibleName("Remove sector sound");
          removeButton->setToolButtonStyle(Qt::ToolButtonIconOnly);
          removeButton->setAutoRaise(true);
          removeButton->setStyleSheet(
              QStringLiteral("QToolButton { border: 0; background: "
                             "transparent; padding: 1px; }"));
          removeButton->setFixedSize(24, 24);
          removeButton->setToolTip("Remove the ambient sound and reset its "
                                   "volume for every selected sector.");
          rowLayout->addWidget(removeButton);
          rowLayout->addStretch(1);
          sound->setFirstColumnSpanned(false);
          properties_->setItemWidget(sound, 1, row);

          connect(
              removeButton, &QToolButton::clicked, this,
              [this, sectorIndex, applyUndoableNoNotify]() {
                (void)applyUndoableNoNotify(
                    "Sector: Sound removed",
                    editor::DocumentDirty::LevelGeometry,
                    [this, sectorIndex](editor::DocumentState &state) -> bool {
                      const auto removeOne =
                          [this, &state](const std::int32_t index) -> bool {
                        if (index < 0 || static_cast<std::size_t>(index) >=
                                             state.level.sectors.size()) {
                          return false;
                        }
                        Sector &target =
                            state.level
                                .sectors[static_cast<std::size_t>(index)];
                        if (isLayerLocked(target.layer) ||
                            (target.sound.empty() &&
                             target.soundVolume == 0.0)) {
                          return false;
                        }
                        target.sound.clear();
                        target.soundVolume = 0.0;
                        return true;
                      };

                      bool changed = false;
                      if (!state.selection.sectors.empty()) {
                        for (const std::int32_t index :
                             state.selection.sectors) {
                          changed = removeOne(index) || changed;
                        }
                      } else {
                        changed = removeOne(sectorIndex);
                      }
                      return changed;
                    });
              });
        }
      }

      if (level.kind == ProjectType::IJIM) {
        bool hasThrust = !sector.thrust.IsZero() ||
                         (sector.flags & SECF_IJIM_UseThrust) != 0;
        for (const std::int32_t idx : document_.selection().sectors) {
          if (idx < 0 ||
              static_cast<std::size_t>(idx) >= level.sectors.size()) {
            continue;
          }
          const Sector &selected = level.sectors[static_cast<std::size_t>(idx)];
          hasThrust = hasThrust || !selected.thrust.IsZero() ||
                      (selected.flags & SECF_IJIM_UseThrust) != 0;
        }
        if (!hasThrust) {
          auto *item = addChild(nullptr, {});
          item->setFirstColumnSpanned(true);
          auto *button = new QToolButton(properties_);
          button->setObjectName(QStringLiteral("prop.sector.thrust.add"));
          button->setText(QStringLiteral("Add Thrust"));
          button->setIcon(ui::makeViewportIcon(ui::ViewportIcon::sectorThrust));
          button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
          button->setSizePolicy(QSizePolicy::MinimumExpanding,
                                QSizePolicy::Fixed);
          button->setToolTip(QStringLiteral("Adds sector thrust"));
          properties_->setItemWidget(item, 0, button);
          connect(
              button, &QToolButton::clicked, this,
              [this, button, sectorIndex, applyUndoableNoNotify]() {
                button->setEnabled(false);
                (void)applyUndoableNoNotify(
                    "Sector: Thrust added",
                    editor::DocumentDirty::LevelGeometry,
                    [this, sectorIndex](editor::DocumentState &state) -> bool {
                      if (sectorIndex < 0 ||
                          static_cast<std::size_t>(sectorIndex) >=
                              state.level.sectors.size()) {
                        return false;
                      }
                      Sector &target =
                          state.level
                              .sectors[static_cast<std::size_t>(sectorIndex)];
                      if (isLayerLocked(target.layer)) {
                        return false;
                      }
                      target.thrust = TVector{0.0, -1.0, 0.0};
                      target.flags |= SECF_IJIM_UseThrust;
                      return true;
                    });
              });
        } else {
          auto *thrust = addGroup(nullptr, "Thrust");

          auto finishThrust = [](Sector &s, const TVector &next) -> bool {
            const std::int32_t nextFlags =
                next.IsZero() ? (s.flags & ~SECF_IJIM_UseThrust)
                              : (s.flags | SECF_IJIM_UseThrust);
            if (s.thrust == next && s.flags == nextFlags) {
              return false;
            }
            s.thrust = next;
            s.flags = nextFlags;
            return true;
          };

          auto editThrustSelection =
              [sectorIndex, finishThrust](
                  editor::DocumentState &state,
                  const std::function<bool(Sector &)> &edit) -> bool {
            // The visible editor belongs to sectorIndex. If it is no
            // longer selected, a stale property row must not mutate it.
            if (!edit || !state.selection.sectors.contains(sectorIndex) ||
                sectorIndex < 0 ||
                static_cast<std::size_t>(sectorIndex) >=
                    state.level.sectors.size()) {
              return false;
            }

            Sector &primary =
                state.level.sectors[static_cast<std::size_t>(sectorIndex)];
            bool any = edit(primary);
            const TVector next = primary.thrust;
            for (const std::int32_t idx : state.selection.sectors) {
              if (idx == sectorIndex || idx < 0 ||
                  static_cast<std::size_t>(idx) >= state.level.sectors.size()) {
                continue;
              }
              any |= finishThrust(
                  state.level.sectors[static_cast<std::size_t>(idx)], next);
            }
            return any;
          };

          auto applyThrustEdit =
              [applyUndoableNoNotify, editThrustSelection](
                  const char *undoLabel,
                  const std::function<bool(Sector &)> &edit) -> bool {
            if (!edit) {
              return false;
            }
            return applyUndoableNoNotify(
                undoLabel, editor::DocumentDirty::LevelGeometry,
                [editThrustSelection, edit](editor::DocumentState &state)
                    -> bool { return editThrustSelection(state, edit); });
          };

          TVector initialDirection = sector.thrust;
          const double initialMagnitude = initialDirection.Normalize();

          auto *directionEditor = new QWidget(properties_);
          auto *directionLayout = new QVBoxLayout(directionEditor);
          directionLayout->setContentsMargins(0, 0, 0, 0);
          directionLayout->setSpacing(0);

          auto *directionPad =
              new ui::SectorThrustDirectionPad(directionEditor);
          directionPad->setObjectName("sectorThrustDirectionPad");
          directionPad->setDirection(
              QVector3D(static_cast<float>(initialDirection.x),
                        static_cast<float>(initialDirection.y),
                        static_cast<float>(initialDirection.z)));

          auto *rotationPlaneButton = new QToolButton(directionEditor);
          rotationPlaneButton->setObjectName("sectorThrustRotationPlane");
          rotationPlaneButton->setAccessibleName(
              QStringLiteral("Change thrust rotation plane"));
          rotationPlaneButton->setAutoRaise(true);
          rotationPlaneButton->setToolButtonStyle(Qt::ToolButtonTextOnly);
          rotationPlaneButton->setStyleSheet(
              QStringLiteral("QToolButton { border: 0; background: "
                             "transparent; padding: 2px 8px; color: #cfd3da; }"
                             "QToolButton:hover { color: #ffffff; background: "
                             "rgba(255, 255, 255, 18); }"));
          const int storedRotationPlane = std::clamp(
              properties_->property("sed.sectorThrustRotationPlane").toInt(),
              static_cast<int>(ui::SectorThrustDirectionPad::RotationPlane::XY),
              static_cast<int>(
                  ui::SectorThrustDirectionPad::RotationPlane::YZ));
          directionPad->setRotationPlane(
              static_cast<ui::SectorThrustDirectionPad::RotationPlane>(
                  storedRotationPlane));

          const auto updateRotationPlaneButton =
              [](QToolButton *button,
                 const ui::SectorThrustDirectionPad::RotationPlane plane) {
                if (!button) {
                  return;
                }
                QString planeName;
                QString axisName;
                QString nextName;
                switch (plane) {
                case ui::SectorThrustDirectionPad::RotationPlane::XY:
                  planeName = QStringLiteral("XY");
                  axisName = QStringLiteral("Z");
                  nextName = QStringLiteral("XZ");
                  break;
                case ui::SectorThrustDirectionPad::RotationPlane::XZ:
                  planeName = QStringLiteral("XZ");
                  axisName = QStringLiteral("Y");
                  nextName = QStringLiteral("YZ");
                  break;
                case ui::SectorThrustDirectionPad::RotationPlane::YZ:
                  planeName = QStringLiteral("YZ");
                  axisName = QStringLiteral("X");
                  nextName = QStringLiteral("XY");
                  break;
                }
                button->setText(QStringLiteral("%1 plane (%2 axis)")
                                    .arg(planeName, axisName));
                button->setToolTip(
                    QStringLiteral("The wheel rotates in the %1 plane about "
                                   "%2. Click to switch to %3.")
                        .arg(planeName, axisName, nextName));
              };
          updateRotationPlaneButton(rotationPlaneButton,
                                    directionPad->rotationPlane());
          connect(rotationPlaneButton, &QToolButton::clicked, this,
                  [this, directionPad, rotationPlaneButton,
                   updateRotationPlaneButton]() {
                    using Plane = ui::SectorThrustDirectionPad::RotationPlane;
                    Plane next = Plane::XY;
                    switch (directionPad->rotationPlane()) {
                    case Plane::XY:
                      next = Plane::XZ;
                      break;
                    case Plane::XZ:
                      next = Plane::YZ;
                      break;
                    case Plane::YZ:
                      next = Plane::XY;
                      break;
                    }
                    properties_->setProperty("sed.sectorThrustRotationPlane",
                                             static_cast<int>(next));
                    directionPad->setRotationPlane(next);
                    updateRotationPlaneButton(rotationPlaneButton, next);
                  });

          directionLayout->addWidget(directionPad, 1);
          directionLayout->addWidget(rotationPlaneButton, 0, Qt::AlignHCenter);

          auto *directionItem = addChild(thrust, "Direction");
          directionItem->setSizeHint(1, QSize(140, 140));
          properties_->setItemWidget(directionItem, 1, directionEditor);

          struct ThrustWheelEdit {
            bool active{};
            bool undoPushed{};
            bool changed{};
            std::vector<std::pair<std::int32_t, TVector>> baselines{};
          };
          const auto wheelEdit = std::make_shared<ThrustWheelEdit>();

          directionPad->directionEditStarted = [this, sectorIndex,
                                                wheelEdit]() {
            if (!documentEditingAllowed()) {
              wheelEdit->active = false;
              wheelEdit->undoPushed = false;
              wheelEdit->changed = false;
              wheelEdit->baselines.clear();
              return;
            }
            wheelEdit->active = true;
            wheelEdit->undoPushed = false;
            wheelEdit->changed = false;
            wheelEdit->baselines.clear();

            const editor::DocumentState &state = document_.current();
            if (state.selection.sectors.contains(sectorIndex)) {
              wheelEdit->baselines.reserve(state.selection.sectors.size());
              for (const std::int32_t idx : state.selection.sectors) {
                if (idx < 0 || static_cast<std::size_t>(idx) >=
                                   state.level.sectors.size()) {
                  continue;
                }
                wheelEdit->baselines.emplace_back(
                    idx,
                    state.level.sectors[static_cast<std::size_t>(idx)].thrust);
              }
            }
          };
          directionPad->directionPreviewed = [this, sectorIndex, wheelEdit,
                                              finishThrust](
                                                 const QVector3D &chosen) {
            if (!documentEditingAllowed()) {
              wheelEdit->active = false;
              wheelEdit->baselines.clear();
              return;
            }
            if (!wheelEdit->active || wheelEdit->baselines.empty()) {
              return;
            }

            TVector direction{static_cast<double>(chosen.x()),
                              static_cast<double>(chosen.y()),
                              static_cast<double>(chosen.z())};
            if (direction.Normalize() == 0.0) {
              return;
            }

            const auto primaryBaseline = std::find_if(
                wheelEdit->baselines.begin(), wheelEdit->baselines.end(),
                [sectorIndex](const auto &entry) {
                  return entry.first == sectorIndex;
                });
            if (primaryBaseline == wheelEdit->baselines.end()) {
              return;
            }

            const double baselineMagnitude = primaryBaseline->second.Length();
            if (!std::isfinite(baselineMagnitude)) {
              return;
            }
            const TVector next = direction.Scaled(
                baselineMagnitude > 1.0e-12 ? baselineMagnitude : 1.0);

            struct NextThrust {
              std::int32_t index{-1};
              TVector value{};
            };
            std::vector<NextThrust> nextValues{};
            nextValues.reserve(wheelEdit->baselines.size());
            bool any = false;
            editor::DocumentState &state = document_.current();
            for (const auto &[idx, baseline] : wheelEdit->baselines) {
              (void)baseline;
              if (idx < 0 ||
                  static_cast<std::size_t>(idx) >= state.level.sectors.size()) {
                continue;
              }
              if (!state.selection.sectors.contains(idx)) {
                continue;
              }
              const Sector &current =
                  state.level.sectors[static_cast<std::size_t>(idx)];
              const std::int32_t nextFlags =
                  next.IsZero() ? (current.flags & ~SECF_IJIM_UseThrust)
                                : (current.flags | SECF_IJIM_UseThrust);
              any |= current.thrust != next || current.flags != nextFlags;
              nextValues.push_back(NextThrust{idx, next});
            }
            if (!any) {
              return;
            }

            if (!wheelEdit->undoPushed) {
              document_.pushUndo(ui::undo::relabelForSelection(
                                     "Sector: Thrust direction changed",
                                     document_.selection()),
                                 editor::DocumentDirty::LevelGeometry);
              updateEditActions();
              wheelEdit->undoPushed = true;
            }
            editor::DocumentState &editableState = document_.current();
            for (const NextThrust &next : nextValues) {
              if (next.index < 0 || static_cast<std::size_t>(next.index) >=
                                        editableState.level.sectors.size()) {
                continue;
              }
              (void)finishThrust(
                  editableState.level
                      .sectors[static_cast<std::size_t>(next.index)],
                  next.value);
            }
            wheelEdit->changed = true;
            // Thrust affects only this overlay. Avoid rebuilding the
            // level mesh while the wheel pointer is moving.
            if (glView_) {
              glView_->update();
            }
          };
          directionPad->directionEditFinished = [this, sectorIndex,
                                                 wheelEdit](const QVector3D &) {
            if (!documentEditingAllowed()) {
              wheelEdit->active = false;
              wheelEdit->undoPushed = false;
              wheelEdit->changed = false;
              wheelEdit->baselines.clear();
              return;
            }
            if (!wheelEdit->active) {
              return;
            }
            wheelEdit->active = false;
            wheelEdit->baselines.clear();
            if (!wheelEdit->changed) {
              return;
            }

            scheduleRebuildChecks(editor::DocumentDirty::LevelGeometry);
            // Keep the exact final wheel direction on screen. Rebuilding
            // the whole property tree here destroyed and recreated the
            // pad, which looked like a release-time snap even when the
            // persisted vector was correct. Only the numeric mirrors
            // need refreshing after a wheel gesture.
            const editor::DocumentState &state = document_.current();
            if (sectorIndex >= 0 && static_cast<std::size_t>(sectorIndex) <
                                        state.level.sectors.size()) {
              TVector direction =
                  state.level.sectors[static_cast<std::size_t>(sectorIndex)]
                      .thrust;
              const double magnitude = direction.Normalize();
              const std::array<std::pair<const char *, double>, 3> components{
                  std::pair{"sectorThrustDirectionX", direction.x},
                  std::pair{"sectorThrustDirectionY", direction.y},
                  std::pair{"sectorThrustDirectionZ", direction.z},
              };
              for (const auto &[name, value] : components) {
                if (auto *edit = properties_->findChild<ui::ScrubFloatEdit *>(
                        QString::fromLatin1(name))) {
                  const QSignalBlocker blocker(edit);
                  edit->setValue(value);
                }
              }
              if (auto *edit = properties_->findChild<ui::ScrubFloatEdit *>(
                      QStringLiteral("sectorThrustMagnitude"))) {
                const QSignalBlocker blocker(edit);
                edit->setValue(magnitude);
              }
            }
            if (glView_) {
              glView_->update();
            }
          };

          auto addDirectionComponent = [&](const QString &key,
                                           const int component,
                                           const double value) {
            auto *item = addChild(thrust, key);
            auto *scrub = new ui::ScrubFloatEdit(properties_);
            scrub->setObjectName(
                QStringLiteral("sectorThrustDirection%1").arg(key));
            scrub->setDecimals(6);
            scrub->setSingleStep(0.01);
            scrub->setRange(-1.0, 1.0);
            scrub->setValue(std::clamp(value, -1.0, 1.0));
            scrub->setToolTip(
                "Normalized thrust direction component, clamped to -1..1.");
            properties_->setItemWidget(item, 1, scrub);

            auto editComponent =
                [finishThrust, component](Sector &s, const double raw) -> bool {
              const double componentValue =
                  std::isfinite(raw) ? std::clamp(raw, -1.0, 1.0) : 0.0;
              const double magnitude = s.thrust.Length();
              if (!std::isfinite(magnitude)) {
                return false;
              }
              if (magnitude <= 1.0e-12 && std::abs(componentValue) <= 1.0e-12) {
                // Recommitting a zero component on a zero vector
                // must not invent a perpendicular direction or
                // enable thrust implicitly.
                return false;
              }

              const QVector3D direction =
                  render::sectorThrustDirectionWithComponent(
                      QVector3D(static_cast<float>(s.thrust.x),
                                static_cast<float>(s.thrust.y),
                                static_cast<float>(s.thrust.z)),
                      component, static_cast<float>(componentValue));
              const TVector exactDirection{static_cast<double>(direction.x()),
                                           static_cast<double>(direction.y()),
                                           static_cast<double>(direction.z())};
              TVector normalizedExactDirection = exactDirection;
              if (normalizedExactDirection.Normalize() == 0.0) {
                return false;
              }
              return finishThrust(s,
                                  normalizedExactDirection.Scaled(
                                      magnitude > 1.0e-12 ? magnitude : 1.0));
            };
            auto onCommit = [applyThrustEdit, editComponent](const double raw) {
              applyThrustEdit("Sector: Thrust direction changed",
                              [editComponent, raw](Sector &s) {
                                return editComponent(s, raw);
                              });
            };
            connect(scrub, &ui::ScrubFloatEdit::valueCommitted, this, onCommit);
            connect(scrub, &ui::ScrubFloatEdit::scrubFinished, this, onCommit);
            connectLiveScrub(
                scrub, "Sector: Thrust direction changed",
                editor::DocumentDirty::LevelGeometry,
                [editThrustSelection, editComponent](
                    editor::DocumentState &state, const double raw) {
                  return editThrustSelection(state,
                                             [editComponent, raw](Sector &s) {
                                               return editComponent(s, raw);
                                             });
                },
                true);
          };

          addDirectionComponent("X", 0, initialDirection.x);
          addDirectionComponent("Y", 1, initialDirection.y);
          addDirectionComponent("Z", 2, initialDirection.z);

          auto *magnitudeItem = addChild(thrust, "Magnitude");
          auto *magnitudeScrub = new ui::ScrubFloatEdit(properties_);
          magnitudeScrub->setObjectName("sectorThrustMagnitude");
          magnitudeScrub->setDecimals(6);
          magnitudeScrub->setSingleStep(0.01);
          constexpr double kMaxEditableThrustMagnitude =
              static_cast<double>(std::numeric_limits<float>::max());
          magnitudeScrub->setRange(0.0, kMaxEditableThrustMagnitude);
          magnitudeScrub->setValue(std::max(0.0, initialMagnitude));
          magnitudeScrub->setToolTip(
              "Exact thrust magnitude; direction remains normalized.");
          properties_->setItemWidget(magnitudeItem, 1, magnitudeScrub);
          auto editMagnitude = [finishThrust, kMaxEditableThrustMagnitude](
                                   Sector &s, const double raw) -> bool {
            const double magnitude =
                std::isfinite(raw)
                    ? std::clamp(raw, 0.0, kMaxEditableThrustMagnitude)
                    : 0.0;
            TVector direction = s.thrust;
            if (direction.Normalize() == 0.0) {
              direction = TVector{1.0, 0.0, 0.0};
            }
            return finishThrust(s, direction.Scaled(magnitude));
          };
          auto onMagnitudeCommit = [applyThrustEdit,
                                    editMagnitude](const double raw) {
            applyThrustEdit("Sector: Thrust magnitude changed",
                            [editMagnitude, raw](Sector &s) {
                              return editMagnitude(s, raw);
                            });
          };
          connect(magnitudeScrub, &ui::ScrubFloatEdit::valueCommitted, this,
                  onMagnitudeCommit);
          connect(magnitudeScrub, &ui::ScrubFloatEdit::scrubFinished, this,
                  onMagnitudeCommit);
          connectLiveScrub(
              magnitudeScrub, "Sector: Thrust magnitude changed",
              editor::DocumentDirty::LevelGeometry,
              [editThrustSelection, editMagnitude](editor::DocumentState &state,
                                                   const double raw) {
                return editThrustSelection(state,
                                           [editMagnitude, raw](Sector &s) {
                                             return editMagnitude(s, raw);
                                           });
              },
              true);

          auto *actionsWidget = new QWidget(properties_);
          auto *actionsLayout = new QHBoxLayout(actionsWidget);
          actionsLayout->setContentsMargins(0, 0, 0, 0);
          actionsLayout->setSpacing(2);

          auto makeThrustActionButton =
              [actionsWidget, actionsLayout](
                  const QString &objectName, const ui::ViewportIcon icon,
                  const QString &accessibleName,
                  const QString &toolTip) -> QToolButton * {
            auto *button = new QToolButton(actionsWidget);
            button->setObjectName(objectName);
            button->setIcon(ui::makeViewportIcon(icon));
            button->setIconSize(QSize(18, 18));
            button->setAccessibleName(accessibleName);
            button->setToolTip(toolTip);
            button->setAutoRaise(true);
            button->setToolButtonStyle(Qt::ToolButtonIconOnly);
            button->setStyleSheet(
                QStringLiteral("QToolButton { border: 0; background: "
                               "transparent; padding: 1px; }"));
            button->setFixedSize(24, 24);
            actionsLayout->addWidget(button);
            return button;
          };

          QToolButton *removeThrustButton = makeThrustActionButton(
              "prop.sector.thrust.remove", ui::ViewportIcon::actionDelete,
              "Sector: Thrust removed",
              "Remove thrust from every selected sector and clear the IJIM "
              "UseThrust flag.");
          QToolButton *copyThrustButton = makeThrustActionButton(
              "prop.sector.thrust.copy", ui::ViewportIcon::actionCopy,
              "Copy sector thrust",
              "Copy this sector's exact thrust vector to the clipboard.");
          QToolButton *pasteThrustButton = makeThrustActionButton(
              "prop.sector.thrust.paste", ui::ViewportIcon::actionPaste,
              "Paste sector thrust",
              "Paste a copied thrust vector into every selected sector.");
          actionsLayout->addStretch(1);

          bool selectionHasThrust = !sector.thrust.IsZero() ||
                                    (sector.flags & SECF_IJIM_UseThrust) != 0;
          for (const std::int32_t idx : document_.selection().sectors) {
            if (idx < 0 ||
                static_cast<std::size_t>(idx) >= level.sectors.size()) {
              continue;
            }
            const Sector &selected =
                level.sectors[static_cast<std::size_t>(idx)];
            selectionHasThrust = selectionHasThrust ||
                                 !selected.thrust.IsZero() ||
                                 (selected.flags & SECF_IJIM_UseThrust) != 0;
          }
          removeThrustButton->setEnabled(selectionHasThrust);

          thrust->setFirstColumnSpanned(false);
          properties_->setItemWidget(thrust, 1, actionsWidget);

          connect(removeThrustButton, &QToolButton::clicked, this,
                  [applyThrustEdit, finishThrust]() {
                    applyThrustEdit("Sector: Thrust removed",
                                    [finishThrust](Sector &s) {
                                      return finishThrust(s, TVector{});
                                    });
                  });

          connect(
              copyThrustButton, &QToolButton::clicked, this,
              [this, sectorIndex, copyThrustButton,
               showPropertyActionFeedback]() {
                const Level &currentLevel = document_.level();
                if (sectorIndex < 0 || static_cast<std::size_t>(sectorIndex) >=
                                           currentLevel.sectors.size()) {
                  return;
                }

                const TVector &value =
                    currentLevel.sectors[static_cast<std::size_t>(sectorIndex)]
                        .thrust;
                const QLocale c = QLocale::c();
                QApplication::clipboard()->setText(
                    QStringLiteral("SED sector thrust: %1 %2 %3")
                        .arg(c.toString(value.x, 'g', 17),
                             c.toString(value.y, 'g', 17),
                             c.toString(value.z, 'g', 17)));
                showPropertyActionFeedback(copyThrustButton,
                                           QStringLiteral("Copied"));
              });

          connect(
              pasteThrustButton, &QToolButton::clicked, this,
              [this, sectorIndex, pasteThrustButton, applyThrustEdit,
               finishThrust, showPropertyActionFeedback]() {
                const QString clipboardText =
                    QApplication::clipboard()->text().trimmed();
                if (clipboardText.isEmpty()) {
                  showPropertyActionFeedback(pasteThrustButton,
                                             QStringLiteral("Nothing to paste"),
                                             true);
                  return;
                }
                static const QRegularExpression kThrustVectorPattern(
                    QStringLiteral(
                        R"(^\s*(?:SED\s+sector\s+thrust\s*:\s*)?([+-]?(?:\d+(?:\.\d*)?|\.\d+)(?:[eE][+-]?\d+)?)\s*[,;]?\s*([+-]?(?:\d+(?:\.\d*)?|\.\d+)(?:[eE][+-]?\d+)?)\s*[,;]?\s*([+-]?(?:\d+(?:\.\d*)?|\.\d+)(?:[eE][+-]?\d+)?)\s*$)"),
                    QRegularExpression::CaseInsensitiveOption);
                const QRegularExpressionMatch match =
                    kThrustVectorPattern.match(clipboardText);
                bool xOk = false;
                bool yOk = false;
                bool zOk = false;
                const QLocale c = QLocale::c();
                const double x = c.toDouble(match.captured(1), &xOk);
                const double y = c.toDouble(match.captured(2), &yOk);
                const double z = c.toDouble(match.captured(3), &zOk);
                if (!match.hasMatch() || !xOk || !yOk || !zOk ||
                    !std::isfinite(x) || !std::isfinite(y) ||
                    !std::isfinite(z)) {
                  showPropertyActionFeedback(
                      pasteThrustButton,
                      QStringLiteral("Thrust data has the wrong format"), true);
                  return;
                }

                const TVector pasted{x, y, z};
                const editor::DocumentState &before = document_.current();
                const bool hasEditableTarget =
                    before.selection.sectors.contains(sectorIndex) &&
                    sectorIndex >= 0 &&
                    static_cast<std::size_t>(sectorIndex) <
                        before.level.sectors.size() &&
                    !isLayerLocked(
                        before.level
                            .sectors[static_cast<std::size_t>(sectorIndex)]
                            .layer);
                const bool applied = applyThrustEdit(
                    "Sector: Thrust pasted", [finishThrust, pasted](Sector &s) {
                      return finishThrust(s, pasted);
                    });
                showPropertyActionFeedback(
                    pasteThrustButton,
                    applied || hasEditableTarget
                        ? QStringLiteral("Pasted")
                        : QStringLiteral("Could not paste to the selection"),
                    !applied && !hasEditableTarget);
              });
        }
      }
    }

    setLinkedCogList(buildLinkedCogRefs(CogType::sec, sectorIndex));

    properties_->expandAll();
    return;
  }

  if (const std::optional<SurfaceRef> activeSurface =
          activeSelectedSurfaceRef()) {
    const SurfaceRef ref = *activeSurface;

    auto addChild = [&](QTreeWidgetItem *parent, const QString &key,
                        const QString &value = {}) -> QTreeWidgetItem * {
      const int parentIndent =
          parent ? parent->data(0, kPropertyIndentRole).toInt() : -1;
      const int indent = parent ? (parentIndent + 1) : 0;

      auto *item = new QTreeWidgetItem(properties_);
      item->setText(0, QString(indent * 2, QChar(' ')) + key);
      item->setText(1, value);
      item->setData(0, kPropertyIndentRole, indent);
      return item;
    };

    auto addGroup = [&](QTreeWidgetItem *parent,
                        const QString &title) -> QTreeWidgetItem * {
      const int parentIndent =
          parent ? parent->data(0, kPropertyIndentRole).toInt() : -1;
      const int indent = parent ? (parentIndent + 1) : 0;

      auto *item = new QTreeWidgetItem(properties_);
      item->setText(0, QString(indent * 2, QChar(' ')) + title);
      item->setFirstColumnSpanned(true);
      item->setChildIndicatorPolicy(
          QTreeWidgetItem::DontShowIndicatorWhenChildless);
      item->setFlags(item->flags() & ~Qt::ItemIsSelectable);
      item->setData(0, kPropertyIndentRole, indent);
      QFont f = item->font(0);
      f.setBold(true);
      item->setFont(0, f);
      item->setExpanded(true);
      return item;
    };

    auto *root = addGroup(nullptr, "General");

    auto colorToQ = [](const ColorF &c, const bool alpha) -> QColor {
      const float a = alpha ? c.a : 1.0f;
      return QColor::fromRgbF(std::clamp(static_cast<double>(c.r), 0.0, 1.0),
                              std::clamp(static_cast<double>(c.g), 0.0, 1.0),
                              std::clamp(static_cast<double>(c.b), 0.0, 1.0),
                              std::clamp(static_cast<double>(a), 0.0, 1.0));
    };

    auto qToColor = [](const QColor &qc) -> ColorF {
      return ColorF{
          clamp01(static_cast<float>(qc.redF())),
          clamp01(static_cast<float>(qc.greenF())),
          clamp01(static_cast<float>(qc.blueF())),
          clamp01(static_cast<float>(qc.alphaF())),
      };
    };

    auto setSwatch = [](QPushButton *button, const QColor &c) {
      ui::configureColorSwatchButton(button, c);
    };

    if (ref.sector < 0 || ref.surface < 0 ||
        static_cast<std::size_t>(ref.sector) >= level.sectors.size() ||
        static_cast<std::size_t>(ref.surface) >=
            level.sectors[static_cast<std::size_t>(ref.sector)]
                .surfaces.size()) {
      return;
    }

    const std::int32_t sectorIndex = ref.sector;
    const std::int32_t surfaceIndex = ref.surface;
    const Sector &sector = level.sectors[static_cast<std::size_t>(sectorIndex)];
    const Surface &surface =
        sector.surfaces[static_cast<std::size_t>(surfaceIndex)];

    addChild(root, "Sector", QString::number(sectorIndex));
    {
      const int layerCount = std::max(1, static_cast<int>(level.layers.size()));
      const int currentLayer = std::clamp(sector.layer, 0, layerCount - 1);

      auto *item = addChild(root, "Layer");
      auto *combo = new QComboBox(properties_);
      combo->setObjectName(QStringLiteral("prop.surface.layer"));
      for (int i = 0; i < layerCount; ++i) {
        QString layerName = QString("Layer%1").arg(i);
        if (static_cast<std::size_t>(i) < level.layers.size() &&
            !level.layers[static_cast<std::size_t>(i)].empty()) {
          layerName =
              QString::fromStdString(level.layers[static_cast<std::size_t>(i)]);
        }
        combo->addItem(QString("%1: %2").arg(i).arg(layerName), i);
      }
      combo->setCurrentIndex(currentLayer);
      properties_->setItemWidget(item, 1, combo);

      connect(combo, qOverload<int>(&QComboBox::currentIndexChanged), this,
              [this, combo, sectorIndex, applyUndoable](const int row) {
                if (!combo || row < 0) {
                  return;
                }
                const int newLayer = combo->itemData(row).toInt();
                if (applyUndoable(
                        "Surface: Layer changed",
                        editor::DocumentDirty::LevelGeometry,
                        [sectorIndex,
                         newLayer](editor::DocumentState &state) -> bool {
                          std::unordered_set<std::int32_t> sectors{};
                          for (const SurfaceRef &selected :
                               state.selection.surfaces) {
                            sectors.insert(selected.sector);
                          }
                          for (const editor::SurfaceVertexRef &selected :
                               state.selection.surfaceVertices) {
                            sectors.insert(selected.surface.sector);
                          }
                          if (sectors.empty()) {
                            sectors.insert(sectorIndex);
                          }

                          bool changed = false;
                          for (const std::int32_t index : sectors) {
                            if (index < 0 || static_cast<std::size_t>(index) >=
                                                 state.level.sectors.size()) {
                              continue;
                            }
                            Sector &owner =
                                state.level
                                    .sectors[static_cast<std::size_t>(index)];
                            if (owner.layer != newLayer) {
                              owner.layer = newLayer;
                              changed = true;
                            }
                          }
                          return changed;
                        },
                        true)) {
                  rebuildOutliner();
                }
              });
    }

    if (surfaceSelectionMode_ == SurfaceSelectionMode::face) {
      // Surface Flags (editable, hex)
      {
        auto *item = addChild(root, "Surface Flags");
        auto *row = new QWidget(properties_);
        auto *rowLayout = new QHBoxLayout(row);
        rowLayout->setContentsMargins(0, 0, 0, 0);
        rowLayout->setSpacing(4);

        auto *edit = new QLineEdit(row);
        edit->setText(hex(static_cast<std::uint32_t>(surface.surfaceFlags)));
        rowLayout->addWidget(edit, 1);

        auto *pickBtn = new QToolButton(row);
        pickBtn->setText("...");
        pickBtn->setToolTip("Edit flags");
        pickBtn->setAutoRaise(true);
        pickBtn->setFocusPolicy(Qt::NoFocus);
        pickBtn->setFocusPolicy(Qt::NoFocus);
        rowLayout->addWidget(pickBtn);
        openAuxiliaryEditorOnDoubleClick(edit, pickBtn);

        properties_->setItemWidget(item, 1, row);
        connect(
            edit, &QLineEdit::editingFinished, this,
            [this, edit, sectorIndex, surfaceIndex, hex, applyUndoable]() {
              const Level &lvl = document_.level();
              if (sectorIndex < 0 || surfaceIndex < 0 ||
                  static_cast<std::size_t>(sectorIndex) >= lvl.sectors.size() ||
                  static_cast<std::size_t>(surfaceIndex) >=
                      lvl.sectors[static_cast<std::size_t>(sectorIndex)]
                          .surfaces.size()) {
                return;
              }

              const Surface &surf =
                  lvl.sectors[static_cast<std::size_t>(sectorIndex)]
                      .surfaces[static_cast<std::size_t>(surfaceIndex)];
              const QString cur =
                  hex(static_cast<std::uint32_t>(surf.surfaceFlags));
              const QString next = edit->text().trimmed();

              const std::optional<std::uint32_t> parsed =
                  sed::text::parseHex(next.toStdString());
              if (!parsed) {
                edit->setText(cur);
                return;
              }

              (void)applyUndoable(
                  "Surface: Flags changed",
                  editor::DocumentDirty::LevelGeometry,
                  [&](editor::DocumentState &state) -> bool {
                    const std::int32_t flags =
                        static_cast<std::int32_t>(*parsed);

                    bool any = false;
                    for (const SurfaceRef &refSel : state.selection.surfaces) {
                      if (refSel.sector < 0 || refSel.surface < 0) {
                        continue;
                      }

                      const std::size_t secIdx =
                          static_cast<std::size_t>(refSel.sector);
                      if (secIdx >= state.level.sectors.size()) {
                        continue;
                      }

                      Sector &sec = state.level.sectors[secIdx];
                      const std::size_t surfIdx =
                          static_cast<std::size_t>(refSel.surface);
                      if (surfIdx >= sec.surfaces.size()) {
                        continue;
                      }

                      Surface &surfMut = sec.surfaces[surfIdx];
                      if (surfMut.surfaceFlags != flags) {
                        surfMut.surfaceFlags = flags;
                        any = true;
                      }
                    }

                    return any;
                  });
            });

        connect(
            pickBtn, &QToolButton::clicked, this,
            [this, edit, openFlagEditor, levelKind = level.kind,
             currentFlags =
                 static_cast<std::uint32_t>(surface.surfaceFlags)]() {
              if (!edit) {
                return;
              }

              constexpr std::uint32_t kSurfaceFloor = 0x01u;
              constexpr std::uint32_t kSurfaceCollision = 0x04u;
              constexpr std::uint32_t kSurfaceAetherium = 0x80u;
              constexpr std::uint32_t kSurfaceSkyHorizon = 0x200u;
              constexpr std::uint32_t kSurfaceSkyCeiling = 0x400u;
              constexpr std::uint32_t kSurfaceKillFloor = 0x1000u;
              constexpr std::uint32_t kSurfaceClimbable = 0x2000u;
              constexpr std::uint32_t kSurfaceTrack = 0x4000u;
              constexpr std::uint32_t kSurfaceChanged = 0x8000u;
              constexpr std::uint32_t kSurfaceMetal = 0x10000u;
              constexpr std::uint32_t kSurfaceWater = 0x20000u;
              constexpr std::uint32_t kSurfaceShallowWater = 0x40000u;
              constexpr std::uint32_t kSurfaceDirt = 0x80000u;
              constexpr std::uint32_t kSurfaceWeb = 0x100000u;
              constexpr std::uint32_t kSurfaceLava = 0x200000u;
              constexpr std::uint32_t kSurfaceSnow = 0x400000u;
              constexpr std::uint32_t kSurfaceWood = 0x800000u;
              constexpr std::uint32_t kSurfaceLedge = 0x1000000u;
              constexpr std::uint32_t kSurfaceWaterLedge = 0x2000000u;
              constexpr std::uint32_t kSurfaceQuarterRes = 0x4000000u;
              constexpr std::uint32_t kSurfaceQuadrupleRes = 0x8000000u;
              constexpr std::uint32_t kSurfaceWhipAim = 0x10000000u;
              constexpr std::uint32_t kSurfaceEcho = 0x20000000u;
              constexpr std::uint32_t kSurfaceWoodEcho = 0x40000000u;
              constexpr std::uint32_t kSurfaceDirtEcho = 0x80000000u;

              std::vector<FlagDef> defs{
                  {0x01u, "Floor",
                   "Surface is ground floor that thing objects can move on."},
                  {0x02u, "COG linked", "Surface is linked to COG logic."},
                  {0x04u, "Collision", "Solid surface with face collision."},
                  {0x08u, "No AI move",
                   "AI actors cannot move onto the surface."},
                  {0x10u, "2x scrolling size",
                   "Double scrolling surface size."},
                  {0x20u, "1/2 scrolling size", "Half scrolling surface size."},
                  {0x40u, "1/8 scrolling size",
                   "Eighth scrolling surface size."},
                  {0x200u, "Horizon sky",
                   "Surface is part of the horizon sky."},
                  {0x400u, "Ceiling sky",
                   "Surface is part of the ceiling sky."},
                  {0x800u, "Scrolling",
                   "Surface is currently scrolling/sliding."},
                  {0x10000u, "Metal", "Metal surface type."},
                  {0x40000u, "Shallow water",
                   "Surface plays shallow-water sounds."},
                  {0x80000u, "Dirt", "Dirt surface type."},
              };
              if (levelKind == ProjectType::IJIM) {
                defs.push_back({kSurfaceAetherium, "Aetherium",
                                "Aetherium surface type."});
                defs.push_back(
                    {kSurfaceKillFloor, "Kill floor",
                     "Actor, player, or weapon is destroyed on contact."});
                defs.push_back({kSurfaceClimbable, "Climbable",
                                "Player can climb this surface."});
                defs.push_back(
                    {kSurfaceTrack, "Track", "Railroad track surface type."});
                defs.push_back({kSurfaceChanged, "Changed",
                                "Engine flag marking the surface as changed.",
                                false});
                defs.push_back({kSurfaceWater, "Water", "Water surface type."});
                defs.push_back(
                    {kSurfaceWeb, "Web", "Spider web surface type."});
                defs.push_back({kSurfaceLava, "Lava", "Lava surface type."});
                defs.push_back({kSurfaceSnow, "Snow", "Snow surface type."});
                defs.push_back({kSurfaceWood, "Wood", "Wood surface type."});
                defs.push_back({kSurfaceLedge, "Ledge",
                                "Surface can be grabbed as a ledge."});
                defs.push_back({kSurfaceWaterLedge, "Water ledge",
                                "Climb-out ledge from water."});
                defs.push_back({kSurfaceQuarterRes, "1/4 scrolling size",
                                "Quarter scrolling surface size."});
                defs.push_back({kSurfaceQuadrupleRes, "4x scrolling size",
                                "Quadruple scrolling surface size."});
                defs.push_back({kSurfaceWhipAim, "Whip aim",
                                "Triggers whip-aim assistance."});
                defs.push_back(
                    {kSurfaceEcho, "Echo", "Echo / indoor surface type."});
                defs.push_back({kSurfaceWoodEcho, "Wood echo",
                                "Wood echo / hollow surface type."});
                defs.push_back({kSurfaceDirtEcho, "Dirt echo",
                                "Dirt echo / hollow surface type."});
              } else {
                defs.push_back({0x80u, "No damage from fall",
                                "Disable fall-damage on this surface."});
                defs.push_back({0x1000u, "Icy", "Icy surface type."});
                defs.push_back({0x2000u, "Very icy", "Very icy surface type."});
                defs.push_back({0x4000u, "Magsealed", "Reflects shots."});
                defs.push_back(
                    {kSurfaceWater, "Deep water", "Deep-water surface type."});
                defs.push_back({kSurfaceWeb, "Very deep water",
                                "Very deep-water surface type."});
              }

              auto normalizeSurfaceFlags = [levelKind](std::uint32_t &value) {
                constexpr std::uint32_t floorBit = 0x01u;
                constexpr std::uint32_t collisionBit = 0x04u;
                constexpr std::uint32_t killFloorBit = 0x1000u;
                constexpr std::uint32_t climbableBit = 0x2000u;
                constexpr std::uint32_t trackBit = 0x4000u;
                constexpr std::uint32_t ledgeBit = 0x1000000u;
                constexpr std::uint32_t waterLedgeBit = 0x2000000u;
                constexpr std::uint32_t whipAimBit = 0x10000000u;

                if ((value & floorBit) != 0u) {
                  value |= collisionBit;
                }

                if ((value & collisionBit) == 0u) {
                  value &= ~floorBit;
                  if (levelKind == ProjectType::IJIM) {
                    value &= ~(climbableBit | ledgeBit | waterLedgeBit);
                  }
                }

                if (levelKind == ProjectType::IJIM) {
                  if ((value & (killFloorBit | trackBit | whipAimBit)) != 0u) {
                    value |= floorBit;
                    value |= collisionBit;
                  }
                  if ((value & (climbableBit | ledgeBit | waterLedgeBit)) !=
                      0u) {
                    value |= collisionBit;
                  }
                  if ((value & floorBit) == 0u) {
                    value &= ~(killFloorBit | trackBit | whipAimBit);
                  }
                }
              };

              auto handleSurfaceFlagToggle = [levelKind, normalizeSurfaceFlags](
                                                 std::uint32_t &value,
                                                 const std::uint32_t bit,
                                                 const bool checked) {
                constexpr std::uint32_t skyHorizonBit = 0x200u;
                constexpr std::uint32_t skyCeilingBit = 0x400u;
                constexpr std::uint32_t metalBit = 0x10000u;
                constexpr std::uint32_t waterBit = 0x20000u;
                constexpr std::uint32_t shallowWaterBit = 0x40000u;
                constexpr std::uint32_t dirtBit = 0x80000u;
                constexpr std::uint32_t aetheriumBit = 0x80u;
                constexpr std::uint32_t webBit = 0x100000u;
                constexpr std::uint32_t lavaBit = 0x200000u;
                constexpr std::uint32_t snowBit = 0x400000u;
                constexpr std::uint32_t woodBit = 0x800000u;
                constexpr std::uint32_t echoBit = 0x20000000u;
                constexpr std::uint32_t woodEchoBit = 0x40000000u;
                constexpr std::uint32_t dirtEchoBit = 0x80000000u;
                constexpr std::uint32_t floorBit = 0x01u;
                constexpr std::uint32_t killFloorBit = 0x1000u;
                constexpr std::uint32_t trackBit = 0x4000u;
                constexpr std::uint32_t whipAimBit = 0x10000000u;

                auto clearBit = [&](const std::uint32_t flag) {
                  value &= ~flag;
                };

                auto isSurfaceTypeBit = [&](const std::uint32_t flag) -> bool {
                  switch (flag) {
                  case skyHorizonBit:
                  case skyCeilingBit:
                  case metalBit:
                  case shallowWaterBit:
                  case dirtBit:
                  case aetheriumBit:
                  case waterBit:
                  case webBit:
                  case lavaBit:
                  case snowBit:
                  case woodBit:
                  case echoBit:
                  case woodEchoBit:
                  case dirtEchoBit:
                    return true;
                  default:
                    return false;
                  }
                };

                if (checked && isSurfaceTypeBit(bit)) {
                  clearBit(skyHorizonBit);
                  clearBit(skyCeilingBit);
                  clearBit(metalBit);
                  clearBit(shallowWaterBit);
                  clearBit(dirtBit);

                  if (levelKind == ProjectType::IJIM) {
                    clearBit(aetheriumBit);
                    if (bit != lavaBit) {
                      clearBit(waterBit);
                    }
                    clearBit(webBit);
                    if (bit != waterBit) {
                      clearBit(lavaBit);
                    }
                    clearBit(snowBit);
                    clearBit(woodBit);
                    clearBit(echoBit);
                    clearBit(woodEchoBit);
                    clearBit(dirtEchoBit);
                  }

                  value |= bit;
                }

                if (!checked && levelKind == ProjectType::IJIM &&
                    bit == floorBit) {
                  value &= ~(killFloorBit | trackBit | whipAimBit);
                }

                normalizeSurfaceFlags(value);
              };

              const auto picked = openFlagEditor(
                  "Surface: Flags changed", currentFlags, defs,
                  handleSurfaceFlagToggle, normalizeSurfaceFlags);
              if (!picked) {
                return;
              }
              edit->setText(QString("0x%1").arg(*picked, 0, 16));
              QMetaObject::invokeMethod(edit, "editingFinished",
                                        Qt::DirectConnection);
            });
      }

      const bool isIJIM = level.kind == ProjectType::IJIM;

      // Render section (GEO + TEX mode)
      {
        auto *render = addGroup(nullptr, "Render");

        // Face Flags (editable, hex)
        {
          auto *item = addChild(render, "Face Flags");
          auto *row = new QWidget(properties_);
          auto *rowLayout = new QHBoxLayout(row);
          rowLayout->setContentsMargins(0, 0, 0, 0);
          rowLayout->setSpacing(4);

          auto *edit = new QLineEdit(row);
          edit->setText(hex(surface.faceFlags));
          rowLayout->addWidget(edit, 1);

          auto *pickBtn = new QToolButton(row);
          pickBtn->setText("...");
          pickBtn->setToolTip("Edit flags");
          pickBtn->setAutoRaise(true);
          rowLayout->addWidget(pickBtn);
          openAuxiliaryEditorOnDoubleClick(edit, pickBtn);

          properties_->setItemWidget(item, 1, row);
          connect(
              edit, &QLineEdit::editingFinished, this,
              [this, edit, sectorIndex, surfaceIndex, hex, applyUndoable]() {
                const Level &lvl = document_.level();
                if (sectorIndex < 0 || surfaceIndex < 0 ||
                    static_cast<std::size_t>(sectorIndex) >=
                        lvl.sectors.size() ||
                    static_cast<std::size_t>(surfaceIndex) >=
                        lvl.sectors[static_cast<std::size_t>(sectorIndex)]
                            .surfaces.size()) {
                  return;
                }

                const Surface &surf =
                    lvl.sectors[static_cast<std::size_t>(sectorIndex)]
                        .surfaces[static_cast<std::size_t>(surfaceIndex)];
                const QString cur = hex(surf.faceFlags);
                const QString next = edit->text().trimmed();

                const std::optional<std::uint32_t> parsed =
                    sed::text::parseHex(next.toStdString());
                if (!parsed) {
                  edit->setText(cur);
                  return;
                }

                (void)applyUndoable(
                    "Surface: Face flags changed",
                    editor::DocumentDirty::LevelGeometry,
                    [&](editor::DocumentState &state) -> bool {
                      std::uint32_t flags = *parsed;
                      if ((flags & sed::FF_Transluent) == 0u) {
                        // Z-write disabled without blending is almost always
                        // accidental; clearing it matches engine behavior where
                        // non-translucent faces are rendered opaque.
                        flags &= ~sed::FF_ZWriteDisabled;
                      }

                      bool any = false;
                      for (const SurfaceRef &refSel :
                           state.selection.surfaces) {
                        if (refSel.sector < 0 || refSel.surface < 0) {
                          continue;
                        }

                        const std::size_t secIdx =
                            static_cast<std::size_t>(refSel.sector);
                        if (secIdx >= state.level.sectors.size()) {
                          continue;
                        }

                        Sector &sec = state.level.sectors[secIdx];
                        const std::size_t surfIdx =
                            static_cast<std::size_t>(refSel.surface);
                        if (surfIdx >= sec.surfaces.size()) {
                          continue;
                        }

                        Surface &surfMut = sec.surfaces[surfIdx];
                        if (surfMut.faceFlags != flags) {
                          surfMut.faceFlags = flags;
                          any = true;
                        }
                      }

                      return any;
                    });
              });

          connect(
              pickBtn, &QToolButton::clicked, this,
              [this, edit, openFlagEditor, levelKind = level.kind,
               currentFlags = static_cast<std::uint32_t>(surface.faceFlags)]() {
                if (!edit) {
                  return;
                }

                std::vector<FlagDef> defs{
                    {sed::FF_DoubleSided, "Double sided",
                     "Disable backface culling (mostly for non-world "
                     "geometry)."},
                    {sed::FF_Transluent, "Translucent",
                     "Enable texture transparency blending."},
                    {sed::FF_TexClampX, "Clamp texture X",
                     "Clamp texture in U instead of wrapping."},
                    {sed::FF_TexClampY, "Clamp texture Y",
                     "Clamp texture in V instead of wrapping."},
                    {sed::FF_TexNoFiltering, "Disable filtering",
                     "Use nearest filtering (pixelated look)."},
                    {sed::FF_ZWriteDisabled, "Disable Z-write",
                     "Do not write this surface into depth buffer."},
                };
                if (levelKind == ProjectType::IJIM) {
                  defs.push_back({sed::FF_IJIM_3doLedge, "3DO ledge (IJIM)",
                                  "3DO face supports ledge interaction."});
                  defs.push_back(
                      {sed::FF_IJIM_FogEnabled, "Fog enabled (IJIM)",
                       "Enable fog blend on this face (non-sky surfaces)."});
                  defs.push_back(
                      {sed::FF_IJIM_3doWhipAim, "3DO whip aim (IJIM)",
                       "3DO face participates in whip-aim interactions."});
                }

                const auto picked = openFlagEditor(
                    "Surface: Face flags changed", currentFlags, defs);
                if (!picked) {
                  return;
                }
                edit->setText(QString("0x%1").arg(*picked, 0, 16));
                QMetaObject::invokeMethod(edit, "editingFinished",
                                          Qt::DirectConnection);
              });
        }

        // Geometry (GEO mode)
        {
          auto *item = addChild(render, "Geometry");
          auto *combo = new QComboBox(properties_);
          combo->setObjectName(QStringLiteral("prop.surface.geometry"));
          combo->addItem("Not rendered", static_cast<int>(TGeoMode::NotDrawn));
          combo->addItem("Draw vertices", static_cast<int>(TGeoMode::Vertex));
          combo->addItem("Draw wireframe",
                         static_cast<int>(TGeoMode::Wireframe));
          combo->addItem("Draw solid", static_cast<int>(TGeoMode::Solid));
          combo->addItem("Draw textured", static_cast<int>(TGeoMode::Texture));

          const int cur = std::clamp(surface.geo, 0, 4);
          for (int i = 0; i < combo->count(); ++i) {
            if (combo->itemData(i).toInt() == cur) {
              combo->setCurrentIndex(i);
              break;
            }
          }

          properties_->setItemWidget(item, 1, combo);
          connect(combo, qOverload<int>(&QComboBox::currentIndexChanged), this,
                  [this, combo, sectorIndex, surfaceIndex, applyUndoable](int) {
                    const int v = combo->currentData().toInt();
                    (void)applyUndoable(
                        "Surface: Geometry mode changed",
                        editor::DocumentDirty::LevelGeometry,
                        [&](editor::DocumentState &state) -> bool {
                          bool any = false;
                          for (const SurfaceRef &refSel :
                               state.selection.surfaces) {
                            if (refSel.sector < 0 || refSel.surface < 0) {
                              continue;
                            }

                            const std::size_t secIdx =
                                static_cast<std::size_t>(refSel.sector);
                            if (secIdx >= state.level.sectors.size()) {
                              continue;
                            }

                            Sector &sec = state.level.sectors[secIdx];
                            const std::size_t surfIdx =
                                static_cast<std::size_t>(refSel.surface);
                            if (surfIdx >= sec.surfaces.size()) {
                              continue;
                            }

                            sec.surfaces[surfIdx].geo = v;
                            any = true;
                          }

                          return any;
                        });
                  });
        }

        // Texture mode (TEX)
        if (!isIJIM) {
          auto *item = addChild(render, "Texture");
          auto *combo = new QComboBox(properties_);
          combo->addItem("Affine", 0);
          combo->addItem("Perspective", 1);
          combo->addItem("Perspective (2)", 2);
          combo->addItem("Perspective (3)", 3);

          const int cur = std::clamp(surface.tex, 0, 3);
          for (int i = 0; i < combo->count(); ++i) {
            if (combo->itemData(i).toInt() == cur) {
              combo->setCurrentIndex(i);
              break;
            }
          }

          properties_->setItemWidget(item, 1, combo);
          connect(combo, qOverload<int>(&QComboBox::currentIndexChanged), this,
                  [this, combo, sectorIndex, surfaceIndex, applyUndoable](int) {
                    const int v = combo->currentData().toInt();
                    (void)applyUndoable(
                        "Surface: Texture mode changed",
                        editor::DocumentDirty::LevelGeometry,
                        [&](editor::DocumentState &state) -> bool {
                          bool any = false;
                          for (const SurfaceRef &refSel :
                               state.selection.surfaces) {
                            if (refSel.sector < 0 || refSel.surface < 0) {
                              continue;
                            }

                            const std::size_t secIdx =
                                static_cast<std::size_t>(refSel.sector);
                            if (secIdx >= state.level.sectors.size()) {
                              continue;
                            }

                            Sector &sec = state.level.sectors[secIdx];
                            const std::size_t surfIdx =
                                static_cast<std::size_t>(refSel.surface);
                            if (surfIdx >= sec.surfaces.size()) {
                              continue;
                            }

                            sec.surfaces[surfIdx].tex = v;
                            any = true;
                          }

                          return any;
                        });
                  });
        }

        // Material (editable)
        {
          auto *item = addChild(render, "Material");
          auto *row = new QWidget(properties_);
          auto *rowLayout = new QHBoxLayout(row);
          rowLayout->setContentsMargins(0, 0, 0, 0);
          rowLayout->setSpacing(4);

          auto *previewBtn = new QToolButton(row);
          ui::configureMaterialPreviewButton(previewBtn);
          updateMaterialPreviewButton(previewBtn,
                                      QString::fromStdString(surface.material));
          rowLayout->addWidget(previewBtn);

          auto *edit = new QLineEdit(row);
          edit->setText(QString::fromStdString(surface.material));
          rowLayout->addWidget(edit, 1);

          auto *pickBtn = new QToolButton(row);
          pickBtn->setText("...");
          pickBtn->setToolTip("Pick material from resources");
          pickBtn->setAutoRaise(true);
          rowLayout->addWidget(pickBtn);
          openAuxiliaryEditorOnDoubleClick(edit, pickBtn);

          properties_->setItemWidget(item, 1, row);
          connect(edit, &QLineEdit::editingFinished, this,
                  [this, edit, sectorIndex, surfaceIndex, applyUndoable]() {
                    const Level &lvl = document_.level();
                    if (sectorIndex < 0 || surfaceIndex < 0 ||
                        static_cast<std::size_t>(sectorIndex) >=
                            lvl.sectors.size() ||
                        static_cast<std::size_t>(surfaceIndex) >=
                            lvl.sectors[static_cast<std::size_t>(sectorIndex)]
                                .surfaces.size()) {
                      return;
                    }

                    const Surface &surf =
                        lvl.sectors[static_cast<std::size_t>(sectorIndex)]
                            .surfaces[static_cast<std::size_t>(surfaceIndex)];
                    const QString cur = QString::fromStdString(surf.material);
                    const QString next = edit->text().trimmed();

                    const std::string nextStd = next.toStdString();
                    (void)applyUndoable(
                        "Surface: Material changed",
                        editor::DocumentDirty::LevelGeometry,
                        [&](editor::DocumentState &state) -> bool {
                          bool any = false;
                          for (const SurfaceRef &refSel :
                               state.selection.surfaces) {
                            if (refSel.sector < 0 || refSel.surface < 0) {
                              continue;
                            }

                            const std::size_t secIdx =
                                static_cast<std::size_t>(refSel.sector);
                            if (secIdx >= state.level.sectors.size()) {
                              continue;
                            }

                            Sector &sec = state.level.sectors[secIdx];
                            const std::size_t surfIdx =
                                static_cast<std::size_t>(refSel.surface);
                            if (surfIdx >= sec.surfaces.size()) {
                              continue;
                            }

                            Surface &surfMut = sec.surfaces[surfIdx];
                            if (surfMut.material != nextStd) {
                              surfMut.material = nextStd;
                              any = true;
                            }
                          }

                          return any;
                        });
                  });

          connect(
              edit, &QLineEdit::textChanged, this,
              [updateMaterialPreviewButton, previewBtn](const QString &text) {
                updateMaterialPreviewButton(previewBtn, text);
              });

          auto pickMaterial = [this, edit, openResourcePicker]() {
            if (!edit) {
              return;
            }
            const auto picked =
                openResourcePicker(ui::ResourcePickerKind::material,
                                   edit->text(), "Pick Material");
            if (!picked) {
              return;
            }

            edit->setText(*picked);
            QMetaObject::invokeMethod(edit, "editingFinished",
                                      Qt::DirectConnection);
          };
          connect(previewBtn, &QToolButton::clicked, this, pickMaterial);
          connect(pickBtn, &QToolButton::clicked, this, pickMaterial);
        }
      }

      // Lighting section (type + extra light)
      {
        auto *lighting = addGroup(nullptr, "Lighting");

        // Light mode (fully lit / not lit / diffuse / gouraud)
        {
          auto *item = addChild(lighting, "Type");
          auto *combo = new QComboBox(properties_);
          combo->addItem("Fully lit", static_cast<int>(TLightMode::None));
          combo->addItem("Not lit", static_cast<int>(TLightMode::Lit));
          combo->addItem("Diffuse", static_cast<int>(TLightMode::Diffuse));
          combo->addItem("Gouraud", static_cast<int>(TLightMode::Gouraud));

          const int cur = std::clamp(surface.light, 0, 3);
          for (int i = 0; i < combo->count(); ++i) {
            if (combo->itemData(i).toInt() == cur) {
              combo->setCurrentIndex(i);
              break;
            }
          }

          properties_->setItemWidget(item, 1, combo);
          connect(combo, qOverload<int>(&QComboBox::currentIndexChanged), this,
                  [this, combo, sectorIndex, surfaceIndex, applyUndoable](int) {
                    const int v = combo->currentData().toInt();
                    (void)applyUndoable(
                        "Surface: Light mode changed",
                        editor::DocumentDirty::LevelGeometry,
                        [&](editor::DocumentState &state) -> bool {
                          bool any = false;
                          for (const SurfaceRef &refSel :
                               state.selection.surfaces) {
                            if (refSel.sector < 0 || refSel.surface < 0) {
                              continue;
                            }

                            const std::size_t secIdx =
                                static_cast<std::size_t>(refSel.sector);
                            if (secIdx >= state.level.sectors.size()) {
                              continue;
                            }

                            Sector &sec = state.level.sectors[secIdx];
                            const std::size_t surfIdx =
                                static_cast<std::size_t>(refSel.surface);
                            if (surfIdx >= sec.surfaces.size()) {
                              continue;
                            }

                            sec.surfaces[surfIdx].light = v;
                            any = true;
                          }

                          return any;
                        });
                  });
        }

        // Extra light. JK/MotS stores one signed scalar. IJIM stores
        // signed RGB(A). QColor shows its absolute RGB magnitude while
        // the popup intensity carries brighten/dim polarity and stays
        // synchronized with HSV Value.
        std::vector<SurfaceRef> targetSurfaceRefs;
        if (!document_.selection().surfaces.empty()) {
          targetSurfaceRefs.assign(document_.selection().surfaces.begin(),
                                   document_.selection().surfaces.end());
        } else if (sectorIndex >= 0 && surfaceIndex >= 0) {
          targetSurfaceRefs.push_back(SurfaceRef{ sectorIndex, surfaceIndex });
        }
        auto originalSurfaceColors =
            std::make_shared<std::vector<std::pair<SurfaceRef, ColorF>>>();
        auto captureSurfaceOriginal =
            [targetSurfaceRefs, originalSurfaceColors](
                const editor::DocumentState &state) {
              originalSurfaceColors->clear();
              for (const SurfaceRef ref : targetSurfaceRefs) {
                if (ref.sector < 0 || ref.surface < 0 ||
                    static_cast<std::size_t>(ref.sector) >=
                        state.level.sectors.size()) {
                  continue;
                }
                const Sector &sector = state.level.sectors[
                    static_cast<std::size_t>(ref.sector)];
                if (static_cast<std::size_t>(ref.surface) >=
                    sector.surfaces.size()) {
                  continue;
                }
                originalSurfaceColors->emplace_back(
                    ref, sector.surfaces[static_cast<std::size_t>(ref.surface)]
                             .extraLight);
              }
            };
        auto restoreSurfaceOriginal =
            [originalSurfaceColors](editor::DocumentState &state) -> bool {
              bool changed = false;
              for (const auto &[ref, original] : *originalSurfaceColors) {
                if (ref.sector < 0 || ref.surface < 0 ||
                    static_cast<std::size_t>(ref.sector) >=
                        state.level.sectors.size()) {
                  continue;
                }
                Sector &sector = state.level.sectors[
                    static_cast<std::size_t>(ref.sector)];
                if (static_cast<std::size_t>(ref.surface) >=
                    sector.surfaces.size()) {
                  continue;
                }
                Surface &surface = sector.surfaces[
                    static_cast<std::size_t>(ref.surface)];
                changed = surface.extraLight != original || changed;
                surface.extraLight = original;
              }
              return changed;
            };
        auto applySignedSurfaceExtra =
            [targetSurfaceRefs](editor::DocumentState &state,
                                const double value) -> bool {
          constexpr double maxFloat =
              static_cast<double>(std::numeric_limits<float>::max());
          if (!std::isfinite(value) || value < -maxFloat || value > maxFloat) {
            return false;
          }
          const float intensity = static_cast<float>(value);
          const ColorF next{intensity, intensity, intensity, 1.0f};
          bool any = false;
          for (const SurfaceRef &refSel : targetSurfaceRefs) {
            if (refSel.sector < 0 || refSel.surface < 0) {
              continue;
            }

            const std::size_t secIdx = static_cast<std::size_t>(refSel.sector);
            if (secIdx >= state.level.sectors.size()) {
              continue;
            }

            Sector &sec = state.level.sectors[secIdx];
            const std::size_t surfIdx =
                static_cast<std::size_t>(refSel.surface);
            if (surfIdx >= sec.surfaces.size()) {
              continue;
            }

            Surface &surf = sec.surfaces[surfIdx];
            if (surf.extraLight.r == next.r && surf.extraLight.g == next.g &&
                surf.extraLight.b == next.b) {
              continue;
            }

            const float previousAlpha = surf.extraLight.a;
            surf.extraLight = next;
            surf.extraLight.a = previousAlpha;
            any = true;
          }
          return any;
        };

        auto addSignedSurfaceExtraEditor = [&](const QString &label) {
          auto *item = addChild(lighting, label);
          auto *scrub = new ui::ScrubFloatEdit(properties_);
          scrub->setObjectName(
              QStringLiteral("surfaceExtraLightIntensityEdit"));
          scrub->setToolTip(
              "Add to or subtract from the surface's baked vertex lighting. "
              "Finite signed float values are supported; the renderer may clamp "
              "evaluated lighting for display.");
          scrub->setDecimals(3);
          scrub->setSingleStep(0.01);
          constexpr double maxFloat =
              static_cast<double>(std::numeric_limits<float>::max());
          scrub->setRange(-maxFloat, maxFloat);
          const double displayIntensity =
              (static_cast<double>(surface.extraLight.r) +
               static_cast<double>(surface.extraLight.g) +
               static_cast<double>(surface.extraLight.b)) /
              3.0;
          scrub->setValue(std::isfinite(displayIntensity) ? displayIntensity : 0.0);
          properties_->setItemWidget(item, 1, scrub);

          connect(scrub, &ui::ScrubFloatEdit::valueCommitted, this,
                  [applyUndoable, applySignedSurfaceExtra](const double value) {
                    (void)applyUndoable(
                        "Surface: Extra light changed",
                        editor::DocumentDirty::LevelGeometry,
                        [&](editor::DocumentState &state) -> bool {
                          return applySignedSurfaceExtra(state, value);
                        });
                  });
          connectLiveScrub(scrub, "Surface: Extra light changed",
                           editor::DocumentDirty::LevelGeometry,
                           applySignedSurfaceExtra, false, true,
                           captureSurfaceOriginal, restoreSurfaceOriginal);
        };

        if (isIJIM) {
          const ui::surface::SignedExtraLightColor signedColor =
              ui::surface::splitSignedExtraLightColor(surface.extraLight);
          const bool supportsAlpha =
              ui::surface::surfaceExtraLightSupportsAlpha(surface);
          auto *item = addChild(lighting, "Color");
          auto *button = new QPushButton(properties_);
          button->setObjectName(QStringLiteral("surfaceExtraLightColorButton"));
          button->setProperty("sed.surfaceExtraLightSupportsAlpha",
                              supportsAlpha);
          button->setProperty("sed.surfaceExtraLightIntensity",
                              signedColor.intensity);
          button->setToolTip(QStringLiteral(
              "Edit the surface extra-light color and signed intensity. "
              "Negative intensity dims baked/Gouraud lighting."));
          const QColor displayColor =
              colorToQ(signedColor.displayColor, supportsAlpha);
          setSwatch(button, displayColor);
          properties_->setItemWidget(item, 1, button);
          connectLiveSignedColor(
              button, displayColor, signedColor.intensity, supportsAlpha,
              "Surface: Extra light changed",
              editor::DocumentDirty::LevelGeometry,
              [qToColor](editor::DocumentState &state, const QColor &color,
                         const qreal intensity) -> bool {
                const ColorF chosenDisplayColor = qToColor(color);
                bool any = false;
                for (const SurfaceRef &refSel : state.selection.surfaces) {
                  if (refSel.sector < 0 || refSel.surface < 0) {
                    continue;
                  }

                  const std::size_t secIdx =
                      static_cast<std::size_t>(refSel.sector);
                  if (secIdx >= state.level.sectors.size()) {
                    continue;
                  }

                  Sector &sec = state.level.sectors[secIdx];
                  const std::size_t surfIdx =
                      static_cast<std::size_t>(refSel.surface);
                  if (surfIdx >= sec.surfaces.size()) {
                    continue;
                  }

                  Surface &surf = sec.surfaces[surfIdx];
                  const bool editAlpha =
                      ui::surface::surfaceExtraLightSupportsAlpha(surf);
                  const float alpha =
                      editAlpha ? chosenDisplayColor.a : surf.extraLight.a;
                  const ColorF next = ui::surface::joinSignedExtraLightColor(
                      chosenDisplayColor, static_cast<float>(intensity), alpha);

                  if (surf.extraLight.r == next.r &&
                      surf.extraLight.g == next.g &&
                      surf.extraLight.b == next.b &&
                      surf.extraLight.a == next.a) {
                    continue;
                  }

                  surf.extraLight = next;
                  if (editAlpha && surf.extraLight.a < 1.0f) {
                    surf.faceFlags |= sed::FF_Transluent;
                  }
                  any = true;
                }

                return any;
              });
        } else {
          addSignedSurfaceExtraEditor("Extra");
        }
      }

      // Adjoin section
      {
        auto *adjoin = addGroup(nullptr, "Adjoin");

        auto *mirror = addChild(adjoin, "Mirror");
        if (surface.adjoin.has_value()) {
          mirror->setText(1, QString("%1:%2")
                                 .arg(surface.adjoin->sector)
                                 .arg(surface.adjoin->surface));
          mirror->setData(
              0, kPropertyActionRole,
              static_cast<int>(PropertyAction::selectMirrorSurface));
          mirror->setData(0, kPropertySectorRole, surface.adjoin->sector);
          mirror->setData(0, kPropertySurfaceRole, surface.adjoin->surface);
        } else {
          mirror->setText(1, "None");
        }

        // Adjoin Flags (editable, hex)
        {
          auto *item = addChild(adjoin, "Flags");
          auto *row = new QWidget(properties_);
          auto *rowLayout = new QHBoxLayout(row);
          rowLayout->setContentsMargins(0, 0, 0, 0);
          rowLayout->setSpacing(4);

          auto *edit = new QLineEdit(row);
          edit->setText(hex(static_cast<std::uint32_t>(surface.adjoinFlags)));
          rowLayout->addWidget(edit, 1);

          auto *pickBtn = new QToolButton(row);
          pickBtn->setText("...");
          pickBtn->setToolTip("Edit flags");
          pickBtn->setAutoRaise(true);
          rowLayout->addWidget(pickBtn);
          openAuxiliaryEditorOnDoubleClick(edit, pickBtn);

          properties_->setItemWidget(item, 1, row);
          connect(
              edit, &QLineEdit::editingFinished, this,
              [this, edit, sectorIndex, surfaceIndex, hex, applyUndoable]() {
                const Level &lvl = document_.level();
                if (sectorIndex < 0 || surfaceIndex < 0 ||
                    static_cast<std::size_t>(sectorIndex) >=
                        lvl.sectors.size() ||
                    static_cast<std::size_t>(surfaceIndex) >=
                        lvl.sectors[static_cast<std::size_t>(sectorIndex)]
                            .surfaces.size()) {
                  return;
                }

                const Surface &surf =
                    lvl.sectors[static_cast<std::size_t>(sectorIndex)]
                        .surfaces[static_cast<std::size_t>(surfaceIndex)];
                const QString cur =
                    hex(static_cast<std::uint32_t>(surf.adjoinFlags));
                const QString next = edit->text().trimmed();

                const std::optional<std::uint32_t> parsed =
                    sed::text::parseHex(next.toStdString());
                if (!parsed) {
                  edit->setText(cur);
                  return;
                }

                (void)applyUndoable(
                    "Surface: Adjoin flags changed",
                    editor::DocumentDirty::LevelGeometry,
                    [&](editor::DocumentState &state) -> bool {
                      const std::int32_t flags =
                          static_cast<std::int32_t>(*parsed);

                      bool any = false;
                      for (const SurfaceRef &refSel :
                           state.selection.surfaces) {
                        if (refSel.sector < 0 || refSel.surface < 0) {
                          continue;
                        }

                        const std::size_t secIdx =
                            static_cast<std::size_t>(refSel.sector);
                        if (secIdx >= state.level.sectors.size()) {
                          continue;
                        }

                        Sector &sec = state.level.sectors[secIdx];
                        const std::size_t surfIdx =
                            static_cast<std::size_t>(refSel.surface);
                        if (surfIdx >= sec.surfaces.size()) {
                          continue;
                        }

                        Surface &surfMut = sec.surfaces[surfIdx];
                        if (surfMut.adjoinFlags != flags) {
                          surfMut.adjoinFlags = flags;
                          any = true;
                        }
                      }

                      return any;
                    });
              });

          connect(
              pickBtn, &QToolButton::clicked, this,
              [this, edit, openFlagEditor,
               currentFlags =
                   static_cast<std::uint32_t>(surface.adjoinFlags)]() {
                if (!edit) {
                  return;
                }
                const std::vector<FlagDef> defs{
                    {static_cast<std::uint32_t>(sed::SAF_Visible), "Visible",
                     "Render geometry through this adjoin."},
                    {static_cast<std::uint32_t>(sed::SAF_Move),
                     "Allow movement",
                     "Allow movement through the adjoin portal."},
                    {static_cast<std::uint32_t>(sed::SAF_AllowSoundPass),
                     "Allow sound pass",
                     "Allow sound to propagate through the adjoin."},
                    {static_cast<std::uint32_t>(sed::SAF_NoAiMove),
                     "No AI move",
                     "Block movement for AI actors (requires Move)."},
                    {static_cast<std::uint32_t>(sed::SAF_NoPlayerMove),
                     "No player move",
                     "Block movement for player (requires Move)."},
                    {static_cast<std::uint32_t>(sed::SAF_BlockLight),
                     "Block light",
                     "SED flag: block baked light transfer across the adjoin."},
                };
                const auto picked = openFlagEditor(
                    "Surface: Adjoin flags changed", currentFlags, defs);
                if (!picked) {
                  return;
                }
                edit->setText(QString("0x%1").arg(*picked, 0, 16));
                QMetaObject::invokeMethod(edit, "editingFinished",
                                          Qt::DirectConnection);
              });
        }
      }

    } else if (surfaceSelectionMode_ == SurfaceSelectionMode::edge) {
      if (surface.vertices.size() >= 2 && !sector.vertices.empty()) {
        auto distSqrPointToSegment3 = [](const TVector &p, const TVector &a,
                                         const TVector &b) -> double {
          const double abx = b.x - a.x;
          const double aby = b.y - a.y;
          const double abz = b.z - a.z;
          const double apx = p.x - a.x;
          const double apy = p.y - a.y;
          const double apz = p.z - a.z;

          const double abLen2 = abx * abx + aby * aby + abz * abz;
          if (abLen2 <= sed::math::kTinyEpsilon) {
            return apx * apx + apy * apy + apz * apz;
          }

          double t = (apx * abx + apy * aby + apz * abz) / abLen2;
          t = std::clamp(t, 0.0, 1.0);

          const double cx = a.x + abx * t;
          const double cy = a.y + aby * t;
          const double cz = a.z + abz * t;
          const double dx = p.x - cx;
          const double dy = p.y - cy;
          const double dz = p.z - cz;
          return dx * dx + dy * dy + dz * dz;
        };

        std::size_t edgeIndex = 0;
        if (haveLastSurfaceClick_ && lastSurfaceClickSurface_ == ref) {
          const TVector p = lastSurfaceClickPos_;
          double bestD2 = std::numeric_limits<double>::infinity();

          for (std::size_t i = 0; i < surface.vertices.size(); ++i) {
            const std::int32_t aidx = surface.vertices[i];
            const std::int32_t bidx =
                surface.vertices[(i + 1) % surface.vertices.size()];
            if (aidx < 0 || bidx < 0 ||
                static_cast<std::size_t>(aidx) >= sector.vertices.size() ||
                static_cast<std::size_t>(bidx) >= sector.vertices.size()) {
              continue;
            }

            const Vertex &va = sector.vertices[static_cast<std::size_t>(aidx)];
            const Vertex &vb = sector.vertices[static_cast<std::size_t>(bidx)];
            const TVector a{va.x, va.y, va.z};
            const TVector b{vb.x, vb.y, vb.z};

            const double d2 = distSqrPointToSegment3(p, a, b);
            if (d2 < bestD2) {
              bestD2 = d2;
              edgeIndex = i;
            }
          }
        }

        const std::int32_t aidx = surface.vertices[edgeIndex];
        const std::int32_t bidx =
            surface.vertices[(edgeIndex + 1) % surface.vertices.size()];
        if (aidx >= 0 && bidx >= 0 &&
            static_cast<std::size_t>(aidx) < sector.vertices.size() &&
            static_cast<std::size_t>(bidx) < sector.vertices.size()) {
          const Vertex &va = sector.vertices[static_cast<std::size_t>(aidx)];
          const Vertex &vb = sector.vertices[static_cast<std::size_t>(bidx)];

          const double dx = vb.x - va.x;
          const double dy = vb.y - va.y;
          const double dz = vb.z - va.z;
          const double len = std::sqrt(dx * dx + dy * dy + dz * dz);

          auto* lengthItem = addChild(root, "Length", ui::formatUiNumber(len, 'f', 6));
          lengthItem->setData(1, kNumericPropertyValueRole, len);
        } else {
          addChild(root, "Length", "");
        }
      } else {
        addChild(root, "Length", "");
      }
    } else {
      if (surface.vertices.empty() || sector.vertices.empty()) {
        addChild(root, "Vertex", "None");
        return;
      }

      std::size_t localIndex = 0;
      if (haveLastSurfaceClick_ && lastSurfaceClickSurface_ == ref) {
        const TVector p = lastSurfaceClickPos_;
        double bestD2 = std::numeric_limits<double>::infinity();

        for (std::size_t i = 0; i < surface.vertices.size(); ++i) {
          const std::int32_t vidx = surface.vertices[i];
          if (vidx < 0 ||
              static_cast<std::size_t>(vidx) >= sector.vertices.size()) {
            continue;
          }

          const Vertex &v = sector.vertices[static_cast<std::size_t>(vidx)];
          const double dx = p.x - v.x;
          const double dy = p.y - v.y;
          const double dz = p.z - v.z;
          const double d2 = dx * dx + dy * dy + dz * dz;
          if (d2 < bestD2) {
            bestD2 = d2;
            localIndex = i;
          }
        }
      }

      const std::int32_t vertexIndex = surface.vertices[localIndex];
      if (vertexIndex < 0 ||
          static_cast<std::size_t>(vertexIndex) >= sector.vertices.size()) {
        addChild(root, "Vertex", "None");
        return;
      }

      addChild(root, "Vertex", QString::number(vertexIndex));

      const Vertex &vtx =
          sector.vertices[static_cast<std::size_t>(vertexIndex)];

      // Position (editable)
      {
        auto *pos = addGroup(root, "Position");

        auto addAxis = [&](const QString &key,
                           const std::function<double(const Vertex &)> &get,
                           const std::function<void(Vertex &, double)> &set) {
          auto *item = addChild(pos, key);
          auto *scrub = new ui::ScrubFloatEdit(properties_);
          scrub->setDecimals(6);
          scrub->setSingleStep(0.01);
          scrub->setValue(get(vtx));
          properties_->setItemWidget(item, 1, scrub);

          auto onCommit = [this, sectorIndex, vertexIndex, set,
                           applyUndoable](double v) {
            (void)applyUndoable(
                "Vertex: Position changed",
                editor::DocumentDirty::LevelGeometry,
                [&](editor::DocumentState &state) -> bool {
                  if (sectorIndex < 0 || vertexIndex < 0 ||
                      static_cast<std::size_t>(sectorIndex) >=
                          state.level.sectors.size() ||
                      static_cast<std::size_t>(vertexIndex) >=
                          state.level
                              .sectors[static_cast<std::size_t>(sectorIndex)]
                              .vertices.size()) {
                    return false;
                  }

                  set(state.level.sectors[static_cast<std::size_t>(sectorIndex)]
                          .vertices[static_cast<std::size_t>(vertexIndex)],
                      v);
                  return true;
                });
          };

          connect(scrub, &ui::ScrubFloatEdit::valueCommitted, this, onCommit);
          connect(scrub, &ui::ScrubFloatEdit::scrubFinished, this, onCommit);
        };

        addAxis(
            "X", [](const Vertex &v) { return v.x; },
            [](Vertex &v, const double x) { v.x = x; });
        addAxis(
            "Y", [](const Vertex &v) { return v.y; },
            [](Vertex &v, const double y) { v.y = y; });
        addAxis(
            "Z", [](const Vertex &v) { return v.z; },
            [](Vertex &v, const double z) { v.z = z; });
      }

      // Vertex color (editable, per-surface)
      if (localIndex < surface.texVertices.size()) {
        const TexVertex &tv = surface.texVertices[localIndex];
        const bool isJKDF2 = level.kind == ProjectType::JKDF2;

        auto *col = addGroup(root, "Vertex Color");

        if (isJKDF2) {
          auto *item = addChild(col, "Intensity");
          auto *scrub = new ui::ScrubFloatEdit(properties_);
          scrub->setDecimals(3);
          scrub->setSingleStep(0.01);
          scrub->setRange(0.0, 1.0);
          scrub->setValue(tv.color.intensity());
          properties_->setItemWidget(item, 1, scrub);

          auto onCommit = [this, sectorIndex, surfaceIndex, localIndex,
                           applyUndoable](double v) {
            (void)applyUndoable(
                "Vertex: Color intensity changed",
                editor::DocumentDirty::LevelGeometry,
                [&](editor::DocumentState &state) -> bool {
                  auto applyOne =
                      [&](const editor::SurfaceVertexRef &ref) -> bool {
                    if (ref.surface.sector < 0 || ref.surface.surface < 0 ||
                        ref.localVertex < 0) {
                      return false;
                    }
                    if (static_cast<std::size_t>(ref.surface.sector) >=
                        state.level.sectors.size()) {
                      return false;
                    }

                    Sector &sector =
                        state.level.sectors[static_cast<std::size_t>(
                            ref.surface.sector)];
                    if (static_cast<std::size_t>(ref.surface.surface) >=
                        sector.surfaces.size()) {
                      return false;
                    }

                    Surface &surf = sector.surfaces[static_cast<std::size_t>(
                        ref.surface.surface)];
                    const std::size_t local =
                        static_cast<std::size_t>(ref.localVertex);
                    if (local >= surf.texVertices.size()) {
                      return false;
                    }

                    surf.texVertices[local].color =
                        intensityToColor(static_cast<float>(v));
                    surf.texVertices[local].color.a = 1.0f;
                    return true;
                  };

                  bool any = false;
                  if (!state.selection.surfaceVertices.empty()) {
                    for (const editor::SurfaceVertexRef &ref :
                         state.selection.surfaceVertices) {
                      any = applyOne(ref) || any;
                    }
                  } else {
                    const editor::SurfaceVertexRef ref{
                        SurfaceRef{sectorIndex, surfaceIndex},
                        static_cast<std::int32_t>(localIndex)};
                    any = applyOne(ref);
                  }

                  return any;
                });
          };

          connect(scrub, &ui::ScrubFloatEdit::valueCommitted, this, onCommit);
          connect(scrub, &ui::ScrubFloatEdit::scrubFinished, this, onCommit);
        } else {
          auto *item = addChild(col, "Color");
          auto *button = new QPushButton(properties_);
          setSwatch(button, colorToQ(tv.color, false));
          properties_->setItemWidget(item, 1, button);

          connectLiveColor(
              button, colorToQ(tv.color, false), false, "Vertex: Color changed",
              editor::DocumentDirty::LevelGeometry,
              [sectorIndex, surfaceIndex, localIndex, qToColor](
                  editor::DocumentState &state, const QColor &color) -> bool {
                const ColorF chosen = qToColor(color);
                auto applyOne =
                    [&](const editor::SurfaceVertexRef &ref) -> bool {
                  if (ref.surface.sector < 0 || ref.surface.surface < 0 ||
                      ref.localVertex < 0) {
                    return false;
                  }
                  if (static_cast<std::size_t>(ref.surface.sector) >=
                      state.level.sectors.size()) {
                    return false;
                  }

                  Sector &sector = state.level.sectors[static_cast<std::size_t>(
                      ref.surface.sector)];
                  if (static_cast<std::size_t>(ref.surface.surface) >=
                      sector.surfaces.size()) {
                    return false;
                  }

                  Surface &surf = sector.surfaces[static_cast<std::size_t>(
                      ref.surface.surface)];
                  const std::size_t local =
                      static_cast<std::size_t>(ref.localVertex);
                  if (local >= surf.texVertices.size()) {
                    return false;
                  }

                  ColorF &current = surf.texVertices[local].color;
                  if (current.r == chosen.r && current.g == chosen.g &&
                      current.b == chosen.b && current.a == 1.0f) {
                    return false;
                  }
                  current = chosen;
                  current.a = 1.0f;
                  return true;
                };

                bool any = false;
                if (!state.selection.surfaceVertices.empty()) {
                  for (const editor::SurfaceVertexRef &ref :
                       state.selection.surfaceVertices) {
                    any = applyOne(ref) || any;
                  }
                } else {
                  const editor::SurfaceVertexRef ref{
                      SurfaceRef{sectorIndex, surfaceIndex},
                      static_cast<std::int32_t>(localIndex)};
                  any = applyOne(ref);
                }

                return any;
              });
        }
      }
    }

    setLinkedCogList(buildLinkedSurfaceCogRefs(ref));

    properties_->expandAll();
    return;
  }

  if (!document_.selection().things.empty()) {
    {
      const std::int32_t thingIndex =
          activeSelectedThingIndex(level, document_.selection(), glView_);
      if (thingIndex < 0 ||
          static_cast<std::size_t>(thingIndex) >= level.things.size()) {
        return;
      }

      const Thing &thing = level.things[static_cast<std::size_t>(thingIndex)];
      const sed::assets::Templates *templatesDb =
          templates_.size() > 0 ? &templates_ : nullptr;

      TVector pos{thing.x, thing.y, thing.z};
      TVector pyr{};
      pyr.pitch = thing.pch;
      pyr.yaw = thing.yaw;
      pyr.roll = thing.rol;
      std::int32_t sector = thing.sector;

      if (transformPreview_.active) {
        if (!transformPreview_.rotating) {
          pos.x = pos.x + transformPreview_.translation.x;
          pos.y = pos.y + transformPreview_.translation.y;
          pos.z = pos.z + transformPreview_.translation.z;
        } else {
          TVector axis = transformPreview_.axis;
          const double axisLen = axis.Normalize();
          if (axisLen != 0.0 && std::abs(transformPreview_.angleDeg) > 1e-9) {
            const TMatrix43 rot = TMatrix43::CreateFromVectorAngle(
                axis, static_cast<float>(transformPreview_.angleDeg));
            const TVector pv = transformPreview_.pivot;

            TVector rel{pos.x - pv.x, pos.y - pv.y, pos.z - pv.z};
            rel = rot.TransformVector(rel);
            pos.x = rel.x + pv.x;
            pos.y = rel.y + pv.y;
            pos.z = rel.z + pv.z;

            const TMatrix43 oldRot = TMatrix43::CreateRotate(pyr);
            TMatrix43 newRot{};
            newRot.rvec = rot.TransformVector(oldRot.rvec);
            newRot.lvec = rot.TransformVector(oldRot.lvec);
            newRot.uvec = rot.TransformVector(oldRot.uvec);
            newRot.dvec = TVector::zero;
            newRot.Normalize();
            pyr = ui::transform::extractPYRCreateRotateCompatible(newRot);
          }
        }

        sector = sed::level::levelFindSectorForXYZ(level, pos.x, pos.y, pos.z);
      }

      QString thingType{};
      if (const TplValue *v =
              sed::level::thingFindValue(level, templatesDb, thing, "type")) {
        thingType = QString::fromStdString(v->asString()).trimmed();
      }
      if (thingType.isEmpty()) {
        thingType = "free";
      }

      const std::int32_t thingFlags =
          sed::level::thingFlags(level, templatesDb, thing);

      const auto anySelectedThingParameter =
          [this, &level, templatesDb,
           thingIndex](const std::string_view name, const bool requireLocal,
                       const bool requireInherited) -> bool {
        const auto matches = [&](const Thing &selectedThing) -> bool {
          return (!requireLocal || thing::parameters::localValue(
                                       selectedThing, name) != nullptr) &&
                 (!requireInherited ||
                  thing::parameters::inheritedValue(
                      level, templatesDb, selectedThing, name) != nullptr);
        };

        if (!document_.selection().things.empty()) {
          for (const std::int32_t selectedIndex :
               document_.selection().things) {
            if (selectedIndex >= 0 &&
                static_cast<std::size_t>(selectedIndex) < level.things.size() &&
                matches(
                    level.things[static_cast<std::size_t>(selectedIndex)])) {
              return true;
            }
          }
          return false;
        }

        return thingIndex >= 0 &&
               static_cast<std::size_t>(thingIndex) < level.things.size() &&
               matches(level.things[static_cast<std::size_t>(thingIndex)]);
      };

      auto addChild = [&](QTreeWidgetItem *parent, const QString &key,
                          const QString &value = {}) -> QTreeWidgetItem * {
        const int parentIndent =
            parent ? parent->data(0, kPropertyIndentRole).toInt() : -1;
        const int indent = parent ? (parentIndent + 1) : 0;

        auto *item = new QTreeWidgetItem(properties_);
        item->setText(0, QString(indent * 2, QChar(' ')) + key);
        item->setText(1, value);
        item->setData(0, kPropertyIndentRole, indent);
        return item;
      };

      auto addGroup = [&](QTreeWidgetItem *parent,
                          const QString &title) -> QTreeWidgetItem * {
        const int parentIndent =
            parent ? parent->data(0, kPropertyIndentRole).toInt() : -1;
        const int indent = parent ? (parentIndent + 1) : 0;

        auto *item = new QTreeWidgetItem(properties_);
        item->setText(0, QString(indent * 2, QChar(' ')) + title);
        item->setFirstColumnSpanned(true);
        item->setChildIndicatorPolicy(
            QTreeWidgetItem::DontShowIndicatorWhenChildless);
        item->setFlags(item->flags() & ~Qt::ItemIsSelectable);
        item->setData(0, kPropertyIndentRole, indent);
        QFont f = item->font(0);
        f.setBold(true);
        item->setFont(0, f);
        item->setExpanded(true);
        return item;
      };

      auto *root = addGroup(nullptr, "General");

      addChild(root, "Type", thingType);

      // Name (editable)
      {
        auto *item = addChild(root, "Name");
        auto *edit = new QLineEdit(properties_);
        edit->setObjectName("prop.thing.name");
        edit->setText(QString::fromStdString(thing.name));
        properties_->setItemWidget(item, 1, edit);
        connect(
            edit, &QLineEdit::editingFinished, this,
            [this, edit, thingIndex, applyUndoable]() {
              const Level &lvl = document_.level();
              if (thingIndex < 0 ||
                  static_cast<std::size_t>(thingIndex) >= lvl.things.size()) {
                return;
              }

              const Thing &t = lvl.things[static_cast<std::size_t>(thingIndex)];
              const QString cur = QString::fromStdString(t.name);
              const QString next = edit->text().trimmed();
              if (next == cur) {
                return;
              }

              const std::string nextStd = next.toStdString();
              (void)applyUndoable(
                  "Thing: Name changed", editor::DocumentDirty::Things,
                  [&](editor::DocumentState &state) -> bool {
                    auto applyOne = [&](Thing &th) -> bool {
                      if (th.name == nextStd) {
                        return false;
                      }
                      th.name = nextStd;
                      return true;
                    };

                    bool any = false;
                    if (!state.selection.things.empty()) {
                      for (const std::int32_t idx : state.selection.things) {
                        if (idx < 0 || static_cast<std::size_t>(idx) >=
                                           state.level.things.size()) {
                          continue;
                        }
                        any |= applyOne(
                            state.level.things[static_cast<std::size_t>(idx)]);
                      }
                      return any;
                    }

                    if (thingIndex < 0 ||
                        static_cast<std::size_t>(thingIndex) >=
                            state.level.things.size()) {
                      return false;
                    }
                    return applyOne(
                        state.level
                            .things[static_cast<std::size_t>(thingIndex)]);
                  });
            });
      }

      // Template (picker-only)
      {
        auto *item = addChild(root, "Template");
        auto *row = new QWidget(properties_);
        auto *rowLayout = new QHBoxLayout(row);
        rowLayout->setContentsMargins(0, 0, 0, 0);
        rowLayout->setSpacing(4);

        auto *edit = new QLineEdit(row);
        edit->setObjectName("prop.thing.template");
        edit->setReadOnly(true);
        edit->setText(QString::fromStdString(thing.templateName));
        rowLayout->addWidget(edit, 1);

        QStringList localRenderOverrides{};
        const auto collectLocalRenderOverrides =
            [&level, templatesDb,
             &localRenderOverrides](const Thing &selectedThing) {
              const auto add = [&localRenderOverrides](
                                   const std::string_view key) {
                const QString text = QString::fromLatin1(
                    key.data(), static_cast<int>(key.size()));
                if (!localRenderOverrides.contains(text)) {
                  localRenderOverrides.push_back(text);
                }
              };
              if (thing::parameters::localValue(selectedThing, "sprite")) {
                add("sprite");
              }
              if (thing::parameters::localValue(selectedThing, "model3d")) {
                add("model3d");
              } else if (!sed::level::thingFindValue(
                             level, templatesDb, selectedThing, "model3d") &&
                         thing::parameters::localValue(selectedThing,
                                                       "model3do")) {
                // model3do is the compatibility fallback only when model3d
                // is absent throughout the local/template lookup chain.
                add("model3do");
              }
            };
        if (!document_.selection().things.empty()) {
          for (const std::int32_t selectedIndex :
               document_.selection().things) {
            if (selectedIndex >= 0 &&
                static_cast<std::size_t>(selectedIndex) < level.things.size()) {
              collectLocalRenderOverrides(
                  level.things[static_cast<std::size_t>(selectedIndex)]);
            }
          }
        } else {
          collectLocalRenderOverrides(thing);
        }
        const QString localRenderOverrideSummary =
            localRenderOverrides.join(QStringLiteral(", "));
        if (!localRenderOverrideSummary.isEmpty()) {
          const QString overrideHelp = QStringLiteral(
              "Local render parameter(s) remain authoritative after a template "
              "change: %1. Restore those parameters to use the template's "
              "render resource.")
                                           .arg(localRenderOverrideSummary);
          item->setToolTip(0, overrideHelp);
          item->setToolTip(1, overrideHelp);
          edit->setToolTip(overrideHelp);
        } else {
          edit->setToolTip(
              "The selected template supplies this Thing's inherited render resource");
        }

        auto *pickBtn = new QToolButton(row);
        pickBtn->setText("...");
        pickBtn->setAutoRaise(true);
        pickBtn->setToolTip("Pick template from resources");
        rowLayout->addWidget(pickBtn);
        openAuxiliaryEditorOnDoubleClick(edit, pickBtn);

        properties_->setItemWidget(item, 1, row);
        connect(
            pickBtn, &QToolButton::clicked, this,
            [this, edit, thingIndex, templatesDb, localRenderOverrideSummary,
             applyUndoable, openResourcePicker]() {
              if (!edit) {
                return;
              }

              QSettings settings = settingsStore();
              const auto picked = openResourcePicker(
                  ui::ResourcePickerKind::template_, edit->text(),
                  "Pick Template",
                  settings.value("templates/allow_base_template_pick", false)
                      .toBool());
              if (!picked) {
                return;
              }

              const Level &lvl = document_.level();
              if (thingIndex < 0 ||
                  static_cast<std::size_t>(thingIndex) >= lvl.things.size()) {
                return;
              }

              const QString next = picked->trimmed();
              const bool overrideThingName = settings.value(
                  "things/template_pick_overrides_name", false).toBool();

              const std::string nextStd = next.toStdString();
              const bool changed = applyUndoable(
                  "Thing: Template changed", editor::DocumentDirty::Things,
                  [&](editor::DocumentState &state) -> bool {
                    auto applyOne = [&](Thing &th) -> bool {
                      return thing::applyThingTemplateSelection(
                          th, nextStd,
                          thing::templateThingBox(state.level, templatesDb,
                                                  nextStd),
                          overrideThingName);
                    };

                    bool any = false;
                    if (!state.selection.things.empty()) {
                      for (const std::int32_t idx : state.selection.things) {
                        if (idx < 0 || static_cast<std::size_t>(idx) >=
                                           state.level.things.size()) {
                          continue;
                        }
                        any |= applyOne(
                            state.level.things[static_cast<std::size_t>(idx)]);
                      }
                      return any;
                    }

                    if (thingIndex < 0 ||
                        static_cast<std::size_t>(thingIndex) >=
                            state.level.things.size()) {
                      return false;
                    }
                    return applyOne(
                        state.level
                            .things[static_cast<std::size_t>(thingIndex)]);
                  });

              if (changed) {
                edit->setText(next);
                if (!localRenderOverrideSummary.isEmpty() && statusBar()) {
                  statusBar()->showMessage(
                      QStringLiteral(
                          "Template changed; local render parameter(s) remain "
                          "authoritative: %1")
                          .arg(localRenderOverrideSummary),
                      6000);
                }
              }
            });
      }

      // Flags (editable, hex; stored as thing value)
      {
        auto *item = addChild(root, "Flags");
        auto *row = new QWidget(properties_);
        auto *rowLayout = new QHBoxLayout(row);
        rowLayout->setContentsMargins(0, 0, 0, 0);
        rowLayout->setSpacing(4);

        auto *edit = new QLineEdit(row);
        edit->setObjectName("prop.thing.flags");
        edit->setText(hex(static_cast<std::uint32_t>(thingFlags)));
        const TplValue *inheritedThingFlags = thing::parameters::inheritedValue(
            level, templatesDb, thing, "thingflags");
        const bool thingFlagsInheritedOnly = inheritedThingFlags != nullptr &&
            thing::parameters::localValue(thing, "thingflags") == nullptr;
        const bool anyInheritedThingFlags =
            anySelectedThingParameter("thingflags", false, true);
        const bool anyRestorableThingFlags =
            anySelectedThingParameter("thingflags", true, true);
        applyInheritedParameterAppearance(edit, item,
                                          thingFlagsInheritedOnly);
        edit->setToolTip(
            inheritedThingFlags
                ? QStringLiteral("Editing creates or updates a Thing override.")
                : QStringLiteral("Thing instance flags."));
        rowLayout->addWidget(edit, 1);

        auto *pickBtn = new QToolButton(row);
        pickBtn->setText("...");
        pickBtn->setToolTip("Edit flags");
        pickBtn->setAutoRaise(true);
        rowLayout->addWidget(pickBtn);
        openAuxiliaryEditorOnDoubleClick(edit, pickBtn);

        QToolButton *restoreFlagsBtn = nullptr;
        if (anyInheritedThingFlags) {
          restoreFlagsBtn = new QToolButton(row);
          restoreFlagsBtn->setObjectName(
              QStringLiteral("prop.thing.flags.restore"));
          restoreFlagsBtn->setText(QStringLiteral("\u21b6"));
          restoreFlagsBtn->setAutoRaise(true);
          restoreFlagsBtn->setEnabled(anyRestorableThingFlags);
          restoreFlagsBtn->setToolTip(
              anyRestorableThingFlags
                  ? QStringLiteral(
                        "Restore each selected Thing's own template flags.")
                  : QStringLiteral("All selected Things already use their "
                                   "template flags."));
          rowLayout->addWidget(restoreFlagsBtn);
        }

        properties_->setItemWidget(item, 1, row);
        connect(
            edit, &QLineEdit::editingFinished, this,
            [this, edit, thingIndex, hex, applyUndoable,
             levelKind = level.kind]() {
              const Level &lvl = document_.level();
              const sed::assets::Templates *templatesDb =
                  templates_.size() > 0 ? &templates_ : nullptr;
              if (thingIndex < 0 ||
                  static_cast<std::size_t>(thingIndex) >= lvl.things.size()) {
                return;
              }

              const Thing &t = lvl.things[static_cast<std::size_t>(thingIndex)];
              const QString cur = hex(static_cast<std::uint32_t>(
                  sed::level::thingFlags(lvl, templatesDb, t)));
              const QString next = edit->text().trimmed();
              if (next == cur) {
                return;
              }

              const std::optional<std::uint32_t> parsed =
                  sed::text::parseHex(next.toStdString());
              if (!parsed) {
                edit->setText(cur);
                return;
              }

              const std::uint32_t originalEditorValue =
                  static_cast<std::uint32_t>(
                      sed::level::thingFlags(lvl, templatesDb, t));
              const std::uint32_t editedValue = *parsed;
              (void)applyUndoable(
                  "Thing: Flags changed", editor::DocumentDirty::Things,
                  [thingIndex, templatesDb, originalEditorValue, editedValue,
                   levelKind](editor::DocumentState &state) -> bool {
                    auto applyOne = [&](Thing &th) -> bool {
                      const TplValue *semanticSource =
                          sed::level::thingFindValue(state.level, templatesDb,
                                                     th, "thingflags");
                      const std::uint32_t current = static_cast<std::uint32_t>(
                          sed::level::thingFlags(state.level, templatesDb, th));
                      std::optional<std::uint32_t> next{};
                      if (state.selection.things.size() <= 1u) {
                        if (current != editedValue) {
                          next = editedValue;
                        }
                      } else {
                        const std::vector<
                            thing::parameter_schema::FlagDefinition>
                            definitions =
                                thing::parameter_schema::flagDefinitions(
                                    levelKind,
                                    thing::parameter_schema::FlagFamily::thing);
                        next = thing::parameter_editor::
                            applyProtectedFlagEditDelta(
                                current, originalEditorValue, editedValue,
                                definitions);
                      }
                      if (!next) {
                        return false;
                      }
                      return thing::parameters::upsertOverrideDetailed(
                                 th, "thingflags",
                                 QStringLiteral("0x%1")
                                     .arg(*next, 0, 16)
                                     .toStdString(),
                                 semanticSource) ==
                             thing::parameters::OverrideWriteResult::changed;
                    };

                    bool any = false;
                    if (!state.selection.things.empty()) {
                      for (const std::int32_t idx : state.selection.things) {
                        if (idx < 0 || static_cast<std::size_t>(idx) >=
                                           state.level.things.size()) {
                          continue;
                        }
                        any |= applyOne(
                            state.level.things[static_cast<std::size_t>(idx)]);
                      }
                      return any;
                    }

                    if (thingIndex < 0 ||
                        static_cast<std::size_t>(thingIndex) >=
                            state.level.things.size()) {
                      return false;
                    }

                    return applyOne(
                        state.level
                            .things[static_cast<std::size_t>(thingIndex)]);
                  });
            });

        const QPointer<QLineEdit> guardedFlagsEdit(edit);
        connect(
            pickBtn, &QToolButton::clicked, this,
            [this, guardedFlagsEdit, openFlagEditor, thingIndex, templatesDb,
             applyUndoable, levelKind = level.kind]() {
              if (!guardedFlagsEdit) {
                return;
              }

              const Level &currentLevel = document_.level();
              if (thingIndex < 0 || static_cast<std::size_t>(thingIndex) >=
                                        currentLevel.things.size()) {
                return;
              }
              const Thing &activeThing =
                  currentLevel.things[static_cast<std::size_t>(thingIndex)];
              const std::uint32_t originalEditorValue =
                  static_cast<std::uint32_t>(sed::level::thingFlags(
                      currentLevel, templatesDb, activeThing));

              std::uint32_t displayedValue = originalEditorValue;
              TplValue displayedFlags{};
              displayedFlags.name = "thingflags";
              displayedFlags.type = ValueType::flag;
              if (displayedFlags.parse(
                      guardedFlagsEdit->text().trimmed().toStdString())) {
                displayedValue =
                    static_cast<std::uint32_t>(displayedFlags.intValue);
              }

              const std::vector<thing::parameter_schema::FlagDefinition>
                  schemaDefinitions = thing::parameter_schema::flagDefinitions(
                      levelKind, thing::parameter_schema::FlagFamily::thing);
              const std::vector<ui::query::LevelQueryFlagDef>
                  editorDefinitions =
                      thing::parameter_editor::flagEditorDefinitions(
                          schemaDefinitions);
              std::vector<FlagDef> defs{};
              defs.reserve(editorDefinitions.size());
              for (const ui::query::LevelQueryFlagDef &definition :
                   editorDefinitions) {
                defs.push_back(FlagDef{
                    definition.bit,
                    definition.name,
                    definition.description,
                    definition.editable,
                });
              }

              const auto picked =
                  openFlagEditor("Thing: Flags changed", displayedValue, defs);
              if (!picked) {
                return;
              }

              (void)applyUndoable(
                  "Thing: Flags changed", editor::DocumentDirty::Things,
                  [thingIndex, templatesDb, originalEditorValue,
                   editedValue = *picked,
                   levelKind](editor::DocumentState &state) -> bool {
                    auto applyOne = [&](Thing &target) -> bool {
                      const TplValue *semanticSource =
                          sed::level::thingFindValue(state.level, templatesDb,
                                                     target, "thingflags");
                      const std::uint32_t current =
                          static_cast<std::uint32_t>(sed::level::thingFlags(
                              state.level, templatesDb, target));
                      std::optional<std::uint32_t> next{};
                      if (state.selection.things.size() <= 1u) {
                        if (current != editedValue) {
                          next = editedValue;
                        }
                      } else {
                        const std::vector<
                            thing::parameter_schema::FlagDefinition>
                            definitions =
                                thing::parameter_schema::flagDefinitions(
                                    levelKind,
                                    thing::parameter_schema::FlagFamily::thing);
                        next = thing::parameter_editor::
                            applyProtectedFlagEditDelta(
                                current, originalEditorValue, editedValue,
                                definitions);
                      }
                      if (!next) {
                        return false;
                      }
                      return thing::parameters::upsertOverrideDetailed(
                                 target, "thingflags",
                                 QStringLiteral("0x%1")
                                     .arg(*next, 0, 16)
                                     .toStdString(),
                                 semanticSource) ==
                             thing::parameters::OverrideWriteResult::changed;
                    };

                    bool changed = false;
                    if (!state.selection.things.empty()) {
                      for (const std::int32_t index : state.selection.things) {
                        if (index < 0 || static_cast<std::size_t>(index) >=
                                             state.level.things.size()) {
                          continue;
                        }
                        changed =
                            applyOne(
                                state.level
                                    .things[static_cast<std::size_t>(index)]) ||
                            changed;
                      }
                      return changed;
                    }

                    if (thingIndex < 0 ||
                        static_cast<std::size_t>(thingIndex) >=
                            state.level.things.size()) {
                      return false;
                    }
                    return applyOne(
                        state.level
                            .things[static_cast<std::size_t>(thingIndex)]);
                  });
            });

        if (restoreFlagsBtn) {
          connect(
              restoreFlagsBtn, &QToolButton::clicked, this,
              [this, thingIndex, templatesDb, applyUndoable]() {
                (void)applyUndoable(
                    "Thing: Flags restored from template",
                    editor::DocumentDirty::Things,
                    [thingIndex,
                     templatesDb](editor::DocumentState &state) -> bool {
                      auto restoreOne = [&](const std::int32_t index) -> bool {
                        return removeThingParameterPreservingFrameSelection(
                            state, index, [&](Thing &value) {
                              if (!thing::parameters::inheritedValue(
                                      state.level, templatesDb, value,
                                      "thingflags")) {
                                return false;
                              }
                              return thing::parameters::removeOverride(
                                  value, "thingflags");
                            });
                      };

                      bool changed = false;
                      if (!state.selection.things.empty()) {
                        for (const std::int32_t index :
                             state.selection.things) {
                          if (index < 0 || static_cast<std::size_t>(index) >=
                                               state.level.things.size()) {
                            continue;
                          }
                          changed = restoreOne(index) || changed;
                        }
                        return changed;
                      }

                      if (thingIndex < 0 ||
                          static_cast<std::size_t>(thingIndex) >=
                              state.level.things.size()) {
                        return false;
                      }
                      return restoreOne(thingIndex);
                    });
              });
        }
      }

      // Sector (read-only; derived)
      auto *sectorItem = addChild(root, "Sector", QString::number(sector));
      sectorItem->setData(0, kPropertyActionRole,
                          static_cast<int>(PropertyAction::selectThingSector));
      sectorItem->setData(0, kPropertyThingRole, static_cast<int>(thingIndex));
      {
        const int layerCount =
            std::max(1, static_cast<int>(level.layers.size()));
        const int currentLayer = std::clamp(thing.layer, 0, layerCount - 1);

        auto *item = addChild(root, "Layer");
        auto *combo = new QComboBox(properties_);
        combo->setObjectName(QStringLiteral("prop.thing.layer"));
        combo->setProperty("sed.itemEditorLayerCombo", true);
        for (int i = 0; i < layerCount; ++i) {
          QString layerName = QString("Layer%1").arg(i);
          if (static_cast<std::size_t>(i) < level.layers.size()) {
            const std::string &raw = level.layers[static_cast<std::size_t>(i)];
            if (!raw.empty()) {
              layerName = QString::fromStdString(raw);
            }
          }
          combo->addItem(QString("%1: %2").arg(i).arg(layerName), i);
        }
        combo->setCurrentIndex(currentLayer);
        properties_->setItemWidget(item, 1, combo);

        connect(
            combo, qOverload<int>(&QComboBox::currentIndexChanged), this,
            [this, combo, thingIndex, applyUndoable](int row) {
              if (!combo || row < 0) {
                return;
              }
              const int newLayer = combo->itemData(row).toInt();
              if (applyUndoable(
                      "Thing: Layer changed", editor::DocumentDirty::Things,
                      [thingIndex,
                       newLayer](editor::DocumentState &state) -> bool {
                        auto applyOne = [newLayer](Thing &th) -> bool {
                          if (th.layer == newLayer) {
                            return false;
                          }
                          th.layer = newLayer;
                          return true;
                        };

                        bool any = false;
                        if (!state.selection.things.empty()) {
                          for (const std::int32_t idx :
                               state.selection.things) {
                            if (idx < 0 || static_cast<std::size_t>(idx) >=
                                               state.level.things.size()) {
                              continue;
                            }
                            any |= applyOne(
                                state.level
                                    .things[static_cast<std::size_t>(idx)]);
                          }
                          return any;
                        }

                        if (thingIndex < 0 ||
                            static_cast<std::size_t>(thingIndex) >=
                                state.level.things.size()) {
                          return false;
                        }

                        return applyOne(
                            state.level
                                .things[static_cast<std::size_t>(thingIndex)]);
                      },
                      true)) {
                rebuildOutliner();
              }
            });
      }
      // Position (editable)
      {
        auto *grp = addGroup(nullptr, "Position");

        auto addAxis = [&](const QString &key, const double value,
                           const std::function<void(Thing &, double)> &set,
                           const QVector3D &axis, const QString &objName) {
          auto *item = addChild(grp, key);
          auto *scrub = new ui::ScrubFloatEdit(properties_);
          scrub->setObjectName(objName);
          scrub->setDecimals(6);
          scrub->setSingleStep(0.01);
          scrub->setValue(value);
          properties_->setItemWidget(item, 1, scrub);

          auto onCommit = [this, thingIndex, set](double v) {
            if (!documentEditingAllowed() ||
                selectionTouchesNonEditableLayer(document_.selection())) {
              return;
            }
            std::vector<std::int32_t> indices{};
            if (!document_.selection().things.empty()) {
              indices.assign(document_.selection().things.begin(),
                             document_.selection().things.end());
            } else if (thingIndex >= 0 && static_cast<std::size_t>(thingIndex) <
                                              document_.level().things.size()) {
              indices.push_back(thingIndex);
            }
            if (indices.empty()) {
              return;
            }

            document_.pushThingSubsetUndo(
                ui::undo::relabelForSelection("Thing: Position changed",
                                              document_.selection()),
                editor::DocumentDirty::Things, indices);
            for (const std::int32_t index : indices) {
              if (index < 0 || static_cast<std::size_t>(index) >=
                                   document_.level().things.size()) {
                continue;
              }
              Thing &thing =
                  document_.level().things[static_cast<std::size_t>(index)];
              set(thing, v);
              thing.sector =
                  glView_ ? glView_->findContainingSector(
                                TVector{thing.x, thing.y, thing.z},
                                thing.sector)
                          : sed::level::levelFindSectorForXYZ(
                                document_.level(), thing.x, thing.y, thing.z);
            }
            if (glView_) {
              glView_->notifyThingTransformsChanged(indices);
            }
            refreshThingChecks(indices, false, true);
            updateEditActions();
            updateThingPropertiesPreview(false);
          };

          connect(scrub, &ui::ScrubFloatEdit::valueCommitted, this, onCommit);

          auto applyDeltaValue =
              [this, axis](editor::DocumentState &state, const double value,
                           const double startValue,
                           const std::vector<std::int32_t> &indices)
              -> DeferredThingApplyResult {
            const double delta = value - startValue;
            bool any = false;
            for (const std::int32_t idx : indices) {
              if (idx < 0 ||
                  static_cast<std::size_t>(idx) >= state.level.things.size()) {
                continue;
              }
              Thing &thing = state.level.things[static_cast<std::size_t>(idx)];
              thing.x += static_cast<double>(axis.x()) * delta;
              thing.y += static_cast<double>(axis.y()) * delta;
              thing.z += static_cast<double>(axis.z()) * delta;
              thing.sector = glView_
                                 ? glView_->findContainingSector(
                                       TVector{thing.x, thing.y, thing.z},
                                       thing.sector)
                                 : sed::level::levelFindSectorForXYZ(
                                       state.level, thing.x, thing.y, thing.z);
              any = true;
            }
            return DeferredThingApplyResult{any, any, any};
          };

          if (glView_) {
            connectDeferredThingTransformScrub(
                scrub, "Thing: Position changed", thingIndex, applyDeltaValue,
                [this, thingIndex,
                 axis](const double v, const double startValue,
                       const std::vector<std::int32_t> &indices) -> bool {
                  if (!glView_) {
                    return false;
                  }
                  QMatrix4x4 transform{};
                  transform.setToIdentity();
                  transform.translate(axis *
                                      static_cast<float>(v - startValue));
                  return glView_->setThingPropertyDeltaPreview(
                      indices, thingIndex, transform, false);
                });
          } else {
            connectLiveScrub(
                scrub, "Thing: Position changed", editor::DocumentDirty::Things,
                [set](editor::DocumentState &state, const double v) -> bool {
                  bool any = false;
                  for (const std::int32_t idx : state.selection.things) {
                    if (idx >= 0 && static_cast<std::size_t>(idx) <
                                        state.level.things.size()) {
                      set(state.level.things[static_cast<std::size_t>(idx)], v);
                      any = true;
                    }
                  }
                  return any;
                });
          }
        };

        addAxis(
            "X", pos.x, [](Thing &t, double x) { t.x = x; },
            QVector3D(1.0f, 0.0f, 0.0f), "prop.thing.pos.x");
        addAxis(
            "Y", pos.y, [](Thing &t, double y) { t.y = y; },
            QVector3D(0.0f, 1.0f, 0.0f), "prop.thing.pos.y");
        addAxis(
            "Z", pos.z, [](Thing &t, double z) { t.z = z; },
            QVector3D(0.0f, 0.0f, 1.0f), "prop.thing.pos.z");
      }

      // Rotation (editable)
      {
        auto *grp = addGroup(nullptr, "Rotation");

        auto addAxis = [&](const QString &key, const double value,
                           const std::function<void(Thing &, double)> &set,
                           const QVector3D &axis, const QString &objName) {
          auto *item = addChild(grp, key);
          auto *scrub = new ui::ScrubFloatEdit(properties_);
          scrub->setObjectName(objName);
          scrub->setDecimals(3);
          scrub->setSingleStep(1.0);
          scrub->setValue(value);
          properties_->setItemWidget(item, 1, scrub);

          auto onCommit = [this, thingIndex, set](double v) {
            if (!documentEditingAllowed() ||
                selectionTouchesNonEditableLayer(document_.selection())) {
              return;
            }
            std::vector<std::int32_t> indices{};
            if (!document_.selection().things.empty()) {
              indices.assign(document_.selection().things.begin(),
                             document_.selection().things.end());
            } else if (thingIndex >= 0 && static_cast<std::size_t>(thingIndex) <
                                              document_.level().things.size()) {
              indices.push_back(thingIndex);
            }
            if (indices.empty()) {
              return;
            }

            document_.pushThingSubsetUndo(
                ui::undo::relabelForSelection("Thing: Rotation changed",
                                              document_.selection()),
                editor::DocumentDirty::Things, indices);
            for (const std::int32_t index : indices) {
              if (index < 0 || static_cast<std::size_t>(index) >=
                                   document_.level().things.size()) {
                continue;
              }
              set(document_.level().things[static_cast<std::size_t>(index)], v);
            }
            if (glView_) {
              glView_->notifyThingTransformsChanged(indices);
            }
            updateEditActions();
            updateThingPropertiesPreview(false);
          };

          connect(scrub, &ui::ScrubFloatEdit::valueCommitted, this, onCommit);

          const auto pivotForTargets =
              [thingIndex](
                  const editor::DocumentState &state,
                  const std::vector<std::int32_t> &indices) -> QVector3D {
            std::int32_t pivotIndex = thingIndex;
            if (pivotIndex < 0 || static_cast<std::size_t>(pivotIndex) >=
                                      state.level.things.size()) {
              pivotIndex = indices.empty() ? -1 : indices.front();
            }
            if (pivotIndex < 0 || static_cast<std::size_t>(pivotIndex) >=
                                      state.level.things.size()) {
              return {};
            }
            const Thing &pivotThing =
                state.level.things[static_cast<std::size_t>(pivotIndex)];
            return QVector3D(static_cast<float>(pivotThing.x),
                             static_cast<float>(pivotThing.y),
                             static_cast<float>(pivotThing.z));
          };

          auto applyDeltaValue = [this, axis, set, pivotForTargets](
                                     editor::DocumentState &state,
                                     const double value,
                                     const double startValue,
                                     const std::vector<std::int32_t> &indices)
              -> DeferredThingApplyResult {
            if (indices.size() == 1u) {
              const std::int32_t idx = indices.front();
              if (idx < 0 ||
                  static_cast<std::size_t>(idx) >= state.level.things.size()) {
                return {};
              }
              Thing &thing = state.level.things[static_cast<std::size_t>(idx)];
              set(thing, value);
              return DeferredThingApplyResult{true, false, false};
            }

            const double angleDeg = value - startValue;
            if (std::abs(angleDeg) <= sed::math::kTinyEpsilon ||
                axis.lengthSquared() <= sed::math::kTinyEpsilonF) {
              return {};
            }

            TVector rotationAxis{static_cast<double>(axis.x()),
                                 static_cast<double>(axis.y()),
                                 static_cast<double>(axis.z())};
            if (rotationAxis.Normalize() == 0.0) {
              return {};
            }
            const QVector3D pivot = pivotForTargets(state, indices);
            const TVector pivotValue{static_cast<double>(pivot.x()),
                                     static_cast<double>(pivot.y()),
                                     static_cast<double>(pivot.z())};
            const TMatrix43 rotation = TMatrix43::CreateFromVectorAngle(
                rotationAxis, static_cast<float>(angleDeg));
            bool any = false;
            for (const std::int32_t idx : indices) {
              if (idx < 0 ||
                  static_cast<std::size_t>(idx) >= state.level.things.size()) {
                continue;
              }
              Thing &thing = state.level.things[static_cast<std::size_t>(idx)];
              TVector relative{thing.x - pivotValue.x, thing.y - pivotValue.y,
                               thing.z - pivotValue.z};
              relative = rotation.TransformVector(relative);
              thing.x = relative.x + pivotValue.x;
              thing.y = relative.y + pivotValue.y;
              thing.z = relative.z + pivotValue.z;

              TVector pyr{};
              pyr.pitch = thing.pch;
              pyr.yaw = thing.yaw;
              pyr.roll = thing.rol;
              const TMatrix43 oldRotation = TMatrix43::CreateRotate(pyr);
              TMatrix43 newRotation{};
              newRotation.rvec = rotation.TransformVector(oldRotation.rvec);
              newRotation.lvec = rotation.TransformVector(oldRotation.lvec);
              newRotation.uvec = rotation.TransformVector(oldRotation.uvec);
              newRotation.dvec = TVector::zero;
              newRotation.Normalize();
              const TVector finalPyr =
                  ui::transform::extractPYRCreateRotateCompatible(newRotation);
              thing.pch = finalPyr.pitch;
              thing.yaw = finalPyr.yaw;
              thing.rol = finalPyr.roll;
              thing.sector = glView_
                                 ? glView_->findContainingSector(
                                       TVector{thing.x, thing.y, thing.z},
                                       thing.sector)
                                 : sed::level::levelFindSectorForXYZ(
                                       state.level, thing.x, thing.y, thing.z);
              any = true;
            }
            return DeferredThingApplyResult{any, any, any};
          };

          if (glView_) {
            connectDeferredThingTransformScrub(
                scrub, "Thing: Rotation changed", thingIndex, applyDeltaValue,
                [this, thingIndex, axis, set, pivotForTargets](
                    const double value, const double startValue,
                    const std::vector<std::int32_t> &indices) -> bool {
                  if (!glView_) {
                    return false;
                  }
                  if (indices.size() == 1u) {
                    const std::int32_t idx = indices.front();
                    if (idx < 0 || static_cast<std::size_t>(idx) >=
                                       document_.level().things.size()) {
                      return false;
                    }
                    const Thing &source =
                        document_.level().things[static_cast<std::size_t>(idx)];
                    Thing preview = source;
                    set(preview, value);
                    return glView_->setThingPropertyTransformPreview(
                        idx, source, preview);
                  }
                  const QVector3D pivot =
                      pivotForTargets(document_.current(), indices);
                  QMatrix4x4 transform{};
                  transform.setToIdentity();
                  transform.translate(pivot);
                  transform.rotate(static_cast<float>(value - startValue),
                                   axis);
                  transform.translate(-pivot);
                  return glView_->setThingPropertyDeltaPreview(
                      indices, thingIndex, transform, true);
                });
          } else {
            connectLiveScrub(
                scrub, "Thing: Rotation changed", editor::DocumentDirty::Things,
                [set](editor::DocumentState &state, const double v) -> bool {
                  bool any = false;
                  for (const std::int32_t idx : state.selection.things) {
                    if (idx >= 0 && static_cast<std::size_t>(idx) <
                                        state.level.things.size()) {
                      set(state.level.things[static_cast<std::size_t>(idx)], v);
                      any = true;
                    }
                  }
                  return any;
                });
          }
        };

        addAxis(
            "Pitch", pyr.pitch, [](Thing &t, double v) { t.pch = v; },
            QVector3D(1.0f, 0.0f, 0.0f), "prop.thing.rot.pitch");
        addAxis(
            "Yaw", pyr.yaw, [](Thing &t, double v) { t.yaw = v; },
            QVector3D(0.0f, 0.0f, 1.0f), "prop.thing.rot.yaw");
        addAxis(
            "Roll", pyr.roll, [](Thing &t, double v) { t.rol = v; },
            QVector3D(0.0f, 1.0f, 0.0f), "prop.thing.rot.roll");
      }

      // Parameters (editable list)
      {
        if (!thingParameters_ || !thingParametersPanel_) {
          return;
        }
        thingParametersPanel_->setVisible(true);

        auto parameterAddChild =
            [this](QTreeWidgetItem *parent, const QString &key,
                   const QString &value = {}) -> QTreeWidgetItem * {
          const int parentIndent =
              parent ? parent->data(0, kPropertyIndentRole).toInt() : -1;
          const int indent = parent ? (parentIndent + 1) : 0;

          auto *item = new QTreeWidgetItem(thingParameters_);
          item->setText(0, QString(indent * 2, QChar(' ')) + key);
          item->setText(1, value);
          item->setData(0, kPropertyIndentRole, indent);
          return item;
        };
        auto parameterAddGroup =
            [this](QTreeWidgetItem *parent,
                   const QString &title) -> QTreeWidgetItem * {
          const int parentIndent =
              parent ? parent->data(0, kPropertyIndentRole).toInt() : -1;
          const int indent = parent ? (parentIndent + 1) : 0;

          auto *item = new QTreeWidgetItem(thingParameters_);
          item->setText(0, QString(indent * 2, QChar(' ')) + title);
          item->setFirstColumnSpanned(true);
          item->setChildIndicatorPolicy(
              QTreeWidgetItem::DontShowIndicatorWhenChildless);
          item->setFlags(item->flags() & ~Qt::ItemIsSelectable);
          item->setData(0, kPropertyIndentRole, indent);
          QFont font = item->font(0);
          font.setBold(true);
          item->setFont(0, font);
          item->setExpanded(true);
          return item;
        };
        QTreeWidgetItem *values = nullptr;

        const std::vector<thing::parameters::EffectiveParameter>
            effectiveParameters = thing::parameters::effectiveParameters(
                level, templatesDb, thing);
        QStringList parameterNameExtras{};
        for (const thing::parameters::EffectiveParameter &parameter :
             effectiveParameters) {
          if (!parameter.value.name.empty()) {
            parameterNameExtras.push_back(
                QString::fromStdString(parameter.value.name));
          }
        }
        const QStringList parameterNameOptions =
            thing::parameter_editor::completionNameOptions(
                level.kind, std::move(parameterNameExtras));

        // Add new value
        {
          auto *item = parameterAddChild(values, "Add");
          auto *btn = new QPushButton(thingParameters_);
          btn->setObjectName(QStringLiteral("prop.thing.parameters.add"));
          btn->setText("+");
          btn->setToolTip("Add value");
          thingParameters_->setItemWidget(item, 1, btn);
          connect(
              btn, &QPushButton::clicked, this,
              [this, thingIndex, applyUndoable, parameterNameOptions,
               openResourcePicker, showPropertyActionFeedback,
               projectType = level.kind, effectiveThingType = thingType]() {
                auto *pendingItem = new QTreeWidgetItem(thingParameters_);

                auto *keyEdit = new QLineEdit(thingParameters_);
                keyEdit->setObjectName(
                    QStringLiteral("prop.thing.parameters.pending_name"));
                keyEdit->setPlaceholderText("key");
                thing::parameter_editor::installNameCompleter(
                    keyEdit, parameterNameOptions,
                    QStringLiteral("thingParameterNameCompleter"));
                thingParameters_->setItemWidget(pendingItem, 0, keyEdit);

                auto *row = new QWidget(thingParameters_);
                auto *layout = new QHBoxLayout(row);
                layout->setContentsMargins(0, 0, 0, 0);
                layout->setSpacing(4);

                auto *valueEdit = new QLineEdit(row);
                valueEdit->setObjectName(
                    QStringLiteral("prop.thing.parameters.pending_value"));
                valueEdit->setPlaceholderText("value");
                valueEdit->setProperty("sed.thingParameterAutoSeeded", false);
                layout->addWidget(valueEdit, 1);

                auto *schemaEditor = new QToolButton(row);
                schemaEditor->setObjectName(QStringLiteral(
                    "prop.thing.parameters.pending_schema_editor"));
                schemaEditor->setAutoRaise(true);
                schemaEditor->setFocusPolicy(Qt::NoFocus);
                schemaEditor->setVisible(false);
                layout->addWidget(schemaEditor);

                auto *resourceEditor = new QToolButton(row);
                resourceEditor->setObjectName(QStringLiteral(
                    "prop.thing.parameters.pending_resource_editor"));
                resourceEditor->setText(QStringLiteral("..."));
                resourceEditor->setAutoRaise(true);
                resourceEditor->setFocusPolicy(Qt::NoFocus);
                resourceEditor->setVisible(false);
                resourceEditor->setToolTip(QStringLiteral("Pick resource"));
                layout->addWidget(resourceEditor);

                auto *del = new QToolButton(row);
                del->setText("-");
                del->setAutoRaise(true);
                del->setToolTip("Remove value");
                layout->addWidget(del);

                thingParameters_->setItemWidget(pendingItem, 1, row);

                auto removePending = [this, pendingItem, keyEdit,
                                      row]() -> void {
                  if (!pendingItem) {
                    return;
                  }

                  if (thingParameters_) {
                    thingParameters_->removeItemWidget(pendingItem, 0);
                    thingParameters_->removeItemWidget(pendingItem, 1);
                  }

                  if (QTreeWidgetItem *parent = pendingItem->parent()) {
                    parent->removeChild(pendingItem);
                  }
                  delete pendingItem;

                  if (keyEdit) {
                    keyEdit->deleteLater();
                  }
                  if (row) {
                    row->deleteLater();
                  }
                };

                connect(del, &QToolButton::clicked, this,
                        [removePending]() { removePending(); });

                const auto updateSchemaEditor = [this, keyEdit, valueEdit,
                                                 schemaEditor, resourceEditor,
                                                 projectType,
                                                 effectiveThingType]() {
                  if (!keyEdit || !schemaEditor) {
                    return;
                  }
                  const std::string parameterName =
                      keyEdit->text().trimmed().toStdString();
                  const thing::parameter_editor::ValueEditorInfo editorInfo =
                      thing::parameter_editor::valueEditorInfo(
                          projectType, parameterName,
                          effectiveThingType.toStdString());
                  const std::optional<ui::ResourcePickerKind> resourceKind =
                      ui::resourcePickerKindForAdjType(
                          tplAdjTypeForName(parameterName));
                  bool selectionCompatible = true;
                  if (sed::text::iequalsAscii(parameterName, "typeflags")) {
                    const Level &currentLevel = document_.level();
                    const sed::assets::Templates *currentTemplates =
                        templates_.size() > 0 ? &templates_ : nullptr;
                    const std::optional<thing::parameter_schema::FlagFamily>
                        activeFamily = thing::parameter_schema::flagFamily(
                            parameterName, effectiveThingType.toStdString());
                    for (const std::int32_t selectedIndex :
                         document_.selection().things) {
                      if (selectedIndex < 0 ||
                          static_cast<std::size_t>(selectedIndex) >=
                              currentLevel.things.size()) {
                        continue;
                      }
                      const Thing &selectedThing =
                          currentLevel
                              .things[static_cast<std::size_t>(selectedIndex)];
                      const TplValue *selectedType = sed::level::thingFindValue(
                          currentLevel, currentTemplates, selectedThing,
                          "type");
                      const std::optional<thing::parameter_schema::FlagFamily>
                          selectedFamily = thing::parameter_schema::flagFamily(
                              parameterName, selectedType
                                                 ? selectedType->asString()
                                                 : std::string{});
                      if (selectedFamily != activeFamily) {
                        selectionCompatible = false;
                        break;
                      }
                    }
                  }
                  schemaEditor->setVisible(
                      editorInfo.kind !=
                      thing::parameter_editor::ValueEditorKind::none);
                  resourceEditor->setVisible(resourceKind.has_value());
                  schemaEditor->setEnabled(selectionCompatible);
                  valueEdit->setEnabled(selectionCompatible);
                  valueEdit->setToolTip(
                      selectionCompatible
                          ? QString{}
                          : QStringLiteral(
                                "Selected Things use different typeflags "
                                "families; edit them separately."));
                  schemaEditor->setText(editorInfo.buttonText);
                  schemaEditor->setToolTip(
                      selectionCompatible
                          ? editorInfo.toolTip
                          : QStringLiteral(
                                "Selected Things use different typeflags "
                                "families; edit them separately."));
                };
                connect(valueEdit, &QLineEdit::textEdited, this,
                        [valueEdit](const QString &) {
                          if (valueEdit) {
                            valueEdit->setProperty(
                                "sed.thingParameterAutoSeeded", false);
                          }
                        });
                connect(keyEdit, &QLineEdit::textEdited, this,
                        [valueEdit](const QString &) {
                          if (valueEdit) {
                            valueEdit->clear();
                            valueEdit->setProperty(
                                "sed.thingParameterAutoSeeded", false);
                          }
                        });
                connect(keyEdit, &QLineEdit::textChanged, this,
                        [updateSchemaEditor](const QString &) {
                          updateSchemaEditor();
                        });
                const QPointer<QLineEdit> guardedKeyEdit(keyEdit);
                const QPointer<QLineEdit> guardedValueEdit(valueEdit);
                const QPointer<QToolButton> guardedSchemaEditor(schemaEditor);
                connect(schemaEditor, &QToolButton::clicked, this,
                        [this, guardedKeyEdit, guardedValueEdit, guardedSchemaEditor,
                         projectType, effectiveThingType]() {
                          if (!guardedKeyEdit || !guardedValueEdit) {
                            return;
                          }
                          const std::string parameterName =
                              guardedKeyEdit->text().trimmed().toStdString();
                          const QString currentValue = guardedValueEdit->text();
                          if (const std::optional<QString> picked =
                                  thing::parameter_editor::openValueEditor(
                                      this, projectType, parameterName,
                                      effectiveThingType.toStdString(),
                                      currentValue,
                                      guardedSchemaEditor)) {
                            if (guardedValueEdit) {
                              guardedValueEdit->setText(*picked);
                              guardedValueEdit->setProperty(
                                  "sed.thingParameterAutoSeeded", false);
                            }
                          }
                        });
                const QPointer<QLineEdit> guardedResourceKey(keyEdit);
                const QPointer<QLineEdit> guardedResourceValue(valueEdit);
                connect(
                    resourceEditor, &QToolButton::clicked, this,
                    [this, guardedResourceKey, guardedResourceValue,
                     openResourcePicker]() {
                      if (!guardedResourceKey || !guardedResourceValue) {
                        return;
                      }
                      const std::string parameterName =
                          guardedResourceKey->text().trimmed().toStdString();
                      const std::optional<ui::ResourcePickerKind> resourceKind =
                          ui::resourcePickerKindForAdjType(
                              tplAdjTypeForName(parameterName));
                      if (!resourceKind) {
                        return;
                      }
                      const std::optional<QString> picked = openResourcePicker(
                          *resourceKind, guardedResourceValue->text(),
                          QStringLiteral("Pick %1").arg(
                              guardedResourceKey->text().trimmed()));
                      if (picked && guardedResourceValue) {
                        guardedResourceValue->setText(picked->trimmed());
                        guardedResourceValue->setProperty(
                            "sed.thingParameterAutoSeeded", false);
                      }
                    });
                updateSchemaEditor();

                static constexpr const char *kCommittedProp =
                    "sed.pendingThingValueCommitted";

                connect(
                    keyEdit, &QLineEdit::editingFinished, this,
                    [this, keyEdit, valueEdit, thingIndex, removePending,
                     projectType]() {
                      if (!keyEdit || !valueEdit) {
                        return;
                      }

                      const QString key = keyEdit->text().trimmed();
                      if (key.isEmpty()) {
                        removePending();
                        return;
                      }

                      if (valueEdit->text().trimmed().isEmpty() &&
                          !thing::parameter_schema::isRepeatableParameter(
                              projectType, key.toStdString())) {
                        const Level &lvl = document_.level();
                        if (thingIndex < 0 ||
                            static_cast<std::size_t>(thingIndex) >=
                                lvl.things.size()) {
                          return;
                        }

                        const Thing &th =
                            lvl.things[static_cast<std::size_t>(thingIndex)];
                        const sed::assets::Templates *templatesDb =
                            templates_.size() > 0 ? &templates_ : nullptr;
                        if (const TplValue *tv = sed::level::thingFindValue(
                                lvl, templatesDb, th, key.toStdString())) {
                          valueEdit->setText(
                              QString::fromStdString(tv->asString()));
                          valueEdit->setProperty("sed.thingParameterAutoSeeded",
                                                 true);
                        } else if (const auto defaultValue =
                                       thing::light_authoring::
                                           defaultValueForParameter(
                                               lvl.kind, key.toStdString())) {
                          valueEdit->setText(
                              QString::fromStdString(*defaultValue));
                          valueEdit->setProperty("sed.thingParameterAutoSeeded",
                                                 true);
                        }
                      }

                      if (keyEdit->hasFocus()) {
                        valueEdit->setFocus();
                        valueEdit->selectAll();
                      }
                    });

                connect(
                    valueEdit, &QLineEdit::editingFinished, this,
                    [this, keyEdit, valueEdit, thingIndex, applyUndoable,
                     removePending, projectType, showPropertyActionFeedback]() {
                      if (!keyEdit || !valueEdit) {
                        return;
                      }

                      if (keyEdit->property(kCommittedProp).toBool()) {
                        return;
                      }

                      const QString key = keyEdit->text().trimmed();
                      if (key.isEmpty()) {
                        removePending();
                        return;
                      }

                      const std::string keyStd = key.toStdString();
                      const bool repeatable =
                          thing::parameter_schema::isRepeatableParameter(
                              projectType, keyStd);
                      const bool useTargetEffectiveSeed =
                          !repeatable &&
                          valueEdit->property("sed.thingParameterAutoSeeded")
                              .toBool();
                      const std::string typedValue =
                          valueEdit->text().trimmed().toStdString();
                      if (repeatable && typedValue.empty()) {
                        return;
                      }
                      const sed::assets::Templates *templatesDb =
                          templates_.size() > 0 ? &templates_ : nullptr;

                      const bool ok = applyUndoable(
                          ui::undo::thingParameterAction(document_.selection(),
                                                         keyStd, "added"),
                          editor::DocumentDirty::Things,
                          [thingIndex, keyStd, typedValue,
                           useTargetEffectiveSeed, repeatable, projectType,
                           templatesDb](editor::DocumentState &state) -> bool {
                            auto applyOne = [&](Thing &th)
                                -> thing::parameters::OverrideWriteResult {
                              const TplValue *semanticSource =
                                  sed::level::thingFindValue(
                                      state.level, templatesDb, th, keyStd);
                              std::string nextText = typedValue;
                              if (useTargetEffectiveSeed) {
                                if (semanticSource) {
                                  nextText = semanticSource->asString();
                                } else if (const auto defaultValue =
                                               thing::light_authoring::
                                                   defaultValueForParameter(
                                                       projectType, keyStd)) {
                                  nextText = *defaultValue;
                                }
                              }

                              if (!repeatable) {
                                return thing::parameters::
                                    upsertOverrideDetailed(th, keyStd, nextText,
                                                           semanticSource);
                              }

                              if (sed::text::iequalsAscii(keyStd, "frame")) {
                                const TplValue *moveValue =
                                    sed::level::thingFindValue(
                                        state.level, templatesDb, th, "move");
                                const bool pathMove =
                                    moveValue && sed::text::iequalsAscii(
                                                     sed::text::trimView(
                                                         moveValue->asString()),
                                                     "path");
                                if (pathMove) {
                                  const thing::parameters::
                                      InheritedPathFrameState pathState =
                                          thing::parameters::
                                              inheritedPathFrameState(
                                                  state.level, templatesDb, th);
                                  if (pathState.capacity != 0u &&
                                      pathState.activeFrames.size() +
                                              sed::level::thingCountFrames(
                                                  th) >=
                                          pathState.capacity) {
                                    return thing::parameters::
                                        OverrideWriteResult::invalid;
                                  }
                                }

                                TplValue frameSemantic{};
                                frameSemantic.name = "frame";
                                frameSemantic.type = ValueType::frame;
                                frameSemantic.adjType =
                                    tplAdjTypeForName(frameSemantic.name);
                                const thing::parameters::OverrideWriteResult
                                    result = thing::parameters::
                                        appendOverrideDetailed(th, keyStd,
                                                               nextText,
                                                               &frameSemantic);
                                if (result ==
                                    thing::parameters::OverrideWriteResult::
                                        changed) {
                                  sed::level::thingSyncNumFrames(th);
                                }
                                return result;
                              }

                              if (const std::optional<std::size_t> limit =
                                      thing::parameter_schema::
                                          repeatableParameterLimit(projectType,
                                                                   keyStd)) {
                                if (thing::parameters::
                                        templateReferenceResolves(state.level,
                                                                  templatesDb,
                                                                  nextText)) {
                                  std::size_t occupied = 0;
                                  for (
                                      const auto &inherited :
                                      thing::parameters::
                                          inheritedRepeatableParameterOccurrences(
                                              state.level, templatesDb, th,
                                              keyStd)) {
                                    occupied +=
                                        thing::parameters::
                                                templateReferenceResolves(
                                                    state.level, templatesDb,
                                                    inherited.value.asString())
                                            ? 1u
                                            : 0u;
                                  }
                                  for (const TplValue &value : th.values) {
                                    occupied +=
                                        sed::text::iequalsAscii(value.name,
                                                                keyStd) &&
                                                thing::parameters::
                                                    templateReferenceResolves(
                                                        state.level,
                                                        templatesDb,
                                                        value.asString())
                                            ? 1u
                                            : 0u;
                                  }
                                  if (occupied >= *limit) {
                                    return thing::parameters::
                                        OverrideWriteResult::invalid;
                                  }
                                }
                              }

                              return thing::parameters::appendOverrideDetailed(
                                  th, keyStd, nextText, semanticSource);
                            };

                            bool any = false;
                            if (!state.selection.things.empty()) {
                              for (const std::int32_t idx :
                                   state.selection.things) {
                                if (idx < 0 || static_cast<std::size_t>(idx) >=
                                                   state.level.things.size()) {
                                  continue;
                                }
                                Thing& targetThing = state.level.things[
                                    static_cast<std::size_t>(idx)];
                                const std::vector<SelectedFrameOrdinal>
                                    oldFrameSelection =
                                        sed::text::iequalsAscii(keyStd, "frame")
                                    ? selectedFrameOrdinalsForThing(
                                          state.selection, targetThing, idx)
                                    : std::vector<SelectedFrameOrdinal>{};
                                const thing::parameters::OverrideWriteResult
                                    result = applyOne(targetThing);
                                if (result ==
                                    thing::parameters::OverrideWriteResult::
                                        invalid) {
                                  return false;
                                }
                                if (result ==
                                        thing::parameters::OverrideWriteResult::
                                            changed
                                    && sed::text::iequalsAscii(keyStd, "frame")) {
                                  remapSelectedFramesForThing(
                                      state.selection, idx, oldFrameSelection,
                                      targetThing);
                                }
                                any = result ==
                                          thing::parameters::
                                              OverrideWriteResult::changed ||
                                      any;
                              }
                              return any;
                            }

                            if (thingIndex < 0 ||
                                static_cast<std::size_t>(thingIndex) >=
                                    state.level.things.size()) {
                              return false;
                            }

                            Thing& targetThing = state.level.things[
                                static_cast<std::size_t>(thingIndex)];
                            const std::vector<SelectedFrameOrdinal>
                                oldFrameSelection =
                                    sed::text::iequalsAscii(keyStd, "frame")
                                ? selectedFrameOrdinalsForThing(
                                      state.selection, targetThing, thingIndex)
                                : std::vector<SelectedFrameOrdinal>{};
                            const thing::parameters::OverrideWriteResult result =
                                applyOne(targetThing);
                            if (result ==
                                    thing::parameters::OverrideWriteResult::
                                        changed
                                && sed::text::iequalsAscii(keyStd, "frame")) {
                              remapSelectedFramesForThing(
                                  state.selection, thingIndex,
                                  oldFrameSelection, targetThing);
                            }
                            return result ==
                                thing::parameters::OverrideWriteResult::changed;
                          });

                      if (ok) {
                        keyEdit->setProperty(kCommittedProp, true);
                        removePending();
                      } else {
                        const QString reason =
                            sed::text::iequalsAscii(keyStd, "debris")
                                ? QStringLiteral("Could not add debris: the "
                                                 "runtime capacity may be full "
                                                 "or the value is invalid.")
                            : sed::text::iequalsAscii(keyStd, "frame")
                                ? QStringLiteral(
                                      "Could not add frame: the value is "
                                      "invalid or the inherited Path buffer is "
                                      "full.")
                                : QStringLiteral("The value is invalid for one "
                                                 "or more selected Things.");
                        showPropertyActionFeedback(valueEdit, reason, true);
                      }
                    });

                keyEdit->setFocus();
              });
        }

        auto isFrameValue = [](const TplValue &v) -> bool {
          return v.type == ValueType::frame &&
                 sed::text::iequalsAscii(v.name, "frame");
        };

        const TplValue *effectiveMove =
            sed::level::thingFindValue(level, templatesDb, thing, "move");
        const bool effectivePathMove =
            effectiveMove &&
            sed::text::iequalsAscii(
                sed::text::trimView(effectiveMove->asString()), "path");
        const thing::parameters::InheritedPathFrameState inheritedPathFrames =
            effectivePathMove ? thing::parameters::inheritedPathFrameState(
                                    level, templatesDb, thing)
                              : thing::parameters::InheritedPathFrameState{};
        const std::size_t inheritedPathRemaining =
            inheritedPathFrames.capacity >
                    inheritedPathFrames.activeFrames.size()
                ? inheritedPathFrames.capacity -
                      inheritedPathFrames.activeFrames.size()
                : 0u;
        const std::size_t frameCount = sed::level::thingCountFrames(thing);
        const bool haveFrames = frameCount > 0;
        if (haveFrames) {
          auto *framesGroup = parameterAddGroup(
              values,
              QString("Frames (%1)").arg(static_cast<qulonglong>(frameCount)));
          std::size_t frameNumber = 0;

          for (std::size_t valueIndex = 0; valueIndex < thing.values.size();
               ++valueIndex) {
            const TplValue &v = thing.values[valueIndex];
            if (!isFrameValue(v)) {
              continue;
            }

            const bool runtimeActive = !effectivePathMove ||
                                       inheritedPathFrames.capacity == 0u ||
                                       frameNumber < inheritedPathRemaining;
            const bool canAppendFrame = !effectivePathMove ||
                                        inheritedPathFrames.capacity == 0u ||
                                        frameCount < inheritedPathRemaining;
            auto *item = parameterAddChild(
                framesGroup,
                runtimeActive ? QString("Frame %1")
                                    .arg(static_cast<qulonglong>(frameNumber))
                              : QString("Frame %1 [ignored]")
                                    .arg(static_cast<qulonglong>(frameNumber)));
            ++frameNumber;
            item->setData(0, kPropertyActionRole,
                          static_cast<int>(PropertyAction::selectFrameValue));
            item->setData(0, kPropertyThingRole, static_cast<int>(thingIndex));
            item->setData(0, kPropertyValueIndexRole,
                          static_cast<int>(valueIndex));

            auto *row = new QWidget(thingParameters_);
            auto *layout = new QHBoxLayout(row);
            layout->setContentsMargins(0, 0, 0, 0);
            layout->setSpacing(4);

            auto *edit = new QLineEdit(row);
            edit->setText(QString::fromStdString(v.asString()));
            edit->setProperty("sed.frameThingIndex", thingIndex);
            edit->setProperty("sed.frameValueIndex",
                              static_cast<int>(valueIndex));
            edit->setReadOnly(!runtimeActive);
            edit->setToolTip(
                runtimeActive
                    ? QStringLiteral("Edit this active local frame.")
                    : QStringLiteral("Ignored: the inherited Path buffer has "
                                     "no remaining capacity."));
            edit->installEventFilter(this);
            layout->addWidget(edit, 1);

            auto *dup = new QToolButton(row);
            dup->setText("Copy");
            dup->setAutoRaise(true);
            dup->setEnabled(runtimeActive && canAppendFrame);
            dup->setToolTip(runtimeActive && canAppendFrame
                                ? QStringLiteral("Duplicate frame to the end")
                                : QStringLiteral("The inherited Path buffer "
                                                 "has no remaining capacity."));
            layout->addWidget(dup);

            auto *del = new QToolButton(row);
            del->setText("-");
            del->setAutoRaise(true);
            del->setToolTip("Remove frame");
            layout->addWidget(del);

            thingParameters_->setItemWidget(item, 1, row);

            connect(
                edit, &QLineEdit::editingFinished, this,
                [this, edit, thingIndex, valueIndex,
                 applyFrameThingUndoable]() {
                  const Level &lvl = document_.level();
                  if (thingIndex < 0 || static_cast<std::size_t>(thingIndex) >=
                                            lvl.things.size()) {
                    return;
                  }

                  const Thing &th =
                      lvl.things[static_cast<std::size_t>(thingIndex)];
                  if (valueIndex >= th.values.size()) {
                    return;
                  }

                  const TplValue &curVal = th.values[valueIndex];
                  if (curVal.type != ValueType::frame ||
                      !sed::text::iequalsAscii(curVal.name, "frame")) {
                    return;
                  }

                  const QString cur = QString::fromStdString(curVal.asString());
                  const QString next = edit->text().trimmed();
                  if (next == cur) {
                    return;
                  }

                  const std::string nextStd = next.toStdString();
                  const bool okApply = applyFrameThingUndoable(
                      "Thing Frame: Changed", thingIndex,
                      [thingIndex, valueIndex,
                       nextStd](editor::DocumentState &state) -> bool {
                        if (thingIndex < 0 ||
                            static_cast<std::size_t>(thingIndex) >=
                                state.level.things.size()) {
                          return false;
                        }

                        Thing &th =
                            state.level
                                .things[static_cast<std::size_t>(thingIndex)];
                        if (valueIndex >= th.values.size()) {
                          return false;
                        }

                        TplValue &v = th.values[valueIndex];
                        if (v.type != ValueType::frame ||
                            !sed::text::iequalsAscii(v.name, "frame")) {
                          return false;
                        }

                        const std::string before = v.asString();
                        if (!v.parse(nextStd)) {
                          return false;
                        }

                        return before != v.asString();
                      });

                  if (!okApply) {
                    edit->setText(cur);
                  }
                });

            connect(
                dup, &QToolButton::clicked, this,
                [this, thingIndex, valueIndex, templatesDb,
                 applyFrameThingUndoable]() {
                  const bool okApply = applyFrameThingUndoable(
                      "Thing Frame: Duplicated", thingIndex,
                      [thingIndex, valueIndex,
                       templatesDb](editor::DocumentState &state) -> bool {
                        if (thingIndex < 0 ||
                            static_cast<std::size_t>(thingIndex) >=
                                state.level.things.size()) {
                          return false;
                        }

                        Thing &thing =
                            state.level
                                .things[static_cast<std::size_t>(thingIndex)];
                        if (valueIndex >= thing.values.size()) {
                          return false;
                        }

                        const TplValue &source = thing.values[valueIndex];
                        if (source.type != ValueType::frame ||
                            !sed::text::iequalsAscii(source.name, "frame")) {
                          return false;
                        }

                        const TplValue *moveValue = sed::level::thingFindValue(
                            state.level, templatesDb, thing, "move");
                        if (moveValue &&
                            sed::text::iequalsAscii(
                                sed::text::trimView(moveValue->asString()),
                                "path")) {
                          const thing::parameters::InheritedPathFrameState
                              pathState =
                                  thing::parameters::inheritedPathFrameState(
                                      state.level, templatesDb, thing);
                          if (pathState.capacity != 0u &&
                              pathState.activeFrames.size() +
                                      sed::level::thingCountFrames(thing) >=
                                  pathState.capacity) {
                            return false;
                            }
                        }

                        const std::vector<SelectedFrameOrdinal> oldSelection =
                            selectedFrameOrdinalsForThing(
                                state.selection, thing, thingIndex);
                        thing.values.push_back(source);
                        sed::level::thingSyncNumFrames(thing);
                        remapSelectedFramesForThing(
                            state.selection, thingIndex, oldSelection, thing);
                        return true;
                      });

                  if (okApply && glView_) {
                    glView_->setHighlightedSelection(document_.selection());
                  }
                });

            connect(
                del, &QToolButton::clicked, this,
                [this, thingIndex, valueIndex,
                 applyFrameThingUndoable]() {
                  const bool okApply = applyFrameThingUndoable(
                      "Thing Frame: Deleted", thingIndex,
                      [thingIndex,
                       valueIndex](editor::DocumentState &state) -> bool {
                        if (thingIndex < 0 ||
                            static_cast<std::size_t>(thingIndex) >=
                                state.level.things.size()) {
                          return false;
                        }

                        Thing &thing =
                            state.level
                                .things[static_cast<std::size_t>(thingIndex)];
                        if (valueIndex >= thing.values.size()) {
                          return false;
                        }

                        if (thing.values[valueIndex].type != ValueType::frame) {
                          return false;
                        }
                        if (!sed::text::iequalsAscii(
                                thing.values[valueIndex].name, "frame")) {
                          return false;
                        }

                        const auto deletedOrdinal = frameOrdinalForValueIndex(
                            thing, static_cast<std::int32_t>(valueIndex));
                        const std::vector<SelectedFrameOrdinal> oldSelection =
                            selectedFrameOrdinalsForThing(
                                state.selection, thing, thingIndex);

                        thing.values.erase(
                            thing.values.begin() +
                            static_cast<std::vector<TplValue>::difference_type>(
                                valueIndex));

                        // Keep `numframes` in sync with the number of `frame`
                        // values.
                        sed::level::thingSyncNumFrames(thing);

                        remapSelectedFramesForThing(
                            state.selection, thingIndex, oldSelection, thing,
                            deletedOrdinal);
                        return true;
                      });

                  if (okApply && glView_) {
                    glView_->clearFrameSelection();
                    glView_->setHighlightedSelection(document_.selection());
                  }
                });
          }
        }

        std::vector<
            std::pair<thing::parameters::InheritedParameterOccurrence, bool>>
            displayedInheritedFrames{};
        if (effectivePathMove) {
          displayedInheritedFrames.reserve(
              inheritedPathFrames.activeFrames.size() +
              inheritedPathFrames.ignoredFrames.size());
          for (const auto &frame : inheritedPathFrames.activeFrames) {
            displayedInheritedFrames.emplace_back(frame, true);
          }
          for (const auto &frame : inheritedPathFrames.ignoredFrames) {
            displayedInheritedFrames.emplace_back(frame, false);
          }
          std::stable_sort(
              displayedInheritedFrames.begin(), displayedInheritedFrames.end(),
              [](const auto &lhs, const auto &rhs) {
                return std::tie(lhs.first.templateOrder, lhs.first.valueIndex) <
                       std::tie(rhs.first.templateOrder, rhs.first.valueIndex);
              });
        }
        for (std::size_t occurrence = 0;
             occurrence < displayedInheritedFrames.size(); ++occurrence) {
          const auto &[inheritedFrame, runtimeActive] =
              displayedInheritedFrames[occurrence];
          auto *item = parameterAddChild(
              values, runtimeActive
                          ? QStringLiteral("frame [Template %1]")
                                .arg(static_cast<qulonglong>(occurrence + 1u))
                          : QStringLiteral("frame [Template %1, ignored]")
                                .arg(static_cast<qulonglong>(occurrence + 1u)));
          item->setData(
              0, kThingParameterSearchRole,
              QStringLiteral("frame %1 %2")
                  .arg(
                      QString::fromStdString(inheritedFrame.templateName),
                      QString::fromStdString(inheritedFrame.value.asString())));
          item->setToolTip(
              0, runtimeActive
                     ? QStringLiteral(
                           "Active inherited Path frame; the runtime buffer "
                           "cannot be replaced by a Thing override.")
                     : QStringLiteral(
                           "Ignored by the runtime because it occurs before "
                           "numframes or beyond the inherited Path capacity."));

          auto *edit = new QLineEdit(thingParameters_);
          edit->setObjectName(
              QStringLiteral("prop.thing.value.frame.inherited.%1")
                  .arg(static_cast<qulonglong>(occurrence)));
          edit->setText(
              QString::fromStdString(inheritedFrame.value.asString()));
          edit->setReadOnly(true);
          applyInheritedParameterAppearance(edit, item, true);
          edit->setToolTip(
              runtimeActive
                  ? QStringLiteral("Inherited from template %1; read-only on "
                                   "placed Things.")
                        .arg(
                            QString::fromStdString(inheritedFrame.templateName))
                  : QStringLiteral(
                        "Authored by template %1 but ignored by the runtime.")
                        .arg(QString::fromStdString(
                            inheritedFrame.templateName)));
          thingParameters_->setItemWidget(item, 1, edit);
        }

        const std::vector<thing::parameters::InheritedParameterOccurrence>
            inheritedDebris =
                thing::parameters::inheritedRepeatableParameterOccurrences(
                    level, templatesDb, thing, "debris");
        const std::size_t debrisLimit =
            thing::parameter_schema::repeatableParameterLimit(level.kind,
                                                              "debris")
                .value_or(std::numeric_limits<std::size_t>::max());
        std::size_t occupiedDebrisSlots = 0;
        for (std::size_t occurrence = 0; occurrence < inheritedDebris.size();
             ++occurrence) {
          const auto &inherited = inheritedDebris[occurrence];
          const bool resolves = thing::parameters::templateReferenceResolves(
              level, templatesDb, inherited.value.asString());
          const bool runtimeActive =
              resolves && occupiedDebrisSlots < debrisLimit;
          occupiedDebrisSlots += runtimeActive ? 1u : 0u;
          auto *item = parameterAddChild(
              values, runtimeActive
                          ? QStringLiteral("debris [Template %1]")
                                .arg(static_cast<qulonglong>(occurrence + 1u))
                          : QStringLiteral("debris [Template %1, ignored]")
                                .arg(static_cast<qulonglong>(occurrence + 1u)));
          item->setData(
              0, kThingParameterSearchRole,
              QStringLiteral("debris %1 %2")
                  .arg(QString::fromStdString(inherited.templateName),
                       QString::fromStdString(inherited.value.asString())));
          item->setToolTip(
              0,
              runtimeActive
                  ? QStringLiteral("Active inherited debris is append-only in "
                                   "the engine. Add a Thing-local occurrence "
                                   "instead of replacing this slot.")
              : resolves
                  ? QStringLiteral("Ignored by the runtime because the debris "
                                   "capacity is already full.")
                  : QStringLiteral("Ignored by the runtime because this "
                                   "template reference does not resolve."));

          auto *edit = new QLineEdit(thingParameters_);
          edit->setObjectName(
              QStringLiteral("prop.thing.value.debris.inherited.%1")
                  .arg(static_cast<qulonglong>(occurrence)));
          edit->setText(QString::fromStdString(inherited.value.asString()));
          edit->setReadOnly(true);
          applyInheritedParameterAppearance(edit, item, true);
          edit->setToolTip(
              runtimeActive
                  ? QStringLiteral("Inherited from template %1; read-only on "
                                   "placed Things.")
                        .arg(QString::fromStdString(inherited.templateName))
                  : QStringLiteral("Authored by template %1 but currently "
                                   "ignored by the runtime.")
                        .arg(QString::fromStdString(inherited.templateName)));
          thingParameters_->setItemWidget(item, 1, edit);
        }

        std::size_t localDebrisOccurrence = 0;
        for (std::size_t valueIndex = 0; valueIndex < thing.values.size();
             ++valueIndex) {
          const TplValue &debrisValue = thing.values[valueIndex];
          if (!sed::text::iequalsAscii(debrisValue.name, "debris")) {
            continue;
          }

          const std::size_t occurrence = localDebrisOccurrence++;
          const bool resolves = thing::parameters::templateReferenceResolves(
              level, templatesDb, debrisValue.asString());
          const bool runtimeActive =
              resolves && occupiedDebrisSlots < debrisLimit;
          occupiedDebrisSlots += runtimeActive ? 1u : 0u;
          auto *item = parameterAddChild(
              values, runtimeActive
                          ? QStringLiteral("debris [Thing %1]")
                                .arg(static_cast<qulonglong>(occurrence + 1u))
                          : QStringLiteral("debris [Thing %1, ignored]")
                                .arg(static_cast<qulonglong>(occurrence + 1u)));
          item->setData(
              0, kThingParameterSearchRole,
              QStringLiteral("debris Thing %1")
                  .arg(QString::fromStdString(debrisValue.asString())));
          item->setToolTip(
              0, runtimeActive
                     ? QStringLiteral("Active Thing-local debris; Edit and "
                                      "Remove affect only this occurrence.")
                 : resolves
                     ? QStringLiteral("Ignored because earlier debris already "
                                      "fills the runtime capacity; Edit or "
                                      "Remove affects only this occurrence.")
                     : QStringLiteral("Ignored because the template reference "
                                      "does not resolve; Edit or Remove "
                                      "affects only this occurrence."));

          auto *row = new QWidget(thingParameters_);
          auto *layout = new QHBoxLayout(row);
          layout->setContentsMargins(0, 0, 0, 0);
          layout->setSpacing(4);

          auto *edit = new QLineEdit(row);
          edit->setObjectName(QStringLiteral("prop.thing.value.debris.local.%1")
                                  .arg(static_cast<qulonglong>(occurrence)));
          edit->setText(QString::fromStdString(debrisValue.asString()));
          edit->setToolTip(
              QStringLiteral("Edit this Thing-local debris occurrence."));
          layout->addWidget(edit, 1);

          auto *pick = new QToolButton(row);
          pick->setObjectName(QStringLiteral("prop.thing.value.debris.pick.%1")
                                  .arg(static_cast<qulonglong>(occurrence)));
          pick->setText(QStringLiteral("..."));
          pick->setAutoRaise(true);
          pick->setFocusPolicy(Qt::NoFocus);
          pick->setToolTip(QStringLiteral("Pick debris Thing template"));
          layout->addWidget(pick);
          openAuxiliaryEditorOnDoubleClick(edit, pick);

          auto *remove = new QToolButton(row);
          remove->setObjectName(
              QStringLiteral("prop.thing.value.debris.remove.%1")
                  .arg(static_cast<qulonglong>(occurrence)));
          remove->setText(QStringLiteral("-"));
          remove->setAutoRaise(true);
          remove->setToolTip(
              QStringLiteral("Remove only this Thing-local debris occurrence"));
          layout->addWidget(remove);

          thingParameters_->setItemWidget(item, 1, row);

          const auto applyDebrisText =
              [thingIndex, occurrence](editor::DocumentState &state,
                                       const std::string &nextText) -> bool {
            const auto applyOne = [&](Thing &selectedThing)
                -> thing::parameters::OverrideWriteResult {
              const std::optional<std::size_t> selectedValueIndex =
                  thing::parameters::localValueIndex(selectedThing, "debris",
                                                     occurrence);
              if (!selectedValueIndex) {
                return thing::parameters::OverrideWriteResult::unchanged;
              }

              TplValue next = selectedThing.values[*selectedValueIndex];
              if (!next.parse(nextText)) {
                return thing::parameters::OverrideWriteResult::invalid;
              }
              if (next.asString() ==
                  selectedThing.values[*selectedValueIndex].asString()) {
                return thing::parameters::OverrideWriteResult::unchanged;
              }
              selectedThing.values[*selectedValueIndex] = std::move(next);
              return thing::parameters::OverrideWriteResult::changed;
            };

            bool changed = false;
            if (!state.selection.things.empty()) {
              for (const std::int32_t selectedIndex : state.selection.things) {
                if (selectedIndex < 0 ||
                    static_cast<std::size_t>(selectedIndex) >=
                        state.level.things.size()) {
                  continue;
                }
                const thing::parameters::OverrideWriteResult result = applyOne(
                    state.level
                        .things[static_cast<std::size_t>(selectedIndex)]);
                if (result == thing::parameters::OverrideWriteResult::invalid) {
                  return false;
                }
                changed =
                    result == thing::parameters::OverrideWriteResult::changed ||
                    changed;
              }
              return changed;
            }

            if (thingIndex < 0 || static_cast<std::size_t>(thingIndex) >=
                                      state.level.things.size()) {
              return false;
            }
            return applyOne(
                       state.level
                           .things[static_cast<std::size_t>(thingIndex)]) ==
                   thing::parameters::OverrideWriteResult::changed;
          };

          connect(
              edit, &QLineEdit::editingFinished, this,
              [this, edit,
               current = QString::fromStdString(debrisValue.asString()),
               applyDebrisText, applyUndoable]() {
                if (!edit) {
                  return;
                }
                if (edit->text().trimmed() == current) {
                  return;
                }
                const std::string nextText =
                    edit->text().trimmed().toStdString();
                if (!applyUndoable(
                                   ui::undo::thingParameterAction(
                                       document_.selection(), "debris",
                                       "changed"),
                                   editor::DocumentDirty::Things,
                                   [applyDebrisText, nextText](
                                       editor::DocumentState &state) -> bool {
                                     return applyDebrisText(state, nextText);
                                   })) {
                  edit->setText(current);
                }
              });

          const QPointer<QLineEdit> guardedDebrisEdit(edit);
          connect(
              pick, &QToolButton::clicked, this,
              [this, guardedDebrisEdit, applyDebrisText, applyUndoable,
               openResourcePicker]() {
                if (!guardedDebrisEdit) {
                  return;
                }
                const std::optional<QString> picked =
                    openResourcePicker(ui::ResourcePickerKind::template_,
                                       guardedDebrisEdit->text(),
                                       QStringLiteral("Pick Debris Template"));
                if (!picked) {
                  return;
                }
                const std::string nextText = picked->trimmed().toStdString();
                (void)applyUndoable(
                                    ui::undo::thingParameterAction(
                                        document_.selection(), "debris",
                                        "changed"),
                                    editor::DocumentDirty::Things,
                                    [applyDebrisText, nextText](
                                        editor::DocumentState &state) -> bool {
                                      return applyDebrisText(state, nextText);
                                    });
              });

          connect(
              remove, &QToolButton::clicked, this,
              [this, thingIndex, occurrence, applyUndoable]() {
                (void)applyUndoable(
                    ui::undo::thingParameterAction(
                        document_.selection(), "debris", "removed"),
                    editor::DocumentDirty::Things,
                    [thingIndex,
                     occurrence](editor::DocumentState &state) -> bool {
                      const auto removeOne = [&](const std::int32_t index) -> bool {
                        return removeThingParameterPreservingFrameSelection(
                            state, index, [&](Thing &selectedThing) {
                              const std::optional<std::size_t> selectedValueIndex =
                                  thing::parameters::localValueIndex(
                                      selectedThing, "debris", occurrence);
                              return selectedValueIndex
                                  && thing::parameters::removeOverrideAt(
                                      selectedThing, *selectedValueIndex,
                                      "debris");
                            });
                      };

                      bool changed = false;
                      if (!state.selection.things.empty()) {
                        for (const std::int32_t selectedIndex :
                             state.selection.things) {
                          if (selectedIndex >= 0 &&
                              static_cast<std::size_t>(selectedIndex) <
                                  state.level.things.size()) {
                            changed = removeOne(selectedIndex) || changed;
                          }
                        }
                        return changed;
                      }

                      return removeOne(thingIndex);
                    });
              });
        }

        for (const thing::parameters::EffectiveParameter &parameter :
             effectiveParameters) {
          const TplValue &v = parameter.value;
          if (v.name.empty() || sed::text::iequalsAscii(v.name, "thingflags") ||
              sed::text::iequalsAscii(v.name, "numframes") ||
              thing::parameter_schema::isRepeatableParameter(level.kind,
                                                             v.name) ||
              isFrameValue(v)) {
            continue;
          }

          const std::string keyName = v.name;
          const std::optional<ui::ResourcePickerKind> resourcePickerKind =
              ui::resourcePickerKindForAdjType(v.adjType);
          auto *item =
              parameterAddChild(values, QString::fromStdString(v.name));
          item->setData(0, kThingParameterSearchRole,
                        QStringLiteral("%1 %2").arg(
                            QString::fromStdString(v.name),
                            QString::fromStdString(v.asString())));
          if (parameter.templateValue) {
            item->setToolTip(
                0, parameter.localOverride
                       ? QStringLiteral("Thing override; restore is available "
                                        "for the template value.")
                       : QStringLiteral("Inherited template parameter; editing "
                                        "creates a Thing override."));
          }

          auto *row = new QWidget(thingParameters_);
          auto *layout = new QHBoxLayout(row);
          layout->setContentsMargins(0, 0, 0, 0);
          layout->setSpacing(4);

          auto *edit = new QLineEdit(row);
          edit->setObjectName(
              QStringLiteral("prop.thing.value.%1")
                  .arg(QString::fromStdString(keyName).toLower()));
          const ValueType numericTextType = ui::typedNumericTextType(v);
          ui::configureTypedNumericLineEdit(
              *edit, QString::fromStdString(v.asString()), numericTextType,
              QByteArrayLiteral("sed.thingParameterBaselineText"));
          const QString displayedParameterText = edit->text();
          // Permanent item widgets can lose focus while a queued property
          // rebuild is replacing their rows. Compare against the text this
          // exact row was built from, not against a subsequently mutated
          // document value; otherwise an untouched stale editor can look
          // changed and write its old value into another parameter commit.
          edit->setProperty("sed.thingParameterBaselineText",
                            displayedParameterText.trimmed());
          applyInheritedParameterAppearance(
              edit, item,
              parameter.templateValue.has_value() &&
                  !parameter.localOverride);
          edit->setToolTip(parameter.templateValue
                               ? QStringLiteral("Editing this value creates or "
                                                "updates a Thing override.")
                               : QStringLiteral("Thing instance parameter."));
          layout->addWidget(edit, 1);

          QToolButton *pickResourceBtn = nullptr;
          if (resourcePickerKind) {
            pickResourceBtn = new QToolButton(row);
            pickResourceBtn->setText("...");
            pickResourceBtn->setAutoRaise(true);
            pickResourceBtn->setToolTip("Pick resource");
            layout->addWidget(pickResourceBtn);
            openAuxiliaryEditorOnDoubleClick(edit, pickResourceBtn);
          }

          QToolButton *editLightValueBtn = nullptr;
          const bool regularLightValue =
              sed::text::iequalsAscii(keyName, "light");
          const bool headlightIntensityValue =
              sed::text::iequalsAscii(keyName, "lightintensity");
          const bool headlightOffsetValue =
              sed::text::iequalsAscii(keyName, "lightoffset");
          if (regularLightValue || headlightIntensityValue ||
              headlightOffsetValue) {
            editLightValueBtn = new QToolButton(row);
            editLightValueBtn->setObjectName(
                QStringLiteral("prop.thing.value.light_editor.%1")
                    .arg(QString::fromStdString(keyName).toLower()));
            editLightValueBtn->setText(QStringLiteral("..."));
            editLightValueBtn->setAutoRaise(true);
            editLightValueBtn->setToolTip(
                QStringLiteral("Edit light parameters"));
            layout->addWidget(editLightValueBtn);
            openAuxiliaryEditorOnDoubleClick(edit, editLightValueBtn);
          }

          QToolButton *editSchemaValueBtn = nullptr;
          const thing::parameter_editor::ValueEditorInfo schemaEditorInfo =
              thing::parameter_editor::valueEditorInfo(level.kind, keyName,
                                                       thingType.toStdString());
          bool schemaEditorSelectionCompatible = true;
          if (sed::text::iequalsAscii(keyName, "typeflags")) {
            const std::optional<thing::parameter_schema::FlagFamily>
                activeFamily = thing::parameter_schema::flagFamily(
                    keyName, thingType.toStdString());
            for (const std::int32_t selectedIndex :
                 document_.selection().things) {
              if (selectedIndex < 0 ||
                  static_cast<std::size_t>(selectedIndex) >=
                      level.things.size()) {
                continue;
              }
              const Thing &selectedThing =
                  level.things[static_cast<std::size_t>(selectedIndex)];
              const TplValue *selectedType = sed::level::thingFindValue(
                  level, templatesDb, selectedThing, "type");
              const std::optional<thing::parameter_schema::FlagFamily>
                  selectedFamily = thing::parameter_schema::flagFamily(
                      keyName,
                      selectedType ? selectedType->asString() : std::string{});
              if (selectedFamily != activeFamily) {
                schemaEditorSelectionCompatible = false;
                break;
              }
            }
          }
          if (!schemaEditorSelectionCompatible) {
            edit->setEnabled(false);
            edit->setToolTip(
                QStringLiteral("Selected Things use different typeflags "
                               "families; edit them separately."));
          }
          if (schemaEditorInfo.kind !=
              thing::parameter_editor::ValueEditorKind::none) {
            editSchemaValueBtn = new QToolButton(row);
            editSchemaValueBtn->setObjectName(
                QStringLiteral("prop.thing.value.schema_editor.%1")
                    .arg(QString::fromStdString(keyName).toLower()));
            editSchemaValueBtn->setText(schemaEditorInfo.buttonText);
            editSchemaValueBtn->setAutoRaise(true);
            editSchemaValueBtn->setFocusPolicy(Qt::NoFocus);
            editSchemaValueBtn->setEnabled(schemaEditorSelectionCompatible);
            editSchemaValueBtn->setToolTip(
                schemaEditorSelectionCompatible
                    ? schemaEditorInfo.toolTip
                    : QStringLiteral("Selected Things use different typeflags "
                                     "families; edit them separately."));
            layout->addWidget(editSchemaValueBtn);
            openAuxiliaryEditorOnDoubleClick(edit, editSchemaValueBtn);
          }

          auto *del = new QToolButton(row);
          del->setAutoRaise(true);
          const bool anySelectedInherited =
              anySelectedThingParameter(keyName, false, true);
          const bool anySelectedRestorable =
              anySelectedThingParameter(keyName, true, true);
          if (anySelectedInherited) {
            del->setObjectName(
                QStringLiteral("prop.thing.value.restore.%1")
                    .arg(QString::fromStdString(keyName).toLower()));
            del->setText(QStringLiteral("\u21b6"));
            del->setEnabled(anySelectedRestorable);
            del->setToolTip(
                anySelectedRestorable
                    ? QStringLiteral(
                          "Restore each selected Thing's own template value.")
                    : QStringLiteral("All selected Things already use their "
                                     "template value."));
          } else {
            del->setObjectName(
                QStringLiteral("prop.thing.value.remove.%1")
                    .arg(QString::fromStdString(keyName).toLower()));
            del->setText("-");
            del->setToolTip("Remove Thing parameter");
          }
          layout->addWidget(del);

          thingParameters_->setItemWidget(item, 1, row);

          const TplValue semanticSource = v;
          const auto applyThingValueText =
              [thingIndex, keyName, semanticSource,
               templatesDb](editor::DocumentState &state,
                            const std::string &nextText) -> bool {
            auto applyOne =
                [&](Thing &th) -> thing::parameters::OverrideWriteResult {
              const TplValue *targetSemanticSource = sed::level::thingFindValue(
                  state.level, templatesDb, th, keyName);
              return thing::parameters::upsertOverrideDetailed(
                  th, keyName, nextText,
                  targetSemanticSource ? targetSemanticSource
                                       : &semanticSource);
            };

            bool any = false;
            if (!state.selection.things.empty()) {
              for (const std::int32_t idx : state.selection.things) {
                if (idx < 0 || static_cast<std::size_t>(idx) >=
                                   state.level.things.size()) {
                  continue;
                }
                const thing::parameters::OverrideWriteResult result =
                    applyOne(state.level.things[static_cast<std::size_t>(idx)]);
                if (result == thing::parameters::OverrideWriteResult::invalid) {
                  return false;
                }
                any =
                    result == thing::parameters::OverrideWriteResult::changed ||
                    any;
              }
              return any;
            }

            if (thingIndex < 0 || static_cast<std::size_t>(thingIndex) >=
                                      state.level.things.size()) {
              return false;
            }
            return applyOne(
                       state.level
                           .things[static_cast<std::size_t>(thingIndex)]) ==
                   thing::parameters::OverrideWriteResult::changed;
          };

          const auto applyThingFlagValueDelta =
              [thingIndex, keyName, semanticSource, templatesDb,
               projectType =
                   level.kind](editor::DocumentState &state,
                               const std::uint32_t originalEditorValue,
                               const std::uint32_t editedValue) -> bool {
            auto applyOne = [&](Thing &th) -> bool {
              const TplValue *targetSemanticSource = sed::level::thingFindValue(
                  state.level, templatesDb, th, keyName);
              const std::uint32_t current =
                  thingParameterFlagBits(targetSemanticSource).value_or(0u);
              std::optional<std::uint32_t> next{};
              if (state.selection.things.size() <= 1u) {
                if (current != editedValue) {
                  next = editedValue;
                }
              } else {
                const TplValue *targetType = sed::level::thingFindValue(
                    state.level, templatesDb, th, "type");
                const std::vector<thing::parameter_schema::FlagDefinition>
                    definitions = thing::parameter_schema::flagDefinitions(
                        projectType, keyName,
                        targetType ? targetType->asString() : std::string{});
                next = thing::parameter_editor::applyProtectedFlagEditDelta(
                    current, originalEditorValue, editedValue, definitions);
              }
              if (!next) {
                return false;
              }
              return thing::parameters::upsertOverrideDetailed(
                         th, keyName,
                         QStringLiteral("0x%1").arg(*next, 0, 16).toStdString(),
                         targetSemanticSource ? targetSemanticSource
                                              : &semanticSource) ==
                     thing::parameters::OverrideWriteResult::changed;
            };

            bool any = false;
            if (!state.selection.things.empty()) {
              for (const std::int32_t idx : state.selection.things) {
                if (idx < 0 || static_cast<std::size_t>(idx) >=
                                   state.level.things.size()) {
                  continue;
                }
                any = applyOne(
                          state.level.things[static_cast<std::size_t>(idx)]) ||
                      any;
              }
              return any;
            }

            if (thingIndex < 0 || static_cast<std::size_t>(thingIndex) >=
                                      state.level.things.size()) {
              return false;
            }
            return applyOne(
                state.level.things[static_cast<std::size_t>(thingIndex)]);
          };

          connect(
              edit, &QLineEdit::editingFinished, this,
              [this, edit, thingIndex, keyName, applyUndoable,
               applyThingValueText, applyThingFlagValueDelta,
               numericTextType, editorKind = schemaEditorInfo.kind]() {
                const QPointer<QLineEdit> guardedEdit(edit);
                const auto setEditorText = [guardedEdit, numericTextType](const QString& canonical) {
                  if (guardedEdit) {
                    ui::setTypedNumericLineEditText(
                        *guardedEdit, canonical, numericTextType,
                        QByteArrayLiteral("sed.thingParameterBaselineText"));
                  }
                };
                const QString next = edit->text().trimmed();
                if (next ==
                    edit->property("sed.thingParameterBaselineText")
                        .toString()) {
                  if (ui::isTypedNumericText(numericTextType) && edit->isModified()) {
                    setEditorText(edit->property("sed.typedNumericCanonicalText").toString());
                  }
                  return;
                }

                const Level &lvl = document_.level();
                if (thingIndex < 0 ||
                    static_cast<std::size_t>(thingIndex) >= lvl.things.size()) {
                  return;
                }

                const Thing &th =
                    lvl.things[static_cast<std::size_t>(thingIndex)];
                const sed::assets::Templates *templatesDb =
                    templates_.size() > 0 ? &templates_ : nullptr;
                const TplValue *curVal =
                    sed::level::thingFindValue(lvl, templatesDb, th, keyName);
                if (!curVal) {
                  return;
                }

                const QString cur = QString::fromStdString(curVal->asString());
                const std::optional<QString> canonicalNext =
                    ui::canonicalTypedNumericText(next, numericTextType, edit->locale());
                if (!canonicalNext) {
                  setEditorText(cur);
                  return;
                }
                const std::string nextStd = canonicalNext->toStdString();
                if (editorKind ==
                    thing::parameter_editor::ValueEditorKind::flags) {
                  TplValue parsedNext{};
                  parsedNext.name = keyName;
                  parsedNext.type = ValueType::flag;
                  const std::optional<std::uint32_t> originalEditorValue =
                      thingParameterFlagBits(curVal);
                  if (!originalEditorValue || !parsedNext.parse(nextStd)) {
                    edit->setText(cur);
                    edit->setProperty("sed.thingParameterBaselineText",
                                      cur.trimmed());
                    return;
                  }

                  const std::uint32_t editedValue =
                      static_cast<std::uint32_t>(parsedNext.intValue);
                  const bool okApply = applyUndoable(
                      ui::undo::thingParameterAction(document_.selection(),
                                                     keyName, "changed"),
                      editor::DocumentDirty::Things,
                      [applyThingFlagValueDelta,
                       originalEditorValue = *originalEditorValue,
                       editedValue](editor::DocumentState &state) -> bool {
                        return applyThingFlagValueDelta(
                            state, originalEditorValue, editedValue);
                      });
                  if (!okApply) {
                    edit->setText(cur);
                    edit->setProperty("sed.thingParameterBaselineText",
                                      cur.trimmed());
                  } else {
                    edit->setProperty("sed.thingParameterBaselineText", next);
                  }
                  return;
                }

                const bool okApply = applyUndoable(
                    ui::undo::thingParameterAction(document_.selection(),
                                                   keyName, "changed"),
                    editor::DocumentDirty::Things,
                    [applyThingValueText,
                     nextStd](editor::DocumentState &state) -> bool {
                      return applyThingValueText(state, nextStd);
                    });

                if (!okApply) {
                  setEditorText(cur);
                } else if (guardedEdit) {
                  const TplValue* settled = sed::level::thingFindValue(
                      document_.level(), templatesDb, document_.level().things[
                          static_cast<std::size_t>(thingIndex)], keyName);
                  setEditorText(settled ? QString::fromStdString(settled->asString())
                                        : *canonicalNext);
                }
              });

          if (pickResourceBtn && resourcePickerKind) {
            pickResourceBtn->setFocusPolicy(Qt::NoFocus);
            const QPointer<QLineEdit> guardedResourceEdit(edit);
            connect(
                pickResourceBtn, &QToolButton::clicked, this,
                [this, guardedResourceEdit, keyName, resourcePickerKind,
                 openResourcePicker, applyThingValueText, applyUndoable]() {
                  if (!guardedResourceEdit || !resourcePickerKind) {
                    return;
                  }
                  const auto picked = openResourcePicker(
                      *resourcePickerKind, guardedResourceEdit->text(),
                      QString("Pick %1").arg(QString::fromStdString(keyName)));
                  if (!picked) {
                    return;
                  }

                  const std::string pickedValue =
                      picked->trimmed().toStdString();
                  (void)applyUndoable(
                      ui::undo::thingParameterAction(document_.selection(),
                                                     keyName, "changed"),
                      editor::DocumentDirty::Things,
                      [applyThingValueText,
                       pickedValue](editor::DocumentState &state) -> bool {
                        return applyThingValueText(state, pickedValue);
                      });
                });
          }

          if (editLightValueBtn) {
            const QPointer<QToolButton> guardedLightEditor(editLightValueBtn);
            connect(editLightValueBtn, &QToolButton::clicked, this,
                    [this, thingIndex, keyName, guardedLightEditor]() {
                      openThingLightValueDialog(
                          thingIndex, keyName, guardedLightEditor);
                    });
          }

          if (editSchemaValueBtn) {
            const QPointer<QLineEdit> guardedEdit(edit);
            const QPointer<QToolButton> guardedSchemaButton(editSchemaValueBtn);
            connect(
                editSchemaValueBtn, &QToolButton::clicked, this,
                [this, guardedEdit, guardedSchemaButton, keyName, thingIndex,
                 applyThingValueText, applyThingFlagValueDelta, applyUndoable,
                 templatesDb, editorKind = schemaEditorInfo.kind,
                 projectType = level.kind, effectiveThingType = thingType]() {
                  if (!guardedEdit) {
                    return;
                  }
                  const QString currentValue = guardedEdit->text();
                  std::optional<std::uint32_t> originalEditorFlagValue{};
                  if (editorKind ==
                      thing::parameter_editor::ValueEditorKind::flags) {
                    const Level &currentLevel = document_.level();
                    if (thingIndex >= 0 &&
                        static_cast<std::size_t>(thingIndex) <
                            currentLevel.things.size()) {
                      originalEditorFlagValue =
                          thingParameterFlagBits(sed::level::thingFindValue(
                              currentLevel, templatesDb,
                              currentLevel
                                  .things[static_cast<std::size_t>(thingIndex)],
                              keyName));
                    }
                  }
                  if (const std::optional<QString> picked =
                          thing::parameter_editor::openValueEditor(
                              this, projectType, keyName,
                              effectiveThingType.toStdString(), currentValue,
                              guardedSchemaButton)) {
                    const std::string pickedValue =
                        picked->trimmed().toStdString();
                    if (originalEditorFlagValue) {
                      TplValue parsedPicked{};
                      parsedPicked.name = keyName;
                      parsedPicked.type = ValueType::flag;
                      if (!parsedPicked.parse(pickedValue)) {
                        return;
                      }
                      const std::uint32_t editedValue =
                          static_cast<std::uint32_t>(parsedPicked.intValue);
                      (void)applyUndoable(
                          ui::undo::thingParameterAction(document_.selection(),
                                                         keyName, "changed"),
                          editor::DocumentDirty::Things,
                          [applyThingFlagValueDelta,
                           originalEditorValue = *originalEditorFlagValue,
                           editedValue](editor::DocumentState &state) -> bool {
                            return applyThingFlagValueDelta(
                                state, originalEditorValue, editedValue);
                          });
                      return;
                    }
                    (void)applyUndoable(
                        ui::undo::thingParameterAction(document_.selection(),
                                                       keyName, "changed"),
                        editor::DocumentDirty::Things,
                        [applyThingValueText,
                         pickedValue](editor::DocumentState &state) -> bool {
                          return applyThingValueText(state, pickedValue);
                        });
                  }
                });
          }

          if (del) {
            const bool restoresTemplate = anySelectedInherited;
            connect(
                del, &QToolButton::clicked, this,
                [this, thingIndex, keyName, restoresTemplate, templatesDb,
                 applyUndoable]() {
                  (void)applyUndoable(
                      ui::undo::thingParameterAction(
                          document_.selection(), keyName,
                          restoresTemplate ? "restored from template"
                                           : "removed"),
                      editor::DocumentDirty::Things,
                      [thingIndex, keyName, restoresTemplate,
                       templatesDb](editor::DocumentState &state) -> bool {
                        auto applyOne = [&](const std::int32_t index) -> bool {
                          return removeThingParameterPreservingFrameSelection(
                              state, index, [&](Thing &th) {
                                if (restoresTemplate &&
                                    !thing::parameters::inheritedValue(
                                        state.level, templatesDb, th, keyName)) {
                                  return false;
                                }
                                return thing::parameters::removeOverride(
                                    th, keyName);
                              });
                        };

                        bool any = false;
                        if (!state.selection.things.empty()) {
                          for (const std::int32_t idx :
                               state.selection.things) {
                            if (idx < 0 || static_cast<std::size_t>(idx) >=
                                               state.level.things.size()) {
                              continue;
                            }
                            any |= applyOne(idx);
                          }
                          return any;
                        }

                        if (thingIndex < 0 ||
                            static_cast<std::size_t>(thingIndex) >=
                                state.level.things.size()) {
                          return false;
                        }

                        return applyOne(thingIndex);
                      });
                });
          }
        }
        if (thingParameterSearch_) {
          thingParameterSearch_->textChanged(thingParameterSearch_->text());
        }
      }

      setLinkedCogList(buildLinkedCogRefs(CogType::thing, thingIndex));

      properties_->expandAll();
      thingParameters_->expandAll();
      return;
    }
  }

  if (!document_.selection().lights.empty()) {
    {
      const std::int32_t lightIndex = *document_.selection().lights.begin();
      if (lightIndex < 0 ||
          static_cast<std::size_t>(lightIndex) >= level.lights.size()) {
        return;
      }

      const SedLight &light =
          level.lights[static_cast<std::size_t>(lightIndex)];

      auto addChild = [&](QTreeWidgetItem *parent, const QString &key,
                          const QString &value = {}) -> QTreeWidgetItem * {
        const int parentIndent =
            parent ? parent->data(0, kPropertyIndentRole).toInt() : -1;
        const int indent = parent ? (parentIndent + 1) : 0;

        auto *item = new QTreeWidgetItem(properties_);
        item->setText(0, QString(indent * 2, QChar(' ')) + key);
        item->setText(1, value);
        item->setData(0, kPropertyIndentRole, indent);
        return item;
      };

      auto addGroup = [&](QTreeWidgetItem *parent,
                          const QString &title) -> QTreeWidgetItem * {
        const int parentIndent =
            parent ? parent->data(0, kPropertyIndentRole).toInt() : -1;
        const int indent = parent ? (parentIndent + 1) : 0;

        auto *item = new QTreeWidgetItem(properties_);
        item->setText(0, QString(indent * 2, QChar(' ')) + title);
        item->setFirstColumnSpanned(true);
        item->setChildIndicatorPolicy(
            QTreeWidgetItem::DontShowIndicatorWhenChildless);
        item->setFlags(item->flags() & ~Qt::ItemIsSelectable);
        item->setData(0, kPropertyIndentRole, indent);
        QFont f = item->font(0);
        f.setBold(true);
        item->setFont(0, f);
        item->setExpanded(true);
        return item;
      };

      auto *root = addGroup(nullptr, "General");

      {
        auto *item = addChild(root, "Parameters");
        auto *button = new QToolButton(properties_);
        button->setObjectName(QStringLiteral("prop.baked_light.editor"));
        button->setText(QStringLiteral("Edit..."));
        button->setToolTip(
            level.kind == ProjectType::JKDF2
                ? QStringLiteral("Edit baked-light intensity and range "
                                 "together with live viewport preview.")
                : QStringLiteral("Edit baked-light color, intensity, and range "
                                 "together with live viewport preview."));
        button->setToolButtonStyle(Qt::ToolButtonTextOnly);
        properties_->setItemWidget(item, 1, button);
        connect(button, &QToolButton::clicked, this, [this, lightIndex]() {
          openBakedLightParametersDialog(lightIndex);
        });
      }

      auto colorToQ = [](const ColorF &c) -> QColor {
        return QColor::fromRgbF(std::clamp(static_cast<double>(c.r), 0.0, 1.0),
                                std::clamp(static_cast<double>(c.g), 0.0, 1.0),
                                std::clamp(static_cast<double>(c.b), 0.0, 1.0),
                                1.0);
      };

      auto qToColor = [](const QColor &qc) -> ColorF {
        return ColorF{
            clamp01(static_cast<float>(qc.redF())),
            clamp01(static_cast<float>(qc.greenF())),
            clamp01(static_cast<float>(qc.blueF())),
            1.0f,
        };
      };

      auto setSwatch = [](QPushButton *button, const QColor &c) {
        ui::configureColorSwatchButton(button, c);
      };

      // Flags (editable, hex)
      {
        auto *item = addChild(root, "Flags");
        auto *row = new QWidget(properties_);
        auto *rowLayout = new QHBoxLayout(row);
        rowLayout->setContentsMargins(0, 0, 0, 0);
        rowLayout->setSpacing(4);

        auto *edit = new QLineEdit(row);
        edit->setText(hex(static_cast<std::uint32_t>(light.flags)));
        rowLayout->addWidget(edit, 1);

        auto *pickBtn = new QToolButton(row);
        pickBtn->setText("...");
        pickBtn->setToolTip("Edit flags");
        pickBtn->setAutoRaise(true);
        rowLayout->addWidget(pickBtn);
        openAuxiliaryEditorOnDoubleClick(edit, pickBtn);

        properties_->setItemWidget(item, 1, row);
        connect(
            edit, &QLineEdit::editingFinished, this,
            [this, edit, lightIndex, hex, applyUndoable]() {
              const Level &lvl = document_.level();
              if (lightIndex < 0 ||
                  static_cast<std::size_t>(lightIndex) >= lvl.lights.size()) {
                return;
              }

              const SedLight &curLight =
                  lvl.lights[static_cast<std::size_t>(lightIndex)];
              const QString cur =
                  hex(static_cast<std::uint32_t>(curLight.flags));
              const QString next = edit->text().trimmed();
              if (next == cur) {
                return;
              }

              const std::optional<std::uint32_t> parsed =
                  sed::text::parseHex(next.toStdString());
              if (!parsed) {
                edit->setText(cur);
                return;
              }

              (void)applyUndoable(
                  "Baked Light: Flags changed", editor::DocumentDirty::Lights,
                  [&](editor::DocumentState &state) -> bool {
                    auto applyOne = [&](SedLight &l) -> bool {
                      const auto nextFlags = static_cast<std::int32_t>(*parsed);
                      if (l.flags == nextFlags) {
                        return false;
                      }
                      l.flags = nextFlags;
                      return true;
                    };

                    bool any = false;
                    if (!state.selection.lights.empty()) {
                      for (const std::int32_t idx : state.selection.lights) {
                        if (idx < 0 || static_cast<std::size_t>(idx) >=
                                           state.level.lights.size()) {
                          continue;
                        }
                        any |= applyOne(
                            state.level.lights[static_cast<std::size_t>(idx)]);
                      }
                      return any;
                    }

                    if (lightIndex < 0 ||
                        static_cast<std::size_t>(lightIndex) >=
                            state.level.lights.size()) {
                      return false;
                    }

                    return applyOne(
                        state.level
                            .lights[static_cast<std::size_t>(lightIndex)]);
                  });
            });

        connect(pickBtn, &QToolButton::clicked, this,
                [this, edit, openFlagEditor, light,
                 levelKind = level.kind]() {
                  if (!edit) {
                    return;
                  }

                  std::vector<FlagDef> defs{
                      {static_cast<std::uint32_t>(sed::LF_NoBlock),
                       "0x01 - Not blocked",
                       "Light is not occluded by geometry during light "
                       "calculations."},
                  };
                  if (levelKind == ProjectType::MOTS) {
                    defs.push_back(
                        {static_cast<std::uint32_t>(sed::LF_AffectArchThings),
                         "0x20 - Affect Arch Things",
                         "Also bake this light into the ARCHLIGHTING vertices "
                         "of MotS Arch Things."});
                  }

                  const auto picked = openFlagEditor(
                      "Baked Light: Flags changed",
                      static_cast<std::uint32_t>(light.flags), defs);
                  if (!picked) {
                    return;
                  }

                  edit->setText(QString("0x%1").arg(*picked, 0, 16));
                  QMetaObject::invokeMethod(edit, "editingFinished",
                                            Qt::DirectConnection);
                });
      }

      {
        const int layerCount =
            std::max(1, static_cast<int>(level.layers.size()));
        const int currentLayer = std::clamp(light.layer, 0, layerCount - 1);

        auto *item = addChild(root, "Layer");
        auto *combo = new QComboBox(properties_);
        combo->setObjectName(QStringLiteral("prop.baked_light.layer"));
        for (int i = 0; i < layerCount; ++i) {
          QString layerName = QString("Layer%1").arg(i);
          if (static_cast<std::size_t>(i) < level.layers.size()) {
            const std::string &raw = level.layers[static_cast<std::size_t>(i)];
            if (!raw.empty()) {
              layerName = QString::fromStdString(raw);
            }
          }
          combo->addItem(QString("%1: %2").arg(i).arg(layerName), i);
        }
        combo->setCurrentIndex(currentLayer);
        properties_->setItemWidget(item, 1, combo);

        connect(
            combo, qOverload<int>(&QComboBox::currentIndexChanged), this,
            [this, combo, lightIndex, applyUndoable](int row) {
              if (!combo || row < 0) {
                return;
              }
              const int newLayer = combo->itemData(row).toInt();
              if (applyUndoable(
                      "Baked Light: Layer changed",
                      editor::DocumentDirty::Lights,
                      [lightIndex,
                       newLayer](editor::DocumentState &state) -> bool {
                        auto applyOne =
                            [newLayer](SedLight &lightValue) -> bool {
                          if (lightValue.layer == newLayer) {
                            return false;
                          }
                          lightValue.layer = newLayer;
                          return true;
                        };

                        bool any = false;
                        if (!state.selection.lights.empty()) {
                          for (const std::int32_t idx :
                               state.selection.lights) {
                            if (idx < 0 || static_cast<std::size_t>(idx) >=
                                               state.level.lights.size()) {
                              continue;
                            }
                            any |= applyOne(
                                state.level
                                    .lights[static_cast<std::size_t>(idx)]);
                          }
                          return any;
                        }

                        if (lightIndex < 0 ||
                            static_cast<std::size_t>(lightIndex) >=
                                state.level.lights.size()) {
                          return false;
                        }

                        return applyOne(
                            state.level
                                .lights[static_cast<std::size_t>(lightIndex)]);
                      },
                      true)) {
                rebuildOutliner();
              }
            });
      }

      // Range (editable)
      {
        auto *item = addChild(root, "Range");
        auto *scrub = new ui::ScrubFloatEdit(properties_);
        scrub->setObjectName(QStringLiteral("prop.baked_light.range"));
        scrub->setDecimals(4);
        scrub->setSingleStep(0.01);
        scrub->setRange(0.0, 1.0e6);
        scrub->setValue(light.range);
        properties_->setItemWidget(item, 1, scrub);
        auto onCommit = [this, lightIndex, applyUndoable](double v) {
          (void)applyUndoable(
              "Baked Light: Range changed", editor::DocumentDirty::Lights,
              [&](editor::DocumentState &state) -> bool {
                const double clamped = std::max(0.0, v);

                auto applyOne = [&](SedLight &l) -> void { l.range = clamped; };

                bool any = false;
                if (!state.selection.lights.empty()) {
                  for (const std::int32_t idx : state.selection.lights) {
                    if (idx < 0 || static_cast<std::size_t>(idx) >=
                                       state.level.lights.size()) {
                      continue;
                    }
                    applyOne(state.level.lights[static_cast<std::size_t>(idx)]);
                    any = true;
                  }
                  return any;
                }

                if (lightIndex < 0 || static_cast<std::size_t>(lightIndex) >=
                                          state.level.lights.size()) {
                  return false;
                }
                applyOne(
                    state.level.lights[static_cast<std::size_t>(lightIndex)]);
                return true;
              });
        };
        connect(scrub, &ui::ScrubFloatEdit::valueCommitted, this, onCommit);

        connectLiveScrub(
            scrub, "Baked Light: Range changed", editor::DocumentDirty::Lights,
            [lightIndex](editor::DocumentState &state, double v) -> bool {
              const double clamped = std::max(0.0, v);

              auto applyOne = [&](SedLight &l) -> void { l.range = clamped; };

              bool any = false;
              if (!state.selection.lights.empty()) {
                for (const std::int32_t idx : state.selection.lights) {
                  if (idx < 0 || static_cast<std::size_t>(idx) >=
                                     state.level.lights.size()) {
                    continue;
                  }
                  applyOne(state.level.lights[static_cast<std::size_t>(idx)]);
                  any = true;
                }
                return any;
              }

              if (lightIndex < 0 || static_cast<std::size_t>(lightIndex) >=
                                        state.level.lights.size()) {
                return false;
              }
              applyOne(
                  state.level.lights[static_cast<std::size_t>(lightIndex)]);
              return true;
            });
      }

      // Intensity (editable for all project types)
      {
        auto *item = addChild(root, "Intensity");
        auto *scrub = new ui::ScrubFloatEdit(properties_);
        scrub->setObjectName(QStringLiteral("prop.baked_light.intensity"));
        scrub->setDecimals(6);
        scrub->setSingleStep(0.1);
        scrub->setRange(0.0, 1.0e6);
        scrub->setValue(light.intensity);
        properties_->setItemWidget(item, 1, scrub);
        auto onCommit = [this, lightIndex, applyUndoable](double v) {
          (void)applyUndoable(
              "Baked Light: Intensity changed", editor::DocumentDirty::Lights,
              [&](editor::DocumentState &state) -> bool {
                const double clamped = std::max(0.0, v);

                auto applyOne = [&](SedLight &l) -> void {
                  l.intensity = clamped;
                };

                bool any = false;
                if (!state.selection.lights.empty()) {
                  for (const std::int32_t idx : state.selection.lights) {
                    if (idx < 0 || static_cast<std::size_t>(idx) >=
                                       state.level.lights.size()) {
                      continue;
                    }
                    applyOne(state.level.lights[static_cast<std::size_t>(idx)]);
                    any = true;
                  }
                  return any;
                }

                if (lightIndex < 0 || static_cast<std::size_t>(lightIndex) >=
                                          state.level.lights.size()) {
                  return false;
                }
                applyOne(
                    state.level.lights[static_cast<std::size_t>(lightIndex)]);
                return true;
              });
        };
        connect(scrub, &ui::ScrubFloatEdit::valueCommitted, this, onCommit);

        connectLiveScrub(
            scrub, "Baked Light: Intensity changed",
            editor::DocumentDirty::Lights,
            [lightIndex](editor::DocumentState &state, double v) -> bool {
              const double clamped = std::max(0.0, v);

              auto applyOne = [&](SedLight &l) -> void {
                l.intensity = clamped;
              };

              bool any = false;
              if (!state.selection.lights.empty()) {
                for (const std::int32_t idx : state.selection.lights) {
                  if (idx < 0 || static_cast<std::size_t>(idx) >=
                                     state.level.lights.size()) {
                    continue;
                  }
                  applyOne(state.level.lights[static_cast<std::size_t>(idx)]);
                  any = true;
                }
                return any;
              }

              if (lightIndex < 0 || static_cast<std::size_t>(lightIndex) >=
                                        state.level.lights.size()) {
                return false;
              }
              applyOne(
                  state.level.lights[static_cast<std::size_t>(lightIndex)]);
              return true;
            });
      }

      // Color (editable for non-JKDF2 projects)
      if (level.kind != ProjectType::JKDF2) {
        auto *item = addChild(root, "Color");
        auto *button = new QPushButton(properties_);
        button->setObjectName(QStringLiteral("prop.baked_light.color"));
        setSwatch(button, colorToQ(light.color));
        properties_->setItemWidget(item, 1, button);
        connectLiveColor(
            button, colorToQ(light.color), false, "Baked Light: Color changed",
            editor::DocumentDirty::Lights,
            [lightIndex, qToColor](editor::DocumentState &state,
                                   const QColor &color) -> bool {
              const ColorF chosen = qToColor(color);
              auto applyOne = [&](SedLight &lightValue) -> bool {
                const ColorF before = lightValue.color;
                lightValue.color = chosen;
                lightValue.color.a = 1.0f;
                const ColorF after = lightValue.color;
                return before.r != after.r || before.g != after.g ||
                       before.b != after.b;
              };

              bool any = false;
              if (!state.selection.lights.empty()) {
                for (const std::int32_t idx : state.selection.lights) {
                  if (idx < 0 || static_cast<std::size_t>(idx) >=
                                     state.level.lights.size()) {
                    continue;
                  }
                  any |= applyOne(
                      state.level.lights[static_cast<std::size_t>(idx)]);
                }
                return any;
              }

              if (lightIndex < 0 || static_cast<std::size_t>(lightIndex) >=
                                        state.level.lights.size()) {
                return false;
              }

              return applyOne(
                  state.level.lights[static_cast<std::size_t>(lightIndex)]);
            },
            true);
      }

      // Position (editable)
      {
        auto *grp = addGroup(nullptr, "Position");

        auto addAxis = [&](const QString &key, const double value,
                           const std::function<double(const SedLight &)> &get,
                           const std::function<void(SedLight &, double)> &set) {
          auto *item = addChild(grp, key);
          auto *scrub = new ui::ScrubFloatEdit(properties_);
          scrub->setDecimals(6);
          scrub->setSingleStep(0.01);
          scrub->setValue(value);
          properties_->setItemWidget(item, 1, scrub);

          auto onCommit = [this, lightIndex, get, set,
                           applyUndoable](double v) {
            (void)applyUndoable(
                "Baked Light: Position changed", editor::DocumentDirty::Lights,
                [&](editor::DocumentState &state) -> bool {
                  auto applyOne = [&](SedLight &l) -> bool {
                    const double before = get(l);
                    if (before == v) {
                      return false;
                    }
                    set(l, v);
                    return true;
                  };

                  bool any = false;
                  if (!state.selection.lights.empty()) {
                    for (const std::int32_t idx : state.selection.lights) {
                      if (idx < 0 || static_cast<std::size_t>(idx) >=
                                         state.level.lights.size()) {
                        continue;
                      }
                      any |= applyOne(
                          state.level.lights[static_cast<std::size_t>(idx)]);
                    }
                    return any;
                  }

                  if (lightIndex < 0 || static_cast<std::size_t>(lightIndex) >=
                                            state.level.lights.size()) {
                    return false;
                  }

                  return applyOne(
                      state.level.lights[static_cast<std::size_t>(lightIndex)]);
                });
          };

          connect(scrub, &ui::ScrubFloatEdit::valueCommitted, this, onCommit);

          connectLiveScrub(
              scrub, "Baked Light: Position changed",
              editor::DocumentDirty::Lights,
              [lightIndex, get, set](editor::DocumentState &state,
                                     double v) -> bool {
                auto applyOne = [&](SedLight &l) -> bool {
                  const double before = get(l);
                  if (before == v) {
                    return false;
                  }
                  set(l, v);
                  return true;
                };

                bool any = false;
                if (!state.selection.lights.empty()) {
                  for (const std::int32_t idx : state.selection.lights) {
                    if (idx < 0 || static_cast<std::size_t>(idx) >=
                                       state.level.lights.size()) {
                      continue;
                    }
                    any |= applyOne(
                        state.level.lights[static_cast<std::size_t>(idx)]);
                  }
                  return any;
                }

                if (lightIndex < 0 || static_cast<std::size_t>(lightIndex) >=
                                          state.level.lights.size()) {
                  return false;
                }

                return applyOne(
                    state.level.lights[static_cast<std::size_t>(lightIndex)]);
              });
        };

        addAxis(
            "X", light.position.x,
            [](const SedLight &l) { return l.position.x; },
            [](SedLight &l, double v) { l.position.x = v; });
        addAxis(
            "Y", light.position.y,
            [](const SedLight &l) { return l.position.y; },
            [](SedLight &l, double v) { l.position.y = v; });
        addAxis(
            "Z", light.position.z,
            [](const SedLight &l) { return l.position.z; },
            [](SedLight &l, double v) { l.position.z = v; });
      }

      properties_->expandAll();
      return;
    }
  }

  if (!document_.selection().cogs.empty()) {
    const std::int32_t cogIndex = *document_.selection().cogs.begin();

    auto addChild = [&](QTreeWidgetItem *parent, const QString &key,
                        const QString &value = {}) -> QTreeWidgetItem * {
      const int parentIndent =
          parent ? parent->data(0, kPropertyIndentRole).toInt() : -1;
      const int indent = parent ? (parentIndent + 1) : 0;

      auto *item = new QTreeWidgetItem(properties_);
      item->setText(0, QString(indent * 2, QChar(' ')) + key);
      item->setText(1, value);
      item->setData(0, kPropertyIndentRole, indent);
      return item;
    };

    auto addGroup = [&](QTreeWidgetItem *parent,
                        const QString &title) -> QTreeWidgetItem * {
      const int parentIndent =
          parent ? parent->data(0, kPropertyIndentRole).toInt() : -1;
      const int indent = parent ? (parentIndent + 1) : 0;

      auto *item = new QTreeWidgetItem(properties_);
      item->setText(0, QString(indent * 2, QChar(' ')) + title);
      item->setFirstColumnSpanned(true);
      item->setChildIndicatorPolicy(
          QTreeWidgetItem::DontShowIndicatorWhenChildless);
      item->setFlags(item->flags() & ~Qt::ItemIsSelectable);
      item->setData(0, kPropertyIndentRole, indent);
      QFont f = item->font(0);
      f.setBold(true);
      item->setFont(0, f);
      item->setExpanded(true);
      return item;
    };

    if (cogIndex < 0 ||
        static_cast<std::size_t>(cogIndex) >= level.cogs.size()) {
      return;
    }

    const Cog &cog = level.cogs[static_cast<std::size_t>(cogIndex)];

    auto *general = addGroup(nullptr, "General");
    addChild(general, "Index", QString::number(cogIndex));
    addChild(general, "Id", QString::number(cog.id));

    // Script (read-only; edit in dedicated editor)
    {
      auto *item = addChild(general, "Script");
      item->setData(0, kPropertyActionRole,
                    static_cast<int>(PropertyAction::openCogScript));
      item->setData(0, kPropertyCogRole, static_cast<int>(cogIndex));

      auto *row = new QWidget(properties_);
      auto *rowLayout = new QHBoxLayout(row);
      rowLayout->setContentsMargins(0, 0, 0, 0);
      rowLayout->setSpacing(4);

      auto *edit = new QLineEdit(row);
      edit->setReadOnly(true);
      edit->setText(QString::fromStdString(cog.name));
      edit->setProperty("sed.cogScriptName", QString::fromStdString(cog.name));
      edit->installEventFilter(this);
      rowLayout->addWidget(edit, 1);

      auto *pickBtn = new QToolButton(row);
      pickBtn->setText("Edit...");
      pickBtn->setAutoRaise(true);
      pickBtn->setToolTip("Edit current COG script");
      rowLayout->addWidget(pickBtn);

      properties_->setItemWidget(item, 1, row);
      connect(pickBtn, &QToolButton::clicked, this, [this, cogIndex]() {
        const Level &lvl = document_.level();
        if (cogIndex < 0 ||
            static_cast<std::size_t>(cogIndex) >= lvl.cogs.size()) {
          return;
        }
        openCogScriptEditor(lvl.cogs[static_cast<std::size_t>(cogIndex)].name);
      });
    }

    auto cogTypeLabel = [](const CogType type) -> QString {
      switch (type) {
      case CogType::ai:
        return "ai";
      case CogType::cog:
        return "cog";
      case CogType::key:
        return "key";
      case CogType::mat:
        return "material";
      case CogType::msg:
        return "message";
      case CogType::model3do:
        return "3do";
      case CogType::sec:
        return "sector";
      case CogType::wav:
        return "wav";
      case CogType::surface:
        return "surface";
      case CogType::template_:
        return "template";
      case CogType::thing:
        return "thing";
      case CogType::int_:
        return "int";
      case CogType::float_:
        return "float";
      case CogType::vect:
        return "vector";
      default:
        return QString::fromStdString(std::string(sed::toString(type)));
      }
    };

    auto currentCogEntityJedValue = [this, &level,
                                     formatSurfaceCogEditorValue]()
        -> std::function<std::optional<QString>(CogType)> {
      auto bestSurfaceRef =
          [](const std::unordered_set<SurfaceRef, editor::SurfaceRefHash,
                                      editor::SurfaceRefEq> &refs)
          -> std::optional<SurfaceRef> {
        if (refs.empty()) {
          return std::nullopt;
        }

        SurfaceRef best{};
        bool haveBest = false;
        for (const SurfaceRef &candidate : refs) {
          if (!haveBest || candidate.sector < best.sector ||
              (candidate.sector == best.sector &&
               candidate.surface < best.surface)) {
            best = candidate;
            haveBest = true;
          }
        }

        if (!haveBest) {
          return std::nullopt;
        }
        return best;
      };

      auto bestSurfaceVertexRef =
          [](const std::unordered_set<
              editor::SurfaceVertexRef, editor::SurfaceVertexRefHash,
              editor::SurfaceVertexRefEq> &refs) -> std::optional<SurfaceRef> {
        if (refs.empty()) {
          return std::nullopt;
        }

        SurfaceRef best{};
        bool haveBest = false;
        for (const editor::SurfaceVertexRef &ref : refs) {
          const SurfaceRef candidate = ref.surface;
          if (!haveBest || candidate.sector < best.sector ||
              (candidate.sector == best.sector &&
               candidate.surface < best.surface)) {
            best = candidate;
            haveBest = true;
          }
        }

        if (!haveBest) {
          return std::nullopt;
        }
        return best;
      };

      return [this, &level, bestSurfaceRef, bestSurfaceVertexRef,
              formatSurfaceCogEditorValue](
                 const CogType type) -> std::optional<QString> {
        const editor::Selection &sel = document_.selection();

        switch (type) {
        case CogType::sec: {
          if (editMode_ == EditMode::sector && !sel.sectors.empty()) {
            const std::int32_t sectorIndex =
                *std::min_element(sel.sectors.begin(), sel.sectors.end());
            if (sectorIndex >= 0 &&
                static_cast<std::size_t>(sectorIndex) < level.sectors.size()) {
              return QString::number(
                  level.sectors[static_cast<std::size_t>(sectorIndex)].num);
            }
          }

          if (editMode_ == EditMode::surface) {
            if (const std::optional<SurfaceRef> ref =
                    bestSurfaceRef(sel.surfaces)) {
              if (ref->sector >= 0 && static_cast<std::size_t>(ref->sector) <
                                          level.sectors.size()) {
                return QString::number(
                    level.sectors[static_cast<std::size_t>(ref->sector)].num);
              }
            }
            if (const std::optional<SurfaceRef> ref =
                    bestSurfaceVertexRef(sel.surfaceVertices)) {
              if (ref->sector >= 0 && static_cast<std::size_t>(ref->sector) <
                                          level.sectors.size()) {
                return QString::number(
                    level.sectors[static_cast<std::size_t>(ref->sector)].num);
              }
            }
          }

          break;
        }
        case CogType::surface: {
          if (editMode_ == EditMode::surface) {
            if (const std::optional<SurfaceRef> ref =
                    bestSurfaceRef(sel.surfaces)) {
              if (ref->sector >= 0 && ref->surface >= 0 &&
                  static_cast<std::size_t>(ref->sector) <
                      level.sectors.size() &&
                  static_cast<std::size_t>(ref->surface) <
                      level.sectors[static_cast<std::size_t>(ref->sector)]
                          .surfaces.size()) {
                return formatSurfaceCogEditorValue(*ref);
              }
            }
            if (const std::optional<SurfaceRef> ref =
                    bestSurfaceVertexRef(sel.surfaceVertices)) {
              if (ref->sector >= 0 && ref->surface >= 0 &&
                  static_cast<std::size_t>(ref->sector) <
                      level.sectors.size() &&
                  static_cast<std::size_t>(ref->surface) <
                      level.sectors[static_cast<std::size_t>(ref->sector)]
                          .surfaces.size()) {
                return formatSurfaceCogEditorValue(*ref);
              }
            }
          }

          break;
        }
        case CogType::thing: {
          if (editMode_ == EditMode::thing && !sel.things.empty()) {
            const std::int32_t thingIndex =
                *std::min_element(sel.things.begin(), sel.things.end());
            if (thingIndex >= 0 &&
                static_cast<std::size_t>(thingIndex) < level.things.size()) {
              return QString::number(
                  level.things[static_cast<std::size_t>(thingIndex)].num);
            }
          }
          break;
        }
        default:
          break;
        }

        return std::nullopt;
      };
    }();

    auto *valuesGroup = addGroup(nullptr, "Variables");
    QTreeWidgetItem *pendingFocusItem = nullptr;
    QWidget *pendingFocusWidget = nullptr;

    for (std::size_t vi = 0; vi < cog.values.size(); ++vi) {
      const CogValue &v = cog.values[vi];
      const QString typeName = cogTypeLabel(v.cogType);
      const QString description =
          QString::fromStdString(v.description).trimmed();
      const QString key =
          QString("%1 : %2").arg(QString::fromStdString(v.name)).arg(typeName);

      auto *item = addChild(valuesGroup, key);
      auto *edit = new QLineEdit(properties_);
      const ValueType numericTextType = v.cogType == CogType::float_
          ? ValueType::floating : v.cogType == CogType::vect ? ValueType::vect : v.type;
      ui::configureTypedNumericLineEdit(*edit, cogValueEditorText(v), numericTextType,
          QByteArrayLiteral("sed.cogValueBaselineText"));
      if (!description.isEmpty()) {
        item->setToolTip(0, description);
        item->setToolTip(1, description);
        edit->setToolTip(description);
      }

      if (cogValueFocusPending_ && cogValueFocusCogIndex_ == cogIndex &&
          cogValueFocusValueIndex_ == static_cast<std::int32_t>(vi)) {
        pendingFocusItem = item;
        pendingFocusWidget = edit;
      }

      const bool isEntityRef =
          (v.cogType == CogType::sec || v.cogType == CogType::surface ||
           v.cogType == CogType::thing);
      const bool isCogRef = (v.cogType == CogType::cog);
      const std::optional<ui::ResourcePickerKind> resourcePickerKind =
          isCogRef ? std::nullopt
                   : ui::resourcePickerKindForCogType(v.cogType);
      if (ui::cogTypeNeedsDoubleClickLineEditFallback(v.cogType)) {
        // Event-filter fallback: double-clicking the embedded line edit should
        // still navigate or open the relevant resource picker like the old
        // Delphi editor.
        edit->setProperty("sed.cogEntityCogIndex", static_cast<int>(cogIndex));
        edit->setProperty("sed.cogEntityValueIndex", static_cast<int>(vi));
        edit->installEventFilter(this);
      }

      connect(
          edit, &QLineEdit::editingFinished, this,
          [this, edit, cogIndex, vi, applyUndoable, cogValueEditorText,
           parseCogValueEditorText, numericTextType]() {
            const Level &lvl = document_.level();
            if (cogIndex < 0 ||
                static_cast<std::size_t>(cogIndex) >= lvl.cogs.size()) {
              return;
            }

            const Cog &cg = lvl.cogs[static_cast<std::size_t>(cogIndex)];
            if (vi >= cg.values.size()) {
              return;
            }

            const QPointer<QLineEdit> guardedEdit(edit);
            const auto setEditorText = [guardedEdit, numericTextType](const QString& canonical) {
              if (guardedEdit) {
                ui::setTypedNumericLineEditText(*guardedEdit, canonical, numericTextType,
                    QByteArrayLiteral("sed.cogValueBaselineText"));
              }
            };
            const QString cur = cogValueEditorText(cg.values[vi]);
            if (edit->text().trimmed() == edit->property("sed.cogValueBaselineText").toString()) {
              setEditorText(cur);
              return;
            }
            const std::optional<QString> next = ui::canonicalTypedNumericText(
                edit->text().trimmed(), numericTextType, edit->locale());
            if (!next || *next == cur) {
              setEditorText(cur);
              return;
            }

            const std::string nextText = next->toStdString();
            std::vector<std::int32_t> targets;
            targets.reserve(document_.selection().cogs.size());
            for (const std::int32_t idx : document_.selection().cogs) {
              targets.push_back(idx);
            }
            if (targets.empty()) {
              targets.push_back(cogIndex);
            }
            const bool ok = applyUndoable(
                "COG: Value changed", editor::DocumentDirty::Cogs,
                [targets, vi, nextText, cogValueEditorText,
                 parseCogValueEditorText](
                    editor::DocumentState &state) -> bool {
                  bool any = false;
                  std::vector<std::pair<std::int32_t, CogValue>> updates;
                  updates.reserve(targets.size());

                  for (const std::int32_t idx : targets) {
                    if (idx < 0 || static_cast<std::size_t>(idx) >=
                                       state.level.cogs.size()) {
                      continue;
                    }

                    Cog &c = state.level.cogs[static_cast<std::size_t>(idx)];
                    if (vi >= c.values.size()) {
                      continue;
                    }

                    CogValue parsed = c.values[vi];
                    if (!parseCogValueEditorText(
                            state.level, c.values[vi],
                            QString::fromStdString(nextText), parsed)) {
                      return false;
                    }

                    const QString curText = cogValueEditorText(c.values[vi]);
                    const QString parsedText = cogValueEditorText(parsed);
                    if (parsedText != curText) {
                      updates.emplace_back(idx, std::move(parsed));
                      any = true;
                    }
                  }

                  if (!any) {
                    return false;
                  }

                  for (auto &u : updates) {
                    const std::int32_t idx = u.first;
                    if (idx < 0 || static_cast<std::size_t>(idx) >=
                                       state.level.cogs.size()) {
                      continue;
                    }
                    Cog &c = state.level.cogs[static_cast<std::size_t>(idx)];
                    if (vi >= c.values.size()) {
                      continue;
                    }
                    c.values[vi] = std::move(u.second);
                  }

                  return true;
                });

            if (!ok) {
              setEditorText(cur);
            } else if (guardedEdit) {
              setEditorText(cogValueEditorText(
                  document_.level().cogs[static_cast<std::size_t>(cogIndex)].values[vi]));
            }
          });

      if (!isEntityRef && !isCogRef && !resourcePickerKind) {
        properties_->setItemWidget(item, 1, edit);
        continue;
      }

      auto *row = new QWidget(properties_);
      auto *rowLayout = new QHBoxLayout(row);
      rowLayout->setContentsMargins(0, 0, 0, 0);
      rowLayout->setSpacing(4);

      QToolButton *materialPreviewBtn = nullptr;
      if (v.cogType == CogType::mat) {
        materialPreviewBtn = new QToolButton(row);
        ui::configureMaterialPreviewButton(materialPreviewBtn);
        updateMaterialPreviewButton(materialPreviewBtn, edit->text());
        rowLayout->addWidget(materialPreviewBtn);
      }
      rowLayout->addWidget(edit, 1);
      if (!description.isEmpty()) {
        row->setToolTip(description);
      }

      QToolButton *entityPickBtn = nullptr;
      QToolButton *entityCurrentBtn = nullptr;
      if (isEntityRef) {
        entityCurrentBtn = new QToolButton(row);
        entityCurrentBtn->setText("Current");
        entityCurrentBtn->setToolTip("Use current selected entity");
        entityCurrentBtn->setAutoRaise(true);
        if (!description.isEmpty()) {
          entityCurrentBtn->setToolTip(description +
                                       "\n\nUse current selected entity");
        }
        rowLayout->addWidget(entityCurrentBtn);

        entityPickBtn = new QToolButton(row);
        entityPickBtn->setText("Pick");
        entityPickBtn->setToolTip("Pick entity from viewport");
        entityPickBtn->setAutoRaise(true);
        if (!description.isEmpty()) {
          entityPickBtn->setToolTip(description +
                                    "\n\nPick entity from viewport");
        }
        rowLayout->addWidget(entityPickBtn);
      }

      QToolButton *resourcePickBtn = nullptr;
      if (resourcePickerKind) {
        resourcePickBtn = new QToolButton(row);
        resourcePickBtn->setText("...");
        resourcePickBtn->setToolTip("Pick resource");
        resourcePickBtn->setAutoRaise(true);
        if (!description.isEmpty()) {
          resourcePickBtn->setToolTip(description + "\n\nPick resource");
        }
        rowLayout->addWidget(resourcePickBtn);
      }

      QToolButton *cogInstancePickBtn = nullptr;
      if (isCogRef) {
        cogInstancePickBtn = new QToolButton(row);
        cogInstancePickBtn->setObjectName(
            QStringLiteral("cogInstancePickButton"));
        cogInstancePickBtn->setText("...");
        cogInstancePickBtn->setToolTip("Pick placed COG instance");
        cogInstancePickBtn->setAutoRaise(true);
        if (!description.isEmpty()) {
          cogInstancePickBtn->setToolTip(
              description + "\n\nPick placed COG instance");
        }
        rowLayout->addWidget(cogInstancePickBtn);
      }

      if (materialPreviewBtn) {
        connect(edit, &QLineEdit::textChanged, this,
                [updateMaterialPreviewButton,
                 materialPreviewBtn](const QString &text) {
                  updateMaterialPreviewButton(materialPreviewBtn, text);
                });
      }

      properties_->setItemWidget(item, 1, row);

      if (!isEntityRef && !isCogRef) {
        if (resourcePickerKind) {
          item->setData(0, kPropertyActionRole,
                        static_cast<int>(PropertyAction::openCogResourceValue));
          item->setData(0, kPropertyCogRole, static_cast<int>(cogIndex));
          item->setData(0, kPropertyValueIndexRole, static_cast<int>(vi));
          item->setData(0, kPropertyResourceKindRole,
                        static_cast<int>(*resourcePickerKind));
        }

        if (resourcePickBtn && resourcePickerKind) {
          auto pickResourceValue = [
              this,
              edit,
              resourcePickerKind,
              openResourcePicker,
              cogIndex,
              vi,
              sourceValue = v,
              sourceOwnerId = cog.id,
              sourceOwnerName = cog.name]()
          {
            QPointer<MainWindow> ownerGuard(this);
            QPointer<QLineEdit> editGuard(edit);
            if ( !editGuard || !resourcePickerKind
                || !documentEditingAllowed() )
            {
              return;
            }

            const Level& sourceLevel = document_.level();
            if ( cogIndex < 0
                || static_cast<std::size_t>(cogIndex) >= sourceLevel.cogs.size() )
            {
              return;
            }
            const Cog& sourceOwner =
                sourceLevel.cogs[static_cast<std::size_t>(cogIndex)];
            if ( sourceOwner.id != sourceOwnerId
                || sourceOwner.name != sourceOwnerName
                || vi >= sourceOwner.values.size()
                || !sameCogValueSnapshot(sourceOwner.values[vi], sourceValue) )
            {
              return;
            }

            const std::uint64_t sourceDocumentEpoch = recoveryDocumentEpoch_;
            const QString sourceText = editGuard->text().trimmed();
            const editor::Selection sourceSelection = document_.selection();
            const auto picked = openResourcePicker(
                *resourcePickerKind,
                sourceText,
                QString::fromStdString(
                    ui::resourcePickerTitle(*resourcePickerKind)));
            if ( !ownerGuard || !editGuard || !picked )
            {
              return;
            }

            if ( recoveryDocumentEpoch_ != sourceDocumentEpoch
                || !documentEditingAllowed()
                || editGuard->text().trimmed() != sourceText )
            {
              return;
            }

            const Level& currentLevel = document_.level();
            if ( static_cast<std::size_t>(cogIndex) >= currentLevel.cogs.size() )
            {
              return;
            }
            const Cog& currentOwner =
                currentLevel.cogs[static_cast<std::size_t>(cogIndex)];
            if ( currentOwner.id != sourceOwnerId
                || currentOwner.name != sourceOwnerName
                || vi >= currentOwner.values.size()
                || !sameCogValueSnapshot(currentOwner.values[vi], sourceValue)
                || !sameEditorSelectionSnapshot(
                    document_.selection(), sourceSelection) )
            {
              return;
            }

            if ( picked->trimmed() == sourceText )
            {
              return;
            }

            cogValueFocusPending_ = true;
            cogValueFocusCogIndex_ = static_cast<std::int32_t>(cogIndex);
            cogValueFocusValueIndex_ = static_cast<std::int32_t>(vi);
            editGuard->setText(*picked);
            QMetaObject::invokeMethod(
                editGuard.data(), "editingFinished", Qt::DirectConnection);
          };
          if (materialPreviewBtn) {
            connect(materialPreviewBtn, &QToolButton::clicked, this,
                    pickResourceValue);
          }
          connect(resourcePickBtn, &QToolButton::clicked, this,
                  pickResourceValue);
        }
        continue;
      }

      // Double-click on row (or line edit via event filter) navigates to the
      // referenced entity/cog.
      item->setData(0, kPropertyActionRole,
                    static_cast<int>(PropertyAction::selectCogEntityValue));
      item->setData(0, kPropertyCogRole, static_cast<int>(cogIndex));
      item->setData(0, kPropertyValueIndexRole, static_cast<int>(vi));
      item->setData(0, kPropertySectorRole, -1);
      item->setData(0, kPropertySurfaceRole, -1);
      item->setData(0, kPropertyEntityIndexRole, -1);

      switch (v.cogType) {
      case CogType::cog: {
        item->setData(
            0, kPropertyActionRole,
            static_cast<int>(PropertyAction::selectCogReferenceValue));
        if (v.intValue >= 0 &&
            static_cast<std::size_t>(v.intValue) < level.cogs.size()) {
          item->setData(0, kPropertyEntityIndexRole,
                        static_cast<int>(v.intValue));
        }
        break;
      }
      case CogType::sec: {
        if (v.type == ValueType::ptr && v.intValue >= 0 &&
            static_cast<std::size_t>(v.intValue) < level.sectors.size()) {
          item->setData(0, kPropertyOutlinerKindRole,
                        static_cast<int>(OutlinerKind::sector));
          item->setData(0, kPropertyEntityIndexRole,
                        static_cast<int>(v.intValue));
        }
        break;
      }
      case CogType::thing: {
        if (v.type == ValueType::ptr && v.intValue >= 0 &&
            static_cast<std::size_t>(v.intValue) < level.things.size()) {
          item->setData(0, kPropertyOutlinerKindRole,
                        static_cast<int>(OutlinerKind::thing));
          item->setData(0, kPropertyEntityIndexRole,
                        static_cast<int>(v.intValue));
        }
        break;
      }
      case CogType::surface: {
        if (const std::optional<SurfaceRef> ref =
                tryResolveSurfaceCogValueRef(v)) {
          item->setData(0, kPropertyOutlinerKindRole,
                        static_cast<int>(OutlinerKind::surface));
          item->setData(0, kPropertyEntityIndexRole,
                        static_cast<int>(ref->surface));
          item->setData(0, kPropertySectorRole, static_cast<int>(ref->sector));
          item->setData(0, kPropertySurfaceRole,
                        static_cast<int>(ref->surface));
        }
        break;
      }
      default:
        break;
      }

      if (entityPickBtn) {
        connect(entityPickBtn, &QToolButton::clicked, this,
                [this, cogIndex, vi]() {
                  if (!documentEditingAllowed()) {
                    return;
                  }
                  const Level &lvl = document_.level();
                  if (cogIndex < 0 ||
                      static_cast<std::size_t>(cogIndex) >= lvl.cogs.size()) {
                    return;
                  }

                  const Cog &cg = lvl.cogs[static_cast<std::size_t>(cogIndex)];
                  if (vi >= cg.values.size()) {
                    return;
                  }

                  const CogValue &cv = cg.values[vi];
                  if (cv.cogType != CogType::sec &&
                      cv.cogType != CogType::surface &&
                      cv.cogType != CogType::thing) {
                    return;
                  }

                  cogEntityPickActive_ = true;
                  cogEntityPickCogIndex_ = static_cast<std::int32_t>(cogIndex);
                  cogEntityPickValueIndex_ = static_cast<std::int32_t>(vi);
                  cogEntityPickType_ = cv.cogType;
                  cogEntityPickRestorePlacementMode_ = placementMode_;

                  // Prevent placement clicks (add cube/thing/light) while in
                  // temporary cog entity pick mode.
                  setPlacementMode(PlacementMode::none);

                  if (glView_) {
                    glView_->setViewportPickingEnabled(true);
                    glView_->setCogEntityPickHighlightEnabled(true);
                    glView_->setHoverPickEnabled(true);
                    glView_->setCursor(Qt::CrossCursor);
                    switch (cv.cogType) {
                    case CogType::sec:
                      glView_->setPickMode(GlView::PickMode::Sector);
                      break;
                    case CogType::surface:
                      glView_->setPickMode(GlView::PickMode::Surface);
                      break;
                    case CogType::thing:
                      glView_->setPickMode(GlView::PickMode::Thing);
                      break;
                    default:
                      break;
                    }
                  }

                  statusBar()->showMessage(
                      "Pick entity in viewport (Esc to cancel).");
                });
      }

      if (entityCurrentBtn) {
        connect(entityCurrentBtn, &QToolButton::clicked, this,
                [this, edit, cogIndex, vi, currentCogEntityJedValue]() {
                  if (!edit) {
                    return;
                  }

                  const Level &lvl = document_.level();
                  if (cogIndex < 0 ||
                      static_cast<std::size_t>(cogIndex) >= lvl.cogs.size()) {
                    return;
                  }

                  const Cog &cg = lvl.cogs[static_cast<std::size_t>(cogIndex)];
                  if (vi >= cg.values.size()) {
                    return;
                  }

                  const CogValue &cv = cg.values[vi];
                  const std::optional<QString> currentValue =
                      currentCogEntityJedValue(cv.cogType);
                  if (!currentValue) {
                    statusBar()->showMessage("No compatible current selection.",
                                             3000);
                    return;
                  }

                  cogValueFocusPending_ = true;
                  cogValueFocusCogIndex_ = static_cast<std::int32_t>(cogIndex);
                  cogValueFocusValueIndex_ = static_cast<std::int32_t>(vi);
                  edit->setText(*currentValue);
                  QMetaObject::invokeMethod(edit, "editingFinished",
                                            Qt::DirectConnection);
                });
      }

      if (cogInstancePickBtn) {
        connect(cogInstancePickBtn, &QToolButton::clicked, this,
                [this, edit, cogIndex, vi]() {
                  if (!edit || !documentEditingAllowed()
                      || selectionTouchesNonEditableLayer(
                          document_.selection())) {
                    return;
                  }

                  const Level &initialLevel = document_.level();
                  if (cogIndex < 0 ||
                      static_cast<std::size_t>(cogIndex) >=
                          initialLevel.cogs.size()) {
                    return;
                  }
                  const Cog &initialOwner =
                      initialLevel.cogs[static_cast<std::size_t>(cogIndex)];
                  if (vi >= initialOwner.values.size()
                      || initialOwner.values[vi].cogType != CogType::cog) {
                    return;
                  }

                  const std::int32_t initialOwnerId = initialOwner.id;
                  const std::string initialOwnerName = initialOwner.name;
                  const CogValue initialValue = initialOwner.values[vi];
                  const auto initialSelection = document_.selection().cogs;
                  const QPointer<QLineEdit> editGuard(edit);
                  const std::optional<std::int32_t> picked =
                      ui::pickPlacedCogInstance(
                          this, initialLevel, initialValue.intValue);
                  if (!picked || !editGuard) {
                    return;
                  }

                  const Level &currentLevel = document_.level();
                  if (cogIndex < 0 ||
                      static_cast<std::size_t>(cogIndex) >=
                          currentLevel.cogs.size()) {
                    return;
                  }
                  const Cog &currentOwner =
                      currentLevel.cogs[static_cast<std::size_t>(cogIndex)];
                  if (currentOwner.id != initialOwnerId
                      || currentOwner.name != initialOwnerName
                      || vi >= currentOwner.values.size()
                      || !sameCogValueSnapshot(
                          currentOwner.values[vi], initialValue)
                      || document_.selection().cogs != initialSelection
                      || (*picked >= 0
                          && static_cast<std::size_t>(*picked) >=
                              currentLevel.cogs.size())) {
                    return;
                  }
                  if (initialValue.intValue == *picked) {
                    return;
                  }

                  cogValueFocusPending_ = true;
                  cogValueFocusCogIndex_ =
                      static_cast<std::int32_t>(cogIndex);
                  cogValueFocusValueIndex_ = static_cast<std::int32_t>(vi);
                  editGuard->setText(QString::number(*picked));
                  QMetaObject::invokeMethod(
                      editGuard.data(), "editingFinished",
                      Qt::DirectConnection);
                });
      }
    }

    properties_->expandAll();
    if (pendingFocusItem) {
      properties_->setCurrentItem(pendingFocusItem);
      properties_->scrollToItem(pendingFocusItem,
                                QAbstractItemView::PositionAtCenter);
      if (pendingFocusWidget) {
        pendingFocusWidget->setFocus(Qt::OtherFocusReason);
      }
    }
    if (cogValueFocusPending_ && cogValueFocusCogIndex_ == cogIndex) {
      cogValueFocusPending_ = false;
      cogValueFocusCogIndex_ = -1;
      cogValueFocusValueIndex_ = -1;
    }
    return;
  }
}

void MainWindow::updateThingPropertiesPreview(
    const bool applyTransformPreview) {
  if (!properties_ || document_.selection().things.empty()) {
    return;
  }

  const Level &level = document_.level();
  const std::int32_t thingIndex =
      activeSelectedThingIndex(level, document_.selection(), glView_);
  if (thingIndex < 0 ||
      static_cast<std::size_t>(thingIndex) >= level.things.size()) {
    return;
  }

  const Thing &thing = level.things[static_cast<std::size_t>(thingIndex)];
  TVector pos{thing.x, thing.y, thing.z};
  TVector pyr{};
  pyr.pitch = thing.pch;
  pyr.yaw = thing.yaw;
  pyr.roll = thing.rol;
  std::int32_t sector = thing.sector;

  if (applyTransformPreview && transformPreview_.active) {
    if (!transformPreview_.rotating) {
      pos.x = pos.x + transformPreview_.translation.x;
      pos.y = pos.y + transformPreview_.translation.y;
      pos.z = pos.z + transformPreview_.translation.z;
    } else {
      TVector axis = transformPreview_.axis;
      const double axisLen = axis.Normalize();
      if (axisLen != 0.0 && std::abs(transformPreview_.angleDeg) > 1e-9) {
        const TMatrix43 rot = TMatrix43::CreateFromVectorAngle(
            axis, static_cast<float>(transformPreview_.angleDeg));
        const TVector pv = transformPreview_.pivot;

        TVector rel{pos.x - pv.x, pos.y - pv.y, pos.z - pv.z};
        rel = rot.TransformVector(rel);
        pos.x = rel.x + pv.x;
        pos.y = rel.y + pv.y;
        pos.z = rel.z + pv.z;

        const TMatrix43 oldRot = TMatrix43::CreateRotate(pyr);
        TMatrix43 newRot{};
        newRot.rvec = rot.TransformVector(oldRot.rvec);
        newRot.lvec = rot.TransformVector(oldRot.lvec);
        newRot.uvec = rot.TransformVector(oldRot.uvec);
        newRot.dvec = TVector::zero;
        newRot.Normalize();
        pyr = ui::transform::extractPYRCreateRotateCompatible(newRot);
      }
    }

    // GlView owns the level-generation containment index. Reuse it for every
    // preview sample and keep the current owner when it still contains the
    // transformed point; rebuilding or scanning the full Level here would
    // make large-level Move/Rotate drags stall.
    sector = glView_
        ? glView_->findContainingSector(pos, thing.sector)
        : sed::level::levelFindSectorForXYZ(level, pos.x, pos.y, pos.z);
  }

  bool anyUpdated = false;

  auto setScrub = [&](const char *name, const double value) -> void {
    if (auto *scrub = properties_->findChild<ui::ScrubFloatEdit *>(
            QString::fromLatin1(name))) {
      scrub->setValue(value);
      anyUpdated = true;
    }
  };

  setScrub("prop.thing.pos.x", pos.x);
  setScrub("prop.thing.pos.y", pos.y);
  setScrub("prop.thing.pos.z", pos.z);
  setScrub("prop.thing.rot.pitch", pyr.pitch);
  setScrub("prop.thing.rot.yaw", pyr.yaw);
  setScrub("prop.thing.rot.roll", pyr.roll);

  QTreeWidgetItem *sectorItem = nullptr;
  for (int i = 0, n = properties_->topLevelItemCount(); i < n; ++i) {
    QTreeWidgetItem *it = properties_->topLevelItem(i);
    if (!it) {
      continue;
    }

    if (it->text(0).trimmed() == "Sector") {
      sectorItem = it;
      break;
    }
  }

  if (sectorItem) {
    sectorItem->setText(1, QString::number(sector));
    anyUpdated = true;
  }

  if (!anyUpdated) {
    rebuildProperties();
  }
}

} // namespace sed
