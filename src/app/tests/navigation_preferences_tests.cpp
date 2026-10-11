#include "test_common.hpp"

#include "app_settings.hpp"
#include "gl_view.hpp"
#include "main_window/main_window.hpp"
#include "sed/level/build.hpp"
#include "ui/components/collapsible_section.hpp"
#include "ui/shortcuts/key_sequence_display.hpp"

#include <QAction>
#include <QApplication>
#include <QColor>
#include <QColorDialog>
#include <QCheckBox>
#include <QComboBox>
#include <QCursor>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFocusEvent>
#include <QFontMetrics>
#include <QHelpEvent>
#include <QImage>
#include <QKeyEvent>
#include <QKeySequenceEdit>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMouseEvent>
#include <QPixmap>
#include <QPushButton>
#include <QPointer>
#include <QScrollArea>
#include <QScrollBar>
#include <QSignalBlocker>
#include <QSettings>
#include <QStyle>
#include <QStyleOptionMenuItem>
#include <QStyleOptionComboBox>
#include <QTemporaryDir>
#include <QTimer>
#include <QToolButton>
#include <QToolTip>
#include <QTreeWidget>
#include <QTreeWidgetItemIterator>
#include <QWheelEvent>
#include <QWidgetAction>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <functional>
#include <optional>
#include <utility>
#include <vector>

namespace sed::app_tests
{
namespace
{

class NavigationSettingsFixture final
{
public:
    NavigationSettingsFixture()
        : previousPath_(appSettingsIniPath())
    {
        check(directory_.isValid(), "navigation preferences use a private settings directory");
        if ( !directory_.isValid() )
        {
            return;
        }
        setAppSettingsIniPathOverrideForTest(
            std::filesystem::path(directory_.path().toStdWString()) / "navigation.ini");
        QSettings settings = settingsStore();
        settings.clear();
        settings.setValue("resources/roots", QStringList{});
        settings.setValue("resources/gobs", QStringList{});
        settings.setValue("recovery/autosave_enabled", false);
        settings.sync();
    }

    ~NavigationSettingsFixture()
    {
        setAppSettingsIniPathOverrideForTest(previousPath_);
    }

private:
    QTemporaryDir directory_{};
    std::filesystem::path previousPath_{};
};

void sendMouse(
    QWidget& target,
    const QEvent::Type type,
    const QPointF& position,
    const Qt::MouseButton button,
    const Qt::MouseButtons buttons,
    const Qt::KeyboardModifiers modifiers = Qt::NoModifier)
{
    const QPointF global = target.mapToGlobal(position.toPoint());
    QMouseEvent event(type, position, global, global, button, buttons, modifiers);
    QApplication::sendEvent(&target, &event);
}

bool sendFlyKey(
    MainWindow& window,
    GlView& view,
    const QEvent::Type type,
    const int key,
    const Qt::KeyboardModifiers modifiers)
{
    QKeyEvent event(type, key, modifiers);
    event.setAccepted(false);
    return window.eventFilterForTest(&view, &event) && event.isAccepted();
}

void sendWheel(GlView& view, const Qt::KeyboardModifiers modifiers)
{
    const QPointF point(120.0, 120.0);
    QWheelEvent event(
        point, view.mapToGlobal(point.toPoint()), QPoint{}, QPoint(0, 120),
        Qt::NoButton, modifiers, Qt::NoScrollPhase, false);
    QApplication::sendEvent(&view, &event);
}

void sendNavigationKey(
    QWidget& target,
    const int key,
    const Qt::KeyboardModifiers modifiers = Qt::NoModifier,
    const bool autoRepeat = false,
    const QString& text = {})
{
    QKeyEvent press(QEvent::KeyPress, key, modifiers, text, autoRepeat);
    QKeyEvent release(QEvent::KeyRelease, key, modifiers, QString{}, autoRepeat);
    QApplication::sendEvent(&target, &press);
    QApplication::sendEvent(&target, &release);
}

void prepareView(GlView& view)
{
    view.setAttribute(Qt::WA_DontShowOnScreen);
    view.resize(640, 480);
    view.setShowAxisWidget(false);
    view.setShowToastOverlays(false);
}

bool drivePreferences(
    MainWindow& window,
    const std::function<void(QDialog&)>& callback)
{
    QAction* const action = window.findChild<QAction*>("edit.preferences");
    check(action != nullptr, "navigation regression finds the real Preferences action");
    if ( !action )
    {
        return false;
    }
    bool handled = false;
    QTimer::singleShot(0, &window, [&]()
    {
        QDialog* const dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget());
        check(dialog && dialog->windowTitle() == QStringLiteral("Preferences"),
            "navigation regression reaches the real Preferences modal");
        if ( dialog )
        {
            handled = true;
            callback(*dialog);
        }
    });
    action->trigger();
    return handled;
}

std::vector<QTreeWidgetItem*> bindingItems(QTreeWidget& tree, const char* const id)
{
    std::vector<QTreeWidgetItem*> result{};
    const QString suffix = QStringLiteral("Binding ID: %1").arg(QString::fromLatin1(id));
    for ( QTreeWidgetItemIterator item(&tree); *item; ++item )
    {
        if ( (*item)->toolTip(0).endsWith(suffix) )
        {
            result.push_back(*item);
        }
    }
    return result;
}

std::optional<QPoint> labelLinkPoint(QLabel& label, const QString& expectedTarget)
{
    QString hovered{};
    const auto hover = QObject::connect(&label, &QLabel::linkHovered, &label,
        [&](const QString& target) { hovered = target; });
    std::optional<QPoint> result{};
    const int xStep = std::max(1, label.fontMetrics().horizontalAdvance(QLatin1Char('m')) / 2);
    const int yStep = std::max(1, label.fontMetrics().height() / 2);
    for ( int y = yStep / 2; y < label.height() && !result; y += yStep )
    {
        for ( int x = xStep / 2; x < label.width() && !result; x += xStep )
        {
            hovered.clear();
            sendMouse(label, QEvent::MouseMove, QPointF(x, y), Qt::NoButton, Qt::NoButton);
            if ( hovered == expectedTarget )
            {
                result = QPoint(x, y);
            }
        }
    }
    QObject::disconnect(hover);
    return result;
}

QKeySequenceEdit* shortcutEditor(QWidget* const controls)
{
    if ( auto* const direct = qobject_cast<QKeySequenceEdit*>(controls) )
    {
        return direct;
    }
    return controls ? controls->findChild<QKeySequenceEdit*>() : nullptr;
}

QLineEdit* shortcutDisplay(QKeySequenceEdit* const editor)
{
    return editor ? editor->findChild<QLineEdit*>() : nullptr;
}

void showMenuForDisplayTest(QMenu& menu, const bool naturalWidth = false)
{
    menu.setStyleSheet(
        "QMenu { background: #181a20; color: #e7e9ee; border: 1px solid #343b46; }"
        "QMenu::item { padding: 7px 18px 7px 12px; }"
        "QMenu::item:selected { background: #313945; }");
    menu.adjustSize();
    if ( !naturalWidth )
    {
        menu.setFixedWidth(300);
    }
    menu.ensurePolished();
    menu.popup(QPoint(80, 80));
    QApplication::processEvents(QEventLoop::AllEvents);
}

class NativeShortcutReferenceMenu final : public QMenu
{
public:
    explicit NativeShortcutReferenceMenu(QString formattedSuffix)
        : formattedSuffix_(formattedSuffix)
    {
    }

protected:
    void initStyleOption(QStyleOptionMenuItem* const option,
        const QAction* const action) const override
    {
        QMenu::initStyleOption(option, action);
        const int tab = option->text.indexOf(QLatin1Char('\t'));
        if ( tab >= 0 )
        {
            // Retain the raw caption used to compute QMenu's native shortcut
            // column, while providing the expected painted glyphs.
            option->text = option->text.left(tab) + QLatin1Char('\t') + formattedSuffix_;
            option->reservedShortcutWidth = std::max(
                option->reservedShortcutWidth,
                option->fontMetrics.horizontalAdvance(formattedSuffix_));
        }
    }

private:
    QString formattedSuffix_;
};

class DynamicMenuPanelWidget final : public QWidget
{
public:
    explicit DynamicMenuPanelWidget(const QSize initialSize, QWidget* const parent = nullptr)
        : QWidget(parent), sizeHint_(initialSize)
    {
    }

    QSize sizeHint() const override
    {
        return sizeHint_;
    }

    void setReportedSizeHint(const QSize size)
    {
        sizeHint_ = size;
        updateGeometry();
    }

private:
    QSize sizeHint_{};
};

QImage captureShownMenuForDisplayTest(QMenu& menu)
{
    const QPixmap captured = menu.grab();
    if ( captured.isNull() )
    {
        return {};
    }
    return captured.toImage().convertToFormat(QImage::Format_ARGB32_Premultiplied);
}

class ScopedMenuAnimationDisable final
{
public:
    ScopedMenuAnimationDisable()
        : fadeEnabled_(QApplication::isEffectEnabled(Qt::UI_FadeMenu)),
          animateEnabled_(QApplication::isEffectEnabled(Qt::UI_AnimateMenu))
    {
        QApplication::setEffectEnabled(Qt::UI_FadeMenu, false);
        QApplication::setEffectEnabled(Qt::UI_AnimateMenu, false);
    }

    ~ScopedMenuAnimationDisable()
    {
        QApplication::setEffectEnabled(Qt::UI_FadeMenu, fadeEnabled_);
        QApplication::setEffectEnabled(Qt::UI_AnimateMenu, animateEnabled_);
    }

