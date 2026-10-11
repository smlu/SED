#pragma once

#include <algorithm>

#include <QApplication>
#include <QColor>
#include <QPainter>
#include <QPainterPath>
#include <QProxyStyle>
#include <QString>
#include <QStyleOption>
#include <QWidget>

namespace sed::ui
{

inline QColor applicationChromeColor() noexcept
{
    return QColor(31, 31, 31);
}

inline QColor applicationCanvasColor() noexcept
{
    return QColor(37, 37, 37);
}

inline QString applicationWindowStyleSheet()
{
    return QStringLiteral(R"(
QDialog
{
    background-color: #2b2b2b;
    color: #eeeeee;
}

QDockWidget
{
    background-color: #2b2b2b;
    color: #eeeeee;
}

QDockWidget::title
{
    background-color: #252525;
    border-bottom: 1px solid #414141;
    padding: 6px 8px;
    text-align: left;
}

QMenu
{
    background-color: #292929;
    border: 1px solid #454545;
    color: #eeeeee;
    padding: 4px;
}

QMenu::item
{
    border-radius: 3px;
    padding: 5px 24px 5px 8px;
}

QMenu::item:selected
{
    background-color: #41464d;
}

QMenu::item:disabled
{
    background-color: transparent;
    color: #686868;
}

QMenu::separator
{
    background-color: #454545;
    height: 1px;
    margin: 4px 6px;
}

QTreeWidget, QTreeView, QListWidget, QListView, QTableWidget, QTableView
{
    background-color: #202020;
    alternate-background-color: #242424;
    border: 1px solid #414141;
    color: #eeeeee;
    outline: 0;
}

QTreeWidget::item:disabled,
QTreeView::item:disabled,
QListWidget::item:disabled,
QListView::item:disabled
{
    color: #707070;
}

QHeaderView::section
{
    background-color: #303030;
    border: 0;
    border-right: 1px solid #484848;
    border-bottom: 1px solid #484848;
    color: #eeeeee;
    padding: 5px 7px;
}

QLineEdit, QAbstractSpinBox, QComboBox
{
    background-color: #202020;
    border: 1px solid #484848;
    border-radius: 3px;
    color: #f2f2f2;
    min-height: 19px;
    padding: 1px 6px;
    selection-background-color: #536578;
}

QTextEdit, QPlainTextEdit
{
    background-color: #202020;
    border: 1px solid #484848;
    border-radius: 3px;
    color: #f2f2f2;
    padding: 4px 6px;
    selection-background-color: #536578;
}

QComboBox
{
    padding-right: 28px;
}

QComboBox::drop-down
{
    subcontrol-origin: padding;
    subcontrol-position: top right;
    background-color: #2b2b2b;
    border: 0;
    border-left: 1px solid #414141;
    border-top-right-radius: 3px;
    border-bottom-right-radius: 3px;
    width: 23px;
}

QComboBox::drop-down:hover
{
    background-color: #383838;
}

QComboBox::drop-down:pressed
{
    background-color: #242424;
}

QComboBox::down-arrow
{
    image: url(:/sed/icons/ui_chevron_down.png);
    width: 10px;
    height: 6px;
}

QComboBox QAbstractItemView
{
    background-color: #252525;
    border: 1px solid #4b4b4b;
    color: #eeeeee;
    outline: 0;
    selection-background-color: #555c64;
    selection-color: #ffffff;
}

QAbstractSpinBox
{
    padding-right: 25px;
}

QAbstractSpinBox::up-button,
QAbstractSpinBox::down-button
{
    subcontrol-origin: padding;
    background-color: #2b2b2b;
    border: 0;
    border-left: 1px solid #414141;
    width: 21px;
}

QAbstractSpinBox::up-button
{
    subcontrol-position: top right;
    border-top-right-radius: 3px;
}

QAbstractSpinBox::down-button
{
    subcontrol-position: bottom right;
    border-top: 1px solid #414141;
    border-bottom-right-radius: 3px;
}

QAbstractSpinBox::up-arrow
{
    image: url(:/sed/icons/ui_chevron_up.png);
    width: 10px;
    height: 6px;
}

QAbstractSpinBox::down-arrow
{
    image: url(:/sed/icons/ui_chevron_down.png);
    width: 10px;
    height: 6px;
}

QAbstractSpinBox::up-button:hover,
QAbstractSpinBox::down-button:hover
{
    background-color: #383838;
}

QAbstractSpinBox::up-button:pressed,
QAbstractSpinBox::down-button:pressed
{
    background-color: #242424;
}

QLineEdit:hover, QTextEdit:hover, QPlainTextEdit:hover, QAbstractSpinBox:hover, QComboBox:hover
{
    border-color: #5a5a5a;
}

QLineEdit:focus, QTextEdit:focus, QPlainTextEdit:focus, QAbstractSpinBox:focus, QComboBox:focus
{
    border-color: #738ca6;
}

QLineEdit:disabled,
QTextEdit:disabled,
QPlainTextEdit:disabled,
QAbstractSpinBox:disabled,
QComboBox:disabled
{
    background-color: #292929;
    border-color: #343434;
    color: #707070;
    selection-background-color: #343434;
}

QComboBox::drop-down:disabled,
QAbstractSpinBox::up-button:disabled,
QAbstractSpinBox::down-button:disabled
{
    background-color: #292929;
    border-color: #343434;
}

QLabel:disabled,
QCheckBox:disabled,
QRadioButton:disabled
{
    color: #707070;
}

QToolButton[autoRaise="true"]
{
    background-color: transparent;
    border: 1px solid transparent;
    border-radius: 3px;
    color: #e5e5e5;
    padding: 3px 6px;
}

QToolButton[autoRaise="true"]:hover
{
    background-color: #3a3a3a;
    border-color: #535353;
}

QToolButton[autoRaise="true"]:pressed,
QToolButton[autoRaise="true"]:checked
{
    background-color: #2d2d2d;
    border-color: #657382;
}

QToolButton[autoRaise="true"]:disabled
{
    background-color: transparent;
    border-color: transparent;
    color: #777777;
}

QPushButton,
QToolButton[autoRaise="false"],
QToolButton[sedButtonRole="stepper"]
{
    background-color: #343434;
    border: 1px solid #4a4a4a;
    border-radius: 3px;
    color: #eeeeee;
    padding: 5px 10px;
}

QPushButton:hover,
QToolButton[autoRaise="false"]:hover,
QToolButton[sedButtonRole="stepper"]:hover
{
    background-color: #404040;
    border-color: #626262;
}

QPushButton:pressed,
QPushButton:checked,
QToolButton[autoRaise="false"]:pressed,
QToolButton[autoRaise="false"]:checked,
QToolButton[sedButtonRole="stepper"]:pressed,
QToolButton[sedButtonRole="stepper"]:checked
{
    background-color: #2d2d2d;
    border-color: #6b7885;
}

QPushButton:focus,
QToolButton[autoRaise="false"]:focus,
QToolButton[sedButtonRole="stepper"]:focus
{
    border-color: #738ca6;
}

QPushButton:default
{
    background-color: #3a3d40;
    border-color: #7d91a6;
}

QPushButton:disabled,
QToolButton[autoRaise="false"]:disabled,
QToolButton[sedButtonRole="stepper"]:disabled
{
    background-color: #2d2d2d;
    border-color: #383838;
    color: #777777;
}

QToolButton[sedButtonRole="stepper"]
{
    min-width: 18px;
    padding: 2px 5px;
}

QTabBar QToolButton
{
    background-color: #2b2b2b;
    border: 0;
    border-radius: 0;
    width: 15px;
    min-width: 15px;
    max-width: 15px;
    height: 20px;
    min-height: 20px;
    max-height: 20px;
    margin: 0;
    padding: 0;
}

QTabBar QToolButton:hover,
QTabBar QToolButton:pressed
{
    background-color: #3a3a3a;
    border: 0;
}

QTabBar QToolButton[autoRaise="true"],
QTabBar QToolButton[autoRaise="true"]:hover,
QTabBar QToolButton[autoRaise="true"]:pressed,
QTabBar QToolButton[autoRaise="true"]:focus,
QTabBar QToolButton[autoRaise="true"]:disabled,
QTabBar QToolButton[autoRaise="false"],
QTabBar QToolButton[autoRaise="false"]:hover,
QTabBar QToolButton[autoRaise="false"]:pressed,
QTabBar QToolButton[autoRaise="false"]:focus,
QTabBar QToolButton[autoRaise="false"]:disabled
{
    background-color: #2b2b2b;
    border: 0;
    border-radius: 0;
    color: #e5e5e5;
    width: 15px;
    min-width: 15px;
    max-width: 15px;
    height: 20px;
    min-height: 20px;
    max-height: 20px;
    margin: 0;
    padding: 0;
}

QTabBar QToolButton[autoRaise="true"]:hover,
QTabBar QToolButton[autoRaise="true"]:pressed,
QTabBar QToolButton[autoRaise="false"]:hover,
QTabBar QToolButton[autoRaise="false"]:pressed
{
    background-color: #3a3a3a;
    border: 0;
}

QTabBar QToolButton[autoRaise="true"]:disabled,
QTabBar QToolButton[autoRaise="false"]:disabled
{
    background-color: #2b2b2b;
    color: #96969f;
}

QTabWidget::pane
{
    background-color: #2b2b2b;
    border: 1px solid #414141;
}

QTabBar::tab
{
    background-color: transparent;
    border: 0;
    border-bottom: 2px solid transparent;
    color: #bfc3c8;
    padding: 6px 10px;
}

QTabBar::tab:hover
{
    background-color: #303030;
    color: #ffffff;
}

QTabBar::tab:selected
{
    background-color: #303030;
    border-bottom-color: #8498ad;
    color: #ffffff;
}

QScrollBar:vertical
{
    background-color: #252525;
    border: 0;
    width: 11px;
    margin: 0;
}

QScrollBar::handle:vertical
{
    background-color: #55585c;
    border-radius: 3px;
    min-height: 28px;
    margin: 2px;
}

QScrollBar::handle:vertical:hover
{
    background-color: #686c71;
}

QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical,
QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical
{
    background: none;
    border: 0;
    height: 0;
}

QScrollBar:horizontal
{
    background-color: #252525;
    border: 0;
    height: 11px;
    margin: 0;
}

QScrollBar::handle:horizontal
{
    background-color: #55585c;
    border-radius: 3px;
    min-width: 28px;
    margin: 2px;
}

QScrollBar::handle:horizontal:hover
{
    background-color: #686c71;
}

QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal,
QScrollBar::add-page:horizontal, QScrollBar::sub-page:horizontal
{
    background: none;
    border: 0;
    width: 0;
}

QToolTip
{
    background-color: #303030;
    border: 1px solid #505050;
    color: #f2f2f2;
    padding: 4px 6px;
}
)");
}

class ModernCheckBoxProxyStyle final : public QProxyStyle
{
public:
    using QProxyStyle::QProxyStyle;

