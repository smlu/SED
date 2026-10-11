#include "test_common.hpp"
#include "app_settings.hpp"
#include "gl_view.hpp"
#include "main_window/main_window.hpp"
#include "render/viewport_color_theme.hpp"
#include "render/grid/viewport_grid_utils.hpp"
#include "ui/uv/uv_island_geometry_utils.hpp"
#include "ui/cog/cog_script_theme_utils.hpp"
#include "ui/theme_file_storage.hpp"

#include <QApplication>
#include <QAction>
#include <QComboBox>
#include <QColorDialog>
#include <QCborArray>
#include <QCborMap>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QGroupBox>
#include <QInputDialog>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QFileInfo>
#include <QDir>
#include <QMap>
#include <QMessageBox>
#include <QPushButton>
#include <QSettings>
#include <QCborValue>
#include <QTimer>
#include <QTemporaryDir>
#include <QToolButton>
#include <QStringList>
#include <QWheelEvent>

#include <algorithm>
#include <filesystem>
#include <numbers>
#include <functional>

namespace sed::app_tests
{
namespace
{
void clickPreferencesDialogButton(
    QDialog* dialog,
    QDialogButtonBox::StandardButton buttonType)
{
    auto* buttonBox = dialog
        ? dialog->findChild<QDialogButtonBox*>()
        : nullptr;
    QAbstractButton* button = buttonBox
        ? buttonBox->button(buttonType)
        : nullptr;
    check(button != nullptr,
        buttonType == QDialogButtonBox::Ok
            ? "Preferences exposes its OK button to the test"
            : "Preferences exposes its Cancel button to the test");
    if (button)
    {
        button->click();
    }
}

void processUvZoomEvents()
{
    QApplication::processEvents(QEventLoop::AllEvents);
}

bool drivePreferencesDialog(
    MainWindow& window,
    const std::function<void(QDialog*)>& interact)
{
    QAction* const preferencesAction = window.findChild<QAction*>(
        QStringLiteral("edit.preferences"));
    check(preferencesAction != nullptr,
        "viewport theme regression finds the production Preferences action");
    if (!preferencesAction)
    {
        return false;
    }
    bool handled = false;
    int attempts = 0;
    QTimer modalWarningWatcher;
    modalWarningWatcher.setInterval(10);
    QObject::connect(&modalWarningWatcher, &QTimer::timeout, &window, [&]()
    {
        if (auto* warning = qobject_cast<QMessageBox*>(
                QApplication::activeModalWidget()))
        {
            std::cerr << "Unexpected modal warning during Preferences workflow: "
                << warning->windowTitle().toStdString() << ": "
                << warning->text().toStdString() << '\n';
            check(false,
                "Preferences workflow showed an unexpected warning; the test dismisses it to continue");
            warning->accept();
        }
    });
    QTimer driver;
    driver.setSingleShot(true);
    QObject::connect(&driver, &QTimer::timeout, &window, [&]()
    {
        if (auto* warning = qobject_cast<QMessageBox*>(
                QApplication::activeModalWidget()))
        {
            std::cerr << "Unexpected modal warning while opening Preferences: "
                << warning->windowTitle().toStdString() << ": "
                << warning->text().toStdString() << '\n';
            check(false,
                "Preferences opened an unexpected warning dialog; the test dismisses it to continue");
            warning->accept();
            driver.start(10);
            return;
        }
        QDialog* dialog = qobject_cast<QDialog*>(
            QApplication::activeModalWidget());
        if (!dialog || dialog->windowTitle() != QStringLiteral("Preferences"))
        {
            dialog = nullptr;
            for (QDialog* candidate : window.findChildren<QDialog*>())
            {
                if (candidate->windowTitle() == QStringLiteral("Preferences"))
                {
                    dialog = candidate;
                    break;
                }
            }
        }
        if (dialog)
        {
            handled = true;
            interact(dialog);
            return;
        }
        if (++attempts < 100)
        {
            driver.start(10);
            return;
        }

        check(false,
            "Preferences dialog becomes available to the viewport theme regression");
        if (auto* const activeDialog = qobject_cast<QDialog*>(
                QApplication::activeModalWidget()))
        {
            clickPreferencesDialogButton(activeDialog, QDialogButtonBox::Cancel);
        }
    });
    modalWarningWatcher.start();
    driver.start(0);
    preferencesAction->trigger();
    driver.stop();
    modalWarningWatcher.stop();
    return handled;
}


bool chooseThemeColor(
    QDialog* preferences,
    const QString& objectName,
    const QColor& color)
{
    auto* swatch = preferences
        ? preferences->findChild<QToolButton*>(objectName)
        : nullptr;
    if (!swatch)
    {
        return false;
    }

    bool handled = false;
    int attempts = 0;
    QTimer driver;
    driver.setSingleShot(true);
    QObject::connect(&driver, &QTimer::timeout, preferences, [&]()
    {
        QColorDialog* picker = qobject_cast<QColorDialog*>(
            QApplication::activeModalWidget());
        if (!picker)
        {
            picker = preferences->findChild<QColorDialog*>();
        }
        if (picker)
        {
            picker->setCurrentColor(color);
            picker->accept();
            handled = true;
            return;
        }
        if (++attempts < 100)
        {
            driver.start(10);
        }
    });
    driver.start(0);
    swatch->click();
    driver.stop();
    return handled;
}

QJsonObject readSavedThemeObject(
    const QSettings& settings,
    const sed::AppThemeKind kind,
    const QString& id)
{
    const std::filesystem::path directory =
        sed::ui::theme_storage::directoryForSettings(settings, kind);
    const auto format = kind == sed::AppThemeKind::viewport
        ? sed::ui::theme_storage::ThemeFormat::viewport
        : sed::ui::theme_storage::ThemeFormat::cog;
    const std::vector<sed::ui::theme_storage::ThemeDocument> documents =
        sed::ui::theme_storage::readThemeDocuments(directory, format);
    const auto found = std::find_if(documents.begin(), documents.end(),
        [&id](const sed::ui::theme_storage::ThemeDocument& document)
        { return document.id.compare(id, Qt::CaseInsensitive) == 0; });
    return found != documents.end() ? found->object : QJsonObject{};
}

QString readSavedThemeValue(
    const QSettings& settings,
    const sed::AppThemeKind kind,
    const QString& id,
    const QString& key)
{
    const QJsonObject object = readSavedThemeObject(settings, kind, id);
    if (key == QStringLiteral("id") || key == QStringLiteral("name"))
    {
        return object.value(key).toString();
    }
    return object.value(QStringLiteral("colors")).toObject()
        .value(key).toString();
}

bool submitThemeName(
    QDialog* preferences,
    QPushButton* saveButton,
    const QString& name)
{
    if (!preferences || !saveButton)
    {
        return false;
    }

    bool handled = false;
    int attempts = 0;
    QTimer driver;
    driver.setSingleShot(true);
    QTimer unexpectedModalWatcher;
    unexpectedModalWatcher.setInterval(10);
    QObject::connect(&unexpectedModalWatcher, &QTimer::timeout,
        preferences, [&]()
    {
        if (auto* warning = qobject_cast<QMessageBox*>(
                QApplication::activeModalWidget()))
        {
            std::cerr << "Unexpected modal after accepting theme name: "
                << warning->windowTitle().toStdString() << ": "
                << warning->text().toStdString() << '\n';
            check(false,
                "Theme creation showed an unexpected warning dialog");
            warning->accept();
            unexpectedModalWatcher.stop();
        }
    });
    QTimer modalWatchdog;
    modalWatchdog.setSingleShot(true);
    QObject::connect(&modalWatchdog, &QTimer::timeout, preferences, [&]()
    {
        QWidget* const active = QApplication::activeModalWidget();
        if (!active || active == preferences)
        {
            return;
        }
        std::cerr << "Theme name action left a modal open: "
            << active->metaObject()->className() << " title='"
            << active->windowTitle().toStdString() << "'\n";
        check(false, "Theme name action completed without leaving another modal open");
        if (auto* dialog = qobject_cast<QDialog*>(active))
        {
            dialog->reject();
        }
        else
        {
            active->close();
        }
    });
    QObject::connect(&driver, &QTimer::timeout, preferences, [&]()
    {
        QInputDialog* input = qobject_cast<QInputDialog*>(
            QApplication::activeModalWidget());
        if (!input)
        {
            input = preferences->findChild<QInputDialog*>();
        }
        if (input)
        {
            const bool viewportDialog = input->objectName()
                == QStringLiteral("pref_viewport_theme_name_dialog");
            const bool cogDialog = input->objectName()
                == QStringLiteral("pref_cog_theme_name_dialog");
            const bool expectedDialog = viewportDialog || cogDialog;
            const bool titleMatches = input->windowTitle() == (viewportDialog
                ? QStringLiteral("Add Viewport Theme")
                : QStringLiteral("Add COG Theme"));
            const bool promptMatches = input->labelText()
                == QStringLiteral("Name for the copied theme");
            const bool widthMatches = input->width() >= 620;
            const bool minMaxButtonsPresent = input->windowFlags().testFlag(
                Qt::WindowMinMaxButtonsHint);
            if (!expectedDialog || !titleMatches || !promptMatches
                || !widthMatches || minMaxButtonsPresent)
            {
                std::cerr << "Add Theme dialog diagnostic: object="
                    << input->objectName().toStdString()
                    << " title=" << input->windowTitle().toStdString()
                    << " prompt=" << input->labelText().toStdString()
                    << " minWidth=" << input->minimumWidth()
                    << " currentSize=" << input->width() << 'x' << input->height()
                    << " minSize=" << input->minimumSize().width() << 'x'
                    << input->minimumSize().height()
                    << " flags=" << static_cast<int>(input->windowFlags())
                    << " minMaxPresent=" << minMaxButtonsPresent << '\n';
            }
            check(expectedDialog && titleMatches && promptMatches
                    && widthMatches && !minMaxButtonsPresent,
                "Add Theme dialogs have clear copy labels, usable width, and no minimize or maximize controls");
            input->setTextValue(name);
            // Theme creation may report a save/preflight problem after the
            // nested input dialog closes. Keep a test-only watcher active so
            // that such a modal is reported and dismissed instead of hanging
            // the focused UI test indefinitely.
            unexpectedModalWatcher.start();
            modalWatchdog.start(200);
            input->accept();
            handled = true;
            return;
        }
        if (++attempts < 100)
        {
            driver.start(10);
        }
    });
    driver.start(0);
    saveButton->click();
    driver.stop();

    // A save error can be queued by the Add Theme slot and appear only after
    // QInputDialog::exec() returns. Keep the diagnostic watcher alive through
    // one bounded event-loop turn instead of stopping it as soon as click()
    // returns.
    QEventLoop modalGraceLoop;
    QTimer modalGraceTimer;
    modalGraceTimer.setSingleShot(true);
    QObject::connect(&modalGraceTimer, &QTimer::timeout,
        &modalGraceLoop, &QEventLoop::quit);
    modalGraceTimer.start(250);
    modalGraceLoop.exec();

    unexpectedModalWatcher.stop();
    modalWatchdog.stop();
    return handled;
}
} // namespace

void runUvZoomPreferenceTests()
{
    QTemporaryDir currentThemeDirectory;
    check(currentThemeDirectory.isValid(),
        "Current viewport theme settings use an isolated directory");
    if (currentThemeDirectory.isValid())
    {
        QSettings currentThemeSettings(
            currentThemeDirectory.filePath(QStringLiteral("settings.ini")),
            QSettings::IniFormat);
        const render::ViewportColorThemeSelection defaultTheme =
            render::resolveViewportColorTheme(currentThemeSettings);
        check(defaultTheme.id == QStringLiteral("night_blue")
                && render::sameViewportColorPalette(
                    defaultTheme.palette,
                    render::nightBlueViewportColorPalette()),
            "A fresh current-format profile selects the built-in Midnight Blue theme");
    }

    QSettings settings = settingsStore();
#ifdef SED_APP_MAIN_WINDOW_TEST_ACCESS
    check(appSettingsIniPathIsOverriddenForTest(),
        "Theme UI integration tests run with an isolated application settings path");
#endif
    const bool hadPreviousMaximum = settings.contains(QStringLiteral("uv_editor/max_zoom"));
    const QVariant previousMaximum = settings.value(QStringLiteral("uv_editor/max_zoom"));
    struct StoredSetting
    {
        QString key{};
        bool present{};
        QVariant value{};
    };
    QStringList viewportThemeKeys{
        QString::fromLatin1(render::kViewportColorThemeSetting),
        QStringLiteral("viewport/msaa_samples"),
        QStringLiteral("uv_editor/colors/canvas_background"),
        QStringLiteral("uv_editor/colors/grid_main"),
        QStringLiteral("uv_editor/colors/grid_sub"),
        QStringLiteral("uv_editor/colors/grid_axis_u"),
        QStringLiteral("uv_editor/colors/grid_axis_v")};
    for (const auto& [key, member] : render::kViewportColorPaletteFields)
    {
        Q_UNUSED(member);
        viewportThemeKeys.push_back(
            QStringLiteral("viewport/colors/") + QString::fromLatin1(key));
    }
    const auto viewportThemeDirectoriesForTest =
        sed::ui::theme_storage::directoriesForSettings(
            settings, sed::AppThemeKind::viewport);
    const auto cogThemeDirectoriesForTest =
        sed::ui::theme_storage::directoriesForSettings(
            settings, sed::AppThemeKind::cog);
    // This executable redirects app settings to a QTemporaryDir. Clear only
    // its associated test theme folders so repeated runs begin deterministically.
    for (const std::filesystem::path& path : viewportThemeDirectoriesForTest)
    {
        (void)QDir(QString::fromStdWString(path.wstring())).removeRecursively();
    }
    for (const std::filesystem::path& path : cogThemeDirectoriesForTest)
    {
        (void)QDir(QString::fromStdWString(path.wstring())).removeRecursively();
    }
    std::vector<StoredSetting> previousCogThemeSettings{};
    QStringList cogKeys{
        QStringLiteral("cog_editor/theme"),
    };
    for (const QString& key : settings.allKeys())
    {
        if (key.startsWith(QStringLiteral("cog_editor/color_")))
        {
            cogKeys.push_back(key);
        }
    }
    cogKeys.removeDuplicates();
    for (const QString& key : cogKeys)
    {
        if (!settings.contains(key))
        {
            continue;
        }
        previousCogThemeSettings.push_back({key, true, settings.value(key)});
        settings.remove(key);
    }
    std::vector<StoredSetting> previousThemeSettings{};
    previousThemeSettings.reserve(static_cast<std::size_t>(viewportThemeKeys.size()));
    for (const QString& key : viewportThemeKeys)
    {
        previousThemeSettings.push_back({
            key, settings.contains(key), settings.value(key)});
        settings.remove(key);
    }
    const auto restoreMaximum = [&]()
    {
        settings.remove(QStringLiteral("uv_editor/max_zoom"));
        if (hadPreviousMaximum)
        {
            settings.setValue(QStringLiteral("uv_editor/max_zoom"), previousMaximum);
        }
        for (const StoredSetting& stored : previousThemeSettings)
        {
            settings.remove(stored.key);
            if (stored.present)
            {
                settings.setValue(stored.key, stored.value);
            }
        }
        for (const QString& key : cogKeys)
        {
            settings.remove(key);
        }
        for (const StoredSetting& stored : previousCogThemeSettings)
        {
            settings.setValue(stored.key, stored.value);
        }
        for (const std::filesystem::path& path : viewportThemeDirectoriesForTest)
        {
            (void)QDir(QString::fromStdWString(path.wstring())).removeRecursively();
        }
        for (const std::filesystem::path& path : cogThemeDirectoriesForTest)
        {
            (void)QDir(QString::fromStdWString(path.wstring())).removeRecursively();
        }
        settings.sync();
    };
    settings.remove(QStringLiteral("uv_editor/max_zoom"));
    settings.sync();

    MainWindow window;
    window.setAttribute(Qt::WA_DontShowOnScreen);
    window.resize(960, 720);

    editor::DocumentState state{};
    state.level.kind = ProjectType::JKDF2;
    state.level.layers.emplace_back("Layer0");
    state.level.uvCoordinateSpace = UvCoordinateSpace::Normalized;
    Sector& sector = state.level.newSector();
    sector.layer = 0;
    sector.vertices = {
        Vertex{0.0, 0.0, 0.0, 0},
        Vertex{1.0, 0.0, 0.0, 1},
        Vertex{1.0, 1.0, 0.0, 2},
        Vertex{0.0, 1.0, 0.0, 3},
    };
    Surface& surface = sector.surfaces.emplace_back();
    surface.vertices = {0, 1, 2, 3};
    surface.normal = TVector{0.0, 0.0, 1.0};
    surface.texVertices = {
        TexVertex{0.0f, 0.0f, ColorWhite},
        TexVertex{1.0f, 0.0f, ColorWhite},
        TexVertex{1.0f, 1.0f, ColorWhite},
        TexVertex{0.0f, 1.0f, ColorWhite},
    };
    state.selection.surfaces = {SurfaceRef{0, 0}};

    window.setDocumentStateForTest(std::move(state));
    window.activateSurfaceEditModeForTest();
    window.mutableDocumentStateForTest().selection.surfaces = {
        SurfaceRef{0, 0}};
    window.updateEditActionsForTest();
    window.show();
    processUvZoomEvents();

    QWidget* canvas = window.findChild<QWidget*>(
        QStringLiteral("uvEditorCanvas"));
    check(canvas && canvas->isVisible(),
        "UV zoom regression opens the production canvas");
    if (!canvas)
    {
        restoreMaximum();
        return;
    }

    const double initialZoom = canvas->property("sed.uvZoom").toDouble();
    check(initialZoom >= 8.0 && initialZoom <= 480.0,
        "UV canvas starts inside the ordinary default/minimum zoom policy");

    const QPointF wheelPosition(canvas->width() * 0.73, canvas->height() * 0.31);
    const auto viewZoom = [canvas]()
    {
        return canvas->property("sed.uvZoom").toDouble();
    };
    const auto viewPan = [canvas]()
    {
        return QPointF(
            canvas->property("sed.uvPanX").toDouble(),
            canvas->property("sed.uvPanY").toDouble());
    };
    const auto viewMirror = [canvas]()
    {
        return canvas->property("sed.uvViewMirrorX").toBool();
    };
    const auto viewRotation = [canvas]()
    {
        return canvas->property("sed.uvViewRotationDegrees").toDouble()
            * (std::numbers::pi_v<qreal> / 180.0);
    };
    const QPointF anchorUv = ui::uv::uvViewUvFromScreen(
        wheelPosition, viewPan(), viewZoom(), viewMirror(), viewRotation());
    for (int step = 0; step < 64; ++step)
    {
        QWheelEvent wheel(
            wheelPosition,
            canvas->mapToGlobal(wheelPosition.toPoint()),
            QPoint{},
            QPoint{0, 120},
            Qt::NoButton,
            Qt::NoModifier,
            Qt::NoScrollPhase,
            false);
        QApplication::sendEvent(canvas, &wheel);
    }
    processUvZoomEvents();

    const double zoom = canvas->property("sed.uvZoom").toDouble();
    check(zoom == 48'000.0,
        "UV canvas wheel zoom reaches the requested 48000 maximum for tiny islands");
    const QPointF anchoredScreen = ui::uv::uvViewScreenFromUv(
        anchorUv, viewPan(), viewZoom(), viewMirror(), viewRotation());
    check(std::abs(anchoredScreen.x() - wheelPosition.x()) < 1.0e-7
            && std::abs(anchoredScreen.y() - wheelPosition.y()) < 1.0e-7,
        "UV wheel zoom keeps the UV point beneath an off-center cursor");
    check(canvas->property("sed.uvPanX").isValid()
            && canvas->property("sed.uvPanY").isValid(),
        "high UV zoom keeps the live pan state available for cursor anchoring");

    settings.setValue(QStringLiteral("uv_editor/max_zoom"), QStringLiteral("not-a-number"));
    const QByteArray highZoomRecovery = window.captureRecoveryEditorStateForTest();
    window.applyUvEditorMaximumZoomForTest();
    check(canvas->property("sed.uvZoom").toDouble() == 48'000.0,
        "malformed UV maximum setting falls back to 48000");
    settings.setValue(QStringLiteral("uv_editor/max_zoom"),
        std::numeric_limits<double>::infinity());
    window.applyUvEditorMaximumZoomForTest();
    check(canvas->property("sed.uvZoom").toDouble() == 48'000.0,
        "nonfinite UV maximum setting falls back to 48000");

    const QPointF center(canvas->width() * 0.5, canvas->height() * 0.5);
    const QPointF centerUv = ui::uv::uvViewUvFromScreen(
        center,
        viewPan(),
        viewZoom(),
        viewMirror(),
        viewRotation());
    settings.setValue(QStringLiteral("uv_editor/max_zoom"), 480);
    window.applyUvEditorMaximumZoomForTest();
    check(canvas->property("sed.uvZoom").toDouble() == 480.0,
        "lowering UV maximum immediately clamps an open canvas");
    const QPointF centerAfter = ui::uv::uvViewScreenFromUv(
        centerUv,
        viewPan(),
        viewZoom(),
        viewMirror(),
        viewRotation());
    check(std::abs(centerAfter.x() - center.x()) < 1.0e-7
            && std::abs(centerAfter.y() - center.y()) < 1.0e-7,
        "lowering UV maximum preserves the canvas center anchor");
    settings.setValue(QStringLiteral("uv_editor/max_zoom"), 1'000'000);
    window.applyUvEditorMaximumZoomForTest();
    check(canvas->property("sed.uvZoom").toDouble() == 480.0,
        "raising UV maximum leaves the current canvas zoom unchanged");

    for (int step = 0; step < 128; ++step)
    {
        QWheelEvent wheel(
            wheelPosition,
            canvas->mapToGlobal(wheelPosition.toPoint()),
            QPoint{},
            QPoint{0, 120},
            Qt::NoButton,
            Qt::NoModifier,
            Qt::NoScrollPhase,
            false);
        QApplication::sendEvent(canvas, &wheel);
    }
    processUvZoomEvents();
    check(canvas->property("sed.uvZoom").toDouble() == 480'000.0,
        "UV canvas wheel zoom clamps an above-range preference to 480000");

    check(window.restoreRecoveryEditorStateForTest(highZoomRecovery),
        "UV recovery accepts a workspace captured below the current maximum");
    check(canvas->property("sed.uvZoom").toDouble() == 48'000.0,
        "UV recovery restores saved zoom below the current maximum");

    settings.setValue(QStringLiteral("uv_editor/max_zoom"), -1);
    window.applyUvEditorMaximumZoomForTest();
    check(canvas->property("sed.uvZoom").toDouble() == 480.0,
        "below-range UV maximum setting clamps to the 480 minimum");
    check(window.restoreRecoveryEditorStateForTest(highZoomRecovery),
        "UV recovery accepts a workspace above the lowered maximum");
    check(canvas->property("sed.uvZoom").toDouble() == 480.0,
        "UV recovery clamps saved zoom above the lowered maximum");
    const QCborMap restoredWorkspace = QCborValue::fromCbor(
        window.captureUvEditorWorkspaceStateForTest()).toMap();
    check(restoredWorkspace.value(QStringLiteral("active")).toArray()
            == QCborArray{0, 0}
        && window.documentStateForTest().selection.surfaces.size() == 1,
        "UV recovery preserves the active and selected island while clamping zoom");

    GlView* const view = window.glViewForTest();
    const auto findSwatchColor = [](QDialog* dialog, const char* name)
    {
        const auto* swatch = dialog->findChild<QToolButton*>(
            QString::fromLatin1(name));
        if (!swatch)
        {
            return QString{};
        }
        const QColor color = swatch->property("sed.colorValue").value<QColor>();
        return color.isValid() ? color.name(QColor::HexRgb) : QString{};
    };
    const bool defaultThemeDialog = drivePreferencesDialog(
        window,
        [&](QDialog* dialog)
        {
            auto* theme = dialog->findChild<QComboBox*>(
                QStringLiteral("pref_viewport_color_theme"));
            const int midnightBlueIndex = theme
                ? theme->findData(QStringLiteral("night_blue")) : -1;
            const int jedIndex = theme
                ? theme->findData(QStringLiteral("original_sed")) : -1;
            const int customIndex = theme
                ? theme->findData(QStringLiteral("custom")) : -1;
            auto* addNewTheme = dialog->findChild<QPushButton*>(
                QStringLiteral("pref_viewport_theme_add_new"));
            auto* appearanceSection = dialog->findChild<QWidget*>(
                QStringLiteral("pref_viewport_appearance_section"));
            auto* appearanceHeader = appearanceSection
                ? appearanceSection->findChild<QToolButton*>(
                    QStringLiteral("pref_section_header"),
                    Qt::FindDirectChildrenOnly)
                : nullptr;
            check(theme && midnightBlueIndex >= 0 && jedIndex >= 0
                    && customIndex < 0 && addNewTheme
                    && theme->itemText(midnightBlueIndex)
                        == QStringLiteral("Midnight Blue")
                    && theme->itemText(jedIndex) == QStringLiteral("JED")
                    && theme->currentData().toString()
                        == QStringLiteral("night_blue")
                    && appearanceSection
                    && appearanceSection->property(
                        "sed.preferencesDisclosurePath").toString()
                        == QStringLiteral("viewport/viewport-colors")
                    && appearanceSection->parentWidget()
                    && appearanceSection->parentWidget()->property(
                        "sed.preferencesDisclosurePath").toString()
                        == QStringLiteral("viewport")
                    && appearanceHeader
                    && appearanceHeader->text() == QStringLiteral("Appearance"),
                "Appearance preferences expose immutable built-ins and Add New without a visible Custom entry");

            auto* background = dialog->findChild<QToolButton*>(
                QStringLiteral("pref_viewport_bg_clear"));
            auto* wireWidth = dialog->findChild<QDoubleSpinBox*>(
                QStringLiteral("pref_viewport_wire_line_width"));
            auto* msaaSamples = dialog->findChild<QComboBox*>(
                QStringLiteral("pref_viewport_msaa_samples"));
            auto* themeGroup = theme
                ? qobject_cast<QGroupBox*>(theme->parentWidget()) : nullptr;
            auto* appearanceGroup = wireWidth
                ? qobject_cast<QGroupBox*>(wireWidth->parentWidget()) : nullptr;
            check(themeGroup && themeGroup->title() == QStringLiteral("Theme")
                    && background && background->parentWidget() == themeGroup
                    && appearanceGroup
                    && appearanceGroup->title() == QStringLiteral("Appearance")
                    && wireWidth->parentWidget() == appearanceGroup,
                "Theme colors and noncolor viewport appearance controls use separate groups");

            constexpr std::array<int, 5> expectedMsaaSamples{ 0, 2, 4, 8, 16 };
            bool msaaChoicesValid = msaaSamples
                && msaaSamples->count()
                    == static_cast<int>(expectedMsaaSamples.size());
            if (msaaChoicesValid)
            {
                for (std::size_t index = 0;
                     index < expectedMsaaSamples.size(); ++index)
                {
                    msaaChoicesValid = msaaChoicesValid
                        && msaaSamples->itemData(static_cast<int>(index)).toInt()
                            == expectedMsaaSamples[index];
                }
            }
            check(msaaChoicesValid
                    && msaaSamples->currentData().toInt() == 4
                    && msaaSamples->toolTip().contains(
                        QStringLiteral("Restart SED"), Qt::CaseInsensitive),
                "Viewport Appearance offers validated 0/2/4/8/16 MSAA requests, defaults to 4, and explains restart");

            if (msaaSamples)
            {
                msaaSamples->setCurrentIndex(msaaSamples->findData(8));
            }

            if (theme)
            {
                theme->setCurrentIndex(theme->findData(
                    QStringLiteral("original_sed")));
            }
            auto* updateBuiltIn = dialog->findChild<QPushButton*>(
                QStringLiteral("pref_viewport_theme_update"));
            auto* deleteBuiltIn = dialog->findChild<QPushButton*>(
                QStringLiteral("pref_viewport_theme_delete"));
            check(updateBuiltIn && !updateBuiltIn->isEnabled()
                    && deleteBuiltIn && !deleteBuiltIn->isEnabled(),
                "Built-in palettes cannot be updated or deleted");
            const std::array<std::pair<const char*, const char*>, 13>
                expectedJedSwatches{{
                    { "pref_viewport_bg_clear", "#000000" },
                    { "pref_viewport_wireframe", "#ffffff" },
                    { "pref_viewport_grid_color", "#ff00ff" },
                    { "pref_viewport_grid_axis_x_color", "#ffff00" },
                    { "pref_viewport_grid_axis_y_color", "#0000ff" },
                    { "pref_viewport_sel_primary", "#ff0000" },
                    { "pref_viewport_sel_multi", "#00ffff" },
                    { "pref_viewport_sel_edge_multi", "#ff7f00" },
                    { "pref_uv_canvas_background_color", "#000000" },
                    { "pref_uv_grid_main_color", "#ff00ff" },
                    { "pref_uv_grid_sub_color", "#7f007f" },
                    { "pref_uv_grid_axis_u_color", "#ffff00" },
                    { "pref_uv_grid_axis_v_color", "#0000ff" },
                }};
            const bool jedSwatchesMatch = std::all_of(
                expectedJedSwatches.begin(), expectedJedSwatches.end(),
                [&](const auto& item)
                {
                    return findSwatchColor(dialog, item.first)
                        == QLatin1String(item.second);
                });
            if (!jedSwatchesMatch)
            {
                std::cerr << "JED palette diagnostic: selectedTheme='"
                    << (theme ? theme->currentData().toString().toStdString()
                              : std::string{ "<missing>" })
                    << "' currentText='"
                    << (theme ? theme->currentText().toStdString()
                              : std::string{ "<missing>" })
                    << "' swatches=";
                for (const auto& item : expectedJedSwatches)
                {
                    std::cerr << item.first << '='
                        << findSwatchColor(dialog, item.first).toStdString()
                        << ',';
                }
                std::cerr << '\n';
            }
            check(findSwatchColor(dialog, "pref_viewport_bg_clear")
                    == QStringLiteral("#000000")
                    && findSwatchColor(dialog, "pref_viewport_wireframe")
                        == QStringLiteral("#ffffff")
                    && findSwatchColor(dialog, "pref_viewport_grid_color")
                        == QStringLiteral("#ff00ff")
                    && findSwatchColor(dialog, "pref_viewport_grid_axis_x_color")
                        == QStringLiteral("#ffff00")
                    && findSwatchColor(dialog, "pref_viewport_grid_axis_y_color")
                        == QStringLiteral("#0000ff")
                    && findSwatchColor(dialog, "pref_viewport_sel_primary")
                        == QStringLiteral("#ff0000")
                    && findSwatchColor(dialog, "pref_viewport_sel_multi")
                        == QStringLiteral("#00ffff")
                    && findSwatchColor(dialog, "pref_viewport_sel_edge_multi")
                        == QStringLiteral("#ff7f00")
                    && findSwatchColor(dialog, "pref_uv_canvas_background_color")
                        == QStringLiteral("#000000")
                    && findSwatchColor(dialog, "pref_uv_grid_main_color")
                        == QStringLiteral("#ff00ff")
                    && findSwatchColor(dialog, "pref_uv_grid_sub_color")
                        == QStringLiteral("#7f007f")
                    && findSwatchColor(dialog, "pref_uv_grid_axis_u_color")
                        == QStringLiteral("#ffff00")
                    && findSwatchColor(dialog, "pref_uv_grid_axis_v_color")
                        == QStringLiteral("#0000ff"),
                "JED applies its viewport and UV-canvas colors as one palette");
            clickPreferencesDialogButton(dialog, QDialogButtonBox::Ok);
        });
    check(defaultThemeDialog,
        "Production Preferences dialog completes the built-in theme interaction");

    const std::filesystem::path viewportThemeDirectory =
        sed::ui::theme_storage::directoryForSettings(
            settings, sed::AppThemeKind::viewport);
    const auto savedSettingsBeforeAddCancel = [&settings]()
    {
        QMap<QString, QVariant> snapshot{};
        for (const QString& key : settings.allKeys())
        {
            snapshot.insert(key, settings.value(key));
        }
        return snapshot;
    }();
    const bool cancelledViewportAddDialog = drivePreferencesDialog(
        window,
        [&](QDialog* dialog)
        {
            auto* theme = dialog->findChild<QComboBox*>(
                QStringLiteral("pref_viewport_color_theme"));
            auto* addNew = dialog->findChild<QPushButton*>(
                QStringLiteral("pref_viewport_theme_add_new"));
            if (theme)
            {
                theme->setCurrentIndex(theme->findData(
                    QStringLiteral("night_blue")));
            }
            check(theme && theme->currentData().toString()
                    == QStringLiteral("night_blue") && addNew,
                "Add New can duplicate a selected immutable built-in theme");
            check(submitThemeName(
                    dialog, addNew, QStringLiteral("Cancelled Copy")),
                "Add New accepts a name before Preferences is cancelled");
            check(theme && theme->currentData().toString()
                    == QStringLiteral("custom_cancelled_copy"),
                "Add New selects the uniquely named duplicate in the dialog");
            clickPreferencesDialogButton(dialog, QDialogButtonBox::Cancel);
        });
    check(cancelledViewportAddDialog,
        "Preferences can close after immediately saving a viewport theme");
    settings.sync();
    check(savedSettingsBeforeAddCancel == [&settings]()
            {
                QMap<QString, QVariant> snapshot{};
                for (const QString& key : settings.allKeys())
                {
                    snapshot.insert(key, settings.value(key));
                }
                return snapshot;
            }()
            && QFileInfo::exists(sed::ui::theme_storage::themeFilePathForLabel(
                viewportThemeDirectory, QStringLiteral("Cancelled Copy"),
                sed::ui::theme_storage::ThemeFormat::viewport))
            && !QFileInfo::exists(QDir(QString::fromStdWString(
                viewportThemeDirectory.wstring())).filePath(QStringLiteral(".manifest"))),
        "Cancel leaves active settings unchanged while the explicit Add New file action remains committed");
    (void)QFile::remove(sed::ui::theme_storage::themeFilePathForLabel(
        viewportThemeDirectory, QStringLiteral("Cancelled Copy"),
        sed::ui::theme_storage::ThemeFormat::viewport));

    settings.sync();
    check(settings.value(QStringLiteral("viewport/msaa_samples")).toInt() == 8,
        "Accepting Preferences persists the selected MSAA request for the next application start");

    settings.setValue(QStringLiteral("viewport/msaa_samples"), 3);
    settings.sync();
    const bool invalidMsaaSamplesDefaultDialog = drivePreferencesDialog(
        window,
        [&](QDialog* dialog)
        {
            auto* msaaSamples = dialog->findChild<QComboBox*>(
                QStringLiteral("pref_viewport_msaa_samples"));
            check(msaaSamples && msaaSamples->currentData().toInt() == 4,
                "Unsupported persisted MSAA requests display the safe default");
            clickPreferencesDialogButton(dialog, QDialogButtonBox::Cancel);
        });
    check(invalidMsaaSamplesDefaultDialog,
        "Preferences can open with an unsupported stored MSAA request");
    settings.sync();
    check(settings.value(QStringLiteral("viewport/msaa_samples")).toInt() == 3,
        "Cancel leaves an unsupported stored MSAA request unchanged");
    const bool msaaOffApplyDialog = drivePreferencesDialog(
        window,
        [&](QDialog* dialog)
        {
            auto* msaaSamples = dialog->findChild<QComboBox*>(
                QStringLiteral("pref_viewport_msaa_samples"));
            check(msaaSamples && msaaSamples->findData(0) >= 0,
                "MSAA Off remains an explicit persisted choice");
            if (msaaSamples)
            {
                msaaSamples->setCurrentIndex(msaaSamples->findData(0));
            }
            clickPreferencesDialogButton(dialog, QDialogButtonBox::Ok);
        });
    check(msaaOffApplyDialog,
        "Preferences applies MSAA Off");
    settings.sync();
    bool msaaOffStoredAsInteger = false;
    const int msaaOffStoredValue = settings.value(
        QStringLiteral("viewport/msaa_samples")).toInt(&msaaOffStoredAsInteger);
    check(msaaOffStoredAsInteger && msaaOffStoredValue == 0,
        "Applying MSAA Off stores numeric zero instead of falling back to four samples");
    const bool msaaOffReopenDialog = drivePreferencesDialog(
        window,
        [&](QDialog* dialog)
        {
            auto* msaaSamples = dialog->findChild<QComboBox*>(
                QStringLiteral("pref_viewport_msaa_samples"));
            check(msaaSamples && msaaSamples->currentData().toInt() == 0,
                "Reopening Preferences keeps the numeric MSAA Off request");
            clickPreferencesDialogButton(dialog, QDialogButtonBox::Ok);
        });
    check(msaaOffReopenDialog,
        "Preferences reopens with MSAA Off selected");

    settings.setValue(QStringLiteral("viewport/msaa_samples"), 8);
    settings.sync();

    check(view && view->backgroundClearColor() == QColor(0, 0, 0)
            && view->wireframeColor() == QColor(255, 255, 255)
            && view->gridColor() == QColor(255, 0, 255)
            && view->gridMajorColor() == QColor(255, 0, 255)
            && view->gridAxisXColor() == QColor(255, 255, 0)
            && view->gridAxisYColor() == QColor(0, 0, 255)
            && view->selectionPrimaryColor() == QColor(255, 0, 0)
            && view->selectionMultiColor() == QColor(0, 255, 255)
            && view->selectionEdgeMultiColor() == QColor(255, 127, 0)
            && canvas->property("sed.uvCanvasBackgroundColor").value<QColor>()
                == QColor(0, 0, 0)
            && canvas->property("sed.uvGridMainColor").value<QColor>()
                == QColor(255, 0, 255)
            && canvas->property("sed.uvGridSubColor").value<QColor>()
                == QColor(127, 0, 127)
            && canvas->property("sed.uvGridAxisUColor").value<QColor>()
                == QColor(255, 255, 0)
            && canvas->property("sed.uvGridAxisVColor").value<QColor>()
                == QColor(0, 0, 255)
            && render::gridAxisColorForDirection(
                QVector3D(1.0f, 0.0f, 0.0f),
                view->gridAxisXColor(), view->gridAxisYColor(), view->gridAxisZColor())
                == QColor(255, 255, 0)
            && render::gridAxisColorForDirection(
                QVector3D(0.0f, 1.0f, 0.0f),
                view->gridAxisXColor(), view->gridAxisYColor(), view->gridAxisZColor())
                == QColor(0, 0, 255),
        "Accepting Original SED applies preset colors through the live viewport and axis renderer");
    check(settings.value(QStringLiteral("viewport/color_theme")).toString()
                == QStringLiteral("original_sed")
            && settings.value(QStringLiteral("viewport/colors/grid_axis_x")).toString()
                == QStringLiteral("#ffff00")
            && settings.value(QStringLiteral("viewport/colors/grid_axis_y")).toString()
                == QStringLiteral("#0000ff")
            && settings.value(QStringLiteral(
                    "viewport/colors/uv_canvas_background")).toString()
                == QStringLiteral("#000000")
            && settings.value(QStringLiteral(
                    "uv_editor/colors/grid_main")).toString()
                == QStringLiteral("#ff00ff")
            && settings.value(QStringLiteral(
                    "uv_editor/colors/grid_axis_u")).toString()
                == QStringLiteral("#ffff00"),
        "JED theme id and full viewport/UV palette persist in QSettings");

    const auto captureSettingsSnapshot = [&settings]()
    {
        QMap<QString, QVariant> snapshot;
        for (const QString& key : settings.allKeys())
        {
            snapshot.insert(key, settings.value(key));
        }
        return snapshot;
    };
    const QMap<QString, QVariant> settingsBeforeNoOpApply =
        captureSettingsSnapshot();
    const auto viewportFilesBeforeNoOpApply = [&viewportThemeDirectory]()
    {
        QMap<QString, QByteArray> snapshot{};
        const QDir directory(QString::fromStdWString(
            viewportThemeDirectory.wstring()));
        for (const QFileInfo& entry : directory.entryInfoList(
                 QDir::Files | QDir::NoDotAndDotDot, QDir::Name))
        {
            QFile file(entry.filePath());
            if (file.open(QIODevice::ReadOnly))
            {
                snapshot.insert(entry.fileName(), file.readAll());
            }
        }
        return snapshot;
    }();
    const bool unchangedPreferencesDialog = drivePreferencesDialog(
        window,
        [](QDialog* dialog)
        {
            clickPreferencesDialogButton(dialog, QDialogButtonBox::Ok);
        });
    check(unchangedPreferencesDialog,
        "Unchanged Preferences can be accepted with current-format themes");
    settings.sync();
    const auto viewportFilesAfterNoOpApply = [&viewportThemeDirectory]()
    {
        QMap<QString, QByteArray> snapshot{};
        const QDir directory(QString::fromStdWString(
            viewportThemeDirectory.wstring()));
        for (const QFileInfo& entry : directory.entryInfoList(
                 QDir::Files | QDir::NoDotAndDotDot, QDir::Name))
        {
            QFile file(entry.filePath());
            if (file.open(QIODevice::ReadOnly))
            {
                snapshot.insert(entry.fileName(), file.readAll());
            }
        }
        return snapshot;
    }();
    check(captureSettingsSnapshot() == settingsBeforeNoOpApply
            && viewportFilesAfterNoOpApply == viewportFilesBeforeNoOpApply,
        "Unchanged Preferences OK performs no settings or theme-file writes");

    render::ViewportColorPalette customPalette =
        render::nightBlueViewportColorPalette();
    customPalette.backgroundClear = QColor("#123456");
    customPalette.gridAxisX = QColor("#234567");
    customPalette.selectionPrimary = QColor("#345678");
    render::saveViewportColorPalette(
        settings, QStringLiteral("viewport/colors"), customPalette);
    const QString sourcePaletteThemeId = QStringLiteral("custom_palette_source");
    std::vector<render::ViewportColorTheme> viewportThemes =
        render::loadCustomViewportColorThemes(settings);
    viewportThemes.erase(std::remove_if(
        viewportThemes.begin(), viewportThemes.end(),
        [&sourcePaletteThemeId](const render::ViewportColorTheme& theme)
        { return theme.id == sourcePaletteThemeId; }), viewportThemes.end());
    viewportThemes.push_back({
        sourcePaletteThemeId,
        QStringLiteral("Palette Source"),
        customPalette,
    });
    QString customThemeSaveError{};
    check(render::saveCustomViewportColorThemes(
              settings, viewportThemes, &customThemeSaveError)
            && customThemeSaveError.isEmpty(),
        "A current-format viewport theme fixture is saved as a named file");
    settings.setValue(QStringLiteral("viewport/color_theme"),
        sourcePaletteThemeId);
    settings.sync();

    const bool customSnapshotDialog = drivePreferencesDialog(
        window,
        [&](QDialog* dialog)
        {
            auto* theme = dialog->findChild<QComboBox*>(
                QStringLiteral("pref_viewport_color_theme"));
            check(theme && theme->currentData().toString()
                    == sourcePaletteThemeId
                    && theme->findData(QStringLiteral("custom")) < 0
                    && theme->currentText() == QStringLiteral("Palette Source")
                    && findSwatchColor(dialog, "pref_viewport_bg_clear")
                        == QStringLiteral("#123456")
                    && findSwatchColor(dialog, "pref_viewport_grid_axis_x_color")
                        == QStringLiteral("#234567")
                    && findSwatchColor(dialog, "pref_viewport_sel_primary")
                        == QStringLiteral("#345678"),
                "A named theme restores its complete saved palette");
            if (theme)
            {
                theme->setCurrentIndex(theme->findData(
                    QStringLiteral("original_sed")));
                check(findSwatchColor(dialog, "pref_viewport_bg_clear")
                        == QStringLiteral("#000000"),
                    "Switching to Original SED changes active colors without replacing Custom");
            }
            clickPreferencesDialogButton(dialog, QDialogButtonBox::Ok);
        });
    check(customSnapshotDialog,
        "Production Preferences opens a current-format named user theme");
    check(view && view->backgroundClearColor() == QColor(0, 0, 0)
            && view->gridAxisXColor() == QColor(255, 255, 0)
            && settings.value(QStringLiteral("viewport/color_theme")).toString()
                == QStringLiteral("original_sed")
            && readSavedThemeValue(settings, sed::AppThemeKind::viewport,
                sourcePaletteThemeId, QStringLiteral("background_clear"))
                == QStringLiteral("#123456"),
        "Selecting a built-in theme leaves the separately saved user theme intact");

    const bool swatchEditDialog = drivePreferencesDialog(
        window,
        [&](QDialog* dialog)
        {
            auto* theme = dialog->findChild<QComboBox*>(
                QStringLiteral("pref_viewport_color_theme"));
            auto* background = dialog->findChild<QToolButton*>(
                QStringLiteral("pref_viewport_bg_clear"));
            check(theme && background
                    && theme->currentData().toString()
                        == QStringLiteral("original_sed")
                    && findSwatchColor(dialog, "pref_viewport_bg_clear")
                        == QStringLiteral("#000000"),
                "Production Preferences reopens the accepted Original SED theme");
            if (theme)
            {
                theme->setCurrentIndex(theme->findData(sourcePaletteThemeId));
            }
            check(theme && theme->currentData().toString()
                    == sourcePaletteThemeId
                    && findSwatchColor(dialog, "pref_viewport_bg_clear")
                        == QStringLiteral("#123456")
                    && findSwatchColor(dialog, "pref_viewport_grid_axis_x_color")
                        == QStringLiteral("#234567"),
                "Reopening Preferences restores the saved user theme after a preset switch");
            bool colorDialogHandled = false;
            int colorDialogAttempts = 0;
            QTimer colorDialogDriver;
            colorDialogDriver.setSingleShot(true);
            QObject::connect(
                &colorDialogDriver,
                &QTimer::timeout,
                dialog,
                [&]()
                {
                    QColorDialog* picker = qobject_cast<QColorDialog*>(
                        QApplication::activeModalWidget());
                    if (!picker)
                    {
                        picker = dialog->findChild<QColorDialog*>();
                    }
                    if (picker)
                    {
                        picker->setCurrentColor(QColor("#fedcba"));
                        picker->accept();
                        colorDialogHandled = true;
                        return;
                    }
                    if (++colorDialogAttempts < 100)
                    {
                        colorDialogDriver.start(10);
                        return;
                    }
                    if (auto* const activeDialog = qobject_cast<QDialog*>(
                            QApplication::activeModalWidget()))
                    {
                        if (activeDialog != dialog)
                        {
                            clickPreferencesDialogButton(activeDialog, QDialogButtonBox::Cancel);
                        }
                    }
                });
            if (background)
            {
                colorDialogDriver.start(0);
                background->click();
                colorDialogDriver.stop();
            }
            check(colorDialogHandled,
                "Viewport color swatch opens a bounded color chooser for editing");
            check(theme && theme->currentData().toString()
                    == sourcePaletteThemeId
                    && findSwatchColor(dialog, "pref_viewport_bg_clear")
                        == QStringLiteral("#fedcba"),
                "Editing a named user theme stages its palette without changing its identity");
            auto* updateButton = dialog->findChild<QPushButton*>(
                QStringLiteral("pref_viewport_theme_update"));
            if (updateButton)
            {
                updateButton->click();
            }
            clickPreferencesDialogButton(dialog, QDialogButtonBox::Ok);
        });
    check(swatchEditDialog,
        "Production Preferences dialog completes a color swatch edit");
    const QJsonObject updatedSourceTheme = readSavedThemeObject(
        settings, sed::AppThemeKind::viewport, sourcePaletteThemeId);
    check(view && view->backgroundClearColor() == QColor("#fedcba")
            && settings.value(QStringLiteral("viewport/color_theme")).toString()
                == sourcePaletteThemeId
            && updatedSourceTheme.value(QStringLiteral("name")).toString()
                == QStringLiteral("Palette Source")
            && updatedSourceTheme.value(QStringLiteral("colors")).toObject()
                .value(QStringLiteral("background_clear")).toString()
                == QStringLiteral("#fedcba"),
        "Update immediately persists the complete named palette");

    const QString namedThemeId = QStringLiteral("custom_twilight_workbench");
    const auto storedNamedThemeValue = [&settings](
        const QString& id, const QString& key)
    {
        return readSavedThemeValue(
            settings, sed::AppThemeKind::viewport, id, key);
    };

    const bool saveNamedThemeDialog = drivePreferencesDialog(
        window,
        [&](QDialog* dialog)
        {
            auto* theme = dialog->findChild<QComboBox*>(
                QStringLiteral("pref_viewport_color_theme"));
            auto* saveButton = dialog->findChild<QPushButton*>(
                QStringLiteral("pref_viewport_theme_add_new"));
            check(theme && theme->currentData().toString()
                    == sourcePaletteThemeId,
                "Add New starts from the selected named user palette");
            check(chooseThemeColor(
                    dialog,
                    QStringLiteral("pref_uv_canvas_background_color"),
                    QColor("#112233"))
                    && chooseThemeColor(
                        dialog,
                        QStringLiteral("pref_uv_grid_main_color"),
                        QColor("#445566")),
                "Viewport palette can edit UV background and main-grid colors");
            check(submitThemeName(
                    dialog, saveButton,
                    QStringLiteral("Twilight Workbench")),
                "Viewport Add New accepts a user-provided name");
            check(theme && theme->currentData().toString() == namedThemeId
                    && theme->currentText()
                        == QStringLiteral("Twilight Workbench")
                    && findSwatchColor(
                        dialog, "pref_uv_canvas_background_color")
                        == QStringLiteral("#112233")
                    && findSwatchColor(dialog, "pref_uv_grid_main_color")
                        == QStringLiteral("#445566"),
                "Add New duplicates and selects a named UV-aware viewport theme");
            check(readSavedThemeValue(settings, sed::AppThemeKind::viewport,
                        namedThemeId, QStringLiteral("name"))
                    == QStringLiteral("Twilight Workbench"),
                "Viewport Add New writes its file before Preferences is accepted");
            clickPreferencesDialogButton(dialog, QDialogButtonBox::Ok);
        });
    check(saveNamedThemeDialog,
        "Preferences completes Add New viewport theme creation");
    settings.sync();
    check(storedNamedThemeValue(namedThemeId, QStringLiteral("name"))
                == QStringLiteral("Twilight Workbench")
            && storedNamedThemeValue(
                namedThemeId, QStringLiteral("uv_canvas_background"))
                == QStringLiteral("#112233")
            && storedNamedThemeValue(
                namedThemeId, QStringLiteral("uv_grid_main"))
                == QStringLiteral("#445566")
            && readSavedThemeValue(settings, sed::AppThemeKind::viewport,
                    sourcePaletteThemeId, QStringLiteral("name"))
                == QStringLiteral("Palette Source")
            && readSavedThemeValue(settings, sed::AppThemeKind::viewport,
                    sourcePaletteThemeId, QStringLiteral("background_clear"))
                == QStringLiteral("#fedcba"),
        "Add New writes a separate file and leaves its source theme unchanged");

    const bool cancelledNamedThemeDraftDialog = drivePreferencesDialog(
        window,
        [&](QDialog* dialog)
        {
            auto* theme = dialog->findChild<QComboBox*>(
                QStringLiteral("pref_viewport_color_theme"));
            auto* updateButton = dialog->findChild<QPushButton*>(
                QStringLiteral("pref_viewport_theme_update"));
            check(theme && theme->currentData().toString() == namedThemeId
                    && updateButton && updateButton->isEnabled(),
                "A saved viewport theme can be edited as a staged draft");
            check(chooseThemeColor(
                    dialog,
                    QStringLiteral("pref_uv_canvas_background_color"),
                    QColor("#010203")),
                "A named palette swatch creates a draft before Update Theme");
            clickPreferencesDialogButton(dialog, QDialogButtonBox::Cancel);
        });
    check(cancelledNamedThemeDraftDialog,
        "Preferences can cancel an uncommitted named-theme edit");
    settings.sync();
    check(storedNamedThemeValue(
                namedThemeId, QStringLiteral("uv_canvas_background"))
                == QStringLiteral("#112233")
            && settings.value(QStringLiteral(
                    "viewport/colors/uv_canvas_background")).toString()
                == QStringLiteral("#112233"),
        "Cancel leaves both the saved theme file and active viewport palette unchanged");

    const bool updateNamedThemeDialog = drivePreferencesDialog(
        window,
        [&](QDialog* dialog)
        {
            auto* theme = dialog->findChild<QComboBox*>(
                QStringLiteral("pref_viewport_color_theme"));
            auto* updateButton = dialog->findChild<QPushButton*>(
                QStringLiteral("pref_viewport_theme_update"));
            check(theme && theme->currentData().toString() == namedThemeId
                    && findSwatchColor(
                        dialog, "pref_uv_canvas_background_color")
                        == QStringLiteral("#112233")
                    && findSwatchColor(dialog, "pref_uv_grid_main_color")
                        == QStringLiteral("#445566"),
                "Reopening Preferences restores the saved theme file after cancelling a draft");
            check(chooseThemeColor(
                    dialog,
                    QStringLiteral("pref_uv_canvas_background_color"),
                    QColor("#fedcba")),
                "A saved viewport theme can stage a new UV background");
            check(updateButton && updateButton->isEnabled(),
                "Update Theme is enabled for a selected saved palette");
            if (updateButton)
            {
                updateButton->click();
            }
            check(findSwatchColor(
                    dialog, "pref_uv_canvas_background_color")
                    == QStringLiteral("#fedcba"),
                "Update Theme retains the edited UV background");
            check(readSavedThemeValue(settings, sed::AppThemeKind::viewport,
                        namedThemeId, QStringLiteral("uv_canvas_background"))
                    == QStringLiteral("#fedcba"),
                "Viewport Update writes the edited palette before Preferences is accepted");
            clickPreferencesDialogButton(dialog, QDialogButtonBox::Ok);
        });
    check(updateNamedThemeDialog,
        "Preferences completes updating a saved named viewport theme");
    settings.sync();
    auto* uvCanvas = window.findChild<QWidget*>(
        QStringLiteral("uvEditorCanvas"));
    check(storedNamedThemeValue(
                namedThemeId, QStringLiteral("uv_canvas_background"))
                == QStringLiteral("#fedcba")
            && storedNamedThemeValue(
                namedThemeId, QStringLiteral("uv_grid_main"))
                == QStringLiteral("#445566")
            && uvCanvas
            && uvCanvas->property("sed.uvCanvasBackgroundColor")
                .value<QColor>() == QColor("#fedcba"),
        "Updated theme colors persist and publish to the live UV canvas");

    const bool cancelledThemeChangesDialog = drivePreferencesDialog(
        window,
        [&](QDialog* dialog)
        {
            auto* theme = dialog->findChild<QComboBox*>(
                QStringLiteral("pref_viewport_color_theme"));
            auto* updateButton = dialog->findChild<QPushButton*>(
                QStringLiteral("pref_viewport_theme_update"));
            check(theme && theme->currentData().toString() == namedThemeId,
                "Cancel regression reopens the currently saved named theme");
            check(chooseThemeColor(
                    dialog,
                    QStringLiteral("pref_uv_grid_main_color"),
                    QColor("#010203")),
                "Cancel regression edits a saved theme before cancellation");
            if (updateButton)
            {
                updateButton->click();
            }
            check(theme && theme->findData(namedThemeId) >= 0,
                "Update Theme leaves the selected user theme available");
            clickPreferencesDialogButton(dialog, QDialogButtonBox::Cancel);
        });
    check(cancelledThemeChangesDialog,
        "Preferences can close after Update Theme has committed the file");
    settings.sync();
    check(storedNamedThemeValue(
                namedThemeId, QStringLiteral("uv_canvas_background"))
                == QStringLiteral("#fedcba")
            && storedNamedThemeValue(
                namedThemeId, QStringLiteral("uv_grid_main"))
                == QStringLiteral("#010203")
            && uvCanvas
            && uvCanvas->property("sed.uvCanvasBackgroundColor")
                .value<QColor>() == QColor("#fedcba"),
        "Cancel preserves the explicit theme-file update without saving unrelated live color preferences");

    const bool deleteNamedThemeDialog = drivePreferencesDialog(
        window,
        [&](QDialog* dialog)
        {
            auto* theme = dialog->findChild<QComboBox*>(
                QStringLiteral("pref_viewport_color_theme"));
            auto* deleteButton = dialog->findChild<QPushButton*>(
                QStringLiteral("pref_viewport_theme_delete"));
            check(theme && theme->currentData().toString() == namedThemeId
                    && deleteButton && deleteButton->isEnabled(),
                "Delete Theme is enabled for a selected saved palette");
            if (deleteButton)
            {
                deleteButton->click();
            }
            check(theme && theme->findData(namedThemeId) < 0
                    && readSavedThemeValue(settings, sed::AppThemeKind::viewport,
                        namedThemeId, QStringLiteral("name")).isEmpty(),
                "Delete Theme removes the selected palette and file immediately");
            clickPreferencesDialogButton(dialog, QDialogButtonBox::Ok);
        });
    check(deleteNamedThemeDialog,
        "Preferences completes named viewport theme deletion");
    settings.sync();
    check(storedNamedThemeValue(
                namedThemeId, QStringLiteral("name")).isEmpty(),
        "Delete Theme removes the named viewport file immediately");
    const QString cogKeywordOverride = QStringLiteral("#ff123456");
    const QString cogKeywordSetting = cogScriptThemeColorSettingKey("keyword");
    settings.setValue(QStringLiteral("cog_editor/theme"), QStringLiteral("sed_dark"));
    settings.setValue(cogKeywordSetting, cogKeywordOverride);
    settings.sync();
    const QString cogThemeId = QStringLiteral("custom_copper_theme");
    const bool cogAddNewDialog = drivePreferencesDialog(
        window,
        [&](QDialog* dialog)
        {
            auto* theme = dialog->findChild<QComboBox*>(
                QStringLiteral("pref_cog_editor_theme"));
            auto* addNew = dialog->findChild<QPushButton*>(
                QStringLiteral("pref_cog_editor_theme_add_new"));
            auto* keyword = dialog->findChild<QToolButton*>(
                QStringLiteral("pref_cog_editor_color_keyword"));
            check(theme && theme->currentData().toString()
                    == QStringLiteral("sed_dark")
                    && addNew && keyword
                    && keyword->property("sed.colorValue").value<QColor>()
                        == QColor(cogKeywordOverride),
                "COG Add New starts from the selected theme with its active color override");
            check(submitThemeName(
                    dialog, addNew, QStringLiteral("Copper Theme")),
                "COG Add New accepts a named user palette");
            check(theme && theme->currentData().toString() == cogThemeId
                    && theme->currentText() == QStringLiteral("Copper Theme"),
                "COG Add New selects the duplicated theme by its display name");
            const QJsonObject immediateCogTheme = readSavedThemeObject(
                settings, sed::AppThemeKind::cog, cogThemeId);
            check(immediateCogTheme.value(QStringLiteral("name")).toString()
                    == QStringLiteral("Copper Theme")
                    && immediateCogTheme.value(QStringLiteral("colors")).toObject()
                        .value(QStringLiteral("label")).isString(),
                "COG Add New writes its name and nested label color before Preferences is accepted");
            clickPreferencesDialogButton(dialog, QDialogButtonBox::Ok);
        });
    check(cogAddNewDialog,
        "Preferences completes a COG Add New workflow");
    settings.sync();
    QJsonObject cogThemeObject = readSavedThemeObject(
        settings, sed::AppThemeKind::cog, cogThemeId);
    check(settings.value(QStringLiteral("cog_editor/theme")).toString()
                == cogThemeId
            && cogThemeObject.value(QStringLiteral("name")).toString()
                == QStringLiteral("Copper Theme")
            && cogThemeObject.value(QStringLiteral("colors")).toObject()
                .value(QStringLiteral("keyword")).toString() == cogKeywordOverride
            && resolveCogScriptTheme(settings).keywordForeground
                == QColor(cogKeywordOverride),
        "COG theme files retain the active per-color override when duplicated");

    const bool cogUpdateDialog = drivePreferencesDialog(
        window,
        [&](QDialog* dialog)
        {
            auto* theme = dialog->findChild<QComboBox*>(
                QStringLiteral("pref_cog_editor_theme"));
            auto* keyword = dialog->findChild<QToolButton*>(
                QStringLiteral("pref_cog_editor_color_keyword"));
            auto* update = dialog->findChild<QPushButton*>(
                QStringLiteral("pref_cog_editor_theme_update"));
            check(theme && theme->currentData().toString() == cogThemeId
                    && keyword && update,
                "COG Update opens for a selected saved theme");
            check(chooseThemeColor(
                    dialog,
                    QStringLiteral("pref_cog_editor_color_keyword"),
                    QColor(QStringLiteral("#ff654321"))),
                "COG saved theme color can be edited before Update");
            if (update)
            {
                update->click();
            }
            check(readSavedThemeValue(settings, sed::AppThemeKind::cog,
                        cogThemeId, QStringLiteral("keyword"))
                    == QStringLiteral("#ff654321"),
                "COG Update writes its color before Preferences is accepted");
            clickPreferencesDialogButton(dialog, QDialogButtonBox::Ok);
        });
    check(cogUpdateDialog,
        "Preferences updates a saved COG theme");
    cogThemeObject = readSavedThemeObject(
        settings, sed::AppThemeKind::cog, cogThemeId);
    check(cogThemeObject.value(QStringLiteral("colors")).toObject()
                .value(QStringLiteral("keyword")).toString()
                == QStringLiteral("#ff654321"),
        "COG Update writes the changed color into that theme’s file");

    const bool cogDeleteDialog = drivePreferencesDialog(
        window,
        [&](QDialog* dialog)
        {
            auto* theme = dialog->findChild<QComboBox*>(
                QStringLiteral("pref_cog_editor_theme"));
            auto* remove = dialog->findChild<QPushButton*>(
                QStringLiteral("pref_cog_editor_theme_delete"));
            check(theme && theme->currentData().toString() == cogThemeId
                    && remove && remove->isEnabled(),
                "COG Delete is enabled for a selected user theme");
            if (remove)
            {
                remove->click();
            }
            check(theme && theme->findData(cogThemeId) < 0
                    && readSavedThemeValue(settings, sed::AppThemeKind::cog,
                        cogThemeId, QStringLiteral("name")).isEmpty(),
                "COG Delete removes the user theme and file immediately");
            clickPreferencesDialogButton(dialog, QDialogButtonBox::Ok);
        });
    check(cogDeleteDialog,
        "Preferences deletes a COG theme");
    const std::filesystem::path cogThemeDirectory =
        sed::ui::theme_storage::directoryForSettings(
            settings, sed::AppThemeKind::cog);
    check(!QFileInfo::exists(sed::ui::theme_storage::themeFilePathForLabel(
                cogThemeDirectory, QStringLiteral("Copper Theme"),
                sed::ui::theme_storage::ThemeFormat::cog))
            && settings.value(QStringLiteral("cog_editor/theme")).toString()
                == QStringLiteral("sed_dark"),
        "COG Delete removes the user file and selects a built-in theme");

    restoreMaximum();
}

} // namespace sed::app_tests