    ScopedMenuAnimationDisable(const ScopedMenuAnimationDisable&) = delete;
    ScopedMenuAnimationDisable& operator=(const ScopedMenuAnimationDisable&) = delete;

private:
    bool fadeEnabled_{};
    bool animateEnabled_{};
};

QRect imageRectForLogicalRect(const QImage& image, const QRect& logicalRect)
{
    const qreal devicePixelRatio = image.devicePixelRatio();
    const int left = qRound(logicalRect.left() * devicePixelRatio);
    const int top = qRound(logicalRect.top() * devicePixelRatio);
    const int right = qRound((logicalRect.x() + logicalRect.width()) * devicePixelRatio);
    const int bottom = qRound((logicalRect.y() + logicalRect.height()) * devicePixelRatio);
    return QRect(left, top, std::max(0, right - left), std::max(0, bottom - top));
}

QRect shortcutGlyphBounds(const QImage& image, const QRect& actionRect)
{
    const QRect row = imageRectForLogicalRect(image, actionRect).intersected(image.rect());
    const int firstColumn = row.left() + row.width() / 2;
    QRect bounds{};
    for ( int y = row.top() + 2; y < row.bottom() - 1; ++y )
    {
        for ( int x = firstColumn; x < row.right() - 2; ++x )
        {
            if ( qGray(image.pixel(x, y)) > 160 )
            {
                const QRect pixelRect(x, y, 1, 1);
                bounds = bounds.isEmpty() ? pixelRect : bounds.united(pixelRect);
            }
        }
    }
    return bounds;
}

QImage shortcutGlyphCrop(const QImage& image, const QRect& actionRect)
{
    const QRect bounds = shortcutGlyphBounds(image, actionRect);
    return bounds.isEmpty() ? QImage{} : image.copy(bounds);
}

QImage renderMenuForDisplayTest(QMenu& menu, const bool naturalWidth = false)
{
    showMenuForDisplayTest(menu, naturalWidth);
    const QImage image = captureShownMenuForDisplayTest(menu);
    menu.hide();
    QApplication::processEvents(QEventLoop::AllEvents);
    return image;
}

bool setBindingAlternative(
    QTreeWidget& tree, const char* const id, const int index, const QKeySequence& sequence)
{
    const auto items = bindingItems(tree, id);
    if ( items.empty() )
    {
        return false;
    }
    QWidget* const controls = tree.itemWidget(items.front(), 1);
    if ( auto* const selector = controls
        ? controls->findChild<QComboBox*>("pref_keymap_alternative") : nullptr )
    {
        if ( index >= selector->count() )
        {
            return false;
        }
        selector->setCurrentIndex(index);
        if ( auto* const edit = shortcutEditor(controls) )
        {
            edit->setKeySequence(sequence);
            return true;
        }
        return false;
    }
    // The pre-regression UI has separate fields/rows. Keeping this fallback
    // lets the conflict regressions reach their real missing validation.
    const auto edits = controls ? controls->findChildren<QKeySequenceEdit*>()
        : QList<QKeySequenceEdit*>{};
    QKeySequenceEdit* const edit = qobject_cast<QKeySequenceEdit*>(controls)
        ? (index < static_cast<int>(items.size())
            ? shortcutEditor(tree.itemWidget(items[static_cast<std::size_t>(index)], 1)) : nullptr)
        : (index < edits.size() ? edits[index] : nullptr);
    if ( !edit )
    {
        return false;
    }
    edit->setKeySequence(sequence);
    return true;
}

bool selectPreferenceCategory(QDialog& dialog, const QString& name)
{
    auto* const categories = dialog.findChild<QListWidget*>("pref_categories");
    for ( int index = 0; categories && index < categories->count(); ++index )
    {
        if ( categories->item(index)->text() == name )
        {
            categories->setCurrentRow(index);
            return true;
        }
    }
    return false;
}

bool revealMouseControls(QDialog& dialog)
{
    if ( !selectPreferenceCategory(dialog, QStringLiteral("Viewport")) )
    {
        return false;
    }
    for ( QToolButton* const header : dialog.findChildren<QToolButton*>() )
    {
        if ( header->text() != QStringLiteral("Mouse Controls") )
        {
            continue;
        }
        if ( auto* const section = dynamic_cast<ui::CollapsibleSection*>(header->parentWidget()) )
        {
            section->setExpandedImmediate(true);
            QApplication::processEvents(QEventLoop::AllEvents);
            for ( QScrollArea* const scroll : dialog.findChildren<QScrollArea*>() )
            {
                if ( scroll->isAncestorOf(section) )
                {
                    scroll->ensureWidgetVisible(section);
                }
            }
            return true;
        }
    }
    return false;
}

bool revealBanners(QDialog& dialog)
{
    if ( !selectPreferenceCategory(dialog, QStringLiteral("Viewport")) )
    {
        return false;
    }
    for ( QToolButton* const header : dialog.findChildren<QToolButton*>() )
    {
        if ( header->text() != QStringLiteral("Banners") )
        {
            continue;
        }
        if ( auto* const section = dynamic_cast<ui::CollapsibleSection*>(header->parentWidget()) )
        {
            section->setExpandedImmediate(true);
            QApplication::processEvents(QEventLoop::AllEvents);
            for ( QScrollArea* const scroll : dialog.findChildren<QScrollArea*>() )
            {
                if ( scroll->isAncestorOf(section) )
                {
                    scroll->ensureWidgetVisible(section);
                }
            }
            return true;
        }
    }
    return false;
}

void processEventsFor(const int durationMs)
{
    QEventLoop loop{};
    QTimer::singleShot(durationMs, &loop, &QEventLoop::quit);
    loop.exec(QEventLoop::AllEvents);
}

QImage keymapRowImage(QTreeWidget& tree, QTreeWidgetItem* const item)
{
    if ( !item )
    {
        return {};
    }
    tree.scrollToItem(item, QAbstractItemView::PositionAtCenter);
    QApplication::processEvents(QEventLoop::AllEvents);
    QRect row = tree.visualItemRect(item);
    if ( !row.isValid() || !row.intersects(tree.viewport()->rect()) )
    {
        return {};
    }
    row.setLeft(0);
    row.setWidth(tree.viewport()->width());
    return tree.viewport()->grab(row).toImage();
}

void neutralizeKeymapTreeSelection(QTreeWidget& tree)
{
    tree.setCurrentItem(nullptr);
    tree.clearSelection();
    tree.setFocus(Qt::OtherFocusReason);
    QApplication::processEvents(QEventLoop::AllEvents);
}

bool chooseBannerColor(
    QDialog& preferences, const QString& objectName, const QColor& color)
{
    auto* const swatch = preferences.findChild<QToolButton*>(objectName);
    if ( !swatch )
    {
        return false;
    }
    bool handled = false;
    int attempts = 0;
    QTimer driver{};
    driver.setSingleShot(true);
    QObject::connect(&driver, &QTimer::timeout, &preferences, [&]()
    {
        QColorDialog* picker = qobject_cast<QColorDialog*>(QApplication::activeModalWidget());
        if ( !picker )
        {
            picker = preferences.findChild<QColorDialog*>(
                QStringLiteral("pref_banner_color_dialog"));
        }
        if ( picker )
        {
            check(picker->options().testFlag(QColorDialog::ShowAlphaChannel),
                "viewport banner color picker exposes alpha editing");
            picker->setCurrentColor(color);
            picker->accept();
            handled = true;
            return;
        }
        if ( ++attempts < 100 )
        {
            driver.start(10);
        }
    });
    driver.start(0);
    swatch->click();
    driver.stop();
    return handled && swatch->property("sed.colorValue").value<QColor>() == color;
}

bool waitForNavigationUi(const std::function<bool()>& predicate)
{
    QElapsedTimer elapsed{};
    elapsed.start();
    do
    {
        QApplication::processEvents(QEventLoop::AllEvents, 20);
        if ( predicate() )
        {
            return true;
        }
    } while ( elapsed.elapsed() < 1000 );
    return false;
}

void testViewportBannerPreferencesExposeIndependentControls()
{
    NavigationSettingsFixture settingsIsolation{};
    MainWindow window(nullptr, true);
    const ViewportBannerSettings defaults{};
    drivePreferences(window, [&](QDialog& dialog)
    {
        const bool viewportSelected = selectPreferenceCategory(dialog, QStringLiteral("Viewport"));
        auto* const banners = dialog.findChild<QWidget*>("pref_viewport_banners");
        check(viewportSelected && banners,
            "Viewport preferences expose a dedicated Banners section");
        const std::array<const char*, 7> bannerIds{
            "general", "projection", "fly_speed", "error",
            "cleave_rotate", "cleave_scale", "placement_yaw",
        };
        bool complete = banners != nullptr;
        for ( std::size_t index = 0; index < bannerIds.size(); ++index )
        {
            const char* const id = bannerIds[index];
            const QString prefix = QStringLiteral("pref_banner_%1_").arg(QString::fromLatin1(id));
            auto* const label = dialog.findChild<QLabel*>(prefix + "label");
            auto* const duration = dialog.findChild<QDoubleSpinBox*>(prefix + "hide_time");
            const bool cleaveDefault = index == 4u || index == 5u;
            const QColor expectedBorder = defaults.styles[index].borderColor;
            complete = complete && label && label->text()
                    == QString::fromLatin1(kViewportBannerKindLabels[index])
                && duration && banners->isAncestorOf(duration)
                && duration->minimum() == 0.10 && duration->maximum() == 30.00
                && duration->decimals() == 3
                && std::abs(duration->value()
                    - static_cast<double>(defaults.styles[index].hideAfterMs) / 1000.0) < 1.0e-9
                && (!cleaveDefault || duration->value() == 0.40)
                && duration->property("sed.viewportBannerPreference").toBool();
            const std::array<QColor, 3> expectedColors{
                defaults.styles[index].textColor,
                defaults.styles[index].backgroundColor,
                expectedBorder,
            };
            for ( const char* const colorField : { "text_color", "background_color", "border_color" } )
            {
                auto* const color = dialog.findChild<QToolButton*>(prefix + QString::fromLatin1(colorField));
                const QVariant colorValue = color ? color->property("sed.colorValue") : QVariant{};
                const std::size_t colorIndex = colorField[0] == 't' ? 0u
                    : colorField[0] == 'b' && colorField[1] == 'a' ? 1u : 2u;
                complete = complete && color && banners->isAncestorOf(color)
                    && color->property("sed.viewportBannerPreference").toBool()
                    && colorValue.isValid() && colorValue.canConvert<QColor>()
                    && colorValue.value<QColor>() == expectedColors[colorIndex];
            }
        }
        for ( const char* const id : { "general", "error" } )
        {
            const QString name = QStringLiteral("pref_banner_%1_use_message_duration")
                .arg(QString::fromLatin1(id));
            auto* const checkbox = dialog.findChild<QCheckBox*>(name);
            const std::size_t index = QString::fromLatin1(id) == QStringLiteral("general") ? 0u : 3u;
            complete = complete && checkbox && banners->isAncestorOf(checkbox)
                && checkbox->text() == QStringLiteral("Use message duration")
                && checkbox->isChecked() == defaults.styles[index].useMessageDuration
                && checkbox->property("sed.viewportBannerPreference").toBool();
        }
        for ( const char* const id : { "projection", "fly_speed", "cleave_rotate",
                 "cleave_scale", "placement_yaw" } )
        {
            complete = complete && !dialog.findChild<QCheckBox*>(
                QStringLiteral("pref_banner_%1_use_message_duration").arg(QString::fromLatin1(id)));
        }
        check(complete,
            "each viewport banner exposes its own bounded duration and three alpha-capable color controls; General and Error retain message-duration defaults");
        dialog.reject();
    });
}

void testViewportBannerDraftCancelAcceptRestoreAndNoOp()
{
    NavigationSettingsFixture settingsIsolation{};
    MainWindow window(nullptr, true);
    GlView* const view = window.glViewForTest();
    check(view != nullptr, "viewport banner preference tests use the real viewport cache");
    if ( !view )
    {
        return;
    }

    ViewportBannerSettings nonCentisecond = view->viewportBannerSettings();
    nonCentisecond.styles[static_cast<std::size_t>(ViewportBannerKind::general)].hideAfterMs = 1351;
    QSettings preloadSettings = settingsStore();
    check(saveViewportBannerSettings(preloadSettings, nonCentisecond)
            && view->setViewportBannerSettings(nonCentisecond),
        "banner precision fixture preloads a valid 1351 ms duration into persistence and the live cache");
    const auto settingsSnapshot = []()
    {
        QVariantMap values{};
        const QSettings settings = settingsStore();
        for ( const QString& key : settings.allKeys() )
        {
            values.insert(key, settings.value(key));
        }
        return values;
    };
    const QVariantMap precisionSnapshot = settingsSnapshot();
    const quint64 precisionPublicationCount = window.property(
        "sed.preferencesApplyPublicationCount").toULongLong();
    drivePreferences(window, [&](QDialog& dialog)
    {
        auto* const duration = dialog.findChild<QDoubleSpinBox*>(
            "pref_banner_general_hide_time");
        check(duration && duration->value() == 1.351,
            "untouched banner draft displays the complete millisecond duration precision");
        auto* const buttons = dialog.findChild<QDialogButtonBox*>();
        auto* const ok = buttons ? buttons->button(QDialogButtonBox::Ok) : nullptr;
        if ( ok )
        {
            ok->click();
        }
        else
        {
            dialog.reject();
        }
    });
    check(view->viewportBannerSettings() == nonCentisecond
            && loadViewportBannerSettings(settingsStore()) == nonCentisecond
            && precisionSnapshot == settingsSnapshot()
            && window.property("sed.preferencesApplyPublicationCount").toULongLong()
                == precisionPublicationCount,
        "unchanged Preferences acceptance preserves the exact 1351 ms persisted/cache value without publication");

    const ViewportBannerSettings original = view->viewportBannerSettings();
    const ViewportNavigationSettings originalNavigation = view->viewportNavigationSettings();
    const auto originalRebuilds = view->meshRebuildCountForTest();
    const std::size_t originalUndoDepth = window.undoDepthForTest();
    const QVector3D originalCameraEye = view->cameraEye();
    const QVector3D originalCameraTarget = view->cameraTarget();
    const auto withoutBannerKeys = [](QVariantMap values)
    {
        for ( auto it = values.begin(); it != values.end(); )
        {
            if ( it.key().startsWith(QStringLiteral("viewport/banners/")) )
            {
                it = values.erase(it);
            }
            else
            {
                ++it;
            }
        }
        return values;
    };
    const QVariantMap originalPersisted = settingsSnapshot();
    const QColor cancelledColor(13, 52, 101, 77);
    const QColor acceptedColor(24, 81, 143, 93);
    const QColor acceptedBackground(62, 28, 91, 117);

    drivePreferences(window, [&](QDialog& dialog)
    {
        check(revealBanners(dialog), "banner Cancel fixture expands Viewport > Banners");
        auto* const generalDuration = dialog.findChild<QDoubleSpinBox*>(
            "pref_banner_general_hide_time");
        auto* const cleaveDuration = dialog.findChild<QDoubleSpinBox*>(
            "pref_banner_cleave_rotate_hide_time");
        auto* const errorDuration = dialog.findChild<QDoubleSpinBox*>(
            "pref_banner_error_hide_time");
        auto* const generalMessageDuration = dialog.findChild<QCheckBox*>(
            "pref_banner_general_use_message_duration");
        auto* const errorMessageDuration = dialog.findChild<QCheckBox*>(
            "pref_banner_error_use_message_duration");
        check(generalDuration && cleaveDuration && errorDuration
                && generalMessageDuration && errorMessageDuration,
            "banner Cancel fixture finds independent durations and both message-duration overrides");
        if ( generalDuration && cleaveDuration && errorDuration
            && generalMessageDuration && errorMessageDuration )
        {
            generalDuration->setValue(2.75);
            cleaveDuration->setValue(1.35);
            errorDuration->setValue(4.25);
            generalMessageDuration->setChecked(false);
            errorMessageDuration->setChecked(false);
            check(chooseBannerColor(dialog, QStringLiteral("pref_banner_general_text_color"),
                    cancelledColor),
                "banner Cancel fixture selects an RGBA text color through the alpha picker");
            check(view->viewportBannerSettings() == original
                    && settingsSnapshot() == originalPersisted
                    && view->meshRebuildCountForTest() == originalRebuilds
                    && window.undoDepthForTest() == originalUndoDepth
                    && view->cameraEye() == originalCameraEye
                    && view->cameraTarget() == originalCameraTarget,
                "banner drafts leave settings, live banner cache, geometry, camera, and history unchanged before OK");
        }
        dialog.reject();
    });
    check(view->viewportBannerSettings() == original
            && settingsSnapshot() == originalPersisted
            && view->meshRebuildCountForTest() == originalRebuilds
            && window.undoDepthForTest() == originalUndoDepth
            && view->cameraEye() == originalCameraEye
            && view->cameraTarget() == originalCameraTarget,
        "Cancel discards all banner colors, durations, and message-duration drafts");

    ViewportBannerSettings expected = original;
    expected.styles[static_cast<std::size_t>(ViewportBannerKind::general)].hideAfterMs = 2750;
    expected.styles[static_cast<std::size_t>(ViewportBannerKind::general)].useMessageDuration = false;
    expected.styles[static_cast<std::size_t>(ViewportBannerKind::general)].textColor = acceptedColor;
    expected.styles[static_cast<std::size_t>(ViewportBannerKind::flySpeed)].backgroundColor = acceptedBackground;
    expected.styles[static_cast<std::size_t>(ViewportBannerKind::cleaveRotate)].hideAfterMs = 1350;
    expected.styles[static_cast<std::size_t>(ViewportBannerKind::error)].hideAfterMs = 4250;
    expected.styles[static_cast<std::size_t>(ViewportBannerKind::error)].useMessageDuration = false;
    bool accepted = false;
    drivePreferences(window, [&](QDialog& dialog)
    {
        check(revealBanners(dialog), "banner Accept fixture expands Viewport > Banners after Cancel");
        auto* const generalDuration = dialog.findChild<QDoubleSpinBox*>(
            "pref_banner_general_hide_time");
        auto* const cleaveDuration = dialog.findChild<QDoubleSpinBox*>(
            "pref_banner_cleave_rotate_hide_time");
        auto* const errorDuration = dialog.findChild<QDoubleSpinBox*>(
            "pref_banner_error_hide_time");
        auto* const generalMessageDuration = dialog.findChild<QCheckBox*>(
            "pref_banner_general_use_message_duration");
        auto* const errorMessageDuration = dialog.findChild<QCheckBox*>(
            "pref_banner_error_use_message_duration");
        if ( !generalDuration || !cleaveDuration || !errorDuration
            || !generalMessageDuration || !errorMessageDuration )
        {
            dialog.reject();
            return;
        }
        generalDuration->setValue(2.75);
        cleaveDuration->setValue(1.35);
        errorDuration->setValue(4.25);
        generalMessageDuration->setChecked(false);
        errorMessageDuration->setChecked(false);
        const bool colorsChosen =
            chooseBannerColor(dialog, QStringLiteral("pref_banner_general_text_color"), acceptedColor)
            && chooseBannerColor(dialog, QStringLiteral("pref_banner_fly_speed_background_color"), acceptedBackground);
        check(colorsChosen, "banner Accept fixture sets independent alpha colors on two banner kinds");
        check(view->viewportBannerSettings() == original
                && settingsSnapshot() == originalPersisted,
            "accepted banner edits remain drafts until the Preferences OK button is pressed");
        auto* const buttons = dialog.findChild<QDialogButtonBox*>();
        auto* const ok = buttons ? buttons->button(QDialogButtonBox::Ok) : nullptr;
        accepted = ok && ok->isEnabled();
        check(accepted, "valid independent banner drafts leave Preferences OK enabled");
        if ( ok && ok->isEnabled() )
        {
            ok->click();
        }
        else
        {
            dialog.reject();
        }
    });
    check(accepted && view->viewportBannerSettings() == expected
            && loadViewportBannerSettings(settingsStore()) == expected
            && settingsStore().value(QStringLiteral("viewport/banners/general/text_color")).toString()
                == acceptedColor.name(QColor::HexArgb)
            && view->viewportNavigationSettings() == originalNavigation
            && view->meshRebuildCountForTest() == originalRebuilds
            && window.undoDepthForTest() == originalUndoDepth
            && view->cameraEye() == originalCameraEye
            && view->cameraTarget() == originalCameraTarget
            && withoutBannerKeys(settingsSnapshot()) == withoutBannerKeys(originalPersisted),
        "OK round-trips independent RGBA banner settings without touching camera, history, meshes, or unrelated preferences");

    drivePreferences(window, [&](QDialog& dialog)
    {
        check(revealBanners(dialog), "banner reopen fixture reaches Viewport > Banners");
        auto* const generalDuration = dialog.findChild<QDoubleSpinBox*>(
            "pref_banner_general_hide_time");
        auto* const cleaveDuration = dialog.findChild<QDoubleSpinBox*>(
            "pref_banner_cleave_rotate_hide_time");
        auto* const generalColor = dialog.findChild<QToolButton*>(
            "pref_banner_general_text_color");
        auto* const flyBackground = dialog.findChild<QToolButton*>(
            "pref_banner_fly_speed_background_color");
        check(generalDuration && generalDuration->value() == 2.75
                && cleaveDuration && cleaveDuration->value() == 1.35
                && generalColor && generalColor->property("sed.colorValue").value<QColor>() == acceptedColor
                && flyBackground && flyBackground->property("sed.colorValue").value<QColor>() == acceptedBackground,
            "reopening Preferences restores accepted durations and alpha colors for their own banner kinds");
        dialog.reject();
    });

    const ViewportBannerSettings defaults{};
    const quint64 publicationCount = window.property(
        "sed.preferencesApplyPublicationCount").toULongLong();
    drivePreferences(window, [&](QDialog& dialog)
    {
        check(revealBanners(dialog), "banner defaults fixture expands Viewport > Banners");
        if ( auto* const generalDuration = dialog.findChild<QDoubleSpinBox*>(
                "pref_banner_general_hide_time") )
        {
            generalDuration->setValue(8.00);
        }
        (void)chooseBannerColor(dialog, QStringLiteral("pref_banner_general_text_color"), cancelledColor);
        if ( auto* const reset = dialog.findChild<QPushButton*>("pref_banner_reset_defaults") )
        {
            reset->click();
        }
        else
        {
            check(false, "Viewport > Banners exposes Restore Defaults");
        }
        bool restored = true;
        for ( std::size_t index = 0; index < kViewportBannerKindCount; ++index )
        {
            const QString prefix = QStringLiteral("pref_banner_%1_")
                .arg(QString::fromLatin1(kViewportBannerKindIds[index]));
            auto* const duration = dialog.findChild<QDoubleSpinBox*>(prefix + "hide_time");
            auto* const textColor = dialog.findChild<QToolButton*>(prefix + "text_color");
            auto* const background = dialog.findChild<QToolButton*>(prefix + "background_color");
            auto* const border = dialog.findChild<QToolButton*>(prefix + "border_color");
            restored = restored && duration
                && duration->value() == static_cast<double>(defaults.styles[index].hideAfterMs) / 1000.0
                && textColor && textColor->property("sed.colorValue").value<QColor>() == defaults.styles[index].textColor
                && background && background->property("sed.colorValue").value<QColor>() == defaults.styles[index].backgroundColor
                && border && border->property("sed.colorValue").value<QColor>() == defaults.styles[index].borderColor;
            if ( auto* const messageDuration = dialog.findChild<QCheckBox*>(
                    prefix + "use_message_duration") )
            {
                restored = restored
                    && messageDuration->isChecked() == defaults.styles[index].useMessageDuration;
            }
        }
        check(restored, "Restore Defaults resets every banner duration, message-duration option, and RGBA swatch");
        auto* const buttons = dialog.findChild<QDialogButtonBox*>();
        auto* const ok = buttons ? buttons->button(QDialogButtonBox::Ok) : nullptr;
        if ( ok )
        {
            ok->click();
        }
        else
        {
            dialog.reject();
        }
    });
    check(view->viewportBannerSettings() == defaults
            && loadViewportBannerSettings(settingsStore()) == defaults
            && window.property("sed.preferencesApplyPublicationCount").toULongLong()
                == publicationCount + 1u
            && view->meshRebuildCountForTest() == originalRebuilds
            && window.undoDepthForTest() == originalUndoDepth
            && view->cameraEye() == originalCameraEye
            && view->cameraTarget() == originalCameraTarget,
        "Restore Defaults publishes the original banner styles without entering geometry, camera, or history paths");

    const QVariantMap beforeNoOp = settingsSnapshot();
    const quint64 beforeNoOpPublications = window.property(
        "sed.preferencesApplyPublicationCount").toULongLong();
    drivePreferences(window, [&](QDialog& dialog)
    {
        auto* const buttons = dialog.findChild<QDialogButtonBox*>();
        auto* const ok = buttons ? buttons->button(QDialogButtonBox::Ok) : nullptr;
        if ( ok )
        {
            ok->click();
        }
        else
        {
            dialog.reject();
        }
    });
    check(settingsSnapshot() == beforeNoOp
            && window.property("sed.preferencesApplyPublicationCount").toULongLong()
                == beforeNoOpPublications
            && view->viewportBannerSettings() == defaults
            && view->meshRebuildCountForTest() == originalRebuilds
            && window.undoDepthForTest() == originalUndoDepth,
        "accepting unchanged banner defaults performs no settings write or Preferences publication");
}

void testPreferencesExposeCompleteNavigation()
{
    NavigationSettingsFixture settingsIsolation{};
    MainWindow window(nullptr, true);
    bool inspected = drivePreferences(window, [&](QDialog& dialog)
    {
        auto* const tree = dialog.findChild<QTreeWidget*>("pref_keymap_tree");
        check(tree != nullptr, "navigation preferences expose the keymap tree");
        if ( tree )
        {
            const auto gridItems = bindingItems(*tree, "viewport.show_grid");
            auto* const editor = gridItems.size() == 1
                ? shortcutEditor(tree->itemWidget(gridItems.front(), 1))
                : nullptr;
            check(editor && editor->keySequence() == QKeySequence(Qt::Key_G),
                "Preferences exposes the existing Grid G binding as an editable action");
            check(gridItems.size() == 1 && gridItems.front()->text(0) == QStringLiteral("Toggle Grid"),
                "Grid visibility uses the unambiguous Toggle Grid action label");

            const auto alignItems = bindingItems(*tree, "viewport.grid_align_to_view");
            QList<QKeySequence> alternatives{};
            if ( alignItems.size() == 1 )
            {
                QWidget* const controls = tree->itemWidget(alignItems.front(), 1);
                if ( auto* const selector = controls
                    ? controls->findChild<QComboBox*>("pref_keymap_alternative") : nullptr )
                {
                    for ( int index = 0; index < selector->count(); ++index )
                    {
                        selector->setCurrentIndex(index);
                        if ( auto* const edit = shortcutEditor(controls) )
                        {
                            alternatives.push_back(edit->keySequence());
                        }
                    }
                    selector->setCurrentIndex(0);
                }
                else if ( auto* const direct = qobject_cast<QKeySequenceEdit*>(controls) )
                {
                    alternatives.push_back(direct->keySequence());
                }
                else if ( controls )
                {
                    for ( QKeySequenceEdit* const alternative : controls->findChildren<QKeySequenceEdit*>() )
                    {
                        alternatives.push_back(alternative->keySequence());
                    }
                }
            }
            check(alignItems.size() == 1
                    && alternatives.contains(QKeySequence(int(Qt::KeypadModifier) | Qt::Key_0))
                    && alternatives.contains(QKeySequence(Qt::Key_F3)),
                "Align Grid to View occupies one action row with both Numpad 0 and F3 alternatives");
            QWidget* const alignControls = alignItems.size() == 1
                ? tree->itemWidget(alignItems.front(), 1) : nullptr;
            check(alignControls && alignControls->findChildren<QKeySequenceEdit*>().size() == 1
                    && alignControls->findChild<QToolButton*>("pref_keymap_add")
                    && alignControls->findChild<QToolButton*>("pref_keymap_remove"),
                "Grid alignment uses one compact editor with add/remove alternatives");
            const auto faceItems = bindingItems(*tree, "surface.submode.face");
            check(faceItems.size() == 1,
                "multi-shortcut actions use one row consistently beyond Grid alignment");
            const auto cogItems = bindingItems(*tree, "cog_editor.reload");
            check(cogItems.size() == 1 && cogItems.front()->text(0) == QStringLiteral("Reload")
                    && cogItems.front()->parent()
                    && cogItems.front()->parent()->text(0) == QStringLiteral("COG Script Editor"),
                "COG Script Editor category makes repeated COG Editor action prefixes unnecessary");
            const auto fastItems = bindingItems(*tree, "viewport.fly.fast");
            const auto slowItems = bindingItems(*tree, "viewport.fly.slow");
            check(fastItems.size() == 1 && slowItems.size() == 1
                    && fastItems.front()->parent() == slowItems.front()->parent()
                    && fastItems.front()->parent()->text(0) == QStringLiteral("Camera"),
                "Fly Faster and Fly Slower keep their shared Camera category under alphabetical ordering");
        }

        const std::array<std::pair<const char*, int>, 6> defaultButtons{
            std::pair{"pick", int(Qt::LeftButton)},
            std::pair{"fly_look", int(Qt::RightButton)},
            std::pair{"orbit", int(Qt::MiddleButton)},
            std::pair{"pan", int(Qt::MiddleButton)},
            std::pair{"dolly", int(Qt::MiddleButton)},
            std::pair{"fast_dolly", int(Qt::MiddleButton)},
        };
        const std::array<int, 6> defaultModifiers{
            0, 0, 0, int(Qt::ShiftModifier), int(Qt::ControlModifier),
            int(Qt::ShiftModifier | Qt::ControlModifier),
        };
        bool completeMouseControls = true;
        for ( std::size_t index = 0; index < defaultButtons.size(); ++index )
        {
            const auto& [id, defaultButton] = defaultButtons[index];
            auto* const button = dialog.findChild<QComboBox*>(
                QStringLiteral("pref_mouse_%1_button").arg(QString::fromLatin1(id)));
            auto* const modifiers = dialog.findChild<QComboBox*>(
                QStringLiteral("pref_mouse_%1_modifiers").arg(QString::fromLatin1(id)));
            completeMouseControls = completeMouseControls && button && modifiers
                && button->currentData().toInt() == defaultButton
                && modifiers->currentData().toInt() == defaultModifiers[index];
        }
        auto* const zoom = dialog.findChild<QComboBox*>("pref_mouse_zoom_modifiers");
        auto* const flySpeed = dialog.findChild<QComboBox*>("pref_mouse_fly_speed_modifiers");
        check(completeMouseControls && zoom && flySpeed
                && zoom->currentData().toInt() == 0 && flySpeed->currentData().toInt() == 0,
            "Mouse Controls offers all current press/modifier actions and both wheel controls with unchanged defaults");
        auto* const buttonHeading = dialog.findChild<QLabel*>("pref_mouse_button_heading");
        auto* const modifierHeading = dialog.findChild<QLabel*>("pref_mouse_modifiers_heading");
        auto* const orbitModifiers = dialog.findChild<QComboBox*>("pref_mouse_orbit_modifiers");
        const int ctrlShiftIndex = orbitModifiers
            ? orbitModifiers->findData(int(Qt::ControlModifier | Qt::ShiftModifier)) : -1;
        check(buttonHeading && buttonHeading->text() == QStringLiteral("Mouse button")
                && modifierHeading && modifierHeading->text() == QStringLiteral("Keyboard modifiers"),
            "Mouse Controls identifies the button and keyboard-modifier columns");
        check(orbitModifiers && ctrlShiftIndex >= 0
                && orbitModifiers->itemText(ctrlShiftIndex) == QStringLiteral("Ctrl Shift"),
            "mouse modifier combinations use spaces between modifier names");
        bool namedSection = false;
        for ( QToolButton* const button : dialog.findChildren<QToolButton*>() )
        {
            namedSection = namedSection || button->text() == QStringLiteral("Mouse Controls");
        }
        auto* const pickButton = dialog.findChild<QComboBox*>("pref_mouse_pick_button");
        auto* const pickModifiers = dialog.findChild<QComboBox*>("pref_mouse_pick_modifiers");
        auto* const zoomInvert = dialog.findChild<QCheckBox*>("pref_mouse_zoom_inverted");
        auto* const horizontal = dialog.findChild<QCheckBox*>("pref_mouse_invert_horizontal");
        auto* const vertical = dialog.findChild<QCheckBox*>("pref_mouse_invert_vertical");
        check(namedSection && pickButton && pickButton->itemText(pickButton->findData(int(Qt::NoButton))) == QStringLiteral("Disabled")
                && pickModifiers && pickModifiers->itemText(pickModifiers->findData(0)) == QStringLiteral("No modifiers")
                && zoomInvert && zoomInvert->text() == QStringLiteral("Invert")
                && horizontal && !horizontal->isChecked() && vertical && !vertical->isChecked(),
            "Mouse Controls has explicit disabled/no-modifier semantics and default-off camera inversion");
        dialog.reject();
    });
    check(inspected, "navigation preferences inspection completed");
}

void testCompactShortcutAlternativesAndPersistence()
{
    NavigationSettingsFixture settingsIsolation{};
    MainWindow window(nullptr, true);
    const QList<QKeySequence> expected{
        QKeySequence(Qt::Key_G), QKeySequence(Qt::Key_F19), QKeySequence(Qt::Key_F20),
    };
    bool changed = false;
    drivePreferences(window, [&](QDialog& dialog)
    {
        auto* const tree = dialog.findChild<QTreeWidget*>("pref_keymap_tree");
        const auto items = tree ? bindingItems(*tree, "viewport.show_grid")
            : std::vector<QTreeWidgetItem*>{};
        QWidget* const controls = items.size() == 1 ? tree->itemWidget(items.front(), 1) : nullptr;
        auto* const selector = controls ? controls->findChild<QComboBox*>("pref_keymap_alternative") : nullptr;
        auto* const add = controls ? controls->findChild<QToolButton*>("pref_keymap_add") : nullptr;
        auto* const remove = controls ? controls->findChild<QToolButton*>("pref_keymap_remove") : nullptr;
        check(selector && add && remove && shortcutEditor(controls),
            "every action has compact controls for adding and removing alternative shortcuts");
        if ( !selector || !add || !remove || !shortcutEditor(controls) )
        {
            dialog.reject();
            return;
        }
        add->click();
        shortcutEditor(controls)->setKeySequence(expected[1]);
        add->click();
        shortcutEditor(controls)->setKeySequence(expected[2]);
        check(selector->count() == 3 && controls->findChildren<QKeySequenceEdit*>().size() == 1
                && bindingItems(*tree, "viewport.show_grid").size() == 1,
            "adding alternatives preserves one row and one visible shortcut editor");
        selector->setCurrentIndex(0);
        check(shortcutEditor(controls)->keySequence() == expected[0],
            "switching alternatives retains the previous edited shortcuts");
        auto* const box = dialog.findChild<QDialogButtonBox*>();
        auto* const ok = box ? box->button(QDialogButtonBox::Ok) : nullptr;
        if ( ok && ok->isEnabled() )
        {
            changed = true;
            ok->click();
        }
        else
        {
            dialog.reject();
        }
    });
    QAction* const action = window.findChild<QAction*>("viewport.show_grid");
    check(changed && action && action->shortcuts() == expected,
        "arbitrary shortcut alternatives publish to the same live QAction");
    MainWindow reopened(nullptr, true);
    QAction* const restored = reopened.findChild<QAction*>("viewport.show_grid");
    check(changed && restored && restored->shortcuts() == expected,
        "compact shortcut alternatives survive settings reconstruction");
    drivePreferences(reopened, [&](QDialog& dialog)
    {
        auto* const tree = dialog.findChild<QTreeWidget*>("pref_keymap_tree");
        const auto items = tree ? bindingItems(*tree, "viewport.show_grid")
            : std::vector<QTreeWidgetItem*>{};
        QWidget* const controls = items.size() == 1 ? tree->itemWidget(items.front(), 1) : nullptr;
        auto* const selector = controls ? controls->findChild<QComboBox*>("pref_keymap_alternative") : nullptr;
        auto* const remove = controls ? controls->findChild<QToolButton*>("pref_keymap_remove") : nullptr;
        if ( selector && remove )
        {
            selector->setCurrentIndex(1);
            remove->click();
            check(selector->count() == 2 && shortcutEditor(controls)
                    && shortcutEditor(controls)->keySequence() == expected[2],
                "removing an alternative retains and selects its surviving neighbor");
            auto* const box = dialog.findChild<QDialogButtonBox*>();
            if ( auto* const ok = box ? box->button(QDialogButtonBox::Ok) : nullptr )
            {
                ok->click();
            }
            else
            {
                dialog.reject();
            }
        }
        else
        {
            dialog.reject();
        }
    });
    check(changed && restored && restored->shortcuts() == QList<QKeySequence>{expected[0], expected[2]},
        "removing one shortcut preserves the other alternatives exactly");
}

void testShortcutEditorDisplaysSpaceSeparatedModifiers()
{
    NavigationSettingsFixture settingsIsolation{};
    const QKeySequence custom(Qt::ControlModifier | Qt::ShiftModifier | Qt::Key_Z);
    const QKeySequence alternative(Qt::ControlModifier | Qt::Key_Q);
    const QKeySequence ctrlPlus(Qt::ControlModifier | Qt::Key_Plus);
    const QKeySequence numpadPlus(int(Qt::KeypadModifier) | Qt::Key_Plus);
    const QKeySequence commaChord(
        Qt::ControlModifier | Qt::Key_K,
        Qt::ControlModifier | Qt::Key_Comma);
    const QKeySequence modifierOnly(int(Qt::ShiftModifier) | Qt::Key_Control);
    const QString canonical = custom.toString(QKeySequence::PortableText);
    const QString alternativeCanonical = alternative.toString(QKeySequence::PortableText);
    const QString ctrlPlusCanonical = ctrlPlus.toString(QKeySequence::PortableText);
    const QString numpadPlusCanonical = numpadPlus.toString(QKeySequence::PortableText);
    const QKeySequence duplicateModifier(int(Qt::ControlModifier | Qt::ShiftModifier)
        | Qt::Key_Shift);
    check(ui::shortcuts::keySequenceDisplayText(custom) == QStringLiteral("Ctrl Shift Z")
            && ui::shortcuts::keySequenceDisplayText(ctrlPlus) == QStringLiteral("Ctrl +")
            && ui::shortcuts::keySequenceDisplayText(numpadPlus) == QStringLiteral("Numpad +")
            && ui::shortcuts::keySequenceDisplayText(commaChord)
                == QStringLiteral("Ctrl K, Ctrl Comma")
            && ui::shortcuts::keySequenceDisplayText(modifierOnly)
                == QStringLiteral("Ctrl Shift")
            && ui::shortcuts::keySequenceDisplayText(duplicateModifier)
                == QStringLiteral("Ctrl Shift")
            && custom.toString(QKeySequence::PortableText) == canonical
            && ctrlPlus.toString(QKeySequence::PortableText) == ctrlPlusCanonical
            && numpadPlus.toString(QKeySequence::PortableText) == numpadPlusCanonical,
        "display formatting uses spaces, keeps Plus and keypad Plus as keys, labels comma chords, and leaves PortableText unchanged");

    {
        QSettings settings = settingsStore();
        settings.setValue(QStringLiteral("keymap/shortcuts/viewport.show_grid"),
            QStringList{canonical, alternativeCanonical});
        settings.sync();
    }

    MainWindow window(nullptr, true);
    QAction* const gridAction = window.findChild<QAction*>("viewport.show_grid");
    const QList<QKeySequence> original = {custom, alternative};
    check(gridAction && gridAction->shortcuts() == original,
        "custom shortcut test preloads the canonical key sequence into the live action");
    bool inspected = drivePreferences(window, [&](QDialog& dialog)
    {
        auto* const tree = dialog.findChild<QTreeWidget*>("pref_keymap_tree");
        const auto rows = tree ? bindingItems(*tree, "viewport.show_grid")
                               : std::vector<QTreeWidgetItem*>{};
        QWidget* const controls = rows.size() == 1
            ? tree->itemWidget(rows.front(), 1) : nullptr;
        QKeySequenceEdit* const editor = shortcutEditor(controls);
        QLineEdit* const display = shortcutDisplay(editor);
        QComboBox* const selector = controls
            ? controls->findChild<QComboBox*>("pref_keymap_alternative") : nullptr;
        check(editor && display && editor->keySequence() == custom
                && display->text() == QStringLiteral("Ctrl Shift Z")
                && editor->keySequence().toString(QKeySequence::PortableText) == canonical,
            "Preferences displays modifier combinations with spaces while retaining the exact QKeySequence");

        if (editor && display)
        {
            {
                const QSignalBlocker blocker(editor);
                editor->setKeySequence(custom);
            }
            check(display->text() == QStringLiteral("Ctrl Shift Z"),
                "same-value blocked QKeySequenceEdit reset refreshes the actual native line edit");

            editor->setKeySequence(ctrlPlus);
            check(editor->keySequence() == ctrlPlus
                    && display->text() == QStringLiteral("Ctrl +")
                    && editor->keySequence().toString(QKeySequence::PortableText)
                        == ctrlPlusCanonical,
                "editing a literal plus keeps the key visible and the canonical sequence intact");
            editor->setKeySequence(numpadPlus);
            check(editor->keySequence() == numpadPlus
                    && display->text() == QStringLiteral("Numpad +")
                    && editor->keySequence().toString(QKeySequence::PortableText)
                        == numpadPlusCanonical,
                "the real shortcut line edit distinguishes keypad Plus without changing its stored sequence");
            editor->setKeySequence(commaChord);
            check(editor->keySequence() == commaChord
                    && display->text() == QStringLiteral("Ctrl K, Ctrl Comma")
                    && editor->keySequence().toString(QKeySequence::PortableText)
                        == commaChord.toString(QKeySequence::PortableText),
                "multi-stroke comma shortcuts remain distinguishable from the chord separator");
            editor->clear();
            check(editor->keySequence().isEmpty() && display->text().isEmpty(),
                "clearing the native shortcut editor clears its displayed key combination");
            editor->setKeySequence(custom);
        }

        if (selector && selector->count() == 2)
        {
            selector->setCurrentIndex(1);
            check(editor && display && editor->keySequence() == alternative
                    && display->text() == QStringLiteral("Ctrl Q"),
                "switching a blocked alternative setter updates the actual line edit display");
            selector->setCurrentIndex(0);
            check(editor && display && editor->keySequence() == custom
                    && display->text() == QStringLiteral("Ctrl Shift Z"),
                "switching back to an alternative restores the formatted selected sequence");
        }

        if (editor && display)
        {
            // Check the recording buffer before Qt publishes the final
            // QKeySequenceEdit value, including its visible next-chord marker.
            const bool keymapSelected = selectPreferenceCategory(
                dialog, QStringLiteral("Keymap"));
            if (tree && !rows.empty())
            {
                for (QTreeWidgetItem* ancestor = rows.front(); ancestor;
                     ancestor = ancestor->parent())
                {
                    ancestor->setExpanded(true);
                }
                tree->scrollToItem(rows.front(), QAbstractItemView::PositionAtCenter);
            }
            QApplication::processEvents(QEventLoop::AllEvents);
            editor->setFocus(Qt::OtherFocusReason);
            QApplication::processEvents(QEventLoop::AllEvents);
            QWidget* const focusedWidget = QApplication::focusWidget();
            const bool editorOwnsFocus = focusedWidget == editor
                || (focusedWidget && editor->isAncestorOf(focusedWidget));
            check(keymapSelected && editor->isVisible() && display->isVisible()
                    && editorOwnsFocus,
                "native shortcut recording uses the visible, scrolled Keymap editor with real focus");
            editor->clear();
            sendNavigationKey(*editor, Qt::Key_Z,
                Qt::ControlModifier | Qt::ShiftModifier, false, QStringLiteral("Z"));
            const QString immediateDisplay = display->text();
            QApplication::processEvents();
            const QString pendingDisplay = display->text();
            const bool pendingPrefix = pendingDisplay.startsWith(
                QStringLiteral("Ctrl Shift Z"));
            const bool pendingSuffix = pendingDisplay.contains(QStringLiteral("..."));
            if (!pendingPrefix || !pendingSuffix)
            {
                const QByteArray immediateUtf8 = immediateDisplay.toUtf8();
                const QByteArray pendingUtf8 = pendingDisplay.toUtf8();
                const QByteArray nativeSequenceUtf8 =
                    editor->keySequence().toString(QKeySequence::NativeText).toUtf8();
                std::fprintf(stderr,
                    "NAV_KEY_SEQUENCE_PENDING displayImmediate='%s' displayAfterEvents='%s' "
                    "nativeKeySequence='%s' editorVisible=%d lineEditVisible=%d "
                    "editorOwnsFocus=%d maxLength=%d\n",
                    immediateUtf8.constData(), pendingUtf8.constData(),
                    nativeSequenceUtf8.constData(), editor->isVisible(),
                    display->isVisible(), editorOwnsFocus,
                    static_cast<int>(editor->maximumSequenceLength()));
            }
            check(pendingPrefix,
                "native Ctrl Shift Z recording formats its in-progress line edit");
            check(pendingSuffix,
                "native shortcut recording keeps Qt's visible next-chord suffix");
            QEventLoop finishSingleStroke{};
            const auto singleStrokeConnection = QObject::connect(
                editor, &QKeySequenceEdit::keySequenceChanged, &finishSingleStroke,
                &QEventLoop::quit);
            QTimer::singleShot(1200, &finishSingleStroke, &QEventLoop::quit);
            finishSingleStroke.exec();
            QObject::disconnect(singleStrokeConnection);
            check(editor->keySequence() == custom
                    && editor->keySequence().toString(QKeySequence::PortableText) == canonical
                    && display->text() == QStringLiteral("Ctrl Shift Z"),
                "native single-stroke completion preserves its canonical shortcut and removes the temporary suffix");

            editor->clear();
            sendNavigationKey(*editor, Qt::Key_K, Qt::ControlModifier, false, QStringLiteral("k"));
            QApplication::processEvents();
            check(display->text().startsWith(QStringLiteral("Ctrl K"))
                    && display->text().contains(QStringLiteral("...")),
                "native first chord displays as Ctrl K while QKeySequenceEdit awaits another stroke");
            sendNavigationKey(*editor, Qt::Key_Comma, Qt::ControlModifier,
                false, QStringLiteral(","));
            QApplication::processEvents();
            check(display->text().startsWith(QStringLiteral("Ctrl K, Ctrl Comma"))
                    && display->text().contains(QStringLiteral("...")),
                "native second chord preserves both the chord separator and the literal Comma key");
            QEventLoop finishChord{};
            const auto chordConnection = QObject::connect(
                editor, &QKeySequenceEdit::keySequenceChanged, &finishChord,
                &QEventLoop::quit);
            QTimer::singleShot(1200, &finishChord, &QEventLoop::quit);
            finishChord.exec();
            QObject::disconnect(chordConnection);
            check(editor->keySequence() == commaChord
                    && editor->keySequence().toString(QKeySequence::PortableText)
                        == commaChord.toString(QKeySequence::PortableText)
                    && display->text() == QStringLiteral("Ctrl K, Ctrl Comma"),
                "native chord recording finishes with its exact PortableText sequence");
        }
        dialog.reject();
    });
    const QSettings settings = settingsStore();
    check(inspected && gridAction && gridAction->shortcuts() == original
            && settings.value(QStringLiteral("keymap/shortcuts/viewport.show_grid")).toStringList()
                == QStringList{canonical, alternativeCanonical},
        "opening and cancelling Preferences preserves the canonical portable shortcut input");
}

void testShortcutMenuDisplayUsesPaintedStyleOption()
{
    const ScopedMenuAnimationDisable disableMenuAnimations{};
    const QKeySequence original(Qt::ControlModifier | Qt::ShiftModifier | Qt::Key_Z);
    const QKeySequence remapped(Qt::ControlModifier | Qt::AltModifier | Qt::Key_Z);
    const QKeySequence numpadPlus(int(Qt::KeypadModifier) | Qt::Key_Plus);
    ui::shortcuts::KeySequenceDisplayMenu menu{};
    QAction* const bound = menu.addAction(QStringLiteral("Visible command"));
    bound->setShortcut(original);
    bound->setShortcutVisibleInContextMenu(true);
    QAction* const nativeTextOnly = menu.addAction(QStringLiteral("&Undo\tCtrl+Z"));
    nativeTextOnly->setShortcutVisibleInContextMenu(true);

    showMenuForDisplayTest(menu);
    const qreal beforeOpacity = menu.windowOpacity();
    const bool beforePopupReady = menu.isVisible()
        && beforeOpacity >= 0.999;
    const QImage before = captureShownMenuForDisplayTest(menu);
    const QSize beforeMenuSize = menu.size();
    const QRect beforeActionRect = menu.actionGeometry(bound);
    bound->setShortcut(remapped);
    menu.update();
    QApplication::processEvents(QEventLoop::AllEvents);
    const qreal afterOpacity = menu.windowOpacity();
    const bool afterPopupReady = menu.isVisible()
        && afterOpacity >= 0.999;
    const QImage after = captureShownMenuForDisplayTest(menu);
    const QSize afterMenuSize = menu.size();
    const QRect afterActionRect = menu.actionGeometry(bound);
    menu.hide();
    QApplication::processEvents(QEventLoop::AllEvents);
    QMenu expectedBeforeMenu{};
    QAction* const expectedBeforeAction =
        expectedBeforeMenu.addAction(QStringLiteral("Visible command\tCtrl Shift Z"));
    expectedBeforeAction->setShortcutVisibleInContextMenu(true);
    QAction* const expectedBeforeUndo =
        expectedBeforeMenu.addAction(QStringLiteral("&Undo\tCtrl+Z"));
    expectedBeforeUndo->setShortcutVisibleInContextMenu(true);
    const QImage expectedBeforeRaster = renderMenuForDisplayTest(expectedBeforeMenu);
    const QRect expectedBeforeActionRect =
        expectedBeforeMenu.actionGeometry(expectedBeforeAction);
    QMenu expectedAfterMenu{};
    QAction* const expectedAfterAction =
        expectedAfterMenu.addAction(QStringLiteral("Visible command\tCtrl Alt Z"));
    expectedAfterAction->setShortcutVisibleInContextMenu(true);
    QAction* const expectedAfterUndo =
        expectedAfterMenu.addAction(QStringLiteral("&Undo\tCtrl+Z"));
    expectedAfterUndo->setShortcutVisibleInContextMenu(true);
    const QImage expectedAfterRaster = renderMenuForDisplayTest(expectedAfterMenu);
    const QRect expectedAfterActionRect =
        expectedAfterMenu.actionGeometry(expectedAfterAction);
    ui::shortcuts::KeySequenceDisplayMenu nativeContextMenu{};
    QAction* const nativeContextAction =
        nativeContextMenu.addAction(QStringLiteral("&Undo\tCtrl+Z"));
    nativeContextAction->setShortcutVisibleInContextMenu(true);
    const QImage nativeContextRaster = renderMenuForDisplayTest(nativeContextMenu);
    const QRect nativeContextActionRect =
        nativeContextMenu.actionGeometry(nativeContextAction);
    NativeShortcutReferenceMenu expectedContextMenu(QStringLiteral("Ctrl Z"));
    QAction* const expectedContextAction =
        expectedContextMenu.addAction(QStringLiteral("&Undo\tCtrl+Z"));
    expectedContextAction->setShortcutVisibleInContextMenu(true);
    const QImage expectedContextRaster = renderMenuForDisplayTest(expectedContextMenu);
    const QRect expectedContextActionRect =
        expectedContextMenu.actionGeometry(expectedContextAction);

    ui::shortcuts::KeySequenceDisplayMenu numpadMenu{};
    QAction* const numpadAction = numpadMenu.addAction(QStringLiteral("Numpad command"));
    numpadAction->setShortcut(numpadPlus);
    numpadAction->setShortcutVisibleInContextMenu(true);
    const QImage numpadRaster = renderMenuForDisplayTest(numpadMenu, true);
    const QRect numpadActionRect = numpadMenu.actionGeometry(numpadAction);
    QMenu expectedNumpadMenu{};
    QAction* const expectedNumpadAction =
        expectedNumpadMenu.addAction(QStringLiteral("Numpad command\tNumpad +"));
    expectedNumpadAction->setShortcutVisibleInContextMenu(true);
    const QImage expectedNumpadRaster = renderMenuForDisplayTest(expectedNumpadMenu, true);
    const QRect expectedNumpadActionRect =
        expectedNumpadMenu.actionGeometry(expectedNumpadAction);
    const QFontMetrics menuFont(numpadMenu.font());
    const int expectedNumpadRowWidth =
        menuFont.horizontalAdvance(QStringLiteral("Numpad command"))
        + menuFont.horizontalAdvance(QStringLiteral("Numpad +")) + 30;

    QStyleOptionMenuItem option{};
    option.text = QStringLiteral("Command\tCtrl+Z");
    option.fontMetrics = QFontMetrics(QApplication::font());
    option.reservedShortcutWidth = option.fontMetrics.horizontalAdvance(QStringLiteral("Ctrl+Z"));
    QAction wideShortcut(QStringLiteral("Command"));
    wideShortcut.setShortcut(numpadPlus);
    ui::shortcuts::formatMenuShortcutOption(&option, &wideShortcut);
    QStyleOptionMenuItem beforeOption{};
    beforeOption.text = QStringLiteral("Visible command\t")
        + original.toString(QKeySequence::NativeText);
    beforeOption.fontMetrics = QFontMetrics(menu.font());
    beforeOption.reservedShortcutWidth = beforeOption.fontMetrics.horizontalAdvance(
        original.toString(QKeySequence::NativeText));
    QAction originalShortcut(QStringLiteral("Visible command"));
    originalShortcut.setShortcut(original);
    ui::shortcuts::formatMenuShortcutOption(&beforeOption, &originalShortcut);
    QStyleOptionMenuItem afterOption{};
    afterOption.text = QStringLiteral("Visible command\t")
        + remapped.toString(QKeySequence::NativeText);
    afterOption.fontMetrics = beforeOption.fontMetrics;
    afterOption.reservedShortcutWidth = afterOption.fontMetrics.horizontalAdvance(
        remapped.toString(QKeySequence::NativeText));
    ui::shortcuts::formatMenuShortcutOption(&afterOption, bound);
    const auto foregroundPixels = [](const QImage& image, const QRect& rect)
    {
        const QRect clipped = imageRectForLogicalRect(image, rect).intersected(image.rect());
        int count = 0;
        for (int y = clipped.top() + 2; y < clipped.bottom() - 1; ++y)
        {
            for (int x = clipped.left() + 2; x < clipped.right() - 1; ++x)
            {
                if (qGray(image.pixel(x, y)) > 160)
                {
                    ++count;
                }
            }
        }
        return count;
    };
    const auto rightColumnDifferencePixels = [](const QImage& lhs, const QImage& rhs)
    {
        if (lhs.size() != rhs.size() || lhs.isNull())
        {
            return 0;
        }
        const int xStart = lhs.width() / 2;
        int count = 0;
        for (int y = 2; y < lhs.height() - 1; ++y)
        {
            for (int x = xStart; x < lhs.width() - 2; ++x)
            {
                if (lhs.pixel(x, y) != rhs.pixel(x, y))
                {
                    ++count;
                }
            }
        }
        return count;
    };
    const int beforeRowForeground = foregroundPixels(before, beforeActionRect);
    const int afterRowForeground = foregroundPixels(after, afterActionRect);
    const int beforeShortcutForeground = foregroundPixels(before,
        QRect(beforeActionRect.left() + beforeActionRect.width() / 2,
            beforeActionRect.top(), beforeActionRect.width() / 2,
            beforeActionRect.height()));
    const int afterShortcutForeground = foregroundPixels(after,
        QRect(afterActionRect.left() + afterActionRect.width() / 2,
            afterActionRect.top(), afterActionRect.width() / 2,
            afterActionRect.height()));
    const int changedShortcutPixels = rightColumnDifferencePixels(before, after);
    const QImage beforeShortcutGlyphs =
        shortcutGlyphCrop(before, beforeActionRect);
    const QImage afterShortcutGlyphs =
        shortcutGlyphCrop(after, afterActionRect);
    const QImage expectedBeforeShortcutGlyphs =
        shortcutGlyphCrop(expectedBeforeRaster, expectedBeforeActionRect);
    const QImage expectedAfterShortcutGlyphs =
        shortcutGlyphCrop(expectedAfterRaster, expectedAfterActionRect);
    const QImage nativeContextShortcutGlyphs =
        shortcutGlyphCrop(nativeContextRaster, nativeContextActionRect);
    const QImage expectedContextShortcutGlyphs =
        shortcutGlyphCrop(expectedContextRaster, expectedContextActionRect);
    const QImage numpadShortcutGlyphs =
        shortcutGlyphCrop(numpadRaster, numpadActionRect);
    const QImage expectedNumpadShortcutGlyphs =
        shortcutGlyphCrop(expectedNumpadRaster, expectedNumpadActionRect);
    const int nativeContextShortcutForeground = foregroundPixels(
        nativeContextRaster, QRect(nativeContextActionRect.left()
                + nativeContextActionRect.width() / 2,
            nativeContextActionRect.top(), nativeContextActionRect.width() / 2,
            nativeContextActionRect.height()));
    const int expectedContextShortcutForeground = foregroundPixels(
        expectedContextRaster, QRect(expectedContextActionRect.left()
                + expectedContextActionRect.width() / 2,
            expectedContextActionRect.top(), expectedContextActionRect.width() / 2,
            expectedContextActionRect.height()));
    const int numpadShortcutForeground = foregroundPixels(numpadRaster,
        QRect(numpadActionRect.left() + numpadActionRect.width() / 2,
            numpadActionRect.top(), numpadActionRect.width() / 2,
            numpadActionRect.height()));
    const int expectedNumpadShortcutForeground = foregroundPixels(
        expectedNumpadRaster, QRect(expectedNumpadActionRect.left()
                + expectedNumpadActionRect.width() / 2,
            expectedNumpadActionRect.top(), expectedNumpadActionRect.width() / 2,
            expectedNumpadActionRect.height()));
    const bool boundRasterChanges = !before.isNull() && !after.isNull()
        && before != after;
    const bool boundShortcutGlyphsDiffer = beforeShortcutForeground > 0
        && afterShortcutForeground > 0 && changedShortcutPixels > 0
        && !beforeShortcutGlyphs.isNull() && !afterShortcutGlyphs.isNull()
        && beforeShortcutGlyphs != afterShortcutGlyphs;
    const bool boundGlyphsMatchReference =
        beforeShortcutGlyphs == expectedBeforeShortcutGlyphs
        && afterShortcutGlyphs == expectedAfterShortcutGlyphs;
    const bool nativeActionUnchanged = menu.actions().size() == 2
        && menu.actions().front() == bound
        && bound->text() == QStringLiteral("Visible command")
        && bound->shortcut() == remapped
        && nativeTextOnly->text() == QStringLiteral("&Undo\tCtrl+Z")
        && nativeTextOnly->shortcut().isEmpty();
    const bool nativeContextRasterMatches =
        !nativeContextShortcutGlyphs.isNull()
        && nativeContextShortcutGlyphs == expectedContextShortcutGlyphs;
    const bool numpadRasterFits = !numpadRaster.isNull()
        && numpadRaster.width() / numpadRaster.devicePixelRatio()
            >= expectedNumpadRowWidth
        && numpadActionRect.width() >= expectedNumpadRowWidth
        && numpadShortcutForeground > 0
        && expectedNumpadShortcutForeground > 0
        && !numpadShortcutGlyphs.isNull()
        && numpadShortcutGlyphs == expectedNumpadShortcutGlyphs;
    const bool optionFormatsWideShortcut =
        option.text == QStringLiteral("Command\tNumpad +")
        && option.reservedShortcutWidth
            >= option.fontMetrics.horizontalAdvance(QStringLiteral("Numpad +"));
    const bool boundOptionsFormatted =
        beforeOption.text == QStringLiteral("Visible command\tCtrl Shift Z")
        && afterOption.text == QStringLiteral("Visible command\tCtrl Alt Z");
    const bool allPaintedRowsVisible = beforePopupReady && afterPopupReady
        && beforeRowForeground > 0 && afterRowForeground > 0
        && nativeContextShortcutForeground > 0
        && expectedContextShortcutForeground > 0
        && numpadShortcutForeground > 0
        && expectedNumpadShortcutForeground > 0;
    if (!boundRasterChanges || !boundShortcutGlyphsDiffer
        || !boundGlyphsMatchReference || !nativeActionUnchanged
        || !nativeContextRasterMatches || !numpadRasterFits
        || !optionFormatsWideShortcut || !boundOptionsFormatted
        || !allPaintedRowsVisible)
    {
        QTemporaryDir diagnosticImages{};
        diagnosticImages.setAutoRemove(false);
        const bool beforeSaved = diagnosticImages.isValid()
            && before.save(diagnosticImages.filePath(QStringLiteral("before.png")));
        const bool afterSaved = diagnosticImages.isValid()
            && after.save(diagnosticImages.filePath(QStringLiteral("after.png")));
        const bool contextNativeSaved = diagnosticImages.isValid()
            && nativeContextRaster.save(
                diagnosticImages.filePath(QStringLiteral("context-native.png")));
        const bool contextExpectedSaved = diagnosticImages.isValid()
            && expectedContextRaster.save(
                diagnosticImages.filePath(QStringLiteral("context-expected.png")));
        const bool contextNativeGlyphSaved = diagnosticImages.isValid()
            && nativeContextShortcutGlyphs.save(
                diagnosticImages.filePath(QStringLiteral("context-native-glyphs.png")));
        const bool contextExpectedGlyphSaved = diagnosticImages.isValid()
            && expectedContextShortcutGlyphs.save(
                diagnosticImages.filePath(QStringLiteral("context-expected-glyphs.png")));
        const bool numpadActualSaved = diagnosticImages.isValid()
            && numpadRaster.save(
                diagnosticImages.filePath(QStringLiteral("numpad-actual.png")));
        const bool numpadExpectedSaved = diagnosticImages.isValid()
            && expectedNumpadRaster.save(
                diagnosticImages.filePath(QStringLiteral("numpad-expected.png")));
        const bool numpadActualGlyphSaved = diagnosticImages.isValid()
            && numpadShortcutGlyphs.save(
                diagnosticImages.filePath(QStringLiteral("numpad-actual-glyphs.png")));
        const bool numpadExpectedGlyphSaved = diagnosticImages.isValid()
            && expectedNumpadShortcutGlyphs.save(
                diagnosticImages.filePath(QStringLiteral("numpad-expected-glyphs.png")));
        const QByteArray beforeOptionUtf8 = beforeOption.text.toUtf8();
        const QByteArray afterOptionUtf8 = afterOption.text.toUtf8();
        std::fprintf(stderr,
            "NAV_MENU_DISPLAY boundBeforeNull=%d boundAfterNull=%d boundRasterChanges=%d "
            "nativeActionUnchanged=%d nativeContextRasterMatches=%d "
            "popupReady=%d,%d opacity=%.3f,%.3f "
            "paintOnScreen=%d translucent=%d opaquePaint=%d noSystemBackground=%d "
            "beforeSize=%dx%d beforeRect=%d,%d,%d,%d rowForeground=%d shortcutForeground=%d "
            "afterSize=%dx%d afterRect=%d,%d,%d,%d rowForeground=%d shortcutForeground=%d "
            "changedShortcutPixels=%d visibleInContext=%d appHidesContextShortcuts=%d "
            "contextSize=%dx%d expectedContextSize=%dx%d contextDpr=%.3f,%.3f "
            "contextRect=%d,%d,%d,%d expectedContextRect=%d,%d,%d,%d "
            "numpadSize=%dx%d expectedNumpadSize=%dx%d numpadDpr=%.3f,%.3f "
            "numpadRect=%d,%d,%d,%d expectedNumpadRect=%d,%d,%d,%d "
            "expectedNumpadWidth=%d optionText='%s' reservedShortcutWidth=%d "
            "formattedNumpadWidth=%d "
            "beforeOption='%s' afterOption='%s' "
            "boundShortcutNative='%s' imagesSaved=%d,%d "
            "contextImagesSaved=%d,%d,%d,%d numpadImagesSaved=%d,%d,%d,%d path='%s'\n",
            before.isNull(), after.isNull(), boundRasterChanges,
            nativeActionUnchanged, nativeContextRasterMatches,
            beforePopupReady, afterPopupReady,
            static_cast<double>(beforeOpacity), static_cast<double>(afterOpacity),
            menu.testAttribute(Qt::WA_PaintOnScreen),
            menu.testAttribute(Qt::WA_TranslucentBackground),
            menu.testAttribute(Qt::WA_OpaquePaintEvent),
            menu.testAttribute(Qt::WA_NoSystemBackground),
            beforeMenuSize.width(), beforeMenuSize.height(),
            beforeActionRect.x(), beforeActionRect.y(),
            beforeActionRect.width(), beforeActionRect.height(),
            beforeRowForeground, beforeShortcutForeground,
            afterMenuSize.width(), afterMenuSize.height(),
            afterActionRect.x(), afterActionRect.y(),
            afterActionRect.width(), afterActionRect.height(),
            afterRowForeground, afterShortcutForeground, changedShortcutPixels,
            bound->isShortcutVisibleInContextMenu(),
            QApplication::testAttribute(
                Qt::AA_DontShowShortcutsInContextMenus),
            nativeContextRaster.width(), nativeContextRaster.height(),
            expectedContextRaster.width(), expectedContextRaster.height(),
            static_cast<double>(nativeContextMenu.devicePixelRatioF()),
            static_cast<double>(expectedContextMenu.devicePixelRatioF()),
            nativeContextActionRect.x(), nativeContextActionRect.y(),
            nativeContextActionRect.width(), nativeContextActionRect.height(),
            expectedContextActionRect.x(), expectedContextActionRect.y(),
            expectedContextActionRect.width(), expectedContextActionRect.height(),
            numpadRaster.width(), numpadRaster.height(),
            expectedNumpadRaster.width(), expectedNumpadRaster.height(),
            static_cast<double>(numpadMenu.devicePixelRatioF()),
            static_cast<double>(expectedNumpadMenu.devicePixelRatioF()),
            numpadActionRect.x(), numpadActionRect.y(),
            numpadActionRect.width(), numpadActionRect.height(),
            expectedNumpadActionRect.x(), expectedNumpadActionRect.y(),
            expectedNumpadActionRect.width(), expectedNumpadActionRect.height(),
            expectedNumpadRowWidth,
            qPrintable(option.text), option.reservedShortcutWidth,
            option.fontMetrics.horizontalAdvance(QStringLiteral("Numpad +")),
            beforeOptionUtf8.constData(), afterOptionUtf8.constData(),
            qPrintable(bound->shortcut().toString(QKeySequence::NativeText)),
            beforeSaved, afterSaved,
            contextNativeSaved, contextExpectedSaved,
            contextNativeGlyphSaved, contextExpectedGlyphSaved,
            numpadActualSaved, numpadExpectedSaved,
            numpadActualGlyphSaved, numpadExpectedGlyphSaved,
            qPrintable(diagnosticImages.path()));
        std::fprintf(stderr,
            "NAV_MENU_PIXELS popupReady=%d,%d boundRows=%d,%d boundShortcut=%d,%d "
            "boundGlyphsMatchReference=%d contextShortcut=%d expectedContextShortcut=%d "
            "contextGlyphCropNull=%d,%d contextGlyphsMatch=%d "
            "numpadShortcut=%d expectedNumpadShortcut=%d "
            "numpadGlyphCropNull=%d,%d numpadGlyphsMatch=%d\n",
            beforePopupReady, afterPopupReady, beforeRowForeground,
            afterRowForeground, beforeShortcutForeground,
            afterShortcutForeground, boundGlyphsMatchReference,
            nativeContextShortcutForeground, expectedContextShortcutForeground,
            nativeContextShortcutGlyphs.isNull(),
            expectedContextShortcutGlyphs.isNull(), nativeContextRasterMatches,
            numpadShortcutForeground, expectedNumpadShortcutForeground,
            numpadShortcutGlyphs.isNull(), expectedNumpadShortcutGlyphs.isNull(),
            numpadShortcutGlyphs == expectedNumpadShortcutGlyphs);
    }
    check(boundRasterChanges,
        "styled menu raster shows changed bound shortcut text without mutating its action");
    check(boundShortcutGlyphsDiffer,
        "styled menu raster paints distinct visible shortcut glyphs for Ctrl Shift Z and Ctrl Alt Z");
    check(boundGlyphsMatchReference,
        "styled menu raster glyphs match the exact Ctrl Shift Z and Ctrl Alt Z reference captions");
    check(nativeActionUnchanged,
        "native-context menu formatting leaves action captions and shortcuts unchanged");
    check(nativeContextRasterMatches,
        "native-context menu shortcut glyphs match the exact formatted reference caption");
    check(numpadRasterFits,
        "naturally sized menu paints the complete Numpad suffix within its width");
    check(optionFormatsWideShortcut,
        "painted style option reserves the full width of the displayed Numpad suffix");
    check(boundOptionsFormatted,
        "painted style options format Ctrl Shift Z and Ctrl Alt Z distinctly");
    check(allPaintedRowsVisible,
        "animated menu popup renders visible shortcut glyphs in bound, native-context, and Numpad rows");
}

void testShortcutMenuNaturalWidthTracksFormattedColumn()
{
    const ScopedMenuAnimationDisable disableMenuAnimations{};
    const QKeySequence longSequence(int(Qt::KeypadModifier) | Qt::Key_F12);
    const QKeySequence shortSequence(Qt::Key_F1);
    ui::shortcuts::KeySequenceDisplayMenu menu{};
    QAction* const action = menu.addAction(QStringLiteral("Width probe"));
    action->setShortcut(longSequence);
    action->setShortcutVisibleInContextMenu(true);
    const QFontMetrics widthProbeFont(menu.font());
    const int longNativeAdvance = widthProbeFont.horizontalAdvance(
        longSequence.toString(QKeySequence::NativeText));
    const int longFormattedAdvance = widthProbeFont.horizontalAdvance(
        ui::shortcuts::keySequenceDisplayText(longSequence));
    const bool longSequenceHasPositiveWidthDelta =
        longFormattedAdvance > longNativeAdvance;

    showMenuForDisplayTest(menu, true);
    const QSize firstPopupSize = menu.size();
    const int firstMinimumWidth = menu.minimumWidth();
    menu.hide();
    QApplication::processEvents(QEventLoop::AllEvents);
    showMenuForDisplayTest(menu, true);
    const QSize repeatedPopupSize = menu.size();
    const int repeatedMinimumWidth = menu.minimumWidth();
    menu.hide();
    QApplication::processEvents(QEventLoop::AllEvents);

    const QSize longHint = menu.sizeHint();
    const int longMinimumWidth = menu.minimumWidth();
    const QSize repeatedLongHint = menu.sizeHint();
    const int repeatedLongMinimumWidth = menu.minimumWidth();
    action->setShortcut(shortSequence);
    const QSize shortHint = menu.sizeHint();
    const int shortMinimumWidth = menu.minimumWidth();

    ui::shortcuts::KeySequenceDisplayMenu visibleOnly{};
    QAction* const visibleAction = visibleOnly.addAction(QStringLiteral("Visible"));
    visibleAction->setShortcut(longSequence);
    visibleAction->setShortcutVisibleInContextMenu(true);
    const int visibleOnlyWidth = visibleOnly.sizeHint().width();
    ui::shortcuts::KeySequenceDisplayMenu withInvisible{};
    QAction* const sameVisibleAction = withInvisible.addAction(QStringLiteral("Visible"));
    sameVisibleAction->setShortcut(longSequence);
    sameVisibleAction->setShortcutVisibleInContextMenu(true);
    QAction* const hiddenAction = withInvisible.addAction(QStringLiteral("Hidden"));
    hiddenAction->setShortcut(QKeySequence(
        Qt::ControlModifier | Qt::ShiftModifier | Qt::AltModifier | Qt::Key_F24));
    hiddenAction->setShortcutVisibleInContextMenu(true);
    hiddenAction->setVisible(false);
    const int withInvisibleWidth = withInvisible.sizeHint().width();

    ui::shortcuts::KeySequenceDisplayMenu fixedWidth{};
    QAction* const fixedAction = fixedWidth.addAction(QStringLiteral("Fixed"));
    fixedAction->setShortcut(longSequence);
    fixedAction->setShortcutVisibleInContextMenu(true);
    fixedWidth.setFixedWidth(321);
    (void)fixedWidth.sizeHint();

    ui::shortcuts::KeySequenceDisplayMenu initialCallerMinimum{};
    initialCallerMinimum.setMinimumWidth(350);
    QAction* const initialMinimumAction =
        initialCallerMinimum.addAction(QStringLiteral("Initial minimum"));
    initialMinimumAction->setShortcut(longSequence);
    initialMinimumAction->setShortcutVisibleInContextMenu(true);
    const QSize initialCallerMinimumHint = initialCallerMinimum.sizeHint();

    ui::shortcuts::KeySequenceDisplayMenu callerMinimum{};
    QAction* const minimumAction = callerMinimum.addAction(QStringLiteral("Minimum"));
    minimumAction->setShortcut(longSequence);
    minimumAction->setShortcutVisibleInContextMenu(true);
    callerMinimum.setMinimumWidth(350);
    (void)callerMinimum.sizeHint();
    const int callerMinimumAfterLong = callerMinimum.minimumWidth();
    minimumAction->setShortcut(shortSequence);
    (void)callerMinimum.sizeHint();

    ui::shortcuts::KeySequenceDisplayMenu loweredCallerMinimum{};
    QAction* const loweredMinimumAction =
        loweredCallerMinimum.addAction(QStringLiteral("Lowered minimum"));
    loweredMinimumAction->setShortcut(longSequence);
    loweredMinimumAction->setShortcutVisibleInContextMenu(true);
    loweredCallerMinimum.setMinimumWidth(350);
    showMenuForDisplayTest(loweredCallerMinimum, true);
    const int originalCallerFloor = loweredCallerMinimum.minimumWidth();
    loweredCallerMinimum.setMinimumWidth(200);
    const QSize loweredCallerFloorHint = loweredCallerMinimum.sizeHint();
    const int loweredCallerFloorValue = loweredCallerMinimum.minimumWidth();
    loweredCallerMinimum.hide();
    QApplication::processEvents(QEventLoop::AllEvents);

    ui::shortcuts::KeySequenceDisplayMenu maximumBound{};
    QAction* const maximumAction = maximumBound.addAction(QStringLiteral("Maximum"));
    maximumAction->setShortcut(longSequence);
    maximumAction->setShortcutVisibleInContextMenu(true);
    maximumBound.setMaximumWidth(150);
    const QSize maximumBoundHint = maximumBound.sizeHint();

    const QKeySequence originalSameWidth(int(Qt::KeypadModifier) | Qt::Key_1);
    const QKeySequence updatedSameWidth(int(Qt::KeypadModifier) | Qt::Key_2);
    QMenu nativeSameSizeWidthReference{};
    QAction* const nativeSameSizeWidthAction = nativeSameSizeWidthReference.addAction(
        QStringLiteral("Width probe\t")
            + originalSameWidth.toString(QKeySequence::NativeText));
    nativeSameSizeWidthAction->setShortcutVisibleInContextMenu(true);
    const int nativeSameSizeWidth = nativeSameSizeWidthReference.sizeHint().width();
    ui::shortcuts::KeySequenceDisplayMenu sameSizeMenu{};
    QAction* const sameSizeAction = sameSizeMenu.addAction(QStringLiteral("Width probe"));
    sameSizeAction->setShortcut(originalSameWidth);
    sameSizeAction->setShortcutVisibleInContextMenu(true);
    const QFontMetrics sameSizeMenuFont(sameSizeMenu.font());
    const int originalNativeAdvance = sameSizeMenuFont.horizontalAdvance(
        originalSameWidth.toString(QKeySequence::NativeText));
    const int updatedNativeAdvance = sameSizeMenuFont.horizontalAdvance(
        updatedSameWidth.toString(QKeySequence::NativeText));
    const int originalFormattedAdvance = sameSizeMenuFont.horizontalAdvance(
        ui::shortcuts::keySequenceDisplayText(originalSameWidth));
    const int updatedFormattedAdvance = sameSizeMenuFont.horizontalAdvance(
        ui::shortcuts::keySequenceDisplayText(updatedSameWidth));
    const bool sameWidthSequenceAdvances =
        originalNativeAdvance == updatedNativeAdvance
        && originalFormattedAdvance == updatedFormattedAdvance
        && originalFormattedAdvance > originalNativeAdvance;
    showMenuForDisplayTest(sameSizeMenu, true);
    const QSize originalSameWidthMenuSize = sameSizeMenu.size();
    const int originalSameWidthMinimum = sameSizeMenu.minimumWidth();
    const QRect originalSameWidthActionRect = sameSizeMenu.actionGeometry(sameSizeAction);
    const QImage originalSameWidthRaster = captureShownMenuForDisplayTest(sameSizeMenu);
    sameSizeMenu.hide();
    QApplication::processEvents(QEventLoop::AllEvents);
    sameSizeAction->setShortcut(updatedSameWidth);
    showMenuForDisplayTest(sameSizeMenu, true);
    const QSize updatedSameWidthMenuSize = sameSizeMenu.size();
    const int updatedSameWidthMinimum = sameSizeMenu.minimumWidth();
    const QRect updatedSameWidthActionRect = sameSizeMenu.actionGeometry(sameSizeAction);
    const QImage updatedSameWidthRaster = captureShownMenuForDisplayTest(sameSizeMenu);
    sameSizeMenu.hide();
    QApplication::processEvents(QEventLoop::AllEvents);
    NativeShortcutReferenceMenu sameSizeReference(QStringLiteral("Numpad 2"));
    QAction* const sameSizeReferenceAction = sameSizeReference.addAction(
        QStringLiteral("Width probe\t")
            + updatedSameWidth.toString(QKeySequence::NativeText));
    sameSizeReferenceAction->setShortcutVisibleInContextMenu(true);
    sameSizeReference.setFixedWidth(updatedSameWidthMenuSize.width());
    const QImage sameSizeReferenceRaster = renderMenuForDisplayTest(
        sameSizeReference, true);
    const QRect sameSizeReferenceActionRect =
        sameSizeReference.actionGeometry(sameSizeReferenceAction);
    const QImage updatedSameWidthGlyphs = shortcutGlyphCrop(
        updatedSameWidthRaster, updatedSameWidthActionRect);
    const QImage expectedSameWidthGlyphs = shortcutGlyphCrop(
        sameSizeReferenceRaster, sameSizeReferenceActionRect);
    const QImage originalSameWidthGlyphs = shortcutGlyphCrop(
        originalSameWidthRaster, originalSameWidthActionRect);
    const bool sameWidthRebindLayoutValid =
        sameWidthSequenceAdvances
        && originalSameWidthMenuSize.width() == updatedSameWidthMenuSize.width()
        && originalSameWidthMinimum > nativeSameSizeWidth
        && updatedSameWidthMinimum > nativeSameSizeWidth
        && originalSameWidthActionRect.width() == updatedSameWidthActionRect.width()
        && updatedSameWidthActionRect == sameSizeReferenceActionRect
        && !originalSameWidthGlyphs.isNull()
        && !updatedSameWidthGlyphs.isNull()
        && originalSameWidthGlyphs != updatedSameWidthGlyphs
        && updatedSameWidthGlyphs == expectedSameWidthGlyphs;

    if ( !sameWidthRebindLayoutValid )
    {
        std::fprintf(stderr,
            "same-width menu diagnostic: advances=%d native=%d/%d formatted=%d/%d "
            "minimum=%d/%d nativeHint=%d menuWidth=%d/%d rect=%d,%d,%d,%d "
            "referenceRect=%d,%d,%d,%d glyphNull=%d/%d/%d glyphChanged=%d glyphMatches=%d\n",
            sameWidthSequenceAdvances, originalNativeAdvance, updatedNativeAdvance,
            originalFormattedAdvance, updatedFormattedAdvance,
            originalSameWidthMinimum, updatedSameWidthMinimum, nativeSameSizeWidth,
            originalSameWidthMenuSize.width(), updatedSameWidthMenuSize.width(),
            updatedSameWidthActionRect.x(), updatedSameWidthActionRect.y(),
            updatedSameWidthActionRect.width(), updatedSameWidthActionRect.height(),
            sameSizeReferenceActionRect.x(), sameSizeReferenceActionRect.y(),
            sameSizeReferenceActionRect.width(), sameSizeReferenceActionRect.height(),
            originalSameWidthGlyphs.isNull(), updatedSameWidthGlyphs.isNull(),
            expectedSameWidthGlyphs.isNull(),
            originalSameWidthGlyphs != updatedSameWidthGlyphs,
            updatedSameWidthGlyphs == expectedSameWidthGlyphs);
    }

    ui::shortcuts::KeySequenceDisplayMenu dynamicWidgetMenu{};
    auto* const dynamicWidgetAction = new QWidgetAction(&dynamicWidgetMenu);
    auto* const dynamicPanel = new DynamicMenuPanelWidget(QSize(150, 24));
    dynamicWidgetAction->setDefaultWidget(dynamicPanel);
    dynamicWidgetMenu.addAction(dynamicWidgetAction);
    const QSize compactWidgetHint = dynamicWidgetMenu.sizeHint();
    const int dynamicWidgetMinimumWidth = dynamicWidgetMenu.minimumWidth();
    dynamicPanel->setReportedSizeHint(QSize(150, 72));
    // QWidgetAction panels in the app invalidate QMenu's cached row size by
    // removing and reinserting the same action after their content changes.
    dynamicWidgetMenu.removeAction(dynamicWidgetAction);
    dynamicWidgetMenu.addAction(dynamicWidgetAction);
    const QSize expandedWidgetHint = dynamicWidgetMenu.sizeHint();

    check(firstPopupSize == repeatedPopupSize
            && firstMinimumWidth == repeatedMinimumWidth
            && longHint == repeatedLongHint
            && longMinimumWidth == repeatedLongMinimumWidth,
        "repeated menu sizing and popup cycles keep the formatted minimum width stable");
    check(shortHint.width() < longHint.width()
            && shortMinimumWidth < longMinimumWidth,
        "shorter shortcut bindings release the menu's previously applied width contribution");
    check(longSequenceHasPositiveWidthDelta,
        "menu-width boundary fixture has a positive formatted shortcut-width delta");
    check(visibleOnlyWidth == withInvisibleWidth,
        "hidden menu actions do not enlarge the formatted shortcut column");
    check(fixedWidth.minimumWidth() == 321 && fixedWidth.maximumWidth() == 321,
        "formatted shortcut sizing preserves a caller's fixed menu width");
    check(initialCallerMinimum.minimumWidth() == 350
            && initialCallerMinimumHint.width() >= 350,
        "formatted shortcut sizing preserves a caller minimum set before the first action");
    check(callerMinimumAfterLong >= 350 && callerMinimum.minimumWidth() == 350,
        "formatted shortcut sizing preserves a caller's minimum width when bindings shrink");
    check(loweredCallerFloorHint.width() < originalCallerFloor
            && loweredCallerFloorValue < originalCallerFloor,
        "a caller can lower a prior menu minimum while the popup is open");
    check(maximumBound.maximumWidth() == 150
            && maximumBound.minimumWidth() <= 150
            && maximumBoundHint.width() <= 150,
        "formatted shortcut sizing preserves a caller's maximum width");
    check(sameWidthRebindLayoutValid,
        "same-width Numpad rebind keeps the formatted column and rebuilds menu action geometry");
    check(dynamicWidgetMinimumWidth == 0
            && expandedWidgetHint.height() > compactWidgetHint.height(),
        "menus without formatted shortcuts bypass width management and re-read dynamic widget-action height");
}

void testKeymapConflictsRespectActiveContexts()
{
    NavigationSettingsFixture settingsIsolation{};
    MainWindow window(nullptr, true);
    QWidget uvCanvas(&window);
    uvCanvas.setObjectName("uvEditorCanvas");
    QAction uvShortcut("Local UV command", &uvCanvas);
    uvShortcut.setObjectName("surface.uv.test_local");
    uvShortcut.setShortcut(QKeySequence(Qt::Key_F14));
    uvShortcut.setShortcutContext(Qt::WidgetWithChildrenShortcut);
    uvCanvas.addAction(&uvShortcut);
    drivePreferences(window, [&](QDialog& dialog)
    {
        auto* const tree = dialog.findChild<QTreeWidget*>("pref_keymap_tree");
        auto* const error = dialog.findChild<QLabel*>("pref_keymap_binding_error");
        auto* const box = dialog.findChild<QDialogButtonBox*>();
        auto* const ok = box ? box->button(QDialogButtonBox::Ok) : nullptr;
        check(tree && ok && ok->isEnabled(),
            "intentional duplicate default shortcuts do not invalidate an unchanged Preferences dialog");
        if ( !tree || !ok )
        {
            dialog.reject();
            return;
        }
        check(setBindingAlternative(*tree, "viewport.show_grid", 0, QKeySequence(Qt::Key_F3)),
            "conflict regression edits the real Toggle Grid binding");
        check(error && error->text().contains(QStringLiteral("Toggle Grid"))
                && error->text().contains(QStringLiteral("Align Grid")) && !ok->isEnabled(),
            "Toggle Grid and Align Grid reject a shared shortcut in their overlapping viewport scopes");
        setBindingAlternative(*tree, "viewport.show_grid", 0, QKeySequence(Qt::CTRL | Qt::Key_F));
        check(error && !error->text().isEmpty() && !ok->isEnabled(),
            "application Find and viewport Toggle Grid reject a simultaneously active shortcut");
        setBindingAlternative(*tree, "viewport.show_grid", 0, QKeySequence(Qt::Key_G));
        setBindingAlternative(*tree, "cog_editor.format", 0, QKeySequence(Qt::Key_G));
        check(error && error->text().isEmpty() && ok->isEnabled(),
            "disjoint focused COG and viewport actions can reuse a shortcut");
        check(setBindingAlternative(*tree, "surface.uv.test_local", 0, QKeySequence(Qt::Key_G))
                && error && error->text().isEmpty() && ok->isEnabled(),
            "a Qt-local UV canvas command can reuse disjoint COG and viewport shortcuts");
        setBindingAlternative(*tree, "cog_editor.goto_definition", 0, QKeySequence(Qt::Key_G));
        check(error && !error->text().isEmpty() && !ok->isEnabled(),
            "two COG actions reject the same shortcut within their shared script focus context");
        setBindingAlternative(*tree, "cog_editor.goto_definition", 0, QKeySequence(Qt::Key_F12));
        setBindingAlternative(*tree, "viewport.fly.forward", 0, QKeySequence(Qt::Key_G));
        check(error && error->text().isEmpty() && ok->isEnabled(),
            "exclusive Fly key ownership permits reuse of an ordinary viewport shortcut");
        setBindingAlternative(*tree, "viewport.fly.fast", 0, QKeySequence(Qt::Key_Control));
        check(error && !error->text().isEmpty() && !ok->isEnabled(),
            "Fly speed hold alternatives still reject a collision within the active Fly context");
        setBindingAlternative(*tree, "viewport.fly.fast", 0, QKeySequence(Qt::Key_Shift));
        auto* const pick = dialog.findChild<QComboBox*>("pref_mouse_pick_button");
        if ( pick )
        {
            pick->setCurrentIndex(pick->findData(int(Qt::RightButton)));
        }
        setBindingAlternative(*tree, "cog_editor.format", 0, QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_F));
        check(pick && !ok->isEnabled(),
            "repairing a keymap conflict cannot enable OK while a mouse-binding conflict remains");
        dialog.reject();
    });
    QAction* const grid = window.findChild<QAction*>("viewport.show_grid");
    check(grid && grid->shortcuts() == QList<QKeySequence>{QKeySequence(Qt::Key_G)},
        "canceled conflict editing preserves the live shortcut mapping");
}

