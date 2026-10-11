#pragma once

class QAbstractButton;
class QColor;
class QImage;
class QToolButton;

namespace sed::ui
{

/** Styles a property color field as a pure swatch without a text-selection focus frame. */
void configureColorSwatchButton(QAbstractButton* button, const QColor& color);

/** Reserves a stable, uncropped area for material thumbnails in property rows. */
void configureMaterialPreviewButton(QToolButton* button);

/** Preserves the shared MAT preview orientation for normal top-left widget painting. */
[[nodiscard]] QImage materialPreviewImageForWidget(const QImage& previewImage);

} // namespace sed::ui
