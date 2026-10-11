#include "ui/resources/resource_authoring_editor.hpp"
#include "ui/shortcuts/key_sequence_display.hpp"
#include "ui/resources/resource_authoring_detail.hpp"
#include "ui/resources/resource_authoring_pane.hpp"

#include "gl_view.hpp"
#include "sed/assets/episode.hpp"
#include "sed/assets/snd.hpp"
#include "sed/assets/spr.hpp"
#include "sed/assets/string_table.hpp"
#include "sed/assets/tpl.hpp"
#include "sed/geometry.hpp"
#include "sed/level.hpp"
#include "sed/text.hpp"
#include "sed/values.hpp"
#include "ui/episode/episode_editor.hpp"
#include "ui/components/live_color_popup.hpp"
#include "ui/components/property_editor_controls.hpp"
#include "ui/components/ui_decimal_format.hpp"
#include "ui/resources/resource_picker.hpp"
#include "ui/audio/app_sound_player.hpp"

#include <QAbstractItemModel>
#include <QAbstractItemView>
#include <QAction>
#include <QApplication>
#include <QByteArray>
#include <QCheckBox>
#include <QCborArray>
#include <QCborValue>
#include <QCloseEvent>
#include <QComboBox>
#include <QCryptographicHash>
#include <QCompleter>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QEvent>
#include <QCursor>
#include <QMouseEvent>
#include <QPalette>
#include <QPainter>
#include <QPainterPath>
#include <QPainterPathStroker>
#include <QPixmap>
#include <QPolygonF>
#include <QScreen>
#include <QScrollBar>
#include <QStyle>
#include <QKeyEvent>
#include <QKeySequence>
#include <QFileInfo>
#include <QFileDialog>
#include <QFrame>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QHelpEvent>
#include <QInputDialog>
#include <QItemSelectionModel>
#include <QListWidget>
#include <QFontMetrics>
#include <QGuiApplication>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QMessageBox>
#include <QMenu>
#include <QMetaObject>
#include <QPersistentModelIndex>
#include <QPointer>
#include <QPushButton>
#include <QColor>
#include <QRegion>
#include <QScrollArea>
#include <QSet>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QSplitter>
#include <QStringList>
#include <QStyledItemDelegate>
#include <QStyleOptionHeader>
#include <QStyleOptionSlider>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTimer>
#include <QToolTip>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QUuid>
#include <QVariant>
#include <QVBoxLayout>

#include <algorithm>
#include <atomic>
#include <array>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iterator>
#include <limits>
#include <memory>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <span>
#include <unordered_map>
#include <utility>
#include <vector>

namespace sed::ui::resources::detail
{

namespace
{
using TableEditorCreated = std::function<void(QWidget*, const QModelIndex&)>;
constexpr std::array<int, 6> kSoundColumnMinimumWidths{80, 160, 100, 56, 56, 230};
constexpr std::array<int, 6> kSoundColumnDefaultWidths{80, 220, 100, 64, 64, 230};
constexpr int kSoundColumnMaximumWidth = 16384;
constexpr int kSoundColumnsMaximumTotalWidth = 65536;

class SoundEntriesTree final : public QTreeWidget
{
  public:
    explicit SoundEntriesTree(QWidget* parent) : QTreeWidget(parent) {}

  protected:
    void scrollContentsBy(const int dx, const int dy) override
    {
        QTreeWidget::scrollContentsBy(dx, dy);
        updateEditorGeometries();
    }

    void resizeEvent(QResizeEvent* const event) override
    {
        QTreeWidget::resizeEvent(event);
        updateEditorGeometries();
    }

    void drawBranches(QPainter* painter, const QRect& rect, const QModelIndex& index) const override
    {
        const auto kind = static_cast<SoundTreeItemKind>(
            index.siblingAtColumn(0).data(kSoundTreeItemKindRole).toInt());
        if ( kind == SoundTreeItemKind::alternative )
        {
            // Alternative rows are leaves. Their native branch pass paints a
            // hover/current gutter, so leave this area neutral while keeping
            // the mode rows' native expanders intact.
            return;
        }
        QTreeWidget::drawBranches(painter, rect, index);
    }

    void drawRow(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override
    {
        QStyleOptionViewItem row(option);
        row.state &= ~(QStyle::State_Selected | QStyle::State_MouseOver | QStyle::State_HasFocus);
        QTreeWidget::drawRow(painter, row, index);

        const auto kind = static_cast<SoundTreeItemKind>(
            index.siblingAtColumn(0).data(kSoundTreeItemKindRole).toInt());
        if ( kind == SoundTreeItemKind::alternative )
        {
            // The native row pass can still vary the indentation gutter with
            // hover/current state. Alternatives have no expander, so repaint
            // only that gutter with the stable viewport base after row paint.
            const QRect firstCell = visualRect(index.siblingAtColumn(0));
            const QRect viewportRect = viewport()->rect();
            const int gutterRight = std::clamp(firstCell.left(), viewportRect.left(), viewportRect.right() + 1);
            const QRect gutter(viewportRect.left(), option.rect.top(), gutterRight - viewportRect.left(),
                               option.rect.height());
            if ( !gutter.isEmpty() )
            {
                painter->fillRect(gutter, viewport()->palette().brush(QPalette::Base));
            }
        }
    }

};

class TrackingItemDelegate final : public QStyledItemDelegate
{
public:
    TrackingItemDelegate(TableEditorCreated editorCreated, QObject* parent)
        : QStyledItemDelegate(parent), editorCreated_(std::move(editorCreated))
    {
    }

    QWidget* createEditor(
        QWidget* parent,
        const QStyleOptionViewItem& option,
        const QModelIndex& index) const override
    {
        QWidget* const editor = QStyledItemDelegate::createEditor(parent, option, index);
        if ( editor && editorCreated_ )
        {
            editorCreated_(editor, index);
        }
        return editor;
    }

private:
    TableEditorCreated editorCreated_{};
};

class ModeSuggestionDelegate final : public QStyledItemDelegate
{
public:
    explicit ModeSuggestionDelegate(
        QStringList modes,
        TableEditorCreated editorCreated,
        QObject* parent = nullptr)
        : QStyledItemDelegate(parent),
          modes_(std::move(modes)),
          editorCreated_(std::move(editorCreated))
    {
    }

    QWidget* createEditor(
        QWidget* parent,
        const QStyleOptionViewItem&,
        const QModelIndex& index) const override
    {
        auto* const combo = new QComboBox(parent);
        combo->setEditable(true);
        combo->setInsertPolicy(QComboBox::NoInsert);
        combo->addItems(modes_);
        if ( QCompleter* const completer = combo->completer() )
        {
            completer->setCaseSensitivity(Qt::CaseInsensitive);
            completer->setFilterMode(Qt::MatchContains);
        }
        if ( editorCreated_ )
        {
            editorCreated_(combo, index);
        }
        return combo;
    }

    void setEditorData(QWidget* editor, const QModelIndex& index) const override
    {
        auto* const combo = qobject_cast<QComboBox*>(editor);
        if ( combo )
        {
            combo->setCurrentText(index.data(Qt::EditRole).toString());
            combo->lineEdit()->selectAll();
        }
    }

    void setModelData(
        QWidget* editor,
        QAbstractItemModel* model,
        const QModelIndex& index) const override
    {
        if ( auto* const combo = qobject_cast<QComboBox*>(editor) )
        {
            model->setData(index, combo->currentText(), Qt::EditRole);
        }
    }

private:
    QStringList modes_{};
    TableEditorCreated editorCreated_{};
};

int soundVolumeValueWidth(const QRect&, const QFontMetrics& metrics)
{
    return std::max(24, metrics.horizontalAdvance(QStringLiteral("0.000")) + 6);
}

QRect soundVolumeRemoveRect(const QRect& cell, const QWidget* const viewport)
{
    if ( !viewport )
    {
        return {};
    }
    const QRect visible = cell.intersected(viewport->rect());
    if ( visible.isEmpty() )
    {
        return {};
    }
    constexpr int buttonSize = 22;
    constexpr int rightMargin = 4;
    if ( visible.width() < buttonSize + rightMargin )
    {
        return {};
    }
    return QRect(visible.right() - rightMargin - buttonSize + 1,
                 visible.top() + (visible.height() - std::min(buttonSize, visible.height())) / 2,
                 buttonSize, std::min(buttonSize, visible.height()));
}

bool soundVolumeHasRemoveSpace(const QRect& cell, const QWidget* const viewport)
{
    const QRect visible = viewport ? cell.intersected(viewport->rect()) : QRect{};
    const QRect remove = soundVolumeRemoveRect(cell, viewport);
    if ( visible.isEmpty() || remove.isEmpty() || !visible.contains(remove) )
    {
        return false;
    }
    const int canvasWidth = remove.left() - visible.left();
    // Keep an ordinary value, a usable slider, their gap, and outer margins.
    return canvasWidth >= soundVolumeValueWidth(visible, QFontMetrics(viewport->font())) + 24;
}

QRect soundVolumeCanvasRect(const QRect& cell, const QWidget* const viewport)
{
    if ( !viewport )
    {
        return cell;
    }
    QRect result = cell.intersected(viewport->rect());
    if ( result.isEmpty() )
    {
        return {};
    }
    // Reserve the shared Remove slot only when the remaining visible controls
    // fit. A small viewport segment keeps its own value instead of assigning
    // that value a negative child position beneath the preceding column.
    if ( soundVolumeHasRemoveSpace(cell, viewport) )
    {
        result.setRight(soundVolumeRemoveRect(cell, viewport).left() - 1);
    }
    return result.isEmpty() ? QRect{} : result;
}

QRect soundVolumeSliderRect(const QRect& cell, const QFontMetrics& metrics)
{
    if ( cell.isEmpty() )
    {
        return {};
    }
    const int height = std::min(16, std::max(0, cell.height() - 4));
    return QRect(cell.left() + 4, cell.top() + (cell.height() - height) / 2,
                 std::max(0, cell.width() - soundVolumeValueWidth(cell, metrics) - 12), height);
}

void configureSoundVolumeValueEdit(QLineEdit* const value)
{
    value->setMaximumWidth(72);
    value->setMinimumWidth(24);
    value->setFrame(false);
    value->setAlignment(Qt::AlignRight);
    value->setStyleSheet(QStringLiteral("QLineEdit { border: none; background: transparent; padding: 0; }"));
}

class SoundVolumeSlider final : public QSlider
{
    public:
    explicit SoundVolumeSlider(QWidget* parent) : QSlider(Qt::Horizontal, parent)
    {
        setRange(0, 1000);
        setTickPosition(QSlider::NoTicks);
        setStyleSheet(QStringLiteral(
            "QSlider { background: transparent; border: none; }"
            "QSlider::groove:horizontal { height: 4px; border: none; border-radius: 2px; background: #a6a6a6; }"
            "QSlider::sub-page:horizontal { border-radius: 2px; background: #53c6ef; }"
            "QSlider::add-page:horizontal { border-radius: 2px; background: #a6a6a6; }"
            "QSlider::handle:horizontal { width: 6px; margin: -4px 0; border: 3px solid #55595c; "
            "border-radius: 6px; background: #53c6ef; }"));
    }

