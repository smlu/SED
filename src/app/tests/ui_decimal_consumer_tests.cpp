#include "test_common.hpp"

#include "app_settings.hpp"
#include "gl_view.hpp"
#include "main_window/main_window.hpp"
#include "sed/level/build.hpp"
#include "sed/level/thing.hpp"
#include "sed/assets/resource_locator.hpp"
#include "thing/thing_parameter_utils.hpp"
#include "ui/components/light_parameters_dialog.hpp"
#include "ui/components/scrub_float_edit.hpp"
#include "ui/components/thing_light_value_dialog.hpp"
#include "ui/components/ui_decimal_format.hpp"
#include "ui/components/typed_numeric_text.hpp"
#include "ui/checks/level_check_utils.hpp"
#include "ui/resources/resource_picker.hpp"

#include <QApplication>
#include <QAction>
#include <QComboBox>
#include <QDialog>
#include <QDoubleSpinBox>
#include <QDir>
#include <QFile>
#include <QPlainTextEdit>
#include <QTemporaryDir>
#include <QLabel>
#include <QLineEdit>
#include <QKeyEvent>
#include <QMenu>
#include <QSettings>
#include <QToolButton>
#include <QPushButton>
#include <QTimer>
#include <QTreeWidget>
#include <QTreeWidgetItemIterator>
#include <QWheelEvent>

#include <cmath>
#include <filesystem>
#include <limits>
#include <utility>
#include <unordered_set>

namespace sed::app_tests
{
namespace
{

class DecimalPreferenceScope final
{
public:
    DecimalPreferenceScope()
        : previousPolicy_(ui::uiDecimalSeparator())
    {
        QSettings settings = settingsStore();
        hadPreference_ = settings.contains(QStringLiteral("ui/decimal_separator"));
        previousPreference_ = settings.value(QStringLiteral("ui/decimal_separator"));
    }

