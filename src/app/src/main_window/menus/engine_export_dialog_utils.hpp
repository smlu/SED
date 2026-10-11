#pragma once

#include <algorithm>
#include <cmath>

#include <QFileInfo>
#include <QString>

#include "sed/level.hpp"

namespace sed::ui::export_dialog
{

enum class Option
{
    Lights,
    Layers,
    Compact,
    ClampExtraLight,
    OmitDefaultMotsArchLighting,
    WeldNearbyVertices,
    VertexWeldDistance,
};

inline constexpr double kDefaultVertexWeldDistance = 0.00001;
inline constexpr double kMinimumVertexWeldDistance = 0.00000001;

inline double normalizedVertexWeldDistance(const double value) noexcept
{
    if (!std::isfinite(value))
    {
        return kDefaultVertexWeldDistance;
    }
    return std::max(kMinimumVertexWeldDistance, value);
}

/** Shared, user-facing option text for the Qt engine-export dialog. */
inline QString optionLabel(const Option option)
{
    switch (option)
    {
        case Option::Lights:
            return QStringLiteral("Include lights");
        case Option::Layers:
            return QStringLiteral("Include layers");
        case Option::Compact:
            return QStringLiteral("Compact output");
        case Option::ClampExtraLight:
            return QStringLiteral("Clamp extra light");
        case Option::OmitDefaultMotsArchLighting:
            return QStringLiteral("Omit default zero ARCHLIGHTING records");
        case Option::WeldNearbyVertices:
            return QStringLiteral("Weld nearby vertices");
        case Option::VertexWeldDistance:
            return QStringLiteral("Max distance");
    }
    return {};
}

inline QString optionDescription(const Option option)
{
    switch (option)
    {
        case Option::Lights:
            return QStringLiteral(
                "Include editor light data in the exported level.");
        case Option::Layers:
            return QStringLiteral(
                "Include editor layer data in the exported level.");
        case Option::Compact:
            return QStringLiteral(
                "Reduce the export file size without changing level content.");
        case Option::ClampExtraLight:
            return QStringLiteral(
                "Clamp exported surface extra light to [-1, 1]. Clear to preserve authored values outside this range.");
        case Option::OmitDefaultMotsArchLighting:
            return QStringLiteral(
                "MotS only. Writes -1 for all-zero ARCHLIGHTING records so the game uses 3DO vertex lighting instead. This can differ from the editor preview.");
        case Option::WeldNearbyVertices:
            return QStringLiteral(
                "Join nearby vertices in the exported level using the maximum Euclidean world-space distance.");
        case Option::VertexWeldDistance:
            return QStringLiteral(
                "Maximum Euclidean world-space distance for joining vertices.");
    }
    return {};
}

enum class FilenameKind
{
    Empty,
    Runtime,
    Legacy,
    Unknown,
};

inline FilenameKind classifyFilename(const QString& filename)
{
    const QString suffix = QFileInfo(filename).suffix().toLower();
    if (suffix == QStringLiteral("jkl")
        || suffix == QStringLiteral("ndy"))
    {
        return FilenameKind::Runtime;
    }
    if (suffix == QStringLiteral("jed"))
    {
        return FilenameKind::Legacy;
    }
    if (suffix.isEmpty())
    {
        return FilenameKind::Empty;
    }
    return FilenameKind::Unknown;
}

inline bool isRuntimeFilename(
    const QString& filename,
    const ProjectType project)
{
    const QString suffix = QFileInfo(filename).suffix().toLower();
    return (project == ProjectType::IJIM
            && suffix == QStringLiteral("ndy"))
        || (project != ProjectType::IJIM
            && suffix == QStringLiteral("jkl"));
}

inline bool runtimeControlsVisible(
    const QString& filename,
    const bool jedOnlyFilter,
    const ProjectType project)
{
    switch (classifyFilename(filename))
    {
        case FilenameKind::Runtime:
            // A compatible explicit runtime suffix is authoritative even if
            // the user has not changed the currently selected legacy filter
            // yet.  A suffix for another engine is intentionally rejected.
            return isRuntimeFilename(filename, project);
        case FilenameKind::Empty:
            return !jedOnlyFilter;
        case FilenameKind::Legacy:
        case FilenameKind::Unknown:
            return false;
    }
    return false;
}

} // namespace sed::ui::export_dialog