void testUnboundViewportActionsReuseAuthoritativeToggles()
{
    NavigationSettingsFixture settingsIsolation{};
    MainWindow window(nullptr, true);
    GlView* const view = window.glViewForTest();
    const std::array<const char*, 16> ids{
        "viewport.render.game_preview", "viewport.audio.mute_sector_sounds",
        "viewport.render_fog", "viewport.xray_selection_cycling",
        "viewport.orbit_around_selection", "viewport.map.grid_follow_camera",
        "viewport.map.grid_rotate_to_view", "viewport.map.grid_keep_view_aligned_visible",
        "viewport.map.grid_adaptive_spacing", "viewport.map.grid_reset_on_rotate",
        "viewport.grid_visible_through_walls", "viewport.gizmos.arch_thing",
        "viewport.transform.grid_snap", "viewport.transform.grid_stick_through",
        "viewport.surface.strict_scope", "viewport.surface.show_normals",
    };
    drivePreferences(window, [&](QDialog& dialog)
    {
        auto* const tree = dialog.findChild<QTreeWidget*>("pref_keymap_tree");
        for ( const char* const id : ids )
        {
            const auto items = tree ? bindingItems(*tree, id) : std::vector<QTreeWidgetItem*>{};
            QAction* const action = window.findChild<QAction*>(QString::fromLatin1(id));
            check(items.size() == 1 && action && action->shortcuts().isEmpty()
                    && shortcutEditor(tree->itemWidget(items.front(), 1)),
                "existing viewport and Map Settings toggles are exposed once with no default binding");
        }
        check(tree && bindingItems(*tree, "viewport.map.grid_always_behind").empty()
                && bindingItems(*tree, "viewport.map.grid_visible_through_walls").empty(),
            "Map Settings copies of existing View-menu actions do not create duplicate keymap commands");
        if ( tree && setBindingAlternative(*tree, "viewport.map.grid_rotate_to_view", 0, QKeySequence(Qt::Key_F18)) )
        {
            auto* const box = dialog.findChild<QDialogButtonBox*>();
            if ( auto* const ok = box ? box->button(QDialogButtonBox::Ok) : nullptr )
            {
                ok->click();
            }
            else
            {
                dialog.reject();
            }
        }
        else
        {
            dialog.reject();
        }
    });
    QAction* const rotate = window.findChild<QAction*>("viewport.map.grid_rotate_to_view");
    const auto rebuilds = view ? view->meshRebuildCountForTest() : 0u;
    if ( rotate && view )
    {
        const bool before = view->gridRotateWithView();
        rotate->trigger();
        check(rotate->shortcuts() == QList<QKeySequence>{QKeySequence(Qt::Key_F18)}
                && view->gridRotateWithView() != before
                && settingsStore().value("viewport/grid_rotate_with_view").toBool() == view->gridRotateWithView()
                && view->meshRebuildCountForTest() == rebuilds,
            "always align grid to view uses the existing toggle and persistence without rebuilding geometry");
    }
}

