#include "test_common.hpp"

#include "app_settings.hpp"
#include "gl_view.hpp"
#include "main_window/main_window.hpp"
#include "render/lighting/dynamic_light_utils.hpp"
#include "sed/assets/3do.hpp"
#include "sed/assets/resource_locator.hpp"
#include "sed/assets/spr.hpp"
#include "ui/recovery/document_recovery_utils.hpp"

#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QColor>
#include <QCborArray>
#include <QCborMap>
#include <QCborValue>
#include <QImage>
#include <QMenu>
#include <QMetaObject>
#include <QMetaType>
#include <QPointF>
#include <QSettings>
#include <QStringList>
#include <QTemporaryDir>
#include <QVector3D>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace sed::app_tests
{
namespace
{

constexpr auto kThingLightsSettingsKey = "viewport/render_thing_lights";
constexpr auto kSectorLightsSettingsKey = "viewport/render_sector_point_lights";
constexpr auto kThingLightsActionName = "viewport.lighting.thing_lights";
constexpr auto kSectorLightsActionName = "viewport.lighting.sector_point_lights";

TplValue makeStringValue(const std::string& name, const std::string& value)
{
    TplValue result{};
    result.name = name;
    result.type = ValueType::str;
    result.stringValue = value;
    return result;
}

template <typename Function>
ColorF applyRuntimeSelfLightWithSwitch(
    Function function,
    const ProjectType project,
    const ColorF& ambient,
    const render::lighting::RuntimeThingLight& light,
    const bool enabled)
{
    if constexpr ( std::is_invocable_r_v<
                      ColorF,
                      Function,
                      ProjectType,
                      const ColorF&,
                      const render::lighting::RuntimeThingLight&,
                      bool> )
    {
        return std::invoke(function, project, ambient, light, enabled);
    }
    else
    {
        // Keeping the old helper call available lets this focused regression
        // compile before the switch is implemented. The disabled expectation
        // below then fails until the runtime contribution gate exists.
        return std::invoke(function, project, ambient, light);
    }
}

template <typename View>
constexpr bool hasThingLightRenderingSetter()
{
    return requires(View& view)
    {
        view.setThingLightRenderingEnabled(true);
    };
}

template <typename View>
constexpr bool hasSectorPointLightRenderingSetter()
{
    return requires(View& view)
    {
        view.setSectorPointLightRenderingEnabled(true);
    };
}

template <typename View>
void setThingLightRenderingIfAvailable(View& view, const bool enabled)
{
    if constexpr ( hasThingLightRenderingSetter<View>() )
    {
        view.setThingLightRenderingEnabled(enabled);
    }
}

template <typename View>
void setSectorPointLightRenderingIfAvailable(View& view, const bool enabled)
{
    if constexpr ( hasSectorPointLightRenderingSetter<View>() )
    {
        view.setSectorPointLightRenderingEnabled(enabled);
    }
}

bool exactlyEqual(const ColorF& left, const ColorF& right) noexcept
{
    return left.r == right.r
        && left.g == right.g
        && left.b == right.b
        && left.a == right.a;
}

class ScopedSettingsRestore final
{
public:
    explicit ScopedSettingsRestore(QSettings& settings)
        : settings_(settings)
        , values_{
              save(kThingLightsSettingsKey),
              save(kSectorLightsSettingsKey),
          }
    {
    }

    ~ScopedSettingsRestore()
    {
        restore(kThingLightsSettingsKey, values_[0]);
        restore(kSectorLightsSettingsKey, values_[1]);
        settings_.sync();
    }

    ScopedSettingsRestore(const ScopedSettingsRestore&) = delete;
    ScopedSettingsRestore& operator=(const ScopedSettingsRestore&) = delete;

private:
    struct Value
    {
        bool existed{};
        QVariant value{};
    };

    Value save(const char* key) const
    {
        const QString name = QString::fromLatin1(key);
        return Value{ settings_.contains(name), settings_.value(name) };
    }

    void restore(const char* key, const Value& value)
    {
        const QString name = QString::fromLatin1(key);
        if ( value.existed )
        {
            settings_.setValue(name, value.value);
        }
        else
        {
            settings_.remove(name);
        }
    }

    QSettings& settings_;
    std::array<Value, 2> values_{};
};

struct LightingScene
{
    enum class Source
    {
        RegularThing,
        Headlight,
        FlatSelf,
        ClassicSelf,
        SpriteSelf,
        SpriteAuthoredOnly,
        SectorPoint,
        SectorPointDiffuse,
        SectorAndThing,
    };

    editor::DocumentState state{};

    explicit LightingScene(
        const Source source,
        const ProjectType project = ProjectType::IJIM)
    {
        state.level.kind = project;
        state.level.header.version = 3;
        state.level.layers.emplace_back("Runtime lighting switch fixture");

        Sector sector{};
        sector.layer = 0;
        sector.ambient = source == Source::FlatSelf
            ? ColorF{ 0.8f, 0.1f, 0.2f, 0.35f }
            : source == Source::ClassicSelf
                ? ColorF{ 0.2f, 0.7f, 0.1f, 0.35f }
                : source == Source::SectorPointDiffuse
                    ? ColorF{ 0.22f, 0.47f, 0.71f, 0.35f }
                : ColorF{ 0.05f, 0.05f, 0.05f, 0.35f };
        if ( source == Source::SectorPoint
            || source == Source::SectorPointDiffuse
            || source == Source::SectorAndThing )
        {
            sector.pointLight.color = ColorF{ 0.9f, 0.5f, 0.2f, 1.0f };
            sector.pointLight.position = TVector{ 0.25, 0.25, 1.0 };
            sector.pointLight.minRange = 2.0;
            sector.pointLight.maxRange = 1.0;
        }
        state.level.sectors.push_back(std::move(sector));

        Thing target{};
        target.name = "runtime-lighting-target";
        target.templateName = target.name;
        target.layer = 0;
        target.sector = 0;
        const bool spriteTarget = source == Source::SpriteSelf
            || source == Source::SpriteAuthoredOnly;
        if ( spriteTarget )
        {
            target.x = 0.5;
            target.y = 0.5;
            target.pch = 90.0;
        }
        target.bbox.x1 = 0.0;
        target.bbox.y1 = 0.0;
        target.bbox.z1 = 0.0;
        target.bbox.x2 = 1.0;
        target.bbox.y2 = 1.0;
        target.values.push_back(makeStringValue(
            spriteTarget ? "sprite" : "model3do",
            spriteTarget
                ? "runtime_lighting_sprite"
                : "runtime_lighting_target.3do"));
        if ( source == Source::FlatSelf || source == Source::SpriteSelf )
        {
            target.values.push_back(makeStringValue(
                "light",
                source == Source::FlatSelf
                    ? "(0.4/0.2/-0.3/0.01)"
                    : "(0.7/0.2/0.1/0.01)"));
        }
        else if ( source == Source::ClassicSelf )
        {
            target.values.push_back(makeStringValue("light", "0.6"));
        }
        state.level.things.push_back(std::move(target));

        if ( source == Source::RegularThing
            || source == Source::SectorAndThing )
        {
            Thing emitter{};
            emitter.name = "runtime-lighting-regular-emitter";
            emitter.templateName = emitter.name;
            emitter.layer = 0;
            emitter.sector = 0;
            emitter.x = 0.25;
            emitter.y = 0.25;
            emitter.z = 1.0;
            emitter.values.push_back(makeStringValue(
                "light", "(2/1.5/1.2/4)"));
            state.level.things.push_back(std::move(emitter));
        }
        else if ( source == Source::Headlight )
        {
            Thing emitter{};
            emitter.name = "runtime-lighting-headlight-emitter";
            emitter.templateName = emitter.name;
            emitter.layer = 0;
            emitter.sector = 0;
            emitter.x = 0.25;
            emitter.y = 0.25;
            emitter.z = 1.0;
            emitter.values.push_back(makeStringValue("type", "actor"));
            emitter.values.push_back(makeStringValue("typeflags", "0x4"));
            emitter.values.push_back(makeStringValue(
                "lightintensity", "(2/1.5/1.2/4)"));
            emitter.values.push_back(makeStringValue(
                "lightoffset", "(0/0/0)"));
            state.level.things.push_back(std::move(emitter));
        }
    }
};

std::shared_ptr<const assets::Model3do> makeRuntimeLightingModel(
    const TLightMode lightMode = TLightMode::Gouraud)
{
    auto model = std::make_shared<assets::Model3do>();
    model->version = 2.1;

    assets::Model3doMesh mesh{};
    mesh.name = "runtime-lighting-target";
    mesh.geo = static_cast<std::int32_t>(TGeoMode::Solid);
    mesh.light = static_cast<std::int32_t>(lightMode);
    mesh.vertices = {
        assets::Model3doVertex{
            0.0, 0.0, 0.0, ColorF{ 0.0f, 0.0f, 0.0f, 1.0f } },
        assets::Model3doVertex{
            1.0, 0.0, 0.0, ColorF{ 0.0f, 0.0f, 0.0f, 1.0f } },
        assets::Model3doVertex{
            0.0, 1.0, 0.0, ColorF{ 0.0f, 0.0f, 0.0f, 1.0f } },
    };

    assets::Model3doFace face{};
    face.geo = static_cast<std::int32_t>(TGeoMode::Solid);
    face.light = static_cast<std::int32_t>(lightMode);
    face.faceFlags = FF_DoubleSided;
    face.extraLight = ColorBlack;
    face.vertices = { 0, 1, 2 };
    mesh.faces.push_back(std::move(face));
    model->meshes.push_back(std::move(mesh));
    return model;
}

constexpr auto kRuntimeLightingSpriteMaterial = "runtime_lighting_switch.mat";
constexpr auto kRuntimeLightingSpriteColormap = "runtime_lighting_switch.cmp";

bool writeRuntimeLightingSpriteFixture(const QTemporaryDir& directory)
{
    if ( !directory.isValid() )
    {
        return false;
    }

    assets::SprFile sprite{};
    sprite.material = kRuntimeLightingSpriteMaterial;
    sprite.type = 0;
    sprite.width = 1.0f;
    sprite.height = 1.0f;
    sprite.geometryMode = static_cast<int>(TGeoMode::Texture);
    sprite.lightMode = static_cast<int>(TLightMode::Gouraud);
    sprite.textureMode = 0;
    sprite.extraLightRGBA = { 0.18f, 0.06f, 0.02f, 1.0f };

    std::ofstream file(
        std::filesystem::path{ directory.path().toStdString() }
            / "runtime_lighting_sprite.spr");
    if ( !file )
    {
        return false;
    }
    assets::saveToSPRText(
        file, sprite, assets::SprProfile::ijim);
    return static_cast<bool>(file);
}

void prepareLightingScene(
    MainWindow& window,
    GlView& view,
    assets::ResourceLocator& resources,
    const LightingScene::Source source,
    const GlView::DynamicLightShading shading,
    const ProjectType project = ProjectType::IJIM,
    const TLightMode modelLightMode = TLightMode::Gouraud,
    std::optional<Thing> placementPreview = std::nullopt,
    const QVector3D& frameMin = QVector3D{ 0.0f, 0.0f, 0.0f },
    const QVector3D& frameMax = QVector3D{ 1.0f, 1.0f, 0.0f })
{
    LightingScene scene{ source, project };
    const std::string colormap = scene.state.level.getMasterCMP();
    QAction* const thingAction = window.findChild<QAction*>(
        QString::fromLatin1(kThingLightsActionName));
    QAction* const sectorAction = window.findChild<QAction*>(
        QString::fromLatin1(kSectorLightsActionName));
    if ( thingAction )
    {
        thingAction->setChecked(true);
    }
    if ( sectorAction )
    {
        sectorAction->setChecked(true);
    }

    view.setPlacementPreviewThing(std::nullopt);
    window.setDocumentStateForTest(std::move(scene.state));
    window.setProjectTypeForTest(project);

    view.setResourceLocator(&resources, false);
    if ( source == LightingScene::Source::SpriteSelf
        || source == LightingScene::Source::SpriteAuthoredOnly )
    {
        QImage spriteTexture{ 2, 2, QImage::Format_RGBA8888 };
        spriteTexture.fill(Qt::white);
        view.primeMaterialTexture(
            kRuntimeLightingSpriteMaterial,
            colormap,
            spriteTexture,
            8);
    }
    else
    {
        view.primeModel3do(
            "runtime_lighting_target.3do",
            makeRuntimeLightingModel(modelLightMode));
    }
    view.setLevel(&window.documentStateForTest().level);
    if ( placementPreview )
    {
        view.setPlacementPreviewThing(std::move(placementPreview));
    }
    view.setDynamicLightShading(shading);
    view.setLightingMode(GlView::LightingMode::Gouraud);
    view.setRenderPipeline(GlView::RenderPipeline::Editor);
    view.setThingViewMode(GlView::ThingViewMode::Shaded);
    view.setShowGrid(false);
    view.setShowWireframe(false);
    view.setShowVertices(false);
    view.setShowSolid(true);
    view.setShowTextures(false);
    view.setShowThings(true);
    view.setShowThingGizmos(false);
    view.setShowAxisWidget(false);
    view.setShowToastOverlays(false);
    view.setShowEditorCubes(false);
    view.setBackgroundClearColor(Qt::black);
    view.resetEditorTopDownView();
    view.frameWorldBounds(frameMin, frameMax, false);

    window.resize(360, 360);
    view.resize(340, 340);
    window.show();
    view.show();
    QApplication::processEvents();
    (void)view.grabFramebuffer();
}

std::optional<QColor> captureTargetColor(
    GlView& view,
    const QVector3D& worldPoint)
{
    view.repaint();
    QApplication::processEvents();
    const QImage frame = view.grabFramebuffer();
    if ( frame.isNull() || frame.width() <= 0 || frame.height() <= 0 )
    {
        return std::nullopt;
    }

    const std::optional<QPointF> widgetPoint = view.projectWorldToViewport(
        worldPoint);
    if ( !widgetPoint
        || widgetPoint->x() < 0.0
        || widgetPoint->x() >= static_cast<qreal>(view.width())
        || widgetPoint->y() < 0.0
        || widgetPoint->y() >= static_cast<qreal>(view.height()) )
    {
        return std::nullopt;
    }

    const double imageScaleX = static_cast<double>(frame.width())
        / std::max(1, view.width());
    const double imageScaleY = static_cast<double>(frame.height())
        / std::max(1, view.height());
    const int centerX = std::clamp(
        static_cast<int>(std::lround(widgetPoint->x() * imageScaleX)),
        0,
        frame.width() - 1);
    const int centerY = std::clamp(
        static_cast<int>(std::lround(widgetPoint->y() * imageScaleY)),
        0,
        frame.height() - 1);

    std::int64_t red = 0;
    std::int64_t green = 0;
    std::int64_t blue = 0;
    int count = 0;
    for ( int y = std::max(0, centerY - 2);
         y <= std::min(frame.height() - 1, centerY + 2);
         ++y )
    {
        for ( int x = std::max(0, centerX - 2);
             x <= std::min(frame.width() - 1, centerX + 2);
             ++x )
        {
            const QColor pixel = frame.pixelColor(x, y);
            red += pixel.red();
            green += pixel.green();
            blue += pixel.blue();
            ++count;
        }
    }

    return count > 0
        ? std::optional<QColor>{ QColor(
              static_cast<int>(red / count),
              static_cast<int>(green / count),
              static_cast<int>(blue / count)) }
        : std::nullopt;
}

int rgbDistance(const QColor& left, const QColor& right) noexcept
{
    return std::abs(left.red() - right.red())
        + std::abs(left.green() - right.green())
        + std::abs(left.blue() - right.blue());
}

void runSelfLightContributionGateTests()
{
    using render::lighting::RuntimeThingLight;
    using render::lighting::RuntimeThingLightSource;
    using render::lighting::ThingLightClass;

    const auto applySelfLight =
        &render::lighting::applyRuntimeThingSelfLight;
    const ColorF ijimAmbient{ 0.8f, 0.1f, 0.2f, 0.35f };
    RuntimeThingLight ijimFlat{};
    ijimFlat.source = RuntimeThingLightSource::Regular;
    ijimFlat.classification = ThingLightClass::Flat;
    ijimFlat.color = QVector3D{ 0.4f, 0.2f, -0.3f };
    const ColorF ijimEnabled = applyRuntimeSelfLightWithSwitch(
        applySelfLight, ProjectType::IJIM, ijimAmbient, ijimFlat, true);
    const ColorF ijimDisabled = applyRuntimeSelfLightWithSwitch(
        applySelfLight, ProjectType::IJIM, ijimAmbient, ijimFlat, false);
    check(exactlyEqual(ijimEnabled, ColorF{ 1.0f, 0.3f, 0.0f, 0.35f }),
        "IJIM flat owner self-light retains its exact clamped RGB and ambient alpha when enabled");
    check(exactlyEqual(ijimDisabled, ijimAmbient),
        "disabling Thing Lights returns the exact IJIM ambient for flat owner self-light");

    const ColorF classicAmbient{ 0.2f, 0.7f, 0.1f, 0.35f };
    RuntimeThingLight classicRegular{};
    classicRegular.source = RuntimeThingLightSource::Regular;
    classicRegular.classification = ThingLightClass::Dynamic;
    classicRegular.color = QVector3D{ 0.6f, 0.6f, 0.6f };
    for ( const ProjectType classicProject : {
             ProjectType::JKDF2,
             ProjectType::MOTS,
         } )
    {
        const ColorF classicEnabled = applyRuntimeSelfLightWithSwitch(
            applySelfLight,
            classicProject,
            classicAmbient,
            classicRegular,
            true);
        const ColorF classicDisabled = applyRuntimeSelfLightWithSwitch(
            applySelfLight,
            classicProject,
            classicAmbient,
            classicRegular,
            false);
        check(exactlyEqual(
                classicEnabled,
                ColorF{ 0.6f, 0.7f, 0.6f, 0.35f }),
            "classic regular owner self-light raises only the ambient floor when enabled");
        check(exactlyEqual(classicDisabled, classicAmbient),
            "disabling Thing Lights preserves exact classic owner ambient and alpha");
    }

    check(hasThingLightRenderingSetter<GlView>(),
        "GlView exposes an idempotent Thing-light contribution switch");
    check(hasSectorPointLightRenderingSetter<GlView>(),
        "GlView exposes an idempotent Sector point-light contribution switch");
}

void runLightingSwitchMenuAndSettingsTests(QSettings& settings)
{
    settings.remove(QString::fromLatin1(kThingLightsSettingsKey));
    settings.remove(QString::fromLatin1(kSectorLightsSettingsKey));
    settings.sync();

    MainWindow window{};
    window.setAttribute(Qt::WA_DontShowOnScreen);
    QMenu* const viewportMenu = window.findChild<QMenu*>(
        QStringLiteral("viewportLightingModeMenu"));
    QMenu* const viewMenu = window.findChild<QMenu*>(
        QStringLiteral("view.lighting_modes"));
    QAction* const thingAction = window.findChild<QAction*>(
        QString::fromLatin1(kThingLightsActionName));
    QAction* const sectorAction = window.findChild<QAction*>(
        QString::fromLatin1(kSectorLightsActionName));
    if ( !viewportMenu || !viewMenu || !thingAction || !sectorAction )
    {
        check(false,
            "viewport and View Lighting menus expose the independent runtime contribution actions");
        return;
    }

    const QList<QAction*> viewportActions = viewportMenu->actions();
    const QList<QAction*> viewActions = viewMenu->actions();
    const QStringList modeLabels{
        QStringLiteral("&Fully Lit"),
        QStringLiteral("&Lit"),
        QStringLiteral("&Diffuse"),
        QStringLiteral("&Gouraud"),
    };
    bool modeRowsMatch = viewportActions.size() == 7
        && viewActions.size() == 7;
    for ( qsizetype index = 0; index < modeLabels.size(); ++index )
    {
        modeRowsMatch = modeRowsMatch
            && viewportActions.at(index)->text() == modeLabels.at(index)
            && viewActions.at(index) == viewportActions.at(index)
            && viewportActions.at(index)->isCheckable()
            && viewportActions.at(index)->actionGroup()
            && viewportActions.at(index)->actionGroup()->isExclusive();
    }
    check(modeRowsMatch,
        "the four shading modes remain the shared exclusive radio group at the start of both menus");
    check(viewportActions.size() == 7 && viewportActions.at(4)->isSeparator()
            && viewActions.size() == 7 && viewActions.at(4)->isSeparator(),
        "a visible separator follows Gouraud in both Lighting menus");
    check(viewportActions.size() == 7
            && viewportActions.at(5) == thingAction
            && viewportActions.at(6) == sectorAction
            && viewActions.size() == 7
            && viewActions.at(5) == thingAction
            && viewActions.at(6) == sectorAction
            && thingAction->isCheckable()
            && sectorAction->isCheckable()
            && thingAction->actionGroup()
                != viewportActions.at(0)->actionGroup()
            && sectorAction->actionGroup()
                != viewportActions.at(0)->actionGroup(),
        "Thing and Sector contribution actions are shared, independent checkboxes outside the mode group");
    if ( viewportActions.size() <= 3 || viewActions.size() <= 3 )
    {
        check(false,
            "both Lighting menus retain all four shading modes before contribution switches");
        return;
    }

    GlView* const view = window.glViewForTest();
    check(view != nullptr && thingAction->isChecked()
            && sectorAction->isChecked(),
        "Thing and Sector point-light contributions default on");
    if ( !view )
    {
        return;
    }

    window.setProjectTypeForTest(ProjectType::IJIM);
    LightingScene warmScene{ LightingScene::Source::RegularThing };
    window.setDocumentStateForTest(std::move(warmScene.state));
    assets::ResourceLocator resources{};
    view->setResourceLocator(&resources, false);
    view->primeModel3do(
        "runtime_lighting_target.3do", makeRuntimeLightingModel());
    view->setThingViewMode(GlView::ThingViewMode::Shaded);
    view->setShowTextures(false);
    view->setLevel(&window.documentStateForTest().level);
    const bool warmThingMesh = view->thingMeshCacheValidForTest()
        && view->thingTriangleCacheVertexCountForTest() > 0u;
    check(warmThingMesh,
        "the Thing-light refresh fixture has a valid, populated rendered Thing cache");
    if ( !warmThingMesh )
    {
        return;
    }

    viewportActions.at(3)->setChecked(true);
    check(sectorAction->isEnabled() && sectorAction->isChecked(),
        "IJIM enables Sector point-light contribution without changing its default-on state");
    sectorAction->setChecked(false);
    thingAction->setChecked(false);
    settings.sync();
    const QString thingSettingsKey = QString::fromLatin1(kThingLightsSettingsKey);
    const QString sectorSettingsKey = QString::fromLatin1(kSectorLightsSettingsKey);
    const QVariant storedThingLights = settings.value(thingSettingsKey);
    const QVariant storedSectorLights = settings.value(sectorSettingsKey);
    check(settings.contains(thingSettingsKey)
            && settings.contains(sectorSettingsKey)
            && storedThingLights.metaType().id() == QMetaType::Bool
            && storedSectorLights.metaType().id() == QMetaType::Bool
            && !storedThingLights.toBool()
            && !storedSectorLights.toBool(),
        "the independent contribution choices persist in global QSettings");

    for ( const ProjectType classic : { ProjectType::JKDF2, ProjectType::MOTS } )
    {
        window.setProjectTypeForTest(classic);
        const bool viewportMenuRefreshes = QMetaObject::invokeMethod(
            viewportMenu, "aboutToShow", Qt::DirectConnection);
        const bool viewMenuRefreshes = QMetaObject::invokeMethod(
            viewMenu, "aboutToShow", Qt::DirectConnection);
        check(viewportMenuRefreshes && viewMenuRefreshes
                && sectorAction->isVisible()
                && !sectorAction->isEnabled() && !sectorAction->isChecked(),
            "classic projects keep Sector Point Lights visible but disabled with its state retained");
    }
    window.setProjectTypeForTest(ProjectType::IJIM);
    const bool viewportMenuRefreshes = QMetaObject::invokeMethod(
        viewportMenu, "aboutToShow", Qt::DirectConnection);
    const bool viewMenuRefreshes = QMetaObject::invokeMethod(
        viewMenu, "aboutToShow", Qt::DirectConnection);
    check(viewportMenuRefreshes && viewMenuRefreshes
            && sectorAction->isEnabled() && !sectorAction->isChecked(),
        "returning to IJIM re-enables Sector Point Lights without losing its disabled-project state");
    check(viewportActions.at(3)->isChecked(),
        "toggling contribution switches leaves the Gouraud lighting mode action state independent");

    MainWindow settingsReloadWindow{};
    settingsReloadWindow.setAttribute(Qt::WA_DontShowOnScreen);
    QAction* const reloadedThingAction = settingsReloadWindow.findChild<QAction*>(
        QString::fromLatin1(kThingLightsActionName));
    QAction* const reloadedSectorAction = settingsReloadWindow.findChild<QAction*>(
        QString::fromLatin1(kSectorLightsActionName));
    check(reloadedThingAction && reloadedSectorAction
            && !reloadedThingAction->isChecked()
            && !reloadedSectorAction->isChecked(),
        "a fresh workspace restores both global runtime contribution choices");

    const std::uint64_t thingRebuildCount = view->meshRebuildCountForTest();
    thingAction->setChecked(true);
    const std::uint64_t thingOnCount = view->meshRebuildCountForTest();
    thingAction->setChecked(false);
    const std::uint64_t thingOffCount = view->meshRebuildCountForTest();
    check(thingOnCount == thingRebuildCount + 1
            && thingOffCount == thingOnCount + 1
            && !view->lastMeshRebuildWasFullForTest(),
        "changing Thing-light contributions performs one ThingsOnly refresh per change");

    const std::uint64_t equalThingCount = view->meshRebuildCountForTest();
    setThingLightRenderingIfAvailable(*view, false);
    check(view->meshRebuildCountForTest() == equalThingCount,
        "setting Thing-light contribution to its current value is an idempotent no-op");

    const std::uint64_t sectorBefore = view->meshRebuildCountForTest();
    sectorAction->setChecked(true);
    sectorAction->setChecked(false);
    const std::uint64_t sectorAfter = view->meshRebuildCountForTest();
    setSectorPointLightRenderingIfAvailable(*view, false);
    check(sectorAfter == sectorBefore
            && view->meshRebuildCountForTest() == sectorAfter,
        "Sector point-light contribution updates uniforms without rebuilding level or Thing meshes");
}

void runRuntimeLightingEditRoundTripTests(QSettings& settings)
{
    settings.setValue(QString::fromLatin1(kThingLightsSettingsKey), false);
    settings.setValue(QString::fromLatin1(kSectorLightsSettingsKey), false);
    settings.sync();

    MainWindow sourceWindow{};
    sourceWindow.setAttribute(Qt::WA_DontShowOnScreen);
    sourceWindow.setProjectTypeForTest(ProjectType::IJIM);
    QAction* const sourceThingAction = sourceWindow.findChild<QAction*>(
        QString::fromLatin1(kThingLightsActionName));
    QAction* const sourceSectorAction = sourceWindow.findChild<QAction*>(
        QString::fromLatin1(kSectorLightsActionName));
    if ( !sourceThingAction || !sourceSectorAction )
    {
        check(false,
            "runtime contribution actions are available for the named EDIT state tests");
        return;
    }
    sourceThingAction->setChecked(false);
    sourceSectorAction->setChecked(false);

    const QByteArray capturedPayload =
        sourceWindow.captureRecoveryEditorStateForTest();
    QCborParserError parseError{};
    const QCborValue capturedValue = QCborValue::fromCbor(
        capturedPayload, &parseError);
    const QCborMap capturedRoot = capturedValue.toMap();
    const QCborMap capturedFlags = capturedRoot.value(
        QStringLiteral("runtimeLighting")).toMap();
    check(parseError.error == QCborError::NoError
            && capturedValue.isMap()
            && capturedFlags.value(QStringLiteral("thingLights")).isBool()
            && !capturedFlags.value(QStringLiteral("thingLights")).toBool()
            && capturedFlags.value(QStringLiteral("sectorPointLights")).isBool()
            && !capturedFlags.value(QStringLiteral("sectorPointLights")).toBool(),
        "EDIT stores both contribution switches by name in one optional root map");

    settings.setValue(QString::fromLatin1(kThingLightsSettingsKey), true);
    settings.setValue(QString::fromLatin1(kSectorLightsSettingsKey), true);
    settings.sync();
    MainWindow restoredWindow{};
    restoredWindow.setAttribute(Qt::WA_DontShowOnScreen);
    restoredWindow.setProjectTypeForTest(ProjectType::IJIM);
    QString restoreWarning{};
    restoreWarning = QStringLiteral("stale warning");
    const bool presentFlagsRestored =
        restoredWindow.restoreRecoveryEditorStateForTest(
            capturedPayload, &restoreWarning);
    QAction* const restoredThingAction = restoredWindow.findChild<QAction*>(
        QString::fromLatin1(kThingLightsActionName));
    QAction* const restoredSectorAction = restoredWindow.findChild<QAction*>(
        QString::fromLatin1(kSectorLightsActionName));
    check(presentFlagsRestored && restoredThingAction && restoredSectorAction
            && !restoredThingAction->isChecked()
            && !restoredSectorAction->isChecked(),
        "present named EDIT contribution values override the current global defaults");
    check(restoreWarning.isEmpty(),
        "successful EDIT restore clears a stale output warning");

    QCborMap unknownRoot = capturedRoot;
    QCborMap unknownFlags = capturedFlags;
    unknownFlags.insert(QStringLiteral("futureThingContribution"), true);
    unknownRoot.insert(QStringLiteral("runtimeLighting"), unknownFlags);
    const QByteArray unknownPayload = ui::recovery::serializeCanonicalCbor(
        QCborValue(unknownRoot));
    QString unknownWarning{ QStringLiteral("stale warning") };
    const bool unknownAccepted = restoredWindow.validateRecoveryEditorStateForTest(
        unknownPayload, &unknownWarning);
    if ( unknownAccepted )
    {
        (void)restoredWindow.restoreRecoveryEditorStateForTest(unknownPayload);
    }
    const QCborMap unknownResavedRoot = QCborValue::fromCbor(
        restoredWindow.captureRecoveryEditorStateForTest()).toMap();
    check(unknownAccepted,
        "unknown future runtime-lighting flags are accepted by EDIT validation");
    check(unknownWarning.isEmpty(),
        "successful EDIT validation clears a stale output warning");
    check(!unknownResavedRoot.value(QStringLiteral("runtimeLighting"))
                .toMap().contains(QStringLiteral("futureThingContribution")),
        "unknown future EDIT flags are accepted and dropped by the current writer");

    QCborMap partialRoot = capturedRoot;
    QCborMap partialFlags = capturedFlags;
    partialFlags.remove(QStringLiteral("sectorPointLights"));
    partialRoot.insert(QStringLiteral("runtimeLighting"), partialFlags);
    const QByteArray partialPayload = ui::recovery::serializeCanonicalCbor(
        QCborValue(partialRoot));
    MainWindow partialFlagsWindow{};
    partialFlagsWindow.setAttribute(Qt::WA_DontShowOnScreen);
    partialFlagsWindow.setProjectTypeForTest(ProjectType::IJIM);
    QString partialWarning{};
    const bool partialFlagsRestored =
        partialFlagsWindow.restoreRecoveryEditorStateForTest(
            partialPayload, &partialWarning);
    QAction* const partialThingAction = partialFlagsWindow.findChild<QAction*>(
        QString::fromLatin1(kThingLightsActionName));
    QAction* const partialSectorAction = partialFlagsWindow.findChild<QAction*>(
        QString::fromLatin1(kSectorLightsActionName));
    check(partialFlagsRestored && partialWarning.isEmpty()
            && partialThingAction && !partialThingAction->isChecked()
            && partialSectorAction && partialSectorAction->isChecked(),
        "an absent named EDIT contribution flag defaults on while a present flag is restored");

    QCborMap missingRoot = capturedRoot;
    missingRoot.remove(QStringLiteral("runtimeLighting"));
    const QByteArray missingPayload = ui::recovery::serializeCanonicalCbor(
        QCborValue(missingRoot));
    settings.remove(QString::fromLatin1(kThingLightsSettingsKey));
    settings.remove(QString::fromLatin1(kSectorLightsSettingsKey));
    settings.sync();
    MainWindow missingFlagsWindow{};
    missingFlagsWindow.setAttribute(Qt::WA_DontShowOnScreen);
    missingFlagsWindow.setProjectTypeForTest(ProjectType::IJIM);
    QString missingWarning{};
    const bool missingFlagsRestored =
        missingFlagsWindow.restoreRecoveryEditorStateForTest(
            missingPayload, &missingWarning);
    QAction* const missingThingAction = missingFlagsWindow.findChild<QAction*>(
        QString::fromLatin1(kThingLightsActionName));
    QAction* const missingSectorAction = missingFlagsWindow.findChild<QAction*>(
        QString::fromLatin1(kSectorLightsActionName));
    check(missingFlagsRestored && missingThingAction && missingSectorAction
            && missingThingAction->isChecked()
            && missingSectorAction->isChecked(),
        "an older EDIT record without the named map uses default-on contribution values");

    const QByteArray beforeMalformedRestore =
        restoredWindow.captureRecoveryEditorStateForTest();
    const QString expectedMalformedWarning = QStringLiteral(
        "The recovered runtime lighting state is invalid; the level was still recovered.");
    const auto checkMalformedRuntimeLighting = [&] (
        QCborMap malformedRoot,
        const QString& description)
    {
        const QByteArray malformedPayload = ui::recovery::serializeCanonicalCbor(
            QCborValue(malformedRoot));
        QString malformedWarning{ QStringLiteral("stale warning") };
        const bool malformedRestored =
            restoredWindow.restoreRecoveryEditorStateForTest(
                malformedPayload, &malformedWarning);
        check(!malformedRestored
                && malformedWarning == expectedMalformedWarning
                && restoredWindow.captureRecoveryEditorStateForTest()
                    == beforeMalformedRestore,
            description.toUtf8().constData());
    };

    QCborMap malformedMapRoot = capturedRoot;
    malformedMapRoot.insert(
        QStringLiteral("runtimeLighting"), QCborArray{ false, true });
    checkMalformedRuntimeLighting(
        std::move(malformedMapRoot),
        QStringLiteral("a runtimeLighting value with the wrong CBOR type is rejected without changing the workspace"));

    QCborMap malformedThingRoot = capturedRoot;
    QCborMap malformedThingFlags = capturedFlags;
    malformedThingFlags.insert(QStringLiteral("thingLights"),
        QStringLiteral("off"));
    malformedThingRoot.insert(
        QStringLiteral("runtimeLighting"), malformedThingFlags);
    checkMalformedRuntimeLighting(
        std::move(malformedThingRoot),
        QStringLiteral("a Thing-light flag with the wrong CBOR type reports the fixed warning without changing the workspace"));

    QCborMap malformedSectorRoot = capturedRoot;
    QCborMap malformedSectorFlags = capturedFlags;
    malformedSectorFlags.insert(QStringLiteral("sectorPointLights"), 0);
    malformedSectorRoot.insert(
        QStringLiteral("runtimeLighting"), malformedSectorFlags);
    checkMalformedRuntimeLighting(
        std::move(malformedSectorRoot),
        QStringLiteral("a Sector-light flag with the wrong CBOR type reports the fixed warning without changing the workspace"));
}

void runRuntimeLightingFramebufferTests()
{
    QSettings settings = settingsStore();
    ScopedSettingsRestore restoreSettings{ settings };
    QTemporaryDir spriteDirectory{};
    assets::ResourceLocator resources{};
    const bool spriteFixtureReady =
        writeRuntimeLightingSpriteFixture(spriteDirectory);
    check(spriteFixtureReady,
        "temporary SPR fixture is available for contribution-switch framebuffer checks");
    if ( spriteFixtureReady )
    {
        resources.addRoot(
            std::filesystem::path{ spriteDirectory.path().toStdString() });
    }

    MainWindow window{};
    window.setAttribute(Qt::WA_DontShowOnScreen);
    GlView* const view = window.glViewForTest();
    QAction* const thingAction = window.findChild<QAction*>(
        QString::fromLatin1(kThingLightsActionName));
    QAction* const sectorAction = window.findChild<QAction*>(
        QString::fromLatin1(kSectorLightsActionName));
    if ( !view || !thingAction || !sectorAction )
    {
        check(false,
            "runtime contribution framebuffer fixture exposes its viewport actions");
        return;
    }

    window.resize(360, 360);
    view->resize(340, 340);
    const auto capture = [view](const QVector3D& point)
    {
        return captureTargetColor(*view, point);
    };
    for ( const GlView::DynamicLightShading shading : {
             GlView::DynamicLightShading::Gouraud,
             GlView::DynamicLightShading::Phong,
         } )
    {
        for ( const LightingScene::Source source : {
                 LightingScene::Source::RegularThing,
                 LightingScene::Source::Headlight,
                 LightingScene::Source::FlatSelf,
                 LightingScene::Source::SectorPoint,
             } )
        {
            const bool isSector = source == LightingScene::Source::SectorPoint;
            prepareLightingScene(
                window,
                *view,
                resources,
                source,
                shading);
            const QVector3D samplePoint{ 0.2f, 0.2f, 0.0f };
            const std::optional<QColor> enabled = capture(samplePoint);
            if ( isSector )
            {
                sectorAction->setChecked(false);
            }
            else
            {
                thingAction->setChecked(false);
            }
            const std::optional<QColor> disabled = capture(samplePoint);
            if ( isSector )
            {
                sectorAction->setChecked(true);
            }
            else
            {
                thingAction->setChecked(true);
            }
            const std::optional<QColor> enabledAgain = capture(samplePoint);
            const bool changed = enabled && disabled
                && rgbDistance(*enabled, *disabled) > 36;
            check(changed,
                isSector
                    ? "IJIM Sector point-light gate changes synthetic Gouraud/Phong 3DO pixels"
                    : "Thing-light gate changes synthetic Gouraud/Phong regular, Headlight, and self-light pixels");
            check(enabled && enabledAgain
                    && rgbDistance(*enabled, *enabledAgain) <= 6,
                "re-enabling a runtime contribution restores the same warmed-scene 3DO pixels");

            if ( isSector )
            {
                check(view->dynamicLightUniformBufferReadyForTest(),
                    "turning Sector point-light contribution off leaves the Thing-light UBO available");
            }
        }
    }

    for ( const GlView::DynamicLightShading shading : {
             GlView::DynamicLightShading::Gouraud,
             GlView::DynamicLightShading::Phong,
         } )
    {
        prepareLightingScene(
            window,
            *view,
            resources,
            LightingScene::Source::SectorPointDiffuse,
            shading,
            ProjectType::IJIM,
            TLightMode::Diffuse);
        const QVector3D samplePoint{ 0.2f, 0.2f, 0.0f };
        const std::optional<QColor> enabled = capture(samplePoint);
        sectorAction->setChecked(false);
        const std::optional<QColor> disabled = capture(samplePoint);
        sectorAction->setChecked(true);
        const std::optional<QColor> enabledAgain = capture(samplePoint);
        const QColor expectedAmbientLinear{ 56, 120, 181 };
        const QColor expectedAmbientSrgb{ 129, 182, 219 };
        const auto matchesDiffuseAmbient = [&](const std::optional<QColor>& pixel)
        {
            return pixel
                && pixel->red() > 20
                && (rgbDistance(*pixel, expectedAmbientLinear) <= 24
                    || rgbDistance(*pixel, expectedAmbientSrgb) <= 24);
        };
        check(enabled && disabled && enabledAgain
                && rgbDistance(*enabled, *disabled) <= 6
                && rgbDistance(*enabled, *enabledAgain) <= 6
                && matchesDiffuseAmbient(enabled)
                && matchesDiffuseAmbient(disabled)
                && matchesDiffuseAmbient(enabledAgain),
            "IJIM Sector point light leaves actual Diffuse 3DO pixels at their fixed nonblack ambient color under Gouraud or Phong sampling");
    }

    prepareLightingScene(
        window,
        *view,
        resources,
        LightingScene::Source::SectorAndThing,
        GlView::DynamicLightShading::Phong);
    thingAction->setChecked(false);
    const QVector3D combinedLightPoint{ 0.2f, 0.2f, 0.0f };
    const std::optional<QColor> sectorWithThingsOff =
        capture(combinedLightPoint);
    sectorAction->setChecked(false);
    const std::optional<QColor> sectorDisabledWithThingsOff =
        capture(combinedLightPoint);
    sectorAction->setChecked(true);
    const std::optional<QColor> sectorRestoredWithThingsOff =
        capture(combinedLightPoint);
    check(view->dynamicLightUniformBufferReadyForTest(),
        "Phong keeps the dynamic-light UBO available when Thing Lights are off and Sector Point Lights remain on");
    check(sectorWithThingsOff && sectorDisabledWithThingsOff
            && sectorRestoredWithThingsOff
            && rgbDistance(*sectorWithThingsOff, *sectorDisabledWithThingsOff)
                > 36
            && rgbDistance(*sectorWithThingsOff, *sectorRestoredWithThingsOff)
                <= 6,
        "Sector point-light contribution remains active independently of the Thing collector in Phong");

    for ( const ProjectType classicProject : {
             ProjectType::JKDF2,
             ProjectType::MOTS,
         } )
    {
        for ( const GlView::DynamicLightShading shading : {
                 GlView::DynamicLightShading::Gouraud,
                 GlView::DynamicLightShading::Phong,
             } )
        {
            prepareLightingScene(
                window,
                *view,
                resources,
                LightingScene::Source::ClassicSelf,
                shading,
                classicProject);
            const QVector3D samplePoint{ 0.25f, 0.25f, 0.0f };
            const std::optional<QColor> enabled = capture(samplePoint);
            thingAction->setChecked(false);
            const std::optional<QColor> disabled = capture(samplePoint);
            thingAction->setChecked(true);
            const std::optional<QColor> enabledAgain = capture(samplePoint);
            check(enabled && disabled
                    && rgbDistance(*enabled, *disabled) > 36,
                "classic JKDF2 and MotS owner self-light changes synthetic Gouraud/Phong 3DO pixels");
            check(enabled && enabledAgain
                    && rgbDistance(*enabled, *enabledAgain) <= 6,
                "classic Thing Lights on/off/on restores the same warmed-scene owner pixels");
        }
    }

    if ( spriteFixtureReady )
    {
        prepareLightingScene(
            window,
            *view,
            resources,
            LightingScene::Source::SpriteSelf,
            GlView::DynamicLightShading::Gouraud);
        const QVector3D spritePoint{ 0.5f, 0.5f, 0.0f };
        const std::optional<QColor> spriteEnabled = capture(spritePoint);
        thingAction->setChecked(false);
        const std::optional<QColor> spriteDisabled = capture(spritePoint);
        thingAction->setChecked(true);
        const std::optional<QColor> spriteEnabledAgain = capture(spritePoint);
        check(spriteEnabled && spriteDisabled && spriteEnabledAgain
                && rgbDistance(*spriteEnabled, *spriteDisabled) > 36
                && rgbDistance(*spriteEnabled, *spriteEnabledAgain) <= 6,
            "Thing Lights gates persistent SPR owner self-light and restores the same warmed-scene pixels");

        prepareLightingScene(
            window,
            *view,
            resources,
            LightingScene::Source::SpriteAuthoredOnly,
            GlView::DynamicLightShading::Gouraud);
        const std::optional<QColor> authoredOn = capture(spritePoint);
        thingAction->setChecked(false);
        const std::optional<QColor> authoredOff = capture(spritePoint);
        check(authoredOn && authoredOff
                && rgbDistance(*authoredOn, *authoredOff) <= 6
                && authoredOff->red() > 20,
            "Thing Lights leaves authored SPR Extra Light visible and unchanged when the runtime self source is absent");

        Thing placementPreview{};
        placementPreview.name = "runtime-lighting-sprite-preview";
        placementPreview.layer = 0;
        placementPreview.sector = 0;
        placementPreview.x = 2.5;
        placementPreview.y = 0.5;
        placementPreview.pch = 90.0;
        placementPreview.values.push_back(makeStringValue(
            "sprite", "runtime_lighting_sprite"));
        placementPreview.values.push_back(makeStringValue(
            "light", "(0.7/0.2/0.1/0.01)"));
        prepareLightingScene(
            window,
            *view,
            resources,
            LightingScene::Source::SpriteSelf,
            GlView::DynamicLightShading::Gouraud,
            ProjectType::IJIM,
            TLightMode::Gouraud,
            placementPreview,
            QVector3D{ -0.5f, 0.0f, 0.0f },
            QVector3D{ 3.5f, 1.0f, 0.0f });
        const QVector3D previewPoint{ 2.5f, 0.5f, 0.0f };
        const std::optional<QColor> previewEnabled = capture(previewPoint);
        thingAction->setChecked(false);
        const std::optional<QColor> previewDisabled = capture(previewPoint);
        thingAction->setChecked(true);
        const std::optional<QColor> previewEnabledAgain = capture(previewPoint);
        check(previewEnabled && previewDisabled && previewEnabledAgain
                && rgbDistance(*previewEnabled, *previewDisabled) > 36
                && rgbDistance(*previewEnabled, *previewEnabledAgain) <= 6,
            "Thing Lights gates placement-preview SPR owner self-light and restores its warmed-scene pixels");

        const std::uint64_t rebuildsBeforePreviewMove =
            view->meshRebuildCountForTest();
        const std::uint64_t fastUpdatesBeforePreviewMove =
            view->placementPreviewFastUpdateCountForTest();
        placementPreview.x = 2.6;
        view->setPlacementPreviewThing(placementPreview);
        check(view->meshRebuildCountForTest() == rebuildsBeforePreviewMove
                && view->placementPreviewFastUpdateCountForTest()
                    == fastUpdatesBeforePreviewMove + 1u,
            "SPR placement still uses its fast transform update after contribution toggles");
        const std::optional<QColor> movedPreview =
            capture(QVector3D{ 2.6f, 0.5f, 0.0f });
        check(previewEnabledAgain && movedPreview
                && rgbDistance(*previewEnabledAgain, *movedPreview) <= 6,
            "fast-moving the placement SPR preserves its restored owner self-light contribution");
    }

    window.hide();
}

} // namespace

void runRuntimeLightingSwitchTests()
{
    runSelfLightContributionGateTests();

    QSettings settings = settingsStore();
    ScopedSettingsRestore restoreSettings{ settings };
    runLightingSwitchMenuAndSettingsTests(settings);
    runRuntimeLightingEditRoundTripTests(settings);
    runRuntimeLightingFramebufferTests();
}

} // namespace sed::app_tests