    ~DecimalPreferenceScope()
    {
        QSettings settings = settingsStore();
        if ( hadPreference_ )
        {
            settings.setValue(QStringLiteral("ui/decimal_separator"), previousPreference_);
        }
        else
        {
            settings.remove(QStringLiteral("ui/decimal_separator"));
        }
        settings.sync();
        ui::applyUiDecimalSeparator(previousPolicy_);
    }

private:
    UiDecimalSeparator previousPolicy_{};
    QVariant previousPreference_{};
    bool hadPreference_{};
};

void editNumericText(QLineEdit& edit, const QString& text)
{
    edit.setText(text);
    edit.setModified(true);
    check(QMetaObject::invokeMethod(&edit, "textEdited", Qt::DirectConnection,
        Q_ARG(QString, text)),
        "decimal consumers: a real numeric editor delivers its typed preview signal");
}

bool sendCleaveWheel(GlView& view, const int angleDelta = 120,
    const Qt::KeyboardModifiers modifiers = Qt::ControlModifier)
{
    const QPointF position = view.rect().center();
    QWheelEvent event(position, view.mapToGlobal(position.toPoint()), {}, {0, angleDelta},
        Qt::NoButton, modifiers, Qt::NoScrollPhase, false);
    event.setAccepted(false);
    QApplication::sendEvent(&view, &event);
    return event.isAccepted();
}

void runCleaveWheelIdentityDecimalConsumers()
{
    const UiDecimalSeparator previousPolicy = ui::uiDecimalSeparator();
    ui::applyUiDecimalSeparator(UiDecimalSeparator::comma);
    editor::DocumentState state{};
    state.level.kind = ProjectType::IJIM;
    state.level.layers.emplace_back("Layer0");
    level::levelCreateCube(state.level, -2.0, 0.0, -2.0,
        TVector{0.0, -1.0, 0.0}, TVector{4.0, 0.0, 0.0}).layer = 0;
    state.selection.sectors.insert(0);
    GlView view{};
    view.resize(640, 480);
    view.setLevel(&state.level);
    view.setSelectionState(0, -1, -1, -1, state.selection);
    view.setSectorCleaveToolEnabled(true);
    view.setCleaveStagedMode(true);
    view.setCleaveChainEnabled(false);
    view.setTransformGridSnapEnabled(false);
    view.setCameraProjectionType(app::ViewportCamera::ProjectionType::Orthographic);
    view.setOrbitViewImmediate(31.0f, 22.0f, 13.0f);
    view.setCameraOrbitDistance(8.0f);
    view.setCameraOrthoScale(96.0f);
    view.captureStagedCleaveContextForTest(false);
    view.addStagedCleaveLineForTest({-1, 0, -1}, {1, 0, -1}, {0, 0, 1});
    view.addStagedCleaveLineForTest({1, 0, -1}, {1, 0, 1}, {-1, 0, 0});
    auto draft = view.captureStagedCleaveDraftState();
    draft.selectedCleaveLines = {0, 1};
    draft.selectedCleaveLine = 1;
    view.restoreStagedCleaveDraftState(draft);
    const auto first = view.stagedCleaveLineFirstForTest(0);
    const auto second = view.stagedCleaveLineSecondForTest(1);
    const auto history = view.captureStagedCleaveHistory();
    const auto sameHistoryPosition = [&]()
    {
        const auto current = view.captureStagedCleaveHistory();
        return history && current && current->cursor == history->cursor
            && current->snapshots.size() == history->snapshots.size();
    };
    const auto release = [&]()
    {
        QKeyEvent event(QEvent::KeyRelease, Qt::Key_Control, Qt::NoModifier);
        QApplication::sendEvent(&view, &event);
    };

    view.setStagedCleaveTransformMode(GlView::StagedCleaveTransformMode::Scale);
    const bool identityAccepted = sendCleaveWheel(view) && sendCleaveWheel(view, -120);
    check(identityAccepted
            && view.stagedCleaveTransformHudText() == QStringLiteral("Scale 1,00×")
            && view.stagedCleaveLineFirstForTest(0) == first
            && view.stagedCleaveLineSecondForTest(1) == second && sameHistoryPosition(),
        "decimal consumers: a cumulative wheel returning to identity displays Comma and restores exact endpoints without a command");
    ui::applyUiDecimalSeparator(UiDecimalSeparator::dot);
    check(view.stagedCleaveTransformHudText() == QStringLiteral("Scale 1.00×")
            && sameHistoryPosition(),
        "decimal consumers: a live identity wheel HUD refreshes to Dot without a history change");
    release();
    check(view.stagedCleaveTransformHudText().isEmpty() && sameHistoryPosition(),
        "decimal consumers: Control release immediately hides an identity wheel HUD without a history change");

    ui::applyUiDecimalSeparator(UiDecimalSeparator::comma);
    view.setStagedCleaveTransformMode(GlView::StagedCleaveTransformMode::Rotate);
    view.setCleaveAngleStepDegrees(0.0);
    const bool turnAccepted = sendCleaveWheel(view, 4320) && sendCleaveWheel(view, 4320);
    check(turnAccepted
            && view.stagedCleaveTransformHudText() == QStringLiteral("Rotate 360,00°")
            && view.stagedCleaveLineFirstForTest(0) == first
            && view.stagedCleaveLineSecondForTest(1) == second && sameHistoryPosition(),
        "decimal consumers: a complete cumulative wheel turn displays Comma while retaining exact baseline endpoints");
    ui::applyUiDecimalSeparator(UiDecimalSeparator::dot);
    check(view.stagedCleaveTransformHudText() == QStringLiteral("Rotate 360.00°")
            && sameHistoryPosition(),
        "decimal consumers: a complete-turn HUD refreshes to Dot without authoring a command");
    release();
    check(view.stagedCleaveTransformHudText().isEmpty() && sameHistoryPosition(),
        "decimal consumers: Control release immediately hides a complete-turn HUD without authoring a command");

    ui::applyUiDecimalSeparator(UiDecimalSeparator::comma);
    auto options = view.cleaveShapeOptions();
    options.kind = ui::viewport::CleaveShapeKind::Rectangle;
    options.dimensionsLinked = false;
    options.width = 1.6;
    options.height = 1.0;
    options.rotationDegrees = 0.0;
    options.placementArmed = true;
    options.cameraPlane = true;
    view.setCleaveShapeOptions(options);
    const auto rotateModifiers = Qt::ControlModifier | Qt::ShiftModifier;
    const bool shapeAccepted = sendCleaveWheel(view, 120, rotateModifiers)
        && sendCleaveWheel(view, 120, rotateModifiers);
    check(shapeAccepted && view.cleaveShapeOptions().rotationDegrees == 10.0
            && view.stagedCleaveTransformHudText() == QStringLiteral("Rotate 10,00°")
            && sameHistoryPosition(),
        "decimal consumers: the armed Ctrl+Shift wheel shows its accumulated rotation using Comma without authored history");
    ui::applyUiDecimalSeparator(UiDecimalSeparator::dot);
    check(view.stagedCleaveTransformHudText() == QStringLiteral("Rotate 10.00°")
            && view.cleaveShapeOptions().rotationDegrees == 10.0 && sameHistoryPosition(),
        "decimal consumers: changing policy during an armed wheel updates its HUD while preserving rotation and history");
    release();
    check(view.stagedCleaveTransformHudText().isEmpty()
            && view.cleaveShapeOptions().rotationDegrees == 10.0 && sameHistoryPosition(),
        "decimal consumers: Control release immediately hides the armed-shape HUD while preserving rotation and history");

    ui::applyUiDecimalSeparator(UiDecimalSeparator::comma);
    view.setCleaveAngleStepDegrees(0.001);
    sendCleaveWheel(view, -120, rotateModifiers);
    check(view.stagedCleaveTransformHudText() == QStringLiteral("Rotate 0,00°")
            && std::abs(view.cleaveShapeOptions().rotationDegrees - 9.999) < 1.0e-12
            && sameHistoryPosition(),
        "decimal consumers: armed wheel negative zero is normalized before localized display without losing the exact option value");
    release();
    ui::applyUiDecimalSeparator(previousPolicy);
}

QTreeWidgetItem* propertyItem(MainWindow& window, const QString& name)
{
    auto* const tree = window.findChild<QTreeWidget*>(QStringLiteral("propertiesTree"));
    if ( !tree )
    {
        return nullptr;
    }
    for ( QTreeWidgetItemIterator item(tree); *item; ++item )
    {
        if ( (*item)->text(0).trimmed() == name )
        {
            return *item;
        }
    }
    return nullptr;
}

QLineEdit* propertyLineEdit(MainWindow& window, const QString& name)
{
    auto* const item = propertyItem(window, name);
    auto* const tree = window.findChild<QTreeWidget*>(QStringLiteral("propertiesTree"));
    QWidget* const editor = item && tree ? tree->itemWidget(item, 1) : nullptr;
    return qobject_cast<QLineEdit*>(editor)
        ? qobject_cast<QLineEdit*>(editor)
        : editor ? editor->findChild<QLineEdit*>() : nullptr;
}

void runTypedNumericBridgeCases()
{
    ui::applyUiDecimalSeparator(UiDecimalSeparator::comma);
    const QLocale inputLocale = ui::uiNumericLocale();
    const QString slash = QStringLiteral("(0.125/-1.5/2.25)");
    const QString slashUi = QStringLiteral("(0,125/-1,5/2,25)");
    const auto slashCanonical = ui::canonicalTypedNumericText(slashUi, ValueType::vect, inputLocale);
    check(ui::localizeTypedNumericText(slash, ValueType::vect) == slashUi
            && slashCanonical && *slashCanonical == slash,
        "decimal consumers: typed slash tuples preserve exact components and separators through the UI bridge");
    const QString frame = QStringLiteral("(0.125/2.5/-3.75:4.5/5.625/6.75)");
    const QString frameUi = QStringLiteral("(0,125/2,5/-3,75:4,5/5,625/6,75)");
    const auto frameCanonical = ui::canonicalTypedNumericText(frameUi, ValueType::frame, inputLocale);
    check(ui::localizeTypedNumericText(frame, ValueType::frame) == frameUi
            && frameCanonical && *frameCanonical == frame,
        "decimal consumers: typed frames keep the position/orientation colon and all six numeric spellings");
    const auto spaceCanonical = ui::canonicalTypedNumericText(
        QStringLiteral("0,125 -1,5 2,25"), ValueType::vect, inputLocale);
    check(ui::localizeTypedNumericText(QStringLiteral("0.125 -1.5 2.25"), ValueType::vect)
                == QStringLiteral("0,125 -1,5 2,25")
            && spaceCanonical && *spaceCanonical == QStringLiteral("0.125 -1.5 2.25"),
        "decimal consumers: space-delimited typed components are not split at decimal commas");
    const QString commaTuple = QStringLiteral("(0.5,1.5,-2.5)");
    check(ui::localizeTypedNumericText(commaTuple, ValueType::vect)
                == QStringLiteral("(0,5/1,5/-2,5)")
            && commaTuple == QStringLiteral("(0.5,1.5,-2.5)"),
        "decimal consumers: canonical comma tuples normalize only their UI component separators and retain their accepted baseline");
    const TplValue classicLight = deserializeTplValue("light", "1.25");
    const TplValue ijimLight = deserializeTplValue("light", "(0.125/0.25/0.5/2.75)");
    check(ui::localizeTypedNumericText(QString::fromStdString(classicLight.asString()),
                ui::typedNumericTextType(classicLight)) == QStringLiteral("1,25")
            && ui::localizeTypedNumericText(QString::fromStdString(ijimLight.asString()),
                ui::typedNumericTextType(ijimLight)) == QStringLiteral("(0,125/0,25/0,5/2,75)"),
        "decimal consumers: classic scalar and IJIM RGB/range light parameters share one numeric presentation boundary");
    const auto precise = ui::canonicalTypedNumericText(
        QStringLiteral("1,2500000000000001e-08"), ValueType::floating, inputLocale);
    check(precise && *precise == QStringLiteral("1.2500000000000001e-08")
            && !ui::canonicalTypedNumericText(QStringLiteral("1.234,5"), ValueType::floating, inputLocale)
            && !ui::canonicalTypedNumericText(QStringLiteral("(1.234,5/0/0)"), ValueType::vect, inputLocale)
            && !ui::canonicalTypedNumericText(QStringLiteral("(nan/0/0)"), ValueType::vect, inputLocale),
        "decimal consumers: typed numeric bridges preserve full scientific spelling and reject grouping or nonfinite components");
    TplValue opaque{};
    opaque.name = "customtext";
    opaque.type = ValueType::str;
    opaque.stringValue = "asset.1,25.mat";
    const auto opaqueText = ui::canonicalTypedNumericText(
        QString::fromStdString(opaque.stringValue), ui::typedNumericTextType(opaque), inputLocale);
    check(ui::typedNumericTextType(opaque) == ValueType::str
            && ui::localizeTypedNumericText(QString::fromStdString(opaque.stringValue), ValueType::str)
                == QString::fromStdString(opaque.stringValue)
            && opaqueText && *opaqueText == QString::fromStdString(opaque.stringValue),
        "decimal consumers: unknown parameter text remains opaque even when it contains decimal punctuation");
}

void runDisplayAndQueryDecimalConsumers()
{
    ui::applyUiDecimalSeparator(UiDecimalSeparator::comma);
    MainWindow window{};
    window.setAttribute(Qt::WA_DontShowOnScreen);
    editor::DocumentState state{};
    state.level.kind = ProjectType::IJIM;
    state.level.layers.emplace_back("Layer0");
    auto& sector = level::levelCreateCube(state.level, 0, 0, 0,
        TVector{0, -1, 0}, TVector{0.125, 0, 0});
    sector.layer = 0;
    sector.soundVolume = 0.125;
    sector.tint = ColorF{0.25f, 0.5f, 0.75f, 1.0f};
    state.level.sectors.emplace_back().soundVolume = 0.5;
    state.selection.surfaces.insert({0, 0});
    window.setDocumentStateForTest(std::move(state));
    window.activateSurfaceEditModeForTest();
    window.activateSurfaceEdgeSubModeForTest();
    auto& surfaceSelection = window.mutableDocumentStateForTest().selection;
    surfaceSelection.surfaces.insert({0, 0});
    if ( auto* const view = window.glViewForTest() )
    {
        view->setSelectionState(0, 0, -1, -1, surfaceSelection);
        view->applySurfaceEdgePointSelectionForTest(SurfaceRef{0, 0}, 0, Qt::NoModifier);
    }
    window.rebuildPropertiesForTest();
    auto* const length = propertyItem(window, QStringLiteral("Length"));
    auto* const scale = window.findChild<QDoubleSpinBox*>(QStringLiteral("uvEditorScaleFactorSpin"));
    const auto undoBefore = window.undoDepthForTest();
    if (!length || length->text(1) != QStringLiteral("0,125000"))
    {
        auto* const view = window.glViewForTest();
        std::cerr << "decimal Surface Length diagnostic: item=" << (length != nullptr)
            << " text=" << (length ? length->text(1).toStdString() : "<missing>")
            << " role=" << (length ? length->data(1, Qt::UserRole + 220).toString().toStdString() : "<missing>")
            << " sectors=" << window.documentStateForTest().selection.sectors.size()
            << " surfaces=" << window.documentStateForTest().selection.surfaces.size()
            << " active=" << (view ? view->activeSectorIndex() : -1)
            << "/" << (view ? view->activeSurfaceIndex() : -1) << '\n';
        if (auto* const tree = window.findChild<QTreeWidget*>(QStringLiteral("propertiesTree")))
        {
            int printed = 0;
            for (QTreeWidgetItemIterator item(tree); *item && printed < 20; ++item, ++printed)
            {
                std::cerr << "  " << (*item)->text(0).toStdString()
                    << "=" << (*item)->text(1).toStdString() << '\n';
            }
        }
    }
    check(length && length->text(1) == QStringLiteral("0,125000"),
        "decimal consumers: actual Surface edge Length readout uses Comma");
    check(scale && scale->toolTip().contains(QStringLiteral("0,01"))
            && scale->toolTip().contains(QStringLiteral("0,5"))
            && !scale->toolTip().contains(QStringLiteral("0.01")),
        "decimal consumers: fractional UV scale examples in actual tooltips use the numeric policy");
    ui::applyUiDecimalSeparator(UiDecimalSeparator::dot);
    QApplication::processEvents();
    check(length && propertyItem(window, QStringLiteral("Length")) == length
            && length->text(1) == QStringLiteral("0.125000")
            && scale && scale->toolTip().contains(QStringLiteral("0.01"))
            && window.undoDepthForTest() == undoBefore,
        "decimal consumers: runtime policy refreshes Surface Length and UV tooltip without replacing property rows or history");

    window.activateSectorEditModeForTest();
    window.mutableDocumentStateForTest().selection.clear();
    if ( auto* const view = window.glViewForTest() )
    {
        view->setSelectionState(-1, -1, -1, -1, window.documentStateForTest().selection);
    }
    auto* const query = window.findChild<QAction*>(QStringLiteral("edit.find"));
    check(query != nullptr, "decimal consumers: the production query action is available");
    if ( !query )
    {
        return;
    }
    auto queryRow = [](QDialog& dialog, const QString& name)
    {
        for ( auto* const label : dialog.findChildren<QLabel*>() )
        {
            if ( label->text() == name )
            {
                return label->parentWidget();
            }
        }
        return static_cast<QWidget*>(nullptr);
    };
    ui::applyUiDecimalSeparator(UiDecimalSeparator::comma);
    bool scalarQueryInspected = false;
    bool scalarPlaceholderLocalized = false;
    QTimer::singleShot(0, &window, [&]()
    {
        auto* const dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget());
        QWidget* const row = dialog ? queryRow(*dialog, QStringLiteral("Volume")) : nullptr;
        auto* const edit = row ? row->findChild<QLineEdit*>() : nullptr;
        auto* const match = row ? row->findChild<QComboBox*>() : nullptr;
        QPushButton* select = nullptr;
        if ( dialog )
        {
            for ( auto* const field : dialog->findChildren<QLineEdit*>() )
            {
                field->clear();
            }
            for ( auto* const button : dialog->findChildren<QPushButton*>() )
            {
                if ( button->text() == QStringLiteral("Select All") )
                {
                    select = button;
                }
            }
        }
        if ( edit && match && select )
        {
            scalarPlaceholderLocalized = edit->placeholderText().contains(QStringLiteral("0,500000"));
            match->setCurrentIndex(match->findData(1));
            edit->setText(QStringLiteral("0,125"));
            edit->setModified(true);
            scalarQueryInspected = select->isEnabled();
            if ( scalarQueryInspected )
            {
                select->click();
                return;
            }
        }
        if ( dialog )
        {
            dialog->reject();
        }
    });
    query->trigger();
    check(scalarQueryInspected && scalarPlaceholderLocalized
            && window.documentStateForTest().selection.sectors == std::unordered_set<std::int32_t>{0},
        "decimal consumers: actual numeric query accepts comma input and localizes its scalar example");

