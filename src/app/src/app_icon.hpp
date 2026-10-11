#pragma once

#include <QIcon>

class QColor;
class QWidget;

namespace sed
{

/// Loads the multi-size application icon used by windows and startup UI.
QIcon loadApplicationIcon();

/// Loads the black-backed icon used by the executable shell and branded UI.
QIcon loadApplicationShellIcon();

/// Applies the executable's large icon to a Windows taskbar entry.
///
/// Qt keeps using the separately embedded borderless icon for the small
/// title-bar slot. Other platforms leave their native icon handling unchanged.
bool applyWindowsTaskbarIcon(QWidget& window);

/// Matches a Windows native caption to the supplied client-area color.
bool applyWindowsTitleBarColor(QWidget& window, const QColor& color);

/// Recalculates an existing Windows frame after a native titlebar transition.
/// Does not create a handle or change window flags, geometry, or activation.
bool refreshWindowsNativeWindowFrame(QWidget& window);

} // namespace sed