void testToolbarToggleBindingsReuseStateAndAvailability()
{
    NavigationSettingsFixture settingsIsolation{};
    const std::array<const char*, 4> ids{
        "viewport.transform.grid_snap", "viewport.transform.grid_stick_through",
        "viewport.surface.strict_scope", "viewport.surface.show_normals",
    };
    const std::array<const char*, 4> settingKeys{
        "viewport/transform_grid_snap", "viewport/transform_grid_stick_through",
        "viewport/surface_strict_selection", "viewport/show_selected_surface_normals",
    };
    {
        QSettings settings = settingsStore();
        for ( std::size_t index = 0; index < ids.size(); ++index )
        {
            QStringList alternatives{
                QKeySequence(Qt::Key_F14 + static_cast<int>(index)).toString(QKeySequence::PortableText),
            };
            if ( index == 0u )
            {
                alternatives.push_back(QKeySequence(Qt::Key_F18).toString(QKeySequence::PortableText));
            }
            settings.setValue(QStringLiteral("keymap/shortcuts/") + QString::fromLatin1(ids[index]),
                alternatives);
        }
    }
    MainWindow window(nullptr, true);
    window.setAttribute(Qt::WA_DontShowOnScreen);
    window.show();
    GlView* const view = window.glViewForTest();
    QAction* const preview = window.findChild<QAction*>("viewport.render.game_preview");
    std::array<QAction*, 4> actions{};
    for ( std::size_t index = 0; index < ids.size(); ++index )
    {
        actions[index] = window.findChild<QAction*>(QString::fromLatin1(ids[index]));
        QList<QKeySequence> expected{QKeySequence(Qt::Key_F14 + static_cast<int>(index))};
        if ( index == 0u )
        {
            expected.push_back(QKeySequence(Qt::Key_F18));
        }
        check(actions[index] && actions[index]->shortcuts() == expected,
            "existing toolbar toggles restore all persisted alternatives before Preferences is opened");
        check(actions[index] && actions[index]->shortcutContext() == Qt::WidgetShortcut,
            "toolbar toggle shortcuts are attached to direct viewport focus");
    }
    if ( !view || !preview
        || std::ranges::any_of(actions, [](const QAction* action) { return !action; }) )
    {
        check(false, "toolbar shortcut regression uses the real viewport and four authoritative toggle actions");
        return;
    }
    const auto capture = [&]()
    {
        return std::array<bool, 4>{
            view->transformGridSnapEnabled(), view->transformGridPlaneStickEnabled(),
            view->strictSurfaceSelectionEnabled(), view->showSelectedSurfaceNormals(),
        };
    };
    const auto sendToggle = [&](const std::size_t index)
    {
        QWidget* const focused = QApplication::focusWidget();
        if ( !focused )
        {
            check(false, "toolbar shortcut dispatch requires an actual focused widget");
            return;
        }
        QKeyEvent press(QEvent::KeyPress, Qt::Key_F14 + static_cast<int>(index), Qt::NoModifier);
        QKeyEvent release(QEvent::KeyRelease, Qt::Key_F14 + static_cast<int>(index), Qt::NoModifier);
        QApplication::sendEvent(focused, &press);
        QApplication::sendEvent(focused, &release);
    };
    window.activateSurfaceEditModeForTest();
    window.activateSurfaceEdgeSubModeForTest();
    QApplication::setActiveWindow(&window);
    view->setFocus(Qt::OtherFocusReason);
    QApplication::processEvents();
    check(QApplication::focusWidget() == view,
        "toolbar toggle keyboard regression gives the actual viewport focus");
    for ( std::size_t index = 0; index < ids.size(); ++index )
    {
        check(actions[index]->isEnabled(),
            "all four existing toolbar options are available in Surface Edge editing");
        auto expected = capture();
        expected[index] = !expected[index];
        sendToggle(index);
        check(capture() == expected
                && settingsStore().value(QString::fromLatin1(settingKeys[index])).toBool() == expected[index],
            "a toolbar keymap toggle changes its existing viewport state and authoritative persisted preference");
    }
    QLineEdit outsideEditor(&window);
    outsideEditor.setGeometry(30, 30, 180, 28);
    outsideEditor.show();
    outsideEditor.setFocus(Qt::OtherFocusReason);
    QApplication::processEvents();
    check(QApplication::focusWidget() == &outsideEditor,
        "toolbar shortcut regression gives a sibling text editor actual focus");
    const auto outsideBefore = capture();
    for ( std::size_t index = 0; index < ids.size(); ++index )
    {
        sendToggle(index);
    }
    check(capture() == outsideBefore,
        "toolbar toggle shortcuts remain inactive while another editor owns focus");
    view->setFocus(Qt::OtherFocusReason);
    window.activateSurfaceFaceSubModeForTest();
    check(!actions[2]->isEnabled() && actions[3]->isEnabled(),
        "Strict keeps its Edge/Vertex eligibility while Surface normals remain available in Face mode");
    const auto faceBefore = capture();
    sendToggle(2u);
    check(capture() == faceBefore, "a rebound Strict shortcut cannot bypass its existing Face-mode gate");
    window.activateThingEditModeForTest();
    check(!actions[2]->isEnabled() && !actions[3]->isEnabled(),
        "Surface toolbar options keep their existing Surface-only availability");
    preview->setChecked(true);
    QApplication::processEvents();
    view->setFocus(Qt::OtherFocusReason);
    check(QApplication::focusWidget() == view,
        "Preview toolbar shortcut regression keeps direct viewport focus");
    const auto previewBefore = capture();
    for ( std::size_t index = 0; index < ids.size(); ++index )
    {
        check(!actions[index]->isEnabled(),
            "actual toolbar toggle actions obey the existing disabled editing gate in Game Preview");
        sendToggle(index);
    }
    check(capture() == previewBefore,
        "rebound toolbar shortcuts cannot alter hidden editing options during Game Preview");
    preview->setChecked(false);
    window.activateSurfaceEditModeForTest();
    window.activateSurfaceEdgeSubModeForTest();
    for ( QAction* const action : actions )
    {
        check(action->isEnabled(),
            "returning to eligible Editor mode restores each authoritative toolbar action");
    }
    {
        QSettings settings = settingsStore();
        settings.setValue("keymap/shortcuts/viewport.transform.grid_snap", QStringList{});
    }
    MainWindow restored(nullptr, true);
    QAction* const cleared = restored.findChild<QAction*>("viewport.transform.grid_snap");
    QAction* const retained = restored.findChild<QAction*>("viewport.transform.grid_stick_through");
    check(cleared && cleared->shortcuts().isEmpty()
            && retained && retained->shortcuts() == QList<QKeySequence>{QKeySequence(Qt::Key_F15)},
        "late toolbar shortcut startup distinguishes an explicit empty binding from an unrelated custom binding");
}

void testRegisteredGlobalActionsOverlapFocusedEditors()
{
    NavigationSettingsFixture settingsIsolation{};
    MainWindow window(nullptr, true);
    drivePreferences(window, [&](QDialog& dialog)
    {
        auto* const tree = dialog.findChild<QTreeWidget*>("pref_keymap_tree");
        auto* const error = dialog.findChild<QLabel*>("pref_keymap_binding_error");
        auto* const box = dialog.findChild<QDialogButtonBox*>();
        auto* const ok = box ? box->button(QDialogButtonBox::Ok) : nullptr;
        if ( !tree || !ok )
        {
            dialog.reject();
            return;
        }
        const QKeySequence shared(Qt::Key_F14);
        check(setBindingAlternative(*tree, "surface.unadjoin", 0, shared)
                && setBindingAlternative(*tree, "cog_editor.format", 0, shared)
                && error && !error->text().isEmpty() && !ok->isEnabled(),
            "registered WindowShortcut world commands overlap focused COG actions");
        setBindingAlternative(*tree, "surface.unadjoin", 0, QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_J));
        check(setBindingAlternative(*tree, "viewport.cancel", 0, shared)
                && error && !error->text().isEmpty() && !ok->isEnabled(),
            "window-wide viewport Cancel retains its actual global conflict scope");
        setBindingAlternative(*tree, "viewport.cancel", 0, QKeySequence(Qt::Key_Escape));
        setBindingAlternative(*tree, "cog_editor.format", 0, QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_F));
        setBindingAlternative(*tree, "viewport.show_grid", 0, QKeySequence(Qt::Key_F19));
        check(setBindingAlternative(*tree, "cleave.transform.move", 0, QKeySequence(Qt::Key_F19))
                && error && !error->text().isEmpty() && !ok->isEnabled(),
            "an intentional shared default does not exempt newly colliding alternatives for that action pair");
        setBindingAlternative(*tree, "viewport.show_grid", 0, QKeySequence(Qt::Key_G));
        setBindingAlternative(*tree, "cleave.transform.move", 0, QKeySequence(Qt::Key_G));
        check(setBindingAlternative(*tree, "viewport.light_gizmos.always_baked", 0, QKeySequence(Qt::Key_F21))
                && setBindingAlternative(*tree, "edit.add_cube", 0, QKeySequence(Qt::Key_F21))
                && error && !error->text().isEmpty() && !ok->isEnabled(),
            "light-marker visibility shortcuts overlap Sector editing because the markers are usable in every mode");
        setBindingAlternative(*tree, "viewport.light_gizmos.always_baked", 0, QKeySequence{});
        setBindingAlternative(*tree, "edit.add_cube", 0, QKeySequence(Qt::Key_A));
        const QKeySequence hold(QStringLiteral("Alt+Meta"), QKeySequence::PortableText);
        check(setBindingAlternative(*tree, "surface_move.modifier.invert_nearby_snap", 0, hold)
                && setBindingAlternative(*tree, "thing_move.modifier.invert_surface_stick", 0, hold)
                && error && error->text().isEmpty() && ok->isEnabled(),
            "Surface-only and Thing-only mouse modifiers can reuse a chord in disjoint edit families");
        dialog.reject();
    });
}

void testGridBehaviorShortcutsRespectPreviewAvailability()
{
    NavigationSettingsFixture settingsIsolation{};
    const std::array<const char*, 4> ids{
        "viewport.map.grid_follow_camera", "viewport.map.grid_rotate_to_view",
        "viewport.map.grid_adaptive_spacing", "viewport.map.grid_reset_on_rotate",
    };
    {
        QSettings settings = settingsStore();
        for ( std::size_t index = 0; index < ids.size(); ++index )
        {
            settings.setValue(QStringLiteral("keymap/shortcuts/") + QString::fromLatin1(ids[index]),
                QStringList{QKeySequence(Qt::Key_F14 + static_cast<int>(index)).toString(QKeySequence::PortableText)});
        }
    }
    MainWindow window(nullptr, true);
    window.setAttribute(Qt::WA_DontShowOnScreen);
    window.show();
    GlView* const view = window.glViewForTest();
    QAction* const preview = window.findChild<QAction*>("viewport.render.game_preview");
    if ( !view || !preview )
    {
        check(false, "Preview grid shortcut regression has the real viewport and Preview action");
        return;
    }
    QApplication::setActiveWindow(&window);
    view->setFocus(Qt::OtherFocusReason);
    const auto capture = [&]()
    {
        return std::array<bool, 4>{view->gridFollowCamera(), view->gridRotateWithView(),
            view->gridAdaptiveSpacing(), view->gridResetOnCameraRotate()};
    };
    const auto before = capture();
    preview->setChecked(true);
    QApplication::processEvents();
    check(QApplication::focusWidget() == view,
        "Preview grid shortcut regression sends keys while the actual viewport owns focus");
    for ( std::size_t index = 0; index < ids.size(); ++index )
    {
        QAction* const action = window.findChild<QAction*>(QString::fromLatin1(ids[index]));
        check(action && action->shortcuts() == QList<QKeySequence>{QKeySequence(Qt::Key_F14 + static_cast<int>(index))},
            "late Grid Behavior actions restore persisted keyboard bindings before Preferences is opened");
        check(action && !action->isEnabled(),
            "each rebound Grid Behavior action is disabled independently in Game Preview");
        QKeyEvent press(QEvent::KeyPress, Qt::Key_F14 + static_cast<int>(index), Qt::NoModifier);
        QKeyEvent release(QEvent::KeyRelease, Qt::Key_F14 + static_cast<int>(index), Qt::NoModifier);
        QApplication::sendEvent(view, &press);
        QApplication::sendEvent(view, &release);
    }
    check(capture() == before,
        "GlView-attached Grid Behavior shortcuts cannot change hidden Editor grid options in Preview");
    preview->setChecked(false);
    for ( const char* const id : ids )
    {
        QAction* const action = window.findChild<QAction*>(QString::fromLatin1(id));
        check(action && action->isEnabled(),
            "returning to Editor restores each Grid Behavior keyboard action");
    }
}

void testViewportShortcutsRespectMapNumericFocus()
{
    NavigationSettingsFixture settingsIsolation{};
    MainWindow window(nullptr, true);
    window.setAttribute(Qt::WA_DontShowOnScreen);
    window.show();
    GlView* const view = window.glViewForTest();
    auto* const spin = window.findChild<QDoubleSpinBox*>("viewport.map.grid_origin_x");
    QAction* const align = window.findChild<QAction*>("viewport.grid_align_to_view");
    check(view && spin && align, "numeric focus regression uses the actual Map Settings grid-origin editor");
    if ( !view || !spin || !align )
    {
        return;
    }
    for ( QWidget* ancestor = spin; ancestor && ancestor != view; ancestor = ancestor->parentWidget() )
    {
        ancestor->show();
    }
    align->setShortcuts({QKeySequence(Qt::Key_F18)});
    int triggered = 0;
    QObject::connect(align, &QAction::triggered, &window, [&]() { ++triggered; });
    QApplication::setActiveWindow(&window);
    spin->setFocus(Qt::OtherFocusReason);
    QApplication::processEvents();
    QWidget* const focused = QApplication::focusWidget();
    check(focused && (focused == spin || spin->isAncestorOf(focused)),
        "Map Settings numeric editor or its line edit owns focus before viewport shortcut dispatch");
    QKeyEvent press(QEvent::KeyPress, Qt::Key_F18, Qt::NoModifier);
    QKeyEvent release(QEvent::KeyRelease, Qt::Key_F18, Qt::NoModifier);
    QApplication::sendEvent(spin, &press);
    QApplication::sendEvent(spin, &release);
    check(triggered == 0,
        "viewport camera/grid shortcut bindings remain inactive while a Map Settings numeric editor has focus");
}

void testCameraInversionSettingsAndPreferencePublication()
{
    NavigationSettingsFixture settingsIsolation{};
    QTemporaryDir scratch{};
    check(scratch.isValid(), "camera inversion codec checks use a private settings directory");
    if ( !scratch.isValid() )
    {
        return;
    }
    QSettings source(scratch.path() + QStringLiteral("/source.ini"), QSettings::IniFormat);
    QSettings result(scratch.path() + QStringLiteral("/result.ini"), QSettings::IniFormat);
    source.setValue("viewport/mouse/invert_horizontal", true);
    source.setValue("viewport/mouse/invert_vertical", true);
    check(saveViewportNavigationSettings(result, loadViewportNavigationSettings(source))
            && result.value("viewport/mouse/invert_horizontal", false).toBool()
            && result.value("viewport/mouse/invert_vertical", false).toBool(),
        "camera inversion preferences survive validated load/save independently of wheel inversion");
    source.setValue("viewport/mouse/invert_horizontal", QStringLiteral("sometimes"));
    source.setValue("viewport/mouse/invert_vertical", 2);
    result.clear();
    check(saveViewportNavigationSettings(result, loadViewportNavigationSettings(source))
            && result.contains("viewport/mouse/invert_horizontal")
            && !result.value("viewport/mouse/invert_horizontal").toBool()
            && result.contains("viewport/mouse/invert_vertical")
            && !result.value("viewport/mouse/invert_vertical").toBool(),
        "malformed camera inversion settings use explicit default-off values");
    MainWindow window(nullptr, true);
    GlView* const view = window.glViewForTest();
    const auto before = view ? view->viewportNavigationSettings() : ViewportNavigationSettings{};
    drivePreferences(window, [&](QDialog& dialog)
    {
        auto* const horizontal = dialog.findChild<QCheckBox*>("pref_mouse_invert_horizontal");
        auto* const vertical = dialog.findChild<QCheckBox*>("pref_mouse_invert_vertical");
        check(horizontal && vertical, "Preferences exposes independent horizontal and vertical camera inversion");
        if ( horizontal )
        {
            horizontal->setChecked(true);
        }
        if ( vertical )
        {
            vertical->setChecked(true);
        }
        dialog.reject();
    });
    check(view && view->viewportNavigationSettings() == before
            && !settingsStore().contains("viewport/mouse/invert_horizontal")
            && !settingsStore().contains("viewport/mouse/invert_vertical"),
        "Cancel preserves both cached navigation and persisted camera inversion preferences");
    const auto rebuilds = view ? view->meshRebuildCountForTest() : 0u;
    drivePreferences(window, [&](QDialog& dialog)
    {
        auto* const horizontal = dialog.findChild<QCheckBox*>("pref_mouse_invert_horizontal");
        auto* const vertical = dialog.findChild<QCheckBox*>("pref_mouse_invert_vertical");
        if ( !horizontal || !vertical )
        {
            dialog.reject();
            return;
        }
        horizontal->setChecked(true);
        vertical->setChecked(true);
        auto* const box = dialog.findChild<QDialogButtonBox*>();
        if ( auto* const ok = box ? box->button(QDialogButtonBox::Ok) : nullptr )
        {
            ok->click();
        }
        else
        {
            dialog.reject();
        }
    });
    result.clear();
    if ( view )
    {
        saveViewportNavigationSettings(result, view->viewportNavigationSettings());
    }
    check(view && result.value("viewport/mouse/invert_horizontal", false).toBool()
            && result.value("viewport/mouse/invert_vertical", false).toBool()
            && settingsStore().value("viewport/mouse/invert_horizontal", false).toBool()
            && settingsStore().value("viewport/mouse/invert_vertical", false).toBool()
            && view->meshRebuildCountForTest() == rebuilds,
        "OK publishes cached camera inversion immediately without a geometry rebuild");
}

void testKeymapAlphabeticalOrderKeepsBindingControls()
{
    NavigationSettingsFixture settingsIsolation{};
    MainWindow window(nullptr, true);
    const std::array<const char*, 3> ids{
        "viewport.map.ui_sort_zulu", "viewport.map.ui_sort_alpha", "viewport.map.ui_sort_mixed",
    };
    const std::array<const char*, 3> labels{
        "alpha ordering regression", "Zulu ordering regression", "bEta ordering regression",
    };
    for ( std::size_t index = 0; index < ids.size(); ++index )
    {
        auto* const action = new QAction(QString::fromLatin1(labels[index]), &window);
        action->setObjectName(QString::fromLatin1(ids[index]));
        action->setProperty("sed_keymap_expose", true);
        action->setShortcut(QKeySequence(Qt::Key_F20 + static_cast<int>(index)));
    }
    drivePreferences(window, [&](QDialog& dialog)
    {
        auto* const tree = dialog.findChild<QTreeWidget*>("pref_keymap_tree");
        auto* const search = dialog.findChild<QLineEdit*>("pref_keymap_search");
        check(tree && search, "alphabetical regression uses the real hierarchical Keymap page");
        if ( !tree || !search )
        {
            dialog.reject();
            return;
        }
        std::function<bool(const QTreeWidgetItem*)> ordered = [&](const QTreeWidgetItem* parent)
        {
            bool result = true;
            for ( int index = 0; index < parent->childCount(); ++index )
            {
                const QTreeWidgetItem* const item = parent->child(index);
                if ( index > 0 )
                {
                    result = QString::compare(parent->child(index - 1)->text(0), item->text(0),
                        Qt::CaseInsensitive) <= 0 && result;
                }
                result = ordered(item) && result;
            }
            return result;
        };
        check(tree->topLevelItemCount() > 2 && ordered(tree->invisibleRootItem()),
            "every Keymap category and action sibling level is alphabetical by displayed friendly label");
        struct RowSnapshot
        {
            QTreeWidgetItem* item{};
            QTreeWidgetItem* parent{};
            QWidget* controls{};
            QString help{};
            QKeySequence firstShortcut{};
        };
        std::vector<RowSnapshot> rows{};
        for ( QTreeWidgetItemIterator item(tree); *item; ++item )
        {
            if ( !(*item)->toolTip(0).contains(QStringLiteral("Binding ID: ")) )
            {
                continue;
            }
            QWidget* const controls = tree->itemWidget(*item, 1);
            QKeySequenceEdit* const editor = shortcutEditor(controls);
            rows.push_back({*item, (*item)->parent(), controls, (*item)->toolTip(0),
                editor ? editor->keySequence() : QKeySequence{}});
        }
        const auto retained = [&](const bool compareShortcuts)
        {
            return std::ranges::all_of(rows, [&](const RowSnapshot& row)
            {
                const QKeySequenceEdit* const editor = shortcutEditor(row.controls);
                return row.item->parent() == row.parent && row.item->toolTip(0) == row.help
                    && tree->itemWidget(row.item, 1) == row.controls
                    && (!compareShortcuts || !editor || editor->keySequence() == row.firstShortcut);
            });
        };
        check(rows.size() > 30, "ordering regression covers real actions and fixed Cleave mouse rows");
        tree->sortItems(0, Qt::AscendingOrder);
        check(ordered(tree->invisibleRootItem()) && retained(true),
            "case-insensitive resort keeps each Binding ID, category parent, and row-owned shortcut widget");
        const auto edited = bindingItems(*tree, ids[0]);
        check(edited.size() == 1 && setBindingAlternative(*tree, ids[0], 0, QKeySequence(Qt::Key_F23)),
            "the alpha-labelled action retains its own editable shortcut after sorting");
        search->setText(QString::fromLatin1(ids[0]));
        check(edited.size() == 1 && !edited.front()->isHidden() && retained(false)
                && shortcutEditor(tree->itemWidget(edited.front(), 1))->keySequence() == QKeySequence(Qt::Key_F23),
            "filtering finds the correct sorted action without moving its shortcut to another Binding ID");
        search->clear();
        QPushButton* reset = nullptr;
        for ( QPushButton* const button : dialog.findChildren<QPushButton*>() )
        {
            if ( button->text() == QStringLiteral("Reset to Defaults") )
            {
                reset = button;
            }
        }
        check(reset != nullptr, "ordering regression reaches the actual keymap Reset to Defaults control");
        if ( reset )
        {
            reset->click();
        }
        tree->sortItems(0, Qt::AscendingOrder);
        check(ordered(tree->invisibleRootItem()) && retained(true),
            "Restore and filtering preserve alphabetical levels and all original row-owned binding controls");
        dialog.reject();
    });
}

class NativeKeymapHoverObserver final : public QObject
{
public:
    int spontaneousMoves{};

protected:
    bool eventFilter(QObject*, QEvent* const event) override
    {
        if ( event->type() == QEvent::MouseMove && event->spontaneous() )
        {
            ++spontaneousMoves;
        }
        return false;
    }
};