    QStyleOptionSlider paintOption() const
    {
        QStyleOptionSlider option;
        initStyleOption(&option);
        return option;
    }
};

QRect soundVolumeValueRect(const QRect& cell, const QFontMetrics& metrics)
{
    if ( cell.isEmpty() )
    {
        return {};
    }
    const int width = std::min(soundVolumeValueWidth(cell, metrics), std::max(0, cell.width() - 8));
    if ( width == 0 )
    {
        return {};
    }
    return QRect(cell.right() - 3 - width, cell.top(), width, cell.height());
}

QIcon soundPreviewIcon(const bool playing)
{
    QPixmap pixels(40, 40);
    pixels.setDevicePixelRatio(2.0);
    pixels.fill(Qt::transparent);
    QPainter painter(&pixels);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.translate(10, 10);
    painter.scale(0.98, 0.98);
    painter.translate(-10, -10);
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(142, 207, 163));
    if ( playing )
    {
        painter.drawRoundedRect(QRectF(5, 5, 10, 10), 1.5, 1.5);
    }
    else
    {
        painter.drawPolygon(QPolygonF{QPointF(1, 1), QPointF(19, 10), QPointF(1, 19)});
    }
    return QIcon(pixels);
}

QIcon soundLoopIcon()
{
    QPixmap pixels(40, 40);
    pixels.setDevicePixelRatio(2.0);
    pixels.fill(Qt::transparent);
    QPainter painter(&pixels);
    painter.setRenderHint(QPainter::Antialiasing);
    QPainterPath arc{};
    arc.moveTo(14.0, 6.0);
    arc.cubicTo(16.0, 8.0, 16.0, 11.0, 14.0, 13.0);
    arc.cubicTo(11.0, 16.0, 7.0, 15.0, 5.0, 13.0);
    arc.cubicTo(3.0, 10.0, 4.0, 6.0, 7.0, 5.0);
    arc.cubicTo(8.0, 4.0, 9.0, 4.0, 10.0, 4.0);

    const QPolygonF arrow{QPointF(14.5, 4.0), QPointF(9.0, 2.0), QPointF(9.0, 6.0)};
    QPainterPathStroker stroker{};
    stroker.setWidth(1.8);
    stroker.setCapStyle(Qt::RoundCap);
    stroker.setJoinStyle(Qt::RoundJoin);
    const QRectF loopBounds = stroker.createStroke(arc).boundingRect().united(arrow.boundingRect());
    constexpr qreal kLoopViewBoxExtent = 18.0;
    const qreal fitScale = kLoopViewBoxExtent / std::max(loopBounds.width(), loopBounds.height());
    painter.translate(10.0, 10.0);
    painter.scale(fitScale, fitScale);
    painter.translate(-loopBounds.center());
    painter.setPen(QPen(QColor(165, 216, 243), 1.8, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.drawPath(arc);
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(165, 216, 243));
    painter.drawPolygon(arrow);
    return QIcon(pixels);
}

QIcon soundRemoveIcon()
{
    QPixmap pixels(40, 40);
    pixels.setDevicePixelRatio(2.0);
    pixels.fill(Qt::transparent);
    QPainter painter(&pixels);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.translate(10, 10);
    painter.scale(0.98, 0.98);
    painter.translate(-10, -10);
    painter.setPen(QPen(QColor(220, 69, 69), 2.5, Qt::SolidLine, Qt::RoundCap));
    painter.drawLine(QPointF(4, 4), QPointF(16, 16));
    painter.drawLine(QPointF(16, 4), QPointF(4, 16));
    return QIcon(pixels);
}

QString soundColumnHelp(const int column)
{
    switch ( column )
    {
    case 1:
        return QStringLiteral("WAV resource name. Double-click to choose a sound; none is a silent alternative.");
    case 2:
        return QStringLiteral("Engine playback flags. Double-click to edit named flags; unknown bits are retained.");
    case 3:
        return QStringLiteral("Minimum radius in engine world units. SED distance display: %1 world unit = 1 m. Full "
                              "volume inside this radius.").arg(ui::formatUiNumber(0.1));
    case 4:
        return QStringLiteral("Maximum radius in engine world units. SED distance display: %1 world unit = 1 m. Sound "
                              "is silent beyond this radius; it must be at least the minimum radius.").arg(ui::formatUiNumber(0.1));
    case 5:
        return QStringLiteral(
            "Maximum volume. Explicit slider edits use 0 to 1. A higher imported value is retained until you edit it.");
    default:
        return QStringLiteral("Preview, loop, or remove this alternative.");
    }
}

class SoundScalarDelegate final : public QStyledItemDelegate
{
  public:
    explicit SoundScalarDelegate(TableEditorCreated editorCreated, QWidget* parent,
        const bool numericDisplay = false)
        : QStyledItemDelegate(parent), editorCreated_(std::move(editorCreated)),
          numericDisplay_(numericDisplay),
          sliderStyleWitness_(new SoundVolumeSlider(parent)),
          valueStyleWitness_(std::make_unique<QLineEdit>())
    {
        sliderStyleWitness_->setRange(0, 1000);
        sliderStyleWitness_->hide();
        sliderStyleWitness_->ensurePolished();
        QLineEdit* const valueWitness = valueStyleWitness_.get();
        valueWitness->setObjectName(QStringLiteral("snd.volume.text.paintWitness"));
        valueWitness->setStyle(parent ? parent->style() : QApplication::style());
        configureSoundVolumeValueEdit(valueWitness);
        valueWitness->setFocusPolicy(Qt::NoFocus);
        valueWitness->setAttribute(Qt::WA_TransparentForMouseEvents);
        valueWitness->hide();
        valueWitness->ensurePolished();
    }

    QString displayText(const QVariant& value, const QLocale& locale) const override
    {
        if ( numericDisplay_ )
        {
            const QString raw = value.toString();
            return parseFiniteFloat(raw) ? ui::localizeUiNumericText(raw) : raw;
        }
        return QStyledItemDelegate::displayText(value, locale);
    }

    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override
    {
        painter->save();
        painter->setClipRect(option.rect, Qt::IntersectClip);
        const auto kind = static_cast<SoundTreeItemKind>(index.siblingAtColumn(0).data(kSoundTreeItemKindRole).toInt());
        QStyle* const itemStyle = option.widget ? option.widget->style() : QApplication::style();
        if ( kind == SoundTreeItemKind::mode )
        {
            QStyleOptionViewItem background(option);
            initStyleOption(&background, index);
            background.text.clear();
            background.state &= ~(QStyle::State_Selected | QStyle::State_MouseOver | QStyle::State_HasFocus);
            itemStyle->drawControl(QStyle::CE_ItemViewItem, &background, painter, option.widget);
            if ( index.column() != 0 )
            {
                painter->restore();
                return;
            }
            const int slot = index.data(kSoundTreeModeSlotRole).toInt();
            const QString badgeText = slot > 0 ? QString::number(slot) : QStringLiteral("—");
            QFont badgeFont = background.font;
            badgeFont.setPointSizeF(std::max(9.0, badgeFont.pointSizeF() - 2.0));
            const int badgeWidth = std::max(32, QFontMetrics(badgeFont).horizontalAdvance(badgeText) + 14);
            const QRect badge(option.rect.left() + 5, option.rect.center().y() - 11, badgeWidth, 22);
            painter->setPen(QColor(93, 105, 118));
            painter->setBrush(QColor(58, 66, 75));
            painter->drawRoundedRect(badge.adjusted(0, 0, -1, -1), 4, 4);
            painter->setFont(badgeFont);
            painter->setPen(QColor(206, 225, 240));
            painter->drawText(badge, Qt::AlignCenter, badgeText);
            painter->setFont(background.font);
            painter->setPen(background.palette.color(QPalette::Text));
            painter->drawText(option.rect.adjusted(badgeWidth + 15, 0, -8, 0), Qt::AlignLeft | Qt::AlignVCenter,
                              index.data(Qt::DisplayRole).toString());
            painter->restore();
            return;
        }
        if ( kind == SoundTreeItemKind::alternative || kind == SoundTreeItemKind::addAlternative )
        {
            painter->fillRect(option.rect, QColor(48, 48, 48));
        }
        if ( kind == SoundTreeItemKind::columnHeader )
        {
            QStyleOptionHeader heading;
            heading.rect = option.rect;
            heading.state = QStyle::State_Enabled | QStyle::State_Horizontal;
            heading.palette = option.palette;
            heading.text = index.data(Qt::DisplayRole).toString();
            const QVariant alignment = index.data(Qt::TextAlignmentRole);
            heading.textAlignment = alignment.isValid()
                ? static_cast<Qt::Alignment>(alignment.toInt()) : Qt::AlignLeft | Qt::AlignVCenter;
            heading.section = index.column();
            itemStyle->drawControl(QStyle::CE_Header, &heading, painter, option.widget);
            painter->restore();
            return;
        }
        if ( kind != SoundTreeItemKind::alternative || index.column() != 5 )
        {
            QStyleOptionViewItem cell(option);
            cell.state &= ~(QStyle::State_Selected | QStyle::State_MouseOver | QStyle::State_HasFocus);
            QStyledItemDelegate::paint(painter, cell, index);
            painter->restore();
            return;
        }
        const auto* const view = qobject_cast<const QAbstractItemView*>(option.widget);
        if ( view && view->indexWidget(index) )
        {
            painter->restore();
            return;
        }
        QStyleOptionViewItem background(option);
        initStyleOption(&background, index);
        background.text.clear();
        background.state &= ~(QStyle::State_Selected | QStyle::State_MouseOver | QStyle::State_HasFocus);
        itemStyle->drawControl(QStyle::CE_ItemViewItem, &background, painter, option.widget);
        const QString raw = index.data(Qt::DisplayRole).toString();
        const auto value = parseFiniteFloat(raw);
        const QFontMetrics metrics(sliderStyleWitness_->font());
        const QRect canvas = soundVolumeCanvasRect(option.rect, view ? view->viewport() : nullptr);
        if ( canvas.isEmpty() )
        {
            painter->restore();
            return;
        }
        painter->fillRect(canvas, QColor(48, 48, 48));
        const QRect sliderRect = soundVolumeSliderRect(canvas, metrics);
        if ( sliderRect.width() > 0 )
        {
            // Native styles may use both the option and its widget while
            // splitting the filled and unfilled track. Keep the one reusable
            // witness consistent with the real editor, in local coordinates.
            const QSignalBlocker blocker(sliderStyleWitness_);
            sliderStyleWitness_->resize(sliderRect.size());
            sliderStyleWitness_->setValue(value ? static_cast<int>(std::lround(
                std::clamp(*value, 0.0f, 1.0f) * 1000.0f)) : 0);
            QStyleOptionSlider slider = sliderStyleWitness_->paintOption();
            slider.subControls = QStyle::SC_SliderGroove | QStyle::SC_SliderHandle;
            painter->save();
            painter->setClipRect(sliderRect, Qt::IntersectClip);
            painter->translate(sliderRect.topLeft());
            sliderStyleWitness_->style()->drawComplexControl(QStyle::CC_Slider, &slider, painter,
                                                              sliderStyleWitness_);
            painter->restore();
        }
        const QRect label = soundVolumeValueRect(canvas, metrics);
        if ( !label.isEmpty() )
        {
            QLineEdit* const valueWitness = valueStyleWitness_.get();
            const QSignalBlocker blocker(valueWitness);
            valueWitness->setFont(view ? view->viewport()->font() : sliderStyleWitness_->font());
            QPalette valuePalette = view ? view->viewport()->palette() : option.palette;
            valuePalette.setColor(QPalette::Text,
                                  option.palette.color(QPalette::Text));
            valueWitness->setPalette(valuePalette);
            valueWitness->setText(metrics.elidedText(displayText(raw, ui::uiNumericLocale()),
                Qt::ElideRight, std::max(0, label.width() - 4)));
            valueWitness->setFixedWidth(label.width());
            valueWitness->setGeometry(QRect(QPoint{}, label.size()));
            const QTransform deviceTransform = painter->deviceTransform();
            const QPointF deviceOrigin = deviceTransform.map(QPointF(label.topLeft()));
            const QPointF deviceAnchor(std::floor(deviceOrigin.x()), std::floor(deviceOrigin.y()));
            const QPointF devicePhase = deviceOrigin - deviceAnchor;
            const qreal devicePixelRatio = painter->device()
                ? painter->device()->devicePixelRatioF() : 1.0;
            const QSize rasterSize(
                std::max(1, static_cast<int>(std::ceil(label.width() * devicePixelRatio + devicePhase.x()))),
                std::max(1, static_cast<int>(std::ceil(label.height() * devicePixelRatio + devicePhase.y()))));
            if ( valueRaster_.size() != rasterSize
                || valueRaster_.devicePixelRatioF() != devicePixelRatio )
            {
                valueRaster_ = QPixmap(rasterSize);
                valueRaster_.setDevicePixelRatio(devicePixelRatio);
            }
            valueRaster_.fill(Qt::transparent);
            QPainter rasterPainter(&valueRaster_);
            rasterPainter.translate(devicePhase.x() / devicePixelRatio,
                                    devicePhase.y() / devicePixelRatio);
            valueWitness->render(&rasterPainter, QPoint{}, QRegion{}, QWidget::DrawChildren);
            rasterPainter.end();
            bool transformInvertible = false;
            const QTransform inverseDeviceTransform = deviceTransform.inverted(&transformInvertible);
            const QPointF compositeOrigin = transformInvertible
                ? inverseDeviceTransform.map(deviceAnchor) : QPointF(label.topLeft());
            painter->save();
            painter->setClipRect(label, Qt::IntersectClip);
            painter->drawPixmap(compositeOrigin, valueRaster_);
            painter->restore();
        }
        painter->restore();
    }

    bool helpEvent(QHelpEvent* event, QAbstractItemView* view, const QStyleOptionViewItem& option,
                   const QModelIndex& index) override
    {
        if ( event && (index.column() == 3 || index.column() == 4) )
        {
            // Build numeric examples lazily; a policy change does not rewrite
            // tree items or visit the retained authored rows.
            QToolTip::showText(event->globalPos(), soundColumnHelp(index.column()), view);
            return true;
        }
        if ( event && index.column() == 5 &&
             index.siblingAtColumn(0).data(kSoundTreeItemKindRole).toInt() ==
                 static_cast<int>(SoundTreeItemKind::alternative) )
        {
            const QString raw = displayText(index.data(Qt::DisplayRole), ui::uiNumericLocale());
            const QFontMetrics metrics(sliderStyleWitness_->font());
            const QRect canvas = soundVolumeCanvasRect(option.rect, view ? view->viewport() : nullptr);
            if ( metrics.horizontalAdvance(raw) > soundVolumeValueRect(canvas, metrics).width() - 4 )
            {
                QToolTip::showText(event->globalPos(), soundColumnHelp(5) + QStringLiteral("\n") + raw, view);
                return true;
            }
        }
        return QStyledItemDelegate::helpEvent(event, view, option, index);
    }

    QWidget* createEditor(QWidget* parent, const QStyleOptionViewItem&, const QModelIndex& index) const override
    {
        if ( index.column() < 3 || index.column() > 5 ||
             index.siblingAtColumn(0).data(kSoundTreeItemKindRole).toInt() !=
                 static_cast<int>(SoundTreeItemKind::alternative) )
        {
            return nullptr;
        }
        auto* const delegate = const_cast<SoundScalarDelegate*>(this);
        QWidget* result = nullptr;
        if ( index.column() == 5 )
        {
            auto* const editor = new QWidget(parent);
            editor->setObjectName(QStringLiteral("snd.volume.editor"));
            editor->setStyleSheet(QStringLiteral(
                "QWidget[objectName=\"snd.volume.editor\"] { background: #303030; border: none; }"));
            editor->setAutoFillBackground(true);
            QPalette palette = editor->palette();
            palette.setColor(QPalette::Window, QColor(48, 48, 48));
            editor->setPalette(palette);
            auto* const slider = new SoundVolumeSlider(editor);
            slider->setObjectName(QStringLiteral("snd.volume.slider"));
            slider->setRange(0, 1000);
            slider->setSingleStep(10);
            slider->setPageStep(100);
            slider->setMinimumWidth(0);
            auto* const text = new QLineEdit(editor);
            text->setObjectName(QStringLiteral("snd.volume.text"));
            configureSoundVolumeValueEdit(text);
            text->setLocale(ui::uiNumericLocale());
            connect(text, &QLineEdit::textChanged, text, [text](const QString& raw)
                {
                    text->setToolTip(raw);
                });
            connect(text, &QLineEdit::textEdited, editor, [editor](const QString& raw)
                {
                    editor->setProperty("snd.numeric.pending",
                        raw != editor->property("snd.numeric.originalDisplay").toString());
                });
            ui::onUiNumericFormatChanged(editor, [editor, text]()
                {
                    const QString original = editor->property("snd.numeric.originalText").toString();
                    if ( !editor->property("snd.numeric.pending").toBool() && parseFiniteFloat(original) )
                    {
                        const QSignalBlocker blocker(text);
                        text->setLocale(ui::uiNumericLocale());
                        text->setText(ui::localizeUiNumericText(original));
                        editor->setProperty("snd.numeric.originalDisplay", text->text());
                        text->setToolTip(text->text());
                    }
                });
            connect(slider, &QSlider::valueChanged, editor,
                    [editor, text](const int value)
                    {
                        text->setLocale(ui::uiNumericLocale());
                        text->setText(ui::localizeUiNumericText(floatText(static_cast<float>(value) / 1000.0f)));
                        editor->setProperty("snd.numeric.pending",
                            text->text() != editor->property("snd.numeric.originalDisplay").toString());
                    });
            connect(slider, &QSlider::sliderReleased, delegate,
                    [delegate, editor]()
                    {
                        Q_EMIT delegate->commitData(editor);
                    });
            connect(slider, &QSlider::actionTriggered, editor,
                    [delegate, editor, slider](int)
                    {
                        if ( !slider->isSliderDown() )
                        {
                            QTimer::singleShot(0, editor,
                                               [delegate, editor]()
                                               {
                                                   Q_EMIT delegate->commitData(editor);
                                               });
                        }
                    });
            connect(text, &QLineEdit::editingFinished, delegate,
                    [delegate, editor]()
                    {
                        Q_EMIT delegate->commitData(editor);
                    });
            result = editor;
        }
        else
        {
            auto* const editor = new ui::ScrubFloatEdit(parent);
            editor->setObjectName(QStringLiteral("snd.numeric.editor"));
            editor->setDecimals(9);
            editor->setRange(0.0, static_cast<double>(std::numeric_limits<float>::max()));
            editor->setSingleStep(0.01);
            editor->setStepperButtonWidth(16);
            editor->lineEdit()->setAlignment(Qt::AlignCenter);
            connect(editor->lineEdit(), &QLineEdit::textEdited, editor, [editor](const QString& raw)
                {
                    editor->setProperty("snd.numeric.pending",
                        raw != editor->property("snd.numeric.originalDisplay").toString());
                });
            connect(editor, &ui::ScrubFloatEdit::scrubValueChanged, editor, [editor]()
                {
                    editor->setProperty("snd.numeric.pending", true);
                });
            connect(editor, &ui::ScrubFloatEdit::valueCommitted, editor, [editor](const double value)
                {
                    const auto original = parseFiniteFloat(editor->property("snd.numeric.originalText").toString());
                    if ( !original || static_cast<float>(value) != *original )
                    {
                        editor->setProperty("snd.numeric.pending", true);
                    }
                });
            ui::onUiNumericFormatChanged(editor, [editor]()
                {
                    const QString original = editor->property("snd.numeric.originalText").toString();
                    if ( const auto parsed = parseFiniteFloat(original);
                        parsed && !editor->property("snd.numeric.pending").toBool() )
                    {
                        editor->setValue(*parsed);
                        editor->lineEdit()->setText(ui::localizeUiNumericText(original));
                        editor->setProperty("snd.numeric.originalDisplay", editor->lineEdit()->text());
                    }
                });
            const auto commit = [delegate, editor]()
            {
                const QPointer<QStyledItemDelegate> guardedDelegate(delegate);
                const QPointer<ui::ScrubFloatEdit> guardedEditor(editor);
                Q_EMIT delegate->commitData(editor);
                if ( guardedDelegate && guardedEditor )
                {
                    Q_EMIT guardedDelegate->closeEditor(guardedEditor.data());
                }
            };
            connect(editor, &ui::ScrubFloatEdit::valueCommitted, delegate, commit);
            connect(editor, &ui::ScrubFloatEdit::scrubFinished, delegate, commit);
            result = editor;
        }
        if ( editorCreated_ )
        {
            editorCreated_(result, index);
        }
        return result;
    }

    void setEditorData(QWidget* editor, const QModelIndex& index) const override
    {
        const QString raw = index.data(Qt::EditRole).toString();
        const std::optional<float> parsed = parseFiniteFloat(raw);
        editor->setProperty("snd.numeric.originalText", raw);
        editor->setProperty("snd.numeric.pending", false);
        if ( auto* const scrub = qobject_cast<ui::ScrubFloatEdit*>(editor) )
        {
            if ( parsed )
            {
                scrub->setValue(*parsed);
            }
            scrub->lineEdit()->setLocale(ui::uiNumericLocale());
            scrub->lineEdit()->setText(parsed ? ui::localizeUiNumericText(raw) : raw);
            editor->setProperty("snd.numeric.originalDisplay", scrub->lineEdit()->text());
            scrub->lineEdit()->selectAll();
        }
        else if ( editor )
        {
            QSlider* const slider = editor->findChild<QSlider*>(QStringLiteral("snd.volume.slider"));
            QLineEdit* const text = editor->findChild<QLineEdit*>(QStringLiteral("snd.volume.text"));
            if ( slider && text )
            {
                const QSignalBlocker blocker(slider);
                slider->setValue(parsed ? static_cast<int>(std::lround(std::clamp(*parsed, 0.0f, 1.0f) * 1000.0f)) : 0);
                text->setLocale(ui::uiNumericLocale());
                text->setText(parsed ? ui::localizeUiNumericText(raw) : raw);
                editor->setProperty("snd.numeric.originalDisplay", text->text());
                text->setToolTip(text->text());
            }
        }
    }

    void setModelData(QWidget* editor, QAbstractItemModel* model, const QModelIndex& index) const override
    {
        if ( !editor )
        {
            return;
        }
        QLineEdit* text{};
        if ( auto* const scrub = qobject_cast<ui::ScrubFloatEdit*>(editor) )
        {
            text = scrub->lineEdit();
        }
        else
        {
            text = editor->findChild<QLineEdit*>(QStringLiteral("snd.volume.text"));
        }
        if ( !text )
        {
            return;
        }
        QString raw = editor->property("snd.numeric.originalText").toString();
        if ( editor->property("snd.numeric.pending").toBool() )
        {
            raw = text->text();
            const QString canonical = ui::canonicalUiNumericText(raw, text->locale());
            if ( const auto parsed = parseFiniteFloat(canonical) )
            {
                raw = index.column() == 5 ? floatText(std::clamp(*parsed, 0.0f, 1.0f)) : canonical;
                if ( index.column() == 5 )
                {
                    text->setLocale(ui::uiNumericLocale());
                    text->setText(ui::localizeUiNumericText(raw));
                }
            }
        }
        // Publish canonical accepted text before the synchronous itemChanged
        // callback reads this editor. Unfinished invalid text stays verbatim.
        editor->setProperty("snd.numeric.originalText", raw);
        editor->setProperty("snd.numeric.originalDisplay", text->text());
        editor->setProperty("snd.numeric.pending", false);
        model->setData(index, raw, Qt::EditRole);
    }

    void updateEditorGeometry(QWidget* editor, const QStyleOptionViewItem& option,
                              const QModelIndex& index) const override
    {
        const QRect geometry = index.column() == 5
            ? soundVolumeCanvasRect(option.rect, editor->parentWidget()) : option.rect;
        editor->setGeometry(geometry);
        if ( index.column() == 5 )
        {
            const QRect cell(QPoint{}, geometry.size());
            const QFontMetrics metrics(editor->font());
            if ( QSlider* const slider = editor->findChild<QSlider*>(QStringLiteral("snd.volume.slider")) )
            {
                slider->setGeometry(soundVolumeSliderRect(cell, metrics));
            }
            if ( QLineEdit* const text = editor->findChild<QLineEdit*>(QStringLiteral("snd.volume.text")) )
            {
                const QRect valueRect = soundVolumeValueRect(cell, metrics);
                text->setFixedWidth(valueRect.width());
                text->setGeometry(valueRect);
            }
        }
    }

  private:
    TableEditorCreated editorCreated_{};
    bool numericDisplay_{};
    SoundVolumeSlider* sliderStyleWitness_{};
    std::unique_ptr<QLineEdit> valueStyleWitness_{};
    mutable QPixmap valueRaster_{};
};
} // namespace

void ResourceAuthoringDialog::buildSoundEditor()
{
    static std::atomic<std::uint64_t> nextPlaybackLease{1U};
    soundPlaybackLease_ = nextPlaybackLease.fetch_add(1U, std::memory_order_relaxed);
    soundAddModeButton_ = makeButton(QStringLiteral("＋ Add sound mode"), "snd.add_mode");
    auto* const modeActions = new QHBoxLayout;
    modeActions->addWidget(soundAddModeButton_);
    modeActions->addStretch(1);
    QPushButton* const expandAll = makeButton(QStringLiteral("Expand All"), "snd.expand_all");
    QPushButton* const collapseAll = makeButton(QStringLiteral("Collapse All"), "snd.collapse_all");
    modeActions->addWidget(expandAll);
    modeActions->addWidget(collapseAll);
    contentLayout_->addLayout(modeActions);
    connect(soundAddModeButton_, &QPushButton::clicked, this, &ResourceAuthoringDialog::openSoundModePopup);
    connect(expandAll, &QPushButton::clicked, this,
            [this]()
            {
                setAllSoundModesExpanded(true);
            });
    connect(collapseAll, &QPushButton::clicked, this,
            [this]()
            {
                setAllSoundModesExpanded(false);
            });
    soundSearch_ = new QLineEdit(contentHost_);
    soundSearch_->setObjectName(QStringLiteral("snd.search"));
    soundSearch_->setPlaceholderText(QStringLiteral("Search sound modes"));
    soundSearch_->setClearButtonEnabled(true);
    soundSearch_->installEventFilter(this);
    contentLayout_->addWidget(soundSearch_);

    soundEntries_ = new SoundEntriesTree(contentHost_);
    soundEntries_->setObjectName(QStringLiteral("snd.entries"));
    soundEntries_->setColumnCount(6);
    soundEntries_->header()->hide();
    soundEntries_->setHeaderLabels({QString{}, QStringLiteral("Sound"), QStringLiteral("Flags"),
                                    QStringLiteral("Min"), QStringLiteral("Max"),
                                    QStringLiteral("Volume")});
    for ( const int column : {1, 2, 3, 4, 5} )
    {
        const Qt::Alignment alignment = column >= 2 && column <= 4
            ? Qt::AlignHCenter | Qt::AlignVCenter : Qt::AlignLeft | Qt::AlignVCenter;
        soundEntries_->headerItem()->setTextAlignment(column, alignment);
    }
    soundEntries_->setSelectionMode(QAbstractItemView::NoSelection);
    soundEntries_->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed);
    soundEntries_->setSortingEnabled(false);
    soundEntries_->setUniformRowHeights(false);
    soundEntries_->setAnimated(true);
    soundEntries_->setExpandsOnDoubleClick(false);
    soundEntries_->setIndentation(14);
    soundEntries_->setMouseTracking(true);
    soundEntries_->viewport()->setMouseTracking(true);
    soundEntries_->installEventFilter(this);
    soundEntries_->viewport()->installEventFilter(this);
    soundEntries_->header()->setSectionResizeMode(QHeaderView::Interactive);
    soundEntries_->header()->setMinimumSectionSize(1);
    soundEntries_->header()->setMaximumSectionSize(16384);
    soundEntries_->header()->setStretchLastSection(false);
    soundEntries_->horizontalScrollBar()->installEventFilter(this);
    soundEntries_->verticalScrollBar()->installEventFilter(this);
    for ( int column = 0; column < static_cast<int>(kSoundColumnDefaultWidths.size()); ++column )
    {
        soundEntries_->setColumnWidth(column, kSoundColumnDefaultWidths[static_cast<std::size_t>(column)]);
    }
    const auto trackEditor = [this](QWidget* editor, const QModelIndex& index)
        {
            trackTableEditor(soundEntries_, editor, index);
        };
    soundEntries_->setItemDelegate(new SoundScalarDelegate(trackEditor, soundEntries_));
    auto* const numericDelegate = new SoundScalarDelegate(trackEditor, soundEntries_, true);
    for ( int column = 3; column <= 5; ++column )
    {
        soundEntries_->setItemDelegateForColumn(column, numericDelegate);
    }
    contentLayout_->addWidget(soundEntries_, 1);

