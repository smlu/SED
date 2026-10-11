#pragma once

#include <algorithm>
#include <cmath>
#include <functional>
#include <optional>

#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPointF>
#include <QVector3D>
#include <QWidget>

namespace sed::ui
{

/**
 * Compact continuous direction wheel used beside the exact normalized XYZ
 * editors. The selected plane determines the two rotated components and its
 * normal component is preserved. The eight perimeter marks are only visual
 * bearings: pointer drags retain their full angle instead of snapping.
 */
class SectorThrustDirectionPad final : public QWidget
{
public:
    enum class RotationPlane
    {
        XY,
        XZ,
        YZ,
    };

    explicit SectorThrustDirectionPad(QWidget* parent = nullptr)
        : QWidget(parent)
    {
        setMinimumSize(132, 112);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        updateInteractionState();
        setFocusPolicy(Qt::StrongFocus);
    }

    void setDirection(const QVector3D& direction)
    {
        const float length = direction.length();
        direction_ = (std::isfinite(length) && length > 1.0e-9f) ? (direction / length) : QVector3D{};
        updateInteractionState();
        update();
    }

    [[nodiscard]] QVector3D direction() const noexcept { return direction_; }

    void setRotationPlane(const RotationPlane plane)
    {
        if (rotationPlane_ == plane)
        {
            return;
        }
        rotationPlane_ = plane;
        updateInteractionState();
        update();
    }

    [[nodiscard]] RotationPlane rotationPlane() const noexcept { return rotationPlane_; }

    /** Whether the selected plane has a stable azimuth for this direction.
     *
     * A zero vector may be initialized in any plane. A nonzero vector parallel
     * to the plane normal cannot be rotated about that same axis: inventing an
     * arbitrary in-plane basis causes the visible 90-degree jump this widget
     * must avoid. The user can select either of the other two planes instead.
     */
    [[nodiscard]] bool canRotateInCurrentPlane() const noexcept
    {
        const float length = direction_.length();
        if (!std::isfinite(length) || length <= 1.0e-6f)
        {
            return true;
        }
        const PlaneComponents components = planeComponents(direction_);
        return std::isfinite(components.a)
            && std::isfinite(components.b)
            && std::hypot(components.a, components.b) > 1.0e-5f;
    }

    /** One pointer gesture maps to one undo transaction in the owner. */
    std::function<void()> directionEditStarted{};
    std::function<void(const QVector3D&)> directionPreviewed{};
    std::function<void(const QVector3D&)> directionEditFinished{};

    // Kept as a small compatibility seam for callers that only need a final
    // value. New editing UI should use the start/preview/finish callbacks.
    std::function<void(const QVector3D&)> directionChosen{};

protected:
    [[nodiscard]] static QPointF mousePosition(const QMouseEvent& event) noexcept
    {
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
        return event.position();
#else
        return event.localPos();
#endif
    }

    void paintEvent(QPaintEvent*) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);

        const QPointF center(width() * 0.5, height() * 0.49);
        const qreal radius = std::max<qreal>(24.0, std::min(width(), height()) * 0.34);

        painter.setPen(QPen(QColor(255, 255, 255, 42), 1.0));
        painter.setBrush(QColor(16, 18, 23, 130));
        painter.drawEllipse(center, radius + 9.0, radius + 9.0);

