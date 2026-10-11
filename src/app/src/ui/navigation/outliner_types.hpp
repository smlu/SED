#pragma once

#include <QtGlobal>

namespace sed
{

/** Entity category stored on outliner items and used by navigation helpers. */
enum class OutlinerKind : int
{
    none = 0,
    sector = 1,
    surface = 2,
    surfaceVertex = 6,
    thing = 3,
    light = 4,
    cog = 5,
};

constexpr int kOutlinerKindRole = Qt::UserRole + 0;
constexpr int kOutlinerIndexRole = Qt::UserRole + 1;
constexpr int kOutlinerSectorRole = Qt::UserRole + 2;
constexpr int kOutlinerSurfaceRole = Qt::UserRole + 3;
constexpr int kOutlinerChildrenPopulatedRole = Qt::UserRole + 4;
constexpr int kOutlinerVertexChildrenPopulatedRole = Qt::UserRole + 5;

/** Action encoded on property-tree rows that need custom navigation or resource handling. */
enum class PropertyAction : int
{
    none = 0,
    selectMirrorSurface = 1,
    selectFrameValue = 2,
    openCogScript = 3,
    selectCogEntityValue = 4,
    selectLinkedCogValue = 5,
    selectCogReferenceValue = 6,
    openCogResourceValue = 7,
    selectThingSector = 8,
};

constexpr int kPropertyActionRole = Qt::UserRole + 10;
constexpr int kPropertySectorRole = Qt::UserRole + 11;
constexpr int kPropertySurfaceRole = Qt::UserRole + 12;
constexpr int kPropertyIndentRole = Qt::UserRole + 13;
constexpr int kPropertyThingRole = Qt::UserRole + 14;
constexpr int kPropertyValueIndexRole = Qt::UserRole + 15;
constexpr int kPropertyCogRole = Qt::UserRole + 16;
constexpr int kPropertyOutlinerKindRole = Qt::UserRole + 17;
constexpr int kPropertyEntityIndexRole = Qt::UserRole + 18;
constexpr int kPropertyResourceKindRole = Qt::UserRole + 19;
constexpr int kThingParameterSearchRole = Qt::UserRole + 210;

} // namespace sed