    soundUndoAction_ = new QAction(QStringLiteral("Undo"), soundEntries_);
    soundUndoAction_->setObjectName(QStringLiteral("snd.undo"));
    soundUndoAction_->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Z));
    soundUndoAction_->setToolTip(QStringLiteral("Undo sound entry, value or mode change (%1)")
        .arg(shortcuts::keySequenceDisplayText(QKeySequence(Qt::CTRL | Qt::Key_Z))));
    soundRedoAction_ = new QAction(QStringLiteral("Redo"), soundEntries_);
    soundRedoAction_->setObjectName(QStringLiteral("snd.redo"));
    soundRedoAction_->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_Z));
    soundRedoAction_->setToolTip(QStringLiteral("Redo sound entry, value or mode change (%1)")
        .arg(shortcuts::keySequenceDisplayText(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_Z))));
    // The tree filter and MainWindow own keyboard routing. These actions are
    // attached only to the body menu, preserving native line-edit Undo.
    connect(soundUndoAction_, &QAction::triggered, this,
            [this]()
            {
                undoRedoSoundEdit(false);
            });
    connect(soundRedoAction_, &QAction::triggered, this,
            [this]()
            {
                undoRedoSoundEdit(true);
            });
    soundEntries_->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(soundEntries_, &QWidget::customContextMenuRequested, this,
            [this](const QPoint& position)
            {
                shortcuts::KeySequenceDisplayMenu menu;
                menu.addAction(soundUndoAction_);
                menu.addAction(soundRedoAction_);
                connect(this, &QObject::destroyed, &menu, &QMenu::close);
                menu.exec(soundEntries_->viewport()->mapToGlobal(position));
            });
    updateSoundHistoryActions();

    soundPlayButton_ = new QToolButton(soundEntries_->viewport());
    soundPlayButton_->setObjectName(QStringLiteral("snd.play"));
    soundPlayButton_->setIcon(soundPreviewIcon(false));
    soundPlayButton_->setIconSize(QSize(13, 13));
    soundPlayButton_->setToolTip(QStringLiteral("Preview this alternative"));
    soundLoopButton_ = new QToolButton(soundEntries_->viewport());
    soundLoopButton_->setObjectName(QStringLiteral("snd.loop"));
    soundLoopButton_->setIcon(soundLoopIcon());
    soundLoopButton_->setIconSize(QSize(13, 13));
    soundLoopButton_->setCheckable(true);
    soundLoopButton_->setStyleSheet(QStringLiteral("QToolButton { color: #a5d8f3; }"));
    soundLoopButton_->setToolTip(QStringLiteral("Loop this preview"));
    soundRemoveButton_ = new QToolButton(soundEntries_->viewport());
    soundRemoveButton_->setObjectName(QStringLiteral("snd.remove"));
    soundRemoveButton_->setIcon(soundRemoveIcon());
    soundRemoveButton_->setIconSize(QSize(13, 13));
    soundRemoveButton_->setStyleSheet(QStringLiteral("QToolButton { color: #dc4545; font-weight: bold; }"));
    soundRemoveButton_->setToolTip(QStringLiteral("Remove this alternative"));
    soundPlayingStopButton_ = new QToolButton(soundEntries_->viewport());
    soundPlayingStopButton_->setObjectName(QStringLiteral("snd.playing.stop"));
    soundPlayingStopButton_->setIcon(soundPreviewIcon(true));
    soundPlayingStopButton_->setIconSize(QSize(13, 13));
    soundPlayingStopButton_->setToolTip(QStringLiteral("Stop this preview"));
    soundPlayingLoopButton_ = new QToolButton(soundEntries_->viewport());
    soundPlayingLoopButton_->setObjectName(QStringLiteral("snd.playing.loop"));
    soundPlayingLoopButton_->setIcon(soundLoopIcon());
    soundPlayingLoopButton_->setIconSize(QSize(13, 13));
    soundPlayingLoopButton_->setCheckable(true);
    soundPlayingLoopButton_->setStyleSheet(QStringLiteral("QToolButton { color: #a5d8f3; }"));
    soundPlayingLoopButton_->setToolTip(QStringLiteral("Loop this preview"));
    for ( QToolButton* const button : {soundPlayButton_, soundLoopButton_, soundRemoveButton_,
                                       soundPlayingStopButton_, soundPlayingLoopButton_} )
    {
        button->setAutoRaise(true);
        button->setStyleSheet(button->styleSheet() +
                             QStringLiteral("QToolButton[autoRaise=\"true\"] { padding: 0px; }"));
        button->setFocusPolicy(Qt::NoFocus);
        button->setMouseTracking(true);
        button->installEventFilter(this);
        button->hide();
    }
    soundPlaybackWatchTimer_ = new QTimer(this);
    soundPlaybackWatchTimer_->setInterval(80);
    soundPlaybackWatchTimer_->setTimerType(Qt::CoarseTimer);
    connect(soundPlaybackWatchTimer_, &QTimer::timeout, this,
            [this]()
            {
                updateSoundPlayingControls();
            });
    connect(soundPlayButton_, &QToolButton::clicked, this,
            [this]()
            {
                if ( soundHoveredIndex_.isValid() )
                {
                    playSoundAlternative(soundHoveredIndex_.data(kSoundTreeSourceIndexRole).toInt());
                }
            });
    connect(soundLoopButton_, &QToolButton::clicked, this,
            [this](const bool loop)
            {
                if ( soundHoveredIndex_.isValid() )
                {
                    setSoundPreviewLoop(soundHoveredIndex_.data(kSoundTreeSourceIndexRole).toInt(), loop);
                }
            });
    connect(soundPlayingStopButton_, &QToolButton::clicked, this,
            [this]()
            {
                if ( soundPlayingRow_ < 0 )
                {
                    return;
                }
                stopSoundPreview();
                if ( soundHoveredIndex_.isValid() )
                {
                    updateSoundHoverControls(soundEntries_->visualRect(soundHoveredIndex_).center());
                }
                setStatus(QStringLiteral("Preview stopped."));
            });
    connect(soundPlayingLoopButton_, &QToolButton::clicked, this,
            [this](const bool loop)
            {
                if ( soundPlayingRow_ >= 0 )
                {
                    setSoundPreviewLoop(soundPlayingRow_, loop);
                }
            });
    connect(soundRemoveButton_, &QToolButton::clicked, this,
            [this]()
            {
                if ( soundHoveredIndex_.isValid() )
                {
                    removeSoundEntry(soundHoveredIndex_.data(kSoundTreeSourceIndexRole).toInt());
                }
            });
    const auto updateHoverGeometry = [this]()
    {
        if ( soundHoveredIndex_.isValid() )
        {
            updateSoundHoverControls(soundEntries_->visualRect(soundHoveredIndex_).center());
        }
        updateSoundPlayingControls();
    };
    connect(soundEntries_->header(), &QHeaderView::sectionResized, this,
            [this, updateHoverGeometry](const int column, int, const int width)
            {
                if ( column < 0 || column >= static_cast<int>(kSoundColumnMinimumWidths.size()) )
                {
                    return;
                }
                if ( applyingEditorPresentation_
                    || soundEntries_->property("snd.resizing_columns").toBool() )
                {
                    return;
                }
                const int minimum = kSoundColumnMinimumWidths[static_cast<std::size_t>(column)];
                std::array<int, 6> nextWidths{};
                int totalWidth = 0;
                for ( int index = 0; index < soundEntries_->columnCount(); ++index )
                {
                    nextWidths[static_cast<std::size_t>(index)] = std::clamp(
                        soundEntries_->columnWidth(index),
                        kSoundColumnMinimumWidths[static_cast<std::size_t>(index)],
                        kSoundColumnMaximumWidth);
                }
                nextWidths[static_cast<std::size_t>(column)] = std::clamp(
                    width, minimum, kSoundColumnMaximumWidth);
                for ( const int nextWidth : nextWidths )
                {
                    totalWidth += nextWidth;
                }
                int excess = std::max(0, totalWidth - kSoundColumnsMaximumTotalWidth);
                for ( int pass = 0; pass < 2 && excess > 0; ++pass )
                {
                    for ( int index = 0; index < soundEntries_->columnCount() && excess > 0; ++index )
                    {
                        const bool isChangedColumn = index == column;
                        if ( isChangedColumn != (pass == 1) )
                        {
                            continue;
                        }
                        const std::size_t widthIndex = static_cast<std::size_t>(index);
                        const int reducible = nextWidths[widthIndex]
                            - kSoundColumnMinimumWidths[widthIndex];
                        const int reduction = std::min(reducible, excess);
                        nextWidths[widthIndex] -= reduction;
                        excess -= reduction;
                    }
                }
                soundEntries_->setProperty("snd.resizing_columns", true);
                for ( int index = 0; index < soundEntries_->columnCount(); ++index )
                {
                    soundEntries_->setColumnWidth(index, nextWidths[static_cast<std::size_t>(index)]);
                }
                soundEntries_->setProperty("snd.resizing_columns", false);
                soundManualColumnWidths_ = true;
                const QPointer<ResourceAuthoringDialog> self(this);
                resizeSoundWavColumn(false, column);
                if ( self )
                {
                    updateHoverGeometry();
                }
            });
    const auto cancelStalePresentationReferences = [this]()
    {
        if ( !pendingTableEditorCarets_.isEmpty() || deferredPresentationScroll_.pending )
        {
            cancelDeferredEditorPresentation();
        }
    };
    QAbstractItemModel* const soundModel = soundEntries_->model();
    connect(soundModel, &QAbstractItemModel::rowsAboutToBeInserted, this,
        [cancelStalePresentationReferences](const QModelIndex&, int, int)
        {
            cancelStalePresentationReferences();
        });
    connect(soundModel, &QAbstractItemModel::rowsAboutToBeRemoved, this,
        [cancelStalePresentationReferences](const QModelIndex&, int, int)
        {
            cancelStalePresentationReferences();
        });
    connect(soundModel, &QAbstractItemModel::rowsAboutToBeMoved, this,
        [cancelStalePresentationReferences](const QModelIndex&, int, int, const QModelIndex&, int)
        {
            cancelStalePresentationReferences();
        });
    connect(soundModel, &QAbstractItemModel::modelAboutToBeReset, this,
        cancelStalePresentationReferences);
    connect(soundEntries_->verticalScrollBar(), &QScrollBar::valueChanged, this, updateHoverGeometry);
    connect(soundEntries_->horizontalScrollBar(), &QScrollBar::valueChanged, this, updateHoverGeometry);
    connect(soundEntries_, &QTreeWidget::itemChanged, this,
            [this](QTreeWidgetItem* item, const int column)
            {
                updateSoundEntryFromCell(item, column);
            });
    connect(soundEntries_, &QTreeWidget::itemClicked, this,
            [this](QTreeWidgetItem* item, int)
            {
                if ( !item )
                {
                    return;
                }
                const auto kind = static_cast<SoundTreeItemKind>(item->data(0, kSoundTreeItemKindRole).toInt());
                if ( kind == SoundTreeItemKind::addAlternative )
                {
                    const QByteArray mode = item->data(0, kSoundTreeModeBytesRole).toByteArray();
                    appendSoundEntry(std::string(mode.constData(), static_cast<std::size_t>(mode.size())));
                }
                else if ( kind == SoundTreeItemKind::mode )
                {
                    item->setExpanded(!item->isExpanded());
                }
            });
    connect(soundEntries_, &QTreeWidget::itemDoubleClicked, this,
            [this](QTreeWidgetItem* item, const int column)
            {
                if ( !item || sourceIndexForSoundItem(item) < 0 )
                {
                    return;
                }
                soundEntries_->setCurrentItem(item, column);
                if ( column == 1 )
                {
                    pickSoundWav();
                }
                else if ( column == 2 )
                {
                    openSoundClassFlagsPopup(item);
                }
            });
    const auto expansionChanged = [this](QTreeWidgetItem* item, const bool collapsed)
    {
        if ( refreshing_ || !item ||
             item->data(0, kSoundTreeItemKindRole).toInt() != static_cast<int>(SoundTreeItemKind::mode) )
        {
            return;
        }
        const QString mode =
            ui::episode::displayTextForGameBytes(item->data(0, kSoundTreeModeBytesRole).toByteArray().toStdString());
        if ( collapsed )
        {
            collapsedSoundModes_.insert(mode);
        }
        else
        {
            collapsedSoundModes_.remove(mode);
        }
        hideSoundHoverControls();
        updateSoundPlayingControls();
        notifyChanged();
    };
    connect(soundEntries_, &QTreeWidget::itemCollapsed, this,
            [expansionChanged](QTreeWidgetItem* item)
            {
                expansionChanged(item, true);
            });
    connect(soundEntries_, &QTreeWidget::itemExpanded, this,
            [expansionChanged](QTreeWidgetItem* item)
            {
                expansionChanged(item, false);
            });
    connect(soundSearch_, &QLineEdit::textChanged, this,
            [this](const QString& text)
            {
                if ( refreshing_ )
                {
                    return;
                }
                ++presentationInteractionGeneration_;
                {
                    const QSignalBlocker blocker(soundEntries_);
                    const QString query = text.trimmed();
                    for ( int index = 0; index < soundEntries_->topLevelItemCount(); ++index )
                    {
                        QTreeWidgetItem* const group = soundEntries_->topLevelItem(index);
                        if ( !group || group->data(0, kSoundTreeItemKindRole).toInt() !=
                                           static_cast<int>(SoundTreeItemKind::mode) )
                        {
                            continue;
                        }
                        group->setHidden(!query.isEmpty() && !group->text(0).contains(query, Qt::CaseInsensitive));
                        const QString mode = ui::episode::displayTextForGameBytes(
                            group->data(0, kSoundTreeModeBytesRole).toByteArray().toStdString());
                        group->setExpanded(!query.isEmpty() ? !group->isHidden()
                                                            : !collapsedSoundModes_.contains(mode));
                    }
                    hideSoundHoverControls();
                    updateSoundPlayingControls();
                }
                // Qt continues using the line control after emitting textChanged.
                // Publish after that transaction so a host may safely delete this pane.
                if ( !soundSearchPublicationPending_ )
                {
                    soundSearchPublicationPending_ = true;
                    QTimer::singleShot(0, this,
                                       [this]()
                                       {
                                           soundSearchPublicationPending_ = false;
                                           notifyChanged();
                                       });
                }
            });
}

