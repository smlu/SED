#pragma once

#include "gl_view.hpp"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QGuiApplication>
#include <QImage>
#include <QKeyEvent>
#include <QMatrix4x4>
#include <QMouseEvent>
#include <QOpenGLBuffer>
#include <QOpenGLContext>
#include <QOpenGLFramebufferObject>
#include <QOpenGLFunctions>
#include <QOpenGLShaderProgram>
#include <QOpenGLTexture>
#include <QOpenGLVertexArrayObject>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QPolygonF>
#include <QRadialGradient>
#include <QTimer>
#include <QVector2D>
#include <QVector4D>
#include <QWheelEvent>
#include <QtGlobal>

#include "sed/assets/3do.hpp"
#include "sed/assets/mat.hpp"
#include "sed/assets/resource_locator.hpp"
#include "sed/assets/spr.hpp"
#include "sed/assets/tpl.hpp"
#include "sed/level.hpp"
#include "sed/level/geometry.hpp"
#include "sed/level/thing.hpp"
#include "sed/math_utils.hpp"
#include "sed/text.hpp"
#include "thing/thing_resource_utils.hpp"
#include "thing/thing_snap_utils.hpp"

#include "render/fast_update/thing_transform_update.hpp"
#include "render/math/gizmo_math.hpp"
#include "render/mesh/render_mesh_utils.hpp"
#include "render/overlays/frame_path_overlay_utils.hpp"
#include "render/thing_visibility_utils.hpp"

#include <array>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <concepts>
#include <cstddef>
#include <cstring>
#include <limits>
#include <numbers>
#include <optional>
#include <sstream>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <utility>

#include "render/detail/common_helpers.hpp"
