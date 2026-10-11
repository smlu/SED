#include "main_window/common.hpp"

namespace sed
{

void MainWindow::setupTransformMenu(QMenu* transformMenu)
{
    if (!transformMenu)
    {
        return;
    }

    // Step 1: register each transform command through a focused action builder.
    setupTranslateTransformAction(transformMenu);
    setupRotateTransformAction(transformMenu);
    setupScaleTransformAction(transformMenu);
    setupMirrorTransformAction(transformMenu);
    setupFlipSurfaceTransformAction(transformMenu);
}

} // namespace sed