void ResourceAuthoringDialog::hideSoundHoverControls()
{
    soundHoveredIndex_ = QPersistentModelIndex{};
    if ( soundPlayButton_ )
    {
        soundPlayButton_->hide();
    }
    if ( soundLoopButton_ )
    {
        soundLoopButton_->hide();
    }
    if ( soundRemoveButton_ )
    {
        soundRemoveButton_->hide();
    }
}

void ResourceAuthoringDialog::updateSoundPlayingControls()
{
    if ( !soundEntries_ || !soundPlayingStopButton_ || !soundPlayingLoopButton_ )
    {
        return;
    }
    ui::audio::AppSoundPlayer& player = ui::audio::AppSoundPlayer::instance();
    const bool sourceRowValid = soundPlayingRow_ >= 0 &&
        soundPlayingRow_ < static_cast<int>(soundPreviewLoopRows_.size());
    const bool leasePlaying =
        player.isPlaying(ui::audio::SoundPlaybackOwner::resourceEditor, soundPlaybackLease_);
    if ( !sourceRowValid || !leasePlaying )
    {
        const bool playbackEnded = soundPlayingRow_ >= 0;
        if ( !sourceRowValid && leasePlaying )
        {
            // A stale row identity must never leave an unanchored preview
            // running against the pane's playback lease.
            player.stop(ui::audio::SoundPlaybackOwner::resourceEditor, soundPlaybackLease_);
        }
        soundPlayingRow_ = -1;
        if ( soundPlaybackWatchTimer_ )
        {
            soundPlaybackWatchTimer_->stop();
        }
        soundPlayingStopButton_->hide();
        soundPlayingLoopButton_->hide();
        if ( playbackEnded )
        {
            QWidget* const viewport = soundEntries_->viewport();
            const QPoint local = viewport->mapFromGlobal(QCursor::pos());
            if ( viewport->rect().contains(local) )
            {
                updateSoundHoverControls(local);
            }
        }
        return;
    }
    if ( soundPlaybackWatchTimer_ && !soundPlaybackWatchTimer_->isActive() )
    {
        soundPlaybackWatchTimer_->start();
    }

    QTreeWidgetItem* const item = soundAlternativeAt(soundPlayingRow_);
    if ( !item )
    {
        soundPlayingStopButton_->hide();
        soundPlayingLoopButton_->hide();
        return;
    }
    const QRect rowRect = soundEntries_->visualItemRect(item);
    const QRect leftRect = soundEntries_->visualRect(soundEntries_->indexFromItem(item, 0));
    if ( rowRect.isEmpty() || leftRect.isEmpty() ||
         !soundEntries_->viewport()->rect().intersects(rowRect) )
    {
        // A collapsed, filtered, or scrolled-out row keeps playing. Its pinned
        // controls return when the row is visible again.
        soundPlayingStopButton_->hide();
        soundPlayingLoopButton_->hide();
        return;
    }
    const int height = std::min(22, rowRect.height());
    const int y = rowRect.top() + (rowRect.height() - height) / 2;
    soundPlayingStopButton_->setGeometry(leftRect.left() + 2, y, 22, height);
    soundPlayingLoopButton_->setGeometry(leftRect.left() + 26, y, 22, height);
    {
        const QSignalBlocker blocker(soundPlayingLoopButton_);
        soundPlayingLoopButton_->setChecked(soundPreviewLoopRows_[static_cast<std::size_t>(soundPlayingRow_)]);
    }
    soundPlayingStopButton_->show();
    soundPlayingLoopButton_->show();
    soundPlayingStopButton_->raise();
    soundPlayingLoopButton_->raise();
}

void ResourceAuthoringDialog::updateSoundHoverControls(const QPoint& position)
{
    QTreeWidgetItem* const item = soundEntries_ ? soundEntries_->itemAt(position) : nullptr;
    const int row = sourceIndexForSoundItem(item);
    if ( !item || row < 0 || row >= static_cast<int>(soundPreviewLoopRows_.size()) )
    {
        hideSoundHoverControls();
        updateSoundPlayingControls();
        return;
    }
    const QRect rowRect = soundEntries_->visualItemRect(item);
    const QRect leftRect = soundEntries_->visualRect(soundEntries_->indexFromItem(item, 0));
    if ( rowRect.isEmpty() || !soundEntries_->viewport()->rect().intersects(rowRect) )
    {
        hideSoundHoverControls();
        updateSoundPlayingControls();
        return;
    }
    soundHoveredIndex_ = soundEntries_->indexFromItem(item, 0);
    const int height = std::min(22, rowRect.height());
    const int y = rowRect.top() + (rowRect.height() - height) / 2;
    soundPlayButton_->setGeometry(leftRect.left() + 2, y, 22, height);
    soundLoopButton_->setGeometry(leftRect.left() + 26, y, 22, height);
    const bool samePlayingRow = soundPlayingRow_ == row &&
        ui::audio::AppSoundPlayer::instance().isPlaying(ui::audio::SoundPlaybackOwner::resourceEditor,
                                                        soundPlaybackLease_);
    soundPlayButton_->setIcon(soundPreviewIcon(false));
    soundPlayButton_->setToolTip(QStringLiteral("Preview this alternative"));
    const QSignalBlocker blocker(soundLoopButton_);
    soundLoopButton_->setChecked(soundPreviewLoopRows_[static_cast<std::size_t>(row)]);
    if ( samePlayingRow )
    {
        soundPlayButton_->hide();
        soundLoopButton_->hide();
    }
    else
    {
        for ( QToolButton* const button : {soundPlayButton_, soundLoopButton_} )
        {
            button->show();
            button->raise();
        }
    }
    const QRect volume = soundEntries_->visualRect(soundEntries_->indexFromItem(item, 5));
    const QRect remove = soundVolumeRemoveRect(volume, soundEntries_->viewport());
    soundRemoveButton_->setGeometry(remove);
    const bool removeHasSpace = soundVolumeHasRemoveSpace(volume, soundEntries_->viewport());
    soundRemoveButton_->setVisible(removeHasSpace);
    if ( removeHasSpace )
    {
        soundRemoveButton_->raise();
    }
    updateSoundPlayingControls();
}

void ResourceAuthoringDialog::setSoundPreviewLoop(const int row, const bool loop)
{
    if ( row < 0 || row >= static_cast<int>(soundPreviewLoopRows_.size()) )
    {
        return;
    }
    soundPreviewLoopRows_[static_cast<std::size_t>(row)] = loop;
    if ( QTreeWidgetItem* const item = soundAlternativeAt(row) )
    {
        const QSignalBlocker blocker(soundEntries_);
        item->setData(0, kSoundTreePreviewLoopRole, loop);
    }
    if ( soundPlayingRow_ == row &&
         ui::audio::AppSoundPlayer::instance().isPlaying(ui::audio::SoundPlaybackOwner::resourceEditor,
                                                         soundPlaybackLease_) )
    {
        playSoundAlternative(row, false);
    }
    updateSoundPlayingControls();
    notifyChanged();
}

void ResourceAuthoringDialog::playSoundAlternative(const int row, const bool togglePlayback)
{
    if ( row < 0 || row >= static_cast<int>(soundFile_.entries.size()) )
    {
        return;
    }
    ui::audio::AppSoundPlayer& player = ui::audio::AppSoundPlayer::instance();
    if ( togglePlayback && soundPlayingRow_ == row &&
         player.isPlaying(ui::audio::SoundPlaybackOwner::resourceEditor, soundPlaybackLease_) )
    {
        stopSoundPreview();
        soundPlayButton_->setIcon(soundPreviewIcon(false));
        soundPlayButton_->setToolTip(QStringLiteral("Preview this alternative"));
        setStatus(QStringLiteral("Preview stopped."));
        return;
    }
    const SoundRowRecord record = soundRowRecord(row);
    const auto wav = ui::episode::gameBytesForDisplayText(record.raw[0]);
    const auto volume = parseFiniteFloat(record.raw[4]);
    if ( !wav || wav->empty() || *wav == "none" || !volume || !context_.resources )
    {
        stopSoundPreview();
        setStatus(QStringLiteral("This alternative has no playable project sound."));
        return;
    }
    const bool playing = ui::audio::AppSoundPlayer::instance().playResource(
        ui::audio::SoundPlaybackOwner::resourceEditor, *context_.resources, *wav, record.previewLoop,
        std::clamp(static_cast<double>(*volume), 0.0, 1.0), soundPlaybackLease_);
    soundPlayingRow_ = playing ? row : -1;
    if ( !playing && soundPlaybackWatchTimer_ )
    {
        soundPlaybackWatchTimer_->stop();
    }
    soundPlayButton_->setIcon(soundPreviewIcon(false));
    soundPlayButton_->setToolTip(QStringLiteral("Preview this alternative"));
    updateSoundPlayingControls();
    if ( soundHoveredIndex_.isValid() )
    {
        updateSoundHoverControls(soundEntries_->visualRect(soundHoveredIndex_).center());
    }
    setStatus(playing ? QStringLiteral("Previewing %1").arg(record.raw[0])
                      : QStringLiteral("The project sound could not be previewed."),
              !playing);
}

void ResourceAuthoringDialog::stopSoundPreview()
{
    if ( soundPlaybackWatchTimer_ )
    {
        soundPlaybackWatchTimer_->stop();
    }
    if ( soundPlaybackLease_ != 0U )
    {
        ui::audio::AppSoundPlayer::instance().stop(ui::audio::SoundPlaybackOwner::resourceEditor, soundPlaybackLease_);
    }
    soundPlayingRow_ = -1;
    if ( soundPlayingStopButton_ )
    {
        soundPlayingStopButton_->hide();
    }
    if ( soundPlayingLoopButton_ )
    {
        soundPlayingLoopButton_->hide();
    }
}

void ResourceAuthoringDialog::updateSoundPreviewVolume(const int row, const QString& raw)
{
    ui::audio::AppSoundPlayer& player = ui::audio::AppSoundPlayer::instance();
    if ( row != soundPlayingRow_ ||
         !player.isPlaying(ui::audio::SoundPlaybackOwner::resourceEditor, soundPlaybackLease_) )
    {
        return;
    }
    if ( const auto volume = parseFiniteFloat(raw) )
    {
        player.setPlaybackVolume(ui::audio::SoundPlaybackOwner::resourceEditor,
                                 std::clamp(static_cast<double>(*volume), 0.0, 1.0), soundPlaybackLease_);
    }
}

void ResourceAuthoringDialog::openSoundModePopup()
{
    QStringList available = soundModeSuggestions();
    for ( const assets::SoundClassEntry& entry : soundFile_.entries )
    {
        available.removeAll(ui::episode::displayTextForGameBytes(entry.mode));
    }
    for ( const QString& mode : soundPresentationModes_ )
    {
        available.removeAll(mode);
    }
    if ( available.isEmpty() || soundFile_.entries.size() >= 4096U )
    {
        return;
    }
    QPointer<ResourceAuthoringDialog> self(this);
    const std::uint64_t revision = authoredRevision_;
    const QCborMap snapshot = authoredContentState();
    QDialog popup(nullptr);
    popup.setWindowModality(Qt::ApplicationModal);
    popup.setObjectName(QStringLiteral("snd.mode.popup"));
    popup.setWindowTitle(QStringLiteral("Add sound mode"));
    popup.setWindowFlags(popup.windowFlags() & ~Qt::WindowMinMaxButtonsHint);
    connect(this, &QObject::destroyed, &popup, &QDialog::reject);
    auto* const layout = new QVBoxLayout(&popup);
    auto* const search = new QLineEdit(&popup);
    search->setObjectName(QStringLiteral("snd.mode.search"));
    search->setPlaceholderText(QStringLiteral("Search available sound modes"));
    search->setClearButtonEnabled(true);
    layout->addWidget(search);
    auto* const modes = new QListWidget(&popup);
    modes->setObjectName(QStringLiteral("snd.mode.list"));
    modes->addItems(available);
    modes->setCurrentRow(0);
    layout->addWidget(modes);
    auto* const buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &popup);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &popup, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &popup, &QDialog::reject);
    connect(modes, &QListWidget::itemDoubleClicked, &popup, &QDialog::accept);
    connect(search, &QLineEdit::textChanged, &popup,
            [modes, buttons](const QString& query)
            {
                QListWidgetItem* first = nullptr;
                for ( int row = 0; row < modes->count(); ++row )
                {
                    QListWidgetItem* const item = modes->item(row);
                    item->setHidden(!item->text().contains(query.trimmed(), Qt::CaseInsensitive));
                    if ( !item->isHidden() && !first )
                    {
                        first = item;
                    }
                }
                modes->setCurrentItem(first);
                buttons->button(QDialogButtonBox::Ok)->setEnabled(first != nullptr);
            });
    popup.resize(340, 410);
    QPoint position = QCursor::pos();
    if ( QScreen* const screen = QGuiApplication::screenAt(position) )
    {
        const QRect availableGeometry = screen->availableGeometry();
        position.setX(std::clamp(position.x(), availableGeometry.left(),
                                 std::max(availableGeometry.left(), availableGeometry.right() - popup.width())));
        position.setY(std::clamp(position.y(), availableGeometry.top(),
                                 std::max(availableGeometry.top(), availableGeometry.bottom() - popup.height())));
    }
    popup.move(position);
    if ( popup.exec() != QDialog::Accepted || !self || !self->authoringSnapshotStillCurrent(revision, snapshot) )
    {
        return;
    }
    const QListWidgetItem* const selected = modes->currentItem();
    if ( !selected || selected->isHidden() )
    {
        return;
    }
    const QString mode = selected->text();
    const bool represented = std::any_of(self->soundFile_.entries.begin(), self->soundFile_.entries.end(),
                                         [&mode](const assets::SoundClassEntry& entry)
                                         {
                                             return ui::episode::displayTextForGameBytes(entry.mode) == mode;
                                         });
    if ( !represented && !self->soundPresentationModes_.contains(mode) && available.contains(mode) )
    {
        self->recordSoundHistoryEdit(SoundHistoryEdit{-1, std::nullopt, std::nullopt, mode});
        self->soundPresentationModes_.insert(mode);
        self->knownSoundModes_.insert(mode);
        self->collapsedSoundModes_.remove(mode);
        {
            const QSignalBlocker blocker(self->soundSearch_);
            self->soundSearch_->clear();
        }
        self->refreshSoundModeGroups();
        for ( int index = 0; index < self->soundEntries_->topLevelItemCount(); ++index )
        {
            QTreeWidgetItem* const group = self->soundEntries_->topLevelItem(index);
            if ( group->data(0, kSoundTreeModeBytesRole).toByteArray() == mode.toUtf8() )
            {
                const QSignalBlocker blocker(self->soundEntries_);
                group->setExpanded(true);
                self->soundEntries_->setCurrentItem(group);
                self->soundEntries_->scrollToItem(group, QAbstractItemView::PositionAtCenter);
                break;
            }
        }
        self->publishSoundHistoryChange();
    }
}