    int pixelMetric(
        PixelMetric metric,
        const QStyleOption* option = nullptr,
        const QWidget* widget = nullptr) const override
    {
        switch (metric)
        {
        case PM_IndicatorWidth:
        case PM_IndicatorHeight:
            return 17;
        case PM_CheckBoxLabelSpacing:
            return 8;
        default:
            return QProxyStyle::pixelMetric(metric, option, widget);
        }
    }

    void drawPrimitive(
        PrimitiveElement element,
        const QStyleOption* option,
        QPainter* painter,
        const QWidget* widget = nullptr) const override
    {
        if (!option || !painter)
        {
            QProxyStyle::drawPrimitive(element, option, painter, widget);
            return;
        }

        if (element == PE_IndicatorArrowDown || element == PE_IndicatorArrowUp
            || element == PE_IndicatorArrowLeft || element == PE_IndicatorArrowRight)
        {
            const QRectF rect(option->rect);
            const QPointF center = rect.center();
            const bool horizontal = element == PE_IndicatorArrowLeft
                || element == PE_IndicatorArrowRight;
            const qreal halfWidth = horizontal
                ? std::clamp(rect.width() * 0.12, 1.4, 2.4)
                : std::clamp(rect.width() * 0.20, 2.5, 4.0);
            const qreal halfHeight = horizontal
                ? std::clamp(rect.height() * 0.20, 2.5, 4.0)
                : std::clamp(rect.height() * 0.12, 1.4, 2.4);

            QPainterPath chevron;
            if (element == PE_IndicatorArrowDown)
            {
                chevron.moveTo(center.x() - halfWidth, center.y() - halfHeight);
                chevron.lineTo(center.x(), center.y() + halfHeight);
                chevron.lineTo(center.x() + halfWidth, center.y() - halfHeight);
            }
            else if (element == PE_IndicatorArrowUp)
            {
                chevron.moveTo(center.x() - halfWidth, center.y() + halfHeight);
                chevron.lineTo(center.x(), center.y() - halfHeight);
                chevron.lineTo(center.x() + halfWidth, center.y() + halfHeight);
            }
            else if (element == PE_IndicatorArrowLeft)
            {
                chevron.moveTo(center.x() + halfWidth, center.y() - halfHeight);
                chevron.lineTo(center.x() - halfWidth, center.y());
                chevron.lineTo(center.x() + halfWidth, center.y() + halfHeight);
            }
            else
            {
                chevron.moveTo(center.x() - halfWidth, center.y() - halfHeight);
                chevron.lineTo(center.x() + halfWidth, center.y());
                chevron.lineTo(center.x() - halfWidth, center.y() + halfHeight);
            }

            QColor arrowColor = option->state.testFlag(State_Enabled)
                ? QColor(222, 226, 232)
                : QColor(112, 116, 122);
            painter->save();
            painter->setRenderHint(QPainter::Antialiasing, true);
            painter->setBrush(Qt::NoBrush);
            painter->setPen(QPen(arrowColor, 1.45, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            painter->drawPath(chevron);
            painter->restore();
            return;
        }

        if (element != PE_IndicatorCheckBox && element != PE_IndicatorItemViewItemCheck)
        {
            QProxyStyle::drawPrimitive(element, option, painter, widget);
            return;
        }

        const bool enabled = option->state.testFlag(State_Enabled);
        const bool hovered = option->state.testFlag(State_MouseOver);
        const bool pressed = option->state.testFlag(State_Sunken);
        const bool checked = option->state.testFlag(State_On);
        const bool partiallyChecked = option->state.testFlag(State_NoChange);

        QColor border = checked || partiallyChecked
            ? QColor(123, 162, 222)
            : QColor(86, 90, 96);
        QColor fill = checked || partiallyChecked
            ? QColor(101, 143, 210)
            : QColor(42, 44, 48);

        if (hovered)
        {
            border = border.lighter(108);
            fill = fill.lighter(104);
        }
        if (pressed)
        {
            border = border.darker(112);
            fill = fill.darker(108);
        }
        if (!enabled)
        {
            border.setAlpha(110);
            fill.setAlpha(160);
        }

        const QRectF rect = QRectF(option->rect).adjusted(1.0, 1.0, -2.0, -2.0);
        const qreal radius = 3.5;

        painter->save();
        painter->setRenderHint(QPainter::Antialiasing, true);
        painter->setPen(QPen(border, 1.0));
        painter->setBrush(fill);
        painter->drawRoundedRect(rect, radius, radius);

        if (checked)
        {
            QPainterPath tick;
            tick.moveTo(rect.left() + 3.4, rect.center().y() + 0.1);
            tick.lineTo(rect.left() + 6.2, rect.bottom() - 3.6);
            tick.lineTo(rect.right() - 3.1, rect.top() + 3.8);
            painter->setPen(QPen(QColor(255, 255, 255, enabled ? 255 : 180), 1.85, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            painter->setBrush(Qt::NoBrush);
            painter->drawPath(tick);
        }
        else if (partiallyChecked)
        {
            painter->setPen(Qt::NoPen);
            painter->setBrush(QColor(255, 255, 255, enabled ? 235 : 165));
            painter->drawRoundedRect(
                QRectF(rect.left() + 3.0, rect.center().y() - 1.1, rect.width() - 6.0, 2.2),
                1.1,
                1.1);
        }

        painter->restore();
    }
};

inline QColor neutralListSelectionFillColor(const bool active = true) noexcept
{
    return active ? QColor(150, 156, 164, 104) : QColor(150, 156, 164, 72);
}

inline QColor neutralListSelectionBorderColor() noexcept
{
    return QColor(221, 225, 230, 214);
}

inline QColor neutralListSeparatorColor() noexcept
{
    return QColor(214, 218, 223, 42);
}

inline QString neutralListSelectionStyle(const QString& selector)
{
    const QString sep = neutralListSeparatorColor().name(QColor::HexArgb);
    const QString sel = neutralListSelectionFillColor(true).name(QColor::HexArgb);
    const QString selInactive = neutralListSelectionFillColor(false).name(QColor::HexArgb);
    return QStringLiteral(
        "%1::item { border-bottom: 1px solid %2; }"
        "%1::item:selected { background-color: %3; color: #ffffff; }"
        "%1::item:selected:active { background-color: %3; color: #ffffff; }"
        "%1::item:selected:!active { background-color: %4; color: #ffffff; }")
        .arg(selector, sep, sel, selInactive);
}

inline QString modernCheckBoxStyleSheet()
{
    return QStringLiteral(
        "QCheckBox { spacing: 8px; padding: 1px 0; }");
}

inline void appendWidgetStyleSheet(QWidget* widget, const QString& extraStyle)
{
    if (!widget || extraStyle.trimmed().isEmpty())
    {
        return;
    }

    QString style = widget->styleSheet();
    if (!style.isEmpty() && !style.endsWith(QLatin1Char('\n')))
    {
        style += QLatin1Char('\n');
    }
    style += extraStyle;
    widget->setStyleSheet(style);
}

inline void ensureGlobalComponentStyles()
{
    if (!qApp || qApp->property("sed.globalComponentStylesApplied").toBool())
    {
        return;
    }

    const QString sep = neutralListSeparatorColor().name(QColor::HexArgb);
    const QString sel = neutralListSelectionFillColor(true).name(QColor::HexArgb);
    const QString selInactive = neutralListSelectionFillColor(false).name(QColor::HexArgb);
    const QString globalListStyle = QStringLiteral(
        "QListWidget, QListView, QTreeWidget, QTreeView, QTableWidget, QTableView {"
        "  alternate-background-color: rgba(214, 218, 223, 10);"
        "  gridline-color: %1;"
        "}"
        "QListWidget::item, QListView::item, QTreeWidget::item, QTreeView::item {"
        "  border-bottom: 1px solid %1;"
        "}"
        "QListWidget::item:selected, QListView::item:selected, QTreeWidget::item:selected, QTreeView::item:selected {"
        "  background-color: %2;"
        "  color: #ffffff;"
        "}"
        "QListWidget::item:selected:active, QListView::item:selected:active, QTreeWidget::item:selected:active, QTreeView::item:selected:active {"
        "  background-color: %2;"
        "  color: #ffffff;"
        "}"
        "QListWidget::item:selected:!active, QListView::item:selected:!active, QTreeWidget::item:selected:!active, QTreeView::item:selected:!active {"
        "  background-color: %3;"
        "  color: #ffffff;"
        "}")
        .arg(sep, sel, selInactive);

    if (!qApp->property("sed.modernCheckBoxStyleInstalled").toBool())
    {
        auto* proxyStyle = new ModernCheckBoxProxyStyle(qApp->style());
        proxyStyle->setParent(qApp);
        qApp->setStyle(proxyStyle);
        qApp->setProperty("sed.modernCheckBoxStyleInstalled", true);
    }

    QString style = qApp->styleSheet();
    if (!style.isEmpty() && !style.endsWith(QLatin1Char('\n')))
    {
        style += QLatin1Char('\n');
    }
    style += applicationWindowStyleSheet();
    style += QLatin1Char('\n');
    style += globalListStyle;
    style += QLatin1Char('\n');
    style += modernCheckBoxStyleSheet();
    qApp->setStyleSheet(style);
    qApp->setProperty("sed.globalComponentStylesApplied", true);
}

} // namespace sed::ui
