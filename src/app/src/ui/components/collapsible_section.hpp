#pragma once

#include <QIcon>
#include <QWidget>

class QString;
class QFrame;
class QPropertyAnimation;
class QToolButton;
class QVBoxLayout;

namespace sed::ui
{

class CollapsibleSection final : public QWidget
{
public:
    explicit CollapsibleSection(const QString& title, QWidget* parent = nullptr);

    QVBoxLayout* contentLayout() const noexcept;

    void setExpanded(bool expanded);
    void setExpandedImmediate(bool expanded);
    bool isExpanded() const noexcept;

    /** Emphasize this section's title without affecting its content font. */
    void setHeaderBold(bool bold);

    /** Allow the content to consume space assigned by a parent splitter. */
    void setContentResizable(bool resizable);

private:
    void setExpandedImpl(bool expanded, bool animate);
    int collapsedHeight() const noexcept;
    int expandedTargetHeight() const noexcept;

    QToolButton* headerButton_ = nullptr;
    QFrame* contentFrame_ = nullptr;
    QVBoxLayout* contentLayout_ = nullptr;
    QIcon expandedIcon_{};
    QIcon collapsedIcon_{};
    QPropertyAnimation* heightAnimation_ = nullptr;
    bool contentResizable_{};
    int rememberedExpandedHeight_{};
};

} // namespace sed::ui
