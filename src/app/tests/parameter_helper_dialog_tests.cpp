#include "test_common.hpp"

#include "thing/thing_parameter_editor_utils.hpp"
#include "ui/components/dialog_window_policy.hpp"

#include <QApplication>
#include <QDialog>
#include <QFontDatabase>
#include <QFontMetrics>
#include <QInputDialog>
#include <QPushButton>
#include <QScreen>
#include <QStyle>
#include <QStyleOptionTitleBar>
#include <QTimer>

namespace sed::app_tests
{

void runParameterHelperDialogTests()
{
    {
        const QRect available(0, 0, 1920, 1080);
        const QRect anchor(300, 300, 90, 24);
        check(ui::dialogPositionNearAnchor(anchor, QSize(360, 220), available)
                == QPoint(398, 300),
            "parameter helper dialog opens immediately to the right of its invoking button when space permits");

        const QRect rightEdgeAnchor(1810, 1020, 80, 24);
        check(ui::dialogPositionNearAnchor(
                    rightEdgeAnchor, QSize(360, 220), available)
                == QPoint(1442, 860),
            "parameter helper dialog flips left and clamps vertically near the lower-right screen edge");

        const QRect negativeScreen(-1920, 0, 1920, 1080);
        const QRect negativeScreenAnchor(-40, 40, 24, 24);
        check(ui::dialogPositionNearAnchor(
                    negativeScreenAnchor, QSize(480, 300), negativeScreen)
                == QPoint(-528, 40),
            "parameter helper dialog placement supports screens with negative desktop coordinates");

        check(ui::dialogPositionNearAnchor(
                    QRect(900, 700, 40, 20), QSize(2200, 1200), available)
                == available.topLeft(),
            "parameter helper dialog placement stays at the screen origin when a dialog exceeds available bounds");
    }

    {
        QDialog dialog{};
        dialog.setWindowTitle(QStringLiteral("Choose a particularly long parameter caption"));
        ui::applyDialogWindowPolicy(dialog);
        check(!dialog.windowFlags().testFlag(Qt::WindowMinimizeButtonHint)
                && !dialog.windowFlags().testFlag(Qt::WindowMaximizeButtonHint)
                && dialog.windowFlags().testFlag(Qt::WindowCloseButtonHint),
            "parameter helper dialog captions expose close while hiding minimize and maximize controls");
        check(dialog.minimumWidth() >= ui::minimumDialogWidthForCaption(dialog),
            "parameter helper dialog minimum width reserves the complete native caption");
    }

    QScreen* screen = QGuiApplication::primaryScreen();
    check(screen != nullptr, "parameter helper dialog integration test has a primary screen");
    if (!screen)
    {
        return;
    }

    QWidget host{};
    host.resize(260, 150);
    auto* anchor = new QPushButton(QStringLiteral("Choose"), &host);
    anchor->setGeometry(host.width() - 90, host.height() - 42, 80, 30);
    const QRect available = screen->availableGeometry();
    host.move(available.right() + 1 - host.width(), available.bottom() + 1 - host.height());
    host.show();
    QCoreApplication::processEvents();

    bool inspected = false;
    QTimer::singleShot(0, &host, [&]()
    {
        for (QWidget* topLevel : QApplication::topLevelWidgets())
        {
            auto* dialog = qobject_cast<QInputDialog*>(topLevel);
            if (!dialog || dialog->windowTitle() != QStringLiteral("Choose collide"))
            {
                continue;
            }

            inspected = true;
            check(!dialog->windowFlags().testFlag(Qt::WindowMinimizeButtonHint)
                    && !dialog->windowFlags().testFlag(Qt::WindowMaximizeButtonHint),
                "collision-type helper has no minimize or maximize window controls");
            const QFont titleFont = QFontDatabase::systemFont(QFontDatabase::TitleFont);
            const QFontMetrics titleMetrics(titleFont);
            const int titleTextWidth = titleMetrics.horizontalAdvance(dialog->windowTitle()) + 12;
            const int titleBarHeight = std::max(
                dialog->style()->pixelMetric(QStyle::PM_TitleBarHeight, nullptr, dialog),
                titleMetrics.height() + 8);
            QStyleOptionTitleBar titleOption{};
            titleOption.initFrom(dialog);
            titleOption.rect = QRect(0, 0, dialog->frameGeometry().width(), titleBarHeight);
            titleOption.text = dialog->windowTitle();
            titleOption.icon = dialog->windowIcon();
            titleOption.titleBarFlags = dialog->windowFlags();
            titleOption.titleBarState = static_cast<int>(dialog->windowState());
            const QRect titleLabelRect = dialog->style()->subControlRect(
                QStyle::CC_TitleBar,
                &titleOption,
                QStyle::SC_TitleBarLabel,
                dialog);
            const bool captionFits = titleLabelRect.isValid()
                ? titleLabelRect.width() >= titleTextWidth
                : dialog->frameGeometry().width() >= ui::minimumDialogWidthForCaption(*dialog);
            if (!captionFits)
            {
                std::cerr << "FAIL: collision-type caption geometry label="
                          << titleLabelRect.x() << ',' << titleLabelRect.y() << ' '
                          << titleLabelRect.width() << 'x' << titleLabelRect.height()
                          << " textWidth=" << titleTextWidth
                          << " frameWidth=" << dialog->frameGeometry().width()
                          << " clientWidth=" << dialog->width()
                          << " minimumWidth=" << dialog->minimumWidth()
                          << " policyWidth=" << ui::minimumDialogWidthForCaption(*dialog)
                          << "\n";
            }
            check(captionFits,
                "collision-type helper keeps its full title caption visible");
            const QRect dialogFrame = dialog->frameGeometry();
            if (!available.contains(dialogFrame))
            {
                std::cerr << "FAIL: collision-type dialog frame "
                          << dialogFrame.x() << ',' << dialogFrame.y() << ' '
                          << dialogFrame.width() << 'x' << dialogFrame.height()
                          << " outside available screen "
                          << available.x() << ',' << available.y() << ' '
                          << available.width() << 'x' << available.height() << "\n";
            }
            check(available.contains(dialogFrame),
                "collision-type helper is clamped inside the available screen at the lower-right edge");

            const QRect anchorGlobal(anchor->mapToGlobal(QPoint(0, 0)), anchor->size());
            const QRect dialogGlobal = dialog->frameGeometry();
            const bool besideRight = dialogGlobal.left() >= anchorGlobal.right()
                && dialogGlobal.left() - anchorGlobal.right() <= 14;
            const bool besideLeft = anchorGlobal.left() >= dialogGlobal.right()
                && anchorGlobal.left() - dialogGlobal.right() <= 14;
            check(besideRight || besideLeft,
                "collision-type helper opens beside the button that invoked it");
            dialog->accept();
            break;
        }
    });

    const std::optional<QString> picked = thing::parameter_editor::openValueEditor(
        &host,
        ProjectType::MOTS,
        "collide",
        {},
        QStringLiteral("2"),
        anchor);
    check(inspected, "collision-type helper dialog was observed during its modal event loop");
    check(picked && *picked == QStringLiteral("2"),
        "accepting the anchored collision-type helper preserves the current serialized value");
}

} // namespace sed::app_tests