void testKeymapNativeAndStylesheetRowHover()
{
    NavigationSettingsFixture settingsIsolation{};
    MainWindow window(nullptr, true);
    window.resize(1100, 760);
    window.show();
    const QPoint previousCursor = QCursor::pos();
    drivePreferences(window, [&](QDialog& dialog)
    {
        auto* const categories = dialog.findChild<QListWidget*>("pref_categories");
        auto* const tree = dialog.findChild<QTreeWidget*>("pref_keymap_tree");
        auto* const search = dialog.findChild<QLineEdit*>("pref_keymap_search");
        if ( !categories || !tree || !search )
        {
            check(false, "native row-hover regression has the real Keymap page");
            dialog.reject();
            return;
        }
        for ( int index = 0; index < categories->count(); ++index )
        {
            if ( categories->item(index)->text() == QStringLiteral("Keymap") )
            {
                categories->setCurrentRow(index);
            }
        }
        dialog.resize(1000, 720);
        dialog.raise();
        dialog.activateWindow();
        QApplication::setActiveWindow(&dialog);
        tree->setSelectionMode(QAbstractItemView::SingleSelection);
        tree->setMouseTracking(true);
        tree->viewport()->setMouseTracking(true);
        NativeKeymapHoverObserver observer{};
        tree->viewport()->installEventFilter(&observer);
        const auto waitFor = waitForNavigationUi;
        QApplication::processEvents();
        check(dialog.isVisible() && tree->viewport()->isVisible()
                && qApp->property("sed.globalComponentStylesApplied").toBool(),
            "native hover runs on the visible Preferences tree with the actual application-wide stylesheet");
        const auto gridRows = bindingItems(*tree, "viewport.show_grid");
        if ( gridRows.size() != 1 || !gridRows.front()->parent() )
        {
            check(false, "native hover regression finds the real Grid Display category");
            dialog.reject();
            return;
        }
        QTreeWidgetItem* const category = gridRows.front()->parent();
        std::vector<QTreeWidgetItem*> leaves{};
        for ( int index = 0; index < category->childCount() && leaves.size() < 2u; ++index )
        {
            QTreeWidgetItem* const item = category->child(index);
            if ( item->childCount() == 0 && tree->itemWidget(item, 1) )
            {
                leaves.push_back(item);
            }
        }
        check(leaves.size() == 2u, "native hover covers adjacent normal and alternate action rows");
        if ( leaves.size() != 2u )
        {
            dialog.reject();
            return;
        }
        const auto bluePixels = [](const QImage& image)
        {
            int count = 0;
            for ( int y = 0; y < image.height(); ++y )
            {
                for ( int x = 0; x < image.width(); ++x )
                {
                    const QColor color = image.pixelColor(x, y);
                    count += color.blue() > color.red() + 30 && color.blue() > color.green() + 12;
                }
            }
            return count;
        };
        for ( const bool inheritedBlueStyle : { false, true } )
        {
            if ( inheritedBlueStyle )
            {
                dialog.setStyleSheet(dialog.styleSheet() + QStringLiteral(
                    "\nQTreeWidget#pref_keymap_tree::item:hover { background-color: #2b78da; }\n"));
            }
            for ( int caseIndex = 0; caseIndex < 5; ++caseIndex )
            {
                QTreeWidgetItem* const item = caseIndex == 0 ? category
                    : leaves[static_cast<std::size_t>((caseIndex - 1) % 2)];
                const bool selected = caseIndex >= 3;
                tree->clearSelection();
                if ( selected )
                {
                    tree->setCurrentItem(item);
                    item->setSelected(true);
                }
                tree->setFocus(Qt::OtherFocusReason);
                tree->scrollToItem(item);
                QApplication::processEvents();
                QCursor::setPos(search->mapToGlobal(search->rect().center()));
                check(waitFor([&]() { return !tree->viewport()->underMouse(); }),
                    "native pointer leaves the viewport before the unhovered row capture");
                const QRect row = tree->visualItemRect(item);
                const int left = std::max(0, row.left()) + 4;
                const QRect sample = QRect(left, row.top() + 2,
                    tree->columnWidth(0) - left - 4, row.height() - 4).intersected(tree->viewport()->rect());
                check(sample.width() > 100 && sample.height() > 12 && item->isSelected() == selected,
                    "native hover samples the full first-column row in its intended selection state");
                const QImage before = tree->viewport()->grab(sample).toImage();
                const QPoint point(tree->columnWidth(0) - 18, row.center().y());
                const int beforeMoves = observer.spontaneousMoves;
                QCursor::setPos(tree->viewport()->mapToGlobal(point));
                const bool nativeHovered = waitFor([&]()
                {
                    return observer.spontaneousMoves > beforeMoves && tree->viewport()->underMouse()
                        && tree->itemAt(tree->viewport()->mapFromGlobal(QCursor::pos())) == item;
                });
                check(nativeHovered, "real native cursor movement reaches the intended Keymap row before capture");
                const QImage hovered = tree->viewport()->grab(sample).toImage();
                const bool unchanged = before == hovered && bluePixels(hovered) <= bluePixels(before);
                if ( !unchanged )
                {
                    std::cerr << "Keymap hover: inherited=" << inheritedBlueStyle
                              << " selected=" << selected << " row=" << item->text(0).toStdString()
                              << " blue-before=" << bluePixels(before) << " blue-after=" << bluePixels(hovered) << '\n';
                }
                check(unchanged, inheritedBlueStyle
                    ? "inherited blue hover stylesheet cannot paint category, alternate, or selected Keymap row backgrounds"
                    : "actual native application style cannot add a blue first-column Keymap hover highlight");
            }
        }
        tree->scrollToItem(gridRows.front());
        QApplication::processEvents();
        const QPoint helpPoint(tree->columnWidth(0) - 18, tree->visualItemRect(gridRows.front()).center().y());
        QHelpEvent help(QEvent::ToolTip, helpPoint, tree->viewport()->mapToGlobal(helpPoint));
        QApplication::sendEvent(tree->viewport(), &help);
        check(QToolTip::text().contains(QStringLiteral("Binding ID: viewport.show_grid")),
            "suppressing row hover preserves native action tooltip help and the Binding ID");
        QToolTip::hideText();
        tree->setCurrentItem(leaves.front());
        QKeyEvent down(QEvent::KeyPress, Qt::Key_Down, Qt::NoModifier);
        QApplication::sendEvent(tree, &down);
        check(tree->currentItem() != leaves.front() && tree->currentItem() && tree->currentItem()->isSelected(),
            "row-hover suppression preserves the view's keyboard selection indication");
        dialog.reject();
    });
    QCursor::setPos(previousCursor);
}

void testKeymapRowsIgnoreMouseHover()
{
    NavigationSettingsFixture settingsIsolation{};
    MainWindow window(nullptr, true);
    drivePreferences(window, [&](QDialog& dialog)
    {
        auto* const categories = dialog.findChild<QListWidget*>("pref_categories");
        auto* const tree = dialog.findChild<QTreeWidget*>("pref_keymap_tree");
        if ( categories )
        {
            for ( int index = 0; index < categories->count(); ++index )
            {
                if ( categories->item(index)->text() == QStringLiteral("Keymap") )
                {
                    categories->setCurrentRow(index);
                }
            }
        }
        const auto items = tree ? bindingItems(*tree, "viewport.show_grid") : std::vector<QTreeWidgetItem*>{};
        check(items.size() == 1, "hover regression finds the Grid row in the actual Keymap page");
        if ( tree && items.size() == 1 )
        {
            tree->scrollToItem(items.front());
            QApplication::processEvents();
            const QRect row = tree->visualItemRect(items.front());
            const QPoint blank(tree->columnWidth(0) - 14, row.center().y());
            QEvent leave(QEvent::Leave);
            QApplication::sendEvent(tree->viewport(), &leave);
            const QRect sample(blank - QPoint(2, 2), QSize(4, 4));
            const QImage before = tree->viewport()->grab(sample).toImage();
            sendMouse(*tree->viewport(), QEvent::MouseMove, blank, Qt::NoButton, Qt::NoButton);
            const QImage hovered = tree->viewport()->grab(sample).toImage();
            check(before == hovered,
                "pointer hover does not add a first-column row highlight to the keymap");
        }
        dialog.reject();
    });
}

void testWheelScrollingUiAndMiddleButtonTransactions()
{
    NavigationSettingsFixture settingsIsolation{};
    MainWindow window(nullptr, true);
    GlView* const view = window.glViewForTest();
    check(view != nullptr, "wheel-click preferences publish to the real viewport cache");
    if ( !view )
    {
        return;
    }
    const auto original = view->viewportNavigationSettings();
    const auto remap = [&](QDialog& dialog)
    {
        auto* const fly = dialog.findChild<QComboBox*>("pref_mouse_fly_look_button");
        auto* const error = dialog.findChild<QLabel*>("pref_mouse_binding_error");
        auto* const box = dialog.findChild<QDialogButtonBox*>();
        auto* const ok = box ? box->button(QDialogButtonBox::Ok) : nullptr;
        if ( !fly || !error || !ok )
        {
            check(false, "wheel-click remap finds actual mouse controls and validation");
            return false;
        }
        fly->setCurrentIndex(fly->findData(int(Qt::MiddleButton)));
        check(!ok->isEnabled() && error->text().contains(QStringLiteral("Fly look"))
                && error->text().contains(QStringLiteral("Orbit")),
            "plain Middle-button Fly explains its duplicate default Orbit chord before publication");
        for ( const char* const id : { "orbit", "pan", "dolly", "fast_dolly" } )
        {
            auto* const button = dialog.findChild<QComboBox*>(
                QStringLiteral("pref_mouse_%1_button").arg(QString::fromLatin1(id)));
            auto* const modifiers = dialog.findChild<QComboBox*>(
                QStringLiteral("pref_mouse_%1_modifiers").arg(QString::fromLatin1(id)));
            check(button && modifiers, "camera-family remap finds every physical button and modifier control");
            if ( !button || !modifiers )
            {
                return false;
            }
            const int chord = modifiers->currentData().toInt();
            button->setCurrentIndex(button->findData(int(Qt::RightButton)));
            check(modifiers->currentData().toInt() == chord,
                "moving the camera family to RMB preserves its Orbit/Pan/Dolly activation modifiers");
        }
        check(ok->isEnabled() && error->text().isEmpty(),
            "Middle-button Fly and the remapped RMB camera family have no shared active chord");
        return ok->isEnabled() && error->text().isEmpty();
    };
    drivePreferences(window, [&](QDialog& dialog)
    {
        auto* const wheel = dialog.findChild<QWidget*>("pref_mouse_wheel_section");
        auto* const heading = dialog.findChild<QLabel*>("pref_mouse_wheel_heading");
        check(wheel && heading && heading->text() == QStringLiteral("Wheel scrolling"),
            "wheel turning has its own Wheel scrolling area separate from physical mouse-button choices");
        bool hasFixedWheelButtonLabel = false;
        for ( QLabel* const label : dialog.findChildren<QLabel*>() )
        {
            hasFixedWheelButtonLabel |= label->text() == QStringLiteral("Mouse wheel");
        }
        check(!hasFixedWheelButtonLabel,
            "wheel Zoom and Fly speed no longer look like locked Mouse button cells");
        for ( const char* const id : { "zoom", "fly_speed" } )
        {
            auto* const modifiers = dialog.findChild<QComboBox*>(
                QStringLiteral("pref_mouse_%1_modifiers").arg(QString::fromLatin1(id)));
            auto* const inverted = dialog.findChild<QCheckBox*>(
                QStringLiteral("pref_mouse_%1_inverted").arg(QString::fromLatin1(id)));
            check(wheel && modifiers && inverted && wheel->isAncestorOf(modifiers)
                    && wheel->isAncestorOf(inverted) && modifiers->isEnabled()
                    && modifiers->findData(-1) >= 0 && modifiers->findData(int(Qt::NoModifier)) >= 0
                    && inverted->text() == QStringLiteral("Invert"),
                "Wheel scrolling retains editable modifier/Disabled choices and its independent Invert control");
            if ( modifiers && inverted )
            {
                modifiers->setCurrentIndex(modifiers->findData(-1));
                check(!inverted->isEnabled(), "disabling wheel scrolling disables only its inversion control");
                modifiers->setCurrentIndex(modifiers->findData(int(Qt::NoModifier)));
                check(inverted->isEnabled(), "restoring wheel scrolling makes its inversion control editable again");
            }
        }
        for ( const char* const id : kViewportMouseActionIds )
        {
            auto* const button = dialog.findChild<QComboBox*>(
                QStringLiteral("pref_mouse_%1_button").arg(QString::fromLatin1(id)));
            const int middle = button ? button->findData(int(Qt::MiddleButton)) : -1;
            check(button && middle >= 0
                    && button->itemText(middle) == QStringLiteral("Middle button (wheel click)")
                    && button->toolTip().contains(QStringLiteral("wheel"), Qt::CaseInsensitive),
                "physical Middle-button choices distinguish pressing the wheel from turning it");
        }
        (void)remap(dialog);
        dialog.reject();
    });
    check(view->viewportNavigationSettings() == original
            && loadViewportNavigationSettings(settingsStore()) == original,
        "Cancel preserves cached and persisted navigation after the complete Middle-button remap draft");
    bool accepted = false;
    drivePreferences(window, [&](QDialog& dialog)
    {
        if ( !remap(dialog) )
        {
            dialog.reject();
            return;
        }
        auto* const box = dialog.findChild<QDialogButtonBox*>();
        if ( auto* const ok = box ? box->button(QDialogButtonBox::Ok) : nullptr )
        {
            accepted = true;
            ok->click();
        }
        else
        {
            dialog.reject();
        }
    });
    auto expected = original;
    expected.mouse[static_cast<std::size_t>(ViewportMouseAction::flyLook)].button = Qt::MiddleButton;
    for ( const auto action : { ViewportMouseAction::orbit, ViewportMouseAction::pan,
            ViewportMouseAction::dolly, ViewportMouseAction::fastDolly } )
    {
        expected.mouse[static_cast<std::size_t>(action)].button = Qt::RightButton;
    }
    check(accepted && view->viewportNavigationSettings() == expected
            && loadViewportNavigationSettings(settingsStore()) == expected,
        "OK persists and immediately publishes Middle-button Fly and all four RMB camera gestures");
    prepareView(*view);
    sendMouse(*view, QEvent::MouseButtonPress, QPointF(210.0, 200.0), Qt::MiddleButton, Qt::MiddleButton);
    check(accepted && view->flyNavigationActive(),
        "the real viewport uses the accepted wheel-click Fly mapping immediately");
    sendMouse(*view, QEvent::MouseButtonRelease, QPointF(210.0, 200.0), Qt::MiddleButton, Qt::NoButton);
}

void testWheelCameraLinksPreservePreferenceTransactions()
{
    NavigationSettingsFixture settingsIsolation{};
    MainWindow window(nullptr, true);
    GlView* const view = window.glViewForTest();
    QAction* const zoomAction = window.findChild<QAction*>("viewport.zoom_in");
    check(view && zoomAction, "wheel link regression uses the real camera and keymap actions");
    if ( !view || !zoomAction )
    {
        return;
    }
    const ViewportNavigationSettings original = view->viewportNavigationSettings();
    const QList<QKeySequence> originalZoom = zoomAction->shortcuts();
    const auto persistedValues = []()
    {
        QVariantMap values{};
        const QSettings settings = settingsStore();
        for ( const QString& key : settings.allKeys() )
        {
            values.insert(key, settings.value(key));
        }
        return values;
    };
    const QVariantMap originalPersisted = persistedValues();
    const auto originalRebuilds = view->meshRebuildCountForTest();
    bool linkAvailable = false;
    for ( const bool accept : { false, true } )
    {
        bool accepted = false;
        drivePreferences(window, [&](QDialog& dialog)
        {
            auto* const hint = dialog.findChild<QLabel*>("pref_mouse_wheel_keyboard_hint");
            auto* const tree = dialog.findChild<QTreeWidget*>("pref_keymap_tree");
            auto* const search = dialog.findChild<QLineEdit*>("pref_keymap_search");
            auto* const zoom = dialog.findChild<QComboBox*>("pref_mouse_zoom_modifiers");
            auto* const inverted = dialog.findChild<QCheckBox*>("pref_mouse_zoom_inverted");
            const QString noteHtml = hint ? hint->text() : QString{};
            linkAvailable = hint
                && noteHtml.startsWith(QStringLiteral("Keymap: "))
                && noteHtml.contains(QStringLiteral("href=\"#camera-zoom\">Zoom In/Out</a>"))
                && noteHtml.contains(QStringLiteral("href=\"#camera-fly-speed\">Fly Speed Up/Down</a>"))
                && !noteHtml.contains(QStringLiteral("Numpad"), Qt::CaseInsensitive)
                && !noteHtml.contains(QStringLiteral("Disabled"), Qt::CaseInsensitive)
                && !noteHtml.contains(QStringLiteral("Invert"), Qt::CaseInsensitive)
                && hint->textInteractionFlags().testFlag(Qt::LinksAccessibleByMouse)
                && hint->textInteractionFlags().testFlag(Qt::LinksAccessibleByKeyboard)
                && (hint->focusPolicy() & Qt::TabFocus) != 0
                && !hint->openExternalLinks()
                && hint->accessibleName().contains(QStringLiteral("Camera"), Qt::CaseInsensitive);
            check(linkAvailable,
                "Wheel scrolling note has separate Zoom and Fly-speed links without wheel-only explanatory text");
            if ( !linkAvailable || !tree || !search || !zoom || !inverted )
            {
                dialog.reject();
                return;
            }
            const auto zoomRows = bindingItems(*tree, "viewport.zoom_in");
            const auto flySpeedRows = bindingItems(*tree, "viewport.fly.speed_up");
            check(zoomRows.size() == 1 && flySpeedRows.size() == 1 && setBindingAlternative(*tree,
                    "viewport.zoom_in", 0, QKeySequence(Qt::Key_F13)),
                "wheel link fixture retains a pending camera shortcut edit");
            if ( zoomRows.size() != 1 || flySpeedRows.size() != 1 )
            {
                dialog.reject();
                return;
            }
            QTreeWidgetItem* const zoomRow = zoomRows.front();
            QTreeWidgetItem* const flySpeedRow = flySpeedRows.front();
            QTreeWidgetItem* const camera = zoomRow->parent();
            zoom->setCurrentIndex(zoom->findData(int(Qt::AltModifier)));
            inverted->setChecked(true);
            int activations = 0;
            QString lastTarget{};
            const auto activated = QObject::connect(hint, &QLabel::linkActivated, &dialog,
                [&](const QString& target) { ++activations; lastTarget = target; });
            for ( const bool keyboard : { true, false } )
            {
                const QString expectedTarget = keyboard
                    ? QStringLiteral("#camera-zoom") : QStringLiteral("#camera-fly-speed");
                QTreeWidgetItem* const targetRow = keyboard ? zoomRow : flySpeedRow;
                search->setText(QStringLiteral("no-camera-bindings-match-this-filter"));
                check(targetRow->isHidden(), "wheel link fixture first hides the Camera bindings with Keymap search");
                tree->collapseAll();
                check(revealMouseControls(dialog), "wheel link fixture shows the actual Mouse Controls note");
                for ( QScrollArea* const scroll : dialog.findChildren<QScrollArea*>() )
                {
                    if ( scroll->isAncestorOf(hint) )
                    {
                        scroll->ensureWidgetVisible(hint);
                    }
                }
                dialog.raise();
                dialog.activateWindow();
                QApplication::setActiveWindow(&dialog);
                QApplication::processEvents(QEventLoop::AllEvents);
                if ( keyboard )
                {
                    hint->setFocus(Qt::TabFocusReason);
                    // QLabel selects its hyperlink through real Tab traversal;
                    // merely delivering FocusIn leaves Return owned by OK.
                    sendNavigationKey(*hint, Qt::Key_Tab);
                    const bool visibleBefore = dialog.isVisible();
                    const int resultBefore = dialog.result();
                    const bool modalBefore = QApplication::activeModalWidget() == &dialog;
                    const bool focusBefore = QApplication::focusWidget() == hint;
                    sendNavigationKey(*hint, Qt::Key_Return);
                    if ( activations != 1 || !dialog.isVisible() || QApplication::activeModalWidget() != &dialog )
                    {
                        std::fprintf(stderr,
                            "WHEEL_KEYMAP_RETURN accept=%d beforeVisible=%d beforeResult=%d beforeModal=%d beforeFocus=%d "
                            "afterVisible=%d afterResult=%d afterModal=%d activations=%d target=%s\n",
                            accept ? 1 : 0, visibleBefore ? 1 : 0, resultBefore, modalBefore ? 1 : 0, focusBefore ? 1 : 0,
                            dialog.isVisible() ? 1 : 0, dialog.result(), QApplication::activeModalWidget() == &dialog ? 1 : 0,
                            activations, lastTarget.toUtf8().constData());
                    }
                }
                else
                {
                    const std::optional<QPoint> anchorPoint = labelLinkPoint(*hint, expectedTarget);
                    check(anchorPoint.has_value(), "wheel keyboard note exposes the requested native mouse anchor");
                    if ( anchorPoint )
                    {
                        sendMouse(*hint, QEvent::MouseButtonPress, *anchorPoint, Qt::LeftButton, Qt::LeftButton);
                        sendMouse(*hint, QEvent::MouseButtonRelease, *anchorPoint, Qt::LeftButton, Qt::NoButton);
                    }
                }
                QApplication::processEvents(QEventLoop::AllEvents);
                auto* const categories = dialog.findChild<QListWidget*>("pref_categories");
                const bool targetReached = activations == (keyboard ? 1 : 2)
                    && lastTarget == expectedTarget && categories && categories->currentItem()
                    && categories->currentItem()->text() == QStringLiteral("Keymap")
                    && search->text().isEmpty() && tree->isVisible()
                    && camera && camera->text(0) == QStringLiteral("Camera")
                    && camera->parent() && camera->parent()->text(0) == QStringLiteral("Viewport")
                    && camera->isExpanded() && camera->parent()->isExpanded()
                    && !targetRow->isHidden() && tree->currentItem() == targetRow
                    && tree->visualItemRect(targetRow).intersects(tree->viewport()->rect())
                    && QApplication::focusWidget() == tree;
                if ( !targetReached )
                {
                    std::fprintf(stderr,
                        "WHEEL_KEYMAP_LINK_TARGET accept=%d keyboard=%d activations=%d target=%s category=%s "
                        "search=%s treeVisible=%d camera=%s parent=%s cameraExpanded=%d parentExpanded=%d "
                        "rowHidden=%d currentMatches=%d rowVisible=%d focusMatches=%d focusClass=%s focusName=%s\n",
                        accept ? 1 : 0, keyboard ? 1 : 0, activations,
                        lastTarget.toUtf8().constData(),
                        categories && categories->currentItem()
                            ? categories->currentItem()->text().toUtf8().constData() : "<none>",
                        search->text().toUtf8().constData(), tree->isVisible() ? 1 : 0,
                        camera ? camera->text(0).toUtf8().constData() : "<none>",
                        camera && camera->parent() ? camera->parent()->text(0).toUtf8().constData() : "<none>",
                        camera && camera->isExpanded() ? 1 : 0,
                        camera && camera->parent() && camera->parent()->isExpanded() ? 1 : 0,
                        targetRow->isHidden() ? 1 : 0, tree->currentItem() == targetRow ? 1 : 0,
                        tree->visualItemRect(targetRow).intersects(tree->viewport()->rect()) ? 1 : 0,
                        QApplication::focusWidget() == tree ? 1 : 0,
                        QApplication::focusWidget() ? QApplication::focusWidget()->metaObject()->className() : "<none>",
                        QApplication::focusWidget() ? QApplication::focusWidget()->objectName().toUtf8().constData() : "<none>");
                }
                check(targetReached,
                    "mouse and keyboard wheel-note links clear search and focus their Camera command groups in this dialog");
                bool allCameraBindingsVisible = true;
                for ( const char* const id : { "viewport.zoom_in", "viewport.zoom_out",
                        "viewport.fly.speed_up", "viewport.fly.speed_down" } )
                {
                    const auto items = bindingItems(*tree, id);
                    allCameraBindingsVisible = allCameraBindingsVisible && items.size() == 1
                        && items.front()->parent() == camera && !items.front()->isHidden();
                }
                const auto* const editor = shortcutEditor(tree->itemWidget(zoomRow, 1));
                const bool draftsRetained = allCameraBindingsVisible && editor && editor->keySequence() == QKeySequence(Qt::Key_F13)
                    && zoom->currentData().toInt() == int(Qt::AltModifier) && inverted->isChecked()
                    && dialog.isVisible() && QApplication::activeModalWidget() == &dialog
                    && view->viewportNavigationSettings() == original && zoomAction->shortcuts() == originalZoom
                    && persistedValues() == originalPersisted
                    && view->meshRebuildCountForTest() == originalRebuilds;
                if ( !draftsRetained )
                {
                    std::fprintf(stderr,
                        "WHEEL_KEYMAP_LINK_DRAFT accept=%d keyboard=%d allCameraVisible=%d editor=%s "
                        "mouseModifier=%d invert=%d dialogVisible=%d modalMatches=%d modalClass=%s "
                        "liveNavigationMatches=%d liveZoomMatches=%d persistedMatches=%d meshMatches=%d\n",
                        accept ? 1 : 0, keyboard ? 1 : 0, allCameraBindingsVisible ? 1 : 0,
                        editor ? editor->keySequence().toString(QKeySequence::PortableText).toUtf8().constData() : "<none>",
                        zoom->currentData().toInt(), inverted->isChecked() ? 1 : 0,
                        dialog.isVisible() ? 1 : 0, QApplication::activeModalWidget() == &dialog ? 1 : 0,
                        QApplication::activeModalWidget() ? QApplication::activeModalWidget()->metaObject()->className() : "<none>",
                        view->viewportNavigationSettings() == original ? 1 : 0,
                        zoomAction->shortcuts() == originalZoom ? 1 : 0,
                        persistedValues() == originalPersisted ? 1 : 0,
                        view->meshRebuildCountForTest() == originalRebuilds ? 1 : 0);
                }
                check(draftsRetained,
                    "wheel-note navigation preserves pending mouse and keymap edits without publishing settings or closing Preferences");
            }
            QObject::disconnect(activated);
            if ( accept )
            {
                auto* const box = dialog.findChild<QDialogButtonBox*>();
                auto* const ok = box ? box->button(QDialogButtonBox::Ok) : nullptr;
                check(ok && ok->isEnabled(), "pending wheel and Camera edits remain valid after link navigation");
                if ( ok && ok->isEnabled() )
                {
                    accepted = true;
                    ok->click();
                    return;
                }
            }
            dialog.reject();
        });
        if ( !accept )
        {
            check(view->viewportNavigationSettings() == original && zoomAction->shortcuts() == originalZoom
                    && persistedValues() == originalPersisted,
                "Cancel after wheel-note navigation discards all pending mouse and Camera edits");
            if ( !linkAvailable )
            {
                return;
            }
        }
        else
        {
            auto expected = original;
            expected.zoom.modifiers = Qt::AltModifier;
            expected.zoom.inverted = true;
            const QSettings settings = settingsStore();
            check(accepted && view->viewportNavigationSettings() == expected
                    && loadViewportNavigationSettings(settings) == expected
                    && zoomAction->shortcuts() == QList<QKeySequence>{ QKeySequence(Qt::Key_F13) }
                    && settings.value("keymap/shortcuts/viewport.zoom_in").toStringList()
                        == QStringList{ QKeySequence(Qt::Key_F13).toString(QKeySequence::PortableText) },
                "OK after wheel-note navigation publishes the preserved mouse and Camera edits together");
        }
    }
}

void testCameraLinkPairHighlightRasterExpiryAndRestart()
{
    NavigationSettingsFixture settingsIsolation{};
    MainWindow window(nullptr, true);
    drivePreferences(window, [&](QDialog& dialog)
    {
        auto* const hint = dialog.findChild<QLabel*>("pref_mouse_wheel_keyboard_hint");
        auto* const tree = dialog.findChild<QTreeWidget*>("pref_keymap_tree");
        auto* const timer = tree
            ? tree->findChild<QTimer*>("pref_keymap_link_highlight_timer") : nullptr;
        const auto zoomInRows = tree ? bindingItems(*tree, "viewport.zoom_in")
                                     : std::vector<QTreeWidgetItem*>{};
        const auto zoomOutRows = tree ? bindingItems(*tree, "viewport.zoom_out")
                                      : std::vector<QTreeWidgetItem*>{};
        const auto flyUpRows = tree ? bindingItems(*tree, "viewport.fly.speed_up")
                                    : std::vector<QTreeWidgetItem*>{};
        const auto flyDownRows = tree ? bindingItems(*tree, "viewport.fly.speed_down")
                                      : std::vector<QTreeWidgetItem*>{};
        const bool available = hint && tree && timer
            && zoomInRows.size() == 1 && zoomOutRows.size() == 1
            && flyUpRows.size() == 1 && flyDownRows.size() == 1;
        check(available,
            "paired camera-link highlight test finds the real note, timer, and four Camera commands");
        if ( !available )
        {
            dialog.reject();
            return;
        }

        QTreeWidgetItem* const zoomIn = zoomInRows.front();
        QTreeWidgetItem* const zoomOut = zoomOutRows.front();
        QTreeWidgetItem* const flyUp = flyUpRows.front();
        QTreeWidgetItem* const flyDown = flyDownRows.front();
        for ( QTreeWidgetItem* item : { zoomIn, zoomOut, flyUp, flyDown } )
        {
            for ( QTreeWidgetItem* parent = item; parent; parent = parent->parent() )
            {
                parent->setExpanded(true);
            }
        }
        selectPreferenceCategory(dialog, QStringLiteral("Keymap"));
        neutralizeKeymapTreeSelection(*tree);
        const QImage baselineZoomIn = keymapRowImage(*tree, zoomIn);
        const QImage baselineZoomOut = keymapRowImage(*tree, zoomOut);
        const QImage baselineFlyUp = keymapRowImage(*tree, flyUp);
        const QImage baselineFlyDown = keymapRowImage(*tree, flyDown);
        check(!baselineZoomIn.isNull() && !baselineZoomOut.isNull()
                && !baselineFlyUp.isNull() && !baselineFlyDown.isNull(),
            "camera-link raster regression captures all four unhighlighted command rows");

        int activations = 0;
        QString lastTarget{};
        const auto activated = QObject::connect(hint, &QLabel::linkActivated, &dialog,
            [&](const QString& target) { ++activations; lastTarget = target; });
        const auto clickLink = [&](const QString& target)
        {
            const int previousActivations = activations;
            if ( !revealMouseControls(dialog) )
            {
                return false;
            }
            for ( QScrollArea* const scroll : dialog.findChildren<QScrollArea*>() )
            {
                if ( scroll->isAncestorOf(hint) )
                {
                    scroll->ensureWidgetVisible(hint);
                }
            }
            QApplication::processEvents(QEventLoop::AllEvents);
            const std::optional<QPoint> point = labelLinkPoint(*hint, target);
            if ( !point )
            {
                return false;
            }
            sendMouse(*hint, QEvent::MouseButtonPress, *point, Qt::LeftButton, Qt::LeftButton);
            sendMouse(*hint, QEvent::MouseButtonRelease, *point, Qt::LeftButton, Qt::NoButton);
            QApplication::processEvents(QEventLoop::AllEvents);
            return activations == previousActivations + 1 && lastTarget == target;
        };
        const auto captureZoomPair = [&]()
        {
            neutralizeKeymapTreeSelection(*tree);
            return std::pair{ keymapRowImage(*tree, zoomIn), keymapRowImage(*tree, zoomOut) };
        };
        const auto captureFlyPair = [&]()
        {
            neutralizeKeymapTreeSelection(*tree);
            return std::pair{ keymapRowImage(*tree, flyUp), keymapRowImage(*tree, flyDown) };
        };
        const auto rowFullyVisible = [&](QTreeWidgetItem* const item)
        {
            return item && tree->viewport()->rect().contains(tree->visualItemRect(item));
        };
        const auto scrollBelowPair = [&](QTreeWidgetItem* const first,
                                         QTreeWidgetItem* const second)
        {
            selectPreferenceCategory(dialog, QStringLiteral("Keymap"));
            QApplication::processEvents(QEventLoop::AllEvents);
            QScrollBar* const scroll = tree->verticalScrollBar();
            scroll->setValue(scroll->maximum());
            QApplication::processEvents(QEventLoop::AllEvents);
            const QRect firstRect = tree->visualItemRect(first);
            const QRect secondRect = tree->visualItemRect(second);
            return scroll->maximum() > 0 && scroll->value() == scroll->maximum()
                && firstRect.isValid() && secondRect.isValid()
                && firstRect.bottom() < tree->viewport()->rect().top()
                && secondRect.bottom() < tree->viewport()->rect().top();
        };

        check(scrollBelowPair(zoomIn, zoomOut),
            "Zoom link starts with the keymap tree scrolled below both target rows");
        check(clickLink(QStringLiteral("#camera-zoom"))
                && rowFullyVisible(zoomIn) && rowFullyVisible(zoomOut)
                && timer->isActive()
                && timer->interval() == 500
                && timer->remainingTime() > 0 && timer->remainingTime() <= 500,
            "Zoom link scrolls both rows fully into view and arms the tree-owned 500 ms timer");
        auto activeZoom = captureZoomPair();
        check(activeZoom.first != baselineZoomIn && activeZoom.second != baselineZoomOut,
            "Zoom link raster highlights both Zoom In and Zoom Out rows");
        const auto inactiveFly = captureFlyPair();
        check(inactiveFly.first == baselineFlyUp && inactiveFly.second == baselineFlyDown,
            "Zoom link leaves both Fly speed rows unhighlighted");

        processEventsFor(200);
        check(clickLink(QStringLiteral("#camera-zoom")) && timer->isActive(),
            "clicking the same Camera link while its timer is armed safely restarts it");
        processEventsFor(350);
        activeZoom = captureZoomPair();
        check(activeZoom.first != baselineZoomIn && activeZoom.second != baselineZoomOut,
            "same-link activation keeps both row highlights visible beyond the first deadline");
        processEventsFor(250);
        const auto expiredZoom = captureZoomPair();
        check(expiredZoom.first == baselineZoomIn && expiredZoom.second == baselineZoomOut
                && !timer->isActive(),
            "both Zoom row highlights expire after the restarted 500 ms interval");

        check(clickLink(QStringLiteral("#camera-zoom")) && timer->isActive(),
            "Zoom target can be reactivated before switching the highlighted pair");
        check(scrollBelowPair(flyUp, flyDown),
            "Fly speed link starts with the keymap tree scrolled below both target rows");
        check(clickLink(QStringLiteral("#camera-fly-speed"))
                && rowFullyVisible(flyUp) && rowFullyVisible(flyDown)
                && timer->isActive(),
            "switching from Zoom to Fly speed restarts the same timer for the new pair");
        const auto clearedZoom = captureZoomPair();
        const auto activeFly = captureFlyPair();
        check(clearedZoom.first == baselineZoomIn && clearedZoom.second == baselineZoomOut,
            "Fly speed navigation immediately clears both previously highlighted Zoom rows");
        check(activeFly.first != baselineFlyUp && activeFly.second != baselineFlyDown,
            "Fly speed link raster highlights both Fly Speed Up and Fly Speed Down rows");
        processEventsFor(550);
        const auto expiredFly = captureFlyPair();
        check(expiredFly.first == baselineFlyUp && expiredFly.second == baselineFlyDown
                && !timer->isActive(),
            "both Fly speed row highlights expire after their own 500 ms deadline");
        QObject::disconnect(activated);
        dialog.reject();
    });
}

