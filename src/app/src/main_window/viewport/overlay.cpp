#include "main_window/common.hpp"
#include "render/gizmos/light_category_gizmo_utils.hpp"
#include "ui/components/collapsible_section.hpp"
#include "ui/components/flow_layout.hpp"
#include "ui/components/preview_resource_disk_watcher.hpp"
#include "ui/components/preview_thumbnail_list.hpp"
#include "ui/components/spin_scrub_utils.hpp"
#include "ui/components/ui_decimal_format.hpp"
#include "ui/components/thing_preview_render_utils.hpp"
#include "ui/project/project_startup_utils.hpp"
#include "ui/shortcuts/selection_modifier_utils.hpp"
#include "ui/shortcuts/key_sequence_display.hpp"
#include "ui/selection/selection_tool_availability_utils.hpp"
#include "ui/viewport/fog_option_utils.hpp"
#include "ui/viewport/thing_placement_drop_utils.hpp"
#include "ui/viewport/thing_placement_panel_resize_utils.hpp"

#include <QElapsedTimer>
#include <QPaintEvent>
#include <QFutureWatcher>
#include <QRegion>
#include <QResizeEvent>
#include <QStyleOptionMenuItem>
#include <QWidgetAction>
#include <QtConcurrent/QtConcurrentRun>

namespace sed {

namespace {
class ThingPlacementTemplateListWidget final : public QListWidget {
public:
  explicit ThingPlacementTemplateListWidget(QWidget *parent = nullptr)
      : QListWidget(parent) {}

protected:
  void keyPressEvent(QKeyEvent *event) override {
    if (event && (event->key() == Qt::Key_PageDown ||
                  event->key() == Qt::Key_PageUp)) {
      if (QScrollBar *scroll = verticalScrollBar()) {
        const int direction = event->key() == Qt::Key_PageDown ? 1 : -1;
        const int pageStep = ui::detail::previewThumbnailPageStep(
            scroll->pageStep(), gridSize().height());
        scroll->setValue(scroll->value() + direction * pageStep);
        event->accept();
        return;
      }
    }
    QListWidget::keyPressEvent(event);
  }

  void wheelEvent(QWheelEvent *event) override {
    if (event && (event->modifiers() & Qt::ShiftModifier) != 0) {
      if (QScrollBar *scroll = verticalScrollBar()) {
        const QPoint delta = event->angleDelta();
        const int dominantDelta =
            std::abs(delta.y()) >= std::abs(delta.x()) ? delta.y() : delta.x();
        if (dominantDelta != 0) {
          const int direction = dominantDelta < 0 ? 1 : -1;
          const int pageStep = ui::detail::previewThumbnailPageStep(
              scroll->pageStep(), gridSize().height());
          scroll->setValue(scroll->value() + direction * pageStep);
        }
      }
      event->accept();
      return;
    }
    QListWidget::wheelEvent(event);
    if (event) {
      event->accept();
    }
  }

  QMimeData *mimeData(const QList<QListWidgetItem *> &items) const override {
    QMimeData *mime = QListWidget::mimeData(items);
    if (!mime || items.isEmpty() || !items.front()) {
      return mime;
    }

    const QString name = items.front()->data(Qt::UserRole).toString().trimmed();
    if (!name.isEmpty()) {
      mime->setText(name);
      mime->setData(ui::viewport::kThingPlacementTemplateMimeType,
                    name.toUtf8());
    }
    return mime;
  }
};

class PersistentViewportSettingsMenu final
    : public ui::shortcuts::KeySequenceDisplayMenu {
public:
  explicit PersistentViewportSettingsMenu(
      QWidget *parent = nullptr, const bool roundedFloatingPanel = false)
      : ui::shortcuts::KeySequenceDisplayMenu(parent),
        roundedFloatingPanel_(roundedFloatingPanel) {
    // This is an in-viewport settings panel, not a transient native popup.
    // It remains visible while its controls are used; the owner dismisses
    // it when its toolbar button or something outside the panel is clicked.
    setWindowFlags(Qt::Widget);
    if (roundedFloatingPanel_) {
      // Stylesheet radii do not clip QMenu's backing store.  Make the
      // Thing panel transparent outside its painted rounded surface and
      // apply a real widget mask whenever its action layout changes.
      setAttribute(Qt::WA_TranslucentBackground, true);
      setAttribute(Qt::WA_NoSystemBackground, true);
      setWindowFlag(Qt::NoDropShadowWindowHint, true);
      setAutoFillBackground(false);
    }
  }

  void setScrollArea(QScrollArea *scrollArea) noexcept {
    scrollArea_ = scrollArea;
  }

protected:
  void paintEvent(QPaintEvent *event) override {
    if (!roundedFloatingPanel_) {
      QMenu::paintEvent(event);
      return;
    }

    // QMenu's native panel primitive includes an internal right/bottom
    // shadow on the Windows style.  It is inside the widget rectangle, so
    // neither the no-drop-shadow window hint nor an outer mask can remove
    // it. Paint only the intended rounded surface and one-pixel border,
    // then let the active style render the individual action rows.
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    if (event) {
      painter.setClipRegion(event->region());
    }

    constexpr qreal radius = 10.0;
    QPainterPath panelPath;
    panelPath.addRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5),
                             radius, radius);
    painter.fillPath(panelPath, QColor(QStringLiteral("#15171c")));
    painter.setPen(QPen(QColor(QStringLiteral("#343b46")), 1.0));
    painter.drawPath(panelPath);
    painter.setClipPath(panelPath, Qt::IntersectClip);

    for (QAction *action : actions()) {
      if (!action || !action->isVisible()) {
        continue;
      }
      QStyleOptionMenuItem option;
      initStyleOption(&option, action);
      option.rect = actionGeometry(action);
      style()->drawControl(QStyle::CE_MenuItem, &option, &painter, this);
    }
  }

  void resizeEvent(QResizeEvent *event) override {
    ui::shortcuts::KeySequenceDisplayMenu::resizeEvent(event);
    if (!roundedFloatingPanel_ || width() <= 0 || height() <= 0) {
      return;
    }

    constexpr qreal radius = 10.0;
    QPainterPath shape;
    shape.addRoundedRect(QRectF(0.0, 0.0, static_cast<qreal>(width()),
                                static_cast<qreal>(height())),
                         radius, radius);
    setMask(QRegion(shape.toFillPolygon().toPolygon()));
  }

  void wheelEvent(QWheelEvent *event) override {
    QScrollBar *scrollBar =
        scrollArea_ ? scrollArea_->verticalScrollBar() : nullptr;
    if (event && scrollBar && scrollBar->maximum() > scrollBar->minimum()) {
      const QPoint pixelDelta = event->pixelDelta();
      const QPoint angleDelta = event->angleDelta();
      const int delta = !pixelDelta.isNull()
                            ? pixelDelta.y()
                            : (angleDelta.y() / 120) *
                                  std::max(24, scrollBar->pageStep() / 8);
      if (delta != 0) {
        scrollBar->setValue(scrollBar->value() - delta);
        event->accept();
        return;
      }
    }
    QMenu::wheelEvent(event);
  }

  void mousePressEvent(QMouseEvent *event) override {
    // QMenu's popup implementation treats a press followed by a drag as
    // a transient-menu gesture and may clear/rebuild the action list when
    // this menu is hosted inside the persistent scroll area.  Keep the
    // panel's own left-button state local: child QWidgetActions still
    // receive their normal mouse events, while ordinary action rows are
    // committed by mouseReleaseEvent below.
    if (event && event->button() == Qt::LeftButton) {
      pressedAction_ = actionAt(event->position().toPoint());
      pressPosition_ = event->position().toPoint();
      dragging_ = false;
      if (pressedAction_) {
        setActiveAction(pressedAction_);
      }
      event->accept();
      return;
    }
    QMenu::mousePressEvent(event);
  }

  void mouseMoveEvent(QMouseEvent *event) override {
    // Do not let QMenu reinterpret an in-panel LMB drag as a popup
    // navigation gesture.  Hover feedback remains available and the
    // panel stays populated throughout the drag.
    if (event && (event->buttons() & Qt::LeftButton) != 0) {
      if (!dragging_ &&
          (event->position().toPoint() - pressPosition_).manhattanLength() >=
              QApplication::startDragDistance()) {
        dragging_ = true;
      }
      if (QAction *action = actionAt(event->position().toPoint())) {
        setActiveAction(action);
      }
      event->accept();
      return;
    }
    QMenu::mouseMoveEvent(event);
  }

  void mouseReleaseEvent(QMouseEvent *event) override {
    if (event && event->button() == Qt::LeftButton) {
      QAction *action = actionAt(event->position().toPoint());
      if (!dragging_ && action == pressedAction_.data() && action &&
          action->isEnabled() && !action->isSeparator() && !action->menu() &&
          !qobject_cast<QWidgetAction *>(action)) {
        action->trigger();
        setActiveAction(action);
        update();
        event->accept();
        pressedAction_.clear();
        return;
      }
      pressedAction_.clear();
      dragging_ = false;
      event->accept();
      return;
    }
    QMenu::mouseReleaseEvent(event);
  }

  void keyPressEvent(QKeyEvent *event) override {
    if (event &&
        (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter ||
         event->key() == Qt::Key_Space)) {
      QAction *action = activeAction();
      if (action && action->isEnabled() && !action->isSeparator() &&
          !action->menu() && !qobject_cast<QWidgetAction *>(action)) {
        action->trigger();
        update();
        event->accept();
        return;
      }
    }
    QMenu::keyPressEvent(event);
    ensureActiveActionVisible();
  }

private:
  void ensureActiveActionVisible() {
    QScrollBar *scrollBar =
        scrollArea_ ? scrollArea_->verticalScrollBar() : nullptr;
    QAction *action = activeAction();
    if (!scrollBar || !action || scrollBar->maximum() <= scrollBar->minimum()) {
      return;
    }

    const QRect actionRect = actionGeometry(action);
    const int visibleTop = scrollBar->value();
    const int visibleBottom = visibleTop + scrollArea_->viewport()->height();
    if (actionRect.top() < visibleTop) {
      scrollBar->setValue(actionRect.top());
    } else if (actionRect.bottom() >= visibleBottom) {
      scrollBar->setValue(actionRect.bottom() -
                          scrollArea_->viewport()->height() + 1);
    }
  }

  QPointer<QScrollArea> scrollArea_{};
  QPointer<QAction> pressedAction_{};
  QPoint pressPosition_{};
  bool dragging_{};
  bool roundedFloatingPanel_{};
};

constexpr int kThingPlacementTemplateCategoryRole = Qt::UserRole + 41;
constexpr int kThingPlacementTemplateSearchRole = Qt::UserRole + 42;
constexpr int kThingPlacementTemplatePreviewPendingRole = Qt::UserRole + 43;
constexpr int kThingPlacementTemplateCategoryNamesRole = Qt::UserRole + 44;
constexpr int kThingPlacementTemplateTypeRole = Qt::UserRole + 45;
constexpr int kThingPlacementCategoryKeyRole = Qt::UserRole + 46;
constexpr int kThingPlacementControlHeight = 28;
constexpr int kThingPlacementThumbnailOpenDelayMs = 180;
constexpr int kThingPlacementThumbnailBatchDelayMs = 32;
constexpr int kMoveOptionButtonExtent = 34;
constexpr int kMoveOptionIconExtent = 22;

QString viewportPopupActionRowsStyleSheet() {
  return QStringLiteral("QMenu::item {"
                        "  color: #ebecf0;"
                        "  margin: 2px 6px;"
                        "  padding: 7px 10px;"
                        "  border: 1px solid transparent;"
                        "  border-radius: 6px;"
                        "}"
                        "QMenu::item:selected {"
                        "  background-color: #292e36;"
                        "  border-color: #343b46;"
                        "}"
                        "QMenu::item:checked {"
                        "  background-color: #315f92;"
                        "  border-color: #70a9e6;"
                        "}"
                        "QMenu::indicator {"
                        "  width: 0px;"
                        "  height: 0px;"
                        "}"
                        "QMenu::separator {"
                        "  height: 1px;"
                        "  margin: 5px 8px;"
                        "  background: #343b46;"
                        "}");
}

QString viewportPopupMenuStyleSheet() {
  return QStringLiteral("QMenu {"
                        "  color: #ebecf0;"
                        "  background-color: #15171c;"
                        "  border: 1px solid #343b46;"
                        "  border-radius: 10px;"
                        "  padding: 4px;"
                        "}") +
         viewportPopupActionRowsStyleSheet();
}

struct ThingPlacementCategoryFilter {
  QString label{};
  QString key{};
  QString tooltip{};
  QStringList templateNames{};
};

std::optional<QString>
thingPlacementTemplateCategoryFileNameForProject(const ProjectType kind) {
  switch (kind) {
  case ProjectType::JKDF2:
    return QStringLiteral("tpls.jlh");
  case ProjectType::MOTS:
    return QStringLiteral("mtpls.jlh");
  case ProjectType::IJIM:
    return QStringLiteral("ijimtpls.jlh");
  default:
    return std::nullopt;
  }
}

std::optional<std::filesystem::path> findThingPlacementDataRoot() {
  std::error_code ec;
  const std::filesystem::path cwd = std::filesystem::current_path(ec);
  if (ec) {
    return std::nullopt;
  }

  for (std::filesystem::path cur = cwd; !cur.empty(); cur = cur.parent_path()) {
    const std::filesystem::path dataDir = cur / "data";
    if (std::filesystem::exists(dataDir, ec) &&
        std::filesystem::is_directory(dataDir, ec)) {
      return dataDir;
    }
    if (cur == cur.root_path()) {
      break;
    }
  }
  return std::nullopt;
}

std::optional<assets::JlhTree>
loadThingPlacementTemplateCategoryTree(const assets::ResourceLocator &resources,
                                       const ProjectType kind) {
  const std::optional<QString> fileName =
      thingPlacementTemplateCategoryFileNameForProject(kind);
  if (!fileName) {
    return std::nullopt;
  }

  if (const auto dataRoot = findThingPlacementDataRoot()) {
    const std::filesystem::path dataPath = *dataRoot / fileName->toStdString();
    std::error_code ec;
    if (std::filesystem::exists(dataPath, ec)) {
      try {
        return assets::loadFromJLH(dataPath);
      } catch (const std::exception &) {
      }
    }
  }

  try {
    return assets::loadFromJLH(fileName->toStdString(), resources);
  } catch (const std::exception &) {
  }
  return std::nullopt;
}

bool isThingPlacementInternalGeneralCategoryNode(
    const assets::JlhNode &node) noexcept {
  return node.list && node.children.empty() &&
         QString::fromStdString(node.label)
                 .compare(QStringLiteral("General"), Qt::CaseInsensitive) == 0;
}

void appendThingPlacementTemplateCategoryEntries(QStringList &out,
                                                 const assets::JlhNode &node) {
  auto addUnique = [&out](const QString &value) {
    const QString trimmed = value.trimmed();
    if (trimmed.isEmpty()) {
      return;
    }
    for (const QString &existing : out) {
      if (existing.compare(trimmed, Qt::CaseInsensitive) == 0) {
        return;
      }
    }
    out.push_back(trimmed);
  };

  if (node.list) {
    for (const std::string &entry : node.list->entries) {
      addUnique(QString::fromStdString(entry));
    }
  }

  for (const assets::JlhNode &child : node.children) {
    appendThingPlacementTemplateCategoryEntries(out, child);
  }
}

void collectThingPlacementCategoryFilters(
    std::vector<ThingPlacementCategoryFilter> &out, const assets::JlhNode &node,
    const QString &parentPath, const QString &keyPrefix) {
  if (isThingPlacementInternalGeneralCategoryNode(node)) {
    return;
  }

  const QString label = QString::fromStdString(node.label).trimmed();
  const QString path = parentPath.isEmpty()
                           ? label
                           : QStringLiteral("%1 / %2").arg(parentPath, label);
  const QString key = QStringLiteral("%1/%2").arg(keyPrefix, label).trimmed();

  QStringList names{};
  appendThingPlacementTemplateCategoryEntries(names, node);
  if (!label.isEmpty() && !names.isEmpty()) {
    out.push_back(ThingPlacementCategoryFilter{
        label,
        key,
        path,
        names,
    });
  }

  for (const assets::JlhNode &child : node.children) {
    collectThingPlacementCategoryFilters(out, child, path, key);
  }
}

QIcon makeThingPlacementChevronIcon(const qreal angleDeg, const QColor &color) {
  QPixmap pixmap(18, 18);
  pixmap.fill(Qt::transparent);

  QPainter painter(&pixmap);
  painter.setRenderHint(QPainter::Antialiasing, true);
  painter.translate(9.0, 9.0);
  painter.rotate(angleDeg);
  painter.translate(-9.0, -9.0);
  painter.setPen(QPen(color, 1.9, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
  painter.setBrush(Qt::NoBrush);
  QPolygonF chevron{};
  chevron << QPointF(4.5, 6.0) << QPointF(9.0, 11.0) << QPointF(13.5, 6.0);
  painter.drawPolyline(chevron);
  return QIcon(pixmap);
}

qreal thingPlacementChevronAngle(const int position, const bool expand) {
  switch (position) {
  case 1:
    return expand ? -90.0 : 90.0;
  case 0:
    return expand ? 90.0 : -90.0;
  case 2:
    return expand ? 0.0 : 180.0;
  case 3:
  default:
    return expand ? 180.0 : 0.0;
  }
}

std::optional<std::string>
thingPlacementTemplateFieldString(const ThingTemplate &tpl,
                                  const std::string_view fieldName) {
  for (const TplValue &value : tpl.values) {
    if (!sed::text::iequalsAscii(value.name, fieldName)) {
      continue;
    }

    const std::string text = sed::text::trim(
        value.stringValue.empty() ? value.asString() : value.stringValue);
    if (!text.empty() && !sed::text::iequalsAscii(text, "none")) {
      return text;
    }
  }
  return std::nullopt;
}

std::optional<std::string>
thingPlacementResolvedPreviewField(const ThingTemplate &tpl,
                                   const assets::Templates &templates,
                                   const std::string_view fieldName) {
  if (!tpl.name.empty()) {
    if (const TplValue *value = templates.findField(tpl.name, fieldName)) {
      const std::string text = sed::text::trim(
          value->stringValue.empty() ? value->asString() : value->stringValue);
      if (!text.empty() && !sed::text::iequalsAscii(text, "none")) {
        return text;
      }
    }
  }
  return thingPlacementTemplateFieldString(tpl, fieldName);
}

QPixmap makeThingPlacementSpritePreviewPixmap(const QImage &source,
                                              const QSize &targetSize) {
  if (source.isNull()) {
    return {};
  }

  const int width = std::max(64, targetSize.width());
  const int height = std::max(64, targetSize.height());
  QImage canvas(width, height, QImage::Format_ARGB32_Premultiplied);
  canvas.fill(QColor(18, 20, 28));

  QPainter p(&canvas);
  p.setRenderHint(QPainter::Antialiasing, true);
  p.setRenderHint(QPainter::SmoothPixmapTransform, true);

  constexpr int cell = 12;
  for (int y = 0; y < height; y += cell) {
    for (int x = 0; x < width; x += cell) {
      const bool odd = ((x / cell) + (y / cell)) % 2 != 0;
      p.fillRect(QRect(x, y, cell, cell),
                 odd ? QColor(44, 46, 50) : QColor(34, 36, 40));
    }
  }

  const int pad = std::max(8, std::min(width, height) / 14);
  const QSize fitSize(std::max(1, width - 2 * pad),
                      std::max(1, height - 2 * pad));
  const QImage scaled =
      source.scaled(fitSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
  const QPoint topLeft((width - scaled.width()) / 2,
                       (height - scaled.height()) / 2);
  p.drawImage(topLeft, scaled);

  p.setPen(QPen(QColor(0, 0, 0, 120), 1.0));
  p.drawRect(canvas.rect().adjusted(0, 0, -1, -1));
  return QPixmap::fromImage(canvas);
}

QIcon makeThingPlacementMarkerIcon(const QSize &targetSize,
                                   const QString &label, const bool hasBox) {
  const int width = std::max(64, targetSize.width());
  const int height = std::max(64, targetSize.height());
  QPixmap pix(width, height);
  pix.fill(Qt::transparent);

  QPainter p(&pix);
  p.setRenderHint(QPainter::Antialiasing, true);
  p.setRenderHint(QPainter::TextAntialiasing, true);

  const QRectF card(1.5, 1.5, width - 3.0, height - 3.0);
  p.setPen(QPen(QColor(255, 255, 255, 36), 1.0));
  p.setBrush(QColor(30, 34, 42, 232));
  p.drawRoundedRect(card, 8.0, 8.0);

  const QPointF center(width * 0.5, height * 0.44);
  const qreal r = std::min(width, height) * 0.24;
  p.setPen(QPen(QColor(120, 205, 255, 220), 2.0));
  p.setBrush(QColor(85, 150, 220, 42));
  if (hasBox) {
    const QRectF front(center.x() - r * 0.75, center.y() - r * 0.55, r * 1.25,
                       r * 1.05);
    const QPointF offset(r * 0.34, -r * 0.28);
    const QRectF back = front.translated(offset);
    p.drawRect(back);
    p.drawRect(front);
    p.drawLine(front.topLeft(), back.topLeft());
    p.drawLine(front.topRight(), back.topRight());
    p.drawLine(front.bottomLeft(), back.bottomLeft());
    p.drawLine(front.bottomRight(), back.bottomRight());
  } else {
    p.drawEllipse(center, r * 0.62, r * 0.62);
    p.drawLine(QPointF(center.x() - r, center.y()),
               QPointF(center.x() + r, center.y()));
    p.drawLine(QPointF(center.x(), center.y() - r),
               QPointF(center.x(), center.y() + r));
  }

  p.setPen(QColor(226, 231, 238, 210));
  QFont font = p.font();
  font.setPointSizeF(std::clamp(static_cast<qreal>(width) * 0.08, 7.0, 10.0));
  font.setBold(true);
  p.setFont(font);
  p.drawText(QRectF(card.left() + 5.0, card.bottom() - 21.0,
                    card.width() - 10.0, 15.0),
             Qt::AlignHCenter | Qt::AlignVCenter, label);

  return QIcon(pix);
}

constexpr std::size_t kThingPlacementTemplateIconCacheCapacity = 192u;

void rememberThingPlacementTemplateIcon(
    std::unordered_map<std::string, QIcon> &iconCache,
    const std::string &cacheKey, const QIcon &icon)
{
    if ( iconCache.size() >= kThingPlacementTemplateIconCacheCapacity
        && iconCache.find(cacheKey) == iconCache.end() )
    {
        iconCache.erase(iconCache.begin());
    }
    iconCache[cacheKey] = icon;
}

QIcon makeThingPlacementTemplateIcon(const ThingTemplate &tpl,
                                     const assets::Templates &templates,
                                     const assets::ResourceLocator &resources,
                                     const ProjectType projectType,
                                     const std::string_view cmpName,
                                     const QSize &iconSize,
                                     std::unordered_map<std::string, QIcon>
                                         &iconCache) {
  const std::string templateKey = sed::text::upperAscii(tpl.name);
  if (const std::optional<std::string> spriteName =
          thingPlacementResolvedPreviewField(tpl, templates, "sprite")) {
    const std::string cacheKey =
        std::string("tplspr:") + templateKey + "|" +
        sed::text::upperAscii(*spriteName) + "|deps=" +
        ui::detail::sharedPreviewSpriteDependencyRevisionToken(
            &resources, cmpName, *spriteName) +
        "|s=" + std::to_string(iconSize.width()) + "x" +
        std::to_string(iconSize.height());
    if (const auto it = iconCache.find(cacheKey); it != iconCache.end()) {
      return it->second;
    }
    if (const std::optional<QImage> image =
            ui::detail::loadSharedPreviewSpriteImage(&resources, cmpName,
                                                     *spriteName)) {
      QIcon icon(ui::detail::makeTemplatePreviewPixmap(*image, iconSize));
      rememberThingPlacementTemplateIcon(iconCache, cacheKey, icon);
      return icon;
    }
  }

  std::optional<std::string> modelName =
      thingPlacementResolvedPreviewField(tpl, templates, "model3d");
  if (!modelName) {
    modelName = thingPlacementResolvedPreviewField(tpl, templates, "model3do");
  }
  if (modelName) {
    if (const std::shared_ptr<const assets::Model3do> model =
            ui::detail::loadSharedPreviewModel(&resources, *modelName)) {
      const std::string cacheKey =
          std::string("tpl3do:") + templateKey + "|" +
          sed::text::upperAscii(*modelName) + "|deps=" +
          ui::detail::previewModelDependencyRevisionToken(
              &resources, cmpName, *modelName, model->materials) +
          "|p=" + std::to_string(static_cast<int>(projectType)) +
          "|s=" + std::to_string(iconSize.width()) + "x" +
          std::to_string(iconSize.height());
      if (const auto it = iconCache.find(cacheKey); it != iconCache.end()) {
        return it->second;
      }
      QIcon icon = ui::detail::makeTemplateModelPreviewIcon(
          *model, iconSize, &resources, cmpName, false);
      rememberThingPlacementTemplateIcon(iconCache, cacheKey, icon);
      return icon;
    }
  }

  return QIcon(ui::detail::makePreviewUnavailablePixmap(iconSize));
}

QPixmap makeThingPlacementImagePreviewPixmap(const QImage &source,
                                             const QSize &targetSize) {
  if (source.isNull()) {
    return {};
  }

  const int width = std::max(64, targetSize.width());
  const int height = std::max(64, targetSize.height());
  QImage canvas(width, height, QImage::Format_ARGB32_Premultiplied);
  canvas.fill(QColor(18, 20, 28));

  QPainter p(&canvas);
  p.setRenderHint(QPainter::Antialiasing, true);
  p.setRenderHint(QPainter::SmoothPixmapTransform, true);

  constexpr int cell = 10;
  for (int y = 0; y < height; y += cell) {
    for (int x = 0; x < width; x += cell) {
      const bool odd = ((x / cell) + (y / cell)) % 2 != 0;
      p.fillRect(QRect(x, y, cell, cell),
                 odd ? QColor(44, 46, 50) : QColor(34, 36, 40));
    }
  }

  const int pad = std::max(8, std::min(width, height) / 14);
  const QSize fitSize(std::max(1, width - 2 * pad),
                      std::max(1, height - 2 * pad));
  const QImage scaled =
      source.scaled(fitSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
  const QPoint topLeft((width - scaled.width()) / 2,
                       (height - scaled.height()) / 2);
  p.drawImage(topLeft, scaled);

  p.setPen(QPen(QColor(0, 0, 0, 120), 1.0));
  p.drawRect(canvas.rect().adjusted(0, 0, -1, -1));
  return QPixmap::fromImage(canvas);
}

std::optional<assets::CmpPalette>
loadThingPlacementPreviewPalette(const assets::ResourceLocator &resources,
                                 const std::string_view cmpName) {
  const std::string cmp = sed::text::trim(cmpName);
  if (cmp.empty()) {
    return std::nullopt;
  }

  const auto bytes = resources.tryReadBinaryWithExt(cmp, ".cmp");
  if (!bytes) {
    return std::nullopt;
  }
  return assets::parseCmpPalette(*bytes);
}

std::optional<ThingBox>
thingPlacementResolvedPreviewBBox(const ThingTemplate &tpl,
                                  const assets::Templates &templates,
                                  const std::string_view tplName) {
  if (!tpl.bbox.IsZero()) {
    return tpl.bbox;
  }

  std::string current = std::string(tplName);
  for (int depth = 0; depth < 256; ++depth) {
    const ThingTemplate *currentTpl = templates.find(current);
    if (!currentTpl) {
      break;
    }
    if (!currentTpl->bbox.IsZero()) {
      return currentTpl->bbox;
    }
    if (currentTpl->parent.empty() ||
        sed::text::iequalsAscii(currentTpl->parent, "none")) {
      break;
    }
    current = currentTpl->parent;
  }
  return std::nullopt;
}

ThingBox thingPlacementSpritePreviewBox(const assets::SprFile &spr) {
  const double halfW = std::max(0.1, static_cast<double>(spr.width) * 0.5);
  const double halfH = std::max(0.1, static_cast<double>(spr.height) * 0.5);
  const double offsetX = static_cast<double>(spr.offsetXYZ[0]);
  const double offsetY = static_cast<double>(spr.offsetXYZ[1]);
  const double offsetZ = static_cast<double>(spr.offsetXYZ[2]);
  const double halfDepth = std::max(0.1, std::max(halfW, halfH) * 0.08);

  ThingBox box{};
  if (spr.type == 2) {
    const double radialHalf = std::max(halfW, halfDepth);
    box.Set(offsetX - radialHalf, offsetY - radialHalf, offsetZ - halfH,
            offsetX + radialHalf, offsetY + radialHalf, offsetZ + halfH);
  } else {
    box.Set(offsetX - halfW, offsetY - halfDepth, offsetZ - halfH,
            offsetX + halfW, offsetY + halfDepth, offsetZ + halfH);
  }
  return box;
}

std::optional<ThingBox> thingPlacementInferTemplatePreviewBox(
    const ThingTemplate &tpl, const assets::Templates &templates,
    const assets::ResourceLocator &resources) {
  if (const std::optional<std::string> spriteName =
          thingPlacementResolvedPreviewField(tpl, templates, "sprite")) {
    if (const std::shared_ptr<const assets::SprFile> sprite =
            ui::detail::loadSharedPreviewSprite(&resources, *spriteName)) {
      return thingPlacementSpritePreviewBox(*sprite);
    }
  }

  std::optional<std::string> modelName =
      thingPlacementResolvedPreviewField(tpl, templates, "model3d");
  if (!modelName) {
    modelName = thingPlacementResolvedPreviewField(tpl, templates, "model3do");
  }
  if (modelName) {
    if (const std::optional<ThingBox> box =
            ui::detail::loadSharedPreviewModelBox(&resources, *modelName)) {
      return *box;
    }
  }

  return thingPlacementResolvedPreviewBBox(tpl, templates, tpl.name);
}

class ThingPlacementSelectedPreviewWidget final : public QWidget {
public:
  explicit ThingPlacementSelectedPreviewWidget(
      const assets::ResourceLocator &resources, QWidget *parent = nullptr)
      : QWidget(parent), resources_(resources) {
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    stack_ = new QStackedWidget(this);
    layout->addWidget(stack_);

    emptyLabel_ = new QLabel("No template selected", stack_);
    emptyLabel_->setAlignment(Qt::AlignCenter);
    emptyLabel_->setWordWrap(true);
    emptyLabel_->setStyleSheet("QLabel {"
                               " color:#9aa7bb;"
                               " background:#0b0d12;"
                               " padding:14px;"
                               " }");
    stack_->addWidget(emptyLabel_);

    spriteLabel_ = new QLabel(stack_);
    spriteLabel_->setAlignment(Qt::AlignCenter);
    spriteLabel_->setStyleSheet("QLabel {"
                                " background:#0b0d12;"
                                " }");
    spriteLabel_->installEventFilter(this);
    stack_->addWidget(spriteLabel_);

    spriteAnimButton_ = new QToolButton(spriteLabel_);
    spriteAnimButton_->setAutoRaise(true);
    spriteAnimButton_->setToolTip(
        "<b>Sprite preview</b><br>Play or stop the sprite animation preview.");
    spriteAnimButton_->setFixedSize(24, 24);
    spriteAnimButton_->setText(QStringLiteral("\u25B6"));
    spriteAnimButton_->setStyleSheet(
        "QToolButton {"
        " background:rgba(28,30,36,220);"
        " border:1px solid rgba(160,170,190,0.45);"
        " border-radius:4px;"
        " color:#dce0e8;"
        " font-weight:bold;"
        " }"
        "QToolButton:hover { background:rgba(50,54,64,235); }");
    spriteAnimButton_->hide();
    connect(spriteAnimButton_, &QToolButton::clicked, this, [this]() {
      if (spriteAnimTimer_ && spriteAnimTimer_->isActive()) {
        stopSpriteAnimation();
      } else {
        startSpriteAnimation();
      }
    });

    spriteAnimTimer_ = new QTimer(this);
    spriteAnimTimer_->setInterval(100);
    connect(spriteAnimTimer_, &QTimer::timeout, this, [this]() {
      if (spriteAnimFrames_.empty()) {
        spriteAnimTimer_->stop();
        return;
      }
      spriteAnimIndex_ =
          (spriteAnimIndex_ + 1) % static_cast<int>(spriteAnimFrames_.size());
      currentSpriteImage_ =
          spriteAnimFrames_[static_cast<std::size_t>(spriteAnimIndex_)];
      updateSpritePixmap();
    });

    view_ = new GlView(stack_);
    view_->setObjectName("thingPlacementSelectedPreviewView");
    view_->setResourceLocator(&resources_);
    // MainWindow's placement watcher owns mounted archive remounts. Keep
    // this delegated preview focused on loose dependency changes so one
    // archive replacement cannot start a second retry loop here.
    view_->setMountedArchiveWatchingEnabled(false);
    view_->setRenderPipeline(GlView::RenderPipeline::GamePreview);
    view_->setShowGrid(false);
    ui::detail::applyThingAssetPreviewRenderMode(*view_, 1);
    view_->setShowThings(true);
    view_->setShowEditorCubes(false);
    view_->setShowPreviewCubes(false);
    view_->setShowThingGizmos(false);
    view_->setThingViewMode(GlView::ThingViewMode::Shaded);
    view_->setLightingMode(GlView::LightingMode::Gouraud);
    view_->setCameraProjectionType(
        app::ViewportCamera::ProjectionType::Orthographic);
    view_->setViewportPickingEnabled(false);
    view_->setHoverPickEnabled(false);
    view_->setPreviewInteractionEnabled(true);
    view_->installEventFilter(this);
    stack_->addWidget(view_);

    overlayLabel_ = new QLabel(this);
    overlayLabel_->setObjectName("thingPlacementSelectedPreviewOverlay");
    overlayLabel_->setWordWrap(true);
    overlayLabel_->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    overlayLabel_->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    overlayLabel_->hide();

    spinTimer_ = new QTimer(this);
    spinTimer_->setObjectName(
        QStringLiteral("thingPlacementSelectedPreviewSpinTimer"));
    spinTimer_->setInterval(33);
    connect(spinTimer_, &QTimer::timeout, this, [this]() {
      if (!view_ || !stack_ || stack_->currentWidget() != view_) {
        if (spinTimer_) {
          spinTimer_->stop();
        }
        return;
      }
      if (!isVisible() || !view_->isVisible() ||
          ui::detail::sharedThumbnailRenderBusy()) {
        // Do not keep waking the GUI thread while the placement
        // panel/window is hidden (or thumbnail GL work owns the
        // shared context).  setSpinActive() re-arms the timer
        // when a visible model preview is selected again.
        if (!isVisible() || !view_->isVisible()) {
          spinTimer_->stop();
        }
        return;
      }
      view_->orbitRotate(-2.4f, 0.0f);
    });

    refitTimer_ = new QTimer(this);
    refitTimer_->setSingleShot(true);
    refitTimer_->setInterval(80);
    connect(refitTimer_, &QTimer::timeout, this, [this]() {
      if (currentPreviewBox_) {
        fitToBox(*currentPreviewBox_, false);
      }
    });

    setMinimumHeight(168);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  }

  void clearPreview(const QString &message) {
    setSpinActive(false);
    currentTemplateName_.clear();
    currentCmpName_.clear();
    currentProjectKind_ = ProjectType::JKDF2;
    templates_.clear();
    level_.clear();
    currentPreviewBox_.reset();
    currentSpriteImage_.reset();
    resetSpriteAnimation();
    if (spriteLabel_) {
      spriteLabel_->setPixmap(QPixmap{});
    }
    if (view_) {
      view_->setTemplateDatabase(nullptr);
      view_->setLevel(nullptr, GlView::SetLevelMode::keepView);
    }
    if (emptyLabel_) {
      emptyLabel_->setText(message);
    }
    if (stack_) {
      stack_->setCurrentWidget(emptyLabel_);
    }
    setOverlayText({});
  }

  /**
   * Runs the selected model preview's idle orbit only while it is actually
   * visible.  The placement panel is constructed with the main window, so a
   * timer started unconditionally here would otherwise wake the GUI thread
   * every 33 ms for the entire editor session, even with no selected Thing.
   */
  void setSpinActive(const bool active) {
    if (!spinTimer_) {
      return;
    }

    const bool canSpin = active && view_ && stack_ &&
                         stack_->currentWidget() == view_ && isVisible() &&
                         view_->isVisible();
    if (!canSpin) {
      spinTimer_->stop();
      return;
    }
    if (!spinTimer_->isActive()) {
      spinTimer_->start();
    }
  }

  void setPreviewTemplate(const assets::Templates &database,
                          const ThingTemplate &tpl,
                          const ProjectType projectKind,
                          const std::string_view cmpName) {
    const std::string cmpKey(cmpName);
    const bool samePreview =
        currentTemplateName_ == tpl.name && currentCmpName_ == cmpKey &&
        currentProjectKind_ == projectKind &&
        (currentPreviewBox_.has_value() ||
         (currentSpriteImage_.has_value() && !currentSpriteImage_->isNull()));
    if (samePreview) {
      // The timer may have been stopped while the main window was
      // minimized/hidden. Re-arm it only when the model page is visible.
      setSpinActive(true);
      return;
    }

    currentTemplateName_ = tpl.name;
    currentCmpName_ = cmpKey;
    currentProjectKind_ = projectKind;
    currentSpriteImage_.reset();
    currentPreviewBox_.reset();
    resetSpriteAnimation();

    if (const std::optional<std::string> spriteName =
            thingPlacementResolvedPreviewField(tpl, database, "sprite")) {
      currentSpriteImage_ = ui::detail::loadSharedPreviewSpriteImage(
          &resources_, cmpName, *spriteName);
      if (currentSpriteImage_ && !currentSpriteImage_->isNull()) {
        if (view_) {
          view_->setTemplateDatabase(nullptr);
          view_->setLevel(nullptr, GlView::SetLevelMode::keepView);
        }
        updateSpritePixmap();
        if (stack_) {
          stack_->setCurrentWidget(spriteLabel_);
        }
        setSpinActive(false);
        setupSpriteAnimation(*spriteName, cmpName);
        return;
      }
    }

    std::optional<std::string> previewModelName =
        thingPlacementResolvedPreviewField(tpl, database, "model3d");
    if (!previewModelName) {
      previewModelName =
          thingPlacementResolvedPreviewField(tpl, database, "model3do");
    }
    const std::optional<ThingBox> box =
        thingPlacementInferTemplatePreviewBox(tpl, database, resources_);
    if (!box) {
      clearPreview(QStringLiteral("No previewable model or sprite."));
      return;
    }

    currentPreviewBox_ = *box;
    templates_.clear();
    for (std::size_t i = 0; i < database.size(); ++i) {
      if (const ThingTemplate *item = database.get(i)) {
        try {
          (void)templates_.addOrUpdate(*item, true);
        } catch (const std::exception &) {
        }
      }
    }

    level_.clear();
    level_.kind = projectKind;
    level_.masterCMP = std::string(cmpName);
    level_.ppunit = 320.0;
    level_.templates.clear();
    for (std::size_t i = 0; i < templates_.size(); ++i) {
      if (const ThingTemplate *item = templates_.get(i)) {
        level_.templates.push_back(*item);
      }
    }
    level_.sectors.push_back(Sector{});
    level_.sectors.back().layer = 0;
    level_.sectors.back().ambient = ColorWhite;

    Thing previewThing{};
    previewThing.name = tpl.name;
    previewThing.templateName = tpl.name;
    previewThing.sector = 0;
    previewThing.bbox = *box;
    level_.things.push_back(previewThing);

    view_->setResourceLocator(&resources_);
    if (previewModelName) {
      ui::detail::primeSharedModelPreviewAssets(
          *view_, &resources_, cmpName, *previewModelName);
    }
    view_->setTemplateDatabase(&templates_);
    view_->setRenderPipeline(GlView::RenderPipeline::Editor);
    view_->resetPreviewViewportState();
    view_->setRenderPipeline(GlView::RenderPipeline::GamePreview);
    ui::detail::applyThingAssetPreviewRenderMode(*view_, 1);
    view_->setShowThings(true);
    view_->setShowEditorCubes(false);
    view_->setShowPreviewCubes(false);
    view_->setThingViewMode(GlView::ThingViewMode::Shaded);
    view_->setLightingMode(GlView::LightingMode::Gouraud);
    view_->setLevel(&level_, GlView::SetLevelMode::resetView);
    if (stack_) {
      stack_->setCurrentWidget(view_);
    }
    setSpinActive(true);
    fitToBox(*box);
  }

  void setOverlayText(const QString &text) {
    if (!overlayLabel_) {
      return;
    }
    overlayLabel_->setText(text);
    overlayLabel_->setVisible(!text.trimmed().isEmpty());
    positionOverlayLabel();
  }

  void invalidateResourceCaches() {
    // Break the setPreviewTemplate same-key fast path and reset the sprite
    // palette/image state that lives outside the embedded GlView.
    currentTemplateName_.clear();
    currentCmpName_.clear();
    currentPreviewBox_.reset();
    currentSpriteImage_.reset();
    palette_.reset();
    paletteKey_.clear();
    paletteResolved_ = false;
    resetSpriteAnimation();
    if (view_) {
      view_->reloadCachedResources();
    }
  }

private:
  void resizeEvent(QResizeEvent *event) override {
    QWidget::resizeEvent(event);
    positionOverlayLabel();
  }

  bool eventFilter(QObject *watched, QEvent *event) override {
    if (spriteLabel_ && watched == spriteLabel_ && event &&
        event->type() == QEvent::Resize) {
      updateSpritePixmap();
      repositionSpriteButton();
    }
    if (view_ && watched == view_ && event && event->type() == QEvent::Resize) {
      if (refitTimer_ && currentPreviewBox_ && stack_ &&
          stack_->currentWidget() == view_) {
        refitTimer_->start();
      }
    }
    return QWidget::eventFilter(watched, event);
  }

  void positionOverlayLabel() {
    if (!overlayLabel_) {
      return;
    }
    constexpr int margin = 10;
    overlayLabel_->setGeometry(margin, margin,
                               std::max(1, width() - margin * 2),
                               std::min(std::max(44, height() / 3), 76));
    overlayLabel_->raise();
    if (spriteAnimButton_) {
      spriteAnimButton_->raise();
    }
  }

  std::optional<assets::CmpPalette>
  previewPalette(const std::string_view cmpName) {
    const std::string key = sed::text::upperAscii(cmpName);
    if (paletteResolved_ && key == paletteKey_) {
      return palette_;
    }
    paletteResolved_ = true;
    paletteKey_ = key;
    palette_ = loadThingPlacementPreviewPalette(resources_, cmpName);
    return palette_;
  }

  void updateSpritePixmap() {
    if (!spriteLabel_) {
      return;
    }
    if (!currentSpriteImage_ || currentSpriteImage_->isNull()) {
      spriteLabel_->setPixmap(QPixmap{});
      return;
    }
    spriteLabel_->setPixmap(makeThingPlacementImagePreviewPixmap(
        *currentSpriteImage_, spriteLabel_->contentsRect().size()));
  }

  void repositionSpriteButton() {
    if (!spriteAnimButton_ || !spriteLabel_) {
      return;
    }
    const int margin = 8;
    spriteAnimButton_->move(
        std::max(margin,
                 spriteLabel_->width() - spriteAnimButton_->width() - margin),
        std::max(margin, spriteLabel_->height() - spriteAnimButton_->height() -
                             margin));
  }

  void loadSpriteAnimationFrames(const std::string_view spriteName,
                                 const std::string_view cmpName) {
    spriteAnimFrames_.clear();
    spriteAnimIndex_ = 0;

    const std::shared_ptr<const assets::SprFile> sprite =
        ui::detail::loadSharedPreviewSprite(&resources_, spriteName);
    if (!sprite) {
      return;
    }

    try {
      const auto matBytes =
          resources_.tryReadBinaryWithExt(sprite->material, ".mat");
      if (!matBytes) {
        return;
      }

      std::optional<assets::CmpPalette> palette = previewPalette(cmpName);
      assets::MatFile mat = assets::MatFile::parse(
          std::vector<std::byte>(matBytes->begin(), matBytes->end()),
          palette ? &*palette : nullptr);

      const std::int32_t count = mat.celCount();
      if (count <= 1) {
        return;
      }

      spriteAnimFrames_.reserve(static_cast<std::size_t>(count));
      for (std::int32_t i = 0; i < count; ++i) {
        QImage img = ui::detail::qImageFromRgba8(mat.decodeCelRgba8(i));
        if (img.isNull()) {
          spriteAnimFrames_.clear();
          return;
        }
        spriteAnimFrames_.push_back(std::move(img));
      }
    } catch (const std::exception &) {
      spriteAnimFrames_.clear();
    }
  }

  void setupSpriteAnimation(const std::string_view spriteName,
                            const std::string_view cmpName) {
    resetSpriteAnimation();
    loadSpriteAnimationFrames(spriteName, cmpName);
    if (spriteAnimFrames_.size() > 1 && spriteAnimButton_) {
      spriteAnimButton_->setText(QStringLiteral("\u25A0"));
      spriteAnimButton_->show();
      repositionSpriteButton();
      spriteAnimTimer_->start();
    }
  }

  void startSpriteAnimation() {
    if (spriteAnimFrames_.size() <= 1 || !spriteAnimTimer_) {
      return;
    }
    spriteAnimButton_->setText(QStringLiteral("\u25A0"));
    spriteAnimTimer_->start();
  }

  void stopSpriteAnimation() {
    if (spriteAnimTimer_) {
      spriteAnimTimer_->stop();
    }
    spriteAnimIndex_ = 0;
    if (!spriteAnimFrames_.empty()) {
      currentSpriteImage_ = spriteAnimFrames_.front();
      updateSpritePixmap();
    }
    if (spriteAnimButton_) {
      spriteAnimButton_->setText(QStringLiteral("\u25B6"));
    }
  }

  void resetSpriteAnimation() {
    if (spriteAnimTimer_) {
      spriteAnimTimer_->stop();
    }
    spriteAnimFrames_.clear();
    spriteAnimIndex_ = 0;
    if (spriteAnimButton_) {
      spriteAnimButton_->hide();
      spriteAnimButton_->setText(QStringLiteral("\u25B6"));
    }
  }

  void fitToBox(const ThingBox &box, const bool resetOrbit = true) {
    if (!view_) {
      return;
    }

    QVector3D minv(static_cast<float>(std::min(box.x1, box.x2)),
                   static_cast<float>(std::min(box.y1, box.y2)),
                   static_cast<float>(std::min(box.z1, box.z2)));
    QVector3D maxv(static_cast<float>(std::max(box.x1, box.x2)),
                   static_cast<float>(std::max(box.y1, box.y2)),
                   static_cast<float>(std::max(box.z1, box.z2)));
    if ((maxv - minv).lengthSquared() <= 1.0e-6f) {
      maxv += QVector3D(0.5f, 0.5f, 0.5f);
      minv -= QVector3D(0.5f, 0.5f, 0.5f);
    }

    constexpr float kPadding = 1.24f;
    constexpr float kYaw = 160.0f;
    constexpr float kPitch = 14.0f;
    constexpr float kRoll = 0.0f;
    const QVector3D center = (minv + maxv) * 0.5f;
    const QVector3D halfExtents = (maxv - minv) * 0.5f * kPadding;
    minv = center - halfExtents;
    maxv = center + halfExtents;

    if (resetOrbit) {
      view_->setOrbitViewImmediate(kYaw, kPitch, kRoll);
    }
    view_->frameWorldBounds(minv, maxv, false);
    view_->tightenOrthoClipToWorldBounds(minv, maxv);
    QTimer::singleShot(0, view_, [view = view_, minv, maxv, resetOrbit]() {
      if (!view) {
        return;
      }
      if (resetOrbit) {
        view->setOrbitViewImmediate(kYaw, kPitch, kRoll);
      }
      view->frameWorldBounds(minv, maxv, false);
      view->tightenOrthoClipToWorldBounds(minv, maxv);
    });
  }

  const assets::ResourceLocator &resources_;
  QStackedWidget *stack_{};
  QLabel *emptyLabel_{};
  QLabel *spriteLabel_{};
  QLabel *overlayLabel_{};
  QToolButton *spriteAnimButton_{};
  QTimer *spriteAnimTimer_{};
  std::vector<QImage> spriteAnimFrames_{};
  int spriteAnimIndex_{};
  GlView *view_{};
  QTimer *spinTimer_{};
  QTimer *refitTimer_{};
  assets::Templates templates_{};
  Level level_{};
  std::optional<assets::CmpPalette> palette_{};
  std::string paletteKey_{};
  bool paletteResolved_{};
  std::string currentTemplateName_{};
  std::string currentCmpName_{};
  ProjectType currentProjectKind_{ProjectType::JKDF2};
  std::optional<QImage> currentSpriteImage_{};
  std::optional<ThingBox> currentPreviewBox_{};
};
} // namespace

#ifdef SED_APP_MAIN_WINDOW_TEST_ACCESS
QImage MainWindow::thingPlacementTemplateIconForTest(
    const ThingTemplate& tpl,
    const QSize& iconSize)
{
    const QIcon icon = makeThingPlacementTemplateIcon(
        tpl,
        templates_,
        resources_,
        document_.level().kind,
        document_.level().getMasterCMP(),
        iconSize,
        thingPlacementTemplatePreviewIconCache_);
    return icon.pixmap(iconSize).toImage();
}
#endif

void MainWindow::setupViewportOverlay() {
  if (!glView_) {
    return;
  }

  if (!viewportVerticesAction_ || !viewportWireframeAction_ ||
      !viewportSolidAction_ || !viewportTexturedAction_ ||
      !viewportWireframeOverlayAction_ || !viewportGamePreviewAction_ ||
      !viewportMuteSoundAction_ || !viewportFullbrightAction_ ||
      !viewportLitAction_ || !viewportDiffuseAction_ ||
      !viewportGouraudAction_ || !thingDotsAction_ || !thingBoxesAction_ ||
      !thingWireframesAction_ || !thingShadedAction_ || !thingRenderAction_ ||
      !thingHiddenAction_ || !thingCollisionShapesAction_ ||
      !thingVisualBoundsAction_ || !thingModelLodAction_ ||
      !thingGizmosAction_ || !thingCubesAction_) {
    return;
  }

  viewportOverlay_ = new QWidget(glView_);
  viewportOverlay_->setAttribute(Qt::WA_StyledBackground, true);
  viewportOverlay_->setObjectName("viewportOverlay");

  auto *layout = new QHBoxLayout(viewportOverlay_);
  layout->setContentsMargins(6, 6, 6, 6);
  layout->setSpacing(4);

  auto makeButton = [&](QAction *action) -> QToolButton * {
    auto *button = new QToolButton(viewportOverlay_);
    button->setDefaultAction(action);
    button->setToolButtonStyle(Qt::ToolButtonIconOnly);
    button->setAutoRaise(true);
    button->setIconSize(QSize(18, 18));
    return button;
  };

  const auto anchorPopupBelowViewportOverlay = [this](QMenu *menu,
                                                      QToolButton *launcher) {
    if (!menu || !launcher) {
      return;
    }
    connect(menu, &QMenu::aboutToShow, this, [this, menu, launcher]() {
      QTimer::singleShot(0, menu, [this, menu, launcher]() {
        if (!glView_ || !viewportOverlay_ || !menu || !launcher ||
            !menu->isVisible()) {
          return;
        }

        constexpr int margin = 10;
        constexpr int gap = 6;
        menu->adjustSize();
        const QPoint launcherTopLeft = launcher->mapTo(glView_, QPoint(0, 0));
        int x = launcherTopLeft.x() + launcher->width() - menu->width();
        x = std::clamp(
            x, margin,
            std::max(margin, glView_->width() - menu->width() - margin));
        const int y = viewportOverlay_->geometry().bottom() + 1 + gap;
        menu->move(glView_->mapToGlobal(QPoint(x, y)));
      });
    });
  };

  viewportCameraButton_ = new QToolButton(viewportOverlay_);
  viewportCameraButton_->setObjectName("viewportCameraSettingsButton");
  viewportCameraButton_->setToolButtonStyle(Qt::ToolButtonIconOnly);
  viewportCameraButton_->setAutoRaise(true);
  viewportCameraButton_->setIcon(
      ui::makeViewportIcon(ui::ViewportIcon::camera));
  viewportCameraButton_->setIconSize(QSize(18, 18));
  viewportCameraButton_->setPopupMode(QToolButton::InstantPopup);
  viewportCameraButton_->setToolTip(
      "<b>Camera options</b><br>Open viewport navigation and camera controls.");

  viewportCameraMenu_ = new ui::ViewportCameraMenu(viewportCameraButton_);
  viewportCameraMenu_->setObjectName("viewportCameraSettingsMenu");
  viewportCameraButton_->setMenu(viewportCameraMenu_);

  {
    auto *panel = new QWidget(viewportCameraMenu_);
    panel->setObjectName("viewportCameraPanel");

    auto *panelLayout = new QVBoxLayout(panel);
    panelLayout->setContentsMargins(10, 10, 10, 10);
    panelLayout->setSpacing(8);

    auto *form = new QFormLayout();
    form->setContentsMargins(0, 0, 0, 0);
    form->setHorizontalSpacing(10);
    form->setVerticalSpacing(6);
    panelLayout->addLayout(form);

    viewportCameraProjectionCombo_ = new QComboBox(panel);
    viewportCameraProjectionCombo_->setObjectName(
        "viewportCameraProjectionCombo");
    viewportCameraProjectionCombo_->addItem(
        "Orthographic",
        static_cast<int>(app::ViewportCamera::ProjectionType::Orthographic));
    viewportCameraProjectionCombo_->addItem(
        "Perspective",
        static_cast<int>(app::ViewportCamera::ProjectionType::Perspective));
    form->addRow("Projection", viewportCameraProjectionCombo_);

    auto *fovRow = new QWidget(panel);
    fovRow->setObjectName("viewportCameraFovRow");
    auto *fovLayout = new QHBoxLayout(fovRow);
    fovLayout->setContentsMargins(0, 0, 0, 0);
    fovLayout->setSpacing(6);

    viewportCameraFovSlider_ = new QSlider(Qt::Horizontal, fovRow);
    viewportCameraFovSlider_->setRange(1, 179);
    viewportCameraFovSlider_->setSingleStep(1);
    viewportCameraFovSlider_->setPageStep(10);

    viewportCameraFovEdit_ = new QLineEdit(fovRow);
    viewportCameraFovEdit_->setFixedWidth(56);
    viewportCameraFovEdit_->setAlignment(Qt::AlignRight);
    auto *fovValidator =
        new QDoubleValidator(1.0, 179.0, 2, viewportCameraFovEdit_);
    fovValidator->setNotation(QDoubleValidator::StandardNotation);
    fovValidator->setLocale(ui::uiNumericLocale());
    viewportCameraFovEdit_->setLocale(ui::uiNumericLocale());
    viewportCameraFovEdit_->setValidator(fovValidator);

    const auto setFovText = [this, fovValidator](const float fov)
    {
      const QSignalBlocker blocker(viewportCameraFovEdit_);
      fovValidator->setLocale(ui::uiNumericLocale());
      viewportCameraFovEdit_->setLocale(ui::uiNumericLocale());
      viewportCameraFovEdit_->setText(ui::formatUiNumber(fov, 'f', 1));
      viewportCameraFovEdit_->setModified(false);
    };
    ui::onUiNumericFormatChanged(viewportCameraFovEdit_, [this, setFovText]()
    {
      if (glView_ && !viewportCameraFovEdit_->isModified())
      {
        setFovText(glView_->cameraFovXDeg());
      }
    });

    fovLayout->addWidget(viewportCameraFovSlider_, 1);
    fovLayout->addWidget(viewportCameraFovEdit_, 0);
    form->addRow("FOV", fovRow);
    QWidget *fovLabel = form->labelForField(fovRow);
    if (fovLabel) {
      fovLabel->setObjectName("viewportCameraFovLabel");
    }

    viewportCameraNearSpin_ = new QDoubleSpinBox(panel);
    viewportCameraNearSpin_->setDecimals(6);
    viewportCameraNearSpin_->setRange(-100000.0, 100000.0);
    viewportCameraNearSpin_->setSingleStep(0.01);
    viewportCameraNearSpin_->setKeyboardTracking(false);
    form->addRow("Near", viewportCameraNearSpin_);

    viewportCameraFarSpin_ = new QDoubleSpinBox(panel);
    viewportCameraFarSpin_->setDecimals(1);
    viewportCameraFarSpin_->setRange(0.1, 1000000.0);
    viewportCameraFarSpin_->setSingleStep(10.0);
    viewportCameraFarSpin_->setKeyboardTracking(false);
    form->addRow("Far", viewportCameraFarSpin_);

    viewportCameraClipCheck_ = new QCheckBox("Enable clipping", panel);
    panelLayout->addWidget(viewportCameraClipCheck_);

    auto *action = new QWidgetAction(viewportCameraMenu_);
    action->setDefaultWidget(panel);
    viewportCameraMenu_->addAction(action);

    viewportCameraMenu_->setStyleSheet(
        "QMenu {"
        "  background-color: #15171c;"
        "  border: 1px solid #343b46;"
        "  border-radius: 10px;"
        "}"
        "QWidget#viewportCameraPanel {"
        "  background: transparent;"
        "  color: rgba(235, 236, 240, 255);"
        "}"
        "QLineEdit, QDoubleSpinBox {"
        "  background: rgba(0, 0, 0, 60);"
        "  border: 1px solid rgba(255, 255, 255, 40);"
        "  border-radius: 6px;"
        "  padding: 2px 6px;"
        "  color: rgba(235, 236, 240, 255);"
        "}"
        "QSlider::groove:horizontal {"
        "  height: 4px;"
        "  background: rgba(255, 255, 255, 35);"
        "  border-radius: 2px;"
        "}"
        "QSlider::handle:horizontal {"
        "  width: 12px;"
        "  margin: -5px 0;"
        "  border-radius: 6px;"
        "  background: rgba(235, 236, 240, 200);"
        "}"
        "QCheckBox {"
        "  padding-top: 2px;"
        "}");

    auto pushNearFar = [this]() {
      if (!glView_ || !viewportCameraNearSpin_ || !viewportCameraFarSpin_) {
        return;
      }

      glView_->setCameraNearFar(
          static_cast<float>(viewportCameraNearSpin_->value()),
          static_cast<float>(viewportCameraFarSpin_->value()));
    };

    auto syncFovVisibility = [this, fovRow, fovLabel]() {
      const bool isPerspective =
          glView_ && glView_->cameraProjectionType() ==
                         app::ViewportCamera::ProjectionType::Perspective;
      fovRow->setVisible(isPerspective);
      if (fovLabel) {
        fovLabel->setVisible(isPerspective);
      }
      if (viewportCameraFovSlider_) {
        viewportCameraFovSlider_->setEnabled(isPerspective);
      }
      if (viewportCameraFovEdit_) {
        viewportCameraFovEdit_->setEnabled(isPerspective);
      }
    };

    syncFovVisibility();
    connect(glView_, &GlView::viewTransformChanged, this, syncFovVisibility);

    connect(viewportCameraProjectionCombo_,
            QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this, syncFovVisibility](int) {
              if (!glView_ || !viewportCameraProjectionCombo_ ||
                  !viewportCameraFovSlider_ || !viewportCameraFovEdit_ ||
                  !viewportCameraNearSpin_ || !viewportCameraFarSpin_) {
                return;
              }

              const auto type =
                  static_cast<app::ViewportCamera::ProjectionType>(
                      viewportCameraProjectionCombo_->currentData().toInt());
              glView_->setCameraProjectionType(type);

              const bool isPerspective =
                  type == app::ViewportCamera::ProjectionType::Perspective;
              syncFovVisibility();

              const double nearMin =
                  isPerspective ? app::ViewportCamera::kPerspectiveNearPlaneMin
                                : -100000.0;
              const double nearMax = isPerspective ? 100000.0 : 0.0;

              const QSignalBlocker b0(viewportCameraNearSpin_);
              const QSignalBlocker b1(viewportCameraFarSpin_);
              viewportCameraNearSpin_->setRange(nearMin, nearMax);
              viewportCameraNearSpin_->setValue(
                  static_cast<double>(glView_->cameraNearPlane()));
              viewportCameraFarSpin_->setValue(
                  static_cast<double>(glView_->cameraFarPlane()));
            });

    connect(viewportCameraFovSlider_, &QSlider::valueChanged, this,
            [this, setFovText](const int value) {
              if (!glView_ || !viewportCameraFovEdit_) {
                return;
              }

              const float fov = static_cast<float>(value);
              setFovText(fov);
              glView_->setCameraFovXDeg(fov);
            });

    connect(viewportCameraFovEdit_, &QLineEdit::editingFinished, this,
            [this, setFovText]() {
              if (!glView_ || !viewportCameraFovSlider_ ||
                  !viewportCameraFovEdit_) {
                return;
              }

              bool ok = false;
              float fov = static_cast<float>(ui::parseUiNumber(
                  viewportCameraFovEdit_->text(), viewportCameraFovEdit_->locale(), &ok));
              if (!ok) {
                return;
              }

              fov = std::clamp(fov, 1.0f, 179.0f);
              const QSignalBlocker b0(viewportCameraFovSlider_);
              viewportCameraFovSlider_->setValue(
                  static_cast<int>(std::lround(static_cast<double>(fov))));
              setFovText(fov);
              glView_->setCameraFovXDeg(fov);
            });

    connect(viewportCameraNearSpin_,
            QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
            [pushNearFar](double) { pushNearFar(); });

    connect(viewportCameraFarSpin_,
            QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
            [pushNearFar](double) { pushNearFar(); });

    connect(viewportCameraClipCheck_, &QCheckBox::toggled, this,
            [this](const bool checked) {
              if (!glView_ || !viewportCameraNearSpin_ ||
                  !viewportCameraFarSpin_) {
                return;
              }

              glView_->setCameraClipEnabled(checked);
            });

    connect(viewportCameraMenu_, &QMenu::aboutToShow, this,
            [this, syncFovVisibility, setFovText]() {
              if (!glView_ || !viewportCameraProjectionCombo_ ||
                  !viewportCameraFovSlider_ || !viewportCameraFovEdit_ ||
                  !viewportCameraNearSpin_ || !viewportCameraFarSpin_ ||
                  !viewportCameraClipCheck_) {
                return;
              }

              const auto projectionType = glView_->cameraProjectionType();
              const bool isPerspective =
                  projectionType ==
                  app::ViewportCamera::ProjectionType::Perspective;
              const float fov = glView_->cameraFovXDeg();
              const float nearPlane = glView_->cameraNearPlane();
              const float farPlane = glView_->cameraFarPlane();
              const bool clipEnabled = glView_->cameraClipEnabled();

              const double nearMin =
                  isPerspective ? app::ViewportCamera::kPerspectiveNearPlaneMin
                                : -100000.0;
              const double nearMax = isPerspective ? 100000.0 : 0.0;

              const QSignalBlocker b0(viewportCameraFovSlider_);
              const QSignalBlocker b1(viewportCameraFovEdit_);
              const QSignalBlocker bProj(viewportCameraProjectionCombo_);
              const QSignalBlocker b2(viewportCameraNearSpin_);
              const QSignalBlocker b3(viewportCameraFarSpin_);
              const QSignalBlocker b4(viewportCameraClipCheck_);

              viewportCameraNearSpin_->setRange(nearMin, nearMax);

              const int projIndex = viewportCameraProjectionCombo_->findData(
                  static_cast<int>(projectionType));
              if (projIndex >= 0) {
                viewportCameraProjectionCombo_->setCurrentIndex(projIndex);
              }

              viewportCameraFovSlider_->setValue(
                  static_cast<int>(std::lround(static_cast<double>(fov))));
              if (!viewportCameraFovEdit_->isModified())
              {
                setFovText(fov);
              }
              viewportCameraNearSpin_->setValue(static_cast<double>(nearPlane));
              viewportCameraFarSpin_->setValue(static_cast<double>(farPlane));
              viewportCameraClipCheck_->setChecked(clipEnabled);

              syncFovVisibility();
            });
  }
  anchorPopupBelowViewportOverlay(viewportCameraMenu_, viewportCameraButton_);

  auto addSeparator = [&](const QString& objectName)
  {
    auto *sep = new QFrame(viewportOverlay_);
    sep->setObjectName(objectName);
    sep->setFrameShape(QFrame::VLine);
    sep->setFrameShadow(QFrame::Plain);
    sep->setStyleSheet("QFrame { color: rgba(255, 255, 255, 60); }");
    layout->addWidget(sep);
  };

  // Level-wide settings (stored in the file header, but presented by purpose).
  auto *viewportLevelSettingsButton = new QToolButton(viewportOverlay_);
  viewportLevelSettingsButton->setObjectName("viewportLevelSettingsButton");
  viewportLevelSettingsButton->setToolButtonStyle(Qt::ToolButtonIconOnly);
  viewportLevelSettingsButton->setAutoRaise(true);
  viewportLevelSettingsButton->setIcon(
      ui::makeViewportIcon(ui::ViewportIcon::levelSettings));
  viewportLevelSettingsButton->setIconSize(QSize(18, 18));
  viewportLevelSettingsButton->setPopupMode(QToolButton::InstantPopup);
  viewportLevelSettingsButton->setToolTip(
      "<b>Level settings</b><br>Edit the settings saved with this level.");
  viewportLevelSettingsButton->setAccessibleName("Level settings");
  viewportLevelSettingsButton->setAccessibleDescription(
      "Edit the settings saved with this level.");

  auto *viewportLevelSettingsMenu =
      new ui::ViewportCameraMenu(viewportLevelSettingsButton);
  viewportLevelSettingsMenu->setObjectName("viewportLevelSettingsMenu");
  viewportLevelSettingsButton->setMenu(viewportLevelSettingsMenu);

  {
    auto *viewportLevelSettingsScrollArea =
        new QScrollArea(viewportLevelSettingsMenu);
    viewportLevelSettingsScrollArea->setObjectName(
        "viewportLevelSettingsScrollArea");
    viewportLevelSettingsScrollArea->setWidgetResizable(false);
    viewportLevelSettingsScrollArea->setHorizontalScrollBarPolicy(
        Qt::ScrollBarAlwaysOff);
    viewportLevelSettingsScrollArea->setVerticalScrollBarPolicy(
        Qt::ScrollBarAlwaysOff);
    viewportLevelSettingsScrollArea->setFrameShape(QFrame::NoFrame);
    viewportLevelSettingsScrollArea->viewport()->setAutoFillBackground(false);
    viewportLevelSettingsScrollArea->setStyleSheet(
        "QScrollArea#viewportLevelSettingsScrollArea {"
        "  background: transparent;"
        "  border: none;"
        "}"
        "QScrollArea#viewportLevelSettingsScrollArea > QWidget > QWidget {"
        "  background: transparent;"
        "}"
        "QScrollArea#viewportLevelSettingsScrollArea QScrollBar:vertical {"
        "  width: 8px;"
        "  margin: 4px 1px;"
        "  background: transparent;"
        "}"
        "QScrollArea#viewportLevelSettingsScrollArea "
        "QScrollBar::handle:vertical {"
        "  min-height: 28px;"
        "  background: rgba(152, 166, 188, 150);"
        "  border-radius: 3px;"
        "}"
        "QScrollArea#viewportLevelSettingsScrollArea "
        "QScrollBar::handle:vertical:hover {"
        "  background: rgba(180, 197, 222, 205);"
        "}"
        "QScrollArea#viewportLevelSettingsScrollArea "
        "QScrollBar::add-line:vertical,"
        "QScrollArea#viewportLevelSettingsScrollArea "
        "QScrollBar::sub-line:vertical {"
        "  height: 0px;"
        "  background: transparent;"
        "  border: none;"
        "}"
        "QScrollArea#viewportLevelSettingsScrollArea "
        "QScrollBar::add-page:vertical,"
        "QScrollArea#viewportLevelSettingsScrollArea "
        "QScrollBar::sub-page:vertical {"
        "  background: transparent;"
        "}");

    auto *panel = new QWidget(viewportLevelSettingsMenu);
    panel->setObjectName("viewportLevelSettingsPanel");
    panel->setAttribute(Qt::WA_StyledBackground, true);
    viewportLevelSettingsScrollArea->setWidget(panel);

    auto *panelLayout = new QVBoxLayout(panel);
    panelLayout->setContentsMargins(10, 10, 10, 10);
    panelLayout->setSpacing(8);

    auto applyLevelSettingsUndoable = [this](const std::string &label,
                                             auto &&mutate) -> bool {
      if (!document_.applyUndoable(label, editor::DocumentDirty::LevelGeometry,
                                   std::forward<decltype(mutate)>(mutate))) {
        return false;
      }

      updateEditActions();
      if (glView_) {
        glView_->update();
      }
      return true;
    };

    auto colorToQ = [](const ColorF &c, const bool alpha) -> QColor {
      const float a = alpha ? c.a : 1.0f;
      return QColor::fromRgbF(std::clamp(static_cast<double>(c.r), 0.0, 1.0),
                              std::clamp(static_cast<double>(c.g), 0.0, 1.0),
                              std::clamp(static_cast<double>(c.b), 0.0, 1.0),
                              std::clamp(static_cast<double>(a), 0.0, 1.0));
    };

    auto qToColor = [](const QColor &qc) -> ColorF {
      return ColorF{
          static_cast<float>(qc.redF()),
          static_cast<float>(qc.greenF()),
          static_cast<float>(qc.blueF()),
          static_cast<float>(qc.alphaF()),
      };
    };

    auto setSwatch = [](QPushButton *button, const QColor &c) {
      if (!button) {
        return;
      }

      button->setText({});
      button->setMinimumHeight(22);
      button->setStyleSheet(
          QString(
              "QPushButton { background-color: rgba(%1, %2, %3, %4); border: "
              "1px solid rgba(255,255,255,60); border-radius: 3px; }"
              "QPushButton:hover { border: 1px solid rgba(255,255,255,120); }")
              .arg(c.red())
              .arg(c.green())
              .arg(c.blue())
              .arg(c.alpha()));
    };

    auto makeGroup =
        [&](const QString &title) -> std::pair<QGroupBox *, QFormLayout *> {
      auto *box = new QGroupBox(title, panel);
      box->setFlat(true);

      auto *form = new QFormLayout(box);
      form->setContentsMargins(0, 8, 0, 0);
      form->setHorizontalSpacing(10);
      form->setVerticalSpacing(6);
      return {box, form};
    };

    auto [generalBox, generalForm] = makeGroup("General");
    panelLayout->addWidget(generalBox);

    auto *gravitySpin = new QDoubleSpinBox(panel);
    gravitySpin->setDecimals(6);
    gravitySpin->setRange(-100000.0, 100000.0);
    gravitySpin->setSingleStep(0.1);
    gravitySpin->setKeyboardTracking(false);
    generalForm->addRow("Gravity", gravitySpin);

    auto *exportBufferButton = new QPushButton("Configure\u2026", panel);
    exportBufferButton->setObjectName("viewportLevelExportBuffersButton");
    exportBufferButton->setToolTip(
        "Reserve room for resources that scripts or gameplay may load after the level starts.");
    generalForm->addRow("Extra buffers", exportBufferButton);

    auto [classicHeaderBox, classicHeaderForm] =
        makeGroup("Classic Level Header");
    classicHeaderBox->setObjectName("viewportLevelSettingsClassicGroup");
    panelLayout->addWidget(classicHeaderBox);

    auto makeClassicDistanceSpin =
        [panel](const QString &objectName) -> QDoubleSpinBox * {
      auto *spin = new QDoubleSpinBox(panel);
      spin->setObjectName(objectName);
      spin->setDecimals(6);
      spin->setRange(0.0, 1000000000.0);
      spin->setSingleStep(0.1);
      spin->setKeyboardTracking(false);
      return spin;
    };

    std::array<QDoubleSpinBox *, 4> mipmapDistanceSpins{};
    std::array<QDoubleSpinBox *, 4> lodDistanceSpins{};
    for (std::size_t i = 0; i < mipmapDistanceSpins.size(); ++i) {
      const int fieldNumber = static_cast<int>(i) + 1;
      mipmapDistanceSpins[i] = makeClassicDistanceSpin(
          QStringLiteral("viewport.level.mipmap_distance_%1")
              .arg(fieldNumber));
      classicHeaderForm->addRow(
          QStringLiteral("Mipmap distance %1").arg(fieldNumber),
          mipmapDistanceSpins[i]);
    }
    for (std::size_t i = 0; i < lodDistanceSpins.size(); ++i) {
      const int fieldNumber = static_cast<int>(i) + 1;
      lodDistanceSpins[i] = makeClassicDistanceSpin(
          QStringLiteral("viewport.level.lod_distance_%1").arg(fieldNumber));
      classicHeaderForm->addRow(
          QStringLiteral("LOD distance %1").arg(fieldNumber),
          lodDistanceSpins[i]);
    }

    auto *perspectiveDistanceSpin = makeClassicDistanceSpin(
        QStringLiteral("viewport.level.perspective_distance"));
    classicHeaderForm->addRow("Perspective distance",
                              perspectiveDistanceSpin);

    auto *gouraudDistanceSpin = makeClassicDistanceSpin(
        QStringLiteral("viewport.level.gouraud_distance"));
    classicHeaderForm->addRow("Gouraud distance", gouraudDistanceSpin);

    auto *pixelsPerUnitSpin = makeClassicDistanceSpin(
        QStringLiteral("viewport.level.pixels_per_unit"));
    pixelsPerUnitSpin->setRange(0.000001, 1000000000.0);
    pixelsPerUnitSpin->setSingleStep(1.0);
    classicHeaderForm->addRow("Pixels per unit", pixelsPerUnitSpin);

    auto *masterCmpRow = new QWidget(panel);
    auto *masterCmpLayout = new QHBoxLayout(masterCmpRow);
    masterCmpLayout->setContentsMargins(0, 0, 0, 0);
    masterCmpLayout->setSpacing(4);
    auto *masterCmpEdit = new QLineEdit(masterCmpRow);
    masterCmpEdit->setObjectName("viewport.level.master_cmp");
    masterCmpEdit->setClearButtonEnabled(true);
    masterCmpEdit->setPlaceholderText("dflt.cmp");
    auto *masterCmpPick = new QToolButton(masterCmpRow);
    masterCmpPick->setObjectName("viewport.level.master_cmp.pick");
    masterCmpPick->setText(QStringLiteral("\u2026"));
    masterCmpPick->setToolTip(
        "<b>Master colormap</b><br>Choose the master colormap file for this level.");
    masterCmpPick->setAccessibleName("Pick master colormap");
    masterCmpLayout->addWidget(masterCmpEdit, 1);
    masterCmpLayout->addWidget(masterCmpPick);
    classicHeaderForm->addRow("Master CMP", masterCmpRow);

    auto [horizonBox, horizonForm] = makeGroup("Horizon Sky");
    panelLayout->addWidget(horizonBox);

    auto *horizonDistSpin = new QDoubleSpinBox(panel);
    horizonDistSpin->setDecimals(6);
    horizonDistSpin->setRange(0.0, 1000000.0);
    horizonDistSpin->setSingleStep(1.0);
    horizonDistSpin->setKeyboardTracking(false);
    horizonForm->addRow("Distance", horizonDistSpin);

    auto *horizonPprSpin = new QDoubleSpinBox(panel);
    horizonPprSpin->setDecimals(6);
    horizonPprSpin->setRange(-1000000.0, 1000000.0);
    horizonPprSpin->setSingleStep(1.0);
    horizonPprSpin->setKeyboardTracking(false);
    horizonForm->addRow("Pixels/Rev", horizonPprSpin);

    auto *horizonOffXSpin = new QDoubleSpinBox(panel);
    horizonOffXSpin->setDecimals(6);
    horizonOffXSpin->setRange(-1000000.0, 1000000.0);
    horizonOffXSpin->setSingleStep(0.01);
    horizonOffXSpin->setKeyboardTracking(false);
    horizonForm->addRow("Offset X", horizonOffXSpin);

    auto *horizonOffYSpin = new QDoubleSpinBox(panel);
    horizonOffYSpin->setDecimals(6);
    horizonOffYSpin->setRange(-1000000.0, 1000000.0);
    horizonOffYSpin->setSingleStep(0.01);
    horizonOffYSpin->setKeyboardTracking(false);
    horizonForm->addRow("Offset Y", horizonOffYSpin);

    auto [ceilingBox, ceilingForm] = makeGroup("Ceiling Sky");
    panelLayout->addWidget(ceilingBox);

    auto *ceilingHeightSpin = new QDoubleSpinBox(panel);
    ceilingHeightSpin->setDecimals(6);
    ceilingHeightSpin->setRange(0.0, 1000000.0);
    ceilingHeightSpin->setSingleStep(1.0);
    ceilingHeightSpin->setKeyboardTracking(false);
    ceilingForm->addRow("Height", ceilingHeightSpin);

    auto *ceilingOffXSpin = new QDoubleSpinBox(panel);
    ceilingOffXSpin->setDecimals(6);
    ceilingOffXSpin->setRange(-1000000.0, 1000000.0);
    ceilingOffXSpin->setSingleStep(0.01);
    ceilingOffXSpin->setKeyboardTracking(false);
    ceilingForm->addRow("Offset X", ceilingOffXSpin);

    auto *ceilingOffYSpin = new QDoubleSpinBox(panel);
    ceilingOffYSpin->setDecimals(6);
    ceilingOffYSpin->setRange(-1000000.0, 1000000.0);
    ceilingOffYSpin->setSingleStep(0.01);
    ceilingOffYSpin->setKeyboardTracking(false);
    ceilingForm->addRow("Offset Y", ceilingOffYSpin);

    auto [fogBox, fogForm] = makeGroup("Fog");
    fogBox->setObjectName("viewportLevelSettingsFogGroup");
    panelLayout->addWidget(fogBox);

    auto *fogEnabled = new QCheckBox("Enabled", panel);
    fogForm->addRow({}, fogEnabled);

    auto *fogColorButton = new QPushButton(panel);
    fogForm->addRow("Color", fogColorButton);

    auto *fogStartSpin = new QDoubleSpinBox(panel);
    fogStartSpin->setDecimals(6);
    fogStartSpin->setRange(-1000000.0, 1000000.0);
    fogStartSpin->setSingleStep(1.0);
    fogStartSpin->setKeyboardTracking(false);
    fogForm->addRow("Start", fogStartSpin);

    auto *fogEndSpin = new QDoubleSpinBox(panel);
    fogEndSpin->setDecimals(6);
    fogEndSpin->setRange(-1000000.0, 1000000.0);
    fogEndSpin->setSingleStep(1.0);
    fogEndSpin->setKeyboardTracking(false);
    fogForm->addRow("End", fogEndSpin);

    auto *action = new QWidgetAction(viewportLevelSettingsMenu);
    action->setDefaultWidget(viewportLevelSettingsScrollArea);
    viewportLevelSettingsMenu->addAction(action);

    viewportLevelSettingsMenu->setStyleSheet(
        "QMenu {"
        "  background-color: #15171c;"
        "  border: 1px solid #343b46;"
        "  border-radius: 10px;"
        "  padding: 0px;"
        "}"
        "QWidget#viewportLevelSettingsPanel {"
        "  background: transparent;"
        "  border: 0;"
        "  color: rgba(235, 236, 240, 255);"
        "}"
        "QGroupBox {"
        "  border: 0;"
        "  margin-top: 12px;"
        "  font-weight: 600;"
        "}"
        "QGroupBox::title {"
        "  subcontrol-origin: margin;"
        "  subcontrol-position: top left;"
        "  padding: 0 0 6px 0;"
        "}"
        "QLineEdit, QDoubleSpinBox {"
        "  background: rgba(0, 0, 0, 60);"
        "  border: 1px solid rgba(255, 255, 255, 40);"
        "  border-radius: 6px;"
        "  padding: 2px 6px;"
        "  color: rgba(235, 236, 240, 255);"
        "}");

    auto applyIfChanged = [applyLevelSettingsUndoable](
                              const std::string &label, const auto &getCurrent,
                              auto &&setCurrent, const double v) -> void {
      const double cur = getCurrent();
      if (std::abs(cur - v) <= sed::math::kTinyEpsilon) {
        return;
      }

      (void)applyLevelSettingsUndoable(
          label, [&](editor::DocumentState &state) -> bool {
            setCurrent(state.level.header, v);
            return true;
          });
    };

    for (std::size_t i = 0; i < mipmapDistanceSpins.size(); ++i) {
      connect(
          mipmapDistanceSpins[i],
          QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
          [this, applyLevelSettingsUndoable, i](const double value) {
            if (std::abs(document_.level().header.mipmapDistances[i] - value) <=
                sed::math::kTinyEpsilon) {
              return;
            }
            (void)applyLevelSettingsUndoable(
                "Edit mipmap distance " + std::to_string(i + 1),
                [i, value](editor::DocumentState &state) -> bool {
                  state.level.header.mipmapDistances[i] = value;
                  return true;
                });
          });
    }

    for (std::size_t i = 0; i < lodDistanceSpins.size(); ++i) {
      connect(lodDistanceSpins[i],
              QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
              [this, applyLevelSettingsUndoable, i](const double value) {
                if (std::abs(document_.level().header.lodDistances[i] -
                             value) <= sed::math::kTinyEpsilon) {
                  return;
                }
                (void)applyLevelSettingsUndoable(
                    "Edit LOD distance " + std::to_string(i + 1),
                    [i, value](editor::DocumentState &state) -> bool {
                      state.level.header.lodDistances[i] = value;
                      return true;
                    });
              });
    }

    connect(
        perspectiveDistanceSpin,
        QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
        [this, applyIfChanged](const double value) {
          applyIfChanged(
              "Edit perspective distance",
              [&]() { return document_.level().header.perspectiveDistance; },
              [](LevelHeader &header, const double next) {
                header.perspectiveDistance = next;
              },
              value);
        });

    connect(
        gouraudDistanceSpin,
        QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
        [this, applyIfChanged](const double value) {
          applyIfChanged(
              "Edit Gouraud distance",
              [&]() { return document_.level().header.gouraudDistance; },
              [](LevelHeader &header, const double next) {
                header.gouraudDistance = next;
              },
              value);
        });

    connect(
        pixelsPerUnitSpin,
        QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
        [this, applyLevelSettingsUndoable](const double value) {
          if (std::abs(document_.level().ppunit - value) <=
              sed::math::kTinyEpsilon) {
            return;
          }
          (void)applyLevelSettingsUndoable(
              "Edit pixels per unit",
              [value](editor::DocumentState &state) -> bool {
                state.level.ppunit = value;
                return true;
              });
        });

    auto applyMasterCmp =
        [this, masterCmpEdit, applyLevelSettingsUndoable]() -> void {
      const QString normalized = masterCmpEdit->text().trimmed();
      const std::string value = normalized.toStdString();
      if (sed::text::iequalsAscii(document_.level().masterCMP, value)) {
        masterCmpEdit->setText(
            QString::fromStdString(document_.level().masterCMP));
        return;
      }
      if (applyLevelSettingsUndoable(
              "Edit master CMP",
              [value](editor::DocumentState &state) -> bool {
                state.level.masterCMP = value;
                return true;
              })) {
        if (glView_) {
          glView_->reloadCachedResources();
          glView_->notifyLevelGeometryChanged();
        }
        invalidateThingPlacementResourcePreviews();
      }
      masterCmpEdit->setText(
          QString::fromStdString(document_.level().masterCMP));
    };

    connect(masterCmpEdit, &QLineEdit::editingFinished, this, applyMasterCmp);
    connect(masterCmpPick, &QToolButton::clicked, this,
            [this, masterCmpEdit, applyMasterCmp]() {
              ui::ResourcePickerOptions options{};
              options.kind = ui::ResourcePickerKind::cmp;
              options.initialName = masterCmpEdit->text().trimmed().toStdString();
              if (!resourceRoots_.empty()) {
                options.preferredDiskRoot = resourceRoots_.front();
              } else if (!resources_.roots().empty()) {
                options.preferredDiskRoot = resources_.roots().front();
              }
              if (!currentPath_.empty()) {
                options.projectDirectory = currentPath_.parent_path();
              }
              options.title = "Pick Master CMP";
              options.previewCmp = document_.level().getMasterCMP();
              options.previewProjectType = document_.level().kind;
              options.templateDatabaseFallback =
                  templates_.size() > 0 ? &templates_ : nullptr;
              options.templateOverrides = &document_.level().templates;
              const std::optional<std::string> picked = ui::pickResource(
                  this, resourceLocatorSnapshot_, options);
              if (!picked) {
                return;
              }
              masterCmpEdit->setText(QString::fromStdString(*picked));
              applyMasterCmp();
            });

    connect(gravitySpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, [this, applyIfChanged](double v) {
              applyIfChanged(
                  "Edit level gravity",
                  [&]() { return document_.level().header.gravity; },
                  [](LevelHeader &h, double x) { h.gravity = x; }, v);
            });

    connect(
        horizonDistSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
        this, [this, applyIfChanged](double v) {
          applyIfChanged(
              "Edit horizon distance",
              [&]() { return document_.level().header.horizonSky.distance; },
              [](LevelHeader &h, double x) { h.horizonSky.distance = x; }, v);
        });

    connect(
        horizonPprSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
        this, [this, applyIfChanged](double v) {
          applyIfChanged(
              "Edit horizon pixels-per-rev",
              [&]() {
                return document_.level().header.horizonSky.pixelsPerRev;
              },
              [](LevelHeader &h, double x) { h.horizonSky.pixelsPerRev = x; },
              v);
        });

    connect(
        horizonOffXSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
        this, [this, applyIfChanged](double v) {
          applyIfChanged(
              "Edit horizon offset X",
              [&]() { return document_.level().header.horizonSky.offset.x; },
              [](LevelHeader &h, double x) { h.horizonSky.offset.x = x; }, v);
        });

    connect(
        horizonOffYSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
        this, [this, applyIfChanged](double v) {
          applyIfChanged(
              "Edit horizon offset Y",
              [&]() { return document_.level().header.horizonSky.offset.y; },
              [](LevelHeader &h, double x) { h.horizonSky.offset.y = x; }, v);
        });

    connect(ceilingHeightSpin,
            QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
            [this, applyIfChanged](double v) {
              applyIfChanged(
                  "Edit ceiling height",
                  [&]() { return document_.level().header.ceilingSky.height; },
                  [](LevelHeader &h, double x) { h.ceilingSky.height = x; }, v);
            });

    connect(
        ceilingOffXSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
        this, [this, applyIfChanged](double v) {
          applyIfChanged(
              "Edit ceiling offset X",
              [&]() { return document_.level().header.ceilingSky.offset.x; },
              [](LevelHeader &h, double x) { h.ceilingSky.offset.x = x; }, v);
        });

    connect(
        ceilingOffYSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
        this, [this, applyIfChanged](double v) {
          applyIfChanged(
              "Edit ceiling offset Y",
              [&]() { return document_.level().header.ceilingSky.offset.y; },
              [](LevelHeader &h, double x) { h.ceilingSky.offset.y = x; }, v);
        });

    connect(fogEnabled, &QCheckBox::toggled, this,
            [this, applyLevelSettingsUndoable](bool checked) {
              const bool cur = document_.level().header.fog.enabled;
              if (cur == checked) {
                return;
              }

              (void)applyLevelSettingsUndoable(
                  "Edit fog enabled",
                  [&](editor::DocumentState &state) -> bool {
                    state.level.header.fog.enabled = checked;
                    return true;
                  });
            });

    connect(fogStartSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, [this, applyIfChanged](double v) {
              applyIfChanged(
                  "Edit fog start",
                  [&]() { return document_.level().header.fog.fogStart; },
                  [](LevelHeader &h, double x) { h.fog.fogStart = x; }, v);
            });

    connect(fogEndSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, [this, applyIfChanged](double v) {
              applyIfChanged(
                  "Edit fog end",
                  [&]() { return document_.level().header.fog.fogEnd; },
                  [](LevelHeader &h, double x) { h.fog.fogEnd = x; }, v);
            });

    connect(fogColorButton, &QPushButton::clicked, this,
            [this, fogColorButton, applyLevelSettingsUndoable, colorToQ,
             qToColor, setSwatch]() {
              const QColor cur =
                  colorToQ(document_.level().header.fog.color, true);
              QColorDialog dlg(cur, this);
              dlg.setOption(QColorDialog::ShowAlphaChannel, true);
              if (dlg.exec() != QDialog::Accepted) {
                return;
              }

              const ColorF chosen = qToColor(dlg.selectedColor());
              (void)applyLevelSettingsUndoable(
                  "Edit fog color", [&](editor::DocumentState &state) -> bool {
                    state.level.header.fog.color = chosen;
                    return true;
                  });

              setSwatch(fogColorButton,
                        colorToQ(document_.level().header.fog.color, true));
            });

    connect(
        exportBufferButton, &QPushButton::clicked, this,
        [this, viewportLevelSettingsMenu]() {
          if (viewportLevelSettingsMenu) {
            viewportLevelSettingsMenu->close();
          }

          // Copy the authored inputs once, then perform the dependency crawl
          // away from the UI thread. The dialog can appear immediately and
          // the snapshots keep a cancelled dialog from racing later edits.
          const auto levelSnapshot =
              std::make_shared<const Level>(document_.level());
          const auto templatesSnapshot =
              std::make_shared<const assets::Templates>(templates_);
          const auto resourcesSnapshot = resourceLocatorSnapshot_
              ? resourceLocatorSnapshot_
              : makeResourceLocatorSnapshot(resources_);

          struct BufferInspectionResult {
            assets::JklExportBufferReport report{};
            QString error{};
          };

          QDialog dialog(this);
          dialog.setObjectName("levelExtraBuffersDialog");
          dialog.setWindowTitle("Extra Buffers");
          dialog.setModal(true);
          dialog.resize(780, 560);

          auto *root = new QVBoxLayout(&dialog);
          auto *explanation =
              new QLabel("Use Extra to reserve room for resources that your "
                         "scripts or gameplay may load after the level starts. "
                         "Required is what this level already needs; Export "
                         "total is Required + Extra. Zero adds no reserve. "
                         "Geometry, colormap, and sector counts follow the "
                         "level itself and are not adjustable here.",
                         &dialog);
          explanation->setObjectName("levelExtraBuffersExplanation");
          explanation->setWordWrap(true);
          root->addWidget(explanation);

          auto *status = new QLabel(
              "Checking the resources used by this level\u2026", &dialog);
          status->setObjectName("levelExtraBuffersStatus");
          status->setWordWrap(true);
          root->addWidget(status);

          auto *warning =
              new QLabel("Some dependencies could not be inspected. Required "
                         "may be incomplete; SED will warn but will not "
                         "silently add hidden capacity.",
                         &dialog);
          warning->setObjectName("levelExtraBuffersIncompleteWarning");
          warning->setWordWrap(true);
          warning->hide();
          root->addWidget(warning);

          auto *scroll = new QScrollArea(&dialog);
          scroll->setWidgetResizable(true);
          scroll->setFrameShape(QFrame::NoFrame);
          auto *fieldsWidget = new QWidget(scroll);
          auto *grid = new QGridLayout(fieldsWidget);
          grid->setColumnStretch(0, 1);
          grid->setColumnStretch(1, 0);
          grid->setColumnStretch(2, 1);
          grid->setColumnStretch(3, 0);
          grid->setHorizontalSpacing(16);
          grid->setVerticalSpacing(7);

          const std::array<const char *, 4> columnNames{
              {"Resource", "Required", "Extra", "Export total"}};
          for (int column = 0; column < static_cast<int>(columnNames.size());
               ++column) {
            auto *header =
                new QLabel(QString::fromUtf8(
                               columnNames[static_cast<std::size_t>(column)]),
                           fieldsWidget);
            QFont font = header->font();
            font.setBold(true);
            header->setFont(font);
            header->setAlignment(column == 0 ? Qt::AlignLeft : Qt::AlignRight);
            grid->addWidget(header, 0, column);
          }

          struct BufferField {
            const char *label{};
            const char *objectSuffix{};
            const char *tooltip{};
            std::uint32_t LevelExportBufferExtras::*member{};
            assets::JklExportBufferUsage assets::JklExportBufferReport::*
                reportMember{};
            std::uint64_t requiredCount{};
            QLabel *required{};
            QSpinBox *spin{};
            QLabel *total{};
          };

          std::array<BufferField, 12> fields{{
              {"Sounds", "sounds",
               "Sound resources collected from sectors, COGs, sound classes, "
               "and their dependencies.",
               &LevelExportBufferExtras::sounds,
               &assets::JklExportBufferReport::sounds},
              {"Materials", "materials",
               "Material resources required by surfaces, templates, sprites, "
               "particles, and 3DOs.",
               &LevelExportBufferExtras::materials,
               &assets::JklExportBufferReport::materials},
              {"AI classes", "aiClasses",
               "AI-class resources required by the level.",
               &LevelExportBufferExtras::aiClasses,
               &assets::JklExportBufferReport::aiClasses},
              {"Models", "models", "3DO model resources required by the level.",
               &LevelExportBufferExtras::models,
               &assets::JklExportBufferReport::models},
              {"Sprites", "sprites", "Sprite resources required by the level.",
               &LevelExportBufferExtras::sprites,
               &assets::JklExportBufferReport::sprites},
              {"Keyframes", "keyframes",
               "Keyframe resources required by the level.",
               &LevelExportBufferExtras::keyframes,
               &assets::JklExportBufferReport::keyframes},
              {"Puppets", "puppets",
               "Animation-class resources required by the level.",
               &LevelExportBufferExtras::puppets,
               &assets::JklExportBufferReport::puppets},
              {"Sound classes", "soundClasses",
               "Sound-class resources required by the level.",
               &LevelExportBufferExtras::soundClasses,
               &assets::JklExportBufferReport::soundClasses},
              {"COG scripts", "cogScripts",
               "Unique COG script definitions required by the level.",
               &LevelExportBufferExtras::cogScripts,
               &assets::JklExportBufferReport::cogScripts},
              {"COG instances", "cogs",
               "Placed COG rows plus one required instance for every explicit "
               "non-none template or Thing cog= parameter. Duplicate script "
               "uses are counted separately.",
               &LevelExportBufferExtras::cogs,
               &assets::JklExportBufferReport::cogs},
              {"Templates", "templates",
               "Every template emitted with the level.",
               &LevelExportBufferExtras::templates,
               &assets::JklExportBufferReport::templates},
              {"Things", "things",
               "Placed Thing records required by the level.",
               &LevelExportBufferExtras::things,
               &assets::JklExportBufferReport::things},
          }};

          const LevelExportBufferExtras current =
              document_.level().exportBufferExtras;
          constexpr std::uint64_t maxTextWorldCapacity =
              static_cast<std::uint64_t>(
                  std::numeric_limits<std::int32_t>::max());
          int row = 1;
          for (BufferField &field : fields) {
            auto *name =
                new QLabel(QString::fromUtf8(field.label), fieldsWidget);
            field.required = new QLabel(QString::fromUtf8("\u2026"), fieldsWidget);
            field.spin = new QSpinBox(fieldsWidget);
            field.spin->setObjectName(
                QStringLiteral("levelExportBufferExtra.%1")
                    .arg(QString::fromLatin1(field.objectSuffix)));
            field.spin->setRange(0, 1000000);
            field.spin->setSingleStep(1);
            field.spin->setKeyboardTracking(false);
            field.spin->setValue(static_cast<int>(
                std::min<std::uint64_t>(current.*(field.member), 1000000u)));
            field.total = new QLabel(QString::fromUtf8("\u2026"), fieldsWidget);

            name->setObjectName(
                QStringLiteral("levelExportBufferName.%1")
                    .arg(QString::fromLatin1(field.objectSuffix)));
            field.required->setObjectName(
                QStringLiteral("levelExportBufferRequired.%1")
                    .arg(QString::fromLatin1(field.objectSuffix)));
            field.total->setObjectName(
                QStringLiteral("levelExportBufferTotal.%1")
                    .arg(QString::fromLatin1(field.objectSuffix)));
            const bool isIjimMaterials =
                document_.level().kind == ProjectType::IJIM
                && field.member == &LevelExportBufferExtras::materials;
            const QString tooltip = isIjimMaterials
                ? QStringLiteral(
                      "For IJIM NDY export, Materials Extra must be 0 or 64. "
                      "Zero adds no spare dynamic MAT slots; 64 adds 64 beyond "
                      "the required dynamic MAT rows. Other values are retained "
                      "in SED and block export.")
                : QString::fromUtf8(field.tooltip);
            for ( QWidget* widget : std::array<QWidget *, 4>{
                     name, field.required, field.spin, field.total } )
            {
                widget->setToolTip(tooltip);
            }
            field.required->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
            field.total->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

            BufferField *fieldPtr = &field;
            connect(
                field.spin, &QSpinBox::valueChanged, field.total,
                [fieldPtr,
                 maxTextWorldCapacity](const int additional) {
                  const std::uint64_t finalCapacity = std::min<std::uint64_t>(
                      maxTextWorldCapacity,
                      fieldPtr->requiredCount
                          + static_cast<std::uint64_t>(additional));
                  fieldPtr->total->setText(QString::number(finalCapacity));
                });

            grid->addWidget(name, row, 0);
            grid->addWidget(field.required, row, 1);
            grid->addWidget(field.spin, row, 2);
            grid->addWidget(field.total, row, 3);
            ++row;
          }
          scroll->setWidget(fieldsWidget);
          root->addWidget(scroll, 1);

          auto *buttons = new QDialogButtonBox(
              QDialogButtonBox::Ok | QDialogButtonBox::Cancel |
                  QDialogButtonBox::RestoreDefaults,
              &dialog);
          buttons->setObjectName("levelExtraBuffersButtons");
          if (QPushButton *restore =
                  buttons->button(QDialogButtonBox::RestoreDefaults)) {
            restore->setObjectName("levelExtraBuffersRestoreDefaultsButton");
            restore->setText("Restore Defaults");
            connect(restore, &QPushButton::clicked, &dialog, [this, &fields]() {
              const QSettings settings = settingsStore();
              const LevelExportBufferExtras defaults =
                  ui::project::configuredNewProjectExportBufferExtras(
                      settings, document_.level().kind);
              for (const BufferField &field : fields) {
                field.spin->setValue(
                    static_cast<int>(defaults.*(field.member)));
              }
            });
          }
          connect(buttons, &QDialogButtonBox::accepted, &dialog,
                  &QDialog::accept);
          connect(buttons, &QDialogButtonBox::rejected, &dialog,
                  &QDialog::reject);
          root->addWidget(buttons);

          QPushButton *okButton = buttons->button(QDialogButtonBox::Ok);
          if (okButton) {
            okButton->setEnabled(false);
            okButton->setToolTip(
                "Available after SED finishes checking this level's resources.");
          }

          auto *watcher = new QFutureWatcher<BufferInspectionResult>(&dialog);
          connect(
              watcher, &QFutureWatcher<BufferInspectionResult>::finished,
              &dialog,
              [watcher, status, warning, okButton, &fields,
               maxTextWorldCapacity]() {
                const BufferInspectionResult result = watcher->result();
                if (!result.error.isEmpty()) {
                  status->setText(
                      QStringLiteral("SED could not calculate the resources "
                                     "required by this level: %1")
                          .arg(result.error));
                  return;
                }

                status->hide();
                warning->setVisible(!result.report.dependencyScanComplete);
                for (BufferField &field : fields) {
                  const assets::JklExportBufferUsage &usage =
                      result.report.*(field.reportMember);
                  field.requiredCount = usage.collected;
                  field.required->setText(
                      QString::number(field.requiredCount));
                  const std::uint64_t total = std::min<std::uint64_t>(
                      maxTextWorldCapacity,
                      field.requiredCount
                          + static_cast<std::uint64_t>(field.spin->value()));
                  field.total->setText(QString::number(total));
                }
                if (okButton) {
                  okButton->setEnabled(true);
                  okButton->setToolTip({});
                }
              });
          watcher->setFuture(QtConcurrent::run(
              [levelSnapshot, templatesSnapshot,
               resourcesSnapshot]() -> BufferInspectionResult {
                BufferInspectionResult result{};
                try {
                  assets::JklSaveOptions options{};
                  options.resources = resourcesSnapshot.get();
                  options.templatesDb = templatesSnapshot->size() > 0
                      ? templatesSnapshot.get()
                      : nullptr;
                  result.report = assets::inspectJklExportBuffers(
                      *levelSnapshot, options);
                } catch (const std::exception &error) {
                  result.error = QString::fromUtf8(error.what());
                } catch (...) {
                  result.error = QStringLiteral(
                      "an unknown error occurred while checking resources");
                }
                return result;
              }));

          if (dialog.exec() != QDialog::Accepted) {
            return;
          }

          LevelExportBufferExtras chosen{};
          for (const BufferField &field : fields) {
            chosen.*(field.member) =
                static_cast<std::uint32_t>(field.spin->value());
          }
          if (chosen == document_.level().exportBufferExtras) {
            return;
          }

          if (document_.applyUndoable(
                  "Export Buffers: Extra capacity changed",
                  editor::DocumentDirty::Structure,
                  [chosen](editor::DocumentState &state) -> bool {
                    state.level.exportBufferExtras = chosen;
                    return true;
                  })) {
            updateEditActions();
          }
        });

    connect(
        viewportLevelSettingsMenu, &QMenu::aboutToShow, this,
        [this, panel, viewportLevelSettingsButton,
         viewportLevelSettingsScrollArea, viewportLevelSettingsMenu,
         classicHeaderBox, mipmapDistanceSpins, lodDistanceSpins,
         perspectiveDistanceSpin, gouraudDistanceSpin, pixelsPerUnitSpin,
         masterCmpEdit, fogBox,
         horizonForm, horizonPprSpin, gravitySpin, horizonDistSpin,
         horizonOffXSpin, horizonOffYSpin, ceilingHeightSpin, ceilingOffXSpin,
         ceilingOffYSpin, fogEnabled, fogStartSpin, fogEndSpin, fogColorButton,
         setSwatch, colorToQ]() {
          const Level &level = document_.level();
          const LevelHeader &h = level.header;

          const bool isIjim = level.kind == ProjectType::IJIM;
          classicHeaderBox->setVisible(!isIjim);
          fogBox->setVisible(isIjim);

          if (QWidget *lbl = horizonForm->labelForField(horizonPprSpin)) {
            lbl->setVisible(!isIjim);
          }
          horizonPprSpin->setVisible(!isIjim);

          const QSignalBlocker b0(gravitySpin);
          const QSignalBlocker b1(horizonDistSpin);
          const QSignalBlocker b2(horizonPprSpin);
          const QSignalBlocker b3(horizonOffXSpin);
          const QSignalBlocker b4(horizonOffYSpin);
          const QSignalBlocker b5(ceilingHeightSpin);
          const QSignalBlocker b6(ceilingOffXSpin);
          const QSignalBlocker b7(ceilingOffYSpin);
          const QSignalBlocker b8(fogEnabled);
          const QSignalBlocker b9(fogStartSpin);
          const QSignalBlocker b10(fogEndSpin);

          for (std::size_t i = 0; i < mipmapDistanceSpins.size(); ++i) {
            const QSignalBlocker blocker(mipmapDistanceSpins[i]);
            mipmapDistanceSpins[i]->setValue(h.mipmapDistances[i]);
          }
          for (std::size_t i = 0; i < lodDistanceSpins.size(); ++i) {
            const QSignalBlocker blocker(lodDistanceSpins[i]);
            lodDistanceSpins[i]->setValue(h.lodDistances[i]);
          }
          {
            const QSignalBlocker blocker(perspectiveDistanceSpin);
            perspectiveDistanceSpin->setValue(h.perspectiveDistance);
          }
          {
            const QSignalBlocker blocker(gouraudDistanceSpin);
            gouraudDistanceSpin->setValue(h.gouraudDistance);
          }
          {
            const QSignalBlocker blocker(pixelsPerUnitSpin);
            pixelsPerUnitSpin->setValue(level.ppunit);
          }
          {
            const QSignalBlocker blocker(masterCmpEdit);
            masterCmpEdit->setText(QString::fromStdString(level.masterCMP));
          }

          gravitySpin->setValue(h.gravity);
          horizonDistSpin->setValue(h.horizonSky.distance);
          horizonPprSpin->setValue(h.horizonSky.pixelsPerRev);
          horizonOffXSpin->setValue(h.horizonSky.offset.x);
          horizonOffYSpin->setValue(h.horizonSky.offset.y);
          ceilingHeightSpin->setValue(h.ceilingSky.height);
          ceilingOffXSpin->setValue(h.ceilingSky.offset.x);
          ceilingOffYSpin->setValue(h.ceilingSky.offset.y);

          fogEnabled->setChecked(h.fog.enabled);
          fogStartSpin->setValue(h.fog.fogStart);
          fogEndSpin->setValue(h.fog.fogEnd);

          setSwatch(fogColorButton, colorToQ(h.fog.color, true));

          // Ensure the popup compacts/expands when project-specific fields are
          // hidden/shown.
          if (panel && viewportLevelSettingsButton &&
              viewportLevelSettingsScrollArea && viewportLevelSettingsMenu) {
            constexpr int margin = 10;
            constexpr int gap = 6;
            constexpr int scrollBarWidth = 8;
            constexpr int menuFrame = 2;

            panel->setMinimumSize(QSize(0, 0));
            panel->setMaximumSize(QSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX));
            panel->adjustSize();
            const QSize naturalSize = panel->sizeHint().expandedTo(QSize(1, 1));
            const QPoint buttonTopLeft =
                glView_
                    ? viewportLevelSettingsButton->mapTo(glView_, QPoint(0, 0))
                    : QPoint(0, 0);
            const int buttonBottom =
                buttonTopLeft.y() + viewportLevelSettingsButton->height();
            const int overlayBottom =
                viewportOverlay_ ? viewportOverlay_->geometry().bottom() + 1
                                 : buttonBottom;
            const int panelTop = std::max(buttonBottom, overlayBottom) + gap;
            const int availableMenuHeight =
                glView_ ? std::max(1, glView_->height() - panelTop - margin)
                        : naturalSize.height() + menuFrame;
            const int availableContentHeight =
                std::max(1, availableMenuHeight - menuFrame);
            const bool needsVerticalScroll =
                naturalSize.height() > availableContentHeight;

            viewportLevelSettingsScrollArea->setVerticalScrollBarPolicy(
                needsVerticalScroll ? Qt::ScrollBarAlwaysOn
                                    : Qt::ScrollBarAlwaysOff);
            viewportLevelSettingsScrollArea->verticalScrollBar()->setFixedWidth(
                scrollBarWidth);
            panel->setFixedSize(naturalSize);
            viewportLevelSettingsScrollArea->setFixedSize(
                naturalSize.width() +
                    (needsVerticalScroll ? scrollBarWidth : 0),
                std::min(naturalSize.height(), availableContentHeight));

            viewportLevelSettingsMenu->setMinimumSize(QSize(0, 0));
            viewportLevelSettingsMenu->setMaximumSize(
                QSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX));
            viewportLevelSettingsMenu->setFixedSize(
                viewportLevelSettingsScrollArea->width() + menuFrame,
                viewportLevelSettingsScrollArea->height() + menuFrame);
          }
        });
  }
  anchorPopupBelowViewportOverlay(viewportLevelSettingsMenu,
                                  viewportLevelSettingsButton);

  // Viewport settings (grid, layers, etc.)
  viewportMapSettingsScrollArea_ = new QScrollArea(glView_);
  QScrollArea *viewportMapSettingsScrollArea = viewportMapSettingsScrollArea_;
  viewportMapSettingsScrollArea->setObjectName("viewportMapSettingsScrollArea");
  viewportMapSettingsScrollArea->setWindowFlags(Qt::Widget);
  viewportMapSettingsScrollArea->setWidgetResizable(false);
  viewportMapSettingsScrollArea->setAlignment(Qt::AlignLeft | Qt::AlignTop);
  viewportMapSettingsScrollArea->setHorizontalScrollBarPolicy(
      Qt::ScrollBarAlwaysOff);
  viewportMapSettingsScrollArea->setVerticalScrollBarPolicy(
      Qt::ScrollBarAlwaysOff);
  viewportMapSettingsScrollArea->setFrameShape(QFrame::NoFrame);
  viewportMapSettingsScrollArea->viewport()->setAutoFillBackground(false);
  viewportMapSettingsScrollArea->setStyleSheet(
      "QScrollArea#viewportMapSettingsScrollArea {"
      "  background-color: #15171c;"
      "  border: 1px solid #343b46;"
      "  border-radius: 10px;"
      "}"
      "QScrollArea#viewportMapSettingsScrollArea > QWidget > QWidget {"
      "  background: transparent;"
      "}"
      "QScrollArea#viewportMapSettingsScrollArea QScrollBar:vertical {"
      "  width: 8px;"
      "  margin: 4px 1px;"
      "  background: transparent;"
      "}"
      "QScrollArea#viewportMapSettingsScrollArea QScrollBar::handle:vertical {"
      "  min-height: 28px;"
      "  background: rgba(152, 166, 188, 150);"
      "  border-radius: 3px;"
      "}"
      "QScrollArea#viewportMapSettingsScrollArea "
      "QScrollBar::handle:vertical:hover {"
      "  background: rgba(180, 197, 222, 205);"
      "}"
      "QScrollArea#viewportMapSettingsScrollArea QScrollBar::add-line:vertical,"
      "QScrollArea#viewportMapSettingsScrollArea QScrollBar::sub-line:vertical "
      "{"
      "  height: 0px;"
      "  background: transparent;"
      "  border: none;"
      "}"
      "QScrollArea#viewportMapSettingsScrollArea QScrollBar::add-page:vertical,"
      "QScrollArea#viewportMapSettingsScrollArea QScrollBar::sub-page:vertical "
      "{"
      "  background: transparent;"
      "}");
  viewportMapSettingsScrollArea->hide();

  auto *persistentViewportMapSettingsMenu =
      new PersistentViewportSettingsMenu();
  persistentViewportMapSettingsMenu->setScrollArea(
      viewportMapSettingsScrollArea);
  viewportMapSettingsMenu_ = persistentViewportMapSettingsMenu;
  QMenu *viewportMapSettingsMenu = viewportMapSettingsMenu_;
  viewportMapSettingsMenu->setObjectName("viewportMapSettingsMenu");
  viewportMapSettingsMenu->setToolTipsVisible(true);
  viewportMapSettingsScrollArea->setWidget(viewportMapSettingsMenu);
  viewportMapSettingsMenu->hide();
  {
    auto *showGridAct = viewportMapSettingsMenu->addAction("Grid");
    showGridAct->setObjectName("viewport.show_grid");
    showGridAct->setIcon(ui::makeViewportIcon(ui::ViewportIcon::mapShowGrid));
    showGridAct->setCheckable(true);
    showGridAct->setChecked(glView_ && glView_->showGrid());
    showGridAct->setShortcutContext(Qt::WidgetShortcut);
    showGridAct->setAutoRepeat(false);
    showGridAct->setToolTip(
        "<b>Grid</b><br>Show or hide the construction grid in the viewport.");

    auto *renderFogAct = viewportMapSettingsMenu->addAction("Render fog");
    renderFogAct->setObjectName("viewport.render_fog");
    renderFogAct->setIcon(ui::makeViewportIcon(ui::ViewportIcon::mapFog));
    renderFogAct->setCheckable(true);
    renderFogAct->setToolTip(
        "<b>Render fog</b><br>Show level fog in the current Editor or Game Preview view.");
    renderFogAct->setVisible(
        ui::viewport::viewportFogRenderOptionAvailable(document_.level().kind));

    auto *xraySelectionAct =
        viewportMapSettingsMenu->addAction("X-Ray Selection");
    xraySelectionAct->setObjectName("viewport.xray_selection_cycling");
    xraySelectionAct->setCheckable(true);
    xraySelectionAct->setChecked(glView_ &&
                                 glView_->xraySelectionCyclingEnabled());
    xraySelectionAct->setToolTip(
        "<b>X-ray selection</b><br>Cycle selectable objects from front to back, including objects behind level geometry. Click an already-selected front object again without a selection modifier to reach the next one; selection modifiers keep the front object selected.");

    auto *orbitAroundSelectionAct =
        viewportMapSettingsMenu->addAction("Orbit Around Selection");
    orbitAroundSelectionAct->setObjectName("viewport.orbit_around_selection");
    orbitAroundSelectionAct->setCheckable(true);
    orbitAroundSelectionAct->setChecked(glView_ &&
                                        glView_->orbitAroundSelectionEnabled());
    orbitAroundSelectionAct->setToolTip(
        "<b>Orbit around selection</b><br>Use the selected objects as the center when you orbit with the middle mouse button. Turn this off to orbit around the camera target; an empty selection already uses the camera target.");

    auto *showArchThingGizmosAct =
        new QAction("Arch Things", viewportMapSettingsMenu);
    showArchThingGizmosAct->setObjectName("viewport.gizmos.arch_thing");
    showArchThingGizmosAct->setCheckable(true);
    showArchThingGizmosAct->setProperty(
        "sed.editorSettingKey",
        QStringLiteral("viewport/arch_thing_gizmos"));
    showArchThingGizmosAct->setProperty(
        "sed.previewSettingKey",
        QStringLiteral("viewport/preview_arch_thing_gizmos"));
    showArchThingGizmosAct->setToolTip(
        "<b>Arch Thing markers</b><br>Show markers for Things that use the classic Arch flag.");
    showArchThingGizmosAct->setIcon(
        ui::makeViewportIcon(ui::ViewportIcon::lightArchitectural));
    showArchThingGizmosAct->setChecked(
        glView_ && glView_->showArchThingGizmos());
    connect(showArchThingGizmosAct, &QAction::toggled, this,
        [this](const bool checked) {
          const bool preview = glView_ && glView_->renderPipeline()
              == GlView::RenderPipeline::GamePreview;
          const QString key = preview
              ? QStringLiteral("viewport/preview_arch_thing_gizmos")
              : QStringLiteral("viewport/arch_thing_gizmos");
          settingsStore().setValue(key, checked);
          if (glView_) {
            glView_->setShowArchThingGizmos(checked);
          }
        });

    auto *showSectorThrustVectorsAct =
        new QAction("Sector Thrust Vectors", viewportMapSettingsMenu);
    showSectorThrustVectorsAct->setObjectName(
        "viewport.show_sector_thrust_vectors");
    showSectorThrustVectorsAct->setCheckable(true);
    showSectorThrustVectorsAct->setProperty(
        "sed.editorSettingKey",
        QStringLiteral("viewport/show_sector_thrust_vectors"));
    showSectorThrustVectorsAct->setProperty(
        "sed.previewSettingKey",
        QStringLiteral("viewport/preview_show_sector_thrust_vectors"));
    showSectorThrustVectorsAct->setToolTip(
        "<b>Sector thrust vectors</b><br>Show thrust arrows while the Sector Thrust tool is off.");
    showSectorThrustVectorsAct->setIcon(
        ui::makeViewportIcon(ui::ViewportIcon::sectorThrust));
    const bool showSectorThrustVectors =
        settingsStore()
            .value("viewport/show_sector_thrust_vectors", true)
            .toBool();
    showSectorThrustVectorsAct->setChecked(showSectorThrustVectors);
    if (glView_) {
      glView_->setShowSectorThrustVectors(showSectorThrustVectors);
    }
    connect(showSectorThrustVectorsAct, &QAction::toggled, this,
            [this, showSectorThrustVectorsAct](const bool checked) {
              const bool preview =
                  glView_ && glView_->renderPipeline() ==
                                 GlView::RenderPipeline::GamePreview;
              settingsStore().setValue(
                  showSectorThrustVectorsAct
                      ->property(preview ? "sed.previewSettingKey"
                                         : "sed.editorSettingKey")
                      .toString(),
                  checked);
              if (glView_) {
                glView_->setShowSectorThrustVectors(checked);
              }
            });

    auto *followGridAct =
        viewportMapSettingsMenu->addAction("Grid follows camera");
    followGridAct->setObjectName("viewport.map.grid_follow_camera");
    followGridAct->setIcon(
        ui::makeViewportIcon(ui::ViewportIcon::mapGridFollow));
    followGridAct->setCheckable(true);
    followGridAct->setToolTip(
        "<b>Grid follows camera</b><br>Keep the grid centered on the camera target as you move through the level.");

    auto *rotateGridAct =
        viewportMapSettingsMenu->addAction("Rotate grid to view");
    rotateGridAct->setObjectName("viewport.map.grid_rotate_to_view");
    rotateGridAct->setIcon(
        ui::makeViewportIcon(ui::ViewportIcon::mapGridRotateView));
    rotateGridAct->setCheckable(true);
    rotateGridAct->setToolTip(
        "<b>Rotate grid to view</b><br>Turn the grid plane to face the current view as you orbit the camera.");

    auto *keepViewGridVisibleAct =
        viewportMapSettingsMenu->addAction("Keep rotated grid visible");
    keepViewGridVisibleAct->setObjectName(
        "viewport.map.grid_keep_view_aligned_visible");
    keepViewGridVisibleAct->setIcon(
        ui::makeViewportIcon(ui::ViewportIcon::mapGridRotateView));
    keepViewGridVisibleAct->setCheckable(true);
    keepViewGridVisibleAct->setToolTip(
        "<b>Keep rotated grid visible</b><br>When Rotate grid to view reaches or passes the camera, show a matching guide on screen. Snapping still uses the original grid plane.");

    auto *adaptiveGridAct =
        viewportMapSettingsMenu->addAction("Auto scale with zoom");
    adaptiveGridAct->setObjectName("viewport.map.grid_adaptive_spacing");
    adaptiveGridAct->setIcon(
        ui::makeViewportIcon(ui::ViewportIcon::mapGridAdaptive));
    adaptiveGridAct->setCheckable(true);
    adaptiveGridAct->setToolTip(
        "<b>Auto scale with zoom</b><br>Adjust major and minor grid spacing as you zoom so the grid stays readable.");

    auto *gridThroughAct =
        viewportMapSettingsMenu->addAction("Grid visible through walls");
    gridThroughAct->setObjectName("viewport.map.grid_visible_through_walls");
    gridThroughAct->setIcon(
        ui::makeViewportIcon(ui::ViewportIcon::mapGridVisibleThrough));
    gridThroughAct->setCheckable(true);
    gridThroughAct->setToolTip(
        "<b>Grid visible through walls</b><br>Draw the grid over level geometry and Things, including parts behind them. Turn this and Always Behind off to let geometry hide the grid.");

    auto *gridBehindAct =
        viewportMapSettingsMenu->addAction("Grid always behind geometry");
    gridBehindAct->setObjectName("viewport.map.grid_always_behind");
    gridBehindAct->setIcon(
        ui::makeViewportIcon(ui::ViewportIcon::mapGridBehind));
    gridBehindAct->setCheckable(true);
    gridBehindAct->setToolTip(
        "<b>Grid always behind geometry</b><br>Draw the grid behind all level geometry. Turn this and Visible Through Walls off to let geometry hide the grid.");

    auto *resetGridOnRotateAct =
        viewportMapSettingsMenu->addAction("Reset grid on camera rotate");
    resetGridOnRotateAct->setObjectName("viewport.map.grid_reset_on_rotate");
    resetGridOnRotateAct->setIcon(
        ui::makeViewportIcon(ui::ViewportIcon::mapGridReset));
    resetGridOnRotateAct->setCheckable(true);
    resetGridOnRotateAct->setToolTip(
        "<b>Reset grid on camera rotate</b><br>Clear temporary grid orientation and center changes when you rotate the view.");

    // Reuse these persistent actions as the keyboard surface. They are created
    // after the menu keymap pass, so restore defaults/custom/cleared bindings
    // here without replacing their authoritative toggle and settings handlers.
    const QSettings mapKeymapSettings = settingsStore();
    for ( QAction* const action : {
        showGridAct, renderFogAct, xraySelectionAct, orbitAroundSelectionAct,
        showArchThingGizmosAct, showSectorThrustVectorsAct, followGridAct,
        rotateGridAct, keepViewGridVisibleAct, adaptiveGridAct, resetGridOnRotateAct,
        alwaysShowBakedLightGizmosAction_, alwaysShowThingLightGizmosAction_,
        alwaysShowSectorLightGizmosAction_, thingGizmosAction_ } )
    {
        if ( !action )
        {
            continue;
        }
        action->setProperty("sed_keymap_expose", true);
        action->setShortcutContext(Qt::WidgetShortcut);
        action->setAutoRepeat(false);
        if ( !action->property("sed_default_shortcuts").isValid() )
        {
            action->setProperty("sed_default_shortcuts", action == showGridAct
                ? QStringList{QStringLiteral("G")} : QStringList{});
        }
        const QStringList defaults = action->property("sed_default_shortcuts").toStringList();
        const QStringList configured = mapKeymapSettings.value(
            QStringLiteral("keymap/shortcuts/") + action->objectName(), defaults).toStringList();
        QList<QKeySequence> shortcuts{};
        for ( const QString& text : configured )
        {
            const QKeySequence sequence(text.trimmed(), QKeySequence::PortableText);
            if ( !sequence.isEmpty() && !shortcuts.contains(sequence) )
            {
                shortcuts.push_back(sequence);
            }
        }
        action->setShortcuts(shortcuts);
        if ( glView_ )
        {
            glView_->addAction(action);
        }
    }

    if (viewportGridAlignToViewAction_) {
      viewportGridAlignToViewAction_->setIcon(
          ui::makeViewportIcon(ui::ViewportIcon::mapGridAlignView));
      viewportGridAlignToViewAction_->setToolTip(
          "<b>Align grid to view</b><br>Keep the grid plane parallel to the current view.");
    }
    if (viewportGridOrientToObjectAction_) {
      viewportGridOrientToObjectAction_->setIcon(
          ui::makeViewportIcon(ui::ViewportIcon::mapGridOrientObject));
      viewportGridOrientToObjectAction_->setToolTip(
          "<b>Orient grid to object</b><br>Set the grid plane and center from the selected object or element.");
    }
    if (viewportGridOrientToSelectionAction_) {
      viewportGridOrientToSelectionAction_->setIcon(
          ui::makeViewportIcon(ui::ViewportIcon::mapGridOrientSelection));
      viewportGridOrientToSelectionAction_->setToolTip(
          "<b>Orient grid to selection</b><br>Set the grid plane from the selected geometry.");
    }
    if (viewportGridCenterOnSelectionAction_) {
      viewportGridCenterOnSelectionAction_->setIcon(
          ui::makeViewportIcon(ui::ViewportIcon::mapGridCenterSelection));
      viewportGridCenterOnSelectionAction_->setToolTip(
          "<b>Center grid on selection</b><br>Move the grid origin to the center of the selected objects or geometry.");
    }
    if (viewportGridClearCenterAction_) {
      viewportGridClearCenterAction_->setIcon(
          ui::makeViewportIcon(ui::ViewportIcon::mapGridClearCenter));
      viewportGridClearCenterAction_->setToolTip(
          "<b>Clear grid overrides</b><br>Clear temporary grid orientation and center changes.");
    }

    auto *gridMajorRow = new QWidget(viewportMapSettingsMenu);
    gridMajorRow->setToolTip(
        "<b>Major spacing</b><br>Set the distance between the emphasized main grid lines.");
    auto *gridMajorLayout = new QHBoxLayout(gridMajorRow);
    gridMajorLayout->setContentsMargins(10, 6, 10, 6);
    gridMajorLayout->setSpacing(8);

    auto *gridMajorLabel = new QLabel("Major", gridMajorRow);
    auto *gridMajorSpin = new QDoubleSpinBox(gridMajorRow);
    gridMajorLabel->setToolTip(gridMajorRow->toolTip());
    gridMajorSpin->setToolTip(gridMajorRow->toolTip());
    gridMajorSpin->setDecimals(6);
    gridMajorSpin->setRange(0.000001, 1000000.0);
    gridMajorSpin->setSingleStep(0.1);
    gridMajorSpin->setKeyboardTracking(false);
    gridMajorSpin->setFixedWidth(110);

    gridMajorLayout->addWidget(gridMajorLabel);
    gridMajorLayout->addStretch(1);
    gridMajorLayout->addWidget(gridMajorSpin);

    auto *gridMinorRow = new QWidget(viewportMapSettingsMenu);
    gridMinorRow->setToolTip(
        "<b>Minor spacing</b><br>Set the distance between minor grid lines and grid-snap points.");
    auto *gridMinorLayout = new QHBoxLayout(gridMinorRow);
    gridMinorLayout->setContentsMargins(10, 0, 10, 6);
    gridMinorLayout->setSpacing(8);

    auto *gridMinorLabel = new QLabel("Minor", gridMinorRow);
    auto *gridMinorSpin = new QDoubleSpinBox(gridMinorRow);
    gridMinorLabel->setToolTip(gridMinorRow->toolTip());
    gridMinorSpin->setToolTip(gridMinorRow->toolTip());
    gridMinorSpin->setDecimals(6);
    gridMinorSpin->setRange(0.000001, 1000000.0);
    gridMinorSpin->setSingleStep(0.01);
    gridMinorSpin->setKeyboardTracking(false);
    gridMinorSpin->setFixedWidth(110);

    gridMinorLayout->addWidget(gridMinorLabel);
    gridMinorLayout->addStretch(1);
    gridMinorLayout->addWidget(gridMinorSpin);

    QSettings initialGridAppearanceSettings = settingsStore();
    auto *gridMinorStyleRow = new QWidget(viewportMapSettingsMenu);
    gridMinorStyleRow->setToolTip(
        "<b>Minor grid style</b><br>Show minor grid lines or dots at each minor-grid intersection.");
    auto *gridMinorStyleLayout = new QHBoxLayout(gridMinorStyleRow);
    gridMinorStyleLayout->setContentsMargins(10, 0, 10, 6);
    gridMinorStyleLayout->setSpacing(8);
    auto *gridMinorStyleLabel = new QLabel("Minor grid", gridMinorStyleRow);
    auto *gridMinorStyleCombo = new QComboBox(gridMinorStyleRow);
    gridMinorStyleCombo->setObjectName("viewportMapGridMinorStyle");
    gridMinorStyleCombo->addItem("Lines", false);
    gridMinorStyleCombo->addItem("Points", true);
    gridMinorStyleCombo->setCurrentIndex(gridMinorStyleCombo->findData(
        initialGridAppearanceSettings.value(
            "viewport/grid_minor_dots", false).toBool()));
    gridMinorStyleCombo->setFixedWidth(110);
    gridMinorStyleLabel->setToolTip(gridMinorStyleRow->toolTip());
    gridMinorStyleCombo->setToolTip(gridMinorStyleRow->toolTip());
    gridMinorStyleLayout->addWidget(gridMinorStyleLabel);
    gridMinorStyleLayout->addStretch(1);
    gridMinorStyleLayout->addWidget(gridMinorStyleCombo);

    auto *gridMinorDotSizeRow = new QWidget(viewportMapSettingsMenu);
    gridMinorDotSizeRow->setToolTip(
        "<b>Dot size</b><br>Set the size of minor-grid dots on screen.");
    auto *gridMinorDotSizeLayout = new QHBoxLayout(gridMinorDotSizeRow);
    gridMinorDotSizeLayout->setContentsMargins(10, 0, 10, 6);
    gridMinorDotSizeLayout->setSpacing(8);
    auto *gridMinorDotSizeLabel = new QLabel("Dot size", gridMinorDotSizeRow);
    auto *gridMinorDotSizeSpin = new QDoubleSpinBox(gridMinorDotSizeRow);
    gridMinorDotSizeSpin->setObjectName("viewportMapGridMinorDotSize");
    gridMinorDotSizeSpin->setRange(
        render::kGridMinorDotSizeMinPx,
        render::kGridMinorDotSizeMaxPx);
    gridMinorDotSizeSpin->setDecimals(2);
    gridMinorDotSizeSpin->setSingleStep(0.25);
    gridMinorDotSizeSpin->setSuffix(" px");
    gridMinorDotSizeSpin->setKeyboardTracking(false);
    gridMinorDotSizeSpin->setFixedWidth(110);
    gridMinorDotSizeSpin->setValue(render::clampedGridMinorDotSizePx(
        static_cast<float>(initialGridAppearanceSettings.value(
            "viewport/grid_minor_dot_size_px", 2.0).toDouble())));
    gridMinorDotSizeSpin->setEnabled(
        gridMinorStyleCombo->currentData().toBool());
    gridMinorDotSizeLabel->setToolTip(gridMinorDotSizeRow->toolTip());
    gridMinorDotSizeSpin->setToolTip(gridMinorDotSizeRow->toolTip());
    gridMinorDotSizeLayout->addWidget(gridMinorDotSizeLabel);
    gridMinorDotSizeLayout->addStretch(1);
    gridMinorDotSizeLayout->addWidget(gridMinorDotSizeSpin);

    auto updateGridColorButton = [](QPushButton *button, const QColor &color) {
      if (!button) {
        return;
      }
      const QColor shown =
          color.isValid() ? color : QColor::fromRgb(112, 128, 156);
      const QString hex = shown.name(QColor::HexRgb);
      const QColor text =
          shown.lightnessF() < 0.52 ? QColor(Qt::white) : QColor(Qt::black);
      button->setText(hex);
      button->setStyleSheet(
          QString("QPushButton { background: %1; color: %2; border: 1px solid "
                  "rgba(255,255,255,70);"
                  " border-radius: 5px; padding: 3px 7px; }"
                  "QPushButton:hover { border-color: rgba(255,255,255,180); }")
              .arg(hex, text.name(QColor::HexRgb)));
    };

    auto *gridColorRow = new QWidget(viewportMapSettingsMenu);
    gridColorRow->setToolTip(
        "<b>Minor grid color</b><br>Choose the minor grid-line color. This does not change geometry edges or major grid lines.");
    auto *gridColorLayout = new QHBoxLayout(gridColorRow);
    gridColorLayout->setContentsMargins(10, 0, 10, 6);
    gridColorLayout->setSpacing(8);
    auto *gridColorLabel = new QLabel("Minor color", gridColorRow);
    auto *gridColorButton = new QPushButton(gridColorRow);
    gridColorButton->setObjectName("viewportMapGridColorButton");
    gridColorButton->setFixedWidth(110);
    gridColorButton->setCursor(Qt::PointingHandCursor);
    gridColorLabel->setToolTip(gridColorRow->toolTip());
    gridColorButton->setToolTip(gridColorRow->toolTip());
    updateGridColorButton(gridColorButton, QColor::fromRgb(112, 128, 156));
    gridColorLayout->addWidget(gridColorLabel);
    gridColorLayout->addStretch(1);
    gridColorLayout->addWidget(gridColorButton);
    auto *gridMajorColorRow = new QWidget(viewportMapSettingsMenu);
    gridMajorColorRow->setToolTip(
        "<b>Major grid color</b><br>Choose the emphasized main grid-line color independently of minor lines.");
    auto *gridMajorColorLayout = new QHBoxLayout(gridMajorColorRow);
    gridMajorColorLayout->setContentsMargins(10, 0, 10, 6);
    gridMajorColorLayout->setSpacing(8);
    auto *gridMajorColorLabel = new QLabel("Major color", gridMajorColorRow);
    auto *gridMajorColorButton = new QPushButton(gridMajorColorRow);
    gridMajorColorButton->setObjectName("viewportMapGridMajorColorButton");
    gridMajorColorButton->setFixedWidth(110);
    gridMajorColorButton->setCursor(Qt::PointingHandCursor);
    gridMajorColorLabel->setToolTip(gridMajorColorRow->toolTip());
    gridMajorColorButton->setToolTip(gridMajorColorRow->toolTip());
    updateGridColorButton(gridMajorColorButton, QColor::fromRgb(170, 184, 208));
    gridMajorColorLayout->addWidget(gridMajorColorLabel);
    gridMajorColorLayout->addStretch(1);
    gridMajorColorLayout->addWidget(gridMajorColorButton);
    auto *gridWidthRow = new QWidget(viewportMapSettingsMenu);
    gridWidthRow->setToolTip(
        "<b>Grid thickness</b><br>Set grid-line thickness on screen. Colored axes stay a readable width.");
    auto *gridWidthLayout = new QHBoxLayout(gridWidthRow);
    gridWidthLayout->setContentsMargins(10, 0, 10, 6);
    gridWidthLayout->setSpacing(8);
    auto *gridWidthLabel = new QLabel("Thickness", gridWidthRow);
    auto *gridWidthSpin = new QDoubleSpinBox(gridWidthRow);
    gridWidthSpin->setObjectName("viewportMapGridLineWidthSpin");
    gridWidthSpin->setRange(static_cast<double>(render::kGridLineWidthMinPx),
                            static_cast<double>(render::kGridLineWidthMaxPx));
    gridWidthSpin->setDecimals(2);
    gridWidthSpin->setSingleStep(0.25);
    gridWidthSpin->setSuffix(" px");
    gridWidthSpin->setKeyboardTracking(false);
    gridWidthSpin->setFixedWidth(110);
    gridWidthLabel->setToolTip(gridWidthRow->toolTip());
    gridWidthSpin->setToolTip(gridWidthRow->toolTip());
    gridWidthLayout->addWidget(gridWidthLabel);
    gridWidthLayout->addStretch(1);
    gridWidthLayout->addWidget(gridWidthSpin);
    auto *gridOpacityRow = new QWidget(viewportMapSettingsMenu);
    auto *gridOpacityLayout = new QHBoxLayout(gridOpacityRow);
    gridOpacityLayout->setContentsMargins(10, 0, 10, 6);
    gridOpacityLayout->setSpacing(8);
    gridOpacityRow->setToolTip(
        "<b>Grid opacity</b><br>Set grid transparency. Increase opacity when bright level textures make the grid hard to see.");

    auto *gridOpacityLabel = new QLabel("Opacity", gridOpacityRow);
    auto *gridOpacitySlider = new QSlider(Qt::Horizontal, gridOpacityRow);
    gridOpacitySlider->setObjectName("viewportMapGridOpacitySlider");
    gridOpacitySlider->setRange(0, 100);
    gridOpacitySlider->setSingleStep(5);
    gridOpacitySlider->setPageStep(10);
    gridOpacitySlider->setFixedWidth(110);
    auto *gridOpacityValue = new QLabel("70%", gridOpacityRow);
    gridOpacityValue->setObjectName("viewportMapGridOpacityValue");
    gridOpacityValue->setMinimumWidth(36);
    gridOpacityValue->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    gridOpacityLabel->setToolTip(gridOpacityRow->toolTip());
    gridOpacitySlider->setToolTip(gridOpacityRow->toolTip());
    gridOpacityValue->setToolTip(gridOpacityRow->toolTip());

    gridOpacityLayout->addWidget(gridOpacityLabel);
    gridOpacityLayout->addStretch(1);
    gridOpacityLayout->addWidget(gridOpacitySlider);
    gridOpacityLayout->addWidget(gridOpacityValue);

    const auto configureMapSettingsDisclosure =
        [](ui::CollapsibleSection *section,
                                  const char *sectionObjectName,
                                  const char *toggleObjectName,
                                  const QString &toolTip) {
          section->setObjectName(sectionObjectName);
          section->setExpandedImmediate(false);
          section->setSizePolicy(QSizePolicy::Expanding,
                                 QSizePolicy::Preferred);
          auto *toggle = section->findChild<QToolButton *>(
              "pref_section_header", Qt::FindDirectChildrenOnly);
          auto *content = section->findChild<QFrame *>(
              "pref_section_content", Qt::FindDirectChildrenOnly);
          Q_ASSERT(toggle);
          Q_ASSERT(content);
          toggle->setObjectName(toggleObjectName);
          toggle->setToolTip(toolTip);
          toggle->setCursor(Qt::PointingHandCursor);
          toggle->setStyleSheet(
              "QToolButton {"
              "  color: rgba(235, 236, 240, 245);"
              "  background: rgba(255, 255, 255, 8);"
              "  border: none;"
              "  border-top: 1px solid rgba(255, 255, 255, 24);"
              "  border-bottom: 1px solid rgba(255, 255, 255, 16);"
              "  padding: 5px 8px;"
              "  text-align: left;"
              "  font-weight: 600;"
              "}"
              "QToolButton:hover { background: rgba(255, 255, 255, 18); }");
          section->contentLayout()->setContentsMargins(8, 5, 8, 7);
          section->contentLayout()->setSpacing(2);
          return std::pair{toggle, content};
        };

    const auto makeActionButton = [](QAction *action, QWidget *parent,
                                     const bool showShortcut = false) {
      auto *button = new QToolButton(parent);
      button->setDefaultAction(action);
      button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
      button->setIconSize(QSize(16, 16));
      button->setAutoRaise(true);
      button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
      button->setMinimumHeight(24);
      button->setStyleSheet(
          "QToolButton { padding: 3px 5px; text-align: left; border-radius: "
          "4px; }"
          "QToolButton:hover { background: rgba(255,255,255,16); }"
          "QToolButton:checked { background: rgba(78,138,211,90); }");
      if (showShortcut && action) {
        const QString baseText = action->text().remove('&');
        const auto refreshText = [button, action, baseText]() {
          const QList<QKeySequence> shortcuts = action->shortcuts();
          const QString shortcut = shortcuts.empty()
              ? QString{}
              : ui::shortcuts::keySequenceDisplayText(shortcuts.front());
          button->setText(shortcut.isEmpty()
              ? baseText
              : QStringLiteral("%1    %2").arg(baseText, shortcut));
        };
        QObject::connect(action, &QAction::changed, button, refreshText);
        refreshText();
        button->setObjectName(action->objectName()
            + QStringLiteral(".map_settings_button"));
      }
      return button;
    };

    // Visibility overrides are grouped separately from grid behavior and
    // geometry controls, while keeping their original QAction connections.
    auto *gizmosSection =
        new ui::CollapsibleSection("Gizmos", viewportMapSettingsMenu);
    auto [gizmosToggle, gizmosContent] = configureMapSettingsDisclosure(
        gizmosSection, "viewport.map.gizmos_section",
        "viewport.map.gizmos_toggle",
        "<b>Gizmo options</b><br>Choose which Thing, light, Arch Thing, and Sector Thrust markers remain visible.");
    gizmosContent->setObjectName("viewport.map.gizmos_content");
    auto *gizmosBody = new QWidget(gizmosContent);
    gizmosBody->setObjectName("viewport.map.gizmos_body");
    auto *gizmosLayout = new QVBoxLayout(gizmosBody);
    gizmosLayout->setContentsMargins(0, 0, 0, 0);
    gizmosLayout->setSpacing(1);
    if (alwaysShowBakedLightGizmosAction_) {
      alwaysShowBakedLightGizmosAction_->setText(QStringLiteral("Baked Lights"));
      alwaysShowBakedLightGizmosAction_->setIcon(
          ui::makeViewportIcon(ui::ViewportIcon::lightBaked));
    }
    if (alwaysShowThingLightGizmosAction_) {
      alwaysShowThingLightGizmosAction_->setText(QStringLiteral("Thing Lights"));
      alwaysShowThingLightGizmosAction_->setIcon(
          ui::makeViewportIcon(ui::ViewportIcon::lightThing));
    }
    if (thingGizmosAction_) {
      thingGizmosAction_->setText(QStringLiteral("Thing Types"));
      thingGizmosAction_->setIcon(
          ui::makeViewportIcon(ui::ViewportIcon::thingGizmos));
    }
    if (alwaysShowSectorLightGizmosAction_) {
      alwaysShowSectorLightGizmosAction_->setText(QStringLiteral("Sector Lights"));
      alwaysShowSectorLightGizmosAction_->setIcon(
          ui::makeViewportIcon(ui::ViewportIcon::lightSector));
    }
    const auto addGizmoActionButton =
        [&](QAction *action, const char *objectName) -> QToolButton * {
          if (!action) {
            return nullptr;
          }
          auto *button = makeActionButton(action, gizmosBody);
          button->setObjectName(objectName);
          gizmosLayout->addWidget(button);
          return button;
        };
    addGizmoActionButton(
        alwaysShowBakedLightGizmosAction_,
        "viewport.map.gizmos.baked_button");
    addGizmoActionButton(
        alwaysShowThingLightGizmosAction_,
        "viewport.map.gizmos.thing_button");
    addGizmoActionButton(
        thingGizmosAction_,
        "viewport.map.gizmos.thing_types_button");
    QToolButton *sectorGizmoButton = addGizmoActionButton(
        alwaysShowSectorLightGizmosAction_,
        "viewport.map.gizmos.sector_button");
    QToolButton *archThingsGizmoButton = addGizmoActionButton(
        showArchThingGizmosAct,
        "viewport.map.gizmos.arch_thing_button");
    QToolButton *thrustGizmoButton = addGizmoActionButton(
        showSectorThrustVectorsAct,
        "viewport.map.gizmos.thrust_button");
    gizmosSection->contentLayout()->addWidget(gizmosBody);
    auto *gizmosAction = new QWidgetAction(viewportMapSettingsMenu);
    gizmosAction->setObjectName("viewport.map.gizmos");
    gizmosAction->setDefaultWidget(gizmosSection);

    // The common toggles belong to one compact behavior disclosure instead
    // of competing with the numeric controls in a long, flat menu.
    for (QAction *action : {followGridAct, rotateGridAct,
                            keepViewGridVisibleAct, adaptiveGridAct,
                            gridThroughAct, gridBehindAct,
                            resetGridOnRotateAct}) {
      viewportMapSettingsMenu->removeAction(action);
    }
    auto *gridBehaviorSection =
        new ui::CollapsibleSection("Grid Behavior", viewportMapSettingsMenu);
    auto [gridBehaviorToggle, gridBehaviorContent] = configureMapSettingsDisclosure(
        gridBehaviorSection, "viewport.map.grid_behavior_section",
        "viewport.map.grid_behavior_toggle",
        "<b>Grid behavior</b><br>Set how the grid follows the camera, appears through geometry, and changes while zooming.");
    gridBehaviorContent->setObjectName("viewport.map.grid_behavior_content");
    auto *gridBehaviorBody = new QWidget(gridBehaviorContent);
    auto *gridBehaviorLayout = new QVBoxLayout(gridBehaviorBody);
    gridBehaviorLayout->setContentsMargins(0, 0, 0, 0);
    gridBehaviorLayout->setSpacing(1);
    for (QAction *action : {followGridAct, rotateGridAct,
                            keepViewGridVisibleAct, adaptiveGridAct,
                            gridThroughAct, gridBehindAct,
                            resetGridOnRotateAct}) {
      gridBehaviorLayout->addWidget(makeActionButton(action, gridBehaviorBody));
    }
    gridBehaviorSection->contentLayout()->addWidget(gridBehaviorBody);
    auto *gridBehaviorAction = new QWidgetAction(viewportMapSettingsMenu);
    gridBehaviorAction->setObjectName("viewport.map.grid_behavior");
    gridBehaviorAction->setDefaultWidget(gridBehaviorSection);
    viewportMapSettingsMenu->addAction(gridBehaviorAction);
    viewportMapSettingsMenu->insertAction(gridBehaviorAction, gizmosAction);

    auto *gridAppearanceSection =
        new ui::CollapsibleSection("Grid Appearance", viewportMapSettingsMenu);
    auto [gridAppearanceToggle, gridAppearanceContent] =
        configureMapSettingsDisclosure(
            gridAppearanceSection, "viewport.map.grid_appearance_section",
            "viewport.map.grid_appearance_toggle",
            "<b>Grid appearance</b><br>Set grid spacing, style, colors, thickness, and transparency.");
    gridAppearanceContent->setObjectName(
        "viewport.map.grid_appearance_content");
    auto *gridAppearanceBody = new QWidget(gridAppearanceContent);
    auto *gridAppearanceLayout = new QVBoxLayout(gridAppearanceBody);
    gridAppearanceLayout->setContentsMargins(0, 0, 0, 0);
    gridAppearanceLayout->setSpacing(0);
    for ( QWidget *row : {gridMajorRow, gridMinorRow, gridMinorStyleRow,
                           gridMinorDotSizeRow, gridColorRow,
                           gridMajorColorRow, gridWidthRow, gridOpacityRow} )
    {
        gridAppearanceLayout->addWidget(row);
    }
    gridAppearanceSection->contentLayout()->addWidget(gridAppearanceBody);
    auto *gridAppearanceAction = new QWidgetAction(viewportMapSettingsMenu);
    gridAppearanceAction->setObjectName("viewport.map.grid_appearance");
    gridAppearanceAction->setDefaultWidget(gridAppearanceSection);
    viewportMapSettingsMenu->addAction(gridAppearanceAction);

    // A compact, state-backed disclosure keeps the common grid controls out
    // of the main action list while still exposing exact origin and
    // orientation values for precision work. Use the shared collapsible
    // section instead of making an ordinary checkable button impersonate a
    // section header. The values are editor state (not preferences), so
    // GlView's existing recovery snapshot carries them without a second
    // persistence path.
    auto *gridTransformSection =
        new ui::CollapsibleSection("Grid Transform", viewportMapSettingsMenu);
    auto [gridTransformToggle, gridTransformContent] = configureMapSettingsDisclosure(
        gridTransformSection, "viewport.map.grid_transform_section",
        "viewport.map.grid_transform_toggle",
        "<b>Grid transform</b><br>Set the grid origin and orientation.");
    gridTransformContent->setObjectName("viewport.map.grid_transform_content");

    auto *gridTransformBody = new QWidget(gridTransformContent);
    gridTransformBody->setObjectName("viewport.map.grid_transform_body");
    auto *gridTransformForm = new QFormLayout(gridTransformBody);
    gridTransformForm->setContentsMargins(0, 0, 0, 0);
    gridTransformForm->setSpacing(4);
    gridTransformSection->contentLayout()->addWidget(gridTransformBody);

    auto makeTransformSpin = [gridTransformBody](const char *objectName,
                                                     const QString& toolTip,
                                                     const double minimum,
                                                     const double maximum,
                                                     const double step)
    {
        auto *spin = new QDoubleSpinBox(gridTransformBody);
        spin->setObjectName(objectName);
        spin->setDecimals(3);
        spin->setRange(minimum, maximum);
        spin->setSingleStep(step);
        spin->setKeyboardTracking(false);
        spin->setFixedWidth(82);
        spin->setToolTip(toolTip);
        return spin;
    };
    auto *gridOriginX = makeTransformSpin(
        "viewport.map.grid_origin_x",
        "<b>Grid origin X</b><br>Set the grid origin's X coordinate.",
        -1.0e9, 1.0e9, 0.1);
    auto *gridOriginY = makeTransformSpin(
        "viewport.map.grid_origin_y",
        "<b>Grid origin Y</b><br>Set the grid origin's Y coordinate.",
        -1.0e9, 1.0e9, 0.1);
    auto *gridOriginZ = makeTransformSpin(
        "viewport.map.grid_origin_z",
        "<b>Grid origin Z</b><br>Set the grid origin's Z coordinate.",
        -1.0e9, 1.0e9, 0.1);
    auto *gridOriginRow = new QWidget(gridTransformBody);
    auto *gridOriginLayout = new QHBoxLayout(gridOriginRow);
    gridOriginLayout->setContentsMargins(0, 0, 0, 0);
    gridOriginLayout->setSpacing(8);
    const auto addTransformField = [](QHBoxLayout *rowLayout,
                                      QWidget *rowParent,
                                      const QString &labelText,
                                      QDoubleSpinBox *spin,
                                      const char *labelObjectName) {
      auto *pair = new QWidget(rowParent);
      auto *pairLayout = new QHBoxLayout(pair);
      pairLayout->setContentsMargins(0, 0, 0, 0);
      pairLayout->setSpacing(2);
      auto *label = new QLabel(labelText, pair);
      label->setObjectName(labelObjectName);
      label->setBuddy(spin);
      label->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
      label->setToolTip(spin->toolTip());
      pairLayout->addWidget(label);
      pairLayout->addWidget(spin);
      rowLayout->addWidget(pair);
    };
    addTransformField(gridOriginLayout, gridOriginRow, "X", gridOriginX,
                      "viewport.map.grid_origin_x_label");
    addTransformField(gridOriginLayout, gridOriginRow, "Y", gridOriginY,
                      "viewport.map.grid_origin_y_label");
    addTransformField(gridOriginLayout, gridOriginRow, "Z", gridOriginZ,
                      "viewport.map.grid_origin_z_label");
    gridTransformForm->addRow("Origin", gridOriginRow);

    auto *gridYaw = makeTransformSpin(
        "viewport.map.grid_angle_yaw",
        "<b>Grid yaw</b><br>Rotate the grid around the world Z axis, in degrees.",
        -180.0, 180.0, 1.0);
    auto *gridPitch = makeTransformSpin(
        "viewport.map.grid_angle_pitch",
        "<b>Grid pitch</b><br>Rotate the grid around the world Y axis, in degrees.",
        -90.0, 90.0, 1.0);
    auto *gridRoll = makeTransformSpin(
        "viewport.map.grid_angle_roll",
        "<b>Grid roll</b><br>Rotate the grid around its X axis, in degrees.",
        -180.0, 180.0, 1.0);
    auto *gridAnglesRow = new QWidget(gridTransformBody);
    auto *gridAnglesLayout = new QHBoxLayout(gridAnglesRow);
    gridAnglesLayout->setContentsMargins(0, 0, 0, 0);
    gridAnglesLayout->setSpacing(8);
    addTransformField(gridAnglesLayout, gridAnglesRow, "Pitch", gridPitch,
                      "viewport.map.grid_angle_pitch_label");
    addTransformField(gridAnglesLayout, gridAnglesRow, "Yaw", gridYaw,
                      "viewport.map.grid_angle_yaw_label");
    addTransformField(gridAnglesLayout, gridAnglesRow, "Roll", gridRoll,
                      "viewport.map.grid_angle_roll_label");
    gridTransformForm->addRow("Orientation", gridAnglesRow);

    auto *gridQuickRow = new QWidget(gridTransformBody);
    auto *gridQuickLayout = new QHBoxLayout(gridQuickRow);
    gridQuickLayout->setContentsMargins(0, 0, 0, 0);
    gridQuickLayout->setSpacing(4);
    auto *gridQuickXY = new QPushButton("XY", gridQuickRow);
    auto *gridQuickXZ = new QPushButton("XZ", gridQuickRow);
    auto *gridQuickYZ = new QPushButton("YZ", gridQuickRow);
    auto *gridQuickReset = new QPushButton("Reset", gridQuickRow);
    gridQuickXY->setObjectName("viewport.map.grid_quick_xy");
    gridQuickXZ->setObjectName("viewport.map.grid_quick_xz");
    gridQuickYZ->setObjectName("viewport.map.grid_quick_yz");
    gridQuickReset->setObjectName("viewport.map.grid_transform_reset");
    gridQuickXY->setToolTip(
        "<b>XY plane</b><br>Align the grid with the world XY plane.");
    gridQuickXZ->setToolTip(
        "<b>XZ plane</b><br>Align the grid with the world XZ plane.");
    gridQuickYZ->setToolTip(
        "<b>YZ plane</b><br>Align the grid with the world YZ plane.");
    gridQuickReset->setToolTip(
        "<b>Reset grid</b><br>Return the grid to XY orientation and the 0,0,0 origin.");
    for (QPushButton *button : {gridQuickXY, gridQuickXZ, gridQuickYZ, gridQuickReset})
    {
        button->setAutoDefault(false);
        button->setFixedHeight(22);
        gridQuickLayout->addWidget(button);
    }
    gridTransformForm->addRow("Quick plane", gridQuickRow);

    auto *gridCommandBody = new QWidget(gridTransformBody);
    auto *gridCommandLayout = new QVBoxLayout(gridCommandBody);
    gridCommandLayout->setContentsMargins(0, 2, 0, 0);
    gridCommandLayout->setSpacing(1);
    for (QAction *action :
         {viewportGridAlignToViewAction_, viewportGridOrientToObjectAction_,
          viewportGridOrientToSelectionAction_,
          viewportGridCenterOnSelectionAction_}) {
      if (action) {
        gridCommandLayout->addWidget(
            makeActionButton(action, gridCommandBody, true));
      }
    }
    gridTransformForm->addRow("Commands", gridCommandBody);

    auto *gridTransformAction = new QWidgetAction(viewportMapSettingsMenu);
    gridTransformAction->setObjectName("viewport.map.grid_transform");
    gridTransformAction->setDefaultWidget(gridTransformSection);
    gridTransformAction->setToolTip(
        "<b>Grid transform</b><br>Set the grid origin and orientation, choose a plane, or reset the grid.");
    viewportMapSettingsMenu->addAction(gridTransformAction);

    // An invisible stable anchor lets each QWidgetAction be removed and
    // reinserted at its original position when QMenu must recalculate the
    // disclosure height. It avoids a redundant trailing separator row.
    auto *gridDisclosureAnchor = new QAction(viewportMapSettingsMenu);
    gridDisclosureAnchor->setObjectName("viewport.map.grid_disclosure_anchor");
    gridDisclosureAnchor->setVisible(false);
    viewportMapSettingsMenu->addAction(gridDisclosureAnchor);

    const auto connectGridDisclosure =
        [this, gridDisclosureAnchor](QToolButton *toggle,
                                     ui::CollapsibleSection *section,
                                     QWidgetAction *action,
                                     QAction *followingAction) {
          connect(toggle, &QToolButton::toggled, this,
                  [this, section, action, followingAction,
                   gridDisclosureAnchor](const bool expanded) {
              // QWidgetAction caches the old action rectangle while a child
              // animates. Resolve this compact disclosure immediately and
              // reinsert the same action before its stable following action.
              // QMenu recalculates QWidgetAction rectangles only
              // when its action list changes; layout invalidation alone leaves
              // the old collapsed rectangle and clips/overlaps the fields.
              section->setExpandedImmediate(expanded);
              if (section->layout()) {
                section->layout()->invalidate();
                section->layout()->activate();
              }
              section->adjustSize();
              section->updateGeometry();
              if (viewportMapSettingsMenu_) {
                viewportMapSettingsMenu_->removeAction(action);
                viewportMapSettingsMenu_->insertAction(
                    followingAction ? followingAction : gridDisclosureAnchor,
                    action);
                if (viewportMapSettingsMenu_->layout()) {
                  viewportMapSettingsMenu_->layout()->invalidate();
                  viewportMapSettingsMenu_->layout()->activate();
                }
                viewportMapSettingsMenu_->updateGeometry();
              }
              positionViewportMapSettingsPanel();
              if (expanded && viewportMapSettingsScrollArea_) {
                viewportMapSettingsScrollArea_->ensureWidgetVisible(
                    section, 0, 6);
              }
            });
        };
    connectGridDisclosure(gizmosToggle, gizmosSection,
                          gizmosAction, gridBehaviorAction);
    connectGridDisclosure(gridBehaviorToggle, gridBehaviorSection,
                          gridBehaviorAction, gridAppearanceAction);
    connectGridDisclosure(gridAppearanceToggle, gridAppearanceSection,
                          gridAppearanceAction, gridTransformAction);
    connectGridDisclosure(gridTransformToggle, gridTransformSection,
                          gridTransformAction, gridDisclosureAnchor);

    // Reserve the width needed by the largest expanded grid disclosure now,
    // so opening it changes only panel height and never makes the floating
    // Viewport Settings panel jump sideways or resize under the cursor.
    {
      const QSignalBlocker gizmosBlocker(gizmosToggle);
      const QSignalBlocker behaviorBlocker(gridBehaviorToggle);
      const QSignalBlocker appearanceBlocker(gridAppearanceToggle);
      const QSignalBlocker transformBlocker(gridTransformToggle);
      for (ui::CollapsibleSection *section : {gizmosSection, gridBehaviorSection,
                                               gridAppearanceSection,
                                               gridTransformSection}) {
        section->setExpandedImmediate(true);
        section->adjustSize();
      }
      if (viewportMapSettingsMenu->layout()) {
        viewportMapSettingsMenu->layout()->invalidate();
        viewportMapSettingsMenu->layout()->activate();
      }
      int stableNaturalWidth = viewportMapSettingsMenu->sizeHint().width();
      for (ui::CollapsibleSection *section : {gizmosSection, gridBehaviorSection,
                                               gridAppearanceSection,
                                               gridTransformSection}) {
        // QMenu styles reserve an icon/check gutter outside QWidgetAction's
        // own hint. Include it explicitly even if this platform still has a
        // cached collapsed action rectangle during construction.
        stableNaturalWidth = std::max(
            stableNaturalWidth, section->sizeHint().width() + 24);
      }
      viewportMapSettingsMenu->setProperty("sed.stableNaturalWidth",
                                           stableNaturalWidth);
      for (ui::CollapsibleSection *section : {gizmosSection, gridBehaviorSection,
                                               gridAppearanceSection,
                                               gridTransformSection}) {
        section->setExpandedImmediate(false);
      }
    }

    auto applyAndStore = [this](const char *key, const QVariant &value,
                                const std::function<void()> &apply) {
      QSettings settings = settingsStore();
      settings.setValue(key, value);
      if (apply) {
        apply();
      }
    };

    const QList<QAction *> lightAndThrustActions{
        alwaysShowBakedLightGizmosAction_,
        alwaysShowThingLightGizmosAction_,
        alwaysShowSectorLightGizmosAction_,
        showArchThingGizmosAct,
        showSectorThrustVectorsAct,
    };
    const auto mapRuntimeOptionSyncing = std::make_shared<bool>(false);
    // Light-family visibility is part of the serialized viewport workspace.
    // Keep the active pipeline in this small shared state so ordinary
    // viewport refreshes and reopening Viewport Settings mirror GlView instead of
    // re-reading machine preferences. Preferences are applied only when the
    // pipeline actually changes (or during initial setup); recovery records
    // therefore remain authoritative after their restore signal returns.
    const auto mapRuntimeOptionPipeline =
        std::make_shared<std::optional<GlView::RenderPipeline>>();
    const auto syncMapRuntimeOptions = [this, showSectorThrustVectorsAct,
                                        showArchThingGizmosAct,
                                        mapRuntimeOptionSyncing,
                                        mapRuntimeOptionPipeline]() {
      if (!glView_ || *mapRuntimeOptionSyncing) {
        return;
      }

      const GlView::RenderPipeline pipeline = glView_->renderPipeline();
      const bool pipelineChanged = !mapRuntimeOptionPipeline->has_value()
          || mapRuntimeOptionPipeline->value() != pipeline;
      // Record the pipeline even while recovery suppresses preference
      // application. Otherwise the first menu refresh after recovery would
      // look like a user pipeline transition and replace the recovered
      // categories with machine settings.
      *mapRuntimeOptionPipeline = pipeline;
      if (recoveryViewportStateRestoreInProgress_) {
        return;
      }

      *mapRuntimeOptionSyncing = true;
      const std::array<std::pair<QAction *, render::LightGizmoCategory>, 4>
          lightOptions{
              std::pair{alwaysShowBakedLightGizmosAction_,
                        render::LightGizmoCategory::Baked},
              std::pair{alwaysShowThingLightGizmosAction_,
                        render::LightGizmoCategory::Thing},
              std::pair{alwaysShowSectorLightGizmosAction_,
                        render::LightGizmoCategory::Sector},
              std::pair{alwaysShowArchitecturalLightGizmosAction_,
                        render::LightGizmoCategory::Architectural},
          };
      if (pipelineChanged) {
        const bool preview = pipeline == GlView::RenderPipeline::GamePreview;
        const char *settingProperty =
            preview ? "sed.previewSettingKey" : "sed.editorSettingKey";
        QSettings settings = settingsStore();
        for (const auto &[action, category] : lightOptions) {
          if (!action) {
            continue;
          }
          const QString key = action->property(settingProperty).toString();
          const bool editorDefault =
              category == render::LightGizmoCategory::Architectural;
          const bool checked =
              settings.value(key, !preview && editorDefault).toBool();
          const QSignalBlocker blocker(action);
          action->setChecked(checked);
          glView_->setAlwaysShowLightGizmoCategory(category, checked);
        }
      } else {
        // The serialized GlView state is authoritative between pipeline
        // transitions, including after recovery and during menu reopening.
        for (const auto &[action, category] : lightOptions) {
          if (!action) {
            continue;
          }
          const QSignalBlocker blocker(action);
          action->setChecked(glView_->alwaysShowLightGizmoCategory(category));
        }
      }

      if (showArchThingGizmosAct) {
        // GlView owns this option in the active Editor/Preview viewport state.
        // Preferences seed each pipeline's default; switching pipelines and
        // recovery restore the saved state, so the QAction only mirrors it.
        const QSignalBlocker blocker(showArchThingGizmosAct);
        showArchThingGizmosAct->setChecked(
            glView_->showArchThingGizmos());
      }

      if (showSectorThrustVectorsAct) {
        bool checked = glView_->showSectorThrustVectors();
        if (pipelineChanged) {
          const bool preview = pipeline == GlView::RenderPipeline::GamePreview;
          const char *settingProperty =
              preview ? "sed.previewSettingKey" : "sed.editorSettingKey";
          const QString key = showSectorThrustVectorsAct
              ->property(settingProperty).toString();
          checked = settingsStore().value(key, !preview).toBool();
        }
        const QSignalBlocker blocker(showSectorThrustVectorsAct);
        showSectorThrustVectorsAct->setChecked(checked);
        if (pipelineChanged) {
          glView_->setShowSectorThrustVectors(checked);
        }
      }
      *mapRuntimeOptionSyncing = false;
    };
    if (glView_) {
      connect(glView_, &GlView::viewportStateChanged, viewportMapSettingsMenu,
              syncMapRuntimeOptions);
    }
    syncMapRuntimeOptions();
    QList<QAction *> editorGridActions{
        showGridAct,
        followGridAct,
        rotateGridAct,
        adaptiveGridAct,
        resetGridOnRotateAct,
        gridBehaviorAction,
        gridAppearanceAction,
        gridTransformAction,
    };
    for (QAction *action :
         {viewportGridAlignToViewAction_, viewportGridOrientToObjectAction_,
          viewportGridOrientToSelectionAction_,
          viewportGridCenterOnSelectionAction_,
          viewportGridClearCenterAction_}) {
      if (action) {
        editorGridActions.push_back(action);
      }
    }

    const auto refreshMapSettingsPanel = [this, showGridAct, renderFogAct,
                                          showArchThingGizmosAct,
                                          showSectorThrustVectorsAct,
                                          sectorGizmoButton,
                                          archThingsGizmoButton,
                                          thrustGizmoButton,
                                          xraySelectionAct,
                                          orbitAroundSelectionAct,
                                          followGridAct, rotateGridAct,
                                          keepViewGridVisibleAct,
                                          adaptiveGridAct, gridThroughAct,
                                          gridBehindAct, resetGridOnRotateAct,
                                          gridMajorSpin, gridMinorSpin,
                                          gridMinorStyleCombo,
                                          gridMinorDotSizeSpin,
                                          gridColorButton, gridMajorColorButton,
                                          gridWidthSpin, gridOpacitySlider,
                                          gridOpacityValue,
                                          gridBehaviorSection,
                                          gridAppearanceSection,
                                          gridTransformSection,
                                          gridOriginX, gridOriginY, gridOriginZ,
                                          gridYaw, gridPitch, gridRoll,
                                          updateGridColorButton,
                                          lightAndThrustActions,
                                          editorGridActions,
                                          syncMapRuntimeOptions]() {
      if (!glView_) {
        return;
      }

      const QSignalBlocker b0(showGridAct);
      const QSignalBlocker b1(renderFogAct);
      const QSignalBlocker bXray(xraySelectionAct);
      const QSignalBlocker bOrbitAroundSelection(orbitAroundSelectionAct);
      const QSignalBlocker b2(followGridAct);
      const QSignalBlocker b3(rotateGridAct);
      const QSignalBlocker bKeepVisible(keepViewGridVisibleAct);
      const QSignalBlocker b4(adaptiveGridAct);
      const QSignalBlocker b5(gridThroughAct);
      const QSignalBlocker b6(gridBehindAct);
      const QSignalBlocker b7(resetGridOnRotateAct);
      const QSignalBlocker b8(gridMajorSpin);
      const QSignalBlocker b9(gridMinorSpin);
      const QSignalBlocker bMinorStyle(gridMinorStyleCombo);
      const QSignalBlocker bMinorDotSize(gridMinorDotSizeSpin);
      const QSignalBlocker b10(gridWidthSpin);
      const QSignalBlocker b11(gridOpacitySlider);
      const QSignalBlocker b12(gridOriginX);
      const QSignalBlocker b13(gridOriginY);
      const QSignalBlocker b14(gridOriginZ);
      const QSignalBlocker b15(gridYaw);
      const QSignalBlocker b16(gridPitch);
      const QSignalBlocker b17(gridRoll);

      const bool previewMode =
          glView_->renderPipeline() == GlView::RenderPipeline::GamePreview;
      const bool fogRenderAvailable =
          ui::viewport::viewportFogRenderOptionAvailable(
              document_.level().kind);

      syncMapRuntimeOptions();

      const ProjectType projectType = document_.level().kind;
      const bool classicProject = projectType == ProjectType::JKDF2
          || projectType == ProjectType::MOTS;
      const bool ijimProject = projectType == ProjectType::IJIM;
      if (showArchThingGizmosAct)
      {
        showArchThingGizmosAct->setText(QStringLiteral("Arch Things"));
        showArchThingGizmosAct->setToolTip(
            QStringLiteral("<b>Arch Thing markers</b><br>Show markers for Things that use the classic Arch flag."));
      }
      for (QAction *action : lightAndThrustActions) {
        if (!action) {
          continue;
        }
        const bool ijimOnly = action == alwaysShowSectorLightGizmosAction_
            || action == showSectorThrustVectorsAct;
        const bool archThingOnly = action == showArchThingGizmosAct;
        const bool available = archThingOnly ? classicProject
            : ijimOnly ? ijimProject : true;
        action->setVisible(available);
        action->setEnabled(available);
        if (action == alwaysShowSectorLightGizmosAction_ && sectorGizmoButton) {
          sectorGizmoButton->setVisible(available);
          sectorGizmoButton->setEnabled(available);
        } else if (action == showArchThingGizmosAct
                   && archThingsGizmoButton) {
          archThingsGizmoButton->setVisible(available);
          archThingsGizmoButton->setEnabled(available);
        } else if (action == showSectorThrustVectorsAct && thrustGizmoButton) {
          thrustGizmoButton->setVisible(available);
          thrustGizmoButton->setEnabled(available);
        }
      }
      for (QAction *action : editorGridActions) {
        if (action) {
          action->setVisible(!previewMode);
          action->setEnabled(!previewMode);
        }
      }

      showGridAct->setChecked(glView_->showGrid());
      renderFogAct->setVisible(fogRenderAvailable);
      renderFogAct->setEnabled(fogRenderAvailable);
      renderFogAct->setChecked(glView_->showFog());
      xraySelectionAct->setChecked(glView_->xraySelectionCyclingEnabled());
      orbitAroundSelectionAct->setChecked(
          glView_->orbitAroundSelectionEnabled());
      followGridAct->setChecked(glView_->gridFollowCamera());
      rotateGridAct->setChecked(glView_->gridRotateWithView());
      keepViewGridVisibleAct->setChecked(
          glView_->gridKeepViewAlignedVisible());
      keepViewGridVisibleAct->setEnabled(
          !previewMode && glView_->gridRotateWithView());
      adaptiveGridAct->setChecked(glView_->gridAdaptiveSpacing());
      gridThroughAct->setChecked(glView_->gridVisibleThroughWalls());
      gridBehindAct->setChecked(glView_->gridAlwaysBehind());
      resetGridOnRotateAct->setChecked(glView_->gridResetOnCameraRotate());
      gridMajorSpin->setValue(static_cast<double>(glView_->gridMajorSpacing()));
      gridMinorSpin->setValue(static_cast<double>(glView_->gridMinorSpacing()));
      gridMinorStyleCombo->setCurrentIndex(
          gridMinorStyleCombo->findData(glView_->gridMinorDots()));
      gridMinorDotSizeSpin->setValue(
          static_cast<double>(glView_->gridMinorDotSizePx()));
      gridMinorDotSizeSpin->setEnabled(glView_->gridMinorDots());
      updateGridColorButton(gridColorButton, glView_->gridColor());
      updateGridColorButton(gridMajorColorButton, glView_->gridMajorColor());
      gridWidthSpin->setValue(static_cast<double>(glView_->gridLineWidthPx()));
      const int opacityPercent = qRound(
          std::clamp(static_cast<double>(glView_->gridOpacity()), 0.0, 1.0) *
          100.0);
      gridOpacitySlider->setValue(opacityPercent);
      gridOpacityValue->setText(QString("%1%").arg(opacityPercent));
      const QVector3D gridOrigin = glView_->gridTransformOrigin();
      const render::GridTransformAngles gridAngles =
          glView_->gridTransformAngles();
      gridOriginX->setValue(static_cast<double>(gridOrigin.x()));
      gridOriginY->setValue(static_cast<double>(gridOrigin.y()));
      gridOriginZ->setValue(static_cast<double>(gridOrigin.z()));
      gridYaw->setValue(static_cast<double>(gridAngles.yawDeg));
      gridPitch->setValue(static_cast<double>(gridAngles.pitchDeg));
      gridRoll->setValue(static_cast<double>(gridAngles.rollDeg));
      gridTransformSection->setEnabled(!previewMode);
      gridBehaviorSection->setEnabled(!previewMode);
      gridAppearanceSection->setEnabled(!previewMode);
      if (viewportGridClearCenterAction_) {
        viewportGridClearCenterAction_->setEnabled(
            !previewMode && glView_->gridOverridesActive());
      }
      if (viewportMapSettingsScrollArea_ &&
          !viewportMapSettingsScrollArea_->isHidden()) {
        positionViewportMapSettingsPanel();
      }
    };
    connect(viewportMapSettingsMenu, &QMenu::aboutToShow, this,
            refreshMapSettingsPanel);
    if (glView_) {
      connect(glView_, &GlView::viewportStateChanged, viewportMapSettingsMenu,
              refreshMapSettingsPanel);
    }

    connect(showGridAct, &QAction::toggled, this,
            [this, applyAndStore](const bool checked) {
              if (!glView_) {
                return;
              }
              applyAndStore("viewport/show_grid", checked, [this, checked]() {
                if (glView_) {
                  glView_->setShowGrid(checked);
                }
              });
            });

    connect(renderFogAct, &QAction::toggled, this,
            [this, applyAndStore](const bool checked) {
              if (!glView_) {
                return;
              }
              if (!ui::viewport::viewportFogRenderOptionAvailable(
                      document_.level().kind)) {
                return;
              }

              const bool previewMode = glView_->renderPipeline() ==
                                       GlView::RenderPipeline::GamePreview;
              applyAndStore(previewMode ? "viewport/preview_render_fog"
                                        : "viewport/editor_render_fog",
                            checked, [this, checked, previewMode]() {
                              if (glView_) {
                                glView_->setDefaultShowFog(
                                    previewMode
                                        ? GlView::RenderPipeline::GamePreview
                                        : GlView::RenderPipeline::Editor,
                                    checked);
                              }
                            });
            });

    connect(xraySelectionAct, &QAction::toggled, this,
            [this, applyAndStore](const bool checked) {
              if (!glView_) {
                return;
              }
              applyAndStore("viewport/xray_selection_cycling", checked,
                            [this, checked]() {
                              if (glView_) {
                                glView_->setXraySelectionCyclingEnabled(
                                    checked);
                              }
                            });
            });

    connect(orbitAroundSelectionAct, &QAction::toggled, this,
            [this, applyAndStore](const bool checked) {
              if (!glView_) {
                return;
              }
              applyAndStore("viewport/orbit_around_selection", checked,
                            [this, checked]() {
                              if (glView_) {
                                glView_->setOrbitAroundSelectionEnabled(
                                    checked);
                              }
                            });
            });

    connect(followGridAct, &QAction::toggled, this,
            [this, applyAndStore](const bool checked) {
              if (!glView_) {
                return;
              }
              applyAndStore("viewport/grid_follow_camera", checked,
                            [this, checked]() {
                              if (glView_) {
                                glView_->setGridFollowCamera(checked);
                              }
                            });
            });

    connect(rotateGridAct, &QAction::toggled, this,
            [this, keepViewGridVisibleAct, applyAndStore](const bool checked) {
              if (!glView_) {
                return;
              }
              applyAndStore("viewport/grid_rotate_with_view", checked,
                            [this, checked]() {
                              if (glView_) {
                                glView_->setGridRotateWithView(checked);
                              }
                            });
              keepViewGridVisibleAct->setEnabled(checked);
            });

    connect(keepViewGridVisibleAct, &QAction::toggled, this,
            [this, applyAndStore](const bool checked) {
              if (!glView_) {
                return;
              }
              applyAndStore(
                  "viewport/grid_keep_view_aligned_visible", checked,
                  [this, checked]() {
                    if (glView_) {
                      glView_->setGridKeepViewAlignedVisible(checked);
                    }
                  });
            });

    connect(adaptiveGridAct, &QAction::toggled, this,
            [this, applyAndStore](const bool checked) {
              if (!glView_) {
                return;
              }
              applyAndStore("viewport/grid_adaptive_spacing", checked,
                            [this, checked]() {
                              if (glView_) {
                                glView_->setGridAdaptiveSpacing(checked);
                              }
                            });
            });

    connect(gridThroughAct, &QAction::toggled, this,
            [this, gridBehindAct, applyAndStore](const bool checked) {
              if (!glView_) {
                return;
              }

              applyAndStore(
                  "viewport/grid_visible_through_walls", checked,
                  [this, gridBehindAct, checked]() {
                    if (checked) {
                      QSettings settings = settingsStore();
                      settings.setValue("viewport/grid_always_behind", false);
                      const QSignalBlocker menuBlocker(gridBehindAct);
                      gridBehindAct->setChecked(false);
                      if (viewportGridAlwaysBehindAction_) {
                        const QSignalBlocker actionBlocker(
                            viewportGridAlwaysBehindAction_);
                        viewportGridAlwaysBehindAction_->setChecked(false);
                      }
                    }
                    if (glView_) {
                      glView_->setGridVisibleThroughWalls(checked);
                    }
                    if (viewportGridVisibleThroughWallsAction_) {
                      const QSignalBlocker actionBlocker(
                          viewportGridVisibleThroughWallsAction_);
                      viewportGridVisibleThroughWallsAction_->setChecked(
                          checked);
                    }
                  });
            });

    connect(
        gridBehindAct, &QAction::toggled, this,
        [this, gridThroughAct, applyAndStore](const bool checked) {
          if (!glView_) {
            return;
          }

          applyAndStore(
              "viewport/grid_always_behind", checked,
              [this, gridThroughAct, checked]() {
                if (checked) {
                  QSettings settings = settingsStore();
                  settings.setValue("viewport/grid_visible_through_walls",
                                    false);
                  const QSignalBlocker menuBlocker(gridThroughAct);
                  gridThroughAct->setChecked(false);
                  if (viewportGridVisibleThroughWallsAction_) {
                    const QSignalBlocker actionBlocker(
                        viewportGridVisibleThroughWallsAction_);
                    viewportGridVisibleThroughWallsAction_->setChecked(false);
                  }
                }
                if (glView_) {
                  glView_->setGridAlwaysBehind(checked);
                }
                if (viewportGridAlwaysBehindAction_) {
                  const QSignalBlocker blocker(viewportGridAlwaysBehindAction_);
                  viewportGridAlwaysBehindAction_->setChecked(checked);
                }
              });
        });

    connect(resetGridOnRotateAct, &QAction::toggled, this,
            [this, applyAndStore](const bool checked) {
              if (!glView_) {
                return;
              }
              applyAndStore("viewport/grid_reset_on_rotate", checked,
                            [this, checked]() {
                              if (glView_) {
                                glView_->setGridResetOnCameraRotate(checked);
                              }
                            });
            });

    connect(gridMajorSpin, qOverload<double>(&QDoubleSpinBox::valueChanged),
            this, [this, applyAndStore](const double v) {
              if (!glView_) {
                return;
              }
              applyAndStore("viewport/grid_spacing", v, [this, v]() {
                if (glView_) {
                  glView_->setGridMajorSpacing(static_cast<float>(v));
                }
              });
            });

    connect(gridMinorSpin, qOverload<double>(&QDoubleSpinBox::valueChanged),
            this, [this, applyAndStore](const double v) {
              if (!glView_) {
                return;
              }
              applyAndStore("viewport/grid_minor_spacing", v, [this, v]() {
                if (glView_) {
                  glView_->setGridMinorSpacing(static_cast<float>(v));
                }
              });
            });

    connect(
        gridMinorStyleCombo,
        qOverload<int>(&QComboBox::currentIndexChanged),
        this,
        [this, applyAndStore, gridMinorStyleCombo,
         gridMinorDotSizeSpin](const int)
        {
            const bool dots = gridMinorStyleCombo->currentData().toBool();
            gridMinorDotSizeSpin->setEnabled(dots);
            if ( !glView_ )
            {
                return;
            }
            applyAndStore(
                "viewport/grid_minor_dots",
                dots,
                [this, dots]()
                {
                    if ( glView_ )
                    {
                        glView_->setGridMinorDots(dots);
                    }
                });
        });

    connect(
        gridMinorDotSizeSpin,
        qOverload<double>(&QDoubleSpinBox::valueChanged),
        this,
        [this, applyAndStore](const double value)
        {
            if ( !glView_ )
            {
                return;
            }
            const float sizePx = render::clampedGridMinorDotSizePx(
                static_cast<float>(value));
            applyAndStore(
                "viewport/grid_minor_dot_size_px",
                static_cast<double>(sizePx),
                [this, sizePx]()
                {
                    if ( glView_ )
                    {
                        glView_->setGridMinorDotSizePx(sizePx);
                    }
                });
        });

    connect(gridColorButton, &QPushButton::clicked, this,
            [this, gridColorButton, applyAndStore, updateGridColorButton]() {
              if (!glView_) {
                return;
              }
              const QColor chosen = QColorDialog::getColor(
                  glView_->gridColor(), this, "Select minor grid-line color");
              if (!chosen.isValid()) {
                return;
              }

              updateGridColorButton(gridColorButton, chosen);
              applyAndStore("viewport/colors/grid", chosen.name(QColor::HexRgb),
                            [this, chosen]() {
                              if (glView_) {
                                glView_->setGridColor(chosen);
                              }
                            });
            });

    connect(
        gridMajorColorButton, &QPushButton::clicked, this,
        [this, gridMajorColorButton, applyAndStore, updateGridColorButton]() {
          if (!glView_) {
            return;
          }
          const QColor chosen = QColorDialog::getColor(
              glView_->gridMajorColor(), this, "Select major grid-line color");
          if (!chosen.isValid()) {
            return;
          }

          updateGridColorButton(gridMajorColorButton, chosen);
          applyAndStore("viewport/colors/grid_major",
                        chosen.name(QColor::HexRgb), [this, chosen]() {
                          if (glView_) {
                            glView_->setGridMajorColor(chosen);
                          }
                        });
        });

    connect(gridWidthSpin, qOverload<double>(&QDoubleSpinBox::valueChanged),
            this, [this, applyAndStore](const double value) {
              const float width =
                  render::clampedGridLineWidthPx(static_cast<float>(value));
              applyAndStore("viewport/grid_line_width_px",
                            static_cast<double>(width), [this, width]() {
                              if (glView_) {
                                glView_->setGridLineWidthPx(width);
                              }
                            });
            });

    connect(gridOpacitySlider, &QSlider::valueChanged, this,
            [this, gridOpacityValue, applyAndStore](const int value) {
              const int bounded = std::clamp(value, 0, 100);
              gridOpacityValue->setText(QString("%1%").arg(bounded));
              const float opacity = static_cast<float>(bounded) / 100.0f;
              applyAndStore("viewport/grid_opacity",
                            static_cast<double>(opacity), [this, opacity]() {
                              if (glView_) {
                                glView_->setGridOpacity(opacity);
                              }
                            });
            });

    auto readGridTransformOrigin = [gridOriginX, gridOriginY, gridOriginZ]()
    {
      return QVector3D(
          static_cast<float>(gridOriginX->value()),
          static_cast<float>(gridOriginY->value()),
          static_cast<float>(gridOriginZ->value()));
    };
    auto readGridTransformAngles = [gridYaw, gridPitch, gridRoll]()
    {
      return render::GridTransformAngles{
          static_cast<float>(gridYaw->value()),
          static_cast<float>(gridPitch->value()),
          static_cast<float>(gridRoll->value())};
    };
    auto applyGridTransformOrigin = [this, readGridTransformOrigin]()
    {
      if (glView_)
      {
        glView_->setGridTransformOrigin(readGridTransformOrigin());
      }
    };
    auto applyGridTransformAngles = [this, readGridTransformAngles]()
    {
      if (glView_)
      {
        glView_->setGridTransformAngles(readGridTransformAngles());
      }
    };
    for (QDoubleSpinBox *spin : {gridOriginX, gridOriginY, gridOriginZ})
    {
      connect(spin, qOverload<double>(&QDoubleSpinBox::valueChanged), this,
              [applyGridTransformOrigin](const double)
              {
                applyGridTransformOrigin();
              });
    }
    for (QDoubleSpinBox *spin : {gridYaw, gridPitch, gridRoll})
    {
      connect(spin, qOverload<double>(&QDoubleSpinBox::valueChanged), this,
              [applyGridTransformAngles](const double)
              {
                applyGridTransformAngles();
              });
    }
    connect(gridQuickXY, &QPushButton::clicked, this,
            [this]
            {
              if (glView_)
              {
                glView_->setGridTransformPlane(GlView::GridPlane::XY);
              }
            });
    connect(gridQuickXZ, &QPushButton::clicked, this,
            [this]
            {
              if (glView_)
              {
                glView_->setGridTransformPlane(GlView::GridPlane::XZ);
              }
            });
    connect(gridQuickYZ, &QPushButton::clicked, this,
            [this]
            {
              if (glView_)
              {
                glView_->setGridTransformPlane(GlView::GridPlane::YZ);
              }
            });
    connect(gridQuickReset, &QPushButton::clicked, this,
            [this]
            {
              if (glView_)
              {
                glView_->resetGridOverrides();
              }
            });

    viewportMapSettingsMenu->setStyleSheet(
        QStringLiteral("QMenu {"
                       "  color: #ebecf0;"
                       "  background: #15171c;"
                       "  border: none;"
                       "}") +
        viewportPopupActionRowsStyleSheet() +
        QStringLiteral(
            "QLabel {"
            "  color: rgba(235, 236, 240, 255);"
            "}"
            "QDoubleSpinBox {"
            "  background: rgba(0, 0, 0, 60);"
            "  border: 1px solid rgba(255, 255, 255, 40);"
            "  border-radius: 6px;"
            "  padding: 2px 6px;"
            "  color: rgba(235, 236, 240, 255);"
            "}"
            "QSlider::groove:horizontal {"
            "  height: 4px; background: rgba(255, 255, 255, 35); "
            "border-radius: 2px;"
            "}"
            "QSlider::sub-page:horizontal {"
            "  background: rgba(98, 158, 231, 190); border-radius: 2px;"
            "}"
            "QSlider::handle:horizontal {"
            "  width: 12px; margin: -4px 0; background: rgba(218, 228, 242, "
            "255); border-radius: 6px;"
            "}"));
  }

  viewportMapSettingsButton_ = new QToolButton(viewportOverlay_);
  QToolButton *viewportMapSettingsButton = viewportMapSettingsButton_;
  viewportMapSettingsButton->setObjectName("viewportMapSettingsButton");
  viewportMapSettingsButton->setToolButtonStyle(Qt::ToolButtonIconOnly);
  viewportMapSettingsButton->setAutoRaise(true);
  viewportMapSettingsButton->setCheckable(true);
  viewportMapSettingsButton->setIcon(
      ui::makeViewportIcon(ui::ViewportIcon::mapSettings));
  viewportMapSettingsButton->setIconSize(QSize(18, 18));
  viewportMapSettingsButton->setToolTip(
      "<b>Viewport settings</b><br>Open grid, visibility, and display options.");
  viewportMapSettingsButton->setAccessibleName("Show/Hide viewport settings");
  connect(viewportMapSettingsButton, &QToolButton::clicked, this,
          [this](const bool checked) {
            if (!viewportMapSettingsMenu_ || !viewportMapSettingsScrollArea_) {
              return;
            }
            if (!checked) {
              viewportMapSettingsMenu_->hide();
              viewportMapSettingsScrollArea_->hide();
              return;
            }

            viewportMapSettingsMenu_->show();
            viewportMapSettingsScrollArea_->show();
            positionViewportMapSettingsPanel();
            viewportMapSettingsScrollArea_->raise();
          });
  connect(viewportMapSettingsMenu, &QMenu::aboutToHide, this, [this]() {
    if (viewportMapSettingsButton_) {
      const QSignalBlocker blocker(viewportMapSettingsButton_);
      viewportMapSettingsButton_->setChecked(false);
    }
  });

  auto *viewportGamePreviewButton = makeButton(viewportGamePreviewAction_);
  viewportGamePreviewButton->setObjectName("viewportGamePreviewButton");
  layout->addWidget(viewportGamePreviewButton);
  addSeparator(QStringLiteral("viewportGamePreviewSeparator"));

  // Level + camera settings.
  layout->addWidget(viewportLevelSettingsButton);
  layout->addWidget(viewportCameraButton_);
  layout->addWidget(viewportMapSettingsButton);
  addSeparator(QStringLiteral("viewportSettingsSeparator"));

  // Render mode menu.
  auto *renderMenu = new ui::ViewportCameraMenu(viewportOverlay_);
  renderMenu->setObjectName("viewportGeometryModeMenu");
  renderMenu->setToolTipsVisible(true);
  renderMenu->addAction(viewportTexturedAction_);
  renderMenu->addAction(viewportSolidAction_);
  renderMenu->addAction(viewportWireframeAction_);
  renderMenu->addAction(viewportVerticesAction_);
  renderMenu->setStyleSheet(viewportPopupMenuStyleSheet());

  viewportRenderModeButton_ = new QToolButton(viewportOverlay_);
  viewportRenderModeButton_->setObjectName("viewportGeometryModeButton");
  viewportRenderModeButton_->setToolButtonStyle(Qt::ToolButtonIconOnly);
  viewportRenderModeButton_->setAutoRaise(true);
  viewportRenderModeButton_->setIconSize(QSize(18, 18));
  viewportRenderModeButton_->setPopupMode(QToolButton::InstantPopup);
  viewportRenderModeButton_->setToolTip(
      "<b>Geometry display</b><br>Choose how level geometry is drawn.");
  viewportRenderModeButton_->setMenu(renderMenu);
  if (shadingGroup_ && shadingGroup_->checkedAction()) {
    viewportRenderModeButton_->setIcon(shadingGroup_->checkedAction()->icon());
  } else {
    viewportRenderModeButton_->setIcon(
        ui::makeViewportIcon(ui::ViewportIcon::textured));
  }
  layout->addWidget(viewportRenderModeButton_);
  anchorPopupBelowViewportOverlay(renderMenu, viewportRenderModeButton_);

  // Wireframe overlay (level geometry).
  layout->addWidget(makeButton(viewportWireframeOverlayAction_));

  addSeparator(QStringLiteral("viewportRenderSeparator"));

  // Lighting menu.
  auto *lightingMenu = new ui::ViewportCameraMenu(viewportOverlay_);
  lightingMenu->setObjectName("viewportLightingModeMenu");
  lightingMenu->setToolTipsVisible(true);
  lightingMenu->addAction(viewportFullbrightAction_);
  lightingMenu->addAction(viewportLitAction_);
  lightingMenu->addAction(viewportDiffuseAction_);
  lightingMenu->addAction(viewportGouraudAction_);
  lightingMenu->addSeparator();
  lightingMenu->addAction(thingLightRenderingAction_);
  lightingMenu->addAction(sectorPointLightRenderingAction_);
    connect(lightingMenu, &QMenu::aboutToShow, this, [this]()
        {
            if ( sectorPointLightRenderingAction_ )
            {
                const bool available = document_.level().kind == ProjectType::IJIM;
                sectorPointLightRenderingAction_->setVisible(true);
                sectorPointLightRenderingAction_->setEnabled(available);
            }
        });
  lightingMenu->setStyleSheet(viewportPopupMenuStyleSheet());

  viewportLightingModeButton_ = new QToolButton(viewportOverlay_);
  viewportLightingModeButton_->setObjectName("viewportLightingModeButton");
  viewportLightingModeButton_->setToolButtonStyle(Qt::ToolButtonIconOnly);
  viewportLightingModeButton_->setAutoRaise(true);
  viewportLightingModeButton_->setIconSize(QSize(18, 18));
  viewportLightingModeButton_->setPopupMode(QToolButton::InstantPopup);
  viewportLightingModeButton_->setToolTip(
      "<b>Lighting view</b><br>Choose how surface lighting appears in the viewport.");
  viewportLightingModeButton_->setMenu(lightingMenu);
  if (lightingGroup_ && lightingGroup_->checkedAction()) {
    viewportLightingModeButton_->setIcon(
        lightingGroup_->checkedAction()->icon());
  } else {
    viewportLightingModeButton_->setIcon(
        ui::makeViewportIcon(ui::ViewportIcon::lightingFullbright));
  }
  layout->addWidget(viewportLightingModeButton_);
  anchorPopupBelowViewportOverlay(lightingMenu, viewportLightingModeButton_);

  addSeparator(QStringLiteral("viewportLightingSeparator"));

  // Thing rendering menu.
  viewportThingRenderingMenu_ =
      new PersistentViewportSettingsMenu(glView_, true);
  QMenu *thingMenu = viewportThingRenderingMenu_;
  thingMenu->setObjectName("viewportThingRenderingMenu");
  thingMenu->setToolTipsVisible(true);
  thingMenu->hide();
  thingMenu->addAction(thingRenderAction_);
  thingMenu->addAction(thingHiddenAction_);
  thingMenu->addAction(thingCubesAction_);
  thingMenu->addSeparator();
  thingMenu->addAction(thingCollisionShapesAction_);
  thingMenu->addAction(thingVisualBoundsAction_);
  thingMenu->addAction(thingModelLodAction_);
  thingMenu->addSeparator();
  connect(thingMenu, &QMenu::aboutToShow, this,
          [this]() { syncThingModelLodActionAvailability(); });
  thingMenu->addAction(thingShadedAction_);
  thingMenu->addAction(thingWireframesAction_);
  thingMenu->addAction(thingBoxesAction_);
  thingMenu->addAction(thingDotsAction_);
  thingMenu->setStyleSheet(viewportPopupMenuStyleSheet());

  viewportThingModeButton_ = new QToolButton(viewportOverlay_);
  viewportThingModeButton_->setObjectName("viewportThingModeButton");
  viewportThingModeButton_->setToolButtonStyle(Qt::ToolButtonIconOnly);
  viewportThingModeButton_->setAutoRaise(true);
  // Checked means that the rendering-options panel is open. The crossed icon
  // independently communicates whether Thing rendering itself is disabled.
  viewportThingModeButton_->setCheckable(true);
  viewportThingModeButton_->setIconSize(QSize(18, 18));
  connect(viewportThingModeButton_, &QToolButton::clicked, this,
          [this](const bool checked) {
            if (!viewportThingRenderingMenu_) {
              return;
            }
            if (!checked) {
              viewportThingRenderingMenu_->hide();
              return;
            }

            syncThingModelLodActionAvailability();
            viewportThingRenderingMenu_->show();
            viewportThingRenderingMenu_->adjustSize();
            positionViewportThingRenderingPanel();
            viewportThingRenderingMenu_->raise();
          });
  connect(thingMenu, &QMenu::aboutToHide, this, [this]() {
    if (viewportThingModeButton_) {
      const QSignalBlocker blocker(viewportThingModeButton_);
      viewportThingModeButton_->setChecked(false);
    }
  });

  const auto refreshThingModeButton = [this]() {
    if (!viewportThingModeButton_) {
      return;
    }

    const bool visible = thingRenderAction_ && thingRenderAction_->isChecked();
    const QAction *mode =
        thingViewGroup_ ? thingViewGroup_->checkedAction() : nullptr;
    const QIcon baseIcon =
        mode ? mode->icon()
             : ui::makeViewportIcon(ui::ViewportIcon::thingShaded);
    QString modeName = mode ? mode->text() : QStringLiteral("Shaded");
    modeName.remove('&');
    modeName.remove(QStringLiteral("Things: "));

    viewportThingModeButton_->setIcon(
        visible ? baseIcon : ui::makeCrossedOutViewportIcon(baseIcon));
    viewportThingModeButton_->setToolTip(
        visible ? QStringLiteral("<b>Thing display</b><br>Things are shown as %1. Click to open Thing display options.")
                      .arg(modeName)
                : QStringLiteral("<b>Thing display</b><br>Things are hidden (%1 is selected). Click to open Thing display options.")
                      .arg(modeName));
    viewportThingModeButton_->setAccessibleName(
        visible ? QStringLiteral("Thing rendering on")
                : QStringLiteral("Thing rendering off"));
    viewportThingModeButton_->setAccessibleDescription(
        visible ? QStringLiteral("Things are shown as %1. Click to open Thing display options.")
                      .arg(modeName)
                : QStringLiteral("Things are hidden; %1 is selected. Click to open Thing display options.")
                      .arg(modeName));
  };
  connect(thingRenderAction_, &QAction::toggled, viewportThingModeButton_,
          [refreshThingModeButton](const bool) { refreshThingModeButton(); });
  connect(thingViewGroup_, &QActionGroup::triggered, viewportThingModeButton_,
          [refreshThingModeButton](QAction *) { refreshThingModeButton(); });
  refreshThingModeButton();
  layout->addWidget(viewportThingModeButton_);

  // Ambient sector sound is stored independently for Editor and Preview and
  // remains the far-right control for quick access in either pipeline.
  auto *viewportMuteSoundButton = makeButton(viewportMuteSoundAction_);
  viewportMuteSoundButton->setObjectName("viewportMuteSoundButton");
  layout->addWidget(viewportMuteSoundButton);

  viewportOverlay_->setStyleSheet("QWidget#viewportOverlay {"
                                  "  background: rgba(18, 18, 22, 180);"
                                  "  border: 1px solid rgba(255, 255, 255, 40);"
                                  "  border-radius: 8px;"
                                  "}"
                                  "QToolButton {"
                                  "  background: transparent;"
                                  "  border: 0;"
                                  "  padding: 4px;"
                                  "}"
                                  "QToolButton::menu-indicator {"
                                  "  image: none;"
                                  "}"
                                  "QToolButton:hover {"
                                  "  background: rgba(255, 255, 255, 25);"
                                  "  border-radius: 6px;"
                                  "}"
                                  "QToolButton:checked {"
                                  "  background: rgba(255, 255, 255, 45);"
                                  "  border-radius: 6px;"
                                  "}");

  viewportOverlay_->adjustSize();
  positionViewportOverlay();

  glView_->installEventFilter(this);
  viewportOverlay_->show();
}

void MainWindow::positionViewportOverlay() {
  if (!glView_ || !viewportOverlay_) {
    return;
  }

  viewportOverlay_->adjustSize();
  const int margin = 10;
  const int x =
      std::max(margin, glView_->width() - viewportOverlay_->width() - margin);
  const int y = margin;

  viewportOverlay_->move(avoidThingPlacementPanelForOverlay(
      viewportOverlay_->size(), QPoint(x, y), margin));
  positionViewportMapSettingsPanel();
  positionViewportThingRenderingPanel();
  // The right bar is one of the placement constraints for every contextual
  // row.  Reflow here as well as from the resize/event synchronizer so a
  // dynamically changing popup or viewport-bar width cannot leave a row at
  // its old, now-overlapping position during an animation.
  reflowTopCenterOverlays();
}

void MainWindow::positionViewportMapSettingsPanel() {
  if (!glView_ || !viewportMapSettingsScrollArea_ ||
      !viewportMapSettingsMenu_ || !viewportMapSettingsButton_ ||
      viewportMapSettingsScrollArea_->isHidden()) {
    return;
  }

  constexpr int margin = 10;
  constexpr int gap = 6;
  constexpr int scrollBarWidth = 8;
  constexpr int panelBorder = 2;

  const QPoint buttonTopLeft =
      viewportMapSettingsButton_->mapTo(glView_, QPoint(0, 0));
  const int buttonBottom =
      buttonTopLeft.y() + viewportMapSettingsButton_->height();
  const int overlayBottom = viewportOverlay_
                                ? viewportOverlay_->geometry().bottom() + 1
                                : buttonBottom;
  const int panelTop = std::max(buttonBottom, overlayBottom) + gap;

  viewportMapSettingsMenu_->setMinimumSize(QSize(0, 0));
  viewportMapSettingsMenu_->setMaximumSize(
      QSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX));
  QSize naturalSize =
      viewportMapSettingsMenu_->sizeHint().expandedTo(QSize(1, 1));
  naturalSize.setWidth(std::max(
      naturalSize.width(),
      viewportMapSettingsMenu_->property("sed.stableNaturalWidth").toInt()));
  const int maximumHeight = std::max(1, glView_->height() - panelTop - margin);
  const bool needsVerticalScroll =
      naturalSize.height() + panelBorder > maximumHeight;
  viewportMapSettingsScrollArea_->setMinimumHeight(1);
  viewportMapSettingsScrollArea_->setMaximumHeight(maximumHeight);
  viewportMapSettingsScrollArea_->setVerticalScrollBarPolicy(
      needsVerticalScroll ? Qt::ScrollBarAlwaysOn : Qt::ScrollBarAlwaysOff);
  viewportMapSettingsScrollArea_->verticalScrollBar()->setFixedWidth(
      scrollBarWidth);
  viewportMapSettingsMenu_->setFixedSize(naturalSize);

  // Reserve the narrow scrollbar gutter even while it is hidden. Expanding a
  // disclosure can then enable scrolling without changing the panel's outer
  // width or shifting its right-edge toolbar anchor.
  const int panelWidth =
      naturalSize.width() + panelBorder + scrollBarWidth;
  const int panelHeight =
      std::min(naturalSize.height() + panelBorder, maximumHeight);
  viewportMapSettingsScrollArea_->resize(panelWidth, panelHeight);
  {
    constexpr qreal radius = 10.0;
    QPainterPath shape;
    shape.addRoundedRect(
        QRectF(0.0, 0.0,
               static_cast<qreal>(viewportMapSettingsScrollArea_->width()),
               static_cast<qreal>(viewportMapSettingsScrollArea_->height())),
        radius, radius);
    viewportMapSettingsScrollArea_->setMask(
        QRegion(shape.toFillPolygon().toPolygon()));
  }

  int x = buttonTopLeft.x() + viewportMapSettingsButton_->width() -
          viewportMapSettingsScrollArea_->width();
  x = std::clamp(x, margin,
                 std::max(margin, glView_->width() -
                                      viewportMapSettingsScrollArea_->width() -
                                      margin));
  viewportMapSettingsScrollArea_->move(x, panelTop);
}

void MainWindow::positionViewportThingRenderingPanel() {
  if (!glView_ || !viewportThingRenderingMenu_ || !viewportThingModeButton_ ||
      viewportThingRenderingMenu_->isHidden()) {
    return;
  }

  viewportThingRenderingMenu_->adjustSize();
  {
    constexpr qreal radius = 10.0;
    QPainterPath shape;
    shape.addRoundedRect(
        QRectF(0.0, 0.0,
               static_cast<qreal>(viewportThingRenderingMenu_->width()),
               static_cast<qreal>(viewportThingRenderingMenu_->height())),
        radius, radius);
    viewportThingRenderingMenu_->setMask(
        QRegion(shape.toFillPolygon().toPolygon()));
  }
  constexpr int margin = 10;
  constexpr int gap = 6;
  const QPoint buttonTopLeft =
      viewportThingModeButton_->mapTo(glView_, QPoint(0, 0));
  const int buttonBottom =
      buttonTopLeft.y() + viewportThingModeButton_->height();
  const int overlayBottom = viewportOverlay_
                                ? viewportOverlay_->geometry().bottom() + 1
                                : buttonBottom;
  const int panelTop = std::max(buttonBottom, overlayBottom) + gap;
  int x = buttonTopLeft.x() + viewportThingModeButton_->width() -
          viewportThingRenderingMenu_->width();
  x = std::clamp(x, margin,
                 std::max(margin, glView_->width() -
                                      viewportThingRenderingMenu_->width() -
                                      margin));
  viewportThingRenderingMenu_->move(x, panelTop);
}

QPoint MainWindow::avoidThingPlacementPanelForOverlay(const QSize &overlaySize,
                                                      QPoint desired,
                                                      const int margin) const {
  if (!glView_ || editMode_ != EditMode::thing || overlaySize.isEmpty()) {
    return desired;
  }

  const QWidget *reservedWidget = nullptr;
  if ((thingPlacementPanelVisible_ || thingPlacementPanelCollapsePending_) &&
      thingPlacementPanel_ && thingPlacementPanel_->isVisible()) {
    reservedWidget = thingPlacementPanel_;
  } else if (!thingPlacementPanelVisible_ && thingPlacementCollapsedBar_ &&
             thingPlacementCollapsedBar_->isVisible()) {
    reservedWidget = thingPlacementCollapsedBar_;
  }

  if (!reservedWidget) {
    return desired;
  }

  const QRect reserveRect =
      reservedWidget->geometry().adjusted(-margin, -margin, margin, margin);
  QRect overlayRect(desired, overlaySize);
  if (!overlayRect.intersects(reserveRect)) {
    return desired;
  }

  switch (thingPlacementDockPosition_) {
  case ThingPlacementDockPosition::left:
    desired.setX(reserveRect.right() + margin);
    break;
  case ThingPlacementDockPosition::right:
    desired.setX(reserveRect.left() - overlaySize.width() - margin);
    break;
  case ThingPlacementDockPosition::top:
    desired.setY(reserveRect.bottom() + margin);
    break;
  case ThingPlacementDockPosition::bottom:
  default:
    desired.setY(reserveRect.top() - overlaySize.height() - margin);
    break;
  }

  desired.setX(std::clamp(
      desired.x(), margin,
      std::max(margin, glView_->width() - overlaySize.width() - margin)));
  desired.setY(std::clamp(
      desired.y(), margin,
      std::max(margin, glView_->height() - overlaySize.height() - margin)));
  return desired;
}

void MainWindow::setupAddSectorOptionsOverlay() {
  if (!glView_) {
    return;
  }

  addSectorOptionsOverlay_ = new QWidget(glView_);
  addSectorOptionsOverlay_->setAttribute(Qt::WA_StyledBackground, true);
  addSectorOptionsOverlay_->setObjectName("addSectorOptionsOverlay");

  auto *layout = new QHBoxLayout(addSectorOptionsOverlay_);
  layout->setContentsMargins(8, 6, 8, 6);
  layout->setSpacing(6);

  addSectorShapeButton_ = new QToolButton(addSectorOptionsOverlay_);
  addSectorShapeButton_->setObjectName("addSectorShapeButton");
  addSectorShapeButton_->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
  addSectorShapeButton_->setIcon(
      ui::makeViewportIcon(ui::ViewportIcon::actionAddCube));
  addSectorShapeButton_->setIconSize(QSize(16, 16));
  addSectorShapeButton_->setPopupMode(QToolButton::InstantPopup);
  addSectorShapeButton_->setMenu(addSectorShapeMenu_);
  addSectorShapeButton_->setToolTip(
      "<b>Sector shape</b><br>Choose the footprint shape to draw when adding a Sector.");
  layout->addWidget(addSectorShapeButton_);

  auto *separatorA = new QFrame(addSectorOptionsOverlay_);
  separatorA->setFrameShape(QFrame::VLine);
  separatorA->setObjectName("addSectorPlaneSeparator");
  layout->addWidget(separatorA);

  auto *planeLabel = new QLabel("Plane", addSectorOptionsOverlay_);
  planeLabel->setObjectName("addSectorPlaneLabel");
  layout->addWidget(planeLabel);

  addSectorPlaneAxisGroup_ = new QActionGroup(addSectorOptionsOverlay_);
  addSectorPlaneAxisGroup_->setExclusive(true);

  auto addPlaneButton = [&](const QString &text,
                            const SectorDrawPlaneAxis axis) -> QAction * {
    QAction *action = new QAction(text, addSectorOptionsOverlay_);
    action->setObjectName(QStringLiteral("addSector.plane_%1").arg(text.toLower()));
    action->setCheckable(true);
    action->setData(static_cast<int>(axis));
    action->setToolTip(QStringLiteral(
        "<b>%1 plane</b><br>Keep the finished Sector aligned with the world %1-axis plane.").arg(text));
    addSectorPlaneAxisGroup_->addAction(action);

    auto *button = new QToolButton(addSectorOptionsOverlay_);
    button->setDefaultAction(action);
    button->setToolButtonStyle(Qt::ToolButtonTextOnly);
    button->setAutoRaise(true);
    button->setObjectName("addSectorSegmentButton");
    button->setToolTip(action->toolTip());
    layout->addWidget(button);
    return action;
  };

  addSectorPlaneXAction_ =
      addPlaneButton(QStringLiteral("X"), SectorDrawPlaneAxis::x);
  addSectorPlaneYAction_ =
      addPlaneButton(QStringLiteral("Y"), SectorDrawPlaneAxis::y);
  addSectorPlaneZAction_ =
      addPlaneButton(QStringLiteral("Z"), SectorDrawPlaneAxis::z);

  connect(addSectorPlaneAxisGroup_, &QActionGroup::triggered, this,
          [this](QAction *action) {
            if (!action) {
              return;
            }

            const auto axis =
                static_cast<SectorDrawPlaneAxis>(action->data().toInt());
            if (sectorDrawPlaneAxis_ == axis) {
              return;
            }

            cancelSectorDrawPlacement();
            sectorDrawPlaneAxis_ = axis;
            syncAddSectorOptionsOverlay();
            if (glView_ && placementMode_ == PlacementMode::cube) {
              updateSectorDrawCursorGuide(
                  glView_->mapFromGlobal(QCursor::pos()));
            }
            if (statusBar() && placementMode_ == PlacementMode::cube) {
              statusBar()->showMessage(
                  "Plane axis changed. Press and drag to draw the footprint.",
                  1800);
            }
          });

  auto *separatorView = new QFrame(addSectorOptionsOverlay_);
  separatorView->setFrameShape(QFrame::VLine);
  separatorView->setObjectName("addSectorAtViewSeparator");
  layout->addWidget(separatorView);

  addSectorAtViewAction_ = new QAction("At view", addSectorOptionsOverlay_);
  addSectorAtViewAction_->setObjectName("addSector.at_view");
  addSectorAtViewAction_->setCheckable(true);
  addSectorAtViewAction_->setToolTip(
      "<b>At view depth</b><br>Draw at the current view depth while keeping the finished Sector aligned with the selected X, Y, or Z axis.");
  auto *atViewButton = new QToolButton(addSectorOptionsOverlay_);
  atViewButton->setDefaultAction(addSectorAtViewAction_);
  atViewButton->setToolButtonStyle(Qt::ToolButtonTextOnly);
  atViewButton->setAutoRaise(true);
  atViewButton->setObjectName("addSectorAtViewButton");
  atViewButton->setToolTip(addSectorAtViewAction_->toolTip());
  layout->addWidget(atViewButton);

  connect(addSectorAtViewAction_, &QAction::toggled, this,
          [this](const bool checked) {
            if (sectorDrawAtCameraView_ == checked) {
              return;
            }
            cancelSectorDrawPlacement();
            sectorDrawAtCameraView_ = checked;
            if (glView_ && placementMode_ == PlacementMode::cube) {
              updateSectorDrawCursorGuide(glView_->mapFromGlobal(QCursor::pos()));
            }
          });

  addSectorFreeViewAction_ = new QAction("Free", addSectorOptionsOverlay_);
  addSectorFreeViewAction_->setObjectName("addSector.free_view");
  addSectorFreeViewAction_->setCheckable(true);
  addSectorFreeViewAction_->setToolTip(
      "<b>Free view plane</b><br>Draw at the current view depth on a plane that faces the camera. The finished Sector keeps that orientation.");
  auto *freeViewButton = new QToolButton(addSectorOptionsOverlay_);
  freeViewButton->setDefaultAction(addSectorFreeViewAction_);
  freeViewButton->setToolButtonStyle(Qt::ToolButtonTextOnly);
  freeViewButton->setAutoRaise(true);
  freeViewButton->setObjectName("addSectorFreeViewButton");
  freeViewButton->setToolTip(addSectorFreeViewAction_->toolTip());
  layout->addWidget(freeViewButton);

  connect(addSectorFreeViewAction_, &QAction::toggled, this,
          [this](const bool checked) {
            if (sectorDrawFreeView_ == checked) {
              return;
            }
            cancelSectorDrawPlacement();
            sectorDrawFreeView_ = checked;
            syncAddSectorOptionsOverlay();
            if (glView_ && placementMode_ == PlacementMode::cube) {
              updateSectorDrawCursorGuide(glView_->mapFromGlobal(QCursor::pos()));
            }
          });

  auto *separatorB = new QFrame(addSectorOptionsOverlay_);
  separatorB->setFrameShape(QFrame::VLine);
  separatorB->setObjectName("addSectorOptionsSeparator");
  layout->addWidget(separatorB);

    if (viewportTransformGridSnapAction_)
    {
        addSectorGridSnapButton_ = new QToolButton(addSectorOptionsOverlay_);
        addSectorGridSnapButton_->setObjectName("addSectorGridSnapButton");
        addSectorGridSnapButton_->setDefaultAction(
            viewportTransformGridSnapAction_);
        addSectorGridSnapButton_->setToolButtonStyle(Qt::ToolButtonIconOnly);
        addSectorGridSnapButton_->setIconSize(
            QSize(kMoveOptionIconExtent, kMoveOptionIconExtent));
        addSectorGridSnapButton_->setFixedSize(
            kMoveOptionButtonExtent, kMoveOptionButtonExtent);
        addSectorGridSnapButton_->setAutoRaise(true);
        addSectorGridSnapButton_->setCursor(Qt::PointingHandCursor);
        addSectorGridSnapButton_->setToolTip(
            viewportTransformGridSnapAction_->toolTip());
        addSectorGridSnapButton_->setAccessibleName(
            "Snap Add Sector to grid");
        addSectorGridSnapButton_->setAccessibleDescription(
            addSectorGridSnapButton_->toolTip());
        layout->addWidget(addSectorGridSnapButton_);

        connect(viewportTransformGridSnapAction_, &QAction::toggled, this,
            [this](const bool)
            {
                if (!glView_ || placementMode_ != PlacementMode::cube)
                {
                    return;
                }
                const QPoint cursor = glView_->mapFromGlobal(QCursor::pos());
                if (sectorDrawStage_ == SectorDrawStage::none)
                {
                    updateSectorDrawCursorGuide(cursor);
                }
                else
                {
                    updateSectorDrawPreview(
                        cursor, QApplication::keyboardModifiers());
                }
            });
    }

  addSectorSidesLabel_ = new QLabel("Sides", addSectorOptionsOverlay_);
  addSectorSidesLabel_->setObjectName("addSectorOptionsLabel");
  layout->addWidget(addSectorSidesLabel_);

  addSectorSidesSpin_ = new QSpinBox(addSectorOptionsOverlay_);
  addSectorSidesSpin_->setObjectName("addSectorSidesSpin");
  addSectorSidesSpin_->setRange(3, 128);
  addSectorSidesSpin_->setKeyboardTracking(false);
  addSectorSidesSpin_->setButtonSymbols(QAbstractSpinBox::PlusMinus);
  addSectorSidesSpin_->setFixedWidth(72);
  addSectorSidesSpin_->setToolTip(
      "<b>Sides or segments</b><br>Set the side count for supported shapes or the segment count for a sphere.");
  layout->addWidget(addSectorSidesSpin_);

  connect(addSectorSidesSpin_, QOverload<int>::of(&QSpinBox::valueChanged),
          this, [this](const int value) {
            if (!selectedSectorShapeSupportsSides()) {
              return;
            }

            setSelectedSectorShapeSides(value);
            if (sectorDrawStage_ != SectorDrawStage::none && glView_) {
              updateSectorDrawPreview(glView_->mapFromGlobal(QCursor::pos()),
                                      QApplication::keyboardModifiers());
            }
          });

  addSectorOptionsOverlay_->setStyleSheet(
      "QWidget#addSectorOptionsOverlay {"
      "  background: rgba(18, 18, 22, 235);"
      "  border: 1px solid rgba(255, 255, 255, 40);"
      "  border-radius: 8px;"
      "}"
      "QLabel#addSectorOptionsLabel, QLabel#addSectorPlaneLabel {"
      "  color: rgba(230, 232, 238, 220);"
      "  padding-left: 2px;"
      "}"
      "QFrame#addSectorOptionsSeparator, QFrame#addSectorPlaneSeparator, QFrame#addSectorAtViewSeparator {"
      "  color: rgba(255, 255, 255, 32);"
      "}"
      "QToolButton {"
      "  color: rgba(236, 238, 242, 245);"
      "  background: rgba(255, 255, 255, 12);"
      "  border: 1px solid rgba(255, 255, 255, 28);"
      "  border-radius: 5px;"
      "  padding: 3px 8px;"
      "}"
      "QToolButton#addSectorSegmentButton {"
      "  min-width: 24px;"
      "  padding-left: 6px;"
      "  padding-right: 6px;"
      "}"
      "QToolButton:hover {"
      "  background: rgba(255, 255, 255, 24);"
      "}"
      "QToolButton:checked {"
      "  background: rgba(90, 145, 220, 190);"
      "  border-color: rgba(150, 195, 255, 130);"
      "}"
      "QSpinBox {"
      "  background: rgba(255, 255, 255, 14);"
      "  border: 1px solid rgba(255, 255, 255, 32);"
      "  border-radius: 5px;"
      "  padding: 2px 5px;"
      "  color: rgba(236, 238, 242, 245);"
      "}"
      "QSpinBox:disabled, QLabel:disabled {"
      "  color: rgba(236, 238, 242, 90);"
      "}");

  addSectorOptionsOverlay_->hide();
  syncAddSectorOptionsOverlay();
}

void MainWindow::positionAddSectorOptionsOverlay() {
  if (!glView_ || !addSectorOptionsOverlay_) {
    return;
  }

  addSectorOptionsOverlay_->adjustSize();

  const int margin = 10;
  int x = std::max(margin,
                   (glView_->width() - addSectorOptionsOverlay_->width()) / 2);
  const int y = margin;

  if (viewportOverlay_ && viewportOverlay_->isVisible()) {
    const QRect optionsRect(x, y, addSectorOptionsOverlay_->width(),
                            addSectorOptionsOverlay_->height());
    const QRect avoidRect =
        viewportOverlay_->geometry().adjusted(-margin, -margin, margin, margin);
    if (optionsRect.intersects(avoidRect)) {
      x = std::max(margin, avoidRect.left() -
                               addSectorOptionsOverlay_->width() - margin);
    }
  }

  addSectorOptionsOverlay_->move(x, y);
  reflowTopCenterOverlays();
}

void MainWindow::syncViewportContextualOverlays() {
  // Tool recovery intentionally publishes QAction and GlView ownership with
  // signals blocked: replaying QAction::trigger() here could create undo
  // records or discard restored staged data.  Keep every contextual surface
  // behind one explicit synchronizer so a restored tool cannot be active in
  // the controller and toolbar while its options remain stale or hidden.
  syncAddSectorOptionsOverlay();
  syncTransformMoveOptionsOverlay();
  syncLightBakeOverlay();
  syncSectorJoinOptionsOverlay();
  syncSectorThrustOptionsOverlay();
  syncCleaveOptionsOverlay();
  syncThingMoveOptionsOverlay();
  syncSurfaceExtrudeOptionsOverlay();
  syncViewportToolsOverlay();
  reflowTopCenterOverlays();
}

void MainWindow::reflowTopCenterOverlays() {
  if (!glView_) {
    return;
  }

  // All of these widgets are children of GlView and therefore use the same
  // coordinate space.  Keep a stable order so two temporarily-visible
  // contextual toolbars never fight over the same row while QAction state is
  // settling (for example when switching from Add Sector to Cleave).
  const std::array<QWidget*, 8> rows{
      lightBakeOverlay_,
      addSectorOptionsOverlay_,
      cleaveOptionsOverlay_,
      transformMoveOptionsOverlay_,
      sectorJoinOptionsOverlay_,
      sectorThrustOptionsOverlay_,
      thingMoveOptionsOverlay_,
      surfaceExtrudeOptionsOverlay_};

  constexpr int margin = 10;
  constexpr int gap = 6;
  int nextTop = margin;

  // A contextual toolbar is a top-center toolbar, not a toolbar centered in
  // whatever horizontal lane happens to remain between the corner bars. Keep
  // its normal full-viewport center and only drop it below the corner bars
  // when that normal rectangle would overlap one of them.
  std::vector<QRect> topBars{};
  topBars.reserve(2);

  // Map through global coordinates rather than relying on every overlay
  // remaining a direct child as their implementations evolve.
  const auto widgetRect = [this](QWidget* const widget) -> QRect {
    if (!widget || !glView_) {
      return {};
    }
    return QRect(glView_->mapFromGlobal(widget->mapToGlobal(QPoint(0, 0))),
                 widget->size());
  };

  // Only the two horizontal corner bars constrain this top-center lane.  The
  // transform and tool palettes are tall vertical strips; using their bottom
  // edge here used to push a contextual row toward the middle of the viewport.
  for (QWidget* const cornerBar :
       std::array<QWidget*, 2>{viewportEditModeOverlay_, viewportOverlay_}) {
    if (!cornerBar || cornerBar->isHidden()) {
      continue;
    }
    cornerBar->adjustSize();
    const QRect rect = widgetRect(cornerBar);
    const QRect expanded = rect.adjusted(-margin, -margin, margin, margin);
    topBars.push_back(expanded);
  }

  for (QWidget* const row : rows)
  {
    if (!row || row->isHidden())
    {
      continue;
    }

    row->adjustSize();
    const int width = row->width();
    const int height = row->height();
    int x = (glView_->width() - width) / 2;
    int y = nextTop;
    const QRect normalCandidate(x, y, width, height);
    int overlappingBarBottom = -1;
    for (const QRect& bar : topBars) {
      if (normalCandidate.intersects(bar)) {
        overlappingBarBottom = std::max(overlappingBarBottom, bar.bottom());
      }
    }
    if (overlappingBarBottom >= 0) {
      y = std::max(y, overlappingBarBottom + 1 + gap);
    }

    // Keep the row in the viewport even when a user has a very small window.
    // It is preferable to let the lower rows continue below the visible area
    // (and be reachable after resizing) than to put controls outside GlView.
    x = std::clamp(x, margin,
                   std::max(margin, glView_->width() - width - margin));
    y = std::max(margin, y);
    row->move(x, y);
    nextTop = y + height + gap;
  }
}

void MainWindow::syncAddSectorOptionsOverlay() {
  if (!addSectorOptionsOverlay_) {
    return;
  }

  const bool active = placementMode_ == PlacementMode::cube;
  addSectorOptionsOverlay_->setVisible(active);
  if (!active) {
    return;
  }

  if (addSectorShapeButton_) {
    const QString shape =
        selectedSectorShapeName_.empty()
            ? QStringLiteral("Cube")
            : QString::fromStdString(selectedSectorShapeName_);
    addSectorShapeButton_->setText(shape);
  }

  auto setChecked = [](QAction *action, const bool checked) {
    if (!action) {
      return;
    }
    const QSignalBlocker blocker(action);
    action->setChecked(checked);
  };

  setChecked(addSectorPlaneXAction_,
             sectorDrawPlaneAxis_ == SectorDrawPlaneAxis::x);
  setChecked(addSectorPlaneYAction_,
             sectorDrawPlaneAxis_ == SectorDrawPlaneAxis::y);
  setChecked(addSectorPlaneZAction_,
             sectorDrawPlaneAxis_ == SectorDrawPlaneAxis::z);
  setChecked(addSectorAtViewAction_, sectorDrawAtCameraView_);
  setChecked(addSectorFreeViewAction_, sectorDrawFreeView_);
    if (addSectorGridSnapButton_ && viewportTransformGridSnapAction_)
    {
        const QSignalBlocker blocker(addSectorGridSnapButton_);
        addSectorGridSnapButton_->setChecked(
            viewportTransformGridSnapAction_->isChecked());
    }

  const bool showAxisPlacement = !sectorDrawFreeView_;
  // Keep Free itself visible while Free mode is active: it is the only
  // unambiguous in-context control for leaving that mode and choosing an
  // axis/At-view placement again.  The axis and At-view controls are hidden
  // (rather than merely disabled) because they have no meaning in Free mode.
  if (QWidget* planeLabel = addSectorOptionsOverlay_->findChild<QWidget*>(
          QStringLiteral("addSectorPlaneLabel"))) {
    planeLabel->setVisible(showAxisPlacement);
  }
  if (QWidget* planeSeparator = addSectorOptionsOverlay_->findChild<QWidget*>(
          QStringLiteral("addSectorPlaneSeparator"))) {
    planeSeparator->setVisible(showAxisPlacement);
  }
  if (QWidget* atViewSeparator = addSectorOptionsOverlay_->findChild<QWidget*>(
          QStringLiteral("addSectorAtViewSeparator"))) {
    atViewSeparator->setVisible(showAxisPlacement);
  }
  // QAction visibility is normally propagated by QToolButton, but that
  // propagation is deferred on some Qt styles while the popup is being
  // resized.  Update both ends explicitly so Free mode cannot leave disabled
  // X/Y/Z or At-view controls as stale, clickable-looking buttons.
  const auto setActionAndButtonsVisible = [this](QAction* const action,
                                                  const bool visible)
  {
    if (!action || !addSectorOptionsOverlay_)
    {
      return;
    }
    action->setVisible(visible);
    const auto buttons = addSectorOptionsOverlay_->findChildren<QToolButton*>();
    for (QToolButton* const button : buttons)
    {
      if (button && button->defaultAction() == action)
      {
        button->setVisible(visible);
      }
    }
  };
  setActionAndButtonsVisible(addSectorPlaneXAction_, showAxisPlacement);
  setActionAndButtonsVisible(addSectorPlaneYAction_, showAxisPlacement);
  setActionAndButtonsVisible(addSectorPlaneZAction_, showAxisPlacement);
  setActionAndButtonsVisible(addSectorAtViewAction_, showAxisPlacement);

  const bool hasSides = selectedSectorShapeSupportsSides();
  if (addSectorSidesLabel_) {
    addSectorSidesLabel_->setText(selectedSectorSidesLabel());
    addSectorSidesLabel_->setEnabled(hasSides);
  }
  if (addSectorSidesSpin_) {
    const QSignalBlocker blocker(addSectorSidesSpin_);
    if (sed::text::iequalsAscii(selectedSectorShapeName_, "UV Sphere")) {
      addSectorSidesSpin_->setRange(4, 96);
    } else {
      addSectorSidesSpin_->setRange(3, 128);
    }
    addSectorSidesSpin_->setValue(std::max(3, selectedSectorShapeSides()));
    addSectorSidesSpin_->setEnabled(hasSides);
  }

  addSectorOptionsOverlay_->adjustSize();
  positionAddSectorOptionsOverlay();
}

void MainWindow::setupTransformMoveOptionsOverlay() {
  if (!glView_ || !viewportTransformGridSnapAction_ ||
      !viewportTransformGridStickAction_) {
    return;
  }

  transformMoveOptionsOverlay_ = new QWidget(glView_);
  transformMoveOptionsOverlay_->setAttribute(Qt::WA_StyledBackground, true);
  transformMoveOptionsOverlay_->setObjectName("transformMoveOptionsOverlay");

  auto *layout = new QHBoxLayout(transformMoveOptionsOverlay_);
  layout->setContentsMargins(8, 6, 8, 6);
  layout->setSpacing(6);

  viewportTransformMoveOptionsLabel_ =
      new QLabel("Move", transformMoveOptionsOverlay_);
  viewportTransformMoveOptionsLabel_->setObjectName(
      "transformMoveOptionsLabel");
  layout->addWidget(viewportTransformMoveOptionsLabel_);

  auto *separator = new QFrame(transformMoveOptionsOverlay_);
  separator->setFrameShape(QFrame::VLine);
  separator->setObjectName("transformMoveOptionsSeparator");
  layout->addWidget(separator);

    viewportPreserveConnectedSurfaceUvsButton_ =
        new QToolButton(transformMoveOptionsOverlay_);
    viewportPreserveConnectedSurfaceUvsButton_->setObjectName(
        "preserveConnectedSurfaceUvsButton");
    viewportPreserveConnectedSurfaceUvsButton_->setText("Preserve UVs");
    viewportPreserveConnectedSurfaceUvsButton_->setCheckable(true);
    viewportPreserveConnectedSurfaceUvsButton_->setChecked(
        settingsStore()
            .value("viewport/preserve_connected_surface_uvs", true)
            .toBool());
    viewportPreserveConnectedSurfaceUvsButton_->setToolButtonStyle(
        Qt::ToolButtonTextOnly);
    viewportPreserveConnectedSurfaceUvsButton_->setAutoRaise(true);
    viewportPreserveConnectedSurfaceUvsButton_->setCursor(
        Qt::PointingHandCursor);
    viewportPreserveConnectedSurfaceUvsButton_->setAccessibleName(
        "Preserve connected surface UVs");
    viewportPreserveConnectedSurfaceUvsButton_->setToolTip(
        "<b>Preserve texture scale</b><br>Keep texture size consistent when moving, rotating, or scaling connected Faces. Whole Faces that move or rotate keep their texture placement; scaling a Face or changing its neighbors adjusts texture placement.");
    glView_->setPreserveConnectedSurfaceUvsEnabled(
        viewportPreserveConnectedSurfaceUvsButton_->isChecked());
    connect(
        viewportPreserveConnectedSurfaceUvsButton_,
        &QToolButton::toggled,
        this,
        [this](const bool checked)
        {
            settingsStore().setValue(
                "viewport/preserve_connected_surface_uvs", checked);
            if ( glView_ )
            {
                glView_->setPreserveConnectedSurfaceUvsEnabled(checked);
            }
        });
    layout->addWidget(viewportPreserveConnectedSurfaceUvsButton_);

  viewportTransformGridSnapButton_ =
      new QToolButton(transformMoveOptionsOverlay_);
  viewportTransformGridSnapButton_->setObjectName(
      "viewportTransformGridSnapButton");
  viewportTransformGridSnapButton_->setDefaultAction(
      viewportTransformGridSnapAction_);
  viewportTransformGridSnapButton_->setToolButtonStyle(Qt::ToolButtonIconOnly);
  viewportTransformGridSnapButton_->setIconSize(
      QSize(kMoveOptionIconExtent, kMoveOptionIconExtent));
  viewportTransformGridSnapButton_->setFixedSize(kMoveOptionButtonExtent,
                                                 kMoveOptionButtonExtent);
  viewportTransformGridSnapButton_->setAutoRaise(true);
  viewportTransformGridSnapButton_->setCursor(Qt::PointingHandCursor);
  viewportTransformGridSnapButton_->setToolTip(
      viewportTransformGridSnapAction_->toolTip());
  viewportTransformGridSnapButton_->setAccessibleName("Snap moves to grid");
  viewportTransformGridSnapButton_->setAccessibleDescription(
      viewportTransformGridSnapAction_->toolTip());
  layout->addWidget(viewportTransformGridSnapButton_);

  viewportTransformGridStickButton_ =
      new QToolButton(transformMoveOptionsOverlay_);
  viewportTransformGridStickButton_->setObjectName(
      "viewportTransformGridStickButton");
  viewportTransformGridStickButton_->setDefaultAction(
      viewportTransformGridStickAction_);
  viewportTransformGridStickButton_->setToolButtonStyle(Qt::ToolButtonIconOnly);
  viewportTransformGridStickButton_->setIconSize(
      QSize(kMoveOptionIconExtent, kMoveOptionIconExtent));
  viewportTransformGridStickButton_->setFixedSize(kMoveOptionButtonExtent,
                                                  kMoveOptionButtonExtent);
  viewportTransformGridStickButton_->setAutoRaise(true);
  viewportTransformGridStickButton_->setCursor(Qt::PointingHandCursor);
  viewportTransformGridStickButton_->setToolTip(
      viewportTransformGridStickAction_->toolTip());
  viewportTransformGridStickButton_->setAccessibleName(
      "Stick through grid plane");
  viewportTransformGridStickButton_->setAccessibleDescription(
      viewportTransformGridStickAction_->toolTip());
  layout->addWidget(viewportTransformGridStickButton_);

  viewportSectorMoveSnapAlignButton_ =
      new QToolButton(transformMoveOptionsOverlay_);
  viewportSectorMoveSnapAlignButton_->setObjectName(
      "sectorMoveSnapAlignButton");
  viewportSectorMoveSnapAlignButton_->setText("Snap align");
  viewportSectorMoveSnapAlignButton_->setIcon(
      ui::makeViewportIcon(ui::ViewportIcon::surfaceSnapAlign));
  viewportSectorMoveSnapAlignButton_->setCheckable(true);
  viewportSectorMoveSnapAlignButton_->setChecked(
      settingsStore().value("sector_move/snap_align", true).toBool());
  viewportSectorMoveSnapAlignButton_->setToolButtonStyle(
      Qt::ToolButtonIconOnly);
  viewportSectorMoveSnapAlignButton_->setIconSize(
      QSize(kMoveOptionIconExtent, kMoveOptionIconExtent));
  viewportSectorMoveSnapAlignButton_->setFixedSize(kMoveOptionButtonExtent,
                                                   kMoveOptionButtonExtent);
  viewportSectorMoveSnapAlignButton_->setAutoRaise(true);
  viewportSectorMoveSnapAlignButton_->setToolTip(
      "<b>Snap and align</b><br>Snap a moving Sector to nearby geometry and turn matching faces into alignment.");
  viewportSectorMoveSnapAlignButton_->setCursor(Qt::PointingHandCursor);
  viewportSectorMoveSnapAlignButton_->setAccessibleName(
      "Align nearby sector surfaces");
  viewportSectorMoveSnapAlignButton_->setAccessibleDescription(
      viewportSectorMoveSnapAlignButton_->toolTip());
  layout->addWidget(viewportSectorMoveSnapAlignButton_);

  viewportSurfaceGeometrySnapButton_ =
      new QToolButton(transformMoveOptionsOverlay_);
  viewportSurfaceGeometrySnapButton_->setObjectName(
      "surfaceGeometrySnapButton");
  viewportSurfaceGeometrySnapButton_->setText("Nearby snap");
  viewportSurfaceGeometrySnapButton_->setIcon(
      ui::makeViewportIcon(ui::ViewportIcon::surfaceSnapAlign));
  viewportSurfaceGeometrySnapButton_->setCheckable(true);
  viewportSurfaceGeometrySnapButton_->setChecked(
      settingsStore().value("viewport/transform_geometry_snap", true).toBool());
  viewportSurfaceGeometrySnapButton_->setToolButtonStyle(
      Qt::ToolButtonIconOnly);
  viewportSurfaceGeometrySnapButton_->setIconSize(
      QSize(kMoveOptionIconExtent, kMoveOptionIconExtent));
  viewportSurfaceGeometrySnapButton_->setFixedSize(kMoveOptionButtonExtent,
                                                    kMoveOptionButtonExtent);
  viewportSurfaceGeometrySnapButton_->setAutoRaise(true);
  viewportSurfaceGeometrySnapButton_->setCursor(Qt::PointingHandCursor);
  viewportSurfaceGeometrySnapButton_->setAccessibleName(
      "Snap Surface elements to nearby geometry");
  viewportSurfaceGeometrySnapButton_->setToolTip(
      "<b>Snap to geometry</b><br>Match moved Vertices with nearby Vertices, or moved Edges and Faces with similarly oriented nearby geometry.");
  layout->addWidget(viewportSurfaceGeometrySnapButton_);

  viewportSurfaceGeometryRotateButton_ =
      new QToolButton(transformMoveOptionsOverlay_);
  viewportSurfaceGeometryRotateButton_->setObjectName(
      "surfaceGeometryRotateButton");
  viewportSurfaceGeometryRotateButton_->setText("Rotate to nearby");
  viewportSurfaceGeometryRotateButton_->setIcon(
      ui::makeViewportIcon(ui::ViewportIcon::toolRotate));
  viewportSurfaceGeometryRotateButton_->setCheckable(true);
  viewportSurfaceGeometryRotateButton_->setChecked(
      settingsStore().value("viewport/transform_geometry_rotate", true)
          .toBool());
  viewportSurfaceGeometryRotateButton_->setToolButtonStyle(
      Qt::ToolButtonIconOnly);
  viewportSurfaceGeometryRotateButton_->setIconSize(
      QSize(kMoveOptionIconExtent, kMoveOptionIconExtent));
  viewportSurfaceGeometryRotateButton_->setFixedSize(
      kMoveOptionButtonExtent, kMoveOptionButtonExtent);
  viewportSurfaceGeometryRotateButton_->setAutoRaise(true);
  viewportSurfaceGeometryRotateButton_->setCursor(Qt::PointingHandCursor);
  viewportSurfaceGeometryRotateButton_->setAccessibleName(
      "Rotate moved Edge or Face to nearby geometry");
  viewportSurfaceGeometryRotateButton_->setToolTip(
      "<b>Rotate to nearby</b><br>Turn a moved Face or freely moved Edge to match nearby geometry. Face Move also turns when you drag an axis handle.");
  viewportSurfaceGeometryRotateButton_->setEnabled(
      viewportSurfaceGeometrySnapButton_->isChecked());
  layout->addWidget(viewportSurfaceGeometryRotateButton_);

  connect(viewportSurfaceGeometrySnapButton_, &QToolButton::toggled, this,
          [this](const bool checked) {
            if (viewportSurfaceGeometryRotateButton_) {
              viewportSurfaceGeometryRotateButton_->setEnabled(checked);
            }
            if (viewportSurfaceGeometrySnapButton_->property(
                    "sed.heldModifierPreview").toBool()) {
              return;
            }
            settingsStore().setValue("viewport/transform_geometry_snap",
                                     checked);
            if (glView_) {
              glView_->setGeometryTransformSnapEnabled(checked);
            }
          });

  connect(viewportSurfaceGeometryRotateButton_, &QToolButton::toggled, this,
          [this](const bool checked) {
            settingsStore().setValue("viewport/transform_geometry_rotate",
                                     checked);
            if (glView_) {
              glView_->setGeometryTransformRotateToNearbyEnabled(checked);
            }
          });

  connect(viewportSectorMoveSnapAlignButton_, &QToolButton::toggled, this,
          [this](const bool checked) {
            settingsStore().setValue("sector_move/snap_align", checked);
            if (glView_) {
              glView_->setSectorMoveSnapAlignEnabled(checked);
            }
          });

  // Edge/vertex strict picking is shared by Select, Move, Rotate and Scale.
  // Put it in this existing top-center contextual row rather than the
  // edit-mode sub-mode bar so its visibility follows the active selection
  // aware transform tool.  The QAction remains the single source of truth;
  // hiding this button never clears its checked state or captured face scope.
  if (surfaceStrictSelectionAction_) {
    viewportTransformSurfaceStrictButton_ =
        new QToolButton(transformMoveOptionsOverlay_);
    viewportTransformSurfaceStrictButton_->setObjectName(
        "viewportTransformSurfaceStrictButton");
    viewportTransformSurfaceStrictButton_->setDefaultAction(
        surfaceStrictSelectionAction_);
    viewportTransformSurfaceStrictButton_->setToolButtonStyle(
        Qt::ToolButtonTextOnly);
    viewportTransformSurfaceStrictButton_->setAutoRaise(true);
    viewportTransformSurfaceStrictButton_->setCursor(Qt::PointingHandCursor);
    viewportTransformSurfaceStrictButton_->setToolTip(
        surfaceStrictSelectionAction_->toolTip());
    viewportTransformSurfaceStrictButton_->setAccessibleName(
        "Strict surface edge and vertex selection");
    viewportTransformSurfaceStrictButton_->setAccessibleDescription(
        surfaceStrictSelectionAction_->toolTip());
    layout->addWidget(viewportTransformSurfaceStrictButton_);
  }

  transformMoveOptionsOverlay_->setStyleSheet(
      "QWidget#transformMoveOptionsOverlay {"
      "  background: rgba(18, 18, 22, 235);"
      "  border: 1px solid rgba(255, 255, 255, 40);"
      "  border-radius: 8px;"
      "}"
      "QLabel#transformMoveOptionsLabel {"
      "  color: rgba(230, 232, 238, 220);"
      "  padding-left: 2px;"
      "}"
      "QFrame#transformMoveOptionsSeparator {"
      "  color: rgba(255, 255, 255, 32);"
      "}"
      "QToolButton#viewportTransformGridSnapButton,"
      "QToolButton#viewportTransformGridStickButton,"
      "QToolButton#sectorMoveSnapAlignButton,"
      "QToolButton#surfaceGeometrySnapButton,"
      "QToolButton#surfaceGeometryRotateButton,"
      "QToolButton#preserveConnectedSurfaceUvsButton,"
      "QToolButton#viewportTransformSurfaceStrictButton {"
      "  color: rgba(236, 238, 242, 245);"
      "  background: transparent;"
      "  border: 1px solid transparent;"
      "  border-radius: 5px;"
      "  padding: 3px;"
      "}"
      "QToolButton#viewportTransformGridSnapButton:hover,"
      "QToolButton#viewportTransformGridStickButton:hover,"
      "QToolButton#sectorMoveSnapAlignButton:hover,"
      "QToolButton#surfaceGeometrySnapButton:hover,"
      "QToolButton#surfaceGeometryRotateButton:hover,"
      "QToolButton#preserveConnectedSurfaceUvsButton:hover,"
      "QToolButton#viewportTransformSurfaceStrictButton:hover {"
      "  background: rgba(255, 255, 255, 24);"
      "}"
      "QToolButton#viewportTransformGridSnapButton:checked,"
      "QToolButton#viewportTransformGridStickButton:checked,"
      "QToolButton#sectorMoveSnapAlignButton:checked,"
      "QToolButton#surfaceGeometrySnapButton:checked,"
      "QToolButton#surfaceGeometryRotateButton:checked,"
      "QToolButton#preserveConnectedSurfaceUvsButton:checked,"
      "QToolButton#viewportTransformSurfaceStrictButton:checked {"
      "  background: rgba(90, 145, 220, 105);"
      "  border-color: rgba(150, 195, 255, 90);"
      "}");

  transformMoveOptionsOverlay_->hide();
  syncTransformMoveOptionsOverlay();
}

void MainWindow::positionTransformMoveOptionsOverlay() {
  if (!glView_ || !transformMoveOptionsOverlay_) {
    return;
  }

  transformMoveOptionsOverlay_->adjustSize();

  const int margin = 10;
  int x = std::max(
      margin, (glView_->width() - transformMoveOptionsOverlay_->width()) / 2);
  int y = margin;

  if (editMode_ == EditMode::light && lightBakeOverlay_ &&
      !lightBakeOverlay_->isHidden()) {
    lightBakeOverlay_->adjustSize();
    x = std::max(margin, lightBakeOverlay_->geometry().center().x() -
                             transformMoveOptionsOverlay_->width() / 2);
    y = lightBakeOverlay_->geometry().bottom() + 1 + margin;
  }

  if (viewportOverlay_ && !viewportOverlay_->isHidden()) {
    const QRect optionsRect(x, y, transformMoveOptionsOverlay_->width(),
                            transformMoveOptionsOverlay_->height());
    const QRect avoidRect =
        viewportOverlay_->geometry().adjusted(-margin, -margin, margin, margin);
    if (optionsRect.intersects(avoidRect)) {
      x = std::max(margin, avoidRect.left() -
                               transformMoveOptionsOverlay_->width() - margin);
    }
  }

  transformMoveOptionsOverlay_->move(x, y);
  reflowTopCenterOverlays();
}

void MainWindow::syncTransformMoveOptionsOverlay() {
  if (!transformMoveOptionsOverlay_) {
    return;
  }

  const bool inPreview = glView_ && (glView_->renderPipeline() ==
                                     GlView::RenderPipeline::GamePreview);
  const bool selectionReadOnly =
      selectionTouchesNonEditableLayer(document_.selection());
  const bool sectorMove = editMode_ == EditMode::sector;
  const bool surfaceMove = editMode_ == EditMode::surface;
  const bool lightMove = editMode_ == EditMode::light &&
                         lightEditSubMode_ != LightEditSubMode::thing;
  const auto transformTool = glView_ ? glView_->transformTool()
                                     : GlView::TransformTool::Select;
  // GlView uses Select as its neutral enum even when the user deliberately
  // deactivated every transform action.  Require the corresponding QAction to
  // be checked as well; otherwise the Strict control would remain visible
  // after a normal level/new-project reset while picking is disabled.
  const bool transformSelectionTool =
      (transformTool == GlView::TransformTool::Select
          && viewportTransformSelectAction_
          && viewportTransformSelectAction_->isChecked())
      || (transformTool == GlView::TransformTool::Move
          && viewportTransformMoveAction_
          && viewportTransformMoveAction_->isChecked())
      || (transformTool == GlView::TransformTool::Rotate
          && viewportTransformRotateAction_
          && viewportTransformRotateAction_->isChecked())
      || (transformTool == GlView::TransformTool::Scale
          && viewportTransformScaleAction_
          && viewportTransformScaleAction_->isChecked());
  const bool showSurfaceStrict =
      surfaceMove && transformSelectionTool
      && surfaceSelectionMode_ != SurfaceSelectionMode::face;
  const bool showSurfaceNearbyOptions = surfaceMove
      && transformTool == GlView::TransformTool::Move;
    const bool showPreserveConnectedUvs = (sectorMove || surfaceMove)
        && (transformTool == GlView::TransformTool::Move
            || transformTool == GlView::TransformTool::Rotate
            || transformTool == GlView::TransformTool::Scale)
        && transformSelectionTool;
  const bool showMoveOptions =
      transformTool == GlView::TransformTool::Move
      && viewportTransformMoveAction_
      && viewportTransformMoveAction_->isChecked()
      && (sectorMove || surfaceMove || lightMove);
  const bool active = placementMode_ == PlacementMode::none && glView_
                      && (showMoveOptions || showSurfaceStrict
                          || showPreserveConnectedUvs)
                      && !surfaceExtrudePreviewActive_ && !selectionReadOnly
                      && !inPreview;

  // Keep the child control's own visibility truthful as well as hiding its
  // parent row.  This prevents a stale Strict button from remaining logically
  // visible to accessibility/tests while Face mode, another edit mode, or a
  // modification tool owns input.
  if (viewportTransformSurfaceStrictButton_) {
    viewportTransformSurfaceStrictButton_->setVisible(
        active && showSurfaceStrict);
  }
  if (viewportSurfaceGeometrySnapButton_) {
    viewportSurfaceGeometrySnapButton_->setVisible(
        active && showSurfaceNearbyOptions);
  }
  if (viewportSurfaceGeometryRotateButton_) {
    const bool supportsNearbyOrientation =
        surfaceSelectionMode_ == SurfaceSelectionMode::face
        || surfaceSelectionMode_ == SurfaceSelectionMode::edge;
    viewportSurfaceGeometryRotateButton_->setVisible(
        active && showSurfaceNearbyOptions && supportsNearbyOrientation);
    viewportSurfaceGeometryRotateButton_->setEnabled(
        viewportSurfaceGeometrySnapButton_
        && viewportSurfaceGeometrySnapButton_->isChecked());
  }
    if ( viewportPreserveConnectedSurfaceUvsButton_ )
    {
        viewportPreserveConnectedSurfaceUvsButton_->setVisible(
            active && showPreserveConnectedUvs);
    }
  transformMoveOptionsOverlay_->setVisible(active);
  if (!active) {
    positionLightBakeOverlay();
    positionSectorThrustOptionsOverlay();
    return;
  }

  if (viewportTransformMoveOptionsLabel_) {
    const auto transformTool = glView_->transformTool();
    const QString toolName =
        transformTool == GlView::TransformTool::Select ? QStringLiteral("Select")
        : transformTool == GlView::TransformTool::Rotate ? QStringLiteral("Rotate")
        : transformTool == GlView::TransformTool::Scale ? QStringLiteral("Scale")
        : QStringLiteral("Move");
    viewportTransformMoveOptionsLabel_->setText(
        surfaceMove ? QStringLiteral("Surface ") + toolName
        : sectorMove ? QStringLiteral("Sector ") + toolName
        : lightEditSubMode_ == LightEditSubMode::sector
            ? QStringLiteral("Sector Light Move")
            : QStringLiteral("Light Move"));
  }
  if (viewportSectorMoveSnapAlignButton_) {
    viewportSectorMoveSnapAlignButton_->setVisible(sectorMove);
  }
  if (viewportTransformGridSnapButton_) {
    viewportTransformGridSnapButton_->setVisible(
        glView_->transformTool() == GlView::TransformTool::Move);
  }
  if (viewportTransformGridStickButton_) {
    viewportTransformGridStickButton_->setVisible(
        glView_->transformTool() == GlView::TransformTool::Move);
  }
  transformMoveOptionsOverlay_->adjustSize();
  positionTransformMoveOptionsOverlay();
  positionLightBakeOverlay();
  positionSectorThrustOptionsOverlay();
}

void MainWindow::setupSectorJoinOptionsOverlay() {
  if (!glView_) {
    return;
  }

  sectorJoinOptionsOverlay_ = new QWidget(glView_);
  sectorJoinOptionsOverlay_->setAttribute(Qt::WA_StyledBackground, true);
  sectorJoinOptionsOverlay_->setObjectName("sectorJoinOptionsOverlay");

  auto *layout = new QHBoxLayout(sectorJoinOptionsOverlay_);
  layout->setContentsMargins(8, 6, 8, 6);
  layout->setSpacing(6);

  auto *title = new QLabel("Join sectors", sectorJoinOptionsOverlay_);
  title->setObjectName("sectorJoinOptionsLabel");
  layout->addWidget(title);

  auto *separator = new QFrame(sectorJoinOptionsOverlay_);
  separator->setFrameShape(QFrame::VLine);
  separator->setObjectName("sectorJoinOptionsSeparator");
  layout->addWidget(separator);

  viewportSectorJoinKeepPiecesButton_ =
      new QToolButton(sectorJoinOptionsOverlay_);
  viewportSectorJoinKeepPiecesButton_->setObjectName(
      "sectorJoinKeepPiecesButton");
  viewportSectorJoinKeepPiecesButton_->setText("Keep overlap pieces");
  viewportSectorJoinKeepPiecesButton_->setCheckable(true);
  viewportSectorJoinKeepPiecesButton_->setToolButtonStyle(
      Qt::ToolButtonTextOnly);
  viewportSectorJoinKeepPiecesButton_->setAutoRaise(true);
  viewportSectorJoinKeepPiecesButton_->setToolTip(
      "On: keep sector pieces created by the overlap. Off: cut holes/adjoins "
      "only and remove the overlap inside targets.");
  viewportSectorJoinKeepPiecesButton_->setAccessibleName("Keep overlap pieces");
  viewportSectorJoinKeepPiecesButton_->setAccessibleDescription(
      viewportSectorJoinKeepPiecesButton_->toolTip());
  viewportSectorJoinKeepPiecesButton_->setChecked(
      settingsStore().value("sector_join/keep_overlap_sectors", true).toBool());
  viewportSectorJoinKeepPiecesButton_->setCursor(Qt::PointingHandCursor);
  layout->addWidget(viewportSectorJoinKeepPiecesButton_);

  connect(viewportSectorJoinKeepPiecesButton_, &QToolButton::toggled, this,
          [this](const bool checked) {
            settingsStore().setValue("sector_join/keep_overlap_sectors",
                                     checked);
            updateSectorJoinPreview();
          });

  auto *separatorB = new QFrame(sectorJoinOptionsOverlay_);
  separatorB->setFrameShape(QFrame::VLine);
  separatorB->setObjectName("sectorJoinOptionsSeparator");
  layout->addWidget(separatorB);

  auto makeButton = [&](const QString &text, const QString &objectName,
                        const QIcon &icon = QIcon{}) -> QToolButton * {
    auto *button = new QToolButton(sectorJoinOptionsOverlay_);
    button->setObjectName(objectName);
    button->setText(text);
    button->setToolButtonStyle(icon.isNull() ? Qt::ToolButtonTextOnly
                                             : Qt::ToolButtonTextBesideIcon);
    button->setIcon(icon);
    button->setIconSize(QSize(16, 16));
    button->setCursor(Qt::PointingHandCursor);
    button->setAutoRaise(true);
    layout->addWidget(button);
    return button;
  };

  sectorJoinApplyButton_ =
      makeButton("Apply", "sectorJoinApplyButton",
                 ui::makeViewportIcon(ui::ViewportIcon::toolConnect));
  sectorJoinCancelButton_ = makeButton("Cancel", "sectorJoinCancelButton");

  connect(sectorJoinApplyButton_, &QToolButton::clicked, this,
          [this]() { applySectorJoinPreview(); });
  connect(sectorJoinCancelButton_, &QToolButton::clicked, this,
          [this]() { cancelSectorJoinPreview(); });

  sectorJoinOptionsOverlay_->setStyleSheet(
      "QWidget#sectorJoinOptionsOverlay {"
      "  background: rgba(18, 18, 22, 235);"
      "  border: 1px solid rgba(255, 255, 255, 40);"
      "  border-radius: 8px;"
      "}"
      "QLabel#sectorJoinOptionsLabel {"
      "  color: rgba(230, 232, 238, 220);"
      "  padding-left: 2px;"
      "}"
      "QFrame#sectorJoinOptionsSeparator {"
      "  color: rgba(255, 255, 255, 32);"
      "}"
      "QToolButton {"
      "  color: rgba(236, 238, 242, 245);"
      "  background: rgba(255, 255, 255, 12);"
      "  border: 1px solid rgba(255, 255, 255, 28);"
      "  border-radius: 5px;"
      "  padding: 3px 8px;"
      "}"
      "QToolButton:hover {"
      "  background: rgba(255, 255, 255, 24);"
      "}"
      "QToolButton:checked {"
      "  background: rgba(90, 145, 220, 105);"
      "  border-color: rgba(150, 195, 255, 90);"
      "}"
      "QToolButton#sectorJoinApplyButton {"
      "  background: rgba(90, 145, 220, 130);"
      "  border-color: rgba(150, 195, 255, 100);"
      "}");

  sectorJoinOptionsOverlay_->hide();
  syncSectorJoinOptionsOverlay();
}

void MainWindow::positionSectorJoinOptionsOverlay() {
  if (!glView_ || !sectorJoinOptionsOverlay_) {
    return;
  }

  sectorJoinOptionsOverlay_->adjustSize();

  const int margin = 10;
  int x = std::max(margin,
                   (glView_->width() - sectorJoinOptionsOverlay_->width()) / 2);
  int y = margin;

  if (transformMoveOptionsOverlay_ &&
      transformMoveOptionsOverlay_->isVisible()) {
    y = transformMoveOptionsOverlay_->geometry().bottom() + 1 + margin;
  }

  if (viewportOverlay_ && viewportOverlay_->isVisible()) {
    const QRect optionsRect(x, y, sectorJoinOptionsOverlay_->width(),
                            sectorJoinOptionsOverlay_->height());
    const QRect avoidRect =
        viewportOverlay_->geometry().adjusted(-margin, -margin, margin, margin);
    if (optionsRect.intersects(avoidRect)) {
      x = std::max(margin, avoidRect.left() -
                               sectorJoinOptionsOverlay_->width() - margin);
    }
  }

  sectorJoinOptionsOverlay_->move(x, y);
  reflowTopCenterOverlays();
}

void MainWindow::syncSectorJoinOptionsOverlay() {
  if (!sectorJoinOptionsOverlay_) {
    return;
  }

  const bool inPreview = glView_ && (glView_->renderPipeline() ==
                                     GlView::RenderPipeline::GamePreview);
  const bool selectionReadOnly =
      selectionTouchesNonEditableLayer(document_.selection());
  const bool active =
      sectorJoinPreviewActive_ &&
      sectorJoinPreviewSelection_.sectors == document_.selection().sectors &&
      editMode_ == EditMode::sector && placementMode_ == PlacementMode::none &&
      ui::selection::canJoinSectorSelection(document_.selection()) &&
      !selectionReadOnly && !inPreview;

  if (sectorJoinPreviewActive_ && !active) {
    cancelSectorJoinPreview();
    return;
  }

  sectorJoinOptionsOverlay_->setVisible(active);
  if (!active) {
    return;
  }

  sectorJoinOptionsOverlay_->adjustSize();
  positionSectorJoinOptionsOverlay();
}

void MainWindow::setupThingMoveOptionsOverlay() {
  if (!glView_ || !viewportTransformGridSnapAction_ ||
      !viewportTransformGridStickAction_) {
    return;
  }

  thingMoveOptionsOverlay_ = new QWidget(glView_);
  thingMoveOptionsOverlay_->setAttribute(Qt::WA_StyledBackground, true);
  thingMoveOptionsOverlay_->setObjectName("thingMoveOptionsOverlay");

  auto *layout = new QHBoxLayout(thingMoveOptionsOverlay_);
  layout->setContentsMargins(8, 6, 8, 6);
  layout->setSpacing(6);

  viewportThingMoveOptionsLabel_ =
      new QLabel("Thing Move", thingMoveOptionsOverlay_);
  viewportThingMoveOptionsLabel_->setObjectName("thingMoveOptionsLabel");
  layout->addWidget(viewportThingMoveOptionsLabel_);

  auto *separator = new QFrame(thingMoveOptionsOverlay_);
  separator->setFrameShape(QFrame::VLine);
  separator->setObjectName("thingMoveOptionsSeparator");
  layout->addWidget(separator);

  auto makeActionButton = [&](QAction *action, const QString &objectName,
                              const QString &accessibleName) -> QToolButton * {
    auto *button = new QToolButton(thingMoveOptionsOverlay_);
    button->setObjectName(objectName);
    button->setDefaultAction(action);
    button->setToolButtonStyle(Qt::ToolButtonIconOnly);
    button->setIconSize(QSize(kMoveOptionIconExtent, kMoveOptionIconExtent));
    button->setFixedSize(kMoveOptionButtonExtent, kMoveOptionButtonExtent);
    button->setAutoRaise(true);
    button->setCursor(Qt::PointingHandCursor);
    button->setToolTip(action ? action->toolTip() : QString{});
    button->setAccessibleName(accessibleName);
    button->setAccessibleDescription(action ? action->toolTip() : QString{});
    layout->addWidget(button);
    return button;
  };

  viewportThingMoveGridSnapButton_ =
      makeActionButton(viewportTransformGridSnapAction_,
                       "viewportThingMoveGridSnapButton", "Snap moves to grid");
  viewportThingMoveGridStickButton_ = makeActionButton(
      viewportTransformGridStickAction_, "viewportThingMoveGridStickButton",
      "Stick through grid plane");

  auto *separatorB = new QFrame(thingMoveOptionsOverlay_);
  separatorB->setFrameShape(QFrame::VLine);
  separatorB->setObjectName("thingMoveSpecificOptionsSeparator");
  viewportThingMoveSpecificOptionsSeparator_ = separatorB;
  layout->addWidget(separatorB);

  auto makeThingMoveButton = [&](const QString &text, const QString &objectName,
                                 const QString &tooltip,
                                 const ui::ViewportIcon icon,
                                 const bool checked) -> QToolButton * {
    auto *button = new QToolButton(thingMoveOptionsOverlay_);
    button->setObjectName(objectName);
    button->setText(text);
    button->setIcon(ui::makeViewportIcon(icon));
    button->setCheckable(true);
    button->setChecked(checked);
    button->setToolButtonStyle(Qt::ToolButtonIconOnly);
    button->setIconSize(QSize(kMoveOptionIconExtent, kMoveOptionIconExtent));
    button->setFixedSize(kMoveOptionButtonExtent, kMoveOptionButtonExtent);
    button->setAutoRaise(true);
    button->setToolTip(tooltip);
    button->setAccessibleName(text);
    button->setAccessibleDescription(tooltip);
    button->setCursor(Qt::PointingHandCursor);
    layout->addWidget(button);
    return button;
  };

  viewportThingMoveStickButton_ = makeThingMoveButton(
      "Surface stick", "thingMoveSurfaceStickButton",
      "<b>Surface stick</b><br>Pause a moved Thing when its bounds touch solid level geometry. Keep dragging to push through.",
      ui::ViewportIcon::thingSurfaceStick,
      settingsStore().value("thing_move/surface_stick", true).toBool());
  viewportThingMoveSurfaceAlignButton_ = makeThingMoveButton(
      "Align to surface", "thingMoveSurfaceAlignButton",
      "<b>Align to surface</b><br>While Surface Stick has contact, turn the Thing to sit against the touched surface.",
      ui::ViewportIcon::surfaceSnapAlign,
      settingsStore().value("thing_move/surface_align", false).toBool());
  viewportThingMoveSnapButton_ = makeThingMoveButton(
      "Thing snap", "thingMoveSnapToThingButton",
      "<b>Snap to Thing</b><br>Snap a moved Thing to a nearby Thing by matching connectors or sides.",
      ui::ViewportIcon::thingSnap,
      settingsStore().value("thing_move/snap_to_thing", true).toBool());
  viewportThingMoveSnapRotateButton_ = makeThingMoveButton(
      "Rotate to snap", "thingMoveSnapRotateButton",
      "<b>Rotate to snap</b><br>When two Things snap together, turn the moved Thing so matching connectors or faces line up.",
      ui::ViewportIcon::thingSnapRotate,
      settingsStore().value("thing_move/snap_rotate", true).toBool());
  viewportThingMoveSnapRotateButton_->setEnabled(
      viewportThingMoveSnapButton_->isChecked());

  connect(viewportThingMoveStickButton_, &QToolButton::toggled, this,
          [this](const bool checked) {
            settingsStore().setValue("thing_move/surface_stick", checked);
            if (glView_) {
              glView_->setThingMoveSurfaceStickEnabled(checked);
            }
            syncThingMoveOptionsOverlay();
          });
  connect(viewportThingMoveSurfaceAlignButton_, &QToolButton::toggled, this,
          [this](const bool checked) {
            settingsStore().setValue("thing_move/surface_align", checked);
            if (glView_) {
              glView_->setThingMoveSurfaceAlignEnabled(checked);
            }
            syncThingMoveOptionsOverlay();
          });
  connect(viewportThingMoveSnapButton_, &QToolButton::toggled, this,
          [this](const bool checked) {
            settingsStore().setValue("thing_move/snap_to_thing", checked);
            if (glView_) {
              glView_->setThingMoveSnapToThingEnabled(checked);
            }
            syncThingMoveOptionsOverlay();
          });
  connect(viewportThingMoveSnapRotateButton_, &QToolButton::toggled, this,
          [this](const bool checked) {
            settingsStore().setValue("thing_move/snap_rotate", checked);
            if (glView_) {
              glView_->setThingMoveSnapRotateEnabled(checked);
            }
            syncThingMoveOptionsOverlay();
          });

  thingMoveOptionsOverlay_->setStyleSheet(
      "QWidget#thingMoveOptionsOverlay {"
      "  background: rgba(18, 18, 22, 235);"
      "  border: 1px solid rgba(255, 255, 255, 40);"
      "  border-radius: 8px;"
      "}"
      "QLabel#thingMoveOptionsLabel {"
      "  color: rgba(230, 232, 238, 220);"
      "  padding-left: 2px;"
      "}"
      "QFrame#thingMoveOptionsSeparator,"
      "QFrame#thingMoveSpecificOptionsSeparator {"
      "  color: rgba(255, 255, 255, 32);"
      "}"
      "QToolButton {"
      "  color: rgba(236, 238, 242, 245);"
      "  background: transparent;"
      "  border: 1px solid transparent;"
      "  border-radius: 5px;"
      "  padding: 3px;"
      "}"
      "QToolButton:hover {"
      "  background: rgba(255, 255, 255, 24);"
      "}"
      "QToolButton:checked {"
      "  background: rgba(90, 145, 220, 105);"
      "  border-color: rgba(150, 195, 255, 90);"
      "}"
      "QToolButton:disabled {"
      "  color: rgba(236, 238, 242, 90);"
      "  background: transparent;"
      "  border-color: transparent;"
      "}");

  thingMoveOptionsOverlay_->hide();
  syncSelectionModifierBindings();
  syncThingMoveOptionsOverlay();
}

void MainWindow::positionThingMoveOptionsOverlay() {
  if (!glView_ || !thingMoveOptionsOverlay_) {
    return;
  }

  thingMoveOptionsOverlay_->adjustSize();

  const int margin = 10;
  int x = std::max(margin,
                   (glView_->width() - thingMoveOptionsOverlay_->width()) / 2);
  const int y = margin;

  if (viewportOverlay_ && viewportOverlay_->isVisible()) {
    const QRect optionsRect(x, y, thingMoveOptionsOverlay_->width(),
                            thingMoveOptionsOverlay_->height());
    const QRect avoidRect =
        viewportOverlay_->geometry().adjusted(-margin, -margin, margin, margin);
    if (optionsRect.intersects(avoidRect)) {
      x = std::max(margin, avoidRect.left() -
                               thingMoveOptionsOverlay_->width() - margin);
    }
  }

  thingMoveOptionsOverlay_->move(x, y);
  reflowTopCenterOverlays();
}

void MainWindow::syncThingMoveOptionsOverlay() {
  if (!thingMoveOptionsOverlay_) {
    clearFramePlacementPreview();
    return;
  }

  const bool inPreview = glView_ && (glView_->renderPipeline() ==
                                     GlView::RenderPipeline::GamePreview);
  const bool selectionReadOnly =
      selectionTouchesNonEditableLayer(document_.selection());
  const bool thingLightMove = editMode_ == EditMode::light &&
                              lightEditSubMode_ == LightEditSubMode::thing;
  const bool frameAdd = editMode_ == EditMode::frame &&
                        viewportFrameAddAction_ &&
                        viewportFrameAddAction_->isChecked();
  if (!frameAdd) {
    clearFramePlacementPreview();
  }
  const bool activeMove =
      (editMode_ == EditMode::thing || editMode_ == EditMode::frame ||
       thingLightMove) &&
      placementMode_ == PlacementMode::none && glView_ &&
      glView_->transformTool() == GlView::TransformTool::Move &&
      !selectionReadOnly;
  const bool active =
      (activeMove || frameAdd) && !inPreview;

  thingMoveOptionsOverlay_->setVisible(active);
  if (!active) {
    clearFramePlacementPreview();
    return;
  }

  const bool frameMove = !frameAdd && glView_->frameEditEnabled();
  if (viewportThingMoveOptionsLabel_) {
    viewportThingMoveOptionsLabel_->setText(frameAdd    ? "Frame Add"
                                            : frameMove ? "Frame Move"
                                            : thingLightMove
                                                ? "Thing Light Owner Move"
                                                : "Thing Move");
  }
  if (viewportThingMoveGridSnapButton_) {
    viewportThingMoveGridSnapButton_->setVisible(!frameAdd);
  }
  if (viewportThingMoveGridStickButton_) {
    viewportThingMoveGridStickButton_->setVisible(!frameAdd);
  }
  const auto holdHint = [](const ui::shortcuts::ModifierBindings& bindings)
  {
    return bindings.isEmpty()
        ? QStringLiteral("No hold shortcut is assigned to temporarily switch this option.")
        : QStringLiteral("Hold %1 to temporarily switch this option.")
              .arg(ui::shortcuts::modifierBindingsText(bindings));
  };
  if (viewportThingMoveStickButton_) {
    viewportThingMoveStickButton_->setVisible(true);
    const QString description = frameAdd
        ? QStringLiteral("Place the frame's Thing on the picked surface. Turn this off to place freely on the cursor plane.")
        : frameMove
            ? QStringLiteral("Pause a moved frame when its Thing touches solid level geometry. Keep dragging to push through.")
            : QStringLiteral("Pause a moved Thing when its bounds touch solid level geometry. Keep dragging to push through.");
    const QString hold = holdHint(thingMoveSurfaceStickInvertModifierBindings());
    viewportThingMoveStickButton_->setToolTip(
        QStringLiteral("<b>Surface stick</b><br>%1 %2").arg(description, hold));
    viewportThingMoveStickButton_->setAccessibleDescription(
        QStringLiteral("%1 %2").arg(description, hold));
  }
  if (viewportThingMoveSurfaceAlignButton_) {
    viewportThingMoveSurfaceAlignButton_->setVisible(true);
    const QString description =
        frameAdd    ? QStringLiteral(
                          "Turn the new frame so its Thing sits against the picked surface.")
        : frameMove ? QStringLiteral(
                          "While Surface Stick has contact, turn the frame's Thing to sit against the touched surface.")
                    : QStringLiteral(
                          "While Surface Stick has contact, turn the Thing to sit against the touched surface.");
    const auto& surfaceAlignInvert = thingMoveSurfaceAlignInvertModifierBindings();
    const QString hold = holdHint(surfaceAlignInvert);
    viewportThingMoveSurfaceAlignButton_->setToolTip(
        QStringLiteral("<b>Align to surface</b><br>%1 %2").arg(description, hold));
    viewportThingMoveSurfaceAlignButton_->setAccessibleDescription(
        QStringLiteral("%1 %2").arg(description, hold));
    viewportThingMoveSurfaceAlignButton_->setEnabled(
        viewportThingMoveStickButton_ &&
        viewportThingMoveStickButton_->isChecked());
  }
  if (viewportThingMoveSpecificOptionsSeparator_) {
    viewportThingMoveSpecificOptionsSeparator_->setVisible(true);
  }
  if (viewportThingMoveSnapButton_) {
    viewportThingMoveSnapButton_->setVisible(true);
    const auto& thingSnapInvert = thingMoveSnapInvertModifierBindings();
    const QString description = frameAdd || frameMove
        ? QStringLiteral("Snap the frame's Thing to a nearby Thing by matching connectors or sides.")
        : QStringLiteral("Snap a moved Thing to a nearby Thing by matching connectors or sides.");
    const QString hold = holdHint(thingSnapInvert);
    viewportThingMoveSnapButton_->setToolTip(
        QStringLiteral("<b>Snap to Thing</b><br>%1 %2").arg(description, hold));
    viewportThingMoveSnapButton_->setAccessibleDescription(
        QStringLiteral("%1 %2").arg(description, hold));
  }
  if (viewportThingMoveSnapRotateButton_) {
    viewportThingMoveSnapRotateButton_->setVisible(true);
    const auto& thingRotateInvert = thingMoveSnapRotateInvertModifierBindings();
    const QString description = frameAdd || frameMove
        ? QStringLiteral("When a frame snaps to another Thing, turn it so matching connectors or faces line up.")
        : QStringLiteral("When two Things snap together, turn the moved Thing so matching connectors or faces line up.");
    const QString hold = holdHint(thingRotateInvert);
    viewportThingMoveSnapRotateButton_->setToolTip(
        QStringLiteral("<b>Rotate to snap</b><br>%1 %2").arg(description, hold));
    viewportThingMoveSnapRotateButton_->setAccessibleDescription(
        QStringLiteral("%1 %2").arg(description, hold));
    viewportThingMoveSnapRotateButton_->setEnabled(
        viewportThingMoveSnapButton_ &&
        viewportThingMoveSnapButton_->isChecked());
  }
  thingMoveOptionsOverlay_->adjustSize();
  positionThingMoveOptionsOverlay();
  if (frameAdd && framePlacementPreview_ && glView_) {
    updateFramePlacementPreview(
        framePlacementPreview_->widgetPosition,
        QApplication::keyboardModifiers()
            & ui::shortcuts::editorModifierMask());
  }
}

void MainWindow::setupThingPlacementPanel() {
  if (!glView_) {
    return;
  }

  {
    QSettings settings = settingsStore();
    thingPlacementKeepActive_ =
        settings.value("thing_placement/keep_active", false).toBool();
    thingPlacementSnapToThing_ =
        settings.value("thing_placement/snap_to_thing", false).toBool();
    thingPlacementSnapRotate_ =
        settings.value("thing_placement/snap_rotate", true).toBool();
    thingPlacementFiltersVisible_ =
        settings.value("thing_placement/filters_visible", true).toBool();
    const QString defaultMode =
        settings
            .value("thing_placement/default_mode", QStringLiteral("surface"))
            .toString();
    if (defaultMode.compare(QStringLiteral("tilt"), Qt::CaseInsensitive) == 0) {
      thingPlacementDefaultMode_ = ThingPlacementDefaultMode::surfaceTilt;
    } else if (defaultMode.compare(QStringLiteral("free"),
                                   Qt::CaseInsensitive) == 0) {
      thingPlacementDefaultMode_ = ThingPlacementDefaultMode::free;
    } else if (defaultMode.compare(QStringLiteral("snap"),
                                   Qt::CaseInsensitive) == 0) {
      thingPlacementDefaultMode_ = ThingPlacementDefaultMode::surface;
      thingPlacementSnapToThing_ = true;
    } else {
      thingPlacementDefaultMode_ = ThingPlacementDefaultMode::surface;
    }

    const QString toolMode =
        settings.value("thing_placement/tool_mode", QStringLiteral("single"))
            .toString();
    thingPlacementToolMode_ =
        toolMode.compare(QStringLiteral("brush"), Qt::CaseInsensitive) == 0
            ? ThingPlacementToolMode::brush
            : ThingPlacementToolMode::single;

    const QString brushShape =
        settings.value("thing_placement/brush_shape", QStringLiteral("circle"))
            .toString();
    if (brushShape.compare(QStringLiteral("square"), Qt::CaseInsensitive) ==
        0) {
      thingPlacementBrushShape_ = ThingPlacementBrushShape::square;
    } else if (brushShape.compare(QStringLiteral("rectangle"),
                                  Qt::CaseInsensitive) == 0) {
      thingPlacementBrushShape_ = ThingPlacementBrushShape::rectangle;
    } else {
      thingPlacementBrushShape_ = ThingPlacementBrushShape::circle;
    }

    thingPlacementBrushSize_ = std::clamp(
        settings.value("thing_placement/brush_size", 0.25).toDouble(), 0.05,
        256.0);
    thingPlacementBrushCount_ = std::clamp(
        settings.value("thing_placement/brush_count", 5).toInt(), 1, 256);
    thingPlacementBrushSpacing_ = std::clamp(
        settings.value("thing_placement/brush_spacing", 0.55).toDouble(), 0.10,
        2.0);
    thingPlacementBrushScatter_ = std::clamp(
        settings.value("thing_placement/brush_scatter", 0.0).toDouble(), 0.0,
        1.0);
    thingPlacementBrushRandomRotation_ =
        settings.value("thing_placement/brush_random_rotation", true).toBool();
    thingPlacementBrushRotatePitch_ =
        settings.value("thing_placement/brush_rotate_pitch", false).toBool();
    thingPlacementBrushRotateYaw_ =
        settings.value("thing_placement/brush_rotate_yaw", true).toBool();
    thingPlacementBrushRotateRoll_ =
        settings.value("thing_placement/brush_rotate_roll", false).toBool();
    thingPlacementBrushFixedAngle_ =
        settings.value("thing_placement/brush_fixed_angle", 0.0).toDouble();
    thingPlacementBrushAllowAir_ =
        settings.value("thing_placement/brush_allow_air", false).toBool();
    thingPlacementFilterContentHeight_ = std::clamp(
        settings.value("thing_placement/filter_content_height", 92).toInt(), 38,
        220);
    thingPlacementPanelSideSize_ = std::clamp(
        settings.value("thing_placement/panel_side_size", 0).toInt(), 0, 4096);
    thingPlacementPanelBandSize_ = std::clamp(
        settings.value("thing_placement/panel_band_size", 0).toInt(), 0, 4096);
  }

  thingPlacementPanel_ = new QWidget(glView_);
  thingPlacementPanel_->setAttribute(Qt::WA_StyledBackground, true);
  thingPlacementPanel_->setAttribute(Qt::WA_NoMousePropagation, true);
  thingPlacementPanel_->setObjectName("thingPlacementPanel");
  thingPlacementPanel_->setMouseTracking(true);
  thingPlacementPanel_->installEventFilter(this);

  auto *layout = new QVBoxLayout(thingPlacementPanel_);
  layout->setContentsMargins(10, 9, 10, 10);
  layout->setSpacing(7);

  thingPlacementHeader_ = new QWidget(thingPlacementPanel_);
  thingPlacementHeader_->setObjectName("thingPlacementHeader");
  thingPlacementHeader_->setCursor(Qt::OpenHandCursor);
  thingPlacementHeader_->installEventFilter(this);
  thingPlacementHeader_->setProperty("sed.thingPlacementHeaderDrag", true);

  auto *header = new QHBoxLayout(thingPlacementHeader_);
  header->setContentsMargins(3, 1, 2, 1);
  header->setSpacing(6);

  auto *title = new QLabel("Thing Placement", thingPlacementHeader_);
  title->setObjectName("thingPlacementTitle");
  title->setContentsMargins(3, 0, 0, 0);
  title->setCursor(Qt::OpenHandCursor);
  title->installEventFilter(this);
  title->setProperty("sed.thingPlacementHeaderDrag", true);
  header->addWidget(title);
  header->addStretch(1);

  thingPlacementDockButton_ = new QToolButton(thingPlacementHeader_);
  thingPlacementDockButton_->setObjectName("thingPlacementDockButton");
  thingPlacementDockButton_->setAutoRaise(true);
  thingPlacementDockButton_->setText("Dock");
  thingPlacementDockButton_->setMinimumWidth(58);
  thingPlacementDockButton_->setPopupMode(QToolButton::InstantPopup);
  auto *dockMenu = new ui::shortcuts::KeySequenceDisplayMenu(thingPlacementDockButton_);
  auto addDockAction = [this,
                        dockMenu](const QString &label,
                                  const ThingPlacementDockPosition position) {
    QAction *action = dockMenu->addAction(label);
    connect(action, &QAction::triggered, this,
            [this, position]() { applyThingPlacementDockPosition(position); });
  };
  addDockAction("Right", ThingPlacementDockPosition::right);
  addDockAction("Left", ThingPlacementDockPosition::left);
  addDockAction("Top", ThingPlacementDockPosition::top);
  addDockAction("Bottom", ThingPlacementDockPosition::bottom);
  thingPlacementDockButton_->setMenu(dockMenu);
  header->addWidget(thingPlacementDockButton_);

  thingPlacementHideButton_ = new QToolButton(thingPlacementHeader_);
  thingPlacementHideButton_->setObjectName("thingPlacementHeaderButton");
  thingPlacementHideButton_->setAutoRaise(true);
  thingPlacementHideButton_->setToolButtonStyle(Qt::ToolButtonIconOnly);
  thingPlacementHideButton_->setIconSize(QSize(18, 18));
  thingPlacementHideButton_->setToolTip(
      "<b>Hide Thing palette</b><br>Collapse the panel while keeping the selected template available.");
  connect(thingPlacementHideButton_, &QToolButton::clicked, this,
          [this]() { setThingPlacementPanelVisible(false); });
  header->addWidget(thingPlacementHideButton_);
  layout->addWidget(thingPlacementHeader_);

  thingPlacementSelectedPreviewHost_ = new QWidget(thingPlacementPanel_);
  thingPlacementSelectedPreviewHost_->setObjectName(
      "thingPlacementSelectedPreview");
  auto *previewLayout = new QVBoxLayout(thingPlacementSelectedPreviewHost_);
  previewLayout->setContentsMargins(0, 0, 0, 0);
  previewLayout->setSpacing(0);
  thingPlacementSelectedPreviewWidget_ =
      new ThingPlacementSelectedPreviewWidget(
          resources_, thingPlacementSelectedPreviewHost_);
  thingPlacementSelectedPreviewWidget_->setMinimumSize(170, 168);
  thingPlacementSelectedPreviewWidget_->setSizePolicy(QSizePolicy::Expanding,
                                                      QSizePolicy::Fixed);
  previewLayout->addWidget(thingPlacementSelectedPreviewWidget_);
  thingPlacementSelectedPreviewName_ =
      new QLabel(thingPlacementSelectedPreviewHost_);
  thingPlacementSelectedPreviewName_->setObjectName(
      "thingPlacementSelectedPreviewName");
  thingPlacementSelectedPreviewName_->setWordWrap(true);
  thingPlacementSelectedPreviewName_->setAlignment(Qt::AlignVCenter |
                                                   Qt::AlignLeft);
  thingPlacementSelectedPreviewName_->hide();
  layout->addWidget(thingPlacementSelectedPreviewHost_);

  auto *optionsHost = new QWidget(thingPlacementPanel_);
  optionsHost->setObjectName("thingPlacementOptions");
  auto *optionsLayout = new QVBoxLayout(optionsHost);
  optionsLayout->setContentsMargins(0, 0, 0, 0);
  optionsLayout->setSpacing(5);

  auto makeModeButton = [](QWidget *parent, const QString &text,
                           const QString &tooltip) -> QToolButton * {
    auto *button = new QToolButton(parent);
    button->setObjectName("thingPlacementSegmentButton");
    button->setText(text);
    button->setToolTip(tooltip);
    button->setCheckable(true);
    button->setAutoRaise(false);
    button->setMinimumHeight(kThingPlacementControlHeight);
    return button;
  };

  auto configurePlacementCheck = [](QCheckBox *checkBox) {
    if (!checkBox) {
      return;
    }

    checkBox->setMinimumHeight(kThingPlacementControlHeight);
    checkBox->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
  };

  // Keep placement modes, snapping, and destination layer as distinct rows.
  // Each option row still wraps when a side-docked panel is narrowed.
  auto *placementSection = new QGroupBox("Placement", optionsHost);
  placementSection->setObjectName("thingPlacementPlacementSection");
  auto *placementLayout = new QVBoxLayout(placementSection);
  placementLayout->setContentsMargins(6, 6, 6, 6);
  placementLayout->setSpacing(4);

  auto *placementModeRow = new QWidget(placementSection);
  placementModeRow->setObjectName("thingPlacementModeRow");
  auto *placementModeFlow = new ui::FlowLayout(placementModeRow, 0, 4, 4);
  thingPlacementModeSurfaceButton_ = makeModeButton(
      placementModeRow, "Surface",
      "<b>Surface placement</b><br>Place on the picked surface without changing the template rotation.");
  thingPlacementModeTiltButton_ =
      makeModeButton(placementModeRow, "Align",
                     "<b>Align placement</b><br>Place on the picked surface and turn the Thing to sit against it.");
  thingPlacementModeFreeButton_ = makeModeButton(
      placementModeRow, "Free",
      "<b>Free placement</b><br>Place freely inside the containing Sector.");
  placementModeFlow->addWidget(thingPlacementModeSurfaceButton_);
  placementModeFlow->addWidget(thingPlacementModeTiltButton_);
  placementModeFlow->addWidget(thingPlacementModeFreeButton_);
  placementLayout->addWidget(placementModeRow);

  auto *placementSnapRow = new QWidget(placementSection);
  placementSnapRow->setObjectName("thingPlacementSnapRow");
  auto *placementSnapFlow = new ui::FlowLayout(placementSnapRow, 0, 4, 4);
  thingPlacementSnapToThingCheck_ =
      new QCheckBox("Snap to Thing", placementSnapRow);
  thingPlacementSnapToThingCheck_->setObjectName("thingPlacementOptionCheck");
  thingPlacementSnapToThingCheck_->setToolTip(
      "<b>Snap to Thing</b><br>In Place mode, snap the new Thing to a compatible side or connector on a nearby Thing. Brush samples ignore this option.");
  configurePlacementCheck(thingPlacementSnapToThingCheck_);
  placementSnapFlow->addWidget(thingPlacementSnapToThingCheck_);
  thingPlacementSnapRotateCheck_ =
      new QCheckBox("Rotate to Align", placementSnapRow);
  thingPlacementSnapRotateCheck_->setObjectName("thingPlacementOptionCheck");
  thingPlacementSnapRotateCheck_->setToolTip(
      "<b>Rotate to align</b><br>In Place mode, turn the new Thing to line up with the snapped side or connector. Brush samples ignore this option.");
  configurePlacementCheck(thingPlacementSnapRotateCheck_);
  placementSnapFlow->addWidget(thingPlacementSnapRotateCheck_);
  placementLayout->addWidget(placementSnapRow);

  auto *placementLayerField = new QWidget(placementSection);
  placementLayerField->setObjectName("thingPlacementLayerField");
  placementLayerField->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
  auto *placementLayerLayout = new QHBoxLayout(placementLayerField);
  placementLayerLayout->setContentsMargins(0, 0, 0, 0);
  placementLayerLayout->setSpacing(4);
  auto *placementLayerLabel = new QLabel("Layer", placementLayerField);
  placementLayerLabel->setToolTip(
      "<b>Layer</b><br>Choose where newly placed Things are added.");
  placementLayerLayout->addWidget(placementLayerLabel);
  thingPlacementLayerCombo_ = new QComboBox(placementLayerField);
  thingPlacementLayerCombo_->setObjectName("thingPlacementLayerCombo");
  thingPlacementLayerCombo_->setMinimumSize(120, kThingPlacementControlHeight);
  thingPlacementLayerCombo_->setMaximumWidth(220);
  thingPlacementLayerCombo_->setSizePolicy(QSizePolicy::Preferred,
                                           QSizePolicy::Fixed);
  thingPlacementLayerCombo_->setToolTip(
      "<b>Layer</b><br>Choose where new Things are added. Auto uses the visible target Sector's layer, or the last visible layer when the target layer is unavailable.");
  placementLayerLayout->addWidget(thingPlacementLayerCombo_);
  placementLayout->addWidget(placementLayerField);
  optionsLayout->addWidget(placementSection);

  // Tool controls use the same responsive section treatment and remain visually
  // separate.
  auto *toolSection = new QGroupBox("Tool", optionsHost);
  toolSection->setObjectName("thingPlacementToolSection");
  auto *toolFlow = new ui::FlowLayout(toolSection, 6, 4, 4);
  thingPlacementToolSingleButton_ =
      makeModeButton(toolSection, "Place",
                     "<b>Place</b><br>Place one Thing with each click.");
  thingPlacementToolBrushButton_ = makeModeButton(
      toolSection, "Brush",
      "<b>Brush</b><br>Paint repeated Thing placements by clicking or dragging in the viewport.");
  toolFlow->addWidget(thingPlacementToolSingleButton_);
  toolFlow->addWidget(thingPlacementToolBrushButton_);
  thingPlacementKeepActiveCheck_ = new QCheckBox("Active", toolSection);
  thingPlacementKeepActiveCheck_->setObjectName("thingPlacementOptionCheck");
  thingPlacementKeepActiveCheck_->setToolTip(
      "<b>Keep active</b><br>Keep the selected template after placing so you can add another copy.");
  configurePlacementCheck(thingPlacementKeepActiveCheck_);
  toolFlow->addWidget(thingPlacementKeepActiveCheck_);
  optionsLayout->addWidget(toolSection);

  thingPlacementBrushOptions_ = new QGroupBox("Brush", optionsHost);
  thingPlacementBrushOptions_->setObjectName("thingPlacementBrushOptions");
  auto *brushFlow = new ui::FlowLayout(thingPlacementBrushOptions_, 6, 6, 4);

  auto configureCompactInput = [](QWidget *widget, const int width) {
    if (!widget) {
      return;
    }
    widget->setMinimumWidth(std::min(width, 88));
    widget->setMaximumWidth(width);
    widget->setMinimumHeight(kThingPlacementControlHeight);
    widget->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
  };

  thingPlacementBrushSizeSpin_ =
      new QDoubleSpinBox(thingPlacementBrushOptions_);
  thingPlacementBrushSizeSpin_->setRange(0.05, 256.0);
  thingPlacementBrushSizeSpin_->setDecimals(2);
  thingPlacementBrushSizeSpin_->setSingleStep(0.05);
  thingPlacementBrushSizeSpin_->setKeyboardTracking(false);
  thingPlacementBrushSizeSpin_->setToolTip(
      "<b>Brush size</b><br>Set the radius of each brush stamp.");
  configureCompactInput(thingPlacementBrushSizeSpin_, 118);
  thingPlacementBrushCountSpin_ = new QSpinBox(thingPlacementBrushOptions_);
  thingPlacementBrushCountSpin_->setRange(1, 256);
  thingPlacementBrushCountSpin_->setKeyboardTracking(false);
  configureCompactInput(thingPlacementBrushCountSpin_, 88);
  thingPlacementBrushShapeCombo_ = new QComboBox(thingPlacementBrushOptions_);
  thingPlacementBrushShapeCombo_->addItem(
      "Circle", static_cast<int>(ThingPlacementBrushShape::circle));
  thingPlacementBrushShapeCombo_->addItem(
      "Square", static_cast<int>(ThingPlacementBrushShape::square));
  thingPlacementBrushShapeCombo_->addItem(
      "Rectangle", static_cast<int>(ThingPlacementBrushShape::rectangle));
  configureCompactInput(thingPlacementBrushShapeCombo_, 130);
  thingPlacementBrushRandomRotateCheck_ =
      new QCheckBox("Random rotate", thingPlacementBrushOptions_);
  thingPlacementBrushRotatePitchCheck_ =
      new QCheckBox("Pitch", thingPlacementBrushOptions_);
  thingPlacementBrushRotateYawCheck_ =
      new QCheckBox("Yaw", thingPlacementBrushOptions_);
  thingPlacementBrushRotateRollCheck_ =
      new QCheckBox("Roll", thingPlacementBrushOptions_);
  configurePlacementCheck(thingPlacementBrushRandomRotateCheck_);
  configurePlacementCheck(thingPlacementBrushRotatePitchCheck_);
  configurePlacementCheck(thingPlacementBrushRotateYawCheck_);
  configurePlacementCheck(thingPlacementBrushRotateRollCheck_);
  thingPlacementBrushFixedAngleSpin_ =
      new QDoubleSpinBox(thingPlacementBrushOptions_);
  thingPlacementBrushFixedAngleSpin_->setRange(-360.0, 360.0);
  thingPlacementBrushFixedAngleSpin_->setDecimals(1);
  thingPlacementBrushFixedAngleSpin_->setSingleStep(15.0);
  thingPlacementBrushFixedAngleSpin_->setKeyboardTracking(false);
  configureCompactInput(thingPlacementBrushFixedAngleSpin_, 118);
  thingPlacementBrushSpacingSpin_ =
      new QDoubleSpinBox(thingPlacementBrushOptions_);
  thingPlacementBrushSpacingSpin_->setRange(0.10, 2.00);
  thingPlacementBrushSpacingSpin_->setDecimals(2);
  thingPlacementBrushSpacingSpin_->setSingleStep(0.05);
  thingPlacementBrushSpacingSpin_->setKeyboardTracking(false);
  thingPlacementBrushSpacingSpin_->setToolTip(
      "<b>Brush spacing</b><br>Set the minimum distance between stamps as a multiple of brush size.");
  configureCompactInput(thingPlacementBrushSpacingSpin_, 118);
  thingPlacementBrushScatterSpin_ =
      new QDoubleSpinBox(thingPlacementBrushOptions_);
  thingPlacementBrushScatterSpin_->setRange(0.0, 1.0);
  thingPlacementBrushScatterSpin_->setDecimals(2);
  thingPlacementBrushScatterSpin_->setSingleStep(0.05);
  thingPlacementBrushScatterSpin_->setKeyboardTracking(false);
  thingPlacementBrushScatterSpin_->setToolTip(
      "<b>Brush scatter</b><br>Randomize sample positions inside each stamp. 0 keeps a stable pattern; 1 fully scatters it.");
  configureCompactInput(thingPlacementBrushScatterSpin_, 118);
  thingPlacementBrushAllowAirCheck_ =
      new QCheckBox("Allow air", thingPlacementBrushOptions_);
  thingPlacementBrushAllowAirCheck_->setToolTip(
      "<b>Allow air</b><br>Place brush samples without a surface hit when they are still inside a Sector.");
  configurePlacementCheck(thingPlacementBrushAllowAirCheck_);
  auto addBrushField = [brushFlow, this](const QString &label,
                                         QWidget *control) {
    auto *field = new QWidget(thingPlacementBrushOptions_);
    field->setObjectName("thingPlacementBrushField");
    field->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    auto *fieldLayout = new QHBoxLayout(field);
    fieldLayout->setContentsMargins(0, 0, 0, 0);
    fieldLayout->setSpacing(4);
    fieldLayout->addWidget(new QLabel(label, field));
    fieldLayout->addWidget(control);
    brushFlow->addWidget(field);
  };
  addBrushField("Size", thingPlacementBrushSizeSpin_);
  addBrushField("Count", thingPlacementBrushCountSpin_);
  addBrushField("Shape", thingPlacementBrushShapeCombo_);
  brushFlow->addWidget(thingPlacementBrushRandomRotateCheck_);

  auto *rotationAxes = new QWidget(thingPlacementBrushOptions_);
  rotationAxes->setObjectName("thingPlacementBrushField");
  rotationAxes->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
  auto *rotationAxesLayout = new QHBoxLayout(rotationAxes);
  rotationAxesLayout->setContentsMargins(0, 0, 0, 0);
  rotationAxesLayout->setSpacing(4);
  rotationAxesLayout->addWidget(new QLabel("Axes", rotationAxes));
  rotationAxesLayout->addWidget(thingPlacementBrushRotatePitchCheck_);
  rotationAxesLayout->addWidget(thingPlacementBrushRotateYawCheck_);
  rotationAxesLayout->addWidget(thingPlacementBrushRotateRollCheck_);
  brushFlow->addWidget(rotationAxes);

  addBrushField("Angle", thingPlacementBrushFixedAngleSpin_);
  addBrushField("Spacing", thingPlacementBrushSpacingSpin_);
  addBrushField("Scatter", thingPlacementBrushScatterSpin_);
  brushFlow->addWidget(thingPlacementBrushAllowAirCheck_);
  auto enableSpinScrub = [this](QAbstractSpinBox *spin) {
    if (!spin) {
      return;
    }
    spin->setProperty(ui::kSpinScrubMarkerProperty, true);
    spin->setCursor(Qt::SizeHorCursor);
    spin->installEventFilter(this);
    if (QLineEdit *edit = spin->findChild<QLineEdit *>()) {
      edit->setProperty(ui::kSpinScrubMarkerProperty, true);
      edit->setCursor(Qt::SizeHorCursor);
      edit->installEventFilter(this);
    }
  };
  enableSpinScrub(thingPlacementBrushSizeSpin_);
  enableSpinScrub(thingPlacementBrushCountSpin_);
  enableSpinScrub(thingPlacementBrushFixedAngleSpin_);
  enableSpinScrub(thingPlacementBrushSpacingSpin_);
  enableSpinScrub(thingPlacementBrushScatterSpin_);
  optionsLayout->addWidget(thingPlacementBrushOptions_);
  layout->addWidget(optionsHost);

  thingPlacementFilterToggleButton_ = new QToolButton(thingPlacementPanel_);
  thingPlacementFilterToggleButton_->setObjectName(
      "thingPlacementFilterButton");
  thingPlacementFilterToggleButton_->setText("Filters");
  thingPlacementFilterToggleButton_->setCheckable(true);
  thingPlacementFilterToggleButton_->setChecked(thingPlacementFiltersVisible_);
  thingPlacementFilterToggleButton_->setToolButtonStyle(
      Qt::ToolButtonTextBesideIcon);
  thingPlacementFilterToggleButton_->setIconSize(QSize(14, 14));
  thingPlacementFilterToggleButton_->setToolTip(
      "<b>Filters</b><br>Show or hide template search and category filters.");

  thingPlacementFilterHost_ = new QWidget(thingPlacementPanel_);
  thingPlacementFilterHost_->setObjectName("thingPlacementFilterBox");
  auto *filterBoxLayout = new QVBoxLayout(thingPlacementFilterHost_);
  filterBoxLayout->setContentsMargins(0, 0, 0, 0);
  filterBoxLayout->setSpacing(0);
  filterBoxLayout->addWidget(thingPlacementFilterToggleButton_);

  thingPlacementFilterContent_ = new QWidget(thingPlacementFilterHost_);
  thingPlacementFilterContent_->setObjectName("thingPlacementFilterContent");
  auto *filterLayout = new QVBoxLayout(thingPlacementFilterContent_);
  filterLayout->setContentsMargins(6, 6, 6, 6);
  filterLayout->setSpacing(5);
  thingPlacementSearchEdit_ = new QLineEdit(thingPlacementFilterContent_);
  thingPlacementSearchEdit_->setObjectName("thingPlacementSearch");
  thingPlacementSearchEdit_->setPlaceholderText("Search templates");
  thingPlacementSearchEdit_->setClearButtonEnabled(true);
  filterLayout->addWidget(thingPlacementSearchEdit_);

  thingPlacementCategoryList_ = new QListWidget(thingPlacementFilterContent_);
  thingPlacementCategoryList_->setObjectName("thingPlacementCategoryList");
  thingPlacementCategoryList_->setSelectionMode(
      QAbstractItemView::SingleSelection);
  thingPlacementCategoryList_->setViewMode(QListView::IconMode);
  thingPlacementCategoryList_->setFlow(QListView::LeftToRight);
  thingPlacementCategoryList_->setWrapping(true);
  thingPlacementCategoryList_->setMovement(QListView::Static);
  thingPlacementCategoryList_->setResizeMode(QListView::Adjust);
  thingPlacementCategoryList_->setHorizontalScrollBarPolicy(
      Qt::ScrollBarAlwaysOff);
  thingPlacementCategoryList_->setVerticalScrollBarPolicy(
      Qt::ScrollBarAsNeeded);
  thingPlacementCategoryList_->setUniformItemSizes(false);
  thingPlacementCategoryList_->setTextElideMode(Qt::ElideNone);
  thingPlacementCategoryList_->setSpacing(4);
  thingPlacementCategoryList_->setMinimumHeight(38);
  thingPlacementCategoryList_->setMaximumHeight(
      thingPlacementFilterContentHeight_);
  filterLayout->addWidget(thingPlacementCategoryList_);
  filterBoxLayout->addWidget(thingPlacementFilterContent_);
  thingPlacementFilterResizeGrip_ = new QWidget(thingPlacementFilterHost_);
  thingPlacementFilterResizeGrip_->setObjectName(
      "thingPlacementFilterResizeGrip");
  thingPlacementFilterResizeGrip_->setFixedHeight(7);
  thingPlacementFilterResizeGrip_->setCursor(Qt::SizeVerCursor);
  thingPlacementFilterResizeGrip_->installEventFilter(this);
  filterBoxLayout->addWidget(thingPlacementFilterResizeGrip_);
  layout->addWidget(thingPlacementFilterHost_);

  thingPlacementTemplateList_ =
      new ThingPlacementTemplateListWidget(thingPlacementPanel_);
  thingPlacementTemplateList_->setObjectName("thingPlacementTemplateList");
  thingPlacementTemplateList_->setAlternatingRowColors(false);
  thingPlacementTemplateList_->setDragEnabled(true);
  thingPlacementTemplateList_->setDragDropMode(QAbstractItemView::DragOnly);
  thingPlacementTemplateList_->setDefaultDropAction(Qt::CopyAction);
  thingPlacementTemplateList_->setSelectionMode(
      QAbstractItemView::SingleSelection);
  thingPlacementTemplateList_->setUniformItemSizes(true);
  thingPlacementTemplateList_->setViewMode(QListView::IconMode);
  thingPlacementTemplateList_->setResizeMode(QListView::Adjust);
  thingPlacementTemplateList_->setMovement(QListView::Static);
  thingPlacementTemplateList_->setFlow(QListView::LeftToRight);
  thingPlacementTemplateList_->setWrapping(true);
  thingPlacementTemplateList_->setVerticalScrollMode(
      QAbstractItemView::ScrollPerPixel);
  thingPlacementTemplateList_->setHorizontalScrollMode(
      QAbstractItemView::ScrollPerPixel);
  thingPlacementTemplateList_->setWordWrap(true);
  thingPlacementTemplateList_->setTextElideMode(Qt::ElideRight);
  thingPlacementTemplateList_->setSpacing(9);
  thingPlacementTemplateList_->setIconSize(QSize(104, 104));
  thingPlacementTemplateList_->setGridSize(QSize(138, 148));
  thingPlacementTemplateList_->setContextMenuPolicy(Qt::CustomContextMenu);
  if (thingPlacementTemplateList_->viewport()) {
    thingPlacementTemplateList_->viewport()->installEventFilter(this);
  }
  layout->addWidget(thingPlacementTemplateList_, 1);
  thingPlacementTemplateIconLoadTimer_.setSingleShot(true);
  connect(&thingPlacementTemplateIconLoadTimer_, &QTimer::timeout, this,
          [this]() {
            populateThingPlacementTemplateIcons(
                thingPlacementTemplateIconScheduledGeneration_,
                thingPlacementTemplateIconScheduledStartIndex_);
          });
  if (QScrollBar *scroll = thingPlacementTemplateList_->verticalScrollBar()) {
    scroll->setSingleStep(32);
    connect(scroll, &QScrollBar::valueChanged, this, [this](int) {
      if (editMode_ == EditMode::thing && thingPlacementPanelVisible_) {
        thingPlacementTemplateIconPublicationPacer_.noteInteraction();
        const int generation = thingPlacementTemplateIconGeneration_;
        const auto kThumbTuning = ui::detail::sharedModelThumbLoadTuning();
        scheduleThingPlacementTemplateIcons(
            generation, 0, kThumbTuning.visibleDelayMs);
      }
    });
  }
  if (QScrollBar *scroll = thingPlacementTemplateList_->horizontalScrollBar()) {
    connect(scroll, &QScrollBar::valueChanged, this, [this](int) {
      if (editMode_ == EditMode::thing && thingPlacementPanelVisible_) {
        thingPlacementTemplateIconPublicationPacer_.noteInteraction();
        const int generation = thingPlacementTemplateIconGeneration_;
        const auto kThumbTuning = ui::detail::sharedModelThumbLoadTuning();
        scheduleThingPlacementTemplateIcons(
            generation, 0, kThumbTuning.visibleDelayMs);
      }
    });
  }

  thingPlacementSummaryLabel_ = new QLabel(thingPlacementPanel_);
  thingPlacementSummaryLabel_->setObjectName("thingPlacementSummary");
  thingPlacementSummaryLabel_->setWordWrap(true);
  layout->addWidget(thingPlacementSummaryLabel_);

  thingPlacementCollapsedBar_ = new QWidget(glView_);
  thingPlacementCollapsedBar_->setAttribute(Qt::WA_StyledBackground, true);
  thingPlacementCollapsedBar_->setObjectName("thingPlacementCollapsedBar");
  auto *collapsedLayout = new QHBoxLayout(thingPlacementCollapsedBar_);
  collapsedLayout->setContentsMargins(5, 5, 5, 5);
  collapsedLayout->setSpacing(4);
  thingPlacementShowButton_ = new QToolButton(thingPlacementCollapsedBar_);
  thingPlacementShowButton_->setObjectName("thingPlacementShowButton");
  thingPlacementShowButton_->setToolButtonStyle(Qt::ToolButtonIconOnly);
  thingPlacementShowButton_->setIconSize(QSize(18, 18));
  thingPlacementShowButton_->setToolTip(
      "<b>Show Thing palette</b><br>Open the panel to search and place Thing templates.");
  thingPlacementShowButton_->setSizePolicy(QSizePolicy::Expanding,
                                           QSizePolicy::Expanding);
  connect(thingPlacementShowButton_, &QToolButton::clicked, this,
          [this]() { setThingPlacementPanelVisible(true); });
  collapsedLayout->addWidget(thingPlacementShowButton_);

  thingPlacementDockPreviewBand_ =
      new QRubberBand(QRubberBand::Rectangle, glView_);
  thingPlacementDockPreviewBand_->hide();

  thingPlacementPanelAnimation_ = new QVariantAnimation(this);
  thingPlacementPanelAnimation_->setObjectName("thingPlacementPanelAnimation");
  thingPlacementPanelAnimation_->setDuration(130);
  thingPlacementPanelAnimation_->setEasingCurve(QEasingCurve::OutCubic);
  connect(thingPlacementPanelAnimation_, &QVariantAnimation::valueChanged, this,
          [this](const QVariant &value) {
            if (thingPlacementPanel_) {
              thingPlacementPanel_->setGeometry(value.toRect());
            }
            positionViewportOverlay();
            positionViewportEditModeOverlay();
            if (glView_) {
              glView_->update();
            }
          });
  connect(thingPlacementPanelAnimation_, &QVariantAnimation::finished, this,
          [this]() {
            if (thingPlacementPanelCollapsePending_) {
              thingPlacementPanelCollapsePending_ = false;
              if (!thingPlacementPanelVisible_ &&
                  editMode_ == EditMode::thing && thingPlacementPanel_) {
                thingPlacementPanel_->hide();
                positionThingPlacementPanel();
                return;
              }
            }
            positionViewportOverlay();
            positionViewportEditModeOverlay();
            if (glView_) {
              glView_->update();
            }
          });
  thingPlacementCollapsedBarAnimation_ = new QVariantAnimation(this);
  thingPlacementCollapsedBarAnimation_->setDuration(115);
  thingPlacementCollapsedBarAnimation_->setEasingCurve(QEasingCurve::OutCubic);
  connect(thingPlacementCollapsedBarAnimation_,
          &QVariantAnimation::valueChanged, this,
          [this](const QVariant &value) {
            if (thingPlacementCollapsedBar_) {
              thingPlacementCollapsedBar_->setGeometry(value.toRect());
            }
            positionViewportOverlay();
            positionViewportEditModeOverlay();
            if (glView_) {
              glView_->update();
            }
          });
  connect(thingPlacementCollapsedBarAnimation_, &QVariantAnimation::finished,
          this, [this]() {
            positionViewportOverlay();
            positionViewportEditModeOverlay();
            if (glView_) {
              glView_->update();
            }
          });

  thingPlacementPanel_->setStyleSheet(
      "QWidget#thingPlacementPanel {"
      "  background: #12151b;"
      "  border: 1px solid #303641;"
      "  border-radius: 0px;"
      "}"
      "QWidget#thingPlacementHeader {"
      "  background: #1b2028;"
      "  border: 1px solid #303641;"
      "  border-radius: 4px;"
      "  padding: 2px;"
      "}"
      "QLabel#thingPlacementTitle {"
      "  color: #f0f2f6;"
      "  font-weight: 600;"
      "}"
      "QToolButton#thingPlacementHeaderButton {"
      "  color: #eceff4;"
      "  background: #242a34;"
      "  border: 1px solid #3b4350;"
      "  border-radius: 5px;"
      "  padding: 2px 5px;"
      "  min-width: 20px;"
      "}"
      "QToolButton#thingPlacementHeaderButton:hover {"
      "  background: #303846;"
      "}"
      "QToolButton#thingPlacementDockButton {"
      "  color: #eceff4;"
      "  background: #242a34;"
      "  border: 1px solid #3b4350;"
      "  border-radius: 5px;"
      "  min-width: 58px;"
      "  padding: 3px 18px 3px 8px;"
      "}"
      "QToolButton#thingPlacementDockButton::menu-indicator {"
      "  subcontrol-origin: padding;"
      "  subcontrol-position: right center;"
      "  right: 5px;"
      "}"
      "QToolButton#thingPlacementDockButton:hover {"
      "  background: #303846;"
      "}"
      "QLineEdit#thingPlacementSearch {"
      "  background: #1b2028;"
      "  border: 1px solid #303641;"
      "  border-radius: 5px;"
      "  padding: 4px 7px;"
      "  color: #eceff4;"
      "  selection-background-color: #4f79bd;"
      "}"
      "QWidget#thingPlacementFilterBox {"
      "  background: #101319;"
      "  border: 1px solid #303641;"
      "  border-radius: 5px;"
      "}"
      "QWidget#thingPlacementFilterContent {"
      "  background: transparent;"
      "  padding: 6px;"
      "}"
      "QWidget#thingPlacementFilterResizeGrip {"
      "  background: transparent;"
      "  border-top: 1px solid #252b36;"
      "}"
      "QWidget#thingPlacementFilterResizeGrip:hover {"
      "  background: #202633;"
      "}"
      "QToolButton#thingPlacementFilterButton {"
      "  color: #dfe5ef;"
      "  background: #151a22;"
      "  border: 0;"
      "  border-bottom: 1px solid #303641;"
      "  border-radius: 0px;"
      "  padding: 5px 9px;"
      "  text-align: left;"
      "}"
      "QToolButton#thingPlacementFilterButton:hover {"
      "  background: #202633;"
      "}"
      "QToolButton#thingPlacementFilterButton:checked {"
      "  background: #1d2634;"
      "  color: white;"
      "}"
      "QListWidget#thingPlacementCategoryList {"
      "  background: #0d1016;"
      "  border: 1px solid #303641;"
      "  border-radius: 5px;"
      "  color: #d9dee8;"
      "}"
      "QListWidget#thingPlacementCategoryList::item {"
      "  padding: 4px 9px;"
      "  border: 1px solid transparent;"
      "  border-radius: 5px;"
      "  margin: 2px;"
      "}"
      "QListWidget#thingPlacementCategoryList::item:hover {"
      "  background: #222936;"
      "}"
      "QListWidget#thingPlacementCategoryList::item:selected {"
      "  background: #385f9c;"
      "  border: 1px solid #6f99d7;"
      "  color: white;"
      "}"
      "QWidget#thingPlacementSelectedPreview {"
      "  background: #101319;"
      "  border: 1px solid #303641;"
      "  border-radius: 5px;"
      "}"
      "QLabel#thingPlacementSelectedPreviewOverlay {"
      "  color: #ffffff;"
      "  font-weight: 700;"
      "  background: transparent;"
      "  border: 0;"
      "  padding: 2px 3px;"
      "}"
      "QLabel#thingPlacementSelectedPreviewImage {"
      "  background: #0b0d12;"
      "  border: 1px solid #252b36;"
      "  border-radius: 4px;"
      "}"
      "QLabel#thingPlacementSelectedPreviewName {"
      "  color: #eceff4;"
      "  font-weight: 600;"
      "}"
      "QWidget#thingPlacementOptions {"
      "  color: #d7dde8;"
      "}"
      "QGroupBox#thingPlacementPlacementSection,"
      "QGroupBox#thingPlacementToolSection,"
      "QGroupBox#thingPlacementBrushOptions {"
      "  background: #151a22;"
      "  border: 1px solid #303641;"
      "  border-radius: 5px;"
      "  color: #c9d1dd;"
      "  font-weight: 600;"
      "  margin-top: 8px;"
      "  padding-top: 5px;"
      "}"
      "QGroupBox#thingPlacementPlacementSection::title,"
      "QGroupBox#thingPlacementToolSection::title,"
      "QGroupBox#thingPlacementBrushOptions::title {"
      "  background: #151a22;"
      "  color: #c9d1dd;"
      "  left: 7px;"
      "  padding: 0 4px;"
      "  subcontrol-origin: margin;"
      "}"
      "QWidget#thingPlacementBrushField {"
      "  background: transparent;"
      "}"
      "QWidget#thingPlacementLayerField {"
      "  background: transparent;"
      "}"
      "QWidget#thingPlacementOptions QLabel {"
      "  color: #aeb7c6;"
      "}"
      "QCheckBox#thingPlacementOptionCheck, QWidget#thingPlacementBrushOptions "
      "QCheckBox {"
      "  color: #d7dde8;"
      "}"
      "QToolButton#thingPlacementSegmentButton {"
      "  color: #dfe5ef;"
      "  background: #202633;"
      "  border: 1px solid #303a4a;"
      "  border-radius: 5px;"
      "  padding: 3px 8px;"
      "  min-width: 46px;"
      "}"
      "QToolButton#thingPlacementSegmentButton:hover {"
      "  background: #293243;"
      "}"
      "QToolButton#thingPlacementSegmentButton:checked {"
      "  background: #3d659f;"
      "  border-color: #79a6e8;"
      "  color: white;"
      "}"
      "QWidget#thingPlacementPlacementSection QComboBox,"
      "QWidget#thingPlacementBrushOptions QSpinBox,"
      "QWidget#thingPlacementBrushOptions QDoubleSpinBox,"
      "QWidget#thingPlacementBrushOptions QComboBox {"
      "  background: #1b2028;"
      "  border: 1px solid #303641;"
      "  border-radius: 4px;"
      "  color: #eceff4;"
      "  padding: 2px 5px;"
      "}"
      "QWidget#thingPlacementPlacementSection QComboBox,"
      "QWidget#thingPlacementBrushOptions QComboBox {"
      "  padding-right: 27px;"
      "}"
      "QWidget#thingPlacementBrushOptions QSpinBox,"
      "QWidget#thingPlacementBrushOptions QDoubleSpinBox {"
      "  padding-right: 24px;"
      "}"
      "QWidget#thingPlacementPlacementSection QComboBox::drop-down,"
      "QWidget#thingPlacementBrushOptions QComboBox::drop-down {"
      "  background: #252c38;"
      "  border: 0;"
      "  border-left: 1px solid #303a48;"
      "  width: 22px;"
      "}"
      "QWidget#thingPlacementPlacementSection QComboBox::drop-down:hover,"
      "QWidget#thingPlacementBrushOptions QComboBox::drop-down:hover {"
      "  background: #303a49;"
      "}"
      "QWidget#thingPlacementBrushOptions QSpinBox::up-button,"
      "QWidget#thingPlacementBrushOptions QSpinBox::down-button,"
      "QWidget#thingPlacementBrushOptions QDoubleSpinBox::up-button,"
      "QWidget#thingPlacementBrushOptions QDoubleSpinBox::down-button {"
      "  background: #252c38;"
      "  border: 0;"
      "  border-left: 1px solid #303a48;"
      "  width: 20px;"
      "}"
      "QWidget#thingPlacementBrushOptions QSpinBox::down-button,"
      "QWidget#thingPlacementBrushOptions QDoubleSpinBox::down-button {"
      "  border-top: 1px solid #303a48;"
      "}"
      "QWidget#thingPlacementBrushOptions QSpinBox::up-button:hover,"
      "QWidget#thingPlacementBrushOptions QSpinBox::down-button:hover,"
      "QWidget#thingPlacementBrushOptions QDoubleSpinBox::up-button:hover,"
      "QWidget#thingPlacementBrushOptions QDoubleSpinBox::down-button:hover {"
      "  background: #303a49;"
      "}"
      "QWidget#thingPlacementPlacementSection QComboBox:disabled,"
      "QWidget#thingPlacementBrushOptions QSpinBox:disabled,"
      "QWidget#thingPlacementBrushOptions QDoubleSpinBox:disabled,"
      "QWidget#thingPlacementBrushOptions QComboBox:disabled {"
      "  background: #0f1319;"
      "  border: 1px solid #232a35;"
      "  color: #687384;"
      "}"
      "QWidget#thingPlacementBrushOptions QCheckBox:disabled {"
      "  color: #687384;"
      "}"
      "QListWidget#thingPlacementTemplateList {"
      "  background: #0b0d12;"
      "  border: 1px solid #303641;"
      "  border-radius: 5px;"
      "  color: #eceff4;"
      "}"
      "QListWidget#thingPlacementTemplateList::item {"
      "  padding: 6px;"
      "  border: 1px solid #252b36;"
      "  border-radius: 7px;"
      "  margin: 2px;"
      "}"
      "QListWidget#thingPlacementTemplateList::item:hover {"
      "  background: #202733;"
      "}"
      "QListWidget#thingPlacementTemplateList::item:selected {"
      "  background: #385f9c;"
      "  color: white;"
      "  border: 1px solid #7da8e8;"
      "  border-radius: 7px;"
      "}"
      "QLabel#thingPlacementSummary {"
      "  color: #cdd3df;"
      "  font-size: 11px;"
      "}");

  thingPlacementCollapsedBar_->setStyleSheet(
      "QWidget#thingPlacementCollapsedBar {"
      "  background: #12151b;"
      "  border: 1px solid #303641;"
      "}"
      "QToolButton#thingPlacementShowButton {"
      "  color: #eceff4;"
      "  background: #1b2028;"
      "  border: 1px solid #3b4350;"
      "  border-radius: 4px;"
      "  padding: 4px 8px;"
      "}"
      "QToolButton#thingPlacementShowButton:hover {"
      "  background: #303846;"
      "}");

  connect(thingPlacementSearchEdit_, &QLineEdit::textChanged, this,
          [this](const QString &text) {
            (void)text;
            thingPlacementTemplateIconPublicationPacer_.noteInteraction();
            applyThingPlacementTemplateFilters();
          });

  connect(thingPlacementCategoryList_, &QListWidget::currentItemChanged, this,
          [this](QListWidgetItem *, QListWidgetItem *) {
            applyThingPlacementTemplateFilters();
          });

  if (thingPlacementKeepActiveCheck_) {
    connect(thingPlacementKeepActiveCheck_, &QCheckBox::toggled, this,
            [this](const bool checked) {
              thingPlacementKeepActive_ = checked;
              saveThingPlacementOptions();
              syncThingPlacementOptionControls();
            });
  }

  auto connectDefaultModeButton = [this](QToolButton *button,
                                         const ThingPlacementDefaultMode mode) {
    if (!button) {
      return;
    }
    connect(button, &QToolButton::clicked, this, [this, mode]() {
      thingPlacementDefaultMode_ = mode;
      saveThingPlacementOptions();
      syncThingPlacementOptionControls();
      if (glView_ && placementMode_ == PlacementMode::thing) {
        updateThingPlacementPreview(glView_->mapFromGlobal(QCursor::pos()),
                                    QApplication::keyboardModifiers());
      }
    });
  };
  connectDefaultModeButton(thingPlacementModeSurfaceButton_,
                           ThingPlacementDefaultMode::surface);
  connectDefaultModeButton(thingPlacementModeTiltButton_,
                           ThingPlacementDefaultMode::surfaceTilt);
  connectDefaultModeButton(thingPlacementModeFreeButton_,
                           ThingPlacementDefaultMode::free);

  if (thingPlacementLayerCombo_) {
    connect(
        thingPlacementLayerCombo_,
        QOverload<int>::of(&QComboBox::currentIndexChanged), this,
        [this](const int index) {
          thingPlacementLayerOverride_ =
              index >= 0 ? thingPlacementLayerCombo_->itemData(index).toInt()
                         : -1;
          if (glView_ && placementMode_ == PlacementMode::thing) {
            updateThingPlacementPreview(glView_->mapFromGlobal(QCursor::pos()),
                                        QApplication::keyboardModifiers());
          }
        });
  }

  if (thingPlacementSnapToThingCheck_) {
    connect(thingPlacementSnapToThingCheck_, &QCheckBox::toggled, this,
            [this](const bool checked) {
              thingPlacementSnapToThing_ = checked;
              saveThingPlacementOptions();
              syncThingPlacementOptionControls();
              if (glView_ && placementMode_ == PlacementMode::thing) {
                updateThingPlacementPreview(
                    glView_->mapFromGlobal(QCursor::pos()),
                    QApplication::keyboardModifiers());
              }
            });
  }
  if (thingPlacementSnapRotateCheck_) {
    connect(thingPlacementSnapRotateCheck_, &QCheckBox::toggled, this,
            [this](const bool checked) {
              thingPlacementSnapRotate_ = checked;
              saveThingPlacementOptions();
              syncThingPlacementOptionControls();
              if (glView_ && placementMode_ == PlacementMode::thing) {
                updateThingPlacementPreview(
                    glView_->mapFromGlobal(QCursor::pos()),
                    QApplication::keyboardModifiers());
              }
            });
  }

  auto connectToolModeButton = [this](QToolButton *button,
                                      const ThingPlacementToolMode mode) {
    if (!button) {
      return;
    }
    connect(button, &QToolButton::clicked, this, [this, mode]() {
      if (thingPlacementToolMode_ == ThingPlacementToolMode::brush &&
          mode != ThingPlacementToolMode::brush &&
          thingPlacementBrushStrokeActive_) {
        (void)finalizeThingPlacementBrushStroke();
      }
      thingPlacementToolMode_ = mode;
      saveThingPlacementOptions();
      syncThingPlacementOptionControls();
      if (glView_ && placementMode_ == PlacementMode::thing) {
        glView_->setPlacementGestureEnabled(mode ==
                                            ThingPlacementToolMode::brush);
        updateThingPlacementPreview(glView_->mapFromGlobal(QCursor::pos()),
                                    QApplication::keyboardModifiers());
      }
    });
  };
  connectToolModeButton(thingPlacementToolSingleButton_,
                        ThingPlacementToolMode::single);
  connectToolModeButton(thingPlacementToolBrushButton_,
                        ThingPlacementToolMode::brush);

  if (thingPlacementBrushSizeSpin_) {
    connect(thingPlacementBrushSizeSpin_,
            QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
            [this](const double value) {
              thingPlacementBrushSize_ = value;
              saveThingPlacementOptions();
              if (glView_ && placementMode_ == PlacementMode::thing) {
                updateThingPlacementPreview(
                    glView_->mapFromGlobal(QCursor::pos()),
                    QApplication::keyboardModifiers());
              }
            });
  }
  if (thingPlacementBrushCountSpin_) {
    connect(thingPlacementBrushCountSpin_,
            QOverload<int>::of(&QSpinBox::valueChanged), this,
            [this](const int value) {
              thingPlacementBrushCount_ = value;
              saveThingPlacementOptions();
              if (glView_ && placementMode_ == PlacementMode::thing) {
                updateThingPlacementPreview(
                    glView_->mapFromGlobal(QCursor::pos()),
                    QApplication::keyboardModifiers());
              }
            });
  }
  if (thingPlacementBrushShapeCombo_) {
    connect(
        thingPlacementBrushShapeCombo_,
        QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) {
          thingPlacementBrushShape_ = static_cast<ThingPlacementBrushShape>(
              thingPlacementBrushShapeCombo_->currentData().toInt());
          saveThingPlacementOptions();
          if (glView_ && placementMode_ == PlacementMode::thing) {
            updateThingPlacementPreview(glView_->mapFromGlobal(QCursor::pos()),
                                        QApplication::keyboardModifiers());
          }
        });
  }
  auto connectBrushCheck = [this](QCheckBox *check, bool MainWindow::*target) {
    if (!check) {
      return;
    }
    connect(
        check, &QCheckBox::toggled, this, [this, target](const bool checked) {
          this->*target = checked;
          saveThingPlacementOptions();
          syncThingPlacementOptionControls();
          if (glView_ && placementMode_ == PlacementMode::thing) {
            updateThingPlacementPreview(glView_->mapFromGlobal(QCursor::pos()),
                                        QApplication::keyboardModifiers());
          }
        });
  };
  connectBrushCheck(thingPlacementBrushRandomRotateCheck_,
                    &MainWindow::thingPlacementBrushRandomRotation_);
  connectBrushCheck(thingPlacementBrushRotatePitchCheck_,
                    &MainWindow::thingPlacementBrushRotatePitch_);
  connectBrushCheck(thingPlacementBrushRotateYawCheck_,
                    &MainWindow::thingPlacementBrushRotateYaw_);
  connectBrushCheck(thingPlacementBrushRotateRollCheck_,
                    &MainWindow::thingPlacementBrushRotateRoll_);
  if (thingPlacementBrushFixedAngleSpin_) {
    connect(thingPlacementBrushFixedAngleSpin_,
            QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
            [this](const double value) {
              thingPlacementBrushFixedAngle_ = value;
              saveThingPlacementOptions();
            });
  }

  if (thingPlacementBrushSpacingSpin_) {
    connect(thingPlacementBrushSpacingSpin_,
            QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
            [this](const double value) {
              thingPlacementBrushSpacing_ = value;
              saveThingPlacementOptions();
            });
  }
  if (thingPlacementBrushScatterSpin_) {
    connect(thingPlacementBrushScatterSpin_,
            QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
            [this](const double value) {
              thingPlacementBrushScatter_ = value;
              saveThingPlacementOptions();
              if (glView_ && placementMode_ == PlacementMode::thing) {
                updateThingPlacementPreview(
                    glView_->mapFromGlobal(QCursor::pos()),
                    QApplication::keyboardModifiers());
              }
            });
  }
  connectBrushCheck(thingPlacementBrushAllowAirCheck_,
                    &MainWindow::thingPlacementBrushAllowAir_);
  if (thingPlacementFilterToggleButton_) {
    connect(thingPlacementFilterToggleButton_, &QToolButton::toggled, this,
            [this](const bool checked) {
              thingPlacementFiltersVisible_ = checked;
              saveThingPlacementOptions();
              syncThingPlacementOptionControls();
            });
  }

  connect(
      thingPlacementTemplateList_, &QListWidget::currentItemChanged, this,
      [this](QListWidgetItem *current, QListWidgetItem *) {
        if (!current) {
          return;
        }

        const QString name = current->data(Qt::UserRole).toString().trimmed();
        if (name.isEmpty()) {
          return;
        }

        const std::string nextName = name.toStdString();
        if (selectedThingTemplateName_ != nextName) {
          thingPlacementYawOffset_ = 0.0;
        }
        selectedThingTemplateName_ = nextName;
        updateThingPlacementSelectedPreview();
        if (placementMode_ != PlacementMode::thing) {
          setPlacementMode(PlacementMode::thing);
        }
        syncThingPlacementPanel();
        if (placementMode_ == PlacementMode::thing && glView_) {
          updateThingPlacementPreview(glView_->mapFromGlobal(QCursor::pos()),
                                      QApplication::keyboardModifiers());
        }
      });

  connect(thingPlacementTemplateList_, &QListWidget::customContextMenuRequested,
          this, [this](const QPoint &pos) {
            if (!thingPlacementTemplateList_) {
              return;
            }

            ui::shortcuts::KeySequenceDisplayMenu menu(thingPlacementTemplateList_);
            QAction *deselectAction = menu.addAction("Deselect template");
            deselectAction->setEnabled(!selectedThingTemplateName_.empty());
            QAction *chosen = menu.exec(
                thingPlacementTemplateList_->viewport()->mapToGlobal(pos));
            if (chosen == deselectAction) {
              deselectThingPlacementTemplate();
            }
          });

  thingPlacementResourceDiskWatcher_ =
      new ui::detail::PreviewResourceDiskWatcher(
          this, [this]() { invalidateThingPlacementResourcePreviews(); });
  // MainWindow::glView_ owns mounted-archive remounts.  The placement
  // watcher retains loose dependency files and directories for overrides,
  // without creating a second archive retry/remount path.
  thingPlacementResourceDiskWatcher_->setMountedArchiveWatchingEnabled(false);
  thingPlacementResourceDiskWatcher_->setResourceLocator(&resources_);
  thingPlacementResourceDiskWatcher_->watchResource(
      document_.level().getMasterCMP(), ".cmp");

  refreshThingPlacementTemplateList();
  refreshThingPlacementLayerControl();
  syncThingPlacementOptionControls();
  updateThingPlacementSelectedPreview();
  syncThingPlacementPanel();
}

bool MainWindow::thingPlacementPointerInsidePanel() const {
  const QPoint globalPos = QCursor::pos();
  return ui::viewport::visibleWidgetContainsGlobalPoint(thingPlacementPanel_,
                                                        globalPos) ||
         ui::viewport::visibleWidgetContainsGlobalPoint(
             thingPlacementCollapsedBar_, globalPos);
}

QString MainWindow::thingPlacementModeLabel(
    const ThingPlacementDefaultMode mode) const {
  switch (mode) {
  case ThingPlacementDefaultMode::surfaceTilt:
    return QStringLiteral("surface tilt");
  case ThingPlacementDefaultMode::free:
    return QStringLiteral("free insert");
  case ThingPlacementDefaultMode::surface:
  default:
    return QStringLiteral("surface attach");
  }
}

void MainWindow::saveThingPlacementOptions() const {
  QSettings settings = settingsStore();
  settings.setValue("thing_placement/keep_active", thingPlacementKeepActive_);
  settings.setValue("thing_placement/snap_to_thing",
                    thingPlacementSnapToThing_);
  settings.setValue("thing_placement/snap_rotate", thingPlacementSnapRotate_);
  settings.setValue("thing_placement/filters_visible",
                    thingPlacementFiltersVisible_);
  switch (thingPlacementDefaultMode_) {
  case ThingPlacementDefaultMode::surfaceTilt:
    settings.setValue("thing_placement/default_mode", QStringLiteral("tilt"));
    break;
  case ThingPlacementDefaultMode::free:
    settings.setValue("thing_placement/default_mode", QStringLiteral("free"));
    break;
  case ThingPlacementDefaultMode::surface:
  default:
    settings.setValue("thing_placement/default_mode",
                      QStringLiteral("surface"));
    break;
  }
  settings.setValue("thing_placement/tool_mode",
                    thingPlacementToolMode_ == ThingPlacementToolMode::brush
                        ? QStringLiteral("brush")
                        : QStringLiteral("single"));
  switch (thingPlacementBrushShape_) {
  case ThingPlacementBrushShape::square:
    settings.setValue("thing_placement/brush_shape", QStringLiteral("square"));
    break;
  case ThingPlacementBrushShape::rectangle:
    settings.setValue("thing_placement/brush_shape",
                      QStringLiteral("rectangle"));
    break;
  case ThingPlacementBrushShape::circle:
  default:
    settings.setValue("thing_placement/brush_shape", QStringLiteral("circle"));
    break;
  }
  settings.setValue("thing_placement/brush_size", thingPlacementBrushSize_);
  settings.setValue("thing_placement/brush_count", thingPlacementBrushCount_);
  settings.setValue("thing_placement/brush_spacing",
                    thingPlacementBrushSpacing_);
  settings.setValue("thing_placement/brush_scatter",
                    thingPlacementBrushScatter_);
  settings.setValue("thing_placement/brush_random_rotation",
                    thingPlacementBrushRandomRotation_);
  settings.setValue("thing_placement/brush_rotate_pitch",
                    thingPlacementBrushRotatePitch_);
  settings.setValue("thing_placement/brush_rotate_yaw",
                    thingPlacementBrushRotateYaw_);
  settings.setValue("thing_placement/brush_rotate_roll",
                    thingPlacementBrushRotateRoll_);
  settings.setValue("thing_placement/brush_fixed_angle",
                    thingPlacementBrushFixedAngle_);
  settings.setValue("thing_placement/brush_allow_air",
                    thingPlacementBrushAllowAir_);
  settings.setValue("thing_placement/filter_content_height",
                    thingPlacementFilterContentHeight_);
  settings.setValue("thing_placement/panel_side_size",
                    thingPlacementPanelSideSize_);
  settings.setValue("thing_placement/panel_band_size",
                    thingPlacementPanelBandSize_);
}

void MainWindow::refreshThingPlacementLayerControl() {
  if (!thingPlacementLayerCombo_) {
    return;
  }

  const Level &level = document_.level();
  const std::size_t layerCount = std::max<std::size_t>(1, level.layers.size());
  if (thingPlacementLayerOverride_ < 0 ||
      static_cast<std::size_t>(thingPlacementLayerOverride_) >= layerCount) {
    thingPlacementLayerOverride_ = -1;
  }

  const QSignalBlocker blocker(thingPlacementLayerCombo_);
  thingPlacementLayerCombo_->clear();
  thingPlacementLayerCombo_->addItem(QStringLiteral("Auto"), -1);
  for (std::size_t index = 0; index < layerCount; ++index) {
    QString name = QStringLiteral("Layer%1").arg(static_cast<int>(index));
    if (index < level.layers.size()) {
      const QString authored =
          QString::fromStdString(level.layers[index]).trimmed();
      if (!authored.isEmpty()) {
        name = authored;
      }
    }

    QString status{};
    if (index < layerVisible_.size() && !layerVisible_[index]) {
      status += QStringLiteral("hidden");
    }
    if (index < layerLocked_.size() && layerLocked_[index]) {
      if (!status.isEmpty()) {
        status += QStringLiteral(", ");
      }
      status += QStringLiteral("locked");
    }
    if (!status.isEmpty()) {
      status = QStringLiteral(" (%1)").arg(status);
    }
    thingPlacementLayerCombo_->addItem(QStringLiteral("%1: %2%3")
                                           .arg(static_cast<int>(index))
                                           .arg(name)
                                           .arg(status),
                                       static_cast<int>(index));
  }

  const int selected =
      thingPlacementLayerCombo_->findData(thingPlacementLayerOverride_);
  thingPlacementLayerCombo_->setCurrentIndex(std::max(0, selected));
}

void MainWindow::syncThingPlacementOptionControls() {
  auto setFoldExpanded = [this](QWidget *widget, const bool expanded,
                                const int durationMs) {
    if (!widget) {
      return;
    }

    constexpr int kMaxWidgetHeight = 16777215;
    const bool animate = thingPlacementPanel_ &&
                         thingPlacementPanel_->isVisible() &&
                         widget->isVisibleTo(thingPlacementPanel_);
    if (!animate) {
      widget->setVisible(expanded);
      widget->setMaximumHeight(expanded ? kMaxWidgetHeight : 0);
      return;
    }

    const int targetHeight = expanded ? widget->sizeHint().height() : 0;
    const bool alreadyAtTarget =
        (expanded && widget->isVisible() &&
         widget->maximumHeight() == kMaxWidgetHeight) ||
        (!expanded && (!widget->isVisible() || widget->maximumHeight() == 0));
    if (alreadyAtTarget) {
      return;
    }

    const QList<QVariantAnimation *> existing =
        widget->findChildren<QVariantAnimation *>(
            QStringLiteral("sedThingPlacementFoldAnimation"));
    for (QVariantAnimation *animation : existing) {
      animation->stop();
      animation->deleteLater();
    }

    widget->setVisible(true);
    const int startHeight = widget->height() > 0
                                ? widget->height()
                                : (expanded ? 0 : widget->sizeHint().height());
    widget->setMaximumHeight(startHeight);

    auto *animation = new QVariantAnimation(widget);
    animation->setObjectName(QStringLiteral("sedThingPlacementFoldAnimation"));
    animation->setDuration(durationMs);
    animation->setEasingCurve(QEasingCurve::OutCubic);
    animation->setStartValue(startHeight);
    animation->setEndValue(targetHeight);
    connect(animation, &QVariantAnimation::valueChanged, widget,
            [widget, this](const QVariant &value) {
              widget->setMaximumHeight(value.toInt());
              widget->updateGeometry();
              if (thingPlacementPanel_) {
                thingPlacementPanel_->updateGeometry();
              }
            });
    connect(animation, &QVariantAnimation::finished, widget,
            [widget, expanded]() {
              widget->setMaximumHeight(expanded ? kMaxWidgetHeight : 0);
              widget->setVisible(expanded);
            });
    animation->start(QAbstractAnimation::DeleteWhenStopped);
  };

  auto setChecked = [](QAbstractButton *button, const bool checked) {
    if (!button) {
      return;
    }
    const QSignalBlocker blocker(button);
    button->setChecked(checked);
  };

  setChecked(thingPlacementKeepActiveCheck_, thingPlacementKeepActive_);
  setChecked(thingPlacementSnapToThingCheck_, thingPlacementSnapToThing_);
  setChecked(thingPlacementSnapRotateCheck_, thingPlacementSnapRotate_);
  if (thingPlacementSnapRotateCheck_) {
    thingPlacementSnapRotateCheck_->setEnabled(thingPlacementSnapToThing_);
  }
  setChecked(thingPlacementFilterToggleButton_, thingPlacementFiltersVisible_);
  if (thingPlacementFilterToggleButton_) {
    thingPlacementFilterToggleButton_->setIcon(makeThingPlacementChevronIcon(
        thingPlacementFiltersVisible_ ? 180.0 : 0.0, QColor(188, 202, 226)));
  }
  setFoldExpanded(thingPlacementFilterContent_, thingPlacementFiltersVisible_,
                  130);
  if (thingPlacementCategoryList_) {
    thingPlacementCategoryList_->setMaximumHeight(
        thingPlacementFilterContentHeight_);
  }
  if (thingPlacementFilterResizeGrip_) {
    thingPlacementFilterResizeGrip_->setVisible(thingPlacementFiltersVisible_);
  }
  setChecked(thingPlacementModeSurfaceButton_,
             thingPlacementDefaultMode_ == ThingPlacementDefaultMode::surface);
  setChecked(thingPlacementModeTiltButton_,
             thingPlacementDefaultMode_ ==
                 ThingPlacementDefaultMode::surfaceTilt);
  setChecked(thingPlacementModeFreeButton_,
             thingPlacementDefaultMode_ == ThingPlacementDefaultMode::free);
  setChecked(thingPlacementToolSingleButton_,
             thingPlacementToolMode_ == ThingPlacementToolMode::single);
  setChecked(thingPlacementToolBrushButton_,
             thingPlacementToolMode_ == ThingPlacementToolMode::brush);

  setFoldExpanded(thingPlacementBrushOptions_,
                  thingPlacementToolMode_ == ThingPlacementToolMode::brush,
                  150);
  if (thingPlacementBrushSizeSpin_) {
    const QSignalBlocker blocker(thingPlacementBrushSizeSpin_);
    thingPlacementBrushSizeSpin_->setValue(thingPlacementBrushSize_);
  }
  if (thingPlacementBrushCountSpin_) {
    const QSignalBlocker blocker(thingPlacementBrushCountSpin_);
    thingPlacementBrushCountSpin_->setValue(thingPlacementBrushCount_);
  }
  if (thingPlacementBrushSpacingSpin_) {
    const QSignalBlocker blocker(thingPlacementBrushSpacingSpin_);
    thingPlacementBrushSpacingSpin_->setValue(thingPlacementBrushSpacing_);
  }
  if (thingPlacementBrushScatterSpin_) {
    const QSignalBlocker blocker(thingPlacementBrushScatterSpin_);
    thingPlacementBrushScatterSpin_->setValue(thingPlacementBrushScatter_);
  }
  if (thingPlacementBrushShapeCombo_) {
    const QSignalBlocker blocker(thingPlacementBrushShapeCombo_);
    const int target = static_cast<int>(thingPlacementBrushShape_);
    const int index = thingPlacementBrushShapeCombo_->findData(target);
    thingPlacementBrushShapeCombo_->setCurrentIndex(std::max(0, index));
  }
  setChecked(thingPlacementBrushRandomRotateCheck_,
             thingPlacementBrushRandomRotation_);
  setChecked(thingPlacementBrushRotatePitchCheck_,
             thingPlacementBrushRotatePitch_);
  setChecked(thingPlacementBrushRotateYawCheck_, thingPlacementBrushRotateYaw_);
  setChecked(thingPlacementBrushRotateRollCheck_,
             thingPlacementBrushRotateRoll_);
  setChecked(thingPlacementBrushAllowAirCheck_, thingPlacementBrushAllowAir_);
  if (thingPlacementBrushFixedAngleSpin_) {
    const QSignalBlocker blocker(thingPlacementBrushFixedAngleSpin_);
    thingPlacementBrushFixedAngleSpin_->setValue(
        thingPlacementBrushFixedAngle_);
    thingPlacementBrushFixedAngleSpin_->setEnabled(
        !thingPlacementBrushRandomRotation_);
  }
  if (glView_ && placementMode_ == PlacementMode::thing) {
    glView_->setPlacementGestureEnabled(thingPlacementToolMode_ ==
                                        ThingPlacementToolMode::brush);
  }
}

void MainWindow::updateThingPlacementSelectedPreview() {
  if (!thingPlacementSelectedPreviewHost_ ||
      !thingPlacementSelectedPreviewName_) {
    return;
  }

  auto *previewWidget = dynamic_cast<ThingPlacementSelectedPreviewWidget *>(
      thingPlacementSelectedPreviewWidget_);
  QListWidgetItem *current = thingPlacementTemplateList_
                                 ? thingPlacementTemplateList_->currentItem()
                                 : nullptr;
  if (!current || selectedThingTemplateName_.empty()) {
    if (previewWidget) {
      previewWidget->clearPreview(QStringLiteral("No template selected"));
      previewWidget->setOverlayText({});
      previewWidget->setSpinActive(false);
    }
    thingPlacementSelectedPreviewName_->setText(
        QStringLiteral("No template selected"));
    thingPlacementSelectedPreviewHost_->show();
    return;
  }

  const int row = thingPlacementTemplateList_
                      ? thingPlacementTemplateList_->row(current)
                      : -1;
  if (row >= 0 &&
      current->data(kThingPlacementTemplatePreviewPendingRole).toBool()) {
    (void)loadThingPlacementTemplateIconForRow(row);
  }

  const QString name = current->data(Qt::UserRole).toString().trimmed();
  if (const ThingTemplate *tpl = templates_.find(name.toStdString())) {
    if (previewWidget) {
      previewWidget->setPreviewTemplate(templates_, *tpl,
                                        document_.level().kind,
                                        document_.level().getMasterCMP());
    }
  } else if (previewWidget) {
    previewWidget->clearPreview(
        QStringLiteral("No previewable model or sprite."));
    previewWidget->setSpinActive(false);
  }

  if (thingPlacementSelectedPreviewImage_) {
    const QSize imageSize = thingPlacementSelectedPreviewImage_->size();
    QIcon previewIcon = current->icon();
    if (const ThingTemplate *tpl = templates_.find(name.toStdString())) {
      previewIcon = makeThingPlacementTemplateIcon(
          *tpl, templates_, resources_, document_.level().kind,
          document_.level().getMasterCMP(), imageSize,
          thingPlacementTemplatePreviewIconCache_);
    }
    thingPlacementSelectedPreviewImage_->setPixmap(
        previewIcon.pixmap(imageSize));
  }

  QString type =
      current->data(kThingPlacementTemplateTypeRole).toString().trimmed();
  if (type.compare(QStringLiteral("Other"), Qt::CaseInsensitive) == 0) {
    type.clear();
  }
  thingPlacementSelectedPreviewName_->setText(
      type.isEmpty() ? name : QStringLiteral("%1\n%2").arg(name, type));
  if (previewWidget) {
    previewWidget->setOverlayText(thingPlacementSelectedPreviewName_->text());
  }
  thingPlacementSelectedPreviewHost_->show();
  if (previewWidget) {
    // The panel can remain constructed but hidden/collapsed for most of
    // the editor session. Keep the idle model orbit event-driven instead
    // of paying a 33 ms timer wakeup while that panel is not visible.
    previewWidget->setSpinActive(
        thingPlacementPanelVisible_ &&
        thingPlacementSelectedPreviewHost_->isVisible());
  }
}

void MainWindow::invalidateThingPlacementResourcePreviews() {
  ui::detail::clearSharedPreviewAssetCaches();
  thingPlacementTemplatePreviewIconCache_.clear();
  thingPlacementModelThumbnailRenderer_.reset();
  const int generation = ++thingPlacementTemplateIconGeneration_;

  if (auto *previewWidget = dynamic_cast<ThingPlacementSelectedPreviewWidget *>(
          thingPlacementSelectedPreviewWidget_)) {
    previewWidget->invalidateResourceCaches();
  }

  if (thingPlacementTemplateList_) {
    const QIcon loadingIcon(ui::detail::makePreviewLoadingPixmap(
        thingPlacementTemplateList_->iconSize()));
    for (int row = 0; row < thingPlacementTemplateList_->count(); ++row) {
      if (QListWidgetItem *item = thingPlacementTemplateList_->item(row)) {
        item->setIcon(loadingIcon);
        item->setData(kThingPlacementTemplatePreviewPendingRole, true);
      }
    }
  }

  updateThingPlacementSelectedPreview();
  if (editMode_ == EditMode::thing && thingPlacementPanelVisible_) {
    const auto kThumbTuning = ui::detail::sharedModelThumbLoadTuning();
    QTimer::singleShot(kThumbTuning.startDelayMs, this, [this, generation]() {
      populateThingPlacementTemplateIcons(generation, 0);
    });
  }
}

void MainWindow::positionThingPlacementPanel() {
  if (!glView_ || !thingPlacementPanel_ || !thingPlacementCollapsedBar_) {
    return;
  }

  if (!documentEditingAllowed() && !previewEditingUiRestoreInProgress_) {
    if (thingPlacementPanelAnimation_) {
      thingPlacementPanelAnimation_->stop();
    }
    if (thingPlacementCollapsedBarAnimation_) {
      thingPlacementCollapsedBarAnimation_->stop();
    }
    thingPlacementPanelCollapsePending_ = false;
    thingPlacementPanel_->hide();
    thingPlacementCollapsedBar_->hide();
    return;
  }

  if (editMode_ != EditMode::thing) {
    thingPlacementPanelCollapsePending_ = false;
    if (thingPlacementPanelAnimation_) {
      thingPlacementPanelAnimation_->stop();
    }
    if (thingPlacementCollapsedBarAnimation_) {
      thingPlacementCollapsedBarAnimation_->stop();
    }
    thingPlacementPanel_->hide();
    thingPlacementCollapsedBar_->hide();
    positionViewportOverlay();
    positionViewportEditModeOverlay();
    if (glView_) {
      glView_->update();
    }
    return;
  }

  const int viewWidth = std::max(1, glView_->width());
  const int viewHeight = std::max(1, glView_->height());
  const bool sideDock =
      thingPlacementDockPosition_ == ThingPlacementDockPosition::left ||
      thingPlacementDockPosition_ == ThingPlacementDockPosition::right;
  const int defaultSideSize =
      std::clamp(static_cast<int>(static_cast<double>(viewWidth) * 0.28), 320,
                 std::min(560, viewWidth));
  const int defaultBandSize =
      std::clamp(static_cast<int>(static_cast<double>(viewHeight) * 0.34), 240,
                 std::min(420, viewHeight));
  const int minSideSize = std::min(340, viewWidth);
  const int maxSideSize = std::max(minSideSize, viewWidth - 48);
  const int minBandSize = std::min(330, viewHeight);
  const int maxBandSize = std::max(minBandSize, viewHeight - 48);
  const int sideSize =
      std::clamp(thingPlacementPanelSideSize_ > 0 ? thingPlacementPanelSideSize_
                                                  : defaultSideSize,
                 minSideSize, maxSideSize);
  const int bandSize =
      std::clamp(thingPlacementPanelBandSize_ > 0 ? thingPlacementPanelBandSize_
                                                  : defaultBandSize,
                 minBandSize, maxBandSize);
  const int panelWidth = sideDock ? sideSize : viewWidth;
  const int panelHeight = sideDock ? viewHeight : bandSize;

  int x = 0;
  int y = 0;
  switch (thingPlacementDockPosition_) {
  case ThingPlacementDockPosition::right: {
    x = viewWidth - panelWidth;
    break;
  }
  case ThingPlacementDockPosition::bottom: {
    y = viewHeight - panelHeight;
    break;
  }
  case ThingPlacementDockPosition::left:
  case ThingPlacementDockPosition::top:
  default:
    break;
  }

  const QRect panelRect(x, y, panelWidth, panelHeight);
  const int collapsedThickness = 42;
  const int dockIndex = static_cast<int>(thingPlacementDockPosition_);
  if (thingPlacementHideButton_) {
    thingPlacementHideButton_->setIcon(makeThingPlacementChevronIcon(
        thingPlacementChevronAngle(dockIndex, false), QColor(188, 202, 226)));
  }
  if (thingPlacementShowButton_) {
    thingPlacementShowButton_->setIcon(makeThingPlacementChevronIcon(
        thingPlacementChevronAngle(dockIndex, true), QColor(188, 202, 226)));
  }
  QRect collapsedRect{};
  switch (thingPlacementDockPosition_) {
  case ThingPlacementDockPosition::left:
    collapsedRect = QRect(0, 0, collapsedThickness, viewHeight);
    break;
  case ThingPlacementDockPosition::right:
    collapsedRect = QRect(viewWidth - collapsedThickness, 0, collapsedThickness,
                          viewHeight);
    break;
  case ThingPlacementDockPosition::top:
    collapsedRect = QRect(0, 0, viewWidth, collapsedThickness);
    break;
  case ThingPlacementDockPosition::bottom:
  default:
    collapsedRect = QRect(0, viewHeight - collapsedThickness, viewWidth,
                          collapsedThickness);
    break;
  }

  auto animateGeometry = [this](QWidget *widget, QVariantAnimation *animation,
                                const QRect &target) {
    if (!widget) {
      return;
    }
    if (previewEditingUiRestoreInProgress_ || thingPlacementResizeActive_ || !animation ||
        widget->geometry() == target) {
      if (animation) {
        animation->stop();
      }
      widget->setGeometry(target);
      return;
    }
    animation->stop();
    animation->setStartValue(widget->geometry());
    animation->setEndValue(target);
    animation->start();
  };

  if (thingPlacementPanelVisible_) {
    thingPlacementPanelCollapsePending_ = false;
    if (thingPlacementCollapsedBarAnimation_) {
      thingPlacementCollapsedBarAnimation_->stop();
    }
    thingPlacementCollapsedBar_->hide();
    if (!thingPlacementPanel_->isVisible()) {
      thingPlacementPanel_->setGeometry(collapsedRect);
    }
    thingPlacementPanel_->show();
    thingPlacementPanel_->raise();
    animateGeometry(thingPlacementPanel_, thingPlacementPanelAnimation_,
                    panelRect);
  } else {
    // Reverse the same panel geometry used for opening. The collapsed bar
    // appears only after the full panel reaches the dock-edge bounds, so
    // hiding reads as the exact counterpart of showing rather than an
    // immediate panel disappearance followed by a stretching chevron.
    const bool animateCollapse =
        thingPlacementPanel_->isVisible() && !thingPlacementResizeActive_ &&
        thingPlacementPanelAnimation_ &&
        thingPlacementPanel_->geometry() != collapsedRect;
    if (thingPlacementShowButton_) {
      thingPlacementShowButton_->setToolTip(
          "<b>Show Thing palette</b><br>Open the panel to search and place Thing templates.");
    }
    if (animateCollapse) {
      if (thingPlacementCollapsedBarAnimation_) {
        thingPlacementCollapsedBarAnimation_->stop();
      }
      thingPlacementCollapsedBar_->hide();
      thingPlacementPanelCollapsePending_ = true;
      thingPlacementPanel_->raise();
      animateGeometry(thingPlacementPanel_, thingPlacementPanelAnimation_,
                      collapsedRect);
    } else {
      thingPlacementPanelCollapsePending_ = false;
      if (thingPlacementPanelAnimation_) {
        thingPlacementPanelAnimation_->stop();
      }
      thingPlacementPanel_->hide();
      if (thingPlacementCollapsedBarAnimation_) {
        thingPlacementCollapsedBarAnimation_->stop();
      }
      thingPlacementCollapsedBar_->setGeometry(collapsedRect);
      thingPlacementCollapsedBar_->show();
      thingPlacementCollapsedBar_->raise();
    }
  }

  positionViewportOverlay();
  positionViewportEditModeOverlay();
  if (glView_) {
    glView_->update();
  }
}

void MainWindow::syncThingPlacementPanel() {
  if (!thingPlacementPanel_) {
    return;
  }

  const bool thingMode = (editMode_ == EditMode::thing);
  if (addThingAction_) {
    const QSignalBlocker blocker(addThingAction_);
    addThingAction_->setEnabled(thingMode);
    addThingAction_->setChecked(thingMode && thingPlacementPanelVisible_);
  }
  if (!thingMode) {
    if (!selectedThingTemplateName_.empty()) {
      selectedThingTemplateName_.clear();
    }
    if (thingPlacementTemplateList_) {
      const QSignalBlocker blocker(thingPlacementTemplateList_);
      thingPlacementTemplateList_->setCurrentRow(-1);
      thingPlacementTemplateList_->clearSelection();
    }
    updateThingPlacementSelectedPreview();
    if (glView_) {
      glView_->clearPlacementPreviewThing();
      glView_->clearPlacementPreviewLines();
    }
    positionThingPlacementPanel();
    return;
  }
  if (thingPlacementTemplateListDirty_) {
    refreshThingPlacementTemplateList();
  }

  if (!thingPlacementPanelVisible_) {
    positionThingPlacementPanel();
    return;
  }

  if (thingPlacementTemplateList_) {
    const QString selected =
        QString::fromStdString(sed::text::trim(selectedThingTemplateName_));
    bool found = false;
    for (int i = 0; i < thingPlacementTemplateList_->count(); ++i) {
      QListWidgetItem *item = thingPlacementTemplateList_->item(i);
      if (!item) {
        continue;
      }
      if (item->data(Qt::UserRole)
              .toString()
              .compare(selected, Qt::CaseInsensitive) == 0) {
        const QSignalBlocker blocker(thingPlacementTemplateList_);
        thingPlacementTemplateList_->setCurrentItem(item);
        found = true;
        break;
      }
    }

    if (!found) {
      const QSignalBlocker blocker(thingPlacementTemplateList_);
      thingPlacementTemplateList_->setCurrentRow(-1);
      thingPlacementTemplateList_->clearSelection();
      if (!selectedThingTemplateName_.empty()) {
        selectedThingTemplateName_.clear();
      }
    }
  }

  if (thingPlacementSummaryLabel_) {
    const std::string name = currentThingTemplateName();
    if (name.empty()) {
      thingPlacementSummaryLabel_->setText(
          QStringLiteral("No template selected"));
      syncThingPlacementOptionControls();
      updateThingPlacementSelectedPreview();
      positionThingPlacementPanel();
      if (thingPlacementTemplateList_ &&
          thingPlacementTemplateList_->count() > 0) {
        const int iconGeneration = thingPlacementTemplateIconGeneration_;
        QTimer::singleShot(kThingPlacementThumbnailOpenDelayMs, this,
                           [this, iconGeneration]() {
                             populateThingPlacementTemplateIcons(iconGeneration,
                                                                 0);
                           });
      }
      return;
    }

    QString type{};
    if (templates_.size() > 0) {
      if (const TplValue *value = templates_.findField(name, "type")) {
        type = QString::fromStdString(value->asString()).trimmed();
      }
    }

    const std::optional<ThingBox> box = resolvedThingTemplateBox(name);
    QString boxText = box ? QStringLiteral("bbox") : QStringLiteral("marker");
    thingPlacementSummaryLabel_->setText(
        type.isEmpty() ? QStringLiteral("%1 - %2").arg(
                             QString::fromStdString(name), boxText)
                       : QStringLiteral("%1 - %2 - %3")
                             .arg(QString::fromStdString(name), type, boxText));
  }

  syncThingPlacementOptionControls();
  updateThingPlacementSelectedPreview();
  positionThingPlacementPanel();
  if (thingPlacementTemplateList_ && thingPlacementTemplateList_->count() > 0) {
    const int iconGeneration = thingPlacementTemplateIconGeneration_;
    QTimer::singleShot(kThingPlacementThumbnailOpenDelayMs, this,
                       [this, iconGeneration]() {
                         populateThingPlacementTemplateIcons(iconGeneration, 0);
                       });
  }
}

void MainWindow::setThingPlacementPanelVisible(const bool visible) {
  thingPlacementPanelVisible_ = visible;
  if (!visible) {
    ++thingPlacementTemplateIconGeneration_;
    deselectThingPlacementTemplate();
  }
  syncThingPlacementPanel();
}

void MainWindow::applyThingPlacementDockPosition(
    const ThingPlacementDockPosition position, const bool preserveSize) {
  (void)preserveSize;
  thingPlacementDockPosition_ = position;
  positionThingPlacementPanel();
}

void MainWindow::deselectThingPlacementTemplate() {
  selectedThingTemplateName_.clear();
  thingPlacementYawOffset_ = 0.0;
  if (thingPlacementTemplateList_) {
    const QSignalBlocker blocker(thingPlacementTemplateList_);
    thingPlacementTemplateList_->setCurrentRow(-1);
    thingPlacementTemplateList_->clearSelection();
  }
  updateThingPlacementSelectedPreview();
  if (placementMode_ == PlacementMode::thing) {
    setPlacementMode(PlacementMode::none);
  }
  if (glView_) {
    glView_->clearPlacementPreviewThing();
    glView_->clearPlacementPreviewLines();
  }
  syncThingPlacementPanel();
}

void MainWindow::refreshThingPlacementTemplateList() {
  if (!thingPlacementTemplateList_) {
    return;
  }

  ++thingPlacementTemplateIconGeneration_;
  thingPlacementModelThumbnailRenderer_.reset();
  if (thingPlacementResourceDiskWatcher_) {
    thingPlacementResourceDiskWatcher_->setResourceLocator(&resources_);
    thingPlacementResourceDiskWatcher_->clearDependencies();
    thingPlacementResourceDiskWatcher_->watchResource(
        document_.level().getMasterCMP(), ".cmp");
  }
  thingPlacementTemplateListDirty_ = false;

  const std::string previous = sed::text::trim(selectedThingTemplateName_);
  const bool allowBaseTemplates =
      settingsStore()
          .value("templates/allow_base_template_pick", false)
          .toBool();

  auto isBaseTemplateName = [](const QString &name) noexcept -> bool {
    const QString trimmed = name.trimmed();
    return !trimmed.isEmpty() &&
           trimmed.compare(QStringLiteral("none"), Qt::CaseInsensitive) != 0 &&
           trimmed.startsWith(QLatin1Char('_'));
  };

  std::vector<const ThingTemplate *> templates{};
  templates.reserve(templates_.size());
  for (std::size_t i = 0; i < templates_.size(); ++i) {
    const ThingTemplate *tpl = templates_.get(i);
    if (!tpl || tpl->name.empty()) {
      continue;
    }

    const QString name = QString::fromStdString(tpl->name).trimmed();
    if (!allowBaseTemplates && isBaseTemplateName(name)) {
      continue;
    }

    templates.push_back(tpl);
  }

  std::stable_sort(
      templates.begin(), templates.end(),
      [](const ThingTemplate *lhs, const ThingTemplate *rhs) {
        return QString::fromStdString(lhs ? lhs->name : std::string())
                   .compare(
                       QString::fromStdString(rhs ? rhs->name : std::string()),
                       Qt::CaseInsensitive) < 0;
      });

  const QSignalBlocker blocker(thingPlacementTemplateList_);
  thingPlacementTemplateList_->clear();

  QString previousCategory = QStringLiteral("All");
  if (thingPlacementCategoryList_) {
    if (QListWidgetItem *categoryItem =
            thingPlacementCategoryList_->currentItem()) {
      previousCategory = categoryItem->data(kThingPlacementCategoryKeyRole)
                             .toString()
                             .trimmed();
      if (previousCategory.isEmpty()) {
        previousCategory = categoryItem->text().trimmed();
      }
    }
  }

  std::vector<ThingPlacementCategoryFilter> categoryFilters{};
  categoryFilters.push_back(ThingPlacementCategoryFilter{
      QStringLiteral("All"),
      QStringLiteral("All"),
      QStringLiteral("All templates"),
      {},
  });

  if (const std::optional<assets::JlhTree> categoryTree =
          loadThingPlacementTemplateCategoryTree(resources_,
                                                 document_.level().kind)) {
    for (const assets::JlhNode &node : categoryTree->roots) {
      collectThingPlacementCategoryFilters(categoryFilters, node, QString{},
                                           QStringLiteral("root"));
    }
  }

  auto addFallbackCategory = [&categoryFilters](const QString &rawCategory) {
    const QString category = rawCategory.trimmed().isEmpty()
                                 ? QStringLiteral("Other")
                                 : rawCategory.trimmed();
    for (const ThingPlacementCategoryFilter &existing : categoryFilters) {
      if (existing.label.compare(category, Qt::CaseInsensitive) == 0) {
        return;
      }
    }
    categoryFilters.push_back(
        ThingPlacementCategoryFilter{category, category, category, {}});
  };

  const bool usingCategoryTree = categoryFilters.size() > 1;
  std::unordered_map<std::string, QStringList> templateCategoryLabels{};
  for (const ThingPlacementCategoryFilter &filter : categoryFilters) {
    if (filter.label.compare(QStringLiteral("All"), Qt::CaseInsensitive) == 0) {
      continue;
    }
    for (const QString &rawName : filter.templateNames) {
      const QString name = rawName.trimmed();
      if (name.isEmpty()) {
        continue;
      }
      QStringList &labels =
          templateCategoryLabels[sed::text::upperAscii(name.toStdString())];
      if (!labels.contains(filter.label, Qt::CaseInsensitive)) {
        labels.push_back(filter.label);
      }
    }
  }

  QListWidgetItem *previousItem = nullptr;
  const QIcon loadingIcon(ui::detail::makePreviewLoadingPixmap(
      thingPlacementTemplateList_->iconSize()));
  for (const ThingTemplate *tpl : templates) {
    if (!tpl) {
      continue;
    }

    const QString name = QString::fromStdString(tpl->name).trimmed();
    if (name.isEmpty()) {
      continue;
    }

    QString type{};
    if (const TplValue *value = templates_.findField(tpl->name, "type")) {
      type = QString::fromStdString(value->asString()).trimmed();
    }
    const QString category = type.isEmpty() ? QStringLiteral("Other") : type;
    if (!usingCategoryTree) {
      addFallbackCategory(category);
    }

    QStringList categoryLabelsForTemplate{};
    if (const auto it = templateCategoryLabels.find(
            sed::text::upperAscii(name.toStdString()));
        it != templateCategoryLabels.end()) {
      categoryLabelsForTemplate = it->second;
    }
    if (categoryLabelsForTemplate.isEmpty()) {
      categoryLabelsForTemplate.push_back(category);
    }

    auto *item = new QListWidgetItem(name, thingPlacementTemplateList_);
    item->setData(Qt::UserRole, name);
    item->setData(kThingPlacementTemplateCategoryRole,
                  categoryLabelsForTemplate.join(QLatin1String(", ")));
    item->setData(kThingPlacementTemplateTypeRole, type);
    item->setToolTip(
        type.isEmpty()
            ? QString::fromStdString(tpl->desc).trimmed()
            : QStringLiteral("%1\nType: %2")
                  .arg(QString::fromStdString(tpl->desc).trimmed(), type)
                  .trimmed());
    item->setData(kThingPlacementTemplateSearchRole,
                  QStringLiteral("%1 %2 %3")
                      .arg(name,
                           categoryLabelsForTemplate.join(QLatin1Char(' ')),
                           item->toolTip())
                      .trimmed());
    item->setTextAlignment(Qt::AlignHCenter);
    item->setSizeHint(thingPlacementTemplateList_->gridSize());
    item->setIcon(loadingIcon);
    item->setData(kThingPlacementTemplatePreviewPendingRole, true);

    if (name.compare(QString::fromStdString(previous), Qt::CaseInsensitive) ==
        0) {
      previousItem = item;
    }
  }

  QListWidgetItem *itemToSelect = previousItem;
  if (itemToSelect) {
    thingPlacementTemplateList_->setCurrentItem(itemToSelect);
    selectedThingTemplateName_ =
        itemToSelect->data(Qt::UserRole).toString().toStdString();
  } else {
    thingPlacementTemplateList_->setCurrentRow(-1);
    thingPlacementTemplateList_->clearSelection();
    selectedThingTemplateName_.clear();
  }

  if (thingPlacementCategoryList_) {
    if (!usingCategoryTree) {
      std::sort(categoryFilters.begin() + 1, categoryFilters.end(),
                [](const auto &lhs, const auto &rhs) {
                  return lhs.label.compare(rhs.label, Qt::CaseInsensitive) < 0;
                });
    }

    const QSignalBlocker categoryBlocker(thingPlacementCategoryList_);
    thingPlacementCategoryList_->clear();
    QListWidgetItem *categoryToSelect = nullptr;
    for (const ThingPlacementCategoryFilter &filter : categoryFilters) {
      auto *categoryItem =
          new QListWidgetItem(filter.label, thingPlacementCategoryList_);
      categoryItem->setData(Qt::UserRole, filter.label);
      categoryItem->setData(kThingPlacementCategoryKeyRole, filter.key);
      categoryItem->setData(kThingPlacementTemplateCategoryNamesRole,
                            filter.templateNames);
      categoryItem->setToolTip(filter.tooltip.isEmpty() ? filter.label
                                                        : filter.tooltip);
      const int textWidth =
          thingPlacementCategoryList_->fontMetrics().horizontalAdvance(
              filter.label);
      categoryItem->setSizeHint(QSize(std::clamp(textWidth + 32, 64, 260), 30));
      if (filter.key.compare(previousCategory, Qt::CaseInsensitive) == 0 ||
          filter.label.compare(previousCategory, Qt::CaseInsensitive) == 0) {
        categoryToSelect = categoryItem;
      }
    }
    thingPlacementCategoryList_->setCurrentItem(
        categoryToSelect ? categoryToSelect
                         : thingPlacementCategoryList_->item(0));
  }

  applyThingPlacementTemplateFilters();
}

void MainWindow::applyThingPlacementTemplateFilters() {
  if (!thingPlacementTemplateList_) {
    return;
  }

  const QString filter = thingPlacementSearchEdit_
                             ? thingPlacementSearchEdit_->text().trimmed()
                             : QString{};
  QStringList categoryTemplateNames{};
  QString categoryFilterLabel = QStringLiteral("All");
  if (thingPlacementCategoryList_) {
    if (QListWidgetItem *categoryItem =
            thingPlacementCategoryList_->currentItem()) {
      categoryTemplateNames =
          categoryItem->data(kThingPlacementTemplateCategoryNamesRole)
              .toStringList();
      categoryFilterLabel =
          categoryItem->data(Qt::UserRole).toString().trimmed();
      if (categoryFilterLabel.isEmpty()) {
        categoryFilterLabel = categoryItem->text().trimmed();
      }
    }
  }
  const bool restrictCategory = !categoryTemplateNames.isEmpty();
  const bool fallbackCategory =
      !restrictCategory && !categoryFilterLabel.isEmpty() &&
      categoryFilterLabel.compare(QStringLiteral("All"), Qt::CaseInsensitive) !=
          0;

  for (int i = 0; i < thingPlacementTemplateList_->count(); ++i) {
    QListWidgetItem *item = thingPlacementTemplateList_->item(i);
    if (!item) {
      continue;
    }

    const QString actualName = item->data(Qt::UserRole).toString().trimmed();
    const QString haystack =
        item->data(kThingPlacementTemplateSearchRole).toString().trimmed();
    bool categoryMatches = !restrictCategory && !fallbackCategory;
    for (const QString &categoryName : categoryTemplateNames) {
      if (categoryName.compare(actualName, Qt::CaseInsensitive) == 0) {
        categoryMatches = true;
        break;
      }
    }
    if (fallbackCategory) {
      const QString itemCategory =
          item->data(kThingPlacementTemplateCategoryRole).toString().trimmed();
      categoryMatches =
          itemCategory.compare(categoryFilterLabel, Qt::CaseInsensitive) == 0;
    }
    const bool textMatches =
        filter.isEmpty() || haystack.contains(filter, Qt::CaseInsensitive);
    item->setHidden(!(categoryMatches && textMatches));
  }

  const int generation = ++thingPlacementTemplateIconGeneration_;
  if (editMode_ == EditMode::thing && thingPlacementPanelVisible_) {
    const auto kThumbTuning = ui::detail::sharedModelThumbLoadTuning();
    scheduleThingPlacementTemplateIcons(
        generation, 0,
        std::max(kThumbTuning.startDelayMs,
                 kThingPlacementThumbnailOpenDelayMs));
  }
}

void MainWindow::scheduleThingPlacementTemplateIcons(
    const int generation, const int startIndex, const int delayMs) {
  if (!thingPlacementTemplateList_ ||
      generation != thingPlacementTemplateIconGeneration_ ||
      editMode_ != EditMode::thing || !thingPlacementPanelVisible_) {
    thingPlacementTemplateIconLoadTimer_.stop();
    return;
  }

  thingPlacementTemplateIconScheduledGeneration_ = generation;
  thingPlacementTemplateIconScheduledStartIndex_ = startIndex;
  const int interactionDelay =
      thingPlacementTemplateIconPublicationPacer_.delayUntilQuiet();
  thingPlacementTemplateIconLoadTimer_.start(
      std::max({0, delayMs, interactionDelay}));
}

void MainWindow::populateThingPlacementTemplateIcons(const int generation,
                                                     const int startIndex) {
  if (!thingPlacementTemplateList_ ||
      generation != thingPlacementTemplateIconGeneration_ ||
      editMode_ != EditMode::thing || !thingPlacementPanelVisible_) {
    return;
  }

  int rowBegin = -1;
  int rowEnd = -1;
  const auto kThumbTuning = ui::detail::sharedModelThumbLoadTuning();
  if (!ui::detail::visiblePreviewListRowRange(thingPlacementTemplateList_,
                                              rowBegin, rowEnd,
                                              kThumbTuning.visibleMargin)) {
    return;
  }
  if (thingPlacementTemplateIconPublicationPacer_.interactionActive()) {
    scheduleThingPlacementTemplateIcons(
        generation, startIndex,
        thingPlacementTemplateIconPublicationPacer_.delayUntilQuiet());
    return;
  }
  if (thingPlacementTemplateIconLoadInProgress_) {
    scheduleThingPlacementTemplateIcons(
        generation, startIndex,
        std::max(kThumbTuning.visibleDelayMs,
                 kThingPlacementThumbnailBatchDelayMs));
    return;
  }

  struct ThingPlacementIconBatchScope final {
    bool &flag;
    explicit ThingPlacementIconBatchScope(bool &value) : flag(value) {
      flag = true;
    }
    ~ThingPlacementIconBatchScope() { flag = false; }
  } batchScope(thingPlacementTemplateIconLoadInProgress_);

  rowBegin = std::max(0, rowBegin - kThumbTuning.visibleMargin);
  rowEnd = std::min(thingPlacementTemplateList_->count() - 1,
                    rowEnd + kThumbTuning.visibleMargin);

  const int kBatchSize = std::max(1, kThumbTuning.visibleBatchCount);
  const std::vector<int> priorityRows =
      ui::detail::previewThumbnailPriorityRows(
          rowBegin, rowEnd, thingPlacementTemplateList_->currentRow());
  int processed = 0;
  bool visiblePending = false;
  QElapsedTimer batchTimer{};
  batchTimer.start();
  for (const int row : priorityRows) {
    const QListWidgetItem *item = thingPlacementTemplateList_->item(row);
    if (!item || item->isHidden()) {
      continue;
    }
    if (!item->data(kThingPlacementTemplatePreviewPendingRole).toBool()) {
      continue;
    }
    visiblePending = true;
    if (!ui::detail::previewThumbnailBatchCanContinue(
            processed, kBatchSize, batchTimer.nsecsElapsed())) {
      continue;
    }
    ++processed;
    bool renderedThisRow = false;
    if (!loadThingPlacementTemplateIconForRow(row, &renderedThisRow)) {
      continue;
    }
    (void)renderedThisRow;
  }

  if (visiblePending) {
    if (ui::detail::previewThumbnailQueueNeedsPolling(
            true, thingPlacementTemplateIconActiveJobs_)) {
      scheduleThingPlacementTemplateIcons(
          generation, 0,
          std::max(kThumbTuning.visibleDelayMs,
                   kThingPlacementThumbnailBatchDelayMs));
    } else {
      thingPlacementTemplateIconLoadTimer_.stop();
    }
  }
}

void MainWindow::queueThingPlacementTemplateModelPrep(
    const std::string &modelName) {
  const QSize iconSize = thingPlacementTemplateList_
                             ? thingPlacementTemplateList_->iconSize()
                             : QSize(104, 104);
  (void)ui::detail::queueSharedPreviewModelBoxPrep(
      this, resourceLocatorSnapshot_, modelName,
      ui::detail::thingAssetPreviewColormap(document_.level()),
      iconSize, false,
      thingPlacementTemplateIconPrepInFlight_,
      thingPlacementTemplateIconActiveJobs_, [this](const bool current) {
        (void)current;
        if (!thingPlacementTemplateList_ || editMode_ != EditMode::thing ||
            !thingPlacementPanelVisible_) {
          return;
        }
        const int generation = thingPlacementTemplateIconGeneration_;
        const auto kThumbTuning = ui::detail::sharedModelThumbLoadTuning();
        scheduleThingPlacementTemplateIcons(
            generation, 0,
            std::max(kThumbTuning.visibleDelayMs,
                     kThingPlacementThumbnailBatchDelayMs));
      });
}

bool MainWindow::loadThingPlacementTemplateIconForRow(
    const int row, bool *renderedAuthoritativeThumbnail) {
  if (renderedAuthoritativeThumbnail) {
    *renderedAuthoritativeThumbnail = false;
  }
  if (!thingPlacementTemplateList_ || row < 0 ||
      row >= thingPlacementTemplateList_->count()) {
    return true;
  }

  QListWidgetItem *item = thingPlacementTemplateList_->item(row);
  if (!item) {
    return true;
  }
  if (item->isHidden()) {
    return true;
  }
  if (!item->data(kThingPlacementTemplatePreviewPendingRole).toBool()) {
    return true;
  }

  const QString templateName = item->data(Qt::UserRole).toString().trimmed();
  const ThingTemplate *tpl = templates_.find(templateName.toStdString());
  const QSize iconSize = thingPlacementTemplateList_->iconSize();

  auto finishUnavailable = [&]() {
    ui::detail::publishPreviewThumbnailItem(
        thingPlacementTemplateList_, item,
        QIcon(ui::detail::makePreviewUnavailablePixmap(iconSize)),
        kThingPlacementTemplatePreviewPendingRole, false);
    if (thingPlacementTemplateList_ &&
        thingPlacementTemplateList_->currentItem() == item) {
      updateThingPlacementSelectedPreview();
    }
  };

  auto finishReady = [&](const QIcon &icon) {
    ui::detail::publishPreviewThumbnailItem(
        thingPlacementTemplateList_, item, icon,
        kThingPlacementTemplatePreviewPendingRole, false);
    if (thingPlacementTemplateList_ &&
        thingPlacementTemplateList_->currentItem() == item) {
      updateThingPlacementSelectedPreview();
    }
  };

  auto rememberIcon = [&](const std::string &key, const QIcon &icon) {
    rememberThingPlacementTemplateIcon(
        thingPlacementTemplatePreviewIconCache_, key, icon);
  };

  if (!tpl) {
    finishUnavailable();
    return true;
  }

  auto stillSameItem = [&]() -> bool {
    return thingPlacementTemplateList_ && row >= 0 &&
           row < thingPlacementTemplateList_->count() &&
           thingPlacementTemplateList_->item(row) == item &&
           item->data(Qt::UserRole)
                   .toString()
                   .trimmed()
                   .compare(templateName, Qt::CaseInsensitive) == 0;
  };

  if (const std::optional<std::string> spriteName =
          thingPlacementResolvedPreviewField(*tpl, templates_, "sprite")) {
    const std::string previewColormap =
        ui::detail::thingAssetPreviewColormap(document_.level());
    if (thingPlacementResourceDiskWatcher_) {
      thingPlacementResourceDiskWatcher_->watchResource(*spriteName, ".spr");
      thingPlacementResourceDiskWatcher_->watchResource(
          previewColormap, ".cmp");
      if (const std::shared_ptr<const assets::SprFile> sprite =
              ui::detail::loadSharedPreviewSprite(&resources_, *spriteName)) {
        thingPlacementResourceDiskWatcher_->watchResource(sprite->material,
                                                          ".mat");
      }
    }
    const std::string cacheKey =
        std::string("spr:") + sed::text::upperAscii(*spriteName) +
        "|deps=" + ui::detail::sharedPreviewSpriteDependencyRevisionToken(
            &resources_, previewColormap, *spriteName);
    if (const auto it = thingPlacementTemplatePreviewIconCache_.find(cacheKey);
        it != thingPlacementTemplatePreviewIconCache_.end()) {
      if (stillSameItem()) {
        finishReady(it->second);
      }
      return true;
    }
    if (const std::optional<QImage> spriteImage =
            ui::detail::loadSharedPreviewSpriteImage(
                &resources_, previewColormap, *spriteName)) {
      const QIcon icon(
          ui::detail::makeTemplatePreviewPixmap(*spriteImage, iconSize));
      rememberIcon(cacheKey, icon);
      if (stillSameItem()) {
        finishReady(icon);
      }
      return true;
    }

    if (stillSameItem()) {
      finishUnavailable();
    }
    return true;
  }

  std::optional<std::string> modelName =
      thingPlacementResolvedPreviewField(*tpl, templates_, "model3d");
  if (!modelName) {
    modelName =
        thingPlacementResolvedPreviewField(*tpl, templates_, "model3do");
  }
  if (modelName) {
    const std::string previewColormap =
        ui::detail::thingAssetPreviewColormap(document_.level());
    if (thingPlacementResourceDiskWatcher_) {
      thingPlacementResourceDiskWatcher_->watchResource(*modelName, ".3do");
      thingPlacementResourceDiskWatcher_->watchResource(
          previewColormap, ".cmp");
    }
    QIcon icon{};
    const auto result = ui::detail::loadSharedModelPreviewIcon(
        thingPlacementTemplatePreviewIconCache_,
        thingPlacementModelThumbnailRenderer_, resourceLocatorSnapshot_,
        document_.level().kind, previewColormap, *modelName,
        iconSize, false, thingPlacementTemplateIconActiveJobs_,
        [this](const std::string &queuedModelName) {
          queueThingPlacementTemplateModelPrep(queuedModelName);
        },
        icon);
    if (thingPlacementResourceDiskWatcher_) {
      std::shared_ptr<const assets::Model3do> model{};
      if (ui::detail::findCachedSharedPreviewModel(
              resourceLocatorSnapshot_.get(), *modelName, model) &&
          model) {
        for (const std::string &material : model->materials) {
          thingPlacementResourceDiskWatcher_->watchResource(material, ".mat");
        }
      }
    }
    switch (result) {
    case ui::detail::SharedModelPreviewIconLoadResult::assigned: {
      if (stillSameItem()) {
        finishReady(icon);
      }
      return true;
    }
    case ui::detail::SharedModelPreviewIconLoadResult::rendered: {
      if (renderedAuthoritativeThumbnail) {
        *renderedAuthoritativeThumbnail = true;
      }
      if (stillSameItem()) {
        finishReady(icon);
      }
      return true;
    }
    case ui::detail::SharedModelPreviewIconLoadResult::pending:
      return false;
    case ui::detail::SharedModelPreviewIconLoadResult::unavailable:
      if (stillSameItem()) {
        finishUnavailable();
      }
      return true;
    }
  }

  if (stillSameItem()) {
    finishUnavailable();
  }
  return true;
}

} // namespace sed
