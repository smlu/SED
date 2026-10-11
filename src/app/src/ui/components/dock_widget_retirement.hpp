#pragma once

#include <QCoreApplication>
#include <QDockWidget>
#include <QEvent>
#include <QMainWindow>
#include <QPointer>
#include <QPropertyAnimation>

#include <functional>
#include <vector>

namespace sed::ui
{

/** Completes only this dock's deferred Qt animation callbacks while it lives. */
inline bool removeDockWidgetAndCompleteAnimations(QMainWindow* const workspace,
    QDockWidget* const dock, const std::function<bool()>& stillCurrent)
{
    const QPointer<QMainWindow> guardedWorkspace(workspace);
    const QPointer<QDockWidget> guardedDock(dock);
    const std::function<bool()> currentContext = stillCurrent;
    const auto current = [guardedWorkspace, guardedDock, currentContext]()
    {
        if ( !guardedWorkspace || !guardedDock )
        {
            return false;
        }
        const bool valid = currentContext();
        return valid && guardedWorkspace && guardedDock;
    };
    if ( !current() )
    {
        return false;
    }
    std::vector<QPointer<QPropertyAnimation>> animations{};
    const auto children = guardedDock->findChildren<QPropertyAnimation*>(
        QString{}, Qt::FindDirectChildrenOnly);
    for ( QPropertyAnimation* const animation : children )
    {
        if ( animation->targetObject() == guardedDock.data()
            && animation->propertyName() == QByteArrayLiteral("geometry") )
        {
            animations.emplace_back(animation);
        }
    }
    guardedWorkspace->removeDockWidget(guardedDock.data());
    if ( !current() )
    {
        return false;
    }
    for ( const QPointer<QPropertyAnimation>& animation : animations )
    {
        if ( animation )
        {
            // Qt's animator calls back into the layout from the animation's
            // destructor, even after its live abort. Run that existing posted
            // completion now, before the dock/layout/tabbars can be destroyed.
            QCoreApplication::sendPostedEvents(animation.data(), QEvent::DeferredDelete);
        }
        if ( !current() )
        {
            return false;
        }
        if ( animation )
        {
            // Do not retire a target while an outstanding completion still
            // retains it, even if an unexpected animation was not posted.
            return false;
        }
    }
    return true;
}

} // namespace sed::ui
