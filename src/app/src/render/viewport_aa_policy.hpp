#pragma once

#include <array>

#include <QByteArray>
#include <QSettings>
#include <QSurfaceFormat>

namespace sed::render
{

inline constexpr int kDefaultViewportMsaaSamples = 4;
inline constexpr std::array<int, 5> kSupportedViewportMsaaSamples{
    0, 2, 4, 8, 16};

inline int configuredViewportMsaaSamples(const QSettings& settings)
{
    bool valid = false;
    const int samples = settings.value(
        QStringLiteral("viewport/msaa_samples"),
        kDefaultViewportMsaaSamples).toInt(&valid);
    if (!valid)
    {
        return kDefaultViewportMsaaSamples;
    }

    for (const int supportedSamples : kSupportedViewportMsaaSamples)
    {
        if (samples == supportedSamples)
        {
            return samples;
        }
    }
    return kDefaultViewportMsaaSamples;
}

inline QSurfaceFormat viewportSurfaceFormat(const int samples)
{
    QSurfaceFormat format;
    format.setRenderableType(QSurfaceFormat::OpenGL);
    format.setVersion(3, 3);
    format.setProfile(QSurfaceFormat::CoreProfile);
    format.setDepthBufferSize(24);
    format.setStencilBufferSize(8);
    format.setSamples(samples);
    return format;
}

inline QSurfaceFormat viewportSurfaceFormat(const QSettings& settings)
{
    return viewportSurfaceFormat(configuredViewportMsaaSamples(settings));
}

struct WireframeAaPolicy
{
    bool surfaceOwnedEdgeSmoothing = false;
    bool multisample = false;
    bool legacyLineSmoothing = false;
};

constexpr WireframeAaPolicy wireframeAaPolicy(
    const bool enabled,
    const bool thickLineShaderAvailable)
{
    return {
        enabled,
        enabled,
        enabled && !thickLineShaderAvailable,
    };
}

inline constexpr char kSurfaceWireCoverageFunctionMarker[] =
    "/* SED_SURFACE_WIRE_COVERAGE_FUNCTION */";

inline QByteArray surfaceWireCoverageFunctionSource()
{
    return QByteArray(R"GLSL(
float surfaceWireOverlayCoverage(float edgeDistancePx, float inwardWidthPx)
{
  if (u_surface_wire_antialiasing_enabled)
  {
    return 1.0 - smoothstep(
      max(0.0, inwardWidthPx - 0.5),
      inwardWidthPx + 0.5,
      edgeDistancePx);
  }
  return step(edgeDistancePx, inwardWidthPx);
}
)GLSL");
}

inline QByteArray injectSurfaceWireCoverageFunction(QByteArray fragmentSource)
{
    fragmentSource.replace(
        QByteArray(kSurfaceWireCoverageFunctionMarker),
        surfaceWireCoverageFunctionSource());
    return fragmentSource;
}

} // namespace sed::render