    ui::applyUiDecimalSeparator(UiDecimalSeparator::dot);
    bool storedScalarCanonical = false;
    QTimer::singleShot(0, &window, [&]()
    {
        auto* const dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget());
        QWidget* const row = dialog ? queryRow(*dialog, QStringLiteral("Volume")) : nullptr;
        auto* const edit = row ? row->findChild<QLineEdit*>() : nullptr;
        storedScalarCanonical = edit && edit->text() == QStringLiteral("0.125");
        if ( dialog )
        {
            dialog->reject();
        }
    });
    query->trigger();
    check(storedScalarCanonical,
        "decimal consumers: remembered numeric query stays canonical dot when reopened under Dot");

    ui::applyUiDecimalSeparator(UiDecimalSeparator::comma);
    window.mutableDocumentStateForTest().selection.clear();
    if ( auto* const view = window.glViewForTest() )
    {
        view->setSelectionState(-1, -1, -1, -1, window.documentStateForTest().selection);
    }
    bool colorQueryInspected = false;
    QTimer::singleShot(0, &window, [&]()
    {
        auto* const dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget());
        QWidget* const row = dialog ? queryRow(*dialog, QStringLiteral("Tint")) : nullptr;
        auto* const edit = row ? row->findChild<QLineEdit*>() : nullptr;
        auto* const match = row ? row->findChild<QComboBox*>() : nullptr;
        QPushButton* select = nullptr;
        if ( dialog )
        {
            for ( auto* const field : dialog->findChildren<QLineEdit*>() )
            {
                field->clear();
            }
            for ( auto* const button : dialog->findChildren<QPushButton*>() )
            {
                if ( button->text() == QStringLiteral("Select All") )
                {
                    select = button;
                }
            }
        }
        if ( edit && match && select )
        {
            match->setCurrentIndex(match->findData(1));
            edit->setText(QStringLiteral("0,25 0,5 0,75"));
            edit->setModified(true);
            colorQueryInspected = select->isEnabled();
            if ( colorQueryInspected )
            {
                select->click();
                return;
            }
        }
        if ( dialog )
        {
            dialog->reject();
        }
    });
    query->trigger();
    check(colorQueryInspected
            && window.documentStateForTest().selection.sectors == std::unordered_set<std::int32_t>{0}
            && window.documentStateForTest().level.sectors[0].soundVolume == 0.125
            && window.undoDepthForTest() == undoBefore,
        "decimal consumers: actual color query preserves decimal commas as components without authoring or history");
}