void testCameraLinkHighlightTimerIsSafeWhenPreferencesCloses()
{
    NavigationSettingsFixture settingsIsolation{};
    MainWindow window(nullptr, true);
    QPointer<QDialog> preferencesPointer{};
    QPointer<QTreeWidget> treePointer{};
    QPointer<QTimer> timerPointer{};
    bool wasArmed = false;
    drivePreferences(window, [&](QDialog& dialog)
    {
        preferencesPointer = &dialog;
        auto* const hint = dialog.findChild<QLabel*>("pref_mouse_wheel_keyboard_hint");
        auto* const tree = dialog.findChild<QTreeWidget*>("pref_keymap_tree");
        auto* const timer = tree
            ? tree->findChild<QTimer*>("pref_keymap_link_highlight_timer") : nullptr;
        treePointer = tree;
        timerPointer = timer;
        check(hint && tree && timer && revealMouseControls(dialog),
            "timer destruction fixture reaches the live Camera note and tree-owned timer");
        if ( hint && tree && timer )
        {
            const std::optional<QPoint> point = labelLinkPoint(*hint, QStringLiteral("#camera-zoom"));
            if ( point )
            {
                sendMouse(*hint, QEvent::MouseButtonPress, *point, Qt::LeftButton, Qt::LeftButton);
                sendMouse(*hint, QEvent::MouseButtonRelease, *point, Qt::LeftButton, Qt::NoButton);
                QApplication::processEvents(QEventLoop::AllEvents);
                wasArmed = timer->isActive() && timer->remainingTime() > 0;
            }
            check(wasArmed, "Preferences closes while the pair-highlight timer is armed");
        }
        dialog.reject();
    });
    check(wasArmed && preferencesPointer.isNull() && treePointer.isNull()
            && timerPointer.isNull(),
        "closing Preferences destroys its active highlight timer with the owning keymap tree");
    processEventsFor(600);
    check(preferencesPointer.isNull() && treePointer.isNull() && timerPointer.isNull(),
        "no stale highlight timeout accesses the destroyed Preferences dialog or rows");
}

void testWheelModifierControlsStayCompactAndReadable()
{
    NavigationSettingsFixture settingsIsolation{};
    MainWindow window(nullptr, true);
    drivePreferences(window, [&](QDialog& dialog)
    {
        auto* const wheel = dialog.findChild<QWidget*>("pref_mouse_wheel_section");
        auto* const upper = dialog.findChild<QComboBox*>("pref_mouse_pick_modifiers");
        check(wheel && upper && revealMouseControls(dialog),
            "wheel layout regression reaches the expanded Mouse Controls section");
        if ( !wheel || !upper )
        {
            dialog.reject();
            return;
        }
        const int narrowWidth = dialog.minimumWidth();
        const int wideWidth = narrowWidth + 40 * dialog.fontMetrics().height();
        for ( const int width : { narrowWidth, wideWidth } )
        {
            dialog.resize(width, std::max(dialog.minimumHeight(), 720));
            QApplication::processEvents(QEventLoop::AllEvents);
            for ( const char* const id : { "zoom", "fly_speed" } )
            {
                auto* const modifiers = dialog.findChild<QComboBox*>(
                    QStringLiteral("pref_mouse_%1_modifiers").arg(QString::fromLatin1(id)));
                check(modifiers && modifiers->isVisible(), "wheel modifier dropdown remains visible at both dialog widths");
                if ( !modifiers )
                {
                    continue;
                }
                int longest = 0;
                int longestWidth = 0;
                for ( int index = 0; index < modifiers->count(); ++index )
                {
                    const int textWidth = modifiers->fontMetrics().horizontalAdvance(modifiers->itemText(index));
                    if ( textWidth > longestWidth )
                    {
                        longest = index;
                        longestWidth = textWidth;
                    }
                }
                modifiers->setCurrentIndex(longest);
                QApplication::processEvents(QEventLoop::AllEvents);
                const int visualTolerance = upper->fontMetrics().height();
                check(modifiers->width() <= upper->width() + visualTolerance
                        && 4 * modifiers->width() <= 3 * wheel->width(),
                    "wheel modifier dropdowns match upper mouse modifiers instead of stretching across the section");
                QStyleOptionComboBox option{};
                option.initFrom(modifiers);
                option.editable = modifiers->isEditable();
                option.currentText = modifiers->currentText();
                option.frame = true;
                const QRect textRect = modifiers->style()->subControlRect(
                    QStyle::CC_ComboBox, &option, QStyle::SC_ComboBoxEditField, modifiers);
                check(modifiers->currentIndex() == longest && textRect.width() >= longestWidth,
                    "compact wheel dropdowns show the full longest selected modifier label with style padding and arrow space");
                modifiers->showPopup();
                (void)waitForNavigationUi([modifiers]() { return modifiers->view()->isVisible(); });
                const bool popupReadable = modifiers->view()->isVisible()
                    && modifiers->view()->viewport()->width() >= longestWidth
                    && modifiers->model()->flags(modifiers->model()->index(longest, 0)).testFlag(Qt::ItemIsEnabled);
                if ( !popupReadable )
                {
                    std::fprintf(stderr,
                        "WHEEL_MODIFIER_POPUP id=%s dialogWidth=%d comboWidth=%d upperWidth=%d "
                        "popupVisible=%d viewportWidth=%d longestWidth=%d longestIndex=%d "
                        "flags=%u comboEnabled=%d modalMatches=%d popupMatches=%d activeWindowMatches=%d\n",
                        id, dialog.width(), modifiers->width(), upper->width(), modifiers->view()->isVisible() ? 1 : 0,
                        modifiers->view()->viewport()->width(), longestWidth, longest,
                        static_cast<unsigned int>(modifiers->model()->flags(modifiers->model()->index(longest, 0))),
                        modifiers->isEnabled() ? 1 : 0, QApplication::activeModalWidget() == &dialog ? 1 : 0,
                        QApplication::activePopupWidget() == modifiers->view()->window() ? 1 : 0,
                        QApplication::activeWindow() == &dialog ? 1 : 0);
                }
                check(popupReadable,
                    "compact wheel dropdown lists retain every selectable full-width modifier label");
                modifiers->hidePopup();
            }
        }
        dialog.reject();
    });
}

void testZoomAndFlySpeedCommandsExposeEditableKeys()
{
    NavigationSettingsFixture settingsIsolation{};
    MainWindow window(nullptr, true);
    const std::array<const char*, 4> ids{
        "viewport.zoom_in", "viewport.zoom_out", "viewport.fly.speed_up", "viewport.fly.speed_down"};
    const std::array<const char*, 4> labels{
        "Zoom In", "Zoom Out", "Fly Speed Up", "Fly Speed Down"};
    const std::array<QKeySequence, 4> defaults{
        QKeySequence(Qt::KeypadModifier | Qt::Key_Plus),
        QKeySequence(Qt::KeypadModifier | Qt::Key_Minus),
        QKeySequence(Qt::KeypadModifier | Qt::Key_Plus),
        QKeySequence(Qt::KeypadModifier | Qt::Key_Minus)};
    const std::array<QKeySequence, 4> remapped{
        QKeySequence(Qt::Key_F13), QKeySequence(Qt::Key_F15),
        QKeySequence(Qt::Key_F13), QKeySequence(Qt::Key_F15)};
    for ( std::size_t index = 0; index < ids.size(); ++index )
    {
        QAction* const action = window.findChild<QAction*>(ids[index]);
        check(action && action->text() == QString::fromLatin1(labels[index])
                && action->shortcuts() == QList<QKeySequence>{defaults[index]},
            "Zoom and persistent Fly-speed commands exist with separate rebindable Numpad +/- defaults");
    }
    bool accepted = false;
    drivePreferences(window, [&](QDialog& dialog)
    {
        auto* const tree = dialog.findChild<QTreeWidget*>("pref_keymap_tree");
        auto* const error = dialog.findChild<QLabel*>("pref_keymap_binding_error");
        auto* const box = dialog.findChild<QDialogButtonBox*>();
        auto* const ok = box ? box->button(QDialogButtonBox::Ok) : nullptr;
        bool complete = tree && error && ok;
        for ( std::size_t index = 0; tree && index < ids.size(); ++index )
        {
            const auto rows = bindingItems(*tree, ids[index]);
            auto* const editor = rows.size() == 1
                ? shortcutEditor(tree->itemWidget(rows.front(), 1)) : nullptr;
            complete &= editor && rows.front()->text(0) == QString::fromLatin1(labels[index])
                && rows.front()->parent()
                && rows.front()->parent()->text(0) == QStringLiteral("Camera")
                && editor->keySequence() == defaults[index];
        }
        check(complete, "Preferences exposes all four keyboard commands under Viewport > Camera");
        if ( !complete )
        {
            dialog.reject();
            return;
        }
        check(error->text().isEmpty() && ok->isEnabled(),
            "exclusive Fly and ordinary Zoom can reuse Numpad +/- without a false keymap conflict");
        setBindingAlternative(*tree, ids[1], 0, defaults[0]);
        check(!ok->isEnabled() && error->text().contains(QStringLiteral("Zoom In"))
                && error->text().contains(QStringLiteral("Zoom Out")),
            "Zoom In and Zoom Out reject the same ordinary viewport binding");
        setBindingAlternative(*tree, ids[1], 0, defaults[1]);
        setBindingAlternative(*tree, ids[3], 0, defaults[2]);
        check(!ok->isEnabled() && error->text().contains(QStringLiteral("Fly Speed Up"))
                && error->text().contains(QStringLiteral("Fly Speed Down")),
            "Fly Speed Up and Down reject overlapping bindings in the exclusive Fly context");
        setBindingAlternative(*tree, ids[3], 0, defaults[3]);
        for ( std::size_t index = 0; index < ids.size(); ++index )
        {
            check(setBindingAlternative(*tree, ids[index], 0, remapped[index]),
                "each keyboard camera command can be changed through the real Preferences editor");
        }
        for ( const char* const id : { ids[0], ids[2] } )
        {
            const auto rows = bindingItems(*tree, id);
            QWidget* const controls = tree->itemWidget(rows.front(), 1);
            auto* const add = controls->findChild<QToolButton*>("pref_keymap_add");
            check(add != nullptr, "Zoom and Fly speed commands support alternative shortcut bindings");
            if ( add )
            {
                add->click();
                shortcutEditor(controls)->setKeySequence(QKeySequence(Qt::Key_F14));
            }
        }
        setBindingAlternative(*tree, ids[1], 0, QKeySequence(Qt::Key_F14));
        check(!ok->isEnabled() && !error->text().isEmpty(),
            "a conflict with a secondary Zoom alternative also disables Preferences acceptance");
        setBindingAlternative(*tree, ids[1], 0, remapped[1]);
        check(ok->isEnabled() && error->text().isEmpty(),
            "separately remapped Fly and Zoom alternatives can reuse keys across their exclusive contexts");
        accepted = ok->isEnabled();
        if ( accepted ) ok->click();
        else dialog.reject();
    });
    check(accepted, "valid keyboard alternatives publish from Preferences");
    if ( accepted )
    {
        GlView* const view = window.glViewForTest();
        if ( view )
        {
            window.setAttribute(Qt::WA_DontShowOnScreen);
            window.show();
            prepareView(*view);
            QApplication::setActiveWindow(&window);
            view->setFocus(Qt::OtherFocusReason);
            QApplication::processEvents();
            sendNavigationKey(*view, Qt::Key_unknown);
            view->setCameraProjectionType(app::ViewportCamera::ProjectionType::Perspective);
            view->setCameraOrbitDistance(8.0f);
            sendNavigationKey(*view, Qt::Key_F13);
            sendNavigationKey(*view, Qt::Key_F14);
            check(close(view->cameraOrbitDistance(), 6.48f, 1.0e-5),
                "accepting Preferences immediately executes both remapped Zoom alternatives on the same live viewport");
            const float distance = view->cameraOrbitDistance();
            const QPointF point(210.0, 200.0);
            view->resetFlySpeedMultiplier();
            sendMouse(*view, QEvent::MouseButtonPress, point, Qt::RightButton, Qt::RightButton);
            view->setFlyNavigationDirection(GlView::FlyDirection::Forward, true);
            const auto travel = [&]()
            {
                const QVector3D before = view->cameraTarget();
                view->tickFlyNavigationForTest();
                return (view->cameraTarget() - before).length();
            };
            const float baseline = travel();
            sendNavigationKey(*view, Qt::Key_F13);
            sendNavigationKey(*view, Qt::Key_F14);
            check(baseline > 0.0f && close(travel(), baseline * 1.21f, 1.0e-5)
                    && view->cameraOrbitDistance() == distance,
                "accepting Preferences immediately executes both Fly speed alternatives in the exclusive live owner");
            sendMouse(*view, QEvent::MouseButtonRelease, point, Qt::RightButton, Qt::NoButton);
        }
    }
    MainWindow restored(nullptr, true);
    for ( std::size_t index = 0; index < ids.size(); ++index )
    {
        const QList<QKeySequence> expected = index == 0u || index == 2u
            ? QList<QKeySequence>{remapped[index], QKeySequence(Qt::Key_F14)}
            : QList<QKeySequence>{remapped[index]};
        QAction* const live = window.findChild<QAction*>(ids[index]);
        QAction* const saved = restored.findChild<QAction*>(ids[index]);
        check(live && saved && live->shortcuts() == expected && saved->shortcuts() == expected,
            "separate Zoom/Fly speed alternatives persist and reconstruct without moving between command IDs");
    }
}

void testKeyboardZoomIgnoresWheelSettingsAndRetainsEditorFocus()
{
    NavigationSettingsFixture settingsIsolation{};
    MainWindow window(nullptr, true);
    window.setAttribute(Qt::WA_DontShowOnScreen);
    window.show();
    GlView* const view = window.glViewForTest();
    if ( !view ) return;
    prepareView(*view);
    QAction* const zoomIn = window.findChild<QAction*>("viewport.zoom_in");
    QAction* const zoomOut = window.findChild<QAction*>("viewport.zoom_out");
    check(zoomIn && zoomOut, "keyboard Zoom regression has executable Zoom In and Out actions");
    if ( !zoomIn || !zoomOut ) return;
    auto navigation = view->viewportNavigationSettings();
    navigation.zoom.enabled = false;
    navigation.zoom.modifiers = Qt::AltModifier;
    navigation.zoom.inverted = true;
    navigation.invertHorizontal = true;
    navigation.invertVertical = true;
    view->setViewportNavigationSettings(navigation);
    QApplication::setActiveWindow(&window);
    view->setFocus(Qt::OtherFocusReason);
    QApplication::processEvents();
    const auto rebuilds = view->meshRebuildCountForTest();
    const auto undoDepth = window.undoDepthForTest();
    const auto fullSnapshots = window.undoSnapshotStatsForTest().full;
    const auto* const sectors = window.documentStateForTest().level.sectors.data();
    const QString title = window.windowTitle();
    window.resetRecoveryEditorStateCaptureCountForTest();
    for ( const auto projection : { app::ViewportCamera::ProjectionType::Perspective,
            app::ViewportCamera::ProjectionType::Orthographic } )
    {
        view->setCameraProjectionType(projection);
        view->setCameraOrbitDistance(8.0f);
        view->setCameraOrthoScale(4.0f);
        const float original = projection == app::ViewportCamera::ProjectionType::Perspective
            ? view->cameraOrbitDistance() : view->cameraOrthoScale();
        const auto amount = [&]()
        {
            return projection == app::ViewportCamera::ProjectionType::Perspective
                ? view->cameraOrbitDistance() : view->cameraOrthoScale();
        };
        sendWheel(*view, Qt::AltModifier);
        check(amount() == original, "Disabled wheel Zoom still blocks wheel input independently of keyboard commands");
        sendNavigationKey(*view, Qt::Key_Plus);
        sendNavigationKey(*view, Qt::Key_Minus);
        check(amount() == original, "unmodified main-row +/- do not match default Numpad Zoom commands");
        sendNavigationKey(*view, Qt::Key_Plus, Qt::KeypadModifier);
        const float zoomed = amount();
        check(close(zoomed, original * (projection == app::ViewportCamera::ProjectionType::Perspective
                ? 0.9f : 1.0f / 0.9f), 1.0e-5),
            "Numpad + zooms one ordinary wheel step in either projection even with wheel Disabled/Invert/Alt set");
        sendNavigationKey(*view, Qt::Key_Plus, Qt::KeypadModifier, true);
        check(close(amount(), zoomed * (projection == app::ViewportCamera::ProjectionType::Perspective
                ? 0.9f : 1.0f / 0.9f), 1.0e-5),
            "held Numpad + auto-repeat advances keyboard Zoom by one step per repeated press");
        sendNavigationKey(*view, Qt::Key_Minus, Qt::KeypadModifier);
        sendNavigationKey(*view, Qt::Key_Minus, Qt::KeypadModifier);
        check(close(amount(), original, 1.0e-5), "Numpad - reverses keyboard Zoom with the same projection-aware step");
    }
    view->setCameraProjectionType(app::ViewportCamera::ProjectionType::Perspective);
    const float minimumDistance = view->recoveryState().editor.orbit.radius * 0.05f;
    view->setCameraOrbitDistance(1.0f);
    for ( int step = 0; step < 100; ++step ) zoomIn->trigger();
    check(close(view->cameraOrbitDistance(), minimumDistance, 1.0e-5),
        "keyboard perspective Zoom retains the existing orbit-radius minimum distance");
    view->setCameraProjectionType(app::ViewportCamera::ProjectionType::Orthographic);
    view->setCameraOrthoScale(0.01f);
    sendNavigationKey(*view, Qt::Key_Minus, Qt::KeypadModifier);
    check(view->cameraOrthoScale() == 0.01f, "keyboard orthographic Zoom Out retains the existing 0.01 minimum");
    view->setCameraOrthoScale(1.0e6f);
    sendNavigationKey(*view, Qt::Key_Plus, Qt::KeypadModifier);
    check(view->cameraOrthoScale() == 1.0e6f, "keyboard orthographic Zoom In retains the existing 1000000 maximum");
    check(view->meshRebuildCountForTest() == rebuilds && window.undoDepthForTest() == undoDepth
            && window.undoSnapshotStatsForTest().full == fullSnapshots
            && window.documentStateForTest().level.sectors.data() == sectors
            && window.windowTitle() == title && window.recoveryEditorStateCaptureCountForTest() == 0u,
        "keyboard Zoom does not rebuild meshes, mutate authored history/data, mark the document dirty, or capture recovery on its hot path");

    QLineEdit editor(&window);
    editor.setGeometry(30, 30, 180, 28);
    editor.show();
    editor.setFocus(Qt::OtherFocusReason);
    QApplication::processEvents();
    const float beforeText = view->cameraOrthoScale();
    sendNavigationKey(editor, Qt::Key_Plus, Qt::NoModifier, false, QStringLiteral("+"));
    sendNavigationKey(editor, Qt::Key_Minus, Qt::KeypadModifier, false, QStringLiteral("-"));
    check(editor.text() == QStringLiteral("+-") && view->cameraOrthoScale() == beforeText,
        "typed main-row and keypad +/- stay in a native editor and leave viewport Zoom unchanged");
    window.activateSurfaceEditModeForTest();
    QAction* const uvEditor = window.findChild<QAction*>("surface.uv.editor");
    if ( uvEditor ) uvEditor->trigger();
    QApplication::processEvents();
    QWidget* const canvas = window.findChild<QWidget*>("uvEditorCanvas");
    check(canvas != nullptr, "Zoom focus regression has the actual UV canvas");
    if ( canvas )
    {
        canvas->setFocus(Qt::OtherFocusReason);
        QAction uvZoom("UV-local Numpad zoom", canvas);
        uvZoom.setShortcut(QKeySequence(Qt::KeypadModifier | Qt::Key_Plus));
        uvZoom.setShortcutContext(Qt::WidgetShortcut);
        canvas->addAction(&uvZoom);
        int uvCommands = 0;
        QObject::connect(&uvZoom, &QAction::triggered, canvas, [&]() { ++uvCommands; });
        const float beforeUv = view->cameraOrthoScale();
        sendNavigationKey(*canvas, Qt::Key_Plus, Qt::KeypadModifier);
        check(uvCommands == 1 && view->cameraOrthoScale() == beforeUv,
            "the focused UV canvas retains a matching local keypad command without invoking viewport Zoom");
    }
}

void testMiddleButtonFlyKeyboardSpeedStepsAndClamps()
{
    NavigationSettingsFixture settingsIsolation{};
    MainWindow window(nullptr, true);
    window.setAttribute(Qt::WA_DontShowOnScreen);
    window.show();
    GlView* const view = window.glViewForTest();
    if ( !view ) return;
    prepareView(*view);
    QAction* const speedUp = window.findChild<QAction*>("viewport.fly.speed_up");
    QAction* const speedDown = window.findChild<QAction*>("viewport.fly.speed_down");
    check(speedUp && speedDown, "wheel-click Fly has separately executable keyboard speed-step commands");
    if ( !speedUp || !speedDown ) return;
    auto navigation = view->viewportNavigationSettings();
    navigation.mouse[static_cast<std::size_t>(ViewportMouseAction::flyLook)].button = Qt::MiddleButton;
    for ( const auto action : { ViewportMouseAction::orbit, ViewportMouseAction::pan,
            ViewportMouseAction::dolly, ViewportMouseAction::fastDolly } )
    {
        navigation.mouse[static_cast<std::size_t>(action)].button = Qt::RightButton;
    }
    navigation.flySpeed.enabled = false;
    navigation.flySpeed.inverted = true;
    navigation.flySpeed.modifiers = Qt::AltModifier;
    view->setViewportNavigationSettings(navigation);
    view->setCameraProjectionType(app::ViewportCamera::ProjectionType::Perspective);
    view->setOrbitViewImmediate(0.0f, 0.0f, 0.0f);
    view->setCameraOrbitDistance(8.0f);
    view->resetFlySpeedMultiplier();
    QApplication::setActiveWindow(&window);
    view->setFocus(Qt::OtherFocusReason);
    QApplication::processEvents();
    // Direct command delivery is also scoped to the captured Fly owner.
    sendNavigationKey(*view, Qt::Key_unknown);
    const float normalSpeed = std::max(0.1f, view->recoveryState().editor.orbit.radius * 0.05f) / 30.0f;
    speedUp->trigger();
    speedDown->trigger();
    speedUp->trigger();
    const QPointF point(210.0, 200.0);
    sendMouse(*view, QEvent::MouseButtonPress, point, Qt::MiddleButton, Qt::MiddleButton);
    check(view->flyNavigationActive(), "Middle-button Fly starts while wheel speed scrolling is disabled");
    view->setFlyNavigationDirection(GlView::FlyDirection::Forward, true);
    const auto travel = [&]()
    {
        const QVector3D before = view->cameraTarget();
        view->tickFlyNavigationForTest();
        return (view->cameraTarget() - before).length();
    };
    const float baseline = travel();
    const float distance = view->cameraOrbitDistance();
    const auto rebuilds = view->meshRebuildCountForTest();
    const auto undoDepth = window.undoDepthForTest();
    const auto fullSnapshots = window.undoSnapshotStatsForTest().full;
    const auto* const sectors = window.documentStateForTest().level.sectors.data();
    const QString title = window.windowTitle();
    window.resetRecoveryEditorStateCaptureCountForTest();
    check(baseline > 0.0f && close(baseline, normalSpeed, 1.0e-5),
        "speed commands delivered outside Fly do not change the speed used on its next entry");
    sendWheel(*view, Qt::AltModifier);
    sendNavigationKey(*view, Qt::Key_Plus, Qt::NoModifier);
    check(close(travel(), baseline, 1.0e-5) && view->cameraOrbitDistance() == distance,
        "disabled wheel and main-row + do not alter keyboard Fly speed or leak into camera Zoom");
    QKeyEvent press(QEvent::KeyPress, Qt::Key_Plus, Qt::KeypadModifier);
    QKeyEvent repeated(QEvent::KeyPress, Qt::Key_Plus, Qt::KeypadModifier, QString{}, true);
    QKeyEvent release(QEvent::KeyRelease, Qt::Key_Plus, Qt::KeypadModifier);
    QApplication::sendEvent(view, &press);
    check(close(travel(), baseline * 1.1f, 1.0e-5) && view->cameraOrbitDistance() == distance,
        "Numpad + changes the persistent Fly multiplier while MMB owns input and never invokes Zoom");
    QApplication::sendEvent(view, &repeated);
    check(close(travel(), baseline * 1.21f, 1.0e-5),
        "held Numpad + repeats persistent Fly speed steps rather than suppressing them as held movement aliases");
    QApplication::sendEvent(view, &release);
    check(close(travel(), baseline * 1.21f, 1.0e-5),
        "releasing Fly Speed Up retains the chosen multiplier instead of undoing a temporary speed hold");
    sendNavigationKey(*view, Qt::Key_Minus, Qt::KeypadModifier);
    sendNavigationKey(*view, Qt::Key_Minus, Qt::KeypadModifier);
    check(close(travel(), baseline, 1.0e-5), "Numpad - is the inverse persistent Fly speed step");
    for ( const auto modifier : { Qt::ControlModifier, Qt::ShiftModifier } )
    {
        const int key = modifier == Qt::ControlModifier ? Qt::Key_Control : Qt::Key_Shift;
        const float holdFactor = modifier == Qt::ControlModifier ? 0.5f : 2.0f;
        QKeyEvent hold(QEvent::KeyPress, key, Qt::NoModifier);
        check(hold.modifiers() == modifier, "speed-step fixture presses the exact temporary speed modifier");
        QApplication::sendEvent(view, &hold);
        check(close(travel(), baseline * holdFactor, 1.0e-5),
            "temporary Ctrl/Shift Fly multipliers retain their established half/double rates");
        sendNavigationKey(*view, Qt::Key_Plus, Qt::KeypadModifier | modifier);
        check(close(travel(), baseline * holdFactor * 1.1f, 1.0e-5)
                && view->cameraOrbitDistance() == distance,
            "persistent Numpad speed steps work with Ctrl/Shift held while keeping the temporary multiplier");
        QKeyEvent releaseHold(QEvent::KeyRelease, key, modifier);
        check(releaseHold.modifiers() == Qt::NoModifier,
            "speed-step fixture releases its temporary multiplier without another held modifier");
        QApplication::sendEvent(view, &releaseHold);
        check(close(travel(), baseline * 1.1f, 1.0e-5),
            "releasing temporary speed restores the chosen persistent multiplier instead of its old baseline");
        sendNavigationKey(*view, Qt::Key_Minus, Qt::KeypadModifier);
        check(close(travel(), baseline, 1.0e-5), "an inverse speed step restores the baseline after a temporary hold");
    }
    sendMouse(*view, QEvent::MouseButtonRelease, point, Qt::RightButton, Qt::MiddleButton);
    check(view->flyNavigationActive(), "an unrelated RMB release preserves the owner of keyboard Fly speed steps");
    for ( int step = 0; step < 60; ++step ) speedUp->trigger();
    check(close(travel(), baseline * 100.0f, 1.0e-4), "keyboard Fly Speed Up clamps at the existing 100x limit");
    speedUp->trigger();
    check(close(travel(), baseline * 100.0f, 1.0e-4), "another keyboard speed step cannot exceed the maximum");
    for ( int step = 0; step < 120; ++step ) speedDown->trigger();
    check(close(travel(), baseline * 0.01f, 1.0e-6), "keyboard Fly Speed Down clamps at the existing 0.01x limit");
    speedDown->trigger();
    check(close(travel(), baseline * 0.01f, 1.0e-6), "another keyboard speed step cannot underflow the minimum");
    check(view->meshRebuildCountForTest() == rebuilds && window.undoDepthForTest() == undoDepth
            && window.undoSnapshotStatsForTest().full == fullSnapshots
            && window.documentStateForTest().level.sectors.data() == sectors
            && window.windowTitle() == title && window.recoveryEditorStateCaptureCountForTest() == 0u,
        "keyboard Fly speed/ticks preserve authored data/history and avoid mesh rebuilds or recovery captures while Fly owns input");
    sendMouse(*view, QEvent::MouseButtonRelease, point, Qt::MiddleButton, Qt::NoButton);
    check(!view->flyNavigationActive(), "the physical Middle-button release ends keyboard speed ownership");
    view->resetFlySpeedMultiplier();
    speedUp->trigger();
    sendMouse(*view, QEvent::MouseButtonPress, point, Qt::MiddleButton, Qt::MiddleButton);
    view->setFlyNavigationDirection(GlView::FlyDirection::Forward, true);
    check(close(travel(), baseline, 1.0e-5), "Fly Speed Up is inactive again after its physical owner is released");
    QLineEdit editor(&window);
    editor.setGeometry(30, 30, 180, 28);
    editor.show();
    editor.setFocus(Qt::OtherFocusReason);
    QApplication::processEvents();
    const QVector3D eye = view->cameraEye();
    sendNavigationKey(editor, Qt::Key_Plus, Qt::KeypadModifier, false, QStringLiteral("+"));
    check(!view->flyNavigationActive() && editor.text() == QStringLiteral("+")
            && view->cameraEye() == eye,
        "explicit native-editor focus cancels MMB Fly and retains keypad text instead of changing speed");
}

