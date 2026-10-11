#include "main_window/common.hpp"

namespace sed
{

void MainWindow::frameBoundsWithNavigationPrefs(const QVector3D &min, const QVector3D &max, const bool animate)
{
    if (!glView_)
    {
        return;
    }

    QSettings settings = settingsStore();
    const float distanceScale = std::clamp(static_cast<float>(settings.value("viewport/outliner_double_click_distance_scale", 1.0).toDouble()), 0.10f, 10.0f);

    const QVector3D center = (min + max) * 0.5f;
    const QVector3D half = (max - min) * 0.5f * distanceScale;
    const QVector3D scaledMin = center - half;
    const QVector3D scaledMax = center + half;

    glView_->frameWorldBounds(scaledMin, scaledMax, animate, 280);
}

} // namespace sed
