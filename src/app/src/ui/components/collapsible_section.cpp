#include "ui/components/collapsible_section.hpp"

#include <algorithm>

#include <QAbstractAnimation>
#include <QEasingCurve>
#include <QFrame>
#include <QPropertyAnimation>
#include <QSignalBlocker>
#include <QStyle>
#include <QToolButton>
#include <QVBoxLayout>

namespace sed::ui
{

CollapsibleSection::CollapsibleSection(const QString& title, QWidget* parent)
    : QWidget(parent)
{
    setObjectName("pref_section_root");

    auto* outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(0, 0, 0, 0);
    outerLayout->setSpacing(0);

    headerButton_ = new QToolButton(this);
    headerButton_->setObjectName("pref_section_header");
    headerButton_->setText(title);
    headerButton_->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    headerButton_->setCheckable(true);
    headerButton_->setChecked(true);
    headerButton_->setAutoRaise(true);
    headerButton_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    headerButton_->setIconSize(QSize(10, 10));

    expandedIcon_ = style()->standardIcon(QStyle::SP_ArrowDown, nullptr, headerButton_);
    collapsedIcon_ = style()->standardIcon(QStyle::SP_ArrowRight, nullptr, headerButton_);
    headerButton_->setIcon(expandedIcon_);

    QObject::connect(headerButton_, &QToolButton::toggled, this, &CollapsibleSection::setExpanded);

    outerLayout->addWidget(headerButton_);

    contentFrame_ = new QFrame(this);
    contentFrame_->setObjectName("pref_section_content");
    contentFrame_->setFrameShape(QFrame::NoFrame);
    contentFrame_->setFrameShadow(QFrame::Plain);
    contentFrame_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    contentFrame_->setMinimumHeight(0);
    contentFrame_->setMaximumHeight(0);

    contentLayout_ = new QVBoxLayout(contentFrame_);
    contentLayout_->setContentsMargins(16, 10, 10, 10);
    contentLayout_->setSpacing(10);

    outerLayout->addWidget(contentFrame_);

    heightAnimation_ = new QPropertyAnimation(contentFrame_, "maximumHeight", this);
    heightAnimation_->setDuration(140);
    heightAnimation_->setEasingCurve(QEasingCurve::InOutCubic);

    QObject::connect(heightAnimation_, &QPropertyAnimation::valueChanged, this,
        [this](const QVariant& value)
        {
            if (!contentResizable_ || heightAnimation_->targetObject() != this)
            {
                return;
            }

            // The splitter-owned section is the clipping viewport during a
            // resizable transition. Keep the content extent synchronized so
            // neither its styled background nor its children can paint into
            // the portion which has already collapsed.
            contentFrame_->setMaximumHeight(
                std::max(0, value.toInt() - collapsedHeight()));
        });

    QObject::connect(heightAnimation_, &QPropertyAnimation::finished, this, [this]()
        {
            if (headerButton_->isChecked())
            {
                contentFrame_->setVisible(true);
                contentFrame_->setMaximumHeight(QWIDGETSIZE_MAX);
                if (contentResizable_)
                {
                    rememberedExpandedHeight_ = std::max(
                        rememberedExpandedHeight_, height());
                    setMaximumHeight(QWIDGETSIZE_MAX);
                }
                return;
            }

            contentFrame_->setMaximumHeight(0);
            contentFrame_->setVisible(false);
            if (contentResizable_)
            {
                setMaximumHeight(collapsedHeight());
                updateGeometry();
            }
        });

    setExpandedImmediate(true);
}

QVBoxLayout* CollapsibleSection::contentLayout() const noexcept
{
    return contentLayout_;
}

void CollapsibleSection::setExpanded(const bool expanded)
{
    setExpandedImpl(expanded, true);
}

void CollapsibleSection::setExpandedImmediate(const bool expanded)
{
    setExpandedImpl(expanded, false);
}

bool CollapsibleSection::isExpanded() const noexcept
{
    return headerButton_ && headerButton_->isChecked();
}

void CollapsibleSection::setHeaderBold(const bool bold)
{
    if (!headerButton_)
    {
        return;
    }
    QFont font = headerButton_->font();
    font.setBold(bold);
    headerButton_->setFont(font);
}

void CollapsibleSection::setContentResizable(const bool resizable)
{
    if (heightAnimation_->state() == QAbstractAnimation::Running)
    {
        heightAnimation_->stop();
    }
    contentResizable_ = resizable;
    if (!contentFrame_ || !headerButton_)
    {
        return;
    }

    contentFrame_->setSizePolicy(
        QSizePolicy::Expanding,
        resizable ? QSizePolicy::Expanding : QSizePolicy::Fixed);
    if (auto* outerLayout = qobject_cast<QVBoxLayout*>(layout()))
    {
        outerLayout->setStretchFactor(contentFrame_, resizable ? 1 : 0);
    }
    setSizePolicy(
        QSizePolicy::Expanding,
        resizable ? QSizePolicy::Expanding : QSizePolicy::Preferred);
    setMaximumHeight(
        resizable && !isExpanded()
            ? collapsedHeight()
            : QWIDGETSIZE_MAX);
    updateGeometry();
}

int CollapsibleSection::collapsedHeight() const noexcept
{
    return headerButton_ ? std::max(0, headerButton_->sizeHint().height()) : 0;
}

int CollapsibleSection::expandedTargetHeight() const noexcept
{
    if (!headerButton_ || !contentFrame_)
    {
        return 0;
    }

    return std::max(
        rememberedExpandedHeight_,
        collapsedHeight() + std::max(0, contentLayout_->sizeHint().height()));
}

void CollapsibleSection::setExpandedImpl(const bool expanded, const bool animate)
{
    if (!headerButton_ || !contentFrame_ || !heightAnimation_)
    {
        return;
    }

    const QSignalBlocker blocker(headerButton_);
    headerButton_->setChecked(expanded);
    headerButton_->setIcon(expanded ? expandedIcon_ : collapsedIcon_);

    if (heightAnimation_->state() == QAbstractAnimation::Running)
    {
        heightAnimation_->stop();
    }

    if (!animate)
    {
        if (expanded)
        {
            if (contentResizable_)
            {
                setMaximumHeight(QWIDGETSIZE_MAX);
            }
            contentFrame_->setVisible(true);
            contentFrame_->setMaximumHeight(QWIDGETSIZE_MAX);
        }
        else
        {
            contentFrame_->setMaximumHeight(0);
            contentFrame_->setVisible(false);
            if (contentResizable_)
            {
                setMaximumHeight(collapsedHeight());
            }
        }
        updateGeometry();
        return;
    }

    if (expanded)
    {
        contentFrame_->setVisible(true);
        contentFrame_->layout()->activate();
        if (contentResizable_)
        {
            const int startHeight = std::max(collapsedHeight(), height());
            const int targetHeight = std::max(startHeight, expandedTargetHeight());

            heightAnimation_->setTargetObject(this);
            heightAnimation_->setPropertyName("maximumHeight");
            setMaximumHeight(startHeight);
            contentFrame_->setMaximumHeight(
                std::max(0, startHeight - collapsedHeight()));
            heightAnimation_->setStartValue(startHeight);
            heightAnimation_->setEndValue(targetHeight);
            heightAnimation_->start();
            return;
        }

        heightAnimation_->setTargetObject(contentFrame_);
        heightAnimation_->setPropertyName("maximumHeight");
        const int targetHeight = std::max(0, contentFrame_->sizeHint().height());

        heightAnimation_->setStartValue(std::max(0, contentFrame_->height()));
        heightAnimation_->setEndValue(targetHeight);
        heightAnimation_->start();
    }
    else
    {
        contentFrame_->setVisible(true);
        if (contentResizable_)
        {
            const int startHeight = std::max(collapsedHeight(), height());
            rememberedExpandedHeight_ = std::max(rememberedExpandedHeight_, startHeight);

            heightAnimation_->setTargetObject(this);
            heightAnimation_->setPropertyName("maximumHeight");
            setMaximumHeight(startHeight);
            contentFrame_->setMaximumHeight(
                std::max(0, startHeight - collapsedHeight()));
            heightAnimation_->setStartValue(startHeight);
            heightAnimation_->setEndValue(collapsedHeight());
            heightAnimation_->start();
            return;
        }

        const int startHeight = std::max(0, contentFrame_->height());
        heightAnimation_->setTargetObject(contentFrame_);
        heightAnimation_->setPropertyName("maximumHeight");
        contentFrame_->setMaximumHeight(startHeight);
        heightAnimation_->setStartValue(startHeight);
        heightAnimation_->setEndValue(0);
        heightAnimation_->start();
    }
}

} // namespace sed::ui