void testSavedNavigationAlternativesAndExplicitEmptyBindings()
{
    NavigationSettingsFixture settingsIsolation{};
    {
        QSettings settings = settingsStore();
        const QStringList alternatives{
            QKeySequence(Qt::Key_F13).toString(QKeySequence::PortableText),
            QKeySequence(Qt::Key_F14).toString(QKeySequence::PortableText)};
        settings.setValue("keymap/shortcuts/viewport.zoom_in", alternatives);
        settings.setValue("keymap/shortcuts/viewport.zoom_out", QStringList{});
        settings.setValue("keymap/shortcuts/viewport.fly.speed_up", alternatives);
        settings.setValue("keymap/shortcuts/viewport.fly.speed_down", QStringList{});
    }
    MainWindow window(nullptr, true);
    window.setAttribute(Qt::WA_DontShowOnScreen);
    window.show();
    GlView* const view = window.glViewForTest();
    if ( !view ) return;
    prepareView(*view);
    QAction* const zoomOut = window.findChild<QAction*>("viewport.zoom_out");
    QAction* const speedDown = window.findChild<QAction*>("viewport.fly.speed_down");
    check(zoomOut && speedDown && zoomOut->shortcuts().isEmpty() && speedDown->shortcuts().isEmpty(),
        "explicit empty Zoom and Fly-speed bindings survive startup without restoring their keypad defaults");
    if ( !zoomOut || !speedDown ) return;
    view->setCameraProjectionType(app::ViewportCamera::ProjectionType::Perspective);
    view->setOrbitViewImmediate(0.0f, 0.0f, 0.0f);
    view->setCameraOrbitDistance(8.0f);
    auto navigation = view->viewportNavigationSettings();
    navigation.mouse[static_cast<std::size_t>(ViewportMouseAction::flyLook)].button = Qt::MiddleButton;
    for ( const auto action : { ViewportMouseAction::orbit, ViewportMouseAction::pan,
            ViewportMouseAction::dolly, ViewportMouseAction::fastDolly } )
    {
        navigation.mouse[static_cast<std::size_t>(action)].button = Qt::RightButton;
    }
    navigation.zoom.enabled = false;
    navigation.flySpeed.enabled = false;
    view->setViewportNavigationSettings(navigation);
    QApplication::setActiveWindow(&window);
    view->setFocus(Qt::OtherFocusReason);
    QApplication::processEvents();
    sendNavigationKey(*view, Qt::Key_unknown);
    sendNavigationKey(*view, Qt::Key_Plus, Qt::KeypadModifier);
    sendNavigationKey(*view, Qt::Key_Minus, Qt::KeypadModifier);
    check(close(view->cameraOrbitDistance(), 8.0f, 1.0e-5),
        "replaced or explicitly empty Zoom bindings remove both former keypad runtime commands");
    sendNavigationKey(*view, Qt::Key_F13);
    check(close(view->cameraOrbitDistance(), 7.2f, 1.0e-5),
        "the first saved Zoom alternative executes the canonical camera command");
    sendNavigationKey(*view, Qt::Key_F14);
    check(close(view->cameraOrbitDistance(), 6.48f, 1.0e-5),
        "the second saved Zoom alternative also executes the same camera command exactly once");
    const float distance = view->cameraOrbitDistance();
    const QPointF point(210.0, 200.0);
    view->resetFlySpeedMultiplier();
    sendMouse(*view, QEvent::MouseButtonPress, point, Qt::MiddleButton, Qt::MiddleButton);
    view->setFlyNavigationDirection(GlView::FlyDirection::Forward, true);
    const auto travel = [&]()
    {
        const QVector3D before = view->cameraTarget();
        view->tickFlyNavigationForTest();
        return (view->cameraTarget() - before).length();
    };
    const float baseline = travel();
    sendNavigationKey(*view, Qt::Key_Plus, Qt::KeypadModifier);
    sendNavigationKey(*view, Qt::Key_Minus, Qt::KeypadModifier);
    check(baseline > 0.0f && close(travel(), baseline, 1.0e-5),
        "replaced and explicitly empty Fly speed bindings remove their former keypad effects");
    sendNavigationKey(*view, Qt::Key_F13);
    check(close(travel(), baseline * 1.1f, 1.0e-5),
        "the first saved Fly speed alternative executes only within its captured owner");
    sendNavigationKey(*view, Qt::Key_F14);
    check(close(travel(), baseline * 1.21f, 1.0e-5) && view->cameraOrbitDistance() == distance,
        "both Fly speed alternatives execute without also invoking their shared ordinary Zoom bindings");
    sendMouse(*view, QEvent::MouseButtonRelease, point, Qt::MiddleButton, Qt::NoButton);
    sendNavigationKey(*view, Qt::Key_F13);
    check(close(view->cameraOrbitDistance(), distance * 0.9f, 1.0e-5),
        "the same remapped key returns to ordinary Zoom after Middle-button Fly releases input");
}

void testMiddleButtonFlyCameraAndWheelOwnership()
{
    NavigationSettingsFixture settingsIsolation{};
    MainWindow window(nullptr, true);
    window.setAttribute(Qt::WA_DontShowOnScreen);
    window.show();
    GlView* const view = window.glViewForTest();
    if ( !view )
    {
        check(false, "Middle-button runtime regression has the actual Level viewport");
        return;
    }
    prepareView(*view);
    view->setCameraProjectionType(app::ViewportCamera::ProjectionType::Perspective);
    view->setOrbitViewImmediate(31.0f, 22.0f, 0.0f);
    view->setCameraOrbitDistance(8.0f);
    view->setOrbitAroundSelectionEnabled(false);
    view->setViewportPickingEnabled(false);
    auto mapping = view->viewportNavigationSettings();
    mapping.mouse[static_cast<std::size_t>(ViewportMouseAction::flyLook)].button = Qt::MiddleButton;
    for ( const auto action : { ViewportMouseAction::orbit, ViewportMouseAction::pan,
            ViewportMouseAction::dolly, ViewportMouseAction::fastDolly } )
    {
        mapping.mouse[static_cast<std::size_t>(action)].button = Qt::RightButton;
    }
    view->setViewportNavigationSettings(mapping);
    QApplication::setActiveWindow(&window);
    view->setFocus(Qt::OtherFocusReason);
    QApplication::processEvents();
    const QPointF point(210.0, 200.0);
    const auto press = [&](const Qt::KeyboardModifiers modifiers)
    {
        sendMouse(*view, QEvent::MouseButtonPress, point, Qt::MiddleButton, Qt::MiddleButton, modifiers);
        check(view->flyNavigationActive(), "Middle-button press captures Fly rather than a camera-drag or edit gesture");
    };
    const auto release = [&]()
    {
        sendMouse(*view, QEvent::MouseButtonRelease, point, Qt::MiddleButton, Qt::NoButton);
        check(!view->flyNavigationActive(), "releasing the captured physical Middle button ends Fly");
    };
    const auto key = [&](const QEvent::Type type, const int value, const Qt::KeyboardModifiers after)
    {
        const Qt::KeyboardModifier flag = value == Qt::Key_Control ? Qt::ControlModifier
            : value == Qt::Key_Shift ? Qt::ShiftModifier : Qt::AltModifier;
        QKeyEvent event(type, value, after ^ flag);
        check(event.modifiers() == after, "MMB speed-key fixture carries exact post-transition modifier state");
        QApplication::sendEvent(view, &event);
    };
    const auto travel = [&]()
    {
        const QVector3D before = view->cameraTarget();
        view->tickFlyNavigationForTest();
        return (view->cameraTarget() - before).length();
    };
    const auto resetModifierState = [&](const char* const phase)
    {
        const Qt::KeyboardModifiers held = QGuiApplication::keyboardModifiers();
        if ( held != Qt::NoModifier )
        {
            std::cerr << "MMB_FLY_MODIFIER_CLEANUP phase=" << phase
                << " held=" << int(held) << '\n';
        }
        for ( const auto [value, modifier] : std::array{
            std::pair{Qt::Key_Control, Qt::ControlModifier},
            std::pair{Qt::Key_Shift, Qt::ShiftModifier},
            std::pair{Qt::Key_Alt, Qt::AltModifier},
            std::pair{Qt::Key_Meta, Qt::MetaModifier} } )
        {
            QKeyEvent event(QEvent::KeyRelease, value, modifier);
            check(event.modifiers() == Qt::NoModifier,
                "MMB fixture modifier cleanup carries the released post-transition state");
            QApplication::sendEvent(view, &event);
        }
        // A modifier key event reports a toggled state. Use an ordinary key
        // too so QApplication's global key state is explicitly neutral, as
        // in the saved Fly-alternative fixture below.
        QKeyEvent neutralPress(QEvent::KeyPress, Qt::Key_unknown, Qt::NoModifier);
        QKeyEvent neutralRelease(QEvent::KeyRelease, Qt::Key_unknown, Qt::NoModifier);
        QApplication::sendEvent(view, &neutralPress);
        QApplication::sendEvent(view, &neutralRelease);
        check(QGuiApplication::keyboardModifiers() == Qt::NoModifier,
            "MMB fixture leaves application-wide modifier state neutral");
    };
    resetModifierState("entry");
    sendWheel(*view, Qt::NoModifier);
    check(!view->flyNavigationActive(), "turning the wheel alone never starts wheel-click Fly");
    press(Qt::NoModifier);
    const QVector3D eyeBeforeLook = view->cameraEye();
    const QVector3D forwardBeforeLook = view->cameraForward();
    sendMouse(*view, QEvent::MouseMove, point + QPointF(32.0, 16.0), Qt::NoButton, Qt::MiddleButton);
    check(closeQVec(view->cameraEye(), eyeBeforeLook)
            && !closeQVec(view->cameraForward(), forwardBeforeLook),
        "MMB Fly changes camera look with a fixed eye rather than orbiting the selection");
    sendMouse(*view, QEvent::MouseButtonRelease, point, Qt::RightButton, Qt::MiddleButton);
    check(view->flyNavigationActive(), "an unrelated RMB release cannot end captured Middle-button Fly");
    release();
    const QVector3D orbitEye = view->cameraEye();
    const QVector3D orbitTarget = view->cameraTarget();
    sendMouse(*view, QEvent::MouseButtonPress, point, Qt::RightButton, Qt::RightButton);
    sendMouse(*view, QEvent::MouseMove, point + QPointF(30.0, 15.0), Qt::NoButton, Qt::RightButton);
    sendMouse(*view, QEvent::MouseButtonRelease, point + QPointF(30.0, 15.0), Qt::RightButton, Qt::NoButton);
    check(!view->flyNavigationActive() && !closeQVec(view->cameraEye(), orbitEye)
            && closeQVec(view->cameraTarget(), orbitTarget),
        "plain RMB uses the remapped Orbit gesture with a fixed target and no Fly ownership");
    for ( const auto modifier : { Qt::ControlModifier, Qt::ShiftModifier } )
    {
        const int value = modifier == Qt::ControlModifier ? Qt::Key_Control : Qt::Key_Shift;
        key(QEvent::KeyPress, value, modifier);
        check(QGuiApplication::keyboardModifiers() == modifier,
            "the configured Fly speed modifier is actually held before pressing MMB");
        press(modifier);
        view->setFlyNavigationDirection(GlView::FlyDirection::Forward, true);
        const float heldDistance = travel();
        key(QEvent::KeyRelease, value, Qt::NoModifier);
        const float normalDistance = travel();
        check(normalDistance > 0.0f && close(heldDistance,
                normalDistance * (modifier == Qt::ControlModifier ? 0.5f : 2.0f), 1.0e-5),
            "Ctrl/Shift held before Middle-button Fly select keyboard half/double speed without starting Dolly/Pan");
        release();
    }
    for ( const auto options : { std::pair{true, false}, std::pair{true, true}, std::pair{false, true} } )
    {
        mapping.flySpeed.enabled = options.first;
        mapping.flySpeed.inverted = options.second;
        mapping.flySpeed.modifiers = Qt::AltModifier;
        view->setViewportNavigationSettings(mapping);
        press(Qt::NoModifier);
        view->setFlyNavigationDirection(GlView::FlyDirection::Forward, true);
        const float baseline = travel();
        const float distanceBeforeWheel = view->cameraOrbitDistance();
        sendWheel(*view, Qt::NoModifier);
        check(close(travel(), baseline, 1.0e-5) && view->cameraOrbitDistance() == distanceBeforeWheel,
            "MMB Fly consumes wheel scrolling without zoom when its speed modifier is absent or disabled");
        sendWheel(*view, Qt::AltModifier);
        const float factor = !options.first ? 1.0f : options.second ? 1.0f / 1.1f : 1.1f;
        check(baseline > 0.0f && close(travel(), baseline * factor, 1.0e-5)
                && view->cameraOrbitDistance() == distanceBeforeWheel,
            "MMB Fly wheel speed obeys modifier gating, independent inversion, and disabled-scroll consumption");
        release();
        key(QEvent::KeyRelease, Qt::Key_Alt, Qt::NoModifier);
    }
    GlView pickView{};
    prepareView(pickView);
    pickView.setViewportNavigationSettings(mapping);
    pickView.setPlacementGestureEnabled(true);
    int pickMoves = 0;
    QPoint pickedPosition{};
    QObject::connect(&pickView, &GlView::placementGestureDragged, &pickView,
        [&](const QPoint& position, Qt::KeyboardModifiers)
        {
            ++pickMoves;
            pickedPosition = position;
        });
    const QVector3D pickEye = pickView.cameraEye();
    sendMouse(pickView, QEvent::MouseButtonPress, point, Qt::LeftButton, Qt::LeftButton);
    sendMouse(pickView, QEvent::MouseMove, point + QPointF(12.0, 8.0), Qt::NoButton, Qt::LeftButton);
    sendMouse(pickView, QEvent::MouseButtonRelease, point + QPointF(12.0, 8.0), Qt::LeftButton, Qt::NoButton);
    check(mapping.mouse[static_cast<std::size_t>(ViewportMouseAction::pick)].button == Qt::LeftButton
            && pickMoves == 1 && pickedPosition == (point + QPointF(12.0, 8.0)).toPoint()
            && !pickView.flyNavigationActive() && pickView.cameraEye() == pickEye,
        "Middle-button Fly remapping keeps the original LMB Pick/Edit pointer dispatch and camera still");
    resetModifierState("exit");
}

void testRemappedMouseGesturesAndOwnership()
{
    NavigationSettingsFixture settingsIsolation{};
    {
        QSettings settings = settingsStore();
        settings.setValue("viewport/mouse/pick_button", int(Qt::RightButton));
        settings.setValue("viewport/mouse/fly_look_button", int(Qt::LeftButton));
        settings.setValue("viewport/mouse/pan_modifiers", int(Qt::AltModifier));
        settings.setValue("viewport/mouse/zoom_modifiers", int(Qt::AltModifier));
        settings.sync();
    }
    GlView view{};
    prepareView(view);
    view.setCameraProjectionType(app::ViewportCamera::ProjectionType::Perspective);
    view.setViewportPickingEnabled(false);
    const QPointF start(200.0, 200.0);
    sendMouse(view, QEvent::MouseButtonPress, start, Qt::LeftButton, Qt::LeftButton);
    check(view.flyNavigationActive(), "configured LMB starts Fly Look instead of edit picking");
    const QVector3D beforeLook = view.cameraForward();
    sendMouse(view, QEvent::MouseMove, start + QPointF(32.0, 16.0),
        Qt::NoButton, Qt::LeftButton);
    check(!closeQVec(view.cameraForward(), beforeLook),
        "configured Fly Look button owns mouse-look samples");
    sendMouse(view, QEvent::MouseButtonRelease, start, Qt::RightButton, Qt::LeftButton);
    check(view.flyNavigationActive(), "an unrelated button release cannot end the captured Fly gesture");
    sendMouse(view, QEvent::MouseButtonRelease, start, Qt::LeftButton, Qt::NoButton);
    check(!view.flyNavigationActive(), "the configured Fly button release ends its captured gesture");
    sendMouse(view, QEvent::MouseButtonPress, start, Qt::RightButton, Qt::RightButton);
    check(!view.flyNavigationActive(), "configured RMB edit picking no longer starts Fly");
    sendMouse(view, QEvent::MouseButtonRelease, start, Qt::RightButton, Qt::NoButton);

    const float zoomBefore = view.cameraOrbitDistance();
    sendWheel(view, Qt::NoModifier);
    check(view.cameraOrbitDistance() == zoomBefore,
        "wheel zoom leaves the camera unchanged when its configured Alt chord is absent");
    sendWheel(view, Qt::AltModifier);
    check(view.cameraOrbitDistance() < zoomBefore,
        "configured Alt wheel zoom reaches the camera");

    const QVector3D beforePan = view.cameraTarget();
    const QVector3D beforePanForward = view.cameraForward();
    sendMouse(view, QEvent::MouseButtonPress, start, Qt::MiddleButton,
        Qt::MiddleButton, Qt::AltModifier);
    sendMouse(view, QEvent::MouseMove, start + QPointF(30.0, 20.0),
        Qt::NoButton, Qt::MiddleButton, Qt::AltModifier);
    sendMouse(view, QEvent::MouseButtonRelease, start + QPointF(30.0, 20.0),
        Qt::MiddleButton, Qt::NoButton, Qt::AltModifier);
    check(!closeQVec(view.cameraTarget(), beforePan)
            && closeQVec(view.cameraForward(), beforePanForward),
        "configured Alt MMB pans instead of falling through to plain orbit");

    {
        QSettings settings = settingsStore();
        settings.setValue("viewport/mouse/pick_button", int(Qt::LeftButton));
        settings.setValue("viewport/mouse/fly_look_button", int(Qt::RightButton));
        settings.sync();
    }
    sendMouse(view, QEvent::MouseButtonPress, start, Qt::LeftButton, Qt::LeftButton);
    check(view.flyNavigationActive(),
        "mouse samples use the cached mapping until an explicit Preferences publication");
    QEvent lostGrab(QEvent::UngrabMouse);
    QApplication::sendEvent(&view, &lostGrab);
    check(!view.flyNavigationActive(), "losing the mouse grab cancels captured Fly and held movement");
    sendMouse(view, QEvent::MouseButtonPress, start, Qt::LeftButton, Qt::LeftButton);
    QFocusEvent focusOut(QEvent::FocusOut, Qt::OtherFocusReason);
    QApplication::sendEvent(&view, &focusOut);
    check(!view.flyNavigationActive(), "viewport focus loss cancels Fly before another editor can own keys");
}

void testMousePreferencesCancelAcceptConflictAndDefaults()
{
    NavigationSettingsFixture settingsIsolation{};
    MainWindow window(nullptr, true);
    GlView* const view = window.glViewForTest();
    check(view != nullptr, "mouse preference transactions have a live viewport");
    const auto readCombo = [](QDialog& dialog, const char* id)
    {
        return dialog.findChild<QComboBox*>(QString::fromLatin1(id));
    };
    const auto setRemap = [&](QDialog& dialog)
    {
        auto* const pick = readCombo(dialog, "pref_mouse_pick_button");
        auto* const fly = readCombo(dialog, "pref_mouse_fly_look_button");
        check(pick && fly, "mouse chooser offers Pick and Fly button remapping");
        if ( pick && fly )
        {
            pick->setCurrentIndex(pick->findData(int(Qt::RightButton)));
            fly->setCurrentIndex(fly->findData(int(Qt::LeftButton)));
        }
        return pick && fly;
    };
    QVariantMap persistedBefore{};
    {
        QSettings settings = settingsStore();
        for ( const QString& key : settings.allKeys() )
        {
            persistedBefore.insert(key, settings.value(key));
        }
    }
    drivePreferences(window, [&](QDialog& dialog)
    {
        (void)setRemap(dialog);
        dialog.reject();
    });
    QVariantMap persistedAfterCancel{};
    {
        QSettings settings = settingsStore();
        for ( const QString& key : settings.allKeys() )
        {
            persistedAfterCancel.insert(key, settings.value(key));
        }
    }
    check(persistedAfterCancel == persistedBefore,
        "Cancel after mouse remapping preserves every persisted settings value");
    const auto rebuildsBefore = view ? view->meshRebuildCountForTest() : 0u;
    bool changed = false;
    drivePreferences(window, [&](QDialog& dialog)
    {
        if ( !setRemap(dialog) )
        {
            dialog.reject();
            return;
        }
        auto* const box = dialog.findChild<QDialogButtonBox*>();
        auto* const ok = box ? box->button(QDialogButtonBox::Ok) : nullptr;
        check(ok && ok->isEnabled(), "a valid swapped Pick/Fly mapping can be accepted");
        if ( ok && ok->isEnabled() )
        {
            changed = true;
            ok->click();
        }
        else
        {
            dialog.reject();
        }
    });
    {
        QSettings settings = settingsStore();
        check(changed && settings.value("viewport/mouse/pick_button").toInt() == int(Qt::RightButton)
                && settings.value("viewport/mouse/fly_look_button").toInt() == int(Qt::LeftButton),
            "OK persists the complete validated mouse mapping");
    }
    if ( view )
    {
        prepareView(*view);
        sendMouse(*view, QEvent::MouseButtonPress, QPointF(120.0, 120.0),
            Qt::LeftButton, Qt::LeftButton);
        check(changed && view->flyNavigationActive()
                && view->meshRebuildCountForTest() == rebuildsBefore,
            "accepted mouse mapping applies immediately without rebuilding level meshes");
        sendMouse(*view, QEvent::MouseButtonRelease, QPointF(120.0, 120.0),
            Qt::LeftButton, Qt::NoButton);
    }
    drivePreferences(window, [&](QDialog& dialog)
    {
        auto* const pick = readCombo(dialog, "pref_mouse_pick_button");
        auto* const fly = readCombo(dialog, "pref_mouse_fly_look_button");
        auto* const error = dialog.findChild<QLabel*>("pref_mouse_binding_error");
        auto* const box = dialog.findChild<QDialogButtonBox*>();
        auto* const ok = box ? box->button(QDialogButtonBox::Ok) : nullptr;
        if ( pick && fly )
        {
            pick->setCurrentIndex(pick->findData(int(Qt::LeftButton)));
        }
        check(pick && fly && error && !error->text().isEmpty() && ok && !ok->isEnabled(),
            "duplicate Pick/Fly chords explain the conflict and prevent partial persistence");
        auto* const reset = dialog.findChild<QPushButton*>("pref_mouse_reset_defaults");
        check(reset != nullptr, "mouse navigation has an accessible Restore Defaults control");
        if ( reset )
        {
            reset->click();
        }
        check(reset && pick && fly && pick->currentData().toInt() == int(Qt::LeftButton)
                && fly->currentData().toInt() == int(Qt::RightButton)
                && error && error->text().isEmpty() && ok && ok->isEnabled(),
            "Restore Defaults repairs a conflict and restores the documented mouse buttons");
        dialog.reject();
    });
}

void testMalformedMousePreferencesUseDefaults()
{
    NavigationSettingsFixture settingsIsolation{};
    {
        QSettings settings = settingsStore();
        settings.setValue("viewport/mouse/pick_button", -9);
        settings.setValue("viewport/mouse/fly_look_button", 999);
        settings.setValue("viewport/mouse/pan_modifiers", QStringLiteral("sometimes"));
        settings.setValue("viewport/mouse/dolly_modifiers", int(Qt::KeypadModifier));
        settings.setValue("viewport/mouse/zoom_modifiers", QStringList{"Alt", "Ctrl"});
        settings.sync();
    }
    MainWindow window(nullptr, true);
    drivePreferences(window, [&](QDialog& dialog)
    {
        auto* const pick = dialog.findChild<QComboBox*>("pref_mouse_pick_button");
        auto* const fly = dialog.findChild<QComboBox*>("pref_mouse_fly_look_button");
        auto* const pan = dialog.findChild<QComboBox*>("pref_mouse_pan_modifiers");
        auto* const dolly = dialog.findChild<QComboBox*>("pref_mouse_dolly_modifiers");
        auto* const zoom = dialog.findChild<QComboBox*>("pref_mouse_zoom_modifiers");
        check(pick && fly && pan && dolly && zoom
                && pick->currentData().toInt() == int(Qt::LeftButton)
                && fly->currentData().toInt() == int(Qt::RightButton)
                && pan->currentData().toInt() == int(Qt::ShiftModifier)
                && dolly->currentData().toInt() == int(Qt::ControlModifier)
                && zoom->currentData().toInt() == 0,
            "malformed buttons, modifier types, and non-editor modifier bits use their safe current defaults");
        dialog.reject();
    });
    GlView* const view = window.glViewForTest();
    if ( view )
    {
        prepareView(*view);
        sendMouse(*view, QEvent::MouseButtonPress, QPointF(120.0, 120.0),
            Qt::RightButton, Qt::RightButton);
        check(view->flyNavigationActive(), "malformed navigation configuration retains usable default RMB Fly");
        sendMouse(*view, QEvent::MouseButtonRelease, QPointF(120.0, 120.0),
            Qt::RightButton, Qt::NoButton);
    }
}

void testFlySlowerOwnsControlAndMovement()
{
    NavigationSettingsFixture settingsIsolation{};
    MainWindow window(nullptr, true);
    window.setAttribute(Qt::WA_DontShowOnScreen);
    window.show();
    GlView* const view = window.glViewForTest();
    check(view != nullptr, "Fly Slower regression has a live viewport");
    if ( !view )
    {
        return;
    }
    QApplication::setActiveWindow(&window);
    view->setFocus(Qt::OtherFocusReason);
    QApplication::processEvents();
    prepareView(*view);
    const QPointF point(200.0, 200.0);
    const int modifiersAtEntry = int(QGuiApplication::keyboardModifiers());
    sendMouse(*view, QEvent::MouseButtonPress, point, Qt::RightButton, Qt::RightButton);
    check(sendFlyKey(window, *view, QEvent::KeyPress, Qt::Key_W, Qt::NoModifier),
        "plain W starts the held Fly movement controller");
    const QVector3D normalBefore = view->cameraTarget();
    view->tickFlyNavigationForTest();
    const float normalDistance = (view->cameraTarget() - normalBefore).length();
    const bool gridBefore = view->showGrid();
    const auto rebuildsBefore = view->meshRebuildCountForTest();
    QAction* const slowAction = window.findChild<QAction*>("viewport.fly.slow");
    check(slowAction && slowAction->shortcuts() == QList<QKeySequence>{QKeySequence(Qt::Key_Control)},
        "Fly Slower defaults to a rebindable Ctrl hold action");
    check(sendFlyKey(window, *view, QEvent::ShortcutOverride, Qt::Key_Control, Qt::ControlModifier)
            && sendFlyKey(window, *view, QEvent::KeyPress, Qt::Key_Control, Qt::ControlModifier),
        "Fly claims and consumes the Ctrl slow modifier before editor commands");
    const QVector3D slowBefore = view->cameraTarget();
    view->tickFlyNavigationForTest();
    const float slowDistance = (view->cameraTarget() - slowBefore).length();
    if ( normalDistance <= 0.0f || !close(slowDistance, normalDistance * 0.5f, 1.0e-5) )
    {
        std::cerr << "NAV_CTRL_SPEED_SAMPLE baseline=" << normalDistance
            << " actual=" << slowDistance << " entry_held=" << modifiersAtEntry
            << " held=" << int(QGuiApplication::keyboardModifiers()) << '\n';
    }
    check(normalDistance > 0.0f && close(slowDistance, normalDistance * 0.5f, 1.0e-5),
        "holding Ctrl halves Fly movement while retaining the existing movement key state");
    check(view->showGrid() == gridBefore && view->meshRebuildCountForTest() == rebuildsBefore,
        "Fly Ctrl neither changes grid visibility nor rebuilds authored geometry");
    check(sendFlyKey(window, *view, QEvent::KeyRelease, Qt::Key_W, Qt::ControlModifier)
            && sendFlyKey(window, *view, QEvent::KeyPress, Qt::Key_W, Qt::ControlModifier),
        "Ctrl held Fly W release and press retain movement ownership");
    const QVector3D restartedSlowBefore = view->cameraTarget();
    view->tickFlyNavigationForTest();
    check(close((view->cameraTarget() - restartedSlowBefore).length(), normalDistance * 0.5f, 1.0e-5),
        "Ctrl held before a new Fly W press still resolves the movement binding");
    sendMouse(*view, QEvent::MouseButtonRelease, point, Qt::RightButton, Qt::NoButton);
}

