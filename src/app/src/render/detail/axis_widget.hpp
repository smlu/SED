#pragma once

struct AxisWidgetEnd
{
    int axis{}; // 0 = X, 1 = Y, 2 = Z
    int sign{}; // +1 or -1
    QPointF pos{};
    float depth{}; // higher = closer to camera
    bool front{};
};

struct AxisWidgetLayout
{
    QRect rect{};
    QPointF center{};
    float outerRadius{};
    float axisLen{};
    float frontRadius{};
    float backRadius{};
    std::array<AxisWidgetEnd, 6> ends{};
};

QRect axisWidgetRectForViewport(const QWidget* viewWidget, const int widgetW, const int widgetH)
{
    constexpr int kMargin = 10;
    constexpr int kMaxSize = 86;
    constexpr int kMinSize = 56;

    const int avail = std::min(widgetW, widgetH) - (kMargin * 2);
    const int size = std::clamp(avail, 0, kMaxSize);
    if (size < kMinSize)
    {
        return {};
    }

    int x = widgetW - kMargin - size;
    int y = kMargin;

    if (viewWidget)
    {
        if (const QWidget* overlay = viewWidget->findChild<QWidget*>("viewportOverlay"))
        {
            if (overlay->isVisible())
            {
                y = overlay->geometry().bottom() + 1 + kMargin;
            }
        }

        const QWidget* reserved = nullptr;
        if (const QWidget* panel = viewWidget->findChild<QWidget*>("thingPlacementPanel"))
        {
            if (panel->isVisible())
            {
                reserved = panel;
            }
        }
        if (!reserved)
        {
            if (const QWidget* collapsed = viewWidget->findChild<QWidget*>("thingPlacementCollapsedBar"))
            {
                if (collapsed->isVisible())
                {
                    reserved = collapsed;
                }
            }
        }

        if (reserved)
        {
            const QRect reserveRect = reserved->geometry().adjusted(-kMargin, -kMargin, kMargin, kMargin);
            QRect axisRect(x, y, size, size);
            if (axisRect.intersects(reserveRect))
            {
                const bool rightDock = reserveRect.right() >= widgetW - kMargin;
                const bool topDock = reserveRect.top() <= kMargin;
                const bool bottomDock = reserveRect.bottom() >= widgetH - kMargin;

                if (rightDock)
                {
                    x = reserveRect.left() - size - kMargin;
                }
                else if (topDock)
                {
                    y = reserveRect.bottom() + kMargin;
                }
                else if (bottomDock)
                {
                    y = reserveRect.top() - size - kMargin;
                }
            }
        }
    }

    const int maxX = std::max(kMargin, widgetW - kMargin - size);
    const int maxY = std::max(kMargin, widgetH - kMargin - size);
    x = std::clamp(x, kMargin, maxX);
    y = std::clamp(y, kMargin, maxY);
    return QRect(x, y, size, size);
}

