#include "ui/components/property_editor_controls.hpp"

#include <QAbstractButton>
#include <QColor>
#include <QCursor>
#include <QImage>
#include <QSize>
#include <QSizePolicy>
#include <QString>
#include <QToolButton>
#include <QVariant>

namespace sed::ui
{

void configureColorSwatchButton(QAbstractButton* button, const QColor& color)
{
    if (!button || !color.isValid())
    {
        return;
    }

    // Step 1: make the whole compact field an immediately recognizable color target.
    button->setText(QString{});
    button->setProperty("sedColorSwatch", true);
    button->setProperty("sedColor", color);
    button->setFocusPolicy(Qt::NoFocus);
    button->setCursor(Qt::PointingHandCursor);
    button->setMinimumHeight(22);
    button->setMaximumHeight(24);
    button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    // Step 2: define every interaction state so the platform style cannot add a
    // dotted text-selection rectangle over the color sample.
    button->setStyleSheet(QStringLiteral(
        "QPushButton[sedColorSwatch=\"true\"] {"
        " background-color: rgba(%1, %2, %3, %4);"
        " border: 1px solid rgba(235, 238, 243, 68);"
        " border-radius: 3px;"
        " padding: 0;"
        " outline: none;"
        "}"
        "QPushButton[sedColorSwatch=\"true\"]:hover {"
        " border-color: rgba(235, 238, 243, 150);"
        "}"
        "QPushButton[sedColorSwatch=\"true\"]:pressed {"
        " border-color: rgba(255, 255, 255, 205);"
        "}"
        "QPushButton[sedColorSwatch=\"true\"]:focus {"
        " border-color: rgba(235, 238, 243, 150);"
        " outline: none;"
        "}")
        .arg(color.red())
        .arg(color.green())
        .arg(color.blue())
        .arg(color.alpha()));
}

void configureMaterialPreviewButton(QToolButton* button)
{
    if (!button)
    {
        return;
    }

    button->setProperty("sedMaterialPreview", true);
    button->setAutoRaise(true);
    button->setFixedSize(56, 34);
    button->setIconSize(QSize(34, 30));
    button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    button->setFocusPolicy(Qt::NoFocus);
    button->setStyleSheet(QStringLiteral(
        "QToolButton[sedMaterialPreview=\"true\"] {"
        " background-color: transparent;"
        " border: 0;"
        " padding: 1px 4px;"
        "}"
        "QToolButton[sedMaterialPreview=\"true\"]:hover {"
        " background-color: #262626;"
        "}"));
}

QImage materialPreviewImageForWidget(const QImage& previewImage)
{
    // loadPreviewMaterialImage() is shared with the software 3DO preview and
    // returns decoded MAT pixels in the orientation consumed by both paths.
    // QLabel/QIcon paint in ordinary top-left image coordinates, so preserve
    // that shared orientation for property-editor thumbnails.
    return previewImage;
}

} // namespace sed::ui
