#pragma once

#include <vector>

#include <QVector3D>

#include "sed/level.hpp"

namespace sed::ui::sector
{

std::vector<QVector3D> wireLinesFromSector(const sed::Sector& sector);
std::vector<TVector> documentWireLinesFromSector(const sed::Sector& sector);

} // namespace sed::ui::sector