bool ResourceAuthoringDialog::handleSoundTreePointerEvent(QEvent* event)
{
    if ( !soundEntries_ || !event )
    {
        return false;
    }
    const bool interrupted = event->type() == QEvent::UngrabMouse || event->type() == QEvent::WindowDeactivate ||
                             event->type() == QEvent::ApplicationDeactivate;
    if ( interrupted )
    {
        soundColumnResizeSection_ = -1;
        soundEntries_->setProperty("snd.pressed_mode", QVariant{});
        soundEntries_->viewport()->unsetCursor();
    }
    if ( !interrupted && event->type() != QEvent::MouseButtonPress && event->type() != QEvent::MouseButtonDblClick &&
         event->type() != QEvent::MouseMove && event->type() != QEvent::MouseButtonRelease )
    {
        return false;
    }
    const QModelIndex active = soundEntries_->currentIndex();
    QWidget* const editor = active.isValid() ? soundEntries_->indexWidget(active) : nullptr;
    QSlider* const slider = editor ? editor->findChild<QSlider*>(QStringLiteral("snd.volume.slider")) : nullptr;
    if ( interrupted )
    {
        if ( slider && slider->property("resource_authoring.inline_drag").toBool() )
        {
            slider->setProperty("resource_authoring.inline_drag", false);
            // This is terminal: sliderReleased may destroy or replace the editor.
            slider->setSliderDown(false);
            return true;
        }
        return false;
    }
    auto* const mouse = static_cast<QMouseEvent*>(event);
    const QPoint local = mouse->position().toPoint();
    const QPersistentModelIndex pressedMode =
        soundEntries_->property("snd.pressed_mode").value<QPersistentModelIndex>();
    if ( pressedMode.isValid() && event->type() == QEvent::MouseMove &&
         !mouse->buttons().testFlag(Qt::LeftButton) )
    {
        soundEntries_->setProperty("snd.pressed_mode", QVariant{});
    }
    if ( pressedMode.isValid() && event->type() == QEvent::MouseButtonRelease && mouse->button() == Qt::LeftButton )
    {
        soundEntries_->setProperty("snd.pressed_mode", QVariant{});
        const QModelIndex released = soundEntries_->indexAt(local);
        if ( released.isValid() && released.siblingAtColumn(0) == pressedMode )
        {
            QTreeWidgetItem* const group = soundEntries_->itemFromIndex(pressedMode);
            const QString mode = ui::episode::displayTextForGameBytes(
                pressedMode.data(kSoundTreeModeBytesRole).toByteArray().toStdString());
            const bool expanded = !group->isExpanded();
            {
                const QSignalBlocker blocker(soundEntries_);
                // Qt supports programmatic reversal while its animation is
                // running; only the native mouse handler ignores those clicks.
                group->setExpanded(expanded);
            }
            if ( expanded )
            {
                collapsedSoundModes_.remove(mode);
            }
            else
            {
                collapsedSoundModes_.insert(mode);
            }
            hideSoundHoverControls();
            updateSoundPlayingControls();
            // Publish after the native expansion transaction has returned.
            notifyChanged();
        }
        return true;
    }
    if ( soundColumnResizeSection_ >= 0 )
    {
        if ( event->type() == QEvent::MouseMove )
        {
            if ( !mouse->buttons().testFlag(Qt::LeftButton) )
            {
                soundColumnResizeSection_ = -1;
                soundEntries_->viewport()->unsetCursor();
                return false;
            }
            soundEntries_->setColumnWidth(
                soundColumnResizeSection_,
                std::max(soundEntries_->header()->minimumSectionSize(),
                         soundColumnResizeStartWidth_ + local.x() - soundColumnResizeStartX_));
        }
        else if ( event->type() == QEvent::MouseButtonRelease )
        {
            soundColumnResizeSection_ = -1;
            soundEntries_->viewport()->unsetCursor();
        }
        return true;
    }
    if ( slider && slider->property("resource_authoring.inline_drag").toBool() &&
         (event->type() == QEvent::MouseMove || event->type() == QEvent::MouseButtonRelease) )
    {
        if ( event->type() == QEvent::MouseMove && !mouse->buttons().testFlag(Qt::LeftButton) )
        {
            slider->setProperty("resource_authoring.inline_drag", false);
            slider->setSliderDown(false);
            return true;
        }
        const QPointer<ResourceAuthoringDialog> self(this);
        const QPointer<QWidget> activeEditor(editor);
        const QPointer<QSlider> activeSlider(slider);
        const QPersistentModelIndex target(active);
        const QPoint position = slider->mapFromGlobal(mouse->globalPosition().toPoint());
        const int handle = slider->style()->pixelMetric(QStyle::PM_SliderLength, nullptr, slider);
        slider->setValue(QStyle::sliderValueFromPosition(
            slider->minimum(), slider->maximum(), position.x() - handle / 2, std::max(1, slider->width() - handle)));
        if ( !self || !activeEditor || !activeSlider || !target.isValid() ||
             soundEntries_->indexWidget(target) != activeEditor.data() )
        {
            return true;
        }
        if ( event->type() == QEvent::MouseButtonRelease )
        {
            slider->setProperty("resource_authoring.inline_drag", false);
            // sliderReleased commits once and may synchronously delete this pane.
            slider->setSliderDown(false);
        }
        return true;
    }
    const QModelIndex index = soundEntries_->indexAt(local);
    if ( !index.isValid() )
    {
        soundEntries_->viewport()->unsetCursor();
        return false;
    }
    const auto kind = static_cast<SoundTreeItemKind>(index.siblingAtColumn(0).data(kSoundTreeItemKindRole).toInt());
    if ( kind == SoundTreeItemKind::mode &&
         (event->type() == QEvent::MouseButtonPress || event->type() == QEvent::MouseButtonDblClick) &&
         mouse->button() == Qt::LeftButton )
    {
        const QPersistentModelIndex target(index.siblingAtColumn(0));
        soundEntries_->setProperty("snd.pressed_mode", QVariant::fromValue(target));
        soundEntries_->setCurrentIndex(target);
        return true;
    }
    if ( kind == SoundTreeItemKind::columnHeader )
    {
        int boundary = -1;
        for ( int column = 0; column < soundEntries_->columnCount(); ++column )
        {
            const int right = soundEntries_->header()->sectionViewportPosition(column) +
                              soundEntries_->header()->sectionSize(column) - 1;
            if ( std::abs(local.x() - right) <= 4 )
            {
                boundary = column;
                break;
            }
        }
        if ( boundary >= 0 )
        {
            soundEntries_->viewport()->setCursor(Qt::SizeHorCursor);
            if ( event->type() == QEvent::MouseButtonPress && mouse->button() == Qt::LeftButton )
            {
                soundColumnResizeSection_ = boundary;
                soundColumnResizeStartX_ = local.x();
                soundColumnResizeStartWidth_ = soundEntries_->columnWidth(boundary);
                return true;
            }
        }
        else
        {
            soundEntries_->viewport()->unsetCursor();
        }
        return false;
    }
    soundEntries_->viewport()->unsetCursor();
    if ( kind != SoundTreeItemKind::alternative || index.column() != 5 || event->type() != QEvent::MouseButtonPress ||
         mouse->button() != Qt::LeftButton ||
         !soundVolumeSliderRect(soundVolumeCanvasRect(soundEntries_->visualRect(index), soundEntries_->viewport()),
                                QFontMetrics(soundEntries_->font())).contains(local) )
    {
        return false;
    }
    QPointer<ResourceAuthoringDialog> self(this);
    const QPersistentModelIndex target(index);
    soundEntries_->setCurrentIndex(target);
    if ( !self || !target.isValid() )
    {
        return true;
    }
    soundEntries_->editItem(soundEntries_->itemFromIndex(target), 5);
    if ( !self || !target.isValid() )
    {
        return true;
    }
    QWidget* const volumeEditor = soundEntries_->indexWidget(target);
    QSlider* const volumeSlider =
        volumeEditor ? volumeEditor->findChild<QSlider*>(QStringLiteral("snd.volume.slider")) : nullptr;
    if ( volumeSlider )
    {
        const QPointer<QWidget> guardedEditor(volumeEditor);
        const QPointer<QSlider> guardedSlider(volumeSlider);
        const QPoint position = volumeSlider->mapFromGlobal(mouse->globalPosition().toPoint());
        const int handle = volumeSlider->style()->pixelMetric(QStyle::PM_SliderLength, nullptr, volumeSlider);
        volumeSlider->setProperty("resource_authoring.inline_drag", true);
        volumeSlider->setSliderDown(true);
        if ( !self || !guardedEditor || !guardedSlider || !target.isValid() ||
             soundEntries_->indexWidget(target) != guardedEditor.data() )
        {
            return true;
        }
        volumeSlider->setValue(QStyle::sliderValueFromPosition(0, 1000, position.x() - handle / 2,
                                                               std::max(1, volumeSlider->width() - handle)));
    }
    return true;
}

void ResourceAuthoringDialog::buildStringEditor()
{
    auto* const toolbar = new QHBoxLayout();
    stringProfile_ = context_.projectType == ProjectType::IJIM
        ? assets::UniProfile::ijim
        : assets::UniProfile::classic;
    stringSearch_ = new QLineEdit(contentHost_);
    stringSearch_->setObjectName(QStringLiteral("uni.search"));
    stringSearch_->setPlaceholderText(QStringLiteral("Search keys and text"));
    stringSearch_->setClearButtonEnabled(true);
    stringSearch_->installEventFilter(this);
    stringAddButton_ = makeButton(QStringLiteral("Add"), "uni.add");
    stringRemoveButton_ = makeButton(QStringLiteral("Remove"), "uni.remove");
    toolbar->addWidget(stringSearch_, 1);
    toolbar->addWidget(stringAddButton_);
    toolbar->addWidget(stringRemoveButton_);
    contentLayout_->addLayout(toolbar);

    stringTable_ = new QTableWidget(contentHost_);
    stringTable_->setObjectName(QStringLiteral("uni.entries"));
    stringTable_->setColumnCount(3);
    stringTable_->setHorizontalHeaderLabels({
        QStringLiteral("Key"), QStringLiteral("Text"), QStringLiteral("Legacy number") });
    stringTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    stringTable_->setSelectionMode(QAbstractItemView::SingleSelection);
    stringTable_->setEditTriggers(
        QAbstractItemView::DoubleClicked
        | QAbstractItemView::SelectedClicked
        | QAbstractItemView::EditKeyPressed
        | QAbstractItemView::AnyKeyPressed);
    stringTable_->setSortingEnabled(false);
    stringTable_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    stringTable_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    stringTable_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    stringTable_->setColumnHidden(2, true);
    stringTable_->installEventFilter(this);
    stringTable_->viewport()->installEventFilter(this);
    stringTable_->horizontalScrollBar()->installEventFilter(this);
    stringTable_->verticalScrollBar()->installEventFilter(this);
    const auto cancelStalePresentationReferences = [this]()
    {
        if ( !pendingTableEditorCarets_.isEmpty() || deferredPresentationScroll_.pending )
        {
            cancelDeferredEditorPresentation();
        }
    };
    QAbstractItemModel* const stringModel = stringTable_->model();
    connect(stringModel, &QAbstractItemModel::rowsAboutToBeInserted, this,
        [cancelStalePresentationReferences](const QModelIndex&, int, int)
        {
            cancelStalePresentationReferences();
        });
    connect(stringModel, &QAbstractItemModel::rowsAboutToBeRemoved, this,
        [cancelStalePresentationReferences](const QModelIndex&, int, int)
        {
            cancelStalePresentationReferences();
        });
    connect(stringModel, &QAbstractItemModel::rowsAboutToBeMoved, this,
        [cancelStalePresentationReferences](const QModelIndex&, int, int, const QModelIndex&, int)
        {
            cancelStalePresentationReferences();
        });
    connect(stringModel, &QAbstractItemModel::modelAboutToBeReset, this,
        cancelStalePresentationReferences);
    stringTable_->setItemDelegate(new TrackingItemDelegate(
        [this](QWidget* editor, const QModelIndex& index)
        {
            trackTableEditor(stringTable_, editor, index);
        },
        stringTable_));
    contentLayout_->addWidget(stringTable_, 1);

    connect(stringTable_, &QTableWidget::itemChanged, this, [this](QTableWidgetItem* item)
        {
            updateStringEntryFromCell(item);
        });
    connect(stringSearch_, &QLineEdit::textChanged, this, [this]()
        {
            if ( !refreshing_ )
            {
                ++presentationInteractionGeneration_;
            }
            filterStringRows();
        });
    connect(stringAddButton_, &QPushButton::clicked, this, [this]()
        {
            if ( stringsFile_.entries.size() >= 4096 )
            {
                setStatus(QStringLiteral("The editor supports up to 4096 string-table rows."), true);
                return;
            }
            syncStringModelFromRawRows();
            {
                const QSignalBlocker blocker(stringTable_);
                stringsFile_.entries.push_back(assets::StringTableEntry{});
                const int row = stringTable_->rowCount() - 1;
                stringTable_->insertRow(row + 1);
                for ( int column = 0; column < 3; ++column )
                {
                    stringTable_->setItem(row + 1, column, new QTableWidgetItem(
                        column == 2 ? QStringLiteral("0") : QString{}));
                }
                stringTable_->setCurrentCell(row + 1, 0);
                if ( stringTable_->item(row + 1, 0) )
                {
                    stringTable_->editItem(stringTable_->item(row + 1, 0));
                }
                stringRemoveButton_->setEnabled(true);
            }
            recordStringHistoryInsert(stringTable_->rowCount() - 1);
            markDirty();
        });
    connect(stringRemoveButton_, &QPushButton::clicked, this, [this]()
        {
            const int row = stringTable_->currentRow();
            if ( row < 0 || row >= static_cast<int>(stringsFile_.entries.size()) )
            {
                return;
            }
            syncStringModelFromRawRows();
            recordStringHistoryRemove(row);
            {
                const QSignalBlocker blocker(stringTable_);
                stringsFile_.entries.erase(stringsFile_.entries.begin() + row);
                stringTable_->removeRow(row);
                filterStringRows();
                stringRemoveButton_->setEnabled(stringTable_->currentRow() >= 0);
            }
            markDirty();
        });
    connect(stringTable_, &QTableWidget::currentCellChanged, this,
        [this](const int currentRow, int, int, int)
        {
            stringRemoveButton_->setEnabled(currentRow >= 0);
        });
    stringRemoveButton_->setEnabled(false);
}

QStringList ResourceAuthoringDialog::soundModeSuggestions(const ProjectType projectType)
{
    QStringList modes{};
    for ( const std::string_view mode : assets::soundClassModeNames(soundProfile(projectType)) )
    {
        modes.push_back(QString::fromLatin1(mode.data(), static_cast<qsizetype>(mode.size())));
    }
    return modes;
}

QStringList ResourceAuthoringDialog::soundModeSuggestions() const
{
    return soundModeSuggestions(context_.projectType);
}

void ResourceAuthoringDialog::refreshSoundRows()
{
    refreshSoundModeGroups();
}

