#pragma once

#include <QCborValue>
#include <QString>

#include "gl_view.hpp"
#include "sed/level.hpp"

namespace sed::ui::recovery
{

bool architecturalLightingWorkspaceToCbor(
    const GlView::RecoveryState::ArchitecturalLightingState& state,
    QCborValue* encoded,
    QString* error = nullptr);

bool architecturalLightingWorkspaceFromCbor(
    const QCborValue& value,
    const Level& level,
    GlView::RecoveryState::ArchitecturalLightingState* state,
    QString* error = nullptr);

} // namespace sed::ui::recovery
