#pragma once

#include <functional>

#include <QColor>
#include <QWidget>

class QCloseEvent;
class QLineEdit;
class QSlider;

namespace sed::ui
{

/**
 * Compact popup color editor used by property swatches.
 *
 * Saturation/value and hue controls report every pointer movement through the
 * preview callback, allowing the viewport and selected entities to update
 * while the user explores colors instead of waiting for a modal dialog.
 */
class LiveColorPopup final : public QWidget
{
public:
    enum class Presentation
    {
        Popup,
        Embedded,
    };

    using PreviewCallback = std::function<void(const QColor&)>;
    using SignedPreviewCallback = std::function<void(const QColor&, qreal)>;
    using ClosedCallback = std::function<void()>;

    LiveColorPopup(
        QWidget* parent,
        const QColor& initialColor,
        bool showAlpha,
        PreviewCallback previewCallback,
        ClosedCallback closedCallback = {},
        Presentation presentation = Presentation::Popup);

    /**
     * Surface-extra-light variant. QColor displays the absolute RGB magnitude;
     * the signed value selects brighten or dim and is kept synchronized with
     * the color's HSV Value component.
     */
    LiveColorPopup(
        QWidget* parent,
        const QColor& initialColor,
        bool showAlpha,
        qreal initialSignedIntensity,
        SignedPreviewCallback previewCallback,
        ClosedCallback closedCallback = {},
        Presentation presentation = Presentation::Popup);

    [[nodiscard]] QColor currentColor() const noexcept;
    [[nodiscard]] qreal signedIntensity() const noexcept;
    [[nodiscard]] bool hasSignedIntensity() const noexcept;

    /** Updates all popup controls and optionally emits a live preview. */
    void setCurrentColor(const QColor& color, bool notifyPreview);

    /** Updates the optional signed intensity strip and optionally previews it. */
    void setSignedIntensity(qreal intensity, bool notifyPreview);

    /** Opens the popup immediately below the anchor while keeping it on-screen. */
    void showFor(const QWidget* anchor);

protected:
    void closeEvent(QCloseEvent* event) override;

private:
    void buildUi();
    void notifyPreview();
    void setSaturationValue(qreal saturation, qreal value);
    void setHue(qreal hue);
    void syncControls();

    QColor currentColor_{};
    Presentation presentation_{ Presentation::Popup };
    qreal hue_{};
    bool showAlpha_{};
    bool showSignedIntensity_{};
    bool closeReported_{};
    qreal signedIntensity_{};
    qreal signedPolarity_{1.0};
    qreal signedSaturation_{};
    PreviewCallback previewCallback_{};
    SignedPreviewCallback signedPreviewCallback_{};
    ClosedCallback closedCallback_{};
    QWidget* colorPlane_{};
    QWidget* hueStrip_{};
    QLineEdit* hexEdit_{};
    QSlider* alphaSlider_{};
    QWidget* signedIntensityStrip_{};
    QWidget* signedIntensityEdit_{};
};

} // namespace sed::ui