void ResourceAuthoringDialog::restoreSoundRawRows(const QCborArray& rows)
{
    const bool wasRefreshing = refreshing_;
    refreshing_ = true;
    const QSignalBlocker blocker(soundEntries_);
    soundRawRows_.clear();
    soundRawRows_.reserve(static_cast<std::size_t>(rows.size()));
    soundPreviewLoopRows_.assign(static_cast<std::size_t>(rows.size()), false);
    for ( const QCborValue& rowValue : rows )
    {
        const QCborArray cells = rowValue.toArray();
        std::array<QString, 5> raw{};
        for ( qsizetype index = 0; index < static_cast<qsizetype>(raw.size()); ++index )
        {
            raw[static_cast<std::size_t>(index)] =
                cells.size() > index + 1 ? cells.at(index + 1).toString() : QString{};
        }
        soundRawRows_.push_back(std::move(raw));
        if ( QTreeWidgetItem* const item = soundAlternativeAt(static_cast<int>(soundRawRows_.size() - 1U)) )
        {
            for ( int column = 1; column <= 5; ++column )
            {
                item->setText(column, soundRawRows_.back()[static_cast<std::size_t>(column - 1)]);
            }
        }
    }
    refreshing_ = wasRefreshing;
}

QTreeWidgetItem* ResourceAuthoringDialog::createSoundModeGroup(const std::string& mode)
{
    const QString modeText = mode.empty() ? QStringLiteral("(empty mode)") : ui::episode::displayTextForGameBytes(mode);
    const auto names = assets::soundClassModeNames(soundProfile(context_.projectType));
    const auto found = std::find(names.begin(), names.end(), mode);
    const int slot = found == names.end() ? 0 : static_cast<int>(std::distance(names.begin(), found)) + 1;
    auto* const group = new QTreeWidgetItem(soundEntries_, {modeText});
    group->setData(0, kSoundTreeItemKindRole, static_cast<int>(SoundTreeItemKind::mode));
    group->setData(0, kSoundTreeModeBytesRole, QByteArray(mode.data(), static_cast<qsizetype>(mode.size())));
    group->setData(0, kSoundTreeModeSlotRole, slot);
    group->setFirstColumnSpanned(true);
    QFont titleFont = soundEntries_->font();
    titleFont.setPointSizeF(std::max(10.0, titleFont.pointSizeF() + 2.0));
    titleFont.setBold(true);
    group->setFont(0, titleFont);
    group->setSizeHint(0, QSize(0, std::max(32, QFontMetrics(titleFont).height() + 12)));
    group->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    group->setToolTip(0, slot > 0 ? QStringLiteral("Sound mode: %1").arg(slot)
                                  : QStringLiteral("This source mode is not recognized by the active engine profile."));
    auto* const headings = new QTreeWidgetItem(group);
    headings->setData(0, kSoundTreeItemKindRole, static_cast<int>(SoundTreeItemKind::columnHeader));
    headings->setData(0, kSoundTreeModeBytesRole, group->data(0, kSoundTreeModeBytesRole));
    headings->setFlags(Qt::ItemIsEnabled);
    headings->setSizeHint(0, QSize(0, 25));
    const std::array<QString, 6> labels{QString{},
                                        QStringLiteral("Sound"),
                                        QStringLiteral("Flags"),
                                        QStringLiteral("Min"),
                                        QStringLiteral("Max"),
                                        QStringLiteral("Volume")};
    for ( int column = 0; column < static_cast<int>(labels.size()); ++column )
    {
        headings->setText(column, labels[static_cast<std::size_t>(column)]);
        headings->setToolTip(column, soundColumnHelp(column));
        if ( column >= 1 )
        {
            const Qt::Alignment alignment = column >= 2 && column <= 4
                ? Qt::AlignHCenter | Qt::AlignVCenter : Qt::AlignLeft | Qt::AlignVCenter;
            headings->setTextAlignment(column, alignment);
        }
    }
    const QString identity = ui::episode::displayTextForGameBytes(mode);
    if ( !knownSoundModes_.contains(identity) )
    {
        knownSoundModes_.insert(identity);
        collapsedSoundModes_.insert(identity);
    }
    return group;
}

void ResourceAuthoringDialog::setAllSoundModesExpanded(const bool expanded)
{
    const bool wasAnimated = soundEntries_->isAnimated();
    const bool updatesEnabled = soundEntries_->updatesEnabled();
    soundEntries_->setUpdatesEnabled(false);
    soundEntries_->setAnimated(false);
    {
        const QSignalBlocker blocker(soundEntries_);
        for ( int index = 0; index < soundEntries_->topLevelItemCount(); ++index )
        {
            QTreeWidgetItem* const group = soundEntries_->topLevelItem(index);
            const QString identity = ui::episode::displayTextForGameBytes(
                group->data(0, kSoundTreeModeBytesRole).toByteArray().toStdString());
            if ( expanded )
            {
                collapsedSoundModes_.remove(identity);
            }
            else
            {
                collapsedSoundModes_.insert(identity);
            }
            group->setExpanded(expanded);
        }
        hideSoundHoverControls();
    }
    soundEntries_->doItemsLayout();
    soundEntries_->setAnimated(wasAnimated);
    soundEntries_->setUpdatesEnabled(updatesEnabled);
    soundEntries_->viewport()->update();
    updateSoundPlayingControls();
    notifyChanged();
}

void ResourceAuthoringDialog::resizeSoundWavColumn(const bool fitColumns, const int changedColumn)
{
    if ( !soundEntries_ || soundEntries_->property("snd.resizing_columns").toBool() )
    {
        return;
    }
    soundEntries_->setProperty("snd.resizing_columns", true);
    const QPointer<ResourceAuthoringDialog> self(this);
    const QPointer<QTreeWidget> tree(soundEntries_);
    const auto finish = [&tree]()
    {
        if ( tree )
        {
            tree->setProperty("snd.resizing_columns", false);
        }
    };
    int minimumColumns = 0;
    for ( int column = 0; column < static_cast<int>(kSoundColumnMinimumWidths.size()); ++column )
    {
        const int minimum = kSoundColumnMinimumWidths[static_cast<std::size_t>(column)];
        minimumColumns += minimum;
        if ( tree->columnWidth(column) < minimum )
        {
            tree->setColumnWidth(column, minimum);
            if ( !self || !tree )
            {
                finish();
                return;
            }
        }
    }
    // Root and content margins are layout chrome. Reserve the style's vertical
    // scrollbar extent unconditionally so the pane minimum stays stable when
    // row count or height makes the scrollbar appear or disappear.
    const QMargins rootMargins = layout() ? layout()->contentsMargins() : QMargins{};
    const QMargins contentMargins = contentLayout_->contentsMargins();
    const int scrollbarWidth = tree->verticalScrollBar()->sizeHint().width();
    if ( !self || !tree )
    {
        finish();
        return;
    }
    const int chrome = rootMargins.left() + rootMargins.right() + contentMargins.left() + contentMargins.right()
                       + 2 * tree->frameWidth() + scrollbarWidth;
    setMinimumWidth(minimumColumns + chrome);
    if ( !self || !tree )
    {
        finish();
        return;
    }
    if ( soundManualColumnWidths_ )
    {
        finish();
        return;
    }
    const int available = tree->viewport()->width();
    int nonVolumeColumns = 0;
    for ( int column = 0; column < 5; ++column )
    {
        nonVolumeColumns += tree->columnWidth(column);
    }
    // Keep metadata compact, give Sound 65% of the surplus, then transfer 40%
    // of the resulting Volume width to Sound until Volume reaches its minimum.
    // Manual section resizes still take the path below unchanged.
    if ( fitColumns )
    {
        const bool useDefaultSmallColumns = available >= minimumColumns + 16;
        for ( const int column : {0, 2, 3, 4} )
        {
            const int minimum = kSoundColumnMinimumWidths[static_cast<std::size_t>(column)];
            const int target = useDefaultSmallColumns
                ? kSoundColumnDefaultWidths[static_cast<std::size_t>(column)]
                : minimum;
            tree->setColumnWidth(column, target);
            if ( !self || !tree )
            {
                finish();
                return;
            }
        }
        int fixedColumns = 0;
        for ( const int column : {0, 2, 3, 4} )
        {
            fixedColumns += tree->columnWidth(column);
        }
        const int remainingForSoundAndVolume = std::max(
            0, available - fixedColumns - kSoundColumnMinimumWidths[1] - kSoundColumnMinimumWidths[5]);
        const int initialSoundWidth = kSoundColumnMinimumWidths[1]
            + static_cast<int>(std::lround(remainingForSoundAndVolume * 0.65));
        const int initialVolumeWidth = std::max(
            kSoundColumnMinimumWidths[5], available - fixedColumns - initialSoundWidth);
        const int volumeTransfer = std::min(
            static_cast<int>(std::lround(initialVolumeWidth * 0.40)),
            initialVolumeWidth - kSoundColumnMinimumWidths[5]);
        tree->setColumnWidth(1, initialSoundWidth + volumeTransfer);
        if ( !self || !tree )
        {
            finish();
            return;
        }
        tree->setColumnWidth(5, initialVolumeWidth - volumeTransfer);
        if ( !self || !tree )
        {
            finish();
            return;
        }
    }
    if ( fitColumns || changedColumn != 5 )
    {
        nonVolumeColumns = 0;
        for ( int column = 0; column < 5; ++column )
        {
            nonVolumeColumns += tree->columnWidth(column);
        }
        tree->setColumnWidth(5, std::max(kSoundColumnMinimumWidths[5], available - nonVolumeColumns));
        if ( !self || !tree )
        {
            finish();
            return;
        }
    }
    finish();
}

QTreeWidgetItem* ResourceAuthoringDialog::createSoundAlternative(QTreeWidgetItem* const group, const int row)
{
    auto* const alternative = new QTreeWidgetItem;
    const std::size_t sourceIndex = static_cast<std::size_t>(row);
    const std::string& mode = soundFile_.entries.at(sourceIndex).mode;
    alternative->setData(0, kSoundTreeItemKindRole, static_cast<int>(SoundTreeItemKind::alternative));
    alternative->setData(0, kSoundTreeSourceIndexRole, row);
    alternative->setData(0, kSoundTreeModeBytesRole, QByteArray(mode.data(), static_cast<qsizetype>(mode.size())));
    alternative->setData(0, kSoundTreePreviewLoopRole, static_cast<bool>(soundPreviewLoopRows_.at(sourceIndex)));
    for ( int column = 0; column <= 5; ++column )
    {
        alternative->setBackground(column, QBrush(QColor(48, 48, 48)));
        if ( column > 0 )
        {
            alternative->setText(column, soundRawRows_.at(sourceIndex)[static_cast<std::size_t>(column - 1)]);
            alternative->setToolTip(column, soundColumnHelp(column));
        }
        if ( column >= 1 )
        {
            const Qt::Alignment alignment = column >= 2 && column <= 4
                ? Qt::AlignHCenter | Qt::AlignVCenter : Qt::AlignLeft | Qt::AlignVCenter;
            alternative->setTextAlignment(column, alignment);
        }
    }
    alternative->setSizeHint(0, QSize(0, 26));
    alternative->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsEditable);
    int childIndex = group->childCount();
    if ( childIndex > 0 && group->child(childIndex - 1)->data(0, kSoundTreeItemKindRole).toInt() ==
                               static_cast<int>(SoundTreeItemKind::addAlternative) )
    {
        --childIndex;
    }
    group->insertChild(childIndex, alternative);
    soundAlternativeItems_.at(sourceIndex) = alternative;
    return alternative;
}

void ResourceAuthoringDialog::appendSoundEntryFooter(QTreeWidgetItem* const group)
{
    auto* const footer = new QTreeWidgetItem(group);
    footer->setText(0, QStringLiteral("＋ Add Entry"));
    footer->setTextAlignment(0, Qt::AlignHCenter | Qt::AlignVCenter);
    footer->setBackground(0, QBrush(QColor(48, 48, 48)));
    footer->setFirstColumnSpanned(true);
    footer->setData(0, kSoundTreeItemKindRole, static_cast<int>(SoundTreeItemKind::addAlternative));
    footer->setData(0, kSoundTreeModeBytesRole, group->data(0, kSoundTreeModeBytesRole));
    footer->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    footer->setDisabled(soundFile_.entries.size() >= 4096U);
}

void ResourceAuthoringDialog::refreshSoundModeGroups()
{
    if ( !soundEntries_ )
    {
        return;
    }
    const bool wasRefreshing = refreshing_;
    refreshing_ = true;
    const QSignalBlocker blocker(soundEntries_);
    if ( soundRawRows_.size() != soundFile_.entries.size() )
    {
        soundRawRows_.clear();
        soundRawRows_.reserve(soundFile_.entries.size());
        for ( const assets::SoundClassEntry& entry : soundFile_.entries )
        {
            soundRawRows_.push_back({
                ui::episode::displayTextForGameBytes(entry.wav),
                flagsText(entry.playFlags),
                floatText(entry.minRadius),
                floatText(entry.maxRadius),
                floatText(entry.maxVolume),
            });
        }
    }
    if ( soundPreviewLoopRows_.size() != soundFile_.entries.size() )
    {
        soundPreviewLoopRows_.resize(soundFile_.entries.size(), false);
    }
    hideSoundHoverControls();
    soundAlternativeItems_.clear();
    soundAlternativeItems_.resize(soundFile_.entries.size(), nullptr);
    soundEntries_->clear();
    std::unordered_map<std::string, QTreeWidgetItem*> groups{};
    const std::span<const std::string_view> modeNames = assets::soundClassModeNames(soundProfile(context_.projectType));
    for ( std::size_t row = 0; row < soundFile_.entries.size(); ++row )
    {
        const assets::SoundClassEntry& entry = soundFile_.entries[row];
        const std::string& mode = entry.mode;
        QTreeWidgetItem* group = nullptr;
        if ( const auto found = groups.find(mode); found != groups.end() )
        {
            group = found->second;
        }
        else
        {
            group = createSoundModeGroup(mode);
            groups.emplace(mode, group);
        }
        createSoundAlternative(group, static_cast<int>(row));
    }
    for ( const QString& identity : soundPresentationModes_ )
    {
        const std::string mode = identity.toStdString();
        if ( !groups.contains(mode) )
        {
            groups.emplace(mode, createSoundModeGroup(mode));
        }
    }
    std::vector<QTreeWidgetItem*> orderedGroups;
    while ( soundEntries_->topLevelItemCount() > 0 )
    {
        orderedGroups.push_back(soundEntries_->takeTopLevelItem(0));
    }
    std::stable_sort(orderedGroups.begin(), orderedGroups.end(),
                     [](const QTreeWidgetItem* left, const QTreeWidgetItem* right)
                     {
                         const int leftSlot = left->data(0, kSoundTreeModeSlotRole).toInt();
                         const int rightSlot = right->data(0, kSoundTreeModeSlotRole).toInt();
                         return (leftSlot > 0 ? leftSlot : std::numeric_limits<int>::max()) <
                                (rightSlot > 0 ? rightSlot : std::numeric_limits<int>::max());
                     });
    for ( QTreeWidgetItem* const group : orderedGroups )
    {
        soundEntries_->addTopLevelItem(group);
        group->setFirstColumnSpanned(true);
    }
    for ( int index = 0; index < soundEntries_->topLevelItemCount(); ++index )
    {
        QTreeWidgetItem* group = soundEntries_->topLevelItem(index);
        if ( !group || group->data(0, kSoundTreeItemKindRole).toInt() != static_cast<int>(SoundTreeItemKind::mode) )
        {
            continue;
        }
        appendSoundEntryFooter(group);
        const QString modeIdentity =
            ui::episode::displayTextForGameBytes(group->data(0, kSoundTreeModeBytesRole).toByteArray().toStdString());
        const QString query = soundSearch_ ? soundSearch_->text().trimmed() : QString{};
        const bool matches = query.isEmpty() || group->text(0).contains(query, Qt::CaseInsensitive);
        group->setHidden(!matches);
        group->setExpanded(query.isEmpty() ? !collapsedSoundModes_.contains(modeIdentity) : matches);
    }

    soundAddModeButton_->setEnabled(soundFile_.entries.size() < 4096U &&
                                    std::any_of(modeNames.begin(), modeNames.end(),
                                                [&groups](const std::string_view mode)
                                                {
                                                    return !groups.contains(std::string(mode));
                                                }));
    refreshing_ = wasRefreshing;
}

std::optional<std::string> ResourceAuthoringDialog::selectedSoundMode() const
{
    if ( !soundEntries_ )
    {
        return std::nullopt;
    }
    QTreeWidgetItem* item = soundEntries_->currentItem();
    if ( !item )
    {
        return std::nullopt;
    }
    if ( item->parent() )
    {
        item = item->parent();
    }
    const QByteArray mode = item->data(0, kSoundTreeModeBytesRole).toByteArray();
    return std::string(mode.constData(), static_cast<std::size_t>(mode.size()));
}

