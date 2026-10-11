#pragma once

#include <optional>

#include <QObject>
#include <QString>

class QEvent;
class QTabWidget;
class QWidget;

namespace sed::ui::cog
{

/** Updates a single COG editor tab label, tooltip, raw-title property, and pin icon. */
void updateCogScriptDockTabPresentation(QTabWidget* tabs, QWidget* page, const QString& rawTitle = QString{});

/** Refreshes all COG editor tab labels/tooltips/icons after hover or pin state changes. */
void refreshCogScriptDockTabPresentation(QTabWidget* tabs);

/** Keeps pinned COG editor tabs before unpinned tabs without changing relative order inside each group. */
void normalizeCogScriptTabOrder(QTabWidget* tabs);

/** Returns a selectable authored file only for an existing loose COG resource. */
std::optional<QString> cogScriptTabRevealPath(const QWidget* page);

/** Pinned tabs must be unpinned before any close affordance can remove them. */
bool cogScriptTabCanClose(const QWidget* page);

/** Event filter that handles COG editor tab hover icons and pin toggling. */
class CogScriptTabHoverFilter final : public QObject
{
public:
    explicit CogScriptTabHoverFilter(QTabWidget* tabs);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    QTabWidget* tabs_{};
};

} // namespace sed::ui::cog