AxisWidgetLayout computeAxisWidgetLayout(const QMatrix4x4& view, const QRect& rect)
{
    AxisWidgetLayout out{};
    if (!rect.isValid())
    {
        return out;
    }

    const int size = std::min(rect.width(), rect.height());

    out.rect = rect;
    out.center = QPointF(out.rect.center());
    out.outerRadius = (static_cast<float>(size) * 0.5f) - 1.0f;
    out.axisLen = static_cast<float>(size) * 0.30f;
    out.frontRadius = std::clamp(static_cast<float>(size) * 0.16f, 7.0f, 12.0f);
    out.backRadius = out.frontRadius * 0.55f;

    const std::array<QVector3D, 3> axes = {
        QVector3D(1.0f, 0.0f, 0.0f),
        QVector3D(0.0f, 1.0f, 0.0f),
        QVector3D(0.0f, 0.0f, 1.0f),
    };

    QVector3D camRight(1.0f, 0.0f, 0.0f);
    QVector3D camUp(0.0f, 1.0f, 0.0f);
    QVector3D camBack(0.0f, 0.0f, 1.0f);
    {
        bool invOk = false;
        const QMatrix4x4 invView = view.inverted(&invOk);
        if (invOk)
        {
            const QVector4D r4 = invView.column(0);
            const QVector4D u4 = invView.column(1);
            const QVector4D b4 = invView.column(2);
            camRight = QVector3D(r4.x(), r4.y(), r4.z());
            camUp = QVector3D(u4.x(), u4.y(), u4.z());
            camBack = QVector3D(b4.x(), b4.y(), b4.z());

            if (camRight.lengthSquared() > sed::math::kTinyEpsilonF)
            {
                camRight.normalize();
            }
            else
            {
                camRight = QVector3D(1.0f, 0.0f, 0.0f);
            }

            if (camUp.lengthSquared() > sed::math::kTinyEpsilonF)
            {
                camUp.normalize();
            }
            else
            {
                camUp = QVector3D(0.0f, 1.0f, 0.0f);
            }

            if (camBack.lengthSquared() > sed::math::kTinyEpsilonF)
            {
                camBack.normalize();
            }
            else
            {
                camBack = QVector3D(0.0f, 0.0f, 1.0f);
            }
        }
    }

    constexpr float kSnapProjLen2 = 0.0030f; // ~within 3 degrees of the view axis
    std::array<QPointF, 3> p2s{};
    std::array<float, 3> depths{};
    std::array<float, 3> projLen2s{};

    for (int a = 0; a < 3; ++a)
    {
        const QVector3D& axis = axes[static_cast<std::size_t>(a)];
        const float projX = QVector3D::dotProduct(axis, camRight);
        const float projY = QVector3D::dotProduct(axis, camUp);
        const float depth = QVector3D::dotProduct(axis, camBack);

        QPointF p2(projX * out.axisLen, -projY * out.axisLen);
        // Floating-point projection can leave an axis that should be exactly
        // view-aligned (for example Z in Top view) a pixel or two off-center.
        // Snap small projections to center; free orbit itself is not clamped.
        const float projLen2 = (projX * projX) + (projY * projY);
        if (projLen2 < kSnapProjLen2)
        {
            p2 = QPointF(0.0, 0.0);
        }

        p2s[static_cast<std::size_t>(a)] = p2;
        depths[static_cast<std::size_t>(a)] = depth;
        projLen2s[static_cast<std::size_t>(a)] = projLen2;
    }

    // If we're in a near-axis view (top/front/side), tiny numerical offsets can cause in-plane axes
    // to be considered "front" on the wrong end (label flips). Stabilize by treating small depths as 0.
    int viewAxis = -1;
    float bestProj = kSnapProjLen2;
    for (int a = 0; a < 3; ++a)
    {
        const float p = projLen2s[static_cast<std::size_t>(a)];
        if (p < bestProj)
        {
            bestProj = p;
            viewAxis = a;
        }
    }

    if (viewAxis >= 0)
    {
        constexpr float kInPlaneDepthEps = 0.06f;
        for (int a = 0; a < 3; ++a)
        {
            if (a == viewAxis)
            {
                continue;
            }
            float& d = depths[static_cast<std::size_t>(a)];
            if (std::abs(d) < kInPlaneDepthEps)
            {
                d = 0.0f;
            }
        }
    }

    for (int a = 0; a < 3; ++a)
    {
        const QPointF p2 = p2s[static_cast<std::size_t>(a)];
        const float depth = depths[static_cast<std::size_t>(a)];

        AxisWidgetEnd plus{};
        plus.axis = a;
        plus.sign = +1;
        plus.pos = out.center + p2;
        plus.depth = depth;

        AxisWidgetEnd minus{};
        minus.axis = a;
        minus.sign = -1;
        minus.pos = out.center - p2;
        minus.depth = -depth;

        const bool plusFront = plus.depth >= minus.depth;
        plus.front = plusFront;
        minus.front = !plusFront;

        out.ends[static_cast<std::size_t>(a) * 2 + 0] = plus;
        out.ends[static_cast<std::size_t>(a) * 2 + 1] = minus;
    }

    return out;
}