void runRemainingNumericDisplayConsumers()
{
    ui::applyUiDecimalSeparator(UiDecimalSeparator::comma);
    Level level{};
    Thing invalidPosition{};
    invalidPosition.x = std::numeric_limits<double>::quiet_NaN();
    invalidPosition.y = 0.125;
    invalidPosition.z = -2.5;
    level.things.push_back(invalidPosition);
    ui::checks::CheckRuleSelection rules{};
    rules.enabled.fill(false);
    rules.setEnabled(ui::checks::CheckRule::thingFinitePosition, true);
    const auto commaIssues = ui::checks::collectThingConsistencyCheckIssues(level, rules, {0});
    check(commaIssues.size() == 1
            && commaIssues.front().message == QStringLiteral("Thing 0: non-finite position (nan, 0,125, -2,5)")
            && commaIssues.front().navKind == OutlinerKind::thing
            && commaIssues.front().navIndex == 0,
        "decimal consumers: actual consistency issue prose localizes only the typed numeric coordinates");
    ui::applyUiDecimalSeparator(UiDecimalSeparator::dot);
    const auto dotIssues = ui::checks::collectThingConsistencyCheckIssues(level, rules, {0});
    check(dotIssues.size() == 1
            && dotIssues.front().message == QStringLiteral("Thing 0: non-finite position (nan, 0.125, -2.5)")
            && std::isnan(level.things.front().x) && level.things.front().y == 0.125
            && level.things.front().z == -2.5,
        "decimal consumers: formatting a consistency issue preserves exact level data and ordinary prose");

    ui::applyUiDecimalSeparator(UiDecimalSeparator::comma);
    QTemporaryDir directory{};
    check(directory.isValid(), "decimal consumers: synthetic WAV fixture has an isolated directory");
    if ( !directory.isValid() )
    {
        return;
    }
    QDir{}.mkpath(directory.filePath(QStringLiteral("sound")));
    const QString filename = QStringLiteral("decimal.1.25.wav");
    const QString path = directory.filePath(QStringLiteral("sound/")) + filename;
    QByteArray bytes{};
    auto appendU16 = [&bytes](const std::uint16_t value)
    {
        bytes.append(static_cast<char>(value & 0xffU));
        bytes.append(static_cast<char>((value >> 8U) & 0xffU));
    };
    auto appendU32 = [&bytes](const std::uint32_t value)
    {
        bytes.append(static_cast<char>(value & 0xffU));
        bytes.append(static_cast<char>((value >> 8U) & 0xffU));
        bytes.append(static_cast<char>((value >> 16U) & 0xffU));
        bytes.append(static_cast<char>((value >> 24U) & 0xffU));
    };
    constexpr std::uint32_t sampleCount = 10'000;
    bytes.append("RIFF", 4);
    appendU32(36U + sampleCount);
    bytes.append("WAVEfmt ", 8);
    appendU32(16U);
    appendU16(1U);
    appendU16(1U);
    appendU32(8'000U);
    appendU32(8'000U);
    appendU16(1U);
    appendU16(8U);
    bytes.append("data", 4);
    appendU32(sampleCount);
    bytes.append(QByteArray(sampleCount, static_cast<char>(0x80)));
    QFile fixture(path);
    const bool written = fixture.open(QIODevice::WriteOnly) && fixture.write(bytes) == bytes.size();
    fixture.close();
    check(written, "decimal consumers: synthetic WAV fixture writes exact 1.25-second mono PCM bytes");
    if ( !written )
    {
        return;
    }
    assets::ResourceLocator resources{};
    const std::filesystem::path rootPath(directory.path().toStdWString());
    resources.addRoot(rootPath);
    ui::ResourcePickerOptions options{};
    options.kind = ui::ResourcePickerKind::wav;
    options.initialName = filename.toStdString();
    options.previewProjectType = ProjectType::IJIM;
    options.preferredDiskRoot = rootPath;
    QWidget owner{};
    owner.setAttribute(Qt::WA_DontShowOnScreen);
    owner.show();
    bool metadataFound = false;
    bool commaDuration = false;
    bool dotDuration = false;
    bool runtimeCommaDuration = false;
    bool resourceNameUnchanged = false;
    int ticks = 0;
    QTimer poll(&owner);
    poll.setInterval(10);
    QObject::connect(&poll, &QTimer::timeout, &owner, [&]()
    {
        auto* const dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget());
        auto* const preview = dialog ? dialog->findChild<QPlainTextEdit*>() : nullptr;
        if ( preview && preview->toPlainText().contains(QStringLiteral("Duration:")) )
        {
            poll.stop();
            metadataFound = true;
            commaDuration = preview->toPlainText().contains(QStringLiteral("Duration: 1,25 s"));
            ui::applyUiDecimalSeparator(UiDecimalSeparator::dot);
            QApplication::processEvents();
            dotDuration = preview->toPlainText().contains(QStringLiteral("Duration: 1.25 s"));
            ui::applyUiDecimalSeparator(UiDecimalSeparator::comma);
            QApplication::processEvents();
            runtimeCommaDuration = preview->toPlainText().contains(QStringLiteral("Duration: 1,25 s"));
            resourceNameUnchanged = preview->toPlainText().contains(QStringLiteral("Name: ") + filename);
            dialog->reject();
        }
        else if ( ++ticks >= 200 )
        {
            poll.stop();
            if ( dialog )
            {
                dialog->reject();
            }
        }
    });
    poll.start();
    const auto picked = ui::pickResource(&owner, resources, options);
    poll.stop();
    check(metadataFound && commaDuration,
        "decimal consumers: the actual WAV picker displays fractional duration using Comma");
    check(metadataFound && dotDuration && runtimeCommaDuration && resourceNameUnchanged,
        "decimal consumers: a live WAV duration refresh changes numeric presentation while retaining the exact resource name");
    check(!picked && fixture.open(QIODevice::ReadOnly) && fixture.readAll() == bytes,
        "decimal consumers: inspecting and refreshing WAV metadata leaves exact resource bytes unchanged");
}

void runTypedParameterDecimalConsumers()
{
    ui::applyUiDecimalSeparator(UiDecimalSeparator::comma);
    {
        MainWindow window{};
        window.setAttribute(Qt::WA_DontShowOnScreen);
        editor::DocumentState state{};
        state.level.kind = ProjectType::IJIM;
        state.level.layers.emplace_back("Layer0");
        state.level.sectors.emplace_back().layer = 0;
        Thing thing{};
        thing.layer = 0;
        thing.sector = 0;
        thing.values.push_back(deserializeTplValue("type", "actor"));
        thing.values.push_back(deserializeTplValue("maxrotvel", "1.25"));
        thing.values.push_back(deserializeTplValue("lightoffset", "(0.125/-1.5/2.25)"));
        TplValue opaque{};
        opaque.name = "decimal_note";
        opaque.type = ValueType::str;
        opaque.stringValue = "keep 1.25 and 1,25 verbatim";
        thing.values.push_back(opaque);
        state.level.things.push_back(thing);
        state.selection.things.insert(0);
        window.setDocumentStateForTest(std::move(state));
        window.activateThingEditModeForTest();
        auto findEdit = [&window](const QString& name)
        {
            return window.findChild<QLineEdit*>(QStringLiteral("prop.thing.value.") + name);
        };
        auto* scalar = findEdit(QStringLiteral("maxrotvel"));
        auto* tuple = findEdit(QStringLiteral("lightoffset"));
        auto* const note = findEdit(QStringLiteral("decimal_note"));
        check(scalar && scalar->text() == QStringLiteral("1,25")
                && tuple && tuple->text() == QStringLiteral("(0,125/-1,5/2,25)")
                && note && note->text() == QString::fromStdString(opaque.stringValue),
            "decimal consumers: actual Thing scalar and tuple editors localize only typed numeric values");
        const auto undoBefore = window.undoDepthForTest();
        if ( scalar )
        {
            editNumericText(*scalar, QStringLiteral("2,75"));
            QMetaObject::invokeMethod(scalar, "editingFinished", Qt::DirectConnection);
        }
        QApplication::processEvents();
        const auto* scalarValue = thing::parameters::localValue(
            window.documentStateForTest().level.things.front(), "maxrotvel");
        check(scalarValue && scalarValue->type == ValueType::str
                && scalarValue->asString() == "2.75"
                && window.undoDepthForTest() == undoBefore + 1,
            "decimal consumers: comma Thing scalar commit stores a typed canonical value and exactly one undo step");
        tuple = findEdit(QStringLiteral("lightoffset"));
        if ( tuple )
        {
            editNumericText(*tuple, QStringLiteral("(0,625/-1,75/2,5)"));
            QMetaObject::invokeMethod(tuple, "editingFinished", Qt::DirectConnection);
        }
        QApplication::processEvents();
        const auto* tupleValue = thing::parameters::localValue(
            window.documentStateForTest().level.things.front(), "lightoffset");
        check(tupleValue && tupleValue->asString() == "(0.625/-1.75/2.5)"
                && serializeTplValue(*tupleValue) == "(0.625/-1.75/2.5)"
                && window.undoDepthForTest() == undoBefore + 2,
            "decimal consumers: comma Thing tuple commit preserves compound delimiters and canonical engine spelling");
        scalar = findEdit(QStringLiteral("maxrotvel"));
        if ( scalar )
        {
            editNumericText(*scalar, QStringLiteral("3,12500"));
            scalar->setSelection(1, 3);
        }
        const auto undoBeforePolicy = window.undoDepthForTest();
        ui::applyUiDecimalSeparator(UiDecimalSeparator::dot);
        QApplication::processEvents();
        check(scalar && scalar->text() == QStringLiteral("3,12500")
                && scalar->selectedText() == QStringLiteral(",12")
                && window.undoDepthForTest() == undoBeforePolicy,
            "decimal consumers: runtime format change preserves a pending Thing scalar and its selection without history");
        if ( scalar )
        {
            QMetaObject::invokeMethod(scalar, "editingFinished", Qt::DirectConnection);
        }
        QApplication::processEvents();
        scalarValue = thing::parameters::localValue(
            window.documentStateForTest().level.things.front(), "maxrotvel");
        scalar = findEdit(QStringLiteral("maxrotvel"));
        if ( !scalarValue || scalarValue->asString() != "3.12500"
            || !scalar || scalar->text() != QStringLiteral("3.12500")
            || window.undoDepthForTest() != undoBeforePolicy + 1 )
        {
            std::cerr << "decimal Thing pending diagnostic: stored="
                << (scalarValue ? scalarValue->asString() : "<missing>")
                << " display=" << (scalar ? scalar->text().toStdString() : "<missing>")
                << " undo=" << window.undoDepthForTest()
                << " expected=" << undoBeforePolicy + 1 << '\n';
        }
        check(scalarValue && scalarValue->type == ValueType::str
                && scalarValue->asString() == "3.12500"
                && scalar && scalar->text() == QStringLiteral("3.12500")
                && window.undoDepthForTest() == undoBeforePolicy + 1,
            "decimal consumers: pending Thing input commits in its old locale then adopts the current numeric display");
        if ( scalar )
        {
            editNumericText(*scalar, QStringLiteral("1,234.5"));
            QMetaObject::invokeMethod(scalar, "editingFinished", Qt::DirectConnection);
        }
        QApplication::processEvents();
        scalarValue = thing::parameters::localValue(
            window.documentStateForTest().level.things.front(), "maxrotvel");
        const auto* opaqueValue = thing::parameters::localValue(
            window.documentStateForTest().level.things.front(), "decimal_note");
        check(scalarValue && scalarValue->asString() == "3.12500"
                && window.undoDepthForTest() == undoBeforePolicy + 1
                && opaqueValue && opaqueValue->stringValue == opaque.stringValue,
            "decimal consumers: invalid grouped Thing input and opaque text cannot change authored values");
    }

    ui::applyUiDecimalSeparator(UiDecimalSeparator::comma);
    {
        MainWindow window{};
        window.setAttribute(Qt::WA_DontShowOnScreen);
        editor::DocumentState state{};
        state.level.kind = ProjectType::IJIM;
        Cog cog{};
        cog.num = 0;
        cog.name = "decimal_instance.cog";
        CogValue scalar{};
        scalar.name = "gain";
        scalar.type = ValueType::floating;
        scalar.cogType = CogType::float_;
        scalar.floatValue = 1.25;
        cog.values.push_back(scalar);
        CogValue tuple{};
        tuple.name = "offset";
        tuple.type = ValueType::vect;
        tuple.cogType = CogType::vect;
        tuple.stringValue = "(0.125/-1.5/2.25)";
        cog.values.push_back(tuple);
        CogValue commaTuple = tuple;
        commaTuple.name = "commaoffset";
        commaTuple.stringValue = "(0.5,1.5,-2.5)";
        cog.values.push_back(commaTuple);
        state.level.cogs.push_back(cog);
        state.selection.cogs.insert(0);
        window.setDocumentStateForTest(std::move(state));
        auto* scalarEdit = propertyLineEdit(window, QStringLiteral("gain : float"));
        auto* tupleEdit = propertyLineEdit(window, QStringLiteral("offset : vector"));
        if ( !scalarEdit || !tupleEdit )
        {
            std::cerr << "decimal COG editor diagnostic: selected="
                << window.documentStateForTest().selection.cogs.size()
                << " mode=" << window.editModeForTest() << '\n';
            if ( auto* const tree = window.findChild<QTreeWidget*>(QStringLiteral("propertiesTree")) )
            {
                int printed = 0;
                for ( QTreeWidgetItemIterator item(tree); *item && printed < 20; ++item, ++printed )
                {
                    std::cerr << "  " << (*item)->text(0).toStdString() << '\n';
                }
            }
        }
        check(scalarEdit && scalarEdit->text() == QStringLiteral("1,25")
                && tupleEdit && tupleEdit->text() == QStringLiteral("(0,125/-1,5/2,25)"),
            "decimal consumers: actual placed-COG float and vector fields honor Comma");
        auto* const commaEdit = propertyLineEdit(window, QStringLiteral("commaoffset : vector"));
        const auto undoBefore = window.undoDepthForTest();
        check(commaEdit && commaEdit->text() == QStringLiteral("(0,5/1,5/-2,5)"),
            "decimal consumers: actual placed-COG comma-delimited baseline has unambiguous localized numeric components");
        if ( commaEdit )
        {
            QMetaObject::invokeMethod(commaEdit, "editingFinished", Qt::DirectConnection);
        }
        check(window.documentStateForTest().level.cogs.front().values[2].stringValue
                    == "(0.5,1.5,-2.5)"
                && window.undoDepthForTest() == undoBefore,
            "decimal consumers: untouched localized placed-COG tuple preserves original accepted text and creates no undo");
        if ( scalarEdit )
        {
            editNumericText(*scalarEdit, QStringLiteral("2,75"));
            QMetaObject::invokeMethod(scalarEdit, "editingFinished", Qt::DirectConnection);
        }
        QApplication::processEvents();
        check(window.documentStateForTest().level.cogs.front().values[0].floatValue == 2.75
                && window.documentStateForTest().level.cogs.front().values[0].asString() == "2.75"
                && window.undoDepthForTest() == undoBefore + 1,
            "decimal consumers: a comma placed-COG float commit stores canonical engine data with one undo record");
        tupleEdit = propertyLineEdit(window, QStringLiteral("offset : vector"));
        if ( tupleEdit )
        {
            editNumericText(*tupleEdit, QStringLiteral("(0,625/-1,75/2,5)"));
            QMetaObject::invokeMethod(tupleEdit, "editingFinished", Qt::DirectConnection);
        }
        QApplication::processEvents();
        check(window.documentStateForTest().level.cogs.front().values[1].asString()
                    == "(0.625/-1.75/2.5)"
                && window.undoDepthForTest() == undoBefore + 2,
            "decimal consumers: placed-COG vector commit preserves slashes and dot syntax in accepted data");
    }
}

} // namespace

