#pragma once
#include "sed/level/cleave.hpp"
#include <QString>

namespace sed::ui::viewport
{
/** Formats a rejected operation without confusing source and candidate geometry. */
inline QString sectorCleaveFailureMessage(const level::SectorCleaveBatchResult& result)
{
    using Error = level::SectorCleaveBatchError;
    if ( result.error == Error::None )
    {
        return {};
    }
    QString target;
    if ( result.failedSector >= 0 )
    {
        target = QStringLiteral("Sector %1").arg(result.failedSector);
        if ( result.failedSurface >= 0 )
        {
            target += QStringLiteral(" / Surface %1").arg(result.failedSurface);
        }
    }
    if ( result.error == Error::InvalidInputGeometry )
    {
        return QStringLiteral("Cleave rejected: ") +
               (target.isEmpty() ? QStringLiteral("the selection") : target) +
               QStringLiteral(" has invalid input geometry. Repair it before cleaving.");
    }
    const QString prefix =
        QStringLiteral("Cleave rejected: ") + (target.isEmpty() ? QString{} : target + QStringLiteral(": "));
    switch ( result.error )
    {
    case Error::InvalidSector:
        return prefix + QStringLiteral("the selected sector does not exist. Select an existing sector.");
    case Error::EmptyPlaneSet:
        return prefix + QStringLiteral("no cutting planes were provided. Draw a cleave line.");
    case Error::InvalidPlane:
        return prefix + (result.failedPlane < 0
                             ? QStringLiteral("the cutting plane is invalid. Redraw the line.")
                             : QStringLiteral("line %1 is invalid. Redraw the line.")
                                   .arg(static_cast<qint64>(result.failedPlane) + 1));
    case Error::PlaneDoesNotSplit:
        return prefix +
               (result.failedPlane < 0
                    ? QStringLiteral(
                          "the line does not split the selected geometry. Move the line through the sector.")
                    : QStringLiteral(
                          "line %1 does not split the selected geometry. Move the line through the sector.")
                          .arg(static_cast<qint64>(result.failedPlane) + 1));
    case Error::CleaveFailed:
        return prefix +
               QStringLiteral("the cut could not be completed. Move the line or choose another sector.");
    case Error::InvalidResultTopology:
        return prefix +
               QStringLiteral("the cut would create invalid topology. Move the line or repair the source.");
    case Error::NonPlanarResultSurface:
        return prefix +
               QStringLiteral(
                   "the cut would create a non-planar face. Move the line or flatten the source face.");
    case Error::NonConvexResultSurface:
        return prefix +
               QStringLiteral(
                   "the cut would create a non-convex face. Move the line or split the source face first.");
    case Error::NonConvexResultSector:
        return prefix +
               QStringLiteral(
                   "the cut would create a non-convex sector. Move the line or repair the source sector.");
    case Error::InvalidResultAdjoin:
        return prefix +
               QStringLiteral(
                   "the cut would create an invalid adjoin. Repair the shared boundary or move the line.");
    case Error::None:
        return {};
    case Error::InvalidInputGeometry:
        break;
    }
    return prefix + QStringLiteral("the operation was rejected.");
}
} // namespace sed::ui::viewport