        for (int i = 0; i < 8; ++i)
        {
            const qreal angle = (static_cast<qreal>(i) * 45.0 - 90.0) * (3.14159265358979323846 / 180.0);
            const QPointF unit(std::cos(angle), std::sin(angle));
            const QPointF tip = center + unit * radius;
            const QPointF side(-unit.y(), unit.x());

            painter.setPen(QPen(QColor(205, 210, 220, 185), 1.8, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            painter.drawLine(tip - unit * 8.0, tip);
            painter.drawLine(tip, tip - unit * 5.0 + side * 3.5);
            painter.drawLine(tip, tip - unit * 5.0 - side * 3.5);
        }

        const PlaneComponents components = planeComponents(direction_);
        const float planarLength = std::hypot(components.a, components.b);
        if (planarLength > 1.0e-6f)
        {
            const QPointF unit(components.a / planarLength, -components.b / planarLength);
            const QPointF tip = center + unit * (radius - 3.0);
            const QPointF side(-unit.y(), unit.x());
            QPen thrustPen(QColor(226, 54, 48), 5.2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
            painter.setPen(thrustPen);
            painter.drawLine(center, tip);
            painter.drawLine(tip, tip - unit * 12.0 + side * 7.0);
            painter.drawLine(tip, tip - unit * 12.0 - side * 7.0);
        }
        else
        {
            painter.setPen(QPen(QColor(226, 54, 48), 3.0));
            painter.setBrush(components.normal >= 0.0f ? QColor(226, 54, 48, 160) : QColor(16, 18, 23));
            painter.drawEllipse(center, 7.0, 7.0);
        }

    }

    void mousePressEvent(QMouseEvent* event) override
    {
        if (!event || event->button() != Qt::LeftButton)
        {
            QWidget::mousePressEvent(event);
            return;
        }

        const QPointF position = mousePosition(*event);
        const auto pointerAngle = pointerAngleAt(position);
        if (!pointerAngle || !canRotateInCurrentPlane())
        {
            event->accept();
            return;
        }

        dragging_ = true;
        dragStartDirection_ = direction_;
        lastPointerAngle_ = *pointerAngle;
        lastPointerPosition_ = position;
        accumulatedAngle_ = 0.0;
        pointerTrackingInterrupted_ = false;
        if (directionEditStarted)
        {
            directionEditStarted();
        }
        event->accept();
    }

    void mouseMoveEvent(QMouseEvent* event) override
    {
        if (!event || !dragging_ || !event->buttons().testFlag(Qt::LeftButton))
        {
            QWidget::mouseMoveEvent(event);
            return;
        }

        if (const auto chosen = directionForPointer(mousePosition(*event)))
        {
            previewDirection(*chosen);
        }
        event->accept();
    }

    void mouseReleaseEvent(QMouseEvent* event) override
    {
        if (!event || event->button() != Qt::LeftButton || !dragging_)
        {
            QWidget::mouseReleaseEvent(event);
            return;
        }

        // Mouse moves are the sole preview source.  Re-evaluating the release
        // position here made the final value platform-dependent: a coalesced
        // or rounded release event could apply one last, visible angular jump
        // after the user had stopped dragging.
        // Flush the final preview before the owner queues its property-row
        // rebuild. Normal QWidget updates are coalesced, which could leave the
        // penultimate needle on screen until the replacement widget appeared
        // and look like a release-time angular snap even though the stored
        // vector was already correct.
        repaint();
        const QVector3D finishedDirection = direction_;
        dragging_ = false;
        if (directionEditFinished)
        {
            directionEditFinished(finishedDirection);
        }
        if (directionChosen)
        {
            directionChosen(finishedDirection);
        }
        event->accept();
    }

private:
    struct PlaneComponents
    {
        float a{};
        float b{};
        float normal{};
    };

    [[nodiscard]] PlaneComponents planeComponents(const QVector3D& value) const noexcept
    {
        switch (rotationPlane_)
        {
        case RotationPlane::XY:
            return PlaneComponents{ value.x(), value.y(), value.z() };
        case RotationPlane::XZ:
            return PlaneComponents{ value.x(), value.z(), value.y() };
        case RotationPlane::YZ:
            return PlaneComponents{ value.y(), value.z(), value.x() };
        }
        return {};
    }

    [[nodiscard]] QVector3D vectorFromPlaneComponents(const PlaneComponents value) const noexcept
    {
        switch (rotationPlane_)
        {
        case RotationPlane::XY:
            return QVector3D(value.a, value.b, value.normal);
        case RotationPlane::XZ:
            return QVector3D(value.a, value.normal, value.b);
        case RotationPlane::YZ:
            return QVector3D(value.normal, value.a, value.b);
        }
        return {};
    }

    [[nodiscard]] QString planeName() const
    {
        switch (rotationPlane_)
        {
        case RotationPlane::XY: return QStringLiteral("XY");
        case RotationPlane::XZ: return QStringLiteral("XZ");
        case RotationPlane::YZ: return QStringLiteral("YZ");
        }
        return {};
    }

    [[nodiscard]] QString normalAxisName() const
    {
        switch (rotationPlane_)
        {
        case RotationPlane::XY: return QStringLiteral("Z");
        case RotationPlane::XZ: return QStringLiteral("Y");
        case RotationPlane::YZ: return QStringLiteral("X");
        }
        return {};
    }

    void updateInteractionState()
    {
        if (!canRotateInCurrentPlane())
        {
            setCursor(Qt::ForbiddenCursor);
            setToolTip(QStringLiteral(
                "The direction is parallel to the %1 rotation axis, so rotating about %1 has no stable direction. "
                "Choose the %2 or %3 plane to rotate without a jump.")
                    .arg(normalAxisName())
                    .arg(rotationPlane_ == RotationPlane::XZ ? QStringLiteral("XY") : QStringLiteral("XZ"))
                    .arg(rotationPlane_ == RotationPlane::YZ ? QStringLiteral("XY") : QStringLiteral("YZ")));
            return;
        }

        setCursor(Qt::PointingHandCursor);
        setToolTip(QStringLiteral(
            "Drag relative to the current arrow for smooth rotation in the %1 plane (about %2). "
            "The %2 component and positive magnitude are preserved; choosing from zero initializes magnitude 1. "
            "Use normalized X/Y/Z below for exact 3D direction.")
                .arg(planeName(), normalAxisName()));
    }

    [[nodiscard]] std::optional<double> pointerAngleAt(const QPointF& position) const
    {
        const QPointF center(width() * 0.5, height() * 0.49);
        const QPointF delta = position - center;
        if (std::hypot(delta.x(), delta.y()) <= 8.0)
        {
            return std::nullopt;
        }

        return std::atan2(-delta.y(), delta.x());
    }

    [[nodiscard]] std::optional<QVector3D> directionForPointer(const QPointF& position)
    {
        const std::optional<double> pointerAngle = pointerAngleAt(position);
        if (!pointerAngle)
        {
            // Azimuth is undefined at the wheel center.  Remember the gap so
            // the first valid sample on the other side becomes a fresh
            // pointer anchor instead of contributing an artificial ~180 deg
            // delta to the direction.
            pointerTrackingInterrupted_ = true;
            lastPointerPosition_ = position;
            return std::nullopt;
        }

        if (pointerTrackingInterrupted_)
        {
            lastPointerAngle_ = *pointerAngle;
            lastPointerPosition_ = position;
            pointerTrackingInterrupted_ = false;
            return std::nullopt;
        }

        // Mouse events may be coalesced, so a fast pass through the dead zone
        // can have valid samples on both sides without ever delivering an
        // event inside it.  Detect the chord crossing as well; applying its
        // signed-angle delta would otherwise look like an instantaneous half
        // turn of the needle.
        const QPointF center(width() * 0.5, height() * 0.49);
        const QPointF from = lastPointerPosition_ - center;
        const QPointF to = position - center;
        const QPointF chord = to - from;
        const double chordLengthSquared = chord.x() * chord.x() + chord.y() * chord.y();
        if (chordLengthSquared > 1.0e-9)
        {
            const double closestParameter = std::clamp(
                -(from.x() * chord.x() + from.y() * chord.y()) / chordLengthSquared,
                0.0,
                1.0);
            const QPointF closest = from + chord * closestParameter;
            constexpr double kCenterDeadZoneRadius = 8.0;
            if (closest.x() * closest.x() + closest.y() * closest.y()
                <= kCenterDeadZoneRadius * kCenterDeadZoneRadius)
            {
                lastPointerAngle_ = *pointerAngle;
                lastPointerPosition_ = position;
                return std::nullopt;
            }
        }

        constexpr double kPi = 3.14159265358979323846;
        constexpr double kTau = kPi * 2.0;
        accumulatedAngle_ += std::remainder(*pointerAngle - lastPointerAngle_, kTau);
        lastPointerAngle_ = *pointerAngle;
        lastPointerPosition_ = position;

        PlaneComponents components = planeComponents(dragStartDirection_);
        float planarLength = std::hypot(components.a, components.b);
        double startAngle = std::atan2(
            static_cast<double>(components.b),
            static_cast<double>(components.a));

        // A zero vector has no azimuth, but intentionally gains a deterministic
        // in-plane direction on its first drag. Nonzero directions parallel to
        // the normal are rejected at mouse press so they never reach this
        // branch and jump onto an arbitrary perpendicular axis.
        if (!std::isfinite(planarLength) || planarLength <= 1.0e-5f)
        {
            if (dragStartDirection_.lengthSquared() > 1.0e-12f)
            {
                return std::nullopt;
            }
            components.normal = 0.0f;
            planarLength = 1.0f;
            startAngle = 0.0;
        }

        const double angle = startAngle + accumulatedAngle_;
        components.a = static_cast<float>(std::cos(angle)) * planarLength;
        components.b = static_cast<float>(std::sin(angle)) * planarLength;
        QVector3D chosen = vectorFromPlaneComponents(components);
        const float length = chosen.length();
        if (!std::isfinite(length) || length <= 1.0e-9f)
        {
            return std::nullopt;
        }
        return chosen / length;
    }

    void previewDirection(const QVector3D& chosen)
    {
        if ((direction_ - chosen).lengthSquared() <= 1.0e-12f)
        {
            return;
        }
        setDirection(chosen);
        // Keep the visible needle in lock-step with every accepted preview.
        // The wheel is small enough that an immediate paint is cheap, and it
        // prevents a coalesced paint from first becoming visible on release.
        repaint();
        if (directionPreviewed)
        {
            directionPreviewed(direction_);
        }
    }

    QVector3D direction_{};
    QVector3D dragStartDirection_{};
    RotationPlane rotationPlane_{RotationPlane::XY};
    QPointF lastPointerPosition_{};
    double lastPointerAngle_{};
    double accumulatedAngle_{};
    bool pointerTrackingInterrupted_{};
    bool dragging_{};
};

} // namespace sed::ui