void runUiDecimalConsumerTests()
{
    const DecimalPreferenceScope restorePreference{};
    check(appSettingsIniPathIsOverriddenForTest(),
        "decimal consumers: the real application preference is process-isolated");
    QSettings settings = settingsStore();
    settings.setValue(QStringLiteral("ui/decimal_separator"), 1);
    settings.setValue(QStringLiteral("recovery/autosave_enabled"), false);
    settings.sync();
    if ( ui::uiDecimalPolicyInstalled() )
    {
        ui::applyUiDecimalSeparator(loadUiDecimalSeparator(settings));
    }

    MainWindow window{};
    window.setAttribute(Qt::WA_DontShowOnScreen);
    window.resize(1280, 800);
    editor::DocumentState state{};
    state.level.kind = ProjectType::IJIM;
    state.level.layers.emplace_back("Layer0");
    level::levelCreateCube(state.level, -2.0, 0.0, -2.0,
        TVector{0.0, -1.0, 0.0}, TVector{4.0, 0.0, 0.0}).layer = 0;
    state.selection.sectors.insert(0);
    window.setDocumentStateForTest(std::move(state));
    window.activateSectorEditModeForTest();
    GlView* const view = window.glViewForTest();
    check(view != nullptr,
        "decimal consumers: the production window owns a viewport");
    if ( !view )
    {
        return;
    }
    view->setLevel(&window.documentStateForTest().level, GlView::SetLevelMode::resetView);
    view->setSelectionState(0, -1, -1, -1, window.documentStateForTest().selection);
    window.show();
    QApplication::processEvents();

    QToolButton* const camera = window.findChild<QToolButton*>(
        QStringLiteral("viewportCameraSettingsButton"));
    QWidget* const fovRow = window.findChild<QWidget*>(
        QStringLiteral("viewportCameraFovRow"));
    QLineEdit* const fov = fovRow ? fovRow->findChild<QLineEdit*>() : nullptr;
    check(camera && camera->menu() && fov,
        "decimal consumers: the production camera menu exposes its manual FOV editor");
    if ( camera && camera->menu() && fov )
    {
        view->setCameraProjectionType(app::ViewportCamera::ProjectionType::Perspective);
        view->setCameraFovXDeg(73.5f);
        QMetaObject::invokeMethod(camera->menu(), "aboutToShow", Qt::DirectConnection);
        check(fov->text() == QStringLiteral("73,5"),
            "decimal consumers: seeded Comma formats the actual camera FOV readout");
        editNumericText(*fov, QStringLiteral("61,25"));
        QMetaObject::invokeMethod(fov, "editingFinished", Qt::DirectConnection);
        check(view->cameraFovXDeg() == 61.25f && fov->text() == QStringLiteral("61,3"),
            "decimal consumers: a comma FOV edit commits its full numeric value and localizes its display");
        editNumericText(*fov, QStringLiteral("1.234,5"));
        QMetaObject::invokeMethod(fov, "editingFinished", Qt::DirectConnection);
        check(view->cameraFovXDeg() == 61.25f,
            "decimal consumers: grouped FOV text cannot author a clamped different value");
    }

    int lightPreviews = 0;
    ui::LightParametersDialog::Values lightPreview{};
    ui::LightParametersDialog popup(&window, QStringLiteral("Decimal light test"),
        ui::LightParametersDialog::Mode::baked, {},
        [&](const ui::LightParametersDialog::Values& values)
        {
            ++lightPreviews;
            lightPreview = values;
        });
    auto* const intensity = popup.findChild<ui::ScrubFloatEdit*>(
        QStringLiteral("lightParameters.intensity"));
    check(intensity != nullptr,
        "decimal consumers: the actual live light popup exposes numeric intensity");
    if ( intensity )
    {
        const int previewsBeforeEdit = lightPreviews;
        editNumericText(*intensity->lineEdit(), QStringLiteral("2,500001"));
        check(lightPreviews == previewsBeforeEdit + 1 && lightPreview.intensity == 2.500001,
            "decimal consumers: comma typing previews every intensity digit immediately");
        const int previewsBeforeInvalid = lightPreviews;
        editNumericText(*intensity->lineEdit(), QStringLiteral("1.234,5"));
        check(lightPreviews == previewsBeforeInvalid,
            "decimal consumers: grouped light input does not publish a live preview");
    }

    QString scalarPreview{};
    ui::ThingLightValueDialog scalar(&window, QStringLiteral("Decimal scalar test"),
        ui::ThingLightValueDialog::Mode::scalar, QStringLiteral("1.25"),
        [&](const QString& value) { scalarPreview = value; });
    auto* const scalarEdit = scalar.findChild<ui::ScrubFloatEdit*>(
        QStringLiteral("thingLightValue.intensity"));
    check(scalarEdit && scalar.valueText() == QStringLiteral("1.25"),
        "decimal consumers: raw engine scalar input remains canonical dot under Comma");
    if ( scalarEdit )
    {
        editNumericText(*scalarEdit->lineEdit(), QStringLiteral("2,75"));
        check(scalarPreview == QStringLiteral("2.75")
                && scalar.valueText() == QStringLiteral("2.75"),
            "decimal consumers: a localized scalar editor emits canonical engine text without waiting for commit");
    }

    QString vectorPreview{};
    ui::ThingLightValueDialog vector(&window, QStringLiteral("Decimal vector test"),
        ui::ThingLightValueDialog::Mode::vector, QStringLiteral("(0.125/-1.5/2.25)"),
        [&](const QString& value) { vectorPreview = value; });
    auto* const vectorEdit = vector.findChild<ui::ScrubFloatEdit*>(
        QStringLiteral("thingLightValue.component.0"));
    check(vectorEdit && vector.valueText() == QStringLiteral("(0.125/-1.5/2.25)"),
        "decimal consumers: slash-delimited engine vectors remain exact independent of display policy");
    if ( vectorEdit )
    {
        editNumericText(*vectorEdit->lineEdit(), QStringLiteral("0,625"));
        check(vectorPreview == QStringLiteral("(0.625/-1.5/2.25)")
                && vector.valueText() == vectorPreview,
            "decimal consumers: a comma vector component previews canonical dot without splitting its decimal comma");
    }

    check(render::cleaveLengthText(0.125, render::CleaveMeasureUnit::IJU)
            == QStringLiteral("1,25 IJU")
            && render::compactCleaveMeasurementNumber(0.000001, 3) == QStringLiteral("1e-06"),
        "decimal consumers: Cleave measurement readouts localize compact fractions without losing tiny values");

    view->setSectorCleaveToolEnabled(true);
    view->setCleaveStagedMode(true);
    view->setCleaveChainEnabled(false);
    view->setTransformGridSnapEnabled(false);
    view->captureStagedCleaveContextForTest(false);
    view->addStagedCleaveLineForTest({-1, 0, -1}, {1, 0, -1}, {0, 0, 1});
    view->addStagedCleaveLineForTest({1, 0, -1}, {1, 0, 1}, {-1, 0, 0});
    auto draft = view->captureStagedCleaveDraftState();
    draft.selectedCleaveLines = {0, 1};
    draft.selectedCleaveLine = 1;
    view->restoreStagedCleaveDraftState(draft);
    view->setStagedCleaveTransformMode(GlView::StagedCleaveTransformMode::Scale);
    const double lengthBefore = (view->stagedCleaveLineSecondForTest(0)
        - view->stagedCleaveLineFirstForTest(0)).length();
    const bool captured = sendCleaveWheel(*view);
    QApplication::processEvents();
    view->grabFramebuffer();
    const double lengthAfter = (view->stagedCleaveLineSecondForTest(0)
        - view->stagedCleaveLineFirstForTest(0)).length();
    check(captured && std::abs(lengthAfter / lengthBefore - 1.1) < 1.0e-6
            && view->stagedCleaveTransformHudTextForTest() == QStringLiteral("Scale 1,10×")
            && !view->stagedCleaveTransformHudRectForTest().isEmpty(),
        "decimal consumers: a real Cleave wheel transform paints a comma HUD with exactly two decimals");

    check(view->finishStagedCleaveWheelSession(),
        "decimal consumers: explicit wheel completion retains a settled HUD for fallback formatting");
    const auto firstBeforePolicy = view->stagedCleaveLineFirstForTest(0);
    const auto secondBeforePolicy = view->stagedCleaveLineSecondForTest(0);
    const auto historyBeforePolicy = view->captureStagedCleaveHistory();
    const auto documentUndoBeforePolicy = window.undoDepthForTest();
    if ( fov )
    {
        editNumericText(*fov, QStringLiteral("62,250"));
        fov->setSelection(1, 3);
    }
    ui::applyUiDecimalSeparator(UiDecimalSeparator::dot);
    QApplication::processEvents();
    view->grabFramebuffer();
    const auto historyAfterPolicy = view->captureStagedCleaveHistory();
    check(view->stagedCleaveTransformHudTextForTest() == QStringLiteral("Scale 1.10×")
            && view->stagedCleaveLineFirstForTest(0) == firstBeforePolicy
            && view->stagedCleaveLineSecondForTest(0) == secondBeforePolicy
            && historyBeforePolicy && historyAfterPolicy
            && historyAfterPolicy->cursor == historyBeforePolicy->cursor
            && historyAfterPolicy->snapshots.size() == historyBeforePolicy->snapshots.size()
            && window.undoDepthForTest() == documentUndoBeforePolicy,
        "decimal consumers: runtime preference refreshes a settled HUD without editing cuts or histories");
    QKeyEvent controlRelease(QEvent::KeyRelease, Qt::Key_Control, Qt::NoModifier);
    QApplication::sendEvent(view, &controlRelease);
    const auto historyAfterRelease = view->captureStagedCleaveHistory();
    check(view->stagedCleaveTransformHudTextForTest().isEmpty()
            && view->stagedCleaveTransformHudRectForTest().isEmpty()
            && view->stagedCleaveLineFirstForTest(0) == firstBeforePolicy
            && view->stagedCleaveLineSecondForTest(0) == secondBeforePolicy
            && historyAfterPolicy && historyAfterRelease
            && historyAfterRelease->cursor == historyAfterPolicy->cursor
            && historyAfterRelease->snapshots.size() == historyAfterPolicy->snapshots.size()
            && window.undoDepthForTest() == documentUndoBeforePolicy,
        "decimal consumers: Control release immediately hides the settled HUD without editing cuts or histories");
    if ( fov )
    {
        check(fov->text() == QStringLiteral("62,250")
                && fov->selectedText() == QStringLiteral("2,2")
                && view->cameraFovXDeg() == 61.25f,
            "decimal consumers: runtime policy preserves pending raw FOV text, selection and camera value");
        QMetaObject::invokeMethod(fov, "editingFinished", Qt::DirectConnection);
        check(view->cameraFovXDeg() == 62.25f && fov->text() == QStringLiteral("62.3"),
            "decimal consumers: pending comma FOV commits using its captured locale then adopts Dot display");
    }
    view->setSectorCleaveToolEnabled(false);
    check(!view->isStagedCleaveInteractionActive(),
        "decimal consumers: the native Cleave tool releases modifier ownership before standalone viewport checks");
    runCleaveWheelIdentityDecimalConsumers();
    runTypedNumericBridgeCases();
    runDisplayAndQueryDecimalConsumers();
    runTypedParameterDecimalConsumers();
    runRemainingNumericDisplayConsumers();
}

} // namespace sed::app_tests
