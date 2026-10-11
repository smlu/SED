#include "test_common.hpp"

#include "render/viewport_aa_policy.hpp"

#include <QSettings>
#include <QTemporaryDir>

#include <array>
#include <utility>

namespace sed::app_tests
{

void runViewportAaPreferenceTests()
{
    QTemporaryDir temporaryDirectory;
    check(temporaryDirectory.isValid(), "viewport AA test settings directory is created");
    if (!temporaryDirectory.isValid())
    {
        return;
    }

    QSettings settings(
        temporaryDirectory.filePath(QStringLiteral("viewport-aa.ini")),
        QSettings::IniFormat);
    settings.clear();
    check(
        render::configuredViewportMsaaSamples(settings)
            == render::kDefaultViewportMsaaSamples,
        "missing viewport MSAA preference uses the four-sample default");

    for (const int samples : render::kSupportedViewportMsaaSamples)
    {
        settings.setValue(QStringLiteral("viewport/msaa_samples"), samples);
        check(
            render::configuredViewportMsaaSamples(settings) == samples,
            "supported viewport MSAA preference is preserved");

        const QSurfaceFormat requestedFormat = render::viewportSurfaceFormat(settings);
        check(
            requestedFormat.samples() == samples,
            "startup surface format requests the configured MSAA count");
        check(
            requestedFormat.renderableType() == QSurfaceFormat::OpenGL,
            "startup surface format requests OpenGL");
        check(
            requestedFormat.majorVersion() == 3
                && requestedFormat.minorVersion() == 3
                && requestedFormat.profile() == QSurfaceFormat::CoreProfile,
            "startup surface format requests OpenGL 3.3 core");
        check(
            requestedFormat.depthBufferSize() == 24
                && requestedFormat.stencilBufferSize() == 8,
            "startup surface format retains depth and stencil buffers");
    }

    for (const int unsupportedSamples : std::array{ 1, 3, 6, -1 })
    {
        settings.setValue(
            QStringLiteral("viewport/msaa_samples"), unsupportedSamples);
        check(
            render::configuredViewportMsaaSamples(settings)
                == render::kDefaultViewportMsaaSamples,
            "unsupported viewport MSAA preference falls back to four samples");
    }
    settings.setValue(QStringLiteral("viewport/msaa_samples"), QStringLiteral("many"));
    check(
        render::configuredViewportMsaaSamples(settings)
            == render::kDefaultViewportMsaaSamples,
        "non-numeric viewport MSAA preference falls back to four samples");

    const render::WireframeAaPolicy aaOffWithShader =
        render::wireframeAaPolicy(false, true);
    check(
        !aaOffWithShader.surfaceOwnedEdgeSmoothing
            && !aaOffWithShader.multisample
            && !aaOffWithShader.legacyLineSmoothing,
        "Wireframe AA Off disables shader smoothing and multisample coverage");

    const render::WireframeAaPolicy aaOffFallback =
        render::wireframeAaPolicy(false, false);
    check(
        !aaOffFallback.surfaceOwnedEdgeSmoothing
            && !aaOffFallback.multisample
            && !aaOffFallback.legacyLineSmoothing,
        "Wireframe AA Off also disables smoothing in the legacy line fallback");

    const render::WireframeAaPolicy aaOnWithShader =
        render::wireframeAaPolicy(true, true);
    check(
        aaOnWithShader.surfaceOwnedEdgeSmoothing
            && aaOnWithShader.multisample
            && !aaOnWithShader.legacyLineSmoothing,
        "shader line path uses shader edge coverage and multisampling when AA is On");

    const render::WireframeAaPolicy aaOnFallback =
        render::wireframeAaPolicy(true, false);
    check(
        aaOnFallback.surfaceOwnedEdgeSmoothing
            && aaOnFallback.multisample
            && aaOnFallback.legacyLineSmoothing,
        "legacy line fallback enables line smoothing when AA is On");

    QByteArray shaderSource = QByteArrayLiteral(
        "#version 330 core\nuniform bool u_surface_wire_antialiasing_enabled;\n")
        + QByteArray(render::kSurfaceWireCoverageFunctionMarker);
    shaderSource = render::injectSurfaceWireCoverageFunction(std::move(shaderSource));
    check(
        !shaderSource.contains(render::kSurfaceWireCoverageFunctionMarker),
        "surface wire shader assembly replaces its coverage-function marker");
    check(
        shaderSource.contains("if (u_surface_wire_antialiasing_enabled)")
            && shaderSource.contains("smoothstep(")
            && shaderSource.contains("return step(edgeDistancePx, inwardWidthPx);"),
        "surface wire shader keeps smooth coverage On and a hard edge cutoff Off");
}

} // namespace sed::app_tests