void ResourceAuthoringDialog::appendSoundEntry(const std::string& mode)
{
    if ( !soundEntries_ || soundFile_.entries.size() >= 4096U )
    {
        setStatus(QStringLiteral("The editor supports up to 4096 sound-class rows."), true);
        return;
    }
    QPointer<ResourceAuthoringDialog> self(this);
    settleActiveTableEditors();
    if ( !self )
    {
        return;
    }
    syncSoundModelFromRawRows();
    assets::SoundClassEntry entry{};
    entry.mode = mode;
    entry.wav = "new_sound.wav";
    entry.playFlags = 0x00000040U;
    entry.minRadius = 0.5f;
    entry.maxRadius = 2.5f;
    entry.maxVolume = 1.0f;
    soundFile_.entries.push_back(std::move(entry));
    soundRawRows_.push_back({QStringLiteral("new_sound.wav"), QStringLiteral("0x00000040"), QStringLiteral("0.5"),
                             QStringLiteral("2.5"), QStringLiteral("1")});
    soundPreviewLoopRows_.push_back(false);
    const int row = static_cast<int>(soundFile_.entries.size() - 1U);
    recordSoundHistoryEdit(SoundHistoryEdit{row, std::nullopt, soundRowRecord(row)});
    const QString modeIdentity = ui::episode::displayTextForGameBytes(mode);
    knownSoundModes_.insert(modeIdentity);
    collapsedSoundModes_.remove(modeIdentity);
    const bool wasRefreshing = refreshing_;
    const bool updatesEnabled = soundEntries_->updatesEnabled();
    refreshing_ = true;
    soundEntries_->setUpdatesEnabled(false);
    {
        const QSignalBlocker blocker(soundEntries_);
        QTreeWidgetItem* group = nullptr;
        const QByteArray modeBytes(mode.data(), static_cast<qsizetype>(mode.size()));
        for ( int index = 0; index < soundEntries_->topLevelItemCount(); ++index )
        {
            QTreeWidgetItem* const candidate = soundEntries_->topLevelItem(index);
            if ( candidate->data(0, kSoundTreeModeBytesRole).toByteArray() == modeBytes )
            {
                group = candidate;
                break;
            }
        }
        if ( !group )
        {
            group = createSoundModeGroup(mode);
            appendSoundEntryFooter(group);
            const int slot = group->data(0, kSoundTreeModeSlotRole).toInt();
            const int last = soundEntries_->topLevelItemCount() - 1;
            int position = last;
            if ( slot > 0 )
            {
                for ( int index = 0; index < last; ++index )
                {
                    const int otherSlot = soundEntries_->topLevelItem(index)->data(0, kSoundTreeModeSlotRole).toInt();
                    if ( otherSlot <= 0 || otherSlot > slot )
                    {
                        position = index;
                        break;
                    }
                }
            }
            if ( position != last )
            {
                soundEntries_->insertTopLevelItem(position, soundEntries_->takeTopLevelItem(last));
            }
            group->setFirstColumnSpanned(true);
        }
        soundAlternativeItems_.push_back(nullptr);
        QTreeWidgetItem* const alternative = createSoundAlternative(group, row);
        group->setExpanded(true);
        soundEntries_->doItemsLayout();
        soundEntries_->setCurrentItem(alternative, 1);
        soundEntries_->scrollToItem(alternative, QAbstractItemView::EnsureVisible);
        if ( soundFile_.entries.size() >= 4096U )
        {
            soundAddModeButton_->setEnabled(false);
            for ( int index = 0; index < soundEntries_->topLevelItemCount(); ++index )
            {
                QTreeWidgetItem* const modeGroup = soundEntries_->topLevelItem(index);
                modeGroup->child(modeGroup->childCount() - 1)->setDisabled(true);
            }
        }
    }
    refreshing_ = wasRefreshing;
    soundEntries_->setUpdatesEnabled(updatesEnabled);
    soundEntries_->viewport()->update();
    publishSoundHistoryChange();
}

void ResourceAuthoringDialog::syncSoundModelFromRawRows()
{
    if ( !soundEntries_ )
    {
        return;
    }
    syncSoundRawRowsFromTree();
    for ( std::size_t row = 0; row < soundFile_.entries.size(); ++row )
    {
        QTreeWidgetItem* const item = soundAlternativeAt(static_cast<int>(row));
        if ( !item )
        {
            continue;
        }
        assets::SoundClassEntry& entry = soundFile_.entries[row];
        const QByteArray mode = item->data(0, kSoundTreeModeBytesRole).toByteArray();
        entry.mode.assign(mode.constData(), static_cast<std::size_t>(mode.size()));
        if ( const auto wav = ui::episode::gameBytesForDisplayText(soundRawRows_[row][0]) )
        {
            entry.wav = *wav;
        }
        if ( const auto value = parseHexFlags(soundRawRows_[row][1]) )
        {
            entry.playFlags = *value;
        }
        if ( const auto value = parseFiniteFloat(soundRawRows_[row][2]) )
        {
            entry.minRadius = *value;
        }
        if ( const auto value = parseFiniteFloat(soundRawRows_[row][3]) )
        {
            entry.maxRadius = *value;
        }
        if ( const auto value = parseFiniteFloat(soundRawRows_[row][4]) )
        {
            entry.maxVolume = *value;
        }
    }
}

void ResourceAuthoringDialog::syncSoundRawRowsFromTree()
{
    if ( !soundEntries_ )
    {
        return;
    }
    if ( soundRawRows_.size() != soundFile_.entries.size() )
    {
        soundRawRows_.resize(soundFile_.entries.size());
    }
    if ( soundPreviewLoopRows_.size() != soundFile_.entries.size() )
    {
        soundPreviewLoopRows_.resize(soundFile_.entries.size(), false);
    }
    for ( std::size_t row = 0; row < soundFile_.entries.size(); ++row )
    {
        QTreeWidgetItem* const item = soundAlternativeAt(static_cast<int>(row));
        if ( !item )
        {
            continue;
        }
        for ( int column = 1; column <= 5; ++column )
        {
            const QModelIndex index = soundEntries_->indexFromItem(item, column);
            soundRawRows_[row][static_cast<std::size_t>(column - 1)] =
                cellTextWithPendingEditor(soundEntries_, index, item->text(column));
        }
    }
}

void ResourceAuthoringDialog::syncStringModelFromRawRows()
{
    if ( !stringTable_ )
    {
        return;
    }
    stringsFile_.entries.resize(static_cast<std::size_t>(stringTable_->rowCount()));
    for ( int row = 0; row < stringTable_->rowCount(); ++row )
    {
        assets::StringTableEntry &entry = stringsFile_.entries[static_cast<std::size_t>(row)];
        const auto text = [this, row](const int column)
        {
            const QTableWidgetItem *item = stringTable_->item(row, column);
            return item ? item->text() : QString{};
        };
        if ( const auto value = ui::episode::gameBytesForDisplayText(text(0)) )
        {
            entry.key = *value;
        }
        if ( const auto value = ui::episode::gameBytesForDisplayText(text(1)) )
        {
            entry.value = *value;
        }
        bool ok = false;
        const qlonglong numeric = text(2).trimmed().toLongLong(&ok, 10);
        if ( ok && numeric >= std::numeric_limits<std::int32_t>::min() &&
             numeric <= std::numeric_limits<std::int32_t>::max() )
        {
            entry.numericValue = static_cast<std::int32_t>(numeric);
        }
    }
}

void ResourceAuthoringDialog::refreshStringRows()
{
    refreshing_ = true;
    const QSignalBlocker blocker(stringTable_);
    stringTable_->setRowCount(static_cast<int>(stringsFile_.entries.size()));
    for ( std::size_t index = 0; index < stringsFile_.entries.size(); ++index )
    {
        const assets::StringTableEntry &entry = stringsFile_.entries[index];
        const int row = static_cast<int>(index);
        const std::array<QString, 3> values{
            ui::episode::displayTextForGameBytes(entry.key),
            ui::episode::displayTextForGameBytes(entry.value),
            QString::number(entry.numericValue),
        };
        for ( int column = 0; column < static_cast<int>(values.size()); ++column )
        {
            auto *const item = new QTableWidgetItem(values[static_cast<std::size_t>(column)]);
            if ( column == 2 )
            {
                item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
            }
            stringTable_->setItem(row, column, item);
        }
    }
    refreshing_ = false;
    filterStringRows();
    stringRemoveButton_->setEnabled(stringTable_->currentRow() >= 0);
}

void ResourceAuthoringDialog::restoreRawRows(QTableWidget *table, const QCborArray &rows)
{
    if ( !table || table->rowCount() != rows.size() )
    {
        return;
    }
    const QSignalBlocker blocker(table);
    for ( int row = 0; row < rows.size(); ++row )
    {
        const QCborArray cells = rows.at(row).toArray();
        for ( int column = 0; column < cells.size(); ++column )
        {
            if ( QTableWidgetItem *item = table->item(row, column) )
            {
                item->setText(cells.at(column).toString());
            }
        }
    }
}

void ResourceAuthoringDialog::updateSoundEntryFromCell(QTreeWidgetItem* const item, const int column)
{
    const int row = sourceIndexForSoundItem(item);
    if ( refreshing_ || row < 0 || row >= static_cast<int>(soundFile_.entries.size()) || column < 1 || column > 5 )
    {
        return;
    }
    const SoundRowRecord before = row < static_cast<int>(soundHistoryCurrent_.size())
        ? soundHistoryCurrent_[static_cast<std::size_t>(row)] : soundRowRecord(row);
    QString& raw = soundRawRows_[static_cast<std::size_t>(row)][static_cast<std::size_t>(column - 1)];
    raw = cellTextWithPendingEditor(soundEntries_, soundEntries_->indexFromItem(item, column), item->text(column));
    if ( raw == before.raw[static_cast<std::size_t>(column - 1)] )
    {
        return;
    }
    QString validationError{};
    assets::SoundClassEntry& entry = soundFile_.entries[static_cast<std::size_t>(row)];
    if ( column == 1 )
    {
        if ( soundPlayingRow_ == row )
        {
            stopSoundPreview();
        }
        if ( const auto wav = ui::episode::gameBytesForDisplayText(raw) )
        {
            entry.wav = *wav;
        }
        else
        {
            validationError = QStringLiteral("WAV names must use characters in the game's one-byte encoding.");
        }
    }
    else if ( column == 2 )
    {
        if ( const auto flags = parseHexFlags(raw) )
        {
            entry.playFlags = *flags;
        }
        else
        {
            validationError = QStringLiteral("Play flags must be a hexadecimal integer.");
        }
    }
    else if ( const auto value = parseFiniteFloat(raw) )
    {
        if ( column == 3 )
        {
            entry.minRadius = *value;
        }
        else if ( column == 4 )
        {
            entry.maxRadius = *value;
        }
        else
        {
            entry.maxVolume = *value;
            updateSoundPreviewVolume(row, raw);
        }
    }
    else
    {
        validationError = QStringLiteral("Sound radii and volume must be finite numbers.");
    }
    recordSoundHistoryEdit(SoundHistoryEdit{row, before, soundRowRecord(row)});
    publishSoundHistoryChange(validationError);
}

ResourceAuthoringDialog::SoundRowRecord ResourceAuthoringDialog::soundRowRecord(const int row) const
{
    const std::size_t index = static_cast<std::size_t>(row);
    return SoundRowRecord{soundFile_.entries.at(index), soundRawRows_.at(index),
                          index < soundPreviewLoopRows_.size() && soundPreviewLoopRows_[index]};
}

void ResourceAuthoringDialog::recordSoundHistoryEdit(SoundHistoryEdit edit)
{
    const auto encodedSize = [this](const SoundHistoryEdit& value)
    {
        QCborMap command{{QStringLiteral("row"), value.row},
            {QStringLiteral("before"), value.before ? QCborValue(soundHistoryRow(*value.before)) : QCborValue(nullptr)},
            {QStringLiteral("after"), value.after ? QCborValue(soundHistoryRow(*value.after)) : QCborValue(nullptr)}};
        if ( !value.presentationMode.isEmpty() )
        {
            command.insert(QStringLiteral("mode"), value.presentationMode);
        }
        return QCborValue(command).toCbor().size();
    };
    if ( soundHistoryCleanCursor_ > soundHistoryCursor_ )
    {
        soundHistoryCleanCursor_ = -1;
        if ( soundHistoryCleanAuthoredCursor_ > soundHistoryAuthoredCursor_ )
        {
            soundHistoryCleanAuthoredCursor_ = -1;
        }
    }
    while ( soundHistory_.size() > static_cast<std::size_t>(soundHistoryCursor_) )
    {
        soundHistoryBytes_ -= encodedSize(soundHistory_.back());
        soundHistory_.pop_back();
    }
    if ( edit.presentationMode.isEmpty() )
    {
        if ( edit.before && edit.after )
        {
            if ( edit.row < static_cast<int>(soundHistoryCurrent_.size()) )
            {
                edit.before = soundHistoryCurrent_[static_cast<std::size_t>(edit.row)];
                soundHistoryCurrent_[static_cast<std::size_t>(edit.row)] = *edit.after;
            }
        }
        else if ( edit.after )
        {
            soundHistoryCurrent_.insert(soundHistoryCurrent_.begin() + edit.row, *edit.after);
        }
        else
        {
            soundHistoryCurrent_.erase(soundHistoryCurrent_.begin() + edit.row);
        }
    }
    soundHistoryBytes_ += encodedSize(edit);
    soundHistory_.push_back(std::move(edit));
    ++soundHistoryCursor_;
    if ( soundHistory_.back().presentationMode.isEmpty() )
    {
        ++soundHistoryAuthoredCursor_;
    }
    while ( soundHistoryCursor_ > 1024 || soundHistoryBytes_ > 8 * 1024 * 1024 )
    {
        const bool authored = soundHistory_.front().presentationMode.isEmpty();
        soundHistoryBytes_ -= encodedSize(soundHistory_.front());
        soundHistory_.erase(soundHistory_.begin());
        --soundHistoryCursor_;
        soundHistoryCleanCursor_ = std::max(-1, soundHistoryCleanCursor_ - 1);
        if ( authored )
        {
            --soundHistoryAuthoredCursor_;
            soundHistoryCleanAuthoredCursor_ = std::max(-1, soundHistoryCleanAuthoredCursor_ - 1);
        }
    }
    updateSoundHistoryActions();
}

void ResourceAuthoringDialog::publishSoundHistoryChange(const QString& validationError)
{
    dirty_ = soundHistoryBaselineDirty_ || soundHistoryAuthoredCursor_ != soundHistoryCleanAuthoredCursor_;
    ++authoredRevision_;
    if ( validationError.isEmpty() )
    {
        setStatus(dirty_ ? QStringLiteral("Unsaved changes.") : QStringLiteral("Changes reverted."));
    }
    else
    {
        setStatus(validationError, true);
    }
    updateDocumentLabel();
    updateSoundHistoryActions();
    notifyChanged();
}

void ResourceAuthoringDialog::resetSoundHistory()
{
    soundHistory_.clear();
    soundHistoryBytes_ = 0;
    soundHistoryCursor_ = 0;
    soundHistoryCleanCursor_ = 0;
    soundHistoryAuthoredCursor_ = 0;
    soundHistoryCleanAuthoredCursor_ = 0;
    soundHistoryBaselineDirty_ = dirty_;
    updateSoundHistoryActions();
}

void ResourceAuthoringDialog::updateSoundHistoryActions()
{
    if ( soundUndoAction_ )
    {
        soundUndoAction_->setEnabled(soundHistoryCursor_ > 0);
    }
    if ( soundRedoAction_ )
    {
        soundRedoAction_->setEnabled(soundHistoryCursor_ < static_cast<int>(soundHistory_.size()));
    }
}

void ResourceAuthoringDialog::removeSoundEntry(const int row)
{
    if ( row < 0 || row >= static_cast<int>(soundFile_.entries.size()) )
    {
        return;
    }
    syncSoundModelFromRawRows();
    const SoundRowRecord before = soundRowRecord(row);
    soundFile_.entries.erase(soundFile_.entries.begin() + row);
    soundRawRows_.erase(soundRawRows_.begin() + row);
    soundPreviewLoopRows_.erase(soundPreviewLoopRows_.begin() + row);
    recordSoundHistoryEdit(SoundHistoryEdit{row, before, std::nullopt});
    stopSoundPreview();
    refreshSoundRows();
    publishSoundHistoryChange();
}

