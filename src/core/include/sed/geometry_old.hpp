#pragma once

/**
 * Compatibility include for older code that still references geometry_old.hpp.
 *
 * The legacy header duplicated sed/geometry.hpp, which made math helpers and
 * geometry types drift-prone. Keep this shim until downstream callers have been
 * migrated to include sed/geometry.hpp directly.
 */
#include "sed/geometry.hpp"
