#pragma once
#include <QByteArray>
#include <QColor>
#include "sed/image.hpp"
#include <cstdint>

namespace sed::app_tests::fixtures
{
inline QByteArray makeDiskChangeModelFixture(
    const QByteArray& material,
    const QByteArray& revisionComment,
    const double halfWidth,
    const std::uint32_t faceFlags = 0u)
{
    QByteArray model{
        "SECTION: HEADER\n"
        "3DO 2.3\n"
        "SECTION: MODELRESOURCE\n"
        "MATERIALS 1\n"
        "0: " };
    model += material;
    model +=
        "\nSECTION: GEOMETRYDEF\n"
        "RADIUS 4\n"
        "INSERT OFFSET 0 0 0\n"
        "GEOSETS 1\n"
        "GEOSET 0\n"
        "MESHES 1\n"
        "MESH 0\n"
        "NAME disk_change_mesh\n"
        "RADIUS 4\n"
        "GEOMETRYMODE 4\n"
        "LIGHTINGMODE 3\n"
        "TEXTUREMODE 3\n"
        "VERTICES 4\n"
        "0: ";
    model += QByteArray::number(-halfWidth, 'f', 3);
    model +=
        " 0 -1 0 0 0 1\n"
        "1: ";
    model += QByteArray::number(halfWidth, 'f', 3);
    model +=
        " 0 -1 0 0 0 1\n"
        "2: ";
    model += QByteArray::number(halfWidth, 'f', 3);
    model +=
        " 0 1 0 0 0 1\n"
        "3: ";
    model += QByteArray::number(-halfWidth, 'f', 3);
    model +=
        " 0 1 0 0 0 1\n"
        "TEXTURE VERTICES 4\n"
        "0: 0 0\n"
        "1: 1 0\n"
        "2: 1 1\n"
        "3: 0 1\n"
        "VERTEX NORMALS\n"
        "0: 0 -1 0\n"
        "1: 0 -1 0\n"
        "2: 0 -1 0\n"
        "3: 0 -1 0\n"
        "FACES 1\n"
        "0: 0 0x";
    model += QByteArray::number(static_cast<qulonglong>(faceFlags), 16);
    model +=
        " 4 3 3 (1/1/1/1) 4 0, 0 1, 1 2, 2 3, 3\n"
        "FACE NORMALS\n"
        "0: 0 -1 0\n"
        "SECTION: HIERARCHYDEF\n"
        "HIERARCHY NODES 1\n"
        "0: 0x0 0x1 0 -1 -1 -1 0 0 0 0 0 0 0 0 0 0 disk_change_mesh\n"
        "# ";
    model += revisionComment;
    model += '\n';
    return model;
}

inline QByteArray makeClassicCmpFixture(
    const std::uint8_t solidColorIndex,
    const QColor& solidColor)
{
    QByteArray bytes(64 + 256 * 3 + 64 * 256, '\0');
    bytes[0] = 'C';
    bytes[1] = 'M';
    bytes[2] = 'P';
    bytes[3] = ' ';

    bytes[64 + solidColorIndex * 3 + 0] =
        static_cast<char>(solidColor.red());
    bytes[64 + solidColorIndex * 3 + 1] =
        static_cast<char>(solidColor.green());
    bytes[64 + solidColorIndex * 3 + 2] =
        static_cast<char>(solidColor.blue());

    for (int level = 0; level < 64; ++level)
    {
        for (int paletteIndex = 0; paletteIndex < 256; ++paletteIndex)
        {
            bytes[64 + 256 * 3 + level * 256 + paletteIndex] =
                static_cast<char>(paletteIndex);
        }
    }
    return bytes;
}

inline QByteArray makeIndexedMatFixture(const std::uint8_t solidColorIndex)
{
    QByteArray bytes("MAT ", 4);
    const auto appendU32 = [&bytes](const std::uint32_t value)
    {
        bytes.append(static_cast<char>(value & 0xffu));
        bytes.append(static_cast<char>((value >> 8u) & 0xffu));
        bytes.append(static_cast<char>((value >> 16u) & 0xffu));
        bytes.append(static_cast<char>((value >> 24u) & 0xffu));
    };

    appendU32(0x32u);
    appendU32(2u);
    appendU32(1u);
    appendU32(1u);
    appendU32(static_cast<std::uint32_t>(ColorMode::Indexed));
    appendU32(8u);
    for (int field = 0; field < 12; ++field)
    {
        appendU32(0u);
    }

    appendU32(8u);
    appendU32(solidColorIndex);
    for (int field = 0; field < 8; ++field)
    {
        appendU32(0u);
    }

    appendU32(2u);
    appendU32(2u);
    appendU32(0u);
    appendU32(0u);
    appendU32(0u);
    appendU32(1u);
    bytes.append(static_cast<char>(solidColorIndex));
    bytes.append(static_cast<char>(solidColorIndex));
    bytes.append(static_cast<char>(solidColorIndex));
    bytes.append(static_cast<char>(solidColorIndex));
    return bytes;
}


}