void testSavedFlySpeedModifierAlternatives()
{
    for (const bool faster : {true, false})
    {
        NavigationSettingsFixture settingsIsolation{};
        const Qt::KeyboardModifier primaryModifier = faster
            ? Qt::ShiftModifier : Qt::ControlModifier;
        const int primaryKey = faster ? Qt::Key_Shift : Qt::Key_Control;
        const Qt::KeyboardModifier chordModifier = faster
            ? Qt::ControlModifier : Qt::ShiftModifier;
        const int chordKey = faster ? Qt::Key_Control : Qt::Key_Shift;
        const QList<QKeySequence> alternatives{
            QKeySequence(primaryKey), QKeySequence(Qt::Key_Alt),
            QKeySequence(int(chordModifier) | Qt::Key_Meta),
            QKeySequence(Qt::Key_F),
        };
        QStringList savedAlternatives{};
        for (const QKeySequence& sequence : alternatives)
        {
            savedAlternatives.push_back(sequence.toString(QKeySequence::PortableText));
        }
        {
            QSettings settings = settingsStore();
            settings.setValue(faster ? "keymap/shortcuts/viewport.fly.fast"
                : "keymap/shortcuts/viewport.fly.slow", savedAlternatives);
            settings.setValue(faster ? "keymap/shortcuts/viewport.fly.slow"
                : "keymap/shortcuts/viewport.fly.fast", QStringList{});
            settings.sync();
        }
        MainWindow window(nullptr, true);
        window.setAttribute(Qt::WA_DontShowOnScreen);
        window.show();
        GlView* const view = window.glViewForTest();
        check(view != nullptr, "saved Fly speed alternatives have a live viewport");
        if (!view) continue;
        prepareView(*view);
        QApplication::setActiveWindow(&window);
        view->setFocus(Qt::OtherFocusReason);
        QApplication::processEvents();
        QAction* const action = window.findChild<QAction*>(faster
            ? "viewport.fly.fast" : "viewport.fly.slow");
        check(action && action->shortcuts() == alternatives,
            "Fly speed alternatives load intact from the saved keymap");
        const auto beforeKeyTransition = [](const int value,
            const Qt::KeyboardModifiers after)
        {
            // QKeyEvent takes the state before the transition; modifiers()
            // toggles the changing modifier bit to report the state after it.
            switch (value)
            {
                case Qt::Key_Shift: return after ^ Qt::ShiftModifier;
                case Qt::Key_Control: return after ^ Qt::ControlModifier;
                case Qt::Key_Alt: return after ^ Qt::AltModifier;
                case Qt::Key_Meta: return after ^ Qt::MetaModifier;
                default: return after;
            }
        };
        for (const int releasedKey : {Qt::Key_Shift, Qt::Key_Control,
            Qt::Key_Alt, Qt::Key_Meta, Qt::Key_W})
        {
            QKeyEvent release(QEvent::KeyRelease, releasedKey,
                beforeKeyTransition(releasedKey, Qt::NoModifier));
            check(release.modifiers() == Qt::NoModifier,
                "fixture reset releases carry the exact post-event modifier state");
            QApplication::sendEvent(view, &release);
        }
        QKeyEvent neutralPress(QEvent::KeyPress, Qt::Key_unknown, Qt::NoModifier);
        QKeyEvent neutralRelease(QEvent::KeyRelease, Qt::Key_unknown, Qt::NoModifier);
        QApplication::sendEvent(view, &neutralPress);
        QApplication::sendEvent(view, &neutralRelease);
        check(QGuiApplication::keyboardModifiers() == Qt::NoModifier,
            "the normal-speed fixture releases all prior modifier and movement keys before Fly entry");
        const auto key = [&](const QEvent::Type type, const int value,
            const Qt::KeyboardModifiers modifiers)
        {
            QKeyEvent event(type, value, beforeKeyTransition(value, modifiers));
            check(event.modifiers() == modifiers,
                "real Fly key fixtures carry the exact requested post-event modifier state");
            event.setAccepted(false);
            QApplication::sendEvent(view, &event);
            check(event.isAccepted() && view->flyNavigationActive(),
                "real Fly speed key events remain owned by the viewport");
        };
        const auto travel = [&]()
        {
            const QVector3D before = view->cameraTarget();
            view->tickFlyNavigationForTest();
            return (view->cameraTarget() - before).length();
        };
        const QPointF point(210.0, 200.0);
        sendMouse(*view, QEvent::MouseButtonPress, point, Qt::RightButton, Qt::RightButton);
        key(QEvent::KeyPress, Qt::Key_W, Qt::NoModifier);
        const float normalDistance = travel();
        const float speedFactor = faster ? 2.0f : 0.5f;
        const auto expectSpeed = [&](const float factor, const char* const message)
        {
            const float distance = travel();
            const bool matches = normalDistance > 0.0f
                && close(distance, normalDistance * factor, 1.0e-5);
            if (!matches)
            {
                std::cerr << "NAV_SPEED_SAMPLE faster=" << faster
                    << " baseline=" << normalDistance << " actual=" << distance
                    << " factor=" << factor
                    << " held=" << int(QGuiApplication::keyboardModifiers())
                    << " assertion=" << message << '\n';
            }
            check(matches, message);
        };
        const bool gridBefore = view->showGrid();
        const auto rebuildsBefore = view->meshRebuildCountForTest();

        key(QEvent::KeyPress, Qt::Key_Alt, Qt::AltModifier);
        expectSpeed(speedFactor, faster
            ? "saved alternative Alt activates Fly Faster at twice normal speed"
            : "saved alternative Alt activates Fly Slower at half normal speed");
        key(QEvent::KeyRelease, Qt::Key_Alt, Qt::NoModifier);
        expectSpeed(1.0f, "releasing the alternative speed modifier restores normal movement");

        key(QEvent::KeyPress, primaryKey, primaryModifier);
        key(QEvent::KeyPress, Qt::Key_Alt, primaryModifier | Qt::AltModifier);
        expectSpeed(speedFactor, "simultaneously held alternatives apply one speed factor");
        key(QEvent::KeyRelease, primaryKey, Qt::AltModifier);
        expectSpeed(speedFactor, "releasing the primary speed modifier retains its held alternative");
        key(QEvent::KeyPress, primaryKey, primaryModifier | Qt::AltModifier);
        key(QEvent::KeyRelease, Qt::Key_Alt, primaryModifier);
        expectSpeed(speedFactor, "releasing the alternative speed modifier retains the held primary");
        key(QEvent::KeyRelease, primaryKey, Qt::NoModifier);
        expectSpeed(1.0f, "releasing all speed alternatives preserves the held movement key");

        key(QEvent::KeyPress, chordKey, chordModifier);
        expectSpeed(1.0f, "an incomplete alternative modifier chord does not change Fly speed");
        key(QEvent::KeyPress, Qt::Key_Meta, chordModifier | Qt::MetaModifier);
        expectSpeed(speedFactor, "completing the saved modifier chord activates the speed action");
        key(QEvent::KeyPress, primaryKey,
            chordModifier | Qt::MetaModifier | primaryModifier);
        key(QEvent::KeyRelease, chordKey, Qt::MetaModifier | primaryModifier);
        expectSpeed(speedFactor, "breaking a speed chord preserves its simultaneously held fallback");
        key(QEvent::KeyRelease, primaryKey, Qt::MetaModifier);
        expectSpeed(1.0f, "breaking all complete alternatives restores normal Fly movement");
        key(QEvent::KeyRelease, Qt::Key_Meta, Qt::NoModifier);

        key(QEvent::KeyPress, Qt::Key_F, Qt::NoModifier);
        expectSpeed(speedFactor, "an ordinary key alternative activates the saved speed action");
        key(QEvent::KeyPress, Qt::Key_Alt, Qt::AltModifier);
        key(QEvent::KeyRelease, Qt::Key_Alt, Qt::NoModifier);
        expectSpeed(speedFactor, "releasing a modifier alternative preserves a held ordinary speed key");
        key(QEvent::KeyRelease, Qt::Key_F, Qt::NoModifier);
        expectSpeed(1.0f, "releasing the ordinary speed key restores normal movement");
        key(QEvent::KeyPress, Qt::Key_Alt, Qt::AltModifier);
        key(QEvent::KeyPress, Qt::Key_F, Qt::AltModifier);
        key(QEvent::KeyRelease, Qt::Key_F, Qt::AltModifier);
        expectSpeed(speedFactor, "releasing the ordinary speed key preserves its held modifier alternative");
        key(QEvent::KeyRelease, Qt::Key_Alt, Qt::NoModifier);
        expectSpeed(1.0f, "releasing both ordinary and modifier alternatives restores normal movement");
        check(view->showGrid() == gridBefore && view->meshRebuildCountForTest() == rebuildsBefore,
            "speed modifier alternatives do not reach Grid or authored geometry actions");
        key(QEvent::KeyRelease, Qt::Key_W, Qt::NoModifier);
        sendMouse(*view, QEvent::MouseButtonRelease, point, Qt::RightButton, Qt::NoButton);

        sendMouse(*view, QEvent::MouseButtonPress, point, Qt::RightButton,
            Qt::RightButton, Qt::AltModifier);
        key(QEvent::KeyPress, Qt::Key_W, Qt::AltModifier);
        expectSpeed(speedFactor, "an alternative already held when entering Fly immediately changes speed");
        key(QEvent::KeyRelease, Qt::Key_Alt, Qt::NoModifier);
        expectSpeed(1.0f, "releasing an entry-held alternative retains the newly held movement key");
        key(QEvent::KeyRelease, Qt::Key_W, Qt::NoModifier);
        sendMouse(*view, QEvent::MouseButtonRelease, point, Qt::RightButton, Qt::NoButton);
    }
}

void testEditedKeymapReachesLiveActions()
{
    NavigationSettingsFixture settingsIsolation{};
    MainWindow window(nullptr, true);
    window.setAttribute(Qt::WA_DontShowOnScreen);
    window.show();
    GlView* const view = window.glViewForTest();
    if (!view) return;
    prepareView(*view);
    const QList<QKeySequence> alignBindings{
        QKeySequence(Qt::Key_F10), QKeySequence(Qt::Key_F11),
    };
    drivePreferences(window, [&](QDialog& dialog)
    {
        auto* const tree = dialog.findChild<QTreeWidget*>("pref_keymap_tree");
        check(tree != nullptr, "live keymap regression reaches shortcut controls");
        if (!tree)
        {
            dialog.reject();
            return;
        }
        const auto gridRows = bindingItems(*tree, "viewport.show_grid");
        const auto alignRows = bindingItems(*tree, "viewport.grid_align_to_view");
        const auto slowRows = bindingItems(*tree, "viewport.fly.slow");
        if (gridRows.size() == 1)
        {
            auto* const edit = shortcutEditor(tree->itemWidget(gridRows.front(), 1));
            if (edit) edit->setKeySequence(QKeySequence(Qt::Key_F9));
        }
        if (alignRows.size() == 1)
        {
            QWidget* const controls = tree->itemWidget(alignRows.front(), 1);
            auto* const selector = controls
                ? controls->findChild<QComboBox*>("pref_keymap_alternative") : nullptr;
            check(selector && selector->count() == 2,
                "one alignment action retains two alternatives in its compact selector");
            for (qsizetype index = 0; index < alignBindings.size(); ++index)
            {
                setBindingAlternative(*tree, "viewport.grid_align_to_view",
                    static_cast<int>(index), alignBindings[index]);
            }
        }
        if (slowRows.size() == 1)
        {
            auto* const edit = shortcutEditor(tree->itemWidget(slowRows.front(), 1));
            if (edit)
            {
                QKeyEvent press(QEvent::KeyPress, Qt::Key_Alt, Qt::AltModifier);
                QKeyEvent release(QEvent::KeyRelease, Qt::Key_Alt, Qt::NoModifier);
                QApplication::sendEvent(edit, &press);
                QApplication::sendEvent(edit, &release);
                check(edit->keySequence() == QKeySequence(Qt::Key_Alt),
                    "modifier-only Fly Slower can be captured with real key events");
            }
        }
        dialog.accept();
    });
    QAction* const grid = window.findChild<QAction*>("viewport.show_grid");
    QAction* const align = window.findChild<QAction*>("viewport.grid_align_to_view");
    QAction* const slow = window.findChild<QAction*>("viewport.fly.slow");
    check(grid && grid->shortcuts() == QList<QKeySequence>{QKeySequence(Qt::Key_F9)}
            && align && align->shortcuts() == alignBindings
            && slow && slow->shortcuts() == QList<QKeySequence>{QKeySequence(Qt::Key_Alt)},
        "accepted keymap edits replace each live QAction binding");
    check(grid && grid->toolTip().contains("F9")
            && align && align->toolTip().contains("F10") && align->toolTip().contains("F11"),
        "Grid and alignment tooltips publish the current alternatives");
    if (!grid || !align) return;
    int gridTriggers = 0;
    int alignTriggers = 0;
    QObject::connect(grid, &QAction::triggered, &window, [&]() { ++gridTriggers; });
    QObject::connect(align, &QAction::triggered, &window, [&]() { ++alignTriggers; });
    QApplication::setActiveWindow(&window);
    view->setFocus(Qt::OtherFocusReason);
    QApplication::processEvents();
    const auto sendKey = [&](const int key, const Qt::KeyboardModifiers modifiers = Qt::NoModifier)
    {
        QKeyEvent press(QEvent::KeyPress, key, modifiers);
        QKeyEvent release(QEvent::KeyRelease, key, modifiers);
        QApplication::sendEvent(view, &press);
        QApplication::sendEvent(view, &release);
    };
    const bool oldGrid = view->showGrid();
    sendKey(Qt::Key_F9);
    check(gridTriggers == 1 && view->showGrid() != oldGrid,
        "edited Grid shortcut routes to the real viewport action exactly once");
    sendKey(Qt::Key_G);
    check(gridTriggers == 1, "reassigned Grid removes its old G runtime binding");
    sendKey(Qt::Key_F10);
    sendKey(Qt::Key_F11);
    check(alignTriggers == 2, "both edited alignment alternatives invoke one live action");
    sendKey(Qt::Key_F3);
    sendKey(Qt::Key_0, Qt::KeypadModifier);
    check(alignTriggers == 2, "edited alignment alternatives remove both former shortcuts");

    sendMouse(*view, QEvent::MouseButtonPress, QPointF(200.0, 200.0),
        Qt::RightButton, Qt::RightButton, Qt::AltModifier);
    sendFlyKey(window, *view, QEvent::KeyPress, Qt::Key_W, Qt::AltModifier);
    const QVector3D slowBefore = view->cameraTarget();
    view->tickFlyNavigationForTest();
    const float slowDistance = (view->cameraTarget() - slowBefore).length();
    sendFlyKey(window, *view, QEvent::KeyRelease, Qt::Key_Alt, Qt::NoModifier);
    const QVector3D normalBefore = view->cameraTarget();
    view->tickFlyNavigationForTest();
    check(slowDistance > 0.0f
            && close((view->cameraTarget() - normalBefore).length(), slowDistance * 2.0f, 1.0e-5),
        "rebound Alt slow hold works when already held on entry and releases without dropping W");
    sendMouse(*view, QEvent::MouseButtonRelease, QPointF(200.0, 200.0),
        Qt::RightButton, Qt::NoButton);
}

void testDefaultModifierRoutingAndCaptureCancellation()
{
    NavigationSettingsFixture settingsIsolation{};
    const ViewportNavigationSettings defaults{};
    check(viewportMouseActionForPress(defaults, Qt::LeftButton,
            Qt::ShiftModifier | Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier)
                == ViewportMouseAction::pick
            && viewportMouseActionForPress(defaults, Qt::RightButton,
                Qt::ShiftModifier | Qt::ControlModifier | Qt::AltModifier)
                == ViewportMouseAction::flyLook
            && viewportMouseActionForPress(defaults, Qt::MiddleButton,
                Qt::ShiftModifier | Qt::ControlModifier | Qt::AltModifier)
                == ViewportMouseAction::fastDolly
            && viewportMouseActionForPress(defaults, Qt::MiddleButton,
                Qt::AltModifier | Qt::MetaModifier) == ViewportMouseAction::orbit,
        "default routing retains broad selection/fly modifiers and ignores Alt/Meta in camera drags");
    MainWindow window(nullptr, true);
    GlView* const view = window.glViewForTest();
    if (!view) return;
    prepareView(*view);
    view->setCameraProjectionType(app::ViewportCamera::ProjectionType::Orthographic);
    view->setOrbitViewImmediate(0.0f, 0.0f);
    const QPoint start(200, 200);
    const QPoint end(270, 250);
    check(window.beginSectorDrawForTest(2, false, false, start),
        "capture cancellation starts a real cube-placement preview");
    sendMouse(*view, QEvent::MouseButtonPress, start, Qt::LeftButton, Qt::LeftButton);
    sendMouse(*view, QEvent::MouseMove, end, Qt::NoButton, Qt::LeftButton);
    const auto sectorsBefore = window.documentStateForTest().level.sectors.size();
    QEvent lostGrab(QEvent::UngrabMouse);
    QApplication::sendEvent(view, &lostGrab);
    check(view->viewportMouseGestureButton() == Qt::NoButton
            && !window.commitSectorDrawPlacementForTest(end)
            && window.documentStateForTest().level.sectors.size() == sectorsBefore,
        "lost capture discards cube placement without committing geometry");
    sendMouse(*view, QEvent::MouseButtonRelease, end, Qt::LeftButton, Qt::NoButton);
    check(window.documentStateForTest().level.sectors.size() == sectorsBefore,
        "a stale placement release after capture loss cannot commit an edit");

    view->setPlacementGestureEnabled(false);
    sendMouse(*view, QEvent::MouseButtonPress, start, Qt::LeftButton, Qt::LeftButton);
    view->previewThingLightTransformForTest(render::LightGizmoKind::ThingRegular, QVector3D(1, 2, 3));
    bool previewCanceled = false;
    QObject::connect(view, &GlView::transformPreviewChanged, &window,
        [&](const bool active, GlView::TransformTool, const QVector3D&, const QVector3D&,
            double, const QVector3D&, double)
        {
            if (!active) previewCanceled = true;
        });
    ViewportNavigationSettings swapped = defaults;
    swapped.mouse[0].button = Qt::RightButton;
    swapped.mouse[1].button = Qt::LeftButton;
    view->setViewportNavigationSettings(swapped);
    check(previewCanceled && view->viewportMouseGestureButton() == Qt::NoButton,
        "applying a mouse remap cancels the captured property-transform preview");
}

void testFlyBlocksOverlappingShortcutsAndEditorsRetainFocus()
{
    NavigationSettingsFixture settingsIsolation{};
    MainWindow window(nullptr, true);
    window.setAttribute(Qt::WA_DontShowOnScreen);
    window.show();
    GlView* const view = window.glViewForTest();
    if (!view) return;
    prepareView(*view);
    QApplication::setActiveWindow(&window);
    view->setFocus(Qt::OtherFocusReason);
    QApplication::processEvents();
    QAction overlap(&window);
    overlap.setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Z));
    overlap.setShortcutContext(Qt::ApplicationShortcut);
    view->addAction(&overlap);
    int triggers = 0;
    QObject::connect(&overlap, &QAction::triggered, &window, [&]() { ++triggers; });
    const QPointF point(200.0, 200.0);
    sendMouse(*view, QEvent::MouseButtonPress, point, Qt::RightButton, Qt::RightButton,
        Qt::ControlModifier | Qt::ShiftModifier);
    for (const bool repeated : { false, true })
    {
        QKeyEvent override(QEvent::ShortcutOverride, Qt::Key_Z, Qt::ControlModifier,
            QString{}, repeated);
        QKeyEvent press(QEvent::KeyPress, Qt::Key_Z, Qt::ControlModifier, QString{}, repeated);
        QApplication::sendEvent(view, &override);
        QApplication::sendEvent(view, &press);
        check(override.isAccepted() && press.isAccepted(),
            "Fly claims an unrelated Ctrl editor shortcut including auto-repeat");
    }
    check(triggers == 0 && view->flyNavigationActive(),
        "Ctrl slow hold blocks an overlapping real QAction without changing Fly ownership");
    QLineEdit editor(&window);
    editor.setFocus(Qt::OtherFocusReason);
    QKeyEvent textPress(QEvent::KeyPress, Qt::Key_A, Qt::NoModifier, QStringLiteral("a"));
    QApplication::sendEvent(&editor, &textPress);
    check(!view->flyNavigationActive() && editor.text() == QStringLiteral("a"),
        "focused text editing ends Fly and retains its input");
}

void testRemappedLightDoubleClicksFollowPickOwnership()
{
    NavigationSettingsFixture settingsIsolation{};
    MainWindow window(nullptr, true);
    window.setAttribute(Qt::WA_DontShowOnScreen);
    window.show();
    editor::DocumentState state{};
    state.level.kind = ProjectType::MOTS;
    state.level.layers.emplace_back("Visible");
    SedLight baked{};
    baked.layer = 0;
    baked.intensity = 1.0;
    state.level.lights.push_back(baked);
    Thing actor{};
    actor.layer = 0;
    actor.name = "navigation-light";
    TplValue lightValue{};
    lightValue.name = "light";
    lightValue.type = ValueType::str;
    lightValue.stringValue = "1";
    actor.values.push_back(lightValue);
    state.level.things.push_back(actor);
    window.setDocumentStateForTest(std::move(state));
    GlView* const view = window.glViewForTest();
    if (!view) return;
    prepareView(*view);
    ViewportNavigationSettings swapped{};
    swapped.mouse[0].button = Qt::RightButton;
    swapped.mouse[1].button = Qt::LeftButton;
    view->setViewportNavigationSettings(swapped);
    const QPointF point(200.0, 200.0);
    const auto closeLightPopups = [&]()
    {
        for (QWidget* const popup : window.findChildren<QWidget*>("lightParametersPopup"))
        {
            popup->close();
        }
        QApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    };
    for (const bool thingLight : { false, true })
    {
        if (thingLight) window.activateThingLightEditModeForTest();
        else window.activateBakedLightEditModeForTest();
        const auto kind = thingLight ? render::LightGizmoKind::ThingRegular
            : render::LightGizmoKind::Baked;
        const auto publishFirstClick = [&]()
        {
            // Publish the same resolved owner/kind used by an ordinary first
            // click, then exercise the real global double-click dispatcher.
            view->clickPicked(-1, -1, thingLight ? 0 : -1, thingLight ? -1 : 0);
            view->setProperty("sed.selectedLightGizmoKind", int(kind));
            QApplication::setActiveWindow(&window);
            view->setFocus(Qt::OtherFocusReason);
        };
        publishFirstClick();
        sendMouse(*view, QEvent::MouseButtonDblClick, point, Qt::LeftButton, Qt::LeftButton);
        QApplication::processEvents();
        check(window.findChildren<QWidget*>("lightParametersPopup").isEmpty()
                && view->flyNavigationActive(),
            thingLight ? "remapped LMB Fly double-click cannot open the remembered Thing-light editor"
                : "remapped LMB Fly double-click cannot open the remembered Baked-light editor");
        closeLightPopups();
        sendMouse(*view, QEvent::MouseButtonRelease, point, Qt::LeftButton, Qt::NoButton);

        publishFirstClick();
        sendMouse(*view, QEvent::MouseButtonPress, point, Qt::LeftButton, Qt::LeftButton);
        sendMouse(*view, QEvent::MouseButtonDblClick, point, Qt::RightButton,
            Qt::LeftButton | Qt::RightButton);
        QApplication::processEvents();
        check(window.findChildren<QWidget*>("lightParametersPopup").isEmpty()
                && view->flyNavigationActive(),
            "a Pick double-click cannot replace an already captured Fly gesture");
        closeLightPopups();
        sendMouse(*view, QEvent::MouseButtonRelease, point, Qt::RightButton, Qt::LeftButton);
        sendMouse(*view, QEvent::MouseButtonRelease, point, Qt::LeftButton, Qt::NoButton);

        publishFirstClick();
        sendMouse(*view, QEvent::MouseButtonDblClick, point, Qt::RightButton, Qt::RightButton);
        QApplication::processEvents();
        QWidget* const popup = window.findChild<QWidget*>("lightParametersPopup");
        check(popup && popup->windowTitle() == (thingLight
                ? QStringLiteral("Thing light") : QStringLiteral("Baked light")),
            thingLight ? "remapped RMB Pick double-click opens the real Thing-light editor"
                : "remapped RMB Pick double-click opens the real Baked-light editor");
        closeLightPopups();
        sendMouse(*view, QEvent::MouseButtonRelease, point, Qt::RightButton, Qt::NoButton);
    }
}

bool sameNavigationCamera(const GlView& left, const GlView& right)
{
    return closeQVec(left.cameraEye(), right.cameraEye(), 1.0e-5f)
        && closeQVec(left.cameraTarget(), right.cameraTarget(), 1.0e-5f)
        && closeQVec(left.cameraForward(), right.cameraForward(), 1.0e-5f)
        && closeQVec(left.cameraRight(), right.cameraRight(), 1.0e-5f)
        && closeQVec(left.cameraUp(), right.cameraUp(), 1.0e-5f)
        && close(left.cameraOrbitDistance(), right.cameraOrbitDistance(), 1.0e-6)
        && close(left.cameraOrthoScale(), right.cameraOrthoScale(), 1.0e-6);
}

void testMouseCameraAxisInversion()
{
    NavigationSettingsFixture settingsIsolation{};
    const auto persistAxes = [](const bool horizontal, const bool vertical)
    {
        QSettings settings = settingsStore();
        settings.setValue("viewport/mouse/invert_horizontal", horizontal);
        settings.setValue("viewport/mouse/invert_vertical", vertical);
        settings.sync();
    };
    const auto configureCamera = [](GlView& view, const bool perspective)
    {
        prepareView(view);
        view.setCameraProjectionType(perspective
            ? app::ViewportCamera::ProjectionType::Perspective
            : app::ViewportCamera::ProjectionType::Orthographic);
        view.setOrbitViewImmediate(31.0f, 22.0f, 13.0f);
        view.setCameraOrbitDistance(8.0f);
        view.setCameraOrthoScale(96.0f);
        view.setOrbitAroundSelectionEnabled(false);
    };
    const QPointF start(240.0, 220.0);
    const QPointF delta(32.0, 16.0);
    const auto drag = [&](GlView& view, const ViewportMouseAction action,
        const QPointF& motion, const bool remapped)
    {
        if ( remapped )
        {
            auto navigation = view.viewportNavigationSettings();
            navigation.mouse[static_cast<std::size_t>(ViewportMouseAction::flyLook)].button
                = Qt::MiddleButton;
            navigation.mouse[static_cast<std::size_t>(ViewportMouseAction::orbit)].button
                = Qt::RightButton;
            view.setViewportNavigationSettings(navigation);
        }
        const auto binding = view.viewportNavigationSettings().mouse[
            static_cast<std::size_t>(action)];
        sendMouse(view, QEvent::MouseButtonPress, start,
            binding.button, binding.button, binding.modifiers);
        sendMouse(view, QEvent::MouseMove, start + motion,
            Qt::NoButton, binding.button, binding.modifiers);
        sendMouse(view, QEvent::MouseButtonRelease, start + motion,
            binding.button, Qt::NoButton, binding.modifiers);
    };

    for ( const bool perspective : {false, true} )
    {
        for ( const auto axes : {std::pair{true, false}, std::pair{false, true},
                std::pair{true, true}} )
        {
            for ( const auto action : {ViewportMouseAction::orbit, ViewportMouseAction::pan,
                    ViewportMouseAction::dolly, ViewportMouseAction::fastDolly,
                    ViewportMouseAction::flyLook} )
            {
                persistAxes(false, false);
                GlView expected{};
                configureCamera(expected, perspective);
                persistAxes(axes.first, axes.second);
                GlView actual{};
                configureCamera(actual, perspective);
                const auto rebuilds = actual.meshRebuildCountForTest();
                // Changing the disk preferences after construction cannot
                // replace the published cache during a pointer sample.
                persistAxes(false, false);
                const QPointF reversed(axes.first ? -delta.x() : delta.x(),
                    axes.second ? -delta.y() : delta.y());
                const bool remapped = action == ViewportMouseAction::orbit;
                drag(expected, action, reversed, remapped);
                drag(actual, action, delta, remapped);
                check(sameNavigationCamera(actual, expected),
                    "cached horizontal and vertical inversion match reversed Orbit/Pan/Dolly/Fast Dolly/Fly mouse motion");
                check(actual.meshRebuildCountForTest() == rebuilds,
                    "camera axis inversion does not rebuild authored geometry during pointer navigation");
            }

            persistAxes(false, false);
            GlView expectedWheel{};
            configureCamera(expectedWheel, perspective);
            persistAxes(axes.first, axes.second);
            GlView actualWheel{};
            configureCamera(actualWheel, perspective);
            sendWheel(expectedWheel, Qt::NoModifier);
            sendWheel(actualWheel, Qt::NoModifier);
            check(sameNavigationCamera(actualWheel, expectedWheel),
                "mouse axis inversion leaves independent wheel zoom direction unchanged");

            actualWheel.setPlacementGestureEnabled(true);
            QPoint capturedPosition{};
            int capturedMoves{};
            QObject::connect(&actualWheel, &GlView::placementGestureDragged, &actualWheel,
                [&](const QPoint& position, const Qt::KeyboardModifiers)
                {
                    capturedPosition = position;
                    ++capturedMoves;
                });
            const QVector3D cameraBeforeEdit = actualWheel.cameraEye();
            sendMouse(actualWheel, QEvent::MouseButtonPress, start,
                Qt::LeftButton, Qt::LeftButton);
            sendMouse(actualWheel, QEvent::MouseMove, start + delta,
                Qt::NoButton, Qt::LeftButton);
            sendMouse(actualWheel, QEvent::MouseButtonRelease, start + delta,
                Qt::LeftButton, Qt::NoButton);
            check(capturedMoves == 1 && capturedPosition == (start + delta).toPoint()
                    && actualWheel.cameraEye() == cameraBeforeEdit,
                "mouse axis inversion preserves absolute placement/edit pointer positions and leaves camera still");
        }

        Level selectedLevel{};
        level::levelCreateCube(selectedLevel, -2.0, 0.0, -2.0,
            TVector{0.0, -1.0, 0.0}, TVector{4.0, 0.0, 0.0});
        editor::Selection selection{};
        selection.sectors.insert(0);
        persistAxes(false, false);
        GlView expectedSelected{};
        expectedSelected.setLevel(&selectedLevel);
        configureCamera(expectedSelected, perspective);
        persistAxes(true, true);
        GlView actualSelected{};
        actualSelected.setLevel(&selectedLevel);
        configureCamera(actualSelected, perspective);
        for ( GlView* const view : {&expectedSelected, &actualSelected} )
        {
            view->setSelectionState(0, -1, -1, -1, selection);
            view->setOrbitAroundSelectionEnabled(true);
            view->orbitPanPixels(QPoint(25, -15));
        }
        check(sameNavigationCamera(actualSelected, expectedSelected),
            "mouse inversion leaves direct keyboard-style camera pan commands unchanged");
        drag(expectedSelected, ViewportMouseAction::orbit, -delta, false);
        drag(actualSelected, ViewportMouseAction::orbit, delta, false);
        check(sameNavigationCamera(actualSelected, expectedSelected),
            "both inverted mouse axes also reverse the banked selection-pivot Orbit path");
    }
}

} // namespace

void runNavigationPreferencesTests()
{
    testViewportBannerPreferencesExposeIndependentControls();
    testViewportBannerDraftCancelAcceptRestoreAndNoOp();
    testPreferencesExposeCompleteNavigation();
    testCompactShortcutAlternativesAndPersistence();
    testShortcutEditorDisplaysSpaceSeparatedModifiers();
    testShortcutMenuDisplayUsesPaintedStyleOption();
    testShortcutMenuNaturalWidthTracksFormattedColumn();
    testKeymapConflictsRespectActiveContexts();
    testUnboundViewportActionsReuseAuthoritativeToggles();
    testToolbarToggleBindingsReuseStateAndAvailability();
    testRegisteredGlobalActionsOverlapFocusedEditors();
    testGridBehaviorShortcutsRespectPreviewAvailability();
    testViewportShortcutsRespectMapNumericFocus();
    testCameraInversionSettingsAndPreferencePublication();
    testKeymapAlphabeticalOrderKeepsBindingControls();
    testKeymapNativeAndStylesheetRowHover();
    testKeymapRowsIgnoreMouseHover();
    testWheelScrollingUiAndMiddleButtonTransactions();
    testWheelCameraLinksPreservePreferenceTransactions();
    testCameraLinkPairHighlightRasterExpiryAndRestart();
    testCameraLinkHighlightTimerIsSafeWhenPreferencesCloses();
    testWheelModifierControlsStayCompactAndReadable();
    testZoomAndFlySpeedCommandsExposeEditableKeys();
    testKeyboardZoomIgnoresWheelSettingsAndRetainsEditorFocus();
    testMiddleButtonFlyKeyboardSpeedStepsAndClamps();
    testSavedNavigationAlternativesAndExplicitEmptyBindings();
    testMiddleButtonFlyCameraAndWheelOwnership();
    testRemappedMouseGesturesAndOwnership();
    testMousePreferencesCancelAcceptConflictAndDefaults();
    testMalformedMousePreferencesUseDefaults();
    testFlySlowerOwnsControlAndMovement();
    testSavedFlySpeedModifierAlternatives();
    testEditedKeymapReachesLiveActions();
    testDefaultModifierRoutingAndCaptureCancellation();
    testFlyBlocksOverlappingShortcutsAndEditorsRetainFocus();
    testRemappedLightDoubleClicksFollowPickOwnership();
    testMouseCameraAxisInversion();
}

void runNavigationShortcutDisplayTests()
{
    testShortcutEditorDisplaysSpaceSeparatedModifiers();
    testShortcutMenuDisplayUsesPaintedStyleOption();
    testShortcutMenuNaturalWidthTracksFormattedColumn();
}

void runNavigationShortcutContextTests()
{
    testKeymapConflictsRespectActiveContexts();
    testRegisteredGlobalActionsOverlapFocusedEditors();
    testFlyBlocksOverlappingShortcutsAndEditorsRetainFocus();
}

} // namespace sed::app_tests
