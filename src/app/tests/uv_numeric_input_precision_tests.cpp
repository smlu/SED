#include "test_common.hpp"
#include "main_window/main_window.hpp"
#include "gl_view.hpp"
#include "sed/assets/sed.hpp"
#include "ui/components/ui_decimal_format.hpp"

#include <QAction>
#include <QApplication>
#include <QInputDialog>
#include <QLocale>
#include <QDoubleSpinBox>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QPixmap>
#include <QKeyEvent>
#include <QLineEdit>
#include <QMessageBox>
#include <QTimer>

#include <array>
#include <cstdio>
#include <cmath>
#include <limits>
#include <numbers>

namespace sed
{
double preciseUvNumericInputForTest(QWidget* parent, double initial, double minimum, double maximum, bool* accepted);
}

namespace sed::app_tests
{
namespace
{
struct SystemUiLocaleScope
{
    QLocale previous = QLocale();
    UiDecimalSeparator previousPolicy = ui::uiDecimalSeparator();
    QLocale previousSystemLocale = ui::uiSystemNumericLocale();
    explicit SystemUiLocaleScope(const QLocale& locale)
    {
        // These fixed legacy vectors include the complete locale's native sign
        // and exponent spelling. Select System explicitly; Comma deliberately
        // keeps an ASCII minus independently of the host's Slovenian locale.
        QLocale::setDefault(ui::numericLocaleForPreference(UiDecimalSeparator::system, locale));
        ui::installUiDecimalPolicy(*qApp, UiDecimalSeparator::system, locale);
    }
    ~SystemUiLocaleScope()
    {
        QLocale::setDefault(previous);
        ui::installUiDecimalPolicy(*qApp, previousPolicy, previousSystemLocale);
    }
};

void runNumericInputReturnContractTests()
{
    struct FocusOutObserver final : QObject
    {
        int count{};
        bool eventFilter(QObject*, QEvent* event) override
        {
            if (event->type() == QEvent::FocusOut) ++count;
            return false;
        }
    };
    struct Spelling
    {
        double value;
        const char* cText;
        const char* commaText;
    };
    const std::array<Spelling,6> spellings{{
        {0.125,"0.125","0,125"}, {0.5,"0.5","0,5"}, {1.0,"1","1"},
        {1.0000000000000002,"1.0000000000000002","1,0000000000000002"},
        {1000000.0,"1000000","1000000"}, {-1000000.0,"-1000000","\xE2\x88\x92" "1000000"}}};
    for ( const bool comma : {false,true} )
    {
        const SystemUiLocaleScope localeScope(comma ? QLocale(QLocale::Slovenian,QLocale::Slovenia) : QLocale::c());
        QWidget parent;
        parent.setAttribute(Qt::WA_DontShowOnScreen);
        parent.resize(16,16);
        parent.setLocale(comma ? QLocale(QLocale::Slovenian,QLocale::Slovenia) : QLocale::c());
        parent.show();
        parent.activateWindow();
        QApplication::processEvents();
        for ( const auto& spelling : spellings )
        {
            QTimer::singleShot(20,&parent,[&]
            {
                auto* dialog = qobject_cast<QInputDialog*>(QApplication::activeModalWidget());
                check(dialog != nullptr,"numeric RETURN fixture opens the actual QInputDialog");
                if ( !dialog ) return;
                QApplication::setActiveWindow(dialog);
                check(dialog->locale() == QLocale(),"numeric fixture dialog uses the explicitly scoped default locale");
                auto* edit = dialog->findChild<QLineEdit*>();
                check(edit && edit->text() == QString::fromUtf8(comma ? spelling.commaText : spelling.cText),
                    "numeric displayed STRING OUTPUT matches fixed locale vectors without deriving expected text from formatter");
                dialog->accept();
            });
            bool accepted = false;
            const double returned = preciseUvNumericInputForTest(&parent,spelling.value,-1000000.0,1000000.0,&accepted);
            check(accepted && returned == spelling.value,
                "numeric untouched endpoint and nextafter RETURN preserve exact input and set OUT flag true");
        }
        for ( int path = 0; path < 9; ++path )
        {
            QTimer::singleShot(20,&parent,[&]
            {
                auto* dialog = qobject_cast<QInputDialog*>(QApplication::activeModalWidget());
                check(dialog != nullptr,"numeric boundary fixture opens the actual dialog");
                if ( !dialog ) return;
                check(dialog->locale() == QLocale(),"numeric fixture dialog uses the explicitly scoped default locale");
                if ( path == 0 ) { dialog->reject(); return; }
                auto* edit = dialog->findChild<QLineEdit*>();
                check(edit != nullptr,"numeric boundary fixture exposes its text control");
                if ( !edit ) { dialog->reject(); return; }
                QApplication::setActiveWindow(dialog);
                FocusOutObserver focusEvents;
                auto* spin = dialog->findChild<QDoubleSpinBox*>();
                check(spin != nullptr,"numeric focus contract observes the actual native spin box");
                edit->setFocus();
                const bool editInitiallyFocused = edit->hasFocus();
                check(editInitiallyFocused,
                    "numeric focus fixture establishes edit focus before observing native FocusOut");
                if (spin) spin->installEventFilter(&focusEvents);

                if ( path == 1 ) edit->setText(QStringLiteral("nan"));
                else if ( path == 2 ) edit->setText(comma ? QStringLiteral("1000000,0000000001") : QStringLiteral("1000000.0000000001"));
                else if ( path == 3 ) edit->setText(QStringLiteral("-0"));
                else if ( path == 4 ) dialog->setDoubleValue(0.5);
                else
                {
                    edit->setText(path == 5 ? (comma ? QStringLiteral("0,25") : QStringLiteral("0.25")) : QStringLiteral("nan"));
                    auto* buttons = dialog->findChild<QDialogButtonBox*>();
                    check(buttons && buttons->button(QDialogButtonBox::Ok),"numeric focus fixture has a real accept button");
                    if ( buttons && buttons->button(QDialogButtonBox::Ok) ) buttons->button(QDialogButtonBox::Ok)->setFocus();
                    const bool focusContract = path == 5
                        ? (!edit->hasFocus() && focusEvents.count == 1)
                        : (edit->hasFocus() && focusEvents.count == 0);
                    if (!focusContract)
                    {
                        std::fprintf(stderr,
                            "UVNUM focus path=%d locale=%s initially_focused=%d has_focus=%d focus_out=%d\n",
                            path, QLocale().name().toUtf8().constData(),
                            editInitiallyFocused, edit->hasFocus(), focusEvents.count);
                    }
                    check(focusContract,
                        "numeric valid text leaves focus once; invalid text retains focus without a native FocusOut");

                    // Deliberately no event turn between the focus request and
                    // programmatic input. Native valid/invalid focus routing is
                    // asserted above; the supplied value must win in either case.
                    dialog->setDoubleValue(path >= 7 ? 0.125 : 0.5);
                    const QString expectedText = path >= 7
                        ? (comma ? QStringLiteral("0,125") : QStringLiteral("0.125"))
                        : (comma ? QStringLiteral("0,5") : QStringLiteral("0.5"));
                    if (path != 8)
                    {
                        QPixmap rendered(edit->size());
                        edit->render(&rendered);
                        check(edit->text() == expectedText,
                            "numeric latest programmatic value displays the fixed concise string when painted");
                    }
                }
                if (path == 3)
                {
                    QPixmap rendered(edit->size());
                    edit->render(&rendered);
                }
                // Path8 accepts a same-value programmatic update without any
                // paint or event turn, independently of display normalization.
                dialog->accept();
            });
            bool accepted = path <= 2;
            const double returned = preciseUvNumericInputForTest(&parent,0.125,-1000000.0,1000000.0,path == 4 ? nullptr : &accepted);
            if ( path <= 2 )
                check(!accepted && returned == 0.125,
                    "numeric rejection or cancellation RETURN restores initial value and overwrites true OUT sentinel with false");
            else if ( path == 3 )
                check(accepted && returned == 0.0 && std::signbit(returned),
                    "numeric accepted negative-zero RETURN preserves its sign and sets false OUT sentinel true");
            else
                check((path == 4 || accepted) && returned == (path >= 7 ? 0.125 : 0.5),
                    "numeric latest valid value wins RETURN with nullable OUT and same-turn focus/programmatic changes");
        }
        const std::array<Spelling,2> subnormalInputs{{
            {0x1p-1074,"4.9406564584124654e-324","4,9406564584124654e-324"},
            {1e-320,"9.9998886718268301e-321","9,9998886718268301e-321"}}};
        for ( const auto& spelling : subnormalInputs )
        for ( const bool pressReturn : {false,true} )
        {
            QTimer::singleShot(20,&parent,[&]
            {
                auto* dialog = qobject_cast<QInputDialog*>(QApplication::activeModalWidget());
                check(dialog != nullptr,"subnormal input contract opens the actual dialog");
                if (!dialog) return;
                QApplication::setActiveWindow(dialog);
                check(dialog->locale() == QLocale(),"subnormal fixture dialog uses the explicitly scoped default locale");
                auto* edit = dialog->findChild<QLineEdit*>();
                check(edit != nullptr,"subnormal input contract exposes its text control");
                if (!edit) { dialog->reject(); return; }
                edit->setFocus();
                edit->selectAll();
                const QString input = QString::fromLatin1(comma ? spelling.commaText : spelling.cText);
                for (const QChar character : input)
                {
                    QKeyEvent typed(QEvent::KeyPress,0,Qt::NoModifier,QString(character));
                    QCoreApplication::sendEvent(edit,&typed);
                }
                check(edit->text() == input,"subnormal input accepts the complete fixed locale spelling character by character");
                if (pressReturn)
                {
                    QKeyEvent enter(QEvent::KeyPress,Qt::Key_Return,Qt::NoModifier);
                    QCoreApplication::sendEvent(edit,&enter);
                    check(!dialog->isVisible(),"subnormal Return key completes the actual numeric dialog");
                    if (dialog->isVisible()) dialog->reject();
                }
                else
                {
                    auto* buttons = dialog->findChild<QDialogButtonBox*>();
                    check(buttons && buttons->button(QDialogButtonBox::Ok),"subnormal focus contract exposes a real focus target");
                    if (buttons && buttons->button(QDialogButtonBox::Ok)) buttons->button(QDialogButtonBox::Ok)->setFocus();
                    const QString formatted = !comma ? input : spelling.value == 0x1p-1074
                        ? QString::fromUtf16(u"4,9406564584124654e\u2212324")
                        : QString::fromUtf16(u"9,9998886718268301e\u2212321");
                    check(!edit->hasFocus() && edit->text() == formatted,
                        "subnormal focus formatting retains the exact fixed finite spelling");
                    dialog->accept();
                }
            });
            bool accepted = false;
            const double returned = preciseUvNumericInputForTest(&parent,0.125,-1000000.0,1000000.0,&accepted);
            check(accepted && returned == spelling.value,
                "subnormal numeric RETURN survives native focus formatting and Return acceptance without decimal quantization");
        }
        parent.hide();
        QApplication::processEvents();
    }
}

struct NumericAnswer
{
    QString text;
    bool cancel{};
    bool injectInvalid{};
    bool stepUp{};
    bool focusRoundTrip{};
};

void prepareNumericUvFixture(MainWindow& window)
{
    editor::DocumentState state;
    state.level.layers = {"Layer0"};
    state.level.uvCoordinateSpace = UvCoordinateSpace::Normalized;
    Sector sector;
    sector.vertices = {{0,0,0,0},{1,0,0,0},{1,1,0,0},{0,1,0,0}};
    Surface face;
    face.vertices = {0,1,2,3};
    face.normal = {0,0,1};
    face.texVertices = {{0,0,ColorWhite},{1,0,ColorWhite},{1,1,ColorWhite},{0,1,ColorWhite}};
    sector.surfaces.push_back(face);
    state.level.sectors.push_back(sector);
    window.setDocumentStateForTest(std::move(state));
    window.activateSurfaceEditModeForTest();
    window.mutableDocumentStateForTest().selection.surfaces = {{0,0}};
    window.glViewForTest()->setSelectedSurface(0,0);
    window.updateEditActionsForTest();
}

void enterNumericUvCommand(MainWindow& window, const char* command, const std::vector<NumericAnswer>& answers)
{
    auto* action = window.findChild<QAction*>(QString::fromLatin1(command));
    check(action && action->isEnabled(), "UV numeric fixture exposes an enabled actual command");
    if (!action || !action->isEnabled()) return;
    std::size_t prompt = 0;
    QTimer respond;
    respond.setInterval(5);
    QObject::connect(&respond, &QTimer::timeout, [&]
    {
        QWidget* modal = QApplication::activeModalWidget();
        if (auto* dialog = qobject_cast<QInputDialog*>(modal))
        {
            check(dialog->locale() == QLocale(),"actual UV numeric command uses the scoped locale under test");
            if (prompt >= answers.size()) { dialog->reject(); return; }
            const auto& answer = answers[prompt++];
            if (answer.cancel) { dialog->reject(); return; }
            auto* edit = dialog->findChild<QLineEdit*>();
            check(edit != nullptr, "UV numeric dialog exposes its editable text");
            if (!edit) { dialog->reject(); return; }
            edit->selectAll();
            if (answer.injectInvalid)
                edit->setText(answer.text); // Exercise accept-time validation before a spinbox can fix up invalid text.
            else
            {
                QString typedText = answer.text;
                typedText.replace(QStringLiteral("."),dialog->locale().decimalPoint());
                QString prefix;
                for (const QChar character : typedText)
                {
                    prefix += character;
                    QKeyEvent typed(QEvent::KeyPress, 0, Qt::NoModifier, QString(character));
                    QCoreApplication::sendEvent(edit, &typed);
                    if (prefix == QStringLiteral("-") || prefix == QStringLiteral("0")+dialog->locale().decimalPoint() || prefix == QStringLiteral("1e"))
                        check(edit->text() == prefix,"precise UV typing preserves incomplete sign, decimal and exponent text");
                }
                if (answer.stepUp)
                {
                    auto* spin = dialog->findChild<QDoubleSpinBox*>();
                    check(spin != nullptr,"precise UV input retains its numeric stepper");
                    if (spin)
                    {
                        QKeyEvent step(QEvent::KeyPress,Qt::Key_Up,Qt::NoModifier);
                        QCoreApplication::sendEvent(spin,&step);
                    }
                }
                if (answer.focusRoundTrip)
                {
                    auto* buttons = dialog->findChild<QDialogButtonBox*>();
                    check(buttons && buttons->button(QDialogButtonBox::Ok),"precise UV fixture exposes a real focus target");
                    if (buttons && buttons->button(QDialogButtonBox::Ok))
                        buttons->button(QDialogButtonBox::Ok)->setFocus();
                    edit->setFocus();
                }
                check(edit->text().size() < 40,"precise UV input remains concise after typing, stepping and focus changes");
            }
            dialog->accept();
        }
        else if (auto* message = qobject_cast<QMessageBox*>(modal)) message->accept();
    });
    respond.start();
    action->trigger();
    check(prompt > 0 && prompt <= answers.size(), "UV numeric command executes real modal input prompts");
}
}

void runUvNumericInputPrecisionTests()
{
    runNumericInputReturnContractTests();
    MainWindow window;
    window.setAttribute(Qt::WA_DontShowOnScreen);
    const std::array<QPointF,4> initial{QPointF{0,0},QPointF{1,0},QPointF{1,1},QPointF{0,1}};
    for (const QLocale& inputLocale : {QLocale::c(),QLocale(QLocale::Slovenian,QLocale::Slovenia)})
    for (int operation = 0; operation < 6; ++operation)
    {
        const SystemUiLocaleScope localeScope(inputLocale);
        window.setLocale(inputLocale);
        prepareNumericUvFixture(window);
        const auto before = assets::saveToSedBytes(window.documentStateForTest().level);
        const auto depth = window.undoDepthForTest();
        const double factor = std::nextafter(1.0,2.0);
        if (operation == 0)
            enterNumericUvCommand(window,"surface.uv.move",{{QStringLiteral("0.0000001")},{QStringLiteral("0")}});
        else if (operation == 1)
            enterNumericUvCommand(window,"surface.uv.rotate",{{QStringLiteral("0.00001")}});
        else if (operation == 2)
            enterNumericUvCommand(window,"surface.uv.scale",{{QString::number(factor,'g',17)},{QStringLiteral("1")}});
        else if (operation == 3)
            enterNumericUvCommand(window,"surface.uv.move",{{QStringLiteral("1e-7"),false,false,false,true},{QStringLiteral("0")}});
        else if (operation == 4)
            enterNumericUvCommand(window,"surface.uv.move",{{QStringLiteral("1e-7"),false,false,true,true},{QStringLiteral("0")}});
        else
            enterNumericUvCommand(window,"surface.uv.move",{{QStringLiteral("-0.0000001"),false,false,false,true},{QStringLiteral("0")}});
        const auto& actual = window.documentStateForTest().level.sectors[0].surfaces[0].texVertices;
        bool exact = actual.size() == initial.size();
        for (std::size_t i = 0; exact && i < actual.size(); ++i)
        {
            double u = initial[i].x(), v = initial[i].y();
            if (operation == 0 || operation == 3) u += 1e-7;
            else if (operation == 4) u += 1.0+1e-7;
            else if (operation == 5) u -= 1e-7;
            else if (operation == 1)
            {
                const double angle = 0.00001*std::numbers::pi/180.0;
                const double x = u-0.5, y = v-0.5;
                u = 0.5+x*std::cos(angle)-y*std::sin(angle);
                v = 0.5+x*std::sin(angle)+y*std::cos(angle);
            }
            else u = 0.5+(u-0.5)*factor;
            exact = actual[i].u == static_cast<float>(u) && actual[i].v == static_cast<float>(v);
        }
        if (!exact || window.undoDepthForTest() != depth+1)
        {
            std::fprintf(stderr,"UVNUM locale=%s operation=%d exact=%d depth=%zu/%zu\n",inputLocale.name().toUtf8().constData(),operation,exact,window.undoDepthForTest(),depth);
            for (const auto& point : actual) std::fprintf(stderr,"  stored=(%.17g,%.17g)\n",double(point.u),double(point.v));
        }
        check(exact && window.undoDepthForTest() == depth+1,
            operation == 0 ? "typed tiny UV Move retains exact representable stored changes"
            : operation == 1 ? "typed tiny UV Rotate retains exact representable stored changes"
            : operation == 2 ? "typed nextafter-one UV Scale retains exact representable stored changes"
            : operation == 3 ? "character-typed scientific UV input survives focus changes with exact stored results"
            : operation == 4 ? "UV numeric stepping uses the complete typed value and retains concise exact results"
            : "negative tiny UV input preserves incomplete sign typing and exact values across focus changes");
        if (window.undoDepthForTest() == depth+1)
            check(window.undoForTest() && assets::saveToSedBytes(window.documentStateForTest().level) == before,
                "precise numeric UV command Undo restores exact native document bytes");
    }
    const SystemUiLocaleScope invalidInputLocaleScope(QLocale::c());
    for (const QString& invalidText : {QString{},QStringLiteral("nan"),QStringLiteral("inf"),QStringLiteral("1000001")})
    {
        prepareNumericUvFixture(window);
        const auto before = assets::saveToSedBytes(window.documentStateForTest().level);
        const auto depth = window.undoDepthForTest();
        if (!invalidText.isEmpty())
            enterNumericUvCommand(window,"surface.uv.move",{{invalidText,false,true},{QStringLiteral("1")}});
        else
            enterNumericUvCommand(window,"surface.uv.move",{{QStringLiteral("0.0000001")},{QString{},true}});
        check(window.undoDepthForTest() == depth && assets::saveToSedBytes(window.documentStateForTest().level) == before,
            !invalidText.isEmpty() ? "invalid or nonfinite UV numeric text is rejected atomically without reverting to an old valid value"
                    : "canceling the second precise UV prompt leaves the entire document and Undo untouched");
    }
}
}