void ResourceAuthoringDialog::applySoundHistoryEdit(const SoundHistoryEdit& edit, const bool redo)
{
    if ( !edit.presentationMode.isEmpty() )
    {
        if ( redo )
        {
            soundPresentationModes_.insert(edit.presentationMode);
            collapsedSoundModes_.remove(edit.presentationMode);
        }
        else
        {
            soundPresentationModes_.remove(edit.presentationMode);
        }
        refreshSoundModeGroups();
        return;
    }
    const auto& old = redo ? edit.before : edit.after;
    const auto& next = redo ? edit.after : edit.before;
    const std::size_t row = static_cast<std::size_t>(edit.row);
    if ( old && next )
    {
        if ( soundPlayingRow_ == edit.row && old->entry.wav != next->entry.wav )
        {
            stopSoundPreview();
        }
        soundFile_.entries[row] = next->entry;
        soundRawRows_[row] = next->raw;
        // Preview looping belongs to presentation, so a cell undo keeps it.
        {
            const QSignalBlocker blocker(soundEntries_);
            if ( QTreeWidgetItem* const item = soundAlternativeAt(edit.row) )
            {
                for ( int column = 1; column <= 5; ++column )
                {
                    item->setText(column, next->raw[static_cast<std::size_t>(column - 1)]);
                }
            }
        }
        if ( old->raw[4] != next->raw[4] )
        {
            updateSoundPreviewVolume(edit.row, next->raw[4]);
        }
    }
    else
    {
        if ( next )
        {
            soundFile_.entries.insert(soundFile_.entries.begin() + edit.row, next->entry);
            soundRawRows_.insert(soundRawRows_.begin() + edit.row, next->raw);
            soundPreviewLoopRows_.insert(soundPreviewLoopRows_.begin() + edit.row, next->previewLoop);
        }
        else
        {
            soundFile_.entries.erase(soundFile_.entries.begin() + edit.row);
            soundRawRows_.erase(soundRawRows_.begin() + edit.row);
            soundPreviewLoopRows_.erase(soundPreviewLoopRows_.begin() + edit.row);
        }
        stopSoundPreview();
        refreshSoundRows();
    }
}

bool ResourceAuthoringDialog::undoRedoSoundEdit(const bool redo)
{
    return kind_ == ResourceAuthoringKind::soundClass && undoRedoDocument(redo, true);
}

bool ResourceAuthoringDialog::commitSoundTableDraftToModel()
{
    if ( !soundEntries_ )
    {
        setStatus(QStringLiteral("The sound-class tree is unavailable."), true);
        markDirty(true);
        return false;
    }

    assets::SoundClassFile candidate{};
    candidate.entries.reserve(soundFile_.entries.size());
    for ( std::size_t index = 0; index < soundFile_.entries.size(); ++index )
    {
        const QTreeWidgetItem* const item = soundAlternativeAt(static_cast<int>(index));
        if ( !item )
        {
            setStatus(QStringLiteral("Sound-class rows are out of sync with the document."), true);
            markDirty(true);
            return false;
        }
        const QByteArray mode = item->data(0, kSoundTreeModeBytesRole).toByteArray();
        const std::optional<std::string> wav = ui::episode::gameBytesForDisplayText(item->text(1));
        const std::optional<std::uint32_t> flags = parseHexFlags(item->text(2));
        const std::optional<float> minRadius = parseFiniteFloat(item->text(3));
        const std::optional<float> maxRadius = parseFiniteFloat(item->text(4));
        const std::optional<float> maxVolume = parseFiniteFloat(item->text(5));
        if ( mode.isEmpty() || !wav || !flags || !minRadius || !maxRadius || !maxVolume )
        {
            setStatus(QStringLiteral("Sound-class row %1 contains text outside the game's single-byte encoding, an "
                                     "invalid flag, or a non-finite number.")
                          .arg(index + 1U),
                      true);
            markDirty(true);
            return false;
        }
        assets::SoundClassEntry entry = soundFile_.entries[index];
        entry.mode.assign(mode.constData(), static_cast<std::size_t>(mode.size()));
        entry.wav = *wav;
        entry.playFlags = *flags;
        entry.minRadius = *minRadius;
        entry.maxRadius = *maxRadius;
        entry.maxVolume = *maxVolume;
        candidate.entries.push_back(std::move(entry));
    }

    soundFile_ = std::move(candidate);
    return true;
}

QTreeWidgetItem* ResourceAuthoringDialog::soundAlternativeAt(const int sourceIndex) const
{
    return sourceIndex >= 0 && sourceIndex < static_cast<int>(soundAlternativeItems_.size())
               ? soundAlternativeItems_[static_cast<std::size_t>(sourceIndex)]
               : nullptr;
}

int ResourceAuthoringDialog::sourceIndexForSoundItem(const QTreeWidgetItem* const item) const
{
    if ( !item || item->data(0, kSoundTreeItemKindRole).toInt() != static_cast<int>(SoundTreeItemKind::alternative) )
    {
        return -1;
    }
    return item->data(0, kSoundTreeSourceIndexRole).toInt();
}

void ResourceAuthoringDialog::updateSoundTreeItem(QTreeWidgetItem* const item, const int column)
{
    updateSoundEntryFromCell(item, column);
}

void ResourceAuthoringDialog::openSoundClassFlagsPopup(QTreeWidgetItem* const item)
{
    const int sourceIndex = sourceIndexForSoundItem(item);
    if ( sourceIndex < 0 || sourceIndex >= static_cast<int>(soundFile_.entries.size()) )
    {
        return;
    }
    const std::optional<std::uint32_t> parsedFlags = parseHexFlags(item->text(2));
    const std::uint32_t originalFlags =
        parsedFlags.value_or(soundFile_.entries[static_cast<std::size_t>(sourceIndex)].playFlags);
    static constexpr std::array<std::pair<std::uint32_t, const char*>, 12> kIjimFlags{{
        {0x001U, "Loop"},
        {0x002U, "Remove after fadeout"},
        {0x004U, "Ambient"},
        {0x008U, "Doppler FX"},
        {0x010U, "Fade in"},
        {0x020U, "Fade out"},
        {0x040U, "Absolute position"},
        {0x080U, "Thing position"},
        {0x100U, "High priority"},
        {0x200U, "Highest priority"},
        {0x400U, "Play once"},
        {0x800U, "Play thing once"},
    }};
    static constexpr std::array<std::pair<std::uint32_t, const char*>, 13> kClassicFlags{{
        {0x001U, "Loop"},
        {0x002U, "Remove after fadeout"},
        {0x004U, "Ambient (no panning)"},
        {0x008U, "Doppler FX"},
        {0x010U, "Fade in"},
        {0x020U, "Fade out"},
        {0x040U, "Absolute position"},
        {0x080U, "Follow Thing"},
        {0x100U, "High priority"},
        {0x200U, "Highest priority"},
        {0x400U, "No duplicate playback"},
        {0x800U, "No duplicates on Thing"},
        {0x4000U, "Precache"},
    }};
    const std::span<const std::pair<std::uint32_t, const char*>> knownFlags =
        context_.projectType == ProjectType::IJIM
            ? std::span<const std::pair<std::uint32_t, const char*>>(kIjimFlags)
            : std::span<const std::pair<std::uint32_t, const char*>>(kClassicFlags);
    QPointer<ResourceAuthoringDialog> self(this);
    const std::uint64_t revision = authoredRevision_;
    const QCborMap snapshot = authoredContentState();
    QDialog popup(nullptr);
    connect(this, &QObject::destroyed, &popup, &QDialog::reject);
    popup.setWindowFlags(Qt::Popup | Qt::FramelessWindowHint);
    popup.setObjectName(QStringLiteral("snd.flags.popup"));
    auto* const layout = new QVBoxLayout(&popup);
    layout->setContentsMargins(10, 8, 10, 8);
    std::vector<QCheckBox*> checks(knownFlags.size());
    for ( std::size_t index = 0; index < knownFlags.size(); ++index )
    {
        checks[index] = new QCheckBox(QString::fromLatin1(knownFlags[index].second), &popup);
        checks[index]->setChecked((originalFlags & knownFlags[index].first) != 0U);
        layout->addWidget(checks[index]);
    }
    auto* const buttons =
        new QDialogButtonBox(QDialogButtonBox::Apply | QDialogButtonBox::Cancel, Qt::Horizontal, &popup);
    layout->addWidget(buttons);
    connect(buttons->button(QDialogButtonBox::Apply), &QPushButton::clicked, &popup, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &popup, &QDialog::reject);
    const QPoint requestedAnchor = QCursor::pos();
    popup.adjustSize();
    QScreen* screen = QGuiApplication::screenAt(requestedAnchor);
    if ( !screen )
    {
        screen = QGuiApplication::primaryScreen();
    }
    const QRect available = screen ? screen->availableGeometry() : QRect{};
    const QSize popupSize = popup.size();
    if ( available.isValid() )
    {
        const QPoint clamped(std::clamp(requestedAnchor.x(), available.left(),
                                        std::max(available.left(), available.right() - popupSize.width() + 1)),
                             std::clamp(requestedAnchor.y(), available.top(),
                                        std::max(available.top(), available.bottom() - popupSize.height() + 1)));
        popup.move(clamped);
    }
    if ( popup.exec() != QDialog::Accepted || !self || !self->authoringSnapshotStillCurrent(revision, snapshot) )
    {
        return;
    }
    std::uint32_t updated = originalFlags;
    for ( std::size_t index = 0; index < checks.size(); ++index )
    {
        if ( checks[index]->isChecked() )
        {
            updated |= knownFlags[index].first;
        }
        else
        {
            updated &= ~knownFlags[index].first;
        }
    }
    if ( updated != originalFlags )
    {
        if ( QTreeWidgetItem* const target = self->soundAlternativeAt(sourceIndex) )
        {
            target->setText(2, flagsText(updated));
        }
    }
}

void ResourceAuthoringDialog::updateStringEntryFromCell(QTableWidgetItem* item)
{
    if ( refreshing_ || !item )
    {
        return;
    }
    const int row = item->row();
    const int column = item->column();
    if ( row < 0 || row >= static_cast<int>(stringsFile_.entries.size()) )
    {
        return;
    }
    assets::StringTableEntry& entry = stringsFile_.entries[static_cast<std::size_t>(row)];
    if ( column == 0 || column == 1 )
    {
        const std::optional<std::string> gameBytes =
            ui::episode::gameBytesForDisplayText(item->text());
        if ( !gameBytes )
        {
            setStatus(
                QStringLiteral("Text must use characters in the game's one-byte encoding."),
                true);
            recordStringHistoryRow(row);
            markDirty(true);
            return;
        }
        if ( column == 0 )
        {
            entry.key = *gameBytes;
        }
        else
        {
            entry.value = *gameBytes;
        }
    }
    else if ( column == 2 )
    {
        bool ok = false;
        const qlonglong value = item->text().trimmed().toLongLong(&ok, 10);
        if ( !ok || value < std::numeric_limits<std::int32_t>::min()
            || value > std::numeric_limits<std::int32_t>::max() )
        {
            setStatus(QStringLiteral("The legacy number must be a signed decimal integer."), true);
            recordStringHistoryRow(row);
            markDirty(true);
            return;
        }
        entry.numericValue = static_cast<std::int32_t>(value);
    }
    filterStringRows();
    recordStringHistoryRow(row);
    markDirty();
}

bool ResourceAuthoringDialog::commitStringTableDraftToModel()
{
    if ( !stringTable_ || stringTable_->rowCount() != static_cast<int>(stringsFile_.entries.size()) )
    {
        setStatus(QStringLiteral("String-table rows are out of sync with the document."), true);
        markDirty(true);
        return false;
    }

    assets::StringTableFile candidate = stringsFile_;
    for ( std::size_t index = 0; index < candidate.entries.size(); ++index )
    {
        const int row = static_cast<int>(index);
        const QTableWidgetItem* const keyItem = stringTable_->item(row, 0);
        const QTableWidgetItem* const valueItem = stringTable_->item(row, 1);
        const QTableWidgetItem* const numberItem = stringTable_->item(row, 2);
        bool numberOk = false;
        const qlonglong parsedNumber = numberItem
            ? numberItem->text().trimmed().toLongLong(&numberOk, 10)
            : 0;
        const std::optional<std::string> key = keyItem
            ? ui::episode::gameBytesForDisplayText(keyItem->text())
            : std::nullopt;
        const std::optional<std::string> value = valueItem
            ? ui::episode::gameBytesForDisplayText(valueItem->text())
            : std::nullopt;
        if ( !key || !value || !numberOk
            || parsedNumber < std::numeric_limits<std::int32_t>::min()
            || parsedNumber > std::numeric_limits<std::int32_t>::max() )
        {
            setStatus(
                QStringLiteral("String-table row %1 must use game-byte text and a signed decimal legacy number.")
                    .arg(row + 1),
                true);
            markDirty(true);
            return false;
        }
        candidate.entries[index] = assets::StringTableEntry{
            *key, static_cast<std::int32_t>(parsedNumber), *value };
    }

    stringsFile_ = std::move(candidate);
    return true;
}

void ResourceAuthoringDialog::filterStringRows()
{
    if ( !stringTable_ || !stringSearch_ )
    {
        return;
    }
    const QString query = stringSearch_->text().trimmed();
    const QString previousQuery = stringSearch_->property(
        "resource_authoring.previous_string_search_query").toString();
    const QPersistentModelIndex selectedCell(stringTable_->currentIndex());
    const QItemSelectionModel* const selection = stringTable_->selectionModel();
    const bool hasSelection = selection && selection->hasSelection();
    const int currentColumn = selectedCell.column();
    for ( int row = 0; row < stringTable_->rowCount(); ++row )
    {
        const QTableWidgetItem* const key = stringTable_->item(row, 0);
        const QTableWidgetItem* const value = stringTable_->item(row, 1);
        const bool visible = query.isEmpty()
            || (key && key->text().contains(query, Qt::CaseInsensitive))
            || (value && value->text().contains(query, Qt::CaseInsensitive));
        stringTable_->setRowHidden(row, !visible);
    }
    stringSearch_->setProperty("resource_authoring.previous_string_search_query", query);
    if ( !previousQuery.isEmpty() && query.isEmpty() && hasSelection && selectedCell.isValid() )
    {
        const QModelIndex selectedColumn = selectedCell.sibling(selectedCell.row(), currentColumn);
        if ( selectedColumn.isValid() && !stringTable_->isRowHidden(selectedColumn.row()) )
        {
            stringTable_->doItemsLayout();
            stringTable_->scrollTo(selectedColumn, QAbstractItemView::PositionAtCenter);
            if ( stringTable_->verticalScrollMode() == QAbstractItemView::ScrollPerItem )
            {
                // Per-item centering rounds against half the viewport; correct one step only when the final
                // cell geometry still misses the center by more than a row.
                const QRect viewportRect = stringTable_->viewport()->rect();
                const QRect selectedRect = stringTable_->visualRect(selectedColumn);
                const int centerDelta = selectedRect.center().y() - viewportRect.center().y();
                if ( !selectedRect.isEmpty() && std::abs(centerDelta) > selectedRect.height() )
                {
                    QScrollBar* const scrollBar = stringTable_->verticalScrollBar();
                    const int direction = centerDelta > 0 ? 1 : -1;
                    const int targetValue = scrollBar->value() + direction * scrollBar->singleStep();
                    if ( targetValue >= scrollBar->minimum() && targetValue <= scrollBar->maximum() )
                    {
                        scrollBar->setValue(targetValue);
                    }
                }
            }
        }
    }
}

void ResourceAuthoringDialog::pickSoundWav()
{
    QPointer<ResourceAuthoringDialog> self(this);
    QTreeWidgetItem* const current = soundEntries_ ? soundEntries_->currentItem() : nullptr;
    const int row = sourceIndexForSoundItem(current);
    if ( row < 0 || row >= static_cast<int>(soundFile_.entries.size()) )
    {
        return;
    }
    const QPersistentModelIndex targetIndex(soundEntries_->indexFromItem(current, 1));
    if ( !targetIndex.isValid() )
    {
        return;
    }
    const std::shared_ptr<const assets::ResourceLocator> locatorSnapshot = context_.resources;
    const std::uint64_t authoredRevision = authoredRevision_;
    const QCborMap authoredState = authoredContentState();
    ResourcePickerOptions options{};
    options.kind = ResourcePickerKind::wav;
    options.initialName = soundFile_.entries[static_cast<std::size_t>(row)].wav;
    if ( !context_.projectDirectory.empty() )
    {
        options.preferredDiskRoot = context_.projectDirectory;
        options.projectDirectory = context_.projectDirectory;
    }
    options.title = "Sound Picker";
    options.previewCmp = context_.masterCmp;
    options.previewProjectType = context_.projectType;
    const std::optional<std::string> selected = pickResource(nullptr, locatorSnapshot, options);
    if ( !self
        || !selected
        || !targetIndex.isValid()
        || !self->authoringSnapshotStillCurrent(authoredRevision, authoredState) )
    {
        return;
    }
    QTreeWidgetItem* const target = self->soundEntries_->itemFromIndex(targetIndex);
    if ( !target
        || self->sourceIndexForSoundItem(target) < 0
        || self->sourceIndexForSoundItem(target) >= static_cast<int>(self->soundFile_.entries.size()) )
    {
        return;
    }
    if ( targetIndex.column() == 1 )
    {
        const QString selectedText = ui::episode::displayTextForGameBytes(*selected);
        if ( target->text(1) != selectedText )
        {
            target->setText(1, selectedText);
        }
    }
}

} // namespace sed::ui::resources::detail
